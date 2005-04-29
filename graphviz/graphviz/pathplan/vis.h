/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#ifndef VISIBILITY_H
#define VISIBILITY_H

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <vispath.h>
#include <pathutil.h>

typedef COORD** array2;
typedef	unsigned char boolean;

#define	OBSCURED	0.0
#define EQ(p,q)		((p.x == q.x) && (p.y == q.y))
#define NEQ(p,q)	(!EQ(p,q))
#define NIL(p)		((p)0)
#define	CW			0
#define	CCW			1

struct vconfig_s {
	int			Npoly;
	int			N;		/* number of points in walk of barriers */
	Ppoint_t	*P;		/* barrier points */
	int			*start;
	int			*next;	
	int			*prev;

	/* this is computed from the above */
	array2		vis;
} ;

extern COORD* ptVis (vconfig_t *, int, Ppoint_t);
extern int directVis (Ppoint_t, int, Ppoint_t, int, vconfig_t *);
extern void visibility (vconfig_t *);
extern int* makePath (Ppoint_t p, int pp, COORD* pvis,
               Ppoint_t q, int qp, COORD* qvis,
               vconfig_t* conf);

#endif
