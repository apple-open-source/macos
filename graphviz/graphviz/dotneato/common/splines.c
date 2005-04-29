/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/* Functions related to creating a spline and attaching it to
 * an edge, starting from a list of control points.
 */

#include <render.h>

/* wantclip:
 * Return false if head/tail end of edge should not be clipped
 * to node.
 */
static boolean 
wantclip(edge_t *e, node_t *n)
{
	char		*str;
	attrsym_t *sym = 0;
	boolean		rv = TRUE;

	if (n == e->tail) sym = E_tailclip;
	if (n == e->head) sym = E_headclip;
	if (sym) {	/* mapbool isn't a good fit, because we want "" to mean TRUE */
		str = agxget(e,sym->index);
		if (str && str[0]) rv = mapbool(str);
		else rv = TRUE;
	}
	return rv;
}

/* arrow_clip:
 * Clip arrow to node boundary.
 * The real work is done elsewhere. Here we get the real edge,
 * check that the edge has arrowheads, and that an endpoint
 * isn't a merge point where several parts of an edge meet.
 * (e.g., with edge concentrators).
 */
static void 
arrow_clip (edge_t *fe, edge_t *le,
            point *ps, int *startp, int *endp, 
            bezier *spl, splineInfo* info)
{
	edge_t *e;
	int i, j, sflag, eflag;
	inside_t inside_context;

	for (e = fe; ED_to_orig(e); e = ED_to_orig(e))
	;
	inside_context.e = e;

	j = info->swapEnds(e);
	arrow_flags (e, &sflag, &eflag);
 	if (info->splineMerge (le->head)) eflag = ARR_NONE;
 	if (info->splineMerge (fe->tail)) sflag = ARR_NONE;
	if (j) {i=sflag; sflag=eflag; eflag=i;} /* swap the two ends */

	if (sflag) *startp = arrowStartClip (&inside_context, ps, *startp, *endp, spl, sflag);
	if (eflag) *endp = arrowEndClip (&inside_context, ps, *startp, *endp, spl, eflag);
}

/* bezier_clip
 * Clip bezier to shape using binary search.
 * The details of the shape are passed in the inside_context;
 * The function providing the inside test is passed as a parameter.
 * left_inside specifies that sp[0] is inside the node, else * sp[3] is taken as inside.
 */
void bezier_clip (inside_t *inside_context, boolean (*inside)(inside_t *inside_context, pointf p), pointf* sp, boolean left_inside)
{
        pointf seg[4], best[4], pt, opt, *left, *right;
        double low, high, t, *idir, *odir;
        boolean found;
        int i;

        if (left_inside) {
                left = NULL; right = seg;  pt = sp[0]; idir = &low;  odir = &high;
        } else {
                left = seg;  right = NULL; pt = sp[3]; idir = &high; odir = &low;
        }
        found = FALSE;
        low = 0.0; high = 1.0;
        do {
                opt = pt;
                t = (high + low) / 2.0;
                pt = Bezier (sp, 3, t, left, right);
                if (inside(inside_context, pt)) {
                        *idir = t;
                } else {
                        for (i = 0; i < 4; i++)
                                best[i] = seg[i];
                        found = TRUE;
                        *odir = t;
                }
        } while (ABS (opt.x - pt.x) > .5 || ABS (opt.y - pt.y) > .5);
        if (found)
                for (i = 0; i < 4; i++)
                        sp[i] = best[i];
        else
                for (i = 0; i < 4; i++)
                        sp[i] = seg[i];
}

/* shape_clip0:
 * Clip Bezier to node shape using binary search.
 * left_inside specifies that curve[0] is inside the node, else
 * curve[3] is taken as inside.
 * Assumes ND_shape(n) and ND_shape(n)->fns->insidefn are non-NULL.
 * See note on shape_clip.
 */
static void 
shape_clip0 (inside_t *inside_context, node_t *n, point curve[4], boolean left_inside)
{
	int i, save_real_size;
	pointf c[4];

	save_real_size = ND_rw_i(n);
	for (i = 0; i < 4; i++) {
		c[i].x = curve[i].x - ND_coord_i(n).x;
		c[i].y = curve[i].y - ND_coord_i(n).y;
	}

	bezier_clip (inside_context, ND_shape(n)->fns->insidefn, c, left_inside);

	for (i = 0; i < 4; i++) {
		curve[i].x = ROUND(c[i].x + ND_coord_i(n).x);
		curve[i].y = ROUND(c[i].y + ND_coord_i(n).y);
	}
	ND_rw_i(n) = save_real_size;
}

/* shape_clip:
 * Clip Bezier to node shape
 * Uses curve[0] to determine which which side is inside the node. 
 * NOTE: This test is bad. It is possible for previous call to
 * shape_clip to produce a Bezier with curve[0] moved to the boundary
 * for which insidefn(curve[0]) is true. Thus, if the new Bezier is
 * fed back to shape_clip, it will again assume left_inside is true.
 * To be safe, shape_clip0 should guarantee that the computed boundary
 * point fails insidefn.
 */
void 
shape_clip (node_t *n, point curve[4], edge_t *e)
{
	int save_real_size;
	boolean left_inside;
	pointf c;
	inside_t inside_context;
	
	if (ND_shape(n) == NULL
	  || ND_shape(n)->fns->insidefn == NULL) return;

	inside_context.n = n;
	inside_context.e = e;
	save_real_size = ND_rw_i(n);
	c.x = curve[0].x - ND_coord_i(n).x;
	c.y = curve[0].y - ND_coord_i(n).y;
	left_inside = ND_shape(n)->fns->insidefn (&inside_context, c);
	ND_rw_i(n) = save_real_size;
	shape_clip0 (&inside_context, n, curve, left_inside);
}

/* new_spline:
 * Create and attach a new bezier of size sz to the edge d
 */
bezier*
new_spline (edge_t* e, int sz)
{
	bezier *rv;

	while (ED_edge_type(e) != NORMAL)
		e = ED_to_orig(e);
	if (ED_spl(e) == NULL)
		ED_spl(e) = NEW (splines);
	ED_spl(e)->list = ALLOC (ED_spl(e)->size + 1, ED_spl(e)->list, bezier);
	rv = &(ED_spl(e)->list[ED_spl(e)->size++]);
	rv->list = N_NEW (sz, point);
	rv->size = sz;
	rv->sflag = rv->eflag = FALSE;
	return rv;
}

/* update_bb:
 * Update the bounding box of g based on the addition of
 * point p.
 */
static void 
update_bb(graph_t* g, point pt)
{
	if (pt.x > GD_bb(g).UR.x)  GD_bb(g).UR.x = pt.x;
	if (pt.y > GD_bb(g).UR.y)  GD_bb(g).UR.y = pt.y;
	if (pt.x < GD_bb(g).LL.x)  GD_bb(g).LL.x = pt.x;
	if (pt.y < GD_bb(g).LL.y)  GD_bb(g).LL.y = pt.y;
}

/* clip_and_install:
 * Given a raw spline (pn control points in ps), representing
 * a path from edge fe ending in edge le, clip the ends to
 * the node boundaries and attach the resulting spline to the
 * edge.
 */
void 
clip_and_install (edge_t* fe, edge_t* le, point* ps, int pn, splineInfo* info)
{
	pointf   p2;
	bezier *newspl;
	node_t *tn, *hn;
	int start, end, i;
	graph_t	*g;
	edge_t	*orig;
	inside_t inside_context;

	tn = fe->tail; hn = le->head; g = tn->graph;
	newspl = new_spline (fe, pn);

	for (orig = fe; ED_edge_type(orig) != NORMAL; orig = ED_to_orig(orig));

		/* may be a reversed flat edge */
	if ((tn->u.rank == hn->u.rank) && (tn->u.order > hn->u.order))
		{node_t *tmp; tmp = hn; hn = tn; tn = tmp;}

		/* spline may be interior to node */
	if (wantclip(orig,tn) && ND_shape(tn) && ND_shape(tn)->fns->insidefn) {
		inside_context.n = tn;
		inside_context.e = fe;
		for (start=0; start < pn - 4; start+=3) {
			p2.x = ps[start+3].x - ND_coord_i(tn).x;
			p2.y = ps[start+3].y - ND_coord_i(tn).y;
			if (ND_shape(tn)->fns->insidefn (&inside_context, p2) == FALSE)
				break;
		}
		shape_clip0 (&inside_context, tn, &ps[start], TRUE);
	} else start = 0;
	if (wantclip(orig,hn) && ND_shape(hn) && ND_shape(hn)->fns->insidefn) {
		inside_context.n = hn;
		inside_context.e = le;
		for (end = pn - 4; end > 0; end -= 3) {
			p2.x = ps[end].x - ND_coord_i(hn).x;
			p2.y = ps[end].y - ND_coord_i(hn).y;
			if (ND_shape(hn)->fns->insidefn (&inside_context, p2) == FALSE)
				break;
		}
		shape_clip0 (&inside_context, hn, &ps[end], FALSE);
	} else end = pn - 4;
	for (; start < pn - 4; start+=3)
		if (ps[start].x != ps[start + 3].x || ps[start].y != ps[start + 3].y)
			break;
	for (; end > 0; end -= 3)
		if (ps[end].x != ps[end + 3].x || ps[end].y != ps[end + 3].y)
			break;
	arrow_clip (fe, le, ps, &start, &end, newspl, info);
	for (i = start; i < end + 4; i++) {
		point		pt;
		pt = newspl->list[i - start] = ps[i];
		update_bb(g,pt);
	}
	newspl->size = end - start + 4;
}
