/*
    defs.h:
    Copyright (C) 2003   Ludovic Rousseau

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

/*
 * $Id$
 */

#include <pcsclite.h>

#include "openct/proto-t1.h"

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
	t1_state_t t1;
} CcidDesc;

typedef enum {
	STATUS_NO_SUCH_DEVICE        = 0xF9,
	STATUS_SUCCESS               = 0xFA,
	STATUS_UNSUCCESSFUL          = 0xFB,
	STATUS_COMM_ERROR            = 0xFC,
	STATUS_DEVICE_PROTOCOL_ERROR = 0xFD,
	STATUS_COMM_NAK              = 0xFE,
	STATUS_SECONDARY_SLOT        = 0xFF
} status_t;

/* Powerflag (used to detect quick insertion removals unnoticed by the
 * resource manager) */
/* Initial value */
#define POWERFLAGS_RAZ 0x00
/* Flag set when a power up has been requested */
#define MASK_POWERFLAGS_PUP 0x01
/* Flag set when a power down is requested */
#define MASK_POWERFLAGS_PDWN 0x02

/* Kobil readers does not support APDU chaining for T=1 so you can't send an
 * extended APDU. The readers supports a command of up to 512 or 420 bytes.
 * For a Kobil KAAN Base/advanced reader you should use
 *  #define CMD_BUF_SIZE 420
 * For the other models you should use
 *  #define CMD_BUF_SIZE 512
 * Kobil is aware of the problem and do not plan to solve it.
 */
/* Communication buffer size (max=adpu+Lc+data+Le) */
#define CMD_BUF_SIZE (4+1+256+1)
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

/* Default communication read timeout in seconds */
#define DEFAULT_COM_READ_TIMEOUT 2

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

