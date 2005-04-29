/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

// a silly translation deal until i make a native agread

#include "common/LGraph-cdt.h"
#include "common/StrAttr.h"

extern "C" {
	#include "agraph.h"
};

StrGraph *ag2str(Agraph_t *g);
Agraph_t *str2ag(StrGraph *gg);

// struct agreadError {};
inline StrGraph *readStrGraph(FILE *f) {
	Agraph_t *g = agread(f,0);
	if(!g)
		return 0;//throw agreadError();
	return ag2str(g);
}
