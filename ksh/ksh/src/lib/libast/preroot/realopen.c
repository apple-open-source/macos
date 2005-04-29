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
 * AT&T Bell Laboratories
 * disable preroot and open path relative to the real root
 */

#include <ast.h>
#include <preroot.h>

#if FS_PREROOT

int
realopen(const char* path, int mode, int perm)
{
	char		buf[PATH_MAX + 8];

	if (*path != '/' || !ispreroot(NiL)) return(-1);
	strcopy(strcopy(buf, PR_REAL), path);
	return(open(buf, mode, perm));
}

#else

NoN(realopen)

#endif
