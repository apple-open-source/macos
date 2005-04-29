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

#ifndef _COMMON_H
#define _COMMON_H

/* some config and conversion definitions from graphviz distribution */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef MSWIN32
#define FEATURE_WIN32
#define FEATURE_MS
#else
#ifndef FEATURE_GTK
#define FEATURE_X11
#endif
#endif
#ifdef HAVECS
#define FEATURE_CS
#endif
#ifdef HAVENETSCAPE
#define FEATURE_NETSCAPE
#endif
#ifdef HAVEGMAP
#define FEATURE_GMAP
#define FEATURE_MINTSIZE
#endif
#ifdef HAVEDOT
#define FEATURE_DOT
#endif
#ifdef GNU
#define FEATURE_GNU
#endif
#ifdef HAVERUSAGE
#define FEATURE_RUSAGE
#endif
/* */

#ifdef FEATURE_CS
#include <ast.h>
#else
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <math.h>
#include <stdio.h>
#include <setjmp.h>
#include <ctype.h>

#ifdef FEATURE_WIN32
#include <windows.h>
#include <commdlg.h>
#endif
#ifdef FEATURE_MS
#include <malloc.h>
#endif

#define POS __FILE__, __LINE__

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#ifndef L_SUCCESS
#define L_SUCCESS 1
#define L_FAILURE 0
#endif

#define CHARSRC 0
#define FILESRC 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef REALSTRCMP
#define Strcmp(s1, s2) ( \
    *(s1) == *(s2) ? ( \
        (*s1) ? strcmp ((s1) + 1, (s2) + 1) : 0 \
    ) : (*(s1) < *(s2) ? -1 : 1) \
)
#else
#define Strcmp(s1, s2) strcmp ((s1), (s2))
#endif

extern int warnflag;
extern char *leftypath, *leftyoptions, *shellpath;
extern jmp_buf exitljbuf;
extern int idlerunmode;
extern fd_set inputfds;

int init (char *);
void term (void);
char *buildpath (char *, int);
char *buildcommand (char *, char *, int, int, char *);
void warning (char *, int, char *, char *, ...);
void panic (char *, int, char *, char *, ...);
void panic2 (char *, int, char *, char *, ...);
#endif /* _COMMON_H */
