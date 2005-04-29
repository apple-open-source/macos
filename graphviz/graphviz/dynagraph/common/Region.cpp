/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Geometry.h"

using namespace std;

#include <algorithm>
void Region::updateBounds() {
	if(!shape.size()) {
		boundary.l = boundary.t = boundary.r = boundary.b = 0.0;
		boundary.valid = false;
	}
	else {
		assert(shape.degree!=0);
		boundary.l = boundary.r = shape.front().x;
		boundary.t = boundary.b = shape.front().y;
		for(Line::iterator i = shape.begin()+1; i!=shape.end(); ++i) {
			boundary.l = min(boundary.l,i->x);
			boundary.r = max(boundary.r,i->x);
#ifndef DOWN_GREATER
			boundary.t = max(boundary.t,i->y);
			boundary.b = min(boundary.b,i->y);
#else
			boundary.t = min(boundary.t,i->y);
			boundary.b = max(boundary.b,i->y);
#endif
		}
		boundary.valid = true;
	}
}
bool sameSide(const Coord &p0, const Coord &p1,const Coord &L0,const Coord &L1) {
	bool s0,s1;
	double a,b,c;

	/* a x + b y = c */
	a = -(L1.y - L0.y);
	b = (L1.x - L0.x);
	c = a * L0.x + b * L0.y;

	s0 = (a*p0.x + b*p0.y - c >= 0);
	s1 = (a*p1.x + b*p1.y - c >= 0);
	return (s0 == s1);
}

bool Region::sameSide(const Coord &p0, const Coord &p1, int seg) const {
	switch(shape.degree) {
	case 1: {
		int i = seg,
			i1 = (seg + 1) % (shape.size()-1);
		const Coord &L0 = shape[i],
			&L1 = shape[i1];
		return ::sameSide(p0,p1,L0,L1);
	}
	case 3: {
		double roots[4];
		/*
		if(fabs(p0.x)<1e-5 && fabs(p1.x)<1e-5) {
			Coord tmp[4];
			for(int i=0;i<4;++i)
				tmp[i] = Coord(shape[seg+i].x+1.0,shape[seg+i].y);
			return splineIntersectsLine(tmp,Segment(Coord(1.0,p0.y),Coord(1.0,p1.y)),roots)==0;
		}
		*/
		if(between2(p0,shape[seg],p1) || between2(p0,shape[(seg+shape.degree)%(shape.size()-1)],p1))
			return false;
		int nroots = splineIntersectsLine(&shape[seg],Segment(p0,p1),roots);
		return nroots==0;
	}
	default:
		assert(0);
		return false;
	}
}
bool Region::Hit(Coord P) const {
	int size = shape.size()-1;
	if(size<=0)
		return false;
	Coord O((boundary.l+boundary.r)/2.0,(boundary.b+boundary.t)/2.0); // origin
    if(!sameSide(P,O,lastOut))
		return false;
	bool s = false;
	int i = lastOut,
		i1 = (lastOut+shape.degree)%size,
		j = 0;
	if(shape.degree==1) {
		const Coord &Q = shape[i],
			&R = shape[i1];
		if((s = ::sameSide(P,Q,R,O)) && ::sameSide(P,R,O,Q)) 
			return true;
		j = 1;
	}
    for(; j < size/shape.degree; j++) {
        if(s) {
            i = i1; 
			i1 = (i + shape.degree) % size;
        }
		else {
            i1 = i; 
			i = (i + size - shape.degree) % size;
        }
        if(!sameSide(P,O,i)) {
            lastOut = i;
            return false;
        }
    }
    return true;
}
bool Region::Overlaps(Coord ofs1,Coord ofs2,const Region &r) {
	if(!((boundary+ofs1)&&(r.boundary+ofs2)))
		return false;
	Line sum1 = shape+ofs1,
		sum2 = r.shape+ofs2;
	return boundary.Contains(sum2[0]-ofs1)&&Hit(sum2[0]-ofs1) ||
		r.boundary.Contains(sum1[0]-ofs2)&&r.Hit(sum1[0]-ofs2) ||
		sum1&&sum2;
}
