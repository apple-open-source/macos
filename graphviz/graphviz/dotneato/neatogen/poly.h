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

#ifndef POLY_H
#define POLY_H

#include "geometry.h"

typedef struct {
    Point   origin;
    Point   corner;
    int     nverts;
    Point*  verts;
    int     kind;
} Poly;

extern void polyFree ();
extern int polyOverlap (Point, Poly*, Point, Poly*);
extern void makePoly (Poly*, Agnode_t *, double);
extern void breakPoly (Poly*);

#endif
