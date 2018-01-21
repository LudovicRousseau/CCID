/*
    ccid.c: CCID common code
    Copyright (C) 2003-2010   Ludovic Rousseau

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

#include <config.h>

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <pcsclite.h>
#include <ifdhandler.h>

#include "debug.h"
#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "commands.h"
#include "utils.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

/*****************************************************************************
 *
 *					ccid_open_hack_pre
 *
 ****************************************************************************/
int ccid_open_hack_pre(unsigned int reader_index)
{
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);

	switch (ccid_descriptor->readerID)
	{
		case MYSMARTPAD:
			ccid_descriptor->dwMaxIFSD = 254;
			break;

		case CL1356D:
			/* the firmware needs some time to initialize */
			(void)sleep(1);
			ccid_descriptor->readTimeout = 60*1000; /* 60 seconds */
			break;

#ifdef ENABLE_ZLP
		case GEMPCTWIN:
		case GEMPCKEY:
		case DELLSCRK:
			/* Only the chipset with firmware version 2.00 is "bogus"
			 * The reader may send packets of 0 bytes when the reader is
			 * connected to a USB 3 port */
			if (0x0200 == ccid_descriptor->IFD_bcdDevice)
			{
				ccid_descriptor->zlp = TRUE;
				DEBUG_INFO1("ZLP fixup");
			}
			break;
#endif

		case OZ776:
		case OZ776_7772:
			ccid_descriptor->dwMaxDataRate = 9600;
			break;

		case ElatecTWN4_CCID_CDC:
		case ElatecTWN4_CCID:
			/* Use a timeout of 1000 ms instead of 100 ms in
			 * CmdGetSlotStatus() used by CreateChannelByNameOrChannel()
			 * The reader answers after up to 1 s if no tag is present */
			ccid_descriptor->readTimeout = DEFAULT_COM_READ_TIMEOUT * 10;
			break;

		case SCM_SCL011:
			/* The SCM SCL011 reader needs 350 ms to answer */
			ccid_descriptor->readTimeout = DEFAULT_COM_READ_TIMEOUT * 4;
			break;
	}

	/* CCID */
	if ((PROTOCOL_CCID == ccid_descriptor->bInterfaceProtocol)
		&& (3 == ccid_descriptor -> bNumEndpoints))
	{
#ifndef TWIN_SERIAL
		/* just wait for 100ms in case a notification is in the pipe */
		(void)InterruptRead(reader_index, 100);
#endif
	}

	/* ICCD type A */
	if (PROTOCOL_ICCD_A == ccid_descriptor->bInterfaceProtocol)
	{
		unsigned char tmp[MAX_ATR_SIZE];
		unsigned int n = sizeof(tmp);

		DEBUG_COMM("ICCD type A");
		(void)CmdPowerOff(reader_index);
		(void)CmdPowerOn(reader_index, &n, tmp, CCID_CLASS_AUTO_VOLTAGE);
		(void)CmdPowerOff(reader_index);
	}

	/* ICCD type B */
	if (PROTOCOL_ICCD_B == ccid_descriptor->bInterfaceProtocol)
	{
		unsigned char tmp[MAX_ATR_SIZE];
		unsigned int n = sizeof(tmp);

		DEBUG_COMM("ICCD type B");
		if (CCID_CLASS_SHORT_APDU ==
			(ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK))
		{
			/* use the extended APDU comm alogorithm */
			ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
			ccid_descriptor->dwFeatures |= CCID_CLASS_EXTENDED_APDU;
		}

		(void)CmdPowerOff(reader_index);
		(void)CmdPowerOn(reader_index, &n, tmp, CCID_CLASS_AUTO_VOLTAGE);
		(void)CmdPowerOff(reader_index);
	}

	return 0;
} /* ccid_open_hack_pre */

#ifndef NO_LOG
/*****************************************************************************
 *
 *					dump_gemalto_firmware_features
 *
 ****************************************************************************/
static void dump_gemalto_firmware_features(struct GEMALTO_FIRMWARE_FEATURES *gff)
{
	DEBUG_INFO2("Dumping Gemalto firmware features (%zd bytes):",
		sizeof(struct GEMALTO_FIRMWARE_FEATURES));

#define YESNO(x) (x) ? "yes" : "no"

	DEBUG_INFO2(" bLogicalLCDLineNumber: %d", gff->bLogicalLCDLineNumber);
	DEBUG_INFO2(" bLogicalLCDRowNumber: %d", gff->bLogicalLCDRowNumber);
	DEBUG_INFO2(" bLcdInfo: 0x%02X", gff->bLcdInfo);
	DEBUG_INFO2(" bEntryValidationCondition: 0x%02X",
		gff->bEntryValidationCondition);

	DEBUG_INFO1(" Reader supports PC/SCv2 features:");
	DEBUG_INFO2("  VerifyPinStart: %s", YESNO(gff->VerifyPinStart));
	DEBUG_INFO2("  VerifyPinFinish: %s", YESNO(gff->VerifyPinFinish));
	DEBUG_INFO2("  ModifyPinStart: %s", YESNO(gff->ModifyPinStart));
	DEBUG_INFO2("  ModifyPinFinish: %s", YESNO(gff->ModifyPinFinish));
	DEBUG_INFO2("  GetKeyPressed: %s", YESNO(gff->GetKeyPressed));
	DEBUG_INFO2("  VerifyPinDirect: %s", YESNO(gff->VerifyPinDirect));
	DEBUG_INFO2("  ModifyPinDirect: %s", YESNO(gff->ModifyPinDirect));
	DEBUG_INFO2("  Abort: %s", YESNO(gff->Abort));
	DEBUG_INFO2("  GetKey: %s", YESNO(gff->GetKey));
	DEBUG_INFO2("  WriteDisplay: %s", YESNO(gff->WriteDisplay));
	DEBUG_INFO2("  SetSpeMessage: %s", YESNO(gff->SetSpeMessage));
	DEBUG_INFO2("  bTimeOut2: %s", YESNO(gff->bTimeOut2));
	DEBUG_INFO2("  bPPDUSupportOverXferBlock: %s",
		YESNO(gff->bPPDUSupportOverXferBlock));
	DEBUG_INFO2("  bPPDUSupportOverEscape: %s",
		YESNO(gff->bPPDUSupportOverEscape));

	DEBUG_INFO2(" bListSupportedLanguages: %s",
		YESNO(gff->bListSupportedLanguages));
	DEBUG_INFO2(" bNumberMessageFix: %s", YESNO(gff->bNumberMessageFix));

	DEBUG_INFO2(" VersionNumber: 0x%02X", gff->VersionNumber);
	DEBUG_INFO2(" MinimumPINSize: %d", gff->MinimumPINSize);
	DEBUG_INFO2(" MaximumPINSize: %d", gff->MaximumPINSize);
	DEBUG_INFO2(" Firewall: %s", YESNO(gff->Firewall));
	if (gff->Firewall && gff->FirewalledCommand_SW1
		&& gff->FirewalledCommand_SW2)
	{
		DEBUG_INFO2("  FirewalledCommand_SW1: 0x%02X",
			gff->FirewalledCommand_SW1);
		DEBUG_INFO2("  FirewalledCommand_SW2: 0x%02X",
			gff->FirewalledCommand_SW2);
	}

} /* dump_gemalto_firmware_features */
#endif

/*****************************************************************************
 *
 *					set_gemalto_firmware_features
 *
 ****************************************************************************/
static void set_gemalto_firmware_features(unsigned int reader_index)
{
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	struct GEMALTO_FIRMWARE_FEATURES *gf_features;

	gf_features = malloc(sizeof(struct GEMALTO_FIRMWARE_FEATURES));
	if (gf_features)
	{
		unsigned char cmd[] = { 0x6A }; /* GET_FIRMWARE_FEATURES command id */
		unsigned int len_features = sizeof *gf_features;
		RESPONSECODE ret;

		ret = CmdEscapeCheck(reader_index, cmd, sizeof cmd,
			(unsigned char*)gf_features, &len_features, 0, TRUE);
		if ((IFD_SUCCESS == ret) &&
		    (len_features == sizeof *gf_features))
		{
			/* Command is supported if it succeeds at CCID level */
			/* and returned size matches our expectation */
			ccid_descriptor->gemalto_firmware_features = gf_features;
#ifndef NO_LOG
			dump_gemalto_firmware_features(gf_features);
#endif
		}
		else
		{
			/* Command is not supported, let's free allocated memory */
			free(gf_features);
			DEBUG_INFO3("GET_FIRMWARE_FEATURES failed: " DWORD_D ", len=%d",
				ret, len_features);
		}
	}
} /* set_gemalto_firmware_features */

/*****************************************************************************
 *
 *					ccid_open_hack_post
 *
 ****************************************************************************/
int ccid_open_hack_post(unsigned int reader_index)
{
	_ccid_descriptor *ccid_descriptor = get_ccid_descriptor(reader_index);
	RESPONSECODE return_value = IFD_SUCCESS;

	switch (ccid_descriptor->readerID)
	{
		case GEMPCKEY:
		case GEMPCTWIN:
			/* Reader announces TPDU but can do APDU (EMV in fact) */
			if (DriverOptions & DRIVER_OPTION_GEMPC_TWIN_KEY_APDU)
			{
				unsigned char cmd[] = { 0x1F, 0x02 };
				unsigned char res[10];
				unsigned int length_res = sizeof(res);

				if (CmdEscape(reader_index, cmd, sizeof(cmd), res, &length_res, 0) == IFD_SUCCESS)
				{
					ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
					ccid_descriptor->dwFeatures |= CCID_CLASS_SHORT_APDU;
				}
			}
			break;

		case VEGAALPHA:
		case GEMPCPINPAD:
			/* load the l10n strings in the pinpad memory */
			{
#define L10N_HEADER_SIZE 5
#define L10N_STRING_MAX_SIZE 16
#define L10N_NB_STRING 10

				unsigned char cmd[L10N_HEADER_SIZE + L10N_NB_STRING * L10N_STRING_MAX_SIZE];
				unsigned char res[20];
				unsigned int length_res = sizeof(res);
				int offset, i, j;

				const char *fr[] = {
					"Entrer PIN",
					"Nouveau PIN",
					"Confirmer PIN",
					"PIN correct",
					"PIN Incorrect !",
					"Delai depasse",
					"* essai restant",
					"Inserer carte",
					"Erreur carte",
					"PIN bloque" };

				const char *de[] = {
					"PIN eingeben",
					"Neue PIN",
					"PIN bestatigen",
					"PIN korrect",
					"Falsche PIN !",
					"Zeit abgelaufen",
					"* Versuche ubrig",
					"Karte einstecken",
					"Fehler Karte",
					"PIN blockiert" };

				const char *es[] = {
					"Introducir PIN",
					"Nuevo PIN",
					"Confirmar PIN",
					"PIN OK",
					"PIN Incorrecto !",
					"Tiempo Agotado",
					"* ensayos quedan",
					"Introducir Tarj.",
					"Error en Tarjeta",
					"PIN bloqueado" };

				const char *it[] = {
					"Inserire PIN",
					"Nuovo PIN",
					"Confermare PIN",
					"PIN Corretto",
					"PIN Errato !",
					"Tempo scaduto",
					"* prove rimaste",
					"Inserire Carta",
					"Errore Carta",
					"PIN ostruito"};

				const char *pt[] = {
					"Insira PIN",
					"Novo PIN",
					"Conf. novo PIN",
					"PIN OK",
					"PIN falhou!",
					"Tempo expirou",
					"* tentiv. restam",
					"Introduza cartao",
					"Erro cartao",
					"PIN bloqueado" };

				const char *nl[] = {
					"Inbrengen code",
					"Nieuwe code",
					"Bevestig code",
					"Code aanvaard",
					"Foute code",
					"Time out",
					"* Nog Pogingen",
					"Kaart inbrengen",
					"Kaart fout",
					"Kaart blok" };

				const char *tr[] = {
					"PIN Giriniz",
					"Yeni PIN",
					"PIN Onayala",
					"PIN OK",
					"Yanlis PIN",
					"Zaman Asimi",
					"* deneme kaldi",
					"Karti Takiniz",
					"Kart Hatasi",
					"Kart Kilitli" };

				const char *en[] = {
					"Enter PIN",
					"New PIN",
					"Confirm PIN",
					"PIN OK",
					"Incorrect PIN!",
					"Time Out",
					"* retries left",
					"Insert Card",
					"Card Error",
					"PIN blocked" };

				const char *lang;
				const char **l10n;

#ifdef __APPLE__
				CFArrayRef cfa;
				CFStringRef slang;

				/* Get the complete ordered list */
				cfa = CFLocaleCopyPreferredLanguages();

				/* Use the first/preferred language
				 * As the driver is run as root we get the language
				 * selected during install */
				slang = CFArrayGetValueAtIndex(cfa, 0);

				/* CFString -> C string */
				lang = CFStringGetCStringPtr(slang, kCFStringEncodingMacRoman);
#else
				/* The other Unixes just use the LANG env variable */
				lang = getenv("LANG");
#endif
				DEBUG_COMM2("Using lang: %s", lang);
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
					else if (0 == strncmp(lang, "pt", 2))
						l10n = pt;
					else if (0 == strncmp(lang, "nl", 2))
						l10n = nl;
					else if (0 == strncmp(lang, "tr", 2))
						l10n = tr;
					else
						l10n = en;
				}

#ifdef __APPLE__
				/* Release the allocated array */
				CFRelease(cfa);
#endif
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

				(void)sleep(1);
				if (IFD_SUCCESS == CmdEscape(reader_index, cmd, sizeof(cmd), res, &length_res, DEFAULT_COM_READ_TIMEOUT))
				{
					DEBUG_COMM("l10n string loaded successfully");
				}
				else
				{
					DEBUG_COMM("Failed to load l10n strings");
					return_value = IFD_COMMUNICATION_ERROR;
				}

				if (DriverOptions & DRIVER_OPTION_DISABLE_PIN_RETRIES)
				{
					/* disable VERIFY from reader */
					const unsigned char cmd2[] = {0xb5, 0x00};
					length_res = sizeof(res);
					if (IFD_SUCCESS == CmdEscape(reader_index, cmd2, sizeof(cmd2), res, &length_res, DEFAULT_COM_READ_TIMEOUT))
					{
						DEBUG_COMM("Disable SPE retry counter successful");
					}
					else
					{
						DEBUG_CRITICAL("Failed to disable SPE retry counter");
					}
				}
			}
			break;

		case HPSMARTCARDKEYBOARD:
		case HP_CCIDSMARTCARDKEYBOARD:
		case FUJITSUSMARTKEYB:
			/* the Secure Pin Entry is bogus so disable it
			 * https://web.archive.org/web/20120320001756/http://martinpaljak.net/2011/03/19/insecure-hp-usb-smart-card-keyboard/
			 *
			 * The problem is that the PIN code entered using the Secure
			 * Pin Entry function is also sent to the host.
			 */

		case C3PO_LTC31_v2:
			ccid_descriptor->bPINSupport = 0;
			break;

		case HID_AVIATOR:      /* OMNIKEY Generic */
		case HID_OMNIKEY_3X21: /* OMNIKEY 3121 or 3021 or 1021 */
		case HID_OMNIKEY_6121: /* OMNIKEY 6121 */
		case CHERRY_XX44:      /* Cherry Smart Terminal xx44 */
		case FUJITSU_D323:     /* Fujitsu Smartcard Reader D323 */
			/* The chip advertises pinpad but actually doesn't have one */
			ccid_descriptor->bPINSupport = 0;
			/* Firmware uses chaining */
			ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
			ccid_descriptor->dwFeatures |= CCID_CLASS_EXTENDED_APDU;
			break;

#if 0
		/* SCM SCR331-DI contactless */
		case SCR331DI:
		/* SCM SCR331-DI-NTTCOM contactless */
		case SCR331DINTTCOM:
		/* SCM SDI010 contactless */
		case SDI010:
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

				if ((IFD_SUCCESS == CmdEscape(reader_index, cmd1, sizeof(cmd1), res, &length_res, 0))
					&& (IFD_SUCCESS == CmdEscape(reader_index, cmd2, sizeof(cmd2), res, &length_res, 0)))
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
#endif
		case CHERRY_KC1000SC:
			if ((0x0100 == ccid_descriptor->IFD_bcdDevice)
				&& (ccid_descriptor->dwFeatures & CCID_CLASS_EXCHANGE_MASK) == CCID_CLASS_SHORT_APDU)
			{
				/* firmware 1.00 is bogus
				 * With a T=1 card and case 2 APDU (data from card to
				 * host) the maximum size returned by the reader is 128
				 * byes. The reader is then using chaining as with
				 * extended APDU.
				 */
				ccid_descriptor->dwFeatures &= ~CCID_CLASS_EXCHANGE_MASK;
				ccid_descriptor->dwFeatures |= CCID_CLASS_EXTENDED_APDU;
			}
			break;

		case ElatecTWN4_CCID_CDC:
		case ElatecTWN4_CCID:
		case SCM_SCL011:
			/* restore default timeout (modified in ccid_open_hack_pre()) */
			ccid_descriptor->readTimeout = DEFAULT_COM_READ_TIMEOUT;
			break;
	}

	/* Gemalto readers may report additional information */
	if (GET_VENDOR(ccid_descriptor->readerID) == VENDOR_GEMALTO)
		set_gemalto_firmware_features(reader_index);

	return return_value;
} /* ccid_open_hack_post */

/*****************************************************************************
 *
 *					ccid_error
 *
 ****************************************************************************/
void ccid_error(int log_level, int error, const char *file, int line,
	const char *function)
{
#ifndef NO_LOG
	const char *text;
	char var_text[30];

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
				(void)snprintf(var_text, sizeof(var_text), "error on byte %d",
					error);
			else
				(void)snprintf(var_text, sizeof(var_text),
					"Unknown CCID error: 0x%02X", error);

			text = var_text;
			break;
	}
	log_msg(log_level, "%s:%d:%s %s", file, line, function, text);
#endif

} /* ccid_error */

