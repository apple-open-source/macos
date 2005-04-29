/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

typedef void (*nodesizefn_t)(Agnode_t*, boolean);

typedef struct {
  boolean (*swapEnds) (edge_t *e);   /* Should head and tail be swapped? */
  boolean (*splineMerge)(node_t* n); /* Is n a node in the middle of an edge? */
} splineInfo;

extern point		add_points(point, point);
extern void		arrow_flags (Agedge_t *e, int *sflag, int *eflag);
extern void		arrow_gen (GVC_t *gvc, point p, point u, double scale, int flag);
extern double		arrow_length (edge_t* e, int flag);
extern int		arrowEndClip (inside_t *inside_context, point* ps, int startp, int endp, bezier* spl, int eflag);
extern int		arrowStartClip (inside_t *inside_context, point* ps, int startp, int endp, bezier* spl, int sflag);
extern void		attach_attrs(Agraph_t *);
extern pointf		Bezier(pointf *, int, double, pointf *, pointf *);
extern void		bezier_clip (inside_t *inside_context, boolean (*insidefn)(inside_t *inside_context, pointf p), pointf* sp, boolean left_inside);
extern shape_desc *     bind_shape(char* name);
extern box		boxof(int, int, int, int);
extern void		cat_libfile(FILE *, char **, char **);
extern void		clip_and_install (edge_t*, edge_t*,point*, int, splineInfo*);
extern int		clust_in_layer(Agraph_t *);
extern char *		canontoken(char *str);
extern void		colorxlate(char *str, color_t *color, color_type target_type);
extern void		common_init_node(node_t *n);
extern int		common_init_edge(edge_t *e);
extern point		coord(node_t *n);
extern pointf		cvt2ptf(point);
extern point		cvt2pt(pointf);
extern Agnode_t		*dequeue(queue *);
extern void		do_graph_label(graph_t *sg);
extern point	dotneato_closest (splines *spl, point p);
extern void		graph_init(graph_t *g);
extern void    	dotneato_initialize(GVC_t *gvc, int, char **);
extern void    	setCmdName(char *);
extern void    	dotneato_usage(int);
extern char*	getFlagOpt (int argc, char** argv, int* idx);
extern void		dotneato_postprocess(Agraph_t *, nodesizefn_t);
extern void		dotneato_set_margins(Agraph_t *);
extern void		dotneato_eof(GVC_t *gvc);
extern void		dotneato_terminate(GVC_t *gvc);
extern void		dotneato_write(GVC_t *gvc);
extern void		dotneato_write_one(GVC_t *gvc);
extern int		edge_in_CB(Agedge_t *);
extern int		edge_in_layer(Agraph_t *, Agedge_t *);
extern double	elapsed_sec(void);
extern void		enqueue_neighbors(queue *, Agnode_t *, int);
extern void		emit_attachment(GVC_t *gvc, textlabel_t *, splines *);
extern void		emit_clusters(GVC_t *gvc, Agraph_t *g, int flags);
extern void		emit_eof(GVC_t *gvc);
extern void		emit_graph(GVC_t *gvc, int flags);
#define EMIT_SORTED 1
#define EMIT_COLORS 2
#define EMIT_CLUSTERS_LAST 4
#define EMIT_PREORDER 8
#define EMIT_EDGE_SORTED 16
extern void		emit_label(GVC_t *gvc, textlabel_t *);
extern void		emit_reset(GVC_t *gvc);
extern void		epsf_init(GVC_t *gvc);
extern void		epsf_free(GVC_t *gvc);
extern void		epsf_gencode(GVC_t *gvc);
extern FILE		*file_select(char *);
extern shape_desc	*find_user_shape(char *);
extern box		flip_rec_box(box b, point p);
extern point	flip_pt(point p);
extern pointf	flip_ptf(pointf p);
extern void		free_line(textline_t *);
extern void		free_label(textlabel_t *);
extern void		free_queue(queue *);
extern void		free_ugraph(graph_t *);
extern char		*gd_alternate_fontlist(char *font);
extern point		gd_textsize(char *str, char *fontname, double fontsz);
extern point		estimate_textsize(char *str, char *fontname, double fontsz);
extern void		getdoubles2pt(graph_t* g, char* name, point* result);
extern void		getdouble(graph_t* g, char* name, double* result);
extern void		global_def(char *, Agsym_t *(*fun)(Agraph_t *, char *, char *));
extern void		init_ugraph(graph_t *g);
extern point	invflip_pt(point p);
extern int		is_natural_number(char *);
extern pointf		label_size(GVC_t *gvc, char *, textlabel_t *, graph_t *);
extern int		late_attr(void*, char *);
extern int		late_bool(void *, Agsym_t *, int);
extern double		late_double(void*, Agsym_t *, double, double);
extern int		late_int(void*, Agsym_t *, int, int);
extern char		*late_nnstring(void*, Agsym_t *, char *);
extern char		*late_string(void*, Agsym_t *, char *);
extern int		layer_index(char *, int);
extern int		layerindex(char *);
extern char		*strdup_and_subst_graph(char *str, Agraph_t *g);
extern char		*strdup_and_subst_node(char *str, Agnode_t *n);
extern char		*strdup_and_subst_edge(char *str, Agedge_t *e);
extern char		*xml_string(char *s);
extern textlabel_t	*make_label(GVC_t *gvc, int, char *, double, char *, char *, graph_t *);
extern int		mapbool(char *);
extern int		maptoken(char *, char **, int *);
extern void		map_begin_cluster(graph_t *g);
extern void		map_begin_edge(Agedge_t *e);
extern void		map_begin_node(Agnode_t *n);
extern void		map_edge(Agedge_t *);
extern point		map_point(point);
extern box		mkbox(point, point);
extern point		neato_closest (splines *spl, point p);
extern bezier*	new_spline (edge_t* e, int sz);
extern FILE		*next_input_file(void);
extern Agraph_t		*next_input_graph(void);
extern int		node_in_CB(node_t *);
extern int		node_in_layer(Agraph_t *, node_t *);
extern void		osize_label(textlabel_t *, int *, int *, int *, int *);
extern point		pageincr(point);
extern point		pagecode(char);
extern char		**parse_style(char* s);
extern void		place_graph_label(Agraph_t *);
extern point		pointof(int, int);
extern void		point_init(GVC_t *gvc);
extern void		poly_init(GVC_t *gvc);
extern void		printptf(FILE *, point);
extern char		*ps_string(char *s);
extern void		rec_attach_bb(Agraph_t *);
extern void		record_init(GVC_t *gvc);
extern int		rect_overlap(box, box);
extern char*		safefile(char *shapefilename);
extern int		selectedlayer(char *);
extern void		setup_graph(graph_t *g);
extern void		shape_clip (node_t *n, point curve[4], edge_t *e); 
extern point		spline_at_y(splines* spl, int y);
extern void		start_timer(void);
extern int		strccnt(char *p, char c);
extern double	textwidth (GVC_t *gvc, char *str, char *fontname, double fontsz);
extern void		translate_bb(Agraph_t *, int);
extern Agnode_t		*UF_find(Agnode_t *);
extern void		UF_remove(Agnode_t *, Agnode_t *);
extern void		UF_setname(Agnode_t *, Agnode_t *);
extern void		UF_singleton(Agnode_t *);
extern Agnode_t		*UF_union(Agnode_t *, Agnode_t *);
extern void		use_library(char *);
extern char		*username();
extern int		validpage(point);
extern void		write_plain(GVC_t *gvc, FILE*);
extern void		write_plain_ext(GVC_t *gvc, FILE*);
extern void*		zmalloc(size_t);
extern void*		zrealloc(void*, size_t, size_t, size_t);
extern void*		gmalloc(size_t);
extern void*		grealloc(void*, size_t);

#if defined(_BLD_dot) && defined(_DLL)
#   define extern __EXPORT__
#endif
extern point		sub_points(point, point);
extern int		lang_select(GVC_t *gvc, char *, int);

extern void		toggle(int);
extern int		test_toggle();
#undef extern
