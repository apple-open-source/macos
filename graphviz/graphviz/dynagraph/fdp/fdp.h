/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef FDP_H
#define FDP_H

#include "common/Dynagraph.h"

namespace FDP {

#define NDIM 2

struct FDPEdge {
	Layout::Edge *layoutE;

	FDPEdge() : layoutE(0) {}
};

struct FDPNode {
	Layout::Node *layoutN;
    bool fixed; // true if node should not move 
    double pos[NDIM], // new position 
		disp[NDIM]; // incremental displacement 

	FDPNode() : layoutN(0),fixed(false) {
		for(int i = 0; i<NDIM; ++i)
			pos[i] = disp[i] = 0.0;
	}
};

typedef LGraph<Nothing,FDPNode,FDPEdge> FDPModel;

inline FDPModel::Node *&modelP(Layout::Node *n) {
	return reinterpret_cast<FDPModel::Node*&>(gd<ModelPointer>(n).model);
}
inline FDPModel::Edge *&modelP(Layout::Edge *e) {
	return reinterpret_cast<FDPModel::Edge*&>(gd<ModelPointer>(e).model);
}
struct Inconsistency : DGException {
  Inconsistency() : DGException("fdp's internal model has become inconsistent with the client graph") {}
};
struct StillHasEdges : DGException {
  StillHasEdges() : DGException("a node was deleted without all of its edges being deleted (impossible!)") {}
};

#include "grid.h"

struct FDPServer : Server,Grid::Visitor {
	int numIters;
	bool useComp,
		useGrid;
	double Width, // Width of bounding box 
		Height,  // Height of bounding box 
		T0,  // Initial temperature 
		K,K2, // Edge length, squared 
		CellSize, // Cell width and height 
		RepFactor,
		AttFactor,
		Afact2, // Phase 2 AttFactor 
		Rfact2, // Phase 2 RepFactor 
		Radius2; // Radius of interaction squared. Anything outside of the radius has no effect on node 

	FDPServer(Layout *client, Layout *current) : 
	  Server(client,current),
	  numIters(40),
	  useComp(false),
	  useGrid(true),
	  Width(8.5),Height(11.0),
	  T0(-1.0),
	  K(1.0),K2(1.0),
	  CellSize(0.0),
	  RepFactor(1.0),
	  AttFactor(1.0),
	  Afact2(0.001),
	  Rfact2(1.0),
	  Radius2(0.0) {}
	~FDPServer() {}
	// Server
	void Process(ChangeQueue &changeQ);
	// Grid::Visitor
	int VisitCell(Cell *cell,Grid *grid);
private:
	FDPModel model;

	void createModelNode(Layout::Node *n);
	void createModelEdge(Layout::Edge *e);
	void deleteModelNode(Layout::Node *n);
	void deleteModelEdge(Layout::Edge *e);

	Position findMedianSize();
	void setParams(Coord avgsize);
	double cool(int t);
	void doRep(FDPModel::Node *p, FDPModel::Node *q, double xdelta, double ydelta, double dist2);
	void applyRep(FDPModel::Node *p, FDPModel::Node *q);
	void applyAttr(FDPModel::Node *p, FDPModel::Node *q);
	void doNeighbor(Grid *grid, int i, int j, const node_list &nodes);
	int gridRepulse(Dt_t*dt, void*v, void*g);
	void adjust(FDPModel *g, double temp, Grid *grid);
	void positionNodes(FDPModel *g, Grid *grid);
	void layout();
};

} // namespace FDP
#endif // FDP_H
