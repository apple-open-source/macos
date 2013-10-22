/*
 * Copyright (c) 2000-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "symCipher.h"

#include <string.h>

const SSLSymmetricCipherParams SSLCipherNullParams = {
    .keyAlg = SSL_CipherAlgorithmNull,
    .ivSize = 0,
    .blockSize = 0,
    .keySize = 0,
    .cipherType = streamCipherType,
};

static int NullInit(
    const SSLSymmetricCipherParams *cipher,
    int encrypting,
	uint8_t *key,
	uint8_t *iv,
	SymCipherContext *cipherCtx)
{
	return 0;
}

static int NullCrypt(
	const uint8_t *src, 
	uint8_t *dest,
    size_t len,
	SymCipherContext cipherCt)
{   
	if (src != dest)
        memcpy(dest, src, len);
    return 0;
}

static int NullFinish(
	SymCipherContext cipherCtx)
{   
	return 0;
}

const SSLSymmetricCipher SSLCipherNull = {
    .params = &SSLCipherNullParams,
    .c.cipher = {
        .initialize = NullInit,
        .encrypt = NullCrypt,
        .decrypt = NullCrypt
    },
    .finish = NullFinish
};
