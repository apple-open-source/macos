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
 * ccmapchr(ccmap(CC_NATIVE,CC_ASCII),c) and strcmp
 */

#include <ast.h>
#include <ccode.h>

#if _lib_stracmp

NoN(stracmp)

#else

#include <ctype.h>

#undef	stracmp

int
stracmp(const char* aa, const char* ab)
{
	register unsigned char*	a;
	register unsigned char*	b;
	register unsigned char*	m;
	register int		c;
	register int		d;

	if (!(m = ccmap(CC_NATIVE, CC_ASCII)))
		return strcmp(aa, ab);
	a = (unsigned char*)aa;
	b = (unsigned char*)ab;
	for (;;)
	{
		c = m[*a++];
		if (d = c - m[*b++])
			return d;
		if (!c)
			return 0;
	}
}

#endif
