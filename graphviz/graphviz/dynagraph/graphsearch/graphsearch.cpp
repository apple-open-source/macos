/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <stdio.h>
#include "graphsearch/Search.h"
#include "common/ag2str.h"

using namespace GSearch;
StrGraph *g_sourceGraph;

extern FILE *gs_yyin;
extern int gs_yyparse(); // in gsgram.c
int main(int argc, char *args[]) {
	setvbuf(stderr,0,_IONBF,0);
	setvbuf(stdout,0,_IONBF,0);
	setvbuf(stdin,0,_IONBF,0);
	if(argc<2) {
		fprintf(stderr,"please specify a graph to search!\n");
		return -1;
	}
	FILE *fSource = fopen(args[1],"r");
	if(!fSource) {
		fprintf(stderr,"file %s not found!\n",args[1]);
		return -1;
	}
	if(!(g_sourceGraph = readStrGraph(fSource))) {
		fprintf(stderr,"error parsing %s\n",args[1]);
		return -1;
	}
	if(argc>2) {
		gs_yyin = fopen(args[2],"r");
		if(!gs_yyin) {
			fprintf(stderr,"couldn't open script %s\n",args[2]);
			return -1;
		}
	}
	else
		gs_yyin = stdin;
	try {
		while(!feof(gs_yyin)) // ?
			gs_yyparse();
	}
	catch(...) {
		fprintf(stderr,"unhandled exception: closing...\n");
	}
	return 0;
}
// a one-time test
/*
	FILE *fPattern = 0,*fSearch = stdin,*fSource = stdin,*fStart = stdin;
	if(argc<2) {
		fprintf(stderr,"graphsearch pattern-file [search-file] [source-file] [startset-file]\n");
		return -1;
	}
	fPattern = fopen(args[1],"r");
	if(argc>2)
		fSearch = fopen(args[2],"r");
	if(argc>3)
		fSource = fopen(args[3],"r");
	if(argc>4)
		fStart = fopen(args[4],"r");
	Patterns patterns;
	while(StrGraph *sPattern = readStrGraph(fPattern)) {
		patterns[gd<Name>(sPattern)].readStrGraph(*sPattern);
		delete sPattern;
	}
	StrGraph *sSearch = readStrGraph(fSearch),
		*source = readStrGraph(fSource);
	StrGraphsByName inputs;
	while(StrGraph *start0 = readStrGraph(fStart)) {
		StrGraph *start = new StrGraph(source);
		start->readSubgraph(start0);
		inputs.push_back(make_pair(gd<Name>(start0),start));
	}
	Search search(*source);
	search.readStrGraph(patterns,*sSearch);
	Search::Node *searchFinish = search.dict["finish"];
	if(!searchFinish) {
		fprintf(stderr,"search must have a stage named \"finish\"\n");
		return -1;
	}
	search.Run(inputs);
	emitGraph(stdout,&gd<SearchStage>(searchFinish).result);
*/
// pattern test only
/*
	Pattern::Node *startstate = pattern.dict["start"];
	if(!start) {
		fprintf(stderr,"pattern must have a state named \"start\"\n");
		return;
	}
	queue<Match> Q;
	hash_set<FollowedPath> followed;
	StrGraph found(source);
	for(StrGraph::node_iter ni = start->nodes().begin(); ni !=start->nodes().end(); ++ni) {
		StrGraph::Node *nn = source->dict[gd<Name>(*ni)];
		if(!nn) {
			fprintf(stderr,"start set node \"%s\" not found\n",gd<Name>(*ni));
			return;
		}
		found.insert(nn);
		Q.push(Match(startstate,nn));
	}
	runPattern(Q,followed,&found);
	emitGraph(stdout,&found);

*/
