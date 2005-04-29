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

#include <aghdr.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void agflatten_elist(Dict_t *d, Dtlink_t **lptr)
{
	dtrestore(d,*lptr);
	(void) dtflatten(d);
	*lptr = dtextract(d);
}

void agflatten_edges(Agraph_t *g, Agnode_t *n)
{
	agflatten_elist(g->e_seq,(Dtlink_t**)&(n->out));
	agflatten_elist(g->e_seq,(Dtlink_t**)&(n->in));
}

void agflatten(Agraph_t *g, int flag)
{
	Agnode_t	*n;

	if (flag) {
		if (g->desc.flatlock == FALSE) {
			dtflatten(g->n_seq);
			g->desc.flatlock = TRUE;
			for (n = agfstnode(g); n; n = agnxtnode(n))
				agflatten_edges(g,n);
		}
	}
	else {
		if (g->desc.flatlock) {
			g->desc.flatlock = FALSE;
		}
	}
}

void agnotflat(Agraph_t *g)
{
	if(g->desc.flatlock) agerror(AGERROR_FLAT,"");
}
