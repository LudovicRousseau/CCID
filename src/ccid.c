/*
    ccid.c: CCID common code
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

#include <stdio.h>
#include <pcsclite.h>
#include <ifdhandler.h>

#include "config.h"
#include "debug.h"
#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "commands.h"

/*****************************************************************************
 *
 *					ccid_open_hack
 *
 ****************************************************************************/
int ccid_open_hack(int lun)
{
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(lun);

	switch (ccid_descriptor->readerID)
	{
		case CARDMAN3121+1:
			/* Reader announces APDU but is in fact TPDU */
			ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
			ccid_descriptor->dwFeatures |= CCID_CLASS_TPDU;
			break;

			/*
			 * Do not switch to APDU mode since it also swicth in EMV mode and
			 * may not work with non EMV cards
			 */
		case GEMPCKEY:
		case GEMPCTWIN:
			/* Reader announces TPDU but can do APDU */
			if (DriverOptions & DRIVER_OPTION_GEMPC_TWIN_KEY_APDU)
			{
				unsigned char cmd[] = "\xA0\x02";
				unsigned char res[10];
				unsigned long length_res = sizeof(res);

				if (CmdEscape(lun, cmd, sizeof(cmd)-1, res, &length_res) == IFD_SUCCESS)
				{
					ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
					ccid_descriptor->dwFeatures |= CCID_CLASS_SHORT_APDU;
				}
			}
			break;
	}

	return 0;
} /* ccid_open_hack */

/*****************************************************************************
 *
 *					ccid_error
 *
 ****************************************************************************/
void ccid_error(int error, char *file, int line)
{
	char *text;

	switch (error)
	{
		case 0x00:
			text = "Command not supported or not allowed";
			break;

		case 0x01:
			text = "Wrong command length";
			break;

		case 0x02:
			text = "Reader detects an excessive current. Card powered off";
			break;

		case 0x03:
			text = "Reader detects a defective voltage. Card powered off";
			break;

		case 0x05:
			text = "Slot number is invalid (it must be set to 0)";
			break;

		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x15:
			text = "Byte displayed is invalid";
			break;

		case 0xA2:
			text = "Card short-circuiting. Card powered off";
			break;

		case 0xA3:
			text = "ATR too long (> 33)";
			break;

		case 0xB0:
			text = "Reader in EMV mode and T=1 message too long";
			break;

		case 0xBB:
			text = "Protocol error in EMV mode";
			break;

		case 0xBD:
			text = "Card error during T=1 exchange";
			break;

		case 0xBE:
			text = "Wrong APDU command length";
			break;

		case 0xF4:
			text = "Procedure byte conflict";
			break;

		case 0xF7:
			text = "Invalid ATR checksum byte (TCK)";
			break;

		case 0xF8:
			text = "Invalid ATR first byte";
			break;

		case 0xFD:
			text = "Parity error during exchange";
			break;

		case 0xFE:
			text = "Card absent or mute";
			break;

		default:
			text = "Unknown CCID error";
			break;

	}
	debug_msg("%s:%d %s", file, line, text);

} /* ccid_error */

