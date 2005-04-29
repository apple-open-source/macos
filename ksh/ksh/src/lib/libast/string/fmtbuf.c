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

#include <ast.h>

/*
 * return small format buffer chunk of size n
 * spin lock for thread access
 * format buffers are short lived
 */

static char		buf[16 * 1024];
static char*		nxt = buf;
static int		lck = -1;

char*
fmtbuf(size_t n)
{
	register char*	cur;

	while (++lck)
		lck--;
	if (n > (&buf[elementsof(buf)] - nxt))
	{
		if (n > elementsof(buf))
			return 0;
		nxt = buf;
	}
	cur = nxt;
	nxt += n;
	lck--;
	return cur;
}
