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
#if defined(_UWIN) && defined(_BLD_ast)

void _STUB_vmstat(){}

#else

#include	"vmhdr.h"

/*	Get statistics from a region.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/

#if __STD_C
int vmstat(Vmalloc_t* vm, Vmstat_t* st)
#else
int vmstat(vm, st)
Vmalloc_t*	vm;
Vmstat_t*	st;
#endif
{
	reg Seg_t*	seg;
	reg Block_t	*b, *endb;
	reg size_t	s = 0;
	reg Vmdata_t*	vd = vm->data;

	if(!st)
		return -1;
	if(!(vd->mode&VM_TRUST))
	{	if(ISLOCK(vd,0))
			return -1;
		SETLOCK(vd,0);
	}

	st->n_busy = st->n_free = 0;
	st->s_busy = st->s_free = st->m_busy = st->m_free = 0;
	st->n_seg = 0;
	st->extent = 0;

	if(vd->mode&VM_MTLAST)
		st->n_busy = 0;
	else if((vd->mode&VM_MTPOOL) && (s = vd->pool) > 0)
	{	s = ROUND(s,ALIGN);
		for(b = vd->free; b; b = SEGLINK(b))
			st->n_free += 1;
	}

	for(seg = vd->seg; seg; seg = seg->next)
	{	st->n_seg += 1;
		st->extent += seg->extent;

		b = SEGBLOCK(seg);
		endb = BLOCK(seg->baddr);

		if(vd->mode&(VM_MTDEBUG|VM_MTBEST|VM_MTPROFILE))
		{	while(b < endb)
			{	s = SIZE(b)&~BITS;
				if(ISJUNK(SIZE(b)) || !ISBUSY(SIZE(b)))
				{	if(s > st->m_free)
						st->m_free = s;
					st->s_free += s;
					st->n_free += 1;
				}
				else	/* get the real size */
				{	if(vd->mode&VM_MTDEBUG)
						s = DBSIZE(DB2DEBUG(DATA(b)));
					else if(vd->mode&VM_MTPROFILE)
						s = PFSIZE(DATA(b));
					if(s > st->m_busy)
						st->m_busy = s;
					st->s_busy += s;
					st->n_busy += 1;
				}

				b = (Block_t*)((Vmuchar_t*)DATA(b) + (SIZE(b)&~BITS) );
			}
		}
		else if(vd->mode&VM_MTLAST)
		{	if((s = seg->free ? (SIZE(seg->free) + sizeof(Head_t)) : 0) > 0)
			{	st->s_free += s;
				st->n_free += 1;
			}
			if((s = ((char*)endb - (char*)b) - s) > 0)
			{	st->s_busy += s;
				st->n_busy += 1;
			}
		}
		else if((vd->mode&VM_MTPOOL) && s > 0)
		{	if(seg->free)
				st->n_free += (SIZE(seg->free)+sizeof(Head_t))/s;
			st->n_busy += ((seg->baddr - (Vmuchar_t*)b) - sizeof(Head_t))/s;
		}
	}

	if((vd->mode&VM_MTPOOL) && s > 0)
	{	st->n_busy -= st->n_free;
		if(st->n_busy > 0)
			st->s_busy = (st->m_busy = vd->pool)*st->n_busy;
		if(st->n_free > 0)
			st->s_free = (st->m_free = vd->pool)*st->n_free;
	}

	CLRLOCK(vd,0);
	return 0;
}

#endif
