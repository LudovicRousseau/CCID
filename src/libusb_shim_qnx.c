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
	int active_config_idx;             /* Active configuration index */
} qnx_device_info_t;

#define MAX_PIPES 32
#define MAX_CONFIG_SEARCH 4
#define DEFAULT_EVENT_BUF_SIZE 0

/* Internal handle structure */
typedef struct {
	struct usbd_device *dev;
	struct usbd_pipe *pipes[MAX_PIPES];       /* Indexed by (addr & 0x0F) + (addr & 0x80 ? 16 : 0) */
	usbd_descriptors_t *pipe_descs[MAX_PIPES]; /* Store descriptors for open pipes to ensure they persist */
} qnx_device_handle_t;

/* Synchronous transfer context */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int completed;
} sync_transfer_t;

static void sync_transfer_cb(struct usbd_urb *urb, struct usbd_pipe *pipe, void *user_data) {
    sync_transfer_t *ctx = (sync_transfer_t *)user_data;
    pthread_mutex_lock(&ctx->mutex);
    ctx->completed = 1;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->mutex);
}

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

	/* Wait a bit for the stack to settle/read descriptors */
	usleep(100000);

	/* Get device descriptor */
	struct usbd_desc_node *node = NULL;
	usbd_device_descriptor_t *desc = usbd_device_descriptor(dev, &node);
	if (desc == NULL) {
		fprintf(stderr, "[libusb] Failed to get device descriptor (errno=%d)\n", errno);
		usbd_detach(dev);
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
	qnx_devices[qnx_device_count].active_config_idx = 0;
	
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
		.path = NULL,         /* Use default path (NULL) as /dev/io-usb-otg might not exist */
		.vusb = USBD_VERSION,
		.vusbd = USBD_VERSION,
		.flags = 0,
		.argc = 0,
		.argv = NULL,
		.evtbufsz = 0,
		.ident = NULL,        /* NULL ident means match all devices */
		.funcs = &funcs,
		.connect_wait = 0,    /* Do not wait, return immediately if stack is present */
		.evtprio = 0,
	};

	fprintf(stderr, "[libusb] Calling usbd_connect (path=NULL)...\n");

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
	
	/* Give the callback some time to enumerate existing devices.
	 * QNX USB stack callbacks are asynchronous.
	 * Increased to 3s to ensure we catch devices before main thread continues.
	 */
	sleep(3);

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
	
	/* Allocate internal QNX handle */
	qnx_device_handle_t *qnx_handle = malloc(sizeof(qnx_device_handle_t));
	if (qnx_handle == NULL) {
		free(h);
		return LIBUSB_ERROR_NO_MEM;
	}
	
	qnx_handle->dev = qnx_dev->dev;
	for (int i = 0; i < MAX_PIPES; i++) {
		qnx_handle->pipes[i] = NULL;
		qnx_handle->pipe_descs[i] = NULL;
	}
	
	h->device = dev;
	h->os_priv = (void *)qnx_handle;
	*handle = h;
	return LIBUSB_SUCCESS;
}

int libusb_close(libusb_device_handle *handle)
{
	fprintf(stderr, "[libusb] libusb_close called\n");
	
	if (handle == NULL) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}
	
	if (handle->os_priv != NULL) {
		qnx_device_handle_t *qnx_handle = (qnx_device_handle_t *)handle->os_priv;
		
		/* Close any open pipes */
		for (int i = 0; i < MAX_PIPES; i++) {
			if (qnx_handle->pipes[i] != NULL) {
				usbd_close_pipe(qnx_handle->pipes[i]);
				qnx_handle->pipes[i] = NULL;
			}
			if (qnx_handle->pipe_descs[i] != NULL) {
				free(qnx_handle->pipe_descs[i]);
				qnx_handle->pipe_descs[i] = NULL;
			}
		}
		
		free(qnx_handle);
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

static int perform_synchronous_transfer(struct usbd_urb *urb, struct usbd_pipe *pipe, 
                                      uint32_t timeout, int *actual_length)
{
    sync_transfer_t ctx;
    pthread_mutex_init(&ctx.mutex, NULL);
    pthread_cond_init(&ctx.cond, NULL);
    ctx.completed = 0;

    int ret = usbd_io(urb, pipe, sync_transfer_cb, &ctx, timeout);
    
    if (ret == EOK) {
        /* Wait for callback */
        pthread_mutex_lock(&ctx.mutex);
        while (!ctx.completed) {
            pthread_cond_wait(&ctx.cond, &ctx.mutex);
        }
        pthread_mutex_unlock(&ctx.mutex);
        
        pthread_mutex_destroy(&ctx.mutex);
        pthread_cond_destroy(&ctx.cond);

        uint32_t transferred = 0;
        uint32_t status = 0;
        usbd_urb_status(urb, &status, &transferred);
        if (actual_length) {
            *actual_length = (int)transferred;
        }
        
        /* fprintf(stderr, "[libusb] Transfer completed: %d bytes, status=0x%x\n", 
                actual_length ? *actual_length : 0, status); */
        
        if (status & USBD_STATUS_CMP) return 0;
        if (status & USBD_STATUS_TIMEOUT) return LIBUSB_ERROR_TIMEOUT;
        if (status & USBD_STATUS_STALL) return LIBUSB_ERROR_PIPE;
        if (status & USBD_STATUS_ABORTED) return LIBUSB_ERROR_INTERRUPTED;
        return LIBUSB_ERROR_IO;
    } else {
        fprintf(stderr, "[libusb] Transfer submission failed: %d\n", ret);
        pthread_mutex_destroy(&ctx.mutex);
        pthread_cond_destroy(&ctx.cond);
        return LIBUSB_ERROR_IO;
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

	/* fprintf(stderr, "[libusb] libusb_interrupt_transfer: ep=%02x len=%d timeout=%u\n",
		endpoint, length, timeout); */

	qnx_device_handle_t *qnx_handle = (qnx_device_handle_t *)dev->os_priv;
	int pipe_idx = (endpoint & 0x0F) + ((endpoint & 0x80) ? 16 : 0);
	
	if (pipe_idx >= 32 || qnx_handle->pipes[pipe_idx] == NULL) {
		fprintf(stderr, "[libusb] Pipe not open for endpoint 0x%02x\n", endpoint);
		return LIBUSB_ERROR_PIPE;
	}
	
	struct usbd_pipe *pipe = qnx_handle->pipes[pipe_idx];
	struct usbd_urb *urb = usbd_alloc_urb(NULL);
	if (urb == NULL) return LIBUSB_ERROR_NO_MEM;
	
	/* Use user buffer directly (no DMA allocation) */
	void *transfer_buffer = data;
	
	/* Setup URB */
	uint32_t flags = (endpoint & 0x80) ? URB_DIR_IN : URB_DIR_OUT;
	flags |= URB_SHORT_XFER_OK;
	
	if (!(endpoint & 0x80) && length > 0) {
		if (length >= 4) {
			/* fprintf(stderr, "[libusb] Data[0-3]: %02x %02x %02x %02x\n", 
				data[0], data[1], data[2], data[3]); */
		}
	}
	
	int setup_ret = usbd_setup_interrupt(urb, flags, transfer_buffer, length);
	if (setup_ret != EOK) {
		fprintf(stderr, "[libusb] usbd_setup_interrupt failed: %d\n", setup_ret);
		usbd_free_urb(urb);
		return LIBUSB_ERROR_OTHER;
	}
	
	/* Execute URB using synchronous wrapper */
	int ret = perform_synchronous_transfer(urb, pipe, timeout, actual_length);
	usbd_free_urb(urb);
	return ret;
}

int libusb_bulk_transfer(libusb_device_handle *dev, unsigned char endpoint,
	unsigned char *data, int length, int *actual_length,
	unsigned int timeout)
{
	if (dev == NULL || data == NULL || actual_length == NULL)
		return LIBUSB_ERROR_INVALID_PARAM;

	/* fprintf(stderr, "[libusb] libusb_bulk_transfer: ep=%02x len=%d timeout=%u\n",
		endpoint, length, timeout); */

	qnx_device_handle_t *qnx_handle = (qnx_device_handle_t *)dev->os_priv;
	int pipe_idx = (endpoint & 0x0F) + ((endpoint & 0x80) ? 16 : 0);
	
	if (pipe_idx >= 32 || qnx_handle->pipes[pipe_idx] == NULL) {
		fprintf(stderr, "[libusb] Pipe not open for endpoint 0x%02x\n", endpoint);
		return LIBUSB_ERROR_PIPE;
	}
	
	struct usbd_pipe *pipe = qnx_handle->pipes[pipe_idx];
	struct usbd_urb *urb = usbd_alloc_urb(NULL);
	if (urb == NULL) return LIBUSB_ERROR_NO_MEM;
	
	/* Allocate DMA-safe buffer */
	void *transfer_buffer = NULL;
	if (length > 0) {
		transfer_buffer = usbd_alloc(length);
		if (transfer_buffer == NULL) {
			usbd_free_urb(urb);
			return LIBUSB_ERROR_NO_MEM;
		}
	}
	
	/* Setup URB */
	uint32_t flags = (endpoint & 0x80) ? URB_DIR_IN : URB_DIR_OUT;
	if (endpoint & 0x80) {
		flags |= URB_SHORT_XFER_OK;
	}
	
	/* Fix: Check direction using endpoint address, as URB_DIR_OUT might be 0 */
	if (!(endpoint & 0x80) && length > 0) {
		/* fprintf(stderr, "[libusb] Copying %d bytes to DMA buffer for OUT transfer\n", length); */
		memcpy(transfer_buffer, data, length);
		if (length >= 4) {
			/* fprintf(stderr, "[libusb] Data[0-3]: %02x %02x %02x %02x\n", 
				data[0], data[1], data[2], data[3]); */
		}
	}
	
	/* fprintf(stderr, "[libusb] usbd_setup_bulk: urb=%p flags=0x%x buf=%p len=%d\n", 
        urb, flags, transfer_buffer, length); */
	
	int setup_ret = usbd_setup_bulk(urb, flags, transfer_buffer, length);
	if (setup_ret != EOK) {
		fprintf(stderr, "[libusb] usbd_setup_bulk failed: %d\n", setup_ret);
		if (transfer_buffer) usbd_free(transfer_buffer);
		usbd_free_urb(urb);
		return LIBUSB_ERROR_OTHER;
	}
	
	/* Execute URB using synchronous wrapper */
	int ret = perform_synchronous_transfer(urb, pipe, timeout, actual_length);
	
	if (ret == 0) {
		if (flags & URB_DIR_IN && *actual_length > 0) {
			memcpy(data, transfer_buffer, *actual_length);
		}
	}
	
	if (transfer_buffer) usbd_free(transfer_buffer);
	usbd_free_urb(urb);
	
	return ret;
}

int libusb_claim_interface(libusb_device_handle *dev, int interface)
{
	fprintf(stderr, "[libusb] libusb_claim_interface: interface=%d\n", interface);
	
	if (dev == NULL) return LIBUSB_ERROR_INVALID_PARAM;
	
	qnx_device_handle_t *qnx_handle = (qnx_device_handle_t *)dev->os_priv;
	qnx_device_info_t *qnx_dev = (qnx_device_info_t *)dev->device;
	
	/* We need to find the interface descriptor to get endpoints */
	struct usbd_desc_node *node = NULL;
	usbd_configuration_descriptor_t *qnx_config = usbd_configuration_descriptor(qnx_dev->dev, qnx_dev->active_config_idx, &node);
	
	/* If default config fails, try to find a valid one */
	if (qnx_config == NULL) {
		fprintf(stderr, "[libusb] Config descriptor index %d returned NULL, attempting iteration\n", qnx_dev->active_config_idx);
		for (int cfg_idx = 0; cfg_idx < 4; cfg_idx++) {
			if (cfg_idx == qnx_dev->active_config_idx) continue;
			node = NULL;
			usbd_configuration_descriptor_t *temp_config = usbd_configuration_descriptor(qnx_dev->dev, cfg_idx, &node);
			if (temp_config != NULL) {
				fprintf(stderr, "[libusb] Found config descriptor at index %d\n", cfg_idx);
				qnx_config = temp_config;
				qnx_dev->active_config_idx = cfg_idx;
				break;
			}
		}
	}

	if (qnx_config == NULL) {
		fprintf(stderr, "[libusb] Failed to get active config descriptor\n");
		return LIBUSB_ERROR_OTHER;
	}
	
	/* Ensure the configuration is selected */
	int cfg_ret = usbd_select_config(qnx_dev->dev, qnx_config->bConfigurationValue);
	if (cfg_ret != EOK) {
		fprintf(stderr, "[libusb] usbd_select_config failed: %d\n", cfg_ret);
		/* Continue anyway, as it might already be selected */
	} else {
		fprintf(stderr, "[libusb] Selected configuration %d\n", qnx_config->bConfigurationValue);
	}
	
	/* Find the interface */
	for (int i = 0; i < qnx_config->bNumInterfaces; i++) {
		struct usbd_desc_node *if_node = NULL;
		usbd_interface_descriptor_t *qnx_if_desc = usbd_interface_descriptor(qnx_dev->dev, qnx_dev->active_config_idx, i, 0, &if_node);
		
		if (qnx_if_desc != NULL && qnx_if_desc->bInterfaceNumber == interface) {
			fprintf(stderr, "[libusb] Found interface %d (idx %d), endpoints: %d\n", interface, i, qnx_if_desc->bNumEndpoints);
			
			/* Select the interface/alternate setting only if it's not the default (0).
			 * Calling SetInterface(0) when already at 0 can cause timeouts on some devices
			 * or if the interface is already active.
			 */
			if (qnx_if_desc->bAlternateSetting != 0) {
				int if_ret = usbd_select_interface(qnx_dev->dev, qnx_if_desc->bInterfaceNumber, qnx_if_desc->bAlternateSetting);
				if (if_ret != EOK) {
					fprintf(stderr, "[libusb] usbd_select_interface failed: %d\n", if_ret);
				} else {
					fprintf(stderr, "[libusb] Selected interface %d alt %d\n", qnx_if_desc->bInterfaceNumber, qnx_if_desc->bAlternateSetting);
				}
			} else {
				fprintf(stderr, "[libusb] Skipping SetInterface for Alt Setting 0 (default)\n");
			}
			
			/* Open pipes for all endpoints using parsing logic */
			int ep_found_count = 0;
			struct usbd_desc_node *next_node = NULL;
			
			fprintf(stderr, "[libusb] Scanning %d endpoints for Interface %d\n", qnx_if_desc->bNumEndpoints, interface);

			/* Use index-based scanning from the interface node, matching qnx_reference_test.c */
			int endpoint_index = 0;
			usbd_endpoint_descriptor_t *qnx_ep_desc;

			while ((qnx_ep_desc = (usbd_endpoint_descriptor_t *)usbd_parse_descriptors(
					qnx_dev->dev, if_node, USB_DESC_ENDPOINT, endpoint_index, &next_node)) != NULL) {
				
				endpoint_index++;
				
				if (qnx_ep_desc->bEndpointAddress == 0x00) {
					fprintf(stderr, "[libusb] Skipping Control Endpoint 0x00 at index %d\n", endpoint_index-1);
					continue;
				}

				fprintf(stderr, "[libusb] Found Endpoint 0x%02x (Attr 0x%02x) MaxPacket=%d at index %d\n", 
					qnx_ep_desc->bEndpointAddress, qnx_ep_desc->bmAttributes, qnx_ep_desc->wMaxPacketSize, endpoint_index-1);

				/* Open Pipe Logic */
				struct usbd_pipe *pipe = NULL;
				
				/* Allocate a persistent descriptor union for the pipe. */
				usbd_descriptors_t *desc_union = malloc(sizeof(usbd_descriptors_t));
				if (desc_union == NULL) {
					fprintf(stderr, "[libusb] Failed to allocate descriptor memory\n");
					return LIBUSB_ERROR_NO_MEM;
				}
				memset(desc_union, 0, sizeof(usbd_descriptors_t));
				memcpy(&desc_union->endpoint, qnx_ep_desc, qnx_ep_desc->bLength);
				
				int ret = usbd_open_pipe(qnx_dev->dev, desc_union, &pipe);
				if (ret == EOK) {
					int pipe_idx = (qnx_ep_desc->bEndpointAddress & 0x0F) + ((qnx_ep_desc->bEndpointAddress & 0x80) ? 16 : 0);
					if (pipe_idx < 32) {
						qnx_handle->pipes[pipe_idx] = pipe;
						qnx_handle->pipe_descs[pipe_idx] = desc_union;
						fprintf(stderr, "[libusb] Pipe opened for ep 0x%02x (idx %d) pipe=%p\n", qnx_ep_desc->bEndpointAddress, pipe_idx, pipe);
					} else {
						free(desc_union);
					}
				} else {
					fprintf(stderr, "[libusb] Failed to open pipe for ep 0x%02x: %d\n", qnx_ep_desc->bEndpointAddress, ret);
					free(desc_union);
				}
				ep_found_count++;
			}
			
			fprintf(stderr, "[libusb] Endpoint scan complete. Found %d endpoints.\n", ep_found_count);
			return 0;
		}
	}
	
	fprintf(stderr, "[libusb] Interface %d not found\n", interface);
	return LIBUSB_ERROR_NOT_FOUND;
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
	fprintf(stderr, "[libusb] DEBUG: qnx_dev->dev = %p\n", qnx_dev->dev);
	usbd_configuration_descriptor_t *qnx_config = usbd_configuration_descriptor(qnx_dev->dev, 0, &node);
	fprintf(stderr, "[libusb] DEBUG: usbd_configuration_descriptor returned %p, node = %p\n", qnx_config, node);
	
	/* Track which config index we successfully retrieved */
	int config_idx = 0;
	
	/* If NULL, try to get the first available configuration by iterating */
	if (qnx_config == NULL) {
		fprintf(stderr, "[libusb] Config descriptor index 0 returned NULL, attempting iteration\n");
		for (int cfg_idx = 1; cfg_idx < 4; cfg_idx++) {
			node = NULL;
			usbd_configuration_descriptor_t *temp_config = usbd_configuration_descriptor(qnx_dev->dev, cfg_idx, &node);
			if (temp_config != NULL) {
				fprintf(stderr, "[libusb] Found config descriptor at index %d\n", cfg_idx);
				qnx_config = temp_config;
				config_idx = cfg_idx;
				break;
			}
		}
	}
	
	qnx_dev->active_config_idx = config_idx;
	
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

		/* Allocate interface array */
		if (qnx_config->bNumInterfaces > 0) {
			lib_config->interface = malloc(sizeof(struct libusb_interface) * qnx_config->bNumInterfaces);
			if (lib_config->interface == NULL) {
				free(lib_config);
				return LIBUSB_ERROR_NO_MEM;
			}
			memset(lib_config->interface, 0, sizeof(struct libusb_interface) * qnx_config->bNumInterfaces);

			/* Get interface descriptors from QNX USB stack using the correct config index */
			for (int i = 0; i < qnx_config->bNumInterfaces; i++) {
				struct usbd_desc_node *if_node = NULL;
				usbd_interface_descriptor_t *qnx_if_desc = usbd_interface_descriptor(qnx_dev->dev, config_idx, i, 0, &if_node);
				
				if (qnx_if_desc != NULL) {
					fprintf(stderr, "[libusb] Found interface descriptor %d from QNX\n", i);
					
					lib_config->interface[i].num_altsetting = 1;
					lib_config->interface[i].altsetting = malloc(sizeof(struct libusb_interface_descriptor));
					if (lib_config->interface[i].altsetting == NULL) {
						free(lib_config->interface);
						free(lib_config);
						return LIBUSB_ERROR_NO_MEM;
					}
					
					struct libusb_interface_descriptor *if_desc = lib_config->interface[i].altsetting;
					memset(if_desc, 0, sizeof(struct libusb_interface_descriptor));
					
					/* Copy interface descriptor from QNX */
					if_desc->bLength = qnx_if_desc->bLength;
					if_desc->bDescriptorType = qnx_if_desc->bDescriptorType;
					if_desc->bInterfaceNumber = qnx_if_desc->bInterfaceNumber;
					if_desc->bAlternateSetting = qnx_if_desc->bAlternateSetting;
					if_desc->bNumEndpoints = qnx_if_desc->bNumEndpoints;
					if_desc->bInterfaceClass = qnx_if_desc->bInterfaceClass;
					if_desc->bInterfaceSubClass = qnx_if_desc->bInterfaceSubClass;
					if_desc->bInterfaceProtocol = qnx_if_desc->bInterfaceProtocol;
					if_desc->iInterface = qnx_if_desc->iInterface;
					
					fprintf(stderr, "[libusb] Interface %d: num=%d alt=%d class: 0x%02x, endpoints: %d (node=%p)\n",
						i, if_desc->bInterfaceNumber, if_desc->bAlternateSetting, if_desc->bInterfaceClass, if_desc->bNumEndpoints, if_node);
					

					
					/* Allocate and get endpoint descriptors */
					if (qnx_if_desc->bNumEndpoints > 0) {
						if_desc->endpoint = malloc(sizeof(struct libusb_endpoint_descriptor) * qnx_if_desc->bNumEndpoints);
						if (if_desc->endpoint == NULL) {
							free(lib_config->interface[i].altsetting);
							free(lib_config->interface);
							free(lib_config);
							return LIBUSB_ERROR_NO_MEM;
						}
						memset(if_desc->endpoint, 0, sizeof(struct libusb_endpoint_descriptor) * qnx_if_desc->bNumEndpoints);
						
						fprintf(stderr, "[libusb] Trying to get %d endpoint descriptors\n", qnx_if_desc->bNumEndpoints);
						
						int ep_count = 0;
						
						/* Search for endpoint descriptors by address.
						 * Evidence suggests the 5th argument to usbd_endpoint_descriptor
						 * is the Endpoint Address, not a sequential index.
						 * We search standard endpoint addresses: 1-15 (OUT) and 129-143 (IN).
						 */
						int ep_addresses_to_check[] = {
							0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 
							0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
							0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 
							0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F
						};
						int num_addresses = sizeof(ep_addresses_to_check) / sizeof(ep_addresses_to_check[0]);

						for (int k = 0; k < num_addresses; k++) {
							if (ep_count >= qnx_if_desc->bNumEndpoints)
								break;  /* Found all expected endpoints */
							
							int search_addr = ep_addresses_to_check[k];
							
							struct usbd_desc_node *ep_node = NULL;
							usbd_endpoint_descriptor_t *qnx_ep_desc = usbd_endpoint_descriptor(
								qnx_dev->dev, config_idx, qnx_if_desc->bInterfaceNumber, 
								qnx_if_desc->bAlternateSetting, search_addr, &ep_node);
							
							if (qnx_ep_desc != NULL) {
								/* Skip control endpoint (address 0x00) - shouldn't happen with our list but safe to keep */
								if (qnx_ep_desc->bEndpointAddress == 0x00) {
									continue;
								}
								
								struct libusb_endpoint_descriptor *ep_desc = &if_desc->endpoint[ep_count];
								
								ep_desc->bLength = qnx_ep_desc->bLength;
								ep_desc->bDescriptorType = qnx_ep_desc->bDescriptorType;
								ep_desc->bEndpointAddress = qnx_ep_desc->bEndpointAddress;
								ep_desc->bmAttributes = qnx_ep_desc->bmAttributes;
								ep_desc->wMaxPacketSize = qnx_ep_desc->wMaxPacketSize;
								ep_desc->bInterval = qnx_ep_desc->bInterval;
								
								fprintf(stderr, "[libusb] Found endpoint %d (addr 0x%02x): attr=0x%02x, maxpkt=%d\n",
									ep_count, ep_desc->bEndpointAddress, 
									ep_desc->bmAttributes, ep_desc->wMaxPacketSize);
								ep_count++;
							}
						}
					
					fprintf(stderr, "[libusb] Got %d of %d expected endpoints\n", ep_count, qnx_if_desc->bNumEndpoints);
				}
			} else {
				fprintf(stderr, "[libusb] No interface descriptor %d available from QNX\n", i);
			}
			}
		}
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
