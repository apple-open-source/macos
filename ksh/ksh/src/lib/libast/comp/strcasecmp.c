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

#if _lib_strcasecmp

NoN(strcasecmp)

#else

#include <ctype.h>

#undef	strcasecmp

int
strcasecmp(register const char* a, register const char* b)
{
	register int	ac;
	register int	bc;
	register int	d;

	for (;;)
	{
		ac = *a++;
		if (isupper(ac))
			ac = tolower(ac);
		bc = *b++;
		if (isupper(bc))
			bc = tolower(bc);
		if (d = ac - bc)
			return d;
		if (!ac)
			return 0;
	}
}

#endif
