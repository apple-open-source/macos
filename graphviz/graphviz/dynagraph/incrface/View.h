/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "createLayoutServer.h"

// basic stuff to handle an engine/layout pair with named nodes & edges.
// applications (e.g. the dynagraph executable & dynagraph COM component)
// derive from this and override the Incr...() methods to share a basic 
// understanding of how to deal with names, how to create/replace the layout server, etc.

struct View;
typedef std::map<DString,View*> Views;
extern Views g_views; // in incrcmds
struct View {
	typedef std::map<DString,Layout::Node*> nodeDict;
	typedef std::map<DString,Layout::Edge*> edgeDict;
	nodeDict nodes;
	edgeDict edges;
	Layout layout, // everything the client has created
        current, // what's currently being controlled by engine
        old; // when switching engines, the stuff that was already here
	ChangeQueue Q;
	Server *dgserver;
	int locks;
	bool allowAnOpen; // when created outside incrface but want to allow "open view"

	// implement these to respond to incrface events
	virtual void IncrHappened() = 0;
	virtual void IncrNewNode(Layout::Node *n) = 0;
	virtual void IncrNewEdge(Layout::Edge *e) = 0;

	View(Name name = Name());
	virtual ~View();
    // create or replace engine based on "engines" attribute of layout
    void createServer();
    // complete an engine replacement by changing the insertions into new engine into modifies
    // (call after dgserver->Process but before dealing with Q
    void completeReplacement();
	// tricky problem: if client doesn't specify a name, we need to generate one
	// can't use e.g. e17 because the client might already have something named that!
	// so generate a random name
	static DString randomName(char prefix);
	// to detect those bad names that agraph is generating
	static bool poorEdgeName(const char *name);
	std::pair<Layout::Node*,bool> getNode(DString id,bool create);
	Layout::Node *getNode(DString id);
	std::pair<Layout::Edge*,bool> getEdge(DString id,Layout::Node *t,Layout::Node *h,bool create);
	std::pair<Layout::Edge*,bool> getEdge(DString id,DString tail,DString head,bool create);
	Layout::Edge *getEdge(DString id,DString tail,DString head);
	Layout::Edge *getEdge(DString id);
	void rename(Layout *l,DString newName);
	void rename(Layout::Node *n,DString newName);
	void rename(Layout::Edge *e,DString newName);
	void forget(Layout::Node *n);
	void forget(Layout::Edge *e);
	// important to destroy edges because graph representation won't allow
	// you to draw two edges t->h
	void destroy(Layout::Node *n);
	void destroy(Layout::Edge *e);
};
