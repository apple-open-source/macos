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

/* $XFree86: xc/programs/xedit/lisp/modules/xt.c,v 1.19 2002/11/23 08:26:52 paulo Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include "internal.h"
#include "private.h"

/*
 * Types
 */
typedef struct {
    XrmQuark qname;
    XrmQuark qtype;
    Cardinal size;
} ResourceInfo;

typedef struct {
    WidgetClass widget_class;
    ResourceInfo **resources;
    Cardinal num_resources;
    Cardinal num_cons_resources;
} ResourceList;

typedef struct {
    Arg *args;
    Cardinal num_args;
} Resources;

typedef struct {
    LispObj *data;
    /* data is => (list* widget callback argument) */
} CallbackArgs;

/*
 * Prototypes
 */
int xtLoadModule(LispMac*);
void LispXtCleanupCallback(Widget, XtPointer, XtPointer);

void LispXtCallback(Widget, XtPointer, XtPointer);
void LispXtInputCallback(XtPointer, int*, XtInputId*);

/* a hack... */
LispObj *Lisp_XtCoerceToWidgetList(LispBuiltin*);

LispObj *Lisp_XtAddCallback(LispBuiltin*);
LispObj *Lisp_XtAppInitialize(LispBuiltin*);
LispObj *Lisp_XtAppMainLoop(LispBuiltin*);
LispObj *Lisp_XtAppAddInput(LispBuiltin*);
LispObj *Lisp_XtAppPending(LispBuiltin*);
LispObj *Lisp_XtAppProcessEvent(LispBuiltin*);
LispObj *Lisp_XtCreateWidget(LispBuiltin*);
LispObj *Lisp_XtCreateManagedWidget(LispBuiltin*);
LispObj *Lisp_XtCreatePopupShell(LispBuiltin*);
LispObj *Lisp_XtDestroyWidget(LispBuiltin*);
LispObj *Lisp_XtGetKeyboardFocusWidget(LispBuiltin*);
LispObj *Lisp_XtGetValues(LispBuiltin*);
LispObj *Lisp_XtManageChild(LispBuiltin*);
LispObj *Lisp_XtUnmanageChild(LispBuiltin*);
LispObj *Lisp_XtSetMappedWhenManaged(LispBuiltin*);
LispObj *Lisp_XtMapWidget(LispBuiltin*);
LispObj *Lisp_XtName(LispBuiltin*);
LispObj *Lisp_XtParent(LispBuiltin*);
LispObj *Lisp_XtUnmapWidget(LispBuiltin*);
LispObj *Lisp_XtPopup(LispBuiltin*);
LispObj *Lisp_XtPopdown(LispBuiltin*);
LispObj *Lisp_XtIsRealized(LispBuiltin*);
LispObj *Lisp_XtRealizeWidget(LispBuiltin*);
LispObj *Lisp_XtUnrealizeWidget(LispBuiltin*);
LispObj *Lisp_XtRemoveInput(LispBuiltin*);
LispObj *Lisp_XtSetSensitive(LispBuiltin*);
LispObj *Lisp_XtSetValues(LispBuiltin*);
LispObj *Lisp_XtWidgetToApplicationContext(LispBuiltin*);
LispObj *Lisp_XtDisplay(LispBuiltin*);
LispObj *Lisp_XtDisplayOfObject(LispBuiltin*);
LispObj *Lisp_XtScreen(LispBuiltin*);
LispObj *Lisp_XtScreenOfObject(LispBuiltin*);
LispObj *Lisp_XtSetKeyboardFocus(LispBuiltin*);
LispObj *Lisp_XtWindow(LispBuiltin*);
LispObj *Lisp_XtWindowOfObject(LispBuiltin*);
LispObj *Lisp_XtAddGrab(LispBuiltin*);
LispObj *Lisp_XtRemoveGrab(LispBuiltin*);
LispObj *Lisp_XtAppGetExitFlag(LispBuiltin*);
LispObj *Lisp_XtAppSetExitFlag(LispBuiltin*);

LispObj *LispXtCreateWidget(LispBuiltin*, int);

static Resources *LispConvertResources(LispObj*, Widget,
				       ResourceList*, ResourceList*);
static void LispFreeResources(Resources*);

static int bcmp_action_resource(_Xconst void*, _Xconst void*);
static ResourceInfo *GetResourceInfo(char*, ResourceList*, ResourceList*);
static ResourceList *GetResourceList(WidgetClass);
static int bcmp_action_resource_list(_Xconst void*, _Xconst void*);
static ResourceList *FindResourceList(WidgetClass);
static int qcmp_action_resource_list(_Xconst void*, _Xconst void*);
static ResourceList *CreateResourceList(WidgetClass);
static int qcmp_action_resource(_Xconst void*, _Xconst void*);
static void BindResourceList(ResourceList*);

static void PopdownAction(Widget, XEvent*, String*, Cardinal*);
static void QuitAction(Widget, XEvent*, String*, Cardinal*);

/*
 * Initialization
 */
static LispBuiltin lispbuiltins[] = {
    {LispFunction, Lisp_XtCoerceToWidgetList, "xt-coerce-to-widget-list number opaque"},

    {LispFunction, Lisp_XtAddGrab, "xt-add-grab widget exclusive spring-loaded"},
    {LispFunction, Lisp_XtAddCallback, "xt-add-callback widget callback-name callback &optional client-data"},
    {LispFunction, Lisp_XtAppAddInput, "xt-app-add-input app-context fileno condition function &optional client-data"},
    {LispFunction, Lisp_XtAppInitialize, "xt-app-initialize app-context-return application-class &optional options fallback-resources"},
    {LispFunction, Lisp_XtAppPending, "xt-app-pending app-context"},
    {LispFunction, Lisp_XtAppMainLoop, "xt-app-main-loop app-context"},
    {LispFunction, Lisp_XtAppProcessEvent, "xt-app-process-event app-context &optional mask"},
    {LispFunction, Lisp_XtAppGetExitFlag, "xt-app-get-exit-flag app-context"},
    {LispFunction, Lisp_XtAppSetExitFlag, "xt-app-set-exit-flag app-context"},
    {LispFunction, Lisp_XtCreateManagedWidget, "xt-create-managed-widget name widget-class parent &optional arguments"},
    {LispFunction, Lisp_XtCreateWidget, "xt-create-widget name widget-class parent &optional arguments"},
    {LispFunction, Lisp_XtCreatePopupShell, "xt-create-popup-shell name widget-class parent &optional arguments"},
    {LispFunction, Lisp_XtDestroyWidget, "xt-destroy-widget widget"},
    {LispFunction, Lisp_XtGetKeyboardFocusWidget, "xt-get-keyboard-focus-widget widget"},
    {LispFunction, Lisp_XtGetValues, "xt-get-values widget arguments"},
    {LispFunction, Lisp_XtManageChild, "xt-manage-child widget"},
    {LispFunction, Lisp_XtName, "xt-name widget"},
    {LispFunction, Lisp_XtUnmanageChild, "xt-unmanage-child widget"},
    {LispFunction, Lisp_XtMapWidget, "xt-map-widget widget"},
    {LispFunction, Lisp_XtUnmapWidget, "xt-unmap-widget widget"},
    {LispFunction, Lisp_XtSetMappedWhenManaged, "xt-set-mapped-when-managed widget map-when-managed"},
    {LispFunction, Lisp_XtParent, "xt-parent widget"},
    {LispFunction, Lisp_XtPopup, "xt-popup widget grab-kind"},
    {LispFunction, Lisp_XtPopdown, "xt-popdown widget"},
    {LispFunction, Lisp_XtIsRealized, "xt-is-realized widget"},
    {LispFunction, Lisp_XtRealizeWidget, "xt-realize-widget widget"},
    {LispFunction, Lisp_XtUnrealizeWidget, "xt-unrealize-widget widget"},
    {LispFunction, Lisp_XtRemoveInput, "xt-remove-input input"},
    {LispFunction, Lisp_XtRemoveGrab, "xt-remove-grab widget"},
    {LispFunction, Lisp_XtSetKeyboardFocus, "xt-set-keyboard-focus widget descendant"},
    {LispFunction, Lisp_XtSetSensitive, "xt-set-sensitive widget sensitive"},
    {LispFunction, Lisp_XtSetValues, "xt-set-values widget arguments"},
    {LispFunction, Lisp_XtWidgetToApplicationContext, "xt-widget-to-application-context widget"},
    {LispFunction, Lisp_XtDisplay, "xt-display widget"},
    {LispFunction, Lisp_XtDisplayOfObject, "xt-display-of-object object"},
    {LispFunction, Lisp_XtScreen, "xt-screen widget"},
    {LispFunction, Lisp_XtScreenOfObject, "xt-screen-of-object object"},
    {LispFunction, Lisp_XtWindow, "xt-window widget"},
    {LispFunction, Lisp_XtWindowOfObject, "xt-window-of-object object"},
};

LispModuleData xtLispModuleData = {
    LISP_MODULE_VERSION,
    xtLoadModule,
};

static ResourceList **resource_list;
static Cardinal num_resource_list;

static Atom delete_window;
static int xtAppContext_t, xtWidget_t, xtWidgetClass_t, xtWidgetList_t,
	   xtInputId_t, xtDisplay_t, xtScreen_t, xtWindow_t;

static XtActionsRec actions[] = {
    {"xt-popdown",	PopdownAction},
    {"xt-quit",		QuitAction},
};

static XrmQuark qCardinal, qInt, qString, qWidget, qFloat;

static CallbackArgs **input_list;
static Cardinal num_input_list, size_input_list;

/*
 * Implementation
 */
int
xtLoadModule(void)
{
    int i;
    char *fname = "XT-LOAD-MODULE";

    xtAppContext_t = LispRegisterOpaqueType("XtAppContext");
    xtWidget_t = LispRegisterOpaqueType("Widget");
    xtWidgetClass_t = LispRegisterOpaqueType("WidgetClass");
    xtWidgetList_t = LispRegisterOpaqueType("WidgetList");
    xtInputId_t = LispRegisterOpaqueType("XtInputId");
    xtDisplay_t = LispRegisterOpaqueType("Display*");
    xtScreen_t = LispRegisterOpaqueType("Screen*");
    xtWindow_t = LispRegisterOpaqueType("Window");

    LispExecute("(DEFSTRUCT XT-WIDGET-LIST NUM-CHILDREN CHILDREN)\n");

    GCDisable();
    (void)LispSetVariable(ATOM2("CORE-WIDGET-CLASS"),
			  OPAQUE(coreWidgetClass, xtWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("COMPOSITE-WIDGET-CLASS"),
			  OPAQUE(compositeWidgetClass, xtWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("CONSTRAINT-WIDGET-CLASS"),
			  OPAQUE(constraintWidgetClass, xtWidgetClass_t),
			  fname, 0);
    (void)LispSetVariable(ATOM2("TRANSIENT-SHELL-WIDGET-CLASS"),
			  OPAQUE(transientShellWidgetClass, xtWidgetClass_t),
			  fname, 0);

    /* parameters for XtPopup */
    (void)LispSetVariable(ATOM2("XT-GRAB-EXCLUSIVE"),
			  INTEGER(XtGrabExclusive), fname, 0);
    (void)LispSetVariable(ATOM2("XT-GRAB-NONE"),
			  INTEGER(XtGrabNone), fname, 0);
    (void)LispSetVariable(ATOM2("XT-GRAB-NONE-EXCLUSIVE"),
			  INTEGER(XtGrabNonexclusive), fname, 0);

    /* parameters for XtAppProcessEvent */
    (void)LispSetVariable(ATOM2("XT-IM-XEVENT"),
			  INTEGER(XtIMXEvent), fname, 0);
    (void)LispSetVariable(ATOM2("XT-IM-TIMER"),
			  INTEGER(XtIMTimer), fname, 0);
    (void)LispSetVariable(ATOM2("XT-IM-ALTERNATE-INPUT"),
			  INTEGER(XtIMAlternateInput), fname, 0);
    (void)LispSetVariable(ATOM2("XT-IM-SIGNAL"),
			  INTEGER(XtIMSignal), fname, 0);
    (void)LispSetVariable(ATOM2("XT-IM-ALL"),
			  INTEGER(XtIMAll), fname, 0);

    /* parameters for XtAppAddInput */
    (void)LispSetVariable(ATOM2("XT-INPUT-READ-MASK"),
			  INTEGER(XtInputReadMask), fname, 0);
    (void)LispSetVariable(ATOM2("XT-INPUT-WRITE-MASK"),
			  INTEGER(XtInputWriteMask), fname, 0);
    (void)LispSetVariable(ATOM2("XT-INPUT-EXCEPT-MASK"),
			  INTEGER(XtInputExceptMask), fname, 0);
    GCEnable();

    qCardinal = XrmPermStringToQuark(XtRCardinal);
    qInt = XrmPermStringToQuark(XtRInt);
    qString = XrmPermStringToQuark(XtRString);
    qWidget = XrmPermStringToQuark(XtRWidget);
    qFloat = XrmPermStringToQuark(XtRFloat);

    for (i = 0; i < sizeof(lispbuiltins) / sizeof(lispbuiltins[0]); i++)
	LispAddBuiltinFunction(&lispbuiltins[i]);

    return (1);
}

void
LispXtCallback(Widget w, XtPointer user_data, XtPointer call_data)
{
    CallbackArgs *args = (CallbackArgs*)user_data;
    LispObj *code, *ocod = COD;

    GCDisable();
		/* callback name */	   /* reall caller */
    code = CONS(CDR(CDR(args->data)), CONS(OPAQUE(w, xtWidget_t),
		CONS(CAR(CDR(args->data)), CONS(OPAQUE(call_data, 0), NIL))));
		     /* user arguments */
    COD = CONS(code, COD);
    GCEnable();

    (void)EVAL(code);
    COD = ocod;
}


void
LispXtCleanupCallback(Widget w, XtPointer user_data, XtPointer call_data)
{
    CallbackArgs *args = (CallbackArgs*)user_data;

    UPROTECT(CAR(args->data), args->data);
    XtFree((XtPointer)args);
}

void
LispXtInputCallback(XtPointer closure, int *source, XtInputId *id)
{
    CallbackArgs *args = (CallbackArgs*)closure;
    LispObj *code, *ocod = COD;

    GCDisable();
		/* callback name */	  /* user arguments */
    code = CONS(CDR(CDR(args->data)), CONS(CAR(CDR(args->data)),
		CONS(INTEGER(*source), CONS(CAR(args->data), NIL))));
		     /* input source */	   /* input id */
    COD = CONS(code, COD);
    GCEnable();

    (void)EVAL(code);
    COD = ocod;
}

LispObj *
Lisp_XtCoerceToWidgetList(LispBuiltin *builtin)
/*
 xt-coerce-to-widget-list number opaque
 */
{
    int i;
    WidgetList children;
    Cardinal num_children;
    LispObj *cons, *widget_list, *result;

    LispObj *onumber, *opaque;

    opaque = ARGUMENT(1);
    onumber = ARGUMENT(0);

    CHECK_INDEX(onumber);
    num_children = FIXNUM_VALUE(onumber);

    if (!CHECKO(opaque, xtWidgetList_t))
	LispDestroy("%s: cannot convert %s to WidgetList",
		    STRFUN(builtin), STROBJ(opaque));
    children = (WidgetList)(opaque->data.opaque.data);

    GCDisable();
    widget_list = cons = NIL;
    for (i = 0; i < num_children; i++) {
	result = CONS(OPAQUE(children[i], xtWidget_t), NIL);
	if (widget_list == NIL)
	    widget_list = cons = result;
	else {
	    RPLACD(cons, result);
	    cons = CDR(cons);
	}
    }

    result = APPLY(ATOM("MAKE-XT-WIDGET-LIST"),
		   CONS(KEYWORD("NUM-CHILDREN"),
			CONS(INTEGER(num_children),
			     CONS(KEYWORD("CHILDREN"),
				  CONS(widget_list, NIL)))));
    GCEnable();

    return (result);
}

LispObj *
Lisp_XtAddCallback(LispBuiltin *builtin)
/*
 xt-add-callback widget callback-name callback &optional client-data
 */
{
    CallbackArgs *arguments;
    LispObj *data;

    LispObj *widget, *callback_name, *callback, *client_data;

    client_data = ARGUMENT(3);
    callback = ARGUMENT(2);
    callback_name = ARGUMENT(1);
    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    CHECK_STRING(callback_name);
    if (!SYMBOLP(callback) && callback->type != LispLambda_t)
	LispDestroy("%s: %s cannot be used as a callback",
		    STRFUN(builtin), STROBJ(callback));

    if (client_data == UNSPEC)
	client_data = NIL;

    data = CONS(widget, CONS(client_data, callback));
    PROTECT(widget, data);

    arguments = XtNew(CallbackArgs);
    arguments->data = data;

    XtAddCallback((Widget)(widget->data.opaque.data), THESTR(callback_name),
		  LispXtCallback, (XtPointer)arguments);
    XtAddCallback((Widget)(widget->data.opaque.data), XtNdestroyCallback,
		  LispXtCleanupCallback, (XtPointer)arguments);

    return (client_data);
}

LispObj *
Lisp_XtAppAddInput(LispBuiltin *builtin)
/*
  xt-app-add-input app-context fileno condition function &optional client-data
 */
{
    LispObj *data, *input;
    XtAppContext appcon;
    int source, condition;
    CallbackArgs *arguments;
    XtInputId id;

    LispObj *app_context, *fileno, *ocondition, *function, *client_data;

    client_data = ARGUMENT(4);
    function = ARGUMENT(3);
    ocondition = ARGUMENT(2);
    fileno = ARGUMENT(1);
    app_context = ARGUMENT(0);

    if (!CHECKO(app_context, xtAppContext_t))
	LispDestroy("%s: cannot convert %s to XtAppContext",
		    STRFUN(builtin), STROBJ(app_context));
    appcon = (XtAppContext)(app_context->data.opaque.data);

    CHECK_LONGINT(fileno);
    source = LONGINT_VALUE(fileno);

    CHECK_FIXNUM(ocondition);
    condition = FIXNUM_VALUE(ocondition);

    if (!SYMBOLP(function) && function->type != LispLambda_t)
	LispDestroy("%s: %s cannot be used as a callback",
		    STRFUN(builtin), STROBJ(function));

    /* client data optional */
    if (client_data == UNSPEC)
	client_data = NIL;

    data = CONS(NIL, CONS(client_data, function));

    arguments = XtNew(CallbackArgs);
    arguments->data = data;

    id = XtAppAddInput(appcon, source, (XtPointer)condition,
		       LispXtInputCallback, (XtPointer)arguments);
    GCDisable();
    input = OPAQUE(id, xtInputId_t);
    GCEnable();
    RPLACA(data, input);
    PROTECT(input, data);

    if (num_input_list + 1 >= size_input_list) {
	++size_input_list;
	input_list = (CallbackArgs**)
	    XtRealloc((XtPointer)input_list,
		      sizeof(CallbackArgs*) * size_input_list);
    }
    input_list[num_input_list++] = arguments;

    return (input);
}

LispObj *
Lisp_XtRemoveInput(LispBuiltin *builtin)
/*
 xt-remove-input input
 */
{
    int i;
    XtInputId id;
    CallbackArgs *args;

    LispObj *input;

    input = ARGUMENT(0);

    if (!CHECKO(input, xtInputId_t))
	LispDestroy("%s: cannot convert %s to XtInputId",
		    STRFUN(builtin), STROBJ(input));

    id = (XtInputId)(input->data.opaque.data);
    for (i = 0; i < num_input_list; i++) {
	args = input_list[i];
	if (id == (XtInputId)(CAR(args->data)->data.opaque.data)) {
	    UPROTECT(CAR(args->data), args->data);
	    XtFree((XtPointer)args);

	    if (i + 1 < num_input_list)
		memmove(input_list + i, input_list + i + 1,
			sizeof(CallbackArgs*) * (num_input_list - i - 1));
	    --num_input_list;

	    XtRemoveInput(id);

	    return (T);
	}
    }

    return (NIL);
}

LispObj *
Lisp_XtAppInitialize(LispBuiltin *builtin)
/*
 xt-app-initialize app-context-return application-class &optional options fallback-resources
 */
{
    XtAppContext appcon;
    Widget shell;
    int zero = 0;
    Resources *resources = NULL;
    String *fallback = NULL;

    LispObj *app_context_return, *application_class,
	    *options, *fallback_resources;

    fallback_resources = ARGUMENT(3);
    options = ARGUMENT(2);
    application_class = ARGUMENT(1);
    app_context_return = ARGUMENT(0);

    CHECK_SYMBOL(app_context_return);
    CHECK_STRING(application_class);
    CHECK_LIST(options);

    /* check fallback resources, if given */
    if (fallback_resources != UNSPEC) {
	LispObj *string;
	int count;

	CHECK_CONS(fallback_resources);
	for (string = fallback_resources, count = 0; CONS_P(string);
	     string = CDR(string), count++)
	    CHECK_STRING(CAR(string));

	/* fallback resources was correctly specified */
	fallback = LispMalloc(sizeof(String) * (count + 1));
	for (string = fallback_resources, count = 0; CONS_P(string);
	     string = CDR(string), count++)
	    fallback[count] = THESTR(CAR(string));
	fallback[count] = NULL;
    }

    shell = XtAppInitialize(&appcon, THESTR(application_class), NULL,
			    0, &zero, NULL, fallback, NULL, 0);
    if (fallback)
	LispFree(fallback);
    (void)LispSetVariable(app_context_return,
			  OPAQUE(appcon, xtAppContext_t),
			  STRFUN(builtin), 0);

    XtAppAddActions(appcon, actions, XtNumber(actions));

    if (options != UNSPEC) {
	resources = LispConvertResources(options, shell,
					 GetResourceList(XtClass(shell)),
					 NULL);
	if (resources) {
	    XtSetValues(shell, resources->args, resources->num_args);
	    LispFreeResources(resources);
	}
    }

    return (OPAQUE(shell, xtWidget_t));
}

LispObj *
Lisp_XtAppMainLoop(LispBuiltin *builtin)
/*
 xt-app-main-loop app-context
 */
{
    LispObj *app_context;

    app_context = ARGUMENT(0);

    if (!CHECKO(app_context, xtAppContext_t))
	LispDestroy("%s: cannot convert %s to XtAppContext",
		    STRFUN(builtin), STROBJ(app_context));

    XtAppMainLoop((XtAppContext)(app_context->data.opaque.data));

    return (NIL);
}

LispObj *
Lisp_XtAppPending(LispBuiltin *builtin)
/*
 xt-app-pending app-context
 */
{
    LispObj *app_context;

    app_context = ARGUMENT(0);

    if (!CHECKO(app_context, xtAppContext_t))
	LispDestroy("%s: cannot convert %s to XtAppContext",
		    STRFUN(builtin), STROBJ(app_context));

    return (INTEGER(
	    XtAppPending((XtAppContext)(app_context->data.opaque.data))));
}

LispObj *
Lisp_XtAppProcessEvent(LispBuiltin *builtin)
/*
 xt-app-process-event app-context &optional mask
 */
{
    XtInputMask mask;
    XtAppContext appcon;

    LispObj *app_context, *omask;

    omask = ARGUMENT(1);
    app_context = ARGUMENT(0);

    if (!CHECKO(app_context, xtAppContext_t))
	LispDestroy("%s: cannot convert %s to XtAppContext",
		    STRFUN(builtin), STROBJ(app_context));

    appcon = (XtAppContext)(app_context->data.opaque.data);
    if (omask == UNSPEC)
	mask = XtIMAll;
    else {
	CHECK_FIXNUM(omask);
	mask = FIXNUM_VALUE(omask);
    }

    if (mask != (mask & XtIMAll))
	LispDestroy("%s: %d does not fit in XtInputMask %d",
		    STRFUN(builtin), mask);

    if (mask)
	XtAppProcessEvent(appcon, mask);

    return (omask == NIL ? FIXNUM(mask) : omask);
}

LispObj *
Lisp_XtRealizeWidget(LispBuiltin *builtin)
/*
 xt-realize-widget widget
 */
{
    Widget widget;

    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    XtRealizeWidget(widget);

    if (XtIsSubclass(widget, shellWidgetClass)) {
	if (!delete_window)
	    delete_window = XInternAtom(XtDisplay(widget),
					"WM_DELETE_WINDOW", False);
	(void)XSetWMProtocols(XtDisplay(widget), XtWindow(widget),
			      &delete_window, 1);
    }

    return (owidget);
}

LispObj *
Lisp_XtUnrealizeWidget(LispBuiltin *builtin)
/*
 xt-unrealize-widget widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    XtUnrealizeWidget((Widget)(widget->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtIsRealized(LispBuiltin *builtin)
/*
 xt-is-realized widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    return (XtIsRealized((Widget)(widget->data.opaque.data)) ? T : NIL);
}

LispObj *
Lisp_XtDestroyWidget(LispBuiltin *builtin)
/*
 xt-destroy-widget widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    XtDestroyWidget((Widget)(widget->data.opaque.data));

    return (NIL);
}

#define UNMANAGED	0
#define MANAGED		1
#define SHELL		2
LispObj *
Lisp_XtCreateWidget(LispBuiltin *builtin)
/*
 xt-create-widget name widget-class parent &optional arguments
 */
{
    return (LispXtCreateWidget(builtin, UNMANAGED));
}

LispObj *
Lisp_XtCreateManagedWidget(LispBuiltin *builtin)
/*
 xt-create-managed-widget name widget-class parent &optional arguments
 */
{
    return (LispXtCreateWidget(builtin, MANAGED));
}

LispObj *
Lisp_XtCreatePopupShell(LispBuiltin *builtin)
/*
 xt-create-popup-shell name widget-class parent &optional arguments
 */
{
    return (LispXtCreateWidget(builtin, SHELL));
}

LispObj *
LispXtCreateWidget(LispBuiltin *builtin, int options)
/*
 xt-create-widget name widget-class parent &optional arguments
 xt-create-managed-widget name widget-class parent &optional arguments
 xt-create-popup-shell name widget-class parent &optional arguments
 */
{
    char *name;
    WidgetClass widget_class;
    Widget widget, parent;
    Resources *resources = NULL;

    LispObj *oname, *owidget_class, *oparent, *arguments;

    arguments = ARGUMENT(3);
    oparent = ARGUMENT(2);
    owidget_class = ARGUMENT(1);
    oname = ARGUMENT(0);

    CHECK_STRING(oname);
    name = THESTR(oname);

    if (!CHECKO(owidget_class, xtWidgetClass_t))
	LispDestroy("%s: cannot convert %s to WidgetClass",
		    STRFUN(builtin), STROBJ(owidget_class));
    widget_class = (WidgetClass)(owidget_class->data.opaque.data);

    if (!CHECKO(oparent, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(oparent));
    parent = (Widget)(oparent->data.opaque.data);

    CHECK_LIST(arguments);

    if (options == SHELL)
	widget = XtCreatePopupShell(name, widget_class, parent, NULL, 0);
    else
	widget = XtCreateWidget(name, widget_class, parent, NULL, 0);

    if (arguments == UNSPEC || arguments == NIL)
	resources = NULL;
    else {
	resources = LispConvertResources(arguments, widget,
					 GetResourceList(widget_class),
					 GetResourceList(XtClass(parent)));
	XtSetValues(widget, resources->args, resources->num_args);
    }
    if (options == MANAGED)
	XtManageChild(widget);
    if (resources)
	LispFreeResources(resources);

    return (OPAQUE(widget, xtWidget_t));
}

LispObj *
Lisp_XtGetKeyboardFocusWidget(LispBuiltin *builtin)
/*
 xt-get-keyboard-focus-widget widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    return (OPAQUE(XtGetKeyboardFocusWidget((Widget)(widget->data.opaque.data)),
		   xtWidget_t));
}

LispObj *
Lisp_XtGetValues(LispBuiltin *builtin)
/*
 xt-get-values widget arguments
 */
{
    Arg args[1];
    Widget widget;
    ResourceList *rlist, *plist;
    ResourceInfo *resource;
    LispObj *list, *object = NIL, *result, *cons = NIL;
    char c1;
    short c2;
    int c4;
#ifdef LONG64
    long c8;
#endif

    LispObj *owidget, *arguments;

    arguments = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (arguments == NIL)
	return (NIL);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    CHECK_CONS(arguments);

    rlist = GetResourceList(XtClass(widget));
    plist =  XtParent(widget) ?
	     GetResourceList(XtClass(XtParent(widget))) : NULL;

    GCDisable();
    result = NIL;
    for (list = arguments; CONS_P(list); list = CDR(list)) {
	CHECK_STRING(CAR(list));
	if ((resource = GetResourceInfo(THESTR(CAR(list)), rlist, plist))
	     == NULL) {
	    int i;
	    Widget child;

	    for (i = 0; i < rlist->num_resources; i++) {
		if (rlist->resources[i]->qtype == qWidget) {
		    XtSetArg(args[0],
			     XrmQuarkToString(rlist->resources[i]->qname),
			     &child);
		    XtGetValues(widget, args, 1);
		    if (child && XtParent(child) == widget) {
			resource =
			    GetResourceInfo(THESTR(CAR(list)),
					    GetResourceList(XtClass(child)),
					    NULL);
			if (resource)
			    break;
		    }
		}
	    }
	    if (resource == NULL) {
		LispMessage("%s: resource %s not available",
			    STRFUN(builtin), THESTR(CAR(list)));
		continue;
	    }
	}
	switch (resource->size) {
	    case 1:
		XtSetArg(args[0], THESTR(CAR(list)), &c1);
		break;
	    case 2:
		XtSetArg(args[0], THESTR(CAR(list)), &c2);
		break;
	    case 4:
		XtSetArg(args[0], THESTR(CAR(list)), &c4);
		break;
#ifdef LONG64
	    case 1:
		XtSetArg(args[0], THESTR(CAR(list)), &c8);
		break;
#endif
	}
	XtGetValues(widget, args, 1);

	/* special resources */
	if (resource->qtype == qString) {
#ifdef LONG64
	    object = CONS(CAR(list), STRING(c8));
#else
	    object = CONS(CAR(list), STRING(c4));
#endif
	}
	else if (resource->qtype == qCardinal || resource->qtype == qInt) {
#ifdef LONG64
	    if (sizeof(int) == 8)
		object = CONS(CAR(list), INTEGER(c8));
	    else
#endif
	    object = CONS(CAR(list), INTEGER(c4));
	}
	else {
	    switch (resource->size) {
		case 1:
		    object = CONS(CAR(list), OPAQUE(c1, 0));
		    break;
		case 2:
		    object = CONS(CAR(list), OPAQUE(c2, 0));
		    break;
		case 4:
		    object = CONS(CAR(list), OPAQUE(c4, 0));
		    break;
#ifdef LONG64
		case 8:
		    object = CONS(CAR(list), OPAQUE(c8, 0));
		    break;
#endif
	    }
	}

	if (result == NIL)
	    result = cons = CONS(object, NIL);
	else {
	    RPLACD(cons, CONS(object, NIL));
	    cons = CDR(cons);
	}
    }
    GCEnable();

    return (result);
}

LispObj *
Lisp_XtManageChild(LispBuiltin *builtin)
/*
 xt-manage-child widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    XtManageChild((Widget)(widget->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtUnmanageChild(LispBuiltin *builtin)
/*
 xt-unmanage-child widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    XtUnmanageChild((Widget)(widget->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtMapWidget(LispBuiltin *builtin)
/*
 xt-map-widget widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    XtMapWidget((Widget)(widget->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtUnmapWidget(LispBuiltin *builtin)
/*
 xt-unmap-widget widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    XtUnmapWidget((Widget)(widget->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtSetMappedWhenManaged(LispBuiltin *builtin)
/*
 xt-set-mapped-when-managed widget map-when-managed
 */
{
    LispObj *widget, *map_when_managed;

    map_when_managed = ARGUMENT(1);
    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    XtSetMappedWhenManaged((Widget)(widget->data.opaque.data),
			   map_when_managed != NIL);

    return (map_when_managed);
}

LispObj *
Lisp_XtPopup(LispBuiltin *builtin)
/*
 xt-popup widget grab-kind
 */
{
    XtGrabKind kind;

    LispObj *widget, *grab_kind;

    grab_kind = ARGUMENT(1);
    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    CHECK_INDEX(grab_kind);
    kind = (XtGrabKind)FIXNUM_VALUE(grab_kind);
    if (kind != XtGrabExclusive && kind != XtGrabNone &&
	kind != XtGrabNonexclusive)
	LispDestroy("%s: %d does not fit in XtGrabKind",
		    STRFUN(builtin), kind);
    XtPopup((Widget)(widget->data.opaque.data), kind);

    return (grab_kind);
}

LispObj *
Lisp_XtPopdown(LispBuiltin *builtin)
/*
 xt-popdown widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    XtPopdown((Widget)(widget->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtSetKeyboardFocus(LispBuiltin *builtin)
/*
 xt-set-keyboard-focus widget descendant
 */
{
    LispObj *widget, *descendant;

    descendant = ARGUMENT(1);
    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    if (!CHECKO(descendant, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(descendant));
    XtSetKeyboardFocus((Widget)(widget->data.opaque.data),
		       (Widget)(descendant->data.opaque.data));

    return (widget);
}

LispObj *
Lisp_XtSetSensitive(LispBuiltin *builtin)
/*
 xt-set-sensitive widget sensitive
 */
{
    LispObj *widget, *sensitive;

    sensitive = ARGUMENT(1);
    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));
    XtSetSensitive((Widget)(widget->data.opaque.data), sensitive != NIL);

    return (sensitive);
}

LispObj *
Lisp_XtSetValues(LispBuiltin *builtin)
/*
 xt-set-values widget arguments
 */
{
    Widget widget;
    Resources *resources;

    LispObj *owidget, *arguments;

    arguments = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (arguments == NIL)
	return (owidget);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    CHECK_CONS(arguments);
    resources = LispConvertResources(arguments, widget,
				     GetResourceList(XtClass(widget)),
				     XtParent(widget) ?
					GetResourceList(XtClass(XtParent(widget))) :
					NULL);
    XtSetValues(widget, resources->args, resources->num_args);
    LispFreeResources(resources);

    return (owidget);
}

LispObj *
Lisp_XtWidgetToApplicationContext(LispBuiltin *builtin)
/*
 xt-widget-to-application-context widget
 */
{
    Widget widget;
    XtAppContext appcon;

    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    appcon = XtWidgetToApplicationContext(widget);

    return (OPAQUE(appcon, xtAppContext_t));
}

LispObj *
Lisp_XtDisplay(LispBuiltin *builtin)
/*
 xt-display widget
 */
{
    Widget widget;
    Display *display;

    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    display = XtDisplay(widget);

    return (OPAQUE(display, xtDisplay_t));
}

LispObj *
Lisp_XtDisplayOfObject(LispBuiltin *builtin)
/*
 xt-display-of-object object
 */
{
    Widget widget;
    Display *display;

    LispObj *object;

    object = ARGUMENT(0);

    if (!CHECKO(object, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(object));
    widget = (Widget)(object->data.opaque.data);
    display = XtDisplayOfObject(widget);

    return (OPAQUE(display, xtDisplay_t));
}

LispObj *
Lisp_XtScreen(LispBuiltin *builtin)
/*
 xt-screen widget
 */
{
    Widget widget;
    Screen *screen;

    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    screen = XtScreen(widget);

    return (OPAQUE(screen, xtScreen_t));
}

LispObj *
Lisp_XtScreenOfObject(LispBuiltin *builtin)
/*
 xt-screen-of-object object
 */
{
    Widget widget;
    Screen *screen;

    LispObj *object;

    object = ARGUMENT(0);

    if (!CHECKO(object, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(object));
    widget = (Widget)(object->data.opaque.data);
    screen = XtScreenOfObject(widget);

    return (OPAQUE(screen, xtScreen_t));
}

LispObj *
Lisp_XtWindow(LispBuiltin *builtin)
/*
 xt-window widget
 */
{
    Widget widget;
    Window window;

    LispObj *owidget;

    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    window = XtWindow(widget);

    return (OPAQUE(window, xtWindow_t));
}

LispObj *
Lisp_XtWindowOfObject(LispBuiltin *builtin)
/*
 xt-window-of-object widget
 */
{
    Widget widget;
    Window window;

    LispObj *object;

    object = ARGUMENT(0);

    if (!CHECKO(object, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(object));
    widget = (Widget)(object->data.opaque.data);
    window = XtWindowOfObject(widget);

    return (OPAQUE(window, xtWindow_t));
}

LispObj *
Lisp_XtAddGrab(LispBuiltin *builtin)
/*
 xt-add-grab widget exclusive spring-loaded
 */
{
    Widget widget;
    Bool exclusive, spring_loaded;

    LispObj *owidget, *oexclusive, *ospring_loaded;

    ospring_loaded = ARGUMENT(2);
    oexclusive = ARGUMENT(1);
    owidget = ARGUMENT(0);

    if (!CHECKO(owidget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(owidget));
    widget = (Widget)(owidget->data.opaque.data);
    exclusive = oexclusive != NIL;
    spring_loaded = ospring_loaded != NIL;

    XtAddGrab(widget, exclusive, spring_loaded);

    return (T);
}

LispObj *
Lisp_XtRemoveGrab(LispBuiltin *builtin)
/*
 xt-remove-grab widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    XtRemoveGrab((Widget)(widget->data.opaque.data));

    return (NIL);
}

LispObj *
Lisp_XtName(LispBuiltin *builtin)
/*
 xt-name widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    return (STRING(XtName((Widget)(widget->data.opaque.data))));
}

LispObj *
Lisp_XtParent(LispBuiltin *builtin)
/*
 xt-parent widget
 */
{
    LispObj *widget;

    widget = ARGUMENT(0);

    if (!CHECKO(widget, xtWidget_t))
	LispDestroy("%s: cannot convert %s to Widget",
		    STRFUN(builtin), STROBJ(widget));

    return (OPAQUE(XtParent((Widget)widget->data.opaque.data), xtWidget_t));
}

LispObj *
Lisp_XtAppGetExitFlag(LispBuiltin *builtin)
/*
 xt-app-get-exit-flag app-context
 */
{
    LispObj *app_context;

    app_context = ARGUMENT(0);

    if (!CHECKO(app_context, xtAppContext_t))
	LispDestroy("%s: cannot convert %s to XtAppContext",
		    STRFUN(builtin), STROBJ(app_context));

    return (XtAppGetExitFlag((XtAppContext)(app_context->data.opaque.data)) ?
	    T : NIL);
}

LispObj *
Lisp_XtAppSetExitFlag(LispBuiltin *builtin)
/*
 xt-app-get-exit-flag app-context
 */
{
    LispObj *app_context;

    app_context = ARGUMENT(0);

    if (!CHECKO(app_context, xtAppContext_t))
	LispDestroy("%s: cannot convert %s to XtAppContext",
		    STRFUN(builtin), STROBJ(app_context));

    XtAppSetExitFlag((XtAppContext)(app_context->data.opaque.data));

    return (T);
}

static Resources *
LispConvertResources(LispObj *list, Widget widget,
		     ResourceList *rlist, ResourceList *plist)
{
    char c1;
    short c2;
    int c4;   
#ifdef LONG64
    long c8;
#endif
    XrmValue from, to;
    LispObj *arg, *val;
    ResourceInfo *resource;
    char *fname = "XT-CONVERT-RESOURCES";
    Resources *resources = (Resources*)XtCalloc(1, sizeof(Resources));

    for (; CONSP(list); list = CDR(list)) {
	if (!CONSP(CAR(list))) {
	    XtFree((XtPointer)resources);
	    LispDestroy("%s: %s is not a cons", fname, STROBJ(CAR(list)));
	}
	arg = CAR(CAR(list));
	val = CDR(CAR(list));

	if (!STRINGP(arg)) {
	    XtFree((XtPointer)resources);
	    LispDestroy("%s: %s is not a string", fname, STROBJ(arg));
	}

	if ((resource = GetResourceInfo(THESTR(arg), rlist, plist)) == NULL) {
	    int i;
	    Arg args[1];
	    Widget child;

	    for (i = 0; i < rlist->num_resources; i++) {
		if (rlist->resources[i]->qtype == qWidget) {
		    XtSetArg(args[0],
			     XrmQuarkToString(rlist->resources[i]->qname),
			     &child);
		    XtGetValues(widget, args, 1);
		    if (child && XtParent(child) == widget) {
			resource =
			    GetResourceInfo(THESTR(arg),
					    GetResourceList(XtClass(child)),
					    NULL);
			if (resource)
			    break;
		    }
		}
	    }
	    if (resource == NULL) {
		LispMessage("%s: resource %s not available",
			    fname, THESTR(arg));
		continue;
	    }
	}

	if (LONGINTP(val) || DFLOATP(val) || OPAQUEP(val)) {
	    resources->args = (Arg*)
		XtRealloc((XtPointer)resources->args,
			  sizeof(Arg) * (resources->num_args + 1));
	    if (!OPAQUEP(val)) {
		float fvalue;

		if (DFLOATP(val))
		    fvalue = DFLOAT_VALUE(val);
		else
		    fvalue = LONGINT_VALUE(val);
		if (resource->qtype == qFloat) {
		    XtSetArg(resources->args[resources->num_args],
			     XrmQuarkToString(resource->qname), fvalue);
		}
		else
		    XtSetArg(resources->args[resources->num_args],
			     XrmQuarkToString(resource->qname),
			     (int)fvalue);
	    }
	    else
		XtSetArg(resources->args[resources->num_args],
			 XrmQuarkToString(resource->qname), val->data.opaque.data);
	    ++resources->num_args;
	    continue;
	}
	else if (val == NIL) {
	    /* XXX assume it is a pointer or a boolean */
#ifdef DEBUG
	    LispWarning("%s: assuming %s is a pointer or boolean",
			fname, XrmQuarkToString(resource->qname));
#endif
	    resources->args = (Arg*)
		XtRealloc((XtPointer)resources->args,
			  sizeof(Arg) * (resources->num_args + 1));
	    XtSetArg(resources->args[resources->num_args],
		     XrmQuarkToString(resource->qname), NULL);
	    ++resources->num_args;
	    continue;
	}
	else if (val == T) {
	    /* XXX assume it is a boolean */
#ifdef DEBUG
	    LispWarning("%s: assuming %s is a boolean",
			fname, XrmQuarkToString(resource->qname));
#endif
	    resources->args = (Arg*)
		XtRealloc((XtPointer)resources->args,
			  sizeof(Arg) * (resources->num_args + 1));
	    XtSetArg(resources->args[resources->num_args],
		     XrmQuarkToString(resource->qname), True);
	    ++resources->num_args;
	    continue;
	}
	else if (!STRINGP(val)) {
	    XtFree((XtPointer)resources);
	    LispDestroy("%s: resource value must be string, number or opaque, not %s",
			fname, STROBJ(val));
	}

	from.size = val == NIL ? 1 : strlen(THESTR(val)) + 1;
	from.addr = val == NIL ? "" : THESTR(val);
	switch (to.size = resource->size) {
	    case 1:
		to.addr = (XtPointer)&c1;
		break;
	    case 2:
		to.addr = (XtPointer)&c2;
		break;
	    case 4:
		to.addr = (XtPointer)&c4;
		break;
#ifdef LONG64
	    case 8:
		to.addr = (XtPointer)&c8;
		break;
#endif
	    default:
		LispWarning("%s: bad resource size %d for %s",
			    fname, to.size, THESTR(arg));
		continue;
	}

	if (qString == resource->qtype)
#ifdef LONG64
	    c8 = (long)from.addr;
#else
	    c4 = (long)from.addr;
#endif
	else if (!XtConvertAndStore(widget, XtRString, &from,
				    XrmQuarkToString(resource->qtype), &to))
	    /* The type converter already have printed an error message */
	    continue;

	resources->args = (Arg*)
	    XtRealloc((XtPointer)resources->args,
		      sizeof(Arg) * (resources->num_args + 1));
	switch (to.size) {
	    case 1:
		XtSetArg(resources->args[resources->num_args],
			 XrmQuarkToString(resource->qname), c1);
		break;
	    case 2:
		XtSetArg(resources->args[resources->num_args],
			 XrmQuarkToString(resource->qname), c2);
		break;
	    case 4:
		XtSetArg(resources->args[resources->num_args],
			 XrmQuarkToString(resource->qname), c4);
		break;
#ifdef LONG64
	    case 8:
		XtSetArg(resources->args[resources->num_args],
			 XrmQuarkToString(resource->qname), c8);
		break;
#endif
	}
	++resources->num_args;
    }

    return (resources);
}

static void
LispFreeResources(Resources *resources)
{
    if (resources) {
	XtFree((XtPointer)resources->args);
	XtFree((XtPointer)resources);
    }
}

static int
bcmp_action_resource(_Xconst void *string, _Xconst void *resource)
{
    return (strcmp((String)string,
		   XrmQuarkToString((*(ResourceInfo**)resource)->qname)));
}   

static ResourceInfo *
GetResourceInfo(char *name, ResourceList *rlist, ResourceList *plist)
{
    ResourceInfo **resource = NULL;

    if (rlist->resources)
	resource = (ResourceInfo**)
	    bsearch(name, rlist->resources, rlist->num_resources,
		    sizeof(ResourceInfo*), bcmp_action_resource);

    if (resource == NULL && plist) {
	resource = (ResourceInfo**)
	  bsearch(name, &plist->resources[plist->num_resources],
		  plist->num_cons_resources, sizeof(ResourceInfo*),
		  bcmp_action_resource);
    }

    return (resource ? *resource : NULL);
}

static ResourceList *
GetResourceList(WidgetClass wc)
{
    ResourceList *list;

    if ((list = FindResourceList(wc)) == NULL)
	list = CreateResourceList(wc);

    return (list);
}

static int
bcmp_action_resource_list(_Xconst void *wc, _Xconst void *list)
{
    return ((char*)wc - (char*)((*(ResourceList**)list)->widget_class));
}

static ResourceList *
FindResourceList(WidgetClass wc)
{  
    ResourceList **list;

    if (!resource_list)
	return (NULL);

    list = (ResourceList**)
	bsearch(wc, resource_list, num_resource_list,
		sizeof(ResourceList*),  bcmp_action_resource_list);

    return (list ? *list : NULL);
}

static int
qcmp_action_resource_list(_Xconst void *left, _Xconst void *right)
{
    return ((char*)((*(ResourceList**)left)->widget_class) -
	    (char*)((*(ResourceList**)right)->widget_class));
}

static ResourceList *
CreateResourceList(WidgetClass wc)
{
    ResourceList *list;

    list = (ResourceList*)XtMalloc(sizeof(ResourceList));
    list->widget_class = wc;
    list->num_resources = list->num_cons_resources = 0;
    list->resources = NULL;

    resource_list = (ResourceList**)
	XtRealloc((XtPointer)resource_list, sizeof(ResourceList*) *
		  (num_resource_list + 1));
    resource_list[num_resource_list++] = list;
    qsort(resource_list, num_resource_list, sizeof(ResourceList*),
	  qcmp_action_resource_list);
    BindResourceList(list);

    return (list);
}

static int
qcmp_action_resource(_Xconst void *left, _Xconst void *right)
{
    return (strcmp(XrmQuarkToString((*(ResourceInfo**)left)->qname),
		   XrmQuarkToString((*(ResourceInfo**)right)->qname)));
}

static void
BindResourceList(ResourceList *list)
{
    XtResourceList xt_list, cons_list;
    Cardinal i, num_xt, num_cons;

    XtGetResourceList(list->widget_class, &xt_list, &num_xt);
    XtGetConstraintResourceList(list->widget_class, &cons_list, &num_cons);
    list->num_resources = num_xt;
    list->num_cons_resources = num_cons;

    list->resources = (ResourceInfo**)
	XtMalloc(sizeof(ResourceInfo*) * (num_xt + num_cons));

    for (i = 0; i < num_xt; i++) {
	list->resources[i] = (ResourceInfo*)XtMalloc(sizeof(ResourceInfo));
	list->resources[i]->qname =
	    XrmPermStringToQuark(xt_list[i].resource_name);
	list->resources[i]->qtype =
	    XrmPermStringToQuark(xt_list[i].resource_type);
	list->resources[i]->size = xt_list[i].resource_size;
    }

    for (; i < num_xt + num_cons; i++) {
	list->resources[i] = (ResourceInfo*)XtMalloc(sizeof(ResourceInfo));
	list->resources[i]->qname =
	    XrmPermStringToQuark(cons_list[i - num_xt].resource_name);
	list->resources[i]->qtype =
	    XrmPermStringToQuark(cons_list[i - num_xt].resource_type);
	list->resources[i]->size = cons_list[i - num_xt].resource_size;
    }

    XtFree((XtPointer)xt_list);
    if (cons_list)
	XtFree((XtPointer)cons_list);

    qsort(list->resources, list->num_resources, sizeof(ResourceInfo*),
	  qcmp_action_resource);
    if (num_cons)
	qsort(&list->resources[num_xt], list->num_cons_resources,
	      sizeof(ResourceInfo*), qcmp_action_resource);
}

/*ARGSUSED*/
static void
PopdownAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XtPopdown(w);
}

/*ARGSUSED*/
static void
QuitAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}
