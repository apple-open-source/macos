/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/LGraph-cdt.h"
#include "common/StrAttr.h"

using namespace std;

// all StrGraphs should be subgraphs of the big graph
// except out, which must be toplevel
struct Collapsing {
	StrGraph *subg;
	NamedAttrs nodeInfo;
};
typedef vector<Collapsing> Collapsings;
void collapse(StrGraph *start,const Collapsings colls,StrGraph *out) {
	vector<StrGraph::Node*> collapsed;
	Collapsings::const_iterator ci;
	for(ci = colls.begin(); ci!=colls.end(); ++ci)
		collapsed[ci-colls.begin()] = out->create_node(ci->nodeInfo);
	map<StrGraph::Node*,StrGraph::Node*> replace;
	for(StrGraph::node_iter ni = start->nodes().begin(); ni!=start->nodes().end(); ++ni) {
		for(ci = colls.begin(); ci!=colls.end(); ++ci)
			if(ci->subg->find(*ni)) { // found: replace with collapse-node
				replace[*ni] = collapsed[ci-colls.begin()];
				break;
			}
		if(ci==colls.end()) // not found: copy into destination
			replace[*ni] = out->create_node(gd<NamedAttrs>(*ni));
	}
	for(StrGraph::graphedge_iter ei = start->edges().begin(); ei!=start->edges().end(); ++ei) {
		for(ci = colls.begin(); ci!=colls.end(); ++ci)
			if(ci->subg->find(*ei)) 
				break;
		if(ci!=colls.end()) // skip all collapsed edges
			continue;
		out->create_edge(replace[(*ei)->tail],replace[(*ei)->head],gd<NamedAttrs>(*ei));
	}
}
