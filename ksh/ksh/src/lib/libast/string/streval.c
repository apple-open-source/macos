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
 * obsolete streval() interface to strexpr()
 */

#include <ast.h>

typedef long (*Old_convert_t)(const char*, char**);

typedef long (*Convert_t)(const char*, char**, void*);

typedef struct
{
	Old_convert_t	convert;
} Handle_t;

static long
userconv(const char* s, char** end, void* handle)
{
	return((*((Handle_t*)handle)->convert)(s, end));
}

long
streval(const char* s, char** end, Old_convert_t convert)
{
	Handle_t	handle;

	return((handle.convert = convert) ? strexpr(s, end, userconv, &handle) : strexpr(s, end, (Convert_t)0, NiL));
}
