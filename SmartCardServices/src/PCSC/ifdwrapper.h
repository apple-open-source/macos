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

            Title  : ifdwrapper.h
            Package: PC/SC Lite
            Author : David Corcoran
            Date   : 10/08/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This wraps the dynamic ifdhandler functions.
	             This abstraction will eventually allow multiple card
	             slots in the same terminal.

********************************************************************/

#ifndef __ifdwrapper_h__
#define __ifdwrapper_h__

#ifdef __cplusplus
extern "C"
{
#endif

	LONG IFDOpenIFD(PREADER_CONTEXT, DWORD);
	LONG IFDCloseIFD(PREADER_CONTEXT);
	LONG IFDPowerICC(PREADER_CONTEXT, DWORD, PUCHAR, PDWORD);
	LONG IFDStatusICC(PREADER_CONTEXT, PDWORD, PDWORD, PUCHAR, PDWORD);
	LONG IFDControl(PREADER_CONTEXT, PUCHAR, DWORD, PUCHAR, PDWORD);
	LONG IFDTransmit(PREADER_CONTEXT, SCARD_IO_HEADER,
		PUCHAR, DWORD, PUCHAR, PDWORD, PSCARD_IO_HEADER);
	LONG IFDSetPTS(PREADER_CONTEXT, DWORD, UCHAR, UCHAR, UCHAR, UCHAR);
	LONG IFDSetCapabilities(PREADER_CONTEXT, DWORD, DWORD, PUCHAR);
	LONG IFDGetCapabilities(PREADER_CONTEXT, DWORD, PDWORD, PUCHAR);

#ifdef __cplusplus
}
#endif

#endif							/* __ifdwrapper_h__ */
