/*
 * $XConsortium: screen.c,v 2.65 95/01/06 16:39:19 swick Exp $
 *
 *
 *		        COPYRIGHT 1987, 1989
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
/* $XFree86: xc/programs/xmh/screen.c,v 1.3 2002/04/05 21:06:29 dickey Exp $ */

/* scrn.c -- management of scrns. */

#include "xmh.h"

XmhMenuEntryRec	folderMenu[] = {
    {"open",			DoOpenFolder},
    {"openInNew", 		DoOpenFolderInNewWindow},
    {"create",			DoCreateFolder},
    {"delete",			DoDeleteFolder},
    {"line",			(XtCallbackProc) NULL},
    {"close",			DoClose},
};

XmhMenuEntryRec	tocMenu[] = {
    {"inc",			DoIncorporateNewMail},
    {"commit",			DoCommit},
    {"pack",			DoPack},
    {"sort",			DoSort},
    {"rescan",			DoForceRescan},
};

XmhMenuEntryRec	messageMenu[] = {
    {"compose",			DoComposeMessage},
    {"next",			DoNextView},
    {"prev",			DoPrevView},
    {"delete",			DoDelete},
    {"move",			DoMove},
    {"copy",			DoCopy},
    {"unmark",			DoUnmark},
    {"viewNew",			DoViewNew},
    {"reply",			DoReply},
    {"forward",			DoForward},
    {"useAsComp",		DoTocUseAsComp},
    {"print",			DoPrint},
};

XmhMenuEntryRec	sequenceMenu[] = {
    {"pick",			DoPickMessages},
    {"openSeq",			DoOpenSeq},
    {"addToSeq",		DoAddToSeq},
    {"removeFromSeq",		DoRemoveFromSeq},
    {"deleteSeq",		DoDeleteSeq},
    {"line",			(XtCallbackProc) NULL},
    {"all",			DoSelectSequence},
};

XmhMenuEntryRec	viewMenu[] = {
    {"reply",			DoViewReply},
    {"forward",			DoViewForward},
    {"useAsComp",		DoViewUseAsComposition},
    {"edit",			DoEditView},
    {"save",			DoSaveView},
    {"print",			DoPrintView},
};

XmhMenuEntryRec	optionMenu[] = {
    {"reverse",			DoReverseReadOrder},
};

XmhMenuButtonDescRec	MenuBoxButtons[] = {
    {"folderButton",	"folderMenu",	XMH_FOLDER,	folderMenu,
	 XtNumber(folderMenu) },
    {"tocButton",	"tocMenu",	XMH_TOC,	tocMenu,
	 XtNumber(tocMenu) },
    {"messageButton",	"messageMenu",	XMH_MESSAGE,	messageMenu,
	 XtNumber(messageMenu) },
    {"sequenceButton",	"sequenceMenu",	XMH_SEQUENCE,	sequenceMenu,
	 XtNumber(sequenceMenu) },
    {"viewButton",	"viewMenu",	XMH_VIEW,	viewMenu,
	 XtNumber(viewMenu) },
    {"optionButton",	"optionMenu",	XMH_OPTION,	optionMenu,
	 XtNumber(optionMenu) },
};


/* Fill in the buttons for the view commands. */

static void FillViewButtons(
Scrn scrn)
{
    ButtonBox buttonbox = scrn->viewbuttons;
    BBoxAddButton(buttonbox, "close", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "reply", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "forward", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "useAsComp", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "edit", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "save", commandWidgetClass, False);
    BBoxAddButton(buttonbox, "print", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "delete", commandWidgetClass, True);
}
    


static void FillCompButtons(
Scrn scrn)
{
    ButtonBox buttonbox = scrn->viewbuttons;
    BBoxAddButton(buttonbox, "close", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "send", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "reset", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "compose", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "save", commandWidgetClass, True);
    BBoxAddButton(buttonbox, "insert", commandWidgetClass, True);
}


static void MakeCommandMenu(
    Scrn		scrn,
    XmhMenuButtonDesc	mbd)
{
    register Cardinal i;
    Cardinal	 n;
    Widget	menu;
    ButtonBox	buttonbox = scrn->mainbuttons;
    XmhMenuEntry	e;
    Boolean	indent;
    WidgetClass	widgetclass;
    Arg		args[4];
    static XtCallbackRec callbacks[] = {
	{ (XtCallbackProc) NULL, (XtPointer) NULL},
	{ (XtCallbackProc) NULL, (XtPointer) NULL},
	{ (XtCallbackProc) NULL, (XtPointer) NULL},
    };

    /* Menus are created as childen of the Paned widget of the scrn in order
     * that they can be used both as pop-up and as pull-down menus.
     */

    n = 0;
    if (mbd->id == XMH_SEQUENCE) {
	XtSetArg(args[n], XtNallowShellResize, True); 	n++;
    }
    menu = XtCreatePopupShell(mbd->menu_name, simpleMenuWidgetClass,
			      scrn->widget, args, n);

    indent = (mbd->id == XMH_SEQUENCE || mbd->id == XMH_OPTION) ? True : False;
    e = mbd->entry;
    for (i=0; i < mbd->num_entries; i++, e++) {
	n = 0;
	if (e->function) {
	    callbacks[0].callback = e->function;
	    callbacks[0].closure  = (XtPointer) scrn;
	    callbacks[1].callback = (app_resources.sticky_menu
				     ? (XtCallbackProc) DoRememberMenuSelection
				     : (XtCallbackProc) NULL);
	    XtSetArg(args[n], XtNcallback, callbacks);	n++;

	    if (indent) { XtSetArg(args[n], XtNleftMargin, 18);	n++; }
	    widgetclass = smeBSBObjectClass;
	} else 
	    widgetclass = smeLineObjectClass;
	XtCreateManagedWidget(e->name, widgetclass, menu, args, n);
    }

    AttachMenuToButton( BBoxFindButtonNamed( buttonbox, mbd->button_name),
		       menu, mbd->menu_name);
    if (mbd->id == XMH_OPTION && app_resources.reverse_read_order)
	ToggleMenuItem(XtNameToWidget(menu, "reverse"), True);
}


/* Create subwidgets for a toc&view window. */

static void MakeTocAndView(Scrn scrn)
{
    register int	i;
    XmhMenuButtonDesc	mbd;
    ButtonBox		buttonbox;
    char		*name;
    static XawTextSelectType sarray[] = {XawselectLine,
					XawselectPosition,
					XawselectAll,
					XawselectNull};
    static Arg args[] = {
	{ XtNselectTypes,	(XtArgVal) sarray},
	{ XtNdisplayCaret,	(XtArgVal) False}
    };

    scrn->mainbuttons   = BBoxCreate(scrn, "menuBox");
    scrn->folderlabel   = CreateTitleBar(scrn, "folderTitlebar");
    scrn->folderbuttons = BBoxCreate(scrn, "folders");
    scrn->toclabel      = CreateTitleBar(scrn, "tocTitlebar");
    scrn->tocwidget	= CreateTextSW(scrn, "toc", args, XtNumber(args));
    if (app_resources.command_button_count > 0) 
	scrn->miscbuttons = BBoxCreate(scrn, "commandBox");
    scrn->viewlabel     = CreateTitleBar(scrn, "viewTitlebar");
    scrn->viewwidget    = CreateTextSW(scrn, "view", args, (Cardinal) 0);

    /* the command buttons and menus */

    buttonbox = scrn->mainbuttons;
    mbd = MenuBoxButtons;
    for (i=0; i < XtNumber(MenuBoxButtons); i++, mbd++) {
	name = mbd->button_name;
	BBoxAddButton(buttonbox, name, menuButtonWidgetClass, True);
	MakeCommandMenu(scrn, mbd);
    }

    /* the folder buttons; folder menus are created on demand. */

    buttonbox = scrn->folderbuttons;
    for (i=0 ; i<numFolders ; i++) {
	name = TocName(folderList[i]);
	if (! IsSubfolder(name))
	    BBoxAddButton(buttonbox, name, menuButtonWidgetClass, True);
	if (app_resources.new_mail_check &&
	    numScrns > 1 &&
	    TocCanIncorporate(folderList[i]))
	    BBoxMailFlag(buttonbox, name, TocHasMail(folderList[i]));
    }

    /* the optional miscellaneous command buttons */

    if (app_resources.command_button_count > 0) {
	char	name[12];
	if (app_resources.command_button_count > 500)
	    app_resources.command_button_count = 500;
	for (i=1; i <= app_resources.command_button_count; i++) {
	    sprintf(name, "button%d", i);
	    BBoxAddButton(scrn->miscbuttons, name, commandWidgetClass, True);
	}
    }
}

static void MakeView(Scrn scrn)
{
    scrn->viewlabel = CreateTitleBar(scrn, "viewTitlebar");
    scrn->viewwidget = CreateTextSW(scrn, "view", (ArgList)NULL, (Cardinal)0);
    scrn->viewbuttons = BBoxCreate(scrn, "viewButtons");
    FillViewButtons(scrn);
}


static void MakeComp(Scrn scrn)
{
    scrn->viewlabel = CreateTitleBar(scrn, "composeTitlebar");
    scrn->viewwidget = CreateTextSW(scrn, "comp", (ArgList)NULL, (Cardinal)0);
    scrn->viewbuttons = BBoxCreate(scrn, "compButtons");
    FillCompButtons(scrn);
}


/* Create a scrn of the given type. */

Scrn CreateNewScrn(ScrnKind kind)
{
    int i;
    Scrn scrn;
    static Arg arglist[] = {
	{ XtNgeometry,	(XtArgVal) NULL},
	{ XtNinput,	(XtArgVal) True}
    };

    for (i=0 ; i<numScrns ; i++)
	if (scrnList[i]->kind == kind && !scrnList[i]->mapped)
	    return scrnList[i];
    switch (kind) {
       case STtocAndView: arglist[0].value =
			   (XtArgVal)app_resources.toc_geometry;	break;
       case STview:	  arglist[0].value =
			   (XtArgVal)app_resources.view_geometry;	break;
       case STcomp:	  arglist[0].value =
			   (XtArgVal)app_resources.comp_geometry;	break;
       case STpick:	  arglist[0].value =
			   (XtArgVal)app_resources.pick_geometry;	break;
    }

    numScrns++;
    scrnList = (Scrn *)
	XtRealloc((char *) scrnList, (unsigned) numScrns*sizeof(Scrn));
    scrn = scrnList[numScrns - 1] = XtNew(ScrnRec);
    bzero((char *)scrn, sizeof(ScrnRec));
    scrn->kind = kind;
    if (numScrns == 1) scrn->parent = toplevel;
    else scrn->parent = XtCreatePopupShell(
				   progName, topLevelShellWidgetClass,
				   toplevel, arglist, XtNumber(arglist));
    XtAugmentTranslations(scrn->parent,
			  app_resources.wm_protocols_translations);
    scrn->widget =
	XtCreateManagedWidget(progName, panedWidgetClass, scrn->parent,
			      (ArgList) NULL, (Cardinal) 0);

    switch (kind) {
	case STtocAndView: 	MakeTocAndView(scrn); break;
	case STview:		MakeView(scrn); break;
	case STcomp:		MakeComp(scrn);	break;
	default: break;
    }

    if (kind != STpick) {
	int	theight, min, max;
	Arg	args[1];

	DEBUG("Realizing...")
	XtRealizeWidget(scrn->parent);
	DEBUG(" done.\n")

	switch (kind) {
	  case STtocAndView:
	    BBoxLockSize(scrn->mainbuttons);
	    BBoxLockSize(scrn->folderbuttons);
	    theight = GetHeight(scrn->tocwidget) + GetHeight(scrn->viewwidget);
	    theight = app_resources.toc_percentage * theight / 100;
	    XawPanedGetMinMax((Widget) scrn->tocwidget, &min, &max);
	    XawPanedSetMinMax((Widget) scrn->tocwidget, theight, theight);
	    XawPanedSetMinMax((Widget) scrn->tocwidget, min, max);
	    if (scrn->miscbuttons)
		BBoxLockSize(scrn->miscbuttons);

	    /* fall through */

	  case STview:

	    /* Install accelerators; not active while editing in the view */

	    XtSetArg(args[0], XtNtranslations, &(scrn->edit_translations));
	    XtGetValues(scrn->viewwidget, args, (Cardinal) 1);
	    XtInstallAllAccelerators(scrn->widget, scrn->widget);
	    if (kind == STtocAndView)
		XtInstallAllAccelerators(scrn->tocwidget, scrn->widget);
	    XtInstallAllAccelerators(scrn->viewwidget, scrn->widget);
	    XtSetArg(args[0], XtNtranslations, &(scrn->read_translations));
	    XtGetValues(scrn->viewwidget, args, (Cardinal) 1);

	    if (kind == STview)
		BBoxLockSize(scrn->viewbuttons);
	    break;

	  case STcomp:
	    BBoxLockSize(scrn->viewbuttons);
	    XtInstallAllAccelerators(scrn->viewwidget, scrn->widget);
	    XtSetKeyboardFocus(scrn->parent, scrn->viewwidget);
	    break;

	  default:
	    break;
	}

	InitBusyCursor(scrn);
	XDefineCursor(XtDisplay(scrn->parent), XtWindow(scrn->parent),
		      app_resources.cursor);
	(void) XSetWMProtocols(XtDisplay(scrn->parent), XtWindow(scrn->parent),
			       protocolList, XtNumber(protocolList));
    }
    scrn->mapped = False;
    return scrn;
}


Scrn NewViewScrn(void)
{
    return CreateNewScrn(STview);
}

Scrn NewCompScrn(void)
{
    Scrn scrn;
    scrn = CreateNewScrn(STcomp);
    scrn->assocmsg = (Msg)NULL;
    return scrn;
}

void ScreenSetAssocMsg(Scrn scrn, Msg msg)
{
    scrn->assocmsg = msg;
}

/* Destroy the screen.  If unsaved changes are in a msg, too bad. */

void DestroyScrn(Scrn scrn)
{
    if (scrn->mapped) {
	scrn->mapped = False;
	XtPopdown(scrn->parent);
	TocSetScrn((Toc) NULL, scrn);
	MsgSetScrnForce((Msg) NULL, scrn);
	lastInput.win = -1;
    }
}


void MapScrn(Scrn scrn)
{
    if (!scrn->mapped) {
	XtPopup(scrn->parent, XtGrabNone);
	scrn->mapped = True;
    }
}


Scrn ScrnFromWidget(Widget w) /* heavily used, should be efficient */
{
    register int i;
    while (w && ! XtIsTopLevelShell(w))
	w = XtParent(w);
    if (w) {
	for (i=0 ; i<numScrns ; i++) {
	    if (w == (Widget) scrnList[i]->parent)
		return scrnList[i];
	}
    }
    Punt("ScrnFromWidget failed!");
    return NULL;
}
 

/* Figure out which buttons should and shouldn't be enabled in the given
 * screen.  This should be called whenever something major happens to the
 * screen.
 */


/*ARGSUSED*/
static void EnableCallback(Widget w, XtPointer data, XtPointer junk)
{
  EnableProperButtons( (Scrn) data);
}  

#define SetButton(buttonbox, name, value) \
    if (value) BBoxEnable(BBoxFindButtonNamed(buttonbox, name)); \
    else BBoxDisable(BBoxFindButtonNamed(buttonbox, name));


void EnableProperButtons(Scrn scrn)
{
    int value, changed, reapable;
    Button	button;

    if (scrn) {
	switch (scrn->kind) {
	  case STtocAndView:
	    button = BBoxFindButtonNamed
		(scrn->mainbuttons, MenuBoxButtons[XMH_TOC].button_name);
	    value = TocCanIncorporate(scrn->toc);
	    SendMenuEntryEnableMsg(button, "inc", value);

	    button = BBoxFindButtonNamed
		(scrn->mainbuttons, MenuBoxButtons[XMH_SEQUENCE].button_name);
	    value = TocHasSequences(scrn->toc);
	    SendMenuEntryEnableMsg(button, "openSeq", value);
	    SendMenuEntryEnableMsg(button, "addToSeq", value);
	    SendMenuEntryEnableMsg(button, "removeFromSeq", value);
	    SendMenuEntryEnableMsg(button, "deleteSeq", value);

	    button = BBoxFindButtonNamed
		 (scrn->mainbuttons, MenuBoxButtons[XMH_VIEW].button_name);
	    value = (scrn->msg != NULL && !MsgGetEditable(scrn->msg));
	    SendMenuEntryEnableMsg(button, "edit", value);
	    SendMenuEntryEnableMsg(button, "save",
				   scrn->msg != NULL && !value);
	    break;
	  case STview:
	    value = (scrn->msg != NULL && !MsgGetEditable(scrn->msg));
	    SetButton(scrn->viewbuttons, "edit", value);
	    SetButton(scrn->viewbuttons, "save", scrn->msg != NULL && !value);
	    break;
	  case STcomp:
	    if (scrn->msg != NULL) {
		changed = MsgChanged(scrn->msg);
		reapable = MsgGetReapable(scrn->msg);
		SetButton(scrn->viewbuttons, "send", changed || !reapable);
		SetButton(scrn->viewbuttons, "save", changed || reapable);
		SetButton(scrn->viewbuttons, "insert",
			  scrn->assocmsg != NULL ? True : False);

		if (!changed) 
		    MsgSetCallOnChange(scrn->msg, EnableCallback,
				       (XtPointer) scrn);
		else 
		    MsgSetCallOnChange(scrn->msg, (XtCallbackProc) NULL,
				       (XtPointer) NULL);

	    } else {
		BBoxDisable( BBoxFindButtonNamed(scrn->viewbuttons, "send"));
		BBoxDisable( BBoxFindButtonNamed(scrn->viewbuttons, "save"));
		BBoxDisable( BBoxFindButtonNamed(scrn->viewbuttons, "insert"));
	    }
	    break;
	  default:
	    break;
	}
    }
}
