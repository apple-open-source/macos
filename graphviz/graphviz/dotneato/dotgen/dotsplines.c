#pragma prototyped
/*
 * Copyright (c) AT&T Corp. 1994, 1995.
 * This code is licensed by AT&T Corp.  For the
 * terms and conditions of the license, see
 * http://www.research.att.com/orgs/ssr/book/reuse
 */

/*
 * set edge splines.
 */

#include "dot.h"


#define	NSUB	9	/* number of subdivisions, re-aiming splines */
#define	CHUNK	128	/* in building list of edges */

#define MINW 16		/* minimum width of a box in the edge path */
#define HALFMINW 8

#define REGULAREDGE 1
#define FLATEDGE    2
#define SELFWPEDGE  4
#define SELFNPEDGE  8
#define SELFEDGE    8
#define EDGETYPEMASK	 15	/* the OR of the above */
#define FWDEDGE    16
#define BWDEDGE    32
#define EDGEDIRMASK	 48	/* the OR of the above */
#define MAINGRAPH  64
#define AUXGRAPH  128
#define GRAPHTYPEMASK	192	/* the OR of the above */
#define TPORT(e) ((e)->attr[1])
#define HPORT(e) ((e)->attr[2])

#define MAKEFWDEDGE(new, old) { \
	edge_t *newp; \
	newp = new; \
	*newp = *old; \
	newp->tail = old->head; \
	newp->head = old->tail; \
	ED_tail_port(newp) = ED_head_port(old); \
	ED_head_port(newp) = ED_tail_port(old); \
	ED_edge_type(newp) = VIRTUAL; \
	ED_to_orig(newp) = old; \
}

#define CCW  -1 /* counter clock-wise */
#define CW    1 /* clock-wise */
#define ANYW  0 /* could go either way */

#define OTHERDIR(dir) ((dir == CCW) ? CW : CCW)

#define P2PF(p, pf) (pf.x = p.x, pf.y = p.y)

static int selfsidemap[16][3] = {
    { BOTTOM, BOTTOM, ANYW },
    {    TOP,    TOP, ANYW },
    {  RIGHT,  RIGHT, ANYW },
    {   LEFT,   LEFT, ANYW },
    { BOTTOM,   LEFT,  CCW },
    {   LEFT, BOTTOM,   CW },
    {    TOP,  RIGHT,   CW },
    {  RIGHT,    TOP,  CCW },
    {    TOP,   LEFT,  CCW },
    {   LEFT,    TOP,   CW },
    { BOTTOM,  RIGHT,  CCW },
    {  RIGHT, BOTTOM,   CW },
    { BOTTOM,    TOP,  CCW },
    {    TOP, BOTTOM,   CW },
    {   LEFT,  RIGHT,  CCW },
    {  RIGHT,   LEFT,   CW },
};

static int flatsidemap[16][6] = {
    { BOTTOM, BOTTOM, BOTTOM, CCW, CCW, FALSE },
    {    TOP,    TOP,    TOP,  CW,  CW, FALSE },
    {  RIGHT,   LEFT, BOTTOM,  CW,  CW,  TRUE },
    { BOTTOM,    TOP,  RIGHT, CCW,  CW,  TRUE },
    {    TOP, BOTTOM,  RIGHT,  CW, CCW,  TRUE },
    {  RIGHT,    TOP,  RIGHT, CCW,  CW,  TRUE },
    {  RIGHT, BOTTOM,  RIGHT,  CW, CCW,  TRUE },
    {    TOP,   LEFT,    TOP,  CW, CCW,  TRUE },
    { BOTTOM,   LEFT, BOTTOM, CCW,  CW,  TRUE },
    {  RIGHT,  RIGHT, BOTTOM,  CW, CCW,  TRUE },
    {   LEFT,   LEFT, BOTTOM, CCW,  CW,  TRUE },
    {   LEFT, BOTTOM, BOTTOM, CCW, CCW, FALSE },
    {    TOP,  RIGHT,    TOP,  CW,  CW, FALSE },
    {   LEFT,    TOP,    TOP,  CW,  CW, FALSE },
    { BOTTOM,  RIGHT, BOTTOM, CCW, CCW, FALSE },
    {   LEFT,  RIGHT, BOTTOM, CCW, CCW, FALSE },
};

#define NEXTSIDE(side, dir) ((dir == CCW) ? \
		((side & 0x8) ? BOTTOM : (side << 1)) : \
		((side & 0x1) ? LEFT : (side >> 1)))

#define AVG(a, b) ((a + b) / 2)

typedef struct pathend_t {
	box nb; /* the node box */
	point np; /* node port */
	int sidemask;
	int boxn;
	box boxes[20];
} pathend_t;

static path *P;
static int LeftBound, RightBound, /* FlatHeight, */ Splinesep, Multisep;
static box *Rank_box;

static point points[1000], points2[1000];
static int pointn;
static box boxes[1000];

static void add_box(box);
static void adjustregularpath(int, int);
static void adjustselfends(box *, box *, point, int, int);
static void beginpath(Agedge_t *, int, pathend_t *);
static Agedge_t *bot_bound(Agedge_t *, int);
static unsigned char pathscross(Agnode_t *, Agnode_t *, Agedge_t *, Agedge_t *);
static void chooseflatsides(pathend_t *, pathend_t *, int *, int *, int *, int *, int *, int *);
static void chooseselfsides(pathend_t *, pathend_t *, int *, int *, int *);
static Agraph_t *cl_bound(Agnode_t *, Agnode_t *);
static int cl_vninside(Agraph_t *, Agnode_t *);
static void completeflatpath(pathend_t *, pathend_t *, int, int, int, int, int, box*, box*, int, int);
static void completeregularpath(Agedge_t *, Agedge_t *, pathend_t *, pathend_t *, box *, int, int);
static void completeselfpath(pathend_t *, pathend_t *, int, int, int, int, int, int, int);
static double conc_slope(Agnode_t *);
static double dist(pointf, pointf);
static int edgecmp(Agedge_t **, Agedge_t **);
static void endpath(Agedge_t *, int, pathend_t *);
static Agedge_t *getmainedge(Agedge_t *);
static splines *getsplinepoints(Agedge_t *);
static box makeflatcomponent(box, box, int, int, int, int, int);
static void make_flat_edge(Agedge_t **, int, int);
static box makeflatend(box, int, int, box);
static void make_regular_edge(Agedge_t **, int, int);
static void makeregularend(box, int, int, box *);
static box makeselfcomponent(box, int, int, int, int, int);
static void make_self_edge(Agedge_t **, int, int);
static box makeselfend(box, int, int, int, int);
static box maximal_bbox(Agnode_t *, Agedge_t *, Agedge_t *);
static Agnode_t *neighbor(Agnode_t *, Agedge_t *, Agedge_t *, int);
static void place_vnlabel(Agnode_t *);
static void place_portlabel (edge_t *e, boolean head_p);
static box rank_box(Agraph_t *, int);
static void recover_slack(Agedge_t *, path *);
static void resize_vn(Agnode_t *, int, int, int);
static void setflags(Agedge_t *, int, int, int);
static int straight_len(Agnode_t *);
static Agedge_t *straight_path(Agedge_t *, int, point *, int *);
static Agedge_t *top_bound(Agedge_t *, int);

#define GROWEDGES (edges = ALLOC (n_edges + CHUNK, edges, edge_t*))

static boolean spline_merge(node_t* n)
{
	return ((ND_node_type(n) == VIRTUAL) && ((ND_in(n).size > 1) || (ND_out(n).size > 1)));
}

static boolean swap_ends_p (edge_t *e)
{
  	while (ED_to_orig(e)) e = ED_to_orig(e);
	if (ND_rank(e->head) > ND_rank(e->tail)) return FALSE;
	if (ND_rank(e->head) < ND_rank(e->tail)) return TRUE;
	if (ND_order(e->head) >= ND_order(e->tail)) return FALSE;
	return TRUE;
}

static splineInfo sinfo = {swap_ends_p, spline_merge};

int portcmp(port p0, port p1)
{
        int             rv;
        if (p1.defined == FALSE)
                return (p0.defined ? 1 : 0);
        if (p0.defined == FALSE)
                return -1;
        rv = p0.p.x - p1.p.x;
        if (rv == 0) rv = p0.p.y - p1.p.y;
        return rv;
}

/* swap_bezier:
 */
static void
swap_bezier (bezier* old, bezier *new)
{
	point*   list;
	point*   lp;
	point*   olp;
	int      i, sz;

	sz = old->size;
	list = N_GNEW(sz, point);
	lp = list;
	olp = old->list + (sz-1);
	for (i = 0; i < sz; i++) {  /* reverse list of points */
		*lp++ = *olp--;
	}

	new->list = list;
	new->size = sz;
	new->sflag = old->eflag;
	new->eflag = old->sflag;
	new->sp = old->ep;
	new->ep = old->sp;
}

/* swap_spline:
 */
static void
swap_spline (splines* s)
{
	bezier*    list;
	bezier*    lp;
	bezier*    olp;
	int        i, sz;
	
	sz = s->size;
	list = N_GNEW(sz, bezier);
	lp = list;
	olp = s->list + (sz-1);
	for (i = 0; i < sz; i++) {  /* reverse and swap list of beziers */
		swap_bezier (olp--, lp++);
	}

	/* free old structures */
	for (i = 0; i < sz; i++) free(s->list[i].list);
	free(s->list);
	
	s->list = list;
}

/* edge_normalize:
 */
static void
edge_normalize (graph_t* g)
{
  edge_t*  e;
  node_t*  n;

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
		if (swap_ends_p(e) && ED_spl(e)) swap_spline (ED_spl(e));
    }
  }
}
void dot_splines (graph_t* g)
{
	int i, j, k, n_nodes, n_edges, ind, cnt;
	node_t *n;
	edge_t fwdedgea, fwdedgeb;
	edge_t *e, *e0, *e1, *ea, *eb, *le0, *le1, **edges;

	mark_lowclusters(g);
	routesplinesinit();
	P = NEW(path);
	/* FlatHeight = 2 * GD_nodesep(g); */
	Splinesep = GD_nodesep(g) / 4;
	Multisep = GD_nodesep(g);
	edges = N_NEW (CHUNK, edge_t*);

	/* compute boundaries and list of splines */
	LeftBound = RightBound = 0;
	n_edges = n_nodes = 0;
	for (i = GD_minrank(g); i <= GD_maxrank(g); i++) {
		n_nodes += GD_rank(g)[i].n;
		if ((n = GD_rank(g)[i].v[0]))
			LeftBound = MIN (LeftBound, (ND_coord_i(n).x - ND_lw_i(n)));
		if (GD_rank(g)[i].n && (n = GD_rank(g)[i].v[GD_rank(g)[i].n - 1]))
			RightBound = MAX (RightBound, (ND_coord_i(n).x + ND_rw_i(n)));

		for (j = 0; j < GD_rank(g)[i].n; j++) {
			n = GD_rank(g)[i].v[j];
			if ((ND_node_type(n) != NORMAL) &&
					(spline_merge(n) == FALSE))
				continue;
			for (k = 0; (e = ND_out(n).list[k]); k++) {
				if ((ED_edge_type(e) == FLATORDER) || (ED_edge_type(e) == IGNORED))
					continue;
				setflags (e, REGULAREDGE, FWDEDGE, MAINGRAPH);
				edges[n_edges++] = e;
				if (n_edges % CHUNK == 0)
					GROWEDGES;
			}
			if (ND_flat_out(n).list)
				for (k = 0; (e = ND_flat_out(n).list[k]); k++) {
					setflags (e, FLATEDGE, 0, AUXGRAPH);
					edges[n_edges++] = e;
					if (n_edges % CHUNK == 0)
						GROWEDGES;
				}
			if (ND_other(n).list)
				for (k = 0; (e = ND_other(n).list[k]); k++) {
					setflags (e, 0, 0, AUXGRAPH);
					edges[n_edges++] = e;
					if (n_edges % CHUNK == 0)
						GROWEDGES;
				}
		}
	}

	qsort((char*) &edges[0], n_edges, sizeof (edges[0]), (qsort_cmpf)edgecmp);

	/* FIXME: just how many boxes can there be? */
	P->boxes = N_NEW (n_nodes + 20 * 2 * NSUB, box);
	Rank_box = N_NEW (i, box);

	for (i = 0; i < n_edges; ) {
		ind = i;
		le0 = getmainedge ((e0 = edges[i++]));
		ea = (ED_tail_port(e0).defined || ED_head_port(e0).defined) ? e0 : le0;
		if (ED_tree_index(ea) & BWDEDGE) {
			MAKEFWDEDGE (&fwdedgea, ea);
			ea = &fwdedgea;
		}
		for (cnt = 1; i < n_edges; cnt++, i++) {
			if (le0 != (le1 = getmainedge ((e1 = edges[i]))))
				break;
			eb = (ED_tail_port(e1).defined || ED_head_port(e1).defined) ? e1 : le1;
			if (ED_tree_index(eb) & BWDEDGE) {
				MAKEFWDEDGE (&fwdedgeb, eb);
				eb = &fwdedgeb;
			}
			if (portcmp(ED_tail_port(ea),ED_tail_port(eb))) break;
			if (portcmp(ED_head_port(ea),ED_head_port(eb))) break;
			if ((ED_tree_index(e0) & EDGETYPEMASK) == FLATEDGE && ED_label(e0) != ED_label(e1))
				break;
			if (ED_tree_index(edges[i]) & MAINGRAPH) /* Aha! -C is on */
				break;
		}
		if (e0->tail == e0->head)
			make_self_edge (edges, ind, cnt);
		else if (ND_rank(e0->tail) == ND_rank(e0->head))
			make_flat_edge (edges, ind, cnt);
		else
			make_regular_edge (edges, ind, cnt);
	}

	/* make the other splines and place labels */
	for (n = GD_nlist(g); n; n = ND_next(n)) {
		if ((ND_node_type(n) == VIRTUAL) && (ND_label(n))) place_vnlabel(n);
	}

	/* normalize splines so they always go from tail to head */
	edge_normalize (g);

    /* vladimir: place port labels */
    if (E_headlabel || E_taillabel)
      for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
        if (E_headlabel) for (e = agfstin(g,n); e; e = agnxtin(g,e))
          if (ED_head_label(e)) place_portlabel (e, TRUE);
        if (E_taillabel) for (e = agfstout(g,n); e; e = agnxtout(g,e))
          if (ED_tail_label(e)) place_portlabel (e, FALSE);
      }
    /* end vladimir */

	free (edges);
	free (P->boxes);
	free (P);
	free (Rank_box);
	routesplinesterm ();
	State = GVSPLINES;
}

	/* compute position of an edge label from its virtual node */
static void place_vnlabel(node_t* n)
{
	pointf		dimen;
	double		width;
	edge_t		*e;
	if (ND_in(n).size == 0) return;	/* skip flat edge labels here */
	for (e = ND_out(n).list[0]; ED_edge_type(e) != NORMAL; e = ED_to_orig(e));
	dimen = ED_label(e)->dimen;
	width = GD_left_to_right(n->graph)? dimen.y : dimen.x;
	ED_label(e)->p.x = ND_coord_i(n).x + POINTS(width/2.0);
	ED_label(e)->p.y = ND_coord_i(n).y;
}

/* vladimir */
static void place_portlabel (edge_t *e, boolean head_p)
/* place the {head,tail}label (depending on HEAD_P) of edge E */
/* edges are normalized, so tail is at spl->list[0].list[0]
 * and head is at spl->list[spl->size-l].list[bez->size-1]
 */ 
{
  textlabel_t *l;
  splines *spl;
  bezier *bez;
  double dist, angle;
  point p;
  pointf c[4], pf;
  int i;

  if (ED_edge_type(e) == IGNORED) return;
  l = head_p ? ED_head_label(e) : ED_tail_label(e);
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
}

static void setflags (e, hint1, hint2, f3)
	edge_t *e;
	int hint1, hint2, f3;
{
	int f1, f2;
	if (hint1 != 0)
		f1 = hint1;
	else {
		if (e->tail == e->head)
			if (ED_tail_port(e).defined || ED_head_port(e).defined)
				f1 = SELFWPEDGE;
			else
				f1 = SELFNPEDGE;
		else if (ND_rank(e->tail) == ND_rank(e->head))
			f1 = FLATEDGE;
		else
			f1 = REGULAREDGE;
	}
	if (hint2 != 0)
		f2 = hint2;
	else {
		if (f1 == REGULAREDGE)
			f2 = (ND_rank(e->tail) < ND_rank(e->head)) ?
					FWDEDGE : BWDEDGE;
		else if (f1 == FLATEDGE)
			f2 = (ND_order(e->tail) < ND_order(e->head)) ?
					FWDEDGE : BWDEDGE;
		else /* f1 == SELF*EDGE */
			f2 = FWDEDGE;
	}
	ED_tree_index(e) = (f1 | f2 | f3);
}
			
static int edgecmp (ptr0, ptr1)
	edge_t **ptr0, **ptr1;
{
	edge_t fwdedgea, fwdedgeb, *e0, *e1, *ea, *eb, *le0, *le1;
	int et0, et1, v0, v1, rv;

	e0 = (edge_t *) *ptr0;
	e1 = (edge_t *) *ptr1;
	et0 = ED_tree_index(e0) & EDGETYPEMASK;
	et1 = ED_tree_index(e1) & EDGETYPEMASK;
	if (et0 != et1)
		return (et1 - et0);
	le0 = getmainedge (e0);
	le1 = getmainedge (e1);
	v0 = ND_rank(le0->tail) - ND_rank(le0->head), v0 = ABS (v0);
	v1 = ND_rank(le1->tail) - ND_rank(le1->head), v1 = ABS (v1);
	if (v0 != v1)
		return (v0 - v1);
	v0 = ND_coord_i(le0->tail).x - ND_coord_i(le0->head).x, v0 = ABS (v0);
	v1 = ND_coord_i(le1->tail).x - ND_coord_i(le1->head).x, v1 = ABS (v1);
	if (v0 != v1)
		return (v0 - v1);
	if (le0->id != le1->id)
		return (le0->id - le1->id);
	ea = (ED_tail_port(e0).defined || ED_head_port(e0).defined) ? e0 : le0;
	if (ED_tree_index(ea) & BWDEDGE) {
		MAKEFWDEDGE (&fwdedgea, ea);
		ea = &fwdedgea;
	}
	eb = (ED_tail_port(e1).defined || ED_head_port(e1).defined) ? e1 : le1;
	if (ED_tree_index(eb) & BWDEDGE) {
		MAKEFWDEDGE (&fwdedgeb, eb);
		eb = &fwdedgeb;
	}
	if ((rv = portcmp(ED_tail_port(ea),ED_tail_port(eb)))) return rv;
	if ((rv = portcmp(ED_head_port(ea),ED_head_port(eb)))) return rv;
	v0 = ED_tree_index(e0) & GRAPHTYPEMASK;
	v1 = ED_tree_index(e1) & GRAPHTYPEMASK;
	if (v0 != v1)
		return (v0 - v1);
	if (et0 == FLATEDGE && ED_label(e0) != ED_label(e1))
		return (int) (ED_label(e0) - ED_label(e1));
	return (e0->id - e1->id);
}

static void make_self_edge (edges, ind, cnt)
	edge_t **edges;
	int ind, cnt;
{
	node_t *n;
	edge_t *e;
	point *ps, np;
	pathend_t tend, hend;
	int i, j, maxx, stepx, stepy, dx, dy, tside, hside, dir, pn;
	double width, height;

	e = edges[ind];
	n = e->tail;
	i = ND_rw_i(n), ND_rw_i(n) = ND_mval(n), ND_mval(n) = i;    /* recover original size */

	/* self edge without ports */

	if ((!ED_tail_port(e).defined) && (!ED_head_port(e).defined)) {
		stepx = Multisep, stepy = (ND_ht_i(n) / 2) / cnt;
		pointn = 0;
		np = ND_coord_i(n);
		dx = ND_rw_i(n), dy = 0;
		for (i = 0; i < cnt; i++) {
			e = edges[ind++];
			dx += stepx, dy += stepy;
			pointn = 0;
			points[pointn++] = np;
			points[pointn++] = pointof (np.x + dx / 3, np.y - dy);
			points[pointn++] = pointof (np.x + dx,     np.y - dy);
			points[pointn++] = pointof (np.x + dx,     np.y);
			points[pointn++] = pointof (np.x + dx,     np.y + dy);
			points[pointn++] = pointof (np.x + dx / 3, np.y + dy);
			points[pointn++] = np;
			if (ED_label(e)) {
				if (GD_left_to_right(e->tail->graph)) {
					width = ED_label(e)->dimen.y;
					height = ED_label(e)->dimen.x;
				} else {
					width = ED_label(e)->dimen.x;
					height = ED_label(e)->dimen.y;
				}
				ED_label(e)->p.x = ND_coord_i(n).x + dx + POINTS(width/2.0);
				ED_label(e)->p.y = ND_coord_i(n).y;
				if (POINTS (width) > stepx)
					dx += POINTS(width) - stepx;
				if (dy + stepy < POINTS(height))
					dy += (POINTS(height) - stepy);
			}
			clip_and_install (e, e, points, pointn, &sinfo);
		}
		return;
	}

	/* self edge with ports */

	tend.nb = boxof (ND_coord_i(n).x - ND_lw_i(n), ND_coord_i(n).y - ND_ht_i(n) / 2,
			ND_coord_i(n).x + ND_rw_i(n), ND_coord_i(n).y + ND_ht_i(n) / 2);
	hend.nb = tend.nb;
	stepx = Multisep, stepy = Multisep / 2;
	dx = 0, dy = 0;
	for (i = 0; i < cnt; i++) {
		e = edges[ind++];
		dx += stepx, dy += stepy;

		/* tail setup */
		beginpath (e, SELFEDGE, &tend);

		/* head setup */
		endpath (e, SELFEDGE, &hend);

		chooseselfsides (&tend, &hend, &tside, &hside, &dir);
		completeselfpath (&tend, &hend, tside, hside, dir,
				dx, dy, Multisep, Multisep);

		ps = routesplines (P, &pn);
		if (pn == 0) return;	/* will result in a lost edge */
		if (ED_label(e)) {
			/* FIXME: labels only right for BOTTOM -> TOP edges */
			for (j = 0, maxx = ND_coord_i(n).x; j < P->nbox; j++)
				if (P->boxes[j].UR.x > maxx)
					maxx = P->boxes[j].UR.x;
			if (GD_left_to_right(e->tail->graph))
				width = ED_label(e)->dimen.y;
			else
				width = ED_label(e)->dimen.x;
			ED_label(e)->p.x = maxx + POINTS(width/2.0);
			ED_label(e)->p.y = ND_coord_i(n).y;
			if (POINTS (width) > stepx)
				dx += POINTS (width) - stepx;
		}
		clip_and_install (e, e, ps, pn, &sinfo);
	}
}

static void make_flat_edge (edges, ind, cnt)
	edge_t **edges;
	int ind, cnt;
{
	node_t *tn, *hn, *n;
	edge_t fwdedge, *e;
	int i, stepx, stepy, /* dx, */ dy, ht1,ht2;
	int tside, hside, mside, tdir, hdir, cross, pn;
	point *ps;
	point tp, hp;
	pathend_t tend, hend;
	box lb, rb, wlb, wrb;
	rank_t *rank;

	/* dx = 0; */
	e = edges[ind];
	if (ED_tree_index(e) & BWDEDGE) {
		MAKEFWDEDGE (&fwdedge, e);
		e = &fwdedge;
	}
	tn = e->tail, hn = e->head;

	/* flat edge without ports that can go straight left to right */

	if (ED_label(e)) {
		edge_t		*f;
		for (f = ED_to_virt(e); ED_to_virt(f); f = ED_to_virt(f));
		ED_label(e)->p = ND_coord_i(f->tail);
	}

	if ((!ED_tail_port(e).defined) && (!ED_head_port(e).defined)) {
		rank = &(GD_rank(tn->graph)[ND_rank(tn)]);
		for (i = ND_order(tn) + 1; i < ND_order(hn); i++) {
			n = rank->v[i];
			if ((ND_node_type(n) == VIRTUAL && ND_label(n)) ||
					ND_node_type(n) == NORMAL)
				break;
		}
		if (i != ND_order(hn))
			goto flatnostraight;

		stepy = (cnt > 1) ? ND_ht_i(tn) / (cnt - 1) : 0;
		tp = ND_coord_i(tn);
		hp = ND_coord_i(hn);
		dy = tp.y - ((cnt > 1) ? ND_ht_i(tn) / 2 : 0);
		for (i = 0; i < cnt; i++) {
			e = edges[ind + i];
			pointn = 0;
			points[pointn++] = tp;
			points[pointn++] = pointof ((2 * tp.x + hp.x) / 3, dy);
			points[pointn++] = pointof ((2 * hp.x + tp.x) / 3, dy);
			points[pointn++] = hp;
#ifdef OBSOLETE
			if (ED_label(e)) { /* FIXME: fix label positioning */
				labelw = ED_label(e)->dimen.x * POINTS_PER_INCH;
				ED_label(e)->p = pointof (tp.x + labelw / 2, tp.y);
				/* dx += labelw;*/
			}
#endif
			dy += stepy;
			clip_and_install (e, e, points, pointn, &sinfo);
		}
		return;
	}

	/* !(flat edge without ports that can go straight left to right) */

flatnostraight:
	tend.nb = boxof (ND_coord_i(tn).x - ND_lw_i(tn), ND_coord_i(tn).y - ND_ht_i(tn) / 2,
			ND_coord_i(tn).x + ND_rw_i(tn), ND_coord_i(tn).y + ND_ht_i(tn) / 2);
	hend.nb = boxof (ND_coord_i(hn).x - ND_lw_i(hn), ND_coord_i(hn).y - ND_ht_i(hn) / 2,
			ND_coord_i(hn).x + ND_rw_i(hn), ND_coord_i(hn).y + ND_ht_i(hn) / 2);
	ht1 = GD_rank(tn->graph)[ND_rank(tn)].pht1;
	ht2 = GD_rank(tn->graph)[ND_rank(tn)].pht2;
	stepx = Multisep / cnt, stepy = ht2 / cnt;
	lb = boxof (ND_coord_i(tn).x - ND_lw_i(tn), ND_coord_i(tn).y - ht1,
			ND_coord_i(tn).x + ND_rw_i(tn), ND_coord_i(tn).y + ht2);
	rb = boxof (ND_coord_i(hn).x - ND_lw_i(hn), ND_coord_i(hn).y - ht1,
			ND_coord_i(hn).x + ND_rw_i(hn), ND_coord_i(hn).y + ht2);
	for (i = 0; i < cnt; i++) {
		e = edges[ind + i];
		if (ED_tree_index(e) & BWDEDGE) {
			MAKEFWDEDGE (&fwdedge, e);
			e = &fwdedge;
		}

		/* tail setup */
		beginpath (e, FLATEDGE, &tend);

		/* head setup */
		endpath (e, FLATEDGE, &hend);

		chooseflatsides (&tend, &hend, &tside, &hside, &mside,
				&tdir, &hdir, &cross);
		if (ED_label(e)) { /* edges with labels aren't multi-edges */
			edge_t *le;
			node_t *ln;
			for (le = e; ED_to_virt(le); le = ED_to_virt(le))
				;
			ln = le->tail;
			wlb.LL.x = lb.LL.x;
			wlb.LL.y = lb.LL.y;
			wlb.UR.x = lb.UR.x;
			wlb.UR.y = ND_coord_i(ln).y - ND_ht_i(ln) / 2;
			wrb.LL.x = rb.LL.x;
			wrb.LL.y = rb.LL.y;
			wrb.UR.x = rb.UR.x;
			wrb.UR.y = ND_coord_i(ln).y - ND_ht_i(ln) / 2;
		} else {
			wlb.LL.x = lb.LL.x - (i + 1) * stepx;
			wlb.LL.y = lb.LL.y - (i + 1) * stepy;
			wlb.UR.x = lb.UR.x + (i + 1) * stepx;
			wlb.UR.y = lb.UR.y + (i + 1) * stepy;
			if (cross) {
				wrb.LL.x = rb.LL.x - (cnt - i) * stepx;
				wrb.LL.y = rb.LL.y - (cnt - i) * stepy;
				wrb.UR.x = rb.UR.x + (cnt - i) * stepx;
				wrb.UR.y = rb.UR.y + (cnt - i) * stepy;
			} else {
				wrb.LL.x = rb.LL.x - (i + 1) * stepx;
				wrb.LL.y = rb.LL.y - (i + 1) * stepy;
				wrb.UR.x = rb.UR.x + (i + 1) * stepx;
				wrb.UR.y = rb.UR.y + (i + 1) * stepy;
			}
		}
		completeflatpath (&tend, &hend, tside, hside, mside,
				tdir, hdir, &wlb, &wrb, stepx, stepy);

		ps = routesplines (P, &pn);
if (pn == 0) return;
		clip_and_install (e, e, ps, pn, &sinfo);
	}
}

static void make_regular_edge (edges, ind, cnt)
	edge_t **edges;
	int ind, cnt;
{
	graph_t *g;
	node_t *tn, *hn;
	edge_t fwdedgea, fwdedgeb, fwdedge, *e, *fe, *le, *segfirst;
	point *ps;
	pathend_t tend, hend;
	box b;
	int boxn, sl, si, smode, i, j, dx, pn, hackflag, longedge;

	sl = 0;
	e = edges[ind];
	hackflag = FALSE;
	if (ABS (ND_rank(e->tail) - ND_rank(e->head)) > 1) {
		fwdedgea = *e;
		if (ED_tree_index(e) & BWDEDGE) {
			MAKEFWDEDGE (&fwdedgeb, e);
			fwdedgea.tail = e->head;
			fwdedgea.u.tail_port = ED_head_port(e);
		} else {
			fwdedgeb = *e;
			fwdedgea.tail = e->tail;
		}
		le = getmainedge (e);
		while (ED_to_virt(le)) le = ED_to_virt(le);
		fwdedgea.head = le->head;
		fwdedgea.u.head_port.defined = FALSE;
		fwdedgea.u.edge_type = VIRTUAL;
		fwdedgea.u.head_port.p.x = fwdedgea.u.head_port.p.y = 0;
		fwdedgea.u.to_orig = e;
		e = &fwdedgea;
		hackflag = TRUE;
	} else {
		if (ED_tree_index(e) & BWDEDGE) {
			MAKEFWDEDGE (&fwdedgea, e);
			e = &fwdedgea;
		}
	}
	fe = e;
		
	/* compute the spline points for the edge */

	boxn = 0;
	pointn = 0;
	segfirst = e;
	g = e->tail->graph;
	tn = e->tail;
	hn = e->head;
	tend.nb = maximal_bbox (tn, NULL, e);
	beginpath (e, REGULAREDGE, &tend);
	makeregularend (tend.boxes[tend.boxn - 1], BOTTOM,
		ND_coord_i(tn).y - GD_rank(tn->graph)[ND_rank(tn)].ht1, &b);
	if (b.LL.x < b.UR.x && b.LL.y < b.UR.y)
		tend.boxes[tend.boxn++] = b;
	longedge = 0;
	smode = FALSE, si = -1;
	while (ND_node_type(hn) == VIRTUAL && !spline_merge (hn)) {
		longedge = 1;
		boxes[boxn++] = rank_box (g, ND_rank(tn));
		if (!smode && ((sl = straight_len (hn)) >= ((GD_has_labels(g)&EDGE_LABEL) ? 4 + 1 : 2 +1))) {
			smode = TRUE;
			si = 1, sl -= 2;
		}
		if (!smode || si > 0) {
			si--;
			boxes[boxn++] = maximal_bbox (hn, e, ND_out(hn).list[0]);
			e = ND_out(hn).list[0];
			tn = e->tail;
			hn = e->head;
			continue;
		}
		hend.nb = maximal_bbox (hn, e, ND_out(hn).list[0]);
		endpath (e, REGULAREDGE, &hend);
		makeregularend (hend.boxes[hend.boxn - 1], TOP,
			ND_coord_i(hn).y + GD_rank(hn->graph)[ND_rank(hn)].ht2, &b);
		if (b.LL.x < b.UR.x && b.LL.y < b.UR.y)
			hend.boxes[hend.boxn++] = b;
		P->end.theta = PI / 2, P->end.constrained = TRUE;
		completeregularpath (segfirst, e, &tend, &hend, boxes, boxn, 1);
		ps = routesplines (P, &pn);
if (pn == 0) return;
		for (i = 0; i < pn; i++)
			points[pointn++] = ps[i];
		e = straight_path (ND_out(hn).list[0], sl, points, &pointn);
		recover_slack (segfirst, P);
		segfirst = e;
		tn = e->tail;
		hn = e->head;
		boxn = 0;
		tend.nb = maximal_bbox (tn, ND_in(tn).list[0], e);
		beginpath (e, REGULAREDGE, &tend);
		makeregularend (tend.boxes[tend.boxn - 1], BOTTOM,
			ND_coord_i(tn).y - GD_rank(tn->graph)[ND_rank(tn)].ht1, &b);
		if (b.LL.x < b.UR.x && b.LL.y < b.UR.y)
			tend.boxes[tend.boxn++] = b;
		P->start.theta = - PI / 2, P->start.constrained = TRUE;
		smode = FALSE;
	}
	boxes[boxn++] = rank_box (g, ND_rank(tn));
	hend.nb = maximal_bbox (hn, e, NULL);
	endpath (hackflag ? &fwdedgeb : e, REGULAREDGE, &hend);
	makeregularend (hend.boxes[hend.boxn - 1], TOP,
		ND_coord_i(hn).y + GD_rank(hn->graph)[ND_rank(hn)].ht2, &b);
	if (b.LL.x < b.UR.x && b.LL.y < b.UR.y)
		hend.boxes[hend.boxn++] = b;
	completeregularpath (segfirst, e, &tend, &hend, boxes, boxn, longedge);
	ps = routesplines (P, &pn);
if (pn == 0) return;
	for (i = 0; i < pn; i++)
		points[pointn++] = ps[i];
	recover_slack (segfirst, P);

	/* make copies of the spline points, one per multi-edge */

	if (cnt == 1) {
		clip_and_install (fe, hackflag ? &fwdedgeb : e, points, pointn, &sinfo);
		return;
	}
	dx = Multisep * (cnt - 1) / 2;
	for (i = 1; i < pointn - 1; i++)
		points[i].x -= dx;
	for (i = 0; i < pointn; i++)
		points2[i] = points[i];
	clip_and_install (fe, hackflag ? &fwdedgeb : e, points2, pointn, &sinfo);
	for (j = 1; j < cnt; j++) {
		e = edges[ind + j];
		if (ED_tree_index(e) & BWDEDGE) {
			MAKEFWDEDGE (&fwdedge, e);
			e = &fwdedge;
		}
		for (i = 1; i < pointn - 1; i++)
			points[i].x += Multisep;
		for (i = 0; i < pointn; i++)
			points2[i] = points[i];
		clip_and_install (e, e, points2, pointn, &sinfo);
	}
}

/* self edges */

static void chooseselfsides (tendp, hendp, tsidep, hsidep, dirp)
	pathend_t *tendp, *hendp;
	int *tsidep, *hsidep, *dirp;
{
	int i;

	for (i = 0; i < 16; i++)
		if ((selfsidemap[i][0] & tendp->sidemask) &&
				(selfsidemap[i][1] & hendp->sidemask))
			break;
	if (i == 16)
		abort ();
	*tsidep = selfsidemap[i][0], *hsidep = selfsidemap[i][1];
	*dirp = selfsidemap[i][2];
	if (*dirp == ANYW) { /* ANYW can appear when tside == hside */
		switch (*tsidep) {
		case BOTTOM:
			*dirp = (tendp->np.x < hendp->np.x) ? CCW : CW;
			break;
		case RIGHT:
			*dirp = (tendp->np.y < hendp->np.y) ? CCW : CW;
			break;
		case TOP:
			*dirp = (tendp->np.x > hendp->np.x) ? CCW : CW;
			break;
		case LEFT:
			*dirp = (tendp->np.y > hendp->np.y) ? CCW : CW;
			break;
		}
	}
}

static void completeselfpath (tendp, hendp, tside, hside, dir, dx, dy, w, h)
	pathend_t *tendp, *hendp;
	int tside, hside, dir, dx, dy, w, h;
{
	int i, side;
	box boxes[4]; /* can't have more than 6 boxes */
	box tb, hb;
	int boxn;

	tb = makeselfend (tendp->boxes[tendp->boxn - 1],
			tside, dir, dx, dy);
	hb = makeselfend (hendp->boxes[hendp->boxn - 1],
			hside, OTHERDIR (dir), dx, dy);

	if (tside == hside && tendp->np.x == hendp->np.x &&
			tendp->np.y == hendp->np.y)
		adjustselfends (&tb, &hb, tendp->np, tside, dir);

	boxn = 0;
	for (side = tside; ; side = NEXTSIDE (side, dir)) {
		boxes[boxn++] = makeselfcomponent (tendp->nb, side, dx, dy, w, h);
		if (side == hside)
			break;
	}
	for (i = 0; i < tendp->boxn; i++)
		add_box (tendp->boxes[i]);
	add_box (tb);
	for (i = 0; i < boxn; i++)
		add_box (boxes[i]);
	add_box (hb);
	for (i = hendp->boxn - 1; i >= 0 ; i--)
		add_box (hendp->boxes[i]);
}

static box makeselfend (b, side, dir, dx, dy)
	box b;
	int side, dir, dx, dy;
{
	box eb;

	switch (side) {
	case BOTTOM:
		eb = boxof (b.LL.x, b.LL.y - dy, b.UR.x, b.LL.y);
		(dir == CCW) ? (eb.UR.x += dx / 2) : (eb.LL.x -= dx / 2);
		break;
	case RIGHT:
		eb = boxof (b.UR.x, b.LL.y, b.UR.x + dx, b.UR.y);
		(dir == CCW) ? (eb.UR.y += dy / 2) : (eb.LL.y -= dy / 2);
		break;
	case TOP:
		eb = boxof (b.LL.x, b.UR.y, b.UR.x, b.UR.y + dy);
		(dir == CCW) ? (eb.LL.x -= dx / 2) : (eb.UR.x += dx / 2);
		break;
	case LEFT:
		eb = boxof (b.LL.x - dx, b.LL.y, b.LL.x, b.UR.y);
		(dir == CCW) ? (eb.LL.y -= dy / 2) : (eb.UR.y += dy / 2);
		break;
	}
	return eb;
}

static box makeselfcomponent (nb, side, dx, dy, w, h)
	box nb;
	int side, dx, dy, w, h;
{
	box b;

	switch (side) {
	case BOTTOM:
		b.LL.x = nb.LL.x - dx - w, b.LL.y = nb.LL.y - dy - h;
		b.UR.x = nb.UR.x + dx + w, b.UR.y = b.LL.y + h;
		break;
	case RIGHT:
		b.LL.x = nb.UR.x + dx,     b.LL.y = nb.LL.y - dy;
		b.UR.x = b.LL.x + w,       b.UR.y = nb.UR.y + dy;
		break;
	case TOP:
		b.LL.x = nb.LL.x - dx - w, b.LL.y = nb.UR.y + dy;
		b.UR.x = nb.UR.x + dx + w, b.UR.y = b.LL.y + h;
		break;
	case LEFT:
		b.LL.x = nb.LL.x - dx - w, b.LL.y = nb.LL.y - dy;
		b.UR.x = b.LL.x + w,       b.UR.y = nb.UR.y + dy;
		break;
	}
	return b;
}

static void adjustselfends (tbp, hbp, p, side, dir)
	box *tbp, *hbp;
	point p;
	int side, dir;
{
	switch (side) {
	case BOTTOM:
		if (dir == CCW) {
			tbp->LL.x -= (tbp->UR.x - p.x), tbp->UR.x = p.x;
			hbp->UR.x += (p.x - hbp->LL.x), hbp->LL.x = p.x;
		} else {
			tbp->UR.x -= (tbp->LL.x - p.x), tbp->LL.x = p.x;
			hbp->LL.x += (p.x - hbp->UR.x), hbp->UR.x = p.x;
		}
		break;
	case RIGHT:
		if (dir == CCW) {
			tbp->LL.y -= (tbp->UR.y - p.y), tbp->UR.y = p.y;
			hbp->UR.y += (p.y - hbp->LL.y), hbp->LL.y = p.y;
		} else {
			tbp->UR.y -= (tbp->LL.y - p.y), tbp->LL.y = p.y;
			hbp->LL.y += (p.y - hbp->UR.y), hbp->UR.y = p.y;
		}
		break;
	case TOP:
		if (dir == CW) {
			tbp->LL.x -= (tbp->UR.x - p.x), tbp->UR.x = p.x;
			hbp->UR.x += (p.x - hbp->LL.x), hbp->LL.x = p.x;
		} else {
			tbp->UR.x -= (tbp->LL.x - p.x), tbp->LL.x = p.x;
			hbp->LL.x += (p.x - hbp->UR.x), hbp->UR.x = p.x;
		}
		break;
	case LEFT:
		if (dir == CW) {
			tbp->LL.y -= (tbp->UR.y - p.y), tbp->UR.y = p.y;
			hbp->UR.y += (p.y - hbp->LL.y), hbp->LL.y = p.y;
		} else {
			tbp->UR.y -= (tbp->LL.y - p.y), tbp->LL.y = p.y;
			hbp->LL.y += (p.y - hbp->UR.y), hbp->UR.y = p.y;
		}
		break;
	}
}

/* flat edges */

static void chooseflatsides (tendp, hendp,
		tsidep, hsidep, msidep, tdirp, hdirp, crossp)
	pathend_t *tendp, *hendp;
	int *tsidep, *hsidep, *tdirp, *hdirp, *msidep, *crossp;
{
	int i;

	for (i = 0; i < 16; i++)
		if ((flatsidemap[i][0] & tendp->sidemask) &&
				(flatsidemap[i][1] & hendp->sidemask))
			break;
	if (i == 16)
		abort ();
	*tsidep = flatsidemap[i][0], *hsidep = flatsidemap[i][1];
	*msidep = flatsidemap[i][2];
	*tdirp = flatsidemap[i][3], *hdirp = flatsidemap[i][4];
	*crossp = flatsidemap[i][5];
}

static void completeflatpath (tendp, hendp, tside, hside, mside,
		tdir, hdir, arg_lb, arg_rb, w, h)
	pathend_t *tendp, *hendp;
	int tside, hside, mside, tdir, hdir;
	box *arg_lb, *arg_rb;
	int w, h;
{
	int i, side, boxn;
	box boxes[8];
	box tb, hb;
	box	lb, rb;
	lb = *arg_lb; rb = *arg_rb;

	tb = makeflatend (tendp->boxes[tendp->boxn - 1], tside, tdir, lb);
	hb = makeflatend (hendp->boxes[hendp->boxn - 1], hside, OTHERDIR (hdir), rb);

	boxn = 0;
	for (side = tside; ; side = NEXTSIDE (side, tdir)) {
		boxes[boxn++] = makeflatcomponent (lb, rb, side,
				(side == mside) ? 0 : -1, tdir, w, h);
		if (side == mside)
			break;
	}
	if (mside == RIGHT)
		mside = LEFT;
	if (mside != hside) {
		for (side = NEXTSIDE (mside, hdir); ; side = NEXTSIDE (side, hdir)) {
			boxes[boxn++] = makeflatcomponent (lb, rb,
					side, 1, hdir, w, h);
			if (side == hside)
				break;
		}
	}

	for (i = 0; i < tendp->boxn; i++)
		add_box (tendp->boxes[i]);
	if (tb.LL.x != tb.UR.x && tb.LL.y != tb.UR.y)
		add_box (tb);
	for (i = 0; i < boxn; i++)
		add_box (boxes[i]);
	if (hb.LL.x != hb.UR.x && hb.LL.y != hb.UR.y)
		add_box (hb);
	for (i = hendp->boxn - 1; i >= 0 ; i--)
		add_box (hendp->boxes[i]);
}

static box makeflatend (b, side, dir, bb)
	box b;
	int side, dir;
	box bb;
{
	box eb;

	switch (side) {
	case BOTTOM:
		eb = boxof (b.LL.x, bb.LL.y, b.UR.x, b.LL.y);
		if (dir == CCW)
			eb.UR.x += (bb.UR.x - b.UR.x) / 2;
		else
			eb.LL.x -= (b.LL.x - bb.LL.x) / 2;
		break;
	case RIGHT:
		eb = boxof (b.UR.x, b.LL.y, bb.UR.x, b.UR.y);
		if (dir == CCW)
			eb.UR.y += (bb.UR.y - b.UR.y) / 2;
		else
			eb.LL.y -= (b.LL.y - bb.LL.y) / 2;
		break;
	case TOP:
		eb = boxof (b.LL.x, b.UR.y, b.UR.x, bb.UR.y);
		if (dir == CCW)
			eb.LL.x -= (b.LL.x - bb.LL.x) / 2;
		else
			eb.UR.x += (bb.UR.x - b.UR.x) / 2;
		break;
	case LEFT:
		eb = boxof (bb.LL.x, b.LL.y, b.LL.x, b.UR.y);
		if (dir == CCW)
			eb.LL.y -= (bb.UR.y - b.UR.y) / 2;
		else
			eb.UR.y += (b.LL.y - bb.LL.y) / 2;
		break;
	}
	return eb;
}

static box makeflatcomponent (lb, rb, side, mode, dir, w, h)
	box lb, rb;
	int side, mode, dir, w, h;
{
	box b;

	/* mode == -1 means use left box, 1 means use right box
	   and 0 means use mostly the left box */

	switch (side) {
	case BOTTOM:
		b.LL.x = lb.LL.x - w, b.UR.x = rb.UR.x + w;
		if (mode <= 0)
			b.LL.y = lb.LL.y - h, b.UR.y = lb.LL.y;
		else
			b.LL.y = rb.LL.y - h, b.UR.y = rb.LL.y;
		break;
	case RIGHT:
		if (mode == -1) {
			b.LL.x = lb.UR.x, b.UR.x = lb.UR.x + w;
			b.LL.y = lb.LL.y, b.UR.y = lb.UR.y;
		} else if (mode == 0) {
			b.LL.x = lb.UR.x, b.UR.x = lb.UR.x + w;
			if (dir == CCW)
				b.LL.y = lb.LL.y, b.UR.y = rb.UR.y;
			else
				b.LL.y = rb.LL.y, b.UR.y = lb.UR.y;
		} else {
			b.LL.x = rb.UR.x, b.UR.x = rb.UR.x + w;
			b.LL.y = rb.LL.y, b.UR.y = rb.UR.y;
		}
		break;
	case TOP:
		b.LL.x = lb.LL.x - w, b.UR.x = rb.UR.x + w;
		if (mode <= 0)
			b.LL.y = lb.UR.y, b.UR.y = lb.UR.y + h;
		else
			b.LL.y = rb.UR.y, b.UR.y = rb.UR.y + h;
		break;
	case LEFT:
		if (mode == -1) {
			b.LL.x = lb.LL.x - w, b.UR.x = lb.LL.x;
			b.LL.y = lb.LL.y, b.UR.y = lb.UR.y;
		} else if (mode == 0) {
			b.LL.x = lb.LL.x - w, b.UR.x = lb.LL.x;
			if (dir == CCW)
				b.LL.y = lb.LL.y, b.UR.y = rb.UR.y;
			else
				b.LL.y = rb.LL.y, b.UR.y = lb.UR.y;
		} else {
			b.LL.x = rb.LL.x - w, b.UR.x = rb.LL.x;
			b.LL.y = rb.LL.y, b.UR.y = rb.UR.y;
		}
		break;
	}
	return b;
}

/* regular edges */

#ifdef DONT_WANT_ANY_ENDPOINT_PATH_REFINEMENT
static void completeregularpath (first, last, tendp, hendp, boxes, boxn, flag)
	edge_t *first, *last;
	pathend_t *tendp, *hendp;
	box *boxes;
	int boxn, flag;
{
	edge_t *uleft, *uright, *lleft, *lright;
	int i, fb, lb;
	splines *spl;
	point *pp;
	int pn;

	fb = lb = -1;
	uleft = uright = NULL;
	uleft = top_bound (first, -1), uright = top_bound (first, 1);
	if (uleft) {
		spl = getsplinepoints (uleft);
		pp = spl->list[0].list, pn = spl->list[0].size;
		P->ulpp = &pp[0];
	}
	if (uright) {
		spl = getsplinepoints (uright);
		pp = spl->list[0].list, pn = spl->list[0].size;
		P->urpp = &pp[0];
	}
	lleft = lright = NULL;
	lleft = bot_bound (last, -1), lright = bot_bound (last, 1);
	if (lleft) {
		spl = getsplinepoints (lleft);
		pp = spl->list[spl->size - 1].list, pn = spl->list[spl->size - 1].size;
		P->llpp = &pp[pn - 1];
	}
	if (lright) {
		spl = getsplinepoints (lright);
		pp = spl->list[spl->size - 1].list, pn = spl->list[spl->size - 1].size;
		P->lrpp = &pp[pn - 1];
	}
	for (i = 0; i < tendp->boxn; i++)
		add_box (tendp->boxes[i]);
	fb = P->nbox + 1; lb = fb + boxn - 3;
	for (i = 0; i < boxn; i++)
		add_box (boxes[i]);
	for (i = hendp->boxn - 1; i >= 0 ; i--)
		add_box (hendp->boxes[i]);
	adjustregularpath (fb, lb);
}
#else
void refineregularends (edge_t *left, edge_t *right, pathend_t *endp, int dir, box b, box *boxes, int *boxnp);

/* box subdivision is obsolete, I think... ek */
static void completeregularpath (first, last, tendp, hendp, boxes, boxn, flag)
	edge_t *first, *last;
	pathend_t *tendp, *hendp;
	box *boxes;
	int boxn, flag;
{
	edge_t *uleft, *uright, *lleft, *lright;
	box uboxes[NSUB], lboxes[NSUB];
	box b;
	int uboxn, lboxn, i, y, fb, lb;

	fb = lb = -1;
	uleft = uright = NULL;
	if (flag || ND_rank(first->tail) + 1 != ND_rank(last->head))
		uleft = top_bound (first, -1), uright = top_bound (first, 1);
	refineregularends (uleft, uright, tendp, 1, boxes[0], uboxes, &uboxn);
	lleft = lright = NULL;
	if (flag || ND_rank(first->tail) + 1 != ND_rank(last->head))
		lleft = bot_bound (last, -1), lright = bot_bound (last, 1);
	refineregularends (lleft, lright, hendp, -1, boxes[boxn - 1], lboxes, &lboxn);
	for (i = 0; i < tendp->boxn; i++)
		add_box (tendp->boxes[i]);
	if (ND_rank(first->tail) + 1 == ND_rank(last->head)) {
		if ((!uleft && !uright) && (lleft || lright)) {
			b = boxes[0];
			y = b.UR.y - b.LL.y;
			for (i = 0; i < NSUB; i++) {
				uboxes[i] = b;
				uboxes[i].UR.y = b.UR.y - y * i / NSUB;
				uboxes[i].LL.y = b.UR.y - y * (i + 1) / NSUB;
			}
			uboxn = NSUB;
		} else if ((uleft || uright) && (!lleft && !lright)) {
			b = boxes[boxn - 1];
			y = b.UR.y - b.LL.y;
			for (i = 0; i < NSUB; i++) {
				lboxes[i] = b;
				lboxes[i].UR.y = b.UR.y - y * i / NSUB;
				lboxes[i].LL.y = b.UR.y - y * (i + 1) / NSUB;
			}
			lboxn = NSUB;
		}
		for (i = 0; i < uboxn; i++) {
			uboxes[i].LL.x = MAX (uboxes[i].LL.x, lboxes[i].LL.x);
			uboxes[i].UR.x = MIN (uboxes[i].UR.x, lboxes[i].UR.x);
		}
		for (i = 0; i < uboxn; i++)
			add_box (uboxes[i]);
	} else {
		for (i = 0; i < uboxn; i++)
			add_box (uboxes[i]);
		fb = P->nbox; lb = fb + boxn - 3;
		for (i = 1; i < boxn - 1; i++)
			add_box (boxes[i]);
		for (i = 0; i < lboxn; i++)
			add_box (lboxes[i]);
	}
	for (i = hendp->boxn - 1; i >= 0 ; i--)
		add_box (hendp->boxes[i]);
	adjustregularpath (fb, lb);
}
#endif

/* for now, regular edges always go from top to bottom */
static void makeregularend (box b, int side, int y, box* bp)
{
	switch (side) {
	case BOTTOM:
		*bp = boxof (b.LL.x, y, b.UR.x, b.LL.y);
		break;
	case TOP:
		*bp = boxof (b.LL.x, b.UR.y, b.UR.x, y);
		break;
	}
}

#ifndef DONT_WANT_ANY_ENDPOINT_PATH_REFINEMENT
void refineregularends (left, right, endp, dir, b, boxes, boxnp)
	edge_t *left, *right;
	pathend_t *endp;
	int dir;
	box b;
	box *boxes;
	int *boxnp;
{
	splines *lspls, *rspls;
	point pp, cp;
	box eb;
	box *bp;
	int y, i, j, k;
	int nsub;

	y = b.UR.y - b.LL.y;
	if ((y == 1) || (!left && !right)) {
		boxes[0] = b;
		*boxnp = 1;
		return;
	}
	nsub = MIN(NSUB,y);
	for (i = 0; i < nsub; i++) {
		boxes[i] = b;
		boxes[i].UR.y = b.UR.y - y * i / nsub;
		boxes[i].LL.y = b.UR.y - y * (i + 1) / nsub;
		if (boxes[i].UR.y == boxes[i].LL.y) abort();
	}
	*boxnp = nsub;
	/* only break big boxes */
	for (j = 0; j < endp->boxn; j++) {
		eb = endp->boxes[j];
		y = eb.UR.y - eb.LL.y;
#ifdef STEVE_AND_LEFTY_GRASPING_AT_STRAWS
		if (y < 15) continue;
#else
		if (y < nsub) continue;
#endif
		for (k = endp->boxn - 1; k > j; k--)
			endp->boxes[k + (nsub - 1)] = endp->boxes[k];
		for (i = 0; i < nsub; i++) {
			bp = &endp->boxes[j + ((dir == 1) ? i : (nsub - i - 1))];
			*bp = eb;
			bp->UR.y = eb.UR.y - y * i / nsub;
			bp->LL.y = eb.UR.y - y * (i + 1) / nsub;
			if (bp->UR.y == bp->LL.y) abort();
		}
		endp->boxn += (nsub - 1);
		j += nsub - 1;
	}
	if (left) {
		lspls = getsplinepoints (left);
		pp = spline_at_y (lspls, boxes[0].UR.y);
		for (i = 0; i < nsub; i++) {
			cp = spline_at_y (lspls, boxes[i].LL.y);
			/*boxes[i].LL.x = AVG (pp.x, cp.x);*/
			boxes[i].LL.x = MAX (pp.x, cp.x);
			pp = cp;
		}
		pp = spline_at_y (lspls, (dir == 1) ?
				endp->boxes[1].UR.y : endp->boxes[1].LL.y);
		for (i = 1; i < endp->boxn; i++) {
			cp = spline_at_y (lspls, (dir == 1) ?
					endp->boxes[i].LL.y : endp->boxes[i].UR.y);
			endp->boxes[i].LL.x = MIN (endp->nb.UR.x, MAX (pp.x, cp.x));
			pp = cp;
		}
		i = (dir == 1) ? 0 : *boxnp - 1;
		if (boxes[i].LL.x > endp->boxes[endp->boxn - 1].UR.x - MINW)
			boxes[i].LL.x = endp->boxes[endp->boxn - 1].UR.x - MINW;
	}
	if (right) {
		rspls = getsplinepoints (right);
		pp = spline_at_y (rspls, boxes[0].UR.y);
		for (i = 0; i < nsub; i++) {
			cp = spline_at_y (rspls, boxes[i].LL.y);
			/*boxes[i].UR.x = AVG (pp.x, cp.x);*/
			boxes[i].UR.x = AVG (pp.x, cp.x);
			pp = cp;
		}
		pp = spline_at_y (rspls, (dir == 1) ?
				endp->boxes[1].UR.y : endp->boxes[1].LL.y);
		for (i = 1; i < endp->boxn; i++) {
			cp = spline_at_y (rspls, (dir == 1) ?
					endp->boxes[i].LL.y : endp->boxes[i].UR.y);
			endp->boxes[i].UR.x = MAX (endp->nb.LL.x, AVG (pp.x, cp.x));
			pp = cp;
		}
		i = (dir == 1) ? 0 : *boxnp - 1;
		if (boxes[i].UR.x < endp->boxes[endp->boxn - 1].LL.x + MINW)
			boxes[i].UR.x = endp->boxes[endp->boxn - 1].LL.x + MINW;
	}
}
#endif

static void adjustregularpath (int fb, int lb)
{
	box *bp1, *bp2;
	int i, x;

	for (i = 0; i < P->nbox; i++) {
		bp1 = &P->boxes[i];
		if ((i - fb) % 2 == 0) {
			if (bp1->LL.x >= bp1->UR.x) {
				x = (bp1->LL.x + bp1->UR.x) / 2;
				bp1->LL.x = x - HALFMINW, bp1->UR.x = x + HALFMINW;
			}
		} else {
			if (bp1->LL.x + MINW > bp1->UR.x) {
				x = (bp1->LL.x + bp1->UR.x) / 2;
				bp1->LL.x = x - HALFMINW, bp1->UR.x = x + HALFMINW;
			}
		}
	}
	for (i = 0; i < P->nbox - 1; i++) {
		bp1 = &P->boxes[i], bp2 = &P->boxes[i + 1];
		if (i >= fb && i <= lb && (i - fb) % 2 == 0) {
			if (bp1->LL.x + MINW > bp2->UR.x)
				bp2->UR.x = bp1->LL.x + MINW;
			if (bp1->UR.x - MINW < bp2->LL.x)
				bp2->LL.x = bp1->UR.x - MINW;
		} else if (i + 1 >= fb && i < lb && (i + 1 - fb) % 2 == 0) {
			if (bp1->LL.x + MINW > bp2->UR.x)
				bp1->LL.x = bp2->UR.x - MINW;
			if (bp1->UR.x - MINW < bp2->LL.x)
				bp1->UR.x = bp2->LL.x + MINW;
		} else {
			if (bp1->LL.x + MINW > bp2->UR.x) {
				x = (bp1->LL.x + bp2->UR.x) / 2;
				bp1->LL.x = x - HALFMINW;
				bp2->UR.x = x + HALFMINW;
			}
			if (bp1->UR.x - MINW < bp2->LL.x) {
				x = (bp1->UR.x + bp2->LL.x) / 2;
				bp1->UR.x = x + HALFMINW;
				bp2->LL.x = x - HALFMINW;
			}
		}
	}
}

static box rank_box (graph_t* g, int r)
{
	box b;
	node_t /* *right0, *right1, */ *left0, *left1;

	b = Rank_box[r];
	if (b.LL.x == b.UR.x) {
		left0  = GD_rank(g)[r].v[0];
		/* right0 = GD_rank(g)[r].v[GD_rank(g)[r].n - 1]; */
		left1  = GD_rank(g)[r + 1].v[0];
		/* right1 = GD_rank(g)[r + 1].v[GD_rank(g)[r + 1].n - 1]; */
		b.LL.x = LeftBound;
		b.LL.y = ND_coord_i(left1).y + GD_rank(g)[r + 1].ht2;
		b.UR.x = RightBound;
		b.UR.y = ND_coord_i(left0).y - GD_rank(g)[r].ht1;
		Rank_box[r] = b;
	}
	return b;
}

/* returns count of vertically aligned edges starting at n */
static int straight_len (node_t* n)
{
	int cnt = 0;
	node_t *v;

	v = n;
	while (1) {
		v = ND_out(v).list[0]->head;
		if (ND_node_type(v) != VIRTUAL)
			break;
		if ((ND_out(v).size != 1) || (ND_in(v).size != 1))
			break;
		if (ND_coord_i(v).x != ND_coord_i(n).x)
			break;
		cnt++;
	}
	return cnt;
}

static edge_t *straight_path (edge_t* e, int cnt, point* plist, int* np)
{
	int n = *np;
	edge_t *f = e;

	while (cnt--)
		f = ND_out(f->head).list[0];
	plist[(*np)++] = plist[n - 1];
	plist[(*np)++] = plist[n - 1];
	plist[(*np)] = ND_coord_i(f->tail);	/* will be overwritten by next spline */
	return f;
}

static void recover_slack (edge_t* e, path* p)
{
	int		b;
	node_t	*vn;

	b = 0;		/* skip first rank box */
	for (vn = e->head; ND_node_type(vn) == VIRTUAL && !spline_merge (vn);
			vn = ND_out(vn).list[0]->head) {
		while ((b < p->nbox) && (p->boxes[b].LL.y > ND_coord_i(vn).y))
			b++;
		if (b >= p->nbox)
			break;
		if (p->boxes[b].UR.y < ND_coord_i(vn).y)
			continue;
		if (ND_label(vn))
			resize_vn (vn, p->boxes[b].LL.x, p->boxes[b].UR.x,
					p->boxes[b].UR.x + ND_rw_i(vn));
		else
			resize_vn (vn, p->boxes[b].LL.x, (p->boxes[b].LL.x +
					p->boxes[b].UR.x) / 2, p->boxes[b].UR.x);
	}
}

static void resize_vn (vn, lx, cx, rx)
	node_t *vn;
	int lx, cx, rx;
{
	ND_coord_i(vn).x = cx;
	ND_lw_i(vn) = cx - lx, ND_rw_i(vn) = rx - cx;
}

/* side > 0 means right. side < 0 means left */
static edge_t *top_bound (edge_t* e, int side)
{
	edge_t *f, *ans = NULL;
	int i;

	for (i = 0; (f = ND_out(e->tail).list[i]); i++) {
#if 0 /* were we out of our minds? */
		if (ED_tail_port(e).p.x != ED_tail_port(f).p.x)
			continue;
#endif
		if (side * (ND_order(f->head) - ND_order(e->head)) <= 0)
			continue;
		if ((ED_spl(f) == NULL) && ((ED_to_orig(f) == NULL) || (ED_to_orig(f)->u.spl == NULL)))
			continue;
		if ((ans == NULL) || (side * (ND_order(ans->head) - ND_order(f->head)) > 0))
			ans = f;
	}
	return ans;
}

static edge_t *bot_bound (edge_t* e, int side)
{
	edge_t *f, *ans = NULL;
	int i;

	for (i = 0; (f = ND_in(e->head).list[i]); i++) {
#if 0 /* same here */
		if (ED_head_port(e).p.x != ED_head_port(f).p.x)
			continue;
#endif
		if (side * (ND_order(f->tail) - ND_order(e->tail)) <= 0)
			continue;
		if ((ED_spl(f) == NULL) && ((ED_to_orig(f) == NULL) || (ED_to_orig(f)->u.spl == NULL)))
			continue;
		if ((ans == NULL) || (side * (ND_order(ans->tail) - ND_order(f->tail)) > 0))
			ans = f;
	}
	return ans;
}

static double dist(p,q)
pointf	p,q;
{
	double	d0,d1;
	d0 = p.x - q.x;
	d1 = p.y - q.y;
	return sqrt(d0*d0 + d1*d1);
}

point closest(splines* spl, point p)
{
	int		i, j, k, besti, bestj;
	double	bestdist, d, dlow, dhigh;
	double low, high, t;
	pointf c[4], pt2, pt;
	point rv;
	bezier bz;

	besti = bestj = -1;
	bestdist = 1e+38;
	pt.x = p.x; pt.y = p.y;
	for (i = 0; i < spl->size; i++) {
		bz = spl->list[i];
		for (j = 0; j < bz.size; j++) {
			pointf b;

			b.x = bz.list[j].x; b.y = bz.list[j].y;
			d = dist(b,pt);
			if ((bestj == -1) || (d < bestdist)) {
				besti = i;
				bestj = j;
				bestdist = d;
			}
		}
	}

	bz = spl->list[besti];
	j = bestj/3; if (j >= spl->size) j--;
	for (k = 0; k < 4; k++) {
		c[k].x = bz.list[j + k].x;
		c[k].y = bz.list[j + k].y;
	}
	low = 0.0; high = 1.0;
	dlow = dist(c[0],pt);
	dhigh = dist(c[3],pt);
	do {
		t = (low + high) / 2.0;
		pt2 = Bezier (c, 3, t, NULL, NULL);
		if (fabs(dlow - dhigh) < 1.0) break;
		if (low == high) break;
		if (dlow < dhigh) {high = t; dhigh = dist(pt2,pt);}
		else {low = t; dlow = dist(pt2,pt); }
	} while (1);
	rv.x = pt2.x;
	rv.y = pt2.y;
	return rv;
}

/* common routines */

static double conc_slope(node_t* n)
{
	double	s_in, s_out,m_in,m_out;
	int		cnt_in,cnt_out;
	pointf	p;
	edge_t	*e;

	s_in = s_out = 0.0;
	for (cnt_in = 0; (e = ND_in(n).list[cnt_in]); cnt_in++)
		s_in += ND_coord_i(e->tail).x;
	for (cnt_out = 0; (e = ND_out(n).list[cnt_out]); cnt_out++)
		s_out += ND_coord_i(e->head).x;
	p.x = ND_coord_i(n).x - (s_in / cnt_in);
	p.y = ND_coord_i(n).y - ND_coord_i(ND_in(n).list[0]->tail).y;
	m_in = atan2(p.y,p.x);
	p.x = (s_out / cnt_out) - ND_coord_i(n).x;
	p.y = ND_coord_i(ND_out(n).list[0]->head).y - ND_coord_i(n).y;
	m_out = atan2(p.y,p.x);
	return ((m_in + m_out) / 2.0);
}

static void beginpath (edge_t* e, int et, pathend_t* endp)
{
	int    mask;
	node_t *n;
	int (*pboxfn)(GVC_t *gvc, int, box *, int *);
	GVC_t *gvc;

	n = e->tail;
	gvc  = GD_gvc(n->graph->root);
	gvc->n = n; gvc->e = e;

        if (ND_shape(n))
		pboxfn = ND_shape(n)->fns->pboxfn;
	else
		pboxfn = NULL;
	P->start.p = add_points (ND_coord_i(n), ED_tail_port(e).p);
	P->ulpp = P->urpp = P->llpp = P->lrpp = NULL;
	if (spline_merge (e->tail)) {
		/*P->start.theta = - PI / 2;*/
		P->start.theta = conc_slope(e->tail);
		P->start.constrained = TRUE;
	} else {
		if (ED_tail_port(e).constrained) {
			P->start.theta = ED_tail_port(e).theta;
			P->start.constrained = TRUE;
		} else
			P->start.constrained = FALSE;
	}
	P->nbox = 0;
	P->data = (void*)e;
	endp->np = P->start.p;
	/* FIXME: check that record_path returns a good path */
	if (pboxfn && (mask = (*pboxfn) (gvc, 1, &endp->boxes[0], &endp->boxn)))
		endp->sidemask = mask;
	else {
		endp->boxes[0] = endp->nb;
		endp->boxn = 1;
	}
	switch (et) {
	case SELFEDGE:
		/* moving the box UR.y by + 1 avoids colinearity between
			port point and box that confuses Proutespline().  it's
			a bug in Proutespline() but this is the easiest fix. */
		endp->boxes[0].UR.y = P->start.p.y + 1;
		endp->sidemask = BOTTOM;
		break;
	case FLATEDGE:
		endp->boxes[0].LL.y = P->start.p.y;
		endp->sidemask = TOP;
		break;
	case REGULAREDGE:
		endp->boxes[0].UR.y = P->start.p.y;
		endp->sidemask = BOTTOM;
		break;
	}
}

static void endpath (edge_t* e, int et, pathend_t* endp)
{
	int    mask;
	node_t *n;
	int (*pboxfn) (GVC_t *gvc, int, box *, int *);
	GVC_t *gvc;

	n = e->head;
	gvc  = GD_gvc(n->graph->root);
	gvc->n = n; gvc->e = e;

        if (ND_shape(n))
		pboxfn = ND_shape(n)->fns->pboxfn;
	else
		pboxfn = NULL;
	P->end.p = add_points (ND_coord_i(n), ED_head_port(e).p);
	if (spline_merge (e->head)) {
		/*P->end.theta = PI / 2;*/
		P->end.theta = conc_slope(e->head) + PI;
		assert(P->end.theta < 2*PI);
		P->end.constrained = TRUE;
	} else {
		if (ED_head_port(e).constrained) {
			P->end.theta = ED_head_port(e).theta;
			P->end.constrained = TRUE;
		} else
			P->end.constrained = FALSE;
	}
	endp->np = P->end.p;
	if (pboxfn && (mask = (*pboxfn) (gvc, 2, &endp->boxes[0], &endp->boxn)))
		endp->sidemask = mask;
	else {
		endp->boxes[0] = endp->nb;
		endp->boxn = 1;
	}
	switch (et) {
	case SELFEDGE:
		endp->boxes[0].LL.y = P->end.p.y;
		endp->sidemask = TOP;
		break;
	case FLATEDGE:
		endp->boxes[0].LL.y = P->end.p.y;
		endp->sidemask = TOP;
		break;
	case REGULAREDGE:
		endp->boxes[0].LL.y = P->end.p.y;
		endp->sidemask = TOP;
		break;
	}
}

static edge_t *getmainedge (edge_t* e)
{
	edge_t *le = e;
	while (ED_to_virt(le))
		le = ED_to_virt(le);
	while (ED_to_orig(le))
		le = ED_to_orig(le);
	return le;
}

static splines *getsplinepoints (edge_t* e)
{
	edge_t *le;
	splines *sp;

	for (le = e; !(sp = ED_spl(le)) && ED_edge_type(le) != NORMAL; le = ED_to_orig(le)) ;
	if (sp == NULL) abort ();
	return sp;
}

static int
cl_vninside(graph_t* cl, node_t* n)
{
	return (BETWEEN(GD_bb(cl).LL.x,ND_coord_i(n).x,GD_bb(cl).UR.x) &&
		BETWEEN(GD_bb(cl).LL.y,ND_coord_i(n).y,GD_bb(cl).UR.y));
}

/* returns the cluster of (adj) that interferes with n,
 */
static graph_t * 
cl_bound(n, adj)
node_t		*n,*adj;
{
    graph_t     *rv,*cl,*tcl,*hcl;
	edge_t		*orig;

    rv = NULL;
	if (ND_node_type(n) == NORMAL) tcl = hcl = ND_clust(n);
	else {
		orig = ND_out(n).list[0]->u.to_orig;
		tcl = ND_clust(orig->tail); hcl = ND_clust(orig->head);
	}
    if (ND_node_type(adj) == NORMAL) {
        cl = ND_clust(adj);
        if (cl && (cl != tcl) && (cl != hcl)) rv = cl;
    }
    else {
        orig = ED_to_orig(ND_out(adj).list[0]);
        cl = ND_clust(orig->tail);
        if (cl && (cl != tcl) && (cl != hcl) && cl_vninside(cl,adj)) rv=cl;
        else {
            cl = ND_clust(orig->head);
            if (cl && (cl != tcl) && (cl != hcl) && cl_vninside(cl,adj)) rv=cl;
        }
    }
    return rv;
}

static box maximal_bbox (vn, ie, oe)
	node_t *vn;
	edge_t *ie, *oe;
{
    int     nb,b;
    graph_t *g = vn->graph, *left_cl,*right_cl;
    node_t  *left,*right;
    box     rv;

	left_cl = right_cl = NULL;

    /* give this node all the available space up to its neighbors */
	b = ND_coord_i(vn).x - ND_lw_i(vn);
    if ((left = neighbor(vn, ie, oe, -1))) {
		if ((left_cl = cl_bound(vn, left))) 
			nb = GD_bb(left_cl).UR.x + Splinesep;
        else {
			nb = ND_coord_i(left).x + ND_mval(left);
			if (ND_node_type(left) == NORMAL) nb += GD_nodesep(g)/2;
			else nb += Splinesep;
		}
		if (nb < b) b = nb;
		rv.LL.x = b;
    }
    else rv.LL.x = MIN(b,LeftBound);

	/* we have to leave room for our own label! */
	if (ND_label(vn)) b = ND_coord_i(vn).x + 10;
	else b = ND_coord_i(vn).x + ND_rw_i(vn);
    if ((right = neighbor(vn, ie, oe, 1))) {
		if ((right_cl = cl_bound(vn, right)))
            nb = GD_bb(right_cl).LL.x - Splinesep;
        else {
			nb = ND_coord_i(right).x - ND_lw_i(right);
			if (ND_node_type(right) == NORMAL) nb -= GD_nodesep(g)/2;
			else nb -= Splinesep;
		}
		if (nb > b) b = nb;
		rv.UR.x = b;
    }
    else rv.UR.x = MAX(b,RightBound);

    if ((ND_node_type(vn) == VIRTUAL) && (ND_label(vn)))
        rv.UR.x -= ND_rw_i(vn);

    rv.LL.y = ND_coord_i(vn).y - GD_rank(g)[ND_rank(vn)].ht1;
    rv.UR.y = ND_coord_i(vn).y + GD_rank(g)[ND_rank(vn)].ht2;
    return rv;
}

static node_t *neighbor (vn, ie, oe, dir)
	node_t *vn;
	edge_t *ie, *oe;
	int dir;
{
	int i;
	node_t *n, *rv = NULL;
	rank_t *rank = &(GD_rank(vn->graph)[ND_rank(vn)]);

	for (i = ND_order(vn) + dir; ((i >= 0) && (i < rank->n)); i += dir) {
		n = rank->v[i];
		if ((ND_node_type(n) == VIRTUAL) && (ND_label(n))) {
			rv=n;
			break;
		}
		if (ND_node_type(n) == NORMAL) {
			rv = n;
			break;
		}
		if (pathscross(n, vn, ie, oe) == FALSE) {
			rv = n;
			break;
		}
	}
	return rv;
}

static boolean pathscross (n0, n1, ie1, oe1)
	node_t *n0, *n1;
	edge_t *ie1, *oe1;
{
	edge_t *e0, *e1;
	node_t *na, *nb;
	int order, cnt;

	order = (ND_order(n0) > ND_order(n1));
	if ((ND_out(n0).size != 1) && (ND_out(n0).size != 1))
		return FALSE;
	e1 = oe1;
	if (ND_out(n0).size == 1 && e1) {
		e0 = ND_out(n0).list[0];
		for (cnt = 0; cnt < 2; cnt++) {
			if ((na = e0->head) == (nb = e1->head))
				break;
			if (order != (ND_order(na) > ND_order(nb)))
				return TRUE;
			if ((ND_out(na).size != 1) || (ND_node_type(na) == NORMAL))
				break;
			e0 = ND_out(na).list[0];
			if ((ND_out(nb).size != 1) || (ND_node_type(nb) == NORMAL))
				break;
			e1 = ND_out(nb).list[0];
		}
	}
	e1 = ie1;
	if (ND_in(n0).size == 1 && e1) {
		e0 = ND_in(n0).list[0];
		for (cnt = 0; cnt < 2; cnt++) {
			if ((na = e0->tail) == (nb = e1->tail))
				break;
			if (order != (ND_order(na) > ND_order(nb)))
				return TRUE;
			if ((ND_in(na).size != 1) || (ND_node_type(na) == NORMAL))
				break;
			e0 = ND_in(na).list[0];
			if ((ND_in(nb).size != 1) || (ND_node_type(nb) == NORMAL))
				break;
			e1 = ND_in(nb).list[0];
		}
	}
	return FALSE;
}

static void add_box (box b)
{
	P->boxes[P->nbox++] = b;
}

#ifdef DEBUG
void showpath(path *p)
{
	int	i;
	point	LL,UR;

	fprintf(stderr,"%%!PS\n");
	for (i = 0; i < p->nbox; i++) {
		LL = p->boxes[i].LL; UR = p->boxes[i].UR;
		fprintf(stderr,"newpath %d %d moveto %d %d lineto %d %d lineto %d %d lineto closepath stroke\n",LL.x,LL.y,UR.x,LL.y,UR.x,UR.y,LL.x,UR.y);
	}
	fprintf(stderr,"showpage\n");
}
#endif
