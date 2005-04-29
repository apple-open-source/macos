/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
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
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * AT&T Research
 *
 * builtin cmd definitions
 */

#ifndef _CMD_H
#define _CMD_H

#include <ast.h>
#include <error.h>
#include <stak.h>

#if _BLD_cmd && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern int	b_basename(int, char**, void*);
extern int	b_cat(int, char**, void*);
extern int	b_chgrp(int, char**, void*);
extern int	b_chmod(int, char**, void*);
extern int	b_chown(int, char**, void*);
extern int	b_cmp(int, char**, void*);
extern int	b_comm(int, char**, void*);
extern int	b_cp(int, char**, void*);
extern int	b_cut(int, char**, void*);
extern int	b_date(int, char**, void*);
extern int	b_dirname(int, char**, void*);
extern int	b_expr(int, char**, void*);
extern int	b_fmt(int, char**, void*);
extern int	b_fold(int, char**, void*);
extern int	b_getconf(int, char**, void*);
extern int	b_head(int, char**, void*);
extern int	b_id(int, char**, void*);
extern int	b_join(int, char**, void*);
extern int	b_ln(int, char**, void*);
extern int	b_logname(int, char**, void*);
extern int	b_mkdir(int, char**, void*);
extern int	b_mkfifo(int, char**, void*);
extern int	b_mv(int, char**, void*);
extern int	b_paste(int, char**, void*);
extern int	b_pathchk(int, char**, void*);
extern int	b_rev(int, char**, void*);
extern int	b_rm(int, char**, void*);
extern int	b_rmdir(int, char**, void*);
extern int	b_stty(int, char**, void*);
extern int	b_tail(int, char**, void*);
extern int	b_tee(int, char**, void*);
extern int	b_tty(int, char**, void*);
extern int	b_uname(int, char**, void*);
extern int	b_uniq(int, char**, void*);
extern int	b_wc(int, char**, void*);

#undef	extern

#if defined(BUILTIN) && !defined(STANDALONE)
#define STANDALONE	BUILTIN
#endif

#ifdef	STANDALONE

#if DYNAMIC

#include <dlldefs.h>

typedef int (*Builtin_f)(int, char**, void*);

#else

extern int STANDALONE(int, char**, void*);

#endif

#ifndef BUILTIN

/*
 * command initialization
 */

static void
cmdinit(register char** argv, void* context, const char* catalog, int flags)
{
	register char*	cp;
	register char*	pp;

	if (cp = strrchr(argv[0], '/'))
		cp++;
	else
		cp = argv[0];
	if (pp = strrchr(cp, '_'))
		cp = pp + 1;
	error_info.id = cp;
	if (!error_info.catalog)
		error_info.catalog = (char*)catalog;
	opt_info.index = 0;
	if (context)
		error_info.flags |= flags;
}

#endif

int
main(int argc, char** argv)
{
#if DYNAMIC
	register char*	s;
	register char*	t;
	void*		dll;
	Builtin_f	fun;
	char		buf[64];

	if (s = strrchr(argv[0], '/'))
		s++;
	else if (!(s = argv[0]))
		return 127;
	if ((t = strrchr(s, '_')) && *++t)
		s = t;
	buf[0] = '_';
	buf[1] = 'b';
	buf[2] = '_';
	strncpy(buf + 3, s, sizeof(buf) - 4);
	buf[sizeof(buf) - 1] = 0;
	if (t = strchr(buf, '.'))
		*t = 0;
	for (;;)
	{
		if (dll = dlopen(NiL, RTLD_LAZY))
		{
			if (fun = (Builtin_f)dlsym(dll, buf + 1))
				break;
			if (fun = (Builtin_f)dlsym(dll, buf))
				break;
		}
		if (dll = dllfind("cmd", NiL, RTLD_LAZY))
		{
			if (fun = (Builtin_f)dlsym(dll, buf + 1))
				break;
			if (fun = (Builtin_f)dlsym(dll, buf))
				break;
		}
		return 127;
	}
	return (*fun)(argc, argv, NiL);
#else
	return STANDALONE(argc, argv, NiL);
#endif
}

#else

#if _BLD_cmd && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern int	cmdrecurse(int, char**, int, char**);
extern void	cmdinit(char**, void*, const char*, int);

#undef	extern

#endif

#endif
