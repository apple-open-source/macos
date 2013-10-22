/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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

#ifndef _OSSL_AES_H_
#define _OSSL_AES_H_

/* symbol renaming */
#define AES_set_encrypt_key	ossl_AES_set_encrypt_key
#define AES_set_decrypt_key	ossl_AES_set_decrypt_key
#define	AES_destroy_key		ossl_AES_destroy_key
#define AES_encrypt		ossl_AES_encrypt
#define AES_decrypt		ossl_AES_decrypt

#define AES_ENCRYPT		1
#define AES_DECRYPT		0

#define AES_BLOCK_SIZE		16

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
#include <CommonCrypto/CommonCryptor.h>

#define AES_KEYST_SIZE    (kCCContextSizeAES128 + 8)

struct aes_key_st {
	unsigned char	data[AES_KEYST_SIZE];
	CCCryptorRef	cref;
};
#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

typedef struct aes_key_st   AES_KEY;

int AES_set_encrypt_key(const unsigned char *userKey, const int bits,
    AES_KEY *key);
int AES_set_decrypt_key(const unsigned char *userKey, const int bits,
    AES_KEY *key);

void AES_destroy_ctx(AES_KEY *key);

void AES_encrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);
void AES_decrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);

#endif /* _OSSL_AES_H_ */
