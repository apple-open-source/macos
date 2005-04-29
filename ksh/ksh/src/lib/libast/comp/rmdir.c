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

#if _lib_rmdir

NoN(rmdir)

#else

#include <ls.h>
#include <error.h>

int
rmdir(const char* path)
{
	register int	n;
	struct stat	st;
	char*		av[3];

	static char*	cmd[] = { "/bin/rmdir", "/usr/5bin/rmdir", 0 };

	if (stat(path, &st) < 0) return(-1);
	if (!S_ISDIR(st.st_mode))
	{
		errno = ENOTDIR;
		return(-1);
	}
	av[0] = "rmdir";
	av[1] = path;
	av[2] = 0;
	for (n = 0; n < elementsof(cmd); n++)
		if (procclose(procopen(cmd[n], av, NiL, NiL, 0)) != -1)
			break;
	n = errno;
	if (access(path, F_OK) < 0)
	{
		errno = n;
		return(0);
	}
	errno = EPERM;
	return(-1);
}

#endif
