/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "graphsearch/Pattern.h"

namespace GSearch {

enum InquiryType {
	UnionInquiry,
	IntersectionInquiry,
	PatternInquiry,
	PathInquiry
};
struct SearchStage {
	InquiryType type;
	Pattern *pattern; // only if PatternInquiry
	StrGraph result;
	bool done;
	SearchStage(StrGraph *parent) : pattern(0),result(parent),done(false)	 {}
	SearchStage(const SearchStage &o) : type(o.type),pattern(o.pattern),result(o.result),done(false) {}
};
// edge names match pattern's start state name(s) or "a" or "b" for PathInquiry
// node names match runSearch input names 
struct NamedStage : Name,SearchStage {
	NamedStage(const NamedStage &o) : Name(o),SearchStage(o) {}
	NamedStage(StrGraph *parent,DString name) : Name(name),SearchStage(parent) {}
};
typedef std::map<DString,StrGraph*> Inputs;
struct Search : LGraph<Nothing,NamedStage,Name> {
	StrGraph &source;
	std::map<DString,Node*> dict;
	Search(const Search &copy) : Graph(copy),source(copy.source) { // don't let them copy dict!
		for(node_iter ni = nodes().begin(); ni!=nodes().end(); ++ni)
			dict[gd<Name>(*ni)] = *ni;
	}
	Search(StrGraph &source);
	void readStrGraph(Patterns &patterns,StrGraph &desc);
	void Run(const Inputs &inputs);

	Node *create_node(DString name) {
		Node *n = Graph::create_node(NamedStage(&source,name));
		dict[name] = n;
		return n;
	}
	void reset() {
		for(node_iter ni = nodes().begin(); ni!=nodes().end(); ++ni) {
			Node *n = *ni;
			SearchStage &stage = gd<SearchStage>(n);
			stage.done = false;
			stage.result.clear();
		}
	}
};
// exceptions
struct StageNotFound : DGException {
  DString name; 
  StageNotFound(DString name) : DGException("search stage not found (impossible!)"),
       name(name) {} 
};
struct PatternNotThere : DGException {
  PatternNotThere() : DGException("search stage is supposed to execute a pattern, but the pattern wasn't specified") {}
};
struct PatternStateNotFound : DGException {
  DString name; 
  PatternStateNotFound(DString name) : DGException("input state of pattern not found"),
       name(name) {} 
};
struct UnknownAction : DGException {
  DString name; UnknownAction(DString name) : DGException("stage's action type not recognized"),
       name(name) {} 
};
struct UndefinedPattern : DGException {
  DString name; 
  UndefinedPattern(DString name) : DGException("pattern not defined"),
       name(name) {} 
};

}
