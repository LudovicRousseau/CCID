/*
    ccid_serial.h:  Serial access routines
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

#ifndef __CCID_SERAL_H__
#define __CCID_SERAL_H__

status_t OpenSerial(CcidDesc * ccid_reader, int channel);

status_t OpenSerialByName(CcidDesc * ccid_reader, char *dev_name);

status_t WriteSerial(CcidDesc * ccid_reader, unsigned int length,
	unsigned char *Buffer);

status_t ReadSerial(CcidDesc * ccid_reader, unsigned int *length,
	unsigned char *Buffer, int bSeq);

status_t CloseSerial(CcidDesc * ccid_reader);

status_t DisconnectSerial(CcidDesc * ccid_reader);

#endif
