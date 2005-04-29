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
 * Glenn Fowler
 * AT&T Research
 *
 * touch file access and modify times of file
 * if flags&PATH_TOUCH_CREATE then file will be created if it doesn't exist
 * if flags&PATH_TOUCH_VERBATIM then times are taken verbatim
 * times have one second granularity
 *
 *	(time_t)(-1)	retain old time
 *	0		use current time
 *
 * the old interface flag values were:
 *	 1	PATH_TOUCH_CREATE
 *	-1	PATH_TOUCH_CREATE|PATH_TOUCH_VERBATIM
 *		PATH_TOUCH_VERBATIM -- not supported
 */

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide utime
#else
#define utime		______utime
#endif

#include <ast.h>
#include <ls.h>
#include <times.h>
#include <error.h>

#if _hdr_utime && _lib_utime
#include <utime.h>
#endif

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide utime
#else
#undef	utime
#endif

#if _lib_utime
#if _hdr_utime
extern int	utime(const char*, const struct utimbuf*);
#else
extern int	utime(const char*, const time_t*);
#endif
#endif

int
touch(const char* file, time_t atime, time_t mtime, int flags)
{
	int		n;
	int		fd;
	int		oerrno = errno;
	int		mode;
#if _lib_utime
	time_t		now;
	struct stat	st;
#if _hdr_utime
	struct utimbuf	ut;
#else
	time_t		ut[2];
#endif

	if (!(flags & PATH_TOUCH_VERBATIM))
	{
		if (atime == (time_t)(-1) || mtime == (time_t)(-1))
		{
			if (stat(file, &st))
				st.st_atime = st.st_mtime = 0;
			if (atime == (time_t)(-1))
				atime = st.st_atime;
			if (mtime == (time_t)(-1))
				mtime = st.st_mtime;
		}
		if (!atime || !mtime)
#if _hdr_utime && _lib_utime_now
		if (atime || mtime)
#endif
		{
			time(&now);
			if (!atime)
				atime = now;
			if (!mtime)
				mtime = now;
		}
	}
#if _hdr_utime
	ut.actime = atime;
	ut.modtime = mtime;
#if _lib_utime_now
	n = utime(file, ((flags & PATH_TOUCH_VERBATIM) || atime || mtime) ? &ut : (struct utimbuf*)0);
#else
	n = utime(file, &ut);
#endif
#else
	ut[0] = atime;
	ut[1] = mtime;
	n = utime(file, ut);
#endif
	if (n)
#else
	if (mtime)
	{
		/*
		 * NOTE: the emulation allows atime to change
		 *	 for mtime only requests
		 */

		errno = EINVAL;
		return -1;
	}
#endif
	{
#if _lib_utime
		if (errno == ENOENT)
#else

		if (access(file, F_OK))
#endif
		{
			if (!(flags & PATH_TOUCH_CREATE))
				return -1;
			umask(mode = umask(0));
			mode = (~mode) & (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
			if ((fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, mode)) < 0)
				return -1;
			close(fd);
			errno = oerrno;
#if _lib_utime
#if _hdr_utime
#if _lib_utime_now
			return ((flags & PATH_TOUCH_VERBATIM) || atime || mtime) ? utime(file, &ut) : 0;
#else
			return 0;
#endif
#else
			return (atime != now || mtime != now) ? utime(file, ut) : 0;
#endif
#else
			return 0;
#endif
		}
#if !_hdr_utime || !_lib_utime
#if _lib_utime
		if (atime == now && mtime == now && (fd = open(file, O_RDWR)) >= 0)
#else
		if ((fd = open(file, O_RDWR)) >= 0)
#endif
		{
			char	c;

			if (read(fd, &c, 1) == 1)
			{
				if (lseek(fd, 0L, 0) == 0L && write(fd, &c, 1) == 1)
				{
					errno = oerrno;
					n = 0;
				}
				close(fd);
			}
			else
			{
				close(fd);
				umask(mode = umask(0));
				mode = (~mode) & (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
				if ((fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, mode)) >= 0)
				{
					close(fd);
					errno = oerrno;
					n = 0;
				}
			}
		}
#endif
	}
	return n;
}
