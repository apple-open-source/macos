/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef SITE_H
#define SITE_H

#include "common/Geometry.h"
#include "common/freelist.h"

namespace Voronoi {

struct Site {
	Coord coord;
	int sitenbr;
	int refcnt;
	Site() : sitenbr(-1),refcnt(0) {}
};

struct Sites {
	Freelist<Site> fsites;
	int nvertices;
	Site *getsite();
	void makevertex(Site*);     /* Transform a site into a vertex */
	void deref(Site*);        
	void ref(Site*);            

	Sites(int N) : fsites(ROUND(sqrt((double)N))), nvertices(0) {}

};

} // namespace Voronoi
#endif // SITE_H
