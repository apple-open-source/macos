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
 * string vector load support
 */

#include <ast.h>
#include <vecargs.h>

/*
 * load a string vector from lines in buf
 * buf may be modified on return
 *
 * each line in buf is treated as a new vector element
 * lines with # as first char are comments
 * \ as the last char joins consecutive lines
 *
 * the vector ends with a 0 sentinel
 *
 * the string array pointer is returned
 */

char**
vecload(char* buf)
{
	register char*	s;
	register int	n;
	register char**	p;
	char**		vec;

	vec = 0;
	n = (*buf == '#') ? -1 : 0;
	for (s = buf;; s++)
	{
		if (*s == '\n')
		{
			if (s > buf && *(s - 1) == '\\') *(s - 1) = *s = ' ';
			else
			{
				*s = 0;
				if (*(s + 1) != '#')
				{
					n++;
					if (!*(s + 1)) break;
				}
			}
		}
		else if (!*s)
		{
			n++;
			break;
		}
	}
	if (n < 0) n = 0;
	if (p = newof(0, char*, n + 3, 0))
	{
		*p++ = s = buf;
		vec = ++p;
		if (n > 0) for (;;)
		{
			if (*s != '#')
			{
				*p++ = s;
				if (--n <= 0) break;
			}
			while (*s) s++;
			s++;
		}
		*p = 0;
		*(vec - 1) = (char*)p;
	}
	return(vec);
}
