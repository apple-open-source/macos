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
 *  graphics code generator
 */

#include	"render.h"
#include	"agxbuf.h"
#include	"utils.h"
#include	"htmltable.h"
#include	"gvrender.h"

char*		BaseLineStyle[3] = {"solid\0","setlinewidth\0001\0",0};
int			Obj;
static int			N_pages = 1;	/* w.r.t. unrotated coords */
static int			Page;			/* w.r.t. unrotated coords */
static int			Layer,Nlayers;
static char			**LayerID;
static point		First,Major,Minor;
static point		Pages;
static box			PB;		/* drawable region in device coords */
static pointf		GP;		/* graph page size, in graph coords */
static box			CB;		/* current page box, in graph coords */
static point		PFC;	/* device page box for centering */
static double	    Deffontsize;
static char			*Deffontname;
static char*		Layerdelims;
static attrsym_t*   G_peripheries;

static char*	lang_name (int langID);

static int write_edge_test(Agraph_t *g, Agedge_t *e) {
	Agraph_t	*sg;
	int			c;

	for(c = 1; c <= GD_n_cluster(g); c++) {
		sg = GD_clust(g)[c];
		if(agcontains(sg, e)) return FALSE;
	}
	return TRUE;
}


static int write_node_test(Agraph_t *g, Agnode_t *n) {
	Agraph_t	*sg;
	int			c;

	for(c = 1; c <= GD_n_cluster(g); c++) {
		sg = GD_clust(g)[c];
		if(agcontains(sg, n)) return FALSE;
	}
	return TRUE;
}


void emit_reset(GVC_t *gvc)
{
	Agraph_t	*g = gvc->g;
	Agnode_t	*n;

	N_pages = 1;
	Page = 0;
	Layer = Nlayers = 0;
	LayerID = (char **) 0;
	First.x = First.y = 0;
	Major.x = Major.y = 0;
	Minor.x = Minor.y = 0;
	Pages.x = Pages.y = 0;
	PB.LL.x = PB.LL.y = PB.UR.x = PB.UR.y = 0;
	GP.x = GP.y = 0;
	CB.LL.x = CB.LL.y = CB.UR.x = CB.UR.y = 0;
	PFC.x = PFC.y = 0;
	Deffontsize = 0;
	Deffontname = (char *) 0;

	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		ND_state(n) = 0;
	}

	gvrender_reset(gvc);
}

static void emit_background(GVC_t *gvc, point LL, point UR)
{
	char	*str;
	point	A[4];
	graph_t *g = gvc->g;

	if (((str = agget(g,"bgcolor")) != 0) && str[0]) {
		A[0].x = A[1].x = LL.x - GD_drawing(g)->margin.x;
		A[2].x = A[3].x = UR.x + GD_drawing(g)->margin.x;
		A[1].y = A[2].y = UR.y + GD_drawing(g)->margin.y;
		A[3].y = A[0].y = LL.y - GD_drawing(g)->margin.y;
		gvrender_set_fillcolor(gvc, str);
		gvrender_set_pencolor(gvc, str);
		gvrender_polygon(gvc, A, 4, TRUE);	/* filled */
	}
}

static void emit_defaults(GVC_t *gvc)
{
	gvrender_set_pencolor(gvc, DEFAULT_COLOR);
	gvrender_set_fillcolor(gvc, DEFAULT_COLOR);
	gvrender_set_font(gvc, Deffontname, Deffontsize);
}


/* even if this makes you cringe, at least it's short */
static void setup_page(GVC_t *gvc, point page)
{
	point		offset;
	double		scale;
	int		rot;
	graph_t *g = gvc->g;

	Page++;

	/* establish current box in graph coordinates */
	CB.LL.x = page.x  * GP.x;	CB.LL.y = page.y * GP.y;
	CB.UR.x = CB.LL.x + GP.x;	CB.UR.y = CB.LL.y + GP.y;

	/* establish offset to be applied, in graph coordinates */
	if (GD_drawing(g)->landscape == FALSE) offset = pointof(-CB.LL.x,-CB.LL.y);
	else { offset.x = (page.y + 1) * GP.y; offset.y = -page.x * GP.x; }
	scale = GD_drawing(g)->scale;
	rot = GD_drawing(g)->landscape ? 90 : 0;
	gvrender_begin_page(gvc, page, scale, rot, offset);
	emit_background(gvc, CB.LL, CB.UR);
	emit_defaults(gvc);
}

/* parse_layers:
 * Split input string into tokens, with separators specified by
 * the layersep attribute. Store the values in the LayerID array,
 * starting at index 1, and return the count.
 * Free previously stored list. Note that there is no mechanism
 * to free the memory before exit.
 */
static int parse_layers(graph_t* g, char* p)
{
	int		ntok;
	char*	pcopy;
	char*	tok;
	int		sz;

	Layerdelims = agget(g,"layersep");
	if (!Layerdelims) Layerdelims = DEFAULT_LAYERSEP;

	ntok = 0;
	sz = 0;
	pcopy = strdup(p);

	if (LayerID) free(LayerID);
	LayerID = 0;
	for (tok = strtok(pcopy,Layerdelims); tok; tok = strtok(NULL,Layerdelims)) {
		ntok++;
		if (ntok > sz) {
			sz += SMALLBUF;
			LayerID = ALLOC(sz,LayerID,char*);
		}
		LayerID[ntok] = tok;
	}
	if (ntok) {
		LayerID = RALLOC(ntok+2,LayerID,char*);  /* shrink to minimum size */
		LayerID[0] = NULL;
		LayerID[ntok+1] = NULL;
	}

	return ntok;
}

static point exch_xy(point p)
{
	int		t;
	t = p.x; p.x = p.y; p.y = t;
	return p;
}

static pointf exch_xyf(pointf p)
{
	double		t;
	t = p.x; p.x = p.y; p.y = t;
	return p;
}

static void set_pagedir(graph_t* g)
{
	char	*str;

	Major.x = Major.y = Minor.x = Minor.y = 0;
	str = agget(g,"pagedir");
	if (str && str[0]) {
		Major = pagecode(str[0]);
		Minor = pagecode(str[1]);
	}
	if ((abs(Major.x + Minor.x) != 1) || (abs(Major.y + Minor.y) != 1)) {
		Major.x = 0; Major.y = 1; Minor.x = 1; Minor.y = 0;
		First.x = First.y = 0;
		if (str) agerr (AGWARN, "pagedir=%s ignored\n",str);
	}
}

/* this isn't a pretty sight... */
void setup_graph(graph_t *g)
{
	double		xscale,yscale,scale;
	char		*p;
	point		PFCLM;	/* page for centering less margins */
	point		DS;		/* device drawable region for a page of the graph */

	assert((GD_bb(g).LL.x == 0) && (GD_bb(g).LL.y == 0));

	if ((p = agget(g,"layers")) != 0) {
		if (gvrender_features(GD_gvc(g)) & GVRENDER_DOES_LAYERS) {
			Nlayers = parse_layers(g,p);
		} else {
			agerr(AGWARN, "layers not supported in %s output\n",
				lang_name(Output_lang));
			Nlayers = 0;
		}
	}
	else {LayerID = NULL; Nlayers = 0;}

	/* determine final drawing size and scale to apply. */
	/* N.B. magnification could be allowed someday in the next conditional */
	/* N.B. size given by user is not rotated by landscape mode */
	if ((GD_drawing(g)->size.x > 0)	/* was given by user... */
		&& ((GD_drawing(g)->size.x < GD_bb(g).UR.x) /* drawing is too big... */
			|| (GD_drawing(g)->size.y < GD_bb(g).UR.y))) {
		xscale = ((double)GD_drawing(g)->size.x) / GD_bb(g).UR.x;
		yscale = ((double)GD_drawing(g)->size.y) / GD_bb(g).UR.y;
		scale = MIN(xscale,yscale);
		GD_drawing(g)->scale = scale;
		GD_drawing(g)->size.x = scale * GD_bb(g).UR.x;
		GD_drawing(g)->size.y = scale * GD_bb(g).UR.y;
	}
	else {	/* go with "natural" size of layout */
		GD_drawing(g)->size = GD_bb(g).UR;
		scale = GD_drawing(g)->scale = 1.0;
	}

	/* determine pagination */
	PB.LL = GD_drawing(g)->margin;
	if ((GD_drawing(g)->page.x > 0) && (GD_drawing(g)->page.y > 0)) {
			/* page was set by user */
		point	tp;
		PFC = GD_drawing(g)->page;
		PFCLM.x = PFC.x - 2*PB.LL.x; PFCLM.y = PFC.y - 2*PB.LL.y;
		GP.x = PFCLM.x ; GP.y = PFCLM.y;	/* convert to double */
		if (GD_drawing(g)->landscape) GP = exch_xyf(GP);
		GP.x = GP.x / scale; GP.y = GP.y / scale;
			/* we don't want graph page to exceed its bounding box */
		GP.x = MIN(GP.x,GD_bb(g).UR.x); GP.y = MIN(GP.y,GD_bb(g).UR.y);
		Pages.x = (GP.x > 0) ? ceil( ((double)GD_bb(g).UR.x) / GP.x) : 1;
		Pages.y = (GP.y > 0) ? ceil( ((double)GD_bb(g).UR.y) / GP.y) : 1;
		N_pages = Pages.x * Pages.y;

			/* find the drawable size in device coords */
		tp = GD_drawing(g)->size;
		if (GD_drawing(g)->landscape) tp = exch_xy(tp);
		DS.x = MIN(tp.x,PFCLM.x);
		DS.y = MIN(tp.y,PFCLM.y);
	}
	else {
			/* page not set by user, assume default when centering,
				but allow infinite page for any other interpretation */
		GP.x = GD_bb(g).UR.x; GP.y = GD_bb(g).UR.y;
		PFC.x = DEFAULT_PAGEWD; PFC.y = DEFAULT_PAGEHT;
		PFCLM.x = PFC.x - 2*PB.LL.x; PFCLM.y = PFC.y - 2*PB.LL.y;
		DS = GD_drawing(g)->size;
		if (GD_drawing(g)->landscape) DS = exch_xy(DS);
		Pages.x = Pages.y = N_pages = 1;
	}

	set_pagedir(g);
	/* determine page box including centering */
	if (GD_drawing(g)->centered) {
		point	extra;
		if ((extra.x = PFCLM.x - DS.x) < 0) extra.x = 0;
		if ((extra.y = PFCLM.y - DS.y) < 0) extra.y = 0;
		PB.LL.x += extra.x / 2; PB.LL.y += extra.y / 2;
	}
	PB.UR = add_points(PB.LL,DS);
	Deffontname = late_nnstring(g->proto->n,N_fontname,DEFAULT_FONTNAME);
	Deffontsize = late_double(g->proto->n,N_fontsize,DEFAULT_FONTSIZE,MIN_FONTSIZE);
}

static void emit_node(GVC_t *gvc, node_t* n)
{
	if (ND_shape(n) == NULL) return;
	if (node_in_layer(n->graph,n) && node_in_CB(n) && (ND_state(n) != Page)) {
		gvrender_begin_node(gvc,n);
		gvrender_begin_context(gvc);
		ND_shape(n)->fns->codefn(gvc);
		ND_state(n) = Page;
	        gvrender_end_context(gvc);
		gvrender_end_node(gvc);
	}
}

static void emit_edge(GVC_t *gvc, edge_t* e)
{
	int		i;
	char	*color,*style;
	char	**styles = 0;
	char	**sp;
	bezier	bz;
	boolean	saved = FALSE;
	double	scale;
	char	*p;

	if ((edge_in_CB(e) == FALSE) || (edge_in_layer(e->head->graph,e) == FALSE))
		return;

	gvrender_begin_edge(gvc,e);
	style = late_string(e,E_style,"");
		/* We shortcircuit drawing an invisible edge because the arrowhead
         * code resets the style to solid, and most of the code generators
		 * (except PostScript) won't honor a previous style of invis.
         */
	if (style[0]) {
      styles = parse_style(style);
      sp = styles;
	  while ((p = *sp++)) {
		if (streq(p, "invis")) {
			gvrender_end_edge(gvc);
			return;
        }
      }
    }
	color = late_string(e,E_color,"");
	scale = late_double(e,E_arrowsz,1.0,0.0);
	if (color[0] || styles) {
		gvrender_begin_context(gvc);
		if (styles) gvrender_set_style(gvc, styles);
		if (color[0]) {
			gvrender_set_pencolor(gvc, color);
			gvrender_set_fillcolor(gvc, color);
		}
		saved = TRUE;
	}
	if (ED_spl(e)) {
		for (i = 0; i < ED_spl(e)->size; i++) {
			bz = ED_spl(e)->list[i];
			if (gvrender_features(gvc) & GVRENDER_DOES_ARROWS) {
				gvrender_beziercurve(gvc, bz.list, bz.size, bz.sflag, bz.eflag);
			} else  {
				gvrender_beziercurve(gvc, bz.list, bz.size, FALSE, FALSE);
				if (bz.sflag)
					arrow_gen(gvc, bz.sp, bz.list[0], scale, bz.sflag);
				if (bz.eflag)
					arrow_gen(gvc, bz.ep, bz.list[bz.size-1], scale, bz.eflag);
			}
		}
	}
	if (ED_label(e)) {
		emit_label(gvc, ED_label(e));
		if (mapbool(late_string(e,E_decorate,"false")) && ED_spl(e))
			emit_attachment(gvc, ED_label(e),ED_spl(e));
	}
	if (ED_head_label(e)) emit_label(gvc, ED_head_label(e)); /* vladimir */
	if (ED_tail_label(e)) emit_label(gvc, ED_tail_label(e)); /* vladimir */

	if (saved) gvrender_end_context(gvc);
	gvrender_end_edge(gvc);
}

void emit_graph(GVC_t *gvc, int flags)
{
	point	curpage;
	graph_t	*sg;
	node_t	*n;
	edge_t	*e;
	int	c;
	char	*str;
	graph_t *g = gvc->g;

	G_peripheries =  agfindattr(g,"peripheries");
	setup_graph(g);
	if (Page == 0)
		gvrender_begin_job(gvc, Lib, Pages);
	gvrender_begin_graph(gvc, g, PB, PFC);
	if (flags & EMIT_COLORS) {
		gvrender_set_fillcolor(gvc, DEFAULT_FILL);
		if (((str = agget(g,"bgcolor")) != 0) && str[0])
			gvrender_set_fillcolor(gvc, str);
		if (((str = agget(g,"fontcolor")) != 0) && str[0])
			gvrender_set_pencolor(gvc, str);
		for (c = 1; c <= GD_n_cluster(g); c++) { 
	                gvc->sg = sg = GD_clust(g)[c];
			if (((str = agget(sg,"color")) != 0) && str[0])
				gvrender_set_pencolor(gvc, str);
			if (((str = agget(sg,"fillcolor")) != 0) && str[0])
				gvrender_set_fillcolor(gvc, str);
			if (((str = agget(sg,"fontcolor")) != 0) && str[0])
				gvrender_set_pencolor(gvc, str);
		}
		for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
			gvc->n = n;
			if (((str = agget(n,"color")) != 0) && str[0])
				gvrender_set_pencolor(gvc, str);
			if (((str = agget(n,"fillcolor")) != 0) && str[0])
				gvrender_set_fillcolor(gvc, str);
			if (((str = agget(n,"fontcolor")) != 0) && str[0])
				gvrender_set_pencolor(gvc, str);
			for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
				gvc->e = e;
				if (((str = agget(e,"color")) != 0) && str[0])
					gvrender_set_pencolor(gvc, str);
				if (((str = agget(e,"fontcolor")) != 0) && str[0])
					gvrender_set_pencolor(gvc, str);
			}
		}
	}

	Layer = 1;
	do {
		if (Nlayers > 0) gvrender_begin_layer(gvc,LayerID[Layer],Layer,Nlayers);
		for (curpage = First; validpage(curpage); curpage = pageincr(curpage)) {
			Obj = NONE;
			setup_page(gvc,curpage);
			if (GD_label(g)) emit_label(gvc, GD_label(g));
			Obj = CLST;
			/* when drawing, lay clusters down before nodes and edges */
			if (!(flags & EMIT_CLUSTERS_LAST)) {
				emit_clusters(gvc,g,flags);
			}
			if (flags & EMIT_SORTED) {
				/* output all nodes, then all edges */
				Obj = NODE;
				gvrender_begin_nodes(gvc);
				for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
					emit_node(gvc,n);
				}
				gvrender_end_nodes(gvc);
				Obj = EDGE;
				gvrender_begin_edges(gvc);
				for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
					for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
						emit_edge(gvc,e);
					}
				}
				gvrender_end_edges(gvc);
			}
			else if (flags & EMIT_EDGE_SORTED) {
				/* output all edges, then all nodes */
				Obj = EDGE;
				gvrender_begin_edges(gvc);
				for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
					for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
						emit_edge(gvc,e);
					}
				}
				gvrender_end_edges(gvc);
				Obj = NODE;
				gvrender_begin_nodes(gvc);
				for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
					emit_node(gvc,n);
				}
				gvrender_end_nodes(gvc);
			}
			else if(flags & EMIT_PREORDER) {
				Obj = NODE;
				gvrender_begin_nodes(gvc);
				for(n = agfstnode(g); n; n = agnxtnode(g, n)) {
					if(write_node_test(g, n))
						emit_node(gvc,n);
				}
				gvrender_end_nodes(gvc);
				Obj = EDGE;
				gvrender_begin_edges(gvc);

				for(n = agfstnode(g); n; n = agnxtnode(g, n)) {
					for(e = agfstout(g, n); e; e = agnxtout(g, e)) {
						if(write_edge_test(g, e))
							emit_edge(gvc,e);
					}
				}
				gvrender_end_edges(gvc);
			}
			else {
				/* output in breadth first graph walk order */
				for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
					Obj = NODE;
					emit_node(gvc,n);
					for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
						Obj = NODE;
						emit_node(gvc,e->head);
						Obj = EDGE;
						emit_edge(gvc,e);
					}
				}    
			}
			/* when mapping, detect events on clusters after nodes and edges */
			if (flags & EMIT_CLUSTERS_LAST) {
				emit_clusters(gvc,g,flags);
			}
			Obj = NONE;
			gvrender_end_page(gvc);
		}
		if (Nlayers > 0) gvrender_end_layer(gvc);
		Layer++;
	} while (Layer <= Nlayers);
        gvrender_end_graph(gvc);
}

void emit_eof(GVC_t* gvc)
{
	if (Page > 0) gvrender_end_job(gvc);
}

void emit_clusters(GVC_t* gvc, Agraph_t *g, int flags)
{
	int			i,c,filled;
	graph_t		*sg;
	point		A[4];
	char		*str,**style;
	node_t		*n;
	edge_t		*e;

	for (c = 1; c <= GD_n_cluster(g); c++) {
		sg = GD_clust(g)[c];
		if (clust_in_layer(sg) == FALSE) continue;
		/* when mapping, detect events on clusters after sub_clusters */
		if (flags & EMIT_CLUSTERS_LAST) {
			emit_clusters(gvc,sg,flags);
		}
		Obj = CLST;
		gvrender_begin_cluster(gvc, sg);
		gvrender_begin_context(gvc);
		filled = FALSE;
		if (((str = agget(sg,"style")) != 0) && str[0]) {
			gvrender_set_style(gvc, (style = parse_style(str)));
			for (i = 0; style[i]; i++) 
				if (strcmp(style[i],"filled")==0) {filled = TRUE; break;}
		}
		if (((str = agget(sg,"pencolor")) != 0) && str[0])
			gvrender_set_pencolor(gvc, str);
		else if (((str = agget(sg,"color")) != 0) && str[0])
			gvrender_set_pencolor(gvc, str);
		/* bgcolor is supported for backward compatability */
		else if (((str = agget(sg,"bgcolor")) != 0) && str[0])
			gvrender_set_pencolor(gvc, str);

		str = 0;
		if (((str = agget(sg,"fillcolor")) != 0) && str[0])
			gvrender_set_fillcolor(gvc, str);
		else if (((str = agget(sg,"color")) != 0) && str[0])
			gvrender_set_fillcolor(gvc, str);
		/* bgcolor is supported for backward compatability */
		else if (((str = agget(sg,"bgcolor")) != 0) && str[0]) {
			filled = TRUE;
			gvrender_set_fillcolor(gvc, str);
		}
		A[0] = GD_bb(sg).LL;
		A[2] = GD_bb(sg).UR;
		A[1].x = A[2].x; A[1].y = A[0].y;
		A[3].x = A[0].x; A[3].y = A[2].y;
		if (late_int(sg,G_peripheries,1,0)) {
			gvrender_polygon(gvc, A, 4, filled);
		}
		else if (filled) {
			gvrender_set_pencolor(gvc, str);
			gvrender_polygon(gvc, A, 4, filled);
        }
		if (GD_label(sg)) emit_label(gvc, GD_label(sg));

		if(flags & EMIT_PREORDER) {
			for(n = agfstnode(sg); n; n = agnxtnode(sg, n)) {
				Obj = NODE;
				emit_node(gvc,n);
				for(e = agfstout(sg, n); e; e = agnxtout(sg, e)) {
					Obj = EDGE;
					emit_edge(gvc,e);
				}
			}
			Obj = NONE;
		}

		gvrender_end_context(gvc);
		gvrender_end_cluster(gvc);
		/* when drawing, lay down clusters before sub_clusters */
		if (!(flags & EMIT_CLUSTERS_LAST)) {
			emit_clusters(gvc,sg,flags);
		}
	}
}

int node_in_CB(node_t* n)
{
	box	nb;

	if (N_pages == 1) return TRUE;
	nb.LL.x = ND_coord_i(n).x - ND_lw_i(n);
	nb.LL.y = ND_coord_i(n).y - ND_ht_i(n)/2;
	nb.UR.x = ND_coord_i(n).x + ND_rw_i(n);
	nb.UR.y = ND_coord_i(n).y + ND_ht_i(n)/2;
	return rect_overlap(CB,nb);
}

int node_in_layer(graph_t* g, node_t* n)
{
	char	*pn,*pe;
	edge_t	*e;

	if (Nlayers <= 0) return TRUE;
	pn = late_string(n,N_layer,"");
	if (selectedlayer(pn)) return TRUE;
	if (pn[0]) return FALSE;  /* Only check edges if pn = "" */
	if ((e = agfstedge(g,n)) == NULL) return TRUE;
	for (e = agfstedge(g,n); e; e = agnxtedge(g,e,n)) {
		pe = late_string(e,E_layer,"");
		if ((pe[0] == '\0') || selectedlayer(pe)) return TRUE;
	}
	return FALSE;
}

int edge_in_layer(graph_t* g, edge_t* e)
{
	char	*pe,*pn;
	int		cnt;

	if (Nlayers <= 0) return TRUE;
	pe = late_string(e,E_layer,"");
	if (selectedlayer(pe)) return TRUE;
	if (pe[0]) return FALSE;
	for (cnt = 0; cnt < 2; cnt++) {
		pn = late_string(cnt < 1? e->tail:e->head,N_layer,"");
		if ((pn[0] == '\0') || selectedlayer(pn)) return TRUE;
	}
	return FALSE;
}

int clust_in_layer(graph_t* sg)
{
	char		*pg;
	node_t		*n;

	if (Nlayers <= 0) return TRUE;
	pg = late_string(sg,agfindattr(sg,"layer"),"");
	if (selectedlayer(pg)) return TRUE;
	if (pg[0]) return FALSE;
	for (n = agfstnode(sg); n; n = agnxtnode(sg,n))
		if (node_in_layer(sg,n)) return TRUE;
	return FALSE;
}

int edge_in_CB(edge_t* e)
{
	int		i,j,np;
	bezier	bz;
	point	*p,pp,sz;
	box		b;
	textlabel_t	*lp;

	if (N_pages == 1) return TRUE;
	if (ED_spl(e) == NULL) return FALSE;
	for (i = 0; i < ED_spl(e)->size; i++) {
		bz = ED_spl(e)->list[i];
		np = bz.size;
		p = bz.list;
		pp = p[0];
		for (j = 0; j < np; j++) {
			if (rect_overlap(CB,mkbox(pp,p[j]))) return TRUE;
			pp = p[j];
		}
	}
	if ((lp = ED_label(e)) == NULL) return FALSE;
	sz = cvt2pt(lp->dimen);
	b.LL.x = lp->p.x - sz.x / 2; b.UR.x = lp->p.x + sz.x / 2;
	b.LL.y = lp->p.y - sz.y / 2; b.UR.y = lp->p.y + sz.y / 2;
	return rect_overlap(CB,b);
}

void emit_attachment(GVC_t *gvc, textlabel_t* lp, splines* spl)
{
	point	sz,A[3];
	char	*s;

	for (s = lp->text; *s; s++) if (isspace(*s) == FALSE) break;
	if (*s == 0) return;

	sz = cvt2pt(lp->dimen);
	A[0] = pointof(lp->p.x + sz.x/2,lp->p.y - sz.y/2);
	A[1] = pointof(A[0].x - sz.x, A[0].y);
	A[2] = dotneato_closest(spl,lp->p);
	/* Don't use edge style to draw attachment */
	gvrender_set_style(gvc, BaseLineStyle);
	gvrender_polyline(gvc, A, 3);
}

point pagecode(char c)
{
	point		rv;
	rv.x = rv.y = 0;
	switch(c) {
		case 'T': First.y = Pages.y - 1; rv.y = -1; break;
		case 'B': rv.y = 1; break;
		case 'L': rv.x = 1; break;
		case 'R': First.x = Pages.x - 1; rv.x = -1; break;
	}
	return rv;
}

int validpage(point page)
{
	return ((page.x >= 0) && (page.x < Pages.x) 
		&& (page.y >= 0) && (page.y < Pages.y));
}

int layerindex(char* tok)
{
	int		i;

	for (i = 1; i <= Nlayers; i++) 
		if (streq(tok,LayerID[i])) return i;
	return -1;
}

int is_natural_number(char* str)
{
	while (*str) if (NOT(isdigit(*str++))) return FALSE;
	return TRUE;
}

int layer_index(char* str, int all)
{
	int		i;

	if (streq(str,"all")) return all;
	if (is_natural_number(str)) return atoi(str);
	if (LayerID)
		for (i = 1; i <= Nlayers; i++)
			if (streq(str,LayerID[i])) return i;
	return -1;
}

int selectedlayer(char* spec)
{
	int		n0,n1;
	unsigned char	buf[SMALLBUF];
	char	*w0, *w1;
    agxbuf  xb;
    int     rval = FALSE;

    agxbinit (&xb, SMALLBUF, buf);
	agxbput(&xb,spec);
	w1 = w0 = strtok(agxbuse(&xb),Layerdelims);
	if (w0) w1 = strtok(NULL,Layerdelims);
	switch((w0 != NULL) + (w1 != NULL)) {
	case 0: 
      rval = FALSE;
      break;
	case 1: 
      n0 = layer_index(w0,Layer);
	  rval = (n0 == Layer);
      break;
	case 2: 
      n0 = layer_index(w0,0);  
      n1 = layer_index(w1,Nlayers);
	  if ((n0 < 0) || (n1 < 0)) rval = TRUE;
	  else if (n0 > n1) {
        int t = n0; 
        n0 = n1; 
        n1 = t;}
		rval = BETWEEN(n0,Layer,n1);
      break;
	}
    agxbfree (&xb);
	return rval;
}

point
pageincr(point page)
{
	page = add_points(page,Minor);
	if (validpage(page) == FALSE) {
		if (Major.y) page.x = First.x;
		else page.y = First.y;
		page = add_points(page,Major);
	}
	return page;
}

void emit_label(GVC_t *gvc, textlabel_t* lp)
{
	int	i, linespacing, left_x, center_x, right_x, width_x;
	point	p;

	if (lp->html) {
		emit_html_label (gvc, lp->u.html, lp);
		return;
	}

	/* make sure that there is something to do */
	if (lp->u.txt.nlines < 1) return;

	/* dimensions of box for label */
	width_x = ROUND(POINTS(lp->dimen.x));
	center_x = lp->p.x;
	left_x = center_x - width_x / 2;
	right_x = center_x + width_x / 2;

	/* set linespacing */
	linespacing = (int)(lp->fontsize * LINESPACING);

	/* position for first line */
	p.y = lp->p.y
		+ (linespacing * (lp->u.txt.nlines -1) / 2)	/* cl of topline */
	       	- lp->fontsize / 3.0 ; /* cl to baseline */

	gvrender_begin_context(gvc);
	gvrender_set_pencolor(gvc, lp->fontcolor);
	gvrender_set_font(gvc, lp->fontname, lp->fontsize*GD_drawing(gvc->g)->font_scale_adj);

	for (i = 0; i < lp->u.txt.nlines; i++) {
		switch(lp->u.txt.line[i].just) {
			case 'l':
				p.x = left_x;
				break;
			case 'r':
				p.x = right_x;
				break;
			default:
			case 'n':
				p.x = center_x;
				break;
		}
		gvrender_textline(gvc, p, &(lp->u.txt.line[i]));

		/* position for next line */
		p.y -= linespacing;
	}

	gvrender_end_context(gvc);
}

static int style_delim(int c)
{
	switch (c) {
		case '(': case ')': case ',': case '\0': return TRUE;
		default : return FALSE;
	}
}

#define SID 1

static int
style_token(char** s, agxbuf *xb)
{
    char* p = *s;
    int token;
    char  c;

	while (*p && (isspace(*p) || (*p ==','))) p++;
	switch (*p) {
	case '\0': token = 0; break;
	case '(': case ')': token = *p++; break;
	default: 
        token = SID;
        while (!style_delim(c = *p)) {
          agxbputc(xb,c);
          p++;
        }
	}
    *s = p;
	return token;
}

#define FUNLIMIT 64
static unsigned char outbuf[SMALLBUF];
static agxbuf ps_xb;

static void
cleanup()
{
  agxbfree(&ps_xb);
}

char **
parse_style(char* s)
{
	static char*   parse[FUNLIMIT];
    static int     first = 1;
	int			   fun = 0;
	boolean		   in_parens = FALSE;
	unsigned char  buf[SMALLBUF];
    char*          p;
    int            c;
    agxbuf         xb;

    if (first) {
      agxbinit (&ps_xb, SMALLBUF, outbuf);
      atexit(cleanup);
      first = 0;
    }

    agxbinit (&xb, SMALLBUF, buf);
	p = s;
	while ((c = style_token(&p,&xb)) != 0) {
		switch (c) {
		case '(':
			if (in_parens) {
				agerr(AGERR, "nesting not allowed in style: %s\n",s);
				parse[0] = (char*)0;
                agxbfree (&xb);
				return parse;
			}
			in_parens = TRUE;
			break;

		case ')':
			if (in_parens == FALSE) {
				agerr(AGERR, "unmatched ')' in style: %s\n",s);
				parse[0] = (char*)0;
                agxbfree (&xb);
				return parse;
			}
			in_parens = FALSE; 
			break;

		default:
			if (in_parens == FALSE) {
                if (fun == FUNLIMIT-1) {
                    agerr(AGWARN, "truncating style '%s'\n", s);
	                parse[fun] = (char*)0;
                    agxbfree (&xb);
                    return parse;
                }
                agxbputc(&ps_xb, '\0'); /* terminate previous */
				parse[fun++] = agxbnext(&ps_xb);
			}
			agxbput (&ps_xb, agxbuse(&xb));
            agxbputc(&ps_xb, '\0');
		}
	}

	if (in_parens) {
		agerr(AGERR, "unmatched '(' in style: %s\n",s);
		parse[0] = (char*)0;
        agxbfree (&xb);
		return parse;
	}
	parse[fun] = (char*)0;
    agxbfree (&xb);
    agxbuse (&ps_xb);  /* adds final '\0' to buffer */
	return parse;
}

static struct cg_s {
        codegen_t   *cg;
        char        *name;
        int         id;
    }
    gens[] = {
        {&PS_CodeGen,"ps",POSTSCRIPT},
        {&PS_CodeGen,"ps2",PDF},
        {&HPGL_CodeGen,"hpgl",HPGL},
        {&HPGL_CodeGen,"pcl",PCL},
        {&MIF_CodeGen,"mif",MIF},
        {&PIC_CodeGen,"pic",PIC_format},
        {&GD_CodeGen,"gd",GD},
#ifdef HAVE_LIBZ
        {&GD_CodeGen,"gd2",GD2},
#endif
#ifdef WITH_GIF
        {&GD_CodeGen,"gif",GIF},
#endif
#ifdef HAVE_LIBJPEG
        {&GD_CodeGen,"jpg",JPEG},
        {&GD_CodeGen,"jpeg",JPEG},
#endif
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
        {&GD_CodeGen,"png",PNG},
#endif
#endif
        {&GD_CodeGen,"wbmp",WBMP},
#ifdef HAVE_LIBXPM
        {&GD_CodeGen,"xbm",XBM},
#endif
        {&ISMAP_CodeGen,"ismap",ISMAP},
        {&IMAP_CodeGen,"imap",IMAP},
        {&CMAP_CodeGen,"cmap",CMAP},
        {&CMAPX_CodeGen,"cmapx",CMAPX},
#ifdef HAVE_LIBPNG
        {&VRML_CodeGen,"vrml",VRML},
#endif
        {&VTX_CodeGen,"vtx",VTX},
        {&MP_CodeGen,"mp",METAPOST},
        {&FIG_CodeGen,"fig",FIG},
        {&SVG_CodeGen,"svg",SVG},
#ifdef HAVE_LIBZ
        {&SVG_CodeGen,"svgz",SVGZ},
        {&DIA_CodeGen,"dia",DIA},
#endif
        {(codegen_t*)0,"dot",ATTRIBUTED_DOT},
        {(codegen_t*)0,"canon",CANONICAL_DOT},
        {(codegen_t*)0,"plain",PLAIN},
        {(codegen_t*)0,"plain-ext",PLAIN_EXT},
        {&XDot_CodeGen,"xdot",EXTENDED_DOT},
        {(codegen_t*)0,(char*)0,0}
    };

int lang_select(GVC_t *gvc, char* str, int warn)
{
    struct  cg_s *p;
    for (p = gens; p->name; p++) {
	if (strcasecmp(str,p->name) == 0) {
		gvc->codegen = p->cg;
		return p->id;
        }
    }
	if (warn) {
    	agerr(AGWARN, "language %s not recognized, use one of:\n",str);
    	for (p = gens; p->name; p++) agerr(AGPREV, " %s",p->name);
    	agerr(AGPREV, "\n");
	}
    return ATTRIBUTED_DOT;
}

char*
lang_name (int langID)
{
    struct  cg_s *p;
    for (p = gens; p->name; p++) {
		if (p->id == langID) return p->name;
	}
	return "<unknown output format>";
}

FILE *
file_select(char* str)
{
    FILE    *rv;
    rv = fopen(str,"wb");
    if (rv == NULL) { perror(str); exit(1); }
    return rv;
}

void use_library(char* name)
{
    static int  cnt = 0;
    if (name) {
        Lib = ALLOC(cnt+2,Lib,char*);
        Lib[cnt++] = name;
        Lib[cnt] = NULL;
    }
}
