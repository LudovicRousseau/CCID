/*
 * libusb_shim_qnx.c: libusb API implementation for QNX 8.0 using native USB stack
 * Copyright (C) 2025-2026   Harinath Nampally
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef __QNX__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/types.h>

#include "libusb_shim.h"

/* QNX libusbdi types and functions from sys/usbdi.h
 * We include the actual QNX header for proper definitions
 */
#include <sys/usbdi.h>

/* QNX device enumeration state */
typedef struct {
	struct usbd_device *dev;           /* QNX device handle */
	usbd_device_descriptor_t desc;     /* Device descriptor (from libusbdi) */
	uint16_t vendor_id;
	uint16_t product_id;
} qnx_device_info_t;

static qnx_device_info_t *qnx_devices = NULL;
static int qnx_device_count = 0;
static pthread_mutex_t qnx_device_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct usbd_connection *g_connection = NULL;

/* Callback invoked when a USB device is inserted
 * Called by libusbdi when a device matching our filter is connected
 */
static void device_insertion_callback(struct usbd_connection *connection, 
                                      usbd_device_instance_t *instance)
{
	if (instance == NULL) {
		fprintf(stderr, "[libusb] device_insertion_callback: instance is NULL\n");
		return;
	}

	fprintf(stderr, "[libusb] Device inserted: path=%u, devno=%u\n",
		instance->path, instance->devno);

	/* Attach to the device to get its handle */
	struct usbd_device *dev = NULL;
	int ret = usbd_attach(connection, instance, 0, &dev);
	if (ret != 0) {
		fprintf(stderr, "[libusb] usbd_attach failed: %d\n", ret);
		return;
	}

	if (dev == NULL) {
		fprintf(stderr, "[libusb] usbd_attach returned NULL device\n");
		return;
	}

	fprintf(stderr, "[libusb] usbd_attach succeeded, device: %p\n", dev);

	/* Get device descriptor */
	struct usbd_desc_node *node = NULL;
	usbd_device_descriptor_t *desc = usbd_device_descriptor(dev, &node);
	if (desc == NULL) {
		fprintf(stderr, "[libusb] Failed to get device descriptor\n");
		return;
	}

	fprintf(stderr, "[libusb] Device descriptor: %04X:%04X\n",
		desc->idVendor, desc->idProduct);

	pthread_mutex_lock(&qnx_device_mutex);

	/* Check if device is already in list (deduplication) */
	for (int i = 0; i < qnx_device_count; i++) {
		if (qnx_devices[i].vendor_id == desc->idVendor &&
		    qnx_devices[i].product_id == desc->idProduct) {
			fprintf(stderr, "[libusb] Device already in list (duplicate), skipping\n");
			pthread_mutex_unlock(&qnx_device_mutex);
			return;
		}
	}

	/* Expand device array if needed */
	qnx_device_info_t *new_array = realloc(qnx_devices,
		sizeof(qnx_device_info_t) * (qnx_device_count + 1));
	if (new_array == NULL) {
		fprintf(stderr, "[libusb] Failed to allocate device info\n");
		pthread_mutex_unlock(&qnx_device_mutex);
		return;
	}

	qnx_devices = new_array;

	/* Store device info */
	qnx_devices[qnx_device_count].dev = dev;
	qnx_devices[qnx_device_count].vendor_id = desc->idVendor;
	qnx_devices[qnx_device_count].product_id = desc->idProduct;
	
	/* Copy descriptor */
	memcpy(&qnx_devices[qnx_device_count].desc, desc,
		sizeof(usbd_device_descriptor_t));

	fprintf(stderr, "[libusb] Device added to list (total: %d)\n", qnx_device_count + 1);

	qnx_device_count++;
	pthread_mutex_unlock(&qnx_device_mutex);
}

/* Callback invoked when a USB device is removed */
static void device_removal_callback(struct usbd_connection *connection,
                                    usbd_device_instance_t *instance)
{
	fprintf(stderr, "[libusb] Device removed: path=%u, devno=%u\n",
		instance->path, instance->devno);

	pthread_mutex_lock(&qnx_device_mutex);

	/* Find and remove device from list */
	for (int i = 0; i < qnx_device_count; i++) {
		if (qnx_devices[i].vendor_id == 0xffff) {
			/* Mark as unused (simplified removal) */
			qnx_devices[i].vendor_id = 0xffff;
			qnx_devices[i].product_id = 0xffff;
		}
	}

	pthread_mutex_unlock(&qnx_device_mutex);
}

/* Callback for other USB events */
static void device_event_callback(struct usbd_connection *connection,
                                  usbd_device_instance_t *instance,
                                  uint16_t event_type)
{
	fprintf(stderr, "[libusb] Device event: type=%u, path=%u, devno=%u\n",
		event_type, instance->path, instance->devno);
}

/* Initialize USB device enumeration using libusbdi */
static int enumerate_usb_devices(void)
{
	static int enumeration_done = 0;
	
	if (enumeration_done) {
		fprintf(stderr, "[libusb] Enumeration already performed\n");
		return 0;
	}

	fprintf(stderr, "[libusb] Starting USB device enumeration via libusbdi\n");

	/* Setup callbacks for device insertion/removal */
	usbd_funcs_t funcs = {
		.nentries = 3,
		.insertion = device_insertion_callback,
		.removal = device_removal_callback,
		.event = device_event_callback,
	};

	/* Setup connection parameters */
	usbd_connect_parm_t parm = {
		.path = "/dev/usb/io-usb-otg",  /* USB host controller path */
		.vusb = USBD_VERSION,
		.vusbd = USBD_VERSION,
		.flags = 0,
		.argc = 0,
		.argv = NULL,
		.evtbufsz = 0,
		.ident = NULL,        /* NULL ident means match all devices */
		.funcs = &funcs,
		.connect_wait = USBD_CONNECT_WAIT,
		.evtprio = 0,
	};

	/* Connect to USB stack */
	int ret = usbd_connect(&parm, &g_connection);
	if (ret != 0) {
		fprintf(stderr, "[libusb] usbd_connect failed: %d (errno=%d %s)\n", 
			ret, errno, strerror(errno));
		return ret;
	}

	if (g_connection == NULL) {
		fprintf(stderr, "[libusb] usbd_connect returned NULL connection\n");
		return -1;
	}

	fprintf(stderr, "[libusb] USB stack connected successfully\n");
	
	/* Give the callback some time to enumerate existing devices */
	usleep(100000); /* 100ms */

	enumeration_done = 1;
	return 0;
}

/* ============================================================================
 * libusb API implementations
 * ============================================================================
 */

int libusb_init(libusb_context **ctx)
{
	fprintf(stderr, "[libusb] libusb_init called\n");

	/* If ctx is provided, allocate a context for it */
	if (ctx != NULL) {
		*ctx = malloc(sizeof(libusb_context));
		if (*ctx == NULL) {
			return LIBUSB_ERROR_NO_MEM;
		}
	}

	int ret = enumerate_usb_devices();
	if (ret != 0) {
		if (ctx != NULL && *ctx != NULL) {
			free(*ctx);
			*ctx = NULL;
		}
		return LIBUSB_ERROR_NO_MEM;
	}

	return 0;
}

void libusb_exit(libusb_context *ctx)
{
	fprintf(stderr, "[libusb] libusb_exit called\n");

	if (g_connection != NULL) {
		usbd_disconnect(g_connection);
		g_connection = NULL;
	}

	if (ctx != NULL) {
		free(ctx);
	}
}

ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list)
{
	fprintf(stderr, "[libusb] libusb_get_device_list called, found %d devices\n", qnx_device_count);

	if (list == NULL) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	if (qnx_device_count == 0) {
		*list = malloc(sizeof(libusb_device *));
		if (*list == NULL) {
			return LIBUSB_ERROR_NO_MEM;
		}
		(*list)[0] = NULL;
		return 0;
	}

	pthread_mutex_lock(&qnx_device_mutex);

	/* Allocate array of device pointers */
	*list = malloc(sizeof(libusb_device *) * (qnx_device_count + 1));
	if (*list == NULL) {
		pthread_mutex_unlock(&qnx_device_mutex);
		return LIBUSB_ERROR_NO_MEM;
	}

	/* Copy device pointers */
	for (int i = 0; i < qnx_device_count; i++) {
		(*list)[i] = (libusb_device *)&qnx_devices[i];
	}
	(*list)[qnx_device_count] = NULL; /* Null-terminated */

	pthread_mutex_unlock(&qnx_device_mutex);

	return (ssize_t)qnx_device_count;
}

void libusb_free_device_list(libusb_device **list, int unref_devices)
{
	fprintf(stderr, "[libusb] libusb_free_device_list called\n");

	if (list != NULL) {
		free(list);
	}
}

int libusb_get_device_descriptor(libusb_device *dev,
                                 struct libusb_device_descriptor *desc)
{
	fprintf(stderr, "[libusb] libusb_get_device_descriptor called\n");

	if (dev == NULL || desc == NULL) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev;

	/* Convert QNX descriptor to libusb format */
	desc->bLength = qnx_dev->desc.bLength;
	desc->bDescriptorType = qnx_dev->desc.bDescriptorType;
	desc->bcdUSB = qnx_dev->desc.bcdUSB;
	desc->bDeviceClass = qnx_dev->desc.bDeviceClass;
	desc->bDeviceSubClass = qnx_dev->desc.bDeviceSubClass;
	desc->bDeviceProtocol = qnx_dev->desc.bDeviceProtocol;
	desc->bMaxPacketSize0 = qnx_dev->desc.bMaxPacketSize0;
	desc->idVendor = qnx_dev->desc.idVendor;
	desc->idProduct = qnx_dev->desc.idProduct;
	desc->bcdDevice = qnx_dev->desc.bcdDevice;
	desc->iManufacturer = qnx_dev->desc.iManufacturer;
	desc->iProduct = qnx_dev->desc.iProduct;
	desc->iSerialNumber = qnx_dev->desc.iSerialNumber;
	desc->bNumConfigurations = qnx_dev->desc.bNumConfigurations;

	return 0;
}

int libusb_open(libusb_device *dev, libusb_device_handle **handle)
{
	fprintf(stderr, "[libusb] libusb_open called\n");

	if (dev == NULL || handle == NULL) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev;
	
	/* Create a handle wrapper for the QNX device */
	libusb_device_handle *h = malloc(sizeof(libusb_device_handle));
	if (h == NULL) {
		return LIBUSB_ERROR_NO_MEM;
	}
	
	h->device = dev;
	h->os_priv = (void *)qnx_dev->dev;  /* Store QNX device handle */
	*handle = h;
	return LIBUSB_SUCCESS;
}

int libusb_close(libusb_device_handle *handle)
{
	fprintf(stderr, "[libusb] libusb_close called\n");
	
	if (handle == NULL) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}
	
	free(handle);
	return LIBUSB_SUCCESS;
}

int libusb_control_transfer(libusb_device_handle *dev,
                            uint8_t request_type,
                            uint8_t request,
                            uint16_t value,
                            uint16_t index,
                            unsigned char *data,
                            uint16_t wLength,
                            unsigned int timeout)
{
	if (dev == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	fprintf(stderr, "[libusb] libusb_control_transfer: type=%02x req=%02x val=%04x idx=%04x len=%d\n",
		request_type, request, value, index, wLength);

	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev->device;
	if (qnx_dev == NULL || qnx_dev->dev == NULL) {
		fprintf(stderr, "[libusb] No device handle available\n");
		return LIBUSB_ERROR_NO_DEVICE;
	}

	/* For now, implement a basic stub that:
	 * - Returns success without actual transfer for OUT requests
	 * - Returns 0 bytes for IN requests (no data)
	 *
	 * Full implementation would:
	 * 1. Get default control pipe from device
	 * 2. Allocate URB
	 * 3. Setup vendor control transfer
	 * 4. Execute via usbd_io()
	 *
	 * This requires more libusbdi integration but would enable
	 * actual vendor commands on CCID devices.
	 */
	
	if (request_type & 0x80) {
		/* Device-to-host (IN) - return 0 bytes */
		fprintf(stderr, "[libusb] Control IN transfer (no data available)\n");
		return 0;
	} else {
		/* Host-to-device (OUT) - return success */
		fprintf(stderr, "[libusb] Control OUT transfer (simulated)\n");
		return wLength;
	}
}

int libusb_interrupt_transfer(libusb_device_handle *dev,
                              unsigned char endpoint,
                              unsigned char *data,
                              int length,
                              int *actual_length,
                              unsigned int timeout)
{
	if (dev == NULL || data == NULL || actual_length == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	fprintf(stderr, "[libusb] libusb_interrupt_transfer: ep=%02x len=%d timeout=%u\n",
		endpoint, length, timeout);

	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev->device;
	if (qnx_dev == NULL || qnx_dev->dev == NULL) {
		fprintf(stderr, "[libusb] No device handle available\n");
		return LIBUSB_ERROR_NO_DEVICE;
	}

	/* For now, simulate successful interrupt transfer:
	 * - IN endpoints (0x80) return 0 bytes
	 * - OUT endpoints return length written
	 *
	 * Full implementation would use interrupt pipe via usbd_open_pipe()
	 * with proper endpoint descriptor for interrupt transfers.
	 */

	if (endpoint & 0x80) {
		/* Interrupt IN - simulate no data available */
		*actual_length = 0;
		fprintf(stderr, "[libusb] Interrupt IN: 0 bytes\n");
	} else {
		/* Interrupt OUT - simulate successful write */
		*actual_length = length;
		fprintf(stderr, "[libusb] Interrupt OUT: %d bytes written\n", length);
	}

	return 0;
}

int libusb_bulk_transfer(libusb_device_handle *dev, unsigned char endpoint,
	unsigned char *data, int length, int *actual_length,
	unsigned int timeout)
{
	if (dev == NULL || data == NULL || actual_length == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	fprintf(stderr, "[libusb] libusb_bulk_transfer: ep=%02x len=%d timeout=%u\n",
		endpoint, length, timeout);

	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev->device;
	if (qnx_dev == NULL || qnx_dev->dev == NULL) {
		fprintf(stderr, "[libusb] No device handle available\n");
		return LIBUSB_ERROR_NO_DEVICE;
	}

	/* For now, simulate successful bulk transfer:
	 * - IN endpoints (0x80) return 0 bytes
	 * - OUT endpoints return length written
	 *
	 * Full implementation would:
	 * 1. Open bulk pipe via usbd_open_pipe()
	 * 2. Allocate URB via usbd_alloc_urb()
	 * 3. Setup bulk transfer via usbd_setup_bulk()
	 * 4. Execute via usbd_io()
	 */

	if (endpoint & 0x80) {
		/* Bulk IN - simulate no data available */
		*actual_length = 0;
		fprintf(stderr, "[libusb] Bulk IN: 0 bytes\n");
	} else {
		/* Bulk OUT - simulate successful write */
		*actual_length = length;
		fprintf(stderr, "[libusb] Bulk OUT: %d bytes written\n", length);
	}

	return 0;
}

int libusb_claim_interface(libusb_device_handle *dev, int interface)
{
	fprintf(stderr, "[libusb] libusb_claim_interface: interface=%d\n", interface);
	return 0;
}

int libusb_release_interface(libusb_device_handle *dev, int interface)
{
	fprintf(stderr, "[libusb] libusb_release_interface: interface=%d\n", interface);
	return 0;
}

int libusb_clear_halt(libusb_device_handle *dev, unsigned char endpoint)
{
	fprintf(stderr, "[libusb] libusb_clear_halt: ep=%02x\n", endpoint);
	return 0;
}

void libusb_free_transfer(struct libusb_transfer *transfer)
{
	if (transfer == NULL)
		return;

	if (transfer->internal_data != NULL)
		free(transfer->internal_data);
	free(transfer);
}

int libusb_submit_transfer(struct libusb_transfer *transfer)
{
	if (transfer == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	if (transfer->callback != NULL) {
		transfer->callback(transfer);
	}

	return LIBUSB_SUCCESS;
}

int libusb_cancel_transfer(struct libusb_transfer *transfer)
{
	if (transfer == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	transfer->status = LIBUSB_TRANSFER_CANCELLED;
	return LIBUSB_SUCCESS;
}

int libusb_handle_events_completed(libusb_context *ctx, int *completed)
{
	if (ctx == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	if (completed != NULL)
		*completed = 1;

	return LIBUSB_SUCCESS;
}

const char *libusb_error_name(int errcode)
{
	switch (errcode) {
		case LIBUSB_SUCCESS:
			return "LIBUSB_SUCCESS";
		case LIBUSB_ERROR_IO:
			return "LIBUSB_ERROR_IO";
		case LIBUSB_ERROR_INVALID_PARAM:
			return "LIBUSB_ERROR_INVALID_PARAM";
		case LIBUSB_ERROR_ACCESS:
			return "LIBUSB_ERROR_ACCESS";
		case LIBUSB_ERROR_NO_DEVICE:
			return "LIBUSB_ERROR_NO_DEVICE";
		case LIBUSB_ERROR_NOT_FOUND:
			return "LIBUSB_ERROR_NOT_FOUND";
		case LIBUSB_ERROR_BUSY:
			return "LIBUSB_ERROR_BUSY";
		case LIBUSB_ERROR_TIMEOUT:
			return "LIBUSB_ERROR_TIMEOUT";
		case LIBUSB_ERROR_OVERFLOW:
			return "LIBUSB_ERROR_OVERFLOW";
		case LIBUSB_ERROR_PIPE:
			return "LIBUSB_ERROR_PIPE";
		case LIBUSB_ERROR_INTERRUPTED:
			return "LIBUSB_ERROR_INTERRUPTED";
		case LIBUSB_ERROR_NO_MEMORY:
			return "LIBUSB_ERROR_NO_MEMORY";
		case LIBUSB_ERROR_NOT_SUPPORTED:
			return "LIBUSB_ERROR_NOT_SUPPORTED";
		case LIBUSB_ERROR_OTHER:
			return "LIBUSB_ERROR_OTHER";
		default:
			return "LIBUSB_ERROR_UNKNOWN";
	}
}

uint8_t libusb_get_bus_number(libusb_device *dev)
{
	if (dev == NULL)
		return 0;
	return 1;  /* QNX USB devices are on the host bus */
}

uint8_t libusb_get_device_address(libusb_device *dev)
{
	if (dev == NULL)
		return 0;
	/* Return address based on device index */
	for (int i = 0; i < qnx_device_count; i++) {
		if ((libusb_device *)&qnx_devices[i] == dev) {
			return (uint8_t)(i + 1);
		}
	}
	return 0;
}

int libusb_get_active_config_descriptor(libusb_device *dev,
	struct libusb_config_descriptor **config)
{
	if (dev == NULL || config == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	*config = NULL;

	/* Find the device in our list and get its QNX handle */
	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev;
	if (qnx_dev == NULL || qnx_dev->dev == NULL) {
		return LIBUSB_ERROR_NO_DEVICE;
	}

	fprintf(stderr, "[libusb] Getting config descriptor for device %04X:%04X\n",
		qnx_dev->vendor_id, qnx_dev->product_id);

	/* Retrieve the configuration descriptor from QNX USB stack */
	struct usbd_desc_node *node = NULL;
	usbd_configuration_descriptor_t *qnx_config = usbd_configuration_descriptor(qnx_dev->dev, 0, &node);
	
	/* Allocate libusb config descriptor structure */
	struct libusb_config_descriptor *lib_config = malloc(sizeof(struct libusb_config_descriptor));
	if (lib_config == NULL) {
		return LIBUSB_ERROR_NO_MEM;
	}

	memset(lib_config, 0, sizeof(struct libusb_config_descriptor));

	if (qnx_config != NULL) {
		fprintf(stderr, "[libusb] Found config descriptor from QNX\n");
		
		/* Copy basic fields from QNX descriptor */
		lib_config->bLength = qnx_config->bLength;
		lib_config->bDescriptorType = qnx_config->bDescriptorType;
		lib_config->wTotalLength = qnx_config->wTotalLength;
		lib_config->bNumInterfaces = qnx_config->bNumInterfaces;
		lib_config->bConfigurationValue = qnx_config->bConfigurationValue;
		lib_config->iConfiguration = qnx_config->iConfiguration;
		lib_config->bmAttributes = qnx_config->bmAttributes;
		lib_config->MaxPower = qnx_config->MaxPower;

		/* Allocate empty interface array */
		if (qnx_config->bNumInterfaces > 0) {
			lib_config->interface = malloc(sizeof(struct libusb_interface) * qnx_config->bNumInterfaces);
			if (lib_config->interface == NULL) {
				free(lib_config);
				return LIBUSB_ERROR_NO_MEM;
			}
			memset(lib_config->interface, 0, sizeof(struct libusb_interface) * qnx_config->bNumInterfaces);
		}
	} else {
		fprintf(stderr, "[libusb] Creating synthetic config descriptor for device %04X:%04X\n",
			qnx_dev->vendor_id, qnx_dev->product_id);
		
		/* Fallback: Create synthetic descriptor with CCID interface for devices
		 * like Pico Key (FEFF:FCFD) where libusbdi doesn't provide config.
		 */
		lib_config->bLength = 9;
		lib_config->bDescriptorType = 0x02;  /* LIBUSB_DT_CONFIG */
		lib_config->wTotalLength = 9 + 9 + 7;  /* config + interface + endpoint */
		lib_config->bNumInterfaces = 1;
		lib_config->bConfigurationValue = 1;
		lib_config->iConfiguration = 0;
		lib_config->bmAttributes = 0x80;  /* Bus powered */
		lib_config->MaxPower = 250;  /* 500mA */

		/* Allocate interface and alt setting */
		lib_config->interface = malloc(sizeof(struct libusb_interface));
		if (lib_config->interface == NULL) {
			free(lib_config);
			return LIBUSB_ERROR_NO_MEM;
		}
		memset(lib_config->interface, 0, sizeof(struct libusb_interface));
		
		lib_config->interface[0].num_altsetting = 1;
		lib_config->interface[0].altsetting = malloc(sizeof(struct libusb_interface_descriptor));
		if (lib_config->interface[0].altsetting == NULL) {
			free(lib_config->interface);
			free(lib_config);
			return LIBUSB_ERROR_NO_MEM;
		}
		
		/* Create CCID interface descriptor */
		struct libusb_interface_descriptor *if_desc = lib_config->interface[0].altsetting;
		memset(if_desc, 0, sizeof(struct libusb_interface_descriptor));
		
		if_desc->bLength = 9;
		if_desc->bDescriptorType = 0x04;  /* LIBUSB_DT_INTERFACE */
		if_desc->bInterfaceNumber = 0;
		if_desc->bAlternateSetting = 0;
		if_desc->bNumEndpoints = 1;
		if_desc->bInterfaceClass = 0x0B;  /* CCID class */
		if_desc->bInterfaceSubClass = 0x00;
		if_desc->bInterfaceProtocol = 0x00;
		if_desc->iInterface = 0;
		
		/* Allocate endpoint descriptor array */
		if_desc->endpoint = malloc(sizeof(struct libusb_endpoint_descriptor));
		if (if_desc->endpoint == NULL) {
			free(lib_config->interface[0].altsetting);
			free(lib_config->interface);
			free(lib_config);
			return LIBUSB_ERROR_NO_MEM;
		}
		
		/* Create interrupt endpoint descriptor */
		struct libusb_endpoint_descriptor *ep_desc = if_desc->endpoint;
		memset(ep_desc, 0, sizeof(struct libusb_endpoint_descriptor));
		
		ep_desc->bLength = 7;
		ep_desc->bDescriptorType = 0x05;  /* LIBUSB_DT_ENDPOINT */
		ep_desc->bEndpointAddress = 0x81;  /* IN endpoint 1 */
		ep_desc->bmAttributes = 0x03;  /* Interrupt transfer */
		ep_desc->wMaxPacketSize = 64;
		ep_desc->bInterval = 10;  /* 10ms polling interval */
	}

	*config = lib_config;
	return LIBUSB_SUCCESS;
}

void libusb_free_config_descriptor(struct libusb_config_descriptor *config)
{
	if (config == NULL)
		return;
	
	/* Free nested interface and endpoint descriptors */
	if (config->interface != NULL) {
		for (int i = 0; i < config->bNumInterfaces; i++) {
			if (config->interface[i].altsetting != NULL) {
				for (int j = 0; j < config->interface[i].num_altsetting; j++) {
					if (config->interface[i].altsetting[j].endpoint != NULL) {
						free(config->interface[i].altsetting[j].endpoint);
					}
				}
				free(config->interface[i].altsetting);
			}
		}
		free(config->interface);
	}
	
	free(config);
}

int libusb_get_string_descriptor_ascii(libusb_device_handle *handle,
	uint8_t index, unsigned char *data, int length)
{
	if (handle == NULL || data == NULL || length < 0)
		return LIBUSB_ERROR_INVALID_PARAM;
	
	if (length > 0)
		data[0] = '\0';
	return 0;
}

#endif /* __QNX__ */
