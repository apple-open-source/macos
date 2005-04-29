/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"

using namespace std;

namespace DynaDAG {

Crossings calculateCrossings(Config &config) {
	Crossings cc;
	for(Config::Ranks::iterator ri = config.ranking.begin(); ri!=config.ranking.end(); ++ri) {
		Rank *r = *ri;
		for(NodeV::iterator ni1 = r->order.begin(); ni1!=r->order.end(); ++ni1)
			for(NodeV::iterator ni2 = ni1+1; ni2!=r->order.end(); ++ni2) 
				cc += uvcross(*ni1,*ni2,false,true);
	}
	return cc;
}
pair<int,Coord> calculateTotalEdgeLength(Config &config) {
	int count=0;
	Coord d(0,0);
	for(Layout::graphedge_iter ei = config.current->edges().begin(); ei!=config.current->edges().end(); ++ei) {
		++count;
		for(DDPath::edge_iter mei = DDp(*ei)->eBegin(); mei!=DDp(*ei)->eEnd(); ++mei) {
			DDModel::Edge *e = *mei;
			Coord d2 = DDd(e->tail).cur-DDd(e->head).cur;
			d += d2.Abs();
		}
	}
	return make_pair(count,d);
}

}
