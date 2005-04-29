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
 */

#include <ast.h>
#include <ctype.h>
#include <hash.h>

/*
 * parse option expression in s using options in tab with element size siz
 * siz==0 implies Hash_table_t*tab
 * options match
 *
 *	[no]name[[:]=['"]value["']][, ]...
 *
 * f is called for each option
 *
 *	(*f)(void* a, char* p, int n, char* v)
 *
 *	a	from stropt
 *	p	matching tab entry, or name if no table
 *	n	0 if option had ``no'' prefix, -1 if :=, 1 otherwise
 *	v	option value pointer
 *
 * for unmatched options p is 0 and v is the offending option
 *
 * names in s may be shorter than tab names
 * longer names must have a prefix that matches a tab name
 * the first match is returned
 * \ escapes value using chresc()
 */

int
stropt(const char* as, const void* tab, int siz, int(*f)(void*, const void*, int, const char*), void* a)
{
	register int	c;
	register char*	s;
	register char*	v;
	register char*	t;
	char**		p;
	char*		u;
	char*		x;
	char*		e;
	int		n;
	int		ql;
	int		qr;
	int		qc;

	if (!as) n = 0;
	else if (!(x = s = strdup(as))) n = -1;
	else
	{
		for (;;)
		{
			while (isspace(*s) || *s == ',') s++;
			if (*s == 'n' && *(s + 1) == 'o')
			{
				s += 2;
				n = 0;
			}
			else n = 1;
			if (!*s)
			{
				n = 0;
				break;
			}
			if (tab)
			{
				if (!siz)
				{
					v = s;
					while ((c = *v++) && !isspace(c) && c != '=' && c != ':' && c != ',');
					*--v = 0;
					t = (p = (char**)hashget((Hash_table_t*)tab, s)) ? s : (char*)0;
					if ((*v = c) == ':' && *(v + 1) == '=')
					{
						v++;
						n = -1;
					}
				}
				else for (p = (char**)tab; t = *p; p = (char**)((char*)p + siz))
				{
					for (v = s; *t && *t++ == *v; v++);
					if (!*t || isspace(*v) || *v == ',' || *v == '=') break;
					if (*v == ':' && *(v + 1) == '=')
					{
						v++;
						n = -1;
						break;
					}
				}
				if (!t)
				{
					u = v = s;
					p = 0;
				}
			}
			else
			{
				p = (char**)(v = s);
				t = 0;
			}
			while (*v && !isspace(*v) && *v != '=' && *v != ',')
				if (*v++ == ':' && *v == '=')
				{
					if (!t) *(v - 1) = 0;
					n = -n;
					break;
				}
			if (*v == '=')
			{
				if (!t) *v = 0;
				t = s = ++v;
				ql = qr = 0;
				while (c = *s++)
				{
					if (c == '\\')
					{
						*t++ = chresc(s - 1, &e);
						s = e;
					}
					else if (c == qr)
					{
						if (qr != ql)
							*t++ = c;
						if (--qc <= 0)
							qr = ql = 0;
					}
					else if (c == ql)
					{
						*t++ = c;
						qc++;
					}
					else if (qr)
						*t++ = c;
					else if (c == ',' || isspace(c))
						break;
					else if (c == '"' || c == '\'')
					{
						ql = qr = c;
						qc = 1;
					}
					else
					{
						*t++ = c;
						if (c == '{')
						{
							ql = c;
							qr = '}';
							qc = 1;
						}
						else if (c == '(')
						{
							ql = c;
							qr = ')';
							qc = 1;
						}
					}
				}
				*t = 0;
			}
			else
			{
				s = v;
				c = *s;
				*s++ = 0;
			}
			n = p ? (*f)(a, p, n, v) : (*f)(a, p, v - u, u);
			if (n || !c) break;
		}
		free(x);
	}
	return(n);
}
