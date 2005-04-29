/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef SHAPES_H
#define SHAPES_H

#include "common/Geometry.h"

/*
 *	shape generators
 */
struct PolyDef {
	bool isEllipse;
	double aspect; // for ellipses only (y/x)
	int sides, // for polys only
		peripheries;
	double perispacing,
		rotation,
		skew,distortion;
	bool regular;   
	Coord interior_box, // minimum inside size (e.g. for text)
		exterior_box; // minimum outside size
	Line input; // overrides sides,input
	PolyDef() : isEllipse(false), aspect(1),sides(4),peripheries(0),perispacing(0),
		rotation(0),skew(0),distortion(0),regular(false),
		interior_box(0,0),exterior_box(0,0) {
	}
};

extern void genpoly(const PolyDef &arg,Lines &out);
Coord polysize(const Line &poly);

// exceptions
struct BadPolyBounds : DGException {
  BadPolyBounds() : DGException("must specify internal or external box of poly; no one-dimensional or negative boxes") {}
};
struct BadPolyDef : DGException { 
  BadPolyDef() : DGException("polygon must have at least three sides") {}
};
struct BadInputPoly : DGException {
    BadInputPoly() : DGException("polydef input poly must have degree>0 and size>2") {}
};

//bezier_t *genellipse(polyreq_t *arg);

#endif
