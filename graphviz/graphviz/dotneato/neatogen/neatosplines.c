/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "neato.h"
#include "pathplan.h"
#include "vispath.h"
#ifndef HAVE_DRAND48
extern double drand48(void);
#endif


#define P2PF(p, pf) (pf.x = p.x, pf.y = p.y)
#define PF2P(pf, p) (p.x = ROUND (pf.x), p.y = ROUND (pf.y))         

extern void printvis (vconfig_t *cp);
static void place_portlabel (edge_t *e, boolean head_p);
static splines *getsplinepoints (edge_t* e);
extern int	in_poly(Ppoly_t argpoly, Ppoint_t q);

static boolean spline_merge(node_t *n)
{
    return FALSE;
}

static boolean swap_ends_p (edge_t *e)
{
	return FALSE;
}

static splineInfo sinfo = {swap_ends_p, spline_merge};

void neato_compute_bb(graph_t *g)
{
	node_t		*n;
	edge_t		*e;
	box			b,bb;
	point		pt,s2;
	int		i,j;

	bb.LL = pointof(MAXINT,MAXINT);
	bb.UR = pointof(-MAXINT,-MAXINT);
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		pt = coord(n);
		s2.x = ND_xsize(n)/2+1; s2.y = ND_ysize(n)/2+1;
		b.LL = sub_points(pt,s2);
		b.UR = add_points(pt,s2);

		bb.LL.x = MIN(bb.LL.x,b.LL.x);
		bb.LL.y = MIN(bb.LL.y,b.LL.y);
		bb.UR.x = MAX(bb.UR.x,b.UR.x);
		bb.UR.y = MAX(bb.UR.y,b.UR.y);
		for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
			if (ED_spl(e) == 0) continue;
			for (i = 0; i < ED_spl(e)->size; i++) {
				for (j = 0; j < ED_spl(e)->list[i].size; j++) {
					pt = ED_spl(e)->list[i].list[j];
					if (bb.LL.x > pt.x) bb.LL.x = pt.x;
					if (bb.LL.y > pt.y) bb.LL.y = pt.y;
					if (bb.UR.x < pt.x) bb.UR.x = pt.x;
					if (bb.UR.y < pt.y) bb.UR.y = pt.y;
				}
			}
		}
	}

    for (i = 1; i <= GD_n_cluster(g); i++) {
      bb.LL.x = MIN(bb.LL.x,GD_clust(g)[i]->u.bb.LL.x);
      bb.LL.y = MIN(bb.LL.y,GD_clust(g)[i]->u.bb.LL.y);
      bb.UR.x = MAX(bb.UR.x,GD_clust(g)[i]->u.bb.UR.x);
      bb.UR.y = MAX(bb.UR.y,GD_clust(g)[i]->u.bb.UR.y);
    }

	GD_bb(g) = bb;
}


static Ppoint_t mkPoint(point p) {Ppoint_t rv; rv.x = p.x; rv.y = p.y; return rv;}


static void
make_barriers(Ppoly_t **poly, int npoly, int pp, int qp, Pedge_t **barriers, int *n_barriers){
    int     i, j, k, n, b;
    Pedge_t *bar;

    n = 0;
    for (i = 0; i < npoly; i++) {
        if (i == pp) continue;
        if (i == qp) continue;
        n = n + poly[i]->pn;
    }
    bar = N_GNEW(n, Pedge_t);
    b = 0;
    for (i = 0; i < npoly; i++) {
        if (i == pp) continue;
        if (i == qp) continue;
        for (j = 0; j < poly[i]->pn; j++) {
            k = j + 1;
            if (k >= poly[i]->pn) k = 0;
            bar[b].a = poly[i]->ps[j];
            bar[b].b = poly[i]->ps[k];
            b++;
        }
    }
    assert(b == n);
    *barriers = bar;
    *n_barriers = n;
}

extern int Plegal_arrangement( Ppoly_t	**polys, int	n_polys);

/* recPt:
 */
static Ppoint_t
recPt (double x, double y, point c, double sep)
{
	Ppoint_t	p;

	p.x = (x * sep) + c.x;
	p.y = (y * sep) + c.y;
	return p;
}

/* updateBB:
 * Reset graphs bounding box to include bounding box of the given label.
 * Asxume the label's position has been set.
 */
static void
updateBB (graph_t* g, textlabel_t* lp)
{
	box      bb = GD_bb(g);
	int      width = ROUND(POINTS(lp->dimen.x));
	int      height = ROUND(POINTS(lp->dimen.y));
	point    p = lp->p;
	int      min, max;

	min = p.x - width/2;
	max = p.x + width/2;
	if (min < bb.LL.x) bb.LL.x = min;
	if (max > bb.UR.x) bb.UR.x = max;
	
	min = p.y - height/2;
	max = p.y + height/2;
	if (min < bb.LL.y) bb.LL.y = min;
	if (max > bb.UR.y) bb.UR.y = max;
	
    GD_bb(g) = bb;
}

/* spline_edges0:
 * Main body for constructing edges.
 * Assumes u.bb for has been computed for g and all clusters
 * (not just top-level clusters), and 
 * that GD_bb(g).LL is at the origin.
 *
 * This last criterion is, I believe, mainly to simplify the code
 * in neato_set_aspect. It would be good to remove this constraint,
 * as this would allow nodes pinned on input to have the same coordinates
 * when output in dot or plain format.
 *
 * As a side-effect, this function sets the u.coord attribute of nodes
 * from the u.pos attribute. This is needed for output. In addition, 
 * it guarantees that all bounding
 * boxes are current; in particular, the bounding box of g reflects the
 * addition of edges. NOTE: intra-cluster edges are not constrained to
 * remain in the cluster's bounding box and, conversely, a cluster's box
 * is not altered to reflect intra-cluster edges.
 * 
 * Note that, currently, edge labels are not used in bounding box 
 * calculations.
 */
void spline_edges0(graph_t *g)
{
	node_t		*n;
	edge_t		*e;
	point		dumb[4],d,ld;
	pointf		polyp;
	Ppoly_t		**obs;
	polygon_t	*poly;
	int 		i=0, j, npoly, sides;
	extern void	poly_init(GVC_t *);
	Ppoint_t	p,q;
	vconfig_t	*vconfig;
	Pedge_t     *barriers;
	double		adj=0.0, SEP;
	char		*str;

	if ((str = agget(g,"sep"))) {SEP = 1.0 + atof(str);}
	else SEP = 1.01;
	neato_set_aspect(g);
	
	/* build configuration */
	if (mapbool(agget(g,"splines"))) {
		obs = N_NEW (agnnodes(g), Ppoly_t*);
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
			ND_coord_i(n).x = POINTS(ND_pos(n)[0]);
			ND_coord_i(n).y = POINTS(ND_pos(n)[1]);
	
			if (ND_shape(n)->fns->initfn == poly_init) {
				obs[i] = NEW(Ppoly_t);
				poly = (polygon_t*) ND_shape_info(n);
				if (poly->sides >= 3) {
					sides = poly->sides;
				}
				else {	/* ellipse */
					sides = 8;
					adj = drand48() * .01;
				}
				obs[i]->pn = sides;
				obs[i]->ps = N_NEW(sides, Ppoint_t);
				/* assuming polys are in CCW order, and pathplan needs CW */
				for (j = 0; j < sides; j++) {
					if (poly->sides >= 3) {
						polyp.x = POINTS(poly->vertices[j].x) * SEP;
						polyp.y = POINTS(poly->vertices[j].y) * SEP;
					}
					else {
						double	c, s;
						c = cos(2.0 * PI * j / sides + adj);
						s = sin(2.0 * PI * j / sides + adj);
						polyp.x = SEP * c * (ND_lw_i(n) + ND_rw_i(n)) / 2.0;
						polyp.y = SEP * s * ND_ht_i(n) / 2.0;
					}
					obs[i]->ps[sides - j - 1].x = polyp.x + ND_coord_i(n).x;
					obs[i]->ps[sides - j - 1].y = polyp.y + ND_coord_i(n).y;
				}
				i++;
			}
			else if (ND_shape(n)->fns->initfn == record_init) {
				box     b;
				point   pt;
				field_t	*fld = (field_t*) ND_shape_info(n);
				b = fld->b;
				obs[i] = NEW(Ppoly_t);
				obs[i]->pn = 4;
				obs[i]->ps = N_NEW(4, Ppoint_t);
				/* CW order */
				pt = ND_coord_i(n);
				obs[i]->ps[0] = recPt (b.LL.x,b.LL.y, pt, SEP);
				obs[i]->ps[1] = recPt (b.LL.x,b.UR.y, pt, SEP);
				obs[i]->ps[2] = recPt (b.UR.x,b.UR.y, pt, SEP);
				obs[i]->ps[3] = recPt (b.UR.x,b.LL.y, pt, SEP);
				i++;
			}
		}
	}
	else {
    	obs = 0;
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
			ND_coord_i(n).x = POINTS(ND_pos(n)[0]);
			ND_coord_i(n).y = POINTS(ND_pos(n)[1]);
    	}
    }
	npoly = i;
	if (obs && NOT(Plegal_arrangement(obs,npoly))) {
		if (Verbose) fprintf(stderr,"nodes touch - falling back to straight line edges\n");
		vconfig = 0;
	}
	else {
		if (obs) vconfig = Pobsopen(obs,npoly);
		else vconfig = 0;
	}

	/* route edges  */
	if (Verbose) fprintf(stderr,"Creating edges using %s\n",
      (vconfig ? "splines" : "line segments"));
	if (vconfig) {
		/* path-finding pass */
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
			for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
				Ppolyline_t line;
				int			pp, qp;

				p = mkPoint(add_points(ND_coord_i(n), ED_tail_port(e).p));
				q = mkPoint(add_points(ND_coord_i(e->head), ED_head_port(e).p));

			   /* determine the polygons (if any) that contain the endpoints */
				pp = qp = POLYID_NONE;
				for (i = 0; i < npoly; i++) {
					if ((pp == POLYID_NONE) && in_poly(*obs[i], p)) pp = i;
					if ((qp == POLYID_NONE) && in_poly(*obs[i], q)) qp = i;
				}
				/*Pobspath(vconfig, p, POLYID_UNKNOWN, q, POLYID_UNKNOWN, &line);*/
				Pobspath(vconfig, p, pp, q, qp, &line);
				ED_path(e) = line;
			}
		}
	}

	/* spline-drawing pass */
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
			node_t*    head = e->head;
            if ((Nop > 1) && ED_spl(e)) {
				p = mkPoint(add_points(ND_coord_i(n), ED_tail_port(e).p));
				q = mkPoint(add_points(ND_coord_i(head), ED_head_port(e).p));
            }
			else if (vconfig && (n != head)) {
				Ppolyline_t line, spline;
				Pvector_t	slopes[2];
				int			n_barriers;
				int			pp, qp;
				point		*ispline;

				line = ED_path(e);
				p = line.ps[0];
				q = line.ps[line.pn-1];
			   /* determine the polygons (if any) that contain the endpoints */
				pp = qp = POLYID_NONE;
				for (i = 0; i < npoly; i++) {
					if ((pp == POLYID_NONE) && in_poly(*obs[i], p)) pp = i;
					if ((qp == POLYID_NONE) && in_poly(*obs[i], q)) qp = i;
				}

				make_barriers(obs, npoly, pp, qp, &barriers, &n_barriers);
				slopes[0].x = slopes[0].y = 0.0;
				slopes[1].x = slopes[1].y = 0.0;
				Proutespline (barriers, n_barriers, line, slopes, &spline);

				/* north why did you ever use int coords */
				ispline = N_GNEW(spline.pn,point);
				for (i = 0; i < spline.pn; i++) {
					ispline[i].x = ROUND(spline.ps[i].x);
					ispline[i].y = ROUND(spline.ps[i].y);
				}
				if (Verbose > 1) 
					fprintf(stderr,"spline %s %s\n",n->name,head->name);
				clip_and_install(e,e,ispline,spline.pn,&sinfo);
				free(ispline);
				free(barriers);
			}
			else {
				dumb[0] = add_points(ND_coord_i(n), ED_tail_port(e).p);
				dumb[3] = add_points(ND_coord_i(head), ED_head_port(e).p);
				p = mkPoint(dumb[0]);
				q = mkPoint(dumb[3]);
				if (n != head) {
					d = sub_points(dumb[3],dumb[0]);
					d.x = d.x / 3;
					d.y = d.y / 3;
					dumb[1] = add_points(dumb[0],d);
					dumb[2] = sub_points(dumb[3],d);
				}
				else {	/* self arc */
					pointf del;
					del.x = dumb[0].x - dumb[3].x;
					del.y = dumb[0].y - dumb[3].y;
					if ((del.x == 0) && (del.y == 0)) {
						d.x = ND_rw_i(head) + POINTS(.66 * SEP);
						d.y = 0;
						dumb[1] = dumb[2] = add_points(dumb[0],d);
						dumb[1].y += d.x;
						dumb[2].y -= d.x;
					}
					else {
						pointf perp, base;
						double l_del, l_perp, sz, l, L;
						perp.x = -del.y;
						perp.y = del.x;
						l_del = sqrt(del.x*del.x + del.y*del.y);
						l_perp = sqrt(perp.x*perp.x + perp.y*perp.y);
						if (abs(del.y) <= abs(del.x)) {  /* horizontal */
							sz = ND_ht_i(head)/2.0;
							if (del.y >= ND_coord_i(n).y) {
								if (perp.y < 0) {
									perp.y *= -1;
									perp.x *= -1;
								}
							}
							else {
								if (perp.y > 0) {
									perp.y *= -1;
									perp.x *= -1;
								}
							}
						}
						else {  /* vertical */
							sz = ND_rw_i(head);
							if (del.x >= ND_coord_i(n).x) {
								if (perp.x < 0) {
									perp.x *= -1;
									perp.y *= -1;
								}
							}
							else {
								if (perp.x > 0) {
									perp.y *= -1;
									perp.x *= -1;
								}
							}
						}
						l = sz + POINTS(.66 * SEP);
						L = l/l_del + 0.5;
						base.x = dumb[3].x + (del.x/2) + (l*perp.x)/l_perp;
						base.y = dumb[3].y + (del.y/2) + (l*perp.y)/l_perp;
						dumb[1].x = base.x + L*del.x;
						dumb[1].y = base.y + L*del.y;
						dumb[2].x = base.x - L*del.x;
						dumb[2].y = base.y - L*del.y;
					}
				}
				clip_and_install(e,e,dumb,4,&sinfo);
			}

            /* This can only hope to work for straight edges.
             * FIX for loops and curves; also if nop > 1, use
             * label position if provided
             */
			if (ED_label(e) && !ED_label(e)->set) {
				d.x = (q.x + p.x)/ 2;
				d.y = (p.y + q.y)/ 2;
				if (abs(p.x - q.x) > abs(p.y - q.y)) {
					ld.x = 0; ld.y = POINTS(ED_label(e)->dimen.y)/2 + 2;
				}
				else {
					ld.x = POINTS(ED_label(e)->dimen.y)/2+ED_label(e)->fontsize;
					ld.y = 0;
				}
				d = add_points(d,ld);
				ED_label(e)->p = d;
				ED_label(e)->set = TRUE;
				updateBB(e->tail->graph, ED_label(e));
			}
		}
	}
	/* GD_bb(g).UR = sub_points(GD_bb(g).UR,GD_bb(g).LL); */
	/* GD_bb(g).LL = pointof(0,0); */
	/* We do not need to recompute the bounding box here because
	 * it was computed before entry, and a side-effect of adding edges
	 * with clip_and_install is to update the bounding box.
	 */
	/* neato_compute_bb(g); */

    /* vladimir: place port labels */
    if (E_headlabel || E_taillabel)
      for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
        if (E_headlabel) for (e = agfstin(g,n); e; e = agnxtin(g,e))
          if (ED_head_label(e)) place_portlabel (e, TRUE);
        if (E_taillabel) for (e = agfstout(g,n); e; e = agnxtout(g,e))
          if (ED_tail_label(e)) place_portlabel (e, FALSE);
      }
    /* end vladimir */
	State = GVSPLINES;
}

/* spline_edges:
 * Construct all edges. We assume the graph
 * has no clusters, and only nodes have been
 * positioned.
 */
void spline_edges(graph_t *g)
{
	node_t		*n;
	pointf		offset;

	neato_compute_bb(g);
	offset = cvt2ptf(GD_bb(g).LL);
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		ND_pos(n)[0] -= offset.x;
		ND_pos(n)[1] -= offset.y;
	}
	GD_bb(g).UR.x -= GD_bb(g).LL.x;
	GD_bb(g).UR.y -= GD_bb(g).LL.y;
	GD_bb(g).LL.x = 0;
	GD_bb(g).LL.y = 0;
    spline_edges0(g);
}

/* scaleEdge:
 * Scale edge by given factor.
 * Assume ED_spl != NULL.
 */
static void 
scaleEdge(edge_t *e, double xf, double yf)
{
	int      i, j;
	point*   pt;
	bezier*  bez;

	bez = ED_spl(e)->list;
	for (i = 0; i < ED_spl(e)->size; i++) {
		pt = bez->list;
		for (j = 0; j < bez->size; j++) {
			pt->x *= xf;
			pt->y *= yf;
			pt++;
		}
		if (bez->sflag) {
			bez->sp.x *= xf;
			bez->sp.y *= yf;
		}
		if (bez->eflag) {
			bez->ep.x *= xf;
			bez->ep.y *= yf;
		}
		bez++;
	}

	if (ED_label(e) && ED_label(e)->set) {
		ED_label(e)->p.x *= xf;
		ED_label(e)->p.y *= yf;
	}
	if (ED_head_label(e) && ED_head_label(e)->set) {
		ED_head_label(e)->p.x *= xf;
		ED_head_label(e)->p.y *= yf;
	}
	if (ED_tail_label(e) && ED_tail_label(e)->set) {
		ED_tail_label(e)->p.x *= xf;
		ED_tail_label(e)->p.y *= yf;
	}
}

/* scaleBB:
 * Scale bounding box of clusters of g by given factors.
 */
static void 
scaleBB(graph_t *g, double xf, double yf)
{
	int		i;

	GD_bb(g).UR.x *= xf;
	GD_bb(g).UR.y *= yf;
	GD_bb(g).LL.x *= xf;
	GD_bb(g).LL.y *= yf;

	if (GD_label(g) && GD_label(g)->set) {
		GD_label(g)->p.x *= xf;
		GD_label(g)->p.y *= yf;
	}

    for (i = 1; i <= GD_n_cluster(g); i++)
      scaleBB(GD_clust(g)[i],xf,yf);
}

/* neato_set_aspect;
 * Assume all bounding boxes are correct and
 * that GD_bb(g).LL is at origin.
 */
void 
neato_set_aspect(graph_t *g)
{
	/* int		i; */
	double	xf,yf,actual,desired;
	char	*str;
	node_t	*n;

	/* neato_compute_bb(g); */
	if (/* (GD_maxrank(g) > 0) && */(str = agget(g,"ratio"))) {
			/* normalize */
        assert (GD_bb(g).LL.x == 0);
        assert (GD_bb(g).LL.y == 0);
		/* GD_bb(g).UR.x -= GD_bb(g).LL.x; */
		/* GD_bb(g).UR.y -= GD_bb(g).LL.y; */
		if (GD_left_to_right(g))
			{int t = GD_bb(g).UR.x; GD_bb(g).UR.x = GD_bb(g).UR.y; GD_bb(g).UR.y = t;}
		if (strcmp(str,"fill") == 0) {
			/* fill is weird because both X and Y can stretch */
			if (GD_drawing(g)->size.x <= 0) return;
			xf = (double)GD_drawing(g)->size.x / (double)GD_bb(g).UR.x;
			yf = (double)GD_drawing(g)->size.y / (double)GD_bb(g).UR.y;
			if ((xf < 1.0) || (yf < 1.0)) {
				if (xf < yf) {yf = yf / xf; xf = 1.0;}
				else {xf = xf / yf; yf = 1.0;}
			}
		}
		else {
			desired = atof(str);
			if (desired == 0.0) return;
			actual = ((double)GD_bb(g).UR.y)/((double)GD_bb(g).UR.x);
			if (actual < desired) {yf = desired/actual; xf = 1.0;}
			else {xf = actual/desired; yf = 1.0;}
		}
		if (GD_left_to_right(g)) {double t = xf; xf = yf; yf = t;}

		/* Not relying on neato_nlist here allows us not to have to 
         * allocate it in the root graph and the connected components. 
		 */
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		/* for (i = 0; (n = GD_neato_nlist(g)[i]); i++) { */
			ND_pos(n)[0] = ND_pos(n)[0] * xf;
			ND_pos(n)[1] = ND_pos(n)[1] * yf;
		}
        scaleBB (g, xf, yf);
        if (Nop > 1) {
			edge_t*  e;
			for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
        		for (e = agfstout(g,n); e; e = agnxtout(g,e)) 
					if (ED_spl(e)) scaleEdge (e, xf, yf);
			}
        }
	}
}

/* vladimir */
static void place_portlabel (edge_t *e, boolean head_p)
/* place the {head,tail}label (depending on HEAD_P) of edge E */
{
  textlabel_t *l;
  splines *spl;
  bezier *bez;
  double dist, angle;
  point p;
  pointf c[4], pf;
  int i;

  l = head_p ? ED_head_label(e) : ED_tail_label(e);
  if (swap_ends_p(e)) head_p = !head_p;
  spl = getsplinepoints(e);
  if (!head_p) {
    bez = &spl->list[0];
    if (bez->sflag) {
      p = bez->sp; 
      P2PF(bez->list[0],pf);
    }
    else {
      p = bez->list[0];
      for (i=0; i<4; i++) P2PF(bez->list[i], c[i]);
      pf = Bezier (c, 3, 0.1, NULL, NULL);
    }
  } else {
    bez = &spl->list[spl->size-1];
    if (bez->eflag) {
      p = bez->ep; 
      P2PF(bez->list[bez->size-1],pf);
    }
    else {
      p = bez->list[bez->size-1];  
      for (i=0; i<4; i++) P2PF(bez->list[bez->size-4+i], c[i]);
      pf = Bezier (c, 3, 0.9, NULL, NULL);
    }
  }
  angle = atan2 (pf.y-p.y, pf.x-p.x) + 
    RADIANS(late_double(e,E_labelangle,PORT_LABEL_ANGLE,-180.0));
  dist = PORT_LABEL_DISTANCE * late_double(e,E_labeldistance,1.0,0.0);
  l->p.x = p.x + ROUND(dist * cos(angle));
  l->p.y = p.y + ROUND(dist * sin(angle));
  l->set = TRUE;
  updateBB(e->tail->graph, l);
}

static splines *getsplinepoints (edge_t* e)
{
	splines *sp;

	sp = ED_spl(e);
	if (sp == NULL) abort ();
	return sp;
}
