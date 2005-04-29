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
 * parse elapsed time in 1/n secs from s
 * compatible with fmtelapsed()
 * also handles ps [day-][hour:]min:sec
 * if e!=0 then it is set to first unrecognized char
 */

#include <ast.h>
#include <ctype.h>

unsigned long
strelapsed(register const char* s, char** e, int n)
{
	register int		c;
	register unsigned long	v;
	unsigned long		t = 0;
	int			f = 0;
	int			p = 0;
	int			m;
	const char*		last;

	for (;;)
	{
		while (isspace(*s) || *s == '_')
			s++;
		if (!*(last = s))
			break;
		v = 0;
		while ((c = *s++) >= '0' && c <= '9')
			v = v * 10 + c - '0';
		v *= n;
		if (c == '.')
			for (m = n; (c = *s++) >= '0' && c <= '9';)
				f += (m /= 10) * (c - '0');
		if (c == '%')
		{
			t = ~t;
			last = s;
		}
		if (s == last)
			break;
		if (!p)
			while (isspace(c) || c == '_')
				c = *s++;
		switch (c)
		{
		case 'S':
			v *= 20 * 12 * 4 * 7 * 24 * 60 * 60;
			break;
		case 'y':
		case 'Y':
			v *= 12 * 4 * 7 * 24 * 60 * 60;
			break;
		case 'M':
			v *= 4 * 7 * 24 * 60 * 60;
			break;
		case 'w':
			v *= 7 * 24 * 60 * 60;
			break;
		case '-':
			p = 1;
			/*FALLTHROUGH*/
		case 'd':
			v *= 24 * 60 * 60;
			break;
		case 'h':
			v *= 60 * 60;
			break;
		case ':':
			p = 1;
			v *= strchr(s, ':') ? (60 * 60) : 60;
			break;
		case 'm':
			if (*s == 'o')
				v *= 4 * 7 * 24 * 60 * 60;
			else
				v *= 60;
			break;
		case 0:
			s--;
			/*FALLTHROUGH*/
		case 's':
			if (*s == 'c')
				v *= 20 * 12 * 4 * 7 * 24 * 60 * 60;
			else
			{
				v += f;
				f = 0;
			}
			break;
		default:
			if (p)
			{
				last = s - 1;
				t += v + f;
			}
			goto done;
		}
		t += v;
		while (isalpha(*s))
			s++;
	}
 done:
	if (e)
		*e = (char*)last;
	return t;
}
