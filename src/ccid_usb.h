/*
    ccid_usb.h:  USB access routines using the libusb library
    Copyright (C) 2003-2004   Ludovic Rousseau

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

/*
 * $Id$
 */

status_t OpenUSB(unsigned int reader_index, int channel);

status_t OpenUSBByName(unsigned int reader_index, /*@null@*/ char *device);

status_t WriteUSB(unsigned int reader_index, unsigned int length,
	unsigned char *Buffer);

status_t ReadUSB(unsigned int reader_index, unsigned int *length,
	unsigned char *Buffer);

status_t CloseUSB(unsigned int reader_index);

#ifdef __USB_H__
int get_desc(int lun, usb_dev_handle **handle, struct usb_device **dev);

/*@null@*/ struct usb_interface *get_ccid_usb_interface(struct usb_device *dev);
#endif

