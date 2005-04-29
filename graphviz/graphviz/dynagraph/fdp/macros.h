/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#define BETWEEN(a,b,c)	(((a) <= (b)) && ((b) <= (c)))
#define RADIANS(deg)	((deg)/180.0 * PI)
#define DEGREES(rad)	((rad)/PI * 180.0)
#define DIST(x1,y1,x2,y2) (sqrt(((x1) - (x2))*((x1) - (x2)) + ((y1) - (y2))*((y1) - (y2))))

#ifndef streq
#define streq(s,t)		(!strcmp((s),(t)))
#endif
#ifndef NOTUSED
#define NOTUSED(var)      (void) var
#endif

#ifndef NIL
#define NIL(T) reinterpret_cast<T>(0)
#endif
