#include <ns.h>

#ifndef uchar
#define uchar unsigned short
#define boolean uchar
#endif

typedef struct nsnode_s {
	Agrec_t			h;
	int 			n_rank;
	int				n_low,n_lim;
	int				n_priority;
	Agedge_t		*n_par;
	Agedge_t		*n_tree[2];
	uchar			n_mark,n_dmark,n_onstack;
} nsnode_t;

typedef struct nsedge_s {
	Agrec_t			h;
	int				e_cutval;
	int				e_weight;
	int				e_minlen;
	Agedge_t		*prv[2],*nxt[2];
	uchar			e_treeflag;
} nsedge_t;

typedef struct nsgraph_s {
	Agrec_t			h;
	int				g_n_tree_edges;
	int				g_n_nodes;
	int				g_maxiter;
	Agnode_t		*g_finger;
} nsgraph_t;
