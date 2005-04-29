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
#include	"dthdr.h"

/*	Walk a dictionary and all dictionaries viewed through it.
**	userf:	user function
**
**	Written by Kiem-Phong Vo (5/25/96)
*/

#if __STD_C
int dtwalk(reg Dt_t* dt, int (*userf)(Dt_t*, Void_t*, Void_t*), Void_t* data)
#else
int dtwalk(dt,userf,data)
reg Dt_t*	dt;
int(*		userf)();
Void_t*		data;
#endif
{
	reg Void_t	*obj, *next;
	reg Dt_t*	walk;
	reg int		rv;

	for(obj = dtfirst(dt); obj; )
	{	if(!(walk = dt->walk) )
			walk = dt;
		next = dtnext(dt,obj);
		if((rv = (*userf)(walk, obj, data )) < 0)
			return rv;
		obj = next;
	}
	return 0;
}
