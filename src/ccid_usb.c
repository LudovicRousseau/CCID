/*
    ccid_usb.c: USB access routines using the libusb library
    Copyright (C) 2003   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * $Id$
 */

#define __CCID_USB__

#include <stdio.h>
#include <string.h> 
#include <errno.h>
#include <usb.h>

#include "pcscdefines.h"
#include "config.h"
#include "debug.h"
#include "utils.h"
#include "parser.h"
#include "ccid.h"


/* read timeout
 * we must wait enough so that the card can finish its calculation
 * the card, and then the reader should send TIME REQUEST bytes
 * so this timeout should never occur */
#define USB_READ_TIMEOUT (60 * 1000)	/* 1 minute timeout */

/* write timeout
 * we don't have to wait a long time since the card was doing nothing */
#define USB_WRITE_TIMEOUT (5 * 1000)  /* 5 seconds timeout */


#define BUS_DEVICE_STRSIZE 32

typedef struct
{
	usb_dev_handle *handle;
	struct usb_device *dev;

	/*
	 * Used to store device name string %s/%s like:
	 * 001/002 (Linux)
	 * /dev/usb0//dev/ (FreeBSD)
	 * /dev/usb0//dev/ugen0 (OpenBSD)
	 */
	char device_name[BUS_DEVICE_STRSIZE];

	/*
	 * Endpoints
	 */
	int bulk_in;
	int bulk_out;

	/*
	 * CCID infos common to USB and serial
	 */
	_ccid_descriptor ccid;

} _usbDevice;

/* The _usbDevice structure must be defined before including ccid_usb.h */
#include "ccid_usb.h"

static _usbDevice usbDevice[PCSCLITE_MAX_READERS] = {
	[ 0 ... (PCSCLITE_MAX_READERS-1) ] = { NULL, NULL, "", 0, 0 }
};

#define PCSCLITE_MANUKEY_NAME                   "ifdVendorID"
#define PCSCLITE_PRODKEY_NAME                   "ifdProductID"
#define PCSCLITE_NAMEKEY_NAME                   "ifdFriendlyName"

/*****************************************************************************
 *
 *					OpenUSB
 *
 ****************************************************************************/
status_t OpenUSB(int lun, int Channel)
{
	static struct usb_bus *busses = NULL;
	int reader = LunToReaderIndex(lun);
	int alias = 0;
	struct usb_bus *bus;
	struct usb_dev_handle *dev_handle;
	char keyValue[TOKEN_MAX_VALUE_SIZE];
	int vendorID, productID;
	char infofile[FILENAME_MAX];

	DEBUG_COMM3("OpenUSB: Lun: %X, Channel: %X", lun, Channel);

	if (busses == NULL)
		usb_init();

	usb_find_busses();
	usb_find_devices();

	busses = usb_get_busses();

	if (busses == NULL)
	{
		DEBUG_CRITICAL("No USB busses found");
		return STATUS_UNSUCCESSFUL;
	}

	/* is the lun already used? */
	if (usbDevice[reader].handle != NULL)
	{
		DEBUG_CRITICAL2("USB driver with lun %X already in use", lun);
		return STATUS_UNSUCCESSFUL;
	}

	/* Info.plist full patch filename */
	snprintf(infofile, sizeof(infofile), "%s/%s/Contents/Info.plist",
		PCSCLITE_HP_DROPDIR, BUNDLE);

	/* general driver info */
	if (!LTPBundleFindValueWithKey(infofile, "ifdManufacturerString", keyValue, 0))
		DEBUG_CRITICAL2("Manufacturer: %s", keyValue);
	else
	{
		DEBUG_CRITICAL2("LTPBundleFindValueWithKey error. Can't find %s?",
			infofile);
		return STATUS_UNSUCCESSFUL;
	}
	if (!LTPBundleFindValueWithKey(infofile, "ifdProductString", keyValue, 0))
		DEBUG_CRITICAL2("ProductString: %s", keyValue);
	else
		return STATUS_UNSUCCESSFUL;
	if (!LTPBundleFindValueWithKey(infofile, "Copyright", keyValue, 0))
		DEBUG_CRITICAL2("Copyright: %s", keyValue);
	else
		return STATUS_UNSUCCESSFUL;
	vendorID = strlen(keyValue);
	alias = 0x1D;
	for (; vendorID--;)
		alias ^= keyValue[vendorID];

	/* for any supported reader */
	while (LTPBundleFindValueWithKey(infofile, PCSCLITE_MANUKEY_NAME, keyValue, alias) == 0)
	{
		vendorID = strtoul(keyValue, 0, 16);

		if (LTPBundleFindValueWithKey(infofile, PCSCLITE_PRODKEY_NAME, keyValue, alias))
			goto end;
		productID = strtoul(keyValue, 0, 16);

		if (LTPBundleFindValueWithKey(infofile, PCSCLITE_NAMEKEY_NAME, keyValue, alias))
			goto end;

		/* on any USB buses */
		for (bus = busses; bus; bus = bus->next)
		{
			struct usb_device *dev;

			/* any device on this bus */
			for (dev = bus->devices; dev; dev = dev->next)
			{
				if (dev->descriptor.idVendor == vendorID
					&& dev->descriptor.idProduct == productID)
				{
					int r, already_used;
					char device_name[BUS_DEVICE_STRSIZE];

					if (snprintf(device_name, BUS_DEVICE_STRSIZE, "%s/%s",
						bus->dirname, dev->filename) < 0)
					{
						DEBUG_CRITICAL2("Device name too long: %s", device_name);
						return STATUS_UNSUCCESSFUL;
					}

					/* is it already opened? */
					already_used = FALSE;
					for (r=0; r<PCSCLITE_MAX_READERS; r++)
					{
						if (usbDevice[r].dev)
						{
							DEBUG_COMM3("Checking new device '%s' against old '%s'",
								device_name, usbDevice[r].device_name);
							if (strcmp(usbDevice[r].device_name, device_name) == 0)
								already_used = TRUE;
						}
					}

					if (!already_used)
					{
						DEBUG_COMM2("Trying to open USB bus/device: %s",
							 device_name);

						dev_handle = usb_open(dev);
						if (dev_handle)
						{
							int interface;

							if (dev->config == NULL)
							{
								DEBUG_CRITICAL2("No dev->config found for %s",
									 device_name);
								return STATUS_UNSUCCESSFUL;
							}

							if (dev->config->interface->altsetting->extralen < 54)
							{
								DEBUG_CRITICAL3("Extra field too short for %s: %d", device_name, dev->config->interface->altsetting->extralen);
								return STATUS_UNSUCCESSFUL;
							}

							interface = dev->config->interface->altsetting->bInterfaceNumber;
							if (usb_claim_interface(dev_handle, interface) < 0)
							{
								DEBUG_CRITICAL3("Can't claim interface %s: %s",
									device_name, strerror(errno));
								return STATUS_UNSUCCESSFUL;
							}

							DEBUG_INFO4("Found Vendor/Product: %04X/%04X (%s)",
								dev->descriptor.idVendor,
								dev->descriptor.idProduct, keyValue);
							DEBUG_INFO2("Using USB bus/device: %s",
								 device_name);

							/* Get Endpoints values*/
							get_end_points(dev, &usbDevice[reader]);

							/* store device information */
							usbDevice[reader].handle = dev_handle;
							usbDevice[reader].dev = dev;
							strncpy(usbDevice[reader].device_name,
								device_name, BUS_DEVICE_STRSIZE);

							/* CCID common informations */
							usbDevice[reader].ccid.bSeq = 1;
							usbDevice[reader].ccid.readerID =
								(dev->descriptor.idVendor << 16) +
								dev->descriptor.idProduct;
							usbDevice[reader].ccid.dwFeatures = dw2i(dev->config->interface->altsetting->extra, 40);
							usbDevice[reader].ccid.dwMaxCCIDMessageLength = dw2i(dev->config->interface->altsetting->extra, 44);

							/* Maybe we have a special treatment
							 * for this reader */
							ccid_open_hack(lun);

							goto end;
						}
						else
							DEBUG_CRITICAL3("Can't usb_open(%s): %s",
								device_name,
								strerror(errno));
					}
					else
					{
						DEBUG_INFO2("USB device %s already in use. Checking next one.",
							device_name);
					}
				}
			}
		}

		/* go to next supported reader */
		alias++;
	}
end:
	if (usbDevice[reader].handle == NULL)
		return STATUS_UNSUCCESSFUL;

	return STATUS_SUCCESS;
} /* OpenUSB */


/*****************************************************************************
 *
 *					WriteUSB
 *
 ****************************************************************************/
status_t WriteUSB(int lun, int length, unsigned char *buffer)
{
	int rv;
	int reader = LunToReaderIndex(lun);
#ifdef DEBUG_LEVEL_COMM
	char debug_header[] = "-> 121234 ";

	sprintf(debug_header, "-> %06X ", (int)lun);
#endif

#ifdef DEBUG_LEVEL_COMM
	DEBUG_XXD(debug_header, buffer, length);
#endif

	rv = usb_bulk_write(usbDevice[reader].handle, usbDevice[reader].bulk_out, buffer, length, USB_WRITE_TIMEOUT);

	if (rv < 0)
	{
		DEBUG_CRITICAL3("usb_bulk_write(%s): %s",
			usbDevice[reader].device_name, strerror(errno));
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
} /* WriteUSB */


/*****************************************************************************
 *
 *					ReadUSB
 *
 ****************************************************************************/
status_t ReadUSB(int lun, int * length, unsigned char *buffer)
{
	int rv;
	int reader = LunToReaderIndex(lun);
#ifdef DEBUG_LEVEL_COMM
	char debug_header[] = "<- 121234 ";

	sprintf(debug_header, "<- %06X ", (int)lun);
#endif


	rv = usb_bulk_read(usbDevice[reader].handle, usbDevice[reader].bulk_in, buffer, *length, USB_READ_TIMEOUT);
	*length = rv;

	if (rv < 0)
	{
		DEBUG_CRITICAL3("usb_bulk_read(%s): %s",
			usbDevice[reader].device_name, strerror(errno));
		return STATUS_UNSUCCESSFUL;
	}

#ifdef DEBUG_LEVEL_COMM
	DEBUG_XXD(debug_header, buffer, *length);
#endif

	return STATUS_SUCCESS;
} /* ReadUSB */


/*****************************************************************************
 *
 *					CloseUSB
 *
 ****************************************************************************/
status_t CloseUSB(int lun)
{
	int reader = LunToReaderIndex(lun);

	/* device not opened */
	if (usbDevice[reader].dev == NULL)
		return STATUS_UNSUCCESSFUL;

	DEBUG_COMM2("Closing USB device: %s", usbDevice[reader].device_name);

	usb_release_interface(usbDevice[reader].handle, usbDevice[reader].dev->config->interface->altsetting->bInterfaceNumber);

	usb_close(usbDevice[reader].handle);

	/* mark the resource unused */
	usbDevice[reader].handle = NULL;
	usbDevice[reader].dev = NULL;
	usbDevice[reader].device_name[0] = '\0';

	return STATUS_SUCCESS;
} /* CloseUSB */


/*****************************************************************************
 *
 *					get_ccid_descriptor
 *
 ****************************************************************************/
_ccid_descriptor *get_ccid_descriptor(int lun)
{
	return &usbDevice[LunToReaderIndex(lun)].ccid;
} /* get_ccid_descriptor */


/*****************************************************************************
 *
 *					get_desc
 *
 ****************************************************************************/
int get_desc(int channel, char *device_name[], usb_dev_handle **handle, struct
	usb_device **dev)
{
	if (channel < 0 || channel > PCSCLITE_MAX_READERS)
		return 1;

	*device_name = usbDevice[channel].device_name;
	*handle = usbDevice[channel].handle;
	*dev = usbDevice[channel].dev;

	return 0;
} /* get_desc */

/*****************************************************************************
 *
 *					get_end_points
 *
 ****************************************************************************/
int get_end_points(struct usb_device *dev, _usbDevice *usb_device)
{
	int i;
	int bEndpointAddress;

	/*
	 * 3 Endpoints maximum: Interrupt In, Bulk In, Bulk Out
	 */
	for (i=0; i<3; i++)
	{
		if (dev->config->interface->altsetting->endpoint[i].bmAttributes != USB_ENDPOINT_TYPE_BULK)
			continue;

		bEndpointAddress = dev->config->interface->altsetting->endpoint[i].bEndpointAddress;

		if ((bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN)
			usb_device->bulk_in = bEndpointAddress;

		if ((bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT)
			usb_device->bulk_out = bEndpointAddress;
	}

	return 0;
} /* get_end_points */

