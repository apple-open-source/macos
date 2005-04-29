/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef strattr_h
#define strattr_h

// string attributes & names are not used directly by the dynagraph layout engines; 
// they are used by the command-line tool to name & tag things the way dot does.
#include "common/StringDict.h"
#include "common/traversal.h"
#include <map>
typedef DString Name;
struct StrAttrs : std::map<DString,DString> {
	DString look(DString attr,DString defval) {
		iterator ai = find(attr);
		if(ai==end()||ai->second.empty())
			return defval;
		else
			return ai->second;
	}
};
typedef std::set<DString> StrAttrChanges;
struct StrAttrs2 : StrAttrs,StrAttrChanges { // sorry, don't know what else to call it
	bool put(DString name,DString val) {
		StrAttrs &attrs = *this;
		StrAttrChanges &cha = *this;
		if(attrs[name]!=val) {
			attrs[name] = val;
			cha.insert(name);
            return true;
		}
        else return false;
	}
};
struct NamedAttrs : StrAttrs,Name,Hit {
	NamedAttrs(DString name = DString()) : Name(name) {}
};
struct DuplicateNodeName : DGException { 
  DString name; 
  DuplicateNodeName(DString name) : 
    DGException("names of StrGraph nodes must be unique"),
    name(name)
  {} 
};
struct NodeNotFound : DGException { 
  DString name; 
  NodeNotFound(DString name) : 
    DGException("StrGraph::readSubgraph encountered a node not in the parent"),
    name(name)
  {} 
};
struct EdgeNotFound : DGException { 
  DString tail,head; 
  EdgeNotFound(DString tail,DString head) : 
    DGException("StrGraph::readSubgraph encountered an edge not in the parent"),
    tail(tail),head(head)
  {} 
};
struct StrGraph : LGraph<NamedAttrs,NamedAttrs,NamedAttrs> {
	// as we all should know by now, deriving a class and adding a dictionary isn't all
	// that good an idea.  this class is no exception; to make this work, gotta call 
	// oopsRefreshDictionary() before you need the dict to work.  what's next?  LGraph events?
	typedef std::map<DString,Node*> Dict;
	Dict dict;
	StrGraph(Graph *parent) : Graph(parent) {}
	StrGraph(StrGraph *parent = 0) : Graph(parent) {}
	StrGraph(const StrGraph &o) : Graph(o) {
		oopsRefreshDictionary();
	}
	Node *create_node(DString name) {
		return create_node(NamedAttrs(name));
	}
	Node *create_node(const NamedAttrs &na) {
		Node *ret = Graph::create_node(na);
		enter(na,ret);
		return ret;
	}
	void oopsRefreshDictionary() {
		dict.clear();
		for(node_iter ni = nodes().begin(); ni!=nodes().end(); ++ni)
			dict[gd<Name>(*ni)] = *ni;
	}
	void readSubgraph(StrGraph *what) {
		std::map<DString,Node*> &pardict = static_cast<StrGraph*>(parent)->dict;
		for(node_iter ni = what->nodes().begin(); ni!=what->nodes().end(); ++ni) {
			Node *n = *ni,*found = pardict[gd<Name>(n)];
			if(!found)
				throw NodeNotFound(gd<Name>(n));
			insert(found);
		}
		for(graphedge_iter ei = what->edges().begin(); ei!=what->edges().end(); ++ei) {
			Edge *e = *ei;
			Node *tail = pardict[gd<Name>(e->tail)],
				*head = pardict[gd<Name>(e->head)];
			Edge *found = parent->find_edge(tail,head);
			if(!found)
				throw EdgeNotFound(gd<Name>(e->tail),gd<Name>(e->head));
			insert(found);
		}
	}
protected:
	void enter(const DString &name,Node *n) {
		if(name.empty())
			return;
		Node *&spot = dict[name];
		if(spot)
			throw DuplicateNodeName(name);
		spot = n;
	}
};

#endif // strattr_h
