/*
 * $XConsortium: init.c,v 2.81 95/01/25 14:56:39 swick Exp $
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
 *
 */
/* $XFree86: xc/programs/xmh/init.c,v 1.6 2002/04/05 21:06:28 dickey Exp $ */

/* Init.c - Handle start-up initialization. */

#include "xmh.h"
#include "actions.h"
#include "version.h"
#include <errno.h>

#define MIN_APP_DEFAULTS_VERSION 1
#define xmhCkpDefault "%d.CKP"

static String FallbackResources[] = {
"*folderButton.label: Close",
"*folderButton.borderWidth: 4",
"*folderButton.translations: #override <Btn1Down>: XmhClose()",
NULL
};

static Boolean static_variable;	 /* whose address is not a widget ID */

/* This is for the check mark in the Options menu */
#define check_width 9
#define check_height 8
static unsigned char check_bits[] = {
   0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00,
   0x31, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04, 0x00
};

#define Offset(field) XtOffsetOf(struct _resources, field)

/* Xmh application resources. */

static XtResource resources[] = {
    {"debug", "Debug", XtRBoolean, sizeof(Boolean),
	 Offset(debug), XtRImmediate, (XtPointer)False},

    {"tempDir", "TempDir", XtRString, sizeof(char *),
	 Offset(temp_dir), XtRString, "/tmp"},
    {"mhPath", "MhPath", XtRString, sizeof(char *),
	 Offset(mh_path), XtRString, "/usr/local/mh6"},
    {"mailPath", "MailPath", XtRString, sizeof(char *),
	 Offset(mail_path), XtRString, NULL},
    {"initialFolder", "InitialFolder", XtRString, sizeof(char *),
	 Offset(initial_folder_name), XtRString, "inbox"},
    {"initialIncFile", "InitialIncFile", XtRString, sizeof(char *),
         Offset(initial_inc_file), XtRString, NULL},
    {"replyInsertFilter", "ReplyInsertFilter", XtRString, sizeof(char *),
	 Offset(insert_filter), XtRString, NULL},
    {"draftsFolder", "DraftsFolder", XtRString, sizeof(char *),
	 Offset(drafts_folder_name), XtRString, "drafts"},
    {"printCommand", "PrintCommand", XtRString, sizeof(char *),
	 Offset(print_command), XtRString,
	 "enscript > /dev/null 2>/dev/null"},

    {"sendWidth", "SendWidth", XtRInt, sizeof(int),
	 Offset(send_line_width), XtRImmediate, (XtPointer)72},
    {"sendBreakWidth", "SendBreakWidth", XtRInt, sizeof(int),
	 Offset(break_send_line_width), XtRImmediate, (XtPointer)85},
    {"tocWidth", "TocWidth", XtRInt, sizeof(int),
	 Offset(toc_width), XtRImmediate, (XtPointer)100},
    {"skipDeleted", "SkipDeleted", XtRBoolean, sizeof(Boolean),
	 Offset(skip_deleted), XtRImmediate, (XtPointer)True},
    {"skipMoved", "SkipMoved", XtRBoolean, sizeof(Boolean),
	 Offset(skip_moved), XtRImmediate, (XtPointer)True},
    {"skipCopied", "SkipCopied", XtRBoolean, sizeof(Boolean),
	 Offset(skip_copied), XtRImmediate, (XtPointer)False},
    {"hideBoringHeaders", "HideBoringHeaders", XtRBoolean, sizeof(Boolean),
	 Offset(hide_boring_headers), XtRImmediate, (XtPointer)True},

    {"geometry", "Geometry", XtRString, sizeof(char *),
	 Offset(geometry), XtRString, NULL},
    {"tocGeometry", "TocGeometry", XtRString, sizeof(char *),
	 Offset(toc_geometry), XtRString, NULL},
    {"viewGeometry", "ViewGeometry", XtRString, sizeof(char *),
	 Offset(view_geometry), XtRString, NULL},
    {"compGeometry", "CompGeometry", XtRString, sizeof(char *),
	 Offset(comp_geometry), XtRString, NULL},
    {"pickGeometry", "PickGeometry", XtRString, sizeof(char *),
	 Offset(pick_geometry), XtRString, NULL},
    {"tocPercentage", "TocPercentage", XtRInt, sizeof(int),
	 Offset(toc_percentage), XtRImmediate, (XtPointer)33},

    {"checkNewMail", "CheckNewMail", XtRBoolean, sizeof(Boolean),
	 Offset(new_mail_check), XtRImmediate, (XtPointer)True},
    {"mailInterval", "Interval", XtRInt, sizeof(int),
	 Offset(mail_interval), XtRImmediate, (XtPointer)-1},
    {"makeCheckpoints", "MakeCheckpoints", XtRBoolean, sizeof(Boolean),
	 Offset(make_checkpoints), XtRImmediate, (XtPointer)False},
    {"checkpointInterval", "Interval", XtRInt, sizeof(int),
	 Offset(checkpoint_interval), XtRImmediate, (XtPointer)-1},
    {"checkpointNameFormat", "CheckpointNameFormat",
	 XtRString, sizeof(char *),
	 Offset(checkpoint_name_format), XtRString, xmhCkpDefault},
    {"rescanInterval", "Interval", XtRInt, sizeof(int),
	 Offset(rescan_interval), XtRImmediate, (XtPointer)-1},
    {"checkFrequency", "CheckFrequency", XtRInt, sizeof(int),
	 Offset(check_frequency), XtRImmediate, (XtPointer)1},
    {"mailWaitingFlag", "MailWaitingFlag", XtRBoolean, sizeof(Boolean),
	 Offset(mail_waiting_flag), XtRImmediate, (XtPointer)False},
    {"newMailIconBitmap", "NewMailBitmap", XtRBitmap, sizeof(Pixmap),
	 Offset(new_mail_icon), XtRString, (XtPointer)"flagup"},
    {"noMailIconBitmap", "NoMailBitmap", XtRBitmap, sizeof(Pixmap),
	 Offset(no_mail_icon), XtRString, (XtPointer)"flagdown"},
    {"newMailBitmap", "NewMailBitmap", XtRBitmap, sizeof(Pixmap),
	 Offset(flag_up), XtRString, (XtPointer)"black6"},
    {"noMailBitmap", "NoMailBitmap", XtRBitmap, sizeof(Pixmap),
	 Offset(flag_down), XtRString, (XtPointer)"box6"},

    {"cursor", "Cursor", XtRCursor, sizeof(Cursor),
	 Offset(cursor), XtRString, "left_ptr"},
    {"pointerColor", "PointerColor", XtRPixel, sizeof(Pixel),
	 Offset(pointer_color), XtRString, XtDefaultForeground},
    {"showOnInc", "ShowOnInc", XtRBoolean, sizeof(Boolean),
	 Offset(show_on_inc), XtRImmediate, (XtPointer)True},
    {"stickyMenu", "StickyMenu", XtRBoolean, sizeof(Boolean), 	
	 Offset(sticky_menu), XtRImmediate, (XtPointer)False},
    {"prefixWmAndIconName", "PrefixWmAndIconName", XtRBoolean, sizeof(Boolean),
	 Offset(prefix_wm_and_icon_name), XtRImmediate, (XtPointer)True},
    {"reverseReadOrder", "ReverseReadOrder", XtRBoolean, sizeof(Boolean),
	 Offset(reverse_read_order), XtRImmediate, (XtPointer)False},
    {"blockEventsOnBusy", "BlockEventsOnBusy", XtRBoolean, sizeof(Boolean),
	 Offset(block_events_on_busy), XtRImmediate, (XtPointer)True},
    {"busyCursor", "BusyCursor", XtRCursor, sizeof(Cursor),
	 Offset(busy_cursor), XtRString, "watch"},
    {"busyPointerColor", "BusyPointerColor", XtRPixel, sizeof(Pixel),
	 Offset(busy_pointer_color), XtRString, XtDefaultForeground},
    {"commandButtonCount", "CommandButtonCount", XtRInt, sizeof(int),
	 Offset(command_button_count), XtRImmediate, (XtPointer)0},
    {"appDefaultsVersion", "AppDefaultsVersion", XtRInt, sizeof(int),
	 Offset(app_defaults_version), XtRImmediate, (XtPointer)0},
    {"banner", "Banner", XtRString, sizeof(char *),
	 Offset(banner), XtRString, XMH_VERSION},
    {"wmProtocolsTranslations", "WMProtocolsTranslations", 
	 XtRTranslationTable, sizeof(XtTranslations),
	 Offset(wm_protocols_translations), XtRString,
	 "<Message>WM_PROTOCOLS: XmhWMProtocols()\n"}
};

#undef Offset

static XrmOptionDescRec table[] = {
    {"-debug",	"debug",		XrmoptionNoArg,	"on"},
    {"-flag",	"mailWaitingFlag",	XrmoptionNoArg, "on"},
    {"-initial","initialFolder",	XrmoptionSepArg, NULL},
    {"-path",	"mailPath",		XrmoptionSepArg, NULL},
};

/* Tell the user how to use this program. */
static void Syntax(char *call)
{
    (void) fprintf(stderr, "usage: %s [-path <path>] [-initial <folder>]\n",
		   call);
    exit(2);
}


static char *FixUpGeometry(char *geo, unsigned defwidth, unsigned defheight)
{
    int gbits;
    int x, y;
    unsigned int width, height;
    if (geo == NULL) geo = app_resources.geometry;
    x = y = 0;
    gbits = XParseGeometry(geo, &x, &y, &width, &height);
    if (!(gbits & WidthValue)) {
	width = defwidth;
	gbits |= WidthValue;
    }
    if (!(gbits & HeightValue)) {
	height = defheight;
	gbits |= HeightValue;
    }
    return CreateGeometry(gbits, x, y, (int) width, (int) height);
}


static int _IOErrorHandler(Display *dpy)
{
    (void) fprintf (stderr,
	     "%s:\tfatal IO error after %lu requests (%lu known processed)\n",
		    progName,
		    NextRequest(dpy) - 1, LastKnownRequestProcessed(dpy));
    (void) fprintf (stderr, "\t%d unprocessed events remaining.\r\n",
		    QLength(dpy));

    if (errno == EPIPE) {
	(void) fprintf (stderr,
     "\tThe connection was probably broken by a server shutdown or KillClient.\r\n");
    }

    Punt("Cannot continue from server error.");
    return 0;
}

/*ARGSUSED*/
static void PopupAppDefaultsWarning(
    Widget w,
    XtPointer closure,
    XEvent *event,
    Boolean *cont)
{
    if (event->type == MapNotify) {
	PopupError(w,
"The minimum application default resources\n\
were not properly installed; many features\n\
will not work properly, if at all.  See the\n\
xmh man page for further information."
		   );
	XtRemoveEventHandler(w, XtAllEvents, True,
			     PopupAppDefaultsWarning, closure);
    }
}


/*ARGSUSED*/
static void _Die(
    Widget w,			/* == toplevel */
    XtPointer client_data,	/* unused */
    XtPointer call_data)	/* unused */
{
    int i;

    for (i=0; i<numScrns; i++)
	if (scrnList[i]->mapped)
	    XtUnmapWidget(scrnList[i]->parent);

    XtDestroyApplicationContext(XtWidgetToApplicationContext(w));
    exit(0);
}


/* All the start-up initialization goes here. */

extern char** environ;	/* POSIX doesn't specify a .h for this */

void InitializeWorld(int argc, char **argv)
{
    int l;
    FILEPTR fid;
    char str[500], str2[500], *ptr;
    Scrn scrn;
    XtAppContext app;
    static XtActionsRec actions[] = {

	/* general Xmh action procedures */

	{"XmhClose",			XmhClose},
	{"XmhComposeMessage",		XmhComposeMessage},
	{"XmhWMProtocols",		XmhWMProtocols},

	/* actions upon folders */

	{"XmhOpenFolder",		XmhOpenFolder},
	{"XmhOpenFolderInNewWindow",	XmhOpenFolderInNewWindow},
	{"XmhCreateFolder",		XmhCreateFolder},
	{"XmhDeleteFolder",		XmhDeleteFolder},

	/* actions to support easier folder manipulation */

	{"XmhPushFolder",		XmhPushFolder},
	{"XmhPopFolder",		XmhPopFolder},
        {"XmhPopupFolderMenu",		XmhPopupFolderMenu},
        {"XmhSetCurrentFolder",		XmhSetCurrentFolder},
        {"XmhLeaveFolderButton",	XmhLeaveFolderButton},
	{"XmhCheckForNewMail",		XmhCheckForNewMail},

	/* actions upon the Table of Contents */

	{"XmhIncorporateNewMail",	XmhIncorporateNewMail},
	{"XmhCommitChanges",		XmhCommitChanges},
	{"XmhPackFolder",		XmhPackFolder},
	{"XmhSortFolder",		XmhSortFolder},
	{"XmhForceRescan",		XmhForceRescan},
	{"XmhReloadSeqLists",		XmhReloadSeqLists},

	/* actions upon the currently selected message(s) */

	{"XmhViewNextMessage",		XmhViewNextMessage},
	{"XmhViewPreviousMessage",	XmhViewPreviousMessage},
	{"XmhMarkDelete",		XmhMarkDelete},
	{"XmhMarkMove",			XmhMarkMove},
	{"XmhMarkCopy",			XmhMarkCopy},
	{"XmhUnmark",			XmhUnmark},
	{"XmhViewInNewWindow",		XmhViewInNewWindow},
	{"XmhReply",			XmhReply},
	{"XmhForward",			XmhForward},
	{"XmhUseAsComposition",		XmhUseAsComposition},
	{"XmhPrint",			XmhPrint},
	{"XmhShellCommand",		XmhShellCommand},

	/* actions upon sequences */

	{"XmhPickMessages",		XmhPickMessages},
	{"XmhOpenSequence",		XmhOpenSequence},
	{"XmhAddToSequence",		XmhAddToSequence},
	{"XmhRemoveFromSequence",	XmhRemoveFromSequence},
	{"XmhDeleteSequence",		XmhDeleteSequence},

	/* actions to support easier sequence manipulation */

	{"XmhPushSequence",		XmhPushSequence},
	{"XmhPopSequence",		XmhPopSequence},

	/* actions upon the currently viewed message */

	{"XmhCloseView",		XmhCloseView},
	{"XmhViewReply",		XmhViewReply},
	{"XmhViewForward",		XmhViewForward},
	{"XmhViewUseAsComposition",	XmhViewUseAsComposition},
	{"XmhEditView",			XmhEditView},
	{"XmhSaveView",			XmhSaveView},
	{"XmhPrintView",		XmhPrintView},
	{"XmhViewMarkDelete",		XmhViewMarkDelete},

       	/* actions upon a composition, reply, or forward */

	/* Close button			XmhCloseView	  (see above) */
	{"XmhResetCompose",		XmhResetCompose},
	/* Compose button 		XmhComposeMessage (see above) */
	{"XmhSave",			XmhSave},
	{"XmhSend",			XmhSend},
	{"XmhInsert",			XmhInsert},

	/* popup dialog box button action procedures */

	{"XmhPromptOkayAction",		XmhPromptOkayAction},

	/* retained for backward compatibility with user resources */
	
	{"XmhCancelPick",		XmhWMProtocols}
    };

    static Arg shell_args[] = {
	{XtNinput, (XtArgVal)True},
	{XtNjoinSession, (XtArgVal)False}, /* join is delayed to end of init */
	{XtNenvironment, (XtArgVal)NULL},  /* set dynamically below */
	{XtNmappedWhenManaged, (XtArgVal)False}
    };

    ptr = strrchr(argv[0], '/');
    if (ptr) progName = ptr + 1;
    else progName = argv[0];

    shell_args[2].value = (XtArgVal)environ;
    toplevel = XtOpenApplication(&app, "Xmh", table, XtNumber(table),
				 &argc, argv, FallbackResources,
				 sessionShellWidgetClass,
				 shell_args, XtNumber(shell_args));
    if (argc > 1) Syntax(progName);

    XSetIOErrorHandler(_IOErrorHandler);

    theDisplay = XtDisplay(toplevel);

    homeDir = getenv("HOME");
    homeDir = XtNewString(homeDir);

    XtGetApplicationResources( toplevel, (XtPointer)&app_resources,
			       resources, XtNumber(resources),
			       NULL, (Cardinal)0 );

    if (app_resources.app_defaults_version < MIN_APP_DEFAULTS_VERSION)
	XtAddEventHandler(toplevel, StructureNotifyMask, False,
			  PopupAppDefaultsWarning, NULL);

    if (app_resources.mail_waiting_flag) app_resources.new_mail_check = True;
    if (app_resources.mail_interval == -1)
	app_resources.mail_interval = app_resources.check_frequency;
    if (app_resources.checkpoint_interval == -1)
	app_resources.checkpoint_interval = 5 * app_resources.check_frequency;
    if (app_resources.rescan_interval == -1)
	app_resources.rescan_interval = 5 * app_resources.check_frequency;
    ptr = strchr(app_resources.checkpoint_name_format, '%');
    while (ptr && *(++ptr) != 'd')
	ptr = strchr(app_resources.checkpoint_name_format, '%');
    if (!ptr || strlen(app_resources.checkpoint_name_format) == 2)
	app_resources.checkpoint_name_format = xmhCkpDefault;

    ptr = getenv("MH");
    if (!ptr) {
	(void) sprintf(str, "%s/.mh_profile", homeDir);
	ptr = str;
    }
    fid = myfopen(ptr, "r");
    if (fid) {
	while ((ptr = ReadLine(fid))) {
	    char *cp;

	    (void) strncpy(str2, ptr, 5);
	    str2[5] = '\0';
	    for (cp = str2; *cp; cp++) {
		if ('A' <= *cp && *cp <= 'Z')
		    *cp = *cp - 'A' + 'a';
	    }
	    if (strcmp(str2, "path:") == 0) {
		ptr += 5;
		while (*ptr == ' ' || *ptr == '\t')
		    ptr++;
		(void) strcpy(str, ptr);
	    }
	}
	myfclose(fid);
    } else {
	(void) strcpy(str, "Mail");
    }
    for (l=strlen(str) - 1; l>=0 && (str[l] == ' ' || str[l] == '\t'); l--)
	str[l] = 0;
    if (str[0] == '/')
	(void) strcpy(str2, str);
    else
	(void) sprintf(str2, "%s/%s", homeDir, str);

    (void) sprintf(str, "%s/draft", str2);
    draftFile = XtNewString(str);
    (void) sprintf(str, "%s/xmhdraft", str2);
    xmhDraftFile = XtNewString(str);

    if (app_resources.mail_path == NULL)
	app_resources.mail_path = XtNewString(str2);

    NullSource = (Widget) NULL;

    l = strlen(app_resources.mh_path) - 1;
    if (l > 0 && app_resources.mh_path[l] == '/')
	app_resources.mh_path[l] = 0;

    rootwidth = WidthOfScreen(XtScreen(toplevel));
    rootheight = HeightOfScreen(XtScreen(toplevel));

    app_resources.toc_geometry =
	FixUpGeometry(app_resources.toc_geometry,
		      (unsigned)rootwidth / 2, 3 * (unsigned)rootheight / 4);
    app_resources.view_geometry =
	FixUpGeometry(app_resources.view_geometry,
		      (unsigned)rootwidth / 2, (unsigned)rootheight / 2);
    app_resources.comp_geometry =
	FixUpGeometry(app_resources.comp_geometry,
		      (unsigned)rootwidth / 2, (unsigned)rootheight / 2);
    app_resources.pick_geometry =
	FixUpGeometry(app_resources.pick_geometry,
		      (unsigned)rootwidth / 2, (unsigned)rootheight / 2);

    numScrns = 0;
    scrnList = (Scrn *) NULL;
    NoMenuForButton = (Widget) &static_variable;
    LastMenuButtonPressed = (Widget) NULL;

    TocInit();
    InitPick();
    BBoxInit();

    XtAppAddActions(app, actions, XtNumber(actions));
    XtRegisterGrabAction(XmhPopupFolderMenu, True, 
			 ButtonPressMask | ButtonReleaseMask,
			 GrabModeAsync, GrabModeAsync);
    wm_protocols = XInternAtom(XtDisplay(toplevel), "WM_PROTOCOLS", False);
    protocolList[0] = wm_delete_window =
	XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW", False);
    protocolList[1] = wm_save_yourself = 
	XInternAtom(XtDisplay(toplevel), "WM_SAVE_YOURSELF", False);

    XtAddCallback(toplevel, XtNsaveCallback, DoSaveYourself, (XtPointer)NULL);
    XtAddCallback(toplevel, XtNdieCallback, _Die, (XtPointer)NULL);

    MenuItemBitmap =
	XCreateBitmapFromData( XtDisplay(toplevel),
			      RootWindowOfScreen( XtScreen(toplevel)),
			      (char *)check_bits, check_width, check_height);

    DEBUG("Making screen ... ")

    scrn = CreateNewScrn(STtocAndView);

    SetCursorColor(scrn->parent, app_resources.cursor,
		   app_resources.pointer_color);
    if (app_resources.block_events_on_busy)
	SetCursorColor(scrn->parent, app_resources.busy_cursor, 
		       app_resources.busy_pointer_color);

    DEBUG(" setting toc ... ")

    TocSetScrn(InitialFolder, scrn);

    DEBUG("done.\n");

    XtVaSetValues(toplevel, XtNjoinSession, (XtArgVal)True, NULL);

    MapScrn(scrn);
}
