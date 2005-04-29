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
 * return formatted <magicid.h> version string
 */

#include <ast.h>

char*
fmtversion(register unsigned long v)
{
	register char*	cur;
	register char*	end;
	char*		buf;
	int		n;

	buf = cur = fmtbuf(n = 18);
	end = cur + n;
	if (v >= 19700101L)
		sfsprintf(cur, end - cur, "%04lu-%02lu-%02lu", (v / 10000) % 10000, (v / 100) % 100, v % 100);
	else
	{
		if (n = (v >> 24) & 0xff)
			cur += sfsprintf(cur, end - cur, "%d.", n);
		if (n = (v >> 16) & 0xff)
			cur += sfsprintf(cur, end - cur, "%d.", n);
		sfsprintf(cur, end - cur, "%ld.%ld", (v >> 8) & 0xff, v & 0xff);
	}
	return buf;
}
