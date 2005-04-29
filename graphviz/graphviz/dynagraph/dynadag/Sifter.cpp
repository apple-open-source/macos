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

int calcIN(SiftMatrix &matrix,DDModel::Node *u,DDModel::Node *v) {
	return matrix.getCrossings(u,v,false);
}
int calcOUT(SiftMatrix &matrix,DDModel::Node *u,DDModel::Node *v) {
	return matrix.getCrossings(u,v,true);
}
int calcALL(SiftMatrix &matrix,DDModel::Node *u,DDModel::Node *v) {
	return matrix.allCrossings(u,v);
}
bool Sifter::pass(SiftMatrix &matrix,NodeV &optimOrder,enum way way) {
	int (*calc)(SiftMatrix &matrix,DDModel::Node *u,DDModel::Node *v);
	switch(way) {
	case lookIn:
		calc = calcIN;
		break;
	case lookOut:
		calc = calcOUT;
		break;
	case lookAll:
		calc = calcALL;
	}
	bool ret = false;
	for(NodeV::iterator ni = optimOrder.begin(); ni!=optimOrder.end(); ++ni) {
		DDModel::Node *v = *ni;
		int r = DDd(v).rank;
		Rank *rank = config.ranking.GetRank(r);
		int numcross=0,numAllCross=0,min=0,what=-1;
		int o;
		// it's a good move if it improves the kind of edges we are looking at (numcross)
		// and doesn't mess up the edges we're not (numAllCross)
		// look to right
		numAllCross = numcross = min = 0;
		for(o = DDd(v).order+1; o!=rank->order.size(); ++o) {
			DDModel::Node *x = rank->order[o];
			numcross += calc(matrix,x,v)-calc(matrix,v,x);
			numAllCross += calcALL(matrix,x,v)-calc(matrix,v,x);
			if(numcross<min && numAllCross<=0) {
				min = numcross;
				what = o+1;
			}
		}
		// look to left
		numAllCross = numcross = 0;
		for(o = DDd(v).order-1; o>=0; --o) {
			DDModel::Node *x = rank->order[o];
			numcross += calc(matrix,v,x)-calc(matrix,x,v);
			numAllCross += calcALL(matrix,v,x)-calc(matrix,x,v);
			if(numcross<min && numAllCross<=0) {
				min = numcross;
				what = o;
			}
		}
		if(what!=-1) {
			matrix.move(v,what==rank->order.size()?0:rank->order[what]);
			config.RemoveNode(v);
			config.InstallAtOrder(v,r,what>DDd(v).order?what-1:what);
			//matrix.check();
			ret = true;
		}
	}
	return ret;
}
struct DegMore {
	bool operator()(DDModel::Node *u,DDModel::Node *v) {
		return u->degree()>v->degree();
	}
};
struct RankLess {
	bool operator()(DDModel::Node *u,DDModel::Node *v) {
		if(DDd(u).rank == DDd(v).rank)
			return DDd(u).order < DDd(v).order;
		return DDd(u).rank < DDd(v).rank;
	}
};
const int MAX_TOPDOWN = 10;
void Sifter::Reorder(Layout &nodes,Layout &edges) {
	int numedges = 0;
	for(Layout::graphedge_iter ei = edges.edges().begin(); ei!=edges.edges().end(); ++ei)
		numedges++;
	report(r_crossopt,"Sifter: %d nodes, %d edges\n",nodes.nodes().size(),numedges);
	NodeV optimOrder;
	getCrossoptModelNodes(nodes,edges,optimOrder);
	SiftMatrix matrix(config);
	// some kinks can only be solved by top-down
	sort(optimOrder.begin(),optimOrder.end(),RankLess());
	bool go;
	int count = MAX_TOPDOWN;
	do {
		go = false;
		go |= pass(matrix,optimOrder,lookIn);
		// or bottom-up
		reverse(optimOrder.begin(),optimOrder.end());
		go |= pass(matrix,optimOrder,lookOut);
		if(go)
			reverse(optimOrder.begin(),optimOrder.end());
	}
	while(go && --count);
	if(!count)
		report(r_crossopt,"warning: sifting topdown was waffling\n");
	// global sifting for rest
	sort(optimOrder.begin(),optimOrder.end(),DegMore());
	go = true;
	while(go) {
		if(!pass(matrix,optimOrder,lookAll)) {
			reverse(optimOrder.begin(),optimOrder.end());
			go = false;
		}
		if(!pass(matrix,optimOrder,lookAll))
			go = false;
		if(go)
			reverse(optimOrder.begin(),optimOrder.end());
	}
}
/* return new coordinate if node were installed in given rank */
double Sifter::Reopt(DDModel::Node *n,UpDown dir) {
	return DDd(n).cur.x;
}

} // namespace DynaDAG
