#ifndef BLOCKPATH_H
#define BLOCKPATH_H

#include <circular.h>

extern nodelist_t* layout_block(Agraph_t* g, block_t* sn, double);
extern int cmpNodeDegree(Dt_t*, Agnode_t**, Agnode_t**, Dtdisc_t*);

#ifdef DEBUG
extern void prTree (Agraph_t* g);
#endif

#endif
