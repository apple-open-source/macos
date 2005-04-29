/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <math.h>
#include "common/Geometry.h"
#include "common/Solvers.h"

inline bool near0(double x) {
	return absol(x)<1E-7;
}
// fwd
int solve2(double coeff[3], double roots[2]);
int solve1(double coeff[2], double *root);

int solve3(double coeff[4], double roots[3]) {
    double a, b, c, d;
    int rootn, i;
    double p, q, disc, b_over_3a, c_over_a, d_over_a;
    double r, theta, temp, alpha, beta;

    a = coeff[3], b = coeff[2], c = coeff[1], d = coeff[0];
    if(near0(a))
        return solve2(coeff, roots);
    b_over_3a = b /(3 * a);
    c_over_a = c / a;
    d_over_a = d / a;

    p = b_over_3a * b_over_3a;
    q = 2 * b_over_3a * p - b_over_3a * c_over_a + d_over_a;
    p = c_over_a / 3 - p;
    disc = q * q + 4 * p * p * p;

    if(disc < 0) {
        r = .5 * sqrt(-disc + q * q);
        theta = atan2(sqrt(-disc), -q);
        temp = 2 * cbrt(r);
        roots[0] = temp * cos(theta / 3);
        roots[1] = temp * cos((theta + M_PI + M_PI) / 3);
        roots[2] = temp * cos((theta - M_PI - M_PI) / 3);
        rootn = 3;
    } else {
        alpha = .5 * (sqrt(disc) - q);
        beta = -q - alpha;
        roots[0] = cbrt(alpha) + cbrt(beta);
        if(disc > 0)
            rootn = 1;
        else
            roots[1] = roots[2] = -.5 * roots[0], rootn = 3;
    }

    for(i = 0; i < rootn; i++)
        roots[i] -= b_over_3a;

    return rootn;
}

int solve2(double coeff[3], double roots[2]) {
    double a, b, c;
    double disc, b_over_2a, c_over_a;

    a = coeff[2], b = coeff[1], c = coeff[0];
    if(near0(a))
        return solve1(coeff, roots);
    b_over_2a = b / (2 * a);
    c_over_a = c / a;

    disc = b_over_2a * b_over_2a - c_over_a;
    if(disc < 0)
        return 0;
    else if(disc == 0) {
        roots[0] = -b_over_2a;
        return 1;
    } else {
        roots[0] = -b_over_2a + sqrt(disc);
        roots[1] = -2 * b_over_2a - roots[0];
        return 2;
    }
}

int solve1(double coeff[2], double *root) {
    double a, b;

    a = coeff[1], b = coeff[0];
    if(near0(a))
        if(near0(b))
            return 4;
        else
            return 0;
    *root = -b / a;
    return 1;
}
