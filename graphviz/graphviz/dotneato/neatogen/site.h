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

#ifndef SITE_H
#define SITE_H

#include "geometry.h"

   /* Sites are also used as vertices on line segments */
typedef struct Site {
    Point        coord;
    int          sitenbr;
    int          refcnt;
} Site;

extern int     siteidx;
extern Site    *bottomsite;

extern void siteinit();
extern Site * getsite ();
extern double dist(Site*, Site*);   /* Distance between two sites */
extern void deref(Site*);          /* Increment refcnt of site  */
extern void ref(Site*);            /* Decrement refcnt of site  */
extern void makevertex(Site*);     /* Transform a site into a vertex */
#endif
