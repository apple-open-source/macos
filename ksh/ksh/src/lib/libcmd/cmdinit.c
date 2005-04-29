/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
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
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * command initialization
 */

#include <cmdlib.h>

void
cmdinit(char** argv, void* context, const char* catalog, int flags)
{
	register char*	cp;

	if (cp = strrchr(argv[0], '/'))
		cp++;
	else
		cp = argv[0];
	error_info.id = cp;
	if (!error_info.catalog)
		error_info.catalog = catalog;
	opt_info.index = 0;
	if (context)
		error_info.flags |= flags;
}
