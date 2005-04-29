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
#include <ls.h>

#if _lib_mknod

NoN(mknod)

#else

#include <error.h>

#ifndef ENOSYS
#define ENOSYS	EINVAL
#endif

int
mknod(const char* path, mode_t mode, dev_t dev)
{
	if (S_ISFIFO(mode))
		return mkfifo(path, mode);
	if (S_ISDIR(mode))
		return mkdir(path, mode);
	errno = ENOSYS;
	return -1;
}

#endif
