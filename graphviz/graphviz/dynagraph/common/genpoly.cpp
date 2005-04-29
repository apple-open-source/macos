/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/genpoly.h"
#include "common/ellipse2bezier.h"

using namespace std;

//#include <limits>

#define	GEMS_DONT_INTERSECT    0
#define	GEMS_DO_INTERSECT      1
#define GEMS_COLLINEAR         2

#ifndef TRUE
#define TRUE 1
#define FALSE (!TRUE)
#endif 

#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))

#define NIL(t) (t)0

Coord origin;

Coord mysincos(double rads) {
  Coord		rv;
  rv.y = sin(rads);
  rv.x = sqrt(1.0 - rv.y * rv.y);
  if((rads > M_PI_2) && (rads < M_PI + M_PI_2)) rv.x = -rv.x;
  return rv;
}

/* flat side on bottom */
void regpolygon(double size,int nsides,Line &out) {
	
  double		t,theta;
  int			i;

  assert (nsides >= 3);
  out.clear();
  out.degree = 1;

  theta = (2.0 * M_PI) / nsides;
  t = (1.5 * M_PI) + (theta / 2.0);	/* starting place */
  for(i = 0; i < nsides; i++) {
    out.push_back(mysincos(t)*size);
    t = t + theta;
    if(t >=  2.0 * M_PI) t = t - 2.0 * M_PI;
  }
  out.push_back(out[0]);
}

static void rotate(Line &poly, double rot) {
  unsigned	i;
  double		r,theta,new_theta;
  Coord		p;

  for(i = 0; i < poly.size(); i++) {
    p = poly[i];
    r = sqrt(p.x * p.x + p.y * p.y);
    theta = atan2(p.y,p.x);
    new_theta = theta + rot;
    while(new_theta > 2.0 * M_PI) 
      new_theta -= 2.0 * M_PI;
    while(new_theta < 0.0)
      new_theta += 2.0*M_PI;
    p = mysincos(new_theta);
    p.x = p.x * r; p.y = p.y * r;
    poly[i] = p;
  }
}
/*
  static void skew_and_distort(polyline_t *poly, double skew, double distortion) {
  int			i;
  Coord		p;
  double		skewdist;

  skewdist = hypot(fabs(distortion)+fabs(skew),1.);

  for(i = 0; i < poly->n; i++) {
  p = poly->p[i];
  poly->p[i].x = p.x * (skewdist + p.y * distortion) + p.y * skew / 2.0;
  }
  poly->p[i] = poly->p[0];
  }
*/
/*
  distortion is measured in portion +/-: 1.0 means "top is 100% wider than
  bottom", -2.0 means "bottom is 200% wider than top"
  skew is measured in the portion of displacement of the top: 1.0 means "move
  top right by its height," -2.0 means "move top left by twice its height"
  (and the bottom always moves equal and opposite)
*/
static void skew_and_distort(Line &poly, double skew, double distortion) {
  unsigned	i;
  Coord		p;
  double		scale;

  // get the general size of poly:
  scale = hypot(poly[0].x,poly[0].y);

  /* below distortion's a straight proportion, so change -1.0 to 0.5, 
     -2.0 to 0.333, 1.0 to 2.0, etc: */
  if(distortion<0.0)
    distortion = 1.0/(1.0-distortion);
  else 
    distortion = 1.0 + distortion;

  for(i = 0; i < poly.size(); i++) {
    p = poly[i];

    /* idea of this is that if distortion==1, no distortion, otherwise
       squeezes so that the top of the poly has width dist * width(bottom) 

       and skew is such that if skew == 0, no skew, otherwise top is pushed
       right of bottom by height * skew.
    */
    poly[i].x = p.x * (1 + p.y * (distortion - 1) / (2 * scale)) + skew * p.y;
  }
}

// too sketchy
#ifdef QUADS
/*  not standard quadrants, just bit zero high if x>0, bit one high if y>0 */
#define quadrant(cd) ((((cd).x>0)?1:0)|(((cd).y>0)?2:0))
/*  mirror a coord into a quadrant: */
#define quadrantize(cd,q) (cd).x = (q&1)?fabs((cd).x):(-fabs((cd).x));\
						  (cd).y = (q&2)?fabs((cd).y):(-fabs((cd).y));
#endif

template<typename Reaction>
static void box_and_poly(Reaction &r,Line &poly,Coord box) {
  //  use diagonals: 
  Coord half(box.x / 2.0,box.y / 2.0);
  Segment D1(Coord(-half.x,-half.y),Coord(half.x,half.y)),
    D2(Coord(-half.x,half.y),Coord(half.x,-half.y));

  for(unsigned i = 0; i < poly.size()-1; i+=poly.degree) {
    Position P = poly.Intersection(i,D1);
    if(!P.valid)
      P = poly.Intersection(i,D2);
    if(!r.grok(box,P))
      break;
  }

}
struct FindScaling {
  double s;
  FindScaling() : s(0.0) {}
  inline bool grok(Coord box,Position P) {
    if(P.valid) 
      s = MAX(s,fabs(box.x/P.x/2.0));
    return true;
  }
};
/* 
 * returns the scale factor needed to fit box inside the polygon.
 * someday the "box" arg could be replaced with an arbitrary polygon.
 * Maybe polar coordinates would be more convenient here.
 */
static double scale_for_interior(Line &poly, Coord box) {
  FindScaling r;
  box_and_poly(r,poly,box);
  return r.s;
}
struct HitTest {
  bool hasHit;
  HitTest() : hasHit(false) {}
  inline bool grok(Coord box,Position P) {
    if(P.valid) {
      hasHit = true;
      return false;
    }
    return true;
  }
};
/*
static bool box_hits_poly(Line &poly,Coord box) {
  HitTest r;
  box_and_poly(r,poly,box);
  return r.hasHit;
}
*/
Coord polysize(const Line &poly) {
  unsigned i;
  Coord	low,high,p,rv;

  low = high = poly[0];
  for(i = 1; i < poly.size(); i++) {
    p = poly[i];
    if(low.x > p.x) low.x = p.x;
    if(low.y > p.y) low.y = p.y;
    if(high.x < p.x) high.x = p.x;
    if(high.y < p.y) high.y = p.y;
  }
  rv.x = high.x - low.x;
  rv.y = high.y - low.y;

  return rv;
}

static Coord scale_for_exterior(const Line &poly, Coord external_box) {
  Coord		size = polysize(poly),
    rv;

  rv.x = external_box.x / size.x;
  rv.y = external_box.y / size.y;
  return rv;
}

static void scalexy(Line &poly, Coord scale) {
  unsigned	i;

  for(i = 0; i < poly.size(); i++) {
    poly[i].x *= scale.x;
    poly[i].y *= scale.y;
  }
}
/*
static void jostle(Lines &polys, Coord size) {
  double minx,miny,maxx,maxy,ofsx,ofsy;
  unsigned i;
  // first poly is largest; adjust for that 
  Line &poly = polys.front();
  minx = maxx = poly[0].x;
  miny = maxy = poly[0].y;
  for(i=1;i<poly.size();i++) {
    if(poly[i].x<minx) 
      minx = poly[i].x;
    if(poly[i].y<miny) 
      miny = poly[i].y;
    if(poly[i].x>maxx)
      maxx = poly[i].x;
    if(poly[i].y>maxy)
      maxy = poly[i].y;
  }
  // if it's smaller than the box, it should center;
  //   if it's over either side, it should duck in
  ofsx = ((maxx-minx)-size.x)/2.0f + ((-size.x/2.0f)-minx);
  ofsy = ((maxy-miny)-size.y)/2.0f + ((-size.y/2.0f)-miny);
  for(Lines::iterator ip = polys.begin();ip!=polys.end();++ip) {
    Line &poly = *ip;
    for(Line::iterator ci = poly.begin(); ci!=poly.end(); ++ci) {
      ci->x += ofsx;
      ci->y += ofsy;
    }
  }
}
*/
// how to deal with infinity on all platforms?  lots&lots of special cases apparently.
const double Infinity = 99999.0; // ??????? cygwin has no <limits> ?????? numeric_limits<double>().Infinity();
inline bool leps(double a) {
  return fabs(a)<1e-8;
}
inline double sqr(double a) {
  return a*a;
}
inline double slope(Segment L) {
  Coord d = L.a - L.b;
  double m = d.y/d.x;
  if(fabs(d.x)<1e-3 || fabs(m)>1e3)
    m = Infinity;
  if(fabs(m)<1e-3)
    m = 0.0;
  return m;
}
// calc a point that is dist from pt along the line with this slope, either toward or away origin
static Coord distance_along_line(Coord pt,double slope,double dist,bool toward) {
  double	a,b,c,	/* coeffs of -b/2a +/- sqrt(sqr(b)-4ac)/2a */
    ofs,	/* for y = slope*x + ofs of this line */
    tmp,tmp2;
  Coord rv;

  if(dist==0.0)
    return pt;

  if(slope==Infinity || slope==-Infinity) {
    if(toward==(pt.y>0.0))
      return Coord(pt.x,pt.y-dist);
    else
      return Coord(pt.x,pt.y+dist);
  }
  /* ofs of perp line */
  ofs = pt.y - slope*pt.x;

  /* this ugly thing just solves distance(pt,rv)==dist, where rv is 
     constrained to fall on y = slope*x + ofs */
  a = 1.0 + sqr(slope);
  b = -2.0*pt.x + 2.0*ofs*slope - 2.0*pt.y*slope;
  c = sqr(pt.x) + sqr(ofs) - 2.0*pt.y*ofs + sqr(pt.y) - sqr(dist);

  // quadratic formula
  assert(a);
  tmp = -b/(2.0*a);
  assert(sqr(b)>4.0*a*c);
  tmp2 = sqrt(sqr(b)-4.0*a*c)/(2.0*a);

  // choose the solution that matches toward
  double x1 = tmp+tmp2,
    x2 = tmp-tmp2;
  if((hypot(x1,slope*x1 + ofs)<hypot(x2,slope*x2 + ofs)) == toward)
    rv.x = x1;
  else
    rv.x = x2;

  rv.y = slope*rv.x + ofs;

  return rv;
}
static void calc_periphery(const Line &poly,double dist,bool inward,Line &out) {
  Segment E; // current edge
  Coord midpt, // midpoint of edge
    lmid, // previous midpoint
    fmid; // first midpoint
  double m, // slope of curr. edge
    n, // slope of perp. segment
    b; // b in y = mx + b of this new edge
  double lm=-17.0,lb=-17.0, // previous m, b
    fm=-17.0,fb=-17.0; // first m, b (all inits to evade gcc3 warning)
  Coord q, // coord of pt on new seg.
    lq, // previous
    fq; // first
  out.clear();
  out.degree = poly.degree;
  out.push_back(Coord(0,0)); // fill in first point after rest

  for(unsigned i = 0; i < poly.size()-1; i++) {
    E.a = poly[i];
    E.b = poly[i+1];

    m = slope(E);
    //assert(m);
    if(fabs(m)<1e-3)
        n = m<0.0?Infinity:-Infinity;
    else
        n = -1.0/m;

    midpt = (E.a+E.b)/2.0;

    q = distance_along_line(midpt,n,dist,inward);

    b = q.y - m*q.x;
		
    if(i==0) { /* save for that last calculation */
      fm = m;
      fb = b;
      fmid = midpt;
      fq = q;
    }
    else {
      /* intersect y = mx + b, y = (lm)x + (lb) */
      Coord c;
      if(leps(m-lm)) 
	c = (lq + q)/2.0;
      else {
	if(m==Infinity||m==-Infinity) { // this calls out for a general line lib!
	  c.x = q.x;
	  c.y = lm*c.x+lb;
	}
	else if(lm==Infinity||lm==-Infinity) {
	  c.x = lq.x;
	  c.y = m*c.x+b;
	}
	else {
	  c.x = (lb - b) / (m - lm);
	  c.y = m*c.x+b;
	}
      }
      assert(c.y<1e5 && c.y>-1e5);
      out.push_back(c);
    }	

    lm = m;
    lb = b;
    lmid = midpt;
    lq = q;
  }
  /* intersect last seg with first to find first point */
  Coord c;
  if(leps(fm-lm)) 
    c = (fq+lq)/2.0;
  else {
    if(fm==Infinity||fm==-Infinity) { // this calls out for a general line lib!
      c.x = fq.x;
      c.y = lm*c.x+lb;
    }
    else if(lm==Infinity||lm==-Infinity) {
      c.x = lq.x;
      c.y = fm*c.x+fb;
    }
    else {
      c.x = (lb - fb) / (fm - lm);
      c.y = fm*c.x+fb;
    }
  }
  out[0] = c;
  out.push_back(c);
}
void makePeripheries(int peris,double perispace,bool inward,Lines &out) {
  if(peris) {
    // add each periphery based on last
    Line *last = &out.front();
    for(int i = 0; i < peris; i++) {
      Line *next;
      if(inward) {
	out.push_back(Line());
	next = &out.back();
      }
      else {
	out.push_front(Line());
	next = &out.front();
      }
      calc_periphery(*last,perispace,inward,*next);
      last = next;
    }
  }
}
void genpoly(const PolyDef &arg,Lines &out) {
  Coord scaling;
  int symmetric;

  // must specify at least one bound, no one-dimensional or negative bounds
  if(!((arg.interior_box.x && arg.interior_box.y) || 
       (arg.exterior_box.x && arg.exterior_box.y)) ||
     !arg.interior_box.x != !arg.interior_box.y || 
     !arg.exterior_box.x != !arg.exterior_box.y ||
     arg.interior_box.x<0.0 || arg.interior_box.y<0.0 || 
     arg.exterior_box.x<0.0 || arg.exterior_box.y<0.0)
    throw BadPolyBounds();

  // make prototype smaller than the interior box
  double startsize = arg.interior_box.x?min(arg.interior_box.x,arg.interior_box.y)/2.0:1.0;
  out.clear();
  out.push_back(Line());
  Line &first = out.back();
  if(arg.input.size()) {
      if(!arg.input.degree || arg.input.size()<3)
          throw BadInputPoly();
        first = arg.input*startsize;
  }
  else if(arg.isEllipse) {
    if(!arg.aspect)
      throw BadPolyDef();
    // upside-down so that it will generate CW, because pathplan wants CCW but thinks up is negative
    Rect r(-startsize,-arg.aspect*startsize,startsize,arg.aspect*startsize); 
    ellipse2bezier(r,first);
  }
  else {
    // 2-gon makes no sense
    if(arg.sides < 3) 
      throw BadPolyDef();
    /* create prototype unscaled polygon */
    regpolygon(startsize,arg.sides,first);
  }
  if(arg.skew != 0.0 || arg.distortion != 0.0) {
    symmetric = FALSE;
    skew_and_distort(first,arg.skew,arg.distortion);
  }
  else symmetric = TRUE;
  if(arg.rotation != 0.0)
    rotate(first,arg.rotation);

  // scale according to size requests...
  bool inward; // which way to add peripheries
  if(arg.interior_box.x || arg.interior_box.y) {
    inward = false;
    if(arg.regular) {
      double s = scale_for_interior(first,arg.interior_box);
      scaling = Coord(s,s);
      scaling.x = scaling.y = MAX(scaling.x,scaling.y); // (actually already ==)
    }
    else {
      /*
      // get the aspect ratio good first
      Coord guess = arg.interior_box;
      guess.x += arg.peripheries*arg.perispacing;
      guess.y += arg.peripheries*arg.perispacing;
      if(guess.x<arg.exterior_box.x)
      guess.x = arg.exterior_box.x;
      if(guess.y<arg.exterior_box.y)
      guess.y = arg.exterior_box.y;
      guess.y /= guess.x;
      guess.x = 1.0;
      scalexy(first,guess);
      scaling = scale_for_interior(first,arg.interior_box,symmetric);
      */
      /*
      // yuck: scale for box and then try to shrink in y
      double s = scale_for_interior(first,arg.interior_box);
      scaling = Coord(s,s);
      double factor = 0.5;
      Coord test(arg.interior_box.x/scaling.x,arg.interior_box.y/scaling.y);
      for(double i = 1./4.; i>1./64.; i/=2.) {
      test.y = factor*arg.interior_box.y/scaling.y;
      if(box_hits_poly(first,scaling))
      factor += i;
      else
      factor -= i;
      }
      scaling.y *= factor;
      */
      double m = MIN(arg.interior_box.x,arg.interior_box.y);
      double s = scale_for_interior(first,Coord(m,m));
      if(arg.interior_box.x>arg.interior_box.y) {
	scaling.y = s;
	scaling.x = s*arg.interior_box.x/arg.interior_box.y;
      }
      else {
	scaling.x = s;
	scaling.y = s*arg.interior_box.y/arg.interior_box.x;
      }

    }
  }
  else {
    scaling = scale_for_exterior(first,arg.exterior_box);
    inward = true;
    if(arg.regular)
      scaling.x = scaling.y = MIN(scaling.x,scaling.y);
  }

  // first shape will be model for rest
  scalexy(first,scaling); 

  // peripheries: 
  makePeripheries(arg.peripheries,arg.perispacing,inward,out);

  // now see if biggest is still smaller than arg.exterior_box 
  if(!inward && (arg.exterior_box.x || arg.exterior_box.y)) {
    Coord size = polysize(out.front());
    bool tooSmall = false;
    if(size.x-arg.exterior_box.x<-1e-5) {
      tooSmall = true;
      size.x = arg.exterior_box.x;
    }
    if(size.y-arg.exterior_box.y<-1e-5) {
      tooSmall = true;
      size.y = arg.exterior_box.y;
    }
    if(tooSmall) {
      Coord scaleExt = scale_for_exterior(out.front(),size);
      scalexy(out.front(),scaleExt);

      // redraw peripheries
      out.erase(++out.begin(),out.end());
      makePeripheries(arg.peripheries,arg.perispacing,true,out);
    }
  }
  /* move into position: 
     if(fits)
     jostle(*rv,arg.exterior_box);*/
}
