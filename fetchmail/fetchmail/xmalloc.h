/** \file xmalloc.h -- Declarations for the fail-on-OOM string functions */

#ifndef XMALLOC_H
#define XMALLOC_H

#include "config.h"

/* xmalloc.c */
#if defined(HAVE_VOIDPOINTER)
#define XMALLOCTYPE void
#else
#define XMALLOCTYPE char
#endif

/** Allocate \a n characters of memory, abort program on failure. */
XMALLOCTYPE *xmalloc(size_t n);

/** Reallocate \a n characters of memory, abort program on failure. */
XMALLOCTYPE *xrealloc(/*@null@*/ XMALLOCTYPE *, size_t n);

/** Free memory at position \a p and set pointer \a p to NULL afterwards. */
#define xfree(p) { if (p) { free(p); } (p) = 0; }

/** Duplicate string \a src to a newly malloc()d memory region and return its
 * pointer, abort program on failure. */
char *xstrdup(const char *src);

/** Duplicate at most the first \a n characters from \a src to a newly
 * malloc()d memory region and NUL-terminate it, and return its pointer, abort
 * program on failure. The memory size is the lesser of either the string
 * length including NUL byte or n + 1. */
char *xstrndup(const char *src, size_t n);

#endif
