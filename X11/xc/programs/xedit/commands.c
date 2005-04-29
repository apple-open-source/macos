/* $XConsortium: commands.c,v 1.33 91/10/21 14:32:18 eswu Exp $ */

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
/* $XFree86: xc/programs/xedit/commands.c,v 1.30 2003/05/27 22:27:01 tsi Exp $ */

#include <X11/Xfuncs.h>
#include <X11/Xos.h>
#include "xedit.h"
#ifdef CRAY
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <X11/Xmu/SysUtil.h>
#include <X11/IntrinsicP.h>
#include <X11/Xaw/TextSrcP.h>

void ResetSourceChanged(xedit_flist_item*);
static void ResetDC(Widget, XtPointer, XtPointer);

static void AddDoubleClickCallback(Widget, Bool);
static Bool ReallyDoLoad(char*, char*);
static char *makeBackupName(String, String, unsigned);

extern Widget scratch, texts[3], labels[3];
static Boolean double_click = FALSE;

#define DC_UNSAVED	1
#define DC_LOADED	2
#define DC_CLOBBER	3
#define DC_KILL		4
#define DC_SAVE		5
static int dc_state;

/*	Function Name: AddDoubleClickCallback(w)
 *	Description: Adds a callback that will reset the double_click flag
 *                   to false when the text is changed.
 *	Arguments: w - widget to set callback upon.
 *                 state - If true add the callback, else remove it.
 *	Returns: none.
 */
static void
AddDoubleClickCallback(Widget w, Bool state)
{
  Arg args[1];
  static XtCallbackRec cb[] = { {NULL, NULL}, {NULL, NULL} };

  if (XtIsSubclass(w, asciiSrcObjectClass)) {
      if (state)
	  XtAddCallback(w, XtNcallback, ResetDC, NULL);
      else
	  XtRemoveCallback(w, XtNcallback, ResetDC, NULL);
  }
  else {
      if (state)
	  cb[0].callback = ResetDC;
      else
	  cb[0].callback = NULL;

      XtSetArg(args[0], XtNcallback, cb);
      XtSetValues(w, args, ONE);
  }
}
  
/*	Function Name: ResetDC
 *	Description: Resets the double click flag.
 *	Arguments: w - the text widget.
 *                 junk, garbage - *** NOT USED ***
 *	Returns: none.
 */

/* ARGSUSED */
static void
ResetDC(Widget w, XtPointer junk, XtPointer garbage)
{
  double_click = FALSE;

  AddDoubleClickCallback(w, FALSE);
}

/*ARGSUSED*/
void
QuitAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    DoQuit(w, NULL, NULL);
}

/*ARGSUSED*/
void
DoQuit(Widget w, XtPointer client_data, XtPointer call_data)
{
    unsigned i;
    Bool source_changed = False;

    if (!double_click || (dc_state && dc_state != DC_UNSAVED)) {
	for (i = 0; i < flist.num_itens; i++)
	    if (flist.itens[i]->flags & CHANGED_BIT) {
		source_changed = True;
		break;
	    }
    }
    if(!source_changed) {
#ifndef __UNIXOS2__
	XeditLispCleanUp();
#endif
	exit(0);
    }

    XeditPrintf("Unsaved changes. Save them, or Quit again.\n");
    Feep();
    double_click = TRUE;
    dc_state = DC_UNSAVED;
    AddDoubleClickCallback(XawTextGetSource(textwindow), True);
}

static char *
makeBackupName(String buf, String filename, unsigned len)
{
    if (app_resources.backupNamePrefix
	&& strlen(app_resources.backupNamePrefix)) {
	if (strchr(app_resources.backupNamePrefix, '/'))
	    XmuSnprintf(buf, len, "%s%s%s", app_resources.backupNamePrefix,
			filename, app_resources.backupNameSuffix);
	else {
	    char fname[BUFSIZ];
	    char *name, ch;

	    strncpy(fname, filename, sizeof(fname) - 1);
	    fname[sizeof(fname) - 1] = '\0';
	    if ((name = strrchr(fname, '/')) != NULL)
		++name;
	    else
		name = filename;
	    ch = *name;
	    *name = '\0';
	    ++name;
	    XmuSnprintf(buf, len, "%s%s%c%s%s",
			fname, app_resources.backupNamePrefix, ch, name,
			app_resources.backupNameSuffix);
	}
    }
    else
	XmuSnprintf(buf, len, "%s%s",
		    filename, app_resources.backupNameSuffix);

    return (strcmp(filename, buf) ? buf : NULL);
}
  
#if defined(USG) && !defined(CRAY)
int rename (from, to)
    char *from, *to;
{
    (void) unlink (to);
    if (link (from, to) == 0) {
        unlink (from);
        return 0;
    } else {
        return -1;
    }
}
#endif

/*ARGSUSED*/
void
SaveFile(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    DoSave(w, NULL, NULL);
}

/*ARGSUSED*/
void
DoSave(Widget w, XtPointer client_data, XtPointer call_data)
{
    String name = GetString(filenamewindow);
    String filename = ResolveName(name);
    char buf[BUFSIZ];
    FileAccess file_access;
    xedit_flist_item *item;
    Boolean exists;
    Widget source = XawTextGetSource(textwindow);

    if (!filename) {
	XeditPrintf("Save: Can't resolve pathname -- nothing saved.\n");
	Feep();
	return;
    }
    else if (*name == '\0') {
	XeditPrintf("Save: No filename specified -- nothing saved.\n");
	Feep();
	return;
    }
    else {
	struct stat st;

	if (stat(filename, &st) == 0 && !S_ISREG(st.st_mode)) {
	    XmuSnprintf(buf, sizeof(buf),
			"Save: file %s is not a regular file -- nothing saved.\n",
			name);
	    XeditPrintf(buf);
	    Feep();
	    return;
	}
    }

    item = FindTextSource(NULL, filename);
    if (item != NULL && item->source != source) {
	if (!double_click || (dc_state && dc_state != DC_LOADED)) {
	    XmuSnprintf(buf, sizeof(buf),
			"Save: file %s is already loaded, "
			"Save again to unload it -- nothing saved.\n",
			name);
	    XeditPrintf(buf);
	    Feep();
	    double_click = TRUE;
	    dc_state = DC_LOADED;
	    AddDoubleClickCallback(XawTextGetSource(textwindow), True);
	    return;
	}
	KillTextSource(item);
	item = FindTextSource(source = XawTextGetSource(textwindow), NULL);
	double_click = FALSE;
	dc_state = 0;
    }
    else if (item && !(item->flags & CHANGED_BIT)) {
	if (!double_click || (dc_state && dc_state != DC_SAVE)) {
	    XeditPrintf("Save: No changes need to be saved, "
			"Save again to override.\n");
	    Feep();
	    double_click = TRUE;
	    dc_state = DC_SAVE;
	    AddDoubleClickCallback(XawTextGetSource(textwindow), True);
	    return; 
	}
	double_click = FALSE;
	dc_state = 0;
    }

    file_access = CheckFilePermissions(filename, &exists);
    if (!item || strcmp(item->filename, filename)) {
	if (file_access == WRITE_OK && exists) {
	    if (!double_click || (dc_state && dc_state != DC_CLOBBER)) {
		XmuSnprintf(buf, sizeof(buf),
			    "Save: file %s already exists, "
			    "Save again to overwrite it -- nothing saved.\n",
			    name);
		XeditPrintf(buf);
		Feep();
		double_click = TRUE;
		dc_state = DC_CLOBBER;
		AddDoubleClickCallback(XawTextGetSource(textwindow), True);
		return;
	    }
	    double_click = FALSE;
	    dc_state = 0;
	}
	if (!item)
	    item = FindTextSource(source, NULL);
    }
  
  if (app_resources.enableBackups && exists) {
    char backup_file[BUFSIZ];

    if (makeBackupName(backup_file, filename, sizeof(backup_file)) == NULL
	|| rename(filename, backup_file) != 0) {
	XmuSnprintf(buf, sizeof(buf),"error backing up file:  %s\n",
		    filename); 
      XeditPrintf(buf);
    }
  }
  
  switch( file_access = MaybeCreateFile(filename)) {
  case NO_READ:
  case READ_OK:
      XmuSnprintf(buf, sizeof(buf),
		  "File %s could not be opened for writing.\n", name);
      Feep();
      break;
  case WRITE_OK:
      if ( XawAsciiSaveAsFile(source, filename) ) {
	  int i;
	  Arg args[1];
	  char label_buf[BUFSIZ];

	  /* Keep file protection mode */
	  if (item && item->mode)
	      chmod(filename, item->mode);

	  XmuSnprintf(label_buf, sizeof(label_buf),
		      "%s       Read - Write", name);
	  XtSetArg(args[0], XtNlabel, label_buf);
	  for (i = 0; i < 3; i++)
	      if (XawTextGetSource(texts[i]) == source)
		  XtSetValues(labels[i], args, 1);

	  XmuSnprintf(buf, sizeof(buf), "Saved file:  %s\n", name);

	  if (item && item->source != scratch) {
	      XtSetArg(args[0], XtNlabel, filename);
	      XtSetValues(item->sme, args, 1);

	      XtSetArg(args[0], XtNeditType, XawtextEdit);
	      XtSetValues(item->source, args, 1);

	      XtFree(item->name);
	      XtFree(item->filename);
	      item->name = XtNewString(name);
	      item->filename = XtNewString(filename);
	      item->flags = EXISTS_BIT;
	  }
	  else {
	      item = flist.itens[0];
	      XtRemoveCallback(scratch, XtNcallback, SourceChanged,
			       (XtPointer)item);
	      item->source = scratch =
		  XtVaCreateWidget("textSource", international ?
				   multiSrcObjectClass : asciiSrcObjectClass,
				   topwindow,
				   XtNtype, XawAsciiFile,
				   XtNeditType, XawtextEdit,
				   NULL, NULL);
	      ResetSourceChanged(item);
	      XtAddCallback(scratch, XtNcallback, SourceChanged,
			    (XtPointer)item);

	      item = AddTextSource(source, name, filename, EXISTS_BIT,
				   file_access);
	      XtAddCallback(item->source, XtNcallback, SourceChanged,
			    (XtPointer)item);
	  }
	  item->flags |= EXISTS_BIT;
	  ResetSourceChanged(item);
      }
      else {
	  XmuSnprintf(buf, sizeof(buf), "Error saving file:  %s\n",  name);
	  Feep();
      }
      break;
  default:
      XmuSnprintf(buf, sizeof(buf), "%s %s",
		  "Internal function MaybeCreateFile()",
	      "returned unexpected value.\n");
      Feep();
      break;
  }

  XeditPrintf(buf);
}

/*ARGSUSED*/
void
DoLoad(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (ReallyDoLoad(GetString(filenamewindow), ResolveName(NULL))) {
        SwitchDirWindow(False);
        XtSetKeyboardFocus(topwindow, textwindow);
    }
}

static Bool
ReallyDoLoad(char *name, char *filename)
{
    Arg args[5];
    Cardinal num_args = 0;
    char buf[BUFSIZ];
    xedit_flist_item *item;
    Widget source = XawTextGetSource(textwindow);

    if (!filename) {
	XeditPrintf("Load: Can't resolve pathname.\n");
	Feep();
	return (False);
    }
    else if (*name == '\0') {
	XeditPrintf("Load: No file specified.\n");
	Feep();
    }
    if ((item = FindTextSource(NULL, filename)) != NULL) {
	SwitchTextSource(item);
	return (True);
    }
    else {
	struct stat st;

	if (stat(filename, &st) == 0 && !S_ISREG(st.st_mode)) {
	    if (S_ISDIR(st.st_mode)) {
		char path[BUFSIZ + 1];

		strncpy(path, filename, sizeof(path) - 2);
		path[sizeof(path) - 2] = '\0';
		if (*path) {
		    if (path[strlen(path) - 1] != '/')
			strcat(path, "/");
		}
		else
		    strcpy(path, "./");
		XtSetArg(args[0], XtNlabel, "");
		XtSetValues(dirlabel, args, 1);
		SwitchDirWindow(True);
		DirWindowCB(dirwindow, path, NULL);
		return (False);
	    }
	}
    }

    {
	Boolean exists;
	int flags;
	FileAccess file_access;

	switch( file_access = CheckFilePermissions(filename, &exists) ) {
	case NO_READ:
	    if (exists)
		XmuSnprintf(buf, sizeof(buf), "File %s, %s", name,
			"exists, and could not be opened for reading.\n");
	    else
		XmuSnprintf(buf, sizeof(buf), "File %s %s %s",  name,
			    "does not exist, and",
			"the directory could not be opened for writing.\n");

	    XeditPrintf(buf);
	    Feep();
	    return (False);
	case READ_OK:
	    XtSetArg(args[num_args], XtNeditType, XawtextRead); num_args++;
	    XmuSnprintf(buf, sizeof(buf), "File %s opened READ ONLY.\n",
			name);
	    break;
	case WRITE_OK:
	    XtSetArg(args[num_args], XtNeditType, XawtextEdit); num_args++;
	    XmuSnprintf(buf, sizeof(buf), "File %s opened read - write.\n",
			name);
	    break;
	default:
	    XmuSnprintf(buf, sizeof(buf), "%s %s",
			"Internal function MaybeCreateFile()",
		    "returned unexpected value.\n");
	    XeditPrintf(buf);
	    Feep();
	    return (False);
	}

	XeditPrintf(buf);

	if (exists) {
	    flags = EXISTS_BIT;
	    XtSetArg(args[num_args], XtNstring, filename); num_args++;
	}
	else {
	    flags = 0;
	    XtSetArg(args[num_args], XtNstring, NULL); num_args++;
	}

	source = XtVaCreateWidget("textSource", international ?
				  multiSrcObjectClass : asciiSrcObjectClass,
				  topwindow,
				  XtNtype, XawAsciiFile,
				  XtNeditType, XawtextEdit,
				  NULL, NULL);
	XtSetValues(source, args, num_args);

	item = AddTextSource(source, name, filename, flags, file_access);
	XtAddCallback(item->source, XtNcallback, SourceChanged,
		      (XtPointer)item);
	if (exists && file_access == WRITE_OK) {
	    struct stat st;

	    if (stat(filename, &st) == 0)
		item->mode = st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}
	SwitchTextSource(item);
	ResetSourceChanged(item);
    }

    return (True);
}

/*	Function Name: SourceChanged
 *	Description: A callback routine called when the source has changed.
 *	Arguments: w - the text source that has changed.
 *		   client_data - xedit_flist_item associated with text buffer.
 *                 call_data - NULL is unchanged
 *	Returns: none.
 */
/*ARGSUSED*/
void
SourceChanged(Widget w, XtPointer client_data, XtPointer call_data)
{
    xedit_flist_item *item = (xedit_flist_item*)client_data;
    Bool changed = (Bool)(long)call_data;

    if (changed) {
	if (item->flags & CHANGED_BIT)
	    return;
	item->flags |= CHANGED_BIT;
    }
    else {
	if (item->flags & CHANGED_BIT)
	    ResetSourceChanged(item);
	return;
    }

    if (flist.pixmap) {
	Arg args[1];
	Cardinal num_args;
	int i;

	num_args = 0;
	XtSetArg(args[num_args], XtNleftBitmap, flist.pixmap);	++num_args;
	XtSetValues(item->sme, args, num_args);

	for (i = 0; i < 3; i++)
	    if (XawTextGetSource(texts[i]) == item->source)
		XtSetValues(labels[i], args, num_args);
    }
}

/*	Function Name: ResetSourceChanged.
 *	Description: Sets the source changed to FALSE, and
 *                   registers a callback to set it to TRUE when
 *                   the source has changed.
 *	Arguments: item - item with widget to register the callback on.
 *	Returns: none.
 */

void
ResetSourceChanged(xedit_flist_item *item)
{
    Arg args[1];
    Cardinal num_args;
    int i;

    num_args = 0;
    XtSetArg(args[num_args], XtNleftBitmap, None);	++num_args;
    XtSetValues(item->sme, args, num_args);

    dc_state = 0;
    double_click = FALSE;
    for (i = 0; i < 3; i++) {
	if (XawTextGetSource(texts[i]) == item->source)
	    XtSetValues(labels[i], args, num_args);
	AddDoubleClickCallback(XawTextGetSource(texts[i]), False);
    }

    num_args = 0;
    XtSetArg(args[num_args], XtNsourceChanged, False);	++num_args;
    XtSetValues(item->source, args, num_args);

    item->flags &= ~CHANGED_BIT;
}

/*ARGSUSED*/
void
KillFile(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    xedit_flist_item *item = FindTextSource(XawTextGetSource(textwindow), NULL);

    if (item->source == scratch) {
	Feep();
	return;
    }

    if (item->flags & CHANGED_BIT) {
	if (!double_click || (dc_state && dc_state != DC_KILL)) {
	    XeditPrintf("Kill: Unsaved changes. Kill again to override.\n");
	    Feep();
	    double_click = TRUE;
	    dc_state = DC_KILL;
	    AddDoubleClickCallback(XawTextGetSource(textwindow), True);
	    return;
	}
	double_click = FALSE;
	dc_state = 0;
    }
    KillTextSource(item);
}

/*ARGSUSED*/
void
FindFile(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    char *string = GetString(filenamewindow);
    char *slash = NULL;
    XawTextBlock block;
    XawTextPosition end = XawTextSourceScan(XawTextGetSource(filenamewindow),
					    0, XawstAll, XawsdRight, 1, True);

    if (string)
	slash = strrchr(string, '/');
    block.firstPos = 0;
    block.format = FMT8BIT;
    block.ptr = string;
    block.length = slash ? slash - string + 1 : 0;

    if (block.length != end)
	XawTextReplace(filenamewindow, 0, end, &block);
    XawTextSetInsertionPoint(filenamewindow, end);
    XtSetKeyboardFocus(topwindow, filenamewindow);
    line_edit = False;
}

/*ARGSUSED*/
void
LoadFile(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    if (line_edit)
	LineEdit(textwindow);
    else if (ReallyDoLoad(GetString(filenamewindow), ResolveName(NULL))) {
	SwitchDirWindow(False);
	XtSetKeyboardFocus(topwindow, textwindow);
    }
}

/*ARGSUSED*/
void
CancelFindFile(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Arg args[1];
    xedit_flist_item *item;

    XtSetKeyboardFocus(topwindow, textwindow);

    item = FindTextSource(XawTextGetSource(textwindow), NULL);

    if (item->source != scratch)
	XtSetArg(args[0], XtNstring, item->name);
    else
	XtSetArg(args[0], XtNstring, NULL);

    XtSetValues(filenamewindow, args, 1);

   if (XtIsManaged(XtParent(dirwindow)))
	SwitchDirWindow(False);

    line_edit = False;
}

static int
compar(_Xconst void *a, _Xconst void *b)
{
    return (strcmp(*(char **)a, *(char **)b));
}

/*ARGSUSED*/
void
FileCompletion(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XawTextBlock block;
    String text;
    int length;
    char **matches, *save, *dir_name, *file_name, match[257];
    unsigned n_matches, len, mlen, buflen;
    DIR *dir;
    Bool changed, slash = False, has_dot = False;
#define	SM_NEVER	0
#define SM_HINT		1
#define SM_ALWAYS	2
    int show_matches;

    text = GetString(filenamewindow);

    if (!text) {
	Feep();
	return;
    }
    else if (line_edit) {
	Feep();
	line_edit = 0;
    }

    {
	XawTextPosition pos = XawTextGetInsertionPoint(w);
	char *cslash = strchr(&text[pos], '/'), *cdot = strchr(&text[pos], '.');

	if (cslash != NULL || cdot != NULL) {
	    if (cslash != NULL && (cdot == NULL || cdot > cslash)) {
		length = cslash - text;
		slash = True;
	    }
	    else {
		length = cdot - text;
		has_dot = True;
	    }
	}
	else
	    length = strlen(text);
    }

    if (*num_params == 1 && length == strlen(text)) {
	switch (params[0][0]) {
	case 'n':		/* Never */
	case 'N':
	    show_matches = SM_NEVER;
	    break;
	case 'h':		/* Hint */
	case 'H':
	    show_matches = SM_HINT;
	    break;
	case 'a':		/* Always */
	case 'A':
	    show_matches = SM_ALWAYS;
	    break;
	default:
	    show_matches = SM_NEVER;
	    XtAppWarning(XtWidgetToApplicationContext(w),
			 "Bad argument to file-completion, "
			 "must be Never, Hint or Always");
	    break;
	}
    }
    else
	show_matches = SM_NEVER;

    matches = NULL;
    n_matches = buflen = 0;
    save = XtMalloc(length + 1);
    memmove(save, text, length);
    save[length] = '\0';

    if (save[0] == '~' && save[1]) {
	char *slash2 = strchr(save, '/');

	if (slash2) {
	    struct passwd *pw;
	    char home[BUFSIZ];
	    char *name;
	    int slen = strlen(save), diff = slash2 - save;

	    *slash2 = '\0';
	    name = save + 1;
	    if (strlen(name) != 0)
		pw = getpwnam(name);
	    else
		pw = getpwuid(getuid());

	    if (pw) {
		char fname[BUFSIZ];
		int hlen;

		strncpy(home, pw->pw_dir, sizeof(home) - 1);
		home[sizeof(home) - 1] = '\0';
		hlen = strlen(home);
		strncpy(fname, slash2 + 1, sizeof(fname) - 1);
		fname[sizeof(fname) - 1] = '\0';
		save = XtRealloc(save, slen - diff + hlen + 2);
		(void)memmove(&save[hlen], slash2, slen - diff + 1);
		(void)memmove(save, home, hlen);
		save[hlen] = '/';
		strcpy(&save[hlen + 1], fname);

		/* expand directory */
		block.length = strlen(save);
		block.ptr = save;
		block.firstPos = 0;
		block.format = FMT8BIT;
		XawTextReplace(filenamewindow, 0, length, &block);
		XawTextSetInsertionPoint(filenamewindow, length = block.length);
	    }
	    else
		*slash2 = '/';
	}
    }

    if ((file_name = strrchr(save, '/')) != NULL) {
	*file_name = '\0';
	++file_name;
	dir_name = save;
	if (!file_name[0])
	    slash = True;
	if (!dir_name[0])
	    dir_name = "/";
    }
    else {
	dir_name = ".";
	file_name = save;
    }
    len = strlen(file_name);

    if ((dir = opendir(dir_name)) != NULL) {
	char path[BUFSIZ], *pptr;
	struct dirent *ent;
	int isdir = 0, first = 1, bytes;

	XmuSnprintf(path, sizeof(path), "%s/", dir_name);
	pptr = path + strlen(path);
	bytes = sizeof(path) - (pptr - path) - 1;

	mlen = 0;
	match[0] = '\0';
	(void)readdir(dir);	/* "." */
	(void)readdir(dir);	/* ".." */
	while ((ent = readdir(dir)) != NULL) {
	    unsigned d_namlen = strlen(ent->d_name);

	    if (d_namlen >= len && strncmp(ent->d_name, file_name, len) == 0) {
		char *tmp = &(ent->d_name[len]), *mat = match;
		struct stat st;
		Bool is_dir = FALSE;

		strncpy(pptr, ent->d_name, bytes);
		pptr[bytes] = '\0';
		if (stat(path, &st) != 0)
		    /* Should check errno, may be a broken symbolic link
		     * a directory with r-- permission, etc */
		    continue;
		else if (first || show_matches != SM_NEVER) {
		    is_dir = S_ISDIR(st.st_mode);
		}

		if (first) {
		    strncpy(match, tmp, sizeof(match) - 1);
		    match[sizeof(match) - 2] = '\0';
		    mlen = strlen(match);
		    first = 0;
		    isdir = is_dir;
		}
		else {
		    while (*tmp && *mat && *tmp++ == *mat)
			++mat;
		    if (mlen > mat - match) {
			mlen = mat - match;
			match[mlen] = '\0';
		    }
		}
		if (show_matches != SM_NEVER) {
		    matches = (char **)XtRealloc((char*)matches, sizeof(char**)
						 * (n_matches + 1));
		    buflen += d_namlen + 1;
		    if (is_dir) {
			matches[n_matches] = XtMalloc(d_namlen + 2);
			strcpy(matches[n_matches], ent->d_name);
			strcat(matches[n_matches], "/");
			++buflen;
		    }
		    else
			matches[n_matches] = XtNewString(ent->d_name);
		}
		else if (mlen == 0 && n_matches >= 1) {
		    ++n_matches;
		    break;
		}
		++n_matches;
	    }
	}

	closedir(dir);
	changed = mlen != 0;

	if (n_matches) {
	    Bool free_matches = True, add_slash = n_matches == 1 && isdir && !slash;

	    if (mlen && has_dot && match[mlen - 1] == '.')
		--mlen;

	    if (mlen || add_slash) {
		XawTextPosition pos;

		block.firstPos = 0;
		block.format = FMT8BIT;
		if (mlen) {
		    pos = length;
		    block.length = mlen;
		    block.ptr = match;
		    XawTextReplace(filenamewindow, pos, pos, &block);
		    XawTextSetInsertionPoint(filenamewindow, pos + block.length);
		}
		if (add_slash) {
		    XawTextPosition actual = XawTextGetInsertionPoint(w);

		    pos = XawTextSourceScan(XawTextGetSource(w), 0, XawstAll,
					    XawsdRight, 1, True);
		    block.length = 1;
		    block.ptr = "/";
		    XawTextReplace(filenamewindow, pos, pos, &block);
		    if (actual == pos)
			XawTextSetInsertionPoint(filenamewindow, pos + 1);
		}
	    }
	    else if (n_matches != 1 || isdir) {
		if (show_matches == SM_NEVER)
		    Feep();
	    }

	    if (show_matches != SM_NEVER) {
		if (show_matches == SM_ALWAYS || (!changed && n_matches != 1)) {
		    char **list = NULL, *label;
		    int n_list;
		    Arg args[2];

		    XtSetArg(args[0], XtNlist, &list);
		    XtSetArg(args[1], XtNnumberStrings, &n_list);
		    XtGetValues(dirwindow, args, 2);

		    matches = (char **)XtRealloc((char*)matches, sizeof(char**)
						 * (n_matches + 2));
		    matches[n_matches++] = XtNewString("./");
		    matches[n_matches++] = XtNewString("../");
		    qsort(matches, n_matches, sizeof(char*), compar);
		    XtSetArg(args[0], XtNlist, matches);
		    XtSetArg(args[1], XtNnumberStrings, n_matches);
		    XtSetValues(dirwindow, args, 2);
		    if (n_list > 0
			&& (n_list != 1 || list[0] != XtName(dirwindow))) {
			while (--n_list > -1)
			    XtFree(list[n_list]);
			XtFree((char*)list);
		    }

		    label = ResolveName(dir_name);
		    XtSetArg(args[0], XtNlabel, label);
		    XtSetValues(dirlabel, args, 1);
		    SwitchDirWindow(True);
		    free_matches = False;
		}
	    }
	    if (free_matches && matches) {
		while (--n_matches > -1)
		    XtFree(matches[n_matches]);
		XtFree((char*)matches);
	    }
	}
	else
	    Feep();
    }
    else
	Feep();

    XtFree(save);
}

/*ARGSUSED*/
void
DirWindowCB(Widget w, XtPointer user_data, XtPointer call_data)
{
    XawListReturnStruct *file_info = (XawListReturnStruct *)call_data;
    char *dir_name, *string, path[BUFSIZ];
    Arg args[2];

    if (file_info == NULL)
	string = (char *)user_data;
    else
	string = file_info->string;

    XtSetArg(args[0], XtNlabel, &dir_name);
    XtGetValues(dirlabel, args, 1);
    if (*dir_name == '\0') {
	strncpy(path, string, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
    }
    else if (strcmp(dir_name, "/") == 0)
	XmuSnprintf(path, sizeof(path), "/%s", string);
    else
	XmuSnprintf(path, sizeof(path), "%s/%s", dir_name, string);

    if (*string && string[strlen(string) - 1] == '/') {
	DIR *dir;

	if ((dir = opendir(path)) != NULL) {
	    struct dirent *ent;
	    struct stat st;
	    unsigned d_namlen;
	    Bool isdir;
	    char **entries = NULL, **list = NULL;
	    int n_entries = 0, n_list = 0;
	    char *label, *pptr = path + strlen(path);
	    int bytes = sizeof(path) - (pptr - path) - 1;

	    while ((ent = readdir(dir)) != NULL) {
		d_namlen = strlen(ent->d_name);
		strncpy(pptr, ent->d_name, bytes);
		pptr[bytes] = '\0';
		if (stat(path, &st) != 0)
		    /* Should check errno, may be a broken symbolic link
		     * a directory with r-- permission, etc */
		    continue;
		else
		    isdir = S_ISDIR(st.st_mode);

		entries = (char **)XtRealloc((char*)entries, sizeof(char*)
					     * (n_entries + 1));
		if (isdir) {
		    entries[n_entries] = XtMalloc(d_namlen + 2);
		    strcpy(entries[n_entries], ent->d_name);
		    strcat(entries[n_entries], "/");
		}
		else
		    entries[n_entries] = XtNewString(ent->d_name);
		++n_entries;
	    }
	    closedir(dir);

	    XtSetArg(args[0], XtNlist, &list);
	    XtSetArg(args[1], XtNnumberStrings, &n_list);
	    XtGetValues(dirwindow, args, 2);

	    if (n_entries == 0) {
		entries = (char**)XtMalloc(sizeof(char*) * 2);
		/* Directory has read but not execute permission? */
		entries[n_entries++] = XtNewString("./");
		entries[n_entries++] = XtNewString("../");
	    }
	    qsort(entries, n_entries, sizeof(char*), compar);
	    XtSetArg(args[0], XtNlist, entries);
	    XtSetArg(args[1], XtNnumberStrings, n_entries);
	    XtSetValues(dirwindow, args, 2);
	    if (n_list > 0
		&& (n_list != 1 || list[0] != XtName(dirwindow))) {
		while (--n_list > -1)
		    XtFree(list[n_list]);
		XtFree((char*)list);
	    }

	    *pptr = '\0';
	    if ((label = ResolveName(path)) == NULL) {
		Feep();
		label = path;
	    }
	    XtSetArg(args[0], XtNlabel, label);
	    XtSetValues(dirlabel, args, 1);

	    strncpy(path, label, sizeof(path) - 2);
	    if (*path && path[strlen(path) - 1] != '/')
		strcat(path, "/");
	    XtSetArg(args[0], XtNstring, path);
	    XtSetValues(filenamewindow, args, 1);
	    XtSetKeyboardFocus(topwindow, filenamewindow);
	    XawTextSetInsertionPoint(filenamewindow, strlen(path));
	}
	else
	    Feep();
    }
    else {
	(void)ReallyDoLoad(path, path);
	SwitchDirWindow(False);
	XtSetKeyboardFocus(topwindow, textwindow);
    }
}
