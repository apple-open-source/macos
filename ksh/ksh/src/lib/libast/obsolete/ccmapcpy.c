/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#pragma prototyped

/*
 * Glenn Fowler
 * AT&T Research
 *
 * character code string map
 */

#include <ast.h>
#include <ccode.h>

#undef	ccmaps

void*
ccmapcpy(void* b, const void* a, size_t n, int in, int out)
{
	register unsigned char*		ub = (unsigned char*)b;
	register unsigned char*		ue = ub + n;
	register const unsigned char*	ua = (const unsigned char*)a;
	register const unsigned char*	m = CCMAP(in, out);

	while (ub < ue)
		*ub++ = m[*ua++];
	return b;
}
