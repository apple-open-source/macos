/*
 * xmalloc.c -- allocate space or die 
 *
 * Copyright 1998 by Eric S. Raymond.
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
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
xmalloc (int n)
{
    XMALLOCTYPE *p;

    p = (XMALLOCTYPE *) malloc(n);
    if (p == (XMALLOCTYPE *) 0)
    {
	report(stderr, _("malloc failed\n"));
	exit(PS_UNDEFINED);
    }
    return(p);
}

XMALLOCTYPE *
xrealloc (XMALLOCTYPE *p, int n)
{
    if (p == 0)
	return xmalloc (n);
    p = (XMALLOCTYPE *) realloc(p, n);
    if (p == (XMALLOCTYPE *) 0)
    {
	report(stderr, _("realloc failed\n"));
	exit(PS_UNDEFINED);
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

/* xmalloc.c ends here */
