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
 * position(g): set ND_coord_i(n) (x and y) for all nodes n of g, using GD_rank(g).
 * (the graph may be modified by merging certain edges with a common endpoint.)
 * the coordinates are computed by constructing and ranking an auxiliary graph.
 * then leaf nodes are inserted in the fast graph.  cluster boundary nodes are
 * created and correctly separated.
 */

#include "dot.h"


void dot_position(graph_t* g)
{
	if (GD_nlist(g) == NULL) return;		/* ignore empty graph */
	mark_lowclusters(g);				/* we could remove from splines.c now */
	set_ycoords(g);
	if (Concentrate) dot_concentrate(g);
	expand_leaves(g);
	if (flat_edges(g)) set_ycoords(g);
	create_aux_edges(g);
	rank(g,2,nsiter2(g));				/* LR balance == 2*/
	set_xcoords(g);
	remove_aux_edges(g);
	set_aspect(g);
}

int nsiter2(graph_t* g)
{
	int		maxiter = MAXINT;
	char	*s;

	if ((s = agget(g,"nslimit")))
		maxiter = atof(s) * agnnodes(g);
	return maxiter;
}

static int searchcnt;
static int go(node_t* u, node_t* v)
{
	int		i;
	edge_t	*e;

	if (u == v) return TRUE;
	for (i = 0; (e = ND_out(u).list[i]); i++) {
		if (go(e->head,v))
			return TRUE;
	}
	return FALSE;
}

static int canreach(node_t* u, node_t* v)
{
	if (++searchcnt == 0) searchcnt = 1;
	return go(u,v);
}

edge_t *
make_aux_edge(node_t *u, node_t *v, int len, int wt)
{
	edge_t	*e;

	e = NEW(edge_t);
	e->tail = u;
	e->head = v;
	ED_minlen(e) = len;
	ED_weight(e) = wt;
	fast_edge(e);
	return e;
}


void allocate_aux_edges(graph_t* g)
{
	int		i,j,n_in;
	node_t	*n;

	/* allocate space for aux edge lists */
	for (n = GD_nlist(g); n; n = ND_next(n)) {
		ND_save_in(n) = ND_in(n);
		ND_save_out(n) = ND_out(n);
		for (i = 0; ND_out(n).list[i]; i++);
		for (j = 0; ND_in(n).list[j]; j++);
		n_in = i + j;
		alloc_elist(n_in + 3,ND_in(n));
		alloc_elist(3,ND_out(n));
	}
}

void make_LR_constraints(graph_t* g)
{
	int		i,j,k;
	int		sw;		/* self width */
	int		m0,m1;
	int		width;
	edge_t	*e, *e0, *e1, *f, *ff;
	node_t	*u,*v, *t0, *h0;
	rank_t	*rank = GD_rank(g);

	/* make edges to constrain left-to-right ordering */
	for (i = GD_minrank(g); i <= GD_maxrank(g); i++) {
		int		last;
		last = rank[i].v[0]->u.rank = 0;
		for (j = 0; j < rank[i].n; j++) {
			u = rank[i].v[j];
			ND_mval(u) = ND_rw_i(u);	/* keep it somewhere safe */
			if (ND_other(u).size > 0) {	/* compute self size */
				sw = 0;
				for (k = 0; (e = ND_other(u).list[k]); k++) {
					if (e->tail == e->head) {
						sw += SELF_EDGE_SIZE;
						if (ED_label(e)) {
							double	label_width;
							label_width = GD_left_to_right(g)? ED_label(e)->dimen.y : ED_label(e)->dimen.x;
							sw += POINTS(label_width);
						}
					}
				}
				ND_rw_i(u) += sw;			/* increment to include self edges */
			}
			v = rank[i].v[j+1];
			if (v) {
				width = ND_rw_i(u) + ND_lw_i(v) + GD_nodesep(g);
				e0 = make_aux_edge(u,v,width,0);
				last = (ND_rank(v) = last + width);
			}

				/* position flat edge endpoints */
			for (k = 0; k < ND_flat_out(u).size; k++) {
				e = ND_flat_out(u).list[k];
				v = e->head;
				if (ND_order(e->tail) < ND_order(e->head))
					{ t0 = e->tail; h0 = e->head; }
				else
					{ t0 = e->head; h0 = e->tail; }

				/* case 1: flat edge with a label */
				if ((f = ED_to_virt(e))) {
					while (ED_to_virt(f)) f = ED_to_virt(f);
					e0 = ND_save_out(f->tail).list[0];
					e1 = ND_save_out(f->tail).list[1];
					if (ND_order(e0->head) > ND_order(e1->head))
						{ ff = e0; e0 = e1; e1 = ff; }
					m0 = (ED_minlen(e) *GD_nodesep(g))/2;
					m1 = m0 + ND_rw_i(e0->head) + ND_lw_i(e0->tail);
					/* these guards are needed because the flat edges
						work very poorly with cluster layout */
					if (canreach(e0->tail,e0->head) == FALSE)
						make_aux_edge(e0->head,e0->tail,m1,ED_weight(e));
					m1 = m0 + ND_rw_i(e1->tail) + ND_lw_i(e1->head);
					if (canreach(e1->head,e1->tail) == FALSE)
						make_aux_edge(e1->tail,e1->head,m1,ED_weight(e));
					continue;
				}

				m0 = ED_minlen(e) *GD_nodesep(g) + ND_rw_i(t0) + ND_lw_i(h0);

				if ((e0 = find_fast_edge(t0,h0)))
					/* case 2: flat edge between neighbors */
					ED_minlen(e0) = MAX(ED_minlen(e0),m0);
				else
					/* case 3: flat edge between non-neighbors */
					make_aux_edge(t0,h0,m0,ED_weight(e));
			}
		}
	}
}

/* make_edge_pairs: make virtual edge pairs corresponding to input edges */
void make_edge_pairs(graph_t* g)
{
	int			i,m0,m1;
	node_t		*n,*sn;
	edge_t		*e;

	for (n = GD_nlist(g); n; n = ND_next(n)) {
		if (ND_save_out(n).list) for (i = 0; (e = ND_save_out(n).list[i]); i++) {
			sn = virtual_node(g);
			ND_node_type(sn) = SLACKNODE;
			m0 = (ED_head_port(e).p.x - ED_tail_port(e).p.x);
			if (m0 > 0) m1 = 0;
			else {m1 = -m0; m0 = 0;}
#ifdef NOTDEF	
/* was trying to improve LR balance */
if ((ND_save_out(n).size % 2 == 0) && (i == ND_save_out(n).size / 2 - 1)) {
node_t *u = ND_save_out(n).list[i]->head;
node_t *v = ND_save_out(n).list[i+1]->head;
int width = ND_rw_i(u) + ND_lw_i(v) + GD_nodesep(g);
m0 = width / 2 - 1;
}
#endif
			make_aux_edge(sn,e->tail,m0+1,ED_weight(e));
			make_aux_edge(sn,e->head,m1+1,ED_weight(e));
			ND_rank(sn) = MIN(ND_rank(e->tail) - m0 -1, ND_rank(e->head) - m1 - 1);
		}
	}
}

/* pos_clusters: create constraints for:
 *	node containment in clusters,
 *	cluster containment in clusters,
 *	separation of sibling clusters.
 */
void pos_clusters(graph_t* g)
{
	if (GD_n_cluster(g) > 0) {
		contain_clustnodes(g);
		keepout_othernodes(g);
		contain_subclust(g);
		separate_subclust(g);
	}
}

void contain_clustnodes(graph_t* g)
{
	int		c;

	if (g != g->root) {
		contain_nodes(g);
		make_aux_edge(GD_ln(g), GD_rn(g), 1, 128);	/* clust compaction edge */
	}
	for (c = 1; c <= GD_n_cluster(g); c++)
		contain_clustnodes(GD_clust(g)[c]);
}

int vnode_not_related_to(graph_t* g, node_t* v)
{
	edge_t	*e;

	if (ND_node_type(v) != VIRTUAL) return FALSE;
	for (e = ND_save_out(v).list[0]; ED_to_orig(e); e = ED_to_orig(e));
	if (agcontains(g,e->tail)) return FALSE;
	if (agcontains(g,e->head)) return FALSE;
	return TRUE;
}

void keepout_othernodes(graph_t* g)
{
	int		i,c,r;
	node_t	*u,*v;

	for (r = GD_minrank(g); r <= GD_maxrank(g); r++) {
		if (GD_rank(g)[r].n == 0) continue;
		v = GD_rank(g)[r].v[0];
		if (v == NULL) continue;
		for (i = ND_order(v) - 1; i >= 0; i--) {
			u = GD_rank(g->root)[r].v[i];
				/* can't use "is_a_vnode_of" because elists are swapped */
			if ((ND_node_type(u) == NORMAL) || vnode_not_related_to(g,u)) {
				make_aux_edge(u,GD_ln(g),CL_OFFSET+ND_rw_i(u)+GD_border(g)[LEFT_IX].x,0);
				break;
			}
		}
		for (i = ND_order(v) + GD_rank(g)[r].n; i < GD_rank(g->root)[r].n; i++) {
			u = ND_rank(g->root)[r].v[i];
			if ((ND_node_type(u) == NORMAL) || vnode_not_related_to(g,u)) {
				make_aux_edge(GD_rn(g),u,CL_OFFSET+ND_lw_i(u)+GD_border(g)[RIGHT_IX].x,0);
				break;
			}
		}
	}
	
	for (c = 1; c <= GD_n_cluster(g); c++)
		keepout_othernodes(GD_clust(g)[c]);
}

void contain_subclust(graph_t* g)
{
	int		c;
	graph_t	*subg;

	make_lrvn(g);
	for (c = 1; c <= GD_n_cluster(g); c++) {
		subg = GD_clust(g)[c];
		make_lrvn(subg);
		make_aux_edge(GD_ln(g), GD_ln(subg), CL_OFFSET + GD_border(subg)[LEFT_IX].x, 0);
		make_aux_edge(GD_rn(subg), GD_rn(g), CL_OFFSET + GD_border(subg)[RIGHT_IX].x, 0);
		contain_subclust(subg);
	}
}

void separate_subclust(graph_t* g)
{
	int			i,j;
	graph_t		*low,*high;
	graph_t		*left,*right;

	for (i = 1; i <= GD_n_cluster(g); i++) make_lrvn(GD_clust(g)[i]);
	for (i = 1; i <= GD_n_cluster(g); i++) {
		for (j = i + 1; j <= GD_n_cluster(g); j++) {
			low = GD_clust(g)[i]; high = GD_clust(g)[j];
			if (GD_minrank(low) > GD_minrank(high))
				{ graph_t	*temp = low; low = high; high= temp; }
			if (GD_maxrank(low) < GD_minrank(high)) continue;
			if (ND_order(GD_rank(low)[GD_minrank(high)].v[0])
				< ND_order(GD_rank(high)[GD_minrank(high)].v[0]))
					{ left = low; right = high; }
				else
					{ left = high; right = low; }
			make_aux_edge(ND_rn(left), ND_ln(right),
				CL_OFFSET+ND_border(left)[RIGHT_IX].x+ND_border(right)[LEFT_IX].x,0);
		}
		separate_subclust(GD_clust(g)[i]);
	}
}

void create_aux_edges(graph_t* g)
{
	allocate_aux_edges(g);
	make_LR_constraints(g);
	make_edge_pairs(g);
	pos_clusters(g);
	compress_graph(g);
}

void remove_aux_edges(graph_t* g)
{
	int		i;
	node_t	*n,*nnext,*nprev;
	edge_t	*e;

	for (n = GD_nlist(g); n; n = ND_next(n)) {
		for (i = 0; (e = ND_out(n).list[i]); i++) free(e);
		free_list(ND_out(n));
		free_list(ND_in(n));
		ND_out(n) = ND_save_out(n);
		ND_in(n) = ND_save_in(n);
	}
	/* cannot be merged with previous loop */
	nprev = NULL;
	for (n = GD_nlist(g); n; n = nnext) {
		nnext = ND_next(n);
		if (ND_node_type(n) == SLACKNODE) {
			if (nprev) ND_next(nprev) = nnext;
			else GD_nlist(g) = nnext;
			free(n);
		}
		else nprev = n;
	}
	GD_nlist(g)->u.prev = NULL;
}

void set_xcoords(graph_t* g)
{
	int		i,j;
	node_t	*v;
	rank_t	*rank = GD_rank(g);

	for (i = GD_minrank(g);  i <= GD_maxrank(g); i++) {
		for (j = 0; j < rank[i].n; j++) {
			v = rank[i].v[j];
			ND_coord_i(v).x = ND_rank(v);
			ND_rank(v) = i;
		}
	}
}

static void clust_ht(graph_t *g);

/* set y coordinates of nodes, a rank at a time */
void set_ycoords(graph_t* g)
{
	int		i,j,r,ht2,maxht,delta,d0,d1;
	node_t	*n;
	edge_t	*e;
	rank_t	*rank = GD_rank(g);
	graph_t	*clust;

	ht2 = maxht = 0;

	/* scan ranks for tallest nodes.  */
	for (r = GD_minrank(g); r <=  GD_maxrank(g); r++) {
		for (i = 0; i < rank[r].n; i++) {
			n = rank[r].v[i];

			/* assumes symmetry, ht1 = ht2 */
			ht2 = (ND_ht_i(n) + 1)/2;


			/* have to look for high self-edge labels, too */
			if (ND_other(n).list) for (j = 0; (e = ND_other(n).list[j]); j++) {
				if (e->tail == e->head) {
					if (ED_label(e)) ht2 = MAX(ht2,POINTS(ED_label(e)->dimen.y)/2);
				}
			}

			/* update global rank ht */
			if (rank[r].pht2 < ht2) rank[r].pht2 = rank[r].ht2 = ht2;
			if (rank[r].pht1 < ht2) rank[r].pht1 = rank[r].ht1 = ht2;

			/* update nearest enclosing cluster rank ht */
			if ((clust = ND_clust(n))) {
				if (ND_rank(n) == GD_minrank(clust))
					GD_ht2(clust) = MAX(GD_ht2(clust),ht2 + CL_OFFSET);
				if (ND_rank(n) == GD_maxrank(clust))
					GD_ht1(clust) = MAX(GD_ht1(clust),ht2 + CL_OFFSET);
			}
		}
	}

	/* scan sub-clusters */
	clust_ht(g);

	/* make the initial assignment of ycoords to leftmost nodes by ranks */
	maxht = 0;
	r = GD_maxrank(g);
	rank[r].v[0]->u.coord.y = rank[r].ht1;
	while (--r >= GD_minrank(g)) {
		d0 = rank[r+1].pht2 + rank[r].pht1 + GD_ranksep(g); /* prim node sep */
		d1 = rank[r+1].ht2 + rank[r].ht1 + CL_OFFSET;	/* cluster sep */
		delta = MAX(d0,d1);
		if (rank[r].n > 0) /* this may reflect some problem */
		rank[r].v[0]->u.coord.y = rank[r + 1].v[0]->u.coord.y + delta;
#ifdef DEBUG
		else fprintf(stderr,"dot set_ycoords: rank %d is empty\n",rank[r].n);
#endif
		maxht = MAX(maxht,delta);
	}

	/* re-assign if ranks are equally spaced */
	if (GD_exact_ranksep(g))
		for (r = GD_maxrank(g) - 1; r >=  GD_minrank(g); r--)
			if (rank[r].n > 0) /* this may reflect the same problem :-() */
				rank[r].v[0]->u.coord.y = rank[r + 1].v[0]->u.coord.y + maxht;

	/* copy ycoord assignment from leftmost nodes to others */
	for (n = GD_nlist(g); n; n = ND_next(n))
		ND_coord_i(n).y = rank[ND_rank(n)].v[0]->u.coord.y;
}

void compute_bb(graph_t* g, graph_t* root)
{
	int		c,r,x;
	node_t	*v;
	point	LL,UR,p,offset;
	
	LL.x = LL.y = MAXINT;
	UR.x = UR.y = -MAXINT;
	for (r = GD_minrank(g); r <= GD_maxrank(g); r++) {
		if (GD_rank(g)[r].n == 0) continue;
		if ((v = GD_rank(g)[r].v[0]) == NULL) continue;
		x = ND_coord_i(v).x - ND_lw_i(v);
		if (g != g->root) x-= CL_OFFSET;
		LL.x = MIN(LL.x,x);
		v = GD_rank(g)[r].v[GD_rank(g)[r].n-1];
		x = ND_coord_i(v).x + ND_rw_i(v);
		if (g != g->root) x+= CL_OFFSET;
		UR.x = MAX(UR.x,x);
	}
	offset.x = offset.y = CL_OFFSET;
	for (c = 1; c <= GD_n_cluster(g); c++) {
		p = sub_points(GD_clust(g)[c]->u.bb.LL,offset);
		if (LL.x > p.x) LL.x = p.x;
		p = add_points(GD_clust(g)[c]->u.bb.UR,offset);
		if (UR.x < p.x) UR.x = p.x;
	}
	LL.y = ND_rank(root)[GD_maxrank(g)].v[0]->u.coord.y - GD_ht1(g);
	UR.y = ND_rank(root)[GD_minrank(g)].v[0]->u.coord.y + GD_ht2(g);
	GD_bb(g).LL = LL; GD_bb(g).UR = UR;
}

void rec_bb(graph_t *g, graph_t *root)
{
	int		c;
	for (c = 1; c <= GD_n_cluster(g); c++)
		rec_bb(GD_clust(g)[c],root);
	compute_bb(g,root);
}

void set_aspect(graph_t* g)
{
	double	xf=0.0,yf=0.0,actual,desired;
	char	*str;
	node_t	*n;
	boolean	scale_it,filled;

	rec_bb(g,g);
	if ((GD_maxrank(g) > 0) && (str = agget(g,"ratio"))) {
		GD_bb(g).UR.x -= GD_bb(g).LL.x;
		GD_bb(g).UR.y -= GD_bb(g).LL.y;	/* normalize */
		if (GD_left_to_right(g))
			{int t = GD_bb(g).UR.x; GD_bb(g).UR.x = GD_bb(g).UR.y; GD_bb(g).UR.y = t;}
		scale_it = TRUE;
		if (streq(str,"auto")) filled = idealsize(g,.5);
		else filled = (streq(str,"fill"));
		if (filled) {
			/* fill is weird because both X and Y can stretch */
			if (GD_drawing(g)->size.x <= 0) scale_it = FALSE;
			else {
				xf = (double)GD_drawing(g)->size.x / (double)GD_bb(g).UR.x;
				yf = (double)GD_drawing(g)->size.y / (double)GD_bb(g).UR.y;
				if ((xf < 1.0) || (yf < 1.0)) {
					if (xf < yf) {yf = yf / xf; xf = 1.0;}
					else {xf = xf / yf; yf = 1.0;}
				}
			}
		}
		else {
			desired = atof(str);
			if (desired == 0.0) scale_it = FALSE;
			else {
				actual = ((double)GD_bb(g).UR.y)/((double)GD_bb(g).UR.x);
				if (actual < desired) {yf = desired/actual; xf = 1.0;}
				else {xf = actual/desired; yf = 1.0;}
			}
		}
		if (scale_it) {
			if (GD_left_to_right(g)) {double t = xf; xf = yf; yf = t;}
			for (n = GD_nlist(g); n; n = ND_next(n)) {
				ND_coord_i(n).x = ND_coord_i(n).x * xf;
				ND_coord_i(n).y = ND_coord_i(n).y * yf;
			}
		}
	}
	rec_bb(g,g);
}

point
resize_leaf(node_t* leaf, point lbound)
{
	dot_nodesize(leaf,GD_left_to_right(leaf->graph));
	ND_coord_i(leaf).y = lbound.y;
	ND_coord_i(leaf).x = lbound.x + ND_lw_i(leaf);
	lbound.x = lbound.x + ND_lw_i(leaf) + ND_rw_i(leaf) + GD_nodesep(leaf->graph);
	return lbound;
}

point
place_leaf(node_t* leaf, point lbound, int order)
{
	node_t	*leader;
	graph_t	*g = leaf->graph;

	leader = UF_find(leaf);
	if (leaf != leader) fast_nodeapp(leader,leaf);
	ND_order(leaf) = order;
	ND_rank(leaf) = ND_rank(leader);
	GD_rank(g)[ND_rank(leaf)].v[ND_order(leaf)] = leaf;
	return resize_leaf(leaf,lbound);
}

/* make space for the leaf nodes of each rank */
void make_leafslots(graph_t* g)
{
	int		i,j,r;
	node_t	*v;

	for (r = GD_minrank(g); r <= GD_maxrank(g); r++) {
		j = 0;
		for (i = 0; i < GD_rank(g)[r].n; i++) {
			v = GD_rank(g)[r].v[i];
			ND_order(v) = j;
			if (ND_ranktype(v) == LEAFSET) j = j + ND_UF_size(v);
			else j++;
		}
		if (j <= GD_rank(g)[r].n) continue;
		GD_rank(g)[r].v = ALLOC(j+1,GD_rank(g)[r].v,node_t*);
		for (i = GD_rank(g)[r].n - 1; i >= 0; i--) {
			v = GD_rank(g)[r].v[i];
			GD_rank(g)[r].v[ND_order(v)] = v;
		}
		GD_rank(g)[r].n = j;
		GD_rank(g)[r].v[j] = NULL;
	}
}

void do_leaves(graph_t* g, node_t* leader)
{
	int		j;
	point	lbound;
	node_t	*n;
	edge_t	*e;

	if (ND_UF_size(leader) <= 1) return;
	lbound.x = ND_coord_i(leader).x - ND_lw_i(leader);
	lbound.y = ND_coord_i(leader).y;
	lbound = resize_leaf(leader,lbound);
	if (ND_out(leader).size > 0) {		/* in-edge leaves */
		n = ND_out(leader).list[0]->head;
		j = ND_order(leader) + 1;
		for (e = agfstin(g,n); e; e = agnxtin(g,e)) {
			if ((e->tail != leader) && (UF_find(e->tail) == leader)) {
				lbound = place_leaf(e->tail,lbound,j++);
				unmerge_oneway(e);
				elist_append(e,ND_in(e->head));
			}
		}
	}
	else {							/* out edge leaves */
		n = ND_in(leader).list[0]->tail;
		j = ND_order(leader) + 1;
		for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
			if ((e->head != leader) && (UF_find(e->head) == leader)) {
				lbound = place_leaf(e->head,lbound,j++);
				unmerge_oneway(e);
				elist_append(e,ND_out(e->tail));
			}
		}
	}
}

int ports_eq(edge_t *e,edge_t *f)
{
	return (
		   (ED_head_port(e).defined == ED_head_port(f).defined)
		&& ( ((ED_head_port(e).p.x == ED_head_port(f).p.x) &&
			 (ED_head_port(e).p.y == ED_head_port(f).p.y))
			|| (ED_head_port(e).defined == FALSE))
		&& ( ((ED_tail_port(e).p.x == ED_tail_port(f).p.x) &&
			 (ED_tail_port(e).p.y == ED_tail_port(f).p.y))
			|| (ED_tail_port(e).defined == FALSE))
	);
}

void expand_leaves(graph_t* g)
{
	int		i,d;
	node_t	*n;
	edge_t	*e,*f;

	make_leafslots(g);
	for (n = GD_nlist(g); n; n = ND_next(n)) {
		if (ND_inleaf(n)) do_leaves(g,ND_inleaf(n));
		if (ND_outleaf(n)) do_leaves(g,ND_outleaf(n));
		if (ND_other(n).list) for (i = 0; (e = ND_other(n).list[i]); i++) {
			if ((d = ND_rank(e->head) - ND_rank(e->head)) == 0) continue;
			f = ED_to_orig(e);
			if (ports_eq(e,f) == FALSE) {
				zapinlist(&(ND_other(n)),e);
				if (d == 1) fast_edge(e);
				/*else unitize(e); ### */
				i--;
			}
		}
	}
}

void compress_graph(graph_t* g)
{
	char		*str;
	double		x;
	point		p;

	p = GD_drawing(g)->size;
	if ((str = agget(g,"ratio")) == NULL) return;
	if (strcmp(str,"compress")) return;
	if (p.x * p.y <= 1) return;
	contain_nodes(g);
	if (GD_left_to_right(g) == FALSE) x = p.x; else x = p.y;
	make_aux_edge(GD_ln(g),GD_rn(g),(int)x,1000);
}

void make_lrvn(graph_t* g)
{
	node_t		*ln,*rn;

	if (GD_ln(g)) return;
	ln = virtual_node(g->root); ND_node_type(ln) = SLACKNODE;
	rn = virtual_node(g->root); ND_node_type(rn) = SLACKNODE;
	GD_ln(g) = ln; GD_rn(g) = rn;
}

/* contain_nodes: make left and right bounding box virtual nodes,
 * 		constrain interior nodes
 */
void contain_nodes(graph_t* g)
{
	int			r;
	node_t		*ln,*rn,*v;

	make_lrvn(g); ln = GD_ln(g); rn = GD_rn(g);
	for (r = GD_minrank(g); r <= GD_maxrank(g); r++) {
		if (GD_rank(g)[r].n == 0) continue;
		v = GD_rank(g)[r].v[0];
		if (v == NULL) {
			agerr(AGERR, "contain_nodes clust %s rank %d missing node\n",g->name,r);
			continue;
		}
		make_aux_edge(ln,v,ND_lw_i(v) + CL_OFFSET,0);
		v = GD_rank(g)[r].v[GD_rank(g)[r].n - 1];
		make_aux_edge(v,rn,ND_rw_i(v) + CL_OFFSET,0);
	}
}

/* set g->drawing->size to a reasonable default.
 * returns a boolean to indicate if drawing is to
 * be scaled and filled */
int idealsize(graph_t* g, double minallowed)
{
	double		xf,yf,f,R;
	point		b,relpage,margin;

	/* try for one page */
	relpage = GD_drawing(g)->page;
	if (relpage.x == 0) return FALSE;				/* no page was specified */
	margin = GD_drawing(g)->margin;
	relpage = sub_points(relpage,margin);
	relpage = sub_points(relpage,margin);
	b.x = GD_bb(g).UR.x; b.y = GD_bb(g).UR.y;
	xf = (double)relpage.x / b.x;
	yf = (double)relpage.y / b.y;
	if ((xf >= 1.0) && (yf >= 1.0)) return FALSE;	/* fits on one page */

	f = MIN(xf,yf);
	xf = yf = MAX(f,minallowed);

	R = ceil((xf * b.x)/relpage.x);
	xf = ((R * relpage.x) / b.x);
	R = ceil((yf * b.y)/relpage.y);
	yf = ((R * relpage.y) / b.y);
	GD_drawing(g)->size.x = b.x * xf;
	GD_drawing(g)->size.y = b.y * yf;
	return TRUE;
}

/*
 * recursively compute cluster ht requirements.  assumes GD_ht1(subg) and ht2
 * are computed from primitive nodes only.  updates ht1 and ht2 to reflect
 * cluster nesting and labels.  also maintains global rank ht1 and ht2.
 */
static void
clust_ht(Agraph_t *g)
{
	int	c, ht1, ht2;
	graph_t	*subg;
	rank_t	*rank = GD_rank(g->root);

	ht1 = GD_ht1(g);
	ht2 = GD_ht2(g);

	/* account for sub-clusters */
	for (c = 1; c <= GD_n_cluster(g); c++) {
		subg = GD_clust(g)[c];
		clust_ht(subg);
		if (GD_maxrank(subg) == GD_maxrank(g))
			ht1 = MAX(ht1,GD_ht1(subg) + CL_OFFSET);
		if (GD_minrank(subg) == GD_minrank(g))
			ht2 = MAX(ht2,GD_ht2(subg) + CL_OFFSET);
	}

	/* account for a possible cluster label in clusters */
    /* room for root graph label is handled in dotneato_postprocess */
    if (g != g->root) {
		ht1 += GD_border(g)[BOTTOM_IX].y;
		ht2 += GD_border(g)[TOP_IX].y;
	}
	GD_ht1(g) = ht1;
	GD_ht2(g) = ht2;

	/* update the global ranks */
	if (g != g->root) {
		rank[GD_minrank(g)].ht2 = MAX(rank[GD_minrank(g)].ht2,ht2);
		rank[GD_maxrank(g)].ht1 = MAX(rank[GD_maxrank(g)].ht1,ht1);
	}
}
