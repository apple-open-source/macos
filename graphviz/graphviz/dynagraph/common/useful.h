/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef useful_h
#define useful_h
#include "common/time-o-matic.h"
#include <stdio.h>

template<typename T>
inline bool sequence(T a,T b,T c) {
	return a <= b && b <= c;
}
template<typename T>
inline T absol(T n) {
	return n<T(0) ? -n : n;
}
/*  don't use "round(..)" as it collides with a system definition */
inline int ROUND(double f) {
	return f>=0.0?int(f + .5):int(f - .5);
}
// silly little
template<typename T>
inline bool assign(T &a,const T &b) {
	if(a!=b)
		return a = b,true;
	return false;
}
// debugging things.  even for internal errors, exceptions are more useful
// than things based on abort()
// assert compiles out in release builds, whereas check doesn't
struct Assertion : DGException {
	char *file;
	int line;
	Assertion(char *file,int line) : DGException("assertion failure") {}
};
#undef assert
#ifndef NDEBUG
inline void assert(bool val) { if(!val) throw Assertion(__FILE__,__LINE__); }
inline void check(bool val) { if(!val) throw Assertion(__FILE__,__LINE__); }
#else
#define assert(X)
#define check(X) (X)
#endif

#pragma warning (disable : 4800)

// cross-platform debug report mechanism
void report(int rt,char *msg,...);
void vreport(int rt, char *msg,va_list va);
enum reportTypes {r_dynadag,r_cmdline,r_crossopt,r_wander,r_stats,r_error,
	r_splineRoute,r_shortestPath,r_grChange,r_timing,r_exchange,r_nsdump,
	r_ranks,r_xsolver,r_modelDump,r_ranker,r_dumpQueue,r_stability,r_readability,r_progress,
	r_bug
};
void enableReport(int rt,FILE *f = stdout);
bool reportEnabled(int rt);
void shush(bool whether);

// writes fields to a file.  deals with combining reports that are going to the same file
struct LoopMinder {
	char sep;
	void Start(int rt);
	void Field(int rt,char *colname,double val);
	void Finish(int rt);
	void Cancel();
private:
	struct FieldSet { // all the data that's going into one file
		enum {start,first,loop,done} state;
		std::vector<std::string> names;
		std::vector<double> data;
		FieldSet() : state(start) {}
	};
	typedef std::map<FILE*,FieldSet> FieldSets;
	FieldSets m_fieldSets;
	void doField(FieldSet &f,char *colname,double val);
};
extern LoopMinder loops;

#endif
