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
static char*     Version = "\n@(#)$Id: cdt (AT&T Labs - Research) 2002-05-31 $\0\n";

/* 	Make a new dictionary
**
**	Written by Kiem-Phong Vo (5/25/96)
*/

#if __STD_C
Dt_t* dtopen(Dtdisc_t* disc, Dtmethod_t* meth)
#else
Dt_t*	dtopen(disc, meth)
Dtdisc_t*	disc;
Dtmethod_t*	meth;
#endif
{
	Dt_t*		dt = (Dt_t*)Version;	/* shut-up unuse warning */
	reg int		e;
	Dtdata_t*	data;

	if(!disc || !meth)
		return NIL(Dt_t*);

	/* allocate space for dictionary */
	if(!(dt = (Dt_t*) malloc(sizeof(Dt_t))))
		return NIL(Dt_t*);

	/* initialize all absolutely private data */
	dt->searchf = NIL(Dtsearch_f);
	dt->meth = NIL(Dtmethod_t*);
	dt->disc = NIL(Dtdisc_t*);
	dtdisc(dt,disc,0);
	dt->type = DT_MALLOC;
	dt->nview = 0;
	dt->view = dt->walk = NIL(Dt_t*);

	if(disc->eventf)
	{	/* if shared/persistent dictionary, get existing data */
		data = NIL(Dtdata_t*);
		if((e = (*disc->eventf)(dt,DT_OPEN,(Void_t*)(&data),disc)) < 0)
			goto err_open;
		else if(e > 0)
		{	if(data)
			{	if(data->type&meth->type)
					goto done;
				else	goto err_open;
			}

			if(!disc->memoryf)
				goto err_open;

			free((Void_t*)dt);
			if(!(dt = (*disc->memoryf)(0, 0, sizeof(Dt_t), disc)) )
				return NIL(Dt_t*);
			dt->searchf = NIL(Dtsearch_f);
			dt->meth = NIL(Dtmethod_t*);
			dt->disc = NIL(Dtdisc_t*);
			dtdisc(dt,disc,0);
			dt->type = DT_MEMORYF;
			dt->nview = 0;
			dt->view = dt->walk = NIL(Dt_t*);
		}
	}

	/* allocate sharable data */
	if(!(data = (Dtdata_t*)(dt->memoryf)(dt,NIL(Void_t*),sizeof(Dtdata_t),disc)) )
	{ err_open:
		free((Void_t*)dt);
		return NIL(Dt_t*);
	}

	data->type = meth->type;
	data->here = NIL(Dtlink_t*);
	data->htab = NIL(Dtlink_t**);
	data->ntab = data->size = data->loop = 0;
	data->minp = 0;

done:
	dt->data = data;
	dt->searchf = meth->searchf;
	dt->meth = meth;

	if(disc->eventf)
		(*disc->eventf)(dt, DT_ENDOPEN, (Void_t*)dt, disc);

	return dt;
}
