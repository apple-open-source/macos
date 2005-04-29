/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef EDGES_H
#define EDGES_H

#include "voronoi/site.h"
#include "voronoi/info.h"

namespace Voronoi {

struct Edge {
    double      a,b,c;         /* edge on line ax + by = c */
    Site       *ep[2];        /* endpoints (vertices) of edge; initially NULL */
    Site       *reg[2];       /* sites forming edge */
    int        edgenbr;
};

typedef enum _EdgeEnds {le = 0, re = 1} EdgeEnd;
inline EdgeEnd opp(EdgeEnd e) {
	if(e==le)
		return re;
	else
		return le;
}

struct Edges {
	Freelist<Edge> fedges;
	int nedges;
	Sites &sites;
	Infos &infos;
	Bounds &bounds;

	Edges(Sites &sites,Infos &infos,Bounds &bounds,int N) : 
	  fedges(ROUND(sqrt((double)N))),
	  nedges(0),
	  sites(sites),
	  infos(infos),
	  bounds(bounds) {}

	void endpoint(Edge*, EdgeEnd, Site*);
	void clip_line(Edge *e);
	Edge *bisect(Site*, Site*);
private:
	void doSeg (Edge *e, double x1, double y1, double x2, double y2);
};

struct Whattux {};

} // namespace Voronoi
#endif
