/*
    ifdhandler.c: IFDH API
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

#include <config.h>

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "misc.h"
#include <pcsclite.h>
#include <ifdhandler.h>
#include <reader.h>

#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "debug.h"
#include "utils.h"
#include "commands.h"
#include "towitoko/atr.h"
#include "towitoko/pps.h"
#include "parser.h"
#include "strlcpycat.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

/* Array of structures to hold the ATR and other state value of each slot */
static CcidDesc CcidSlots[CCID_DRIVER_MAX_READERS];

/* global mutex */
#ifdef HAVE_PTHREAD
static pthread_mutex_t ifdh_context_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int LogLevel = DEBUG_LEVEL_CRITICAL | DEBUG_LEVEL_INFO;
int DriverOptions = 0;
int PowerOnVoltage = -1;
static int DebugInitialized = FALSE;

/* local functions */
static void init_driver(void);
static void extra_egt(ATR_t *atr, _ccid_descriptor *ccid_desc, DWORD Protocol);
static char find_baud_rate(unsigned int baudrate, unsigned int *list);
static unsigned int T0_card_timeout(double f, double d, int TC1, int TC2,
	int clock_frequency);
static unsigned int T1_card_timeout(double f, double d, int TC1, int BWI,
	int CWI, int clock_frequency);
static int get_IFSC(ATR_t *atr, int *i);

static void FreeChannel(int reader_index)
{
#ifdef HAVE_PTHREAD
	(void)pthread_mutex_lock(&ifdh_context_mutex);
#endif

	(void)ClosePort(reader_index);

	free(CcidSlots[reader_index].readerName);
	memset(&CcidSlots[reader_index], 0, sizeof(CcidSlots[reader_index]));

	ReleaseReaderIndex(reader_index);

#ifdef HAVE_PTHREAD
	(void)pthread_mutex_unlock(&ifdh_context_mutex);
#endif
}

static RESPONSECODE CreateChannelByNameOrChannel(DWORD Lun,
	LPSTR lpcDevice, DWORD Channel)
{
	RESPONSECODE return_value = IFD_SUCCESS;
	int reader_index;
	status_t ret;

	if (! DebugInitialized)
		init_driver();

	if (lpcDevice)
	{
		DEBUG_INFO3("Lun: " DWORD_X ", device: %s", Lun, lpcDevice);
	}
	else
	{
		DEBUG_INFO3("Lun: " DWORD_X ", Channel: " DWORD_X, Lun, Channel);
	}

#ifdef HAVE_PTHREAD
	(void)pthread_mutex_lock(&ifdh_context_mutex);
#endif

	reader_index = GetNewReaderIndex(Lun);

#ifdef HAVE_PTHREAD
	(void)pthread_mutex_unlock(&ifdh_context_mutex);
#endif

	if (-1 == reader_index)
		return IFD_COMMUNICATION_ERROR;

	/* Reset ATR buffer */
	CcidSlots[reader_index].nATRLength = 0;
	*CcidSlots[reader_index].pcATRBuffer = '\0';

	/* Reset PowerFlags */
	CcidSlots[reader_index].bPowerFlags = POWERFLAGS_RAZ;

	/* reader name */
	if (lpcDevice)
		CcidSlots[reader_index].readerName = strdup(lpcDevice);
	else
		CcidSlots[reader_index].readerName = strdup("no name");

	if (lpcDevice)
		ret = OpenPortByName(reader_index, lpcDevice);
	else
		ret = OpenPort(reader_index, Channel);

	if (ret != STATUS_SUCCESS)
	{
		DEBUG_CRITICAL("failed");
		if (STATUS_NO_SUCH_DEVICE == ret)
			return_value = IFD_NO_SUCH_DEVICE;
		else
			return_value = IFD_COMMUNICATION_ERROR;

		goto error;
	}
	else
	{
		unsigned char pcbuffer[SIZE_GET_SLOT_STATUS];
		unsigned int oldReadTimeout;
		RESPONSECODE cmd_ret;
		_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

		/* Maybe we have a special treatment for this reader */
		(void)ccid_open_hack_pre(reader_index);

		/* Try to access the reader */
		/* This "warm up" sequence is sometimes needed when pcscd is
		 * restarted with the reader already connected. We get some
		 * "usb_bulk_read: Resource temporarily unavailable" on the first
		 * few tries. It is an empirical hack */

		/* The reader may have to start here so give it some time */
		cmd_ret = CmdGetSlotStatus(reader_index, pcbuffer);
		if (IFD_NO_SUCH_DEVICE == cmd_ret)
		{
			return_value = cmd_ret;
			goto error;
		}

		/* save the current read timeout computed from card capabilities */
		oldReadTimeout = ccid_descriptor->readTimeout;

		/* 100 ms just to resync the USB toggle bits */
		/* Do not use a fixed 100 ms value but compute it from the
		 * default timeout. It is now possible to use a different value
		 * by changing readTimeout in ccid_open_hack_pre() */
		ccid_descriptor->readTimeout = ccid_descriptor->readTimeout * 100.0 / DEFAULT_COM_READ_TIMEOUT;

		if ((IFD_COMMUNICATION_ERROR == CmdGetSlotStatus(reader_index, pcbuffer))
			&& (IFD_COMMUNICATION_ERROR == CmdGetSlotStatus(reader_index, pcbuffer)))
		{
			DEBUG_CRITICAL("failed");
			return_value = IFD_COMMUNICATION_ERROR;
		}
		else
		{
			/* Maybe we have a special treatment for this reader */
			return_value = ccid_open_hack_post(reader_index);
			if (return_value != IFD_SUCCESS)
			{
				DEBUG_CRITICAL("failed");
			}
		}

		/* set back the old timeout */
		ccid_descriptor->readTimeout = oldReadTimeout;
	}

error:
	if (return_value != IFD_SUCCESS)
	{
		/* release the allocated resources */
		FreeChannel(reader_index);
	}

	return return_value;
} /* CreateChannelByNameOrChannel */


EXTERNAL RESPONSECODE IFDHCreateChannelByName(DWORD Lun, LPSTR lpcDevice)
{
	return CreateChannelByNameOrChannel(Lun, lpcDevice, -1);
}

EXTERNAL RESPONSECODE IFDHCreateChannel(DWORD Lun, DWORD Channel)
{
	/*
	 * Lun - Logical Unit Number, use this for multiple card slots or
	 * multiple readers. 0xXXXXYYYY - XXXX multiple readers, YYYY multiple
	 * slots. The resource manager will set these automatically.  By
	 * default the resource manager loads a new instance of the driver so
	 * if your reader does not have more than one smartcard slot then
	 * ignore the Lun in all the functions. Future versions of PC/SC might
	 * support loading multiple readers through one instance of the driver
	 * in which XXXX would be important to implement if you want this.
	 */

	/*
	 * Channel - Channel ID.  This is denoted by the following: 0x000001 -
	 * /dev/pcsc/1 0x000002 - /dev/pcsc/2 0x000003 - /dev/pcsc/3
	 *
	 * USB readers may choose to ignore this parameter and query the bus
	 * for the particular reader.
	 */

	/*
	 * This function is required to open a communications channel to the
	 * port listed by Channel.  For example, the first serial reader on
	 * COM1 would link to /dev/pcsc/1 which would be a sym link to
	 * /dev/ttyS0 on some machines This is used to help with intermachine
	 * independance.
	 *
	 * Once the channel is opened the reader must be in a state in which
	 * it is possible to query IFDHICCPresence() for card status.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR
	 */
	return CreateChannelByNameOrChannel(Lun, NULL, Channel);
} /* IFDHCreateChannel */


EXTERNAL RESPONSECODE IFDHCloseChannel(DWORD Lun)
{
	/*
	 * This function should close the reader communication channel for the
	 * particular reader.  Prior to closing the communication channel the
	 * reader should make sure the card is powered down and the terminal
	 * is also powered down.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR
	 */
	int reader_index;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO3("%s (lun: " DWORD_X ")", CcidSlots[reader_index].readerName,
		Lun);

	/* Restore the default timeout
	 * No need to wait too long if the reader disapeared */
	get_ccid_descriptor(reader_index)->readTimeout = DEFAULT_COM_READ_TIMEOUT;

	(void)CmdPowerOff(reader_index);
	/* No reader status check, if it failed, what can you do ? :) */

	FreeChannel(reader_index);

	return IFD_SUCCESS;
} /* IFDHCloseChannel */


#if !defined(TWIN_SERIAL)
static RESPONSECODE IFDHPolling(DWORD Lun, int timeout)
{
	int reader_index;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	/* log only if DEBUG_LEVEL_PERIODIC is set */
	if (LogLevel & DEBUG_LEVEL_PERIODIC)
		DEBUG_INFO4("%s (lun: " DWORD_X ") %d ms",
			CcidSlots[reader_index].readerName, Lun, timeout);

	return InterruptRead(reader_index, timeout);
}

/* on an ICCD device the card is always inserted
 * so no card movement will ever happen: just do nothing */
static RESPONSECODE IFDHSleep(DWORD Lun, int timeout)
{
	int reader_index;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO4("%s (lun: " DWORD_X ") %d ms",
		CcidSlots[reader_index].readerName, Lun, timeout);

	/* just sleep for 5 seconds since the polling thread is NOT killable
	 * so pcscd event thread must loop to exit cleanly
	 *
	 * Once the driver (libusb in fact) will support
	 * TAG_IFD_POLLING_THREAD_KILLABLE then we could use a much longer delay
	 * and be killed before pcscd exits
	 */
	(void)usleep(timeout * 1000);
	return IFD_SUCCESS;
}

static RESPONSECODE IFDHStopPolling(DWORD Lun)
{
	int reader_index;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO3("%s (lun: " DWORD_X ")",
		CcidSlots[reader_index].readerName, Lun);

	(void)InterruptStop(reader_index);
	return IFD_SUCCESS;
}
#endif


EXTERNAL RESPONSECODE IFDHGetCapabilities(DWORD Lun, DWORD Tag,
	PDWORD Length, PUCHAR Value)
{
	/*
	 * This function should get the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information requested example: TAG_IFD_ATR -
	 * return the Atr and its size (required). these tags are defined in
	 * ifdhandler.h
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG
	 */
	int reader_index;
	RESPONSECODE return_value = IFD_SUCCESS;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO4("tag: 0x" DWORD_X ", %s (lun: " DWORD_X ")", Tag,
		CcidSlots[reader_index].readerName, Lun);

	switch (Tag)
	{
		case TAG_IFD_ATR:
		case SCARD_ATTR_ATR_STRING:
			/* If Length is not zero, powerICC has been performed.
			 * Otherwise, return NULL pointer
			 * Buffer size is stored in *Length */
			if ((int)*Length >= CcidSlots[reader_index].nATRLength)
			{
				*Length = CcidSlots[reader_index].nATRLength;

				memcpy(Value, CcidSlots[reader_index].pcATRBuffer, *Length);
			}
			else
				return_value = IFD_ERROR_INSUFFICIENT_BUFFER;
			break;

		case SCARD_ATTR_ICC_INTERFACE_STATUS:
			*Length = 1;
			if (IFD_ICC_PRESENT == IFDHICCPresence(Lun))
				/* nonzero if contact is active */
				*Value = 1;
			else
				/* smart card electrical contact is not active */
				*Value = 0;
			break;

		case SCARD_ATTR_ICC_PRESENCE:
			*Length = 1;
			/* Single byte indicating smart card presence:
			 * 0 = not present
			 * 1 = card present but not swallowed (applies only if
			 *     reader supports smart card swallowing)
			 * 2 = card present (and swallowed if reader supports smart
			 *     card swallowing)
			 * 4 = card confiscated. */
			if (IFD_ICC_PRESENT == IFDHICCPresence(Lun))
				/* Card present */
				*Value = 2;
			else
				/* Not present */
				*Value = 0;
			break;

#ifdef HAVE_PTHREAD
		case TAG_IFD_SIMULTANEOUS_ACCESS:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = CCID_DRIVER_MAX_READERS;
			}
			else
				return_value = IFD_ERROR_INSUFFICIENT_BUFFER;
			break;

		case TAG_IFD_THREAD_SAFE:
			if (*Length >= 1)
			{
				*Length = 1;
#ifdef __APPLE__
				*Value = 0; /* Apple pcscd is bogus (rdar://problem/5697388) */
#else
				*Value = 1; /* Can talk to multiple readers at the same time */
#endif
			}
			else
				return_value = IFD_ERROR_INSUFFICIENT_BUFFER;
			break;
#endif

		case TAG_IFD_SLOTS_NUMBER:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = 1 + get_ccid_descriptor(reader_index) -> bMaxSlotIndex;
#ifdef USE_COMPOSITE_AS_MULTISLOT
				{
					/* On MacOS X or Linux+libusb we can simulate a
					 * composite device with 2 CCID interfaces by a
					 * multi-slot reader */
					int readerID =  get_ccid_descriptor(reader_index) -> readerID;

					/* 2 CCID interfaces */
					if ((GEMALTOPROXDU == readerID)
						|| (GEMALTOPROXSU == readerID)
						|| (HID_OMNIKEY_5422 == readerID))
						*Value = 2;

					/* 3 CCID interfaces */
					if (FEITIANR502DUAL == readerID)
						*Value = 3;
				}
#endif
				DEBUG_INFO2("Reader supports %d slot(s)", *Value);
			}
			else
				return_value = IFD_ERROR_INSUFFICIENT_BUFFER;
			break;

		case TAG_IFD_SLOT_THREAD_SAFE:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = 0; /* Can NOT talk to multiple slots at the same time */
			}
			else
				return_value = IFD_ERROR_INSUFFICIENT_BUFFER;
			break;

		case SCARD_ATTR_VENDOR_IFD_VERSION:
			{
				int IFD_bcdDevice = get_ccid_descriptor(reader_index)->IFD_bcdDevice;

				/* Vendor-supplied interface device version (DWORD in the form
				 * 0xMMmmbbbb where MM = major version, mm = minor version, and
				 * bbbb = build number). */
				*Length = 4;
				if (Value)
					*(uint32_t *)Value = IFD_bcdDevice << 16;
			}
			break;

		case SCARD_ATTR_VENDOR_NAME:
			{
				const char *sIFD_iManufacturer = get_ccid_descriptor(reader_index) -> sIFD_iManufacturer;

				if (sIFD_iManufacturer)
				{
					strlcpy((char *)Value, sIFD_iManufacturer, *Length);
					*Length = strlen((char *)Value) +1;
				}
				else
				{
					/* not supported */
					*Length = 0;
				}
			}
			break;

		case SCARD_ATTR_MAXINPUT:
			*Length = sizeof(uint32_t);
			if (Value)
				*(uint32_t *)Value = get_ccid_descriptor(reader_index) -> dwMaxCCIDMessageLength -10;
			break;

#if !defined(TWIN_SERIAL)
		case TAG_IFD_POLLING_THREAD_WITH_TIMEOUT:
			{
				_ccid_descriptor *ccid_desc;

				/* default value: not supported */
				*Length = 0;

				ccid_desc = get_ccid_descriptor(reader_index);

				/* CCID and not ICCD */
				if ((PROTOCOL_CCID == ccid_desc -> bInterfaceProtocol)
					/* 3 end points */
					&& (3 == ccid_desc -> bNumEndpoints))
				{
					*Length = sizeof(void *);
					if (Value)
						*(void **)Value = IFDHPolling;
				}

				if ((PROTOCOL_ICCD_A == ccid_desc->bInterfaceProtocol)
					|| (PROTOCOL_ICCD_B == ccid_desc->bInterfaceProtocol))
				{
					*Length = sizeof(void *);
					if (Value)
						*(void **)Value = IFDHSleep;
				}
			}
			break;

		case TAG_IFD_POLLING_THREAD_KILLABLE:
			{
				_ccid_descriptor *ccid_desc;

				/* default value: not supported */
				*Length = 0;

				ccid_desc = get_ccid_descriptor(reader_index);
				if ((PROTOCOL_ICCD_A == ccid_desc->bInterfaceProtocol)
					|| (PROTOCOL_ICCD_B == ccid_desc->bInterfaceProtocol))
				{
					*Length = 1;	/* 1 char */
					if (Value)
						*Value = 1;	/* TRUE */
				}
			}
			break;

		case TAG_IFD_STOP_POLLING_THREAD:
			{
				_ccid_descriptor *ccid_desc;

				/* default value: not supported */
				*Length = 0;

				ccid_desc = get_ccid_descriptor(reader_index);
				/* CCID and not ICCD */
				if ((PROTOCOL_CCID == ccid_desc -> bInterfaceProtocol)
					/* 3 end points */
					&& (3 == ccid_desc -> bNumEndpoints))
				{
					*Length = sizeof(void *);
					if (Value)
						*(void **)Value = IFDHStopPolling;
				}
			}
			break;
#endif

		case SCARD_ATTR_VENDOR_IFD_SERIAL_NO:
			{
				_ccid_descriptor *ccid_desc;

				ccid_desc = get_ccid_descriptor(reader_index);
				if (ccid_desc->sIFD_serial_number)
				{
					strlcpy((char *)Value, ccid_desc->sIFD_serial_number, *Length);
					*Length = strlen((char *)Value)+1;
				}
				else
				{
					/* not supported */
					*Length = 0;
				}
			}
			break;

		default:
			return_value = IFD_ERROR_TAG;
	}

	return return_value;
} /* IFDHGetCapabilities */


EXTERNAL RESPONSECODE IFDHSetCapabilities(DWORD Lun, DWORD Tag,
	/*@unused@*/ DWORD Length, /*@unused@*/ PUCHAR Value)
{
	/*
	 * This function should set the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information needing set
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG IFD_ERROR_SET_FAILURE
	 * IFD_ERROR_VALUE_READ_ONLY
	 */

	(void)Length;
	(void)Value;

	int reader_index;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO4("tag: 0x" DWORD_X ", %s (lun: " DWORD_X ")", Tag,
		CcidSlots[reader_index].readerName, Lun);

	return IFD_NOT_SUPPORTED;
} /* IFDHSetCapabilities */


EXTERNAL RESPONSECODE IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol,
	UCHAR Flags, UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
	/*
	 * This function should set the PTS of a particular card/slot using
	 * the three PTS parameters sent
	 *
	 * Protocol - SCARD_PROTOCOL_T0 or SCARD_PROTOCOL_T1
	 * Flags - Logical OR of possible values:
	 *  IFD_NEGOTIATE_PTS1
	 *  IFD_NEGOTIATE_PTS2
	 *  IFD_NEGOTIATE_PTS3
	 * to determine which PTS values to negotiate.
	 * PTS1,PTS2,PTS3 - PTS Values.
	 *
	 * returns:
	 *  IFD_SUCCESS
	 *  IFD_ERROR_PTS_FAILURE
	 *  IFD_COMMUNICATION_ERROR
	 *  IFD_PROTOCOL_NOT_SUPPORTED
	 */

	BYTE pps[PPS_MAX_LENGTH];
	ATR_t atr;
	unsigned int len;
	int convention;
	int reader_index;
	int atr_ret;

	/* Set ccid desc params */
	CcidDesc *ccid_slot;
	_ccid_descriptor *ccid_desc;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO4("protocol T=" DWORD_D ", %s (lun: " DWORD_X ")",
		Protocol-SCARD_PROTOCOL_T0, CcidSlots[reader_index].readerName, Lun);

	/* Set to zero buffer */
	memset(pps, 0, sizeof(pps));
	memset(&atr, 0, sizeof(atr));

	/* Get ccid params */
	ccid_slot = get_ccid_slot(reader_index);
	ccid_desc = get_ccid_descriptor(reader_index);

	/* Do not send CCID command SetParameters or PPS to the CCID
	 * The CCID will do this himself */
	if (ccid_desc->dwFeatures & CCID_CLASS_AUTO_PPS_PROP)
	{
		DEBUG_COMM2("Timeout: %d ms", ccid_desc->readTimeout);
		goto end;
	}

	/* Get ATR of the card */
	atr_ret = ATR_InitFromArray(&atr, ccid_slot->pcATRBuffer,
		ccid_slot->nATRLength);
	if (ATR_MALFORMED == atr_ret)
		return IFD_PROTOCOL_NOT_SUPPORTED;

	/* Apply Extra EGT patch for bogus cards */
	extra_egt(&atr, ccid_desc, Protocol);

	if (SCARD_PROTOCOL_T0 == Protocol)
		pps[1] |= ATR_PROTOCOL_TYPE_T0;
	else
		if (SCARD_PROTOCOL_T1 == Protocol)
			pps[1] |= ATR_PROTOCOL_TYPE_T1;
		else
			return IFD_PROTOCOL_NOT_SUPPORTED;

	/* TA2 present -> specific mode */
	if (atr.ib[1][ATR_INTERFACE_BYTE_TA].present)
	{
		if (pps[1] != (atr.ib[1][ATR_INTERFACE_BYTE_TA].value & 0x0F))
		{
			/* wrong protocol */
			DEBUG_COMM3("Specific mode in T=%d and T=%d requested",
				atr.ib[1][ATR_INTERFACE_BYTE_TA].value & 0x0F, pps[1]);

			return IFD_PROTOCOL_NOT_SUPPORTED;
		}
	}

	/* TCi (i>2) indicates CRC instead of LRC */
	if (SCARD_PROTOCOL_T1 == Protocol)
	{
		t1_state_t *t1 = &(ccid_slot -> t1);
		int i;

		/* TCi (i>2) present? */
		for (i=2; i<ATR_MAX_PROTOCOLS; i++)
			if (atr.ib[i][ATR_INTERFACE_BYTE_TC].present)
			{
				if (0 == atr.ib[i][ATR_INTERFACE_BYTE_TC].value)
				{
					DEBUG_COMM("Use LRC");
					(void)t1_set_param(t1, IFD_PROTOCOL_T1_CHECKSUM_LRC, 0);
				}
				else
					if (1 == atr.ib[i][ATR_INTERFACE_BYTE_TC].value)
					{
						DEBUG_COMM("Use CRC");
						(void)t1_set_param(t1, IFD_PROTOCOL_T1_CHECKSUM_CRC, 0);
					}
					else
						DEBUG_COMM2("Wrong value for TCi: %d",
							atr.ib[i][ATR_INTERFACE_BYTE_TC].value);

				/* only the first TCi (i>2) must be used */
				break;
			}
	}

	/* PTS1? */
	if (Flags & IFD_NEGOTIATE_PTS1)
	{
		/* just use the value passed in argument */
		pps[1] |= 0x10; /* PTS1 presence */
		pps[2] = PTS1;
	}
	else
	{
		/* TA1 present */
		if (atr.ib[0][ATR_INTERFACE_BYTE_TA].present)
		{
			unsigned int card_baudrate;
			unsigned int default_baudrate;
			double f, d;

			(void)ATR_GetParameter(&atr, ATR_PARAMETER_D, &d);
			(void)ATR_GetParameter(&atr, ATR_PARAMETER_F, &f);

			/* may happen with non ISO cards */
			if ((0 == f) || (0 == d))
			{
				/* values for TA1=11 */
				f = 372;
				d = 1;
			}

			/* Baudrate = f x D/F */
			card_baudrate = (unsigned int) (1000 * ccid_desc->dwDefaultClock
				* d / f);

			default_baudrate = (unsigned int) (1000 * ccid_desc->dwDefaultClock
				* ATR_DEFAULT_D / ATR_DEFAULT_F);

			/* if the card does not try to lower the default speed */
			if ((card_baudrate > default_baudrate)
				/* and the reader is fast enough */
				&& (card_baudrate <= ccid_desc->dwMaxDataRate))
			{
				/* the reader has no baud rates table */
				if ((NULL == ccid_desc->arrayOfSupportedDataRates)
					/* or explicitely support it */
					|| find_baud_rate(card_baudrate,
						ccid_desc->arrayOfSupportedDataRates))
				{
					pps[1] |= 0x10; /* PTS1 presence */
					pps[2] = atr.ib[0][ATR_INTERFACE_BYTE_TA].value;

					DEBUG_COMM2("Set speed to %d bauds", card_baudrate);
				}
				else
				{
					DEBUG_COMM2("Reader does not support %d bauds",
						card_baudrate);

					/* TA2 present -> specific mode: the card is supporting
					 * only the baud rate specified in TA1 but reader does not
					 * support this value. Reject the card. */
					if (atr.ib[1][ATR_INTERFACE_BYTE_TA].present)
						return IFD_COMMUNICATION_ERROR;
				}
			}
			else
			{
				/* the card is too fast for the reader */
				if ((card_baudrate > ccid_desc->dwMaxDataRate +2)
					/* but TA1 <= 97 */
					&& (atr.ib[0][ATR_INTERFACE_BYTE_TA].value <= 0x97))
				{
					unsigned char old_TA1;

					old_TA1 = atr.ib[0][ATR_INTERFACE_BYTE_TA].value;
					while (atr.ib[0][ATR_INTERFACE_BYTE_TA].value > 0x94)
					{
						/* use a lower TA1 */
						atr.ib[0][ATR_INTERFACE_BYTE_TA].value--;

						(void)ATR_GetParameter(&atr, ATR_PARAMETER_D, &d);
						(void)ATR_GetParameter(&atr, ATR_PARAMETER_F, &f);

						/* Baudrate = f x D/F */
						card_baudrate = (unsigned int) (1000 *
							ccid_desc->dwDefaultClock * d / f);

						/* the reader has a baud rate table */
						if ((ccid_desc->arrayOfSupportedDataRates
							/* and the baud rate is supported */
							&& find_baud_rate(card_baudrate,
							ccid_desc->arrayOfSupportedDataRates))
							/* or the reader has NO baud rate table */
							|| ((NULL == ccid_desc->arrayOfSupportedDataRates)
							/* and the baud rate is bellow the limit */
							&& (card_baudrate <= ccid_desc->dwMaxDataRate)))
						{
							pps[1] |= 0x10; /* PTS1 presence */
							pps[2] = atr.ib[0][ATR_INTERFACE_BYTE_TA].value;

							DEBUG_COMM2("Set adapted speed to %d bauds",
								card_baudrate);

							break;
						}
					}

					/* restore original TA1 value */
					atr.ib[0][ATR_INTERFACE_BYTE_TA].value = old_TA1;
				}
			}
		}
	}

	/* PTS2? */
	if (Flags & IFD_NEGOTIATE_PTS2)
	{
		pps[1] |= 0x20; /* PTS2 presence */
		pps[3] = PTS2;
	}

	/* PTS3? */
	if (Flags & IFD_NEGOTIATE_PTS3)
	{
		pps[1] |= 0x40; /* PTS3 presence */
		pps[4] = PTS3;
	}

	/* Generate PPS */
	pps[0] = 0xFF;

	/* Automatic PPS made by the ICC? */
	if ((! (ccid_desc->dwFeatures & CCID_CLASS_AUTO_PPS_CUR))
		/* TA2 absent: negociable mode */
		&& (! atr.ib[1][ATR_INTERFACE_BYTE_TA].present))
	{
		int default_protocol;

		ATR_GetDefaultProtocol(&atr, &default_protocol, NULL);

		/* if the requested protocol is not the default one
		 * or a TA1/PPS1 is present */
		if (((pps[1] & 0x0F) != default_protocol) || (PPS_HAS_PPS1(pps)))
		{
#ifdef O2MICRO_OZ776_PATCH
			if ((OZ776 == ccid_desc->readerID)
				|| (OZ776_7772 == ccid_desc->readerID))
			{
				/* convert from ATR_PROTOCOL_TYPE_T? to SCARD_PROTOCOL_T? */
				Protocol = default_protocol +
					(SCARD_PROTOCOL_T0 - ATR_PROTOCOL_TYPE_T0);
				DEBUG_INFO2("PPS not supported on O2Micro readers. Using T=" DWORD_D,
					Protocol - SCARD_PROTOCOL_T0);
			}
			else
#endif
			if (PPS_Exchange(reader_index, pps, &len, &pps[2]) != PPS_OK)
			{
				DEBUG_INFO1("PPS_Exchange Failed");

				return IFD_ERROR_PTS_FAILURE;
			}
		}
	}

	/* Now we must set the reader parameters */
	(void)ATR_GetConvention(&atr, &convention);

	/* specific mode and implicit parameters? (b5 of TA2) */
	if (atr.ib[1][ATR_INTERFACE_BYTE_TA].present
		&& (atr.ib[1][ATR_INTERFACE_BYTE_TA].value & 0x10))
		return IFD_COMMUNICATION_ERROR;

	/* T=1 */
	if (SCARD_PROTOCOL_T1 == Protocol)
	{
		BYTE param[] = {
			0x11,	/* Fi/Di		*/
			0x10,	/* TCCKS		*/
			0x00,	/* GuardTime	*/
			0x4D,	/* BWI/CWI		*/
			0x00,	/* ClockStop	*/
			0x20,	/* IFSC			*/
			0x00	/* NADValue		*/
		};
		int i;
		t1_state_t *t1 = &(ccid_slot -> t1);
		RESPONSECODE ret;
		double f, d;
		int ifsc;

		/* TA1 is not default */
		if (PPS_HAS_PPS1(pps))
			param[0] = pps[2];

		/* CRC checksum? */
		if (2 == t1->rc_bytes)
			param[1] |= 0x01;

		/* the CCID should ignore this bit */
		if (ATR_CONVENTION_INVERSE == convention)
			param[1] |= 0x02;

		/* get TC1 Extra guard time */
		if (atr.ib[0][ATR_INTERFACE_BYTE_TC].present)
			param[2] = atr.ib[0][ATR_INTERFACE_BYTE_TC].value;

		/* TBi (i>2) present? BWI/CWI */
		for (i=2; i<ATR_MAX_PROTOCOLS; i++)
			if (atr.ib[i][ATR_INTERFACE_BYTE_TB].present)
			{
				DEBUG_COMM3("BWI/CWI (TB%d) present: 0x%02X", i+1,
					atr.ib[i][ATR_INTERFACE_BYTE_TB].value);
				param[3] = atr.ib[i][ATR_INTERFACE_BYTE_TB].value;

				{
					/* Hack for OpenPGP card */
					unsigned char openpgp_atr[] = { 0x3B, 0xFA, 0x13,
						0x00, 0xFF, 0x81, 0x31, 0x80, 0x45, 0x00, 0x31,
						0xC1, 0x73, 0xC0, 0x01, 0x00, 0x00, 0x90, 0x00, 0xB1 };

					if (0 == memcmp(ccid_slot->pcATRBuffer, openpgp_atr,
						ccid_slot->nATRLength))
						/* change BWI from 4 to 7 to increase BWT from
						 * 1.4s to 11s and avoid a timeout during on
						 * board key generation (bogus card) */
					{
						param[3] = 0x75;
						DEBUG_COMM2("OpenPGP hack, using 0x%02X", param[3]);
					}
				}

				/* only the first TBi (i>2) must be used */
				break;
			}

		/* compute communication timeout */
		(void)ATR_GetParameter(&atr, ATR_PARAMETER_F, &f);
		(void)ATR_GetParameter(&atr, ATR_PARAMETER_D, &d);
		ccid_desc->readTimeout = T1_card_timeout(f, d, param[2],
			(param[3] & 0xF0) >> 4 /* BWI */, param[3] & 0x0F /* CWI */,
			ccid_desc->dwDefaultClock);

		ifsc = get_IFSC(&atr, &i);
		if (ifsc > 0)
		{
			DEBUG_COMM3("IFSC (TA%d) present: %d", i, ifsc);
			param[5] = ifsc;
		}

		DEBUG_COMM2("Timeout: %d ms", ccid_desc->readTimeout);

		ret = SetParameters(reader_index, 1, sizeof(param), param);
		if (IFD_SUCCESS != ret)
			return ret;
	}
	else
	/* T=0 */
	{
		BYTE param[] = {
			0x11,	/* Fi/Di			*/
			0x00,	/* TCCKS			*/
			0x00,	/* GuardTime		*/
			0x0A,	/* WaitingInteger	*/
			0x00	/* ClockStop		*/
		};
		RESPONSECODE ret;
		double f, d;

		/* TA1 is not default */
		if (PPS_HAS_PPS1(pps))
			param[0] = pps[2];

		if (ATR_CONVENTION_INVERSE == convention)
			param[1] |= 0x02;

		/* get TC1 Extra guard time */
		if (atr.ib[0][ATR_INTERFACE_BYTE_TC].present)
			param[2] = atr.ib[0][ATR_INTERFACE_BYTE_TC].value;

		/* TC2 WWT */
		if (atr.ib[1][ATR_INTERFACE_BYTE_TC].present)
			param[3] = atr.ib[1][ATR_INTERFACE_BYTE_TC].value;

		/* compute communication timeout */
		(void)ATR_GetParameter(&atr, ATR_PARAMETER_F, &f);
		(void)ATR_GetParameter(&atr, ATR_PARAMETER_D, &d);

		ccid_desc->readTimeout = T0_card_timeout(f, d, param[2] /* TC1 */,
			param[3] /* TC2 */, ccid_desc->dwDefaultClock);

		DEBUG_COMM2("Communication timeout: %d ms", ccid_desc->readTimeout);

		ret = SetParameters(reader_index, 0, sizeof(param), param);
		if (IFD_SUCCESS != ret)
			return ret;
	}

	/* set IFSC & IFSD in T=1 */
	if (SCARD_PROTOCOL_T1 == Protocol)
	{
		t1_state_t *t1 = &(ccid_slot -> t1);
		int i, ifsc;

		ifsc = get_IFSC(&atr, &i);
		if (ifsc > 0)
		{
			DEBUG_COMM3("IFSC (TA%d) present: %d", i, ifsc);
			(void)t1_set_param(t1, IFD_PROTOCOL_T1_IFSC, ifsc);
		}

		/* IFSD not negociated by the reader? */
		if (! (ccid_desc->dwFeatures & CCID_CLASS_AUTO_IFSD))
		{
			DEBUG_COMM2("Negotiate IFSD at %d", ccid_desc -> dwMaxIFSD);
			if (t1_negotiate_ifsd(t1, 0, ccid_desc -> dwMaxIFSD) < 0)
				return IFD_COMMUNICATION_ERROR;
		}
		(void)t1_set_param(t1, IFD_PROTOCOL_T1_IFSD, ccid_desc -> dwMaxIFSD);

		DEBUG_COMM3("T=1: IFSC=%d, IFSD=%d", t1->ifsc, t1->ifsd);
	}

end:
	/* store used protocol for use by the secure commands (verify/change PIN) */
	ccid_desc->cardProtocol = Protocol;

	return IFD_SUCCESS;
} /* IFDHSetProtocolParameters */


EXTERNAL RESPONSECODE IFDHPowerICC(DWORD Lun, DWORD Action,
	PUCHAR Atr, PDWORD AtrLength)
{
	/*
	 * This function controls the power and reset signals of the smartcard
	 * reader at the particular reader/slot specified by Lun.
	 *
	 * Action - Action to be taken on the card.
	 *
	 * IFD_POWER_UP - Power and reset the card if not done so (store the
	 * ATR and return it and its length).
	 *
	 * IFD_POWER_DOWN - Power down the card if not done already
	 * (Atr/AtrLength should be zero'd)
	 *
	 * IFD_RESET - Perform a quick reset on the card.  If the card is not
	 * powered power up the card.  (Store and return the Atr/Length)
	 *
	 * Atr - Answer to Reset of the card.  The driver is responsible for
	 * caching this value in case IFDHGetCapabilities is called requesting
	 * the ATR and its length.  This should not exceed MAX_ATR_SIZE.
	 *
	 * AtrLength - Length of the Atr.  This should not exceed
	 * MAX_ATR_SIZE.
	 *
	 * Notes:
	 *
	 * Memory cards without an ATR should return IFD_SUCCESS on reset but
	 * the Atr should be zero'd and the length should be zero
	 *
	 * Reset errors should return zero for the AtrLength and return
	 * IFD_ERROR_POWER_ACTION.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_POWER_ACTION IFD_COMMUNICATION_ERROR
	 * IFD_NOT_SUPPORTED
	 */

	unsigned int nlength;
	RESPONSECODE return_value = IFD_SUCCESS;
	unsigned char pcbuffer[10+MAX_ATR_SIZE];
	int reader_index;
#ifndef NO_LOG
	const char *actions[] = { "PowerUp", "PowerDown", "Reset" };
#endif
	unsigned int oldReadTimeout;
	_ccid_descriptor *ccid_descriptor;

	/* By default, assume it won't work :) */
	*AtrLength = 0;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_INFO4("action: %s, %s (lun: " DWORD_X ")",
		actions[Action-IFD_POWER_UP], CcidSlots[reader_index].readerName, Lun);

	switch (Action)
	{
		case IFD_POWER_DOWN:
			/* Clear ATR buffer */
			CcidSlots[reader_index].nATRLength = 0;
			*CcidSlots[reader_index].pcATRBuffer = '\0';

			/* Memorise the request */
			CcidSlots[reader_index].bPowerFlags |= MASK_POWERFLAGS_PDWN;

			/* send the command */
			if (IFD_SUCCESS != CmdPowerOff(reader_index))
			{
				DEBUG_CRITICAL("PowerDown failed");
				return_value = IFD_ERROR_POWER_ACTION;
				goto end;
			}

			/* clear T=1 context */
			t1_release(&(get_ccid_slot(reader_index) -> t1));
			break;

		case IFD_POWER_UP:
		case IFD_RESET:
			/* save the current read timeout computed from card capabilities */
			ccid_descriptor = get_ccid_descriptor(reader_index);
			oldReadTimeout = ccid_descriptor->readTimeout;

			/* The German eID card is bogus and need to be powered off
			 * before a power on */
			if (KOBIL_IDTOKEN == ccid_descriptor -> readerID)
			{
				/* send the command */
				if (IFD_SUCCESS != CmdPowerOff(reader_index))
				{
					DEBUG_CRITICAL("PowerDown failed");
					return_value = IFD_ERROR_POWER_ACTION;
					goto end;
				}
			}

			/* use a very long timeout since the card can use up to
			 * (9600+12)*33 ETU in total
			 * 12 ETU per byte
			 * 9600 ETU max between each byte
			 * 33 bytes max for ATR
			 * 1 ETU = 372 cycles during ATR
			 * with a 4 MHz clock => 29 seconds
			 */
			ccid_descriptor->readTimeout = 60*1000;

			nlength = sizeof(pcbuffer);
			return_value = CmdPowerOn(reader_index, &nlength, pcbuffer,
				PowerOnVoltage);

			/* set back the old timeout */
			ccid_descriptor->readTimeout = oldReadTimeout;

			if (return_value != IFD_SUCCESS)
			{
				/* used by GemCore SIM PRO: no card is present */
				if (GEMCORESIMPRO == ccid_descriptor -> readerID)
					get_ccid_descriptor(reader_index)->dwSlotStatus
						= IFD_ICC_NOT_PRESENT;

				DEBUG_CRITICAL("PowerUp failed");
				return_value = IFD_ERROR_POWER_ACTION;
				goto end;
			}

			/* Power up successful, set state variable to memorise it */
			CcidSlots[reader_index].bPowerFlags |= MASK_POWERFLAGS_PUP;
			CcidSlots[reader_index].bPowerFlags &= ~MASK_POWERFLAGS_PDWN;

			/* Reset is returned, even if TCK is wrong */
			CcidSlots[reader_index].nATRLength = *AtrLength =
				(nlength < MAX_ATR_SIZE) ? nlength : MAX_ATR_SIZE;
			memcpy(Atr, pcbuffer, *AtrLength);
			memcpy(CcidSlots[reader_index].pcATRBuffer, pcbuffer, *AtrLength);

			/* initialise T=1 context */
			(void)t1_init(&(get_ccid_slot(reader_index) -> t1), reader_index);
			break;

		default:
			DEBUG_CRITICAL("Action not supported");
			return_value = IFD_NOT_SUPPORTED;
	}
end:

	return return_value;
} /* IFDHPowerICC */


EXTERNAL RESPONSECODE IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci,
	PUCHAR TxBuffer, DWORD TxLength,
	PUCHAR RxBuffer, PDWORD RxLength, /*@unused@*/ PSCARD_IO_HEADER RecvPci)
{
	/*
	 * This function performs an APDU exchange with the card/slot
	 * specified by Lun.  The driver is responsible for performing any
	 * protocol specific exchanges such as T=0/1 ... differences.  Calling
	 * this function will abstract all protocol differences.
	 *
	 * SendPci Protocol - 0, 1, .... 14 Length - Not used.
	 *
	 * TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F
	 * 0x00) TxLength - Length of this buffer. RxBuffer - Receive APDU
	 * example (0x61 0x14) RxLength - Length of the received APDU.  This
	 * function will be passed the size of the buffer of RxBuffer and this
	 * function is responsible for setting this to the length of the
	 * received APDU.  This should be ZERO on all errors.  The resource
	 * manager will take responsibility of zeroing out any temporary APDU
	 * buffers for security reasons.
	 *
	 * RecvPci Protocol - 0, 1, .... 14 Length - Not used.
	 *
	 * Notes: The driver is responsible for knowing what type of card it
	 * has.  If the current slot/card contains a memory card then this
	 * command should ignore the Protocol and use the MCT style commands
	 * for support for these style cards and transmit them appropriately.
	 * If your reader does not support memory cards or you don't want to
	 * then ignore this.
	 *
	 * RxLength should be set to zero on error.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR IFD_RESPONSE_TIMEOUT
	 * IFD_ICC_NOT_PRESENT IFD_PROTOCOL_NOT_SUPPORTED
	 */

	RESPONSECODE return_value;
	unsigned int rx_length;
	int reader_index;
	int old_read_timeout;
	int restore_timeout = FALSE;
	_ccid_descriptor *ccid_descriptor;

	(void)RecvPci;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	ccid_descriptor = get_ccid_descriptor(reader_index);

	DEBUG_INFO3("%s (lun: " DWORD_X ")", CcidSlots[reader_index].readerName,
		Lun);

	/* special APDU for the Kobil IDToken (CLASS = 0xFF) */
	if (KOBIL_IDTOKEN == ccid_descriptor -> readerID)
	{
		char manufacturer[] = {0xFF, 0x9A, 0x01, 0x01, 0x00};
		char product_name[] = {0xFF, 0x9A, 0x01, 0x03, 0x00};
		char firmware_version[] = {0xFF, 0x9A, 0x01, 0x06, 0x00};
		char driver_version[] = {0xFF, 0x9A, 0x01, 0x07, 0x00};

		if ((sizeof manufacturer == TxLength)
			&& (memcmp(TxBuffer, manufacturer, sizeof manufacturer) == 0))
		{
			DEBUG_INFO1("IDToken: Manufacturer command");
			memcpy(RxBuffer, "KOBIL systems\220\0", 15);
			*RxLength = 15;
			return IFD_SUCCESS;
		}

		if ((sizeof product_name == TxLength)
			&& (memcmp(TxBuffer, product_name, sizeof product_name) == 0))
		{
			DEBUG_INFO1("IDToken: Product name command");
			memcpy(RxBuffer, "IDToken\220\0", 9);
			*RxLength = 9;
			return IFD_SUCCESS;
		}

		if ((sizeof firmware_version == TxLength)
			&& (memcmp(TxBuffer, firmware_version, sizeof firmware_version) == 0))
		{
			int IFD_bcdDevice = ccid_descriptor -> IFD_bcdDevice;

			DEBUG_INFO1("IDToken: Firmware version command");
			*RxLength = sprintf((char *)RxBuffer, "%X.%02X",
				IFD_bcdDevice >> 8, IFD_bcdDevice & 0xFF);
			RxBuffer[(*RxLength)++] = 0x90;
			RxBuffer[(*RxLength)++] = 0x00;
			return IFD_SUCCESS;
		}

		if ((sizeof driver_version == TxLength)
			&& (memcmp(TxBuffer, driver_version, sizeof driver_version) == 0))
		{
			DEBUG_INFO1("IDToken: Driver version command");
#define DRIVER_VERSION "2012.2.7\220\0"
			memcpy(RxBuffer, DRIVER_VERSION, sizeof DRIVER_VERSION -1);
			*RxLength = sizeof DRIVER_VERSION -1;
			return IFD_SUCCESS;
		}

	}

	/* Pseudo-APDU as defined in PC/SC v2 part 10 supplement document
	 * CLA=0xFF, INS=0xC2, P1=0x01 */
	if (0 == memcmp(TxBuffer, "\xFF\xC2\x01", 3))
	{
		/* Yes, use the same timeout as for SCardControl() */
		restore_timeout = TRUE;
		old_read_timeout = ccid_descriptor -> readTimeout;
		ccid_descriptor -> readTimeout = 90 * 1000;	/* 90 seconds */
	}

	rx_length = *RxLength;
	return_value = CmdXfrBlock(reader_index, TxLength, TxBuffer, &rx_length,
		RxBuffer, SendPci.Protocol);
	if (IFD_SUCCESS == return_value)
		*RxLength = rx_length;
	else
		*RxLength = 0;

	/* restore timeout */
	if (restore_timeout)
		ccid_descriptor -> readTimeout = old_read_timeout;

	return return_value;
} /* IFDHTransmitToICC */


EXTERNAL RESPONSECODE IFDHControl(DWORD Lun, DWORD dwControlCode,
	PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength,
	PDWORD pdwBytesReturned)
{
	/*
	 * This function performs a data exchange with the reader (not the
	 * card) specified by Lun.  Here XXXX will only be used. It is
	 * responsible for abstracting functionality such as PIN pads,
	 * biometrics, LCD panels, etc.  You should follow the MCT, CTBCS
	 * specifications for a list of accepted commands to implement.
	 *
	 * TxBuffer - Transmit data TxLength - Length of this buffer. RxBuffer
	 * - Receive data RxLength - Length of the received data.  This
	 * function will be passed the length of the buffer RxBuffer and it
	 * must set this to the length of the received data.
	 *
	 * Notes: RxLength should be zero on error.
	 */
	RESPONSECODE return_value = IFD_ERROR_NOT_SUPPORTED;
	int reader_index;
	_ccid_descriptor *ccid_descriptor;

	reader_index = LunToReaderIndex(Lun);
	if ((-1 == reader_index) || (NULL == pdwBytesReturned))
		return IFD_COMMUNICATION_ERROR;

	ccid_descriptor = get_ccid_descriptor(reader_index);

	DEBUG_INFO4("ControlCode: 0x" DWORD_X ", %s (lun: " DWORD_X ")",
		dwControlCode, CcidSlots[reader_index].readerName, Lun);
	DEBUG_INFO_XXD("Control TxBuffer: ", TxBuffer, TxLength);

	/* Set the return length to 0 to avoid problems */
	*pdwBytesReturned = 0;

	if (IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE == dwControlCode)
	{
		int allowed = (DriverOptions & DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED);
		int readerID = ccid_descriptor -> readerID;

		if (VENDOR_GEMALTO == GET_VENDOR(readerID))
		{
			unsigned char switch_interface[] = { 0x52, 0xF8, 0x04, 0x01, 0x00 };

			/* get firmware version escape command */
			if ((1 == TxLength) && (0x02 == TxBuffer[0]))
				allowed = TRUE;

			/* switch interface escape command on the GemProx DU
			 * the next byte in the command is the interface:
			 * 0x01 switch to contactless interface
			 * 0x02 switch to contact interface
			 */
			if ((GEMALTOPROXDU == readerID)
				&& (6 == TxLength)
				&& (0 == memcmp(TxBuffer, switch_interface, sizeof(switch_interface))))
				allowed = TRUE;
		}

		if (!allowed)
		{
			DEBUG_INFO1("ifd exchange (Escape command) not allowed");
			return_value = IFD_COMMUNICATION_ERROR;
		}
		else
		{
			unsigned int iBytesReturned;

			iBytesReturned = RxLength;
			/* 30 seconds timeout for long commands */
			return_value = CmdEscape(reader_index, TxBuffer, TxLength,
				RxBuffer, &iBytesReturned, 30*1000);
			*pdwBytesReturned = iBytesReturned;
		}
	}

	/* Implement the PC/SC v2.02.07 Part 10 IOCTL mechanism */

	/* Query for features */
	/* 0x313520 is the Windows value for SCARD_CTL_CODE(3400)
	 * This hack is needed for RDP applications */
	if ((CM_IOCTL_GET_FEATURE_REQUEST == dwControlCode)
		|| (0x313520 == dwControlCode))
	{
		unsigned int iBytesReturned = 0;
		PCSC_TLV_STRUCTURE *pcsc_tlv = (PCSC_TLV_STRUCTURE *)RxBuffer;
		int readerID = ccid_descriptor -> readerID;

		/* we need room for up to five records */
		if (RxLength < 6 * sizeof(PCSC_TLV_STRUCTURE))
			return IFD_ERROR_INSUFFICIENT_BUFFER;

		/* We can only support direct verify and/or modify currently */
		if (ccid_descriptor -> bPINSupport & CCID_CLASS_PIN_VERIFY)
		{
			pcsc_tlv -> tag = FEATURE_VERIFY_PIN_DIRECT;
			pcsc_tlv -> length = 0x04; /* always 0x04 */
			pcsc_tlv -> value = htonl(IOCTL_FEATURE_VERIFY_PIN_DIRECT);

			pcsc_tlv++;
			iBytesReturned += sizeof(PCSC_TLV_STRUCTURE);
		}

		if (ccid_descriptor -> bPINSupport & CCID_CLASS_PIN_MODIFY)
		{
			pcsc_tlv -> tag = FEATURE_MODIFY_PIN_DIRECT;
			pcsc_tlv -> length = 0x04; /* always 0x04 */
			pcsc_tlv -> value = htonl(IOCTL_FEATURE_MODIFY_PIN_DIRECT);

			pcsc_tlv++;
			iBytesReturned += sizeof(PCSC_TLV_STRUCTURE);
		}

		/* Provide IFD_PIN_PROPERTIES only for pinpad readers */
		if (ccid_descriptor -> bPINSupport)
		{
			pcsc_tlv -> tag = FEATURE_IFD_PIN_PROPERTIES;
			pcsc_tlv -> length = 0x04; /* always 0x04 */
			pcsc_tlv -> value = htonl(IOCTL_FEATURE_IFD_PIN_PROPERTIES);

			pcsc_tlv++;
			iBytesReturned += sizeof(PCSC_TLV_STRUCTURE);
		}

		if ((KOBIL_TRIBANK == readerID)
			|| (KOBIL_MIDENTITY_VISUAL == readerID))
		{
			pcsc_tlv -> tag = FEATURE_MCT_READER_DIRECT;
			pcsc_tlv -> length = 0x04; /* always 0x04 */
			pcsc_tlv -> value = htonl(IOCTL_FEATURE_MCT_READER_DIRECT);

			pcsc_tlv++;
			iBytesReturned += sizeof(PCSC_TLV_STRUCTURE);
		}

		pcsc_tlv -> tag = FEATURE_GET_TLV_PROPERTIES;
		pcsc_tlv -> length = 0x04; /* always 0x04 */
		pcsc_tlv -> value = htonl(IOCTL_FEATURE_GET_TLV_PROPERTIES);
		pcsc_tlv++;
		iBytesReturned += sizeof(PCSC_TLV_STRUCTURE);

		/* IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE */
		if (DriverOptions & DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED)
		{
			pcsc_tlv -> tag = FEATURE_CCID_ESC_COMMAND;
			pcsc_tlv -> length = 0x04; /* always 0x04 */
			pcsc_tlv -> value = htonl(IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE);

			pcsc_tlv++;
			iBytesReturned += sizeof(PCSC_TLV_STRUCTURE);
		}

		*pdwBytesReturned = iBytesReturned;
		return_value = IFD_SUCCESS;
	}

	/* Get PIN handling capabilities */
	if (IOCTL_FEATURE_IFD_PIN_PROPERTIES == dwControlCode)
	{
		PIN_PROPERTIES_STRUCTURE *caps = (PIN_PROPERTIES_STRUCTURE *)RxBuffer;
		int validation;

		if (RxLength < sizeof(PIN_PROPERTIES_STRUCTURE))
			return IFD_ERROR_INSUFFICIENT_BUFFER;

		/* Only give the LCD size for now */
		caps -> wLcdLayout = ccid_descriptor -> wLcdLayout;

		/* Hardcoded special reader cases */
		switch (ccid_descriptor->readerID)
		{
			case GEMPCPINPAD:
			case VEGAALPHA:
			case CHERRYST2000:
				validation = 0x02; /* Validation key pressed */
				break;
			default:
				validation = 0x07; /* Default */
		}

		/* Gemalto readers providing firmware features */
		if (ccid_descriptor -> gemalto_firmware_features)
			validation = ccid_descriptor -> gemalto_firmware_features -> bEntryValidationCondition;

		caps -> bEntryValidationCondition = validation;
		caps -> bTimeOut2 = 0x00; /* We do not distinguish bTimeOut from TimeOut2 */

		*pdwBytesReturned = sizeof(*caps);
		return_value = IFD_SUCCESS;
	}

	/* Reader features */
	if (IOCTL_FEATURE_GET_TLV_PROPERTIES == dwControlCode)
	{
		int p = 0;
		int tmp;

		/* wLcdLayout */
		RxBuffer[p++] = PCSCv2_PART10_PROPERTY_wLcdLayout;	/* tag */
		RxBuffer[p++] = 2;	/* length */
		tmp = ccid_descriptor -> wLcdLayout;
		RxBuffer[p++] = tmp & 0xFF;	/* value in little endian order */
		RxBuffer[p++] = (tmp >> 8) & 0xFF;

		/* only if the reader has a display */
		if (ccid_descriptor -> wLcdLayout)
		{
			/* wLcdMaxCharacters */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_wLcdMaxCharacters;	/* tag */
			RxBuffer[p++] = 2;	/* length */
			tmp = ccid_descriptor -> wLcdLayout & 0xFF;
			RxBuffer[p++] = tmp & 0xFF;	/* value in little endian order */
			RxBuffer[p++] = (tmp >> 8) & 0xFF;

			/* wLcdMaxLines */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_wLcdMaxLines;	/* tag */
			RxBuffer[p++] = 2;	/* length */
			tmp = ccid_descriptor -> wLcdLayout >> 8;
			RxBuffer[p++] = tmp & 0xFF;	/* value in little endian order */
			RxBuffer[p++] = (tmp >> 8) & 0xFF;
		}

		/* bTimeOut2 */
		RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bTimeOut2;
		RxBuffer[p++] = 1;	/* length */
		/* IFD does not distinguish bTimeOut from bTimeOut2 */
		RxBuffer[p++] = 0x00;

		/* sFirmwareID */
		if (VENDOR_GEMALTO == GET_VENDOR(ccid_descriptor -> readerID))
		{
			unsigned char firmware[256];
			const unsigned char cmd[] = { 0x02 };
			RESPONSECODE ret;
			unsigned int len;

			len = sizeof(firmware);
			ret = CmdEscape(reader_index, cmd, sizeof(cmd), firmware, &len, 0);

			if (IFD_SUCCESS == ret)
			{
				RxBuffer[p++] = PCSCv2_PART10_PROPERTY_sFirmwareID;
				RxBuffer[p++] = len;
				memcpy(&RxBuffer[p], firmware, len);
				p += len;
			}
		}

		/* Gemalto PC Pinpad V1 */
		if (((GEMPCPINPAD == ccid_descriptor -> readerID)
			&& (0x0100 == ccid_descriptor -> IFD_bcdDevice))
			/* Covadis Véga-Alpha */
			|| (VEGAALPHA == ccid_descriptor->readerID))
		{
			/* bMinPINSize */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bMinPINSize;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = 4;	/* min PIN size */

			/* bMaxPINSize */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bMaxPINSize;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = 8;	/* max PIN size */

			/* bEntryValidationCondition */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bEntryValidationCondition;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = 0x02;	/* validation key pressed */
		}

		/* Cherry GmbH SmartTerminal ST-2xxx */
		if (CHERRYST2000 == ccid_descriptor -> readerID)
		{
			/* bMinPINSize */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bMinPINSize;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = 0;	/* min PIN size */

			/* bMaxPINSize */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bMaxPINSize;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = 25;	/* max PIN size */

			/* bEntryValidationCondition */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bEntryValidationCondition;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = 0x02;	/* validation key pressed */
		}

		/* Gemalto readers providing firmware features */
		if (ccid_descriptor -> gemalto_firmware_features)
		{
			struct GEMALTO_FIRMWARE_FEATURES *features = ccid_descriptor -> gemalto_firmware_features;

			/* bMinPINSize */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bMinPINSize;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = features -> MinimumPINSize;	/* min PIN size */

			/* bMaxPINSize */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bMaxPINSize;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = features -> MaximumPINSize;	/* max PIN size */

			/* bEntryValidationCondition */
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bEntryValidationCondition;
			RxBuffer[p++] = 1;	/* length */
			RxBuffer[p++] = features -> bEntryValidationCondition;	/* validation key pressed */
		}

		/* bPPDUSupport */
		RxBuffer[p++] = PCSCv2_PART10_PROPERTY_bPPDUSupport;
		RxBuffer[p++] = 1;	/* length */
		RxBuffer[p++] =
			(DriverOptions & DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED) ? 1 : 0;
			/* bit0: PPDU is supported over SCardControl using
			 * FEATURE_CCID_ESC_COMMAND */

		/* wIdVendor */
		{
			int idVendor = ccid_descriptor -> readerID >> 16;
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_wIdVendor;
			RxBuffer[p++] = 2;	/* length */
			RxBuffer[p++] = idVendor & 0xFF;
			RxBuffer[p++] = idVendor >> 8;
		}

		/* wIdProduct */
		{
			int idProduct = ccid_descriptor -> readerID & 0xFFFF;
			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_wIdProduct;
			RxBuffer[p++] = 2;	/* length */
			RxBuffer[p++] = idProduct & 0xFF;
			RxBuffer[p++] = idProduct >> 8;
		}

		/* dwMaxAPDUDataSize */
		{
			int MaxAPDUDataSize = 0; /* short APDU only by default */

			/* reader is TPDU or extended APDU */
			if ((ccid_descriptor -> dwFeatures & CCID_CLASS_EXTENDED_APDU)
				|| (ccid_descriptor -> dwFeatures & CCID_CLASS_TPDU))
				MaxAPDUDataSize = 0x10000;

			RxBuffer[p++] = PCSCv2_PART10_PROPERTY_dwMaxAPDUDataSize;
			RxBuffer[p++] = 4;	/* length */
			RxBuffer[p++] = MaxAPDUDataSize & 0xFF;
			RxBuffer[p++] = (MaxAPDUDataSize >> 8) & 0xFF;
			RxBuffer[p++] = (MaxAPDUDataSize >> 16) & 0xFF;
			RxBuffer[p++] = (MaxAPDUDataSize >> 24) & 0xFF;
		}

		*pdwBytesReturned = p;
		return_value = IFD_SUCCESS;
	}

	/* Verify a PIN, plain CCID */
	if (IOCTL_FEATURE_VERIFY_PIN_DIRECT == dwControlCode)
	{
		unsigned int iBytesReturned;

		iBytesReturned = RxLength;
		return_value = SecurePINVerify(reader_index, TxBuffer, TxLength,
			RxBuffer, &iBytesReturned);
		*pdwBytesReturned = iBytesReturned;
	}

	/* Modify a PIN, plain CCID */
	if (IOCTL_FEATURE_MODIFY_PIN_DIRECT == dwControlCode)
	{
		unsigned int iBytesReturned;

		iBytesReturned = RxLength;
		return_value = SecurePINModify(reader_index, TxBuffer, TxLength,
			RxBuffer, &iBytesReturned);
		*pdwBytesReturned = iBytesReturned;
	}

	/* MCT: Multifunctional Card Terminal */
	if (IOCTL_FEATURE_MCT_READER_DIRECT == dwControlCode)
	{
		if ( (TxBuffer[0] != 0x20)	/* CLA */
			|| ((TxBuffer[1] & 0xF0) != 0x70)	/* INS */
			/* valid INS are
			 * 0x70: SECODER INFO
			 * 0x71: SECODER SELECT APPLICATION
			 * 0x72: SECODER APPLICATION ACTIVE
			 * 0x73: SECODER DATA CONFIRMATION
			 * 0x74: SECODER PROCESS AUTHENTICATION TOKEN */
			|| ((TxBuffer[1] & 0x0F) > 4)
			|| (TxBuffer[2] != 0x00)	/* P1 */
			|| (TxBuffer[3] != 0x00)	/* P2 */
			|| (TxBuffer[4] != 0x00)	/* Lind */
		   )
		{
			DEBUG_INFO1("MCT Command refused by driver");
			return_value = IFD_COMMUNICATION_ERROR;
		}
		else
		{
			unsigned int iBytesReturned;

			/* we just transmit the buffer as a CCID Escape command */
			iBytesReturned = RxLength;
			return_value = CmdEscape(reader_index, TxBuffer, TxLength,
				RxBuffer, &iBytesReturned, 0);
			*pdwBytesReturned = iBytesReturned;
		}
	}

	if (IFD_SUCCESS != return_value)
		*pdwBytesReturned = 0;

	DEBUG_INFO_XXD("Control RxBuffer: ", RxBuffer, *pdwBytesReturned);
	return return_value;
} /* IFDHControl */


EXTERNAL RESPONSECODE IFDHICCPresence(DWORD Lun)
{
	/*
	 * This function returns the status of the card inserted in the
	 * reader/slot specified by Lun.  It will return either:
	 *
	 * returns: IFD_ICC_PRESENT IFD_ICC_NOT_PRESENT
	 * IFD_COMMUNICATION_ERROR
	 */

	unsigned char pcbuffer[SIZE_GET_SLOT_STATUS];
	RESPONSECODE return_value = IFD_COMMUNICATION_ERROR;
	int oldLogLevel;
	int reader_index;
	_ccid_descriptor *ccid_descriptor;
	unsigned int oldReadTimeout;

	if (-1 == (reader_index = LunToReaderIndex(Lun)))
		return IFD_COMMUNICATION_ERROR;

	DEBUG_PERIODIC3("%s (lun: " DWORD_X ")", CcidSlots[reader_index].readerName, Lun);

	ccid_descriptor = get_ccid_descriptor(reader_index);

	if ((GEMCORESIMPRO == ccid_descriptor->readerID)
	     && (ccid_descriptor->IFD_bcdDevice < 0x0200))
	{
		/* GemCore SIM Pro firmware 2.00 and up features
		 * a full independant second slot */
		return_value = ccid_descriptor->dwSlotStatus;
		goto end;
	}

	/* save the current read timeout computed from card capabilities */
	oldReadTimeout = ccid_descriptor->readTimeout;

	/* use default timeout since the reader may not be present anymore */
	ccid_descriptor->readTimeout = DEFAULT_COM_READ_TIMEOUT;

	/* if DEBUG_LEVEL_PERIODIC is not set we remove DEBUG_LEVEL_COMM */
	oldLogLevel = LogLevel;
	if (! (LogLevel & DEBUG_LEVEL_PERIODIC))
		LogLevel &= ~DEBUG_LEVEL_COMM;

	return_value = CmdGetSlotStatus(reader_index, pcbuffer);

	/* set back the old timeout */
	ccid_descriptor->readTimeout = oldReadTimeout;

	/* set back the old LogLevel */
	LogLevel = oldLogLevel;

	if (return_value != IFD_SUCCESS)
		return return_value;

	return_value = IFD_COMMUNICATION_ERROR;
	switch (pcbuffer[7] & CCID_ICC_STATUS_MASK)	/* bStatus */
	{
		case CCID_ICC_PRESENT_ACTIVE:
			return_value = IFD_ICC_PRESENT;
			/* use default slot */
			break;

		case CCID_ICC_PRESENT_INACTIVE:
			if ((CcidSlots[reader_index].bPowerFlags == POWERFLAGS_RAZ)
				|| (CcidSlots[reader_index].bPowerFlags & MASK_POWERFLAGS_PDWN))
				/* the card was previously absent */
				return_value = IFD_ICC_PRESENT;
			else
			{
				/* the card was previously present but has been
				 * removed and inserted between two consecutive
				 * IFDHICCPresence() calls */
				CcidSlots[reader_index].bPowerFlags = POWERFLAGS_RAZ;
				return_value = IFD_ICC_NOT_PRESENT;
			}
			break;

		case CCID_ICC_ABSENT:
			/* Reset ATR buffer */
			CcidSlots[reader_index].nATRLength = 0;
			*CcidSlots[reader_index].pcATRBuffer = '\0';

			/* Reset PowerFlags */
			CcidSlots[reader_index].bPowerFlags = POWERFLAGS_RAZ;

			return_value = IFD_ICC_NOT_PRESENT;
			break;
	}

#if 0
	/* SCR331-DI contactless reader */
	if (((SCR331DI == ccid_descriptor->readerID)
		|| (SDI010 == ccid_descriptor->readerID)
		|| (SCR331DINTTCOM == ccid_descriptor->readerID))
		&& (ccid_descriptor->bCurrentSlotIndex > 0))
	{
		unsigned char cmd[] = { 0x11 };
		/*  command: 11 ??
		 * response: 00 11 01 ?? no card
		 *           01 04 00 ?? card present */

		unsigned char res[10];
		unsigned int length_res = sizeof(res);
		RESPONSECODE ret;

		/* if DEBUG_LEVEL_PERIODIC is not set we remove DEBUG_LEVEL_COMM */
		oldLogLevel = LogLevel;
		if (! (LogLevel & DEBUG_LEVEL_PERIODIC))
			LogLevel &= ~DEBUG_LEVEL_COMM;

		ret = CmdEscape(reader_index, cmd, sizeof(cmd), res, &length_res, 0);

		/* set back the old LogLevel */
		LogLevel = oldLogLevel;

		if (ret != IFD_SUCCESS)
		{
			DEBUG_INFO1("CmdEscape failed");
			/* simulate a card absent */
			res[0] = 0;
		}

		if (0x01 == res[0])
			return_value = IFD_ICC_PRESENT;
		else
		{
			/* Reset ATR buffer */
			CcidSlots[reader_index].nATRLength = 0;
			*CcidSlots[reader_index].pcATRBuffer = '\0';

			/* Reset PowerFlags */
			CcidSlots[reader_index].bPowerFlags = POWERFLAGS_RAZ;

			return_value = IFD_ICC_NOT_PRESENT;
		}
	}
#endif

end:
	DEBUG_PERIODIC2("Card %s",
		IFD_ICC_PRESENT == return_value ? "present" : "absent");

	return return_value;
} /* IFDHICCPresence */


CcidDesc *get_ccid_slot(unsigned int reader_index)
{
	return &CcidSlots[reader_index];
} /* get_ccid_slot */


void init_driver(void)
{
	char infofile[FILENAME_MAX];
	char *e;
	int rv;
	list_t plist, *values;

	DEBUG_INFO1("Driver version: " VERSION);

	/* Info.plist full patch filename */
	(void)snprintf(infofile, sizeof(infofile), "%s/%s/Contents/Info.plist",
		PCSCLITE_HP_DROPDIR, BUNDLE);

	rv = bundleParse(infofile, &plist);
	if (0 == rv)
	{
		/* Log level */
		rv = LTPBundleFindValueWithKey(&plist, "ifdLogLevel", &values);
		if (0 == rv)
		{
			/* convert from hex or dec or octal */
			LogLevel = strtoul(list_get_at(values, 0), NULL, 0);

			/* print the log level used */
			DEBUG_INFO2("LogLevel: 0x%.4X", LogLevel);
		}

		/* Driver options */
		rv = LTPBundleFindValueWithKey(&plist, "ifdDriverOptions", &values);
		if (0 == rv)
		{
			/* convert from hex or dec or octal */
			DriverOptions = strtoul(list_get_at(values, 0), NULL, 0);

			/* print the log level used */
			DEBUG_INFO2("DriverOptions: 0x%.4X", DriverOptions);
		}

		bundleRelease(&plist);
	}

	e = getenv("LIBCCID_ifdLogLevel");
	if (e)
	{
		/* convert from hex or dec or octal */
		LogLevel = strtoul(e, NULL, 0);

		/* print the log level used */
		DEBUG_INFO2("LogLevel from LIBCCID_ifdLogLevel: 0x%.4X", LogLevel);
	}

	/* get the voltage parameter */
	switch ((DriverOptions >> 4) & 0x03)
	{
		case 0:
			PowerOnVoltage = VOLTAGE_5V;
			break;

		case 1:
			PowerOnVoltage = VOLTAGE_3V;
			break;

		case 2:
			PowerOnVoltage = VOLTAGE_1_8V;
			break;

		case 3:
			PowerOnVoltage = VOLTAGE_AUTO;
			break;
	}

	/* initialise the Lun to reader_index mapping */
	InitReaderIndex();

	DebugInitialized = TRUE;
} /* init_driver */


void extra_egt(ATR_t *atr, _ccid_descriptor *ccid_desc, DWORD Protocol)
{
	/* This function use an EGT value for cards who comply with followings
	 * criterias:
	 * - TA1 > 11
	 * - current EGT = 0x00 or 0xFF
	 * - T=0 or (T=1 and CWI >= 2)
	 *
	 * Without this larger EGT some non ISO 7816-3 smart cards may not
	 * communicate with the reader.
	 *
	 * This modification is harmless, the reader will just be less restrictive
	 */

	unsigned int card_baudrate;
	unsigned int default_baudrate;
	double f, d;

	/* if TA1 not present */
	if (! atr->ib[0][ATR_INTERFACE_BYTE_TA].present)
		return;

	(void)ATR_GetParameter(atr, ATR_PARAMETER_D, &d);
	(void)ATR_GetParameter(atr, ATR_PARAMETER_F, &f);

	/* may happen with non ISO cards */
	if ((0 == f) || (0 == d))
		return;

	/* Baudrate = f x D/F */
	card_baudrate = (unsigned int) (1000 * ccid_desc->dwDefaultClock * d / f);

	default_baudrate = (unsigned int) (1000 * ccid_desc->dwDefaultClock
		* ATR_DEFAULT_D / ATR_DEFAULT_F);

	/* TA1 > 11? */
	if (card_baudrate <= default_baudrate)
		return;

	/* Current EGT = 0 or FF? */
	if (atr->ib[0][ATR_INTERFACE_BYTE_TC].present &&
		((0x00 == atr->ib[0][ATR_INTERFACE_BYTE_TC].value) ||
		(0xFF == atr->ib[0][ATR_INTERFACE_BYTE_TC].value)))
	{
		if (SCARD_PROTOCOL_T0 == Protocol)
		{
			/* Init TC1 */
			atr->ib[0][ATR_INTERFACE_BYTE_TC].present = TRUE;
			atr->ib[0][ATR_INTERFACE_BYTE_TC].value = 2;
			DEBUG_INFO1("Extra EGT patch applied");
		}

		if (SCARD_PROTOCOL_T1 == Protocol)
		{
			int i;

			/* TBi (i>2) present? BWI/CWI */
			for (i=2; i<ATR_MAX_PROTOCOLS; i++)
			{
				/* CWI >= 2 ? */
				if (atr->ib[i][ATR_INTERFACE_BYTE_TB].present &&
					((atr->ib[i][ATR_INTERFACE_BYTE_TB].value & 0x0F) >= 2))
				{
					/* Init TC1 */
					atr->ib[0][ATR_INTERFACE_BYTE_TC].present = TRUE;
					atr->ib[0][ATR_INTERFACE_BYTE_TC].value = 2;
					DEBUG_INFO1("Extra EGT patch applied");

					/* only the first TBi (i>2) must be used */
					break;
				}
			}
		}
	}
} /* extra_egt */


static char find_baud_rate(unsigned int baudrate, unsigned int *list)
{
	int i;

	DEBUG_COMM2("Card baud rate: %d", baudrate);

	/* Does the reader support the announced smart card data speed? */
	for (i=0;; i++)
	{
		/* end of array marker */
		if (0 == list[i])
			break;

		DEBUG_COMM2("Reader can do: %d", list[i]);

		/* We must take into account that the card_baudrate integral value
		 * is an approximative result, computed from the d/f float result.
		 */
		if ((baudrate < list[i] + 2) && (baudrate > list[i] - 2))
			return TRUE;
	}

	return FALSE;
} /* find_baud_rate */


static unsigned int T0_card_timeout(double f, double d, int TC1, int TC2,
	int clock_frequency)
{
	unsigned int timeout = DEFAULT_COM_READ_TIMEOUT;
	double EGT, WWT;
	unsigned int t;

	/* Timeout applied on ISO_IN or ISO_OUT card exchange
	 * we choose the maximum computed value.
	 *
	 * ISO_IN timeout is the sum of:
	 * Terminal:					Smart card:
	 * 5 bytes header cmd  ->
	 *                    <-		Procedure byte
	 * 256 data bytes	   ->
	 *					  <-		SW1-SW2
	 * = 261 EGT       + 3 WWT     + 3 WWT
	 *
	 * ISO_OUT Timeout is the sum of:
	 * Terminal:                    Smart card:
	 * 5 bytes header cmd  ->
	 *					  <-        Procedure byte + 256 data bytes + SW1-SW2
	 * = 5 EGT          + 1 WWT     + 259 WWT
	 */

	/* clock_frequency is in kHz so the times are in milliseconds and not
	 * in seconds */

	/* may happen with non ISO cards */
	if ((0 == f) || (0 == d) || (0 == clock_frequency))
		return 60 * 1000;	/* 60 seconds */

	/* EGT */
	/* see ch. 6.5.3 Extra Guard Time, page 12 of ISO 7816-3 */
	EGT = 12 * f / d / clock_frequency + (f / d) * TC1 / clock_frequency;

	/* card WWT */
	/* see ch. 8.2 Character level, page 15 of ISO 7816-3 */
	WWT = 960 * TC2 * f / clock_frequency;

	/* ISO in */
	t  = 261 * EGT + (3 + 3) * WWT;
	if (timeout < t)
		timeout = t;

	/* ISO out */
	t = 5 * EGT + (1 + 259) * WWT;
	if (timeout < t)
		timeout = t;

	return timeout;
} /* T0_card_timeout  */


static unsigned int T1_card_timeout(double f, double d, int TC1,
	int BWI, int CWI, int clock_frequency)
{
	double EGT, BWT, CWT, etu;
	unsigned int timeout;

	/* Timeout applied on ISO in + ISO out card exchange
	 *
     * Timeout is the sum of:
	 * - ISO in delay between leading edge of the first character sent by the
	 *   interface device and the last one (NAD PCB LN APDU CKS) = 260 EGT,
	 * - delay between ISO in and ISO out = BWT,
	 * - ISO out delay between leading edge of the first character sent by the
	 *   card and the last one (NAD PCB LN DATAS CKS) = 260 CWT.
	 */

	/* clock_frequency is in kHz so the times are in milliseconds and not
	 * in seconds */

	/* may happen with non ISO cards */
	if ((0 == f) || (0 == d) || (0 == clock_frequency))
		return 60 * 1000;	/* 60 seconds */

	/* see ch. 6.5.2 Transmission factors F and D, page 12 of ISO 7816-3 */
	etu = f / d / clock_frequency;

	/* EGT */
	/* see ch. 6.5.3 Extra Guard Time, page 12 of ISO 7816-3 */
	EGT = 12 * etu + (f / d) * TC1 / clock_frequency;

	/* card BWT */
	/* see ch. 9.5.3.2 Block Waiting Time, page 20 of ISO 7816-3 */
	BWT = 11 * etu + (1<<BWI) * 960 * 372 / clock_frequency;

	/* card CWT */
	/* see ch. 9.5.3.1 Caracter Waiting Time, page 20 of ISO 7816-3 */
	CWT = (11 + (1<<CWI)) * etu;

	timeout = 260*EGT + BWT + 260*CWT;

	/* This is the card/reader timeout.  Add 1 second for the libusb
	 * timeout so we get the error from the reader. */
	timeout += 1000;

	return timeout;
} /* T1_card_timeout  */


static int get_IFSC(ATR_t *atr, int *idx)
{
	int i, ifsc, protocol = -1;

	/* default return values */
	ifsc = -1;
	*idx = -1;

	for (i=0; i<ATR_MAX_PROTOCOLS; i++)
	{
		/* TAi (i>2) present and protocol=1 => IFSC */
		if (i >= 2 && protocol == 1
			&& atr->ib[i][ATR_INTERFACE_BYTE_TA].present)
		{
			ifsc = atr->ib[i][ATR_INTERFACE_BYTE_TA].value;
			*idx = i+1;
			/* only the first TAi (i>2) must be used */
			break;
		}

		/* protocol T=? */
		if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present)
			protocol = atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F;
	}

	if (ifsc > 254)
	{
		/* 0xFF is not a valid value for IFSC */
		DEBUG_INFO2("Non ISO IFSC: 0x%X", ifsc);
		ifsc = 254;
	}

	return ifsc;
} /* get_IFSC */

