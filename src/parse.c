/*
    parse.c: parse CCID structure
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

#include <stdio.h>
#include <string.h> 
#include <usb.h>

#include "pcscdefines.h"
#include "ccid.h"
#include "ccid_usb.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int ccid_parse_interface_descriptor(usb_dev_handle *handle,
	struct usb_device *dev);


/*****************************************************************************
 *
 *					main
 *
 ****************************************************************************/
int main(int argc, char *argv[])
{
	status_t res;
	int channel;

	for (channel=0; channel<PCSCLITE_MAX_READERS; channel++)
	{
		res = OpenUSB(channel<<16, channel);
		if (res != STATUS_SUCCESS)
		{
			fprintf(stderr, "ccid_OpenUSB: 0x%02X\n", res);
			break;
		}
		else
		{
			usb_dev_handle *handle;
			struct usb_device *dev;

			get_desc(channel, &handle, &dev);
			res = ccid_parse_interface_descriptor(handle, dev);
		}
	}

	if (channel == 0)
		printf("No known CCID reader found\n");

	for (;channel>=0; channel--)
		CloseUSB(channel<<16);

	return 0;
} /* main */


/*****************************************************************************
 *
 *					Parse a CCID USB Descriptor
 *
 ****************************************************************************/
int ccid_parse_interface_descriptor(usb_dev_handle *handle,
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
	if (usb_get_string_simple(handle, dev->descriptor.iManufacturer,
		buffer, sizeof(buffer)) < 0)
		printf(" Can't get iManufacturer string\n");
	else
		printf(" iManufacturer: %s\n", buffer);

	if (usb_get_string_simple(handle, dev->descriptor.iProduct,
		buffer, sizeof(buffer)) < 0)
		printf(" Can't get iProduct string\n");
	else
		printf(" iProduct: %s\n", buffer);

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
	printf("  dwDataRate: %d bps\n", dw2i(extra, 19));
	printf("  dwMaxDataRate: %d bps\n", dw2i(extra, 23));
	printf("  bNumDataRatesSupported: %d\n", extra[27]);
	printf("  dwMaxIFSD: %d\n", dw2i(extra, 28));
	printf("  dwSynchProtocols: 0x%08X\n", dw2i(extra, 32));

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
	if (extra[40] == 0)
		printf("   No special characteristics\n");
	if (extra[40] & 0x02)
		printf("   Automatic parameter configuration based on ATR data\n");
	if (extra[40] & 0x04)
		printf("   Automatic activation of ICC on inserting\n");
	if (extra[40] & 0x08)
		printf("   Automatic ICC voltage selection\n");
	if (extra[40] & 0x10)
		printf("   Automatic ICC clock frequency change according to parameters\n");
	if (extra[40] & 0x20)
		printf("   Automatic baud rate change according to frequency and Fi, Di parameters\n");
	if (extra[40] & 0x40)
		printf("   Automatic parameters negotiation made by the ICC\n");
	if (extra[40] & 0x80)
		printf("   Automatic PPS made by the ICC\n");
	if (extra[41] & 0x01)
		printf("   CCID can set ICC in clock stop mode\n");
	if (extra[41] & 0x02)
		printf("   NAD value other than 00 accepted (T=1)\n");
	if (extra[41] & 0x04)
		printf("   Automatic IFSD exchange as first exchange (T=1)\n");
	if (extra[42] & 0x01)
		printf("   TPDU level exchange\n");
	if (extra[42] & 0x02)
		printf("   Short APDU level exchange\n");
	if (extra[42] & 0x04)
		printf("   Short and Extended APDU level exchange\n");

	printf("  dwMaxCCIDMessageLength: %d bytes\n", dw2i(extra, 44));
	printf("  bClassGetResponse: %d\n", extra[48]);
	printf("  bClassEnveloppe: %d\n", extra[49]);
	printf("  wLcdLayout: 0x%04X\n", (extra[51] << 8)+extra[50]);
	printf("  bPINSupport: 0x%02X\n", extra[52]);
	printf("  bMaxCCIDBusySlots: %d\n", extra[53]);

	return FALSE;
} /* ccid_parse_interface_descriptor */

