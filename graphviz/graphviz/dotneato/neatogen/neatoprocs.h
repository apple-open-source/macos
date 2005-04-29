/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

extern int			allow_edits(int);
extern void		avoid_cycling(graph_t *, Agnode_t *, double *);
extern shape_desc *bind_shape(char* name);
extern Agnode_t	*choose_node(graph_t *, int);
extern int		circuit_model(graph_t *, int);
extern void		D2E(Agraph_t *, int, int, double *);
extern void		diffeq_model(graph_t *, int);
extern double		distvec(double *, double *, double *);
extern void		do_graph_label(Agraph_t *);
extern void		final_energy(graph_t *, int);
extern double		doubleattr(void*, int, double);
extern double		fpow32(double);
extern void		heapdown(Agnode_t *);
extern void		heapup(Agnode_t *);
extern void		initial_positions(graph_t *, int);
extern int		init_port(Agnode_t *, Agedge_t *, char *, boolean);
extern void		jitter3d(Agnode_t *, int);
extern void		make_spring(graph_t *, Agnode_t *, Agnode_t *, double);
extern void		move_node(graph_t *, int, Agnode_t *);
extern int		init_nop(graph_t* g);
extern void neato_nodesize(node_t* n, boolean flip);
extern void neato_cleanup(graph_t* g);
extern void neato_cleanup_edge(edge_t* e);
extern void neato_cleanup_graph(graph_t* g);
extern void neato_cleanup_node(node_t* n);
extern void		neato_compute_bb(Agraph_t *);
extern node_t	*neato_dequeue(void);
extern void		neato_enqueue(node_t *);
extern void neato_free_splines(edge_t* e);
extern void		neato_init_node_edge(Agraph_t *);
extern void		neato_init_edge(Agedge_t *);
extern void		neato_init_node(Agnode_t *);
extern void neato_layout(Agraph_t *g);
extern void neato_init_graph(graph_t *g);
extern void		randompos(Agnode_t *, int);
extern void		s1(graph_t *, node_t *);
extern int		scan_graph(graph_t *);
extern void		free_scan_graph(graph_t *);
extern void		shortest_path(graph_t *, int);
extern void		solve(double *, double *, double *, int);
extern void		solve_model(graph_t *, int);
extern void		spline_edges(Agraph_t *);
extern void     spline_edges0(graph_t *g);
extern void		neato_set_aspect(graph_t *g);
extern void		toggle(int);
extern int			user_pos(Agsym_t *, Agnode_t *, int);
extern double **new_array(int i, int j, double val);
extern void free_array(double **rv);
extern int matinv(double **A, double **Ainv, int n);
