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

#ifndef MEMORY_H
#define MEMORY_H

#ifndef NULL
#define NULL 0
#endif

  /* Support for freelists */

typedef struct freelist{
    struct freenode    *head;          /* List of free nodes */
    struct freeblock   *blocklist;     /* List of malloced blocks */
    int                nodesize;       /* Size of node */
} Freelist;

extern void *getfree(Freelist *);
extern void freeinit(Freelist *, int);
extern void makefree(void *,Freelist *);

#endif

