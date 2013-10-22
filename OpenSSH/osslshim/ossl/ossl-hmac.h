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

/*
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
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

#ifndef _OSSL_HMAC_H_
#define _OSSL_HMAC_H_    1

#include "ossl-evp.h"

/* symbol renaming */
#define HMAC_CTX_init		ossl_HMAC_CTX_init
#define HMAC_CTX_cleanup	ossl_HMAC_CTX_cleanup
#define HMAC_cleanup		ossl_HMAC_CTX_cleanup
#define HMAC_size		ossl_HMAC_size
#define HMAC_Init		ossl_HMAC_Init
#define HMAC_Init_ex		ossl_HMAC_Init_ex
#define HMAC_Update		ossl_HMAC_Update
#define HMAC_Final		ossl_HMAC_Final
#define HMAC			ossl_HMAC

/*
 *
 */

#define HMAC_MAX_MD_CBLOCK    128	/* assumes SHA512 is the largest */

typedef struct ossl_HMAC_CTX   HMAC_CTX;

struct ossl_HMAC_CTX {
	const EVP_MD *	md;
	EVP_MD_CTX	md_ctx;
	EVP_MD_CTX	i_ctx;
	EVP_MD_CTX	o_ctx;
	unsigned int	key_length;
	unsigned char	key[HMAC_MAX_MD_CBLOCK];
};


void HMAC_CTX_init(HMAC_CTX *);
void HMAC_CTX_cleanup(HMAC_CTX *ctx);

size_t HMAC_size(const HMAC_CTX *ctx);

void HMAC_Init(HMAC_CTX *, const void *, size_t, const EVP_MD *);
void HMAC_Init_ex(HMAC_CTX *, const void *, size_t, const EVP_MD *, ENGINE *);
void HMAC_Update(HMAC_CTX *ctx, const void *data, size_t len);
void HMAC_Final(HMAC_CTX *ctx, void *md, unsigned int *len);

void *HMAC(const EVP_MD *evp_md, const void *key, size_t key_len,
	    const void *data, size_t n, void *md, unsigned int *md_len);

#endif /* _OSSL_HMAC_H_ */
