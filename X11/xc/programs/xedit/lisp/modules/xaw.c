/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/lisp/modules/xaw.c,v 1.14 2002/11/23 08:26:52 paulo Exp $ */

#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiSink.h>
#include <X11/Xaw/AsciiSrc.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Grip.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/MultiSink.h>
#include <X11/Xaw/MultiSrc.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Panner.h>
#include <X11/Xaw/Porthole.h>
#include <X11/Xaw/Repeater.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Simple.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Sme.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/StripChart.h>
#include <X11/Xaw/Text.h>
#include <X11/Xaw/TextSink.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/Xaw/Tip.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Tree.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Vendor.h>
#include "internal.h"
#include "private.h"

/*
 * Types
 */
typedef struct {
    LispObj *object;
    void *data;
} WidgetData;

/*
 * Prototypes
 */
int xawLoadModule(void);
void LispXawCleanupCallback(Widget, XtPointer, XtPointer);

/* until a better/smarter interface be written... */
LispObj *Lisp_XawCoerceToListReturnStruct(LispBuiltin*);
LispObj *Lisp_XawScrollbarCoerceToReal(LispBuiltin*);

LispObj *Lisp_XawFormDoLayout(LispBuiltin*);
LispObj *Lisp_XawListChange(LispBuiltin*);
LispObj *Lisp_XawListHighlight(LispBuiltin*);
LispObj *Lisp_XawListUnhighlight(LispBuiltin*);
LispObj *Lisp_XawTextGetSource(LispBuiltin*);
LispObj *Lisp_XawTextLastPosition(LispBuiltin*);
LispObj *Lisp_XawTextReplace(LispBuiltin*);
LispObj *Lisp_XawTextSearch(LispBuiltin*);
LispObj *Lisp_XawTextGetInsertionPoint(LispBuiltin*);
LispObj *Lisp_XawTextSetInsertionPoint(LispBuiltin*);
LispObj *Lisp_XawScrollbarSetThumb(LispBuiltin*);

/*
 * Initialization
 */

static LispBuiltin lispbuiltins[] = {
    {LispFunction, Lisp_XawCoerceToListReturnStruct, "xaw-coerce-to-list-return-struct opaque"},
    {LispFunction, Lisp_XawScrollbarCoerceToReal, "xaw-scrollbar-coerce-to-real opaque"},

    {LispFunction, Lisp_XawScrollbarSetThumb, "xaw-scrollbar-set-thumb widget top &optional shown"},
    {LispFunction, Lisp_XawFormDoLayout, "xaw-form-do-layout widget force"},
    {LispFunction, Lisp_XawListChange, "xaw-list-change widget list &optional longest resize"},
    {LispFunction, Lisp_XawListHighlight, "xaw-list-highlight widget index"},
    {LispFunction, Lisp_XawListUnhighlight, "xaw-list-unhighlight widget"},
    {LispFunction, Lisp_XawTextGetSource, "xaw-text-get-source widget"},
    {LispFunction, Lisp_XawTextLastPosition, "xaw-text-last-position widget"},
    {LispFunction, Lisp_XawTextReplace, "xaw-text-replace widget left right text"},
    {LispFunction, Lisp_XawTextSearch, "xaw-text-search widget direction text"},
    {LispFunction, Lisp_XawTextGetInsertionPoint, "xaw-text-get-insertion-point widget"},
    {LispFunction, Lisp_XawTextSetInsertionPoint, "xaw-text-set-insertion-point widget position"},
};

LispModuleData xawLispModuleData = {
    LISP_MODULE_VERSION,
    xawLoadModule
};

static int xawWidget_t, xawWidgetClass_t, xawListReturnStruct_t, xawFloatp_t;
static WidgetData **list_data;
static int num_list_data;

/*
 * Implementation
 */
int
xawLoadModule(void)
{
    int i;
    char *fname = "XAW-LOAD-MODULE";

    xawWidget_t = LispRegisterOpaqueType("Widget");
    xawWidgetClass_t = LispRegisterOpaqueType("WidgetClass");
    xawListReturnStruct_t = LispRegisterOpaqueType("XawListReturnStruct");
    xawFloatp_t = LispRegisterOpaqueType("float*");

    LispExecute("(DEFSTRUCT XAW-LIST-RETURN-STRUCT STRING INDEX)\n");

    GCDisable();
    (void)LispSetVariable(ATOM2("ASCII-SINK-OBJECT-CLASS"),
			  OPAQUE(asciiSinkObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("ASCII-SRC-OBJECT-CLASS"),
			  OPAQUE(asciiSinkObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("ASCII-TEXT-WIDGET-CLASS"),
			  OPAQUE(asciiTextWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("BOX-WIDGET-CLASS"),
			  OPAQUE(boxWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("COMMAND-WIDGET-CLASS"),
			  OPAQUE(commandWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("DIALOG-WIDGET-CLASS"),
			  OPAQUE(dialogWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("FORM-WIDGET-CLASS"),
			  OPAQUE(formWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("GRIP-WIDGET-CLASS"),
			  OPAQUE(gripWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("LABEL-WIDGET-CLASS"),
			  OPAQUE(labelWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("LIST-WIDGET-CLASS"),
			  OPAQUE(listWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("MENU-BUTTON-WIDGET-CLASS"),
			  OPAQUE(menuButtonWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("MULTI-SINK-OBJEC-TCLASS"),
			  OPAQUE(multiSinkObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("MULTI-SRC-OBJECT-CLASS"),
			  OPAQUE(multiSrcObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("PANED-WIDGET-CLASS"),
			  OPAQUE(panedWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("PANNER-WIDGET-CLASS"),
			  OPAQUE(pannerWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("PORTHOLE-WIDGET-CLASS"),
			  OPAQUE(portholeWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("REPEATER-WIDGET-CLASS"),
			  OPAQUE(repeaterWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("SCROLLBAR-WIDGET-CLASS"),
			  OPAQUE(scrollbarWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("SIMPLE-MENU-WIDGET-CLASS"),
			  OPAQUE(simpleMenuWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("SIMPLE-WIDGET-CLASS"),
			  OPAQUE(simpleWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("SME-BSB-OBJECT-CLASS"),
			  OPAQUE(smeBSBObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("SME-LINE-OBJECT-CLASS"),
			  OPAQUE(smeLineObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("SME-OBJECT-CLASS"),
			  OPAQUE(smeObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("STRIP-CHART-WIDGET-CLASS"),
			  OPAQUE(stripChartWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TEXT-WIDGET-CLASS"),
			  OPAQUE(textWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TEXT-SINKOBJECT-CLASS"),
			  OPAQUE(textSinkObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TEXT-SRC-OBJECT-CLASS"),
			  OPAQUE(textSrcObjectClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TIP-WIDGET-CLASS"),
			  OPAQUE(tipWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TOGGLE-WIDGET-CLASS"),
			  OPAQUE(toggleWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TREE-WIDGET-CLASS"),
			  OPAQUE(treeWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("VIEWPORT-WIDGET-CLASS"),
			  OPAQUE(viewportWidgetClass, xawWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("VENDOR-SHELL-WIDGET-CLASS"),
			  OPAQUE(vendorShellWidgetClass, xawWidgetClass_t),
			  fname, 0);

    /* return codes of XawTextReplace */
    (void)LispSetVariable(ATOM2("XAW-REPLACE-ERROR"),
			  INTEGER(XawReplaceError), fname, 0);
    (void)LispSetVariable(ATOM2("XAW-EDIT-DONE"),
			  INTEGER(XawEditDone), fname, 0);
    (void)LispSetVariable(ATOM2("XAW-EDIT-ERROR"),
			  INTEGER(XawEditError), fname, 0);
    (void)LispSetVariable(ATOM2("XAW-POSITION-ERROR"),
			  INTEGER(XawPositionError), fname, 0);

    /* return code of XawTextSearch */
    (void)LispSetVariable(ATOM2("XAW-TEXT-SEARCH-ERROR"),
			  INTEGER(XawTextSearchError), fname, 0);

    /* enum XawTextScanDirection */
    (void)LispSetVariable(ATOM2("XAWSD-LEFT"),
			  INTEGER(XawsdLeft), fname, 0);
    (void)LispSetVariable(ATOM2("XAWSD-RIGHT"),
			  INTEGER(XawsdRight), fname, 0);
    GCEnable();

    for (i = 0; i < sizeof(lispbuiltins) / sizeof(lispbuiltins[0]); i++)
	LispAddBuiltinFunction(&lispbuiltins[i]);

    return (1);
}

void
LispXawCleanupCallback(Widget w, XtPointer user_data, XtPointer call_data)
{
    WidgetData *data = (WidgetData*)user_data;

    UPROTECT(CAR(data->object), data->object);
    XtFree((XtPointer)data->data);
    XtFree((XtPointer)data);
}

LispObj *
Lisp_XawCoerceToListReturnStruct(LispBuiltin *builtin)
/*
 xaw-coerce-to-list-return-struct opaque
 */
{
    LispObj *result, *code, *ocod = COD;
    XawListReturnStruct *retlist;

    LispObj *opaque;

    opaque = ARGUMENT(0);

    if (!CHECKO(opaque, xawListReturnStruct_t))
	LispDestroy("%s: cannot convert %s to XawListReturnStruct",
		    STRFUN(builtin), STROBJ(opaque));

    retlist = (XawListReturnStruct*)(opaque->data.opaque.data);

    GCDisable();
    code = CONS(ATOM("MAKE-XAW-LIST-RETURN-STRUCT"),
		CONS(KEYWORD("STRING"),
		       CONS(STRING(retlist->string),
			    CONS(KEYWORD("INDEX"),
				 CONS(INTEGER(retlist->list_index), NIL)))));
    COD = CONS(code, COD);
    GCEnable();

    result = EVAL(code);
    COD = ocod;

    return (result);
}

LispObj *
Lisp_XawScrollbarCoerceToReal(LispBuiltin *builtin)
/*
 xaw-scrollbar-coerce-to-real opaque
 */
{
    LispObj *result;
    float *floatp;
    double real;

    LispObj *opaque;

    opaque = ARGUMENT(0);

    if (!CHECKO(opaque, xawFloatp_t))
	LispDestroy("%s: cannot convert %s to float*",
		    STRFUN(builtin), STROBJ(opaque));

    floatp = (float*)(opaque->data.opaque.data);
    real = *floatp;

    return (DFLOAT(real));
}

LispObj *
Lisp_XawFormDoLayout(LispBuiltin *builtin)
/*
 xaw-form-do-layout widget force
 */
{
    int force;

    LispObj *owidget, *oforce;

    oforce = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));

    force = oforce != NIL;
    XawFormDoLayout((Widget)(owidget->data.opaque.data), force);

    return (oforce);
}

LispObj *
Lisp_XawTextGetSource(LispBuiltin *builtin)
/*
 xaw-text-get-source widget
 */
{
    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));

    return (OPAQUE(XawTextGetSource((Widget)(owidget->data.opaque.data)),
		   xawWidget_t));
}

LispObj *
Lisp_XawTextLastPosition(LispBuiltin *builtin)
/*
 xaw-text-last-position widget
 */
{
    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));

    return (FIXNUM(XawTextLastPosition((Widget)(owidget->data.opaque.data))));
}

LispObj *
Lisp_XawTextGetInsertionPoint(LispBuiltin *builtin)
/*
 xaw-text-get-insertion-point widget
 */
{
    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));

    return (FIXNUM(XawTextGetInsertionPoint((Widget)(owidget->data.opaque.data))));
}

LispObj *
Lisp_XawTextSetInsertionPoint(LispBuiltin *builtin)
/*
 xaw-text-set-insertion-point widget position
 */
{
    Widget widget;
    XawTextPosition position;

    LispObj *owidget, *oposition;

    oposition = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);

    CHECK_INDEX(oposition);
    position = (XawTextPosition)FIXNUM_VALUE(oposition);

    XawTextSetInsertionPoint(widget, position);

    return (oposition);
}

LispObj *
Lisp_XawTextReplace(LispBuiltin *builtin)
/*
 xaw-text-replace widget left right text
 */
{
    Widget widget;
    XawTextPosition left, right;
    XawTextBlock block;

    LispObj *owidget, *oleft, *oright, *otext;

    otext = ARGUMENT(3);
    oright = ARGUMENT(2);
    oleft = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);

    CHECK_INDEX(oleft);
    left = (XawTextPosition)FIXNUM_VALUE(oleft);

    CHECK_INDEX(oright);
    right = (XawTextPosition)FIXNUM_VALUE(oright);

    CHECK_STRING(otext);
    block.firstPos = 0;
    block.ptr = THESTR(otext);
    block.length = strlen(block.ptr);
    block.format = FMT8BIT;

    return (FIXNUM(XawTextReplace(widget, left, right, &block)));
}

LispObj *
Lisp_XawTextSearch(LispBuiltin *builtin)
/*
 xaw-text-search widget direction text
 */
{
    Widget widget;
    XawTextScanDirection direction;
    XawTextBlock block;

    LispObj *owidget, *odirection, *otext;

    otext = ARGUMENT(2);
    odirection = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);

    CHECK_INDEX(odirection);
    direction = (XawTextPosition)FIXNUM_VALUE(odirection);
    if (direction != XawsdLeft && direction != XawsdRight)
	LispDestroy("%s: %d does not fit in XawTextScanDirection",
		    STRFUN(builtin), direction);

    CHECK_STRING(otext);
    block.firstPos = 0;
    block.ptr = THESTR(otext);
    block.length = strlen(block.ptr);
    block.format = FMT8BIT;

    return (FIXNUM(XawTextSearch(widget, direction, &block)));
}

LispObj *
Lisp_XawListChange(LispBuiltin *builtin)
/*
 xaw-list-change widget list &optional longest resize
 */
{
    Widget widget;
    String *list;
    int i, nitems;
    int longest;
    Boolean resize;
    LispObj *object;
    WidgetData *data;

    LispObj *owidget, *olist, *olongest, *oresize;

    oresize = ARGUMENT(3);
    olongest = ARGUMENT(2);
    olist = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);

    CHECK_LIST(olist);
    for (nitems = 0, object = olist; CONSP(object);
	 ++nitems, object = CDR(object))
	CHECK_STRING(CAR(object));

    if (olongest != UNSPEC) {
	CHECK_INDEX(olongest);
	longest = FIXNUM_VALUE(olongest);
    }
    else
	XtVaGetValues(widget, XtNlongest, &longest, NULL, 0);
    resize = oresize != UNSPEC && oresize != NIL;

    /* No errors in arguments, build string list */
    list = (String*)XtMalloc(sizeof(String) * nitems);
    for (i = 0, object = olist; CONSP(object); i++, object = CDR(object))
	list[i] = THESTR(CAR(object));

    /* Check if xaw-list-change was already called
      * for this widget and free previous data */
    for (i = 0; i < num_list_data; i++)
	if ((Widget)CAR(list_data[i]->object)->data.opaque.data == widget) {
	    XtRemoveCallback(widget, XtNdestroyCallback,
			     LispXawCleanupCallback, list_data[i]);
	    LispXawCleanupCallback(widget, list_data[i], NULL);
	    break;
	}

    if (i >= num_list_data) {
	++num_list_data;
	list_data = (WidgetData**)XtRealloc((XtPointer)list_data,
					    sizeof(WidgetData*) * num_list_data);
    }

    data = (WidgetData*)XtMalloc(sizeof(WidgetData));
    data->data = list;
    list_data[i] = data;
    data->object = CONS(owidget, olist);
    PROTECT(owidget, data->object);
    XtAddCallback(widget, XtNdestroyCallback, LispXawCleanupCallback, data);

    XawListChange(widget, list, nitems, longest, resize);

    return (olist);
}

LispObj *
Lisp_XawListHighlight(LispBuiltin *builtin)
/*
 xaw-list-highlight widget index
 */
{
    Widget widget;
    int position;

    LispObj *owidget, *oindex;

    oindex = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);

    CHECK_INDEX(oindex);
    position = FIXNUM_VALUE(oindex);

    XawListHighlight(widget, position);

    return (oindex);
}

LispObj *
Lisp_XawListUnhighlight(LispBuiltin *builtin)
/*
 xaw-list-unhighlight widget
 */
{
    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));

    XawListUnhighlight((Widget)(owidget->data.opaque.data));

    return (NIL);
}

LispObj *
Lisp_XawScrollbarSetThumb(LispBuiltin *builtin)
/*
 xaw-scrollbar-set-thumb widget top &optional shown
 */
{
    Widget widget;
    double top, shown;

    LispObj *owidget, *otop, *oshown;

    oshown = ARGUMENT(2);
    otop = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xawWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);

    CHECK_DFLOAT(otop);
    top = DFLOAT_VALUE(otop);

    if (oshown == UNSPEC)
	shown = 1.0;
    else {
	CHECK_DFLOAT(oshown);
	shown = DFLOAT_VALUE(oshown);
    }

    XawScrollbarSetThumb(widget, top, shown);

    return (oshown == UNSPEC ? DFLOAT(shown) : oshown);
}
