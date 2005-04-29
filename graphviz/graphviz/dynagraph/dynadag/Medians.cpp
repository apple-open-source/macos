/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"
#include "common/weightedMedian.h"

namespace DynaDAG {

void autoMVal(DDModel::Node *n, UpDown dir) {
	doubV vec;
	DDModel::Node *n2;
	/*if(DDd(n).amNodePart()) {
		if(dir==UP)
			n2 = DDd(n).multi->top();
		else
			n2 = DDd(n).multi->bottom();
	}
	else*/
		n2 = n;
	// ick
	if(dir==UP)
		for(DDModel::inedge_iter ei = n->ins().begin(); ei!=n->ins().end(); ++ei) 
			vec.push_back(DDd((*ei)->tail).order);
	else
		for(DDModel::outedge_iter ei = n->outs().begin(); ei!=n->outs().end(); ++ei) 
			vec.push_back(DDd((*ei)->head).order);
	if(!vec.size()) 
		DDd(n).med[dir].exists = false;
	else {
		DDd(n).med[dir].exists = true;
		DDd(n).med[dir].val = weightedMedian(vec);
	}
	DDd(n).med[dir].cached = true;
}
void InvalidateMVal(DDModel::Node *n, UpDown dir) {
	DDd(n).med[dir].cached = false;
}
bool MValExists(DDModel::Node *n, UpDown dir) {
	DDNode &ddn = DDd(n);
	if(!ddn.med[dir].cached)
		autoMVal(n,dir);
	return ddn.med[dir].exists;
}
double MVal(DDModel::Node *n, UpDown dir) {
	DDNode &ddn = DDd(n);
	if(!ddn.med[dir].cached)
		autoMVal(n,dir);
	return ddn.med[dir].val;
}
void SetMVal(DDModel::Node *n, double val, UpDown dir) {
	DDNode &ddn = DDd(n);

	ddn.med[dir].exists = true;
	ddn.med[dir].cached = true;
	ddn.med[dir].val = val;
}

/* this could be a lot better.  forces a lot of needless
 * recomputation.  could cache node pointers from which median
 * was computed, etc.  just live with this for now.
 */
void InvalidateAdjMVals(DDModel::Node *n) {
	for(DDModel::inedge_iter ini = n->ins().begin(); ini!=n->ins().end(); ++ini)
		InvalidateMVal((*ini)->tail,DOWN);
	for(DDModel::outedge_iter outi = n->outs().begin(); outi!=n->outs().end(); ++outi)
		InvalidateMVal((*outi)->head,UP);
}

} // namespace DynaDAG
