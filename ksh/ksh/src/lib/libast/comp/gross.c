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
/*
 * porting hacks here
 */

#include <ast.h>
#include <ls.h>

#include "FEATURE/hack"

void _STUB_gross(){}

#if _lcl_xstat

extern int fstat(int fd, struct stat* st)
{
#if _lib___fxstat
	return __fxstat(_STAT_VER, fd, st);
#else
	return _fxstat(_STAT_VER, fd, st);
#endif
}

extern int lstat(const char* path, struct stat* st)
{
#if _lib___lxstat
	return __lxstat(_STAT_VER, path, st);
#else
	return _lxstat(_STAT_VER, path, st);
#endif
}

extern int stat(const char* path, struct stat* st)
{
#if _lib___xstat
	return __xstat(_STAT_VER, path, st);
#else
	return _xstat(_STAT_VER, path, st);
#endif
}

#endif

#if _lcl_xstat64

extern int fstat64(int fd, struct stat64* st)
{
#if _lib___fxstat64
	return __fxstat64(_STAT_VER, fd, st);
#else
	return _fxstat64(_STAT_VER, fd, st);
#endif
}

extern int lstat64(const char* path, struct stat64* st)
{
#if _lib___lxstat64
	return __lxstat64(_STAT_VER, path, st);
#else
	return _lxstat64(_STAT_VER, path, st);
#endif
}

extern int stat64(const char* path, struct stat64* st)
{
#if _lib___xstat64
	return __xstat64(_STAT_VER, path, st);
#else
	return _xstat64(_STAT_VER, path, st);
#endif
}

#endif

#if __sgi && _hdr_locale_attr

#include "gross_sgi.h"

#endif
