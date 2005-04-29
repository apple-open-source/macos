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
 * -last fcntl
 */

#include <ast.h>

#ifndef fcntl

NoN(fcntl)

#else

#include <ls.h>
#include <ast_tty.h>
#include <error.h>

#if F_SETFD >= _ast_F_LOCAL
#if _sys_filio
#include <sys/filio.h>
#endif
#endif

#if _lib_fcntl
#undef	fcntl
extern int	fcntl(int, int, ...);
#endif

int
_ast_fcntl(int fd, int op, ...)
{
	int		n;
	int		save_errno;
	struct stat	st;
	va_list		ap;

	save_errno = errno;
	va_start(ap, op);
	if (op >= _ast_F_LOCAL) switch (op)
	{
#if F_DUPFD >= _ast_F_LOCAL
	case F_DUPFD:
		n = va_arg(ap, int);
		op = dup2(fd, n);
		break;
#endif
#if F_GETFL >= _ast_F_LOCAL
	case F_GETFL:
		op = fstat(fd, &st);
		break;
#endif
#if F_SETFD >= _ast_F_LOCAL && defined(FIOCLEX)
	case F_SETFD:
		n = va_arg(ap, int);
		op = ioctl(fd, n == FD_CLOEXEC ? FIOCLEX : FIONCLEX, 0);
		break;
#endif
	default:
		errno = EINVAL;
		op = -1;
		break;
	}
	else
#if _lib_fcntl
	op = fcntl(fd, op, va_arg(ap, int));
#else
	{
		errno = EINVAL;
		op = -1;
	}
#endif
	va_end(ap);
	return(op);
}

#endif
