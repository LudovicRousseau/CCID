/*
 * libusb_shim.h: libusb API compatibility shim for QNX 8.0
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

#ifndef __LIBUSB_SHIM_H__
#define __LIBUSB_SHIM_H__

#ifdef __QNX__

#include <stdint.h>
#include <sys/types.h>

/* ============================================================================
 * libusb error codes mapping to QNX USB stack codes
 * ============================================================================
 */
typedef enum {
	LIBUSB_SUCCESS = 0,
	LIBUSB_ERROR_IO = -1,
	LIBUSB_ERROR_INVALID_PARAM = -2,
	LIBUSB_ERROR_ACCESS = -3,
	LIBUSB_ERROR_NO_DEVICE = -4,
	LIBUSB_ERROR_NOT_FOUND = -5,
	LIBUSB_ERROR_BUSY = -6,
	LIBUSB_ERROR_TIMEOUT = -7,
	LIBUSB_ERROR_OVERFLOW = -8,
	LIBUSB_ERROR_PIPE = -9,
	LIBUSB_ERROR_INTERRUPTED = -10,
	LIBUSB_ERROR_NO_MEMORY = -11,
	LIBUSB_ERROR_NO_MEM = -11,          /* Alias for NO_MEMORY */
	LIBUSB_ERROR_NOT_SUPPORTED = -12,
	LIBUSB_ERROR_OTHER = -99,
} libusb_error;

/* ============================================================================
 * Device/Descriptor structures
 * ============================================================================
 */
typedef struct libusb_device libusb_device;
typedef struct libusb_device_handle libusb_device_handle;
typedef struct libusb_context libusb_context;
typedef struct libusb_transfer libusb_transfer;

/* Forward declarations for completeness */
struct libusb_device {
	int bus_number;
	int device_address;
	void *os_priv;
};

struct libusb_device_handle {
	libusb_device *device;
	void *os_priv;
};

struct libusb_context {
	void *os_priv;
};

/* Descriptor types */
struct libusb_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
};

struct libusb_endpoint_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
	unsigned char *extra;
	int extra_length;
};

struct libusb_interface_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
	struct libusb_endpoint_descriptor *endpoint;
	unsigned char *extra;
	int extra_length;
};

struct libusb_interface {
	struct libusb_interface_descriptor *altsetting;
	int num_altsetting;
};

struct libusb_config_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t MaxPower;
	struct libusb_interface *interface;
	unsigned char *extra;
	int extra_length;
};

/* Transfer structures */
typedef void (*libusb_transfer_cb_fn)(struct libusb_transfer *transfer);

struct libusb_transfer {
	libusb_device_handle *dev_handle;
	uint8_t endpoint;
	uint8_t type;
	unsigned int timeout;
	enum {
		LIBUSB_TRANSFER_FREE_BUFFER = 1 << 0,
		LIBUSB_TRANSFER_FREE_TRANSFER = 1 << 1,
		LIBUSB_TRANSFER_ADD_ZERO_PACKET = 1 << 2,
	} flags;
	unsigned char *buffer;
	int length;
	int actual_length;
	libusb_transfer_cb_fn callback;
	void *user_data;
	int status;
	void *internal_data;	/* Internal use only */
	libusb_device *usbDevice;  /* Alias for dev_handle */
};

/* ============================================================================
 * Descriptor limits and definitions
 * ============================================================================
 */
#define LIBUSB_DT_DEVICE                    0x01
#define LIBUSB_DT_CONFIG                    0x02
#define LIBUSB_DT_STRING                    0x03
#define LIBUSB_DT_INTERFACE                 0x04
#define LIBUSB_DT_ENDPOINT                  0x05

#define LIBUSB_REQUEST_GET_STATUS           0x00
#define LIBUSB_REQUEST_CLEAR_FEATURE        0x01
#define LIBUSB_REQUEST_SET_FEATURE          0x03
#define LIBUSB_REQUEST_SET_ADDRESS          0x05
#define LIBUSB_REQUEST_GET_DESCRIPTOR       0x06
#define LIBUSB_REQUEST_SET_DESCRIPTOR       0x07
#define LIBUSB_REQUEST_GET_CONFIGURATION    0x08
#define LIBUSB_REQUEST_SET_CONFIGURATION    0x09
#define LIBUSB_REQUEST_GET_INTERFACE        0x0A
#define LIBUSB_REQUEST_SET_INTERFACE        0x0B
#define LIBUSB_REQUEST_SYNCH_FRAME          0x0C

#define LIBUSB_ENDPOINT_IN                  0x80
#define LIBUSB_ENDPOINT_OUT                 0x00
#define LIBUSB_ENDPOINT_DIR_MASK            0x80

#define LIBUSB_TRANSFER_TYPE_CONTROL        0
#define LIBUSB_TRANSFER_TYPE_ISOCHRONOUS    1
#define LIBUSB_TRANSFER_TYPE_BULK           2
#define LIBUSB_TRANSFER_TYPE_INTERRUPT      3

/* Transfer status codes */
#define LIBUSB_TRANSFER_COMPLETED           0
#define LIBUSB_TRANSFER_ERROR               1
#define LIBUSB_TRANSFER_TIMED_OUT           2
#define LIBUSB_TRANSFER_CANCELLED           3
#define LIBUSB_TRANSFER_STALL               4
#define LIBUSB_TRANSFER_NO_DEVICE           5
#define LIBUSB_TRANSFER_OVERFLOW            6

#define LIBUSB_REQUEST_TYPE_STANDARD        (0x00 << 5)
#define LIBUSB_REQUEST_TYPE_CLASS           (0x01 << 5)
#define LIBUSB_REQUEST_TYPE_VENDOR          (0x02 << 5)
#define LIBUSB_RECIPIENT_DEVICE             0x00
#define LIBUSB_RECIPIENT_INTERFACE          0x01
#define LIBUSB_RECIPIENT_ENDPOINT           0x02

#define LIBUSB_CONTROL_SETUP_SIZE           8

/* ============================================================================
 * Core API Functions
 * ============================================================================
 */

/* Initialize the USB library */
int libusb_init(libusb_context **ctx);

/* Exit the USB library */
void libusb_exit(libusb_context *ctx);

/* Get device list */
ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***devs);

/* Free device list */
void libusb_free_device_list(libusb_device **devs, int unref_devices);

/* Open a device */
int libusb_open(libusb_device *dev, libusb_device_handle **handle);

/* Close a device */
int libusb_close(libusb_device_handle *handle);

/* Get device descriptor */
int libusb_get_device_descriptor(libusb_device *dev, 
	struct libusb_device_descriptor *desc);

/* Get active config descriptor */
int libusb_get_active_config_descriptor(libusb_device *dev,
	struct libusb_config_descriptor **config);

/* Free config descriptor */
void libusb_free_config_descriptor(struct libusb_config_descriptor *config);

/* Get string descriptor */
int libusb_get_string_descriptor_ascii(libusb_device_handle *handle,
	uint8_t index, unsigned char *data, int length);

/* Get bus number */
uint8_t libusb_get_bus_number(libusb_device *dev);

/* Get device address */
uint8_t libusb_get_device_address(libusb_device *dev);

/* Claim interface */
int libusb_claim_interface(libusb_device_handle *dev, int interface);

/* Release interface */
int libusb_release_interface(libusb_device_handle *dev, int interface);

/* Set configuration */
int libusb_set_configuration(libusb_device_handle *dev, int config);

/* Control transfer */
int libusb_control_transfer(libusb_device_handle *dev,
	uint8_t request_type, uint8_t request, uint16_t value,
	uint16_t index, unsigned char *data, uint16_t wLength,
	unsigned int timeout);

/* Bulk transfer */
int libusb_bulk_transfer(libusb_device_handle *dev, unsigned char endpoint,
	unsigned char *data, int length, int *actual_length,
	unsigned int timeout);

/* Interrupt transfer */
int libusb_interrupt_transfer(libusb_device_handle *dev,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual_length, unsigned int timeout);

/* Allocate transfer */
struct libusb_transfer *libusb_alloc_transfer(int iso_packets);

/* Free transfer */
void libusb_free_transfer(struct libusb_transfer *transfer);

/* Submit transfer */
int libusb_submit_transfer(struct libusb_transfer *transfer);

/* Cancel transfer */
int libusb_cancel_transfer(struct libusb_transfer *transfer);

/* Handle events */
int libusb_handle_events_completed(libusb_context *ctx, int *completed);

/* Get error name for debugging */
const char *libusb_error_name(int errcode);

/* ============================================================================
 * Macro helpers for transfer setup (compatibility)
 * ============================================================================
 */
#define libusb_fill_control_setup(buf, reqtype, req, value, index, len) do { \
	(buf)[0] = (reqtype); \
	(buf)[1] = (req); \
	(buf)[2] = (value) & 0xff; \
	(buf)[3] = ((value) >> 8) & 0xff; \
	(buf)[4] = (index) & 0xff; \
	(buf)[5] = ((index) >> 8) & 0xff; \
	(buf)[6] = (len) & 0xff; \
	(buf)[7] = ((len) >> 8) & 0xff; \
} while(0)

#define libusb_fill_control_transfer(transfer, dev_handle, setup, data, \
	len, cb, user_data, timeout) do { \
	(transfer)->dev_handle = (dev_handle); \
	(transfer)->endpoint = 0; \
	(transfer)->type = LIBUSB_TRANSFER_TYPE_CONTROL; \
	(transfer)->timeout = (timeout); \
	(transfer)->buffer = (data); \
	(transfer)->length = (len); \
	(transfer)->user_data = (user_data); \
	(transfer)->callback = (cb); \
} while(0)

#define libusb_fill_interrupt_transfer(transfer, dev_handle, endpoint, \
	data, len, cb, user_data, timeout) do { \
	(transfer)->dev_handle = (dev_handle); \
	(transfer)->endpoint = (endpoint); \
	(transfer)->type = LIBUSB_TRANSFER_TYPE_INTERRUPT; \
	(transfer)->timeout = (timeout); \
	(transfer)->buffer = (data); \
	(transfer)->length = (len); \
	(transfer)->user_data = (user_data); \
	(transfer)->callback = (cb); \
} while(0)

#define libusb_fill_bulk_transfer(transfer, dev_handle, endpoint, data, \
	len, cb, user_data, timeout) do { \
	(transfer)->dev_handle = (dev_handle); \
	(transfer)->endpoint = (endpoint); \
	(transfer)->type = LIBUSB_TRANSFER_TYPE_BULK; \
	(transfer)->timeout = (timeout); \
	(transfer)->buffer = (data); \
	(transfer)->length = (len); \
	(transfer)->user_data = (user_data); \
	(transfer)->callback = (cb); \
} while(0)

#else

/* If not QNX, use the real libusb.h */
#include <libusb.h>

#endif /* __QNX__ */

#endif /* __LIBUSB_SHIM_H__ */
