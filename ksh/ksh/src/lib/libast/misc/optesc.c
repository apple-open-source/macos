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
 * escape optget() special chars in s and write to sp
 */

#include <optlib.h>
#include <ctype.h>

int
optesc(Sfio_t* sp, register const char* s)
{
	register const char*	m;
	register int		c;

	if (*s == '[' && *(s + 1) == '+' && *(s + 2) == '?')
	{
		c = strlen(s);
		if (s[c - 1] == ']')
		{
			sfprintf(sp, "%-.*s", c - 4, s + 3);
			return 0;
		}
	}
	while (c = *s++)
	{
		if (isalnum(c))
		{
			for (m = s - 1; isalnum(*s); s++);
			if (isalpha(c) && *s == '(' && isdigit(*(s + 1)) && *(s + 2) == ')')
			{
				sfputc(sp, '\b');
				sfwrite(sp, m, s - m);
				sfputc(sp, '\b');
				sfwrite(sp, s, 3);
				s += 3;
			}
			else
				sfwrite(sp, m, s - m);
		}
		else if (c == '-' && *s == '-' || c == '<')
		{
			m = s - 1;
			if (c == '-')
				s++;
			else if (*s == '/')
				s++;
			while (isalnum(*s))
				s++;
			if (c == '<' && *s == '>' || isspace(*s) || *s == 0 || *s == '=' || *s == ':' || *s == ';' || *s == '.' || *s == ',')
			{
				sfputc(sp, '\b');
				sfwrite(sp, m, s - m);
				sfputc(sp, '\b');
			}
			else
				sfwrite(sp, m, s - m);
		}
		else
		{
			if (c == ']')
				sfputc(sp, c);
			sfputc(sp, c);
		}
	}
	return 0;
}
