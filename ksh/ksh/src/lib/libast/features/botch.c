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
 * generate ast traps for botched standard prototypes
 */

#include <sys/types.h>

#include "FEATURE/lib"
#include "FEATURE/types"

extern int		getgroups(int, gid_t*);
extern int		printf(const char*, ...);

main()
{
#if _lib_getgroups
	if (sizeof(int) > sizeof(gid_t))
	{
		int	n;
		int	i;
		int	r;
		gid_t	groups[32 * sizeof(int) / sizeof(gid_t)];

		r = sizeof(int) / sizeof(gid_t);
		if ((n = getgroups((sizeof(groups) / sizeof(groups[0])) / r, groups)) > 0)
			for (i = 1; i <= n; i++)
			{
				groups[i] = ((gid_t)0);
				if (getgroups(i, groups) != i)
					goto botched;
				if (groups[i] != ((gid_t)0))
					goto botched;
				groups[i] = ((gid_t)-1);
				if (getgroups(i, groups) != i)
					goto botched;
				if (groups[i] != ((gid_t)-1))
					goto botched;
			}
	}
	return(0);
 botched:
	printf("#undef	getgroups\n");
	printf("#define getgroups	_ast_getgroups /* implementation botches gid_t* arg */\n");
#endif
	return(0);
}
