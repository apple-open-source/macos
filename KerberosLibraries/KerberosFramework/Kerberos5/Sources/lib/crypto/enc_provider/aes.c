/*
 * lib/crypto/enc_provider/aes.h
 *
 * Copyright (C) 2003, 2007 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "enc_provider.h"

#include <CommonCrypto/CommonCryptor.h>

static void
aes_cts_encrypt(const unsigned char *in, unsigned char *out,
		size_t len, void *keydata, size_t keysize,
		unsigned char *ivec, const int encryptp)
{
    unsigned char tmp[kCCBlockSizeAES128], livec[kCCBlockSizeAES128];
    CCCryptorRef cryptorRef;
    size_t size, i;
    CCCryptorStatus ret;

    if (ivec == NULL) {
	ivec = livec;
	memset(livec, 0, sizeof(livec));
    }
    

    ret = CCCryptorCreate(encryptp ? kCCEncrypt : kCCDecrypt,
			  kCCAlgorithmAES128,
			  kCCOptionECBMode,
			  keydata,
			  keysize,
			  NULL,
			  &cryptorRef);
    if (ret)
	abort();

    if (len == kCCBlockSizeAES128) {
	ret = CCCryptorUpdate(cryptorRef, in, kCCBlockSizeAES128,
			      out, kCCBlockSizeAES128, &size);
	CCCryptorRelease(cryptorRef);
	if (ret)
	    abort();
	return;
    }

    if (encryptp) {

	while(len > kCCBlockSizeAES128) {
	    for (i = 0; i < kCCBlockSizeAES128; i++)
		tmp[i] = in[i] ^ ivec[i];
	    ret = CCCryptorUpdate(cryptorRef, tmp, kCCBlockSizeAES128,
				  out, kCCBlockSizeAES128, &size);
	    if (ret)
		abort();
	    memcpy(ivec, out, kCCBlockSizeAES128);
	    len -= kCCBlockSizeAES128;
	    in += kCCBlockSizeAES128;
	    out += kCCBlockSizeAES128;
	}

	for (i = 0; i < len; i++)
	    tmp[i] = in[i] ^ ivec[i];
	for (; i < kCCBlockSizeAES128; i++)
	    tmp[i] = 0 ^ ivec[i];

	ret = CCCryptorUpdate(cryptorRef, tmp, kCCBlockSizeAES128,
			      out - kCCBlockSizeAES128, kCCBlockSizeAES128,
			      &size);
	if (ret)
	    abort();

	memcpy(out, ivec, len);
	memcpy(ivec, out - kCCBlockSizeAES128, kCCBlockSizeAES128);

    } else {
	unsigned char tmp2[kCCBlockSizeAES128];
	unsigned char tmp3[kCCBlockSizeAES128];

	while(len > kCCBlockSizeAES128 * 2) {
	    memcpy(tmp, in, kCCBlockSizeAES128);
	    ret = CCCryptorUpdate(cryptorRef, in, kCCBlockSizeAES128,
				  out, kCCBlockSizeAES128, &size);
	    if (ret)
		abort();
	    for (i = 0; i < kCCBlockSizeAES128; i++)
		out[i] ^= ivec[i];
	    memcpy(ivec, tmp, kCCBlockSizeAES128);
	    len -= kCCBlockSizeAES128;
	    in += kCCBlockSizeAES128;
	    out += kCCBlockSizeAES128;
	}

	len -= kCCBlockSizeAES128;

	memcpy(tmp, in, kCCBlockSizeAES128); /* save last iv */
	ret = CCCryptorUpdate(cryptorRef, in, kCCBlockSizeAES128,
			      tmp2, kCCBlockSizeAES128, &size);
	if (ret)
	    abort();

	memcpy(tmp3, in + kCCBlockSizeAES128, len);
	memcpy(tmp3 + len, tmp2 + len, kCCBlockSizeAES128 - len); /* xor 0 */

	for (i = 0; i < len; i++)
	    out[i + kCCBlockSizeAES128] = tmp2[i] ^ tmp3[i];

	ret = CCCryptorUpdate(cryptorRef, tmp3, kCCBlockSizeAES128,
			      out, kCCBlockSizeAES128, &size);
	if (ret)
	    abort();
	for (i = 0; i < kCCBlockSizeAES128; i++)
	    out[i] ^= ivec[i];
	memcpy(ivec, tmp, kCCBlockSizeAES128);
    }
    CCCryptorRelease(cryptorRef);
}

krb5_error_code
krb5int_aes_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
		    const krb5_data *input, krb5_data *output)
{
    aes_cts_encrypt(input->data, output->data, input->length,
		    key->contents, key->length,
		    ivec ? ivec->data : NULL, 1);

    return 0;
}

krb5_error_code
krb5int_aes_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
		    const krb5_data *input, krb5_data *output)
{
    aes_cts_encrypt(input->data, output->data, input->length,
		    key->contents, key->length,
		    ivec ? ivec->data : NULL, 0);
    return 0;
}

static krb5_error_code
k5_aes_make_key(const krb5_data *randombits, krb5_keyblock *key)
{
    if (key->length != 16 && key->length != 32)
	return(KRB5_BAD_KEYSIZE);
    if (randombits->length != key->length)
	return(KRB5_CRYPTO_INTERNAL);

    key->magic = KV5M_KEYBLOCK;

    memcpy(key->contents, randombits->data, randombits->length);
    return(0);
}

static krb5_error_code
krb5int_aes_init_state (const krb5_keyblock *key, krb5_keyusage usage,
			krb5_data *state)
{
    state->length = 16;
    state->data = (void *) malloc(16);
    if (state->data == NULL)
	return ENOMEM;
    memset(state->data, 0, state->length);
    return 0;
}

const struct krb5_enc_provider krb5int_enc_aes128 = {
    16,
    16, 16,
    krb5int_aes_encrypt,
    krb5int_aes_decrypt,
    k5_aes_make_key,
    krb5int_aes_init_state,
    krb5int_default_free_state
};

const struct krb5_enc_provider krb5int_enc_aes256 = {
    16,
    32, 32,
    krb5int_aes_encrypt,
    krb5int_aes_decrypt,
    k5_aes_make_key,
    krb5int_aes_init_state,
    krb5int_default_free_state
};
