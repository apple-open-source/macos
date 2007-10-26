/*
 * $XConsortium: folder.c,v 2.44 94/08/29 20:25:49 swick Exp $
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
/* $XFree86: xc/programs/xmh/folder.c,v 1.3 2001/10/28 03:34:38 tsi Exp $ */

/* folder.c -- implement buttons relating to folders and other globals. */

#include "xmh.h"
#include <X11/Xmu/CharSet.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xatom.h>
#include <sys/stat.h>
#include <ctype.h>
#include "bboxint.h"
#include "tocintrnl.h"
#include "actions.h"

typedef struct {	/* client data structure for callbacks */
    Scrn	scrn;		/* the xmh scrn of action */
    Toc		toc;		/* the toc of the selected folder */
    Toc		original_toc;	/* the toc of the current folder */
} DeleteDataRec, *DeleteData;


static void CheckAndDeleteFolder(XMH_CB_ARGS);
static void CancelDeleteFolder(XMH_CB_ARGS);
static void CheckAndConfirmDeleteFolder(XMH_CB_ARGS);
static void FreeMenuData(XMH_CB_ARGS);
static void CreateFolderMenu(Button);
static void AddFolderMenuEntry(Button, char *);
static void DeleteFolderMenuEntry(Button, char *);

#ifdef DEBUG_CLEANUP
extern Boolean ExitLoop;
#endif

/* Close this toc&view scrn.  If this is the last toc&view, quit xmh. */

/*ARGSUSED*/
void DoClose(
    Widget	widget,
    XtPointer	client_data,
    XtPointer	call_data)
{
    Scrn	scrn = (Scrn) client_data;
    register int i, count;
    Toc		toc;
    XtCallbackRec	confirm_callbacks[2];

    count = 0;
    for (i=0 ; i<numScrns ; i++)
	if (scrnList[i]->kind == STtocAndView && scrnList[i]->mapped)
	    count++;

    confirm_callbacks[0].callback = (XtCallbackProc) DoClose;
    confirm_callbacks[0].closure = (XtPointer) scrn;
    confirm_callbacks[1].callback = (XtCallbackProc) NULL;
    confirm_callbacks[1].closure = (XtPointer) NULL;

    if (count <= 1) {

	for (i = numScrns - 1; i >= 0; i--)
	    if (scrnList[i] != scrn) {
		if (MsgSetScrn((Msg) NULL, scrnList[i], confirm_callbacks,
			       (XtCallbackList) NULL) == NEEDS_CONFIRMATION)
		    return;
	    }
	for (i = 0; i < numFolders; i++) {
	    toc = folderList[i];

	    if (TocConfirmCataclysm(toc, confirm_callbacks,
				    (XtCallbackList) NULL))
		return;
	}
/* 	if (MsgSetScrn((Msg) NULL, scrn))
 *	    return;
 * %%%
 *	for (i = 0; i < numFolders; i++) {
 *	    toc = folderList[i];
 *	    if (toc->scanfile && toc->curmsg)
 *		CmdSetSequence(toc, "cur", MakeSingleMsgList(toc->curmsg));
 *	}
 */

#ifdef DEBUG_CLEANUP
	XtDestroyWidget(scrn->parent);
	ExitLoop = TRUE;
	return;
#else
	XtVaSetValues(scrn->parent, XtNjoinSession, (XtArgVal)False, NULL);
	XtUnmapWidget(scrn->parent);
	XtDestroyApplicationContext
	    (XtWidgetToApplicationContext(scrn->parent));
	exit(0);
#endif
    }
    else {
	if (MsgSetScrn((Msg) NULL, scrn, confirm_callbacks, 
		       (XtCallbackList) NULL) == NEEDS_CONFIRMATION)
	    return;
	DestroyScrn(scrn);	/* doesn't destroy first toc&view scrn */
    }
}

/*ARGSUSED*/
void XmhClose(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*params,	/* unused */
    Cardinal	*num_params)	/* unused */
{
    Scrn 	scrn = ScrnFromWidget(w);
    DoClose(w, (XtPointer) scrn, (XtPointer) NULL);
}

/* Open the selected folder in this screen. */

/* ARGSUSED*/
void DoOpenFolder(
    Widget	widget,
    XtPointer	client_data,
    XtPointer	call_data)
{
    /* Invoked by the Folder menu entry "Open Folder"'s notify action. */

    Scrn	scrn = (Scrn) client_data;
    Toc		toc  = SelectedToc(scrn);
    if (TocFolderExists(toc))
	TocSetScrn(toc, scrn);
    else
	PopupError(scrn->parent, "Cannot open selected folder.");
}


/*ARGSUSED*/
void XmhOpenFolder(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*params,
    Cardinal	*num_params)
{
    Scrn	scrn = ScrnFromWidget(w);

    /* This action may be invoked from folder menu buttons or from folder
     * menus, as an action procedure on an event specified in translations.
     * In this case, the action will open a folder only if that folder
     * was actually selected from a folder button or menu.  If the folder
     * was selected from a folder menu, the menu entry callback procedure,
     * which changes the selected folder, and is invoked by the "notify" 
     * action, must have already executed; and the menu entry "unhightlight"
     * action must execute after this action.
     *
     * This action does not execute if invoked as an accelerator whose
     * source widget is a menu button or a folder menu.  However, it 
     * may be invoked as a keyboard accelerator of any widget other than
     * the folder menu buttons or the folder menus.  In that case, it will
     * open the currently selected folder.
     *
     * If given a parameter, it will take it as the name of a folder to
     * select and open.
     */

    if (! UserWantsAction(w, scrn)) return;
    if (*num_params) SetCurrentFolderName(scrn, params[0]);
    DoOpenFolder(w, (XtPointer) scrn, (XtPointer) NULL);
}


/* Compose a new message. */

/*ARGSUSED*/
void DoComposeMessage(
    Widget	widget,
    XtPointer	client_data,
    XtPointer	call_data)
{
    Scrn        scrn = NewCompScrn();
    Msg		msg = TocMakeNewMsg(DraftsFolder);
    MsgLoadComposition(msg);
    MsgSetTemporary(msg);
    MsgSetReapable(msg);
    MsgSetScrnForComp(msg, scrn);
    MapScrn(scrn);
}

   
/*ARGSUSED*/
void XmhComposeMessage(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*params,	/* unused */
    Cardinal	*num_params)	/* unused */
{
    DoComposeMessage(w, (XtPointer) NULL, (XtPointer) NULL);
}


/* Make a new scrn displaying the given folder. */

/*ARGSUSED*/
void DoOpenFolderInNewWindow(
    Widget	widget,
    XtPointer	client_data,
    XtPointer	call_data)
{
    Scrn	scrn = (Scrn) client_data;
    Toc 	toc = SelectedToc(scrn);
    if (TocFolderExists(toc)) {
	scrn = CreateNewScrn(STtocAndView);
	TocSetScrn(toc, scrn);
	MapScrn(scrn);
    } else 
	PopupError(scrn->parent, "Cannot open selected folder.");
}


/*ARGSUSED*/
void XmhOpenFolderInNewWindow(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*params,	/* unused */
    Cardinal	*num_params)	/* unused */
{
    Scrn scrn = ScrnFromWidget(w);
    DoOpenFolderInNewWindow(w, (XtPointer) scrn, (XtPointer) NULL);
}


/* Create a new folder with the given name. */

static char *previous_label = NULL;
/*ARGSUSED*/
static void CreateFolder(
    Widget	widget,		/* the okay button of the dialog widget */
    XtPointer	client_data,	/* the dialog widget */
    XtPointer	call_data)
{
    Toc		toc;
    register int i;
    char	*name;
    Widget	dialog = (Widget) client_data;
    Arg		args[3];
    char 	str[300], *label;

    name = XawDialogGetValueString(dialog);
    for (i=0 ; name[i] > ' ' ; i++) ;
    name[i] = '\0';
    toc = TocGetNamed(name);
    if ((toc) || (i==0) || (name[0]=='/') || ((toc = TocCreateFolder(name))
					      == NULL)) {
	if (toc) 
	    (void) sprintf(str, "Folder \"%s\" already exists.  Try again.",
			   name);
	else if (name[0]=='/')
	    (void) sprintf(str, "Please specify folders relative to \"%s\".",
			   app_resources.mail_path);
	else 
	    (void) sprintf(str, "Cannot create folder \"%s\".  Try again.",
			   name);
	label = XtNewString(str);
	XtSetArg(args[0], XtNlabel, label);
	XtSetArg(args[1], XtNvalue, "");
	XtSetValues(dialog, args, TWO);
	if (previous_label)
	    XtFree(previous_label);
	previous_label = label;
	return;
    }
    for (i = 0; i < numScrns; i++)
	if (scrnList[i]->folderbuttons) {
	    char	*c;
	    Button	button;
	    if ((c = strchr(name, '/'))) { /* if is subfolder */
		c[0] = '\0';
		button = BBoxFindButtonNamed(scrnList[i]->folderbuttons,
					     name);
		c[0] = '/';
		if (button) AddFolderMenuEntry(button, name);
	    }
	    else
		BBoxAddButton(scrnList[i]->folderbuttons, name,
			      menuButtonWidgetClass, True);
	}
    DestroyPopup(widget, (XtPointer) XtParent(dialog), (XtPointer) NULL);
}


/* Create a new folder.  Requires the user to name the new folder. */

/*ARGSUSED*/
void DoCreateFolder(
    Widget	widget,		/* unused */
    XtPointer	client_data,
    XtPointer	call_data)	/* unused */
{
    Scrn scrn = (Scrn) client_data;
    PopupPrompt(scrn->parent, "Create folder named:", CreateFolder);
}


/*ARGSUSED*/
void XmhCreateFolder(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*params,	/* unused */
    Cardinal	*num_params)	/* unused */
{
    Scrn scrn = ScrnFromWidget(w);
    DoCreateFolder(w, (XtPointer)scrn, (XtPointer)NULL);
}


/*ARGSUSED*/
static void CancelDeleteFolder(
    Widget	widget,		/* unused */
    XtPointer	client_data,
    XtPointer	call_data)	/* unused */
{
    DeleteData	deleteData = (DeleteData) client_data;

    TocClearDeletePending(deleteData->toc);

    /* When the delete request is made, the toc currently being viewed is
     * changed if necessary to be the toc under consideration for deletion.
     * Once deletion has been confirmed or cancelled, we revert to display
     * the toc originally under view, unless the toc originally under
     * view has been deleted.
     */

    if (deleteData->original_toc != NULL)
	TocSetScrn(deleteData->original_toc, deleteData->scrn);
    XtFree((char *) deleteData);
}


/*ARGSUSED*/
static void CheckAndConfirmDeleteFolder(
    Widget	widget,		/* unreliable; sometimes NULL */
    XtPointer	client_data,	/* data structure */
    XtPointer	call_data)	/* unused */
{
    DeleteData  deleteData = (DeleteData) client_data;
    Scrn	scrn = deleteData->scrn;
    Toc		toc  = deleteData->toc;
    char	str[300];
    XtCallbackRec confirms[2];
    XtCallbackRec cancels[2];

    static XtCallbackRec yes_callbacks[] = {
	{CheckAndDeleteFolder,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    static XtCallbackRec no_callbacks[] = {
	{CancelDeleteFolder,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    /* Display the toc of the folder to be deleted. */

    TocSetScrn(toc, scrn);

    /* Check for pending delete, copy, move, or edits on messages in the
     * folder to be deleted, and ask for confirmation if they are found.
     */

    confirms[0].callback = (XtCallbackProc) CheckAndConfirmDeleteFolder;
    confirms[0].closure = client_data;
    confirms[1].callback = (XtCallbackProc) NULL;
    confirms[1].closure = (XtPointer) NULL;
    
    cancels[0].callback = (XtCallbackProc) CancelDeleteFolder;
    cancels[0].closure = client_data;
    cancels[1].callback = (XtCallbackProc) NULL;
    cancels[1].closure = (XtPointer) NULL;

    if (TocConfirmCataclysm(toc, confirms, cancels) ==	NEEDS_CONFIRMATION)
	return;

    /* Ask the user for confirmation on destroying the folder. */

    yes_callbacks[0].closure = client_data;
    no_callbacks[0].closure =  client_data;
    (void) sprintf(str, "Are you sure you want to destroy %s?", TocName(toc));
    PopupConfirm(scrn->tocwidget, str, yes_callbacks, no_callbacks);
}


/*ARGSUSED*/
static void CheckAndDeleteFolder(
    Widget	widget,		/* unused */
    XtPointer	client_data,	/* data structure */
    XtPointer	call_data)	/* unused */
{
    DeleteData  deleteData = (DeleteData) client_data;
    Scrn	scrn = deleteData->scrn;
    Toc		toc =  deleteData->toc;
    XtCallbackRec confirms[2];
    XtCallbackRec cancels[2];
    int 	i;
    char	*foldername;
    
    /* Check for changes occurring after the popup was first presented. */

    confirms[0].callback = (XtCallbackProc) CheckAndConfirmDeleteFolder;
    confirms[0].closure = client_data;
    confirms[1].callback = (XtCallbackProc) NULL;
    confirms[1].closure = (XtPointer) NULL;
    
    cancels[0].callback = (XtCallbackProc) CancelDeleteFolder;
    cancels[0].closure = client_data;
    cancels[1].callback = (XtCallbackProc) NULL;
    cancels[1].closure = (XtPointer) NULL;
    
    if (TocConfirmCataclysm(toc, confirms, cancels) == NEEDS_CONFIRMATION)
	return;

    /* Delete.  Restore the previously viewed toc, if it wasn't deleted. */

    foldername = TocName(toc);
    TocSetScrn(toc, (Scrn) NULL);
    TocDeleteFolder(toc);
    for (i=0 ; i<numScrns ; i++)
	if (scrnList[i]->folderbuttons) {

	    if (IsSubfolder(foldername)) {
		char parent_folder[300];
		char *c = strchr( strcpy(parent_folder, foldername), '/');
		*c = '\0';

/* Since menus are built upon demand, and are a per-xmh-screen resource, 
 * not all xmh toc & view screens will have the same menus built.
 * So the menu entry deletion routines must be able to handle a button
 * whose menu field is null.  It would be better to share folder menus
 * between xmh screens, but accelerators call action procedures which depend
 * upon being able to get the xmh screen (Scrn) from the widget argument.
 */

		DeleteFolderMenuEntry
		    ( BBoxFindButtonNamed( scrnList[i]->folderbuttons,
					  parent_folder), 
		     foldername);
	    }
	    else {
		BBoxDeleteButton
		    (BBoxFindButtonNamed( scrnList[i]->folderbuttons,
					 foldername));
	    }

	    /* If we've deleted the current folder, show the Initial Folder */

	    if ((! strcmp(scrnList[i]->curfolder, foldername)) 
		&& (BBoxNumButtons(scrnList[i]->folderbuttons))
		&& (strcmp(foldername, app_resources.initial_folder_name)))
		TocSetScrn(InitialFolder, scrnList[i]);
	}
    XtFree(foldername);
    if (deleteData->original_toc != NULL) 
	TocSetScrn(deleteData->original_toc, scrn);
    XtFree((char *) deleteData);
}


/* Delete the selected folder.  Requires confirmation! */

/*ARGSUSED*/
void DoDeleteFolder(
    Widget	w,
    XtPointer	client_data,
    XtPointer	call_data)
{
    Scrn	scrn = (Scrn) client_data;
    Toc		toc  = SelectedToc(scrn);
    DeleteData	deleteData;

    if (! TocFolderExists(toc)) {
	/* Too hard to clean up xmh when the folder doesn't exist anymore. */
	PopupError(scrn->parent,
		   "Cannot open selected folder for confirmation to delete.");
	return;
    }

    /* Prevent more than one confirmation popup on the same folder. 
     * TestAndSet returns true if there is a delete pending on this folder.
     */
    if (TocTestAndSetDeletePending(toc))	{
	PopupError(scrn->parent, "There is a delete pending on this folder.");
	return;
    }

    deleteData = XtNew(DeleteDataRec);
    deleteData->scrn = scrn;
    deleteData->toc = toc;
    deleteData->original_toc = CurrentToc(scrn);
    if (deleteData->original_toc == toc)
	deleteData->original_toc = (Toc) NULL;

    CheckAndConfirmDeleteFolder(w, (XtPointer) deleteData, (XtPointer) NULL);
}


/*ARGSUSED*/
void XmhDeleteFolder(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*params,	/* unused */
    Cardinal	*num_params)	/* unused */
{
    Scrn	scrn = ScrnFromWidget(w);
    DoDeleteFolder(w, (XtPointer) scrn, (XtPointer) NULL);
}


/*-----	Notes on MenuButtons as folder buttons ---------------------------
 *
 * I assume that the name of the button is identical to the name of the folder.
 * Only top-level folders have buttons.
 * Only top-level folders may have subfolders.
 * Top-level folders and their subfolders may have messages.
 *
 */

static char filename[500];	/* for IsFolder() and for callback */
static int  flen = 0;		/* length of a substring of filename */


/* Function name:	IsFolder
 * Description:		determines if a file is an mh subfolder.
 */
static int IsFolder(char *name)
{
    register int i, len;
    struct stat buf;

    /* mh does not like subfolder names to be strings of digits */

    if (isdigit(name[0]) || name[0] == '#') {
	len = strlen(name);
	for(i=1; i < len && isdigit(name[i]); i++)
	    ;
	if (i == len) return FALSE;
    }
    else if (name[0] == '.')
	return FALSE;

    (void) sprintf(filename + flen, "/%s", name);
    if (stat(filename, &buf) /* failed */) return False;
#ifdef S_ISDIR
    return S_ISDIR(buf.st_mode);
#else
    return (buf.st_mode & S_IFMT) == S_IFDIR;
#endif
}


/* menu entry selection callback for folder menus. */

/*ARGSUSED*/
static void DoSelectFolder(
    Widget 	w,		/* the menu entry object */
    XtPointer	closure,	/* foldername */
    XtPointer	data)	
{
    Scrn	scrn = ScrnFromWidget(w);
    SetCurrentFolderName(scrn, (char *) closure);
}

/*ARGSUSED*/
static void FreeMenuData(
    Widget	w,
    XtPointer	client_data,
    XtPointer	call_data)
{
    XtFree((char*) client_data);
}

/* Function name:	AddFolderMenuEntry
 * Description:	
 *	Add an entry to a menu.  If the menu is not already created,
 *	create it, including the (already existing) new subfolder directory.
 * 	If the menu is already created,	add the new entry.
 */

static void AddFolderMenuEntry(
    Button	button,		/* the corresponding menu button */
    char	*entryname)	/* the new entry, relative to MailDir */
{
    Arg		args[4];
    char *	name;
    char *	c;
    char        tmpname[300];
    char *	label;
    static XtCallbackRec callbacks[] = {
	{ DoSelectFolder,		(XtPointer) NULL },
	{ (XtCallbackProc) NULL,	(XtPointer) NULL}
    };
    static XtCallbackRec destroyCallbacks[] = {
	{ FreeMenuData,			(XtPointer) NULL },
	{ (XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    /* The menu must be created before we can add an entry to it. */

    if (button->menu == NULL || button->menu == NoMenuForButton) {
	CreateFolderMenu(button);
	return;
    }
    name = XtNewString(entryname);
    callbacks[0].closure = (XtPointer) name;
    destroyCallbacks[0].closure = (XtPointer) name;
    XtSetArg(args[0], XtNcallback, callbacks);			/* ONE */
    XtSetArg(args[1], XtNdestroyCallback, destroyCallbacks);	/* TWO */

    /* When a subfolder and its parent folder have identical names,
     * the widget name of the subfolder's menu entry must be unique.
     */
    label = entryname;
    c = strchr( strcpy(tmpname, entryname), '/');
    if (c) {
	*c = '\0';
	label = ++c;
	if (strcmp(tmpname, c) == 0) {
	    c--;
	    *c = '_';
	}
	name = c;
    }
    XtSetArg(args[2], XtNlabel, label);				/* THREE */
    XtCreateManagedWidget(name, smeBSBObjectClass, button->menu, 
			  args, THREE);
}



/* Function name:	CreateFolderMenu
 * Description:	
 *	Menus are created for folder buttons if the folder has at least one
 *	subfolder.  For the directory given by the concatentation of 
 *	app_resources.mail_path, '/', and the name of the button, 
 *	CreateFolderMenu creates the menu whose entries are
 *	the subdirectories which do not begin with '.' and do not have
 *	names which are all digits, and do not have names which are a '#'
 *	followed by all digits.  The first entry is always the name of the
 *	parent folder.  Remaining entries are alphabetized.
 */

static void CreateFolderMenu(
    Button	button)
{
    char **namelist;
    register int i, n, length;
    char	directory[500];

    n = strlen(app_resources.mail_path);
    (void) strncpy(directory, app_resources.mail_path, n);
    directory[n++] = '/';
    (void) strcpy(directory + n, button->name);
    flen = strlen(directory);		/* for IsFolder */
    (void) strcpy(filename, directory);	/* for IsFolder */
    n = ScanDir(directory, &namelist, IsFolder);
    if (n <= 0) {
	/* no subfolders, therefore no menu */
	button->menu = NoMenuForButton;
	return;
    }

    button->menu = XtCreatePopupShell("menu", simpleMenuWidgetClass,
				      button->widget, (ArgList) NULL, ZERO);
	
    /* The first entry is always the parent folder */

    AddFolderMenuEntry(button, button->name);

    /* Build the menu by adding all the current entries to the new menu. */

    length = strlen(button->name);
    (void) strncpy(directory, button->name, length);
    directory[length++] = '/';
    for (i=0; i < n; i++) {
	(void) strcpy(directory + length, namelist[i]);
	free((char *) namelist[i]);
	AddFolderMenuEntry(button, directory);
    }
    free((char *) namelist);
}


/* Function:	DeleteFolderMenuEntry
 * Description:	Remove a subfolder from a menu.
 */

static void DeleteFolderMenuEntry(
    Button	button,
    char	*foldername)
{
    char *	c;
    Arg		args[2];
    char *	subfolder;
    int		n;
    char	tmpname[300];
    Widget	entry;
    
    if (button == NULL || button->menu == NULL) return;
    XtSetArg(args[0], XtNnumChildren, &n);
    XtSetArg(args[1], XtNlabel, &c);
    XtGetValues(button->menu, args, TWO);
    if ((n <= 3 && c) || n <= 2) {
	XtDestroyWidget(button->menu);	
	button->menu = NoMenuForButton;
	return;
    }

    c = strchr( strcpy(tmpname, foldername), '/');
    if (c) {
	*c = '\0';
	subfolder = ++c;
	if (strcmp(button->name, subfolder) == 0) {
	    c--;
	    *c = '_';
	    subfolder = c;
	}
	if ((entry = XtNameToWidget(button->menu, subfolder)) != NULL)
	    XtDestroyWidget(entry);
    }
}

/* Function Name:	PopupFolderMenu
 * Description:		This action should alwas be taken when the user
 *	selects a folder button.  A folder button represents a folder 
 *	and zero or more subfolders.  The menu of subfolders is built upon
 *	the first reference to it, by this routine.  If there are no 
 *	subfolders, this routine will mark the folder as having no 
 *	subfolders, and no menu will be built.  In that case, the menu
 *	button emulates a command button.  When subfolders exist,
 *	the menu will popup, using the menu button action PopupMenu.
 */

/*ARGSUSED*/
void XmhPopupFolderMenu(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*vector,	/* unused */
    Cardinal	*count)		/* unused */
{
    Button	button;
    Scrn	scrn;

    scrn = ScrnFromWidget(w);
    if ((button = BBoxFindButton(scrn->folderbuttons, w)) == NULL)
	return;
    if (button->menu == NULL)
	CreateFolderMenu(button);

    if (button->menu == NoMenuForButton)
	LastMenuButtonPressed = w;
    else {
	XtCallActionProc(button->widget, "PopupMenu", (XEvent *) NULL,
			 (String *) NULL, (Cardinal) 0);
	XtCallActionProc(button->widget, "reset", (XEvent *) NULL,
			 (String *) NULL, (Cardinal) 0);
    }
}


/* Function Name:	XmhSetCurrentFolder
 * Description:		This action procedure allows menu buttons to 
 *	emulate toggle widgets in their function of folder selection.
 *	Therefore, mh folders with no subfolders can be represented
 * 	by a button instead of a menu with one entry.  Sets the currently
 *	selected folder.
 */

/*ARGSUSED*/
void XmhSetCurrentFolder(
    Widget	w,
    XEvent	*event,		/* unused */
    String	*vector,	/* unused */
    Cardinal	*count)		/* unused */
{
    Button	button;
    Scrn	scrn;

    /* The MenuButton widget has a button grab currently active; the
     * currently selected folder will be updated if the user has released
     * the mouse button while the mouse pointer was on the same menu button
     * widget that orginally activated the button grab.  This mechanism is
     * insured by the XmhPopupFolderMenu action setting LastMenuButtonPressed.
     * The action XmhLeaveFolderButton, and it's translation in the application
     * defaults file, bound to LeaveWindow events, insures that the menu
     * button behaves properly when the user moves the pointer out of the 
     * menu button window.
     *
     * This action is for menu button widgets only.
     */

    if (w != LastMenuButtonPressed)
	return;
    scrn = ScrnFromWidget(w);
    if ((button = BBoxFindButton(scrn->folderbuttons, w)) == NULL)
	return;
    SetCurrentFolderName(scrn, button->name);
}


/*ARGSUSED*/
void XmhLeaveFolderButton(
    Widget	w,
    XEvent	*event,
    String	*vector,
    Cardinal	*count)
{
    LastMenuButtonPressed = NULL;
}


void Push(
    Stack	*stack_ptr,
    char 	*data)
{
    Stack	new = XtNew(StackRec);
    new->data = data;
    new->next = *stack_ptr;
    *stack_ptr = new;
}

char * Pop(
    Stack	*stack_ptr)
{
    Stack	top;
    char 	*data = NULL;

    if ((top = *stack_ptr) != NULL) {
	data = top->data;
	*stack_ptr = top->next;
	XtFree((char *) top);
    }
    return data;
}

/* Parameters are taken as names of folders to be pushed on the stack.
 * With no parameters, the currently selected folder is pushed.
 */

/*ARGSUSED*/
void XmhPushFolder(
    Widget	w,
    XEvent 	*event,
    String 	*params,
    Cardinal 	*count)
{
    Scrn	scrn = ScrnFromWidget(w);
    Cardinal	i;

    for (i=0; i < *count; i++) 
	Push(&scrn->folder_stack, params[i]);

    if (*count == 0 && scrn->curfolder)
	Push(&scrn->folder_stack, scrn->curfolder);
}

/* Pop the stack & take that folder to be the currently selected folder. */

/*ARGSUSED*/
void XmhPopFolder(
    Widget	w,
    XEvent 	*event,
    String 	*params,
    Cardinal 	*count)
{
    Scrn	scrn = ScrnFromWidget(w);
    char	*folder;

    if ((folder = Pop(&scrn->folder_stack)) != NULL)
	SetCurrentFolderName(scrn, folder);
}

static Boolean InParams(
    String str,
    String *p,
    Cardinal n)
{
    Cardinal i;
    for (i=0; i < n; p++, i++)
	if (! XmuCompareISOLatin1(*p, str)) return True;
    return False;
}

/* generalized routine for xmh participation in WM protocols */

/*ARGSUSED*/
void XmhWMProtocols(
    Widget	w,	/* NULL if from checkpoint timer */
    XEvent *	event,	/* NULL if from checkpoint timer */
    String *	params,
    Cardinal *	num_params)
{
    Boolean	dw = False;	/* will we do delete window? */
    Boolean	sy = False;	/* will we do save yourself? */
    static char*WM_DELETE_WINDOW = "WM_DELETE_WINDOW";
    static char*WM_SAVE_YOURSELF = "WM_SAVE_YOURSELF";

#define DO_DELETE_WINDOW InParams(WM_DELETE_WINDOW, params, *num_params)
#define DO_SAVE_YOURSELF InParams(WM_SAVE_YOURSELF, params, *num_params)

    /* Respond to a recognized WM protocol request iff 
     * event type is ClientMessage and no parameters are passed, or
     * event type is ClientMessage and event data is matched to parameters, or
     * event type isn't ClientMessage and parameters make a request.
     */

    if (event && event->type == ClientMessage) {
	if (event->xclient.message_type == wm_protocols) {
	    if (event->xclient.data.l[0] == wm_delete_window &&
		(*num_params == 0 || DO_DELETE_WINDOW))
		dw = True;
	    else if (event->xclient.data.l[0] == wm_save_yourself &&
		     (*num_params == 0 || DO_SAVE_YOURSELF))
		sy = True;
	}
    } else {
	if (DO_DELETE_WINDOW)
	    dw = True;
	if (DO_SAVE_YOURSELF)
	    sy = True;
    }

#undef DO_DELETE_WINDOW
#undef DO_SAVE_YOURSELF

    if (sy) {
	register int i;
	for (i=0; i<numScrns; i++)
	    if (scrnList[i]->msg) 
		MsgCheckPoint(scrnList[i]->msg);
	if (w) /* don't generate a property notify via the checkpoint timer */
	    XChangeProperty(XtDisplay(toplevel), XtWindow(toplevel),
			    XA_WM_COMMAND, XA_STRING, 8, PropModeAppend,
			    (unsigned char *)"", 0);
    }
    if (dw && w) {
	Scrn scrn;

	while (w && !XtIsShell(w))
	    w = XtParent(w);
	if (XtIsTransientShell(w)) {
	    WMDeletePopup(w, event);
	    return;
	}
	scrn = ScrnFromWidget(w);
	switch (scrn->kind) {
	  case STtocAndView:
	    DoClose(w, (XtPointer)scrn, (XtPointer)NULL);
	    break;
	  case STview:
	  case STcomp:
	    DoCloseView(w, (XtPointer)scrn, (XtPointer)NULL);
	    break;
	  case STpick:
	    DestroyScrn(scrn);
	    break;
	}
    }
}


typedef struct _InteractMsgTokenRec {
    Scrn			scrn;
    XtCheckpointToken		cp_token;
} InteractMsgTokenRec, *InteractMsgToken;

static void CommitMsgChanges(
    Widget	w,		/* unused */
    XtPointer   client_data,	/* InteractMsgToken */
    XtPointer	call_data)
{
    Cardinal zero = 0;
    InteractMsgToken iToken = (InteractMsgToken) client_data;

    XmhSave(iToken->scrn->parent, (XEvent*)NULL, (String*)NULL, &zero);

    if (MsgChanged(iToken->scrn->msg))
	iToken->cp_token->save_success = False;

    XtSessionReturnToken(iToken->cp_token);
    XtFree((XtPointer)iToken);
}

static void CancelMsgChanges(
    Widget	w,		/* unused */
    XtPointer   client_data,	/* InteractMsgToken */
    XtPointer	call_data)
{
    InteractMsgToken iToken = (InteractMsgToken) client_data;

    /* don't change any msg state now; this is only a checkpoint
     * and the session might be continuing. */

    MsgCheckPoint(iToken->scrn->msg);

    XtSessionReturnToken(iToken->cp_token);
    XtFree((XtPointer)iToken);
}

static void CommitMsgInteract(
    Widget	w,		/* unused */
    XtPointer   client_data,	/* Scrn */
    XtPointer	call_data)	/* XtCheckpointToken */
{
    Scrn		scrn = (Scrn) client_data;
    XtCheckpointToken	cpToken = (XtCheckpointToken) call_data;
    char		str[300];
    InteractMsgToken	iToken;
    static XtCallbackRec yes_callbacks[] = {
	{CommitMsgChanges,		(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    static XtCallbackRec no_callbacks[] = {
	{CancelMsgChanges,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    if (cpToken->interact_style != SmInteractStyleAny
	|| cpToken->cancel_shutdown) {
	XtSessionReturnToken(cpToken);
	return;
    }

    iToken = XtNew(InteractMsgTokenRec);

    iToken->scrn = scrn;
    iToken->cp_token = cpToken;

    yes_callbacks[0].closure = no_callbacks[0].closure = (XtPointer) iToken;

    (void)sprintf(str,"Save changes to message %s?", MsgName(scrn->msg));

    /* %%% should add cancel button */
    PopupConfirm(scrn->parent, str, yes_callbacks, no_callbacks);
}


typedef struct _InteractTocTokenRec {
    Toc				toc;
    XtCheckpointToken		cp_token;
} InteractTocTokenRec, *InteractTocToken;

static void CommitTocChanges(
    Widget	w,		/* unused */
    XtPointer   client_data,	/* InteractTocToken */
    XtPointer	call_data)
{
    InteractTocToken iToken = (InteractTocToken) client_data;

    TocCommitChanges(w, (XtPointer) iToken->toc, (XtPointer) NULL);

    XtSessionReturnToken(iToken->cp_token);
    XtFree((XtPointer)iToken);
}

static void CancelTocChanges(
    Widget	w,		/* unused */
    XtPointer   client_data,	/* InteractTocToken */
    XtPointer	call_data)
{
    InteractTocToken iToken = (InteractTocToken) client_data;

    /* don't change any folder or msg state now; this is only
     * a checkpoint and the session might be continuing. */

    XtSessionReturnToken(iToken->cp_token);
    XtFree((XtPointer)iToken);
}

static void CommitTocInteract(
    Widget	w,		/* unused */
    XtPointer   client_data,	/* Toc */
    XtPointer	call_data)	/* XtCheckpointToken */
{
    Toc			toc = (Toc) client_data;
    XtCheckpointToken	cpToken = (XtCheckpointToken) call_data;
    char		str[300];
    Widget		tocwidget;
    Cardinal		i;
    InteractTocToken	iToken;
    static XtCallbackRec yes_callbacks[] = {
	{CommitTocChanges,		(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    static XtCallbackRec no_callbacks[] = {
	{CancelTocChanges,	(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    if (cpToken->interact_style != SmInteractStyleAny
	|| cpToken->cancel_shutdown) {
	XtSessionReturnToken(cpToken);
	return;
    }

    iToken = XtNew(InteractTocTokenRec);

    iToken->toc = toc;
    iToken->cp_token = cpToken;

    yes_callbacks[0].closure = no_callbacks[0].closure = (XtPointer) iToken;

    (void)sprintf(str,"Commit all changes to %s folder?", toc->foldername);

    tocwidget = NULL;
    for (i=0; i < toc->num_scrns; i++)
	if (toc->scrn[i]->mapped) {
	    tocwidget = toc->scrn[i]->tocwidget;
	    break;
	}

    /* %%% should add cancel button */
    PopupConfirm(tocwidget, str, yes_callbacks, no_callbacks);
}

/* Callback for Session Manager SaveYourself */

/*ARGSUSED*/
void DoSaveYourself(
    Widget	w,		/* unused; s/b toplevel */
    XtPointer	client_data,	/* unused */
    XtPointer	call_data)	/* XtCheckpointToken */
{
    XtCheckpointToken cpToken = (XtCheckpointToken)call_data;

    { /* confirm any uncommitted msg changes */
	int i;
	for (i=0 ; i<numScrns ; i++) {
	    if (MsgChanged(scrnList[i]->msg)) {
		if (cpToken->interact_style == SmInteractStyleAny)
		    XtAddCallback(toplevel, XtNinteractCallback,
				  CommitMsgInteract, (XtPointer)scrnList[i]);
		else {
		    Cardinal zero = 0;
		    XmhSave(scrnList[i]->parent, (XEvent*)NULL,
			    (String*)NULL, &zero);
		    if (MsgChanged(scrnList[i]->msg)) {
			MsgCheckPoint(scrnList[i]->msg);
			cpToken->save_success = False;
		    }
		}
	    }
	}
    }

    { /* confirm any uncommitted folder changes */
	int i;
	for (i = 0; i < numFolders; i++) {
	    if (TocHasChanges(folderList[i])) {
		if (cpToken->interact_style == SmInteractStyleAny)
		    XtAddCallback(toplevel, XtNinteractCallback,
				  CommitTocInteract, (XtPointer)folderList[i]);
		else
		    TocCommitChanges(w, (XtPointer)folderList[i],
				     (XtPointer) NULL);
	    }
	}
    }
}
