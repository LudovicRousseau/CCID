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
#include "ifdhandler.h"
#include "commands.h"
#include "ccid_usb.h"
#include "defs.h"
#include "config.h"
#include "debug.h"

#define SET_ISO_MODE 1

RESPONSECODE CmdPowerOn(int lun, int * nlength, unsigned char buffer[])
{
	unsigned char cmd[10];
	status_t res;
	int atr_len, length, count = 1;
	RESPONSECODE return_value = IFD_SUCCESS;

	/* store length of buffer[] */
	length = *nlength;
again:
	cmd[0] = 0x62; /* IccPowerOn */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_get_seq(lun);
	cmd[7] = 0x01;	/* 5.0V */
	cmd[8] = cmd[9] = 0; /* RFU */

	res = WriteUSB(lun, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	/* reset available buffer size */
	/* needed if we go back after a switch to ISO mode */
	*nlength = length;

	res = ReadUSB(lun, nlength, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (buffer[STATUS_OFFSET] & 0x40)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */

		/* Protocol error in EMV mode */
		if (buffer[ERROR_OFFSET] == 0xBB)
		{
			DEBUG_INFO("Switch reader in ISO mode");
			if ((return_value = Escape(lun, SET_ISO_MODE)) != IFD_SUCCESS)
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

RESPONSECODE Escape(int lun, int command)
{
	unsigned char cmd[12];
	status_t res;
	int length;
	RESPONSECODE return_value = IFD_SUCCESS;

	cmd[0] = 0x6B; /* PC_to_RDR_Escape */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_get_seq(lun);
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	switch (command)
	{
		/* Gemplus proprietary */
		case SET_ISO_MODE:
			cmd[10] = 0x1F;	/* switch mode */
			cmd[11] = 0x01;	/* set ISO mode */
			cmd[1] = 2;	/* length of data */
			length = 12;
			break;

		default:
			DEBUG_CRITICAL2("Unkown Escape command: %d", command);
			return return_value;
	}

	res = WriteUSB(lun, length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	res = ReadUSB(lun, &length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (cmd[STATUS_OFFSET] & 0x40)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* Escape */

RESPONSECODE CmdPowerOff(int lun)
{
	unsigned char cmd[10];
	status_t res;
	int length;
	RESPONSECODE return_value = IFD_SUCCESS;

	cmd[0] = 0x63; /* IccPowerOff */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_get_seq(lun);
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WriteUSB(lun, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	res = ReadUSB(lun, &length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (cmd[STATUS_OFFSET] & 0x40)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdPowerOff */

RESPONSECODE CmdGetSlotStatus(int lun, unsigned char buffer[])
{
	unsigned char cmd[10];
	status_t res;
	int length;
	RESPONSECODE return_value = IFD_SUCCESS;

	cmd[0] = 0x65; /* GetSlotStatus */
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;	/* dwLength */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_get_seq(lun);
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */

	res = WriteUSB(lun, sizeof(cmd), cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = SIZE_GET_SLOT_STATUS;
	res = ReadUSB(lun, &length, buffer);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (buffer[STATUS_OFFSET] & 0x40)
	{
		ccid_error(buffer[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	return return_value;
} /* CmdGetSlotStatus */

RESPONSECODE CmdXfrBlock(int lun, int tx_length, unsigned char tx_buffer[],
	PDWORD rx_length, unsigned char rx_buffer[])
{
	unsigned char cmd[10+CMD_BUF_SIZE];	/* CCID + APDU buffer */
	status_t res;
	int length;
	RESPONSECODE return_value = IFD_SUCCESS;

	cmd[0] = 0x6F; /* IccPowerOff */
	i2dw(tx_length, cmd+1);	/* APDU length */
	cmd[5] = 0;	/* slot number */
	cmd[6] = ccid_get_seq(lun);
	cmd[7] = cmd[8] = cmd[9] = 0; /* RFU */
	memcpy(cmd+10, tx_buffer, tx_length);

	res = WriteUSB(lun, 10+tx_length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	length = sizeof(cmd);
	res = ReadUSB(lun, &length, cmd);
	if (res != STATUS_SUCCESS)
		return IFD_COMMUNICATION_ERROR;

	if (cmd[STATUS_OFFSET] & 0x40)
	{
		ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);    /* bError */
		*rx_length = 0; /* nothing received */
		return_value = IFD_COMMUNICATION_ERROR;
	}

	length = dw2i(cmd, 1);
	if (length < *rx_length)
		*rx_length = length;
	else
		length = *rx_length;
	memcpy(rx_buffer, cmd+10, length);

	return return_value;
} /* CmdXfrBlock */

void i2dw(int value, unsigned char buffer[])
{
	buffer[0] = value & 0xFF;
	buffer[1] = (value >> 8) & 0xFF;
	buffer[2] = (value >> 16) & 0xFF;
	buffer[3] = (value >> 24) & 0xFF;
} /* i2dw */

