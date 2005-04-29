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
 * AT&T Bell Laboratories
 *
 * return ls -l style file mode string given file mode bits
 * if external!=0 then mode is modex canonical
 */

#include "modelib.h"

char*
fmtmode(register int mode, int external)
{
	register char*		s;
	register struct modeop*	p;
	char*			buf;

	if (!external)
		mode = modex(mode);
	s = buf = fmtbuf(MODELEN + 1);
	for (p = modetab; p < &modetab[MODELEN]; p++)
		*s++ = p->name[((mode & p->mask1) >> p->shift1) | ((mode & p->mask2) >> p->shift2)];
	*s = 0;
	return buf;
}
