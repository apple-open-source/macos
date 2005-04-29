/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <assert.h>
#include "PathPlan.h"

void PathPlan::PolyBarriers(const LineV &polygons, SegmentV &out) {
	unsigned n = 0,i;
	for(i = 0; i < polygons.size(); i++)
		n += polygons[i].size();

	out.reserve(n);
	for(i = 0; i < polygons.size(); i++) {
		const Line &pp = polygons[i];
		for(unsigned j = 0; j < pp.size(); j++) {
			int k = (j + 1)%pp.size();
			out.push_back(Segment(pp[j],pp[k]));
		}
	}
	assert(out.size() == n);
}
