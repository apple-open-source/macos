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
			return DDd(n1).order<DDd(n2).order;
		return DDd(n1).rank<DDd(n2).rank;
	}
};
struct RankMore {
	bool operator()(DDModel::Node *n1,DDModel::Node *n2) {
		if(DDd(n1).rank == DDd(n2).rank)
			return DDd(n1).order>DDd(n2).order;
		return DDd(n1).rank>DDd(n2).rank;
	}
};
struct MValLess {
	UpDown dir;
	MValLess(UpDown dir) : dir(dir) {}
	bool operator()(DDModel::Node *n1,DDModel::Node *n2) {
		if(!MValExists(n1,dir))
			return false;
		else if(!MValExists(n2,dir))
			return true;
		else return MVal(n1,dir)<MVal(n2,dir);
	}
};
typedef NS::NSNode<void*,void*> MSNSNode;
struct MSNodeData : MSNSNode {
	DDModel::Node *n;
	MSNodeData() : n(0) {}
};
typedef NS::NSEdge<void*,void*> MSNSEdge;
typedef LGraph<NS::NSData<void*,void*>,MSNodeData,NS::NSEdge<void*,void*> > MSGraph;
typedef NS::NS<MSGraph,NS::AccessNoAttr<MSGraph> > MSNS;

typedef vector<MSGraph::Node*> msgnv;
struct MSGNRankLess {
	bool operator()(MSGraph::Node *n1,MSGraph::Node *n2) {
		return gd<MSNSNode>(n1).rank<gd<MSNSNode>(n2).rank;
	}
};
// not a member because msvc++ member templates wouldn't allow
template<typename node_iter,typename HowOrdered>
bool pass(Config &config,node_iter begin,node_iter end,UpDown dir,HowOrdered ho) {
	bool ret = false;
	node_iter ni = begin;
	while(ni!=end) {
		int rank = DDd(*ni).rank;
		node_iter last;
		for(last = ni; last!=end; ++last)
			if(DDd(*last).rank!=rank)
				break;
		MSGraph msg;
		Rank *r = config.ranking.GetRank(rank);
		msgnv order(r->order.size(),0);
		MSGraph::Node *prev = 0;
		// unmoving or no mval: make strong constraint
		NodeV::iterator ni2;
		for(ni2 = r->order.begin(); ni2!=r->order.end(); ++ni2)
			if(!MValExists(*ni2,dir) || !binary_search(ni,last,*ni2,ho)) { 
				MSGraph::Node *curr = msg.create_node();
				order[DDd(*ni2).order] = curr;
				gd<MSNodeData>(curr).n = *ni2;
				if(prev) {
					MSGraph::Edge *e = msg.create_edge(prev,curr).first;
					gd<MSNSEdge>(e).minlen = 1;
					gd<MSNSEdge>(e).weight = 0;
				}
				prev = curr;
			}
		NodeV mvalSort(r->order.begin(),r->order.end());
		stable_sort(mvalSort.begin(),mvalSort.end(),MValLess(dir));
		// create weak constraints to try to keep nodes() in median order
		int n = 0;
		prev = 0;
		for(ni2 = mvalSort.begin(); ni2!=mvalSort.end(); ++ni2) {
			MSGraph::Node *&curr = order[DDd(*ni2).order];
			if(!curr) {
				curr = msg.create_node();
				gd<MSNodeData>(curr).n = *ni2;
			}
			if(prev) {
				MSGraph::Node *weak = msg.create_node();
				// make it cheap to put curr to right of prev, expensive to left
				MSGraph::Edge *cheap = msg.create_edge(weak,curr).first,
					*pricy = msg.create_edge(weak,prev).first;
				gd<MSNSEdge>(cheap).minlen = 1;
				gd<MSNSEdge>(cheap).weight = 0;
				gd<MSNSEdge>(pricy).minlen = 0;
				gd<MSNSEdge>(pricy).weight = 1;
			}
			prev = curr;
		}
		report(r_crossopt,"MSGraph has %d nodes()\n",msg.nodes().size());
		MSNS().Solve(&msg,NS::RECHECK|NS::VALIDATE|NS::NORMALIZE);
		stable_sort(order.begin(),order.end(),MSGNRankLess());
		// to make the minimal disruption, scan old and new orders simultaneously
		// to fix a mismatch, see if what should be there is somewhere else
		// if so, it's later, so cut until it shows up
		// if it's not, we already cut it, so insert it.
		n = 0;
		for(msgnv::iterator msni2 = order.begin(); msni2!=order.end(); ++msni2) {
			DDModel::Node *mn = gd<MSNodeData>(*msni2).n;
			if(r->order[n]!=mn) {
				ret = true;
				if(DDd(mn).inConfig) {
					do config.RemoveNode(r->order[n]);
					while(r->order[n]!=mn);
				}
				else
					config.InstallAtOrder(mn,rank,n);
			}
			n++;
		}
		/*
		if(msni2!=order.end()) {
			for(msgnv::iterator msni2 = order.begin(); msni2!=order.end(); ++msni2) {
				DDModel::Node *mn = gd<MSNodeData>(*msni2).n;
				config.RemoveNode(mn);
			}
			for(msni2 = order.begin(); msni2!=order.end(); ++msni2) {
				DDModel::Node *mn = gd<MSNodeData>(*msni2).n;
				config.InstallAtRight(mn,rank);
			}
			ret = true;
		}
		*/
		ni = last;
	}
	return ret;
}
void MedianShuffle::Reorder(Layout &nodes,Layout &edges) {
	NodeV optimOrder;
	getCrossoptModelNodes(nodes,edges,optimOrder);
	sort(optimOrder.begin(),optimOrder.end(),RankLess());
	bool moving = true;
	for(int i = 0; i<MINCROSS_PASSES/2 && moving; ++i) {
		moving = false;
		moving |= pass(config,optimOrder.begin(),optimOrder.end(),UP,RankLess());
		moving |= pass(config,optimOrder.rbegin(),optimOrder.rend(),DOWN,RankMore());
	}
}
/* return new coordinate if node were installed in given rank */
double MedianShuffle::Reopt(DDModel::Node *n,UpDown dir) {
	return DDd(n).cur.x;
}

} // namespace DynaDAG
