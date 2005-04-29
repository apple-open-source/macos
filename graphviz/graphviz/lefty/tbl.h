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

#ifndef _TBL_H
#define _TBL_H
typedef enum {
    T_INTEGER = 1, T_REAL, T_STRING, T_CODE, T_TABLE, T_SIZE
} Ttype_t;

typedef struct Tinteger_t {
    Mheader_t head;
    long i;
} Tinteger_t;
#define T_INTEGERSIZE sizeof (Tinteger_t)
typedef struct Treal_t {
    Mheader_t head;
    double d;
} Treal_t;
#define T_REALSIZE sizeof (Treal_t)
typedef struct Tstring_t {
    Mheader_t head;
    char s[1];
} Tstring_t;
#define T_STRINGSIZE(l) (l + Tstringoffset)
typedef struct Tcode_t {
    Mheader_t head;
    Code_t c[1];
} Tcode_t;
#define T_CODESIZE(l) (l * C_CODESIZE + Tcodeoffset)
typedef struct Tkv_t {
    void *ko;
    void *vo;
} Tkv_t;
#define T_KVSIZE sizeof (Tkv_t)
typedef struct Tkvlist_t {
    long i, n;
    Tkv_t kv[1];
} Tkvlist_t;
#define T_KVLISTSIZE(l) (l * T_KVSIZE + Tkvoffset)
#define T_KVLISTPTRSIZE sizeof (Tkvlist_t *)
typedef struct Ttable_t {
    Mheader_t head;
    long n, ln;
    long time;
    Tkvlist_t **lp;
} Ttable_t;
#define T_TABLESIZE sizeof (Ttable_t)
typedef struct Tkvindex_t {
    Ttable_t *tp;
    Tkv_t *kvp;
    long i, j;
} Tkvindex_t;

typedef void *Tobj;

typedef struct Tonm_t { /* Object aNd Mark */
    Tobj o;
    long m;
} Tonm_t;

#define T_ISSTRING(o)  (M_TYPEOF (o) == T_STRING)
#define T_ISTABLE(o)   (M_TYPEOF (o) == T_TABLE)
#define T_ISINTEGER(o) (M_TYPEOF (o) == T_INTEGER)
#define T_ISREAL(o)    (M_TYPEOF (o) == T_REAL)
#define T_ISNUMBER(o)  (M_TYPEOF (o) == T_REAL || M_TYPEOF (o) == T_INTEGER)

#define Tgettype(p)    ((Ttype_t)(((Mheader_t *) p)->type))
#define Tgettablen(p)  (((Ttable_t *) p)->n)
#define Tgettime(p)    (((Ttable_t *) p)->time)
#define Tgetstring(p)  (char *) &(((Tstring_t *) p)->s[0])
#define Tgetcode(p)    (Code_t *) &(((Tcode_t *) p)->c[0])
#define Tgetinteger(p) (((Tinteger_t *) p)->i)
#define Tgetreal(p)    (((Treal_t *) p)->d)
#define Tgetnumber(p)  (T_ISINTEGER (p) ? Tgetinteger (p) : Tgetreal (p))

#define TCgettype(p, off)    ((Tgetcode (p) + off)->ctype)
#define TCgetfp(p, off)      ((Tgetcode (p) + off)->u.fp)
#define TCgetinteger(p, off) ((Tgetcode (p) + off)->u.i)
#define TCgetobject(p, off)  ((Tgetcode (p) + off)->u.o)
#define TCgetreal(p, off)    ((Tgetcode (p) + off)->u.d)
#define TCgetstring(p, off)  ((char *) &((Tgetcode (p) + off)->u.s))
#define TCgetnext(p, off)    ((Tgetcode (p) + off)->next)
#define TCgetaddr(p, off)    (Tgetcode (p) + off)

extern long Ttime;
extern int Tstringoffset, Tcodeoffset, Tkvoffset;
extern Tobj Ttrue, Tfalse;

void Tinit (void);
void Tterm (void);
void Tgchelper (void *);
void Tfreehelper (void *);
Tobj Tinteger (long);
Tobj Treal (double);
Tobj Tstring (char *);
Tobj Tcode (Code_t *, int, int);
Tobj Ttable (long);
void Tinsi (Tobj, long, Tobj);
void Tinsr (Tobj, double, Tobj);
void Tinss (Tobj, char *, Tobj);
void Tinso (Tobj, Tobj, Tobj);
Tobj Tfindi (Tobj, long);
Tobj Tfindr (Tobj, double);
Tobj Tfinds (Tobj, char *);
Tobj Tfindo (Tobj, Tobj);
void Tdeli (Tobj, long);
void Tdelr (Tobj, double);
void Tdels (Tobj, char *);
void Tdelo (Tobj, Tobj);
Tobj Tcopy (Tobj);
void Tgetfirst (Tobj, Tkvindex_t *);
void Tgetnext (Tkvindex_t *);
#endif /* _TBL_H */
