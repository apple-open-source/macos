/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/emitGraph.h"

void emitAttrs(std::ostream &os,const StrAttrs &attrs,const DString &id) {
	bool comma=false;
	os << "[";
	if(!id.empty()) {
		os << "id=" << id;
		comma = true;
	}
	for(StrAttrs::const_iterator ai = attrs.begin(); ai!=attrs.end(); ++ai) {
		if(comma)
			os << ", ";
		os << mquote(ai->first) << "=" << mquote(ai->second);
		comma = true;
	}
	os << "]" << std::endl;
}
