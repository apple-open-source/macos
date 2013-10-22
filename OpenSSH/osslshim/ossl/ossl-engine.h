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

#ifndef _OSSL_ENGINE_H_
#define _OSSL_ENGINE_H_			1

/* symbol renaming */
#define ENGINE_add_conf_module		ossl_ENGINE_add_conf_module
#define ENGINE_by_dso			ossl_ENGINE_by_dso
#define ENGINE_by_id			ossl_ENGINE_by_id
#define ENGINE_finish			ossl_ENGINE_finish
#define ENGINE_get_DH			ossl_ENGINE_get_DH
#define ENGINE_get_DSA			ossl_ENGINE_get_DSA
#define ENGINE_get_RSA			ossl_ENGINE_get_RSA
#define ENGINE_get_RAND			ossl_ENGINE_get_RAND
#define ENGINE_get_id			ossl_ENGINE_get_id
#define ENGINE_get_name			ossl_ENGINE_get_name
#define ENGINE_load_builtin_engines	ossl_ENGINE_load_builtin_engines
#define ENGINE_set_DH			ossl_ENGINE_set_DH
#define ENGINE_set_RSA			ossl_ENGINE_set_RSA
#define ENGINE_set_id			ossl_ENGINE_set_id
#define ENGINE_set_name			ossl_ENGINE_set_name
#define ENGINE_set_destroy_function	ossl_ENGINE_set_destroy_function
#define ENGINE_new			ossl_ENGINE_new
#define ENGINE_free			ossl_ENGINE_free
#define ENGINE_up_ref			ossl_ENGINE_up_ref
#define ENGINE_get_default_DH		ossl_ENGINE_get_default_DH
#define ENGINE_get_default_RSA		ossl_ENGINE_get_default_RSA
#define ENGINE_set_default_DH		ossl_ENGINE_set_default_DH
#define ENGINE_set_default_RSA		ossl_ENGINE_set_default_RSA

/*
 *
 */

typedef struct ossl_engine   ENGINE;

#include "ossl-rsa.h"
#include "ossl-dsa.h"
#include "ossl-dh.h"
#include "ossl-rand.h"

/*
 *
 */

#define OPENSSL_DYNAMIC_VERSION    (unsigned long)0x00020000

typedef int (*openssl_bind_engine)(ENGINE *, const char *, const void *);
typedef unsigned long (*openssl_v_check)(unsigned long);

ENGINE *ENGINE_new(void);
int ENGINE_free(ENGINE *);
void ENGINE_add_conf_module(void);
void ENGINE_load_builtin_engines(void);
ENGINE *ENGINE_by_id(const char *);
ENGINE *ENGINE_by_dso(const char *, const char *);
int ENGINE_finish(ENGINE *);
int ENGINE_up_ref(ENGINE *);
int ENGINE_set_id(ENGINE *, const char *);
int ENGINE_set_name(ENGINE *, const char *);
int ENGINE_set_DSA(ENGINE *, const DSA_METHOD *);
int ENGINE_set_RSA(ENGINE *, const RSA_METHOD *);
int ENGINE_set_DH(ENGINE *, const DH_METHOD *);

int ENGINE_set_destroy_function(ENGINE *, void (*)(ENGINE *));

const char *ENGINE_get_id(const ENGINE *);
const char *ENGINE_get_name(const ENGINE *);
const DSA_METHOD *ENGINE_get_DSA(const ENGINE *);
const RSA_METHOD *ENGINE_get_RSA(const ENGINE *);
const DH_METHOD *ENGINE_get_DH(const ENGINE *);
const RAND_METHOD *ENGINE_get_RAND(const ENGINE *);

int ENGINE_set_default_RSA(ENGINE *);
ENGINE *ENGINE_get_default_DSA(void);
ENGINE *ENGINE_get_default_RSA(void);
int ENGINE_set_default_DH(ENGINE *);
ENGINE *ENGINE_get_default_DH(void);

#endif /* __OSSL_ENGINE_H_ */
