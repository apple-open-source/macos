/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "graphsearch/Pattern.h"
#include <stdio.h>
#include <sstream>
#include "common/parsestr.h"

using namespace std;

namespace GSearch {

istream &operator>>(istream &is,Test &test) {
	is >> ws >> test.attr >> ws >> must("=") >> ws >> test.value;
	return is;
}
istream &operator>>(istream &is,Path &pt) {
	pt.tests.clear();
	is >> ws;
	while(!is.eof()) {
		pt.tests.push_back(Test());
		is >> pt.tests.back();
		is >> ws;
		if(!is.eof())
			is >> must(",");
		is >> ws;
	}		
	return is;
}
struct DirNames : map<DString,int> {
	DirNames() {
		map<DString,int> &mhp = *this;
		mhp["down"] = 1;
		mhp["up"] = 2;
		mhp["both"] = 3;
	}
} g_dirNames;
void Pattern::readStrGraph(StrGraph &desc) {
	clear();
	map<StrGraph::Node*,Pattern::Node*> recollect;
	for(StrGraph::node_iter ni = desc.nodes().begin(); ni!=desc.nodes().end(); ++ni) {
		Node *n = create_node(gd<Name>(*ni));
		recollect[*ni] = n;
	}
	for(StrGraph::graphedge_iter ei = desc.edges().begin(); ei!=desc.edges().end(); ++ei) {
		Edge *e = create_edge(recollect[(*ei)->tail],recollect[(*ei)->head]).first;
		DString match = gd<StrAttrs>(*ei).look("match","");
		istringstream stream(match);
		stream >> gd<Path>(e);
		DString dir = gd<StrAttrs>(*ei).look("dir","both");
		if(!(gd<Path>(e).direction = g_dirNames[dir]))
			throw BadArgument("dir",dir);
	}
}

void runPattern(queue<Match> &Q,PathsFollowed &followed,StrGraph *dest) {
	while(!Q.empty()) {
		Match match = Q.front();
		Q.pop();
		//printf("consider %x %x\n",match.pattern,match.match);
		for(Pattern::outedge_iter pei = match.pattern->outs().begin(); pei!=match.pattern->outs().end(); ++pei) {
			if(gd<Path>(*pei).direction&matchDown)
				for(StrGraph::outedge_iter sei = match.match->outs().begin(); sei!=match.match->outs().end(); ++sei) {
					//printf("look %x -> %x\n",(*sei)->tail,(*sei)->head);
					if(!followed.insert(FollowedPath(*pei,*sei)).second)
						continue;
					if(gd<Path>(*pei).matches(*sei)) {
					  //StrGraph::Edge *e = 
					  dest->insert(*sei).first;
						Q.push(Match((*pei)->head,(*sei)->head));
						//printf("push %x %x\t",(*pei)->head,(*sei)->head);
					}
				}
			if(gd<Path>(*pei).direction&matchUp)
				for(StrGraph::inedge_iter sei = match.match->ins().begin(); sei!=match.match->ins().end(); ++sei) {
					//printf("look %x -> %x\n",(*sei)->tail,(*sei)->head);
					if(!followed.insert(FollowedPath(*pei,*sei)).second)
						continue;
					if(gd<Path>(*pei).matches(*sei)) {
					  //StrGraph::Edge *e = 
					  dest->insert(*sei).first;
						Q.push(Match((*pei)->head,(*sei)->tail));
						//printf("push %x %x\t",(*pei)->head,(*sei)->tail);
					}
				}
		}
	}
}
void matchPattern(Pattern::Node *start,StrGraph::Node *source,StrGraph *dest) {
  //StrGraph::Node *place = 
  dest->insert(source).first;
	queue<Match> Q;
	PathsFollowed followed;
	Q.push(Match(start,source));
	runPattern(Q,followed,dest);
}

}
