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

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern int
putenv(const char* s)
{
	return setenviron(s) ? 0 : -1;
}

extern int
setenv(const char* name, const char* value, int overwrite)
{
	char*	s;

	if (overwrite || !getenv(name))
	{
		if (!(s = sfprints("%s=%s", name, value)) || !(s = strdup(s)))
			return -1;
		return setenviron(s) ? 0 : -1;
	}
	return 0;
}

extern void
unsetenv(const char *name)
{
	if (!strchr(name, '='))
		setenviron(name);
}
