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
 * convert a 0 terminated character constant string to an int
 */

#include <ast.h>

int
chrtoi(register const char* s)
{
	register int	c;
	register int	n;
	register int	x;
	char*		p;

	c = 0;
	for (n = 0; n < sizeof(int) * CHAR_BIT; n += CHAR_BIT)
	{
		switch (x = *((unsigned char*)s++))
		{
		case '\\':
			x = chresc(s - 1, &p);
			s = (const char*)p;
			break;
		case 0:
			return(c);
		}
		c = (c << CHAR_BIT) | x;
	}
	return(c);
}
