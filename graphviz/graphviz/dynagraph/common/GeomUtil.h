/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#define ISCCW 1
#define ISCW  2
#define ISON  3

/* ccw test: CCW, CW, or co-linear */
inline int ccw(Coord p1, Coord p2, Coord p3) {
    double d = (p1.y - p2.y)*(p3.x - p2.x) -
           (p3.y - p2.y)*(p1.x - p2.x);
    return (d > 1e-8) ? ISCCW :((d < -1e-8) ? ISCW : ISON);
}

/* is b between a and c */
inline bool between(Coord a, Coord b, Coord c) {
    if(ccw(a, b, c) != ISON)
        return false;
    Coord p1 = b-a, 
		p2 = c-a;
    return p2*p1 >= 0 && p2*p2 <= p1*p1;
}
// this one appears to work better:
inline bool between2(Coord a, Coord b, Coord c) {
    if(ccw(a, b, c) != ISON)
        return false;
	return (a.x<=b.x && b.x <= c.x || c.x<=b.x && b.x<=a.x) &&
		(a.y<=b.y && b.y<=c.y || c.y<=b.y && b.y<=a.y);
}

/* line to line intersection */
inline bool segsIntersect(Coord a, Coord b, Coord c, Coord d) {
    if(ccw(a, b, c) == ISON || ccw(a, b, d) == ISON ||
            ccw(c, d, a) == ISON || ccw(c, d, b) == ISON) {
        if(between(a, b, c) || between(a, b, d) ||
                between(c, d, a) || between(c, d, b))
            return true;
    } else {
        bool ccw1 = ccw(a, b, c) == ISCCW,
			ccw2 = ccw(a, b, d) == ISCCW,
			ccw3 = ccw(c, d, a) == ISCCW,
			ccw4 = ccw(c, d, b) == ISCCW;
        return(ccw1 ^ ccw2) && (ccw3 ^ ccw4);
    }
    return false;
}
inline double tri_area_2(Coord a, Coord b, Coord c ) {
    return  a.x * b.y - a.y * b.x +
            a.y * c.x - a.x * c.y +
            b.x * c.y - c.x * b.y;
}
inline double tri_area(Coord a, Coord b, Coord c) {
	return fabs(tri_area_2(a,b,c))/2.0;
}
 /* centroidOf:
  * Compute centroid of triangle with vertices a, b, c.
  * Return coordinates in x and y.
  */
inline Coord centroid(Coord a,Coord b,Coord c) {
    return Coord((a.x + b.x + c.x)/3,(a.y + b.y + c.y)/3);
}
inline bool leftOf(Coord a, Coord b, Coord c) {
	return tri_area_2( a, b, c ) > 0;
}

inline Position intersection( Coord a, Coord b, Coord c, Coord d) {
    double  s, t;   /* The two parameters of the parametric eqns. */
    double  denom;  /* Denominator of solutions. */

    denom =
      a.x * ( d.y - c.y ) +
      b.x * ( c.y - d.y ) +
      d.x * ( b.y - a.y ) +
      c.x * ( a.y - b.y );

      /* If denom is zero, then the line segments are parallel. */
      /* In this case, return false even though the segments might overlap. */
    if (denom == 0.0) return Position();

    s = ( a.x * ( d.y - c.y ) +
          c.x * ( a.y - d.y ) +
          d.x * ( c.y - a.y )
        ) / denom;
    t = -( a.x * ( c.y - b.y ) +
           b.x * ( a.y - c.y ) +
           c.x * ( b.y - a.y )
         ) / denom;

	Position ret;
    ret.x = a.x + s * ( b.x - a.x );
    ret.y = a.y + s * ( b.y - a.y );

    if ((0.0 <= s) && (s <= 1.0) &&
        (0.0 <= t) && (t <= 1.0))
                ret.valid = true;
    else ret.valid = false;
	return ret;
}
