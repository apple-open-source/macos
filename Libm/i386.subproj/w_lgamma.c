/* @(#)w_lgamma.c 5.1 93/09/24 */
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
__RCSID("$NetBSD: w_lgamma.c,v 1.9 2001/01/06 00:15:00 christos Exp $");
#endif

/* double lgamma(double x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call __ieee754_lgamma_r
 */

#define _REENTRANT 1
#include "math.h"
#include "math_private.h"

double lgamma(double x)
{
	return lgamma_r(x,&signgam);
}
