/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef HEDGES_H
#define HEDGES_H

#include "voronoi/site.h"
#include "voronoi/edges.h"

namespace Voronoi {

struct Halfedge {
    Halfedge    *ELleft, *ELright;
    Edge               *ELedge;
    int                ELrefcnt;
    EdgeEnd            ELpm;
    Site               *vertex;
    double              ystar;
    Halfedge    *PQnext;
};

struct Halfedges {
	Halfedge *leftend, *rightend;
	Freelist<Halfedge> fhedges;
	std::vector<Halfedge*> hash;
	Rect range; // of site coords
	Site *bottomsite; // weird to hold this here but this is the only class that uses it
	Sites &sites;

	Halfedges(Sites &sites,int N);
	void init(int N);
	Site *hintersect(Halfedge*, Halfedge*);
	Halfedge *create(Edge*, EdgeEnd);
	void insert(Halfedge *where, Halfedge *he);
	Halfedge *leftbnd(Coord);
	void erase(Halfedge *);
	Halfedge *ELleftbnd(Coord);
	Site *leftreg(Halfedge*), *rightreg(Halfedge*);

	void PQinitialize();
	Halfedge * PQextractmin();
	Coord PQ_min();
	int PQempty();
	void PQdelete(Halfedge*);
	void PQinsert(Halfedge*, Site*, double);

private:
	Halfedge *gethash(int b);

	std::vector<Halfedge> PQhash;
	int PQhashsize;
	int PQcount;
	int PQmin;

	int PQbucket(Halfedge *he);
};
bool right_of(Halfedge*, Coord);
inline Halfedge *left(Halfedge*he) {
	return he->ELleft;
}
inline Halfedge *right(Halfedge*he) {
	return he->ELright;
}

} // namespace Voronoi
#endif
