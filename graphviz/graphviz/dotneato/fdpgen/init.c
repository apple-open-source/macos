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

/* init.c:
 * Written by Emden R. Gansner
 *
 * Mostly boilerplate initialization and cleanup code.
 */

/* uses PRIVATE interface */
#define FDP_PRIVATE 1

#include    "fdp.h"
#include    "tlayout.h"
#include    "neatoprocs.h"

static void
initialPositions (graph_t *g)
{
  int         i;
  node_t*     np;
  attrsym_t*  possym;
  attrsym_t*  pinsym;
  double*     pvec;
  char*       p;
  char        c;

  possym = agfindattr(g->proto->n,"pos");
  if (!possym) return;
  pinsym = agfindattr(g->proto->n,"pin");
  for (i = 0; (np = GD_neato_nlist(g)[i]); i++) {
    p = agxget(np,possym->index);
    if (p[0]) {
      pvec = ND_pos(np);
      c = '\0';
      if (sscanf(p,"%lf,%lf%c",pvec,pvec+1,&c) >= 2) {
        if (PSinputscale > 0.0) {
          int i;
          for (i = 0; i < NDIM; i++) pvec[i] = pvec[i] / PSinputscale;
        }
        ND_pinned(np) = P_SET;
        if ((c == '!') || (pinsym && mapbool(agxget(np,pinsym->index))))
          ND_pinned(np) = P_PIN;
      }
      else 
        fprintf(stderr,"Warning: node %s, position %s, expected two floats\n",
          np->name,p);
    }
  }
}

static void
fdp_initNode (node_t* n)
{
  ND_alg(n) = (void*)NEW(ndata); /* freed in cleanup_node */
  ND_pos(n) = N_GNEW(GD_ndim(n->graph),double);
}

static void
init_node (node_t* n)
{
  neato_init_node (n);
  fdp_initNode (n);
}

/* init_edge:
 * Return true if edge has label.
 */
static int
init_edge (edge_t* e, attrsym_t* E_len)
{
    int     r;

    ED_factor(e) = late_double(e,E_weight,1.0,0.0);
    ED_dist(e) = late_double(e,E_len,fdp_tvals.K,0.0);

    /* initialize labels and set r TRUE if edge has one */
    r = common_init_edge(e);

    init_port(e->tail,e,agget(e,"tailport"),FALSE);
    init_port(e->head,e,agget(e,"headport"),TRUE);

    return r;
}

void 
fdp_init_node_edge (graph_t *g)
{
  attrsym_t* E_len;
  node_t* n;
  edge_t* e;
  int     nn;
  int     i = 0;

  nn = agnnodes (g);
  GD_neato_nlist(g) = N_NEW(nn + 1,node_t*);
  GD_alg(g) = (void*)NEW(gdata); /* freed in cleanup_node */

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    init_node(n);
    GD_neato_nlist(g)[i] = n;
    ND_id(n) = i++;
  }

  E_len = agfindattr(g->proto->e,"len");
  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
      if (init_edge(e, E_len)) GD_has_labels(g) = TRUE;
    }
  }
  initialPositions(g);
  
}

static void 
fdp_cleanup_node(node_t* n)
{
    GVC_t *gvc = GD_gvc(n->graph->root);

    gvc->n = n;

    free (ND_alg(n));
    free (ND_pos(n));
    if (ND_shape(n))
        ND_shape(n)->fns->freefn(gvc);
    free_label(ND_label(n));
    memset(&(n->u),0,sizeof(Agnodeinfo_t));
}

static void 
fdp_free_splines(edge_t* e)
{
    int     i;
    if (ED_spl(e)) {
        for (i = 0; i < ED_spl(e)->size; i++)
                free(ED_spl(e)->list[i].list);
        free(ED_spl(e)->list);
        free(ED_spl(e));
    }
    ED_spl(e) = NULL;
}

static void 
fdp_cleanup_edge(edge_t* e)
{
    fdp_free_splines(e);
    free_label(ED_label(e));
    memset(&(e->u),0,sizeof(Agedgeinfo_t));
}

static void
cleanup_subgs(graph_t* g)
{
  graph_t*  mg;
  edge_t*   me;
  node_t*   mn;
  graph_t*  subg;

  mg = g->meta_node->graph;
  for (me = agfstout(mg,g->meta_node); me; me = agnxtout(mg,me)) {
    mn = me->head;
    subg = agusergraph(mn);
    free_label(GD_label(subg));
    if (GD_alg(subg)) {
      free (PORTS(subg));
      free (GD_alg(subg));
    }
    cleanup_subgs (subg);
  }
}

static void 
fdp_cleanup_graph(graph_t* g)
{
  cleanup_subgs (g);
  free(GD_neato_nlist(g));
  free (GD_alg(g));
  free_ugraph(g);
  free_label(GD_label(g));
  memset(&(g->u),0,sizeof(Agraphinfo_t));
}

void 
fdp_cleanup(graph_t* g)
{
  node_t  *n;
  edge_t  *e;

  for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
    for (e = agfstedge(g, n); e; e = agnxtedge(g, e, n)) {
      fdp_cleanup_edge(e);
    }
    fdp_cleanup_node(n);
  }
  fdp_cleanup_graph(g);
  agclose (g);
}
