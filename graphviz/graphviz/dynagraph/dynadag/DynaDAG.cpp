/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"
#include "dynadag/Measurements.h"

using namespace std;

namespace DynaDAG {

DynaDAGServer::~DynaDAGServer() {
	for(Layout::graphedge_iter i = current->edges().begin();i!=current->edges().end();++i)
		closeLayoutEdge(*i);
	for(Layout::node_iter j(current->nodes().begin()); j!=current->nodes().end(); ++j)
		closeLayoutNode(*j);
}
// DynaDAGServices
pair<DDMultiNode*,DDModel::Node*> DynaDAGServer::OpenModelNode(Layout::Node *layoutN) {
	DDMultiNode *m = 0;
	DDModel::Node *mn = model.create_node();
	if(layoutN) { // part of multinode: attach multi to model & view nodes
		assert(layoutN->g==client); // don't store the wrong subnode
		m = DDp(layoutN); // see if view node's already been modeled
		if(!m) {
			(m = new DDMultiNode)->layoutN = layoutN;
			m->node = mn;
			DDp(layoutN) = m;
		}
		DDd(mn).multi = m;
	}
	return make_pair(m,mn);
}
void DynaDAGServer::CloseModelNode(DDModel::Node *n) {
	/* must already be isolated in graph */
	assert(n->ins().size()==0 && n->outs().size()==0);
	xsolver.RemoveNodeConstraints(n);
	config.RemoveNode(n);
	model.erase_node(n);
}
pair<DDPath*,DDModel::Edge*> DynaDAGServer::OpenModelEdge(DDModel::Node *u, DDModel::Node *v, Layout::Edge *layoutE) {
	DDPath *p = 0;
	DDModel::Edge *me = 0;
	if(u&&v)
		me = model.create_edge(u,v).first;
	if(layoutE) { // part of path: attach path to model & view edges
		assert(layoutE->g==client); // don't store the wrong subedge
		p = DDp(layoutE); // see if view edge's already been modeled
		if(!p) {
			(p = new DDPath)->layoutE = layoutE;
			DDp(layoutE) = p;
		}
		if(me)
			DDd(me).path = p;
	}
	return make_pair(p,me);
}
void DynaDAGServer::CloseModelEdge(DDModel::Edge *e) {
	xsolver.RemoveEdgeConstraints(e);
	model.erase_edge(e);
}
void DynaDAGServer::CloseChain(DDChain *chain,bool killEndNodes) {
	if(!chain->first) 
		return;
	DDModel::Edge *ei = chain->first;
	DDModel::Node *ni;
	while(1) {
		ni = ei->head;
		DDModel::Node *killn = 0;
		if(killEndNodes || ei!=chain->first)
			killn = ei->tail;
		CloseModelEdge(ei);
		if(killn)
			CloseModelNode(killn);
		if(ei==chain->last)
			break;
		assert(ni->outs().size()==1);
		ei = *ni->outs().begin();
	}
	if(killEndNodes)
		CloseModelNode(ni);
	chain->first = chain->last = 0;
}
Optimizer *DynaDAGServer::GetOptimizer() {
	return optimizer;
}
// private methods
void DynaDAGServer::closeLayoutNode(Layout::Node *n) {
	DDMultiNode *m = DDp(n);
	ranker.RemoveLayoutNodeConstraints(m);
	//xsolver.RemoveLayoutNodeConstraints(m);
	CloseChain(m,true);
	if(m->node) 
		CloseModelNode(m->node);
	delete m;
	DDp(n) = 0;
}
void DynaDAGServer::closeLayoutEdge(Layout::Edge *e) {
	DDPath *p = DDp(e);
	report(r_bug,"e %p p %p\n",e,p);
	assert(p);
	ranker.RemovePathConstraints(p);
	if(p->first) {
		InvalidateMVal(p->first->tail,DOWN);
		InvalidateMVal(p->last->head,UP);
	}
	CloseChain(p,false);
	delete p;
	DDp(e) = 0;
}
void DynaDAGServer::executeDeletions(ChangeQueue &changeQ) {
	for(Layout::graphedge_iter j = changeQ.delE.edges().begin(); j!=changeQ.delE.edges().end();) {
		Layout::Edge *e = *j++;
		closeLayoutEdge(e);
	}
	for(Layout::node_iter i = changeQ.delN.nodes().begin(); i!=changeQ.delN.nodes().end(); ++i) 
		closeLayoutNode(*i);
}
// pretty aggressive
void DynaDAGServer::findOrdererSubgraph(ChangeQueue &changeQ,Layout &outN,Layout &outE) {
	// do crossing optimization on all inserted nodes & edges...
	outN = changeQ.insN;
	outE = changeQ.insE;
	// all moved...
	Layout::node_iter ni;
	for(ni = changeQ.modN.nodes().begin(); ni!=changeQ.modN.nodes().end(); ++ni)
		if(igd<Update>(*ni).flags&DG_UPD_MOVE)
			outN.insert(*ni);
	for(Layout::graphedge_iter ei=changeQ.modE.edges().begin(); ei!=changeQ.modE.edges().end(); ++ei)
		if((igd<Update>(*ei).flags&DG_UPD_MOVE) && !userDefinedMove(*ei))
			outE.insert(*ei);
	// and all adjacent (this will add the edges off of a node that has a new or changed edge, but not the other ends of those edges)
	outN |= outE; // nodes adjacent to edges
	for(ni = outN.nodes().begin(); ni!=outN.nodes().end(); ++ni) // edges adjacent to nodes 
		for(Layout::nodeedge_iter ei = (*ni)->alledges().begin(); ei!=(*ni)->alledges().end(); ++ei)
			outE.insert(*ei);
	if(reportEnabled(r_dynadag)) {
		loops.Field(r_dynadag,"number of layout nodes",current->nodes().size());
		loops.Field(r_dynadag,"layout nodes for crossopt",outN.nodes().size());
		loops.Field(r_dynadag,"layout edges for crossopt",outE.edges().size());
	}
}
void DynaDAGServer::updateBounds(ChangeQueue &changeQ) {
  bool got = false;
  double glb=0.0,grb=0.0;  // init for gcc's sake argh
  for(Config::Ranks::iterator ri = config.ranking.begin(); ri!=config.ranking.end(); ++ri)
    if((*ri)->order.size()) {
      DDModel::Node *left = (*ri)->order.front();
      double lb = DDd(left).cur.x - config.LeftExtent(left);
      if(!got || glb > lb) 
	glb = lb;
      DDModel::Node *right = (*ri)->order.back();
      double rb = DDd(right).cur.x + config.RightExtent(right);
      if(!got || grb < rb) 
	grb = rb;
      got = true;
    }
  Bounds bb;
  if(got) {
    bb.valid = true;
    bb.l = glb;
    bb.t = config.ranking.front()->yAbove(0);
    bb.r = grb;
    bb.b = config.ranking.back()->yBelow(0);
  }
  if(gd<GraphGeom>(current).bounds != bb) {
    gd<GraphGeom>(current).bounds = bb;
    changeQ.GraphUpdateFlags() |= DG_UPD_BOUNDS;
  }
}
void DynaDAGServer::findChangedNodes(ChangeQueue &changeQ) {
	// calculate how much nodes moved
	Coord moved(0,0);
	int nmoves=0;
	for(Layout::node_iter ni = current->nodes().begin(); ni!=current->nodes().end(); ++ni) {
		if(!changeQ.delN.find(*ni)) {
			Position pos = DDp(*ni)->pos();
			assert(pos.valid);
			Position &p = gd<NodeGeom>(*ni).pos;
			if(p != pos) 
			{
				if(p.valid) {
					moved += (p-pos).Abs();
					++nmoves;
				}
				p = pos;
				changeQ.ModNode(*ni,DG_UPD_MOVE);
			}
		}
	}
	loops.Field(r_stability,"layout nodes moved",nmoves);
	loops.Field(r_stability,"node x movement",moved.x);
	loops.Field(r_stability,"node y movement",moved.y);
}
bool DynaDAGServer::edgeNeedsRedraw(DDPath *path,ChangeQueue &changeQ) {
	if(path->unclippedPath.Empty()) // new edge
		return true;
	Layout::Node *tailMod = changeQ.modN.find(path->layoutE->tail),
		*headMod = changeQ.modN.find(path->layoutE->head);
	// check for endpoint resize or move
	if(tailMod && igd<Update>(tailMod).flags & (DG_UPD_REGION|DG_UPD_MOVE) || 
		headMod && igd<Update>(headMod).flags & (DG_UPD_REGION|DG_UPD_MOVE))
		return true;
	if(!path->first) // flat or self edge
		return false;
	double sep = config.nodeSep.x;
	for(DDPath::node_iter ni = path->nBegin(); ni!=path->nEnd(); ++ni) {
		DDModel::Node *n = *ni;
		if(!DDd(n).actualXValid)
			return true;
		double x = DDd(n).actualX;
		if(DDModel::Node *left = config.Left(n)) {
			if(DDd(left).amEdgePart()) {
				if(DDd(left).actualXValid && DDd(left).actualX + sep > x) 
					return true;
			}
			else { 
				if(DDd(left).cur.x + config.RightExtent(left) + sep > x) 
					return true; 
			}
		}
		if(DDModel::Node *right = config.Right(n)) {
			if(DDd(right).amEdgePart()) {
				if(DDd(right).actualXValid && DDd(right).actualX - sep < x) 
					return true;
			}
			else { 
				if(DDd(right).cur.x - config.LeftExtent(right) - sep < x) 
					return true;
			}
		}
	}
	return false;
}
void DynaDAGServer::sketchEdge(DDPath *path) {
	// draw an edge just based on vnodes
	EdgeGeom &eg = gd<EdgeGeom>(path->layoutE);
	path->unclippedPath.degree = 1;
	Layout::Node *head = path->layoutE->head,
		*tail=path->layoutE->tail;
	// if a backedge (head is lower rank than tail), path->first->tail is head
	// so we have to clip accordingly and then reverse the result (for arrowheads etc.)
	bool reversed = DDd(DDp(head)->top()).rank<DDd(DDp(tail)->bottom()).rank;
	if(reversed) 
		swap(head,tail);
	if(!path->first) {
		if(tail!=head) { // flat
			path->unclippedPath.push_back(DDp(tail)->pos()+eg.tailPort.pos);
			path->unclippedPath.push_back(DDp(head)->pos()+eg.headPort.pos);
		}
		else {} // self
	}
	else {
		path->unclippedPath.push_back(DDd(path->first->tail).cur+eg.tailPort.pos);
		for(DDPath::node_iter ni = path->nBegin(); ni!=path->nEnd(); ++ni)
			path->unclippedPath.push_back(DDd(*ni).cur);
		path->unclippedPath.push_back(DDd(path->last->head).cur+eg.headPort.pos);
	}
	bool clipFirst = eg.headClipped,
		clipLast = eg.tailClipped;
	if(reversed) 
		swap(clipFirst,clipLast);
	eg.pos.ClipEndpoints(path->unclippedPath,
		gd<NodeGeom>(tail).pos,clipFirst?&gd<NodeGeom>(tail).region:0,
		gd<NodeGeom>(head).pos,clipLast?&gd<NodeGeom>(head).region:0);
	if(reversed)
		reverse(eg.pos.begin(),eg.pos.end());
}
void DynaDAGServer::redrawEdges(ChangeQueue &changeQ,bool force) {
	Layout::graphedge_iter ei;
	for(ei = current->edges().begin(); ei!=current->edges().end(); ++ei)
		if(force || !changeQ.delE.find(*ei) && (gd<EdgeGeom>(*ei).pos.Empty() || edgeNeedsRedraw(DDp(*ei),changeQ))) {
			DDp(*ei)->unclippedPath.Clear();
			changeQ.ModEdge(*ei,DG_UPD_MOVE);
		}
	for(ei = current->edges().begin(); ei!=current->edges().end(); ++ ei)
		if(!changeQ.delE.find(*ei) && DDp(*ei)->unclippedPath.Empty()) {
			if(gd<GraphGeom>(current).splineLevel==DG_SPLINELEVEL_VNODE ||
					!spliner.MakeEdgeSpline(DDp(*ei),gd<GraphGeom>(current).splineLevel))
				sketchEdge(DDp(*ei));
		}
}
void DynaDAGServer::cleanUp() { // dd_postprocess
	for(DDModel::node_iter ni = model.nodes().begin(); ni!=model.nodes().end(); ++ni) {
		DDNode &ddn = DDd(*ni);
		ddn.prev = ddn.cur;
		// ddn.orderFixed = true;
	}
	for(Layout::node_iter vni = current->nodes().begin(); vni!=current->nodes().end(); ++vni) {
		DDMultiNode *n = DDp(*vni);
		if(!n)
			continue; // deleted
		//n->coordFixed = true;
		n->oldTopRank = DDd(n->top()).rank;
		n->oldBottomRank = DDd(n->bottom()).rank;
	}
}
void DynaDAGServer::dumpModel() {
	if(!reportEnabled(r_modelDump))
		return;
	report(r_modelDump,"digraph dynagraphModel {\n");
	for(DDModel::node_iter ni = model.nodes().begin(); ni!=model.nodes().end(); ++ni) 
		report(r_modelDump,"\t\"%p\" [label=\"%p\\n%p\"];\n",*ni,*ni,DDd(*ni).multi);
	for(DDModel::graphedge_iter ei = model.edges().begin(); ei!=model.edges().end(); ++ei)
		report(r_modelDump,"\t\"%p\"->\"%p\" [label=\"%p\\n%p\"];\n",(*ei)->tail,(*ei)->head,*ei,DDd(*ei).path);
	report(r_modelDump,"}\n");
}
void DynaDAGServer::Process(ChangeQueue &changeQ) {
	loops.Field(r_dynadag,"nodes inserted - input",changeQ.insN.nodes().size());
	loops.Field(r_dynadag,"edges inserted - input",changeQ.insE.edges().size());
	loops.Field(r_dynadag,"nodes modified - input",changeQ.modN.nodes().size());
	loops.Field(r_dynadag,"edges modified - input",changeQ.modE.edges().size());
	loops.Field(r_dynadag,"nodes deleted - input",changeQ.delN.nodes().size());
	loops.Field(r_dynadag,"edges deleted - input",changeQ.delE.nodes().size());
	if(changeQ.Empty())
		return;

	// erase model objects for everything that's getting deleted
	executeDeletions(changeQ);
	timer.LoopPoint(r_timing,"preliminary");

	// find ranks for nodes
	ranker.Rerank(changeQ);
	timer.LoopPoint(r_timing,"re-rank nodes");

	// figure out subgraph for crossing optimization
	Layout crossN(config.current),crossE(config.current);
	findOrdererSubgraph(changeQ,crossN,crossE);
	//optimizer = optChooser.choose(crossN.nodes().size()); // should prob. be no. of nodes in corresponding model subgraph

	// synch model graph with changes
	config.Update(changeQ); 
	loops.Field(r_dynadag,"model nodes",model.nodes().size());
	timer.LoopPoint(r_timing,"update model graph");

	// crossing optimization
	optimizer->Reorder(crossN,crossE); 
	timer.LoopPoint(r_timing,"crossing optimization");

	// find X coords
	xsolver.Place(changeQ); 

	// (with ConseqRanks, find rank Ys from node heights.)  find node Y coords from rank Ys.
	config.SetYs(); 
	timer.LoopPoint(r_timing,"optimize x coordinates");

	// calculate bounding rectangle
	updateBounds(changeQ);
	
	// find node & edge moves
	findChangedNodes(changeQ); 

	// (a microscopic amount of time gets added to splines)

	redrawEdges(changeQ,changeQ.GraphUpdateFlags()&DG_UPD_EDGESTYLE);
	timer.LoopPoint(r_timing,"draw splines");

	// reset flags
	cleanUp();

	dumpModel();

	loops.Field(r_dynadag,"nodes inserted - output",changeQ.insN.nodes().size());
	loops.Field(r_dynadag,"edges inserted - output",changeQ.insE.edges().size());
	loops.Field(r_dynadag,"nodes modified - output",changeQ.modN.nodes().size());
	loops.Field(r_dynadag,"edges modified - output",changeQ.modE.edges().size());
	loops.Field(r_dynadag,"nodes deleted - output",changeQ.delN.nodes().size());
	loops.Field(r_dynadag,"edges deleted - output",changeQ.delE.nodes().size());
	if(reportEnabled(r_readability)) {
		Crossings cc = calculateCrossings(config);
		loops.Field(r_readability,"edge-edge crossings",cc.edgeEdgeCross);
		loops.Field(r_readability,"edge-node crossings",cc.nodeEdgeCross);
		loops.Field(r_readability,"node-node crossings",cc.nodeNodeCross);

		pair<int,Coord> elen = calculateTotalEdgeLength(config);
		Coord avg = elen.second/elen.first;
		loops.Field(r_readability,"average edge x-length",avg.x);
		loops.Field(r_readability,"average edge y-length",avg.y);
	}
}

} // namespace DynaDAG
