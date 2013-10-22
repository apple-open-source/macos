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
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#ifndef _OSSL_RAND_H_
#define _OSSL_RAND_H_    1

typedef struct RAND_METHOD   RAND_METHOD;

#include "ossl-engine.h"

/* symbol renaming */
#define RAND_bytes			ossl_RAND_bytes
#define RAND_pseudo_bytes		ossl_RAND_pseudo_bytes
#define RAND_seed			ossl_RAND_seed
#define RAND_cleanup			ossl_RAND_cleanup
#define RAND_add			ossl_RAND_add
#define RAND_set_rand_method		ossl_RAND_set_rand_method
#define RAND_get_rand_method		ossl_RAND_get_rand_method
#define RAND_set_rand_engine		ossl_RAND_set_rand_engine
#define RAND_file_name			ossl_RAND_file_name
#define RAND_load_file			ossl_RAND_load_file
#define RAND_write_file			ossl_RAND_write_file
#define RAND_status			ossl_RAND_status
#define RAND_egd			ossl_RAND_egd
#define RAND_egd_bytes			ossl_RAND_egd_bytes
#define RAND_fortuna_method		ossl_RAND_fortuna_method
#define RAND_egd_method			ossl_RAND_egd_method
#define RAND_unix_method		ossl_RAND_unix_method
#define RAND_cc_method			ossl_RAND_cc_method
#define RAND_w32crypto_method		ossl_RAND_w32crypto_method

/*
 *
 */

struct RAND_METHOD {
	void	(*seed)(const void *, int);
	int	(*bytes)(unsigned char *, int);
	void	(*cleanup)(void);
	void	(*add)(const void *, int, double);
	int	(*pseudorand)(unsigned char *, int);
	int	(*status)(void);
};

/*
 *
 */

int RAND_bytes(void *, size_t num);

int RAND_pseudo_bytes(void *, size_t);
void RAND_seed(const void *, size_t);
void RAND_cleanup(void);

void RAND_add(const void *, size_t, double);

int RAND_set_rand_method(const RAND_METHOD *);
const RAND_METHOD *
RAND_get_rand_method(void);
int RAND_set_rand_engine(ENGINE *);

const char *
RAND_file_name(char *, size_t);
int RAND_load_file(const char *, size_t);
int RAND_write_file(const char *);
int RAND_status(void);
int RAND_egd(const char *);
int RAND_egd_bytes(const char *, int);


const RAND_METHOD *RAND_arc4_method(void);
const RAND_METHOD *RAND_fortuna_method(void);
const RAND_METHOD *RAND_unix_method(void);
const RAND_METHOD *RAND_cc_method(void);
const RAND_METHOD *RAND_egd_method(void);
const RAND_METHOD *RAND_w32crypto_method(void);

#endif /* _OSSL_RAND_H_ */
