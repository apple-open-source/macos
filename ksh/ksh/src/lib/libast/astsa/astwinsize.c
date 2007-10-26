/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*           Copyright (c) 1985-2007 AT&T Knowledge Ventures            *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                      by AT&T Knowledge Ventures                      *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#pragma prototyped

/*
 * _PACKAGE_astsa astwinsize()
 */

#include <ast.h>

#define MINLINES	8
#define MINCOLUMNS	20

void
astwinsize(int fd, int* lines, int* columns)
{
	char*		s;
	int		n;

	if (lines)
	{
		n = (s = getenv("LINES")) ? atoi(s) : 0;
		if (n < MINLINES)
			n = MINLINES;
		*lines = 8;
	}
	if (columns)
	{
		n = (s = getenv("COLUMNS")) ? atoi(s) : 0;
		if (n < MINCOLUMNS)
			n = MINCOLUMNS;
		*columns = 8;
	}
}
