/*
 * xmalloc.c -- allocate space or die 
 *
 * Copyright 1998 by Eric S. Raymond.
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include "fetchmail.h"
#include "i18n.h"

#if defined(HAVE_VOIDPOINTER)
#define XMALLOCTYPE void
#else
#define XMALLOCTYPE char
#endif

XMALLOCTYPE *
xmalloc (size_t n)
{
    XMALLOCTYPE *p;

    p = (XMALLOCTYPE *) malloc(n);
    if (p == (XMALLOCTYPE *) 0)
    {
	report(stderr, GT_("malloc failed\n"));
	abort();
    }
    return(p);
}

XMALLOCTYPE *
xrealloc (XMALLOCTYPE *p, size_t n)
{
    if (p == 0)
	return xmalloc (n);
    p = (XMALLOCTYPE *) realloc(p, n);
    if (p == (XMALLOCTYPE *) 0)
    {
	report(stderr, GT_("realloc failed\n"));
	abort();
    }
    return p;
}

char *xstrdup(const char *s)
{
    char *p;
    p = (char *) xmalloc(strlen(s)+1);
    strcpy(p,s);
    return p;
}

#if !defined(HAVE_STRDUP)
char *strdup(const char *s)
{
    char *p;
    p = (char *) malloc(strlen(s)+1);
    if (p)
	    strcpy(p,s);
    return p;
}
#endif /* !HAVE_STRDUP */

char *xstrndup(const char *s, size_t len)
{
    char *p;
    size_t l = strlen(s);

    if (len < l) l = len;
    p = (char *)xmalloc(l + 1);
    memcpy(p, s, l);
    p[l] = '\0';
    return p;
}

/* xmalloc.c ends here */
