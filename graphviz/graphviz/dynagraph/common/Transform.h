/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef Transform_h
#define Transform_h

#include "common/Geometry.h"
// dynagraph uses dimensionless up-positive coords, but not everyone else does!
// so when it reads or writes .dot files or talks over a pipe, it uses one of these:
// multiplies by relevant ratios on read, divides on write
class Transform {
	Coord nodeRatio, // for node size
		coordRatio; // for node position, edge coords, and bounding box
public:
	Coord ll; // lower left corner
	Transform(Coord nr,Coord cr) : nodeRatio(nr),coordRatio(cr) {}

	Coord inSize(Coord c) {
		return Coord(c.x*nodeRatio.x,c.y*nodeRatio.y);
	}
	Coord outSize(Coord c) {
		return Coord(c.x/nodeRatio.x,c.y/nodeRatio.y);
	}
	Coord in(Coord c) {
		return Coord((c+ll).x*coordRatio.x,(c+ll).y*coordRatio.y);
	}
	Coord out(Coord c) {
		return Coord(c.x/coordRatio.x-ll.x,c.y/coordRatio.y-ll.y);
	}
	Rect in(Rect r) {
		Coord ul = in(Coord(r.l,r.t)),
			lr = in(Coord(r.r,r.b));
		return Rect(ul.x,ul.y,lr.x,lr.y);
	}
	Rect out(Rect r) {
		Coord ul = out(Coord(r.l,r.t)),
			lr = out(Coord(r.r,r.b));
		return Rect(ul.x,ul.y,lr.x,lr.y);
	}
};
// dot transform: node size in inches, coords in down-positive points
// (dynagraph will do calcs in up-positive points)
extern Transform g_dotRatios;

#endif // Transform_h
