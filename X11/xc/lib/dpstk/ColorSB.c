/* 
 * ColorSB.c
 *
 * (c) Copyright 1993-1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/lib/dpstk/ColorSB.c,v 1.2 2000/06/07 22:02:58 tsi Exp $ */

#ifndef X_NOT_POSIX
#include <unistd.h>
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/ShellP.h>
#include <stdlib.h>
#include <Xm/Xm.h>

/* There are no words to describe how I feel about having to do this */

#if XmVersion > 1001		
#include <Xm/ManagerP.h>
#else
#include <Xm/XmP.h>
#endif

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/DrawingA.h>
#include <Xm/Scale.h>
#include <Xm/RowColumn.h>
#include <Xm/Frame.h>
#include <Xm/MessageB.h>

#include <DPS/dpsXclient.h>
#include "dpsXcommonI.h"
#include <DPS/dpsXshare.h>
#include "eyedrop16.xbm"
#include "eyedropmask16.xbm"
#include "eyedrop32.xbm"
#include "eyedropmask32.xbm"
#include "heyedrop.xbm"
#include "square.xbm"
#include "squaremask.xbm"
#include "CSBwraps.h"
#include <math.h>
#include <stdio.h>
#include <pwd.h>
#include <DPS/ColorSBP.h>

#define PATH_BUF_SIZE 1024

/* Turn a string into a compound string */
#define CS(str, w) CreateSharedCS(str, w)

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define TO_PCT(val) ((int) (val * 100.0 + 0.5))
#define TO_X(color) ((color) * 65535)

#define Offset(field) XtOffsetOf(ColorSelectionBoxRec, csb.field)

static XtResource resources[] = {
    {XtNcontext, XtCContext, XtRDPSContext, sizeof(DPSContext),
	Offset(context), XtRDPSContext, (XtPointer) NULL},
    {XtNrgbLabels, XtCRgbLabels, XtRString, sizeof(String),
	Offset(rgb_labels), XtRString, (XtPointer) "R:G:B"},
    {XtNcmykLabels, XtCCmykLabels, XtRString, sizeof(String),
	Offset(cmyk_labels), XtRString, (XtPointer) "C:M:Y:K"},
    {XtNhsbLabels, XtCHsbLabels, XtRString, sizeof(String),
	Offset(hsb_labels), XtRString, (XtPointer) "H:S:B"},
    {XtNgrayLabels, XtCGrayLabels, XtRString, sizeof(String),
	Offset(gray_labels), XtRString, (XtPointer) "Gray"},
    {XtNcellSize, XtCCellSize, XtRDimension, sizeof(Dimension),
	Offset(cell_size), XtRImmediate, (XtPointer) 15},
    {XtNnumCells, XtCNumCells, XtRShort, sizeof(short),
	Offset(num_cells), XtRImmediate, (XtPointer) 30},
    {XtNfillMe, XtCFillMe, XtRString, sizeof(String),
	Offset(fill_me), XtRString, (XtPointer) "Fill me with colors"},
    {XtNcurrentSpace, XtCCurrentSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(current_space), XtRImmediate, (XtPointer) CSBSpaceHSB},
    {XtNcurrentRendering, XtCCurrentRendering, XtRRenderingType,
	sizeof(CSBRenderingType), Offset(current_rendering),
	XtRImmediate, (XtPointer) CSBDisplayDPS},
    {XtNcurrentPalette, XtCCurrentPalette, XtRShort, sizeof(short),
	Offset(current_palette), XtRImmediate, (XtPointer) 0},
    {XtNbrokenPaletteLabel, XtCBrokenPaletteLabel, XtRString,
	sizeof(String), Offset(broken_palette_label),
	XtRString, (XtPointer) "(broken)"},
    {XtNbrokenPaletteMessage, XtCBrokenPaletteMessage, XtRString,
	sizeof(String), Offset(broken_palette_message),
	XtRString, (XtPointer) "The current palette contains an error"},

    {XtNpalette0Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[0]), XtRString, (XtPointer) NULL},
    {XtNpalette0Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[0]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette0ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[0]), XtRImmediate, (XtPointer) False},
    {XtNpalette0Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[0]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette1Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[1]), XtRString, (XtPointer) NULL},
    {XtNpalette1Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[1]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette1ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[1]), XtRImmediate, (XtPointer) False},
    {XtNpalette1Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[1]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette2Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[2]), XtRString, (XtPointer) NULL},
    {XtNpalette2Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[2]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette2ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[2]), XtRImmediate, (XtPointer) False},
    {XtNpalette2Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[2]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette3Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[3]), XtRString, (XtPointer) NULL},
    {XtNpalette3Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[3]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette3ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[3]), XtRImmediate, (XtPointer) False},
    {XtNpalette3Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[3]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette4Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[4]), XtRString, (XtPointer) NULL},
    {XtNpalette4Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[4]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette4ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[4]), XtRImmediate, (XtPointer) False},
    {XtNpalette4Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[4]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette5Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[5]), XtRString, (XtPointer) NULL},
    {XtNpalette5Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[5]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette5ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[5]), XtRImmediate, (XtPointer) False},
    {XtNpalette5Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[5]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette6Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[6]), XtRString, (XtPointer) NULL},
    {XtNpalette6Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[6]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette6ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[6]), XtRImmediate, (XtPointer) False},
    {XtNpalette6Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[6]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette7Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[7]), XtRString, (XtPointer) NULL},
    {XtNpalette7Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[7]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette7ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[7]), XtRImmediate, (XtPointer) False},
    {XtNpalette7Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[7]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette8Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[8]), XtRString, (XtPointer) NULL},
    {XtNpalette8Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[8]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette8ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[8]), XtRImmediate, (XtPointer) False},
    {XtNpalette8Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[8]), XtRImmediate, (XtPointer) NULL},

    {XtNpalette9Label, XtCPaletteLabel, XtRString, sizeof(String),
	Offset(palette_label[9]), XtRString, (XtPointer) NULL},
    {XtNpalette9Space, XtCPaletteSpace, XtRColorSpace, sizeof(CSBColorSpace),
	Offset(palette_space[9]), XtRImmediate, (XtPointer) CSBSpaceRGB},
    {XtNpalette9ColorDependent, XtCPaletteColorDependent,
	XtRBoolean, sizeof(Boolean),
	Offset(palette_color_dependent[9]), XtRImmediate, (XtPointer) False},
    {XtNpalette9Function, XtCPaletteFunction, XtRString, sizeof(String),
	Offset(palette_function[9]), XtRImmediate, (XtPointer) NULL},

    {XtNokCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(ok_callback), XtRCallback, (XtPointer) NULL},
    {XtNapplyCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(apply_callback), XtRCallback, (XtPointer) NULL},
    {XtNresetCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(reset_callback), XtRCallback, (XtPointer) NULL},
    {XtNcancelCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(cancel_callback), XtRCallback, (XtPointer) NULL},
    {XtNvalueChangedCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(value_changed_callback), XtRCallback, (XtPointer) NULL}
};

static Boolean SetColor         (Widget w, CSBColorSpace space, double c1, double c2, double c3, double c4, Bool setSpace);
static Boolean SetValues        (Widget old, Widget req, Widget new, ArgList args, Cardinal *num_args);
static XtGeometryResult GeometryManager (Widget w, XtWidgetGeometry *desired, XtWidgetGeometry *allowed);
static void ChangeLabel         (Widget label, double n);
static void ChangeManaged       (Widget w);
static void ClassInitialize     (void);
static void ClassPartInitialize (WidgetClass widget_class);
static void CreateChildren      (ColorSelectionBoxWidget csb);
static void Destroy             (Widget widget);
static void DrawDock            (ColorSelectionBoxWidget csb);
static void DrawPalette         (ColorSelectionBoxWidget csb);
static void FillPatch           (ColorSelectionBoxWidget csb);
static void GetColor            (Widget w, CSBColorSpace space, float *c1, float *c2, float *c3, float *c4);
static void Initialize          (Widget request, Widget new, ArgList args, Cardinal *num_args);
static void InitializeDock      (ColorSelectionBoxWidget csb);
static void Realize             (Widget w, XtValueMask *mask, XSetWindowAttributes *attr);
static void Resize              (Widget widget);
static void SaveDockContents    (ColorSelectionBoxWidget csb);
static void SetBackground       (ColorSelectionBoxWidget csb);
static void SetCMYKValues       (ColorSelectionBoxWidget csb);
static void SetColorSpace       (ColorSelectionBoxWidget csb);
static void SetGrayValues       (ColorSelectionBoxWidget csb);
static void SetHSBValues        (ColorSelectionBoxWidget csb);
static void SetRGBValues        (ColorSelectionBoxWidget csb);
static void SetRendering        (ColorSelectionBoxWidget csb);
static void SetSliders          (ColorSelectionBoxWidget csb);
static void UpdateColorSpaces   (ColorSelectionBoxWidget csb, CSBColorSpace masterSpace);

static void DockPress           (Widget w, XtPointer data, XEvent *event, Boolean *goOn);
static void EyedropPointer      (Widget w, XtPointer data, XEvent *event, Boolean *goOn);
static void FormResize          (Widget w, XtPointer data, XEvent *event, Boolean *goOn);
static void PalettePress        (Widget w, XtPointer data, XEvent *event, Boolean *goOn);
static void PatchPress          (Widget w, XtPointer data, XEvent *event, Boolean *goOn);
static void PatchRelease        (Widget w, XtPointer data, XEvent *event, Boolean *goOn);

static void ApplyCallback       (Widget w, XtPointer clientData, XtPointer callData);
static void DoEyedropCallback   (Widget w, XtPointer clientData, XtPointer callData);
static void DrawDockCallback    (Widget w, XtPointer clientData, XtPointer callData);
static void DrawPaletteCallback (Widget w, XtPointer clientData, XtPointer callData);
static void FillPatchCallback   (Widget w, XtPointer clientData, XtPointer callData);
static void OKCallback          (Widget w, XtPointer clientData, XtPointer callData);
static void SetCMYKCallback     (Widget w, XtPointer clientData, XtPointer callData);
static void SetGrayCallback     (Widget w, XtPointer clientData, XtPointer callData);
static void SetHSBCallback      (Widget w, XtPointer clientData, XtPointer callData);
static void SetRGBCallback      (Widget w, XtPointer clientData, XtPointer callData);
static void Slider1Callback     (Widget w, XtPointer clientData, XtPointer callData);
static void Slider2Callback     (Widget w, XtPointer clientData, XtPointer callData);
static void Slider3Callback     (Widget w, XtPointer clientData, XtPointer callData);
static void Slider4Callback     (Widget w, XtPointer clientData, XtPointer callData);

ColorSelectionBoxClassRec colorSelectionBoxClassRec = {
    /* Core class part */
  {
    /* superclass	     */	(WidgetClass) &xmManagerClassRec,
    /* class_name	     */ "ColorSelectionBox",
    /* widget_size	     */ sizeof(ColorSelectionBoxRec),
    /* class_initialize      */ ClassInitialize,
    /* class_part_initialize */ ClassPartInitialize,
    /* class_inited          */	False,
    /* initialize	     */	Initialize,
    /* initialize_hook       */	NULL,
    /* realize		     */	Realize,
    /* actions		     */	NULL,
    /* num_actions	     */	0,
    /* resources	     */	resources,
    /* num_resources	     */	XtNumber(resources),
    /* xrm_class	     */	NULLQUARK,
    /* compress_motion	     */	True,
    /* compress_exposure     */	XtExposeCompressMultiple,
    /* compress_enterleave   */	True,
    /* visible_interest	     */	False,
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
   /* ColorSelectionBox class part */
  {
    /* set_color	     */ SetColor,
    /* get_color	     */ GetColor,
    /* extension	     */	NULL,
  }
};

WidgetClass colorSelectionBoxWidgetClass =
	(WidgetClass) &colorSelectionBoxClassRec;

static XmString CreateSharedCS(String str, Widget w)
{
    XrmValue src, dst;
    XmString result;

    src.addr = str;
    src.size = strlen(str);

    dst.addr = (caddr_t) &result;
    dst.size = sizeof(result);

    if (XtConvertAndStore(w, XtRString, &src, XmRXmString, &dst)) {
	return result;
    } else return NULL;
}
 
static Boolean LowerCase(String from, String to, int size)
{
    register char ch;
    register int i;

    for (i = 0; i < size; i++) {
	ch = from[i];
	if (ch >= 'A' && ch <= 'Z') to[i] = ch - 'A' + 'a';
	else to[i] = ch;
	if (ch == '\0') return False;
    }
    return TRUE;
}

/* ARGSUSED */

static Boolean CvtStringToColorSpace(
    Display *dpy,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *data)
{
#define LOWER_SIZE 5
    char lower[LOWER_SIZE];	/* Lower cased string value */
    Boolean badConvert;
    static CSBColorSpace c;

    if (*num_args != 0) {	/* Check for correct number */
	XtAppErrorMsg(XtDisplayToApplicationContext(dpy),
		"cvtStringToColorSpace", "wrongParameters",
		"XtToolkitError",
		"String to colorspace conversion needs no extra arguments",
		(String *) NULL, (Cardinal *) NULL);
    }

    /* Lower case the value */
    badConvert = LowerCase(from->addr, lower, LOWER_SIZE);

    /* Try to convert if a short enough string specified */
    if (!badConvert) {
	if (strcmp(lower, "rgb") == 0) c = CSBSpaceRGB;
	else if (strcmp(lower, "cmyk") == 0) c = CSBSpaceCMYK;
	else if (strcmp(lower, "hsb") == 0) c = CSBSpaceHSB;
	else if (strcmp(lower, "gray") == 0) c = CSBSpaceGray;
	else if (strcmp(lower, "grey") == 0) c = CSBSpaceGray;
	else badConvert = True;
    }

    /* String too long or unknown value -- issue warning */
    if (badConvert) {
	XtDisplayStringConversionWarning(dpy, from->addr, "ColorSpace");
    } else {
	if (to->addr == NULL) to->addr = (caddr_t) &c;

	else if (to->size < sizeof(CSBColorSpace)) badConvert = TRUE;
	else *(CSBColorSpace *) to->addr = c;

	to->size = sizeof(CSBColorSpace);
    }
    return !badConvert;
#undef LOWER_SIZE
}

/* ARGSUSED */

static Boolean CvtStringToRenderingType(
    Display *dpy,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *data)
{
#define LOWER_SIZE 5
    char lower[LOWER_SIZE];	/* Lower cased string value */
    Boolean badConvert;
    static CSBRenderingType c;

    if (*num_args != 0) {	/* Check for correct number */
	XtAppErrorMsg(XtDisplayToApplicationContext(dpy),
		"cvtStringToRenderingType", "wrongParameters",
		"XtToolkitError",
		"String to rendering type conversion needs no extra arguments",
		(String *) NULL, (Cardinal *) NULL);
    }

    /* Lower case the value */
    badConvert = LowerCase(from->addr, lower, LOWER_SIZE);

    /* Try to convert if a short enough string specified */
    if (!badConvert) {
	if (strcmp(lower, "x") == 0) c = CSBDisplayX;
	else if (strcmp(lower, "dps") == 0) c = CSBDisplayDPS;
	else if (strcmp(lower, "both") == 0) c = CSBDisplayBoth;
	else badConvert = True;
    }

    /* String too long or unknown value -- issue warning */
    if (badConvert) {
	XtDisplayStringConversionWarning(dpy, from->addr, "RenderingType");
    } else {
	if (to->addr == NULL) to->addr = (caddr_t) &c;

	else if (to->size < sizeof(CSBRenderingType)) badConvert = TRUE;
	else *(CSBRenderingType *) to->addr = c;

	to->size = sizeof(CSBRenderingType);
    }
    return !badConvert;
#undef LOWER_SIZE
}

static void ClassInitialize(void)
{
    /* Register converters */

    XtSetTypeConverter(XtRString, XtRColorSpace,
	    CvtStringToColorSpace, (XtConvertArgList) NULL, 0,
	    XtCacheAll, (XtDestructor) NULL);
    XtSetTypeConverter(XtRString, XtRRenderingType,
	    CvtStringToRenderingType, (XtConvertArgList) NULL, 0,
	    XtCacheAll, (XtDestructor) NULL);
}

/* ARGSUSED */

static void ClassPartInitialize(WidgetClass widget_class)
{
    register ColorSelectionBoxWidgetClass wc =
	    (ColorSelectionBoxWidgetClass) widget_class;
    ColorSelectionBoxWidgetClass super =
	    (ColorSelectionBoxWidgetClass) wc->core_class.superclass;

    if (wc->csb_class.set_color == InheritSetColor) {
	wc->csb_class.set_color = super->csb_class.set_color;
    }
    if (wc->csb_class.get_color == InheritGetColor) {
	wc->csb_class.get_color = super->csb_class.get_color;
    }
}

static void ToUserSpace(
    ColorSelectionBoxWidget csb,
    int xWidth, int xHeight,
    float *uWidth, float *uHeight)
{
    register float *i = csb->csb.itransform;

    *uWidth = i[0] * xWidth - i[2] * xHeight + i[4];
    *uHeight= i[1] * xWidth - i[3] * xHeight + i[5];
}

static void ColorizeRGB(ColorSelectionBoxWidget csb)
{
    Dimension height, width;
    int depth, steps;
    float w, h;

    XtVaGetValues(csb->csb.slider_child[0], XtNwidth, &width,
		  XtNheight, &height,
		  XtNdepth, &depth, NULL);

    if (csb->csb.red_pixmap != None && width != csb->csb.rgb_slider_width) {
	XFreePixmap(XtDisplay(csb), csb->csb.red_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.green_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.blue_pixmap);
	csb->csb.red_pixmap = None;
    }

    if (csb->csb.red_pixmap == None) {
	csb->csb.rgb_slider_width = width;
	if (csb->csb.visual_class == TrueColor) steps = width / 2;
	else steps = width / 4;
	
	ToUserSpace(csb, width, height, &w, &h);

	csb->csb.red_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					    width, height, depth);
    
	XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	XDPSSetContextDrawable(csb->csb.context, csb->csb.red_pixmap, height);

	_DPSCRGBBlend(csb->csb.context, 0.0, 0.0, w, h, "0 0", steps);

	csb->csb.green_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					      width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context,
			       csb->csb.green_pixmap, height);

	_DPSCRGBBlend(csb->csb.context, 0.0, 0.0, w, h, "0 exch 0", steps);

	csb->csb.blue_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					     width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context, csb->csb.blue_pixmap, height);

	_DPSCRGBBlend(csb->csb.context,
		      0.0, 0.0, w, h, "0 0 3 -1 roll", steps);

	DPSWaitContext(csb->csb.context);
    }

    XtVaSetValues(csb->csb.slider_child[0],
		  XtNbackgroundPixmap, csb->csb.red_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[1],
		  XtNbackgroundPixmap, csb->csb.green_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[2],
		  XtNbackgroundPixmap, csb->csb.blue_pixmap, NULL);
}

static void ColorizeCMYK(ColorSelectionBoxWidget csb)
{
    Dimension height, width;
    int depth, steps;
    float w, h;

    XtVaGetValues(csb->csb.slider_child[0], XtNwidth, &width,
		  XtNheight, &height,
		  XtNdepth, &depth, NULL);

    if (csb->csb.cyan_pixmap != None && width != csb->csb.cmyk_slider_width) {
	XFreePixmap(XtDisplay(csb), csb->csb.cyan_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.magenta_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.yellow_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.black_pixmap);
	csb->csb.cyan_pixmap = None;
    }

    if (csb->csb.cyan_pixmap == None) {
	csb->csb.cmyk_slider_width = width;
	if (csb->csb.visual_class == TrueColor) steps = width / 2;
	else steps = width / 4;
	
	ToUserSpace(csb, width, height, &w, &h);

	csb->csb.cyan_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					     width, height, depth);
    
	XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	XDPSSetContextDrawable(csb->csb.context, csb->csb.cyan_pixmap, height);

	_DPSCCMYKBlend(csb->csb.context, 0.0, 0.0, w, h, "0 0 0", steps);

	csb->csb.magenta_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
						width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context, csb->csb.magenta_pixmap,
			       height);

	_DPSCCMYKBlend(csb->csb.context, 0.0, 0.0, w, h, "0 exch 0 0", steps);

	csb->csb.yellow_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					       width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context, csb->csb.yellow_pixmap,
			       height);

	_DPSCCMYKBlend(csb->csb.context, 0.0, 0.0, w, h, "0 0 3 -1 roll 0",
		       steps);

	csb->csb.black_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					      width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context, csb->csb.black_pixmap,
			       height);

	_DPSCCMYKBlend(csb->csb.context, 0.0, 0.0, w, h, "0 0 0 4 -1 roll",
		       steps);

	DPSWaitContext(csb->csb.context);
    }

    XtVaSetValues(csb->csb.slider_child[0], XtNbackgroundPixmap,
		  csb->csb.cyan_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[1], XtNbackgroundPixmap,
		  csb->csb.magenta_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[2], XtNbackgroundPixmap,
		  csb->csb.yellow_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[3], XtNbackgroundPixmap,
		  csb->csb.black_pixmap, NULL);
}

static void ColorizeHSB(ColorSelectionBoxWidget csb)
{
    Dimension height, width;
    int depth, steps;
    float w, h;

    XtVaGetValues(csb->csb.slider_child[0], XtNwidth, &width,
		  XtNheight, &height,
		  XtNdepth, &depth, NULL);

    if (csb->csb.hue_pixmap != None && width != csb->csb.hsb_slider_width) {
	XFreePixmap(XtDisplay(csb), csb->csb.hue_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.sat_pixmap);
	XFreePixmap(XtDisplay(csb), csb->csb.bright_pixmap);
	csb->csb.hue_pixmap = None;
    }

    if (csb->csb.hue_pixmap == None) {
	csb->csb.hsb_slider_width = width;
	if (csb->csb.visual_class == TrueColor) steps = width / 2;
	else steps = width / 4;
	
	ToUserSpace(csb, width, height, &w, &h);

	csb->csb.hue_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					    width, height, depth);
    
	XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	XDPSSetContextDrawable(csb->csb.context, csb->csb.hue_pixmap, height);

	_DPSCHSBBlend(csb->csb.context, 0.0, 0.0, w, h, "1 1", steps);

	csb->csb.sat_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					    width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context, csb->csb.sat_pixmap, height);

	_DPSCHSBBlend(csb->csb.context, 0.0, 0.0, w, h, "0 exch 1", steps);

	csb->csb.bright_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					       width, height, depth);
    
	XDPSSetContextDrawable(csb->csb.context, csb->csb.bright_pixmap,
			       height);

	_DPSCHSBBlend(csb->csb.context, 0.0, 0.0, w, h, "0 1 3 -1 roll",
		      steps);

	DPSWaitContext(csb->csb.context);
    }

    XtVaSetValues(csb->csb.slider_child[0], XtNbackgroundPixmap,
		  csb->csb.hue_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[1], XtNbackgroundPixmap,
		  csb->csb.sat_pixmap, NULL);
    XtVaSetValues(csb->csb.slider_child[2], XtNbackgroundPixmap,
		  csb->csb.bright_pixmap, NULL);
}

static void ColorizeGray(ColorSelectionBoxWidget csb)
{
    Dimension height, width;
    int depth, steps;
    float w, h;

    XtVaGetValues(csb->csb.slider_child[0], XtNwidth, &width,
		  XtNheight, &height,
		  XtNdepth, &depth, NULL);

    if (csb->csb.gray_pixmap != None && width != csb->csb.gray_slider_width) {
	XFreePixmap(XtDisplay(csb), csb->csb.gray_pixmap);
	csb->csb.gray_pixmap = None;
    }

    if (csb->csb.gray_pixmap == None) {
	csb->csb.gray_slider_width = width;
	if (csb->csb.visual_class == TrueColor) steps = width / 2;
	else steps = width / 4;
	
	ToUserSpace(csb, width, height, &w, &h);

	csb->csb.gray_pixmap = XCreatePixmap(XtDisplay(csb), XtWindow(csb),
					     width, height, depth);
    
	XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	XDPSSetContextDrawable(csb->csb.context, csb->csb.gray_pixmap, height);

	_DPSCGrayBlend(csb->csb.context, 0.0, 0.0, w, h, " ", steps);

	DPSWaitContext(csb->csb.context);
    }

    XtVaSetValues(csb->csb.slider_child[0], XtNbackgroundPixmap,
		  csb->csb.gray_pixmap, NULL);
}

static void ColorizeSliders(ColorSelectionBoxWidget csb)
{
    if (!XtIsRealized(csb)) return;

    switch (csb->csb.current_space) {
	case CSBSpaceRGB:
	    ColorizeRGB(csb);
	    break;
	case CSBSpaceCMYK:
	    ColorizeCMYK(csb);
	    break;
	case CSBSpaceHSB:
	    ColorizeHSB(csb);
	    break;
	case CSBSpaceGray:
	    ColorizeGray(csb);
	    break;
    }
}

/* ARGSUSED */

static void FormResize(
    Widget w,
    XtPointer data,
    XEvent *event,
    Boolean *goOn)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) data;
    
    if (event->type != ConfigureNotify && event->type != MapNotify) return;

    csb->csb.rgb_slider_width = csb->csb.cmyk_slider_width =
	    csb->csb.hsb_slider_width = csb->csb.gray_slider_width = 0;
    csb->csb.palette_pixmap_valid = False;
    if (csb->csb.patch_gstate != 0) {
	XDPSFreeContextGState(csb->csb.context, csb->csb.patch_gstate);
	csb->csb.patch_gstate = 0;
    }
    if (csb->csb.dock_gstate != 0) {
	XDPSFreeContextGState(csb->csb.context, csb->csb.dock_gstate);
	csb->csb.dock_gstate = 0;
    }
    ColorizeSliders(csb);
    DrawPalette(csb);
    if (XtIsRealized(csb->csb.patch_child)) {
	XClearArea(XtDisplay(csb), XtWindow(csb->csb.patch_child),
		   0, 0, 1000, 1000, True);
    }
}

static void FillCallbackRec(
    ColorSelectionBoxWidget csb,
    CSBCallbackRec *rec)
{
    rec->current_space = csb->csb.current_space;
    rec->red = csb->csb.current_color.red;
    rec->green = csb->csb.current_color.green;
    rec->blue = csb->csb.current_color.blue;
    rec->cyan = csb->csb.current_color.cyan;
    rec->magenta = csb->csb.current_color.magenta;
    rec->yellow = csb->csb.current_color.yellow;
    rec->black = csb->csb.current_color.black;
    rec->hue = csb->csb.current_color.hue;
    rec->saturation = csb->csb.current_color.saturation;
    rec->brightness = csb->csb.current_color.brightness;
    rec->gray = csb->csb.current_color.gray;
}

/* ARGSUSED */

static void OKCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    CSBCallbackRec rec;

    csb->csb.save_color = csb->csb.current_color;
    FillCallbackRec(csb, &rec);
    rec.reason = CSBOK;
    XtCallCallbackList((Widget) csb, csb->csb.ok_callback, (XtPointer) &rec);
    if (XtIsShell(XtParent(csb))) XtPopdown(XtParent(csb));

    SaveDockContents(csb);
}

/* ARGSUSED */

static void ApplyCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    CSBCallbackRec rec;

    csb->csb.save_color = csb->csb.current_color;
    FillCallbackRec(csb, &rec);
    rec.reason = CSBApply;
    XtCallCallbackList((Widget) csb, csb->csb.apply_callback,
		       (XtPointer) &rec);

    SaveDockContents(csb);
}

/* ARGSUSED */

static void ResetCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    CSBCallbackRec rec;

    csb->csb.current_color = csb->csb.save_color;
    FillPatch(csb);
    SetSliders(csb);
    FillCallbackRec(csb, &rec);
    rec.reason = CSBReset;
    XtCallCallbackList((Widget) csb, csb->csb.reset_callback,
		       (XtPointer) &rec);
}

/* ARGSUSED */

static void CancelCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    CSBCallbackRec rec;

    csb->csb.current_color = csb->csb.save_color;
    FillPatch(csb);
    SetSliders(csb);
    FillCallbackRec(csb, &rec);
    rec.reason = CSBCancel;
    XtCallCallbackList((Widget) csb, csb->csb.cancel_callback,
		       (XtPointer) &rec);
    if (XtIsShell(XtParent(csb))) XtPopdown(XtParent(csb));
}

/* ARGSUSED */

static void DoValueChangedCallback(ColorSelectionBoxWidget csb)
{
    CSBCallbackRec rec;

    FillCallbackRec(csb, &rec);
    rec.reason = CSBValueChanged;
    XtCallCallbackList((Widget) csb, csb->csb.value_changed_callback,
		       (XtPointer) &rec);
}

/* ARGSUSED */

static void ChangeLabelCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    XmScaleCallbackStruct *scaleData = (XmScaleCallbackStruct *) callData;

    ChangeLabel((Widget) clientData, ((float) scaleData->value) / 100.0);
}

static void ChangeLabel(Widget label, double n)
{
    char buf[10];

    sprintf(buf, "%d", TO_PCT(n));
    XtVaSetValues(label, XmNlabelString, CS(buf, label), NULL);
}

static void CreateModelMenu(Widget parent, Widget csb)
{
    Widget kids[4];

    kids[0] = XmCreatePushButtonGadget(parent, "rgb", (ArgList) NULL, 0);
    XtAddCallback(kids[0], XmNactivateCallback,
		  SetRGBCallback, (XtPointer) csb);
    kids[1] = XmCreatePushButtonGadget(parent, "cmyk", (ArgList) NULL, 0);
    XtAddCallback(kids[1], XmNactivateCallback,
		  SetCMYKCallback, (XtPointer) csb);
    kids[2] = XmCreatePushButtonGadget(parent, "hsb", (ArgList) NULL, 0);
    XtAddCallback(kids[2], XmNactivateCallback,
		  SetHSBCallback, (XtPointer) csb);
    kids[3] = XmCreatePushButtonGadget(parent, "gray", (ArgList) NULL, 0);
    XtAddCallback(kids[3], XmNactivateCallback,
		  SetGrayCallback, (XtPointer) csb);

    XtManageChildren(kids, 4);
}

typedef struct {
    ColorSelectionBoxWidget csb;
    CSBRenderingType rendering;
} RenderingRec;

/* ARGSUSED */

static void SetRenderingCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    RenderingRec *r = (RenderingRec *) clientData;

    r->csb->csb.current_rendering = r->rendering;
    FillPatch(r->csb);
}

static void CreateDisplayMenu(Widget parent, ColorSelectionBoxWidget csb)
{
    Widget kids[3];
    RenderingRec *r;

    r = XtNew(RenderingRec);
    r->csb = csb;
    r->rendering = CSBDisplayDPS;
    kids[0] = XmCreatePushButtonGadget(parent, "displayDPS",
				       (ArgList) NULL, 0);
    XtAddCallback(kids[0], XmNactivateCallback,
		  SetRenderingCallback, (XtPointer) r);
    r = XtNew(RenderingRec);
    r->csb = csb;
    r->rendering = CSBDisplayX;
    kids[1] = XmCreatePushButtonGadget(parent, "displayX", (ArgList) NULL, 0);
    XtAddCallback(kids[1], XmNactivateCallback,
		  SetRenderingCallback, (XtPointer) r);
    r = XtNew(RenderingRec);
    r->csb = csb;
    r->rendering = CSBDisplayBoth;
    kids[2] = XmCreatePushButtonGadget(parent, "displayBoth",
				       (ArgList) NULL, 0);
    XtAddCallback(kids[2], XmNactivateCallback,
		  SetRenderingCallback, (XtPointer) r);

    XtManageChildren(kids, 3);
}

typedef struct {
    ColorSelectionBoxWidget csb;
    int n;
} PaletteRec;

/* ARGSUSED */

static void SetPaletteCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    PaletteRec *p = (PaletteRec *) clientData;

    if (p->csb->csb.palette_broken[p->n]) return;

    if (p->n != p->csb->csb.current_palette ||
	p->csb->csb.palette_color_dependent[p->n]) {
	p->csb->csb.palette_pixmap_valid = False;
    }

    p->csb->csb.current_palette = p->n;
    DrawPalette(p->csb);
}

static void CreatePaletteMenu(Widget parent, ColorSelectionBoxWidget csb)
{
    Widget w, managed[PALETTE_MAX];
    int j, k;
    char buf[10];
    PaletteRec *p;

    j = 0;

    for (k = 0; k < PALETTE_MAX; k++) {
	p = XtNew(PaletteRec);
	p->csb = csb;
	p->n = k;
	sprintf(buf, "palette%d", k);
	w = XtVaCreateWidget(buf, xmPushButtonGadgetClass, parent, NULL);
	if (csb->csb.palette_label[k] != NULL) {
	    XtVaSetValues(w, XtVaTypedArg, XmNlabelString,
			     XtRString, csb->csb.palette_label[k],
			     strlen(csb->csb.palette_label[k])+1,
			     NULL);
	}
	XtAddCallback(w, XmNactivateCallback,
		      SetPaletteCallback, (XtPointer) p);
	if (csb->csb.palette_function[k] != NULL) managed[j++] = w;
    }

    if (j != 0) XtManageChildren(managed, j);
}

static void CreateChildren(ColorSelectionBoxWidget csb)
{
    int i;
    Arg args[20];
    Widget form, menu, button, w, dock_frame, palette_frame;
    Pixel fg, bg;
    int depth;
    Pixmap eyedrop;

    i = 0;
    XtSetArg(args[i], XmNresizePolicy, XmRESIZE_NONE);			i++;
    form = XtCreateManagedWidget("panel", xmFormWidgetClass,
				 (Widget) csb, args, i);
    csb->csb.form_child = form;
    XtAddEventHandler(form, StructureNotifyMask, False, FormResize,
		      (XtPointer) csb);

    i = 0;
    menu = XmCreatePulldownMenu(form, "modelMenu", args, i);
    CreateModelMenu(menu, (Widget) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);			i++;
    XtSetArg(args[i], XmNsubMenuId, menu);				i++;
    csb->csb.model_option_menu_child =
	    XmCreateOptionMenu(form, "modelOptionMenu",
			       args, i);
    XtManageChild(csb->csb.model_option_menu_child);

    XtVaGetValues(form, XtNbackground, &bg, XmNforeground, &fg,
		  XtNdepth, &depth, NULL);
    eyedrop = XCreatePixmapFromBitmapData(XtDisplay(csb),
					  RootWindowOfScreen(XtScreen(csb)),
					  (char *) heyedrop_bits,
					  heyedrop_width, heyedrop_height,
					  fg, bg, depth);
				    
    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, csb->csb.model_option_menu_child);	i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.model_option_menu_child);	i++;
    XtSetArg(args[i], XmNlabelPixmap, eyedrop);				i++;
    button = XtCreateManagedWidget("eyedropButton", xmPushButtonWidgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback,
		  DoEyedropCallback, (XtPointer) csb);
    XtInsertRawEventHandler(button, PointerMotionMask | ButtonReleaseMask,
			    False, EyedropPointer, (XtPointer) csb,
			    XtListHead);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.model_option_menu_child);	i++;
    csb->csb.label_child[0] =
	    XtCreateManagedWidget("label1", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.model_option_menu_child);	i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    csb->csb.value_child[0] =
	    XtCreateManagedWidget("value1", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, csb->csb.label_child[0]);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.model_option_menu_child);	i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.value_child[0]);		i++;
    csb->csb.slider_child[0] =
	    XtCreateManagedWidget("slider1", xmScaleWidgetClass,
				  form, args, i);
    XtAddCallback(csb->csb.slider_child[0], XmNvalueChangedCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[0]);
    XtAddCallback(csb->csb.slider_child[0], XmNdragCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[0]);
    XtAddCallback(csb->csb.slider_child[0], XmNvalueChangedCallback,
		  Slider1Callback, (XtPointer) csb);
    XtAddCallback(csb->csb.slider_child[0], XmNdragCallback,
		  Slider1Callback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[0]);		i++;
    csb->csb.label_child[1] =
	    XtCreateManagedWidget("label2", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[0]);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.value_child[0]);		i++;
    csb->csb.value_child[1] =
	    XtCreateManagedWidget("value2", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, csb->csb.slider_child[0]);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[0]);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.slider_child[0]);	i++;
    csb->csb.slider_child[1] =
	    XtCreateManagedWidget("slider2", xmScaleWidgetClass,
				  form, args, i);
    XtAddCallback(csb->csb.slider_child[1], XmNvalueChangedCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[1]);
    XtAddCallback(csb->csb.slider_child[1], XmNdragCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[1]);
    XtAddCallback(csb->csb.slider_child[1], XmNvalueChangedCallback,
		  Slider2Callback, (XtPointer) csb);
    XtAddCallback(csb->csb.slider_child[1], XmNdragCallback,
		  Slider2Callback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[1]);		i++;
    csb->csb.label_child[2] =
	    XtCreateManagedWidget("label3", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[1]);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.value_child[0]);		i++;
    csb->csb.value_child[2] =
	    XtCreateManagedWidget("value3", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, csb->csb.slider_child[0]);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[1]);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.slider_child[0]);	i++;
    csb->csb.slider_child[2] =
	    XtCreateManagedWidget("slider3", xmScaleWidgetClass,
				  form, args, i);
    XtAddCallback(csb->csb.slider_child[2], XmNvalueChangedCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[2]);
    XtAddCallback(csb->csb.slider_child[2], XmNdragCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[2]);
    XtAddCallback(csb->csb.slider_child[2], XmNvalueChangedCallback,
		  Slider3Callback, (XtPointer) csb);
    XtAddCallback(csb->csb.slider_child[2], XmNdragCallback,
		  Slider3Callback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[2]);		i++;
    csb->csb.label_child[3] =
	    XtCreateManagedWidget("label4", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[2]);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.value_child[0]);		i++;
    csb->csb.value_child[3] =
	    XtCreateManagedWidget("value4", xmLabelWidgetClass, form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, csb->csb.slider_child[0]);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.slider_child[2]);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, csb->csb.slider_child[0]);	i++;
    csb->csb.slider_child[3] =
	    XtCreateManagedWidget("slider4", xmScaleWidgetClass,
				  form, args, i);
    XtAddCallback(csb->csb.slider_child[3], XmNvalueChangedCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[3]);
    XtAddCallback(csb->csb.slider_child[3], XmNdragCallback,
		  ChangeLabelCallback, (XtPointer) csb->csb.value_child[3]);
    XtAddCallback(csb->csb.slider_child[3], XmNvalueChangedCallback,
		  Slider4Callback, (XtPointer) csb);
    XtAddCallback(csb->csb.slider_child[3], XmNdragCallback,
		  Slider4Callback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("okButton", xmPushButtonWidgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback, OKCallback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNdefaultButton, button);			i++;
    XtSetValues(form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, button);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("applyButton", xmPushButtonWidgetClass,
				   form, args, i);

    XtAddCallback(button, XmNactivateCallback, ApplyCallback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, button);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("resetButton", xmPushButtonWidgetClass,
				   form, args, i);
    XtAddCallback(button, XmNactivateCallback, ResetCallback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, button);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    button = XtCreateManagedWidget("cancelButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(button, XmNactivateCallback,
		  CancelCallback, (XtPointer) csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, button);				i++;
    w = XtCreateManagedWidget("separator", xmSeparatorGadgetClass,
			      form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, w);				i++;
    palette_frame = XtCreateManagedWidget("paletteFrame", xmFrameWidgetClass,
					  form, args, i);

    i = 0;
    csb->csb.palette_child =
	    XtCreateManagedWidget("palette", xmDrawingAreaWidgetClass,
				  palette_frame, args, i);
    XtAddCallback(csb->csb.palette_child, XmNexposeCallback,
		  DrawPaletteCallback, (XtPointer) csb);
    XtAddEventHandler(csb->csb.palette_child, ButtonPressMask, False,
		      PalettePress, (XtPointer) csb);

    i = 0;
    menu = XmCreatePulldownMenu(form, "paletteMenu", args, i);
    CreatePaletteMenu(menu, csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, palette_frame);			i++;
    XtSetArg(args[i], XmNsubMenuId, menu);				i++;
    csb->csb.palette_option_menu_child =
	    XmCreateOptionMenu(form, "paletteOptionMenu",
			       args, i);
    XtManageChild(csb->csb.palette_option_menu_child);

    i = 0;
    menu = XmCreatePulldownMenu(form, "displayMenu", args, i);
    CreateDisplayMenu(menu, csb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);			i++;
    XtSetArg(args[i], XmNsubMenuId, menu);				i++;
    csb->csb.display_option_menu_child =
	    XmCreateOptionMenu(form, "displayOptionMenu",
			       args, i);
    XtManageChild(csb->csb.display_option_menu_child);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, csb->csb.display_option_menu_child);i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, palette_frame);			i++;
    dock_frame = XtCreateManagedWidget("dockFrame", xmFrameWidgetClass,
				       form, args, i);

    i = 0;
    csb->csb.dock_child =
	    XtCreateManagedWidget("dock", xmDrawingAreaWidgetClass,
				  dock_frame, args, i);
    XtAddCallback(csb->csb.dock_child, XmNexposeCallback,
		  DrawDockCallback, (XtPointer) csb);
    XtAddEventHandler(csb->csb.dock_child, ButtonPressMask, False, DockPress,
		      (XtPointer) csb);

    {
	Dimension height;
	int q;

	XtVaGetValues(csb->csb.dock_child, XtNheight, &height, NULL);
	if (height < csb->csb.cell_size) height = csb->csb.cell_size;
	else if (height % csb->csb.cell_size != 0) {
	    q = height / csb->csb.cell_size;
	    height = csb->csb.cell_size * q;
	}
	XtVaSetValues(csb->csb.dock_child, XtNheight, height, NULL);
    }

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, dock_frame);			i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, csb->csb.display_option_menu_child);i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, dock_frame);			i++;
    w = XtCreateManagedWidget("patchFrame", xmFrameWidgetClass,
			      form, args, i);

    i = 0;
    csb->csb.patch_child =
	    XtCreateManagedWidget("patch", xmDrawingAreaWidgetClass,
				  w, args, i);
    XtAddCallback(csb->csb.patch_child, XmNexposeCallback,
		  FillPatchCallback, (XtPointer) csb);
    XtAddRawEventHandler(csb->csb.patch_child, ButtonPressMask,
			 False, PatchPress, (XtPointer) csb);
    XtAddRawEventHandler(csb->csb.patch_child, ButtonReleaseMask,
			 False, PatchRelease, (XtPointer) csb);
}

static void NoBackgroundPixel(ColorSelectionBoxWidget csb)
{
    Widget w, message;

    csb->csb.no_background = True;
    w = XtNameToWidget((Widget) csb, "*displayX");
    XtSetSensitive(w, False);
    w = XtNameToWidget((Widget) csb, "*displayBoth");
    XtSetSensitive(w, False);
    w = XtNameToWidget((Widget) csb, "*displayDPS");
    XtVaSetValues(csb->csb.display_option_menu_child, XmNmenuHistory, w, NULL);

    message = XmCreateInformationDialog(csb->csb.form_child,
					"noBackgroundMessage",
					(ArgList) NULL, 0);
    w =	XmMessageBoxGetChild(message, XmDIALOG_CANCEL_BUTTON);
    XtUnmanageChild(w);
    w =	XmMessageBoxGetChild(message, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(w);
    
    XtManageChild(message);
}

/* labelString is changed by this */

static void ParseLabels(String labelString, String labels[4], int n)
{
    register char *ch;
    int i;

    ch = labelString;
    for (i = 0; i < n; i++) {
	labels[i] = ch;
	while (*ch != ':' && *ch != '\0') ch++;
	*ch++ = '\0';
    }

    for (i = n; i < 4; i++) labels[i] = NULL;
}

static void SetLabels(ColorSelectionBoxWidget csb, String *labels)
{
    Widget w = (Widget) csb;
    int i;

    for (i = 0; i < 4; i++) {
	if (labels[i] != NULL) {
	    XtVaSetValues(csb->csb.label_child[i],
			  XmNlabelString, CS(labels[i], w), NULL);
	}
    }
}

static void MapChildren(Widget *children, int n)
{
    XtManageChildren(children, n);
}

static void UnmapChildren(Widget *children, int n)
{
    XtUnmanageChildren(children, n);
}

static void SetSliders(ColorSelectionBoxWidget csb)
{
    switch(csb->csb.current_space) {
	case CSBSpaceRGB:		SetRGBValues(csb);	break;
	case CSBSpaceCMYK:		SetCMYKValues(csb);	break;
	case CSBSpaceHSB:		SetHSBValues(csb);	break;
	case CSBSpaceGray:		SetGrayValues(csb);	break;
    }
}

static void SetRGBValues(ColorSelectionBoxWidget csb)
{
    XmScaleSetValue(csb->csb.slider_child[0],
		    TO_PCT(csb->csb.current_color.red));
    XmScaleSetValue(csb->csb.slider_child[1],
		    TO_PCT(csb->csb.current_color.green));
    XmScaleSetValue(csb->csb.slider_child[2],
		    TO_PCT(csb->csb.current_color.blue));
    ChangeLabel(csb->csb.value_child[0], csb->csb.current_color.red);
    ChangeLabel(csb->csb.value_child[1], csb->csb.current_color.green);
    ChangeLabel(csb->csb.value_child[2], csb->csb.current_color.blue);
}

/* ARGSUSED */

static void SetRGBCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Widget rgb;
    Widget children[6];
    String labels[4];
    int i, j;

    csb->csb.current_space = CSBSpaceRGB;

    ParseLabels(csb->csb.rgb_labels, labels, 3);

    rgb = XtNameToWidget((Widget) csb, "*rgb");
    
    XtVaSetValues(csb->csb.model_option_menu_child, XmNmenuHistory, rgb, NULL);

    SetLabels(csb, labels);

    SetRGBValues(csb);

    j = 0;
    for (i = 1; i < 3; i++) {
	children[j++] = csb->csb.label_child[i];
	children[j++] = csb->csb.slider_child[i];
	children[j++] = csb->csb.value_child[i];
    }

    MapChildren(children, 6);

    children[0] = csb->csb.label_child[3];
    children[1] = csb->csb.slider_child[3];
    children[2] = csb->csb.value_child[3];

    UnmapChildren(children, 3);

    ColorizeSliders(csb);
    FillPatch(csb);
}

static void SetCMYKValues(ColorSelectionBoxWidget csb)
{
    XmScaleSetValue(csb->csb.slider_child[0],
		    TO_PCT(csb->csb.current_color.cyan));
    XmScaleSetValue(csb->csb.slider_child[1],
		    TO_PCT(csb->csb.current_color.magenta));
    XmScaleSetValue(csb->csb.slider_child[2],
		    TO_PCT(csb->csb.current_color.yellow));
    XmScaleSetValue(csb->csb.slider_child[3],
		    TO_PCT(csb->csb.current_color.black));
    ChangeLabel(csb->csb.value_child[0], csb->csb.current_color.cyan);
    ChangeLabel(csb->csb.value_child[1], csb->csb.current_color.magenta);
    ChangeLabel(csb->csb.value_child[2], csb->csb.current_color.yellow);
    ChangeLabel(csb->csb.value_child[3], csb->csb.current_color.black);
}

/* ARGSUSED */

static void SetCMYKCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Widget cmyk;
    Widget children[9];
    String labels[4];
    int i, j;

    csb->csb.current_space = CSBSpaceCMYK;

    ParseLabels(csb->csb.cmyk_labels, labels, 4);

    cmyk = XtNameToWidget((Widget) csb, "*cmyk");
    
    XtVaSetValues(csb->csb.model_option_menu_child,
		  XmNmenuHistory, cmyk, NULL);

    SetLabels(csb, labels);

    SetCMYKValues(csb);

    j = 0;
    for (i = 1; i < 4; i++) {
	children[j++] = csb->csb.label_child[i];
	children[j++] = csb->csb.slider_child[i];
	children[j++] = csb->csb.value_child[i];
    }

    MapChildren(children, 9);

    ColorizeSliders(csb);
    FillPatch(csb);
}

static void SetHSBValues(ColorSelectionBoxWidget csb)
{
    XmScaleSetValue(csb->csb.slider_child[0],
		    TO_PCT(csb->csb.current_color.hue));
    XmScaleSetValue(csb->csb.slider_child[1],
		    TO_PCT(csb->csb.current_color.saturation));
    XmScaleSetValue(csb->csb.slider_child[2],
		    TO_PCT(csb->csb.current_color.brightness));
    ChangeLabel(csb->csb.value_child[0], csb->csb.current_color.hue);
    ChangeLabel(csb->csb.value_child[1], csb->csb.current_color.saturation);
    ChangeLabel(csb->csb.value_child[2], csb->csb.current_color.brightness);
}

/* ARGSUSED */

static void SetHSBCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Widget hsb;
    Widget children[6];
    String labels[4];
    int i, j;

    csb->csb.current_space = CSBSpaceHSB;

    ParseLabels(csb->csb.hsb_labels, labels, 3);

    hsb = XtNameToWidget((Widget) csb, "*hsb");
    
    XtVaSetValues(csb->csb.model_option_menu_child, XmNmenuHistory, hsb, NULL);

    SetLabels(csb, labels);

    SetHSBValues(csb);

    j = 0;
    for (i = 1; i < 3; i++) {
	children[j++] = csb->csb.label_child[i];
	children[j++] = csb->csb.slider_child[i];
	children[j++] = csb->csb.value_child[i];
    }

    MapChildren(children, 6);

    children[0] = csb->csb.label_child[3];
    children[1] = csb->csb.slider_child[3];
    children[2] = csb->csb.value_child[3];

    UnmapChildren(children, 3);

    ColorizeSliders(csb);
    FillPatch(csb);
}

static void SetGrayValues(ColorSelectionBoxWidget csb)
{
    XmScaleSetValue(csb->csb.slider_child[0],
		    TO_PCT(csb->csb.current_color.gray));
    ChangeLabel(csb->csb.value_child[0], csb->csb.current_color.gray);
}

/* ARGSUSED */

static void SetGrayCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Widget gray;
    Widget children[9];
    String labels[4];
    int i, j;

    csb->csb.current_space = CSBSpaceGray;

    gray = XtNameToWidget((Widget) csb, "*gray");
    
    XtVaSetValues(csb->csb.model_option_menu_child, XmNmenuHistory, gray, NULL);

    labels[0] = csb->csb.gray_labels;
    labels[1] = labels[2] = labels[3] = NULL;
    SetLabels(csb, labels);

    SetGrayValues(csb);

    j = 0;
    for (i = 1; i < 4; i++) {
	children[j++] = csb->csb.label_child[i];
	children[j++] = csb->csb.slider_child[i];
	children[j++] = csb->csb.value_child[i];
    }

    UnmapChildren(children, 9);

    ColorizeSliders(csb);
    FillPatch(csb);
}

static void RGBToCMYK(ColorSelectionBoxWidget csb)
{
    csb->csb.current_color.cyan = 1.0 - csb->csb.current_color.red;
    csb->csb.current_color.magenta = 1.0 - csb->csb.current_color.green;
    csb->csb.current_color.yellow = 1.0 - csb->csb.current_color.blue;
    csb->csb.current_color.black = 0.0;
}

static void RGBToGray(ColorSelectionBoxWidget csb)
{
    csb->csb.current_color.gray = .3 * csb->csb.current_color.red +
	    .59 * csb->csb.current_color.green +
	    .11 * csb->csb.current_color.blue;
}

static void HSBToRGB(ColorSelectionBoxWidget csb)
{
    double r, g, bl;
    double h, s, b;
    double f, m, n, k;
    int	i;
	
    if (csb->csb.current_color.saturation == 0) {
	r = g = bl = csb->csb.current_color.brightness;
    } else {
	h = csb->csb.current_color.hue;
	s = csb->csb.current_color.saturation;
	b = csb->csb.current_color.brightness;

	h = 6.0 * h;
	if (h >= 6.0) h = 0.0;
	i = (int) h;
	f = h - (double)i;
	m = b * (1.0 - s);
	n = b * (1.0 - (s * f));
	k = b * (1.0 - (s * (1.0 - f)));

	switch(i) {
	    default:
	    case 0:     r = b;		g = k;		bl = m;		break;
	    case 1:	r = n;		g = b;		bl = m;		break;
	    case 2:	r = m;		g = b;		bl = k;		break;
	    case 3:	r = m;		g = n;		bl = b;		break;
	    case 4:	r = k;		g = m;		bl = b;		break;
	    case 5:	r = b;		g = m;		bl = n;		break;
	}
    }

    csb->csb.current_color.red = r;
    csb->csb.current_color.green = g;
    csb->csb.current_color.blue = bl;
}

static void RGBToHSB(ColorSelectionBoxWidget csb)
{
    double hue, sat, value;
    double diff, x, r, g, b;
    double red, green, blue;

    red = csb->csb.current_color.red;
    green = csb->csb.current_color.green;
    blue = csb->csb.current_color.blue;

    hue = sat = 0.0;
    value = x = red;
    if (green > value) value = green;  else x = green;
    if (blue > value) value = blue;
    if (blue < x) x = blue;

    if (value != 0.0) {
	diff = value - x;
	if (diff != 0.0) {
	    sat = diff / value;
	    r = (value - red) / diff;
	    g = (value - green) / diff;
	    b = (value - blue) / diff;
	    if      (red == value)   hue = (green == x) ? 5.0 + b : 1.0 - g;
	    else if (green == value) hue = (blue == x) ? 1.0 + r : 3.0 - b;
	    else                     hue = (red == x) ? 3.0 + g : 5.0 - r;
	    hue /= 6.0;  if (hue >= 1.0 || hue <= 0.0) hue = 0.0;
	}
    }
    csb->csb.current_color.hue = hue;
    csb->csb.current_color.saturation = sat;
    csb->csb.current_color.brightness = value;
}

static void UpdateColorSpaces(
    ColorSelectionBoxWidget csb,
    CSBColorSpace masterSpace)
{
    switch (masterSpace) {
	case CSBSpaceRGB:
	    RGBToCMYK(csb);
	    RGBToHSB(csb);
	    RGBToGray(csb);
	    break;

	case CSBSpaceCMYK:
	    csb->csb.current_color.red =
		    1.0 - MIN(1.0, csb->csb.current_color.cyan +
			           csb->csb.current_color.black);
	    csb->csb.current_color.green =
		    1.0 - MIN(1.0, csb->csb.current_color.magenta +
			           csb->csb.current_color.black);
	    csb->csb.current_color.blue =
		    1.0 - MIN(1.0, csb->csb.current_color.yellow +
			           csb->csb.current_color.black);
	    RGBToHSB(csb);
	    RGBToGray(csb);
	    break;

	case CSBSpaceHSB:
	    HSBToRGB(csb);
	    RGBToCMYK(csb);
	    RGBToGray(csb);
	    break;

	case CSBSpaceGray:
	    csb->csb.current_color.red = csb->csb.current_color.green =
		    csb->csb.current_color.blue = csb->csb.current_color.gray;

	    csb->csb.current_color.hue =
		    csb->csb.current_color.saturation = 0.0;
	    csb->csb.current_color.brightness = csb->csb.current_color.gray;

	    csb->csb.current_color.cyan = csb->csb.current_color.magenta =
		    csb->csb.current_color.yellow = 0.0;
	    csb->csb.current_color.black = 1.0 - csb->csb.current_color.gray;
	    break;
    }
}

/* ARGSUSED */

static void Slider1Callback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    XmScaleCallbackStruct *scaleData = (XmScaleCallbackStruct *) callData;

    switch(csb->csb.current_space) {
	case CSBSpaceRGB:
	    csb->csb.current_color.red = scaleData->value / 100.0;
	    break;
	case CSBSpaceCMYK:
	    csb->csb.current_color.cyan = scaleData->value / 100.0;
	    break;
	case CSBSpaceHSB:
	    csb->csb.current_color.hue = scaleData->value / 100.0;
	    break;
	case CSBSpaceGray:
	    csb->csb.current_color.gray = scaleData->value / 100.0;
	    break;
    }

    UpdateColorSpaces(csb, csb->csb.current_space);
    DoValueChangedCallback(csb);
    FillPatch(csb);
}

/* ARGSUSED */

static void Slider2Callback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    XmScaleCallbackStruct *scaleData = (XmScaleCallbackStruct *) callData;

    switch(csb->csb.current_space) {
	case CSBSpaceRGB:
	    csb->csb.current_color.green = scaleData->value / 100.0;
	    break;
	case CSBSpaceCMYK:
	    csb->csb.current_color.magenta = scaleData->value / 100.0;
	    break;
	case CSBSpaceHSB:
	    csb->csb.current_color.saturation = scaleData->value / 100.0;
	    break;
	case CSBSpaceGray:
	    break;
    }

    UpdateColorSpaces(csb, csb->csb.current_space);
    DoValueChangedCallback(csb);
    FillPatch(csb);
}

/* ARGSUSED */

static void Slider3Callback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    XmScaleCallbackStruct *scaleData = (XmScaleCallbackStruct *) callData;

    switch(csb->csb.current_space) {
	case CSBSpaceRGB:
	    csb->csb.current_color.blue = scaleData->value / 100.0;
	    break;
	case CSBSpaceCMYK:
	    csb->csb.current_color.yellow = scaleData->value / 100.0;
	    break;
	case CSBSpaceHSB:
	    csb->csb.current_color.brightness = scaleData->value / 100.0;
	    break;
	case CSBSpaceGray:
	    break;
    }

    UpdateColorSpaces(csb, csb->csb.current_space);
    DoValueChangedCallback(csb);
    FillPatch(csb);
}

/* ARGSUSED */

static void Slider4Callback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    XmScaleCallbackStruct *scaleData = (XmScaleCallbackStruct *) callData;

    csb->csb.current_color.black = scaleData->value / 100.0;

    UpdateColorSpaces(csb, csb->csb.current_space);
    DoValueChangedCallback(csb);
    FillPatch(csb);
}

static void FillPatch(ColorSelectionBoxWidget csb)
{
    Colormap c;
    XColor xc;
    Widget patch = csb->csb.patch_child;

    if (!XtIsRealized(csb->csb.patch_child)) return;

    if (csb->csb.no_background) {
	XClearArea(XtDisplay(patch), XtWindow(patch), 0, 0, 1000, 1000, True);
	return;
    }

    /* All we have to do is set the background; the expose event will
       do the rest */

    XtVaGetValues(patch, XtNcolormap, (XtPointer) &c, NULL);

    if (csb->csb.current_space == CSBSpaceGray) {
	xc.red = xc.green = xc.blue = TO_X(csb->csb.current_color.gray);
    } else {
	xc.red = TO_X(csb->csb.current_color.red);
	xc.green = TO_X(csb->csb.current_color.green);
	xc.blue = TO_X(csb->csb.current_color.blue);
    }

    if (csb->csb.static_visual) {
	(void) XAllocColor(XtDisplay(patch), c, &xc);
	csb->csb.background = xc.pixel;
	XtVaSetValues(patch, XtNbackground, csb->csb.background, NULL);
    } else {
	xc.pixel = csb->csb.background;
	xc.flags = DoRed | DoGreen | DoBlue;
	XStoreColor(XtDisplay(patch), c, &xc);
    }

    XClearArea(XtDisplay(patch), XtWindow(patch), 0, 0, 1000, 1000, True);
}

/* ARGSUSED */

static void FillPatchCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Dimension height, width;
    float fh, fw;

    if (csb->csb.current_rendering != CSBDisplayX) {
	XtVaGetValues(w, XtNheight, &height, XtNwidth, &width, NULL);
	if (csb->csb.patch_gstate == 0) {
	    XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	    XDPSSetContextDrawable(csb->csb.context, XtWindow(w), height);
	    (void) XDPSCaptureContextGState(csb->csb.context,
					    &csb->csb.patch_gstate);
	} else XDPSSetContextGState(csb->csb.context, csb->csb.patch_gstate);

	switch (csb->csb.current_space) {
	    case CSBSpaceRGB:
	        DPSsetrgbcolor(csb->csb.context, csb->csb.current_color.red,
			       csb->csb.current_color.green,
			       csb->csb.current_color.blue);
		break;
	    case CSBSpaceCMYK:
		DPSsetcmykcolor(csb->csb.context, csb->csb.current_color.cyan,
				csb->csb.current_color.magenta,
				csb->csb.current_color.yellow,
				csb->csb.current_color.black);
		break;
	    case CSBSpaceHSB:
		DPSsethsbcolor(csb->csb.context, csb->csb.current_color.hue,
			       csb->csb.current_color.saturation,
			       csb->csb.current_color.brightness);
		break;
	    case CSBSpaceGray:
		DPSsetgray(csb->csb.context, csb->csb.current_color.gray);
		break;
	}
    }

    switch (csb->csb.current_rendering) {
	case CSBDisplayDPS:
	    DPSrectfill(csb->csb.context, 0.0, 0.0, 1000.0, 1000.0);
	    break;
	case CSBDisplayX:
	    break;
	case CSBDisplayBoth:
	    ToUserSpace(csb, width, height, &fw, &fh);
	    _DPSCTriangle(csb->csb.context, fh, fw);
	    break;
    }	
}

/* The following function Copyright 1987, 1988 by Digital Equipment
Corporation, Maynard, Massachusetts, and the Massachusetts Institute of
Technology, Cambridge, Massachusetts. */

static String GetRootDirName(String buf)
{
#ifndef X_NOT_POSIX
     uid_t uid;
#else
     int uid;
     extern int getuid();
#ifndef SYSV386
     extern struct passwd *getpwuid(), *getpwnam();
#endif
#endif
     struct passwd *pw;
     static char *ptr = NULL;

     if (ptr == NULL) {
        if (!(ptr = getenv("HOME"))) {
            if ((ptr = getenv("USER")) != 0) {
		pw = getpwnam(ptr);
            } else {
                uid = getuid();
                pw = getpwuid(uid);
            }
            if (pw) ptr = pw->pw_dir;
            else {
                ptr = NULL;
                *buf = '\0';
            }
        }
     }

     if (ptr)
        (void) strcpy(buf, ptr);

     buf += strlen(buf);
     *buf = '/';
     buf++;
     *buf = '\0';
     return buf;
}

static void AllocateDock(ColorSelectionBoxWidget csb)
{
    int entry;

    csb->csb.dock_cyan = (float *) XtCalloc(csb->csb.num_cells, sizeof(float));
    csb->csb.dock_magenta =
	    (float *) XtCalloc(csb->csb.num_cells, sizeof(float));
    csb->csb.dock_yellow =
	    (float *) XtCalloc(csb->csb.num_cells, sizeof(float));
    csb->csb.dock_black =
	    (float *) XtCalloc(csb->csb.num_cells, sizeof(float));
    csb->csb.dock_used =
	    (Boolean *) XtCalloc(csb->csb.num_cells, sizeof(Boolean));

    for (entry = 0; entry < csb->csb.num_cells; entry++) {
	csb->csb.dock_used[entry] = 0;
    }
}

static void InitializeDock(ColorSelectionBoxWidget csb)
{
    String dockEnv;
    char homeDir[PATH_BUF_SIZE];
    FILE *dockFile = NULL;
    char fileName[PATH_BUF_SIZE];
#define BUF 256
    char buf[BUF+1];
    int entry;
    float cyan, magenta, yellow, black;
#define CHECK(v) ((v) > 1.0 ? 1.0 : ((v) < 0.0 ? 0.0 : (v)))

    AllocateDock(csb);
    csb->csb.dock_changed = False;

    dockEnv = getenv("DPSCPICKRC");

    if (dockEnv != NULL) dockFile = fopen(dockEnv, "r");

    if (dockFile == NULL) {
	(void) GetRootDirName(homeDir);

	if (dockFile == NULL) {
	    sprintf(fileName, "%s/.dpscpickrc", homeDir);
	    dockFile = fopen(fileName, "r");
	    
	    if (dockFile == NULL) return;
	}
    }

    while (1) {
	if (fgets(buf, BUF, dockFile) == NULL) {
	    fclose(dockFile);
	    return;
	}
	if (sscanf(buf, "%d %f %f %f %f",
		   &entry, &cyan, &magenta, &yellow, &black) == 5) {
	    if (entry <= csb->csb.num_cells) {
		csb->csb.dock_cyan[entry] = CHECK(cyan);
		csb->csb.dock_magenta[entry] = CHECK(magenta);
		csb->csb.dock_yellow[entry] = CHECK(yellow);
		csb->csb.dock_black[entry] = CHECK(black);
		csb->csb.dock_used[entry] = True;
	    }
	}
    }

#undef BUF
#undef CHECK
}

static void SaveDockContents(ColorSelectionBoxWidget csb)
{
    String dockEnv;
    char homeDir[PATH_BUF_SIZE];
    FILE *dockFile = NULL;
    char fileName[PATH_BUF_SIZE];
    int i;

    if (!csb->csb.dock_changed) return;

    dockEnv = getenv("DPSCPICKRC");

    if (dockEnv != NULL) dockFile = fopen(dockEnv, "w");

    if (dockFile == NULL) {
	(void) GetRootDirName(homeDir);

	if (dockFile == NULL) {
	    sprintf(fileName, "%s/.dpscpickrc", homeDir);
	    dockFile = fopen(fileName, "w");
	    
	    if (dockFile == NULL) return;
	}
    }

    for (i = 0; i < csb->csb.num_cells; i++) {
	if (!csb->csb.dock_used[i]) continue;
	fprintf(dockFile, "%d %g %g %g %g\n", i, csb->csb.dock_cyan[i],
		csb->csb.dock_magenta[i], csb->csb.dock_yellow[i],
		csb->csb.dock_black[i]);
    }
    fclose(dockFile);
    csb->csb.dock_changed = False;
}

/* ARGSUSED */

static void DrawDockCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;

    XClearArea(XtDisplay(csb), XtWindow(csb->csb.dock_child),
	       0, 0, 1000, 1000, False);
    DrawDock(csb);
}

static void DrawDock(ColorSelectionBoxWidget csb)
{
    Dimension height;
    float w, h;
    int lines;
    int i, row, col;
    Boolean didAny = False;

    XtVaGetValues(csb->csb.dock_child, XtNheight, &height, NULL);

    lines = height / csb->csb.cell_size;

    if (csb->csb.dock_gstate == 0) {
	XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	XDPSSetContextDrawable(csb->csb.context,
			       XtWindow(csb->csb.dock_child), height);
	(void) XDPSCaptureContextGState(csb->csb.context,
					&csb->csb.dock_gstate);
    } else XDPSSetContextGState(csb->csb.context, csb->csb.dock_gstate);

    ToUserSpace(csb, csb->csb.cell_size, csb->csb.cell_size, &w, &h);

    for (i = 0; i < csb->csb.num_cells; i++) {
	if (!csb->csb.dock_used[i]) continue;
	row = (lines - 1) - (i % lines);
	col = i / lines;

	DPSsetcmykcolor(csb->csb.context, csb->csb.dock_cyan[i],
			csb->csb.dock_magenta[i], csb->csb.dock_yellow[i],
			csb->csb.dock_black[i]);
	
	DPSrectfill(csb->csb.context,
		    (float) (col * w), (float) (row * h), w, h);
	didAny = True;
    }
    if (!didAny) _DPSCShowFillMe(csb->csb.context, csb->csb.fill_me);
}

static void StoreColorInDock(
    ColorSelectionBoxWidget csb,
    int x_offset,
    int y_offset,
    Dimension dockHeight)
{
    int i, lines, row, col;

    lines = dockHeight / csb->csb.cell_size;

    row = y_offset / (int) csb->csb.cell_size;
    col = x_offset / (int) csb->csb.cell_size;
    i = col * lines + row;

    if (i >= csb->csb.num_cells) i = csb->csb.num_cells;
    csb->csb.dock_cyan[i] = csb->csb.current_color.cyan;
    csb->csb.dock_magenta[i] = csb->csb.current_color.magenta;
    csb->csb.dock_yellow[i] = csb->csb.current_color.yellow;
    csb->csb.dock_black[i] = csb->csb.current_color.black;
    csb->csb.dock_used[i] = True;
    csb->csb.dock_changed = True;
    DrawDock(csb);
}

/* ARGSUSED */

static void DockPress(
    Widget w,
    XtPointer data,
    XEvent *event,
    Boolean *goOn)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) data;
    Dimension height;
    int i, lines, row, col;

    XtVaGetValues(csb->csb.dock_child, XtNheight, &height, NULL);

    lines = height / csb->csb.cell_size;

    row = event->xbutton.y / (int) csb->csb.cell_size;
    col = event->xbutton.x / (int) csb->csb.cell_size;
    i = col * lines + row;
    if (i >= csb->csb.num_cells) i = csb->csb.num_cells;

    if (!csb->csb.dock_used[i]) return;

    csb->csb.current_color.cyan = csb->csb.dock_cyan[i];
    csb->csb.current_color.magenta = csb->csb.dock_magenta[i];
    csb->csb.current_color.yellow = csb->csb.dock_yellow[i];
    csb->csb.current_color.black = csb->csb.dock_black[i];
    UpdateColorSpaces(csb, CSBSpaceCMYK);
    DoValueChangedCallback(csb);
    FillPatch(csb);
    SetSliders(csb);
}

static void InitializePalettes(ColorSelectionBoxWidget csb)
{
    int k;

    for (k = 0; k < PALETTE_MAX; k++) {
	if (csb->csb.palette_function[k] != NULL) {
	    DPSPrintf(csb->csb.context,
		      "/palette%dfunc%d { %s } bind def\n", k, (int) csb,
		      csb->csb.palette_function[k]);
	}
	csb->csb.palette_broken[k] = False;
    }
}

static void InvalidatePalette(ColorSelectionBoxWidget csb)
{
    int len;
    char *buf;
    Widget w;
    register int i = csb->csb.current_palette;

    len = strlen(csb->csb.palette_label[i]) +
	    strlen(csb->csb.broken_palette_label) + 2;
    len = MAX(len, 11);
    buf = (char *) XtMalloc(len);

    csb->csb.palette_broken[i] = True;
    sprintf(buf, "*palette%d", csb->csb.current_palette);
    w = XtNameToWidget((Widget) csb, buf);
    if (w != NULL) XtSetSensitive(w, False);
    sprintf(buf, "%s %s", csb->csb.palette_label[i],
	    csb->csb.broken_palette_label);
    len = strlen(buf);
    XtVaSetValues(w, XtVaTypedArg, XmNlabelString, XtRString, buf, len, NULL);
}

static void DoPalette(
    ColorSelectionBoxWidget csb,
    Dimension pixelWidth,
    float w,
    float h)
{
    char whichFunc[25];
    int steps;
    int success;

    sprintf(whichFunc, "palette%dfunc%d", csb->csb.current_palette, (int) csb);
    if (csb->csb.visual_class == TrueColor) steps = pixelWidth / 2;
    else steps = pixelWidth / 4;
	
    if (csb->csb.palette_color_dependent[csb->csb.current_palette]) {
	switch (csb->csb.palette_space[csb->csb.current_palette]) {
	    case CSBSpaceRGB:
		_DPSCDoRGBColorPalette(csb->csb.context, whichFunc,
				       csb->csb.current_color.red,
				       csb->csb.current_color.green,
				       csb->csb.current_color.blue,
				       w, h, steps, &success);
		break;
	    case CSBSpaceCMYK:
		_DPSCDoCMYKColorPalette(csb->csb.context, whichFunc,
					csb->csb.current_color.cyan,
					csb->csb.current_color.magenta,
					csb->csb.current_color.yellow,
					csb->csb.current_color.black,
					w, h, steps, &success);
		break;
	    case CSBSpaceHSB:
		_DPSCDoHSBColorPalette(csb->csb.context, whichFunc,
				       csb->csb.current_color.hue,
				       csb->csb.current_color.saturation,
				       csb->csb.current_color.brightness,
				       w, h, steps, &success);
		break;
	    case CSBSpaceGray:
		_DPSCDoGrayColorPalette(csb->csb.context, whichFunc,
					csb->csb.current_color.gray,
					w, h, steps, &success);
		break;
	}
    } else {
	switch (csb->csb.palette_space[csb->csb.current_palette]) {
	    case CSBSpaceRGB:
		_DPSCDoRGBPalette(csb->csb.context, whichFunc, w, h,
				  steps, &success);
		break;
	    case CSBSpaceCMYK:
		_DPSCDoCMYKPalette(csb->csb.context, whichFunc, w, h,
				   steps, &success);
		break;
	    case CSBSpaceHSB:
		_DPSCDoHSBPalette(csb->csb.context, whichFunc, w, h,
				  steps, &success);
		break;
	    case CSBSpaceGray:
		_DPSCDoGrayPalette(csb->csb.context, whichFunc, w, h,
				   steps, &success);
		break;
	}
    }
    if (!success) {
	InvalidatePalette(csb);
	_DPSCShowMessage(csb->csb.context, csb->csb.broken_palette_message);
    }
}

static void DrawPalette(ColorSelectionBoxWidget csb)
{
    DrawPaletteCallback(csb->csb.palette_child,
			(XtPointer) csb, (XtPointer) NULL);
}

/* ARGSUSED */

static void DrawPaletteCallback(
    Widget wid,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Dimension width, height;
    Pixmap palette_pixmap;
    int depth;
    float w, h;

    if (csb->csb.palette_broken[csb->csb.current_palette]) return;
    if (!csb->csb.palette_pixmap_valid) {
	XtVaGetValues(csb->csb.palette_child,
		      XtNwidth, &width, XtNheight, &height,
		      XtNdepth, &depth, NULL);

	ToUserSpace(csb, width, height, &w, &h);

	palette_pixmap =
		XCreatePixmap(XtDisplay(csb), XtWindow(csb->csb.palette_child),
			      width, height, depth);
    
	XDPSSetContextGState(csb->csb.context, csb->csb.base_gstate);
	XDPSSetContextDrawable(csb->csb.context, palette_pixmap, height);

	DoPalette(csb, width, w, h);
	csb->csb.palette_color = csb->csb.current_color;
	DPSWaitContext(csb->csb.context);
	XtVaSetValues(csb->csb.palette_child,
		      XtNbackgroundPixmap, palette_pixmap, NULL);
	XFreePixmap(XtDisplay(csb), palette_pixmap);
	csb->csb.palette_pixmap_valid = True;
    }
}

/* ARGSUSED */

static void PalettePress(
    Widget w,
    XtPointer data,
    XEvent *event,
    Boolean *goOn)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) data;
    Dimension width;
    float pct;
    char whichFunc[25];
    int success;
    float f1, f2, f3, f4;

    if (csb->csb.palette_broken[csb->csb.current_palette]) return;

    sprintf(whichFunc, "palette%dfunc%d", csb->csb.current_palette, (int) csb);

    XtVaGetValues(csb->csb.palette_child, XtNwidth, &width, NULL);

    pct = ((float) event->xbutton.x) / ((float) width);

    if (csb->csb.palette_color_dependent[csb->csb.current_palette]) {
	switch (csb->csb.palette_space[csb->csb.current_palette]) {
	    case CSBSpaceRGB:
		_DPSCQueryRGBColorPalette(csb->csb.context, whichFunc, pct,
					  csb->csb.palette_color.red,
					  csb->csb.palette_color.green,
					  csb->csb.palette_color.blue,
					  &f1, &f2, &f3, &success);
		if (success) {
		    csb->csb.current_color.red = f1;
		    csb->csb.current_color.green = f2;
		    csb->csb.current_color.blue = f3;
		}
		break;
	    case CSBSpaceCMYK:
		_DPSCQueryCMYKColorPalette(csb->csb.context, whichFunc, pct,
					   csb->csb.palette_color.cyan,
					   csb->csb.palette_color.magenta,
					   csb->csb.palette_color.yellow,
					   csb->csb.palette_color.black,
					   &f1, &f2, &f3, &f4, &success);
		if (success) {
		    csb->csb.current_color.cyan = f1;
		    csb->csb.current_color.magenta = f2;
		    csb->csb.current_color.yellow = f3;
		    csb->csb.current_color.black = f4;
		}
		break;
	    case CSBSpaceHSB:
		_DPSCQueryHSBColorPalette(csb->csb.context, whichFunc, pct,
					  csb->csb.palette_color.hue,
					  csb->csb.palette_color.saturation,
					  csb->csb.palette_color.brightness,
					  &f1, &f2, &f3, &success);
		if (success) {
		    csb->csb.current_color.hue = f1;
		    csb->csb.current_color.saturation = f2;
		    csb->csb.current_color.brightness = f3;
		}
		break;
	    case CSBSpaceGray:
		_DPSCQueryGrayColorPalette(csb->csb.context, whichFunc, pct,
					   csb->csb.palette_color.gray,
					   &f1, &success);
		if (success) csb->csb.current_color.gray = f1;
		break;
	}
    } else {
	switch (csb->csb.palette_space[csb->csb.current_palette]) {
	    case CSBSpaceRGB:
		_DPSCQueryRGBPalette(csb->csb.context, whichFunc, pct,
				     &f1, &f2, &f3, &success);
		if (success) {
		    csb->csb.current_color.red = f1;
		    csb->csb.current_color.green = f2;
		    csb->csb.current_color.blue = f3;
		}
		break;
	    case CSBSpaceCMYK:
		_DPSCQueryCMYKPalette(csb->csb.context, whichFunc, pct,
				      &f1, &f2, &f3, &f4, &success);
		if (success) {
		    csb->csb.current_color.cyan = f1;
		    csb->csb.current_color.magenta = f2;
		    csb->csb.current_color.yellow = f3;
		    csb->csb.current_color.black = f4;
		}
		break;
	    case CSBSpaceHSB:
		_DPSCQueryHSBPalette(csb->csb.context, whichFunc, pct,
				     &f1, &f2, &f3, &success);
		if (success) {
		    csb->csb.current_color.hue = f1;
		    csb->csb.current_color.saturation = f2;
		    csb->csb.current_color.brightness = f3;
		}
		break;
	    case CSBSpaceGray:
		_DPSCQueryGrayPalette(csb->csb.context, whichFunc, pct,
				      &f1, &success);
		if (success) csb->csb.current_color.gray = f1;
		break;
	}
    }
    if (!success) InvalidatePalette(csb);
    else {
	UpdateColorSpaces(csb,
			  csb->csb.palette_space[csb->csb.current_palette]);
	DoValueChangedCallback(csb);
	FillPatch(csb);
	SetSliders(csb);
    }
}

/* ARGSUSED */

static void DoEyedropCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) clientData;
    Pixmap eyedropBitmap, eyedropMaskBitmap;
    XColor black, fg;
    Display *dpy;
    unsigned int x, y;
    XEvent ev;

    dpy = XtDisplay(w);

    black.red = 0;
    black.green = 0;
    black.blue = 0;

    fg.red = 65535;
    fg.green = 65535;
    fg.blue = 65535;

    if (csb->csb.eyedrop == None) {
	XQueryBestCursor(dpy, XtWindow(w), 32, 32,
			 &x, &y);

	if (x >= 32 && y >= 32) {
	    eyedropBitmap =
		    XCreateBitmapFromData(dpy, XtWindow(w),
					  (char *) eyedrop32_bits,
					  eyedrop32_width, eyedrop32_height);

	    eyedropMaskBitmap =
		    XCreateBitmapFromData(dpy, XtWindow(w),
					  (char *) eyedropmask32_bits,
					  eyedropmask32_width,
					  eyedropmask32_height);

	    csb->csb.eyedrop =
		    XCreatePixmapCursor(dpy, eyedropBitmap,
					eyedropMaskBitmap,
					&fg, &black,
					eyedrop32_x_hot, eyedrop32_y_hot);
	} else {
	    eyedropBitmap =
		    XCreateBitmapFromData(dpy, XtWindow(w),
					  (char *) eyedrop16_bits,
					  eyedrop16_width, eyedrop16_height);

	    eyedropMaskBitmap =
		    XCreateBitmapFromData(dpy, XtWindow(w),
					  (char *) eyedropmask16_bits,
					  eyedropmask16_width,
					  eyedropmask16_height);

	    csb->csb.eyedrop =
		    XCreatePixmapCursor(dpy, eyedropBitmap,
					eyedropMaskBitmap,
					&fg, &black,
					eyedrop16_x_hot, eyedrop16_y_hot);
	}
    } else {
	XRecolorCursor(dpy, csb->csb.eyedrop, &fg, &black);
    }
				  
    (void) XtGrabPointer(w, False,
			 PointerMotionMask | PointerMotionHintMask |
				 ButtonReleaseMask,
			 GrabModeAsync, GrabModeAsync,
			 None, csb->csb.eyedrop,
			 XtLastTimestampProcessed(dpy));
    csb->csb.eyedrop_grabbed = True;

    ev.type = 0;
    EyedropPointer(w, (XtPointer) csb, &ev, (Boolean *) NULL);
}

/* ARGSUSED */

static void EyedropPointer(
    Widget w,
    XtPointer data,
    XEvent *event,
    Boolean *goOn)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) data;
    XColor fg, black;
    Window root, child, stop, old_child = None;
    int root_x, root_y, x, y;
    unsigned int mask;
    XWindowAttributes att;
    XImage *image;
    Pixel pixel;
    Colormap colormap = 0;
    Display *dpy = XtDisplay(w);

    if (!csb->csb.eyedrop_grabbed) return;

    if (event->type == ButtonPress || event->type == ButtonRelease) {
	root = event->xbutton.root;
	root_x = event->xbutton.x_root;
	root_y = event->xbutton.y_root;

	XTranslateCoordinates(dpy, root, root, root_x, root_y, &x, &y, &child);

    } else {
	XQueryPointer(dpy, RootWindowOfScreen(XtScreen(w)),
		      &root, &child, &root_x, &root_y, &x, &y, &mask);
    }

    if (child == None) child = root;
    else {
	stop = child;

	while (stop != None) {
	    XTranslateCoordinates(dpy, root, stop, x, y, &x, &y, &child);
	    root = stop;
	    if (child != None && XGetWindowAttributes(dpy, child, &att) &&
		att.class != InputOutput) break;
	    stop = child;
	}
	child = root;
    }

    if (child != old_child) {
	XGetWindowAttributes(dpy, child, &att);
	colormap = att.colormap;
	old_child = child;
    }

    image = XGetImage(dpy, child, x, y, 1, 1, AllPlanes, XYPixmap);

    pixel = XGetPixel(image, 0, 0);

    XDestroyImage(image);
    fg.pixel = pixel;
    XQueryColors(dpy, colormap, &fg, 1);
	
    black.red = 0;
    black.green = 0;
    black.blue = 0;

    XRecolorCursor(dpy, csb->csb.eyedrop, &fg, &black);

    if (event->type == ButtonRelease) {
	XtUngrabPointer(w, XtLastTimestampProcessed(dpy));
	csb->csb.eyedrop_grabbed = False;

	csb->csb.current_color.red = (float) fg.red / 65535.0;
	csb->csb.current_color.green = (float) fg.green / 65535.0;
	csb->csb.current_color.blue = (float) fg.blue / 65535.0;
	UpdateColorSpaces(csb, CSBSpaceRGB);
	DoValueChangedCallback(csb);
	FillPatch(csb);
	SetSliders(csb);
   }
}

/* ARGSUSED */

static void PatchPress(
    Widget w,
    XtPointer data,
    XEvent *event,
    Boolean *goOn)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) data;
    Pixmap squareBitmap, squareMaskBitmap;
    XColor black, fg;
    Display *dpy;

    dpy = XtDisplay(w);

    black.red = 0;
    black.green = 0;
    black.blue = 0;

    fg.red = TO_X(csb->csb.current_color.red);
    fg.green = TO_X(csb->csb.current_color.green);
    fg.blue = TO_X(csb->csb.current_color.blue);

    if (csb->csb.square == None) {
	squareBitmap =
		XCreateBitmapFromData(dpy, XtWindow(w), (char *) square_bits,
				      square_width, square_height);

	squareMaskBitmap =
		XCreateBitmapFromData(dpy, XtWindow(w),
				      (char *) squaremask_bits,
				      squaremask_width, squaremask_height);

	csb->csb.square =
		XCreatePixmapCursor(dpy, squareBitmap, squareMaskBitmap,
				    &fg, &black, square_x_hot, square_y_hot);
    } else {
	XRecolorCursor(dpy, csb->csb.square, &fg, &black);
    }

    (void) XtGrabPointer(w, False, ButtonReleaseMask,
			 GrabModeAsync, GrabModeAsync,
			 None, csb->csb.square, XtLastTimestampProcessed(dpy));
}

/* ARGSUSED */

static void PatchRelease(
    Widget w,
    XtPointer data,
    XEvent *event,
    Boolean *goOn)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) data;
    Dimension width, height;
    Position left, top;

    XtUngrabPointer(w, XtLastTimestampProcessed(XtDisplay(w)));
    XFlush(XtDisplay(w));

    XtVaGetValues(csb->csb.dock_child, XtNwidth, &width,
		  XtNheight, &height, NULL);

    XtTranslateCoords(csb->csb.dock_child, (Position) 0, (Position) 0,
		      &left, &top);

    if ((int) event->xbutton.x_root >= left &&
	(int) event->xbutton.x_root <= left + (int) width &&
	(int) event->xbutton.y_root >= top  &&
	(int) event->xbutton.y_root <= top + (int) height) {
	StoreColorInDock(csb, event->xbutton.x_root - left,
			 event->xbutton.y_root - top, height);
    }
}

static void GetVisualInfo(
    ColorSelectionBoxWidget csb,
    Visual **visual)
{
    Widget w = (Widget) csb;
    XVisualInfo *vip, viproto;
    int n;
    XWindowAttributes xwa;

    XGetWindowAttributes(XtDisplay(w), XtWindow(w), &xwa);

    *visual = viproto.visual = xwa.visual;
    viproto.visualid = XVisualIDFromVisual(xwa.visual);
    vip = XGetVisualInfo(XtDisplay(w), VisualIDMask, &viproto, &n);

    if (n != 1) {
	csb->csb.static_visual = False;	/* Actually we have no idea, but... */
	csb->csb.visual_class = PseudoColor;
    } else {
	csb->csb.visual_class = vip->class;
	csb->csb.static_visual = (vip->class == StaticGray ||
				  vip->class == TrueColor ||
				  vip->class == StaticColor);
    }

    if (n > 0) XFree((char *) vip);
}

static void SetBackground(ColorSelectionBoxWidget csb)
{
    Colormap c;
    XColor xc;
    int status;
    unsigned long pix;
    unsigned long mask;

    XtVaGetValues(csb->csb.patch_child, XtNcolormap, (XtPointer) &c, NULL);

    if (csb->csb.current_space == CSBSpaceGray) {
	xc.red = xc.green = xc.blue = TO_X(csb->csb.current_color.gray);
    } else {
	xc.red = TO_X(csb->csb.current_color.red);
	xc.green = TO_X(csb->csb.current_color.green);
	xc.blue = TO_X(csb->csb.current_color.blue);
    }

    if (csb->csb.static_visual) {
	status = XAllocColor(XtDisplay(csb), c, &xc);
	if (status == 0) NoBackgroundPixel(csb);
	else {
	    csb->csb.background = xc.pixel;
	    XtVaSetValues(csb->csb.patch_child,
			  XtNbackground, csb->csb.background, NULL);
	}

    } else {
	if (csb->csb.visual_class == DirectColor) {
	    status = XAllocColorPlanes(XtDisplay(csb), c,
				       False, &pix, 1, 0, 0, 0,
				       &mask, &mask, &mask);
	} else {
	    status = XAllocColorCells(XtDisplay(csb), c,
				      False, (unsigned long *) NULL, 0,
				      &pix, 1);
	}

	if (status == 0) NoBackgroundPixel(csb);
	else {
	    xc.pixel = pix;
	    xc.flags = DoRed | DoGreen | DoBlue;
	    XStoreColor(XtDisplay(csb), c, &xc);

	    csb->csb.background = xc.pixel;
	    XtVaSetValues(csb->csb.patch_child,
			  XtNbackground, csb->csb.background, NULL);
	}
    }
}

/* ARGSUSED */

static void Initialize(
    Widget request, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) new;
    Bool inited;
    int i;

    if (csb->csb.rgb_labels != NULL) {
	csb->csb.rgb_labels = XtNewString(csb->csb.rgb_labels);
    }
    if (csb->csb.cmyk_labels != NULL) {
	csb->csb.cmyk_labels = XtNewString(csb->csb.cmyk_labels);
    }
    if (csb->csb.hsb_labels != NULL) {
	csb->csb.hsb_labels = XtNewString(csb->csb.hsb_labels);
    }
    if (csb->csb.gray_labels != NULL) {
	csb->csb.gray_labels = XtNewString(csb->csb.gray_labels);
    }
    if (csb->csb.fill_me != NULL) {
	csb->csb.fill_me = XtNewString(csb->csb.fill_me);
    }
    if (csb->csb.broken_palette_label != NULL) {
	csb->csb.broken_palette_label =
		XtNewString(csb->csb.broken_palette_label);
    }
    if (csb->csb.broken_palette_message != NULL) {
	csb->csb.broken_palette_message =
		XtNewString(csb->csb.broken_palette_message);
    }

    for (i = 0; i < PALETTE_MAX; i++) {
	if (csb->csb.palette_function[i] != NULL) {
	    csb->csb.palette_function[i] =
		    XtNewString(csb->csb.palette_function[i]);
	}
    }

    if (csb->csb.num_cells <= 0) csb->csb.num_cells = 1;

    /* Get the context */

    if (csb->csb.context == NULL) {
	csb->csb.context = XDPSGetSharedContext(XtDisplay(csb));
    }

    if (_XDPSTestComponentInitialized(csb->csb.context,
				      dps_init_bit_csb, &inited) ==
	dps_status_unregistered_context) {
	XDPSRegisterContext(csb->csb.context, False);
    }

    if (!inited) {
	(void) _XDPSSetComponentInitialized(csb->csb.context,
					    dps_init_bit_csb);
	InitializePalettes(csb);
    }

    if (csb->csb.current_palette < 0 ||
	csb->csb.current_palette > PALETTE_MAX ||
	csb->csb.palette_function[csb->csb.current_palette] == NULL) {
	csb->csb.current_palette = 0;
    }

    /* Initialize non-resource fields */

    CreateChildren(csb);
    csb->csb.no_background = False;
    csb->csb.patch_gstate = csb->csb.dock_gstate = 0;
    csb->csb.red_pixmap = csb->csb.green_pixmap = csb->csb.blue_pixmap =
	    csb->csb.cyan_pixmap = csb->csb.magenta_pixmap =
	    csb->csb.yellow_pixmap = csb->csb.black_pixmap =
	    csb->csb.hue_pixmap = csb->csb.sat_pixmap =
	    csb->csb.bright_pixmap = csb->csb.gray_pixmap = None;

    csb->csb.square = csb->csb.eyedrop = None;
    csb->csb.eyedrop_grabbed = False;

    for (i = 0; i < PALETTE_MAX; i++) csb->csb.palette_broken[i] = False;
    csb->csb.palette_pixmap_valid = False;

    csb->csb.current_color.hue = 0.0;
    csb->csb.current_color.saturation = 1.0;
    csb->csb.current_color.brightness = 1.0;
    UpdateColorSpaces(csb, CSBSpaceHSB);
    csb->csb.save_color = csb->csb.current_color;
    SetSliders(csb);

    InitializeDock(csb);    
    SetColorSpace(csb);
    SetRendering(csb);
}

static void Destroy(Widget widget)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) widget;
    Display *dpy = XtDisplay(csb);
    int i;

    /* Lots of stuff to destroy! */

    if (csb->csb.patch_gstate != 0) {
	XDPSFreeContextGState(csb->csb.context, csb->csb.patch_gstate);
    }
    if (csb->csb.dock_gstate != 0) {
	XDPSFreeContextGState(csb->csb.context, csb->csb.dock_gstate);
    }
    if (csb->csb.base_gstate != 0) {
	XDPSFreeContextGState(csb->csb.context, csb->csb.base_gstate);
    }

    if (csb->csb.rgb_labels != NULL) XtFree(csb->csb.rgb_labels);
    if (csb->csb.cmyk_labels != NULL) XtFree(csb->csb.cmyk_labels);
    if (csb->csb.hsb_labels != NULL) XtFree(csb->csb.hsb_labels);
    if (csb->csb.gray_labels != NULL) XtFree(csb->csb.gray_labels);
    if (csb->csb.fill_me != NULL) XtFree(csb->csb.fill_me);
    if (csb->csb.broken_palette_message != NULL) {
	XtFree(csb->csb.broken_palette_message);
    }
    if (csb->csb.broken_palette_label != NULL) {
	XtFree(csb->csb.broken_palette_label);
    }
	
    XtFree((XtPointer) csb->csb.dock_cyan);
    XtFree((XtPointer) csb->csb.dock_magenta);
    XtFree((XtPointer) csb->csb.dock_yellow);
    XtFree((XtPointer) csb->csb.dock_black);
    XtFree((XtPointer) csb->csb.dock_used);

    for (i = 0; i < PALETTE_MAX; i++) {
	if (csb->csb.palette_function[i] != NULL) {
	    XtFree(csb->csb.palette_function[i]);
	}
    }

    if (csb->csb.eyedrop != None) XFreeCursor(dpy, csb->csb.eyedrop);
    if (csb->csb.square != None) XFreeCursor(dpy, csb->csb.square);

    if (csb->csb.red_pixmap != None) XFreePixmap(dpy, csb->csb.red_pixmap);
    if (csb->csb.green_pixmap != None) XFreePixmap(dpy, csb->csb.green_pixmap);
    if (csb->csb.blue_pixmap != None) XFreePixmap(dpy, csb->csb.blue_pixmap);
    if (csb->csb.cyan_pixmap != None) XFreePixmap(dpy, csb->csb.cyan_pixmap);
    if (csb->csb.magenta_pixmap != None)
	    XFreePixmap(dpy, csb->csb.magenta_pixmap);
    if (csb->csb.yellow_pixmap != None)
	    XFreePixmap(dpy, csb->csb.yellow_pixmap);
    if (csb->csb.black_pixmap != None) XFreePixmap(dpy, csb->csb.black_pixmap);
    if (csb->csb.hue_pixmap != None) XFreePixmap(dpy, csb->csb.hue_pixmap);
    if (csb->csb.sat_pixmap != None) XFreePixmap(dpy, csb->csb.sat_pixmap);
    if (csb->csb.bright_pixmap != None)
	    XFreePixmap(dpy, csb->csb.bright_pixmap);
    if (csb->csb.gray_pixmap != None) XFreePixmap(dpy, csb->csb.gray_pixmap);
}

static void ChangeManaged(Widget w)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) w;

    w->core.width = csb->composite.children[0]->core.width;
    w->core.height = csb->composite.children[0]->core.height;
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

static void SetColorSpace(ColorSelectionBoxWidget csb)
{
    switch(csb->csb.current_space) {
	case CSBSpaceRGB:	
	    SetRGBCallback((Widget) csb, (XtPointer) csb, (XtPointer) NULL);
	    break;

	case CSBSpaceCMYK:
	    SetCMYKCallback((Widget) csb, (XtPointer) csb, (XtPointer) NULL);
	    break;

	case CSBSpaceHSB:
	    SetHSBCallback((Widget) csb, (XtPointer) csb, (XtPointer) NULL);
	    break;

	case CSBSpaceGray:
	    SetGrayCallback((Widget) csb, (XtPointer) csb, (XtPointer) NULL);
	    break;
    }
}

static void SetRendering(ColorSelectionBoxWidget csb)
{
    Widget w;

    switch(csb->csb.current_rendering) {
	default:
	case CSBDisplayDPS:
	    w = XtNameToWidget((Widget) csb, "*displayDPS");
	    break;
	case CSBDisplayX:
	    w = XtNameToWidget((Widget) csb, "*displayX");
	    break;
	case CSBDisplayBoth:
	    w = XtNameToWidget((Widget) csb, "*displayBoth");
	    break;
    }
    XtVaSetValues(csb->csb.display_option_menu_child, XmNmenuHistory, w, NULL);
    if (XtIsRealized(csb->csb.patch_child)) {
	XClearArea(XtDisplay(csb), XtWindow(csb->csb.patch_child),
		   0, 0, 1000, 1000, True);
    }
}

static void SetPalette(ColorSelectionBoxWidget csb)
{
    Widget w;
    char buf[10];

    sprintf(buf, "*palette%d", csb->csb.current_palette);
    w = XtNameToWidget((Widget) csb, buf);

    XtVaSetValues(csb->csb.palette_option_menu_child, XmNmenuHistory, w, NULL);

    csb->csb.palette_pixmap_valid = False;
    DrawPalette(csb);
}

static void SetBaseGState(
    ColorSelectionBoxWidget csb,
    Visual *visual)
{
    XStandardColormap colorCube, grayRamp;
    int match;

    /* If the context's colormap matches the widget's colormap, assume that
       everything is already set up right in the color cube department.  This
       allows an application to supply us with a custom color cube by
       installing it in the context before calling us */

    _DPSCColormapMatch(csb->csb.context, csb->core.colormap, &match);

    if (match) {
	XDPSSetContextParameters(csb->csb.context, XtScreen(csb),
				 csb->core.depth, XtWindow(csb),
				 csb->core.height, NULL, NULL,
				 XDPSContextScreenDepth | XDPSContextDrawable);
    } else {
	grayRamp.colormap = colorCube.colormap = csb->core.colormap;

	XDPSCreateStandardColormaps(XtDisplay(csb), XtWindow(csb), visual,
				    0, 0, 0, 0, &colorCube, &grayRamp, False);

	XDPSSetContextParameters(csb->csb.context, XtScreen(csb),
				 csb->core.depth, XtWindow(csb),
				 csb->core.height,
				 (XDPSStandardColormap *) &colorCube,
				 (XDPSStandardColormap *) &grayRamp,
				 XDPSContextScreenDepth | XDPSContextDrawable |
				 XDPSContextRGBMap | XDPSContextGrayMap);
    }

    XDPSCaptureContextGState(csb->csb.context, &csb->csb.base_gstate);
}

/* ARGSUSED */

static Boolean SetValues(
    Widget old, Widget req, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    ColorSelectionBoxWidget oldcsb = (ColorSelectionBoxWidget) old;
    ColorSelectionBoxWidget newcsb = (ColorSelectionBoxWidget) new;
    Bool inited;
    char buf[10];
    Widget w = 0;
    int i;

#define NE(field) newcsb->csb.field != oldcsb->csb.field

    if (NE(rgb_labels)) {
	XtFree(oldcsb->csb.rgb_labels);
	newcsb->csb.rgb_labels = XtNewString(newcsb->csb.rgb_labels);
    }
    if (NE(cmyk_labels)) {
	XtFree(oldcsb->csb.cmyk_labels);
	newcsb->csb.cmyk_labels = XtNewString(newcsb->csb.cmyk_labels);
    }
    if (NE(hsb_labels)) {
	XtFree(oldcsb->csb.hsb_labels);
	newcsb->csb.hsb_labels = XtNewString(newcsb->csb.hsb_labels);
    }
    if (NE(gray_labels)) {
	XtFree(oldcsb->csb.gray_labels);
	newcsb->csb.gray_labels = XtNewString(newcsb->csb.gray_labels);
    }

    if (NE(context)) {
	if (newcsb->csb.context == NULL) {
	    newcsb->csb.context = XDPSGetSharedContext(XtDisplay(newcsb));
	} 
	if (_XDPSTestComponentInitialized(newcsb->csb.context,
					  dps_init_bit_csb, &inited) ==
	    dps_status_unregistered_context) {
	    XDPSRegisterContext(newcsb->csb.context, False);
	}
	if (!inited) {
	    (void) _XDPSSetComponentInitialized(newcsb->csb.context,
						dps_init_bit_csb);
	    InitializePalettes(newcsb);
	}
	newcsb->csb.patch_gstate = newcsb->csb.dock_gstate = 0;
	XDPSFreeContextGState(newcsb->csb.context, newcsb->csb.patch_gstate);
	XDPSFreeContextGState(newcsb->csb.context, newcsb->csb.dock_gstate);
	if (XtIsRealized(newcsb)) {
	    XWindowAttributes xwa;

	    XGetWindowAttributes(XtDisplay(newcsb), XtWindow(newcsb), &xwa);
	    SetBaseGState(newcsb, xwa.visual);
	}
    }	

    if (NE(fill_me)) {
	XtFree(oldcsb->csb.fill_me);
	newcsb->csb.fill_me = XtNewString(newcsb->csb.fill_me);
    }

    if (NE(broken_palette_label)) {
	XtFree(oldcsb->csb.broken_palette_label);
	newcsb->csb.broken_palette_label =
		XtNewString(newcsb->csb.broken_palette_label);
    }

    if (NE(broken_palette_message)) {
	XtFree(oldcsb->csb.broken_palette_message);
	newcsb->csb.broken_palette_message =
		XtNewString(newcsb->csb.broken_palette_message);
    }

    if (newcsb->csb.num_cells <= 0) newcsb->csb.num_cells = 1;
    if (NE(num_cells)) {
	int i, min;

	AllocateDock(newcsb);
	min = MIN(newcsb->csb.num_cells, oldcsb->csb.num_cells);
	for (i = 0; i < min; i++) {
	    newcsb->csb.dock_cyan[i] = oldcsb->csb.dock_cyan[i];
	    newcsb->csb.dock_magenta[i] = oldcsb->csb.dock_magenta[i];
	    newcsb->csb.dock_yellow[i] = oldcsb->csb.dock_yellow[i];
	    newcsb->csb.dock_black[i] = oldcsb->csb.dock_black[i];
	    newcsb->csb.dock_used[i] = oldcsb->csb.dock_used[i];
	}
	XtFree((XtPointer) oldcsb->csb.dock_cyan);
	XtFree((XtPointer) oldcsb->csb.dock_magenta);
	XtFree((XtPointer) oldcsb->csb.dock_yellow);
	XtFree((XtPointer) oldcsb->csb.dock_black);
	XtFree((XtPointer) oldcsb->csb.dock_used);
    }

    for (i = 0; i < PALETTE_MAX; i++) {
	if (NE(palette_function[i]) || NE(palette_label[i])) {
	    sprintf(buf, "*palette%d", i);
	    w = XtNameToWidget((Widget) newcsb, buf);
	}
	if (NE(palette_function[i])) {
	    if (newcsb->csb.palette_function[i] != NULL) {
		DPSPrintf(newcsb->csb.context,
			  "/palette%dfunc%d { %s } bind def\n", i,
			  (int) newcsb, newcsb->csb.palette_function[i]);
		/* Assume the best... */
		newcsb->csb.palette_broken[i] = False;
		XtManageChild(w);
	    } else {
		XtUnmanageChild(w);
		if (newcsb->csb.current_palette == i) {
		    newcsb->csb.current_palette = -1;
		}
	    }
	}
	if (NE(palette_label[i]) || NE(palette_function[i])) {
	    XtSetSensitive(w, True);
	    XtVaSetValues(w, XtVaTypedArg, XmNlabelString, XtRString,
			  newcsb->csb.palette_label[i],
			  strlen(newcsb->csb.palette_label[i])+1, NULL);
	}
    }

    if (NE(current_palette)) {
	if (newcsb->csb.current_palette < 0 ||
	    newcsb->csb.current_palette > PALETTE_MAX ||
	    newcsb->csb.palette_function[newcsb->csb.current_palette] == NULL ||
	    newcsb->csb.palette_broken[newcsb->csb.current_palette]) {
	    newcsb->csb.current_palette = 0;
	}
    }
    if (NE(current_palette) ||
	NE(palette_function[newcsb->csb.current_palette])) SetPalette(newcsb);

    if ((NE(cell_size) || NE(fill_me)) &&
	XtIsRealized(newcsb->csb.dock_child)) {
	XClearArea(XtDisplay(newcsb), XtWindow(newcsb->csb.dock_child),
		   0, 0, 1000, 1000, True);
    }

    if (NE(current_space)) SetColorSpace(newcsb);
    if (NE(current_rendering)) SetRendering(newcsb);

    return False;
#undef NE
}

static void Realize(
    Widget w,
    XtValueMask *mask,
    XSetWindowAttributes *attr)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) w;
    Visual *v;

    (*colorSelectionBoxClassRec.core_class.superclass->core_class.realize)
	    (w, mask, attr);

    GetVisualInfo(csb, &v);
    SetBackground(csb);
    SetBaseGState(csb, v);
    _DPSCGetInvCTM(csb->csb.context, csb->csb.itransform);
}

static void Resize(Widget widget)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) widget;

    XtResizeWidget(csb->csb.form_child, csb->core.width, csb->core.height, 0);
}

static Boolean SetColor(
    Widget w,
    CSBColorSpace space,
    double c1, double c2, double c3, double c4,
    Bool setSpace)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) w;
#define CHECK(c) if ((c) > 1.0 || (c) < 0.0) return False;

    CHECK(c1);
    switch (space) {
	case CSBSpaceRGB:
	    CHECK(c2);
	    CHECK(c3);
	    csb->csb.current_color.red = c1;
	    csb->csb.current_color.green = c2;
	    csb->csb.current_color.blue = c3;
	    break;
	case CSBSpaceCMYK:
	    CHECK(c2);
	    CHECK(c3);
	    CHECK(c4);
	    csb->csb.current_color.cyan = c1;
	    csb->csb.current_color.magenta = c2;
	    csb->csb.current_color.yellow = c3;
	    csb->csb.current_color.black = c4;
	    break;
	case CSBSpaceHSB:
	    CHECK(c2);
	    CHECK(c3);
	    csb->csb.current_color.hue = c1;
	    csb->csb.current_color.saturation = c2;
	    csb->csb.current_color.brightness = c3;
	    break;
	case CSBSpaceGray:
	    csb->csb.current_color.gray = c1;
	    break;
    }
    UpdateColorSpaces(csb, space);
    csb->csb.save_color = csb->csb.current_color;
    DoValueChangedCallback(csb);
    FillPatch(csb);
    SetSliders(csb);
    if (setSpace) XtVaSetValues(w, XtNcurrentSpace, space, NULL);
    return True;
#undef CHECK
}

Boolean CSBSetColor(
    Widget w,
    CSBColorSpace space,
    double c1, double c2, double c3, double c4,
    Bool setSpace)
{
    XtCheckSubclass(w, colorSelectionBoxWidgetClass, NULL);

    return (*((ColorSelectionBoxWidgetClass) XtClass(w))->
	    csb_class.set_color) (w, space, c1, c2, c3, c4, setSpace);
}

static void GetColor(
    Widget w,
    CSBColorSpace space,
    float *c1, float *c2, float *c3, float *c4)
{
    ColorSelectionBoxWidget csb = (ColorSelectionBoxWidget) w;

    switch (space) {
	case CSBSpaceRGB:
	    *c1 = csb->csb.current_color.red;
	    *c2 = csb->csb.current_color.green;
	    *c3 = csb->csb.current_color.blue;
	    break;
	case CSBSpaceCMYK:
	    *c1 = csb->csb.current_color.cyan;
	    *c2 = csb->csb.current_color.magenta;
	    *c3 = csb->csb.current_color.yellow;
	    *c4 = csb->csb.current_color.black;
	    break;
	case CSBSpaceHSB:
	    *c1 = csb->csb.current_color.hue;
	    *c2 = csb->csb.current_color.saturation;
	    *c3 = csb->csb.current_color.brightness;
	    break;
	case CSBSpaceGray:
	    *c1 = csb->csb.current_color.gray;
	    break;
    }
}

void CSBGetColor(
    Widget w,
    CSBColorSpace space,
    float *c1, float *c2, float *c3, float *c4)
{
    XtCheckSubclass(w, colorSelectionBoxWidgetClass, NULL);

    (*((ColorSelectionBoxWidgetClass) XtClass(w))->
	    csb_class.get_color) (w, space, c1, c2, c3, c4);
}
