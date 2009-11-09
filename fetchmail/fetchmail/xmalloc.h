/* xmalloc.h -- split out of fetchmail.h */

#ifndef XMALLOC_H
#define XMALLOC_H

#include "config.h"

/* xmalloc.c */
#if defined(HAVE_VOIDPOINTER)
#define XMALLOCTYPE void
#else
#define XMALLOCTYPE char
#endif
XMALLOCTYPE *xmalloc(size_t);
XMALLOCTYPE *xrealloc(/*@null@*/ XMALLOCTYPE *, size_t);
#define xfree(p) { if (p) { free(p); } (p) = 0; }
char *xstrdup(const char *);

#endif
