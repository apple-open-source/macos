#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: fileio.c,v 1.1.1.1 1999/04/15 17:45:12 wsanchez Exp $";
#endif
/*
 * Program:	ASCII file reading routines
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 * Copyright 1991-1993  University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee to the University of
 * Washington is hereby granted, provided that the above copyright notice
 * appears in all copies and that both the above copyright notice and this
 * permission notice appear in supporting documentation, and that the name
 * of the University of Washington not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  This software is made available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pine and Pico are trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior
 * written permission of the University of Washington.
 *
 */
/*
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here. A better message writing scheme should
 * be used.
 */
#include        <stdio.h>
#include	"estruct.h"
#include        "edef.h"


FILE    *ffp;                           /* File pointer, all functions. */

/*
 * Open a file for reading.
 */
ffropen(fn)
  char    *fn;
{
    if ((ffp=fopen(fn, "r")) == NULL)
      return (FIOFNF);

    return (FIOSUC);
}


/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Check only at the newline.
 */
ffputline(buf, nbuf)
    CELL  buf[];
{
    register int    i;

    for (i = 0; i < nbuf; ++i)
       fputc(buf[i].c&0xFF, ffp);

    fputc('\n', ffp);

    if (ferror(ffp)) {
        emlwrite("Write I/O error");
        return (FIOERR);
    }

    return (FIOSUC);
}



/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 */
ffgetline(buf, nbuf)
  register char   buf[];
{
    register int    c;
    register int    i;

    i = 0;

    while ((c = fgetc(ffp)) != EOF && c != '\n') {
	/*
	 * Don't blat the CR should the newline be CRLF and we're
	 * running on a unix system.  NOTE: this takes care of itself
	 * under DOS since the non-binary open turns newlines into '\n'.
	 */
	if(c == '\r'){
	    if((c = fgetc(ffp)) == EOF || c == '\n')
	      break;

	    if (i < nbuf-2)		/* Bare CR. Insert it and go on... */
	      buf[i++] = '\r';		/* else, we're up a creek */
	}

        if (i >= nbuf-2) {
	    buf[nbuf - 2] = c;	/* store last char read */
	    buf[nbuf - 1] = 0;	/* and terminate it */
            emlwrite("File has long line");
            return (FIOLNG);
        }
        buf[i++] = c;
    }

    if (c == EOF) {
        if (ferror(ffp)) {
            emlwrite("File read error");
            return (FIOERR);
        }

        if (i != 0)
	  emlwrite("File doesn't end with newline.  Adding one.", NULL);
	else
	  return (FIOEOF);
    }

    buf[i] = 0;
    return (FIOSUC);
}
