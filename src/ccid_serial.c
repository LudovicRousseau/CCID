/*
 * ccid_serial.c: communicate with a GemPC Twin smart card reader
 * Copyright (C) 2001-2025 Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * Thanks to Niki W. Waibel <niki.waibel@gmx.net> for a prototype version
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ifdhandler.h>

#include <config.h>
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "debug.h"
#include "ccid.h"
#include "utils.h"
#include "commands.h"
#include "parser.h"
#include "strlcpycat.h"

#define SYNC 0x03
#define CTRL_ACK 0x06
#define CTRL_NAK 0x15
#define CARD_ABSENT 0x02
#define CARD_PRESENT 0x03

/*
 * normal command:
 * 1 : SYNC
 * 1 : CTRL
 * 10 +data length : CCID command
 * 1 : LRC
 *
 * SYNC : 0x03
 * CTRL : ACK (0x06) or NAK (0x15)
 * CCID command : see USB CCID specs
 * LRC : xor of all the previous byes
 *
 * Error message:
 * 1 : SYNC (0x03)
 * 1 : CTRL (NAK: 0x15)
 * 1 : LRC (0x16)
 *
 * Card insertion/withdrawal
 * 1 : RDR_to_PC_NotifySlotChange (0x50)
 * 1 : bmSlotIccState
 *     0x02 if card absent
 *     0x03 is card present
 *
 * Time request
 * T=1 : normal CCID command
 * T=0 : 1 byte (value between 0x80 and 0xFF)
 *
 */

/*
 * You may get read timeout after a card movement.
 * This is because you will get the echo of the CCID command
 * but not the result of the command.
 *
 * This is not an applicative issue since the card is either removed (and
 * powered off) or just inserted (and not yet powered on).
 */

/* The _serialDevice structure must be defined before including ccid_serial.h */
#include "ccid_serial.h"

/* data rates supported by the GemPC Twin (serial and PCMCIA) */
static unsigned int SerialTwinDataRates[] = { ISO_DATA_RATES, 0 };

/* data rates supported by the GemPC PinPad, GemCore Pos Pro & SIM Pro */
static unsigned int SerialExtendedDataRates[] = { ISO_DATA_RATES, 500000, 0 };

/* data rates supported by the secondary slots on the GemCore Pos Pro & SIM Pro */
static unsigned int SerialCustomDataRates[] = { GEMPLUS_CUSTOM_DATA_RATES, 0 };

/* data rates supported by the GemCore SIM Pro 2 */
static unsigned int SIMPro2DataRates[] = { SIMPRO2_ISO_DATA_RATES, 0  };

extern CcidDesc CcidSlots[];

/* unexported functions */
static int ReadChunk(CcidDesc * ccid_reader, unsigned char *buffer,
	int buffer_length, int min_length);

static int get_bytes(CcidDesc * ccid_reader, /*@out@*/ unsigned char *buffer,
	int length);


/*****************************************************************************
 *
 *				WriteSerial: Send bytes to the card reader
 *
 *****************************************************************************/
status_t WriteSerial(CcidDesc * ccid_reader, unsigned int length,
	unsigned char *buffer)
{
	unsigned int i;
	unsigned char lrc;
	unsigned char low_level_buffer[GEMPCTWIN_MAXBUF];

	char debug_header[] = "-> lun: 12345678, ";

	(void)snprintf(debug_header, sizeof(debug_header), "-> lun: %X, ",
		ccid_reader->lun);

	if (length > GEMPCTWIN_MAXBUF-3)
	{
		DEBUG_CRITICAL3("command too long: %d for max %d",
			length, GEMPCTWIN_MAXBUF-3);
		return STATUS_UNSUCCESSFUL;
	}

	/* header */
	low_level_buffer[0] = 0x03;	/* SYNC */
	low_level_buffer[1] = 0x06;	/* ACK */

	/* CCID command */
	memcpy(low_level_buffer+2, buffer, length);

	/* checksum */
	lrc = 0;
	for(i=0; i<length+2; i++)
		lrc ^= low_level_buffer[i];
	low_level_buffer[length+2] = lrc;

	DEBUG_XXD(debug_header, low_level_buffer, length+3);

	if (write(ccid_reader->device.fd, low_level_buffer,
		length+3) != length+3)
	{
		DEBUG_CRITICAL2("write error: %s", strerror(errno));
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
} /* WriteSerial */


/*****************************************************************************
 *
 *				ReadSerial: Receive bytes from the card reader
 *
 *****************************************************************************/
status_t ReadSerial(CcidDesc * ccid_reader,
	unsigned int *length, unsigned char *buffer, int bSeq)
{
	unsigned char c;
	int rv;
	int echo;
	int to_read;
	int i;

	/* ignore bSeq */
	(void)bSeq;

	/* we get the echo first */
	echo = ccid_reader->device.echo;

start:
	DEBUG_COMM("start");
	if ((rv = get_bytes(ccid_reader, &c, 1)) != STATUS_SUCCESS)
		return rv;

	if (c == RDR_to_PC_NotifySlotChange)
		goto slot_change;

	if (c == SYNC)
		goto sync;

	if (c >= 0x80)
	{
		DEBUG_COMM2("time request: 0x%02X", c);
		goto start;
	}

	DEBUG_CRITICAL2("Got 0x%02X", c);
	return STATUS_COMM_ERROR;

slot_change:
	DEBUG_COMM("slot change");
	if ((rv = get_bytes(ccid_reader, &c, 1)) != STATUS_SUCCESS)
		return rv;

	if (c == CARD_ABSENT)
	{
		DEBUG_COMM("Card removed");
	}
	else
		if (c == CARD_PRESENT)
		{
			DEBUG_COMM("Card inserted");
		}
		else
		{
			DEBUG_COMM2("Unknown card movement: %d", c);
		}
	goto start;

sync:
	DEBUG_COMM("sync");
	if ((rv = get_bytes(ccid_reader, &c, 1)) != STATUS_SUCCESS)
		return rv;

	if (c == CTRL_ACK)
		goto ack;

	if (c == CTRL_NAK)
		goto nak;

	DEBUG_CRITICAL2("Got 0x%02X instead of ACK/NAK", c);
	return STATUS_COMM_ERROR;

nak:
	DEBUG_COMM("nak");
	if ((rv = get_bytes(ccid_reader, &c, 1)) != STATUS_SUCCESS)
		return rv;

	if (c != (SYNC ^ CTRL_NAK))
	{
		DEBUG_CRITICAL2("Wrong LRC: 0x%02X", c);
		return STATUS_COMM_ERROR;
	}
	else
	{
		DEBUG_COMM("NAK requested");
		return STATUS_COMM_NAK;
	}

ack:
	DEBUG_COMM("ack");
	/* normal CCID frame */
	if ((rv = get_bytes(ccid_reader, buffer, 5)) != STATUS_SUCCESS)
		return rv;

	/* total frame size */
	to_read = 10+dw2i(buffer, 1);

	if ((to_read < 10) || (to_read > (int)*length))
	{
		DEBUG_CRITICAL2("Wrong value for frame size: %d", to_read);
		return STATUS_COMM_ERROR;
	}

	DEBUG_COMM2("frame size: %d", to_read);
	if ((rv = get_bytes(ccid_reader, buffer+5, to_read-5)) != STATUS_SUCCESS)
		return rv;

	DEBUG_XXD("frame: ", buffer, to_read);

	/* lrc */
	DEBUG_COMM("lrc");
	if ((rv = get_bytes(ccid_reader, &c, 1)) != STATUS_SUCCESS)
		return rv;

	DEBUG_COMM2("lrc: 0x%02X", c);
	for (i=0; i<to_read; i++)
		c ^= buffer[i];

	if (c != (SYNC ^ CTRL_ACK))
		DEBUG_CRITICAL2("Wrong LRC: 0x%02X", c);

	if (echo)
	{
		echo = false;
		goto start;
	}

	/* length of data read */
	*length = to_read;

	return STATUS_SUCCESS;
} /* ReadSerial */


/*****************************************************************************
 *
 *				get_bytes: get n bytes
 *
 *****************************************************************************/
int get_bytes(CcidDesc * ccid_reader, unsigned char *buffer, int length)
{
	int offset = ccid_reader->device.buffer_offset;
	int offset_last = ccid_reader->device.buffer_offset_last;

	DEBUG_COMM3("available: %d, needed: %d", offset_last-offset,
		length);
	/* enough data are available */
	if (offset + length <= offset_last)
	{
		DEBUG_COMM("data available");
		memcpy(buffer, ccid_reader->device.buffer + offset, length);
		ccid_reader->device.buffer_offset += length;
	}
	else
	{
		int present, rv;

		/* copy available data */
		present = offset_last - offset;

		if (present > 0)
		{
			DEBUG_COMM2("some data available: %d", present);
			memcpy(buffer, ccid_reader->device.buffer + offset,
				present);
		}

		/* get fresh data */
		DEBUG_COMM2("get more data: %d", length - present);
		rv = ReadChunk(ccid_reader, ccid_reader->device.buffer,
			sizeof(ccid_reader->device.buffer), length - present);
		if (rv < 0)
		{
			ccid_reader->device.buffer_offset = 0;
			ccid_reader->device.buffer_offset_last = 0;
			return STATUS_COMM_ERROR;
		}

		/* fill the buffer */
		memcpy(buffer + present, ccid_reader->device.buffer,
			length - present);
		ccid_reader->device.buffer_offset = length - present;
		ccid_reader->device.buffer_offset_last = rv;
		DEBUG_COMM3("offset: %d, last_offset: %d",
			ccid_reader->device.buffer_offset,
			ccid_reader->device.buffer_offset_last);
	}

	return STATUS_SUCCESS;
} /* get_bytes */


/*****************************************************************************
 *
 *				ReadChunk: read a minimum number of bytes
 *
 *****************************************************************************/
static int ReadChunk(CcidDesc * ccid_reader, unsigned char *buffer,
	int buffer_length, int min_length)
{
	int fd = ccid_reader->device.fd;
# ifndef S_SPLINT_S
	fd_set fdset;
# endif
	struct timeval t;
	int i, rv = 0;
	int already_read;
	char debug_header[] = "<- lun: 12345678, ";

	(void)snprintf(debug_header, sizeof(debug_header), "<- lun: %X, ",
		ccid_reader->lun);

	already_read = 0;
	while (already_read < min_length)
	{
		/* use select() to, eventually, timeout */
		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);
		t.tv_sec = ccid_reader->device.ccid.readTimeout / 1000;
		t.tv_usec = (ccid_reader->device.ccid.readTimeout - t.tv_sec*1000)*1000;

		i = select(fd+1, &fdset, NULL, NULL, &t);
		if (i == -1)
		{
			DEBUG_CRITICAL2("select: %s", strerror(errno));
			return -1;
		}
		else
			if (i == 0)
			{
				DEBUG_COMM2("Timeout! (%d ms)", ccid_reader->device.ccid.readTimeout);
				return -1;
			}

		rv = read(fd, buffer + already_read, buffer_length - already_read);
		if (rv < 0)
		{
			DEBUG_COMM2("read error: %s", strerror(errno));
			return -1;
		}

		DEBUG_XXD(debug_header, buffer + already_read, rv);

		already_read += rv;
		DEBUG_COMM3("read: %d, to read: %d", already_read,
			min_length);
	}

	return already_read;
} /* ReadChunk */


/*****************************************************************************
 *
 *				OpenSerial: open the port
 *
 *****************************************************************************/
status_t OpenSerial(CcidDesc * ccid_reader, int channel)
{
	char dev_name[FILENAME_MAX];

	DEBUG_COMM3("Reader lun: %X, Channel: %d", ccid_reader->lun, channel);

	/*
	 * Conversion of old-style ifd-hanler 1.0 CHANNELID
	 */
	if (channel == 0x0103F8)
		channel = 1;
	else
		if (channel == 0x0102F8)
			channel = 2;
		else
			if (channel == 0x0103E8)
				channel = 3;
			else
				if (channel == 0x0102E8)
					channel = 4;

	if (channel < 0)
	{
		DEBUG_CRITICAL2("wrong port number: %d", channel);
		return STATUS_UNSUCCESSFUL;
	}

	(void)snprintf(dev_name, sizeof(dev_name), "/dev/pcsc/%d", channel);

	return OpenSerialByName(ccid_reader, dev_name);
} /* OpenSerial */

/*****************************************************************************
 *
 *				set_ccid_descriptor: init ccid descriptor
 *				depending on reader type specified in device.
 *
 *				return: STATUS_UNSUCCESSFUL,
 *						STATUS_SUCCESS,
 *						-1 (Reader already used)
 *
 *****************************************************************************/
static status_t set_ccid_descriptor(CcidDesc * ccid_reader,
	const char *reader_name, const char *dev_name)
{
	int readerID;
	int i;
	bool already_used = false;
	CcidDesc * previous_ccid_reader = NULL;

	readerID = GEMPCTWIN;
	if (0 == strcasecmp(reader_name,"GemCorePOSPro"))
		readerID = GEMCOREPOSPRO;
	else if (0 == strcasecmp(reader_name,"GemCoreSIMPro"))
		readerID = GEMCORESIMPRO;
	else if (0 == strcasecmp(reader_name,"GemCoreSIMPro2"))
		readerID = GEMCORESIMPRO2;
	else if (0 == strcasecmp(reader_name,"GemPCPinPad"))
		readerID = GEMPCPINPAD;
	/* SEC1210 has been in CCID driver for some time and assumed SEC1210UR2,
	so check for both the previously working reader name to maintain legacy behavior
	 as well as check for the more explicit device name that specifies a varient. */
	else if ((0 == strcasecmp(reader_name,"SEC1210")) ||
			(0 == strcasecmp(reader_name,"SEC1210UR2"))) // Dual slot varient of SEC1210
		readerID = SEC1210;
	else if (0 == strcasecmp(reader_name,"SEC1210URT")) // Single slot varient of SEC1210
		readerID = SEC1210URT;

	/* check if the same channel is not already used to manage multi-slots readers*/
	for (i = 0; i < CCID_DRIVER_MAX_READERS; i++)
	{
		if (CcidSlots[i].device.device
			&& strcmp(CcidSlots[i].device.device, dev_name) == 0)
		{
			already_used = true;

			DEBUG_COMM2("%s already used. Multi-slot reader?", dev_name);
			break;
		}
	}

	/* this reader is already managed by us */
	if (already_used)
	{
		if ((previous_ccid_reader != NULL)
			&& previous_ccid_reader->device.device
			&& (strcmp(previous_ccid_reader->device.device, dev_name) == 0)
			&& previous_ccid_reader->device.ccid.bCurrentSlotIndex < previous_ccid_reader->device.ccid.bMaxSlotIndex)
		{
			/* we reuse the same device and the reader is multi-slot */
			ccid_reader->device = previous_ccid_reader->device;

			*ccid_reader->device.nb_opened_slots += 1;
			ccid_reader->device.ccid.bCurrentSlotIndex++;
			ccid_reader->device.ccid.dwSlotStatus = IFD_ICC_PRESENT;
			DEBUG_INFO2("Opening slot: %d",
					ccid_reader->device.ccid.bCurrentSlotIndex);
			switch (readerID)
			{
				case GEMCOREPOSPRO:
				case GEMCORESIMPRO:
					{
						/* Allocate a memory buffer that will be
						 * released in CloseUSB() */
						void *ptr = malloc(sizeof SerialCustomDataRates);
						if (ptr)
						{
							memcpy(ptr, SerialCustomDataRates,
									sizeof SerialCustomDataRates);
						}

						ccid_reader->device.ccid.arrayOfSupportedDataRates = ptr;
					}
					ccid_reader->device.ccid.dwMaxDataRate = 125000;
					break;

				case SEC1210:
				case SEC1210URT:
					ccid_reader->device.ccid.arrayOfSupportedDataRates = NULL;
					ccid_reader->device.ccid.dwMaxDataRate = 826000;
					break;

				/* GemPC Twin or GemPC Card */
				default:
					ccid_reader->device.ccid.arrayOfSupportedDataRates = SerialTwinDataRates;
					ccid_reader->device.ccid.dwMaxDataRate = 344086;
					break;
			}
			goto end;
		}
		else
		{
			DEBUG_CRITICAL2("Trying to open too many slots on %s", dev_name);
			return STATUS_UNSUCCESSFUL;
		}

	}

	/* Common to all readers */
	ccid_reader->device.ccid.real_bSeq = 0;
	ccid_reader->device.ccid.pbSeq = &ccid_reader->device.ccid.real_bSeq;
	ccid_reader->device.real_nb_opened_slots = 1;
	ccid_reader->device.nb_opened_slots = &ccid_reader->device.real_nb_opened_slots;
	ccid_reader->device.ccid.bCurrentSlotIndex = 0;

	ccid_reader->device.ccid.dwMaxCCIDMessageLength = 271;
	ccid_reader->device.ccid.dwMaxIFSD = 254;
	ccid_reader->device.ccid.dwFeatures = 0x00010230;
	ccid_reader->device.ccid.dwDefaultClock = 4000;

	ccid_reader->device.buffer_offset = 0;
	ccid_reader->device.buffer_offset_last = 0;

	ccid_reader->device.ccid.readerID = readerID;
	ccid_reader->device.ccid.bPINSupport = 0x0;
	ccid_reader->device.ccid.dwMaxDataRate = 344086;
	ccid_reader->device.ccid.bMaxSlotIndex = 0;
	ccid_reader->device.ccid.bMaxCCIDBusySlots = 1;
	ccid_reader->device.ccid.arrayOfSupportedDataRates = SerialTwinDataRates;
	ccid_reader->device.ccid.readTimeout = DEFAULT_COM_READ_TIMEOUT;
	ccid_reader->device.ccid.dwSlotStatus = IFD_ICC_PRESENT;
	ccid_reader->device.ccid.bVoltageSupport = 0x07;	/* 1.8V, 3V and 5V */
	ccid_reader->device.ccid.gemalto_firmware_features = NULL;
	ccid_reader->device.ccid.dwProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
#ifdef ENABLE_ZLP
	ccid_reader->device.ccid.zlp = false;
#endif
	ccid_reader->device.echo = true;

	/* change some values depending on the reader */
	switch (readerID)
	{
		case GEMCOREPOSPRO:
			ccid_reader->device.ccid.bMaxSlotIndex = 4;	/* 5 slots */
			ccid_reader->device.ccid.arrayOfSupportedDataRates = SerialExtendedDataRates;
			ccid_reader->device.echo = false;
			ccid_reader->device.ccid.dwMaxDataRate = 500000;
			break;

		case GEMCORESIMPRO:
			ccid_reader->device.ccid.bMaxSlotIndex = 1; /* 2 slots */
			ccid_reader->device.ccid.arrayOfSupportedDataRates = SerialExtendedDataRates;
			ccid_reader->device.echo = false;
			ccid_reader->device.ccid.dwMaxDataRate = 500000;
			break;

		case GEMCORESIMPRO2:
			ccid_reader->device.ccid.dwDefaultClock = 4800;
			ccid_reader->device.ccid.bMaxSlotIndex = 1; /* 2 slots */
			ccid_reader->device.ccid.arrayOfSupportedDataRates = SIMPro2DataRates;
			ccid_reader->device.echo = false;
			ccid_reader->device.ccid.dwMaxDataRate = 825806;
			break;

		case GEMPCPINPAD:
			ccid_reader->device.ccid.bPINSupport = 0x03;
			ccid_reader->device.ccid.arrayOfSupportedDataRates = SerialExtendedDataRates;
			ccid_reader->device.ccid.dwMaxDataRate = 500000;
			break;

		/* Most settings are shared between both SEC1210 varients */
		case SEC1210:
		case SEC1210URT:
			ccid_reader->device.ccid.dwFeatures = 0x000100B2;
			ccid_reader->device.ccid.dwDefaultClock = 4800;
			ccid_reader->device.ccid.dwMaxDataRate = 826000;
			ccid_reader->device.ccid.arrayOfSupportedDataRates = NULL;
			ccid_reader->device.echo = false;
			if (SEC1210URT == readerID)
			{
				ccid_reader->device.ccid.bMaxSlotIndex = 0;	/* SEC1210URT Varient has 1 slot */
			}
			else
			{
				ccid_reader->device.ccid.bMaxSlotIndex = 1;	/* SEC1210UR2 Varient has 2 slots */
			}
			break;

	}

end:
	/* memorise the current reader_index so we can detect
	 * a new OpenSerialByName on a multi slot reader */
	previous_ccid_reader = ccid_reader;

	/* we just created a secondary slot on a multi-slot reader */
	if (already_used)
		return STATUS_SECONDARY_SLOT;

	return STATUS_SUCCESS;
} /* set_ccid_descriptor  */


/*****************************************************************************
 *
 *				OpenSerialByName: open the port
 *
 *****************************************************************************/
status_t OpenSerialByName(CcidDesc * ccid_reader, char *dev_name)
{
	struct termios current_termios;
	/* 255 is MAX_DEVICENAME in pcscd.h */
	char reader_name[255] = "GemPCTwin";
	char *p;
	status_t ret;
	int readerID;

	DEBUG_COMM3("Reader lun: %X, Device: %s", ccid_reader->lun, dev_name);

	/* parse dev_name using the pattern "device:name" */
	p = strchr(dev_name, ':');
	if (p)
	{
		/* copy the second part of the string */
		strlcpy(reader_name, p+1, sizeof(reader_name));

		/* replace ':' by '\0' so that dev_name only contains the device name */
		*p = '\0';
	}

	ret = set_ccid_descriptor(ccid_reader, reader_name, dev_name);
	if (STATUS_UNSUCCESSFUL == ret)
		return STATUS_UNSUCCESSFUL;

	/* secondary slot so do not physically open the device */
	if (STATUS_SECONDARY_SLOT == ret)
		return STATUS_SUCCESS;

	ccid_reader->device.fd = open(dev_name, O_RDWR | O_NOCTTY);

	if (-1 == ccid_reader->device.fd)
	{
		DEBUG_CRITICAL3("open %s: %s", dev_name, strerror(errno));
		return STATUS_UNSUCCESSFUL;
	}

	/* Set RTS signal to low to prevent the smart card reader
	 * from sending its plug and play string. */
	{
		int flags;

		if (ioctl(ccid_reader->device.fd, TIOCMGET, &flags) < 0)
		{
			DEBUG_CRITICAL2("Get RS232 signals state failed: %s",
				strerror(errno));
		}
		else
		{
			flags &= ~TIOCM_RTS;
			if (ioctl(ccid_reader->device.fd, TIOCMSET, &flags) < 0)
			{
				DEBUG_CRITICAL2("Set RTS to low failed: %s", strerror(errno));
			}
			else
			{
				DEBUG_COMM("Plug-n-Play inhibition successful");
			}
		}
	}

	/* set channel used */
	ccid_reader->device.device = strdup(dev_name);

	/* empty in and out serial buffers */
	if (tcflush(ccid_reader->device.fd, TCIOFLUSH))
			DEBUG_INFO2("tcflush() function error: %s", strerror(errno));

	/* get config attributes */
	if (tcgetattr(ccid_reader->device.fd, &current_termios) == -1)
	{
		DEBUG_INFO2("tcgetattr() function error: %s", strerror(errno));
		(void)close(ccid_reader->device.fd);
		ccid_reader->device.fd = -1;

		return STATUS_UNSUCCESSFUL;
	}

	/* IGNBRK: ignore BREAK condition on input
	 * IGNPAR: ignore framing errors and parity errors. */
	current_termios.c_iflag = IGNBRK | IGNPAR;
	current_termios.c_oflag = 0;	/* Raw output modes */
	/* CS8: 8-bits character size
	 * CSTOPB: set two stop bits
	 * CREAD: enable receiver
	 * CLOCAL: ignore modem control lines */
	current_termios.c_cflag = CS8 | CSTOPB | CREAD | CLOCAL;

	/* Do not echo characters because if you connect to a host it or your modem
	 * will echo characters for you.  Don't generate signals. */
	current_termios.c_lflag = 0;

	readerID = ccid_reader->device.ccid.readerID;
	if (GEMCORESIMPRO2 == readerID)
	{
		unsigned char pcbuffer[SIZE_GET_SLOT_STATUS];
		unsigned int old_timeout;
		RESPONSECODE r;

		/* Unless we resume from a stand-by condition, GemCoreSIMPro2
		 * starts at 9600 bauds, so let's first try this speed */
		/* set serial port speed to 9600 bauds */
		(void)cfsetspeed(&current_termios, B9600);
		DEBUG_INFO1("Set serial port baudrate to 9600 and correct configuration");
		if (tcsetattr(ccid_reader->device.fd, TCSANOW, &current_termios) == -1)
		{
			(void)close(ccid_reader->device.fd);
			ccid_reader->device.fd = -1;
			DEBUG_CRITICAL2("tcsetattr error: %s", strerror(errno));

			return STATUS_UNSUCCESSFUL;
		}

		/* Test current speed issuing a CmdGetSlotStatus with a very
		 * short time out of 1 seconds */
		old_timeout = ccid_reader->device.ccid.readTimeout;

		ccid_reader->device.ccid.readTimeout = 1*1000;
		r = CmdGetSlotStatus(ccid_reader, pcbuffer);

		/* Restore default time out value */
		ccid_reader->device.ccid.readTimeout = old_timeout;

		if (IFD_SUCCESS == r)
		{
			/* We are at 9600 bauds, let's move to 115200 */
			unsigned char tx_buffer[] = { 0x01, 0x10, 0x20 };
			unsigned char rx_buffer[50];
			unsigned int rx_length = sizeof(rx_buffer);

			if (IFD_SUCCESS == CmdEscape(ccid_reader, tx_buffer,
				sizeof(tx_buffer), rx_buffer, &rx_length, 0))
			{
				/* Let the reader setup its new communication speed */
				(void)usleep(250*1000);
			}
			else
			{
				DEBUG_INFO1("CmdEscape to configure 115200 bauds failed");
			}
		}
		/* In case of a failure, reader is probably already at 115200
		 * bauds as code below assumes */
	}

	/* set serial port speed to 115200 bauds */
	(void)cfsetspeed(&current_termios, B115200);

	DEBUG_INFO1("Set serial port baudrate to 115200 and correct configuration");
	if (tcsetattr(ccid_reader->device.fd, TCSANOW, &current_termios) == -1)
	{
		(void)close(ccid_reader->device.fd);
		ccid_reader->device.fd = -1;
		DEBUG_INFO2("tcsetattr error: %s", strerror(errno));

		return STATUS_UNSUCCESSFUL;
	}

	/* perform a command to be sure a Gemalto reader is connected
	 * get the reader firmware */
	{
		unsigned char tx_buffer[1];
		unsigned char rx_buffer[50];
		unsigned int rx_length = sizeof(rx_buffer);


		if ((SEC1210 == readerID) || (SEC1210URT == readerID))
			tx_buffer[0] = 0x06; // unknown but supported command
		else
			tx_buffer[0] = 0x02; // get reader firmware

		/* 2 seconds timeout to not wait too long if no reader is connected */
		if (IFD_SUCCESS != CmdEscape(ccid_reader, tx_buffer, sizeof(tx_buffer),
			rx_buffer, &rx_length, 2*1000))
		{
			DEBUG_CRITICAL("Get firmware failed. Maybe the reader is not connected");
			(void)CloseSerial(ccid_reader);
			return STATUS_UNSUCCESSFUL;
		}

		rx_buffer[rx_length] = '\0';
		DEBUG_INFO2("Firmware: %s", rx_buffer);
	}

	/* perform a command to configure GemPC Twin reader card movement
	 * notification to synchronous mode: the card movement is notified _after_
	 * the host command and _before_ the reader answer */

	if ((SEC1210 != readerID) && (SEC1210URT != readerID))
	{
		unsigned char tx_buffer[] = { 0x01, 0x01, 0x01};
		unsigned char rx_buffer[50];
		unsigned int rx_length = sizeof(rx_buffer);

		if (IFD_SUCCESS != CmdEscape(ccid_reader, tx_buffer, sizeof(tx_buffer),
			rx_buffer, &rx_length, 0))
		{
			DEBUG_CRITICAL("Change card movement notification failed.");
			(void)CloseSerial(ccid_reader);
			return STATUS_UNSUCCESSFUL;
		}
	}

	ccid_reader->device.ccid.sIFD_serial_number = NULL;
	ccid_reader->device.ccid.sIFD_iManufacturer = NULL;
	ccid_reader->device.ccid.IFD_bcdDevice = 0;

	return STATUS_SUCCESS;
} /* OpenSerialByName */


/*****************************************************************************
 *
 *				CloseSerial: close the port
 *
 *****************************************************************************/
status_t CloseSerial(CcidDesc * ccid_reader)
{
	/* device not opened */
	if (NULL == ccid_reader->device.device)
		return STATUS_UNSUCCESSFUL;

	DEBUG_COMM2("Closing serial device: %s", ccid_reader->device.device);

	/* Decrement number of opened slot */
	(*ccid_reader->device.nb_opened_slots)--;

	/* release the allocated resources for the last slot only */
	if (0 == *ccid_reader->device.nb_opened_slots)
	{
		DEBUG_COMM("Last slot closed. Release resources");

		(void)close(ccid_reader->device.fd);
		ccid_reader->device.fd = -1;

		free(ccid_reader->device.device);
		ccid_reader->device.device = NULL;
	}

	return STATUS_SUCCESS;
} /* CloseSerial */


/*****************************************************************************
 *
 *					DisconnectSerial
 *
 ****************************************************************************/
status_t DisconnectSerial(CcidDesc * ccid_reader)
{
	(void)ccid_reader;

	DEBUG_COMM("Disconnect reader");

	return STATUS_UNSUCCESSFUL;
} /* DisconnectSerial */

