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

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/* Header for dynamic hash table routines
 * Author - Eric Young
 */

#ifndef _OSSL_LHASH_H_
#define _OSSL_LHASH_H_

#include <stdio.h>

#include "ossl-bio.h"

/* symbol rewrite */
#define lh_new				ossl_lh_new
#define lh_free				ossl_lh_free
#define lh_insert			ossl_lh_insert
#define lh_delete			ossl_lh_delete
#define lh_retrieve			ossl_lh_retrieve
#define lh_doall			ossl_lh_doall
#define lh_doall_arg			ossl_lh_doall_arg
#define lh_strhash			ossl_lh_strhash
#define lh_num_items			ossl_lh_num_items

#define lh_stats			ossl_lh_stats
#define lh_node_stats			ossl_lh_node_stats
#define lh_node_usage_stats		ossl_lh_node_usage_stats

#define lh_stats_bio			ossl_lh_stats_bio
#define lh_node_stats_bio		ossl_lh_node_stats_bio
#define lh_node_usage_stats_bio		ossl_lh_node_usage_stats_bio

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct lhash_node_st {
	void *			data;
	struct lhash_node_st *	next;
#ifndef NO_HASH_COMP
	unsigned long		hash;
#endif
} LHASH_NODE;

typedef int (*LHASH_COMP_FN_TYPE)(const void *, const void *);
typedef unsigned long (*LHASH_HASH_FN_TYPE)(const void *);
typedef void (*LHASH_DOALL_FN_TYPE)(void *);
typedef void (*LHASH_DOALL_ARG_FN_TYPE)(void *, void *);

/* Macros for declaring and implementing type-safe wrappers for LHASH callbacks.
 * This way, callbacks can be provided to LHASH structures without function
 * pointer casting and the macro-defined callbacks provide per-variable casting
 * before deferring to the underlying type-specific callbacks. NB: It is
 * possible to place a "static" in front of both the DECLARE and IMPLEMENT
 * macros if the functions are strictly internal. */

/* First: "hash" functions */
#define DECLARE_LHASH_HASH_FN(f_name, o_type) \
	unsigned long f_name ## _LHASH_HASH(const void *);
#define IMPLEMENT_LHASH_HASH_FN(f_name, o_type)		       \
	unsigned long f_name ## _LHASH_HASH(const void *arg) { \
		o_type a = (o_type)arg;			       \
		return f_name(a); }
#define LHASH_HASH_FN(f_name)    f_name ## _LHASH_HASH

/* Second: "compare" functions */
#define DECLARE_LHASH_COMP_FN(f_name, o_type) \
	int f_name ## _LHASH_COMP(const void *, const void *);
#define IMPLEMENT_LHASH_COMP_FN(f_name, o_type)				\
	int f_name ## _LHASH_COMP(const void *arg1, const void *arg2) {	\
		o_type a = (o_type)arg1;				\
		o_type b = (o_type)arg2;				\
		return f_name(a, b); }
#define LHASH_COMP_FN(f_name)    f_name ## _LHASH_COMP

/* Third: "doall" functions */
#define DECLARE_LHASH_DOALL_FN(f_name, o_type) \
	void f_name ## _LHASH_DOALL(void *);
#define IMPLEMENT_LHASH_DOALL_FN(f_name, o_type) \
	void f_name ## _LHASH_DOALL(void *arg) { \
		o_type a = (o_type)arg;		 \
		f_name(a); }
#define LHASH_DOALL_FN(f_name)    f_name ## _LHASH_DOALL

/* Fourth: "doall_arg" functions */
#define DECLARE_LHASH_DOALL_ARG_FN(f_name, o_type, a_type) \
	void f_name ## _LHASH_DOALL_ARG(void *, void *);
#define IMPLEMENT_LHASH_DOALL_ARG_FN(f_name, o_type, a_type)	  \
	void f_name ## _LHASH_DOALL_ARG(void *arg1, void *arg2) { \
		o_type a = (o_type)arg1;			  \
		a_type b = (a_type)arg2;			  \
		f_name(a, b); }
#define LHASH_DOALL_ARG_FN(f_name)    f_name ## _LHASH_DOALL_ARG

typedef struct lhash_st {
	LHASH_NODE **		b;
	LHASH_COMP_FN_TYPE	comp;
	LHASH_HASH_FN_TYPE	hash;
	unsigned int		num_nodes;
	unsigned int		num_alloc_nodes;
	unsigned int		p;
	unsigned int		pmax;
	unsigned long		up_load;        /* load times 256 */
	unsigned long		down_load;      /* load times 256 */
	unsigned long		num_items;

	unsigned long		num_expands;
	unsigned long		num_expand_reallocs;
	unsigned long		num_contracts;
	unsigned long		num_contract_reallocs;
	unsigned long		num_hash_calls;
	unsigned long		num_comp_calls;
	unsigned long		num_insert;
	unsigned long		num_replace;
	unsigned long		num_delete;
	unsigned long		num_no_delete;
	unsigned long		num_retrieve;
	unsigned long		num_retrieve_miss;
	unsigned long		num_hash_comps;

	int			error;
} LHASH;

#define LH_LOAD_MULT    256

/* Indicates a malloc() error in the last call, this is only bad
 * in lh_insert(). */
#define lh_error(lh)    ((lh)->error)

LHASH *lh_new(LHASH_HASH_FN_TYPE h, LHASH_COMP_FN_TYPE c);
void lh_free(LHASH *lh);
void *lh_insert(LHASH *lh, void *data);
void *lh_delete(LHASH *lh, const void *data);
void *lh_retrieve(LHASH *lh, const void *data);
void lh_doall(LHASH *lh, LHASH_DOALL_FN_TYPE func);
void lh_doall_arg(LHASH *lh, LHASH_DOALL_ARG_FN_TYPE func, void *arg);
unsigned long lh_strhash(const char *c);
unsigned long lh_num_items(const LHASH *lh);

void lh_stats(const LHASH *lh, FILE *out);
void lh_node_stats(const LHASH *lh, FILE *out);
void lh_node_usage_stats(const LHASH *lh, FILE *out);

void lh_stats_bio(const LHASH *lh, BIO *out);
void lh_node_stats_bio(const LHASH *lh, BIO *out);
void lh_node_usage_stats_bio(const LHASH *lh, BIO *out);

#ifdef  __cplusplus
}
#endif

#endif /*  _OSSL_LHASH_H_ */
