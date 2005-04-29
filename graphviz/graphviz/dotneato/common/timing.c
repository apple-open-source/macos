/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

#ifndef MSWIN32

#include        <unistd.h>
#include	<sys/types.h>
#include	<sys/times.h>
#include	<sys/param.h>
#ifndef HZ
#define HZ 60
#endif
typedef struct tms mytime_t;
#define GET_TIME(S) times(&(S))
#define DIFF_IN_SECS(S,T) ((S.tms_utime + S.tms_stime - T.tms_utime - T.tms_stime)/(double)HZ)

#else

#include	<time.h>
typedef clock_t mytime_t;
#define GET_TIME(S) S = clock()
#define DIFF_IN_SECS(S,T) ((S - T) / (double)CLOCKS_PER_SEC)

#endif
	

static mytime_t T;

void
start_timer(void)
{
	GET_TIME(T);
}

double
elapsed_sec(void)
{
	mytime_t	S;
	double		rv;

	GET_TIME(S);
	rv = DIFF_IN_SECS(S,T);
	return rv;
}
