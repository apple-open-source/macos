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
 * getdate implementation
 */

#define _def_map_ast	1

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide getdate
#else
#define getdate	______getdate
#endif

#include <ast.h>
#include <tm.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide getdate
#else
#undef	getdate
#endif

#undef	_def_map_ast

#include <ast_map.h>

#undef	_lib_getdate	/* we can pass X/Open */

#if _lib_getdate

NoN(getdate)

#else

#ifndef getdate_err
__DEFINE__(int, getdate_err, 0);
#endif

extern struct tm*
getdate(const char* s)
{
	char*	e;
	char*	f;
	time_t	t;

	t = tmscan(s, &e, NiL, &f, NiL, TM_PEDANTIC);
	if (*e || *f)
	{
		/* of course we all know what 7 means */
		getdate_err = 7;
		return 0;
	}
	return tmmake(&t);
}

#endif
