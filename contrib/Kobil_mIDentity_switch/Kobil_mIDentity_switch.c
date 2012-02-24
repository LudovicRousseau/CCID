/*
    Activate the smartcard interface on the kobil midentity usb device
    Copyright (C) 2006  Norbert Federa <norbert.federa@neoware.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

Author:	Norbert Federa <norbert.federa@neoware.com>
Date:   2006-04-06


Description:

This tool is needed to activate the smartcard interface on the kobil midentity
usb device (vendor 0x04D6 id 0x4081)

Kobil's own implementation was a kernel usb driver which did just send a
libusb_control_transfer in the probe routine.

We do the same via libusb and call this program from our /sbin/hotblug script
if the mIDentity gets added.

The kobil switcher driver was found inside this zip ...
http://www.kobil.com/download/partner/KOBIL_mIDentity_SDK_Build_20060320_RELEASE.zip
... under Interfaces/Linux/module_with_binary_final.tar.gz.

Here the interesting part of the kernel driver inside the probe function:

        if (dev->descriptor.idVendor == KOBIL_VENDOR_ID){
                printk("!!!!! DEVICE FOUND !!! !\n");
		ret = libusb_control_transfer(dev,
		send_pipe,
		0x09,
		0x22,
		0x0200,
		0x0001,
		switchCmd,
		sizeof(switchCmd),
		5000);
	}

Initally the it did not work with libusb because the ioctl gets ignored with
the used RequestType of 0x22 in combination with index 0x0001, but index 0x0002
worked.  See usb/devio.c functions  proc_control() ->  check_ctrlrecip() ->
findintfep() in order to understand why.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libusb.h>
#include <errno.h>

#include "config.h"

#define KOBIL_VENDOR_ID		0x0D46
#define MID_DEVICE_ID		0x4081
#define KOBIL_TIMEOUT		5000
#define VAL_STARTUP_4080        1
#define VAL_STARTUP_4000        2
#define VAL_STARTUP_4020        3
#define VAL_STARTUP_40A0        4
#define HIDCMD_SWITCH_DEVICE    0x0004

#define bmRequestType           0x22
#define bRequest                0x09
#define wValue                  0x0200
#define wIndex                  0x0002  /* this was originally 0x0001 */


static int kobil_midentity_control_msg(libusb_device_handle *usb)
{
	int ret;

	unsigned char switchCmd[10];

	unsigned char Sleep = 1;
	unsigned char hardDisk = 1;

	unsigned char param = ((hardDisk) << 4) | (Sleep);

	memset(switchCmd, 0x0, sizeof(switchCmd));
	switchCmd[0] = HIDCMD_SWITCH_DEVICE >> 8;
	switchCmd[1] = HIDCMD_SWITCH_DEVICE;
	switchCmd[5] = VAL_STARTUP_4000;
	switchCmd[9] = param;

	ret = libusb_control_transfer(usb, bmRequestType, bRequest, wValue, wIndex,
			switchCmd, sizeof(switchCmd), KOBIL_TIMEOUT);

	return(!(ret==sizeof(switchCmd)));
}


static int kobil_midentity_claim_interface(libusb_device_handle *usb, int ifnum)
{
	int rv;

	printf("claiming interface #%d ... ", ifnum);
	rv = libusb_claim_interface(usb, ifnum);
	if (rv == 0)
	{
		printf("success\n");
		return rv;
	}
	else
		printf("failed\n");

	printf("failed with error %d, trying to detach kernel driver ....\n", rv);
	rv = libusb_detach_kernel_driver(usb, ifnum);
	if (rv == 0)
	{
		printf("success, claiming interface again ...");
		rv = libusb_claim_interface(usb, ifnum);
		if (rv == 0)
		{
			printf("success\n");
			return rv;
		}
		else
			printf("failed\n");
	}

	printf("failed with error %d, giving up.\n", rv);
	return rv;
}


int main(int argc, char *argv[])
{
	libusb_device **devs, *dev;
	libusb_device *found_dev = NULL;
	struct libusb_device_handle *usb = NULL;
	int rv, i;
	ssize_t cnt;

	(void)argc;
	(void)argv;

	rv = libusb_init(NULL);
	if (rv < 0)
	{
		(void)printf("libusb_init() failed\n");
		return rv;
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

		rv = libusb_get_device_descriptor(dev, &desc);
		if (rv < 0) {
			(void)printf("failed to get device descriptor\n");
			continue;
		}

		printf("vendor/product: %04X %04X\n", desc.idVendor, desc.idProduct);
		if (desc.idVendor == KOBIL_VENDOR_ID && desc.idProduct == MID_DEVICE_ID)
			found_dev = dev;
	}

	if (found_dev == NULL)
	{
		printf("device not found. aborting.\n");
		if (0 != geteuid())
			printf("Try to rerun this program as root.\n");
		exit(1);
	}

	printf("Device found, opening ... ");
	rv = libusb_open(found_dev, &usb);
	if (rv < 0)
	{
		printf("failed, aborting.\n");
		exit(2);
	}
	printf("success\n");

	rv = kobil_midentity_claim_interface(usb, 0);
	if (rv < 0)
	{
		libusb_close(usb);
		exit(3);
	}

	rv = kobil_midentity_claim_interface(usb, 1);
	if (rv < 0)
	{
		libusb_close(usb);
		exit(3);
	}

	printf("Activating the CCID configuration .... ");
	rv = kobil_midentity_control_msg(usb);
	if (rv == 0)
		printf("success\n");
	else
		printf("failed with error %d, giving up.\n", rv);

	libusb_close(usb);

	return 0;
}

