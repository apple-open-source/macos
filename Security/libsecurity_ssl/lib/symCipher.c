/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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

/*
 * symCipher.c - CDSA-based symmetric cipher module
 */

#include "sslContext.h"
#include "cryptType.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include <CommonCrypto/CommonCryptor.h>
#include "symCipher.h"

/*
 * CommonCrypto-based symmetric cipher callouts
 */
OSStatus CCSymmInit(
	uint8_t *key,
	uint8_t* iv,
	CipherContext *cipherCtx,
	SSLContext *ctx)
{
	/*
	 * Cook up a CCCryptorRef. Assumes:
	 * 		cipherCtx->symCipher.keyAlg
	 *		cipherCtx->encrypting
	 * 		key (raw key bytes)
	 *		iv (raw bytes)
	 * On successful exit:
	 * 		Resulting CCCryptorRef --> cipherCtx->cryptorRef
	 */
	CCCryptorStatus ccrtn;
	CCOperation op = cipherCtx->encrypting ? kCCEncrypt : kCCDecrypt;

	if(cipherCtx->cryptorRef) {
		CCCryptorRelease(cipherCtx->cryptorRef);
		cipherCtx->cryptorRef = NULL;
	}

	ccrtn = CCCryptorCreate(op, cipherCtx->symCipher->keyAlg,
		0,		/* options - no padding, default CBC */
		key, cipherCtx->symCipher->keySize,
		iv,
		&cipherCtx->cryptorRef);
	if(ccrtn) {
		sslErrorLog("CCCryptorCreate returned %d\n", (int)ccrtn);
		return internalComponentErr;
	}
	return noErr;
}

/* same for en/decrypt */
OSStatus CCSymmEncryptDecrypt(
	const uint8_t *src,
	uint8_t *dest,
	size_t len,
	CipherContext *cipherCtx,
	SSLContext *ctx)
{
	CCCryptorStatus ccrtn;

	ASSERT(cipherCtx != NULL);
	ASSERT(cipherCtx->cryptorRef != NULL);
	if(cipherCtx->cryptorRef == NULL) {
		sslErrorLog("CCSymmEncryptDecrypt: NULL cryptorRef\n");
		return internalComponentErr;
	}
    size_t data_moved;
	ccrtn = CCCryptorUpdate(cipherCtx->cryptorRef, src, len,
		dest, len, &data_moved);
    assert(data_moved == len);
	#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmEncryptDecrypt: returned %d\n", (int)ccrtn);
		return internalComponentErr;
	}
	#endif
	return noErr;
}

OSStatus CCSymmFinish(
	CipherContext *cipherCtx,
	SSLContext *ctx)
{
	if(cipherCtx->cryptorRef) {
		CCCryptorRelease(cipherCtx->cryptorRef);
		cipherCtx->cryptorRef = NULL;
	}
	return noErr;
}

