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

#if _lib_memcpy

NoN(memcpy)

#else

#undef	memcpy

#if _lib_bcopy

extern void	bcopy(void*, void*, size_t);

void*
memcpy(void* s1, void* s2, size_t n)
{
	bcopy(s2, s1, n);
	return(s1);
}

#else

void*
memcpy(void* as1, const void* as2, register size_t n)
{
	register char*		s1 = (char*)as1;
	register const char*	s2 = (const char*)as2;

	while (n-- > 0)
		*s1++ = *s2++;
	return(as1);
}

#endif

#endif
