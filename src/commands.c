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

#include "pcscdefines.h"
#include "commands.h"
#include "ccid.h"
#include "defs.h"
#include "ifdhandler.h"
#include "config.h"
#include "debug.h"


/*****************************************************************************
 *
 *					CmdPowerOn
 *
 ****************************************************************************/
RESPONSECODE CmdPowerOn(int lun, int * nlength, unsigned char buffer[])
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
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */

		/* Protocol error in EMV mode */
		if (buffer[ERROR_OFFSET] == 0xBB &&
			ccid_descriptor->readerID == GEMPC433)
		{
			if ((return_value = CmdEscape(lun, ESC_GEMPC_SET_ISO_MODE)) != IFD_SUCCESS)
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
 *					Escape
 *
 ****************************************************************************/
RESPONSECODE CmdEscape(int lun, int command)
{
	unsigned char cmd[12];
	status_t res;
	int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	cmd[0] = 0x6B; /* PC_to_RDR_Escape */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	switch (command)
	{
		/* Gemplus proprietary */
		case ESC_GEMPC_SET_ISO_MODE:
			DEBUG_INFO("Switch reader in ISO mode");
			cmd[10] = 0x1F;	/* switch mode */
			cmd[11] = 0x01;	/* set ISO mode */
			cmd[1] = 2;	/* length of data */
			length = 12;
			break;

		/* Gemplus proprietary */
		case ESC_GEMPC_SET_APDU_MODE:
			DEBUG_INFO("Switch reader in APDU mode");
			cmd[10] = 0xA0;	/* switch mode */
			cmd[11] = 0x02;	/* set APDU mode */
			cmd[1] = 2;	/* length of data */
			length = 12;
			break;

		default:
			DEBUG_CRITICAL2("Unkown Escape command: %d", command);
			return return_value;
	}

	res = WritePort(lun, length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	res = ReadPort(lun, &length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

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
	int length;
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
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
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
	int length;
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
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdGetSlotStatus */


/*****************************************************************************
 *
 *					CmdXfrBlock
 *
 ****************************************************************************/
RESPONSECODE CmdXfrBlock(int lun, int tx_length, unsigned char tx_buffer[],
	int *rx_length, unsigned char rx_buffer[])
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
			return_value = CmdXfrBlockTPDU(lun, tx_length, tx_buffer, rx_length,
				rx_buffer);
			break;

		case CCID_CLASS_SHORT_APDU:
		case CCID_CLASS_EXTENDED_APDU:
			/* We only support extended APDU if the reader can support the
			 * command length. See test above */
			return_value = CmdXfrBlockShortAPDU(lun, tx_length, tx_buffer,
				rx_length, rx_buffer);
			break;

		default:
			return_value = IFD_COMMUNICATION_ERROR;
	}

clean_up_and_return:
	return return_value;
} /* CmdXfrBlock */


/*****************************************************************************
 *
 *					CmdXfrBlockShortAPDU
 *
 ****************************************************************************/
RESPONSECODE CmdXfrBlockShortAPDU(int lun, int tx_length,
	unsigned char tx_buffer[], int *rx_length, unsigned char rx_buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	status_t res;
	int length;
	RESPONSECODE return_value = IFD_SUCCESS;
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	DEBUG_COMM2("ShortAPDU: %d bytes", tx_length);

	cmd[0] = 0x6F; /* IccPowerOff */
	i2dw(tx_length, cmd+1);	/* APDU length */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_descriptor->bSeq++;
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */
	memcpy(cmd+10, tx_buffer, tx_length);

	res = WritePort(lun, 10+tx_length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

time_request:
	length = sizeof(cmd);
	res = ReadPort(lun, &length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (cmd[STATUS_OFFSET] & CCID_COMMAND_FAILED)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		*rx_length = 0; /* nothing received */
		return_value = IFD_COMMUNICATION_ERROR;
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

	return return_value;
} /* CmdXfrBlockShortAPDU */


/*****************************************************************************
 *
 *					CmdXfrBlockTPDU
 *
 ****************************************************************************/
RESPONSECODE CmdXfrBlockTPDU(int lun, int tx_length, unsigned char tx_buffer[],
	int  *rx_length, unsigned char rx_buffer[])
{
	RESPONSECODE return_value = IFD_SUCCESS;
	int Le;

	DEBUG_COMM2("TPDU: %d bytes", tx_length);

	/* Size should be command + one byte of length for
	 * an outgoing TPDU (CLA, INS, P1, P2, P3) */
	if (tx_length == (ISO_CMD_SIZE + ISO_LENGTH_SIZE))
	{
		return_value = CmdXfrBlockShortAPDU(lun, tx_length, tx_buffer, rx_length,
			rx_buffer);
	}
	else
	{
		/* just (CLA, INS, P1, P2) for an APDU */
		if (tx_length == ISO_CMD_SIZE)
		{
			return_value = CmdXfrBlockShortAPDU(lun, tx_length, tx_buffer,
					rx_length, rx_buffer);
		}
		else
		{
			DWORD ntestlength;

			if (tx_length > (ISO_CMD_SIZE + ISO_LENGTH_SIZE))
			{
				/* Check length to see if it is a full APDU or a TPDU */
				ntestlength = tx_buffer[ISO_OFFSET_LENGTH] +
					ISO_CMD_SIZE + ISO_LENGTH_SIZE;

				if (tx_length == (ntestlength + ISO_LENGTH_SIZE))
				{
					DWORD old_rx_length = *rx_length;

					/* tx_buffer holds a proper APDU */
					Le = tx_buffer[tx_length-1];
					return_value = CmdXfrBlockShortAPDU(lun, tx_length,
						tx_buffer, rx_length, rx_buffer);

#if 0
					if ((return_value == IFD_SUCCESS) && (*rx_length == 2))
					{
						/* Buffer for Get Response */
						tx_buffer[0] = 0x00;
						tx_buffer[1] = 0xC0;
						tx_buffer[2] = 0x00;
						tx_buffer[3] = 0x00;

						if (rx_buffer[0] == 0x61)
						{
							/* Get Response */
							DEBUG_COMM2("TPDU: Automatic Get Response after 61%02X", rx_buffer[1]);
							*rx_length = old_rx_length;

							/* Card sent 61 xx
							 * xx = 0 means 256 */
							if (rx_buffer[1] == 0)
								/* we want to receive what the APDU asked */
								rx_buffer[1] = Le;
							if (Le == 0)
								/* we want what the card want to send */
								Le = rx_buffer[1];

							/* Get Response with P3 = min(Le, Lx) */
							tx_buffer[4] = rx_buffer[1] < Le ? rx_buffer[1] : Le;

							return_value = CmdXfrBlockShortAPDU(lun, tx_length,
								tx_buffer, rx_length, rx_buffer);
						}
						if ((rx_buffer[0] == 0x90) && (rx_buffer[1] == 0x00))
						{
							/* Get Response */
							DEBUG_COMM("TPDU: Automatic Get Response after 9000");
							*rx_length = old_rx_length;

							/* Card sent 90 00
							 * Get Response with P3 = Le */
							tx_buffer[4] = Le;

							return_value = CmdXfrBlockShortAPDU(lun, tx_length,
								tx_buffer, rx_length, rx_buffer);
						}
					}
#endif
				}
				else
				{
					if (tx_length > (ntestlength + ISO_LENGTH_SIZE))
					{
						/* Data are too long */
						return_value = IFD_COMMUNICATION_ERROR;

						goto clean_up_and_return;
					}
					else
					{
						/* tx_buffer holds a proper TPDU */
						return_value = CmdXfrBlockShortAPDU(lun, tx_length, tx_buffer, rx_length, rx_buffer);
					}
				}
			}
			else
			{
				/* tx_buffer holds too little data to form an APDU+length */
				return_value = IFD_COMMUNICATION_ERROR;

				goto clean_up_and_return;
			}
		}
	}

clean_up_and_return:
	return return_value;
} /* CmdXfrBlockTPDU */


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

