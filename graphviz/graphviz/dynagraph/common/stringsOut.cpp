/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"
#include "common/Transform.h"
#include <sstream>
#include <iomanip>
using namespace std;

const unsigned int AllFlags = 0xffffffff;
const double ln10 = 2.30258509299404568401799145468436;
// these write dynagraph Geom changes to the StrAttrs and StrAttrChanges attributes

inline void initStream(ostringstream &o,Layout *l) {
	double lbase10 = log(gd<GraphGeom>(l).resolution.x)/ln10;
	int precX = int(-floor(lbase10));
	o.flags(o.flags()|ios::fixed);
	o.width(0);
	o.precision(precX);
}
// some attributes belong to all Geoms
template<typename T>
void stringifyAny(Transform *trans,T *o,Update u) {
	// if there's lines to tell OR there useta be and now there aren't...
	if(u.flags&DG_UPD_DRAWN && (gd<Drawn>(o).size() || gd<StrAttrs>(o)["lines"].length())) {
		ostringstream stream;
		stream << gd<Drawn>(o) << ends;
		gd<StrAttrs>(o)["lines"] = stream.str();
		gd<StrAttrChanges>(o).insert("lines");
	}
}
void stringifyChanges(Transform *trans,Layout *l,Update u) {
	stringifyAny(trans,l,u);
	GraphGeom &gg = gd<GraphGeom>(l);
	StrAttrs2 &att = gd<StrAttrs2>(l);
	if(u.flags&DG_UPD_BOUNDS) {
		ostringstream o;
		initStream(o,l);
		Bounds b = gg.bounds;
		if(b.valid && trans) 
			b = trans->out(b);
		if(b.valid)
			o << b.l << ',' << b.b << ',' << b.r << ',' << b.t;
		else 
			o << "0,0,0,0";
		att.put("bb",o.str());
	}
}
void stringifyChanges(Transform *trans,Layout::Node *n,Update u) {
	stringifyAny(trans,n,u);
	NodeGeom &ng = gd<NodeGeom>(n);
	StrAttrs2 &att = gd<StrAttrs2>(n);
	if(u.flags&DG_UPD_MOVE) {
		ostringstream o;
		initStream(o,n->g);
		if(ng.pos.valid) {
			Coord p = ng.pos;
			if(trans)
				p = trans->out(p);
			o << p.x << ',' << p.y;
		}
		att.put("pos",o.str());
	}
	if(u.flags&DG_UPD_POLYDEF) {
		char buf[20]; // agh  can't make myself use <<, am i getting old?
		PolyDef &here=gd<PolyDef>(n),norm;
		if(here.isEllipse) {
			if(!norm.isEllipse)
				att.put("shape","ellipse");
			if(here.aspect!=norm.aspect) {
				sprintf(buf,"%f",here.aspect);
				att.put("aspect",buf);
			}
		}
		else {
			if(gd<StrAttrs>(n).find("shape")==gd<StrAttrs>(n).end())
				att.put("shape","polygon");
			if(here.sides!=norm.sides) {
				sprintf(buf,"%d",here.sides);
				att.put("sides",buf);
			}
		}
		if(here.peripheries!=norm.peripheries) {
			sprintf(buf,"%d",here.peripheries);
			att.put("peripheries",buf);
		}
		if(here.perispacing!=norm.perispacing) {
			sprintf(buf,"%f",here.perispacing);
			att.put("perispacing",buf);
		}
		if(here.rotation!=norm.rotation) {
			sprintf(buf,"%f",(here.rotation*180.0)/M_PI);
			att.put("orientation",buf);
		}
		if(here.skew!=norm.skew) {
			sprintf(buf,"%f",here.skew);
			att.put("skew",buf);
		}
		if(here.distortion!=norm.distortion) {
			sprintf(buf,"%f",here.distortion);
			att.put("distortion",buf);
		}
		if(here.regular)
			att.put("regular",0);
		if(here.exterior_box.x||here.exterior_box.y) {
		  Coord conv = trans->outSize(here.exterior_box);
		  if(conv.x) {
		    sprintf(buf,"%f",conv.x);
		    att.put("width",buf);
		  }
		  if(conv.y) {
		    sprintf(buf,"%f",conv.y);
		    att.put("height",buf);
		  }
		}
		if(here.interior_box.x||here.interior_box.y) {
			sprintf(buf,"%f,%f",here.interior_box.x,here.interior_box.y);
			att.put("textsize",buf);
		}
	}

}
void stringifyChanges(Transform *trans,Layout::Edge *e,Update u) {
	stringifyAny(trans,e,u);
	EdgeGeom &eg = gd<EdgeGeom>(e);
	StrAttrs &att = gd<StrAttrs>(e);
	StrAttrChanges &cha = gd<StrAttrChanges>(e);
	if(u.flags&DG_UPD_MOVE) {
		ostringstream o;
		initStream(o,e->g);
		for(Line::iterator pi = eg.pos.begin(); pi!=eg.pos.end(); ++pi) {
			if(pi!=eg.pos.begin())
				o << ' ';
			Coord p = *pi;
			if(trans)
				p = trans->out(p);
			o << p.x << ',' << p.y;
		}
		att["pos"] = o.str();
		cha.insert("pos");
	}
}
void stringsOut(Transform *trans,ChangeQueue &Q) {
	bool llchanged = false;
	if(trans) {
		trans->ll = Coord(0,0);
		/*
		// grappa used to require bounding box with lower left = (0,0)
		// this means you have to resend all coords pretty much every step
		// but there's probably a legitimate purpose for this disabled feature
		Coord ll = gd<GraphGeom>(Q.client).bounds.LowerLeft();
		if(ll!=trans->ll) {
			trans->ll = ll;
			llchanged = true;
		}
		*/
	}
	if(Q.GraphUpdateFlags()) 
		stringifyChanges(trans,Q.client,Q.GraphUpdateFlags());
	Layout::node_iter ni;
	Layout::graphedge_iter ei;
	if(llchanged) { 
		// all coordinates have changed because they're based on lower-left corner
		for(ni = Q.current->nodes().begin(); ni!=Q.current->nodes().end(); ++ni)
			if(!Q.insN.find(*ni) && !Q.delN.find(*ni))
				stringifyChanges(trans,*ni,DG_UPD_MOVE|igd<Update>(*ni).flags);
		for(ei = Q.current->edges().begin(); ei!=Q.current->edges().end(); ++ei)
			if(!Q.insE.find(*ei) && !Q.delE.find(*ei))
				stringifyChanges(trans,*ei,DG_UPD_MOVE|igd<Update>(*ei).flags);
	}
	else {
		for(ni = Q.insN.nodes().begin(); ni!=Q.insN.nodes().end(); ++ni)
			stringifyChanges(trans,*ni,AllFlags);
		for(ei = Q.insE.edges().begin(); ei!=Q.insE.edges().end(); ++ei)
			stringifyChanges(trans,*ei,AllFlags);
		for(ni = Q.modN.nodes().begin(); ni!=Q.modN.nodes().end(); ++ni)
			stringifyChanges(trans,*ni,igd<Update>(*ni));
		for(ei = Q.modE.edges().begin(); ei!=Q.modE.edges().end(); ++ei)
			stringifyChanges(trans,*ei,igd<Update>(*ei));
	}
}
