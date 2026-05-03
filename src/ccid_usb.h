/*
    ccid_usb.h:  USB access routines using the libusb library
    Copyright (C) 2003-2021   Ludovic Rousseau

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

#ifndef __CCID_USB_H__
#define __CCID_USB_H__
status_t OpenUSB(CcidDesc * ccid_reader, int channel);

status_t OpenUSBByName(CcidDesc * ccid_reader, /*@null@*/ char *device);

status_t WriteUSB(CcidDesc * ccid_reader, unsigned int length,
	unsigned char *Buffer);

status_t ReadUSB(CcidDesc * ccid_reader, unsigned int *length,
	/*@out@*/ unsigned char *Buffer, int bSeq);

status_t CloseUSB(CcidDesc * ccid_reader);
status_t DisconnectUSB(CcidDesc * ccid_reader);

#include <libusb.h>
/*@null@*/ const struct libusb_interface *get_ccid_usb_interface(
	struct libusb_config_descriptor *desc, int *num);

const unsigned char *get_ccid_device_descriptor(const struct libusb_interface *usb_interface);

int get_ccid_usb_device_path(CcidDesc * ccid_reader, unsigned char *buf, unsigned int *buflen);

int ControlUSB(CcidDesc * ccid_reader, int requesttype, int request, int value,
	unsigned char *bytes, unsigned int size);

int InterruptRead(CcidDesc * ccid_reader, int timeout);
void InterruptStop(CcidDesc * ccid_reader);
#endif
