/*
 * FontSample.c
 *
 * (c) Copyright 1991-1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/lib/dpstk/FontSample.c,v 1.2 2000/06/07 22:03:00 tsi Exp $ */

#include <ctype.h>
#include <stdio.h>
#include <X11/Xos.h>
#include <stdlib.h>
#include <math.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>

/* There are no words to describe how I feel about having to do this */

#if XmVersion > 1001		
#include <Xm/ManagerP.h>
#else
#include <Xm/XmP.h>
#endif

#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PanedW.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/TextF.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include <Xm/ScrolledW.h>
#include <Xm/ToggleBG.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <DPS/dpsXclient.h>
#include <DPS/dpsops.h>
#include <DPS/dpsXcommon.h>
#include <DPS/dpsXshare.h>
#include <DPS/FontSBP.h>
#include "FSBwraps.h"
#include "FontSBI.h"
#include <DPS/FontSamplP.h>

#if 0
/* This is not in Xos.h for some reason */
char *strstr();
#endif

#undef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define UnsharedCS(str) XmStringCreate(str, XmSTRING_DEFAULT_CHARSET)

static float defaultSizeList[] = {
#ifndef SAMPLER_DEFAULT_SIZE_LIST
    8, 10, 12, 14, 16, 18, 24, 36, 48, 72
#else
	SAMPLER_DEFAULT_SIZE_LIST
#endif /* DEFAULT_SIZE_LIST */
};

#ifndef SAMPLER_DEFAULT_SIZE_LIST_COUNT
#define SAMPLER_DEFAULT_SIZE_LIST_COUNT 10
#endif /* DEFAULT_SIZE_LIST_COUNT */

#ifndef SAMPLER_DEFAULT_SIZE
#define SAMPLER_DEFAULT_SIZE 24.0
#endif /* SAMPLER_DEFAULT_SIZE */

static Boolean DisplayAllWorkProc(XtPointer client_data);
static Boolean DisplaySelectedWorkProc(XtPointer client_data);
static Boolean DisplaySelectedFamilyWorkProc(XtPointer client_data);
static Boolean DisplayFilteredWorkProc(XtPointer client_data);

#define Offset(field) XtOffsetOf(FontSamplerRec, sampler.field)

static XtResource resources[] = {
    {XtNsizes, XtCSizes, XtRFloatList, sizeof(float*),
	Offset(sizes), XtRImmediate, (XtPointer) defaultSizeList},
    {XtNsizeCount, XtCSizeCount, XtRInt, sizeof(int),
	Offset(size_count), XtRImmediate,
	(XtPointer) SAMPLER_DEFAULT_SIZE_LIST_COUNT},
    {XtNdismissCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(dismiss_callback), XtRCallback, (XtPointer) NULL},
    {XtNfontSelectionBox, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(fsb), XtRWidget, (XtPointer) NULL},
    {XtNminimumWidth, XtCMinimumWidth, XtRDimension, sizeof(Dimension),
	Offset(minimum_width), XtRImmediate, (XtPointer) 100},
    {XtNminimumHeight, XtCMinimumHeight, XtRDimension, sizeof(Dimension),
	Offset(minimum_height), XtRImmediate, (XtPointer) 100},
    {XtNnoRoomMessage, XtCMessage, XmRXmString, sizeof(XmString),
	Offset(no_room_message), XtRString,
	"Current size is too large or panel is too small"}, 
    {XtNnoFontMessage, XtCMessage, XmRXmString, sizeof(XmString),
	Offset(no_font_message), XtRString,
	"There are no fonts!"},
    {XtNnoSelectedFontMessage, XtCMessage, XmRXmString, sizeof(XmString),
	Offset(no_selected_font_message), XtRString,
	"No font is currently selected"},
    {XtNnoSelectedFamilyMessage, XtCMessage, XmRXmString, sizeof(XmString),
	Offset(no_selected_family_message), XtRString,
	"No family is currently selected"},
    {XtNnoFamilyFontMessage, XtCMessage, XmRXmString, sizeof(XmString),
	Offset(no_family_font_message), XtRString,
	"Selected family has no fonts!"},
    {XtNnoMatchMessage, XtCMessage, XmRXmString, sizeof(XmString),
	Offset(no_match_message), XtRString,
	"No fonts match filters"},
    {XtNpanelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(panel_child), XtRImmediate, (XtPointer) NULL},
    {XtNareaChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(area_child), XtRImmediate, (XtPointer) NULL},
    {XtNtextChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(text_child), XtRImmediate, (XtPointer) NULL},
    {XtNfontLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(font_label_child), XtRImmediate, (XtPointer) NULL},
    {XtNscrolledWindowChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(scrolled_window_child), XtRImmediate, (XtPointer) NULL},
    {XtNdisplayButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(display_button_child), XtRImmediate, (XtPointer) NULL},
    {XtNdismissButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(dismiss_button_child), XtRImmediate, (XtPointer) NULL},
    {XtNstopButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(stop_button_child), XtRImmediate, (XtPointer) NULL},
    {XtNclearButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(clear_button_child), XtRImmediate, (XtPointer) NULL},
    {XtNradioFrameChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(radio_frame_child), XtRImmediate, (XtPointer) NULL},
    {XtNradioBoxChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(radio_box_child), XtRImmediate, (XtPointer) NULL},
    {XtNallToggleChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(all_toggle_child), XtRImmediate, (XtPointer) NULL},
    {XtNselectedToggleChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(selected_toggle_child), XtRImmediate, (XtPointer) NULL},
    {XtNselectedFamilyToggleChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(selected_family_toggle_child), XtRImmediate, (XtPointer) NULL},
    {XtNfilterToggleChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(filter_toggle_child), XtRImmediate, (XtPointer) NULL},
    {XtNfilterTextChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(filter_text_child), XtRImmediate, (XtPointer) NULL},
    {XtNfilterBoxChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(filter_box_child), XtRImmediate, (XtPointer) NULL},
    {XtNfilterFrameChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(filter_frame_child), XtRImmediate, (XtPointer) NULL},
    {XtNsizeLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_label_child), XtRWidget, (XtPointer) NULL},
    {XtNsizeTextFieldChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_text_field_child), XtRWidget, (XtPointer) NULL},
    {XtNsizeOptionMenuChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_option_menu_child), XtRWidget, (XtPointer) NULL},
};

/* Forward declarations */

static Boolean SetValues(Widget old, Widget req, Widget new, ArgList args, Cardinal *num_args);
static XtGeometryResult GeometryManager(Widget w, XtWidgetGeometry *desired, XtWidgetGeometry *allowed);
static void Cancel(Widget w);
static void ChangeManaged(Widget w);
static void ClassInitialize(void);
static void ClassPartInitialize(WidgetClass widget_class);
static void ClickAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void Destroy(Widget widget);
static void Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args);
static void Resize(Widget widget);

static XtActionsRec actions[] = {
    {"FSBClickAction", ClickAction}
};

FontSamplerClassRec fontSamplerClassRec = {
    /* Core class part */
  {
    /* superclass	     */	(WidgetClass) &xmManagerClassRec,
    /* class_name	     */ "FontSampler",
    /* widget_size	     */ sizeof(FontSamplerRec),
    /* class_initialize      */ ClassInitialize,
    /* class_part_initialize */ ClassPartInitialize,
    /* class_inited          */	False,
    /* initialize	     */	Initialize,
    /* initialize_hook       */	NULL,
    /* realize		     */	XtInheritRealize,
    /* actions		     */	actions,
    /* num_actions	     */	XtNumber(actions),
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
   /* FontSampler class part */
  {
    /* cancel		     */ Cancel,
    /* extension	     */	NULL,
  }
};

WidgetClass fontSamplerWidgetClass =
	(WidgetClass) &fontSamplerClassRec;

struct _FilterRec;

typedef Boolean (*MatchProc)(String name, struct _FilterRec *filter);

typedef struct _FilterRec {
    char *name;
    char *particles[9];
    MatchProc special;
} FilterRec;

static Boolean MatchRoman(String name, FilterRec *filter);
static Boolean MatchMedium(String name, FilterRec *filter);
static Boolean MatchBlack(String name, FilterRec *filter);

FilterRec filters[] = {
    {"roman",		{"Roman", NULL}, MatchRoman},
    {"italic",		{"Italic", "Kursiv", "Oblique", "Slanted", NULL}},
    {"symbol",		{"Pi", "Symbol", "Logo", "Math", "Ornaments",
				 "Carta", "Sonata", "Dingbats", NULL}},
    {"display",		{"Display", "Titling", NULL}},
    {"alternate",	{"Alternate", NULL}},
    {"expert",		{"Expert", NULL}},
    {"oldstyle",	{"Oldstyle Figures", "Old Style Figures",
				 "Expert", NULL}},
    {"smallcaps",	{"Small Caps", NULL}},
    {"swash",		{"Swash", NULL}},
    {"script",		{"Script", NULL}},
    {"separator1",	{ NULL}},
    {"condensed",	{"Condensed", "Compressed", "Narrow", NULL}},
    {"extended",	{"Extended", NULL}},
    {"separator2",	{ NULL}},
    {"light",		{"Light", "Thin", NULL}},
    {"book",		{"Book", NULL}},
    {"medium",		{"Medium", "Normal", "Regular",
				 "Roman", NULL}, MatchMedium},
    {"demi",		{"Demi", "Semi", "Demibold", "Semibold", NULL}},
    {"bold",		{"Bold", NULL}},
    {"black",		{"Black", "Heavy", "Poster", "Scal",
				 "Ultra", NULL}, MatchBlack},
    {"separator3",	{ NULL}},
    { NULL,		{ NULL}}
};

#define ITALIC_FILTER 1
#define SYMBOL_FILTER 2
#define TYPE_FILTERS 0
#define WIDTH_FILTERS 11
#define WEIGHT_FILTERS 14

static int class_indices[] = {TYPE_FILTERS, WIDTH_FILTERS, WEIGHT_FILTERS, -1};

static void ShowLabel(FontSamplerWidget s, XmString string)
{
    XtVaSetValues(s->sampler.font_label_child, XmNlabelString, string, NULL);
}

static void UnhighlightFont(FontSamplerWidget s)
{
    DisplayedFontRec *d = s->sampler.highlighted_font;

    XCopyArea(XtDisplay(s->sampler.area_child), s->sampler.pixmap,
	      XtWindow(s->sampler.area_child),
	      s->sampler.gc, d->l-1, d->t-1, d->r - d->l + 2, d->b - d->t + 2,
	      d->l-1, d->t-1);
}

static void HighlightFont(FontSamplerWidget s)
{
    DisplayedFontRec *d = s->sampler.highlighted_font;
    FontRec *f = d->font;
    BlendRec *b = d->blend;
    String fontName;
    int bogusFont;

    if (b == NULL) fontName = f->font_name;
    else fontName = b->font_name;

    (void) _FSBDownloadFontIfNecessary(d->font, s->sampler.fsb);

    XDPSSetContextGState(s->sampler.fsb->fsb.context, s->sampler.gstate);
    DPSsetrgbcolor(s->sampler.fsb->fsb.context, 1.0, 0.0, 0.0);
    _DPSFShowText(s->sampler.fsb->fsb.context, d->text->str,
		  fontName, d->text->size, d->x, d->y, &bogusFont);
}

/* ARGSUSED */

static void ClickAction(
    Widget widget,
    XEvent *event,
    String *params,
    Cardinal *num_params)
{
    XButtonEvent *b = (XButtonEvent *) event;
    DisplayedFontRec *f;
    FontSamplerWidget s =
	    (FontSamplerWidget) XtParent(XtParent(XtParent(XtParent(widget))));
    XmString CSname;
    char buf[512];

    if (event->type != ButtonPress) return;

    if (s->sampler.current_display_info == NULL) return;

    f = s->sampler.current_display_info->shown_fonts;

    while (f != NULL &&
	   (b->x < f->l || b->y < f->t || b->x > f->r || b->y > f->b)) {
	f = f->next;
    }

    if (f != NULL) {
	if (s->sampler.highlighted_font == f) return;
	if (s->sampler.highlighted_font != NULL) UnhighlightFont(s);
	s->sampler.highlighted_font = f;
	HighlightFont(s);
	if (f->blend == NULL) CSname = UnsharedCS(f->font->full_name);
	else {
	    sprintf(buf, "%s %s", f->font->full_name, f->blend->blend_name);
	    CSname = UnsharedCS(buf);
	}
	ShowLabel(s, CSname);
	XmStringFree(CSname);
	if (f->blend == NULL) {
	    _FSBSetCurrentFont(s->sampler.fsb, f->font->font_name);
	} else {
	    _FSBSetCurrentFont(s->sampler.fsb, f->blend->font_name);
	}
    }
}
    
static void UpdateDisplayedFontRecs(
    DisplayRecord *info,
    Position newHeight,
    Position oldHeight,
    Position newWidth)
{
    float *m = info->sampler->sampler.invctm;
    float h, w;
    Position oldInfoHeight = info->height;
    DisplayedFontRec *f;

    info->window_height = newHeight;
    h = newHeight;
    w = newWidth;

    info->width = (int) (m[0] * w - m[2] * h + m[4]);
    info->height = (int) (m[1] * w - m[3] * h + m[5]);

    info->y += info->height - oldInfoHeight;

    for (f = info->shown_fonts; f != NULL; f = f->next) {
	f->y += info->height - oldInfoHeight;
    }
}

/* ARGSUSED */

static void ResizeEventHandler(
    Widget widget,
    XtPointer clientData,
    XEvent *event,
    Boolean *continueToDispatch)
{
    Dimension clip_width, clip_height, new_width, new_height,
	    area_width, area_height;
    int depth;
    FontSamplerWidget s = (FontSamplerWidget) clientData;
    Pixmap p;

    if (event->type != ConfigureNotify) return;

    XtVaGetValues(s->sampler.clip_widget, XtNwidth, &clip_width,
		  XtNheight, &clip_height, NULL);
    XtVaGetValues(s->sampler.area_child, XtNwidth, &area_width,
		  XtNheight, &area_height, XtNdepth, &depth, NULL);

    /* Trying to make it fit exactly causes looooping... */

    new_width = clip_width-2;
    new_height = clip_height-2;

    if (clip_width < s->sampler.minimum_width) {
	new_width = s->sampler.minimum_width;
    }
    if (clip_height < s->sampler.minimum_height) {
	new_height = s->sampler.minimum_height;
    }

    if (new_height != area_height || new_width != area_width) {
	XtVaSetValues(s->sampler.area_child, XtNwidth, new_width,
		      XtNheight, new_height, NULL);

	p = XCreatePixmap(XtDisplay(s->sampler.area_child),
			  RootWindowOfScreen(XtScreen(s->sampler.area_child)),
			  new_width, new_height, depth);

	if (s->sampler.gstate != 0) {
	    XDPSSetContextGState(s->sampler.fsb->fsb.context,
				 s->sampler.gstate);
	    XDPSSetContextParameters(s->sampler.fsb->fsb.context,
				 XtScreen(s->sampler.area_child), depth,
				 XtWindow(s->sampler.area_child),
				 new_height,
				 (XDPSStandardColormap *) NULL,
				 (XDPSStandardColormap *) NULL,
				 XDPSContextScreenDepth | XDPSContextDrawable |
				 XDPSContextRGBMap | XDPSContextGrayMap);
	    _DPSFReclip(s->sampler.fsb->fsb.context);
	    _DPSFGetCTM(s->sampler.fsb->fsb.context,
			s->sampler.ctm, s->sampler.invctm);
	    XDPSUpdateContextGState(s->sampler.fsb->fsb.context,
				    s->sampler.gstate);
	    XDPSSetContextGState(s->sampler.fsb->fsb.context,
				 s->sampler.pixmap_gstate);
	    XDPSSetContextParameters(s->sampler.fsb->fsb.context,
				 (Screen *) NULL, 0,
				 p, new_height,
				 (XDPSStandardColormap *) NULL,
				 (XDPSStandardColormap *) NULL,
				 XDPSContextDrawable);
	    XDPSUpdateContextGState(s->sampler.fsb->fsb.context,
				    s->sampler.pixmap_gstate);

	    _DPSFClearWindow(s->sampler.fsb->fsb.context);
	    /* La di dah */
	    DPSWaitContext(s->sampler.fsb->fsb.context);
	    XCopyArea(XtDisplay(s), s->sampler.pixmap, p,
		      s->sampler.gc, 0, 0, new_width, new_height, 0, 0);
	}
	XFreePixmap(XtDisplay(s), s->sampler.pixmap);
	s->sampler.pixmap = p;
	UpdateDisplayedFontRecs(s->sampler.current_display_info,
				new_height, area_height, new_width);
    }
}

static void ClassInitialize(void)
{
    XtInitializeWidgetClass(fontSelectionBoxWidgetClass);
}

static void ClassPartInitialize(WidgetClass widget_class)
{
    register FontSamplerWidgetClass wc =
	    (FontSamplerWidgetClass) widget_class;
    FontSamplerWidgetClass super =
	    (FontSamplerWidgetClass) wc->core_class.superclass;

    if (wc->sampler_class.cancel == InheritCancel) {
	wc->sampler_class.cancel = super->sampler_class.cancel;
    }
}

static void FreeDisplayInfo(DisplayRecord *info)
{
    DisplayedFontRec *f;
    DisplayedTextRec *t;

    if (info == NULL) return;

    XtVaSetValues(info->sampler->sampler.font_label_child,
		  XtVaTypedArg, XmNlabelString, XtRString,
		  " ", 2, NULL);

    while ((f = info->shown_fonts) != NULL) {
	info->shown_fonts = f->next;
	XtFree((char *) f);
    }

    while ((t = info->text_list) != NULL) {
	info->text_list = t->next;
	XtFree((char *) t->str);
	XtFree((char *) t);
    }

    XtFree((char *) info);
}

static Boolean IsSet(Widget widget)
{
    return XmToggleButtonGadgetGetState(widget);
}

/* ARGSUSED */

static void DisplayCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    XtAppContext app;
    float h, w;
    DisplayRecord *info;
    FontSamplerWidget s = (FontSamplerWidget) clientData;
    float *m;
    char *value;
    DisplayedTextRec *t;

    if (s->sampler.current_display_proc != None) {
	XtRemoveWorkProc(s->sampler.current_display_proc);
    }
    FreeDisplayInfo(s->sampler.current_display_info);
    s->sampler.highlighted_font = NULL;

    app = XtDisplayToApplicationContext(XtDisplay(widget));

    info = s->sampler.current_display_info =
	    (DisplayRecord *) XtNew(DisplayRecord);

    XtVaGetValues(s->sampler.area_child,
		  XtNwidth, &info->width,
		  XtNheight, &info->window_height,
		  XtNdepth, &info->depth,
		  NULL);

    if (s->sampler.gstate == 0) {
	XDPSSetContextParameters(s->sampler.fsb->fsb.context,
				 XtScreen(s->sampler.area_child), info->depth,
				 XtWindow(s->sampler.area_child),
				 info->window_height,
				 (XDPSStandardColormap *) NULL,
				 (XDPSStandardColormap *) NULL,
				 XDPSContextScreenDepth | XDPSContextDrawable |
				 XDPSContextRGBMap | XDPSContextGrayMap);
	DPSsetgray(s->sampler.fsb->fsb.context, 0.0);
	XDPSCaptureContextGState(s->sampler.fsb->fsb.context,
				 &s->sampler.gstate);
	_DPSFGetCTM(s->sampler.fsb->fsb.context,
		    s->sampler.ctm, s->sampler.invctm);
	XDPSSetContextParameters(s->sampler.fsb->fsb.context,
				 (Screen *) NULL, 0,
				 s->sampler.pixmap, info->window_height,
				 (XDPSStandardColormap *) NULL,
				 (XDPSStandardColormap *) NULL,
				 XDPSContextDrawable);
	DPSsetgray(s->sampler.fsb->fsb.context, 0.0);
	XDPSCaptureContextGState(s->sampler.fsb->fsb.context,
				 &s->sampler.pixmap_gstate);
    }

    h = info->window_height;
    w = info->width;

    m = s->sampler.invctm;

    info->width = (int) (m[0] * w - m[2] * h + m[4]);
    info->height = (int) (m[1] * w - m[3] * h + m[5]);
    info->sampler = s;
    info->inited = info->any_shown = False;
    info->column_width = 0;
    info->x = 5;
    info->y = info->height;
    info->shown_fonts = NULL;

    t = info->text_list = XtNew(DisplayedTextRec);
    t->next = NULL;
    value = XmTextFieldGetString(s->sampler.text_child);
    t->str = XtNewString(value);

    value = XmTextFieldGetString(s->sampler.size_text_field_child);

    if (value == NULL || *value == '\0') t->size = SAMPLER_DEFAULT_SIZE;
    else {
	t->size = atof(value);
	if (t->size <= 0) t->size = SAMPLER_DEFAULT_SIZE;
    }

    s->sampler.displaying = True;

    XDPSSetContextGState(s->sampler.fsb->fsb.context, s->sampler.gstate);
    _DPSFClearWindow(s->sampler.fsb->fsb.context);
    XDPSSetContextGState(s->sampler.fsb->fsb.context,
			 s->sampler.pixmap_gstate);
    _DPSFClearWindow(s->sampler.fsb->fsb.context);

    XtSetSensitive(s->sampler.stop_button_child, True);

    if (IsSet(s->sampler.all_toggle_child)) {
	s->sampler.current_display_proc =
		XtAppAddWorkProc(app, DisplayAllWorkProc,
				 (XtPointer) info);
    } else if (IsSet(s->sampler.selected_toggle_child)) {
	s->sampler.current_display_proc =
		XtAppAddWorkProc(app, DisplaySelectedWorkProc,
				 (XtPointer) info);
    } else if (IsSet(s->sampler.selected_family_toggle_child)) {
	s->sampler.current_display_proc =
		XtAppAddWorkProc(app, DisplaySelectedFamilyWorkProc,
				 (XtPointer) info);
    } else if (IsSet(s->sampler.filter_toggle_child)) {
	s->sampler.current_display_proc =
		XtAppAddWorkProc(app, DisplayFilteredWorkProc,
				 (XtPointer) info);
    }
}

static void FinishUpDisplaying(FontSamplerWidget s)
{
    XtSetSensitive(s->sampler.stop_button_child, False);
    s->sampler.current_display_proc = None;
}

/* ARGSUSED */

static void FilterCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    FontSamplerWidget s = (FontSamplerWidget) clientData;
    
    s->sampler.filters_changed = True;

    if (IsSet(s->sampler.filter_toggle_child)) return;

    XmToggleButtonGadgetSetState(s->sampler.filter_toggle_child, True, True);

    XmToggleButtonGadgetSetState(s->sampler.all_toggle_child, False, False);
    XmToggleButtonGadgetSetState(s->sampler.selected_toggle_child,
				 False, False);
    XmToggleButtonGadgetSetState(s->sampler.selected_family_toggle_child,
			   False, False);
}

/* ARGSUSED */

static void TextCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    FontSamplerWidget s = (FontSamplerWidget) clientData;
    DisplayedTextRec *t;
    char *value;

    if (!s->sampler.displaying) return;

    t = XtNew(DisplayedTextRec);

    value = XmTextFieldGetString(s->sampler.text_child);
    t->str = XtNewString(value);
    t->size = s->sampler.current_display_info->text_list->size;
    t->next = s->sampler.current_display_info->text_list;
    s->sampler.current_display_info->text_list = t;
}

/* ARGSUSED */

static void StopCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    FontSamplerWidget s = (FontSamplerWidget) clientData;

    if (s->sampler.current_display_proc == None) return;

    XtRemoveWorkProc(s->sampler.current_display_proc);
    FinishUpDisplaying(s);
}

/* ARGSUSED */

static void DismissCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    FontSamplerWidget s = (FontSamplerWidget) clientData;

    if (XtIsShell(XtParent(s))) XtPopdown(XtParent(s));

    if (s->sampler.current_display_proc != None) {
	XtRemoveWorkProc(s->sampler.current_display_proc);
    }
    FinishUpDisplaying(s);

    XtCallCallbackList(widget, s->sampler.dismiss_callback, (XtPointer) NULL);
}

/* ARGSUSED */

static void PopdownCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    FontSamplerWidget s =
	    (FontSamplerWidget)
		    (((CompositeWidget) widget)->composite.children[0]);

    if (s->sampler.current_display_proc != None) {
	XtRemoveWorkProc(s->sampler.current_display_proc);
    }
}    

/* ARGSUSED */

static void ExposeCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    XmDrawingAreaCallbackStruct *da = (XmDrawingAreaCallbackStruct *) callData;
    XExposeEvent *ev = (XExposeEvent *) da->event;
    FontSamplerWidget s = (FontSamplerWidget) clientData;

    if (ev->type != Expose || !s->sampler.displaying) return;

    XCopyArea(XtDisplay(widget), s->sampler.pixmap, XtWindow(widget),
	      s->sampler.gc, ev->x, ev->y, ev->width, ev->height,
	      ev->x, ev->y);
    if (s->sampler.highlighted_font != NULL) HighlightFont(s);
}

/* ARGSUSED */

static void ClearCallback(Widget widget, XtPointer clientData, XtPointer callData)
{
    int j;
    FontSamplerWidget s = (FontSamplerWidget) clientData;

    for (j = 0; filters[j].name != NULL; j++) {
	if (filters[j].particles[0] != NULL) {
	    XmToggleButtonGadgetSetState(s->sampler.filter_widgets[j],
					 False, False);
	}
    }

    XmTextFieldSetString(s->sampler.filter_text_child, "");
}

/* ARGSUSED */

static void SizeSelect(Widget widget, XtPointer clientData, XtPointer callData)
{
    FontSamplerWidget s = (FontSamplerWidget) clientData;
    String value;
    Widget option;
    char *ch;
    DisplayedTextRec *t;

    value = XmTextFieldGetString(widget);

    if (value == NULL) option = s->sampler.other_size;
    else {
	for (ch = value; *ch != '\0'; ch++) if (*ch == '.') *ch = '-';

	option = XtNameToWidget(s->sampler.size_menu, value);
	if (option == NULL) option = s->sampler.other_size;
    }

    XtVaSetValues(s->sampler.size_option_menu_child,
		  XmNmenuHistory, option, NULL);

    if (!s->sampler.displaying) return;

    t = XtNew(DisplayedTextRec);

    t->str = XtNewString(s->sampler.current_display_info->text_list->str);
    if (value == NULL || *value == '\0') t->size = SAMPLER_DEFAULT_SIZE;
    else {
	t->size = atof(value);
	if (t->size <= 0) t->size = SAMPLER_DEFAULT_SIZE;
    }
    t->next = s->sampler.current_display_info->text_list;
    s->sampler.current_display_info->text_list = t;
}

/* There's a problem; sometimes the change has already been made in the field,
   and sometimes it hasn't.  The times when it has seem to correspond to
   making changes with the size option menu, so we use this disgusting
   global flag to notice when this happens.  */

static Boolean changingSize = False;

/* ARGSUSED */

static void TextVerify(Widget widget, XtPointer clientData, XtPointer callData)
{
    int i;
    XmTextVerifyPtr v = (XmTextVerifyPtr) callData;
    char ch, *cp;
    int decimalPoints = 0;
    
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

    cp = XmTextFieldGetString(widget);

    for (/**/; *cp != '\0'; cp++) {
	if (*cp == '.') decimalPoints++;
    }

    if (decimalPoints > 1) v->doit = False;
}

/* ARGSUSED */

static void SetSize(Widget widget, XtPointer clientData, XtPointer callData)
{
    char buf[20], *ch;
    FontSamplerWidget s = (FontSamplerWidget) clientData;

    strcpy(buf, XtName(widget));
    for (ch = buf; *ch != '\0'; ch++) if (*ch == '-') *ch++ = '.';

    changingSize = True;
    XmTextFieldSetString(s->sampler.size_text_field_child, buf);
    changingSize = False;
}

static void CreateSizeMenu(FontSamplerWidget s, Boolean destroyOldChildren)
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
	XtVaGetValues(s->sampler.size_menu, XtNchildren, &children,
		    XtNnumChildren, &num_children, NULL);

	/* Don't destroy first child ("other") */
	for (j = 1; (Cardinal)j < num_children; j++) XtDestroyWidget(children[j]);

	sizes = (Widget *) XtMalloc((s->sampler.size_count+1) *
				    sizeof(Widget));
	sizes[0] = children[0];
    } else {
	i = 0;
	sizes = (Widget *) XtMalloc((s->sampler.size_count+1) *
				    sizeof(Widget));
	s->sampler.other_size = sizes[0] =
		XtCreateManagedWidget("other", xmPushButtonGadgetClass,
				      s->sampler.size_menu, args, i);
    }

    for (j = 0; j < s->sampler.size_count; j++) {
	(void) sprintf(buf, "%g", s->sampler.sizes[j]);
	csName = UnsharedCS(buf);
	for (ch = buf; *ch != '\0'; ch++) if (*ch == '.') *ch = '-';
	i = 0;
	XtSetArg(args[i], XmNlabelString, csName);			i++;
	sizes[j+1] =
		XmCreatePushButtonGadget(s->sampler.size_menu, buf, args, i);
	XmStringFree(csName);
	XtAddCallback(sizes[j+1], XmNactivateCallback, SetSize, (XtPointer) s);
    }
    XtManageChildren(sizes, j+1);
    XtFree((char *) sizes);
}

static void CreateFilters(FontSamplerWidget s)
{
    FilterRec *f;
    int i;

    s->sampler.filter_widgets =
	    (Widget *) XtCalloc(XtNumber(filters)-1, sizeof(Widget));

    s->sampler.filter_flags =
	    (Boolean *) XtCalloc(XtNumber(filters)-1, sizeof(Boolean));

    for (i = 0; filters[i].name != NULL; i++) {
	f = filters+i;
	if (f->particles[0] == NULL) {
	    s->sampler.filter_widgets[i] =
		    XtCreateManagedWidget(f->name, xmSeparatorGadgetClass,
					  s->sampler.filter_box_child,
					  (ArgList) NULL, 0);
	} else {
	    s->sampler.filter_widgets[i] =
		    XtCreateManagedWidget(f->name, xmToggleButtonGadgetClass,
					  s->sampler.filter_box_child,
					  (ArgList) NULL, 0);
	    XtAddCallback(s->sampler.filter_widgets[i],
			  XmNvalueChangedCallback,
			  FilterCallback, (XtPointer) s);
	}
    }
}

static void CreateChildren(FontSamplerWidget s)
{
    Arg args[20];
    int i;
    Widget form;
    Dimension area_width, area_height;
    int depth;
    Widget w, rowcol;

    form = s->sampler.panel_child =
	    XtCreateManagedWidget("panel", xmFormWidgetClass,
				  (Widget) s, (ArgList) NULL, 0);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    s->sampler.display_button_child =
	    XtCreateManagedWidget("displayButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(s->sampler.display_button_child, XmNactivateCallback,
		  DisplayCallback,  (XtPointer) s);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, s->sampler.display_button_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XtNsensitive, False);				i++;
    s->sampler.stop_button_child =
	    XtCreateManagedWidget("stopButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(s->sampler.stop_button_child, XmNactivateCallback,
		  StopCallback, (XtPointer) s);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, s->sampler.stop_button_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    s->sampler.dismiss_button_child =
	    XtCreateManagedWidget("dismissButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(s->sampler.dismiss_button_child, XmNactivateCallback,
		  DismissCallback, (XtPointer) s);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, s->sampler.dismiss_button_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    s->sampler.size_label_child =
	    XtCreateManagedWidget("sizeLabel", xmLabelWidgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, s->sampler.size_label_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, s->sampler.size_label_child);	i++;
    s->sampler.size_text_field_child =
	    XtCreateManagedWidget("sizeTextField",
				  xmTextFieldWidgetClass,
				  form, args, i);
    XtAddCallback(s->sampler.size_text_field_child, XmNvalueChangedCallback,
		  SizeSelect, (XtPointer) s);
    XtAddCallback(s->sampler.size_text_field_child, XmNmodifyVerifyCallback,
		  TextVerify, (XtPointer) NULL);

    i = 0;
    s->sampler.size_menu = XmCreatePulldownMenu(form, "sizeMenu", args, i);

    CreateSizeMenu(s, False);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, s->sampler.size_text_field_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, s->sampler.size_label_child);	i++;
    XtSetArg(args[i], XmNsubMenuId, s->sampler.size_menu);		i++;
    s->sampler.size_option_menu_child =
	    XmCreateOptionMenu(form, "sizeOptionMenu", args, i);
    XtManageChild(s->sampler.size_option_menu_child);

    SizeSelect(s->sampler.size_text_field_child, (XtPointer) s,
	       (XtPointer) NULL);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);			i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    rowcol = XtCreateManagedWidget("rowColumn", xmRowColumnWidgetClass,
				   form, args, i);

    i = 0;
    s->sampler.radio_frame_child =
	    XtCreateManagedWidget("radioFrame", xmFrameWidgetClass,
				  rowcol, args, i);

    i = 0;
    s->sampler.radio_box_child = XmCreateRadioBox(s->sampler.radio_frame_child,
						  "radioBox", args, i);
    XtManageChild(s->sampler.radio_box_child);

    i = 0;
    s->sampler.all_toggle_child =
	    XtCreateManagedWidget("allToggle", xmToggleButtonGadgetClass,
				  s->sampler.radio_box_child, args, i);

    i = 0;
    s->sampler.selected_toggle_child =
	    XtCreateManagedWidget("selectedToggle", xmToggleButtonGadgetClass,
				  s->sampler.radio_box_child, args, i);

    i = 0;
    s->sampler.selected_family_toggle_child =
	    XtCreateManagedWidget("selectedFamilyToggle",
				  xmToggleButtonGadgetClass,
				  s->sampler.radio_box_child, args, i);

    i = 0;
    s->sampler.filter_toggle_child =
	    XtCreateManagedWidget("filterToggle",
				  xmToggleButtonGadgetClass,
				  s->sampler.radio_box_child, args, i);

    i = 0;
    s->sampler.filter_frame_child =
	    XtCreateManagedWidget("filterFrame", xmFrameWidgetClass,
				  rowcol, args, i);

    i = 0;
    s->sampler.filter_box_child =
	    XtCreateManagedWidget("filterBox", xmRowColumnWidgetClass,
				  s->sampler.filter_frame_child, args, i);

    CreateFilters(s);

    i = 0;
    s->sampler.filter_text_child =
	    XtCreateManagedWidget("filterText", xmTextFieldWidgetClass,
				  s->sampler.filter_box_child, args, i);
    XtAddCallback(s->sampler.filter_text_child,
		  XmNvalueChangedCallback,
		  FilterCallback, (XtPointer) s);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, rowcol);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, rowcol);				i++;
    s->sampler.clear_button_child =
	    XtCreateManagedWidget("clearButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(s->sampler.clear_button_child, XmNactivateCallback,
		  ClearCallback, (XtPointer) s);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);			i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, rowcol);				i++;
    s->sampler.text_child =
	    XtCreateManagedWidget("text", xmTextFieldWidgetClass,
				  form, args, i);
    XtAddCallback(s->sampler.text_child,
		  XmNvalueChangedCallback,
		  TextCallback, (XtPointer) s);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, s->sampler.text_child);		i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNleftWidget, s->sampler.text_child);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNrightWidget, s->sampler.text_child);		i++;
    s->sampler.font_label_child =
	    XtCreateManagedWidget("fontLabel", xmLabelGadgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, s->sampler.font_label_child);	i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, rowcol);				i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, s->sampler.display_button_child);i++;
    XtSetArg(args[i], XmNscrollingPolicy, XmAUTOMATIC);			i++;
    s->sampler.scrolled_window_child =
	    XtCreateManagedWidget("scrolledWindow",
				  xmScrolledWindowWidgetClass,
				  form, args, i);
    i = 0;
    s->sampler.area_child =
	    XtCreateManagedWidget("area", xmDrawingAreaWidgetClass,
				  s->sampler.scrolled_window_child, args, i);
    XtAddCallback(s->sampler.area_child, XmNexposeCallback,
		  ExposeCallback, (XtPointer) s);

    XtVaGetValues(s->sampler.scrolled_window_child,
		  XmNclipWindow, &s->sampler.clip_widget,
		  NULL);

    /* I would like to use translations for this, but Motif overwrites the
       clip window's translation.  Grr... */

    XtAddEventHandler(s->sampler.clip_widget, StructureNotifyMask, False,
		      ResizeEventHandler, (XtPointer) s);

    XtVaSetValues(s->sampler.scrolled_window_child,
		  XmNworkWindow, s->sampler.area_child, NULL);
    
    XtVaGetValues(s->sampler.area_child,
		  XtNheight, &area_height,
		  XtNwidth, &area_width,
		  XtNdepth, &depth,
		  NULL);

    if (area_height < s->sampler.minimum_height ||
	area_width < s->sampler.minimum_width) {
	area_height = MAX(area_height, s->sampler.minimum_height);
	area_width = MAX(area_width, s->sampler.minimum_width);

	XtVaSetValues(s->sampler.area_child, XtNwidth, area_width,
		      XtNheight, area_height, NULL);
    }

    s->sampler.pixmap =
	    XCreatePixmap(XtDisplay(s->sampler.area_child),
			  RootWindowOfScreen(XtScreen(s->sampler.area_child)),
			  area_width, area_height, depth);

    XtVaSetValues(form, XmNdefaultButton, s->sampler.display_button_child,
		  NULL);

    s->sampler.gc = XtGetGC(s->sampler.area_child, 0, (XGCValues *) NULL);

    for (w = XtParent(s); !XtIsShell(w); w = XtParent(w)) {}
    XtAddCallback(w, XtNpopdownCallback, PopdownCallback, (XtPointer) NULL);
}

/* ARGSUSED */

static void Initialize(
    Widget request, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    FontSamplerWidget sampler = (FontSamplerWidget) new;

    /* Must have a fsb */

    if (sampler->sampler.fsb == NULL) {
	XtAppErrorMsg(XtWidgetToApplicationContext(new),
		      "initializeFontSampler", "noFontSelectionBox",
		      "FontSelectionBoxError",
		      "No font selection box given to font sampler",
		      (String *) NULL, (Cardinal *) NULL);
    }

    /* Verify size list */

    if (sampler->sampler.size_count > 0 && sampler->sampler.sizes == NULL) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontSampler", "sizeMismatch",
			"FontSelectionBoxError",
			"Size count specified but no sizes present",
			(String *) NULL, (Cardinal *) NULL);
	sampler->sampler.size_count = 0;
    }

    if (sampler->sampler.size_count < 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontSampler", "negativeSize",
			"FontSelectionBoxError",
			"Size count should not be negative",
			(String *) NULL, (Cardinal *) NULL);
	sampler->sampler.size_count = 0;
    }

    /* Initialize non-resource fields */

    sampler->sampler.displaying = False;
    sampler->sampler.current_display_proc = None;
    sampler->sampler.current_display_info = NULL;
    sampler->sampler.gstate = sampler->sampler.pixmap_gstate = 0;

    CreateChildren(sampler);
}

static void AdvanceInfoToNextFont(DisplayRecord *info)
{	
    if (info->current_font->blend_data != NULL) {
	if (info->current_blend == NULL) {
	    info->current_blend = info->current_font->blend_data->blends;
	} else info->current_blend = info->current_blend->next;
	if (info->current_blend == NULL) {
	    info->current_font = info->current_font->next;
	}
    } else info->current_font = info->current_font->next;

    if (info->current_font == NULL) {
	info->current_family = info->current_family->next;
	if (info->current_family != NULL) {
	    info->current_font = info->current_family->fonts;
	}
    }
}

static Boolean ShowFont(DisplayRecord *info)
{
    float width, left, right, top, bottom;
    FontRec *f = info->current_font;
    BlendRec *b = info->current_blend;
    DisplayedFontRec *d;
    FontSamplerWidget s = info->sampler;
    float *m;
    DisplayedTextRec *t = info->text_list;
    String fontName;
    int bogusFont;
    int oldx, oldy;

    if (f == NULL) return True;

    oldx = info->x;
    oldy = info->y;

    info->y -= t->size * 5 / 4;

    if (info->y < 0) {
	if (info->column_width == 0) return False;
	info->y = info->height - (t->size * 5 / 4);
	info->x += info->column_width + (t->size / 4);
	if (info->x > (int) info->width) return False;
	info->column_width = 0;
    }

    if (!_FSBDownloadFontIfNecessary(f, s->sampler.fsb)) {
	AdvanceInfoToNextFont(info);
	return True;
    }

    if (b == NULL) fontName = f->font_name;
    else fontName = b->font_name;

    /* Do ...AndGetDimensions on the pixmap to make sure that it's synced.
       That way we can reliably do an XCopyArea without first doing a
       WaitContext.  */

    XDPSSetContextGState(s->sampler.fsb->fsb.context, s->sampler.gstate);
    _DPSFShowText(s->sampler.fsb->fsb.context, t->str, fontName,
		  t->size, info->x, info->y, &bogusFont);

    AdvanceInfoToNextFont(info);

    if (bogusFont) {
	info->x = oldx;
	info->y = oldy;

	XCopyArea(XtDisplay(s), s->sampler.pixmap,
		  XtWindow(s->sampler.area_child),
		  s->sampler.gc, 0, 0, info->width, info->height, 0, 0);

	if (info->current_font == f) {
	    /* Must be the same font, different blend */
	    info->current_font = info->current_font->next;

	    if (info->current_font == NULL) {
		info->current_family = info->current_family->next;
		if (info->current_family != NULL) {
		    info->current_font = info->current_family->fonts;
		}
	    }
	}

	_FSBFlushFont(s->sampler.fsb, f);
	return True;
    }

    XDPSSetContextGState(s->sampler.fsb->fsb.context,
			 s->sampler.pixmap_gstate);
    _DPSFShowTextAndGetDimensions(s->sampler.fsb->fsb.context,
				  t->str, fontName,
				  t->size, info->x, info->y,
				  &width, &left, &right, &top, &bottom);

    width = ceil(width);

    if (width > (int) info->column_width) info->column_width = (int) width;

    d = XtNew(DisplayedFontRec);

    m = s->sampler.ctm;

    d->l = (int) (m[0] * left + m[2] * top + m[4]);
    d->r = (int) ceil(m[0] * right + m[2] * bottom + m[4]);
    d->t = (int) ceil(m[1] * left + m[3] * top + m[5] + info->window_height);
    d->b = (int) (m[1] * right + m[3] * bottom + m[5] + info->window_height);
    d->x = info->x;
    d->y = info->y;
    d->font = f;
    d->blend = b;
    d->text = info->text_list;
    d->next = info->shown_fonts;
    info->shown_fonts = d;

    return True;
}

static Boolean DisplayAllWorkProc(XtPointer client_data)
{
    DisplayRecord *info = (DisplayRecord *) client_data;
    FontSamplerWidget s = info->sampler;

    if (!info->inited) {
	info->inited = True;
	info->current_family = s->sampler.fsb->fsb.known_families;
	info->current_font = info->current_family->fonts;
	info->current_blend = NULL;
    }

    if (!ShowFont(info)) {
	if (!info->any_shown) ShowLabel(s, s->sampler.no_room_message);
	FinishUpDisplaying(s);
	return True;
    }
    
    info->any_shown = True;
    if (info->current_family == NULL) {
	if (!info->any_shown) ShowLabel(s, s->sampler.no_font_message);
	FinishUpDisplaying(s);
	return True;
    }
    return False;
}

static Boolean DisplaySelectedWorkProc(XtPointer client_data)
{
    DisplayRecord *info = (DisplayRecord *) client_data;
    FontSamplerWidget s = info->sampler;

    info->current_family = s->sampler.fsb->fsb.currently_selected_family;
    info->current_font = s->sampler.fsb->fsb.currently_selected_face;
    info->current_blend = s->sampler.fsb->fsb.currently_selected_blend;

    if (info->current_font != NULL) {
	if (!ShowFont(info)) ShowLabel(s, s->sampler.no_room_message);
    } else ShowLabel(s, s->sampler.no_selected_font_message);

    FinishUpDisplaying(s);
    return True;
}

static Boolean DisplaySelectedFamilyWorkProc(XtPointer client_data)
{
    DisplayRecord *info = (DisplayRecord *) client_data;
    FontSamplerWidget s = info->sampler;
    FontFamilyRec *currentFamily;

    if (!info->inited) {
	info->inited = True;
	info->current_family = s->sampler.fsb->fsb.currently_selected_family;
	if (info->current_family != NULL) {
	    info->current_font = info->current_family->fonts;
	    info->current_blend = NULL;
	} else {
	    ShowLabel(s, s->sampler.no_selected_family_message);
	    FinishUpDisplaying(s);
	    return True;
	}
    }

    currentFamily = info->current_family;

    if (!ShowFont(info)) {
	if (!info->any_shown) ShowLabel(s, s->sampler.no_room_message);
	FinishUpDisplaying(s);
	return True;
    }
    
    info->any_shown = True;
    if (info->current_family != currentFamily) {
	if (!info->any_shown) ShowLabel(s, s->sampler.no_family_font_message);
	FinishUpDisplaying(s);
	return True;
    }
    return False;
}

/* ARGSUSED */

static Boolean MatchRoman(String name, FilterRec *filter)
{
    FilterRec *f;
    char *ch, **search, *start;
    int len;
    
    /* Roman means not italic and not symbol */

    for (f = filters + ITALIC_FILTER; f <= filters + SYMBOL_FILTER; f++) {
	for (search = f->particles; *search != NULL; search++) {
	    start = name;
	    do {
		ch = strstr(start, *search);
		if (ch != NULL) {
		    len = strlen(*search);
		    if (ch[len] == ' ' || ch[len] == '\0') return False;
		    else start = ch+1;
		}
	    } while (ch != NULL);
	}
    }
    return True;
}

static Boolean MatchMedium(String name, FilterRec *filter)
{
    FilterRec *f;
    char *ch, **search, *start;
    int len;

    for (search = filter->particles; *search != NULL; search++) {
	start = name;
	do {
	    ch = strstr(start, *search);
	    if (ch != NULL) {
		len = strlen(*search);
		if (ch[len] == ' ' || ch[len] == '\0') return True;
		else start = ch+1;
	    }
	} while (ch != NULL);
    }

    /* Also match anything that has none of the other weight particles */

    for (f = filters + WEIGHT_FILTERS; f->name != NULL; f++) {
	if (f == filter) continue;
	for (search = f->particles; *search != NULL; search++) {
	    start = name;
	    do {
		ch = strstr(start, *search);
		if (ch != NULL) {
		    len = strlen(*search);
		    if (ch[len] == ' ' || ch[len] == '\0') return False;
		    else start = ch+1;
		}
	    } while (ch != NULL);
	}
    }
    return True;
}

static Boolean MatchBlack(String name, FilterRec *filter)
{
    char *ch, **search, *start;
    int len;
    Boolean ultra;

    for (search = filter->particles; *search != NULL; search++) {
	ultra = (strcmp(*search, "Ultra") == 0);
	start = name;
	do {
	    ch = strstr(start, *search);
	    if (ch != NULL) {
		len = strlen(*search);
		if (ch[len] == '\0') return True;
		if (ch[len] == ' ') {
		    if (!ultra) return True;
		    /* Only match "Ultra" if not followed by "Compressed" or
		       "Light".  We'd also like to add "Condensed" to this
		       list, but some fonts use "Ultra Condensed" to mean
		       "Ultra & Condensed" while others use it to mean "Very
		       much Condensed".  Sigh... */
		    start = ch+len+1;
		    if (strncmp(start, "Compressed", 10) != 0 &&
			strncmp(start, "Light", 5) != 0) return True;
		    else start = ch+1;
		}
		else start = ch+1;
	    }
	} while (ch != NULL);
    }
    return False;
}

static void UpdateFilters(FontSamplerWidget s)
{
    int i;

    for (i = 0; filters[i].name != NULL; i++) {
	if (filters[i].particles[0] != NULL) {
	    s->sampler.filter_flags[i] = IsSet(s->sampler.filter_widgets[i]);
	}
    }

    s->sampler.filter_text =
	    XmTextFieldGetString(s->sampler.filter_text_child);
}

static Boolean FontMatchesFilters(
    FontRec *font,
    BlendRec *blend,
    FontSamplerWidget s)
{
    int *cl, i;
    FilterRec *f;
    char *ch, **search, *start;
    int len;
    Boolean anyset, foundone, allmatch;    
    char *name;
    char buf[512];

    if (blend != NULL) {
	sprintf(buf, "%s %s", font->full_name, blend->blend_name);
	name = buf;
    } else name = font->full_name;

    allmatch = False;
    if (s->sampler.filters_changed) UpdateFilters(s);

    for (cl = class_indices; *cl != -1; cl++) {
	anyset = foundone = False;
	for (i = *cl;
	     filters[i].particles[0] != NULL && filters[i].name != NULL; i++) {

	    f = filters+i;

	    if (!s->sampler.filter_flags[i]) continue;
	    anyset = True;

	    if (f->special != NULL) {
		if ((*f->special)(name, f)) {
		    foundone = True;
		    goto NEXT_CLASS;
		}
		continue;
	    }

	    for (search = f->particles; *search != NULL; search++) {
		start = name;
		do {
		    ch = strstr(start, *search);
		    if (ch != NULL) {
			len = strlen(*search);
			if (ch[len] == ' ' || ch[len] == '\0') {
			    foundone = True;
			    goto NEXT_CLASS;
			}
			else start = ch+1;
		    }
		} while (ch != NULL);
	    }
	}
NEXT_CLASS: ;
	/* If there were any filters set in this class, but we didn't match,
	   return False */
	if (anyset && !foundone) return False;
	if (anyset && foundone) allmatch = True;
    }

    /* Now check against the text field */

    if (s->sampler.filter_text == NULL || s->sampler.filter_text[0] == '\0') {
	return allmatch;
    }

    ch = strstr(name, s->sampler.filter_text);

    return (ch != NULL);
}

static Boolean DisplayFilteredWorkProc(XtPointer client_data)
{
    DisplayRecord *info = (DisplayRecord *) client_data;
    FontSamplerWidget s = info->sampler;

    if (!info->inited) {
	info->inited = True;
	info->current_family = s->sampler.fsb->fsb.known_families;
	info->current_font = info->current_family->fonts;
	info->current_blend = NULL;
	s->sampler.filters_changed = True;
    }

    if (FontMatchesFilters(info->current_font, info->current_blend, s)) {
	if (!ShowFont(info)) {
	    if (!info->any_shown) ShowLabel(s, s->sampler.no_room_message);
	    FinishUpDisplaying(s);
	    return True;
	}
	info->any_shown = True;
    } else AdvanceInfoToNextFont(info);
    
    if (info->current_font == NULL) {
	if (!info->any_shown) ShowLabel(s, s->sampler.no_match_message);
	FinishUpDisplaying(s);
	return True;
    }
    return False;
}

static void Destroy(Widget widget)
{
    FontSamplerWidget s = (FontSamplerWidget) widget;

    if (s->sampler.gstate != 0) {
	XDPSFreeContextGState(s->sampler.fsb->fsb.context,
			      s->sampler.pixmap_gstate);
	XDPSFreeContextGState(s->sampler.fsb->fsb.context, s->sampler.gstate);
    }
    XtReleaseGC(widget, s->sampler.gc);
    XFreePixmap(XtDisplay(widget), s->sampler.pixmap);
    if (s->sampler.current_display_proc != None) {
	XtRemoveWorkProc(s->sampler.current_display_proc);
    }
    if (s->sampler.current_display_info != NULL) {
	FreeDisplayInfo(s->sampler.current_display_info);
    }
    XtFree((char *) s->sampler.filter_widgets);
    XtFree((char *) s->sampler.filter_flags);
}

static void Resize(Widget widget)
{
    FontSamplerWidget s = (FontSamplerWidget) widget;

    XtResizeWidget(s->sampler.panel_child, s->core.width, s->core.height, 0);
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
    FontSamplerWidget s = (FontSamplerWidget) w;

    w->core.width = s->composite.children[0]->core.width;
    w->core.height = s->composite.children[0]->core.height;
}

/* ARGSUSED */

static Boolean SetValues(
    Widget old, Widget req, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    FontSamplerWidget olds = (FontSamplerWidget) old;
    FontSamplerWidget news = (FontSamplerWidget) new;

#define NE(field) news->sampler.field != olds->sampler.field
#define DONT_CHANGE(field) \
    if (NE(field)) news->sampler.field = olds->sampler.field;

    DONT_CHANGE(panel_child);
    DONT_CHANGE(area_child);
    DONT_CHANGE(text_child);
    DONT_CHANGE(font_label_child);
    DONT_CHANGE(scrolled_window_child);
    DONT_CHANGE(display_button_child);
    DONT_CHANGE(dismiss_button_child);
    DONT_CHANGE(stop_button_child);
    DONT_CHANGE(clear_button_child);
    DONT_CHANGE(radio_frame_child);
    DONT_CHANGE(radio_box_child);
    DONT_CHANGE(all_toggle_child);
    DONT_CHANGE(selected_toggle_child);
    DONT_CHANGE(selected_family_toggle_child);
    DONT_CHANGE(filter_toggle_child);
    DONT_CHANGE(filter_box_child);
    DONT_CHANGE(filter_frame_child);
    DONT_CHANGE(size_option_menu_child);
    DONT_CHANGE(size_text_field_child);
    DONT_CHANGE(size_label_child);
    DONT_CHANGE(fsb);
#undef DONT_CHANGE

    if (news->sampler.size_count > 0 && news->sampler.sizes == NULL) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontSampler", "sizeMismatch",
			"FontSelectionBoxError",
			"Size count specified but no sizes present",
			(String *) NULL, (Cardinal *) NULL);
	news->sampler.size_count = 0;
    }

    if (news->sampler.size_count < 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontSampler", "negativeSize",
			"FontSelectionBoxError",
			"Size count should not be negative",
			(String *) NULL, (Cardinal *) NULL);
	news->sampler.size_count = 0;
    }

    if (NE(sizes)) CreateSizeMenu(news, True);

    return False;
#undef NE
}

static void Cancel(Widget w)
{
    FontSamplerWidget s = (FontSamplerWidget) w;

    if (s->sampler.current_display_proc != None) {
	XtRemoveWorkProc(s->sampler.current_display_proc);
    }
}    

void FSBCancelSampler(Widget w)
{
    XtCheckSubclass(w, fontSamplerWidgetClass, NULL);

    (*((FontSamplerWidgetClass) XtClass(w))->sampler_class.cancel) (w);
}

#ifdef NO_STRSTR_AVAILABLE
String strstr(String s1, String s2)
{
    register int len1, len2;

    len1 = strlen(s1);
    len2 = strlen(s2);

    while (len1 >= len2) {
	if (*s1 == *s2) {
	    if (strncmp(s1+1, s2+1, len2-1) == 0) return s1;
	}
	len1--;
	s1++;
    }
}
#endif /* NO_STRSTR_AVAILABLE */
