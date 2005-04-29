#ifndef CIRCO_H
#define CIRCO_H

#include <render.h>

extern void circular_layout(Agraph_t *g);
extern void circularLayout(Agraph_t* sg);
extern void circular_cleanup(Agraph_t* g);
extern void circular_nodesize(node_t* n, boolean flip);
extern void circular_init_graph(graph_t *g);

#endif
