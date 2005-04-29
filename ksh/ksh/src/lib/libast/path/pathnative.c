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
 * convert path to native fs representation in <buf,siz>
 * length of converted path returned
 * if return length >= siz then buf is indeterminate, but another call
 * with siz=length+1 would work
 * if buf==0 then required size is returned
 */

#include <ast.h>

#if _UWIN

extern int	uwin_path(const char*, char*, int);

size_t
pathnative(const char* path, char* buf, size_t siz)
{
	return uwin_path(path, buf, siz);
}

#else

#if __CYGWIN__

extern void	cygwin_conv_to_win32_path(const char*, char*);

size_t
pathnative(const char* path, char* buf, size_t siz)
{
	size_t		n;

	if (!buf || siz < PATH_MAX)
	{
		char	tmp[PATH_MAX];

		cygwin_conv_to_win32_path(path, tmp);
		if ((n = strlen(tmp)) < siz && buf)
			memcpy(buf, tmp, n + 1);
		return n;
	}
	cygwin_conv_to_win32_path(path, buf);
	return strlen(buf);
}

#else

#if __EMX__

size_t
pathnative(const char* path, char* buf, size_t siz)
{
	char*		s;
	size_t		n;

	if (!_fullpath(buf, path, siz))
	{
		for (s = buf; *s; s++)
			if (*s == '/')
				*s = '\\';
	}
	else if ((n = strlen(path)) < siz && buf)
		memcpy(buf, path, n + 1);
	return n;
}

#else

#if __INTERIX

#include <interix/interix.h>

size_t
pathnative(const char* path, char* buf, size_t siz)
{
	*buf = 0;
	if (path[1] == ':')
		strlcpy(buf, path, siz);
	else
		unixpath2win(path, 0, buf, siz);
	return strlen(buf);
}

#else

size_t
pathnative(const char* path, char* buf, size_t siz)
{
	size_t		n;

	if ((n = strlen(path)) < siz && buf)
		memcpy(buf, path, n + 1);
	return n;
}

#endif

#endif

#endif

#endif
