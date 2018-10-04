/*
	ccid_usb.c: USB access routines using the libusb library
	Copyright (C) 2003-2010	Ludovic Rousseau

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

#define __CCID_USB__

#include <stdio.h>
#include <string.h>
# ifdef S_SPLINT_S
# include <sys/types.h>
# endif
#include <libusb.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <ifdhandler.h>

#include <config.h>
#include "misc.h"
#include "ccid.h"
#include "debug.h"
#include "defs.h"
#include "utils.h"
#include "parser.h"
#include "ccid_ifdhandler.h"


/* write timeout
 * we don't have to wait a long time since the card was doing nothing */
#define USB_WRITE_TIMEOUT (5 * 1000)	/* 5 seconds timeout */

/*
 * Proprietary USB Class (0xFF) are (or are not) accepted
 * A proprietary class is used for devices released before the final CCID
 * specifications were ready.
 * We should not have problems with non CCID devices because the
 * Manufacturer and Product ID are also used to identify the device */
#define ALLOW_PROPRIETARY_CLASS

#define BUS_DEVICE_STRSIZE 32

/* Using the default libusb context */
/* does not work for libusb <= 1.0.8 */
/* #define ctx NULL */
libusb_context *ctx = NULL;

#define CCID_INTERRUPT_SIZE 8

struct usbDevice_MultiSlot_Extension
{
	int reader_index;

	/* The multi-threaded polling part */
	int terminated;
	int status;
	unsigned char buffer[CCID_INTERRUPT_SIZE];
	pthread_t thread_proc;
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	struct libusb_transfer *transfer;
};

typedef struct
{
	libusb_device_handle *dev_handle;
	uint8_t bus_number;
	uint8_t device_address;
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

	/* libusb transfer for the polling (or NULL) */
	struct libusb_transfer *polling_transfer;

	/* pointer to the multislot extension (if any) */
	struct usbDevice_MultiSlot_Extension *multislot_extension;

} _usbDevice;

/* The _usbDevice structure must be defined before including ccid_usb.h */
#include "ccid_usb.h"

/* Specific hooks for multislot readers */
static int Multi_InterruptRead(int reader_index, int timeout /* in ms */);
static void Multi_InterruptStop(int reader_index);
static struct usbDevice_MultiSlot_Extension *Multi_CreateFirstSlot(int reader_index);
static struct usbDevice_MultiSlot_Extension *Multi_CreateNextSlot(int physical_reader_index);
static void Multi_PollingTerminate(struct usbDevice_MultiSlot_Extension *msExt);

static int get_end_points(struct libusb_config_descriptor *desc,
	_usbDevice *usbdevice, int num);
int ccid_check_firmware(struct libusb_device_descriptor *desc);
static unsigned int *get_data_rates(unsigned int reader_index,
	struct libusb_config_descriptor *desc, int num);

/* ne need to initialize to 0 since it is static */
static _usbDevice usbDevice[CCID_DRIVER_MAX_READERS];

#define PCSCLITE_MANUKEY_NAME "ifdVendorID"
#define PCSCLITE_PRODKEY_NAME "ifdProductID"
#define PCSCLITE_NAMEKEY_NAME "ifdFriendlyName"

struct _bogus_firmware
{
	int vendor;		/* idVendor */
	int product;	/* idProduct */
	int firmware;	/* bcdDevice: previous firmwares have bugs */
};

static struct _bogus_firmware Bogus_firmwares[] = {
	{ 0x04e6, 0xe001, 0x0516 },	/* SCR 331 */
	{ 0x04e6, 0x5111, 0x0620 },	/* SCR 331-DI */
	{ 0x04e6, 0xe003, 0x0510 },	/* SPR 532 */
	{ 0x0D46, 0x3001, 0x0037 },	/* KAAN Base */
	{ 0x0D46, 0x3002, 0x0037 },	/* KAAN Advanced */
	{ 0x09C3, 0x0008, 0x0203 },	/* ActivCard V2 */
	{ 0x0DC3, 0x1004, 0x0502 },	/* ASE IIIe USBv2 */
	{ 0x0DC3, 0x1102, 0x0607 },	/* ASE IIIe KB USB */
	{ 0x058F, 0x9520, 0x0102 },	/* Alcor AU9520-G */
	{ 0x072F, 0x2200, 0x0206 }, /* ACS ACR122U-WB-R */
	{ 0x08C3, 0x0402, 0x5000 },	/* Precise Biometrics Precise 200 MC */
	{ 0x08C3, 0x0401, 0x5000 },	/* Precise Biometrics Precise 250 MC */
	{ 0x0B0C, 0x0050, 0x0101 },	/* Todos Argos Mini II */
	{ 0x0DC3, 0x0900, 0x0200 }, /* Athena IDProtect Key v2 */
	{ 0x03F0, 0x0036, 0x0124 }, /* HP USB CCID Smartcard Keyboard */
	{ 0x062D, 0x0001, 0x0102 }, /* THRC Smart Card Reader */
	{ 0x04E6, 0x5291, 0x0112 }, /* SCM SCL010 Contactless Reader */

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
 *					close_libusb_if_needed
 *
 ****************************************************************************/
static void close_libusb_if_needed(void)
{
	int i, to_exit = TRUE;

	if (NULL == ctx)
		return;

	/* if at least 1 reader is still in use we do not exit libusb */
	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
	{
		if (usbDevice[i].dev_handle != NULL)
			to_exit = FALSE;
	}

	if (to_exit)
	{
		DEBUG_INFO1("libusb_exit");
		libusb_exit(ctx);
		ctx = NULL;
	}
} /* close_libusb_if_needed */

/*****************************************************************************
 *
 *					OpenUSB
 *
 ****************************************************************************/
status_t OpenUSB(unsigned int reader_index, /*@unused@*/ int Channel)
{
	(void)Channel;

	return OpenUSBByName(reader_index, NULL);
} /* OpenUSB */


/*****************************************************************************
 *
 *					OpenUSBByName
 *
 ****************************************************************************/
status_t OpenUSBByName(unsigned int reader_index, /*@null@*/ char *device)
{
	unsigned int alias;
	struct libusb_device_handle *dev_handle;
	char infofile[FILENAME_MAX];
#ifndef __APPLE__
	unsigned int device_vendor, device_product;
	unsigned int device_bus = 0;
	unsigned int device_addr = 0;
#else
	/* 100 ms delay */
	struct timespec sleep_time = { 0, 100 * 1000 * 1000 };
	int count_libusb = 10;
#endif
	int interface_number = -1;
	int i;
	static int previous_reader_index = -1;
	libusb_device **devs, *dev;
	ssize_t cnt;
	list_t plist, *values, *ifdVendorID, *ifdProductID, *ifdFriendlyName;
	int rv;
	int claim_failed = FALSE;
	int return_value = STATUS_SUCCESS;

	DEBUG_COMM3("Reader index: %X, Device: %s", reader_index, device);

#ifndef __APPLE__
	/* device name specified */
	if (device)
	{
		char *dirname;

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

		/* format usb:%04x/%04x:libudev:%d:%s
		 * with %d set to
		 * 01 (or whatever the interface number is)
		 * and %s set to
		 * /dev/bus/usb/008/004
		 */
		if ((dirname = strstr(device, "libudev:")) != NULL)
		{
			/* convert the interface number, bus and device ids */
			if (sscanf(dirname + 8, "%d:/dev/bus/usb/%d/%d", &interface_number, &device_bus, &device_addr) == 3) {
				DEBUG_COMM2("interface_number: %d", interface_number);
				DEBUG_COMM3("usb bus/device: %d/%d", device_bus, device_addr);
			}
		}
		else
		{
			/* format usb:%04x/%04x:libusb-1.0:%d:%d:%d */
			if ((dirname = strstr(device, "libusb-1.0:")) != NULL)
			{
				/* convert the interface number, bus and device ids */
				if (sscanf(dirname + 11, "%d:%d:%d",
					&device_bus, &device_addr, &interface_number) == 3)
				{
					DEBUG_COMM2("interface_number: %d", interface_number);
					DEBUG_COMM3("usb bus/device: %d/%d", device_bus,
						device_addr);
				}
			}
		}
	}
#endif

	/* is the reader_index already used? */
	if (usbDevice[reader_index].dev_handle != NULL)
	{
		DEBUG_CRITICAL2("USB driver with index %X already in use",
			reader_index);
		return STATUS_UNSUCCESSFUL;
	}

	/* Info.plist full patch filename */
	(void)snprintf(infofile, sizeof(infofile), "%s/%s/Contents/Info.plist",
		PCSCLITE_HP_DROPDIR, BUNDLE);
	DEBUG_INFO2("Using: %s", infofile);

	rv = bundleParse(infofile, &plist);
	if (rv)
		return STATUS_UNSUCCESSFUL;

#define GET_KEY(key, values) \
	rv = LTPBundleFindValueWithKey(&plist, key, &values); \
	if (rv) \
	{ \
		DEBUG_CRITICAL2("Value/Key not defined for " key " in %s", infofile); \
		return_value = STATUS_UNSUCCESSFUL; \
		goto end1; \
	} \
	else \
		DEBUG_INFO2(key ": %s", (char *)list_get_at(values, 0));

	/* general driver info */
	GET_KEY("ifdManufacturerString", values)
	GET_KEY("ifdProductString", values)
	GET_KEY("Copyright", values)

	if (NULL == ctx)
	{
		rv = libusb_init(&ctx);
		if (rv != 0)
		{
			DEBUG_CRITICAL2("libusb_init failed: %s", libusb_error_name(rv));
			return_value = STATUS_UNSUCCESSFUL;
			goto end1;
		}
	}

#define GET_KEYS(key, values) \
	rv = LTPBundleFindValueWithKey(&plist, key, values); \
	if (rv) \
	{ \
		DEBUG_CRITICAL2("Value/Key not defined for " key " in %s", infofile); \
		return_value = STATUS_UNSUCCESSFUL; \
		goto end1; \
	}

	GET_KEYS("ifdVendorID", &ifdVendorID)
	GET_KEYS("ifdProductID", &ifdProductID);
	GET_KEYS("ifdFriendlyName", &ifdFriendlyName)

	/* The 3 lists do not have the same size */
	if ((list_size(ifdVendorID) != list_size(ifdProductID))
		|| (list_size(ifdVendorID) != list_size(ifdFriendlyName)))
	{
		DEBUG_CRITICAL2("Error parsing %s", infofile);
		return_value = STATUS_UNSUCCESSFUL;
		goto end1;
	}

#ifdef __APPLE__
again_libusb:
#endif
	cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0)
	{
		DEBUG_CRITICAL("libusb_get_device_list() failed\n");
		return_value = STATUS_UNSUCCESSFUL;
		goto end1;
	}

	/* for any supported reader */
	for (alias=0; alias<list_size(ifdVendorID); alias++)
	{
		unsigned int vendorID, productID;
		char *friendlyName;

		vendorID = strtoul(list_get_at(ifdVendorID, alias), NULL, 0);
		productID = strtoul(list_get_at(ifdProductID, alias), NULL, 0);
		friendlyName = list_get_at(ifdFriendlyName, alias);

#ifndef __APPLE__
		/* the device was specified but is not the one we are trying to find */
		if (device
			&& (vendorID != device_vendor || productID != device_product))
			continue;
#else
		/* Leopard puts the friendlyname in the device argument */
		if (device && strcmp(device, friendlyName))
			continue;
#endif

		/* for every device */
		i = 0;
		while ((dev = devs[i++]) != NULL)
		{
			struct libusb_device_descriptor desc;
			struct libusb_config_descriptor *config_desc;
			uint8_t bus_number = libusb_get_bus_number(dev);
			uint8_t device_address = libusb_get_device_address(dev);

#ifndef __APPLE__
			if ((device_bus || device_addr)
				&& ((bus_number != device_bus)
				|| (device_address != device_addr))) {
				/* not USB the device we are looking for */
				continue;
			}
#endif
			DEBUG_COMM3("Try device: %d/%d", bus_number, device_address);

			int r = libusb_get_device_descriptor(dev, &desc);
			if (r < 0)
			{
				DEBUG_INFO3("failed to get device descriptor for %d/%d",
					bus_number, device_address);
				continue;
			}

			DEBUG_COMM3("vid/pid : %04X/%04X", desc.idVendor, desc.idProduct);

			if (desc.idVendor == vendorID && desc.idProduct == productID)
			{
				int already_used;
				const struct libusb_interface *usb_interface = NULL;
				int interface;
				int num = 0;
				const unsigned char *device_descriptor;
				int readerID = (vendorID << 16) + productID;

#ifdef USE_COMPOSITE_AS_MULTISLOT
				/* use the first CCID interface on first call */
				static int static_interface = -1;
				int max_interface_number = 2;

				/* simulate a composite device as when libudev is used */
				if ((GEMALTOPROXDU == readerID)
					|| (GEMALTOPROXSU == readerID)
					|| (HID_OMNIKEY_5422 == readerID)
					|| (FEITIANR502DUAL == readerID))
				{
						/*
						 * We can't talk to the two CCID interfaces
						 * at the same time (the reader enters a
						 * dead lock). So we simulate a multi slot
						 * reader. By default multi slot readers
						 * can't use the slots at the same time. See
						 * TAG_IFD_SLOT_THREAD_SAFE
						 *
						 * One side effect is that the two readers
						 * are seen by pcscd as one reader so the
						 * interface name is the same for the two.
						 *
	* So we have:
	* 0: Gemalto Prox-DU [Prox-DU Contact_09A00795] (09A00795) 00 00
	* 1: Gemalto Prox-DU [Prox-DU Contact_09A00795] (09A00795) 00 01
	* instead of
	* 0: Gemalto Prox-DU [Prox-DU Contact_09A00795] (09A00795) 00 00
	* 1: Gemalto Prox-DU [Prox-DU Contactless_09A00795] (09A00795) 01 00
						 */

					/* for the Gemalto Prox-DU/SU the interfaces are:
					 * 0: Prox-DU HID (not used)
					 * 1: Prox-DU Contactless (CCID)
					 * 2: Prox-DU Contact (CCID)
					 *
					 * For the Feitian R502 the interfaces are:
					 * 0: R502 Contactless Reader (CCID)
					 * 1: R502 Contact Reader (CCID)
					 * 2: R502 SAM1 Reader (CCID)
					 *
					 * For the HID Omnikey 5422 the interfaces are:
					 * 0: OMNIKEY 5422CL Smartcard Reader
					 * 1: OMNIKEY 5422 Smartcard Reader
					 */
					interface_number = static_interface;

					if (HID_OMNIKEY_5422 == readerID)
						/* only 2 interfaces for this device */
						max_interface_number = 1;
				}
#endif
				/* is it already opened? */
				already_used = FALSE;

				DEBUG_COMM3("Checking device: %d/%d",
					bus_number, device_address);
				for (r=0; r<CCID_DRIVER_MAX_READERS; r++)
				{
					if (usbDevice[r].dev_handle)
					{
						/* same bus, same address */
						if (usbDevice[r].bus_number == bus_number
							&& usbDevice[r].device_address == device_address)
							already_used = TRUE;
					}
				}

				/* this reader is already managed by us */
				if (already_used)
				{
					if ((previous_reader_index != -1)
						&& usbDevice[previous_reader_index].dev_handle
						&& (usbDevice[previous_reader_index].bus_number == bus_number)
						&& (usbDevice[previous_reader_index].device_address == device_address)
						&& usbDevice[previous_reader_index].ccid.bCurrentSlotIndex < usbDevice[previous_reader_index].ccid.bMaxSlotIndex)
					{
						/* we reuse the same device
						 * and the reader is multi-slot */
						usbDevice[reader_index] = usbDevice[previous_reader_index];
						/* The other slots of GemCore SIM Pro firmware
						 * 1.0 do not have the same data rates.
						 * Firmware 2.0 do not have this limitation */
						if ((GEMCOREPOSPRO == readerID)
							|| ((GEMCORESIMPRO == readerID)
							&& (usbDevice[reader_index].ccid.IFD_bcdDevice < 0x0200)))
						{
							/* Allocate a memory buffer that will be
							 * released in CloseUSB() */
							void *ptr = malloc(sizeof SerialCustomDataRates);
							if (ptr)
							{
								memcpy(ptr, SerialCustomDataRates,
									sizeof SerialCustomDataRates);
							}

							usbDevice[reader_index].ccid.arrayOfSupportedDataRates = ptr;
							usbDevice[reader_index].ccid.dwMaxDataRate = 125000;
						}

						*usbDevice[reader_index].nb_opened_slots += 1;
						usbDevice[reader_index].ccid.bCurrentSlotIndex++;
						usbDevice[reader_index].ccid.dwSlotStatus =
							IFD_ICC_PRESENT;
						DEBUG_INFO2("Opening slot: %d",
							usbDevice[reader_index].ccid.bCurrentSlotIndex);

						/* This is a multislot reader
						 * Init the multislot stuff for this next slot */
						usbDevice[reader_index].multislot_extension = Multi_CreateNextSlot(previous_reader_index);
						goto end;
					}
					else
					{
						/* if an interface number is given by HAL we
						 * continue with this device. */
						if (-1 == interface_number)
						{
							DEBUG_INFO3("USB device %d/%d already in use."
								" Checking next one.",
								bus_number, device_address);
							continue;
						}
					}
				}

				DEBUG_COMM3("Trying to open USB bus/device: %d/%d",
					bus_number, device_address);

				r = libusb_open(dev, &dev_handle);
				if (r < 0)
				{
					DEBUG_CRITICAL4("Can't libusb_open(%d/%d): %s",
						bus_number, device_address, libusb_error_name(r));

					continue;
				}

again:
				r = libusb_get_active_config_descriptor(dev, &config_desc);
				if (r < 0)
				{
#ifdef __APPLE__
					/* Some early Gemalto Ezio CB+ readers have
					 * bDeviceClass, bDeviceSubClass and bDeviceProtocol set
					 * to 0xFF (proprietary) instead of 0x00.
					 *
					 * So on Mac OS X the reader configuration is not done
					 * by the OS/kernel and we do it ourself.
					 */
					if ((0xFF == desc.bDeviceClass)
						&& (0xFF == desc.bDeviceSubClass)
						&& (0xFF == desc.bDeviceProtocol))
					{
						r = libusb_set_configuration(dev_handle, 1);
						if (r < 0)
						{
							(void)libusb_close(dev_handle);
							DEBUG_CRITICAL4("Can't set configuration on %d/%d: %s",
									bus_number, device_address,
									libusb_error_name(r));
							continue;
						}
					}

					/* recall */
					r = libusb_get_active_config_descriptor(dev, &config_desc);
					if (r < 0)
					{
#endif
						(void)libusb_close(dev_handle);
						DEBUG_CRITICAL4("Can't get config descriptor on %d/%d: %s",
							bus_number, device_address, libusb_error_name(r));
						continue;
					}
#ifdef __APPLE__
				}
#endif


				usb_interface = get_ccid_usb_interface(config_desc, &num);
				if (usb_interface == NULL)
				{
					libusb_free_config_descriptor(config_desc);
					(void)libusb_close(dev_handle);
					if (0 == num)
						DEBUG_CRITICAL3("Can't find a CCID interface on %d/%d",
							bus_number, device_address);
					interface_number = -1;
					continue;
				}

				device_descriptor = get_ccid_device_descriptor(usb_interface);
				if (NULL == device_descriptor)
				{
					libusb_free_config_descriptor(config_desc);
					(void)libusb_close(dev_handle);
					DEBUG_CRITICAL3("Unable to find the device descriptor for %d/%d",
						bus_number, device_address);
					return_value = STATUS_UNSUCCESSFUL;
					goto end2;
				}

				interface = usb_interface->altsetting->bInterfaceNumber;
				if (interface_number >= 0 && interface != interface_number)
				{
					libusb_free_config_descriptor(config_desc);
					/* an interface was specified and it is not the
					 * current one */
					DEBUG_INFO3("Found interface %d but expecting %d",
						interface, interface_number);
					DEBUG_INFO3("Wrong interface for USB device %d/%d."
						" Checking next one.", bus_number, device_address);

					/* check for another CCID interface on the same device */
					num++;

					goto again;
				}

				r = libusb_claim_interface(dev_handle, interface);
				if (r < 0)
				{
					libusb_free_config_descriptor(config_desc);
					(void)libusb_close(dev_handle);
					DEBUG_CRITICAL4("Can't claim interface %d/%d: %s",
						bus_number, device_address, libusb_error_name(r));
					claim_failed = TRUE;
					interface_number = -1;
					continue;
				}

				DEBUG_INFO4("Found Vendor/Product: %04X/%04X (%s)",
					desc.idVendor, desc.idProduct, friendlyName);
				DEBUG_INFO3("Using USB bus/device: %d/%d",
					bus_number, device_address);

				/* check for firmware bugs */
				if (ccid_check_firmware(&desc))
				{
					libusb_free_config_descriptor(config_desc);
					(void)libusb_close(dev_handle);
					return_value = STATUS_UNSUCCESSFUL;
					goto end2;
				}

#ifdef USE_COMPOSITE_AS_MULTISLOT
				if ((GEMALTOPROXDU == readerID)
					|| (GEMALTOPROXSU == readerID)
					|| (HID_OMNIKEY_5422 == readerID)
					|| (FEITIANR502DUAL == readerID))
				{
					/* use the next interface for the next "slot" */
					static_interface = interface + 1;

					/* reset for a next reader */
					/* max interface number for all 3 readers is 2 */
					if (static_interface > max_interface_number)
						static_interface = -1;
				}
#endif

				/* Get Endpoints values*/
				(void)get_end_points(config_desc, &usbDevice[reader_index], num);

				/* store device information */
				usbDevice[reader_index].dev_handle = dev_handle;
				usbDevice[reader_index].bus_number = bus_number;
				usbDevice[reader_index].device_address = device_address;
				usbDevice[reader_index].interface = interface;
				usbDevice[reader_index].real_nb_opened_slots = 1;
				usbDevice[reader_index].nb_opened_slots = &usbDevice[reader_index].real_nb_opened_slots;
				usbDevice[reader_index].polling_transfer = NULL;

				/* CCID common informations */
				usbDevice[reader_index].ccid.real_bSeq = 0;
				usbDevice[reader_index].ccid.pbSeq = &usbDevice[reader_index].ccid.real_bSeq;
				usbDevice[reader_index].ccid.readerID =
					(desc.idVendor << 16) + desc.idProduct;
				usbDevice[reader_index].ccid.dwFeatures = dw2i(device_descriptor, 40);
				usbDevice[reader_index].ccid.wLcdLayout =
					(device_descriptor[51] << 8) + device_descriptor[50];
				usbDevice[reader_index].ccid.bPINSupport = device_descriptor[52];
				usbDevice[reader_index].ccid.dwMaxCCIDMessageLength = dw2i(device_descriptor, 44);
				usbDevice[reader_index].ccid.dwMaxIFSD = dw2i(device_descriptor, 28);
				usbDevice[reader_index].ccid.dwDefaultClock = dw2i(device_descriptor, 10);
				usbDevice[reader_index].ccid.dwMaxDataRate = dw2i(device_descriptor, 23);
				usbDevice[reader_index].ccid.bMaxSlotIndex = device_descriptor[4];
				usbDevice[reader_index].ccid.bCurrentSlotIndex = 0;
				usbDevice[reader_index].ccid.readTimeout = DEFAULT_COM_READ_TIMEOUT;
				if (device_descriptor[27])
					usbDevice[reader_index].ccid.arrayOfSupportedDataRates = get_data_rates(reader_index, config_desc, num);
				else
				{
					usbDevice[reader_index].ccid.arrayOfSupportedDataRates = NULL;
					DEBUG_INFO1("bNumDataRatesSupported is 0");
				}
				usbDevice[reader_index].ccid.bInterfaceProtocol = usb_interface->altsetting->bInterfaceProtocol;
				usbDevice[reader_index].ccid.bNumEndpoints = usb_interface->altsetting->bNumEndpoints;
				usbDevice[reader_index].ccid.dwSlotStatus = IFD_ICC_PRESENT;
				usbDevice[reader_index].ccid.bVoltageSupport = device_descriptor[5];
				usbDevice[reader_index].ccid.sIFD_serial_number = NULL;
				usbDevice[reader_index].ccid.gemalto_firmware_features = NULL;
#ifdef ENABLE_ZLP
				usbDevice[reader_index].ccid.zlp = FALSE;
#endif
				if (desc.iSerialNumber)
				{
					unsigned char serial[128];
					int ret;

					ret = libusb_get_string_descriptor_ascii(dev_handle,
							desc.iSerialNumber, serial,
							sizeof(serial));
					if (ret > 0)
						usbDevice[reader_index].ccid.sIFD_serial_number
							= strdup((char *)serial);
				}

				usbDevice[reader_index].ccid.sIFD_iManufacturer = NULL;
				if (desc.iManufacturer)
				{
					unsigned char iManufacturer[128];
					int ret;

					ret = libusb_get_string_descriptor_ascii(dev_handle,
							desc.iManufacturer, iManufacturer,
							sizeof(iManufacturer));
					if (ret > 0)
						usbDevice[reader_index].ccid.sIFD_iManufacturer
							= strdup((char *)iManufacturer);
				}

				usbDevice[reader_index].ccid.IFD_bcdDevice = desc.bcdDevice;

				/* If this is a multislot reader, init the multislot stuff */
				if (usbDevice[reader_index].ccid.bMaxSlotIndex)
					usbDevice[reader_index].multislot_extension = Multi_CreateFirstSlot(reader_index);
				else
					usbDevice[reader_index].multislot_extension = NULL;

				libusb_free_config_descriptor(config_desc);
				goto end;
			}
		}
	}
end:
	if (usbDevice[reader_index].dev_handle == NULL)
	{
		/* free the libusb allocated list & devices */
		libusb_free_device_list(devs, 1);

#ifdef __APPLE__
		/* give some time to libusb to detect the new USB devices on Mac OS X */
		if (count_libusb > 0)
		{
			count_libusb--;
			DEBUG_INFO2("Wait after libusb: %d", count_libusb);
			nanosleep(&sleep_time, NULL);

			goto again_libusb;
		}
#endif

		/* free bundle list */
		bundleRelease(&plist);

		/* failed */
		close_libusb_if_needed();

		if (claim_failed)
			return STATUS_COMM_ERROR;
		DEBUG_INFO1("Device not found?");
		return STATUS_NO_SUCH_DEVICE;
	}

	/* memorise the current reader_index so we can detect
	 * a new OpenUSBByName on a multi slot reader */
	previous_reader_index = reader_index;

end2:
	/* free the libusb allocated list & devices */
	libusb_free_device_list(devs, 1);

end1:
	/* free bundle list */
	bundleRelease(&plist);

	if (return_value != STATUS_SUCCESS)
		close_libusb_if_needed();

	return return_value;
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
	int actual_length;
	char debug_header[] = "-> 121234 ";

	(void)snprintf(debug_header, sizeof(debug_header), "-> %06X ",
		(int)reader_index);

#ifdef ENABLE_ZLP
	if (usbDevice[reader_index].ccid.zlp)
	{ /* Zero Length Packet */
		int dummy_length;

		/* try to read a ZLP so transfer length = 0
		 * timeout of 10 ms */
		(void)libusb_bulk_transfer(usbDevice[reader_index].dev_handle,
			usbDevice[reader_index].bulk_in, NULL, 0, &dummy_length, 10);
	}
#endif

	DEBUG_XXD(debug_header, buffer, length);

	rv = libusb_bulk_transfer(usbDevice[reader_index].dev_handle,
		usbDevice[reader_index].bulk_out, buffer, length,
		&actual_length, USB_WRITE_TIMEOUT);

	if (rv < 0)
	{
		DEBUG_CRITICAL5("write failed (%d/%d): %d %s",
			usbDevice[reader_index].bus_number,
			usbDevice[reader_index].device_address, rv, libusb_error_name(rv));

		if (LIBUSB_ERROR_NO_DEVICE == rv)
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
	int actual_length;
	char debug_header[] = "<- 121234 ";
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	int duplicate_frame = 0;

read_again:
	(void)snprintf(debug_header, sizeof(debug_header), "<- %06X ",
		(int)reader_index);

	rv = libusb_bulk_transfer(usbDevice[reader_index].dev_handle,
		usbDevice[reader_index].bulk_in, buffer, *length,
		&actual_length, usbDevice[reader_index].ccid.readTimeout);

	if (rv < 0)
	{
		*length = 0;
		DEBUG_CRITICAL5("read failed (%d/%d): %d %s",
			usbDevice[reader_index].bus_number,
			usbDevice[reader_index].device_address, rv, libusb_error_name(rv));

		if (LIBUSB_ERROR_NO_DEVICE == rv)
			return STATUS_NO_SUCH_DEVICE;

		return STATUS_UNSUCCESSFUL;
	}

	*length = actual_length;

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
		DEBUG_INFO1("Duplicate frame detected");
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
	if (usbDevice[reader_index].dev_handle == NULL)
		return STATUS_UNSUCCESSFUL;

	DEBUG_COMM3("Closing USB device: %d/%d",
		usbDevice[reader_index].bus_number,
		usbDevice[reader_index].device_address);

	/* one slot closed */
	(*usbDevice[reader_index].nb_opened_slots)--;

	/* release the allocated ressources for the last slot only */
	if (0 == *usbDevice[reader_index].nb_opened_slots)
	{
		struct usbDevice_MultiSlot_Extension *msExt;

		DEBUG_COMM("Last slot closed. Release resources");

		msExt = usbDevice[reader_index].multislot_extension;
		/* If this is a multislot reader, close using the multislot stuff */
		if (msExt)
		{
			/* terminate the interrupt waiter thread */
			Multi_PollingTerminate(msExt);

			/* wait for the thread to actually terminate */
			pthread_join(msExt->thread_proc, NULL);

			/* release the shared objects */
			pthread_cond_destroy(&msExt->condition);
			pthread_mutex_destroy(&msExt->mutex);

			/* Deallocate the extension itself */
			free(msExt);

			/* Stop the slot */
			usbDevice[reader_index].multislot_extension = NULL;
		}

		if (usbDevice[reader_index].ccid.gemalto_firmware_features)
			free(usbDevice[reader_index].ccid.gemalto_firmware_features);

		if (usbDevice[reader_index].ccid.sIFD_serial_number)
			free(usbDevice[reader_index].ccid.sIFD_serial_number);

		if (usbDevice[reader_index].ccid.sIFD_iManufacturer)
			free(usbDevice[reader_index].ccid.sIFD_iManufacturer);

		if (usbDevice[reader_index].ccid.arrayOfSupportedDataRates)
			free(usbDevice[reader_index].ccid.arrayOfSupportedDataRates);

		(void)libusb_release_interface(usbDevice[reader_index].dev_handle,
			usbDevice[reader_index].interface);
		(void)libusb_close(usbDevice[reader_index].dev_handle);
	}

	/* mark the resource unused */
	usbDevice[reader_index].dev_handle = NULL;
	usbDevice[reader_index].interface = 0;

	close_libusb_if_needed();

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
 *					get_ccid_device_descriptor
 *
 ****************************************************************************/
const unsigned char *get_ccid_device_descriptor(const struct libusb_interface *usb_interface)
{
#ifdef O2MICRO_OZ776_PATCH
	uint8_t last_endpoint;
#endif

	if (54 == usb_interface->altsetting->extra_length)
		return usb_interface->altsetting->extra;

	if (0 != usb_interface->altsetting->extra_length)
	{
		/* If extra_length is zero, the descriptor might be at
		 * the end, but if it's not zero, we have a
		 * problem. */
		DEBUG_CRITICAL2("Extra field has a wrong length: %d",
			usb_interface->altsetting->extra_length);
		return NULL;
	}

#ifdef O2MICRO_OZ776_PATCH
	/* Some devices, such as the Oz776, Reiner SCT and bludrive II
	 * report the device descriptor at the end of the endpoint
	 * descriptors; to support those, look for it at the end as well.
	 */
	last_endpoint = usb_interface->altsetting->bNumEndpoints-1;
	if (usb_interface->altsetting->endpoint
		&& usb_interface->altsetting->endpoint[last_endpoint].extra_length == 54)
		return usb_interface->altsetting->endpoint[last_endpoint].extra;
#else
	DEBUG_CRITICAL2("Extra field has a wrong length: %d",
		usb_interface->altsetting->extra_length);
#endif

	return NULL;
} /* get_ccid_device_descriptor */


/*****************************************************************************
 *
 *					get_end_points
 *
 ****************************************************************************/
static int get_end_points(struct libusb_config_descriptor *desc,
	_usbDevice *usbdevice, int num)
{
	int i;
	int bEndpointAddress;
	const struct libusb_interface *usb_interface;

	usb_interface = get_ccid_usb_interface(desc, &num);

	/*
	 * 3 Endpoints maximum: Interrupt In, Bulk In, Bulk Out
	 */
	for (i=0; i<usb_interface->altsetting->bNumEndpoints; i++)
	{
		/* interrupt end point (if available) */
		if (usb_interface->altsetting->endpoint[i].bmAttributes
			== LIBUSB_TRANSFER_TYPE_INTERRUPT)
		{
			usbdevice->interrupt =
				usb_interface->altsetting->endpoint[i].bEndpointAddress;
			continue;
		}

		if (usb_interface->altsetting->endpoint[i].bmAttributes
			!= LIBUSB_TRANSFER_TYPE_BULK)
			continue;

		bEndpointAddress =
			usb_interface->altsetting->endpoint[i].bEndpointAddress;

		if ((bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
			== LIBUSB_ENDPOINT_IN)
			usbdevice->bulk_in = bEndpointAddress;

		if ((bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
			== LIBUSB_ENDPOINT_OUT)
			usbdevice->bulk_out = bEndpointAddress;
	}

	return 0;
} /* get_end_points */


/*****************************************************************************
 *
 *					get_ccid_usb_interface
 *
 ****************************************************************************/
/*@null@*/ EXTERNAL const struct libusb_interface * get_ccid_usb_interface(
	struct libusb_config_descriptor *desc, int *num)
{
	const struct libusb_interface *usb_interface = NULL;
	int i;

	/* if multiple interfaces use the first one with CCID class type */
	for (i = *num; i < desc->bNumInterfaces; i++)
	{
		/* CCID Class? */
		if (desc->interface[i].altsetting->bInterfaceClass == 0xb
#ifdef ALLOW_PROPRIETARY_CLASS
			|| (desc->interface[i].altsetting->bInterfaceClass == 0xff
			&& 54 == desc->interface[i].altsetting->extra_length)
#endif
			)
		{
			usb_interface = &desc->interface[i];
			/* store the interface number for further reference */
			*num = i;
			break;
		}
	}

	return usb_interface;
} /* get_ccid_usb_interface */


/*****************************************************************************
 *
 *					ccid_check_firmware
 *
 ****************************************************************************/
int ccid_check_firmware(struct libusb_device_descriptor *desc)
{
	unsigned int i;

	for (i=0; i<COUNT_OF(Bogus_firmwares); i++)
	{
		if (desc->idVendor != Bogus_firmwares[i].vendor)
			continue;

		if (desc->idProduct != Bogus_firmwares[i].product)
			continue;

		/* firmware too old and buggy */
		if (desc->bcdDevice < Bogus_firmwares[i].firmware)
		{
			if (DriverOptions & DRIVER_OPTION_USE_BOGUS_FIRMWARE)
			{
				DEBUG_INFO3("Firmware (%X.%02X) is bogus! but you choosed to use it",
					desc->bcdDevice >> 8, desc->bcdDevice & 0xFF);
				return FALSE;
			}
			else
			{
				DEBUG_CRITICAL3("Firmware (%X.%02X) is bogus! Upgrade the reader firmware or get a new reader.",
					desc->bcdDevice >> 8, desc->bcdDevice & 0xFF);
				return TRUE;
			}
		}
	}

	/* by default the firmware is not bogus */
	return FALSE;
} /* ccid_check_firmware */


/*****************************************************************************
 *
 *					get_data_rates
 *
 ****************************************************************************/
static unsigned int *get_data_rates(unsigned int reader_index,
	struct libusb_config_descriptor *desc, int num)
{
	int n, i, len;
	unsigned char buffer[256*sizeof(int)];	/* maximum is 256 records */
	unsigned int *uint_array;

	/* See CCID 3.7.3 page 25 */
	n = ControlUSB(reader_index,
		0xA1, /* request type */
		0x03, /* GET_DATA_RATES */
		0x00, /* value */
		buffer, sizeof(buffer));

	/* we got an error? */
	if (n <= 0)
	{
		DEBUG_INFO2("IFD does not support GET_DATA_RATES request: %d", n);
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
	len = get_ccid_device_descriptor(get_ccid_usb_interface(desc, &num))[27]; /* bNumDataRatesSupported */
	if ((n != len) && len)
	{
		DEBUG_INFO3("Got %d data rates but was expecting %d", n, len);

		/* we got more data than expected */
		if (n > len)
			n = len;
	}

	uint_array = calloc(n+1, sizeof(uint_array[0]));
	if (NULL == uint_array)
	{
		DEBUG_CRITICAL("Memory allocation failed");
		return NULL;
	}

	/* convert in correct endianess */
	for (i=0; i<n; i++)
	{
		uint_array[i] = dw2i(buffer, i*4);
		DEBUG_INFO2("declared: %d bps", uint_array[i]);
	}

	/* end of array marker */
	uint_array[i] = 0;

	return uint_array;
} /* get_data_rates */


/*****************************************************************************
 *
 *					ControlUSB
 *
 ****************************************************************************/
int ControlUSB(int reader_index, int requesttype, int request, int value,
	unsigned char *bytes, unsigned int size)
{
	int ret;

	DEBUG_COMM2("request: 0x%02X", request);

	if (0 == (requesttype & 0x80))
		DEBUG_XXD("send: ", bytes, size);

	ret = libusb_control_transfer(usbDevice[reader_index].dev_handle,
		requesttype, request, value, usbDevice[reader_index].interface,
		bytes, size, usbDevice[reader_index].ccid.readTimeout);

	if (ret < 0)
	{
		DEBUG_CRITICAL5("control failed (%d/%d): %d %s",
			usbDevice[reader_index].bus_number,
			usbDevice[reader_index].device_address, ret, libusb_error_name(ret));

		return ret;
	}

	if (requesttype & 0x80)
		DEBUG_XXD("receive: ", bytes, ret);

	return ret;
} /* ControlUSB */

/*****************************************************************************
 *
 *					Transfer is complete
 *
 ****************************************************************************/
static void bulk_transfer_cb(struct libusb_transfer *transfer)
{
	int *completed = transfer->user_data;
	*completed = 1;
	/* caller interprets results and frees transfer */
}

/*****************************************************************************
 *
 *					InterruptRead
 *
 ****************************************************************************/
int InterruptRead(int reader_index, int timeout /* in ms */)
{
	int ret, actual_length;
	int return_value = IFD_SUCCESS;
	unsigned char buffer[8];
	struct libusb_transfer *transfer;
	int completed = 0;

	/* Multislot reader: redirect to Multi_InterrupRead */
	if (usbDevice[reader_index].multislot_extension != NULL)
		return Multi_InterruptRead(reader_index, timeout);

	DEBUG_PERIODIC3("before (%d), timeout: %d ms", reader_index, timeout);

	transfer = libusb_alloc_transfer(0);
	if (NULL == transfer)
		return LIBUSB_ERROR_NO_MEM;

	libusb_fill_bulk_transfer(transfer,
		usbDevice[reader_index].dev_handle,
		usbDevice[reader_index].interrupt, buffer, sizeof(buffer),
		bulk_transfer_cb, &completed, timeout);
	transfer->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;

	ret = libusb_submit_transfer(transfer);
	if (ret < 0) {
		libusb_free_transfer(transfer);
		DEBUG_CRITICAL2("libusb_submit_transfer failed: %s",
			libusb_error_name(ret));
		return IFD_COMMUNICATION_ERROR;
	}

	usbDevice[reader_index].polling_transfer = transfer;

	while (!completed)
	{
		ret = libusb_handle_events_completed(ctx, &completed);
		if (ret < 0)
		{
			if (ret == LIBUSB_ERROR_INTERRUPTED)
				continue;
			libusb_cancel_transfer(transfer);
			while (!completed)
				if (libusb_handle_events_completed(ctx, &completed) < 0)
					break;
			libusb_free_transfer(transfer);
			DEBUG_CRITICAL2("libusb_handle_events failed: %s",
				libusb_error_name(ret));
			return IFD_COMMUNICATION_ERROR;
		}
	}

	actual_length = transfer->actual_length;
	ret = transfer->status;

	usbDevice[reader_index].polling_transfer = NULL;
	libusb_free_transfer(transfer);

	DEBUG_PERIODIC3("after (%d) (%d)", reader_index, ret);

	switch (ret)
	{
		case LIBUSB_TRANSFER_COMPLETED:
			DEBUG_XXD("NotifySlotChange: ", buffer, actual_length);
			break;

		case LIBUSB_TRANSFER_TIMED_OUT:
			break;

		default:
			/* if libusb_interrupt_transfer() times out we get EILSEQ or EAGAIN */
			DEBUG_COMM4("InterruptRead (%d/%d): %d",
				usbDevice[reader_index].bus_number,
				usbDevice[reader_index].device_address, ret);
			return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* InterruptRead */


/*****************************************************************************
 *
 *					Stop the async loop
 *
 ****************************************************************************/
void InterruptStop(int reader_index)
{
	struct libusb_transfer *transfer;

	/* Multislot reader: redirect to Multi_InterrupStop */
	if (usbDevice[reader_index].multislot_extension != NULL)
	{
		Multi_InterruptStop(reader_index);
		return;
	}

	transfer = usbDevice[reader_index].polling_transfer;
	usbDevice[reader_index].polling_transfer = NULL;
	if (transfer)
	{
		int ret;

		ret = libusb_cancel_transfer(transfer);
		if (ret < 0)
			DEBUG_CRITICAL2("libusb_cancel_transfer failed: %s",
				libusb_error_name(ret));
	}
} /* InterruptStop */


/*****************************************************************************
 *
 *					Multi_PollingProc
 *
 ****************************************************************************/
static void *Multi_PollingProc(void *p_ext)
{
	struct usbDevice_MultiSlot_Extension *msExt = p_ext;
	int rv, status, actual_length;
	unsigned char buffer[CCID_INTERRUPT_SIZE];
	struct libusb_transfer *transfer;
	int completed;

	DEBUG_COMM3("Multi_PollingProc (%d/%d): thread starting",
		usbDevice[msExt->reader_index].bus_number,
		usbDevice[msExt->reader_index].device_address);

	rv = 0;
	while (!msExt->terminated)
	{
		DEBUG_COMM3("Multi_PollingProc (%d/%d): waiting",
			usbDevice[msExt->reader_index].bus_number,
			usbDevice[msExt->reader_index].device_address);

		transfer = libusb_alloc_transfer(0);
		if (NULL == transfer)
		{
			rv = LIBUSB_ERROR_NO_MEM;
			DEBUG_COMM2("libusb_alloc_transfer err %d", rv);
			break;
		}

		libusb_fill_bulk_transfer(transfer,
			usbDevice[msExt->reader_index].dev_handle,
			usbDevice[msExt->reader_index].interrupt,
			buffer, CCID_INTERRUPT_SIZE,
			bulk_transfer_cb, &completed, 0); /* No timeout ! */

		transfer->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;

		rv = libusb_submit_transfer(transfer);
		if (rv)
		{
			libusb_free_transfer(transfer);
			DEBUG_COMM2("libusb_submit_transfer err %d", rv);
			break;
		}

		usbDevice[msExt->reader_index].polling_transfer = transfer;

		completed = 0;
		while (!completed && !msExt->terminated)
		{
			rv = libusb_handle_events_completed(ctx, &completed);
			if (rv < 0)
			{
				DEBUG_COMM2("libusb_handle_events err %d", rv);

				if (rv == LIBUSB_ERROR_INTERRUPTED)
					continue;

				libusb_cancel_transfer(transfer);

				while (!completed && !msExt->terminated)
				{
					if (libusb_handle_events_completed(ctx, &completed) < 0)
						break;
				}

				break;
			}
		}

		usbDevice[msExt->reader_index].polling_transfer = NULL;

		if (rv < 0)
			libusb_free_transfer(transfer);
		else
		{
			int b, slot;

			actual_length = transfer->actual_length;
			status = transfer->status;

			libusb_free_transfer(transfer);

			switch (status)
			{
				case LIBUSB_TRANSFER_COMPLETED:
					DEBUG_COMM3("Multi_PollingProc (%d/%d): OK",
						usbDevice[msExt->reader_index].bus_number,
						usbDevice[msExt->reader_index].device_address);
					DEBUG_XXD("NotifySlotChange: ", buffer, actual_length);

					/* log the RDR_to_PC_NotifySlotChange data */
					slot = 0;
					for (b=0; b<actual_length-1; b++)
					{
						int s;

						/* 4 slots per byte */
						for (s=0; s<4; s++)
						{
							/* 2 bits per slot */
							int slot_status = ((buffer[1+b] >> (s*2)) & 3);
							const char *present, *change;

							present = (slot_status & 1) ? "present" : "absent";
							change = (slot_status & 2) ? "status changed" : "no change";

							DEBUG_COMM3("slot %d status: %d",
								s + b*4, slot_status);
							DEBUG_COMM3("ICC %s, %s", present, change);
						}
						slot += 4;
					}
					break;

				case LIBUSB_TRANSFER_TIMED_OUT:
					DEBUG_COMM3("Multi_PollingProc (%d/%d): Timeout",
						usbDevice[msExt->reader_index].bus_number,
						usbDevice[msExt->reader_index].device_address);
					break;

				default:
					/* if libusb_interrupt_transfer() times out
					 * we get EILSEQ or EAGAIN */
					DEBUG_COMM4("Multi_PollingProc (%d/%d): %d",
						usbDevice[msExt->reader_index].bus_number,
						usbDevice[msExt->reader_index].device_address,
						status);
			}

			/* Tell other slots that there's a new interrupt buffer */
			DEBUG_COMM3("Multi_PollingProc (%d/%d): Broadcast to slot(s)",
				usbDevice[msExt->reader_index].bus_number,
				usbDevice[msExt->reader_index].device_address);

			/* Lock the mutex */
			pthread_mutex_lock(&msExt->mutex);

			/* Set the status and the interrupt buffer */
			msExt->status = status;
			memset(msExt->buffer, 0, sizeof msExt->buffer);
			memcpy(msExt->buffer, buffer, actual_length);

			/* Broadcast the condition and unlock */
			pthread_cond_broadcast(&msExt->condition);
			pthread_mutex_unlock(&msExt->mutex);
		}
	}

	msExt->terminated = TRUE;

	if (rv < 0)
	{
		DEBUG_CRITICAL4("Multi_PollingProc (%d/%d): error %d",
			usbDevice[msExt->reader_index].bus_number,
			usbDevice[msExt->reader_index].device_address, rv);
	}

	/* Wake up the slot threads so they will exit as well */

	/* Lock the mutex */
	pthread_mutex_lock(&msExt->mutex);

	/* Set the status and fill-in the interrupt buffer */
	msExt->status = 0;
	memset(msExt->buffer, 0xFF, sizeof msExt->buffer);

	/* Broadcast the condition */
	pthread_cond_broadcast(&msExt->condition);

	/* Unlock */
	pthread_mutex_unlock(&msExt->mutex);

	/* Now exit */
	DEBUG_COMM3("Multi_PollingProc (%d/%d): Thread terminated",
		usbDevice[msExt->reader_index].bus_number,
		usbDevice[msExt->reader_index].device_address);

	pthread_exit(NULL);
	return NULL;
} /* Multi_PollingProc */


/*****************************************************************************
 *
 *					Multi_PollingTerminate
 *
 ****************************************************************************/
static void Multi_PollingTerminate(struct usbDevice_MultiSlot_Extension *msExt)
{
	struct libusb_transfer *transfer;

	if (msExt && !msExt->terminated)
	{
		msExt->terminated = TRUE;

		transfer = usbDevice[msExt->reader_index].polling_transfer;

		if (transfer)
		{
			int ret;

			ret = libusb_cancel_transfer(transfer);
			if (ret < 0)
				DEBUG_CRITICAL2("libusb_cancel_transfer failed: %d", ret);
		}
	}
} /* Multi_PollingTerminate */


/*****************************************************************************
 *
 *					Multi_InterruptRead
 *
 ****************************************************************************/
static int Multi_InterruptRead(int reader_index, int timeout /* in ms */)
{
	struct usbDevice_MultiSlot_Extension *msExt;
	unsigned char buffer[CCID_INTERRUPT_SIZE];
	struct timespec cond_wait_until;
	struct timeval local_time;
	int rv, status, interrupt_byte, interrupt_mask;

	msExt = usbDevice[reader_index].multislot_extension;

	/* When stopped, return 0 so IFDHPolling will return IFD_NO_SUCH_DEVICE */
	if ((msExt == NULL) || msExt->terminated)
		return 0;

	DEBUG_PERIODIC3("Multi_InterruptRead (%d), timeout: %d ms",
		reader_index, timeout);

	/* Select the relevant bit in the interrupt buffer */
	interrupt_byte = (usbDevice[reader_index].ccid.bCurrentSlotIndex / 4) + 1;
	interrupt_mask = 0x02 << (2 * (usbDevice[reader_index].ccid.bCurrentSlotIndex % 4));

	/* Wait until the condition is signaled or a timeout occurs */
	gettimeofday(&local_time, NULL);
	cond_wait_until.tv_sec = local_time.tv_sec;
	cond_wait_until.tv_nsec = local_time.tv_usec * 1000;

	cond_wait_until.tv_sec += timeout / 1000;
	cond_wait_until.tv_nsec += 1000000 * (timeout % 1000);

again:
	pthread_mutex_lock(&msExt->mutex);

	rv = pthread_cond_timedwait(&msExt->condition, &msExt->mutex,
		&cond_wait_until);

	if (0 == rv)
	{
		/* Retrieve interrupt buffer and request result */
		memcpy(buffer, msExt->buffer, sizeof buffer);
		status = msExt->status;
	}
	else
		if (rv == ETIMEDOUT)
			status = LIBUSB_TRANSFER_TIMED_OUT;
		else
			status = -1;

	/* Don't forget to unlock the mutex */
	pthread_mutex_unlock(&msExt->mutex);

	/* When stopped, return 0 so IFDHPolling will return IFD_NO_SUCH_DEVICE */
	if (msExt->terminated)
		return 0;

	/* Not stopped */
	if (status == LIBUSB_TRANSFER_COMPLETED)
	{
		if (0 == (buffer[interrupt_byte] & interrupt_mask))
		{
			DEBUG_PERIODIC2("Multi_InterruptRead (%d) -- skipped", reader_index);
			goto again;
		}
		DEBUG_PERIODIC2("Multi_InterruptRead (%d), got an interrupt", reader_index);
	}
	else
	{
		DEBUG_PERIODIC3("Multi_InterruptRead (%d), status=%d", reader_index, status);
	}

	return status;
} /* Multi_InterruptRead */


/*****************************************************************************
 *
 *					Multi_InterruptStop
 *
 ****************************************************************************/
static void Multi_InterruptStop(int reader_index)
{
	struct usbDevice_MultiSlot_Extension *msExt;
	int interrupt_byte, interrupt_mask;

	msExt = usbDevice[reader_index].multislot_extension;

	/* Already stopped ? */
	if ((NULL == msExt) || msExt->terminated)
		return;

	DEBUG_PERIODIC2("Stop (%d)", reader_index);

	interrupt_byte = (usbDevice[reader_index].ccid.bCurrentSlotIndex / 4) + 1;
	interrupt_mask = 0x02 << (2 * (usbDevice[reader_index].ccid.bCurrentSlotIndex % 4));

	pthread_mutex_lock(&msExt->mutex);

	/* Broacast an interrupt to wake-up the slot's thread */
	msExt->buffer[interrupt_byte] |= interrupt_mask;
	pthread_cond_broadcast(&msExt->condition);

	pthread_mutex_unlock(&msExt->mutex);
} /* Multi_InterruptStop */


/*****************************************************************************
 *
 *					Multi_CreateFirstSlot
 *
 ****************************************************************************/
static struct usbDevice_MultiSlot_Extension *Multi_CreateFirstSlot(int reader_index)
{
	struct usbDevice_MultiSlot_Extension *msExt;

	/* Allocate a new extension buffer */
	msExt = malloc(sizeof(struct usbDevice_MultiSlot_Extension));
	if (NULL == msExt)
		return NULL;

	/* Remember the index */
	msExt->reader_index = reader_index;

	msExt->terminated = FALSE;
	msExt->status = 0;
	msExt->transfer = NULL;

	/* Create mutex and condition object for the interrupt polling */
	pthread_mutex_init(&msExt->mutex, NULL);
	pthread_cond_init(&msExt->condition, NULL);

	/* create the thread in charge of the interrupt polling */
	pthread_create(&msExt->thread_proc, NULL, Multi_PollingProc, msExt);

	return msExt;
} /* Multi_CreateFirstSlot */


/*****************************************************************************
 *
 *					Multi_CreateNextSlot
 *
 ****************************************************************************/
static struct usbDevice_MultiSlot_Extension *Multi_CreateNextSlot(int physical_reader_index)
{
	/* Take the extension buffer from the main slot */
	return usbDevice[physical_reader_index].multislot_extension;
} /* Multi_CreateNextSlot */

