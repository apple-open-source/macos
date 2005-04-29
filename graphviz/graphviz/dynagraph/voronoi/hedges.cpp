/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "voronoi/hedges.h"

namespace Voronoi {

const Edge *DELETED = reinterpret_cast<Edge*>(-2);

Halfedges::Halfedges(Sites &sites,int N) : 
	fhedges(ROUND(sqrt((double)N))),bottomsite(0),sites(sites) {
	init(N);
}
void Halfedges::init(int N) {
    int hs = int(2 * sqrt((double)N));
	hash.clear();
	hash.resize(hs,0);
    leftend = create( (Edge *)NULL, le);
    rightend = create( (Edge *)NULL, le);
    leftend -> ELleft = (Halfedge *)NULL;
    leftend -> ELright = rightend;
    rightend -> ELleft = leftend;
    rightend -> ELright = (Halfedge *)NULL;
    hash[0] = leftend;
    hash[hash.size()-1] = rightend;
    PQhashsize = int(4 * sqrt((double)N));
	PQinitialize();
}

Site *Halfedges::hintersect(Halfedge *el1, Halfedge *el2) {
    Edge *e1 = el1 -> ELedge,
		*e2 = el2 -> ELedge;
    if(!e1 || !e2)
    	return 0;
    if(e1->reg[1] == e2->reg[1]) 
		return 0;

    double d = e1->a * e2->b - e1->b * e2->a;
    if (-1.0e-10<d && d<1.0e-10) 
		return 0;

    Coord intr((e1->c*e2->b - e2->c*e1->b)/d,
		(e2->c*e1->a - e1->c*e2->a)/d);

    Edge *e;
    Halfedge *el;
    if( (e1->reg[1]->coord.y < e2->reg[1]->coord.y) ||
        (e1->reg[1]->coord.y == e2->reg[1]->coord.y &&
    	e1->reg[1]->coord.x < e2->reg[1]->coord.x) )
    {	el = el1; e = e1;}
    else
    {	el = el2; e = e2;};

    bool right_of_site = intr.x >= e -> reg[1] -> coord.x;
    if ((right_of_site && el -> ELpm == le) ||
       (!right_of_site && el -> ELpm == re)) 
	   return 0;

    Site *v = sites.getsite ();
    v -> refcnt = 0;
    v -> coord = intr;
    return v;
}

/* returns 1 if c is to right of halfedge e */
bool right_of(Halfedge *el, Coord c) {
    Edge *e;
    Site *topsite;
    int right_of_site, above, fast;
    double dxp, dyp, dxs, t1, t2, t3, yl;

    e = el -> ELedge;
    topsite = e -> reg[1];
    right_of_site = c.x > topsite -> coord.x;
    if(right_of_site && el -> ELpm == le) return(1);
    if(!right_of_site && el -> ELpm == re) return (0);

    if (e->a == 1.0)
    {	dyp = c.y - topsite->coord.y;
    	dxp = c.x - topsite->coord.x;
    	fast = 0;
    	if ((!right_of_site && e->b<0.0) || (right_of_site&&e->b>=0.0) )
    	{	above = dyp>= e->b*dxp;	
    		fast = above;
    	}
    	else 
    	{	above = c.x + c.y*e->b > e-> c;
    		if(e->b<0.0) above = !above;
    		if (!above) fast = 1;
    	};
    	if (!fast)
    	{	dxs = topsite->coord.x - (e->reg[0])->coord.x;
    		above = e->b * (dxp*dxp - dyp*dyp) <
    	        	dxs*dyp*(1.0+2.0*dxp/dxs + e->b*e->b);
    		if(e->b<0.0) above = !above;
    	};
    }
    else  /*e->b==1.0 */
    {	yl = e->c - e->a*c.x;
    	t1 = c.y - yl;
    	t2 = c.x - topsite->coord.x;
    	t3 = yl - topsite->coord.y;
    	above = t1*t1 > t2*t2 + t3*t3;
    };
    return (el->ELpm==le ? above : !above);
}

Halfedge *Halfedges::create(Edge *e, EdgeEnd pm) {
    Halfedge *answer;
    answer = fhedges.alloc();
    answer -> ELedge = e;
    answer -> ELpm = pm;
    answer -> PQnext = (Halfedge *) NULL;
    answer -> vertex = (Site *) NULL;
    answer -> ELrefcnt = 0;
    return(answer);
}


void Halfedges::insert(Halfedge *lb, Halfedge *he) {
    he -> ELleft = lb;
    he -> ELright = lb -> ELright;
    lb -> ELright -> ELleft = he;
    lb -> ELright = he;
}

/* Get entry from hash table, pruning any deleted nodes */
Halfedge *Halfedges::gethash(int b) {
    Halfedge *he;

    if(b<0 || unsigned(b)>=hash.size()) 
		return 0;
    he = hash[b]; 
    if(!he || he -> ELedge != DELETED) 
		return he;

/* Hash table points to deleted half edge.  Patch as necessary. */
    hash[b] = 0;
    if((he -> ELrefcnt -= 1) == 0) 
		fhedges.free(he);
    return 0;
}	

Halfedge *Halfedges::leftbnd(Coord c) {
    Halfedge *he;

/* Use hash table to get close to desired halfedge */
    unsigned bucket = unsigned((c.x - range.l)/range.Width() * hash.size());
    if(bucket<0) 
		bucket =0;
    if(bucket>=hash.size()) 
		bucket = hash.size() - 1;
    he = gethash(bucket);
    if(!he) 
		for(int i=1; 1 ; i += 1) {
			if ((he=gethash(bucket-i)) != (Halfedge *) NULL) break;
    		if ((he=gethash(bucket+i)) != (Halfedge *) NULL) break;
        }
/* Now search linear list of halfedges for the corect one */
	if(he==leftend || (he != rightend && right_of(he,c))) {
		do he = he -> ELright;
		while (he!=rightend && right_of(he,c));
		he = he -> ELleft;
    }
    else do he = he -> ELleft;
		while (he!=leftend && !right_of(he,c));

/* Update hash table and reference counts */
    if(bucket > 0 && bucket < hash.size()-1) {
		if(hash[bucket])
    		hash[bucket] -> ELrefcnt -= 1;
    	hash[bucket] = he;
    	hash[bucket] -> ELrefcnt += 1;
    };
    return he;
}

    
/* This delete routine can't reclaim node, since pointers from hash
   table may be present.   */
void Halfedges::erase(Halfedge *he) {
    (he -> ELleft) -> ELright = he -> ELright;
    (he -> ELright) -> ELleft = he -> ELleft;
    he -> ELedge = (Edge *)DELETED;
}


Site *Halfedges::leftreg(Halfedge *he) {
    if(!he -> ELedge) 
		return bottomsite;
    return he -> ELedge -> reg[he -> ELpm];
}

Site *Halfedges::rightreg(Halfedge *he) {
    if(!he -> ELedge) 
		return bottomsite;
    return he -> ELedge -> reg[opp(he -> ELpm)];
}



} // namespace Voronoi
