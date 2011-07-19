/* $XConsortium: defs.h,v 1.25 94/04/17 20:43:50 converse Exp $ */
/*

Copyright (c) 1987, 1988  X Consortium

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
/* $XFree86: xc/programs/xman/defs.h,v 1.2 2000/03/03 23:16:26 dawes Exp $ */

/*
 * xman - X window system manual page display program.
 * Author:    Chris D. Peterson, MIT Project Athena
 * Created:   October 22, 1987
 */

#ifndef HELPFILE
#define HELPFILE "/usr/lib/X11/xman.help" /* name of the default helpfile. */ 
#endif

/* The default cursors */

#define XMAN_CURSOR "left_ptr"		/* Top level cursor. */
#define HELP_CURSOR "left_ptr"	        /* The help cursor. */
#define MANPAGE_CURSOR "left_ptr"	/* The manpage cursor. */
#define SEARCH_ENTRY_CURSOR "question_arrow"	/* The search text widget
						   cursor. */
#define DIRECTORY_NORMAL "fixed" /* The default dir font */

#define OPTION_MENU "optionMenu" /* Name of the Option Menu. */
#define SECTION_MENU "sectionMenu" /* Name of the Section Menu. */

#define HELP_BUTTON "helpButton" /* Name of top help button */
#define QUIT_BUTTON "quitButton" /* Name of top quit button */
#define MANPAGE_BUTTON "manpageButton" /* Name of top manpage button */

#define TOPBOXNAME  "topBox"	/* Name of the Top Box. */
#define MANNAME "manualBrowser"	/* name for each manual page widget. */
#define SEARCHNAME "search" /* The name for the search widget. */
#define HELPNAME  "help"	/* The name of the help widget. */
#define DIRECTORY_NAME "directory" /* name of the directory widget. */
#define MANUALPAGE "manualPage"	/* name of the Scrollbyline widget that
				 contains the man page. */
#define DIALOG         "dialog"

/* Names of the menu buttons */

#ifdef INCLUDE_XPRINT_SUPPORT
#define NUM_OPTIONS 10		/* Number of menu options. */
#else /* !INCLUDE_XPRINT_SUPPORT */
#define NUM_OPTIONS 9		/* Number of menu options. */
#endif /* !INCLUDE_XPRINT_SUPPORT */

#define DIRECTORY      "displayDirectory"
#define MANPAGE        "displayManualPage"
#define HELP           "help"
#define SEARCH         "search"
#define BOTH_SCREENS   "showBothScreens"
#define REMOVE_MANPAGE "removeThisManpage"
#define OPEN_MANPAGE   "openNewManpage"
#ifdef INCLUDE_XPRINT_SUPPORT
#define PRINT_MANPAGE  "printManualPage"
#endif /* INCLUDE_XPRINT_SUPPORT */
#define SHOW_VERSION   "showVersion"
#define QUIT           "quit"

/* definitions of string to use for show both and show one. */

#define SHOW_BOTH "Show Both Screens"
#define SHOW_ONE "Show One Screen"

/* 
 * Things will not look right if you change these names to make 
 * MANUALSEARCH longer APROPOSSEARCH, see search.c for details.
 */

#define MANUALSEARCH "manualPage"
#define APROPOSSEARCH "apropos"
#define CANCEL "cancel"

#define MANUAL 0
#define APROPOS 1

#define NO_SECTION_DEFAULTS ("no default sections")

/*
 * Define HANDLE_ROFFSEQ to enable parsing of '\" <string>
 * sequences in source files to set the format pipeline.
 * This is necessary because the default pipeline causes incorrect
 * display of ascii(7) on Linux.
 * This depends on GNU roff.
 */
#ifdef HAS_GROFF
#define HANDLE_ROFFSEQ
#endif

#define DEFAULT_WIDTH 500	/* The default width of xman. */
#define SECTALLOC  8		/* The number of entries allocated
				   at a time for the manual structures. */
#define ENTRYALLOC 100		/* The number of entries allocated
				   at a time for a section. */

#define INITIAL_DIR 0		/* The Initial Directory displayed. */

#define COPY "cp"		/* copy command */
#define CHMOD_MODE 00666	/* permissions set on saved formatted files */
#define MANDESC "mandesc"	/* name of the mandesc files */

#define INDENT 15
#define TYP20STR "MMMMMMMMMMMMMMMMMMMM"

#define FILE_SAVE "yes"
#define CANCEL_FILE_SAVE "no"
#define MANTEMP "/tmp/xmanXXXXXX"

/*
 * Macro Definitions.
 */

#define streq(a, b)        ( strcmp((a), (b)) == 0 )

/* 
 * Function definitions moved to man.h
 */
