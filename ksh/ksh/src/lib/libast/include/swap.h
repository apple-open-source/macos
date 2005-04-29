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
 * integral representation conversion support definitions
 * supports sizeof(integral_type)<=sizeof(int_max)
 */

#ifndef _SWAP_H
#define _SWAP_H

#include <int.h>

#define SWAP_MAX	8

#define SWAPOP(n)	(((n)&int_swap)^(n))

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern void*		swapmem(int, const void*, void*, size_t);
extern int_max		swapget(int, const void*, int);
extern void*		swapput(int, void*, int, int_max);
extern int		swapop(const void*, const void*, int);

#undef	extern

#endif
