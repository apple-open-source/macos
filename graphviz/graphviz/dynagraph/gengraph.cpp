/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/LGraph-cdt.h"
#include "common/StrAttr.h"
#include "common/emitGraph.h"
#include <time.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

int main(int narg,char *argh[]) {
	int V = 100, E = 100;
	if(narg>1)
		V = atoi(argh[1]);
	if(narg>2)
		E = atoi(argh[2]);
	if(!V || !E) {
		fprintf(stderr,"gengraph #nodes #edges\n");
		return 1;
	}
	unsigned seed = (unsigned)time(NULL);
	srand(seed);

	StrGraph g;
	char buf[20];
	sprintf(buf,"%d",seed);
	gd<StrAttrs>(&g)["seed"] = buf;
	vector<StrGraph::Node*> ez;
	ez.resize(V);
	int v = V;
	while(v--) {
		char name[10];
		sprintf(name,"%d",v);
		ez[v] = g.create_node(name);
	}
	while(E--) {
		int t,h;
		do 
			t = rand()%V, h = rand()%V;
		while(t==h || g.find_edge(ez[t],ez[h]) || g.find_edge(ez[h],ez[t])); // play to dynagraph's weaknesses
		StrGraph::Edge *e = g.create_edge(ez[t],ez[h]).first;
		char *color=0;
		switch(rand()%3) {
			case 0: color = "red";
				break;
			case 1: color = "yellow";
				break;
			case 2: color = "blue";
				break;
		}
		gd<StrAttrs>(e)["color"] = color;
	}
	emitGraph(cout,&g);
	return 0;
}
