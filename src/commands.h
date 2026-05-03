/*
    commands.h: Commands sent to the card
    Copyright (C) 2003-2025   Ludovic Rousseau

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

#include "defs.h"

#define SIZE_GET_SLOT_STATUS 10
#define STATUS_OFFSET 7
#define ERROR_OFFSET 8
#define CHAIN_PARAMETER_OFFSET 9
#define CCID_RESPONSE_HEADER_SIZE 10

RESPONSECODE CmdPowerOn(CcidDesc * ccid_reader, unsigned int * nlength,
	/*@out@*/ unsigned char buffer[], int voltage);

RESPONSECODE SecurePINVerify(CcidDesc * ccid_slot,
	unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength);

RESPONSECODE SecurePINModify(CcidDesc * ccid_slot,
	unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength);

RESPONSECODE CmdEscape(CcidDesc * ccid_reader,
	const unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength, unsigned int timeout);

RESPONSECODE CmdEscapeCheck(CcidDesc * ccid_reader,
	const unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength, unsigned int timeout,
	bool mayfail);

RESPONSECODE CmdPowerOff(CcidDesc * ccid_reader);

RESPONSECODE CmdGetSlotStatus(CcidDesc * ccid_reader,
	/*@out@*/ unsigned char buffer[static SIZE_GET_SLOT_STATUS]);

RESPONSECODE CmdXfrBlock(CcidDesc * ccid_slot, unsigned int tx_length,
	unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[], int protoccol);

RESPONSECODE CCID_Transmit(CcidDesc * ccid_reader, unsigned int tx_length,
	const unsigned char tx_buffer[], unsigned short rx_length, unsigned char bBWI);

RESPONSECODE CCID_Receive(CcidDesc * ccid_reader,
	/*@out@*/ unsigned int *rx_length,
	/*@out@*/ unsigned char rx_buffer[], unsigned char *chain_parameter);

RESPONSECODE SetParameters(CcidDesc * ccid_reader, char protocol,
	unsigned int length, unsigned char buffer[]);

int isCharLevel(CcidDesc * ccid_reader);

