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

            Title  : prothandler.h
            Package: PC/SC Lite
            Author : David Corcoran
            Date   : 10/06/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles protocol defaults, PTS, etc.

********************************************************************/

#ifndef __prothandler_h__
#define __prothandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

	UCHAR PHGetDefaultProtocol(PUCHAR, DWORD);
	UCHAR PHGetAvailableProtocols(PUCHAR, DWORD);
	UCHAR PHSetProtocol(struct ReaderContext *, DWORD, UCHAR);

#ifdef __cplusplus
}
#endif

#endif							/* __prothandler_h__ */
