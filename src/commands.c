/*
    commands.c: Commands sent to the card
    Copyright (C) 2003-2010   Ludovic Rousseau
    Copyright (C) 2005 Martin Paljak

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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pcsclite.h>
#include <ifdhandler.h>
#include <reader.h>

#include "misc.h"
#include "commands.h"
#include "openct/proto-t1.h"
#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "config.h"
#include "debug.h"
#include "ccid_usb.h"

/* All the pinpad readers I used are more or less bogus
 * I use code to change the user command and make the firmware happy */
#define BOGUS_PINPAD_FIRMWARE

/* The firmware of SCM readers reports dwMaxCCIDMessageLength = 263
 * instead of 270 so this prevents from sending a full length APDU
 * of 260 bytes since the driver check this value */
#define BOGUS_SCM_FIRMWARE_FOR_dwMaxCCIDMessageLength

#define max( a, b )   ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* internal functions */
static RESPONSECODE CmdXfrBlockAPDU_extended(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[]);

static RESPONSECODE CmdXfrBlockTPDU_T0(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[]);

static RESPONSECODE CmdXfrBlockCHAR_T0(unsigned int reader_index, unsigned int
	tx_length, unsigned char tx_buffer[], unsigned int *rx_length, unsigned
	char rx_buffer[]);

static RESPONSECODE CmdXfrBlockTPDU_T1(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[]);

static void i2dw(int value, unsigned char *buffer);


/*****************************************************************************
 *
 *					CmdPowerOn
 *
 ****************************************************************************/
RESPONSECODE CmdPowerOn(unsigned int reader_index, unsigned int * nlength,
	unsigned char buffer[], int voltage)
{
	unsigned char cmd[10];
	status_t res;
	int length, count = 1;
	unsigned int atr_len;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

#ifndef TWIN_SERIAL
	if (ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		int r;
		unsigned char pcbuffer[SIZE_GET_SLOT_STATUS];

		/* first power off to reset the ICC state machine */
		r = CmdPowerOff(reader_index);
		if (r != IFD_SUCCESS)
			return r;

		/* wait for ready */
		r = CmdGetSlotStatus(reader_index, pcbuffer);
		if (r != IFD_SUCCESS)
			return r;

		/* Power On */
		r = ControlUSB(reader_index, 0xA1, 0x62, 0, buffer, *nlength);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Power On failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		*nlength = r;

		return IFD_SUCCESS;
	}

	if (ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		int r;
		unsigned char tmp[MAX_ATR_SIZE+1];

		/* first power off to reset the ICC state machine */
		r = CmdPowerOff(reader_index);
		if (r != IFD_SUCCESS)
			return r;

		/* Power On */
		r = ControlUSB(reader_index, 0x21, 0x62, 1, NULL, 0);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Power On failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		/* Data Block */
		r = ControlUSB(reader_index, 0xA1, 0x6F, 0, tmp, sizeof(tmp));

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Data Block failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		if (tmp[0] != 0x00)
		{
			DEBUG_CRITICAL2("bResponseType: 0x%02X", tmp[0]);

			/* Status Information? */
			if (0x40 == tmp[0])
				ccid_error(tmp[2], __FILE__, __LINE__, __FUNCTION__);
			return IFD_COMMUNICATION_ERROR;
		}

		DEBUG_INFO_XXD("Data Block: ", tmp, r);
		if (*nlength > r-1)
			*nlength = r-1;
		memcpy(buffer, tmp+1, *nlength);

		return IFD_SUCCESS;
	}
#endif

	/* store length of buffer[] */
	length = *nlength;

	if (ccid_descriptor->dwFeatures & CCID_CLASS_AUTO_VOLTAGE)
		voltage = 0;	/* automatic voltage selection */
	else
	{
		int bVoltageSupport = ccid_descriptor->bVoltageSupport;

		if ((1 == voltage) && !(bVoltageSupport & 1))
		{
			DEBUG_INFO("5V requested but not support by reader");
			voltage = 2;	/* 3V */
		}

		if ((2 == voltage) && !(bVoltageSupport & 2))
		{
			DEBUG_INFO("3V requested but not support by reader");
			voltage = 3;	/* 1.8V */
		}

		if ((3 == voltage) && !(bVoltageSupport & 4))
		{
			DEBUG_INFO("1.8V requested but not support by reader");
			voltage = 0;	/* auto */
		}
	}

again:
	cmd[0] = 0x62; /* IccPowerOn */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = voltage;
	cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(reader_index, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	/* reset available buffer size */
	/* needed if we go back after a switch to ISO mode */
	*nlength = length;

	res = ReadPort(reader_index, nlength, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (*nlength < STATUS_OFFSET+1)
	{
		DEBUG_CRITICAL2("Not enough data received: %d bytes", *nlength);
		return IFD_COMMUNICATION_ERROR;
	}

	if (buffer[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */

		if (0xBB == buffer[ERROR_OFFSET] &&	/* Protocol error in EMV mode */
			((GEMPC433 == ccid_descriptor->readerID)
			|| (CHERRYXX33 == ccid_descriptor->readerID)))
		{
			unsigned char cmd_tmp[] = {0x1F, 0x01};
			unsigned char res_tmp[1];
			unsigned int res_length = sizeof(res_tmp);

			if ((return_value = CmdEscape(reader_index, cmd_tmp,
				sizeof(cmd_tmp), res_tmp, &res_length)) != IFD_SUCCESS)
				return return_value;

			/* avoid looping if we can't switch mode */
			if (count--)
				goto again;
			else
				DEBUG_CRITICAL("Can't set reader in ISO mode");
		}

		/* continue with 3 volts and 5 volts */
		if (voltage > 1)
		{
			const char *voltage_code[] = { "auto", "5V", "3V", "1.8V" };

			DEBUG_INFO3("Power up with %s failed. Try with %s.",
				voltage_code[voltage], voltage_code[voltage-1]);
			voltage--;
			goto again;
		}

		return IFD_COMMUNICATION_ERROR;
	}

	/* extract the ATR */
	atr_len = dw2i(buffer, 1);	/* ATR length */
	if (atr_len > *nlength)
		atr_len = *nlength;
	else
		*nlength = atr_len;

	memmove(buffer, buffer+10, atr_len);

	return return_value;
} /* CmdPowerOn */


/*****************************************************************************
 *
 *					SecurePINVerify
 *
 ****************************************************************************/
RESPONSECODE SecurePINVerify(unsigned int reader_index,
	unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength)
{
	unsigned char cmd[11+14+CMD_BUF_SIZE];
	unsigned int a, b;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	int old_read_timeout;
	RESPONSECODE ret;

	cmd[0] = 0x69;	/* Secure */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = 0;		/* bBWI */
	cmd[8] = 0;		/* wLevelParameter */
	cmd[9] = 0;
	cmd[10] = 0;	/* bPINOperation: PIN Verification */

	/* 19 is the size of the PCSCv2 PIN verify structure
	 * The equivalent CCID structure is only 14-bytes long */
	if (TxLength > 19+CMD_BUF_SIZE) /* command too large? */
	{
		DEBUG_INFO3("Command too long: %d > %d", TxLength, 19+CMD_BUF_SIZE);
		return IFD_NOT_SUPPORTED;
	}

	if (TxLength < 19+4 /* 4 = APDU size */)	/* command too short? */
	{
		DEBUG_INFO3("Command too short: %d < %d", TxLength, 19+4);
		return IFD_NOT_SUPPORTED;
	}

	if (dw2i(TxBuffer, 15) + 19 != TxLength) /* ulDataLength field coherency */
	{
		DEBUG_INFO3("Wrong lengths: %d %d", dw2i(TxBuffer, 15) + 19, TxLength);
		return IFD_NOT_SUPPORTED;
	}

	/* make sure bEntryValidationCondition is valid
	 * The Cherry XX44 reader crashes with a wrong value */
	if ((0x00 == TxBuffer[7]) || (TxBuffer[7] > 0x07))
	{
		DEBUG_INFO2("Correct bEntryValidationCondition (was 0x%02X)",
			TxBuffer[7]);
		TxBuffer[7] = 0x02;
	}

#ifdef BOGUS_PINPAD_FIRMWARE
	/* bug circumvention for the GemPC Pinpad */
	if ((GEMPCPINPAD == ccid_descriptor->readerID)
		|| (VEGAALPHA == ccid_descriptor->readerID))
	{
		/* the firmware reject the cases: 00h No string and FFh default
		 * CCID message. The only value supported is 01h (display 1 message) */
		if (0x01 != TxBuffer[8])
		{
			DEBUG_INFO2("Correct bNumberMessage for GemPC Pinpad (was %d)",
				TxBuffer[8]);
			TxBuffer[8] = 0x01;
		}

		/* The reader does not support, and actively reject, "max size reached"
		 * and "timeout occured" validation conditions */
		if (0x02 != TxBuffer[7])
		{
			DEBUG_INFO2("Correct bEntryValidationCondition for GemPC Pinpad (was %d)",
				TxBuffer[7]);
			TxBuffer[7] = 0x02;	/* validation key pressed */
		}

	}

	if ((DELLSCRK == ccid_descriptor->readerID)
		|| (DELLSK == ccid_descriptor->readerID))
	{
		/* the firmware rejects the cases: 01h-FEh and FFh default
		 * CCID message. The only value supported is 00h (no message) */
		if (0x00 != TxBuffer[8])
		{
			DEBUG_INFO2("Correct bNumberMessage for Dell keyboard (was %d)",
				TxBuffer[8]);
			TxBuffer[8] = 0x00;
		}

		/* avoid the command rejection because the Enter key is still
		 * pressed. Wait a bit for the key to be released */
		(void)usleep(250*1000);
	}

	if (DELLSK == ccid_descriptor->readerID)
	{
		/* the 2 bytes of wPINMaxExtraDigit are reversed */
		int tmp;

		tmp = TxBuffer[6];
		TxBuffer[6] = TxBuffer[5];
		TxBuffer[5] = tmp;
		DEBUG_INFO("Correcting wPINMaxExtraDigit for Dell keyboard");
	}
#endif

	/* T=1 Protocol Management for a TPDU reader */
	if ((SCARD_PROTOCOL_T1 == ccid_descriptor->cardProtocol)
		&& (CCID_CLASS_TPDU == (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)))
	{
		ct_buf_t sbuf;
		unsigned char sdata[T1_BUFFER_SIZE];

		/* Initialize send buffer with the APDU */
		ct_buf_set(&sbuf,
			(void *)(TxBuffer + offsetof(PIN_VERIFY_STRUCTURE, abData)),
			TxLength - offsetof(PIN_VERIFY_STRUCTURE, abData));

		/* Create T=1 block */
		ret = t1_build(&((get_ccid_slot(reader_index))->t1),
			sdata, 0, T1_I_BLOCK, &sbuf, NULL);

		/* Increment the sequence numbers  */
		get_ccid_slot(reader_index)->t1.ns ^= 1;
		get_ccid_slot(reader_index)->t1.nr ^= 1;

		/* Copy the generated T=1 block prologue into the teoprologue
		 * of the CCID command */
		memcpy(TxBuffer + offsetof(PIN_VERIFY_STRUCTURE, bTeoPrologue),
			sdata, 3);
	}

	/* Build a CCID block from a PC/SC V2.02.05 Part 10 block */
	for (a = 11, b = 0; b < TxLength; b++)
	{
		if (1 == b) /* bTimeOut2 field */
			/* Ignore the second timeout as there's nothing we can do with
			 * it currently */
			continue;

		if ((b >= 15) && (b <= 18)) /* ulDataLength field (4 bytes) */
			/* the ulDataLength field is not present in the CCID frame
			 * so do not copy */
			continue;

		/* copy the CCID block 'verbatim' */
		cmd[a] = TxBuffer[b];
		a++;
	}

	/* SPR532 and Case 1 APDU */
	if ((SPR532 == ccid_descriptor->readerID)
		/* bmPINBlockString = 0 => PIN length not inserted in APDU */
		&& (0 == TxBuffer[3])
		/* case 1 APDU */
		&& (4 == TxBuffer[15]))
	{
		RESPONSECODE return_value;
		unsigned char cmd_tmp[] = { 0x80, 0x02, 0x00 };
		unsigned char res_tmp[1];
		unsigned int res_length = sizeof(res_tmp);

		/* the SPR532 will append the PIN code without any padding */
		return_value = CmdEscape(reader_index, cmd_tmp, sizeof(cmd_tmp),
			res_tmp, &res_length);
		if (return_value != IFD_SUCCESS)
			return return_value;

		/* we need to set bSeq again to avoid a "Duplicate frame detected"
		 * error since the bSeq of CmdEscape is now greater than bSeq set at
		 * the beginning of this function */
		cmd[6] = (*ccid_descriptor->pbSeq)++;
	}

	i2dw(a - 10, cmd + 1);  /* CCID message length */

	old_read_timeout = ccid_descriptor -> readTimeout;
	ccid_descriptor -> readTimeout = max(30, TxBuffer[0]+10);	/* at least 30 seconds */

	if (WritePort(reader_index, a, cmd) != STATUS_SUCCESS)
	{
		ret = IFD_COMMUNICATION_ERROR;
		goto end;
	}

	ret = CCID_Receive(reader_index, RxLength, RxBuffer, NULL);

	/* T=1 Protocol Management for a TPDU reader */
	if ((SCARD_PROTOCOL_T1 == ccid_descriptor->cardProtocol)
		&& (CCID_CLASS_TPDU == (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)))
	{
		/* timeout and cancel cases are faked by CCID_Receive() */
		if ((2 == *RxLength)
			/* the CCID command is rejected or failed */
		   || (IFD_SUCCESS != ret))
		{
			/* Decrement the sequence numbers since no TPDU was sent */
			get_ccid_slot(reader_index)->t1.ns ^= 1;
			get_ccid_slot(reader_index)->t1.nr ^= 1;
		}
		else
		{
			/* get only the T=1 data */
			/* FIXME: manage T=1 error blocks */
			memmove(RxBuffer, RxBuffer+3, *RxLength -4);
			*RxLength -= 4;	/* remove NAD, PCB, LEN and CRC */
		}
	}

end:
	ccid_descriptor -> readTimeout = old_read_timeout;
	return ret;
} /* SecurePINVerify */


/*****************************************************************************
 *
 *					SecurePINModify
 *
 ****************************************************************************/
RESPONSECODE SecurePINModify(unsigned int reader_index,
	unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength)
{
	unsigned char cmd[11+19+CMD_BUF_SIZE];
	unsigned int a, b;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	int old_read_timeout;
	RESPONSECODE ret;
#ifdef BOGUS_PINPAD_FIRMWARE
	int bNumberMessages = 0; /* for GemPC Pinpad */
#endif

	cmd[0] = 0x69;	/* Secure */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = 0;		/* bBWI */
	cmd[8] = 0;		/* wLevelParameter */
	cmd[9] = 0;
	cmd[10] = 1;	/* bPINOperation: PIN Modification */

	/* 24 is the size of the PCSC PIN modify structure
	 * The equivalent CCID structure is only 18 or 19-bytes long */
	if (TxLength > 24+CMD_BUF_SIZE) /* command too large? */
	{
		DEBUG_INFO3("Command too long: %d > %d", TxLength, 24+CMD_BUF_SIZE);
		return IFD_NOT_SUPPORTED;
	}

	if (TxLength < 24+4 /* 4 = APDU size */) /* command too short? */
	{
		DEBUG_INFO3("Command too short: %d < %d", TxLength, 24+4);
		return IFD_NOT_SUPPORTED;
	}

	if (dw2i(TxBuffer, 20) + 24 != TxLength) /* ulDataLength field coherency */
	{
		DEBUG_INFO3("Wrong lengths: %d %d", dw2i(TxBuffer, 20) + 24, TxLength);
		return IFD_NOT_SUPPORTED;
	}

	/* Make sure in the beginning if bNumberMessage is valid or not.
	 * 0xFF is the default value. */
	if ((TxBuffer[11] > 3) && (TxBuffer[11] != 0xFF))
	{
		DEBUG_INFO2("Wrong bNumberMessage: %d", TxBuffer[11]);
		return IFD_NOT_SUPPORTED;
	}

	/* Make sure bEntryValidationCondition is valid
	 * The Cherry XX44 reader crashes with a wrong value */
	if ((0x00 == TxBuffer[10]) || (TxBuffer[10] > 0x07))
	{
		DEBUG_INFO2("Correct bEntryValidationCondition (was 0x%02X)",
			TxBuffer[10]);
		TxBuffer[10] = 0x02;
	}

#ifdef BOGUS_PINPAD_FIRMWARE
	/* some firmwares are buggy so we try to "correct" the frame */
	/*
	 * SPR 532 and Cherry ST 2000C has no display but requires _all_
	 * bMsgIndex fields with bNumberMessage set to 0.
	 */
	if ((SPR532 == ccid_descriptor->readerID)
		|| (CHERRYST2000 == ccid_descriptor->readerID))
	{
		TxBuffer[11] = 0x03; /* set bNumberMessages to 3 so that
								all bMsgIndex123 are filled */
		TxBuffer[14] = TxBuffer[15] = TxBuffer[16] = 0;	/* bMsgIndex123 */
	}

	/* the bug is a bit different than for the Cherry ST 2000C
	 * with bNumberMessages < 3 the command seems to be accepted
	 * and the card sends 6B 80 */
	if (CHERRYXX44 == ccid_descriptor->readerID)
	{
		TxBuffer[11] = 0x03; /* set bNumberMessages to 3 so that
								all bMsgIndex123 are filled */
	}

	/* bug circumvention for the GemPC Pinpad */
	if ((GEMPCPINPAD == ccid_descriptor->readerID)
		|| (VEGAALPHA == ccid_descriptor->readerID))
	{
		/* The reader does not support, and actively reject, "max size reached"
		 * and "timeout occured" validation conditions */
		if (0x02 != TxBuffer[10])
		{
			DEBUG_INFO2("Correct bEntryValidationCondition for GemPC Pinpad (was %d)",
				TxBuffer[10]);
			TxBuffer[10] = 0x02;	/* validation key pressed */
		}

		/* the reader does not support any other value than 3 for the number
		 * of messages */
		bNumberMessages = TxBuffer[11];
		if (0x03 != TxBuffer[11])
		{
			DEBUG_INFO2("Correct bNumberMessages for GemPC Pinpad (was %d)",
				TxBuffer[11]);
			TxBuffer[11] = 0x03; /* 3 messages */
		}
	}
#endif

	/* T=1 Protocol Management for a TPDU reader */
	if ((SCARD_PROTOCOL_T1 == ccid_descriptor->cardProtocol)
		&& (CCID_CLASS_TPDU == (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)))
	{
		ct_buf_t sbuf;
		unsigned char sdata[T1_BUFFER_SIZE];

		/* Initialize send buffer with the APDU */
		ct_buf_set(&sbuf,
			(void *)(TxBuffer + offsetof(PIN_MODIFY_STRUCTURE, abData)),
			TxLength - offsetof(PIN_MODIFY_STRUCTURE, abData));

		/* Create T=1 block */
		ret = t1_build(&((get_ccid_slot(reader_index))->t1),
			sdata, 0, T1_I_BLOCK, &sbuf, NULL);

		/* Increment the sequence numbers  */
		get_ccid_slot(reader_index)->t1.ns ^= 1;
		get_ccid_slot(reader_index)->t1.nr ^= 1;

		/* Copy the generated T=1 block prologue into the teoprologue
		 * of the CCID command */
		memcpy(TxBuffer + offsetof(PIN_MODIFY_STRUCTURE, bTeoPrologue),
			sdata, 3);
	}

	/* Build a CCID block from a PC/SC V2.02.05 Part 10 block */

	/* Do adjustments as needed - CCID spec is not exact with some
	 * details in the format of the structure, per-reader adaptions
	 * might be needed.
	 */
	for (a = 11, b = 0; b < TxLength; b++)
	{
		if (1 == b) /* bTimeOut2 */
			/* Ignore the second timeout as there's nothing we can do with it
			 * currently */
			continue;

		if (15 == b) /* bMsgIndex2 */
		{
			/* in CCID the bMsgIndex2 is present only if bNumberMessage != 0 */
			if (0 == TxBuffer[11])
				continue;
		}

		if (16 == b) /* bMsgIndex3 */
		{
			/* in CCID the bMsgIndex3 is present only if bNumberMessage == 3 */
			if (TxBuffer[11] < 3)
				continue;
		}

		if ((b >= 20) && (b <= 23)) /* ulDataLength field (4 bytes) */
			/* the ulDataLength field is not present in the CCID frame
			 * so do not copy */
			continue;

		/* copy to the CCID block 'verbatim' */
		cmd[a] = TxBuffer[b];
		a++;
 	}

#ifdef BOGUS_PINPAD_FIRMWARE
	if ((SPR532 == ccid_descriptor->readerID)
		|| (CHERRYST2000 == ccid_descriptor->readerID))
	{
		cmd[21] = 0x00; /* set bNumberMessages to 0 */
	}

	if ((GEMPCPINPAD == ccid_descriptor->readerID)
		|| (VEGAALPHA == ccid_descriptor->readerID))
		cmd[21] = bNumberMessages;	/* restore the real value */
#endif

	/* We know the size of the CCID message now */
	i2dw(a - 10, cmd + 1);	/* command length (includes bPINOperation) */

	old_read_timeout = ccid_descriptor -> readTimeout;
	ccid_descriptor -> readTimeout = max(30, TxBuffer[0]+10);	/* at least 30 seconds */

	if (WritePort(reader_index, a, cmd) != STATUS_SUCCESS)
	{
 		ret = IFD_COMMUNICATION_ERROR;
		goto end;
	}

 	ret = CCID_Receive(reader_index, RxLength, RxBuffer, NULL);

	/* T=1 Protocol Management for a TPDU reader */
	if ((SCARD_PROTOCOL_T1 == ccid_descriptor->cardProtocol)
		&& (CCID_CLASS_TPDU == (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)))
	{
		/* timeout and cancel cases are faked by CCID_Receive() */
		if ((2 == *RxLength)
			/* the CCID command is rejected or failed */
			|| (IFD_SUCCESS != ret))
		{
			/* Decrement the sequence numbers since no TPDU was sent */
			get_ccid_slot(reader_index)->t1.ns ^= 1;
			get_ccid_slot(reader_index)->t1.nr ^= 1;
		}
		else
		{
			/* get only the T=1 data */
			/* FIXME: manage T=1 error blocks */
			memmove(RxBuffer, RxBuffer+3, *RxLength -4);
			*RxLength -= 4;	/* remove NAD, PCB, LEN and CRC */
		}
	}

end:
	ccid_descriptor -> readTimeout = old_read_timeout;
	return ret;
} /* SecurePINModify */


/*****************************************************************************
 *
 *					Escape
 *
 ****************************************************************************/
RESPONSECODE CmdEscape(unsigned int reader_index,
	const unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength)
{
	unsigned char *cmd_in, *cmd_out;
	status_t res;
	unsigned int length_in, length_out;
	RESPONSECODE return_value = IFD_SUCCESS;
	int old_read_timeout;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	old_read_timeout = ccid_descriptor -> readTimeout;
	ccid_descriptor -> readTimeout = 30;	/* 30 seconds */

again:
	/* allocate buffers */
	length_in = 10 + TxLength;
	if (NULL == (cmd_in = malloc(length_in)))
	{
		return_value = IFD_COMMUNICATION_ERROR;
		goto end;
	}

	length_out = 10 + *RxLength;
	if (NULL == (cmd_out = malloc(length_out)))
	{
		free(cmd_in);
		return_value = IFD_COMMUNICATION_ERROR;
		goto end;
	}

	cmd_in[0] = 0x6B; /* PC_to_RDR_Escape */
	i2dw(length_in - 10, cmd_in+1);	/* dwLength */
	cmd_in[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd_in[6] = (*ccid_descriptor->pbSeq)++;
	cmd_in[7] = cmd_in[8] = cmd_in[9] = 0; /* RFU */

	/* copy the command */
	memcpy(&cmd_in[10], TxBuffer, TxLength);

	res = WritePort(reader_index, length_in, cmd_in);
	free(cmd_in);
	if (res != STATUS_SUCCESS)
	{
		free(cmd_out);
		return_value = IFD_COMMUNICATION_ERROR;
		goto end;
	}

	res = ReadPort(reader_index, &length_out, cmd_out);

	/* replay the command if NAK
	 * This (generally) happens only for the first command sent to the reader
	 * with the serial protocol so it is not really needed for all the other
	 * ReadPort() calls */
	if (STATUS_COMM_NAK == res)
	{
		free(cmd_out);
		goto again;
	}

	if (res != STATUS_SUCCESS)
	{
		free(cmd_out);
		return_value = IFD_COMMUNICATION_ERROR;
		goto end;
	}

	if (length_out < STATUS_OFFSET+1)
	{
		DEBUG_CRITICAL2("Not enough data received: %d bytes", length_out);
		return_value = IFD_COMMUNICATION_ERROR;
		goto end;
	}

	if (cmd_out[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd_out[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	/* copy the response */
	length_out = dw2i(cmd_out, 1);
	if (length_out > *RxLength)
		length_out = *RxLength;
	*RxLength = length_out;
	memcpy(RxBuffer, &cmd_out[10], length_out);

	free(cmd_out);

end:
	ccid_descriptor -> readTimeout = old_read_timeout;
	return return_value;
} /* Escape */


/*****************************************************************************
 *
 *					CmdPowerOff
 *
 ****************************************************************************/
RESPONSECODE CmdPowerOff(unsigned int reader_index)
{
	unsigned char cmd[10];
	status_t res;
	unsigned int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

#ifndef TWIN_SERIAL
	if (ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		int r;

		/* PowerOff */
		r = ControlUSB(reader_index, 0x21, 0x63, 0, NULL, 0);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Power Off failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		return IFD_SUCCESS;
	}

	if (ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		int r;
		unsigned char buffer[3];

		/* PowerOff */
		r = ControlUSB(reader_index, 0x21, 0x63, 0, NULL, 0);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Power Off failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		/* SlotStatus */
		r = ControlUSB(reader_index, 0xA1, 0x81, 0, buffer, sizeof(buffer));

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC SlotStatus failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		return IFD_SUCCESS;
	}
#endif

	cmd[0] = 0x63; /* IccPowerOff */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(reader_index, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	res = ReadPort(reader_index, &length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (length < STATUS_OFFSET+1)
	{
		DEBUG_CRITICAL2("Not enough data received: %d bytes", length);
		return IFD_COMMUNICATION_ERROR;
	}

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdPowerOff */


/*****************************************************************************
 *
 *					CmdGetSlotStatus
 *
 ****************************************************************************/
RESPONSECODE CmdGetSlotStatus(unsigned int reader_index, unsigned char buffer[])
{
	unsigned char cmd[10];
	status_t res;
	unsigned int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

#ifndef TWIN_SERIAL
	if (ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		int r;
		unsigned char status[1];

again_status:
		/* SlotStatus */
		r = ControlUSB(reader_index, 0xA1, 0xA0, 0, status, sizeof(status));

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Slot Status failed: %s", strerror(errno));
			if (ENODEV == errno)
				return IFD_NO_SUCH_DEVICE;
			return IFD_COMMUNICATION_ERROR;
		}

		/* busy */
		if (status[0] & 0x40)
		{
			DEBUG_INFO2("Busy: 0x%02X", status[0]);
			(void)usleep(1000 * 10);
			goto again_status;
		}

		/* simulate a CCID bStatus */
		/* present and active by default */
		buffer[7] = CCID_ICC_PRESENT_ACTIVE;

		/* mute */
		if (0x80 == status[0])
			buffer[7] = CCID_ICC_ABSENT;

		/* store the status for CmdXfrBlockCHAR_T0() */
		buffer[0] = status[0];

		return IFD_SUCCESS;
	}

	if (ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		int r;
		unsigned char buffer_tmp[3];

		/* SlotStatus */
		r = ControlUSB(reader_index, 0xA1, 0x81, 0, buffer_tmp,
			sizeof(buffer_tmp));

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Slot Status failed: %s", strerror(errno));
			if (ENODEV == errno)
				return IFD_NO_SUCH_DEVICE;
			return IFD_COMMUNICATION_ERROR;
		}

		/* simulate a CCID bStatus */
		switch (buffer_tmp[1] & 0x03)
		{
			case 0:
				buffer[7] = CCID_ICC_PRESENT_ACTIVE;
				break;
			case 1:
				buffer[7] = CCID_ICC_PRESENT_INACTIVE;
				break;
			case 2:
			case 3:
				buffer[7] = CCID_ICC_ABSENT;
		}
		return IFD_SUCCESS;
	}
#endif

	cmd[0] = 0x65; /* GetSlotStatus */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(reader_index, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
	{
		if (STATUS_NO_SUCH_DEVICE == res)
			return IFD_NO_SUCH_DEVICE;
		return IFD_COMMUNICATION_ERROR;
	}

	length = SIZE_GET_SLOT_STATUS;
	res = ReadPort(reader_index, &length, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (length < STATUS_OFFSET+1)
	{
		DEBUG_CRITICAL2("Not enough data received: %d bytes", length);
		return IFD_COMMUNICATION_ERROR;
	}

	if ((buffer[STATUS_OFFSET] & CCID_COMMAND_FAILED)
		/* card absent or mute is not an communication error */
		&& (buffer[ERROR_OFFSET] != 0xFE))
	{
		return_value = IFD_COMMUNICATION_ERROR;
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
	}

	return return_value;
} /* CmdGetSlotStatus */


/*****************************************************************************
 *
 *					CmdXfrBlock
 *
 ****************************************************************************/
RESPONSECODE CmdXfrBlock(unsigned int reader_index, unsigned int tx_length,
	unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[], int protocol)
{
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	/* APDU or TPDU? */
	switch (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)
	{
		case CCID_CLASS_TPDU:
			if (protocol == T_0)
				return_value = CmdXfrBlockTPDU_T0(reader_index,
					tx_length, tx_buffer, rx_length, rx_buffer);
			else
				if (protocol == T_1)
					return_value = CmdXfrBlockTPDU_T1(reader_index, tx_length,
						tx_buffer, rx_length, rx_buffer);
				else
					return_value = IFD_PROTOCOL_NOT_SUPPORTED;
			break;

		case CCID_CLASS_SHORT_APDU:
			return_value = CmdXfrBlockTPDU_T0(reader_index,
				tx_length, tx_buffer, rx_length, rx_buffer);
			break;

		case CCID_CLASS_EXTENDED_APDU:
			return_value = CmdXfrBlockAPDU_extended(reader_index,
				tx_length, tx_buffer, rx_length, rx_buffer);
			break;

		case CCID_CLASS_CHARACTER:
			if (protocol == T_0)
				return_value = CmdXfrBlockCHAR_T0(reader_index, tx_length,
					tx_buffer, rx_length, rx_buffer);
			else
				if (protocol == T_1)
					return_value = CmdXfrBlockTPDU_T1(reader_index, tx_length,
						tx_buffer, rx_length, rx_buffer);
 				else
					return_value = IFD_PROTOCOL_NOT_SUPPORTED;
			break;

		default:
			return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdXfrBlock */


/*****************************************************************************
 *
 *					CCID_Transmit
 *
 ****************************************************************************/
RESPONSECODE CCID_Transmit(unsigned int reader_index, unsigned int tx_length,
	const unsigned char tx_buffer[], unsigned short rx_length, unsigned char bBWI)
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	status_t ret;

#ifndef TWIN_SERIAL
	if (ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		int r;

		/* Xfr Block */
		r = ControlUSB(reader_index, 0x21, 0x65, 0,
			(unsigned char *)tx_buffer, tx_length);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Xfr Block failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		return IFD_SUCCESS;
	}

	if (ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		int r;

		/* nul block so we are chaining */
		if (NULL == tx_buffer)
			rx_length = 0x10;	/* bLevelParameter */

		/* Xfr Block */
		DEBUG_COMM2("chain parameter: %d", rx_length);
		r = ControlUSB(reader_index, 0x21, 0x65, rx_length << 8,
			(unsigned char *)tx_buffer, tx_length);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Xfr Block failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		return IFD_SUCCESS;
	}
#endif

	cmd[0] = 0x6F; /* XfrBlock */
	i2dw(tx_length, cmd+1);	/* APDU length */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = bBWI;	/* extend block waiting timeout */
	cmd[8] = rx_length & 0xFF;	/* Expected length, in character mode only */
	cmd[9] = (rx_length >> 8) & 0xFF;

	/* check that the command is not too large */
	if (tx_length > CMD_BUF_SIZE)
	{
		DEBUG_CRITICAL2("TX Length too big: %d", tx_length);
		return IFD_NOT_SUPPORTED;
	}

	memcpy(cmd+10, tx_buffer, tx_length);

	ret = WritePort(reader_index, 10+tx_length, cmd);
	if (STATUS_NO_SUCH_DEVICE == ret)
		return IFD_NO_SUCH_DEVICE;
	if (ret != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
} /* CCID_Transmit */


/*****************************************************************************
 *
 *					CCID_Receive
 *
 ****************************************************************************/
RESPONSECODE CCID_Receive(unsigned int reader_index, unsigned int *rx_length,
	unsigned char rx_buffer[], unsigned char *chain_parameter)
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	unsigned int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	status_t ret;

#ifndef TWIN_SERIAL
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	if (ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		int r;

		/* Data Block */
		r = ControlUSB(reader_index, 0xA1, 0x6F, 0, rx_buffer, *rx_length);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Data Block failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		/* we need to store returned value */
		*rx_length = r;

		return IFD_SUCCESS;
	}

	if (ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		int r;
		unsigned char rx_tmp[4];
		unsigned char *old_rx_buffer = NULL;
		int old_rx_length = 0;

		/* read a nul block. buffer need to be at least 4-bytes */
		if (NULL == rx_buffer)
		{
			rx_buffer = rx_tmp;
			*rx_length = sizeof(rx_tmp);
		}

		/* the buffer must be 4 bytes minimum for ICCD-B */
		if (*rx_length < 4)
		{
			old_rx_buffer = rx_buffer;
			old_rx_length = *rx_length;
			rx_buffer = rx_tmp;
			*rx_length = sizeof(rx_tmp);
		}

time_request_ICCD_B:
		/* Data Block */
		r = ControlUSB(reader_index, 0xA1, 0x6F, 0, rx_buffer, *rx_length);

		/* we got an error? */
		if (r < 0)
		{
			DEBUG_INFO2("ICC Data Block failed: %s", strerror(errno));
			return IFD_COMMUNICATION_ERROR;
		}

		/* copy from the 4 bytes buffer if used */
		if (old_rx_buffer)
		{
			memcpy(old_rx_buffer, rx_buffer, min(r, old_rx_length));
			rx_buffer = old_rx_buffer;
		}

		/* bResponseType */
		switch (rx_buffer[0])
		{
			case 0x00:
				/* the abData field contains the information created by the
				 * preceding request */
				break;

			case 0x40:
				/* Status Information */
				ccid_error(rx_buffer[2], __FILE__, __LINE__, __FUNCTION__);
				return IFD_COMMUNICATION_ERROR;

			case 0x80:
				/* Polling */
			{
				int delay;

				delay = (rx_buffer[2] << 8) + rx_buffer[1];
				DEBUG_COMM2("Pooling delay: %d", delay);

				if (0 == delay)
					/* host select the delay */
					delay = 1;
				(void)usleep(delay * 1000 * 10);
				goto time_request_ICCD_B;
			}

			case 0x01:
			case 0x02:
			case 0x03:
			case 0x10:
				/* Extended case
				 * Only valid for Data Block frames */
				if (chain_parameter)
					*chain_parameter = rx_buffer[0];
				break;

			default:
				DEBUG_CRITICAL2("Unknown bResponseType: 0x%02X", rx_buffer[0]);
				return IFD_COMMUNICATION_ERROR;
		}

		memmove(rx_buffer, rx_buffer+1, r-1);
		*rx_length = r-1;

		return IFD_SUCCESS;
	}
#endif

time_request:
	length = sizeof(cmd);
	ret = ReadPort(reader_index, &length, cmd);
	if (ret != STATUS_SUCCESS)
	{
		if (STATUS_NO_SUCH_DEVICE == ret)
			return IFD_NO_SUCH_DEVICE;
		return IFD_COMMUNICATION_ERROR;
	}

	if (length < STATUS_OFFSET+1)
	{
		DEBUG_CRITICAL2("Not enough data received: %d bytes", length);
		return IFD_COMMUNICATION_ERROR;
	}

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		switch (cmd[ERROR_OFFSET])
		{
			case 0xEF:	/* cancel */
				if (*rx_length < 2)
					return IFD_COMMUNICATION_ERROR;
				rx_buffer[0]= 0x64;
				rx_buffer[1]= 0x01;
				*rx_length = 2;
				return IFD_SUCCESS;

			case 0xF0:	/* timeout */
				if (*rx_length < 2)
					return IFD_COMMUNICATION_ERROR;
				rx_buffer[0]= 0x64;
				rx_buffer[1]= 0x00;
				*rx_length = 2;
				return IFD_SUCCESS;

			case 0xFD:	/* Parity error during exchange */
				return IFD_PARITY_ERROR;

			default:
				return IFD_COMMUNICATION_ERROR;
		}
	}

	if (cmd[STATUS_OFFSET] & CCID_TIME_EXTENSION)
	{
		DEBUG_COMM2("Time extension requested: 0x%02X", cmd[ERROR_OFFSET]);
		goto time_request;
	}

	/* we have read less (or more) data than the CCID frame says to contain */
	if (length-10 != dw2i(cmd, 1))
	{
		DEBUG_CRITICAL3("Can't read all data (%d out of %d expected)",
			length-10, dw2i(cmd, 1));
		return_value = IFD_COMMUNICATION_ERROR;
	}

	length = dw2i(cmd, 1);
	if (length <= *rx_length)
		*rx_length = length;
	else
	{
		DEBUG_CRITICAL2("overrun by %d bytes", length - *rx_length);
		length = *rx_length;
		return_value = IFD_ERROR_INSUFFICIENT_BUFFER;
	}

	/* Kobil firmware bug. No support for chaining */
	if (length && (NULL == rx_buffer))
	{
		DEBUG_CRITICAL2("Nul block expected but got %d bytes", length);
		return_value = IFD_COMMUNICATION_ERROR;
	}
	else
		memcpy(rx_buffer, cmd+10, length);

	/* Extended case?
	 * Only valid for RDR_to_PC_DataBlock frames */
	if (chain_parameter)
		*chain_parameter = cmd[CHAIN_PARAMETER_OFFSET];

	return return_value;
} /* CCID_Receive */


/*****************************************************************************
 *
 *					CmdXfrBlockAPDU_extended
 *
 ****************************************************************************/
static RESPONSECODE CmdXfrBlockAPDU_extended(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	RESPONSECODE return_value;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	unsigned char chain_parameter;
	unsigned int local_tx_length, sent_length;
	unsigned int local_rx_length, received_length;
	int buffer_overflow = 0;

	if (ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		/* length is on 16-bits only
		 * if a size > 0x1000 is used then usb_control_msg() fails with
		 * "Invalid argument" */
		if (*rx_length > 0x1000)
			*rx_length = 0x1000;
	}

	DEBUG_COMM2("T=0 (extended): %d bytes", tx_length);

	/* send the APDU */
	sent_length = 0;

	/* we suppose one command is enough */
	chain_parameter = 0x00;

	local_tx_length = tx_length - sent_length;
	if (local_tx_length > CMD_BUF_SIZE)
	{
		local_tx_length = CMD_BUF_SIZE;
		/* the command APDU begins with this command, and continue in the next
		 * PC_to_RDR_XfrBlock */
		chain_parameter = 0x01;
	}
	if (local_tx_length > ccid_descriptor->dwMaxCCIDMessageLength-10)
	{
		local_tx_length = ccid_descriptor->dwMaxCCIDMessageLength-10;
		chain_parameter = 0x01;
	}

send_next_block:
	return_value = CCID_Transmit(reader_index, local_tx_length, tx_buffer,
		chain_parameter, 0);
	if (return_value != IFD_SUCCESS)
		return return_value;

	sent_length += local_tx_length;
	tx_buffer += local_tx_length;

	/* we just sent the last block (0x02) or only one block was needded (0x00) */
	if ((0x02 == chain_parameter) || (0x00 == chain_parameter))
		goto receive_block;

	/* read a nul block */
	return_value = CCID_Receive(reader_index, &local_rx_length, NULL, NULL);
	if (return_value != IFD_SUCCESS)
		return return_value;

	/* size of the next block */
	if (tx_length - sent_length > local_tx_length)
	{
		/* the abData field continues a command APDU and
		 * another block is to follow */
		chain_parameter = 0x03;
	}
	else
	{
		/* this abData field continues a command APDU and ends
		 * the APDU command */
		chain_parameter = 0x02;

		/* last (smaller) block */
		local_tx_length = tx_length - sent_length;
	}

	goto send_next_block;

receive_block:
	/* receive the APDU */
	received_length = 0;

receive_next_block:
	local_rx_length = *rx_length - received_length;
	return_value = CCID_Receive(reader_index, &local_rx_length, rx_buffer,
		&chain_parameter);
	if (IFD_ERROR_INSUFFICIENT_BUFFER == return_value)
	{
		buffer_overflow = 1;

		/* we continue to read all the response APDU */
		return_value = IFD_SUCCESS;
	}

	if (return_value != IFD_SUCCESS)
		return return_value;

	/* advance in the reiceiving buffer */
	rx_buffer += local_rx_length;
	received_length += local_rx_length;

	switch (chain_parameter)
	{
		/* the response APDU begins and ends in this command */
		case 0x00:
		/* this abData field continues the response APDU and ends the response
		 * APDU */
		case 0x02:
			break;

		/* the response APDU begins with this command and is to continue */
		case 0x01:
		/* this abData field continues the response APDU and another block is
		 * to follow */
		case 0x03:
		/* empty abData field, continuation of the command APDU is expected in
		 * next PC_to_RDR_XfrBlock command */
		case 0x10:
			/* send a nul block */
			/* set wLevelParameter to 0010h: empty abData field,
			 * continuation of response APDU is
			 * expected in the next RDR_to_PC_DataBlock. */
			return_value = CCID_Transmit(reader_index, 0, NULL, 0x10, 0);
			if (return_value != IFD_SUCCESS)
				return return_value;

			goto receive_next_block;
	}

	*rx_length = received_length;

	/* generate an overflow detected by pcscd */
	if (buffer_overflow)
		(*rx_length)++;

	return IFD_SUCCESS;
} /* CmdXfrBlockAPDU_extended */


/*****************************************************************************
 *
 *					CmdXfrBlockTPDU_T0
 *
 ****************************************************************************/
static RESPONSECODE CmdXfrBlockTPDU_T0(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	DEBUG_COMM2("T=0: %d bytes", tx_length);

	/* command length too big for CCID reader? */
	if (tx_length > ccid_descriptor->dwMaxCCIDMessageLength-10)
	{
#ifdef BOGUS_SCM_FIRMWARE_FOR_dwMaxCCIDMessageLength
		if (263 == ccid_descriptor->dwMaxCCIDMessageLength)
		{
			DEBUG_INFO3("Command too long (%d bytes) for max: %d bytes."
				" SCM reader with bogus firmware?",
				tx_length, ccid_descriptor->dwMaxCCIDMessageLength-10);
		}
		else
#endif
		{
			DEBUG_CRITICAL3("Command too long (%d bytes) for max: %d bytes",
				tx_length, ccid_descriptor->dwMaxCCIDMessageLength-10);
			return IFD_COMMUNICATION_ERROR;
		}
	}

	/* command length too big for CCID driver? */
	if (tx_length > CMD_BUF_SIZE)
	{
		DEBUG_CRITICAL3("Command too long (%d bytes) for max: %d bytes",
				tx_length, CMD_BUF_SIZE);
		return IFD_COMMUNICATION_ERROR;
	}

	return_value = CCID_Transmit(reader_index, tx_length, tx_buffer, 0, 0);
	if (return_value != IFD_SUCCESS)
		return return_value;

	return CCID_Receive(reader_index, rx_length, rx_buffer, NULL);
} /* CmdXfrBlockTPDU_T0 */


/*****************************************************************************
 *
 *					T0CmdParsing
 *
 ****************************************************************************/
static RESPONSECODE T0CmdParsing(unsigned char *cmd, unsigned int cmd_len,
	/*@out@*/ unsigned int *exp_len)
{
	*exp_len = 0;

	/* Ref: 7816-4 Annex A */
	switch (cmd_len)
	{
		case 4:	/* Case 1 */
			*exp_len = 2; /* SW1 and SW2 only */
			break;

		case 5: /* Case 2 */
			if (cmd[4] != 0)
				*exp_len = cmd[4] + 2;
			else
				*exp_len = 256 + 2;
			break;

		default: /* Case 3 */
			if (cmd_len > 5 && cmd_len == (unsigned int)(cmd[4] + 5))
				*exp_len = 2; /* SW1 and SW2 only */
			else
				return IFD_COMMUNICATION_ERROR;	/* situation not supported */
			break;
	}

	return IFD_SUCCESS;
} /* T0CmdParsing */


/*****************************************************************************
 *
 *					T0ProcACK
 *
 ****************************************************************************/
static RESPONSECODE T0ProcACK(unsigned int reader_index,
	unsigned char **snd_buf, unsigned int *snd_len,
	unsigned char **rcv_buf, unsigned int *rcv_len,
	unsigned char **in_buf, unsigned int *in_len,
	unsigned int proc_len, int is_rcv)
{
	RESPONSECODE return_value;
	unsigned int remain_len;
	unsigned char tmp_buf[512];
	unsigned int ret_len;

	DEBUG_COMM2("Enter, is_rcv = %d", is_rcv);

	if (is_rcv == 1)
	{	/* Receiving mode */
		if (*in_len > 0)
		{	/* There are still available data in our buffer */
			if (*in_len >= proc_len)
			{
				/* We only need to get the data from our buffer */
				memcpy(*rcv_buf, *in_buf, proc_len);
				*rcv_buf += proc_len;
				*in_buf += proc_len;
				*rcv_len += proc_len;
				*in_len -= proc_len;

				return IFD_SUCCESS;
			}
			else
			{
				/* Move all data in the input buffer to the reply buffer */
				remain_len = proc_len - *in_len;
				memcpy(*rcv_buf, *in_buf, *in_len);
				*rcv_buf += *in_len;
				*in_buf += *in_len;
				*rcv_len += *in_len;
				*in_len = 0;
			}
		}
		else
			/* There is no data in our tmp_buf,
			 * we have to read all data we needed */
			remain_len = proc_len;

		/* Read the expected data from the smartcard */
		if (*in_len != 0)
		{
			DEBUG_CRITICAL("*in_len != 0");
			return IFD_COMMUNICATION_ERROR;
		}

		memset(tmp_buf, 0, sizeof(tmp_buf));

#ifdef O2MICRO_OZ776_PATCH
		if((0 != remain_len) && (0 == (remain_len + 10) % 64))
        {
			/* special hack to avoid a command of size modulo 64
			 * we send two commands instead */
            ret_len = 1;
            return_value = CCID_Transmit(reader_index, 0, *snd_buf, ret_len, 0);
            if (return_value != IFD_SUCCESS)
                return return_value;
            return_value = CCID_Receive(reader_index, &ret_len, tmp_buf, NULL);
            if (return_value != IFD_SUCCESS)
                return return_value;

            ret_len = remain_len - 1;
            return_value = CCID_Transmit(reader_index, 0, *snd_buf, ret_len, 0);
            if (return_value != IFD_SUCCESS)
                return return_value;
            return_value = CCID_Receive(reader_index, &ret_len, &tmp_buf[1],
				NULL);
            if (return_value != IFD_SUCCESS)
                return return_value;

            ret_len += 1;
        }
        else
#endif
		{
			ret_len = remain_len;
			return_value = CCID_Transmit(reader_index, 0, *snd_buf, ret_len, 0);
			if (return_value != IFD_SUCCESS)
				return return_value;

			return_value = CCID_Receive(reader_index, &ret_len, tmp_buf, NULL);
			if (return_value != IFD_SUCCESS)
				return return_value;
		}
		memcpy(*rcv_buf, tmp_buf, remain_len);
		*rcv_buf += remain_len, *rcv_len += remain_len;

		/* If ret_len != remain_len, our logic is erroneous */
		if (ret_len != remain_len)
		{
			DEBUG_CRITICAL("ret_len != remain_len");
			return IFD_COMMUNICATION_ERROR;
		}
	}
	else
	{	/* Sending mode */

		return_value = CCID_Transmit(reader_index, proc_len, *snd_buf, 1, 0);
		if (return_value != IFD_SUCCESS)
			return return_value;

		*snd_len -= proc_len;
		*snd_buf += proc_len;
	}

	DEBUG_COMM("Exit");

	return IFD_SUCCESS;
} /* T0ProcACK */


/*****************************************************************************
 *
 *					T0ProcSW1
 *
 ****************************************************************************/
static RESPONSECODE T0ProcSW1(unsigned int reader_index,
	unsigned char *rcv_buf, unsigned int *rcv_len,
	unsigned char *in_buf, unsigned int in_len)
{
	RESPONSECODE return_value = IFD_SUCCESS;
	UCHAR tmp_buf[512];
	unsigned char *rcv_buf_tmp = rcv_buf;
	const unsigned int rcv_len_tmp = *rcv_len;
	unsigned char sw1, sw2;

	/* store the SW1 */
	sw1 = *rcv_buf = *in_buf;
	rcv_buf++;
	in_buf++;
	in_len--;
	(*rcv_len)++;

	/* store the SW2 */
	if (0 == in_len)
	{
		return_value = CCID_Transmit(reader_index, 0, rcv_buf, 1, 0);
		if (return_value != IFD_SUCCESS)
			return return_value;

		in_len = 1;

		return_value = CCID_Receive(reader_index, &in_len, tmp_buf, NULL);
		if (return_value != IFD_SUCCESS)
			return return_value;

		in_buf = tmp_buf;
	}
	sw2 = *rcv_buf = *in_buf;
	rcv_buf++;
	in_buf++;
	in_len--;
	(*rcv_len)++;

	if (return_value != IFD_SUCCESS)
	{
		rcv_buf_tmp[0] = rcv_buf_tmp[1] = 0;
		*rcv_len = rcv_len_tmp;
	}

	DEBUG_COMM3("Exit: SW=%02X %02X", sw1, sw2);

	return return_value;
} /* T0ProcSW1 */


/*****************************************************************************
 *
 *					CmdXfrBlockCHAR_T0
 *
 ****************************************************************************/
static RESPONSECODE CmdXfrBlockCHAR_T0(unsigned int reader_index,
	unsigned int snd_len, unsigned char snd_buf[], unsigned int *rcv_len,
	unsigned char rcv_buf[])
{
	int is_rcv;
	unsigned char cmd[5];
	unsigned char tmp_buf[512];
	unsigned int exp_len, in_len;
	unsigned char ins, *in_buf;
	unsigned char backup_len = *rcv_len;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	DEBUG_COMM2("T=0: %d bytes", snd_len);

	if (ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		unsigned char pcbuffer[SIZE_GET_SLOT_STATUS];

		/* length is on 16-bits only
		 * if a size > 0x1000 is used then usb_control_msg() fails with
		 * "Invalid argument" */
		if (*rcv_len > 0x1000)
			*rcv_len = 0x1000;

		/* Command to send to the smart card (must be 5 bytes) */
		memset(cmd, 0, sizeof(cmd));
		if (snd_len == 4)
		{
			memcpy(cmd, snd_buf, 4);
			snd_buf += 4;
			snd_len -= 4;
		}
		else
		{
			memcpy(cmd, snd_buf, 5);
			snd_buf += 5;
			snd_len -= 5;
		}

		/* at most 5 bytes */
		return_value = CCID_Transmit(reader_index, 5, cmd, 0, 0);
		if (return_value != IFD_SUCCESS)
			return return_value;

		/* wait for ready */
		return_value = CmdGetSlotStatus(reader_index, pcbuffer);
		if (return_value != IFD_SUCCESS)
			return return_value;

		if (0x10 == pcbuffer[0])
		{
			if (snd_len > 0)
			{
				/* continue sending the APDU */
				return_value = CCID_Transmit(reader_index, snd_len, snd_buf,
					0, 0);
				if (return_value != IFD_SUCCESS)
					return return_value;
			}
			else
			{
				/* read apdu data */
				return_value = CCID_Receive(reader_index, rcv_len, rcv_buf,
						NULL);
				if (return_value != IFD_SUCCESS)
					return return_value;
			}
		}

		return_value = CmdGetSlotStatus(reader_index, pcbuffer);
		if (return_value != IFD_SUCCESS)
			return return_value;

		/* SW1-SW2 available */
		if (0x20 == pcbuffer[0])
		{
			/* backup apdu data length */
			/* if no data recieved before - backup length must be zero */
			backup_len = (backup_len == *rcv_len) ? 0 : *rcv_len;

			/* wait for 2 bytes (SW1-SW2) */
			*rcv_len = 2;

			return_value = CCID_Receive(reader_index, rcv_len,
				rcv_buf + backup_len, NULL);
			if (return_value != IFD_SUCCESS)
				DEBUG_CRITICAL("CCID_Receive failed");

			/* restore recieved length */
			*rcv_len += backup_len;
		}
		return return_value;
	}

	in_buf = tmp_buf;
	in_len = 0;
	*rcv_len = 0;

	return_value = T0CmdParsing(snd_buf, snd_len, &exp_len);
	if (return_value != IFD_SUCCESS)
	{
		DEBUG_CRITICAL("T0CmdParsing failed");
		return IFD_COMMUNICATION_ERROR;
	}

	if (snd_len == 5 || snd_len == 4)
		is_rcv = 1;
	else
		is_rcv = 0;

	/* Command to send to the smart card (must be 5 bytes, from 7816 p.15) */
	memset(cmd, 0, sizeof(cmd));
	if (snd_len == 4)
	{
		memcpy(cmd, snd_buf, 4);
		snd_buf += 4;
		snd_len -= 4;
	}
	else
	{
		memcpy(cmd, snd_buf, 5);
		snd_buf += 5;
		snd_len -= 5;
	}

	/* Make sure this is a valid command by checking the INS field */
	ins = cmd[1];
	if ((ins & 0xF0) == 0x60 ||	/* 7816-3 8.3.2 */
		(ins & 0xF0) == 0x90)
	{
		DEBUG_CRITICAL2("fatal: INS (0x%02X) = 0x6X or 0x9X", ins);
		return IFD_COMMUNICATION_ERROR;
	}

	return_value = CCID_Transmit(reader_index, 5, cmd, 1, 0);
	if (return_value != IFD_SUCCESS)
		return return_value;

	while (1)
	{
		if (in_len == 0)
		{
			in_len = 1;
			return_value = CCID_Receive(reader_index, &in_len, tmp_buf, NULL);
			if (return_value != IFD_SUCCESS)
			{
				DEBUG_CRITICAL("CCID_Receive failed");
				return return_value;
			}
			in_buf = tmp_buf;
		}
		if (in_len == 0)
		{
			/* Suppose we should be able to get data.
			 * If not, error. Set the time-out error */
			DEBUG_CRITICAL("error: in_len = 0");
			return IFD_RESPONSE_TIMEOUT;
		}

		/* Start to process the procedure bytes */
		if (*in_buf == 0x60)
		{
			in_len = 0;
			return_value = CCID_Transmit(reader_index, 0, cmd, 1, 0);

			if (return_value != IFD_SUCCESS)
				return return_value;

			continue;
		}
		else if (*in_buf == ins || *in_buf == (ins ^ 0x01))
		{
			/* ACK => To transfer all remaining data bytes */
			in_buf++, in_len--;
			if (is_rcv)
				return_value = T0ProcACK(reader_index, &snd_buf, &snd_len,
					&rcv_buf, rcv_len, &in_buf, &in_len, exp_len - *rcv_len, 1);
			else
				return_value = T0ProcACK(reader_index, &snd_buf, &snd_len,
					&rcv_buf, rcv_len, &in_buf, &in_len, snd_len, 0);

			if (*rcv_len == exp_len)
				return return_value;

			continue;
		}
		else if (*in_buf == (ins ^ 0xFF) || *in_buf == (ins ^ 0xFE))
		{
			/* ACK => To transfer 1 remaining bytes */
			in_buf++, in_len--;
			return_value = T0ProcACK(reader_index, &snd_buf, &snd_len,
				&rcv_buf, rcv_len, &in_buf, &in_len, 1, is_rcv);

			if (return_value != IFD_SUCCESS)
				return return_value;

			continue;
		}
		else if ((*in_buf & 0xF0) == 0x60 || (*in_buf & 0xF0) == 0x90)
			/* SW1 */
			return T0ProcSW1(reader_index, rcv_buf, rcv_len, in_buf, in_len);

		/* Error, unrecognized situation found */
		DEBUG_CRITICAL2("Unrecognized Procedure byte (0x%02X) found!", *in_buf);
		return return_value;
	}

	return return_value;
} /* CmdXfrBlockCHAR_T0 */


/*****************************************************************************
 *
 *					CmdXfrBlockTPDU_T1
 *
 ****************************************************************************/
static RESPONSECODE CmdXfrBlockTPDU_T1(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	RESPONSECODE return_value = IFD_SUCCESS;
	int ret;

	DEBUG_COMM3("T=1: %d and %d bytes", tx_length, *rx_length);

	ret = t1_transceive(&((get_ccid_slot(reader_index)) -> t1), 0,
		tx_buffer, tx_length, rx_buffer, *rx_length);

	if (ret < 0)
		return_value = IFD_COMMUNICATION_ERROR;
	else
		*rx_length = ret;

	return return_value;
} /* CmdXfrBlockTPDU_T1 */


/*****************************************************************************
 *
 *					SetParameters
 *
 ****************************************************************************/
RESPONSECODE SetParameters(unsigned int reader_index, char protocol,
	unsigned int length, unsigned char buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	DEBUG_COMM2("length: %d bytes", length);

	cmd[0] = 0x61; /* SetParameters */
	i2dw(length, cmd+1);	/* APDU length */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = protocol;	/* bProtocolNum */
	cmd[8] = cmd[9] = 0; /* RFU */

	/* check that the command is not too large */
	if (length > CMD_BUF_SIZE)
		return IFD_NOT_SUPPORTED;

	memcpy(cmd+10, buffer, length);

	if (WritePort(reader_index, 10+length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	if (ReadPort(reader_index, &length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (length < STATUS_OFFSET+1)
	{
		DEBUG_CRITICAL2("Not enough data received: %d bytes", length);
		return IFD_COMMUNICATION_ERROR;
	}

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		if (0x00 == cmd[ERROR_OFFSET])	/* command not supported */
			return IFD_NOT_SUPPORTED;
		else
			if ((cmd[ERROR_OFFSET] >= 1) && (cmd[ERROR_OFFSET] <= 127))
				/* a parameter is not changeable */
				return IFD_SUCCESS;
			else
				return IFD_COMMUNICATION_ERROR;
	}

	return IFD_SUCCESS;
} /* SetParameters */


/*****************************************************************************
 *
 *					isCharLevel
 *
 ****************************************************************************/
int isCharLevel(int reader_index)
{
	return CCID_CLASS_CHARACTER == (get_ccid_descriptor(reader_index)->dwFeatures & CCID_CLASS_EXCHANGE_MASK);
} /* isCharLevel */


/*****************************************************************************
 *
 *					i2dw
 *
 ****************************************************************************/
static void i2dw(int value, unsigned char buffer[])
{
	buffer[0] = value & 0xFF;
	buffer[1] = (value >> 8) & 0xFF;
	buffer[2] = (value >> 16) & 0xFF;
	buffer[3] = (value >> 24) & 0xFF;
} /* i2dw */

