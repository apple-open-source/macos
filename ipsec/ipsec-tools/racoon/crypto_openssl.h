/* $Id: crypto_openssl.h,v 1.11 2004/11/13 11:28:01 manubsd Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CRYPTO_OPENSSL_H
#define _CRYPTO_OPENSSL_H

#include "vmbuf.h"
#include "crypto_openssl.h"
#ifdef HAVE_OPENSSL
#include <openssl/x509v3.h>
#include <openssl/rsa.h>

#define GENT_OTHERNAME	GEN_OTHERNAME
#define GENT_EMAIL	GEN_EMAIL
#define GENT_DNS	GEN_DNS
#define GENT_IPADD	GEN_IPADD

extern int eay_cmp_asn1dn (vchar_t *, vchar_t *);
extern int eay_get_x509subjectaltname (vchar_t *, char **, int *, int, int*);
extern vchar_t *eay_get_x509_common_name (vchar_t *);

/* string error */
extern char *eay_strerror (void);

/* OpenSSL initialization */
extern void eay_init (void);
#endif /* HAVE_OPENSSL */

/* DES */
extern vchar_t *eay_des_encrypt (vchar_t *, vchar_t *, vchar_t *);
extern vchar_t *eay_des_decrypt (vchar_t *, vchar_t *, vchar_t *);
extern int eay_des_weakkey (vchar_t *);
extern int eay_des_keylen (int);

/* 3DES */
extern vchar_t *eay_3des_encrypt (vchar_t *, vchar_t *, vchar_t *);
extern vchar_t *eay_3des_decrypt (vchar_t *, vchar_t *, vchar_t *);
extern int eay_3des_weakkey (vchar_t *);
extern int eay_3des_keylen (int);

/* AES(RIJNDAEL) */
extern vchar_t *eay_aes_encrypt (vchar_t *, vchar_t *, vchar_t *);
extern vchar_t *eay_aes_decrypt (vchar_t *, vchar_t *, vchar_t *);
extern int eay_aes_weakkey (vchar_t *);
extern int eay_aes_keylen (int);

/* misc */
extern int eay_null_keylen (int);
extern int eay_null_hashlen (void);

/* hash */
#if defined(WITH_SHA2)
/* HMAC SHA2 */
extern vchar_t *eay_hmacsha2_512_one (vchar_t *, vchar_t *);
extern caddr_t eay_hmacsha2_512_init (vchar_t *);
extern void eay_hmacsha2_512_update (caddr_t, vchar_t *);
extern vchar_t *eay_hmacsha2_512_final (caddr_t);
extern vchar_t *eay_hmacsha2_384_one (vchar_t *, vchar_t *);
extern caddr_t eay_hmacsha2_384_init (vchar_t *);
extern void eay_hmacsha2_384_update (caddr_t, vchar_t *);
extern vchar_t *eay_hmacsha2_384_final (caddr_t);
extern vchar_t *eay_hmacsha2_256_one (vchar_t *, vchar_t *);
extern caddr_t eay_hmacsha2_256_init (vchar_t *);
extern void eay_hmacsha2_256_update (caddr_t, vchar_t *);
extern vchar_t *eay_hmacsha2_256_final (caddr_t);
#endif
/* HMAC SHA1 */
extern vchar_t *eay_hmacsha1_one (vchar_t *, vchar_t *);
extern caddr_t eay_hmacsha1_init (vchar_t *);
extern void eay_hmacsha1_update (caddr_t, vchar_t *);
extern vchar_t *eay_hmacsha1_final (caddr_t);
/* HMAC MD5 */
extern vchar_t *eay_hmacmd5_one (vchar_t *, vchar_t *);
extern caddr_t eay_hmacmd5_init (vchar_t *);
extern void eay_hmacmd5_update (caddr_t, vchar_t *);
extern vchar_t *eay_hmacmd5_final (caddr_t);


#if defined(WITH_SHA2)
/* SHA2 functions */
extern caddr_t eay_sha2_512_init (void);
extern void eay_sha2_512_update (caddr_t, vchar_t *);
extern vchar_t *eay_sha2_512_final (caddr_t);
extern vchar_t *eay_sha2_512_one (vchar_t *);
#endif
extern int eay_sha2_512_hashlen (void);

#if defined(WITH_SHA2)
extern caddr_t eay_sha2_384_init (void);
extern void eay_sha2_384_update (caddr_t, vchar_t *);
extern vchar_t *eay_sha2_384_final (caddr_t);
extern vchar_t *eay_sha2_384_one (vchar_t *);
#endif
extern int eay_sha2_384_hashlen (void);

#if defined(WITH_SHA2)
extern caddr_t eay_sha2_256_init (void);
extern void eay_sha2_256_update (caddr_t, vchar_t *);
extern vchar_t *eay_sha2_256_final (caddr_t);
extern vchar_t *eay_sha2_256_one (vchar_t *);
#endif
extern int eay_sha2_256_hashlen (void);

/* SHA functions */
extern caddr_t eay_sha1_init (void);
extern void eay_sha1_update (caddr_t, vchar_t *);
extern vchar_t *eay_sha1_final (caddr_t);
extern vchar_t *eay_sha1_one (vchar_t *);
extern int eay_sha1_hashlen (void);

/* MD5 functions */
extern caddr_t eay_md5_init (void);
extern void eay_md5_update (caddr_t, vchar_t *);
extern vchar_t *eay_md5_final (caddr_t);
extern vchar_t *eay_md5_one (vchar_t *);
extern int eay_md5_hashlen (void);

/* RNG */
extern vchar_t *eay_set_random (u_int32_t);
extern u_int32_t eay_random (void);

/* DH */
extern int eay_dh_generate (vchar_t *, u_int32_t, u_int, vchar_t **, vchar_t **);
extern int eay_dh_compute (vchar_t *, u_int32_t, vchar_t *, vchar_t *, vchar_t *, vchar_t **);

/* misc */
#ifdef HAVE_OPENSSL
#include <openssl/bn.h>
extern int eay_v2bn (BIGNUM **, vchar_t *);
extern int eay_bn2v (vchar_t **, BIGNUM *);

extern const char *eay_version (void);
#endif

#define CBC_BLOCKLEN 8
#define IPSEC_ENCRYPTKEYLEN 8

#endif /* _CRYPTO_OPENSSL_H */
