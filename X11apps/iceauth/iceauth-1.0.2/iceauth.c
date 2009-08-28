/*
 * $Xorg: iceauth.c,v 1.4 2001/02/09 02:05:31 xorgcvs Exp $
 *
 * xauth - manipulate authorization file
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
 * Original Author of "xauth" : Jim Fulton, MIT X Consortium
 * Modified into "iceauth"    : Ralph Mor, X Consortium
 */
/* $XFree86: xc/programs/iceauth/iceauth.c,v 1.4 2001/01/17 23:44:54 dawes Exp $ */

#include "iceauth.h"


/*
 * global data
 */
const char *ProgramName;		/* argv[0], set at top of main() */
int verbose = -1;			/* print certain messages */
Bool ignore_locks = False;		/* for error recovery */
Bool break_locks = False;		/* for error recovery */

/*
 * local data
 */

static char *authfilename = NULL;	/* filename of cookie file */
static char *defcmds[] = { "source", "-", NULL };  /* default command */
static int ndefcmds = 2;
static const char *defsource = "(stdin)";


/*
 * utility routines
 */
static void usage (void)
{
    static const char prefixmsg[] = 
"\n"
"where options include:\n"
"    -f authfilename                name of authority file to use\n"
"    -v                             turn on extra messages\n"
"    -q                             turn off extra messages\n"
"    -i                             ignore locks on authority file\n"
"    -b                             break locks on authority file\n"
"\n"
"and commands have the following syntax:\n";
    static const char suffixmsg[] = 
"A dash may be used with the \"merge\" and \"source\" to read from the\n"
"standard input.  Commands beginning with \"n\" use numeric format.\n";

    fprintf (stderr, "usage:  %s [-options ...] [command arg ...]\n",
	     ProgramName);
    fprintf (stderr, "%s\n", prefixmsg);
    print_help (stderr, "    ");	/* match prefix indentation */
    fprintf (stderr, "\n%s\n", suffixmsg);
    exit (1);
}


/*
 * The main routine - parses command line and calls action procedures
 */
int
main (int argc, char *argv[])
{
    int i;
    const char *sourcename = defsource;
    char **arglist = defcmds;
    int nargs = ndefcmds;
    int status;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (arg[0] == '-') {
	    char *flag;

	    for (flag = (arg + 1); *flag; flag++) {
		switch (*flag) {
		  case 'f':		/* -f authfilename */
		    if (++i >= argc) usage ();
		    authfilename = argv[i];
		    continue;
		  case 'v':		/* -v */
		    verbose = 1;
		    continue;
		  case 'q':		/* -q */
		    verbose = 0;
		    continue;
		  case 'b':		/* -b */
		    break_locks = True;
		    continue;
		  case 'i':		/* -i */
		    ignore_locks = True;
		    continue;
		  default:
		    usage ();
		}
	    }
	} else {
	    sourcename = "(argv)";
	    nargs = argc - i;
	    arglist = argv + i;
	    if (verbose == -1) verbose = 0;
	    break;
	}
    }

    if (verbose == -1) {		/* set default, don't junk stdout */
	verbose = (isatty(fileno(stdout)) != 0);
    }

    if (!authfilename) {
	authfilename = IceAuthFileName ();	/* static name, do not free */
	if (!authfilename) {
	    fprintf (stderr,
		     "%s:  unable to generate an authority file name\n",
		     ProgramName);
	    exit (1);
	}
    }
    if (auth_initialize (authfilename) != 0) {
	/* error message printed in auth_initialize */
	exit (1);
    }

    status = process_command (sourcename, 1, nargs, arglist);

    (void) auth_finalize ();
    exit ((status != 0) ? 1 : 0);
}
