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
 * tmpnam implementation
 */

#define _def_map_ast	1

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide tmpnam
#else
#define tmpnam		______tmpnam
#endif

#include <ast.h>
#include <stdio.h>

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide tmpnam
#else
#undef	tmpnam
#endif

#undef	_def_map_ast

#include <ast_map.h>

#ifndef L_tmpnam
#define L_tmpnam	25
#endif

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern char*
tmpnam(char* s)
{
	static char	buf[L_tmpnam];

	return pathtemp(s ? s : buf, L_tmpnam, NiL, "tn", NiL);
}
