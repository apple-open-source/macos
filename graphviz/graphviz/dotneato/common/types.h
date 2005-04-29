/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#include "pathplan.h"

typedef unsigned char boolean;

typedef int (*qsort_cmpf)(const void*, const void*);
typedef int (*bsearch_cmpf) (const void *, const void *);

typedef struct Agraph_t graph_t;
typedef struct Agnode_t node_t;
typedef struct Agedge_t edge_t;
typedef struct Agsym_t attrsym_t;

typedef struct GVC_s GVC_t;
typedef struct gvrender_engine_s gvrender_engine_t;

typedef struct htmllabel_t htmllabel_t;

typedef struct pointf {
	double		x,y;
} pointf;

typedef struct inside_t {
	pointf *p;
	double *r;
	node_t *n;
	edge_t *e;
} inside_t;

typedef struct point {
	int			x,y;
} point;

typedef struct box {
	point		LL,UR;
} box;

typedef struct port {			/* internal edge endpoint specification */
	point		p;					/* aiming point */
	double		theta;				/* slope in radians */
	box*		bp;                 /* if not null, points to bbox of 
                                     * rectangular area that is port target
                                     */
	boolean	constrained,defined;
	unsigned char order;			/* for mincross */
} port;

typedef struct path {			/* internal specification for an edge spline */
	port		start,end;
	point		*ulpp, *urpp, *llpp, *lrpp;	/* tangents of near splines */
	int			nbox;				/* number of subdivisions */
	box			*boxes;				/* rectangular regions of subdivision */
	void		*data;
} path;

typedef struct bezier {
	point		*list;
	int			size;
	int			sflag, eflag;
	point		sp, ep;
} bezier;

typedef struct splines {
	bezier		*list;
	int			size;
} splines;

typedef struct textline_t {
	char			*str;
	short			width;
	char			just;
} textline_t;

typedef struct textlabel_t {
	char			*text, *fontname,*fontcolor;
	double			fontsize;
	pointf			dimen;
	point			p;
	union {
		struct {
			textline_t		*line;
			short			nlines;
		} txt;
		htmllabel_t*		html;
	} u;
	boolean         set;        /* true if position is set */
	boolean         html;       /* true if html label */
} textlabel_t;

typedef struct polygon_t {		/* mutable shape information for a node */
	int			regular; 			/* true for symmetric shapes */
	int			peripheries;		/* number of periphery lines */
	int			sides;				/* number of sides */
	double		orientation;		/* orientation of shape (+ve degrees) */
	double		distortion;			/* distortion factor - as in trapezium */
	double		skew;				/* skew factor - as in parallelogram */
	int			option;				/* ROUNDED, DIAGONAL corners, etc. */
	pointf		*vertices;		    /* array of vertex points */
} polygon_t;

typedef struct	stroke_t {	/* information about a single stroke */
	/* we would have called it a path if that term wasn't already used */
	int	nvertices;	/* number of points in the stroke */
	int	flags;		/* stroke style flags */
	pointf	*vertices;	/* array of vertex points */
} stroke_t;

/* flag definitions for stroke_t */
#define STROKE_CLOSED (1 << 0)
#define STROKE_FILLED (1 << 1)
#define STROKE_PENDOWN (1 << 2) 
#define STROKE_VERTICES_ALLOCATED (1 << 3) 

typedef struct	shape_t {	/* mutable shape information for a node */
	int	nstrokes;	/* number of strokes in array */
	stroke_t *strokes;	/* array of strokes */
	/* The last stroke must always be closed, but can be pen_up.
	 * It is used as the clipping path */
} shape_t;

typedef struct shape_functions {	/* read-only shape functions */
	void	(*initfn)(GVC_t *);	/* initializes shape from node u.shape_info structure */
	void	(*freefn)(GVC_t *);	/* frees  shape from node u.shape_info structure */
	port	(*portfn)(GVC_t *, char *); /* finds aiming point and slope of port */
	boolean	(*insidefn)(inside_t *inside_context, pointf); /* clips incident gvc->e spline on shape of gvc->n */
	int	(*pboxfn)(GVC_t *, int, box *, int *); /* finds box path to reach port */
	void	(*codefn)(GVC_t *);	/* emits graphics code for node */
} shape_functions;

typedef struct shape_desc {		/* read-only shape descriptor */
	char		*name;		/* as read from graph file */
	shape_functions *fns;		
	polygon_t	*polygon;	/* base polygon info */
	boolean		usershape;
} shape_desc;

#if ENABLE_CODEGENS

typedef struct codegen_t {
	void    (*reset)(void);
	void	(*begin_job)(FILE *ofp, graph_t *g, char **lib, char *user, char *info[], point pages);
	void 	(*end_job)(void);
	void	(*begin_graph)(graph_t* g, box bb, point pb);
	void	(*end_graph)(void);
	void	(*begin_page)(graph_t *g, point page, double scale, int rot, point offset);
	void	(*end_page)(void);
	void    (*begin_layer)(char *layerName, int n, int nLayers);
	void	(*end_layer)(void); 
	void    (*begin_cluster)(graph_t *g);
	void	(*end_cluster)(void); 
	void    (*begin_nodes)(void);
	void	(*end_nodes)(void); 
	void    (*begin_edges)(void);
	void	(*end_edges)(void); 
	void	(*begin_node)(node_t *n);
	void	(*end_node)(void);
	void	(*begin_edge)(edge_t *e);
	void	(*end_edge)(void);
	void	(*begin_context)(void);
	void	(*end_context)(void);
	void	(*set_font)(char *fontname, double fontsize);
	void	(*textline)(point p, textline_t *str);
	void	(*set_pencolor)(char *name);
	void	(*set_fillcolor)(char *name);
	void	(*set_style)(char **s);
	void	(*ellipse)(point p, int rx, int ry, int filled);
	void	(*polygon)(point *A, int n, int filled);
	void	(*beziercurve)(point *A, int n, int arrow_at_start, int arrow_at_end);
	void	(*polyline)(point *A,int n);
	boolean bezier_has_arrows;
	void	(*comment)(void* obj, attrsym_t* sym);
	point	(*textsize)(char *str, char *fontname, double fontsz);
	void	(*user_shape)(char *name, point *A, int sides, int filled);
	point	(*usershapesize)(node_t *n, char *name);
} codegen_t;

#endif

typedef struct queue {
	node_t	**store,**limit,**head,**tail;
} queue;

typedef struct adjmatrix_t {
	int		nrows,ncols;
	char	*data;
} adjmatrix_t;

typedef struct rank_t {
	int				n;			/* number of nodes in this rank			*/
	node_t			**v;		/* ordered list of nodes in rank		*/
	int				an;			/* globally allocated number of nodes	*/
	node_t			**av;		/* allocated list of nodes in rank		*/
	int 			ht1,ht2;	/* height below/above centerline		*/
	int 			pht1,pht2;	/* as above, but only primitive nodes 	*/
	boolean			candidate;	/* for transpose ()						*/
	boolean			valid;
	int				cache_nc;	/* caches number of crossings			*/
	adjmatrix_t		*flat;
} rank_t;

typedef enum engine_e {DOT, NEATO, TWOPI, FDP, CIRCULAR} engine_t;

typedef struct layout_t {
	double			quantum,scale,font_scale_adj;
	point			margin, page, size;
	struct {point size; double zoom; pointf focus;} viewport;
	boolean			landscape,centered;
	engine_t		engine;
} layout_t;

/* for "record" shapes */
typedef struct field_t {
	point		size;			/* its dimension */
	box			b;				/* its placement in node's coordinates */
	int			n_flds;	
	textlabel_t	*lp;			/* n_flds == 0 */
	struct field_t	**fld;		/* n_flds > 0 */
	int		LR;					/* if box list is horizontal (left to right) */
	char	*id;				/* user's identifier */
} field_t;

typedef struct hsbcolor_t {
	char			*name;
	unsigned char	h,s,b;
} hsbcolor_t;

/* possible representations of color in color_t */
typedef enum color_type_e {HSV_DOUBLE, RGBA_BYTE, RGBA_WORD, CMYK_BYTE} color_type;

/* color_t can hold a color spec in a choice or representations */
typedef struct color_s {
	union {
		double          HSV[3];
		unsigned char   rgba[4];
		unsigned char   cmyk[4];
		int             rrggbbaa[4];
	} u;
	color_type type;
} color_t;

typedef struct nlist_t {
      node_t        **list;
      int           size;
} nlist_t;

typedef struct elist {
      edge_t        **list;
      int           size;
} elist;

#define elist_fastapp(item,L) do {L.list[L.size++] = item; L.list[L.size] = NULL;} while(0)
#define elist_append(item,L)  do {L.list = ALLOC(L.size + 2,L.list,edge_t*); L.list[L.size++] = item; L.list[L.size] = NULL;} while(0)
#define alloc_elist(n,L)      do {L.size = 0; L.list = N_NEW(n + 1,edge_t*); } while (0)
#define free_list(L)          do {if (L.list) free(L.list);} while (0)

typedef struct Agraphinfo_t {
		/* to generate code */
	layout_t			*drawing;
	GVC_t                  *gvc;	/* rendering */
    textlabel_t        	*label;         /* if the cluster has a title */
	box					bb;				/* bounding box */
	point				border[4];		/* sizes of margins for graph labels */
	boolean				left_to_right,has_labels,has_Latin1char;
	int					ht1,ht2;		/* below and above extremal ranks */
    void*               alg;

#ifndef DOT_ONLY
		/* to place nodes */
	node_t				**neato_nlist;
	int					move;
	double				**dist,**spring,**sum_t,***t;
	int					ndim;
#endif
#ifndef NEATO_ONLY
		/* to have subgraphs */
	int					n_cluster;
	graph_t				**clust;
	node_t				*nlist;
    rank_t              *rank;
        /* fast graph node list */
    nlist_t             comp;
        /* connected components */
    node_t              *minset,*maxset;                /* set leaders */
    long                n_nodes;
        /* includes virtual */
    short               minrank,maxrank;

        /* various flags */
    boolean             has_flat_edges;
    boolean             showboxes;
	boolean				cluster_was_collapsed;

    int                 nodesep,ranksep;
    node_t              *ln,*rn;        /* left, right nodes of bounding box */


        /* for clusters */
    node_t              *leader,**rankleader;
    boolean             expanded;
    char                installed;
    char                set_type;
    char                label_pos;
    boolean             exact_ranksep;
#endif

} Agraphinfo_t;

#define GD_alg(g) (g)->u.alg
#define GD_bb(g) (g)->u.bb
#define GD_border(g) (g)->u.border
#define GD_cl_cnt(g) (g)->u.cl_cnt
#define GD_clust(g) (g)->u.clust
#define GD_cluster_was_collapsed(g) (g)->u.cluster_was_collapsed
#define GD_comp(g) (g)->u.comp
#define GD_dist(g) (g)->u.dist
#define GD_drawing(g) (g)->u.drawing
#define GD_exact_ranksep(g) (g)->u.exact_ranksep
#define GD_expanded(g) (g)->u.expanded
#define GD_gvc(g) (g)->u.gvc
#define GD_has_Latin1char(g) (g)->u.has_Latin1char
#define GD_has_labels(g) (g)->u.has_labels
#define GD_has_flat_edges(g) (g)->u.has_flat_edges
#define GD_ht1(g) (g)->u.ht1
#define GD_ht2(g) (g)->u.ht2
#define GD_inleaf(g) (g)->u.inleaf
#define GD_installed(g) (g)->u.installed
#define GD_label(g) (g)->u.label
#define GD_leader(g) (g)->u.leader
#define GD_left_to_right(g) (g)->u.left_to_right
#define GD_ln(g) (g)->u.ln
#define GD_maxrank(g) (g)->u.maxrank
#define GD_maxset(g) (g)->u.maxset
#define GD_minrank(g) (g)->u.minrank
#define GD_minset(g) (g)->u.minset
#define GD_move(g) (g)->u.move
#define GD_n_cluster(g) (g)->u.n_cluster
#define GD_n_nodes(g) (g)->u.n_nodes
#define GD_ndim(g) (g)->u.ndim
#define GD_neato_nlist(g) (g)->u.neato_nlist
#define GD_nlist(g) (g)->u.nlist
#define GD_nodesep(g) (g)->u.nodesep
#define GD_outleaf(g) (g)->u.outleaf
#define GD_rank(g) (g)->u.rank
#define GD_rankleader(g) (g)->u.rankleader
#define GD_ranksep(g) (g)->u.ranksep
#define GD_rn(g) (g)->u.rn
#define GD_set_type(g) (g)->u.set_type
#define GD_label_pos(g) (g)->u.label_pos
#define GD_showboxes(g) (g)->u.showboxes
#define GD_spring(g) (g)->u.spring
#define GD_sum_t(g) (g)->u.sum_t
#define GD_t(g) (g)->u.t

typedef struct Agnodeinfo_t {
	shape_desc			*shape;
	void				*shape_info;
	point				coord;
	double				width,height;
	int					ht,lw,rw;
	textlabel_t			*label;
    void*               alg;
	char				state;

#ifndef DOT_ONLY
	short				xsize,ysize;
	int					id,heapindex,hops;
	double				*pos,dist;
	boolean				pinned;
#endif
#ifndef NEATO_ONLY
    boolean             showboxes,has_port;

        /* fast graph */
    char                node_type,mark,onstack;
    char                ranktype,weight_class;
    node_t              *next,*prev;
    elist               in,out,flat_out,flat_in,other;
    graph_t             *clust;

        /* for union-find and collapsing nodes */
    int                 UF_size;
    node_t              *UF_parent;
    node_t              *inleaf,*outleaf;

        /* for placing nodes */
    int                 rank,order;     /* initially, order = 1 for ordered edges */
    int                 mval;
    elist               save_in,save_out;

        /* for network-simplex */
    elist               tree_in,tree_out;
    edge_t              *par;
    int                 low,lim;
    int                 priority;

    double              pad[1];
#endif

} Agnodeinfo_t;

#define ND_UF_parent(n) (n)->u.UF_parent
#define ND_UF_size(n) (n)->u.UF_size
#define ND_alg(n) (n)->u.alg
#define ND_border(n) (n)->u.border
#define ND_clust(n) (n)->u.clust
#define ND_coord_i(n) (n)->u.coord
#define ND_dist(n) (n)->u.dist
#define ND_flat_in(n) (n)->u.flat_in
#define ND_flat_out(n) (n)->u.flat_out
#define ND_has_port(n) (n)->u.has_port
#define ND_heapindex(n) (n)->u.heapindex
#define ND_height(n) (n)->u.height
#define ND_hops(n) (n)->u.hops
#define ND_ht_i(n) (n)->u.ht
#define ND_id(n) (n)->u.id
#define ND_in(n) (n)->u.in
#define ND_inleaf(n) (n)->u.inleaf
#define ND_label(n) (n)->u.label
#define ND_lim(n) (n)->u.lim
#define ND_ln(n) (n)->u.ln
#define ND_low(n) (n)->u.low
#define ND_lw_i(n) (n)->u.lw
#define ND_mark(n) (n)->u.mark
#define ND_mval(n) (n)->u.mval
#define ND_n_cluster(n) (n)->u.n_cluster
#define ND_next(n) (n)->u.next
#define ND_node_type(n) (n)->u.node_type
#define ND_onstack(n) (n)->u.onstack
#define ND_order(n) (n)->u.order
#define ND_other(n) (n)->u.other
#define ND_out(n) (n)->u.out
#define ND_outleaf(n) (n)->u.outleaf
#define ND_par(n) (n)->u.par
#define ND_pinned(n) (n)->u.pinned
#define ND_pos(n) (n)->u.pos
#define ND_prev(n) (n)->u.prev
#define ND_priority(n) (n)->u.priority
#define ND_rank(n) (n)->u.rank
#define ND_ranktype(n) (n)->u.ranktype
#define ND_rn(n) (n)->u.rn
#define ND_rw_i(n) (n)->u.rw
#define ND_save_in(n) (n)->u.save_in
#define ND_save_out(n) (n)->u.save_out
#define ND_shape(n) (n)->u.shape
#define ND_shape_info(n) (n)->u.shape_info
#define ND_showboxes(n) (n)->u.showboxes
#define ND_state(n) (n)->u.state
#define ND_tree_in(n) (n)->u.tree_in
#define ND_tree_out(n) (n)->u.tree_out
#define ND_weight_class(n) (n)->u.weight_class
#define ND_width(n) (n)->u.width
#define ND_xsize(n) (n)->u.xsize
#define ND_ysize(n) (n)->u.ysize

typedef struct Agedgeinfo_t {
	splines				*spl;
	port				tail_port,head_port;	/* might be used someday */
	textlabel_t			*label,*head_label,*tail_label;
	char				edge_type;
	char				label_ontop;
	edge_t				*to_orig;				/* for dot's shapes.c    */
    void*               alg;

#ifndef DOT_ONLY
	double				factor;
	double				dist;
	Ppolyline_t			path;
#endif
#ifndef NEATO_ONLY
    boolean             showboxes;
    boolean             conc_opp_flag;
    short               xpenalty;
    int                 weight;
    int                 cutvalue,tree_index;
    short               count,minlen;
	edge_t              *to_virt;
#endif

} Agedgeinfo_t;

#define ED_alg(e) (e)->u.alg
#define ED_conc_opp_flag(e) (e)->u.conc_opp_flag
#define ED_count(e) (e)->u.count
#define ED_cutvalue(e) (e)->u.cutvalue
#define ED_dist(e) (e)->u.dist
#define ED_edge_type(e) (e)->u.edge_type
#define ED_factor(e) (e)->u.factor
#define ED_head_label(e) (e)->u.head_label
#define ED_head_port(e) (e)->u.head_port
#define ED_label(e) (e)->u.label
#define ED_label_ontop(e) (e)->u.label_ontop
#define ED_minlen(e) (e)->u.minlen
#define ED_path(e) (e)->u.path
#define ED_showboxes(e) (e)->u.showboxes
#define ED_spl(e) (e)->u.spl
#define ED_tail_label(e) (e)->u.tail_label
#define ED_tail_port(e) (e)->u.tail_port
#define ED_to_orig(e) (e)->u.to_orig
#define ED_to_virt(e) (e)->u.to_virt
#define ED_tree_index(e) (e)->u.tree_index
#define ED_weight(e) (e)->u.weight
#define ED_xpenalty(e) (e)->u.xpenalty
