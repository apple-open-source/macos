/* w_lgammaf.c -- float version of w_lgamma.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

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
__RCSID("$NetBSD: w_lgammaf.c,v 1.6 2001/01/06 00:15:00 christos Exp $");
#endif

#define _REENTRANT 1
#include "math.h"
#include "math_private.h"

float lgammaf(float x)
{
	return lgammaf_r(x,&signgam);
}
