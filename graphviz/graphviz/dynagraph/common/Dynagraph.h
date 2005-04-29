/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef Dynagraph_h
#define Dynagraph_h

#include "common/LGraph-cdt.h"
#include "common/traversal.h"
#include "common/StrAttr.h"
#include "common/Geometry.h"
#include "common/genpoly.h"

/*
        UPDATE flags. use with ChangeQueue::ModNode,ModEdge to set this 
	modify subgraph specific flag.  
	(this update flag is a poor use for instance-specific data, because the data 
	is unused in the other subgraphs)
*/
typedef enum {
	DG_UPD_MOVE = 1<<0, // NodeGeom:: or EdgeGeom:: pos attribute
	DG_UPD_REGION = 1<<1, // NodeGeom::region
	DG_UPD_NAIL = 1<<2, // NodeGeom::nail
	DG_UPD_TAIL = 1<<3, // EdgeGeom::tailPort or ::tailClipped 
	DG_UPD_HEAD = 1<<4, // (sorry - can't change end nodes! create a new edge and delete this one)
	DG_UPD_WIDTH = 1<<5, // EdgeGeom::
	DG_UPD_LENGTH = 1<<6, // EdgeGeom::
	DG_UPD_COST = 1<<7, // EdgeGeom::
	DG_UPD_CONSTRAINT = 1<<8, // EdgeGeom::
	DG_UPD_LABEL = 1<<9, // all Geom:: any label changed (hmm this update flag model is breaking down.  should i reserve 5 bits for 32 labels har har)
	DG_UPD_BOUNDS = 1<<10, // NodeGeom:: and GraphGeom::
	DG_UPD_NESTING = 1<<11, // dream on
	DG_UPD_EDGESTYLE = 1<<12, // GraphGeom::splineLevel (useful for debugging.  real edge styles also should be on the way...)
	DG_UPD_RESOLUTION = 1<<13, // GraphGeom::
	DG_UPD_SEPARATION = 1<<14, // GraphGeom::
	DG_UPD_DRAWN = 1<<15, // nodes,graphs,edges: the Drawn lines have changed
	DG_UPD_POLYDEF = 1<<16 // anything in node's PolyDef
} UpdateFlags;
struct Update { // subgraph-specific datum
	unsigned flags;
	Update(unsigned flags = 0) : flags(flags) {}
};

/*
	MODEL pointer - annoying type unsafe pocket in each layout object for 
	a layout server to put a back-pointer; there's no provision for additional servers

	Really, the layout servers should be templated on the layout graph type, and
	the GraphAttrs, NodeAttrs, EdgeAttrs shouldn't be defined here but in client code.

	But I haven't seen a great need yet, and don't know what compiler-specific
	tricks you'd have to pull to still have separate compilation.  
*/
struct ModelPointer {
	void *model;
	ModelPointer() : model(0) {}
};

// the lines to render an object. applies to all nodes, edges, and the Layout itself (right now just for debugging)
// position-relative for nodes, absolute for others
struct Drawn : Lines {}; 

/*
	GRAPH attributes
*/
typedef enum {
	DG_SPLINELEVEL_VNODE,
	DG_SPLINELEVEL_BOUNDS,
	DG_SPLINELEVEL_SHORTEST,
	DG_SPLINELEVEL_SPLINE
} SpliningLevel;
struct GraphGeom {
	Bounds bounds;
	SpliningLevel splineLevel;
	Coord labelGap; // between node and label
	Coord resolution; // smallest units in each dimension (e.g. 1,1 for integer)
	Coord separation; // space between horizontally adjacent nodes and edges; minimum edge height
	Coord defaultSize; // a hint for the engines about node size; clients such as incrface use this
	float ticks; // time limit, in seconds, 0 - no limit (NYI)
	GraphGeom() : splineLevel(DG_SPLINELEVEL_SPLINE),labelGap(0,0),resolution(0.1,0.1),separation(0.5,0.5),defaultSize(1,1),ticks(0) {}
};
struct StaticLabel {
	Rect bounds;
};
struct StaticLabels {
	std::vector<StaticLabel> labels;
};
// at present dynagraph has no orientation parameter; that is, you can't 
// specify which way is down and thus which way dynadag edges will point.
// instead, translate your coords to and from dynagraph using e.g. reorient.h
// and set this parameter so that things that don't want to be translated,
// like the shape generator and label placer, know how to compensate.
typedef enum {
	DG_ORIENT_RIGHT,
	DG_ORIENT_UP,
	DG_ORIENT_LEFT,
	DG_ORIENT_DOWN
} Orientation;
struct Translation {
	Orientation orientation;
	Translation() : orientation(DG_ORIENT_DOWN) {}
};

struct GraphAttrs : Name,StrAttrs2,GraphGeom,Drawn,Translation,StaticLabels,ModelPointer,Hit {};

// generated shapes for nodes (relative to NodeGeom::pos); 
// this (unlike NodeGeom::region), is not translated according to Translation::orientation
// in other words, translate dynagraph coords first, then add these.
// (nyi) line ([0]) + arrows for edges (absolute canvas coords)

/*
	NODE attributes
*/
typedef enum {
	DG_NONAIL = 0,
	DG_NAIL_X = 1,
	DG_NAIL_Y = 2,
	DG_NAIL_BOTH = 3
} NailType;
struct NodeGeom {
	Position pos;
	Region region; 
	NailType nail;
	NodeGeom() : nail(DG_NONAIL) {}
	Bounds BoundingBox() {
		if(!pos.valid)
			return Bounds();
		if(region.boundary.valid)
			return Bounds(region.boundary+pos);
		else 
			return Bounds(pos);
	}
	bool Overlaps(const NodeGeom &o) {
		return region.Overlaps(pos,o.pos,o.region);
	}
};
typedef enum {
	DG_NODELABEL_CENTER,
	DG_NODELABEL_RIGHT,
	DG_NODELABEL_TOP,
	DG_NODELABEL_LEFT,
	DG_NODELABEL_BOTTOM
} NodeLabelPlacement;

struct NodeLabel {
	// input
	NodeLabelPlacement where;
	Coord size;
	// output
	Position pos; // upper-left corner

	NodeLabel(NodeLabelPlacement where = DG_NODELABEL_CENTER, Coord size = Coord(0,0)) :
		where(where), size(size) {}
};
struct Port {
	Coord pos;
	double angle;
	Port(Coord pos=Coord(),double angle=0) : pos(pos),angle(angle) {}
};
typedef std::vector<std::pair<int,Port> > IdentifiedPorts;
typedef std::vector<NodeLabel> NodeLabels;
struct IfPolyDef : PolyDef {
	bool whether; // whether to use, else client has other way to fill NodeGeom::region
	IfPolyDef() : whether(true) {}
};
struct NodeAttrs : Name,StrAttrs2,NodeGeom,NodeLabels,Hit,IfPolyDef,Drawn,ModelPointer {};

/*
	EDGE attributes
*/
struct EdgeGeom {
	double width,lengthHint,cost;
	Line pos;
	bool constraint, // whether should affect ranking
		manualRoute; // try to use 
	Port tailPort,headPort;
	bool tailClipped,headClipped;
	EdgeGeom() : width(1.0),lengthHint(1.0),cost(0.0),constraint(true),
		manualRoute(false),tailPort(Coord(0.0,0.0)),headPort(Coord(0.0,0.0)),
		tailClipped(true),headClipped(true) {}
};
struct EdgeLabel {
	// input
	double where, // 0 at tail, 1 at head
		length; // length of straight line to fit here
	Line *shape; // if !=0, translate this shape each time into gd<Drawn>
	// if shape==0, output, otherwise these are points in the shape which should be matched to what we find.
	Position pos1,pos2;
	EdgeLabel(double where = 0.5f,double length = 0.0f) : where(where),length(length),shape(0) {}
};
typedef std::vector<EdgeLabel> EdgeLabels;
struct EdgeAttrs : Name,StrAttrs2,EdgeGeom,EdgeLabels,ModelPointer,Hit,Drawn {

};

/*
	LAYOUT (spec & geom graph)
*/
typedef LGraph<GraphAttrs,NodeAttrs,EdgeAttrs,Update,Update,Update> Layout;

/*
	CHANGE QUEUE
*/
struct ChangeQueue {
	// the client edits this supergraph of server's current layout
	// then calls the methods below to signal the changes in the subgraphs
	Layout * const client, * const current; 
	Layout insN,modN,delN,
		insE,modE,delE;

	ChangeQueue(Layout *client,Layout *current);
	ChangeQueue(ChangeQueue &copy); // compiler never gets these right
	void InsNode(Layout::Node *n);
	void InsEdge(Layout::Edge *e);
	void ModNode(Layout::Node *n,Update u); // ORs u into the igd<Update> of modN subnode
	void ModEdge(Layout::Edge *e,Update u);
	void DelNode(Layout::Node *n);
	void DelEdge(Layout::Edge *e);
	unsigned &GraphUpdateFlags() { return igd<Update>(&modN).flags; }

	// called by server to update current subgraph based on current changes
	void UpdateCurrent();
	void CalcBounds();

	// called by client after server processing clear subgraphs and maybe do deletions
	void Okay(bool doDelete = false);

	bool Empty() { return insN.nodes().empty()&&modN.nodes().empty()&&delN.nodes().empty()&&
		insE.nodes().empty()&&modE.nodes().empty()&&delE.nodes().empty()&&GraphUpdateFlags()==0; }

	// copy
	ChangeQueue &operator=(ChangeQueue &Q);
	// accumulate
	ChangeQueue &operator+=(ChangeQueue &Q);

	// Exceptions

	// insertions must not already be inserted; modifications & deletions must already be inserted
	struct InsertInserted : DGException {
	  InsertInserted() : DGException("insertion of an already inserted object") {}
	};
	struct ModifyUninserted : DGException {
	  ModifyUninserted() : DGException("modify of an uninserted object") {}
	};
	struct DeleteUninserted : DGException {
	  DeleteUninserted() : DGException("deletion of an uninserted object") {}
	};
	struct EndnodesNotInserted : DGException {
	  EndnodesNotInserted() : DGException("insertion of edge without nodes") {}
	};
};
/*
	SERVER (e.g. topology layout, edge layout, label layout)
*/
struct Server {
	// shared by all actors in a system;  represents everything currently inserted
	// note this is a subgraph of the client layout
	Layout *const client, *const current; 

	virtual void Process(ChangeQueue &Q) = 0;
	Server(Layout *client,Layout *current) : client(client), current(current) {}
	virtual ~Server() {}
};
struct CompoundServer : Server {
	typedef std::vector<Server*> ServerV;
	ServerV actors;
	void Process(ChangeQueue &Q);
	CompoundServer(Layout *client,Layout *currentLayout) : Server(client,currentLayout) {}
	~CompoundServer();
};
// simple server that just updates the current subgraph based on changes.
// this must only be done once, that's why individual layout servers can't be responsible.
struct UpdateCurrent : Server {
	UpdateCurrent(Layout *client,Layout *currentLayout) : Server(client,currentLayout) {}
	void Process(ChangeQueue &Q) {
		Q.UpdateCurrent();
	}
};
struct LabelPlacer : Server {
	void Process(ChangeQueue &Q);
	LabelPlacer(Layout *client,Layout *currentLayout) : Server(client,currentLayout) {}
	~LabelPlacer() {}
};
struct ShapeGenerator : Server {
	void Process(ChangeQueue &Q);
	ShapeGenerator(Layout *client,Layout *currentLayout) : Server(client,currentLayout) {}
	~ShapeGenerator() {}
};
/* 
	EXCEPTIONS
*/
struct NailWithoutPos : DGException {
	Layout::Node *n;
	NailWithoutPos(Layout::Node *n) : 
	  DGException("nailing a node without specifying a position"),
	  n(n) {}
};
struct BackForth : DGException { 
	Layout::Edge *e; 
	BackForth(Layout::Edge *e) : DGException("dynadag can't handle a->b->a  (a->b->c->a is okay)"),
	  e(e) {} 
}; 

#endif
