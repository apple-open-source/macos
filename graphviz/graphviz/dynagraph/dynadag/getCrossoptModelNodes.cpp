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

void getCrossoptModelNodes(Layout &nodes,Layout &edges,NodeV &out) {
	for(Layout::node_iter ni = nodes.nodes().begin(); ni!=nodes.nodes().end(); ++ni) 
		for(DDMultiNode::node_iter mnni = DDp(*ni)->nBegin(); mnni!=DDp(*ni)->nEnd(); ++ mnni)
			out.push_back(*mnni);
	int ec=0;
	for(Layout::graphedge_iter ei = edges.edges().begin(); ei!=edges.edges().end(); ++ei,++ec)
		for(DDPath::node_iter pni = DDp(*ei)->nBegin(); pni!=DDp(*ei)->nEnd(); ++ pni)
			out.push_back(*pni);
	loops.Field(r_crossopt,"layout nodes for crossopt",nodes.nodes().size());
	loops.Field(r_crossopt,"layout edges for crossopt",ec);

}

}
