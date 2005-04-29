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

#if _lib_memccpy && !_UWIN

NoN(memccpy)

#else

/*
 * Copy s2 to s1, stopping if character c is copied. Copy no more than n bytes.
 * Return a pointer to the byte after character c in the copy,
 * or 0 if c is not found in the first n bytes.
 */

void*
memccpy(void* as1, const void* as2, register int c, size_t n)
{
	register char*		s1 = (char*)as1;
	register const char*	s2 = (char*)as2;
	register const char*	ep = s2 + n;

	while (s2 < ep)
		if ((*s1++ = *s2++) == c)
			return(s1);
	return(0);
}

#endif
