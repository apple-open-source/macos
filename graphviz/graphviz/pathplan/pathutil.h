/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#pragma prototyped

#ifndef _PATHUTIL_INCLUDE
#define _PATHUTIL_INCLUDE
#include <pathplan.h>

#define NOT(x)	(!(x))
#ifndef FALSE
#define FALSE	0
#define TRUE	(NOT(FALSE))
#endif

typedef double COORD;
extern COORD area2 (Ppoint_t, Ppoint_t, Ppoint_t);
extern COORD dist2 (Ppoint_t, Ppoint_t);
extern int intersect(Ppoint_t a,Ppoint_t b,Ppoint_t c,Ppoint_t d);

int	in_poly(Ppoly_t argpoly, Ppoint_t q);
Ppoly_t	copypoly(Ppoly_t);
void	freepoly(Ppoly_t);

#endif
