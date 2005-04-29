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
 * ast library system(3)
 */

#define _def_map_ast	1

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide system
#else
#define system		______system
#endif

#define _STDLIB_H_	1	/* uwin workaround */

#include <ast.h>
#include <proc.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide system
#else
#undef	system
#endif

#undef	_def_map_ast

#include <ast_map.h>

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern int
system(const char* cmd)
{
	char*	sh[4];

	if (!cmd)
		return !access(pathshell(), X_OK);
	sh[0] = "sh";
	sh[1] = "-c";
	sh[2] = (char*)cmd;
	sh[3] = 0;
	return procrun(NiL, sh);
}
