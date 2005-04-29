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

int
fsetpos(Sfio_t* f, const fpos_t* pos)
{
	STDIO_INT(f, "fsetpos", int, (Sfio_t*, const fpos_t*), (f, pos))

	return sfseek(f, (Sfoff_t)pos->_sf_offset, SF_PUBLIC) == (Sfoff_t)pos->_sf_offset ? 0 : -1;
}

#ifdef _ast_int8_t

int
fsetpos64(Sfio_t* f, const fpos64_t* pos)
{
	STDIO_INT(f, "fsetpos64", int, (Sfio_t*, const fpos64_t*), (f, pos))

	return sfseek(f, (Sfoff_t)pos->_sf_offset, SF_PUBLIC) == (Sfoff_t)pos->_sf_offset ? 0 : -1;
}

#endif
