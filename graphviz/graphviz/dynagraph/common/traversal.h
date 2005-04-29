/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

// so as not to mess with callbacks, this is iterators returning *both* node and edge
// the edge was followed to get to the node, unless the traversal didn't follow an edge to get there.

#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include <stack>
#include <queue>
#include <bitset>
#include <assert.h>

// a traversal is an expensive iterator with bool stop() instead of an end() iterator
// its operator*() returns both a Node* and an Edge*

// embedded tag for simultaneous traversals
#define MAX_TRAVERSAL sizeof(unsigned long)*8 // this is the min for bitset
typedef std::bitset<MAX_TRAVERSAL> hitflags;
typedef hitflags Hit;
template<class G>
struct EN {
	typedef typename G::Node Node;
	typedef typename G::Edge Edge;
	Edge *e;
	Node *n;
	EN(Edge *e=0,Node *n=0) : e(e),n(n) {}
};

template<class G>
struct Traversal {
	Traversal(G *g) : m_g(g) {
		resetAll();
		m_hitpos = findHitpos();
	}
	~Traversal() {
		resetAll();
		gd<Hit>(m_g)[m_hitpos] = 0;
	}
protected:
	G *m_g;
	int m_hitpos;
	void resetAll() {
		for(typename G::node_iter ni = m_g->nodes().begin(); ni!=m_g->nodes().end();++ni)
			gd<Hit>(*ni)[m_hitpos] = false;
		for(typename G::graphedge_iter ei = m_g->edges().begin(); ei!=m_g->edges().end();++ei)
			gd<Hit>(*ei)[m_hitpos] = false;
	}
	int findHitpos() {
		unsigned hitpos;
		for(hitpos = 0;hitpos<MAX_TRAVERSAL;++hitpos)
			if(!gd<Hit>(m_g)[hitpos])
				break;
		assert(hitpos<MAX_TRAVERSAL);
		gd<Hit>(m_g)[hitpos] = true;
		return hitpos;
	}
};


template<class G>
struct DFS : Traversal<G> {
	typedef EN<G> V;
	typedef typename G::Node Node;
	typedef typename G::Edge Edge;
	V operator *() {
		return m_curr;
	}
	DFS &operator++() {
		if(m_curr.e)
			gd<Hit>(m_curr.e)[m_hitpos] = true;
		else {
			assert(m_curr.n);
			gd<Hit>(m_curr.n)[m_hitpos] = true;
		}
		// try edges
		if(m_curr.n && follow())
			return *this;
		// try popping stack
		if(restore())
			return *this;
		// look for a new tree
		if(next())
			return *this;
		// nope
		m_stop = true;
		return *this;
	}
	// no postfix ++ because that requires copying the iterator
	bool stop() {
		return m_stop;
	}
	DFS(G *g,typename G::Node *start=0,bool outward=true,bool inward=true,bool outsfirst=true) : 
		Traversal<G>(g),m_outward(outward),m_inward(inward),m_outsfirst(outsfirst),m_stop(false)
	{
		m_nodeiter = start?g->iter(start):g->nodes().begin();
		if(!next())
			m_stop = true;
	}
private:
	bool m_outward,m_inward,m_outsfirst,m_stop;
	typedef std::stack<V> ne_stack;
	ne_stack m_stack;
	V m_curr;
	typename G::node_iter m_nodeiter;
	bool follow(Edge *deadpath=0) {
		typename G::inedge_iter startIn = m_curr.n->ins().begin();
		typename G::outedge_iter startOut = m_curr.n->outs().begin();
		if(deadpath) { // popped off stack
			if(deadpath->tail==m_curr.n) {
				++(startOut = m_curr.n->outs().iter(deadpath));
			}
			else {
				assert(deadpath->head==m_curr.n);
				++(startIn = m_curr.n->ins().iter(deadpath));
			}
		}
		if(m_outsfirst) {
			if(startIn==m_curr.n->ins().begin())
				if(outs(startOut))
					return true;
			if(ins(startIn))
				return true;
		}
		else {
			if(startOut==m_curr.n->outs().begin())
				if(ins(startIn))
					return true;
			if(outs(startOut))
				return true;
		}
		return false;
	}
	bool outs(typename G::outedge_iter start) {
		for(typename G::outedge_iter ei = start; ei!=m_curr.n->outs().end(); ++ei)
			if(!gd<Hit>(*ei)[m_hitpos]) {
				m_stack.push(m_curr);
				m_curr.e = *ei;
				if(!gd<Hit>(m_curr.e->head)[m_hitpos])
					m_curr.n = m_curr.e->head;
				else
					m_curr.n = 0;
				return true;
			}
		return false;
	}
	bool ins(typename G::inedge_iter start) {
		for(typename G::inedge_iter ei = start; ei!=m_curr.n->ins().end(); ++ei)
			if(!gd<Hit>(*ei)[m_hitpos]) {
				m_stack.push(m_curr);
				m_curr.e = *ei;
				if(!gd<Hit>(m_curr.e->tail)[m_hitpos])
					m_curr.n = m_curr.e->tail;
				else
					m_curr.n = 0;
				return true;
			}
		return false;
	}
	bool restore() {
		Edge *deadpath;
		do {
			if(m_stack.empty())
				return false;
			deadpath = m_curr.e;
			m_curr = m_stack.top();
			m_stack.pop();
		}
		while(!follow(deadpath)); 
		return true;
	}
	bool next() {
		for(;m_nodeiter!=m_g->parent->nodes().end();++m_nodeiter) 
			if(!gd<Hit>(*m_nodeiter)[m_hitpos]) {
				m_curr.e = 0;
				m_curr.n = *m_nodeiter;
				m_nodeiter++;
				return true;
			}
		return false;
	}
};
template<class G>
struct BFS : Traversal<G> {
	typedef EN<G> V;
	typedef typename G::Node Node;
	typedef typename G::Edge Edge;
	V operator *() {
		assert(!m_queue.empty());
		return m_queue.front();
	}
	BFS &operator++() {
		V last = **this;
		m_queue.pop();
		if(last.n) {
			if(m_inwards) 
				for(typename G::inedge_iter ei = last.n->ins().begin(); ei!=last.n->ins().end(); ++ei)
					if(!gd<Hit>(*ei)[m_hitpos]) {
						Node *t = (*ei)->tail;
						if(gd<Hit>(t)[m_hitpos])
							t = 0;
						else
							gd<Hit>(t)[m_hitpos] = true;
						m_queue.push(V(*ei,t));
						gd<Hit>(*ei)[m_hitpos] = true;
					}
			if(m_outwards) 
				for(typename G::outedge_iter ei = last.n->outs().begin(); ei!=last.n->outs().end(); ++ei)
					if(!gd<Hit>(*ei)[m_hitpos]) {
						Node *h = (*ei)->head;
						if(gd<Hit>(h)[m_hitpos])
							h = 0;
						else
							gd<Hit>(h)[m_hitpos] = true;
						m_queue.push(V(*ei,h));
						gd<Hit>(*ei)[m_hitpos] = true;
					}
		}
		if(m_queue.empty())
			for(;m_nodeiter!=m_g->nodes().end(); ++m_nodeiter)
				if(!gd<Hit>(*m_nodeiter)[m_hitpos]) {
					gd<Hit>(*m_nodeiter)[m_hitpos] = true;
					m_queue.push(V(0,*m_nodeiter++));
					break;
				}
		return *this;
	}
	bool stop() {
		return m_queue.empty();
	}
	BFS(G *g,typename G::Node *start = 0,bool inwards=true,bool outwards=true) : Traversal<G>(g),m_inwards(inwards),m_outwards(outwards) {
		m_nodeiter = start?g->iter(start):g->nodes().begin();
		if(m_nodeiter!=g->nodes().end()) {
			gd<Hit>(*m_nodeiter)[m_hitpos] = true;
			m_queue.push(V(0,*m_nodeiter++));
		}
	}
private:
	bool m_inwards,m_outwards;
	typedef std::queue<V> ne_queue;
	ne_queue m_queue;
	typename G::node_iter m_nodeiter;

};

#endif // TRAVERSAL_H
