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

void _STUB_vmwalk(){}

#else

#include	"vmhdr.h"

/*	Walks all segments created in region(s)
**
**	Written by Kiem-Phong Vo, kpv@research.att.com (02/08/96)
*/

#if __STD_C
int vmwalk(Vmalloc_t* vm, int(*segf)(Vmalloc_t*, Void_t*, size_t, Vmdisc_t*) )
#else
int vmwalk(vm, segf)
Vmalloc_t*	vm;
int(*		segf)(/* Vmalloc_t*, Void_t*, size_t, Vmdisc_t* */);
#endif
{	
	reg Seg_t*	seg;
	reg int		rv;

	if(!vm)
	{	for(vm = Vmheap; vm; vm = vm->next)
		{	if(!(vm->data->mode&VM_TRUST) && ISLOCK(vm->data,0) )
				continue;

			SETLOCK(vm->data,0);
			for(seg = vm->data->seg; seg; seg = seg->next)
			{	rv = (*segf)(vm, seg->addr, seg->extent, vm->disc);
				if(rv < 0)
					return rv;
			}
			CLRLOCK(vm->data,0);
		}
	}
	else
	{	if(!(vm->data->mode&VM_TRUST) && ISLOCK(vm->data,0) )
			return -1;

		SETLOCK(vm->data,0);
		for(seg = vm->data->seg; seg; seg = seg->next)
		{	rv = (*segf)(vm, seg->addr, seg->extent, vm->disc);
			if(rv < 0)
				return rv;
		}
		CLRLOCK(vm->data,0);
	}

	return 0;
}

#endif
