/*
 * cabsf() wrapper for hypotf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: w_cabsf.c,v 1.4 2001/01/06 00:15:00 christos Exp $");
#endif

#include <math.h>

float cabsf(_complexf z)
{
	return hypotf(z.Real, z.Imag);
}
