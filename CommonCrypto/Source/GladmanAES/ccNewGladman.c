/* 
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
 * ccNewGladman.c - shim between Gladman AES and CommonEncryption.
 *
 * Created 3/30/06 by Doug Mitchell. 
 */

#include <CommonCrypto/aesopt.h>

#ifdef	_APPLE_COMMON_CRYPTO_

#include <strings.h>

int aes_cc_set_key(
	aes_cc_ctx *cx, 
	const void *rawKey, 
	aes_32t keyLength,
	int forEncrypt)
{
	if(forEncrypt) {
		switch(keyLength) {
			case 16: 
				aes_encrypt_key128((const unsigned char *)rawKey, &cx->encrypt);
				break;
			case 24: 
				aes_encrypt_key192((const unsigned char *)rawKey, &cx->encrypt);
				break;
			case 32: 
				aes_encrypt_key256((const unsigned char *)rawKey, &cx->encrypt);
				break;
			default:
				return -1;
		}
		cx->encrypt.cbcEnable = 0;
	}
	else {
		switch(keyLength) {
			case 16: 
				aes_decrypt_key128((const unsigned char *)rawKey, &cx->decrypt);
				break;
			case 24: 
				aes_decrypt_key192((const unsigned char *)rawKey, &cx->decrypt);
				break;
			case 32: 
				aes_decrypt_key256((const unsigned char *)rawKey, &cx->decrypt);
				break;
			default:
				return -1;
		}
		cx->decrypt.cbcEnable = 0;
	}
	return 0;
}

void aes_cc_set_iv(aes_cc_ctx *cx, int forEncrypt, const void *iv)
{
	if(forEncrypt) {
		if(iv == NULL) {
			cx->encrypt.cbcEnable = 0;
		}
		else {
			memmove(cx->encrypt.chainBuf, iv, AES_BLOCK_SIZE);
			cx->encrypt.cbcEnable = 1;
		}
	}
	else {
		if(iv == NULL) {
			cx->decrypt.cbcEnable = 0;
		}
		else {
			memmove(cx->decrypt.chainBuf, iv, AES_BLOCK_SIZE);
			cx->decrypt.cbcEnable = 1;
		}
	}
}

#ifndef	NULL
#define NULL ((void *)0)
#endif

void aes_cc_encrypt(aes_cc_ctx *cx, const void *blocksIn, aes_32t numBlocks,
	void *blocksOut)
{
	aes_encrypt_cbc((const unsigned char *)blocksIn, 
		NULL,	/* IV - we set via aes_cc_set_iv */
		(unsigned)numBlocks, (unsigned char *)blocksOut, &cx->encrypt);
}

void aes_cc_decrypt(aes_cc_ctx *cx, const void *blocksIn, aes_32t numBlocks,
	void *blocksOut)
{
	aes_decrypt_cbc((const unsigned char *)blocksIn,
		NULL,	/* IV - we set via aes_cc_set_iv */
		(unsigned)numBlocks, (unsigned char *)blocksOut, &cx->decrypt);
}

#endif	/* _APPLE_COMMON_CRYPTO_ */
