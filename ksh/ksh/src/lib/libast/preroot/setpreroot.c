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
 * force current command to run under dir preroot
 */

#include <ast.h>
#include <preroot.h>

#if FS_PREROOT

#include <option.h>

void
setpreroot(register char** argv, const char* dir)
{
	register char*	s;
	register char**	ap;
	int		argc;
	char*		cmd;
	char**		av;
	char		buf[PATH_MAX];

	if ((argv || (argv = opt_info.argv)) && (dir || (dir = getenv(PR_BASE)) && *dir) && !ispreroot(dir) && (*(cmd = *argv++) == '/' || (cmd = pathpath(buf, cmd, NiL, PATH_ABSOLUTE|PATH_REGULAR|PATH_EXECUTE))))
	{
		argc = 3;
		for (ap = argv; *ap++; argc++);
		if (av = newof(0, char*, argc, 0))
		{
			ap = av;
			*ap++ = PR_COMMAND;
			*ap++ = dir;
			*ap++ = cmd;
			while (*ap++ = *argv++);
			if (!(s = getenv(PR_SILENT)) || !*s)
			{
				sfprintf(sfstderr, "+");
				ap = av;
				while (s = *ap++)
					sfprintf(sfstderr, " %s", s);
				sfprintf(sfstderr, "\n");
				sfsync(sfstderr);
			}
			execv(*av, av);
			free(av);
		}
	}
}

#else

NoN(setpreroot)

#endif
