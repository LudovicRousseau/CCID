/*
    scardcontrol.c: sample code to use/test SCardControl() API
    Copyright (C) 2004   Ludovic Rousseau

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
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <PCSC/winscard.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define SCARD_CTL_CODE(code) (0x42000000 + (code))

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE     SCARD_CTL_CODE(1)
#define IOCTL_SMARTCARD_VENDOR_VERIFY_PIN       SCARD_CTL_CODE(2)
#define IOCTL_SMARTCARD_VENDOR_MODIFY_PIN       SCARD_CTL_CODE(3)
#define IOCTL_SMARTCARD_VENDOR_TRANSFER_PIN     SCARD_CTL_CODE(4)

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
	LPTSTR mszReaders;
	char *ptr, **readers = NULL;
	int nbReaders;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwReaderLen, dwState, dwProt, dwAtrLen;
	BYTE pbAtr[MAX_ATR_SIZE] = "";
	char pbReader[MAX_READERNAME] = "";
	int reader_nb;
	int i, offset;
	unsigned char bSendBuffer[MAX_BUFFER_SIZE];
	unsigned char bRecvBuffer[MAX_BUFFER_SIZE];
	DWORD length;
	unsigned char attribute[1];
	DWORD attribute_length;
	SCARD_IO_REQUEST pioRecvPci;

	printf("SCardControl sample code\n");
	printf("V 1.0 2004, Ludovic Rousseau <ludovic.rousseau@free.fr>\n");

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
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardListReader: %lX\n", rv);
	}

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
	printf(" State: 0x%lX\n", dwState);
	printf(" Prot: %ld\n", dwProt);
	printf(" ATR (length %ld bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR_CONT(rv, "SCardStatus")

	/* does the reader support PIN verification? */
	attribute_length = sizeof(attribute);
	rv = SCardGetAttrib(hCard, IOCTL_SMARTCARD_VENDOR_VERIFY_PIN, attribute,
		&attribute_length);
	PCSC_ERROR_CONT(rv, "SCardGetAttrib")
	if (FALSE == attribute[0])
	{
		printf("Reader %s does not support PIN verification\n",
			readers[reader_nb]);
		goto end;
	}

	/* connect to a reader (even without a card) */
	dwActiveProtocol = -1;
	rv = SCardReconnect(hCard, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0, SCARD_UNPOWER_CARD, &dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
	PCSC_ERROR_EXIT(rv, "SCardReconnect")

	/* APDU select DF */
	memcpy(bSendBuffer, "\x00\xA4\x04\x00\x05\x47\x54\x4F\x4B\x31", 10);
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, SCARD_PCI_T0, bSendBuffer, 10,
		&pioRecvPci, bRecvBuffer, &length);
	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_EXIT(rv, "SCardTransmit")

	/* APDU select EF */
	memcpy(bSendBuffer, "\x00\xA4\x02\x00\x02\x00\x04", 7);
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, SCARD_PCI_T0, bSendBuffer, 7,
		&pioRecvPci, bRecvBuffer, &length);
	printf(" card response:");
	for (i=0; i<length; i++)
		printf(" %02X", bRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR_EXIT(rv, "SCardTransmit")

	/* verify PIN */
	printf(" Secure verify PIN\n");
	offset = 0;
	/* APDU: 00 20 00 00 08 30 30 30 30 00 00 00 00 */
	bSendBuffer[offset++] = 0x00;	/* CLA */
	bSendBuffer[offset++] = 0x20;	/* INS: VERIFY */
	bSendBuffer[offset++] = 0x00;	/* P1 */
	bSendBuffer[offset++] = 0x00;	/* P2 */
	bSendBuffer[offset++] = 0x08;	/* Lc: 8 data bytes */
	bSendBuffer[offset++] = 0x30;	/* '0' */
	bSendBuffer[offset++] = 0x30;	/* '0' */
	bSendBuffer[offset++] = 0x30;	/* '0' */
	bSendBuffer[offset++] = 0x30;	/* '0' */
	bSendBuffer[offset++] = 0x00;	/* '\0' */
	bSendBuffer[offset++] = 0x00;	/* '\0' */
	bSendBuffer[offset++] = 0x00;	/* '\0' */
	bSendBuffer[offset++] = 0x00;	/* '\0' */

	/* CCID PIN verification data structure */
	bSendBuffer[offset++] = 0x00;	/* bTimeOut */
	bSendBuffer[offset++] = 0x82;	/* bmFormatString */
	bSendBuffer[offset++] = 0x04;	/* bmPINBlockString (PIN length) */
	bSendBuffer[offset++] = 0x00;	/* bmPINLengthFormat */
	bSendBuffer[offset++] = 0x08;	/* wPINMaxExtraDigit: max */
	bSendBuffer[offset++] = 0x04;	/* wPINMaxExtraDigit: min */
	bSendBuffer[offset++] = 0x02;	/* bEntryValidationCondition */
	bSendBuffer[offset++] = 0x00;	/* bNumberMessage */
	bSendBuffer[offset++] = 0x04;	/* wLangId: english */
	bSendBuffer[offset++] = 0x09;	/* " */
	bSendBuffer[offset++] = 0x00;	/* bMsgIndex */
	bSendBuffer[offset++] = 0x00;	/* bTeoPrologue */
	bSendBuffer[offset++] = 0x00;	/* " */
	bSendBuffer[offset++] = 0x00;	/* " */

	printf(" command:");
	for (i=0; i<offset; i++)
		printf(" %02X", bSendBuffer[i]);
	printf("\n");
	printf("Enter your PIN: ");
	fflush(stdout);
	rv = SCardControl(hCard, IOCTL_SMARTCARD_VENDOR_VERIFY_PIN, bSendBuffer,
		offset, bRecvBuffer, sizeof(bRecvBuffer), &length);

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

		FD_ZERO(&fd);
		FD_SET(STDIN_FILENO, &fd);	/* stdin */
		timeout.tv_sec = 0;			/* timeout = 0 */
		timeout.tv_usec = 0;

		/* we only try to read stdin if the pinpad is on a keyboard
		 * we do not read stdin for a SPR 532 for example */
		if (select(1, &fd, NULL, NULL, &timeout) > 0)
		{
			/* read the fake digits */
			char in[10];	/* 4 digits + \n + \0 */
			(void)fgets(in, sizeof(in), stdin);

			printf("keyboard sent: %s", in);
		}
	}

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
	free(mszReaders);
	free(readers);

	return 0;
} /* main */

