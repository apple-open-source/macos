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
 * Landon Kurt Knoll
 * Phong Vo
 *
 * FNV-1 linear congruent checksum/hash/PRNG
 * see http://www.isthe.com/chongo/tech/comp/fnv/
 */

#ifndef _FNV_H
#define _FNV_H

#include <ast_common.h>

#define FNV_INIT	0x811c9dc5L
#define FNV_MULT	0x01000193L

#define FNVINIT(h)	(h = FNV_INIT)
#define FNVPART(h,c)	(h = h * FNV_MULT ^ (c))
#define FNVSUM(h,s,n)	do { \
			register size_t _i_ = 0; \
			while (_i_ < n) \
				FNVPART(h, ((unsigned char*)s)[_i_++]); \
			} while (0)

#ifdef _ast_int8_t

#ifdef _ast_LL

#define FNV_INIT64	0xcbf29ce484222325LL
#define FNV_MULT64	0x00000100000001b3LL

#else

#define FNV_INIT64	((_ast_int8_t)0xcbf29ce484222325)
#define FNV_MULT64	((_ast_int8_t)0x00000100000001b3)

#endif

#define FNVINIT64(h)	(h = FNV_INIT64)
#define FNVPART64(h,c)	(h = h * FNV_MULT64 ^ (c))
#define FNVSUM64(h,s,n)	do { \
			register int _i_ = 0; \
			while (_i_ < n) \
				FNVPART64(h, ((unsigned char*)s)[_i_++]); \
			} while (0)

#endif

#endif
