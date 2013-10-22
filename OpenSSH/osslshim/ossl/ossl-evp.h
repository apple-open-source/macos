/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
 * Copyright (c) 2005 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _OSSL_EVP_H_
#define _OSSL_EVP_H_    1

#include <stdlib.h>

#include "ossl-engine.h"
#include "ossl-objects.h"

/* symbol renaming */
#define EVP_CIPHER_CTX_block_size		ossl_EVP_CIPHER_CTX_block_size
#define EVP_CIPHER_CTX_cipher			ossl_EVP_CIPHER_CTX_cipher
#define EVP_CIPHER_CTX_cleanup			ossl_EVP_CIPHER_CTX_cleanup
#define EVP_CIPHER_CTX_flags			ossl_EVP_CIPHER_CTX_flags
#define EVP_CIPHER_CTX_get_app_data		ossl_EVP_CIPHER_CTX_get_app_data
#define EVP_CIPHER_CTX_init			ossl_EVP_CIPHER_CTX_init
#define EVP_CIPHER_CTX_iv_length		ossl_EVP_CIPHER_CTX_iv_length
#define EVP_CIPHER_CTX_key_length		ossl_EVP_CIPHER_CTX_key_length
#define EVP_CIPHER_CTX_mode			ossl_EVP_CIPHER_CTX_mode
#define EVP_CIPHER_CTX_set_app_data		ossl_EVP_CIPHER_CTX_set_app_data
#define EVP_CIPHER_CTX_set_key_length		ossl_EVP_CIPHER_CTX_set_key_length
#define EVP_CIPHER_CTX_set_padding		ossl_EVP_CIPHER_CTX_set_padding
#define EVP_CIPHER_block_size			ossl_EVP_CIPHER_block_size
#define EVP_CIPHER_iv_length			ossl_EVP_CIPHER_iv_length
#define EVP_CIPHER_key_length			ossl_EVP_CIPHER_key_length
#define EVP_CIPHER_nid				ossl_EVP_CIPHER_nid
#define EVP_Cipher				ossl_EVP_Cipher
#define EVP_CipherInit_ex			ossl_EVP_CipherInit_ex
#define EVP_CipherInit				ossl_EVP_CipherInit
#define EVP_CipherUpdate			ossl_EVP_CipherUpdate
#define EVP_CipherFinal_ex			ossl_EVP_CipherFinal_ex
#define EVP_Digest				ossl_EVP_Digest
#define EVP_DigestFinal				ossl_EVP_DigestFinal
#define EVP_DigestFinal_ex			ossl_EVP_DigestFinal_ex
#define EVP_DigestInit				ossl_EVP_DigestInit
#define EVP_DigestInit_ex			ossl_EVP_DigestInit_ex
#define EVP_DigestUpdate			ossl_EVP_DigestUpdate
#define EVP_get_digestbynid			ossl_EVP_get_digestbynid
#define EVP_MD_CTX_block_size			ossl_EVP_MD_CTX_block_size
#define EVP_MD_CTX_cleanup			ossl_EVP_MD_CTX_cleanup
#define EVP_MD_CTX_copy_ex			ossl_EVP_MD_CTX_copy_ex
#define EVP_MD_CTX_create			ossl_EVP_MD_CTX_create
#define EVP_MD_CTX_init				ossl_EVP_MD_CTX_init
#define EVP_MD_CTX_destroy			ossl_EVP_MD_CTX_destroy
#define EVP_MD_CTX_md				ossl_EVP_MD_CTX_md
#define EVP_MD_CTX_size				ossl_EVP_MD_CTX_size
#define EVP_MD_block_size			ossl_EVP_MD_block_size
#define EVP_MD_size				ossl_EVP_MD_size
#define EVP_aes_128_cbc				ossl_EVP_aes_128_cbc
#define EVP_aes_128_ecb				ossl_EVP_aes_128_ecb
#define EVP_aes_192_cbc				ossl_EVP_aes_192_cbc
#define EVP_aes_192_ecb				ossl_EVP_aes_192_ecb
#define EVP_aes_256_cbc				ossl_EVP_aes_256_cbc
#define EVP_aes_256_ecb				ossl_EVP_aes_256_ecb
#define EVP_aes_128_cfb8			ossl_EVP_aes_128_cfb8
#define EVP_aes_192_cfb8			ossl_EVP_aes_192_cfb8
#define EVP_aes_256_cfb8			ossl_EVP_aes_256_cfb8

#define EVP_des_cbc				ossl_EVP_des_cbc
#define EVP_des_ecb				ossl_EVP_des_ecb
#define EVP_des_ede3_cbc			ossl_EVP_des_ede3_cbc
#define EVP_des_ede3_ecb			ossl_EVP_des_ede3_ecb
#define EVP_bf_cbc				ossl_EVP_bf_cbc
#define EVP_bf_ecb				ossl_EVP_bf_ecb
#define EVP_cast5_cbc				ossl_EVP_cast5_cbc
#define EVP_cast5_ecb				ossl_EVP_cast5_ecb
#define EVP_enc_null				ossl_EVP_enc_null
#define EVP_md2					ossl_EVP_md2
#define EVP_md4					ossl_EVP_md4
#define EVP_md5					ossl_EVP_md5
#define EVP_md_null				ossl_EVP_md_null
#define EVP_rc2_40_cbc				ossl_EVP_rc2_40_cbc
#define EVP_rc2_64_cbc				ossl_EVP_rc2_64_cbc
#define EVP_rc2_cbc				ossl_EVP_rc2_cbc
#define EVP_rc4					ossl_EVP_rc4
#define EVP_rc4_40				ossl_EVP_rc4_40
#define EVP_camellia_128_cbc			ossl_EVP_camellia_128_cbc
#define EVP_camellia_192_cbc			ossl_EVP_camellia_192_cbc
#define EVP_camellia_256_cbc			ossl_EVP_camellia_256_cbc
#define EVP_sha					ossl_EVP_sha
#define EVP_sha1				ossl_EVP_sha1
#define EVP_sha224				ossl_EVP_sha224
#define EVP_sha256				ossl_EVP_sha256
#define EVP_sha384				ossl_EVP_sha384
#define EVP_sha512				ossl_EVP_sha512
#define EVP_rmd128				ossl_EVP_rmd128
#define EVP_rmd160				ossl_EVP_rmd160
#define EVP_rmd256				ossl_EVP_rmd256
#define EVP_rmd320				ossl_EVP_rmd320
#define EVP_ripemd128				ossl_EVP_ripemd128
#define EVP_ripemd160				ossl_EVP_ripemd160
#define EVP_ripemd256				ossl_EVP_ripemd256
#define EVP_ripemd320				ossl_EVP_ripemd320
#define PKCS5_PBKDF2_HMAC_SHA1			ossl_PKCS5_PBKDF2_HMAC_SHA1
#define EVP_BytesToKey				ossl_EVP_BytesToKey
#define EVP_get_pw_prompt			ossl_EVP_get_pw_prompt
#define EVP_set_pw_prompt			ossl_EVP_set_pw_prompt
#define EVP_read_pw_string			ossl_EVP_read_pw_string
#define EVP_get_cipherbyname			ossl_EVP_get_cipherbyname

/*
 * #define	OpenSSL_add_all_algorithms ossl_OpenSSL_add_all_algorithms
 * #define	OpenSSL_add_all_algorithms_conf ossl_OpenSSL_add_all_algorithms_conf
 * #define	OpenSSL_add_all_algorithms_noconf ossl_OpenSSL_add_all_algorithms_noconf
 */
#define EVP_CIPHER_CTX_ctrl			ossl_EVP_CIPHER_CTX_ctrl
#define EVP_CIPHER_CTX_rand_key			ossl_EVP_CIPHER_CTX_rand_key

#define EVP_PKEY_new				ossl_EVP_PKEY_new
#define EVP_PKEY_get1_RSA			ossl_EVP_PKEY_get1_RSA
#define EVP_PKEY_set1_RSA			ossl_EVP_PKEY_set1_RSA
#define EVP_PKEY_get1_DSA			ossl_EVP_PKEY_get1_DSA
#define EVP_PKEY_set1_DSA			ossl_EVP_PKEY_set1_DSA
#define EVP_PKEY_free				ossl_EVP_PKEY_free
#define EVP_PKEY_type				ossl_EVP_PKEY_type

#define EVP_DecodeBlock				ossl_EVP_DecodeBlock
#define EVP_DecodeFinal				ossl_EVP_DecodeFinal
#define EVP_DecodeUpdate			ossl_EVP_DecodeUpdate
#define EVP_DecodeInit				ossl_EVP_DecodeInit
#define EVP_EncodeBlock				ossl_EVP_EncodeBlock
#define EVP_EncodeFinal				ossl_EVP_EncodeFinal
#define EVP_EncodeUpdate			ossl_EVP_EncodeUpdate
#define EVP_EncodeInit				ossl_EVP_EncodeInit

#define OPENSSL_free				free
#define OPENSSL_malloc				malloc

/*
 *
 */

struct ossl_evp_pkey {
	int	type;
	int	save_type;
	int	references;
	union {
		char *	ptr;
		RSA *	rsa;
		DSA *	dsa;
		DH *	dh;
	}
	pkey;
	int	save_parameters;
};

typedef struct ossl_EVP_MD_CTX		EVP_MD_CTX;
typedef struct ossl_evp_pkey		EVP_PKEY;
typedef struct ossl_evp_md		EVP_MD;
typedef struct ossl_CIPHER		EVP_CIPHER;
typedef struct ossl_CIPHER_CTX		EVP_CIPHER_CTX;

#define EVP_MAX_IV_LENGTH	16
#define EVP_MAX_BLOCK_LENGTH	32
#define EVP_MAX_MD_SIZE		64

struct ossl_CIPHER {
	int		nid;
	int		block_size;
	int		key_len;
	int		iv_len;
	unsigned long	flags;

	/* The lowest 3 bits is used as integer field for the mode the
	 * cipher is used in (use EVP_CIPHER.._mode() to extract the
	 * mode). The rest of the flag field is a bitfield.
	 */
#define EVP_CIPH_STREAM_CIPHER		0x0
#define EVP_CIPH_ECB_MODE		0x1
#define EVP_CIPH_CBC_MODE		0x2
#define EVP_CIPH_CFB_MODE		0x3
#define EVP_CIPH_OFB_MODE		0x4
#define EVP_CIPH_CFB8_MODE		0x5
#define EVP_CIPH_MODE			0x7

#define EVP_CIPH_VARIABLE_LENGTH	0x008 /* variable key length */
#define EVP_CIPH_CUSTOM_IV		0x010
#define EVP_CIPH_ALWAYS_CALL_INIT	0x020
#define EVP_CIPH_RAND_KEY		0x200

#define EVP_MAX_KEY_LENGTH		32

	int		(*init)(EVP_CIPHER_CTX *, const unsigned char *, const unsigned char *, int);
	int		(*do_cipher)(EVP_CIPHER_CTX *, unsigned char *,
				    const unsigned char *, unsigned int);
	int		(*cleanup)(EVP_CIPHER_CTX *);
	int		ctx_size;
	void *		set_asn1_parameters;
	void *		get_asn1_parameters;
	int		(*ctrl)(EVP_CIPHER_CTX *, int type, int arg, void *ptr);
#define EVP_CTRL_RAND_KEY    0x6

	void *		app_data;
};

struct ossl_EVP_MD_CTX {
	const EVP_MD *	md;
	ENGINE *	engine;
	void *		ptr;
};

struct ossl_CIPHER_CTX {
	const EVP_CIPHER *	cipher;
	ENGINE *		engine;
	int			encrypt;
	int			buf_len; /* bytes stored in buf for EVP_CipherUpdate */
	unsigned char		oiv[EVP_MAX_IV_LENGTH];
	unsigned char		iv[EVP_MAX_IV_LENGTH];
	unsigned char		buf[EVP_MAX_BLOCK_LENGTH];
	int			num;
	void *			app_data;
	int			key_len;
	unsigned long		flags;
	void *			cipher_data;
	int			final_used;
	int			block_mask;
	unsigned char		final[EVP_MAX_BLOCK_LENGTH];
};

typedef int (*ossl_evp_md_init)(EVP_MD_CTX *);
typedef int (*ossl_evp_md_update)(EVP_MD_CTX *, const void *, size_t);
typedef int (*ossl_evp_md_final)(void *, EVP_MD_CTX *);
typedef int (*ossl_evp_md_cleanup)(EVP_MD_CTX *);

struct ossl_evp_md {
	int			hash_size;
	int			block_size;
	int			ctx_size;
	ossl_evp_md_init	init;
	ossl_evp_md_update	update;
	ossl_evp_md_final	final;
	ossl_evp_md_cleanup	cleanup;
};

typedef struct evp_Encode_Ctx_st {
	int		num;
	int		length;
	unsigned char	enc_data[80];
	int		line_num;
	int		expect_nl;
} EVP_ENCODE_CTX;

typedef struct evp_cipher_info_st {
	const EVP_CIPHER *	cipher;
	unsigned char		iv[EVP_MAX_IV_LENGTH];
} EVP_CIPHER_INFO;

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

/*
 * Avaible crypto algs
 */

const EVP_MD *EVP_md_null(void);
const EVP_MD *EVP_md2(void);
const EVP_MD *EVP_md4(void);
const EVP_MD *EVP_md5(void);
const EVP_MD *EVP_sha(void);
const EVP_MD *EVP_sha1(void);
const EVP_MD *EVP_sha224(void);
const EVP_MD *EVP_sha256(void);
const EVP_MD *EVP_sha384(void);
const EVP_MD *EVP_sha512(void);
const EVP_MD *EVP_rmd128(void);
const EVP_MD *EVP_rmd160(void);
const EVP_MD *EVP_rmd256(void);
const EVP_MD *EVP_rmd320(void);
const EVP_MD *EVP_ripemd128(void);
const EVP_MD *EVP_ripemd160(void);
const EVP_MD *EVP_ripemd256(void);
const EVP_MD *EVP_ripemd320(void);

const EVP_CIPHER *EVP_aes_128_cbc(void);
const EVP_CIPHER *EVP_aes_128_ecb(void);
const EVP_CIPHER *EVP_aes_192_cbc(void);
const EVP_CIPHER *EVP_aes_192_ecb(void);
const EVP_CIPHER *EVP_aes_256_cbc(void);
const EVP_CIPHER *EVP_aes_256_ecb(void);
const EVP_CIPHER *EVP_aes_128_cfb8(void);
const EVP_CIPHER *EVP_aes_192_cfb8(void);
const EVP_CIPHER *EVP_aes_256_cfb8(void);
const EVP_CIPHER *EVP_bf_cbc(void);
const EVP_CIPHER *EVP_bf_ecb(void);
const EVP_CIPHER *EVP_cast5_cbc(void);
const EVP_CIPHER *EVP_cast5_ecb(void);
const EVP_CIPHER *EVP_des_cbc(void);
const EVP_CIPHER *EVP_des_ecb(void);
const EVP_CIPHER *EVP_des_ede3_cbc(void);
const EVP_CIPHER *EVP_des_ede3_ecb(void);
const EVP_CIPHER *EVP_enc_null(void);
const EVP_CIPHER *EVP_rc2_40_cbc(void);
const EVP_CIPHER *EVP_rc2_64_cbc(void);
const EVP_CIPHER *EVP_rc2_cbc(void);
const EVP_CIPHER *EVP_rc4(void);
const EVP_CIPHER *EVP_rc4_40(void);
const EVP_CIPHER *EVP_camellia_128_cbc(void);
const EVP_CIPHER *EVP_camellia_192_cbc(void);
const EVP_CIPHER *EVP_camellia_256_cbc(void);

size_t EVP_MD_size(const EVP_MD *);
size_t EVP_MD_block_size(const EVP_MD *);

const EVP_MD *EVP_MD_CTX_md(EVP_MD_CTX *);
size_t EVP_MD_CTX_size(EVP_MD_CTX *);
size_t EVP_MD_CTX_block_size(EVP_MD_CTX *);

EVP_MD_CTX *EVP_MD_CTX_create(void);
void EVP_MD_CTX_init(EVP_MD_CTX *);
void EVP_MD_CTX_destroy(EVP_MD_CTX *);
int EVP_MD_CTX_cleanup(EVP_MD_CTX *);
int EVP_MD_CTX_copy_ex(EVP_MD_CTX *, const EVP_MD_CTX *);

int EVP_DigestInit(EVP_MD_CTX *, const EVP_MD *);
int EVP_DigestInit_ex(EVP_MD_CTX *, const EVP_MD *, ENGINE *);

int EVP_DigestUpdate(EVP_MD_CTX *, const void *, size_t);
int EVP_DigestFinal(EVP_MD_CTX *, void *, unsigned int *);
int EVP_DigestFinal_ex(EVP_MD_CTX *, void *, unsigned int *);

int EVP_Digest(const void *, size_t, void *, unsigned int *,
		const EVP_MD *, ENGINE *);

const EVP_MD *EVP_get_digestbynid(int nid);

/*
 *
 */

const EVP_CIPHER *EVP_get_cipherbyname(const char *);

size_t EVP_CIPHER_block_size(const EVP_CIPHER *);
size_t EVP_CIPHER_key_length(const EVP_CIPHER *);
size_t EVP_CIPHER_iv_length(const EVP_CIPHER *);
int EVP_CIPHER_nid(const EVP_CIPHER *);

void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *);
int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *);
int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *, int);
int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *, int);
unsigned long EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *);
int EVP_CIPHER_CTX_mode(const EVP_CIPHER_CTX *);

const EVP_CIPHER *EVP_CIPHER_CTX_cipher(EVP_CIPHER_CTX *);
size_t EVP_CIPHER_CTX_block_size(const EVP_CIPHER_CTX *);
size_t EVP_CIPHER_CTX_key_length(const EVP_CIPHER_CTX *);
size_t EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *);
void *EVP_CIPHER_CTX_get_app_data(EVP_CIPHER_CTX *);
void EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *, void *);

int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *, int, int, void *);
int EVP_CIPHER_CTX_rand_key(EVP_CIPHER_CTX *, void *);


int EVP_CipherInit_ex(EVP_CIPHER_CTX *, const EVP_CIPHER *, ENGINE *,
    const void *, const void *, int);
int EVP_CipherInit(EVP_CIPHER_CTX *, const EVP_CIPHER *, const void *,
		    const void *, int);

int EVP_CipherUpdate(EVP_CIPHER_CTX *, void *, int *, void *, size_t);
int EVP_CipherFinal_ex(EVP_CIPHER_CTX *, void *, int *);

int EVP_Cipher(EVP_CIPHER_CTX *, void *, const void *, size_t);

int PKCS5_PBKDF2_HMAC_SHA1(const void *, size_t, const void *, size_t,
unsigned long, size_t, void *);

int EVP_BytesToKey(const EVP_CIPHER *, const EVP_MD *,
const void *, const void *, size_t,
unsigned int, void *, void *);
char *EVP_get_pw_prompt(void);
void EVP_set_pw_prompt(const char *prompt);
int EVP_read_pw_string(char *buf, int len, const char *prompt, int verify);


RSA *EVP_PKEY_get1_RSA(EVP_PKEY *pkey);
int EVP_PKEY_set1_RSA(EVP_PKEY *pkey, RSA *key);
DSA *EVP_PKEY_get1_DSA(EVP_PKEY *pkey);
int EVP_PKEY_set1_DSA(EVP_PKEY *pkey, DSA *key);
EVP_PKEY *EVP_PKEY_new(void);
void EVP_PKEY_free(EVP_PKEY *key);
int EVP_PKEY_type(int type);

void EVP_DecodeInit(EVP_ENCODE_CTX *ctx);
int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
			    const unsigned char *in, int inl);
int EVP_DecodeBlock(unsigned char *t, const unsigned char *f, int n);
int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl);
void EVP_EncodeInit(EVP_ENCODE_CTX *ctx);
void EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
			    const unsigned char *in, int inl);
void EVP_EncodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl);
int EVP_EncodeBlock(unsigned char *t, const unsigned char *f, int dlen);


#define EVP_PKEY_NONE		NID_undef
#define EVP_PKEY_RSA		NID_rsaEncryption
#define EVP_PKEY_RSA2		NID_rsa

#define EVP_PKEY_DSA		NID_dsa
#define EVP_PKEY_DSA1		NID_dsa_2
#define EVP_PKEY_DSA2		NID_dsaWithSHA
#define EVP_PKEY_DSA3		NID_dsaWithSHA1
#define EVP_PKEY_DSA4		NID_dsaWithSHA1_2
#define EVP_PKEY_DH		NID_dhKeyAgreement

#endif /* _OSSL_EVP_H_ */
