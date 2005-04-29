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
 * AT&T Bell Laboratories
 *
 * return base b representation for n
 * if p!=0 then base prefix is included
 * otherwise if n==0 or b==0 then output is signed base 10
 */

#include <ast.h>

#undef	fmtbasell

char*
fmtbasell(register _ast_intmax_t n, register int b, int p)
{
	char*	buf;
	int	z;

	buf = fmtbuf(z = 72);
	if (!p && (n == 0 || b == 0))
		sfsprintf(buf, z, "%I*d", sizeof(n), n);
	else
		sfsprintf(buf, z, p ? "%#..*I*u" : "%..*I*u", b, sizeof(n), n);
	return buf;
}

#undef	fmtbase

char*
fmtbase(long n, int b, int p)
{
	return fmtbasell((_ast_intmax_t)n, b, p);
}
