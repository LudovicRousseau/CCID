/*
    commands.h: Commands sent to the card
    Copyright (C) 2003   Ludovic Rousseau

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

#define SIZE_GET_SLOT_STATUS 10
#define STATUS_OFFSET 7
#define ERROR_OFFSET 8

RESPONSECODE CmdPowerOn(unsigned int reader_index, unsigned int * nlength,
	unsigned char buffer[], int voltage);

RESPONSECODE SecurePINVerify(unsigned int reader_index,
	const unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength);

RESPONSECODE SecurePINModify(unsigned int reader_index,
	unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength);

RESPONSECODE CmdEscape(unsigned int reader_index,
	const unsigned char TxBuffer[], unsigned int TxLength,
	unsigned char RxBuffer[], unsigned int *RxLength);

RESPONSECODE CmdPowerOff(unsigned int reader_index);

RESPONSECODE CmdGetSlotStatus(unsigned int reader_index,
	unsigned char buffer[]);

RESPONSECODE CmdXfrBlock(unsigned int reader_index, unsigned int tx_length,
	unsigned char tx_buffer[], unsigned int *rx_length,
	unsigned char rx_buffer[], int protoccol);

RESPONSECODE CCID_Transmit(unsigned int reader_index, unsigned int tx_length,
	const unsigned char tx_buffer[], unsigned short rx_length, unsigned char bBWI);

RESPONSECODE CCID_Receive(unsigned int reader_index, unsigned int *rx_length,
	unsigned char rx_buffer[]);

RESPONSECODE SetParameters(unsigned int reader_index, char protocol,
	unsigned int length, unsigned char buffer[]);

int isCharLevel(int reader_index);

