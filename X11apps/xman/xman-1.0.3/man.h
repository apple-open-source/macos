/* $XConsortium: man.h,v 1.31 94/12/16 21:36:53 gildea Exp $ */
/* $XdotOrg: xc/programs/xman/man.h,v 1.6 2004/07/29 00:40:35 gisburn Exp $ */
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
/* $XFree86: xc/programs/xman/man.h,v 1.4 2001/07/25 15:05:27 dawes Exp $ */


/* X toolkit header files */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Cardinals.h>

/* Std system and C header files */

#include <stdio.h>
#include <limits.h>

#include <X11/Xfuncs.h>
#include <X11/Xos.h>

/* X include files */

#include <X11/Xatom.h>

/* Widget header files. */

#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Viewport.h>

/* program specific header files. */

#include "ScrollByL.h"

#include "version.h"
#include "defs.h"

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))

#define Error(x) { printf x ; exit(EXIT_FAILURE); }
#define Assertion(expr, msg) { if (!(expr)) { Error msg } }
#ifdef DEBUG
#  define Log(x)   { if(True)  printf x; }
#else
#  define Log(x)   { if(False) printf x; }
#endif /* DEBUG */

/* 
 * Assigning values here allows the user of Bitwise Or.
 */

typedef struct _XmanFonts {
  XFontStruct * directory;		/* The font for the directory.  */
} XmanFonts;

typedef struct _XmanCursors {
  Cursor top,			/* The top Cursor, default for xman. */
    help,			/* The top cursor for the help menu. */
    manpage,			/* The cursor for the Manpage. */
    search_entry;		/* The cursor for the text widget in the
				   search box.*/
  Pixel fg_color;		/* foreground color of cursors. */
  Pixel bg_color;		/* background color of cursors. */
} XmanCursors;

typedef struct _ManPageWidgets {
  Widget manpage,		/* The manual page window (scrolled) */
    directory,			/* The widget in which all directories will
				   appear. */
    *box;			/* The boxes containing the sections. */
} ManPageWidgets;

typedef struct _MenuStruct {
  caddr_t data;
  int number;
  XrmQuark quark;
} MenuStruct;

/*
 * The manual sections and entries
 */

typedef struct tManual {
  char * blabel;		/* The button label. */
  char ** entries; 		/* The individual man page file names. */
  char ** entries_less_paths;   /* Entry names only */
  int nentries;			/* how many (TOTAL)*/
  int nalloc;			/* how much space allocated */
  int flags;			/* suffix, fold */
} Manual;
   
/* psuedo Globals that are specific to each manpage created. */

typedef struct _ManpageGlobals{
  int current_directory;	/* The directory currently being shown 
				   on this manpage. */
  Boolean dir_shown,		/* True if the directory is then current
				   visable screen */
    both_shown;			/* If true then both the manpage and
				   the directory are to be shown.*/
  Widget label,			/* The label widget at the top of the page. */
    standby,			/* The please standby widget. */
    save,			/* The "would you like to save?" widget. */
    search_widget,		/* The search widget popup. */
    help_button,		/* The help button. */
    option_menu,		/* The option menu. */
    text_widget;		/* text widget containing search string. */
 
  /* Widgets (Objects really) for the command menu entries. */ 
 
  Widget dir_entry, manpage_entry, help_entry, 
    search_entry, both_screens_entry, remove_entry, 
    open_entry, print_entry, version_entry, quit_entry; 
 
#ifdef INCLUDE_XPRINT_SUPPORT
  /* Print objects and data */
  Widget printdialog_shell;     /* Shell for the print dialog */
  Widget printdialog;           /* Print dialog */
#endif /*INCLUDE_XPRINT_SUPPORT */
  /* Misc. */

  char manpage_title[80];       /* The label to use for the current manpage. */

  char save_file[80];		/* the name of the file to save fomatted 
				   page into. */
  char tempfile[80];		/* the name of the file to copy the formatted
				   page from. */
  Boolean compress;		/* Compress file on save? */
  Boolean gzip;			/* Gzip file on save? */
  Boolean deletetempfile;	/* Need to delete tempfile when done? */
  char ** section_name;		/* The name of each of the sections */

  ManPageWidgets manpagewidgets; /* The manpage widgets. */

  /* Things to remember when cleaning up when killing manpage. */

  Widget This_Manpage;		/* a pointer to the root of
				   this manpage. */

  FILE  *curr_file;             /* Current file shown in manpage widget */
} ManpageGlobals;


/* Resource manager sets these. */

typedef struct _Xman_Resources {
  XmanFonts fonts;		/* The fonts used for the man pages. */
  XmanCursors cursors;		/* The cursors for xman. */
  Boolean show_help_syntax;	/* True if syntax message should be dumped to
				   stdout. */
  Boolean both_shown_initial;	/* The initial state of the manual pages
				   show two screens or only one. */
  Boolean top_box_active;	/* Put up the Top Box. */
  Boolean clear_search_string;	/* clear the search string each time it
				   is popped down? */
  int directory_height;	        /* The default height of directory in 
				   both_shown mode. */
  char * help_file;		/* The name of the help file. */
  char * title;	    	        /* The title for topBox */
  Boolean iconic;		/* Should topBox come up in an iconic state */
} Xman_Resources;

/************************************************************
 *
 * Function Defintions 
 * 
 ************************************************************/

/*
 * This is easier than trying to find all calls to StrAlloc().
 */

#define StrAlloc(ptr) XtNewString(ptr)

/* Standard library function definitions. */
#include <stdlib.h>

/* buttons.c */

ManpageGlobals * InitPsuedoGlobals(void);
Widget CreateManpage(FILE * file);
void CreateManpageWidget(ManpageGlobals * man_globals, char * name, Boolean full_instance);
void FormUpWidgets(Widget parent, char ** full_size, char ** half_size);
void MakeDirectoryBox(ManpageGlobals *man_globals, Widget parent, Widget *dir_disp, int section);
void MakeSaveWidgets(ManpageGlobals *man_globals, Widget parent);
void MakeTopBox(void);

/* handler.c */

void DirPopupCallback(Widget w, XtPointer pointer, XtPointer junk);
void DirectoryHandler(Widget w, XtPointer global_pointer, XtPointer ret_val);
void OptionCallback(Widget w, XtPointer pointer, XtPointer junk);
void Popup(Widget w, XtGrabKind grab_kind);

/* Action Routines. */

void CreateNewManpage(Widget w, XEvent * event, String * params, Cardinal * num_params);
void GotoPage(Widget w, XEvent * event, String * params, Cardinal * num_params);
void PopupHelp(Widget w, XEvent * event, String * params, Cardinal * num_params);
void PopupSearch(Widget w, XEvent * event, String * params, Cardinal * num_params);
void Quit(Widget w, XEvent * event, String * params, Cardinal * num_params);
void RemoveThisManpage(Widget w, XEvent * event, String * params, Cardinal * num_params);
void SaveFormattedPage(Widget w, XEvent * event, String * params, Cardinal * num_params);
void Search(Widget w, XEvent * event, String * params, Cardinal * num_params);
#ifdef INCLUDE_XPRINT_SUPPORT
void PrintThisManpage(Widget w, XEvent * event, String * params, Cardinal * num_params);
#endif /* INCLUDE_XPRINT_SUPPORT */
void ShowVersion(Widget w, XEvent * event, String * params, Cardinal * num_params);

/* help.c */

Boolean MakeHelpWidget(void);
Boolean OpenHelpfile(ManpageGlobals * man_globals);

/* man.c */
Bool ReadManConfig(char manpath[]);
int Man(void);

/* misc.c */
FILE * FindManualFile(ManpageGlobals * man_globals, int section_num, int entry_num);
ManpageGlobals * GetGlobals(Widget w);
void AddCursor(Widget w, Cursor cursor);
void ChangeLabel(Widget w, char * str);
void OpenFile(ManpageGlobals * man_globals, FILE * file);
void PopupWarning(ManpageGlobals * man_globals, char * string);
void PositionCenter(Widget widget, int x, int y, int above, int left, int v_space, int h_space);
void PrintError(char * string);
void RemoveGlobals(Widget w);
void SaveGlobals(Widget w, ManpageGlobals * globals);
void ParseEntry(char *entry, char *path, char *sect, char *page);
FILE * Format(ManpageGlobals * man_globals, char * entry);

/* search */
FILE * DoSearch(ManpageGlobals * man_globals, int type);
void MakeSearchWidget(ManpageGlobals * man_globals, Widget parent);

/* tkfunctions.c */

int Width(Widget);
int Height(Widget);
int BorderWidth(Widget);
char * Name(Widget);
