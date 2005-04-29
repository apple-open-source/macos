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
 * strptime implementation
 */

#define _def_map_ast	1

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide strptime
#else
#define strptime	______strptime
#endif

#include <ast.h>
#include <tm.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide strptime
#else
#undef	strptime
#endif

#undef	_def_map_ast

#include <ast_map.h>

#if _lib_strptime

NoN(strptime)

#else

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern char*
strptime(const char* s, const char* format, struct tm* tm)
{
	char*	e;
	char*	f;
	time_t	t;

	t = tmtime(tm, TM_LOCALZONE);
	t = tmscan(s, &e, format, &f, &t, 0);
	if (e == (char*)s || *f)
		return 0;
	*tm = *tmmake(&t);
	return e;
}

#endif
