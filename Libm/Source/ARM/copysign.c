/*
 *  copysignf.c
 *  cLibm
 *
 *  Created by Ian Ollmann on 6/13/07.
 *  Copyright 2007 Apple Inc. All rights reserved.
 *
 */

#include <math.h>
#include <stdint.h>

double copysign( double x, double y )
{
	union{ double f; uint64_t u; }ux, uy;
	
	ux.f = x;
	uy.f = y;
	
	ux.u &= 0x7fffffffffffffffULL;
	ux.u |= uy.u & 0x8000000000000000ULL;

	return ux.f;
}
