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
 * use tw to recurse on argc,argv with pfxc,pfxv prefix args
 */

#include <cmdlib.h>
#include <proc.h>
#include <ftwalk.h>

int
cmdrecurse(int argc, char** argv, int pfxc, char** pfxv)
{
	register char**	v;
	register char**	a;
	int		resolve = 'L';
	char		arg[16];

	if (!(a = (char**)stakalloc((argc + pfxc + 4) * sizeof(char**))))
		error(ERROR_exit(1), "out of space");
	v = a;
	*v++ = "tw";
	*v++ = arg;
	*v++ = *(argv - opt_info.index);
	while (*v = *pfxv++)
	{
		if (streq(*v, "-H"))
			resolve = 'H';
		else if (streq(*v, "-P"))
			resolve = 'P';
		v++;
	}
	while (*v++ = *argv++);
	sfsprintf(arg, sizeof(arg), "-%cc%d", resolve, pfxc + 2);
	procopen(*a, a, NiL, NiL, PROC_OVERLAY);
	return(-1);
}
