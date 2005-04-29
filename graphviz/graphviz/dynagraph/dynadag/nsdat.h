/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

template<typename N, typename E>
struct NSNode {
	int rank;
	int low,lim;
	int priority;
	E paredge;
	E tree[2];
	bool mark,dmark,onstack;
	bool brandNew;
	NSNode() : brandNew(true) {
		reset();
	}
	void reset() {
		rank = low = lim = priority = 0;
		paredge = tree[INEDGE] = tree[OUTEDGE] = 0;
		mark = dmark = onstack = false;
	}
};
template<typename N, typename E>
struct NSEdge {
	int cutval;
	int weight;
	int minlen;
	E prv[2],nxt[2];
	bool treeflag;
	bool brandNew;
	NSEdge() : weight(1),minlen(1),brandNew(true) {
		reset();
	}
	void reset() {
		cutval = 0;
		treeflag = false;
		prv[INEDGE] = prv[OUTEDGE] = nxt[INEDGE] = nxt[OUTEDGE] = 0;
	}
};
template<typename N, typename E>
struct NSData {
	int n_tree_edges;
	int maxiter;
	N finger;
	bool brandNew;
	NSData() : maxiter(INT_MAX),brandNew(true) {
		reset();
	}
	void reset() {
		n_tree_edges = 0;
		finger = 0;
	}
};
