/*
    commands.c: Commands sent to the card
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

#include <string.h>
#include <stdlib.h>
#include <pcsclite.h>
#include <ifdhandler.h>
#include <reader.h>

#include "commands.h"
#include "openct/proto-t1.h"
#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "config.h"
#include "debug.h"

/* All the pinpad readers I used are more or less bogus
 * I use code to change the user command and make the firmware happy */
#define BOGUS_PINPAD_FIRMWARE

#define max( a, b )   ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* internal functions */
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

	/* store length of buffer[] */
	length = *nlength;

	if (ccid_descriptor->dwFeatures & CCID_CLASS_AUTO_VOLTAGE)
		voltage = 0;	/* automatic voltage selection */

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

	if (buffer[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */

		if (0xBB == buffer[ERROR_OFFSET] &&	/* Protocol error in EMV mode */
			((GEMPC433 == ccid_descriptor->readerID)
			|| (CHERRYXX33 == ccid_descriptor->readerID)))
		{
			unsigned char cmd[] = {0x1F, 0x01};
			unsigned char res[1];
			unsigned int res_length = sizeof(res);

			if ((return_value = CmdEscape(reader_index, cmd, sizeof(cmd), res,
				&res_length)) != IFD_SUCCESS)
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
			char *voltage_code[] = { "auto", "5V", "3V", "1.8V" };

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
		*RxLength = 0;
		return IFD_NOT_SUPPORTED;
	}

	if (TxLength < 19+4 /* 4 = APDU size */)	/* command too short? */
	{
		DEBUG_INFO3("Command too short: %d < %d", TxLength, 19+4);
		*RxLength = 0;
		return IFD_NOT_SUPPORTED;
	}

	if (dw2i(TxBuffer, 15) + 19 != TxLength) /* ulDataLength field coherency */
	{
		DEBUG_INFO3("Wrong lengths: %d %d", dw2i(TxBuffer, 15) + 19, TxLength);
		*RxLength = 0;
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
	if (GEMPCPINPAD == ccid_descriptor->readerID)
	{
		/* the firmware reject the cases: 00h No string and FFh default
		 * CCID message. The only value supported is 01h (display 1 message) */
		if (0x01 != TxBuffer[8])
		{
			DEBUG_INFO2("Correct bNumberMessage for GemPC Pinpad (was %d)",
				TxBuffer[8]);
			TxBuffer[8] = 0x01;
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

	/* Build a CCID block from a PC/SC V2.1.2 Part 10 block */
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
	if ((SPR532 == ccid_descriptor->readerID) && (TxBuffer[15] == 4))
	{
		RESPONSECODE return_value;
		unsigned char cmd[] = { 0x80, 0x02, 0x00 };
		unsigned char res[1];
		unsigned int res_length = sizeof(res);

		/* the SPR532 will append the PIN code without any padding */
		return_value = CmdEscape(reader_index, cmd, sizeof(cmd), res,
			&res_length);
		if (return_value != IFD_SUCCESS)
		{
			ccid_error(res[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);
			return return_value;
		}
	}

	i2dw(a - 10, cmd + 1);  /* CCID message length */

	old_read_timeout = ccid_descriptor -> readTimeout;
	ccid_descriptor -> readTimeout = max(30, TxBuffer[0]);	/* at least 30 seconds */

	if (WritePort(reader_index, a, cmd) != STATUS_SUCCESS)
	{
		*RxLength = 0;
		return IFD_COMMUNICATION_ERROR;
	}

	ret = CCID_Receive(reader_index, RxLength, RxBuffer);

	/* T=1 Protocol Management for a TPDU reader */
	if ((IFD_SUCCESS == ret)
		&& (SCARD_PROTOCOL_T1 == ccid_descriptor->cardProtocol)
		&& (CCID_CLASS_TPDU == (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)))
	{
		/* timeout and cancel cases are faked by CCID_Receive() */
		if (2 == *RxLength)
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
		*RxLength = 0;
		return IFD_NOT_SUPPORTED;
	}

	if (TxLength < 24+4 /* 4 = APDU size */) /* command too short? */
	{
		DEBUG_INFO3("Command too short: %d < %d", TxLength, 24+4);
		*RxLength = 0;
		return IFD_NOT_SUPPORTED;
	}

	if (dw2i(TxBuffer, 20) + 24 != TxLength) /* ulDataLength field coherency */
	{
		DEBUG_INFO3("Wrong lengths: %d %d", dw2i(TxBuffer, 20) + 24, TxLength);
		*RxLength = 0;
		return IFD_NOT_SUPPORTED;
	}

	/* Make sure in the beginning if bNumberMessage is valid or not */
	if (TxBuffer[11] > 3)
	{
		DEBUG_INFO2("Wrong bNumberMessage: %d", TxBuffer[11]);
		*RxLength = 0;
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
	if (GEMPCPINPAD == ccid_descriptor->readerID)
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

	/* Build a CCID block from a PC/SC V2.1.2 Part 10 block */

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
#endif

	/* We know the size of the CCID message now */
	i2dw(a - 10, cmd + 1);	/* command length (includes bPINOperation) */

	old_read_timeout = ccid_descriptor -> readTimeout;
	ccid_descriptor -> readTimeout = max(30, TxBuffer[0]);	/* at least 30 seconds */

	if (WritePort(reader_index, a, cmd) != STATUS_SUCCESS)
	{
		*RxLength = 0;
 		return IFD_COMMUNICATION_ERROR;
	}

 	ret = CCID_Receive(reader_index, RxLength, RxBuffer);

	/* T=1 Protocol Management for a TPDU reader */
	if ((IFD_SUCCESS == ret)
		&& (SCARD_PROTOCOL_T1 == ccid_descriptor->cardProtocol)
		&& (CCID_CLASS_TPDU == (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK)))
	{
		/* timeout and cancel cases are faked by CCID_Receive() */
		if (2 == *RxLength)
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
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

again:
	/* allocate buffers */
	length_in = 10 + TxLength;
	if (NULL == (cmd_in = malloc(length_in)))
		return IFD_COMMUNICATION_ERROR;

	length_out = 10 + *RxLength;
	if (NULL == (cmd_out = malloc(length_out)))
	{
		free(cmd_in);
		return IFD_COMMUNICATION_ERROR;
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
		return IFD_COMMUNICATION_ERROR;
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
		return IFD_COMMUNICATION_ERROR;
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

	cmd[0] = 0x65; /* GetSlotStatus */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(reader_index, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = SIZE_GET_SLOT_STATUS;
	res = ReadPort(reader_index, &length, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (buffer[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */

		/* card absent or mute is not an communication error */
		if (buffer[ERROR_OFFSET] != 0xFE)
			return_value = IFD_COMMUNICATION_ERROR;
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

	/* command length too big for CCID reader? */
	if (tx_length > ccid_descriptor->dwMaxCCIDMessageLength)
	{
		DEBUG_CRITICAL3("Command too long (%d bytes) for max: %d bytes",
			tx_length, ccid_descriptor->dwMaxCCIDMessageLength);
		return_value = IFD_COMMUNICATION_ERROR;
		goto clean_up_and_return;
	}

	/* command length too big for CCID driver? */
	if (tx_length > CMD_BUF_SIZE)
	{
		DEBUG_CRITICAL3("Command too long (%d bytes) for max: %d bytes",
			tx_length, CMD_BUF_SIZE);
		return_value = IFD_COMMUNICATION_ERROR;
		goto clean_up_and_return;
	}

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
		case CCID_CLASS_EXTENDED_APDU:
			/* We only support extended APDU if the reader can support the
			 * command length. See test above */
			return_value = CmdXfrBlockTPDU_T0(reader_index,
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
			*rx_length = 0;
			return_value = IFD_COMMUNICATION_ERROR;
	}

clean_up_and_return:
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

	cmd[0] = 0x6F; /* XfrBlock */
	i2dw(tx_length, cmd+1);	/* APDU length */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = bBWI;	/* extend block waiting timeout */
	cmd[8] = rx_length & 0xFF;	/* Expected length, in character mode only */
	cmd[9] = (rx_length >> 8) & 0xFF;

	/* check that the command is not too large */
	if (tx_length > CMD_BUF_SIZE)
		return IFD_NOT_SUPPORTED;

	memcpy(cmd+10, tx_buffer, tx_length);

	if (WritePort(reader_index, 10+tx_length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
} /* CCID_Transmit */


/*****************************************************************************
 *
 *					CCID_Receive
 *
 ****************************************************************************/
RESPONSECODE CCID_Receive(unsigned int reader_index, unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	unsigned int length;

time_request:
	length = sizeof(cmd);
	if (ReadPort(reader_index, &length, cmd) != STATUS_SUCCESS)
	{
		*rx_length = 0;
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
				*rx_length = 0; /* nothing received */
				return IFD_PARITY_ERROR;

			default:
				*rx_length = 0; /* nothing received */
				return IFD_COMMUNICATION_ERROR;
		}
	}

	if (cmd[STATUS_OFFSET] & CCID_TIME_EXTENSION)
	{
		DEBUG_COMM2("Time extension requested: 0x%02X", cmd[ERROR_OFFSET]);
		goto time_request;
	}

	length = dw2i(cmd, 1);
	if (length < *rx_length)
		*rx_length = length;
	else
		length = *rx_length;
	memcpy(rx_buffer, cmd+10, length);

	return IFD_SUCCESS;
} /* CCID_Receive */


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

	DEBUG_COMM2("T=0: %d bytes", tx_length);

	return_value = CCID_Transmit(reader_index, tx_length, tx_buffer, 0, 0);
	if (return_value != IFD_SUCCESS)
		return return_value;

	return CCID_Receive(reader_index, rx_length, rx_buffer);
} /* CmdXfrBlockTPDU_T0 */


/*****************************************************************************
 *
 *					T0CmdParsing
 *
 ****************************************************************************/
static RESPONSECODE T0CmdParsing(unsigned char *cmd, unsigned int cmd_len,
	unsigned int *exp_len)
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

		ret_len = remain_len;
		return_value = CCID_Transmit(reader_index, 0, *snd_buf, ret_len, 0);
		if (return_value != IFD_SUCCESS)
			return return_value;

		return_value = CCID_Receive(reader_index, &ret_len, tmp_buf);
		if (return_value != IFD_SUCCESS)
			return return_value;

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

		return_value = CCID_Receive(reader_index, &in_len, tmp_buf);
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
	RESPONSECODE return_value = IFD_SUCCESS;

	DEBUG_COMM2("T=0: %d bytes", snd_len);

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
			return_value = CCID_Receive(reader_index, &in_len, tmp_buf);
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

	DEBUG_COMM2("T=1: %d bytes", tx_length);

	ret = t1_transceive(&((get_ccid_slot(reader_index)) -> t1), 0,
		tx_buffer, tx_length, rx_buffer, *rx_length);

	if (ret < 0)
	{
		*rx_length = 0;
		return_value = IFD_COMMUNICATION_ERROR;
	}
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

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		if (0x00 == cmd[ERROR_OFFSET])	/* command not supported */
			return IFD_NOT_SUPPORTED;
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

