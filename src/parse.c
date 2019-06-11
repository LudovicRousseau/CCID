/*
    parse.c: parse CCID structure
    Copyright (C) 2003-2010   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 51
	Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <string.h>
# ifdef S_SPLINT_S
# include <sys/types.h>
# endif
#include <errno.h>
#include <unistd.h>

#include "defs.h"
#include "ccid.h"

/* define DISPLAY_EXTRA_VALUES to display the extra (invalid) values
 * returned by bNumClockSupported and bNumDataRatesSupported */
#undef DISPLAY_EXTRA_VALUES

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define BLUE "\33[34m"
#define RED "\33[31m"
#define BRIGHT_RED "\33[01;31m"
#define GREEN "\33[32m"
#define MAGENTA "\33[35m"
#define NORMAL "\33[0m"

/* global variables used in ccid_usb.c but defined in ifdhandler.c */
int LogLevel = 1+2+4+8; /* full debug */
int DriverOptions = 0;

static int ccid_parse_interface_descriptor(libusb_device_handle *handle,
	struct libusb_device_descriptor desc,
	struct libusb_config_descriptor *config_desc,
	int num,
	const struct libusb_interface *usb_interface);


/*****************************************************************************
 *
 *					main
 *
 ****************************************************************************/
int main(int argc, char *argv[])
{
	libusb_device **devs, *dev;
	int nb = 0, r, i;
	unsigned char buffer[256];
	char class_ff = FALSE;
	ssize_t cnt;

	if ((argc > 1) && (0 == strcmp(argv[1], "-p")))
		class_ff = TRUE;

	r = libusb_init(NULL);
	if (r < 0)
	{
		(void)printf("libusb_init() failed\n");
        return r;
	}

	cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
	{
		(void)printf("libusb_get_device_list() failed\n");
        return (int)cnt;
	}

	/* for every device */
	i = 0;
	while ((dev = devs[i++]) != NULL)
	{
		struct libusb_device_descriptor desc;
		struct libusb_config_descriptor *config_desc;
		struct libusb_device_handle *handle;
		const struct libusb_interface *usb_interface = NULL;
#ifndef __APPLE__
		int interface;
#endif
		int num = 0;

		r = libusb_open(dev, &handle);
		if (r < 0)
		{
			(void)fprintf(stderr, "Can't libusb_open(): %s\n",
				libusb_error_name(r));
			if (getuid())
			{
				(void)fprintf(stderr,
					BRIGHT_RED "Please, restart the command as root\n" NORMAL);
				return 1;
			}
			continue;
		}

		r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0)
		{
            (void)fprintf(stderr,
				BRIGHT_RED "failed to get device descriptor" NORMAL);
            return 1;
        }

		(void)fprintf(stderr,
			"Parsing USB bus/device: %04X:%04X (bus %d, device %d)\n",
			desc.idVendor, desc.idProduct,
            libusb_get_bus_number(dev), libusb_get_device_address(dev));

		(void)fprintf(stderr, " idVendor:  0x%04X", desc.idVendor);
		r = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer,
			buffer, sizeof(buffer));
		if (r < 0)
		{
			(void)fprintf(stderr, "  Can't get iManufacturer string\n");
			if (getuid())
			{
				(void)fprintf(stderr,
					BRIGHT_RED "Please, restart the command as root\n" NORMAL);
				return 1;
			}
		}
		else
			(void)fprintf(stderr,
				"  iManufacturer: " BLUE "%s\n" NORMAL, buffer);

		(void)fprintf(stderr, " idProduct: 0x%04X", desc.idProduct);
		r = libusb_get_string_descriptor_ascii(handle, desc.iProduct,
			buffer, sizeof(buffer));
		if (r < 0)
			(void)fprintf(stderr, "  Can't get iProduct string\n");
		else
			(void)fprintf(stderr, "  iProduct: " BLUE "%s\n" NORMAL, buffer);

again:
		/* check if the device has bInterfaceClass == 11 */
		r = libusb_get_active_config_descriptor(dev, &config_desc);
		if (r < 0)
		{
			(void)fprintf(stderr, "  Can't get config descriptor: %d\n", r);
			(void)libusb_close(handle);
			continue;
		}

		usb_interface = get_ccid_usb_interface(config_desc, &num);
		if (NULL == usb_interface)
		{
			(void)libusb_close(handle);
			/* only if we found no CCID interface */
			if (0 == num)
				(void)fprintf(stderr, RED "  NOT a CCID/ICCD device\n" NORMAL);
			continue;
		}
		if (!class_ff && (0xFF == usb_interface->altsetting->bInterfaceClass))
		{
			(void)libusb_close(handle);
			(void)fprintf(stderr, MAGENTA "  Found a possibly CCID/ICCD device (bInterfaceClass = 0xFF). Use %s -p\n" NORMAL, argv[0]);
			continue;
		}
		(void)fprintf(stderr,
			GREEN "  Found a CCID/ICCD device at interface %d\n" NORMAL, num);

		/* now we found a free reader and we try to use it */
#if 0
		if (NULL == dev->config)
		{
			(void)libusb_close(handle);
			(void)fprintf(stderr, "No dev->config found for %s/%s\n",
					bus->dirname, dev->filename);
			continue;
		}
#endif

#ifndef __APPLE__
		interface = usb_interface->altsetting->bInterfaceNumber;
		r = libusb_claim_interface(handle, interface);
		if (r < 0)
		{
			(void)fprintf(stderr,
				"Can't claim interface (bus %d, device %d): %s\n",
				libusb_get_bus_number(dev), libusb_get_device_address(dev),
				libusb_error_name(r));
			(void)libusb_close(handle);

			if (EBUSY == errno)
			{
				(void)fprintf(stderr,
					BRIGHT_RED " Please, stop pcscd and retry\n\n" NORMAL);

				if (class_ff)
					/* maybe the device with Class = 0xFF is NOT a CCID
					 * reader */
					continue;
				else
					return TRUE;
			}
			continue;
		}
#endif

		(void)ccid_parse_interface_descriptor(handle, desc, config_desc, num,
			usb_interface);
		nb++;

#ifndef __APPLE__
		(void)libusb_release_interface(handle, interface);
#endif
		/* check for another CCID interface on the same device */
		num++;
		goto again;
	}

	if ((0 == nb) && (0 != geteuid()))
		(void)fprintf(stderr,
			"Can't find any CCID device.\nMaybe you must run parse as root?\n");

	libusb_exit(NULL);

	return 0;
} /* main */


/*****************************************************************************
 *
 *					Parse a CCID USB Descriptor
 *
 ****************************************************************************/
static int ccid_parse_interface_descriptor(libusb_device_handle *handle,
	struct libusb_device_descriptor desc,
	struct libusb_config_descriptor *config_desc,
	int num,
	const struct libusb_interface *usb_interface)
{
	const struct libusb_interface_descriptor *usb_interface_descriptor;
	const unsigned char *device_descriptor;
	unsigned char buffer[256*sizeof(int)];  /* maximum is 256 records */
	int r;

	/*
	 * Vendor/model name
	 */
	(void)printf(" idVendor: 0x%04X\n", desc.idVendor);
	r = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer,
		buffer, sizeof(buffer));
	if (r < 0)
	{
		(void)printf("  Can't get iManufacturer string\n");
		if (getuid())
		{
			(void)fprintf(stderr,
				BRIGHT_RED "Please, restart the command as root\n\n" NORMAL);
			return TRUE;
		}
	}
	else
		(void)printf("  iManufacturer: %s\n", buffer);

	(void)printf(" idProduct: 0x%04X\n", desc.idProduct);
	r =  libusb_get_string_descriptor_ascii(handle, desc.iProduct,
		buffer, sizeof(buffer));
	if (r < 0)
		(void)printf("  Can't get iProduct string\n");
	else
		(void)printf("  iProduct: %s\n", buffer);

	(void)printf(" bcdDevice: %X.%02X (firmware release?)\n",
		desc.bcdDevice >> 8, desc.bcdDevice & 0xFF);

	usb_interface_descriptor = get_ccid_usb_interface(config_desc, &num)->altsetting;

	(void)printf(" bLength: %d\n", usb_interface_descriptor->bLength);

	(void)printf(" bDescriptorType: %d\n", usb_interface_descriptor->bDescriptorType);

	(void)printf(" bInterfaceNumber: %d\n", usb_interface_descriptor->bInterfaceNumber);

	(void)printf(" bAlternateSetting: %d\n", usb_interface_descriptor->bAlternateSetting);

	(void)printf(" bNumEndpoints: %d\n", usb_interface_descriptor->bNumEndpoints);
	switch (usb_interface_descriptor->bNumEndpoints)
	{
		case 0:
			(void)printf("  Control only\n");
			break;
		case 1:
			(void)printf("  Interrupt-IN\n");
			break;
		case 2:
			(void)printf("  bulk-IN and bulk-OUT\n");
			break;
		case 3:
			(void)printf("  bulk-IN, bulk-OUT and Interrupt-IN\n");
			break;
		default:
			(void)printf("  UNKNOWN value\n");
	}

	(void)printf(" bInterfaceClass: 0x%02X", usb_interface_descriptor->bInterfaceClass);
	if (usb_interface_descriptor->bInterfaceClass == 0x0b)
		(void)printf(" [Chip Card Interface Device Class (CCID)]\n");
	else
	{
		(void)printf("\n  NOT A CCID DEVICE\n");
		if (usb_interface_descriptor->bInterfaceClass != 0xFF)
			return TRUE;
		else
			(void)printf("  Class is 0xFF (proprietary)\n");
	}

	(void)printf(" bInterfaceSubClass: %d\n",
		usb_interface_descriptor->bInterfaceSubClass);
	if (usb_interface_descriptor->bInterfaceSubClass)
		(void)printf("  UNSUPPORTED SubClass\n");

	(void)printf(" bInterfaceProtocol: %d\n",
		usb_interface_descriptor->bInterfaceProtocol);
	switch (usb_interface_descriptor->bInterfaceProtocol)
	{
		case 0:
			(void)printf("  bulk transfer, optional interrupt-IN (CCID)\n");
			break;
		case 1:
			(void)printf("  ICCD Version A, Control transfers, (no interrupt-IN)\n");
			break;
		case 2:
			(void)printf("  ICCD Version B, Control transfers, (optional interrupt-IN)\n");
			break;
		default:
			(void)printf("  UNSUPPORTED InterfaceProtocol\n");
	}

	r = libusb_get_string_descriptor_ascii(handle, usb_interface_descriptor->iInterface,
		buffer, sizeof(buffer));
	if (r < 0)
		(void)printf(" Can't get iInterface string\n");
	else
		(void)printf(" iInterface: %s\n", buffer);

	device_descriptor = get_ccid_device_descriptor(usb_interface);
	if (NULL == device_descriptor)
	{
		(void)printf("\n  NOT A CCID DEVICE\n");
		return TRUE;
	}

	/*
	 * CCID Class Descriptor
	 */
	(void)printf(" CCID Class Descriptor\n");

	(void)printf("  bLength: 0x%02X\n", device_descriptor[0]);
	if (device_descriptor[0] != 0x36)
	{
		(void)printf("   UNSUPPORTED bLength\n");
		return TRUE;
	}

	(void)printf("  bDescriptorType: 0x%02X\n", device_descriptor[1]);
	if (device_descriptor[1] != 0x21)
	{
		if (0xFF == device_descriptor[1])
			(void)printf("   PROPRIETARY bDescriptorType\n");
		else
		{
			(void)printf("   UNSUPPORTED bDescriptorType\n");
			return TRUE;
		}
	}

	(void)printf("  bcdCCID: %X.%02X\n", device_descriptor[3], device_descriptor[2]);
	(void)printf("  bMaxSlotIndex: 0x%02X\n", device_descriptor[4]);
	(void)printf("  bVoltageSupport: 0x%02X\n", device_descriptor[5]);
	if (device_descriptor[5] & 0x01)
		(void)printf("   5.0V\n");
	if (device_descriptor[5] & 0x02)
		(void)printf("   3.0V\n");
	if (device_descriptor[5] & 0x04)
		(void)printf("   1.8V\n");

	(void)printf("  dwProtocols: 0x%02X%02X 0x%02X%02X\n", device_descriptor[9], device_descriptor[8],
		device_descriptor[7], device_descriptor[6]);
	if (device_descriptor[6] & 0x01)
		(void)printf("   T=0\n");
	if (device_descriptor[6] & 0x02)
		(void)printf("   T=1\n");

	(void)printf("  dwDefaultClock: %.3f MHz\n", dw2i(device_descriptor, 10)/1000.0);
	(void)printf("  dwMaximumClock: %.3f MHz\n", dw2i(device_descriptor, 14)/1000.0);
	(void)printf("  bNumClockSupported: %d%s\n", device_descriptor[18],
		device_descriptor[18] ? "" : " (will use whatever is returned)");
	{
		int n;

		/* See CCID 3.7.2 page 25 */
		n = libusb_control_transfer(handle,
			0xA1, /* request type */
			0x02, /* GET CLOCK FREQUENCIES */
			0x00, /* value */
			usb_interface_descriptor->bInterfaceNumber, /* interface */
			buffer,
			sizeof(buffer),
			2 * 1000);

		/* we got an error? */
		if (n <= 0)
		{
			(void)(void)printf("   IFD does not support GET CLOCK FREQUENCIES request: %s\n", libusb_error_name(n));
			if (EBUSY == errno)
			{
				(void)fprintf(stderr,
					BRIGHT_RED "   Please, stop pcscd and retry\n\n" NORMAL);
				return TRUE;
			}
		}
		else
			if (n % 4)	/* not a multiple of 4 */
				(void)printf("   wrong size for GET CLOCK FREQUENCIES: %d\n", n);
			else
			{
				int i;

				/* we do not get the expected number of data rates */
				if ((n != device_descriptor[18]*4) && device_descriptor[18])
				{
					(void)printf("   Got %d clock frequencies but was expecting %d\n",
						n/4, device_descriptor[18]);

					/* we got more data than expected */
#ifndef DISPLAY_EXTRA_VALUES
					if (n > device_descriptor[18]*4)
						n = device_descriptor[18]*4;
#endif
				}

				for (i=0; i<n; i+=4)
					(void)printf("   Support %d kHz\n", dw2i(buffer, i));
			}
	}
	(void)printf("  dwDataRate: %d bps\n", dw2i(device_descriptor, 19));
	(void)printf("  dwMaxDataRate: %d bps\n", dw2i(device_descriptor, 23));
	(void)printf("  bNumDataRatesSupported: %d%s\n", device_descriptor[27],
		device_descriptor[27] ? "" : " (will use whatever is returned)");
	{
		int n;

		/* See CCID 3.7.3 page 25 */
		n = libusb_control_transfer(handle,
			0xA1, /* request type */
			0x03, /* GET DATA RATES */
			0x00, /* value */
			usb_interface_descriptor->bInterfaceNumber, /* interface */
			buffer,
			sizeof(buffer),
			2 * 1000);

		/* we got an error? */
		if (n <= 0)
			(void)printf("   IFD does not support GET_DATA_RATES request: %s\n",
				libusb_error_name(n));
		else
			if (n % 4)	/* not a multiple of 4 */
				(void)printf("   wrong size for GET_DATA_RATES: %d\n", n);
			else
			{
				int i;

				/* we do not get the expected number of data rates */
				if ((n != device_descriptor[27]*4) && device_descriptor[27])
				{
					(void)printf("   Got %d data rates but was expecting %d\n", n/4,
						device_descriptor[27]);

					/* we got more data than expected */
#ifndef DISPLAY_EXTRA_VALUES
					if (n > device_descriptor[27]*4)
						n = device_descriptor[27]*4;
#endif
				}

				for (i=0; i<n; i+=4)
					(void)printf("   Support %d bps\n", dw2i(buffer, i));
			}
	}
	(void)printf("  dwMaxIFSD: %d\n", dw2i(device_descriptor, 28));
	(void)printf("  dwSynchProtocols: 0x%08X\n", dw2i(device_descriptor, 32));
	if (device_descriptor[32] & 0x01)
			(void)printf("   2-wire protocol\n");
	if (device_descriptor[32] & 0x02)
			(void)printf("   3-wire protocol\n");
	if (device_descriptor[32] & 0x04)
			(void)printf("   I2C protocol\n");

	(void)printf("  dwMechanical: 0x%08X\n", dw2i(device_descriptor, 36));
	if (device_descriptor[36] == 0)
		(void)printf("   No special characteristics\n");
	if (device_descriptor[36] & 0x01)
		(void)printf("   Card accept mechanism\n");
	if (device_descriptor[36] & 0x02)
		(void)printf("   Card ejection mechanism\n");
	if (device_descriptor[36] & 0x04)
		(void)printf("   Card capture mechanism\n");
	if (device_descriptor[36] & 0x08)
		(void)printf("   Card lock/unlock mechanism\n");

	(void)printf("  dwFeatures: 0x%08X\n", dw2i(device_descriptor, 40));
	if (dw2i(device_descriptor, 40) == 0)
		(void)printf("   No special characteristics\n");
	if (device_descriptor[40] & 0x02)
		(void)printf("   ....02 Automatic parameter configuration based on ATR data\n");
	if (device_descriptor[40] & 0x04)
		(void)printf("   ....04 Automatic activation of ICC on inserting\n");
	if (device_descriptor[40] & 0x08)
		(void)printf("   ....08 Automatic ICC voltage selection\n");
	if (device_descriptor[40] & 0x10)
		(void)printf("   ....10 Automatic ICC clock frequency change according to parameters\n");
	if (device_descriptor[40] & 0x20)
		(void)printf("   ....20 Automatic baud rate change according to frequency and Fi, Di params\n");
	if (device_descriptor[40] & 0x40)
		(void)printf("   ....40 Automatic parameters negotiation made by the CCID\n");
	if (device_descriptor[40] & 0x80)
		(void)printf("   ....80 Automatic PPS made by the CCID\n");
	if (device_descriptor[41] & 0x01)
		(void)printf("   ..01.. CCID can set ICC in clock stop mode\n");
	if (device_descriptor[41] & 0x02)
		(void)printf("   ..02.. NAD value other than 00 accepted (T=1)\n");
	if (device_descriptor[41] & 0x04)
		(void)printf("   ..04.. Automatic IFSD exchange as first exchange (T=1)\n");
	if (device_descriptor[41] & 0x08)
		(void)printf("   ..08.. Unknown (ICCD?)\n");
	switch (device_descriptor[42] & 0x07)
	{
		case 0x00:
			(void)printf("   00.... Character level exchange\n");
			break;

		case 0x01:
			(void)printf("   01.... TPDU level exchange\n");
			break;

		case 0x02:
			(void)printf("   02.... Short APDU level exchange\n");
			break;

		case 0x04:
			(void)printf("   04.... Short and Extended APDU level exchange\n");
			break;
	}
	if (device_descriptor[42] & 0x10)
		(void)printf("   10.... USB Wake up signaling supported on card insertion and removal\n");

	(void)printf("  dwMaxCCIDMessageLength: %d bytes\n", dw2i(device_descriptor, 44));
	(void)printf("  bClassGetResponse: 0x%02X\n", device_descriptor[48]);
	if (0xFF == device_descriptor[48])
		(void)printf("   echoes the APDU class\n");
	(void)printf("  bClassEnvelope: 0x%02X\n", device_descriptor[49]);
	if (0xFF == device_descriptor[49])
		(void)printf("   echoes the APDU class\n");
	(void)printf("  wLcdLayout: 0x%04X\n", (device_descriptor[51] << 8)+device_descriptor[50]);
	if (device_descriptor[51])
		(void)printf("   %2d lines\n", device_descriptor[51]);
	if (device_descriptor[50])
		(void)printf("   %2d characters per line\n", device_descriptor[50]);
	(void)printf("  bPINSupport: 0x%02X\n", device_descriptor[52]);
	if (device_descriptor[52] & 0x01)
		(void)printf("   PIN Verification supported\n");
	if (device_descriptor[52] & 0x02)
		(void)printf("   PIN Modification supported\n");
	(void)printf("  bMaxCCIDBusySlots: %d\n", device_descriptor[53]);

	return FALSE;
} /* ccid_parse_interface_descriptor */

