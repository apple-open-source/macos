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
 * single dir support for pathaccess()
 */

#include <ast.h>

char*
pathcat(char* path, register const char* dirs, int sep, const char* a, register const char* b)
{
	register char*	s;

	s = path;
	while (*dirs && *dirs != sep)
		*s++ = *dirs++;
	if (s != path)
		*s++ = '/';
	if (a)
	{
		while (*s = *a++)
			s++;
		if (b)
			*s++ = '/';
	}
	else if (!b)
		b = ".";
	if (b)
		while (*s++ = *b++);
	return *dirs ? (char*)++dirs : 0;
}
