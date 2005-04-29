/* @(#)e_scalb.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include <sys/cdefs.h>
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: e_scalb.c,v 1.8 1999/07/02 15:37:41 simonb Exp $");
#endif

/*
 * __ieee754_scalb(x, fn) is provide for
 * passing various standard test suite. One
 * should use scalbn() instead.
 */

extern double scalbn ( double, int );
double scalb(double x, int fn)
{
	return scalbn(x,fn);
}
