/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

/* adjust.c
 * Routines for repositioning nodes after initial layout in
 * order to reduce/remove node overlaps.
 */
//#include "fdp.h"
//#include "options.h"
#include "common/Dynagraph.h"
#include "voronoi/voronoi.h"
#include "voronoi/info.h"
#include "voronoi/edges.h"

using namespace std;

namespace Voronoi {

/* chkBoundBox:
 *   Compute extremes of graph, then set up bounding box.
 *   If user supplied a bounding box, use that;
 *   else if "window" is a graph attribute, use that; 
 *   otherwise, define bounding box as a percentage expansion of
 *   graph extremes.
 *   In the first two cases, check that graph fits in bounding box.
 */
void VoronoiServer::chkBoundBox() {
	bounds.valid = false;
    for(vector<Info>::iterator ii = infos.nodes.begin(); ii !=infos.nodes.end(); ii++) 
		bounds |= gd<NodeGeom>(ii->layoutN).BoundingBox();

	if(bounds.valid) {
		Coord delta = bounds.Size()*margin;
		bounds = Rect(bounds.l-delta.x,bounds.t+delta.y,bounds.r+delta.x,bounds.b-delta.y);
	}
}

 /* makeInfo:
  * For each node in the graph, create a Info data structure 
  */
void VoronoiServer::makeInfo() {
	int N = current->nodes().size();
    infos.nodes.resize(N);

	Layout::node_iter ni = current->nodes().begin();
    vector<Info>::iterator ii = infos.nodes.begin();
    for(; ni!=current->nodes().end(); ++ni,++ii) {
		Layout::Node *n = *ni;

        ii->site.coord = gd<NodeGeom>(n).pos;

        ii->site.sitenbr = ii-infos.nodes.begin();
        ii->site.refcnt = 1;
        ii->layoutN = n;
        ii->verts = 0;
    }
}

/* sort sites on y, then x, coord */
struct scomp {
	bool operator ()(Site *s1,Site *s2) {
		if(s1 -> coord.y < s2 -> coord.y) return true;
		if(s1 -> coord.y > s2 -> coord.y) return false;
		if(s1 -> coord.x < s2 -> coord.x) return true;
		return false;
	}
};
 /* sortSites:
  * Fill array of pointer to sites and sort the sites using scomp
  */
void VoronoiServer::sortSites(vector<Site*> &sort) {
	int nsites = current->nodes().size();
    vector<Site*>::iterator sp;
    vector<Info>::iterator ip;

	sort.resize(nsites);
    sp = sort.begin();
    ip = infos.nodes.begin();
    for (; ip!=infos.nodes.end(); ++ip) {
        *sp++ = &(ip->site);
        ip->verts = NULL;
        ip->site.refcnt = 1;
    }

	std::sort(sort.begin(),sort.end(),scomp());
}

void VoronoiServer::geomUpdate (vector<Site*> &sort) {
    sortSites (sort);

    /* compute ranges */
	hedges.range = Rect(sort[0]->coord);
    for(int i = 1; i < current->nodes().size(); i++) 
		hedges.range |= sort[i]->coord;
}
int VoronoiServer::countOverlap(int iter) {
    int          count = 0;
    int          i;

	int nsites = current->nodes().size();
    for (i = 0; i < nsites; ++i)
		infos.nodes[i].overlaps = false;

    for(vector<Info>::iterator ip = infos.nodes.begin(); ip!=infos.nodes.end(); ++ip)
		for(vector<Info>::iterator jp = ip+1; jp!=infos.nodes.end(); ++jp)
			if(gd<NodeGeom>(ip->layoutN).region.Overlaps(ip->site.coord, 
				jp->site.coord, gd<NodeGeom>(jp->layoutN).region)) {
			  count++;
			  ip->overlaps = jp->overlaps = true;
			}

			/*
    if (Verbose > 1)
      fprintf (stderr, "overlap [%d] : %d\n", iter, count);
	  */
    return count;
}

void VoronoiServer::increaseBoundBox() {
    double ydelta = incr * bounds.Height(),
		xdelta = incr * bounds.Width();

    bounds.r += xdelta;
    bounds.t += ydelta;
    bounds.l -= xdelta;
    bounds.b -= ydelta;
}


 /* isInterior:
  * return true if polygon in not on boundary of bounding box.
  */
bool VoronoiServer::isInterior (Info* ip) {
    Coord    a;
    PtItem*  pt;

    for (pt = ip->verts; pt; pt = pt->next) {
      a = pt->p;
      if ((a.x == bounds.l) || (a.x == bounds.r) ||
          (a.y == bounds.b) || (a.y == bounds.t)) return false;
    }
    return true;
}

 /* newpos;
  * The new position is the centroid of the
  * voronoi polygon. This is the weighted sum of the
  * centroids of a triangulation, normalized to the
  * total area.
  */
void VoronoiServer::newpos(Info* ip) {
    PtItem*  anchor = ip->verts;
	Coord ws(0,0);
    double totalArea = 0.0;


	if(anchor && anchor->next && anchor->next->next) {
#ifdef VORLINES
		{
		Layout *l = infos.nodes.front().layoutN->g;
		Line seg;
		seg.degree = 1;
		for(PtItem *p = anchor; p; p = p->next)
			seg.push_back(p->p);
		seg.push_back(anchor->p); 
		gd<Drawn>(l).push_back(seg);
		}
#endif
		for(PtItem *p = anchor->next,*q = p->next;q;p = q,q = q->next) {
			double area = tri_area(anchor->p, p->p, q->p);
			Coord ctr = centroid(anchor->p, p->p, q->p);
			ws += ctr*area;
			totalArea += area;
		}
		ip->site.coord = ws/totalArea;
	}
	else 
		ip->site.coord.x += 1;
}

 /* addCorners:
  * Add corners of clipping window to appropriate sites.
  * A site gets a corner if it is the closest site to that corner.
  */
void VoronoiServer::addCorners() {
	vector<Info>::iterator ip = infos.nodes.begin(),
		sws = ip,
		nws = ip,
		ses = ip,
		nes = ip;
	double   swd = dSquared(ip->site.coord, bounds.SW());
	double   nwd = dSquared(ip->site.coord, bounds.NW());
	double   sed = dSquared(ip->site.coord, bounds.SE());
	double   ned = dSquared(ip->site.coord, bounds.NE());
	double   d;
    
    while(++ip!=infos.nodes.end()) {
        d = dSquared(ip->site.coord, bounds.SW());
        if (d < swd) {
            swd = d;
            sws = ip;
        }
        d = dSquared(ip->site.coord, bounds.SE());
        if (d < sed) {
            sed = d;
            ses = ip;
        }
        d = dSquared(ip->site.coord, bounds.NW());
        if (d < nwd) {
            nwd = d;
            nws = ip;
        }
        d = dSquared(ip->site.coord, bounds.NE());
        if (d < ned) {
            ned = d;
            nes = ip;
        }
    }

    infos.addVertex (&sws->site, bounds.SW());
    infos.addVertex (&ses->site, bounds.SE());
    infos.addVertex (&nws->site, bounds.NW());
    infos.addVertex (&nes->site, bounds.NE());
}

 /* newPos:
  * Calculate the new position of each site as the centroid
  * of its voronoi polygon, if it overlaps other nodes.
  * The polygons are finite by being clipped to the clipping
  * window.
  * We first add the corner of the clipping windows to the
  * vertex lists of the appropriate sites.
  */
void VoronoiServer::newPos() {
    addCorners ();
    for(vector<Info>::iterator ii = infos.nodes.begin(); ii!=infos.nodes.end(); ++ii)
        if (doAll || ii->overlaps) 
			newpos(&*ii);
}

bool VoronoiServer::vAdjust () {
    int                iterCnt = 0;
    int                overlapCnt = 0;
    int                badLevel = 0;
    int                increaseCnt = 0;
    int                cnt;

    if (!useIter || (iterations > 0))
      overlapCnt = countOverlap (iterCnt);
    
    if ((overlapCnt == 0) || (iterations == 0))
      return 0;

	doAll = false;

	vector<Site*> sort;
    geomUpdate(sort);
    voronoi(sort); 
    while (1) {
      newPos ();
      iterCnt++;
      
      if (useIter && (iterCnt == iterations)) break;
      cnt = countOverlap (iterCnt);
      if (cnt == 0) break;
      if (cnt >= overlapCnt) badLevel++;
      else badLevel = 0;
      overlapCnt = cnt;

      switch (badLevel) {
      case 0:
        /* doAll = 1; */
        doAll = false;
        break;
/*
      case 1:
        doAll = 1;
        break;
*/
      default :
        doAll = true;
        increaseCnt++;
        increaseBoundBox ();
        break;
      }

      geomUpdate(sort);
      voronoi(sort); 
    }

	/*
    if (Verbose) {
      fprintf (stderr, "Number of iterations = %d\n", iterCnt);
      fprintf (stderr, "Number of increases = %d\n", increaseCnt);
    }
	*/

    return 1;
}
/*
static void
rePos (Coord c)
{
    int      i;
    Info*    ip = nodes;
    double   f = 1.0 + incr;

  
    for (i = 0; i < nsites; i++) {
      ip->site.coord.x = f*(ip->site.coord.x - c.x) + c.x;
      ip->site.coord.y = f*(ip->site.coord.y - c.y) + c.y;
      ip++;
    }

}

static int
sAdjust ()
{
    int                iterCnt = 0;
    int                overlapCnt = 0;
    int                increaseCnt = 0;
    int                cnt;
    Coord              center;

    if (!useIter || (iterations > 0))
      overlapCnt = countOverlap (iterCnt);
    
    if ((overlapCnt == 0) || (iterations == 0))
      return 0;

    center.x =  (pxmin + pxmax)/2.0;
    center.y =  (pymin + pymax)/2.0;
    while (1) {
      rePos (center);
      iterCnt++;
      
      if (useIter && (iterCnt == iterations)) break;
      cnt = countOverlap (iterCnt);
      if (cnt == 0) break;
    }

    if (Verbose) {
      fprintf (stderr, "Number of iterations = %d\n", iterCnt);
      fprintf (stderr, "Number of increases = %d\n", increaseCnt);
    }

    return 1;
}
*/
 /* updateGraph:
  * Enter new node positions into the graph
  */
void VoronoiServer::updateLayout(ChangeQueue &Q) {
    for(vector<Info>::iterator ii = infos.nodes.begin(); ii!=infos.nodes.end(); ++ii) {
		gd<NodeGeom>(ii->layoutN).pos = ii->site.coord;
		Q.ModNode(ii->layoutN,DG_UPD_MOVE);
	}
}
/*
static void 
normalize(graph_t *g)
{
	node_t	*v;
	edge_t	*e;

	double	theta;
	pointf	p;

	if (!mapbool(agget(g,"normalize"))) return;

	v = agfstnode(g); p.x = v->u.pos[0]; p.y = v->u.pos[1];
	for (v = agfstnode(g); v; v = agnxtnode(g,v))
		{v->u.pos[0] -= p.x; v->u.pos[1] -= p.y;}

	e = NULL;
	for (v = agfstnode(g); v; v = agnxtnode(g,v))
		if ((e = agfstout(g,v))) break;
	if (e == NULL) return;

	theta = -atan2(e->head->u.pos[1] - e->tail->u.pos[1],
		e->head->u.pos[0] - e->tail->u.pos[0]);

	for (v = agfstnode(g); v; v = agnxtnode(g,v)) {
		p.x = v->u.pos[0]; p.y = v->u.pos[1];
		v->u.pos[0] = p.x * cos(theta) - p.y * sin(theta);
		v->u.pos[1] = p.x * sin(theta) + p.y * cos(theta);
	}
}
*/
void VoronoiServer::Process(ChangeQueue &Q) {
	//Q.UpdateCurrent();
    makeInfo();

      /* establish and verify bounding box */
    chkBoundBox();

    if(vAdjust())
		updateLayout(Q);
#ifdef VORLINES
	Q.GraphUpdateFlags() |= DG_UPD_LINES;
#endif
}

} // namespace Voronoi
