/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#pragma warning (disable : 4786 4503)
#include <string>
#include <vector>
#include <map>
#include <stdarg.h>

struct Timer {
	double Start();
	double Elapsed(int rt,double start,char *s,...);
	double Now(int rt,char *s,...);

	void LoopPoint(int rt,char *s);
	Timer();
private:
	double currentTime();
	double display(int rt,double last,char *format,char *s,va_list va);
#ifdef _WIN32
	__int64 m_frequency,m_start;
#endif
	double m_last;
	std::vector<double> m_looplasts;
};
extern Timer timer;
