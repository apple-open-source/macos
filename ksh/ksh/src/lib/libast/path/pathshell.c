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
 * G. S. Fowler
 * D. G. Korn
 * AT&T Bell Laboratories
 *
 * shell library support
 */

#include <ast.h>
#include <sys/stat.h>

/*
 * return pointer to the full path name of the shell
 *
 * SHELL is read from the environment and must start with /
 *
 * if set-uid or set-gid then the executable and its containing
 * directory must not be owned by the real user/group
 *
 * root/administrator has its own test
 *
 * astconf("SHELL",NiL,NiL) is returned by default
 *
 * NOTE: csh is rejected because the bsh/csh differentiation is
 *       not done for `csh script arg ...'
 */

char*
pathshell(void)
{
	register char*	sh;
	int		ru;
	int		eu;
	int		rg;
	int		eg;
	struct stat	st;

	static char*	val;

	if ((sh = getenv("SHELL")) && *sh == '/' && strmatch(sh, "*/(sh|*[!cC]sh)?(-+([a-zA-Z0-9.]))?(.exe)"))
	{
		if (!(ru = getuid()) || !access("/bin", W_OK))
		{
			if (stat(sh, &st))
				goto defshell;
			if (ru != st.st_uid && !strmatch(sh, "?(/usr)?(/local)/?(l)bin/?([a-z])sh?(.exe)"))
				goto defshell;
		}
		else
		{
			eu = geteuid();
			rg = getgid();
			eg = getegid();
			if (ru != eu || rg != eg)
			{
				char*	s;
				char	dir[PATH_MAX];

				s = sh;
				for (;;)
				{
					if (stat(s, &st))
						goto defshell;
					if (ru != eu && st.st_uid == ru)
						goto defshell;
					if (rg != eg && st.st_gid == rg)
						goto defshell;
					if (s != sh)
						break;
					if (strlen(s) >= sizeof(dir))
						goto defshell;
					strcpy(dir, s);
					if (!(s = strrchr(dir, '/')))
						break;
					*s = 0;
					s = dir;
				}
			}
		}
		return sh;
	}
 defshell:
	if (!(sh = val))
	{
		if (!*(sh = astconf("SHELL", NiL, NiL)) || *sh != '/' || access(sh, X_OK) || !(sh = strdup(sh)))
			sh = "/bin/sh";
		val = sh;
	}
	return sh;
}
