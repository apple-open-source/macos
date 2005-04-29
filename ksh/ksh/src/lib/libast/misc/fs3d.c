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
 * 3d fs operations
 * only active for non-shared 3d library
 */

#include <ast.h>
#include <fs3d.h>

int
fs3d(register int op)
{
	register int	cur;
	register char*	v;
	char		val[sizeof(FS3D_off) + 8];

	static int	fsview;
	static char	on[] = FS3D_on;
	static char	off[] = FS3D_off;

	if (fsview < 0) return(0);

	/*
	 * get the current setting
	 */

	if (!fsview && mount("", "", 0, NiL))
		goto nope;
	if (FS3D_op(op) == FS3D_OP_INIT && mount(FS3D_init, NiL, FS3D_VIEW, NiL))
		goto nope;
	if (mount(on, val, FS3D_VIEW|FS3D_GET|FS3D_SIZE(sizeof(val)), NiL))
		goto nope;
	if (v = strchr(val, ' ')) v++;
	else v = val;
	if (!strcmp(v, on))
		cur = FS3D_ON;
	else if (!strncmp(v, off, sizeof(off) - 1) && v[sizeof(off)] == '=')
		cur = FS3D_LIMIT((int)strtol(v + sizeof(off) + 1, NiL, 0));
	else cur = FS3D_OFF;
	if (cur != op)
	{
		switch (FS3D_op(op))
		{
		case FS3D_OP_OFF:
			v = off;
			break;
		case FS3D_OP_ON:
			v = on;
			break;
		case FS3D_OP_LIMIT:
			sfsprintf(val, sizeof(val), "%s=%d", off, FS3D_arg(op));
			v = val;
			break;
		default:
			v = 0;
			break;
		}
		if (v && mount(v, NiL, FS3D_VIEW, NiL))
			goto nope;
	}
	fsview = 1;
	return(cur);
 nope:
	fsview = -1;
	return(0);
}
