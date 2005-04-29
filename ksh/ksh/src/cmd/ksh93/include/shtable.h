/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1982-2004 AT&T Corp.                *
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
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
#ifndef _SHTABLE_H

/*
 * David Korn
 * AT&T Labs
 *
 * Interface definitions read-only data tables for shell
 *
 */

#define _SHTABLE_H	1

typedef struct shtable1
{
	const char	*sh_name;
	unsigned	sh_number;
} Shtable_t;

struct shtable2
{
	const char	*sh_name;
	unsigned	sh_number;
	const char	*sh_value;
};

struct shtable3
{
	const char	*sh_name;
	unsigned	sh_number;
	int		(*sh_value)(int, char*[], void*);
};

#define sh_lookup(name,value)	sh_locate(name,(Shtable_t*)(value),sizeof(*(value)))
extern const Shtable_t		shtab_testops[];
extern const Shtable_t		shtab_options[];
extern const Shtable_t		shtab_attributes[];
extern const struct shtable2	shtab_variables[];
extern const struct shtable2	shtab_aliases[];
extern const struct shtable2	shtab_signals[];
extern const struct shtable3	shtab_builtins[];
extern const Shtable_t		shtab_reserved[];
extern int	sh_locate(const char*, const Shtable_t*, int);

#endif /* SH_TABLE_H */
