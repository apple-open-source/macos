/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 * 
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "k5-int.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include "des_int.h"
#include "keyhash_provider.h"

#define CONFLENGTH 8

/* Force acceptance of krb5-beta5 md4des checksum for now. */
#define KRB5_MD4DES_BETA5_COMPAT

/* des-cbc(xorkey, conf | rsa-md4(conf | data)) */

/* this could be done in terms of the md4 and des providers, but
   that's less efficient, and there's no need for this to be generic */

static krb5_error_code
k5_md4des_hash(const krb5_keyblock *key, krb5_keyusage usage, const krb5_data *ivec,
	       const krb5_data *input, krb5_data *output)
{
    krb5_error_code ret;
    krb5_data data;
    CC_MD4_CTX ctx;
    unsigned char conf[CONFLENGTH];
    unsigned char xorkey[8];
    unsigned int i;

    if (key->length != 8)
	return(KRB5_BAD_KEYSIZE);
    if (ivec)
	return(KRB5_CRYPTO_INTERNAL);
    if (output->length != (CONFLENGTH+CC_MD4_DIGEST_LENGTH))
	return(KRB5_CRYPTO_INTERNAL);

    /* create the confouder */

    data.length = CONFLENGTH;
    data.data = (char *) conf;
    if ((ret = krb5_c_random_make_octets(/* XXX */ 0, &data)))
	return(ret);

    /* create and schedule the encryption key */

    memcpy(xorkey, key->contents, sizeof(xorkey));
    for (i=0; i<sizeof(xorkey); i++)
	xorkey[i] ^= 0xf0;
    
    /* hash the confounder, then the input data */

    CC_MD4_Init(&ctx);
    CC_MD4_Update(&ctx, conf, CONFLENGTH);
    CC_MD4_Update(&ctx, (unsigned char *) input->data,
		   (unsigned int) input->length);
    CC_MD4_Final(output->data+CONFLENGTH, &ctx);

    /* construct the buffer to be encrypted */

    memcpy(output->data, conf, CONFLENGTH);

    /* encrypt it, in place.  this has a return value, but it's
       always zero.  */
	
	{
		CCCryptorStatus cret;
		size_t movedData;

		cret = CCCrypt(kCCEncrypt,
					   kCCAlgorithmDES,
					   0,
					   xorkey,
					   sizeof(xorkey),
					   mit_des_zeroblock,
					   output->data,
					   output->length,
					   output->data,
					   output->length,
					   &movedData);
		if (cret)
			return(KRB5_CRYPTO_INTERNAL);
		
	}
    return(0);
}

static krb5_error_code
k5_md4des_verify(const krb5_keyblock *key, krb5_keyusage usage,
		 const krb5_data *ivec,
		 const krb5_data *input, const krb5_data *hash,
		 krb5_boolean *valid)
{
    CC_MD4_CTX ctx;
    unsigned char plaintext[CONFLENGTH+CC_MD4_DIGEST_LENGTH];
    unsigned char xorkey[8];
    unsigned char digest[CC_MD4_DIGEST_LENGTH];
    unsigned int i;
    int compathash = 0;

    if (key->length != 8)
	return(KRB5_BAD_KEYSIZE);
    if (ivec)
	return(KRB5_CRYPTO_INTERNAL);
    if (hash->length != (CONFLENGTH+CC_MD4_DIGEST_LENGTH)) {
#ifdef KRB5_MD4DES_BETA5_COMPAT
	if (hash->length != CC_MD4_DIGEST_LENGTH)
	    return(KRB5_CRYPTO_INTERNAL);
	else
	    compathash = 1;
#else
	return(KRB5_CRYPTO_INTERNAL);
#endif
	return(KRB5_CRYPTO_INTERNAL);
    }

    /* create and schedule the encryption key */

    memcpy(xorkey, key->contents, sizeof(xorkey));
    if (!compathash) {
	for (i=0; i<sizeof(xorkey); i++)
	    xorkey[i] ^= 0xf0;
    }
    
    /* decrypt it.  this has a return value, but it's always zero.  */

	{
		CCCryptorStatus ret;
		size_t movedData;
		
		ret = CCCrypt(kCCEncrypt,
					  kCCAlgorithmDES,
					  0,
					  xorkey,
					  sizeof(xorkey),
					  compathash ? xorkey : mit_des_zeroblock,
					  hash->data,
					  hash->length,
					  plaintext,
					  sizeof(plaintext),
					  &movedData);
		if (ret)
			return(KRB5_CRYPTO_INTERNAL);
		
	}

    /* hash the confounder, then the input data */

    CC_MD4_Init(&ctx);
    if (!compathash) {
	CC_MD4_Update(&ctx, plaintext, CONFLENGTH);
    }
    CC_MD4_Update(&ctx, (unsigned char *) input->data, 
		   (unsigned int) input->length);
    CC_MD4_Final(digest, &ctx);

    /* compare the decrypted hash to the computed one */

    if (!compathash) {
	*valid =
	    (memcmp(plaintext+CONFLENGTH, digest, CC_MD4_DIGEST_LENGTH)
	     == 0);
    } else {
	*valid =
	    (memcmp(plaintext, digest, CC_MD4_DIGEST_LENGTH) == 0);
    }

    memset(plaintext, 0, sizeof(plaintext));

    return(0);
}

const struct krb5_keyhash_provider krb5int_keyhash_md4des = {
    CONFLENGTH+CC_MD4_DIGEST_LENGTH,
    k5_md4des_hash,
    k5_md4des_verify
};
