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

/*
 * Circular layout. Biconnected components are put on circles.
 * block-cutnode tree is done recursively, with children placed
 * about parent block.
 * Based on:
 *   Six and Tollis, "A Framework for Circular Drawings of
 * Networks", GD '99, LNCS 1731, pp. 107-116;
 *   Six and Tollis, "Circular Drawings of Biconnected Graphs",
 * Proc. ALENEX '99, pp 57-73.
 *   Kaufmann and Wiese, "Maintaining the Mental Map for Circular
 * Drawings", GD '02, LNCS 2528, pp. 12-22.
 */

#include	<string.h>
#include    "circular.h"
#include    "adjust.h"
#include    "pack.h"
#include    "neatoprocs.h"

void 
circular_nodesize(node_t* n, boolean flip)
{
  int         w;

  w = ND_xsize(n) = POINTS(ND_width(n));
  ND_lw_i(n)  = ND_rw_i(n) = w / 2;
  ND_ht_i(n) = ND_ysize(n) = POINTS(ND_height(n));
}


static void 
circular_init_node (node_t* n)
{
  common_init_node(n);

  circular_nodesize(n,GD_left_to_right(n->graph));
  ND_pos(n) = N_NEW(GD_ndim(n->graph),double);
  ND_alg(n) = NEW(ndata);
}


static void 
circular_initPort(node_t* n, edge_t* e, char* name)
{
	port  port;
	GVC_t *gvc = GD_gvc(n->graph->root);

	gvc->n = n;

	if (name == NULL) return;
	port = ND_shape(n)->fns->portfn(gvc,name);
		
	#ifdef NOTDEF
		if(n->GD_left_to_right(graph)) 
			port.p = invflip_pt(port.p);
	#endif

	port.order = 0;
	if (e->tail == n) 
		ED_tail_port(e) = port; 
	else 
		ED_head_port(e) = port;
}


static void
circular_init_edge (edge_t* e)
{
	common_init_edge(e);

	ED_factor(e) = late_double(e,E_weight,1.0,0.0);

	circular_initPort(e->tail,e,agget(e,"tailport"));
	circular_initPort(e->head,e,agget(e,"headport"));
}


static void
circular_init_node_edge(graph_t *g)
{
	node_t* n;
	edge_t* e;
	int     i = 0;

	GD_neato_nlist(g) = N_NEW(agnnodes(g) + 1,node_t*);
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		GD_neato_nlist(g)[i++] = n;
		circular_init_node(n);
	}
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
			circular_init_edge(e);
		}
	}
}


void
circular_init_graph(graph_t *g)
{
  UseRankdir = FALSE;
  graph_init(g);
  GD_drawing(g)->engine = CIRCULAR;
  /* GD_ndim(g) = late_int(g,agfindattr(g,"dim"),2,2); */
  Ndim = GD_ndim(g) = 2;  /* The algorithm only makes sense in 2D */
  circular_init_node_edge(g);
}

static node_t*
makeDerivedNode (graph_t* dg, char* name, int isNode, void* orig)
{
	node_t* n = agnode(dg, name);		
	ND_alg(n) = (void*)NEW(cdata);
	if (isNode) {
		ND_pos(n) = N_NEW(Ndim, double);
		ORIGN(n) = (node_t*)orig;
	}
	else
		ORIGG(n) = (graph_t*)orig;
	return n;
}

/* circomps:
 * Construct a strict, undirected graph with no loops from g.
 * Construct the connected components with the provision that all
 * nodes in a block subgraph are considered connected.
 * Return array of components with number of components in cnt.
 * Each component has its blocks as subgraphs.
 * FIX: Check that blocks are disjoint.
 */
Agraph_t**
circomps(Agraph_t* g, int* cnt)
{
	int			c_cnt;
	Agraph_t**	ccs;
	Agraph_t*   dg;
	Agnode_t	*n, *v, *dt, *dh;
	Agedge_t*   e;
	Agraph_t*   sg;
	int			i;
	Agedge_t*   ep;
	Agnode_t*   p;
#ifdef USER_BLOCKS
	Agraph_t	*ssg, *ssgl, *subg;
	Agnode_t*   t;
	Agedge_t*   me;
#endif

	dg = agopen("derived", AGFLAG_STRICT);
#ifdef USER_BLOCKS
	sg = g->meta_node->graph;	
	for(me = agfstout(sg, g->meta_node); me; me = agnxtout(sg, me)) {
		subg = agusergraph(me->head);

		if(strncmp(subg->name, "block", 5) != 0)
			continue;

		if (agnnodes(subg) == 0) continue;

		n = makeDerivedNode(dg, subg->name, 0, subg);		
		for(v = agfstnode(subg); v; v = agnxtnode(subg, v)) {
			DNODE(v) = n;
		}
	}
#endif

	for(v = agfstnode(g); v; v = agnxtnode(g, v)) {
		if(DNODE(v)) continue;
		n = makeDerivedNode(dg, v->name, 1, v);		
		DNODE(v) = n;
	}

	for(v = agfstnode(g); v; v = agnxtnode(g, v)) {
		for(e = agfstout(g, v); e; e = agnxtout(g, e)) {
			dt = DNODE(e->tail);
			dh = DNODE(e->head);
			if(dt != dh) agedge(dg, dt, dh);
		}
	}

	ccs = ccomps(dg, &c_cnt, 0);

		/* replace block nodes with block contents */
	for(i = 0; i < c_cnt; i++) {
		sg = ccs[i];

#ifdef USER_BLOCKS
		for(n = agfstnode(sg); n; n = agnxtnode(sg, n)) {
			/* Expand block nodes, and create block subgraph in sg */
			if(agobjkind(ORIGN(n)) != AGNODE) {
				ssg = ORIGG(n);
				free (ND_alg(n));
				agdelete (n->graph, n);
				ssgl = agsubg(sg, ssg->name);
				for(t = agfstnode(ssg); t; t = agnxtnode(ssg, t)) {
					p = makeDerivedNode (dg, t->name, 1, t);
					DNODE(t) = p;
					aginsert(ssgl, p);
				}
			}
		}
#endif

		/* add edges: since sg is a union of components, all edges
         * of any node should be added, except loops.
         */
		for(n = agfstnode(sg); n; n = agnxtnode(sg, n)) {
			p = ORIGN(n); 
			for(e = agfstout(g, p); e; e = agnxtout(g, e)) {
				/* n = DNODE(e->tail); by construction since e->tail == p */
				dh = DNODE(e->head);
				if(n != dh) {
					ep = agedge (dg, n, dh);
					aginsert(sg, ep);
				}
			}
		}
	}

		/* Finally, add edge data to edges */
	for(n = agfstnode(dg); n; n = agnxtnode(dg, n)) {
		for(e = agfstout(dg, n); e; e = agnxtout(dg, e)) {
			ED_alg(e) = NEW(edata);
		}
	}

	*cnt = c_cnt;
	return ccs;
}

/* closeDerivedGraph:
 */
static void
closeDerivedGraph (graph_t* g)
{
	node_t*   n;
	edge_t*   e;

	for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
		for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
			free (ED_alg(e));
		}
		free (ND_alg(n));
		free (ND_pos(n));
	}
	agclose (g);
}

/* copyPosns:
 * Copy position of nodes in given subgraph of derived graph
 * to corresponding node in original graph.
 * FIX: consider assigning from n directly to ORIG(n).
 */
static void
copyPosns (graph_t* g)
{
	node_t*   n;
	node_t*   v;

	for(n = agfstnode(g); n; n = agnxtnode(g, n)) {
		v = ORIGN(n);
		ND_pos(v)[0] = ND_pos(n)[0];
		ND_pos(v)[1] = ND_pos(n)[1];
	}
}

/* circular_layout:
 */
void
circular_layout(Agraph_t* g)
{

	Agraph_t** ccs;
	Agraph_t*  sg;
	int        ncc;
	int        i;

	circular_init_graph(g);
	if (agnnodes(g)) {
		ccs = circomps(g, &ncc);
	
		if (ncc == 1) {
			circularLayout(ccs[0]);
			copyPosns (ccs[0]);
			/* FIX: adjustNodes(g); */
			spline_edges(g);
		}
		else {
			pack_info pinfo;
			pack_mode pmode = getPackMode (g,l_node);
	
			for (i = 0; i < ncc; i++) {
				sg = ccs[i];
				circularLayout(sg);
				copyPosns (sg);
				/* FIX: adjustNodes(g); */
			}
			spline_edges(g);
			pinfo.margin = getPack (g, CL_OFFSET, CL_OFFSET);
			pinfo.doSplines = 1;
			pinfo.mode = pmode;
			pinfo.fixed = 0;
			packSubgraphs (ncc, ccs, g, &pinfo);
		}
	}

	dotneato_postprocess(g, circular_nodesize);
}

static void
circular_cleanup_node(node_t* n)
{
	GVC_t *gvc = GD_gvc(n->graph->root);

	gvc->n = n;

	free(ND_alg(n));
	free(ND_pos(n));
	if (ND_shape(n))
		ND_shape(n)->fns->freefn(gvc);
	free_label(ND_label(n));
	memset(&(n->u),0,sizeof(Agnodeinfo_t));
}

static void
circular_free_splines(edge_t* e)
{
	int		i;
	if (ED_spl(e)) {
		for (i = 0; i < ED_spl(e)->size; i++)
		       	free(ED_spl(e)->list[i].list);
		free(ED_spl(e)->list);
		free(ED_spl(e));
	}
	ED_spl(e) = NULL;
}

static void
circular_cleanup_edge(edge_t* e)
{
	circular_free_splines(e);
	free_label(ED_label(e));
	memset(&(e->u),0,sizeof(Agedgeinfo_t));
}

static void
circular_cleanup_graph(graph_t* g)
{
	free(GD_neato_nlist(g));
	free_ugraph(g);
	free_label(GD_label(g));
	memset(&(g->u),0,sizeof(Agraphinfo_t));
}

void
circular_cleanup(graph_t* g)
{
	node_t  *n;
	edge_t  *e;

	n = agfstnode(g);
	if (n == NULL) return;  /* g is empty */
	
	closeDerivedGraph (DNODE(n)->graph);  /* delete derived graph */

	for (; n; n = agnxtnode(g, n)) {
		for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
			circular_cleanup_edge(e);
		}
		circular_cleanup_node(n);
	}
	circular_cleanup_graph(g);
}
