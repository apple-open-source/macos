/* $XConsortium: main.c,v 1.23 94/04/17 20:43:55 rws Exp $ */
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
/* $XFree86: xc/programs/xman/main.c,v 1.4 2001/10/28 03:34:36 tsi Exp $ */

/*
 * xman - X window system manual page display program.
 * Author:    Chris D. Peterson, MIT Project Athena
 * Created:   October 22, 1987
 */

#include "globals.h"
#ifndef ZERO
#include <X11/Xaw/Cardinals.h>
#endif /* ZERO */

#if !defined(lint) && !defined(SABER) && 0
  static char version[] = XMAN_VERSION;  /* via strings. */
#endif

static void ArgError(int argc, char ** argv);
static void AdjustDefResources(void);

#define Offset(field) (XtOffsetOf(Xman_Resources , field))

static XtResource my_resources[] = {
  {"directoryFontNormal", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     Offset(fonts.directory), XtRString, DIRECTORY_NORMAL},
  {"bothShown", XtCBoolean, XtRBoolean, sizeof(Boolean),
     Offset(both_shown_initial), XtRString, "False"},
  {"directoryHeight", "DirectoryHeight", XtRInt, sizeof(int),
     Offset(directory_height), XtRString, "150"},  
  {"topCursor", XtCCursor, XtRCursor, sizeof(Cursor), 
     Offset(cursors.top), XtRString, XMAN_CURSOR},
  {"helpCursor", XtCCursor, XtRCursor, sizeof(Cursor),
     Offset(cursors.help), XtRString, HELP_CURSOR},
  {"manpageCursor", XtCCursor, XtRCursor, sizeof(Cursor),
     Offset(cursors.manpage), XtRString, MANPAGE_CURSOR},
  {"searchEntryCursor", XtCCursor, XtRCursor, sizeof(Cursor),
     Offset(cursors.search_entry), XtRString, SEARCH_ENTRY_CURSOR},
  {"pointerColor", XtCForeground, XtRPixel, sizeof(Pixel),
     Offset(cursors.fg_color), XtRString, "XtDefaultForeground"},
  {"pointerColorBackground", XtCBackground, XtRPixel, sizeof(Pixel),
     Offset(cursors.bg_color), XtRString, "XtDefaultBackground"},
  {"help", XtCBoolean, XtRBoolean, sizeof(Boolean),
     Offset(show_help_syntax), XtRImmediate, FALSE},
  {"helpFile", XtCFile, XtRString, sizeof(char *),
     Offset(help_file), XtRString, HELPFILE},
  {"topBox", XtCBoolean, XtRBoolean, sizeof(Boolean),
     Offset(top_box_active), XtRString, "True"},
  {"clearSearchString", "ClearSearchString", XtRBoolean, sizeof(Boolean),
     Offset(clear_search_string), XtRImmediate, (caddr_t) TRUE},
  {"title", XtCString, XtRString, sizeof(char *),
     Offset(title), XtRString, "xman"},
  {"iconic", XtCBoolean, XtRBoolean, sizeof(Boolean),
     Offset(iconic), XtRString, "False"},
};

#undef Offset

/*
 * The resource that we absolutely need.
 */

static char * fallback_resources[] = { "Xman*quitButton.translations:	#override \\n   <Btn1Up>: Quit() reset()",
 "Xman*helpButton.sensitive:                    FALSE",
 "Xman*manpageButton.sensitive:                 FALSE",
 "Xman*helpButton.Label:			Help",
 "Xman*quitButton.Label:			Quit",
 "Xman*manpageButton.Label:		        Manual Page",
 "Xman*topLabel.label:         		        No App-Defaults File",
 NULL,
};

/*
 * This is necessary to keep all TopLevel shells from becoming
 * the size that is specified on the command line.
 */

static XrmOptionDescRec xman_options[] = {
{"-geometry", "*topBox.geometry",        XrmoptionSepArg, (caddr_t) NULL},
{"-help",     "help",                    XrmoptionNoArg,  (caddr_t) "True"},
{"=",         "*topBox.geometry",        XrmoptionIsArg,  (caddr_t) NULL},
{"-pagesize", "*manualBrowser.geometry", XrmoptionSepArg, (caddr_t) NULL},
{"-notopbox", "topBox",                  XrmoptionNoArg,  (caddr_t) "False"},
{"-helpfile", "helpFile",                XrmoptionSepArg, (caddr_t) NULL},
{"-bothshown","bothShown",               XrmoptionNoArg,  (caddr_t) "True"},
{"-title",    "title",                   XrmoptionSepArg, (caddr_t) "xman"}, 
{"-iconic",   "iconic",                  XrmoptionNoArg,  (caddr_t) "True"},
};

XtActionsRec xman_actions[] = {
  {"GotoPage",          GotoPage},
  {"Quit",              Quit},
  {"Search",            Search},
  {"PopupHelp",         PopupHelp},
  {"PopupSearch",       PopupSearch},
  {"CreateNewManpage",  CreateNewManpage},
  {"RemoveThisManpage", RemoveThisManpage},
  {"SaveFormattedPage", SaveFormattedPage},
  {"ShowVersion",       ShowVersion},
};

char **saved_argv;
int saved_argc;

/*
 * This atom is used to make the application ICCCM compliant.
 */
Atom wm_delete_window;

/*	Function Name: main
 *	Description: This is the main driver for Xman.
 *	Arguments: argc, argv - the command line arguments.
 *	Returns: return, what return.
 */

int main(int argc, char ** argv)
{
  XtAppContext app_con;

  saved_argc = argc;
  saved_argv = (char **)XtMalloc(argc * sizeof(char *));
  bcopy(argv, saved_argv, argc * sizeof(char *));

  XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

  initial_widget = XtAppInitialize(&app_con, "Xman", xman_options,
				   XtNumber(xman_options), &argc, argv,
				   fallback_resources, NULL, ZERO);
  wm_delete_window = XInternAtom(XtDisplay(initial_widget), "WM_DELETE_WINDOW",
				 False);

  manglobals_context = XStringToContext(MANNAME);

  AdjustDefResources();

  XtGetApplicationResources( initial_widget, (caddr_t) &resources, 
			    my_resources, XtNumber(my_resources),
			    NULL, (Cardinal) 0);

  if ( (argc != 1) || (resources.show_help_syntax) ) {
    ArgError(argc, argv);
    exit(1);
  }

  XtAppAddActions(app_con, xman_actions, XtNumber(xman_actions));

  if (!resources.fonts.directory)
	PrintError("Failed to get the directory font.");

#ifdef DEBUG
  printf("debugging mode\n");
#endif

/*
 * Set the default width and height.
 * I am not real happy with this method, but it will usually do something
 * reasonable, if not the "right" thing.  It is not a real big issue since
 * it is easy to change the values with resources or command line options.
 * NOTE: if you are in a 100 dpi display you will lose.
 */

  default_width = DEFAULT_WIDTH;
  default_height=DisplayHeight(XtDisplay(initial_widget), 
			       DefaultScreen(XtDisplay(initial_widget)));
  default_height *= 3;
  default_height /= 4;

  if ( (sections = Man()) == 0 )
    PrintError("There are no manual sections to display, check your MANPATH.");

  if (resources.top_box_active) 
    MakeTopBox();	
  else
    (void) CreateManpage(NULL);

/*
 * We need to keep track of the number of manual pages that are shown on
 * the screen so that if this user does not have a top box then when he
 * removes all his manual pages we can kill off the xman process.
 * To make things easier we will consider the top box a shown manual page
 * here, but since you cannot remove it, man_page_shown only goes to zero when
 * no top box is present.
 */

  man_pages_shown = 1;		

  XtAppMainLoop(app_con);

  exit(0);
}

/*	Function Name: ArgError
 *	Description:  Prints error message about unknow arguments.
 *	Arguments: argc, argv - args not understood.
 *	Returns: none.
 */

static void 
ArgError(int argc, char ** argv)
{
  int i;

  static char **syntax, *syntax_def[] = {
  "-help",                   "Print this message",
  "-helpfile <filename>",    "Specifies the helpfile to use.",
  "-bothshown",              "Show both the directory and manpage at once.",
  "-notopbox",               "Starts with manpage rather than topbox.",
  "-geometery <geom>",       "Specifies the geometry of the top box.",
  "=<geom>",                 "Specifies the geometry of the top box.",
  "-pagesize <geom>",        "Specifies the geometry of the manual page.",
  "-bw <pixels>",            "Width of all window borders.",
  "-borderwidth <pixels>",   "Width of all window borders.",
  "-bd <color>",             "Color of all window borders.",
  "-bordercolor <color>",    "Color of all window borders.",
  "-fg <color>",             "Foreground color for the application.",
  "-foreground <color>",     "Foreground color for the application.",
  "-bg <color>",             "Background color for the application.",
  "-background <color>",     "Background color for the application.",
  "-display <display name>", "Specify a display that is not the default",
  "-fn <font>",              "Font to be used for button and label text.",
  "-font <font>",            "Font to be used for button and label text.",
  "-name <name>",            "Change the name used for retrieving resources.",
  "-title <name>",           "Change the name without affecting resources.",
  "-xrm <resource>",         "Specifies a resource on the command line.",
  NULL, NULL,
  };
  
  syntax = syntax_def;

  for (i = 1; i < argc ; i++) 
    (void) printf("This argument is unknown to Xman: %s\n", argv[i]);
  
  (void) printf("\nKnown arguments are:\n");

  while ( *syntax != NULL ) {
    printf("%-30s - %s\n", syntax[0], syntax[1]);
    syntax += 2;
  }
}

/*    Function Name: AdjustDefResources
 *    Description: Changes default resources which contain paths when
 *            XWINHOME is set
 *    Arguments: none
 *    Returns: nothing
 */

static void
AdjustDefResources(void)
{
  char        *xwinhome = NULL;
  int i;

  if (!(xwinhome = getenv("XWINHOME")))
    return;

  for (i = 0; i < sizeof(my_resources)/sizeof(XtResource); i++) {
    if (!strcmp(my_resources[i].resource_name, "helpFile")) {
      if (!(my_resources[i].default_addr =
              malloc(strlen(xwinhome) + sizeof("/lib/X11/xman.help")))) {
        fprintf(stderr, "malloc failure\n");
        exit(1);
      }
      sprintf(my_resources[i].default_addr, "%s/lib/X11/xman.help", xwinhome);
    }
  }
}
