#ifndef BLOCKTREE_H
#define BLOCKTREE_H

#include <render.h>
#include <circular.h>

extern block_t* createBlocktree(Agraph_t* g, circ_state* state);
extern void freeBlocktree(block_t*);
#ifdef DEBUG
extern void print_blocktree(block_t* sn, int depth);
#endif

#endif
