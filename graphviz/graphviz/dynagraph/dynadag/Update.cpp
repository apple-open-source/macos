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

namespace DynaDAG {

// translation of dag/order.c

// vn must already be inserted in server's current layout
double xAvgOfNeighbors(Layout::Node *vn) {
	double sum = 0.0;
	int count = 0;
	for(Layout::nodeedge_iter ei=vn->alledges().begin(); ei!=vn->alledges().end(); ++ei) {
		Layout::Node *oth = ei.target();
		NodeGeom &ng = gd<NodeGeom>(oth);
		if(ng.pos.valid) { // rely on layout's geom (e.g. hang off moved node)
			sum += ng.pos.x;
			count++;
		} // or maybe it was already inserted but pos hasn't made it back yet
		else if(DDMultiNode *mn = DDp(oth)) {
			Position pos = mn->pos();
			if(pos.valid) {
				sum += pos.x;
				count++;
			}
		}
	}
	if(count)
		return sum/count;
	else
		return 0.0;
}
struct constX : XGenerator {
	const double x;
	constX(double x) : x(x) {}
	double xval(double y) { return x; }
};
void Config::insertNode(Layout::Node *vn) {
	NodeGeom &ng = gd<NodeGeom>(vn);
	DDMultiNode *n = DDp(vn);
	double x=0.0; // init for gcc
	bool haveX;
	if(ng.pos.valid) {
		x = ng.pos.x,haveX = true;
		//n->coordFixed = true;
	}
	else {
		if(haveX = vn->degree()!=0)
			x = xAvgOfNeighbors(vn);
		//n->coordFixed = false;
	}
	if(n->newTopRank!=n->newBottomRank) {
		DDModel::Node *top = n->node,
			*bottom = dynaDAG->OpenModelNode(vn).second;
		if(haveX) 
			InstallAtPos(top,n->newTopRank,x);
		else
			InstallAtRight(top,n->newTopRank);
		InstallAtPos(bottom,n->newBottomRank,DDd(top).cur.x);
		constX cx(DDd(top).cur.x);
		buildChain(n,top,bottom,&cx,vn,0);
		n->node = 0;
	}
	else
		if(haveX)
			InstallAtPos(n->node,n->newTopRank,x);
		else
			InstallAtRight(n->node,n->newTopRank);
}
void Config::insertNewNodes(ChangeQueue &changeQ) {
	for(Layout::node_iter ni = changeQ.insN.nodes().begin(); ni!=changeQ.insN.nodes().end(); ++ni)
		insertNode(client->find(*ni));
}
Coord interpolate(Coord p0, Coord p1, double t) {
	return p0 + (p1-p0)*t;
}

/* returns model nodes of layout edge, ordered with tail=low rank */
bool getLayoutEndpoints(Layout::Edge *ve, DDModel::Node **p_tl, DDModel::Node **p_hd) {
	bool ret = true;
	DDMultiNode *tail = DDp(ve->tail),
		*head = DDp(ve->head);
	DDModel::Node *t = tail->bottom(),
		*h = head->top();
	if(DDd(h).rank < DDd(t).rank) {
		DDModel::Node *rt = head->bottom(),
			*rh = tail->top();
		if(DDd(rh).rank < DDd(rt).rank) {
			assert(DDd(t).rank==DDd(rt).rank && DDd(h).rank==DDd(rh).rank); // make sure it's flat
			ret = false;
		}
		t = rt;
		h = rh;
	}
	*p_tl = t; *p_hd = h;
	return ret;
}
void Config::buildChain(DDChain *chain, DDModel::Node *t, DDModel::Node *h, XGenerator *xgen,Layout::Node *vn,Layout::Edge *ve) {
	dynaDAG->CloseChain(chain,false);
	if(t==h)
		return;
	int tr = DDd(t).rank,
		hr = DDd(h).rank;
	assert(ranking.Above(tr,hr));
	Ranks::iterator ti = ranking.GetIter(tr),
		hi = ranking.GetIter(hr),
		ri = ti;
	chain->first = chain->last = 0;
	if(++ri!=hi) {
		DDModel::Node *prev = h;
		for((ri = hi)--; ri!=ti; ri--) {
			Ranks::index i = ranking.IndexOfIter(ri);
			DDModel::Node *mn = dynaDAG->OpenModelNode(vn).second;
			InstallAtPos(mn,i,xgen->xval((*ri)->yBase));
			DDModel::Edge *me = dynaDAG->OpenModelEdge(mn,prev,ve).second;
			if(!chain->last)
				chain->last = me;
			prev = mn;
		}
		chain->first = dynaDAG->OpenModelEdge(t,prev,ve).second;
		assert(chain->last);
	}
	else chain->first = chain->last = dynaDAG->OpenModelEdge(t,h,ve).second;
}
struct autoX : XGenerator {
	Coord tc,hc;
	autoX(Coord tc,Coord hc) : tc(tc),hc(hc) {
	  assert(tc.y!=hc.y);
	}
	double xval(double y) {
		return interpolate(tc,hc,(tc.y-y)/(tc.y-hc.y)).x;
	}
};
struct userX : autoX {
	Line &pos;
	userX(Coord tc,Coord hc,Line &pos) : autoX(tc,hc),pos(pos) {}
	double xval(double y) {
		Position yint = pos.YIntersection(y);
		if(yint.valid)
			return yint.x;
		else
			return autoX::xval(y);
	}
};
void Config::userRouteEdge(Layout::Edge *ve) {
	DDModel::Node *t, *h;
	getLayoutEndpoints(ve,&t,&h);
	userX xgen(DDd(t).multi->pos(),DDd(h).multi->pos(),gd<EdgeGeom>(ve).pos);
	buildChain(dynaDAG->OpenModelEdge(0,0,ve).first,t,h,&xgen,0,ve);
}

void Config::autoRouteEdge(Layout::Edge *ve) {
	DDModel::Node *t, *h;
	if(!getLayoutEndpoints(ve,&t,&h))
		dynaDAG->CloseChain(DDp(ve),false); // flat
	else {
		Position tp = DDd(t).multi->pos(),
		  hp = DDd(h).multi->pos();
		assert(tp.valid && hp.valid);
		autoX xgen(tp,hp);
		buildChain(dynaDAG->OpenModelEdge(0,0,ve).first,t,h,&xgen,0,ve);
	}
}
// warning: overindulgent use of member pointers & references ahead...
void Config::adjustChain(DDChain *chain, bool tail,Ranks::index dest,Layout::Node *vn,Layout::Edge *ve) {
	DDModel::Node *endpoint,*v;
	if(tail) {
		endpoint = chain->first->tail;
		v = chain->first->head;
	}
	else {
		endpoint = chain->last->head;
		v = chain->last->tail;
	}
	// how to get end rank from penultimate rank
	Ranks::index (Ranks::*Out)(Ranks::index) = tail?&Ranks::Up:&Ranks::Down;
	Ranks::index start = (ranking.*Out)(DDd(v).rank);
	if(start==dest)
		return;
	//xconOwner->RemoveNodeConstraints(v); // so that no RemoveNodeConstraints happen while chain is broken
	DDModel::Edge *&endEdge = tail?chain->first:chain->last,
		*&beginEdge = tail?chain->last:chain->first;
	dynaDAG->CloseModelEdge(endEdge);
	if(beginEdge == endEdge) 
		beginEdge = endEdge = 0;
	else 
		endEdge = 0;
	bool (Ranks::*Pred)(Ranks::index,Ranks::index) = tail?&Ranks::Below:&Ranks::Above;
	if((ranking.*Pred)(start,dest)) // stretch
		while((ranking.*Pred)((ranking.*Out)(DDd(v).rank),dest)) {
			DDModel::Node *nv = dynaDAG->OpenModelNode(vn).second;
			if(vn) // multinodes have single X
				InstallAtPos(nv,(ranking.*Out)(DDd(v).rank),DDd(v).cur.x);
			else
				percolate(nv,v,(ranking.*Out)(DDd(v).rank));
			DDModel::Edge *e = tail?
				dynaDAG->OpenModelEdge(nv,v,ve).second:
				dynaDAG->OpenModelEdge(v,nv,ve).second;
			if(!beginEdge) 
				beginEdge = e;
			v = nv;
		}
	else // shrink
		while((ranking.*Pred)(dest,(ranking.*Out)(DDd(v).rank))) {
			DDModel::Node *nv = tail?
				(*v->outs().begin())->head:
				(*v->ins().begin())->tail;
			if(DDd(v).multi)
				assert(DDd(v).multi==chain);
			for(DDModel::nodeedge_iter ei=v->alledges().begin(); ei!=v->alledges().end();) {
				DDModel::Edge *del = *ei++;
				if(DDd(del).path)
					assert(DDd(del).path==chain);
				dynaDAG->CloseModelEdge(del);
				if(del==beginEdge)
					beginEdge = 0;
			}
			dynaDAG->CloseModelNode(v);
			v = nv;
		}

	endEdge = tail?
		dynaDAG->OpenModelEdge(endpoint,v,ve).second:
		dynaDAG->OpenModelEdge(v,endpoint,ve).second;
	if(!beginEdge)
		beginEdge = endEdge;
}
void Config::rerouteChain(DDChain *chain,int tailRank,int headRank,XGenerator *xgen) {
	int r = tailRank;
	for(DDPath::node_iter ni = chain->nBegin(); ni!=chain->nEnd(); ++ni, r = ranking.Down(r)) {
		RemoveNode(*ni);
		if(ranking.Below(r,headRank))
			DDd(*ni).rank = r; // don't install if it's not getting used (might be off bottom of config even)
		else
			InstallAtPos(*ni,r,xgen->xval(ranking.GetRank(r)->yBase));
	}
}
/* Adjust a virtual node chain, by shrinking, moving, and/or stretching
 * the path.  The DDd().newRank values determine the new endpoints.
 * Shrinking means cutting off vnodes on the tail side.  Stretching
 * means iteratively copying the last virtual node down to the next rank.
 * The rationale is that the tail moves downward toward the head.
 */
void Config::autoAdjustChain(DDChain *chain,int otr,int ohr,int ntr,int nhr,Layout::Node *vn,Layout::Edge *ve) {
	assert(chain->first); 
	if(nhr == ntr) 
		dynaDAG->CloseChain(chain,false);	/* flat edge / single node */
	else {
		if(!(ranking.Above(otr,nhr)&&ranking.Above(ntr,ohr)) 
			|| ve && gd<EdgeGeom>(ve).pos.Empty()) {
			if(vn) {
				constX cx(gd<NodeGeom>(vn).pos.x);
				rerouteChain(chain,ntr,nhr,&cx);
			}
			else {
				autoX ax(DDp(ve->tail)->pos(),DDp(ve->head)->pos());
				rerouteChain(chain,ntr,nhr,&ax);
			}
		}
		// stretch/shrink ends
		adjustChain(chain,false,nhr,vn,ve);
		adjustChain(chain,true,ntr,vn,ve);

		/* do not call dd_opt_path(chain) here!  other
		edges are not yet adjusted, so tangles can result. */
	}
}
void Config::autoAdjustEdge(Layout::Edge *ve) {
	DDModel::Node *t, *h;
	getLayoutEndpoints(ve,&t,&h);
	assert(DDd(t).amNodePart()&&DDd(h).amNodePart());
	int otr = DDd(t).multi->oldBottomRank, ntr = DDd(t).multi->newBottomRank,
		ohr = DDd(h).multi->oldTopRank, nhr = DDd(h).multi->newTopRank;
	if(otr >= ohr != ntr >= nhr) // moving into or out of backwardness
		autoRouteEdge(ve);
	else autoAdjustChain(DDp(ve),otr,ohr,ntr,nhr,0,ve);

	// invalidate edge cost constraints so they get reconnected right.
	// xconOwner->InvalidatePathConstraints(DDp(ve));
}
/*
void unbindEndpoints(Layout::Edge *ve) {
	DDp(ve->tail)->coordFixed = false;
	DDp(ve->head)->coordFixed = false;
}
*/
void Config::insertEdge(Layout::Edge *ve) {
	if(ve->head==ve->tail) // special case b/c head *model* node != tail *model* node
		dynaDAG->CloseChain(DDp(ve),false);
	else if(!gd<EdgeGeom>(ve).pos.Empty()) 
		userRouteEdge(ve);
	else 
		autoRouteEdge(ve);

	/*unbindEndpoints(ve); */ 	/* i don't know if this is good or bad */
}
void Config::unfixOldSingletons(ChangeQueue &changeQ) {
	/* this heuristic unfixes any node on first edge insertion */
	for(Layout::node_iter ni = changeQ.insE.nodes().begin(); ni!=changeQ.insE.nodes().end(); ++ni) {
		/* only unstick nodes if they changed ranks */
		DDModel::Node *mn = DDp(*ni)->top();
		if(!DDd(mn).prev.valid) 
			continue;
		if(DDd(mn).rank == DDd(mn).multi->oldTopRank) 
			continue;
		Layout::Node *vn = current->find(*ni);
		bool anyOldEdges = false;
		for(Layout::nodeedge_iter ei=vn->alledges().begin(); ei!=vn->alledges().end(); ++ei)
			if(!changeQ.insE.find(*ei)) {
				anyOldEdges = true;
				break;
			}
        /*
		if(!anyOldEdges)
			DDp(*ni)->coordFixed = false;
        */
	}
}

void Config::insertNewEdges(ChangeQueue &changeQ) {
	for(Layout::graphedge_iter ei = changeQ.insE.edges().begin(); ei!=changeQ.insE.edges().end(); ++ei)
		insertEdge(client->find(*ei));
	unfixOldSingletons(changeQ);
}
/* push a node through adjacent ranks.  */

void Config::percolate(DDModel::Node *n,DDModel::Node *ref,Ranks::index destrank) {
	Ranks::index r = DDd(ref).rank;
	bool down = ranking.Above(r,destrank);
	double x = DDd(ref).cur.x;
	if(down)
		for(r = ranking.Down(r); !ranking.Below(r,destrank); r = ranking.Down(r))
			x = placeAndReopt(n,r,x);
	else
		for(r = ranking.Up(r); !ranking.Above(r,destrank); r = ranking.Up(r))
			x = placeAndReopt(n,r,x);
}
double Config::placeAndReopt(DDModel::Node *n, Ranks::index r, double x) {
	int oldRank = DDd(n).rank;
	if(DDd(n).inConfig)
		RemoveNode(n);
	InstallAtPos(n,r,x);
	//dir = (oldRank < r)? UP : DOWN;
	UpDown dir = (oldRank < r)? DOWN : UP;
	return dynaDAG->GetOptimizer()->Reopt(n,dir);
}
struct compOldRank {
	bool operator()(Layout::Node *n1,Layout::Node *n2) {
		return DDp(n1)->oldTopRank < DDp(n2)->oldTopRank;
	}
};
void Config::moveOldNodes(ChangeQueue &changeQ) {
	VNodeV moveOrder;
	for(Layout::node_iter vni = changeQ.modN.nodes().begin(); vni!=changeQ.modN.nodes().end(); ++vni)
		moveOrder.push_back(*vni);
	sort(moveOrder.begin(),moveOrder.end(),compOldRank());
	for(VNodeV::iterator ni = moveOrder.begin(); ni != moveOrder.end(); ++ni) {
		Layout::Node *vn = *ni,
			*mvn = client->find(*ni);
		NodeGeom &ng = gd<NodeGeom>(vn);
		DDMultiNode *n = DDp(vn);
		if(n->newTopRank!=n->oldTopRank || n->newBottomRank!=n->oldBottomRank) {
			double x;
			DDMultiNode::node_iter ni;
			// move all nodes to either specified X or percolated X
			if(igd< ::Update>(vn).flags & DG_UPD_MOVE && ng.pos.valid || 
					gd<NodeGeom>(vn).nail & DG_NAIL_X) {
				if(!ng.pos.valid)
					throw NailWithoutPos(vn);
				x = ng.pos.x;
				//n->coordFixed = true;
				ni = n->nBegin();
			}
			else {
				percolate(n->top(),n->top(),n->newTopRank);
				x = DDd(n->top()).cur.x;
				(ni = n->nBegin())++;
				//n->coordFixed = false;
			}
			for(; ni!=n->nEnd(); ++ni) {
				int r = DDd(*ni).rank;
				RemoveNode(*ni);
				InstallAtPos(*ni,r,x);
			}
			if(!n->first) {
				if(n->newTopRank!=n->newBottomRank) { // 1-node is becoming a chain
					RemoveNode(n->node);
					InstallAtPos(n->node,n->newTopRank,x);
					DDModel::Node *bottom = dynaDAG->OpenModelNode(mvn).second;
					InstallAtPos(bottom,n->newBottomRank,x);
					constX cx(x);
					buildChain(n,n->node,bottom,&cx,mvn,0);
					n->node = 0; 
				}
				else {
					RemoveNode(n->node);
					InstallAtPos(n->node,n->newTopRank,x);
				}
			}
			else { // already a chain
				DDModel::Node *top = n->top(); // cache in case last edge gets broken
				RemoveNode(n->top());
				InstallAtPos(n->top(),n->newTopRank,DDd(n->top()).cur.x);
				RemoveNode(n->bottom());
				InstallAtPos(n->bottom(),n->newBottomRank,DDd(n->bottom()).cur.x);
				// stretch/shrink chain
				autoAdjustChain(n,n->oldTopRank,n->oldBottomRank,n->newTopRank,n->newBottomRank,mvn,0);
				// chain became a 1-node
				if(!n->node && !n->first)
					n->node = top;
			}
		}
		else { // only x has changed
			if(ng.pos.valid) {
				for(DDMultiNode::node_iter ni = n->nBegin(); ni!=n->nEnd(); ++ni) {
					DDModel::Node *mn = *ni,
						*left = Left(mn),
						*right = Right(mn);
					if((left && (DDd(left).cur.x > ng.pos.x)) ||
						(right && (DDd(right).cur.x < ng.pos.x))) {
						int r = DDd(mn).rank;
						RemoveNode(mn);
						InstallAtPos(mn,r,ng.pos.x);
					}
					else 
						DDd(mn).cur.x = ng.pos.x;
				}
				//n->coordFixed = true;
			}
			//else n->coordFixed = false;
		}
	}
}
void Config::moveOldEdges(ChangeQueue &changeQ) {
	for(Layout::graphedge_iter ei = changeQ.modE.edges().begin(); ei!=changeQ.modE.edges().end(); ++ei)
		if(igd< ::Update>(*ei).flags&DG_UPD_MOVE) // yes that space in < :: is necessary >:(
			if((*ei)->head==(*ei)->tail) // ignore self-edges
				;
			else if(userDefinedMove(*ei))
				userRouteEdge(client->find(*ei));
			else
				autoAdjustEdge(client->find(*ei));
}
void Config::splitRank(DDChain *chain,DDModel::Edge *e,Layout::Node *vn, Layout::Edge *ve) {
	Ranks::index newR = ranking.Down(DDd(e->tail).rank);
	if(newR==DDd(e->head).rank)
		return; // already there
	assert(ranking.Above(newR,DDd(e->head).rank));
	report(r_ranks,"%s %p: chain split at %d->%d:\n",vn?"multinode":"path",chain,
		DDd(e->tail).rank,DDd(e->head).rank);
	DDModel::Node *v = dynaDAG->OpenModelNode(vn).second;
	double x = (DDd(e->tail).cur.x+DDd(e->head).cur.x)/2.0; // roughly interpolate so as not to introduce crossings
	InstallAtPos(v,newR,x); 
	DDModel::Edge *newE1 = dynaDAG->OpenModelEdge(e->tail,v,ve).second,
		*newE2 = dynaDAG->OpenModelEdge(v,e->head,ve).second;
	if(chain->first==e)
		chain->first = newE1;
	if(chain->last==e)
		chain->last = newE2;
	assert(chain->first && chain->last);
	dynaDAG->CloseModelEdge(e);
	Ranks::index ur = DDd(newE1->tail).rank,
		vr = DDd(newE1->head).rank,
		wr = DDd(newE2->head).rank;
	report(r_ranks,"now %d->%d->%d\n",ur,vr,wr);
}
void Config::joinRanks(DDChain *chain,DDModel::Node *n,Layout::Edge *ve) {
	assert(n->ins().size()==1);
	assert(n->outs().size()==1);
	DDModel::Edge *e1 = *n->ins().begin(),
		*e2 = *n->outs().begin(),
		*newE = dynaDAG->OpenModelEdge(e1->tail,e2->head,ve).second;
	report(r_ranks,"%s %p: chain joined at %d->%d->%d\n",ve?"path":"multinode",chain,
		DDd(e1->tail).rank,DDd(n).rank,DDd(e2->head).rank);
	// make sure this is in the middle of the specified chain
	if(DDd(n).amNodePart())
		assert(DDd(e1).amNodePart() && DDd(e2).amNodePart() && DDd(n).multi==chain);
	else
		assert(DDd(e1).amEdgePart() && DDd(e1).path==chain && DDd(e1).path==DDd(e2).path);
	if(chain->first==e1)
		chain->first = newE;
	if(chain->last==e2)
		chain->last = newE;
	assert(chain->first && chain->last);
	dynaDAG->CloseModelEdge(e1);
	dynaDAG->CloseModelEdge(e2);
	dynaDAG->CloseModelNode(n);
	report(r_ranks,"now %d->%d\n",DDd(newE->tail).rank,DDd(newE->head).rank);
}
#ifdef FLEXIRANKS
void Config::updateRanks(ChangeQueue &changeQ) {
	// everything old has already been moved around, so we don't have to
	// deal with ends of chains
	ranking.newRanks.push_back(INT_MAX); // simple way to deal with endgame
	if(ranking.oldRanks.empty())
		ranking.oldRanks.push_back(INT_MAX);
	Ranks::IndexV::iterator ni=ranking.newRanks.begin(),
		oi = ranking.oldRanks.begin();
	if(reportEnabled(r_ranks)) {
		int dr = ranking.newRanks.size()-ranking.oldRanks.size();
		report(r_ranks,"update config: %d ranks (%s %d)\n",ranking.newRanks.size(),dr<0?"closing":"creating",abs(dr));
		/*
		report(r_ranks,"old ranks: ");
		for(Ranks::IndexV::iterator test = ranking.oldRanks.begin(); test!=ranking.oldRanks.end(); ++test)
			report(r_ranks,"%d ",*test);
		report(r_ranks,"\nnew ranks: ");
		for(test = ranking.newRanks.begin(); test!=ranking.newRanks.end(); ++test)
			report(r_ranks,"%d ",*test);
		report(r_ranks,"\n");
		*/
	}
	while(*ni!=INT_MAX || *oi!=INT_MAX) {
		while(*ni==*oi && *ni!=INT_MAX) {
			++ni;
			++oi;
		}
		while(ranking.Above(*ni,*oi)) { // additions
			report(r_ranks,"adding %d\n",*ni);
			Ranks::iterator ri = ranking.EnsureRank(*ni);
			if(ri!=ranking.begin()) {
				Ranks::iterator ri2 = ri;
				ri2--;
				for(NodeV::iterator ni = (*ri2)->order.begin(); ni!=(*ri2)->order.end(); ++ni) 
					for(DDModel::outedge_iter ei = (*ni)->outs().begin(); ei!=(*ni)->outs().end();) {
						DDModel::Edge *e = *ei++;
						if(DDd(e).amEdgePart()) {
							changeQ.ModEdge(DDd(e).path->layoutE,DG_UPD_MOVE);
							splitRank(DDd(e).path,e,0,DDd(e).path->layoutE);
						}
						else if(DDd(*ni).amNodePart() && DDd(*ni).multi==DDd(e->head).multi) {
							splitRank(DDd(*ni).multi,e,DDd(*ni).multi->layoutN,0);
							changeQ.ModNode(DDd(*ni).multi->layoutN,DG_UPD_MOVE);
						}
						else assert(0); // what's this edge doing?
					}
			}
			++ni;
		}
		while(ranking.Above(*oi,*ni)) { // deletions
			report(r_ranks,"removing %d\n",*oi);
			Ranks::iterator ri = ranking.GetIter(*oi);
			assert(ri!=ranking.end());
			while((*ri)->order.size()) {
				DDModel::Node *n = (*ri)->order.back();
				if(DDd(n).amEdgePart()) {
					DDPath *path = DDd(*n->ins().begin()).path;
					joinRanks(path,n,path->layoutE);
					changeQ.ModEdge(path->layoutE,DG_UPD_MOVE);
				}
				else {
					changeQ.ModNode(DDd(n).multi->layoutN,DG_UPD_MOVE);
					joinRanks(DDd(n).multi,n,0);
				}
			}
			ranking.RemoveRank(ri);
			++oi;
		}
	}
	ranking.oldRanks = ranking.newRanks;
}
#endif
void Config::Update(ChangeQueue &changeQ) {
	moveOldNodes(changeQ);
	moveOldEdges(changeQ);
#ifdef FLEXIRANKS
	checkEdges(false);
	updateRanks(changeQ);
	ranking.Check();
	checkEdges(true);
#endif
	insertNewNodes(changeQ);
	insertNewEdges(changeQ);
	checkEdges(true);
	checkX();
}

} // namespace DynaDAG
