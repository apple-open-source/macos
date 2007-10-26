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
            Title  : atrhandler.h
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This keeps track of smartcard protocols,
                     timing issues, and atr handling.
 
********************************************************************/

#ifndef __atrhandler_h__
#define __atrhandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

#define SCARD_CONVENTION_DIRECT  0x0001
#define SCARD_CONVENTION_INVERSE 0x0002

	typedef struct _SMARTCARD_EXTENSION
	{

		struct _ATR
		{
			DWORD Length;
			UCHAR Value[MAX_ATR_SIZE];
			DWORD HistoryLength;
			UCHAR HistoryValue[MAX_ATR_SIZE];
		}
		ATR;

		DWORD ReadTimeout;

		struct _CardCapabilities
		{
			UCHAR AvailableProtocols;
			UCHAR CurrentProtocol;
			UCHAR Convention;
			USHORT ETU;

			struct _PtsData
			{
				UCHAR F1;
				UCHAR D1;
				UCHAR I1;
				UCHAR P1;
				UCHAR N1;
			}
			PtsData;

			struct _T1
			{
				USHORT BGT;
				USHORT BWT;
				USHORT CWT;
				USHORT CGT;
				USHORT WT;
			}
			T1;

			struct _T0
			{
				USHORT BGT;
				USHORT BWT;
				USHORT CWT;
				USHORT CGT;
				USHORT WT;
			}
			T0;

		}
		CardCapabilities;

		/*
		 * PREADER_CONNECTION psReaderConnection; 
		 */

	}
	SMARTCARD_EXTENSION, *PSMARTCARD_EXTENSION;

	/*
	 * Decodes the ATR and fills the structure 
	 */

	short ATRDecodeAtr(PSMARTCARD_EXTENSION psExtension,
		const unsigned char *pucAtr, DWORD dwLength);

#ifdef __cplusplus
}
#endif

#endif							/* __smclib_h__ */
