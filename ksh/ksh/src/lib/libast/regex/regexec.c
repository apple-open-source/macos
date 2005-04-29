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
 * posix regex executor
 * single unsized-string interface
 */

#include "reglib.h"

/*
 * standard wrapper for the sized-record interface
 */

int
regexec(const regex_t* p, const char* s, size_t nmatch, regmatch_t* match, regflags_t flags)
{
	if (flags & REG_STARTEND)
	{
		int		r;
		int		m = match->rm_so;
		regmatch_t*	e;

		if (!(r = regnexec(p, s + m, match->rm_eo - m, nmatch, match, flags)) && m > 0)
			for (e = match + nmatch; match < e; match++)
				if (match->rm_so >= 0)
				{
					match->rm_so += m;
					match->rm_eo += m;
				}
		return r;
	}
	return regnexec(p, s, s ? strlen(s) : 0, nmatch, match, flags);
}
