/*
    defs.h:
    Copyright (C) 2003-2010   Ludovic Rousseau

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

#define max( a, b )   ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#include <pcsclite.h>

#include "openct/proto-t1.h"

typedef struct CCID_DESC
{
	/*
	 * ATR
	 */
	int nATRLength;
	unsigned char pcATRBuffer[MAX_ATR_SIZE];

	/*
	 * Card state
	 */
	unsigned char bPowerFlags;

	/*
	 * T=1 Protocol context
	 */
	t1_state_t t1;

	/* reader name passed to IFDHCreateChannelByName() */
	char *readerName;
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

/* Communication buffer size (max=adpu+Lc+data+Le)
 * we use a 64kB for extended APDU on APDU mode readers */
#define CMD_BUF_SIZE (4 +3 +64*1024 +3)

/* Protocols */
#define T_0 0
#define T_1 1

/* Default communication read timeout in milliseconds */
#define DEFAULT_COM_READ_TIMEOUT (3*1000)

/* DWORD type formating */
#ifdef __APPLE__
/* Apple defines DWORD as uint32_t */
#define DWORD_X "%X"
#define DWORD_D "%d"
#else
/* pcsc-lite defines DWORD as unsigned long */
#define DWORD_X "%lX"
#define DWORD_D "%ld"
#endif

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

