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

#ifndef _EXEC_H
#define _EXEC_H
typedef struct Tonm_t lvar_t;

extern Tobj root, null;
extern Tobj rtno;
extern int Erun;
extern int Eerrlevel, Estackdepth, Eshowbody, Eshowcalls, Eoktorun;

void Einit (void);
void Eterm (void);
Tobj Eunit (Tobj);
Tobj Efunction (Tobj, char *);
#endif /* _EXEC_H */
