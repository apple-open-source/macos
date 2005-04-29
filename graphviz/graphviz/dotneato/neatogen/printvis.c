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
#include <stdio.h>
#include <vis.h>
typedef Ppoint_t point;


void printvis (vconfig_t *cp)
{
    int i, j;
    int *next, *prev;
    point *pts;
    array2 arr;

    next = cp->next;
    prev = cp->prev;
    pts = cp->P;
    arr = cp->vis;

    printf ("this next prev point\n");
    for (i = 0; i < cp->N; i++)
      printf ("%3d  %3d  %3d    (%f,%f)\n", i, next[i],prev[i],
        (double)pts[i].x, (double)pts[i].y);

    printf ("\n\n");

    for (i = 0; i < cp->N; i++) {
      for (j = 0; j < cp->N; j++)
        printf ("%4.1f ", arr[i][j]);
      printf ("\n");
    }
}
