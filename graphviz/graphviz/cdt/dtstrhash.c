/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#include	"dthdr.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

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
