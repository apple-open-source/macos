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

ConstraintGraph::ConstraintGraph() : anchor(create_node()) {
	gd<ConstraintType>(anchor).why = ConstraintType::anchor;
}
DDCGraph::Node *ConstraintGraph::GetVar(NodeConstraints &nc) {
	if(!nc.n) 
		gd<ConstraintType>(nc.n = create_node()).why = ConstraintType::node;
	return nc.n;
}

void ConstraintGraph::Stabilize(NodeConstraints &nc, int newrank, int weight) {
	if(!nc.stab) 
		gd<ConstraintType>(nc.stab = create_node()).why = ConstraintType::stab;
//	assert(newrank>-1000000 && newrank<1000000);
	DDCGraph::Node *var = GetVar(nc);
	int len0,len1;
	if (newrank >= 0) {
		len0 = 0; 
		len1 = newrank;
	}
	else {
		len0 = -newrank; 
		len1 = 0;
	}
	NSEdgePair ep(nc.stab,anchor,var);
	DDNS::NSd(ep.e[0]).minlen = len0;
	DDNS::NSd(ep.e[1]).minlen = len1;
	DDNS::NSd(ep.e[0]).weight = weight;
	DDNS::NSd(ep.e[1]).weight = weight;
}
void ConstraintGraph::Unstabilize(NodeConstraints &nc) {
	if(nc.stab) { 
		erase_node(nc.stab);
		nc.stab = 0;
	}
	/*if (nd->con[csys].n)
		agdelete(cg,nd->con[csys].n); */ 	/* incorrect? */
}
void ConstraintGraph::RemoveNodeConstraints(NodeConstraints &nc) {
	if(nc.n) {
		erase_node(nc.n);
		nc.n = 0;
	}
	if(nc.stab) {
		erase_node(nc.stab);
		nc.stab = 0;
	}
}

} // namespace DynaDAG
