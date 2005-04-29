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
 * return scaled number n
 * string width is 5 chars or less
 * if m>1 then n divided by m before scaling
 */

#include <ast.h>

char*
fmtnum(register unsigned long n, int m)
{
	register int		i;
	register unsigned long	r;
	char*			buf;
	int			z;

	char			suf[2];

	if (m > 1)
	{
		r = n;
		n /= m;
		r -= n;
	}
	else
		r = 0;
	suf[1] = 0;
	if (n < 1024)
		suf[0] = 0;
	else if (n < 1024 * 1024)
	{
		suf[0] = 'k';
		r = ((n % 1024) * 100) / 1024;
		n /= 1024;
	}
	else if (n < 1024 * 1024 * 1024)
	{
		suf[0] = 'm';
		r = ((n % (1024 * 1024)) * 100) / (1024 * 1024);
		n /= 1024 * 1024;
	}
	else
	{
		suf[0] = 'g';
		r = ((n % (1024 * 1024 * 1024)) * 100) / (1024 * 1024 * 1024);
		n /= 1024 * 1024 * 1024;
	}
	if (r)
	{
		if (n >= 100)
			r = 0;
		else if (n >= 10)
		{
			i = 1;
			if (r >= 10)
				r /= 10;
		}
		else
			i = 2;
	}
	buf = fmtbuf(z = 8);
	if (r)
		sfsprintf(buf, z, "%lu.%0*lu%s", n, i, r, suf);
	else
		sfsprintf(buf, z, "%lu%s", n, suf);
	return buf;
}
