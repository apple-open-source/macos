/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

void InvalidateMVal(DDModel::Node *n, UpDown dir);
bool MValExists(DDModel::Node *n, UpDown dir);
double MVal(DDModel::Node *n, UpDown dir);
void SetMVal(DDModel::Node *n, double val, UpDown dir);
void InvalidateAdjMVals(DDModel::Node *n);
