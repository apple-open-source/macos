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

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide strlcat
#else
#define strlcat		______strlcat
#endif

#include <ast.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide strlcat
#else
#undef	strlcat
#endif

/*
 * append at t onto s limiting total size of s to n
 * s 0 terminated if n>0
 * min(n,strlen(s))+strlen(t) returned
 */

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern size_t
strlcat(register char* s, register const char* t, register size_t n)
{
	const char*	o = t;

	if (n)
	{
		while (--n && *s)
			s++;
		if (n)
			do
			{
				if (!--n)
				{
					*s = 0;
					break;
				}
			} while (*s++ = *t++);
		else
			*s = 0;
	}
	if (!n)
		while (*t++);
	return t - o - 1;
}
