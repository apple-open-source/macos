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
 * locale state private definitions
 */

#ifndef _LCLIB_H
#define _LCLIB_H	1

#define categories	_ast_categories
#define locales		_ast_locales
#define translate	_ast_translate

struct Lc_info_s;

#define _LC_PRIVATE_ \
	struct Lc_info_s	info[AST_LC_COUNT]; \
	struct Lc_s*		next;

#define _LC_TERRITORY_PRIVATE_ \
	unsigned char		indices[LC_territory_language_max];

#include <ast.h>
#include <error.h>
#include <lc.h>

typedef struct Lc_numeric_s
{
	int		decimal;
	int		thousand;
} Lc_numeric_t;

#define LCINFO(c)	(&locales[c]->info[c])

extern	Lc_category_t	categories[];
extern	Lc_t*		locales[];

extern char*		translate(const char*, const char*, const char*, const char*);

#endif
