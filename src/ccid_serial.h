/*
    ccid_serial.h:  Serial access routines
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

status_t OpenSerial(unsigned int reader_index, int channel);

status_t OpenSerialByName(unsigned int reader_index, char *dev_name);

status_t WriteSerial(unsigned int reader_index, unsigned int length,
	unsigned char *Buffer);

status_t ReadSerial(unsigned int reader_index, unsigned int *length,
	unsigned char *Buffer);

status_t CloseSerial(unsigned int reader_index);

