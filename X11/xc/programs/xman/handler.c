/* $XConsortium: handler.c,v 1.22 94/12/16 21:36:53 gildea Exp $ */
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
/* $XFree86: xc/programs/xman/handler.c,v 1.7 2003/04/09 20:31:31 herrb Exp $ */

/*
 * xman - X window system manual page display program.
 * Author:    Chris D. Peterson, MIT Project Athena
 * Created:   October 29, 1987
 */

#include <sys/types.h>
#include <sys/stat.h>
#include "globals.h"
#include "vendor.h"

static void PutUpManpage(ManpageGlobals * man_globals, FILE * file);
static void ToggleBothShownState(ManpageGlobals * man_globals);

/*	Function Name: OptionCallback
 *	Description: This is the callback function for the callback menu.
 *	Arguments: w - the widget we are calling back from. 
 *                 globals_pointer - a pointer to the psuedo globals structure
 *                                  for this manpage.
 *                 junk - (call data) not used.
 *	Returns: none.
 */

/*ARGSUSED*/
void
OptionCallback(Widget w, XtPointer pointer, XtPointer junk)
{
  ManpageGlobals * man_globals = (ManpageGlobals *) pointer;
  String params;
  Cardinal num_params = 1;

  if ( w == man_globals->search_entry )
    PopupSearch(XtParent(w), NULL, NULL, NULL);
  else if (w == man_globals->dir_entry) {       /* Put Up Directory */
    params = "Directory";
    GotoPage(XtParent(w), NULL, &params, &num_params);
  }
  else if (w == man_globals->manpage_entry ) {  /* Put Up Man Page */
    params = "ManualPage";
    GotoPage(XtParent(w), NULL, &params, &num_params);
  }
  else if ( w == man_globals->help_entry )      /* Help */
    PopupHelp(XtParent(w), NULL, NULL, NULL);
  else if ( w == man_globals->both_screens_entry ) /*Toggle Both_Shown State.*/
    ToggleBothShownState(man_globals);
  else if ( w == man_globals->remove_entry)     /* Kill the manpage */
    RemoveThisManpage(XtParent(w), NULL, NULL, NULL);
  else if ( w == man_globals->open_entry)       /* Open new manpage */
    CreateNewManpage(XtParent(w), NULL, NULL, NULL);
  else if ( w == man_globals->version_entry)    /* Get version */
    ShowVersion(XtParent(w), NULL, NULL, NULL);
  else if ( w == man_globals->quit_entry)      /* Quit. */
    Quit(XtParent(w), NULL, NULL, NULL);
}

/*      Function Name: ToggleBothShownState;
 *      Description: toggles the state of the both shown feature.
 *      Arguments: man_globals - the man globals structure.
 *      Returns: none.
 */

/*
 * I did not have a two state widget, which is the way this
 * should really be done.  1/22/88 - CDP.
 */

static void
ToggleBothShownState(ManpageGlobals * man_globals)
{
  char * label_str;
  Arg arglist[1];

  if (man_globals->both_shown == TRUE) {
    label_str = SHOW_BOTH;
    if (man_globals->dir_shown)
      XtUnmanageChild(man_globals->manpagewidgets.manpage);
    else
      XtUnmanageChild(man_globals->manpagewidgets.directory);
  }
  else {
    Widget manpage = man_globals->manpagewidgets.manpage;
    Widget dir = man_globals->manpagewidgets.directory;
    
    label_str = SHOW_ONE;

    XtSetArg(arglist[0], XtNpreferredPaneSize, resources.directory_height);
    XtSetValues(dir, arglist, (Cardinal) 1);

    if (!man_globals->dir_shown) {
      XtUnmanageChild(manpage);
      XtManageChild(dir);
    }
    XtManageChild(manpage);
  }
  man_globals->both_shown = !man_globals->both_shown;
  
  if (man_globals->dir_shown)
    ChangeLabel(man_globals->label,
		man_globals->section_name[man_globals->current_directory]);
  else
    ChangeLabel(man_globals->label, man_globals->manpage_title);
  
  XtSetArg(arglist[0], XtNlabel, label_str);
  XtSetValues(man_globals->both_screens_entry, arglist, ONE);
  
  /* if both are shown there is no need to switch between the two. */

  XtSetArg(arglist[0], XtNsensitive, !man_globals->both_shown);
  XtSetValues(man_globals->manpage_entry, arglist, ONE);
  XtSetValues(man_globals->dir_entry, arglist, ONE);
}

/*	Function Name: Popup
 *	Description: This function pops up the given widget under the cursor.
 *	Arguments: w - the widget to popup.
 *                 grab_kind - the kind of grab to register.
 *	Returns: none
 */

/* How far off the top of the widget to have the initial cursor postion. */

#define OFF_OF_TOP 25

void
Popup(Widget w, XtGrabKind grab_kind)
{
  int x_root,y_root,y_pos,garbage;
  unsigned int mask;
  Window junk_window;

  XQueryPointer(XtDisplay(w), XtWindow(w), &junk_window, &junk_window,
		&x_root, &y_root, &garbage, &garbage, &mask);

  y_pos = OFF_OF_TOP - Height(w)/2 - BorderWidth(w);
  PositionCenter(w, x_root, y_root, y_pos, 0, 2, 2);
  XtPopup(w, grab_kind);
}

/*	Function Name: PutUpManpage
 *	Description: Puts the manpage on the display.
 *	Arguments: man_globals - a pointer to the psuedo globals structure
 *                                  for this manpage.
 *                 file - the file to display.
 *	Returns: none.
 */

static void
PutUpManpage(ManpageGlobals * man_globals, FILE * file)
{
  String params = "ManualPage";
  Cardinal num_params = 1;
  
  if (file == NULL)
    return;

  OpenFile(man_globals, file);

  if (!man_globals->both_shown) {
    Arg arglist[1];
    XtSetArg(arglist[0], XtNsensitive, TRUE);
    XtSetValues(man_globals->manpage_entry, arglist, ONE);
    XtSetValues(man_globals->both_screens_entry, arglist, ONE);
  }
  GotoPage(man_globals->manpagewidgets.manpage, NULL, &params, &num_params);
}

/*	Function Name: DirectoryHandler
 *	Description: This is the callback function for the directory listings.
 *	Arguments: w - the widget we are calling back from. 
 *                 global_pointer - the pointer to the psuedo global structure
 *                                  associated with this manpage.
 *                 ret_val - return value from the list widget.
 *	Returns: none.
 */

void
DirectoryHandler(Widget w, XtPointer global_pointer, XtPointer ret_val)
{
  FILE * file;			/* The manpage file. */
  ManpageGlobals * man_globals = (ManpageGlobals *) global_pointer;
  XawListReturnStruct * ret_struct = (XawListReturnStruct *) ret_val;

  file = FindManualFile(man_globals, man_globals->current_directory,
			ret_struct->list_index);
  PutUpManpage(man_globals, file);
  fclose(file);
}

/*	Function Name: DirPopupCallback
 *	Description: This is the callback function for the callback menu.
 *	Arguments: w - the widget we are calling back from. 
 *                 pointer - a pointer to the psuedo globals structure
 *                                  for this manpage.
 *                 junk - (call data) not used.
 *	Returns: none.
 */

/*ARGSUSED*/
void
DirPopupCallback(Widget w, XtPointer pointer, XtPointer junk)
{
  ManpageGlobals * man_globals; 
  MenuStruct * menu_struct;
  Widget parent;
  int number;
  int current_box;

  menu_struct = (MenuStruct *) pointer;
  man_globals = (ManpageGlobals *) menu_struct->data;

  number = menu_struct->number;
  current_box = man_globals->current_directory;

  /* We have used this guy, pop down the menu. */
  
  if (number != current_box) {
    /* This is the only one that we know has a parent. */
    parent = XtParent(man_globals->manpagewidgets.box[INITIAL_DIR]);

    MakeDirectoryBox(man_globals, parent,
		     man_globals->manpagewidgets.box + number, number);
    XtUnmanageChild(man_globals->manpagewidgets.box[current_box]);
    XtManageChild(man_globals->manpagewidgets.box[number]);

    XawListUnhighlight(man_globals->manpagewidgets.box[current_box]);
    ChangeLabel(man_globals->label, man_globals->section_name[number]);
    man_globals->current_directory = number;
  }

  /* put up directory. */
  if (!man_globals->both_shown) {
    XtUnmanageChild(man_globals->manpagewidgets.manpage);
    XtManageChild(man_globals->manpagewidgets.directory);
  }
}
    
/************************************************************
 *
 * Action Routines.
 *
 ************************************************************/

/*	Function Name: SaveFormattedPage
 *	Description: This is the action routine may save the manpage.
 *      Arguments: w - any widget in the widget tree.
 *                 event - NOT USED.
 *                 params, num_params - the parameters paseed to the action
 *                                      routine, can be either Manpage or
 *                                      Directory.
 *      Returns: none.
 */

/*ARGSUSED*/
void
SaveFormattedPage(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  ManpageGlobals * man_globals;
  char cmdbuf[BUFSIZ], error_buf[BUFSIZ];

  if (*num_params != 1) {
    XtAppWarning(XtWidgetToApplicationContext(w), 
       "Xman - SaveFormattedPage: This action routine requires one argument.");
    return;
  }

  man_globals = GetGlobals(w);

/*
 * If we are not active then take no action.
 */

  if (man_globals->tempfile == '\0') return;

  switch (params[0][0]) {
  case 'S':
  case 's':

#ifndef NO_COMPRESS
    if (!man_globals->compress)
#endif

      sprintf(cmdbuf, "%s %s %s", COPY, man_globals->tempfile, 
	      man_globals->save_file);

#ifndef NO_COMPRESS
    else
      if (man_globals->gzip)
	sprintf(cmdbuf, "%s < %s > %s", GZIP_COMPRESS, man_globals->tempfile,
		man_globals->save_file);
      else
	sprintf(cmdbuf, "%s < %s > %s", COMPRESS, man_globals->tempfile, 
		man_globals->save_file);
#endif

    if(! system(cmdbuf)) {
	/* make sure the formatted man page is fully accessible by the world */
	if (chmod(man_globals->save_file, CHMOD_MODE) != 0) {
	    sprintf(error_buf,
		    "Couldn't set permissions on formatted man page '%s'.\n",
		    man_globals->save_file);
	    PopupWarning( man_globals, error_buf);
	}
    } else {
	sprintf(error_buf, "Error while executing the command '%s'.\n",
		cmdbuf);
	PopupWarning( man_globals, error_buf);
    }
    break;
  case 'C':
  case 'c':
    break;
  default:
    sprintf(error_buf,"%s %s", "Xman - SaveFormattedPagee:",
	    "Unknown argument must be either 'Save' or 'Cancel'.");
    PopupWarning(man_globals, error_buf);
    return;
  }
    
/*
 * We do not need the filename anymore, and have the fd open.
 * We will unlink it.     
 */

  unlink(man_globals->tempfile);
  XtPopdown( XtParent(XtParent(w)) );
}

/*      Function Name: GotoPage
 *      Description: The Action routine that switches over to the manpage
 *                   or directory.
 *      Arguments: w - any widget in the widget tree.
 *                 event - NOT USED.
 *                 params, num_params - the parameters paseed to the action
 *                                      routine, can be either Manpage or
 *                                      Directory.
 *      Returns: none.
 */

/*ARGSUSED*/
void
GotoPage(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  ManpageGlobals * man_globals;
  char error_buf[BUFSIZ];
  Arg arglist[1];
  Boolean sensitive;

  if (*num_params != 1) {
    XtAppWarning(XtWidgetToApplicationContext(w), 
		"Xman - GotoPage: This action routine requires one argument.");
    return;
  }

  man_globals = GetGlobals(w);

  if (man_globals->both_shown) {
    ChangeLabel(man_globals->label, 
		man_globals->section_name[man_globals->current_directory]);
    return;
  }

  switch (params[0][0]) {
  case 'M':
  case 'm':
    XtSetArg(arglist[0], XtNsensitive, &sensitive);
    XtGetValues(man_globals->manpage_entry, arglist, ONE);
    if (sensitive) {
      ChangeLabel(man_globals->label,man_globals->manpage_title);
      XtUnmanageChild(man_globals->manpagewidgets.directory);
      XtManageChild(man_globals->manpagewidgets.manpage);
      man_globals->dir_shown = FALSE;
    }
    break;
  case 'D':
  case 'd':
    ChangeLabel(man_globals->label,
		man_globals->section_name[man_globals->current_directory]);
    XtUnmanageChild(man_globals->manpagewidgets.manpage);
    XtManageChild(man_globals->manpagewidgets.directory);  
    man_globals->dir_shown = TRUE;
    break;
  default:
    sprintf(error_buf,"%s %s", "Xman - GotoPage: Unknown argument must be",
	    "either Manpage or Directory.");
    XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
    return;
  }
}

/*      Function Name: Quit.
 *      Description: Quits Xman.
 *      Arguments: w - any widget.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
Quit(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  XCloseDisplay(XtDisplay(w));
  exit(0);
}

/*      Function Name: PopupHelp
 *      Description: Pops up xman's help.
 *      Arguments: w - NOT USED.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
PopupHelp(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  if (MakeHelpWidget())
    XtPopup(help_widget,XtGrabNone);
}

/*      Function Name: PopupSearch
 *      Description: Pops up this manual pages search widget.
 *      Arguments: w - any widget in this manpage.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
PopupSearch(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  ManpageGlobals * man_globals = GetGlobals(w);

  if (man_globals->search_widget) {
    if (!XtIsRealized(man_globals->search_widget)) {
      XtRealizeWidget(man_globals->search_widget);
      AddCursor(man_globals->search_widget, resources.cursors.search_entry);
    }
    Popup(man_globals->search_widget, XtGrabNone);
  }
}

/*      Function Name: CreateNewManpage
 *      Description: Creates A New Manual Page.
 *      Arguments: w - NOT USED.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
CreateNewManpage(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  (void) CreateManpage(NULL);
  man_pages_shown++;
}

/*      Function Name: RemoveThisManpage
 *      Description: Removes a manual page.
 *      Arguments: w - any widget in the manpage.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
RemoveThisManpage(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  ManpageGlobals * man_globals = GetGlobals(w);
  
  if (man_globals->This_Manpage != help_widget) {
    RemoveGlobals(man_globals->This_Manpage);
    XtDestroyWidget(man_globals->This_Manpage);

    XtFree( (char *) man_globals->section_name);
    XtFree( (char *) man_globals->manpagewidgets.box);
    XtFree( (char *) man_globals);

    if ( (--man_pages_shown) == 0)
      Quit(w, NULL, NULL, NULL);  
  }
  else
    XtPopdown(help_widget);
}

/*      Function Name: Search
 *      Description: Actually performs a search.
 *      Arguments: w - any widget in the manpage.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
Search(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  ManpageGlobals * man_globals = GetGlobals(w);
  FILE * file = NULL;

  XtPopdown(  XtParent(XtParent(w)) );       /* popdown the search widget */

  if ( (*num_params < 1) || (*num_params > 2) ) {
    XtAppWarning(XtWidgetToApplicationContext(w), 
      "Xman - Search: This action routine requires one or two arguments.");
    return;
  }

  switch(params[0][0]) {
  case 'a':
  case 'A':
    file = DoSearch(man_globals,APROPOS);
    break;
  case 'm':
  case 'M':
    file = DoSearch(man_globals,MANUAL);
    break;
  case 'c':
  case 'C':
    file = NULL;
    break;
  default:
    XtAppWarning(XtWidgetToApplicationContext(w), 
		 "Xman - Search: First parameter unknown.");
    file = NULL;
    break;
  }

  if ( *num_params == 2 ) 
    switch (params[1][0]) {
    case 'O':
    case 'o':
      if (file != NULL) {
	Widget w;
	char * label;

	w = CreateManpage(file);
	man_pages_shown++;

	/* Put title into new manual page. */

	label = man_globals->manpage_title;
	man_globals = GetGlobals(w);
	strcpy(man_globals->manpage_title, label);
	ChangeLabel(man_globals->label, label);
      }
      break;
    default:
      XtAppWarning(XtWidgetToApplicationContext(w), 
		   "Xman - Search: Second parameter unknown.");
      break;
    }
  else {
    PutUpManpage(man_globals, file);
  }
  if (file != NULL)
	  fclose(file);
}

/*      Function Name: ShowVersion
 *      Description: Show current version.
 *      Arguments: w - any widget in the manpage.
 *                 event - NOT USED.
 *                 params, num_params - NOT USED.
 *      Returns: none.
 */

/*ARGSUSED*/
void 
ShowVersion(Widget w, XEvent * event, String * params, Cardinal * num_params)
{
  ManpageGlobals * man_globals = GetGlobals(w);
  ChangeLabel(man_globals->label, XMAN_VERSION);
}
