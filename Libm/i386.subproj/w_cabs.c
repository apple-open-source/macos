/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: w_cabs.c,v 1.4 2001/01/06 00:15:00 christos Exp $");
#endif

#include <math.h>

double cabs ( __complex_t z )
{
	return hypot(z.Real, z.Imag);
}
