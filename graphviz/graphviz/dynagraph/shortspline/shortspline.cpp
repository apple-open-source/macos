/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <assert.h>
#include <math.h>
#include "shortspline/shortspline.h"

extern "C" {
#include "pathplan.h"
#include "vispath.h"
#include "pathutil.h"
#include "legal_arrangement.h"
}

using namespace std;

/* #define ARROWLENGTH 9  shouldn't this be ARROW_LENGTH from const.h? */
/* #define ARROWLENGTHSQ 81  bah */
#define sqr(a) ((long) (a) * (a))
#define dstsq(a, b) (sqr (a.x - b.x) + sqr (a.y - b.y))
#define ldstsq(a, b) (sqr ((long) a.x - b.x) + sqr ((long) a.y - b.y))
#define dst(a, b) sqrt ((double) dstsq (a, b))
#define P2PF(p, pf) (pf.x = p.x, pf.y = p.y)
#define PF2P(pf, p) (p.x = ROUND (pf.x), p.y = ROUND (pf.y))         




static void
make_barriers(Ppoly_t **poly, int npoly, int pp, int qp, Pedge_t **barriers, int *n_barriers){
    int     i, j, k, n, b;
    Pedge_t *bar;

    n = 0;
    for (i = 0; i < npoly; i++) {
        if (i == pp) continue;
        if (i == qp) continue;
        n = n + poly[i]->pn;
    }
    bar = reinterpret_cast<Pedge_t*>(malloc(n * sizeof(Pedge_t)));
    b = 0;
    for (i = 0; i < npoly; i++) {
        if (i == pp) continue;
        if (i == qp) continue;
        for (j = 0; j < poly[i]->pn; j++) {
            k = j + 1;
            if (k >= poly[i]->pn) k = 0;
            bar[b].a = poly[i]->ps[j];
            bar[b].b = poly[i]->ps[k];
            b++;
        }
    }
    assert(b == n);
    *barriers = bar;
    *n_barriers = n;
}
void ShortSpliner::Process(ChangeQueue &changeQ) {
	//changeQ.UpdateCurrent();
	changeQ.CalcBounds();

	/*
	point		dumb[4],d,ld;
	pointf		offset, polyp;
	Ppoly_t		**obs;
	polygon_t	*poly;
	int 		i, j, npoly, sides;
	extern void	polygon_init(node_t *);
	Ppoint_t	p,q;
	vconfig_t	*vconfig;
	Pedge_t     *barriers;
	char		*str;
	*/
	double		SEP = gd<GraphGeom>(changeQ.current).separation.Len();

	/* build configuration */

	int N = current->nodes().size();
	vector<Ppoly_t*> obs;
	obs.resize(N,0);
	vector<Ppoly_t> polys;
	polys.resize(N);
	vector<Line> polydat;
	polydat.resize(N);
	int i = 0;
	for(Layout::node_iter ni = current->nodes().begin(); ni!=current->nodes().end(); ++ni) {
		obs[i] = &polys[i];
		polydat[i] = gd<NodeGeom>(*ni).region.shape + gd<NodeGeom>(*ni).pos;
		reverse(polydat[i].begin(),polydat[i].end()); // pathplan wants CW
		polys[i].ps = reinterpret_cast<Ppoint_t*>(&*polydat[i].begin());
		polys[i].pn = polydat[i].size()-1;
		++i;
	}
	int npoly = i;
	vconfig_t	*vconfig;
	if (!Plegal_arrangement(&*obs.begin(),npoly)) {
		//if (Verbose) fprintf(stderr,"nodes touch - falling back to straight line edges\n");
		vconfig = 0;
	}
	else 
		vconfig = Pobsopen(&*obs.begin(),npoly);

	/* route edges  */
	for(Layout::graphedge_iter ei = current->edges().begin(); ei!=current->edges().end(); ++ei) {
		Layout::Edge *e = *ei;
		NodeGeom &tg = gd<NodeGeom>(e->tail),
			&hg = gd<NodeGeom>(e->head);
		EdgeGeom &eg = gd<EdgeGeom>(e);
		Ppoint_t p = *reinterpret_cast<Ppoint_t*>(&tg.pos),
			q = *reinterpret_cast<Ppoint_t*>(&hg.pos);
		Line unclipped;
		unclipped.degree = 1;
		if (vconfig && (e->tail != e->head)) {
		  Ppolyline_t line;
		  int			pp, qp;
		  
		  /* determine the polygons (if any) that contain the endpoints */
		  pp = qp = POLYID_NONE;
		  for (i = 0; i < npoly; i++) {
		    if ((pp == POLYID_NONE) && in_poly(*obs[i], p)) pp = i;
		    if ((qp == POLYID_NONE) && in_poly(*obs[i], q)) qp = i;
		  }
		  if(!Pobspath(vconfig, p, pp, q, qp, &line)) 
		    throw ClockwiseShapes();
		  Ppolyline_t spline;
		  Pvector_t	slopes[2];
		  int			n_barriers;
		  
		  Pedge_t     *barriers;
		  make_barriers(&*obs.begin(), npoly, pp, qp, &barriers, &n_barriers);
		  slopes[0].x = slopes[0].y = 0.0;
		  slopes[1].x = slopes[1].y = 0.0;
		  Coord *begin,*end;
		  spline.ps = 0;
		  if(!Proutespline (barriers, n_barriers, line, slopes, &spline)) {
		    begin = reinterpret_cast<Coord*>(spline.ps);
		    end = begin+spline.pn;
		    unclipped.degree = 3;
		  }
		  else {
		    begin = reinterpret_cast<Coord*>(line.ps);
		    end = begin+line.pn;
		    unclipped.degree = 1;
		  }
		  unclipped.clear();
		  unclipped.insert(unclipped.begin(),begin,end);
		  // free(spline.ps); // oh right, globals, good.
		  free(barriers);
		  free(line.ps);
		}
		if(unclipped.empty()) {
			unclipped.resize(4);
			unclipped[0] = tg.pos;
			unclipped[3] = hg.pos;
			if (e->tail != e->head) {
				Coord d = (unclipped[3] - unclipped[0])/3.0;
				unclipped[1] = unclipped[0]+d;
				unclipped[2] = unclipped[3]-d;
			}
			else {	/* self arc */
				Coord d(tg.region.boundary.Width()/2.0+.66 * SEP,0.0);
				unclipped[1] = unclipped[2] = unclipped[0]+d;
				unclipped[1].y += d.x;
				unclipped[2].y -= d.x;
			}
		}
		gd<EdgeGeom>(e).pos.ClipEndpoints(unclipped,tg.pos,eg.tailClipped?&tg.region:0,
			hg.pos,eg.headClipped?&hg.region:0);
		changeQ.ModEdge(e,DG_UPD_MOVE);
	}
}
