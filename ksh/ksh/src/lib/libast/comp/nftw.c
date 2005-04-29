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
 * nftw implementation
 */

#include <ast.h>
#include <ftw.h>

static int	nftw_flags;
static int	(*nftw_userf)(const char*, const struct stat*, int, struct FTW*);

static int
nftw_user(Ftw_t* ftw)
{
	register int	n = ftw->info;
	struct FTW	nftw;
	struct stat	st;

	if (n & (FTW_C|FTW_NX))
		n = FTW_DNR;
	else if ((n & FTW_SL) && (!(nftw_flags & FTW_PHYSICAL) || stat(ftw->path, &st)))
		n = FTW_SLN;
	nftw.base = ftw->pathlen - ftw->namelen;
	nftw.level = ftw->level;
	nftw.quit = 0;
	n = (*nftw_userf)(ftw->path, &ftw->statb, n, &nftw);
	ftw->status = nftw.quit;
	return n;
}

int
nftw(const char* path, int(*userf)(const char*, const struct stat*, int, struct FTW*), int depth, int flags)
{
	NoP(depth);
	nftw_userf = userf;
	if (flags & FTW_CHDIR) flags &= ~FTW_DOT;
	else flags |= FTW_DOT;
	nftw_flags = flags;
	return ftwalk(path, nftw_user, flags, NiL);
}
