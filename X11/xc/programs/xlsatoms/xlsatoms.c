/*
 * $Xorg: xlsatoms.c,v 1.4 2001/02/09 02:05:54 xorgcvs Exp $
 *
Copyright 1989, 1998  The Open Group

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
 */
/* $XFree86: xc/programs/xlsatoms/xlsatoms.c,v 1.6 2001/12/14 20:02:08 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmu/Error.h>

char *ProgramName;

static void do_name ( Display *dpy, char *format, char *name );
static int parse_range ( char *range, long *lowp, long *highp );
static void do_range ( Display *dpy, char *format, char *range );
static int catcher ( Display *dpy, XErrorEvent *err );
static void list_atoms ( Display *dpy, char *format, int mask, 
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
    Display *dpy = NULL;

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
			do_range (dpy, format, argv[i]);
			didit = 1;
		    }
		    continue;
		  case 'n':			/* -name string */
		    if (++i >= argc) usage ();
		    if (doit) {
			do_name (dpy, format, argv[i]);
			didit = 1;
		    }
		    continue;
		}
	    }
	    usage ();
	}
	if (!doit) {
	    dpy = XOpenDisplay (displayname);
	    if (!dpy) {
		fprintf (stderr, "%s:  unable to open display \"%s\"\n",
			 ProgramName, XDisplayName (displayname));
		exit (1);
	    }
	} else
	    if (!didit)		/* no options, default is list all */
		list_atoms(dpy, format, 0, 0, 0);
    }

    XCloseDisplay (dpy);
    exit (0);
}

static void
do_name(Display *dpy, char *format, char *name)
{
    Atom a = XInternAtom (dpy, name, True);

    if (a != None) {
	printf (format, (unsigned long) a, name);
	putchar ('\n');
    } else {
	fprintf (stderr, "%s:  no atom named \"%s\" on server \"%s\"\n",
		 ProgramName, name, DisplayString(dpy));
    }
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
do_range(Display *dpy, char *format, char *range)
{
    int mask;
    long low, high;

    mask = parse_range (range, &low, &high);
    list_atoms (dpy, format, mask, low, high);
}


static int 
catcher(Display *dpy, XErrorEvent *err)
{
    if (err->request_code != X_GetAtomName) {
	XmuPrintDefaultErrorMessage (dpy, err, stderr);
    }
    return 0;
}

static void
list_atoms(Display *dpy, char *format, int mask, long low, long high)
{
    XErrorHandler oldhandler = XSetErrorHandler (catcher);

    switch (mask) {
      case RangeHigh:
	low = 1;
	/* fall through */
      case (RangeLow | RangeHigh):
	for (; low <= high; low++) {
	    char *s = XGetAtomName (dpy, (Atom)low);
	    if (s) {
		printf (format, low, s);
		putchar ('\n');
		XFree (s);
	    }
	}
	break;

      default:
	low = 1;
	/* fall through */
      case RangeLow:
	for (; ; low++) {
	    char *s = XGetAtomName (dpy, (Atom)low);
	    if (s) {
		printf (format, low, s);
		putchar ('\n');
		XFree (s);
	    } else {
		break;
	    }
	}
	break;
    }

    XSetErrorHandler (oldhandler);
    return;
}
