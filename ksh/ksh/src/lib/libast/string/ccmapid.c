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
 * return ccode map id given name
 */

#include <ast.h>
#include <ccode.h>
#include <iconv.h>

int
ccmapid(const char* name)
{
	return iconv_name(name, NiL, 0);
}

/*
 * return ccode map name given id
 */

char*
ccmapname(register int id)
{
	register iconv_list_t*	ic;

	for (ic = iconv_list(NiL); ic; ic = iconv_list(ic))
		if (id == ic->ccode)
			return (char*)ic->name;
	return 0;
}
