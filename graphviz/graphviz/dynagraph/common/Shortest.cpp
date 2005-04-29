/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <limits.h>
#include <deque>
#include <algorithm>
#include "common/PathPlan.h"

using namespace std;

struct PnL {
	const Coord *pp;
	PnL *link;
	PnL(const Coord *pp,PnL *link=0) : pp(pp),link(link) {}
};
struct Triangle;
struct TriEdge {
    PnL *pnl0p,
		*pnl1p;
    Triangle *ltp,
		*rtp;
	TriEdge() : pnl0p(0),pnl1p(0),ltp(0),rtp(0) {}
};
struct Triangle {
    int mark;
    struct TriEdge e[3];

	Triangle(PnL *pnlap,PnL *pnlbp,PnL *pnlcp) : mark(0) {
		e[0].pnl0p = pnlap; e[0].pnl1p = pnlbp;
		e[1].pnl0p = pnlbp; e[1].pnl1p = pnlcp;
		e[2].pnl0p = pnlcp; e[2].pnl1p = pnlap;
		for(int i=0;i<3;++i)
			e[i].ltp = this;
	}
	bool PointIn(Coord c) {
	        int sum=0;
		for(int ei = 0; ei < 3; ei++)
			if(ccw(*e[ei].pnl0p->pp,*e[ei].pnl1p->pp,c) != ISCW)
				sum++;
		return sum == 3 || sum == 0;
	}
};

typedef vector<PnL> PnLV;
typedef vector<PnL*> PnLPV;
typedef vector<Triangle> TriangleV;
struct PnLD {
	PnLD(int n) : begin(n),end(n-1) {
		impl.resize(n*2,0);
	}
	int apex;
	PnL *front() {
		assert(!empty());
		return impl[begin];
	}
	PnL *back() {
		assert(!empty());
		return impl[end];
	}
	int add_front(PnL *pnlp) {
		if(!empty())
			pnlp->link = front();
		impl[--begin] = pnlp;
		return begin;
	}
	int add_back(PnL *pnlp) {
		if(!empty())
			pnlp->link = back();
		impl[++end] = pnlp;
		return end;
	}
	void split_back(int index) {
        /* if the split is behind the apex, then reset apex */
		if(index>apex)
			apex = index;
		begin = index;
	}
	void split_front(int index) {
        /* if the split is in front of the apex, then reset apex */
		if(index<apex)
			apex = index;
		end = index;
	}
	int findSplit(PnL *pnlp) {
	        int index;
		for(index = begin; index < apex; index++)
			if(ccw(*impl[index+1]->pp, *impl[index]->pp, *pnlp->pp) == ISCCW)
				return index;
		for(index = end; index > apex; index--)
			if(ccw(*impl[index-1]->pp, *impl[index]->pp, *pnlp->pp) == ISCW)
				return index;
		return apex;
	}
private:
	PnLPV impl;
	int begin,end;
	bool empty() {
		return end<begin;
	}
};

static void triangulate(PnLPV &pnls,TriangleV &out);
static bool isDiagonal(PnLPV &pnls,unsigned pnli, unsigned pnlip2);
static void connectTris(Triangle &tri1, Triangle &tri2);
static bool markTriPath(Triangle &tri1, Triangle &tri2);

void PathPlan::Shortest(const Line &boundary, Segment endpoints, Line &out) {
	// former globals(!!!)
	PnLV pnls;
	PnLPV pnlps;
	TriangleV tris;
	PnLD dq(boundary.size());

    /* make sure polygon is CCW and load pnls array */
	pnls.reserve(boundary.size());
	double minx = HUGE_VAL;
	int minpi = -1;
    for(unsigned pi = 0; pi < boundary.size(); pi++)
        if(minx > boundary[pi].x)
            minx = boundary[pi].x, minpi = pi;
    Coord p2 = boundary[minpi],
		p1 = boundary[minpi == 0 ? boundary.size()-1 : minpi - 1],
		p3 = boundary[minpi == int(boundary.size()) - 1 ? 0 : minpi + 1];
    if(p1.x == p2.x && p2.x == p3.x && p3.y > p2.y ||
            ccw(p1, p2, p3) != ISCCW) 
        for(Line::const_reverse_iterator pi = boundary.rbegin(); pi != boundary.rend(); pi++) {
            if(pi != boundary.rbegin() && *pi == pi[-1])
                continue;
			pnls.push_back(&*pi);
			pnlps.push_back(&pnls.back());
        }
    else
        for(Line::const_iterator pi = boundary.begin(); pi!=boundary.end(); ++pi) {
            if(pi != boundary.begin() && *pi == pi[-1])
                continue;
			pnls.push_back(&*pi);
			pnlps.push_back(&pnls.back());
        }

	if(reportEnabled(r_shortestPath)) {
    report(r_shortestPath,"shortest: points (%d)\n", pnlps.size());
    for(PnLPV::iterator pnli = pnlps.begin(); pnli!=pnlps.end(); ++pnli)
        report(r_shortestPath,"%f,%f\n", (*pnli)->pp->x, (*pnli)->pp->y);
	}

    /* generate list of triangles */
    triangulate(pnlps,tris);

	if(reportEnabled(r_shortestPath)) {
		report(r_shortestPath,"triangles\n%d\n", tris.size());
		for(TriangleV::iterator trii = tris.begin(); trii!=tris.end(); ++trii) {
			for(int ei = 0; ei < 3; ei++)
				report(r_shortestPath,"(%f,%f) ", trii->e[ei].pnl0p->pp->x,
						trii->e[ei].pnl0p->pp->y);
			report(r_shortestPath,"\n");
		}
	}

    /* connect all pairs of triangles that share an edge */
        TriangleV::iterator ti;
	for(ti = tris.begin(); ti!=tris.end(); ++ti)
		for(TriangleV::iterator tj = ti+1; tj!=tris.end(); ++tj)
            connectTris(*ti,*tj);

    /* find first and last triangles */
	TriangleV::iterator ftrii,ltrii;
    for(ti = tris.begin(); ti!=tris.end(); ++ti)
        if(ti->PointIn(endpoints.a))
            break;
	if(ti==tris.end())
        throw EndpointNotInPolygon(false);
	ftrii = ti;
    for(ti = tris.begin(); ti!=tris.end(); ++ti)
        if(ti->PointIn(endpoints.b))
            break;
	if(ti==tris.end())
        throw EndpointNotInPolygon(true);
    ltrii = ti;

    /* mark the strip of triangles from eps[0] to eps[1] */
    check(markTriPath(*ftrii, *ltrii)); // prerror("cannot find triangle path");

    /* if endpoints in same triangle, use a single line */
    if(ftrii == ltrii) {
		out.Clear();
		out.degree = 1;
		out.push_back(endpoints.a);
		out.push_back(endpoints.b);
        return;
    }

    /* build funnel and shortest path linked list(in add2dq) */
	PnL epnls[2] = {&endpoints.a,&endpoints.b};
    dq.apex = dq.add_front(&epnls[0]);
	Triangle *trii = &*ftrii; 
    while(trii) {
		trii->mark = 2;

        /* find the left and right points of the exiting edge */
	int ei;
        for(ei = 0; ei < 3; ei++)
            if(trii->e[ei].rtp && trii->e[ei].rtp->mark == 1)
                break;
		PnL *lpnlp,*rpnlp;
        if(ei == 3) { /* in last triangle */
            if(ccw(endpoints.b, *dq.front()->pp,*dq.back()->pp) == ISCCW)
                lpnlp = dq.back(), rpnlp = &epnls[1];
            else
                lpnlp = &epnls[1], rpnlp = dq.back();
        } else {
            PnL *pnlp = trii->e[(ei + 1) % 3].pnl1p;
            if(ccw(*trii->e[ei].pnl0p->pp, *pnlp->pp,
                    *trii->e[ei].pnl1p->pp) == ISCCW)
                lpnlp = trii->e[ei].pnl1p, rpnlp = trii->e[ei].pnl0p;
            else
                lpnlp = trii->e[ei].pnl0p, rpnlp = trii->e[ei].pnl1p;
        }

        /* update deque */
        if(trii == &*ftrii) {
            dq.add_back(lpnlp);
            dq.add_front(rpnlp);
        } else {
            if(dq.front() != rpnlp && dq.back() != rpnlp) {
                /* add right point to deque */
				int splitindex = dq.findSplit(rpnlp);
                dq.split_back(splitindex);
				dq.add_front(rpnlp);
            } else {
                /* add left point to deque */
                int splitindex = dq.findSplit(lpnlp);
                dq.split_front(splitindex);
				dq.add_back(lpnlp);
            }
        }
		Triangle &tri = *trii;
        trii = 0;
        for(ei = 0; ei < 3; ei++)
            if(tri.e[ei].rtp && tri.e[ei].rtp->mark == 1) {
                trii = tri.e[ei].rtp;
                break;
            }
    }

	if(reportEnabled(r_shortestPath)) {
		report(r_shortestPath,"polypath");
		for(PnL *pnlp = &epnls[1]; pnlp; pnlp = pnlp->link)
			report(r_shortestPath," %f %f", pnlp->pp->x, pnlp->pp->y);
		report(r_shortestPath,"\n");
	}

	out.degree = 1;
	for(PnL *pnlp = &epnls[1]; pnlp; pnlp = pnlp->link)
		out.push_back(*pnlp->pp);
	reverse(out.begin(),out.end());
}

/* triangulate polygon */
static void triangulate(PnLPV &pnlps,TriangleV &out) {
    if(pnlps.size() > 3) {
        for(unsigned pnli = 0; pnli < pnlps.size(); pnli++) {
            unsigned pnlip1 =(pnli + 1) % pnlps.size(),
				pnlip2 =(pnli + 2) % pnlps.size();
            if(isDiagonal(pnlps, pnli, pnlip2)) {
				out.push_back(Triangle(pnlps[pnli], pnlps[pnlip1], pnlps[pnlip2]));
				pnlps.erase(pnlps.begin()+pnlip1);
                triangulate(pnlps, out);
                return;
            }
        }
        throw PathPlan::InvalidBoundary();
    } else
        out.push_back(Triangle(pnlps[0], pnlps[1], pnlps[2]));
}

/* check if (i, i + 2) is a diagonal */
static bool isDiagonal(PnLPV &pnlps, unsigned pnli, unsigned pnlip2) {
    /* neighborhood test */
    int pnlip1 =(pnli + 1) % pnlps.size(),
		pnlim1 =(pnli + pnlps.size() - 1) % pnlps.size();
    /* If P[pnli] is a convex vertex [ pnli+1 left of(pnli-1,pnli) ]. */
	bool res;
    if(ccw(*pnlps[pnlim1]->pp, *pnlps[pnli]->pp, *pnlps[pnlip1]->pp) == ISCCW)
        res = ccw(*pnlps[pnli]->pp, *pnlps[pnlip2]->pp,*pnlps[pnlim1]->pp) == ISCCW &&
               ccw(*pnlps[pnlip2]->pp, *pnlps[pnli]->pp,*pnlps[pnlip1]->pp) == ISCCW;
    /* Assume(pnli - 1, pnli, pnli + 1) not collinear. */
    else
        res = ccw(*pnlps[pnli]->pp, *pnlps[pnlip2]->pp,*pnlps[pnlip1]->pp) == ISCW;
    if(!res)
        return false;

    /* check against all other edges */
    for(unsigned pnlj = 0; pnlj < pnlps.size(); pnlj++) {
        unsigned pnljp1 =(pnlj + 1) % pnlps.size();
        if(!(pnlj == pnli || pnljp1 == pnli || 
			pnlj == pnlip2 || pnljp1 == pnlip2))
            if(segsIntersect(*pnlps[pnli]->pp, *pnlps[pnlip2]->pp,
                    *pnlps[pnlj]->pp, *pnlps[pnljp1]->pp))
                return false;
    }
    return true;
}
/* connect a pair of triangles at their common edge (if any) */
static void connectTris(Triangle &tri1, Triangle &tri2) {
    for(int ei = 0 ; ei < 3; ei++) {
        for(int ej = 0; ej < 3; ej++) {
            if((tri1.e[ei].pnl0p->pp == tri2.e[ej].pnl0p->pp &&
                    tri1.e[ei].pnl1p->pp == tri2.e[ej].pnl1p->pp) ||
                   (tri1.e[ei].pnl0p->pp == tri2.e[ej].pnl1p->pp &&
                    tri1.e[ei].pnl1p->pp == tri2.e[ej].pnl0p->pp))
                tri1.e[ei].rtp = &tri2, tri2.e[ej].rtp = &tri1;
        }
    }
}

/* find and mark path from tri1 to tri2 */
static bool markTriPath(Triangle &tri1, Triangle &tri2) {
    if(tri1.mark)
        return false;
    tri1.mark = 1;
    if(&tri1 == &tri2)
        return true;
    for(int ei = 0; ei < 3; ei++)
        if(tri1.e[ei].rtp && markTriPath(*tri1.e[ei].rtp, tri2))
            return true;
    tri1.mark = 0;
    return false;
}
