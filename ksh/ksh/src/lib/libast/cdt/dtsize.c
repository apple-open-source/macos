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

/*	Return the # of objects in the dictionary
**
**	Written by Kiem-Phong Vo (5/25/96)
*/

#if __STD_C
static int treecount(reg Dtlink_t* e)
#else
static int treecount(e)
reg Dtlink_t*	e;
#endif
{	return e ? treecount(e->left) + treecount(e->right) + 1 : 0;
}

#if __STD_C
int dtsize(Dt_t* dt)
#else
int dtsize(dt)
Dt_t*	dt;
#endif
{
	reg Dtlink_t*	t;
	reg int		size;

	UNFLATTEN(dt);

	if(dt->data->size < 0) /* !(dt->data->type&(DT_SET|DT_BAG)) */
	{	if(dt->data->type&(DT_OSET|DT_OBAG))
			dt->data->size = treecount(dt->data->here);
		else if(dt->data->type&(DT_LIST|DT_STACK|DT_QUEUE))
		{	for(size = 0, t = dt->data->head; t; t = t->right)
				size += 1;
			dt->data->size = size;
		}
	}

	return dt->data->size;
}
