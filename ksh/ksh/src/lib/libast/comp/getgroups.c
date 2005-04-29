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

#if !defined(getgroups) && defined(_lib_getgroups)

NoN(getgroups)

#else

#include <error.h>

#if defined(getgroups)
#undef	getgroups
#define	ast_getgroups	_ast_getgroups
#define botched		1
extern int		getgroups(int, int*);
#else
#define ast_getgroups	getgroups
#endif

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern int
ast_getgroups(int len, gid_t* set)
{
#if botched
#if NGROUPS_MAX < 1
#undef	NGROUPS_MAX
#define NGROUPS_MAX	1
#endif
	register int	i;
	int		big[NGROUPS_MAX];
#else
#undef	NGROUPS_MAX
#define NGROUPS_MAX	1
#endif
	if (!len) return(NGROUPS_MAX);
	if (len < 0 || !set)
	{
		errno = EINVAL;
		return(-1);
	}
#if botched
	len = getgroups(len > NGROUPS_MAX ? NGROUPS_MAX : len, big);
	for (i = 0; i < len; i++)
		set[i] = big[i];
	return(len);
#else
	*set = getgid();
	return(1);
#endif
}

#endif
