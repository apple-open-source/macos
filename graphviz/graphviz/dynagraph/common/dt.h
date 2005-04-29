/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef DT_H
#define DT_H

extern "C" {
#include "cdt.h"
}

namespace cdt {
inline Dtlink_t *v2l(void *vp) {
	return reinterpret_cast<Dtlink_t*>(vp);
}
struct initdtlink : Dtlink_t {
	initdtlink() { right = hl._left = 0; }
};
struct seqlink : initdtlink {};
struct treelink : initdtlink {};
struct seqtreelink : seqlink,treelink {};
template<typename Ty,typename L>
struct derived_accessor {
	typedef Ty T;
	Dtlink_t *operator [](T *o) {
		return static_cast<Dtlink_t*>(static_cast<L*>(o));
	}
	T *operator [](Dtlink_t *l) {
		return static_cast<T*>(static_cast<L*>(l));
	}
};
template<typename Accessor,typename Compare>
struct disc : Dtdisc_t {
	typedef Accessor A;
	typedef typename Accessor::T T;
	typedef disc<Accessor,Compare> my_type;
	Accessor m_acc;
	Compare m_compare;
	static int compareFunction(Dt_t *d,Void_t *o1,Void_t *o2,Dtdisc_t *dsc) {
		my_type *This = static_cast<my_type*>(dsc);
		return This->m_compare(This->m_acc[v2l(o1)],This->m_acc[v2l(o2)]);
	}
	disc(Accessor acc = Accessor(),Compare compare = Compare()) : m_acc(acc),m_compare(compare) {
		memset(static_cast<Dtdisc_t*>(this),0,sizeof(Dtdisc_t));
		comparf = compareFunction;
	}
};
template<typename Accessor>
struct sequence {
	typedef typename Accessor::T T;
	Accessor m_acc;
	Dtlink_t *m_left,*m_right;
	sequence(Dtlink_t *left = 0,Dtlink_t *right = 0,Accessor acc = Accessor()) : m_acc(acc),m_left(left),m_right(right) {}
	struct iterator {
		Accessor m_acc;
		Dtlink_t *m_link;
		iterator() {
			m_link = 0;
		}
		iterator(Accessor acc,Dtlink_t *link) : m_acc(acc),m_link(link) {}
		T *operator *() {
			return m_acc[m_link];
		}
		iterator &operator++() {
			m_link = m_link->right;
			return *this;
		}
		iterator &operator--() {
			m_link = m_link->hl._left;
			return *this;
		}
		iterator operator++(int) {
			iterator ret = *this;
			operator++();
			return ret;
		}
		iterator operator--(int) {
			iterator ret = *this;
			operator--();
			return ret;
		}
		iterator &operator =(const iterator &i) {
			m_link = i.m_link;
			m_acc = i.m_acc;
			return *this;
		}
		bool operator ==(const iterator &i) {
			return m_link==i.m_link;
		}
		bool operator !=(const iterator &i) {
			return !(*this==i);
		}
	};
	struct reverse_iterator {
		iterator i;
		reverse_iterator(Accessor acc,T *o) : i(acc,o) {}
		T *operator *() {
			return *i;
		}
		reverse_iterator &operator++() {
			--i;
			return *this;
		}
		// ...
	};
	iterator make_iter(Dtlink_t *link) {
		return iterator(m_acc,link);
	}
	iterator iter(T *o) {
		if(!o)
			return end();
		return make_iter(m_acc[o]);
	}
	iterator rev_iter(Dtlink_t *link) {
		return reverse_iterator(m_acc,link);
	}
	iterator begin() {
		return make_iter(m_left);
	}
	iterator end() {
		return make_iter((Dtlink_t*)0);
	}
	reverse_iterator rbegin() {
		return rev_iter(m_right);
	}
	reverse_iterator rend() {
		return rev_iter(0);
	}
	void insert(iterator i,T *x) {
		Dtlink_t *link = m_acc[x];
		if(i==end()) {
			if(m_right) {
				link->hl._left = m_right;
				m_right->right = link;
			}
			m_right = link;
			if(!m_left)
				m_left = m_right;
		}
		else {
			Dtlink_t *ll = i.m_link->hl._left;
			if(ll)
				ll->right = link;
			else
				m_left = link;
			link->hl._left = ll;
			link->right = i.m_link;
			i.m_link->hl._left = link;
		}
	}
	void erase(iterator i) {
		Dtlink_t *link = i.m_link;
		if(link->right)
			link->right->hl._left = link->hl._left;
		else
			if(!(m_right = link->hl._left))
				m_left = 0;
		if(link->hl._left)
			link->hl._left->right = link->right;
		else
			if(!(m_left = link->right))
				m_right = 0;
		link->hl._left = link->right = 0;
	}
	bool empty() {
		return !m_left;
	}
};
template<typename Disc>
struct tree_dict {
	Dt_t *m_dict;
	tree_dict(Disc &disc) {
		m_dict = dtopen(&disc,Dtoset);
	}
	operator Dt_t *() {
		return m_dict;
	}
};
		
template<typename Disc,bool shareDict = true>
struct tree {
	typedef Disc D;
	typedef typename Disc::T T;
	typedef typename Disc::A A; 
	A m_acc;
	Dt_t *m_dict;
	Dtlink_t *m_root;
	int m_size;
	tree(Disc &disc,Dt_t *dict) : m_acc(disc.m_acc),m_dict(dict),m_root(0),m_size(0) {}
	void restore() {
		if(shareDict) {
			dtrestore(m_dict,m_root);
		}
		m_dict->data->size = m_size;
	}
	void extract() {
		m_size = m_dict->data->size;
		if(shareDict) {
			m_root = v2l(dtextract(m_dict));
		}
	}
	// heavy tree iterator
	struct iterator {
		tree *m_tree;
		Dtlink_t *m_link;
		iterator() : m_tree(0),m_link(0) {}
		iterator(tree *t,Dtlink_t *l) : m_tree(t),m_link(l) {}
		T *operator *() {
			return m_tree->m_acc[m_link];
		}
		iterator &operator =(const iterator &i) {
			m_link = i.m_link;
			m_tree = i.m_tree;
			return *this;
		}
		iterator &operator++() {
			m_tree->restore();
			m_link = v2l(dtnext(m_tree->m_dict,m_link));
			m_tree->extract();
			return *this;
		}
		iterator &operator--() {
			m_tree->restore();
			m_link = v2l(dtprev(m_tree->m_dict,m_link));
			m_tree->extract();
			return *this;
		}
		bool operator ==(const iterator &i) {
			return m_link == i.m_link;
		}
		bool operator!=(const iterator &i) {
			return !(*this==i);
		}
		// ...
	};
	iterator iter(T *o) {
		return iterator(this,m_acc[o]);
	}
	iterator begin() {
		restore();
		iterator ret(this,v2l(dtfirst(m_dict)));
		extract();
		return ret;
	}
	iterator end() {
		return iterator(this,0);
	}
	iterator insert(T *o) { 
		Dtlink_t *link = m_acc[o];
		restore();
		Dtlink_t *link2 = reinterpret_cast<Dtlink_t*>(dtinsert(m_dict,link));
		T *r = m_acc[link2];
		extract();
		return iter(r);
	}
	bool erase(T *o) {
		restore();
		bool ret = dtdelete(m_dict,m_acc[o])!=0;
		extract();
		return ret;
	}
	iterator find(T *o) {
		restore();
		Dtlink_t *s = v2l(dtsearch(m_dict,m_acc[o]));
		extract();
		return iterator(this,s);
	}
	int size() {
		return m_size;
	}
	bool empty() {
		return !size();
	}
};
template<typename Tree,typename Sequence>
struct ordering : Tree,Sequence {
	typedef typename Tree::D D;
	typedef typename Tree::T T;
	typedef typename Sequence::iterator iterator;
	typedef typename Sequence::reverse_iterator reverse_iterator;
	ordering(D &disc,Dt_t *dict) : Tree(disc,dict) {}
	bool empty() {
		return Tree::empty();
	}
	int size() {
		return Tree::size();
	}
	iterator iter(T *o) {
		return Sequence::iter(o);
	}
	iterator begin() {
		return Sequence::begin();
	}
	iterator end() {
		return Sequence::end();
	}
	// kind of anomalous method for cdt.
	iterator find(T *o) {
		typename Tree::iterator i = Tree::find(o); // could also just look at Dtlink_t with Tree::m_acc[*o] !
		if(i!=Tree::end())
			return Sequence::iter(*i);
		else
			return end();
	}
	std::pair<iterator,bool> insert(T *o) {
		typename Tree::iterator i = Tree::insert(o);
		if(i==Tree::iter(o)) {
			// insert into sequence before whatever is next in tree
			typename Tree::iterator j = i;
			Sequence::insert(Sequence::iter(*++j),o);
			return std::make_pair(Sequence::iter(*i),true);
		}
		else
			return std::make_pair(Sequence::iter(*i),false);
	}
	void erase(T *o) {
		if(Tree::erase(o))
			Sequence::erase(Sequence::iter(o));
	}
	void erase(iterator i) {
		return erase(*i);
	}
};

};
#endif // DT_H
