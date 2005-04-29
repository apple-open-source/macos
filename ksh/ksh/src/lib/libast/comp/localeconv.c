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
 * localeconv() intercept
 */

#include "lclib.h"

#undef	localeconv

static char	null[] = "";

static struct lconv	debug_lconv =
{
	",",
	".",
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
};

#if !_lib_localeconv

static struct lconv	default_lconv =
{
	".",
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	&null[0],
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
};

struct lconv*
localeconv(void)
{
	return &default_lconv;
}

#endif

/*
 * localeconv() intercept
 */

struct lconv*
_ast_localeconv(void)
{
	return ((locales[AST_LC_MONETARY]->flags | locales[AST_LC_NUMERIC]->flags) & LC_debug) ? &debug_lconv : localeconv();
}
