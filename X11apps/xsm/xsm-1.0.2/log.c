/* $Xorg: log.c,v 1.4 2001/02/09 02:06:01 xorgcvs Exp $ */
/******************************************************************************

Copyright 1994, 1998  The Open Group

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
******************************************************************************/
/* $XFree86: xc/programs/xsm/log.c,v 1.4 2001/01/17 23:46:29 dawes Exp $ */

#include "xsm.h"
#include "save.h"
#include "popup.h"
#include "log.h"

#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>

static Widget logPopup;
static Widget   logForm;
static Widget	   logText;
static Widget	   logOkButton;



void
DisplayLogXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    static int first_time = 1;

    if (client_log_visible)
    {
	/* Make sure it is visible */

	XMapRaised (XtDisplay (topLevel), XtWindow (logPopup));
    }
    else
    {
	PopupPopup (mainWindow, logPopup,
	    False, first_time, 50, 50, "DelLogWinAction()");

	client_log_visible = 1;

	if (first_time)
	    first_time = 0;
    }
}



static void
logOkXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    XtPopdown (logPopup);
    client_log_visible = 0;
}



void
add_log_text(char *str)
{
    XawTextPosition pos = XawTextGetInsertionPoint (logText);
    XawTextBlock text;

    text.firstPos = 0;
    text.length = strlen (str);
    text.ptr = str;
    text.format = XawFmt8Bit;

    XawTextReplace (logText, pos, pos, &text);
}



static void
DelLogWinAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XtCallCallbacks (logOkButton, XtNcallback, NULL);
}



void
create_log_popup(void)

{
    /*
     * Pop up for session log
     */

    static XtActionsRec actions[] = {
        {"DelLogWinAction", DelLogWinAction}
    };

    XtAppAddActions (appContext, actions, XtNumber (actions));

    logPopup = XtVaCreatePopupShell ("logPopup",
	topLevelShellWidgetClass, topLevel,
	XtNallowShellResize, True,
	NULL);
    
    logForm = XtVaCreateManagedWidget (
	"logForm", formWidgetClass, logPopup,
	NULL);

    logText = XtVaCreateManagedWidget (
	"logText", asciiTextWidgetClass, logForm,
        XtNfromHoriz, NULL,
        XtNfromVert, NULL,
	XtNeditType, XawtextAppend,
        XtNdisplayCaret, False,
	XtNscrollVertical, XawtextScrollAlways,
	XtNscrollHorizontal, XawtextScrollWhenNeeded,
	XtNresizable, True,
	XtNtop, XawChainTop,
	XtNbottom, XawChainBottom,
	NULL);

    logOkButton = XtVaCreateManagedWidget (
	"logOkButton", commandWidgetClass, logForm,
        XtNfromHoriz, NULL,
        XtNfromVert, logText,
	XtNtop, XawChainBottom,
	XtNbottom, XawChainBottom,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
        NULL);

    XtAddCallback (logOkButton, XtNcallback, logOkXtProc, NULL);
}
