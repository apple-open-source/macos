/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Geometry.h"

namespace PathPlan {
	typedef std::vector<Segment> SegmentV;
	typedef std::vector<Line> LineV;

    struct InvalidBoundary : DGException {
	  InvalidBoundary() : DGException("the shortest path algorithm requires a polygon boundary") {}
	};
	struct EndpointNotInPolygon : DGException { 
	  bool which; 
	  EndpointNotInPolygon(bool which) : 
	    DGException("the shortest path algorithm requires end-points within the boundary polygon"),
	    which(which) {}
	};
	/* find shortest euclidean path within a simple polygon */
	void Shortest(const Line &boundary, Segment endpoints, Line &out);

	/* fit a spline to an input polyline, without touching barrier segments */
	bool Route(const SegmentV &barriers, const Line &inputRoute,
		Segment endSlopes,Line &out);

	/* utility function to convert from a set of polygonal obstacles to barriers */
	void PolyBarriers(const LineV &polygons, SegmentV &out);
}
