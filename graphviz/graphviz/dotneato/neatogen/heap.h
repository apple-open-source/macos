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

#ifndef HEAP_H
#define HEAP_H

#include "hedges.h"

extern void PQinitialize();
extern void PQcleanup();
extern Halfedge * PQextractmin();
extern Point PQ_min();
extern int PQempty();
extern void PQdelete(Halfedge*);
extern void PQinsert(Halfedge*, Site*, double);

#endif
