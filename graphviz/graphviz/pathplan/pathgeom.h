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

#ifndef _PATHGEOM_INCLUDE
#define _PATHGEOM_INCLUDE
typedef struct Pxy_t {
    double x, y;
} Pxy_t;

typedef struct Pxy_t Ppoint_t;
typedef struct Pxy_t Pvector_t;

typedef struct Ppoly_t {
    Ppoint_t *ps;
    int pn;
} Ppoly_t;

typedef Ppoly_t Ppolyline_t;

typedef struct Pedge_t {
    Ppoint_t a, b;
} Pedge_t;

/* opaque state handle for visibility graph operations */
typedef struct vconfig_s vconfig_t;
#endif
