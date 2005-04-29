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
 * internal representation conversion support
 */

#include <ast.h>
#include <swap.h>

/*
 * return the swap operation for external to internal conversion
 */

int
swapop(const void* internal, const void* external, int size)
{
	register int	op;
	char		tmp[sizeof(int_max)];

	if (size <= 1)
		return 0;
	if (size <= sizeof(int_max))
		for (op = 0; op < size; op++)
			if (!memcmp(internal, swapmem(op, external, tmp, size), size))
			{
				/*
				 * le on 4 bytes is also le on 8
				 * nuxi pdp is the anomaly
				 */

				if (op == 3 && size == 4)
					op = 7;
				return op;
			}
	return -1;
}
