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

/*	Hashing a string
**
**	Written by Kiem-Phong Vo (05/22/96)
*/
#if __STD_C
uint dtstrhash(reg uint h, Void_t* args, reg int n)
#else
uint dtstrhash(h,args,n)
reg uint	h;
Void_t*		args;
reg int		n;
#endif
{
	reg unsigned char*	s = (unsigned char*)args;

	if(n <= 0)
	{	for(; (n = *s) != 0; ++s)
			h = dtcharhash(h,n);
	}
	else
	{	reg unsigned char*	ends;
		for(ends = s+n; s < ends; ++s)
		{	n = *s;
			h = dtcharhash(h,n);
		}
	}

	return h;
}
