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
 * AT&T Research
 *
 * string vector load support
 */

#include <ast.h>
#include <ls.h>
#include <vecargs.h>

/*
 * load a string vector from lines in file
 */

char**
vecfile(const char* file)
{
	register int	n;
	register char*	buf;
	register char**	vec;
	int		fd;
	struct stat	st;

	vec = 0;
	if ((fd = open(file, O_RDONLY)) >= 0)
	{
		if (!fstat(fd, &st) && S_ISREG(st.st_mode) && (n = st.st_size) > 0 && (buf = newof(0, char, n + 1, 0)))
		{
			if (read(fd, buf, n) == n)
			{
				buf[n] = 0;
				vec = vecload(buf);
			}
			if (!vec) free(buf);
		}
		close(fd);
	}
	return(vec);
}
