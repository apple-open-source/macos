/* $XConsortium: util.c,v 1.16 92/08/12 16:46:22 converse Exp $ */

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
/* $XFree86: xc/programs/xedit/util.c,v 1.25 2002/12/04 05:27:56 paulo Exp $ */

#include <stdio.h>
#include <stdlib.h>		/* for realpath() */
#include "xedit.h"

#include <X11/Xfuncs.h>
#include <X11/Xos.h>		/* for types.h */

#include <sys/stat.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SimpleMenu.h>

/*
 * Prototypes
 */
static void SwitchSourceCallback(Widget, XtPointer, XtPointer);
static int WindowIndex(Widget);
static void ChangeTextWindow(Widget);

/*
 * External
 */
extern void _XawTextShowPosition(TextWidget);

/*
 * Initialization
 */
extern Widget scratch;
extern Widget vpanes[2], labels[3], texts[3], forms[3];
extern XawTextWrapMode wrapmodes[3];

/*
 * Implementation
 */
void
XeditPrintf(char *str)
{
    XawTextBlock text;
    XawTextPosition pos = XawTextSourceScan(XawTextGetSource(messwidget),
					    0, XawstAll, XawsdRight, 1, True);

    text.length = strlen(str);
    text.ptr = str;
    text.firstPos = 0;
    text.format = FMT8BIT;

    XawTextReplace(messwidget, pos, pos, &text);

    XawTextSetInsertionPoint(messwidget, pos + text.length);
}

Widget
MakeCommandButton(Widget box, char *name, XtCallbackProc function)
{
  Widget w = XtCreateManagedWidget(name, commandWidgetClass, box, NULL, ZERO);
  if (function != NULL)
    XtAddCallback(w, XtNcallback, function, (caddr_t) NULL);
  return w;
}

Widget 
MakeStringBox(Widget parent, String name, String string)
{
  Arg args[5];
  Cardinal numargs = 0;
  Widget StringW;

  XtSetArg(args[numargs], XtNeditType, XawtextEdit); numargs++;
  XtSetArg(args[numargs], XtNstring, string); numargs++;

  StringW = XtCreateManagedWidget(name, asciiTextWidgetClass, 
				  parent, args, numargs);
  return(StringW);  
}
 
/*	Function Name: GetString
 *	Description: retrieves the string from a asciiText widget.
 *	Arguments: w - the ascii text widget.
 *	Returns: the filename.
 */

String
GetString(Widget w)
{
  String str;
  Arg arglist[1];
  
  XtSetArg(arglist[0], XtNstring, &str);
  XtGetValues( w, arglist, ONE);
  return(str);
}

/*	Function Name: MaybeCreateFile
 *	Description: Checks to see if file exists, and if not, creates it.
 *	Arguments: file - name of file to check.
 *	Returns: permissions status
 */

FileAccess
MaybeCreateFile(char *file)
{
    Boolean exists;
    int fd;

    if (access(file, F_OK) != 0) {
	fd = creat(file, 0666);
	if (fd != -1)
	    close(fd);
    }

    return(CheckFilePermissions(file, &exists));
}


FileAccess
CheckFilePermissions(char *file, Boolean *exists)
{
    char temp[BUFSIZ], *ptr;

    if (access(file, F_OK) == 0) {
	*exists = TRUE;

	if (access(file, R_OK) != 0) 
	    return(NO_READ);
	
	if (access(file, R_OK | W_OK) == 0) 
	    return(WRITE_OK);
	return(READ_OK);
    }

    *exists = FALSE;
    
    strncpy(temp, file, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    if ( (ptr = rindex(temp, '/')) == NULL) 
	strcpy(temp, ".");
    else 
	*ptr = '\0';
    
    if (access(temp, R_OK | W_OK | X_OK) == 0)
	return(WRITE_OK);
    return(NO_READ);
}

xedit_flist_item *
AddTextSource(Widget source, char *name, char *filename, int flags,
	      FileAccess file_access)
{
    xedit_flist_item *item;
    char *wid_name;

    item = (xedit_flist_item*)XtMalloc(sizeof(xedit_flist_item));
    item->source = source;
    item->name = XtNewString(name);
    item->filename = XtNewString(filename);
    item->flags = flags;
    item->file_access = file_access;
    item->display_position = item->insert_position = 0;
    item->mode = 0;
    item->properties = NULL;
    item->xldata = NULL;

    flist.itens = (xedit_flist_item**)
	XtRealloc((char*)flist.itens, sizeof(xedit_flist_item*)
		  * (flist.num_itens + 1));

    flist.itens[flist.num_itens++] = item;

    if (!flist.popup) {
	flist.popup = XtCreatePopupShell("fileMenu", simpleMenuWidgetClass,
					 topwindow, NULL, 0);
	/* XXX hack: this don't allow setting the geometry of the popup widget
	 * as it will override any settings when a menu entry is inserted
	 * or deleted, but saves us the trouble generated by a resource
	 * setting like: '*geometry: <width>x<height>'
	 */
	XtRealizeWidget(flist.popup);
    }
    if ((wid_name = strrchr(item->name, '/')) == NULL)
	wid_name = item->name;
    else
	++wid_name;
    item->sme = XtVaCreateManagedWidget(wid_name, smeBSBObjectClass,
					flist.popup, XtNlabel, filename,
					NULL, NULL);
    XtAddCallback(item->sme, XtNcallback,
		  SwitchSourceCallback, (XtPointer)item);

    SetTextProperties(item);

    return (item);
}

Bool
KillTextSource(xedit_flist_item *item)
{
    xedit_flist_item *nitem = NULL;
    unsigned idx, i;
    Arg targs[3];
    Cardinal tnum_args;
    Arg largs[2];
    Cardinal lnum_args;
    char label_buf[BUFSIZ];

    for (idx = 0; idx < flist.num_itens; idx++)
	if (flist.itens[idx] == item) {
	    if (idx + 1 < flist.num_itens)
		nitem = flist.itens[idx + 1];
	    else if (idx - 1 >= 0)
		nitem = flist.itens[idx - 1];
	    break;
	}

    if (idx >= flist.num_itens)
	return (False);

    flist.current = nitem;
    if (item == flist.other)
	flist.other = NULL;

    if (nitem->file_access == READ_OK)
	XmuSnprintf(label_buf, sizeof(label_buf), "%s       READ ONLY",
		    nitem->name);
    else if (nitem->file_access == WRITE_OK)
	XmuSnprintf(label_buf, sizeof(label_buf), "%s       Read - Write",
		    nitem->name);
    lnum_args = 0;
    XtSetArg(largs[lnum_args], XtNlabel, label_buf);		++lnum_args;
    if (nitem->flags & CHANGED_BIT)
	XtSetArg(largs[lnum_args], XtNleftBitmap, flist.pixmap);
    else
	XtSetArg(largs[lnum_args], XtNleftBitmap, None);
    ++lnum_args;

    tnum_args = 0;
    XtSetArg(targs[tnum_args], XtNtextSource,
	     nitem->source);				++tnum_args;
    XtSetArg(targs[tnum_args], XtNdisplayPosition,
	     nitem->display_position);			++tnum_args;
    XtSetArg(targs[tnum_args], XtNinsertPosition,
	     nitem->insert_position);			++tnum_args;
    for (i = 0; i < 3; i++)
	if (XawTextGetSource(texts[i]) == item->source) {
	    XtSetValues(labels[i], largs, lnum_args);
	    XawTextDisableRedisplay(texts[i]);
	    XtSetValues(texts[i], targs, tnum_args);

	    UpdateTextProperties(0);

	    _XawTextShowPosition((TextWidget)texts[i]);
	    XawTextEnableRedisplay(texts[i]);
	    if (texts[i] == textwindow) {
		Arg args[1];

		if (nitem->source != scratch)
		    XtSetArg(args[0], XtNstring, nitem->name);
		else
		    XtSetArg(args[0], XtNstring, NULL);
		XtSetValues(filenamewindow, args, 1);
	    }
	}

    UnsetTextProperties(item);
    XtFree(item->name);
    XtFree(item->filename);
    XtDestroyWidget(item->sme);
    XtDestroyWidget(item->source);
    XtFree((char*)item);

    if (idx < flist.num_itens - 1)
	memmove(&flist.itens[idx], &flist.itens[idx + 1],
		(flist.num_itens - idx) * sizeof(xedit_flist_item*));

    --flist.num_itens;

    return (True);
}

xedit_flist_item *
FindTextSource(Widget source, char *filename)
{
    unsigned i;

    if (source) {
	for (i = 0; i < flist.num_itens; i++)
	    if (flist.itens[i]->source == source)
		return (flist.itens[i]);
    }
    else if (filename) {
	for (i = 0; i < flist.num_itens; i++)
	    if (strcmp(flist.itens[i]->filename, filename) == 0)
		return (flist.itens[i]);
    }

    return (NULL);
}

void
SwitchTextSource(xedit_flist_item *item)
{
    Arg args[4];
    Cardinal num_args;
    char label_buf[BUFSIZ];
    xedit_flist_item *old_item =
	FindTextSource(XawTextGetSource(textwindow), NULL);
    int i;

    if (old_item != item)
	flist.other = old_item;
    flist.current = item;

    XawTextDisableRedisplay(textwindow);
    if (item->file_access == READ_OK)
	XmuSnprintf(label_buf, sizeof(label_buf), "%s       READ ONLY",
		    item->name);
    else if (item->file_access == WRITE_OK)
	XmuSnprintf(label_buf, sizeof(label_buf), "%s       Read - Write",
		    item->name);
    num_args = 0;
    XtSetArg(args[num_args], XtNlabel, label_buf);		++num_args;
    if (item->flags & CHANGED_BIT)
	XtSetArg(args[num_args], XtNleftBitmap, flist.pixmap);
    else
	XtSetArg(args[num_args], XtNleftBitmap, None);
    ++num_args;
    XtSetValues(labelwindow, args, num_args);

    for (i = 0; i < 3; i++)
	if (XawTextGetSource(texts[i]) == item->source
	    && XtIsManaged(texts[i]))
	    break;

    if (i < 3) {
	num_args = 0;
	XtSetArg(args[num_args], XtNdisplayPosition,
	     &(item->display_position));			++num_args;
	XtSetArg(args[num_args], XtNinsertPosition,
	     &(item->insert_position));				++num_args;
	XtGetValues(texts[i], args, num_args);
    }
    if (old_item != item) {
	int count, idx = 0;

	num_args = 0;
	XtSetArg(args[num_args], XtNdisplayPosition,
	     &(old_item->display_position));			++num_args;
	XtSetArg(args[num_args], XtNinsertPosition,
	     &(old_item->insert_position));			++num_args;
	XtGetValues(textwindow, args, num_args);

	for (count = 0, i = 0; i < 3; i++)
	    if (XawTextGetSource(texts[i]) == old_item->source
		&& XtIsManaged(texts[i])) {
		if (++count > 1)
		    break;
		idx = i;
	    }

	if (count == 1) {
	    num_args = 0;
	    XtSetArg(args[num_args], XtNdisplayPosition,
		     &(old_item->display_position));		++num_args;
		XtSetArg(args[num_args], XtNinsertPosition,
		     &(old_item->insert_position));		++num_args;
	    XtGetValues(texts[idx], args, num_args);
	}
    }

    num_args = 0;
    XtSetArg(args[num_args], XtNtextSource, item->source);	++num_args;
    XtSetArg(args[num_args], XtNdisplayPosition, item->display_position);
    ++num_args;
    XtSetArg(args[num_args], XtNinsertPosition, item->insert_position);
    ++num_args;
    if (item->flags & WRAP_BIT)
	XtSetArg(args[num_args], XtNwrap, item->wrap);
    else
	XtSetArg(args[num_args], XtNwrap, wrapmodes[WindowIndex(textwindow)]);
    ++num_args;
    XtSetValues(textwindow, args, num_args);

    UpdateTextProperties(0);

    _XawTextShowPosition((TextWidget)textwindow);
    XawTextEnableRedisplay(textwindow);

    num_args = 0;
    if (item->source != scratch) {
	XtSetArg(args[num_args], XtNstring, item->name);	++num_args;
    }
    else {
	XtSetArg(args[num_args], XtNstring, NULL);		++num_args;
    }
    XtSetValues(filenamewindow, args, num_args);
}

/* XXX sizeof(name) must match argument size for realpath */
static char name[BUFSIZ];
char *
ResolveName(char *filename)
{
    if (filename == NULL)
	filename = GetString(filenamewindow);

#ifndef __UNIXOS2__
    return (realpath(filename, name));
#else
    return filename;
#endif
}

static void
ChangeTextWindow(Widget w)
{
    Arg args[1];

    if (textwindow != w) {
	xedit_flist_item *other, *current;

	other = FindTextSource(XawTextGetSource(textwindow), NULL);
	current = FindTextSource(XawTextGetSource(w), NULL);
	if (other != current)
	    flist.other = other;
	if (current)
	    flist.current = current;
	XtSetArg(args[0], XtNdisplayCaret, False);
	XtSetValues(textwindow, args, 1);
	XtSetArg(args[0], XtNdisplayCaret, True);
	XtSetValues(w, args, 1);
	XawTextUnsetSelection(textwindow);
	textwindow = w;
    }
}

/*ARGSUSED*/
void
XeditFocus(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Arg args[1];
    xedit_flist_item *item;
    int idx = WindowIndex(w);

    XtSetKeyboardFocus(topwindow, w);

    ChangeTextWindow(w);

    labelwindow = labels[idx];
    item = FindTextSource(XawTextGetSource(textwindow), NULL);

    if (item->source != scratch)
	XtSetArg(args[0], XtNstring, item->name);
    else
	XtSetArg(args[0], XtNstring, NULL);

    XtSetValues(filenamewindow, args, 1);
}

void
PopupMenu(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Cardinal n_params = num_params ? *num_params : 0;

    if (*num_params && XmuCompareISOLatin1(*params, "editMenu") == 0)
	SetEditMenu();

    XtCallActionProc(w, "XawPositionSimpleMenu", event, params, n_params);
    XtCallActionProc(w, "XtMenuPopup", event, params, n_params);
}

/*ARGSUSED*/
static void
SwitchSourceCallback(Widget entry, XtPointer client_data, XtPointer call_data)
{
    SwitchTextSource((xedit_flist_item*)client_data);
}

static int
WindowIndex(Widget w)
{
    int i;

    for (i = 0; i < 3; i++)
	if (texts[i] == w)
	    return (i);

    return (-1);
}

void
DeleteWindow(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Widget unmanage[2];
    int idx = WindowIndex(w), uidx;
    Bool current = False;

    if (*num_params == 1 && (*params[0] == 'd' || *params[0] == 'D')) {
	if (XtIsManaged(XtParent(dirwindow)))
	    SwitchDirWindow(False);
	return;
    }

    if (idx < 0 || (!XtIsManaged(texts[1]) && !XtIsManaged(texts[2]))) {
	Feep();
	return;
    }

    if (num_params && *num_params == 1 &&
	(*params[0] == 'o' || *params[0] == 'O'))
	current = True;

    uidx = XtIsManaged(texts[1]) ? 1 : 2;

    unmanage[0] = forms[uidx];
    unmanage[1] = texts[uidx];
    XtUnmanageChildren(unmanage, 2);

    if (!XtIsManaged(texts[2]))
	XtUnmanageChild(vpanes[1]);

    if ((!current && idx == 0) || (current && idx != 0)) {
	Arg args[4];
	Cardinal num_args;
	String label_str;
	Pixmap label_pix;
	XawTextPosition d_pos, i_pos;
	Widget source;
	xedit_flist_item *item;

	num_args = 0;
	XtSetArg(args[num_args], XtNlabel, &label_str);		++num_args;
	XtSetArg(args[num_args], XtNleftBitmap, &label_pix);	++num_args;
	XtGetValues(labels[current ? idx : uidx], args, num_args);

	num_args = 0;
	XtSetArg(args[num_args], XtNlabel, label_str);		++num_args;
	XtSetArg(args[num_args], XtNleftBitmap, label_pix);	++num_args;
	XtSetValues(labels[0], args, num_args);

	num_args = 0;
	XtSetArg(args[num_args], XtNdisplayPosition, &d_pos);	++num_args;
	XtSetArg(args[num_args], XtNinsertPosition, &i_pos);	++num_args;
	XtSetArg(args[num_args], XtNtextSource, &source);	++num_args;
	XtGetValues(texts[current ? idx : uidx], args, num_args);

	num_args = 0;
	XtSetArg(args[num_args], XtNdisplayPosition, d_pos);	++num_args;
	XtSetArg(args[num_args], XtNinsertPosition, i_pos);	++num_args;
	XtSetArg(args[num_args], XtNtextSource, source);	++num_args;

	item = FindTextSource(source, NULL);
	if (item && (item->flags & WRAP_BIT))
	    XtSetArg(args[num_args], XtNwrap, item->wrap);
	else
	    XtSetArg(args[num_args], XtNwrap,
		     wrapmodes[WindowIndex(texts[current ? idx : uidx])]);
	XtSetValues(texts[0], args, num_args);

	UpdateTextProperties(0);
    }

    labelwindow = labels[0];
    XeditFocus(texts[0], NULL, NULL, NULL);
}

void
SwitchSource(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    int idx = WindowIndex(w);
    Widget source;
    int i;

    if (idx < 0 || textwindow != texts[idx]) {
	Feep();
	return;
    }

    source = XawTextGetSource(textwindow);

    for (i = 0; i < flist.num_itens; i++)
	if (flist.itens[i]->source == source) {
	    if (i > 0 && i == flist.num_itens - 1)
		i = 0;
	    else if (i < flist.num_itens - 1)
		++i;
	    else {
		Feep();
		return;
	    }
	    break;
	}

    if (i >= flist.num_itens) {
	Feep();
	return;
    }

    SwitchTextSource(flist.itens[i]);
}

void
OtherWindow(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    int oidx, idx = WindowIndex(w);

    if (idx < 0 || (!XtIsManaged(texts[1]) && !XtIsManaged(texts[2]))) {
	Feep();
	return;
    }

    if (idx == 0)
	oidx = XtIsManaged(texts[1]) ? 1 : 2;
    else
	oidx = 0;

    XeditFocus(texts[oidx], event, params, num_params);
}

void
SplitWindow(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Arg args[6];
    Cardinal num_args;
    Widget nlabel, ntext, source, sink, manage[2];
    Dimension width, height, bw;
    XawTextPosition i_pos, d_pos;
    String label_str;
    Pixmap label_pix;
    int idx = WindowIndex(w), dimension;
    Bool vert = True;
    xedit_flist_item *item;

    if (num_params && *num_params == 1
	&& (*params[0] == 'h' || *params[0] == 'H'))
	vert = False;

    if (idx < 0
	|| (vert && XtIsManaged(texts[1]))
	|| (!vert && XtIsManaged(vpanes[1]))) {
	Feep();
	return;
    }

    if (vert) {
	nlabel = labels[1];
	ntext = texts[1];
    }
    else {
	nlabel = labels[2];
	ntext = texts[2];
    }
    ChangeTextWindow(texts[idx]);
    labelwindow = labels[idx];

    num_args = 0;
    XtSetArg(args[num_args], XtNinsertPosition, &i_pos);	++num_args;
    XtSetArg(args[num_args], XtNdisplayPosition, &d_pos);	++num_args;
    XtSetArg(args[num_args], XtNtextSource, &source);		++num_args;
    XtSetArg(args[num_args], XtNtextSink, &sink);		++num_args;
    XtSetArg(args[num_args], XtNwidth, &width);			++num_args;
    XtSetArg(args[num_args], XtNheight, &height);		++num_args;
    XtGetValues(w, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNinternalBorderWidth, &bw);	++num_args;
    XtGetValues(XtParent(w), args, num_args);

    if (vert) {
	dimension = (int)height - (((int)bw) << 1);
	num_args = 0;
	XtSetArg(args[num_args], XtNheight, &height);		++num_args;
	XtGetValues(labelwindow, args, num_args);
	dimension -= (int)height;
    }
    else
	dimension = (int)width - (int)bw;

    dimension >>= 1;

    if (dimension <= 0 || dimension < XawTextSinkMaxHeight(sink, 3)) {
	Feep();
	return;
    }

    num_args = 0;
    XtSetArg(args[num_args], XtNlabel, &label_str);		++num_args;
    XtSetArg(args[num_args], XtNleftBitmap, &label_pix);	++num_args;
    XtGetValues(labelwindow, args, num_args);

    if (vert) {
	if (XtIsManaged(texts[2])) {
	    manage[0] = forms[2];
	    manage[1] = texts[2];
	    XtUnmanageChildren(manage, 2);
	    XtUnmanageChild(vpanes[1]);
	}
    }
    else {
	if (XtIsManaged(texts[1])) {
	    manage[0] = forms[1];
	    manage[1] = texts[1];
	    XtUnmanageChildren(manage, 2);
	}
    }

    XawTextDisableRedisplay(texts[0]);
    XawTextDisableRedisplay(ntext);
    if (textwindow == texts[1] || textwindow == texts[2]) {
	num_args = 0;
	XtSetArg(args[num_args], XtNdisplayPosition, d_pos);	++num_args;
	XtSetArg(args[num_args], XtNinsertPosition, i_pos);	++num_args;
	XtSetArg(args[num_args], XtNtextSource, source);	++num_args;
	ChangeTextWindow(texts[0]);
	XtSetValues(textwindow, args, num_args);
	XtSetKeyboardFocus(topwindow, textwindow);

	num_args = 0;
	XtSetArg(args[num_args], XtNlabel, label_str);		++num_args;
	XtSetArg(args[num_args], XtNleftBitmap, label_pix);	++num_args;
	XtSetValues(labelwindow = labels[0], args, num_args);
    }

    num_args = 0;
    XtSetArg(args[num_args], XtNlabel, label_str);		++num_args;
    XtSetArg(args[num_args], XtNleftBitmap, label_pix);		++num_args;
    XtSetValues(nlabel, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNmin, dimension);		++num_args;
    XtSetArg(args[num_args], XtNmax, dimension);		++num_args;
    if (!vert)
	XtSetValues(vpanes[1], args, num_args);
    else
	XtSetValues(ntext, args, num_args);

    manage[0] = XtParent(nlabel);
    manage[1] = ntext;
    XtManageChildren(manage, 2);
    if (!vert)
	XtManageChild(vpanes[1]);

    num_args = 0;
    XtSetArg(args[num_args], XtNmin, 1);			++num_args;
    XtSetArg(args[num_args], XtNmax, 65535);			++num_args;
    if (!vert) {
	XtSetValues(vpanes[1], args, num_args);
	num_args = 0;
    }
    XtSetArg(args[num_args], XtNtextSource, source);		++num_args;
    XtSetArg(args[num_args], XtNdisplayPosition, d_pos);	++num_args;
    XtSetArg(args[num_args], XtNinsertPosition, i_pos);		++num_args;
    item = FindTextSource(source, NULL);
    if (item && (item->flags & WRAP_BIT))
	XtSetArg(args[num_args], XtNwrap, item->wrap);
    else
	XtSetArg(args[num_args], XtNwrap, wrapmodes[WindowIndex(ntext)]);
    ++num_args;
    XtSetValues(ntext, args, num_args);

    UpdateTextProperties(0);

    _XawTextShowPosition((TextWidget)textwindow);
    _XawTextShowPosition((TextWidget)ntext);

    XawTextEnableRedisplay(textwindow);
    XawTextEnableRedisplay(ntext);
}

void
SwitchDirWindow(Bool show)
{
    static int map;	/* There must be one instance of this
			 * variable per top level window */
    Widget manage[2];

    if (!show && XtIsManaged(XtParent(dirwindow))) {
	manage[0] = dirlabel;
	manage[1] = XtParent(dirwindow);
	XtUnmanageChildren(manage, 2);
	XtUnmanageChild(vpanes[1]);

	XtManageChild(vpanes[0]);
	if (map == 2) {
	    Arg args[2];
	    Dimension width, bw;

	    XtSetArg(args[0], XtNwidth, &width);
	    XtGetValues(texts[0], args, 1);
	    XtSetArg(args[0], XtNinternalBorderWidth, &bw);
	    XtGetValues(XtParent(texts[0]), args, 1);
	    width = (width - bw) >> 1;
	    XtSetArg(args[0], XtNmin, width);
	    XtSetArg(args[0], XtNmax, width);
	    XtSetValues(vpanes[0], args, 1);
	    manage[0] = forms[2];
	    manage[1] = texts[2];
	    XtManageChildren(manage, 2);
	    XtManageChild(vpanes[1]);
	    XtSetArg(args[0], XtNmin, 1);
	    XtSetArg(args[0], XtNmax, 65535);
	    XtSetValues(vpanes[0], args, 1);
	}
    }
    else if (show && !XtIsManaged(XtParent(dirwindow))) {
	XtUnmanageChild(vpanes[0]);
	if (XtIsManaged(texts[2])) {
	    manage[0] = forms[2];
	    manage[1] = texts[2];
	    XtUnmanageChildren(manage, 2);
	    map = 2;
	}
	else {
	    map = XtIsManaged(texts[1]);
	    XtManageChild(vpanes[1]);
	}

	manage[0] = dirlabel;
	manage[1] = XtParent(dirwindow);
	XtManageChildren(manage, 2);
    }
}

/*ARGSUSED*/
void
DirWindow(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Arg args[1];
    char path[BUFSIZ + 1], *dir;

    if (XtIsManaged(XtParent(dirwindow)))
	return;

    if (*num_params == 1) {
	strncpy(path, params[0], sizeof(path - 2));
	path[sizeof(path) - 2] = '\0';
    }
    else {
	char *slash;
	xedit_flist_item *item = FindTextSource(XawTextGetSource(textwindow), NULL);

	if (item == NULL || item->source == scratch
	    || (slash = rindex(item->filename, '/')) == NULL)
	    strcpy(path, "./");
	else {
	    int len = slash - item->filename + 1;

	    if (len > sizeof(path) - 2)
		len = sizeof(path) - 2;
	    strncpy(path, item->filename, len);
	    path[len] = '\0';
	}
    }

    dir = ResolveName(path);
    if (dir != NULL) {
	strncpy(path, dir, sizeof(path) - 2);
	path[sizeof(path) - 2] = '\0';
	if (*path && path[strlen(path) - 1] != '/')
	    strcat(path, "/");

	XtSetArg(args[0], XtNlabel, "");
	XtSetValues(dirlabel, args, 1);

	SwitchDirWindow(True);
	DirWindowCB(dirwindow, path, NULL);
    }
    else
	Feep();
}
