/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"
#include "common/PathPlan.h"

using namespace std;

namespace DynaDAG {

struct RouteBounds {
	RouteBounds(Config &config,Bounds &bounds) : config(config),bounds(bounds) {
		assert(bounds.valid);
	}
	void poly(Line &out);
	void term(DDModel::Edge *e,Coord t, bool start);
	void path(DDModel::Edge *e);
private:
	Config &config;
	Bounds &bounds;
	Line left,right; /* left and right sides of region */
	Line &getSide(LeftRight side) {
		if(side==LEFT)
			return left;
		else {
			assert(side==RIGHT);
			return right;
		}
	}
	void appendPoint(LeftRight side,Coord c) {
		Line &l = getSide(side);
		if(l.size()) {
			Coord c0 = getSide(side).back();
#ifndef DOWN_GREATER
			assert(c0.y>=c.y);
#else
			assert(c0.y<=c.y);
#endif
			if(c==c0)
				return;
		}
		getSide(side).push_back(c);
	}
	void appendQuad(double l1,double r1,double l2,double r2,double y1,double y2) {
		appendPoint(LEFT,Coord(l1,y1));
		appendPoint(LEFT,Coord(l2,y2));
		appendPoint(RIGHT,Coord(r1,y1));
		appendPoint(RIGHT,Coord(r2,y2));
	}
	double side(DDModel::Node *n,int s);
	bool localCrossing(DDModel::Edge *e,UpDown ud, DDModel::Node *n);
	double bounding(DDModel::Edge *e,LeftRight lr,UpDown ud,bool skipNodeEnds);
};
void RouteBounds::poly(Line &out) {
	out.Clear();
	out.degree = 1;
	for(Line::iterator pi = left.begin(); pi!=left.end(); ++pi)
		if(out.empty() || out.back()!=*pi)
			out.push_back(*pi);
	for(Line::reverse_iterator pri = right.rbegin(); pri!=right.rend(); ++pri)
		if(out.empty() || out.back()!=*pri)
			out.push_back(*pri);
}
double RouteBounds::side(DDModel::Node *n,int s) {
	return DDd(n).cur.x + s*(((s==LEFT)?config.LeftExtent(n):config.RightExtent(n)) + config.nodeSep.x/2.0);
}
bool RouteBounds::localCrossing(DDModel::Edge *e,UpDown ud, DDModel::Node *n) {
	const int dist=2;
	assert(DDd(n).amEdgePart());
	DDModel::Node *v = ud==UP?e->tail:e->head,
		*w = n;
	assert(DDd(v).rank==DDd(w).rank);
	bool vw = DDd(v).order<DDd(w).order;
	int i;
	for(i = 0;i<dist;++i) {
		DDModel::Node *vv,*ww;
		if(DDd(w).amNodePart())
			break;
		ww = (*w->outs().begin())->head;
		if(v==e->tail)
			vv = e->head;
		else if(DDd(v).amNodePart())
			break;
		else
			vv = (*v->outs().begin())->head;
		if(vv==ww) // common end node
			return false;
		assert(DDd(vv).rank==DDd(ww).rank);
		if((DDd(vv).order<DDd(ww).order) != vw)
			return true;
		v = vv;
		w = ww;
	}
	v = ud==UP?e->tail:e->head;
	w = n;
	for(i = 0;i<dist;++i) {
		DDModel::Node *vv,*ww;
		if(DDd(w).amNodePart())
			break;
		ww = (*w->ins().begin())->tail;
		if(v==e->head)
			vv = e->tail;
		else if(DDd(v).amNodePart())
			break;
		else
			vv = (*v->ins().begin())->tail;
		if(vv==ww) // common end node
			return false;
		assert(DDd(vv).rank==DDd(ww).rank);
		if((DDd(vv).order<DDd(ww).order) != vw)
			return true;
		v = vv;
		w =ww;
	}
	return false;
}
double RouteBounds::bounding(DDModel::Edge *e,LeftRight lr,UpDown ud, bool skipNodeEnds) {
	DDModel::Node *n = ud==DOWN?e->head:e->tail;
	DDModel::Node *q = n;
	while((q = config.RelNode(q,lr))) {
		if(DDd(q).amNodePart()) {
			if(skipNodeEnds) {
				if(q==DDd(q).multi->top() && ud==DOWN && q->ins().empty())
					continue;
				if(q==DDd(q).multi->bottom() && ud==UP && q->outs().empty())
					continue;
			}
			break;
		}
		else if(!localCrossing(e,ud,q))
			break;
	}
	return q?side(q,-lr):(lr==LEFT?bounds.l:bounds.r);
}
void RouteBounds::term(DDModel::Edge *e,Coord t, bool start) {
	DDModel::Node *n = start?e->tail:e->head;
	double l = DDd(n).cur.x - config.LeftExtent(n),
		r = DDd(n).cur.x + config.RightExtent(n),
		el = bounding(e,LEFT,start?UP:DOWN,false),
		er = bounding(e,RIGHT,start?UP:DOWN,false);
	double edge = config.ranking.GetRank(DDd(n).rank)->yBase;
	if(start) 
		appendQuad(l,r,el,er,t.y,edge);
	else
		appendQuad(el,er,l,r,edge,t.y);
}
void RouteBounds::path(DDModel::Edge *e) {
	double tl = bounding(e,LEFT,UP,true),
		tr = bounding(e,RIGHT,UP,true),
		hl = bounding(e,LEFT,DOWN,true),
		hr = bounding(e,RIGHT,DOWN,true),
		ty = config.ranking.GetRank(DDd(e->tail).rank)->yBase,
		hy = config.ranking.GetRank(DDd(e->head).rank)->yBase;
	appendQuad(tl,tr,hl,hr,ty,hy);
}
bool FlexiSpliner::MakeEdgeSpline(DDPath *path,SpliningLevel level) {
	assert(path->unclippedPath.Empty());

	DDModel::Node *tl = DDp(path->layoutE->tail)->bottom(),
		*hd = DDp(path->layoutE->head)->top();

	bool reversed = DDd(tl).rank > DDd(hd).rank,
		flat = false;
	if(reversed) {
		tl = DDp(path->layoutE->head)->bottom();
		hd = DDp(path->layoutE->tail)->top();
		if(DDd(tl).rank>DDd(hd).rank)
			flat = true;
	}
	EdgeGeom &eg = gd<EdgeGeom>(path->layoutE);
	Coord tailpt = eg.tailPort.pos + DDd(tl).multi->pos(),
		headpt = eg.headPort.pos + DDd(hd).multi->pos();

	Line &unclipped = path->unclippedPath;
	if(path->layoutE->tail==path->layoutE->head) {	/* self arc */
	}
	else {
		Line region;
		if(flat)
			;
		else {
			RouteBounds rb(config,gd<GraphGeom>(path->layoutE->g).bounds);
			//rb.term(path->first,tailpt,true);
			for(DDMultiNode::edge_iter ei0 = DDd(tl).multi->eBegin(); ei0!=DDd(tl).multi->eEnd(); ++ei0)
				rb.path(*ei0);
			for(DDPath::edge_iter ei = path->eBegin(); ei!=path->eEnd(); ++ei)
				rb.path(*ei);
			for(DDMultiNode::edge_iter ei2 = DDd(hd).multi->eBegin(); ei2!=DDd(hd).multi->eEnd(); ++ei2)
				rb.path(*ei2);
			//rb.term(path->last,headpt,false);
			rb.poly(region);
		}
		switch(level) {
		case DG_SPLINELEVEL_BOUNDS:
			eg.pos.ClipEndpoints(region,Coord(),0,Coord(),0); // silly way to copy line
			return true;
		case DG_SPLINELEVEL_SHORTEST:
		case DG_SPLINELEVEL_SPLINE: {
			try {
				Line polylineRoute;
				PathPlan::Shortest(region,Segment(tailpt,headpt),polylineRoute);
				if(level==DG_SPLINELEVEL_SPLINE) {
					PathPlan::SegmentV barriers;
					PathPlan::PolyBarriers(PathPlan::LineV(1,region),barriers);
					
					Segment endSlopes(Coord(0.0,0.0),Coord(0.0,0.0));
					check(PathPlan::Route(barriers,polylineRoute,endSlopes,unclipped));
				}
				else
					unclipped = polylineRoute;
			}
			catch(...) {
				return false;
			}
			break;
		}
		default:
			assert(0);
		}
	}
	NodeGeom &tg = gd<NodeGeom>(path->layoutE->tail),
		&hg = gd<NodeGeom>(path->layoutE->head);
	if(reversed) {
		eg.pos.ClipEndpoints(path->unclippedPath,hg.pos,eg.headClipped?&hg.region:0,
			tg.pos,eg.tailClipped?&tg.region:0);
		reverse(eg.pos.begin(),eg.pos.end());
	}
	else 
		eg.pos.ClipEndpoints(path->unclippedPath,tg.pos,eg.tailClipped?&tg.region:0,
			hg.pos,eg.headClipped?&hg.region:0);
	return true;
}

}
