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
 * put name=value in the environment
 * pointer to value returned
 * environ==0 is ok
 *
 *	setenviron("N=V")	add N=V
 *	setenviron("N")		delete N
 *	setenviron(0)		expect more (pre-fork optimization)
 *
 * _ always placed at the top
 */

#include <ast.h>
#include <fs3d.h>

#define INCREMENT	16		/* environ increment		*/

char*
setenviron(const char* akey)
{
	static char**	envv;		/* recorded environ		*/
	static char**	next;		/* next free slot		*/
	static char**	last;		/* last free slot (0)		*/
	static char	ok[] = "";	/* delete/optimization ok return*/

	char*		key = (char*)akey;
	register char**	v = environ;
	register char**	p = envv;
	register char*	s;
	register char*	t;
	int		n;

	ast.env_serial++;
	if (p && !v)
	{
		environ = next = p;
		*++next = 0;
	}
	else if (p != v || !v)
	{
		if (v)
		{
			while (*v++);
			n = v - environ + INCREMENT;
			v = environ;
		}
		else
			n = INCREMENT;
		if (!p || (last - p + 1) < n)
		{
			if (!p && fs3d(FS3D_TEST))
			{
				/*
				 * kick 3d initialization
				 */

				close(open(".", O_RDONLY));
				v = environ;
			}
			if (!(p = newof(p, char*, n, 0)))
				return 0;
			last = p + n - 1;
		}
		envv = environ = p;
		if (v && v[0] && v[0][0] == '_' && v[0][1] == '=')
			*p++ = *v++;
		else
			*p++ = "_=";
		if (!v)
			*p = 0;
		else
			while (*p = *v++)
				if (p[0][0] == '_' && p[0][1] == '=')
					envv[0] = *p;
				else
					p++;
		next = p;
		p = envv;
	}
	else if (next == last)
	{
		n = last - v + INCREMENT + 1;
		if (!(p = newof(p, char*, n, 0)))
			return 0;
		last = p + n - 1;
		next = last - INCREMENT;
		envv = environ = p;
	}
	if (!key)
		return ok;
	for (; s = *p; p++)
	{
		t = key;
		do
		{
			if (!*t || *t == '=')
			{
				if (*s == '=')
				{
					if (!*t)
					{
						v = p++;
						while (*v++ = *p++);
						next--;
						return ok;
					}
					*p = key;
					return (s = strchr(key, '=')) ? s + 1 : (char*)0;
				}
				break;
			}
		} while (*t++ == *s++);
	}
	if (!(s = strchr(key, '=')))
		return ok;
	p = next;
	*++next = 0;
	*p = key;
	return s + 1;
}
