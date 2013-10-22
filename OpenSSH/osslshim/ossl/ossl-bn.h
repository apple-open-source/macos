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

#ifndef _OSSL_BN_H_
#define _OSSL_BN_H_    1

#include <stdio.h>

/* symbol renaming */
#define BN_GENCB_call		ossl_BN_GENCB_call
#define BN_GENCB_set		ossl_BN_GENCB_set
#define BN_add			ossl_BN_add
#define BN_add_word		ossl_BN_add_word
#define BN_bin2bn		ossl_BN_bin2bn
#define BN_bn2bin		ossl_BN_bn2bin
#define BN_bn2dec		ossl_BN_bn2dec
#define BN_bn2hex		ossl_BN_bn2hex
#define BN_clear		ossl_BN_clear
#define BN_clear_bit		ossl_BN_clear_bit
#define BN_clear_free		ossl_BN_clear_free
#define BN_cmp			ossl_BN_cmp
#define BN_copy			ossl_BN_copy
#define BN_dec2bn		ossl_BN_dec2bn
#define BN_div			ossl_BN_div
#define BN_dup			ossl_BN_dup
#define BN_free			ossl_BN_free
#define BN_gcd			ossl_BN_gcd
#define BN_generate_prime_ex	ossl_BN_generate_prime_ex
#define BN_init			ossl_BN_init
#undef BN_is_negative
#define BN_is_negative(a)    ((a)->neg != 0)
#define BN_get_word		ossl_BN_get_word
#define BN_hex2bn		ossl_BN_hex2bn
#define BN_is_bit_set		ossl_BN_is_bit_set
#undef BN_abs_is_word
#define BN_abs_is_word(a, w)				      \
	((((a)->top == 1) && ((a)->d[0] == (BN_ULONG)(w))) || \
	(((w) == 0) && ((a)->top == 0)))
#undef BN_is_one
#define BN_is_one(a)    (BN_abs_is_word((a), 1) && !(a)->neg)
#define BN_is_prime_ex			ossl_BN_is_prime_ex
#define BN_is_prime_fasttest_ex		ossl_BN_is_prime_fasttest_ex
#undef BN_is_zero
#define BN_is_zero(a)		((a)->top == 0)
#undef BN_is_word
#define BN_is_word(a, w)	(BN_abs_is_word((a), (w)) && (!(w) || !(a)->neg))
#undef BN_is_odd
#define BN_is_odd(a)		(((a)->top > 0) && ((a)->d[0] & 1))
#define BN_lshift	ossl_BN_lshift
#define BN_lshift1	ossl_BN_lshift1
#undef BN_set_flags
#define BN_set_flags(b, n)	((b)->flags |= (n))
#undef BN_get_flags
#define BN_get_flags(b, n)	((b)->flags&(n))

#define BN_mask_bits			ossl_BN_mask_bits
#undef BN_mod
#define BN_mod				ossl_BN_mod
#define BN_mod_exp			ossl_BN_mod_exp
#define BN_mod_exp_mont			ossl_BN_mod_exp_mont
#define BN_mod_exp2_mont		ossl_BN_mod_exp2_mont
#define BN_mod_exp_mont_consttime	ossl_BN_mod_exp_mont_consttime
#define BN_mod_exp_mont_word		ossl_BN_mod_exp_mont_word
#define BN_mod_inverse			ossl_BN_mod_inverse
#define BN_mod_word			ossl_BN_mod_word
#define BN_mul				ossl_BN_mul
#define BN_new				ossl_BN_new
#define BN_num_bits			ossl_BN_num_bits
#define BN_num_bits_word		ossl_BN_num_bits_word
#define BN_num_bytes			ossl_BN_num_bytes
#define BN_print_fp			ossl_BN_print_fp
#define BN_rand				ossl_BN_rand
#define BN_pseudo_rand			ossl_BN_pseudo_rand
#define BN_rand_range			ossl_BN_rand_range
#define BN_pseudo_rand_range		ossl_BN_pseudo_rand_range
#define BN_rshift			ossl_BN_rshift
#define BN_rshift1			ossl_BN_rshift1
#define BN_set_bit			ossl_BN_set_bit
#define BN_set_negative			ossl_BN_set_negative
#define BN_set_word			ossl_BN_set_word
#define BN_sqr				ossl_BN_sqr
#define BN_sub				ossl_BN_sub
#define BN_sub_word			ossl_BN_sub_word
#define BN_uadd				ossl_BN_uadd
#define BN_ucmp				ossl_BN_ucmp
#define BN_usub				ossl_BN_usub
#define BN_value_one			ossl_BN_value_one
#define BN_CTX_new			ossl_BN_CTX_new
#define BN_CTX_free			ossl_BN_CTX_free
#define BN_CTX_get			ossl_BN_CTX_get
#define BN_CTX_start			ossl_BN_CTX_start
#define BN_CTX_end			ossl_BN_CTX_end

#define BN_mod_mul_montgomery		ossl_BN_mod_mul_montgomery
#define BN_from_montgomery		ossl_BN_from_montgomery
#define BN_MONT_CTX_new			ossl_BN_MONT_CTX_new
#define BN_MONT_CTX_init		ossl_BN_MONT_CTX_init
#define BN_MONT_CTX_free		ossl_BN_MONT_CTX_free
#define BN_MONT_CTX_set			ossl_BN_MONT_CTX_set
#define BN_MONT_CTX_copy		ossl_BN_MONT_CTX_copy
#define BN_MONT_CTX_set_locked		ossl_BN_MONT_CTX_set_locked

#define BN_to_montgomery(r, a, mont, ctx) \
	BN_mod_mul_montgomery((r), (a), &((mont)->RR), (mont), (ctx))

#define BN_zero(a)	(BN_set_word((a), 0))
#define BN_one(a)	(BN_set_word((a), 1))

#undef BN_with_flags

/* get a clone of a BIGNUM with changed flags, for *temporary* use only
 *  * (the two BIGNUMs cannot not be used in parallel!) */
#define BN_with_flags(dest, b, n)			   \
	((dest)->d = (b)->d,				   \
	(dest)->top = (b)->top,				   \
	(dest)->dmax = (b)->dmax,			   \
	(dest)->neg = (b)->neg,				   \
	(dest)->flags = (((dest)->flags & BN_FLG_MALLOCED) \
	|  ((b)->flags & ~BN_FLG_MALLOCED)		   \
	|  BN_FLG_STATIC_DATA				   \
	|  (n)))


#if defined(__LP64__) && defined(__x86_64__)

#  define SIXTY_FOUR_BIT_LONG		1

#  define BN_ULLONG			unsigned long long
#  define BN_ULONG			unsigned long
#  define BN_LONG			long
#  define BN_BITS			128
#  define BN_BYTES			8
#  define BN_BITS2			64
#  define BN_BITS4			32
#  define BN_MASK			(0xffffffffffffffffffffffffffffffffLL)
#  define BN_MASK2			(0xffffffffffffffffL)
#  define BN_MASK2l			(0xffffffffL)
#  define BN_MASK2h			(0xffffffff00000000L)
#  define BN_MASK2h1			(0xffffffff80000000L)
#  define BN_TBIT			(0x8000000000000000L)
#  define BN_DEC_CONV			(10000000000000000000UL)
#  define BN_DEC_FMT1			"%lu"
#  define BN_DEC_FMT2			"%019lu"
#  define BN_DEC_NUM			19

#elif !defined(__LP64__) && defined(__i386__)

#  define THIRTY_TWO_BIT	1

#  define BN_ULLONG		unsigned long long
#  define BN_ULONG		unsigned long
#  define BN_LONG		long
#  define BN_BITS		64
#  define BN_BYTES		4
#  define BN_BITS2		32
#  define BN_BITS4		16
#  define BN_MASK		(0xffffffffffffffffLL)
#  define BN_MASK2		(0xffffffffL)
#  define BN_MASK2l		(0xffff)
#  define BN_MASK2h1		(0xffff8000L)
#  define BN_MASK2h		(0xffff0000L)
#  define BN_TBIT		(0x80000000L)
#  define BN_DEC_CONV		(1000000000L)
#  define BN_DEC_FMT1		"%lu"
#  define BN_DEC_FMT2		"%09lu"
#  define BN_DEC_NUM		9

#else

#   error "Unknown arch"

#endif /* ! __LP64__ */


/*
 *
 */

typedef struct bignum_ctx		BN_CTX;
typedef struct bignum_gencb_st		BN_GENCB;
typedef struct bn_mont_ctx_st		BN_MONT_CTX;
typedef struct BN_BLINDING		BN_BLINDING;

typedef struct bignum_st {
	BN_ULONG *	d;      /* Pointer to an array of 'BN_BITS2' bit chunks. */
	int		top;    /* Index of last used d +1. */
	/* The next are internal book keeping for bn_expand. */
	int		dmax;   /* Size of the d array. */
	int		neg;    /* one if the number is negative */
	int		flags;
} BIGNUM;

/* BIGNUM flags: */
#define BN_FLG_MALLOCED		0x01    /* d has been malloc()'ed */
#define BN_FLG_STATIC_DATA	0x02    /* static or constant data */
#define BN_FLG_CONSTTIME	0x04    /* avoid timing attacks */

#define BN_prime_checks		0       /* default */

struct bignum_gencb_st {
	unsigned int	ver;
	void *		arg;
	union {
		int (*cb_2)(int, int, BN_GENCB *);
	}
	cb;
};

struct bn_mont_ctx_st {
	int		ri;     /* number of bits in R */
	BIGNUM		RR;     /* used to convert to montgomery form */
	BIGNUM		N;      /* The modulus */
	BIGNUM		Ni;     /* R*(1/R mod N) - N*Ni = 1
	                         * (Ni is only stored for bignum algorithm) */
	BN_ULONG	n0;     /* least significant word of Ni */
	int		flags;
};


/*
 *
 */

BIGNUM *BN_new(void);
void BN_init(BIGNUM *);
void BN_free(BIGNUM *);
void BN_clear_free(BIGNUM *);
void BN_clear(BIGNUM *);
BIGNUM *BN_dup(const BIGNUM *);
BIGNUM *BN_copy(BIGNUM *, const BIGNUM *);

int BN_num_bits(const BIGNUM *);

int BN_num_bits_word(BN_ULONG);
int BN_num_bytes(const BIGNUM *);

int BN_cmp(const BIGNUM *, const BIGNUM *);
int BN_ucmp(const BIGNUM *, const BIGNUM *);

void BN_set_negative(BIGNUM *, int);

int BN_is_bit_set(const BIGNUM *, int);
int BN_set_bit(BIGNUM *, int);
int BN_clear_bit(BIGNUM *, int);
int BN_mask_bits(BIGNUM *, int);

int BN_set_word(BIGNUM *, BN_ULONG);
BN_ULONG BN_get_word(const BIGNUM *);
int BN_add_word(BIGNUM *a, BN_ULONG num);
int BN_sub_word(BIGNUM *a, BN_ULONG num);

BN_ULONG BN_div_word(BIGNUM *, BN_ULONG);
BN_ULONG BN_mod_word(const BIGNUM *a, BN_ULONG w);

int BN_mul_word(BIGNUM *, BN_ULONG);

int BN_mod_exp_mont_word(BIGNUM *rr, BN_ULONG a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont);
int BN_mod_exp_mont(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont);
int BN_mod_exp2_mont(BIGNUM *rr, const BIGNUM *a1, const BIGNUM *p1,
    const BIGNUM *a2, const BIGNUM *p2, const BIGNUM *m,
    BN_CTX *ctx, BN_MONT_CTX *in_mont);
int BN_mod_exp_mont_consttime(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont);

int BN_lshift(BIGNUM *, const BIGNUM *, int);
int BN_lshift1(BIGNUM *, const BIGNUM *);
int BN_rshift(BIGNUM *, const BIGNUM *, int);
int BN_rshift1(BIGNUM *, const BIGNUM *);

BIGNUM *BN_bin2bn(const unsigned char *, int len, BIGNUM *);
int BN_bn2bin(const BIGNUM *, unsigned char *);
int BN_hex2bn(BIGNUM **, const char *);
char *BN_bn2hex(const BIGNUM *);
int BN_print(void *fp, const BIGNUM *a);
int BN_print_fp(FILE *, const BIGNUM *);

int BN_uadd(BIGNUM *, const BIGNUM *, const BIGNUM *);
int BN_usub(BIGNUM *, const BIGNUM *, const BIGNUM *);
int BN_sub(BIGNUM *, const BIGNUM *, const BIGNUM *);
int BN_add(BIGNUM *, const BIGNUM *, const BIGNUM *);

int BN_div(BIGNUM *, BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
int BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx);

int BN_mod(BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
int BN_mod_mul(BIGNUM *, const BIGNUM *, const BIGNUM *,
    const BIGNUM *, BN_CTX *);
int BN_mod_exp(BIGNUM *, const BIGNUM *, const BIGNUM *,
    const BIGNUM *, BN_CTX *);
BIGNUM *BN_mod_inverse(BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *ctx);

int BN_sqr(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx);

int BN_is_prime_ex(const BIGNUM *a, int checks, BN_CTX *ctx_passed, BN_GENCB *cb);
int BN_is_prime_fasttest_ex(const BIGNUM *p, int nchecks, BN_CTX *ctx,
    int do_trial_division, BN_GENCB *cb);

const BIGNUM *BN_value_one(void);

int BN_rand(BIGNUM *, int, int, int);
int BN_pseudo_rand(BIGNUM *, int, int, int);
int BN_rand_range(BIGNUM *, const BIGNUM *);
int BN_pseudo_rand_range(BIGNUM *, const BIGNUM *);

int BN_generate_prime_ex(BIGNUM *, int, int, const BIGNUM *, const BIGNUM *, BN_GENCB *);
int BN_gcd(BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);

void BN_GENCB_set(BN_GENCB *, int (*)(int, int, BN_GENCB *), void *);
int BN_GENCB_call(BN_GENCB *, int, int);

BN_CTX *BN_CTX_new(void);
void BN_CTX_free(BN_CTX *);
BIGNUM *BN_CTX_get(BN_CTX *);
void BN_CTX_start(BN_CTX *);
void BN_CTX_end(BN_CTX *);

int BN_mod_mul_montgomery(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
    BN_MONT_CTX *mont, BN_CTX *ctx);
int BN_from_montgomery(BIGNUM *ret, const BIGNUM *a,
    BN_MONT_CTX *mont, BN_CTX *ctx);
BN_MONT_CTX *BN_MONT_CTX_new(void);
void BN_MONT_CTX_init(BN_MONT_CTX *ctx);
void BN_MONT_CTX_free(BN_MONT_CTX *mont);
int BN_MONT_CTX_set(BN_MONT_CTX *mont, const BIGNUM *mod, BN_CTX *ctx);
BN_MONT_CTX *BN_MONT_CTX_copy(BN_MONT_CTX *to, BN_MONT_CTX *from);
BN_MONT_CTX *BN_MONT_CTX_set_locked(BN_MONT_CTX **pmont, int lock,
    const BIGNUM *mod, BN_CTX *ctx);

int BN_dec2bn(BIGNUM **a, const char *str);
char *BN_bn2dec(const BIGNUM *a);

#endif /* _OSSL_BN_H_ */
