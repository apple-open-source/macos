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

#include "lclib.h"

/*
 * low level for ERROR_translate()
 * this fills in NiL arg defaults and calls error_info.translate
 */

char*
errorx(const char* loc, const char* cmd, const char* cat, const char* msg)
{
	char*	s;

	if (ERROR_translating())
	{
		if (!loc)
			loc = (const char*)locales[AST_LC_MESSAGES]->code;
		if (!cat)
		{
			cat = (const char*)error_info.catalog;
			if (!cmd)
				cmd = (const char*)error_info.id;
		}
		if (s = (*error_info.translate)(loc, cmd, cat, msg))
			return s;
	}
	return (char*)msg;
}
