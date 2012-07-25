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
 * symCipher.h - CDSA-based symmetric cipher module
 */

#ifndef	_SYM_CIPHER_H_
#define _SYM_CIPHER_H_

#include "sslContext.h"
#include "cryptType.h"


#ifdef __cplusplus
extern "C" {
#endif

/*
 * CommonCrypto-based symmetric cipher callouts
 */
OSStatus CCSymmInit(
	uint8_t *key,
	uint8_t* iv,
	CipherContext *cipherCtx,
	SSLContext *ctx);
OSStatus CCSymmEncryptDecrypt(
	const uint8_t *src,
	uint8_t *dest,
	size_t len,
	CipherContext *cipherCtx,
	SSLContext *ctx);
OSStatus CCSymmFinish(
	CipherContext *cipherCtx,
	SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif	/* _SYM_CIPHER_H_ */
