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
 * if sig>=0 then return signal text for signal sig
 * otherwise return signal name for signal -sig
 */

#include <ast.h>
#include <sig.h>

char*
fmtsignal(register int sig)
{
	char*	buf;
	int	z;

	if (sig >= 0)
	{
		if (sig <= sig_info.sigmax)
			buf = sig_info.text[sig];
		else
		{
			buf = fmtbuf(z = 20);
			sfsprintf(buf, z, "Signal %d", sig);
		}
	}
	else
	{
		sig = -sig;
		if (sig <= sig_info.sigmax)
			buf = sig_info.name[sig];
		else
		{
			buf = fmtbuf(z = 20);
			sfsprintf(buf, z, "%d", sig);
		}
	}
	return buf;
}
