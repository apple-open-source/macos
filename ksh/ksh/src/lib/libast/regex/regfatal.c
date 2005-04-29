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
 * posix regex fatal error interface to error()
 */

#include "reglib.h"

#include <error.h>

void
regfatalpat(regex_t* p, int level, int code, const char* pat)
{
	char	buf[128];

	regerror(code, p, buf, sizeof(buf));
	regfree(p);
	if (pat)
		error(level, "regular expression: %s: %s", pat, buf);
	else
		error(level, "regular expression: %s", buf);
}

void
regfatal(regex_t* p, int level, int code)
{
	regfatalpat(p, level, code, NiL);
}
