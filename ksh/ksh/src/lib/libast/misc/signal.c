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
 * signal that disables syscall restart on interrupt with clear signal mask
 * fun==SIG_DFL also unblocks signal
 */

#if !_UWIN
#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:hide signal
#else
#undef	signal
#define signal		______signal
#endif
#endif

#include <ast.h>
#include <sig.h>

#if !_UWIN
#if defined(__STDPP__directive) && defined(__STDPP__hide)
__STDPP__directive pragma pp:nohide signal
#else
#undef	signal
#endif
#endif

#if !_std_signal && (_lib_sigaction && defined(SA_NOCLDSTOP) || _lib_sigvec && defined(SV_INTERRUPT))

#if !defined(SA_NOCLDSTOP) || !defined(SA_INTERRUPT) && defined(SV_INTERRUPT)
#undef	SA_INTERRUPT
#define SA_INTERRUPT	SV_INTERRUPT
#undef	sigaction
#define sigaction	sigvec
#undef	sigemptyset
#define sigemptyset(p)	(*(p)=0)
#undef	sa_flags
#define sa_flags	sv_flags
#undef	sa_handler
#define sa_handler	sv_handler
#undef	sa_mask
#define	sa_mask		sv_mask
#endif

Sig_handler_t
signal(int sig, Sig_handler_t fun)
{
	struct sigaction	na;
	struct sigaction	oa;
#ifdef SIGNO_MASK
	unsigned int		flags;
#endif

#ifdef SIGNO_MASK
	flags = sig & ~SIGNO_MASK;
	sig &= SIGNO_MASK;
#endif
	if (fun == SIG_DFL)
		sigunblock(sig);
	memzero(&na, sizeof(na));
	na.sa_handler = fun;
#if defined(SA_INTERRUPT) || defined(SA_RESTART)
	switch (sig)
	{
#if defined(SIGIO) || defined(SIGTSTP) || defined(SIGTTIN) || defined(SIGTTOU)
#if defined(SIGIO)
	case SIGIO:
#endif
#if defined(SIGTSTP)
	case SIGTSTP:
#endif
#if defined(SIGTTIN)
	case SIGTTIN:
#endif
#if defined(SIGTTOU)
	case SIGTTOU:
#endif
#if defined(SA_RESTART)
		na.sa_flags = SA_RESTART;
#endif
		break;
#endif
	default:
#if defined(SA_INTERRUPT)
		na.sa_flags = SA_INTERRUPT;
#endif
		break;
	}
#endif
	return(sigaction(sig, &na, &oa) ? (Sig_handler_t)0 : oa.sa_handler);
}

#else

NoN(signal)

#endif
