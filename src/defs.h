/*
    defs.h:
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

#include "pcscdefines.h"
#include "protocol_t1/protocol_t1.h"

typedef struct CCID_DESC
{
	/*
	 * ATR
	 */
	DWORD nATRLength;
	UCHAR pcATRBuffer[MAX_ATR_SIZE];

	/*
	 * Card state
	 */
	UCHAR bPowerFlags;

	/*
	 * T=1 Protocol context
	 */
	Protocol_T1 t1;
} CcidDesc;

/* Powerflag (used to detect quick insertion removals unnoticed by the 
 * resource manager)Initial value */
#define POWERFLAGS_RAZ 0x00
/* Flag set when a power up has been requested */
#define MASK_POWERFLAGS_PUP 0x01
/* Flag set when a power down is requestedA */
#define MASK_POWERFLAGS_PDWN 0x02

/* Communication buffer size (max=cmd+adpu+Lc+data+Le) */
#define CMD_BUF_SIZE (1+4+1+256+1)
/* Larger communication buffer size (max=reader status+data+sw) */
#define RESP_BUF_SIZE (1+256+2)

/* Protocols */
#define T_0 0
#define T_1 1

/* Size of an ISO command (CLA+INS+P1+P2) */
#define ISO_CMD_SIZE 4
/* Offset of the length byte in an TPDU */
#define ISO_OFFSET_LENGTH 4
/* Offset of the data in a TPDU */
#define ISO_OFFSET_TPDU_DATA 5
/* ISO length size (1 in general) */
#define ISO_LENGTH_SIZE 1


/*
 * communication ports abstraction
 */
#ifdef TWIN_SERIAL

#define OpenPortByName OpenSerialByName
#define OpenPort OpenSerial
#define ClosePort CloseSerial
#define ReadPort ReadSerial
#define WritePort WriteSerial
#include "ccid_serial.h"

#else

#define OpenPortByName OpenUSBByName
#define OpenPort OpenUSB
#define ClosePort CloseUSB
#define ReadPort ReadUSB
#define WritePort WriteUSB
#include "ccid_usb.h"

#endif

