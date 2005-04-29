/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"

namespace DynaDAG {

Crossings uvcross(DDModel::Node *v, DDModel::Node *w, bool use_in, bool use_out) {
	Crossings ret;
	if(use_in) 
		for(DDModel::inedge_iter ei = w->ins().begin(); ei!=w->ins().end(); ++ei) 
			for(DDModel::inedge_iter ej = v->ins().begin(); ej!=v->ins().end(); ++ej) 
				if(DDd((*ej)->tail).order > DDd((*ei)->tail).order)
					ret.add(*ei,*ej);
	if (use_out) 
		for(DDModel::outedge_iter ei = w->outs().begin(); ei!=w->outs().end(); ++ei) 
			for(DDModel::outedge_iter ej = v->outs().begin(); ej!=v->outs().end(); ++ej) 
				if(DDd((*ej)->head).order > DDd((*ei)->head).order)
					ret.add(*ei,*ej);
	return ret;
}

} // namespace DynaDAG
