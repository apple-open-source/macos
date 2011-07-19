/*
 * $XConsortium: compfuncs.c,v 2.18 92/04/08 12:20:08 rws Exp $
 *
 *
 * 		      COPYRIGHT 1987, 1989
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
/* $XFree86: xc/programs/xmh/compfuncs.c,v 1.2 2001/08/01 00:45:06 tsi Exp $ */

/* comp.c -- action procedures to handle composition buttons. */

#include "xmh.h"
#include "actions.h"

/* Reset this composition widget to be one with just a blank message
   template. */

/*ARGSUSED*/
void DoResetCompose(
    Widget	widget,		/* unused */
    XtPointer	client_data,
    XtPointer	call_data)	/* unused */
{
    Scrn	scrn = (Scrn) client_data;
    Msg		msg;
    XtCallbackRec	confirms[2];

    confirms[0].callback = (XtCallbackProc) DoResetCompose;
    confirms[0].closure = (XtPointer) scrn;
    confirms[1].callback = (XtCallbackProc) NULL;
    confirms[1].closure = (XtPointer) NULL;

    if (MsgSetScrn((Msg) NULL, scrn, confirms, (XtCallbackList) NULL) ==
	NEEDS_CONFIRMATION)
	return;

    msg = TocMakeNewMsg(DraftsFolder);
    MsgLoadComposition(msg);
    MsgSetTemporary(msg);
    MsgSetReapable(msg);
    (void) MsgSetScrn(msg, scrn, (XtCallbackList) NULL, (XtCallbackList) NULL);
}

/*ARGSUSED*/
void XmhResetCompose(
    Widget	w,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    Scrn scrn = ScrnFromWidget(w);
    DoResetCompose(w, (XtPointer) scrn, (XtPointer) NULL);
}


/* Send the message in this widget.  Avoid sending the same message twice.
   (Code elsewhere actually makes sure this button is disabled to avoid
   sending the same message twice, but it doesn't hurt to be safe here.) */

/*ARGSUSED*/
void XmhSend(
    Widget	w,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    Scrn scrn = ScrnFromWidget(w);
    if (scrn->msg == NULL) return;
    if (MsgChanged(scrn->msg) || ! MsgGetReapable(scrn->msg)) {
	MsgSend(scrn->msg);
	MsgSetReapable(scrn->msg);
    }
}


/* Save any changes to the message.  This also makes this message permanent. */

/*ARGSUSED*/
void XmhSave(
    Widget	w,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    Scrn scrn = ScrnFromWidget(w);
    DEBUG("XmhSave\n")
    if (scrn->msg == NULL) return;
    MsgSetPermanent(scrn->msg);
    if (MsgSaveChanges(scrn->msg))
	MsgClearReapable(scrn->msg);
}


/* Utility routine; creates a composition screen containing a forward message
   of the messages in the given msglist. */

void CreateForward(
  MsgList mlist,
  String *params,
  Cardinal num_params)
{
    Scrn scrn;
    Msg msg;
    scrn = NewCompScrn();
    msg = TocMakeNewMsg(DraftsFolder);
    MsgLoadForward(scrn, msg, mlist, params, num_params);
    MsgSetTemporary(msg);
    MsgSetScrnForComp(msg, scrn);
    MapScrn(scrn);
}
