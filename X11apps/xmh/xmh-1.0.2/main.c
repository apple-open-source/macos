/* $XConsortium: main.c,v 2.30 95/01/25 14:33:57 swick Exp $
 *
 *
 *		       COPYRIGHT 1987, 1989
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */
/* $XFree86: xc/programs/xmh/main.c,v 1.2 2001/08/01 00:45:06 tsi Exp $ */

#include "xmh.h"
#include "actions.h"

Display	*theDisplay;		/* Display variable. */
Widget	toplevel;		/* The top level widget (A hack %%%). */
char	*progName;		/* Program name. */
char	*homeDir;		/* User's home directory. */
Atom	wm_protocols;		/* WM_PROTOCOLS atom for this display */
Atom	wm_delete_window;	/* see ICCCM on Window Deletion */
Atom	wm_save_yourself;	/* see ICCCM on session management */
Atom	protocolList[2];	/* contains the two above */

struct _resources app_resources;

char	*draftFile;		/* Filename of draft. */
char	*xmhDraftFile;		/* Filename for sending. */
Toc	*folderList;		/* Array of folders. */
int	numFolders;		/* Number of entries in above array. */
Toc	InitialFolder;		/* Toc containing initial folder. */
Toc	DraftsFolder;		/* Toc containing drafts. */
Scrn	*scrnList;		/* Array of scrns in use. */
int	numScrns;		/* Number of scrns in above array. */
Widget	NoMenuForButton;	/* Flag menu widget value: no menu */
Widget	LastMenuButtonPressed;	/* to `toggle' menu buttons */
Widget	NullSource;		/* null text widget source */
Dimension rootwidth;		/* Dimensions of root window.  */
Dimension rootheight;
Pixmap	MenuItemBitmap;		/* Options menu item checkmark */
XtTranslations NoTextSearchAndReplace; /* no-op ^S and ^R in Text */

struct _LastInput lastInput;

Boolean	subProcessRunning; 	/* interlock for DoCommand/CheckMail */

/*ARGSUSED*/
static void NeedToCheckScans(
    XtPointer client_data,
    XtIntervalId *id)		/* unused */
{
    int i;
    if (!subProcessRunning) {
        DEBUG("[magic toc check ...")
	for (i = 0; i < numScrns; i++) {
	    if (scrnList[i]->toc)
		TocRecheckValidity(scrnList[i]->toc);
	    if (scrnList[i]->msg)
		TocRecheckValidity(MsgGetToc(scrnList[i]->msg));
	}
        DEBUG(" done]\n")
    }
    (void) XtAppAddTimeOut((XtAppContext)client_data,
			   (unsigned long) app_resources.rescan_interval,
			   NeedToCheckScans, client_data);
}

/*ARGSUSED*/
static void Checkpoint(
    XtPointer client_data,
    XtIntervalId *id)		/* unused */
{
    if (!subProcessRunning) {
	Cardinal n = 1;
	String params = "wm_save_yourself";
	DEBUG("(Checkpointing...")
	XmhWMProtocols(NULL, NULL, &params, &n);
        DEBUG(" done)\n")
    }
    (void) XtAppAddTimeOut((XtAppContext)client_data,
			   (unsigned long) app_resources.checkpoint_interval,
			   Checkpoint, client_data);
}

/*ARGSUSED*/
static void CheckMail(
    XtPointer client_data,
    XtIntervalId *id)		/* unused */
{
    if (!subProcessRunning) {
        DEBUG("(Checking for new mail...")
        XmhCheckForNewMail(NULL, NULL, NULL, NULL);
        DEBUG(" done)\n")
    }
    (void) XtAppAddTimeOut((XtAppContext)client_data,
			   (unsigned long) app_resources.mail_interval,
			   CheckMail, client_data);
}

/* Main loop. */

#ifdef DEBUG_CLEANUP
Boolean ExitLoop = FALSE;
#endif

int main(int argc, char **argv)
{
    XtAppContext appCtx;

    InitializeWorld(argc, argv);
    subProcessRunning = False;
    appCtx = XtWidgetToApplicationContext(toplevel);
    (void) XtAppSetWarningMsgHandler(appCtx, PopupWarningHandler);

    if (app_resources.new_mail_check && app_resources.mail_interval > 0) {
	app_resources.mail_interval *= 60000;
	(void) XtAppAddTimeOut(appCtx, (unsigned long) 0,
			       CheckMail, (XtPointer)appCtx);
    }
    if (app_resources.rescan_interval > 0) {
	app_resources.rescan_interval *= 60000;
	(void) XtAppAddTimeOut(appCtx,
			       (unsigned long) app_resources.rescan_interval,
			       NeedToCheckScans, (XtPointer)appCtx);
    }
    if (app_resources.make_checkpoints &&
	app_resources.checkpoint_interval > 0) {
	app_resources.checkpoint_interval *= 60000;
	(void) XtAppAddTimeOut(appCtx, (unsigned long)
			       app_resources.checkpoint_interval,
			       Checkpoint, (XtPointer)appCtx);
    }

    lastInput.win = -1;		/* nothing mapped yet */
#ifdef DEBUG_CLEANUP
    while (!ExitLoop) {
#else
    for (;;) {
#endif
	XEvent ev;
	XtAppNextEvent( appCtx, &ev );
	if (ev.type == KeyPress) {
	    lastInput.win = ev.xany.window;
	    lastInput.x = ev.xkey.x_root;
	    lastInput.y = ev.xkey.y_root;
	} else if (ev.type == ButtonPress) {
	    lastInput.win = ev.xany.window;
	    lastInput.x = ev.xbutton.x_root;
	    lastInput.y = ev.xbutton.y_root;
	}
	XtDispatchEvent( &ev );
    }
#ifdef DEBUG_CLEANUP
    XtDestroyApplicationContext(appCtx);
#endif    
    exit(0);
}
