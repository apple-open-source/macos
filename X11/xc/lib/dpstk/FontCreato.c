/* 
 * FontCreato.c
 *
 * (c) Copyright 1992-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dpstk/FontCreato.c,v 1.2 2000/06/07 22:02:59 tsi Exp $ */

#include <stdio.h>
#include <ctype.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/ShellP.h>
#include <Xm/Xm.h>

/* There are no words to describe how I feel about having to do this */

#if XmVersion > 1001		
#include <Xm/ManagerP.h>
#else
#include <Xm/XmP.h>
#endif

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/DrawingA.h>
#include <Xm/Scale.h>
#include <Xm/MessageB.h>
#include <Xm/TextF.h>
#include <Xm/PanedW.h>
#include <Xm/List.h>
#include <Xm/SeparatoG.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>
#include <DPS/dpsXclient.h>
#include <DPS/dpsXshare.h>
#include <DPS/FontSBP.h>
#include <DPS/FontCreatP.h>
#include <stdlib.h>
#include <math.h>
#include "FontSBI.h"
#include "FSBwraps.h"

/* Turn a string into a compound string */
#define UnsharedCS(str) XmStringCreate(str, XmSTRING_DEFAULT_CHARSET)
#define CS(str, w) _FSBCreateSharedCS(str, w)
static XmString CSempty;
static char *opticalSize = NULL;

#define Canonical(str) XrmQuarkToString(XrmStringToQuark(str))

static float defaultSizeList[] = {
#ifndef CREATOR_DEFAULT_SIZE_LIST
    8, 10, 12, 14, 16, 18, 24, 36, 48, 72
#else
    CREATOR_DEFAULT_SIZE_LIST
#endif /* CREATOR_DEFAULT_SIZE_LIST */
};

#ifndef CREATOR_DEFAULT_SIZE_LIST_COUNT
#define CREATOR_DEFAULT_SIZE_LIST_COUNT 10
#endif /* CREATOR_DEFAULT_SIZE_LIST_COUNT */

#define Offset(field) XtOffsetOf(FontCreatorRec, creator.field)

static XtResource resources[] = {
    {XtNsizes, XtCSizes, XtRFloatList, sizeof(float*),
	Offset(sizes), XtRImmediate, (XtPointer) defaultSizeList},
    {XtNsizeCount, XtCSizeCount, XtRInt, sizeof(int),
	Offset(size_count), XtRImmediate,
	(XtPointer) CREATOR_DEFAULT_SIZE_LIST_COUNT},
    {XtNdismissCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(dismiss_callback), XtRCallback, (XtPointer) NULL},
    {XtNfontSelectionBox, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(fsb), XtRWidget, (XtPointer) NULL},
};

/* Forward declarations */

static Boolean SetValues(Widget old, Widget req, Widget new, ArgList args, Cardinal *num_args);
static XtGeometryResult GeometryManager(Widget w, XtWidgetGeometry *desired, XtWidgetGeometry *allowed);
static void ChangeManaged(Widget w);
static void ClassInitialize(void);
static void Destroy(Widget widget);
static void Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args);
static void Resize(Widget widget);

FontCreatorClassRec fontCreatorClassRec = {
    /* Core class part */
  {
    /* superclass	     */	(WidgetClass) &xmManagerClassRec,
    /* class_name	     */ "FontCreator",
    /* widget_size	     */ sizeof(FontCreatorRec),
    /* class_initialize      */ ClassInitialize,
    /* class_part_initialize */ NULL,
    /* class_inited          */	FALSE,
    /* initialize	     */	Initialize,
    /* initialize_hook       */	NULL,
    /* realize		     */	XtInheritRealize,
    /* actions		     */	NULL,
    /* num_actions	     */	0,
    /* resources	     */	resources,
    /* num_resources	     */	XtNumber(resources),
    /* xrm_class	     */	NULLQUARK,
    /* compress_motion	     */	TRUE,
    /* compress_exposure     */	XtExposeCompressMultiple,
    /* compress_enterleave   */	TRUE,
    /* visible_interest	     */	FALSE,
    /* destroy		     */	Destroy,
    /* resize		     */	Resize,
    /* expose		     */	NULL,
    /* set_values	     */	SetValues,
    /* set_values_hook       */	NULL,			
    /* set_values_almost     */	XtInheritSetValuesAlmost,  
    /* get_values_hook       */	NULL,			
    /* accept_focus	     */	NULL,
    /* version		     */	XtVersion,
    /* callback offsets      */	NULL,
    /* tm_table              */	NULL,
    /* query_geometry	     */	XtInheritQueryGeometry,
    /* display_accelerator   */	NULL,
    /* extension	     */	NULL,
  },
   /* Composite class part */
  {
    /* geometry_manager	     */	GeometryManager,
    /* change_managed	     */	ChangeManaged,
    /* insert_child	     */	XtInheritInsertChild,
    /* delete_child	     */	XtInheritDeleteChild,
    /* extension	     */	NULL,
  },
   /* Constraint class part */
  {
    /* resources	     */ NULL,
    /* num_resources	     */ 0,
    /* constraint_size	     */ 0,
    /* initialize	     */ NULL,
    /* destroy		     */ NULL,
    /* set_values	     */ NULL,
    /* extension	     */ NULL,
  },
   /* Manager class part */
  {
    /* translations	     */ XtInheritTranslations,
    /* syn_resources	     */ NULL,
    /* num_syn_resources     */ 0,
    /* syn_constraint_resources */ NULL,
    /* num_syn_constraint_resources */ 0,
    /* parent_process	     */ XmInheritParentProcess,
    /* extension	     */ NULL,
  },
   /* FontCreator class part */
  {
    /* extension	     */	NULL,
  }
};

WidgetClass fontCreatorWidgetClass =
	(WidgetClass) &fontCreatorClassRec;
 
static void ClassInitialize(void)
{
    XtInitializeWidgetClass(fontSelectionBoxWidgetClass);

    CSempty = UnsharedCS("");
    opticalSize = Canonical("OpticalSize");
}

/* ARGSUSED */

static void ResizePreview(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    Dimension height;
    Cardinal depth;
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    if (!XtIsRealized(widget) || fc->creator.gstate == 0) return;

    XtVaGetValues(widget, XmNheight, &height,
		XmNdepth, &depth, NULL);

    XDPSSetContextGState(fc->creator.fsb->fsb.context, fc->creator.gstate);

    XDPSSetContextParameters(fc->creator.fsb->fsb.context, XtScreen(widget),
			     depth, XtWindow(widget), height,
			     (XDPSStandardColormap *) NULL,
			     (XDPSStandardColormap *) NULL,
			     XDPSContextScreenDepth | XDPSContextDrawable);

    _DPSFReclip(fc->creator.fsb->fsb.context);

    XDPSUpdateContextGState(fc->creator.fsb->fsb.context, fc->creator.gstate);
}

static void DrawMM(FontCreatorWidget fc)
{
    int i, j;
    String str;
    float p[MAX_AXES];
    float b[MAX_BLENDS];
    int val;
    float size;
    char *chSize;
    DPSContext context;
    Dimension hgt;
    BlendDataRec *bd = fc->creator.font->blend_data;
    float total;
    int bogusFont;

    str = XmTextFieldGetString(fc->creator.display_text_child);

    for (i = 0; i < bd->num_axes; i++) {
	XtVaGetValues(fc->creator.axis_scale_child[i], XmNvalue, &val, NULL);
	p[i] = _FSBNormalize(val, bd, i);
    }

    XtVaGetValues(fc->creator.preview_child, XtNheight, &hgt, NULL);
    context = fc->creator.fsb->fsb.context;
    if (fc->creator.gstate == 0) {
	XDPSSetContextDrawable(context,
			       XtWindow(fc->creator.preview_child), hgt);
	XDPSCaptureContextGState(context, &fc->creator.gstate);
    } else XDPSSetContextGState(context, fc->creator.gstate);

    /* Force b[0] to be 1 - total(b[1..n]) to avoid round-off error */

    total = 0.0;
    for (i = 1; i < bd->num_designs; i++) {
	b[i] = 1.0;
	for (j = 0; j < bd->num_axes; j++) {
	    if (bd->design_positions[i*bd->num_axes + j] == 1.0) b[i] *= p[j];
	    else b[i] *= 1.0 - p[j];
	}
	total += b[i];
    }
    b[0] = 1.0 - total;

    XtVaGetValues(fc->creator.size_text_field_child,
		  XmNvalue, &chSize, NULL); 

    if (chSize == NULL || *chSize == '\0') return;
    size = atof(chSize);

    _DPSFSetUpMM(context, fc->creator.font->font_name,
		 str, size, hgt, b, bd->num_designs, &bogusFont);
    DPSWaitContext(context);
    XClearWindow(XtDisplay(fc->creator.preview_child),
		 XtWindow(fc->creator.preview_child));
    _DPSFDrawMM(context, str, hgt);
}

/* ARGSUSED */

static void DrawMMCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    DrawMM(fc);
}

/* ARGSUSED */

static void ExposeCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    XmDrawingAreaCallbackStruct *dac =
	    (XmDrawingAreaCallbackStruct *) callData;

    if (!fc->creator.preview_fixed) {
	XSetWindowAttributes att;
	att.bit_gravity = ForgetGravity;
	XChangeWindowAttributes(XtDisplay(fc),
				XtWindow(fc->creator.preview_child),
				CWBitGravity, &att);
	fc->creator.preview_fixed = TRUE;
    }

    if (dac != NULL && dac->event->type == Expose &&
	dac->event->xexpose.count != 0) return;

    DrawMM(fc);
}

static void SetUpBlendList(FontCreatorWidget fc)
{
    XmString *CSblends;
    int count, i;
    BlendRec *b;
    char buf[256];
    FontRec *f = fc->creator.font;

    sprintf(buf, "%s Blends", f->face_name);
    XtVaSetValues(fc->creator.blend_label_child,
		  XtVaTypedArg, XmNlabelString, XtRString,
		     buf, strlen(buf)+1,
		  NULL);

    if (f->blend_count == 0) {
	count = 1;
	CSblends = &CSempty;

    } else {
	count = f->blend_count;
	CSblends = (XmString *) XtCalloc(count, sizeof(XmString));

	for (i = 0, b = f->blend_data->blends; i < f->blend_count;
	     i++, b = b->next) {
	    CSblends[i] = b->CS_blend_name;
	}
    }

    XtVaSetValues(fc->creator.blend_scrolled_list_child, XmNitemCount, count,
		  XmNitems, CSblends, NULL);

    if (f->blend_count != 0) XtFree((XtPointer) CSblends);
}

static void CalcCarryValues(FontCreatorWidget fc, FontRec *oldf, int *carry_values)
{
    FontRec *f = fc->creator.font;
    BlendDataRec *bd = f->blend_data, *oldbd = oldf->blend_data;
    int i, j;

    for (i = 0; i < bd->num_axes; i++) {
	carry_values[i] = -1;
	for (j = 0; j < oldbd->num_axes; j++) {
	    if (bd->name[i] == oldbd->name[j]) {
		XmScaleGetValue(fc->creator.axis_scale_child[j],
				carry_values+i);
		break;
	    }
	}
    }
}

static void SetUpAxisLabels(FontCreatorWidget fc, FontRec *oldf, int *carry_values)
{
    int i;
    char buf[20];
    XmString cs;
    BlendDataRec *bd = fc->creator.font->blend_data, *oldbd = 0;
    char *value;

    if (oldf != NULL) oldbd = oldf->blend_data;

    for (i = 0; i < bd->num_axes; i++) {
	if (oldf == NULL || i >= oldbd->num_axes ||
	    oldbd->name[i] != bd->name[i]) {
	    cs = UnsharedCS(bd->name[i]);
	    XtVaSetValues(fc->creator.axis_label_child[i],
			  XmNlabelString, cs, NULL);
	    XmStringFree(cs);
	}
	if (oldf == NULL || i >= oldbd->num_axes ||
	    oldbd->min[i] != bd->min[i]) {
	    sprintf(buf, "%d", bd->min[i]);
	    cs = UnsharedCS(buf);
	    XtVaSetValues(fc->creator.axis_min_label_child[i],
			  XmNlabelString, cs, NULL);
	    XmStringFree(cs);
	}
	if (oldf == NULL || i >= oldbd->num_axes ||
	    oldbd->max[i] != bd->max[i]) {
	    sprintf(buf, "%d", bd->max[i]);
	    cs = UnsharedCS(buf);
	    XtVaSetValues(fc->creator.axis_max_label_child[i],
			  XmNlabelString, cs, NULL);
	    XmStringFree(cs);
	}
	if (oldf == NULL || carry_values[i] == -1) {
	    if (bd->name[i] == opticalSize &&
		XmToggleButtonGadgetGetState(
			fc->creator.follow_size_toggle_child)) {
		XtVaGetValues(fc->creator.fsb->fsb.size_text_field_child,
			      XmNvalue, &value, NULL);
		if (value == NULL || *value == '\0') {
		    carry_values[i] = bd->min[i];
		} else carry_values[i] = atof(value) + 0.5;
	    } else carry_values[i] = bd->min[i];
	}
	if (carry_values[i] < bd->min[i]) carry_values[i] = bd->min[i];
	else if (carry_values[i] > bd->max[i]) carry_values[i] = bd->max[i];
	XtVaSetValues(fc->creator.axis_scale_child[i],
		      XmNminimum, bd->min[i], XmNmaximum, bd->max[i],
		      XmNvalue, carry_values[i], NULL);
    }
}

static void ManageAxes(FontCreatorWidget fc)
{
    Widget w[5*MAX_AXES];
    int i, j;
    int diff;

    diff = fc->creator.managed_axes - fc->creator.font->blend_data->num_axes;

    if (diff == 0) return;

    if (diff < 0) {
	for (i = fc->creator.managed_axes, j=0; j < -diff * 5; i++, j+=5) {
	    w[j] = fc->creator.axis_label_child[i];
	    w[j+1] = fc->creator.axis_scale_child[i];
	    w[j+2] = fc->creator.axis_value_text_child[i];
	    w[j+3] = fc->creator.axis_min_label_child[i];
	    w[j+4] = fc->creator.axis_max_label_child[i];
	}
	XtManageChildren(w, -diff * 5);
    } else {
	for (i = fc->creator.font->blend_data->num_axes, j=0; j < diff * 5;
	     i++, j+=5) {
	    w[j] = fc->creator.axis_label_child[i];
	    w[j+1] = fc->creator.axis_scale_child[i];
	    w[j+2] = fc->creator.axis_value_text_child[i];
	    w[j+3] = fc->creator.axis_min_label_child[i];
	    w[j+4] = fc->creator.axis_max_label_child[i];
	}
	XtUnmanageChildren(w, diff * 5);
    }
    fc->creator.managed_axes = fc->creator.font->blend_data->num_axes;
}

static void SetScaleValues(FontCreatorWidget fc)
{
    int val;
    char buf[32];
    int i, axes;

    axes = fc->creator.font->blend_data->num_axes;

    for (i = 0; i < axes; i++) {
	XmScaleGetValue(fc->creator.axis_scale_child[i], &val);
	sprintf(buf, "%d", val);
	XmTextFieldSetString(fc->creator.axis_value_text_child[i], buf);
    }
}

static void SetUpAxes(FontCreatorWidget fc, FontRec *oldf)
{
    int carry_values[MAX_AXES];

    if (oldf != NULL) CalcCarryValues(fc, oldf, carry_values);
    SetUpAxisLabels(fc, oldf, carry_values);
    SetScaleValues(fc);
    ManageAxes(fc);
}

/* ARGSUSED */

static void FaceSelect(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XmListCallbackStruct *listCB = (XmListCallbackStruct *) callData;
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    FontRec *f, *oldf = fc->creator.font;
    int i;

    i = 0;
    f = fc->creator.family->fonts;
    while (f != NULL) {
	if (f->blend_data != NULL) i++;
        if (i == listCB->item_position) break;
	f = f->next;
    }

    if (f == NULL) return;
    if (!_FSBDownloadFontIfNecessary(f, fc->creator.fsb)) {
	_FSBFlushFont(fc->creator.fsb, f);
	return;
    }
    if (fc->creator.font != NULL) fc->creator.font->in_font_creator = False;
    fc->creator.font = f;
    f->in_font_creator = True;
    SetUpBlendList(fc);
    SetUpAxes(fc, oldf);

    DrawMM(fc);
}

static void HandleSelectedBlend(FontCreatorWidget fc, int n)
{
    BlendDataRec *bd = fc->creator.font->blend_data;
    BlendRec *b;
    int i;
    int value;
    char buf[32];

    b = bd->blends;
    /* List uses 1-based addressing!! */
    for (i = 1; i < n; i++) b = b->next;
    
    XmTextFieldSetString(fc->creator.name_text_child, b->blend_name);

    for (i = 0; i < bd->num_axes; i++) {
	value = _FSBUnnormalize(b->data[i], bd, i);
	XmScaleSetValue(fc->creator.axis_scale_child[i], value);
	sprintf(buf, "%d", value);
	XmTextFieldSetString(fc->creator.axis_value_text_child[i], buf);
    }
}

/* ARGSUSED */

static void BlendSelect(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XmListCallbackStruct *listCB = (XmListCallbackStruct *) callData;
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    if (fc->creator.font->blend_count == 0) return;

    HandleSelectedBlend(fc, listCB->item_position);

    DrawMM(fc);
}

/* ARGSUSED */

static void SetValue(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XmScaleCallbackStruct *scaleData = (XmScaleCallbackStruct *) callData;
    Widget text = (Widget) clientData;
    char buf[32];

    sprintf(buf, "%d", scaleData->value);
    XmTextFieldSetString(text, buf);
}

/* ARGSUSED */

static void SetScale(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    Widget scale = (Widget) clientData;
    char *value;
    int val, min, max;
    char buf[32];

    value = XmTextFieldGetString(widget);
    val = atoi(value);
    XtVaGetValues(scale, XmNminimum, &min, XmNmaximum, &max, NULL);
    if (val < min) val = min;
    if (val > max) val = max;
    XmScaleSetValue(scale, val);

    /* Handle range and illegal characters this way...*/

    sprintf(buf, "%d", val);
    XmTextFieldSetString(widget, buf);
}

/* ARGSUSED */

static void DeleteMessage(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XtDestroyWidget(widget);
}

static void PutUpDialog(FontCreatorWidget fc, char *name)
{
    Widget message, w;

    message = XmCreateInformationDialog((Widget) fc, name, (ArgList) NULL, 0);
    w =	XmMessageBoxGetChild(message, XmDIALOG_CANCEL_BUTTON);
    XtUnmanageChild(w);
    w =	XmMessageBoxGetChild(message, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(w);
    XtAddCallback(message, XmNokCallback, DeleteMessage, (XtPointer) NULL);
    
    XtManageChild(message);
}

static void NoName(FontCreatorWidget fc)
{
    PutUpDialog(fc, "noNameMessage");
}

static void UsedName(FontCreatorWidget fc)
{
    PutUpDialog(fc, "usedNameMessage");
}

static void SomeUsedName(FontCreatorWidget fc)
{
    PutUpDialog(fc, "someUsedNameMessage");
}

static void NoSuchName(FontCreatorWidget fc)
{
    PutUpDialog(fc, "noSuchNameMessage");
}

static Boolean DoAdd(FontCreatorWidget fc, FontRec *f, String name)
{
    char *spaceName;
    BlendRec *b, *newb, **last;
    BlendDataRec *bd = f->blend_data;
    int val[MAX_AXES], i;

    for (b = bd->blends; b != NULL; b = b->next) {
	if (strcmp(name, b->blend_name) == 0) return True;
    }

    newb = (BlendRec *) XtMalloc(sizeof(BlendRec));
    newb->blend_name = Canonical(name);
    newb->CS_blend_name = CS(newb->blend_name, (Widget) fc);

    spaceName = (char *) XtMalloc(strlen(name) + 4);
    spaceName[0] = spaceName[1] = spaceName[2] = ' ';
    strcpy(spaceName+3, name);
    newb->CS_space_blend_name = CS(spaceName, (Widget) fc);
    XtFree((XtPointer) spaceName);

    for (i = 0; i < bd->num_axes; i++) {
	XtVaGetValues(fc->creator.axis_scale_child[i],
		      XmNvalue, val+i, NULL);
	newb->data[i] = _FSBNormalize(val[i], bd, i);
    }
    for (/**/; i < MAX_AXES; i++) newb->data[i] = 0.0;

    newb->font_name = _FSBGenFontName(f->font_name, val, bd);

    f->blend_count++;
    fc->creator.family->blend_count++;

    last = &bd->blends;
    for (b = bd->blends; b != NULL; b = b->next) {
	if (strcmp(name, b->blend_name) < 0) break;
	last = &b->next;
    }
    newb->next = b;
    *last = newb;

    SetUpBlendList(fc);
    _FSBSetUpFaceList(fc->creator.fsb, False);
    return False;
}

/* ARGSUSED */

static void AddCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    char *value;
    FontRec *f;
    Boolean failures = False;
    BlendDataRec *bd = fc->creator.font->blend_data;
    int i;

    value = XmTextFieldGetString(fc->creator.name_text_child);

    if (value == NULL || *value == '\0') {
	NoName(fc);
	return;
    }

    if (XmToggleButtonGadgetGetState(fc->creator.do_all_toggle_child)) {
	for (f = fc->creator.family->fonts; f != NULL; f = f->next) {
	    if (f->blend_data != NULL &&
		f->blend_data->num_axes == bd->num_axes) {
		for (i = 0; i < bd->num_axes; i++) {
		    if (f->blend_data->name[i] != bd->name[i]) break;
		}
		if (i == bd->num_axes) failures |= DoAdd(fc, f, value);
	    }
	}
	if (failures) SomeUsedName(fc);
    } else if (DoAdd(fc, fc->creator.font, value)) UsedName(fc);
}

static Boolean DoReplace(FontCreatorWidget fc, FontRec *f, String name)
{
    BlendDataRec *bd = f->blend_data;
    BlendRec *b;
    int val[MAX_AXES], i;

    name = Canonical(name);
    for (b = bd->blends; b != NULL; b = b->next) {
	if (name == b->blend_name) {
	    for (i = 0; i < bd->num_axes; i++) {
		XtVaGetValues(fc->creator.axis_scale_child[i],
			      XmNvalue, val+i, NULL);
		b->data[i] = _FSBNormalize(val[i], bd, i);
	    }
	    b->font_name = _FSBGenFontName(f->font_name, val, bd);
	    if (fc->creator.fsb->fsb.currently_selected_blend == b) {
		_FSBSetUpFaceList(fc->creator.fsb, True);
	    }
	    return False;
	}
    }
    return True;
}

/* ARGSUSED */

static void ReplaceCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    char *value;
    FontRec *f;
    Boolean failures = True;
    BlendDataRec *bd = fc->creator.font->blend_data;
    int i;

    value = XmTextFieldGetString(fc->creator.name_text_child);

    if (value == NULL || *value == '\0') {
	NoName(fc);
	return;
    }

    if (XmToggleButtonGadgetGetState(fc->creator.do_all_toggle_child)) {
	for (f = fc->creator.family->fonts; f != NULL; f = f->next) {
	    if (f->blend_data != NULL &&
		f->blend_data->num_axes == bd->num_axes) {
		for (i = 0; i < bd->num_axes; i++) {
		    if (f->blend_data->name[i] != bd->name[i]) break;
		}
		if (i == bd->num_axes) failures &= DoReplace(fc, f, value);
	    }
	}
	if (failures) NoSuchName(fc);
    } else if (DoReplace(fc, fc->creator.font, value)) NoSuchName(fc);
}

static Boolean DoDelete(FontCreatorWidget fc, FontRec *f, String name)
{
    BlendDataRec *bd = f->blend_data;
    BlendRec *b, *oldb;
    Boolean current = FALSE;

    name = Canonical(name);
    for (b = bd->blends, oldb = NULL; b != NULL; oldb = b, b = b->next) {
	if (name == b->blend_name) {
	    if (oldb == NULL) bd->blends = b->next;
	    else oldb->next = b->next;
	    if (fc->creator.fsb->fsb.currently_selected_blend == b) {
		fc->creator.fsb->fsb.currently_selected_blend = NULL;
		current = TRUE;
	    }
	    XtFree((XtPointer) b);
	    f->blend_count--;
	    fc->creator.family->blend_count--;
	    SetUpBlendList(fc);
	    _FSBSetUpFaceList(fc->creator.fsb, current);
	    return False;
	}
    }
    return True;
}

/* ARGSUSED */

static void DeleteCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    char *value;
    FontRec *f;
    Boolean failures = True;

    value = XmTextFieldGetString(fc->creator.name_text_child);

    if (value == NULL || *value == '\0') {
	NoName(fc);
	return;
    }

    if (XmToggleButtonGadgetGetState(fc->creator.do_all_toggle_child)) {
	for (f = fc->creator.family->fonts; f != NULL; f = f->next) {
	    if (f->blend_data != NULL) {
		failures &= DoDelete(fc, f, value);
	    }
	}
	if (failures) NoSuchName(fc);
    } else if (DoDelete(fc, fc->creator.font, value)) NoSuchName(fc);
}

/* ARGSUSED */

static void UnmanageOptions(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    XtUnmanageChild(fc->creator.option_box);
}

/* ARGSUSED */

static void ShowOptions(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    XtManageChild(fc->creator.option_box);
}

/* ARGSUSED */

static void GenerateCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    BlendDataRec *bd = fc->creator.font->blend_data;
    int i, val[MAX_AXES];
    char nameBuf[256];
    char *ch;

    for (i = 0; i < bd->num_axes; i++) {
	XtVaGetValues(fc->creator.axis_scale_child[i],
		      XmNvalue, val+i, NULL);
    }

    ch = nameBuf;

    for (i = 0; i < bd->num_axes - 1; i++) {
	sprintf(ch, "%d ", val[i]);
	ch = ch + strlen(ch);
    }

    sprintf(ch, "%d", val[bd->num_axes - 1]);

    XmTextFieldSetString(fc->creator.name_text_child, nameBuf);
}    

/* ARGSUSED */

static void DismissCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    if (XtIsShell(XtParent(fc))) XtPopdown(XtParent(fc));
    XtCallCallbackList(widget, fc->creator.dismiss_callback, (XtPointer) NULL);
}

/* ARGSUSED */

static void SizeChanged(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    String value;
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    int size;
    FontRec *f = fc->creator.font;
    BlendDataRec *bd;
    int i;
    char buf[32];

    if (f == NULL || f->blend_data == NULL) return;

    /* See if we have an optical size scale */
    bd = f->blend_data;

    for (i = 0; i < bd->num_axes; i++) {
	if (bd->name[i] == opticalSize) break;
    }
    if (i == bd->num_axes) return;

    if (!XmToggleButtonGadgetGetState(fc->creator.follow_size_toggle_child)) {
	return;
    }

    XtVaGetValues(widget, XmNvalue, &value, NULL);

    if (value == NULL || *value == '\0') return;
    size = atof(value) + 0.5;
    sprintf(buf, "%d", size);
    XmTextFieldSetString(fc->creator.axis_value_text_child[i], buf);

    SetScale(fc->creator.axis_value_text_child[i],
	     (XtPointer) fc->creator.axis_scale_child[i], (XtPointer) NULL);
    DrawMM(fc);
}

/* There's a problem; sometimes the change has already been made in the field,
   and sometimes it hasn't.  The times when it has seem to correspond to
   making changes with the size option menu, so we use this disgusting
   global flag to notice when this happens.  We also use this to tell whether
   or not the change is coming from internal to the widget or as a result
   of user interaction.  */

static Boolean changingSize = False;

/* ARGSUSED */

static void SizeSelect(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    String value;
    Widget option;
    FontCreatorWidget fc = (FontCreatorWidget) clientData;
    char *ch;

    XtVaGetValues(widget, XmNvalue, &value, NULL);

    if (value == NULL) option = fc->creator.other_size;
    else {
	for (ch = value; *ch != '\0'; ch++) if (*ch == '.') *ch = '-';

	option = XtNameToWidget(fc->creator.size_menu, value);
	if (option == NULL) option = fc->creator.other_size;
    }

    XtVaSetValues(fc->creator.size_option_menu_child,
		  XmNmenuHistory, option, NULL);
}

/* ARGSUSED */

static void TextVerify(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XmTextVerifyPtr v = (XmTextVerifyPtr) callData;
    char ch, *cp;
    int decimalPoints = 0;
    int i;

    if (changingSize) return;	/* We know what we're doing; allow it */

    /* Should probably look at format field, but seems to contain garbage */

    if (v->text->length == 0) return;

    for (i = 0; i < v->text->length; i++) {
	ch = v->text->ptr[i];
	if (ch == '.') decimalPoints++;
	else if (!isdigit(ch)) {
	    v->doit = False;
	    return;
	}
    }

    if (decimalPoints > 1) {
	v->doit = False;
	return;
    }

    XtVaGetValues(widget, XmNvalue, &cp, NULL);

    for (/**/; *cp != '\0'; cp++) {
	if (*cp == '.') decimalPoints++;
    }

    if (decimalPoints > 1) v->doit = False;
}

/* ARGSUSED */

static void SetSize(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    char buf[20];
    char *ch;
    FontCreatorWidget fc = (FontCreatorWidget) clientData;

    strcpy(buf, XtName(widget));
    for (ch = buf; *ch != '\0'; ch++) if (*ch == '-') *ch++ = '.';

    changingSize = True;
    XtVaSetValues(fc->creator.size_text_field_child, XmNvalue, buf, NULL);
    changingSize = False;
}

/* This makes sure the selected item is visible */

static void ListSelectPos(Widget w, int pos, Boolean notify)
{
    int topPos, items, visible;

    XmListSelectPos(w, pos, notify);
    
    XtVaGetValues(w, XmNtopItemPosition, &topPos,
		XmNvisibleItemCount, &visible,
		XmNitemCount, &items, NULL);

    if (pos >= topPos && pos < topPos + visible) return;
    topPos = pos - (visible-1)/2;
    if (topPos + visible > items) topPos = items - visible + 1;
    if (topPos < 1) topPos = 1;

    XtVaSetValues(w, XmNtopItemPosition, topPos, NULL);
}

static void CreateSizeMenu(
    FontCreatorWidget fc,
    Boolean destroyOldChildren)
{
    Arg args[20];
    int i, j;
    Widget *sizes;
    char buf[20];
    Widget *children;
    Cardinal num_children;
    XmString csName;
    char *ch;

    if (destroyOldChildren) {
	XtVaGetValues(fc->creator.size_menu, XtNchildren, &children,
		      XtNnumChildren, &num_children, NULL);
	
	/* Don't destroy first child ("other") */
	for (j = 1; (Cardinal)j < num_children; j++) XtDestroyWidget(children[j]);

	sizes = (Widget *) XtMalloc((fc->creator.size_count+1) *
				    sizeof(Widget));
	sizes[0] = children[0];
    } else {
	sizes = (Widget *) XtMalloc((fc->creator.size_count+1) *
				    sizeof(Widget));
	i = 0;
	fc->creator.other_size = sizes[0] =
		XtCreateManagedWidget("other", xmPushButtonGadgetClass,
				  fc->creator.size_menu, args, i);
    }

    for (j = 0; j < fc->creator.size_count; j++) {
	(void) sprintf(buf, "%g", fc->creator.sizes[j]);
	csName = UnsharedCS(buf);
	for (ch = buf; *ch != '\0'; ch++) if (*ch == '.') *ch = '-';
	i = 0;
	XtSetArg(args[i], XmNlabelString, csName);			i++;
	sizes[j+1] =
		XmCreatePushButtonGadget(fc->creator.size_menu, buf, args, i);
	XmStringFree(csName);
	XtAddCallback(sizes[j+1], XmNactivateCallback,
		      SetSize, (XtPointer) fc);
	XtAddCallback(sizes[j+1], XmNactivateCallback,
		      DrawMMCallback, (XtPointer) fc);
    }
    XtManageChildren(sizes, j+1);
    XtFree((char *) sizes);
}

static void CreateChildren(FontCreatorWidget fc)
{
    Arg args[20];
    int i, j;
    Widget form, prev, w, label, sep, button;
    char buf[20];

    i = 0;
    fc->creator.pane_child =
	    XtCreateManagedWidget("pane", xmPanedWindowWidgetClass,
				  (Widget) fc, args, i);

    i = 0;
    fc->creator.preview_child =
	    XtCreateManagedWidget("preview", xmDrawingAreaWidgetClass,
				  fc->creator.pane_child, args, i);
    XtAddCallback(fc->creator.preview_child, XmNexposeCallback,
		  ExposeCallback, (XtPointer) fc);
    XtAddCallback(fc->creator.preview_child, XmNresizeCallback,
		  ResizePreview, (XtPointer) fc);

    i = 0;	
    form = XtCreateManagedWidget("panel", xmFormWidgetClass,
				 fc->creator.pane_child, args, i);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("deleteButton", xmPushButtonGadgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback, DeleteCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, button);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("replaceButton", xmPushButtonGadgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback,
		  ReplaceCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, button);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("addButton", xmPushButtonGadgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback, AddCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, button);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    fc->creator.generate_button_child =
	    XtCreateManagedWidget("generateButton", xmPushButtonGadgetClass,
				  form, args, i);
    XtAddCallback(fc->creator.generate_button_child, XmNactivateCallback,
		  GenerateCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, fc->creator.generate_button_child);i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("optionsButton", xmPushButtonGadgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback, ShowOptions, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("dismissButton", xmPushButtonGadgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback,
		  DismissCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, button);				i++;
    sep = XtCreateManagedWidget("separator", xmSeparatorGadgetClass,
				form, args, i);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, sep);				i++;
    label = XtCreateManagedWidget("sizeLabel", xmLabelGadgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, label);				i++;
    fc->creator.size_text_field_child =
	    XtCreateManagedWidget("sizeTextField", xmTextFieldWidgetClass,
				  form, args, i);
    XtAddCallback(fc->creator.size_text_field_child, XmNvalueChangedCallback,
		  SizeSelect, (XtPointer) fc);
    XtAddCallback(fc->creator.size_text_field_child, XmNmodifyVerifyCallback,
		  TextVerify, (XtPointer) fc);
    XtAddCallback(fc->creator.size_text_field_child, XmNactivateCallback,
		  DrawMMCallback, (XtPointer) fc);

    i = 0;
    fc->creator.size_menu = XmCreatePulldownMenu(form, "sizeMenu", args, i);

    CreateSizeMenu(fc, FALSE);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, fc->creator.size_text_field_child);i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, label);				i++;
    XtSetArg(args[i], XmNsubMenuId, fc->creator.size_menu);		i++;
    fc->creator.size_option_menu_child =
	    XmCreateOptionMenu(form, "sizeOptionMenu", args, i);
    XtManageChild(fc->creator.size_option_menu_child);

    SizeSelect(fc->creator.size_text_field_child, (XtPointer) fc,
	       (XtPointer) NULL);

    i = 0;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, sep);				i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    fc->creator.name_text_child =
	    XtCreateManagedWidget("nameText", xmTextFieldWidgetClass,
				  form, args, i);
    XtAddCallback(fc->creator.name_text_child, XmNactivateCallback,
		  AddCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, fc->creator.name_text_child);	i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    label = XtCreateManagedWidget("nameLabel", xmLabelGadgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);			i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    label = XtCreateManagedWidget("faceLabel",xmLabelGadgetClass,
				  form, args, i); 

    i = 0;
    XtSetArg(args[i], XmNitemCount, 1);					i++;
    XtSetArg(args[i], XmNitems, &CSempty);				i++;
    fc->creator.face_scrolled_list_child =
	    XmCreateScrolledList(form, "faceList", args, i);
    XtAddCallback(fc->creator.face_scrolled_list_child,
		  XmNbrowseSelectionCallback, FaceSelect, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, label);				i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_POSITION);		i++;
    XtSetValues(XtParent(fc->creator.face_scrolled_list_child), args, i);
    XtManageChild(fc->creator.face_scrolled_list_child);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    fc->creator.blend_label_child =
	    XtCreateManagedWidget("blendLabel",xmLabelGadgetClass,
				  form, args, i); 

    i = 0;
    XtSetArg(args[i], XmNitemCount, 1);					i++;
    XtSetArg(args[i], XmNitems, &CSempty);				i++;
    fc->creator.blend_scrolled_list_child =
	    XmCreateScrolledList(form, "blendList", args, i);
    XtAddCallback(fc->creator.blend_scrolled_list_child,
		  XmNbrowseSelectionCallback, BlendSelect, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, fc->creator.blend_label_child);	i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fc->creator.name_text_child);	i++;
    XtSetValues(XtParent(fc->creator.blend_scrolled_list_child), args, i);
    XtManageChild(fc->creator.blend_scrolled_list_child);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNtopWidget,
	     XtParent(fc->creator.face_scrolled_list_child));		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    fc->creator.display_text_child =
	    XtCreateManagedWidget("displayText", xmTextFieldWidgetClass,
				  form, args, i);
    XtAddCallback(fc->creator.display_text_child, XmNactivateCallback,
		  DrawMMCallback, (XtPointer) fc);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNtopWidget, fc->creator.display_text_child);	i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    label = XtCreateManagedWidget("displayTextLabel", xmLabelGadgetClass,
				   form, args, i);

    prev = fc->creator.display_text_child;

    for (j = 0; j < 4; j++) {
	i = 0;
	XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);	i++;
	XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
	XtSetArg(args[i], XmNtopWidget, prev);				i++;
	sprintf(buf, "axisValue%d", j+1);
	fc->creator.axis_value_text_child[j] =
		XtCreateWidget(buf, xmTextFieldWidgetClass, form, args, i);

	i = 0;
	XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);	i++;
	XtSetArg(args[i], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
	XtSetArg(args[i], XmNtopWidget,
		 fc->creator.axis_value_text_child[j]);			i++;
	XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
	XtSetArg(args[i], XmNrightWidget,
		 fc->creator.axis_value_text_child[j]);			i++;
	sprintf(buf, "axisScale%d", j+1);
	fc->creator.axis_scale_child[j] =
		XtCreateWidget(buf, xmScaleWidgetClass, form, args, i);
	XtAddCallback(fc->creator.axis_scale_child[j],
		      XmNvalueChangedCallback, DrawMMCallback, (XtPointer) fc);
	XtAddCallback(fc->creator.axis_scale_child[j],
		      XmNdragCallback, DrawMMCallback, (XtPointer) fc);
	XtAddCallback(fc->creator.axis_scale_child[j],
		      XmNvalueChangedCallback, SetValue,
		      (XtPointer) fc->creator.axis_value_text_child[j]);
	XtAddCallback(fc->creator.axis_scale_child[j],
		      XmNdragCallback, SetValue,
		      (XtPointer) fc->creator.axis_value_text_child[j]);
	XtAddCallback(fc->creator.axis_value_text_child[j],
		      XmNactivateCallback, SetScale,
		      (XtPointer) fc->creator.axis_scale_child[j]);

	i = 0;
	XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
	XtSetArg(args[i], XmNtopWidget,
		 fc->creator.axis_scale_child[j]);			i++;
	XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);i++;
	XtSetArg(args[i], XmNrightWidget,
		 fc->creator.axis_scale_child[j]);			i++;
	sprintf(buf, "axisMax%d", j+1);
	fc->creator.axis_max_label_child[j] =
		XtCreateWidget(buf, xmLabelGadgetClass, form, args, i);

	i = 0;
	XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
	XtSetArg(args[i], XmNtopWidget,
		 fc->creator.axis_scale_child[j]);			i++;
	XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
	XtSetArg(args[i], XmNleftWidget,
		 fc->creator.axis_scale_child[j]);			i++;
	sprintf(buf, "axisMin%d", j+1);
	fc->creator.axis_min_label_child[j] =
		XtCreateWidget(buf, xmLabelGadgetClass, form, args, i);

	i = 0;
	XtSetArg(args[i], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
	XtSetArg(args[i], XmNtopWidget,
		 fc->creator.axis_value_text_child[j]);			i++;
	XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);	i++;
	sprintf(buf, "axisLabel%d", j+1);
	fc->creator.axis_label_child[j] =
		XtCreateWidget(buf, xmLabelGadgetClass, form, args, i);

	prev = fc->creator.axis_value_text_child[j];
    }

    /* Create the options box so we have the toggles */

    fc->creator.option_box = XmCreateFormDialog((Widget) fc, "optionBox",
						(Arg *) NULL, 0);
    w = XtCreateManagedWidget("filterBox", xmRowColumnWidgetClass,
			      fc->creator.option_box, (Arg *) NULL, 0);
    fc->creator.do_all_toggle_child = 
	    XtCreateManagedWidget("doAllToggle", xmToggleButtonGadgetClass,
				  w, (Arg *) NULL, 0);
    fc->creator.follow_size_toggle_child =
	    XtCreateManagedWidget("followSizeToggle",
				  xmToggleButtonGadgetClass,
				  w, (Arg *) NULL, 0);
    button = XtCreateManagedWidget("dismissOptionButton",
				   xmPushButtonGadgetClass,
				   w, (Arg *) NULL, 0);
    XtAddCallback(button, XmNactivateCallback,
		  UnmanageOptions, (XtPointer) fc);
}

/* ARGSUSED */

static void Initialize(
    Widget request, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    FontCreatorWidget fc = (FontCreatorWidget) new;

    /* Must have a fsb */

    if (fc->creator.fsb == NULL) {
        XtAppErrorMsg(XtWidgetToApplicationContext(new),
		      "initializeFontCreator", "noFontSelectionBox",
		      "FontSelectionBoxError",
		      "No font selection box given to font creator",
		      (String *) NULL, (Cardinal *) NULL);
    }

    /* Verify size list */

    if (fc->creator.size_count > 0 && fc->creator.sizes == NULL) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontCreator", "sizeMismatch",
			"FontSelectionBoxError",
			"Size count specified but no sizes present",
			(String *) NULL, (Cardinal *) NULL);
	fc->creator.size_count = 0;
    }

    if (fc->creator.size_count < 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontCreator", "negativeSize",
			"FontSelectionBoxError",
			"Size count should not be negative",
			(String *) NULL, (Cardinal *) NULL);
	fc->creator.size_count = 0;
    }

    fc->creator.gstate = 0;
    fc->creator.family = NULL;
    fc->creator.font = NULL;
    fc->creator.managed_axes = 0;
    fc->creator.preview_fixed = False;
    fc->creator.option_box = NULL;

    CreateChildren(fc);
    XtAddCallback(fc->creator.fsb->fsb.size_text_field_child,
		  XmNvalueChangedCallback, SizeChanged, (XtPointer) fc);
}

static void SelectBlend(FontCreatorWidget fc, BlendRec *cur_b)
{
    int i, cur = 0;
    BlendRec *b;
    int *selectList, selectCount;

    if (cur_b == NULL) {
	if (!XmListGetSelectedPos(fc->creator.blend_scrolled_list_child,
				  &selectList, &selectCount)) return;
	if (selectCount == 0 || *selectList < 1) return;
	cur = *selectList;
	XtFree((XtPointer) selectList);
    } else {
	for (i = 0, b = fc->creator.font->blend_data->blends;
	     i < fc->creator.font->blend_count; i++, b = b->next) {
	    if (b == cur_b) {
		cur = i+1;
		break;
	    }
	}
    }
    ListSelectPos(fc->creator.blend_scrolled_list_child, cur, FALSE);
    HandleSelectedBlend(fc, cur);
}

void _FSBSetCreatorFamily(Widget w, FontFamilyRec *ff)
{
    FontCreatorWidget fc = (FontCreatorWidget) w;
    int i, count = 0, cur = 1;
    FontRec *newf = NULL, *f, *oldf = fc->creator.font;
    XmString *CSfaces;

    if (ff != fc->creator.family) {
	fc->creator.family = ff;

	CSfaces = (XmString *) XtCalloc(ff->font_count, sizeof(XmString));

	for (i = 0, f = ff->fonts; i < ff->font_count; i++, f = f->next) {
	    if (f->blend_data == NULL) continue;

	    if (newf == NULL) newf = f;
	    CSfaces[count] = f->CS_face_name;
	    count++;
	    if (f == fc->creator.fsb->fsb.currently_selected_face) {
		cur = count;
		newf = f;
	    }
	}

	XtVaSetValues(fc->creator.face_scrolled_list_child,
		      XmNitemCount, count, XmNitems, CSfaces, NULL);

	XtFree((XtPointer) CSfaces);

    } else {
	for (i = 0, f = ff->fonts; i < ff->font_count; i++, f = f->next) {
	    if (f->blend_data == NULL) continue;
	    count++;
	    if (newf == NULL) newf = f;
	    if (f == fc->creator.fsb->fsb.currently_selected_face) {
		cur = count;
		newf = f;
		break;
	    }
	}
    }

    if (fc->creator.font != NULL) fc->creator.font->in_font_creator = False;
    fc->creator.font = newf;
    newf->in_font_creator = True;
    ListSelectPos(fc->creator.face_scrolled_list_child, cur, FALSE);
    SetUpBlendList(fc);
    SetUpAxes(fc, oldf);
    if (fc->creator.fsb->fsb.currently_selected_blend != 0) {
	SelectBlend(fc, fc->creator.fsb->fsb.currently_selected_blend);
    } else {
	SelectBlend(fc, NULL);
    }
    SetScaleValues(fc);
    XmTextFieldSetString(fc->creator.display_text_child, ff->family_name);
    DrawMM(fc);
}
    
static void Destroy(Widget widget)
{
    FontCreatorWidget fc = (FontCreatorWidget) widget;

    if (fc->creator.gstate != 0) {
	XDPSFreeContextGState(fc->creator.fsb->fsb.context,
			      fc->creator.gstate);
    }
}

static void Resize(Widget widget)
{
    FontCreatorWidget fc = (FontCreatorWidget) widget;

    XtResizeWidget(fc->creator.pane_child, fc->core.width, fc->core.height, 0);
}

/* ARGSUSED */

static Boolean SetValues(
    Widget old, Widget req, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    FontCreatorWidget oldfc = (FontCreatorWidget) old;
    FontCreatorWidget newfc = (FontCreatorWidget) new;

#define NE(field) newfc->creator.field != oldfc->creator.field

    if (NE(fsb)) newfc->creator.fsb = oldfc->creator.fsb;

    if (newfc->creator.size_count > 0 && newfc->creator.sizes == NULL) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontCreator", "sizeMismatch",
			"FontSelectionBoxError",
			"Size count specified but no sizes present",
			(String *) NULL, (Cardinal *) NULL);
	newfc->creator.size_count = 0;
    }

    if (newfc->creator.size_count < 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontCreator", "negativeSize",
			"FontSelectionBoxError",
			"Size count should not be negative",
			(String *) NULL, (Cardinal *) NULL);
	newfc->creator.size_count = 0;
    }

    if (NE(sizes)) CreateSizeMenu(newfc, TRUE);
#undef NE
    return False;
}

/* ARGSUSED */

static XtGeometryResult GeometryManager(
    Widget w,
    XtWidgetGeometry *desired, XtWidgetGeometry *allowed)
{
#define WANTS(flag) (desired->request_mode & flag)

    if (WANTS(XtCWQueryOnly)) return XtGeometryYes;

    if (WANTS(CWWidth)) w->core.width = desired->width;
    if (WANTS(CWHeight)) w->core.height = desired->height;
    if (WANTS(CWX)) w->core.x = desired->x;
    if (WANTS(CWY)) w->core.y = desired->y;
    if (WANTS(CWBorderWidth)) {
	w->core.border_width = desired->border_width;
    }

    return XtGeometryYes;
#undef WANTS
}

static void ChangeManaged(Widget w)
{
    FontCreatorWidget fc = (FontCreatorWidget) w;

    w->core.width = fc->composite.children[0]->core.width;
    w->core.height = fc->composite.children[0]->core.height;
}
