#ifndef CIRCLE_H
#define CIRCLE_H
#include <render.h>

typedef struct {
  int      nStepsToLeaf;
  int      subtreeSize;
  int      nChildren;
  int      nStepsToCenter;
  node_t*  parent;
  double   span;
  double   theta;
} rdata;

#define RDATA(n) ((rdata*)((n)->u.alg))
#define SLEAF(n) (RDATA(n)->nStepsToLeaf)
#define STSIZE(n) (RDATA(n)->subtreeSize)
#define NCHILD(n) (RDATA(n)->nChildren)
#define SCENTER(n) (RDATA(n)->nStepsToCenter)
#define SPARENT(n) (RDATA(n)->parent)
#define SPAN(n) (RDATA(n)->span)
#define THETA(n) (RDATA(n)->theta)

extern void circleLayout(Agraph_t* sg, Agnode_t* center);
extern void twopi_layout(Agraph_t* g);
extern void twopi_cleanup(Agraph_t* g);
extern void twopi_nodesize(node_t* n, boolean flip);
extern void twopi_init_graph(graph_t *g);
#endif
