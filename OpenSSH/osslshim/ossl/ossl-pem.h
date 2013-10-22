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

#ifndef _OSSL_PEM_H_
#define _OSSL_PEM_H_

#include "ossl-bio.h"

#define PEM_STRING_EVP_PKEY		"ANY PRIVATE KEY"
#define PEM_STRING_RSA			"RSA PRIVATE KEY"
#define PEM_STRING_DSA			"DSA PRIVATE KEY"
#define PEM_STRING_RSA_PUBLIC		"RSA PUBLIC KEY"
#define PEM_STRING_DSA_PUBLIC		"DSA PUBLIC KEY"
#define PEM_STRING_PUBLIC		"PUBLIC KEY"

#define PEM_BUFSIZE			1024

#define PEM_TYPE_ENCRYPTED		10
#define PEM_TYPE_MIC_ONLY		20
#define PEM_TYPE_MIC_CLEAR		30
#define PEM_TYPE_CLEAR			40

/* Rewrite symbols */
#define PEM_bytes_read_bio		ossl_PEM_bytes_read_bio

#define PEM_read_bio			ossl_PEM_read_bio
#define PEM_read_bio_PrivateKey		ossl_PEM_read_bio_PrivateKey

#define PEM_write_bio_RSAPrivateKey	ossl_PEM_write_bio_RSAPrivateKey
#define PEM_write_bio_DSAPrivateKey	ossl_PEM_write_bio_DSAPrivateKey
#define PEM_do_header			ossl_PEM_do_header
#define PEM_get_EVP_CIPHER_INFO		ossl_PEM_get_EVP_CIPHER_INFO
#define PEM_write_RSAPrivateKey		ossl_PEM_write_RSAPrivateKey
#define PEM_write_DSAPrivateKey		ossl_PEM_write_DSAPrivateKey

#define PEM_read_PUBKEY			ossl_PEM_read_PUBKEY
#define PEM_write_DSA_PUBKEY		ossl_PEM_write_DSA_PUBKEY
#define PEM_write_RSA_PUBKEY		ossl_PEM_write_RSA_PUBKEY

typedef int   pem_password_cb (char *buf, int size, int rwflag, void *userdata);

int PEM_write_bio_RSAPrivateKey(BIO *bp, RSA *x, const EVP_CIPHER *enc,
    unsigned char *kstr, int klen, pem_password_cb *cb, void *u);

int PEM_write_bio_DSAPrivateKey(BIO *bp, DSA *x, const EVP_CIPHER *enc,
    unsigned char *kstr, int klen, pem_password_cb *cb, void *u);

int PEM_bytes_read_bio(unsigned char **pdata, long *plen, char **pnm, const char *name, BIO *bp,
    pem_password_cb *cb, void *u);
int PEM_read_bio(BIO *bp, char **name, char **header, unsigned char **data, long *len);
EVP_PKEY *PEM_read_bio_PrivateKey(BIO *bp, EVP_PKEY **x, pem_password_cb *cb, void *u);
int PEM_do_header(EVP_CIPHER_INFO *cipher, unsigned char *data, long *len,
    pem_password_cb *callback, void *u);
int PEM_get_EVP_CIPHER_INFO(char *header, EVP_CIPHER_INFO *cipher);
RSA *PEM_read_RSAPublicKey(FILE *fp, RSA *rsa, pem_password_cb *cb, void *u);
int PEM_write_RSAPrivateKey(FILE *fp, RSA *rsa, const EVP_CIPHER *enc, unsigned char *kstr,
    int klen, pem_password_cb *callback, void *u);
int PEM_write_DSAPrivateKey(FILE *fp, DSA *dsa, const EVP_CIPHER *enc, unsigned char *kstr,
    int klen, pem_password_cb *callback, void *u);

EVP_PKEY *PEM_read_PUBKEY(FILE *fp, EVP_PKEY **pkey, pem_password_cb *cb, void *u);
int PEM_write_DSA_PUBKEY(FILE *fp, DSA *dsa);
int PEM_write_RSA_PUBKEY(FILE *fp, RSA *dsa);

#endif /* _OSSL_PEM_H_ */
