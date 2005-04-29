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

#ifndef HEDGES_H
#define HEDGES_H

#include "site.h"
#include "edges.h"

typedef struct Halfedge {
    struct Halfedge    *ELleft, *ELright;
    Edge               *ELedge;
    int                ELrefcnt;
    char               ELpm;
    Site               *vertex;
    double              ystar;
    struct Halfedge    *PQnext;
} Halfedge;

extern Halfedge *ELleftend, *ELrightend;

extern void ELinitialize();
extern void ELcleanup();
extern int right_of(Halfedge*, Point*);
extern Site *hintersect(Halfedge*, Halfedge*);
extern Halfedge *HEcreate(Edge*, char);
extern void ELinsert(Halfedge *, Halfedge *);
extern Halfedge *ELleftbnd(Point*);
extern void ELdelete(Halfedge *);
extern Halfedge *ELleft(Halfedge*), *ELright(Halfedge*);
extern Halfedge *ELleftbnd(Point*);
extern Site *leftreg(Halfedge*), *rightreg(Halfedge*);

#endif
