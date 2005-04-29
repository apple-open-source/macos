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

#ifndef _INTERNAL_H
#define _INTERNAL_H
typedef struct Ifunc_t {
    char *name;
    int (*func) (int, Tonm_t *);
    int min, max;
} Ifunc_t;

void Iinit (void);
void Iterm (void);
int Igetfunc (char *);

extern Ifunc_t Ifuncs[];
extern int Ifuncn;
#endif /* _INTERNAL_H */
