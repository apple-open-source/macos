/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"

namespace DynaDAG {

void Config::resetRankBox(Rank *rank) {
#ifdef FLEXIRANKS
	Ranks::index r = ranking.y2r(rank->yBase);
	Ranks::iterator i = ranking.GetIter(r);
	rank->deltaAbove = 0;
	if(i!=ranking.begin())
		rank->deltaAbove = ((*--i)->yBase-rank->yBase)/2.0;
	i = ranking.GetIter(r);
	rank->deltaBelow = 0;
	if(++i!=ranking.end())
		rank->deltaBelow = (rank->yBase-(*i)->yBase)/2.0;
	if(!rank->deltaAbove)
		rank->deltaAbove = rank->deltaBelow;
	if(!rank->deltaBelow)
		rank->deltaBelow = rank->deltaAbove;
	rank->spaceBelow = 0;
	//rank->deltaBelow -= rank->spaceBelow = rank->deltaBelow/10.0;
#else
	double maxTop = nodeSep.y / 20.0,
		maxBottom = nodeSep.y / 20.0;
	for(NodeV::iterator ni = rank->order.begin(); ni!=rank->order.end(); ++ni) {
		if(DDd(*ni).amEdgePart()) 
			continue;
		double nt = TopExtent(*ni);
		if(maxTop < nt) 
			maxTop = nt;
		double nb = BottomExtent(*ni);
		if(maxBottom > nb) 
			maxBottom = nb;
	}

	rank->deltaAbove = maxTop;
	rank->deltaBelow = maxBottom;
	rank->spaceBelow = nodeSep.y;
#endif
}

void Config::resetBaselines() {
#ifndef FLEXIRANKS
	if(ranking.empty())
		return;
	if(prevLow == INT_MAX)
		prevLow = ranking.Low();
	Rank *base = ranking.GetRank(prevLow);
	Ranks::iterator start = ranking.GetIter(prevLow);

	/* work upward from prevLow rank */
	Rank *prev = base;
	// (Note reverse_iterator::operator* returns *(fwdIt-1))
	for(Ranks::reverse_iterator rri(start); rri!=ranking.rend(); ++rri) {
		Rank *rank = *rri;
#ifndef DOWN_GREATER
		rank->yBase = prev->yAbove(1.0) + rank->deltaBelow;
#else
		rank->yBase = prev->yAbove(1.0) - rank->deltaBelow;
#endif
		prev = rank;
	}

	prev = base;
	for(Ranks::iterator ri = start+1; ri!=ranking.end(); ++ri) {
		Rank *rank = *ri;
#ifndef DOWN_GREATER
		rank->yBase = prev->yBelow(1.0) - rank->deltaAbove;
#else
		rank->yBase = prev->yBelow(1.0) + rank->deltaAbove;
#endif
		prev = rank;
	}
	prevLow = ranking.Low();
#endif
}

void Config::SetYs() {
  Ranks::iterator ri;
	for(ri = ranking.begin(); ri!=ranking.end(); ++ri) 
		resetRankBox(*ri);

	resetBaselines();

	for(ri = ranking.begin(); ri!=ranking.end(); ++ri) 
		for(NodeV::iterator ni = (*ri)->order.begin(); ni!=(*ri)->order.end(); ++ni) {
			DDNode &ddn = DDd(*ni);
			double newY = (*ri)->yBase;
			ddn.cur.y = newY;
		}
}

} // namespace DynaDAG
