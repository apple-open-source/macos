/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

struct ServerUnknown : DGException { 
  DString serverName; 
  ServerUnknown() : DGException("incremental engine name unknown") {}
};
// creates the servers specified in gd<StrAttrs>(client)["engines"]
Server *createLayoutServer(Layout *client,Layout *current);
