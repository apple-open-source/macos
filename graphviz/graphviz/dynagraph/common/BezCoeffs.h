/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

inline double B0(double t) {
    double tmp = 1.0 - t;
    return tmp * tmp * tmp;
}

inline double B1(double t) {
    double tmp = 1.0 - t;
    return 3 * t * tmp * tmp;
}

inline double B2(double t) {
    double tmp = 1.0 - t;
    return 3 * t * t * tmp;
}

inline double B3(double t) {
    return t * t * t;
}

inline double B01(double t) {
    double tmp = 1.0 - t;
    return tmp * tmp *(tmp + 3 * t);
}

inline double B23(double t) {
    double tmp = 1.0 - t;
    return t * t *(3 * tmp + t);
}
