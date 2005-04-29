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

/*
 * return polygon for a given route 
 */

  /*
static bool localCrossing(DDModel::Node *v, DDModel::Node *w) {
	const int pathlen = 2;
	bool vw_order = DDd(w).order > DDd(v).order;
	for(int pass = 0; pass < 2; pass++) {
		DDModel::Node *vv = v,
			*ww = w;
		for(int i = 0; i < pathlen; i++) {
			if(!DDd(vv).amEdgePart()) 
				break;
			if(!DDd(ww).amEdgePart())
				break;
			if(pass==0)
				vv = (*vv->ins().begin())->tail;
			else
				vv = (*vv->outs().begin())->head;
			if(pass==0)
				ww = (*ww->ins().begin())->tail;
			else
				ww = (*ww->outs().begin())->head;
			bool vvww_order = DDd(ww).order > DDd(vv).order;
			if(vw_order != vvww_order) 
				return true;
		}
	}
	return false;
}
  */
static DDModel::Node *boundingNode(Config &config,DDModel::Node *inp, LeftRight dir) {
	DDModel::Node *v = inp,*w;
	while((w = config.RelNode(v,dir))) {
		if(DDd(w).amNodePart())
			break;
		break;
		//if(!localCrossing(v,w)) break;
		v = w;
	}
	return w;
}

double findBound(Config &config,const Bounds &bb,DDModel::Node *u, LeftRight side) {
	assert(bb.valid);
	double sep = config.nodeSep.x/2.0;
	double ret;
	if(DDModel::Node *w = boundingNode(config,u,side)) {
		if(side==LEFT)
			ret = DDd(w).cur.x + config.RightExtent(w) + sep;
		else
			ret = DDd(w).cur.x - config.LeftExtent(w) - sep;
	}
	else 
		ret = side==RIGHT?bb.r:bb.l;
	return ret;
}

struct TempRoute {
	TempRoute(Config &config,const Bounds &bb) : config(config),bb(bb) {}
	void appendPoint(LeftRight side, Coord c);
	void appendPoint(LeftRight side, double x, double y) { appendPoint(side,Coord(x,y)); }
	void appendCurve(LeftRight side, Line &curve, double y0, double y1);
	void addExtrema(double y);
	void appendBox(int toprank, double f0, double f1);
	void appendSide(LeftRight side, double x, double top, double bot);
	void appendVNode(DDModel::Node *virt);
	void termRoute(DDModel::Node *n, DDModel::Edge *e, Coord port, double fract);
	void defineBoundaryPoly(Line &out);
private:
	double bound(DDModel::Node *u,LeftRight side) {
		return findBound(config,bb,u,side);
	}
	Config &config;
	const Bounds &bb;
	Line left,right; /* left and right sides of region */
	Line &getSide(LeftRight side) {
		assert(sequence(LEFT,side,RIGHT));
		if(side==LEFT)
			return left;
		else
			return right;
	}
};
void TempRoute::appendPoint(LeftRight side, Coord c) {
	if(getSide(side).size()) {
		Coord c0 = getSide(side).back();
#ifndef DOWN_GREATER
		assert(c0.y>=c.y);
#else
		assert(c0.y<=c.y);
#endif
		if(c==c0)
			return;
		assert(find(getSide(side).begin(),getSide(side).end(),c)==getSide(side).end());
	}
	getSide(side).push_back(c);
}
void TempRoute::appendCurve(LeftRight side, Line &curve, double y0, double y1) {
	if(y0 > y1) 
		swap(y0,y1);
	appendPoint(side,checkPos(curve.YIntersection(y0)));
	appendPoint(side,checkPos(curve.YIntersection(y1)));
}

void TempRoute::addExtrema(double y) {
	appendPoint(LEFT,bb.l,y);
	appendPoint(RIGHT,bb.r,y);
}

void TempRoute::appendBox(int toprank, double f0, double f1) {
	Rank *rank = config.ranking.GetRank(toprank);
	double y = rank->yBelow(f0),
		y2 = rank->yBelow(f1);
	if(y2!=y) {
		addExtrema(y);
		addExtrema(y2);
	}
}

void TempRoute::appendSide(LeftRight side, double x, double top, double bot) {
	appendPoint(side,x,top);
	appendPoint(side,x,bot);
}

void TempRoute::appendVNode(DDModel::Node *virt) {
	int r = DDd(virt).rank;
	double top_y = config.ranking.GetRank(config.ranking.Up(r))->yBelow(1.0),
		bot_y = config.ranking.GetRank(r)->yBelow(0.0),
		left_x = bound(virt, LEFT),
		right_x = bound(virt, RIGHT);
	assert(left_x < right_x);
	appendSide(LEFT,left_x,top_y, bot_y);
	appendSide(RIGHT,right_x,top_y, bot_y);
}
static DDModel::Edge *neighboringEdge(DDModel::Node *n, DDModel::Edge *e, LeftRight side) {
#ifdef NEIGHBORING_EDGE
	DDModel::edge_sequence &seq = n==e->head?n->ins():n->outs;
	DDModel::Edge *rv = 0;
	int dir = side==LEFT?-1:1,
		delta0 = 0;
	for(DDModel::edge_iter ei = seq.begin(); ei!=seq.end(); ++ei)
		if(*ei != e && !DDd(*ei).path->unclippedPath.Empty()) {
			int delta = DDd((*ei)->other(n)).order - DDd(e->other(n)).order;
			if(delta * dir > 0) {
				delta = absol(delta);
				if(!rv || delta < delta0) {
					rv = *ei;
					delta0 = delta;
				}
			}
		}
	return rv;
#else
	return 0;
#endif
}
void TempRoute::termRoute(DDModel::Node *n, DDModel::Edge *e, Coord port, double fract) {
	int rb = (n == e->tail) ? DDd(n).rank : config.ranking.Up(DDd(n).rank);
	double y_cept = config.ranking.GetRank(rb)->yBelow(fract);
	double leftx_port, leftx_cept, rightx_port, rightx_cept;
	DDModel::Edge *ne;
	if((ne = neighboringEdge(n,e,LEFT)) && !DDd(ne).path->unclippedPath.Empty()) {
		leftx_port = checkPos(DDd(ne).path->unclippedPath.YIntersection(port.y)).x;
		leftx_cept = checkPos(DDd(ne).path->unclippedPath.YIntersection(y_cept)).x;
	}
	else 
		leftx_port = leftx_cept = bound(n,LEFT);

	if((ne = neighboringEdge(n,e,RIGHT)) && !DDd(ne).path->unclippedPath.Empty()) {
		rightx_port = checkPos(DDd(ne).path->unclippedPath.YIntersection(port.y)).x;
		rightx_cept = checkPos(DDd(ne).path->unclippedPath.YIntersection(y_cept)).x;
	}
	else 
		rightx_port = rightx_cept = bound(n,RIGHT);

	if(leftx_port > rightx_port)
		swap(leftx_port,rightx_port);

	if(leftx_cept > rightx_cept)
		swap(leftx_cept,rightx_cept);

#ifndef DOWN_GREATER
	if(port.y > y_cept) {
#else
	if(port.y < y_cept) {
#endif
		appendPoint(LEFT, leftx_port, port.y);
		appendPoint(LEFT, leftx_cept, y_cept);
		appendPoint(RIGHT, rightx_port, port.y);
		appendPoint(RIGHT, rightx_cept, y_cept);
	}
	else {
		appendPoint(LEFT, leftx_cept, y_cept);
		appendPoint(LEFT, leftx_port, port.y);
		appendPoint(RIGHT, rightx_cept, y_cept);
		appendPoint(RIGHT, rightx_port, port.y);
	}
}
void TempRoute::defineBoundaryPoly(Line &out) {
	out.Clear();
	out.degree = 1;
	for(Line::iterator pi = left.begin(); pi!=left.end(); ++pi)
		if(out.empty() || out.back()!=*pi)
			out.push_back(*pi);
	for(Line::reverse_iterator pri = right.rbegin(); pri!=right.rend(); ++pri)
		if(out.empty() || out.back()!=*pri)
			out.push_back(*pri);
}

void Spliner::forwardEdgeRegion(DDModel::Node *tl, DDModel::Node *hd,DDPath *inp, Coord tp, Coord hp, Line &out) {
	double prev_fract = .3;
	TempRoute route(config,gd<GraphGeom>(inp->layoutE->g).bounds);
	assert(tl==inp->first->tail);
	route.termRoute(tl,inp->first,tp,prev_fract);
	int lastR = DDd(tl).rank;
	for(DDPath::node_iter ni = inp->nBegin(); ni!=inp->nEnd(); ++ni) {
		assert(lastR<DDd(*ni).rank);
		route.appendBox(config.ranking.Up(DDd(*ni).rank),prev_fract,1.0);
		route.appendVNode(*ni);
		prev_fract = 0.0;
	}

	route.appendBox(config.ranking.Up(DDd(hd).rank),prev_fract,.7);
	assert(hd==inp->last->head);
	route.termRoute(hd,inp->last,hp,.7);

	route.defineBoundaryPoly(out);
}

void Spliner::flatEdgeRegion(DDModel::Node *tl, DDModel::Node *hd, Coord tp, Coord hp, Line &out) {
	out.Clear();
	out.degree = 1;
	DDModel::Node *left, *right;
	Coord lp,rp;
	if(DDd(tl).order < DDd(hd).order) {
		left = tl;  
		right = hd; 
		lp = tp; 
		rp = hp;
	}
	else {
		left = hd; 
		right = tl; 
		lp = hp; 
		rp = tp;
	}
	int n_obstacles = DDd(right).order - DDd(left).order - 1;
	Rank *r = config.ranking.GetRank(DDd(tl).rank);

	/* walk along bottom */
	if(n_obstacles == 0) {
		double y = r->yBelow(0);
		out.push_back(Coord(lp.x,y));
		out.push_back(Coord(rp.x,y));
	}
	else {
		out.push_back(lp);
		DDModel::Node *obstacle = left;
		for(int i = 0; i < n_obstacles; i++) {
			obstacle = config.Right(obstacle);
			double t = config.TopExtent(obstacle);
#ifndef DOWN_GREATER
			double y = r->yBase + t,
#else
			double y = r->yBase - t,
#endif
				xleft = DDd(obstacle).cur.x - config.LeftExtent(obstacle),
				xright = DDd(obstacle).cur.x + config.RightExtent(obstacle);
			out.push_back(Coord(xleft,y));
			out.push_back(Coord(xright,y));
		}
		out.push_back(rp);
	}

	/* add the top */
	double y = r->yAbove(0.5);
	out.push_back(Coord(rp.x,y));
	out.push_back(Coord(lp.x,y));
}

void Spliner::adjustPath(DDPath *path) {
	Line &curve = path->unclippedPath;
	for(DDPath::node_iter ni = path->nBegin(); ni!=path->nEnd(); ++ni) {
		DDModel::Node *left = config.Left(*ni),
			*right = config.Right(*ni);
		Bounds &b = gd<GraphGeom>(path->layoutE->g).bounds;
		double y = DDd(*ni).cur.y,
			x = checkPos(curve.YIntersection(y)).x,
			//halfwidth = config.NodeSize(*ni).x / 2.0,
			lb = findBound(config,b,*ni,LEFT) + config.LeftExtent(*ni),
			rb = findBound(config,b,*ni,RIGHT) - config.RightExtent(*ni);
		// (used to be set in a separate pass but this makes sense)
		DDd(*ni).actualX = x;
		DDd(*ni).actualXValid = true;
		if(x < lb) 
			x = lb;
		if(x > rb) 
			x = rb;

		/* don't move if it's too small */
		double least_move = config.nodeSep.x;
		if(x < rb && x > lb && absol(x - DDd(*ni).cur.x) >= least_move)
			DDd(*ni).cur.x = x;
		/* it's possible that local_crossing() swaps *ni order */
		while((left = config.Left(*ni))) {
				/* for testing - should never happen right now! */
			if(DDd(left).cur.x > x) {
				abort();
				config.Exchange(left,*ni);
			}
			else break;
		}
		while((right = config.Right(*ni))) {
			if(DDd(right).cur.x < x) {
				abort();
				config.Exchange(right,*ni);
			}
			else break;
		}
	}
}

bool Spliner::MakeEdgeSpline(DDPath *path,SpliningLevel splineLevel) {
	assert(path->unclippedPath.Empty());

	DDModel::Node *tl = DDp(path->layoutE->tail)->bottom(),
		*hd = DDp(path->layoutE->head)->top();

	bool reversed = DDd(tl).rank > DDd(hd).rank;
	if(reversed) {
		tl = DDp(path->layoutE->head)->bottom();
		hd = DDp(path->layoutE->tail)->top();
	}
	EdgeGeom &eg = gd<EdgeGeom>(path->layoutE);
	Coord tailpt = eg.tailPort.pos + DDd(tl).multi->pos(),
		headpt = eg.headPort.pos + DDd(hd).multi->pos();

	Line &unclipped = path->unclippedPath;
	if(path->layoutE->tail==path->layoutE->head) {	/* self arc */
		Coord sep = config.nodeSep;
		unclipped.degree = 3;
		Coord farpt(DDd(tl).multi->pos().x + config.RightExtent(tl) + 2.0 * sep.x,
			(tailpt.y + headpt.y) / 2.0);
		double rb = findBound(config,gd<GraphGeom>(path->layoutE->g).bounds,tl, RIGHT);
		if(!config.Right(tl)) 
			rb += 2.0 * sep.x; /* ok to grow */
		if(farpt.x > rb) 
			farpt.x = rb;
#ifndef DOWN_GREATER
		Coord dy(0.0,-sep.y/2.0);
#else
		Coord dy(0.0,sep.y/2.0);
#endif
		unclipped.push_back(tailpt);
		unclipped.push_back(tailpt+dy);
		unclipped.push_back(farpt+dy);
		unclipped.push_back(farpt);
		unclipped.push_back(farpt-dy);
		unclipped.push_back(headpt-dy);
		unclipped.push_back(headpt);
	}
	else {
		Line region;
		bool adjustVNodes = false;
		if(DDd(tl).rank == DDd(hd).rank) 
			flatEdgeRegion(tl,hd,tailpt,headpt,region);
		else {
			forwardEdgeRegion(tl,hd,path,tailpt,headpt,region);
			adjustVNodes = true;
		}
		switch(splineLevel) {
		case DG_SPLINELEVEL_BOUNDS:
		  eg.pos.ClipEndpoints(region,Coord(),0,Coord(),0); // silly way to copy line
			return true;
		case DG_SPLINELEVEL_SHORTEST:
		case DG_SPLINELEVEL_SPLINE: {
			try {
				Line polylineRoute;
				PathPlan::Shortest(region,Segment(tailpt,headpt),polylineRoute);
				if(splineLevel==DG_SPLINELEVEL_SPLINE) {
					PathPlan::SegmentV barriers;
					PathPlan::PolyBarriers(PathPlan::LineV(1,region),barriers);
					
					Segment endSlopes(Coord(0.0,0.0),Coord(0.0,0.0));
					check(PathPlan::Route(barriers,polylineRoute,endSlopes,unclipped));
				}
				else
					unclipped = polylineRoute;
				if(adjustVNodes) 
					adjustPath(path);
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

} // namespace DynaDAG
