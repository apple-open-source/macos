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


/*************************************************************************
  File: ssl2CipherMap.c    Maps SSL 2 cipher kinds to SSL 3 cipher suites
 *************************************************************************/

#include "sslContext.h"
#include "cryptType.h"

const SSLCipherMapping SSL2CipherMap[] =
{   {   SSL2_RC4_128_WITH_MD5, SSL_RSA_WITH_RC4_128_MD5 },
    {   SSL2_RC4_128_EXPORT_40_WITH_MD5, SSL_RSA_EXPORT_WITH_RC4_40_MD5 },
    {   SSL2_RC2_128_CBC_WITH_MD5, SSL_RSA_WITH_RC2_CBC_MD5 },
    {   SSL2_RC2_128_CBC_EXPORT40_WITH_MD5, SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5 },
    {   SSL2_IDEA_128_CBC_WITH_MD5, SSL_RSA_WITH_IDEA_CBC_MD5 },
    {   SSL2_DES_64_CBC_WITH_MD5, SSL_RSA_WITH_DES_CBC_MD5 },
    {   SSL2_DES_192_EDE3_CBC_WITH_MD5, SSL_RSA_WITH_3DES_EDE_CBC_MD5}
};

const unsigned SSL2CipherMapCount = sizeof(SSL2CipherMap) / sizeof(SSLCipherMapping);
