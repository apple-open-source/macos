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
 * return number n scaled to metric powers of k { 1000 1024 }
 */

#include <ast.h>

char*
fmtscale(register Sfulong_t n, int k)
{
	register Sfulong_t	m;
	int			r;
	int			z;
	const char*		u;
	char*			buf;

	static const char	scale[] = "bKMGTPX";

	m = 0;
	u = scale;
	while (n > k && *(u + 1))
	{
		m = n;
		n /= k;
		u++;
	}
	buf = fmtbuf(z = 8);
	r = (m % k) / (k / 10 + 1);
	if (n > 0 && n < 10)
		sfsprintf(buf, z, "%I*u.%d%c", sizeof(n), n, r, *u);
	else
	{
		if (r >= 5)
			n++;
		sfsprintf(buf, z, "%I*u%c", sizeof(n), n, *u);
	}
	return buf;
}
