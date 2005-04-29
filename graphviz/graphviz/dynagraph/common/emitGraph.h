/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"
#include "common/parsestr.h"

void emitAttrs(std::ostream &os,const StrAttrs &attrs,const DString &id=DString());

template<typename G>
void emitGraph(std::ostream &os,G *g) {
  os << "digraph " << mquote(gd<Name>(g));
  os << " {" << endl << "\tgraph ";
  emitAttrs(os,gd<StrAttrs>(g));
  for(typename G::node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
    os << '\t' << mquote(gd<Name>(*ni)) << ' ';
    emitAttrs(os,gd<StrAttrs>(*ni));
  }
  for(typename G::graphedge_iter ei = g->edges().begin(); ei!=g->edges().end(); ++ei) {
    os << '\t' << mquote(gd<Name>((*ei)->tail)) << " -> " << mquote(gd<Name>((*ei)->head));
    os << ' ';
    emitAttrs(os,gd<StrAttrs>(*ei),gd<Name>(*ei));
  }
  os << "}\n";
}

// try to substitute labels for names to make dotfile more pleasant
template<typename G>
void emitGraph2(std::ostream &os,G *g) {
	typedef std::map<DString,typename G::Node*> node_dict;
	typedef std::map<typename G::Node*,DString> node_reverse_dict;
	typedef std::map<DString,typename G::Edge*> edge_dict;
	node_dict ndict;
	node_reverse_dict nameOf;
	StrAttrs::iterator ati = gd<StrAttrs>(g).find("label");
	Name &gname = (ati!=gd<StrAttrs>(g).end())?ati->second:gd<Name>(g);

	os << "digraph " << mquote(gname);
	os << " {" << endl << "\tgraph ";
	emitAttrs(os,gd<StrAttrs>(g));
	for(typename G::node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
		StrAttrs::iterator ati = gd<StrAttrs>(*ni).find("label");
		Name nname;
		if(ati!=gd<StrAttrs>(*ni).end()&&!ndict[ati->second]) {
			ndict[ati->second] = *ni;
			nameOf[*ni] = ati->second;
			nname = ati->second;
		}
		else 
			ndict[nameOf[*ni] = nname = gd<Name>(*ni)] = *ni;
		os << '\t' << mquote(nname) << ' ';
		emitAttrs(os,gd<StrAttrs>(*ni));
	}
	edge_dict edict;
	for(typename G::graphedge_iter ei = g->edges().begin(); ei!=g->edges().end(); ++ei) {
		os << '\t' << mquote(nameOf[(*ei)->tail]) << " -> " << mquote(nameOf[(*ei)->head]);
		os << ' ';
		StrAttrs::iterator ati = gd<StrAttrs>(*ei).find("label");
		Name ename;
		if(ati!=gd<StrAttrs>(*ei).end()&&!ndict[ati->second]) {
			edict[ati->second] = *ei;
			ename = ati->second;
		}
		else
			edict[ename = gd<Name>(*ei)] = *ei;
		emitAttrs(os,gd<StrAttrs>(*ei),ename);
	}
	os << "}\n";
}
