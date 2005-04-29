/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http:www.opensource.apple.com/apsl/ and read it before using this
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

#include <sys/cdefs.h>
#include <stdint.h>
#include <strings.h>
#include <float.h>
#include <math.h>
#include <alloca.h>

#include "fpmath.h"

#define	BITS64		64
#define	DBL_BIAS	(DBL_MAX_EXP - 1)
#define	DBL_SUBEXP	(-DBL_BIAS + 1)
#define	LL_BITS		(8 * sizeof(int64_t))
#define	LL_HIGHBIT	(1LL << 63)

__private_extern__ void
_ldbl2array32dd(union IEEEl2bits u, uint32_t *a)
{
	int bit, shift, highbit;
	uint64_t a64[2];
	int64_t t64;

	bzero(a64, 2 * sizeof(*a64));

	switch (__fpclassifyd(u.d[0])) {
	case FP_NORMAL:
		a64[1] = (1LL << (LDBL_MANT_DIG - BITS64 - 1));
		/* drop through */
	case FP_SUBNORMAL:
		a64[1] |= ((uint64_t)u.bits.manh >> (BITS64 - LDBL_MANL_SIZE));
		a64[0] = ((uint64_t)u.bits.manh << LDBL_MANL_SIZE);
		break;
	default:
		goto done;
	}

	switch (__fpclassifyd(u.d[1])) {
	case FP_NORMAL:
		bit = LDBL_MANT_DIG - (int)u.bits.exp + (int)u.bits.exp2 - 1;
		t64 = (1LL << bit);
		break;
	case FP_SUBNORMAL:
		bit = LDBL_MANT_DIG - (int)u.bits.exp;
		t64 = 0;
		break;
	default:
		goto done;
	}
	shift = LDBL_MANL_SIZE - bit - 1;
	if (shift >= 0)
		t64 |= (u.bits.manl >> shift);
	else
		t64 |= (u.bits.manl << (-shift));
	highbit = ((a64[0] & LL_HIGHBIT) != 0);
	if (u.bits.sign == u.bits.sign2) {
		a64[0] += t64;
		if (highbit && !(a64[0] & LL_HIGHBIT))	/* carry */
			a64[1]++;
	} else {
		a64[0] -= t64;
		if (!highbit && (a64[0] & LL_HIGHBIT))	/* borrow */
			a64[1]--;
	}

	done:
	a[0] = (uint32_t)a64[0];
	a[1] = (uint32_t)(a64[0] >> 32);
	a[2] = (uint32_t)a64[1];
	a[3] = (uint32_t)(a64[1] >> 32);
}
