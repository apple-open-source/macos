/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

//#include <io.h>
#include <fcntl.h>
// at one point i was able to nest two yacc parsers okay, but it stopped working,
// ag_yyparse seems to mess up the state of the input stream for gs_yyparse.
// so just make a brand new stream for each agread.
// (this is only successful sometimes)
struct bufferGraphStream {
	FILE *fin;
	static std::pair<bool,int> braceCount(const char *s) {
		bool action = false;
		int ct = 0;
		for(const char *ci = s; *ci; ++ci)
			if(*ci=='{')
				++ct,action=true;
			else if(*ci=='}')
				--ct,action=true;
		return std::make_pair(action,ct);
	}
	bufferGraphStream(FILE *f) : fin(0) {
		fin = tmpfile();
		bool action=false;
		int countBrace = 0;
		while(!action || countBrace) {
			char buf2[400];
			fgets(buf2,400,f);
			fputs(buf2,fin);
			std::pair<bool,int> bi = braceCount(buf2);
			action |= bi.first;
			countBrace += bi.second;
		}
		fseek(fin,0,SEEK_SET);
		setvbuf(fin,0,_IONBF,0);
	}
	~bufferGraphStream() {
		if(fin)
			fclose(fin);
	}
};
