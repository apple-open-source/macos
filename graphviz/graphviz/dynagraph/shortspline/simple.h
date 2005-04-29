/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */
#pragma prototyped
#include <stdio.h>
#include "pathplan.h"

#define MAXINTS  10000	/* modify this line to reflect the max no. of 
	intersections you want reported -- 50000 seems to break the program */

#define SLOPE(p,q) ( ( ( p.y ) - ( q.y ) ) / ( ( p.x ) - ( q.x ) ) )

#define after(v) (((v)==((v)->poly->finish))?((v)->poly->start):((v)+1))
#define prior(v) (((v)==((v)->poly->start))?((v)->poly->finish):((v)-1))

struct position  { float x,y; };


struct vertex	{	
	struct position pos;
	struct polygon *poly;
	struct active_edge *active;
		};

struct polygon	{ struct vertex *start,*finish;};

struct intersection   { 
	struct vertex *firstv,*secondv; 
	struct polygon *firstp,*secondp; 
	float x,y; } ;

struct active_edge { struct vertex *name; struct active_edge *next,*last; }; 
struct active_edge_list { struct active_edge *first,*final ; int number ; } ;
struct data    { int nvertices, npolygons, ninters;} ;
