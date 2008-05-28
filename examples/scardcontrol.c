/*
    scardcontrol.c: sample code to use/test SCardControl() API
    Copyright (C) 2004-2008   Ludovic Rousseau

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

/*
 * $Id$
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

#undef VERIFY_PIN
#define MODIFY_PIN

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE     SCARD_CTL_CODE(1)

/* PCSC error message pretty print */
#define PCSC_ERROR_EXIT(rv, text) \
if (rv != SCARD_S_SUCCESS) \
{ \
	printf(text ": %s (0x%lX)\n", pcsc_stringify_error(rv), rv); \
	goto end; \
} \
else \
	printf(text ": OK\n\n");

#define PCSC_ERROR_CONT(rv, text) \
if (rv != SCARD_S_SUCCESS) \
	printf(text ": %s (0x%lX)\n", pcsc_stringify_error(rv), rv); \
else \
	printf(text ": OK\n\n");

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

	printf("SCardControl sample code\n");
	printf("V 1.3 2004-2008, Ludovic Rousseau <ludovic.rousseau@free.fr>\n");

	printf("\nTHIS PROGRAM IS NOT DESIGNED AS A TESTING TOOL!\n");
	printf("Do NOT use it unless you really know what you do.\n\n");

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext: Cannot Connect to Resource Manager %lX\n", rv);
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
		printf("SCardListReader: %lX\n", rv);

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
	while (*ptr != '\0')
	{
		printf("%d: %s\n", nbReaders, ptr);
		readers[nbReaders] = ptr;
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

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
	rv = SCardConnect(hContext, readers[reader_nb], SCARD_SHARE_DIRECT,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
	PCSC_ERROR_EXIT(rv, "SCardConnect")

	/* get GemPC firmware */
	printf(" Get GemPC Firmware\n");

	/* this is specific to Gemplus readers */
	bSendBuffer[0] = 0x02;
	rv = SCardControl(hCard, IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE, bSendBuffer,
		1, bRecvBuffer, sizeof(bRecvBuffer), &length);

	printf(" Firmware: ");
	for (i=0; i<length; i++)
		printf("%02X ", bRecvBuffer[i]);
	printf("\n");

	bRecvBuffer[length] = '\0';
	printf(" Firmware: %s (length %ld bytes)\n", bRecvBuffer, length);

	PCSC_ERROR_CONT(rv, "SCardControl")

	/* get card status */
	dwAtrLen = sizeof(pbAtr);
	dwReaderLen = sizeof(pbReader);
	rv = SCardStatus(hCard, pbReader, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);
	printf(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
	printf(" State: 0x%04lX\n", dwState);
	printf(" Prot: %ld\n", dwProt);
	printf(" ATR (length %ld bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR_CONT(rv, "SCardStatus")

	if (dwState & SCARD_ABSENT)
	{
		printf("No card inserted\n");
		goto end;
	}

	/* does the reader support PIN verification? */
	rv = SCardControl(hCard, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0,
		bRecvBuffer, sizeof(bRecvBuffer), &length);

	printf(" TLV (%ld): ", length);
	for (i=0; i<length; i++)
		printf("%02X ", bRecvBuffer[i]);
	printf("\n");

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
		if (pcsc_tlv[i].tag == FEATURE_VERIFY_PIN_DIRECT)
			verify_ioctl = ntohl(pcsc_tlv[i].value);
		if (pcsc_tlv[i].tag == FEATURE_MODIFY_PIN_DIRECT)
			modify_ioctl = ntohl(pcsc_tlv[i].value);
	}

	if (0 == verify_ioctl)
	{
		printf("Reader %s does not support PIN verification\n",
			readers[reader_nb]);
		goto end;
	}

	/* connect to a reader (even without a card) */
	dwActiveProtocol = -1;
	rv = SCardReconnect(hCard, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD,
		&dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
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

	/* PC/SC v2.0.2 Part 10 PIN verification data structure */
	pin_verify -> bTimerOut = 0x00;
	pin_verify -> bTimerOut2 = 0x00;
	pin_verify -> bmFormatString = 0x82;
	pin_verify -> bmPINBlockString = 0x04;
	pin_verify -> bmPINLengthFormat = 0x00;
	pin_verify -> wPINMaxExtraDigit = HOST_TO_CCID_16(0x0408); /* Min Max */
	pin_verify -> bEntryValidationCondition = 0x02;	/* validation key pressed */
	pin_verify -> bNumberMessage = 0x01;
	pin_verify -> wLangId = HOST_TO_CCID_16(0x0904);
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
	pin_verify -> ulDataLength = HOST_TO_CCID_32(offset);	/* APDU size */

	length = sizeof(PIN_VERIFY_STRUCTURE) + offset -1;	/* -1 because PIN_VERIFY_STRUCTURE contains the first byte of abData[] */

	printf(" command:");
	for (i=0; i<length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	printf("Enter your PIN: ");
	fflush(stdout);
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
			(void)fgets(in, sizeof(in), stdin);

			printf("keyboard sent: %s", in);
		}
		else
			/* if it is not a keyboard */
			printf("\n");
	}

	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
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
	/* PC/SC v2.0.2 Part 10 PIN verification data structure */
	pin_modify -> bTimerOut = 0x00;
	pin_modify -> bTimerOut2 = 0x00;
	pin_modify -> bmFormatString = 0x82;
	pin_modify -> bmPINBlockString = 0x04;
	pin_modify -> bmPINLengthFormat = 0x00;
	pin_modify -> bInsertionOffsetOld = 0x00; 	/* offset from APDU start */
	pin_modify -> bInsertionOffsetNew = 0x04;	/* offset from APDU start */
	pin_modify -> wPINMaxExtraDigit = HOST_TO_CCID_16(0x0408);	/* Min Max */
	pin_modify -> bConfirmPIN = 0x03;	/* b0 set = confirmation requested */
									/* b1 set = current PIN entry requested */
	pin_modify -> bEntryValidationCondition = 0x02;	/* validation key pressed */
	pin_modify -> bNumberMessage = 0x03; /* see table above */
	pin_modify -> wLangId = HOST_TO_CCID_16(0x0904);
	pin_modify -> bMsgIndex1 = 0x00;
	pin_modify -> bMsgIndex2 = 0x00;
	pin_modify -> bMsgIndex3 = 0x00;
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
	pin_modify -> ulDataLength = HOST_TO_CCID_32(offset);	/* APDU size */

	length = sizeof(PIN_MODIFY_STRUCTURE) + offset -1;	/* -1 because PIN_MODIFY_STRUCTURE contains the first byte of abData[] */

	printf(" command:");
	for (i=0; i<length; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	printf("Enter your PIN: ");
	fflush(stdout);
	rv = SCardControl(hCard, modify_ioctl, bSendBuffer,
		length, bRecvBuffer, sizeof(bRecvBuffer), &length);

	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_CONT(rv, "SCardControl")

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

				(void)fgets(in, sizeof(in), stdin);
				printf("keyboard sent: %s", in);
			}
		}
	}

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
		printf("SCardReleaseContext: %s (0x%lX)\n", pcsc_stringify_error(rv),
			rv);

	/* free allocated memory */
	if (mszReaders)
		free(mszReaders);
	if (readers)
		free(readers);

	return 0;
} /* main */

