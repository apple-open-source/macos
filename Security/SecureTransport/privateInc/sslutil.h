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
    File: sslutil.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslutil.h    Utility functions

    These functions get used in message decoding all over the place.

    ****************************************************************** */

#ifndef _SSLUTIL_H_
#define _SSLUTIL_H_ 1

#ifndef _SECURE_TRANSPORT_H_
#include "SecureTransport.h"
#endif

#ifndef	_SSL_PRIV_H_
#include "sslPriv.h"
#endif

UInt32  SSLDecodeInt(const unsigned char *p, int length);
unsigned char *SSLEncodeInt(unsigned char *p, UInt32 value, int length);
void    IncrementUInt64(sslUint64 *v);

UInt32 SSLGetCertificateChainLength(const SSLCertificate *c);
Boolean sslIsSessionActive(const SSLContext *ctx);
OSStatus sslDeleteCertificateChain(SSLCertificate *certs, SSLContext *ctx);

#if	SSL_DEBUG
extern const char *protocolVersStr(SSLProtocolVersion prot);
#endif

#define SET_SSL_BUFFER(buf, d, l)   do { (buf).data = (d); (buf).length = (l); } while (0)

#endif
