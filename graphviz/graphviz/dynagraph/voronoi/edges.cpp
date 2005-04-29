/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */
#include "voronoi/edges.h"


namespace Voronoi {

Edge *Edges::bisect(Site *s1, Site *s2) {
    double dx,dy,adx,ady;
    Edge *newedge;

    newedge = fedges.alloc();

    newedge -> reg[0] = s1;
    newedge -> reg[1] = s2;
    sites.ref(s1); 
    sites.ref(s2);
    newedge -> ep[0] = (Site *) 0;
    newedge -> ep[1] = (Site *) 0;

    dx = s2->coord.x - s1->coord.x;
    dy = s2->coord.y - s1->coord.y;
    adx = absol(dx);
    ady = absol(dy);
    newedge -> c = s1->coord.x * dx + s1->coord.y * dy + (dx*dx + dy*dy)*0.5;
    if (adx>ady)
    {	newedge -> a = 1.0; newedge -> b = dy/dx; newedge -> c /= dx;}
    else
    {	newedge -> b = 1.0; newedge -> a = dx/dy; newedge -> c /= dy;};

    newedge -> edgenbr = nedges;
    nedges += 1;
    return(newedge);
}


void Edges::doSeg (Edge *e, double x1, double y1, double x2, double y2) {
	infos.addVertex (e->reg[0], Coord(x1, y1));
	infos.addVertex (e->reg[0], Coord(x2, y2));
	infos.addVertex (e->reg[1], Coord(x1, y1));
	infos.addVertex (e->reg[1], Coord(x2, y2));
}
void Edges::clip_line(Edge *e) {
    Site *s1, *s2;
    double x1,x2,y1,y2;

    if(e -> a == 1.0 && e ->b >= 0.0) {    
        s1 = e -> ep[1];
        s2 = e -> ep[0];
    }
    else {
        s1 = e -> ep[0];
        s2 = e -> ep[1];
    }
#ifdef THROW_LESS_AWAY
    if(e -> a == 1.0) {
        if (s1!=(Site *)0) {
          y1 = s1->coord.y;
          if (y1 > bounds.t) {
			  y1 = bounds.t;
            x1 = e -> c - e -> b * y1;
		  }
          else if (y1 >= bounds.b) x1 = s1->coord.x;
          else {
            y1 = bounds.b;
            x1 = e -> c - e -> b * y1;
          }
        }
        else {
          y1 = bounds.b;
          x1 = e -> c - e -> b * y1;
        }
            
        if (s2!=(Site *)0) {
          y2 = s2->coord.y;
          if(y2<bounds.b) {
			  y2 = bounds.b;
            x2 = e -> c - e -> b * y2;
		  }
          else if (y2 <= bounds.t) x2 = s2->coord.x;
          else {
            y2 = bounds.t;
            x2 = e -> c - e -> b * y2;
          }
        }
        else {
          y2 = bounds.t;
          x2 = e -> c - e -> b * y2;
        }

        if ((x1> bounds.r && x2>bounds.r) || (x1<bounds.l&&x2<bounds.l)) 
			return;
        if(x1> bounds.r)
        {    x1 = bounds.r; y1 = (e -> c - x1)/e -> b;};
        if(x1<bounds.l)
        {    x1 = bounds.l; y1 = (e -> c - x1)/e -> b;};
        if(x2>bounds.r)
        {    x2 = bounds.r; y2 = (e -> c - x2)/e -> b;};
        if(x2<bounds.l)
        {    x2 = bounds.l; y2 = (e -> c - x2)/e -> b;};
    }
    else {
        if (s1!=(Site *)0) {
          x1 = s1->coord.x;
          if(x1>bounds.r) {
			x1 = bounds.r;
  			y1 = e -> c - e -> a * x1;
		  }
          else if (x1 >= bounds.l) y1 = s1->coord.y;
          else {
            x1 = bounds.l;
            y1 = e -> c - e -> a * x1;
          }
        }
        else {
          x1 = bounds.l;
          y1 = e -> c - e -> a * x1;
        }

        if (s2!=(Site *)0) {
          x2 = s2->coord.x;
          if(x2<bounds.l) {
			  x2 = bounds.l;
            y2 = e -> c - e -> a * x2;
		  }
          else if (x2 <= bounds.r) y2 = s2->coord.y;
          else {
            x2 = bounds.r;
            y2 = e -> c - e -> a * x2;
          }
        }
        else {
          x2 = bounds.r;
          y2 = e -> c - e -> a * x2;
        }

        if ((y1> bounds.t && y2>bounds.t) || (y1<bounds.b && y2<bounds.b)) 
			return;
        if(y1> bounds.t)
        {    y1 = bounds.t; x1 = (e -> c - y1)/e -> a;};
        if(y1<bounds.b)
        {    y1 = bounds.b; x1 = (e -> c - y1)/e -> a;};
        if(y2>bounds.t)
        {    y2 = bounds.t; x2 = (e -> c - y2)/e -> a;};
        if(y2<bounds.b)
        {    y2 = bounds.b; x2 = (e -> c - y2)/e -> a;};
    }
#else
    if(e -> a == 1.0) {
        if (s1!=(Site *)0) {
          y1 = s1->coord.y;
          if (y1 > bounds.t) 
			  return;
          else if (y1 >= bounds.b) x1 = s1->coord.x;
          else {
            y1 = bounds.b;
            x1 = e -> c - e -> b * y1;
          }
        }
        else {
          y1 = bounds.b;
          x1 = e -> c - e -> b * y1;
        }
            
        if (s2!=(Site *)0) {
          y2 = s2->coord.y;
          if(y2<bounds.b) 
			  return;
          else if (y2 <= bounds.t) x2 = s2->coord.x;
          else {
            y2 = bounds.t;
            x2 = e -> c - e -> b * y2;
          }
        }
        else {
          y2 = bounds.t;
          x2 = e -> c - e -> b * y2;
        }

        if ((x1> bounds.r && x2>bounds.r) || (x1<bounds.l&&x2<bounds.l)) 
			return;
        if(x1> bounds.r)
        {    x1 = bounds.r; y1 = (e -> c - x1)/e -> b;};
        if(x1<bounds.l)
        {    x1 = bounds.l; y1 = (e -> c - x1)/e -> b;};
        if(x2>bounds.r)
        {    x2 = bounds.r; y2 = (e -> c - x2)/e -> b;};
        if(x2<bounds.l)
        {    x2 = bounds.l; y2 = (e -> c - x2)/e -> b;};
    }
    else {
        if (s1!=(Site *)0) {
          x1 = s1->coord.x;
          if(x1>bounds.r) 
			  return;
          else if (x1 >= bounds.l) y1 = s1->coord.y;
          else {
            x1 = bounds.l;
            y1 = e -> c - e -> a * x1;
          }
        }
        else {
          x1 = bounds.l;
          y1 = e -> c - e -> a * x1;
        }

        if (s2!=(Site *)0) {
          x2 = s2->coord.x;
          if(x2<bounds.l) 
			  return;
          else if (x2 <= bounds.r) y2 = s2->coord.y;
          else {
            x2 = bounds.r;
            y2 = e -> c - e -> a * x2;
          }
        }
        else {
          x2 = bounds.r;
          y2 = e -> c - e -> a * x2;
        }

        if ((y1> bounds.t && y2>bounds.t) || (y1<bounds.b && y2<bounds.b)) 
			return;
        if(y1> bounds.t)
        {    y1 = bounds.t; x1 = (e -> c - y1)/e -> a;};
        if(y1<bounds.b)
        {    y1 = bounds.b; x1 = (e -> c - y1)/e -> a;};
        if(y2>bounds.t)
        {    y2 = bounds.t; x2 = (e -> c - y2)/e -> a;};
        if(y2<bounds.b)
        {    y2 = bounds.b; x2 = (e -> c - y2)/e -> a;};
    }
#endif    
    doSeg(e,x1,y1,x2,y2);
#ifdef VORLINES_CLIPEDGES
	Layout *l = infos.nodes.front().layoutN->g;
	Line seg;
	seg.degree = 1;
	seg.push_back(Coord(x1,y1));
	seg.push_back(Coord(x2,y2));
	gd<Drawn>(l).push_back(seg);
#endif
#ifdef STANDALONE
    if (doPS) line (x1,y1,x2,y2);
#endif
}
void Edges::endpoint(Edge *e, EdgeEnd lr, Site *s) {
    e -> ep[lr] = s;
    sites.ref(s);
    if(e -> ep[opp(lr)] == (Site *) 0) return;
    clip_line (e);
#ifdef STANDALONE
    out_ep(e);
#endif
    sites.deref(e->reg[le]);
    sites.deref(e->reg[re]);
    fedges.free(e);
}



} // namespace Voronoi
