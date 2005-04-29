/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#pragma prototyped

#ifndef _PACK_H
#define _PACK_H 1

#include <render.h>

/* Type indicating granularity and method 
 *  l_undef  - unspecified
 *  l_node   - polyomino using nodes and edges
 *  l_clust  - polyomino using nodes and edges and top-level clusters
 *             (assumes ND_clust(n) unused by application)
 *  l_graph  - polyomino using graph bounding box
 *  l_hull   - polyomino using convex hull (unimplemented)
 *  l_tile   - tiling using graph bounding box (unimplemented)
 *  l_bisect - alternate bisection using graph bounding box (unimplemented)
 */
typedef enum { l_undef, l_clust, l_node, l_graph} pack_mode;

typedef struct {
#ifdef UNIMPLEMENTED
	float aspect;        /* desired aspect ratio */
#endif
	unsigned int margin; /* margin left around objects, in points */
	int doSplines;       /* use splines in constructing graph shape */
	pack_mode mode;      /* granularity and method */
	boolean* fixed;      /* fixed[i] == true implies g[i] should not be moved */
} pack_info;

extern point* putGraphs (int, Agraph_t**, Agraph_t*, pack_info*);
extern int shiftGraphs (int, Agraph_t**, point*, Agraph_t*, int);
extern int packGraphs (int, Agraph_t**, Agraph_t*, pack_info*);
extern int packSubgraphs (int, Agraph_t**, Agraph_t*, pack_info*);
extern pack_mode getPackMode (Agraph_t* g, pack_mode dflt);
extern int getPack (Agraph_t*, int not_def, int dflt);

extern int isConnected (Agraph_t*);
extern Agraph_t** ccomps (Agraph_t*, int*, char*);
extern Agraph_t** pccomps (Agraph_t*, int*, char*, boolean*);
extern int nodeInduce (Agraph_t*);

#endif

