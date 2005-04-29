/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef GRID_H
#define GRID_H

#include "cdt.h"
#include "common/derivable_dt.h"
#include "common/freelist.h"
#include <list>

struct gridpt {
	int i, j;
};
typedef std::list<FDPModel::Node*> node_list;
// this extra struct is to evade gcc3 warnings, which
// do not like offsetof(complicated struct)
struct PODCell {
  gridpt p;
  Dtlink_t link;
};
struct Cell : PODCell {
  node_list nodes;
};
struct Grid : derivable_dt { // cells indexed by (i,j)
	Freelist<Cell> cells; // this is wasteful; isn't deque supposed to not realloc if you just push?
	Grid(int size);
	~Grid() {
		close();
	}
	Cell *getCell();
	void clear() {
		dtclear(this);
		cells.clear();
	}
	void add(int i, int j, FDPModel::Node*);
	Cell* find(int i, int j);
	void walk(int(*walkf)(Dt_t *dt,void *cell,void *grid));
	struct Visitor {
		virtual int VisitCell(Cell *cell,Grid *grid) = 0;
	};
	void walk(Visitor *visitor);
};

extern int gLength (Cell* p);

#endif
