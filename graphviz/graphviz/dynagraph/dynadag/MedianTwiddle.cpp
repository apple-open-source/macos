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

struct RankLess {
	bool operator()(DDModel::Node *n1,DDModel::Node *n2) {
		if(DDd(n1).rank == DDd(n2).rank)
			return DDd(n1).order < DDd(n2).order;
		return DDd(n1).rank < DDd(n2).rank;
	}
};
void MedianTwiddle::Reorder(Layout &nodes,Layout &edges) {
	NodeV optimOrder;
	getCrossoptModelNodes(nodes,edges,optimOrder);
	sort(optimOrder.begin(),optimOrder.end(),RankLess());
	bool moving = true;
	for(int i = 0; i<MINCROSS_PASSES/2 && moving; ++i) {
		moving = false;
		for(vector<DDModel::Node*>::iterator ni = optimOrder.begin(); ni!=optimOrder.end(); ++ni) 
			moving |= repos(*ni,UP);
		for(vector<DDModel::Node*>::reverse_iterator rni = optimOrder.rbegin(); rni!=optimOrder.rend(); ++rni) 
			moving |= repos(*rni,DOWN);
	}
}
bool MedianTwiddle::repos(DDModel::Node *n,UpDown dir) {
	bool acted = false;
	if(MValExists(n,dir)) {
		if(rightgoing(n,dir)) {
			acted = true;
			do {
				report(r_crossopt,"%s: because mval %.3f > %.3f,\n",dir==UP?"UP":"DOWN",MVal(n,dir),MVal(config.Right(n),dir));
				config.Exchange(n,config.Right(n));
			}
			while(rightgoing(n,dir));
		}
		else if(leftgoing(n,dir)) {
			acted = true;
			do {
				report(r_crossopt,"%s: because mval %.3f < %.3f,\n",dir==UP?"UP":"DOWN",MVal(n,dir),MVal(config.Left(n),dir));
				config.Exchange(config.Left(n),n);
			}
			while(leftgoing(n,dir));
		}
	}
	return acted;
}
bool MedianTwiddle::leftgoing(DDModel::Node *n,UpDown dir) {
	if(!MValExists(n,dir)) 
		return false;
	DDModel::Node *left = config.Left(n);
	while(left) {
		if(MValExists(left,dir)) 
			return MVal(n,dir) < MVal(left,dir);
		left = config.Left(left);
	}
	return false;
}
bool MedianTwiddle::rightgoing(DDModel::Node *n,UpDown dir) {
	if(!MValExists(n,dir)) 
		return false;
	DDModel::Node *right = config.Right(n);
	while(right) {
		if(MValExists(right,dir)) 
			return MVal(n,dir) > MVal(right,dir);
		right = config.Right(right);
	}
	return false;
}
/* return new coordinate if node were installed in given rank */
double MedianTwiddle::Reopt(DDModel::Node *n,UpDown dir) {
	return DDd(n).cur.x;
}

} // namespace DynaDAG
