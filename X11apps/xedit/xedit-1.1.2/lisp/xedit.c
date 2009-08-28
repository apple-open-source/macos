/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/xedit.c,v 1.25 2003/04/27 18:17:35 tsi Exp $ */

#include "../xedit.h"
#include <X11/Xaw/TextSrcP.h>	/* Needs some private definitions */
#include <X11/Xaw/TextSinkP.h>	/* Also needs private definitions... */
#include <X11/Xmu/Xmu.h>
#define XEDIT_LISP_PRIVATE
#include "xedit.h"
#include <signal.h>

/* Initialize to enter lisp */
#define LISP_SETUP()						\
    int lisp__running = lisp__data.running

/* XXX Maybe should use ualarm or better, setitimer, but one
 *     second seens good enough to check for interrupts */

#define	ENABLE_SIGALRM()					\
    old_sigalrm = signal(SIGALRM, SigalrmHandler);		\
    alarm(1)

#define DISABLE_SIGALRM()					\
    alarm(0);							\
    signal(SIGALRM, old_sigalrm)

/* Enter lisp */
#define LISP_ENTER()						\
    if (!lisp__running) {					\
	lisp__data.running = 1;					\
	XFlush(XtDisplay(textwindow));				\
	ENABLE_SIGALRM();					\
	if (sigsetjmp(lisp__data.jmp, 1) != 0) {		\
	    DISABLE_SIGALRM();					\
	    lisp__data.running = 0;				\
	    return;						\
	}							\
    }

/* Leave lisp */
#define LISP_LEAVE()						\
    if (!lisp__running) {					\
	DISABLE_SIGALRM();					\
	LispTopLevel();						\
	lisp__data.running = 0;					\
    }

/*
 * Types
 */
typedef struct {
    XawTextPosition left, right;
    XrmQuark property;
} EntityInfo;

/*
 * Prototypes
 */
static Bool ControlGPredicate(Display*, XEvent*, XPointer);
static ssize_t WriteToStdout(int, const void*, size_t);
static ssize_t WriteToStderr(int, const void*, size_t);
static ssize_t WrapWrite(Widget, const void*, size_t);
static void XeditUpdateModeInfos(void);
static void XeditPrint(Widget, LispObj*, int);
static void XeditInteractiveCallback(Widget, XtPointer, XtPointer);
static void XeditIndentationCallback(Widget, XtPointer, XtPointer);
static LispObj *XeditCharAt(LispBuiltin*, int);
static LispObj *XeditSearch(LispBuiltin*, XawTextScanDirection);

/*
 * Initialization
 */
#ifdef SIGNALRETURNSINT
static int (*old_sigalrm)(int);
#else
static void (*old_sigalrm)(int);
#endif

EditModeInfo *mode_infos;
Cardinal num_mode_infos;

static LispObj *Oauto_modes, *Oauto_mode, *Osyntax_highlight, *Osyntable_indent;

/* Just to make calling interactive reparse easier */
static LispObj interactive_arguments[4];

static LispObj *justify_modes[4];
static LispObj *wrap_modes[3];
static LispObj *scan_types[6];
static LispObj *scan_directions[2];
static LispObj execute_stream;
static LispString execute_string;
static LispObj result_stream;
static LispString result_string;
static XawTextPropertyList **property_lists;
static Cardinal num_property_lists;

/* Some hacks to (at lest try to) avoid problems reentering Xlib while
 * testing for user interrupts */
static volatile int disable_timeout, request_timeout;

extern int pagesize;

static LispBuiltin xeditbuiltins[] = {
    {LispFunction, Xedit_AddEntity, "add-entity offset length identifier"},
    {LispFunction, Xedit_AutoFill, "auto-fill &optional value"},
    {LispFunction, Xedit_Background, "background &optional color"},
    {LispFunction, Xedit_CharAfter, "char-after &optional offset"},
    {LispFunction, Xedit_CharBefore, "char-before &optional offset"},
    {LispFunction, Xedit_ClearEntities, "clear-entities left right"},
    {LispFunction, Xedit_ConvertPropertyList, "convert-property-list name definition"},
    {LispFunction, Xedit_Font, "font &optional font"},
    {LispFunction, Xedit_Foreground, "foreground &optional color"},
    {LispFunction, Xedit_GotoChar, "goto-char offset"},
    {LispFunction, Xedit_HorizontalScrollbar, "horizontal-scrollbar &optional state"},
    {LispFunction, Xedit_Insert, "insert text"},
    {LispFunction, Xedit_Justification, "justification &optional value"},
    {LispFunction, Xedit_LeftColumn, "left-column &optional left"},
    {LispFunction, Xedit_Point, "point"},
    {LispFunction, Xedit_PointMax, "point-max"},
    {LispFunction, Xedit_PointMin, "point-min"},
    {LispFunction, Xedit_PropertyList, "property-list &optional value"},
    {LispFunction, Xedit_ReadText, "read-text offset length"},
    {LispFunction, Xedit_ReplaceText, "replace-text left right text"},
    {LispFunction, Xedit_RightColumn, "right-column &optional right"},
    {LispFunction, Xedit_Scan, "scan offset type direction &key count include"},
    {LispFunction, Xedit_SearchBackward, "search-backward string &optional offset ignore-case"},
    {LispFunction, Xedit_SearchForward, "search-forward string &optional offset ignore-case"},
    {LispFunction, Xedit_VerticalScrollbar, "vertical-scrollbar &optional state"},
    {LispFunction, Xedit_WrapMode, "wrap-mode &optional value"},

	/* This should be available from elsewhere at some time... */
    {LispFunction, Xedit_XrmStringToQuark, "xrm-string-to-quark string"},
};

/*
 * Implementation
 */
/*ARGUSED*/
static Bool
ControlGPredicate(Display *display, XEvent *event, XPointer arguments)
{
    char buffer[2];

    return ((event->type == KeyPress || event->type == KeyRelease) &&
	    (event->xkey.state & ControlMask) &&
	    XLookupString(&(event->xkey), buffer, sizeof(buffer), NULL, NULL) &&
	    buffer[0] == '\a');
}

/*ARGSUSED*/
static
#ifdef SIGNALRETURNSINT
int
#else
void
#endif
SigalrmHandler(int signum)
{
    XEvent event;

    if (disable_timeout) {
	request_timeout = 1;
	return;
    }

    /* Check if user pressed C-g */
    if (XCheckIfEvent(XtDisplay(textwindow), &event, ControlGPredicate, NULL)) {
	XPutBackEvent(XtDisplay(textwindow), &event);
	alarm(0);
	/* Tell a signal was received, print message for SIGINT */
	LispSignal(SIGINT);
    }
    else
	alarm(1);
#ifdef SIGNALRETURNSINT
    return (0);
#endif
}

static ssize_t
WrapWrite(Widget output, const void *buffer, size_t nbytes)
{
    XawTextBlock block;
    XawTextPosition position;

    disable_timeout = 1;
    position = XawTextGetInsertionPoint(output);
    block.firstPos = 0;
    block.format = FMT8BIT;
    block.length = nbytes;
    block.ptr = (String)buffer;
    XawTextReplace(output, position, position, &block);
    XawTextSetInsertionPoint(output, position + block.length);
    disable_timeout = 0;

    if (request_timeout) {
	XFlush(XtDisplay(output));
	request_timeout = 0;
	SigalrmHandler(SIGALRM);
    }

    return ((ssize_t)nbytes);
}

static ssize_t
WriteToStdout(int fd, const void *buffer, size_t nbytes)
{
    return (WrapWrite(textwindow, buffer, nbytes));
}

static ssize_t
WriteToStderr(int fd, const void *buffer, size_t nbytes)
{
    return (WrapWrite(messwidget, buffer, nbytes));
}

void
LispXeditInitialize(void)
{
    int i;
    char *string;
    LispObj *xedit, *list, *savepackage;

    LispSetFileWrite(Stdout, WriteToStdout);
    LispSetFileWrite(Stderr, WriteToStderr);

    justify_modes[0]	= KEYWORD("LEFT");
    justify_modes[1]	= KEYWORD("RIGHT");
    justify_modes[2]	= KEYWORD("CENTER");
    justify_modes[3]	= KEYWORD("FULL");

    wrap_modes[0]	= KEYWORD("NEVER");
    wrap_modes[1]	= KEYWORD("LINE");
    wrap_modes[2]	= KEYWORD("WORD");

    scan_types[0]	= KEYWORD("POSITIONS");
    scan_types[1]	= KEYWORD("WHITE-SPACE");
    scan_types[2]	= KEYWORD("EOL");
    scan_types[3]	= KEYWORD("PARAGRAPH");
    scan_types[4]	= KEYWORD("ALL");
    scan_types[5]	= KEYWORD("ALPHA-NUMERIC");

    scan_directions[0]	= justify_modes[0];
    scan_directions[1]	= justify_modes[1];

    /* Remember value of current package */
    savepackage = PACKAGE;

    /* Create the XEDIT package */
    xedit = LispNewPackage(STRING("XEDIT"), NIL);

    /* Update list of packages */
    PACK = CONS(xedit, PACK);

    /* Temporarily switch to the XEDIT package */
    lisp__data.pack = lisp__data.savepack = xedit->data.package.package;
    PACKAGE = xedit;

    /* Add XEDIT builtin functions */
    for (i = 0; i < sizeof(xeditbuiltins) / sizeof(xeditbuiltins[0]); i++)
	LispAddBuiltinFunction(&xeditbuiltins[i]);

    /* Create these objects in the xedit package */
    Oauto_modes		= STATIC_ATOM("*AUTO-MODES*");
    Oauto_mode		= STATIC_ATOM("AUTO-MODE");
    Osyntax_highlight	= STATIC_ATOM("SYNTAX-HIGHLIGHT");
    Osyntable_indent	= STATIC_ATOM("SYNTABLE-INDENT");

    /*  Import symbols from the LISP and EXT packages */
    for (list = PACK; CONSP(list); list = CDR(list)) {
	string = THESTR(CAR(list)->data.package.name);
	if (strcmp(string, "LISP") == 0 || strcmp(string, "EXT") == 0)
	    LispUsePackage(CAR(list));
    }

    /* Restore previous package */
    lisp__data.pack = savepackage->data.package.package;
    PACKAGE = savepackage;

    /* Initialize helper static objects used when executing expressions */
    execute_stream.type = LispStream_t;
    execute_stream.data.stream.source.string = &execute_string;
    execute_stream.data.stream.pathname = NIL;
    execute_stream.data.stream.type = LispStreamString;
    execute_stream.data.stream.readable = 1;
    execute_stream.data.stream.writable = 0;
    execute_string.output = 0;
    result_stream.type = LispStream_t;
    result_stream.data.stream.source.string = &result_string;
    result_stream.data.stream.pathname = NIL;
    result_stream.data.stream.type = LispStreamString;
    result_stream.data.stream.readable = 0;
    result_stream.data.stream.writable = 1;
    result_string.string = XtMalloc(pagesize);
    result_string.space = pagesize;

    /* Initialize interactive edition function arguments */
    /* first argument is syntax table */
    interactive_arguments[0].type = LispCons_t;
    interactive_arguments[0].data.cons.cdr = &interactive_arguments[1];
    /* second argument is where to start reparsing */
    interactive_arguments[1].type = LispCons_t;
    interactive_arguments[1].data.cons.cdr = &interactive_arguments[2];
    /* third argument is where to stop reparsing */
    interactive_arguments[2].type = LispCons_t;
    interactive_arguments[2].data.cons.cdr = &interactive_arguments[3];
    /* fourth argument is interactive flag */
    interactive_arguments[3].type = LispCons_t;
    interactive_arguments[3].data.cons.car = T;
    interactive_arguments[3].data.cons.cdr = NIL;

    /* Load extra functions and data type definitions */
    EXECUTE("(require \"xedit\")");


    /*
     *	This assumes that the *auto-modes* variable is a list where every
     * item has the format:
     *	    (regexp string-desc load-file-desc . symbol-name)
     *	Minimal error checking is done.
     */

    if (Oauto_modes->data.atom->a_object) {
	LispObj *desc, *modes = Oauto_modes->data.atom->property->value;

	for (; CONSP(modes); modes = CDR(modes)) {
	    list = CAR(modes);

	    desc = NIL;
	    for (i = 0; i < 3 && CONSP(list); i++, list = CDR(list)) {
		if (i == 1)
		    desc = CAR(list);
	    }
	    if (i == 3 && STRINGP(desc)) {
		mode_infos = (EditModeInfo*)
		    XtRealloc((XtPointer)mode_infos, sizeof(EditModeInfo) *
			      (num_mode_infos + 1));
		mode_infos[num_mode_infos].desc = XtNewString(THESTR(desc));
		mode_infos[num_mode_infos].symbol = list;
		mode_infos[num_mode_infos].syntax = NULL;
		++num_mode_infos;
	    }
	}
    }
}

static void
XeditUpdateModeInfos(void)
{
    int i;

    for (i = 0; i < num_mode_infos; i++) {
	if (mode_infos[i].symbol &&
	    mode_infos[i].syntax == NULL &&
	    XSYMBOLP(mode_infos[i].symbol) &&
	    mode_infos[i].symbol->data.atom->a_object)
	    mode_infos[i].syntax =
		mode_infos[i].symbol->data.atom->property->value;
    }
}

void
XeditLispExecute(Widget output, XawTextPosition left, XawTextPosition right)
{
    GC_ENTER();
    LISP_SETUP();
    int alloced, return_count;
    XawTextBlock block;
    XawTextPosition position;
    char *string, *ptr;
    LispObj *result, *code, *_cod, *returns;

    LISP_ENTER();

    position = left;
    XawTextSourceRead(XawTextGetSource(textwindow), left, &block, right - left);
    if (block.length < right - left) {
	alloced = 1;
	string = ptr = LispMalloc(right - left);
	memcpy(ptr, block.ptr, block.length);
	position = left + block.length;
	ptr += block.length;
	for (; position < right;) {
	    XawTextSourceRead(XawTextGetSource(textwindow),
			      position, &block, right - position);
	    memcpy(ptr, block.ptr, block.length);
	    position += block.length;
	    ptr += block.length;
	}
    }
    else {
	alloced = 0;
	string = block.ptr;
    }

    execute_string.string = string;
    execute_string.length = right - left;
    execute_string.input = 0;
    LispPushInput(&execute_stream);
    _cod = COD;
    result = NIL;
    if ((code = LispRead()) != NULL)
	result = EVAL(code);
    COD = _cod;
    LispPopInput(&execute_stream);

    returns = NIL;
    if (RETURN_COUNT > 0) {
	GC_PROTECT(result);
	returns = _cod = CONS(RETURN(0), NIL);
	GC_PROTECT(returns);
	for (return_count = 1; return_count < RETURN_COUNT; return_count++) {
	    RPLACD(_cod, CONS(RETURN(return_count), NIL));
	    _cod = CDR(_cod);
	}
    }
    LispFflush(Stdout);
    LispUpdateResults(code, result);
    if (RETURN_COUNT >= 0) {
	XeditPrint(output, result, 1);
	for (; CONSP(returns); returns = CDR(returns))
	    XeditPrint(output, CAR(returns), 0);
    }

    if (alloced)
	LispFree(string);
    GC_LEAVE();

    LISP_LEAVE();
}

static void
XeditPrint(Widget output, LispObj *object, int newline)
{
    XawTextBlock block;
    XawTextPosition position;

    result_string.length = result_string.output = 0;
    if (newline) {
	position = XawTextGetInsertionPoint(output);
	if (position != XawTextSourceScan(XawTextGetSource(output),
					  position, XawstEOL,
					  XawsdLeft, 1, False))
	    LispSputc(&result_string, '\n');
    }
    LispWriteObject(&result_stream, object);
    LispSputc(&result_string, '\n');

    position = XawTextGetInsertionPoint(output);
    block.firstPos = 0;
    block.format = FMT8BIT;
    block.length = result_string.length;
    block.ptr = result_string.string;
    XawTextReplace(output, position, position, &block);
    XawTextSetInsertionPoint(output, position + block.length);
}

/*
 *  This function is defined here to avoid exporting all the lisp interfaces
 * to the core xedit code.
 */
void
XeditLispSetEditMode(xedit_flist_item *item, LispObj *symbol)
{
    GC_ENTER();
    LISP_SETUP();
    LispObj *syntax, *name;

    item->xldata = (XeditLispData*)XtCalloc(1, sizeof(XeditLispData));

    LISP_ENTER();

    /* Create an object that represents the buffer filename.
     * Note that the entire path is passed to the auto-mode
     * function, so that directory names may be also be used
     * when determining a file type. */
    name = STRING(item->filename);
    GC_PROTECT(name);

    /*  Call the AUTO-MODE function to check if there is a
     * syntax definition for the file being loaded */
    if (symbol == NULL)
	syntax = APPLY1(Oauto_mode, name);
    else
	syntax = APPLY2(Oauto_mode, name, symbol);

    /* Don't need the name object anymore */
    GC_LEAVE();

    if (syntax != NIL) {
	Arg arg[1];
	LispObj arguments;
	XawTextPropertyList *property_list;

	item->xldata->syntax = syntax;

	/* Apply the syntax highlight to the current buffer */
	arguments.type = LispCons_t;
	arguments.data.cons.car = syntax;
	arguments.data.cons.cdr = NIL;
	LispFuncall(Osyntax_highlight, &arguments, 1);

	/*  The previous call added the property list to the widget,
	 * remember it when switching sources. */
	XtSetArg(arg[0], XawNtextProperties, &property_list);
	XtGetValues(XawTextGetSink(textwindow), arg, 1);
	item->properties = property_list;

	/* Add callback for interactive changes */
	XtAddCallback(item->source, XtNpropertyCallback,
		      XeditInteractiveCallback, item->xldata);

	/* Update information as a new file may have been loaded */
	XeditUpdateModeInfos();
    }
    else
	item->properties = NULL;

    LISP_LEAVE();
}

void
XeditLispUnsetEditMode(xedit_flist_item *item)
{
    if (item->xldata) {
	XtRemoveCallback(item->source, XtNpropertyCallback,
			 XeditInteractiveCallback, item->xldata);
	XtFree((XtPointer)item->xldata);
	item->xldata = NULL;
    }
}

#define MAX_INFOS	32
/*
 *  This callback tries to do it's best in generating correct output while
 * also doing minimal work/redrawing of the screen. It probably will fail
 * for some syntax-definitions, or will just not properly repaint the
 * screen. In the later case, just press Ctrl+L.
 *  There isn't yet any command to force reparsing of some regions, and if
 * the parser becomes confused, you may need to go to a line, press a space
 * and undo, just to force it to reparse the line, and possibly some extra
 * lines until the parser thinks the display is in sync.
 *  Sometimes it will repaint a lot more of text than what is being requested
 * by this callback, this should be fixed at some time, as for certain cases
 * it is also required some redesign in the Xaw interface.
 */
static void
XeditInteractiveCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    LISP_SETUP();
    XeditLispData *data = (XeditLispData*)client_data;
    LispObj *syntax = data->syntax;
    XawTextPropertyInfo *info = (XawTextPropertyInfo*)call_data;
    LispObj *result, *syntable;
    XawTextAnchor *anchor;
    XawTextEntity *entity;
    XawTextPosition first, last, left, right, begin, next, tmp, position;
    int i, j, indent;
    TextSrcObject src = (TextSrcObject)w;
    EntityInfo oinfo[MAX_INFOS], ninfo[MAX_INFOS];
    XrmQuark props[MAX_INFOS];
    int num_oinfo, num_ninfo, num_props;
    XmuScanline *clip, *oclip, *nclip;
    XmuSegment segment, *seg;

    if (data->disable_highlight)
	return;

    LISP_ENTER();

    first = XawTextSourceScan(w, 0, XawstAll, XawsdLeft, 1, True);
    last = XawTextSourceScan(w, 0, XawstAll, XawsdRight, 1, True);

    left = info->left;
    right = left + info->block->length;

    /* For now, only call the indent hook if a single character was typed */
    indent = (info->right == left) && (right == left + 1);

    /* Always reparse full lines */
    left = begin = XawTextSourceScan(w, left, XawstEOL, XawsdLeft, 1, False);
    right = next = XawTextSourceScan(w, right, XawstEOL, XawsdRight, 1, False);


    /*  Check properties in the modified text. If a complex nested syntax
     * table was parsed, the newline has it's default property, so, while
     * the newline has a property, backup a line to make sure everything is
     * properly parsed.
     *  Maybe should limit the number of backuped lines, but if the parsing
     * becomes noticeable slow, better to rethink the syntax definition. */
    while (left > first) {
	position = XawTextSourceScan(w, left, XawstEOL, XawsdLeft, 1, True);
	if (XawTextSourceAnchorAndEntity(w, position, &anchor, &entity))
	    left = XawTextSourceScan(w, left, XawstEOL, XawsdLeft, 2, False);
	else
	    break;
    }

    /*	While the newline after the right position has a "hidden" property,
     * keep incrementing a line to be reparsed. */
    while (right < last) {
	if (XawTextSourceAnchorAndEntity(w, right, &anchor, &entity))
	    right = XawTextSourceScan(w, right, XawstEOL, XawsdRight, 2, False);
	else
	    break;
    }

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define STORE_STATE(count, info, from, to)				\
    (count) = 0;							\
    if ((anchor = XawTextSourceFindAnchor(w, (from))) != NULL) {	\
	entity = anchor->entities;					\
	/* Find first entity in the region to parse */			\
	while (entity &&						\
	       anchor->position + entity->offset + entity->length <=	\
	       (from))							\
	    entity = entity->next;					\
	/* Loop storing information */					\
	while (entity &&						\
	    (position = anchor->position + entity->offset) < (to)) {	\
	    (info)[(count)].left = MAX(position, (from));		\
	    position += entity->length;					\
	    (info)[(count)].right = MIN(position, (to));		\
	    (info)[(count)].property = entity->property;		\
	    /* If the changes are so complex, user need press Ctrl+L */	\
	    if (++(count) >= MAX_INFOS)					\
		break;							\
	    if ((entity = entity->next) == NULL &&			\
		(anchor = XawTextSourceNextAnchor(w, anchor)) != NULL)	\
		entity = anchor->entities;				\
	}								\
    }

    /* Remember old state */
    STORE_STATE(num_oinfo, oinfo, begin, right);

    /* Reparse the lines in the modified/edited range of text */
    interactive_arguments[0].data.cons.car = syntax;
    interactive_arguments[1].data.cons.car = FIXNUM(left);
    interactive_arguments[2].data.cons.car = FIXNUM(right);
    result = APPLY(Osyntax_highlight, &interactive_arguments[0]);
    /* Indent table is the second return value */
    if (RETURN_COUNT)
	syntable = RETURN(0);
    else
	syntable = NIL;

    /* This normally is the same value as right, but the parser may have
     * continued when the syntax table stack did not finish. */
    if (FIXNUMP(result))
	right = FIXNUM_VALUE(result);

    LISP_LEAVE();

    /* Check what have changed */
    STORE_STATE(num_ninfo, ninfo, begin, right);

    /* Initialize to redraw everything. */
    clip = XmuNewScanline(0, begin, right);

#define CLIP_MASK(mask, from, to)					\
    if ((from) < (to)) {						\
	segment.x1 = (from);						\
	segment.x2 = (to);						\
	XmuScanlineOrSegment((mask), &segment);				\
    }

    oclip = XmuNewScanline(0, 0, 0);
    nclip = XmuNewScanline(0, 0, 0);

#define CLIP_DEFAULT(mask, from, info, num_info)			\
    for (tmp = (from), i = 0; i < (num_info); i++) {			\
	CLIP_MASK((mask), tmp, (info)[i].left);				\
	tmp = (info)[i].right;						\
    }

    /* First generate masks of regions with the default property */
    CLIP_DEFAULT(oclip, begin, oinfo, num_oinfo);
    CLIP_DEFAULT(nclip, begin, ninfo, num_ninfo);

    /* Store unchanged region in oclip */
    XmuScanlineAnd(oclip, nclip);

    /* Don't need to redraw the region in oclip */
    XmuScanlineXor(clip, oclip);

#define LIST_PROPERTIES(prop, num_prop, info, num_info)			\
    (num_prop) = 0;							\
    for (i = 0; i < (num_info); i++) {					\
	for (j = 0; j < (num_prop); j++)				\
	    if ((prop)[j] == (info)[i].property)			\
		break;							\
	if (j == (num_prop))						\
	    (prop)[(num_prop)++] = (info)[i].property;			\
    }

    /* Prepare to generate masks of regions of text with defined properties */
    LIST_PROPERTIES(props, num_props, oinfo, num_oinfo);

#define CLIP_PROPERTY(mask, prop, info, num_info)			\
    for (j = 0; j < (num_info); j++) {					\
	if ((info)[j].property == (prop)) {				\
	    CLIP_MASK((mask), (info)[j].left, (info)[j].right);		\
	}								\
    }

    /* Only care about the old properties, new ones need to be redrawn */
    for (i = 0; i < num_props; i++) {
	XrmQuark property = props[i];

	/* Reset oclip and nclip */
	XmuScanlineXor(oclip, oclip);
	XmuScanlineXor(nclip, nclip);

	/* Generate masks */
	CLIP_PROPERTY(oclip, property, oinfo, num_oinfo);
	CLIP_PROPERTY(nclip, property, ninfo, num_ninfo);

	/* Store unchanged region in oclip */
	XmuScanlineAnd(oclip, nclip);

	/* Don't need to redraw the region in oclip */
	XmuScanlineXor(clip, oclip);
	XmuOptimizeScanline(clip);
    }

    XmuDestroyScanline(oclip);
    XmuDestroyScanline(nclip);

    /* Tell Xaw that need update some regions */
    for (seg = clip->segment; seg; seg = seg->next) {
	for (i = 0; i < src->textSrc.num_text; i++)
	    /* This really should have an exported interface... */
	    _XawTextNeedsUpdating((TextWidget)(src->textSrc.text[i]),
				  seg->x1, seg->x2 + (seg->x2 > next));
    }
    XmuDestroyScanline(clip);

    data->syntable = syntable;
    /* XXX check lisp__running to know if at the toplevel parsing state */
    if (indent && syntable != NIL && !lisp__running &&
	/* Doing an undo, probably will need an exported interface for this
	 * case. Should not change the text now. */
	(!src->textSrc.enable_undo || !src->textSrc.undo_state))
	XtAddCallback(textwindow, XtNpositionCallback,
		      XeditIndentationCallback, data);
}

/*
 * This callback is called if the syntax table where the cursor is located
 * defines an indentation function.
 */
static void
XeditIndentationCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    LISP_SETUP();
    LispObj *indentp;
    XeditLispData *data = (XeditLispData*)client_data;

    data->disable_highlight = True;
    XtRemoveCallback(w, XtNpositionCallback, XeditIndentationCallback, data);

    LISP_ENTER();

    /* Get pointer to indentation function */
    indentp = APPLY1(Osyntable_indent, data->syntable);

    /* Execute indentation function */
    if (indentp != NIL)
	APPLY2(indentp, data->syntax, data->syntable);

    data->disable_highlight = False;

    LISP_LEAVE();
}

/************************************************************************
 * Builtin functions
 ************************************************************************/
LispObj *
Xedit_AddEntity(LispBuiltin *builtin)
/*
 add-entity offset length identifier
 */
{
    LispObj *offset, *length, *identifier;

    identifier = ARGUMENT(2);
    length = ARGUMENT(1);
    offset = ARGUMENT(0);

    CHECK_INDEX(offset);
    CHECK_INDEX(length);
    CHECK_LONGINT(identifier);

    return (XawTextSourceAddEntity(XawTextGetSource(textwindow), 0, 0, NULL,
				   FIXNUM_VALUE(offset), FIXNUM_VALUE(length),
				   LONGINT_VALUE(identifier)) ? T : NIL);
}

LispObj *
Xedit_AutoFill(LispBuiltin *builtin)
/*
 auto-fill &optional value
 */
{
    Arg arg[1];
    Boolean state;

    LispObj *value;

    value = ARGUMENT(0);

    if (value != UNSPEC) {
	XtSetArg(arg[0], XtNautoFill, value == NIL ? False : True);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNautoFill, &state);
	XtGetValues(textwindow, arg, 1);
	value = state ? T : NIL;
    }

    return (value);
}

LispObj *
Xedit_Background(LispBuiltin *builtin)
/*
 background &optional color
 */
{
    Pixel pixel;
    Arg arg[1];
    XrmValue from, to;

    LispObj *color;

    color = ARGUMENT(0);

    if (color != UNSPEC) {
	CHECK_STRING(color);

	from.size = STRLEN(color);
	from.addr = (XtPointer)THESTR(color);
	to.size = sizeof(Pixel);
	to.addr = (XtPointer)&pixel;

	if (!XtConvertAndStore(XawTextGetSink(textwindow),
			       XtRString, &from, XtRPixel, &to))
	    LispDestroy("cannot convert %s to Pixel", STROBJ(color));

	XtSetArg(arg[0], XtNbackground, pixel);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	from.size = sizeof(Pixel);
	from.addr = (XtPointer)&pixel;
	to.size = 0;
	to.addr = NULL;

	XtSetArg(arg[0], XtNbackground, &pixel);
	XtGetValues(XawTextGetSink(textwindow), arg, 1);
	/* This cannot fail */
	XtConvertAndStore(textwindow, XtRPixel, &from, XtRString, &to);

	color = STRING(to.addr);
    }

    return (color);
}

static LispObj *
XeditCharAt(LispBuiltin *builtin, int before)
{
    Widget source = XawTextGetSource(textwindow);
    XawTextPosition first, point, last;
    XawTextBlock block;

    LispObj *offset;

    offset = ARGUMENT(0);
    if (offset != UNSPEC) {
	CHECK_INDEX(offset);
    }

    first = XawTextSourceScan(source, 0, XawstAll, XawsdLeft, 1, True);
    if (FIXNUMP(offset))
	point = FIXNUM_VALUE(offset);
    else
	point = XawTextGetInsertionPoint(textwindow);
    if (before && point > first) {
	XawTextPosition position =
	    XawTextSourceScan(source, point, XawstPositions, XawsdLeft, 1, True);

	if (position < point)
	    point = position;
	else
	    return (NIL);
    }
    last = XawTextSourceScan(source, 0, XawstAll, XawsdRight, 1, True);

    if (point < first || point > last)
	return (NIL);

    XawTextSourceRead(source, point, &block, 1);

    return (block.length ? SCHAR(*(unsigned char*)block.ptr) : NIL);
}

LispObj *
Xedit_CharAfter(LispBuiltin *builtin)
/*
 char-after &optional offset
 */
{
    return (XeditCharAt(builtin, 0));
}

LispObj *
Xedit_CharBefore(LispBuiltin *builtin)
/*
 char-before &optional offset
 */
{
    return (XeditCharAt(builtin, 1));
}

LispObj *
Xedit_ClearEntities(LispBuiltin *builtin)
/*
 clear-entities left right
 */
{
    LispObj *left, *right;

    right = ARGUMENT(1);
    left = ARGUMENT(0);

    CHECK_INDEX(left);
    CHECK_INDEX(right);

    XawTextSourceClearEntities(XawTextGetSource(textwindow),
			       FIXNUM_VALUE(left), FIXNUM_VALUE(right));

    return (T);
}

LispObj *
Xedit_ConvertPropertyList(LispBuiltin *builtin)
/*
 convert-property-list name definition
 */
{
    LispObj *result;
    XawTextPropertyList *property_list;

    LispObj *name, *definition;

    definition = ARGUMENT(1);
    name = ARGUMENT(0);

    CHECK_STRING(name);
    CHECK_STRING(definition);

    result = NIL;
    property_list = XawTextSinkConvertPropertyList(THESTR(name),
						   THESTR(definition),
						   topwindow->core.screen,
						   topwindow->core.colormap,
						   topwindow->core.depth);

    if (property_list) {
	Cardinal i;

	for (i = 0; i < num_property_lists; i++)
	    /* Check if a new property list was created */
	    if (property_lists[i]->identifier == property_list->identifier)
		break;

	/* Remember this pointer when asked back for it */
	if (i == num_property_lists) {
	    property_lists = (XawTextPropertyList**)
		XtRealloc((XtPointer)property_lists,
			  sizeof(XawTextPropertyList) *
			  (num_property_lists + 1));
	    property_lists[num_property_lists++] = property_list;
	}
	result = INTEGER(property_list->identifier);
    }

    return (result);
}

LispObj *
Xedit_Font(LispBuiltin *builtin)
/*
 font &optional font
 */
{
    XFontStruct *font_struct;
    Arg arg[1];
    XrmValue from, to;

    LispObj *font;

    font = ARGUMENT(0);

    if (font != UNSPEC) {
	CHECK_STRING(font);

	from.size = STRLEN(font);
	from.addr = (XtPointer)THESTR(font);
	to.size = sizeof(XFontStruct*);
	to.addr = (XtPointer)&font_struct;

	if (!XtConvertAndStore(textwindow, XtRString, &from, XtRFontStruct, &to))
	    LispDestroy("cannot convert %s to FontStruct", STROBJ(font));

	XtSetArg(arg[0], XtNfont, font_struct);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	from.size = sizeof(XFontStruct*);
	from.addr = (XtPointer)&font_struct;
	to.size = 0;
	to.addr = NULL;

	XtSetArg(arg[0], XtNfont, &font_struct);
	XtGetValues(XawTextGetSink(textwindow), arg, 1);
	/* This cannot fail */
	XtConvertAndStore(textwindow, XtRFontStruct, &from, XtRString, &to);

	font = STRING(to.addr);
    }

    return (font);
}

LispObj *
Xedit_Foreground(LispBuiltin *builtin)
/*
 foreground &optional color
 */
{
    Pixel pixel;
    Arg arg[1];
    XrmValue from, to;

    LispObj *color;

    color = ARGUMENT(0);

    if (color != UNSPEC) {
	CHECK_STRING(color);

	from.size = STRLEN(color);
	from.addr = (XtPointer)THESTR(color);
	to.size = sizeof(Pixel);
	to.addr = (XtPointer)&pixel;

	if (!XtConvertAndStore(XawTextGetSink(textwindow),
			       XtRString, &from, XtRPixel, &to))
	    LispDestroy("cannot convert %s to Pixel", STROBJ(color));

	XtSetArg(arg[0], XtNforeground, pixel);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	from.size = sizeof(Pixel);
	from.addr = (XtPointer)&pixel;
	to.size = 0;
	to.addr = NULL;

	XtSetArg(arg[0], XtNforeground, &pixel);
	XtGetValues(XawTextGetSink(textwindow), arg, 1);
	/* This cannot fail */
	XtConvertAndStore(textwindow, XtRPixel, &from, XtRString, &to);

	color = STRING(to.addr);
    }

    return (color);
}

LispObj *
Xedit_GotoChar(LispBuiltin *builtin)
/*
 goto-char offset
 */
{
    LispObj *offset;
    XawTextPosition point;

    offset = ARGUMENT(0);

    CHECK_INDEX(offset);
    XawTextSetInsertionPoint(textwindow, FIXNUM_VALUE(offset));
    point = XawTextGetInsertionPoint(textwindow);
    if (point != FIXNUM_VALUE(offset))
	offset = FIXNUM(point);

    return (offset);
}

LispObj *
Xedit_HorizontalScrollbar(LispBuiltin *builtin)
/*
 horizontal-scrollbar &optional state
 */
{
    Arg arg[1];
    XawTextScrollMode scroll;

    LispObj *state;

    state = ARGUMENT(0);

    if (state != UNSPEC) {
	scroll = state == NIL ? XawtextScrollNever : XawtextScrollAlways;
	XtSetArg(arg[0], XtNscrollHorizontal, scroll);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNscrollHorizontal, &scroll);
	XtGetValues(textwindow, arg, 1);
	state = scroll == XawtextScrollAlways ? T : NIL;
    }

    return (state);
}

LispObj *
Xedit_Insert(LispBuiltin *builtin)
/*
 insert text
 */
{
    XawTextPosition point = XawTextGetInsertionPoint(textwindow);
    XawTextBlock block;

    LispObj *text;

    text = ARGUMENT(0);

    CHECK_STRING(text);
    
    block.firstPos = 0;
    block.format = FMT8BIT;
    block.length = STRLEN(text);
    block.ptr = THESTR(text);
    XawTextReplace(textwindow, point, point, &block);
    XawTextSetInsertionPoint(textwindow, point + block.length);

    return (text);
}

LispObj *
Xedit_Justification(LispBuiltin *builtin)
/*
 justification &optional value
 */
{
    int i;
    Arg arg[1];
    XawTextJustifyMode justify;

    LispObj *value;

    value = ARGUMENT(0);

    if (value != UNSPEC) {
	for (i = 0; i < 4; i++)
	    if (value == justify_modes[i])
		break;
	if (i >= 4)
	    LispDestroy("%s: argument must be "
			":LEFT, :RIGHT, :CENTER, or :FULL, not %s",
			STRFUN(builtin), STROBJ(value));
	XtSetArg(arg[0], XtNjustifyMode, (XawTextJustifyMode)i);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNjustifyMode, &justify);
	XtGetValues(textwindow, arg, 1);
	i = (int)justify;
	if (i <= 0 || i >= 4)
	    i = 0;
	value = justify_modes[i];
    }

    return (value);
}

LispObj *
Xedit_LeftColumn(LispBuiltin *builtin)
/*
 left-column &optional left
 */
{
    short left;
    Arg arg[1];

    LispObj *oleft;

    oleft = ARGUMENT(0);

    if (oleft != UNSPEC) {
	CHECK_INDEX(oleft);
	if (FIXNUM_VALUE(oleft) >= 32767)
	    left = 32767;
	else
	    left = FIXNUM_VALUE(oleft);

	XtSetArg(arg[0], XtNleftColumn, left);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNleftColumn, &left);
	XtGetValues(textwindow, arg, 1);

	oleft = FIXNUM((long)left);
    }

    return (oleft);
}

LispObj *
Xedit_Point(LispBuiltin *builtin)
/*
 point
 */
{
    return (FIXNUM(XawTextGetInsertionPoint(textwindow)));
}

LispObj *
Xedit_PointMax(LispBuiltin *builtin)
/*
 point-max
 */
{
    return (FIXNUM(XawTextSourceScan(XawTextGetSource(textwindow), 0,
				     XawstAll, XawsdRight, 1, True)));
}

LispObj *
Xedit_PointMin(LispBuiltin *builtin)
/*
 point-min
 */
{
    return (FIXNUM(XawTextSourceScan(XawTextGetSource(textwindow), 0,
				     XawstAll, XawsdLeft, 1, True)));
}

LispObj *
Xedit_PropertyList(LispBuiltin *builtin)
/*
 property-list &optional value
 */
{
    Arg arg[1];
    XawTextPropertyList *property_list;

    LispObj *value;

    value = ARGUMENT(0);

    if (value != UNSPEC) {
	Cardinal i;
	XrmQuark quark;

	CHECK_LONGINT(value);
	property_list = NULL;
	quark = LONGINT_VALUE(value);
	for (i = 0; i < num_property_lists; i++)
	    if (property_lists[i]->identifier == quark) {
		property_list = property_lists[i];
		break;
	    }

	if (property_list) {
	    XtSetArg(arg[0], XawNtextProperties, property_list);
	    XtSetValues(XawTextGetSink(textwindow), arg, 1);
	}
	else
	    /* Maybe should generate an error here */
	    value = NIL;
    }
    else {
	XtSetArg(arg[0], XawNtextProperties, &property_list);
	XtGetValues(XawTextGetSink(textwindow), arg, 1);
	if (property_list)
	    value = INTEGER(property_list->identifier);
    }

    return (value);
}

LispObj *
Xedit_ReadText(LispBuiltin *builtin)
/*
 read-text offset length
 */
{
    XawTextPosition last = XawTextSourceScan(XawTextGetSource(textwindow), 0,
					     XawstAll, XawsdRight, 1, True);
    XawTextPosition from, to, len;
    XawTextBlock block;
    char *string, *ptr;

    LispObj *offset, *length;

    length = ARGUMENT(1);
    offset = ARGUMENT(0);

    CHECK_INDEX(offset);
    CHECK_INDEX(length);

    from = FIXNUM_VALUE(offset);
    to = from + FIXNUM_VALUE(length);
    if (from > last)
	from = last;
    if (to > last)
	to = last;

    if (from == to)
	return (STRING(""));

    len = to - from;
    string = LispMalloc(len);

    for (ptr = string; from < to;) {
	XawTextSourceRead(XawTextGetSource(textwindow), from, &block, to - from);
	memcpy(ptr, block.ptr, block.length);
	ptr += block.length;
	from += block.length;
    }

    return (LSTRING2(string, len));
}

LispObj *
Xedit_ReplaceText(LispBuiltin *builtin)
/*
 replace-text left right text
 */
{
    XawTextPosition last = XawTextSourceScan(XawTextGetSource(textwindow), 0,
					     XawstAll, XawsdRight, 1, True);
    XawTextPosition left, right;
    XawTextBlock block;

    LispObj *oleft, *oright, *text;

    text = ARGUMENT(2);
    oright = ARGUMENT(1);
    oleft = ARGUMENT(0);

    CHECK_INDEX(oleft);
    CHECK_INDEX(oright);
    CHECK_STRING(text);

    left = FIXNUM_VALUE(oleft);
    right = FIXNUM_VALUE(oright);
    if (left > last)
	left = last;
    if (left > right)
	right = left;
    else if (right > last)
	right = last;

    block.firstPos = 0;
    block.format = FMT8BIT;
    block.length = STRLEN(text);
    block.ptr = THESTR(text);
    XawTextReplace(textwindow, left, right, &block);

    return (text);
}

LispObj *
Xedit_RightColumn(LispBuiltin *builtin)
/*
 right-column &optional right
 */
{
    short right;
    Arg arg[1];

    LispObj *oright;

    oright = ARGUMENT(0);

    if (oright != UNSPEC) {
	CHECK_INDEX(oright);
	if (FIXNUM_VALUE(oright) >= 32767)
	    right = 32767;
	else
	    right = FIXNUM_VALUE(oright);

	XtSetArg(arg[0], XtNrightColumn, right);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNrightColumn, &right);
	XtGetValues(textwindow, arg, 1);

	oright = FIXNUM(right);
    }

    return (oright);
}

LispObj *
Xedit_Scan(LispBuiltin *builtin)
/*
 scan offset type direction &key count include
 */
{
    int i;
    XawTextPosition offset;
    XawTextScanType type;
    XawTextScanDirection direction;
    int count;

    LispObj *ooffset, *otype, *odirection, *ocount, *include;

    include = ARGUMENT(4);
    if (include == UNSPEC)
	include = NIL;
    ocount = ARGUMENT(3);
    odirection = ARGUMENT(2);
    otype = ARGUMENT(1);
    ooffset = ARGUMENT(0);

    CHECK_INDEX(ooffset);
    offset = FIXNUM_VALUE(ooffset);

    for (i = 0; i < 2; i++)
	if (odirection == scan_directions[i])
	    break;
    if (i >= 2)
	LispDestroy("%s: direction must be "
		    ":LEFT or :RIGHT, not %s",
		    STRFUN(builtin), STROBJ(odirection));
    direction = (XawTextScanDirection)i;

    for (i = 0; i < 6; i++)
	if (otype == scan_types[i])
	    break;
    if (i >= 6)
	LispDestroy("%s: direction must be "
		    ":POSITIONS, :WHITE-SPACE, :EOL, "
		    ":PARAGRAPH, :ALL, or :ALPHA-NUMERIC, not %s",
		    STRFUN(builtin), STROBJ(otype));
    type = (XawTextScanType)i;

    if (ocount == UNSPEC)
	count = 1;
    else {
	CHECK_INDEX(ocount);
	count = FIXNUM_VALUE(ocount);
    }

    offset = XawTextSourceScan(XawTextGetSource(textwindow),
			       offset, type, direction, count,
			       include != NIL);

    return (FIXNUM(offset));
}

static LispObj *
XeditSearch(LispBuiltin *builtin, XawTextScanDirection direction)
{
    XawTextBlock block;
    XawTextPosition position;

    LispObj *string, *offset, *ignore_case;

    ignore_case = ARGUMENT(2);
    offset = ARGUMENT(1);
    string = ARGUMENT(0);

    CHECK_STRING(string);
    if (offset != UNSPEC) {
	CHECK_INDEX(offset);
	position = FIXNUM_VALUE(offset);
    }
    else
	position = XawTextGetInsertionPoint(textwindow);

    block.firstPos = (ignore_case != UNSPEC && ignore_case != NIL) ? 1 : 0;
    block.format = FMT8BIT;
    block.length = STRLEN(string);
    block.ptr = THESTR(string);
    position = XawTextSourceSearch(XawTextGetSource(textwindow),
				   position, direction, &block);

    return (position != XawTextSearchError ? FIXNUM(position) : NIL);
}


LispObj *
Xedit_SearchBackward(LispBuiltin *builtin)
/*
 search-backward string &optional offset ignore-case
 */
{
    return (XeditSearch(builtin, XawsdLeft));
}

LispObj *
Xedit_SearchForward(LispBuiltin *builtin)
/*
 search-forward string &optional offset ignore-case
 */
{
    return (XeditSearch(builtin, XawsdRight));
}

LispObj *
Xedit_VerticalScrollbar(LispBuiltin *builtin)
/*
 vertical-scrollbar &optional state
 */
{
    Arg arg[1];
    XawTextScrollMode scroll;

    LispObj *state;

    state = ARGUMENT(0);

    if (state != UNSPEC) {
	scroll = state == NIL ? XawtextScrollNever : XawtextScrollAlways;
	XtSetArg(arg[0], XtNscrollVertical, scroll);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNscrollVertical, &scroll);
	XtGetValues(textwindow, arg, 1);
	state = scroll == XawtextScrollAlways ? T : NIL;
    }

    return (state);
}

LispObj *
Xedit_WrapMode(LispBuiltin *builtin)
/*
 wrap-mode &optional value
 */
{
    int i;
    Arg arg[1];
    XawTextWrapMode wrap;

    LispObj *value;

    value = ARGUMENT(0);

    if (value != UNSPEC) {
	for (i = 0; i < 3; i++)
	    if (value == wrap_modes[i])
		break;
	if (i >= 3)
	    LispDestroy("%s: argument must be "
			":NEVER, :LINE, or :WORD, not %s",
			STRFUN(builtin), STROBJ(value));
	XtSetArg(arg[0], XtNwrap, (XawTextWrapMode)i);
	XtSetValues(textwindow, arg, 1);
    }
    else {
	XtSetArg(arg[0], XtNwrap, &wrap);
	XtGetValues(textwindow, arg, 1);
	i = (int)wrap;
	if (i <= 0 || i >= 3)
	    i = 0;
	value = wrap_modes[i];
    }

    return (value);
}

LispObj *
Xedit_XrmStringToQuark(LispBuiltin *builtin)
/*
 xrm-string-to-quark string
 */
{
    LispObj *string;

    string = ARGUMENT(0);

    CHECK_STRING(string);

    return (INTEGER(XrmStringToQuark(THESTR(string))));
}
