/*
    ccid.c: CCID common code
    Copyright (C) 2003-2005   Ludovic Rousseau

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
int ccid_open_hack(unsigned int reader_index)
{
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	switch (ccid_descriptor->readerID)
	{
		case CARDMAN3121+1:
			/* Reader announces APDU but is in fact TPDU */
			ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
			ccid_descriptor->dwFeatures |= CCID_CLASS_TPDU;
			break;

		case GEMPCKEY:
		case GEMPCTWIN:
			/* Reader announces TPDU but can do APDU */
			if (DriverOptions & DRIVER_OPTION_GEMPC_TWIN_KEY_APDU)
			{
				unsigned char cmd[] = { 0xA0, 0x02 };
				unsigned char res[10];
				unsigned int length_res = sizeof(res);

				if (CmdEscape(reader_index, cmd, sizeof(cmd), res, &length_res) == IFD_SUCCESS)
				{
					ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
					ccid_descriptor->dwFeatures |= CCID_CLASS_SHORT_APDU;
				}
			}
			break;

		case GEMPCPINPAD:
			/* load the l10n strings in the pinpad memory */
			{
#define L10N_HEADER_SIZE 5
#define L10N_STRING_MAX_SIZE 16
#define L10N_NB_STRING 9

				unsigned char cmd[L10N_HEADER_SIZE + L10N_NB_STRING * L10N_STRING_MAX_SIZE];
				unsigned char res[20];
				unsigned int length_res = sizeof(res);
				int offset, i, j;

				char *fr[] = {
					"Entrer PIN",
					"Nouveau PIN",
					"Confirmer PIN",
					"PIN correct",
					"PIN Incorrect !",
					"Delai depasse",
					"* essai restant",
					"Inserer carte",
					"Erreur carte" };

				char *de[] = {
					"PIN eingeben",
					"Neue PIN",
					"PIN bestatigen",
					"PIN korrect",
					"Falsche PIN !",
					"Zeit abgelaufen",
					"* Versuche ubrig",
					"Karte einstecken",
					"Fehler Karte" };

				char *es[] = {
					"Introducir PIN",
					"Nuevo PIN",
					"Confirmar PIN",
					"PIN OK",
					"PIN Incorrecto !",
					"Tiempo Agotado",
					"quedan * ensayos",
					"Introducir Tarj.",
					"Error en Tarjeta" };

				char *it[] = {
					"Inserire PIN",
					"Nuovo PIN",
					"Confermare PIN",
					"PIN Corretto",
					"PIN Errato !",
					"Tempo scaduto",
					"* prove rimaste",
					"Inserire Carta",
					"Errore Carta" };

				char *en[] = {
					"Enter PIN",
					"New PIN",
					"Confirm PIN",
					"PIN OK",
					"Incorrect PIN!",
					"Time Out",
					"* retries left",
					"Insert Card",
					"Card Error" };

				char *lang;
				char **l10n;

				lang = getenv("LANG");
				if (NULL == lang)
					l10n = en;
				else
				{
					if (0 == strncmp(lang, "fr", 2))
						l10n = fr;
					else if (0 == strncmp(lang, "de", 2))
						l10n = de;
					else if (0 == strncmp(lang, "es", 2))
						l10n = es;
					else if (0 == strncmp(lang, "it", 2))
						l10n = it;
					else
						l10n = en;
				}

				offset = 0;
				cmd[offset++] = 0xB2;	/* load strings */
				cmd[offset++] = 0xA0;	/* address of the memory */
				cmd[offset++] = 0x00;	/* address of the first byte */
				cmd[offset++] = 0x4D;	/* magic value */
				cmd[offset++] = 0x4C;	/* magic value */

				/* for each string */
				for (i=0; i<L10N_NB_STRING; i++)
				{
					/* copy the string */
					for (j=0; l10n[i][j]; j++)
						cmd[offset++] = l10n[i][j];

					/* pad with " " */
					for (; j<L10N_STRING_MAX_SIZE; j++)
						cmd[offset++] = ' ';
				}

				if (IFD_SUCCESS == CmdEscape(reader_index, cmd, sizeof(cmd), res, &length_res))
				{
					DEBUG_COMM("l10n string loaded successfully");
				}
				else
				{
					DEBUG_COMM("Failed to load l10n strings");
				}
			}
			break;

		/* SCM SCR331-DI contactless */
		case SCR331DI:
			/* the contactless reader is in the second slot */
			if (ccid_descriptor->bCurrentSlotIndex > 0)
			{
				unsigned char cmd1[] = { 0x00 };
				/*  command: 00 ??
				 * response: 06 10 03 03 00 00 00 01 FE FF FF FE 01 ?? */
				unsigned char cmd2[] = { 0x02 };
				/*  command: 02 ??
				 * response: 00 ?? */

				unsigned char res[20];
				unsigned int length_res = sizeof(res);

				if ((IFD_SUCCESS == CmdEscape(reader_index, cmd1, sizeof(cmd1), res, &length_res))
					&& (IFD_SUCCESS == CmdEscape(reader_index, cmd2, sizeof(cmd2), res, &length_res)))
				{
					DEBUG_COMM("SCM SCR331-DI contactless detected");
				}
				else
				{
					DEBUG_COMM("SCM SCR331-DI contactless init failed");
				}

				/* hack since the contactless reader do not share dwFeatures */
				ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
				ccid_descriptor->dwFeatures |= CCID_CLASS_SHORT_APDU;

				ccid_descriptor->dwFeatures |= CCID_CLASS_AUTO_IFSD;
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
void ccid_error(int error, char *file, int line, const char *function)
{
	char *text;
	char var_text[20];

	switch (error)
	{
		case 0x00:
			text = "Command not supported or not allowed";
			break;

		case 0x01:
			text = "Wrong command length";
			break;

		case 0x05:
			text = "Invalid slot number";
			break;

		case 0xA2:
			text = "Card short-circuiting. Card powered off";
			break;

		case 0xA3:
			text = "ATR too long (> 33)";
			break;

		case 0xAB:
			text = "No data exchanged";
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

		case 0xE0:
			text = "Slot busy";
			break;

		case 0xEF:
			text = "PIN cancelled";
			break;

		case 0xF0:
			text = "PIN timeout";
			break;

		case 0xF2:
			text = "Busy with autosequence";
			break;

		case 0xF3:
			text = "Deactivated protocol";
			break;

		case 0xF4:
			text = "Procedure byte conflict";
			break;

		case 0xF5:
			text = "Class not supported";
			break;

		case 0xF6:
			text = "Protocol not supported";
			break;

		case 0xF7:
			text = "Invalid ATR checksum byte (TCK)";
			break;

		case 0xF8:
			text = "Invalid ATR first byte";
			break;

		case 0xFB:
			text = "Hardware error";
			break;
			
		case 0xFC:
			text = "Overrun error";
			break;

		case 0xFD:
			text = "Parity error during exchange";
			break;

		case 0xFE:
			text = "Card absent or mute";
			break;

		case 0xFF:
			text = "Activity aborted by Host";
			break;

		default:
			if ((error >= 1) && (error <= 127))
			{
				sprintf(var_text, "error on byte %d", error);
				text = var_text;
			}
			else
				text = "Unknown CCID error";
			break;
	}
	log_msg(PCSC_LOG_ERROR, "%s:%d:%s %s", file, line, function, text);

} /* ccid_error */

