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
 * in place replace of first occurrence of /match/ with /replace/ in path
 * end of path returned
 */

#include <ast.h>

char*
pathrepl(register char* path, const char* match, register const char* replace)
{
	register const char*	m = match;
	register const char*	r;
	char*			t;

	if (!match) match = "";
	if (!replace) replace = "";
	if (streq(match, replace))
		return(path + strlen(path));
	for (;;)
	{
		while (*path && *path++ != '/');
		if (!*path) break;
		if (*path == *m)
		{
			t = path;
			while (*m && *m++ == *path) path++;
			if (!*m && *path == '/')
			{
				register char*	p;

				p = t;
				r = replace;
				while (p < path && *r) *p++ = *r++;
				if (p < path) while (*p++ = *path++);
				else if (*r && p >= path)
				{
					register char*	u;

					t = path + strlen(path);
					u = t + strlen(r);
					while (t >= path) *u-- = *t--;
					while (*r) *p++ = *r++;
				}
				else p += strlen(p) + 1;
				return(p - 1);
			}
			path = t;
			m = match;
		}
	}
	return(path);
}
