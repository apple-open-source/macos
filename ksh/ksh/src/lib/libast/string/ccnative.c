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
 * copy table with element size n
 * indexed by CC_ASCII to table
 * indexed by CC_NATIVE
 */

#include <ast.h>
#include <ccode.h>

void*
ccnative(void* b, const void* a, size_t n)
{
#if CC_ASCII == CC_NATIVE
	return memcpy(b, a, n * (UCHAR_MAX + 1));
#else
	register int			c;
	register const unsigned char*	m;
	register unsigned char*		cb = (unsigned char*)b;
	register unsigned char*		ca = (unsigned char*)a;

	m = CCMAP(CC_ASCII, CC_NATIVE);
	if (n == sizeof(char))
		for (c = 0; c <= UCHAR_MAX; c++)
			cb[c] = ca[m[c]];
	else
		for (c = 0; c <= UCHAR_MAX; c++)
			memcpy(cb + n * c, ca + n * m[c], n);
	return b;
#endif
}
