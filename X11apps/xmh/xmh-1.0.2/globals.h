/*
 *  $XConsortium: globals.h /main/37 1996/02/02 14:27:39 kaleb $
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
/* $XFree86$ */

#ifdef MAIN
#define ext
#else
#define ext extern
#endif

extern Display	*theDisplay;		/* Display variable. */
extern Widget	toplevel;		/* The top level widget (A hack %%%). */
extern char	*progName;		/* Program name. */
extern char	*homeDir;		/* User's home directory. */
extern Atom	wm_protocols;		/* WM_PROTOCOLS atom for this display */
extern Atom	wm_delete_window;	/* see ICCCM on Window Deletion */
extern Atom	wm_save_yourself;	/* see ICCCM on session management */
extern Atom	protocolList[2];	/* contains the two above */

struct _resources {
    Boolean	debug;
    char	*mail_path;		/* mh's mail directory. */
    char	*temp_dir;		/* Directory for temporary files. */
    char	*mh_path;		/* Path for mh commands. */
    char	*initial_folder_name;	/* Initial folder to use. */
    char	*initial_inc_file;	/* -file for inc on initial folder */
    char	*insert_filter;		/* Insert message filter command */
    char	*drafts_folder_name;	/* Folder for drafts. */
    int		send_line_width;	/* How long to break lines on send. */
    int		break_send_line_width;	/* Minimum length of a line before
					   we'll break it. */
    char	*print_command;		/* Printing command. */
    int		toc_width;	/* How many characters wide to use in tocs */
    Boolean	skip_deleted;		/* If true, skip over deleted msgs. */
    Boolean	skip_moved;		/* If true, skip over moved msgs. */
    Boolean	skip_copied;		/* If true, skip over copied msgs. */
    Boolean	hide_boring_headers;
    char	*geometry;	/* Default geometry to use for things. */
    char	*toc_geometry;
    char	*view_geometry;
    char	*comp_geometry;
    char	*pick_geometry;
    int		toc_percentage;		/* % of toc and view used by toc */
    Boolean	new_mail_check;		/* should xmh check for new mail? */
    Boolean	make_checkpoints;       /* should xmh save edits in progress?*/
    int		check_frequency;	/* backwards compatibility */
    int		mail_waiting_flag;	/* If true, change icon on new mail */
    int		mail_interval;		/* how often to check for new mail */
    int		rescan_interval;	/* how often to check viewed tocs */
    int		checkpoint_interval;	/* how often to save edits */
    char *	checkpoint_name_format; /* format of checkpoint file name */
    Pixmap	flag_up;		/* folder has new mail */
    Pixmap	flag_down;		/* folder has no new mail */
    Pixmap	new_mail_icon;		/* new mail icon for wm hints */
    Pixmap	no_mail_icon;		/* no mail icon for wm hints */
    Cursor	cursor;			/* application cursor */
    Pixel	pointer_color;		/* application cursor color */
    Boolean	show_on_inc;		/* show 1st new message after inc? */
    Boolean	sticky_menu;		/* command menu entries are sticky? */
    Boolean	prefix_wm_and_icon_name;/* prefix wm names with progName ? */
    Boolean	reverse_read_order;	/* decrement counter to next msg ? */
    Boolean	block_events_on_busy;	/* disallow user input while busy ? */
    Cursor	busy_cursor;		/* the cursor while input blocked */
    Pixel	busy_pointer_color;	/* busy cursor color */
    int		command_button_count;	/* number of buttons in command box */
    int		app_defaults_version;	/* for sanity check */
    char 	*banner;		/* defaults to xmh version string */
    XtTranslations wm_protocols_translations; /* for all shells seen by WM */
};

extern struct _resources app_resources;

extern char	*draftFile;		/* Filename of draft. */
extern char	*xmhDraftFile;		/* Filename for sending. */
extern Toc	*folderList;		/* Array of folders. */
extern int	numFolders;		/* Number of entries in above array. */
extern Toc	InitialFolder;		/* Toc containing initial folder. */
extern Toc	DraftsFolder;		/* Toc containing drafts. */
extern Scrn	*scrnList;		/* Array of scrns in use. */
extern int	numScrns;		/* Number of scrns in above array. */
extern Widget	NoMenuForButton;	/* Flag menu widget value: no menu */
extern Widget	LastMenuButtonPressed;	/* to `toggle' menu buttons */
extern Widget	NullSource;		/* null text widget source */
extern Dimension rootwidth;		/* Dimensions of root window.  */
extern Dimension rootheight;
extern Pixmap	MenuItemBitmap;		/* Options menu item checkmark */
extern XtTranslations NoTextSearchAndReplace; /* no-op ^S and ^R in Text */

struct _LastInput {
    Window win;
    int x;
    int y;
};

extern struct _LastInput lastInput;

extern Boolean	subProcessRunning; /* interlock for DoCommand/CheckMail */

#define PNullSource (NullSource != NULL ? NullSource : \
(NullSource = (Widget)  CreateFileSource(scrn->viewlabel, "/dev/null", False)))


typedef struct _XmhMenuEntry {
    char	*name;			/* menu entry name */
    void   	(*function)(XMH_CB_ARGS); /* menu entry callback function */
} XmhMenuEntryRec, *XmhMenuEntry;	


typedef struct _XmhMenuButtonDesc {
    char	*button_name;		/* menu button name */
    char	*menu_name;		/* menu name */
    int		id;			/* an internal key */
    XmhMenuEntry entry;			/* list of menu entries */
    Cardinal	num_entries;		/* count of menu entries in list */
} XmhMenuButtonDescRec, *XmhMenuButtonDesc;


extern XmhMenuEntryRec	folderMenu[];
extern XmhMenuEntryRec	tocMenu[];
extern XmhMenuEntryRec	messageMenu[];
extern XmhMenuEntryRec	sequenceMenu[];
extern XmhMenuEntryRec	viewMenu[];
extern XmhMenuEntryRec	optionMenu[];

extern XmhMenuButtonDescRec	MenuBoxButtons[];

/* Used as indices into MenuBoxButtons; these must correspond. */

#define XMH_FOLDER	0
#define XMH_TOC		1
#define XMH_MESSAGE	2
#define	XMH_SEQUENCE	3
#define XMH_VIEW	4
#define XMH_OPTION	5

/* Bell types Feep() */
#ifdef XKB
#include <X11/extensions/XKBbells.h>
#else
#define	XkbBI_Info			0
#define	XkbBI_MinorError		1
#define	XkbBI_MajorError		2
#define	XkbBI_Failure			6
#define	XkbBI_Wait			7
#define	XkbBI_NewMail			12
#endif
