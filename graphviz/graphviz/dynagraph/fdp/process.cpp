/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "fdp.h"
#include "common/weightedMedian.h"

using namespace std;

namespace FDP {

Position FDPServer::findMedianSize() {
	int N = current->nodes().size(),i=0;
	if(N==0)
		return Position();
	vector<double> Xs(N),Ys(N);
	for(Layout::node_iter ni = current->nodes().begin(); ni!=current->nodes().end(); ++ni,++i)
		if(gd<NodeGeom>(*ni).region.boundary.valid)
			Xs[i] = gd<NodeGeom>(*ni).region.boundary.Width(),
			Ys[i] = gd<NodeGeom>(*ni).region.boundary.Height();
		else
			Xs[i] = Ys[i] = 0.0;
	return Coord(weightedMedian(Xs),weightedMedian(Ys));
}
void FDPServer::setParams(Coord avgsize) {
	double k = max(avgsize.x,avgsize.y);//avgsize.Len();
	if(k)
		K = k;
	else
		K = 1.0;

	if(T0 <= 0.0) 
		T0 = K*sqrt((double)model.nodes().size())/5;

	K2 = K*K;

	if(useGrid) {
		if(CellSize <= 0.0) 
			CellSize = 3*K;
		Radius2 = CellSize *CellSize;
	}
}

void FDPServer::createModelNode(Layout::Node *n) {
	if(modelP(n))
		throw Inconsistency();
	n = client->find(n);
	FDPModel::Node *fn = model.create_node();
	gd<FDPNode>(fn).layoutN = n;
	modelP(n) = fn;
}
void FDPServer::createModelEdge(Layout::Edge *e) {
	if(modelP(e))
		throw Inconsistency();
	e = client->find(e);
	FDPModel::Node *ft = modelP(e->tail),
		*fh = modelP(e->head);
	if(!ft || !fh)
		throw Inconsistency();
	FDPModel::Edge *fe = model.create_edge(ft,fh).first;
	gd<FDPEdge>(fe).layoutE = e;
	modelP(e) = fe;
}
void FDPServer::deleteModelNode(Layout::Node *n) {
	if(!modelP(n))
		throw Inconsistency();
	FDPModel::Node *fn = modelP(n);
	if(fn->degree())
		throw StillHasEdges();
	model.erase(fn);
	modelP(n) = 0;
}
void FDPServer::deleteModelEdge(Layout::Edge *e) {
	if(!modelP(e))
		throw Inconsistency();
	FDPModel::Edge *fe = modelP(e);
	model.erase(fe);
	modelP(e) = 0;
}
inline void readPos(Layout::Node *n) {
	FDPNode &fdpn = gd<FDPNode>(modelP(n));
	Position pos = gd<NodeGeom>(n).pos;
	if(!pos.valid)
		pos = Coord(0,0);
	fdpn.pos[0] = pos.x;
	fdpn.pos[1] = pos.y;
}
void FDPServer::Process(ChangeQueue &Q) {
	// this is not incremental, really: just respond to events, run, 
	// and then say "everything changed"!
	Layout::node_iter ni;
	Layout::graphedge_iter ei;
	for(ni = Q.insN.nodes().begin(); ni!=Q.insN.nodes().end(); ++ni) {
		createModelNode(*ni);
		readPos(*ni);
	}
	for(ei = Q.insE.edges().begin(); ei!=Q.insE.edges().end(); ++ei)
		createModelEdge(*ei);

	for(ni = Q.modN.nodes().begin(); ni!=Q.modN.nodes().end(); ++ni)
		if(igd<Update>(*ni).flags & DG_UPD_MOVE)
			readPos(*ni);

	for(ei = Q.delE.edges().begin(); ei!=Q.delE.edges().end(); ++ei)
		deleteModelEdge(*ei);
	for(ni = Q.delN.nodes().begin(); ni!=Q.delN.nodes().end(); ++ni)
		deleteModelNode(*ni);

	Position medianSize = findMedianSize();
	if(!medianSize.valid)
		return;
	setParams(medianSize);
	layout();
	//splineEdges();

	// anything that's not ins or del is modified
	for(ni = Q.current->nodes().begin(); ni!=Q.current->nodes().end(); ++ni) {
		FDPModel::Node *fn = modelP(*ni);
		gd<NodeGeom>(*ni).pos = Coord(gd<FDPNode>(fn).pos[0],gd<FDPNode>(fn).pos[1]);
		Q.ModNode(*ni,DG_UPD_MOVE);
	}
	/*
	this algorithm doesn't deal with edges
	for(ei = Q.current->edges().begin(); ei!=Q.current->edges().end(); ++ei)
		Q.ModEdge(*ei,DG_UPD_MOVE);
	*/
}

} // namespace FDP
