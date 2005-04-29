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

#if _lib_setsid

NoN(setsid)

#else

#include <ast_tty.h>
#include <error.h>

/*
 * become new process group leader and drop control tty
 */

pid_t
setsid(void)
{
	int	pg;
#ifdef TIOCNOTTY
	int	fd;
#endif

	/*
	 * become a new process group leader
	 */

	if ((pg = getpid()) == getpgrp())
	{
		errno = EPERM;
		return(-1);
	}
	setpgid(pg, pg);
#ifdef TIOCNOTTY

	/*
	 * drop the control tty
	 */

	if ((fd = open("/dev/tty", O_RDONLY)) >= 0)
	{
		ioctl(fd, TIOCNOTTY, 0);
		close(fd);
	}
#else

	/*
	 * second child in s5 to avoid reacquiring the control tty
	 */

#if _lib_fork && HUH920711 /* some s5's botch this */
	switch (fork())
	{
	case -1:
		exit(1);
	case 0:
		break;
	default:
		exit(0);
	}
#endif

#endif
	return(pg);
}

#endif
