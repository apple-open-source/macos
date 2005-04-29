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
 * posix wordexp interface definitions
 */

#ifndef _WORDEXP_H
#define _WORDEXP_H

#include	<ast.h>

#define WRDE_APPEND	01
#define WRDE_DOOFFS	02
#define WRDE_NOCMD	04
#define WRDE_NOSYS	0100
#define WRDE_REUSE	010
#define WRDE_SHOWERR	020
#define WRDE_UNDEF	040

#define WRDE_BADCHAR	1
#define WRDE_BADVAL	2
#define WRDE_CMDSUB	3
#define WRDE_NOSPACE	4
#define WRDE_SYNTAX	5
#define WRDE_NOSHELL	6

typedef struct _wdarg
{
	size_t	we_wordc;
	char	**we_wordv;
	size_t	we_offs;
} wordexp_t;

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern int wordexp(const char*, wordexp_t*, int);
extern int wordfree(wordexp_t*);

#undef	extern

#endif /* _WORDEXP_H */
