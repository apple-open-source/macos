/*
 * $Xorg: xlsclients.c,v 1.4 2001/02/09 02:05:54 xorgcvs Exp $
 *
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
 * *
 * Author:  Jim Fulton, MIT X Consortium
 */
/* $XFree86: xc/programs/xlsclients/xlsclients.c,v 1.6 2001/12/14 20:02:09 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmu/WinUtil.h>

char *ProgramName;

static void lookat ( Display *dpy, Window root, Bool verbose, int maxcmdlen );
static void print_client_properties ( Display *dpy, Window w, 
				      Bool verbose, int maxcmdlen );
static void print_text_field ( Display *dpy, char *s, XTextProperty *tp );
static int print_quoted_word ( char *s, int maxlen );
static void unknown ( Display *dpy, Atom actual_type, int actual_format );

static void 
usage(void)
{
    fprintf (stderr,
	     "usage:  %s  [-display dpy] [-m len] [-[a][l]]\n", ProgramName);
    exit (1);
}

int
main(int argc, char *argv[])
{
    int i;
    char *displayname = NULL;
    Bool all_screens = False;
    Bool verbose = False;
    Display *dpy;
    int maxcmdlen = 10000;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (arg[0] == '-') {
	    char *cp;

	    switch (arg[1]) {
	      case 'd':			/* -display dpyname */
		if (++i >= argc) usage ();
		displayname = argv[i];
		continue;
	      case 'm':			/* -max maxcmdlen */
		if (++i >= argc) usage ();
		maxcmdlen = atoi (argv[i]);
		continue;
	    }

	    for (cp = &arg[1]; *cp; cp++) {
		switch (*cp) {
		  case 'a':		/* -all */
		    all_screens = True;
		    continue;
		  case 'l':		/* -long */
		    verbose = True;
		    continue;
		  default:
		    usage ();
		}
	    }
	} else {
	    usage ();
	}
    }

    dpy = XOpenDisplay (displayname);
    if (!dpy) {
	fprintf (stderr, "%s:  unable to open display \"%s\"\r\n",
		 ProgramName, XDisplayName (displayname));
	exit (1);
    }

    if (all_screens) {
	for (i = 0; i < ScreenCount(dpy); i++) {
	    lookat (dpy, RootWindow(dpy,i), verbose, maxcmdlen);
	}
    } else {
	lookat (dpy, DefaultRootWindow(dpy), verbose, maxcmdlen);
    }

    XCloseDisplay (dpy);
    exit (0);
}

static void
lookat(Display *dpy, Window root, Bool verbose, int maxcmdlen)
{
    Window dummy, *children = NULL, client;
    unsigned int i, nchildren = 0;

    /*
     * clients are not allowed to stomp on the root and ICCCM doesn't yet
     * say anything about window managers putting stuff there; but, try
     * anyway.
     */
    print_client_properties (dpy, root, verbose, maxcmdlen);

    /*
     * then, get the list of windows
     */
    if (!XQueryTree (dpy, root, &dummy, &dummy, &children, &nchildren)) {
	return;
    }

    for (i = 0; i < nchildren; i++) {
	client = XmuClientWindow (dpy, children[i]);
	if (client != None)
	  print_client_properties (dpy, client, verbose, maxcmdlen);
    }
}

static char *Nil = "(nil)";

static void
print_client_properties(Display *dpy, Window w, Bool verbose, int maxcmdlen)
{
    char **cliargv = NULL;
    int i, cliargc;
    XTextProperty nametp, machtp, tp;
    int charsleft = maxcmdlen;

    /*
     * get the WM_MACHINE and WM_COMMAND list of strings
     */
    if (!XGetWMClientMachine (dpy, w, &machtp)) {
	machtp.value = NULL;
	machtp.encoding = None;
    }

    if (!XGetCommand (dpy, w, &cliargv, &cliargc)) {
	if (machtp.value) XFree ((char *) machtp.value);
	return;
    }

    /*
     * do header information
     */
    if (verbose) {
	printf ("Window 0x%lx:\n", w);
	print_text_field (dpy, "  Machine:  ", &machtp);
	if (XGetWMName (dpy, w, &nametp)) {
	    print_text_field (dpy, "  Name:  ", &nametp);
	    if (nametp.value) XFree ((char *) nametp.value);
	}
    } else {
	print_text_field (dpy, NULL, &machtp);
	putchar (' ');
	putchar (' ');
    }
    if (machtp.value) XFree ((char *) machtp.value);

    if (verbose) {
	if (XGetWMIconName (dpy, w, &tp)) {
	    print_text_field (dpy, "  Icon Name:  ", &tp);
	    if (tp.value) XFree ((char *) tp.value);
	}
    }


    /*
     * do the command
     */
    if (verbose) {
	printf ("  Command:  ");
    }
    for (i = 0; i < cliargc && charsleft > 0; ) {
	charsleft -= print_quoted_word (cliargv[i], charsleft);
	i++;
	if (i < cliargc  &&  charsleft > 0) {
	    putchar (' '); charsleft--;
	}
    }
    putchar ('\n');
    XFreeStringList (cliargv);


    /*
     * do trailer information
     */
    if (verbose) {
	XClassHint clh;
	if (XGetClassHint (dpy, w, &clh)) {
	    printf ("  Instance/Class:  %s/%s",
		    clh.res_name ? clh.res_name : Nil,
		    clh.res_class ? clh.res_class : Nil);
	    if (clh.res_name) XFree (clh.res_name);
	    if (clh.res_class) XFree (clh.res_class);
	    putchar ('\n');
	}
    }
}

static void
print_text_field(Display *dpy, char *s, XTextProperty *tp)
{
    if (tp->encoding == None || tp->format == 0) {
	printf ("''");
	return;
    }

    if (s) printf ("%s", s);
    if (tp->encoding == XA_STRING && tp->format == 8) {
	printf ("%s", tp->value ? (char *) tp->value : Nil);
    } else {
	unknown (dpy, tp->encoding, tp->format);
    }
    if (s) putchar ('\n');
}

/* returns the number of characters printed */
static int
print_quoted_word(char *s, 
		  int maxlen)		/* max number of chars we can print */
{
    register char *cp;
    Bool need_quote = False, in_quote = False;
    char quote_char = '\'', other_quote = '"';
    int charsprinted = 0;

    /*
     * walk down seeing whether or not we need to quote
     */
    for (cp = s; *cp; cp++) {

	if (! ((isascii(*cp) && isalnum(*cp)) || 
	       (*cp == '-' || *cp == '_' || *cp == '.' || *cp == '+' ||
		*cp == '/' || *cp == '=' || *cp == ':' || *cp == ','))) {
	    need_quote = True;
	    break;
	}
    }

    /*
     * write out the string: if we hit a quote, then close any previous quote,
     * emit the other quote, swap quotes and continue on.
     */
    in_quote = need_quote;
    if (need_quote) {
	putchar (quote_char);
	charsprinted++; maxlen--;
    }
    for (cp = s; *cp && maxlen>0; cp++) {
	if (*cp == quote_char) {
	    if (in_quote) {
		putchar (quote_char);
		charsprinted++; maxlen--;
	    }
	    putchar (other_quote);
	    charsprinted++; maxlen--;
	    { 
		char tmp = other_quote; 
		other_quote = quote_char; quote_char = tmp;
	    }
	    in_quote = True;
	}
	putchar (*cp);
	charsprinted++; maxlen--;
    }
    /* close the quote if we opened one and if we printed the whole string */
    if (in_quote && maxlen>0) {
	putchar (quote_char);
	charsprinted++; maxlen--;
    }

    return charsprinted;
}

static void
unknown(Display *dpy, Atom actual_type, int actual_format)
{
    char *s;

    printf ("<unknown type ");
    if (actual_type == None) printf ("None");
    else if ((s = XGetAtomName (dpy, actual_type)) != NULL) {
	fputs (s, stdout);
	XFree (s);
    } else {
	fputs (Nil, stdout);
    }
    printf (" (%ld) or format %d>", actual_type, actual_format);
}

