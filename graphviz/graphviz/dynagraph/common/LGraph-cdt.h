/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef LGRAPH_H
#define LGRAPH_H

#pragma warning (disable : 4786 4503)
#include <list>
#include <map>
#include <set>
#include <algorithm>
#pragma warning (disable : 4786 4503)
#include "common/dt.h"

// extracts a part of MI datum, by type
template<typename D,typename GO>
inline D &gd(GO *go) {
	return *static_cast<D*>(go->dat);
}
// extracts instance-specific datum by type
template<typename D,typename GO>
inline D &igd(GO *go) {
	return static_cast<D&>(go->idat);
}
struct Nothing {};

// exceptions
struct LGraphException {}; 
// when trying to use set operations with subgraphs
struct NoCommonParent : LGraphException {}; 
struct NullPointer : LGraphException {};
struct IteratorOutOfBounds : LGraphException {};
// attempted to use objects from another graph
struct WrongGraph : LGraphException {};
// attempted to use edge with node it's not connected to  
struct WrongNode : LGraphException {}; 

template<class T>
class pseudo_seq {
	T m_begin,m_end;
public:
	pseudo_seq(T begin,T end) : m_begin(begin),m_end(end) {}
	T begin() { return m_begin; }
	T end() { return m_end; }
	int size() {
		int ret=0;
		for(T i = m_begin; i!=m_end; ++i) ++ret;
		return ret;
	}
};
template<typename GraphDatum,typename NodeDatum, typename EdgeDatum, // same data in every image
	// different data in different images; gets copied on insert
	typename GraphIDat = Nothing, typename NodeIDat = Nothing, typename EdgeIDat = Nothing> 
struct LGraph {
	typedef LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat> Graph;
	struct Edge;
	class Node;

private:
	struct Seq {
		int seq;
	};
	struct ND2 : NodeDatum, Seq {
		ND2(const NodeDatum &d) : NodeDatum(d) {}
	};
	struct ED2 : EdgeDatum, Seq {
		ED2(const EdgeDatum &d) : EdgeDatum(d) {}
	};
	template<typename T>
	struct SeqComp {
		int operator ()(T *one,T *two) const {
			int s1 = gd<Seq>(one).seq,
				s2 = gd<Seq>(two).seq;
			return s1 - s2;
		}
	};
	struct inseqlink : cdt::seqlink {};
	struct outseqlink : cdt::seqlink {};
	struct intreelink : cdt::treelink {};
	struct outtreelink : cdt::treelink {};
	struct headtreelink : cdt::treelink {};
public:
	struct Edge : inseqlink,outseqlink,intreelink,outtreelink,headtreelink {
		LGraph * const g;
		ED2 * const dat;
		EdgeIDat idat;
		Node *const tail,*const head;
		Node *other(Node *n) {
			if(n==tail)
				return head;
			else if(n==head)
				return tail;
			else
				return 0;
		}
		bool amMain() { return g->amMain(); }
	private:
		friend struct LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat>;
		Edge(LGraph *g, Node *tail, Node *head, ED2 *dat) : 
		  g(g),
		  dat(dat),
		  tail(tail),
		  head(head) {
			if(g) {
				if(!tail) throw NullPointer();
				if(!head) throw NullPointer();
				if(!dat) throw NullPointer();
			}
		}
		~Edge() {}
	};
	struct HeadSeqComp {
		int operator ()(Edge *e1,Edge *e2) const {
			int s1 = gd<Seq>(e1->head).seq,
				s2 = gd<Seq>(e2->head).seq;
			return s1 - s2;
		}
	};
	typedef cdt::derived_accessor<Edge,inseqlink> inedge_sequence_accessor;
	typedef cdt::derived_accessor<Edge,outseqlink> outedge_sequence_accessor;
	typedef cdt::sequence<inedge_sequence_accessor> inedge_sequence;
	typedef cdt::sequence<outedge_sequence_accessor> outedge_sequence;

	typedef cdt::derived_accessor<Edge,intreelink> inedge_tree_accessor;
	typedef cdt::derived_accessor<Edge,outtreelink> outedge_tree_accessor;
	typedef cdt::disc<inedge_tree_accessor,SeqComp<Edge> > inedge_tree_disc;
	typedef cdt::disc<outedge_tree_accessor,SeqComp<Edge> > outedge_tree_disc;
	inedge_tree_disc m_inedgetreedisc;
	outedge_tree_disc m_outedgetreedisc;
	typedef cdt::tree_dict<inedge_tree_disc> inedge_tree_dict;
	typedef cdt::tree_dict<outedge_tree_disc> outedge_tree_dict;
	inedge_tree_dict m_inedgetreedict;
	outedge_tree_dict m_outedgetreedict;
	typedef cdt::tree<inedge_tree_disc> inedge_tree;
	typedef cdt::tree<outedge_tree_disc> outedge_tree;

	typedef cdt::ordering<inedge_tree,inedge_sequence> inedge_order;
	typedef cdt::ordering<outedge_tree,outedge_sequence> outedge_order;
	//typedef inedge_tree inedge_order;
	//typedef outedge_tree outedge_order;
	
	typedef typename inedge_order::iterator inedge_iter;
	typedef typename outedge_order::iterator outedge_iter;

	typedef cdt::derived_accessor<Edge,headtreelink> head_tree_accessor;
	typedef cdt::disc<head_tree_accessor,HeadSeqComp> head_tree_disc;
	head_tree_disc m_headdisc;
	typedef cdt::tree_dict<head_tree_disc> head_tree_dict;
	head_tree_dict m_headtreedict;
	typedef cdt::tree<head_tree_disc> head_tree;

	class nodeedge_iter { 
		inedge_sequence *m_ins;
		outedge_sequence *m_outs;
		struct itre { // eh, inefficient, but whatever.  can't do union
			typename inedge_sequence::iterator in;
			typename outedge_sequence::iterator out;
		} i;
		void goOut() {
			m_ins = 0;
			if(m_outs->empty())
				m_outs = 0;
			else 
				i.out = m_outs->begin();
		}
		friend struct LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat>;
		nodeedge_iter(inedge_sequence *ins, outedge_sequence *outs) : m_ins(ins), m_outs(outs) {
			if(m_ins) {
				if(m_ins->empty()) 
					goOut();
				else 
					i.in = m_ins->begin();
			}
		}
	public:
		Edge *operator *() {
			if(!m_outs)
				return 0;
			else if(m_ins)
				return *i.in;
			else
				return *i.out;
		}
		bool headward() {
			return !m_ins;
		}
		Node *target() {
			Edge *e = **this;
			if(!e)
				return 0;
			else if(headward())
				return e->head;
			else
				return e->tail;
		}
		nodeedge_iter &operator ++() {
			if(!m_outs)
				throw IteratorOutOfBounds();
			if(!m_ins) {
				if(++i.out==m_outs->end())
					m_outs = 0;
			}
			else if(++i.in==m_ins->end())
				goOut();
			return *this;
		}
		nodeedge_iter operator ++(int) {
			nodeedge_iter ret = *this;
			operator++();
			return ret;
		}
		bool operator ==(nodeedge_iter other) {
			if(m_ins!=other.m_ins || m_outs!=other.m_outs)
				return false;
			if(m_ins==0 && m_outs==0)
				return true;
			if(m_ins==0)
				return i.out==other.i.out;
			else
				return i.in==other.i.in;
		}
		bool operator !=(nodeedge_iter other) {
			return !(*this==other);
		}
		inedge_iter inIter() {
			return head()->inIter(this);
		}
		outedge_iter outIter() {
			return tail()->outIter(this);
		}
	};
	// workaround for circular typing problems w/ friend decl: do not call!
	static nodeedge_iter ne_iter(inedge_sequence *i,outedge_sequence *o) {
		return nodeedge_iter(i,o);
	}
	class Node : public cdt::seqtreelink {
	  inedge_order m_ins;
	  outedge_order m_outs;
	  friend struct LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat>;
	  head_tree m_outFinder; // used exclusively by find_edge(t,h)
	  Node(LGraph *g,ND2 *dat) : 
	  m_ins(g->m_inedgetreedisc,g->m_inedgetreedict),
	  m_outs(g->m_outedgetreedisc,g->m_outedgetreedict),
	  m_outFinder(g->m_headdisc,g->m_headtreedict),
	  g(g),
	  dat(dat) {}
	  ~Node() {}
	public:
		LGraph * const g;
		ND2 * const dat;
		NodeIDat idat;
		
		inedge_order &ins() {
			return m_ins;
		}
		outedge_order &outs() {
			return m_outs;
		}
		pseudo_seq<nodeedge_iter> alledges() {
			return pseudo_seq<nodeedge_iter>(ne_iter(&m_ins,&m_outs),ne_iter(0,0));
		};
		
		int degree() {
			return m_ins.size() + m_outs.size();
		}
		bool amMain() { return g->amMain(); }
		inedge_iter inIter(Edge *e) {
			if(e->head!=this)
				throw WrongNode();
			return m_ins.make_iter(static_cast<inseqlink*>(e));
		}
		inedge_iter outIter(Edge *e) {
			if(e->tail!=this)
				throw WrongNode();
			return m_outs.make_iter(static_cast<outseqlink*>(e));
		}
	};
	
	typedef std::list<LGraph *> subgraph_list;
	typedef typename subgraph_list::iterator subgraph_iter;

	typedef cdt::derived_accessor<Node,cdt::seqlink> node_sequence_accessor;
	typedef cdt::derived_accessor<Node,cdt::treelink> node_tree_accessor;
	typedef cdt::sequence<node_sequence_accessor> node_sequence;
	typedef cdt::disc<node_tree_accessor,SeqComp<Node> > node_tree_disc;
	node_tree_disc m_nodetreedisc;
	typedef cdt::tree_dict<node_tree_disc> node_tree_dict;
	node_tree_dict m_nodetreedict;
	typedef cdt::tree<node_tree_disc> node_tree;
	typedef cdt::ordering<node_tree,node_sequence> node_order;
	//typedef node_tree node_order;
	typedef typename node_order::iterator node_iter;

private:
	subgraph_list m_subs;
	node_order m_nodes;
	int m_nNumber, m_eNumber;

public:
	LGraph * const parent;
	GraphDatum * const dat;
	GraphIDat idat;
	LGraph(LGraph *parent=0) : 
	  m_inedgetreedict(m_inedgetreedisc),
	  m_outedgetreedict(m_outedgetreedisc),
	  m_headtreedict(m_headdisc),
	  m_nodetreedict(m_nodetreedisc),
	  m_nodes(m_nodetreedisc,m_nodetreedict),
	  m_nNumber(0), m_eNumber(0),
	  parent(parent), 
	  dat(parent?parent->dat:new GraphDatum)
	{
		if(parent)
			parent->m_subs.push_back(this);
	}
	// explicit copy constructor because it's a really bad idea to do memberwise copy!
	LGraph(const LGraph &other) : 
	  m_inedgetreedict(m_inedgetreedisc),
	  m_outedgetreedict(m_outedgetreedisc),
	  m_headtreedict(m_headdisc),
	  m_nodetreedict(m_nodetreedisc),
	  m_nodes(m_nodetreedisc,m_nodetreedict),
	  m_nNumber(0), m_eNumber(0),
	  parent(other.parent), 
	  dat(parent?parent->dat:new GraphDatum(*other.dat)) {
	    *this = other;
	}
	~LGraph() {
		clear();
		if(parent)
			parent->m_subs.remove(this);
		if(!parent)
			delete dat;
	}
	node_iter iter(Node *n) {
		return m_nodes.make_iter(static_cast<cdt::seqlink*>(n));
	}
	node_order &nodes() {
		return m_nodes;
	}
	subgraph_list &subgraphs() {
		return m_subs;
	}
	// to iterate on all edges in a graph
	struct graphedge_set;
	struct graphedge_iter {
		Edge *operator *() {
			if(!g)
				return 0;
			return *ei;
		}
		graphedge_iter &operator ++() {
			++ei;
			advance();
			return *this;
		}
		graphedge_iter operator ++(int) {
			graphedge_iter ret = *this;
			operator++();
			return ret;
		}
		bool operator ==(graphedge_iter other) {
			return g==0?other.g==0:(g==other.g && ni==other.ni && ei==other.ei);
		}
		bool operator !=(graphedge_iter other) {
			return !(*this==other);
		}
		graphedge_iter() : g(0) {}
	private:
		friend struct LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat>;
		graphedge_iter(LGraph *g) : g(g) {
			if(g) {
				if((ni = g->nodes().begin())==g->nodes().end())
					this->g = 0;
				else {
					ei = (*ni)->outs().begin();
					advance();
				}
			}
		}
		void advance() {
			if(!g) throw IteratorOutOfBounds();
			while(ei==(*ni)->outs().end()) {
				if(++ni==g->nodes().end()) {
					g = 0;
					return;
				}
				ei = (*ni)->outs().begin();
			}
		}
		typename node_sequence::iterator ni;
		typename outedge_sequence::iterator ei;
		LGraph *g;
	};
	pseudo_seq<graphedge_iter> edges() {
		return pseudo_seq<graphedge_iter>(graphedge_iter(this),graphedge_iter(0));
	}
	// methods available only on main graphs
	Node *create_node(const NodeDatum &d = NodeDatum()) {
		if(parent)
			return 0;
		Node *ret = new Node(this,new ND2(d));
		gd<Seq>(ret).seq = ++m_nNumber;
		m_nodes.insert(ret);
		return ret;
	}
	std::pair<Edge*,bool> create_edge(Node *tail, Node *head,const EdgeDatum &d = EdgeDatum()) {
		if(parent)
			return std::make_pair((Edge*)0,false);
		if(!tail) throw NullPointer();
		if(tail->g!=this) throw WrongGraph();
		if(!head) throw NullPointer();
		if(head->g!=this) throw WrongGraph();
		if(Edge *found = find_edge(tail,head))
			return std::make_pair(found,false);
		Edge *ret = new Edge(this,tail,head,new ED2(d));
		gd<Seq>(ret).seq = ++m_eNumber;
		tail->m_outs.insert(ret);
		tail->m_outFinder.insert(ret);
		head->m_ins.insert(ret);
		return std::make_pair(ret,true);
	}
	// methods available only on subgraphs
	// the shorter, overloaded methods insert,erase,find are intended to 
	// mimic std::set<> operations.  it's a little less explicit what you're doing though.
	std::pair<Node*,bool> insert_subnode(Node *n) {
		if(Node *found = find_nodeimage(n))
			return std::make_pair(found,false);
		if(!parent||!parent->insert_subnode(n).first)
			return std::make_pair((Node*)0,false);
		Node *ret = new Node(this,n->dat);
		ret->idat = n->idat;
		m_nodes.insert(ret);
		return std::make_pair(ret,true);
	}
	std::pair<Edge*,bool> insert_subedge(Edge *e) {
		if(Edge *found = find_edgeimage(e))
			return std::make_pair(found,false);
		if(!parent||!parent->insert_subedge(e).first)
			return std::make_pair((Edge*)0,false);
		Node *t = insert_subnode(e->tail).first,
			*h = insert_subnode(e->head).first;
		Edge *ret = new Edge(this,t,h,e->dat);
		ret->idat = e->idat;
		t->m_outs.insert(ret);
		t->m_outFinder.insert(ret);
		h->m_ins.insert(ret);
		return std::make_pair(ret,true);
	}
	std::pair<Node*,bool> insert(Node *n) {
		return insert_subnode(n);
	}
	std::pair<Edge*,bool> insert(Edge *e) {
		return insert_subedge(e);
	}
	// methods available on both graphs and subgraphs
	bool amMain() {
		return !parent;
	}
	bool erase_node(Node *n) {
		if(n->g!=this)
			if(Node *n2 = find_nodeimage(n))
				n = n2;
			else
				return false;
		for(typename subgraph_list::iterator i = m_subs.begin(); i!=m_subs.end(); ++i)
			(*i)->erase_node(n);
		while(!n->m_outs.empty())
			erase_edge(*n->m_outs.begin());
		while(!n->m_ins.empty())
			erase_edge(*n->m_ins.begin());
		m_nodes.erase(n);
		if(!parent)
			delete n->dat;
		delete n;
		return true;
	}
	bool erase_edge(Edge *e) {
		if(e->head->g!=this)
			if(Edge *e2 = find_edgeimage(e))
				e = e2;
			else
				return false;
		for(typename subgraph_list::iterator i = m_subs.begin(); i!=m_subs.end(); ++i)
			(*i)->erase_edge(e);
		e->tail->m_outs.erase(e);
		e->tail->m_outFinder.erase(e);
		e->head->m_ins.erase(e);
		if(!parent)
			delete e->dat;
		delete e;
		return true;
	}
	bool erase(Node *n) {
		return erase_node(n);
	}
	bool erase(Edge *e) {
		return erase_edge(e);
	}
	Edge *find_edge(Node *tail, Node *head) {
		if(tail->g!=this)
			if(!(tail = find_nodeimage(tail)))
				return 0;
		if(head->g!=this)
			if(!(head = find_nodeimage(head)))
				return 0;
		Edge key(0,0,head,0);
		typename head_tree::iterator i = tail->m_outFinder.find(&key);
		if(i==tail->m_outFinder.end())
			return 0;
		else
			return *i;
	}
	Node *find_nodeimage(Node *n) {
		node_iter i = m_nodes.find(n);
		if(i==m_nodes.end())
			return 0;
		else
			return *i;
	}
	Edge *find_edgeimage(Edge *e) {
		node_iter i = m_nodes.find(e->tail);
		if(i==m_nodes.end())
			return 0;
		outedge_iter j = (*i)->m_outs.find(e);
		if(j==(*i)->m_outs.end())
			return 0;
		else
			return *j;
	}
	Node *find(Node *n) {
		return find_nodeimage(n);
	}
	Edge *find(Edge *e) {
		return find_edgeimage(e);
	}
	// another std::set<> like method
	void clear() {
		while(!m_nodes.empty())
			erase_node(*m_nodes.begin());
	}
	void check_common_parent(const LGraph &g) {
		const LGraph *p,*p2;
		for(p = parent; p->parent; p = p->parent);
		for(p2 = &g; p2->parent; p2 = p2->parent);
		if(p!=p2)
			throw NoCommonParent();
	}
	// operators
	LGraph &operator|=(const LGraph &cg) { // union
		LGraph &g = const_cast<LGraph&>(cg); // uck
		if(parent) {
			// if a subgraph, we're inserting everything
			check_common_parent(g);
			for(node_iter ni = g.m_nodes.begin(); ni!=g.m_nodes.end(); ++ni)
				insert(*ni);
			for(graphedge_iter ei(&g); ei!=graphedge_iter(); ++ei)
				insert(*ei);
		}
		else {
			// if not, we're copying everything
			std::map<Node*,Node*> remember;
			node_iter ni;
			for(ni = g.m_nodes.begin(); ni!=g.m_nodes.end(); ++ni) {
				// don't copy Seq, just rest!
				Node *n = create_node(*(*ni)->dat);
				// copy instance data
				n->idat = (*ni)->idat;
				remember[*ni] = n;
			}
			for(ni = g.m_nodes.begin(); ni!=g.m_nodes.end(); ++ni) 
				for(outedge_iter ei = (*ni)->m_outs.begin(); ei!=(*ni)->m_outs.end(); ++ei) {
					Node *t = remember[(*ei)->tail],
						*h = remember[(*ei)->head];
					Edge *e = create_edge(t,h,*(*ei)->dat).first;
					e->idat = (*ei)->idat;
				}
		}
		return *this;
	}
	LGraph &operator&=(LGraph &g) {
		check_common_parent(g);
		for(node_iter ni = nodes().begin(),next=ni; ni!=nodes().end(); ni = next) {
			++next;
			if(!g.find(*ni))
				erase(*ni);
		}
		for(graphedge_iter ei = edges().begin(),nex=ei; ei!=edges().end(); ei = nex) {
			++nex;
		  	if(!g.find(*ei))
				erase(*ei);
		}
		return *this;
	}			
	LGraph &operator=(const LGraph &g) {
		clear();
		*this |= g;
		*dat = *g.dat;
		return *this;
	}

};
#ifdef STUPID_DFS
/* depth first search.  visitor looks like
	struct Visitor {
		bool vEdge(Edge *e,bool headward);
		bool vNode(Node *n);
	};
   visitor methods return true to continue down this tree. */
template<typename GD,typename ND,typename ED,typename GID,typename NID,typename EID,
        typename Visitor>
void dfs(typename LGraph<GD,ND,ED,GID,NID,EID>::Edge *e,Visitor &v,bool headward) {
	if(!v.vEdge(e,headward))
		return;
	if(headward)
		dfs<GD,ND,ED,GID,NID,EID,Visitor>(e->head,v,e);
	else
		dfs<GD,ND,ED,GID,NID,EID,Visitor>(e->tail,v,e);
}
template<typename GD,typename ND,typename ED,typename GID,typename NID,typename EID,
        typename Visitor>
void dfs(LGraph<GD,ND,ED,GID,NID,EID>::Node *n,Visitor &v,
                LGraph<GD,ND,ED,GID,NID,EID>::Edge *entered=0) {
	if(!v.vNode(n))
		return;
    typename LGraph<GD,ND,ED,GID,NID,EID>::edge_iter i;
	for(i = n->outs.begin();i!=n->outs.end();++i)
		if(*i!=entered)
			dfs<GD,ND,ED,GID,NID,EID,Visitor>(*i,v,true);
	for(i = n->ins.begin(); i!=n->ins.end(); ++i)
		if(*i!=entered)
			dfs<GD,ND,ED,GID,NID,EID,Visitor>(*i,v,false);

}
#endif
		
#endif
