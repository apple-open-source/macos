/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/dgxep.h"
struct ParseError : DGException {
	ParseError() : DGException("graphsearch parse error") {}
};
struct GSError : DGException {
	GSError() : DGException("graphsearch error") {}
};
// not yet implemented
struct NYI : DGException {
	NYI() : DGException("graphsearch ins/mod/del commands not yet implemented") {}
};
