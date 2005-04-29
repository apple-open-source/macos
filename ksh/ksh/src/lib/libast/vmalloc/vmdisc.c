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

void _STUB_vmdisc(){}

#else

#include	"vmhdr.h"

/*	Change the discipline for a region.  The old discipline
**	is returned.  If the new discipline is NIL then the
**	discipline is not changed.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/
#if __STD_C
Vmdisc_t* vmdisc(Vmalloc_t* vm, Vmdisc_t* disc)
#else
Vmdisc_t* vmdisc(vm, disc)
Vmalloc_t*	vm;
Vmdisc_t*	disc;
#endif
{
	Vmdisc_t*	old = vm->disc;

	if(disc)
	{	if(disc->memoryf != old->memoryf)
			return NIL(Vmdisc_t*);
		if(old->exceptf &&
		   (*old->exceptf)(vm,VM_DISC,(Void_t*)disc,old) != 0 )
			return NIL(Vmdisc_t*);
		vm->disc = disc;
	}
	return old;
}

#endif
