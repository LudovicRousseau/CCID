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


/*****************************************************************************
 *
 *					CmdPowerOn
 *
 ****************************************************************************/
RESPONSECODE CmdPowerOn(int lun, unsigned int * nlength, unsigned char buffer[])
{
	unsigned char cmd[10];
	status_t res;
	int atr_len, length, count = 1;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	/* store length of buffer[] */
	length = *nlength;
again:
	cmd[0] = 0x62; /* IccPowerOn */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	if (ccid_descriptor->dwFeatures & CCID_CLASS_AUTO_VOLTAGE)
		cmd[7] = 0x00;	/* Automatic voltage selection */
	else
		cmd[7] = 0x01;	/* 5.0V */
	cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(lun, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	/* reset available buffer size */
	/* needed if we go back after a switch to ISO mode */
	*nlength = length;

	res = ReadPort(lun, nlength, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (buffer[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */

		/* Protocol error in EMV mode */
		if (buffer[ERROR_OFFSET] == 0xBB &&
			ccid_descriptor->readerID == GEMPC433)
		{
			unsigned char cmd[] = "\x1F\x01";
			unsigned char res[1];
			unsigned int res_length = sizeof(res);

			if ((return_value = CmdEscape(lun, cmd, sizeof(cmd)-1, res,
				&res_length)) != IFD_SUCCESS)
				return return_value;

			/* avoid looping if we can't switch mode */
			if (count--)
				goto again;
			else
				DEBUG_CRITICAL("Can't set reader in ISO mode");
		}

		return IFD_COMMUNICATION_ERROR;
	}

	/* extract the ATR */
	atr_len = dw2i(buffer, 1);	/* ATR length */
	if (atr_len > *nlength)
		atr_len = *nlength;
	else
		*nlength = atr_len;

	memcpy(buffer, buffer+10, atr_len);

	return return_value;
} /* CmdPowerOn */


/*****************************************************************************
 *
 *					SecurePIN
 *
 ****************************************************************************/
RESPONSECODE SecurePIN(int lun, const unsigned char TxBuffer[],
	unsigned int TxLength, unsigned char RxBuffer[], unsigned int *RxLength)
{
	unsigned char cmd[11+14+CMD_BUF_SIZE];
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);
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
	cmd[5] = 0;		/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = 0;		/* bBWI */
	cmd[8] = 0;		/* wLevelParameter */
	cmd[9] = 0;
	cmd[10] = 0;	/* bPINOperation: PIN Verification */

	if (WritePort(lun, length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	return CCID_Receive(lun, RxLength, RxBuffer);
} /* SecurePIN */


/*****************************************************************************
 *
 *					Escape
 *
 ****************************************************************************/
RESPONSECODE CmdEscape(int lun, const unsigned char TxBuffer[],
	unsigned int TxLength, unsigned char RxBuffer[], unsigned int *RxLength)
{
	unsigned char *cmd_in, *cmd_out;
	status_t res;
	unsigned int length_in, length_out;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

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
	cmd_in[5] = 0;	/* slot number */
	cmd_in[6] = ccid_descriptor->bSeq++;
	cmd_in[7] = cmd_in[8] = cmd_in[9] = 0; /* RFU */

	/* copy the command */
	memcpy(&cmd_in[10], TxBuffer, TxLength);

	res = WritePort(lun, length_in, cmd_in);
	free(cmd_in);
	if (res != STATUS_SUCCESS)
	{
		free(cmd_out);
		return IFD_COMMUNICATION_ERROR;
	}

	res = ReadPort(lun, &length_out, cmd_out);
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
RESPONSECODE CmdPowerOff(int lun)
{
	unsigned char cmd[10];
	status_t res;
	unsigned int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	cmd[0] = 0x63; /* IccPowerOff */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(lun, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	res = ReadPort(lun, &length, cmd);
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
RESPONSECODE CmdGetSlotStatus(int lun, unsigned char buffer[])
{
	unsigned char cmd[10];
	status_t res;
	unsigned int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	cmd[0] = 0x65; /* GetSlotStatus */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WritePort(lun, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = SIZE_GET_SLOT_STATUS;
	res = ReadPort(lun, &length, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (buffer[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdGetSlotStatus */


/*****************************************************************************
 *
 *					CmdXfrBlock
 *
 ****************************************************************************/
RESPONSECODE CmdXfrBlock(int lun, unsigned int tx_length,
	unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[], int protocol)
{
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

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
				return_value = CmdXfrBlockTPDU_T0(lun, tx_length, tx_buffer,
					rx_length, rx_buffer);
			else
				if (protocol == T_1)
					return_value = CmdXfrBlockTPDU_T1(lun, tx_length,
						tx_buffer, rx_length, rx_buffer);
				else
					return_value = IFD_PROTOCOL_NOT_SUPPORTED;
			break;

		case CCID_CLASS_SHORT_APDU:
		case CCID_CLASS_EXTENDED_APDU:
			/* We only support extended APDU if the reader can support the
			 * command length. See test above */
			return_value = CmdXfrBlockTPDU_T0(lun, tx_length, tx_buffer,
				rx_length, rx_buffer);
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
RESPONSECODE CCID_Transmit(int lun, unsigned int tx_length,
	const unsigned char tx_buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	cmd[0] = 0x6F; /* XfrBlock */
	i2dw(tx_length, cmd+1);	/* APDU length */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */
	memcpy(cmd+10, tx_buffer, tx_length);

	if (WritePort(lun, 10+tx_length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
} /* CCID_Transmit */


/*****************************************************************************
 *
 *					CCID_Receive
 *
 ****************************************************************************/
RESPONSECODE CCID_Receive(int lun, unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	unsigned int length;

time_request:
	length = sizeof(cmd);
	if (ReadPort(lun, &length, cmd) != STATUS_SUCCESS)
	{
		*rx_length = 0;
		return IFD_COMMUNICATION_ERROR;
	}

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		*rx_length = 0; /* nothing received */
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
RESPONSECODE CmdXfrBlockTPDU_T0(int lun, unsigned int tx_length,
	unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	RESPONSECODE return_value = IFD_SUCCESS;

	DEBUG_COMM2("T=0: %d bytes", tx_length);

	return_value = CCID_Transmit(lun, tx_length, tx_buffer);
	if (return_value != IFD_SUCCESS)
		return return_value;
	
	return CCID_Receive(lun, rx_length, rx_buffer);
} /* CmdXfrBlockTPDU_T0 */


/*****************************************************************************
 *
 *					CmdXfrBlockTPDU_T1
 *
 ****************************************************************************/
RESPONSECODE CmdXfrBlockTPDU_T1(int lun, unsigned int tx_length,
	unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[])
{
	RESPONSECODE return_value;
	APDU_Cmd cmd;
	APDU_Rsp rsp;

	DEBUG_COMM2("T=1: %d bytes", tx_length);

	/* set up command APDU */
	cmd.command = tx_buffer;
	cmd.length = tx_length;

	return_value = Protocol_T1_Command(&((get_ccid_slot(lun)) -> t1), &cmd,
		&rsp);

	if (return_value == PROTOCOL_T1_OK)
	{
		return_value = IFD_SUCCESS;

		/* copy the response */
		memcpy(rx_buffer, rsp.response, rsp.length);

		/* free the allocated response buffer */
		free(rsp.response);

		*rx_length = rsp.length;
	}
	else
	{
		*rx_length = 0;
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdXfrBlockTPDU_T1 */


/*****************************************************************************
 *
 *					SetParameters
 *
 ****************************************************************************/
RESPONSECODE SetParameters(int lun, char protocol, unsigned int length,
	unsigned char buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	DEBUG_COMM2("length: %d bytes", length);

	cmd[0] = 0x61; /* SetParameters */
	i2dw(length, cmd+1);	/* APDU length */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = protocol;	/* bProtocolNum */
	cmd[8] = cmd[9] = 0; /* RFU */
	memcpy(cmd+10, buffer, length);

	if (WritePort(lun, 10+length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	if (ReadPort(lun, &length, cmd) != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__, __FUNCTION__);    /* bError */
		return IFD_COMMUNICATION_ERROR;
	}

	return IFD_SUCCESS;
} /* SetParameters */


/*****************************************************************************
 *
 *					i2dw
 *
 ****************************************************************************/
void i2dw(int value, unsigned char buffer[])
{
	buffer[0] = value & 0xFF;
	buffer[1] = (value >> 8) & 0xFF;
	buffer[2] = (value >> 16) & 0xFF;
	buffer[3] = (value >> 24) & 0xFF;
} /* i2dw */

