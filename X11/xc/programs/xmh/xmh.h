/*
 * $XConsortium: xmh.h,v 2.32 93/09/08 15:31:11 kaleb Exp $
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
/* $XFree86: xc/programs/xmh/xmh.h,v 1.3 2002/07/01 02:26:05 tsi Exp $ */

#ifndef _xmh_h
#define _xmh_h

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Paned.h>
#if defined(sun) && defined(SVR4)
#define _XOPEN_SOURCE
#include <stdio.h>
#undef _XOPEN_SOURCE
#else
#include <stdio.h>
#endif

#define DELETEABORTED	-1
#define NEEDS_CONFIRMATION	-1
#define MARKPOS		4

#define xMargin 2
#define yMargin 2

#define DEBUG(msg) \
	if (app_resources.debug) \
	    {(void)fprintf(stderr, msg); (void)fflush(stderr);}

#define DEBUG1(msg, arg) \
	if (app_resources.debug) \
	    {(void)fprintf(stderr, msg, arg); (void)fflush(stderr);}

#define DEBUG2(msg, arg1, arg2) \
	if (app_resources.debug) \
	    {(void)fprintf(stderr,msg,arg1,arg2); (void)fflush(stderr);}

typedef int * dp;		/* For debugging. */

typedef FILE* FILEPTR;

typedef struct _ButtonRec *Button;
typedef struct _XmhButtonBoxRec *ButtonBox;
typedef struct _TocRec *Toc;
typedef struct _MsgRec *Msg;
typedef struct _PickRec *Pick;

typedef enum {
    Fignore, Fmove, Fcopy, Fdelete
} FateType;

typedef enum {
    STtocAndView,
    STview,
    STcomp,
    STpick
} ScrnKind;

typedef struct _StackRec {
    char		*data;
    struct _StackRec	*next;
} StackRec, *Stack;


typedef struct _ScrnRec {
   Widget	parent;		/* The parent widget of the scrn */
   Widget	widget;		/* The pane widget for the scrn */
   int		mapped;		/* TRUE only if we've mapped this screen. */
   ScrnKind	kind;		/* What kind of scrn we have. */
   ButtonBox	mainbuttons;	/* Main xmh control buttons. */
   Widget	folderlabel;	/* Folder titlebar */
   ButtonBox	folderbuttons;	/* Folder buttons. */
   Widget	toclabel;	/* Toc titlebar. */
   Widget	tocwidget;	/* Toc text. */
   ButtonBox	miscbuttons;	/* optional miscellaneous command buttons */
   Widget	viewlabel;	/* View titlebar. */
   Widget	viewwidget;	/* View text. */
   ButtonBox 	viewbuttons;	/* View control buttons. */
   char *	curfolder;	/* Currently selected folder name */
   Toc		toc;		/* The table of contents. */
   Msg		msg;		/* The message being viewed. */
   Pick		pick;		/* Pick in this screen. */
   XtTranslations edit_translations;	/* Text widget translations */
   XtTranslations read_translations;	/* overridden by accelerators */
   Msg		assocmsg;	/* Associated message for reply, etc. */
   Window	wait_window;	/* InputOnly window with busy cursor */
   Stack	folder_stack;	/* Stack of folder names */
} ScrnRec, *Scrn;


typedef struct {
    int nummsgs;
    Msg *msglist;
} MsgListRec, *MsgList;


typedef struct {
   char		*name;		/* Name of this sequence. */
   MsgList	mlist;		/* Messages in this sequence. */
} SequenceRec, *Sequence;

#define XMH_CB_ARGS Widget, XtPointer, XtPointer

#include "globals.h"
#include "externs.h"
#include "mlist.h"
#include "bbox.h"
#include "msg.h"
#include "toc.h"

#endif /* _xmh_h */
