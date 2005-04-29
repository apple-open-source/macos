/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"
#include "common/reorient.h"
#include <sstream>
using namespace std;
void ShapeGenerator::Process(ChangeQueue &Q) {
  Layout *subs[2] = {&Q.insN,&Q.modN};
  for(int i=0; i<2; ++i)
    for(Layout::node_iter ni = subs[i]->nodes().begin(); ni !=subs[i]->nodes().end(); ++ni) {
      Layout::Node *n = *ni;
      if((i==0 || igd<Update>(n).flags&DG_UPD_POLYDEF) && gd<IfPolyDef>(n).whether) {
		// hack: this fixes an apparent bug in gcc (genpoly will call clear() too)
		gd<Drawn>(n).clear(); 
        try {
		    genpoly(gd<PolyDef>(n),gd<Drawn>(n));
        }
        catch(BadPolyDef) {
            // silly users with your zero-gons and such
            gd<Drawn>(n).clear();
        }
		NodeGeom &ng = gd<NodeGeom>(n);
		ng.region.shape.Clear();
		if(gd<Drawn>(n).size()) {
			// it would be nice to make this section an update for a DG_UPD_DRAWN
			// flag, so that the user could specify gd<Drawn> instead of this shapegen...
			Line &biggest = gd<Drawn>(n).front(); // first one is biggest
			ng.region.shape.resize(biggest.size());
			for(size_t i = 0; i<biggest.size(); ++i) 
				ng.region.shape[i] = reorient(biggest[i],true,gd<Translation>(Q.current).orientation);
			ng.region.shape.degree = biggest.degree;
		}
		ng.region.updateBounds();
		Q.ModNode(n,DG_UPD_REGION|DG_UPD_DRAWN);
      }
    }
}
