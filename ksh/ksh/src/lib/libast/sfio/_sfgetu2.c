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
/* OBSOLETE 19961031 -- for shared library compatibility */

#include	"sfhdr.h"

#undef	_sfgetu2

_BEGIN_EXTERNS_
#if _BLD_sfio && defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern long	_sfgetu2 _ARG_((Sfio_t*, long));

#undef	extern
_END_EXTERNS_

#if __STD_C
long _sfgetu2(reg Sfio_t* f, long v)
#else
long _sfgetu2(f, v)
reg Sfio_t*	f;
long		v;
#endif
{
	if (v < 0)
		return -1;
	sfungetc(f, v);
	return sfgetu(f);
}
