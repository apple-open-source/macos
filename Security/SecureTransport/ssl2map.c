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
    File: ssl2map.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssl2map.c    Maps SSL 2 cipher kinds to SSL 3 cipher suites

    We use the SSL 3 CipherSuites to look up ciphers, hashes, etc; thus,
    this table maps two-byte SSL 3 CipherSuite values to three-byte SSL 2
    CipherKind values.

    ****************************************************************** */

#ifndef	_SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _CRYPTTYPE_H_
#include "cryptType.h"
#endif

const SSLCipherMapping SSL2CipherMap[] =
{   {   SSL2_RC4_128_WITH_MD5, SSL_RSA_WITH_RC4_128_MD5 },
    {   SSL2_RC4_128_EXPORT_40_WITH_MD5, SSL_RSA_EXPORT_WITH_RC4_40_MD5 },
    {   SSL2_RC2_128_CBC_WITH_MD5, SSL_RSA_WITH_RC2_CBC_MD5 },
    {   SSL2_RC2_128_CBC_EXPORT40_WITH_MD5, SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5 },
    {   SSL2_IDEA_128_CBC_WITH_MD5, SSL_RSA_WITH_IDEA_CBC_MD5 },
    {   SSL2_DES_64_CBC_WITH_MD5, SSL_RSA_WITH_DES_CBC_MD5 },
    {   SSL2_DES_192_EDE3_CBC_WITH_MD5, SSL_RSA_WITH_3DES_EDE_CBC_MD5}
};

const int SSL2CipherMapCount = sizeof(SSL2CipherMap) / sizeof(SSLCipherMapping);
