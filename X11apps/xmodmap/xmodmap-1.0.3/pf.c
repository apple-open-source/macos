/* $Xorg: pf.c,v 1.4 2001/02/09 02:05:56 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <ctype.h>
#include "xmodmap.h"

#define NOTINFILEFILENAME "commandline"
const char *inputFilename = NOTINFILEFILENAME;
int lineno = 0;

void process_file (const char *filename)	/* NULL means use stdin */
{
    FILE *fp;
    char buffer[BUFSIZ];

    /* open the file, eventually we'll want to pipe through cpp */

    if (!filename) {
	fp = stdin;
	inputFilename = "stdin"; 
    } else {
	fp = fopen (filename, "r");
	if (!fp) {
	    fprintf (stderr, "%s:  unable to open file '%s' for reading\n",
		     ProgramName, filename);
	    parse_errors++;
	    return;
	}
	inputFilename = filename;
    }


    /* read the input and filter */

    if (verbose) {
	printf ("! %s:\n", inputFilename);
    }

    for (lineno = 0; ; lineno++) {
	buffer[0] = '\0';
	if (fgets (buffer, BUFSIZ, fp) == NULL)
	  break;

	process_line (buffer);
    }

    inputFilename = NOTINFILEFILENAME;
    lineno = 0;
    (void) fclose (fp);
}


void process_line (char *buffer)
{
    int len;
    int i;
    char *cp;

    /* copy buffer since it may point to unwritable date */
    len = strlen(buffer);
    cp = chk_malloc(len + 1);
    strcpy(cp, buffer);
    buffer = cp;
    
    for (i = 0; i < len; i++) {		/* look for blank lines */
	register char c = buffer[i];
	if (!(isspace(c) || c == '\n')) break;
    }
    if (i == len) return;

    cp = &buffer[i];

    if (*cp == '!') return;		/* look for comments */
    len -= (cp - buffer);		/* adjust len by how much we skipped */

					/* pipe through cpp */

					/* strip trailing space */
    for (i = len-1; i >= 0; i--) {
	register char c = cp[i];
	if (!(isspace(c) || c == '\n')) break;
    }
    if (i >= 0) cp[len = (i+1)] = '\0';  /* nul terminate */

    if (verbose) {
	printf ("! %d:  %s\n", lineno+1, cp);
    }

    /* handle input */
    handle_line (cp, len);
}
