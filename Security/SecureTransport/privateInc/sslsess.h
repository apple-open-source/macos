/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*  *********************************************************************
    File: sslsess.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslsess.h    SSL Session Interface

    Prototypes for the SSL session interface functions in sslsess.c.

    ****************************************************************** */

#ifndef _SSLSESS_H_
#define _SSLSESS_H_ 1

#define SSL_SESSION_ID_LEN  16      /* 16 <= SSL_SESSION_ID_LEN <= 32 */

SSLErr SSLAddSessionID(const SSLContext *ctx);
SSLErr SSLGetSessionID(SSLBuffer *sessionData, const SSLContext *ctx);
SSLErr SSLDeleteSessionID(const SSLContext *ctx);
SSLErr SSLRetrieveSessionIDIdentifier(
	const SSLBuffer sessionData, 
	SSLBuffer *identifier, 
	const SSLContext *ctx);
SSLErr SSLRetrieveSessionIDProtocolVersion(
	const SSLBuffer sessionID, 
	SSLProtocolVersion *version, 
	const SSLContext *ctx);
SSLErr SSLInstallSessionID(const SSLBuffer sessionData, SSLContext *ctx);

#endif /* _SSLSESS_H_ */
