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


#include "sslContext.h"

#include <string.h>

static OSStatus NullInit(
	uint8 *key, 
	uint8* iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
static OSStatus NullCrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx,
	SSLContext *ctx);
static OSStatus NullFinish(
	CipherContext *cipherCtx,
	SSLContext *ctx);

extern "C" {
extern const SSLSymmetricCipher SSLCipherNull;
}
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

static OSStatus NullInit(
	uint8 *key, 
	uint8* iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{  
 	return noErr;
}

static OSStatus NullCrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx,
	SSLContext *ctx)
{   
	if (src.data != dest.data)
        memcpy(dest.data, src.data, src.length);
    return noErr;
}

static OSStatus NullFinish(
	CipherContext *cipherCtx,
	SSLContext *ctx)
{   
	return noErr;
}
