/*
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

#include <X11/Intrinsic.h>
#include <stdio.h>
#include <stdlib.h>

#define NONAME "-AppResTest-"

static char *ProgramName;

static XrmQuark XrmQString;

static void 
usage (void)
{
    fprintf (stderr,
	     "usage:  %s  [class [instance]] [-1] [toolkitoptions]\n",
	     ProgramName);
    fprintf (stderr,
	     "-1      list resources only at the specified level\n");
    fprintf (stderr,
	     "The number of class and instance elements must be equal.\n");
    exit (1);
}

/* stolen from Xlib Xrm.c */
static void 
PrintBindingQuarkList(XrmBindingList bindings, 
				  XrmQuarkList quarks,
				  FILE* stream)
{
    Bool	firstNameSeen;

    for (firstNameSeen = False; *quarks; bindings++, quarks++) {
	if (*bindings == XrmBindLoosely) {
	    (void) fprintf(stream, "*");
	} else if (firstNameSeen) {
	    (void) fprintf(stream, ".");
	}
	firstNameSeen = True;
	(void) fputs(XrmQuarkToString(*quarks), stream);
    }
}

/* stolen from Xlib Xrm.c */
/* output out the entry in correct file syntax */
/*ARGSUSED*/
static Bool 
DumpEntry(XrmDatabase *db, 
	  XrmBindingList bindings, 
	  XrmQuarkList quarks, 
	  XrmRepresentation *type, 
	  XrmValuePtr value, 
	  XPointer data)
{
    FILE			*stream = (FILE *)data;
    register unsigned int	i;
    register char		*s;
    register char		c;

    if (*type != XrmQString)
	(void) putc('!', stream);
    PrintBindingQuarkList(bindings, quarks, stream);
    s = value->addr;
    i = value->size;
    if (*type == XrmQString) {
	(void) fputs(":\t", stream);
	if (i)
	    i--;
    }
    else
	fprintf(stream, "=%s:\t", XrmRepresentationToString(*type));
    if (i && (*s == ' ' || *s == '\t'))
	(void) putc('\\', stream); /* preserve leading whitespace */
    while (i--) {
	c = *s++;
	if (c == '\n') {
	    if (i)
		(void) fputs("\\n\\\n", stream);
	    else
		(void) fputs("\\n", stream);
	} else if (c == '\\')
	    (void) fputs("\\\\", stream);
	else if ((c < ' ' && c != '\t') ||
		 ((unsigned char)c >= 0x7f && (unsigned char)c < 0xa0))
	    (void) fprintf(stream, "\\%03o", (unsigned char)c);
	else
	    (void) putc(c, stream);
    }
    (void) putc('\n', stream);
    return False;
}

int
main (int argc, char *argv[])
{
    Widget toplevel;
    char *iname = NONAME, *cname = NONAME;
    XtAppContext xtcontext;
    XrmName names[101];
    XrmClass classes[101];
    int i;
    int mode = XrmEnumAllLevels;

    ProgramName = argv[0];
    if (argc > 1 && argv[1][0] != '-') {
	cname = argv[1];
	if (argc > 2 && argv[2][0] != '-')
	    iname = argv[2];
    }

    XrmStringToClassList(cname, classes);
    XrmStringToNameList(iname, names);
    for (i = 0; names[i]; i++)
	;
    if (!i || classes[i] || !classes[i-1]) usage ();
    argv[0] = XrmNameToString(names[0]);

    toplevel = XtAppInitialize(&xtcontext, XrmClassToString(classes[0]),
			       NULL, 0, &argc, argv, NULL, NULL, 0);

    iname = NULL;
    cname = NULL;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-1"))
	    mode = XrmEnumOneLevel;
	else if (argv[i][0] == '-')
	    usage();
	else if (!cname)
	    cname = argv[i];
	else if (!iname)
	    iname = argv[i];
	else
	    usage();
    }

    if (!iname) {
	XtGetApplicationNameAndClass(XtDisplay(toplevel), &iname, &cname);
	names[0] = XrmStringToName(iname);
    }

    XrmQString = XrmPermStringToQuark("String");

    XrmEnumerateDatabase(XtDatabase(XtDisplay(toplevel)),
			 names, classes, mode,
			 DumpEntry, (XPointer)stdout);

    return (0);
}
