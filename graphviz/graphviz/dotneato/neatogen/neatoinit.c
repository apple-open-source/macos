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

#include	"neato.h"
#include	"pack.h"


static int Pack;    /* If >= 0, layout components separately and pack together
                     * The value of Pack gives margins around graphs.
                     */
static char*  cc_pfx = "_neato_cc";

void neato_nodesize(node_t* n, boolean flip)
{
    int         w;

    w = ND_xsize(n) = POINTS(ND_width(n));
    ND_lw_i(n)  = ND_rw_i(n) = w / 2;
    ND_ht_i(n) = ND_ysize(n) = POINTS(ND_height(n));
}

void neato_init_node(node_t* n)
{
	common_init_node(n);
	ND_pos(n) = ALLOC(GD_ndim(n->graph),0,double);
	neato_nodesize(n,GD_left_to_right(n->graph));
}

void neato_init_edge(edge_t* e)
{
	common_init_edge(e);

	ED_factor(e) = late_double(e,E_weight,1.0,1.0);

	init_port(e->tail,e,agget(e,"tailport"), FALSE);
	init_port(e->head,e,agget(e,"headport"), TRUE);
}

void neato_init_node_edge(graph_t *g)
{
	node_t *n;
	edge_t *e;
    int         nG = agnnodes(g);
	attrsym_t*  N_pos = agfindattr(g->proto->n,"pos");

	N_pin = agfindattr(g->proto->n,"pin");  /* used in user_pos */

    for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		neato_init_node(n);
		user_pos(N_pos,n,nG);  /* set user position if given */
    }
    for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
        for (e = agfstout(g,n); e; e = agnxtout(g,e)) neato_init_edge(e);
    }
}

int init_port(node_t* n, edge_t* e, char* name, boolean isHead)
{
	port	p0;
	GVC_t *gvc;

	if (!name[0]) return FALSE;
	gvc = GD_gvc(n->graph->root);
	gvc->n = n; gvc->e = e;
	ND_has_port(n) = TRUE;
	p0 = ND_shape(n)->fns->portfn(gvc,name);
#ifdef NOTDEF
	if (n->GD_left_to_right(graph)) p0.p = invflip_pt(p0.p);
#endif 
	p0.order = 0;
	if (isHead) ED_head_port(e) = p0;
	else ED_tail_port(e) = p0;
	return TRUE;
}

void neato_cleanup_node(node_t* n)
{
	GVC_t *gvc;

	if (ND_shape(n)) {
		gvc = GD_gvc(n->graph->root);
		gvc->n = n;
		ND_shape(n)->fns->freefn(gvc);
	}
	free (ND_pos(n));
	free_label(ND_label(n));
	memset(&(n->u),0,sizeof(Agnodeinfo_t));
}

void neato_free_splines(edge_t* e)
{
	int		i;
	if (ED_spl(e)) {
		for (i = 0; i < ED_spl(e)->size; i++) free(ED_spl(e)->list[i].list);
		free(ED_spl(e)->list);
		free(ED_spl(e));
	}
	ED_spl(e) = NULL;
}

void neato_cleanup_edge(edge_t* e)
{
	neato_free_splines(e);
	free_label(ED_label(e));
	memset(&(e->u),0,sizeof(Agedgeinfo_t));
}

void neato_cleanup_graph(graph_t* g)
{
	if (Nop || (Pack < 0)) free_scan_graph(g);
	else {
		Agraph_t* mg;
		Agedge_t* me;
		Agnode_t* mn;
		Agraph_t* subg;
        int       slen = strlen (cc_pfx);

		mg = g->meta_node->graph;
		for (me = agfstout(mg,g->meta_node); me; me = agnxtout(mg,me)) {
			mn = me->head;
			subg = agusergraph(mn);
			if (strncmp(subg->name,cc_pfx,slen) == 0)
              free_scan_graph (subg);
		}
	}
	free_ugraph(g);
	free_label(GD_label(g));
	memset(&(g->u),0,sizeof(Agraphinfo_t));
}

void neato_cleanup(graph_t* g)
{
	node_t  *n;
	edge_t  *e;

	for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
		for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
			neato_cleanup_edge(e);
		}
		neato_cleanup_node(n);
	}
	neato_cleanup_graph(g);
}

static int
numFields (char* pos)
{
	int    cnt = 0;
	char   c;

	do {
		while (isspace(*pos)) pos++;   /* skip white space */
		cnt++;
		while ((c = *pos) && !isspace(c) && (c != ';')) pos++;  /* skip token */
	} while (isspace(c));
	return cnt;
}

static void
set_elabel (edge_t* e, textlabel_t* l, char* name)
{
    double   x,y;
	point  pt;
	char   *lp;
	lp = agget(e,name);
	if (lp && (sscanf(lp,"%lf,%lf",&x,&y) == 2)) {
       	pt.x = (int)(x);
       	pt.y = (int)(y);
		l->p = pt;
		l->set = TRUE;
	}
}

/* user_spline:
 * Attempt to use already existing pos info for spline
 * Return 1 if successful, 0 otherwise.
 * Assume E_pos != NULL and ED_spl(e) == NULL.
 */
static int
user_spline (attrsym_t* E_pos, edge_t* e)
{
    char*    pos;
    int      i, n, npts, nc;
    point*   ps = 0;
    point*   pp;
    double   x,y;
    int      sflag = 0, eflag = 0;
    point    sp, ep;
	bezier*  newspl;
	int      more = 1;
    int      stype, etype;

    pos = agxget(e,E_pos->index);
    if (*pos == '\0') return 0;
    
    arrow_flags (e, &stype, &etype);
	do {
	      /* check for s head */
	    i = sscanf(pos,"s,%lf,%lf%n",&x,&y,&nc);
	    if (i == 2) {
	      sflag = 1;
	      pos = pos+nc;
	      sp.x = (int)(x);
	      sp.y = (int)(y);
	    }
	
	      /* check for e head */
	    i = sscanf(pos," e,%lf,%lf%n",&x,&y,&nc);
	    if (i == 2) {
	      eflag = 1;
	      pos = pos+nc;
	      ep.x = (int)(x);
	      ep.y = (int)(y);
	    }
	
	    npts = numFields (pos);  /* count potential points */
	    n = npts;
	    if ((n < 4) || (n%3 != 1)) {
			neato_free_splines (e);
			return 0;
		}
	    ps = ALLOC(n,0,point);
	    pp = ps;
	    while (n) {
	      i = sscanf(pos,"%lf,%lf%n",&x,&y,&nc);
	      if (i < 2) {
	        free (ps);
			neato_free_splines (e);
	        return 0;
	      }
	      pos = pos+nc;
	      pp->x = (int)(x);
	      pp->y = (int)(y);
	      pp++;
	      n--;
	    }
		if (*pos == '\0') more = 0;
		else pos++;
	
	      /* parsed successfully; create spline */
		newspl = new_spline (e, npts);
	    if (sflag) {
	      newspl->sflag = stype;
	      newspl->sp = sp;
	    }
	    if (eflag) {
	      newspl->eflag = etype;
	      newspl->ep = ep;
	    }
	    for (i=0; i < npts; i++) {
	      newspl->list[i] = ps[i];
	    }
	    free (ps);
    } while (more);

	if (ED_label(e)) set_elabel (e, ED_label(e), "lp");
	if (ED_head_label(e)) set_elabel (e, ED_head_label(e), "head_lp");
	if (ED_tail_label(e)) set_elabel (e, ED_tail_label(e), "tail_lp");
	
    return 1;
}

/* Nop can be:
 *  0 - do full layout
 *  1 - assume initial node positions, do (optional) adjust and all splines
 *  2 - assume final node and edges positions, do nothing except compute
 *      missing splines
 */

 /* Indicates the amount of edges with position information */
typedef enum {NoEdges, SomeEdges, AllEdges} pos_edge;

/* nop_init_edges:
 * Check edges for position info.
 * If position info exists, check for edge label positions.
 * Return number of edges with position info.
 */
static pos_edge
nop_init_edges (Agraph_t *g)
{
	node_t*     n;
	edge_t*     e;
    int         nedges = 0;
	attrsym_t*  E_pos = agfindattr(g->proto->e,"pos");

    if (!E_pos || (Nop < 2)) return NoEdges;

	for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
		for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
			if (user_spline (E_pos, e)) {
				nedges++;
            }
		}
	}
	if (nedges) {
		if (nedges == agnedges(g)) return AllEdges;
		else return SomeEdges;
	}
	else return NoEdges;
}

/* chkBB:
 * Scans for a correct bb attribute. If available, sets it
 * in the graph and returns 1.
 */
#define BS "%d,%d,%d,%d"

static int
chkBB (Agraph_t *g, attrsym_t* G_bb)
{
	char*       s;
	box         bb;

	s = agxget (g, G_bb->index);
	if (sscanf (s,BS,&bb.LL.x,&bb.LL.y,&bb.UR.x,&bb.UR.y) == 4) {
		GD_bb(g) = bb;
		return 1;
	}
	else return 0;
}

static void
add_cluster (Agraph_t *g, Agraph_t *subg)
{
	int     cno;
	cno = ++(GD_n_cluster(g));
	GD_clust(g) = ZALLOC(cno+1,GD_clust(g),graph_t*,GD_n_cluster(g));
	GD_clust(g)[cno] = subg;
	do_graph_label(subg);
}

static void nop_init_graphs (Agraph_t*, attrsym_t*, attrsym_t*);

/* dfs:
 */
static void
dfs (node_t* mn, Agraph_t* g, attrsym_t* G_lp, attrsym_t* G_bb)
{
	graph_t     *subg;

	subg = agusergraph(mn);
	if (!strncmp(subg->name,"cluster",7) && chkBB(subg, G_bb)) {
		add_cluster (g, subg);
		nop_init_graphs (subg, G_lp, G_bb);
	}
	else {
		graph_t* mg = g->meta_node->graph;
		edge_t*  me;
		for (me = agfstout(mg,mn); me; me = agnxtout(mg,me)) {
        	dfs (me->head, g, G_lp, G_bb);
    	}
	}
}

/* nop_init_graphs:
 * Read in clusters and graph label info.
 * A subgraph is a cluster if its name starts with "cluster" and
 * it has a valid bb.
 */
static void
nop_init_graphs (Agraph_t *g, attrsym_t* G_lp, attrsym_t* G_bb)
{
	graph_t     *mg;
	edge_t      *me;
	char*       s;
	point       p;

    if (GD_label(g) && G_lp) {
		s = agxget (g, G_lp->index);
		if (sscanf (s,"%d,%d",&p.x,&p.y) == 2) {
			GD_label(g)->set = TRUE;
			GD_label(g)->p = p;
    	}
    }

	if (!G_bb) return;
	mg = g->meta_node->graph;
	for (me = agfstout(mg,g->meta_node); me; me = agnxtout(mg,me)) {
        dfs (me->head, g, G_lp, G_bb);
    }
}

/* translateE:
 * Translate edge by offset.
 * Assume ED_spl(e) != NULL
 */
static void
translateE (edge_t *e, pointf offset)
{
	int      i, j;
	point*   pt;
	bezier*  bez;

	bez = ED_spl(e)->list;
	for (i = 0; i < ED_spl(e)->size; i++) {
		pt = bez->list;
		for (j = 0; j < bez->size; j++) {
			pt->x -= offset.x;
			pt->y -= offset.y;
			pt++;
		}
		if (bez->sflag) {
			bez->sp.x -= offset.x;
			bez->sp.y -= offset.y;
		}
		if (bez->eflag) {
			bez->ep.x -= offset.x;
			bez->ep.y -= offset.y;
		}
		bez++;
	}

	if (ED_label(e) && ED_label(e)->set) {
		ED_label(e)->p.x -= offset.x;
		ED_label(e)->p.y -= offset.y;
	}
	if (ED_head_label(e) && ED_head_label(e)->set) {
		ED_head_label(e)->p.x -= offset.x;
		ED_head_label(e)->p.y -= offset.y;
	}
	if (ED_tail_label(e) && ED_tail_label(e)->set) {
		ED_tail_label(e)->p.x -= offset.x;
		ED_tail_label(e)->p.y -= offset.y;
	}
}

/* translateG:
 */
static void
translateG (Agraph_t *g, point offset)
{
	int  i;

	GD_bb(g).UR.x -= offset.x;
	GD_bb(g).UR.y -= offset.y;
	GD_bb(g).LL.x -= offset.x;
	GD_bb(g).LL.y -= offset.y;

	if (GD_label(g) && GD_label(g)->set) {
		GD_label(g)->p.x -= offset.x;
		GD_label(g)->p.y -= offset.y;
	}

    for (i = 1; i <= GD_n_cluster(g); i++)
      translateG (GD_clust(g)[i],offset);
}

/* translate:
 */
static void
translate (Agraph_t *g, pos_edge posEdges)
{
	node_t      *n;
	edge_t      *e;
	pointf      offset;

	offset = cvt2ptf(GD_bb(g).LL);
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		ND_pos(n)[0] -= offset.x;
		ND_pos(n)[1] -= offset.y;
	}
	if (posEdges != NoEdges) {
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
        	for (e = agfstout(g,n); e; e = agnxtout(g,e)) 
				if (ED_spl(e)) translateE(e, offset);
		}
	}
	translateG (g, GD_bb(g).LL);
}

/* init_nop:
 * This assumes all nodes have been positioned, and the
 * root graph has a bb defined, as name/value pairs. 
 * (We could possible weaken the latter and perform a 
 * recomputation of all bb.) It also assumes none of the 
 * relevant fields in A*info_t have set.
 * The input may provide additional position information for
 * clusters, edges and labels. If certain position information
 * is missing, init_nop will use a standard neato technique to
 * supply it.
 */
int
init_nop (Agraph_t *g)
{
	int         i;
    node_t*     np;
	pos_edge    posEdges;  /* How many edges have spline info */
	attrsym_t*  G_lp = agfindattr (g, "lp");
	attrsym_t*  G_bb = agfindattr (g, "bb");

   	scan_graph(g);       /* mainly to set up GD_neato_nlist */
	for (i = 0; (np = GD_neato_nlist(g)[i]); i++) {
		if (!hasPos(np)) {
			agerr(AGERR, "node %s in graph %s has no position\n",
				np->name, g->name);
			return 1;
		}
    }
	nop_init_graphs (g, G_lp, G_bb);
	posEdges = nop_init_edges (g);
	
	if (Nop == 1) adjustNodes(g);

    /* If G_bb not defined, define it */
    if (!G_bb) G_bb = agraphattr (g, "bb", ""); 

    /* If g does not have a good "bb" attribute, compute it. */
    if (!chkBB(g, G_bb)) neato_compute_bb (g);

    /* At this point, all bounding boxes should be correctly defined.
     * If necessary, we translate the graph to the origin.
     */
	if (GD_bb(g).LL.x || GD_bb(g).LL.y) translate (g, posEdges);

	if (posEdges != AllEdges) spline_edges0(g);
	else {
		node_t      *n;
		neato_set_aspect(g);
		State = GVSPLINES;
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
				ND_coord_i(n).x = POINTS(ND_pos(n)[0]);
				ND_coord_i(n).y = POINTS(ND_pos(n)[1]);
    	}
    }
	return 0;
}

void
neato_init_graph (Agraph_t *g)
{
	UseRankdir = FALSE;
    graph_init(g);
    GD_drawing(g)->engine = NEATO;
	GD_ndim(g) = late_int(g,agfindattr(g,"dim"),2,2);
	Ndim = GD_ndim(g) = MIN(GD_ndim(g),MAXDIM);
    neato_init_node_edge(g);
}

void neato_layout(Agraph_t *g)
{
    int         nG;
   
	neato_init_graph(g);
	if (Nop) {
    	if (init_nop (g)) {
			agerr(AGPREV, "as required by the -n flag\n");
			exit (1);
		}
 	}
	else {
		char* p = agget(g,"model");
		pack_mode mode = getPackMode (g, l_undef);
        Pack = getPack (g, -1, CL_OFFSET);
          /* pack if just packmode defined. */
        if (mode == l_undef) mode = l_node;
        else if (Pack < 0) Pack = CL_OFFSET; 
        if (Pack >= 0) {
			graph_t*    gc;
			graph_t**   cc;
			int         n_cc;
			int         n_n;
			int         i;
			int         useCircuit;
			pack_info   pinfo;
			boolean     pin;

			useCircuit = (p && (streq(p,"circuit")));
			cc = pccomps (g, &n_cc, cc_pfx, &pin);

			for (i = 0; i < n_cc; i++) {
				gc = cc[i];
				nodeInduce (gc);
    			n_n = scan_graph(gc);
				if (useCircuit) circuit_model(gc,n_n);
				else shortest_path(gc, n_n);
				initial_positions(gc, n_n);
    			diffeq_model(gc, n_n);
    			solve_model(gc, n_n);
    			final_energy(gc, n_n); 
    			adjustNodes(gc);
			}
			if (n_cc > 1) {
				boolean*  bp;
				if (pin) {
					bp = N_NEW(n_cc,boolean);
					bp[0] = TRUE;
				}
				else bp = 0;
            	pinfo.margin = Pack;
            	pinfo.doSplines = 0;
            	pinfo.mode = mode;
				pinfo.fixed = bp;
				packGraphs (n_cc, cc, 0, &pinfo);
				if (bp) free (bp);
			}
  			neato_compute_bb (g);
   			spline_edges(g);
        }
        else {
    		nG = scan_graph(g);
			if (p && (streq(p,"circuit"))) {
              if (!circuit_model(g,nG)) {
                agerr(AGWARN, "graph %s is disconnected. In this case, the circuit model\n", g->name);
                agerr(AGPREV, "is undefined and neato is reverting to the shortest path model.\n");
                agerr(AGPREV, "Alternatively, consider running neato using -Gpack=true or decomposing\n");
                agerr(AGPREV, "the graph into connected components.\n");
			    shortest_path(g, nG);
              }
            }
			else shortest_path(g, nG);
    		initial_positions(g, nG);
    		diffeq_model(g, nG);
    		solve_model(g, nG);
    		final_energy(g, nG); 
    		adjustNodes(g);
   			spline_edges(g);
		}
	}
	dotneato_postprocess(g, neato_nodesize);
}   
