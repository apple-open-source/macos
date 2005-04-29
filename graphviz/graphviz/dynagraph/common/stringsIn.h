/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef stringsIn_h
#define stringsIn_h

#include "common/Transform.h"
Update stringsIn(Transform *trans,Layout *l,const StrAttrs &attrs,bool clearOld);
Update stringsIn(Transform *trans,Layout::Node *n,const StrAttrs &attrs,bool clearOld);
Update stringsIn(Transform *trans,Layout::Edge *e,const StrAttrs &attrs,bool clearOld);

void applyStrGraph(Transform *trans,StrGraph *g,Layout *out, Layout *subg);

struct NonSizeableShape {};
struct UnknownShape {};

#endif // stringsIn_h
