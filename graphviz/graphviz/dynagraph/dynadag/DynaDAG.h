/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"
#include "dynadag/ns.h"
#include <float.h>

namespace DynaDAG {

struct InternalErrorException : DGException {
  InternalErrorException() : DGException("an internal error has occurred in dynadag's x constraints") {}
};

// future template parameters?
#define FLEXIRANKS
 
// how many times must I say this?
#pragma warning (disable : 4786 4503)

// dynadag constraint graphs: basic data + debug accounting
struct ConstraintType {
	enum {unknown,anchor,stab,node,rankWeak,orderEdgeStraighten} why;
};
struct DDCNodeData : NS::NSNode<void*,void*>, ConstraintType {};
typedef LGraph<NS::NSData<void*,void*>,DDCNodeData,NS::NSEdge<void*,void*> > DDCGraph;
typedef NS::NS<DDCGraph,NS::AccessNoAttr<DDCGraph> > DDNS;

struct NodeConstraints {
	DDCGraph::Node *n, // the variable
		*stab; // for stability constraints
	NodeConstraints() : n(0),stab(0) {}
	~NodeConstraints() {
		assert(!n);
		assert(!stab);
	}
};
// these templates are a workaround for the circular typing problem
// DDNode and DDEdge cannot refer to the graph made with them.  
template<typename N,typename E>
struct Chain {
	E *first,*last;
	Chain() : first(0),last(0) {}
	struct edge_iter {
		E *operator *() {
			return i;
		}
		edge_iter &operator ++() {
			if(i==chain->last)
				i = 0;
			else {
				assert(i);
				i = *i->head->outs().begin(); 
			}
			return *this;
		}
		edge_iter operator ++(int) {
			edge_iter ret = *this;
			operator++();
			return ret;
		}
		bool operator ==(const edge_iter &it) const {
			return it.chain==chain && it.i==i;
		}
		bool operator !=(const edge_iter &it) const {
			return !(*this==it);
		}
		edge_iter &operator=(const edge_iter &it) {
			i = it.i;
			chain = it.chain;
			return *this;
		}
		friend struct Chain<N,E>;
		Chain *chain; // public cuz I can't do friend struct Chain<N,E>::node_iter; (in msvc++?)
		edge_iter() {}
	private:
		edge_iter(Chain *chain,E *i) : chain(chain), i(i) {
			if(i)
				assert(i->head->g); // make sure valid pointer
		}
		E *i;
	};
	struct node_iter {
		N *operator *() {
			return *ei?(*ei)->head:0;
		}
		node_iter &operator ++() {
			++ei;
			skipLast();
			return *this;
		}
		node_iter operator ++(int) {
			node_iter ret = *this;
			operator++();
			return ret;
		}
		bool operator ==(const node_iter &it) const {
			return ei==it.ei;
		}
		bool operator !=(const node_iter &it) const {
			return !(*this==it);
		}
		friend struct Chain<N,E>;
	private:
		node_iter(edge_iter ei) : ei(ei) {
			skipLast();
		}
		edge_iter ei;
		void skipLast() {
			if(*ei==ei.chain->last)
				++ei;
		}
	};
	edge_iter eBegin() {
		return edge_iter(this,first);
	}
	edge_iter eEnd() {
		return edge_iter(this,0);
	}
	node_iter nBegin() {
		return node_iter(eBegin());
	}
	node_iter nEnd() {
		return node_iter(eEnd());
	}
};
template<typename N,typename E>
struct MultiNode : Chain<N,E> {
	// one node: first==last==0, node!=0
	// multi-node: first,last!=0, node==0
	N *node;
	Layout::Node *layoutN;
	NodeConstraints topC,bottomC,xcon;
	bool hit; // for rank dfs
	int newTopRank,	// destination rank assignment
		newBottomRank,
		oldTopRank,	// previous rank assignment
		oldBottomRank;
	bool rankFixed; // whether nailed in Y
	//	coordFixed; // whether a good place in X has been found
	MultiNode() : node(0),layoutN(0),hit(false),newTopRank(0),newBottomRank(0),
		oldTopRank(0),oldBottomRank(0),rankFixed(false) /*,coordFixed(false)*/ {}
	struct node_iter {
		N *operator *() {
			switch(state) {
			case Mine:
				return MN()->node;
			case Tail:
				return MN()->first->tail;
			case Heads:
				return *ei?(*ei)->head:0;
			default:
				return 0;
			}
		}
		node_iter &operator ++() {
			switch(state) {
			case Mine:
			case Tail:
				state = Heads;
				return *this;
			case Heads:
				++ei;
				return *this;
			default:
				assert(0);
				return *this;
			}
		}
		node_iter operator ++(int) {
			node_iter ret = *this;
			operator++();
			return ret;
		}
		bool operator ==(const node_iter &it) const {
			return state==it.state && ei==it.ei;
		}
		bool operator !=(const node_iter &it) const {
			return !(*this==it);
		}
		node_iter &operator=(const node_iter it) {
			ei = it.ei;
			state = it.state;
			return *this;
		}
		friend struct MultiNode<N,E>;
		node_iter() {}
	private:
		node_iter(typename Chain<N,E>::edge_iter ei,bool end) : ei(ei) {
			if(end)
				state = Heads;
			else if(MN()->node)
				state = Mine;
			else if(MN()->first)
				state = Tail;
			else
				state = Heads;
		}
		MultiNode *MN() { return static_cast<MultiNode*>(ei.chain); }
		enum State {Mine,Tail,Heads} state;
		typename Chain<N,E>::edge_iter ei;
	};
	node_iter nBegin() {
		return node_iter(eBegin(),false);
	}
	node_iter nEnd() {
		return node_iter(eEnd(),true);
	}
	Position pos() {
		if(!top() || !DDd(top()).cur.valid)
			return Position();
		return Position(DDd(top()).cur.x,(DDd(top()).cur.y+DDd(bottom()).cur.y)/2.0);
	}
	N *top() {
		return node?node:first->tail;
	}
	N *bottom() {
		return node?node:last->head;
	}
	int len() {
		int n=0;
		for(node_iter ni = nBegin(); ni!=nEnd(); ++ni,++n);
		return n;
	}
};
// if a single edge, first=last=the edge
template<typename N,typename E>
struct Path : Chain<N,E> {
	// self or flat: first==last==0; use layoutE to figure out ends
	Layout::Edge *layoutE;
	Line unclippedPath;
	// ranking vars
	DDCGraph::Node *weak; 
	DDCGraph::Edge *strong;
	Path() : weak(0),strong(0) {}
};
struct NSEdgePair {
	DDCGraph::Edge *e[2];
	NSEdgePair(DDCGraph::Node *anchor,DDCGraph::Node *tail,DDCGraph::Node *head) {
		e[0] = anchor->g->create_edge(anchor,tail).first;
		e[1] = anchor->g->create_edge(anchor,head).first;
	}
};
typedef enum _UpDown {UP,DOWN} UpDown;
typedef enum _LeftRight {LEFT=-1,RIGHT=1} LeftRight;
struct Median {
	double val; // value 
	bool exists, // if defined 
		cached; // if definition is current 
	Median() : val(0),exists(false),cached(false) {}
};
template<typename N,typename E>
struct DDNodeT {
	// general
	MultiNode<N,E> *multi; // the chain of nodes this is part of, if representing layout node
	NodeConstraints xcon; // constraints for x Coord
	NodeConstraints &getXcon() { // defer to multi's if any
		if(multi)
			return multi->xcon;
		else
			return xcon;
	}
	// config
	Median med[2]; // UpDown
	int rank,
		order;
	bool inConfig;
	// geometry
	Position cur, prev;
	// mincross order w/in rank constraint
	int orderConstraint;
	// only used in path vnodes:
	double actualX;	// spline intercept 
	bool actualXValid;

	DDNodeT() : multi(0),rank(0),order(0),inConfig(false),actualX(0),
		actualXValid(false) {}
	bool amNodePart() {
		return multi!=0;
	}
	bool amEdgePart() {
		return multi==0;
	}
};
template<typename N,typename E>
struct DDEdgeT {
	Path<N,E> *path; // the chain of edges this is part of, if representing a layout edge
	DDCGraph::Node *cn; // X constraint node
	DDEdgeT() : path(0),cn(0) {}
	~DDEdgeT() {
		assert(!cn);
	}
	bool amNodePart() {
		return path==0;
	}
	bool amEdgePart() {
		return path!=0;
	}
};

// vertical constraint weights
#define UPWARD_TENDENCY 0 // try to pull children toward parents
#define	STABILITY_FACTOR_Y	1 // keep nodes near where they were
#define EDGELENGTH_WEIGHT 10 // try to shorten edges
#define	BACKEDGE_PENALTY	100  // try to point weak edges downward
#define NODEHEIGHT_PENALTY	1000000 // don't stretch nodes

// horizontal constraint weights
#define COMPACTION_STRENGTH 0 // how much to allow gaps between nodes
#define	STABILITY_FACTOR_X	100 // keep nodes near where they were
#define BEND_WEIGHT 1000 // keep adjacent v-nodes close

#define MINCROSS_PASSES 12	
#define NODECROSS_PENALTY 1000 // don't let anything cross nodes

#pragma warning (disable : 4355)

struct DDModel : LGraph<Nothing,DDNodeT<void,void>,DDEdgeT<void,void>,Nothing,Nothing > {
	typedef LGraph<Nothing,DDNodeT<void,void>,DDEdgeT<void,void>,Nothing,Nothing > G;
	// the real types, hampered by circular typing problems
	typedef Chain<Node,Edge> C;
	typedef MultiNode<Node,Edge> MN;
	typedef Path<Node,Edge> P;
	typedef DDNodeT<Node,Edge> DDN;
	typedef DDEdgeT<Node,Edge> DDE;
	DDModel() : dirty(this) {}

	G dirty;
};
typedef DDModel::C DDChain;
typedef DDModel::MN DDMultiNode;
typedef DDModel::P DDPath;
typedef DDModel::DDN DDNode;
typedef DDModel::DDE DDEdge;
inline DDMultiNode *&DDp(Layout::Node *n) {
	DDMultiNode *&ret = *reinterpret_cast<DDMultiNode**>(&gd<ModelPointer>(n).model);
	return ret;
}
inline DDPath *&DDp(Layout::Edge *e) {
	DDPath *&ret = *reinterpret_cast<DDPath**>(&gd<ModelPointer>(e).model);
	return ret;
}
/*
template<>
inline DDNode &gd<DDNode,DDModel::Node>(DDModel::Node *n) {
	return reinterpret_cast<DDNode&>(gd<DDNodeT<void,void> >(n));
}
*/
inline DDNode &DDd(DDModel::Node *n) {
	return reinterpret_cast<DDNode&>(gd<DDNodeT<void,void> >(n));
}
inline DDEdge &DDd(DDModel::Edge *e) {
	return reinterpret_cast<DDEdge&>(gd<DDEdgeT<void,void> >(e));
}
inline const char *type(DDModel::Node *mn) {
	return DDd(mn).amEdgePart()?"path":"multinode";
}
inline void *thing(DDModel::Node *mn) {
	if(DDd(mn).multi)
		return DDd(mn).multi;
	else 
		return DDd(*mn->ins().begin()).path;
}
typedef std::vector<DDModel::Node*> NodeV;
typedef std::vector<DDModel::Edge*> EdgeV;
typedef std::pair<DDModel::Node *,DDModel::Node *> NodePair;
typedef std::set<Layout::Node*> NodeSet;
#include "dynadag/Medians.h"
// utility
struct Crossings {
	unsigned edgeEdgeCross,nodeEdgeCross,nodeNodeCross;
	Crossings() : edgeEdgeCross(0),nodeEdgeCross(0),nodeNodeCross(0) {}
	Crossings(DDModel::Edge *e,DDModel::Edge *f) : edgeEdgeCross(0),nodeEdgeCross(0),nodeNodeCross(0) {
		add(e,f);
	}
	void add(DDModel::Edge *e,DDModel::Edge *f) {
		switch(DDd(e).amNodePart()+DDd(f).amNodePart()) {
		case 0:
			edgeEdgeCross++;
			break;
		case 1:
			nodeEdgeCross++;
			break;
		case 2:
			nodeNodeCross++;
			break;
		}
	}
	Crossings &operator +=(const Crossings &c) {
		edgeEdgeCross += c.edgeEdgeCross;
		nodeEdgeCross += c.nodeEdgeCross;
		nodeNodeCross += c.nodeNodeCross;
		return *this;
	}
}; 
Crossings uvcross(DDModel::Node *v, DDModel::Node *w, bool use_in, bool use_out);
inline unsigned crossweight(Crossings cc) {
  return cc.edgeEdgeCross + NODECROSS_PENALTY*cc.nodeEdgeCross + 
	  NODECROSS_PENALTY*NODECROSS_PENALTY*cc.nodeNodeCross;
}
inline unsigned crosslight(Crossings cc) {
	return cc.edgeEdgeCross + cc.nodeEdgeCross + cc.nodeNodeCross;
}
struct Optimizer {
	virtual void Reorder(Layout &nodes,Layout &edges) = 0;
	virtual double Reopt(DDModel::Node *n,UpDown dir) = 0;
};
struct DynaDAGServices {
	virtual std::pair<DDMultiNode*,DDModel::Node*> OpenModelNode(Layout::Node *layoutN) = 0;
	virtual void CloseModelNode(DDModel::Node *n) = 0;
	virtual std::pair<DDPath*,DDModel::Edge*> OpenModelEdge(DDModel::Node *u, DDModel::Node *v, Layout::Edge *layoutE) = 0;
	virtual void CloseModelEdge(DDModel::Edge *e) = 0;
	virtual void CloseChain(DDChain *chain,bool killEndNodes) = 0;
	virtual Optimizer *GetOptimizer() = 0;
};
// config needs to erase x ordering constraints but otherwise doesn't "do" constraints.
// thus XSolver should handle it.
struct XConstraintOwner {
	virtual void RemoveNodeConstraints(DDModel::Node *n) = 0;
	//virtual void InvalidatePathConstraints(DDPath *path) = 0;
	virtual void DeleteLRConstraint(DDModel::Node *u, DDModel::Node *v) = 0;
};

struct Rank {
	NodeV order;
	double yBase,	/* absolute */
		deltaAbove, deltaBelow, spaceBelow;
	Rank(double sep) : yBase(-17),deltaAbove(sep/20),
	  deltaBelow(sep/20), spaceBelow(sep) {}
	Rank(Rank &o) : order(o.order),yBase(o.yBase),
	  deltaAbove(o.deltaAbove),deltaBelow(o.deltaBelow) {}
	double yBelow(double fract) {
#ifdef FLEXIRANKS
		return yBase;
#else
		return yBase - deltaBelow - fract*spaceBelow;
#endif
	}
	double yAbove(double fract) {
#ifdef FLEXIRANKS
		return yBase + 2*deltaAbove;
#else
		return yBase + deltaAbove + fract*spaceBelow; // HACK: wrong spaceBelow but all are nodeSep.y now
#endif
	}
	double Height() {
		return deltaAbove+deltaBelow+spaceBelow;
	}
};
struct ConseqRanks : std::vector<Rank*> {
	typedef int index;
	double sep;
	ConseqRanks(double div,double sep) : sep(sep),low(0),high(-1) {}
	ConseqRanks(ConseqRanks &o) {
		*this = o;
	}
	ConseqRanks &operator =(ConseqRanks &o) {
		clear();
		sep = o.sep;
		low = o.low;
		high = o.high;
		for(iterator ri = o.begin(); ri!=o.end(); ++ri) 
			push_back(new Rank(**ri));
		return *this;
	}
	index Low() { return low; }
	index High() { return high; }
	bool Above(index a,index b) {
		return a<b;
	}
	bool Below(index a,index b) {
		return a>b;
	}
	index Up(index r) {
		return r-1;
	}
	index Down(index r) {
		return r+1;
	}
	iterator GetIter(index r) {
		if(low > high || r < low || r > high)
			return end();
		return begin() + r - low;
	}
	Rank *GetRank(index r)	{
		iterator ri = GetIter(r);
		if(ri==end())
			return 0;
		else 
			return *ri;
	}
	iterator EnsureRank(index r) {
		if(r < low || r > high)
			if(low <= high)
				extendConfig(std::min(r,low),std::max(r,high));
			else // first rank
				extendConfig(r,r);
		return GetIter(r);
	}
	index IndexOfIter(iterator ri) {
		return index(ri-begin()+low);
	}
	index MapCoordToRank(double y) {
		if(empty()) {		/* no config exists yet */
			index ret = 0;
			Rank *rank = *EnsureRank(ret);
			rank->yBase = y;
			return ret;
		}
		Rank *rank = front();
	#ifndef DOWN_GREATER
		if(y > rank->yAbove(1.0)) /* above min rank */
			return ROUND(low - (y - rank->yBase) / rank->Height());
	#else
		if(y < rank->yAbove(1.0)) /* above min rank */
			return ROUND(low - (rank->yBase - y) / rank->Height());
	#endif
		rank = back();
	#ifndef DOWN_GREATER
		if(y < rank->yBelow(1.0)) /* below max rank */
			return ROUND(high + (rank->yBase - y) / rank->Height());
	#else
		if(y > rank->yBelow(1.0)) /* below max rank */
			return ROUND(high + (y - rank->yBase) / rank->Height());
	#endif

		/* somewhere in between */
		index bestrank = low;
		double bestdist = DBL_MAX;
		for(iterator ri = begin(); ri!=end(); ++ri) {
			double d = absol(y - (*ri)->yBase);
			if(d < bestdist) {
				bestdist = d; 
				bestrank = index(ri-begin()+low);
			}
		}
		return bestrank;
	}
	void Check();
private:
	index low,high;
        Rank* &at(int i) { return operator[](i); } // for earlier gccs
	void extendConfig(int newLow, int newHigh) {
		int osize = high<low ? 0 : high - low + 1,
			nsize = newHigh - newLow + 1;
		int where = int(newLow < low ? 0 : size());
		// make some sorta vague initial Ys
		double dy,y;
		if(osize) {
		  dy = where?-at(high-low)->Height():at(0)->Height(),
		  y = where?at(high-low)->yBase:at(0)->yBase;
		}
		else {
		  assert(nsize==1);
		  y = dy = -17; // will get reset
		}
		insert(begin()+where,nsize-osize,(Rank*)0);
		for(int i = 0; i < nsize-osize; ++i) {
		  y += dy;
		  (at(where+i) = new Rank(sep))->yBase = y;
		}
		low = newLow;
		high = newHigh;
	}
};
struct CompRank {
	bool operator()(Rank *r1,Rank *r2) const {
#ifndef DOWN_GREATER
		return r1->yBase>r2->yBase;
#else
		return r1->yBase<r2->yBase;
#endif
	}
};
struct FlexiRanks : std::set<Rank*,CompRank> {
	typedef int index;
	double div,sep;
	FlexiRanks(double div,double sep) : div(div),sep(sep) {}
	FlexiRanks(FlexiRanks &o) {
		*this = o;
	}
	~FlexiRanks() {
		reset();
	}
	void reset() {
		iterator ri = begin();
		while(ri!=end()) {
			Rank *r = *ri;
			iterator er = ri++;
			erase(er);
			delete r;
		}
	}
	FlexiRanks &operator =(FlexiRanks &o) {
		reset();
		oldRanks = o.oldRanks;
		newRanks = o.newRanks;
		div = o.div;
		sep = o.sep;
		for(iterator ri = o.begin(); ri!=o.end(); ++ri) {
			Rank *nr = new Rank(**ri);
			insert(nr);
		}
		return *this;
	}
	Rank *front() { return *begin(); }
	Rank *back() { return *rbegin(); }
	index Low() { if(empty()) return 0; else return y2r(front()->yBase); }
	index High() { if(empty()) return 0; else return y2r(back()->yBase); }
	bool Above(index a,index b) {
		return a<b;
	}
	bool Below(index a,index b) {
		return a>b;
	}
	index Up(index r) {
		if(r==INT_MIN)
			return r;
		iterator ri = GetIter(r);
		if(ri==begin())
			return INT_MIN;
		return IndexOfIter(--ri);
	}
	index Down(index r) {
		if(r==INT_MAX)
			return r;
		iterator ri = GetIter(r);
		if(++ri==end())
			return INT_MAX;
		return IndexOfIter(ri);
	}
	iterator GetIter(index r) {
		Rank q(sep);
		q.yBase = r2y(r);
		return find(&q);
	}
	Rank *GetRank(index r)	{
		iterator ri = GetIter(r);
		if(ri==end())
			return 0;
		else 
			return *ri;
	}
	iterator EnsureRank(index r) {
		assert(r!=INT_MAX && r!=INT_MIN); // off bottom or top
		iterator ri = GetIter(r);
		if(ri==end()) {
			Rank *rank = new Rank(sep);
			rank->yBase = r2y(r);
			ri = insert(rank).first;
		}
		return ri;
	}
	index IndexOfIter(iterator ri) {
		return y2r((*ri)->yBase);
	}
	index MapCoordToRank(double y) {
		return y2r(y);
	}
	// Flexi-specific
	void RemoveRank(iterator ri) {
		Rank *del = *ri;
		erase(ri);
		delete del;
	}
	index y2r(double y) {
#ifndef DOWN_GREATER
		return -ROUND(y/div);
#else
		return ROUND(y/div);
#endif
	}
	double r2y(index r) {
#ifndef DOWN_GREATER
		return -r*div;
#else
		return r*div;
#endif
	}
	void Check();
	typedef std::vector<index> IndexV;
	IndexV newRanks,oldRanks;
};
struct XGenerator {
	virtual double xval(double y) = 0;
};
struct Config {
#ifdef FLEXIRANKS // pseudo-template
	typedef FlexiRanks Ranks; 
#else
	typedef ConseqRanks Ranks;
#endif
	Config(DynaDAGServices *dynaDAG,DDModel &model, 
	       Layout *client,Layout *current,
	       XConstraintOwner *xconOwner, 
	       double yRes, Coord sep) : 
	  ranking(yRes,sep.y),
	  prevLow(INT_MAX),
	  nodeSep(sep),
	  model(model),
	  client(client),
	  current(current),
	  dynaDAG(dynaDAG),
	  xconOwner(xconOwner)
  {}
	// called by DynaDAGServer
	void Update(ChangeQueue &changeQ);
	void SetYs();
	// services
	double LeftExtent(DDModel::Node *n);
	double RightExtent(DDModel::Node *n);
	double TopExtent(DDModel::Node *n);
	double BottomExtent(DDModel::Node *n);
	double UVSep(DDModel::Node *left,DDModel::Node *right);
	double CoordBetween(DDModel::Node *L, DDModel::Node *R);
	NodePair NodesAround(int r,double x);
	DDModel::Node *RelNode(DDModel::Node *n, int offset);
	DDModel::Node *Right(DDModel::Node *n);
	DDModel::Node *Left(DDModel::Node *n);
	void InstallAtRight(DDModel::Node *n, Ranks::index r);
	void InstallAtOrder(DDModel::Node *n, Ranks::index r, unsigned order,double x);
	void InstallAtOrder(DDModel::Node *n, Ranks::index r, unsigned order);
	void InstallAtPos(DDModel::Node *n, Ranks::index r, double x);
	void RemoveNode(DDModel::Node *n);
	void Exchange(DDModel::Node *u, DDModel::Node *v);
	void Restore(Ranks &backup);

	Ranks ranking;
	Ranks::index prevLow;
	const Coord nodeSep;
	DDModel &model;
	Layout *client,*current; // same as DynaDAGServer::
private:
	DynaDAGServices *dynaDAG;
	XConstraintOwner *xconOwner;

	// update
	void insertNode(Layout::Node *vn);
	void insertNewNodes(ChangeQueue &changeQ);
	void buildChain(DDChain *chain, DDModel::Node *t, DDModel::Node *h, XGenerator *xgen,Layout::Node *vn,Layout::Edge *ve);
	void routeEdge(Layout::Edge *ve, XGenerator *xgen);
	void userRouteEdge(Layout::Edge *ve);
	void autoRouteEdge(Layout::Edge *vi);
	void adjustChain(DDChain *path,bool tail,Ranks::index dest,Layout::Node *vn,Layout::Edge *ve);
	//void adjustTail(DDChain *path, Ranks::index dest);
	void rerouteChain(DDChain *chain,int tailRank,int headRank,XGenerator *xgen);
	void autoAdjustChain(DDChain *chain,int otr,int ohr,int ntr,int nhr,Layout::Node *vn,Layout::Edge *ve);
	void autoAdjustEdge(Layout::Edge *ve);
	void insertEdge(Layout::Edge *ve);
	void unfixOldSingletons(ChangeQueue &changeQ);
	void insertNewEdges(ChangeQueue &changeQ);
	void percolate(DDModel::Node *n,DDModel::Node *ref,Ranks::index destrank);
	double placeAndReopt(DDModel::Node *n, Ranks::index r, double x);
	void moveOldNodes(ChangeQueue &changeQ);
	void moveOldEdges(ChangeQueue &changeQ);
	void splitRank(DDChain *chain,DDModel::Edge *e,Layout::Node *vn, Layout::Edge *ve);
	void joinRanks(DDChain *chain,DDModel::Node *n,Layout::Edge *ve);
#ifdef FLEXIRANKS
	void updateRanks(ChangeQueue &changeQ);
#endif
	void reoptAllEdgesTouched(ChangeQueue &changeQ);
        // set Ys
	void resetRankBox(Rank *rank);
	void resetBaselines();
	// check
	void checkEdges(bool strict);
	void checkX();
};
struct ConstraintGraph : DDCGraph {
	Node *anchor;

	ConstraintGraph();
	Node *GetVar(NodeConstraints &nc);
	void Stabilize(NodeConstraints &nc, int newrank, int weight);
	void Unstabilize(NodeConstraints &nc);
	void RemoveNodeConstraints(NodeConstraints &nc);
};
inline int rankLength(Layout::Edge *e) {
	return std::max(0,int(gd<EdgeGeom>(e).lengthHint));
}
struct Ranker {
	Ranker(DynaDAGServices *dynaDAG,Config &config) : dynaDAG(dynaDAG),config(config),top(cg.create_node()) {}
	// called by DynaDAG
	void RemoveLayoutNodeConstraints(DDMultiNode *m);
	void RemovePathConstraints(DDPath *path);
	void Rerank(ChangeQueue &changeQ);
private:
	DynaDAGServices *dynaDAG;
	Config &config;
	ConstraintGraph cg;
	ConstraintGraph::Node *top; // to pull loose nodes upward
	void makeStrongConstraint(DDPath *path);
	void makeWeakConstraint(DDPath *path);
	void fixNode(Layout::Node *n,bool fix);
	void moveOldNodes(ChangeQueue &changeQ);
	void insertNewNodes(ChangeQueue &changeQ);
	void stabilizePositionedNodes(ChangeQueue &changeQ);
	void insertNewEdges(ChangeQueue &changeQ);
	bool simpleCase(ChangeQueue &changeQ);
	void recomputeRanks(ChangeQueue &changeQ);
#ifdef FLEXIRANKS
	void makeRankList(ChangeQueue &changeQ);
#endif
	void checkStrongConstraints(ChangeQueue &changeQ);
};
// the classic DynaDAG path optimizer
struct PathOptim : Optimizer {
	PathOptim(Config &config) : config(config) {}
	void Reorder(Layout &nodes,Layout &edges);
	double Reopt(DDModel::Node *n,UpDown dir);
private:
	Config &config;
	void optPath(DDPath *path);
	bool leftgoing(DDModel::Node *n, UpDown dir, int eq_pass);
	void shiftLeft(DDModel::Node *n);
	bool rightgoing(DDModel::Node *n, UpDown dir, int eq_pass);
	void shiftRight(DDModel::Node *n);
	double coordBetween(DDModel::Node *L, DDModel::Node *R);
	void resetCoord(DDModel::Node *n);
	void optElt(DDModel::Node *n, UpDown dir, int eq_pass);
};
struct MedianTwiddle : Optimizer {
	MedianTwiddle(Config &config) : config(config) {}
	void Reorder(Layout &nodes,Layout &edges);
	double Reopt(DDModel::Node *n,UpDown dir);
private:
	Config &config;
	bool repos(DDModel::Node *n,UpDown dir);
	bool leftgoing(DDModel::Node *n,UpDown dir);
	bool rightgoing(DDModel::Node *n,UpDown dir);
};
struct MedianShuffle : Optimizer {
	MedianShuffle(Config &config) : config(config) {}
	void Reorder(Layout &nodes,Layout &edges);
	double Reopt(DDModel::Node *n,UpDown dir);
private:
	Config &config;
};
#include "dynadag/SiftMatrix.h"

struct Sifter : Optimizer {
	Sifter(Config &config) : config(config) {}
	void Reorder(Layout &nodes,Layout &edges);
	double Reopt(DDModel::Node *n,UpDown dir);
private:
	Config &config;
	enum way {lookIn,lookOut,lookAll};
	bool pass(SiftMatrix &matrix,NodeV &optimOrder,enum way way);
};
struct HybridOptimizer : Optimizer {
	HybridOptimizer(Config &config) : config(config) {}
	void Reorder(Layout &nodes,Layout &edges);
	double Reopt(DDModel::Node *n,UpDown dir);
private:
	Config &config;
};
struct DotlikeOptimizer : Optimizer {
	DotlikeOptimizer(Config &config) : config(config) {}
	void Reorder(Layout &nodes,Layout &edges);
	double Reopt(DDModel::Node *n,UpDown dir);
private:
	Config &config;
};
void getCrossoptModelNodes(Layout &nodes,Layout &edges,NodeV &out);
struct XSolver : XConstraintOwner {
	XSolver(Config &config, double xRes) : 
		xScale(1.0/xRes),config(config) {}
        virtual ~XSolver() {} // to shut gcc up
	const double xScale; 
	void Place(ChangeQueue &changeQ);
	void RemoveEdgeConstraints(DDModel::Edge *e);
	// XConstraintOwner
	void RemoveNodeConstraints(DDModel::Node *n);
	void InvalidateChainConstraints(DDChain *path);
	void DeleteLRConstraint(DDModel::Node *u, DDModel::Node *v);
private:
	Config &config;
	ConstraintGraph cg;
	void fixSeparation(DDModel::Node *mn);
	void doNodesep(Layout *subLayout);
	void doEdgesep(Layout *subLayout);
	void restoreNodesep(ChangeQueue &changeQ);
	void fixEdgeCost(DDModel::Edge *me);
	void fixLostEdges(Layout *subLayout);
	void doEdgeCost(Layout *subLayout);
	void restoreEdgeCost(ChangeQueue &changeQ);
	void stabilizeNodes(ChangeQueue &changeQ);
	void readoutCoords();
	void checkLRConstraints();
	void checkEdgeConstraints();
};
struct Spliner {
	Spliner(Config &config) : config(config) {}
	friend struct TempRoute;
	bool MakeEdgeSpline(DDPath *path,SpliningLevel splineLevel);
private:
	Config &config;
	void forwardEdgeRegion(DDModel::Node *tl, DDModel::Node *hd,DDPath *inp, Coord tp, Coord hp, Line &out);
	void flatEdgeRegion(DDModel::Node *tl, DDModel::Node *hd, Coord tp, Coord hp, Line &out);
	void adjustPath(DDPath *path);
};
struct FlexiSpliner {
	FlexiSpliner(Config &config) : config(config) {}
	bool MakeEdgeSpline(DDPath *path,SpliningLevel splineLevel);
private:
	Config &config;
};
/*
struct OptimizerChooser {
	typedef std::pair<int,Optimizer*> Choice;
	typedef std::vector<Choice> Choices;
	Choices choices;
	Optimizer *choose(int n) {
		for(Choices::reverse_iterator rci = choices.rbegin(); rci!=choices.rend(); ++rci)
			if(n>=rci->first)
				return rci->second;
		return 0;
	}
};
*/
struct DynaDAGServer : Server,DynaDAGServices {
	DDModel model; // client graph + virtual nodes & edges for tall nodes & edge chains
	Config config;	// indexes layout nodes by rank and order 
	Ranker ranker;
	//OptimizerChooser optChooser;
	Optimizer *optimizer;
	XSolver xsolver;
#ifdef FLEXIRANKS
	FlexiSpliner spliner;
#else
	Spliner spliner;
#endif

	DynaDAGServer(Layout *client,Layout *current) :
		Server(client,current),
		model(),
		config(this,model,client,current,&xsolver,gd<GraphGeom>(current).resolution.y,gd<GraphGeom>(current).separation), 
		ranker(this,config), 
		optimizer(new DotlikeOptimizer(config)),
		xsolver(config,gd<GraphGeom>(current).resolution.x),
		spliner(config) {}
	~DynaDAGServer();
	// Server
	void Process(ChangeQueue &changeQ);
	// DynaDAGServices
	std::pair<DDMultiNode*,DDModel::Node*> OpenModelNode(Layout::Node *layoutN);
	void CloseModelNode(DDModel::Node *n);
	std::pair<DDPath*,DDModel::Edge*> OpenModelEdge(DDModel::Node *u, DDModel::Node *v, Layout::Edge *layoutE);
	void CloseModelEdge(DDModel::Edge *e);
	void CloseChain(DDChain *chain,bool killEndNodes);
	Optimizer *GetOptimizer();
private:
	void closeLayoutNode(Layout::Node *n);
	void closeLayoutEdge(Layout::Edge *e);
	void executeDeletions(ChangeQueue &changeQ);
	void findOrdererSubgraph(ChangeQueue &changeQ,Layout &outN,Layout &outE);
	void updateBounds(ChangeQueue &changeQ);
	void findChangedNodes(ChangeQueue &changeQ);
	bool edgeNeedsRedraw(DDPath *path,ChangeQueue &changeQ);
	void sketchEdge(DDPath *path); // draw polyline, for debug
	void redrawEdges(ChangeQueue &changeQ,bool force);
	void cleanUp();
	void dumpModel();
};
typedef std::vector<Layout::Node*> VNodeV;

inline bool userDefinedMove(Layout::Edge *ve) {
	return gd<EdgeGeom>(ve).manualRoute;
	//return flags & DG_UPD_MOVE && !gd<EdgeGeom>(ve).pos.Empty();
}

} // namespace DynaDAG
