#ifndef FDPDBG_H
#define FDPDBG_H

#ifdef DEBUG

#include <fdp.h>
#include <stdio.h>
#include <graph.h>

extern double Scale;
extern void outputGraph (Agraph_t*,FILE*,int);

extern void incInd (void);
extern void decInd (void);
extern void prIndent (void);

extern void dump (graph_t* g, int doAll, int doBB);
extern void dumpE (graph_t* g, int derived);
extern void dumpG (graph_t* g, char* fname, int);

#endif

#endif
