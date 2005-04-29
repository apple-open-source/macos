/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"

ChangeQueue::ChangeQueue(Layout *client,Layout *current) : client(client),current(current),
	insN(client),modN(client),delN(client),insE(client),modE(client),delE(client) {}
// compiler never gets copy constructors right
ChangeQueue::ChangeQueue(ChangeQueue &copy) : client(copy.client),current(copy.current),
	insN(client),modN(client),delN(client),insE(client),modE(client),delE(client) {
	insN = copy.insN;
	modN = copy.modN;
	delN = copy.delN;
	insE = copy.insE;
	modE = copy.modE;
	delE = copy.delE;
}
// we don't check inserts here because it's more helpful if the server throws
void ChangeQueue::InsNode(Layout::Node *n) {
	insN.insert(n);
}
void ChangeQueue::InsEdge(Layout::Edge *e) {
	insE.insert(e);
}
void ChangeQueue::ModNode(Layout::Node *n,Update u) {
	if(u.flags && !insN.find(n) && !delN.find(n)) {
		Layout::Node *n2 = modN.insert(n).first;
		igd<Update>(n2).flags |= u.flags;
	}
}
void ChangeQueue::ModEdge(Layout::Edge *e,Update u) {
	if(u.flags && !insE.find(e) && !delE.find(e)) {
		Layout::Edge *e2 = modE.insert(e).first;
		igd<Update>(e2).flags |= u.flags;
	}
}
void ChangeQueue::DelNode(Layout::Node *n) {
	insN.erase(n);
	modN.erase(n);
	delN.insert(n);
	n = current->find(n); // remove edges that are currently inserted
	for(Layout::nodeedge_iter i = n->alledges().begin(); i!=n->alledges().end(); ++i) 
		DelEdge(*i);
}
void ChangeQueue::DelEdge(Layout::Edge *e) {
	insE.erase(e);
	modE.erase(e);
	delE.insert(e);
}
void ChangeQueue::UpdateCurrent() {
	Layout::node_iter ni;
	Layout::graphedge_iter ei;
	for(ni = insN.nodes().begin(); ni!=insN.nodes().end(); ++ni)
		if(!current->insert(*ni).second)
			throw InsertInserted();
	for(ei = insE.edges().begin(); ei!=insE.edges().end(); ++ei) {
		Layout::Node *t =(*ei)->tail,*h = (*ei)->head;
		if(!current->find(t) && !insN.find(t))
			throw EndnodesNotInserted();
		if(!current->find(h) && !insN.find(h))
			throw EndnodesNotInserted();
		if(!current->insert(*ei).second)
			throw InsertInserted();
	}
	for(ei = delE.edges().begin(); ei!=delE.edges().end(); ++ei)
		if(!current->erase(*ei))
			throw DeleteUninserted();
	for(ni = delN.nodes().begin(); ni!=delN.nodes().end(); ++ni)
		if(!current->erase(*ni))
			throw DeleteUninserted();
	for(ni = modN.nodes().begin(); ni!=modN.nodes().end(); ++ni)
		if(!current->find(*ni))
			throw ModifyUninserted();
	for(ei = modE.edges().begin(); ei!=modE.edges().end(); ++ei)
		if(!current->find(*ei))
			throw ModifyUninserted();
}
void ChangeQueue::CalcBounds() {
	Bounds &b = gd<GraphGeom>(current).bounds,
		b2;
	for(Layout::node_iter ni = current->nodes().begin(); ni!=current->nodes().end(); ++ni)
		b2 |= gd<NodeGeom>(*ni).BoundingBox();
	for(Layout::graphedge_iter ei = current->edges().begin(); ei!=current->edges().end(); ++ei)
		b2 |= gd<EdgeGeom>(*ei).pos.BoundingBox();
	if(b!=b2) {
		b = b2;
		GraphUpdateFlags() |= DG_UPD_BOUNDS;
	}
}
// clear update flags and maybe do deletions
void ChangeQueue::Okay(bool doDelete) {
	insN.clear();
	insE.clear();
	modN.clear();
	modE.clear();
	if(doDelete) {
		for(Layout::graphedge_iter j = delE.edges().begin(); j!=delE.edges().end();) {
			Layout::Edge *e = *j++;
			check(client->erase_edge(e));
		}
        delE.clear(); // the nodes may still exist
		for(Layout::node_iter i = delN.nodes().begin(); i!=delN.nodes().end();) {
			Layout::Node *n = *i++;
			check(client->erase_node(n));
		}
	}
	else {
		delE.clear();
		delN.clear();
	}
	GraphUpdateFlags() = 0;
    assert(Empty());
}
ChangeQueue &ChangeQueue::operator=(ChangeQueue &Q) {
	assert(client==Q.client);
	insN = Q.insN;
	modN = Q.modN;
	delN = Q.delN;
	insE = Q.insE;
	modE = Q.modE;
	delE = Q.delE;
	GraphUpdateFlags() = Q.GraphUpdateFlags();
	return *this;
}
ChangeQueue &ChangeQueue::operator+=(ChangeQueue &Q) {
	assert(client==Q.client);
	Layout::node_iter ni;
	Layout::graphedge_iter ei;
	for(ni = Q.insN.nodes().begin(); ni!=Q.insN.nodes().end(); ++ni)
		InsNode(*ni);
	for(ei = Q.insE.edges().begin(); ei!=Q.insE.edges().end(); ++ei)
		InsEdge(*ei);
	for(ni = Q.modN.nodes().begin(); ni!=Q.modN.nodes().end(); ++ni)
		ModNode(*ni,igd<Update>(*ni));
	for(ei = Q.modE.edges().begin(); ei!=Q.modE.edges().end(); ++ei)
		ModEdge(*ei,igd<Update>(*ei));
	for(ni = Q.delN.nodes().begin(); ni!=Q.delN.nodes().end(); ++ni)
		DelNode(*ni);
	for(ei = Q.delE.edges().begin(); ei!=Q.delE.edges().end(); ++ei)
		DelEdge(*ei);
	GraphUpdateFlags() |= Q.GraphUpdateFlags();
	return *this;
}

// CompoundServer
void CompoundServer::Process(ChangeQueue &Q) {
	for(ServerV::iterator i = actors.begin(); i!=actors.end(); ++i)
		(*i)->Process(Q);
}
CompoundServer::~CompoundServer() {
	for(ServerV::iterator i = actors.begin(); i!=actors.end(); ++i)
		delete *i;
	actors.clear();
}
