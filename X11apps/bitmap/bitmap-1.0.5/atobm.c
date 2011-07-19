/*

Copyright 1988, 1993, 1998  The Open Group

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

/*
 * atobm - ascii to bitmap filter
 * Author:  Jim Fulton, MIT X Consortium
 */

#include <stdio.h>
#include <ctype.h>
#include <X11/Xos.h>
#include <stdlib.h>

static char *ProgramName;

static void doit(FILE *fp, char *filename, char *chars, 
		 int xhot, int yhot, char *name);

static void 
usage (void)
{
    fprintf (stderr, "usage:  %s [-options ...] [filename]\n\n",
	     ProgramName);
    fprintf (stderr, 
	"where options include:\n");
    fprintf (stderr, 
	"    -chars cc        chars to use for 0 and 1 bits, respectively\n");
    fprintf (stderr, 
	"    -name variable   name to use in bitmap file\n");
    fprintf (stderr, 
	"    -xhot number     x position of hotspot\n");
    fprintf (stderr,
	"    -yhot number     y position of hotspot\n");
    fprintf (stderr, "\n");
    exit (1);
}


static char *
cify_name (char *name)
{
    int length = name ? strlen (name) : 0;
    int i;

    for (i = 0; i < length; i++) {	/* strncpy (result, begin, length); */
	char c = name[i];
	if (!((isascii(c) && isalnum(c)) || c == '_')) name[i] = '_';
    }
    return name;
}

static char *
StripName(char *name)
{
  char *begin = strrchr(name, '/');
  char *end, *result;
  int length;

  begin = (begin ? begin+1 : name);
  end = strchr(begin, '.');	/* change to strrchr to allow longer names */
  length = (end ? (end - begin) : strlen (begin));
  result = (char *) malloc (length + 1);
  strncpy (result, begin, length);
  result [length] = '\0';
  return (result);
}

int
main (int argc, char *argv[])
{
    int i;
    int xhot = -1, yhot = -1;
    char *filename = NULL;
    char *chars = "-#";
    char *name = NULL;
    FILE *fp;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (arg[0] == '-') {
	    switch (arg[1]) {
	      case '\0':
		filename = NULL;
		continue;
	      case 'c':
		if (++i >= argc) usage ();
		chars = argv[i];
		continue;
	      case 'n':
		if (++i >= argc) usage ();
		name = argv[i];
		continue;
	      case 'x':
		if (++i >= argc) usage ();
		xhot = atoi (argv[i]);
		continue;
	      case 'y':
		if (++i >= argc) usage ();
		yhot = atoi (argv[i]);
		continue;
	      default:
		usage ();
	    }
	} else {
	    filename = arg;
	}
    }

    if (strlen (chars) != 2) {
	fprintf (stderr,
	 "%s:  bad character list \"%s\", must have exactly 2 characters\n",
		 ProgramName, chars);
	exit (1);
    }

    if (filename) {
	fp = fopen (filename, "r");
	if (!fp) {
	    fprintf (stderr, "%s:  unable to open file \"%s\".\n",
		     ProgramName, filename);
	    exit (1);
	}
    } else {
	fp = stdin;
    }

    if (!name) name = filename ? StripName (filename) : "";
    cify_name (name);
    doit (fp, filename, chars, xhot, yhot, name);

    if (filename) (void) fclose (fp);
    exit (0);
}


static void
doit (FILE *fp, 
      char *filename, 
      char *chars, 
      int xhot, int yhot, 
      char *name)
{
    int i, j;
    int last_character;
    char buf[BUFSIZ];
    char *cp, *newline;
    int width = 0, height = 0;
    int len;
    int removespace = (((isascii(chars[0]) && isspace(chars[0])) ||
			(isascii(chars[1]) && isspace(chars[1]))) ? 0 : 1);
    int lineno = 0;
    int bytes_per_scanline = 0;
    struct _scan_list {
	int allocated;
	int used;
	unsigned char *scanlines;
	struct _scan_list *next;
    } *head = NULL, *slist = NULL;
    static unsigned char masktable[] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    int padded = 0;
	
#define NTOALLOC 16
#define NewSList() \
	    slist = (struct _scan_list *) calloc (1, sizeof *slist); \
	    if (!slist) { \
		fprintf (stderr, "%s:  unable to allocate scan list\n", \
			 ProgramName); \
		return; \
	    } \
	    slist->allocated = NTOALLOC * bytes_per_scanline; \
	    slist->scanlines = (unsigned char *) calloc(slist->allocated, 1); \
	    if (!slist->scanlines) { \
		fprintf (stderr, "%s:  unable to allocate char array\n", \
			 ProgramName); \
		return; \
	    } \
	    slist->used = 0; \
	    slist->next = NULL; 

    while (1) {
	buf[0] = '\0';
	lineno++;
	if (fgets (buf, sizeof buf, fp) == NULL) break;

	cp = buf;
	if (removespace) {
	    for (cp = buf; *cp && isascii(*cp) && isspace(*cp); cp++) ;
	}
	if (*cp == '\n' || !*cp) continue;  /* empty line */

	newline = strchr(cp, '\n');
	if (!newline) {
	    fprintf (stderr, "%s:  line %d too long.\n",
		     ProgramName, lineno);
	    return;
	}

	if (removespace) {
	    for (; --newline > cp && isascii(*newline) && isspace(*newline); );
	    newline++;
	}

	if (newline == cp + 1) continue;

	*newline = '\0';
	len = strlen (cp);

	if (width == 0) {
	    width = len;
	    padded = ((width & 7) != 0);
	    bytes_per_scanline = (len + 7) / 8;
	    NewSList ();
	    head = slist;
	} else if (width != len) {
	    fprintf (stderr,
		     "%s:  line %d is %d characters wide instead of %d\n",
		     ProgramName, lineno, len, width);
	    return;
	}

	if (slist->used + 1 >= slist->allocated) {
	    struct _scan_list *old = slist;
	    NewSList ();
	    old->next = slist;
	}

	/* okay, parse the line and stick values into the scanline array */
	for (i = 0; i < width; i++) {
	    int ind = (i & 7);
	    int on = 0;

	    if (cp[i] == chars[1]) {
		on = 1;
	    } else if (cp[i] != chars[0]) {
		fprintf (stderr, "%s:  bad character '%c' on line %d\n",
			 ProgramName, cp[i], lineno);
	    }

	    if (on) slist->scanlines[slist->used] |= masktable[ind];
	    if (ind == 7) slist->used++;
	}
	if (padded) slist->used++;
	height++;
    }

    printf ("#define %s_width %d\n", name, width);
    printf ("#define %s_height %d\n", name, height);
    if (xhot >= 0) printf ("#define %s_x_hot %d\n", name, xhot);
    if (yhot >= 0) printf ("#define %s_y_hot %d\n", name, yhot);
    printf ("\n");
    printf ("static unsigned char %s_bits[] = {\n", name);

    j = 0;
    last_character = height * bytes_per_scanline - 1;
    for (slist = head; slist; slist = slist->next) {
	for (i = 0; i < slist->used; i++) {
	    printf (" 0x%02x", slist->scanlines[i]);
	    if (j != last_character) putchar (',');
	    if ((j % 12) == 11) putchar ('\n');
	    j++;
	}
    }
    printf (" };\n");
    return;
}
