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

#include "stdhdr.h"

#include <sfstr.h>

int
vfwprintf(Sfio_t* f, const wchar_t* fmt, va_list args)
{
	char*	m;
	char*	x;
	wchar_t*w;
	size_t	n;
	int	v;
	Sfio_t*	t;

	STDIO_INT(f, "vfwprintf", int, (Sfio_t*, const wchar_t*, va_list), (f, fmt, args))

	FWIDE(f, WEOF);
	n = wcstombs(NiL, fmt, 0);
	if (m = malloc(n + 1))
	{
		if (t = sfstropen())
		{
			wcstombs(m, fmt, n + 1);
			sfvprintf(t, m, args);
			free(m);
			x = sfstruse(t);
			n = mbstowcs(NiL, x, 0);
			if (w = (wchar_t*)sfreserve(f, n * sizeof(wchar_t) + 1, 0))
				v = mbstowcs(w, x, n + 1);
			else
				v = -1;
			sfstrclose(t);
		}
		else
			v = -1;
	}
	else
		v = -1;
	return v;
}
