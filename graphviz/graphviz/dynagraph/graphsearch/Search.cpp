/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "graphsearch/Search.h"

using namespace std;

namespace GSearch {

Search::Search(StrGraph &source) : source(source) {}

void Search::readStrGraph(Patterns &patterns,StrGraph &desc) {
	clear();
	map<StrGraph::Node*,Node*> recollect;
	for(StrGraph::node_iter ni = desc.nodes().begin(); ni!=desc.nodes().end(); ++ni) {
		Node *n = create_node(gd<Name>(*ni));
		recollect[*ni] = n;
		SearchStage &stage = gd<SearchStage>(n);
		DString action = gd<StrAttrs>(*ni).look("action","union");
		if(action=="union")
			stage.type = UnionInquiry;
		else if(action=="intersection")
			stage.type = IntersectionInquiry;
		else if(action=="pattern") {
			stage.type = PatternInquiry;
			DString pattern = gd<StrAttrs>(*ni).look("pattern","");
			Patterns::iterator pi = patterns.find(pattern);
			if(pi==patterns.end())
				throw UndefinedPattern(pattern);
			stage.pattern = &pi->second;
		}
		else if(action=="path")
			stage.type = PathInquiry;
		else 
			throw UnknownAction(action);
	}
	for(StrGraph::graphedge_iter ei = desc.edges().begin(); ei!=desc.edges().end(); ++ei) {
		Edge *e = create_edge(recollect[(*ei)->tail],recollect[(*ei)->head]).first;
		DString input = gd<StrAttrs>(*ei).look("input","");
		gd<Name>(e) = input;
	}
}


void Search::Run(const Inputs &inputs) {
	for(Inputs::const_iterator ngi = inputs.begin(); ngi!=inputs.end(); ++ngi) {
		node_iter ni;
		for(ni = nodes().begin(); ni!=nodes().end(); ++ni)
			if(gd<Name>(*ni)==ngi->first) {
				StrGraph &dest = gd<SearchStage>(*ni).result;
				dest = *ngi->second;
				break;
			}
        /*
		if(ni==nodes().end())
			throw StageNotFound(ngi->first);
        */
	}
	while(1) {
		Node *stage = 0;
		// find a stage for which all prereq stages are done
		for(node_iter ni = nodes().begin(); ni!=nodes().end(); ++ni) {
			if(gd<SearchStage>(*ni).done)
				continue;
			inedge_iter ei;
			for(ei = (*ni)->ins().begin(); ei!=(*ni)->ins().end(); ++ei)
				if(!gd<SearchStage>((*ei)->tail).done)
					break;
			if(ei==(*ni)->ins().end()) {
				stage = *ni;
				break;
			}
		}
		if(!stage)
			break;
		SearchStage &earchage = gd<SearchStage>(stage);
		Search::inedge_iter ei = stage->ins().begin();
		if(ei!=stage->ins().end())
			switch(earchage.type) {
			case UnionInquiry:
				earchage.result = gd<SearchStage>((*ei)->tail).result;
				for(; ei!=stage->ins().end(); ++ei)
					earchage.result |= gd<SearchStage>((*ei)->tail).result;
				break;
			case IntersectionInquiry:
				earchage.result = gd<SearchStage>((*ei)->tail).result;
				for(; ei!=stage->ins().end(); ++ei)
					earchage.result &= gd<SearchStage>((*ei)->tail).result;
				break;
			case PatternInquiry: {
				if(!earchage.pattern)
					throw PatternNotThere();
				queue<Match> Q;
				PathsFollowed followed;
				for(; ei!=stage->ins().end(); ++ei) {
					const Name &name = gd<Name>(*ei);
					Pattern::node_iter ni;
					for(ni = earchage.pattern->nodes().begin();
							ni!=earchage.pattern->nodes().end(); ++ni)
						if(gd<Name>(*ni)==name) {
							StrGraph &input = gd<SearchStage>((*ei)->tail).result;
							for(StrGraph::node_iter inpi = input.nodes().begin(); inpi !=input.nodes().end(); ++inpi) {
								earchage.result.insert(*inpi);
								Q.push(Match(*ni,source.find(*inpi)));
							}
							break;
						}
					if(ni==earchage.pattern->nodes().end())
						throw PatternStateNotFound(name);
				}
				runPattern(Q,followed,&earchage.result);
				break;
			}
			case PathInquiry:;
			}
		earchage.done = true;
	}
}

}
