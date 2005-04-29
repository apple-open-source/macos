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

static const char id[] = "\n@(#)$Id: ast (AT&T Labs Research) 2003-06-11 $\0\n";

#include <ast.h>

#undef	strcmp

#if __OBSOLETE__ && __OBSOLETE__ < 20020401

/*
 * _Ast_info_t grew
 * the old exported symbol was _ast_state, retained here for link compatibility
 * new compilations will use _ast_info
 * extra space was added to avoid this in the future
 */

typedef struct
{

	char*		id;

	struct
	{
	unsigned int	serial;
	unsigned int	set;
	}		locale;

	long		tmp_long;
	size_t		tmp_size;
	short		tmp_short;
	char		tmp_char;
	wchar_t		tmp_wchar;

	int		(*collate)(const char*, const char*);

	int		tmp_int;
	void*		tmp_pointer;

} _Ast_state_t;

#if defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern _Ast_state_t	_ast_state;

__EXTERN__(_Ast_state_t, _ast_state);

#undef	extern

_Ast_state_t	_ast_state =
{
	"libast",
	{ 0, 0 },
	0, 0, 0, 0, 0,
	strcmp
};

#endif

_Ast_info_t	_ast_info =
{
	"libast",
	{ 0, 0 },
	0, 0, 0, 0, 0,
	strcmp,
	0, 0,
	1
};

__EXTERN__(_Ast_info_t, _ast_info);
