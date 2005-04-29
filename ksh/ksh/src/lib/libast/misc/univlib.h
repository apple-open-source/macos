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
 * universe support
 *
 * symbolic link external representation has trailing '\0' and $(...) style
 * conditionals where $(...) corresponds to a kernel object (i.e., probably
 * not environ)
 *
 * universe symlink conditionals use $(UNIVERSE)
 */

#ifndef _UNIVLIB_H
#define _UNIVLIB_H

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide getuniverse readlink setuniverse symlink universe
#else
#define getuniverse	______getuniverse
#define readlink	______readlink
#define setuniverse	______setuniverse
#define symlink		______symlink
#define universe	______universe
#endif

#include <ast.h>
#include <ls.h>
#include <errno.h>

#define UNIV_SIZE	9

#if _cmd_universe && _sys_universe
#include <sys/universe.h>
#endif

#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide getuniverse readlink setuniverse symlink universe
#else
#undef	getuniverse
#undef	readlink
#undef	setuniverse
#undef	symlink
#undef	universe
#endif

#if _cmd_universe
#ifdef NUMUNIV
#define UNIV_MAX	NUMUNIV
#else
#define UNIV_MAX	univ_max
extern char*		univ_name[];
extern int		univ_max;
#endif

extern char		univ_cond[];
extern int		univ_size;

#else

extern char		univ_env[];

#endif

extern int		getuniverse(char*);
extern int		readlink(const char*, char*, int);
extern int		setuniverse(int);
extern int		symlink(const char*, const char*);
extern int		universe(int);

#endif
