/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <stdio.h>
#include "fdp.h"
#include "macros.h"

namespace FDP {

#ifndef offsetof
#define offsetof(typ,fld)  ((int)(&(((typ*)0)->fld)))
#endif

static int ijcmpf(Dt_t *d, void *key1, void *key2, Dtdisc_t *disc) {
    Cell *cell1 = reinterpret_cast<Cell*>(key1);
    Cell *cell2 = reinterpret_cast<Cell*>(key2);
    int diff;

    NOTUSED(d);
    NOTUSED(disc);
    /*return((cell1->p.i != cell2->p.i) || (cell1->p.j != cell2->p.j)); */
    diff = cell1->p.i - cell2->p.i;
    if(diff != 0)
      return diff;
    else
      return(cell1->p.j - cell2->p.j);
}

static void *newCell(Dt_t *d, void *obj, Dtdisc_t *disc) {
    Cell *cellp = reinterpret_cast<Cell*>(obj),
		*newp;

    NOTUSED(disc);
    newp = static_cast<Grid*>(d)->getCell();
    newp->p.i = cellp->p.i;
    newp->p.j = cellp->p.j;

    return newp;
}

static Dtdisc_t gridDisc = {
    offsetof(PODCell, p),
    sizeof(gridpt),
    offsetof(PODCell,link),
    newCell,
    NIL(Dtfree_f),
    ijcmpf,
    NIL(Dthash_f),
    NIL(Dtmemory_f),
    NIL(Dtevent_f)
} ;
Grid::Grid(int size) : cells(ROUND(sqrt((double)size))){
	open(&gridDisc, Dtoset);
}
Cell *Grid::getCell() {
	return cells.alloc();
}
void Grid::add(int i, int j, FDPModel::Node *n) {
    Cell *cellp;
    Cell key;

    key.p.i = i;
    key.p.j = j;
    cellp = reinterpret_cast<Cell*>(dtinsert(this, &key));
    cellp->nodes.push_front(n);
	/*
    if(Verbose >= 3) {
      fprintf(stderr, "grid(%d,%d): %s\n", i, j, n->name);
    }
	*/
}
Cell *Grid::find(int i, int j) {
    Cell key;

    key.p.i = i;
    key.p.j = j;
    return reinterpret_cast<Cell*>(dtsearch(this, &key));
}

void Grid::walk(int(*walkf)(Dt_t*,void*,void*)) {
    dtwalk(this, walkf, this);
}
int visitingWalk(Dt_t *dt,void *obj,void *parm) {
	Grid *grid = static_cast<Grid*>(dt);
	Cell *cell = reinterpret_cast<Cell*>(obj);
	Grid::Visitor *visitor = reinterpret_cast<Grid::Visitor*>(parm);
	return visitor->VisitCell(cell,grid);
}
void Grid::walk(Visitor *visitor) {
	dtwalk(this,visitingWalk,visitor);
}


int gLength(Cell *p) {
	return p->nodes.size();
}

}
