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

#include "ossl-config.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "krb5-types.h"
#include "rfc2459_asn1.h" /* XXX */
#include "der.h"

#include "ossl-bio.h"
#include "ossl-bn.h"
#include "ossl-rand.h"

#define BN_prime_checks    0
#define BN_prime_checks_for_size(b) \
	((b) >= 1300 ?  2 :	    \
	(b) >= 850 ?  3 :	    \
	(b) >= 650 ?  4 :	    \
	(b) >= 550 ?  5 :	    \
	(b) >= 550 ?  5 :	    \
	(b) >= 400 ?  7 :	    \
	(b) >= 350 ?  8 :	    \
	(b) >= 300 ?  9 :	    \
	(b) >= 250 ? 12 :	    \
	(b) >= 200 ? 15 :	    \
	(b) >= 150 ? 18 :	    \
	/* b >= 100 */ 27)

/* BN_window_bits_for_exponent_size -- macro for sliding window mod_exp functions */
#define BN_window_bits_for_exponent_size(b) \
	((b) > 671 ? 6 :		    \
	(b) > 239 ? 5 :			    \
	(b) > 79 ? 4 :			    \
	(b) > 17 ? 3 : 1)


#ifdef BN_LLONG
#define mul_add(r, a, w, c)			    \
	{					    \
		BN_ULLONG t;			    \
		t = (BN_ULLONG)w * (a) + (r) + (c); \
		(r) = Lw(t);			    \
		(c) = Hw(t);			    \
	}

#define mul(r, a, w, c)			      \
	{				      \
		BN_ULLONG t;		      \
		t = (BN_ULLONG)w * (a) + (c); \
		(r) = Lw(t);		      \
		(c) = Hw(t);		      \
	}

#define sqr(r0, r1, a)			\
	{				\
		BN_ULLONG t;		\
		t = (BN_ULLONG)(a)*(a);	\
		(r0) = Lw(t);		\
		(r1) = Hw(t);		\
	}

#elif defined(BN_UMULT_LOHI)
#define mul_add(r, a, w, c)			    \
	{					    \
		BN_ULONG high, low, ret, tmp = (a); \
		ret = (r);			    \
		BN_UMULT_LOHI(low, high, w, tmp);   \
		ret += (c);			    \
		(c) = (ret < (c)) ? 1 : 0;	    \
		(c) += high;			    \
		ret += low;			    \
		(c) += (ret < low) ? 1 : 0;	    \
		(r) = ret;			    \
	}

#define mul(r, a, w, c)				   \
	{					   \
		BN_ULONG high, low, ret, ta = (a); \
		BN_UMULT_LOHI(low, high, w, ta);   \
		ret = low + (c);		   \
		(c) = high;			   \
		(c) += (ret < low) ? 1 : 0;	   \
		(r) = ret;			   \
	}

#define sqr(r0, r1, a)				 \
	{					 \
		BN_ULONG tmp = (a);		 \
		BN_UMULT_LOHI(r0, r1, tmp, tmp); \
	}

#elif defined(BN_UMULT_HIGH)
#define mul_add(r, a, w, c)			    \
	{					    \
		BN_ULONG high, low, ret, tmp = (a); \
		ret = (r);			    \
		high = BN_UMULT_HIGH(w, tmp);	    \
		ret += (c);			    \
		low = (w) * tmp;		    \
		(c) = (ret < (c)) ? 1 : 0;	    \
		(c) += high;			    \
		ret += low;			    \
		(c) += (ret < low) ? 1 : 0;	    \
		(r) = ret;			    \
	}

#define mul(r, a, w, c)				   \
	{					   \
		BN_ULONG high, low, ret, ta = (a); \
		low = (w) * ta;			   \
		high = BN_UMULT_HIGH(w, ta);	   \
		ret = low + (c);		   \
		(c) = high;			   \
		(c) += (ret < low) ? 1 : 0;	   \
		(r) = ret;			   \
	}

#define sqr(r0, r1, a)				\
	{					\
		BN_ULONG tmp = (a);		\
		(r0) = tmp * tmp;		\
		(r1) = BN_UMULT_HIGH(tmp, tmp);	\
	}

#else

/*************************************************************
 * No long long type
 */

#define LBITS(a)	((a)&BN_MASK2l)
#define HBITS(a)	(((a)>>BN_BITS4)&BN_MASK2l)
#define L2HBITS(a)	(((a)<<BN_BITS4)&BN_MASK2)

#define LLBITS(a)	((a)&BN_MASKl)
#define LHBITS(a)	(((a)>>BN_BITS2)&BN_MASKl)
#define LL2HBITS(a)	((BN_ULLONG)((a)&BN_MASKl)<<BN_BITS2)

#define mul64(l, h, bl, bh)							 \
	{									 \
		BN_ULONG m, m1, lt, ht;						 \
										 \
		lt = l;								 \
		ht = h;								 \
		m = (bh)*(lt);							 \
		lt = (bl)*(lt);							 \
		m1 = (bl)*(ht);							 \
		ht = (bh)*(ht);							 \
		m = (m+m1)&BN_MASK2; if (m < m1) { ht += L2HBITS((BN_ULONG)1); } \
		ht += HBITS(m);							 \
		m1 = L2HBITS(m);						 \
		lt = (lt+m1)&BN_MASK2; if (lt < m1) { ht++; }			 \
		(l) = lt;							 \
		(h) = ht;							 \
	}

#define sqr64(lo, ho, in)				\
	{						\
		BN_ULONG l, h, m;			\
							\
		h = (in);				\
		l = LBITS(h);				\
		h = HBITS(h);				\
		m = (l)*(h);				\
		l *= l;					\
		h *= h;					\
		h += (m&BN_MASK2h1)>>(BN_BITS4-1);	\
		m = (m&BN_MASK2l)<<(BN_BITS4+1);	\
		l = (l+m)&BN_MASK2; if (l < m) { h++; }	\
		(lo) = l;				\
		(ho) = h;				\
	}

#define mul_add(r, a, bl, bh, c)			    \
	{						    \
		BN_ULONG l, h;				    \
							    \
		h = (a);				    \
		l = LBITS(h);				    \
		h = HBITS(h);				    \
		mul64(l, h, (bl), (bh));		    \
							    \
		/* non-multiply part */			    \
		l = (l+(c))&BN_MASK2; if (l < (c)) { h++; } \
		(c) = (r);				    \
		l = (l+(c))&BN_MASK2; if (l < (c)) { h++; } \
		(c) = h&BN_MASK2;			    \
		(r) = l;				    \
	}

#define mul(r, a, bl, bh, c)				   \
	{						   \
		BN_ULONG l, h;				   \
							   \
		h = (a);				   \
		l = LBITS(h);				   \
		h = HBITS(h);				   \
		mul64(l, h, (bl), (bh));		   \
							   \
		/* non-multiply part */			   \
		l += (c); if ((l&BN_MASK2) < (c)) { h++; } \
		(c) = h&BN_MASK2;			   \
		(r) = l&BN_MASK2;			   \
	}
#endif /* !BN_LLONG */

#define bn_wexpand(a, words)    (((words) <= (a)->dmax) ? (a) : bn_expand2((a), (words)))

#define bn_expand(a, bits)					 \
	((((((bits + BN_BITS2 - 1)) / BN_BITS2)) <= (a)->dmax) ? \
	(a) : bn_expand2((a), (bits + BN_BITS2 - 1) / BN_BITS2))
#define bn_pollute(a)
#define bn_check_top(a)

#if defined(BN_LLONG) || defined(BN_UMULT_HIGH)

BN_ULONG
bn_mul_add_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG c1 = 0;

	assert(num >= 0);
	if (num <= 0) {
		return (c1);
	}

	while (num&~3) {
		mul_add(rp[0], ap[0], w, c1);
		mul_add(rp[1], ap[1], w, c1);
		mul_add(rp[2], ap[2], w, c1);
		mul_add(rp[3], ap[3], w, c1);
		ap += 4;
		rp += 4;
		num -= 4;
	}
	if (num) {
		mul_add(rp[0], ap[0], w, c1);
		if (--num == 0) {
			return (c1);
		}
		mul_add(rp[1], ap[1], w, c1);
		if (--num == 0) {
			return (c1);
		}
		mul_add(rp[2], ap[2], w, c1);
		return (c1);
	}

	return (c1);
}


BN_ULONG
bn_mul_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG c1 = 0;

	assert(num >= 0);
	if (num <= 0) {
		return (c1);
	}

	while (num&~3) {
		mul(rp[0], ap[0], w, c1);
		mul(rp[1], ap[1], w, c1);
		mul(rp[2], ap[2], w, c1);
		mul(rp[3], ap[3], w, c1);
		ap += 4;
		rp += 4;
		num -= 4;
	}
	if (num) {
		mul(rp[0], ap[0], w, c1);
		if (--num == 0) {
			return (c1);
		}
		mul(rp[1], ap[1], w, c1);
		if (--num == 0) {
			return (c1);
		}
		mul(rp[2], ap[2], w, c1);
	}
	return (c1);
}


void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0) {
		return;
	}
	while (n&~3) {
		sqr(r[0], r[1], a[0]);
		sqr(r[2], r[3], a[1]);
		sqr(r[4], r[5], a[2]);
		sqr(r[6], r[7], a[3]);
		a += 4;
		r += 8;
		n -= 4;
	}
	if (n) {
		sqr(r[0], r[1], a[0]);
		if (--n == 0) {
			return;
		}
		sqr(r[2], r[3], a[1]);
		if (--n == 0) {
			return;
		}
		sqr(r[4], r[5], a[2]);
	}
}


#else /* !(defined(BN_LLONG) || defined(BN_UMULT_HIGH)) */

BN_ULONG
bn_mul_add_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG c = 0;
	BN_ULONG bl, bh;

	assert(num >= 0);
	if (num <= 0) {
		return ((BN_ULONG)0);
	}

	bl = LBITS(w);
	bh = HBITS(w);

	for ( ; ; ) {
		mul_add(rp[0], ap[0], bl, bh, c);
		if (--num == 0) {
			break;
		}
		mul_add(rp[1], ap[1], bl, bh, c);
		if (--num == 0) {
			break;
		}
		mul_add(rp[2], ap[2], bl, bh, c);
		if (--num == 0) {
			break;
		}
		mul_add(rp[3], ap[3], bl, bh, c);
		if (--num == 0) {
			break;
		}
		ap += 4;
		rp += 4;
	}
	return (c);
}


BN_ULONG
bn_mul_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG carry = 0;
	BN_ULONG bl, bh;

	assert(num >= 0);
	if (num <= 0) {
		return ((BN_ULONG)0);
	}

	bl = LBITS(w);
	bh = HBITS(w);

	for ( ; ; ) {
		mul(rp[0], ap[0], bl, bh, carry);
		if (--num == 0) {
			break;
		}
		mul(rp[1], ap[1], bl, bh, carry);
		if (--num == 0) {
			break;
		}
		mul(rp[2], ap[2], bl, bh, carry);
		if (--num == 0) {
			break;
		}
		mul(rp[3], ap[3], bl, bh, carry);
		if (--num == 0) {
			break;
		}
		ap += 4;
		rp += 4;
	}
	return (carry);
}


void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0) {
		return;
	}
	for ( ; ; ) {
		sqr64(r[0], r[1], a[0]);
		if (--n == 0) {
			break;
		}

		sqr64(r[2], r[3], a[1]);
		if (--n == 0) {
			break;
		}

		sqr64(r[4], r[5], a[2]);
		if (--n == 0) {
			break;
		}

		sqr64(r[6], r[7], a[3]);
		if (--n == 0) {
			break;
		}

		a += 4;
		r += 8;
	}
}


#endif /* !(defined(BN_LLONG) || defined(BN_UMULT_HIGH)) */

BN_ULONG
bn_sub_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULONG t1, t2;
	int c = 0;

	assert(n >= 0);
	if (n <= 0) {
		return ((BN_ULONG)0);
	}

	for ( ; ; ) {
		t1 = a[0];
		t2 = b[0];
		r[0] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) {
			c = (t1 < t2);
		}
		if (--n <= 0) {
			break;
		}

		t1 = a[1];
		t2 = b[1];
		r[1] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) {
			c = (t1 < t2);
		}
		if (--n <= 0) {
			break;
		}

		t1 = a[2];
		t2 = b[2];
		r[2] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) {
			c = (t1 < t2);
		}
		if (--n <= 0) {
			break;
		}

		t1 = a[3];
		t2 = b[3];
		r[3] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) {
			c = (t1 < t2);
		}
		if (--n <= 0) {
			break;
		}

		a += 4;
		b += 4;
		r += 4;
	}
	return (c);
}


static inline void
bn_correct_top(BIGNUM *a)
{
	BN_ULONG *ftl;

	if ((a)->top > 0) {
		for (ftl = &((a)->d[(a)->top-1]); (a)->top > 0; (a)->top--) {
			if (*(ftl--)) {
				break;
			}
		}
	}
	bn_pollute(a);
}


static BN_ULONG *
bn_expand_internal(const BIGNUM *b, int words)
{
	BN_ULONG *A, *a = NULL;
	const BN_ULONG *B;
	int i;

	bn_check_top(b);

	if (words > (INT_MAX/(4*BN_BITS2))) {
		/* BNerr(BN_F_BN_EXPAND_INTERNAL,BN_R_BIGNUM_TOO_LONG); */
		return (NULL);
	}
	if (BN_get_flags(b, BN_FLG_STATIC_DATA)) {
		/* BNerr(BN_F_BN_EXPAND_INTERNAL,BN_R_EXPAND_ON_STATIC_BIGNUM_DATA); */
		return (NULL);
	}
	a = A = (BN_ULONG *)malloc(sizeof(BN_ULONG)*words);
	if (A == NULL) {
		/* BNerr(BN_F_BN_EXPAND_INTERNAL,ERR_R_MALLOC_FAILURE); */
		return (NULL);
	}
#if 1
	B = b->d;
	if (B != NULL) {
		for (i = b->top>>2; i > 0; i--, A += 4, B += 4) {
			BN_ULONG a0, a1, a2, a3;
			a0 = B[0];
			a1 = B[1];
			a2 = B[2];
			a3 = B[3];
			A[0] = a0;
			A[1] = a1;
			A[2] = a2;
			A[3] = a3;
		}
		switch (b->top & 3) {
		case 3:
			A[2] = B[2];

		case 2:
			A[1] = B[1];

		case 1:
			A[0] = B[0];

		case 0:
			break;
		}
	}
#else
	memset(A, 0, sizeof(BN_ULONG)*words);
	memcpy(A, b->d, sizeof(b->d[0])*b->top);
#endif
	return (a);
}


static BIGNUM *
bn_expand2(BIGNUM *b, int words)
{
	bn_check_top(b);

	if (words > b->dmax) {
		BN_ULONG *a = bn_expand_internal(b, words);

		if (!a) {
			return (NULL);
		}
		if (b->d) {
			free(b->d);
		}
		b->d = a;
		b->dmax = words;
	}

	bn_check_top(b);

	return (b);
}


BIGNUM *
BN_new(void)
{
	BIGNUM *ret;

	if ((ret = (BIGNUM *)malloc(sizeof(BIGNUM))) == NULL) {
		/* BNerr(BN_F_BN_NEW,ERR_R_MALLOC_FAILURE); */
		return (NULL);
	}
    BN_init(ret);

	ret->flags = BN_FLG_MALLOCED;
	ret->top = 0;
	ret->dmax = 0;
	ret->d = NULL;

	bn_check_top(ret);

	return (ret);
}


void
BN_init(BIGNUM *a)
{
	memset(a, 0, sizeof(BIGNUM));
	bn_check_top(a);
}


void
BN_free(BIGNUM *a)
{
	if (NULL == a) {
		return;
	}
	bn_check_top(a);
	if ((a->d != NULL) && !(BN_get_flags(a, BN_FLG_STATIC_DATA))) {
		free(a->d);
	}
	if (a->flags & BN_FLG_MALLOCED) {
		free(a);
	} else{
		a->d = NULL;
	}
}


void
BN_clear(BIGNUM *a)
{
	bn_check_top(a);
	if (a->d != NULL) {
		memset(a->d, 0, a->dmax * sizeof(a->d[0]));
	}
	a->top = 0;
	a->neg = 0;
}


void
BN_clear_free(BIGNUM *a)
{
	int i;

	if (NULL == a) {
		return;
	}
	if (a->d != NULL) {
		memset(a->d, 0, a->dmax * sizeof(a->d[0]));
		if (!(BN_get_flags(a, BN_FLG_STATIC_DATA))) {
			free(a->d);
		}
	}
	i = BN_get_flags(a, BN_FLG_MALLOCED);
	memset(a, 0, sizeof(*a));
	if (i) {
		free(a);
	}
}


/*
 * Callback when doing slow generation of numbers, like primes.
 */
void
BN_GENCB_set(BN_GENCB *gencb, int (*cb_2)(int, int, BN_GENCB *), void *ctx)
{
	gencb->ver = 2;
	gencb->cb.cb_2 = cb_2;
	gencb->arg = ctx;
}


int
BN_GENCB_call(BN_GENCB *cb, int a, int b)
{
	if ((cb == NULL) || (cb->cb.cb_2 == NULL)) {
		return (1);
	}
	return (cb->cb.cb_2(a, b, cb));
}


/*
 * ctx pools
 */

/* How many bignums are in each "pool item"; */
#define BN_CTX_POOL_SIZE	16
/* The stack frame info is resizing, set a first-time expansion size; */
#define BN_CTX_START_FRAMES	32

/***********/
/* BN_POOL */
/***********/

/* A bundle of bignums that can be linked with other bundles */
typedef struct bignum_pool_item {
	/* The bignum values */
	BIGNUM				vals[BN_CTX_POOL_SIZE];
	/* Linked-list admin */
	struct bignum_pool_item *	prev, *next;
} BN_POOL_ITEM;

/* A linked-list of bignums grouped in bundles */
typedef struct bignum_pool {
	/* Linked-list admin */
	BN_POOL_ITEM *	head, *current, *tail;
	/* Stack depth and allocation size */
	unsigned	used, size;
} BN_POOL;

static void BN_POOL_init(BN_POOL *);
static void BN_POOL_finish(BN_POOL *);

#ifndef OPENSSL_NO_DEPRECATED
static void BN_POOL_reset(BN_POOL *);

#endif
static BIGNUM *BN_POOL_get(BN_POOL *);
static void BN_POOL_release(BN_POOL *, unsigned int);

/************/
/* BN_STACK */
/************/

/* A wrapper to manage the "stack frames" */
typedef struct bignum_ctx_stack {
	/* Array of indexes into the bignum stack */
	unsigned int *	indexes;
	/* Number of stack frames, and the size of the allocated array */
	unsigned int	depth, size;
} BN_STACK;
static void BN_STACK_init(BN_STACK *);
static void BN_STACK_finish(BN_STACK *);

#ifndef OPENSSL_NO_DEPRECATED
static void BN_STACK_reset(BN_STACK *);

#endif
static int BN_STACK_push(BN_STACK *, unsigned int);
static unsigned int BN_STACK_pop(BN_STACK *);

/**********/
/* BN_CTX */
/**********/

/* The opaque BN_CTX type */
struct bignum_ctx {
	/* The bignum bundles */
	BN_POOL		pool;
	/* The "stack frames", if you will */
	BN_STACK	stack;
	/* The number of bignums currently assigned */
	unsigned int	used;
	/* Depth of stack overflow */
	int		err_stack;
	/* Block "gets" until an "end" (compatibility behaviour) */
	int		too_many;
};

BN_CTX *
BN_CTX_new(void)
{
	BN_CTX *ret = malloc(sizeof(BN_CTX));

	if (!ret) {
		/* BNerr(BN_F_BN_CTX_NEW,ERR_R_MALLOC_FAILURE); */
		return (NULL);
	}
	/* Initialise the structure */
	BN_POOL_init(&ret->pool);
	BN_STACK_init(&ret->stack);
	ret->used = 0;
	ret->err_stack = 0;
	ret->too_many = 0;

	return (ret);
}


void
BN_CTX_free(BN_CTX *ctx)
{
	if (ctx == NULL) {
		return;
	}
	BN_STACK_finish(&ctx->stack);
	BN_POOL_finish(&ctx->pool);
	free(ctx);
}


void
BN_CTX_start(BN_CTX *ctx)
{
	/* If we're already overflowing ... */
	if (ctx->err_stack || ctx->too_many) {
		ctx->err_stack++;
	}
	/* (Try to) get a new frame pointer */
	else if (!BN_STACK_push(&ctx->stack, ctx->used)) {
		/* BNerr(BN_F_BN_CTX_START,BN_R_TOO_MANY_TEMPORARY_VARIABLES); */
		ctx->err_stack++;
	}
}


void
BN_CTX_end(BN_CTX *ctx)
{
	if (ctx->err_stack) {
		ctx->err_stack--;
	} else{
		unsigned int fp = BN_STACK_pop(&ctx->stack);
		/* Does this stack frame have anything to release? */
		if (fp < ctx->used) {
			BN_POOL_release(&ctx->pool, ctx->used - fp);
		}
		ctx->used = fp;
		/* Unjam "too_many" in case "get" had failed */
		ctx->too_many = 0;
	}
}


BIGNUM *
BN_CTX_get(BN_CTX *ctx)
{
	BIGNUM *ret;

	if (ctx->err_stack || ctx->too_many) {
		return (NULL);
	}
	if ((ret = BN_POOL_get(&ctx->pool)) == NULL) {
		/* Setting too_many prevents repeated "get" attempts from
		 * cluttering the error stack. */
		ctx->too_many = 1;
		/* BNerr(BN_F_BN_CTX_GET,BN_R_TOO_MANY_TEMPORARY_VARIABLES); */
		return (NULL);
	}
	/* OK, make sure the returned bignum is "zero" */
	BN_zero(ret);
	ctx->used++;
	return (ret);
}


/************/
/* BN_STACK */
/************/

static void
BN_STACK_init(BN_STACK *st)
{
	st->indexes = NULL;
	st->depth = st->size = 0;
}


static void
BN_STACK_finish(BN_STACK *st)
{
	if (st->size) {
		free(st->indexes);
	}
}


static int
BN_STACK_push(BN_STACK *st, unsigned int idx)
{
	if (st->depth == st->size) {
		/* Need to expand */
		unsigned int newsize = (st->size ?
		    (st->size * 3 / 2) : BN_CTX_START_FRAMES);
		unsigned int *newitems = malloc(newsize *
			sizeof(unsigned int));
		if (!newitems) {
			return (0);
		}
		if (st->depth) {
			memcpy(newitems, st->indexes, st->depth *
			    sizeof(unsigned int));
		}
		if (st->size) {
			free(st->indexes);
		}
		st->indexes = newitems;
		st->size = newsize;
	}
	st->indexes[(st->depth)++] = idx;
	return (1);
}


static unsigned int
BN_STACK_pop(BN_STACK *st)
{
	return (st->indexes[--(st->depth)]);
}


/***********/
/* BN_POOL */
/***********/

static void
BN_POOL_init(BN_POOL *p)
{
	p->head = p->current = p->tail = NULL;
	p->used = p->size = 0;
}


static void
BN_POOL_finish(BN_POOL *p)
{
	while (p->head) {
		unsigned int loop = 0;
		BIGNUM *bn = p->head->vals;
		while (loop++ < BN_CTX_POOL_SIZE) {
			if (bn->d) {
				BN_clear_free(bn);
			}
			bn++;
		}
		p->current = p->head->next;
		free(p->head);
		p->head = p->current;
	}
}


#ifndef OPENSSL_NO_DEPRECATED
static void BN_POOL_reset(BN_POOL *p)
{
	BN_POOL_ITEM *item = p->head;

	while (item) {
		unsigned int loop = 0;
		BIGNUM *bn = item->vals;
		while (loop++ < BN_CTX_POOL_SIZE) {
			if (bn->d) {
				BN_clear(bn);
			}
			bn++;
		}
		item = item->next;
	}
	p->current = p->head;
	p->used = 0;
}


#endif

static BIGNUM *
BN_POOL_get(BN_POOL *p)
{
	if (p->used == p->size) {
		BIGNUM *bn;
		unsigned int loop = 0;
		BN_POOL_ITEM *item = malloc(sizeof(BN_POOL_ITEM));
		if (!item) {
			return (NULL);
		}
		/* Initialise the structure */
		bn = item->vals;
		while (loop++ < BN_CTX_POOL_SIZE) {
			BN_init(bn++);
		}
		item->prev = p->tail;
		item->next = NULL;
		/* Link it in */
		if (!p->head) {
			p->head = p->current = p->tail = item;
		} else{
			p->tail->next = item;
			p->tail = item;
			p->current = item;
		}
		p->size += BN_CTX_POOL_SIZE;
		p->used++;
		/* Return the first bignum from the new pool */
		return (item->vals);
	}
	if (!p->used) {
		p->current = p->head;
	} else if ((p->used % BN_CTX_POOL_SIZE) == 0) {
		p->current = p->current->next;
	}
	return (p->current->vals + ((p->used++) % BN_CTX_POOL_SIZE));
}


static void
BN_POOL_release(BN_POOL *p, unsigned int num)
{
	unsigned int offset = (p->used - 1) % BN_CTX_POOL_SIZE;

	p->used -= num;
	while (num--) {
		bn_check_top(p->current->vals + offset);
		if (!offset) {
			offset = BN_CTX_POOL_SIZE - 1;
			p->current = p->current->prev;
		}else          {
			offset--;
		}
	}
}


/*
 *
 */
BIGNUM *
BN_dup(const BIGNUM *a)
{
	BIGNUM *t;

	if (NULL == a) {
		return (NULL);
	}
	bn_check_top(a);

	t = BN_new();
	if (t == NULL) {
		return (NULL);
	}
	if (!BN_copy(t, a)) {
		BN_free(t);
		return (NULL);
	}
	bn_check_top(t);
	return (t);
}


BIGNUM *
BN_copy(BIGNUM *a, const BIGNUM *b)
{
	int i;
	BN_ULONG *A;
	const BN_ULONG *B;

	bn_check_top(b);
	if (a == b) {
		return (a);
	}
	if (bn_wexpand(a, b->top) == NULL) {
		return (NULL);
	}
	A = a->d;
	B = b->d;
#if 1
	for (i = b->top>>2; i > 0; i--, A += 4, B += 4) {
		BN_ULONG a0, a1, a2, a3;
		a0 = B[0];
		a1 = B[1];
		a2 = B[2];
		a3 = B[3];
		A[0] = a0;
		A[1] = a1;
		A[2] = a2;
		A[3] = a3;
	}
	switch (b->top&3) {
	case 3:
		A[2] = B[2];

	case 2:
		A[1] = B[1];

	case 1:
		A[0] = B[0];

	case 0:
		break;
	}
#else
	memcpy(a->d, b->d, sizeof(b->d[0]) * b->top);
#endif
	a->top = b->top;
	a->neg = b->neg;
	bn_check_top(a);

	return (a);
}


const BIGNUM *
BN_value_one(void)
{
	static BN_ULONG data_one = 1L;
	static const BIGNUM const_one =
	{
		.d	= &data_one,
		.top	=	 1,
		.dmax	=	 1,
		.neg	=	 0,
		.flags	= BN_FLG_STATIC_DATA
	};

	return (&const_one);
}


int
BN_num_bits_word(BN_ULONG l)
{
	static const char bits[256] =
	{
		0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	};

#if defined(SIXTY_FOUR_BIT_LONG)
	if (l & 0xffffffff00000000L) {
		if (l & 0xffff000000000000L) {
			if (l & 0xff00000000000000L) {
				return (bits[(int)(l >> 56)] + 56);
			} else{
				return (bits[(int)(l >> 48)] + 48);
			}
		} else {
			if (l & 0x0000ff0000000000L) {
				return (bits[(int)(l >> 40)] + 40);
			} else{
				return (bits[(int)(l >> 32)] + 32);
			}
		}
	} else
#else
#ifdef SIXTY_FOUR_BIT
	if (l & 0xffffffff00000000LL) {
		if (l & 0xffff000000000000LL) {
			if (l & 0xff00000000000000LL) {
				return (bits[(int)(l>>56)]+56);
			} else{
				return (bits[(int)(l>>48)]+48);
			}
		} else {
			if (l & 0x0000ff0000000000LL) {
				return (bits[(int)(l>>40)]+40);
			} else{
				return (bits[(int)(l>>32)]+32);
			}
		}
	} else
#endif  /* SIXTY_FOUR_BIT */
#endif  /* SIXTY_FOUR_BIT_LONG */
	{
#if defined(THIRTY_TWO_BIT) || defined(SIXTY_FOUR_BIT) || defined(SIXTY_FOUR_BIT_LONG)
		if (l & 0xffff0000L) {
			if (l & 0xff000000L) {
				return (bits[(int)(l >> 24L)] + 24);
			} else{
				return (bits[(int)(l >> 16L)] + 16);
			}
		} else
#endif
		{
#if defined(SIXTEEN_BIT) || defined(THIRTY_TWO_BIT) || defined(SIXTY_FOUR_BIT) || defined(SIXTY_FOUR_BIT_LONG)
			if (l & 0xff00L) {
				return (bits[(int)(l >> 8)]+8);
			} else
#endif
			return (bits[(int)(l)]);
		}
	}
}


int
BN_num_bits(const BIGNUM *a)
{
	int i = a->top - 1;

	bn_check_top(a);

	if (BN_is_zero(a)) {
		return (0);
	}
	return ((i*BN_BITS2) + BN_num_bits_word(a->d[i]));
}


int
BN_lshift1(BIGNUM *r, const BIGNUM *a)
{
	register BN_ULONG *ap, *rp, t, c;
	int i;

	bn_check_top(r);
	bn_check_top(a);

	if (r != a) {
		r->neg = a->neg;
		if (bn_wexpand(r, a->top+1) == NULL) {
			return (0);
		}
		r->top = a->top;
	} else {
		if (bn_wexpand(r, a->top+1) == NULL) {
			return (0);
		}
	}
	ap = a->d;
	rp = r->d;
	c = 0;
	for (i = 0; i < a->top; i++) {
		t = *(ap++);
		*(rp++) = ((t<<1) | c) & BN_MASK2;
		c = (t & BN_TBIT) ? 1 : 0;
	}
	if (c) {
		*rp = 1;
		r->top++;
	}

	bn_check_top(r);
	return (1);
}


int
BN_rshift1(BIGNUM *r, const BIGNUM *a)
{
	BN_ULONG *ap, *rp, t, c;
	int i;

	bn_check_top(r);
	bn_check_top(a);

	if (BN_is_zero(a)) {
		BN_zero(r);
		return (1);
	}
	if (a != r) {
		if (bn_wexpand(r, a->top) == NULL) {
			return (0);
		}
		r->top = a->top;
		r->neg = a->neg;
	}
	ap = a->d;
	rp = r->d;
	c = 0;
	for (i = a->top-1; i >= 0; i--) {
		t = ap[i];
		rp[i] = ((t >> 1) & BN_MASK2) | c;
		c = (t & 1) ? BN_TBIT : 0;
	}
	bn_correct_top(r);
	bn_check_top(r);

	return (1);
}


int
BN_lshift(BIGNUM *r, const BIGNUM *a, int n)
{
	int i, nw, lb, rb;
	BN_ULONG *t, *f;
	BN_ULONG l;

	bn_check_top(r);
	bn_check_top(a);

	r->neg = a->neg;
	nw = n / BN_BITS2;
	if (bn_wexpand(r, a->top + nw + 1) == NULL) {
		return (0);
	}
	lb = n % BN_BITS2;
	rb = BN_BITS2-lb;
	f = a->d;
	t = r->d;
	t[a->top+nw] = 0;
	if (lb == 0) {
		for (i = a->top-1; i >= 0; i--) {
			t[nw+i] = f[i];
		}
	} else{
		for (i = a->top-1; i >= 0; i--) {
			l = f[i];
			t[nw+i+1] |= (l>>rb) & BN_MASK2;
			t[nw+i] = (l << lb) & BN_MASK2;
		}
	}
	memset(t, 0, nw * sizeof(t[0]));
	r->top = a->top+nw+1;
	bn_correct_top(r);
	bn_check_top(r);

	return (1);
}


int
BN_rshift(BIGNUM *r, const BIGNUM *a, int n)
{
	int i, j, nw, lb, rb;
	BN_ULONG *t, *f;
	BN_ULONG l, tmp;

	bn_check_top(r);
	bn_check_top(a);

	nw = n / BN_BITS2;
	rb = n % BN_BITS2;
	lb = BN_BITS2 - rb;
	if ((nw >= a->top) || (a->top == 0)) {
		BN_zero(r);
		return (1);
	}
	if (r != a) {
		r->neg = a->neg;
		if (bn_wexpand(r, a->top - nw + 1) == NULL) {
			return (0);
		}
	} else {
		if (n == 0) {
			return (1); /* or the copying loop will go berserk */
		}
	}

	f = &(a->d[nw]);
	t = r->d;
	j = a->top-nw;
	r->top = j;

	if (rb == 0) {
		for (i = j; i != 0; i--) {
			*(t++) = *(f++);
		}
	} else {
		l = *(f++);
		for (i = j-1; i != 0; i--) {
			tmp = (l >> rb) & BN_MASK2;
			l = *(f++);
			*(t++) = (tmp | (l << lb)) & BN_MASK2;
		}
		*(t++) = (l >> rb) & BN_MASK2;
	}
	bn_correct_top(r);
	bn_check_top(r);

	return (1);
}


int
BN_num_bytes(const BIGNUM *bn)
{
/*
 *  return (bn->top);
 */
	return ((int)((BN_num_bits(bn) + 7) / 8));
}


BIGNUM *
BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret)
{
	unsigned int i, m;
	unsigned int n;
	BN_ULONG l;
	BIGNUM *bn = NULL;

	if (ret == NULL) {
		ret = bn = BN_new();
	}
	if (ret == NULL) {
		return (NULL);
	}
	bn_check_top(ret);
	l = 0;
	n = len;
	if (n == 0) {
		ret->top = 0;
		return (ret);
	}
	i = ((n - 1) / BN_BYTES) + 1;
	m = ((n - 1) % (BN_BYTES));
	if (bn_wexpand(ret, (int)i) == NULL) {
		if (bn) {
			BN_free(bn);
		}
		return (NULL);
	}
	ret->top = i;
	ret->neg = 0;
	while (n--) {
		l = (l << 8L) | *(s++);
		if (m-- == 0) {
			ret->d[--i] = l;
			l = 0;
			m = BN_BYTES-1;
		}
	}

	/* need to call this due to clear byte at top if avoiding
	 * having the top bit set (-ve number) */
	bn_correct_top(ret);
	return (ret);
}


/* ignore negative */
int
BN_bn2bin(const BIGNUM *a, unsigned char *to)
{
	int n, i;
	BN_ULONG l;

	bn_check_top(a);
	n = i = BN_num_bytes(a);
	while (i--) {
		l = a->d[i / BN_BYTES];
		*(to++) = (unsigned char)(l >> (8 * (i % BN_BYTES))) & 0xff;
	}
	return (n);
}


int
BN_ucmp(const BIGNUM *a, const BIGNUM *b)
{
	int i;
	BN_ULONG t1, t2, *ap, *bp;

	bn_check_top(a);
	bn_check_top(b);

	i = a->top - b->top;
	if (i != 0) {
		return (i);
	}
	ap = a->d;
	bp = b->d;
	for (i = a->top - 1; i >= 0; i--) {
		t1 = ap[i];
		t2 = bp[i];
		if (t1 != t2) {
			return ((t1 > t2) ? 1 : -1);
		}
	}
	return (0);
}


int
BN_cmp(const BIGNUM *a, const BIGNUM *b)
{
	int i;
	int gt, lt;
	BN_ULONG t1, t2;

	if ((a == NULL) || (b == NULL)) {
		if (a != NULL) {
			return (-1);
		} else if (b != NULL) {
			return (1);
		} else{
			return (0);
		}
	}

	bn_check_top(a);
	bn_check_top(b);

	if (a->neg != b->neg) {
		if (a->neg) {
			return (-1);
		} else{
			return (1);
		}
	}
	if (a->neg == 0) {
		gt = 1;
		lt = -1;
	} else {
		gt = -1;
		lt = 1;
	}

	if (a->top > b->top) {
		return (gt);
	}
	if (a->top < b->top) {
		return (lt);
	}
	for (i = a->top-1; i >= 0; i--) {
		t1 = a->d[i];
		t2 = b->d[i];
		if (t1 > t2) {
			return (gt);
		}
		if (t1 < t2) {
			return (lt);
		}
	}

	return (0);
}


int
BN_set_bit(BIGNUM *a, int n)
{
	int i, j, k;

	if (n < 0) {
		return (0);
	}

	i = n / BN_BITS2;
	j = n % BN_BITS2;
	if (a->top <= i) {
		if (bn_wexpand(a, i+1) == NULL) {
			return (0);
		}
		for (k = a->top; k < i + 1; k++) {
			a->d[k] = 0;
		}
		a->top = i + 1;
	}

	a->d[i] |= (((BN_ULONG)1) << j);
	bn_check_top(a);

	return (1);
}


int
BN_clear_bit(BIGNUM *a, int n)
{
	int i, j;

	bn_check_top(a);
	if (n < 0) {
		return (0);
	}

	i = n / BN_BITS2;
	j = n % BN_BITS2;
	if (a->top <= i) {
		return (0);
	}

	a->d[i] &= (~(((BN_ULONG)1) << j));
	bn_correct_top(a);
	return (1);
}


int
BN_is_bit_set(const BIGNUM *a, int n)
{
	int i, j;

	bn_check_top(a);
	if (n < 0) {
		return (0);
	}
	i = n / BN_BITS2;
	j = n % BN_BITS2;
	if (a->top <= i) {
		return (0);
	}
	return (((a->d[i]) >> j) & ((BN_ULONG)1));
}


int
BN_mask_bits(BIGNUM *a, int n)
{
	int b, w;

	bn_check_top(a);
	if (n < 0) {
		return (0);
	}

	w = n / BN_BITS2;
	b = n % BN_BITS2;
	if (w >= a->top) {
		return (0);
	}
	if (b == 0) {
		a->top = w;
	} else{
		a->top = w + 1;
		a->d[w] &= ~(BN_MASK2 << b);
	}
	bn_correct_top(a);

	return (1);
}


static const char Hex[] = "0123456789ABCDEF";

/* Must 'free' the returned data */
char *
BN_bn2hex(const BIGNUM *a)
{
	int i, j, v, z = 0;
	char *buf;
	char *p;

	buf = (char *)malloc(a->top * BN_BYTES * 2 + 2);
	if (buf == NULL) {
		/* BNerr(BN_F_BN_BN2HEX,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = buf;
	if (a->neg) {
		*(p++) = '-';
	}
	if (BN_is_zero(a)) {
		*(p++) = '0';
	}
	for (i = a->top-1; i >= 0; i--) {
		for (j = BN_BITS2-8; j >= 0; j -= 8) {
			/* strip leading zeros */
			v = ((int)(a->d[i] >> (long)j)) & 0xff;
			if (z || (v != 0)) {
				*(p++) = Hex[v>>4];
				*(p++) = Hex[v&0x0f];
				z = 1;
			}
		}
	}
	*p = '\0';
err:
	return (buf);
}


/* Must 'free' the returned data */
char *
BN_bn2dec(const BIGNUM *a)
{
	int i = 0, num, ok = 0;
	char *buf = NULL;
	char *p;
	BIGNUM *t = NULL;
	BN_ULONG *bn_data = NULL, *lp;

	/* get an upper bound for the length of the decimal integer
	 * num <= (BN_num_bits(a) + 1) * log(2)
	 *     <= 3 * BN_num_bits(a) * 0.1001 + log(2) + 1     (rounding error)
	 *     <= BN_num_bits(a)/10 + BN_num_bits/1000 + 1 + 1
	 */
	i = BN_num_bits(a) * 3;
	num = (i / 10 + i / 1000 + 1) + 1;
	bn_data = (BN_ULONG *)malloc((num / BN_DEC_NUM + 1) * sizeof(BN_ULONG));
	buf = (char *)malloc(num + 3);
	if ((buf == NULL) || (bn_data == NULL)) {
		/* BNerr(BN_F_BN_BN2DEC,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	if ((t = BN_dup(a)) == NULL) {
		goto err;
	}

#define BUF_REMAIN    (num+3 - (size_t)(p - buf))
	p = buf;
	lp = bn_data;
	if (BN_is_zero(t)) {
		*(p++) = '0';
		*(p++) = '\0';
	} else {
		if (BN_is_negative(t)) {
			*p++ = '-';
		}

		i = 0;
		while (!BN_is_zero(t)) {
			*lp = BN_div_word(t, BN_DEC_CONV);
			lp++;
		}
		lp--;

		/* We now have a series of blocks, BN_DEC_NUM chars
		 * in length, where the last one needs truncation.
		 * The blocks need to be reversed in order. */
		snprintf(p, BUF_REMAIN, BN_DEC_FMT1, *lp);
		while (*p) {
			p++;
		}
		while (lp != bn_data) {
			lp--;
			snprintf(p, BUF_REMAIN, BN_DEC_FMT2, *lp);
			while (*p) {
				p++;
			}
		}
	}
	ok = 1;
err:
	if (bn_data != NULL) {
		free(bn_data);
	}
	if (t != NULL) {
		BN_free(t);
	}
	if (!ok && buf) {
		free(buf);
		buf = NULL;
	}

	return (buf);
}


int
BN_hex2bn(BIGNUM **bn, const char *a)
{
	BIGNUM *ret = NULL;
	BN_ULONG l = 0;
	int neg = 0, h, m, i, j, k, c;
	int num;

	if ((a == NULL) || (*a == '\0')) {
		return (0);
	}

	if (*a == '-') {
		neg = 1;
		a++;
	}

	for (i = 0; isxdigit((unsigned char)a[i]); i++) {
	}

	num = i + neg;
	if (bn == NULL) {
		return (num);
	}

	/* a is the start of the hex digits, and it is 'i' long */
	if (*bn == NULL) {
		if ((ret = BN_new()) == NULL) {
			return (0);
		}
	} else {
		ret = *bn;
		BN_zero(ret);
	}

	/* i is the number of hex digests; */
	if (bn_expand(ret, i * 4) == NULL) {
		goto err;
	}

	j = i; /* least significant 'hex' */
	m = 0;
	h = 0;
	while (j > 0) {
		m = ((BN_BYTES*2) <= j) ? (BN_BYTES*2) : j;
		l = 0;
		for ( ; ; ) {
			c = a[j-m];
			if ((c >= '0') && (c <= '9')) {
				k = c-'0';
			} else if ((c >= 'a') && (c <= 'f')) {
				k = c-'a'+10;
			} else if ((c >= 'A') && (c <= 'F')) {
				k = c-'A'+10;
			} else {
				k = 0; /* paranoia */
			}
			l = (l<<4) | k;

			if (--m <= 0) {
				ret->d[h++] = l;
				break;
			}
		}
		j -= (BN_BYTES * 2);
	}
	ret->top = h;
	bn_correct_top(ret);
	ret->neg = neg;

	*bn = ret;
	bn_check_top(ret);
	return (num);

err:
	if (*bn == NULL) {
		BN_free(ret);
	}
	return (0);
}


int
BN_dec2bn(BIGNUM **bn, const char *a)
{
	BIGNUM *ret = NULL;
	BN_ULONG l = 0;
	int neg = 0, i, j;
	int num;

	if ((a == NULL) || (*a == '\0')) {
		return (0);
	}
	if (*a == '-') {
		neg = 1;
		a++;
	}

	for (i = 0; isdigit((unsigned char)a[i]); i++) {
	}

	num = i + neg;
	if (bn == NULL) {
		return (num);
	}

	/* a is the start of the digits, and it is 'i' long.
	 * We chop it into BN_DEC_NUM digits at a time */
	if (*bn == NULL) {
		if ((ret = BN_new()) == NULL) {
			return (0);
		}
	} else {
		ret = *bn;
		BN_zero(ret);
	}

	/* i is the number of digests, a bit of an over expand; */
	if (bn_expand(ret, i * 4) == NULL) {
		goto err;
	}

	j = BN_DEC_NUM - (i % BN_DEC_NUM);
	if (j == BN_DEC_NUM) {
		j = 0;
	}
	l = 0;
	while (*a) {
		l *= 10;
		l += *a - '0';
		a++;
		if (++j == BN_DEC_NUM) {
			BN_mul_word(ret, BN_DEC_CONV);
			BN_add_word(ret, l);
			l = 0;
			j = 0;
		}
	}
	ret->neg = neg;

	bn_correct_top(ret);
	*bn = ret;
	bn_check_top(ret);
	return (num);

err:
	if (*bn == NULL) {
		BN_free(ret);
	}
	return (0);
}


int
BN_print_fp(FILE *fp, const BIGNUM *a)
{
	BIO *b;
	int ret;

	if ((b = BIO_new(BIO_s_file())) == NULL) {
		return (0);
	}
	BIO_set_fp(b, fp, BIO_NOCLOSE);
	ret = BN_print(b, a);
	BIO_free(b);
	return (ret);
}


int
BN_print(void *bp, const BIGNUM *a)
{
	int i, j, v, z = 0;
	int ret = 0;

	if ((a->neg) && (BIO_write((BIO *)bp, "-", 1) != 1)) {
		goto end;
	}
	if (BN_is_zero(a) && (BIO_write((BIO *)bp, "0", 1) != 1)) {
		goto end;
	}
	for (i = a->top - 1; i >= 0; i--) {
		for (j = BN_BITS2 - 4; j >= 0; j -= 4) {
			/* strip leading zeros */
			v = ((int)(a->d[i] >> (long)j)) & 0x0f;
			if (z || (v != 0)) {
				if (BIO_write((BIO *)bp, &(Hex[v]), 1) != 1) {
					goto end;
				}
				z = 1;
			}
		}
	}
	ret = 1;
end:
	return (ret);
}


void
BN_set_negative(BIGNUM *a, int b)
{
	if (b && !BN_is_zero(a)) {
		a->neg = 1;
	} else{
		a->neg = 0;
	}
}


int
BN_set_word(BIGNUM *a, BN_ULONG w)
{
	bn_check_top(a);
	if (bn_expand(a, (int)sizeof(BN_ULONG) * 8) == NULL) {
		return (0);
	}
	a->neg = 0;
	a->d[0] = w;
	a->top = (w ? 1 : 0);
	bn_check_top(a);
	return (1);
}


BN_ULONG
BN_get_word(const BIGNUM *a)
{
	if (a->top > 1) {
		return (BN_MASK2);
	} else if (a->top == 1) {
		return (a->d[0]);
	}
	return (0);
}


BN_ULONG
BN_mod_word(const BIGNUM *a, BN_ULONG w)
{
#ifndef BN_LLONG
	BN_ULONG ret = 0;
#else
	BN_ULLONG ret = 0;
#endif
	int i;

	if (w == 0) {
		return ((BN_ULONG)-1);
	}

	bn_check_top(a);
	w &= BN_MASK2;
	for (i = a->top - 1; i >= 0; i--) {
#ifndef BN_LLONG
		ret = ((ret << BN_BITS4) | ((a->d[i] >> BN_BITS4) & BN_MASK2l)) % w;
		ret = ((ret << BN_BITS4) | (a->d[i] & BN_MASK2l)) % w;
#else
		ret = (BN_ULLONG)(((ret << (BN_ULLONG)BN_BITS2) | a->d[i]) %
		    (BN_ULLONG)w);
#endif
	}

	return ((BN_ULONG)ret);
}


#ifdef BN_LLONG
static BN_ULONG
bn_div_words(BN_ULONG h, BN_ULONG l, BN_ULONG d)
{
	return ((BN_ULONG)(((((BN_ULLONG)h) << BN_BITS2) | l) / (BN_ULLONG)d));
}


#else

/* Divide h,l by d and return the result. */
/* I need to test this some more :-( */
BN_ULONG
bn_div_words(BN_ULONG h, BN_ULONG l, BN_ULONG d)
{
	BN_ULONG dh, dl, q, ret = 0, th, tl, t;
	int i, count = 2;

	if (d == 0) {
		return (BN_MASK2);
	}

	i = BN_num_bits_word(d);
	assert((i == BN_BITS2) || (h <= (BN_ULONG)1<<i));

	i = BN_BITS2 - i;
	if (h >= d) {
		h -= d;
	}

	if (i) {
		d <<= i;
		h = (h << i) | (l >> (BN_BITS2 - i));
		l <<= i;
	}
	dh = (d & BN_MASK2h) >> BN_BITS4;
	dl = (d & BN_MASK2l);
	for ( ; ; ) {
		if ((h >> BN_BITS4) == dh) {
			q = BN_MASK2l;
		} else{
			q = h / dh;
		}

		th = q * dh;
		tl = dl * q;
		for ( ; ; ) {
			t = h - th;
			if ((t & BN_MASK2h) ||
			    ((tl) <= (
				    (t<<BN_BITS4)|
				    ((l&BN_MASK2h)>>BN_BITS4)))) {
				break;
			}
			q--;
			th -= dh;
			tl -= dl;
		}
		t = (tl >> BN_BITS4);
		tl = (tl << BN_BITS4) & BN_MASK2h;
		th += t;

		if (l < tl) {
			th++;
		}
		l -= tl;
		if (h < th) {
			h += d;
			q--;
		}
		h -= th;

		if (--count == 0) {
			break;
		}

		ret = q << BN_BITS4;
		h = ((h << BN_BITS4) | (l >> BN_BITS4)) & BN_MASK2;
		l = (l & BN_MASK2l) << BN_BITS4;
	}
	ret |= q;

	return (ret);
}


#endif /* BN_LLONG */

BN_ULONG
BN_div_word(BIGNUM *a, BN_ULONG w)
{
	BN_ULONG ret = 0;
	int i, j;

	bn_check_top(a);
	w &= BN_MASK2;

	if (!w) {
		/* actually this an error (division by zero) */
		return ((BN_ULONG)-1);
	}
	if (a->top == 0) {
		return (0);
	}

	/* normalize input (so bn_div_words doesn't complain) */
	j = BN_BITS2 - BN_num_bits_word(w);
	w <<= j;
	if (!BN_lshift(a, a, j)) {
		return ((BN_ULONG)-1);
	}

	for (i = a->top-1; i >= 0; i--) {
		BN_ULONG l, d;

		l = a->d[i];
		d = bn_div_words(ret, l, w);
		ret = (l - ((d * w) & BN_MASK2)) & BN_MASK2;
		a->d[i] = d;
	}
	if ((a->top > 0) && (a->d[a->top-1] == 0)) {
		a->top--;
	}
	ret >>= j;
	bn_check_top(a);
	return (ret);
}


int
BN_add_word(BIGNUM *a, BN_ULONG w)
{
	BN_ULONG l;
	int i;

	bn_check_top(a);
	w &= BN_MASK2;

	/* degenerate case: w is zero */
	if (!w) {
		return (1);
	}
	/* degenerate case: a is zero */
	if (BN_is_zero(a)) {
		return (BN_set_word(a, w));
	}
	/* handle 'a' when negative */
	if (a->neg) {
		a->neg = 0;
		i = BN_sub_word(a, w);
		if (!BN_is_zero(a)) {
			a->neg = !(a->neg);
		}
		return (i);
	}
	/* Only expand (and risk failing) if it's possibly necessary */
	if (((BN_ULONG)(a->d[a->top - 1] + 1) == 0) &&
	    (bn_wexpand(a, a->top+1) == NULL)) {
		return (0);
	}
	i = 0;
	for ( ; ; ) {
		if (i >= a->top) {
			l = w;
		} else{
			l = (a->d[i] + w) & BN_MASK2;
		}
		a->d[i] = l;
		if (w > l) {
			w = 1;
		} else{
			break;
		}
		i++;
	}
	if (i >= a->top) {
		a->top++;
	}
	bn_check_top(a);
	return (1);
}


int
BN_sub_word(BIGNUM *a, BN_ULONG w)
{
	int i;

	bn_check_top(a);
	w &= BN_MASK2;

	/* degenerate case: w is zero */
	if (!w) {
		return (1);
	}
	/* degenerate case: a is zero */
	if (BN_is_zero(a)) {
		i = BN_set_word(a, w);
		if (i != 0) {
			BN_set_negative(a, 1);
		}
		return (i);
	}
	/* handle 'a' when negative */
	if (a->neg) {
		a->neg = 0;
		i = BN_add_word(a, w);
		a->neg = 1;
		return (i);
	}

	if ((a->top == 1) && (a->d[0] < w)) {
		a->d[0] = w - a->d[0];
		a->neg = 1;
		return (1);
	}
	i = 0;
	for ( ; ; ) {
		if (a->d[i] >= w) {
			a->d[i] -= w;
			break;
		} else {
			a->d[i] = (a->d[i] - w) & BN_MASK2;
			i++;
			w = 1;
		}
	}
	if ((a->d[i] == 0) && (i == (a->top-1))) {
		a->top--;
	}
	bn_check_top(a);
	return (1);
}


int
BN_mul_word(BIGNUM *a, BN_ULONG w)
{
	BN_ULONG ll;

	bn_check_top(a);
	w &= BN_MASK2;
	if (a->top) {
		if (w == 0) {
			BN_zero(a);
		} else{
			ll = bn_mul_words(a->d, a->d, a->top, w);
			if (ll) {
				if (bn_wexpand(a, a->top + 1) == NULL) {
					return (0);
				}
				a->d[a->top++] = ll;
			}
		}
	}
	bn_check_top(a);
	return (1);
}


/*
 *
 */

/* r can == a or b */
int
BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	const BIGNUM *tmp;
	int a_neg = a->neg, ret;

	bn_check_top(a);
	bn_check_top(b);

	/*  a +  b	a+b
	 *  a + -b	a-b
	 * -a +  b	b-a
	 * -a + -b	-(a+b)
	 */
	if (a_neg ^ b->neg) {
		/* only one is negative */
		if (a_neg) {
			tmp = a;
			a = b;
			b = tmp;
		}

		/* we are now a - b */

		if (BN_ucmp(a, b) < 0) {
			if (!BN_usub(r, b, a)) {
				return (0);
			}
			r->neg = 1;
		} else {
			if (!BN_usub(r, a, b)) {
				return (0);
			}
			r->neg = 0;
		}
		return (1);
	}

	ret = BN_uadd(r, a, b);
	r->neg = a_neg;
	bn_check_top(r);
	return (ret);
}


#ifdef BN_LLONG
BN_ULONG
bn_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULLONG ll = 0;

	assert(n >= 0);
	if (n <= 0) {
		return ((BN_ULONG)0);
	}

	for ( ; ; ) {
		ll += (BN_ULLONG)a[0] + b[0];
		r[0] = (BN_ULONG)ll & BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) {
			break;
		}

		ll += (BN_ULLONG)a[1] + b[1];
		r[1] = (BN_ULONG)ll & BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) {
			break;
		}

		ll += (BN_ULLONG)a[2] + b[2];
		r[2] = (BN_ULONG)ll & BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) {
			break;
		}

		ll += (BN_ULLONG)a[3] + b[3];
		r[3] = (BN_ULONG)ll & BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) {
			break;
		}

		a += 4;
		b += 4;
		r += 4;
	}

	return ((BN_ULONG)ll);
}


#else /* !BN_LLONG */

BN_ULONG
bn_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULONG c, l, t;

	assert(n >= 0);
	if (n <= 0) {
		return ((BN_ULONG)0);
	}

	c = 0;
	for ( ; ; ) {
		t = a[0];
		t = (t+c) & BN_MASK2;
		c = (t < c);
		l = (t + b[0]) & BN_MASK2;
		c += (l < t);
		r[0] = l;
		if (--n <= 0) {
			break;
		}

		t = a[1];
		t = (t + c) & BN_MASK2;
		c = (t < c);
		l = (t + b[1]) & BN_MASK2;
		c += (l < t);
		r[1] = l;
		if (--n <= 0) {
			break;
		}

		t = a[2];
		t = (t+c) & BN_MASK2;
		c = (t < c);
		l = (t+b[2]) & BN_MASK2;
		c += (l < t);
		r[2] = l;
		if (--n <= 0) {
			break;
		}

		t = a[3];
		t = (t + c)&BN_MASK2;
		c = (t < c);
		l = (t + b[3]) & BN_MASK2;
		c += (l < t);
		r[3] = l;
		if (--n <= 0) {
			break;
		}

		a += 4;
		b += 4;
		r += 4;
	}
	return ((BN_ULONG)c);
}


#endif /* !BN_LLONG */

/* unsigned add of b to a */
int
BN_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max, min, dif;
	BN_ULONG *ap, *bp, *rp, carry, t1, t2;
	const BIGNUM *tmp;

	bn_check_top(a);
	bn_check_top(b);

	if (a->top < b->top) {
		tmp = a;
		a = b;
		b = tmp;
	}
	max = a->top;
	min = b->top;
	dif = max - min;

	if (bn_wexpand(r, max+1) == NULL) {
		return (0);
	}

	r->top = max;


	ap = a->d;
	bp = b->d;
	rp = r->d;

	carry = bn_add_words(rp, ap, bp, min);
	rp += min;
	ap += min;
	bp += min;

	if (carry) {
		while (dif) {
			dif--;
			t1 = *(ap++);
			t2 = (t1 + 1) & BN_MASK2;
			*(rp++) = t2;
			if (t2) {
				carry = 0;
				break;
			}
		}
		if (carry) {
			/* carry != 0 => dif == 0 */
			*rp = 1;
			r->top++;
		}
	}
	if (dif && (rp != ap)) {
		while (dif--) {
			/* copy remaining words if ap != rp */
			*(rp++) = *(ap++);
		}
	}
	r->neg = 0;
	bn_check_top(r);

	return (1);
}


/* unsigned subtraction of b from a, a must be larger than b. */
int
BN_usub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max, min, dif;
	register BN_ULONG t1, t2, *ap, *bp, *rp;
	int i, carry;

	bn_check_top(a);
	bn_check_top(b);

	max = a->top;
	min = b->top;
	dif = max - min;

	if (dif < 0) {
		/* BNerr(BN_F_BN_USUB,BN_R_ARG2_LT_ARG3); */
		return (0);
	}

	if (bn_wexpand(r, max) == NULL) {
		return (0);
	}

	ap = a->d;
	bp = b->d;
	rp = r->d;

#if 1
	carry = 0;
	for (i = min; i != 0; i--) {
		t1 = *(ap++);
		t2 = *(bp++);
		if (carry) {
			carry = (t1 <= t2);
			t1 = (t1-t2-1)&BN_MASK2;
		} else {
			carry = (t1 < t2);
			t1 = (t1-t2) & BN_MASK2;
		}
		*(rp++) = t1 & BN_MASK2;
	}
#else
	carry = bn_sub_words(rp, ap, bp, min);
	ap += min;
	bp += min;
	rp += min;
#endif
	if (carry) { /* subtracted */
		if (!dif) {
			/* error: a < b */
			return (0);
		}
		while (dif) {
			dif--;
			t1 = *(ap++);
			t2 = (t1 -1) & BN_MASK2;
			*(rp++) = t2;
			if (t1) {
				break;
			}
		}
	}
#if 0
	memcpy(rp, ap, sizeof(*rp)*(max-i));
#else
	if (rp != ap) {
		for ( ; ; ) {
			if (!dif--) {
				break;
			}
			rp[0] = ap[0];
			if (!dif--) {
				break;
			}
			rp[1] = ap[1];
			if (!dif--) {
				break;
			}
			rp[2] = ap[2];
			if (!dif--) {
				break;
			}
			rp[3] = ap[3];
			rp += 4;
			ap += 4;
		}
	}
#endif

	r->top = max;
	r->neg = 0;
	bn_correct_top(r);
	return (1);
}


int
BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max;
	int add = 0, neg = 0;
	const BIGNUM *tmp;

	bn_check_top(a);
	bn_check_top(b);

	/*  a -  b	a-b
	 *  a - -b	a+b
	 * -a -  b	-(a+b)
	 * -a - -b	b-a
	 */
	if (a->neg) {
		if (b->neg) {
			tmp = a;
			a = b;
			b = tmp;
		} else {
			add = 1;
			neg = 1;
		}
	} else {
		if (b->neg) {
			add = 1;
			neg = 0;
		}
	}

	if (add) {
		if (!BN_uadd(r, a, b)) {
			return (0);
		}
		r->neg = neg;
		return (1);
	}

	/* We are actually doing a - b :-) */

	max = (a->top > b->top) ? a->top : b->top;
	if (bn_wexpand(r, max) == NULL) {
		return (0);
	}
	if (BN_ucmp(a, b) < 0) {
		if (!BN_usub(r, b, a)) {
			return (0);
		}
		r->neg = 1;
	} else {
		if (!BN_usub(r, a, b)) {
			return (0);
		}
		r->neg = 0;
	}
	bn_check_top(r);

	return (1);
}


/*
 * div
 *
 */
int
BN_div(BIGNUM *dv, BIGNUM *rem, const BIGNUM *m, const BIGNUM *d,
    BN_CTX *ctx)
{
	int i, nm, nd;
	int ret = 0;
	BIGNUM *D;

	bn_check_top(m);
	bn_check_top(d);
	if (BN_is_zero(d)) {
		/* BNerr(BN_F_BN_DIV,BN_R_DIV_BY_ZERO); */
		return (0);
	}

	if (BN_ucmp(m, d) < 0) {
		if (rem != NULL) {
			if (BN_copy(rem, m) == NULL) {
				return (0);
			}
		}
		if (dv != NULL) {
			BN_zero(dv);
		}
		return (1);
	}

	BN_CTX_start(ctx);
	D = BN_CTX_get(ctx);
	if (dv == NULL) {
		dv = BN_CTX_get(ctx);
	}
	if (rem == NULL) {
		rem = BN_CTX_get(ctx);
	}
	if ((D == NULL) || (dv == NULL) || (rem == NULL)) {
		goto end;
	}

	nd = BN_num_bits(d);
	nm = BN_num_bits(m);
	if (BN_copy(D, d) == NULL) {
		goto end;
	}
	if (BN_copy(rem, m) == NULL) {
		goto end;
	}

	/* The next 2 are needed so we can do a dv->d[0]|=1 later
	 * since BN_lshift1 will only work once there is a value :-) */
	BN_zero(dv);
	if (bn_wexpand(dv, 1) == NULL) {
		goto end;
	}
	dv->top = 1;

	if (!BN_lshift(D, D, nm - nd)) {
		goto end;
	}
	for (i = nm-nd; i >= 0; i--) {
		if (!BN_lshift1(dv, dv)) {
			goto end;
		}
		if (BN_ucmp(rem, D) >= 0) {
			dv->d[0] |= 1;
			if (!BN_usub(rem, rem, D)) {
				goto end;
			}
		}
		if (!BN_rshift1(D, D)) {
			goto end;
		}
	}
	rem->neg = BN_is_zero(rem) ? 0 : m->neg;
	dv->neg = m->neg ^ d->neg;
	ret = 1;
end:
	BN_CTX_end(ctx);

	return (ret);
}


/*
 * Mul
 */
void bn_mul_normal(BN_ULONG *r, BN_ULONG *a, int na, BN_ULONG *b, int nb);

#if defined(OPENSSL_NO_ASM) || !defined(OPENSSL_BN_ASM_PART_WORDS)

/* Here follows specialised variants of bn_add_words() and
 * bn_sub_words().  They have the property performing operations on
 * arrays of different sizes.  The sizes of those arrays is expressed through
 * cl, which is the common length ( basicall, min(len(a),len(b)) ), and dl,
 * which is the delta between the two lengths, calculated as len(a)-len(b).
 * All lengths are the number of BN_ULONGs...  For the operations that require
 * a result array as parameter, it must have the length cl+abs(dl).
 * These functions should probably end up in bn_asm.c as soon as there are
 * assembler counterparts for the systems that use assembler files.  */
BN_ULONG
bn_sub_part_words(BN_ULONG *r,
    const BN_ULONG *a, const BN_ULONG *b,
    int cl, int dl)
{
	BN_ULONG c, t;

	assert(cl >= 0);
	c = bn_sub_words(r, a, b, cl);

	if (dl == 0) {
		return (c);
	}

	r += cl;
	a += cl;
	b += cl;

	if (dl < 0) {
#ifdef BN_COUNT
		fprintf(stderr, "  bn_sub_part_words %d + %d (dl < 0, c = %d)\n", cl, dl, c);
#endif
		for ( ; ; ) {
			t = b[0];
			r[0] = (0-t-c)&BN_MASK2;
			if (t != 0) {
				c = 1;
			}
			if (++dl >= 0) {
				break;
			}

			t = b[1];
			r[1] = (0-t-c)&BN_MASK2;
			if (t != 0) {
				c = 1;
			}
			if (++dl >= 0) {
				break;
			}

			t = b[2];
			r[2] = (0-t-c)&BN_MASK2;
			if (t != 0) {
				c = 1;
			}
			if (++dl >= 0) {
				break;
			}

			t = b[3];
			r[3] = (0-t-c)&BN_MASK2;
			if (t != 0) {
				c = 1;
			}
			if (++dl >= 0) {
				break;
			}

			b += 4;
			r += 4;
		}
	}else          {
		int save_dl = dl;
#ifdef BN_COUNT
		fprintf(stderr, "  bn_sub_part_words %d + %d (dl > 0, c = %d)\n", cl, dl, c);
#endif
		while (c) {
			t = a[0];
			r[0] = (t-c)&BN_MASK2;
			if (t != 0) {
				c = 0;
			}
			if (--dl <= 0) {
				break;
			}

			t = a[1];
			r[1] = (t-c)&BN_MASK2;
			if (t != 0) {
				c = 0;
			}
			if (--dl <= 0) {
				break;
			}

			t = a[2];
			r[2] = (t-c)&BN_MASK2;
			if (t != 0) {
				c = 0;
			}
			if (--dl <= 0) {
				break;
			}

			t = a[3];
			r[3] = (t-c)&BN_MASK2;
			if (t != 0) {
				c = 0;
			}
			if (--dl <= 0) {
				break;
			}

			save_dl = dl;
			a += 4;
			r += 4;
		}
		if (dl > 0) {
#ifdef BN_COUNT
			fprintf(stderr, "  bn_sub_part_words %d + %d (dl > 0, c == 0)\n", cl, dl);
#endif
			if (save_dl > dl) {
				switch (save_dl - dl) {
				case 1:
					r[1] = a[1];
					if (--dl <= 0) {
						break;
					}

				case 2:
					r[2] = a[2];
					if (--dl <= 0) {
						break;
					}

				case 3:
					r[3] = a[3];
					if (--dl <= 0) {
						break;
					}
				}
				a += 4;
				r += 4;
			}
		}
		if (dl > 0) {
#ifdef BN_COUNT
			fprintf(stderr, "  bn_sub_part_words %d + %d (dl > 0, copy)\n", cl, dl);
#endif
			for ( ; ; ) {
				r[0] = a[0];
				if (--dl <= 0) {
					break;
				}
				r[1] = a[1];
				if (--dl <= 0) {
					break;
				}
				r[2] = a[2];
				if (--dl <= 0) {
					break;
				}
				r[3] = a[3];
				if (--dl <= 0) {
					break;
				}

				a += 4;
				r += 4;
			}
		}
	}
	return (c);
}


#endif

BN_ULONG bn_add_part_words(BN_ULONG *r,
    const BN_ULONG *a, const BN_ULONG *b,
    int cl, int dl)
{
	BN_ULONG c, l, t;

	assert(cl >= 0);
	c = bn_add_words(r, a, b, cl);

	if (dl == 0) {
		return (c);
	}

	r += cl;
	a += cl;
	b += cl;

	if (dl < 0) {
		int save_dl = dl;
#ifdef BN_COUNT
		fprintf(stderr, "  bn_add_part_words %d + %d (dl < 0, c = %d)\n", cl, dl, c);
#endif
		while (c) {
			l = (c+b[0])&BN_MASK2;
			c = (l < c);
			r[0] = l;
			if (++dl >= 0) {
				break;
			}

			l = (c+b[1])&BN_MASK2;
			c = (l < c);
			r[1] = l;
			if (++dl >= 0) {
				break;
			}

			l = (c+b[2])&BN_MASK2;
			c = (l < c);
			r[2] = l;
			if (++dl >= 0) {
				break;
			}

			l = (c+b[3])&BN_MASK2;
			c = (l < c);
			r[3] = l;
			if (++dl >= 0) {
				break;
			}

			save_dl = dl;
			b += 4;
			r += 4;
		}
		if (dl < 0) {
#ifdef BN_COUNT
			fprintf(stderr, "  bn_add_part_words %d + %d (dl < 0, c == 0)\n", cl, dl);
#endif
			if (save_dl < dl) {
				switch (dl - save_dl) {
				case 1:
					r[1] = b[1];
					if (++dl >= 0) {
						break;
					}

				case 2:
					r[2] = b[2];
					if (++dl >= 0) {
						break;
					}

				case 3:
					r[3] = b[3];
					if (++dl >= 0) {
						break;
					}
				}
				b += 4;
				r += 4;
			}
		}
		if (dl < 0) {
#ifdef BN_COUNT
			fprintf(stderr, "  bn_add_part_words %d + %d (dl < 0, copy)\n", cl, dl);
#endif
			for ( ; ; ) {
				r[0] = b[0];
				if (++dl >= 0) {
					break;
				}
				r[1] = b[1];
				if (++dl >= 0) {
					break;
				}
				r[2] = b[2];
				if (++dl >= 0) {
					break;
				}
				r[3] = b[3];
				if (++dl >= 0) {
					break;
				}

				b += 4;
				r += 4;
			}
		}
	}else          {
		int save_dl = dl;
#ifdef BN_COUNT
		fprintf(stderr, "  bn_add_part_words %d + %d (dl > 0)\n", cl, dl);
#endif
		while (c) {
			t = (a[0]+c)&BN_MASK2;
			c = (t < c);
			r[0] = t;
			if (--dl <= 0) {
				break;
			}

			t = (a[1]+c)&BN_MASK2;
			c = (t < c);
			r[1] = t;
			if (--dl <= 0) {
				break;
			}

			t = (a[2]+c)&BN_MASK2;
			c = (t < c);
			r[2] = t;
			if (--dl <= 0) {
				break;
			}

			t = (a[3]+c)&BN_MASK2;
			c = (t < c);
			r[3] = t;
			if (--dl <= 0) {
				break;
			}

			save_dl = dl;
			a += 4;
			r += 4;
		}
#ifdef BN_COUNT
		fprintf(stderr, "  bn_add_part_words %d + %d (dl > 0, c == 0)\n", cl, dl);
#endif
		if (dl > 0) {
			if (save_dl > dl) {
				switch (save_dl - dl) {
				case 1:
					r[1] = a[1];
					if (--dl <= 0) {
						break;
					}

				case 2:
					r[2] = a[2];
					if (--dl <= 0) {
						break;
					}

				case 3:
					r[3] = a[3];
					if (--dl <= 0) {
						break;
					}
				}
				a += 4;
				r += 4;
			}
		}
		if (dl > 0) {
#ifdef BN_COUNT
			fprintf(stderr, "  bn_add_part_words %d + %d (dl > 0, copy)\n", cl, dl);
#endif
			for ( ; ; ) {
				r[0] = a[0];
				if (--dl <= 0) {
					break;
				}
				r[1] = a[1];
				if (--dl <= 0) {
					break;
				}
				r[2] = a[2];
				if (--dl <= 0) {
					break;
				}
				r[3] = a[3];
				if (--dl <= 0) {
					break;
				}

				a += 4;
				r += 4;
			}
		}
	}
	return (c);
}


#ifdef BN_RECURSION

/* Karatsuba recursive multiplication algorithm
 * (cf. Knuth, The Art of Computer Programming, Vol. 2) */

/* r is 2*n2 words in size,
 * a and b are both n2 words in size.
 * n2 must be a power of 2.
 * We multiply and return the result.
 * t must be 2*n2 words in size
 * We calculate
 * a[0]*b[0]
 * a[0]*b[0]+a[1]*b[1]+(a[0]-a[1])*(b[1]-b[0])
 * a[1]*b[1]
 */
/* dnX may not be positive, but n2/2+dnX has to be */
void bn_mul_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n2,
    int dna, int dnb, BN_ULONG *t)
{
	int n = n2/2, c1, c2;
	int tna = n+dna, tnb = n+dnb;
	unsigned int neg, zero;
	BN_ULONG ln, lo, *p;

# ifdef BN_COUNT
	fprintf(stderr, " bn_mul_recursive %d%+d * %d%+d\n", n2, dna, n2, dnb);
# endif
# ifdef BN_MUL_COMBA
#  if 0
	if (n2 == 4) {
		bn_mul_comba4(r, a, b);
		return;
	}
#  endif

	/* Only call bn_mul_comba 8 if n2 == 8 and the
	 * two arrays are complete [steve]
	 */
	if ((n2 == 8) && (dna == 0) && (dnb == 0)) {
		bn_mul_comba8(r, a, b);
		return;
	}
# endif /* BN_MUL_COMBA */
	/* Else do normal multiply */
	if (n2 < BN_MUL_RECURSIVE_SIZE_NORMAL) {
		bn_mul_normal(r, a, n2+dna, b, n2+dnb);
		if ((dna + dnb) < 0) {
			memset(&r[2*n2 + dna + dnb], 0,
			    sizeof(BN_ULONG) * -(dna + dnb));
		}
		return;
	}
	/* r=(a[0]-a[1])*(b[1]-b[0]) */
	c1 = bn_cmp_part_words(a, &(a[n]), tna, n-tna);
	c2 = bn_cmp_part_words(&(b[n]), b, tnb, tnb-n);
	zero = neg = 0;
	switch (c1*3+c2) {
	case -4:
		bn_sub_part_words(t, &(a[n]), a, tna, tna-n);           /* - */
		bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n-tnb);     /* - */
		break;

	case -3:
		zero = 1;
		break;

	case -2:
		bn_sub_part_words(t, &(a[n]), a, tna, tna-n);           /* - */
		bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb-n);     /* + */
		neg = 1;
		break;

	case -1:
	case 0:
	case 1:
		zero = 1;
		break;

	case 2:
		bn_sub_part_words(t, a, &(a[n]), tna, n-tna);           /* + */
		bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n-tnb);     /* - */
		neg = 1;
		break;

	case 3:
		zero = 1;
		break;

	case 4:
		bn_sub_part_words(t, a, &(a[n]), tna, n-tna);
		bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb-n);
		break;
	}

# ifdef BN_MUL_COMBA
	if ((n == 4) && (dna == 0) && (dnb == 0)) {
		/* XXX: bn_mul_comba4 could take
		 * extra args to do this well */
		if (!zero) {
			bn_mul_comba4(&(t[n2]), t, &(t[n]));
		} else{
			memset(&(t[n2]), 0, 8*sizeof(BN_ULONG));
		}

		bn_mul_comba4(r, a, b);
		bn_mul_comba4(&(r[n2]), &(a[n]), &(b[n]));
	}else if ((n == 8) && (dna == 0) && (dnb == 0))                {
		/* XXX: bn_mul_comba8 could
		 * take extra args to do this
		 * well */
		if (!zero) {
			bn_mul_comba8(&(t[n2]), t, &(t[n]));
		} else{
			memset(&(t[n2]), 0, 16*sizeof(BN_ULONG));
		}

		bn_mul_comba8(r, a, b);
		bn_mul_comba8(&(r[n2]), &(a[n]), &(b[n]));
	}else
# endif /* BN_MUL_COMBA */
	{
		p = &(t[n2*2]);
		if (!zero) {
			bn_mul_recursive(&(t[n2]), t, &(t[n]), n, 0, 0, p);
		} else{
			memset(&(t[n2]), 0, n2*sizeof(BN_ULONG));
		}
		bn_mul_recursive(r, a, b, n, 0, 0, p);
		bn_mul_recursive(&(r[n2]), &(a[n]), &(b[n]), n, dna, dnb, p);
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0]), c1 is the sign
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	if (neg) { /* if t[32] is negative */
		c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));
	}else          {
		/* Might have a carry */
		c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), t, n2));
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0])+(a[0]*b[0])+(a[1]*b[1])
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n+n2]);
		lo = *p;
		ln = (lo+c1)&BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo = *p;
				ln = (lo+1)&BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}


/* n+tn is the word length
 * t needs to be n*4 is size, as does r */
/* tnX may not be negative but less than n */
void
bn_mul_part_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n,
    int tna, int tnb, BN_ULONG *t)
{
	int i, j, n2 = n*2;
	int c1, c2, neg;
	BN_ULONG ln, lo, *p;

# ifdef BN_COUNT
	fprintf(stderr, " bn_mul_part_recursive (%d%+d) * (%d%+d)\n",
	    n, tna, n, tnb);
# endif
	if (n < 8) {
		bn_mul_normal(r, a, n+tna, b, n+tnb);
		return;
	}

	/* r=(a[0]-a[1])*(b[1]-b[0]) */
	c1 = bn_cmp_part_words(a, &(a[n]), tna, n-tna);
	c2 = bn_cmp_part_words(&(b[n]), b, tnb, tnb-n);
	neg = 0;
	switch (c1*3+c2) {
	case -4:
		bn_sub_part_words(t, &(a[n]), a, tna, tna-n);           /* - */
		bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n-tnb);     /* - */
		break;

	case -3:
	/* break; */
	case -2:
		bn_sub_part_words(t, &(a[n]), a, tna, tna-n);           /* - */
		bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb-n);     /* + */
		neg = 1;
		break;

	case -1:
	case 0:
	case 1:
	/* break; */
	case 2:
		bn_sub_part_words(t, a, &(a[n]), tna, n-tna);           /* + */
		bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n-tnb);     /* - */
		neg = 1;
		break;

	case 3:
	/* break; */
	case 4:
		bn_sub_part_words(t, a, &(a[n]), tna, n-tna);
		bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb-n);
		break;
	}

	/* The zero case isn't yet implemented here. The speedup
	 * would probably be negligible. */
# if 0
	if (n == 4) {
		bn_mul_comba4(&(t[n2]), t, &(t[n]));
		bn_mul_comba4(r, a, b);
		bn_mul_normal(&(r[n2]), &(a[n]), tn, &(b[n]), tn);
		memset(&(r[n2+tn*2]), 0, sizeof(BN_ULONG)*(n2-tn*2));
	}else
# endif
	if (n == 8) {
		bn_mul_comba8(&(t[n2]), t, &(t[n]));
		bn_mul_comba8(r, a, b);
		bn_mul_normal(&(r[n2]), &(a[n]), tna, &(b[n]), tnb);
		memset(&(r[n2+tna+tnb]), 0, sizeof(BN_ULONG)*(n2-tna-tnb));
	}else          {
		p = &(t[n2*2]);
		bn_mul_recursive(&(t[n2]), t, &(t[n]), n, 0, 0, p);
		bn_mul_recursive(r, a, b, n, 0, 0, p);
		i = n/2;

		/* If there is only a bottom half to the number,
		 * just do it */
		if (tna > tnb) {
			j = tna - i;
		} else{
			j = tnb - i;
		}
		if (j == 0) {
			bn_mul_recursive(&(r[n2]), &(a[n]), &(b[n]),
			    i, tna-i, tnb-i, p);
			memset(&(r[n2+i*2]), 0, sizeof(BN_ULONG)*(n2-i*2));
		}else if (j > 0)          { /* eg, n == 16, i == 8 and tn == 11 */
			bn_mul_part_recursive(&(r[n2]), &(a[n]), &(b[n]),
			    i, tna-i, tnb-i, p);
			memset(&(r[n2+tna+tnb]), 0,
			    sizeof(BN_ULONG)*(n2-tna-tnb));
		}else                  { /* (j < 0) eg, n == 16, i == 8 and tn == 5 */
			memset(&(r[n2]), 0, sizeof(BN_ULONG)*n2);
			if ((tna < BN_MUL_RECURSIVE_SIZE_NORMAL) &&
			    (tnb < BN_MUL_RECURSIVE_SIZE_NORMAL)) {
				bn_mul_normal(&(r[n2]), &(a[n]), tna, &(b[n]), tnb);
			}else          {
				for ( ; ; ) {
					i /= 2;

					/* these simplified conditions work
					 * exclusively because difference
					 * between tna and tnb is 1 or 0 */
					if ((i < tna) || (i < tnb)) {
						bn_mul_part_recursive(&(r[n2]),
						    &(a[n]), &(b[n]),
						    i, tna-i, tnb-i, p);
						break;
					}else if ((i == tna) || (i == tnb))              {
						bn_mul_recursive(&(r[n2]),
						    &(a[n]), &(b[n]),
						    i, tna-i, tnb-i, p);
						break;
					}
				}
			}
		}
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0]), c1 is the sign
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	if (neg) { /* if t[32] is negative */
		c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));
	}else          {
		/* Might have a carry */
		c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), t, n2));
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0])+(a[0]*b[0])+(a[1]*b[1])
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n+n2]);
		lo = *p;
		ln = (lo+c1)&BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo = *p;
				ln = (lo+1)&BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}


/* a and b must be the same size, which is n2.
 * r needs to be n2 words and t needs to be n2*2
 */
void
bn_mul_low_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n2,
    BN_ULONG *t)
{
	int n = n2/2;

# ifdef BN_COUNT
	fprintf(stderr, " bn_mul_low_recursive %d * %d\n", n2, n2);
# endif

	bn_mul_recursive(r, a, b, n, 0, 0, &(t[0]));
	if (n >= BN_MUL_LOW_RECURSIVE_SIZE_NORMAL) {
		bn_mul_low_recursive(&(t[0]), &(a[0]), &(b[n]), n, &(t[n2]));
		bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
		bn_mul_low_recursive(&(t[0]), &(a[n]), &(b[0]), n, &(t[n2]));
		bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
	}else          {
		bn_mul_low_normal(&(t[0]), &(a[0]), &(b[n]), n);
		bn_mul_low_normal(&(t[n]), &(a[n]), &(b[0]), n);
		bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
		bn_add_words(&(r[n]), &(r[n]), &(t[n]), n);
	}
}


/* a and b must be the same size, which is n2.
 * r needs to be n2 words and t needs to be n2*2
 * l is the low words of the output.
 * t needs to be n2*3
 */
void
bn_mul_high(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, BN_ULONG *l, int n2,
    BN_ULONG *t)
{
	int i, n;
	int c1, c2;
	int neg, oneg, zero;
	BN_ULONG ll, lc, *lp, *mp;

# ifdef BN_COUNT
	fprintf(stderr, " bn_mul_high %d * %d\n", n2, n2);
# endif
	n = n2/2;

	/* Calculate (al-ah)*(bh-bl) */
	neg = zero = 0;
	c1 = bn_cmp_words(&(a[0]), &(a[n]), n);
	c2 = bn_cmp_words(&(b[n]), &(b[0]), n);
	switch (c1*3+c2) {
	case -4:
		bn_sub_words(&(r[0]), &(a[n]), &(a[0]), n);
		bn_sub_words(&(r[n]), &(b[0]), &(b[n]), n);
		break;

	case -3:
		zero = 1;
		break;

	case -2:
		bn_sub_words(&(r[0]), &(a[n]), &(a[0]), n);
		bn_sub_words(&(r[n]), &(b[n]), &(b[0]), n);
		neg = 1;
		break;

	case -1:
	case 0:
	case 1:
		zero = 1;
		break;

	case 2:
		bn_sub_words(&(r[0]), &(a[0]), &(a[n]), n);
		bn_sub_words(&(r[n]), &(b[0]), &(b[n]), n);
		neg = 1;
		break;

	case 3:
		zero = 1;
		break;

	case 4:
		bn_sub_words(&(r[0]), &(a[0]), &(a[n]), n);
		bn_sub_words(&(r[n]), &(b[n]), &(b[0]), n);
		break;
	}

	oneg = neg;
	/* t[10] = (a[0]-a[1])*(b[1]-b[0]) */
	/* r[10] = (a[1]*b[1]) */
# ifdef BN_MUL_COMBA
	if (n == 8) {
		bn_mul_comba8(&(t[0]), &(r[0]), &(r[n]));
		bn_mul_comba8(r, &(a[n]), &(b[n]));
	}else
# endif
	{
		bn_mul_recursive(&(t[0]), &(r[0]), &(r[n]), n, 0, 0, &(t[n2]));
		bn_mul_recursive(r, &(a[n]), &(b[n]), n, 0, 0, &(t[n2]));
	}

	/* s0 == low(al*bl)
	 * s1 == low(ah*bh)+low((al-ah)*(bh-bl))+low(al*bl)+high(al*bl)
	 * We know s0 and s1 so the only unknown is high(al*bl)
	 * high(al*bl) == s1 - low(ah*bh+s0+(al-ah)*(bh-bl))
	 * high(al*bl) == s1 - (r[0]+l[0]+t[0])
	 */
	if (l != NULL) {
		lp = &(t[n2+n]);
		c1 = (int)(bn_add_words(lp, &(r[0]), &(l[0]), n));
	}else          {
		c1 = 0;
		lp = &(r[0]);
	}

	if (neg) {
		neg = (int)(bn_sub_words(&(t[n2]), lp, &(t[0]), n));
	} else{
		bn_add_words(&(t[n2]), lp, &(t[0]), n);
		neg = 0;
	}

	if (l != NULL) {
		bn_sub_words(&(t[n2+n]), &(l[n]), &(t[n2]), n);
	}else          {
		lp = &(t[n2+n]);
		mp = &(t[n2]);
		for (i = 0; i < n; i++) {
			lp[i] = ((~mp[i])+1)&BN_MASK2;
		}
	}

	/* s[0] = low(al*bl)
	 * t[3] = high(al*bl)
	 * t[10] = (a[0]-a[1])*(b[1]-b[0]) neg is the sign
	 * r[10] = (a[1]*b[1])
	 */

	/* R[10] = al*bl
	 * R[21] = al*bl + ah*bh + (a[0]-a[1])*(b[1]-b[0])
	 * R[32] = ah*bh
	 */

	/* R[1]=t[3]+l[0]+r[0](+-)t[0] (have carry/borrow)
	 * R[2]=r[0]+t[3]+r[1](+-)t[1] (have carry/borrow)
	 * R[3]=r[1]+(carry/borrow)
	 */
	if (l != NULL) {
		lp = &(t[n2]);
		c1 = (int)(bn_add_words(lp, &(t[n2+n]), &(l[0]), n));
	}else          {
		lp = &(t[n2+n]);
		c1 = 0;
	}
	c1 += (int)(bn_add_words(&(t[n2]), lp, &(r[0]), n));
	if (oneg) {
		c1 -= (int)(bn_sub_words(&(t[n2]), &(t[n2]), &(t[0]), n));
	} else{
		c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), &(t[0]), n));
	}

	c2 = (int)(bn_add_words(&(r[0]), &(r[0]), &(t[n2+n]), n));
	c2 += (int)(bn_add_words(&(r[0]), &(r[0]), &(r[n]), n));
	if (oneg) {
		c2 -= (int)(bn_sub_words(&(r[0]), &(r[0]), &(t[n]), n));
	} else{
		c2 += (int)(bn_add_words(&(r[0]), &(r[0]), &(t[n]), n));
	}

	if (c1 != 0) { /* Add starting at r[0], could be +ve or -ve */
		i = 0;
		if (c1 > 0) {
			lc = c1;
			do {
				ll = (r[i]+lc)&BN_MASK2;
				r[i++] = ll;
				lc = (lc > ll);
			} while (lc);
		}else          {
			lc = -c1;
			do {
				ll = r[i];
				r[i++] = (ll-lc)&BN_MASK2;
				lc = (lc > ll);
			} while (lc);
		}
	}
	if (c2 != 0) { /* Add starting at r[1] */
		i = n;
		if (c2 > 0) {
			lc = c2;
			do {
				ll = (r[i]+lc)&BN_MASK2;
				r[i++] = ll;
				lc = (lc > ll);
			} while (lc);
		}else          {
			lc = -c2;
			do {
				ll = r[i];
				r[i++] = (ll-lc)&BN_MASK2;
				lc = (lc > ll);
			} while (lc);
		}
	}
}


#endif /* BN_RECURSION */

int
BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
{
	int ret = 0;
	int top, al, bl;
	BIGNUM *rr;

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
	int i;
#endif
#ifdef BN_RECURSION
	BIGNUM *t = NULL;
	int j = 0, k;
#endif

#ifdef BN_COUNT
	fprintf(stderr, "BN_mul %d * %d\n", a->top, b->top);
#endif

	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(r);

	al = a->top;
	bl = b->top;

	if ((al == 0) || (bl == 0)) {
		BN_zero(r);
		return (1);
	}
	top = al + bl;

	BN_CTX_start(ctx);
	if ((r == a) || (r == b)) {
		if ((rr = BN_CTX_get(ctx)) == NULL) {
			goto err;
		}
	} else{
		rr = r;
	}
	rr->neg = a->neg^b->neg;

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
	i = al - bl;
#endif
#ifdef BN_MUL_COMBA
	if (i == 0) {
# if 0
		if (al == 4) {
			if (bn_wexpand(rr, 8) == NULL) {
				goto err;
			}
			rr->top = 8;
			bn_mul_comba4(rr->d, a->d, b->d);
			goto end;
		}
# endif
		if (al == 8) {
			if (bn_wexpand(rr, 16) == NULL) {
				goto err;
			}
			rr->top = 16;
			bn_mul_comba8(rr->d, a->d, b->d);
			goto end;
		}
	}
#endif  /* BN_MUL_COMBA */
#ifdef BN_RECURSION
	if ((al >= BN_MULL_SIZE_NORMAL) && (bl >= BN_MULL_SIZE_NORMAL)) {
		if ((i >= -1) && (i <= 1)) {
			/* Find out the power of two lower or equal
			 * to the longest of the two numbers */
			if (i >= 0) {
				j = BN_num_bits_word((BN_ULONG)al);
			}
			if (i == -1) {
				j = BN_num_bits_word((BN_ULONG)bl);
			}
			j = 1 << (j-1);
			assert(j <= al || j <= bl);
			k = j + j;
			t = BN_CTX_get(ctx);
			if (t == NULL) {
				goto err;
			}
			if ((al > j) || (bl > j)) {
				if (bn_wexpand(t, k*4) == NULL) {
					goto err;
				}
				if (bn_wexpand(rr, k*4) == NULL) {
					goto err;
				}
				bn_mul_part_recursive(rr->d, a->d, b->d,
				    j, al-j, bl-j, t->d);
			} else {
				/* al <= j || bl <= j */
				if (bn_wexpand(t, k * 2) == NULL) {
					goto err;
				}
				if (bn_wexpand(rr, k * 2) == NULL) {
					goto err;
				}
				bn_mul_recursive(rr->d, a->d, b->d,
				    j, al - j, bl - j, t->d);
			}
			rr->top = top;
			goto end;
		}
#if 0
		if ((i == 1) && !BN_get_flags(b, BN_FLG_STATIC_DATA)) {
			BIGNUM *tmp_bn = (BIGNUM *)b;
			if (bn_wexpand(tmp_bn, al) == NULL) {
				goto err;
			}
			tmp_bn->d[bl] = 0;
			bl++;
			i--;
		}else if ((i == -1) && !BN_get_flags(a, BN_FLG_STATIC_DATA))             {
			BIGNUM *tmp_bn = (BIGNUM *)a;
			if (bn_wexpand(tmp_bn, bl) == NULL) {
				goto err;
			}
			tmp_bn->d[al] = 0;
			al++;
			i++;
		}
		if (i == 0) {
			/* symmetric and > 4 */
			/* 16 or larger */
			j = BN_num_bits_word((BN_ULONG)al);
			j = 1<<(j-1);
			k = j+j;
			t = BN_CTX_get(ctx);
			if (al == j) { /* exact multiple */
				if (bn_wexpand(t, k*2) == NULL) {
					goto err;
				}
				if (bn_wexpand(rr, k*2) == NULL) {
					goto err;
				}
				bn_mul_recursive(rr->d, a->d, b->d, al, t->d);
			}else          {
				if (bn_wexpand(t, k*4) == NULL) {
					goto err;
				}
				if (bn_wexpand(rr, k*4) == NULL) {
					goto err;
				}
				bn_mul_part_recursive(rr->d, a->d, b->d, al-j, j, t->d);
			}
			rr->top = top;
			goto end;
		}
#endif
	}
#endif  /* BN_RECURSION */
	if (bn_wexpand(rr, top) == NULL) {
		goto err;
	}
	rr->top = top;
	bn_mul_normal(rr->d, a->d, al, b->d, bl);

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
end:
#endif
	bn_correct_top(rr);
	if (r != rr) {
		BN_copy(r, rr);
	}
	ret = 1;
err:
	bn_check_top(r);
	BN_CTX_end(ctx);
	return (ret);
}


void
bn_mul_normal(BN_ULONG *r, BN_ULONG *a, int na, BN_ULONG *b, int nb)
{
	BN_ULONG *rr;

#ifdef BN_COUNT
	fprintf(stderr, " bn_mul_normal %d * %d\n", na, nb);
#endif

	if (na < nb) {
		int itmp;
		BN_ULONG *ltmp;

		itmp = na;
		na = nb;
		nb = itmp;
		ltmp = a;
		a = b;
		b = ltmp;
	}
	rr = &(r[na]);
	if (nb <= 0) {
		(void)bn_mul_words(r, a, na, 0);
		return;
	} else{
		rr[0] = bn_mul_words(r, a, na, b[0]);
	}

	for ( ; ; ) {
		if (--nb <= 0) {
			return;
		}
		rr[1] = bn_mul_add_words(&(r[1]), a, na, b[1]);
		if (--nb <= 0) {
			return;
		}
		rr[2] = bn_mul_add_words(&(r[2]), a, na, b[2]);
		if (--nb <= 0) {
			return;
		}
		rr[3] = bn_mul_add_words(&(r[3]), a, na, b[3]);
		if (--nb <= 0) {
			return;
		}
		rr[4] = bn_mul_add_words(&(r[4]), a, na, b[4]);
		rr += 4;
		r += 4;
		b += 4;
	}
}


#ifdef RECURSION
void
bn_mul_low_normal(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
{
#ifdef BN_COUNT
	fprintf(stderr, " bn_mul_low_normal %d * %d\n", n, n);
#endif
	bn_mul_words(r, a, n, b[0]);

	for ( ; ; ) {
		if (--n <= 0) {
			return;
		}
		bn_mul_add_words(&(r[1]), a, n, b[1]);
		if (--n <= 0) {
			return;
		}
		bn_mul_add_words(&(r[2]), a, n, b[2]);
		if (--n <= 0) {
			return;
		}
		bn_mul_add_words(&(r[3]), a, n, b[3]);
		if (--n <= 0) {
			return;
		}
		bn_mul_add_words(&(r[4]), a, n, b[4]);
		r += 4;
		b += 4;
	}
}


#endif /* RECURSION */

/*
 * mod
 */
int
BN_mod(BIGNUM *rem, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx)
{
	return (BN_div(NULL, rem, m, d, ctx));
}


int
BN_nnmod(BIGNUM *r, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx)
{
	/* like BN_mod, but returns non-negative remainder
	 * (i.e.,  0 <= r < |d|  always holds) */

	if (!(BN_mod(r, m, d, ctx))) {
		return (0);
	}
	if (!r->neg) {
		return (1);
	}
	/* now   -|d| < r < 0,  so we have to set  r := r + |d| */
	return ((d->neg ? BN_sub : BN_add)(r, r, d));
}


/* slow but works */
int
BN_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m,
    BN_CTX *ctx)
{
	BIGNUM *t;
	int ret = 0;

	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(m);

	BN_CTX_start(ctx);
	if ((t = BN_CTX_get(ctx)) == NULL) {
		goto err;
	}
	if (a == b) {
		if (!BN_sqr(t, a, ctx)) {
			goto err;
		}
	} else {
		if (!BN_mul(t, a, b, ctx)) {
			goto err;
		}
	}
	if (!BN_nnmod(r, t, m, ctx)) {
		goto err;
	}
	bn_check_top(r);
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}


#define EXP_TABLE_SIZE    32

/* The old fallback, simple version :-) */
int
BN_mod_exp_simple(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx)
{
	int i, j, bits, ret = 0, wstart, wend, window, wvalue;
	int start = 1;
	BIGNUM *d;
	/* Table of variables obtained from 'ctx' */
	BIGNUM *val[EXP_TABLE_SIZE];

	if (BN_get_flags(p, BN_FLG_CONSTTIME) != 0) {
		/* BN_FLG_CONSTTIME only supported by BN_mod_exp_mont() */
		/* BNerr(BN_F_BN_MOD_EXP_SIMPLE,ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED); */
		return (-1);
	}

	bits = BN_num_bits(p);

	if (bits == 0) {
		ret = BN_one(r);
		return (ret);
	}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	val[0] = BN_CTX_get(ctx);
	if (!d || !val[0]) {
		goto err;
	}

	if (!BN_nnmod(val[0], a, m, ctx)) {
		goto err;                                       /* 1 */
	}
	if (BN_is_zero(val[0])) {
		BN_zero(r);
		ret = 1;
		goto err;
	}

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1) {
		if (!BN_mod_mul(d, val[0], val[0], m, ctx)) {
			goto err;                               /* 2 */
		}
		j = 1<<(window-1);
		for (i = 1; i < j; i++) {
			if (((val[i] = BN_CTX_get(ctx)) == NULL) ||
			    !BN_mod_mul(val[i], val[i-1], d, m, ctx)) {
				goto err;
			}
		}
	}

	start = 1;              /* This is used to avoid multiplication etc
	                         * when there is only the value '1' in the
	                         * buffer. */
	wvalue = 0;             /* The 'value' of the window */
	wstart = bits - 1;      /* The top bit of the window */
	wend = 0;               /* The bottom bit of the window */

	if (!BN_one(r)) {
		goto err;
	}

	for ( ; ; ) {
		if (BN_is_bit_set(p, wstart) == 0) {
			if (!start) {
				if (!BN_mod_mul(r, r, r, m, ctx)) {
					goto err;
				}
			}
			if (wstart == 0) {
				break;
			}
			wstart--;
			continue;
		}

		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window */
		j = wstart;
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart - i < 0) {
				break;
			}
			if (BN_is_bit_set(p, wstart - i)) {
				wvalue <<= (i - wend);
				wvalue |= 1;
				wend = i;
			}
		}

		/* wend is the size of the current window */
		j = wend + 1;
		/* add the 'bytes above' */
		if (!start) {
			for (i = 0; i < j; i++) {
				if (!BN_mod_mul(r, r, r, m, ctx)) {
					goto err;
				}
			}
		}

		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul(r, r, val[wvalue >> 1], m, ctx)) {
			goto err;
		}

		/* move the 'window' down further */
		wstart -= wend + 1;
		wvalue = 0;
		start = 0;
		if (wstart < 0) {
			break;
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	bn_check_top(r);

	return (ret);
}


int
BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m,
    BN_CTX *ctx)
{
	int ret;

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	/* For even modulus  m = 2^k*m_odd,  it might make sense to compute
	 * a^p mod m_odd  and  a^p mod 2^k  separately (with Montgomery
	 * exponentiation for the odd part), using appropriate exponent
	 * reductions, and combine the results using the CRT.
	 *
	 * For now, we use Montgomery only if the modulus is odd; otherwise,
	 * exponentiation using the reciprocal-based quick remaindering
	 * algorithm is used.
	 *
	 * (Timing obtained with expspeed.c [computations  a^p mod m
	 * where  a, p, m  are of the same length: 256, 512, 1024, 2048,
	 * 4096, 8192 bits], compared to the running time of the
	 * standard algorithm:
	 *
	 *   BN_mod_exp_mont   33 .. 40 %  [AMD K6-2, Linux, debug configuration]
	 *                     55 .. 77 %  [UltraSparc processor, but
	 *                                  debug-solaris-sparcv8-gcc conf.]
	 *
	 *   BN_mod_exp_recp   50 .. 70 %  [AMD K6-2, Linux, debug configuration]
	 *                     62 .. 118 % [UltraSparc, debug-solaris-sparcv8-gcc]
	 *
	 * On the Sparc, BN_mod_exp_recp was faster than BN_mod_exp_mont
	 * at 2048 and more bits, but at 512 and 1024 bits, it was
	 * slower even than the standard algorithm!
	 *
	 * "Real" timings [linux-elf, solaris-sparcv9-gcc configurations]
	 * should be obtained when the new Montgomery reduction code
	 * has been integrated into OpenSSL.)
	 */

#define MONT_MUL_MOD
#define MONT_EXP_WORD

/*
 * #define RECP_MUL_MOD
 */

#ifdef MONT_MUL_MOD

	/* I have finally been able to take out this pre-condition of
	 * the top bit being set.  It was caused by an error in BN_div
	 * with negatives.  There was also another problem when for a^b%m
	 * a >= m.  eay 07-May-97 */
/*	if ((m->d[m->top-1]&BN_TBIT) && BN_is_odd(m)) */

	if (BN_is_odd(m)) {
#  ifdef MONT_EXP_WORD
		if ((a->top == 1) && !a->neg && (BN_get_flags(p, BN_FLG_CONSTTIME) == 0)) {
			BN_ULONG A = a->d[0];
			ret = BN_mod_exp_mont_word(r, A, p, m, ctx, NULL);
		} else
#  endif
		ret = BN_mod_exp_mont(r, a, p, m, ctx, NULL);
	} else
#endif
#ifdef RECP_MUL_MOD
	{
		ret = BN_mod_exp_recp(r, a, p, m, ctx);
	}
#else
	{
		ret = BN_mod_exp_simple(r, a, p, m, ctx);
	}
#endif

	bn_check_top(r);
	return (ret);
}


#ifdef RECP_MUL_MOD
int
BN_mod_exp_recp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx)
{
	int i, j, bits, ret = 0, wstart, wend, window, wvalue;
	int start = 1;
	BIGNUM *aa;
	/* Table of variables obtained from 'ctx' */
	BIGNUM *val[EXP_TABLE_SIZE];
	BN_RECP_CTX recp;

	if (BN_get_flags(p, BN_FLG_CONSTTIME) != 0) {
		/* BN_FLG_CONSTTIME only supported by BN_mod_exp_mont() */
		/* BNerr(BN_F_BN_MOD_EXP_RECP,ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED); */
		return (-1);
	}

	bits = BN_num_bits(p);

	if (bits == 0) {
		ret = BN_one(r);
		return (ret);
	}

	BN_CTX_start(ctx);
	aa = BN_CTX_get(ctx);
	val[0] = BN_CTX_get(ctx);
	if (!aa || !val[0]) {
		goto err;
	}

	BN_RECP_CTX_init(&recp);
	if (m->neg) {
		/* ignore sign of 'm' */
		if (!BN_copy(aa, m)) {
			goto err;
		}
		aa->neg = 0;
		if (BN_RECP_CTX_set(&recp, aa, ctx) <= 0) {
			goto err;
		}
	} else {
		if (BN_RECP_CTX_set(&recp, m, ctx) <= 0) {
			goto err;
		}
	}

	if (!BN_nnmod(val[0], a, m, ctx)) {
		goto err;                                       /* 1 */
	}
	if (BN_is_zero(val[0])) {
		BN_zero(r);
		ret = 1;
		goto err;
	}

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1) {
		if (!BN_mod_mul_reciprocal(aa, val[0], val[0], &recp, ctx)) {
			goto err;                               /* 2 */
		}
		j = 1 << (window - 1);
		for (i = 1; i < j; i++) {
			if (((val[i] = BN_CTX_get(ctx)) == NULL) ||
			    !BN_mod_mul_reciprocal(val[i], val[i-1],
			    aa, &recp, ctx)) {
				goto err;
			}
		}
	}

	start = 1;              /* This is used to avoid multiplication etc
	                         * when there is only the value '1' in the
	                         * buffer. */
	wvalue = 0;             /* The 'value' of the window */
	wstart = bits - 1;      /* The top bit of the window */
	wend = 0;               /* The bottom bit of the window */

	if (!BN_one(r)) {
		goto err;
	}

	for ( ; ; ) {
		if (BN_is_bit_set(p, wstart) == 0) {
			if (!start) {
				if (!BN_mod_mul_reciprocal(r, r, r, &recp, ctx)) {
					goto err;
				}
			}
			if (wstart == 0) {
				break;
			}
			wstart--;
			continue;
		}

		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window */
		j = wstart;
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart - i < 0) {
				break;
			}
			if (BN_is_bit_set(p, wstart-i)) {
				wvalue <<= (i - wend);
				wvalue |= 1;
				wend = i;
			}
		}

		/* wend is the size of the current window */
		j = wend + 1;
		/* add the 'bytes above' */
		if (!start) {
			for (i = 0; i < j; i++) {
				if (!BN_mod_mul_reciprocal(r, r, r, &recp, ctx)) {
					goto err;
				}
			}
		}

		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul_reciprocal(r, r, val[wvalue >> 1], &recp, ctx)) {
			goto err;
		}

		/* move the 'window' down further */
		wstart -= wend + 1;
		wvalue = 0;
		start = 0;
		if (wstart < 0) {
			break;
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	BN_RECP_CTX_free(&recp);
	bn_check_top(r);
	return (ret);
}


#endif /* RECP_MUL_MOD */

int
BN_mod_exp_mont_word(BIGNUM *rr, BN_ULONG a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
	BN_MONT_CTX *mont = NULL;
	int b, bits, ret = 0;
	int r_is_one;
	BN_ULONG w, next_w;
	BIGNUM *d, *r, *t;
	BIGNUM *swap_tmp;

#define BN_MOD_MUL_WORD(r, w, m)		\
	(BN_mul_word(r, (w)) &&			\
	(        /* BN_ucmp(r, (m)) < 0 ? 1 :*/	\
		(BN_mod(t, r, m, ctx) && (swap_tmp = r, r = t, t = swap_tmp, 1))))

	/* BN_MOD_MUL_WORD is only used with 'w' large,
	 * so the BN_ucmp test is probably more overhead
	 * than always using BN_mod (which uses BN_copy if
	 * a similar test returns true). */

	/* We can use BN_mod and do not need BN_nnmod because our
	 * accumulator is never negative (the result of BN_mod does
	 * not depend on the sign of the modulus).
	 */
#define BN_TO_MONTGOMERY_WORD(r, w, mont) \
	(BN_set_word(r, (w)) && BN_to_montgomery(r, r, (mont), ctx))

	if (BN_get_flags(p, BN_FLG_CONSTTIME) != 0) {
		/* BN_FLG_CONSTTIME only supported by BN_mod_exp_mont() */
		/* BNerr(BN_F_BN_MOD_EXP_MONT_WORD,ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED); */
		return (-1);
	}

	bn_check_top(p);
	bn_check_top(m);

	if (!BN_is_odd(m)) {
		/* BNerr(BN_F_BN_MOD_EXP_MONT_WORD,BN_R_CALLED_WITH_EVEN_MODULUS); */
		return (0);
	}
	if (m->top == 1) {
		a %= m->d[0]; /* make sure that 'a' is reduced */
	}
	bits = BN_num_bits(p);
	if (bits == 0) {
		ret = BN_one(rr);
		return (ret);
	}
	if (a == 0) {
		BN_zero(rr);
		ret = 1;
		return (ret);
	}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	t = BN_CTX_get(ctx);
	if ((d == NULL) || (r == NULL) || (t == NULL)) {
		goto err;
	}

	if (in_mont != NULL) {
		mont = in_mont;
	} else{
		if ((mont = BN_MONT_CTX_new()) == NULL) {
			goto err;
		}
		if (!BN_MONT_CTX_set(mont, m, ctx)) {
			goto err;
		}
	}

	r_is_one = 1; /* except for Montgomery factor */

	/* bits-1 >= 0 */

	/* The result is accumulated in the product r*w. */
	w = a; /* bit 'bits-1' of 'p' is always set */
	for (b = bits-2; b >= 0; b--) {
		/* First, square r*w. */
		next_w = w*w;
		if ((next_w/w) != w) {
			/* overflow */
			if (r_is_one) {
				if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) {
					goto err;
				}
				r_is_one = 0;
			} else {
				if (!BN_MOD_MUL_WORD(r, w, m)) {
					goto err;
				}
			}
			next_w = 1;
		}
		w = next_w;
		if (!r_is_one) {
			if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
				goto err;
			}
		}

		/* Second, multiply r*w by 'a' if exponent bit is set. */
		if (BN_is_bit_set(p, b)) {
			next_w = w*a;
			if ((next_w / a) != w) {
				/* overflow */
				if (r_is_one) {
					if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) {
						goto err;
					}
					r_is_one = 0;
				} else {
					if (!BN_MOD_MUL_WORD(r, w, m)) {
						goto err;
					}
				}
				next_w = a;
			}
			w = next_w;
		}
	}

	/* Finally, set r:=r*w. */
	if (w != 1) {
		if (r_is_one) {
			if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) {
				goto err;
			}
			r_is_one = 0;
		} else {
			if (!BN_MOD_MUL_WORD(r, w, m)) {
				goto err;
			}
		}
	}

	if (r_is_one) {
		/* can happen only if a == 1*/
		if (!BN_one(rr)) {
			goto err;
		}
	} else {
		if (!BN_from_montgomery(rr, r, mont, ctx)) {
			goto err;
		}
	}
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) {
		BN_MONT_CTX_free(mont);
	}
	BN_CTX_end(ctx);
	bn_check_top(rr);
	return (ret);
}


int
BN_mod_exp_mont(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
	int i, j, bits, ret = 0, wstart, wend, window, wvalue;
	int start = 1;
	BIGNUM *d, *r;
	const BIGNUM *aa;
	/* Table of variables obtained from 'ctx' */
	BIGNUM *val[EXP_TABLE_SIZE];
	BN_MONT_CTX *mont = NULL;

	if (BN_get_flags(p, BN_FLG_CONSTTIME) != 0) {
		return (BN_mod_exp_mont_consttime(rr, a, p, m, ctx, in_mont));
	}

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	if (!BN_is_odd(m)) {
		/* BNerr(BN_F_BN_MOD_EXP_MONT,BN_R_CALLED_WITH_EVEN_MODULUS); */
		return (0);
	}
	bits = BN_num_bits(p);
	if (bits == 0) {
		ret = BN_one(rr);
		return (ret);
	}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	val[0] = BN_CTX_get(ctx);
	if (!d || !r || !val[0]) {
		goto err;
	}

	/* If this is not done, things will break in the montgomery
	 * part */

	if (in_mont != NULL) {
		mont = in_mont;
	} else{
		if ((mont = BN_MONT_CTX_new()) == NULL) {
			goto err;
		}
		if (!BN_MONT_CTX_set(mont, m, ctx)) {
			goto err;
		}
	}

	if (a->neg || (BN_ucmp(a, m) >= 0)) {
		if (!BN_nnmod(val[0], a, m, ctx)) {
			goto err;
		}
		aa = val[0];
	} else{
		aa = a;
	}
	if (BN_is_zero(aa)) {
		BN_zero(rr);
		ret = 1;
		goto err;
	}
	if (!BN_to_montgomery(val[0], aa, mont, ctx)) {
		goto err;                                    /* 1 */
	}
	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1) {
		if (!BN_mod_mul_montgomery(d, val[0], val[0], mont, ctx)) {
			goto err;                                               /* 2 */
		}
		j = 1<<(window-1);
		for (i = 1; i < j; i++) {
			if (((val[i] = BN_CTX_get(ctx)) == NULL) ||
			    !BN_mod_mul_montgomery(val[i], val[i-1],
			    d, mont, ctx)) {
				goto err;
			}
		}
	}

	start = 1;              /* This is used to avoid multiplication etc
	                         * when there is only the value '1' in the
	                         * buffer. */
	wvalue = 0;             /* The 'value' of the window */
	wstart = bits-1;        /* The top bit of the window */
	wend = 0;               /* The bottom bit of the window */

	if (!BN_to_montgomery(r, BN_value_one(), mont, ctx)) {
		goto err;
	}
	for ( ; ; ) {
		if (BN_is_bit_set(p, wstart) == 0) {
			if (!start) {
				if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
					goto err;
				}
			}
			if (wstart == 0) {
				break;
			}
			wstart--;
			continue;
		}

		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window */
		j = wstart;
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart-i < 0) {
				break;
			}
			if (BN_is_bit_set(p, wstart-i)) {
				wvalue <<= (i-wend);
				wvalue |= 1;
				wend = i;
			}
		}

		/* wend is the size of the current window */
		j = wend + 1;
		/* add the 'bytes above' */
		if (!start) {
			for (i = 0; i < j; i++) {
				if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
					goto err;
				}
			}
		}

		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul_montgomery(r, r, val[wvalue>>1], mont, ctx)) {
			goto err;
		}

		/* move the 'window' down further */
		wstart -= wend + 1;
		wvalue = 0;
		start = 0;
		if (wstart < 0) {
			break;
		}
	}
	if (!BN_from_montgomery(rr, r, mont, ctx)) {
		goto err;
	}
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) {
		BN_MONT_CTX_free(mont);
	}
	BN_CTX_end(ctx);
	bn_check_top(rr);
	return (ret);
}


int
BN_mod_exp2_mont(BIGNUM *rr, const BIGNUM *a1, const BIGNUM *p1,
    const BIGNUM *a2, const BIGNUM *p2, const BIGNUM *m,
    BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
	int i, j, bits, b, bits1, bits2, ret = 0, wpos1, wpos2, window1, window2, wvalue1, wvalue2;
	int r_is_one = 1;
	BIGNUM *d, *r;
	const BIGNUM *a_mod_m;
	/* Tables of variables obtained from 'ctx' */
	BIGNUM *val1[EXP_TABLE_SIZE], *val2[EXP_TABLE_SIZE];
	BN_MONT_CTX *mont = NULL;

	bn_check_top(a1);
	bn_check_top(p1);
	bn_check_top(a2);
	bn_check_top(p2);
	bn_check_top(m);

	if (!(m->d[0] & 1)) {
		/* BNerr(BN_F_BN_MOD_EXP2_MONT,BN_R_CALLED_WITH_EVEN_MODULUS); */
		return (0);
	}
	bits1 = BN_num_bits(p1);
	bits2 = BN_num_bits(p2);
	if ((bits1 == 0) && (bits2 == 0)) {
		ret = BN_one(rr);
		return (ret);
	}

	bits = (bits1 > bits2) ? bits1 : bits2;

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	val1[0] = BN_CTX_get(ctx);
	val2[0] = BN_CTX_get(ctx);
	if (!d || !r || !val1[0] || !val2[0]) {
		goto err;
	}

	if (in_mont != NULL) {
		mont = in_mont;
	} else{
		if ((mont = BN_MONT_CTX_new()) == NULL) {
			goto err;
		}
		if (!BN_MONT_CTX_set(mont, m, ctx)) {
			goto err;
		}
	}

	window1 = BN_window_bits_for_exponent_size(bits1);
	window2 = BN_window_bits_for_exponent_size(bits2);

	/*
	 * Build table for a1:   val1[i] := a1^(2*i + 1) mod m  for i = 0 .. 2^(window1-1)
	 */
	if (a1->neg || (BN_ucmp(a1, m) >= 0)) {
		if (!BN_mod(val1[0], a1, m, ctx)) {
			goto err;
		}
		a_mod_m = val1[0];
	} else{
		a_mod_m = a1;
	}
	if (BN_is_zero(a_mod_m)) {
		BN_zero(rr);
		ret = 1;
		goto err;
	}

	if (!BN_to_montgomery(val1[0], a_mod_m, mont, ctx)) {
		goto err;
	}
	if (window1 > 1) {
		if (!BN_mod_mul_montgomery(d, val1[0], val1[0], mont, ctx)) {
			goto err;
		}

		j = 1<<(window1-1);
		for (i = 1; i < j; i++) {
			if (((val1[i] = BN_CTX_get(ctx)) == NULL) ||
			    !BN_mod_mul_montgomery(val1[i], val1[i-1],
			    d, mont, ctx)) {
				goto err;
			}
		}
	}


	/*
	 * Build table for a2:   val2[i] := a2^(2*i + 1) mod m  for i = 0 .. 2^(window2-1)
	 */
	if (a2->neg || (BN_ucmp(a2, m) >= 0)) {
		if (!BN_mod(val2[0], a2, m, ctx)) {
			goto err;
		}
		a_mod_m = val2[0];
	} else{
		a_mod_m = a2;
	}
	if (BN_is_zero(a_mod_m)) {
		BN_zero(rr);
		ret = 1;
		goto err;
	}
	if (!BN_to_montgomery(val2[0], a_mod_m, mont, ctx)) {
		goto err;
	}
	if (window2 > 1) {
		if (!BN_mod_mul_montgomery(d, val2[0], val2[0], mont, ctx)) {
			goto err;
		}

		j = 1<<(window2-1);
		for (i = 1; i < j; i++) {
			if (((val2[i] = BN_CTX_get(ctx)) == NULL) ||
			    !BN_mod_mul_montgomery(val2[i], val2[i-1],
			    d, mont, ctx)) {
				goto err;
			}
		}
	}


	/* Now compute the power product, using independent windows. */
	r_is_one = 1;
	wvalue1 = 0;    /* The 'value' of the first window */
	wvalue2 = 0;    /* The 'value' of the second window */
	wpos1 = 0;      /* If wvalue1 > 0, the bottom bit of the first window */
	wpos2 = 0;      /* If wvalue2 > 0, the bottom bit of the second window */

	if (!BN_to_montgomery(r, BN_value_one(), mont, ctx)) {
		goto err;
	}
	for (b = bits-1; b >= 0; b--) {
		if (!r_is_one) {
			if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
				goto err;
			}
		}

		if (!wvalue1) {
			if (BN_is_bit_set(p1, b)) {
				/* consider bits b-window1+1 .. b for this window */
				i = b-window1+1;
				while (!BN_is_bit_set(p1, i)) { /* works for i<0 */
					i++;
				}
				wpos1 = i;
				wvalue1 = 1;
				for (i = b-1; i >= wpos1; i--) {
					wvalue1 <<= 1;
					if (BN_is_bit_set(p1, i)) {
						wvalue1++;
					}
				}
			}
		}

		if (!wvalue2) {
			if (BN_is_bit_set(p2, b)) {
				/* consider bits b-window2+1 .. b for this window */
				i = b-window2+1;
				while (!BN_is_bit_set(p2, i)) {
					i++;
				}
				wpos2 = i;
				wvalue2 = 1;
				for (i = b-1; i >= wpos2; i--) {
					wvalue2 <<= 1;
					if (BN_is_bit_set(p2, i)) {
						wvalue2++;
					}
				}
			}
		}

		if (wvalue1 && (b == wpos1)) {
			/* wvalue1 is odd and < 2^window1 */
			if (!BN_mod_mul_montgomery(r, r, val1[wvalue1>>1], mont, ctx)) {
				goto err;
			}
			wvalue1 = 0;
			r_is_one = 0;
		}

		if (wvalue2 && (b == wpos2)) {
			/* wvalue2 is odd and < 2^window2 */
			if (!BN_mod_mul_montgomery(r, r, val2[wvalue2>>1], mont, ctx)) {
				goto err;
			}
			wvalue2 = 0;
			r_is_one = 0;
		}
	}
	if (!BN_from_montgomery(rr, r, mont, ctx)) {
		goto err;
	}
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) {
		BN_MONT_CTX_free(mont);
	}
	BN_CTX_end(ctx);
	bn_check_top(rr);
	return (ret);
}


/* BN_mod_exp_mont_consttime() stores the precomputed powers in a specific layout
 * so that accessing any of these table values shows the same access pattern as far
 * as cache lines are concerned.  The following functions are used to transfer a BIGNUM
 * from/to that table. */

/* BN_mod_exp_mont_conttime is based on the assumption that the
 * L1 data cache line width of the target processor is at least
 * the following value.
 */
#define MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH	(64)
#define MOD_EXP_CTIME_MIN_CACHE_LINE_MASK	(MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH - 1)

#if MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH == 64
#  define BN_window_bits_for_ctime_exponent_size(b) \
	((b) > 937 ? 6 :			    \
	(b) > 306 ? 5 :				    \
	(b) > 89 ? 4 :				    \
	(b) > 22 ? 3 : 1)

#  define BN_MAX_WINDOW_BITS_FOR_CTIME_EXPONENT_SIZE    (6)

#else

#  define BN_window_bits_for_ctime_exponent_size(b) \
	((b) > 306 ? 5 :			    \
	(b) > 89 ? 4 :				    \
	(b) > 22 ? 3 : 1)

#  define BN_MAX_WINDOW_BITS_FOR_CTIME_EXPONENT_SIZE    (5)
#endif /* MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH == 64 */

static int
MOD_EXP_CTIME_COPY_TO_PREBUF(BIGNUM *b, int top, unsigned char *buf, int idx, int width)
{
	size_t i, j;

	if (bn_wexpand(b, top) == NULL) {
		return (0);
	}
	while (b->top < top) {
		b->d[b->top++] = 0;
	}

	for (i = 0, j = idx; i < top * sizeof b->d[0]; i++, j += width) {
		buf[j] = ((unsigned char *)b->d)[i];
	}

	bn_correct_top(b);
	return (1);
}


static int
MOD_EXP_CTIME_COPY_FROM_PREBUF(BIGNUM *b, int top, unsigned char *buf, int idx, int width)
{
	size_t i, j;

	if (bn_wexpand(b, top) == NULL) {
		return (0);
	}

	for (i = 0, j = idx; i < top * sizeof b->d[0]; i++, j += width) {
		((unsigned char *)b->d)[i] = buf[j];
	}

	b->top = top;
	bn_correct_top(b);
	return (1);
}


/* Given a pointer value, compute the next address that is a cache line multiple. */
#define MOD_EXP_CTIME_ALIGN(x_)	\
	((unsigned char *)(x_) + (MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH - (((BN_ULONG)(x_)) & (MOD_EXP_CTIME_MIN_CACHE_LINE_MASK))))

/* This variant of BN_mod_exp_mont() uses fixed windows and the special
 * precomputation memory layout to limit data-dependency to a minimum
 * to protect secret exponents (cf. the hyper-threading timing attacks
 * pointed out by Colin Percival,
 * http://www.daemonology.net/hyperthreading-considered-harmful/)
 */
int
BN_mod_exp_mont_consttime(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
	int i, bits, ret = 0, idx, window, wvalue;
	int top;
	BIGNUM *r;
	const BIGNUM *aa;
	BN_MONT_CTX *mont = NULL;

	int numPowers;
	unsigned char *powerbufFree = NULL;
	int powerbufLen = 0;
	unsigned char *powerbuf = NULL;
	BIGNUM *computeTemp = NULL, *am = NULL;

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	top = m->top;

	if (!(m->d[0] & 1)) {
		/* BNerr(BN_F_BN_MOD_EXP_MONT_CONSTTIME,BN_R_CALLED_WITH_EVEN_MODULUS); */
		return (0);
	}
	bits = BN_num_bits(p);
	if (bits == 0) {
		ret = BN_one(rr);
		return (ret);
	}

	/* Initialize BIGNUM context and allocate intermediate result */
	BN_CTX_start(ctx);
	r = BN_CTX_get(ctx);
	if (r == NULL) {
		goto err;
	}

	/* Allocate a montgomery context if it was not supplied by the caller.
	 * If this is not done, things will break in the montgomery part.
	 */
	if (in_mont != NULL) {
		mont = in_mont;
	} else{
		if ((mont = BN_MONT_CTX_new()) == NULL) {
			goto err;
		}
		if (!BN_MONT_CTX_set(mont, m, ctx)) {
			goto err;
		}
	}

	/* Get the window size to use with size of p. */
	window = BN_window_bits_for_ctime_exponent_size(bits);

	/* Allocate a buffer large enough to hold all of the pre-computed
	 * powers of a.
	 */
	numPowers = 1 << window;
	powerbufLen = sizeof(m->d[0])*top*numPowers;
	if ((powerbufFree = (unsigned char *)malloc(powerbufLen+MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH)) == NULL) {
		goto err;
	}

	powerbuf = MOD_EXP_CTIME_ALIGN(powerbufFree);
	memset(powerbuf, 0, powerbufLen);

	/* Initialize the intermediate result. Do this early to save double conversion,
	 * once each for a^0 and intermediate result.
	 */
	if (!BN_to_montgomery(r, BN_value_one(), mont, ctx)) {
		goto err;
	}
	if (!MOD_EXP_CTIME_COPY_TO_PREBUF(r, top, powerbuf, 0, numPowers)) {
		goto err;
	}

	/* Initialize computeTemp as a^1 with montgomery precalcs */
	computeTemp = BN_CTX_get(ctx);
	am = BN_CTX_get(ctx);
	if ((computeTemp == NULL) || (am == NULL)) {
		goto err;
	}

	if (a->neg || (BN_ucmp(a, m) >= 0)) {
		if (!BN_mod(am, a, m, ctx)) {
			goto err;
		}
		aa = am;
	} else{
		aa = a;
	}
	if (!BN_to_montgomery(am, aa, mont, ctx)) {
		goto err;
	}
	if (!BN_copy(computeTemp, am)) {
		goto err;
	}
	if (!MOD_EXP_CTIME_COPY_TO_PREBUF(am, top, powerbuf, 1, numPowers)) {
		goto err;
	}

	/* If the window size is greater than 1, then calculate
	 * val[i=2..2^winsize-1]. Powers are computed as a*a^(i-1)
	 * (even powers could instead be computed as (a^(i/2))^2
	 * to use the slight performance advantage of sqr over mul).
	 */
	if (window > 1) {
		for (i = 2; i < numPowers; i++) {
			/* Calculate a^i = a^(i-1) * a */
			if (!BN_mod_mul_montgomery(computeTemp, am, computeTemp, mont, ctx)) {
				goto err;
			}
			if (!MOD_EXP_CTIME_COPY_TO_PREBUF(computeTemp, top, powerbuf, i, numPowers)) {
				goto err;
			}
		}
	}

	/* Adjust the number of bits up to a multiple of the window size.
	 * If the exponent length is not a multiple of the window size, then
	 * this pads the most significant bits with zeros to normalize the
	 * scanning loop to there's no special cases.
	 *
	 * * NOTE: Making the window size a power of two less than the native
	 * * word size ensures that the padded bits won't go past the last
	 * * word in the internal BIGNUM structure. Going past the end will
	 * * still produce the correct result, but causes a different branch
	 * * to be taken in the BN_is_bit_set function.
	 */
	bits = ((bits + window - 1) / window) * window;
	idx = bits - 1; /* The top bit of the window */

	/* Scan the exponent one window at a time starting from the most
	 * significant bits.
	 */
	while (idx >= 0) {
		wvalue = 0; /* The 'value' of the window */

		/* Scan the window, squaring the result as we go */
		for (i = 0; i < window; i++, idx--) {
			if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
				goto err;
			}
			wvalue = (wvalue<<1)+BN_is_bit_set(p, idx);
		}

		/* Fetch the appropriate pre-computed value from the pre-buf */
		if (!MOD_EXP_CTIME_COPY_FROM_PREBUF(computeTemp, top, powerbuf, wvalue, numPowers)) {
			goto err;
		}

		/* Multiply the result into the intermediate result */
		if (!BN_mod_mul_montgomery(r, r, computeTemp, mont, ctx)) {
			goto err;
		}
	}

	/* Convert the final result from montgomery to standard format */
	if (!BN_from_montgomery(rr, r, mont, ctx)) {
		goto err;
	}
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) {
		BN_MONT_CTX_free(mont);
	}
	if (powerbuf != NULL) {
		memset(powerbuf, 0, powerbufLen);
		free(powerbufFree);
	}
	if (am != NULL) {
		BN_clear(am);
	}
	if (computeTemp != NULL) {
		BN_clear(computeTemp);
	}
	BN_CTX_end(ctx);
	return (ret);
}


/*
 *  Sqr
 */

static void bn_sqr_normal(BN_ULONG *r, const BN_ULONG *a, int n, BN_ULONG *tmp);

#ifdef BN_RECURSION
static void bn_sqr_recursive(BN_ULONG *r, const BN_ULONG *a, int n2, BN_ULONG *t);

#endif

/* r must not be a */
/* I've just gone over this and it is now %20 faster on x86 - eay - 27 Jun 96 */
int
BN_sqr(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx)
{
	int max, al;
	int ret = 0;
	BIGNUM *tmp, *rr;

#ifdef BN_COUNT
	fprintf(stderr, "BN_sqr %d * %d\n", a->top, a->top);
#endif
	bn_check_top(a);

	al = a->top;
	if (al <= 0) {
		r->top = 0;
		return (1);
	}

	BN_CTX_start(ctx);
	rr = (a != r) ? r : BN_CTX_get(ctx);
	tmp = BN_CTX_get(ctx);
	if (!rr || !tmp) {
		goto err;
	}

	max = 2 * al; /* Non-zero (from above) */
	if (bn_wexpand(rr, max) == NULL) {
		goto err;
	}

	if (al == 4) {
#ifndef BN_SQR_COMBA
		BN_ULONG t[8];
		bn_sqr_normal(rr->d, a->d, 4, t);
#else
		bn_sqr_comba4(rr->d, a->d);
#endif
	} else if (al == 8) {
#ifndef BN_SQR_COMBA
		BN_ULONG t[16];
		bn_sqr_normal(rr->d, a->d, 8, t);
#else
		bn_sqr_comba8(rr->d, a->d);
#endif
	} else {
#if defined(BN_RECURSION)
		if (al < BN_SQR_RECURSIVE_SIZE_NORMAL) {
			BN_ULONG t[BN_SQR_RECURSIVE_SIZE_NORMAL * 2];
			bn_sqr_normal(rr->d, a->d, al, t);
		} else {
			int j, k;

			j = BN_num_bits_word((BN_ULONG)al);
			j = 1 << (j - 1);
			k = j + j;
			if (al == j) {
				if (bn_wexpand(tmp, k*2) == NULL) {
					goto err;
				}
				bn_sqr_recursive(rr->d, a->d, al, tmp->d);
			} else {
				if (bn_wexpand(tmp, max) == NULL) {
					goto err;
				}
				bn_sqr_normal(rr->d, a->d, al, tmp->d);
			}
		}
#else
		if (bn_wexpand(tmp, max) == NULL) {
			goto err;
		}
		bn_sqr_normal(rr->d, a->d, al, tmp->d);
#endif
	}

	rr->neg = 0;

	/* If the most-significant half of the top word of 'a' is zero, then
	 * the square of 'a' will max-1 words. */
	if (a->d[al - 1] == (a->d[al - 1] & BN_MASK2l)) {
		rr->top = max - 1;
	} else{
		rr->top = max;
	}
	if (rr != r) {
		BN_copy(r, rr);
	}
	ret = 1;
err:
	bn_check_top(rr);
	bn_check_top(tmp);
	BN_CTX_end(ctx);
	return (ret);
}


/* tmp must have 2*n words */
static void
bn_sqr_normal(BN_ULONG *r, const BN_ULONG *a, int n, BN_ULONG *tmp)
{
	int i, j, max;
	const BN_ULONG *ap;
	BN_ULONG *rp;

	max = n * 2;
	ap = a;
	rp = r;
	rp[0] = rp[max - 1] = 0;
	rp++;
	j = n;

	if (--j > 0) {
		ap++;
		rp[j] = bn_mul_words(rp, ap, j, ap[-1]);
		rp += 2;
	}

	for (i = n-2; i > 0; i--) {
		j--;
		ap++;
		rp[j] = bn_mul_add_words(rp, ap, j, ap[-1]);
		rp += 2;
	}

	bn_add_words(r, r, r, max);

	/* There will not be a carry */

	bn_sqr_words(tmp, a, n);

	bn_add_words(r, r, tmp, max);
}


#ifdef BN_RECURSION

/* r is 2*n words in size,
 * a and b are both n words in size.    (There's not actually a 'b' here ...)
 * n must be a power of 2.
 * We multiply and return the result.
 * t must be 2*n words in size
 * We calculate
 * a[0]*b[0]
 * a[0]*b[0]+a[1]*b[1]+(a[0]-a[1])*(b[1]-b[0])
 * a[1]*b[1]
 */
static void
bn_sqr_recursive(BN_ULONG *r, const BN_ULONG *a, int n2, BN_ULONG *t)
{
	int n = n2 / 2;
	int zero, c1;
	BN_ULONG ln, lo, *p;

#ifdef BN_COUNT
	fprintf(stderr, " bn_sqr_recursive %d * %d\n", n2, n2);
#endif
	if (n2 == 4) {
#ifndef BN_SQR_COMBA
		bn_sqr_normal(r, a, 4, t);
#else
		bn_sqr_comba4(r, a);
#endif
		return;
	} else if (n2 == 8) {
#ifndef BN_SQR_COMBA
		bn_sqr_normal(r, a, 8, t);
#else
		bn_sqr_comba8(r, a);
#endif
		return;
	}
	if (n2 < BN_SQR_RECURSIVE_SIZE_NORMAL) {
		bn_sqr_normal(r, a, n2, t);
		return;
	}
	/* r=(a[0]-a[1])*(a[1]-a[0]) */
	c1 = bn_cmp_words(a, &(a[n]), n);
	zero = 0;
	if (c1 > 0) {
		bn_sub_words(t, a, &(a[n]), n);
	} else if (c1 < 0) {
		bn_sub_words(t, &(a[n]), a, n);
	} else{
		zero = 1;
	}

	/* The result will always be negative unless it is zero */
	p = &(t[n2 * 2]);

	if (!zero) {
		bn_sqr_recursive(&(t[n2]), t, n, p);
	} else{
		memset(&(t[n2]), 0, n2 * sizeof(BN_ULONG));
	}
	bn_sqr_recursive(r, a, n, p);
	bn_sqr_recursive(&(r[n2]), &(a[n]), n, p);

	/* t[32] holds (a[0]-a[1])*(a[1]-a[0]), it is negative or zero
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	/* t[32] is negative */
	c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));

	/* t[32] holds (a[0]-a[1])*(a[1]-a[0])+(a[0]*a[0])+(a[1]*a[1])
	 * r[10] holds (a[0]*a[0])
	 * r[32] holds (a[1]*a[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n+n2]);
		lo = *p;
		ln = (lo+c1)&BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo = *p;
				ln = (lo+1)&BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}


#endif /* BN_RECURSION */

/*
 * rand
 */
static int
bnrand(int pseudorand, BIGNUM *rnd, int bits, int top, int bottom)
{
	unsigned char *buf = NULL;
	int ret = 0, bit, bytes, mask;
	time_t tim;

	if (bits == 0) {
		BN_zero(rnd);
		return (1);
	}

	bytes = (bits + 7) / 8;
	bit = (bits - 1) % 8;
	mask = 0xff << (bit + 1);

	buf = (unsigned char *)malloc(bytes);
	if (buf == NULL) {
		/* BNerr(BN_F_BNRAND,ERR_R_MALLOC_FAILURE); */
		fprintf(stderr, "malloc() failed\n");
		goto err;
	}

	/* make a random number and set the top and bottom bits */
	time(&tim);
	RAND_add(&tim, sizeof(tim), 0.0);

	if (pseudorand) {
		if (RAND_pseudo_bytes(buf, bytes) == -1) {
			fprintf(stderr, "RAND_pseudo_bytes() failed\n");
			goto err;
		}
	} else {
		if (RAND_bytes(buf, bytes) <= 0) {
			fprintf(stderr, "RAND_bytes() failed\n");
			goto err;
		}
	}

#if 1
	if (pseudorand == 2) {
		/* generate patterns that are more likely to trigger BN
		 * library bugs */
		int i;
		unsigned char c;

		for (i = 0; i < bytes; i++) {
			RAND_pseudo_bytes(&c, 1);
			if ((c >= 128) && (i > 0)) {
				buf[i] = buf[i-1];
			} else if (c < 42) {
				buf[i] = 0;
			} else if (c < 84) {
				buf[i] = 255;
			}
		}
	}
#endif

	if (top != -1) {
		if (top) {
			if (bit == 0) {
				buf[0] = 1;
				buf[1] |= 0x80;
			} else {
				buf[0] |= (3<<(bit-1));
			}
		} else {
			buf[0] |= (1<<bit);
		}
	}
	buf[0] &= ~mask;
	if (bottom) { /* set bottom bit if requested */
		buf[bytes-1] |= 1;
	}
	if (!BN_bin2bn(buf, bytes, rnd)) {
		goto err;
	}
	ret = 1;
err:
	if (buf != NULL) {
		memset(buf, 0, bytes);
		free(buf);
	}
	bn_check_top(rnd);

	return (ret);
}


int
BN_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	return (bnrand(0, rnd, bits, top, bottom));
}


int
BN_pseudo_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	return (bnrand(1, rnd, bits, top, bottom));
}


#if 1
int
BN_bntest_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	return (bnrand(2, rnd, bits, top, bottom));
}


#endif

/* random number r:  0 <= r < range */
static int
bn_rand_range(int pseudo, BIGNUM *r, const BIGNUM *range)
{
	int (*bn_rand)(BIGNUM *, int, int, int) = pseudo ? BN_pseudo_rand : BN_rand;
	int n;
	int count = 100;

	if (range->neg || BN_is_zero(range)) {
		/* BNerr(BN_F_BN_RAND_RANGE, BN_R_INVALID_RANGE); */
		return (0);
	}

	n = BN_num_bits(range); /* n > 0 */

	/* BN_is_bit_set(range, n - 1) always holds */

	if (n == 1) {
		BN_zero(r);
	} else if (!BN_is_bit_set(range, n - 2) && !BN_is_bit_set(range, n - 3)) {
		/* range = 100..._2,
		 * so  3*range (= 11..._2)  is exactly one bit longer than  range */
		do {
			if (!bn_rand(r, n + 1, -1, 0)) {
				return (0);
			}

			/* If  r < 3*range,  use  r := r MOD range
			 * (which is either  r, r - range,  or  r - 2*range).
			 * Otherwise, iterate once more.
			 * Since  3*range = 11..._2, each iteration succeeds with
			 * probability >= .75. */
			if (BN_cmp(r, range) >= 0) {
				if (!BN_sub(r, r, range)) {
					return (0);
				}
				if (BN_cmp(r, range) >= 0) {
					if (!BN_sub(r, r, range)) {
						return (0);
					}
				}
			}

			if (!--count) {
				/* BNerr(BN_F_BN_RAND_RANGE, BN_R_TOO_MANY_ITERATIONS); */
				return (0);
			}
		} while (BN_cmp(r, range) >= 0);
	} else {
		do {
			/* range = 11..._2  or  range = 101..._2 */
			if (!bn_rand(r, n, -1, 0)) {
				return (0);
			}

			if (!--count) {
				/* BNerr(BN_F_BN_RAND_RANGE, BN_R_TOO_MANY_ITERATIONS); */
				return (0);
			}
		} while (BN_cmp(r, range) >= 0);
	}

	bn_check_top(r);

	return (1);
}


int
BN_rand_range(BIGNUM *r, const BIGNUM *range)
{
	return (bn_rand_range(0, r, range));
}


int
BN_pseudo_rand_range(BIGNUM *r, const BIGNUM *range)
{
	return (bn_rand_range(1, r, range));
}


/*
 *  primes
 *
 */

#ifndef EIGHT_BIT
#define NUMPRIMES	2048
typedef unsigned short		prime_t;
#else
#define NUMPRIMES	54
typedef unsigned char		prime_t;
#endif
static const prime_t primes[NUMPRIMES] =
{
	2,	   3,	  5,	 7,    11,    13,    17,    19,
	23,	  29,	 31,	37,    41,    43,    47,    53,
	59,	  61,	 67,	71,    73,    79,    83,    89,
	97,	 101,	103,   107,   109,   113,   127,   131,
	137,	 139,	149,   151,   157,   163,   167,   173,
	179,	 181,	191,   193,   197,   199,   211,   223,
	227,	 229,	233,   239,   241,   251,
#ifndef EIGHT_BIT
	257,	 263,
	269,	 271,	277,   281,   283,   293,   307,   311,
	313,	 317,	331,   337,   347,   349,   353,   359,
	367,	 373,	379,   383,   389,   397,   401,   409,
	419,	 421,	431,   433,   439,   443,   449,   457,
	461,	 463,	467,   479,   487,   491,   499,   503,
	509,	 521,	523,   541,   547,   557,   563,   569,
	571,	 577,	587,   593,   599,   601,   607,   613,
	617,	 619,	631,   641,   643,   647,   653,   659,
	661,	 673,	677,   683,   691,   701,   709,   719,
	727,	 733,	739,   743,   751,   757,   761,   769,
	773,	 787,	797,   809,   811,   821,   823,   827,
	829,	 839,	853,   857,   859,   863,   877,   881,
	883,	 887,	907,   911,   919,   929,   937,   941,
	947,	 953,	967,   971,   977,   983,   991,   997,
	1009,	1013,  1019,  1021,  1031,  1033,  1039,  1049,
	1051,	1061,  1063,  1069,  1087,  1091,  1093,  1097,
	1103,	1109,  1117,  1123,  1129,  1151,  1153,  1163,
	1171,	1181,  1187,  1193,  1201,  1213,  1217,  1223,
	1229,	1231,  1237,  1249,  1259,  1277,  1279,  1283,
	1289,	1291,  1297,  1301,  1303,  1307,  1319,  1321,
	1327,	1361,  1367,  1373,  1381,  1399,  1409,  1423,
	1427,	1429,  1433,  1439,  1447,  1451,  1453,  1459,
	1471,	1481,  1483,  1487,  1489,  1493,  1499,  1511,
	1523,	1531,  1543,  1549,  1553,  1559,  1567,  1571,
	1579,	1583,  1597,  1601,  1607,  1609,  1613,  1619,
	1621,	1627,  1637,  1657,  1663,  1667,  1669,  1693,
	1697,	1699,  1709,  1721,  1723,  1733,  1741,  1747,
	1753,	1759,  1777,  1783,  1787,  1789,  1801,  1811,
	1823,	1831,  1847,  1861,  1867,  1871,  1873,  1877,
	1879,	1889,  1901,  1907,  1913,  1931,  1933,  1949,
	1951,	1973,  1979,  1987,  1993,  1997,  1999,  2003,
	2011,	2017,  2027,  2029,  2039,  2053,  2063,  2069,
	2081,	2083,  2087,  2089,  2099,  2111,  2113,  2129,
	2131,	2137,  2141,  2143,  2153,  2161,  2179,  2203,
	2207,	2213,  2221,  2237,  2239,  2243,  2251,  2267,
	2269,	2273,  2281,  2287,  2293,  2297,  2309,  2311,
	2333,	2339,  2341,  2347,  2351,  2357,  2371,  2377,
	2381,	2383,  2389,  2393,  2399,  2411,  2417,  2423,
	2437,	2441,  2447,  2459,  2467,  2473,  2477,  2503,
	2521,	2531,  2539,  2543,  2549,  2551,  2557,  2579,
	2591,	2593,  2609,  2617,  2621,  2633,  2647,  2657,
	2659,	2663,  2671,  2677,  2683,  2687,  2689,  2693,
	2699,	2707,  2711,  2713,  2719,  2729,  2731,  2741,
	2749,	2753,  2767,  2777,  2789,  2791,  2797,  2801,
	2803,	2819,  2833,  2837,  2843,  2851,  2857,  2861,
	2879,	2887,  2897,  2903,  2909,  2917,  2927,  2939,
	2953,	2957,  2963,  2969,  2971,  2999,  3001,  3011,
	3019,	3023,  3037,  3041,  3049,  3061,  3067,  3079,
	3083,	3089,  3109,  3119,  3121,  3137,  3163,  3167,
	3169,	3181,  3187,  3191,  3203,  3209,  3217,  3221,
	3229,	3251,  3253,  3257,  3259,  3271,  3299,  3301,
	3307,	3313,  3319,  3323,  3329,  3331,  3343,  3347,
	3359,	3361,  3371,  3373,  3389,  3391,  3407,  3413,
	3433,	3449,  3457,  3461,  3463,  3467,  3469,  3491,
	3499,	3511,  3517,  3527,  3529,  3533,  3539,  3541,
	3547,	3557,  3559,  3571,  3581,  3583,  3593,  3607,
	3613,	3617,  3623,  3631,  3637,  3643,  3659,  3671,
	3673,	3677,  3691,  3697,  3701,  3709,  3719,  3727,
	3733,	3739,  3761,  3767,  3769,  3779,  3793,  3797,
	3803,	3821,  3823,  3833,  3847,  3851,  3853,  3863,
	3877,	3881,  3889,  3907,  3911,  3917,  3919,  3923,
	3929,	3931,  3943,  3947,  3967,  3989,  4001,  4003,
	4007,	4013,  4019,  4021,  4027,  4049,  4051,  4057,
	4073,	4079,  4091,  4093,  4099,  4111,  4127,  4129,
	4133,	4139,  4153,  4157,  4159,  4177,  4201,  4211,
	4217,	4219,  4229,  4231,  4241,  4243,  4253,  4259,
	4261,	4271,  4273,  4283,  4289,  4297,  4327,  4337,
	4339,	4349,  4357,  4363,  4373,  4391,  4397,  4409,
	4421,	4423,  4441,  4447,  4451,  4457,  4463,  4481,
	4483,	4493,  4507,  4513,  4517,  4519,  4523,  4547,
	4549,	4561,  4567,  4583,  4591,  4597,  4603,  4621,
	4637,	4639,  4643,  4649,  4651,  4657,  4663,  4673,
	4679,	4691,  4703,  4721,  4723,  4729,  4733,  4751,
	4759,	4783,  4787,  4789,  4793,  4799,  4801,  4813,
	4817,	4831,  4861,  4871,  4877,  4889,  4903,  4909,
	4919,	4931,  4933,  4937,  4943,  4951,  4957,  4967,
	4969,	4973,  4987,  4993,  4999,  5003,  5009,  5011,
	5021,	5023,  5039,  5051,  5059,  5077,  5081,  5087,
	5099,	5101,  5107,  5113,  5119,  5147,  5153,  5167,
	5171,	5179,  5189,  5197,  5209,  5227,  5231,  5233,
	5237,	5261,  5273,  5279,  5281,  5297,  5303,  5309,
	5323,	5333,  5347,  5351,  5381,  5387,  5393,  5399,
	5407,	5413,  5417,  5419,  5431,  5437,  5441,  5443,
	5449,	5471,  5477,  5479,  5483,  5501,  5503,  5507,
	5519,	5521,  5527,  5531,  5557,  5563,  5569,  5573,
	5581,	5591,  5623,  5639,  5641,  5647,  5651,  5653,
	5657,	5659,  5669,  5683,  5689,  5693,  5701,  5711,
	5717,	5737,  5741,  5743,  5749,  5779,  5783,  5791,
	5801,	5807,  5813,  5821,  5827,  5839,  5843,  5849,
	5851,	5857,  5861,  5867,  5869,  5879,  5881,  5897,
	5903,	5923,  5927,  5939,  5953,  5981,  5987,  6007,
	6011,	6029,  6037,  6043,  6047,  6053,  6067,  6073,
	6079,	6089,  6091,  6101,  6113,  6121,  6131,  6133,
	6143,	6151,  6163,  6173,  6197,  6199,  6203,  6211,
	6217,	6221,  6229,  6247,  6257,  6263,  6269,  6271,
	6277,	6287,  6299,  6301,  6311,  6317,  6323,  6329,
	6337,	6343,  6353,  6359,  6361,  6367,  6373,  6379,
	6389,	6397,  6421,  6427,  6449,  6451,  6469,  6473,
	6481,	6491,  6521,  6529,  6547,  6551,  6553,  6563,
	6569,	6571,  6577,  6581,  6599,  6607,  6619,  6637,
	6653,	6659,  6661,  6673,  6679,  6689,  6691,  6701,
	6703,	6709,  6719,  6733,  6737,  6761,  6763,  6779,
	6781,	6791,  6793,  6803,  6823,  6827,  6829,  6833,
	6841,	6857,  6863,  6869,  6871,  6883,  6899,  6907,
	6911,	6917,  6947,  6949,  6959,  6961,  6967,  6971,
	6977,	6983,  6991,  6997,  7001,  7013,  7019,  7027,
	7039,	7043,  7057,  7069,  7079,  7103,  7109,  7121,
	7127,	7129,  7151,  7159,  7177,  7187,  7193,  7207,
	7211,	7213,  7219,  7229,  7237,  7243,  7247,  7253,
	7283,	7297,  7307,  7309,  7321,  7331,  7333,  7349,
	7351,	7369,  7393,  7411,  7417,  7433,  7451,  7457,
	7459,	7477,  7481,  7487,  7489,  7499,  7507,  7517,
	7523,	7529,  7537,  7541,  7547,  7549,  7559,  7561,
	7573,	7577,  7583,  7589,  7591,  7603,  7607,  7621,
	7639,	7643,  7649,  7669,  7673,  7681,  7687,  7691,
	7699,	7703,  7717,  7723,  7727,  7741,  7753,  7757,
	7759,	7789,  7793,  7817,  7823,  7829,  7841,  7853,
	7867,	7873,  7877,  7879,  7883,  7901,  7907,  7919,
	7927,	7933,  7937,  7949,  7951,  7963,  7993,  8009,
	8011,	8017,  8039,  8053,  8059,  8069,  8081,  8087,
	8089,	8093,  8101,  8111,  8117,  8123,  8147,  8161,
	8167,	8171,  8179,  8191,  8209,  8219,  8221,  8231,
	8233,	8237,  8243,  8263,  8269,  8273,  8287,  8291,
	8293,	8297,  8311,  8317,  8329,  8353,  8363,  8369,
	8377,	8387,  8389,  8419,  8423,  8429,  8431,  8443,
	8447,	8461,  8467,  8501,  8513,  8521,  8527,  8537,
	8539,	8543,  8563,  8573,  8581,  8597,  8599,  8609,
	8623,	8627,  8629,  8641,  8647,  8663,  8669,  8677,
	8681,	8689,  8693,  8699,  8707,  8713,  8719,  8731,
	8737,	8741,  8747,  8753,  8761,  8779,  8783,  8803,
	8807,	8819,  8821,  8831,  8837,  8839,  8849,  8861,
	8863,	8867,  8887,  8893,  8923,  8929,  8933,  8941,
	8951,	8963,  8969,  8971,  8999,  9001,  9007,  9011,
	9013,	9029,  9041,  9043,  9049,  9059,  9067,  9091,
	9103,	9109,  9127,  9133,  9137,  9151,  9157,  9161,
	9173,	9181,  9187,  9199,  9203,  9209,  9221,  9227,
	9239,	9241,  9257,  9277,  9281,  9283,  9293,  9311,
	9319,	9323,  9337,  9341,  9343,  9349,  9371,  9377,
	9391,	9397,  9403,  9413,  9419,  9421,  9431,  9433,
	9437,	9439,  9461,  9463,  9467,  9473,  9479,  9491,
	9497,	9511,  9521,  9533,  9539,  9547,  9551,  9587,
	9601,	9613,  9619,  9623,  9629,  9631,  9643,  9649,
	9661,	9677,  9679,  9689,  9697,  9719,  9721,  9733,
	9739,	9743,  9749,  9767,  9769,  9781,  9787,  9791,
	9803,	9811,  9817,  9829,  9833,  9839,  9851,  9857,
	9859,	9871,  9883,  9887,  9901,  9907,  9923,  9929,
	9931,	9941,  9949,  9967,  9973, 10007, 10009, 10037,
	10039, 10061, 10067, 10069, 10079, 10091, 10093, 10099,
	10103, 10111, 10133, 10139, 10141, 10151, 10159, 10163,
	10169, 10177, 10181, 10193, 10211, 10223, 10243, 10247,
	10253, 10259, 10267, 10271, 10273, 10289, 10301, 10303,
	10313, 10321, 10331, 10333, 10337, 10343, 10357, 10369,
	10391, 10399, 10427, 10429, 10433, 10453, 10457, 10459,
	10463, 10477, 10487, 10499, 10501, 10513, 10529, 10531,
	10559, 10567, 10589, 10597, 10601, 10607, 10613, 10627,
	10631, 10639, 10651, 10657, 10663, 10667, 10687, 10691,
	10709, 10711, 10723, 10729, 10733, 10739, 10753, 10771,
	10781, 10789, 10799, 10831, 10837, 10847, 10853, 10859,
	10861, 10867, 10883, 10889, 10891, 10903, 10909, 10937,
	10939, 10949, 10957, 10973, 10979, 10987, 10993, 11003,
	11027, 11047, 11057, 11059, 11069, 11071, 11083, 11087,
	11093, 11113, 11117, 11119, 11131, 11149, 11159, 11161,
	11171, 11173, 11177, 11197, 11213, 11239, 11243, 11251,
	11257, 11261, 11273, 11279, 11287, 11299, 11311, 11317,
	11321, 11329, 11351, 11353, 11369, 11383, 11393, 11399,
	11411, 11423, 11437, 11443, 11447, 11467, 11471, 11483,
	11489, 11491, 11497, 11503, 11519, 11527, 11549, 11551,
	11579, 11587, 11593, 11597, 11617, 11621, 11633, 11657,
	11677, 11681, 11689, 11699, 11701, 11717, 11719, 11731,
	11743, 11777, 11779, 11783, 11789, 11801, 11807, 11813,
	11821, 11827, 11831, 11833, 11839, 11863, 11867, 11887,
	11897, 11903, 11909, 11923, 11927, 11933, 11939, 11941,
	11953, 11959, 11969, 11971, 11981, 11987, 12007, 12011,
	12037, 12041, 12043, 12049, 12071, 12073, 12097, 12101,
	12107, 12109, 12113, 12119, 12143, 12149, 12157, 12161,
	12163, 12197, 12203, 12211, 12227, 12239, 12241, 12251,
	12253, 12263, 12269, 12277, 12281, 12289, 12301, 12323,
	12329, 12343, 12347, 12373, 12377, 12379, 12391, 12401,
	12409, 12413, 12421, 12433, 12437, 12451, 12457, 12473,
	12479, 12487, 12491, 12497, 12503, 12511, 12517, 12527,
	12539, 12541, 12547, 12553, 12569, 12577, 12583, 12589,
	12601, 12611, 12613, 12619, 12637, 12641, 12647, 12653,
	12659, 12671, 12689, 12697, 12703, 12713, 12721, 12739,
	12743, 12757, 12763, 12781, 12791, 12799, 12809, 12821,
	12823, 12829, 12841, 12853, 12889, 12893, 12899, 12907,
	12911, 12917, 12919, 12923, 12941, 12953, 12959, 12967,
	12973, 12979, 12983, 13001, 13003, 13007, 13009, 13033,
	13037, 13043, 13049, 13063, 13093, 13099, 13103, 13109,
	13121, 13127, 13147, 13151, 13159, 13163, 13171, 13177,
	13183, 13187, 13217, 13219, 13229, 13241, 13249, 13259,
	13267, 13291, 13297, 13309, 13313, 13327, 13331, 13337,
	13339, 13367, 13381, 13397, 13399, 13411, 13417, 13421,
	13441, 13451, 13457, 13463, 13469, 13477, 13487, 13499,
	13513, 13523, 13537, 13553, 13567, 13577, 13591, 13597,
	13613, 13619, 13627, 13633, 13649, 13669, 13679, 13681,
	13687, 13691, 13693, 13697, 13709, 13711, 13721, 13723,
	13729, 13751, 13757, 13759, 13763, 13781, 13789, 13799,
	13807, 13829, 13831, 13841, 13859, 13873, 13877, 13879,
	13883, 13901, 13903, 13907, 13913, 13921, 13931, 13933,
	13963, 13967, 13997, 13999, 14009, 14011, 14029, 14033,
	14051, 14057, 14071, 14081, 14083, 14087, 14107, 14143,
	14149, 14153, 14159, 14173, 14177, 14197, 14207, 14221,
	14243, 14249, 14251, 14281, 14293, 14303, 14321, 14323,
	14327, 14341, 14347, 14369, 14387, 14389, 14401, 14407,
	14411, 14419, 14423, 14431, 14437, 14447, 14449, 14461,
	14479, 14489, 14503, 14519, 14533, 14537, 14543, 14549,
	14551, 14557, 14561, 14563, 14591, 14593, 14621, 14627,
	14629, 14633, 14639, 14653, 14657, 14669, 14683, 14699,
	14713, 14717, 14723, 14731, 14737, 14741, 14747, 14753,
	14759, 14767, 14771, 14779, 14783, 14797, 14813, 14821,
	14827, 14831, 14843, 14851, 14867, 14869, 14879, 14887,
	14891, 14897, 14923, 14929, 14939, 14947, 14951, 14957,
	14969, 14983, 15013, 15017, 15031, 15053, 15061, 15073,
	15077, 15083, 15091, 15101, 15107, 15121, 15131, 15137,
	15139, 15149, 15161, 15173, 15187, 15193, 15199, 15217,
	15227, 15233, 15241, 15259, 15263, 15269, 15271, 15277,
	15287, 15289, 15299, 15307, 15313, 15319, 15329, 15331,
	15349, 15359, 15361, 15373, 15377, 15383, 15391, 15401,
	15413, 15427, 15439, 15443, 15451, 15461, 15467, 15473,
	15493, 15497, 15511, 15527, 15541, 15551, 15559, 15569,
	15581, 15583, 15601, 15607, 15619, 15629, 15641, 15643,
	15647, 15649, 15661, 15667, 15671, 15679, 15683, 15727,
	15731, 15733, 15737, 15739, 15749, 15761, 15767, 15773,
	15787, 15791, 15797, 15803, 15809, 15817, 15823, 15859,
	15877, 15881, 15887, 15889, 15901, 15907, 15913, 15919,
	15923, 15937, 15959, 15971, 15973, 15991, 16001, 16007,
	16033, 16057, 16061, 16063, 16067, 16069, 16073, 16087,
	16091, 16097, 16103, 16111, 16127, 16139, 16141, 16183,
	16187, 16189, 16193, 16217, 16223, 16229, 16231, 16249,
	16253, 16267, 16273, 16301, 16319, 16333, 16339, 16349,
	16361, 16363, 16369, 16381, 16411, 16417, 16421, 16427,
	16433, 16447, 16451, 16453, 16477, 16481, 16487, 16493,
	16519, 16529, 16547, 16553, 16561, 16567, 16573, 16603,
	16607, 16619, 16631, 16633, 16649, 16651, 16657, 16661,
	16673, 16691, 16693, 16699, 16703, 16729, 16741, 16747,
	16759, 16763, 16787, 16811, 16823, 16829, 16831, 16843,
	16871, 16879, 16883, 16889, 16901, 16903, 16921, 16927,
	16931, 16937, 16943, 16963, 16979, 16981, 16987, 16993,
	17011, 17021, 17027, 17029, 17033, 17041, 17047, 17053,
	17077, 17093, 17099, 17107, 17117, 17123, 17137, 17159,
	17167, 17183, 17189, 17191, 17203, 17207, 17209, 17231,
	17239, 17257, 17291, 17293, 17299, 17317, 17321, 17327,
	17333, 17341, 17351, 17359, 17377, 17383, 17387, 17389,
	17393, 17401, 17417, 17419, 17431, 17443, 17449, 17467,
	17471, 17477, 17483, 17489, 17491, 17497, 17509, 17519,
	17539, 17551, 17569, 17573, 17579, 17581, 17597, 17599,
	17609, 17623, 17627, 17657, 17659, 17669, 17681, 17683,
	17707, 17713, 17729, 17737, 17747, 17749, 17761, 17783,
	17789, 17791, 17807, 17827, 17837, 17839, 17851, 17863,
#endif
};

static int witness(BIGNUM *w, const BIGNUM *a, const BIGNUM *a1,
    const BIGNUM *a1_odd, int k, BN_CTX *ctx, BN_MONT_CTX *mont);
static int probable_prime(BIGNUM *rnd, int bits);
static int probable_prime_dh(BIGNUM *rnd, int bits,
    const BIGNUM *add, const BIGNUM *rem, BN_CTX *ctx);
static int probable_prime_dh_safe(BIGNUM *rnd, int bits,
    const BIGNUM *add, const BIGNUM *rem, BN_CTX *ctx);

int
BN_generate_prime_ex(BIGNUM *ret, int bits, int safe,
    const BIGNUM *add, const BIGNUM *rem, BN_GENCB *cb)
{
	BIGNUM *t;
	int found = 0;
	int i, j, c1 = 0;
	BN_CTX *ctx;
	int checks = BN_prime_checks_for_size(bits);

	ctx = BN_CTX_new();
	if (ctx == NULL) {
		goto err;
	}
	BN_CTX_start(ctx);
	t = BN_CTX_get(ctx);
	if (!t) {
		goto err;
	}
loop:
	/* make a random number and set the top and bottom bits */
	if (add == NULL) {
		if (!probable_prime(ret, bits)) {
			goto err;
		}
	} else {
		if (safe) {
			if (!probable_prime_dh_safe(ret, bits, add, rem, ctx)) {
				goto err;
			}
		} else {
			if (!probable_prime_dh(ret, bits, add, rem, ctx)) {
				goto err;
			}
		}
	}
	/* if (BN_mod_word(ret,(BN_ULONG)3) == 1) goto loop; */
	if (!BN_GENCB_call(cb, 0, c1++)) {
		/* aborted */
		goto err;
	}

	if (!safe) {
		i = BN_is_prime_fasttest_ex(ret, checks, ctx, 0, cb);
		if (i == -1) {
			goto err;
		}
		if (i == 0) {
			goto loop;
		}
	} else {
		/* for "safe prime" generation,
		 * check that (p-1)/2 is prime.
		 * Since a prime is odd, We just
		 * need to divide by 2 */
		if (!BN_rshift1(t, ret)) {
			goto err;
		}

		for (i = 0; i < checks; i++) {
			j = BN_is_prime_fasttest_ex(ret, 1, ctx, 0, cb);
			if (j == -1) {
				goto err;
			}
			if (j == 0) {
				goto loop;
			}

			j = BN_is_prime_fasttest_ex(t, 1, ctx, 0, cb);
			if (j == -1) {
				goto err;
			}
			if (j == 0) {
				goto loop;
			}

			if (!BN_GENCB_call(cb, 2, c1 - 1)) {
				goto err;
			}
			/* We have a safe prime test pass */
		}
	}
	/* we have a prime :-) */
	found = 1;
err:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	bn_check_top(ret);

	return (found);
}


int
BN_is_prime_ex(const BIGNUM *a, int checks, BN_CTX *ctx_passed, BN_GENCB *cb)
{
	return (BN_is_prime_fasttest_ex(a, checks, ctx_passed, 0, cb));
}


int
BN_is_prime_fasttest_ex(const BIGNUM *a, int checks, BN_CTX *ctx_passed,
    int do_trial_division, BN_GENCB *cb)
{
	int i, j, ret = -1;
	int k;
	BN_CTX *ctx = NULL;
	BIGNUM *A1, *A1_odd, *check; /* taken from ctx */
	BN_MONT_CTX *mont = NULL;
	const BIGNUM *A = NULL;

	if (BN_cmp(a, BN_value_one()) <= 0) {
		return (0);
	}

	if (checks == BN_prime_checks) {
		checks = BN_prime_checks_for_size(BN_num_bits(a));
	}

	/* first look for small factors */
	if (!BN_is_odd(a)) {
		/* a is even => a is prime if and only if a == 2 */
		return (BN_is_word(a, 2));
	}
	if (do_trial_division) {
		for (i = 1; i < NUMPRIMES; i++) {
			if (BN_mod_word(a, primes[i]) == 0) {
				return (0);
			}
		}
		if (!BN_GENCB_call(cb, 1, -1)) {
			goto err;
		}
	}

	if (ctx_passed != NULL) {
		ctx = ctx_passed;
	} else if ((ctx = BN_CTX_new()) == NULL) {
		goto err;
	}
	BN_CTX_start(ctx);

	/* A := abs(a) */
	if (a->neg) {
		BIGNUM *t;
		if ((t = BN_CTX_get(ctx)) == NULL) {
			goto err;
		}
		BN_copy(t, a);
		t->neg = 0;
		A = t;
	} else{
		A = a;
	}
	A1 = BN_CTX_get(ctx);
	A1_odd = BN_CTX_get(ctx);
	check = BN_CTX_get(ctx);
	if (check == NULL) {
		goto err;
	}

	/* compute A1 := A - 1 */
	if (!BN_copy(A1, A)) {
		goto err;
	}
	if (!BN_sub_word(A1, 1)) {
		goto err;
	}
	if (BN_is_zero(A1)) {
		ret = 0;
		goto err;
	}

	/* write  A1  as  A1_odd * 2^k */
	k = 1;
	while (!BN_is_bit_set(A1, k)) {
		k++;
	}
	if (!BN_rshift(A1_odd, A1, k)) {
		goto err;
	}

	/* Montgomery setup for computations mod A */
	mont = BN_MONT_CTX_new();
	if (mont == NULL) {
		goto err;
	}
	if (!BN_MONT_CTX_set(mont, A, ctx)) {
		goto err;
	}

	for (i = 0; i < checks; i++) {
		if (!BN_pseudo_rand_range(check, A1)) {
			goto err;
		}
		if (!BN_add_word(check, 1)) {
			goto err;
		}
		/* now 1 <= check < A */

		j = witness(check, A, A1, A1_odd, k, ctx, mont);
		if (j == -1) {
			goto err;
		}
		if (j) {
			ret = 0;
			goto err;
		}
		if (!BN_GENCB_call(cb, 1, i)) {
			goto err;
		}
	}
	ret = 1;
err:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		if (ctx_passed == NULL) {
			BN_CTX_free(ctx);
		}
	}
	if (mont != NULL) {
		BN_MONT_CTX_free(mont);
	}

	return (ret);
}


static int
witness(BIGNUM *w, const BIGNUM *a, const BIGNUM *a1,
    const BIGNUM *a1_odd, int k, BN_CTX *ctx, BN_MONT_CTX *mont)
{
#if 0
	if (!BN_mod_exp(w, w, a1_odd, a, ctx)) { /* w := w^a1_odd mod a */
		return (-1);
	}
#else
	if (!BN_mod_exp_mont(w, w, a1_odd, a, ctx, mont)) { /* w := w^a1_odd mod a */
		return (-1);
	}
#endif
	if (BN_is_one(w)) {
		return (0); /* probably prime */
	}
	if (BN_cmp(w, a1) == 0) {
		return (0); /* w == -1 (mod a),  'a' is probably prime */
	}
	while (--k) {
		if (!BN_mod_mul(w, w, w, a, ctx)) { /* w := w^2 mod a */
			return (-1);
		}
		if (BN_is_one(w)) {
			return (1); /* 'a' is composite, otherwise a previous 'w' would
		                     * have been == -1 (mod 'a') */
		}
		if (BN_cmp(w, a1) == 0) {
			return (0); /* w == -1 (mod a), 'a' is probably prime */
		}
	}

	/* If we get here, 'w' is the (a-1)/2-th power of the original 'w',
	 * and it is neither -1 nor +1 -- so 'a' cannot be prime */
	bn_check_top(w);
	return (1);
}


static int
probable_prime(BIGNUM *rnd, int bits)
{
	int i;
	prime_t mods[NUMPRIMES];
	BN_ULONG delta, maxdelta;

again:
	if (!BN_rand(rnd, bits, 1, 1)) {
		return (0);
	}
	/* we now have a random number 'rand' to test. */
	for (i = 1; i < NUMPRIMES; i++) {
		mods[i] = (prime_t)BN_mod_word(rnd, (BN_ULONG)primes[i]);
	}
	maxdelta = BN_MASK2 - primes[NUMPRIMES-1];
	delta = 0;
loop: for (i = 1; i < NUMPRIMES; i++) {
		/* check that rnd is not a prime and also
		 * that gcd(rnd-1,primes) == 1 (except for 2) */
		if (((mods[i]+delta)%primes[i]) <= 1) {
			delta += 2;
			if (delta > maxdelta) {
				goto again;
			}
			goto loop;
		}
	}
	if (!BN_add_word(rnd, delta)) {
		return (0);
	}
	bn_check_top(rnd);
	return (1);
}


static int probable_prime_dh(BIGNUM *rnd, int bits,
    const BIGNUM *add, const BIGNUM *rem, BN_CTX *ctx)
{
	int i, ret = 0;
	BIGNUM *t1;

	BN_CTX_start(ctx);
	if ((t1 = BN_CTX_get(ctx)) == NULL) {
		goto err;
	}

	if (!BN_rand(rnd, bits, 0, 1)) {
		goto err;
	}

	/* we need ((rnd-rem) % add) == 0 */

	if (!BN_mod(t1, rnd, add, ctx)) {
		goto err;
	}
	if (!BN_sub(rnd, rnd, t1)) {
		goto err;
	}
	if (rem == NULL) {
		if (!BN_add_word(rnd, 1))           {
			goto err;
		}
	}else                                                             {
		if (!BN_add(rnd, rnd, rem))                                                            {
			goto err;
		}
	}

	/* we now have a random number 'rand' to test. */

loop: for (i = 1; i < NUMPRIMES; i++) {
		/* check that rnd is a prime */
		if (BN_mod_word(rnd, (BN_ULONG)primes[i]) <= 1) {
			if (!BN_add(rnd, rnd, add)) {
				goto err;
			}
			goto loop;
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	bn_check_top(rnd);
	return (ret);
}


static int
probable_prime_dh_safe(BIGNUM *p, int bits, const BIGNUM *padd,
    const BIGNUM *rem, BN_CTX *ctx)
{
	int i, ret = 0;
	BIGNUM *t1, *qadd, *q;

	bits--;
	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	q = BN_CTX_get(ctx);
	qadd = BN_CTX_get(ctx);
	if (qadd == NULL) {
		goto err;
	}

	if (!BN_rshift1(qadd, padd)) {
		goto err;
	}

	if (!BN_rand(q, bits, 0, 1)) {
		goto err;
	}

	/* we need ((rnd-rem) % add) == 0 */
	if (!BN_mod(t1, q, qadd, ctx)) {
		goto err;
	}
	if (!BN_sub(q, q, t1)) {
		goto err;
	}
	if (rem == NULL) {
		if (!BN_add_word(q, 1)) {
			goto err;
		}
	} else {
		if (!BN_rshift1(t1, rem)) {
			goto err;
		}
		if (!BN_add(q, q, t1)) {
			goto err;
		}
	}

	/* we now have a random number 'rand' to test. */
	if (!BN_lshift1(p, q)) {
		goto err;
	}
	if (!BN_add_word(p, 1)) {
		goto err;
	}

loop: for (i = 1; i < NUMPRIMES; i++) {
		/* check that p and q are prime */

		/* check that for p and q
		 * gcd(p-1,primes) == 1 (except for 2) */
		if ((BN_mod_word(p, (BN_ULONG)primes[i]) == 0) ||
		    (BN_mod_word(q, (BN_ULONG)primes[i]) == 0)) {
			if (!BN_add(p, p, padd)) {
				goto err;
			}
			if (!BN_add(q, q, qadd)) {
				goto err;
			}
			goto loop;
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	bn_check_top(p);

	return (ret);
}


/*
 *  GCD
 */
static BIGNUM *euclid(BIGNUM *a, BIGNUM *b);

int
BN_gcd(BIGNUM *r, const BIGNUM *in_a, const BIGNUM *in_b, BN_CTX *ctx)
{
	BIGNUM *a, *b, *t;
	int ret = 0;

	bn_check_top(in_a);
	bn_check_top(in_b);

	BN_CTX_start(ctx);
	a = BN_CTX_get(ctx);
	b = BN_CTX_get(ctx);
	if ((a == NULL) || (b == NULL)) {
		goto err;
	}

	if (BN_copy(a, in_a) == NULL) {
		goto err;
	}
	if (BN_copy(b, in_b) == NULL) {
		goto err;
	}
	a->neg = 0;
	b->neg = 0;

	if (BN_cmp(a, b) < 0) {
		t = a;
		a = b;
		b = t;
	}
	t = euclid(a, b);
	if (t == NULL) {
		goto err;
	}

	if (BN_copy(r, t) == NULL) {
		goto err;
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	bn_check_top(r);
	return (ret);
}


static BIGNUM *
euclid(BIGNUM *a, BIGNUM *b)
{
	BIGNUM *t;
	int shifts = 0;

	bn_check_top(a);
	bn_check_top(b);

	/* 0 <= b <= a */
	while (!BN_is_zero(b)) {
		/* 0 < b <= a */

		if (BN_is_odd(a)) {
			if (BN_is_odd(b)) {
				if (!BN_sub(a, a, b)) {
					goto err;
				}
				if (!BN_rshift1(a, a)) {
					goto err;
				}
				if (BN_cmp(a, b) < 0) {
					t = a;
					a = b;
					b = t;
				}
			} else {
				/* a odd - b even */
				if (!BN_rshift1(b, b)) {
					goto err;
				}
				if (BN_cmp(a, b) < 0) {
					t = a;
					a = b;
					b = t;
				}
			}
		} else {
			/* a is even */
			if (BN_is_odd(b)) {
				if (!BN_rshift1(a, a)) {
					goto err;
				}
				if (BN_cmp(a, b) < 0) {
					t = a;
					a = b;
					b = t;
				}
			} else {
				/* a even - b even */
				if (!BN_rshift1(a, a)) {
					goto err;
				}
				if (!BN_rshift1(b, b)) {
					goto err;
				}
				shifts++;
			}
		}
		/* 0 <= b <= a */
	}

	if (shifts) {
		if (!BN_lshift(a, a, shifts)) {
			goto err;
		}
	}
	bn_check_top(a);

	return (a);

err:
	return (NULL);
}


/* solves ax == 1 (mod n) */
static BIGNUM *BN_mod_inverse_no_branch(BIGNUM *in,
    const BIGNUM *a, const BIGNUM *n, BN_CTX *ctx);

BIGNUM *
BN_mod_inverse(BIGNUM *in, const BIGNUM *a, const BIGNUM *n,
    BN_CTX *ctx)
{
	BIGNUM *A, *B, *X, *Y, *M, *D, *T, *R = NULL;
	BIGNUM *ret = NULL;
	int sign;

	if ((BN_get_flags(a, BN_FLG_CONSTTIME) != 0) || (BN_get_flags(n, BN_FLG_CONSTTIME) != 0)) {
		return (BN_mod_inverse_no_branch(in, a, n, ctx));
	}

	bn_check_top(a);
	bn_check_top(n);

	BN_CTX_start(ctx);
	A = BN_CTX_get(ctx);
	B = BN_CTX_get(ctx);
	X = BN_CTX_get(ctx);
	D = BN_CTX_get(ctx);
	M = BN_CTX_get(ctx);
	Y = BN_CTX_get(ctx);
	T = BN_CTX_get(ctx);
	if (T == NULL) {
		goto err;
	}

	if (in == NULL) {
		R = BN_new();
	} else{
		R = in;
	}
	if (R == NULL) {
		goto err;
	}

	BN_one(X);
	BN_zero(Y);
	if (BN_copy(B, a) == NULL) {
		goto err;
	}
	if (BN_copy(A, n) == NULL) {
		goto err;
	}
	A->neg = 0;
	if (B->neg || (BN_ucmp(B, A) >= 0)) {
		if (!BN_nnmod(B, B, A, ctx)) {
			goto err;
		}
	}
	sign = -1;

	/* From  B = a mod |n|,  A = |n|  it follows that
	 *
	 *      0 <= B < A,
	 *     -sign*X*a  ==  B   (mod |n|),
	 *      sign*Y*a  ==  A   (mod |n|).
	 */

	if (BN_is_odd(n) && (BN_num_bits(n) <= ((BN_BITS <= 32) ? 450 : 2048))) {
		/* Binary inversion algorithm; requires odd modulus.
		 * This is faster than the general algorithm if the modulus
		 * is sufficiently small (about 400 .. 500 bits on 32-bit
		 * sytems, but much more on 64-bit systems) */
		int shift;

		while (!BN_is_zero(B)) {
			/*
			 *      0 < B < |n|,
			 *      0 < A <= |n|,
			 * (1) -sign*X*a  ==  B   (mod |n|),
			 * (2)  sign*Y*a  ==  A   (mod |n|)
			 */

			/* Now divide  B  by the maximum possible power of two in the integers,
			 * and divide  X  by the same value mod |n|.
			 * When we're done, (1) still holds. */
			shift = 0;
			while (!BN_is_bit_set(B, shift)) {
				/* note that 0 < B */
				shift++;

				if (BN_is_odd(X)) {
					if (!BN_uadd(X, X, n)) {
						goto err;
					}
				}
				/* now X is even, so we can easily divide it by two */
				if (!BN_rshift1(X, X)) {
					goto err;
				}
			}
			if (shift > 0) {
				if (!BN_rshift(B, B, shift)) {
					goto err;
				}
			}


			/* Same for  A  and  Y.  Afterwards, (2) still holds. */
			shift = 0;
			while (!BN_is_bit_set(A, shift)) {
				/* note that 0 < A */
				shift++;

				if (BN_is_odd(Y)) {
					if (!BN_uadd(Y, Y, n)) {
						goto err;
					}
				}
				/* now Y is even */
				if (!BN_rshift1(Y, Y)) {
					goto err;
				}
			}
			if (shift > 0) {
				if (!BN_rshift(A, A, shift)) {
					goto err;
				}
			}


			/* We still have (1) and (2).
			 * Both  A  and  B  are odd.
			 * The following computations ensure that
			 *
			 *     0 <= B < |n|,
			 *      0 < A < |n|,
			 * (1) -sign*X*a  ==  B   (mod |n|),
			 * (2)  sign*Y*a  ==  A   (mod |n|),
			 *
			 * and that either  A  or  B  is even in the next iteration.
			 */
			if (BN_ucmp(B, A) >= 0) {
				/* -sign*(X + Y)*a == B - A  (mod |n|) */
				if (!BN_uadd(X, X, Y)) {
					goto err;
				}

				/* NB: we could use BN_mod_add_quick(X, X, Y, n), but that
				 * actually makes the algorithm slower */
				if (!BN_usub(B, B, A)) {
					goto err;
				}
			} else {
				/*  sign*(X + Y)*a == A - B  (mod |n|) */
				if (!BN_uadd(Y, Y, X)) {
					goto err;
				}
				/* as above, BN_mod_add_quick(Y, Y, X, n) would slow things down */
				if (!BN_usub(A, A, B)) {
					goto err;
				}
			}
		}
	} else {
		/* general inversion algorithm */

		while (!BN_is_zero(B)) {
			BIGNUM *tmp;

			/*
			 *      0 < B < A,
			 * (*) -sign*X*a  ==  B   (mod |n|),
			 *      sign*Y*a  ==  A   (mod |n|)
			 */

			/* (D, M) := (A/B, A%B) ... */
			if (BN_num_bits(A) == BN_num_bits(B)) {
				if (!BN_one(D)) {
					goto err;
				}
				if (!BN_sub(M, A, B)) {
					goto err;
				}
			} else if (BN_num_bits(A) == BN_num_bits(B) + 1) {
				/* A/B is 1, 2, or 3 */
				if (!BN_lshift1(T, B)) {
					goto err;
				}
				if (BN_ucmp(A, T) < 0) {
					/* A < 2*B, so D=1 */
					if (!BN_one(D)) {
						goto err;
					}
					if (!BN_sub(M, A, B)) {
						goto err;
					}
				} else {
					/* A >= 2*B, so D=2 or D=3 */
					if (!BN_sub(M, A, T)) {
						goto err;
					}
					if (!BN_add(D, T, B)) {
						goto err;             /* use D (:= 3*B) as temp */
					}
					if (BN_ucmp(A, D) < 0) {
						/* A < 3*B, so D=2 */
						if (!BN_set_word(D, 2)) {
							goto err;
						}
						/* M (= A - 2*B) already has the correct value */
					} else {
						/* only D=3 remains */
						if (!BN_set_word(D, 3)) {
							goto err;
						}
						/* currently  M = A - 2*B,  but we need  M = A - 3*B */
						if (!BN_sub(M, M, B)) {
							goto err;
						}
					}
				}
			} else {
				if (!BN_div(D, M, A, B, ctx)) {
					goto err;
				}
			}

			/* Now
			 *      A = D*B + M;
			 * thus we have
			 * (**)  sign*Y*a  ==  D*B + M   (mod |n|).
			 */

			tmp = A; /* keep the BIGNUM object, the value does not matter */

			/* (A, B) := (B, A mod B) ... */
			A = B;
			B = M;
			/* ... so we have  0 <= B < A  again */

			/* Since the former  M  is now  B  and the former  B  is now  A,
			 * (**) translates into
			 *       sign*Y*a  ==  D*A + B    (mod |n|),
			 * i.e.
			 *       sign*Y*a - D*A  ==  B    (mod |n|).
			 * Similarly, (*) translates into
			 *      -sign*X*a  ==  A          (mod |n|).
			 *
			 * Thus,
			 *   sign*Y*a + D*sign*X*a  ==  B  (mod |n|),
			 * i.e.
			 *        sign*(Y + D*X)*a  ==  B  (mod |n|).
			 *
			 * So if we set  (X, Y, sign) := (Y + D*X, X, -sign),  we arrive back at
			 *      -sign*X*a  ==  B   (mod |n|),
			 *       sign*Y*a  ==  A   (mod |n|).
			 * Note that  X  and  Y  stay non-negative all the time.
			 */

			/* most of the time D is very small, so we can optimize tmp := D*X+Y */
			if (BN_is_one(D)) {
				if (!BN_add(tmp, X, Y)) {
					goto err;
				}
			} else {
				if (BN_is_word(D, 2)) {
					if (!BN_lshift1(tmp, X)) {
						goto err;
					}
				} else if (BN_is_word(D, 4)) {
					if (!BN_lshift(tmp, X, 2)) {
						goto err;
					}
				} else if (D->top == 1) {
					if (!BN_copy(tmp, X)) {
						goto err;
					}
					if (!BN_mul_word(tmp, D->d[0])) {
						goto err;
					}
				} else {
					if (!BN_mul(tmp, D, X, ctx)) {
						goto err;
					}
				}
				if (!BN_add(tmp, tmp, Y)) {
					goto err;
				}
			}

			M = Y; /* keep the BIGNUM object, the value does not matter */
			Y = X;
			X = tmp;
			sign = -sign;
		}
	}

	/*
	 * The while loop (Euclid's algorithm) ends when
	 *      A == gcd(a,n);
	 * we have
	 *       sign*Y*a  ==  A  (mod |n|),
	 * where  Y  is non-negative.
	 */

	if (sign < 0) {
		if (!BN_sub(Y, n, Y)) {
			goto err;
		}
	}
	/* Now  Y*a  ==  A  (mod |n|).  */


	if (BN_is_one(A)) {
		/* Y*a == 1  (mod |n|) */
		if (!Y->neg && (BN_ucmp(Y, n) < 0)) {
			if (!BN_copy(R, Y)) {
				goto err;
			}
		} else {
			if (!BN_nnmod(R, Y, n, ctx)) {
				goto err;
			}
		}
	} else {
		/* BNerr(BN_F_BN_MOD_INVERSE,BN_R_NO_INVERSE); */
		goto err;
	}
	ret = R;
err:
	if ((ret == NULL) && (in == NULL)) {
		BN_free(R);
	}
	BN_CTX_end(ctx);
	bn_check_top(ret);
	return (ret);
}


/* BN_mod_inverse_no_branch is a special version of BN_mod_inverse.
 * It does not contain branches that may leak sensitive information.
 */
static BIGNUM *
BN_mod_inverse_no_branch(BIGNUM *in,
    const BIGNUM *a, const BIGNUM *n, BN_CTX *ctx)
{
	BIGNUM *A, *B, *X, *Y, *M, *D, *T, *R = NULL;
	BIGNUM local_A, local_B;
	BIGNUM *pA, *pB;
	BIGNUM *ret = NULL;
	int sign;

	bn_check_top(a);
	bn_check_top(n);

	BN_CTX_start(ctx);
	A = BN_CTX_get(ctx);
	B = BN_CTX_get(ctx);
	X = BN_CTX_get(ctx);
	D = BN_CTX_get(ctx);
	M = BN_CTX_get(ctx);
	Y = BN_CTX_get(ctx);
	T = BN_CTX_get(ctx);
	if (T == NULL) {
		goto err;
	}

	if (in == NULL) {
		R = BN_new();
	} else{
		R = in;
	}
	if (R == NULL) {
		goto err;
	}

	BN_one(X);
	BN_zero(Y);
	if (BN_copy(B, a) == NULL) {
		goto err;
	}
	if (BN_copy(A, n) == NULL) {
		goto err;
	}
	A->neg = 0;

	if (B->neg || (BN_ucmp(B, A) >= 0)) {
		/* Turn BN_FLG_CONSTTIME flag on, so that when BN_div is invoked,
		 * BN_div_no_branch will be called eventually.
		 */
		pB = &local_B;
		BN_with_flags(pB, B, BN_FLG_CONSTTIME);
		if (!BN_nnmod(B, pB, A, ctx)) {
			goto err;
		}
	}
	sign = -1;

	/* From  B = a mod |n|,  A = |n|  it follows that
	 *
	 *      0 <= B < A,
	 *     -sign*X*a  ==  B   (mod |n|),
	 *      sign*Y*a  ==  A   (mod |n|).
	 */

	while (!BN_is_zero(B)) {
		BIGNUM *tmp;

		/*
		 *      0 < B < A,
		 * (*) -sign*X*a  ==  B   (mod |n|),
		 *      sign*Y*a  ==  A   (mod |n|)
		 */

		/* Turn BN_FLG_CONSTTIME flag on, so that when BN_div is invoked,
		 * BN_div_no_branch will be called eventually.
		 */
		pA = &local_A;
		BN_with_flags(pA, A, BN_FLG_CONSTTIME);

		/* (D, M) := (A/B, A%B) ... */
		if (!BN_div(D, M, pA, B, ctx)) {
			goto err;
		}

		/* Now
		 *      A = D*B + M;
		 * thus we have
		 * (**)  sign*Y*a  ==  D*B + M   (mod |n|).
		 */

		tmp = A; /* keep the BIGNUM object, the value does not matter */

		/* (A, B) := (B, A mod B) ... */
		A = B;
		B = M;
		/* ... so we have  0 <= B < A  again */

		/* Since the former  M  is now  B  and the former  B  is now  A,
		 * (**) translates into
		 *       sign*Y*a  ==  D*A + B    (mod |n|),
		 * i.e.
		 *       sign*Y*a - D*A  ==  B    (mod |n|).
		 * Similarly, (*) translates into
		 *      -sign*X*a  ==  A          (mod |n|).
		 *
		 * Thus,
		 *   sign*Y*a + D*sign*X*a  ==  B  (mod |n|),
		 * i.e.
		 *        sign*(Y + D*X)*a  ==  B  (mod |n|).
		 *
		 * So if we set  (X, Y, sign) := (Y + D*X, X, -sign),  we arrive back at
		 *      -sign*X*a  ==  B   (mod |n|),
		 *       sign*Y*a  ==  A   (mod |n|).
		 * Note that  X  and  Y  stay non-negative all the time.
		 */

		if (!BN_mul(tmp, D, X, ctx)) {
			goto err;
		}
		if (!BN_add(tmp, tmp, Y)) {
			goto err;
		}

		M = Y; /* keep the BIGNUM object, the value does not matter */
		Y = X;
		X = tmp;
		sign = -sign;
	}

	/*
	 * The while loop (Euclid's algorithm) ends when
	 *      A == gcd(a,n);
	 * we have
	 *       sign*Y*a  ==  A  (mod |n|),
	 * where  Y  is non-negative.
	 */

	if (sign < 0) {
		if (!BN_sub(Y, n, Y)) {
			goto err;
		}
	}
	/* Now  Y*a  ==  A  (mod |n|).  */

	if (BN_is_one(A)) {
		/* Y*a == 1  (mod |n|) */
		if (!Y->neg && (BN_ucmp(Y, n) < 0)) {
			if (!BN_copy(R, Y)) {
				goto err;
			}
		} else {
			if (!BN_nnmod(R, Y, n, ctx)) {
				goto err;
			}
		}
	} else {
		/* BNerr(BN_F_BN_MOD_INVERSE_NO_BRANCH,BN_R_NO_INVERSE); */
		goto err;
	}
	ret = R;
err:
	if ((ret == NULL) && (in == NULL)) {
		BN_free(R);
	}
	BN_CTX_end(ctx);
	bn_check_top(ret);
	return (ret);
}


/*
 * mont
 */

/*
 * Details about Montgomery multiplication algorithms can be found at
 * http://security.ece.orst.edu/publications.html, e.g.
 * http://security.ece.orst.edu/koc/papers/j37acmon.pdf and
 * sections 3.8 and 4.2 in http://security.ece.orst.edu/koc/papers/r01rsasw.pdf
 */

#define MONT_WORD    /* use the faster word-based algorithm */

#if defined(MONT_WORD) && defined(OPENSSL_BN_ASM_MONT) && (BN_BITS2 <= 32)

/* This condition means we have a specific non-default build:
 * In the 0.9.8 branch, OPENSSL_BN_ASM_MONT is normally not set for any
 * BN_BITS2<=32 platform; an explicit "enable-montasm" is required.
 * I.e., if we are here, the user intentionally deviates from the
 * normal stable build to get better Montgomery performance from
 * the 0.9.9-dev backport.
 *
 * In this case only, we also enable BN_from_montgomery_word()
 * (another non-stable feature from 0.9.9-dev).
 */
#define MONT_FROM_WORD___NON_DEFAULT_0_9_8_BUILD
#endif

#ifdef MONT_FROM_WORD___NON_DEFAULT_0_9_8_BUILD
static int BN_from_montgomery_word(BIGNUM *ret, BIGNUM *r, BN_MONT_CTX *mont);

#endif

int
BN_mod_mul_montgomery(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
    BN_MONT_CTX *mont, BN_CTX *ctx)
{
	BIGNUM *tmp;
	int ret = 0;

#if defined(OPENSSL_BN_ASM_MONT) && defined(MONT_WORD)
	int num = mont->N.top;

	if ((num > 1) && (a->top == num) && (b->top == num)) {
		if (bn_wexpand(r, num) == NULL) {
			return (0);
		}
		if (bn_mul_mont(r->d, a->d, b->d, mont->N.d, &mont->n0, num)) {
			r->neg = a->neg^b->neg;
			r->top = num;
			bn_correct_top(r);
			return (1);
		}
	}
#endif

	BN_CTX_start(ctx);
	tmp = BN_CTX_get(ctx);
	if (tmp == NULL) {
		goto err;
	}

	bn_check_top(tmp);
	if (a == b) {
		if (!BN_sqr(tmp, a, ctx)) {
			goto err;
		}
	} else {
		if (!BN_mul(tmp, a, b, ctx)) {
			goto err;
		}
	}
	/* reduce from aRR to aR */
#ifdef MONT_FROM_WORD___NON_DEFAULT_0_9_8_BUILD
	if (!BN_from_montgomery_word(r, tmp, mont)) {
		goto err;
	}
#else
	if (!BN_from_montgomery(r, tmp, mont, ctx)) {
		goto err;
	}
#endif
	bn_check_top(r);
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}


#ifdef MONT_FROM_WORD___NON_DEFAULT_0_9_8_BUILD
static int
BN_from_montgomery_word(BIGNUM *ret, BIGNUM *r, BN_MONT_CTX *mont)
{
	BIGNUM *n;
	BN_ULONG *ap, *np, *rp, n0, v, *nrp;
	int al, nl, max, i, x, ri;

	n = &(mont->N);

	/* mont->ri is the size of mont->N in bits (rounded up
	 * to the word size) */
	al = ri = mont->ri / BN_BITS2;

	nl = n->top;
	if ((al == 0) || (nl == 0)) {
		ret->top = 0;
		return (1);
	}

	max = (nl+al+1); /* allow for overflow (no?) XXX */
	if (bn_wexpand(r, max) == NULL) {
		return (0);
	}

	r->neg ^= n->neg;
	np = n->d;
	rp = r->d;
	nrp = &(r->d[nl]);

	/* clear the top words of T */
	for (i = r->top; i < max; i++) { /* memset? XXX */
		r->d[i] = 0;
	}

	r->top = max;
	n0 = mont->n0;

#ifdef BN_COUNT
	fprintf(stderr, "word BN_from_montgomery_word %d * %d\n", nl, nl);
#endif
	for (i = 0; i < nl; i++) {
#ifdef __TANDEM
		{
			long long t1;
			long long t2;
			long long t3;
			t1 = rp[0] * (n0 & 0177777);
			t2 = 037777600000l;
			t2 = n0 & t2;
			t3 = rp[0] & 0177777;
			t2 = (t3 * t2) & BN_MASK2;
			t1 = t1 + t2;
			v = bn_mul_add_words(rp, np, nl, (BN_ULONG)t1);
		}
#else
		v = bn_mul_add_words(rp, np, nl, (rp[0]*n0) & BN_MASK2);
#endif
		nrp++;
		rp++;
		if (((nrp[-1] += v)&BN_MASK2) >= v) {
			continue;
		} else{
			if (((++nrp[0])&BN_MASK2) != 0) {
				continue;
			}
			if (((++nrp[1])&BN_MASK2) != 0) {
				continue;
			}
			for (x = 2; (((++nrp[x])&BN_MASK2) == 0); x++) {
			}
		}
	}
	bn_correct_top(r);

	/* mont->ri will be a multiple of the word size and below code
	 * is kind of BN_rshift(ret,r,mont->ri) equivalent */
	if (r->top <= ri) {
		ret->top = 0;
		return (1);
	}
	al = r->top-ri;

	if (bn_wexpand(ret, ri) == NULL) {
		return (0);
	}
	x = 0 - (((al-ri) >> (sizeof(al) * 8 - 1)) & 1);
	ret->top = x = (ri & ~x) | (al & x);    /* min(ri,al) */
	ret->neg = r->neg;

	rp = ret->d;
	ap = &(r->d[ri]);

	{
		size_t m1, m2;

		v = bn_sub_words(rp, ap, np, ri);

		/* this ----------------^^ works even in al<ri case
		 * thanks to zealous zeroing of top of the vector in the
		 * beginning. */

		/* if (al==ri && !v) || al>ri) nrp=rp; else nrp=ap; */

		/* in other words if subtraction result is real, then
		 * trick unconditional memcpy below to perform in-place
		 * "refresh" instead of actual copy. */
		m1 = 0-(size_t)(((al-ri)>>(sizeof(al)*8-1))&1); /* al<ri */
		m2 = 0-(size_t)(((ri-al)>>(sizeof(al)*8-1))&1); /* al>ri */
		m1 |= m2;                                       /* (al!=ri) */
		m1 |= (0-(size_t)v);                            /* (al!=ri || v) */
		m1 &= ~m2;                                      /* (al!=ri || v) && !al>ri */
		nrp = (BN_ULONG *)(((size_t)rp&~m1)|((size_t)ap&m1));
	}

	/* 'i<ri' is chosen to eliminate dependency on input data, even
	 * though it results in redundant copy in al<ri case. */
	for (i = 0, ri -= 4; i < ri; i += 4) {
		BN_ULONG t1, t2, t3, t4;

		t1 = nrp[i+0];
		t2 = nrp[i+1];
		t3 = nrp[i+2];
		ap[i+0] = 0;
		t4 = nrp[i+3];
		ap[i+1] = 0;
		rp[i+0] = t1;
		ap[i+2] = 0;
		rp[i+1] = t2;
		ap[i+3] = 0;
		rp[i+2] = t3;
		rp[i+3] = t4;
	}
	for (ri += 4; i < ri; i++) {
		rp[i] = nrp[i], ap[i] = 0;
	}
	bn_correct_top(r);
	bn_correct_top(ret);
	bn_check_top(ret);

	return (1);
}


int
BN_from_montgomery(BIGNUM *ret, const BIGNUM *a, BN_MONT_CTX *mont,
    BN_CTX *ctx)
{
	int retn = 0;
	BIGNUM *t;

	BN_CTX_start(ctx);
	if ((t = BN_CTX_get(ctx)) && BN_copy(t, a)) {
		retn = BN_from_montgomery_word(ret, t, mont);
	}
	BN_CTX_end(ctx);
	return (retn);
}


#else /* !MONT_FROM_WORD___NON_DEFAULT_0_9_8_BUILD */

int
BN_from_montgomery(BIGNUM *ret, const BIGNUM *a, BN_MONT_CTX *mont,
    BN_CTX *ctx)
{
	int retn = 0;

#ifdef MONT_WORD
	BIGNUM *n, *r;
	BN_ULONG *ap, *np, *rp, n0, v, *nrp;
	int al, nl, max, i, x, ri;

	BN_CTX_start(ctx);
	if ((r = BN_CTX_get(ctx)) == NULL) {
		goto err;
	}

	if (!BN_copy(r, a)) {
		goto err;
	}
	n = &(mont->N);

	ap = a->d;

	/* mont->ri is the size of mont->N in bits (rounded up
	 * to the word size) */
	al = ri = mont->ri/BN_BITS2;

	nl = n->top;
	if ((al == 0) || (nl == 0)) {
		r->top = 0;
		return (1);
	}

	max = (nl+al+1); /* allow for overflow (no?) XXX */
	if (bn_wexpand(r, max) == NULL) {
		goto err;
	}

	r->neg = a->neg^n->neg;
	np = n->d;
	rp = r->d;
	nrp = &(r->d[nl]);

	/* clear the top words of T */
#if 1
	for (i = r->top; i < max; i++) { /* memset? XXX */
		r->d[i] = 0;
	}
#else
	memset(&(r->d[r->top]), 0, (max-r->top)*sizeof(BN_ULONG));
#endif

	r->top = max;
	n0 = mont->n0;

#ifdef BN_COUNT
	fprintf(stderr, "word BN_from_montgomery %d * %d\n", nl, nl);
#endif
	for (i = 0; i < nl; i++) {
#ifdef __TANDEM
		{
			long long t1;
			long long t2;
			long long t3;
			t1 = rp[0] * (n0 & 0177777);
			t2 = 037777600000l;
			t2 = n0 & t2;
			t3 = rp[0] & 0177777;
			t2 = (t3 * t2) & BN_MASK2;
			t1 = t1 + t2;
			v = bn_mul_add_words(rp, np, nl, (BN_ULONG)t1);
		}
#else
		v = bn_mul_add_words(rp, np, nl, (rp[0]*n0)&BN_MASK2);
#endif
		nrp++;
		rp++;
		if (((nrp[-1] += v)&BN_MASK2) >= v) {
			continue;
		} else{
			if (((++nrp[0])&BN_MASK2) != 0) {
				continue;
			}
			if (((++nrp[1])&BN_MASK2) != 0) {
				continue;
			}
			for (x = 2; (((++nrp[x])&BN_MASK2) == 0); x++) {
			}
		}
	}
	bn_correct_top(r);

	/* mont->ri will be a multiple of the word size and below code
	 * is kind of BN_rshift(ret,r,mont->ri) equivalent */
	if (r->top <= ri) {
		ret->top = 0;
		retn = 1;
		goto err;
	}
	al = r->top-ri;

# define BRANCH_FREE    1
# if BRANCH_FREE
	if (bn_wexpand(ret, ri) == NULL) {
		goto err;
	}
	x = 0-(((al-ri)>>(sizeof(al)*8-1))&1);
	ret->top = x = (ri&~x)|(al&x);      /* min(ri,al) */
	ret->neg = r->neg;

	rp = ret->d;
	ap = &(r->d[ri]);

	{
		size_t m1, m2;

		v = bn_sub_words(rp, ap, np, ri);

		/* this ----------------^^ works even in al<ri case
		 * thanks to zealous zeroing of top of the vector in the
		 * beginning. */

		/* if (al==ri && !v) || al>ri) nrp=rp; else nrp=ap; */

		/* in other words if subtraction result is real, then
		 * trick unconditional memcpy below to perform in-place
		 * "refresh" instead of actual copy. */
		m1 = 0-(size_t)(((al-ri)>>(sizeof(al)*8-1))&1); /* al<ri */
		m2 = 0-(size_t)(((ri-al)>>(sizeof(al)*8-1))&1); /* al>ri */
		m1 |= m2;                                       /* (al!=ri) */
		m1 |= (0-(size_t)v);                            /* (al!=ri || v) */
		m1 &= ~m2;                                      /* (al!=ri || v) && !al>ri */
		nrp = (BN_ULONG *)(((size_t)rp&~m1)|((size_t)ap&m1));
	}

	/* 'i<ri' is chosen to eliminate dependency on input data, even
	 * though it results in redundant copy in al<ri case. */
	for (i = 0, ri -= 4; i < ri; i += 4) {
		BN_ULONG t1, t2, t3, t4;

		t1 = nrp[i+0];
		t2 = nrp[i+1];
		t3 = nrp[i+2];
		ap[i+0] = 0;
		t4 = nrp[i+3];
		ap[i+1] = 0;
		rp[i+0] = t1;
		ap[i+2] = 0;
		rp[i+1] = t2;
		ap[i+3] = 0;
		rp[i+2] = t3;
		rp[i+3] = t4;
	}
	for (ri += 4; i < ri; i++) {
		rp[i] = nrp[i], ap[i] = 0;
	}
	bn_correct_top(r);
	bn_correct_top(ret);
# else
	if (bn_wexpand(ret, al) == NULL) {
		goto err;
	}
	ret->top = al;
	ret->neg = r->neg;

	rp = ret->d;
	ap = &(r->d[ri]);
	al -= 4;
	for (i = 0; i < al; i += 4) {
		BN_ULONG t1, t2, t3, t4;

		t1 = ap[i+0];
		t2 = ap[i+1];
		t3 = ap[i+2];
		t4 = ap[i+3];
		rp[i+0] = t1;
		rp[i+1] = t2;
		rp[i+2] = t3;
		rp[i+3] = t4;
	}
	al += 4;
	for ( ; i < al; i++) {
		rp[i] = ap[i];
	}
# endif
#else   /* !MONT_WORD */
	BIGNUM *t1, *t2;

	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	t2 = BN_CTX_get(ctx);
	if ((t1 == NULL) || (t2 == NULL)) {
		goto err;
	}

	if (!BN_copy(t1, a)) {
		goto err;
	}
	BN_mask_bits(t1, mont->ri);

	if (!BN_mul(t2, t1, &mont->Ni, ctx)) {
		goto err;
	}
	BN_mask_bits(t2, mont->ri);

	if (!BN_mul(t1, t2, &mont->N, ctx)) {
		goto err;
	}
	if (!BN_add(t2, a, t1)) {
		goto err;
	}
	if (!BN_rshift(ret, t2, mont->ri)) {
		goto err;
	}
#endif  /* MONT_WORD */

#if !defined(BRANCH_FREE) || BRANCH_FREE == 0
	if (BN_ucmp(ret, &(mont->N)) >= 0) {
		if (!BN_usub(ret, ret, &(mont->N))) {
			goto err;
		}
	}
#endif
	retn = 1;
	bn_check_top(ret);
err:
	BN_CTX_end(ctx);
	return (retn);
}


#endif /* MONT_FROM_WORD___NON_DEFAULT_0_9_8_BUILD */

BN_MONT_CTX *
BN_MONT_CTX_new(void)
{
	BN_MONT_CTX *ret;

	if ((ret = (BN_MONT_CTX *)malloc(sizeof(BN_MONT_CTX))) == NULL) {
		return (NULL);
	}

	BN_MONT_CTX_init(ret);
	ret->flags = BN_FLG_MALLOCED;
	return (ret);
}


void
BN_MONT_CTX_init(BN_MONT_CTX *ctx)
{
	ctx->ri = 0;
	BN_init(&(ctx->RR));
	BN_init(&(ctx->N));
	BN_init(&(ctx->Ni));
#if 0   /* for OpenSSL 0.9.9 mont->n0 */
	ctx->n0[0] = ctx->n0[1] = 0;
#else
	ctx->n0 = 0;
#endif
	ctx->flags = 0;
}


void
BN_MONT_CTX_free(BN_MONT_CTX *mont)
{
	if (mont == NULL) {
		return;
	}

	BN_free(&(mont->RR));
	BN_free(&(mont->N));
	BN_free(&(mont->Ni));
	if (mont->flags & BN_FLG_MALLOCED) {
		free(mont);
	}
}


int
BN_MONT_CTX_set(BN_MONT_CTX *mont, const BIGNUM *mod, BN_CTX *ctx)
{
	int ret = 0;
	BIGNUM *Ri, *R;

	BN_CTX_start(ctx);
	if ((Ri = BN_CTX_get(ctx)) == NULL) {
		goto err;
	}
	R = &(mont->RR);                                /* grab RR as a temp */
	if (!BN_copy(&(mont->N), mod)) {
		goto err;                               /* Set N */
	}
	mont->N.neg = 0;

#ifdef MONT_WORD
	{
		BIGNUM tmod;
		BN_ULONG buf[2];

		mont->ri = (BN_num_bits(mod)+(BN_BITS2-1))/BN_BITS2*BN_BITS2;
		BN_zero(R);
		if (!(BN_set_bit(R, BN_BITS2))) {
			goto err;       /* R */
		}
		buf[0] = mod->d[0];     /* tmod = N mod word size */
		buf[1] = 0;

		BN_init(&tmod);
		tmod.d = buf;
		tmod.top = buf[0] != 0 ? 1 : 0;
		tmod.dmax = 2;
		tmod.neg = 0;

		/* Ri = R^-1 mod N*/
		if ((BN_mod_inverse(Ri, R, &tmod, ctx)) == NULL) {
			goto err;
		}
		if (!BN_lshift(Ri, Ri, BN_BITS2)) {
			goto err;                         /* R*Ri */
		}
		if (!BN_is_zero(Ri)) {
			if (!BN_sub_word(Ri, 1)) {
				goto err;
			}
		} else {
			/* if N mod word size == 1 */
			if (!BN_set_word(Ri, BN_MASK2)) {
				goto err;                         /* Ri-- (mod word size) */
			}
		}
		if (!BN_div(Ri, NULL, Ri, &tmod, ctx)) {
			goto err;
		}

		/* Ni = (R*Ri-1)/N,
		 * keep only least significant word: */
		mont->n0 = (Ri->top > 0) ? Ri->d[0] : 0;
	}
#else   /* !MONT_WORD */
	{ /* bignum version */
		mont->ri = BN_num_bits(&mont->N);
		BN_zero(R);
		if (!BN_set_bit(R, mont->ri)) {
			goto err;                       /* R = 2^ri */
		}
		/* Ri = R^-1 mod N*/
		if ((BN_mod_inverse(Ri, R, &mont->N, ctx)) == NULL) {
			goto err;
		}
		if (!BN_lshift(Ri, Ri, mont->ri)) {
			goto err;                         /* R*Ri */
		}
		if (!BN_sub_word(Ri, 1)) {
			goto err;
		}
		/* Ni = (R*Ri-1) / N */
		if (!BN_div(&(mont->Ni), NULL, Ri, &mont->N, ctx)) {
			goto err;
		}
	}
#endif

	/* setup RR for conversions */
	BN_zero(&(mont->RR));
	if (!BN_set_bit(&(mont->RR), mont->ri*2)) {
		goto err;
	}
	if (!BN_mod(&(mont->RR), &(mont->RR), &(mont->N), ctx)) {
		goto err;
	}

	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}


BN_MONT_CTX *
BN_MONT_CTX_copy(BN_MONT_CTX *to, BN_MONT_CTX *from)
{
	if (to == from) {
		return (to);
	}

	if (!BN_copy(&(to->RR), &(from->RR))) {
		return (NULL);
	}
	if (!BN_copy(&(to->N), &(from->N))) {
		return (NULL);
	}
	if (!BN_copy(&(to->Ni), &(from->Ni))) {
		return (NULL);
	}
	to->ri = from->ri;
	to->n0 = from->n0;
	return (to);
}


#if 0
BN_MONT_CTX *
BN_MONT_CTX_set_locked(BN_MONT_CTX **pmont, int lock,
    const BIGNUM *mod, BN_CTX *ctx)
{
	int got_write_lock = 0;
	BN_MONT_CTX *ret;

	CRYPTO_r_lock(lock);
	if (!*pmont) {
		CRYPTO_r_unlock(lock);
		CRYPTO_w_lock(lock);
		got_write_lock = 1;

		if (!*pmont) {
			ret = BN_MONT_CTX_new();
			if (ret && !BN_MONT_CTX_set(ret, mod, ctx)) {
				BN_MONT_CTX_free(ret);
			} else{
				*pmont = ret;
			}
		}
	}

	ret = *pmont;

	if (got_write_lock) {
		CRYPTO_w_unlock(lock);
	} else{
		CRYPTO_r_unlock(lock);
	}

	return (ret);
}
#endif
