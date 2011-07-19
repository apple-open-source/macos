/*
 * $XConsortium: msg.c /main/2 1996/01/14 16:51:45 kaleb $
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
/* $XFree86: xc/programs/xmh/msg.c,v 1.4 2002/04/05 21:06:28 dickey Exp $ */

/* msgs.c -- handle operations on messages. */

#include <X11/Xaw/Cardinals.h>

#include "xmh.h"
#include "tocintrnl.h"
#include "actions.h"

static int SetScrn(Msg, Scrn, Boolean, XtCallbackList, XtCallbackList);

/*	Function Name: SetEditable
 *	Description: Sets the editable flag for this message.
 *	Arguments: msg - the message.
 *                 edit - set editable to this.
 *	Returns: none
 */

static void
SetEditable(Msg msg, Boolean edit)
{
  Arg args[1];

  if (edit)
    XtSetArg(args[0], XtNeditType, XawtextEdit);
  else
    XtSetArg(args[0], XtNeditType, XawtextRead);

  XtSetValues(msg->source, args, ONE);
}

/*	Function Name: IsEditable
 *	Description: Returns true if this is an editable message.
 *	Arguments: msg - the message to edit.
 *	Returns: TRUE if editable.
 */

static Boolean
IsEditable(Msg msg)
{
  Arg args[1];
  XawTextEditType type;

  XtSetArg(args[0], XtNeditType, &type);
  XtGetValues(msg->source, args, ONE);

  return(type == XawtextEdit);
}

/* Return the user-viewable name of the given message. */

char *MsgName(Msg msg)
{
    static char result[100];
    (void) sprintf(result, "%s:%d", msg->toc->foldername, msg->msgid);
    return result;
}


/* Update the message titlebar in the given scrn. */

static void ResetMsgLabel(Scrn scrn)
{
    Msg msg;
    char str[200];
    if (scrn) {
 	msg = scrn->msg;
	if (msg == NULL) (void) strcpy(str, app_resources.banner);
	else {
	    (void) strcpy(str, MsgName(msg));
	    switch (msg->fate) {
	      case Fdelete:
		(void) strcat(str, " -> *Delete*");
 		break;
	      case Fcopy:
	      case Fmove:
		(void) strcat(str, " -> ");
		(void) strcat(str, msg->desttoc->foldername);
		if (msg->fate == Fcopy)
		    (void) strcat(str, " (Copy)");
	      default:
		break;
	    }
	    if (msg->temporary) (void)strcat(str, " [Temporary]");
	}
	ChangeLabel((Widget) scrn->viewlabel, str);
    }
}


/* A major msg change has occured; redisplay it.  (This also should
work even if we now have a new source to display stuff from.)  This
routine arranges to hide boring headers, and also will set the text
insertion point to the proper place if this is a composition and we're
viewing it for the first time. */

static void RedisplayMsg(Scrn scrn)
{
    Msg msg;
    XawTextPosition startPos, lastPos, nextPos;
    int length; char str[100];
    XawTextBlock block;
    if (scrn) {
	msg = scrn->msg;
	if (msg) {
	    startPos = 0;
	    if (app_resources.hide_boring_headers && scrn->kind != STcomp) {
		lastPos = XawTextSourceScan(msg->source, (XawTextPosition) 0,
					    XawstAll, XawsdRight, 1, FALSE);
 		while (startPos < lastPos) {
		    nextPos = startPos;
		    length = 0;
		    while (length < 8 && nextPos < lastPos) {
		        nextPos = XawTextSourceRead(msg->source, nextPos,
						    &block, 8 - length);
			(void) strncpy(str + length, block.ptr, block.length);
 			length += block.length;
		    }
		    if (length == 8) {
			if (strncmp(str, "From:", 5) == 0 ||
			    strncmp(str, "To:", 3) == 0 ||
			    strncmp(str, "Date:", 5) == 0 ||
			    strncmp(str, "Subject:", 8) == 0) break;
		    }
		    startPos = XawTextSourceScan(msg->source, startPos,
					        XawstEOL, XawsdRight, 1, TRUE);
 		}
		if (startPos >= lastPos) startPos = 0;
	    }
	    XawTextSetSource(scrn->viewwidget, msg->source, startPos);
	    if (msg->startPos > 0) {
		XawTextSetInsertionPoint(scrn->viewwidget, msg->startPos);
		msg->startPos = 0; /* Start in magic place only once. */
	    }
	} else {
	    XawTextSetSource(scrn->viewwidget, PNullSource,
			     (XawTextPosition)0);
 	}
    }
}



static char tempDraftFile[100] = "";

/* Temporarily move the draftfile somewhere else, so we can exec an mh
   command that affects it. */

static void TempMoveDraft(void)
{
    char *ptr;
    if (FileExists(draftFile)) {
	do {
	    ptr = MakeNewTempFileName();
	    (void) strcpy(tempDraftFile, draftFile);
	    (void) strcpy(strrchr(tempDraftFile, '/'), strrchr(ptr, '/'));
	} while (FileExists(tempDraftFile));
	RenameAndCheck(draftFile, tempDraftFile);
    }
}



/* Restore the draftfile from its temporary hiding place. */

static void RestoreDraft(void)
{
    if (*tempDraftFile) {
	RenameAndCheck(tempDraftFile, draftFile);
	*tempDraftFile = 0;
    }
}



/* Public routines */


/* Given a message, return the corresponding filename. */

char *MsgFileName(Msg msg)
{
    static char result[500];
    (void) sprintf(result, "%s/%d", msg->toc->path, msg->msgid);
    return result;
}



/* Save any changes to a message.  Also calls the toc routine to update the
   scanline for this msg.  Returns True if saved, false otherwise. */

int MsgSaveChanges(Msg msg)
{
    int i;
    Window w;
    if (msg->source) {
	if (XawAsciiSave(msg->source)) {
	    for (i=0; i < (int) msg->num_scrns; i++)
		EnableProperButtons(msg->scrn[i]);
	    if (!msg->temporary)
		TocMsgChanged(msg->toc, msg);
	    return True;
	}
	else {
	    char str[256];
	    (void) sprintf(str, "Cannot save changes to \"%s/%d\"!",
			   msg->toc->foldername, msg->msgid);
	    PopupError((Widget)NULL, str);
	    return False;
	}
    }
    w= (msg->source?XtWindow(msg->source):None);
    Feep(XkbBI_Failure,0,w);
    return False;
}


/*
 * Show the given message in the given scrn.  If a message is changed, and we
 * are removing it from any scrn, then ask for confirmation first.  If the
 * scrn was showing a temporary msg that is not being shown in any other scrn,
 * it is deleted.  If scrn is NULL, then remove the message from every scrn
 * that's showing it.
 */


/*ARGSUSED*/
static void ConfirmedNoScrn(
    Widget	widget,		/* unused */
    XtPointer	client_data,
    XtPointer	call_data)	/* unused */
{
    Msg		msg = (Msg) client_data;
    register int i;

    for (i=msg->num_scrns - 1 ; i >= 0 ; i--)
	SetScrn((Msg)NULL, msg->scrn[i], TRUE, (XtCallbackList) NULL,
		(XtCallbackList) NULL);
}


static void RemoveMsgConfirmed(Scrn scrn)
{
    if (scrn->kind == STtocAndView && MsgChanged(scrn->msg)) {
	Arg	args[1];
	XtSetArg(args[0], XtNtranslations, scrn->read_translations);
	XtSetValues(scrn->viewwidget, args, (Cardinal) 1);
    }
    scrn->msg->scrn[0] = NULL;
    scrn->msg->num_scrns = 0;
    XawTextSetSource(scrn->viewwidget, PNullSource, (XawTextPosition) 0);
    XtDestroyWidget(scrn->msg->source);
    scrn->msg->source = NULL;
    if (scrn->msg->temporary) {
	(void) unlink(MsgFileName(scrn->msg));
	TocRemoveMsg(scrn->msg->toc, scrn->msg);
	MsgFree(scrn->msg);
    }		
}


static void SetScrnNewMsg(
    Msg		msg,
    Scrn	scrn)
{
   scrn->msg = msg;
   if (msg == NULL) {
	XawTextSetSource(scrn->viewwidget, PNullSource, (XawTextPosition) 0);
	ResetMsgLabel(scrn);
	EnableProperButtons(scrn);
	if (scrn->kind != STtocAndView && scrn->kind != STcomp) {
	    StoreWindowName(scrn, progName);
	    DestroyScrn(scrn);
	}
    } else {
	msg->num_scrns++;
	msg->scrn = (Scrn *) XtRealloc((char *)msg->scrn,
				       (unsigned) sizeof(Scrn)*msg->num_scrns);
	msg->scrn[msg->num_scrns - 1] = scrn;
	if (msg->source == NULL)
	    msg->source = CreateFileSource(scrn->viewwidget, MsgFileName(msg),
					   scrn->kind == STcomp);
	ResetMsgLabel(scrn);
	RedisplayMsg(scrn);
	EnableProperButtons(scrn);
	if (scrn->kind != STtocAndView)
	    StoreWindowName(scrn, MsgName(msg));
    }
}

typedef struct _MsgAndScrn {
    Msg		msg;
    Scrn	scrn;
} MsgAndScrnRec, *MsgAndScrn;

/*ARGSUSED*/
static void ConfirmedWithScrn(
    Widget	widget,		/* unused */
    XtPointer	client_data,
    XtPointer	call_data)	/* unused */
{
    MsgAndScrn	mas = (MsgAndScrn) client_data;
    RemoveMsgConfirmed(mas->scrn);
    SetScrnNewMsg(mas->msg, mas->scrn);
    XtFree((char *) mas);
}
    
    
static int SetScrn(
    Msg		msg,
    Scrn	scrn,
    Boolean	force,			/* if true, force msg set scrn */
    XtCallbackList	confirms,	/* callbacks upon confirmation */
    XtCallbackList	cancels)	/* callbacks upon cancellation */
{
    register int i, num_scrns;
    static XtCallbackRec yes_callbacks[] = {
	{(XtCallbackProc) NULL,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    if (scrn == NULL) {
	if (msg == NULL || msg->num_scrns == 0) return 0;
	if (!force && XawAsciiSourceChanged(msg->source)) {
	    char str[100];
	    (void) sprintf(str,
			   "Are you sure you want to remove changes to %s?",
			   MsgName(msg));

	    yes_callbacks[0].callback = ConfirmedNoScrn;
	    yes_callbacks[0].closure = (XtPointer) msg;
	    yes_callbacks[1].callback = confirms[0].callback;
	    yes_callbacks[1].closure = confirms[0].closure;

	    PopupConfirm((Widget) NULL, str, yes_callbacks, cancels);
	    return NEEDS_CONFIRMATION;
	}
	ConfirmedNoScrn((Widget)NULL, (XtPointer) msg, (XtPointer) NULL);
	return 0;
    }

    if (scrn->msg == msg) return 0;

    if (scrn->msg) {
	num_scrns = scrn->msg->num_scrns;
	for (i=0 ; i<num_scrns ; i++)
	    if (scrn->msg->scrn[i] == scrn) break;
	if (i >= num_scrns) Punt("Couldn't find scrn in SetScrn!");
	if (num_scrns > 1)
	    scrn->msg->scrn[i] = scrn->msg->scrn[--(scrn->msg->num_scrns)];
	else {
	    if (!force && XawAsciiSourceChanged(scrn->msg->source)) {
		char		str[100];
		MsgAndScrn	cb_data;

		cb_data = XtNew(MsgAndScrnRec);
		cb_data->msg = msg;
		cb_data->scrn = scrn;
		(void)sprintf(str,
			      "Are you sure you want to remove changes to %s?",
			      MsgName(scrn->msg));
		yes_callbacks[0].callback = ConfirmedWithScrn;
		yes_callbacks[0].closure = (XtPointer) cb_data;
		yes_callbacks[1].callback = confirms[0].callback;
		yes_callbacks[1].closure = confirms[0].closure;
		PopupConfirm(scrn->viewwidget, str, yes_callbacks, cancels);
		return NEEDS_CONFIRMATION;
	    }
	    RemoveMsgConfirmed(scrn);
	}
    }
    SetScrnNewMsg(msg, scrn);
    return 0;
}



/* Associate the given msg and scrn, asking for confirmation if necessary. */

int MsgSetScrn(
    Msg msg,
    Scrn scrn,
    XtCallbackList confirms,
    XtCallbackList cancels)
{
    return SetScrn(msg, scrn, FALSE, confirms, cancels);
}


/* Same as above, but with the extra information that the message is actually
   a composition.  (Nothing currently takes advantage of that extra fact.) */

void MsgSetScrnForComp(Msg msg, Scrn scrn)
{
    (void) SetScrn(msg, scrn, FALSE, (XtCallbackList) NULL, 
		   (XtCallbackList) NULL);
}


/* Associate the given msg and scrn, even if it means losing some unsaved
   changes. */

void MsgSetScrnForce(Msg msg, Scrn scrn)
{
    (void) SetScrn(msg, scrn, TRUE, (XtCallbackList) NULL,
		   (XtCallbackList) NULL);
}



/* Set the fate of the given message. */

void MsgSetFate(Msg msg, FateType fate, Toc desttoc)
{
    Toc toc = msg->toc;
    XawTextBlock block;
    int i;
    msg->fate = fate;
    msg->desttoc = desttoc;
    if (fate == Fignore && msg == msg->toc->curmsg)
	block.ptr = "+";
    else {
	switch (fate) {
	    case Fignore:	block.ptr = " "; break;
	    case Fcopy:		block.ptr = "C"; break;
	    case Fmove:		block.ptr = "^"; break;
	    case Fdelete:	block.ptr = "D"; break;
	}
    }
    block.firstPos = 0;
    block.format = FMT8BIT;
    block.length = 1;
    if (toc->stopupdate)
	toc->needsrepaint = TRUE;
    if (toc->num_scrns && msg->visible && !toc->needsrepaint &&
	    *block.ptr != msg->buf[MARKPOS])
	(void)XawTextReplace(msg->toc->scrn[0]->tocwidget, /*%%%SourceReplace*/
			    msg->position + MARKPOS,
			    msg->position + MARKPOS + 1, &block);
    else
	msg->buf[MARKPOS] = *block.ptr;
    for (i=0; i < (int) msg->num_scrns; i++)
	ResetMsgLabel(msg->scrn[i]);
}



/* Get the fate of this message. */

FateType MsgGetFate(Msg msg, Toc *toc)
{
    if (toc) *toc = msg->desttoc;
    return msg->fate;
}


/* Make this a temporary message. */

void MsgSetTemporary(Msg msg)
{
    int i;
    msg->temporary = TRUE;
    for (i=0; i < (int) msg->num_scrns; i++)
	ResetMsgLabel(msg->scrn[i]);
}


/* Make this a permanent message. */

void MsgSetPermanent(Msg msg)
{
    int i;
    msg->temporary = FALSE;
    for (i=0; i < (int) msg->num_scrns; i++)
	ResetMsgLabel(msg->scrn[i]);
}



/* Return the id# of this message. */

int MsgGetId(Msg msg)
{
    return msg->msgid;
}


/* Return the scanline for this message. */

char *MsgGetScanLine(Msg msg)
{
    return msg->buf;
}



/* Return the toc this message is in. */

Toc MsgGetToc(Msg msg)
{
    return msg->toc;
}


/* Set the reapable flag for this msg. */

void MsgSetReapable(Msg msg)
{
    int i;
    msg->reapable = TRUE;
    for (i=0; i < (int) msg->num_scrns; i++)
	EnableProperButtons(msg->scrn[i]);
}



/* Clear the reapable flag for this msg. */

void MsgClearReapable(Msg msg)
{
    int i;
    msg->reapable = FALSE;
    for (i=0; i < (int) msg->num_scrns; i++)
	EnableProperButtons(msg->scrn[i]);
}


/* Get the reapable value for this msg.  Returns TRUE iff the reapable flag
   is set AND no changes have been made. */

int MsgGetReapable(Msg msg)
{
    return msg == NULL || (msg->reapable &&
			   (msg->source == NULL ||
			    !XawAsciiSourceChanged(msg->source)));
}


/* Make it possible to edit the given msg. */
void MsgSetEditable(Msg msg)
{
    int i;
    if (msg && msg->source) {
	SetEditable(msg, TRUE);
	for (i=0; i < (int) msg->num_scrns; i++)
	    EnableProperButtons(msg->scrn[i]);
    }
}



/* Turn off editing for the given msg. */

void MsgClearEditable(Msg msg)
{
    int i;
    if (msg && msg->source) {
	SetEditable(msg, FALSE);
	for (i=0; i < (int) msg->num_scrns; i++)
	    EnableProperButtons(msg->scrn[i]);
    }
}



/* Get whether the msg is editable. */

int MsgGetEditable(Msg msg)
{
    return msg && msg->source && IsEditable(msg);
}


/* Get whether the msg has changed since last saved. */

int MsgChanged(Msg msg)
{
    return msg && msg->source && XawAsciiSourceChanged(msg->source);
}

/* Call the given function when the msg changes. */

void 
MsgSetCallOnChange(Msg msg, void (*func)(XMH_CB_ARGS), XtPointer param)
{
  Arg args[1];
  static XtCallbackRec cb[] = { {NULL, NULL}, {NULL, NULL} };

  if (func != NULL) {
    cb[0].callback = func;
    cb[0].closure = param;
    XtSetArg(args[0], XtNcallback, cb);
  }
  else
    XtSetArg(args[0], XtNcallback, NULL);

  XtSetValues(msg->source, args, (Cardinal) 1);

}

/* Send (i.e., mail) the given message as is.  First break it up into lines,
   and copy it to a new file in the process.  The new file is one of 10
   possible draft files; we rotate amoung the 10 so that the user can have up
   to 10 messages being sent at once.  (Using a file in /tmp is a bad idea
   because these files never actually get deleted, but renamed with some
   prefix.  Also, these should stay in an area private to the user for
   security.) */

void MsgSend(Msg msg)
{
    FILEPTR from;
    FILEPTR to;
    int     p, c, l, inheader, sendwidth, sendbreakwidth;
    char   *ptr, *ptr2, **argv, str[100];
    static int sendcount = -1;
    (void) MsgSaveChanges(msg);
    from = FOpenAndCheck(MsgFileName(msg), "r");
    sendcount = (sendcount + 1) % 10;
    (void) sprintf(str, "%s%d", xmhDraftFile, sendcount);
    to = FOpenAndCheck(str, "w");
    sendwidth = app_resources.send_line_width;
    sendbreakwidth = app_resources.break_send_line_width;
    inheader = TRUE;
    while ((ptr = ReadLine(from))) {
	if (inheader) {
	    if (strncmpIgnoringCase(ptr, "sendwidth:", 10) == 0) {
		if (atoi(ptr+10) > 0) sendwidth = atoi(ptr+10);
		continue;
	    }
	    if (strncmpIgnoringCase(ptr, "sendbreakwidth:", 15) == 0) {
		if (atoi(ptr+15) > 0) sendbreakwidth = atoi(ptr+15);
		continue;
	    }
	    for (l = 0, ptr2 = ptr ; *ptr2 && !l ; ptr2++)
		l = (*ptr2 != ' ' && *ptr2 != '\t' && *ptr != '-');
	    if (l) {
		(void) fprintf(to, "%s\n", ptr);
		continue;
	    }
	    inheader = FALSE;
	    if (sendbreakwidth < sendwidth) sendbreakwidth = sendwidth;
	}
	do {
	    for (p = c = l = 0, ptr2 = ptr;
		 *ptr2 && c < sendbreakwidth;
		 p++, ptr2++) {
		 if (*ptr2 == ' ' && c < sendwidth)
		     l = p;
		 if (*ptr2 == '\t') {
		     if (c < sendwidth) l = p;
		     c += 8 - (c % 8);
		 }
		 else
		 c++;
	     }
	    if (c < sendbreakwidth) {
		(void) fprintf(to, "%s\n", ptr);
		*ptr = 0;
	    }
	    else
		if (l) {
		    ptr[l] = 0;
		    (void) fprintf(to, "%s\n", ptr);
		    ptr += l + 1;
		}
		else {
		    for (c = 0; c < sendwidth; ) {
			if (*ptr == '\t') c += 8 - (c % 8);
			else c++;
			(void) fputc(*ptr++, to);
		    }
		    (void) fputc('\n', to);
		}
	} while (*ptr);
    }
    myfclose(from);
    myfclose(to);
    argv = MakeArgv(3);
    argv[0] = "send";
    argv[1] = "-push";
    argv[2] = str;
    DoCommand(argv, (char *) NULL, (char *) NULL);
    XtFree((char *) argv);
}


/* Make the msg into the form for a generic composition.  Set msg->startPos
   so that the text insertion point will be placed at the end of the first
   line (which is usually the "To:" field). */

void MsgLoadComposition(Msg msg)
{
    static char *blankcomp = NULL; /* Array containing comp template */
    static int compsize = 0;
    static XawTextPosition startPos;
    char *file, **argv;
    int fid;
    if (blankcomp == NULL) {
	file = MakeNewTempFileName();
	argv = MakeArgv(5);
	argv[0] = "comp";
	argv[1] = "-file";
	argv[2] = file;
	argv[3] = "-nowhatnowproc";
	argv[4] = "-nodraftfolder";
	DoCommand(argv, (char *) NULL, (char *) NULL);
	XtFree((char *) argv);
	compsize = GetFileLength(file);
	if (compsize > 0) {
	    blankcomp = XtMalloc((Cardinal) compsize);
	    fid = myopen(file, O_RDONLY, 0666);
	    if (compsize != read(fid, blankcomp, compsize))
		Punt("Error reading in MsgLoadComposition!");
	    myclose(fid);
	    DeleteFileAndCheck(file);
	} else {
 	    blankcomp = "To: \n--------\n";
 	    compsize = strlen(blankcomp);
 	}
	startPos = strchr(blankcomp, '\n') - blankcomp;
    }
    fid = myopen(MsgFileName(msg), O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (compsize != write(fid, blankcomp, compsize))
	Punt("Error writing in MsgLoadComposition!");
    myclose(fid);
    TocSetCacheValid(msg->toc);
    msg->startPos = startPos;
}



/* Load a msg with a template of a reply to frommsg.  Set msg->startPos so
   that the text insertion point will be placed at the beginning of the
   message body. */

void MsgLoadReply(
    Msg msg,
    Msg frommsg,
    String *params,
    Cardinal num_params)
{
    char **argv;
    char str[100];
    int status;

    TempMoveDraft();
    argv = MakeArgv(5 + num_params);
    argv[0] = "repl";
    argv[1] = TocMakeFolderName(frommsg->toc);
    (void) sprintf(str, "%d", frommsg->msgid);
    argv[2] = str;
    argv[3] = "-nowhatnowproc";
    argv[4] = "-nodraftfolder";
    memmove( (char *)(argv + 5), (char *)params, num_params * sizeof(String *));
    status = DoCommand(argv, (char *) NULL, (char *) NULL);
    XtFree(argv[1]);
    XtFree((char*)argv);
    if (!status) {
	RenameAndCheck(draftFile, MsgFileName(msg));
	RestoreDraft();
	TocSetCacheValid(frommsg->toc); /* If -anno is set, this keeps us from
					   rescanning folder. */
	TocSetCacheValid(msg->toc);
	msg->startPos = GetFileLength(MsgFileName(msg));
    }
}



/* Load a msg with a template of forwarding a list of messages.  Set 
   msg->startPos so that the text insertion point will be placed at the end
   of the first line (which is usually a "To:" field). */

void MsgLoadForward(
  Scrn scrn,
  Msg msg,
  MsgList mlist,
  String *params,
  Cardinal num_params)
{
    char  **argv, str[100];
    int     i;
    TempMoveDraft();
    argv = MakeArgv(4 + mlist->nummsgs + num_params);
    argv[0] = "forw";
    argv[1] = TocMakeFolderName(mlist->msglist[0]->toc);
    for (i = 0; i < mlist->nummsgs; i++) {
        (void) sprintf(str, "%d", mlist->msglist[i]->msgid);
        argv[2 + i] = XtNewString(str);
    }
    argv[2 + i] = "-nowhatnowproc";
    argv[3 + i] = "-nodraftfolder";
    memmove( (char *)(argv + 4 + i), (char *)params, 
	  num_params * sizeof(String *));
    DoCommand(argv, (char *) NULL, (char *) NULL);
    for (i = 1; i < 2 + mlist->nummsgs; i++)
        XtFree((char *) argv[i]);
    XtFree((char *) argv);
    RenameAndCheck(draftFile, MsgFileName(msg));
    RestoreDraft();
    TocSetCacheValid(msg->toc);
    msg->source = CreateFileSource(scrn->viewlabel, MsgFileName(msg), True);
    msg->startPos = XawTextSourceScan(msg->source, (XawTextPosition) 0, 
				      XawstEOL, XawsdRight, 1, False);
}


/* Load msg with a copy of frommsg. */

void MsgLoadCopy(Msg msg, Msg frommsg)
{
    char str[500];
    (void)strcpy(str, MsgFileName(msg));
    CopyFileAndCheck(MsgFileName(frommsg), str);
    TocSetCacheValid(msg->toc);
}

/* Checkpoint the given message if it contains unsaved edits. */

void MsgCheckPoint(Msg msg)
{
    int len;
    char file[500];

    if (!msg || !msg->source || !IsEditable(msg) ||
	!XawAsciiSourceChanged(msg->source))
	return;

    if (*app_resources.checkpoint_name_format == '/') {
	(void) sprintf(file, app_resources.checkpoint_name_format, msg->msgid);
    } else {
	(void) sprintf(file, "%s/", msg->toc->path);
	len = strlen(file);
	(void) sprintf(file + len, app_resources.checkpoint_name_format,
		       msg->msgid);
    }
    if (!XawAsciiSaveAsFile(msg->source, file)) {
	char str[256];
	(void) sprintf(str, "Unsaved edits cannot be checkpointed to %s.",
		       file);
	PopupError((Widget)NULL, str);
    }
    TocSetCacheValid(msg->toc);
}

/* Free the storage being used by the given msg. */

void MsgFree(Msg msg)
{
    XtFree(msg->buf);
    XtFree((char *)msg);
}

/* Insert the associated message, if any, filtering it first */

/*ARGSUSED*/
void XmhInsert(
    Widget	w,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    Scrn scrn = ScrnFromWidget(w);
    Msg msg = scrn->msg;
    XawTextPosition pos;
    XawTextBlock block;

    if (msg == NULL || scrn->assocmsg == NULL) return;

    if (app_resources.insert_filter && *app_resources.insert_filter) {
	char command[1024];
	char *argv[4];
	argv[0] = "/bin/sh";
	argv[1] = "-c";
	sprintf(command, "%s %s", app_resources.insert_filter,
		MsgFileName(scrn->assocmsg));
	argv[2] = command;
	argv[3] = NULL;
	block.ptr = DoCommandToString(argv);
        block.length = strlen(block.ptr);
    }
    else {
	/* default filter is equivalent to 'echo "<filename>"' */
	block.ptr = XtNewString(MsgFileName(scrn->assocmsg));
	block.length = strlen(block.ptr);
    }
    block.firstPos = 0;
    block.format = FMT8BIT;
    pos = XawTextGetInsertionPoint(scrn->viewwidget);
    if (XawTextReplace(scrn->viewwidget, pos, pos, &block) != XawEditDone)
	PopupError(scrn->parent, "Insertion failed!");
    XtFree(block.ptr);
}

/*	Function Name: CreateFileSource
 *	Description: Creates an AsciiSource for a file. 
 *	Arguments: w - the widget to create the source for.
 *                 filename - the file to assign to this source.
 *                 edit - if TRUE then this disk source is editable.
 *	Returns: the source.
 */

Widget
CreateFileSource(Widget w, String filename, Boolean edit)
{
  Arg arglist[10];
  Cardinal num_args = 0;

  XtSetArg(arglist[num_args], XtNtype, XawAsciiFile);  num_args++;
  XtSetArg(arglist[num_args], XtNstring, filename);    num_args++;
  if (edit) 
      XtSetArg(arglist[num_args], XtNeditType, XawtextEdit);
  else
      XtSetArg(arglist[num_args], XtNeditType, XawtextRead);
  num_args++;

  return(XtCreateWidget("textSource", asciiSrcObjectClass, w, 
			arglist, num_args));
}
