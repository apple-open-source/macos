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
    File: nullciph.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: nullciph.c   A dummy implementation of the null cipher

    The null cipher is used for SSL_NULL_WITH_NULL_NULL,
    SSL_RSA_WITH_NULL_MD5, and SSL_RSA_WITH_NULL_SHA ciphers.

    ****************************************************************** */

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#include <string.h>

static SSLErr NullInit(
	uint8 *key, 
	uint8* iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
static SSLErr NullCrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx,
	SSLContext *ctx);
static SSLErr NullFinish(
	CipherContext *cipherCtx,
	SSLContext *ctx);

const SSLSymmetricCipher SSLCipherNull = {
    0,          /* Key size in bytes (ignoring parity) */
    0,          /* Secret key size */
    0,          /* IV size */
    0,          /* Block size */
    CSSM_ALGID_NONE,	
    CSSM_ALGID_NONE,	
    CSSM_ALGMODE_NONE,
	CSSM_PADDING_NONE,
    NullInit,
    NullCrypt,
    NullCrypt,
    NullFinish
};

static SSLErr NullInit(
	uint8 *key, 
	uint8* iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{  
 	return SSLNoErr;
}

static SSLErr NullCrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx,
	SSLContext *ctx)
{   
	if (src.data != dest.data)
        memcpy(dest.data, src.data, src.length);
    return SSLNoErr;
}

static SSLErr NullFinish(
	CipherContext *cipherCtx,
	SSLContext *ctx)
{   
	return SSLNoErr;
}
