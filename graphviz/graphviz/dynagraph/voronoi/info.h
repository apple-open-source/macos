/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef INFO_H
#define INFO_H

#include "common/Dynagraph.h"
#include "common/freelist.h"
#include "voronoi/site.h"

namespace Voronoi {

struct PtItem {           /* Point std::list */
	PtItem*    next;
	Coord             p;
	PtItem() : next(0) {}
};
struct Info {                  /* Info concerning site */
	Layout::Node *layoutN;     /* libgraph node */
	Site site;                 /* site used by voronoi code */
	bool overlaps;             /* true if node overlaps other nodes */
	PtItem *verts;             /* sorted std::list of vertices of */
							   /* voronoi polygon */
	Info() : layoutN(0),overlaps(false),verts(0) {}
};
struct Infos {
	Freelist<PtItem> fpoints;


	std::vector<Info> nodes;			/* Array of node info */

	Infos(int N) : fpoints(ROUND(sqrt((double)N))),nodes(N) {}

	/* Insert vertex into sorted std::list */
	void addVertex (Site*, Coord);  
};
inline double dist(Site *s, Site *t) {
	return ::dist(s->coord,t->coord);
}

} // namespace Voronoi
#endif // INFO_H
