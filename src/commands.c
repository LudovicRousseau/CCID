/*
    commands.c: Commands sent to the card
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

#include <string.h>
#include <stdlib.h>
#include <PCSC/pcsclite.h>
#include <PCSC/ifdhandler.h>

#include "commands.h"
#include "openct/proto-t1.h"
#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "config.h"
#include "debug.h"

/*
 * Possible values :
 * 3 -> 1.8V, 3V, 5V
 * 2 -> 3V, 5V
 * 1 -> 5V only
 */
/*
 * To be safe we only use 5V
 * otherwise we would have to parse the ATR and get the value of TAi (i>2) when
 * in T=15
 */
#define DEFAULT_VOLTAGE 1 /* start with 5 volts */

/* internal functions */
static RESPONSECODE CmdXfrBlockTPDU_T0(unsigned int reader_index,
	unsigned int tx_length, unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[]);

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
	unsigned char buffer[])
{
	unsigned char cmd[10];
	status_t res;
	int atr_len, length, count = 1;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	char voltage;

	/* store length of buffer[] */
	length = *nlength;

	if (ccid_descriptor->dwFeatures & CCID_CLASS_AUTO_VOLTAGE)
		voltage = 0;	/* automatic voltage selection */
	else
		voltage = DEFAULT_VOLTAGE;

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
 *					SecurePIN
 *
 ****************************************************************************/
RESPONSECODE SecurePIN(unsigned int reader_index,
	const unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength)

{
	unsigned char cmd[11+14+CMD_BUF_SIZE];
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	int length = 0;

	/* PIN verification data structure WITHOUT TeoPrologue */
	if (TxBuffer[4] /* Lc */
		+ 5 /* CLA, INS, P1, P2, Lc */
		+ 11 /* CCID PIN verification data structure */ == TxLength)
	{
		i2dw(TxLength+3+1, cmd+1);	/* command length */

		/* copy the CCID data structure */
		memcpy(cmd +11, TxBuffer + TxBuffer[4] + 5, 11);

		/* TeoPrologue not used */
		memset(cmd +11 +11, 0, 3);

		/* copy the APDU */
		memcpy(cmd +11 +14, TxBuffer, TxLength-11);

		length = 14 + TxLength;
	}
	/* PIN verification data structure WITH TeoPrologue */
	else if (TxBuffer[4] /* Lc */
		+ 5 /* CLA, INS, P1, P2, Lc */
		+ 14 /* CCID PIN verification data structure */ == TxLength)
	{
		i2dw(TxLength+1, cmd+1);	/* command length */

		/* copy the CCID data structure */
		memcpy(cmd +11, TxBuffer + TxBuffer[4] + 5, 14);

		/* copy the APDU */
		memcpy(cmd +11 +14, TxBuffer, TxLength-14);

		length = 11 + TxLength;
	}
	else
	{
		*RxLength = 0;
		return IFD_COMMUNICATION_ERROR;
	}

	cmd[0] = 0x69;	/* Secure */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = 0;		/* bBWI */
	cmd[8] = 0;		/* wLevelParameter */
	cmd[9] = 0;
	cmd[10] = 0;	/* bPINOperation: PIN Verification */

	if (WritePort(reader_index, length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	return CCID_Receive(reader_index, RxLength, RxBuffer);
} /* SecurePIN */


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
	const unsigned char tx_buffer[], unsigned char bBWI)
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	cmd[0] = 0x6F; /* XfrBlock */
	i2dw(tx_length, cmd+1);	/* APDU length */
	cmd[5] = ccid_descriptor->bCurrentSlotIndex;	/* slot number */
	cmd[6] = (*ccid_descriptor->pbSeq)++;
	cmd[7] = bBWI;	/* extend block waiting timeout */
	cmd[8] = cmd[9] = 0; /* RFU */
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
		*rx_length = 0; /* nothing received */
		if (0xFD == cmd[ERROR_OFFSET]) /* Parity error during exchange */
			return IFD_PARITY_ERROR;
		else
			return IFD_COMMUNICATION_ERROR;
	}

	if (cmd[STATUS_OFFSET] & CCID_TIME_EXTENSION)
	{
		DEBUG_CRITICAL2("Time extension requested: 0x%02X", cmd[ERROR_OFFSET]);
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

	return_value = CCID_Transmit(reader_index, tx_length, tx_buffer, 0);
	if (return_value != IFD_SUCCESS)
		return return_value;
	
	return CCID_Receive(reader_index, rx_length, rx_buffer);
} /* CmdXfrBlockTPDU_T0 */


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

