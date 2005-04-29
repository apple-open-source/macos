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

#ifndef EDGES_H
#define EDGES_H

#include "site.h"

typedef struct Edge {
    double      a,b,c;         /* edge on line ax + by = c */
    Site       *ep[2];        /* endpoints (vertices) of edge; initially NULL */
    Site       *reg[2];       /* sites forming edge */
    int        edgenbr;
} Edge;

#define le 0
#define re 1

extern double pxmin, pxmax, pymin, pymax;  /* clipping window */
extern void edgeinit(void);
extern void endpoint(Edge*, int, Site*);
extern void clip_line(Edge *e);
extern Edge *bisect(Site*, Site*);

#endif
