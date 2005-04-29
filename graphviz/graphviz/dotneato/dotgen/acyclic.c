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
 * Break cycles in a directed graph by depth-first search.
 */

#include "dot.h"


void acyclic(graph_t* g)
{
	int		c;
	node_t	*n;

	for (c = 0; c < GD_comp(g).size; c++) {
		GD_nlist(g) = GD_comp(g).list[c];
		for (n = GD_nlist(g); n; n = ND_next(n)) ND_mark(n) = FALSE;
		for (n = GD_nlist(g); n; n = ND_next(n)) dfs(n);
	}
}

void dfs(node_t* n)
{
	int		i;
	edge_t	*e;
	node_t	*w;
	
	if (ND_mark(n)) return;
	ND_mark(n) = TRUE;
	ND_onstack(n) = TRUE;
	for (i = 0; (e = ND_out(n).list[i]); i++) {
		w = e->head;
		if (ND_onstack(w)) { reverse_edge(e); i--; }
		else { if (ND_mark(w) == FALSE) dfs(w); }
	}
	ND_onstack(n) = FALSE;
}

void reverse_edge(edge_t* e)
{
	edge_t		*f;

	delete_fast_edge(e);
	if ((f = find_fast_edge(e->head,e->tail))) merge_oneway(e,f);
	else virtual_edge(e->head,e->tail,e);
}
