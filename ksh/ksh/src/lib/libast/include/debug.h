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
 * common ast debug definitions
 * include after the ast headers
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#include <ast.h>
#include <error.h>

#if DEBUG || _BLD_DEBUG
#define debug(x)	x
#define message(x)	do if (error_info.trace < 0) { error x; } while (0)
#define messagef(x)	do if (error_info.trace < 0) { errorf x; } while (0)
#else
#define debug(x)
#define message(x)
#define messagef(x)
#endif

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern void		systrace(const char*);

#undef	extern

#endif
