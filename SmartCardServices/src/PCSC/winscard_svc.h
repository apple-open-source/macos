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
	    Title  : winscard_svc.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 03/30/01
	    License: Copyright (C) 2001 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This demarshalls functions over the message
	             queue and keeps track of clients and their
                     handles.

********************************************************************/

#ifndef __winscard_svc_h__
#define __winscard_svc_h__

#ifdef __cplusplus
extern "C"
{
#endif

	LONG MSGFunctionDemarshall(psharedSegmentMsg);
	LONG MSGAddContext(SCARDCONTEXT, DWORD);
	LONG MSGRemoveContext(SCARDCONTEXT, DWORD);
	LONG MSGAddHandle(SCARDCONTEXT, DWORD, SCARDHANDLE);
	LONG MSGRemoveHandle(SCARDCONTEXT, DWORD, SCARDHANDLE);
	LONG MSGCleanupClient(psharedSegmentMsg);

#ifdef __cplusplus
}
#endif

#endif
