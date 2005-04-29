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
 * strftime implementation
 */

#define _def_map_ast	1

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide strftime
#else
#define strftime	______strftime
#endif

#include <ast.h>
#include <tm.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide strftime
#else
#undef	strftime
#endif

#undef	_def_map_ast

#include <ast_map.h>

#undef	_lib_strftime	/* we can pass X/Open */

#if _lib_strftime

NoN(strftime)

#else

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern size_t
strftime(char* buf, size_t len, const char* format, const struct tm* tm)
{
	register char*	s;
	time_t		t;
	Tm_t		tl;

	/*
	 * nl_langinfo() may call strftime() with bogus tm except for
	 * one value -- what a way to go
	 */

	if (tm->tm_sec < 0 || tm->tm_sec > 60 ||
	    tm->tm_min < 0 || tm->tm_min > 59 ||
	    tm->tm_hour < 0 || tm->tm_hour > 23 ||
	    tm->tm_wday < 0 || tm->tm_wday > 6 ||
	    tm->tm_mday < 1 || tm->tm_mday > 31 ||
	    tm->tm_mon < 0 || tm->tm_mon > 11 ||
	    tm->tm_year < 0 || tm->tm_year > (2138 - 1900))
	{
		memset(&tl, 0, sizeof(tl));
		if (tm->tm_sec >= 0 && tm->tm_sec <= 60)
			tl.tm_sec = tm->tm_sec;
		if (tm->tm_min >= 0 && tm->tm_min <= 59)
			tl.tm_min = tm->tm_min;
		if (tm->tm_hour >= 0 && tm->tm_hour <= 23)
			tl.tm_hour = tm->tm_hour;
		if (tm->tm_wday >= 0 && tm->tm_wday <= 6)
			tl.tm_wday = tm->tm_wday;
		if (tm->tm_mday >= 0 && tm->tm_mday <= 31)
			tl.tm_mday = tm->tm_mday;
		if (tm->tm_mon >= 0 && tm->tm_mon <= 11)
			tl.tm_mon = tm->tm_mon;
		if (tm->tm_year >= 0 && tm->tm_year <= (2138 - 1900))
			tl.tm_year = tm->tm_year;
		tm = (const struct tm*)&tl;
	}
	t = tmtime((struct tm*)tm, TM_LOCALZONE);
	if (!(s = tmfmt(buf, len, format, &t)))
		return 0;
	return s - buf;
}

#endif
