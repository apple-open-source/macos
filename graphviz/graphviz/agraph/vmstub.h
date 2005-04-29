/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#ifndef _VMSTUB_H
#define _VMSTUB_H
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
typedef void Vmalloc_t;
#define vmalloc(heap,size) malloc(size)
#define vmopen(x,y,z) (Vmalloc_t*)(0)
#define vmclose(x) while (0)
#define vmresize(heap,ptr,size,oktomoveflag)  realloc((ptr),(size))
#define vmfree(heap,ptr) free(ptr)
#ifndef EXTERN
#define EXTERN extern
#endif
EXTERN void *Vmregion;
#endif
