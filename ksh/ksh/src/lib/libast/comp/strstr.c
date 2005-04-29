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
__STDPP__directive pragma pp:hide strstr
#else
#define strstr		______strstr
#endif

#include <ast.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide strstr
#else
#undef	strstr
#endif

#if _lib_strstr

NoN(strstr)

#else

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern char*
strstr(register const char* s1, register const char* s2)
{
	register int		c1;
	register int		c2;
	register const char*	t1;
	register const char*	t2;
	
	if (s2)
	{
		if (!*s2)
			return (char*)s1;
		c2 = *s2++;
		while (c1 = *s1++)
			if (c1 == c2)
			{
				t1 = s1;
				t2 = s2;
				do
				{
					if (!*t2)
						return (char*)s1 - 1;
				} while (*t1++ == *t2++);
			}
	}
	return 0;
}

#endif
