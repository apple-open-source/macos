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

#ifndef _VIS_INCLUDE
#define _VIS_INCLUDE

#include <pathgeom.h>

#if defined(_BLD_pathplan) && defined(_DLL)
#   define extern __EXPORT__
#endif

/* open a visibility graph */
extern vconfig_t *Pobsopen(Ppoly_t **obstacles, int n_obstacles);

/* close a visibility graph, freeing its storage */
extern void Pobsclose(vconfig_t *config);

/* route a polyline from p0 to p1, avoiding obstacles.
 * if an endpoint is inside an obstacle, pass the polygon's index >=0
 * if the endpoint is not inside an obstacle, pass POLYID_NONE
 * if the endpoint location is not known, pass POLYID_UNKNOWN
 */

extern int Pobspath(vconfig_t *config, Ppoint_t p0, int poly0,
	Ppoint_t p1, int poly1, 
	Ppolyline_t *output_route);

#define POLYID_NONE		-1111
#define POLYID_UNKNOWN	-2222

#undef extern

#endif
