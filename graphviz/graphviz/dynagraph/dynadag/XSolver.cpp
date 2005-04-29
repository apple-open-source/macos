/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"

using namespace std;

// changing incrementally is buggy.  not so expensive to rewrite all constraints.
#define REDO_ALL

namespace DynaDAG {

// XConstraintOwner (Config & DynaDAG call-ins)
void XSolver::RemoveNodeConstraints(DDModel::Node *n) {
	cg.RemoveNodeConstraints(DDd(n).getXcon());
	/*
	if(DDMultiNode *multi = DDd(n).multi) 
		if(multi->xcon.n) // kill all connected edge constraints (but only once)
			for(DDMultiNode::node_iter ni = multi->nBegin(); ni!=multi->nEnd(); ++ni)
				for(DDModel::nodeedge_iter ei(*ni); ei!=DDModel::nodeedge_iter(); ++ei) 
					RemoveEdgeConstraints(*ei);
	*/
}
void XSolver::RemoveEdgeConstraints(DDModel::Edge *e) {
	DDCGraph::Node *cn = DDd(e).cn;
	if(cn) {
		cg.erase(cn);
		DDd(e).cn = 0;
	}
}
void XSolver::InvalidateChainConstraints(DDChain *path) {
	for(DDPath::edge_iter ei = path->eBegin(); ei!=path->eEnd(); ++ei)
		RemoveEdgeConstraints(*ei);
	for(DDPath::node_iter ni = path->nBegin(); ni!=path->nEnd(); ++ni)
		RemoveNodeConstraints(*ni);
}
void XSolver::DeleteLRConstraint(DDModel::Node *u,DDModel::Node *v) {
	DDCGraph::Node *cn_u = u?DDd(u).getXcon().n:cg.anchor,
		*cn_v = DDd(v).getXcon().n;
	DDCGraph::Edge *ce; 
	if(cn_u && cn_v && (ce = cg.find_edge(cn_u,cn_v)))
		cg.erase(ce);
}
// internals
void XSolver::fixSeparation(DDModel::Node *mn) {
	DDModel::Node *left, *right;	/* mn's left and right neighbors */
	double left_width, right_width,
		sep = config.nodeSep.x,
		left_ext = config.LeftExtent(mn),
		right_ext = config.RightExtent(mn);
	DDCGraph::Node *var = cg.GetVar(DDd(mn).getXcon()),
		*left_var,*right_var;

	if((left = config.Left(mn))) {
		left_var = cg.GetVar(DDd(left).getXcon());
		left_width = config.RightExtent(left);
	}
	else {
		left_var = cg.anchor;
		left_width = -DDNS::NSd(left_var).rank;
	}

	DDCGraph::Edge *ce = cg.create_edge(left_var,var).first;
	int scaled_len = ROUND(xScale * (sep + left_width + left_ext));
	DDNS::NSd(ce).minlen = scaled_len;
	DDNS::NSd(ce).weight = COMPACTION_STRENGTH;
	/*
	if(left && DDNS::NSd(var).rank - DDNS::NSd(left_var).rank < scaled_len)
		cg.inconsistent = true;
	*/

	if((right = config.Right(mn))) {
		right_var = cg.GetVar(DDd(right).getXcon());
		right_width = config.LeftExtent(right);
		ce = cg.create_edge(var,right_var).first;
		scaled_len = ROUND(xScale * (sep + right_width + right_ext));
		DDNS::NSd(ce).minlen = scaled_len;
		DDNS::NSd(ce).weight = COMPACTION_STRENGTH;
		/*
		if(DDNS::NSd(right_var).rank - DDNS::NSd(var).rank < scaled_len)
			cg.inconsistent = true;
		*/
	}
}

void XSolver::doNodesep(Layout *subLayout) {
	for(Layout::node_iter vni = subLayout->nodes().begin(); vni!=subLayout->nodes().end(); ++vni)
		for(DDMultiNode::node_iter mni = DDp(*vni)->nBegin(); mni!=DDp(*vni)->nEnd(); ++mni)
			fixSeparation(*mni);
}

void XSolver::doEdgesep(Layout *subLayout) {
	for(Layout::graphedge_iter ei = subLayout->edges().begin(); ei!=subLayout->edges().end(); ++ei) {
		DDPath *path = DDp(*ei);
		if(path->first) 
			for(DDPath::node_iter ni = path->nBegin(); ni!=path->nEnd(); ++ni)
				fixSeparation(*ni);
		else if((*ei)->head!=(*ei)->tail) { /* flat */
			DDModel::Node *u = DDp((*ei)->tail)->top(), // any subnode is fine
				*v = DDp((*ei)->head)->top();
			int ux = DDd(u).order,
				vx = DDd(v).order;
			if(abs(ux - vx) == 1) {
				if(ux > vx) 
					swap(u,v);
				DDCGraph::Node *uvar = DDd(u).getXcon().n,
					*vvar = DDd(v).getXcon().n;
				DDCGraph::Edge *ce = cg.create_edge(uvar,vvar).first;
				double sep = config.RightExtent(u) + config.LeftExtent(v) + 3.0 * config.nodeSep.x;
				DDNS::NSd(ce).minlen = ROUND(xScale * sep);
			}
		}
		else {} /* self */
	}
}
void XSolver::restoreNodesep(ChangeQueue &changeQ) {
	doNodesep(&changeQ.insN);
	doNodesep(&changeQ.modN);
	// hack: nodes of edges may also be changed
	doNodesep(&changeQ.insE);
	doNodesep(&changeQ.modE);

	doEdgesep(&changeQ.insE);
	doEdgesep(&changeQ.modE);

	for(DDModel::node_iter ni = config.model.dirty.nodes().begin(); ni!=config.model.dirty.nodes().end(); ++ni)
		fixSeparation(*ni);
}
void XSolver::fixEdgeCost(DDModel::Edge *me) {
	if(!DDd(me).cn) {
		DDd(me).cn = cg.create_node();
		gd<ConstraintType>(DDd(me).cn).why = ConstraintType::orderEdgeStraighten;
	}
	NSEdgePair ep(DDd(me).cn,cg.GetVar(DDd(me->tail).getXcon()),cg.GetVar(DDd(me->head).getXcon()));
	DDNS::NSd(ep.e[0]).weight = BEND_WEIGHT;
	DDNS::NSd(ep.e[1]).weight = BEND_WEIGHT;
	DDNS::NSd(ep.e[0]).minlen = 0;
	DDNS::NSd(ep.e[1]).minlen = 0;
}

void XSolver::doEdgeCost(Layout *subLayout) {
	for(Layout::graphedge_iter ei = subLayout->edges().begin(); ei!=subLayout->edges().end(); ++ei) {
		DDPath *path = DDp(*ei);
		for(DDPath::edge_iter ei2 = path->eBegin(); ei2!=path->eEnd(); ++ei2)
			fixEdgeCost(*ei2);
	}
} 

	// restore around changed nodes
void XSolver::fixLostEdges(Layout *subLayout) {
	for(Layout::node_iter ni = subLayout->nodes().begin(); ni!=subLayout->nodes().end(); ++ni) {
	  report(r_xsolver,"fixing multinode %p\n",DDp(*ni));
		DDModel::Node *top = DDp(*ni)->top(),
			*bottom = DDp(*ni)->bottom();
		for(DDModel::inedge_iter ini = top->ins().begin(); ini!=top->ins().end(); ++ini)
			fixEdgeCost(*ini);
		for(DDModel::outedge_iter outi = bottom->outs().begin(); outi!=bottom->outs().end(); ++outi)
			fixEdgeCost(*outi);
	}
}
void XSolver::restoreEdgeCost(ChangeQueue &changeQ) {
	doEdgeCost(&changeQ.insE);
	doEdgeCost(&changeQ.modE);
	fixLostEdges(&changeQ.insN);
	fixLostEdges(&changeQ.modN);
	fixLostEdges(&changeQ.insE);
	fixLostEdges(&changeQ.modE);
}

void XSolver::stabilizeNodes(ChangeQueue &changeQ) {
#ifdef REDO_ALL
	for(Layout::node_iter ni = changeQ.current->nodes().begin(); ni!=changeQ.current->nodes().end(); ++ni) 
        if(gd<NodeGeom>(*ni).pos.valid)  // DDp(*ni)->coordFixed) { assert(gd<NodeGeom>(*ni).pos.valid);
			cg.Stabilize(DDd(DDp(*ni)->top()).getXcon(),ROUND(xScale * gd<NodeGeom>(*ni).pos.x),STABILITY_FACTOR_X);
    /*
	Layout *modedges[2] = {&changeQ.insE,&changeQ.modE};
    // it's not necessary to unstabilize nodes that are connected to edges because the cost
    // of edge bends is much higher than that of stability
	int i;
	for(i = 0; i < 2; i++) 
		for(Layout::node_iter ni = modedges[i]->nodes().begin(); ni!=modedges[i]->nodes().end(); ++ni) 
			if(!DDp(*ni)->coordFixed)
				cg.Unstabilize(DDd(DDp(*ni)->top()).getXcon());
    */
#else
	Layout *modnodes[2] = {&changeQ.insN,&changeQ.modN}, 
		*modedges[2] = {&changeQ.insE,&changeQ.modE};

	/* what about delE? make sure endpoint isn't deleted too. */

	int i;
	for(i = 0; i < 2; i++) 
		for(Layout::node_iter ni = modnodes[i]->nodes().begin(); ni!=modnodes[i]->nodes().end(); ++ni) 
            if(gd<NodeGeom>(*ni).pos.valid) { // DDp(*ni)->coordFixed) { assert(gd<NodeGeom>(*ni).pos.valid);
				double x = gd<NodeGeom>(*ni).pos.x;
				int ix = ROUND(xScale * x);
				cg.Stabilize(DDd(DDp(*ni)->top()).getXcon(),ix,STABILITY_FACTOR_X);
			}
    /*
	for(i = 0; i < 2; i++) 
		for(Layout::node_iter ni = modedges[i]->nodes().begin(); ni!=modedges[i]->nodes().end(); ++ni) 
			if(!DDp(*ni)->coordFixed)
				cg.Unstabilize(DDd(DDp(*ni)->top()).getXcon());
    */
#endif
}
void XSolver::readoutCoords() {
	int anchor_rank = DDNS::NSd(cg.anchor).rank;
	for(DDModel::node_iter ni = config.model.nodes().begin(); ni!=config.model.nodes().end(); ++ni)
		if(DDCGraph::Node *cn = DDd(*ni).getXcon().n) 
			DDd(*ni).cur.x = (DDNS::NSd(cn).rank - anchor_rank) / xScale;
}
// DynaDAG callin
void XSolver::Place(ChangeQueue &changeQ) {
#ifdef REDO_ALL
	DDModel::graphedge_iter ei;
	DDModel::node_iter ni;
	for(ei = config.model.edges().begin(); ei!=config.model.edges().end(); ++ei)
		RemoveEdgeConstraints(*ei);
	for(ni = config.model.nodes().begin(); ni!=config.model.nodes().end(); ++ni)
		RemoveNodeConstraints(*ni);
	for(ni = config.model.nodes().begin(); ni!=config.model.nodes().end(); ++ni)
		fixSeparation(*ni);
	for(ei = config.model.edges().begin(); ei!=config.model.edges().end(); ++ei)
		fixEdgeCost(*ei);
#else
	// obliterate constraints on all changed nodes & edges
	for(Layout::graphedge_iter ei = changeQ.modE.edges().begin(); ei!=changeQ.modE.edges().end(); ++ei)
		if(igd<Update>(*ei).flags&DG_UPD_MOVE)
			InvalidateChainConstraints(DDp(*ei));
	for(Layout::node_iter ni = changeQ.modN.nodes().begin(); ni!=changeQ.modN.nodes().end(); ++ni)
		if(igd<Update>(*ni).flags&DG_UPD_MOVE)
			InvalidateChainConstraints(DDp(*ni));
	restoreNodesep(changeQ);
	restoreEdgeCost(changeQ);
#endif
	//timer.LoopPoint(r_timing,"XConstraints");
	config.model.dirty.clear();
	stabilizeNodes(changeQ);
	// if(cg.inconsistent || 1) 
	{
		checkLRConstraints();
		checkEdgeConstraints();
		try {
			DDNS().Solve(&cg,NS::RECHECK|NS::VALIDATE|NS::NORMALIZE);
		}
		catch(DDNS::CycleException ce) {
			for(Config::Ranks::iterator ri = config.ranking.begin(); ri!=config.ranking.end(); ++ri) {
				report(r_error,"%d> ",config.ranking.IndexOfIter(ri));
				for(NodeV::iterator ni = (*ri)->order.begin(); ni!=(*ri)->order.end(); ++ni) {
					if(DDd(*ni).amNodePart()) {
						bool found = DDd(*ni).multi->xcon.n==ce.involving;
						report(r_error,"n%p%c",thing(*ni),found?'*':' ');
					}
					else
						report(r_error,"e%p ",thing(*ni));
				}
				report(r_error,"\n");
			}
			throw InternalErrorException();
		}
		catch(DDNS::DisconnectedException) {
			report(r_error,"internal error: disconnected constraint graph!\n");
			throw InternalErrorException();
		}
		readoutCoords();
		// cg.inconsistent = false;
	}
}

} // namespace DynaDAG	
