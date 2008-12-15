/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */


/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : test.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This is a test program for pcsc-lite.
	            
********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "pcsclite.h"
#include "winscard.h"

/*
 * #define REPEAT_TEST 1 
 */

int main(int argc, char **argv)
{
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	SCARD_READERSTATE_A rgReaderStates[1];
	uint32_t dwReaderLen, dwState, dwProt, dwAtrLen;
	// unsigned long dwSendLength, dwRecvLength;
	uint32_t dwPref, dwReaders;
	char *pcReaders, *mszReaders;
	unsigned char pbAtr[MAX_ATR_SIZE];
	const char *mszGroups;
	long rv;
	int i, p, iReader;
	int iList[16];

	int t = 0;

	printf("\nMUSCLE PC/SC Lite Test Program\n\n");

doInit:
	printf("Testing SCardEstablishContext    : ");
	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		return -1;
	}

	printf("Testing SCardGetStatusChange \n");
	printf("Please insert a working reader   : ");
	rv = SCardGetStatusChange(hContext, INFINITE, 0, 0);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardListReaders         : ");

	mszGroups = 0;
	rv = SCardListReaders(hContext, mszGroups, 0, &dwReaders);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	mszReaders = (char *) malloc(sizeof(char) * dwReaders);
	rv = SCardListReaders(hContext, mszGroups, mszReaders, &dwReaders);

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	/*
	 * Have to understand the multi-string here 
	 */
	p = 0;
	for (i = 0; i < dwReaders - 1; i++)
	{
		++p;
		printf("Reader %02d: %s\n", p, &mszReaders[i]);
		iList[p] = i;
		while (mszReaders[++i] != 0) ;
	}

#ifdef REPEAT_TEST
	if (t == 0)
	{
#endif

		do
		{
			/* scanf doesn't provide a friendly way to 'throw away' the garbage input
			 * so we grab a line and then try to parse it */
			size_t iScanLength;
			char *sLine;
			printf("Enter the reader number          : ");
			sLine = fgetln(stdin, &iScanLength);
			if(sLine == NULL) /* EOF */
				return 0;
			/* Null terminate by replacing \n w/ \0*/
			sLine[iScanLength - 1] = '\0';
			iReader = atoi(sLine);
			/* Since 0 is invalid input, no need to test errno */
			if(iReader > p || iReader <= 0) {
				printf("Invalid Value - try again\n");
			}
		}
		while (iReader > p || iReader <= 0);

#ifdef REPEAT_TEST
		t = 1;
	}
#endif

	rgReaderStates[0].szReader = &mszReaders[iList[iReader]];
	rgReaderStates[0].dwCurrentState = SCARD_STATE_EMPTY;

	printf("Waiting for card insertion         \n");
	rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);

	printf("                                 : %s\n",
		pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

//	printf("   context handle: %d [0x%08X]\n", hContext, hContext);
	printf("Testing SCardConnect             : ");
	rv = SCardConnect(hContext, &mszReaders[iList[iReader]],
		SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&hCard, &dwPref);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardStatus              : ");

	dwReaderLen = MAX_READERNAME;
	pcReaders = (char *) malloc(sizeof(char) * MAX_READERNAME);
	dwAtrLen = MAX_ATR_SIZE;
	
	rv = SCardStatus(hCard, pcReaders, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);

	printf("%s\n", pcsc_stringify_error(rv));

	printf("Current Reader Name              : %s\n", pcReaders);
	printf("Current Reader State             : 0x%X\n", dwState);
	printf("Current Reader Protocol          : 0x%X\n", dwProt - 1);
	printf("Current Reader ATR Size          : %d (0x%x)\n", dwAtrLen, dwAtrLen);
	printf("Current Reader ATR Value         : ");

	for (i = 0; i < dwAtrLen; i++)
	{
		printf("%02X ", pbAtr[i]);
	}
	printf("\n");

	if (rv != SCARD_S_SUCCESS)
	{
		SCardDisconnect(hCard, SCARD_RESET_CARD);
		SCardReleaseContext(hContext);
	}

	printf("Testing SCardDisconnect          : ");
	rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardReleaseContext      : ");
	rv = SCardReleaseContext(hContext);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		return -1;
	}
	if(t == 0) {
		t = 1;
		goto doInit;
	}

	printf("\n");
	printf("PC/SC Test Completed Successfully !\n");

	return 0;
}
