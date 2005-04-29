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
 *  strsort - sort an array pointers using fn
 *
 *	fn follows strcmp(3) conventions
 *
 *   David Korn
 *   AT&T Bell Laboratories
 *
 *  derived from Bourne Shell
 */

#include <ast.h>

void
strsort(char** argv, int n, int(*fn)(const char*, const char*))
{
	register int 	i;
	register int 	j;
	register int 	m;
	register char**	ap;
	char*		s;
	int 		k;

	for (j = 1; j <= n; j *= 2);
	for (m = 2 * j - 1; m /= 2;)
		for (j = 0, k = n - m; j < k; j++)
			for (i = j; i >= 0; i -= m)
			{
				ap = &argv[i];
				if ((*fn)(ap[m], ap[0]) >= 0) break;
				s = ap[m];
				ap[m] = ap[0];
				ap[0] = s;
			}
}
