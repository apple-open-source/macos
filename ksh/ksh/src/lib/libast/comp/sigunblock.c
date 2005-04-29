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

#include <ast.h>

#if _lib_sigunblock

NoN(sigunblock)

#else

#include <sig.h>

#ifndef SIG_UNBLOCK
#undef	_lib_sigprocmask
#endif

int
sigunblock(int s)
{
#if _lib_sigprocmask
	int		op;
	sigset_t	mask;

	sigemptyset(&mask);
	if (s)
	{
		sigaddset(&mask, s);
		op = SIG_UNBLOCK;
	}
	else op = SIG_SETMASK;
	return(sigprocmask(op, &mask, NiL));
#else
#if _lib_sigsetmask
	return(sigsetmask(s ? (sigsetmask(0L) & ~sigmask(s)) : 0L));
#else
	NoP(s);
	return(0);
#endif
#endif
}

#endif
