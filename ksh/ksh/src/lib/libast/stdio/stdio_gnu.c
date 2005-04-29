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
 * gnu stdio extensions
 */

#include "stdhdr.h"

int
fcloseall(void)
{
	return sfsync(NiL) < 0 ? -1 : 0;
}

Sfio_t*
fmemopen(void* buf, size_t size, const char* mode)
{
	return sfnew(NiL, buf, size, -1, SF_STRING|_sftype(mode, NiL, NiL));
}

ssize_t
__getdelim(char** pbuf, size_t* psize, int del, Sfio_t* sp)
{
	char*	s;
	size_t	n;
	size_t	m;

	if (!(s = sfgetr(sp, del, 1)))
		return -1;
	n = sfsize(sp);
	m = n + 1;
	if (!*pbuf || m > *psize)
	{
		m = roundof(m, 1024);
		if (!(*pbuf = newof(*pbuf, char, m, 0)))
			return -1;
		*psize = m;
	}
	memcpy(*pbuf, s, n);
	return n;
}

ssize_t
getdelim(char** pbuf, size_t* psize, int del, Sfio_t* sp)
{
	return __getdelim(pbuf, psize, del, sp);
}

ssize_t
getline(char** pbuf, size_t* psize, Sfio_t* sp)
{
	return __getdelim(pbuf, psize, '\n', sp);
}
