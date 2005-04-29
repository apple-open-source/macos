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

#include	"render.h"


static boolean	Flip;
static point	Offset;

static void place_flip_graph_label(graph_t* g);

#define M1 \
	"/pathbox { /Y exch %d sub def /X exch %d sub def /y exch %d sub def /x exch %d sub def newpath x y moveto X y lineto X Y lineto x Y lineto closepath stroke } def\n"
#define M2 \
	"/pathbox { /X exch neg %d sub def /Y exch %d sub def /x exch neg %d sub def /y exch %d sub def newpath x y moveto X y lineto X Y lineto x Y lineto closepath stroke } def\n"

point
map_point(point p)
{
	int		x = p.x;
	int 	y = p.y;

	if (Flip) { p.x = -y - Offset.x; p.y = x - Offset.y; }
	else { p.x = x - Offset.x; p.y = y - Offset.y; }
	return p;
}

void
map_edge(edge_t* e)
{
	int			j,k;
	bezier		bz;

if (ED_spl(e) == NULL) {
	if ((Concentrate == FALSE) || (ED_edge_type(e) != IGNORED))
		agerr(AGERR, "lost %s %s edge\n",e->tail->name,e->head->name);
	return;
}
	for (j = 0; j < ED_spl(e)->size; j++) {
		bz = ED_spl(e)->list[j];
		for (k = 0; k < bz.size; k++)
			bz.list[k] = map_point(bz.list[k]);
		if (bz.sflag)
			ED_spl(e)->list[j].sp = map_point (ED_spl(e)->list[j].sp);
		if (bz.eflag)
			ED_spl(e)->list[j].ep = map_point (ED_spl(e)->list[j].ep);
	}
	if (ED_label(e)) ED_label(e)->p = map_point(ED_label(e)->p);
    /* vladimir */
	if (ED_head_label(e)) ED_head_label(e)->p = map_point(ED_head_label(e)->p);
	if (ED_tail_label(e)) ED_tail_label(e)->p = map_point(ED_tail_label(e)->p);
}

void translate_bb(graph_t* g, int lr)
{
	int			c;
	box			bb,new_bb;

	bb = GD_bb(g);
	if (lr) {
		new_bb.LL = map_point(pointof(bb.LL.x,bb.UR.y));
		new_bb.UR = map_point(pointof(bb.UR.x,bb.LL.y));
	}
	else {
		new_bb.LL = map_point(pointof(bb.LL.x,bb.LL.y));
		new_bb.UR = map_point(pointof(bb.UR.x,bb.UR.y));
	}
	GD_bb(g) = new_bb;
	if (GD_label(g)) {
		GD_label(g)->p = map_point(GD_label(g)->p);
    }
	for (c = 1; c <= GD_n_cluster(g); c++) translate_bb(GD_clust(g)[c],lr);
}

static void translate_drawing(graph_t* g, nodesizefn_t ns)
{
	node_t		*v;
	edge_t		*e;
	int         shift = (Offset.x || Offset.y);

	for (v = agfstnode(g); v; v = agnxtnode(g,v)) {
		ns(v,FALSE);
		if (shift) {
			ND_coord_i(v) = map_point(ND_coord_i(v));
			for (e = agfstout(g,v); e; e = agnxtout(g,e)) map_edge(e);
		}
	}
	if (shift) translate_bb(g,GD_left_to_right(g));
}

static void
place_root_label (graph_t* g)
{
    point       p,d;
    pointf	dimen;

    dimen = GD_label(g)->dimen;  /* get label dimension in inches */
    dimen.x += 4*GAP; /* allow some minimum whitespace around label */
    dimen.y += 2*GAP;
    d = cvt2pt(dimen);

    if (GD_label_pos(g) & LABEL_AT_RIGHT) {
		p.x = GD_bb(g).UR.x - d.x/2;
	} 
	else if (GD_label_pos(g) & LABEL_AT_LEFT) {
		p.x = GD_bb(g).LL.x + d.x/2;
	}
	else {
		p.x = (GD_bb(g).LL.x + GD_bb(g).UR.x)/2;	
	}

    if (GD_label_pos(g) & LABEL_AT_TOP) {
		p.y = GD_bb(g).UR.y - d.y/2;
	}
	else {
		p.y = GD_bb(g).LL.y + d.y/2;
	}

    GD_label(g)->p = p;
    GD_label(g)->set = TRUE;
}

/* dotneato_postprocess:
 * Set graph and cluster label positions.
 * Add space for root graph label and translate graph accordingly.
 * Set final nodesize using ns.
 * Assumes the boxes of all clusters have been computed.
 * When done, the bounding box of g has LL at origin.
 */
void dotneato_postprocess(Agraph_t *g, nodesizefn_t ns)
{
	int   diff;
	pointf dimen;
	point d;

	Flip = GD_left_to_right(g);
	if (Flip) place_flip_graph_label(g);
	else place_graph_label(g);

	if (GD_label(g)) {
		dimen = GD_label(g)->dimen;
		dimen.x += 4*GAP;
		dimen.y += 2*GAP;
		d = cvt2pt(dimen);
	} 
	else {
		d.x = 0;
		d.y = 0;
	}
	if (Flip) {
		if (GD_label(g) && !GD_label(g)->set) {
			if (GD_label_pos(g) & LABEL_AT_TOP) {
				GD_bb(g).UR.x += d.y;
			} else {
				GD_bb(g).LL.x -= d.y;
			}

			if (d.x > GD_bb(g).UR.y - GD_bb(g).LL.y) {
				diff = d.x - (GD_bb(g).UR.y - GD_bb(g).LL.y);
				diff = diff/2;
				GD_bb(g).LL.y -= diff; GD_bb(g).UR.y += diff;
			}			
		}
		Offset.x = -GD_bb(g).UR.y;
		Offset.y = GD_bb(g).LL.x;
	}
	else {
		if (GD_label(g) && !GD_label(g)->set) {
			if (GD_label_pos(g) & LABEL_AT_TOP) {
				GD_bb(g).UR.y += d.y;
			} else {
				GD_bb(g).LL.y -= d.y;
			}

			if (d.x > GD_bb(g).UR.x - GD_bb(g).LL.x) {
				diff = d.x - (GD_bb(g).UR.x - GD_bb(g).LL.x);
				diff = diff/2;
				GD_bb(g).LL.x -= diff; GD_bb(g).UR.x += diff;
			}
		}
		Offset = GD_bb(g).LL;
	}
	translate_drawing(g, ns);
	if (GD_label(g) && !GD_label(g)->set) place_root_label (g);

	if (Show_boxes) {
		if (Flip)
			fprintf (stderr, M2, Offset.x, Offset.y, Offset.x, Offset.y);
		else
			fprintf (stderr, M1, Offset.y, Offset.x, Offset.y, Offset.x);
	}
}


void osize_label(textlabel_t *label, int *b, int* t ,int *l, int *r)
{
	point	pt,sz2;
	pointf  dimen;

	dimen = label->dimen;
	dimen.x += 4*GAP;
	dimen.y += 2*GAP;
	sz2 = cvt2pt(label->dimen);
	sz2.x /= 2;
	sz2.y /= 2;
	pt = add_points(label->p,sz2);
	if (*r < pt.x) *r = pt.x;
	if (*t < pt.y) *t = pt.y;
	pt = sub_points(label->p,sz2);
	if (*l > pt.x) *l = pt.x;
	if (*b > pt.y) *b = pt.y;
}

/* place_flip_graph_label:
 * Put cluster labels recursively in the flip case.
 */
static void place_flip_graph_label(graph_t* g)
{
	int		c,maxx,minx;
	int		maxy, miny;
	point		p,d;
	pointf		dimen;
		
    if ((g != g->root) && (GD_label(g)) && !GD_label(g)->set) {
	dimen = GD_label(g)->dimen;
	dimen.x += 4*GAP;
	dimen.y += 2*GAP;
        d = cvt2pt(dimen);
		
    	if (GD_label_pos(g) & LABEL_AT_RIGHT) {
          p.y = GD_bb(g).LL.y + d.x/2;
          maxy = p.y + d.x/2;
          if (GD_bb(g->root).UR.y < maxy) GD_bb(g->root).UR.y = maxy;
        }
    	else if (GD_label_pos(g) & LABEL_AT_LEFT) {
          p.y = GD_bb(g).UR.y - d.x/2;
          miny = p.y - d.x/2;
          if (GD_bb(g->root).LL.y > miny) GD_bb(g->root).LL.y = miny;
		}
        else {
          p.y = (GD_bb(g).LL.y + GD_bb(g).UR.y)/2;
          maxy = p.y + d.x/2;
          miny = p.y - d.x/2;
        }

		if (GD_label_pos(g) & LABEL_AT_TOP) {
		  p.x = GD_bb(g).UR.x + d.y/2;
          maxx = GD_bb(g).UR.x + d.y;
		  GD_bb(g).UR.x = maxx;
          if (GD_bb(g->root).UR.x < maxx) GD_bb(g->root).UR.x = maxx;		  
		} else {
		  p.x = GD_bb(g).LL.x - d.y/2;
          minx = GD_bb(g).LL.x - d.y;
		  GD_bb(g).LL.x = minx;
          if (GD_bb(g->root).LL.x > minx) GD_bb(g->root).LL.x = minx;

		}
   	    GD_label(g)->p = p;
   	    GD_label(g)->set = TRUE;
    }

	for (c = 1; c <= GD_n_cluster(g); c++)
		place_flip_graph_label(GD_clust(g)[c]);
}

/* place_graph_label:
 * Put cluster labels recursively in the non-flip case.
 */
void place_graph_label(graph_t* g)
{
	int			c;
	/* int			maxy,miny; */
	int         minx, maxx;
	point		p,d;
	pointf		dimen;

    if ((g != g->root) && (GD_label(g)) && !GD_label(g)->set) {

	dimen = GD_label(g)->dimen;
	dimen.x += 4*GAP;
	dimen.y += 2*GAP;
        d = cvt2pt(dimen);

    	if (GD_label_pos(g) & LABEL_AT_RIGHT) {
          p.x = GD_bb(g).UR.x - d.x/2;
          minx = p.x - d.x/2;
		  if (GD_bb(g).LL.x > minx) GD_bb(g).LL.x = minx;
          if (GD_bb(g->root).LL.x > minx) GD_bb(g->root).LL.x = minx;
        }
    	else if (GD_label_pos(g) & LABEL_AT_LEFT) {
          p.x = GD_bb(g).LL.x + d.x/2;
          maxx = p.x + d.x/2;
		  if (GD_bb(g).UR.x < maxx) GD_bb(g).UR.x = maxx;
          if (GD_bb(g->root).UR.x < maxx) GD_bb(g->root).UR.x = maxx;
		}
        else {
          p.x = (GD_bb(g).LL.x + GD_bb(g).UR.x)/2;
          maxx = p.x + d.x/2;
          minx = p.x - d.x/2;
		  if (GD_bb(g).UR.x < maxx) GD_bb(g).UR.x = maxx;
		  if (GD_bb(g).LL.x > minx) GD_bb(g).LL.x = minx;
          if (GD_bb(g->root).UR.x < maxx) GD_bb(g->root).UR.x = maxx;
          if (GD_bb(g->root).LL.x > minx) GD_bb(g->root).LL.x = minx;
        }
		if (GD_label_pos(g) & LABEL_AT_TOP) {
          p.y = GD_bb(g).UR.y - d.y/2;
        }
        else {
          p.y = GD_bb(g).LL.y + d.y/2;
        }
        GD_label(g)->p = p;
        GD_label(g)->set = TRUE;
    }

	for (c = 1; c <= GD_n_cluster(g); c++)
		place_graph_label(GD_clust(g)[c]);
}
