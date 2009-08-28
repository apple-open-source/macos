/*
 *	rcs_id[] = "$XConsortium: xedit.h,v 1.19 89/10/07 14:59:46 kit Exp $";
 */
 
/*
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
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT RIGHTS,
 * APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN ADDITION TO THAT
 * SET FORTH ABOVE.
 *
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be 
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */
/* $XFree86: xc/programs/xedit/xedit.h,v 1.17 2002/10/06 17:11:39 paulo Exp $ */

#include <stdio.h>
#include <time.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/SimpleMenu.h>

#include <X11/IntrinsicP.h>
#include <X11/Xaw/TextSinkP.h>		/* for the property list definition */

#include <X11/Xmu/SysUtil.h>

#define LSCAN(from, count, include)	\
	XawTextSourceScan(source, from, XawstEOL, XawsdLeft, count, include)
#define RSCAN(from, count, include)	\
	XawTextSourceScan(source, from, XawstEOL, XawsdRight, count, include)

typedef struct _xedit_hints {
    char *resource;
    unsigned long interval;
    XtIntervalId timer;
    char **hints;
    unsigned num_hints;
    unsigned cur_hint;
} xedit_hints;

typedef enum {NO_READ, READ_OK, WRITE_OK} FileAccess;

typedef struct _XeditLispData XeditLispData;
typedef struct _XeditTagsInfo XeditTagsInfo;

#define CHANGED_BIT	0x01
#define EXISTS_BIT	0x02
#define WRAP_BIT	0x10
typedef struct _xedit_flist_item {
    Widget source, sme;
    String name;
    String filename;
    int flags;
    FileAccess file_access;
    XawTextPosition display_position, insert_position;
    int mode;
    time_t mtime;
    XawTextPropertyList *properties;
    XawTextWrapMode wrap;
    XeditLispData *xldata;
    XeditTagsInfo *tags;
} xedit_flist_item;

extern struct _xedit_flist {
    Widget popup;
    Pixmap pixmap;
    xedit_flist_item **itens;
    xedit_flist_item *current, *other;
    Cardinal num_itens;
} flist;

extern Widget options_popup;

extern struct _app_resources {
    Boolean enableBackups;
    char *backupNamePrefix;
    char *backupNameSuffix;
    xedit_hints hints;
    char *changed_pixmap_name;
    char *position_format;
    char *auto_replace;
    char *tagsName;
    Boolean loadTags;
} app_resources;

extern Widget topwindow, textwindow, labelwindow, filenamewindow, messwidget;
extern Widget dirlabel, dirwindow;
extern Boolean international;
extern Boolean line_edit;

/*	externals in xedit.c 	*/
void Feep(void);

/*	externals in util.c 	*/
void XeditPrintf(const char *format, ...);
Widget MakeCommandButton(Widget, char*, XtCallbackProc);
Widget MakeStringBox(Widget, String, String);
String GetString(Widget);
FileAccess MaybeCreateFile(char*), CheckFilePermissions(char*, Boolean*);
xedit_flist_item *AddTextSource(Widget, String, String, int, FileAccess);
xedit_flist_item *FindTextSource(Widget, char*);
Bool KillTextSource(xedit_flist_item*);
char *ResolveName(char*);
void DeleteWindow(Widget, XEvent*, String*, Cardinal*);
void SplitWindow(Widget, XEvent*, String*, Cardinal*);
void SwitchTextSource(xedit_flist_item*);
void PopupMenu(Widget, XEvent*, String*, Cardinal*);
void OtherWindow(Widget, XEvent*, String*, Cardinal*);
void SwitchSource(Widget, XEvent*, String*, Cardinal*);
void XeditFocus(Widget, XEvent*, String*, Cardinal*);
void SwitchDirWindow(Bool);
void DirWindow(Widget, XEvent*, String*, Cardinal*);

/*	externs in commands.c 	*/
void DoQuit(Widget, XtPointer, XtPointer);
void QuitAction(Widget, XEvent*, String*, Cardinal*);
void DoSave(Widget, XtPointer, XtPointer);
void SaveFile(Widget, XEvent*, String*, Cardinal*);
void DoLoad(Widget, XtPointer, XtPointer);
#ifdef INCLUDE_XPRINT_SUPPORT
void DoPrint(Widget, XtPointer, XtPointer);
#endif /* INCLUDE_XPRINT_SUPPORT */
void CancelFindFile(Widget, XEvent*, String*, Cardinal*);
void FindFile(Widget, XEvent*, String*, Cardinal*);
void LoadFile(Widget, XEvent*, String*, Cardinal*);
Bool LoadFileInTextwindow(char *name, char *resolved_name);
#ifdef INCLUDE_XPRINT_SUPPORT
void PrintFile(Widget, XEvent*, String*, Cardinal*);
#endif /* INCLUDE_XPRINT_SUPPORT */
void FileCompletion(Widget, XEvent*, String*, Cardinal*);
void KillFile(Widget, XEvent*, String*, Cardinal*);
void DirWindowCB(Widget, XtPointer, XtPointer);
void SourceChanged(Widget, XtPointer, XtPointer);

/*	externs in ispell.c	*/
void IspellAction(Widget, XEvent*, String*, Cardinal*);
void IspellCallback(Widget, XtPointer, XtPointer);

/*	externs in options.c	*/
void CreateEditPopup(void);
void SetEditMenu(void);
void UpdateTextProperties(int);

/*	externs in hook.c	*/
Bool StartHooks(XtAppContext);
void LineEditAction(Widget, XEvent*, String*, Cardinal*);
void LineEdit(Widget);

/*	externs in lisp.c	*/
void XeditLispInitialize(void);
void XeditLispEval(Widget, XEvent*, String*, Cardinal*);
void XeditPrintLispEval(Widget, XEvent*, String*, Cardinal*);
void XeditKeyboardReset(Widget, XEvent*, String*, Cardinal*);
void XeditLispCleanUp(void);
void SetTextProperties(xedit_flist_item*);
void UnsetTextProperties(xedit_flist_item*);
void CreateEditModePopup(Widget);
void SetEditModeMenu(void);

/* tags.c */
void TagsAction(Widget, XEvent*, String*, Cardinal*);
void SearchTagsFile(xedit_flist_item *item);

/*	externs for system replacement functions */
#ifdef NEED_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#ifdef NEED_REALPATH
#include <sys/param.h>
#if defined(ISC)
#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif
#endif
char *realpath(const char *pathname, char resolvedname[MAXPATHLEN]);
#endif

