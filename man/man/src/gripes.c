#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "gripes.h"
#include "man.h"		/* for progname */

extern char *msg[];

static char *mantexts = "man";		/* e.g. /usr/lib/locale/%L/man.cat */

#ifdef NONLS

static char *
getmsg (int n) {
    char *s;

    if (0 < n && n <= MAXMSG)
      s = msg[n];
    else {
	fprintf (stderr, "man: internal error - cannot find message %d\n", n);
	exit (1);
    }
    return s;
}

#else /* NONLS */

#include <string.h>
#include <nl_types.h>
#include "../catopen/catopen.c"

nl_catd catfd = (nl_catd) -1;
int cat_is_open = 0;

static void
catinit (void) {
    if (!cat_is_open) {
#ifdef NL_CAT_LOCALE
	catfd = my_catopen(mantexts,NL_CAT_LOCALE);
#else
	catfd = my_catopen(mantexts,0);
#endif
	if (catfd == (nl_catd) -1) {
	    /*
	     * Only complain if LANG exists, and LANG != "en"
	     * (or when debugging). Also accept en_ZA etc.
	     * No messages for C locale.
	     */
	    if (debug) {
		fprintf(stderr,
"Looked whether there exists a message catalog %s, but there is none\n"
"(and for English messages none is needed)\n\n",
			mantexts);
            }
	}
    }
    cat_is_open = 1;
}

/*
 * This routine is unnecessary, but people ask for such things.
 *
 * Maybe man is suid or sgid to some user that owns the cat directories.
 * Maybe NLSPATH can be manipulated by the user - even though
 * modern glibc avoids using environment variables when the
 * program is suid or sgid.
 * So, maybe the string s that we are returning was user invented
 * and we have to avoid %n and the like.
 *
 * As a random hack, only allow %s,%d,%o, and only two %-signs.
 */
static int
is_suspect (char *s) {
	int ct = 0;

	while (*s) {
		if (*s++ == '%') {
			ct++;
			if (*s != 's' && *s != 'd' && *s != 'o')
				return 1;
		}
	}
	return (ct > 2);
}

static char *
getmsg (int n) {
	char *s = "";

	catinit ();
	if (catfd != (nl_catd) -1) {
		s = catgets(catfd, 1, n, "");
		if (*s && is_suspect(s))
			s = "";
	}
	if (*s == 0 && 0 < n && n <= MAXMSG)
		s = msg[n];
	if (*s == 0) {
		fprintf(stderr,
			"man: internal error - cannot find message %d\n", n);
		exit (1);
        }
	return s;
}

#endif /* NONLS */

void
gripe (int n, ...) {
    va_list p;

    va_start(p, n);
    vfprintf (stderr, getmsg(n), p);
    va_end(p);
    fflush (stderr);
}

void
fatal (int n, ...) {
    va_list p;
    fprintf (stderr, "%s: ", progname);
    va_start(p, n);
    vfprintf (stderr, getmsg(n), p);
    va_end(p);
    exit (1);
}
