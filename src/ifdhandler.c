/*
    ifdhandler.c: IFDH API
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

/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <PCSC/pcsclite.h>
#include <PCSC/ifdhandler.h>

#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "config.h"
#include "debug.h"
#include "utils.h"
#include "commands.h"
#include "towitoko/atr.h"
#include "towitoko/pps.h"
#include "parser.h"

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
static int DebugInitialized = FALSE;

/* local functions */
static void init_driver(void);


RESPONSECODE IFDHCreateChannelByName(DWORD Lun, LPTSTR lpcDevice)
{
	RESPONSECODE return_value = IFD_SUCCESS;

	if (! DebugInitialized)
		init_driver();

	DEBUG_INFO3("lun: %X, device: %s", Lun, lpcDevice);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	/* Reset ATR buffer */
	CcidSlots[LunToReaderIndex(Lun)].nATRLength = 0;
	*CcidSlots[LunToReaderIndex(Lun)].pcATRBuffer = '\0';

	/* Reset PowerFlags */
	CcidSlots[LunToReaderIndex(Lun)].bPowerFlags = POWERFLAGS_RAZ;

#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&ifdh_context_mutex);
#endif

	if (OpenPortByName(Lun, lpcDevice) != STATUS_SUCCESS)
	{
		DEBUG_CRITICAL("failed");
		return_value = IFD_COMMUNICATION_ERROR;
	}

	/* Maybe we have a special treatment for this reader */
	ccid_open_hack(Lun);

#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&ifdh_context_mutex);
#endif

	return return_value;
} /* IFDHCreateChannelByName */


RESPONSECODE IFDHCreateChannel(DWORD Lun, DWORD Channel)
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
	RESPONSECODE return_value = IFD_SUCCESS;

	if (! DebugInitialized)
		init_driver();

	DEBUG_INFO2("lun: %X", Lun);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	/* Reset ATR buffer */
	CcidSlots[LunToReaderIndex(Lun)].nATRLength = 0;
	*CcidSlots[LunToReaderIndex(Lun)].pcATRBuffer = '\0';

	/* Reset PowerFlags */
	CcidSlots[LunToReaderIndex(Lun)].bPowerFlags = POWERFLAGS_RAZ;

#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&ifdh_context_mutex);
#endif

	if (OpenPort(Lun, Channel) != STATUS_SUCCESS)
	{
		DEBUG_CRITICAL("failed");
		return_value = IFD_COMMUNICATION_ERROR;
	}

	/* Maybe we have a special treatment for this reader */
	ccid_open_hack(Lun);

#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&ifdh_context_mutex);
#endif

	return return_value;
} /* IFDHCreateChannel */


RESPONSECODE IFDHCloseChannel(DWORD Lun)
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

	DEBUG_INFO2("lun: %X", Lun);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	(void)CmdPowerOff(Lun);
	/* No reader status check, if it failed, what can you do ? :) */

#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&ifdh_context_mutex);
#endif

	(void)ClosePort(Lun);

#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&ifdh_context_mutex);
#endif

	return IFD_SUCCESS;
} /* IFDHCloseChannel */


RESPONSECODE IFDHGetCapabilities(DWORD Lun, DWORD Tag,
	PDWORD Length, PUCHAR Value)
{
	/*
	 * This function should get the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information requested example: TAG_IFD_ATR -
	 * return the Atr and it's size (required). these tags are defined in
	 * ifdhandler.h
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG
	 */

	DEBUG_INFO3("lun: %X, tag: 0x%X", Lun, Tag);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	switch (Tag)
	{
		case TAG_IFD_ATR:
		case SCARD_ATTR_ATR_STRING:
			/* If Length is not zero, powerICC has been performed.
			 * Otherwise, return NULL pointer
			 * Buffer size is stored in *Length */
			*Length = (*Length < CcidSlots[LunToReaderIndex(Lun)].nATRLength) ?
				*Length : CcidSlots[LunToReaderIndex(Lun)].nATRLength;

			if (*Length)
				memcpy(Value, CcidSlots[LunToReaderIndex(Lun)]
					.pcATRBuffer, *Length);
			break;

#ifdef HAVE_PTHREAD
		case TAG_IFD_SIMULTANEOUS_ACCESS:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = CCID_DRIVER_MAX_READERS;
			}
			break;

		case TAG_IFD_THREAD_SAFE:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = 1; /* Can talk to multiple readers at the same time */
			}
			break;
#endif

		case TAG_IFD_SLOTS_NUMBER:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = 1; /* One slot only */
			}
			break;

		case IOCTL_SMARTCARD_VENDOR_VERIFY_PIN:
			if (*Length >= 1)
			{
				*Length = 1;
				*Value = get_ccid_descriptor(Lun) -> bPINSupport & CCID_CLASS_PIN_VERIFY;
			}
			break;

		default:
			return IFD_ERROR_TAG;
	}

	return IFD_SUCCESS;
} /* IFDHGetCapabilities */


RESPONSECODE IFDHSetCapabilities(DWORD Lun, DWORD Tag,
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

	/* By default, say it worked */

	DEBUG_INFO3("lun: %X, tag: 0x%X", Lun, Tag);

	/* if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR; */

	return IFD_NOT_SUPPORTED;
} /* IFDHSetCapabilities */


RESPONSECODE IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol,
	UCHAR Flags, UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
	/*
	 * This function should set the PTS of a particular card/slot using
	 * the three PTS parameters sent
	 *
	 * Protocol - 0 .... 14 T=0 .... T=14
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
	ATR atr;
	unsigned int len;
	int convention;

	/* Set ccid desc params */
	CcidDesc *ccid_slot;
	_ccid_descriptor *ccid_desc;

	DEBUG_INFO3("lun: %X, protocol T=%d", Lun, Protocol-1);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	/* Set to zero buffer */
	memset(pps, 0, sizeof(pps));
	memset(&atr, 0, sizeof(atr));

	/* Get ccid params */
	ccid_slot = get_ccid_slot(Lun);
	ccid_desc = get_ccid_descriptor(Lun);

	/* Get ATR of the card */
	ATR_InitFromArray(&atr, ccid_slot->pcATRBuffer, ccid_slot->nATRLength);

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
					t1_set_param(t1, IFD_PROTOCOL_T1_CHECKSUM_LRC, 0);
				}
				else
					if (1 == atr.ib[i][ATR_INTERFACE_BYTE_TC].value)
					{
						DEBUG_COMM("Use CRC");
						t1_set_param(t1, IFD_PROTOCOL_T1_CHECKSUM_CRC, 0);
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
		/* PPS not negociated by reader, and TA1 present */
		if (atr.ib[0][ATR_INTERFACE_BYTE_TA].present &&
			! (ccid_desc->dwFeatures & CCID_CLASS_AUTO_PPS_CUR))
		{
			unsigned int card_baudrate;
			unsigned int default_baudrate;
			double f, d;

			ATR_GetParameter(&atr, ATR_PARAMETER_D, &d);
			ATR_GetParameter(&atr, ATR_PARAMETER_F, &f);

			/* Baudrate = f x D/F */
			card_baudrate = (unsigned int) (1000 * ccid_desc->dwDefaultClock
				* d / f);

			default_baudrate = (unsigned int) (1000 * ccid_desc->dwDefaultClock
				* ATR_DEFAULT_D / ATR_DEFAULT_F);

			/* if the reader is fast enough */
			if ((card_baudrate < ccid_desc->dwMaxDataRate)
				/* and the card does not try to lower the default speed */
				&& (card_baudrate > default_baudrate ))
			{
				pps[1] |= 0x10; /* PTS1 presence */
				pps[2] = atr.ib[0][ATR_INTERFACE_BYTE_TA].value;

				DEBUG_COMM2("Set speed to %d bauds", card_baudrate);
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

		if (ATR_MALFORMED == ATR_GetDefaultProtocol(&atr, &default_protocol))
			return IFD_PROTOCOL_NOT_SUPPORTED;

		/* if the requested protocol is not the default one
		 * or a TA1/PPS1 is present */
		if (((pps[1] & 0x0F) != default_protocol) || (PPS_HAS_PPS1(pps)))
			if (PPS_Exchange(Lun, pps, &len, &pps[2]) != PPS_OK)
			{
				DEBUG_INFO("PPS_Exchange Failed");

				return IFD_ERROR_PTS_FAILURE;
			}
	}

	/* Now we must set the reader parameters */
	ATR_GetConvention(&atr, &convention);

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
			0x4D,	/* BWI/BCI		*/
			0x00,	/* ClockStop	*/
			0x20,	/* IFSC			*/
			0x00	/* NADValue		*/
		};
		int i;

		/* TA1 is not default */
		if (PPS_HAS_PPS1(pps))
			param[0] = pps[2];

		if (ATR_CONVENTION_INVERSE == convention)
			param[1] &= 0x02;

		/* get TC1 Extra guard time */
		if (atr.ib[0][ATR_INTERFACE_BYTE_TC].present)
			param[2] = atr.ib[0][ATR_INTERFACE_BYTE_TC].value;

		/* TBi (i>2) present? BWI/BCI */
		for (i=2; i<ATR_MAX_PROTOCOLS; i++)
			if (atr.ib[i][ATR_INTERFACE_BYTE_TB].present)
			{
				DEBUG_COMM3("BWI/BCI (TB%d) present: %d", i+1,
					atr.ib[i][ATR_INTERFACE_BYTE_TB].value);
				param[3] = atr.ib[i][ATR_INTERFACE_BYTE_TB].value;

				/* only the first TBi (i>2) must be used */
				break;
			}

		if (IFD_SUCCESS != SetParameters(Lun, 1, sizeof(param), param))
			return IFD_COMMUNICATION_ERROR;
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

		/* TA1 is not default */
		if (PPS_HAS_PPS1(pps))
			param[0] = pps[2];

		if (ATR_CONVENTION_INVERSE == convention)
			param[1] &= 0x02;

		/* get TC1 Extra guard time */
		if (atr.ib[0][ATR_INTERFACE_BYTE_TC].present)
			param[2] = atr.ib[0][ATR_INTERFACE_BYTE_TC].value;

		/* TC2 WWT */
		if (atr.ib[1][ATR_INTERFACE_BYTE_TC].present)
			param[3] = atr.ib[1][ATR_INTERFACE_BYTE_TC].value;

		if (IFD_SUCCESS != SetParameters(Lun, 0, sizeof(param), param))
			return IFD_COMMUNICATION_ERROR;
	}

	/* set IFSC & IFSD in T=1 */
	if (SCARD_PROTOCOL_T1 == Protocol)
	{
		t1_state_t *t1 = &(ccid_slot -> t1);
		int i;

		/* TAi (i>2) present? */
		for (i=2; i<ATR_MAX_PROTOCOLS; i++)
			if (atr.ib[i][ATR_INTERFACE_BYTE_TA].present)
			{
				DEBUG_COMM3("IFSC (TA%d) present: %d", i+1,
					atr.ib[i][ATR_INTERFACE_BYTE_TA].value);
				t1_set_param(t1, IFD_PROTOCOL_T1_IFSC,
					atr.ib[i][ATR_INTERFACE_BYTE_TA].value);

				/* only the first TAi (i>2) must be used */
				break;
			}

		/* IFSD not negociated by the reader? */
		if (! (ccid_desc->dwFeatures & CCID_CLASS_AUTO_IFSD))
		{
			DEBUG_COMM2("Negociate IFSD at %d",  ccid_desc -> dwMaxIFSD);
			if (t1_negociate_ifsd(t1, 0, ccid_desc -> dwMaxIFSD) < 0)
				return IFD_COMMUNICATION_ERROR;
		}
		t1_set_param(t1, IFD_PROTOCOL_T1_IFSD, ccid_desc -> dwMaxIFSD);

		DEBUG_COMM3("T=1: IFSC=%d, IFSD=%d", t1->ifsc, t1->ifsd);
	}

	return IFD_SUCCESS;
} /* IFDHSetProtocolParameters */


RESPONSECODE IFDHPowerICC(DWORD Lun, DWORD Action,
	PUCHAR Atr, PDWORD AtrLength)
{
	/*
	 * This function controls the power and reset signals of the smartcard
	 * reader at the particular reader/slot specified by Lun.
	 *
	 * Action - Action to be taken on the card.
	 *
	 * IFD_POWER_UP - Power and reset the card if not done so (store the
	 * ATR and return it and it's length).
	 *
	 * IFD_POWER_DOWN - Power down the card if not done already
	 * (Atr/AtrLength should be zero'd)
	 *
	 * IFD_RESET - Perform a quick reset on the card.  If the card is not
	 * powered power up the card.  (Store and return the Atr/Length)
	 *
	 * Atr - Answer to Reset of the card.  The driver is responsible for
	 * caching this value in case IFDHGetCapabilities is called requesting
	 * the ATR and it's length.  This should not exceed MAX_ATR_SIZE.
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
	unsigned char pcbuffer[RESP_BUF_SIZE];

	DEBUG_INFO2("lun: %X", Lun);

	/* By default, assume it won't work :) */
	*AtrLength = 0;

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	switch (Action)
	{
		case IFD_POWER_DOWN:
			/* Clear ATR buffer */
			CcidSlots[LunToReaderIndex(Lun)].nATRLength = 0;
			*CcidSlots[LunToReaderIndex(Lun)].pcATRBuffer = '\0';

			/* Memorise the request */
			CcidSlots[LunToReaderIndex(Lun)].bPowerFlags |=
				MASK_POWERFLAGS_PDWN;

			/* send the command */
			if (IFD_SUCCESS != CmdPowerOff(Lun))
			{
				DEBUG_CRITICAL("PowerDown failed");
				return_value = IFD_ERROR_POWER_ACTION;
				goto end;
			}

			/* clear T=1 context */
			t1_release(&(get_ccid_slot(Lun) -> t1));
			break;

		case IFD_POWER_UP:
		case IFD_RESET:
			nlength = sizeof(pcbuffer);
			if (CmdPowerOn(Lun, &nlength, pcbuffer) != IFD_SUCCESS)
			{
				DEBUG_CRITICAL("PowerUp failed");
				return_value = IFD_ERROR_POWER_ACTION;
				goto end;
			}

			/* Power up successful, set state variable to memorise it */
			CcidSlots[LunToReaderIndex(Lun)].bPowerFlags |=
				MASK_POWERFLAGS_PUP;
			CcidSlots[LunToReaderIndex(Lun)].bPowerFlags &=
				~MASK_POWERFLAGS_PDWN;

			/* Reset is returned, even if TCK is wrong */
			CcidSlots[LunToReaderIndex(Lun)].nATRLength = *AtrLength =
				(nlength < MAX_ATR_SIZE) ? nlength : MAX_ATR_SIZE;
			memcpy(Atr, pcbuffer, *AtrLength);
			memcpy(CcidSlots[LunToReaderIndex(Lun)].pcATRBuffer,
				pcbuffer, *AtrLength);

			/* initialise T=1 context */
			t1_init(&(get_ccid_slot(Lun) -> t1), Lun);
			break;

		default:
			DEBUG_CRITICAL("Action not supported");
			return_value = IFD_NOT_SUPPORTED;
	}
end:

	return return_value;
} /* IFDHPowerICC */


RESPONSECODE IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci,
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

	DEBUG_INFO2("lun: %X", Lun);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	rx_length = *RxLength;
	return_value = CmdXfrBlock(Lun, TxLength, TxBuffer, &rx_length, RxBuffer,
		SendPci.Protocol);
	*RxLength = rx_length;

	return return_value;
} /* IFDHTransmitToICC */


RESPONSECODE IFDHControl(DWORD Lun, DWORD dwControlCode, PUCHAR TxBuffer,
	DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength, PDWORD pdwBytesReturned)
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
	RESPONSECODE return_value = IFD_COMMUNICATION_ERROR;

	DEBUG_INFO3("lun: %X, ControlCode: 0x%X", Lun, dwControlCode);

	if (CheckLun(Lun) || (NULL == pdwBytesReturned) || (NULL == RxBuffer))
		return return_value;

	/* Set the return length to 0 to avoid problems */
	*pdwBytesReturned = 0;

	if (IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE == dwControlCode)
	{
		if (FALSE == (DriverOptions & DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED))
		{
			DEBUG_INFO("ifd exchange (Escape command) not allowed");
			return_value = IFD_COMMUNICATION_ERROR;
		}
		else
		{
			unsigned int iBytesReturned;

			iBytesReturned = RxLength;
			return_value = CmdEscape(Lun, TxBuffer, TxLength, RxBuffer,
				&iBytesReturned);
			*pdwBytesReturned = iBytesReturned;
		}
	}

	if (IOCTL_SMARTCARD_VENDOR_VERIFY_PIN == dwControlCode)
	{
		unsigned int iBytesReturned;

		iBytesReturned = RxLength;
		return_value = SecurePIN(Lun, TxBuffer, TxLength, RxBuffer,
			&iBytesReturned);
		*pdwBytesReturned = iBytesReturned;
	}

	return return_value;
} /* IFDHControl */


RESPONSECODE IFDHICCPresence(DWORD Lun)
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

	DEBUG_PERIODIC2("lun: %X", Lun);

	if (CheckLun(Lun))
		return IFD_COMMUNICATION_ERROR;

	/* if DEBUG_LEVEL_PERIODIC is not set we remove DEBUG_LEVEL_COMM */
	oldLogLevel = LogLevel;
	if (! (LogLevel & DEBUG_LEVEL_PERIODIC))
		LogLevel &= ~DEBUG_LEVEL_COMM;

	return_value = CmdGetSlotStatus(Lun, pcbuffer);

	/* set back the old LogLevel */
	LogLevel = oldLogLevel;

	if (return_value != IFD_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	return_value = IFD_COMMUNICATION_ERROR;
	switch (pcbuffer[7])	/* bStatus */
	{
		case 0x00:
		case 0x01:
			return_value = IFD_ICC_PRESENT;
			break;

		case 0x02:
			/* Reset ATR buffer */
			CcidSlots[LunToReaderIndex(Lun)].nATRLength = 0;
			*CcidSlots[LunToReaderIndex(Lun)].pcATRBuffer = '\0';

			/* Reset PowerFlags */
			CcidSlots[LunToReaderIndex(Lun)].bPowerFlags = POWERFLAGS_RAZ;

			return_value = IFD_ICC_NOT_PRESENT;
			break;
	}

	return return_value;
} /* IFDHICCPresence */


CcidDesc *get_ccid_slot(unsigned int lun)
{
	return &CcidSlots[LunToReaderIndex(lun)];
} /* get_ccid_slot */


void init_driver(void)
{
	char keyValue[TOKEN_MAX_VALUE_SIZE];
	char infofile[FILENAME_MAX];

	/* Info.plist full patch filename */
	snprintf(infofile, sizeof(infofile), "%s/%s/Contents/Info.plist",
		PCSCLITE_HP_DROPDIR, BUNDLE);

	/* Log level */
	if (0 == LTPBundleFindValueWithKey(infofile, "ifdLogLevel", keyValue, 0))
	{
		/* convert from hex or dec or octal */
		LogLevel = strtoul(keyValue, NULL, 0);

		/* print the log level used */
		debug_msg("%s:%d:%s LogLevel: 0x%.4X", __FILE__, __LINE__, __FUNCTION__,
			LogLevel);
	}

	/* Driver options */
	if (0 == LTPBundleFindValueWithKey(infofile, "ifdDriverOptions", keyValue, 0))
	{
		/* convert from hex or dec or octal */
		DriverOptions = strtoul(keyValue, NULL, 0);

		/* print the log level used */
		debug_msg("%s:%d:%s DriverOptions: 0x%.4X", __FILE__, __LINE__,
			__FUNCTION__, DriverOptions);
	}

	DebugInitialized = TRUE;
} /* init_driver */

