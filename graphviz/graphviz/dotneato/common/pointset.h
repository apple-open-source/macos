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

#ifndef _POINTSET_H
#define _POINTSET_H 1

#include <render.h>

typedef Dict_t PointSet;

extern PointSet* newPS ();
extern void      freePS (PointSet*);
extern void      insertPS (PointSet*, point);
extern void      addPS (PointSet*, int, int);
extern int       inPS (PointSet*, point);
extern int       isInPS (PointSet*, int, int);
extern int       sizeOf (PointSet*);
extern point*    pointsOf (PointSet*);

#endif

