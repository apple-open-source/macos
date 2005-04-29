/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <math.h>
#define W_DEGREE 5
// in Route.cpp
extern int splineIntersectsLine(const Coord *sps, Segment seg,double *roots); // both * are array[4]

// adapted from incr/edgeclip.c
inline Coord Bezier(const Coord *V, int degree, double t,Coord *Left=0, Coord *Right=0) {
    int i, j; /* Index variables  */
    struct { double x,y; } Vtemp[W_DEGREE + 1][W_DEGREE + 1];

    /* Copy control points  */
    for(j = 0; j <= degree; j++) {
        Vtemp[0][j].x = V[j].x;
		Vtemp[0][j].y = V[j].y;
    }

    /* Triangle computation */
    for(i = 1; i <= degree; i++) {
        for(j = 0; j <= degree - i; j++) {
            Vtemp[i][j].x =
                (1.0 - t) * Vtemp[i-1][j].x + t * Vtemp[i-1][j+1].x;
            Vtemp[i][j].y =
                (1.0 - t) * Vtemp[i-1][j].y + t * Vtemp[i-1][j+1].y;
        }
    }

    if(Left)
        for(j = 0; j <= degree; j++) {
            Left[j].x = Vtemp[j][0].x;
            Left[j].y = Vtemp[j][0].y;
		}
    if(Right)
        for(j = 0; j <= degree; j++) {
            Right[j].x = Vtemp[degree-j][j].x;
            Right[j].y = Vtemp[degree-j][j].y;
		}
	Coord ret;
	ret.x = Vtemp[degree][0].x;
	ret.y = Vtemp[degree][0].y;
    return ret;
}
struct segsizes : std::vector<double> {
        double &at(int i) { return operator[](i); } // for earlier gccs
	segsizes(Coord *ps, int n, int degree) {
		int nseg = (n-1)/degree,i;
		resize(nseg);
		if(!nseg)
			return;
		for(i = 0; i<nseg; ++i) {
			const Coord *start = ps+i*degree;
			if(degree==1) 
				at(i) = sqrt(dist(start[1],*start));
			else if(degree==3) {
				at(i) = 0;
				Coord last = *start;
				for(double t = 0.1; t<1; t+=0.1) {
					Coord n = Bezier(start,3,t);
					at(i) += sqrt(dist(n,last));
					last = n;
				}
				at(i) += sqrt(dist(start[3],last));
			}
		}
		double tot = 0;
		for(i = 0; i<nseg; ++i)
			tot += at(i);
		for(i = 0; i<nseg; ++i) {
			at(i) /= tot;
			if(i)
				at(i) += at(i-1);
		}
		assert(fabs(at(nseg-1)-1.0)<0.00001);
	}
};
template<typename Pred>
inline Coord bezier_find(Coord *pts,int degree,Pred &pred) {
	double high,low;
	if(pred(pts[0])) {// at(seg).y < at(seg+degree).y) { 
		high = 0.0; 
		low = 1.0; 
	}
	else  { 
		high = 1.0; 
		low = 0.0; 
	}
	Coord ret;
	do {
		double t = (high + low) / 2.0;
		ret = Bezier(pts,degree,t);
		if(pred(ret))
			high = t;
		else 
			low = t;
	} 
	while(absol(high - low) > .01); /* should be adaptive */
	return Position(ret);
}
struct dist_pred {
	Coord x;
	double len;
	dist_pred(Coord x,double len) : x(x),len(len) {}
	bool operator()(Coord y) {
		return dist(x,y)>len;
	}
};
inline std::pair<Coord,Coord> secant(Coord *ps,int n,int degree,const segsizes &sizes,double portion,double len) {
	assert(portion>=0.0 && portion <=1.0);
	int nseg = (n-1)/degree;
	int seg;
	for(seg = 0; seg<nseg; ++seg)
		if((sizes[seg]-portion)>-0.0001)
			break;
	assert(seg<nseg);
	double t = seg?(portion-sizes[seg-1])/(sizes[seg]-sizes[seg-1])
		:portion/sizes[0];
	std::pair<Coord,Coord> ret;
	ret.first = Bezier(ps+seg*degree,degree,t);
	if(absol(len)<0.00001) {
		ret.second = ret.first;
		return ret;
	}
	int i;
	if(len<0) {
		for(i = seg*degree; i>=0; i -= degree)
			if(dist(ps[i],ret.first)>-len)
				break;
		if(i<0) {
			ret.second = ps[0];
			return ret;
		}
	}
	else {
		for(i = (seg+1)*degree; i<n; i += degree)
			if(dist(ps[i],ret.first)>len)
				break;
		if(i>=n) {
			ret.second = ps[n-1];
			return ret;
		}
	}
	dist_pred dp(ret.first,absol(len));
	ret.second = bezier_find(ps+i,degree,dp);
	return ret;
}
// Callback::operator() returns true to stop
template<typename Callback>
inline bool enumerate(const Coord *ray,int degree,double delta, Callback &cb) {
    // find a dt that yields a segment smaller than delta at 0.5
	double dt = 0.5,D;
	Coord h = Bezier(ray,degree,0.5);
	int limit = 7; // up to 128 points
	while(--limit && (D = dist(h,Bezier(ray,degree,0.5+dt)))>delta)
		dt /= 2.0;
	for(double t = 0.0; t<=1.0; t+= dt)
		if(cb(Bezier(ray,degree,t)))
			return true;
	return false;
}
template<typename Callback>
inline bool enumerate(const Coord *ray,int n,int degree,double delta,Callback &cb) {
	for(int i=0;i<n-1; i+=degree)
		if(enumerate(ray+i,degree,delta,cb))
			return true;
	return false;
}
inline std::pair<Coord,double> close(const Coord *ray,int degree,Coord pt,double delta) { 
	double l = 0.0, r = 1.0;
	Coord ptl = Bezier(ray,degree,l),
		ptr = Bezier(ray,degree,r);
	do {
		if(dist(ptl,pt)<dist(ptr,pt)) {
			r = (l+r)/2.0;
			ptr = Bezier(ray,degree,r);
		}
		else {
			l = (l+r)/2.0;
			ptl = Bezier(ray,degree,l);
		}
	}
	while(dist(ptl,ptr)>delta);
	std::pair<Coord,double> ret;
	ret.first.x = (ptl.x+ptr.x)/2;
	ret.first.y = (ptl.y+ptr.y)/2;
	ret.second = (l+r)/2;
	return ret;
}
inline std::pair<Coord,double> close(Coord *ps,int n,int degree,Coord pt,double delta) {
	// find the closest segment endpoint
	int closest = 0;
	double closeh = dist(ps[0],pt);
	for(int i = degree;i<n;i+=degree) {
		double h2 = dist(ps[i],pt);
		if(h2<closeh) {
			closest = i;
			closeh = h2;
		}
	}
	// find the segment(s) to test
	int beg;
	bool doNext = false;
	if(closest==0) 
		beg = 0;
	else if(closest==n-1) 
		beg = n-degree-1;
	else {
		beg = closest-degree;
		doNext = true;
	}
	std::pair<Coord,double> res = close(ps+beg,degree,pt,delta);
	int x = beg/degree;
	if(doNext) {
		std::pair<Coord,double> res2 = close(ps+closest,degree,pt,delta);
		if(dist(res2.first,pt)<dist(res.first,pt)) {
			res = res2;
			x = closest/degree;
		}
	}
	segsizes sizes(ps,n,degree);
	double prev=x?sizes[x-1]:0;
	res.second = prev + res.second*(sizes[x]-prev);
	return res;
}
// in xlines.c
#define	GEMS_DONT_INTERSECT    0
#define	GEMS_DO_INTERSECT      1
#define GEMS_COLLINEAR         2
extern "C" {
int gems_lines_intersect(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4, double *x, double *y);
};
inline Position lines_intersect(Segment L0, Segment L1) {
	double		rx,ry;
	int			code;

	code = gems_lines_intersect(
		L0.a.x,L0.a.y,
		L0.b.x,L0.b.y,
		L1.a.x,L1.a.y,
		L1.b.x,L1.b.y,
		&rx, &ry);
		
	switch (code) {
		case GEMS_COLLINEAR:	/* weird case for us */
		case GEMS_DO_INTERSECT:	
			return Position(Coord(rx,ry));
		default:
		case GEMS_DONT_INTERSECT:	return Position();
	}
}

