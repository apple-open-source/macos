/* $Xorg: dsimple.c,v 1.4 2001/02/09 02:05:54 xorgcvs Exp $ */
/*

Copyright 1993, 1998  The Open Group

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
/* $XFree86: xc/programs/xlsfonts/dsimple.c,v 3.6 2001/12/14 20:02:09 dawes Exp $ */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
/*
 * Other_stuff.h: Definitions of routines in other_stuff.
 *
 * Written by Mark Lillibridge.   Last updated 7/1/87
 */

#include "dsimple.h"

/*
 * Just_display: A group of routines designed to make the writting of simple
 *               X11 applications which open a display but do not open
 *               any windows much faster and easier.  Unless a routine says
 *               otherwise, it may be assumed to require program_name, dpy,
 *               and screen already defined on entry.
 *
 * Written by Mark Lillibridge.   Last updated 7/1/87
 */


/* This stuff is defined in the calling program by just_display.h */
char    *program_name = "unknown_program";
Display *dpy = NULL;
int      screen = 0;

/*
 * Malloc: like malloc but handles out of memory using Fatal_Error.
 */
char *
Malloc(unsigned size)
{
	char *data;

	if (!(data = malloc(size)))
	  Fatal_Error("Out of memory!");

	return(data);
}
	

/*
 * Realloc: like Malloc except for realloc, handles NULL using Malloc.
 */
char *
Realloc(char *ptr, int size)
{
	char *new_ptr;

	if (!ptr)
	  return(Malloc(size));

	if (!(new_ptr = realloc(ptr, size)))
	  Fatal_Error("Out of memory!");

	return(new_ptr);
}


/*
 * Get_Display_Name (argc, argv) Look for -display, -d, or host:dpy (obselete)
 * If found, remove it from command line.  Don't go past a lone -.
 */
static char *
Get_Display_Name(int *pargc/* MODIFIED */, char **argv/* MODIFIED */)
{
    int argc = *pargc;
    char **pargv = argv+1;
    char *displayname = NULL;
    int i;

    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (!strcmp (arg, "-display") || !strcmp (arg, "-d")) {
	    if (++i >= argc) usage ();

	    displayname = argv[i];
	    *pargc -= 2;
	    continue;
	}
	if (!strcmp(arg,"-")) {
		while (i<argc)
			*pargv++ = argv[i++];
		break;
	}
	*pargv++ = arg;
    }

    *pargv = NULL;
    return (displayname);
}


/*
 * Open_Display: Routine to open a display with correct error handling.
 *               Does not require dpy or screen defined on entry.
 */
static Display *
Open_Display(char *display_name)
{
	Display *d;

	d = XOpenDisplay(display_name);
	if (d == NULL) {
	    fprintf (stderr, "%s:  unable to open display '%s'\n",
		     program_name, XDisplayName (display_name));
	    exit(1);
	}

	return(d);
}


/*
 * Setup_Display_And_Screen: This routine opens up the correct display (i.e.,
 *                           it calls Get_Display_Name) and then stores a
 *                           pointer to it in dpy.  The default screen
 *                           for this display is then stored in screen.
 *                           Does not require dpy or screen defined.
 */
void
Setup_Display_And_Screen(int *argc/* MODIFIED */, char **argv/* MODIFIED */)
{
	char *displayname = Get_Display_Name(argc, argv);

	dpy = Open_Display (displayname);
	screen = XDefaultScreen(dpy);
}

/*
 * Close_Display: Close display
 */
void Close_Display(void)
{
    if (dpy == NULL)
      return;
      
    XCloseDisplay(dpy);
    dpy = NULL;
}


/*
 * Standard fatal error routine - call like printf but maximum of 7 arguments.
 * Does not require dpy or screen defined.
 */
void Fatal_Error(char *msg, ...)
{
	va_list args;
	fflush(stdout);
	fflush(stderr);
	fprintf(stderr, "%s: error: ", program_name);
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);
	fprintf(stderr, "\n");
        Close_Display();
	exit(EXIT_FAILURE);
}
