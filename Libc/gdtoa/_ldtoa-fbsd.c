/*-
 * Copyright (c) 2003 David Schultz <das@FreeBSD.ORG>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gdtoa/_ldtoa.c,v 1.2 2004/01/18 07:53:49 das Exp $");

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "fpmath.h"
#include "gdtoaimp.h"

/*
 * ldtoa() is a wrapper for gdtoa() that makes it smell like dtoa(),
 * except that the floating point argument is passed by reference.
 * When dtoa() is passed a NaN or infinity, it sets expt to 9999.
 * However, a long double could have a valid exponent of 9999, so we
 * use INT_MAX in ldtoa() instead.
 */
char *
__ldtoa(long double *ld, int mode, int ndigits, int *decpt, int *sign,
    char **rve)
{
	static FPI fpi0 = {
		LDBL_MANT_DIG,			/* nbits */
		LDBL_MIN_EXP - LDBL_MANT_DIG,	/* emin */
		LDBL_MAX_EXP - LDBL_MANT_DIG,	/* emax */
		FPI_Round_near,	       		/* rounding */
#ifdef Sudden_Underflow	/* unused, but correct anyway */
		1
#else
		0
#endif
	};
	int be, kind;
	char *ret;
	union IEEEl2bits u;
	uint32_t bits[(LDBL_MANT_DIG + 31) / 32];
	FPI *fpi = &fpi0, fpi1;
#ifdef Honor_FLT_ROUNDS
	int rounding = Flt_Rounds;
#endif
	int type;

	u.e = *ld;
#if defined(__ppc__) || defined(__ppc64__)
	/*
	 * Subnormal head-tail doubles don't seem to be converted correctly
	 * by gdtoa.  So we multiply by 10^32 to make them normal then
	 * subtract 32 from the exponent later.
	 */
	if ((type = __fpclassify(u.e)) == FP_NORMAL && __fpclassifyd(u.d[1]) == FP_SUBNORMAL)
		type = FP_SUBNORMAL;
	if (type == FP_SUBNORMAL)
		u.e *= 1.0e32L;
#else /* !defined(__ppc__) && !defined(__ppc64__) */
	type = fpclassify(u.e);
#endif /* defined(__ppc__) || defined(__ppc64__) */
	*sign = u.bits.sign;
	be = u.bits.exp - (LDBL_MAX_EXP - 1) - (LDBL_MANT_DIG - 1);
#if defined(__ppc__) || defined(__ppc64__)
	be -= LDBL_TO_ARRAY32(u, bits);
#else /* !defined(__ppc__) && !defined(__ppc64__) */
	LDBL_TO_ARRAY32(u, bits);
#endif /* defined(__ppc__) || defined(__ppc64__) */

	switch (type) {
#if defined(__ppc__) || defined(__ppc64__)
	case FP_SUBNORMAL:
#endif /* defined(__ppc__) || defined(__ppc64__) */
	case FP_NORMAL:
	case FP_SUPERNORMAL:
		kind = STRTOG_Normal;
/* For ppc/ppc64 and head-tail long double, the implicit bit is already there */
#if !defined(__ppc__) && !defined(__ppc64__)
#ifdef	LDBL_IMPLICIT_NBIT
		bits[LDBL_MANT_DIG / 32] |= 1 << ((LDBL_MANT_DIG - 1) % 32);
#endif /* LDBL_IMPLICIT_NBIT */
#endif /* !defined(__ppc__) && !defined(__ppc64__) */
		break;
	case FP_ZERO:
		kind = STRTOG_Zero;
		break;
#if !defined(__ppc__) && !defined(__ppc64__)
	case FP_SUBNORMAL:
		kind = STRTOG_Denormal;
		be++;
		break;
#endif /* !defined(__ppc__) && !defined(__ppc64__) */
	case FP_INFINITE:
		kind = STRTOG_Infinite;
		break;
	case FP_NAN:
		kind = STRTOG_NaN;
		break;
	default:
		LIBC_ABORT("fpclassify returned %d", type);
	}

#ifdef Honor_FLT_ROUNDS
	if (rounding != fpi0.rounding) {
		fpi1 = fpi0; /* for thread safety */
		fpi1.rounding = rounding;
		fpi = &fpi1;
		}
#endif /* Honor_FLT_ROUNDS */
	ret = gdtoa(fpi, be, (ULong *)bits, &kind, mode, ndigits, decpt, rve);
	if (*decpt == -32768)
		*decpt = INT_MAX;
#if defined(__ppc__) || defined(__ppc64__)
	else if (type == FP_SUBNORMAL)
		*decpt -= 32;
#endif /* defined(__ppc__) || defined(__ppc64__) */
	return ret;
}
