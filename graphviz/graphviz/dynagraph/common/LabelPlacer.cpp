/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"

using namespace std;

void placeLabels(Layout::Edge *e) {
	EdgeLabels &el = gd<EdgeLabels>(e);
	if(el.empty())
		return;

	EdgeGeom &eg = gd<EdgeGeom>(e);
	Drawn &drawn = gd<Drawn>(e);
	drawn.clear();
	drawn.push_back(eg.pos);
	Line &l = eg.pos;
	if(l.empty())
		return;
	segsizes sizes(&*l.begin(),l.size(),l.degree);
	for(EdgeLabels::iterator il = el.begin(); il!=el.end(); ++il) 
		if(l.size()) {
			pair<Coord,Coord> pc = secant(&*l.begin(),l.size(),l.degree,sizes,il->where,il->length);
			if(il->shape) {
				Coord dq = pc.second-pc.first,
					dp = il->pos2 - il->pos1;
				double thq = atan2(dq.y,dq.x),
					thp = atan2(dp.y,dp.x),
					dth = thq-thp,
					mult = dq.Len()/dp.Len();
				drawn.push_back(Line());
				Line &dest = drawn.back();
				dest.reserve(il->shape->size());
				for(Line::iterator pi = il->shape->begin(); pi!=il->shape->end(); ++pi) {
					Coord r = *pi - il->pos1,
						r2(r.x*cos(dth)-r.y*sin(dth),r.y*cos(dth)+r.x*sin(dth));
					r2 *= mult;
					r2 += pc.first;
					dest.push_back(r2);
				}
			}
			else {
				il->pos1 = pc.first;
				il->pos2 = pc.second;
			}
		}
		else
			il->pos1 = il->pos2 = Position();
}
NodeLabelPlacement xlateNodeLabelPlacement(NodeLabelPlacement in,Orientation rient) {
	if(1)
		return in;
	// both measure rotation in 90° increments but NodeLabelPlacement starts with 1
	return (NodeLabelPlacement)((in+rient)%4+1);  // terse!
}
void placeLabels(Layout::Node *n) {
	NodeLabels &nl = gd<NodeLabels>(n);
	if(nl.empty())
		return;

	NodeGeom &ng = gd<NodeGeom>(n);
	Coord start = ng.pos;
	Coord gap = gd<GraphGeom>(n->g).labelGap;
	for(NodeLabels::iterator il = nl.begin(); il!=nl.end(); ++il) {
		Coord ul = il->size/-2.0;
#ifndef DOWN_GREATER
		ul.y *= -1.0;
#endif
		il->pos = start + ul;
		switch(xlateNodeLabelPlacement(il->where,gd<Translation>(n->g).orientation)) {
		case DG_NODELABEL_CENTER:
			break;
		case DG_NODELABEL_LEFT:
			il->pos.x = ng.BoundingBox().l - gap.x - il->size.x;
			break;
		case DG_NODELABEL_TOP:
			il->pos.y = ng.BoundingBox().t 
#ifndef DOWN_GREATER
				+ 
#else
				-
#endif
				(gap.y + il->size.y);
			break;
		case DG_NODELABEL_RIGHT:
			il->pos.x = ng.BoundingBox().r + gap.x;
			break;
		case DG_NODELABEL_BOTTOM:
			il->pos.y = ng.BoundingBox().b
#ifndef DOWN_GREATER
				- 
#else
				+
#endif
				gap.y;
			break;
		}
	}
}

void LabelPlacer::Process(ChangeQueue &Q) {
	Layout::graphedge_iter ei = Q.insE.edges().begin();
	for(; ei!=Q.insE.edges().end(); ++ei)
		placeLabels(*ei);
	for(ei = Q.modE.edges().begin(); ei!=Q.modE.edges().end(); ++ei)
		if(igd<Update>(*ei).flags&(DG_UPD_MOVE|DG_UPD_LABEL))
			placeLabels(*ei);
	Layout::node_iter ni;
	for(ni = Q.insN.nodes().begin(); ni!=Q.insN.nodes().end(); ++ni)
		placeLabels(*ni);
	for(ni = Q.modN.nodes().begin(); ni!=Q.modN.nodes().end(); ++ni)
		if(igd<Update>(*ni).flags&(DG_UPD_MOVE|DG_UPD_LABEL))
			placeLabels(*ni);
}
