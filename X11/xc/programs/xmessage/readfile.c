/* $XConsortium: readfile.c,v 1.5 94/12/11 14:25:48 gildea Exp $ */
/*

Copyright (c) 1988, 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xmessage/readfile.c,v 1.2 2000/02/14 19:21:04 dawes Exp $ */

#include <X11/Xos.h>			/* for types.h */
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * get_data_from_file - read data from a file into a single buffer; meant 
 * for small files containing messages.
 */
static char *get_data_from_file (filename, len_return)
    char *filename;
    int *len_return;
{
    FILE *fp;
    struct stat statbuf;
    char *cp;
    int count;

    if (stat (filename, &statbuf) != 0 || statbuf.st_size < 0) {
	perror(filename);
	return NULL;
    }

    cp = malloc (statbuf.st_size + 1);
    if (!cp) {
	fprintf(stderr, "cannot get memory for message file\n");
	return NULL;
    }

    fp = fopen (filename, "r");
    if (!fp) {
	perror(filename);
	(void) free (cp);
	return NULL;
    }

    count = fread (cp, 1, statbuf.st_size, fp);
    if (count == 0 && statbuf.st_size != 0) {
	perror(filename);
	(void) free (cp);
	(void) fclose (fp);
	return NULL;
    }

    cp[count] = '\0';		/* since we allocated one extra */
    *len_return = count;
    (void) fclose (fp);
    return cp;
}

/*
 * get_data_from_stdin - read data from stdin into a single buffer.
 */
static char *get_data_from_stdin (len_return)
    int *len_return;
{
    char *cp;
    int count;
    int allocated;
    int n;

    allocated = BUFSIZ;
    cp = malloc (allocated + 1);
    if (!cp) {
	fprintf(stderr, "cannot get memory for message file\n");
	return NULL;
    }
    count = 0;

    while ((n = fread (cp + count, 1, BUFSIZ, stdin)) > 0) {
	count += n;
	/* Here count <= allocated. Prepare for next round. */
	if (count + BUFSIZ > allocated) {
	    allocated = 2 * allocated;
	    cp = realloc (cp, allocated + 1);
	    if (!cp) {
		fprintf(stderr, "cannot get memory for message file\n");
		return NULL;
	    }
	}
    }

    cp[count] = '\0';		/* since we allocated one extra */
    *len_return = count;
    return cp;
}


/*
 * read_file - read data from indicated file and return pointer to malloced
 * buffer.  Returns NULL on error or if no such file.
 */
char *read_file (filename, len)
    char *filename;
    int *len;			/* returned */
{
    if (filename[0] == '-' && filename[1] == '\0') {
	return (get_data_from_stdin (len));
    } else {
	return (get_data_from_file (filename, len));
    }
}
