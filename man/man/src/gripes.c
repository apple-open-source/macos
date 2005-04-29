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

#include <nl_types.h>
#include "../catopen/catopen.c"

nl_catd catfd = (nl_catd) -1;
int cat_is_open = 0;

static void
catinit (void) {
    if (!cat_is_open) {
	catfd = my_catopen(mantexts,0);
	if (catfd == (nl_catd) -1) {
	    /* Only complain if at least one of NLSPATH and LANG exists,
	       and LANG != "en" (or when debugging). Also accept en_ZA etc. */
	    char *s, *lg;
	    s = getenv("NLSPATH");
	    lg = getenv("LANG");
	    if (!lg)
		    lg = getenv("LC_MESSAGES");
	    if (!lg)
		    lg = getenv("LC_ALL");
	    if ((s || lg) && (!lg || strncmp(lg, "en", 2))) {
		perror(mantexts);
		fprintf(stderr,
"Failed to open the message catalog %s on the path NLSPATH=%s\n\n",
			mantexts, s ? s : "<none>");
	    } else if (debug) {
		perror(mantexts);
		fprintf(stderr,
"Looked whether there exists a message catalog %s, but there is none\n"
"(and for English messages none is needed)\n\n",
			mantexts);
            }
	}
    }
    cat_is_open = 1;
}

static char *
getmsg (int n) {
    char *s;

    catinit ();
    if (catfd == (nl_catd) -1 || !*(s = catgets(catfd, 1, n, ""))) {
        if (0 < n && n <= MAXMSG)
          s = msg[n];
        else {
	  fprintf (stderr, "man: internal error - cannot find message %d\n", n);
	  exit (1);
        }
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
