/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#ifdef HEXFLOAT

/*
 * hexfloat.c provides routines that vfprintf and vfwprintf can call to convert
 * floating point numbers to hexidecimal, as used by the %a and %A conversions.
 * This is necessarily dependent not only on the floating point data format,
 * but also byte order and existence of long double type.
 *
 * The union hexdouble represents the IEEE-754 double precision format, while
 * union hexlongdouble represents the IEEE-754 extended double precision
 * format.
 */

#include <stdio.h>
#include <math.h>
#include <limits.h>

#define	EXPBIAS		1023
#define	EXPMIN		-1022
#define	EXPSPECIAL	2047
#define	FRACTHEX	13

union hexdouble {
	double d;
	struct {
#if defined(__ppc__)
		unsigned int sign:1;
		unsigned int exp:11;
		unsigned long long fract:52;
#elif defined(__i386__)
		unsigned long long fract:52;
		unsigned int exp:11;
		unsigned int sign:1;
#else
#error Unsupported architecture
#endif
	} s;
};

#if !__TYPE_LONGDOUBLE_IS_DOUBLE
#ifdef __i386__

#define	LEXPBIAS	16383
#define	LEXPMIN		-16382
#define	LEXPSPECIAL	32767
#define	LFRACTHEX	16

union hexlongdouble {
	long double d;
	struct {
		unsigned long long fract:63;
		unsigned int i:1;
		unsigned int exp:15;
		unsigned int sign:1;
	} s;
};
#endif /* __i386__ */
#endif /* !__TYPE_LONGDOUBLE_IS_DOUBLE */

int
__hdtoa(double d, const char *xdigs, int prec, char *cp,
    int *expt, int *signflag, char **dtoaend)
{
	union hexdouble u;
	char buf[FRACTHEX];
	char *hp;
	int i;
	long long fract;
	//char *start = cp; //DEBUG

	u.d = d;
	//printf("\nsign=%d exp=%x fract=%llx\n", u.s.sign, u.s.exp, u.s.fract); //DEBUG
	*signflag = u.s.sign;
	fract = u.s.fract;
	switch (u.s.exp) {
	case EXPSPECIAL:	/* NaN or Inf */
		*expt = INT_MAX;
		*cp = (fract ? 'N' : 'I');
		return 0;
	case 0:			/* Zero or denormalized */
		*cp++ = '0';
		*expt = (fract ? EXPMIN : 0);
		break;
	default:		/* Normal numbers */
		*cp++ = '1';
		*expt = u.s.exp - EXPBIAS;
		break;
	}
	if (prec < 0)
		prec = FRACTHEX;
	//printf("prec=%d expt=%d\n", prec, *expt); //DEBUG
	if (prec > 0) {
		int dig = (prec > FRACTHEX ? FRACTHEX : prec);
		int zero = prec - dig;
		int shift = FRACTHEX - dig;
		if (shift > 0)
		    fract >>= (shift << 2);
		for (hp = buf + dig, i = dig; i > 0; i--) {
			*--hp = xdigs[fract & 0xf];
			fract >>= 4;
		}
		strncpy(cp, hp, dig);
		cp += dig;
		while(zero-- > 0)
			*cp++ = '0';
	}
	*dtoaend = cp;
	//while (start < cp) putchar(*start++); //DEBUG
	//putchar('\n'); //DEBUG
	return prec;
}

#if !__TYPE_LONGDOUBLE_IS_DOUBLE
#ifdef __i386__
int
__hldtoa(long double d, const char *xdigs, int prec, char *cp,
    int *expt, int *signflag, char **dtoaend)
{
	union hexlongdouble u;
	char buf[LFRACTHEX];
	char *hp;
	int i;
	unsigned long long fract;
	//char *start = cp; //DEBUG

	u.d = d;
	//printf("d=%Lg u.d=%Lg\n", d, u.d); //DEBUG
	//printf("\nsign=%d exp=%x fract=%llx\n", u.s.sign, u.s.exp, u.s.fract); //DEBUG
	*signflag = u.s.sign;
	fract = (u.s.fract << 1);
	switch (u.s.exp) {
	case LEXPSPECIAL:	/* NaN or Inf */
		*expt = INT_MAX;
		*cp = (fract ? 'N' : 'I');
		return 0;
	default:		/* Normal or denormalized */
		*cp++ = u.s.i ? '1' : '0';
		*expt = u.s.exp - LEXPBIAS;
		break;
	}
	if (prec < 0)
		prec = LFRACTHEX;
	//printf("prec=%d expt=%d\n", prec, *expt); //DEBUG
	if (prec > 0) {
		int dig = (prec > LFRACTHEX ? LFRACTHEX : prec);
		int zero = prec - dig;
		int shift = LFRACTHEX - dig;
		if (shift > 0)
		    fract >>= (shift << 2);
		for (hp = buf + dig, i = dig; i > 0; i--) {
			*--hp = xdigs[fract & 0xf];
			fract >>= 4;
		}
		strncpy(cp, hp, dig);
		cp += dig;
		while(zero-- > 0)
			*cp++ = '0';
	}
	*dtoaend = cp;
	//while (start < cp) putchar(*start++); //DEBUG
	//putchar('\n'); //DEBUG
	return prec;
}
#endif /* __i386__ */
#endif /* !__TYPE_LONGDOUBLE_IS_DOUBLE */

#endif /* HEXFLOAT */
