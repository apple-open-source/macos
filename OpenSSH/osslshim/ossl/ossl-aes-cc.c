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

#include "ossl-config.h"

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CommonCrypto/CommonCryptor.h>

#include "ossl-aes.h"

/*
 * CommonCrypto shims for OSSL AES lowlevel API
 */
static int
_AES_set_key(int enc, const unsigned char *userKey, const int bits,
    AES_KEY *key)
{
	CCCryptorStatus status;
	size_t keysize = (size_t)(bits / 8);

	if (!userKey || !key) {
		return (-1);
	}

	if ((keysize < kCCKeySizeAES128) || (keysize > kCCKeySizeAES256)) {
		return (-2);
	}

	/*
	 * XXX The headerdoc for CCCryptorCreateFromData() claims to "Create
	 * a cryptographic context using caller-supplied memory" and that
	 * calling CCCryptorRelease() "is not strictly necessary".  Unfortunately,
	 * this is incorrect.  CCCryptorCreateFromData() allocates 4K of memory
	 * that will never be release unless CCCryptorRelease() is called.
	 * See <rdr://problem/10867937>.
	 */
	status = CCCryptorCreateFromData(enc, kCCAlgorithmAES128,
		kCCOptionECBMode, (const void *)userKey, keysize, NULL,
		key->data, AES_KEYST_SIZE, &key->cref, NULL);
	if (status != kCCSuccess) {
		return (-3);
	}

	return (0);
}

/*
 * XXX Unfortunately CCCryptorCreateFromData() still allocates memory even passing
 * it a buffer to use.  Therefore, we need to explicitly free that memory by calling
 * CCCryptorRelease().  Also see the comment above and <rdr://problem/10867937>.
 */
void
AES_destroy_ctx(AES_KEY *key)
{
	if (key && key->cref) {
		CCCryptorRelease(key->cref);
		key->cref = NULL;
	}
}

int
AES_set_encrypt_key(const unsigned char *userKey, const int bits,
    AES_KEY *key)
{
	return (_AES_set_key(kCCEncrypt, userKey, bits, key));
}

int
AES_set_decrypt_key(const unsigned char *userKey, const int bits,
    AES_KEY *key)
{
	return (_AES_set_key(kCCDecrypt, userKey, bits, key));
}


void
AES_encrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key)
{
	(void)CCCryptorUpdate(key->cref, (const void *)in, kCCBlockSizeAES128,
	    (void *)out, kCCBlockSizeAES128, NULL);
}


void
AES_decrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key)
{
	(void)CCCryptorUpdate(key->cref, (const void *)in, kCCBlockSizeAES128,
	    (void *)out, kCCBlockSizeAES128, NULL);
}

#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */
