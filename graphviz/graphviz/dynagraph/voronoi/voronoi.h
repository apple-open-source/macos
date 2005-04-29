/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef VORONOI_H
#define VORONOI_H

#include "voronoi/site.h"
#include "voronoi/info.h"
#include "voronoi/edges.h"
#include "voronoi/hedges.h"

namespace Voronoi {

struct VoronoiServer : Server {
	double margin;     /* Create initial bounding box by adding
                        * margin * dimension around box enclosing
                        * nodes.
                        */
	double incr;       /* Increase bounding box by adding
                        * incr * dimension around box.
                        */
	double pmargin;  /* Margin around polygons, in inches */
	int iterations;  /* Number of iterations */
	bool useIter;   /* Use specified number of iterations */

	bool doAll;  /* Move all nodes, regardless of overlap */
	Bounds bounds;     

	const int N;

	VoronoiServer(Layout *client, Layout *current) : 
		Server(client,current),margin(0.05),incr(0.025),pmargin(5.0/72),iterations(-1),useIter(false),
		N(400), infos(N),sites(N),edges(sites,infos,bounds,N),hedges(sites,N) {}


	// Server
	void Process(ChangeQueue &changeQ);

private:
	Infos infos;
	Sites sites;
	Edges edges;
	Halfedges hedges;

	void chkBoundBox();
	void makeInfo();
	void sortSites(std::vector<Site*> &sort);
	void geomUpdate(std::vector<Site*> &sort);
	int countOverlap(int iter);
	void increaseBoundBox();
	bool isInterior (Info* ip);
	void newpos(Info* ip);
	void addCorners();
	void newPos();
	bool vAdjust();
	void updateLayout(ChangeQueue &Q);

	void voronoi(const std::vector<Site*> &order);
};

} // namespace Voronoi

#endif // VORONOI_H
