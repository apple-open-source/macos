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
 * struniq - uniq a sorted argv
 * 0 sentinel is neither expected nor restored
 *
 * Glenn Fowler
 * David Korn
 * AT&T Labs Research
 */

#include <ast.h>

int
struniq(char** argv, int n)
{
	register char**	ao;
	register char**	an;
	register char**	ae;

	ao = an = argv;
	ae = ao + n;
	while (++an < ae)
	{
		while (streq(*ao, *an))
			if (++an >= ae)
				return ao - argv + 1;
		*++ao = *an;
	}
	return ao - argv + 1;
}
