/*
 * $XConsortium: tocutil.c,v 2.60 95/01/09 16:52:53 swick Exp $
 * $XFree86: xc/programs/xmh/tocutil.c,v 3.3 2001/10/28 03:34:40 tsi Exp $
 *
 *
 *			COPYRIGHT 1987, 1989
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

/* tocutil.c -- internal routines for toc stuff. */

#include "xmh.h"
#include "toc.h"
#include "tocutil.h"
#include "tocintrnl.h"

#ifdef X_NOT_POSIX
extern long lseek();
#endif

Toc TUMalloc(void)
{
    Toc toc;
    toc = XtNew(TocRec);
    bzero((char *)toc, (int) sizeof(TocRec));
    toc->msgs = (Msg *) NULL;
    toc->seqlist = (Sequence *) NULL;
    toc->validity = unknown;
    return toc;
}


/* Returns TRUE if the scan file for the given toc is out of date. */

int TUScanFileOutOfDate(Toc toc)
{
    return LastModifyDate(toc->path) > toc->lastreaddate;
}


/* Make sure the sequence menu entries correspond exactly to the sequences 
 * for this toc.
 */

void TUCheckSequenceMenu(Toc toc)
{
    Scrn	scrn;
    register int i, n;
    Arg		query_args[2];
    char 	*name;
    Cardinal	j;
    int		numChildren;
    Widget	menu, item;
    Button	button;
    WidgetList	children;

    static XtCallbackRec callbacks[] = {
	{ DoSelectSequence,		(XtPointer) NULL},
	{ (XtCallbackProc) NULL,	(XtPointer) NULL},
    };
    static Arg  args[] = {
	{ XtNcallback,			(XtArgVal) callbacks},
	{ XtNleftMargin, 		(XtArgVal) 18},
    };

    for (j=0; j < toc->num_scrns; j++) {
	scrn = toc->scrn[j];

	/* Find the sequence menu and the number of entries in it. */

	name = MenuBoxButtons[XMH_SEQUENCE].button_name;
	button = BBoxFindButtonNamed(scrn->mainbuttons, name);
	menu = BBoxMenuOfButton(button);
	XtSetArg(query_args[0], XtNnumChildren, &numChildren);
	XtSetArg(query_args[1], XtNchildren, &children);
	XtGetValues(menu, query_args, (Cardinal) 2);
	n = MenuBoxButtons[XMH_SEQUENCE].num_entries;
	if (strcmp(XtName(children[0]), "menuLabel") == 0)
	    n++;

	/* Erase the current check mark. */

	for (i=(n-1); i < numChildren; i++) 
	    ToggleMenuItem(children[i], False);

	/* Delete any entries which should be deleted. */

	for (i=n; i < numChildren; i++)
	    if (! TocGetSeqNamed(toc, XtName(children[i])))
		XtDestroyWidget(children[i]);

	/* Create any entries which should be created. */
	
	callbacks[0].closure = (XtPointer) scrn;
	for (i=1; i < toc->numsequences; i++) 
	    if (! XtNameToWidget(menu, toc->seqlist[i]->name))
		XtCreateManagedWidget(toc->seqlist[i]->name, smeBSBObjectClass,
				      menu, args, XtNumber(args));

	/* Set the check mark. */

	name = toc->viewedseq->name;
	if ((item = XtNameToWidget(menu, name)) != NULL)
	    ToggleMenuItem(item, True);
    }
    TocSetSelectedSequence(toc, toc->viewedseq);
}


void TUScanFileForToc(Toc toc)
{
    Scrn scrn;
    char  **argv, str[100];
    if (toc) {
	TUGetFullFolderInfo(toc);
	if (toc->num_scrns) scrn = toc->scrn[0];
	else scrn = scrnList[0];

	(void) sprintf(str, "Rescanning %s", toc->foldername);
	ChangeLabel(scrn->toclabel, str);

	argv = MakeArgv(5);
	argv[0] = "scan";
	argv[1] = TocMakeFolderName(toc);
	argv[2] = "-width";
	(void) sprintf(str, "%d", app_resources.toc_width);
	argv[3] = str;
	argv[4] = "-noheader";
	DoCommand(argv, (char *) NULL, toc->scanfile);
	XtFree(argv[1]);
	XtFree((char *) argv);

	toc->needslabelupdate = True;
	toc->validity = valid;
	toc->curmsg = NULL;	/* Get cur msg somehow! %%% */
    }
}



int TUGetMsgPosition(Toc toc, Msg msg)
{
    int msgid, h = 0, l, m;
    char str[100];
    static Boolean ordered = True;
    msgid = msg->msgid;
    if (ordered) {
	l = 0;
	h = toc->nummsgs - 1;
	while (l < h - 1) {
	    m = (l + h) / 2;
	    if (toc->msgs[m]->msgid > msgid)
		h = m;
	    else
		l = m;
	}
	if (toc->msgs[l] == msg) return l;
	if (toc->msgs[h] == msg) return h;
    }
    ordered = False;
    for (l = 0; l < toc->nummsgs; l++) {
	if (msgid == toc->msgs[l]->msgid) return l;
    }
    (void) sprintf(str,
		   "TUGetMsgPosition search failed! hi=%d, lo=%d, msgid=%d",
		   h, l, msgid);
    Punt(str);
    return 0; /* Keep lint happy. */
}


void TUResetTocLabel(Scrn scrn)
{
    char str[500];
    Toc toc;
    if (scrn) {
	toc = scrn->toc;
	if (toc == NULL)
	    (void) strcpy(str, " ");
	else {
	    if (toc->stopupdate) {
		toc->needslabelupdate = TRUE;
		return;
	    }
	    (void) sprintf(str, "%s:%s", toc->foldername,
			   toc->viewedseq->name);
	    toc->needslabelupdate = FALSE;
	}
	ChangeLabel((Widget) scrn->toclabel, str);
    }
}


/* A major toc change has occured; redisplay it.  (This also should work even
   if we now have a new source to display stuff from.) */

void TURedisplayToc(Scrn scrn)
{
    Toc toc;
    Widget source;
    if (scrn != NULL && scrn->tocwidget != NULL) {
	toc = scrn->toc;
 	if (toc) {
	    if (toc->stopupdate) {
		toc->needsrepaint = TRUE;
		return;
	    }
	    XawTextDisableRedisplay(scrn->tocwidget);
	    source = XawTextGetSource(scrn->tocwidget);
	    if (toc->force_reset || source != toc->source) {
		XawTextSetSource(scrn->tocwidget, toc->source,
				 (XawTextPosition) 0);
		toc->force_reset = False; /* %%% temporary */
	    }
	    TocSetCurMsg(toc, TocGetCurMsg(toc));
	    XawTextEnableRedisplay(scrn->tocwidget);
	    TUCheckSequenceMenu(toc);
	    toc->needsrepaint = FALSE;
	} else {
	    XawTextSetSource(scrn->tocwidget, PNullSource, (XawTextPosition) 0);
	}
    }
}


void TULoadSeqLists(Toc toc)
{
    Sequence seq;
    FILEPTR fid;
    char    str[500], *ptr, *ptr2, viewed[500], selected[500];
    int     i;
    if (toc->viewedseq) (void) strcpy(viewed, toc->viewedseq->name);
    else *viewed = 0;
    if (toc->selectseq) (void) strcpy(selected, toc->selectseq->name);
    else *selected = 0;
    for (i = 0; i < toc->numsequences; i++) {
	seq = toc->seqlist[i];
	XtFree((char *) seq->name);
	if (seq->mlist) FreeMsgList(seq->mlist);
	XtFree((char *)seq);
    }
    toc->numsequences = 1;
    toc->seqlist = (Sequence *) XtRealloc((char *) toc->seqlist,
					  (Cardinal) sizeof(Sequence));
    seq = toc->seqlist[0] = XtNew(SequenceRec);
    seq->name = XtNewString("all");
    seq->mlist = NULL;
    toc->viewedseq = seq;
    toc->selectseq = seq;
    (void) sprintf(str, "%s/.mh_sequences", toc->path);
    fid = myfopen(str, "r");
    if (fid) {
	while ((ptr = ReadLine(fid))) {
	    ptr2 = strchr(ptr, ':');
	    if (ptr2) {
		*ptr2 = 0;
		if (strcmp(ptr, "all") != 0 &&
		    strcmp(ptr, "cur") != 0 &&
		    strcmp(ptr, "unseen") != 0) {
		    toc->numsequences++;
		    toc->seqlist = (Sequence *)
			XtRealloc((char *) toc->seqlist, (Cardinal)
				  toc->numsequences * sizeof(Sequence));
		    seq = toc->seqlist[toc->numsequences - 1] =
			XtNew(SequenceRec);
		    seq->name = XtNewString(ptr);
		    seq->mlist = StringToMsgList(toc, ptr2 + 1);
		    if (strcmp(seq->name, viewed) == 0) {
			toc->viewedseq = seq;
			*viewed = 0;
		    }
		    if (strcmp(seq->name, selected) == 0) {
			toc->selectseq = seq;
			*selected = 0;
		    }
		}
	    }
	}
	(void) myfclose(fid);
    }
}



/* Refigure what messages are visible. */

void TURefigureWhatsVisible(Toc toc)
{
    MsgList mlist;
    Msg msg, oldcurmsg;
    int i;
    int	w, changed, newval, msgid;
    Sequence seq = toc->viewedseq;
    mlist = seq->mlist;
    oldcurmsg = toc->curmsg;
    TocSetCurMsg(toc, (Msg)NULL);
    w = 0;
    changed = FALSE;

    for (i = 0; i < toc->nummsgs; i++) {
	msg = toc->msgs[i];
	msgid = msg->msgid;
	while (mlist && mlist->msglist[w] && mlist->msglist[w]->msgid < msgid)
	    w++;
	newval = (!mlist
		  || (mlist->msglist[w] && mlist->msglist[w]->msgid == msgid));
	if (newval != msg->visible) {
	    changed = TRUE;
	    msg->visible = newval;
	}
    }
    if (changed) {
	TURefigureTocPositions(toc);
	if (oldcurmsg) {
	    if (!oldcurmsg->visible) {
		toc->curmsg = TocMsgAfter(toc, oldcurmsg);
		if (toc->curmsg == NULL)
		    toc->curmsg = TocMsgBefore(toc, oldcurmsg);
	    } else toc->curmsg = oldcurmsg;
	}
	for (i=0 ; i<toc->num_scrns ; i++)
	    TURedisplayToc(toc->scrn[i]);
    } else TocSetCurMsg(toc, oldcurmsg);
    for (i=0 ; i<toc->num_scrns ; i++)
	TUResetTocLabel(toc->scrn[i]);
}


/* (Re)load the toc from the scanfile.  If reloading, this makes efforts to
   keep the fates of msgs, and to keep msgs that are being edited.  Note that
   this routine must know of all places that msg ptrs are stored; it expects
   them to be kept only in tocs, in scrns, and in msg sequences. */

#define SeemsIdentical(msg1, msg2) ((msg1)->msgid == (msg2)->msgid &&	      \
				    ((msg1)->temporary || (msg2)->temporary ||\
				     strcmp((msg1)->buf, (msg2)->buf) == 0))

void TULoadTocFile(Toc toc)
{
    int maxmsgs, l, orignummsgs, i, j, origcurmsgid;
    FILEPTR fid;
    XawTextPosition position;
    char *ptr;
    Msg msg, curmsg;
    Msg *origmsgs;
    int bufsiz = app_resources.toc_width + 1;
    static char *buf;

    if (!buf)
	buf = XtMalloc((Cardinal) bufsiz);
    TocStopUpdate(toc);
    toc->lastreaddate = LastModifyDate(toc->scanfile);
    if (toc->curmsg) {
	origcurmsgid = toc->curmsg->msgid;
	TocSetCurMsg(toc, (Msg)NULL);
    } else origcurmsgid = 0;  /* The "default" current msg; 0 means none */
    fid = FOpenAndCheck(toc->scanfile, "r");
    maxmsgs = orignummsgs = toc->nummsgs;
    if (maxmsgs == 0) maxmsgs = 100;
    toc->nummsgs = 0;
    origmsgs = toc->msgs;
    toc->msgs = (Msg *) XtMalloc((Cardinal) maxmsgs * sizeof(Msg));
    position = 0;
    i = 0;
    curmsg = NULL;
    while ((ptr = fgets(buf, bufsiz, fid))) {
	toc->msgs[toc->nummsgs++] = msg = XtNew(MsgRec);
	bzero((char *) msg, sizeof(MsgRec));
	msg->toc = toc;
	msg->position = position;
	msg->length = l = strlen(ptr);
	position += l;
	if (l == app_resources.toc_width && buf[bufsiz-2] != '\n') {
	    buf[bufsiz-2] = '\n';
	    msg->buf = strcpy(XtMalloc((Cardinal) ++l), ptr);
	    msg->msgid = atoi(ptr);
	    do 
		ptr = fgets(buf, bufsiz, fid);
	    while (ptr && (int) strlen(ptr) == app_resources.toc_width
		   && buf[bufsiz-2] != '\n');
	} else {
	    msg->buf = strcpy(XtMalloc((Cardinal) ++l), ptr);
	    msg->msgid = atoi(ptr);
	}
	if (msg->msgid == origcurmsgid)
	    curmsg = msg;
	msg->buf[MARKPOS] = ' ';
	msg->changed = FALSE;
	msg->fate = Fignore;
	msg->desttoc = NULL;
	msg->visible = TRUE;
	if (toc->nummsgs >= maxmsgs) {
	    maxmsgs += 100;
	    toc->msgs = (Msg *) XtRealloc((char *) toc->msgs,
					  (Cardinal) maxmsgs * sizeof(Msg));
	}
	while (i < orignummsgs && origmsgs[i]->msgid < msg->msgid) i++;
	if (i < orignummsgs) {
	    origmsgs[i]->buf[MARKPOS] = ' ';
	    if (SeemsIdentical(origmsgs[i], msg))
		MsgSetFate(msg, origmsgs[i]->fate, origmsgs[i]->desttoc);
	}
    }
    toc->length = toc->origlength = toc->lastPos = position;
    toc->msgs = (Msg *) XtRealloc((char *) toc->msgs,
				  (Cardinal) toc->nummsgs * sizeof(Msg));
    (void) myfclose(fid);
    if ( (toc->source == NULL) && ( toc->num_scrns > 0 ) ) {
        Arg args[1];

	XtSetArg(args[0], XtNtoc, toc);
	toc->source = XtCreateWidget("tocSource", tocSourceWidgetClass,
				     toc->scrn[0]->tocwidget,
				     args, (Cardinal) 1);
    }
    for (i=0 ; i<numScrns ; i++) {
	msg = scrnList[i]->msg;
	if (msg && msg->toc == toc) {
	    for (j=0 ; j<toc->nummsgs ; j++) {
		if (SeemsIdentical(toc->msgs[j], msg)) {
		    msg->position = toc->msgs[j]->position;
		    msg->visible = TRUE;
		    ptr = toc->msgs[j]->buf;
		    l = toc->msgs[j]->length;
		    *(toc->msgs[j]) = *msg;
		    toc->msgs[j]->buf = ptr;
		    toc->msgs[j]->length = l;
		    scrnList[i]->msg = toc->msgs[j];
		    break;
		}
	    }
	    if (j >= toc->nummsgs) {
		msg->temporary = FALSE;	/* Don't try to auto-delete msg. */
		MsgSetScrnForce(msg, (Scrn) NULL);
	    }
	}
    }
    for (i=0 ; i<orignummsgs ; i++)
	MsgFree(origmsgs[i]);
    XtFree((char *)origmsgs);
    TocSetCurMsg(toc, curmsg);
    TULoadSeqLists(toc);
    TocStartUpdate(toc);
}


void TUSaveTocFile(Toc toc)
{
    Msg msg;
    int fid;
    int i;
    XawTextPosition position;
    char c;
    if (toc->stopupdate) {
	toc->needscachesave = TRUE;
	return;
    }
    fid = -1;
    position = 0;
    for (i = 0; i < toc->nummsgs; i++) {
	msg = toc->msgs[i];
	if (fid < 0 && msg->changed) {
	    fid = myopen(toc->scanfile, O_RDWR, 0666);
	    (void) lseek(fid, (long)position, 0);
	}
	if (fid >= 0) {
	    c = msg->buf[MARKPOS];
	    msg->buf[MARKPOS] = ' ';
	    (void) write(fid, msg->buf, msg->length);
	    msg->buf[MARKPOS] = c;
	}
	position += msg->length;
    }
    if (fid < 0 && toc->length != toc->origlength)
	fid = myopen(toc->scanfile, O_RDWR, 0666);
    if (fid >= 0) {
#if defined(SYSV) && (defined(i386) || defined(MOTOROLA))
	(void) ftruncate_emu(fid, toc->length, toc->scanfile);
#else
	(void) ftruncate(fid, toc->length);
	myclose(fid);
#endif
	toc->origlength = toc->length;
    }
    toc->needscachesave = FALSE;
    toc->lastreaddate = LastModifyDate(toc->scanfile);
}


static Boolean UpdateScanFile(
  XtPointer client_data)	/* Toc */
{
    Toc toc = (Toc)client_data;
    int i;

    if (app_resources.block_events_on_busy) ShowBusyCursor();

    TUScanFileForToc(toc);
    TULoadTocFile(toc);

    for (i=0 ; i<toc->num_scrns ; i++)
	TURedisplayToc(toc->scrn[i]);

    if (app_resources.block_events_on_busy) UnshowBusyCursor();

    return True;
}


void TUEnsureScanIsValidAndOpen(Toc toc, Boolean delay)
{
    if (toc) {
	TUGetFullFolderInfo(toc);
	if (TUScanFileOutOfDate(toc)) {
	    if (!delay)
		UpdateScanFile((XtPointer)toc);
	    else {
		/* this is a hack to get the screen mapped before
		 * spawning the subprocess (and blocking).
		 * Need to make sure the scanfile exists at this point.
		 */
		int fid = myopen(toc->scanfile, O_RDWR|O_CREAT, 0666);
		myclose(fid);
		XtAppAddWorkProc(XtWidgetToApplicationContext(toplevel),
				 UpdateScanFile,
				 (XtPointer)toc);
	    }
	}
	if (toc->source == NULL)
	    TULoadTocFile(toc);
    }
}



/* Refigure all the positions, based on which lines are visible. */

void TURefigureTocPositions(Toc toc)
{
    int i;
    Msg msg;
    XawTextPosition position, length;
    position = length = 0;
    for (i=0; i<toc->nummsgs ; i++) {
	msg = toc->msgs[i];
	msg->position = position;
	if (msg->visible) position += msg->length;
	length += msg->length;
    }
    toc->lastPos = position;
    toc->length = length;
}



/* Make sure we've loaded ALL the folder info for this toc, including its
   path and sequence lists. */

void TUGetFullFolderInfo(Toc toc)
{
    char str[500];
    if (! toc->scanfile) {
	if (! toc->path) {
	    /* usually preset by TocFolderExists */
	    (void) sprintf(str, "%s/%s", app_resources.mail_path,
			   toc->foldername);
	    toc->path = XtNewString(str);
	}
	(void) sprintf(str, "%s/.xmhcache", toc->path);
	toc->scanfile = XtNewString(str);
	toc->lastreaddate = LastModifyDate(toc->scanfile);
	if (TUScanFileOutOfDate(toc))
	    toc->validity = invalid;
	else {
	    toc->validity = valid;
	    TULoadTocFile(toc);
	}
    }
}

/* Append a message to the end of the toc.  It has the given scan line.  This
   routine will figure out the message number, and change the scan line
   accordingly. */

Msg TUAppendToc(Toc toc, char *ptr)
{
    Msg msg;
    int msgid;

    TUGetFullFolderInfo(toc);
    if (toc->validity != valid)
	return NULL;
	    
    if (toc->nummsgs > 0)
	msgid = toc->msgs[toc->nummsgs - 1]->msgid + 1;
    else
	msgid = 1;
    (toc->nummsgs)++;
    toc->msgs = (Msg *) XtRealloc((char *) toc->msgs,
				  (Cardinal) toc->nummsgs * sizeof(Msg));
    toc->msgs[toc->nummsgs - 1] = msg = XtNew(MsgRec);
    bzero((char *) msg, (int) sizeof(MsgRec));
    msg->toc = toc;
    msg->buf = XtNewString(ptr);
    if (msgid >= 10000)
	msgid %= 10000;
    (void)sprintf(msg->buf, "%4d", msgid);
    msg->buf[MARKPOS] = ' ';
    msg->msgid = msgid;
    msg->position = toc->lastPos;
    msg->length = strlen(ptr);
    msg->changed = TRUE;
    msg->fate = Fignore;
    msg->desttoc = NULL;
    if (toc->viewedseq == toc->seqlist[0]) {
	msg->visible = TRUE;
	toc->lastPos += msg->length;
    }
    else
	msg->visible = FALSE;
    toc->length += msg->length;
    if ( (msg->visible) && (toc->source != NULL) )
	TSourceInvalid(toc, msg->position, msg->length);
    TUSaveTocFile(toc);
    return msg;
}
