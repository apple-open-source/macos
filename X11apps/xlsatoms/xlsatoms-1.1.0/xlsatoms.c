/*
 *
Copyright 1989, 1998  The Open Group
Copyright 2009 Open Text Corporation

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Jim Fulton, MIT X Consortium
 * Author:  Peter Harris, Open Text Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define ATOMS_PER_BATCH 100 /* This number can be tuned 
				higher for fewer round-trips
				lower for less bandwidth wasted */

static char *ProgramName;
static char *DisplayString;

static void do_name ( xcb_connection_t *c, char *format, char *name );
static int parse_range ( char *range, long *lowp, long *highp );
static void do_range ( xcb_connection_t *c, char *format, char *range );
static void list_atoms ( xcb_connection_t *c, char *format, int mask, 
			 long low, long high );

static void 
usage(void)
{
    fprintf (stderr, "usage:  %s [-options...]\n\n", ProgramName);
    fprintf (stderr, "where options include:\n");
    fprintf (stderr,
	     "    -display dpy            X server to which to connect\n");
    fprintf (stderr,
	     "    -format string          printf-style format to use\n");
    fprintf (stderr,
	     "    -range [num]-[num]      atom values to list\n");
    fprintf (stderr,
	     "    -name string            name of single atom to print\n");
    putc ('\n', stderr);
    exit (1);
}

int
main(int argc, char *argv[])
{
    char *displayname = NULL;
    char *format = "%lu\t%s";
    int i, doit;
    int didit = 0;
    xcb_connection_t *c = NULL;

    ProgramName = argv[0];

    for (doit = 0; doit < 2; doit++) {	/* pre-parse to get display */
	for (i = 1; i < argc; i++) {
	    char *arg = argv[i];

	    if (arg[0] == '-') {
		switch (arg[1]) {
		  case 'd':			/* -display dpy */
		    if (++i >= argc) usage ();
		    if (!doit) displayname = argv[i];
		    continue;
		  case 'f':			/* -format string */
		    if (++i >= argc) usage ();
		    if (doit) format = argv[i];
		    continue;
		  case 'r':			/* -range num-[num] */
		    if (++i >= argc) usage ();
		    if (doit) {
			do_range (c, format, argv[i]);
			didit = 1;
		    }
		    continue;
		  case 'n':			/* -name string */
		    if (++i >= argc) usage ();
		    if (doit) {
			do_name (c, format, argv[i]);
			didit = 1;
		    }
		    continue;
		}
	    }
	    usage ();
	}
	if (!doit) {
	    DisplayString = displayname;
	    if (!DisplayString)
		DisplayString = getenv("DISPLAY");
	    if (!DisplayString)
		DisplayString = "";
	    c = xcb_connect(displayname, NULL);
	    if (!c || xcb_connection_has_error(c)) {
		fprintf (stderr, "%s:  unable to open display \"%s\"\n",
			 ProgramName, DisplayString);
		exit (1);
	    }
	} else
	    if (!didit)		/* no options, default is list all */
		list_atoms(c, format, 0, 0, 0);
    }

    xcb_disconnect(c);
    exit (0);
}

static void
do_name(xcb_connection_t *c, char *format, char *name)
{
    xcb_intern_atom_reply_t *a = xcb_intern_atom_reply(c, 
	xcb_intern_atom_unchecked(c, 1, strlen(name), name), NULL);

    if (a && a->atom != XCB_NONE) {
	printf (format, (unsigned long) a->atom, name);
	putchar ('\n');
    } else {
	fprintf (stderr, "%s:  no atom named \"%s\" on server \"%s\"\n",
		 ProgramName, name, DisplayString);
    }

    if (a)
	free(a);
}


#define RangeLow (1 << 0)
#define RangeHigh (1 << 1)

static int 
parse_range(char *range, long *lowp, long *highp)
{
    char *dash;
    int mask = 0;

    if (!range) {			/* NULL means default */
	*lowp = 1;
	return RangeLow;
    }

    dash = strchr(range, '-');
    if (!dash) dash = strchr(range, ':');
    if (dash) {
	if (dash == range) {		/* -high */
	    *lowp = 1;
	} else {			/* low-[high] */
	    *dash = '\0';
	    *lowp = atoi (range);
	    *dash = '-';
	}
	mask |= RangeLow;
	dash++;
	if (*dash) {			/* [low]-high */
	    *highp = atoi (dash);
	    mask |= RangeHigh;
	}
    } else {				/* number (low == high) */
	*lowp = *highp = atoi (range);
	mask |= (RangeLow | RangeHigh);
    }

    return mask;
}

static void
do_range(xcb_connection_t *c, char *format, char *range)
{
    int mask;
    long low, high;

    mask = parse_range (range, &low, &high);
    list_atoms (c, format, mask, low, high);
}

static int
say_batch(xcb_connection_t *c, char *format, xcb_get_atom_name_cookie_t *cookie, long low, long count)
{
    xcb_generic_error_t *e;
    char atom_name[1024];
    long i;
    int done = 0;

    for (i = 0; i < count; i++)
	cookie[i] = xcb_get_atom_name(c, i + low);

    for (i = 0; i < count; i++) {
	xcb_get_atom_name_reply_t *r;
	r = xcb_get_atom_name_reply(c, cookie[i], &e);
	if (r) {
	    /* We could just use %.*s in 'format', but we want to be compatible
	       with legacy command line usage */
	    snprintf(atom_name, sizeof(atom_name), "%.*s",
		r->name_len, xcb_get_atom_name_name(r));

	    printf (format, i + low, atom_name);
	    putchar ('\n');
	    free(r);
	}
	if (e) {
	    done = 1;
	    free(e);
	}
    }

    return done;
}

static void
list_atoms(xcb_connection_t *c, char *format, int mask, long low, long high)
{
    xcb_get_atom_name_cookie_t *cookie_jar;
    int done = 0;

    switch (mask) {
      case RangeHigh:
	low = 1;
	/* fall through */
      case (RangeLow | RangeHigh):
	cookie_jar = malloc((high - low + 1) * sizeof(xcb_get_atom_name_cookie_t));
        if (!cookie_jar) {
	    fprintf(stderr, "Out of memory allocating space for %ld atom requests\n", high - low);
	    return;
	}

	say_batch(c, format, cookie_jar, low, high - low + 1);
	free(cookie_jar);
	break;

      default:
	low = 1;
	/* fall through */
      case RangeLow:
	cookie_jar = malloc(ATOMS_PER_BATCH * sizeof(xcb_get_atom_name_cookie_t));
        if (!cookie_jar) {
	    fprintf(stderr, "Out of memory allocating space for %ld atom requests\n", (long) ATOMS_PER_BATCH);
	    return;
	}
	while (!done) {
	    done = say_batch(c, format, cookie_jar, low, ATOMS_PER_BATCH);
	    low += ATOMS_PER_BATCH;
	}
	free(cookie_jar);
	break;
    }

    return;
}
