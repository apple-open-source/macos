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
 * return pointer to formatted clock() tics t
 * return value length is at most 6
 */

#include <ast.h>
#include <tm.h>

char*
fmtclock(register Sfulong_t t)
{
	register int		u;
	char*			buf;
	int			z;

	if (t == 0)
		return "0";
	if (t == ((Sfulong_t)~0))
		return "%";
	t = (t * 1000000) / CLOCKS_PER_SEC;
	if (t < 1000)
		u = 'u';
	else if ((t /= 1000) < 1000)
		u = 'm';
	else
		return fmtelapsed(t / 10, 100);
	buf = fmtbuf(z = 7);
	sfsprintf(buf, z, "%I*u%cs", sizeof(t), t, u);
	return buf;
}
