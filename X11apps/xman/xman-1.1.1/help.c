/* $XConsortium: help.c,v 1.10 94/04/17 20:43:53 dave Exp $ */
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
/* $XFree86$ */

/*
 * xman - X window system manual page display program.
 * Author:    Chris D. Peterson, MIT Project Athena
 * Created:   January 19, 1988
 */

#include "globals.h"

/*	Function Name: MakeHelpWidget.
 *	Description: This function creates the help widget so that it will be
 *                   ready to be displayed.
 *	Arguments: none.
 *	Returns: none.
 */

Boolean
MakeHelpWidget(void)
{

  ManpageGlobals * man_globals;	/* The psuedo global structure. */
  
  if (help_widget != NULL)	/* If we already have a help widget. 
				   then do not create one. */
    return(TRUE);

  man_globals = InitPsuedoGlobals();

  CreateManpageWidget(man_globals, HELPNAME, FALSE);
  help_widget = man_globals->This_Manpage;

  if (OpenHelpfile(man_globals) == FALSE) {
    XtDestroyWidget(help_widget);
    help_widget = NULL;
    return(FALSE);
  }

  ChangeLabel(man_globals->label, "Xman Help");

  XtManageChild( man_globals->manpagewidgets.manpage );
  XtRealizeWidget(  help_widget );
  SaveGlobals( man_globals->This_Manpage, man_globals );
  AddCursor( help_widget, resources.cursors.manpage);

/*
 * Set up ICCCM delete window.
 */
  XtOverrideTranslations
      (man_globals->This_Manpage, 
       XtParseTranslationTable ("<Message>WM_PROTOCOLS: RemoveThisManpage()"));
  (void) XSetWMProtocols (XtDisplay(man_globals->This_Manpage),
			  XtWindow(man_globals->This_Manpage),
			  &wm_delete_window, 1);

  return(TRUE);
}

/*	Function Name: OpenHelpfile
 *	Description: opens the helpfile.
 *	Arguments: man_globals - the psuedo globals structure.
 *	Returns: False if no helpfile was found.
 */

Boolean
OpenHelpfile(ManpageGlobals * man_globals)
{
  FILE * help_file_ptr;

  if( (help_file_ptr = fopen(resources.help_file, "r")) == NULL ) {
    PopupWarning(man_globals,
		 "Could not open help file, NO HELP WILL BE AVALIABLE.");
    return(FALSE);
  }
    
  OpenFile(man_globals, help_file_ptr);
  return(TRUE);
}
