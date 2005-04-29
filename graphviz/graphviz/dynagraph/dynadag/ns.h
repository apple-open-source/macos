/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <queue>
#include <limits.h>

#include "common/StrAttr.h"

namespace NS {
enum {INEDGE,OUTEDGE};
const int SearchSize = 20;

#pragma inline_recursion(off)

#include "dynadag/nsdat.h"
#include "common/useful.h"

// minimum data for this alg:
// typedef LGraph<NSData<void*,void*>,NSNode<void*,void*>,NSEdge<void*,void*> > NSGraph;
// minimal NS algorithm
// typedef NS<NSGraph,AccessNoAttr<NSGraph> > NSAlg;

// attribute access is templated so your NS graph can have a different string
// attribute system, or none.
typedef NSData<void*,void*> vNSD;
typedef NSNode<void*,void*> vNSN;
typedef NSEdge<void*,void*> vNSE;
template<typename G>
struct AccessStrAttr {
	void scan(typename G::Node *n) {
		StrAttrs::iterator i = gd<StrAttrs>(n).find("rank");
		if(i!=gd<StrAttrs>(n).end())
			gd<vNSN>(n).rank = atoi(i->second.c_str());
	}
	void scan(typename G::Edge *e) {
		StrAttrs::iterator i = gd<StrAttrs>(e).find("weight");
		if(i!=gd<StrAttrs>(e).end())
			gd<vNSE>(e).weight = atoi(i->second.c_str());
		i = gd<StrAttrs>(e).find("minlen");
		if(i!=gd<StrAttrs>(e).end())
			gd<vNSE>(e).minlen = atoi(i->second.c_str());
	}
	void scan(G *g) {
		StrAttrs::iterator i = gd<StrAttrs>(g).find("nslimit");
		if(i!=gd<StrAttrs>(g).end())
			gd<vNSD>(g).maxiter = atoi(i->second.c_str());
	}
	void attach(G *g) {
		char buf[24];
		for(typename G::node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			sprintf(buf,"%d",gd<vNSN>(*ni).rank);
			gd<StrAttrs>(*ni)["rank"] = buf;
			for(typename G::outedge_iter ei = (*ni)->outs().begin(); ei!=(*ni)->outs().end(); ++ei) {
				sprintf(buf,"%d",gd<vNSE>(*ei));
				gd<StrAttrs>(*ei)["weight"] = buf;
				sprintf(buf,"%d",gd<vNSE>(*ei));
				gd<StrAttrs>(*ei)["minlen"] = buf;
			}
		}
	}
};
template<typename G>
struct AccessNoAttr {
	void scan(typename G::Node *n) {}
	void scan(typename G::Edge *e) {}
	void scan(G *g) {}
	void attach(G *g) {}
};

enum {
	VERBOSE = (1 << 0),
	VALIDATE = (1 << 1),
	ATTACHATTRS = (1 << 2),
	RECHECK = (1 << 3),
	NORMALIZE = (1 << 4)};

template<typename G,typename AccessAttr>
struct NS {
	typedef typename G::Node Node;
	typedef typename G::Edge Edge;
	typedef typename G::node_iter node_iter;
	typedef typename G::inedge_iter inedge_iter;
	typedef typename G::outedge_iter outedge_iter;
	typedef typename G::nodeedge_iter nodeedge_iter;
	typedef NSData<Node*,Edge*> NSG;
	typedef NSNode<Node*,Edge*> NSN;
	typedef NSEdge<Node*,Edge*> NSE;
	typedef std::queue<Node*> NodeQ;
	typedef std::vector<Node*> NodeV;

	AccessAttr accAttr;

	static NSG &NSd(G *g) {
		return reinterpret_cast<NSG &>(gd<NSData<void*,void*> >(g));
	}
	static NSN &NSd(Node *n) {
		return reinterpret_cast<NSN &>(gd<NSNode<void*,void*> >(n));
	}
	static NSE &NSd(Edge *e) {
		return reinterpret_cast<NSE &>(gd<NSEdge<void*,void*> >(e));
	}

	struct CycleException : DGException {
		Node *involving;
		CycleException(Node *n) : 
		  DGException("the network symplex algorithm encountered a cycle"),
		  involving(n) {}
	};
	struct DisconnectedException : DGException {
		Node *bad;
		DisconnectedException(Node *n) : 
		  DGException("the network symplex algorithm encountered a disconnected graph"),
		  bad(n) {}
	};


private:

	// direct translation of ns.c

	/* create local data */
	template<typename T>
	bool precondition(T *a) {
		if(NSd(a).brandNew) {
			NSd(a).brandNew = false;
			return true;
		}
		else {
			NSd(a).reset();
			return false;
		}
	}
	template<typename T>
	void scan(T *a) {
		if(precondition(a))
			accAttr.scan(a);
	}
	/* decide when to cut off NS iteration */
	bool step(G *g, int iter, int verbose) {
		/*
		if (verbose && (iter % 100 == 0)) {
			int d = iter % 1000;
			if (d == 0) fputs("network simplex: ",stderr);
			fprintf(stderr,"%d ",iter);
			if (d == 9) fputc('\n',stderr);
		}
		*/
		return iter >= NSd(g).maxiter;
	}
	/* tight tree maintenance */
	void addTreeEdge(Edge *e) {
		NSE &nse = NSd(e);
		assert(!nse.prv[INEDGE]);
		assert(!nse.prv[OUTEDGE]);
		assert(!nse.nxt[INEDGE]);
		assert(!nse.nxt[OUTEDGE]);

		Node *v[2] = {e->head,e->tail};
		for(int i=0; i<2; ++i) {
			NSN &nsv = NSd(v[i]);
			nsv.mark = true;
			Edge *f = nsv.tree[i];
			if(f) {
				NSE &nsf = NSd(f);
				assert(!nsf.prv[i]);
				nsf.prv[i] = e;
			}
			nse.nxt[i] = f;
			nsv.tree[i] = e;
		}
		nse.treeflag = true;
		NSd(e->head->g).n_tree_edges++;
	}
	void delTreeEdge(Edge *e) {
		NSE &nse = NSd(e);
		Node *v[2] = {e->head,e->tail};
		for(int i=0; i<2; i++) {
			Edge *prev = nse.prv[i],
				*next = nse.nxt[i];
			nse.prv[i] = nse.nxt[i] = 0;
			if(prev)
				NSd(prev).nxt[i] = next;
			else
				NSd(v[i]).tree[i] = next;
			if(next)
				NSd(next).prv[i] = prev;
		}
		nse.treeflag = false;
		NSd(e->head->g).n_tree_edges--;
	}
	void initRank(G *g) {
		NodeQ Q;
		for(node_iter ni=g->nodes().begin(); ni!=g->nodes().end(); ++ni)
			if(!NSd(*ni).priority)
				Q.push(*ni);

		int ctr = 0;
		while(!Q.empty()) {
			Node *n = Q.front(); Q.pop();
			NSN &nsn = NSd(n);

			ctr++;
			nsn.rank = 0;

			for(inedge_iter ini = n->ins().begin(); ini!=n->ins().end(); ++ini) {
				int r = NSd((*ini)->tail).rank + NSd(*ini).minlen;
				if(nsn.rank < r)
					nsn.rank = r;
			}
			for(outedge_iter outi = n->outs().begin(); outi!=n->outs().end(); ++outi) {
				NSN &nsh = NSd((*outi)->head);
				check(nsh.priority>0);
				if(!--nsh.priority)
					Q.push((*outi)->head);
			}
		}
		check(ctr == g->nodes().size());
	}
	Node *incident(Edge *e) {
		if(NSd(e->tail).mark==NSd(e->head).mark)
			return 0;
		return NSd(e->head).mark?e->tail:e->head;
	}
	/*
	*  Returns some tree edge whose cutval is negative, or else nil.
	*  NSd converges faster if edges are searched cyclically.
	*  Might be faster if it cached negative edges not chosen this time?
	*/
	Edge *leaveEdge(G *g) {
		Node *start = NSd(g).finger;
		if(!start)
			start = *g->nodes().begin();
		int cnt = 0, pcv = 0; // pcv init for gcc
		Edge *ret = 0;
		node_iter si = g->nodes().find(start),
			ni = si;
		do {
			for(outedge_iter ei = (*ni)->outs().begin(); ei!=(*ni)->outs().end();  ++ei) {
				int cv = NSd(*ei).cutval;
				if(cv<0) {
					cnt++;
					if(!ret || cv<pcv) {
						ret = *ei;
						pcv = cv;
					}
					if(cnt>SearchSize)
						break;
				}
			}
			if(++ni==g->nodes().end())
				ni = g->nodes().begin();
		} while(ni != si);
		NSd(g).finger = *ni;
		return ret;
	}

	/*
	* choose an edge with minimum slack to bring into the tight tree
	*/
	static int calcSlack(Edge *e) {
		return NSd(e->head).rank - NSd(e->tail).rank - NSd(e).minlen;
	}
	struct MinSlackDFS {
		Edge *enter;
		int low,lim,slack;
		template<typename Edgelist>
		void goHelper(Node *v,Edgelist &el,bool inFlag) {
			for(typename Edgelist::iterator ei = el.begin(); ei!=el.end(); ++ei) {
				Node *node = inFlag?(*ei)->tail:(*ei)->head;
				if(!NSd(*ei).treeflag) {
					if(!sequence(low,NSd(node).lim,lim)) {
						int eslack = calcSlack(*ei);
						if(eslack < slack || !enter) {
							enter = *ei;
							slack = eslack;
						}
					}
				}
				else if(NSd(node).lim < NSd(v).lim)
					go(node,inFlag);
			}
		}
		void go(Node *v, bool inFlag) {
			if(inFlag)
				goHelper(v,v->ins(),true);
			else
				goHelper(v,v->outs(),false);
			int dir = inFlag?OUTEDGE:INEDGE;
			for(Edge *e = NSd(v).tree[dir]; e && slack>0; e = NSd(e).nxt[dir]) {
				Node *node = e->other(v);
				if(NSd(node).lim < NSd(v).lim)
					go(node,inFlag);
			}
		}
	};
	friend struct NS<G,AccessAttr>::MinSlackDFS;
	Edge *enterEdge(Edge *e) {
		Node *v = (NSd(e->tail).lim < NSd(e->head).lim)?
			e->tail:e->head;
		MinSlackDFS dfs;
		dfs.enter = 0;
		dfs.slack = INT_MAX;
		dfs.low = NSd(v).low;
		dfs.lim = NSd(v).lim;
 		dfs.go(v,v==e->tail);
		return dfs.enter;
	}
	bool tsearch(G *g, Node *n) {
		for(nodeedge_iter ei = n->alledges().begin(); ei!=n->alledges().end(); ++ei)
			if(!calcSlack(*ei) && !NSd(ei.target()).mark) {
				addTreeEdge(*ei);
				if(NSd(g).n_tree_edges == g->nodes().size())
					return true;
				if(tsearch(g,ei.target()))
					return true;
			}
		return false;
	}
	int tightTree(G *g) {
		if(!tsearch(g,*g->nodes().begin()))
			for(node_iter ni = ++g->nodes().begin(); ni!=g->nodes().end(); ++ni)
				if(NSd(*ni).mark && tsearch(g,*ni))
					break;
		return NSd(g).n_tree_edges + 1;
	}
	void feasibleTree(G *g, bool use_ranks) {
		if(g->nodes().size() <= 1)
			return;
		if(!use_ranks)
			initRank(g);

		NodeV nodes;
		int t;
		while((t = tightTree(g)) < g->nodes().size()) {
			if(t == 1)
				NSd(*g->nodes().begin()).mark = true;
			nodes.clear();
			Edge *e = 0;
			for(node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
				if(NSd(*ni).mark) {
					assert(NSd(*ni).tree[INEDGE] || NSd(*ni).tree[OUTEDGE] || (t == 1));
					nodes.push_back(*ni);
				}
				for(nodeedge_iter ei = (*ni)->alledges().begin(); ei!=(*ni)->alledges().end(); ++ei)
					if(!NSd(*ei).treeflag && incident(*ei))
						if(!e || calcSlack(*ei)<calcSlack(e))
							e = *ei;
			}
			check(e);
			// fprintf(stderr,"bring in %s\n",agnameof(incident(e)));
			int adj = calcSlack(e);
			check(adj);
			if(incident(e) == e->tail)
				adj = -adj;
			for(typename NodeV::iterator i = nodes.begin(); i!=nodes.end(); ++i)
				NSd(*i).rank += adj;
		}
		initCutValues(g);
	}
	/* walk up from v to LCA(v,w), setting new cutvalues. */
	Node *treeUpdate(Node *v, Node *w, int cutvalue, bool dir) {
		while(!sequence(NSd(v).low,NSd(w).lim,NSd(v).lim)) {
			Edge *e = NSd(v).paredge;
			bool d = (v == e->tail)==dir;
			if(d)
				NSd(e).cutval += cutvalue;
			else NSd(e).cutval -= cutvalue;
			if(NSd(e->tail).lim > NSd(e->head).lim)
				v = e->tail;
			else v = e->head;
		}
		return v;
	}
	void rerank(Node *v,int d) {
		NSd(v).rank -= d;
		Edge *e;
		for(e = NSd(v).tree[OUTEDGE]; e; e = NSd(e).nxt[OUTEDGE])
			if(e != NSd(v).paredge)
				rerank(e->head,d);
		for(e = NSd(v).tree[INEDGE]; e; e = NSd(e).nxt[INEDGE])
			if(e != NSd(v).paredge)
				rerank(e->tail,d);
	}

	/* e is the tree edge that is leaving and f is the nontree edge that
	* is entering.  compute new cut values, ranks, and exchange e and f.
	*/
	void update(Edge *e, Edge *f) {
		int minlen = calcSlack(f);

		/* "for(v = in nodes in tail side of e) do rank(v) -= minlen;" */
		if(minlen > 0) {
			if(NSd(e->tail).lim < NSd(e->head).lim)
				rerank(e->tail,minlen);
			else rerank(e->head,-minlen);
		}

		int cutvalue = NSd(e).cutval;
		Node *lca = treeUpdate(f->tail,f->head,cutvalue,1);
		check(treeUpdate(f->head,f->tail,cutvalue,0) == lca);
		NSd(e).cutval = 0;
		NSd(f).cutval = -cutvalue;
		delTreeEdge(e);
		addTreeEdge(f);
		dfsRange(lca,NSd(lca).paredge,NSd(lca).low);
	}
	/* utility function to compute cut values */
	int xVal(Edge *e, Node *v, bool dir) {
		int rv;
		bool f;

		Node *other = e->other(v);
		if(!sequence(NSd(v).low,NSd(other).lim,NSd(v).lim)) {
			f = true;
			rv = NSd(e).weight;
		}
		else {
			f = false;
			if(NSd(e).treeflag)
				rv = NSd(e).cutval;
			else rv = 0;
			rv -= NSd(e).weight;
		}
		bool d = dir?(e->head == v):(e->tail == v);
		if(f)
			d = !d;
		if(!d)
			rv = -rv;
		return rv;
	}
	/* set cut value of f, assuming values of edges on one side were already std::set */
	void setCutVal(Edge *f) {
		/* set v to the node on the side of the edge already searched */
		Node *v;
		bool dir;
		if(NSd(f->tail).paredge == f) {
			v = f->tail;
			dir = true;
		}
		else {
			v = f->head;
			dir = false;
		}

		int sum = 0;
		for(nodeedge_iter ei=v->alledges().begin(); ei!=v->alledges().end(); ++ei)
			sum += xVal(*ei,v,dir);
		NSd(f).cutval = sum;
	}
	void dfsCutVal(Node *v, Edge *par) {
		Edge *e;
		for(e = NSd(v).tree[OUTEDGE]; e; e = NSd(e).nxt[OUTEDGE])
			if(e != par)
				dfsCutVal(e->head,e);
		for(e = NSd(v).tree[INEDGE]; e; e = NSd(e).nxt[INEDGE])
			if(e != par)
				dfsCutVal(e->tail,e);
		if(par)
			setCutVal(par);
	}
	int dfsRange(Node *v, Edge *par, int low) {
		NSd(v).paredge = par;
		int lim = NSd(v).low = low;

		Edge *e;
		for(e = NSd(v).tree[OUTEDGE]; e; e = NSd(e).nxt[OUTEDGE])
			if(e != par)
				lim = dfsRange(e->head,e,lim);
		for(e = NSd(v).tree[INEDGE]; e; e = NSd(e).nxt[INEDGE])
			if(e != par)
				lim = dfsRange(e->tail,e,lim);
		NSd(v).lim = lim;
		return lim + 1;
	}

	void initCutValues(G *g) {
		dfsRange(*g->nodes().begin(),0,1);
		dfsCutVal(*g->nodes().begin(),0);
	}
	bool initGraph(G *g) {
		scan(g);
		node_iter ni;
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			scan(*ni);
			for(outedge_iter ei = (*ni)->outs().begin(); ei !=(*ni)->outs().end(); ++ei)
				scan(*ei);
		}

		bool feasible = true;
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			NSd(*ni).priority = 0;
			for(inedge_iter ei = (*ni)->ins().begin(); ei !=(*ni)->ins().end(); ++ei) {
				NSd(*ni).priority++;
				NSd(*ei).cutval = 0;
				if(feasible && (NSd(*ni).rank - NSd((*ei)->tail).rank) < NSd(*ei).minlen)
					feasible = false;
			}
		}
		return feasible;
	}

public:
	NS(AccessAttr accAttr = AccessAttr()) : accAttr(accAttr),Run(0) {}
	/* main entry point  to
	* assign ranks to nodes of a DAG
	*/
	int Run;
	void Solve(G *g, unsigned int flags) {
		if(reportEnabled(r_nsdump)) {
			for(node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni)
				report(r_nsdump,"%p ",*ni);
			report(r_nsdump,"\n");
			for(typename G::graphedge_iter ei = g->edges().begin(); ei!=g->edges().end(); ++ei)
				report(r_nsdump,"%p->%p\n",(*ei)->tail,(*ei)->head);
		}
		Run++;
		bool verbose = flags & VERBOSE;
		bool feasible = initGraph(g);
		if(flags & VALIDATE) 
			checkGraph(g);
		feasibleTree(g,feasible);

		int iter = 0;
		while(Edge *e = leaveEdge(g)) {
			Edge *f = enterEdge(e);
			update(e,f);
			if(step(g,++iter, verbose)) 
				break;
		}

		if(verbose || flags&RECHECK) 
			checkRanks(g,iter,verbose);
		if(flags&NORMALIZE) 
			Normalize(g);
		if(flags&ATTACHATTRS) 
			AttachAttrs(g);
	}
	void SetMinLength(Edge *e, int len) {
		preconditionEdge(e);
		NSd(e).minlen = len;
	}
	void SetWeight(Edge *e, int weight) {
		preconditionEdge(e);
		NSd(e).weight = weight;
	}
	int GetWeight(Edge *e) {
		return NSd(e).weight;
	}
	int GetMinLength(Edge *e) {
		return NSd(e).minlen;
	}
	int GetRank(Node *n) {
		return NSd(n).rank;
	}
	void SetRank(Node *n, int r) {
		preconditionNode(n);
		NSd(n).rank = r;
	}
private:
	int checkRanks(G *g, int iter, int verbose) {
		int cost = 0;
		for(node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			Node *t = *ni;
			for(outedge_iter ei = (*ni)->outs().begin(); ei!=(*ni)->outs().end(); ++ei) {
				Edge *e = *ei;
				Node *h = (*ei)->head;
				int len = NSd(h).rank - NSd(t).rank;
				check(len >= NSd(e).minlen);
				cost += NSd(e).weight*len;
			}
		}
		/*
		if(verbose) 
			fprintf(stderr,"ns: %s, %d iter, %d cost\n",agnameof(g),iter,cost);
		*/
		return cost;
	}
	void checkCutValues(G *g) {
		for(node_iter ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			for(Edge *e = NSd(*ni).tree[OUTEDGE]; e; e = NSd(e).nxt[OUTEDGE]) {
				int save = NSd(e).cutval;
				setCutVal(e);
				check(save == NSd(e).cutval);

				Edge *f;
				for(f = NSd(e->head).tree[INEDGE]; f; f = NSd(f).nxt[INEDGE])
					if(e==f) break;
				check(f);
			}
		}
	}

	void checkDFS(Node *n) {
		if(NSd(n).dmark) 
			return;
		NSd(n).dmark = true;
		NSd(n).onstack = true;
		for(outedge_iter ei = n->outs().begin(); ei!=n->outs().end(); ++ei) {
			Node *w = (*ei)->head;
			if(NSd(w).onstack) 
				throw CycleException(n);
			if(!NSd(w).dmark)
				checkDFS(w);
		}
		NSd(n).onstack = false;
	}
	void checkReach(Node *n) {
		if(n) {
			NSd(n).dmark = true;
			for(outedge_iter outi=n->outs().begin(); outi!=n->outs().end(); ++outi) {
				Node *o = (*outi)->head;
				if(!NSd(o).dmark) 
					checkReach(o);
			}
			for(inedge_iter ini=n->ins().begin(); ini!=n->ins().end(); ++ini) {
				Node *o = (*ini)->tail;
				if(!NSd(o).dmark) 
					checkReach(o);
			}
		}
	}
	void checkCon(G *g) {
		/* check if connected */
		node_iter ni;
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni)
			NSd(*ni).dmark = false;
		checkReach(*g->nodes().begin());
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			Node *n = *ni;
			if(!NSd(n).dmark)
				throw DisconnectedException(n);
		}
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) 
			NSd(*ni).dmark = false;
	}
	void checkGraph(G *g) {
		node_iter ni;
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) {
			NSd(*ni).onstack = false;
			NSd(*ni).dmark = false;
		}
		checkCon(g);

		/* check for cycles */
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni)
			checkDFS(*ni);
	}
	void AttachAttrs(G *g) {
		accAttr.attach(g);
	}
	void Normalize(G *g) {
		int minr = INT_MAX;
		node_iter ni;
		for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni)
			if(minr > NSd(*ni).rank) 
				minr = NSd(*ni).rank;
		if(minr != 0) 
			for(ni = g->nodes().begin(); ni!=g->nodes().end(); ++ni) 
				NSd(*ni).rank = NSd(*ni).rank - minr;
	}
};
}; // namespace NS
