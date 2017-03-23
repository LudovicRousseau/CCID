/*
    scardcontrol.c: sample code to use/test SCardControl() API
    Copyright (C) 2004-2011   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 51
	Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif
#include <reader.h>

#include "PCSCv2part10.h"

#define VERIFY_PIN
#undef MODIFY_PIN
#undef GET_GEMPC_FIRMWARE

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE     SCARD_CTL_CODE(1)

#define BLUE "\33[34m"
#define RED "\33[31m"
#define BRIGHT_RED "\33[01;31m"
#define GREEN "\33[32m"
#define NORMAL "\33[0m"
#define MAGENTA "\33[35m"

/* DWORD printf(3) format */
#ifdef __APPLE__
/* Apple defines DWORD as uint32_t so %d is correct */
#define LF
#else
/* pcsc-lite defines DWORD as unsigned long so %ld is correct */
#define LF "l"
#endif

/* PCSC error message pretty print */
#define PCSC_ERROR_EXIT(rv, text) \
if (rv != SCARD_S_SUCCESS) \
{ \
	printf(text ": " RED "%s (0x%"LF"X)\n" NORMAL, pcsc_stringify_error(rv), rv); \
	goto end; \
} \
else \
	printf(text ": " BLUE "OK\n\n" NORMAL);

#define PCSC_ERROR_CONT(rv, text) \
if (rv != SCARD_S_SUCCESS) \
	printf(text ": " BLUE "%s (0x%"LF"X)\n" NORMAL, pcsc_stringify_error(rv), rv); \
else \
	printf(text ": " BLUE "OK\n\n" NORMAL);

#define PRINT_GREEN(text, value) \
	printf("%s: " GREEN "%s\n" NORMAL, text, value)

#define PRINT_GREEN_DEC(text, value) \
	printf("%s: " GREEN "%d\n" NORMAL, text, value)

#define PRINT_RED_DEC(text, value) \
	printf("%s: " RED "%d\n" NORMAL, text, value)

#define PRINT_GREEN_HEX2(text, value) \
	printf("%s: " GREEN "0x%02X\n" NORMAL, text, value)

#define PRINT_GREEN_HEX4(text, value) \
	printf("%s: " GREEN "0x%04X\n" NORMAL, text, value)

static void parse_properties(unsigned char *bRecvBuffer, int length)
{
	unsigned char *p;
	int i;

	p = bRecvBuffer;
	while (p-bRecvBuffer < length)
	{
		int tag, len, value;

		tag = *p++;
		len = *p++;

		switch(len)
		{
			case 1:
				value = *p;
				break;
			case 2:
				value = *p + (*(p+1)<<8);
				break;
			case 4:
				value = *p + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24);
				break;
			default:
				value = -1;
		}

		switch(tag)
		{
			case PCSCv2_PART10_PROPERTY_wLcdLayout:
				PRINT_GREEN_HEX4(" wLcdLayout", value);
				break;
			case PCSCv2_PART10_PROPERTY_bEntryValidationCondition:
				PRINT_GREEN_HEX2(" bEntryValidationCondition", value);
				break;
			case PCSCv2_PART10_PROPERTY_bTimeOut2:
				PRINT_GREEN_HEX2(" bTimeOut2", value);
				break;
			case PCSCv2_PART10_PROPERTY_wLcdMaxCharacters:
				PRINT_GREEN_HEX4(" wLcdMaxCharacters", value);
				break;
			case PCSCv2_PART10_PROPERTY_wLcdMaxLines:
				PRINT_GREEN_HEX4(" wLcdMaxLines", value);
				break;
			case PCSCv2_PART10_PROPERTY_bMinPINSize:
				PRINT_GREEN_HEX2(" bMinPINSize", value);
				break;
			case PCSCv2_PART10_PROPERTY_bMaxPINSize:
				PRINT_GREEN_HEX2(" bMaxPINSize", value);
				break;
			case PCSCv2_PART10_PROPERTY_sFirmwareID:
				printf(" sFirmwareID: " GREEN);
				for (i=0; i<len; i++)
					putchar(p[i]);
				printf(NORMAL "\n");
				break;
			case PCSCv2_PART10_PROPERTY_bPPDUSupport:
				PRINT_GREEN_HEX2(" bPPDUSupport", value);
				if (value & 1)
					printf("  PPDU is supported over SCardControl using FEATURE_CCID_ESC_COMMAND\n");
				if (value & 2)
					printf("  PPDU is supported over SCardTransmit\n");
				break;
			case PCSCv2_PART10_PROPERTY_dwMaxAPDUDataSize:
				PRINT_GREEN_DEC(" dwMaxAPDUDataSize", value);
				break;
			case PCSCv2_PART10_PROPERTY_wIdVendor:
				PRINT_GREEN_HEX2(" wIdVendor", value);
				break;
			case PCSCv2_PART10_PROPERTY_wIdProduct:
				PRINT_GREEN_HEX2(" wIdProduct", value);
				break;
			default:
				printf(" Unknown tag: 0x%02X (length = %d)\n", tag, len);
		}

		p += len;
	}
} /* parse_properties */


static const char *pinpad_return_codes(unsigned char bRecvBuffer[])
{
	const char * ret = "UNKNOWN";

	if ((0x90 == bRecvBuffer[0]) && (0x00 == bRecvBuffer[1]))
		ret = "Success";

	if (0x64 == bRecvBuffer[0])
	{
		switch (bRecvBuffer[1])
		{
			case 0x00:
				ret = "Timeout";
				break;

			case 0x01:
				ret = "Cancelled by user";
				break;

			case 0x02:
				ret = "PIN mismatch";
				break;

			case 0x03:
				ret = "Too short or too long PIN";
				break;
		}
	}

	return ret;
}

int main(int argc, char *argv[])
{
	LONG rv;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	LPSTR mszReaders = NULL;
	char *ptr, **readers = NULL;
	int nbReaders;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwReaderLen, dwState, dwProt, dwAtrLen;
	BYTE pbAtr[MAX_ATR_SIZE] = "";
	char pbReader[MAX_READERNAME] = "";
	int reader_nb;
	unsigned int i;
	unsigned char bSendBuffer[MAX_BUFFER_SIZE];
	unsigned char bRecvBuffer[MAX_BUFFER_SIZE];
	DWORD send_length, length;
	DWORD verify_ioctl = 0;
	DWORD modify_ioctl = 0;
	DWORD pin_properties_ioctl = 0;
	DWORD mct_readerdirect_ioctl = 0;
	DWORD properties_in_tlv_ioctl = 0;
	DWORD ccid_esc_command = 0;
	SCARD_IO_REQUEST pioRecvPci;
	SCARD_IO_REQUEST pioSendPci;
	PCSC_TLV_STRUCTURE *pcsc_tlv;
#if defined(VERIFY_PIN) | defined(MODIFY_PIN)
	int offset;
#endif
#ifdef VERIFY_PIN
	PIN_VERIFY_STRUCTURE *pin_verify;
#endif
#ifdef MODIFY_PIN
	PIN_MODIFY_STRUCTURE *pin_modify;
#endif
	int PIN_min_size = 4;
	int PIN_max_size = 8;

	/* table for bEntryValidationCondition
	 * 0x01: Max size reached
	 * 0x02: Validation key pressed
	 * 0x04: Timeout occured
	 */
	int bEntryValidationCondition = 7;

	printf("SCardControl sample code\n");
	printf("V 1.4 © 2004-2010, Ludovic Rousseau <ludovic.rousseau@free.fr>\n\n");

	printf(MAGENTA "THIS PROGRAM IS NOT DESIGNED AS A TESTING TOOL!\n");
	printf("Do NOT use it unless you really know what you do.\n\n" NORMAL);

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext: Cannot Connect to Resource Manager %"LF"X\n", rv);
		return 1;
	}

	/* Retrieve the available readers list */
	rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
	PCSC_ERROR_EXIT(rv, "SCardListReaders")

	mszReaders = malloc(sizeof(char)*dwReaders);
	if (mszReaders == NULL)
	{
		printf("malloc: not enough memory\n");
		goto end;
	}

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	if (rv != SCARD_S_SUCCESS)
		printf("SCardListReader: %"LF"X\n", rv);

	/* Extract readers from the null separated string and get the total
	 * number of readers */
	nbReaders = 0;
	ptr = mszReaders;
	while (*ptr != '\0')
	{
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

	if (nbReaders == 0)
	{
		printf("No reader found\n");
		goto end;
	}

	/* allocate the readers table */
	readers = calloc(nbReaders, sizeof(char *));
	if (NULL == readers)
	{
		printf("Not enough memory for readers[]\n");
		goto end;
	}

	/* fill the readers table */
	nbReaders = 0;
	ptr = mszReaders;
	printf("Available readers (use command line argument to select)\n");
	while (*ptr != '\0')
	{
		printf("%d: %s\n", nbReaders, ptr);
		readers[nbReaders] = ptr;
		ptr += strlen(ptr)+1;
		nbReaders++;
	}
	printf("\n");

	if (argc > 1)
	{
		reader_nb = atoi(argv[1]);
		if (reader_nb < 0 || reader_nb >= nbReaders)
		{
			printf("Wrong reader index: %d\n", reader_nb);
			goto end;
		}
	}
	else
		reader_nb = 0;

	/* connect to a reader (even without a card) */
	dwActiveProtocol = -1;
	printf("Using reader: " GREEN "%s\n" NORMAL, readers[reader_nb]);
	rv = SCardConnect(hContext, readers[reader_nb], SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	printf(" Protocol: " GREEN "%"LF"d\n" NORMAL, dwActiveProtocol);
	PCSC_ERROR_EXIT(rv, "SCardConnect")

#ifdef GET_GEMPC_FIRMWARE
	/* get GemPC firmware */
	printf(" Get GemPC Firmware\n");

	/* this is specific to Gemalto readers */
	bSendBuffer[0] = 0x02;
	rv = SCardControl(hCard, IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE, bSendBuffer,
		1, bRecvBuffer, sizeof(bRecvBuffer), &length);

	printf(" Firmware: " GREEN);
	for (i=0; i<length; i++)
		printf("%02X ", bRecvBuffer[i]);
	printf(NORMAL "\n");

	bRecvBuffer[length] = '\0';
	printf(" Firmware: " GREEN "%s" NORMAL" (length " GREEN "%ld" NORMAL " bytes)\n", bRecvBuffer, length);

	PCSC_ERROR_CONT(rv, "SCardControl")
#endif

	/* does the reader support PIN verification? */
	rv = SCardControl(hCard, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0,
		bRecvBuffer, sizeof(bRecvBuffer), &length);
	PCSC_ERROR_EXIT(rv, "SCardControl")

	printf(" TLV (%"LF"d): " GREEN, length);
	for (i=0; i<length; i++)
		printf("%02X ", bRecvBuffer[i]);
	printf(NORMAL "\n");

	PCSC_ERROR_CONT(rv, "SCardControl(CM_IOCTL_GET_FEATURE_REQUEST)")

	if (length % sizeof(PCSC_TLV_STRUCTURE))
	{
		printf("Inconsistent result! Bad TLV values!\n");
		goto end;
	}

	/* get the number of elements instead of the complete size */
	length /= sizeof(PCSC_TLV_STRUCTURE);

	pcsc_tlv = (PCSC_TLV_STRUCTURE *)bRecvBuffer;
	for (i = 0; i < length; i++)
	{
		switch (pcsc_tlv[i].tag)
		{
			case FEATURE_VERIFY_PIN_DIRECT:
				PRINT_GREEN("Reader supports", "FEATURE_VERIFY_PIN_DIRECT");
				verify_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_MODIFY_PIN_DIRECT:
				PRINT_GREEN("Reader supports", "FEATURE_MODIFY_PIN_DIRECT");
				modify_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_IFD_PIN_PROPERTIES:
				PRINT_GREEN("Reader supports", "FEATURE_IFD_PIN_PROPERTIES");
				pin_properties_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_MCT_READER_DIRECT:
				PRINT_GREEN("Reader supports", "FEATURE_MCT_READER_DIRECT");
				mct_readerdirect_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_GET_TLV_PROPERTIES:
				PRINT_GREEN("Reader supports", "FEATURE_GET_TLV_PROPERTIES");
				properties_in_tlv_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_CCID_ESC_COMMAND:
				PRINT_GREEN("Reader supports", "FEATURE_CCID_ESC_COMMAND");
				ccid_esc_command = ntohl(pcsc_tlv[i].value);
				break;
			default:
				PRINT_RED_DEC("Can't parse tag", pcsc_tlv[i].tag);
		}
	}
	printf("\n");

	if (properties_in_tlv_ioctl)
	{
		int value;
		int ret;

		rv = SCardControl(hCard, properties_in_tlv_ioctl, NULL, 0,
			bRecvBuffer, sizeof(bRecvBuffer), &length);
		PCSC_ERROR_CONT(rv, "SCardControl(GET_TLV_PROPERTIES)")

		printf("GET_TLV_PROPERTIES (" GREEN "%"LF"d" NORMAL "): " GREEN, length);
		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf(NORMAL "\n");

		printf("\nDisplay all the properties:\n");
		parse_properties(bRecvBuffer, length);

		printf("\nFind a specific property:\n");
		ret = PCSCv2Part10_find_TLV_property_by_tag_from_buffer(bRecvBuffer, length, PCSCv2_PART10_PROPERTY_wIdVendor, &value);
		if (ret)
			PRINT_RED_DEC(" wIdVendor", ret);
		else
			PRINT_GREEN_HEX4(" wIdVendor", value);

		ret = PCSCv2Part10_find_TLV_property_by_tag_from_hcard(hCard, PCSCv2_PART10_PROPERTY_wIdProduct, &value);
		if (ret)
			PRINT_RED_DEC(" wIdProduct", ret);
		else
			PRINT_GREEN_HEX4(" wIdProduct", value);

		ret = PCSCv2Part10_find_TLV_property_by_tag_from_hcard(hCard, PCSCv2_PART10_PROPERTY_bMinPINSize, &value);
		if (0 == ret)
		{
			PIN_min_size = value;
			PRINT_GREEN_DEC(" PIN min size defined", PIN_min_size);
		}


		ret = PCSCv2Part10_find_TLV_property_by_tag_from_hcard(hCard, PCSCv2_PART10_PROPERTY_bMaxPINSize, &value);
		if (0 == ret)
		{
			PIN_max_size = value;
			PRINT_GREEN_DEC(" PIN max size defined", PIN_max_size);
		}

		ret = PCSCv2Part10_find_TLV_property_by_tag_from_hcard(hCard, PCSCv2_PART10_PROPERTY_bEntryValidationCondition, &value);
		if (0 == ret)
		{
			bEntryValidationCondition = value;
			PRINT_GREEN_DEC(" Entry Validation Condition defined",
				bEntryValidationCondition);
		}

		printf("\n");
	}

	if (mct_readerdirect_ioctl)
	{
		char secoder_info[] = { 0x20, 0x70, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };

		rv = SCardControl(hCard, mct_readerdirect_ioctl, secoder_info,
			sizeof(secoder_info), bRecvBuffer, sizeof(bRecvBuffer), &length);
		PCSC_ERROR_CONT(rv, "SCardControl(MCT_READER_DIRECT)")

		printf("MCT_READER_DIRECT (%"LF"d): ", length);
		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf("\n");
	}

	if (pin_properties_ioctl)
	{
		PIN_PROPERTIES_STRUCTURE *pin_properties;

		rv = SCardControl(hCard, pin_properties_ioctl, NULL, 0,
			bRecvBuffer, sizeof(bRecvBuffer), &length);
		PCSC_ERROR_CONT(rv, "SCardControl(pin_properties_ioctl)")

		printf("PIN PROPERTIES (" GREEN "%"LF"d" NORMAL "): " GREEN, length);
		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf(NORMAL "\n");

		pin_properties = (PIN_PROPERTIES_STRUCTURE *)bRecvBuffer;
		bEntryValidationCondition = pin_properties ->	bEntryValidationCondition;
		PRINT_GREEN_HEX4(" wLcdLayout", pin_properties -> wLcdLayout);
		PRINT_GREEN_DEC(" bEntryValidationCondition", bEntryValidationCondition);
		PRINT_GREEN_DEC(" bTimeOut2", pin_properties -> bTimeOut2);

		printf("\n");
	}

#ifdef GET_GEMPC_FIRMWARE
	if (ccid_esc_command)
	{
		/* get GemPC firmware */
		printf("Get GemPC Firmware\n");

		/* this is specific to Gemalto readers */
		bSendBuffer[0] = 0x02;
		rv = SCardControl(hCard, ccid_esc_command, bSendBuffer,
			1, bRecvBuffer, sizeof(bRecvBuffer), &length);

		printf(" Firmware: " GREEN);
		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf(NORMAL "\n");

		bRecvBuffer[length] = '\0';
		printf(" Firmware: " GREEN "%s" NORMAL" (length " GREEN "%ld" NORMAL " bytes)\n", bRecvBuffer, length);

		PCSC_ERROR_CONT(rv, "SCardControl")
	}
#endif

	if (0 == verify_ioctl)
	{
		printf("Reader %s does not support PIN verification\n",
			readers[reader_nb]);
		goto end;
	}

	/* get card status */
	dwAtrLen = sizeof(pbAtr);
	dwReaderLen = sizeof(pbReader);
	rv = SCardStatus(hCard, pbReader, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);
	printf(" Reader: %s (length %"LF"d bytes)\n", pbReader, dwReaderLen);
	printf(" State: 0x%04"LF"X\n", dwState);
	printf(" Prot: %"LF"d\n", dwProt);
	printf(" ATR (length %"LF"d bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR_CONT(rv, "SCardStatus")

	if (dwState & SCARD_ABSENT)
	{
		printf("No card inserted\n");
		goto end;
	}

	/* re-connect to a reader (with a card) */
	dwActiveProtocol = -1;
	rv = SCardReconnect(hCard, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD,
		&dwActiveProtocol);
	printf(" Protocol: %"LF"d\n", dwActiveProtocol);
	PCSC_ERROR_EXIT(rv, "SCardReconnect")

	switch(dwActiveProtocol)
	{
		case SCARD_PROTOCOL_T0:
			pioSendPci = *SCARD_PCI_T0;
			break;
		case SCARD_PROTOCOL_T1:
			pioSendPci = *SCARD_PCI_T1;
			break;
		default:
			printf("Unknown protocol. No card present?\n");
			return -1;
	}

	/* APDU select applet */
	printf("Select applet: ");
	send_length = 11;
	memcpy(bSendBuffer, "\x00\xA4\x04\x00\x06\xA0\x00\x00\x00\x18\xFF",
		send_length);
	for (i=0; i<send_length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, bSendBuffer, send_length,
		&pioRecvPci, bRecvBuffer, &length);
	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_EXIT(rv, "SCardTransmit")
	if ((bRecvBuffer[0] != 0x90) || (bRecvBuffer[1] != 0x00))
	{
		printf("Error: test applet not found!\n");
		goto end;
	}

#ifdef VERIFY_PIN
	/* verify PIN */
	printf(" Secure verify PIN\n");
	pin_verify = (PIN_VERIFY_STRUCTURE *)bSendBuffer;

	/* PC/SC v2.02.05 Part 10 PIN verification data structure */
	pin_verify -> bTimerOut = 0x00;
	pin_verify -> bTimerOut2 = 0x00;
	pin_verify -> bmFormatString = 0x82;
	pin_verify -> bmPINBlockString = 0x08;
	pin_verify -> bmPINLengthFormat = 0x00;
	pin_verify -> wPINMaxExtraDigit = (PIN_min_size << 8) + PIN_max_size;
	pin_verify -> bEntryValidationCondition = bEntryValidationCondition;
	pin_verify -> bNumberMessage = 0x01;
	pin_verify -> wLangId = 0x0904;
	pin_verify -> bMsgIndex = 0x00;
	pin_verify -> bTeoPrologue[0] = 0x00;
	pin_verify -> bTeoPrologue[1] = 0x00;
	pin_verify -> bTeoPrologue[2] = 0x00;
	/* pin_verify -> ulDataLength = 0x00; we don't know the size yet */

	/* APDU: 00 20 00 00 08 30 30 30 30 00 00 00 00 */
	offset = 0;
	pin_verify -> abData[offset++] = 0x00;	/* CLA */
	pin_verify -> abData[offset++] = 0x20;	/* INS: VERIFY */
	pin_verify -> abData[offset++] = 0x00;	/* P1 */
	pin_verify -> abData[offset++] = 0x00;	/* P2 */
	pin_verify -> abData[offset++] = 0x08;	/* Lc: 8 data bytes */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> ulDataLength = offset;	/* APDU size */

	length = sizeof(PIN_VERIFY_STRUCTURE) + offset;

	printf(" command:");
	for (i=0; i<length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	printf("Enter your PIN: ");
	fflush(stdout);
	memset(bRecvBuffer, 0xAA, sizeof bRecvBuffer);
	rv = SCardControl(hCard, verify_ioctl, bSendBuffer,
		length, bRecvBuffer, sizeof(bRecvBuffer), &length);

	{
#ifndef S_SPLINT_S
		fd_set fd;
#endif
		struct timeval timeout;

		FD_ZERO(&fd);
		FD_SET(STDIN_FILENO, &fd);	/* stdin */
		timeout.tv_sec = 0;			/* timeout = 0.1s */
		timeout.tv_usec = 100000;

		/* we only try to read stdin if the pinpad is on a keyboard
		 * we do not read stdin for a SPR 532 for example */
		if (select(1, &fd, NULL, NULL, &timeout) > 0)
		{
			/* read the fake digits */
			char in[40];	/* 4 digits + \n + \0 */
			char *s = fgets(in, sizeof(in), stdin);

			if (s)
				printf("keyboard sent: %s", in);
		}
		else
			/* if it is not a keyboard */
			printf("\n");
	}

	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf(": %s\n", pinpad_return_codes(bRecvBuffer));
	PCSC_ERROR_CONT(rv, "SCardControl")

	/* verify PIN dump */
	printf("\nverify PIN dump: ");
	send_length = 5;
	memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
		send_length);
	for (i=0; i<send_length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, bSendBuffer, send_length,
		&pioRecvPci, bRecvBuffer, &length);
	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_EXIT(rv, "SCardTransmit")

	if ((2 == length) && (0x6C == bRecvBuffer[0]))
	{
		printf("\nverify PIN dump: ");
		send_length = 5;
		memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
			send_length);
		bSendBuffer[4] = bRecvBuffer[1];
		for (i=0; i<send_length; i++)
			printf(" %02X", bSendBuffer[i]);
		printf("\n");
		length = sizeof(bRecvBuffer);
		rv = SCardTransmit(hCard, &pioSendPci, bSendBuffer, send_length,
			&pioRecvPci, bRecvBuffer, &length);
		printf(" card response:");
		for (i=0; i<length; i++)
			printf(" %02X", bRecvBuffer[i]);
		printf("\n");
		PCSC_ERROR_EXIT(rv, "SCardTransmit")
	}
#endif

	/* check if the reader supports Modify PIN */
	if (0 == modify_ioctl)
	{
		printf("Reader %s does not support PIN modification\n",
			readers[reader_nb]);
		goto end;
	}

#ifdef MODIFY_PIN
	/* Modify PIN */
	printf(" Secure modify PIN\n");
	pin_modify = (PIN_MODIFY_STRUCTURE *)bSendBuffer;

	/* Table for bConfirmPIN and bNumberMessage
	 * bConfirmPIN = 3, bNumberMessage = 3: "Enter Pin" "New Pin" "Confirm Pin"
	 * bConfirmPIN = 2, bNumberMessage = 2: "Enter Pin" "New Pin"
	 * bConfirmPIN = 1, bNumberMessage = 2: "New Pin" "Confirm Pin"
	 * bConfirmPIN = 0, bNumberMessage = 1: "New Pin"
	 */
	/* table for bMsgIndex[1-3]
	 * 00: PIN insertion prompt        “ENTER SMARTCARD PIN”
	 * 01: PIN Modification prompt     “ ENTER NEW PIN”
	 * 02: NEW PIN Confirmation prompt “ CONFIRM NEW PIN”
	 */
	/* PC/SC v2.02.05 Part 10 PIN modification data structure */
	pin_modify -> bTimerOut = 0x00;
	pin_modify -> bTimerOut2 = 0x00;
	pin_modify -> bmFormatString = 0x82;
	pin_modify -> bmPINBlockString = 0x04;
	pin_modify -> bmPINLengthFormat = 0x00;
	pin_modify -> bInsertionOffsetOld = 0x00;	/* offset from APDU start */
	pin_modify -> bInsertionOffsetNew = 0x04;	/* offset from APDU start */
	pin_modify -> wPINMaxExtraDigit = (PIN_min_size << 8) + PIN_max_size;
	pin_modify -> bConfirmPIN = 0x03;	/* b0 set = confirmation requested */
									/* b1 set = current PIN entry requested */
	pin_modify -> bEntryValidationCondition = bEntryValidationCondition;
	pin_modify -> bNumberMessage = 0x03; /* see table above */
	pin_modify -> wLangId = 0x0904;
	pin_modify -> bMsgIndex1 = 0x00;
	pin_modify -> bMsgIndex2 = 0x01;
	pin_modify -> bMsgIndex3 = 0x02;
	pin_modify -> bTeoPrologue[0] = 0x00;
	pin_modify -> bTeoPrologue[1] = 0x00;
	pin_modify -> bTeoPrologue[2] = 0x00;
	/* pin_modify -> ulDataLength = 0x00; we don't know the size yet */

	/* APDU: 00 20 00 00 08 30 30 30 30 00 00 00 00 */
	offset = 0;
	pin_modify -> abData[offset++] = 0x00;	/* CLA */
	pin_modify -> abData[offset++] = 0x24;	/* INS: CHANGE/UNBLOCK */
	pin_modify -> abData[offset++] = 0x00;	/* P1 */
	pin_modify -> abData[offset++] = 0x00;	/* P2 */
	pin_modify -> abData[offset++] = 0x08;	/* Lc: 2x8 data bytes */
	pin_modify -> abData[offset++] = 0x30;	/* '0' old PIN */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' new PIN */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> ulDataLength = offset;	/* APDU size */

	length = sizeof(PIN_MODIFY_STRUCTURE) + offset;

	printf(" command:");
	for (i=0; i<length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	printf("Enter your PIN: ");
	fflush(stdout);
	memset(bRecvBuffer, 0xAA, sizeof bRecvBuffer);
	rv = SCardControl(hCard, modify_ioctl, bSendBuffer,
		length, bRecvBuffer, sizeof(bRecvBuffer), &length);

	{
#ifndef S_SPLINT_S
		fd_set fd;
#endif
		struct timeval timeout;

		/* old PIN, new PIN, confirmation PIN */
		/* if the command is aborted we will not read every "PIN" */
		for (i=0; i<3; i++)
		{
			FD_ZERO(&fd);
			FD_SET(STDIN_FILENO, &fd);	/* stdin */
			timeout.tv_sec = 0;			/* timeout = 0.1s */
			timeout.tv_usec = 100000;

			/* we only try to read stdin if the pinpad is on a keyboard
			 * we do not read stdin for a SPR 532 for example */
			if (select(1, &fd, NULL, NULL, &timeout) > 0)
			{
				/* read the fake digits */
				char in[40];	/* 4 digits + \n + \0 */
				char *ret;

				ret = fgets(in, sizeof(in), stdin);
				if (ret)
					printf("keyboard sent: %s", in);
			}
			else
			{
				/* if it is not a keyboard */
				printf("\n");

				/* exit the for() loop */
				break;
			}
		}
	}

	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_CONT(rv, "SCardControl")

	/* modify PIN dump */
	printf("\nmodify PIN dump: ");
	send_length = 5;
	memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
		send_length);
	for (i=0; i<send_length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, bSendBuffer, send_length,
		&pioRecvPci, bRecvBuffer, &length);
	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_EXIT(rv, "SCardTransmit")

	if ((2 == length) && (0x6C == bRecvBuffer[0]))
	{
		printf("\nverify PIN dump: ");
		send_length = 5;
		memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
			send_length);
		bSendBuffer[4] = bRecvBuffer[1];
		for (i=0; i<send_length; i++)
			printf(" %02X", bSendBuffer[i]);
		printf("\n");
		length = sizeof(bRecvBuffer);
		rv = SCardTransmit(hCard, &pioSendPci, bSendBuffer, send_length,
			&pioRecvPci, bRecvBuffer, &length);
		printf(" card response:");
		for (i=0; i<length; i++)
			printf(" %02X", bRecvBuffer[i]);
		printf("\n");
		PCSC_ERROR_EXIT(rv, "SCardTransmit")
	}
#endif

	/* card disconnect */
	rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
	PCSC_ERROR_CONT(rv, "SCardDisconnect")

end:
	/* We try to leave things as clean as possible */
	rv = SCardReleaseContext(hContext);
	if (rv != SCARD_S_SUCCESS)
		printf("SCardReleaseContext: %s (0x%"LF"X)\n", pcsc_stringify_error(rv),
			rv);

	/* free allocated memory */
	if (mszReaders)
		free(mszReaders);
	if (readers)
		free(readers);

	return 0;
} /* main */

