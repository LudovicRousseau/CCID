/*
    ccid_usb.c: USB access routines using the libusb library
    Copyright (C) 2003-2009   Ludovic Rousseau

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*
 * $Id$
 */

#define __CCID_USB__

#include <stdio.h>
#include <string.h>
#include <errno.h>
# ifdef S_SPLINT_S
# include <sys/types.h>
# endif
#include <usb.h>
#include <ifdhandler.h>

#include "misc.h"
#include "ccid.h"
#include "config.h"
#include "debug.h"
#include "defs.h"
#include "utils.h"
#include "parser.h"
#include "ccid_ifdhandler.h"


/* write timeout
 * we don't have to wait a long time since the card was doing nothing */
#define USB_WRITE_TIMEOUT (5 * 1000)  /* 5 seconds timeout */

/*
 * Proprietary USB Class (0xFF) are (or are not) accepted
 * A proprietary class is used for devices released before the final CCID
 * specifications were ready.
 * We should not have problems with non CCID devices becasue the
 * Manufacturer and Product ID are also used to identify the device */
#define ALLOW_PROPRIETARY_CLASS

#define BUS_DEVICE_STRSIZE 32

typedef struct
{
	usb_dev_handle *handle;
	char *dirname;
	char *filename;
	int interface;

	/*
	 * Endpoints
	 */
	int bulk_in;
	int bulk_out;
	int interrupt;

	/* Number of slots using the same device */
	int real_nb_opened_slots;
	int *nb_opened_slots;

	/*
	 * CCID infos common to USB and serial
	 */
	_ccid_descriptor ccid;

} _usbDevice;

/* The _usbDevice structure must be defined before including ccid_usb.h */
#include "ccid_usb.h"

static int get_end_points(struct usb_device *dev, _usbDevice *usbdevice, int num);
int ccid_check_firmware(struct usb_device *dev);
static unsigned int *get_data_rates(unsigned int reader_index,
	struct usb_device *dev, int num);

/* ne need to initialize to 0 since it is static */
static _usbDevice usbDevice[CCID_DRIVER_MAX_READERS];

#define PCSCLITE_MANUKEY_NAME                   "ifdVendorID"
#define PCSCLITE_PRODKEY_NAME                   "ifdProductID"
#define PCSCLITE_NAMEKEY_NAME                   "ifdFriendlyName"

struct _bogus_firmware
{
	int vendor;		/* idVendor */
	int product;	/* idProduct */
	int firmware;	/* bcdDevice: previous firmwares have bugs */
};

static struct _bogus_firmware Bogus_firmwares[] = {
	{ 0x04e6, 0xe001, 0x0516 },	/* SCR 331 */
	{ 0x04e6, 0x5111, 0x0620 },	/* SCR 331-DI */
	{ 0x04e6, 0x5115, 0x0514 },	/* SCR 335 */
	{ 0x04e6, 0xe003, 0x0510 },	/* SPR 532 */
	{ 0x0D46, 0x3001, 0x0037 },	/* KAAN Base */
	{ 0x0D46, 0x3002, 0x0037 },	/* KAAN Advanced */
	{ 0x09C3, 0x0008, 0x0203 },	/* ActivCard V2 */
	{ 0x0DC3, 0x1004, 0x0502 },	/* ASE IIIe USBv2 */
	{ 0x0DC3, 0x1102, 0x0607 },	/* ASE IIIe KB USB */
	{ 0x058F, 0x9520, 0x0102 },	/* Alcor AU9520-G */
	{ 0x072F, 0x2200, 0x0206 }, /* ACS ACR122U-WB-R */

	/* the firmware version is not correct since I do not have received a
	 * working reader yet */
#ifndef O2MICRO_OZ776_PATCH
	{ 0x0b97, 0x7762, 0x0111 },	/* Oz776S */
#endif
};

/* data rates supported by the secondary slots on the GemCore Pos Pro & SIM Pro */
unsigned int SerialCustomDataRates[] = { GEMPLUS_CUSTOM_DATA_RATES, 0 };


/*****************************************************************************
 *
 *					OpenUSB
 *
 ****************************************************************************/
status_t OpenUSB(unsigned int reader_index, /*@unused@*/ int Channel)
{
	return OpenUSBByName(reader_index, NULL);
} /* OpenUSB */


/*****************************************************************************
 *
 *					OpenUSBByName
 *
 ****************************************************************************/
status_t OpenUSBByName(unsigned int reader_index, /*@null@*/ char *device)
{
	static struct usb_bus *busses = NULL;
	int alias = 0;
	struct usb_bus *bus;
	struct usb_dev_handle *dev_handle;
	char keyValue[TOKEN_MAX_VALUE_SIZE];
	unsigned int vendorID, productID;
	char infofile[FILENAME_MAX];
#ifndef __APPLE__
	unsigned int device_vendor, device_product;
#endif
	char *dirname = NULL, *filename = NULL;
	char *serial = NULL;
	int interface_number = -1;
	static int previous_reader_index = -1;

	DEBUG_COMM3("Reader index: %X, Device: %s", reader_index, device);

#ifndef __APPLE__
	/* device name specified */
	if (device)
	{
		/* format: usb:%04x/%04x, vendor, product */
		if (strncmp("usb:", device, 4) != 0)
		{
			DEBUG_CRITICAL2("device name does not start with \"usb:\": %s",
				device);
			return STATUS_UNSUCCESSFUL;
		}

		if (sscanf(device, "usb:%x/%x", &device_vendor, &device_product) != 2)
		{
			DEBUG_CRITICAL2("device name can't be parsed: %s", device);
			return STATUS_UNSUCCESSFUL;
		}

		/* format usb:%04x/%04x:libusb:%s
		 * with %s set to %s:%s, dirname, filename */
		if ((dirname = strstr(device, "libusb:")) != NULL)
		{
			/* dirname points to the first char after libusb: */
			dirname += strlen("libusb:");

			/* search the : (separation) char */
			filename = strchr(dirname, ':');

			if (filename)
			{
				/* end the dirname string */
				*filename = '\0';

				/* filename points to the first char after : */
				filename++;
			}
			else
			{
				/* parse failed */
				dirname = NULL;

				DEBUG_CRITICAL2("can't parse using libusb scheme: %s", device);
			}
		}

		/* format usb:%04x/%04x:libhal:%s
		 * with %s set to
		 * /org/freedesktop/Hal/devices/usb_device_VID_PID_SERIAL_ifX
		 * VID is VendorID
		 * PID is ProductID
		 * SERIAL is device serial number
		 * X is the interface number
		 */
		if ((dirname = strstr(device, "libhal:")) != NULL)
		{
			char *p;

#define HAL_HEADER "usb_device_"

			/* parse the hal string */
			if (
				/* search the last '/' char */
				(p = strrchr(dirname, '/'))

				/* if the string starts with "usb_device_" we continue */
				&& (0 == strncmp(++p, HAL_HEADER, sizeof(HAL_HEADER)-1))
				/* skip the HAL header */
				&& (p += sizeof(HAL_HEADER)-1)

				/* search the '_' before PID */
				&& (p = strchr(++p, '_'))

				/* search the '_' before SERIAL */
				&& (p = strchr(++p, '_'))
				&& (serial = p+1)

				/* search the '_' before ifX */
				&& (p = strchr(++p, '_'))
				&& (0 == strncmp(++p, "if", 2))
			   )
			{
				/* terminate the serial number C-string */
				*(p-1) = '\0';

				/* convert the interface number */
				interface_number = atoi(p+2);
			}
			else
				DEBUG_CRITICAL2("can't parse using libhal scheme: %s", device);

			/* dirname was just a temporary variable */
			dirname = NULL;
		}
	}
#endif

	if (busses == NULL)
		usb_init();

	(void)usb_find_busses();
	(void)usb_find_devices();

	busses = usb_get_busses();

	if (busses == NULL)
	{
		DEBUG_CRITICAL("No USB busses found");
		return STATUS_UNSUCCESSFUL;
	}

	/* is the reader_index already used? */
	if (usbDevice[reader_index].handle != NULL)
	{
		DEBUG_CRITICAL2("USB driver with index %X already in use",
			reader_index);
		return STATUS_UNSUCCESSFUL;
	}

	/* Info.plist full patch filename */
	(void)snprintf(infofile, sizeof(infofile), "%s/%s/Contents/Info.plist",
		PCSCLITE_HP_DROPDIR, BUNDLE);

	/* general driver info */
	if (!LTPBundleFindValueWithKey(infofile, "ifdManufacturerString", keyValue, 0))
	{
		DEBUG_INFO2("Manufacturer: %s", keyValue);
	}
	else
	{
		DEBUG_INFO2("LTPBundleFindValueWithKey error. Can't find %s?",
			infofile);
		return STATUS_UNSUCCESSFUL;
	}
	if (!LTPBundleFindValueWithKey(infofile, "ifdProductString", keyValue, 0))
	{
		DEBUG_INFO2("ProductString: %s", keyValue);
	}
	else
		return STATUS_UNSUCCESSFUL;
	if (!LTPBundleFindValueWithKey(infofile, "Copyright", keyValue, 0))
	{
		DEBUG_INFO2("Copyright: %s", keyValue);
	}
	else
		return STATUS_UNSUCCESSFUL;
	vendorID = strlen(keyValue);
	alias = 0x1C;
	for (; vendorID--;)
		alias ^= keyValue[vendorID];

	/* for any supported reader */
	while (LTPBundleFindValueWithKey(infofile, PCSCLITE_MANUKEY_NAME, keyValue, alias) == 0)
	{
		vendorID = strtoul(keyValue, NULL, 0);

		if (LTPBundleFindValueWithKey(infofile, PCSCLITE_PRODKEY_NAME, keyValue, alias))
			goto end;
		productID = strtoul(keyValue, NULL, 0);

		if (LTPBundleFindValueWithKey(infofile, PCSCLITE_NAMEKEY_NAME, keyValue, alias))
			goto end;

		/* go to next supported reader for next round */
		alias++;

#ifndef __APPLE__
		/* the device was specified but is not the one we are trying to find */
		if (device
			&& (vendorID != device_vendor || productID != device_product))
			continue;
#else
		/* Leopard puts the friendlyname in the device argument */
		if (device && strcmp(device, keyValue))
			continue;
#endif

		/* on any USB buses */
		for (bus = busses; bus; bus = bus->next)
		{
			struct usb_device *dev;

			/* any device on this bus */
			for (dev = bus->devices; dev; dev = dev->next)
			{
				/* device defined by name? */
				if (dirname && (strcmp(dirname, bus->dirname)
					|| strcmp(filename, dev->filename)))
					continue;

				if (dev->descriptor.idVendor == vendorID
					&& dev->descriptor.idProduct == productID)
				{
					int r, already_used;
					struct usb_interface *usb_interface = NULL;
					int interface;
					int num = 0;

#ifdef USE_COMPOSITE_AS_MULTISLOT
					static int static_interface = 1;

					{
						/* simulate a composite device as when libhal is
						 * used */
						int readerID = (vendorID << 16) + productID;

						if ((GEMALTOPROXDU == readerID)
							|| (GEMALTOPROXSU == readerID))
						{
							if(interface_number >= 0)
							{
								DEBUG_CRITICAL("USE_COMPOSITE_AS_MULTISLOT can't be used with libhal");
								continue;
							}

							/* the CCID interfaces are 1 and 2 */
							interface_number = static_interface;
						}
					}
#endif
					/* is it already opened? */
					already_used = FALSE;

					DEBUG_COMM3("Checking device: %s/%s",
						bus->dirname, dev->filename);
					for (r=0; r<CCID_DRIVER_MAX_READERS; r++)
					{
						if (usbDevice[r].handle)
						{
							/* same busname, same filename */
							if (strcmp(usbDevice[r].dirname, bus->dirname) == 0 && strcmp(usbDevice[r].filename, dev->filename) == 0)
								already_used = TRUE;
						}
					}

					/* this reader is already managed by us */
					if (already_used)
					{
						if ((previous_reader_index != -1)
							&& usbDevice[previous_reader_index].handle
							&& (strcmp(usbDevice[previous_reader_index].dirname, bus->dirname)  == 0)
							&& (strcmp(usbDevice[previous_reader_index].filename, dev->filename) == 0)
							&& usbDevice[previous_reader_index].ccid.bCurrentSlotIndex < usbDevice[previous_reader_index].ccid.bMaxSlotIndex)
						{
							/* we reuse the same device
							 * and the reader is multi-slot */
							usbDevice[reader_index] = usbDevice[previous_reader_index];
							/* the other slots do not have the same data rates */
							if ((GEMCOREPOSPRO == usbDevice[reader_index].ccid.readerID)
								|| (GEMCORESIMPRO == usbDevice[reader_index].ccid.readerID))
							{
								usbDevice[reader_index].ccid.arrayOfSupportedDataRates = SerialCustomDataRates;
								usbDevice[reader_index].ccid.dwMaxDataRate = 125000;
							}

							*usbDevice[reader_index].nb_opened_slots += 1;
							usbDevice[reader_index].ccid.bCurrentSlotIndex++;
							usbDevice[reader_index].ccid.dwSlotStatus =
								IFD_ICC_PRESENT;
							DEBUG_INFO2("Opening slot: %d",
								usbDevice[reader_index].ccid.bCurrentSlotIndex);
							goto end;
						}
						else
						{
							/* if an interface number is given by HAL we
							 * continue with this device. */
							if (-1 == interface_number)
							{
								DEBUG_INFO3("USB device %s/%s already in use."
									" Checking next one.",
									bus->dirname, dev->filename);
								continue;
							}
						}
					}

					DEBUG_COMM3("Trying to open USB bus/device: %s/%s",
						 bus->dirname, dev->filename);

					dev_handle = usb_open(dev);
					if (dev_handle == NULL)
					{
						DEBUG_CRITICAL4("Can't usb_open(%s/%s): %s",
							bus->dirname, dev->filename, strerror(errno));

						continue;
					}

					/* now we found a free reader and we try to use it */
					if (dev->config == NULL)
					{
						(void)usb_close(dev_handle);
						DEBUG_CRITICAL3("No dev->config found for %s/%s",
							 bus->dirname, dev->filename);
						return STATUS_UNSUCCESSFUL;
					}

again:
					usb_interface = get_ccid_usb_interface(dev, &num);
					if (usb_interface == NULL)
					{
						(void)usb_close(dev_handle);
						if (0 == num)
							DEBUG_CRITICAL3("Can't find a CCID interface on %s/%s",
								bus->dirname, dev->filename);
						interface_number = -1;
						continue;
					}

					if (usb_interface->altsetting->extralen != 54)
					{
						(void)usb_close(dev_handle);
						DEBUG_CRITICAL4("Extra field for %s/%s has a wrong length: %d", bus->dirname, dev->filename, usb_interface->altsetting->extralen);
						return STATUS_UNSUCCESSFUL;
					}

					interface = usb_interface->altsetting->bInterfaceNumber;
					if (interface_number >= 0 && interface != interface_number)
					{
						/* an interface was specified and it is not the
						 * current one */
						DEBUG_INFO3("Wrong interface for USB device %s/%s."
							" Checking next one.", bus->dirname, dev->filename);

						/* check for another CCID interface on the same device */
						num++;

						goto again;
					}

					if (usb_claim_interface(dev_handle, interface) < 0)
					{
						(void)usb_close(dev_handle);
						DEBUG_CRITICAL4("Can't claim interface %s/%s: %s",
							bus->dirname, dev->filename, strerror(errno));
						interface_number = -1;
						continue;
					}

					DEBUG_INFO4("Found Vendor/Product: %04X/%04X (%s)",
						dev->descriptor.idVendor,
						dev->descriptor.idProduct, keyValue);
					DEBUG_INFO3("Using USB bus/device: %s/%s",
						 bus->dirname, dev->filename);

					/* check for firmware bugs */
					if (ccid_check_firmware(dev))
					{
						(void)usb_close(dev_handle);
						return STATUS_UNSUCCESSFUL;
					}

#ifdef USE_COMPOSITE_AS_MULTISLOT
					/* use the next interface for the next "slot" */
					static_interface++;

					/* reset for a next reader */
					if (static_interface > 2)
						static_interface = 1;
#endif

					/* Get Endpoints values*/
					(void)get_end_points(dev, &usbDevice[reader_index], num);

					/* store device information */
					usbDevice[reader_index].handle = dev_handle;
					usbDevice[reader_index].dirname = strdup(bus->dirname);
					usbDevice[reader_index].filename = strdup(dev->filename);
					usbDevice[reader_index].interface = interface;
					usbDevice[reader_index].real_nb_opened_slots = 1;
					usbDevice[reader_index].nb_opened_slots = &usbDevice[reader_index].real_nb_opened_slots;

					/* CCID common informations */
					usbDevice[reader_index].ccid.real_bSeq = 0;
					usbDevice[reader_index].ccid.pbSeq = &usbDevice[reader_index].ccid.real_bSeq;
					usbDevice[reader_index].ccid.readerID =
						(dev->descriptor.idVendor << 16) +
						dev->descriptor.idProduct;
					usbDevice[reader_index].ccid.dwFeatures = dw2i(usb_interface->altsetting->extra, 40);
					usbDevice[reader_index].ccid.wLcdLayout =
						(usb_interface->altsetting->extra[51] << 8) +
						usb_interface->altsetting->extra[50];
					usbDevice[reader_index].ccid.bPINSupport = usb_interface->altsetting->extra[52];
					usbDevice[reader_index].ccid.dwMaxCCIDMessageLength = dw2i(usb_interface->altsetting->extra, 44);
					usbDevice[reader_index].ccid.dwMaxIFSD = dw2i(usb_interface->altsetting->extra, 28);
					usbDevice[reader_index].ccid.dwDefaultClock = dw2i(usb_interface->altsetting->extra, 10);
					usbDevice[reader_index].ccid.dwMaxDataRate = dw2i(usb_interface->altsetting->extra, 23);
					usbDevice[reader_index].ccid.bMaxSlotIndex = usb_interface->altsetting->extra[4];
					usbDevice[reader_index].ccid.bCurrentSlotIndex = 0;
					usbDevice[reader_index].ccid.readTimeout = DEFAULT_COM_READ_TIMEOUT;
					usbDevice[reader_index].ccid.arrayOfSupportedDataRates = get_data_rates(reader_index, dev, num);
					usbDevice[reader_index].ccid.bInterfaceProtocol = usb_interface->altsetting->bInterfaceProtocol;
					usbDevice[reader_index].ccid.bNumEndpoints = usb_interface->altsetting->bNumEndpoints;
					usbDevice[reader_index].ccid.dwSlotStatus = IFD_ICC_PRESENT;
					usbDevice[reader_index].ccid.bVoltageSupport = usb_interface->altsetting->extra[5];
					goto end;
				}
			}
		}
	}
end:
	if (usbDevice[reader_index].handle == NULL)
		return STATUS_UNSUCCESSFUL;

	/* memorise the current reader_index so we can detect
	 * a new OpenUSBByName on a multi slot reader */
	previous_reader_index = reader_index;

	return STATUS_SUCCESS;
} /* OpenUSBByName */


/*****************************************************************************
 *
 *					WriteUSB
 *
 ****************************************************************************/
status_t WriteUSB(unsigned int reader_index, unsigned int length,
	unsigned char *buffer)
{
	int rv;
	char debug_header[] = "-> 121234 ";

	(void)snprintf(debug_header, sizeof(debug_header), "-> %06X ",
		(int)reader_index);

	DEBUG_XXD(debug_header, buffer, length);

	rv = usb_bulk_write(usbDevice[reader_index].handle,
		usbDevice[reader_index].bulk_out, (char *)buffer, length,
		USB_WRITE_TIMEOUT);

	if (rv < 0)
	{
		DEBUG_CRITICAL4("usb_bulk_write(%s/%s): %s",
			usbDevice[reader_index].dirname, usbDevice[reader_index].filename,
			strerror(errno));

		if (ENODEV == errno)
			return STATUS_NO_SUCH_DEVICE;

		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
} /* WriteUSB */


/*****************************************************************************
 *
 *					ReadUSB
 *
 ****************************************************************************/
status_t ReadUSB(unsigned int reader_index, unsigned int * length,
	unsigned char *buffer)
{
	int rv;
	char debug_header[] = "<- 121234 ";
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	int duplicate_frame = 0;

read_again:
	(void)snprintf(debug_header, sizeof(debug_header), "<- %06X ",
		(int)reader_index);

	rv = usb_bulk_read(usbDevice[reader_index].handle,
		usbDevice[reader_index].bulk_in, (char *)buffer, *length,
		usbDevice[reader_index].ccid.readTimeout * 1000);

	if (rv < 0)
	{
		*length = 0;
		DEBUG_CRITICAL4("usb_bulk_read(%s/%s): %s",
			usbDevice[reader_index].dirname, usbDevice[reader_index].filename,
			strerror(errno));

		if (ENODEV == errno)
			return STATUS_NO_SUCH_DEVICE;

		return STATUS_UNSUCCESSFUL;
	}

	*length = rv;

	DEBUG_XXD(debug_header, buffer, *length);

#define BSEQ_OFFSET 6
	if ((*length >= BSEQ_OFFSET)
		&& (buffer[BSEQ_OFFSET] < *ccid_descriptor->pbSeq -1))
	{
		duplicate_frame++;
		if (duplicate_frame > 10)
		{
			DEBUG_CRITICAL("Too many duplicate frame detected");
			return STATUS_UNSUCCESSFUL;
		}
		DEBUG_INFO("Duplicate frame detected");
		goto read_again;
	}

	return STATUS_SUCCESS;
} /* ReadUSB */


/*****************************************************************************
 *
 *					CloseUSB
 *
 ****************************************************************************/
status_t CloseUSB(unsigned int reader_index)
{
	/* device not opened */
	if (usbDevice[reader_index].handle == NULL)
		return STATUS_UNSUCCESSFUL;

	DEBUG_COMM3("Closing USB device: %s/%s",
		usbDevice[reader_index].dirname,
		usbDevice[reader_index].filename);

	if (usbDevice[reader_index].ccid.arrayOfSupportedDataRates
		&& (usbDevice[reader_index].ccid.bCurrentSlotIndex == 0))
	{
		free(usbDevice[reader_index].ccid.arrayOfSupportedDataRates);
		usbDevice[reader_index].ccid.arrayOfSupportedDataRates = NULL;
	}

	/* one slot closed */
	(*usbDevice[reader_index].nb_opened_slots)--;

	/* release the allocated ressources for the last slot only */
	if (0 == *usbDevice[reader_index].nb_opened_slots)
	{
		DEBUG_COMM("Last slot closed. Release resources");

		/* reset so that bSeq starts at 0 again */
		if (DriverOptions & DRIVER_OPTION_RESET_ON_CLOSE)
			(void)usb_reset(usbDevice[reader_index].handle);

		(void)usb_release_interface(usbDevice[reader_index].handle,
			usbDevice[reader_index].interface);
		(void)usb_close(usbDevice[reader_index].handle);

		free(usbDevice[reader_index].dirname);
		free(usbDevice[reader_index].filename);
	}

	/* mark the resource unused */
	usbDevice[reader_index].handle = NULL;
	usbDevice[reader_index].dirname = NULL;
	usbDevice[reader_index].filename = NULL;
	usbDevice[reader_index].interface = 0;

	return STATUS_SUCCESS;
} /* CloseUSB */


/*****************************************************************************
 *
 *					get_ccid_descriptor
 *
 ****************************************************************************/
_ccid_descriptor *get_ccid_descriptor(unsigned int reader_index)
{
	return &usbDevice[reader_index].ccid;
} /* get_ccid_descriptor */


/*****************************************************************************
 *
 *					get_end_points
 *
 ****************************************************************************/
static int get_end_points(struct usb_device *dev, _usbDevice *usbdevice,
	int num)
{
	int i;
	int bEndpointAddress;
	struct usb_interface *usb_interface = get_ccid_usb_interface(dev, &num);

	/*
	 * 3 Endpoints maximum: Interrupt In, Bulk In, Bulk Out
	 */
	for (i=0; i<usb_interface->altsetting->bNumEndpoints; i++)
	{
		/* interrupt end point (if available) */
		if (usb_interface->altsetting->endpoint[i].bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT)
		{
			usbdevice->interrupt = usb_interface->altsetting->endpoint[i].bEndpointAddress;
			continue;
		}

		if (usb_interface->altsetting->endpoint[i].bmAttributes != USB_ENDPOINT_TYPE_BULK)
			continue;

		bEndpointAddress = usb_interface->altsetting->endpoint[i].bEndpointAddress;

		if ((bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN)
			usbdevice->bulk_in = bEndpointAddress;

		if ((bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT)
			usbdevice->bulk_out = bEndpointAddress;
	}

	return 0;
} /* get_end_points */


/*****************************************************************************
 *
 *					get_ccid_usb_interface
 *
 ****************************************************************************/
/*@null@*/ EXTERNAL struct usb_interface * get_ccid_usb_interface(
	struct usb_device *dev, int *num)
{
	struct usb_interface *usb_interface = NULL;
	int i;
#ifdef O2MICRO_OZ776_PATCH
	int readerID;
#endif

	/* if multiple interfaces use the first one with CCID class type */
	for (i = *num; dev->config && i<dev->config->bNumInterfaces; i++)
	{
		/* CCID Class? */
		if (dev->config->interface[i].altsetting->bInterfaceClass == 0xb
#ifdef ALLOW_PROPRIETARY_CLASS
			|| dev->config->interface[i].altsetting->bInterfaceClass == 0xff
#endif
			)
		{
			usb_interface = &dev->config->interface[i];
			/* store the interface number for further reference */
			*num = i;
			break;
		}
	}

#ifdef O2MICRO_OZ776_PATCH
	readerID = (dev->descriptor.idVendor << 16) + dev->descriptor.idProduct;
	if (usb_interface != NULL
		&& ((OZ776 == readerID) || (OZ776_7772 == readerID)
		|| (REINER_SCT == readerID) || (BLUDRIVEII_CCID == readerID))
		&& (0 == usb_interface->altsetting->extralen)) /* this is the bug */
	{
		int j;
		for (j=0; j<usb_interface->altsetting->bNumEndpoints; j++)
		{
			/* find the extra[] array */
			if (54 == usb_interface->altsetting->endpoint[j].extralen)
			{
				/* get the extra[] from the endpoint */
				usb_interface->altsetting->extralen = 54;
				usb_interface->altsetting->extra =
					usb_interface->altsetting->endpoint[j].extra;
				/* avoid double free on close */
				usb_interface->altsetting->endpoint[j].extra = NULL;
				usb_interface->altsetting->endpoint[j].extralen = 0;
				break;
			}
		}
	}
#endif

	return usb_interface;
} /* get_ccid_usb_interface */


/*****************************************************************************
 *
 *					ccid_check_firmware
 *
 ****************************************************************************/
int ccid_check_firmware(struct usb_device *dev)
{
	unsigned int i;

	for (i=0; i<sizeof(Bogus_firmwares)/sizeof(Bogus_firmwares[0]); i++)
	{
		if (dev->descriptor.idVendor != Bogus_firmwares[i].vendor)
				continue;

		if (dev->descriptor.idProduct != Bogus_firmwares[i].product)
			continue;

		/* firmware too old and buggy */
		if (dev->descriptor.bcdDevice < Bogus_firmwares[i].firmware)
		{
			if (DriverOptions & DRIVER_OPTION_USE_BOGUS_FIRMWARE)
			{
				DEBUG_INFO3("Firmware (%X.%02X) is bogus! but you choosed to use it",
					dev->descriptor.bcdDevice >> 8,
					dev->descriptor.bcdDevice & 0xFF);
				return FALSE;
			}
			else
			{
				DEBUG_CRITICAL3("Firmware (%X.%02X) is bogus! Upgrade the reader firmware or get a new reader.",
					dev->descriptor.bcdDevice >> 8,
					dev->descriptor.bcdDevice & 0xFF);
				return TRUE;
			}
		}
	}

	/* by default the firmware is not bogus */
	return FALSE;
} /* ccid_check_firmware */


/*****************************************************************************
 *
 *                                      get_data_rates
 *
 ****************************************************************************/
static unsigned int *get_data_rates(unsigned int reader_index,
	struct usb_device *dev, int num)
{
	int n, i, len;
	unsigned char buffer[256*sizeof(int)];	/* maximum is 256 records */
	unsigned int *int_array;

	/* See CCID 3.7.3 page 25 */
	n = ControlUSB(reader_index,
		0xA1, /* request type */
		0x03, /* GET_DATA_RATES */
		0x00, /* value */
		buffer, sizeof(buffer));

	/* we got an error? */
	if (n <= 0)
	{
		DEBUG_INFO2("IFD does not support GET_DATA_RATES request: %s",
			strerror(errno));
		return NULL;
	}

	/* we got a strange value */
	if (n % 4)
	{
		DEBUG_INFO2("Wrong GET DATA RATES size: %d", n);
		return NULL;
	}

	/* allocate the buffer (including the end marker) */
	n /= sizeof(int);

	/* we do not get the expected number of data rates */
	len = get_ccid_usb_interface(dev, &num)->altsetting->extra[27]; /* bNumDataRatesSupported */
	if ((n != len) && len)
	{
		DEBUG_INFO3("Got %d data rates but was expecting %d", n, len);

		/* we got more data than expected */
		if (n > len)
			n = len;
	}

	int_array = calloc(n+1, sizeof(int));
	if (NULL == int_array)
	{
		DEBUG_CRITICAL("Memory allocation failed");
		return NULL;
	}

	/* convert in correct endianess */
	for (i=0; i<n; i++)
	{
		int_array[i] = dw2i(buffer, i*4);
		DEBUG_INFO2("declared: %d bps", int_array[i]);
	}

	/* end of array marker */
	int_array[i] = 0;

	return int_array;
} /* get_data_rates */


/*****************************************************************************
 *
 *                                      ControlUSB
 *
 ****************************************************************************/
int ControlUSB(int reader_index, int requesttype, int request, int value,
	unsigned char *bytes, unsigned int size)
{
	int ret;

	DEBUG_COMM2("request: 0x%02X", request);

	if (0 == (requesttype & 0x80))
		DEBUG_XXD("send: ", bytes, size);

	ret = usb_control_msg(usbDevice[reader_index].handle, requesttype,
		request, value, usbDevice[reader_index].interface, (char *)bytes, size,
		usbDevice[reader_index].ccid.readTimeout * 1000);

	if (requesttype & 0x80)
		DEBUG_XXD("receive: ", bytes, ret);

	return ret;
} /* ControlUSB */

/*****************************************************************************
 *
 *					InterruptRead
 *
 *
 ****************************************************************************/
int InterruptRead(int reader_index)
{
	int ret;
	char buffer[8];
	int timeout = 2*1000; /* 2 seconds */

	DEBUG_COMM("before");
	ret = usb_interrupt_read(usbDevice[reader_index].handle,
		usbDevice[reader_index].interrupt, buffer, sizeof(buffer), timeout);
	DEBUG_COMM2("after %d", ret);

	if (ret < 0)
	{
		/* if usb_interrupt_read() times out we get EILSEQ or EAGAIN */
		if ((errno != EILSEQ) && (errno != EAGAIN) && (errno != ENODEV) && (errno != 0))
			DEBUG_COMM4("usb_interrupt_read(%s/%s): %s",
					usbDevice[reader_index].dirname,
					usbDevice[reader_index].filename, strerror(errno));
	}
	else
		DEBUG_XXD("NotifySlotChange: ", buffer, ret);

	return ret;
} /* InterruptRead */

