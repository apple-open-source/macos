/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement i
s available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef GRAPH_H
#define GRAPH_H

#pragma warning (disable : 4786 4503)
#include <assert.h>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#pragma warning (disable : 4786 4503)
using namespace std;
#pragma warning (disable : 4786 4503)

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
	struct ND2 : NodeDatum, Seq {};
	struct ED2 : EdgeDatum, Seq {};
	template<typename T>
	struct SeqLess {
		bool operator ()(T *one,T *two) const {
			return gd<Seq>(one).seq < gd<Seq>(two).seq;
		}
	};
public:
	struct Edge {
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
		Edge(LGraph *g, Node *tail, Node *head, ED2 *dat) : g(g),tail(tail),head(head),dat(dat) {
			if(g) {
				assert(tail);
				assert(head);
				assert(dat);
			}
		}
		~Edge() {}
	};
	struct HeadSeqLess {
		bool operator ()(Edge *e1,Edge *e2) const {
			return gd<Seq>(e1->head).seq < gd<Seq>(e2->head).seq;
		}
	};
	typedef std::set<Edge*,HeadSeqLess> headseq_set;
	typedef std::set<Edge*,SeqLess<Edge> > edge_sequence;
	typedef edge_sequence::iterator inedge_iter;
	typedef edge_sequence::iterator outedge_iter;
	class nodeedge_iter { 
		edge_sequence *m_ins,*m_outs;
		edge_sequence::iterator i;
		void goOut() {
			m_ins = 0;
			if(!m_outs->size())
				m_outs = 0;
			else 
				i = m_outs->begin();
		}
		friend struct LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat>;
		nodeedge_iter(edge_sequence *ins, edge_sequence *outs) : m_ins(ins), m_outs(outs) {
			if(m_ins) {
				if(!m_ins->size()) 
					goOut();
				else 
					i = m_ins->begin();
			}
		}
	public:
		Edge *operator *() {
			if(!m_outs)
				return 0;
			else return *i;
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
			assert(m_outs);
			if(!m_ins) {
				if(++i==m_outs->end())
					m_outs = 0;
			}
			else if(++i==m_ins->end())
				goOut();
			return *this;
		}
		nodeedge_iter operator ++(int) {
			nodeedge_iter ret = *this;
			operator++();
			return ret;
		}
		bool operator ==(nodeedge_iter other) {
			return m_outs==0 && other.m_outs==0 || other.i==i;
		}
		bool operator !=(nodeedge_iter other) {
			return !(*this==other);
		}
	};
	// workaround for circular typing problems w/ friend decl: do not call!
	static nodeedge_iter ne_iter(edge_sequence *i,edge_sequence *o) {
		return nodeedge_iter(i,o);
	}
	class Node : NodeIDat {
		edge_sequence m_ins,m_outs;
		friend struct LGraph<GraphDatum,NodeDatum,EdgeDatum,GraphIDat,NodeIDat,EdgeIDat>;
		headseq_set m_outFinder; // used exclusively by find_edge(t,h)
		Node(LGraph *g,ND2 *dat) : g(g),dat(dat) {}
		~Node() {}
	public:
		LGraph * const g;
		ND2 * const dat;
		NodeIDat idat;
		
		edge_sequence &ins() {
			return m_ins;
		}
		edge_sequence &outs() {
			return m_outs;
		}
		pseudo_seq<nodeedge_iter> alledges() {
			return pseudo_seq<nodeedge_iter>(ne_iter(&m_ins,&m_outs),ne_iter(0,0));
		};
		
		int degree() {
			return m_ins.size() + m_outs.size();
		}
		bool amMain() { return g->amMain(); }
	};
	
	typedef std::list<LGraph *> subgraph_std::list;
	typedef std::set<Node*,SeqLess<Node> > node_sequence;
	typedef node_sequence::iterator node_iter;
	typedef subgraph_list::iterator subgraph_iter;

private:
	subgraph_list m_subs;
	node_sequence m_nodes;
	int m_nNumber, m_eNumber;

public:
	LGraph * const parent;
	GraphDatum * const dat;
	GraphIDat idat;
	node_sequence &nodes() {
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
			assert(g);
			while(ei==(*ni)->outs().end()) {
				if(++ni==g->nodes().end()) {
					g = 0;
					return;
				}
				ei = (*ni)->outs().begin();
			}
		}
		node_sequence::iterator ni;
		edge_sequence::iterator ei;
		LGraph *g;
	};
	pseudo_seq<graphedge_iter> edges() {
		return pseudo_seq<graphedge_iter>(graphedge_iter(this),graphedge_iter(0));
	}
	LGraph(LGraph *parent=0) : parent(parent), m_nNumber(0), m_eNumber(0),
			dat(parent?parent->dat:new GraphDatum) {
		if(parent)
			parent->m_subs.push_back(this);
	}
	~LGraph() {
		clear();
		if(parent)
			parent->m_subs.remove(this);
		if(!parent)
			delete dat;
	}
	// methods available only on main graphs
	Node *create_node() {
		if(parent)
			return 0;
		Node *ret = new Node(this,new ND2);
		gd<Seq>(ret).seq = ++m_nNumber;
		m_nodes.insert(ret);
		return ret;
	}
	std::pair<Edge*,bool> create_edge(Node *tail, Node *head) {
		if(parent)
			return std::make_pair((Edge*)0,false);
		assert(tail && tail->g==this);
		assert(head && head->g==this);
		if(Edge *found = find_edge(tail,head))
			return std::make_pair(found,false);
		Edge *ret = new Edge(this,tail,head,new ED2);
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
		for(subgraph_list::iterator i = m_subs.begin(); i!=m_subs.end(); ++i)
			(*i)->erase_node(n);
		while(n->m_outs.size())
			erase_edge(*n->m_outs.begin());
		while(n->m_ins.size())
			erase_edge(*n->m_ins.begin());
		m_nodes.erase(n);
		if(!parent)
			delete n->dat;
		delete n;
		return true;
	}
	bool erase_edge(Edge *e) {
		if(e->g!=this)
			if(Edge *e2 = find_edgeimage(e))
				e = e2;
			else
				return false;
		for(subgraph_list::iterator i = m_subs.begin(); i!=m_subs.end(); ++i)
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
		headseq_set::iterator i = tail->m_outFinder.find(&key);
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
	// another set<> like method
	void clear() {
		while(m_nodes.size())
			erase_node(*m_nodes.begin());
	}
	// operators
	LGraph &operator+=(LGraph &g) {
		if(parent) {
			// if a subgraph, we're inserting everything
			LGraph *p,*p2;
			for(p = parent; p->parent; p = p->parent);
			for(p2 = &g; p2->parent; p2 = p2->parent);
			assert(p==p2);
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
				Node *n = create_node();
				// don't copy Seq, just rest!
				*static_cast<NodeDatum*>(n->dat) = *static_cast<NodeDatum*>((*ni)->dat);
				// copy instance data
				n->idat = (*ni)->idat;
				remember[*ni] = n;
			}
			for(ni = g.m_nodes.begin(); ni!=g.m_nodes.end(); ++ni) 
				for(outedge_iter ei = (*ni)->m_outs.begin(); ei!=(*ni)->m_outs.end(); ++ei) {
					Node *t = remember[(*ei)->tail],
						*h = remember[(*ei)->head];
					Edge *e = create_edge(t,h).first;
					*static_cast<EdgeDatum*>(e->dat) = *static_cast<EdgeDatum*>((*ei)->dat);
					e->idat = (*ei)->idat;
				}
		}
		return *this;
	}
	LGraph &operator=(LGraph &g) {
		clear();
		*this += g;
		*dat = *g.dat;
		return *this;
	}

};
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
        LGraph<GD,ND,ED,GID,NID,EID>::edge_iter i;
	for(i = n->outs.begin();i!=n->outs.end();++i)
		if(*i!=entered)
			dfs<GD,ND,ED,GID,NID,EID,Visitor>(*i,v,true);
	for(i = n->ins.begin(); i!=n->ins.end(); ++i)
		if(*i!=entered)
			dfs<GD,ND,ED,GID,NID,EID,Visitor>(*i,v,false);

}
		
#endif
