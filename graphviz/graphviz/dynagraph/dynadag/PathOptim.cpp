/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"

namespace DynaDAG {

void PathOptim::optPath(DDPath *path) {
	for(int pass = 0; pass < MINCROSS_PASSES; pass++) 
		if(pass % 2 == 0) 
			for(DDModel::Edge *e = path->first; e != path->last; e = *e->head->outs().begin())
				optElt(e->head,UP,(pass%4)<2);
		else 
			for(DDModel::Edge *e = path->last; e != path->first; e = *e->tail->ins().begin())
				optElt(e->tail,DOWN,(pass%4)<2);
}
#define BOTH_MVAL_GOING
bool PathOptim::leftgoing(DDModel::Node *n, UpDown dir, int eq_pass) {
	DDModel::Node *left = config.Left(n);
	if(left) {
		int	uvx = crossweight(uvcross(left,n,dir==UP,dir==DOWN)),
			vux = crossweight(uvcross(n,left,dir==UP,dir==DOWN)),
			diff = uvx - vux;
		if(diff > 0 || diff == 0 && eq_pass)
			return true;
	}
#ifdef BOTH_MVAL_GOING
	if(!MValExists(n,UP)||!MValExists(n,DOWN)) 
		return false;
	while(left) {
		if(MValExists(left,UP)&&MValExists(left,DOWN)) 
			return MVal(n,UP)+MVal(n,DOWN) < MVal(left,UP)+MVal(left,DOWN);
		left = config.Left(left);
	}
#else
	if(!MValExists(n,dir)) 
		return false;
	while(left) {
		if(MValExists(left,dir)) 
			return MVal(n,dir) < MVal(left,dir);
		left = Left(left);
	}
#endif
	return false;
}

void PathOptim::shiftLeft(DDModel::Node *n) {
	config.Exchange(config.Left(n),n);
}

bool PathOptim::rightgoing(DDModel::Node *n, UpDown dir, int eq_pass) {
	DDModel::Node *right = config.Right(n);
	if(right) {
		int	uvx = crossweight(uvcross(n,right,dir==UP,dir==DOWN)),
			vux = crossweight(uvcross(right,n,dir==UP,dir==DOWN)),
			diff = uvx - vux;
		if(diff > 0 || diff == 0 && eq_pass)
			return true;
	}
#ifdef BOTH_MVAL_GOING
	if(!MValExists(n,UP)||!MValExists(n,DOWN)) 
		return false;
	while(right) {
		if(MValExists(right,UP)&&MValExists(right,DOWN)) 
			return MVal(n,UP)+MVal(n,DOWN) > MVal(right,UP)+MVal(right,DOWN);
		right = config.Right(right);
	}
#else
	if(!MValExists(n,dir)) 
		return false;
	while(right) {
		if(MValExists(right,dir)) 
			return MVal(n,dir) > MVal(right,dir);
		right = Right(right);
	}
#endif
	return false;
}

void PathOptim::shiftRight(DDModel::Node *n) {
	config.Exchange(n,config.Right(n));
}
void PathOptim::resetCoord(DDModel::Node *n) {
	DDModel::Node *L = config.Left(n),
		*R = config.Right(n);
	if(L||R)
		DDd(n).cur.x = config.CoordBetween(L,R);
}
void PathOptim::optElt(DDModel::Node *n, UpDown dir, int eq_pass) {
	if(leftgoing(n,dir,eq_pass)) { 
		do shiftLeft(n); 
		while(leftgoing(n,dir,eq_pass)); 
	}
	else if(rightgoing(n,dir,eq_pass)) { 
		do shiftRight(n); 
		while(rightgoing(n,dir,eq_pass)); 
	}
	resetCoord(n);
}
// do optimization on a certain subgraph
void PathOptim::Reorder(Layout &nodes,Layout &edges) {
	for(Layout::graphedge_iter ei = edges.edges().begin(); ei!=edges.edges().end(); ++ei)
		optPath(DDp(*ei));
}
/* return new coordinate if node were installed in given rank */
double PathOptim::Reopt(DDModel::Node *n,UpDown dir) {
	double x = DDd(n).cur.x;

	/* go left or right */
	bool go_left = leftgoing(n,dir,false),
		go_right = rightgoing(n,dir,false);
	DDModel::Node *ln,*rn;
	if(go_left && !go_right) {
		rn = config.Right(n);
		for(ln = config.Left(n); ln; ln = config.Left(ln)) {
			if(MValExists(ln,dir)) {
				if(MVal(ln,dir) <= MVal(n,dir)) 
					break;
			}
			else rn = ln;
		}
	}
	else if(go_right && !go_left) {
		ln = config.Left(n);
		for(rn = config.Right(n); rn; rn = config.Right(rn)) {
			if(MValExists(rn,dir))  {
				if(MVal(rn,dir) >= MVal(n,dir)) 
					break;
			}
			else ln = rn;
		}
	}
	else {	/* it's frozen in place */
		ln = config.Left(n);
		rn = config.Right(n);
	}

	if(ln && (x <= DDd(ln).cur.x) || rn && (x >= DDd(rn).cur.x))
		x = config.CoordBetween(ln,rn);
	return x;
}

} // namespace DynaDAG
