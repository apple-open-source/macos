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
 * -last 3 arg open
 */

#include <ast.h>

#if !defined(open) || !defined(_ast_O_LOCAL)

NoN(open)

#else

#undef	open

extern int	open(const char*, int, ...);

#include <ls.h>
#include <error.h>

#ifdef O_NOCTTY
#include <ast_tty.h>
#endif

int
_ast_open(const char* path, int op, ...)
{
	int		fd;
	int		mode;
	int		save_errno;
	struct stat	st;
	va_list		ap;

	save_errno = errno;
	va_start(ap, op);
	mode = (op & O_CREAT) ? va_arg(ap, int) : S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	va_end(ap);
	if (op & ~(_ast_O_LOCAL-1))
	{
		if (!(op & O_CREAT))
			op &= ~O_EXCL;
		for (;;)
		{
			if (op & O_TRUNC)
			{
				if ((op & O_EXCL) && !access(path, F_OK))
				{
					errno = EEXIST;
					return(-1);
				}
				if ((fd = creat(path, (op & O_EXCL) ? 0 : mode)) < 0)
					return(-1);
				if (op & O_EXCL)
				{
					if (fstat(fd, &st) || (st.st_mode & S_IPERM))
					{
						errno = EEXIST;
						close(fd);
						return(-1);
					}
#if _lib_fchmod
					if (mode && fchmod(fd, mode))
#else
					if (mode && chmod(path, mode))
#endif
						errno = save_errno;
				}
				if (op & O_RDWR)
				{
					close(fd);
					op &= ~(O_CREAT|O_TRUNC);
					continue;
				}
			}
			else if ((fd = open(path, op & (_ast_O_LOCAL-1), mode)) < 0)
			{
				if (op & O_CREAT)
				{
					op |= O_TRUNC;
					continue;
				}
				return(-1);
			}
			else if ((op & O_APPEND) && lseek(fd, 0L, SEEK_END) == -1L)
				errno = save_errno;
#if O_NOCTTY
			if ((op & O_NOCTTY) && ioctl(fd, TIOCNOTTY, 0))
				errno = save_errno;
#endif
			break;
		}
	}
	else fd = open(path, op, mode);
	return(fd);
}

#endif
