/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "fdp.h"
#include "voronoi/voronoi.h"
#include "shortspline/shortspline.h"
struct FVSCombo : CompoundServer {
	FVSCombo(Layout *client,Layout *current) : CompoundServer(client,current) {
		actors.push_back(new FDP::FDPServer(client,current));
		actors.push_back(new Voronoi::VoronoiServer(client,current));
		actors.push_back(new ShortSpliner(client,current));
	}
};
