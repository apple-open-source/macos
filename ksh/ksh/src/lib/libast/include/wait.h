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
 * ast POSIX wait/exit support
 */

#ifndef _WAIT_H
#define _WAIT_H

#include <ast.h>
#include <ast_wait.h>

#if _sys_wait
#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide wait waitpid
#else
#define wait		______wait
#define waitpid		______waitpid
#endif
#include <sys/wait.h>
#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide wait waitpid
#else
#undef	wait
#undef	waitpid
#endif
#endif

#ifndef WNOHANG
#define WNOHANG		1
#endif

#ifndef WUNTRACED
#define WUNTRACED	2
#endif

#if !_ok_wif
#undef	WIFEXITED
#undef	WEXITSTATUS
#undef	WIFSIGNALED
#undef	WTERMSIG
#undef	WIFSTOPPED
#undef	WSTOPSIG
#undef	WTERMCORE
#endif

#ifndef WIFEXITED
#define WIFEXITED(x)	(!((x)&((1<<(EXIT_BITS-1))-1)))
#endif

#ifndef WEXITSTATUS
#define WEXITSTATUS(x)	(((x)>>EXIT_BITS)&((1<<EXIT_BITS)-1))
#endif

#ifndef WIFSIGNALED
#define WIFSIGNALED(x)	(((x)&((1<<(EXIT_BITS-1))-1))!=0)
#endif

#ifndef WTERMSIG
#define WTERMSIG(x)	((x)&((1<<(EXIT_BITS-1))-1))
#endif

#ifndef WIFSTOPPED
#define WIFSTOPPED(x)	(((x)&((1<<EXIT_BITS)-1))==((1<<(EXIT_BITS-1))-1))
#endif

#ifndef WSTOPSIG
#define WSTOPSIG(x)	WEXITSTATUS(x)
#endif

#ifndef WTERMCORE
#define WTERMCORE(x)	((x)&(1<<(EXIT_BITS-1)))
#endif

extern pid_t		wait(int*);
extern pid_t		waitpid(pid_t, int*, int);

#endif
