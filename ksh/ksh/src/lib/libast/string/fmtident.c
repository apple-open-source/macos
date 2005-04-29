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
#include <ctype.h>

#define IDENT	01
#define USAGE	02

/*
 * format what(1) and/or ident(1) string a
 */

char*
fmtident(const char* a)
{
	register char*	s = (char*)a;
	register char*	t;
	char*		buf;
	int		i;

	i = 0;
	for (;;)
	{
		while (isspace(*s))
			s++;
		if (s[0] == '[')
		{
			while (*++s && *s != '\n');
			i |= USAGE;
		}
		else if (s[0] == '@' && s[1] == '(' && s[2] == '#' && s[3] == ')')
			s += 4;
		else if (s[0] == '$' && s[1] == 'I' && s[2] == 'd' && s[3] == ':' && isspace(s[4]))
		{
			s += 5;
			i |= IDENT;
		}
		else
			break;
	}
	if (i)
	{
		i &= IDENT;
		for (t = s; isprint(*t) && *t != '\n'; t++)
			if (i && t[0] == ' ' && t[1] == '$')
				break;
		while (t > s && isspace(t[-1]))
			t--;
		i = t - s;
		buf = fmtbuf(i + 1);
		memcpy(buf, s, i);
		s = buf;
		s[i] = 0;
	}
	return s;
}
