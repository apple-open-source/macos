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

#if _lib_rename

NoN(rename)

#else

#include <error.h>
#include <proc.h>

#ifdef EPERM

static int
mvdir(const char* from, const char* to)
{
	char*			argv[4];
	int			oerrno;

	static const char	mvdir[] = "/usr/lib/mv_dir";

	oerrno = errno;
	if (!access(mvdir, X_OK))
	{
		argv[0] = mvdir;
		argv[1] = from;
		argv[2] = to;
		argv[3] = 0;
		if (!procrun(argv[0], argv))
		{
			errno = oerrno;
			return 0;
		}
	}
	errno = EPERM;
	return -1;
}

#endif

int
rename(const char* from, const char* to)
{
	int	oerrno;
	int	ooerrno;

	ooerrno = errno;
	while (link(from, to))
	{
#ifdef EPERM
		if (errno == EPERM)
		{
			errno = ooerrno;
			return mvdir(from, to);
		}
#endif
		oerrno = errno;
		if (unlink(to))
		{
#ifdef EPERM
			if (errno == EPERM)
			{
				errno = ooerrno;
				return mvdir(from, to);
			}
#endif
			errno = oerrno;
			return -1;
		}
	}
	errno = ooerrno;
	return unlink(from);
}

#endif
