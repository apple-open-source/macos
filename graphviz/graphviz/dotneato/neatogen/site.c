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
#include "mem.h"
#include "site.h"
#include <math.h>


int             siteidx;
Site            *bottomsite;

static Freelist         sfl;
static int              nvertices;

void
siteinit()
{
    /* double sn; */

    freeinit(&sfl, sizeof (Site));
    nvertices = 0;
    /* sn = nsites+4; */
    /* sqrt_nsites = sqrt(sn); */
}


Site *
getsite ()
{
    return ((Site *) getfree(&sfl));
}

double 
dist(Site *s, Site *t)
{
    double ans;
    double dx,dy;

    dx = s->coord.x - t->coord.x;
    dy = s->coord.y - t->coord.y;
    ans = sqrt(dx*dx + dy*dy);
    return ans;
}


void 
makevertex(Site *v)
{
    v -> sitenbr = nvertices;
    nvertices += 1;
#ifdef STANDALONE
    out_vertex(v);
#endif
}


void
deref(Site *v)
{
    v -> refcnt -= 1;
    if (v -> refcnt == 0 ) makefree(v, &sfl);
}

void
ref(Site *v)
{
    v -> refcnt += 1;
}
