/* $XConsortium: toc.c,v 2.59 95/01/09 16:52:53 swick Exp $
 * $XFree86: xc/programs/xmh/toc.c,v 3.5 2002/04/05 21:06:29 dickey Exp $
 *
 *
 *			  COPYRIGHT 1987
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

/* toc.c -- handle things in the toc widget. */

#include "xmh.h"
#include "tocintrnl.h"
#include "toc.h"
#include "tocutil.h"
#include "actions.h"

#include <sys/stat.h>

static int IsDir(char *name)
{
    char str[500];
    struct stat buf;
    if (*name == '.')
	return FALSE;
    (void) sprintf(str, "%s/%s", app_resources.mail_path, name);
    if (stat(str, &buf) /* failed */) return False;
#ifdef S_ISDIR
    return S_ISDIR(buf.st_mode);
#else
    return (buf.st_mode & S_IFMT) == S_IFDIR;
#endif
}


static void MakeSureFolderExists(
    char ***namelistptr,
    int *numfoldersptr,
    char *name)
{
    int i;
    char str[200];
    for (i=0 ; i<*numfoldersptr ; i++)
	if (strcmp((*namelistptr)[i], name) == 0) return;
    (void) sprintf(str, "%s/%s", app_resources.mail_path, name);
    (void) mkdir(str, 0700);
    *numfoldersptr = ScanDir(app_resources.mail_path, namelistptr, IsDir);
    for (i=0 ; i<*numfoldersptr ; i++)
	if (strcmp((*namelistptr)[i], name) == 0) return;
    Punt("Can't create new mail folder!");
}


static void MakeSureSubfolderExists(
    char ***		namelistptr,
    int *		numfoldersptr,
    char *		name)
{
    char folder[300];
    char subfolder_path[300];
    char *subfolder;
    struct stat buf;

    /* Make sure that the parent folder exists */

    subfolder = strchr( strcpy(folder, name), '/');
    *subfolder = '\0';
    subfolder++;
    MakeSureFolderExists(namelistptr, numfoldersptr, folder);
	
    /* The parent folder exists.  Make sure the subfolder exists. */

    (void) sprintf(subfolder_path, "%s/%s", app_resources.mail_path, name);
    if (stat(subfolder_path, &buf) /* failed */) {
	(void) mkdir(subfolder_path, 0700);
	if (stat(subfolder_path, &buf) /* failed */)
	    Punt("Can't create new xmh subfolder!");
    }
#ifdef S_ISDIR
    if (!S_ISDIR(buf.st_mode))
#else
    if ((buf.st_mode & S_IFMT) != S_IFDIR)
#endif
	Punt("Can't create new xmh subfolder!");
}

int TocFolderExists(Toc toc)
{
    struct stat buf;
    if (! toc->path) {
	char str[500];
	(void) sprintf(str, "%s/%s", app_resources.mail_path, toc->foldername);
	toc->path = XtNewString(str);
    }
    return ((stat(toc->path, &buf) == 0) &&
#ifdef S_ISDIR
	    (S_ISDIR(buf.st_mode)));
#else
	    ((buf.st_mode & S_IFMT) == S_IFDIR));
#endif
}

static void LoadCheckFiles(void)
{
    FILE *fid;
    char str[1024];

    (void) sprintf(str, "%s/.xmhcheck", homeDir);
    fid = myfopen(str, "r");
    if (fid) {
	int i;
	char *ptr, *ptr2;

	while ((ptr = ReadLine(fid))) {
	    while (*ptr == ' ' || *ptr == '\t') ptr++;
	    ptr2 = ptr;
	    while (*ptr2 && *ptr2 != ' ' && *ptr2 != '\t') ptr2++;
	    if (*ptr2 == 0) continue;
	    *ptr2++ = 0;
	    while (*ptr2 == ' ' || *ptr2 == '\t') ptr2++;
	    if (*ptr2 == 0) continue;
	    for (i=0 ; i<numFolders ; i++) {
		if (strcmp(ptr, folderList[i]->foldername) == 0) {
		    folderList[i]->incfile = XtNewString(ptr2);
		    break;
		}
	    }
	}
	myfclose(fid);
    } else if ( app_resources.initial_inc_file &&
	       *app_resources.initial_inc_file)
	InitialFolder->incfile = app_resources.initial_inc_file;
}
	    

/*	PUBLIC ROUTINES 	*/


/* Read in the list of folders. */

void TocInit(void)
{
    Toc toc;
    char **namelist;
    int i;
    numFolders = ScanDir(app_resources.mail_path, &namelist, IsDir);
    if (numFolders < 0) {
	(void) mkdir(app_resources.mail_path, 0700);
	numFolders = ScanDir(app_resources.mail_path, &namelist, IsDir);
	if (numFolders < 0)
	    Punt("Can't create or read mail directory!");
    }
    if (IsSubfolder(app_resources.initial_folder_name))
	MakeSureSubfolderExists(&namelist, &numFolders,
				app_resources.initial_folder_name);
    else
	MakeSureFolderExists(&namelist, &numFolders,
			     app_resources.initial_folder_name);

    if (IsSubfolder(app_resources.drafts_folder_name))
	MakeSureSubfolderExists(&namelist, &numFolders,
				app_resources.drafts_folder_name);
    else
	MakeSureFolderExists(&namelist, &numFolders,
			     app_resources.drafts_folder_name);
    folderList = (Toc *) XtMalloc((Cardinal)numFolders * sizeof(Toc));
    for (i=0 ; i<numFolders ; i++) {
	toc = folderList[i] = TUMalloc();
	toc->foldername = XtNewString(namelist[i]);
	free((char *)namelist[i]);
    }
    if (! (InitialFolder = TocGetNamed(app_resources.initial_folder_name)))
	InitialFolder = TocCreate(app_resources.initial_folder_name);

    if (! (DraftsFolder = TocGetNamed(app_resources.drafts_folder_name)))
	DraftsFolder = TocCreate(app_resources.drafts_folder_name);
    free((char *)namelist);
    LoadCheckFiles();
}



/* Create a toc and add a folder to the folderList.  */

Toc TocCreate(char *foldername)
{
    Toc		toc = TUMalloc();

    toc->foldername = XtNewString(foldername);
    folderList = (Toc *) XtRealloc((char *) folderList,
				   (unsigned) ++numFolders * sizeof(Toc));
    folderList[numFolders - 1] = toc;
    return toc;
}


/* Create a new folder with the given name. */

Toc TocCreateFolder(char *foldername)
{
    Toc toc;
    char str[500];
    if (TocGetNamed(foldername)) return NULL;
    (void) sprintf(str, "%s/%s", app_resources.mail_path, foldername);
    if (mkdir(str, 0700) < 0) return NULL;
    toc = TocCreate(foldername);
    return toc;
}

int TocHasMail(Toc toc)
{
    return toc->mailpending;
}

static int CheckForNewMail(Toc toc)
{
    if (toc->incfile)
	return (GetFileLength(toc->incfile) > 0);
    else if (toc == InitialFolder) {
	char **argv;
	char *result;
	int hasmail;

	argv = MakeArgv(4);
	argv[0] = "msgchk";
	argv[1] = "-nonotify";
	argv[2] = "nomail";
	argv[3] = "-nodate";
	result = DoCommandToString(argv);
	hasmail = (*result != '\0');
	XtFree(result);
	XtFree((char*)argv);
	return hasmail;
    }
    return False;
}

/*ARGSUSED*/
void TocCheckForNewMail(
    Boolean update)	/* if True, actually make the check */
{
    Toc toc;
    Scrn scrn;
    int i, j, hasmail;
    Boolean mail_waiting = False;

    if (update) {
	for (i=0 ; i<numFolders ; i++) {
	    toc = folderList[i];
	    if (TocCanIncorporate(toc)) {
		toc->mailpending = hasmail = CheckForNewMail(toc);
		if (hasmail) mail_waiting = True;
		for (j=0 ; j<numScrns ; j++) {
		    scrn = scrnList[j];
		    if (scrn->kind == STtocAndView)
			/* give visual indication of new mail waiting */
			BBoxMailFlag(scrn->folderbuttons, TocName(toc),
				     hasmail);
		}
	    }
	}
    } else {
	for (i=0; i < numFolders; i++) {
	    toc = folderList[i];
	    if (toc->mailpending) {
		mail_waiting = True;
		break;
	    }
	}
    }

    if (app_resources.mail_waiting_flag) {
	Arg args[1];
	static Boolean icon_state = -1;

	if (icon_state != mail_waiting) {
	    icon_state = mail_waiting;
	    for (i=0; i < numScrns; i++) {
		scrn = scrnList[i];
		if (scrn->kind == STtocAndView) {
		    XtSetArg(args[0], XtNiconPixmap,
			     (mail_waiting ? app_resources.new_mail_icon
			                   : app_resources.no_mail_icon));
		    XtSetValues(scrn->parent, args, (Cardinal)1);
		}
	    }
	}
    }
}

/* Intended to support mutual exclusion on deleting folders, so that you
 * cannot have two confirm popups at the same time on the same folder.
 *
 * You can have confirm popups on different folders simultaneously.
 * However, I did not protect the user from popping up a delete confirm
 * popup on folder A, then popping up a delete confirm popup on folder
 * A/subA, then deleting A, then deleting A/subA -- which of course is 
 * already gone, and will cause xmh to Punt.
 *
 * TocClearDeletePending is a callback from the No confirmation button
 * of the confirm popup.
 */

Boolean TocTestAndSetDeletePending(Toc toc)
{
    Boolean flag;

    flag = toc->delete_pending;
    toc->delete_pending = True;
    return flag;
}

void TocClearDeletePending(Toc toc)
{
    toc->delete_pending = False;
}


/* Recursively delete an entire directory.  Nasty. */

static void NukeDirectory(char *path)
{
    struct stat buf;

#ifdef S_IFLNK
    /* POSIX.1 does not discuss symbolic links. */
    if (lstat(path, &buf) /* failed */)
	return;
    if ((buf.st_mode & S_IFMT) == S_IFLNK) {
	(void) unlink(path);
	return;
    }
#endif
    if (stat(path, &buf) /* failed */)
	return;
    if (buf.st_mode & S_IWRITE) {
	char **argv = MakeArgv(3);
	argv[0] = "/bin/rm";
	argv[1] = "-rf";
	argv[2] = path;
	(void) DoCommand(argv, (char*)NULL, (char*)NULL);
	XtFree((char*)argv);
    } 
}


/* Destroy the given folder. */

void TocDeleteFolder(Toc toc)
{
    Toc toc2;
    int i, j, w;
    if (toc == NULL) return;
    TUGetFullFolderInfo(toc);

    w = -1;
    for (i=0 ; i<numFolders ; i++) {
	toc2 = folderList[i];
	if (toc2 == toc)
	    w = i;
	else if (toc2->validity == valid)
	    for (j=0 ; j<toc2->nummsgs ; j++)
		if (toc2->msgs[j]->desttoc == toc)
		    MsgSetFate(toc2->msgs[j], Fignore, (Toc) NULL);
    }
    if (w < 0) Punt("Couldn't find it in TocDeleteFolder!");
    NukeDirectory(toc->path);
    if (toc->validity == valid) {
	for (i=0 ; i<toc->nummsgs ; i++) {
	    MsgSetScrnForce(toc->msgs[i], (Scrn) NULL);
	    MsgFree(toc->msgs[i]);
	}
	XtFree((char *) toc->msgs);
    }
    XtFree((char *)toc);
    numFolders--;
    for (i=w ; i<numFolders ; i++) folderList[i] = folderList[i+1];
}


/*
 * Display the given toc in the given scrn.  If scrn is NULL, then remove the
 * toc from all scrns displaying it.
 */

void TocSetScrn(Toc toc, Scrn scrn)
{
    Cardinal i;

    if (toc == NULL && scrn == NULL) return;
    if (scrn == NULL) {
	for (i=0 ; i<toc->num_scrns ; i++)
	    TocSetScrn((Toc) NULL, toc->scrn[i]);
	return;
    }
    if (scrn->toc == toc) return;
    if (scrn->toc != NULL) {
	for (i=0 ; i<scrn->toc->num_scrns ; i++)
	    if (scrn->toc->scrn[i] == scrn) break;
	if (i >= scrn->toc->num_scrns)
	    Punt("Couldn't find scrn in TocSetScrn!");
	scrn->toc->scrn[i] = scrn->toc->scrn[--scrn->toc->num_scrns];
    }
    scrn->toc = toc;
    if (toc == NULL) {
	TUResetTocLabel(scrn);
	TURedisplayToc(scrn);
	StoreWindowName(scrn, progName);
    } else {
	toc->num_scrns++;
	toc->scrn = (Scrn *) XtRealloc((char *) toc->scrn,
				       (unsigned)toc->num_scrns*sizeof(Scrn));
	toc->scrn[toc->num_scrns - 1] = scrn;
	TUEnsureScanIsValidAndOpen(toc, True);
	TUResetTocLabel(scrn);
	if (app_resources.prefix_wm_and_icon_name) {
	    char wm_name[64];
	    int length = strlen(progName);
	    (void) strncpy(wm_name, progName, length);
	    (void) strncpy(wm_name + length , ": ", 2);
	    (void) strcpy(wm_name + length + 2, toc->foldername);
	    StoreWindowName(scrn, wm_name);
	}
	else
	    StoreWindowName(scrn, toc->foldername);
	TURedisplayToc(scrn);
	SetCurrentFolderName(scrn, toc->foldername);
    }
    EnableProperButtons(scrn);
}



/* Remove the given message from the toc.  Doesn't actually touch the file.
   Also note that it does not free the storage for the msg. */

void TocRemoveMsg(Toc toc, Msg msg)
{
    Msg newcurmsg;
    MsgList mlist;
    int i;
    if (toc->validity == unknown)
	TUGetFullFolderInfo(toc);
    if (toc->validity != valid)
	return;
    newcurmsg = TocMsgAfter(toc, msg);
    if (newcurmsg) newcurmsg->changed = TRUE;
    newcurmsg = toc->curmsg;
    if (msg == toc->curmsg) {
	newcurmsg = TocMsgAfter(toc, msg);
	if (newcurmsg == NULL) newcurmsg = TocMsgBefore(toc, msg);
	toc->curmsg = NULL;
    }
    toc->length -= msg->length;
    if (msg->visible) toc->lastPos -= msg->length;
    for(i = TUGetMsgPosition(toc, msg), toc->nummsgs--; i<toc->nummsgs ; i++) {
	toc->msgs[i] = toc->msgs[i+1];
	if (msg->visible) toc->msgs[i]->position -= msg->length;
    }
    for (i=0 ; i<toc->numsequences ; i++) {
	mlist = toc->seqlist[i]->mlist;
	if (mlist) DeleteMsgFromMsgList(mlist, msg);
    }

    if (msg->visible && toc->num_scrns > 0 && !toc->needsrepaint)
	TSourceInvalid(toc, msg->position, -msg->length);
    TocSetCurMsg(toc, newcurmsg);
    TUSaveTocFile(toc);
}
    


void TocRecheckValidity(Toc toc)
{
    Cardinal i;

    if (toc && toc->validity == valid && TUScanFileOutOfDate(toc)) {
	if (app_resources.block_events_on_busy) ShowBusyCursor();

	TUScanFileForToc(toc);
	if (toc->source)
	    TULoadTocFile(toc);
	for (i=0 ; i<toc->num_scrns ; i++)
	    TURedisplayToc(toc->scrn[i]);

	if (app_resources.block_events_on_busy) UnshowBusyCursor();
    }
}


/* Set the current message. */

void TocSetCurMsg(Toc toc, Msg msg)
{
    Msg msg2;
    Cardinal i;

    if (toc->validity != valid) return;
    if (msg != toc->curmsg) {
	msg2 = toc->curmsg;
	toc->curmsg = msg;
	if (msg2)
	    MsgSetFate(msg2, msg2->fate, msg2->desttoc);
    }
    if (msg) {
	MsgSetFate(msg, msg->fate, msg->desttoc);
	if (toc->num_scrns) {
	    if (toc->stopupdate)
		toc->needsrepaint = TRUE;
	    else {
		for (i=0 ; i<toc->num_scrns ; i++)
		    XawTextSetInsertionPoint(toc->scrn[i]->tocwidget,
						msg->position);
	    }
	}
    }
}


/* Return the current message. */

Msg TocGetCurMsg(Toc toc)
{
    return toc->curmsg;
}




/* Return the message after the given one.  (If none, return NULL.) */

Msg TocMsgAfter(Toc toc, Msg msg)
{
    int i;
    i = TUGetMsgPosition(toc, msg);
    do {
	i++;
	if (i >= toc->nummsgs)
	    return NULL;
    } while (!(toc->msgs[i]->visible));
    return toc->msgs[i];
}



/* Return the message before the given one.  (If none, return NULL.) */

Msg TocMsgBefore(Toc toc, Msg msg)
{
    int i;
    i = TUGetMsgPosition(toc, msg);
    do {
	i--;
	if (i < 0)
	    return NULL;
    } while (!(toc->msgs[i]->visible));
    return toc->msgs[i];
}



/* The caller KNOWS the toc's information is out of date; rescan it. */

void TocForceRescan(Toc toc)
{
    register Cardinal i;

    if (toc->num_scrns) {
	toc->viewedseq = toc->seqlist[0];
	for (i=0 ; i<toc->num_scrns ; i++)
	    TUResetTocLabel(toc->scrn[i]);
	TUScanFileForToc(toc);
	TULoadTocFile(toc);
	for (i=0 ; i<toc->num_scrns ; i++)
	    TURedisplayToc(toc->scrn[i]);
    } else {
	TUGetFullFolderInfo(toc);
	(void) unlink(toc->scanfile);
	toc->validity = invalid;
    }
}



/* The caller has just changed a sequence list.  Reread them from mh. */

void TocReloadSeqLists(Toc toc)
{
    Cardinal i;

    TocSetCacheValid(toc);
    TULoadSeqLists(toc);
    TURefigureWhatsVisible(toc);
    for (i=0 ; i<toc->num_scrns ; i++) {
	TUResetTocLabel(toc->scrn[i]);
	EnableProperButtons(toc->scrn[i]);
    }
}


/*ARGSUSED*/
void XmhReloadSeqLists(
    Widget	w,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    Scrn scrn = ScrnFromWidget(w);
    TocReloadSeqLists(scrn->toc);
    TUCheckSequenceMenu(scrn->toc);
}



/* Return TRUE if the toc has an interesting sequence. */

int TocHasSequences(Toc toc)
{
    return toc && toc->numsequences > 1;
}


/* Change which sequence is being viewed. */

void TocChangeViewedSeq(Toc toc, Sequence seq)
{
    if (seq == NULL) seq = toc->viewedseq;
    toc->viewedseq = seq;
    toc->force_reset = True; /* %%% force Text source to be reset */
    TURefigureWhatsVisible(toc);
}


/* Return the sequence with the given name in the given toc. */

Sequence TocGetSeqNamed(Toc toc, char *name)
{
    register int i;
    if (name == NULL)
	return (Sequence) NULL;

    for (i=0 ; i<toc->numsequences ; i++)
	if (strcmp(toc->seqlist[i]->name, name) == 0)
	    return toc->seqlist[i];
    return (Sequence) NULL;
}


/* Return the sequence currently being viewed in the toc. */

Sequence TocViewedSequence(Toc toc)
{
    return toc->viewedseq;
}


/* Set the selected sequence in the toc */

void TocSetSelectedSequence(
    Toc		toc,
    Sequence	sequence)
{
    if (toc) 
	toc->selectseq = sequence;
}


/* Return the sequence currently selected */

Sequence TocSelectedSequence(Toc toc)
{
    if (toc) return (toc->selectseq);
    else return (Sequence) NULL;
}


/* Return the list of messages currently selected. */

#define SrcScan XawTextSourceScan

MsgList TocCurMsgList(Toc toc)
{
    MsgList result;
    XawTextPosition pos1, pos2;

    if (toc->num_scrns == 0) return NULL;
    result = MakeNullMsgList();
    XawTextGetSelectionPos( toc->scrn[0]->tocwidget, &pos1, &pos2); /* %%% */
    if (pos1 < pos2) {
	pos1 = SrcScan(toc->source, pos1, XawstEOL, XawsdLeft, 1, FALSE);
	pos2 = SrcScan(toc->source, pos2, XawstPositions, XawsdLeft, 1, TRUE);
	pos2 = SrcScan(toc->source, pos2, XawstEOL, XawsdRight, 1, FALSE);
	while (pos1 < pos2) {
	    AppendMsgList(result, MsgFromPosition(toc, pos1, XawsdRight));
	    pos1 = SrcScan(toc->source, pos1, XawstEOL, XawsdRight, 1, TRUE);
	}
    }
    return result;
}



/* Unset the current selection. */

void TocUnsetSelection(Toc toc)
{
    if (toc->source)
        XawTextUnsetSelection(toc->scrn[0]->tocwidget);
}



/* Create a brand new, blank message. */

Msg TocMakeNewMsg(Toc toc)
{
    Msg msg;
    static int looping = False;
    TUEnsureScanIsValidAndOpen(toc, False);
    msg = TUAppendToc(toc, "####  empty\n");
    if (FileExists(MsgFileName(msg))) {
	if (looping++) Punt( "Cannot correct scan file" );
        DEBUG2("**** FOLDER %s WAS INVALID; msg %d already existed!\n",
	       toc->foldername, msg->msgid);
	TocForceRescan(toc);
	return TocMakeNewMsg(toc); /* Try again.  Using recursion here is ugly,
				      but what the hack ... */
    }
    CopyFileAndCheck("/dev/null", MsgFileName(msg));
    looping = False;
    return msg;
}


/* Set things to not update cache or display until further notice. */

void TocStopUpdate(Toc toc)
{
    Cardinal i;

    for (i=0 ; i<toc->num_scrns ; i++)
	XawTextDisableRedisplay(toc->scrn[i]->tocwidget);
    toc->stopupdate++;
}


/* Start updating again, and do whatever updating has been queued. */

void TocStartUpdate(Toc toc)
{
    Cardinal i;

    if (toc->stopupdate && --(toc->stopupdate) == 0) {
	for (i=0 ; i<toc->num_scrns ; i++) {
	    if (toc->needsrepaint) 
		TURedisplayToc(toc->scrn[i]);
	    if (toc->needslabelupdate)
		TUResetTocLabel(toc->scrn[i]);
	}
	if (toc->needscachesave)
	    TUSaveTocFile(toc);
    }
    for (i=0 ; i<toc->num_scrns ; i++)
	XawTextEnableRedisplay(toc->scrn[i]->tocwidget);
}



/* Something has happened that could later convince us that our cache is out
   of date.  Make this not happen; our cache really *is* up-to-date. */

void TocSetCacheValid(Toc toc)
{
    TUSaveTocFile(toc);
}


/* Return the full folder pathname of the given toc, prefixed w/'+' */

char *TocMakeFolderName(Toc toc)
{
    char* name = XtMalloc((Cardinal) (strlen(toc->path) + 2) );
    (void)sprintf( name, "+%s", toc->path );
    return name;
}

char *TocName(Toc toc)
{
    return toc->foldername;
}



/* Given a foldername, return the corresponding toc. */

Toc TocGetNamed(char *name)
{
    int i;
    for (i=0; i<numFolders ; i++)
	if (strcmp(folderList[i]->foldername, name) == 0) return folderList[i];
    return NULL;
}


Boolean TocHasChanges(Toc toc)
{
    int i;
    for (i=0 ; i<toc->nummsgs ; i++)
	if (toc->msgs[i]->fate != Fignore) return True;

    return False;
}



/* Throw out all changes to this toc, and close all views of msgs in it.
   Requires confirmation by the user. */

/*ARGSUSED*/
static void TocCataclysmOkay(
    Widget	widget,		/* unused */
    XtPointer	client_data,
    XtPointer	call_data)	/* unused */
{
    Toc			toc = (Toc) client_data;
    register int	i;

    for (i=0; i < toc->nummsgs; i++)
	MsgSetFate(toc->msgs[i], Fignore, (Toc)NULL);

/* Doesn't make sense to have this MsgSetScrn for loop here. dmc. %%% */
    for (i=0; i < toc->nummsgs; i++)
	MsgSetScrn(toc->msgs[i], (Scrn) NULL, (XtCallbackList) NULL, 
		   (XtCallbackList) NULL);
}
	
int TocConfirmCataclysm(
    Toc			toc,
    XtCallbackList	confirms,
    XtCallbackList	cancels)
{	
    register int	i;

    static XtCallbackRec yes_callbacks[] = {
	{TocCataclysmOkay,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    if (! toc)
	return 0;

    if (TocHasChanges(toc)) {
	char		str[300];
	Widget		tocwidget;

	(void)sprintf(str,"Are you sure you want to remove all changes to %s?",
		      toc->foldername);
	yes_callbacks[0].closure = (XtPointer) toc;
	yes_callbacks[1].callback = confirms[0].callback;
	yes_callbacks[1].closure = confirms[0].closure;

	tocwidget = NULL;
	for (i=0; i < toc->num_scrns; i++)
	    if (toc->scrn[i]->mapped) {
		tocwidget = toc->scrn[i]->tocwidget;
		break;
	    }

	PopupConfirm(tocwidget, str, yes_callbacks, cancels);
	return NEEDS_CONFIRMATION;
    }
    else {
/* Doesn't make sense to have this MsgSetFate for loop here. dmc. %%% */
	for (i=0 ; i<toc->nummsgs ; i++)
	    MsgSetFate(toc->msgs[i], Fignore, (Toc)NULL);

	for (i=0 ; i<toc->nummsgs ; i++)
	    if (MsgSetScrn(toc->msgs[i], (Scrn) NULL, confirms, cancels))
		return NEEDS_CONFIRMATION;
	return 0;
    }
}
    

/* Commit all the changes in this toc; all messages will meet their 'fate'. */

/*ARGSUSED*/
void TocCommitChanges(
    Widget	widget,		/* unused */
    XtPointer	client_data,	
    XtPointer	call_data)	/* unused */
{
    Toc toc = (Toc) client_data;
    Msg msg;
    int i, cur = 0;
    char str[100], **argv = NULL;
    FateType curfate, fate; 
    Toc desttoc;
    Toc curdesttoc = NULL;
    XtCallbackRec	confirms[2];

    confirms[0].callback = TocCommitChanges;
    confirms[0].closure = (XtPointer) toc;
    confirms[1].callback = (XtCallbackProc) NULL;
    confirms[1].closure = (XtPointer) NULL;

    if (toc == NULL) return;
    for (i=0 ; i<toc->nummsgs ; i++) {
	msg = toc->msgs[i];
	fate = MsgGetFate(msg, (Toc *)NULL);
	if (fate != Fignore && fate != Fcopy)
	    if (MsgSetScrn(msg, (Scrn) NULL, confirms, (XtCallbackList) NULL)
		== NEEDS_CONFIRMATION)
	        return;
    }
    XFlush(XtDisplay(toc->scrn[0]->parent));
    for (i=0 ; i<numFolders ; i++)
	TocStopUpdate(folderList[i]);
    toc->haschanged = TRUE;
    if (app_resources.block_events_on_busy) ShowBusyCursor();

    do {
	curfate = Fignore;
	i = 0;
	while (i < toc->nummsgs) {
	    msg = toc->msgs[i];
	    fate = MsgGetFate(msg, &desttoc);
	    if (curfate == Fignore && fate != Fignore) {
		curfate = fate;
		argv = MakeArgv(2);
		switch (curfate) {
		  case Fdelete:
		    argv[0] = XtNewString("rmm");
		    argv[1] = TocMakeFolderName(toc);
		    cur = 2;
		    curdesttoc = NULL;
		    break;
		  case Fmove:
		  case Fcopy:
		    argv[0] = XtNewString("refile");
		    cur = 1;
		    curdesttoc = desttoc;
		    break;
		  default:
		    break;
		}
	    }
	    if (curfate != Fignore &&
		  curfate == fate && desttoc == curdesttoc) {
		argv = ResizeArgv(argv, cur + 1);
		(void) sprintf(str, "%d", MsgGetId(msg));
		argv[cur++] = XtNewString(str);
		MsgSetFate(msg, Fignore, (Toc)NULL);
		if (curdesttoc) {
		    (void) TUAppendToc(curdesttoc, MsgGetScanLine(msg));
		    curdesttoc->haschanged = TRUE;
		}
		if (curfate != Fcopy) {
		    TocRemoveMsg(toc, msg);
		    MsgFree(msg);
		    i--;
		}
		if (cur > 40)
		    break;	/* Do only 40 at a time, just to be safe. */
	    } 
	    i++;
	}
	if (curfate != Fignore) {
	    switch (curfate) {
	      case Fmove:
	      case Fcopy:
		argv = ResizeArgv(argv, cur + 4);
		argv[cur++] = XtNewString(curfate == Fmove ? "-nolink"
				       			   : "-link");
		argv[cur++] = XtNewString("-src");
		argv[cur++] = TocMakeFolderName(toc);
		argv[cur++] = TocMakeFolderName(curdesttoc);
		break;
	      default:
		break;
	    }
	    if (app_resources.debug) {
		for (i = 0; i < cur; i++)
		    (void) fprintf(stderr, "%s ", argv[i]);
		(void) fprintf(stderr, "\n");
		(void) fflush(stderr);
	    }
	    DoCommand(argv, (char *) NULL, (char *) NULL);
	    for (i = 0; argv[i]; i++)
		XtFree((char *) argv[i]);
	    XtFree((char *) argv);
	}
    } while (curfate != Fignore);
    for (i=0 ; i<numFolders ; i++) {
	if (folderList[i]->haschanged) {
	    TocReloadSeqLists(folderList[i]);
	    folderList[i]->haschanged = FALSE;
	}
	TocStartUpdate(folderList[i]);
    }

    if (app_resources.block_events_on_busy) UnshowBusyCursor();
}



/* Return whether the given toc can incorporate mail. */

int TocCanIncorporate(Toc toc)
{
    return (toc && (toc == InitialFolder || toc->incfile));
}


/* Incorporate new messages into the given toc. */

int TocIncorporate(Toc toc)
{
    char **argv;
    char str[100], *file, *ptr;
    Msg msg, firstmessage = NULL;
    FILEPTR fid;

    argv = MakeArgv(toc->incfile ? 7 : 4);
    argv[0] = "inc";
    argv[1] = TocMakeFolderName(toc);
    argv[2] = "-width";
    (void) sprintf(str, "%d", app_resources.toc_width);
    argv[3] = str;
    if (toc->incfile) {
	argv[4] = "-file";
	argv[5] = toc->incfile;
	argv[6] = "-truncate";
    }
    if (app_resources.block_events_on_busy) ShowBusyCursor();

    file = DoCommandToFile(argv);
    XtFree(argv[1]);
    XtFree((char *)argv);
    TUGetFullFolderInfo(toc);
    if (toc->validity == valid) {
	fid = FOpenAndCheck(file, "r");
	TocStopUpdate(toc);
	while ((ptr = ReadLineWithCR(fid))) {
	    if (atoi(ptr) > 0) {
		msg = TUAppendToc(toc, ptr);
		if (firstmessage == NULL) firstmessage = msg;
	    }
	}
	if (firstmessage && firstmessage->visible) {
	    TocSetCurMsg(toc, firstmessage);
	}
	TocStartUpdate(toc);
	myfclose(fid);
    }
    DeleteFileAndCheck(file);

    if (app_resources.block_events_on_busy) UnshowBusyCursor();

    toc->mailpending = False;
    return (firstmessage != NULL);
}


/* The given message has changed.  Rescan it and change the scanfile. */

void TocMsgChanged(Toc toc, Msg msg)
{
    char **argv, str[100], str2[10], *ptr;
    int length, delta;
    int i;
    FateType fate;
    Toc desttoc;

    if (toc->validity != valid) return;
    fate = MsgGetFate(msg, &desttoc);
    MsgSetFate(msg, Fignore, (Toc) NULL);
    argv = MakeArgv(6);
    argv[0] = "scan";
    argv[1] = TocMakeFolderName(toc);
    (void) sprintf(str, "%d", msg->msgid);
    argv[2] = str;
    argv[3] = "-width";
    (void) sprintf(str2, "%d", app_resources.toc_width);
    argv[4] = str2;
    argv[5] = "-noheader";
    ptr = DoCommandToString(argv);
    XtFree(argv[1]);
    XtFree((char *) argv);
    if (strcmp(ptr, msg->buf) != 0) {
	length = strlen(ptr);
	delta = length - msg->length;
	XtFree(msg->buf);
	msg->buf = ptr;
	msg->length = length;
	toc->length += delta;
	if (msg->visible) {
	    if (delta != 0) {
		for (i=TUGetMsgPosition(toc, msg)+1; i<toc->nummsgs ; i++)
		    toc->msgs[i]->position += delta;
		toc->lastPos += delta;
	    }
	    for (i=0 ; i<toc->num_scrns ; i++)
		TURedisplayToc(toc->scrn[i]);
	}
	MsgSetFate(msg, fate, desttoc);
	TUSaveTocFile(toc);
    } else XtFree(ptr);
}



Msg TocMsgFromId(Toc toc, int msgid)
{
    int h, l, m;
    l = 0;
    h = toc->nummsgs - 1;
    if (h < 0) {
	if (app_resources.debug) {
	    char str[100];
	    (void)sprintf(str, "Toc is empty! folder=%s\n", toc->foldername);
	    DEBUG( str )
	}
	return NULL;
    }
    while (l < h - 1) {
	m = (l + h) / 2;
	if (toc->msgs[m]->msgid > msgid)
	    h = m;
	else
	    l = m;
    }
    if (toc->msgs[l]->msgid == msgid) return toc->msgs[l];
    if (toc->msgs[h]->msgid == msgid) return toc->msgs[h];
    if (app_resources.debug) {
	char str[100];
	(void) sprintf(str,
		      "TocMsgFromId search failed! hi=%d, lo=%d, msgid=%d\n",
		      h, l, msgid);
	DEBUG( str )
    }
    return NULL;
}

/* Sequence names are put on a stack which is specific to the folder. 
 * Sequence names are very volatile, so we make our own copies of the strings.
 */

/*ARGSUSED*/
void XmhPushSequence(
    Widget	w,
    XEvent	*event,
    String	*params,
    Cardinal	*count)
{
    Scrn	scrn = ScrnFromWidget(w);
    Toc		toc;
    Cardinal	i;

    if (! (toc = scrn->toc)) return;
    
    if (*count == 0) {
	if (toc->selectseq)
	    Push(&toc->sequence_stack, XtNewString(toc->selectseq->name));
    }
    else
	for (i=0; i < *count; i++) 
	    Push(&toc->sequence_stack, XtNewString(params[i]));
}


/*ARGSUSED*/
void XmhPopSequence(
    Widget	w,		/* any widget on the screen of interest */
    XEvent	*event,
    String	*params,
    Cardinal	*count)
{
    Scrn	scrn = ScrnFromWidget(w);
    char	*seqname;
    Widget	sequenceMenu, selected, original;
    Button	button;
    Sequence	sequence;

    if ((seqname = Pop(&scrn->toc->sequence_stack)) != NULL) {

	button = BBoxFindButtonNamed(scrn->mainbuttons,
				     MenuBoxButtons[XMH_SEQUENCE].button_name);
	sequenceMenu = BBoxMenuOfButton(button);

	if ((selected = XawSimpleMenuGetActiveEntry(sequenceMenu)))
	    ToggleMenuItem(selected, False);

	if ((original = XtNameToWidget(sequenceMenu, seqname))) {
	    ToggleMenuItem(original, True);
	    sequence = TocGetSeqNamed(scrn->toc, seqname);
	    TocSetSelectedSequence(scrn->toc, sequence);
	}
	XtFree(seqname);
    }
}
