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
 * ccmapc(c, CC_NATIVE, CC_ASCII) and strncmp
 */

#include <ast.h>
#include <ccode.h>

#if _lib_strnacmp

NoN(strnacmp)

#else

#include <ctype.h>

#undef	strnacmp

int
strnacmp(const char* a, const char* b, size_t n)
{
#if CC_NATIVE == CC_ASCII
	return strncmp(a, b, n);
#else
	register unsigned char*	ua = (unsigned char*)a;
	register unsigned char*	ub = (unsigned char*)b;
	register unsigned char*	ue;
	register unsigned char*	m;
	register int		c;
	register int		d;

	m = ccmap(CC_NATIVE, CC_ASCII);
	ue = ua + n;
	while (ua < ue)
	{
		c = m[*ua++];
		if (d = c - m[*ub++])
			return d;
		if (!c)
			return 0;
	}
	return 0;
#endif
}

#endif
