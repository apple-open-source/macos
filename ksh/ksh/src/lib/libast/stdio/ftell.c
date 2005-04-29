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

#include "stdhdr.h"

long
ftell(Sfio_t* f)
{
	STDIO_INT(f, "ftell", long, (Sfio_t*), (f))

	return (long)sfseek(f, (Sfoff_t)0, SEEK_CUR);
}

#ifdef _ast_int8_t

_ast_int8_t
ftell64(Sfio_t* f)
{
	STDIO_INT(f, "ftell64", _ast_int8_t, (Sfio_t*), (f))

	return (_ast_int8_t)sfseek(f, (Sfoff_t)0, SEEK_CUR);
}

#endif
