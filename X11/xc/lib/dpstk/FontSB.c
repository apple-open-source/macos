/* 
 * FontSB.c
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
/* $XFree86: xc/lib/dpstk/FontSB.c,v 1.2 2000/06/07 22:02:59 tsi Exp $ */

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
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
#include <Xm/MessageB.h>
#include <DPS/dpsXclient.h>
#include "dpsXcommonI.h"
#include <DPS/dpsXcommon.h>
#include <DPS/dpsXshare.h>
#include <DPS/PSres.h>
#include <DPS/FontSBP.h>
#include "FSBwraps.h"
#include "FontSBI.h"
#include <DPS/FontSample.h>
#include <DPS/FontCreato.h>
#include <pwd.h>

#define PATH_BUF_SIZE 1024

/* Turn a string into a compound string */
#define UnsharedCS(str) XmStringCreate(str, XmSTRING_DEFAULT_CHARSET)
#define CS(str, w) _FSBCreateSharedCS(str, w)
static XmString CSempty;

/* Create a canonical representation of a string, and as a side effect
   make sure the string is in permanent storage.  This implementation may
   not work under all Xlibs */

#define Canonical(str) XrmQuarkToString(XrmStringToQuark(str))

static float defaultSizeList[] = {
#ifndef DEFAULT_SIZE_LIST
    8, 10, 12, 14, 16, 18, 24, 36, 48, 72
#else
	DEFAULT_SIZE_LIST
#endif /* DEFAULT_SIZE_LIST */
};

#ifndef DEFAULT_SIZE_LIST_COUNT
#define DEFAULT_SIZE_LIST_COUNT 10
#endif /* DEFAULT_SIZE_LIST_COUNT */

#ifndef DEFAULT_SIZE
static float default_size = 12.0;
#else
static float default_size = DEFAULT_SIZE
#endif /* DEFAULT_SIZE */

#ifndef DEFAULT_RESOURCE_PATH
#define DEFAULT_RESOURCE_PATH NULL
#endif /* DEFAULT_RESOURCE_PATH */

#ifndef DEFAULT_MAX_PENDING
#define DEFAULT_MAX_PENDING 10
#endif /* DEFAULT_MAX_PENDING */

#define Offset(field) XtOffsetOf(FontSelectionBoxRec, fsb.field)

static XtResource resources[] = {
    {XtNcontext, XtCContext, XtRDPSContext, sizeof(DPSContext),
	Offset(context), XtRDPSContext, (XtPointer) NULL},
    {XtNpreviewString, XtCPreviewString, XtRString, sizeof(String),
	Offset(preview_string), XtRString, (XtPointer) NULL},
    {XtNsizes, XtCSizes, XtRFloatList, sizeof(float*),
	Offset(sizes), XtRImmediate, (XtPointer) defaultSizeList},
    {XtNsizeCount, XtCSizeCount, XtRInt, sizeof(int),
	Offset(size_count), XtRImmediate, (XtPointer) DEFAULT_SIZE_LIST_COUNT},
    {XtNdefaultResourcePath, XtCDefaultResourcePath, XtRString, sizeof(String),
	Offset(default_resource_path), XtRImmediate,
	(XtPointer) DEFAULT_RESOURCE_PATH},
    {XtNresourcePathOverride, XtCResourcePathOverride,
	XtRString, sizeof(String),
	Offset(resource_path_override), XtRString, (XtPointer) NULL},
    {XtNuseFontName, XtCUseFontName, XtRBoolean, sizeof(Boolean),
	Offset(use_font_name), XtRImmediate, (XtPointer) True},
    {XtNfontName, XtCFontName, XtRString, sizeof(String),
	Offset(font_name), XtRString, (XtPointer) NULL},
    {XtNfontFamily, XtCFontFamily, XtRString, sizeof(String),
	Offset(font_family), XtRString, (XtPointer) NULL},
    {XtNfontFace, XtCFontFace, XtRString, sizeof(String),
	Offset(font_face), XtRString, (XtPointer) NULL},
    {XtNfontBlend, XtCFontBlend, XtRString, sizeof(String),
	Offset(font_blend), XtRString, (XtPointer) NULL},
    {XtNfontSize, XtCFontSize, XtRFloat, sizeof(String),
	Offset(font_size), XtRFloat, (XtPointer) &default_size},
    {XtNfontNameMultiple, XtCFontNameMultiple, XtRBoolean, sizeof(Boolean),
	Offset(font_name_multiple), XtRImmediate, (XtPointer) False},
    {XtNfontFamilyMultiple, XtCFontFamilyMultiple, XtRBoolean, sizeof(Boolean),
	Offset(font_family_multiple), XtRImmediate, (XtPointer) False},
    {XtNfontFaceMultiple, XtCFontFaceMultiple, XtRBoolean, sizeof(Boolean),
	Offset(font_face_multiple), XtRImmediate, (XtPointer) False},
    {XtNfontSizeMultiple, XtCFontSizeMultiple, XtRBoolean, sizeof(Boolean),
	Offset(font_size_multiple), XtRImmediate, (XtPointer) False},
    {XtNgetServerFonts, XtCGetServerFonts, XtRBoolean, sizeof(Boolean),
	Offset(get_server_fonts), XtRImmediate, (XtPointer) True},
    {XtNgetAFM, XtCGetAFM, XtRBoolean, sizeof(Boolean),
	Offset(get_afm), XtRImmediate, (XtPointer) False},
    {XtNautoPreview, XtCAutoPreview, XtRBoolean, sizeof(Boolean),
	Offset(auto_preview), XtRImmediate, (XtPointer) True},
    {XtNpreviewOnChange, XtCPreviewOnChange, XtRBoolean, sizeof(Boolean),
	Offset(preview_on_change), XtRImmediate, (XtPointer) True},
    {XtNundefUnusedFonts, XtCUndefUnusedFonts, XtRBoolean, sizeof(Boolean),
	Offset(undef_unused_fonts), XtRImmediate, (XtPointer) True},
    {XtNmaxPendingDeletes, XtCMaxPendingDeletes, XtRCardinal, sizeof(Cardinal),
	Offset(max_pending_deletes), XtRImmediate,
	(XtPointer) DEFAULT_MAX_PENDING},
    {XtNmakeFontsShared, XtCMakeFontsShared, XtRBoolean, sizeof(Boolean),
	Offset(make_fonts_shared), XtRImmediate, (XtPointer) True},
    {XtNshowSampler, XtCShowSampler, XtRBoolean, sizeof(Boolean),
	Offset(show_sampler), XtRImmediate, (XtPointer) False},
    {XtNshowSamplerButton, XtCShowSamplerButton, XtRBoolean, sizeof(Boolean),
	Offset(show_sampler_button), XtRImmediate, (XtPointer) True},
    {XtNtypographicSort, XtCTypographicSort, XtRBoolean, sizeof(Boolean),
	Offset(typographic_sort), XtRImmediate, (XtPointer) True},

    {XtNokCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(ok_callback), XtRCallback, (XtPointer) NULL},
    {XtNapplyCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(apply_callback), XtRCallback, (XtPointer) NULL},
    {XtNresetCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(reset_callback), XtRCallback, (XtPointer) NULL},
    {XtNcancelCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(cancel_callback), XtRCallback, (XtPointer) NULL},
    {XtNvalidateCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(validate_callback), XtRCallback, (XtPointer) NULL},
    {XtNfaceSelectCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(face_select_callback), XtRCallback, (XtPointer) NULL},
    {XtNcreateSamplerCallback, XtCCallback, XtRCallback,
	sizeof(XtCallbackList), Offset(create_sampler_callback),
	XtRCallback, (XtPointer) NULL},
    {XtNcreateCreatorCallback, XtCCallback, XtRCallback,
	sizeof(XtCallbackList), Offset(create_creator_callback),
	XtRCallback, (XtPointer) NULL},
    {XtNvalueChangedCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(value_changed_callback), XtRCallback, (XtPointer) NULL},

    {XtNpaneChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(pane_child), XtRWidget, (XtPointer) NULL},
    {XtNpreviewChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(preview_child), XtRWidget, (XtPointer) NULL},
    {XtNpanelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(panel_child), XtRWidget, (XtPointer) NULL},
    {XtNfamilyLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(family_label_child), XtRWidget, (XtPointer) NULL},
    {XtNfamilyMultipleLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(family_multiple_label_child), XtRWidget, (XtPointer) NULL},
    {XtNfamilyScrolledListChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(family_scrolled_list_child), XtRWidget, (XtPointer) NULL},
    {XtNfaceLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(face_label_child), XtRWidget, (XtPointer) NULL},
    {XtNfaceMultipleLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(face_multiple_label_child), XtRWidget, (XtPointer) NULL},
    {XtNfaceScrolledListChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(face_scrolled_list_child), XtRWidget, (XtPointer) NULL},
    {XtNsizeLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_label_child), XtRWidget, (XtPointer) NULL},
    {XtNsizeTextFieldChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_text_field_child), XtRWidget, (XtPointer) NULL},
    {XtNsizeOptionMenuChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_option_menu_child), XtRWidget, (XtPointer) NULL},
    {XtNsizeMultipleLabelChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(size_multiple_label_child), XtRWidget, (XtPointer) NULL},
    {XtNpreviewButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(preview_button_child), XtRWidget, (XtPointer) NULL},
    {XtNsamplerButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(sampler_button_child), XtRWidget, (XtPointer) NULL},
    {XtNseparatorChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(separator_child), XtRWidget, (XtPointer) NULL},
    {XtNokButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(ok_button_child), XtRWidget, (XtPointer) NULL},
    {XtNapplyButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(apply_button_child), XtRWidget, (XtPointer) NULL},
    {XtNresetButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(reset_button_child), XtRWidget, (XtPointer) NULL},
    {XtNcancelButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(cancel_button_child), XtRWidget, (XtPointer) NULL},
    {XtNmultipleMasterButtonChild, XtCReadOnly, XtRWidget, sizeof(Widget),
	Offset(multiple_master_button_child), XtRWidget, (XtPointer) NULL}
};

/* Forward declarations */

static Boolean ChangeBlends(Widget w, String base_name, String blend_name, FSBBlendAction action, int *axis_values, float *axis_percents);
static Boolean DownloadFontName(Widget w, String name);
static Boolean MatchFontFace(Widget w, String old_face, String new_family, String *new_face);
static Boolean SetValues(Widget old, Widget req, Widget new, ArgList args, Cardinal *num_args);
static Boolean Verify(FontSelectionBoxWidget fsb, FSBValidateCallbackRec *cb, String afm, Boolean doIt);
static String FindAFM(Widget w, String name);
static String FindFontFile(Widget w, String name);
static XtGeometryResult GeometryManager(Widget w, XtWidgetGeometry *desired, XtWidgetGeometry *allowed);
static void ChangeManaged(Widget w);
static void ClassInitialize(void);
static void ClassPartInitialize(WidgetClass widget_class);
static void Destroy(Widget widget);
static void DisplayFontFamilies(FontSelectionBoxWidget fsb);
static void FontFamilyFaceBlendToName(Widget w, String family, String face, String blend, String *font_name);
static void FontFamilyFaceToName(Widget w, String family, String face, String *font_name);
static void FontNameToFamilyFace(Widget w, String font_name, String *family, String *face);
static void FontNameToFamilyFaceBlend(Widget w, String font_name, String *family, String *face, String *blend);
static void FreeFontRec(FontRec *f);
static void GetBlendInfo(Widget w, String name, int *num_axes_return, int *num_designs_return, String **axis_names_return, float **blend_positions_return, int **blend_map_count_return, int **blend_design_coords_return, float **blend_normalized_coords_return);
static void GetBlendList(Widget w, String name, int *count_return, String **blend_return, String **font_name_return, float **axis_values_return);
static void GetFaceList(Widget w, String family, int *count, String **face_list, String **font_list);
static void GetFamilyList(Widget w, int *count, String **list);
static void GetTextDimensions(Widget w, String text, String font, double size, double x, double y, float *dx, float *dy, float *left, float *right, float *top, float *bottom);
static void Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args);
static void ReadBlends(FontSelectionBoxWidget fsb);
static void RefreshFontList(Widget w);
static void Resize(Widget widget);
static void SetFontFamilyFace(Widget w, String family, String face, Bool family_multiple, Bool face_multiple);
static void SetFontFamilyFaceBlend(Widget w, String family, String face, String blend, Bool family_multiple, Bool face_multiple);
static void SetFontName(Widget w, String name, Bool name_multiple);
static void SetFontSize(Widget w, double size, Bool size_multiple);
static void SetUpCurrentSelections(FontSelectionBoxWidget fsb);
static void UndefUnusedFonts(Widget w);
static void WriteBlends(FontSelectionBoxWidget fsb);

FontSelectionBoxClassRec fontSelectionBoxClassRec = {
    /* Core class part */
  {
    /* superclass	     */	(WidgetClass) &xmManagerClassRec,
    /* class_name	     */ "FontSelectionBox",
    /* widget_size	     */ sizeof(FontSelectionBoxRec),
    /* class_initialize      */ ClassInitialize,
    /* class_part_initialize */ ClassPartInitialize,
    /* class_inited          */	False,
    /* initialize	     */	Initialize,
    /* initialize_hook       */	NULL,
    /* realize		     */	XtInheritRealize,
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
   /* FontSelectionBox class part */
  {
    /* set_font_name	     */	SetFontName,
    /* set_font_family_face  */	SetFontFamilyFace,
    /* set_font_size	     */	SetFontSize,
    /* refresh_font_list     */ RefreshFontList,
    /* get_family_list	     */	GetFamilyList,
    /* get_face_list	     */	GetFaceList,
    /* undef_unused_fonts    */ UndefUnusedFonts,
    /* download_font_name    */ DownloadFontName,
    /* match_font_face	     */ MatchFontFace,
    /* font_name_to_family_face */ FontNameToFamilyFace,
    /* font_family_face_to_name */ FontFamilyFaceToName,
    /* find_afm		     */ FindAFM,
    /* find_font_file	     */ FindFontFile,
    /* get_text_dimensions   */ GetTextDimensions,			
    /* set_font_family_face_blend     */ SetFontFamilyFaceBlend,
    /* font_name_to_family_face_blend */ FontNameToFamilyFaceBlend,
    /* font_family_face_blend_to_name */ FontFamilyFaceBlendToName,
    /* get_blend_list        */ GetBlendList,
    /* get_blend_info        */ GetBlendInfo,
    /* change_blends	     */ ChangeBlends,
    /* extension	     */	NULL,
  }
};

WidgetClass fontSelectionBoxWidgetClass =
	(WidgetClass) &fontSelectionBoxClassRec;

/* ARGSUSED */

static Boolean CvtStringToFloatList(
    Display *dpy,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *data)
{
    register int i, count = 1;
    register char *ch, *start = from->addr;
    static float *list;
    char save;

    if (*num_args != 0) {	/* Check for correct number */
	XtAppErrorMsg(XtDisplayToApplicationContext(dpy),
	       "cvtStringToFloatList", "wrongParameters",
	       "XtToolkitError",
	       "String to integer list conversion needs no extra arguments",
	       (String *) NULL, (Cardinal *) NULL);
    }

    if (to->addr != NULL && to->size < sizeof(int *)) {
	to->size = sizeof(int *);
	return False;
    }
    if (start == NULL || *start == '\0') list = NULL;
    else {
	for (ch = start; *ch != '\0'; ch++) {    /* Count floats */
	    if (!isdigit(*ch) && *ch != '.' && *ch != ',') {
		XtDisplayStringConversionWarning(dpy, from->addr, "FloatList");
		return False;
	    }
	    if (*ch == ',') count++;
	}
	list = (float *) XtCalloc(count+1, sizeof(float));

	for (i = 0; i < count; i++) {
	    for (ch = start; *ch != ',' && *ch != '\0'; ch++) {}
	    save = *ch;
	    *ch = '\0';
	    list[i] = atof(start);
	    *ch = save;
	    start = ch + 1;
	}
    }
    if (to->addr == NULL) to->addr = (caddr_t) &list;
    else *(float **) to->addr = list;
    to->size = sizeof(int *);
    return True;
}

/* ARGSUSED */

static void FloatListDestructor(
    XtAppContext app,
    XrmValuePtr to,
    XtPointer converter_data,
    XrmValuePtr args,
    Cardinal *num_args)
{
    float *list = (float *) to->addr;

    if (list == NULL) return;
    XtFree((XtPointer) list);
}

XmString _FSBCreateSharedCS(String str, Widget w)
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
 
static Boolean ScanFloat(char *src, float *f, char **past)
{
    char buf[20], *ch;
    int countDecimals;

    ch = buf;
    countDecimals = 0;
    while (*src == '.' || isdigit(*src)) {
	if (*src == '.') {
	    if (countDecimals) return False;
	    else countDecimals++;
	}
	*ch++ = *src++;
    }
    if (ch == buf) return False;
    *ch++ = '\0';
    *f = atof(buf);
    *past = src;
    return True;
}

static Boolean ScanInt(char *src, int *i, char **past)
{
    char buf[20], *ch;

    ch = buf;
    while (isdigit(*src)) *ch++ = *src++;
    if (ch == buf) return False;
    *ch++ = '\0';
    *i = atoi(buf);
    *past = src;
    return True;
}

static void ClassInitialize(void)
{
    /* Register a converter for string to int list */

    XtSetTypeConverter(XtRString, XtRFloatList,
	    CvtStringToFloatList, (XtConvertArgList) NULL, 0,
	    XtCacheAll | XtCacheRefCount, FloatListDestructor);

    CSempty = UnsharedCS("");
}

static void ClassPartInitialize(WidgetClass widget_class)
{
    register FontSelectionBoxWidgetClass wc =
	    (FontSelectionBoxWidgetClass) widget_class;
    FontSelectionBoxWidgetClass super =
	    (FontSelectionBoxWidgetClass) wc->core_class.superclass;

    if (wc->fsb_class.set_font_name == InheritSetFontName) {
	wc->fsb_class.set_font_name = super->fsb_class.set_font_name;
    }
    if (wc->fsb_class.set_font_family_face == InheritSetFontFamilyFace) {
	wc->fsb_class.set_font_family_face =
		super->fsb_class.set_font_family_face;
    }
    if (wc->fsb_class.set_font_size == InheritSetFontSize) {
	wc->fsb_class.set_font_size = super->fsb_class.set_font_size;
    }
    if (wc->fsb_class.refresh_font_list == InheritRefreshFontList) {
	wc->fsb_class.refresh_font_list = super->fsb_class.refresh_font_list;
    }
    if (wc->fsb_class.get_family_list == InheritGetFamilyList) {
	wc->fsb_class.get_family_list = super->fsb_class.get_family_list;
    }
    if (wc->fsb_class.get_face_list == InheritGetFaceList) {
	wc->fsb_class.get_face_list = super->fsb_class.get_face_list;
    }
    if (wc->fsb_class.undef_unused_fonts == InheritUndefUnusedFonts) {
	wc->fsb_class.undef_unused_fonts = super->fsb_class.undef_unused_fonts;
    }
    if (wc->fsb_class.download_font_name == InheritDownloadFontName) {
	wc->fsb_class.download_font_name = super->fsb_class.download_font_name;
    }
    if (wc->fsb_class.match_font_face == InheritMatchFontFace) {
	wc->fsb_class.match_font_face = super->fsb_class.match_font_face;
    }
    if (wc->fsb_class.font_name_to_family_face ==
						InheritFontNameToFamilyFace) {
	wc->fsb_class.font_name_to_family_face =
		super->fsb_class.font_name_to_family_face;
    }
    if (wc->fsb_class.font_family_face_to_name ==
						InheritFontFamilyFaceToName) {
	wc->fsb_class.font_family_face_to_name =
		super->fsb_class.font_family_face_to_name;
    }
    if (wc->fsb_class.find_afm == InheritFindAFM) {
	wc->fsb_class.find_afm = super->fsb_class.find_afm;
    }
    if (wc->fsb_class.find_font_file == InheritFindFontFile) {
	wc->fsb_class.find_font_file = super->fsb_class.find_font_file;
    }
    if (wc->fsb_class.get_text_dimensions == InheritGetTextDimensions) {
	wc->fsb_class.get_text_dimensions =
		super->fsb_class.get_text_dimensions;
    }
    if (wc->fsb_class.set_font_family_face_blend ==
	InheritSetFontFamilyFaceBlend) {
	wc->fsb_class.set_font_family_face_blend =
		super->fsb_class.set_font_family_face_blend;
    }
    if (wc->fsb_class.font_name_to_family_face_blend ==
	InheritFontNameToFamilyFaceBlend) {
	wc->fsb_class.font_name_to_family_face_blend =
		super->fsb_class.font_name_to_family_face_blend;
    }
    if (wc->fsb_class.font_family_face_blend_to_name ==
	InheritFontFamilyFaceBlendToName) {
	wc->fsb_class.font_family_face_blend_to_name =
		super->fsb_class.font_family_face_blend_to_name;
    }
    if (wc->fsb_class.get_blend_list == InheritGetBlendList) {
	wc->fsb_class.get_blend_list =
		super->fsb_class.get_blend_list;
    }
    if (wc->fsb_class.get_blend_info == InheritGetBlendInfo) {
	wc->fsb_class.get_blend_info =
		super->fsb_class.get_blend_info;
    }
    if (wc->fsb_class.change_blends == InheritChangeBlends) {
	wc->fsb_class.change_blends =
		super->fsb_class.change_blends;
    }
}

static String bugFamilies[] = {
    "Berkeley", "CaslonFiveForty", "CaslonThree", "GaramondThree",
    "Music", "TimesTen", NULL
};

static String fixedFamilies[] = {
    "ITC Berkeley Oldstyle", "Caslon 540", "Caslon 3", "Garamond 3",
    "Sonata", "Times 10", NULL
};

static String missingFoundries[] = {
    "Berthold ", "ITC ", "Linotype ", NULL
};

static int missingFoundryLen[] = {
    9, 4, 9, 0
};

/* I wish we didn't have to do this! */

static void MungeFontNames(
    String name, String family, String fullname, String weight,
    String *familyReturn, String *fullnameReturn, String *faceReturn)
{
    register char *src, *dst, prev;
    char buf[256];
    int digits = 0;
    int i, diff;
    static Bool inited = False;
    static String FetteFrakturDfr, LinotextDfr;

    /* Don't make bugFamilies canonical; we'd have to make the initial
       family canonical to do anything with it and there's no point in that */

    if (!inited) {
	for (i = 0; fixedFamilies[i] != NULL; i++) {
	    fixedFamilies[i] = Canonical(fixedFamilies[i]);
	}
	FetteFrakturDfr = Canonical("FetteFraktur-Dfr");
	LinotextDfr = Canonical("Linotext-Dfr");
	inited = True;
    }

    /* Copy the fullname into buf, enforcing one space between words.
       Eliminate leading digits and spaces, ignore asterisks, if the
       full name ends with 5 digits strip them, and replace periods that
       aren't followed by a space with a space.  If leading digits are
       followed by " pt " skip that too. */

    dst = buf;
    prev = ' ';
    src = fullname; 
    while (isdigit(*src)) src++;
    while (*src == ' ' || *src == '\t') src++;
    if (strncmp(src, "pt ", 3) == 0) src += 3;
    else if (strncmp(src, "pt. ", 4) == 0) src += 4;

    while (*src != '\0') {
	if (*src == '*') {
	    src++;
	    continue;
	}

	if (*src == '.') {
	    if (*(src+1) != ' ') {
		prev = *dst++ = ' ';
	    } else prev = *dst++ = '.';
	    src++;
	    continue;
	}

	if (isdigit(*src)) digits++;
	else digits = 0;

	if (isupper(*src)) {
	    if (prev != ' ' && (islower(*(src+1)) || islower(prev))) {
		*dst++ = ' ';
		prev = *dst++ = *src++;
	    } else prev = *dst++ = *src++;

	} else if (*src == ' ' || *src == '\t') {
	    if (prev == ' ') {
		src++;
		continue;
	    }
	    prev = *dst++ = ' ';
	    src++;

	} else prev = *dst++ = *src++;
    }

    if (digits == 5) {
	dst -= 5;
    }
    if (dst > buf && *(dst-1) == ' ') dst--;

    *dst = '\0';

    /* Stupid Fette Fraktur */

    if (name == FetteFrakturDfr) {
	strcat(buf, " Black Dfr");
    } else if (name == LinotextDfr) {
	strcat(buf, " Dfr");
    }

    if (strncmp(fullname, "pt ", 3) == 0) {
	src = buf + 2;
	while (*++src != '\0') *(src-3) = *src;
	*(src-3) = '\0';
    }
    *fullnameReturn = XtNewString(buf);

    /* From here on fullname should not be used */

    /* Done with the full name; now onto the family */

    for (i = 0; bugFamilies[i] != NULL; i++) {
	diff = strcmp(family, bugFamilies[i]);
	if (diff < 0) break;
	if (diff == 0) {
	    *familyReturn = fixedFamilies[i];
	    goto FAMILY_DONE;
	}
    }

    /* Copy the family into buf, enforcing one space between words */

    dst = buf;
    prev = ' ';
    src = family; 

    while (*src != '\0') {
	if (isupper(*src)) {
	    if (prev != ' ' && (islower(*(src+1)) || islower(prev))) {
		*dst++ = ' ';
		prev = *dst++ = *src++;
	    } else prev = *dst++ = *src++;

	} else if (*src == ' ' || *src == '\t') {
	    if (prev == ' ') {
		src++;
		continue;
	    }
	    prev = *dst++ = ' ';
	    src++;

	} else prev = *dst++ = *src++;
    }

    if (dst > buf && *(dst-1) == ' ') dst--;
    *dst = '\0';

    /* Compensate for fonts with foundries in the full name but not the
       family name by adding to the family name */
 
    for (i = 0; missingFoundries[i] != NULL; i++) {
	diff = strncmp(*fullnameReturn, missingFoundries[i],
		       missingFoundryLen[i]);
	if (diff > 0) continue;
	if (diff == 0 && strncmp(buf, missingFoundries[i],
		       missingFoundryLen[i] != 0)) {
	    while (dst >= buf) {
		*(dst+missingFoundryLen[i]) = *dst;
		dst--;
	    }
	    strncpy(buf, missingFoundries[i], missingFoundryLen[i]);
	}
	break;
    }

    /* From here on dst no longer points to the end of the buffer */

    /* Stupid Helvetica Rounded! */

    if (strncmp(*fullnameReturn, "Helvetica Rounded ", 18) == 0) {
	strcat(buf, " Rounded");
    }

    *familyReturn = Canonical(buf);

    /* From here on family should not be used */

FAMILY_DONE:

    /* Now to find the face in all this */

    src = *fullnameReturn;
    dst = *familyReturn;
    while (*dst == *src && *dst != '\0') {
        src++;
        dst++;
    }
    if (*src == ' ') src++;

    if (*src != '\0') *faceReturn = Canonical(src);
    else if (*weight != '\0') {
	/* Handle Multiple Master fonts */
	if (strcmp(weight, "All") == 0) *faceReturn = Canonical("Roman");
	else {
	    if (islower(weight[0])) weight[0] = toupper(weight[0]);
	    *faceReturn = Canonical(weight);
	}
    } else *faceReturn = Canonical("Medium");
}

static String strip[] = {
    "Adobe ", "Bauer ", "Berthold ", "ITC ", "Linotype ",
    "New ", "Simoncini ", "Stempel ", NULL};

static int striplen[] = {6, 6, 9, 4, 9, 4, 10, 8, 0};

#define STEMPELINDEX 7

static Boolean CreateSortKey(String family, String key)
{
    char newkey[256];
    int len = strlen(family);
    register int i, diff;

    if (family[len-2] == 'P' && family[len-1] == 'i') {
	key[0] = 'P';
	key[1] = 'i';
	key[2] = ' ';
	strcpy(key+3, family);
	key[len] = '\0';
	return True;
    }

    for (i = 0; strip[i] != NULL; i++) {
	diff = strncmp(family, strip[i], striplen[i]);
	if (diff < 0) break;
	if (diff == 0) {
	    if (i == STEMPELINDEX) {
		if (strcmp(family, "Stempel Schneidler") == 0) break;
	    }
	    strcpy(key, family + striplen[i]);
	    key[len - striplen[i]] = ' ';
	    strcpy(key + len - striplen[i] + 1, strip[i]);
	    key[len] = '\0';
	    if (CreateSortKey(key, newkey)) strcpy(key, newkey);
	    return True;
	}
    }
    strcpy(key, family);
    return False;
}

#define SKIP_SPACE(buf) while (*buf == ' ' || *buf == '\t') buf++;

static int CountAxes(char *buf)
{
    int count = 0;

    while (*buf != '\0') {
	SKIP_SPACE(buf)
	if (*buf != '/') return 0;
	buf++;
	count++;
	if (*buf == ' ' || *buf == '\t' || *buf == '\0') return 0;
	while (*buf != ' ' && *buf != '\t' && *buf != '\0') buf++;
    }
    return count;
}

static Boolean ParseBlendPositions(
    char *buf,
    float *blendPos,
    int *axes, int *designs)
{
    int i, j = 0;
    float f;

    *designs = 0;

    while (*buf != '\0') {
	SKIP_SPACE(buf)
	if (*buf++ != '[') return True;

	/* Now there should be *axes positive floats, separated by space */
	SKIP_SPACE(buf)
	for (i = 0; i < *axes; i++) {
	    if (!ScanFloat(buf, &f, &buf)) return True;
	    blendPos[j++] = f;
	    SKIP_SPACE(buf)
	}
	if (*buf++ != ']') return True;
	(*designs)++;
    }
    return False;
}

static Boolean ParseBlendMap(
    char *buf,
    int *breakCount,
    int *blendBreak,
    float *blendBreakValue,
    int *axes)
{
    int i, j = 0;
    int n;
    float f;

    /* OK.  What we expect to see here is *axes arrays.  Each one contains at
       least 2 and no more than 12 subarrays, each of which contains 2 values,
       an int and a float */

    for (i = 0; i < *axes; i++) {

	breakCount[i] = 0;

	SKIP_SPACE(buf)
	if (*buf++ != '[') return True;
	SKIP_SPACE(buf)

	while (*buf == '[') {
	    buf++;
	    SKIP_SPACE(buf)
	    /* Now there should be an integer */
	    if (!ScanInt(buf, &n, &buf)) return True;
	    blendBreak[j] = n;

	    SKIP_SPACE(buf)

	    /* Now there should be a float */
	    if (!ScanFloat(buf, &f, &buf)) return True;
	    blendBreakValue[j++] = f;
	    SKIP_SPACE(buf)

	    /* Nothing more in the array */
	    if (*buf++ != ']') return True;
	    SKIP_SPACE(buf)

	    breakCount[i]++;
	    if (breakCount[i] == 12 && *buf != ']') return True;
	}
	if (*buf++ != ']') return True;
    }
    SKIP_SPACE(buf)
    if (*buf != '\0') return True;
    return False;
}

static Boolean ParseAxisNames(
    int axes,
    char *buf,
    char *names[])
{
    int i = 0;

    /* We expect to see axes names, each optionally preceded with a / and
       separated by space */

    while (*buf != '\0') {
	SKIP_SPACE(buf)
	if (*buf == '/') buf++;
	names[i] = buf;
	while (*buf !=  ' ' && *buf != '\t' && *buf != '\0') buf++;
	if (buf != names[i]) i++;
	if (*buf != '\0') *buf++ = '\0';
	if (i >= axes) return True;
    }
    return False;
}	
#undef SKIP_SPACE

static void GetPSFontInfo(
    FontSelectionBoxWidget fsb,
    char *name,
    int *axes,
    int *designs,
    char *axisNames,
    float *blendPos,
    int *breakCount,
    int *blendBreak,
    float *blendBreakValue)
{
    int entries;
    char **names, **data;

    entries = ListPSResourceFiles(fsb->fsb.resource_path_override,
				  fsb->fsb.default_resource_path,
				  "FontAxes", name,
				  &names, &data);
    if (entries < 1) {
	*axes = 0;
	return;
    }
    *axes = CountAxes(data[0]);
    if (*axes == 0) return;
    strcpy(axisNames, data[0]);

    entries = ListPSResourceFiles(fsb->fsb.resource_path_override,
				  fsb->fsb.default_resource_path,
				  "FontBlendMap", name,
				  &names, &data);
    if (entries < 1) {
	*axes = 0;
	return;
    }
    if (ParseBlendMap(data[0], breakCount,
		      blendBreak, blendBreakValue, axes)) {
	*axes = 0;
	return;
    }

    entries = ListPSResourceFiles(fsb->fsb.resource_path_override,
				  fsb->fsb.default_resource_path,
				  "FontBlendPositions", name,
				  &names, &data);
    if (entries < 1) {
	*axes = 0;
	return;
    }
    if (ParseBlendPositions(data[0], blendPos, axes, designs)) {
	*axes = 0;
	return;
    }
}

static void AddFontRecord(
    FontSelectionBoxWidget fsb,
    int serverNum,
    String name, String family, String fullname, String weight,
    Boolean resident)
{
    FontFamilyRec *ff;
    FontRec *f;
    String familyReturn, fullnameReturn, faceReturn;
    char axisNameBuf[256];
    char *axisName[MAX_AXES];
    int blendedFont, undefineIt, brokenFont;
    int axes, designs, breakCount[MAX_AXES],
	    blendBreak[12 * MAX_AXES];
    float blendBreakValue[12 * MAX_AXES], blendPos[MAX_AXES * MAX_BLENDS];
    char key[256];
    int i, j, k, n;

    name = Canonical(name);

    /* First see if it's there already */

    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == name) {
		if (!f->resident && resident) f->resident = True;
		return;
	    }
	}
    }

    /* We believe that names gotten from PS resource files have been
       pre-munged, so no need to do it again */

    if (resident) {
	/* Have to get the info from the server */
	_DPSFGetFontInfo(fsb->fsb.context, serverNum, fsb->fsb.old_server,
			 family, fullname,
			 weight, &blendedFont, &undefineIt, &brokenFont);

	if (brokenFont) return;

	/* Deal with fonts that don't have useful information */

	if (family[0] == '\0') {
	    if (fullname[0] == '\0') {
		strcpy(family, name);
		strcpy(fullname, name);
	    } else strcpy(family, fullname);
	} else if (fullname[0] == '\0') strcpy(fullname, family);

	MungeFontNames(name, family, fullname, weight,
		       &familyReturn, &fullnameReturn, &faceReturn);
	if (blendedFont) {
	    _DPSFGetBlendedFontInfo(fsb->fsb.context, serverNum,
				    undefineIt, fsb->fsb.old_server,
				    &axes, &designs, axisNameBuf,
				    blendPos, breakCount, blendBreak,
				    blendBreakValue, &brokenFont);
	    if (brokenFont) axes = 0;
	} else axes = 0;

    } else {
	familyReturn = Canonical(family);
	fullnameReturn = XtNewString(fullname);
	faceReturn = Canonical(weight);
	GetPSFontInfo(fsb, name, &axes, &designs, axisNameBuf, blendPos,
		      breakCount, blendBreak, blendBreakValue);
    }

    /* We didn't get an exact match, go for family match */

    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	if (ff->family_name == familyReturn) break;
    }

    if (ff == NULL) {
	ff = (FontFamilyRec *) XtMalloc(sizeof(FontFamilyRec));
	ff->next = fsb->fsb.known_families;
	ff->family_name = familyReturn;
	ff->fonts = NULL;
	ff->font_count = 0;
	ff->blend_count = 0;
	if (fsb->fsb.typographic_sort) {
	    (void) CreateSortKey(familyReturn, key);
	    ff->sort_key = XtNewString(key);
	} else ff->sort_key = ff->family_name;
	fsb->fsb.known_families = ff;
	fsb->fsb.family_count++;
    }

    f = (FontRec *) XtMalloc(sizeof(FontRec));
    f->next = ff->fonts;
    f->font_name = name;
    f->full_name = fullnameReturn;
    f->resident = resident;
    f->temp_resident = False;
    f->in_font_creator = False;
    f->pending_delete_next = NULL;
    f->face_name = faceReturn;
    f->CS_face_name = CS(f->face_name, (Widget) fsb);
    f->blend_count = 0;

    if (axes != 0 && ParseAxisNames(axes, axisNameBuf, axisName)) {
	BlendDataRec *b;
	
	f->blend_data = b = XtNew(BlendDataRec);
	b->num_axes = axes;
	b->num_designs = designs;
	k = 0;

	for (i = 0; i < axes; i++) {
	    b->internal_points[i] = breakCount[i] - 2;
	    if (b->internal_points[i] <= 0) {
		b->internal_break[i] = NULL;
		b->internal_value[i] = NULL;
		b->internal_points[i] = 0;
	    } else {
		b->internal_break[i] = (int *)
			XtMalloc(b->internal_points[i] * sizeof(int));
		b->internal_value[i] = (float *)
			XtMalloc(b->internal_points[i] * sizeof(float));
	    }

	    n = 0;
	    for (j = 0; j < breakCount[i]; j++) {
		if (blendBreakValue[k] == 0.0) b->min[i] = blendBreak[k];
		else if (blendBreakValue[k] == 1.0) b->max[i] = blendBreak[k];
		else {
		    b->internal_break[i][n] = blendBreak[k];
		    b->internal_value[i][n++] = blendBreakValue[k];
		}
		k++;
	    }
	    b->name[i] = Canonical(axisName[i]);
	}

	b->design_positions =
		(float *) XtMalloc(axes * designs * sizeof(float));
	for (i = 0; i < axes * designs; i++) {
	    b->design_positions[i] = blendPos[i];
	}
	b->blends = NULL;
    } else f->blend_data = NULL;

    ff->fonts = f;
    ff->font_count++;
}

static void SortFontNames(FontFamilyRec *ff)
{
    FontRec *f, *highest, **prev, **highestPrev;
    FontRec *newFontList = NULL;

    while (ff->fonts != NULL) {
	prev = highestPrev = &ff->fonts;
	highest = ff->fonts;

	for (f = ff->fonts->next; f != NULL; f = f->next) {
	    prev = &(*prev)->next;
	    if (strcmp(f->face_name, highest->face_name) > 0) {
		highest = f;
		highestPrev = prev;
	    }
	}

	*highestPrev = highest->next;
	highest->next = newFontList;
	newFontList = highest;
    }
    ff->fonts = newFontList;
}

static void SortFontFamilies(FontSelectionBoxWidget fsb)
{
    FontFamilyRec *ff, *highest, **prev, **highestPrev;
    FontFamilyRec *newFamilyList = NULL;

    while (fsb->fsb.known_families != NULL) {
	prev = highestPrev = &fsb->fsb.known_families;
	highest = fsb->fsb.known_families;

	for (ff = fsb->fsb.known_families->next; ff != NULL; ff = ff->next) {
	    prev = &(*prev)->next;
	    if (strcmp(ff->sort_key, highest->sort_key) > 0) {
		highest = ff;
		highestPrev = prev;
	    }
	}

	*highestPrev = highest->next;
	highest->next = newFamilyList;
	newFamilyList = highest;
	SortFontNames(highest);
	if (fsb->fsb.typographic_sort) XtFree(highest->sort_key);
	highest->sort_key = NULL;
    }
    fsb->fsb.known_families = newFamilyList;
}

static void AddFamily(
    FontSelectionBoxWidget fsb,
    char *family, char *fonts, char *weight, char *fullname, char *name)
{
    int j;
    char *ch;

    ch = fonts;
    while (*ch != '\0') {
	j = 0;
	while (1) {
            if (*ch == '\\' && (*(ch+1) == '\\' || *(ch+1) == ',')) {
		ch++;
		weight[j++] = *ch++;
	    } else if (*ch == '\0' || *ch == ',') {
		weight[j] = '\0';
		break;
	    } else weight[j++] = *ch++;
	}
	if (*ch == ',') {
	    j = 0;
	    ch++;
	    while (1) {
		if (*ch == '\\' && (*(ch+1) == '\\' || *(ch+1) == ',')) {
		    ch++;
		    name[j++] = *ch++;
		} else if (*ch == '\0' || *ch == ',') {
		    name[j] = '\0';
		    break;
	        } else name[j++] = *ch++;
	    }
	    strcpy(fullname, family);
	    strcat(fullname, " ");
	    strcat(fullname, weight);
	    AddFontRecord(fsb, 0, name, family, fullname, weight, False);
	    if (*ch == ',') ch++;
	}
    }
}

static void GetFontNames(FontSelectionBoxWidget fsb)
{
    int i;
    char name[256], family[256], fullname[256], weight[256];
    char *buffer, *ch, *start;
    int fontCount, totalLength;
    char **loadableFamilies = NULL, **loadableFamilyFonts = NULL;
    
    fsb->fsb.family_count = 0;

    fontCount = ListPSResourceFiles(fsb->fsb.resource_path_override,
				    fsb->fsb.default_resource_path,
				    PSResFontFamily, NULL,
				    &loadableFamilies, &loadableFamilyFonts);
    for (i = 0; i < fontCount; i++) {
        AddFamily(fsb, loadableFamilies[i], loadableFamilyFonts[i],
		  weight, fullname, name);
    }

    XtFree((XtPointer) loadableFamilies);
    XtFree((XtPointer) loadableFamilyFonts);
    FreePSResourceStorage(False);

    if (fsb->fsb.get_server_fonts) {
	_DPSFEnumFonts(fsb->fsb.context, &fontCount, &totalLength);

	buffer = XtMalloc(totalLength);
	_DPSFGetAllFontNames(fsb->fsb.context, fontCount, totalLength, buffer);
	ch = start = buffer;
	for (i = 0; i < fontCount; i++) {
	    while (*ch != ' ') ch++;
	    *ch = '\0';
	    AddFontRecord(fsb, i, start, family, fullname, weight, True);
	    start = ch+1;
	}
	XtFree(buffer);
    }

    _DPSFFreeFontInfo(fsb->fsb.context);
    SortFontFamilies(fsb);
    ReadBlends(fsb);
}

static void SensitizeReset(FontSelectionBoxWidget fsb)
{
    XtSetSensitive(fsb->fsb.reset_button_child, True);
}

static void DesensitizeReset(FontSelectionBoxWidget fsb)
{
    XtSetSensitive(fsb->fsb.reset_button_child, False);
}

static void ManageFamilyMultiple(FontSelectionBoxWidget fsb)
{
    XtManageChild(fsb->fsb.family_multiple_label_child);

    XtVaSetValues(XtParent(fsb->fsb.family_scrolled_list_child),
		  XmNtopWidget, fsb->fsb.family_multiple_label_child, NULL);
}

static void ManageFaceMultiple(FontSelectionBoxWidget fsb)
{
    XtManageChild(fsb->fsb.face_multiple_label_child);

    XtVaSetValues(XtParent(fsb->fsb.face_scrolled_list_child),
		  XmNtopWidget, fsb->fsb.face_multiple_label_child, NULL);
}

static void ManageMultipleMaster(FontSelectionBoxWidget fsb)
{
    XtManageChild(fsb->fsb.multiple_master_button_child);

    XtVaSetValues(XtParent(fsb->fsb.face_scrolled_list_child),
		  XmNbottomWidget, fsb->fsb.multiple_master_button_child,
		  NULL);
}

static void ManageSizeMultiple(FontSelectionBoxWidget fsb)
{
    XtManageChild(fsb->fsb.size_multiple_label_child);
}

static void UnmanageFamilyMultiple(FontSelectionBoxWidget fsb)
{
    XtVaSetValues(XtParent(fsb->fsb.family_scrolled_list_child),
		  XmNtopWidget, fsb->fsb.family_label_child, NULL);

    XtUnmanageChild(fsb->fsb.family_multiple_label_child);
}

static void UnmanageFaceMultiple(FontSelectionBoxWidget fsb)
{
    XtVaSetValues(XtParent(fsb->fsb.face_scrolled_list_child),
		  XmNtopWidget, fsb->fsb.face_label_child, NULL);

    XtUnmanageChild(fsb->fsb.face_multiple_label_child);
}

static void UnmanageMultipleMaster(FontSelectionBoxWidget fsb)
{
    XtUnmanageChild(fsb->fsb.multiple_master_button_child);

    XtVaSetValues(XtParent(fsb->fsb.face_scrolled_list_child),
		  XmNbottomWidget, fsb->fsb.size_text_field_child, NULL);
}

static void UnmanageSizeMultiple(FontSelectionBoxWidget fsb)
{
    XtUnmanageChild(fsb->fsb.size_multiple_label_child);
}

/* Callbacks for subwidgets */

static Boolean DownloadFont(
    FontSelectionBoxWidget fsb,
    String name,
    DPSContext ctxt,
    Boolean make_shared)
{
    int count;
    char **names, **files;
    FILE *f;
#define BUFLEN 256
    char buf[BUFLEN];
    static char eobuf[] = "\n$Adobe$DPS$Lib$Dict /downloadSuccess true put\n\
stop\n\
Magic end of data line )))))))))) 99#2 2#99 <xyz> // 7gsad,32h4ghNmndFgj2\n";
    int currentShared, ok;

    /* Assume context is correct */

    count = ListPSResourceFiles(fsb->fsb.resource_path_override,
				fsb->fsb.default_resource_path,
				PSResFontOutline, name,
				&names, &files);
    if (count == 0) return False;

    f = fopen(files[0], "r");
    if (f == NULL) return False;

    /* A bug in 1006.9 and earlier servers prevents the more robust
       downloading method from working reliably. */

    if (fsb->fsb.old_server) {
	DPSPrintf(ctxt, "\ncurrentshared %s setshared\n",
		  (make_shared ? "true" : "false"));
	while (fgets(buf, BUFLEN, f) != NULL) {
	    DPSWritePostScript(ctxt, buf, strlen(buf));
	}
	DPSWritePostScript(ctxt, "\nsetshared\n", 11);
	ok = True;

    } else {
	_DPSFPrepareToDownload(ctxt, make_shared, &currentShared);
	DPSWriteData(ctxt, "\nexec\n", 6);

	while (fgets(buf, BUFLEN, f) != NULL) {
	    DPSWriteData(ctxt, buf, strlen(buf));
	}

	/* This marks the end of the data stream */
	DPSWriteData(ctxt, eobuf, strlen(eobuf));

	/* Check the results of the download by getting the error status */
	_DPSFFinishDownload(ctxt, currentShared, &ok);
    }

    fclose (f);
    free(names);
    free(files);
    return ok;

#undef BUFLEN
}

static void UndefSomeUnusedFonts(
    FontSelectionBoxWidget fsb,
    Boolean all)
{
    FontRec *f, *nextf, **start;
    int i;

    if (!all
     && (Cardinal)fsb->fsb.pending_delete_count < fsb->fsb.max_pending_deletes) {
	return;
    }

    if (all) start = &fsb->fsb.pending_delete_font;
    else {
	/* Skip to the end of the ones we're keeping */
	f = fsb->fsb.pending_delete_font;
	for (i = 1; f != NULL && (Cardinal)i < fsb->fsb.max_pending_deletes; i++) {
	    f = f->pending_delete_next;
	}
	if (f == NULL) return;
	start = &f->pending_delete_next;
    }

    for (f = *start; f != NULL; f = nextf) {
	nextf = f->pending_delete_next;
	if (f == fsb->fsb.currently_previewed) {
	    start = &f->pending_delete_next;
	    continue;
	}
	*start = nextf;
	if (!f->resident && !f->in_font_creator) {
	    _DPSFUndefineFont(fsb->fsb.context, f->font_name,
			      fsb->fsb.old_server);
	}
	f->temp_resident = False;
	fsb->fsb.pending_delete_count--;	    
	f->pending_delete_next = NULL;
    }
}

static void UndefUnusedFonts(Widget w)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;

    UndefSomeUnusedFonts(fsb, True);
}

Boolean _FSBDownloadFontIfNecessary(
    FontRec *f,
    FontSelectionBoxWidget fsb)
{
    Boolean shared;

    if (!f->resident && !f->temp_resident) {

	shared = fsb->fsb.make_fonts_shared && !fsb->fsb.undef_unused_fonts;
	if (!fsb->fsb.get_server_fonts) {
	    int resident;
	    /* This font might already be there, so check before downloading */
	    _DPSFIsFontResident(fsb->fsb.context, f->font_name, &resident);
	    if (resident) {
		f->resident = True;
		return True;
	    }
	}
	if (!DownloadFont(fsb, f->font_name, fsb->fsb.context, shared)) {
	    _FSBFlushFont(fsb, f);
	    return False;
	}
	if (shared) f->resident = True;
	else f->temp_resident = True;

	if (f->pending_delete_next == NULL && fsb->fsb.undef_unused_fonts) {
	    f->pending_delete_next = fsb->fsb.pending_delete_font;
	    fsb->fsb.pending_delete_font = f;
	    fsb->fsb.pending_delete_count++;
	    UndefSomeUnusedFonts(fsb, False);
	}
    }
    return True;
}

static void DoPreview(
    FontSelectionBoxWidget fsb,
    Boolean override)
{
    int i, n;
    int *selectList, selectCount;
    float size;
    FontFamilyRec *ff = fsb->fsb.known_families;
    FontRec *f;
    BlendRec *b;
    char *chSize, *fontName;
    Dimension height;
    Cardinal depth;
    int bogusFont;

    if (!XtIsRealized(fsb)) return;

    XtVaGetValues(fsb->fsb.preview_child, XmNheight, &height,
		  XmNdepth, &depth, NULL);

    if (fsb->fsb.gstate == 0) {
	XDPSSetContextParameters(fsb->fsb.context, XtScreen(fsb), depth,
				 XtWindow(fsb->fsb.preview_child), height,
				 (XDPSStandardColormap *) NULL,
				 (XDPSStandardColormap *) NULL,
				 XDPSContextScreenDepth | XDPSContextDrawable |
				 XDPSContextRGBMap | XDPSContextGrayMap);
	XDPSCaptureContextGState(fsb->fsb.context, &fsb->fsb.gstate);
    } else XDPSSetContextGState(fsb->fsb.context, fsb->fsb.gstate);

    _DPSFClearWindow(fsb->fsb.context);

    if (override) {
	if (fsb->fsb.current_family_multiple ||
	    fsb->fsb.current_face_multiple ||
	    fsb->fsb.current_size_multiple) return;
	f = fsb->fsb.currently_previewed;
	size = fsb->fsb.currently_previewed_size;
	b = fsb->fsb.currently_previewed_blend;
    }

    if (!override || f == NULL || size == 0.0) {
	if (!XmListGetSelectedPos(fsb->fsb.family_scrolled_list_child,
				  &selectList, &selectCount)) return;
	if (selectCount == 0 ||
	    *selectList < 1 || *selectList > fsb->fsb.family_count) return;

	for (i = 1; i < *selectList; i++) ff = ff->next;
    
	XtFree((XtPointer) selectList);

	if (!XmListGetSelectedPos(fsb->fsb.face_scrolled_list_child,
				  &selectList, &selectCount)) return;
	if (selectCount == 0 ||
	    *selectList < 1 ||
	    *selectList > ff->font_count + ff->blend_count) return;

	f = ff->fonts;
	n = 0;
	while (1) {
	    n += f->blend_count + 1;
	    if (n >= *selectList) {
		n -= f->blend_count;
		if (n == *selectList) b = NULL;
		else for (b = f->blend_data->blends;
			  n < *selectList - 1; b = b->next) n++;
		break;
	    }
	    f = f->next;
	}
    
	XtFree((XtPointer) selectList);

	XtVaGetValues(fsb->fsb.size_text_field_child,
		      XmNvalue, &chSize, NULL); 

	if (chSize == NULL || *chSize == '\0') return;
	size = atof(chSize);
    }

    if (size <= 0.0) return;

    fsb->fsb.currently_previewed = f;
    fsb->fsb.currently_previewed_blend = b;
    fsb->fsb.currently_previewed_size = size;

    if (!_FSBDownloadFontIfNecessary(f, fsb)) return;

    if (b == NULL) fontName = f->font_name;
    else fontName = b->font_name;

    if (fsb->fsb.preview_string == NULL) {
	_DPSFPreviewString(fsb->fsb.context, fontName, size,
			   f->full_name, height, &bogusFont);
    } else _DPSFPreviewString(fsb->fsb.context, fontName, size,
			      fsb->fsb.preview_string, height, &bogusFont);
    if (bogusFont) {
	_FSBBogusFont(fsb, f);
    }
}

static void DoValueChangedCallback(FontSelectionBoxWidget fsb)
{
    String afm = NULL;
    FSBValidateCallbackRec cb;

    if (fsb->fsb.get_afm) {
	if (fsb->fsb.currently_selected_face == NULL) afm = NULL;
	else afm = FindAFM((Widget) fsb,
			   fsb->fsb.currently_selected_face->font_name);
    }

    (void) Verify(fsb, &cb, afm, False);
    cb.reason = FSBValueChanged;

    XtCallCallbackList((Widget) fsb, fsb->fsb.value_changed_callback, &cb);
}

static void ValueChanged(FontSelectionBoxWidget fsb)
{
    if (fsb->fsb.auto_preview) DoPreview(fsb, False);
    DoValueChangedCallback(fsb);
}

/* ARGSUSED */

static void PreviewText(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;
    XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) callData;

    if (!fsb->fsb.preview_fixed) {
	XSetWindowAttributes att;
	att.bit_gravity = ForgetGravity;
	XChangeWindowAttributes(XtDisplay(fsb),
				XtWindow(fsb->fsb.preview_child),
				CWBitGravity, &att);
	fsb->fsb.preview_fixed = True;
    }

    if (cb != NULL && cb->event->type == Expose &&
	cb->event->xexpose.count != 0) return;

    DoPreview(fsb, True);
}

/* ARGSUSED */

static void PreviewCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    DoPreview(fsb, False);
}

/* ARGSUSED */

static void DismissSamplerCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    fsb->fsb.show_sampler = False;
}

static void ShowSampler(FontSelectionBoxWidget fsb)
{
    int i;
    Arg args[2];
    Widget s;

    if (fsb->fsb.sampler == NULL) {
	FSBCreateSamplerCallbackRec cs;

	cs.sampler_shell = NULL;

	XtCallCallbackList((Widget) fsb, fsb->fsb.create_sampler_callback,
			   (XtPointer) &cs);

	if (cs.sampler_shell == NULL || cs.sampler == NULL) {
	    fsb->fsb.sampler =
		    XtCreatePopupShell("samplerShell",
				       transientShellWidgetClass,	
				       (Widget) fsb, (ArgList) NULL, 0);
	    i = 0;
	    XtSetArg(args[i], XtNfontSelectionBox, fsb);		i++;
	    s = XtCreateManagedWidget("sampler", fontSamplerWidgetClass,
				      fsb->fsb.sampler, args, i);
	    XtAddCallback(s, XtNdismissCallback,
			  DismissSamplerCallback, (XtPointer) fsb);
	} else {
	    fsb->fsb.sampler = cs.sampler_shell;
	    XtAddCallback(cs.sampler, XtNdismissCallback,
			  DismissSamplerCallback, (XtPointer) fsb);
	}
    }
    XtPopup(fsb->fsb.sampler, XtGrabNone);
    XRaiseWindow(XtDisplay(fsb->fsb.sampler), XtWindow(fsb->fsb.sampler));
    fsb->fsb.show_sampler = True;
}

static void ShowCreator(FontSelectionBoxWidget fsb)
{
    int i;
    Arg args[2];
    FSBCreateCreatorCallbackRec cc;

    if (fsb->fsb.creator == NULL) {

	cc.creator_shell = NULL;

	XtCallCallbackList((Widget) fsb, fsb->fsb.create_creator_callback,
			   (XtPointer) &cc);

	if (cc.creator_shell == NULL || cc.creator == NULL) {
	    cc.creator_shell =
		    XtCreatePopupShell("creatorShell",
				       transientShellWidgetClass,	
				       (Widget) fsb, (ArgList) NULL, 0);
	    i = 0;
	    XtSetArg(args[i], XtNfontSelectionBox, fsb);		i++;
	    cc.creator =
		    XtCreateManagedWidget("creator", fontCreatorWidgetClass,
					  cc.creator_shell, args, i);
	}
	fsb->fsb.creator_shell = cc.creator_shell;
	fsb->fsb.creator = cc.creator;
    }

    XtPopup(fsb->fsb.creator_shell, XtGrabNone);
    XRaiseWindow(XtDisplay(fsb->fsb.creator_shell),
		 XtWindow(fsb->fsb.creator_shell)); 

    _FSBSetCreatorFamily(fsb->fsb.creator, fsb->fsb.currently_selected_family);
}

/* ARGSUSED */

static void ShowCreatorCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    ShowCreator(fsb);
}

/* ARGSUSED */

static void ShowSamplerCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    ShowSampler(fsb);
}

/* ARGSUSED */

static void PreviewDoubleClick(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    DoPreview(fsb, False);
}

/* ARGSUSED */

static void ResizePreview(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    Dimension height;
    Cardinal depth;
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    if (!XtIsRealized(widget) || fsb->fsb.gstate == 0) return;

    XtVaGetValues(widget, XmNheight, &height, XmNdepth, &depth, NULL);

    XDPSSetContextGState(fsb->fsb.context, fsb->fsb.gstate);

    XDPSSetContextParameters(fsb->fsb.context, XtScreen(widget), depth,
			     XtWindow(widget), height,
			     (XDPSStandardColormap *) NULL,
			     (XDPSStandardColormap *) NULL,
			     XDPSContextScreenDepth | XDPSContextDrawable);

    _DPSFReclip(fsb->fsb.context);

    XDPSUpdateContextGState(fsb->fsb.context, fsb->fsb.gstate);
}

static String FindAFMRecursive(
    Widget w,
    String name,
    Boolean recur)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    int count;
    char **names, **files;
    String ret, ch;

    if (name == NULL) return NULL;

    count = ListPSResourceFiles(fsb->fsb.resource_path_override,
				fsb->fsb.default_resource_path,
				PSResFontAFM,
				name,
				&names, &files);
    
    if (count == 0 && recur) {
	for (ch = name; *ch != '_' && *ch != '\0'; ch++) {}
	if (*ch == '\0') return NULL;
	*ch = '\0';
	ret = FindAFMRecursive(w, name, False);
	*ch = '_';
	return ret;
    }

    if (count == 0) return NULL;
    ret = files[0];
    free(names);
    free(files);
    return ret;
}

static String FindAFM(Widget w, String name)
{
    return FindAFMRecursive(w, name, True);
}

static String FindFontFileRecursive(
    Widget w,
    String name,
    Boolean recur)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    int count;
    char **names, **files;
    String ret, ch;

    if (name == NULL) return NULL;

    count = ListPSResourceFiles(fsb->fsb.resource_path_override,
				fsb->fsb.default_resource_path,
				PSResFontOutline,
				name,
				&names, &files);
    
    if (count == 0 && recur) {
	for (ch = name; *ch != '_' && *ch != '\0'; ch++) {}
	if (*ch == '\0') return NULL;
	*ch = '\0';
	ret = FindFontFileRecursive(w, name, False);
	*ch = '_';
	return ret;
    }

    if (count == 0) return NULL;
    ret = files[0];
    free(names);
    free(files);
    return ret;
}

static String FindFontFile(Widget w, String name)
{
    return FindFontFileRecursive(w, name, True);
}

static Boolean Verify(
    FontSelectionBoxWidget fsb,
    FSBValidateCallbackRec *cb,
    String afm,
    Boolean doIt)
{
    char *chSize;
    int i;

    if (fsb->fsb.current_family_multiple) {
	cb->family = NULL;
	cb->family_selection = FSBMultiple;
    } else if (fsb->fsb.currently_selected_family == NULL) {
	cb->family = NULL;
	cb->family_selection = FSBNone;
    } else {
	cb->family = fsb->fsb.currently_selected_family->family_name;
	cb->family_selection = FSBOne;
    }

    if (fsb->fsb.current_face_multiple) {
	cb->face = NULL;
	cb->face_selection = FSBMultiple;
    } else if (fsb->fsb.currently_selected_face == NULL) {
	cb->face = NULL;
	cb->face_selection = FSBNone;
    } else {
	cb->face = fsb->fsb.currently_selected_face->face_name;
	cb->face_selection = FSBOne;
    }
	
    if (cb->family_selection == FSBMultiple ||
	cb->face_selection == FSBMultiple) {
	cb->name = NULL;
	cb->name_selection = FSBMultiple;
    } else if (fsb->fsb.currently_selected_face == NULL) {
	cb->name = NULL;
	cb->name_selection = FSBNone;
    } else {
	if (fsb->fsb.currently_selected_blend != NULL) {
	    cb->name = fsb->fsb.currently_selected_blend->font_name;
	} else cb->name = fsb->fsb.currently_selected_face->font_name;
	cb->name_selection = FSBOne;
    }

    if (fsb->fsb.current_size_multiple) {
	cb->size = 0.0;
	cb->size_selection = FSBMultiple;
    } else {
	XtVaGetValues(fsb->fsb.size_text_field_child, XmNvalue, &chSize, NULL);

	if (chSize == NULL || *chSize == '\0') {
	    cb->size = 0.0;
	    cb->size_selection = FSBNone;
	} else {
	    cb->size = atof(chSize);
	    cb->size_selection = FSBOne;
	}
    }

    cb->afm_filename = afm;
    cb->afm_present = (afm != NULL);
    cb->doit = True;

    if (fsb->fsb.currently_selected_blend == NULL) {
	cb->blend = cb->base_name = NULL;
	for (i = 0; i < MAX_AXES; i++) cb->axis_percent[i] = 0.0;
    } else {
	cb->blend = fsb->fsb.currently_selected_blend->blend_name;
	cb->base_name = fsb->fsb.currently_selected_face->font_name;
	for (i = 0; i < MAX_AXES; i++) {
	    cb->axis_percent[i] = fsb->fsb.currently_selected_blend->data[i];
	}
    }

    if (doIt) XtCallCallbackList((Widget) fsb, fsb->fsb.validate_callback, cb);
    return cb->doit;
}

static Boolean VerifyAndCallback(
    FontSelectionBoxWidget fsb,
    FSBCallbackReason reason,
    XtCallbackList callback)
{
    String afm = NULL;
    FSBValidateCallbackRec cb;
    FontRec *fsave, *face;

    if (fsb->fsb.get_afm) {
	if (fsb->fsb.currently_selected_face == NULL) afm = NULL;
	else afm = FindAFM((Widget) fsb,
			   fsb->fsb.currently_selected_face->font_name);
    }

    DoPreview(fsb, False);

    cb.reason = reason;
    if (!Verify(fsb, &cb, afm, True)) return False;

    fsb->fsb.font_family_multiple = fsb->fsb.current_family_multiple;
    if (!fsb->fsb.font_family_multiple &&
	fsb->fsb.currently_selected_family != NULL) {
	fsb->fsb.font_family =
		fsb->fsb.currently_selected_family->family_name;
    } else fsb->fsb.font_family = NULL;

    fsb->fsb.font_face_multiple = fsb->fsb.current_face_multiple;
    if (!fsb->fsb.font_face_multiple &&
	fsb->fsb.currently_selected_face != NULL) {
	fsb->fsb.font_face = fsb->fsb.currently_selected_face->face_name;
    } else fsb->fsb.font_face = NULL;

    fsb->fsb.font_name_multiple =
	    fsb->fsb.font_family_multiple || fsb->fsb.font_face_multiple;
    if (!fsb->fsb.font_name_multiple &&
	fsb->fsb.currently_selected_face != NULL) {
	fsb->fsb.font_name = fsb->fsb.currently_selected_face->font_name;
    } else fsb->fsb.font_name = NULL;

    fsb->fsb.font_size_multiple = fsb->fsb.current_size_multiple;
    if (!fsb->fsb.font_size_multiple) {
	fsb->fsb.font_size = cb.size;
    }

    if (fsb->fsb.currently_selected_blend != NULL) {
	fsb->fsb.font_blend = fsb->fsb.currently_selected_blend->blend_name;
    } else fsb->fsb.font_blend = NULL;

    if (fsb->fsb.undef_unused_fonts) {
	fsave = fsb->fsb.currently_previewed;
	if (fsb->fsb.make_fonts_shared) {
	    fsb->fsb.currently_previewed = NULL;
	}
	UndefUnusedFonts((Widget)fsb);
	fsb->fsb.currently_previewed = fsave;
	face = fsb->fsb.currently_selected_face;
	if (face != NULL && !face->resident) {
	    face->resident = True;
	    if (fsb->fsb.make_fonts_shared) {
		(void) DownloadFont(fsb, face->font_name,
				    fsb->fsb.context, True);
		/* If making it shared, be sure to synchronize with
		   the caller who might be using a different context */
		DPSWaitContext(fsb->fsb.context);
	    }
	}
    }

    XtCallCallbackList((Widget) fsb, callback, &cb);
    return True;
}

/* ARGSUSED */

static void OKCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    if (!VerifyAndCallback(fsb, FSBOK, fsb->fsb.ok_callback)) return;
    if (XtIsShell(XtParent(fsb))) XtPopdown(XtParent(fsb));
    WriteBlends(fsb);
    DesensitizeReset(fsb);
    if (fsb->fsb.show_sampler) XtPopdown(fsb->fsb.sampler);
}

/* ARGSUSED */

static void ApplyCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    (void) VerifyAndCallback(fsb, FSBApply, fsb->fsb.apply_callback);
    WriteBlends(fsb);
    DesensitizeReset(fsb);
}

static void ResetFSB(
    FontSelectionBoxWidget fsb,
    FSBCallbackReason reason)
{
    FSBCallbackRec cb;
    int i;

    fsb->fsb.currently_previewed = NULL;
    fsb->fsb.currently_previewed_size = fsb->fsb.currently_selected_size = 0.0;
    SetUpCurrentSelections(fsb);
    if (fsb->fsb.undef_unused_fonts) UndefUnusedFonts((Widget)fsb);

    cb.reason = reason;
    if (fsb->fsb.font_family_multiple) {
	cb.family = NULL;
	cb.family_selection = FSBMultiple;
    } else if (fsb->fsb.font_family == NULL) {
	cb.family = NULL;
	cb.family_selection = FSBNone;
    } else {
	cb.family = fsb->fsb.font_family;
	cb.family_selection = FSBOne;
    }

    if (fsb->fsb.font_face_multiple) {
	cb.face = NULL;
	cb.face_selection = FSBMultiple;
    } else if (fsb->fsb.font_face == NULL) {
	cb.face = NULL;
	cb.face_selection = FSBNone;
    } else {
	cb.face = fsb->fsb.font_face;
	cb.face_selection = FSBOne;
    }
	
    if (cb.family_selection == FSBMultiple ||
	cb.face_selection == FSBMultiple) {
	cb.name = NULL;
	cb.name_selection = FSBMultiple;
    } else if (fsb->fsb.font_face == NULL) {
	cb.name = NULL;
	cb.name_selection = FSBNone;
    } else {
	cb.name = fsb->fsb.font_name;
	cb.name_selection = FSBOne;
    }

    if (fsb->fsb.font_size_multiple) {
	cb.size = 0.0;
	cb.size_selection = FSBMultiple;
    } else {
	cb.size = fsb->fsb.font_size;
	cb.size_selection = FSBOne;
    }

    cb.afm_filename = NULL;
    cb.afm_present = False;

    cb.blend = fsb->fsb.font_blend;
    if (cb.blend == NULL || fsb->fsb.currently_selected_blend == NULL) {
	cb.base_name = NULL;
	for (i = 0; i < MAX_AXES; i++) cb.axis_percent[i] = 0;
    } else {
	cb.base_name = fsb->fsb.currently_selected_face->font_name;
	for (i = 0; i < MAX_AXES; i++) {
	    cb.axis_percent[i] = fsb->fsb.currently_selected_blend->data[i];
	}
    }

    if (reason == FSBReset) {
	XtCallCallbackList((Widget) fsb, fsb->fsb.reset_callback, &cb);
    } else XtCallCallbackList((Widget) fsb, fsb->fsb.cancel_callback, &cb);
}

/* ARGSUSED */

static void ResetCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    ResetFSB(fsb, FSBReset);
    DesensitizeReset(fsb);
}

/* ARGSUSED */

static void CancelCallback(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    ResetFSB(fsb, FSBCancel);
    if (XtIsShell(XtParent(fsb))) XtPopdown(XtParent(fsb));
    DesensitizeReset(fsb);
    if (fsb->fsb.show_sampler) XtPopdown(fsb->fsb.sampler);
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
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;
    char *ch;

    XtVaGetValues(widget, XmNvalue, &value, NULL);

    if (value == NULL) option = fsb->fsb.other_size;
    else {
	if (value[0] != '\0' && fsb->fsb.current_size_multiple) {
	    fsb->fsb.current_size_multiple = False;
	    UnmanageSizeMultiple(fsb);
	}
	for (ch = value; *ch != '\0'; ch++) if (*ch == '.') *ch = '-';

	option = XtNameToWidget(fsb->fsb.size_menu, value);
	if (option == NULL) option = fsb->fsb.other_size;
    }

    XtVaSetValues(fsb->fsb.size_option_menu_child,
		  XmNmenuHistory, option, NULL);

    if (value != NULL && value[0] != '\0') {
	fsb->fsb.currently_selected_size = atof(value);
    } else fsb->fsb.currently_selected_size = 0.0;

    if (!changingSize) SensitizeReset(fsb);
    fsb->fsb.current_size_multiple = False;
}

/* ARGSUSED */

static void TextVerify(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    int i;
    XmTextVerifyPtr v = (XmTextVerifyPtr) callData;
    char ch, *cp;
    int decimalPoints = 0;
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    if (changingSize) return;	/* We know what we're doing; allow it */

    /* Should probably look at format field, but seems to contain garbage */

    if (v->text->length == 0) return;

    if (v->text->length == 1) {
	ch = v->text->ptr[0];
	if (ch == 'p' || ch == 'P') {
	    XtCallCallbacks(fsb->fsb.preview_button_child,
			    XmNactivateCallback, NULL);
	    v->doit = False;
	    return;
	}
    }
	
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
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;

    if (fsb->fsb.current_size_multiple) {
	fsb->fsb.current_size_multiple = False;
	UnmanageSizeMultiple(fsb);
    }

    strcpy(buf, XtName(widget));
    for (ch = buf; *ch != '\0'; ch++) if (*ch == '-') *ch++ = '.';

    changingSize = True;
    XtVaSetValues(fsb->fsb.size_text_field_child, XmNvalue, buf, NULL);
    changingSize = False;

    SensitizeReset(fsb);
    ValueChanged(fsb);
}

/* This makes sure the selected item is visible */

static void ListSelectPos(
    Widget w,
    int pos,
    Boolean notify)
{
    int topPos, items, visible;

    XmListSelectPos(w, pos, notify);
    
    XtVaGetValues(w, XmNtopItemPosition, &topPos,
		  XmNvisibleItemCount, &visible, XmNitemCount, &items, NULL);

    if (pos >= topPos && pos < topPos + visible) return;
    topPos = pos - (visible-1)/2;
    if (topPos + visible > items) topPos = items - visible + 1;
    if (topPos < 1) topPos = 1;

    XtVaSetValues(w, XmNtopItemPosition, topPos, NULL);
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
            if ((ptr = getenv("USER")) != NULL) pw = getpwnam(ptr);
            else {
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

static void WriteBlendLine(
    FILE *f,
    String family, String face, String blend, String name,
    int axes,
    float *p)
{
    register char *ch;
    int i;

    ch = family;
    while (*ch != '\0') {
	if (*ch == ',' || *ch == '\\') (void) putc('\\', f);
	(void) putc(*ch++, f);
    }
    putc(',', f);
    ch = face;
    while (*ch != '\0') {
	if (*ch == ',' || *ch == '\\') (void) putc('\\', f);
	(void) putc(*ch++, f);
    }
    putc(',', f);
    ch = blend;
    while (*ch != '\0') {
	if (*ch == ',' || *ch == '\\') (void) putc('\\', f);
	(void) putc(*ch++, f);
    }
    (void) putc(',', f);
    ch = name;
    while (*ch != '\0') {
	if (*ch == ',' || *ch == '\\') (void) putc('\\', f);
	(void) putc(*ch++, f);
    }
    for (i = 0; i < axes; i++) fprintf(f, ",%f", p[i]);
    (void) putc('\n', f);
}

static void WriteBlends(FontSelectionBoxWidget fsb)
{
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;
    String blendEnv;
    char homeDir[PATH_BUF_SIZE];
    FILE *blendFile = NULL;
    char fileName[PATH_BUF_SIZE];

    if (!fsb->fsb.blends_changed) return;

    blendEnv = getenv("DPSFONTRC");

    if (blendEnv != NULL) blendFile = fopen(blendEnv, "w");

    if (blendFile == NULL) {
	(void) GetRootDirName(homeDir);
	sprintf(fileName, "%s/.dpsfontrc", homeDir);
	blendFile = fopen(fileName, "w");

	if (blendFile == NULL) return;
    }

    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->blend_data != NULL) {
		for (b = f->blend_data->blends; b != NULL; b = b->next) {
		    WriteBlendLine(blendFile, ff->family_name, f->face_name,
				   b->blend_name, b->font_name,
				   f->blend_data->num_axes, b->data);
		}
	    }
	}
    }

    fclose(blendFile);
    fsb->fsb.blends_changed = False;
}

static Boolean ParseBlendLine(
    String buf, String family, String face, String blend, String name,
    float *p)
{
    char *src, *dst;
    int i;
    float f;

    src = buf;
    dst = family;
    while (*src != ',' && *src != '\0') {
	if (*src == '\\') src++;
	if (*src == '\0') return False;
	*dst++ = *src++;
    }
    if (*src == '\0') return False;
    *dst = '\0';
    src++;
    dst = face;
    while (*src != ',' && *src != '\0') {
	if (*src == '\\') src++;
	if (*src == '\0') return False;
	*dst++ = *src++;
    }
    if (*src == '\0') return False;
    *dst = '\0';
    src++;
    dst = blend;
    while (*src != ',' && *src != '\0') {
	if (*src == '\\') src++;
	if (*src == '\0') return False;
	*dst++ = *src++;
    }
    if (*src == '\0') return False;
    *dst = '\0';
    src++;
    dst = name;
    while (*src != ',' && *src != '\0') {
	if (*src == '\\') src++;
	if (*src == '\0') return False;
	*dst++ = *src++;
    }
    if (*src == '\0') return False;
    *dst = '\0';
    for (i = 0; i < MAX_AXES; i++) {
	src++;
	if (!ScanFloat(src, &f, &src)) {
	    for (/**/; i < MAX_AXES; i++) p[i] = 0;
	    return True;;
	}
	else p[i] = f;
    }
    return True;
}

static void ReadBlends(FontSelectionBoxWidget fsb)
{
    String blendEnv;
    char homeDir[PATH_BUF_SIZE];
    FILE *blendFile = NULL;
    char fileName[PATH_BUF_SIZE];
#define BUF 256
    char buf[BUF+1], family[BUF+1], face[BUF+1], blend[BUF+1], name[BUF+1];
    char *cfamily, *cface;
    float p[MAX_AXES];
    FontRec *f;
    FontFamilyRec *ff = 0;
    BlendRec *b, *newb, **lastb;
    char *spaceBlend;	    
    char *lastFamily = NULL;
    int cmp, i;

    blendEnv = getenv("DPSFONTRC");

    if (blendEnv != NULL) blendFile = fopen(blendEnv, "r");

    if (blendFile == NULL) {
	(void) GetRootDirName(homeDir);
	sprintf(fileName, "%s/.dpsfontrc", homeDir);
	blendFile = fopen(fileName, "r");

	if (blendFile == NULL) return;
    }

    while (1) {
	if (fgets(buf, BUF, blendFile) == NULL) {
	    fclose(blendFile);
	    return;
	}
	if (ParseBlendLine(buf, family, face, blend, name, p)) {
	    cfamily = Canonical(family);
	    if (cfamily != lastFamily) {
		for (ff = fsb->fsb.known_families;
		     ff != NULL && ff->family_name != cfamily;
		     ff = ff->next) {}
	    }
	    if (ff == NULL) continue;
	    lastFamily = cfamily;
	    cface = Canonical(face);
	    for (f = ff->fonts; f != NULL && f->face_name != cface;
		 f = f->next) {}
	    /* If the blend data is NULL, we have a blend line for a font
	       that we don't believe is a MM font.  Ignore it */
	    if (f != NULL && f->blend_data != NULL) {
		lastb = &f->blend_data->blends;
		cmp = -1;
		for (b = f->blend_data->blends; b != NULL; b = b->next) {
		    cmp = strcmp(blend, b->blend_name);
		    if (cmp < 0) break;
		    lastb = &b->next;
		}
		if (cmp != 0) {
		    newb = XtNew(BlendRec);
		    newb->blend_name = Canonical(blend);
		    newb->CS_blend_name = CS(newb->blend_name, (Widget) fsb);

		    spaceBlend = (char *) XtMalloc(strlen(blend) + 4);
		    spaceBlend[0] = spaceBlend[1] = spaceBlend[2] = ' ';
		    strcpy(spaceBlend+3, blend);
		    newb->CS_space_blend_name = CS(spaceBlend, (Widget) fsb);
		    XtFree((XtPointer) spaceBlend);

		    for (i = 0; i < MAX_AXES; i++) newb->data[i] = p[i];
		    newb->font_name = Canonical(name);

		    f->blend_count++;
		    ff->blend_count++;

		    newb->next = b;
		    *lastb = newb;
		}
	    }
	}
    }
}

static void SetUpFaceList(
    FontSelectionBoxWidget fsb,
    FontFamilyRec *ff)
{
    FontRec *f;
    BlendRec *b;
    XmString *CSfaces;
    Boolean multiple = False;
    int i;

    for (f = ff->fonts; f != NULL; f = f->next) {
	if (f->blend_data != NULL) {
	    multiple = True;
	    break;
	}
    }
    if (multiple) ManageMultipleMaster(fsb);
    else UnmanageMultipleMaster(fsb);

    CSfaces = (XmString *) XtCalloc(ff->font_count + ff->blend_count,
				    sizeof(XmString));

    i = 0;
    for (f = ff->fonts; f != NULL; f = f->next) {
	CSfaces[i++] = f->CS_face_name;
	if (f->blend_data != NULL) {
	    for (b = f->blend_data->blends; b != NULL; b = b->next) {
		CSfaces[i++] = b->CS_space_blend_name;
	    }
	}
    }

    XtVaSetValues(fsb->fsb.face_scrolled_list_child,
		  XmNitemCount, ff->font_count + ff->blend_count,
		  XmNitems, CSfaces, NULL);
    XtFree((XtPointer) CSfaces);
}

/* ARGSUSED */

static void DeleteMessage(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XtDestroyWidget(widget);
}

static void FlushFont(
    FontSelectionBoxWidget fsb,
    FontRec *font)
{
    FontRec *f = 0, *f1;
    FontFamilyRec *ff, *ff1;
    Boolean previewedFamily = False;

    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f == font) goto FOUND_BOGUS;
	}
    }

FOUND_BOGUS:
    if (f != NULL) {
	for (f1 = ff->fonts; f1 != NULL; f1 = f1->next) {
	    if (f1 == fsb->fsb.currently_previewed) {
		previewedFamily = True;
		break;
	    }
	}

	if (ff->fonts == f) {
	    ff->fonts = f->next;
	} else {
	    for (f1 = ff->fonts; f1 != NULL && f1->next != f; f1 = f1->next) {}
	    if (f1 != NULL) f1->next = f->next;
	}

	ff->font_count--;
	ff->blend_count -= f->blend_count;

	if (f == fsb->fsb.currently_selected_face) {
	    fsb->fsb.currently_selected_face = NULL;
	    fsb->fsb.currently_selected_blend = NULL;
	}

	if (previewedFamily) SetUpFaceList(fsb, ff);

	if (f == fsb->fsb.currently_previewed) {
	    fsb->fsb.currently_previewed = NULL;
	    fsb->fsb.currently_previewed_blend = NULL;
	    ValueChanged(fsb);
	}

	/* We do not free the FontRec or FontFamilyRec.  In the long
	   run we don't expect to leak much storage this way, since we
	   shouldn't have many bogus fonts, and invalidating every 
	   reference here, in the sampler, and in the creator isn't
	   worth the small storage waste. */

	if (ff->fonts == NULL) {
	    if (fsb->fsb.known_families == ff) {
		fsb->fsb.known_families = ff->next;
	    } else {
		for (ff1 = fsb->fsb.known_families;
		     ff1 != NULL && ff1->next != ff; ff1 = ff1->next) {}
		if (ff1 != NULL) ff1->next = ff->next;
	    }

	    fsb->fsb.family_count--;

	    if (ff == fsb->fsb.currently_selected_family) {
		fsb->fsb.currently_selected_family = NULL;
	    }

	    DisplayFontFamilies(fsb);
	}
    }
}

void _FSBFlushFont(
    FontSelectionBoxWidget fsb,
    FontRec *font)
{
    if (font == fsb->fsb.currently_previewed) _FSBBogusFont(fsb, font);
    else FlushFont(fsb, font);
}

void _FSBBogusFont(
    FontSelectionBoxWidget fsb,
    FontRec *font)
{
    Widget message, w;

    message = XmCreateInformationDialog((Widget) fsb, "invalidFontMessage",
					(ArgList) NULL, 0);
    w =	XmMessageBoxGetChild(message, XmDIALOG_CANCEL_BUTTON);
    XtUnmanageChild(w);
    w =	XmMessageBoxGetChild(message, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(w);
    XtAddCallback(message, XmNokCallback, DeleteMessage, (XtPointer) NULL);
    
    XtManageChild(message);

    /* Now get this blasted thing out of here */
    FlushFont(fsb, font);
}

void _FSBSetUpFaceList(
    FontSelectionBoxWidget fsb,
    Bool redisplay)
{
    FontRec *f;
    BlendRec *b;
    int i;

    SetUpFaceList(fsb, fsb->fsb.currently_selected_family);
    
    f = fsb->fsb.currently_selected_family->fonts;
    i = 1;
    while (f != NULL) {
	if (f == fsb->fsb.currently_selected_face) {
	    if (f->blend_data != NULL) {
		b = f->blend_data->blends;
		if (fsb->fsb.currently_selected_blend != NULL) {
		    i++;
		    while (b != NULL &&
			   b != fsb->fsb.currently_selected_blend) {
			i++;
			b = b->next;
		    }
		}
	    }
	    break;
	} else {
	    i += f->blend_count+1;
	    f = f->next;
	}
    }

    ListSelectPos(fsb->fsb.face_scrolled_list_child, i, False);
    if (redisplay) ValueChanged(fsb);
    fsb->fsb.blends_changed = True;
}

static String categories[][6] = {
    {"Regular", "Roman", "Medium", "Book", "Light", NULL},
    {"Italic", "Slanted", "Oblique", NULL},
    {"Demi", "Semibold", "Heavy", "Bold", NULL},
    {NULL},
};

#define NORMALINDEX 0][0
#define ITALICINDEX 1][0
#define BOLDINDEX 2][3
#define DEMIINDEX 2][0
#define LIGHTINDEX 0][4
#define BOOKINDEX 0][3

static String extraNormalFaces[] = {"Demi", "Semibold", NULL};

static int MatchFaceName(
    FSBFaceSelectCallbackRec *rec,
    Boolean *gaveUp)
{
    int i, j, k, face;
#define PIECEMAX 10
    String pieces[PIECEMAX];
    int numPieces;
    int pass;
    char *ch, *start, *compare;
    char save;
    static Boolean categoriesInited = False;
    static char *canonicalBold, *canonicalLight, *canonicalBook;

    *gaveUp = False;

    if (!categoriesInited) {
	for (i = 0; categories[i][0] != NULL; i++) {
	    for (j = 0; categories[i][j] != NULL; j++) {
		categories[i][j] = Canonical(categories[i][j]);
	    }
	}
	for (i = 0; extraNormalFaces[i] != NULL; i++) {
	    extraNormalFaces[i] = Canonical(extraNormalFaces[i]);
	}
	canonicalBold = categories[BOLDINDEX];
	canonicalLight = categories[LIGHTINDEX];
	canonicalBook = categories[BOOKINDEX];
	categoriesInited = True;
    }

    if (rec->current_face == NULL || rec->current_face[0] == '\0') {
	goto GIVE_UP;
    }

    /* First check for an exact match */

    for (i = 0; i < rec->num_available_faces; i++) {
	if (rec->available_faces[i] == rec->current_face) return i;
    }

    /* Try some category matching.  We make two passes; in the first pass
       we remove "Bold" from the "Demi" family and "Light" and "Book" from
       the "Regular" family; in the second pass we include them.  We ignore
       leading digits in the face name.  */

    categories[BOLDINDEX] = categories[LIGHTINDEX] =
	    categories[BOOKINDEX] = NULL;

    i = 0;
    ch = rec->current_face;
    while (*ch == ' ' || isdigit(*ch)) ch++;
    start = ch;

    while (1) {
	while (*ch != ' ' && *ch != '\0') ch++;
	save = *ch;
	*ch = '\0';
	compare = Canonical(start);
	for (j = 0; categories[j][0] != NULL; j++) {
	    for (k = 0; categories[j][k] != NULL; k++) {
		if (compare == categories[j][k]) {
		    pieces[i++] = categories[j][0];
		    goto FOUND_PIECE;
		}
	    }
	}
	pieces[i++] = compare;	/* A unique piece */
FOUND_PIECE:
	*ch = save;
	while (*ch == ' ') ch++;
	if (*ch == '\0') break;
	if (i >= PIECEMAX) goto GIVE_UP;
	start = ch;
    }
    numPieces = i;
    if (numPieces == 0) goto GIVE_UP;

    /* Special case starting with the italic category */

    if (pieces[0] == categories[ITALICINDEX] && numPieces < PIECEMAX-1) {
	for (i = numPieces; i > 0; i--) pieces[i] = pieces[i-1];
	pieces[0] = categories[NORMALINDEX];
	numPieces++;
    }

    for (pass = 0; pass < 2; pass++) {
	if (pass == 1) {
	    categories[BOLDINDEX] = canonicalBold;
	    categories[LIGHTINDEX] = canonicalLight;
	    categories[BOOKINDEX] = canonicalBook;
	    for (i = 0; i < numPieces; i++) {
		if (pieces[i] == canonicalBold) {
		    pieces[i] = categories[DEMIINDEX];
		} else if (pieces[i] == canonicalLight) {
		    pieces[i] = categories[NORMALINDEX];
		}  else if (pieces[i] == canonicalBook) {
		    pieces[i] = categories[NORMALINDEX];
		}
	    }
	}

	/* Now match against each face */

	for (face = 0; face < rec->num_available_faces; face++) {
	    i = 0;
	    ch = rec->available_faces[face];
	    while (*ch == ' ' || isdigit(*ch)) ch++;
	    start = ch;

	    while (1) {
		while (*ch != ' ' && *ch != '\0') ch++;
		save = *ch;
		*ch = '\0';
		compare = Canonical(start);
		for (j = 0; categories[j][0] != NULL; j++) {
		    for (k = 0; categories[j][k] != NULL; k++) {
			if (compare == categories[j][k]) {
			    compare = categories[j][0];
			    goto MATCH;
			}
		    }
		}
    MATCH:
		/* Special case matching the italic category again */

		if (i == 0 && compare == categories[ITALICINDEX] &&
		    pieces[0] == categories[NORMALINDEX] &&
		    numPieces > 1 &&
		    pieces[1] == categories[ITALICINDEX]) i = 1;

		if (pieces[i] != compare) {
		    *ch = save;
		    goto NEXT_FACE;
		} else i++;

		*ch = save;
		while (*ch == ' ') ch++;
		if (*ch == '\0') break;
		if (i >= numPieces) goto NEXT_FACE;
		start = ch;
	    }
	    if (i == numPieces) return face;	/* Found a match! */
    NEXT_FACE:		
	    ;
	}
    }

    /* Couldn't find a match.  Look for a "normal face".  Make sure "Light"
       and "Book" are installed. Again, ignore leading spaces.  */
GIVE_UP:
    *gaveUp = True;
    categories[LIGHTINDEX] = canonicalLight;
    categories[BOOKINDEX] = canonicalBook;

    for (i = 0; categories[0][i] != NULL; i++) {
	for (face = 0; face < rec->num_available_faces; face++) {
	    compare = rec->available_faces[face];
	    while (*compare == ' ' || isdigit(*compare)) compare++;
	    if (compare != rec->available_faces[face]) {
		compare = Canonical(compare);
	    }
	    if (categories[0][i] == compare) return face;
	}
    }

    for (i = 0; extraNormalFaces[i] != NULL; i++) {
	for (face = 0; face < rec->num_available_faces; face++) {
	    compare = rec->available_faces[face];
	    while (*compare == ' ' || isdigit(*compare)) compare++;
	    if (compare != rec->available_faces[face]) {
		compare = Canonical(compare);
	    }
	    if (extraNormalFaces[i] == compare) return face;
	}
    }
    
    /* Oh, well.  Use the first one */
    return 0;
}

static void GetInitialFace(
    FontSelectionBoxWidget fsb,
    FontFamilyRec *ff)
{
    FSBFaceSelectCallbackRec rec;
    String *faces;
    int i, j;
    FontRec *f;
    Boolean junk;

    faces = (String *) XtMalloc(ff->font_count * sizeof(String));
    i = 0;
    for (f = ff->fonts; f != NULL; f = f->next) faces[i++] = f->face_name;

    rec.available_faces = faces;
    rec.num_available_faces = ff->font_count;

    if (fsb->fsb.currently_selected_face != NULL) {
	rec.current_face = fsb->fsb.currently_selected_face->face_name;
    } else rec.current_face = fsb->fsb.font_face;

    rec.new_face = NULL;

    XtCallCallbackList((Widget) fsb, fsb->fsb.face_select_callback, &rec);
    if (rec.new_face != NULL) {
	for (i = 0; i < ff->font_count; i++) {
	    if (rec.new_face == faces[i]) break;
	}
    }
    if (rec.new_face == NULL || i >= ff->font_count) {
	i = MatchFaceName(&rec, &junk);
    }
    XtFree((XtPointer) faces);

    j = 0;
    for (f = ff->fonts; i != 0; f= f->next) {
	j += f->blend_count + 1;
	i--;
    }
    
    ListSelectPos(fsb->fsb.face_scrolled_list_child, j+1, False);
    fsb->fsb.currently_selected_face = f;
    fsb->fsb.currently_selected_blend = NULL;
}

/* ARGSUSED */

static void FamilySelect(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XmListCallbackStruct *listCB = (XmListCallbackStruct *) callData;
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;
    FontFamilyRec *ff = fsb->fsb.known_families;
    int i;

    if (fsb->fsb.current_family_multiple) {
	fsb->fsb.current_family_multiple = False;
	UnmanageFamilyMultiple(fsb);
    }

    /* List uses 1-based addressing!! */
    for (i = 1; i < listCB->item_position; i++) ff = ff->next;

    fsb->fsb.currently_selected_family = ff;

    SensitizeReset(fsb);
    SetUpFaceList(fsb, ff);
    if (!fsb->fsb.current_face_multiple) GetInitialFace(fsb, ff);
    ValueChanged(fsb);
}

/* ARGSUSED */

static void FaceSelect(
    Widget widget,
    XtPointer clientData, XtPointer callData)
{
    XmListCallbackStruct *listCB = (XmListCallbackStruct *) callData;
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) clientData;
    FontRec *f;
    BlendRec *b;
    int n;

    if (fsb->fsb.currently_selected_family == NULL) return;
    f = fsb->fsb.currently_selected_family->fonts;

    if (fsb->fsb.current_face_multiple) {
	fsb->fsb.current_face_multiple = False;
	UnmanageFaceMultiple(fsb);
    }

    /* List uses 1-based addressing!! */
    n = 0;
    while (1) {
	n += f->blend_count + 1;
	if (n >= listCB->item_position) {
	    n -= f->blend_count;
	    if (n == listCB->item_position) b = NULL;
	    else for (b = f->blend_data->blends; n < listCB->item_position - 1;
		      b = b->next) n++;
	    break;
	}
	f = f->next;
    }

    fsb->fsb.currently_selected_face = f;
    fsb->fsb.currently_selected_blend = b;

    SensitizeReset(fsb);
    ValueChanged(fsb);
}

static void CreateSizeMenu(
    FontSelectionBoxWidget fsb,
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
	XtVaGetValues(fsb->fsb.size_menu, XtNchildren, &children,
		      XtNnumChildren, &num_children, NULL);

	/* Don't destroy first child ("other") */
	for (j = 1; (Cardinal)j < num_children; j++) XtDestroyWidget(children[j]);

	sizes = (Widget *) XtMalloc((fsb->fsb.size_count+1) * sizeof(Widget));
	sizes[0] = children[0];
    } else {
	i = 0;
	sizes = (Widget *) XtMalloc((fsb->fsb.size_count+1) * sizeof(Widget));
	fsb->fsb.other_size = sizes[0] =
		XtCreateManagedWidget("other", xmPushButtonGadgetClass,
				  fsb->fsb.size_menu, args, i);
    }

    for (j = 0; j < fsb->fsb.size_count; j++) {
	(void) sprintf(buf, "%g", fsb->fsb.sizes[j]);
	csName = UnsharedCS(buf);
	for (ch = buf; *ch != '\0'; ch++) if (*ch == '.') *ch = '-';
	i = 0;
	XtSetArg(args[i], XmNlabelString, csName);			i++;
	sizes[j+1] =
		XmCreatePushButtonGadget(fsb->fsb.size_menu, buf, args, i);
	XmStringFree(csName);
	XtAddCallback(sizes[j+1], XmNactivateCallback,
		      SetSize, (XtPointer) fsb);
    }
    XtManageChildren(sizes, j+1);
    XtFree((char *) sizes);
}

static void CreateChildren(FontSelectionBoxWidget fsb)
{
    Arg args[20];
    int i;
    Widget form;

    i = 0;
    fsb->fsb.pane_child =
	    XtCreateManagedWidget("pane", xmPanedWindowWidgetClass,
				  (Widget) fsb, args, i);

    i = 0;
    fsb->fsb.preview_child =
	    XtCreateManagedWidget("preview", xmDrawingAreaWidgetClass,
				  fsb->fsb.pane_child, args, i);
    XtAddCallback(fsb->fsb.preview_child, XmNexposeCallback,
		  PreviewText, (XtPointer) fsb);
    XtAddCallback(fsb->fsb.preview_child, XmNresizeCallback,
		  ResizePreview, (XtPointer) fsb);

    i = 0;	
    form = XtCreateManagedWidget("panel", xmFormWidgetClass,
				 fsb->fsb.pane_child, args, i);
    fsb->fsb.panel_child = form;

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.ok_button_child =
	    XtCreateManagedWidget("okButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(fsb->fsb.ok_button_child, XmNactivateCallback,
		  OKCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget,fsb->fsb.ok_button_child );		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.apply_button_child =
	    XtCreateManagedWidget("applyButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(fsb->fsb.apply_button_child, XmNactivateCallback,
		  ApplyCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget,fsb->fsb.apply_button_child );	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.reset_button_child =
	    XtCreateManagedWidget("resetButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(fsb->fsb.reset_button_child, XmNactivateCallback,
		  ResetCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget,fsb->fsb.reset_button_child );	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.cancel_button_child =
	    XtCreateManagedWidget("cancelButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(fsb->fsb.cancel_button_child, XmNactivateCallback,
		  CancelCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.ok_button_child);	i++;
    fsb->fsb.separator_child =
	    XtCreateManagedWidget("separator", xmSeparatorGadgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.separator_child);	i++;
    fsb->fsb.size_label_child =
	    XtCreateManagedWidget("sizeLabel", xmLabelWidgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, fsb->fsb.size_label_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.size_label_child);	i++;
    fsb->fsb.size_text_field_child =
	    XtCreateManagedWidget("sizeTextField", xmTextFieldWidgetClass,
				  form, args, i);
    XtAddCallback(fsb->fsb.size_text_field_child, XmNvalueChangedCallback,
		  SizeSelect, (XtPointer) fsb);
    XtAddCallback(fsb->fsb.size_text_field_child, XmNmodifyVerifyCallback,
		  TextVerify, (XtPointer) fsb);

    i = 0;
    fsb->fsb.size_menu = XmCreatePulldownMenu(form, "sizeMenu", args, i);

    CreateSizeMenu(fsb, False);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, fsb->fsb.size_text_field_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.size_label_child);	i++;
    XtSetArg(args[i], XmNsubMenuId, fsb->fsb.size_menu);		i++;
    fsb->fsb.size_option_menu_child =
	    XmCreateOptionMenu(form, "sizeOptionMenu", args, i);
    XtManageChild(fsb->fsb.size_option_menu_child);

    i = 0;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNleftWidget, fsb->fsb.size_option_menu_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.separator_child);	i++;
    fsb->fsb.size_multiple_label_child =
	    XtCreateWidget("sizeMultipleLabel", xmLabelWidgetClass,
			   form, args, i);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.size_label_child);	i++;
    fsb->fsb.preview_button_child =
	    XtCreateManagedWidget("previewButton", xmPushButtonWidgetClass,
				  form, args, i);
    XtAddCallback(fsb->fsb.preview_button_child, XmNactivateCallback,
		  PreviewCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNrightWidget, fsb->fsb.preview_button_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);	i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.preview_button_child);	i++;
    fsb->fsb.sampler_button_child =
	    XtCreateWidget("samplerButton", xmPushButtonWidgetClass,
				  form, args, i);
    if (fsb->fsb.show_sampler_button) {
	XtManageChild(fsb->fsb.sampler_button_child);
    }
    XtAddCallback(fsb->fsb.sampler_button_child, XmNactivateCallback,
		  ShowSamplerCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightPosition, 50);				i++;
    fsb->fsb.family_label_child =
	    XtCreateManagedWidget("familyLabel", xmLabelGadgetClass,
				  form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);			i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNleftPosition, 50);				i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.face_label_child =
	    XtCreateManagedWidget("faceLabel", xmLabelGadgetClass,
				  form, args, i);

    /* The next two must be widgets in order to be reversed in color */

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, fsb->fsb.family_label_child);	i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightPosition, 50);				i++;
    fsb->fsb.family_multiple_label_child =
	    XtCreateWidget("familyMultipleLabel", xmLabelWidgetClass,
			   form, args, i);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, fsb->fsb.face_label_child);		i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNleftPosition, 50);				i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.face_multiple_label_child =
	    XtCreateWidget("faceMultipleLabel", xmLabelWidgetClass,
			   form, args, i);

    i = 0; 
    XtSetArg(args[i], XmNitemCount, 1);					i++;
    XtSetArg(args[i], XmNitems, &CSempty);				i++;
    fsb->fsb.family_scrolled_list_child =
	    XmCreateScrolledList(form, "familyScrolledList", args, i);
    XtAddCallback(fsb->fsb.family_scrolled_list_child,
		  XmNbrowseSelectionCallback, FamilySelect, (XtPointer) fsb);
    XtAddCallback(fsb->fsb.family_scrolled_list_child,
		  XmNdefaultActionCallback,
		  PreviewDoubleClick, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, fsb->fsb.family_label_child);	i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.size_text_field_child);	i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);		i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNrightPosition, 50);				i++;
    XtSetValues(XtParent(fsb->fsb.family_scrolled_list_child), args, i);
    XtManageChild(fsb->fsb.family_scrolled_list_child);

    i = 0;
    XtSetArg(args[i], XmNitemCount, 1);					i++;
    XtSetArg(args[i], XmNitems, &CSempty);				i++;
    fsb->fsb.face_scrolled_list_child =
	    XmCreateScrolledList(form, "faceScrolledList", args, i);
    XtAddCallback(fsb->fsb.face_scrolled_list_child,
		  XmNbrowseSelectionCallback, FaceSelect, (XtPointer) fsb);
    XtAddCallback(fsb->fsb.face_scrolled_list_child,
		  XmNdefaultActionCallback, PreviewDoubleClick,
		  (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNtopWidget, fsb->fsb.face_label_child);		i++;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.size_text_field_child);	i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNleftPosition, 50);				i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    XtSetValues(XtParent(fsb->fsb.face_scrolled_list_child), args, i);
    XtManageChild(fsb->fsb.face_scrolled_list_child);

    i = 0;
    XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);		i++;
    XtSetArg(args[i], XmNbottomWidget, fsb->fsb.size_text_field_child);	i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_POSITION);		i++;
    XtSetArg(args[i], XmNleftPosition, 50);				i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);		i++;
    fsb->fsb.multiple_master_button_child =
	    XtCreateWidget("multipleMasterButton", xmPushButtonWidgetClass,
			   form, args, i);
    XtAddCallback(fsb->fsb.multiple_master_button_child, XmNactivateCallback,
		  ShowCreatorCallback, (XtPointer) fsb);

    i = 0;
    XtSetArg(args[i], XmNdefaultButton, fsb->fsb.ok_button_child);	i++;
    XtSetValues(form, args, i);
}

static void DisplayFontFamilies(FontSelectionBoxWidget fsb)
{
    FontFamilyRec *ff;
    XmString *CSlist, *str;
    
    CSlist = (XmString *) XtMalloc(fsb->fsb.family_count * sizeof(XmString));
    str = CSlist;
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	*str++ = UnsharedCS(ff->family_name);
    }
    
    XtVaSetValues(fsb->fsb.family_scrolled_list_child,
		  XmNitemCount, fsb->fsb.family_count,
		  XmNitems, CSlist, NULL);		

    /* The list makes a copy, so we can delete the list */
    XtFree((char *) CSlist);
}

static void SetUpCurrentFontFromName(FontSelectionBoxWidget fsb)
{
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;
    int i, j;

    fsb->fsb.currently_selected_face = NULL;
    fsb->fsb.currently_selected_family = NULL;
    fsb->fsb.currently_selected_blend = NULL;

    if (fsb->fsb.font_name_multiple || fsb->fsb.font_name == NULL) {
	fsb->fsb.font_name = NULL;
	fsb->fsb.font_family = NULL;
	fsb->fsb.font_blend = NULL;
	fsb->fsb.font_face = NULL;
	if (fsb->fsb.font_name_multiple) {
	    fsb->fsb.current_family_multiple = True;
	    fsb->fsb.current_face_multiple = True;
	    ManageFamilyMultiple(fsb);
	    ManageFaceMultiple(fsb);
	}
	XmListDeselectAllItems(fsb->fsb.family_scrolled_list_child);
	XmListDeselectAllItems(fsb->fsb.face_scrolled_list_child);
	XmListDeleteAllItems(fsb->fsb.face_scrolled_list_child);
	XmListAddItem(fsb->fsb.face_scrolled_list_child, CSempty, 1);
	return;
    }

    if (!fsb->fsb.font_name_multiple) {
	fsb->fsb.current_family_multiple = False;
	fsb->fsb.current_face_multiple = False;
	UnmanageFamilyMultiple(fsb);
	UnmanageFaceMultiple(fsb);
    }

    fsb->fsb.font_name = Canonical(fsb->fsb.font_name);
    i = 1;
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	j = 1;
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == fsb->fsb.font_name) {
		fsb->fsb.font_family = ff->family_name;
		fsb->fsb.font_face = f->face_name;
		SetUpFaceList(fsb, ff);
		ListSelectPos(fsb->fsb.family_scrolled_list_child, i, False);
		ListSelectPos(fsb->fsb.face_scrolled_list_child, j, False);
		fsb->fsb.currently_selected_face = f;
		fsb->fsb.currently_selected_family = ff;
		fsb->fsb.currently_selected_blend = NULL;
		return;
	    }
	    j++;
	    if (f->blend_data != NULL && f->blend_data->blends != NULL) {
		for (b = f->blend_data->blends; b != NULL; b = b->next) {
		    if (b->font_name == fsb->fsb.font_name) {
			fsb->fsb.font_family = ff->family_name;
			fsb->fsb.font_face = f->face_name;
			SetUpFaceList(fsb, ff);
			ListSelectPos(fsb->fsb.family_scrolled_list_child, i,
				      False);
			ListSelectPos(fsb->fsb.face_scrolled_list_child, j,
				      False);
			fsb->fsb.currently_selected_face = f;
			fsb->fsb.currently_selected_family = ff;
			fsb->fsb.currently_selected_blend = b;
			return;
		    }
		    j++;
		}
	    }

	}
	i++;
    }
 
   /* Didn't find it! */
    fsb->fsb.font_name = NULL;
    fsb->fsb.font_family = NULL;
    fsb->fsb.font_face = NULL;
    fsb->fsb.font_blend = NULL;
    XmListDeselectAllItems(fsb->fsb.family_scrolled_list_child);
    XmListDeselectAllItems(fsb->fsb.face_scrolled_list_child);
    XmListDeleteAllItems(fsb->fsb.face_scrolled_list_child);
    XmListAddItem(fsb->fsb.face_scrolled_list_child, CSempty, 1);
}

static void SetUpCurrentFontFromFamilyFace(FontSelectionBoxWidget fsb)
{
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;
    int i;

    fsb->fsb.currently_selected_face = NULL;
    fsb->fsb.currently_selected_family = NULL;
    fsb->fsb.currently_selected_blend = NULL;

    if (fsb->fsb.font_family_multiple) {
	fsb->fsb.font_family = NULL;
	fsb->fsb.current_family_multiple = True;
	ManageFamilyMultiple(fsb);
    } else {
	fsb->fsb.current_family_multiple = False;
	UnmanageFamilyMultiple(fsb);
    }

    if (fsb->fsb.font_face_multiple) {
	fsb->fsb.font_face = NULL;
	fsb->fsb.current_face_multiple = True;
	ManageFaceMultiple(fsb);
    } else {
	fsb->fsb.current_face_multiple = False;
	UnmanageFaceMultiple(fsb);
    }

    fsb->fsb.font_name_multiple =
	    fsb->fsb.font_family_multiple || fsb->fsb.font_face_multiple;

    if (fsb->fsb.font_family != NULL) {
	fsb->fsb.font_family = Canonical(fsb->fsb.font_family);
	i = 1;
	for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	    if (fsb->fsb.font_family == ff->family_name) {
		ListSelectPos(fsb->fsb.family_scrolled_list_child, i, False);
		fsb->fsb.currently_selected_family = ff;
		SetUpFaceList(fsb, ff);
		break;
	    }
	    i++;
	}
	if (ff == NULL) fsb->fsb.font_family = NULL;
    }   
	
    if (fsb->fsb.font_family == NULL) {
	fsb->fsb.font_face = NULL;
	fsb->fsb.font_blend = NULL;
	fsb->fsb.font_name = NULL;
	XmListDeselectAllItems(fsb->fsb.family_scrolled_list_child);
	XmListDeselectAllItems(fsb->fsb.face_scrolled_list_child);
	XmListDeleteAllItems(fsb->fsb.face_scrolled_list_child);
	XmListAddItem(fsb->fsb.face_scrolled_list_child, CSempty, 1);
	return;
    }

    if (fsb->fsb.font_face != NULL) {
	fsb->fsb.font_face = Canonical(fsb->fsb.font_face);

	i = 1;
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (fsb->fsb.font_face == f->face_name) {
		fsb->fsb.currently_selected_face = f;
		if (fsb->fsb.font_blend != NULL) {
		    fsb->fsb.font_blend = Canonical(fsb->fsb.font_blend);
		    for (b = f->blend_data->blends; b != NULL; b = b->next) {
			i++;
			if (b->blend_name == fsb->fsb.font_blend) {
			    fsb->fsb.currently_selected_blend = b;
			    break;
			}
		    }
		    if (b == NULL) {
			fsb->fsb.font_blend = NULL;
			i -= f->blend_count;
		    }
		}
		ListSelectPos(fsb->fsb.face_scrolled_list_child, i, False);
		break;
	    }
	    i += f->blend_count + 1;
	}
	if (f == NULL) fsb->fsb.font_face = NULL;
    } else {
	f = NULL;
	XmListDeselectAllItems(fsb->fsb.face_scrolled_list_child);
    }

    if (f == NULL && !fsb->fsb.font_face_multiple) GetInitialFace(fsb, ff);
}

static void SetUpCurrentFont(FontSelectionBoxWidget fsb)
{
    if (fsb->fsb.use_font_name) SetUpCurrentFontFromName(fsb);
    else SetUpCurrentFontFromFamilyFace(fsb);
}

static void SetUpCurrentSize(FontSelectionBoxWidget fsb)
{
    char buf[20];

    if (fsb->fsb.font_size_multiple) {
	changingSize = True;
	XtVaSetValues(fsb->fsb.size_text_field_child, XmNvalue, "", NULL);
	changingSize = False;
	fsb->fsb.current_size_multiple = True;
	ManageSizeMultiple(fsb);
	return;
    } else UnmanageSizeMultiple(fsb);
    
    if (fsb->fsb.currently_selected_size == 0.0) {
	sprintf(buf, "%g", fsb->fsb.font_size);
    } else sprintf(buf, "%g", fsb->fsb.currently_selected_size);

    changingSize = True;
    XtVaSetValues(fsb->fsb.size_text_field_child, XmNvalue, buf, NULL);
    changingSize = False;
}

static void SetUpCurrentSelections(FontSelectionBoxWidget fsb)
{
    SetUpCurrentFont(fsb);
    SetUpCurrentSize(fsb);
    if (fsb->fsb.preview_on_change) DoPreview(fsb, False);
    DoValueChangedCallback(fsb);
}

/* ARGSUSED */

static void Initialize(
    Widget request, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) new;
    Bool inited;
    char version[20];

    /* Verify size list */

    if (fsb->fsb.size_count > 0 && fsb->fsb.sizes == NULL) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontBox", "sizeMismatch",
			"FontSelectionBoxError",
			"Size count specified but no sizes present",
			(String *) NULL, (Cardinal *) NULL);
	fsb->fsb.size_count = 0;
    }

    if (fsb->fsb.size_count < 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontBox", "negativeSize",
			"FontSelectionBoxError",
			"Size count should not be negative",
			(String *) NULL, (Cardinal *) NULL);
	fsb->fsb.size_count = 0;
    }

    if (fsb->fsb.max_pending_deletes <= 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"initializeFontBox", "nonPositivePendingDelete",
			"FontSelectionBoxError",
			"Pending delete max must be positive",
			(String *) NULL, (Cardinal *) NULL);
	fsb->fsb.max_pending_deletes = 1;
    }

    /* Copy strings.  SetUpCurrentSelection will copy the font strings */

    if (fsb->fsb.preview_string != NULL) {
	fsb->fsb.preview_string = XtNewString(fsb->fsb.preview_string);
    }
    if (fsb->fsb.default_resource_path != NULL) {
	fsb->fsb.default_resource_path =
		XtNewString(fsb->fsb.default_resource_path);
    }
    if (fsb->fsb.resource_path_override != NULL) {
	fsb->fsb.resource_path_override =
		XtNewString(fsb->fsb.resource_path_override);
    }

    /* Get the context */

    if (fsb->fsb.context == NULL) {
	fsb->fsb.context = XDPSGetSharedContext(XtDisplay(fsb));
    }

    if (_XDPSTestComponentInitialized(fsb->fsb.context,
				      dps_init_bit_fsb, &inited) ==
	dps_status_unregistered_context) {
	XDPSRegisterContext(fsb->fsb.context, False);
    }

    if (!inited) {
	(void) _XDPSSetComponentInitialized(fsb->fsb.context,
					    dps_init_bit_fsb);
	_DPSFDefineFontEnumFunctions(fsb->fsb.context);
    }

    DPSversion(fsb->fsb.context, 20, version);
    fsb->fsb.old_server = (atof(version) < 1007);

    /* Initialize non-resource fields */

    fsb->fsb.gstate = 0;
    fsb->fsb.sampler = fsb->fsb.creator = NULL;
    fsb->fsb.known_families = NULL;
    fsb->fsb.family_count = 0;
    fsb->fsb.currently_previewed = NULL;
    fsb->fsb.currently_selected_face = NULL;
    fsb->fsb.currently_selected_family = NULL;
    fsb->fsb.currently_previewed_blend = NULL;
    fsb->fsb.currently_selected_blend = NULL;
    fsb->fsb.currently_previewed_size = 0.0;
    fsb->fsb.currently_selected_size = 0.0;
    fsb->fsb.pending_delete_count = 0;
    fsb->fsb.pending_delete_font = NULL;
    fsb->fsb.preview_fixed = False;
    fsb->fsb.current_family_multiple = False;
    fsb->fsb.current_face_multiple = False;
    fsb->fsb.current_size_multiple = False;
    fsb->fsb.blends_changed = False;

    GetFontNames(fsb);
    CreateChildren(fsb);

    DisplayFontFamilies(fsb);
    SetUpCurrentSelections(fsb);
    DesensitizeReset(fsb);
    if (fsb->fsb.show_sampler) ShowSampler(fsb);
}

static void FreeFontRec(FontRec *f)
{
    BlendDataRec *bd;
    BlendRec *b, *next_b;

    if (f->blend_data != NULL) {
	bd = f->blend_data;
	for (b = bd->blends; b != NULL; b = next_b) {
	    next_b = b->next;
	    XtFree((char *) b);
	}
	XtFree((char *) bd->internal_break);
	XtFree((char *) bd->internal_value);
	XtFree((char *) bd->design_positions);
	XtFree((char *) bd);
    }
    XtFree(f->full_name);
}

static void FreeFontLists(
    FontSelectionBoxWidget fsb)
{
    FontFamilyRec *ff, *next_ff;
    FontRec *f, *next_f;

    /* font_name, face_name, family_name, and blend_name are canonical
       strings and so should not be freed.  The face and blend compound
       strings were gotten from converters and so should likewise remain. */

    for (ff = fsb->fsb.known_families; ff != NULL; ff = next_ff) {
	for (f = ff->fonts; f != NULL; f = next_f) {
	    FreeFontRec(f);
	    next_f = f->next;
	    XtFree((char *) f);
	}
	next_ff = ff->next;
	XtFree((char *) ff);
    }
    fsb->fsb.known_families = NULL;
}

static void Destroy(Widget widget)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) widget;

    /* Lots of stuff to destroy! */

    if (fsb->fsb.gstate != 0) XDPSFreeContextGState(fsb->fsb.context,
						    fsb->fsb.gstate);
    if (fsb->fsb.preview_string != NULL) XtFree(fsb->fsb.preview_string);
    if (fsb->fsb.default_resource_path != NULL) {
	XtFree(fsb->fsb.default_resource_path);
    }
    if (fsb->fsb.resource_path_override != NULL) {
	XtFree(fsb->fsb.resource_path_override);
    }

    FreeFontLists(fsb);
}

static void Resize(Widget widget)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) widget;

    XtResizeWidget(fsb->fsb.pane_child, fsb->core.width, fsb->core.height, 0);
}

/* ARGSUSED */

static Boolean SetValues(
    Widget old, Widget req, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    FontSelectionBoxWidget oldfsb = (FontSelectionBoxWidget) old;
    FontSelectionBoxWidget newfsb = (FontSelectionBoxWidget) new;
    Boolean refreshLists = False, setSelection = False, do_preview = False;
    Bool inited;

#define NE(field) newfsb->fsb.field != oldfsb->fsb.field
#define DONT_CHANGE(field) \
    if (NE(field)) newfsb->fsb.field = oldfsb->fsb.field;

    DONT_CHANGE(typographic_sort);
    DONT_CHANGE(pane_child);
    DONT_CHANGE(preview_child);
    DONT_CHANGE(panel_child);
    DONT_CHANGE(family_label_child);
    DONT_CHANGE(family_multiple_label_child);
    DONT_CHANGE(family_scrolled_list_child);
    DONT_CHANGE(face_label_child);
    DONT_CHANGE(face_multiple_label_child);
    DONT_CHANGE(face_scrolled_list_child);
    DONT_CHANGE(size_label_child);
    DONT_CHANGE(size_text_field_child);
    DONT_CHANGE(size_option_menu_child);
    DONT_CHANGE(preview_button_child);
    DONT_CHANGE(sampler_button_child);
    DONT_CHANGE(separator_child);
    DONT_CHANGE(ok_button_child);
    DONT_CHANGE(apply_button_child);
    DONT_CHANGE(reset_button_child);
    DONT_CHANGE(cancel_button_child);
#undef DONT_CHANGE

    if (newfsb->fsb.size_count > 0 && newfsb->fsb.sizes == NULL) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontBox", "sizeMismatch",
			"FontSelectionBoxError",
			"Size count specified but no sizes present",
			(String *) NULL, (Cardinal *) NULL);
	newfsb->fsb.size_count = 0;
    }

    if (newfsb->fsb.size_count < 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontBox", "negativeSize",
			"FontSelectionBoxError",
			"Size count should not be negative",
			(String *) NULL, (Cardinal *) NULL);
	newfsb->fsb.size_count = 0;
    }

    if (newfsb->fsb.max_pending_deletes <= 0) {
	XtAppWarningMsg(XtWidgetToApplicationContext(new),
			"setValuesFontBox", "nonPositivePendingDelete",
			"FontSelectionBoxError",
			"Pending delete max must be positive",
			(String *) NULL, (Cardinal *) NULL);
	newfsb->fsb.max_pending_deletes = 1;
    }

    if (NE(preview_string)) {
	XtFree(oldfsb->fsb.preview_string);
	newfsb->fsb.preview_string = XtNewString(newfsb->fsb.preview_string);
	do_preview = True;
    }

    if (NE(default_resource_path)) {
	XtFree(oldfsb->fsb.default_resource_path);
	newfsb->fsb.default_resource_path =
		XtNewString(newfsb->fsb.default_resource_path);
	refreshLists = True;
    }

    if (NE(resource_path_override)) {
	XtFree(oldfsb->fsb.resource_path_override);
	newfsb->fsb.resource_path_override =
		XtNewString(newfsb->fsb.resource_path_override);
	refreshLists = True;
    }

    if (newfsb->fsb.undef_unused_fonts) UndefSomeUnusedFonts(newfsb, False);

    if (NE(context)) {
	if (newfsb->fsb.context == NULL) {
	    newfsb->fsb.context = XDPSGetSharedContext(XtDisplay(newfsb));
	} 
	if (_XDPSTestComponentInitialized(newfsb->fsb.context,
					  dps_init_bit_fsb, &inited) ==
	    dps_status_unregistered_context) {
	    XDPSRegisterContext(newfsb->fsb.context, False);
	}
	if (!inited) {
	    (void) _XDPSSetComponentInitialized(newfsb->fsb.context,
						dps_init_bit_fsb);
	    _DPSFDefineFontEnumFunctions(newfsb->fsb.context);
	}
    }	

    if (refreshLists) {
	UndefUnusedFonts((Widget)newfsb);
	newfsb->fsb.pending_delete_font = NULL;
	newfsb->fsb.pending_delete_count = 0;
	FreeFontLists(newfsb);
	GetFontNames(newfsb);
	DisplayFontFamilies(newfsb);
	setSelection = True;
    }

    if (NE(sizes)) {
	CreateSizeMenu(newfsb, True);
	setSelection = True;
    }

    if (NE(show_sampler)) {
	if (newfsb->fsb.show_sampler) ShowSampler(newfsb);
	else XtPopdown(newfsb->fsb.sampler);
    }

    if (NE(show_sampler_button)) {
	if (newfsb->fsb.show_sampler_button) {
	    XtManageChild(newfsb->fsb.sampler_button_child);
	} else XtUnmanageChild(newfsb->fsb.sampler_button_child);
    }

    if (NE(font_size)) newfsb->fsb.currently_selected_size = 0.0;

    if (NE(use_font_name) || NE(font_name) || NE(font_family) ||
	NE(font_face) || NE(font_size) || NE(font_name_multiple) ||
	NE(font_family_multiple) || NE(font_face_multiple) ||
	NE(font_size_multiple) || NE(font_blend)) setSelection = True;

    if (setSelection) SetUpCurrentSelections(newfsb);
    else if (do_preview && newfsb->fsb.preview_on_change) {
	DoPreview(newfsb, False);
    }

    if ((NE(font_name) || NE(font_size)) &&
	XtIsSensitive(newfsb->fsb.reset_button_child)) {

	if ((newfsb->fsb.font_size_multiple ||
	     newfsb->fsb.font_size == newfsb->fsb.currently_selected_size) &&
	    (newfsb->fsb.font_name_multiple ||
	     newfsb->fsb.currently_selected_face == NULL ||
	     newfsb->fsb.font_name ==
		    newfsb->fsb.currently_selected_face->font_name)) {
	    DesensitizeReset(newfsb);
	}
    }

    return False;
#undef NE
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
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;

    w->core.width = fsb->composite.children[0]->core.width;
    w->core.height = fsb->composite.children[0]->core.height;
}

static void SetFontName(
    Widget w,
    String name,
    Bool name_multiple)
{
    XtVaSetValues(w, XtNfontName, name, XtNuseFontName, True,
		  XtNfontNameMultiple, name_multiple, NULL);
}

void FSBSetFontName(
    Widget w,
    String name,
    Bool name_multiple)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->fsb_class.set_font_name)
	    (w, name, name_multiple);
}

static void SetFontFamilyFace(
    Widget w,
    String family, String face,
    Bool family_multiple, Bool face_multiple)
{
    XtVaSetValues(w, XtNfontFamily, family, XtNfontFace, face,
		  XtNuseFontName, False,
		  XtNfontFamilyMultiple, family_multiple,
		  XtNfontFaceMultiple, face_multiple, NULL);
}

void FSBSetFontFamilyFace(
    Widget w,
    String family, String face,
    Bool family_multiple, Bool face_multiple)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.set_font_family_face)
	    (w, family, face, family_multiple, face_multiple);
}

static void SetFontSize(
    Widget w,
    double size,
    Bool size_multiple)
{
    int i;
    Arg args[2];

    union {
	int i;
	float f;
    } kludge;

    kludge.f = size;

    i = 0;
    if (sizeof(float) > sizeof(XtArgVal)) {
	XtSetArg(args[i], XtNfontSize, &kludge.f);			i++;
    } else XtSetArg(args[i], XtNfontSize, kludge.i);			i++;
    XtSetArg(args[i], XtNfontSizeMultiple, size_multiple);		i++;
    XtSetValues(w, args, i);
}

void FSBSetFontSize(
    Widget w,
    double size,
    Bool size_multiple)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->fsb_class.set_font_size)
	    (w, size, size_multiple);
}

static void RefreshFontList(Widget w)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;

    UndefUnusedFonts((Widget)fsb);
    fsb->fsb.pending_delete_font = NULL;
    fsb->fsb.pending_delete_count = 0;
    FreeFontLists(fsb);
    FreePSResourceStorage(True);
    GetFontNames(fsb);
    DisplayFontFamilies(fsb);
    SetUpCurrentSelections(fsb);
}

void FSBRefreshFontList(
    Widget w)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.refresh_font_list) (w);
}

static void GetFamilyList(
    Widget w,
    int *count,
    String **list)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    String *buf;
    FontFamilyRec *ff;

    *count = fsb->fsb.family_count;
    *list = buf = (String *) XtMalloc(*count * sizeof(String));
    
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	*buf++ = ff->family_name;
    }
}

void FSBGetFamilyList(
    Widget w,
    int *count,
    String **list)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.get_family_list) (w, count, list);
}

static void GetFaceList(
    Widget w,
    String family,
    int *count,
    String **face_list, String **font_list)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    String *buf1, *buf2;
    FontFamilyRec *ff;
    FontRec *f;
    
    family = Canonical(family);
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	if (ff->family_name == family) break;
    }

    if (ff == NULL) {
	*count = 0;
	*face_list = *font_list = NULL;
	return;
    }

    *count = ff->font_count;
    *face_list = buf1 = (String *) XtMalloc(*count * sizeof(String));
    *font_list = buf2 = (String *) XtMalloc(*count * sizeof(String));

    for (f = ff->fonts; f != NULL; f = f->next) {
	*buf1++ = f->face_name;
	*buf2++ = f->font_name;
    }
}

void FSBGetFaceList(
    Widget w,
    String family,
    int *count_return,
    String **face_list, String **font_list)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.get_face_list) (w, family, count_return,
					      face_list, font_list);
}

void FSBUndefineUnusedFonts(
    Widget w)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.undef_unused_fonts) (w);
}

static Boolean DownloadFontName(Widget w, String name)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    FontFamilyRec *ff;
    FontRec *f;
    Boolean ret;

    name = Canonical(name);
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == name) {
		if (!fsb->fsb.get_server_fonts) {
		    int resident;
		    _DPSFIsFontResident(fsb->fsb.context, f->font_name,
					&resident);
		    if (resident) f->resident = True;
		}
		if (f->resident) return True;
		else {
		    ret = DownloadFont(fsb, name, fsb->fsb.context,
				       fsb->fsb.make_fonts_shared);
		    if (fsb->fsb.make_fonts_shared && ret) f->resident = True;
		    return ret;
		}
	    }
	}
    }
 
    return DownloadFont(fsb, name, fsb->fsb.context,
			fsb->fsb.make_fonts_shared);
}

Boolean FSBDownloadFontName(
    Widget w,
    String name)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    if (name == NULL) return False;
    return (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.download_font_name) (w, name);
}

static Boolean MatchFontFace(
    Widget w,
    String old_face, String new_family,
    String *new_face)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    FSBFaceSelectCallbackRec rec;
    String *faces;
    int i;
    FontFamilyRec *ff;
    FontRec *f;
    Boolean retVal;

    new_family = Canonical(new_family);
    old_face = Canonical(old_face);
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	if (ff->family_name == new_family) break;
    }
    if (ff == NULL) {
	*new_face = NULL;
	return False;
    }

    faces = (String *) XtMalloc(ff->font_count * sizeof(String));
    i = 0;
    for (f = ff->fonts; f != NULL; f = f->next) faces[i++] = f->face_name;

    rec.available_faces = faces;
    rec.num_available_faces = ff->font_count;
    rec.current_face = old_face;
    rec.new_face = NULL;

    i = MatchFaceName(&rec, &retVal);
    *new_face = faces[i];
    XtFree((XtPointer) faces);
    return !retVal;
}

Boolean FSBMatchFontFace(
    Widget w,
    String old_face, String new_family,
    String *new_face)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    return (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.match_font_face) (w, old_face,
						new_family, new_face);
}

static void FontNameToFamilyFaceBlend(
    Widget w,
    String font_name,
    String *family, String *face, String *blend)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;

    font_name = Canonical(font_name);
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == font_name) {
		*family = ff->family_name;
		*face = f->face_name;
		*blend = NULL;
		return;
	    }
	    if (f->blend_data != NULL) {
		for (b = f->blend_data->blends; b != NULL; b = b->next) {
		    if (b->font_name == font_name) {
			*family = ff->family_name;
			*face = f->face_name;
			*blend = b->blend_name;
			return;
		    }
		}
	    }
	}
    }

    *family = NULL;
    *face = NULL;
    *blend = NULL;
}

static void FontNameToFamilyFace(
    Widget w,
    String font_name,
    String *family, String *face)
{
    String blend;

    FontNameToFamilyFaceBlend(w, font_name, family, face, &blend);
}

void FSBFontNameToFamilyFace(
    Widget w,
    String font_name,
    String *family, String *face)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.font_name_to_family_face) (w, font_name,
							 family, face);
}

static void FontFamilyFaceBlendToName(
    Widget w,
    String family, String face, String blend,
    String *font_name)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;

    family = Canonical(family);
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	if (ff->family_name == family) break;
    }
    if (ff == NULL) {
	*font_name = NULL;
	return;
    }

    face = Canonical(face);
    for (f = ff->fonts; f != NULL; f = f->next) {
	if (f->face_name == face) break;
    }
    if (f == NULL) {
	*font_name = NULL;
	return;
    }

    if (blend == NULL) {
	*font_name = f->font_name;
	return;
    }
    if (f->blend_data == NULL) {
	*font_name = NULL;
	return;
    }

    blend = Canonical(blend);
    for (b = f->blend_data->blends; b != NULL; b = b->next) {
	if (b->blend_name == blend) {
	    *font_name = b->font_name;
	    return;
	}
    }
    *font_name = NULL;
}

static void FontFamilyFaceToName(
    Widget w,
    String family, String face,
    String *font_name)
{
    FontFamilyFaceBlendToName(w, family, face, NULL, font_name);
}

void FSBFontFamilyFaceToName(
    Widget w,
    String family, String face,
    String *font_name)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass)
       XtClass(w))->fsb_class.font_family_face_to_name) (w, family, face,
							 font_name);
}

String FSBFindAFM(
    Widget w,
    String font_name)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    return (*((FontSelectionBoxWidgetClass) XtClass(w))->
	    fsb_class.find_afm) (w, font_name);
}

String FSBFindFontFile(
    Widget w,
    String font_name)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    return (*((FontSelectionBoxWidgetClass) XtClass(w))->
	    fsb_class.find_font_file) (w, font_name);
}

static void GetTextDimensions(
    Widget w,
    String text, String font,
    double size, double x, double y,
    float *dx, float *dy, float *left, float *right, float *top, float *bottom)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    int bogusFont;

    _DPSFGetTextDimensions(fsb->fsb.context, text, font, size, x, y,
			   dx, dy, left, right, top, bottom, &bogusFont);
}

void FSBGetTextDimensions(
    Widget w,
    String text, String font,
    double size, double x, double y,
    float *dx, float *dy, float *left, float *right, float *top, float *bottom)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->
	fsb_class.get_text_dimensions) (w, text, font, size, x, y,
					dx, dy, left, right, top, bottom);
}

static void SetFontFamilyFaceBlend(
    Widget w,
    String family,
    String face,
    String blend,
    Bool family_multiple,
    Bool face_multiple)
{
    XtVaSetValues(w, XtNfontFamily, family, XtNfontFace, face,
		  XtNfontBlend, blend, XtNuseFontName, False,
		  XtNfontFamilyMultiple, family_multiple,
		  XtNfontFaceMultiple, face_multiple, NULL);
}

void FSBSetFontFamilyFaceBlend(
    Widget w,
    String font_family,
    String font_face,
    String font_blend,
    Bool font_family_multiple,
    Bool font_face_multiple)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->
        fsb_class.set_font_family_face_blend) (w, font_family, font_face,
					       font_blend,
					       font_family_multiple,
					       font_face_multiple);
}

void FSBFontNameToFamilyFaceBlend(
    Widget w,
    String font_name,
    String *family,
    String *face,
    String *blend)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->
        fsb_class.font_name_to_family_face_blend) (w, font_name, family,
						   face, blend);
}
    
void FSBFontFamilyFaceBlendToName(
    Widget w,
    String family,
    String face,
    String blend,
    String *font_name)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->
        fsb_class.font_family_face_blend_to_name) (w, family, face,
						   blend, font_name);
}

static void GetBlendList(
    Widget w,
    String name,
    int *count_return,
    String **blend_return,
    String **font_name_return,
    float **axis_values_return)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    String *buf1, *buf2;
    float *buf3;
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;
    int i;

    name = Canonical(name);
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == name) break;
	}
    }

    if (ff == NULL || f == NULL || f->blend_data == NULL) {
	*count_return = 0;
	*blend_return = *font_name_return = NULL;
	*axis_values_return = NULL;
	return;
    }
	
    *count_return = f->blend_count;
    *blend_return = buf1 = (String *) XtMalloc(*count_return * sizeof(String));
    *font_name_return = buf2 =
	    (String *) XtMalloc(*count_return * sizeof(String));
    *axis_values_return = buf3 =
	    (float *) XtMalloc(*count_return * MAX_AXES * sizeof(float));


    for (b = f->blend_data->blends; b != NULL; b = b->next) {
	*buf1++ = b->blend_name;
	*buf2++ = b->font_name;
	for (i = 0; i < MAX_AXES; i++) *buf3++ = b->data[i];
    }
}

void FSBGetBlendList(
    Widget w,
    String name,
    int *count_return,
    String **blend_return,
    String **font_name_return,
    float **axis_values_return)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->
        fsb_class.get_blend_list) (w, name, count_return, blend_return,
				   font_name_return, axis_values_return);
}

static void GetBlendInfo(
    Widget w,
    String name,
    int *num_axes_return,
    int *num_designs_return,
    String **axis_names_return,
    float **blend_positions_return,
    int **blend_map_count_return,
    int **blend_design_coords_return,
    float **blend_normalized_coords_return)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    FontFamilyRec *ff;
    FontRec *f;
    BlendDataRec *bd;
    int i, j;
    float *fbuf;
    int *ibuf;
    String *sbuf;
    int coords;

    name = Canonical(name);
    if (fsb->fsb.currently_selected_face->font_name == name) {
	bd = fsb->fsb.currently_selected_face->blend_data;
    } else {

	for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	    for (f = ff->fonts; f != NULL; f = f->next) {
		if (f->font_name == name) goto FOUND_IT;
	     }
	}
	*num_axes_return = *num_designs_return = 0;
	*axis_names_return = NULL;
	*blend_positions_return = *blend_normalized_coords_return = NULL;
	*blend_map_count_return = *blend_design_coords_return = NULL;
	return;

FOUND_IT:
	bd = f->blend_data;
    }

    *num_axes_return = bd->num_axes;
    *num_designs_return = bd->num_designs;

    *axis_names_return = sbuf =
	    (String *) XtMalloc(bd->num_axes * sizeof(String));
    *blend_map_count_return = ibuf =
	    (int *) XtMalloc(bd->num_axes * sizeof(int));
    coords = 0;
    for (i = 0; i < bd->num_axes; i++) {
	*sbuf++ = bd->name[i];
	*ibuf++ = bd->internal_points[i] + 2;
	coords += bd->internal_points[i] + 2;
    }

    *blend_positions_return = fbuf =
	    (float *) XtMalloc(bd->num_axes * bd->num_designs * sizeof(float));
    for (i = 0; i < bd->num_axes * bd->num_designs; i++) {
	*fbuf++ = bd->design_positions[i];
    }

    *blend_design_coords_return = ibuf =
	    (int *) XtMalloc(coords * sizeof(int));
    *blend_normalized_coords_return = fbuf =
	    (float *) XtMalloc(coords * sizeof(float));

    for (i = 0; i < bd->num_axes; i++) {
	*ibuf++ = bd->min[i];
	*fbuf++ = 0.0;
	for (j = 0; j < bd->internal_points[i]; j++) {
	    *ibuf++ = bd->internal_break[i][j];
	    *fbuf++ = bd->internal_value[i][j];
	}
	*ibuf++ = bd->max[i];
	*fbuf++ = 1.0;
    }
}

void FSBGetBlendInfo(
    Widget w,
    String name,
    int *num_axes_return,
    int *num_designs_return,
    String **axis_names_return,
    float **blend_positions_return,
    int **blend_map_count_return,
    int **blend_design_coords_return,
    float **blend_normalized_coords_return)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    (*((FontSelectionBoxWidgetClass) XtClass(w))->
        fsb_class.get_blend_info) (w, name, num_axes_return,
				   num_designs_return, axis_names_return,
				   blend_positions_return,
				   blend_map_count_return,
				   blend_design_coords_return,
				   blend_normalized_coords_return);
}

static Boolean ChangeBlends(
    Widget w,
    String base_name,
    String blend_name,
    FSBBlendAction action,
    int *axis_values,
    float *axis_percents)
{
    FontSelectionBoxWidget fsb = (FontSelectionBoxWidget) w;
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b = NULL, *newb, **lastb;
    BlendDataRec *bd;
    String spaceBlend;
    int val[4];
    float pct[4];
    int i;

    base_name = Canonical(base_name);
    blend_name = Canonical(blend_name);

    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == base_name) {
		if ((bd = f->blend_data) == NULL) return False;

		for (b = f->blend_data->blends; b != NULL; b = b->next) {
		    if (b->blend_name == blend_name) break;
		}
		goto FOUND_BASE;
	    }
	}
    }
    return False;

FOUND_BASE:
    if (action != FSBDeleteBlend) { 
	if (axis_values != NULL) {
	    for (i = 0; i < bd->num_axes; i++) {
		val[i] = axis_values[i];
		pct[i] = _FSBNormalize(val[i], bd, i);
	    }
	    for (/**/; i < 4; i++) pct[i] = 0.0;
	} else {
	    if (axis_percents == NULL) return False;
	    for (i = 0; i < bd->num_axes; i++) {
		pct[i] = axis_percents[i];
		val[i] = _FSBUnnormalize(pct[i], bd, i);
	    }
	    for (/**/; i < 4; i++) pct[i] = 0.0;
	}
    }

    switch (action) {
	case FSBAddBlend:
	    if (b != NULL) return False;
	    newb = XtNew(BlendRec);
	    newb->blend_name = blend_name;
	    newb->CS_blend_name = CS(blend_name, (Widget) fsb);

	    spaceBlend = (char *) XtMalloc(strlen(blend_name) + 4);
	    spaceBlend[0] = spaceBlend[1] = spaceBlend[2] = ' ';
	    strcpy(spaceBlend+3, blend_name);
	    newb->CS_space_blend_name = CS(spaceBlend, (Widget) fsb);
	    XtFree((XtPointer) spaceBlend);

	    for (i = 0; i < MAX_AXES; i++) newb->data[i] = pct[i];
	    newb->font_name = _FSBGenFontName(base_name, val, bd);

	    f->blend_count++;
	    ff->blend_count++;

	    lastb = &bd->blends;
	    for (b = bd->blends; b != NULL; b = b->next) {
		if (strcmp(blend_name, b->blend_name) < 0) break;
		lastb = &b->next;
	    }

	    newb->next = b;
	    *lastb = newb;
	    break;

	case FSBReplaceBlend:
	    if (b == NULL) return False;

	    for (i = 0; i < MAX_AXES; i++) b->data[i] = pct[i];
	    b->font_name = _FSBGenFontName(base_name, val, bd);
	    if (b == fsb->fsb.currently_previewed_blend) DoPreview(fsb, False);

	    break;

	case FSBDeleteBlend:
	    if (b == NULL) return False;

	    if (bd->blends == b) {
		bd->blends = b->next;
	    } else {
		for (newb = bd->blends; newb->next != b; newb = newb->next) {}
		newb->next = b->next;
	    }

	    f->blend_count--;
	    ff->blend_count--;

	    /* Don't actually delete the blend record, in case it's displayed
	       in the sampler. */
	    break;
    }
    if (f->in_font_creator) _FSBSetCreatorFamily(fsb->fsb.creator, ff);
    if (ff == fsb->fsb.currently_selected_family) SetUpFaceList(fsb, ff);
    fsb->fsb.blends_changed = True;
    WriteBlends(fsb);
    return True;
}

Boolean FSBChangeBlends(
    Widget w,
    String base_name,
    String blend_name,
    FSBBlendAction action,
    int *axis_values,
    float *axis_percents)
{
    XtCheckSubclass(w, fontSelectionBoxWidgetClass, NULL);

    return (*((FontSelectionBoxWidgetClass) XtClass(w))->
	    fsb_class.change_blends) (w, base_name, blend_name, action,
				    axis_values, axis_percents);
}

void _FSBSetCurrentFont(
    FontSelectionBoxWidget fsb,
    String name)
{
    FontFamilyRec *ff;
    FontRec *f;
    BlendRec *b;
    int i, j;

    fsb->fsb.current_family_multiple = False;
    fsb->fsb.current_face_multiple = False;
    UnmanageFamilyMultiple(fsb);
    UnmanageFaceMultiple(fsb);

    name = Canonical(name);
    i = 1;
    for (ff = fsb->fsb.known_families; ff != NULL; ff = ff->next) {
	j = 1;
	for (f = ff->fonts; f != NULL; f = f->next) {
	    if (f->font_name == name) {
		b = NULL;
		goto FOUND_NAME;
	    }
	    j++;
	    if (f->blend_data != NULL && f->blend_data->blends != NULL) {
		for (b = f->blend_data->blends; b != NULL; b = b->next) {
		    if (b->font_name == name) {
			goto FOUND_NAME;
		    }
		    j++;
		}
	    }

	}
	i++;
    }
    return;
FOUND_NAME:
    SetUpFaceList(fsb, ff);
    ListSelectPos(fsb->fsb.family_scrolled_list_child, i, False);
    ListSelectPos(fsb->fsb.face_scrolled_list_child, j, False);
    fsb->fsb.currently_selected_face = f;
    fsb->fsb.currently_selected_family = ff;
    fsb->fsb.currently_selected_blend = b;
    SensitizeReset(fsb);
    DoPreview(fsb, False);
}

float _FSBNormalize(
    int val,
    BlendDataRec *bd,
    int i)
{
    int j;
    int lessBreak, moreBreak;
    float lessValue, moreValue;

    if (bd->internal_points[i] == 0) {
	return ((float) (val - bd->min[i])) /
		((float) (bd->max[i] - bd->min[i]));
    }

    /* Find the largest breakpoint less than val and the smallest one greater
       than it */

    lessBreak = bd->min[i];
    lessValue = 0.0;
    moreBreak = bd->max[i];
    moreValue = 1.0;

    for (j = 0; j < bd->internal_points[i]; j++) {
	if (bd->internal_break[i][j] > lessBreak &&
	    bd->internal_break[i][j] <= val) {
	    lessBreak = bd->internal_break[i][j];
	    lessValue = bd->internal_value[i][j];
	}
	if (bd->internal_break[i][j] < moreBreak &&
	    bd->internal_break[i][j] >= val) {
	    moreBreak = bd->internal_break[i][j];
	    moreValue = bd->internal_value[i][j];
	}
    }

    if (moreBreak == lessBreak) return moreValue;

    return lessValue + (moreValue - lessValue) *
	    ((float) (val - lessBreak)) / ((float) (moreBreak - lessBreak));
}

int _FSBUnnormalize(val, bd, i)
    float val;
    BlendDataRec *bd;
    int i;
{
    int j;
    int lessBreak, moreBreak;
    float lessValue, moreValue;

    if (bd->internal_points[i] == 0) {
	return val * (bd->max[i] - bd->min[i]) + bd->min[i] + 0.5;
    }

    /* Find the largest breakpoint less than val and the smallest one greater
       than it */

    lessBreak = bd->min[i];
    lessValue = 0.0;
    moreBreak = bd->max[i];
    moreValue = 1.0;

    for (j = 0; j < bd->internal_points[i]; j++) {
	if (bd->internal_value[i][j] > lessValue &&
	    bd->internal_value[i][j] <= val) {
	    lessBreak = bd->internal_break[i][j];
	    lessValue = bd->internal_value[i][j];
	}
	if (bd->internal_value[i][j] < moreBreak &&
	    bd->internal_value[i][j] >= val) {
	    moreBreak = bd->internal_break[i][j];
	    moreValue = bd->internal_value[i][j];
	}
    }

    if (moreBreak == lessBreak) return moreBreak;

    return ((float) (val - lessValue)) / ((float) (moreValue - lessValue)) *
	    (moreBreak - lessBreak) + lessBreak + 0.5;
}

String _FSBGenFontName(
    String name,
    int *val,
    BlendDataRec *bd)
{
    char nameBuf[256];
    int i;
    char *ch;

    strcpy(nameBuf, name);
    ch = nameBuf + strlen(nameBuf);

    for (i = 0; i < bd->num_axes; i++) {
	sprintf(ch, "_%d_%s", val[i], bd->name[i]);
	ch = ch + strlen(ch);
    }

    return Canonical(nameBuf);
}
