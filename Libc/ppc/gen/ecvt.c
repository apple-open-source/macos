/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 *	File:	libc/m98k/gen/ecvt.c
 *
 * char *ecvt(double x, int ndigits, int *decimal, int *sign);
 * char *fcvt(double x, int ndigits, int *decimal, int *sign);
 *
 * The function `ecvt' converts the double `x' to a null-terminated
 * string of `ndigits' ASCII digits and returns a pointer to the string.
 * The position of the decimal point relative to the beginning of the
 * string is stored in the int pointed to by `decimal'.  A negative
 * value means that the decimal point appears to the left of the returned
 * digits.  If the sign of the result is negative, a non-zero value is
 * stored in the int pointed to by `sign'; otherwise, a zero value is stored.
 * The low-order digit of the returned value is rounded.
 *
 * The function `fcvt' is identical to `ecvt', except that the correct digit
 * has been rounded for Fortran F-format output of the number of digits
 * specified by `ndigits'.
 *
 * HISTORY
 *  10-Nov-92  Derek B Clegg (dclegg@next.com)
 *	Ported to m98k.
 *   8-Jan-92  Peter King (king@next.com)
 *	Created from M68K sources which was created from VAX sources.
 */
#import <math.h>

static double ecvt_rint(double x);
static double ecvt_copysign(double x, double y);
static char *cvt(double arg, int ndigits, int *decptp, int *signp, int eflag);

#define isNAN(x) ((x) != (x))

/* big enough to handle %.20f conversion of 1e308 */
#define	NDIG		350

char *
ecvt(double arg, int ndigits, int *decptp, int *signp)
{
    return (cvt(arg, ndigits, decptp, signp, 1));
}

char *
fcvt(double arg, int ndigits, int *decptp, int *signp)
{
    return (cvt(arg, ndigits, decptp, signp, 0));
}

static char *
cvt(double arg, int ndigits, int *decptp, int *signp, int eflag)
{
    int decpt;
    double fi, fj;
    char *p, *p1;
    static char buf[NDIG] = { 0 };

    if (ndigits < 0)
	ndigits = 0;
    if (ndigits >= NDIG - 1)
	ndigits = NDIG - 2;

    decpt = 0;
    *signp = 0;
    p = &buf[0];

    if (arg == 0) {
	*decptp = 0;
	while (p < &buf[ndigits])
	    *p++ = '0';
	*p = '\0';
	return (buf);
    } else if (arg < 0) {
	*signp = 1;
	arg = -arg;
    }

    arg = modf(arg, &fi);
    p1 = &buf[NDIG];

    /* Do integer part */

    if (fi != 0) {
	while (fi != 0) {
	    fj = modf(fi/10, &fi);
#if 0
	    *--p1 = (int)((fj + 0.03) * 10) + '0';
#else
	    *--p1 = (int)ecvt_rint(fj * 10) + '0';
#endif
	    decpt++;
	}
	while (p1 < &buf[NDIG])
	    *p++ = *p1++;
    } else if (arg > 0) {
	while ((fj = arg*10) < 1) {
	    arg = fj;
	    decpt--;
	}
    }
    *decptp = decpt;

    /* Do the fractional part.
     * p pts to where fraction should be concatenated.
     * p1 is how far conversion must go to.
     */
    p1 = &buf[ndigits];
    if (eflag == 0) {
	/* fcvt must provide ndigits after decimal pt */
	p1 += decpt;
	/* if decpt was negative, we might be done for fcvt */
	if (p1 < &buf[0]) {
	    buf[0] = '\0';
	    return (buf);
	}
    }

    while (p <= p1 && p < &buf[NDIG]) {
	arg *= 10;
	arg = modf(arg, &fj);
	*p++ = (int)fj + '0';
    }

    /* If we converted all the way to the end of the buf, don't mess with
     * rounding since there's nothing significant out here anyway.
     */
    if (p1 >= &buf[NDIG]) {
	buf[NDIG-1] = '\0';
	return (buf);
    }

    /* Round by adding 5 to last digit and propagating carries. */
    p = p1;
    *p1 += 5;
    while (*p1 > '9') {
	*p1 = '0';
	if (p1 > buf) {
	    ++*--p1;
	} else {
	    *p1 = '1';
	    (*decptp)++;
	    if (eflag == 0) {
		if (p > buf)
		    *p = '0';
		p++;
	    }
	}
    }
    *p = '\0';
    return (buf);
}

static double L = 4503599627370496.0E0;		/* 2**52 */

static int ecvt_init = 0;

/*
 * FIXME: This deserves a comment if you turn this off!
 * This used to #pragma CC_OPT_OFF.
 * (Probably this was because the isNAN test was optimized away.)
 * Why don't we just use the value of L given above?
 */

static double
ecvt_rint(double x)
{
    double s, t, one;

    one = 1.0;

    if (ecvt_init == 0) {
	int i;
	L = 1.0;
	for (i = 52; i != 0; i--)
	    L *= 2.0;
	ecvt_init = 1;
    }
    if (isNAN(x))
	return (x);
    if (ecvt_copysign(x, one) >= L)		/* already an integer */
	return (x);
    s = ecvt_copysign(L, x);
    t = x + s;				/* x+s rounded to integer */
    return (t - s);
}

/* Couldn't we use something like the following structure instead of the
   hacky unsigned short pointer stuff?

struct double_format {
    unsigned sign:1;
    unsigned exponent:11;
    unsigned hi_fraction:20;
    unsigned lo_fraction:32;
};

*/

#define msign ((unsigned short)0x7fff)
#define mexp ((unsigned short)0x7ff0)

static double
ecvt_copysign(double x, double y)
{
    unsigned short *px, *py;

    px = (unsigned short *)&x;
    py = (unsigned short *)&y;
    *px = (*px & msign) | (*py & ~msign);
    return (x);
}

/*
 * This used to #pragma CC_OPT_ON
 */
 
