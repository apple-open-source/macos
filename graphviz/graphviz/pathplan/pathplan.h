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

#ifndef _PATH_INCLUDE
#define _PATH_INCLUDE

#include <pathgeom.h>


#if defined(_BLD_pathplan) && defined(_DLL)
#   define extern __EXPORT__
#endif

/* find shortest euclidean path within a simple polygon */
extern int Pshortestpath(Ppoly_t *boundary, Ppoint_t endpoints[2],
	Ppolyline_t *output_route);

/* fit a spline to an input polyline, without touching barrier segments */
extern int Proutespline (Pedge_t *barriers, int n_barriers,
	Ppolyline_t input_route, Pvector_t endpoint_slopes[2],
	Ppolyline_t *output_route);

/* utility function to convert from a set of polygonal obstacles to barriers */
extern int Ppolybarriers(Ppoly_t **polys, int npolys, Pedge_t **barriers, int *n_barriers);

#undef extern

#endif
