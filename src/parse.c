/*
    parse.c: parse CCID structure
    Copyright (C) 2003-2004   Ludovic Rousseau

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

#include <stdio.h>
#include <string.h> 
# ifdef S_SPLINT_S
# include <sys/types.h>
# endif
#include <usb.h>

#include <ifdhandler.h>
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "ccid.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static int ccid_parse_interface_descriptor(usb_dev_handle *handle,
	struct usb_device *dev);


/*****************************************************************************
 *
 *					main
 *
 ****************************************************************************/
int main(/*@unused@*/ int argc, /*@unused@*/ char *argv[])
{
	status_t res;
	unsigned int channel;

	for (channel=0; channel<CCID_DRIVER_MAX_READERS; channel++)
	{
		res = IFDHCreateChannel(channel<<16, channel);
		if (res != IFD_SUCCESS)
		{
			fprintf(stderr, "ccid_OpenUSB: %d\n", res);
			break;
		}
		else
		{
			usb_dev_handle *handle;
			struct usb_device *dev;

			if (get_desc(channel<<16, &handle, &dev))
				res = ccid_parse_interface_descriptor(handle, dev);
			else
				fprintf(stderr, "Reader %d not found\n", channel);
		}
	}

	if (channel == 0)
		printf("No known CCID reader found\n");
	else
		for (channel--; channel!=0; channel--)
			(void)IFDHCloseChannel(channel<<16);

	return 0;
} /* main */


/*****************************************************************************
 *
 *					Parse a CCID USB Descriptor
 *
 ****************************************************************************/
static int ccid_parse_interface_descriptor(usb_dev_handle *handle,
	struct usb_device *dev)
{
	struct usb_interface_descriptor *usb_interface;
	unsigned char *extra;
	char buffer[255];

	/*
	 * Interface Descriptor
	 */
	printf("Parsing Interface Descriptor for device: %s/%s\n",
		dev->bus->dirname, dev->filename);

	/*
	 * Vendor/model name
	 */
	printf(" idVendor: 0x%04X\n", dev->descriptor.idVendor);
	if (usb_get_string_simple(handle, dev->descriptor.iManufacturer,
		buffer, sizeof(buffer)) < 0)
		printf("  Can't get iManufacturer string\n");
	else
		printf("  iManufacturer: %s\n", buffer);

	printf(" idProduct: 0x%04X\n", dev->descriptor.idProduct);
	if (usb_get_string_simple(handle, dev->descriptor.iProduct,
		buffer, sizeof(buffer)) < 0)
		printf("  Can't get iProduct string\n");
	else
		printf("  iProduct: %s\n", buffer);

	printf(" bcdDevice: %X.%02X (firmware release?)\n",
		dev->descriptor.bcdDevice >> 8, dev->descriptor.bcdDevice & 0xFF);

	usb_interface = get_ccid_usb_interface(dev)->altsetting;
	
	printf(" bLength: %d\n", usb_interface->bLength);
	
	printf(" bDescriptorType: %d\n", usb_interface->bDescriptorType);
	
	printf(" bInterfaceNumber: %d\n", usb_interface->bInterfaceNumber);
	
	printf(" bAlternateSetting: %d\n", usb_interface->bAlternateSetting);
	
	printf(" bNumEndpoints: %d\n", usb_interface->bNumEndpoints);

	printf(" bInterfaceClass: 0x%02X", usb_interface->bInterfaceClass);
	if (usb_interface->bInterfaceClass == 0x0b)
		printf(" [Chip Card Interface Device Class (CCID)]\n");
	else
	{
		printf("\n  NOT A CCID DEVICE\n");
		if (usb_interface->bInterfaceClass != 0xFF)
			return TRUE;
		else
			printf("  Class is 0xFF (proprietary)\n");
	}
	
	printf(" bInterfaceSubClass: %d\n", usb_interface->bInterfaceSubClass);
	if (usb_interface->bInterfaceSubClass)
		printf("  UNSUPPORTED SubClass\n");

	printf(" bInterfaceProtocol: %d\n", usb_interface->bInterfaceProtocol);
	if (usb_interface->bInterfaceProtocol)
		printf("  UNSUPPORTED InterfaceProtocol\n");

	printf(" iInterface: %d\n", usb_interface->iInterface);

	if (usb_interface->extralen < 54)
	{
		printf("USB extra length is too short: %d\n", usb_interface->extralen);
		printf("\n  NOT A CCID DEVICE\n");
		return TRUE;
	}

	/*
	 * CCID Class Descriptor
	 */
	printf(" CCID Class Descriptor\n");
	extra = usb_interface->extra;

	printf("  bLength: 0x%02X\n", extra[0]);
	if (extra[0] != 0x36)
	{
		printf("   UNSUPPORTED bLength\n");
		return TRUE;
	}

	printf("  bDescriptorType: 0x%02X\n", extra[1]);
	if (extra[1] != 0x21)
	{
		if (0xFF == extra[1])
			printf("   PROPRIETARY bDescriptorType\n");
		else
		{
			printf("   UNSUPPORTED bDescriptorType\n");
			return TRUE;
		}
	}

	printf("  bcdCCID: %X.%02X\n", extra[3], extra[2]);
	printf("  bMaxSlotIndex: 0x%02X\n", extra[4]);
	printf("  bVoltageSupport: 0x%02X\n", extra[5]);
	if (extra[5] & 0x01)
		printf("   5.0V\n");
	if (extra[5] & 0x02)
		printf("   3.0V\n");
	if (extra[5] & 0x04)
		printf("   1.8V\n");

	printf("  dwProtocols: 0x%02X%02X 0x%02X%02X\n", extra[9], extra[8],
		extra[7],extra[6]);
	if (extra[6] & 0x01)
		printf("   T=0\n");
	if (extra[6] & 0x02)
		printf("   T=1\n");

	printf("  dwDefaultClock: %.3f MHz\n", dw2i(extra, 10)/1000.0);
	printf("  dwMaximumClock: %.3f MHz\n", dw2i(extra, 14)/1000.0);
	printf("  bNumClockSupported: 0x%02X\n", extra[18]);
	{
		unsigned char buffer[256*sizeof(int)];  /* maximum is 256 records */
		int n;

		/* See CCID 3.7.2 page 25 */
		n = usb_control_msg(handle,
			0xA1, /* request type */
			0x02, /* GET CLOCK FREQUENCIES */
			0x00, /* value */
			usb_interface->bInterfaceNumber, /* interface */
			(char *)buffer,
			sizeof(buffer),
			2 * 1000);

		/* we got an error? */
		if (n <= 0)
			printf("   IFD does not support GET CLOCK FREQUENCIES request\n");
		else
			if (n % 4) 	/* not a multiple of 4 */
				printf("   wrong size for GET CLOCK FREQUENCIES: %d\n", n);
			else
			{
				int i;

				for (i=0; i<n; i+=4)
					printf("   Support %d kHz\n", dw2i(buffer, i));
			}
	}
	printf("  dwDataRate: %d bps\n", dw2i(extra, 19));
	printf("  dwMaxDataRate: %d bps\n", dw2i(extra, 23));
	printf("  bNumDataRatesSupported: %d\n", extra[27]);
	{
		unsigned char buffer[256*sizeof(int)];  /* maximum is 256 records */
		int n;

		/* See CCID 3.7.3 page 25 */
		n = usb_control_msg(handle,
			0xA1, /* request type */
			0x03, /* GET DATA RATES */
			0x00, /* value */
			usb_interface->bInterfaceNumber, /* interface */
			(char *)buffer,
			sizeof(buffer),
			2 * 1000);

		/* we got an error? */
		if (n <= 0)
			printf("   IFD does not support GET_DATA_RATES request\n");
		else
			if (n % 4) 	/* not a multiple of 4 */
				printf("   wrong size for GET_DATA_RATES: %d\n", n);
			else
			{
				int i;

				for (i=0; i<n; i+=4)
					printf("   Support %d bps\n", dw2i(buffer, i));
			}
	}
	printf("  dwMaxIFSD: %d\n", dw2i(extra, 28));
	printf("  dwSynchProtocols: 0x%08X\n", dw2i(extra, 32));
	if (extra[35] & 0x01)
			printf("   2-wire protocol\n");
	if (extra[35] & 0x02)
			printf("   3-wire protocol\n");
	if (extra[35] & 0x04)
			printf("   I2C protocol\n");

	printf("  dwMechanical: 0x%08X\n", dw2i(extra, 36));
	if (extra[36] == 0)
		printf("   No special characteristics\n");
	if (extra[36] & 0x01)
		printf("   Card accept mechanism\n");
	if (extra[36] & 0x02)
		printf("   Card ejection mechanism\n");
	if (extra[36] & 0x04)
		printf("   Card capture mechanism\n");
	if (extra[36] & 0x08)
		printf("   Card lock/unlock mechanism\n");

	printf("  dwFeatures: 0x%08X\n", dw2i(extra, 40));
	if (dw2i(extra, 40) == 0)
		printf("   No special characteristics\n");
	if (extra[40] & 0x02)
		printf("   ....02 Automatic parameter configuration based on ATR data\n");
	if (extra[40] & 0x04)
		printf("   ....04 Automatic activation of ICC on inserting\n");
	if (extra[40] & 0x08)
		printf("   ....08 Automatic ICC voltage selection\n");
	if (extra[40] & 0x10)
		printf("   ....10 Automatic ICC clock frequency change according to parameters\n");
	if (extra[40] & 0x20)
		printf("   ....20 Automatic baud rate change according to frequency and Fi, Di params\n");
	if (extra[40] & 0x40)
		printf("   ....40 Automatic parameters negotiation made by the CCID\n");
	if (extra[40] & 0x80)
		printf("   ....80 Automatic PPS made by the CCID\n");
	if (extra[41] & 0x01)
		printf("   ..01.. CCID can set ICC in clock stop mode\n");
	if (extra[41] & 0x02)
		printf("   ..02.. NAD value other than 00 accepted (T=1)\n");
	if (extra[41] & 0x04)
		printf("   ..04.. Automatic IFSD exchange as first exchange (T=1)\n");
	switch (extra[42] & 0x07)
	{
		case 0x00:
			printf("   00.... Character level exchange\n");
			break;

		case 0x01:
			printf("   01.... TPDU level exchange\n");
			break;

		case 0x02:
			printf("   02.... Short APDU level exchange\n");
			break;

		case 0x04:
			printf("   04.... Short and Extended APDU level exchange\n");
			break;
	}

	printf("  dwMaxCCIDMessageLength: %d bytes\n", dw2i(extra, 44));
	printf("  bClassGetResponse: 0x%02X\n", extra[48]);
	if (0xFF == extra[48])
		printf("   echoes the APDU class\n");
	printf("  bClassEnveloppe: %d\n", extra[49]);
	printf("  wLcdLayout: 0x%04X\n", (extra[51] << 8)+extra[50]);
	printf("  bPINSupport: 0x%02X\n", extra[52]);
	if (extra[52] & 0x01)
		printf("   PIN Verification supported\n");
	if (extra[52] & 0x02)
		printf("   PIN Modification supported\n");
	printf("  bMaxCCIDBusySlots: %d\n", extra[53]);

	return FALSE;
} /* ccid_parse_interface_descriptor */

