/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef freelist_h
#define freelist_h

#define COUNT_ALLOCATED
template<typename T>
struct Freelist {
	T *alloc() {
		if(!head) {
			int amount = sizeof(Blockheader) + blocksize * sizeof(T);
#ifdef COUNT_ALLOCATED
			allocated+=amount;
#endif
			Blockheader *mem = reinterpret_cast<Blockheader*>(malloc(amount));
			char *cp = reinterpret_cast<char*>(mem) + sizeof(Blockheader);
			for(int i=0; i<blocksize; i++)
				free(reinterpret_cast<T*>(cp) + i);
			mem->next = blocks;
			blocks = mem;
		}
		Freenode *t = head;
		head = t->nextfree;
		new(t) T;
		return reinterpret_cast<T*>(t);
	}
	void free(T *x) {
		Freenode *f = reinterpret_cast<Freenode*>(x);
		f->nextfree = head;
		head = f;
	}
	void clear() {
		head = 0;
		if(blocks) 
			for(Blockheader *bp = blocks,*np; bp; bp = np) {
				np = bp->next;
				::free(bp);
			}
		blocks = 0;
#ifdef COUNT_ALLOCATED
		allocated=0;
#endif
	}
	Freelist(int blocksize) : head(0),blocksize(blocksize),blocks(0) {
#ifdef COUNT_ALLOCATED
		allocated=0;
#endif
	}
	~Freelist() { clear(); }
protected:
	struct Freenode {
		Freenode *nextfree;
	};

	struct Blockheader {
		Blockheader *next;
	};

	Freenode *head; // list of free nodes
	int blocksize; // number of nodes per block
	Blockheader *blocks; // list of blocks
#ifdef COUNT_ALLOCATED
public:
	int allocated;
#endif
};
#endif // freelist_h
