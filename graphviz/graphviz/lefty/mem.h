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
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#ifndef _MEM_H
#define _MEM_H

#ifdef FEATURE_MINTSIZE
typedef int Msize_t;
#define M_SIZEMAX INT_MAX
#else
typedef short Msize_t;
#define M_SIZEMAX SHRT_MAX
#endif
typedef struct Mheader_t {
    char type;
    char area;
    Msize_t size;
} Mheader_t;
#define M_HEADERSIZE sizeof (Mheader_t)

typedef enum {
    M_GCOFF, M_GCON
} Mgcstate_t;

typedef enum {
    M_GCFULL, M_GCINCR
} Mgctype_t;

#define M_UNITSIZE sizeof (long)
#define M_BYTE2SIZE(l) ((long) (((l + M_UNITSIZE - 1) / M_UNITSIZE)))
#define M_AREAOF(p) ((int) (((Mheader_t *) p)->area))
#define M_TYPEOF(p) ((int) (((Mheader_t *) p)->type))

extern int Mhaspointers[];
extern Mgcstate_t Mgcstate;
extern int Mcouldgc;

void Minit (void (*) (void));
void Mterm (void);
void *Mnew (long, int);
void *Mallocate (long);
void Mfree (void *, long);
void *Marrayalloc (long);
void *Marraygrow (void *, long);
void Marrayfree (void *);
long Mpushmark (void *);
void Mpopmark (long);
void Mresetmark (long, void *);
void Mmkcurr (void *);
void Mdogc (Mgctype_t);
void Mreport (void);
#endif /* _MEM_H */
