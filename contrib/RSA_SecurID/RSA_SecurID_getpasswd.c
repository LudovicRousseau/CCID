/*
    RSA_SecurID_getpasswd.c: get the one-use password from a RSA sid-800 token
    Copyright (C) 2006   Ludovic Rousseau <ludovic.rousseau@free.fr>

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
#include <string.h>
#include <winscard.h>

/* PCSC error message pretty print */
#define PCSC_ERROR_EXIT(rv, text) \
if (rv != SCARD_S_SUCCESS) \
{ \
	printf(text ": %s (0x%lX)\n", pcsc_stringify_error(rv), rv); \
	goto end; \
}

int main(void)
{
	unsigned char cmd1[] = { 0x00, 0xa4, 0x04, 0x00, 0x0a, 0xa0, 0x00, 0x00, 0x00, 0x63, 0x86, 0x53, 0x49, 0x44, 0x01};
	unsigned char cmd2[] = { 0x80, 0x56, 0x00, 0x00, 0x04 };
	unsigned char cmd3[] = { 0x80, 0x48, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xff };
	unsigned char cmd4[] = { 0x80, 0x44, 0x00, 0x00, 0x05};
	LONG rv;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	LPSTR mszReaders = NULL;
	char **readers = NULL;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
	unsigned char bRecvBuffer[MAX_BUFFER_SIZE];
	DWORD length;
	SCARD_IO_REQUEST pioRecvPci;
 	SCARD_IO_REQUEST pioSendPci;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext: Cannot Connect to Resource Manager %lX\n", rv);
		return 1;
	}

	/* Retrieve the available readers list */
	rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
	PCSC_ERROR_EXIT(rv, "SCardListReader");

	if (dwReaders < 4)
	{
		printf("No reader found!\n");
		return -1;
	}

	mszReaders = malloc(sizeof(char)*dwReaders);
	if (mszReaders == NULL)
	{
		printf("malloc: not enough memory\n");
		goto end;
	}

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	PCSC_ERROR_EXIT(rv, "SCardListReader");

	/* connect to the first reader */
	dwActiveProtocol = -1;
	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_EXCLUSIVE,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	PCSC_ERROR_EXIT(rv, "SCardConnect")

    switch(dwActiveProtocol)
    {
        case SCARD_PROTOCOL_T0:
            pioSendPci = *SCARD_PCI_T0;
            break;
        case SCARD_PROTOCOL_T1:
            pioSendPci = *SCARD_PCI_T1;
            break;
        default:
            printf("Unknown protocol\n");
            return -1;
    }

	/* APDU select applet */
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, cmd1, sizeof cmd1,
		&pioRecvPci, bRecvBuffer, &length);
	PCSC_ERROR_EXIT(rv, "SCardTransmit")
	if ((length != 2) || (bRecvBuffer[0] != 0x90) || (bRecvBuffer[1] != 0x00))
	{
		printf("cmd1 failed: %02X%02X\n", bRecvBuffer[0], bRecvBuffer[1]);
		goto end;
	}

	/* non ISO APDU */
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, cmd2, sizeof cmd2,
		&pioRecvPci, bRecvBuffer, &length);
	PCSC_ERROR_EXIT(rv, "SCardTransmit")
	if ((length != 6) || (bRecvBuffer[4] != 0x90) || (bRecvBuffer[5] != 0x00))
	{
		printf("cmd2 failed (%ld) : %02X%02X\n", length, bRecvBuffer[4],
			bRecvBuffer[5]);
		goto end;
	}

	/* get the argument for cmd3 from result of cmd2 */
	memcpy(cmd3+5, bRecvBuffer, 4);

	/* non ISO APDU */
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, cmd3, sizeof cmd3,
		&pioRecvPci, bRecvBuffer, &length);
	PCSC_ERROR_EXIT(rv, "SCardTransmit")
	if ((length != 2) || (bRecvBuffer[0] != 0x90) || (bRecvBuffer[1] != 0x00))
	{
		printf("cmd2 failed (%ld) : %02X%02X\n", length, bRecvBuffer[0],
			bRecvBuffer[1]);
		goto end;
	}

	/* non iSO APDU */
	length = sizeof(bRecvBuffer);
	rv = SCardTransmit(hCard, &pioSendPci, cmd4, sizeof cmd4,
		&pioRecvPci, bRecvBuffer, &length);
	PCSC_ERROR_EXIT(rv, "SCardTransmit")
	if ((length != 7) || (bRecvBuffer[5] != 0x90) || (bRecvBuffer[6] != 0x00))
	{
		printf("cmd3 failed (%ld): %02X%02X\n", length, bRecvBuffer[5],
			bRecvBuffer[6]);
		goto end;
	}

	printf("%02X%02X%02X\n", bRecvBuffer[2], bRecvBuffer[3], bRecvBuffer[4]);

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

