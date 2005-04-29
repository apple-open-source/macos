/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef Geometry_h
#define Geometry_h

#pragma warning (disable : 4786 4503)
#include <math.h>
#include "common/moremath.h"
#include <vector>
#include <list>
#include <algorithm>
#include <float.h>
#include "common/StringDict.h"
#include "common/useful.h"

struct Coord {
	double x,y;
    Coord() : x(-17),y(-17) {}
	Coord(double x, double y) : x(x),y(y){}
	bool operator ==(const Coord &c) const {
		return double(x)==double(c.x) && double(y)==double(c.y);
	}
	bool operator !=(const Coord &c) const {
		return !(*this==c);
	} 
	Coord operator +(const Coord &c) const {
		return Coord(x+c.x,y+c.y);
	}
	Coord operator -(const Coord &c) const {
		return Coord(x-c.x,y-c.y);
	}
	Coord operator -() const {
		return Coord(-x,-y);
	}
	Coord operator *(double a) const {
		return Coord(a*x,a*y);
	}
	Coord operator /(double a) const {
		return Coord(x/a,y/a);
	}
	Coord &operator +=(const Coord &c) {
		return *this = *this + c;
	}
	Coord &operator -=(const Coord &c) {
		return *this = *this - c;
	}
	Coord &operator *=(double a) {
		return *this = *this*a;
	}
	Coord &operator /=(double a) {
		return *this = *this/a;
	}
	// dot product
	double operator *(Coord a) const {
		return x*a.x+y*a.y;
	}
	double Len() const {
		return sqrt(*this * *this);
	}
	Coord Norm() const {
		double d = Len();
		if(d)
			return *this / d;
		else
			return *this;
	}
	Coord Abs() const {
		return Coord(fabs(x),fabs(y));
	}
};
inline double dSquared(Coord a,Coord b) {
	Coord c = a-b;
	return c*c;
}
inline double dist(Coord a,Coord b) {
	return sqrt(dSquared(a,b));
}
struct Position : Coord {
	bool valid;
	Position() : valid(false) {}
	Position(double x, double y) : Coord(x,y),valid(true) {}
	Position(Coord c) : Coord(c),valid(true) {}
	Position &operator =(const Coord &c) {
		static_cast<Coord&>(*this) = c;
		valid = true;
		return *this;
	}
	void invalidate() {
		valid = false;
		x = y = -17;
	}
	bool operator ==(const Position &p) const {
		return valid==p.valid && ((Coord)p==*this);
	}
	bool operator !=(const Position &p) const {
		return !(*this==p);
	}
};
struct Segment {
	Coord a,b;
	Segment(Coord a = Coord(),Coord b = Coord()) : a(a), b(b) {}
};
struct Rect {
	double l,t,r,b;
	Rect() {}
	Rect(const Rect &r) : l(r.l),t(r.t),r(r.r),b(r.b) {}
	Rect(double l,double t,double r,double b) : l(l),t(t),r(r),b(b) {}
	// note this allows implicit conversion from coord to rect
	Rect(Coord c) : l(c.x),t(c.y),r(c.x),b(c.y) {}
	bool operator==(const Rect &r) const {
		return l==r.l && t==r.t && this->r==r.r && b==r.b;
	}
	bool operator!=(const Rect &r) const {
		return !(*this==r);
	}
	double Width() const {
		return r-l;
	}
	double Height() const {
		return fabs(t-b);
	}
	Coord Size() const {
		return Coord(Width(),Height());
	}
	bool Contains(Coord c) const {
		return l<=c.x && c.x<=r &&
			(b<t)?(b<=c.y && c.y<=t):(t<=c.y && c.y<=b);
	}
	Coord Center() const {
		return Coord((l+r)/2.0,(t+b)/2.0);
	}
	Coord SW() const {
		return Coord(l,b);
	}
	Coord NE() const {
		return Coord(r,t);
	}
	Coord SE() const {
		return Coord(r,b);
	}
	Coord NW() const {
		return Coord(l,t);
	}
	Rect operator+(Coord c) const {
		return Rect(l+c.x,t+c.y,r+c.x,b+c.y);
	}
	Rect operator-(Coord c) const {
		return Rect(l-c.x,t-c.y,r-c.x,b-c.y);
	}
	Rect operator|(Rect o) const { // union
		return (b<=t)?Rect(std::min(l,o.l),std::max(t,o.t),std::max(r,o.r),std::min(b,o.b)):
			Rect(std::min(l,o.l),std::min(t,o.t),std::max(r,o.r),std::max(b,o.b));
	}
	Rect operator|=(Rect o) {
		return *this = *this|o;
	}
	bool operator &&(Rect o) const { // intersect test
		return ((b<=t)?(b<o.t && t>o.b):(b>o.t && t<o.b))
			&& l<o.r && r>o.l;
	}
};

struct Bounds : Rect {
	bool valid;
	Bounds() : valid(false) {}
	Bounds(Rect r) : Rect(r),valid(true) {}
	bool operator ==(const Bounds &b) const {
		if(valid!=b.valid)
			return false;
		return !valid || Rect::operator ==(b);
	}
	bool operator !=(const Bounds &b) const {
		return !(*this==b);
	}
	Bounds &operator |=(const Rect &r) {
		if(!valid)
			*this = r;
		else 
			static_cast<Rect&>(*this) |= r;
		return *this;
	}
	Bounds &operator |=(const Bounds &b) {
		if(b.valid)
			*this |= static_cast<const Rect&>(b);
		return *this;
	}
};
struct Region;
struct Line : std::vector<Coord> {
	typedef std::vector<Coord>::iterator iterator; // for gcc 3.0; why?  
    Coord &at(int i) { return operator[](i); } // for earlier gccs
	Coord at(int i) const { return operator[](i); }
	int degree;
	Line() : degree(0) {}
	Line(size_t N) : std::vector<Coord>(N),degree(0) {} // ick
	void Clear() {
		clear();
		degree = 0;
	}
	bool Empty() {
		return degree==0 || !size();
	}
	int GetSeg(double y);
	template<typename It> // T* and std::vector<T>::iterator are finally different types in gcc 3.0!
	void AddSeg(It ci) {
		It b = ci,
			e = ci+degree+1;
		if(size()) {
			assert(back()==*b);
			b++;
		}
		insert(end(),b,e);
	}
	Position YIntersection(double y);
	Position Intersection(int seg,Segment other);
	Bounds BoundingBox() {
		Bounds ret;
		for(iterator ci = begin(); ci!=end(); ++ci)
			ret |= *ci;
		return ret;
	}
	Line &operator +=(const Line &append) {
		assert(degree==append.degree);
		Coord b = back(),f = append.front();
		assert(b==f);
		insert(end(),append.begin()+1,append.end());
		return *this;
	}
	Line operator +(Coord c) const {
		Line ret;
        ret.degree = degree;
		for(const_iterator pi = begin(); pi!=end(); ++pi)
			ret.push_back(*pi+c);
		return ret;
	}
	Line operator -(Coord c) const {
		return *this + -c;
	}
	Line operator *(double a) const {
		Line ret(size());
        ret.degree = degree;
		iterator out = ret.begin();
		for(const_iterator pi = begin(); pi!=end(); ++pi)
			*out++ = *pi*a;
		return ret;
	}
	Line operator /(double a) const {
		return *this * (1.0/a);
	}
	bool operator &&(const Line &l2) const;
	void ClipEndpoints(Line &source,const Coord offsetTail,const Region *tl,
		const Coord offsetHead,const Region *hd); // NG*'s NULL to not clip
private:
	Coord *clip(Coord *in,const Coord offset,const Region &reg,Coord *out);
};
typedef std::list<Line> Lines;

struct Region {
	Bounds boundary; 
	Line shape;
	mutable int lastOut; // to make iterative calls quicker
	Region() : lastOut(0) {}
	void updateBounds(); // from new Line data
	bool Hit(Coord c) const;
	bool Overlaps(Coord ofs1,Coord ofs2,const Region &r);
private:
	bool sameSide(const Coord &p0, const Coord &p1, int seg) const;
};
// check that e.g. YIntersection exists
inline Coord checkPos(Position p) { check(p.valid); return p; }

#include "GeomUtil.h"
#include "LineTricks.h"
#include "StreamGeom.h"

#endif // Geometry_h
