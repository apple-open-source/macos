/*
 *  vmdh.c
 *  Security
 *
 *  Created by Michael Brouwer on 11/7/06.
 *  Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
 *
 */

/*!
	@header vmdh
	The functions provided in vmdh.h implement the crypto exchange required
    for a Diffie-Hellman voicemail exchange.
*/


#include "vmdh.h"
#include <CommonCrypto/CommonCryptor.h>
#include <utilities/debugging.h>
#include <string.h>
#include <Security/SecInternal.h>
#include <Security/SecDH.h>

vmdh_t vmdh_create(uint32_t g, const uint8_t *p, size_t p_len,
    const uint8_t *recip, size_t recip_len) {
	SecDHContext dh;
	if (SecDHCreate(g, p, p_len, 0/*l*/, recip, recip_len, &dh))
		return NULL;
	return (vmdh_t)dh;
}

bool vmdh_generate_key(vmdh_t vmdh, uint8_t *pub_key, size_t *pub_key_len) {
	return !SecDHGenerateKeypair((SecDHContext)vmdh, pub_key, pub_key_len);
}

bool vmdh_encrypt_password(vmdh_t vmdh,
	const uint8_t *pub_key, size_t pub_key_len,
    const uint8_t *pw, size_t pw_len, uint8_t *encpw, size_t *encpw_len) {
	uint8_t aes_key[kCCKeySizeAES128];
	size_t aes_key_len = kCCKeySizeAES128;

	if (SecDHComputeKey((SecDHContext)vmdh, pub_key, pub_key_len,
		aes_key, &aes_key_len)) {
		return false;
	}

    /* Use the first 16 bytes in aes_key as an AES key. */
	if (CCCrypt(kCCEncrypt, kCCAlgorithmAES128,
		kCCOptionPKCS7Padding, aes_key, kCCKeySizeAES128, NULL,
		pw, pw_len, encpw, *encpw_len, encpw_len)) {
		return false;
	}

	/* Zero out key material. */
	bzero(aes_key, kCCKeySizeAES128);

    return true;
}

void vmdh_destroy(vmdh_t vmdh) {
    return SecDHDestroy((SecDHContext)vmdh);
}
