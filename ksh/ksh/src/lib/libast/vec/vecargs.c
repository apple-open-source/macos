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
 * string vector argv insertion
 */

#include <ast.h>
#include <vecargs.h>
#include <ctype.h>

/*
 * insert the string vector vec between
 * (*argvp)[0] and (*argvp)[1], sliding (*argvp)[1] ... over
 * null and blank args are deleted
 *
 * vecfree always called
 *
 * -1 returned if insertion failed
 */

int
vecargs(register char** vec, int* argcp, char*** argvp)
{
	register char**	argv;
	register char**	oargv;
	char**		ovec;
	char*		s;
	int		num;

	if (!vec) return(-1);
	if ((num = (char**)(*(vec - 1)) - vec) > 0)
	{
		if (!(argv = newof(0, char*, num + *argcp + 1, 0)))
		{
			vecfree(vec, 0);
			return(-1);
		}
		oargv = *argvp;
		*argvp = argv;
		*argv++ = *oargv++;
		ovec = vec;
		while (s = *argv = *vec++)
		{
			while (isspace(*s)) s++;
			if (*s) argv++;
		}
		vecfree(ovec, 1);
		while (*argv = *oargv++) argv++;
		*argcp = argv - *argvp;
	}
	else vecfree(vec, 0);
	return(0);
}
