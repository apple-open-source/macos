#ifndef FDP_H
#define FDP_H
#include <render.h>

#ifdef FDP_PRIVATE

#ifndef NOTUSED
#define NOTUSED(var)      (void) var
#endif

#define NDIM     2

typedef struct {
    pointf       LL,UR;
} boxf;

typedef struct bport_s {
  edge_t*         e;
  node_t*         n;
  double          alpha;
} bport_t;

/* gdata is attached to each cluster graph, 
 * and to each derived graph.
 * Graphs also use "builtin" fields:
 *   n_cluster, clust - to record clusters  
 */
typedef struct {
  bport_t*   ports;          /* boundary ports. 0-terminated */
  int        nports;         /* no. of ports */
  boxf       bb;             /* bounding box of graph */
  point      delta;          /* offset of bb from original */
  int        flags;
} gdata;

#define GDATA(g)    ((gdata*)(GD_alg(g)))
#define BB(g)       (GDATA(g)->bb)
#define PORTS(g)    (GDATA(g)->ports)
#define NPORTS(g)   (GDATA(g)->nports)
#define DELTA(g)    (GDATA(g)->delta)

/* ndata is attached to nodes in real graphs.
 * Real nodes also use "builtin" fields:
 *   pos   - position information
 *   width,height     - node dimensions
 *   xsize,ysize      - node dimensions in points
 */
typedef struct {
  node_t*  dn;                  /* points to corresponding derived node,
                                 * which may represent the node or its
                                 * containing cluster. */
  graph_t* parent;              /* smallest containing cluster */
} ndata;

#define NDATA(n) ((ndata*)(ND_alg(n)))
#define DNODE(n) (NDATA(n)->dn)
#define PARENT(n) (NDATA(n)->parent)

/* dndata is attached to nodes in derived graphs.
 * Derived nodes also use "builtin" fields:
 *   clust - for cluster nodes, points to cluster in real graph.
 *   pos   - position information
 *   width,height     - node dimensions
 */
typedef struct {
  int      deg;                 /* degree of node */
  int      wdeg;                /* weighted degree of node */
  node_t*  dn;                  /* If derived node is not a cluster, */
                                /* dn points real node. */
  double   disp[NDIM];          /* incremental displacement */
} dndata;

#define DNDATA(n) ((dndata*)(ND_alg(n)))
#define DISP(n) (DNDATA(n)->disp)
#define ANODE(n) (DNDATA(n)->dn)
#define DEG(n) (DNDATA(n)->deg)
#define WDEG(n) (DNDATA(n)->wdeg)
#define IS_PORT(n) (!ANODE(n) && !ND_clust(n))  /* see fdp_isPort */

#endif /*  FDP_PRIVATE */

extern void fdp_layout(Agraph_t* g);
extern void fdp_nodesize(node_t*, boolean);
extern void fdp_init_graph (Agraph_t *g);
extern void fdp_init_node_edge(Agraph_t* g);
extern void fdp_cleanup(Agraph_t* g);

extern int  fdp_isPort (Agnode_t*);

#endif
