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
#include <signal.h>
#include <stdio.h>
#include "agraph.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define NILgraph			NIL(Agraph_t*)
#define NILnode				NIL(Agnode_t*)
#define NILedge				NIL(Agedge_t*)
#define NILsym				NIL(Agsym_t*)
#define NILstr				NIL(char*)

void main(int argc, char **argv)
{
	Agraph_t 	*g;
	Agnode_t	*n;
	Agedge_t	*e;

	while (g = agread(stdin,NIL(Agdisc_t*))) {
		fprintf(stderr,"nodes %d edges %d\n",agnnodes(g), agnedges(g));
		for (n = agfstnode(g); n; n = agnxtnode(n)) {
			if (agdegree(n,TRUE,TRUE) == 0) 
				printf("u . \"%s\" {}\n",agnameof(n));
			else {
				for (e = agfstout(n); e; e = agnxtout(e)) {
					printf("u .> \"%s\" {} e%x {} ",agnameof(agtail(e)),e);
					printf("\"%s\" {}\n",agnameof(aghead(e)));
				}
			}
		}
	}
}
