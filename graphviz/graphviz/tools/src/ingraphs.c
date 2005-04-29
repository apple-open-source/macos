/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/*
 * Written by Emden Gansner
 */

#include <stdio.h>
#include <stdlib.h>

#define FREE_STATE 1

typedef struct { char* dummy; } Agraph_t;

extern void  agsetfile(char*);

#include <ingraphs.h>

/* nextFile:
 * Set next available file.
 * If Files is NULL, we just read from stdin.
 */
static void
nextFile(ingraph_state* sp)
{
    void*   rv = NULL;

    if (sp->Files == NULL) {
        if (sp->ctr++ == 0) {
          rv = sp->fns->dflt;
        }
    }
    else {
        while (sp->Files[sp->ctr]) {
            if ((rv = sp->fns->openf(sp->Files[sp->ctr++])) != 0) break;
            else fprintf(stderr,"Can't open %s\n",sp->Files[sp->ctr-1]);
        }
    }
    if (rv) agsetfile (fileName (sp));
    sp->fp = rv;
}

/* nextGraph:
 * Read and return next graph; return NULL if done.
 * Read graph from currently open file. If none, open next file.
 */
Agraph_t* 
nextGraph (ingraph_state* sp)
{
    Agraph_t*       g;

    if (sp->fp == NULL) nextFile(sp);
    g = NULL;

    while (sp->fp != NULL) {
        if ((g = sp->fns->readf(sp->fp)) != 0) break;
        if (sp->Files)    /* Only close if not using stdin */
          sp->fns->closef (sp->fp);
        nextFile(sp);
    }
    return g;
}

/* newIng:
 * Create new ingraph state. If sp is non-NULL, we
 * assume user is supplying memory.
 */
ingraph_state* 
newIng (ingraph_state* sp, char** files, ingdisc* disc)
{
  if (!sp) {
    sp = (ingraph_state*)malloc(sizeof(ingraph_state));
    if (!sp) {
      fprintf (stderr, "ingraphs: out of memory\n");
      return 0;
    }
    sp->heap = 1;
  }
  else sp->heap = 0;
  sp->Files = files;
  sp->ctr = 0;
  sp->fp = NULL;
  sp->fns = (ingdisc*)malloc(sizeof(ingdisc));
  if (!sp->fns) {
    fprintf (stderr, "ingraphs: out of memory\n");
    if (sp->heap) free (sp);
    return 0;
  }
  if (!disc->openf || !disc->readf || !disc->closef || !disc->dflt) {
    free (sp->fns);
    if (sp->heap) free (sp);
    fprintf (stderr, "ingraphs: NULL field in ingdisc argument\n");
    return 0;
  }
  *sp->fns = *disc;
  return sp;
}

static void*
dflt_open (char* f)
{
  return fopen (f, "r");
}

static int
dflt_close (void* fp)
{
  return fclose ((FILE*)fp);
}

typedef Agraph_t*  (*xopengfn)(void*);

static ingdisc   dflt_disc = { dflt_open, 0, dflt_close, 0 };

/* newIngraph:
 * At present, we require opf to be non-NULL. In
 * theory, we could assume a function agread(FILE*,void*)
 */
ingraph_state* 
newIngraph (ingraph_state* sp, char** files, opengfn opf)
{
    if (!dflt_disc.dflt) dflt_disc.dflt = stdin;
    if (opf) dflt_disc.readf = (xopengfn)opf;
    else {
      fprintf (stderr, "ingraphs: NULL graph reader\n");
      return 0;
    }
    return newIng (sp, files, &dflt_disc);
}

/* closeIngraph:
 * Close any open files and free discipline
 * Free sp if necessary.
 */
void
closeIngraph (ingraph_state* sp)
{
  if (sp->Files && sp->fp) sp->fns->closef (sp->fp);
  free (sp->fns);
  if (sp->heap) free (sp);
}

/* fileName:
 * Return name of current file being processed.
 */
char* 
fileName (ingraph_state* sp)
{
  if (sp->Files) {
    if (sp->ctr) return sp->Files[sp->ctr - 1];
    else return "<>";
  }
  else return "<stdin>";
}
