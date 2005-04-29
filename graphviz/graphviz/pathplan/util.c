/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#include <assert.h>
#include <pathutil.h>
#include <stdlib.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

Ppoly_t copypoly(Ppoly_t argpoly)
{
    Ppoly_t rv;
    int     i;

    rv.pn = argpoly.pn;
    rv.ps = malloc(sizeof(Ppoint_t) * argpoly.pn);
    for (i = 0; i < argpoly.pn; i++) rv.ps[i] = argpoly.ps[i];
    return rv;
}

void freepoly(Ppoly_t argpoly)
{
    free(argpoly.ps);
}

int Ppolybarriers(Ppoly_t **polys, int npolys, Pedge_t **barriers, int *n_barriers)
{
	Ppoly_t		pp;
	int			i, j, k, n, b;
	Pedge_t		*bar;

	n = 0;
	for (i = 0; i < npolys; i++)
		n = n + polys[i]->pn;

	bar = malloc(n * sizeof(Pedge_t));

	b = 0;
	for (i = 0; i < npolys; i++) {
		pp = *polys[i];
		for (j = 0; j < pp.pn; j++) {
			k = j + 1;
			if (k >= pp.pn) k = 0;
			bar[b].a = pp.ps[j];
			bar[b].b = pp.ps[k];
			b++;
		}
	}
	assert(b == n);
	*barriers = bar;
	*n_barriers = n;
	return 1;
}
