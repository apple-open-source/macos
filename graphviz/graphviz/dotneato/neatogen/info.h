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

#ifndef INFO_H
#define INFO_H

#include "voronoi.h"
#include "poly.h"
#include "graph.h"

typedef struct ptitem {           /* Point list */
    struct ptitem*    next;
    Point             p;
} PtItem;

typedef struct {                  /* Info concerning site */
    Agnode_t*   node;                 /* libgraph node */
    Site        site;                 /* site used by voronoi code */
    int         overlaps;             /* true if node overlaps other nodes */
    Poly        poly;                 /* polygon at node */
    PtItem*     verts;                /* sorted list of vertices of */
                                      /* voronoi polygon */
} Info_t;

extern Info_t*    nodeInfo;                 /* Array of node info */

extern void infoinit();
  /* Insert vertex into sorted list */
extern void addVertex (Site*, double, double);  
#endif
