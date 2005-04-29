/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"
#include "common/emitGraph.h"
using namespace std;

const unsigned int AllFlags = 0xffffffff;

void emitStrChanges(ostream &os,StrAttrChanges &cha,StrAttrs &att) {
	os << "[";
	for(StrAttrChanges::iterator ci = cha.begin(); ci!=cha.end(); ++ci) {
	  if(ci!=cha.begin())
	    os << ",";
	  os << mquote(*ci) << "=" << mquote(att[*ci]);
	}
	os << "]\n";
	cha.clear();
}
void emitChanges(ostream &os,ChangeQueue &Q,const char *view) {
	os << "lock view " << view << endl;
	if(!gd<StrAttrChanges>(Q.client).empty()) {
	  os << "modify view " << view << " ";
	  emitStrChanges(os,gd<StrAttrChanges>(Q.client),gd<StrAttrs>(Q.client));
	}
	Layout::node_iter ni;
	Layout::graphedge_iter ei;
	for(ni = Q.delN.nodes().begin(); ni!=Q.delN.nodes().end(); ++ni) {
	  os << "delete " << view << " node " << mquote(gd<Name>(*ni).c_str()) << endl;
	  gd<StrAttrChanges>(*ni).clear();
	}
	for(ei = Q.delE.edges().begin(); ei!=Q.delE.edges().end(); ++ei) {
	  os << "delete " << view << " edge " << mquote(gd<Name>(*ei).c_str()) << endl;
	  gd<StrAttrChanges>(*ei).clear();
	}
	for(ni = Q.insN.nodes().begin(); ni!=Q.insN.nodes().end(); ++ni) {
	  os << "insert " << view << " node " << mquote(gd<Name>(*ni).c_str()) << " ";
	  emitStrChanges(os,gd<StrAttrChanges>(*ni),gd<StrAttrs>(*ni));
	}
	for(ei = Q.insE.edges().begin(); ei!=Q.insE.edges().end(); ++ei) {
	  os << "insert " << view << " edge " << mquote(gd<Name>(*ei))
	     << " " << mquote(gd<Name>((*ei)->tail))
	     << " " << mquote(gd<Name>((*ei)->head)) << " ";
	  emitStrChanges(os,gd<StrAttrChanges>(*ei),gd<StrAttrs>(*ei));
	}
	// all things that still have StrAttrChanges are either layout-modified or had an irrelevant string changed 
	for(ni = Q.client->nodes().begin(); ni!=Q.client->nodes().end(); ++ni)
	  if(!gd<StrAttrChanges>(*ni).empty()) {
	    os << "modify " << view << " node " << mquote(gd<Name>(*ni).c_str()) << " ";
	    emitStrChanges(os,gd<StrAttrChanges>(*ni),gd<StrAttrs>(*ni));
	  }
	for(ei = Q.client->edges().begin(); ei!=Q.client->edges().end(); ++ei)
	  if(!gd<StrAttrChanges>(*ei).empty()) {
	    os << "modify " << view << " edge " << mquote(gd<Name>(*ei)) << " ";
	    emitStrChanges(os,gd<StrAttrChanges>(*ei),gd<StrAttrs>(*ei));
	  }
	os << "unlock view " << view << endl;
}
