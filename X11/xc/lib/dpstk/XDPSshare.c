/* 
 * XDPSshare.c
 *
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/lib/dpstk/XDPSshare.c,v 1.3 2000/09/26 15:57:00 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>

#include <DPS/dpsXclient.h>
#include <DPS/dpsops.h>
#include <DPS/XDPSlib.h>
#include <DPS/dpsXshare.h>

#include "XDPSswraps.h"
#include "dpsXcommonI.h"

static int extensionId = 0;

/* 
   Alloc...Info allocates an info entry and stores it at the head of the list.
   Find...Info looks for an info entry and returns NULL if not found.
   Lookup...Info looks for an info entry and creates one if not found.
*/

typedef struct _ContextInfoRec {
    int extensionId;
    DPSContextExtensionRec next;
    DPSContext text;
    Bool enableText;
    unsigned long initFlags;
    struct _DisplayInfoRec *displayInfo;
} ContextInfoRec, *ContextInfo;
   
typedef enum {ext_yes, ext_no, ext_no_idea} ExtensionStatus;

typedef struct _DisplayInfoRec {
    Display *display;
    ExtensionStatus extensionPresent;
    DPSContext defaultContext;
    int *depthsForScreen;
    int **validDepths;
    GC **gcForDepth;
    struct _DisplayInfoRec *next;
} DisplayInfoRec, *DisplayInfo;

/* If a display is in displayList, it means that we have looked to see if
   the extension exists on the display.  If context is not NULL, the
   display has a default context associated with it. */

static DisplayInfo displayList = NULL;

static DisplayInfo LookupDisplayInfo(Display *display);

static ContextInfo AllocContextInfo(DPSContext context)
{
    ContextInfo c = (ContextInfo) calloc(1, sizeof(ContextInfoRec));

    if (extensionId == 0) extensionId = DPSGenerateExtensionRecID();

    c->extensionId = extensionId;
    DPSAddContextExtensionRec(context, (DPSContextExtensionRec *) c);

    return c;
}

static ContextInfo FindContextInfo(DPSContext context)
{
    if (extensionId == 0) extensionId = DPSGenerateExtensionRecID();

    return (ContextInfo) DPSGetContextExtensionRec(context, extensionId);
}

static ContextInfo RemoveContextInfo(DPSContext context)
{
    return (ContextInfo) DPSRemoveContextExtensionRec(context,
						      extensionId);
}

/* May only be called for a display in the display list. */

static ContextInfo LookupContext(
    Display *display,
    DPSContext context)
{
    ContextInfo c = FindContextInfo(context);

    if (c != NULL) return c;

    /* Create one */

    c = AllocContextInfo(context);
    c->displayInfo = LookupDisplayInfo(display);
    return c;
}

static DisplayInfo AllocDisplayInfo(
    Display *display,
    DPSContext context)
{
    DisplayInfo d = (DisplayInfo) malloc(sizeof(DisplayInfoRec));
    register int i;

    if (d == NULL) return NULL;
    d->next = displayList;
    displayList = d;

    d->display = display;
    d->defaultContext = context;
    d->extensionPresent = (context == NULL) ? ext_no_idea : ext_yes;

    d->depthsForScreen = (int *) calloc(ScreenCount(display), sizeof(int));
    d->validDepths = (int **) calloc(ScreenCount(display), sizeof(int *));
    d->gcForDepth = (GC **) calloc(ScreenCount(display), sizeof(GC *));

    for (i = 0; i < ScreenCount(display); i++) {
        d->validDepths[i] = XListDepths(display, i, &d->depthsForScreen[i]);
	d->gcForDepth[i] = (GC *) calloc(d->depthsForScreen[i], sizeof(GC));
    }

    return d;
}

static DisplayInfo FindDisplayInfo(Display *display)
{
    DisplayInfo d = displayList;

    while (d != NULL && d->display != display) d = d->next;
    return d;
}

static DisplayInfo LookupDisplayInfo(Display *display)
{
    DisplayInfo d = FindDisplayInfo(display);

    if (d == NULL) d = AllocDisplayInfo(display, (DPSContext) NULL);

    return d;
}

int _XDPSSetComponentInitialized(DPSContext context, unsigned long bit)
{
    ContextInfo c = FindContextInfo(context);

    if (c == NULL) return dps_status_unregistered_context;
    c->initFlags |= bit;
    return dps_status_success;
}

int _XDPSTestComponentInitialized(
    DPSContext context,
    unsigned long bit,
    Bool *result)
{
    ContextInfo c = FindContextInfo(context);

    if (c == NULL) {
	*result = False;
	return dps_status_unregistered_context;
    }
    *result = ((c->initFlags & bit) != 0);
    return dps_status_success;
}

int XDPSSetContextDepth(
    DPSContext context,
    Screen *screen,
    int depth)
{
    return XDPSSetContextParameters(context, screen, depth, None, 0,
				    (XDPSStandardColormap *) NULL,
				    (XDPSStandardColormap *) NULL,
				    XDPSContextScreenDepth);
}

int XDPSSetContextDrawable(
    DPSContext context,
    Drawable drawable,
    int height)
{
    if (drawable != None && height <= 0) return dps_status_illegal_value;
    _DPSSSetContextDrawable(context, drawable, height);
    return dps_status_success;
}

int XDPSSetContextRGBMap(
    DPSContext context,
    XDPSStandardColormap *map)
{
    return XDPSSetContextParameters(context, (Screen *) NULL, 0, None, 0,
				    map, (XDPSStandardColormap *) NULL,
				    XDPSContextRGBMap);
}

int XDPSSetContextGrayMap(
    DPSContext context,
    XDPSStandardColormap *map)
{
    return XDPSSetContextParameters(context, (Screen *) NULL, 0, None, 0,
				    map, (XDPSStandardColormap *) NULL,
				    XDPSContextGrayMap);
}

static GC DisplayInfoSharedGC(DisplayInfo d, Screen *screen, int depth)
{
    int s = XScreenNumberOfScreen(screen);
    register int i;
    XGCValues v;
    Pixmap p;

    if (s >= ScreenCount(DisplayOfScreen(screen))) return NULL;
	  
    for (i = 0; i < d->depthsForScreen[s] &&
	       d->validDepths[s][i] != depth; i++) {}
	  
    if (i >= d->depthsForScreen[s]) return NULL;
	  
    if (d->gcForDepth[s][i] == 0) {	/* Not "None" -- used calloc */
	if (depth == DefaultDepthOfScreen(screen)) {
	    d->gcForDepth[s][i] = XCreateGC(d->display,
					    RootWindowOfScreen(screen), 0, &v);
	} else {
	    p = XCreatePixmap(d->display,
			      RootWindowOfScreen(screen),
			      1, 1, depth);
	    d->gcForDepth[s][i] = XCreateGC(d->display, p, 0, &v);
	    XFreePixmap(d->display, p);
	}
    }

    return d->gcForDepth[s][i];
}

int XDPSSetContextParameters(
    DPSContext context,
    Screen *screen,
    int depth,
    Drawable drawable,
    int height,
    XDPSStandardColormap *rgbMap,
    XDPSStandardColormap *grayMap,
    unsigned int flags)
{
    ContextInfo c = FindContextInfo(context);
    Bool doDepth = False, doDrawable = False, doRGB = False, doGray = False;
    Colormap map = None;
    XStandardColormap cmap;
    GC gc;
    GContext gctx = None;
    DisplayInfo d;
    Display *dpy;
    int rgb_base_pixel = 0;
    int red_max = 0;
    int red_mult = 0;
    int green_max = 0;
    int green_mult = 0;
    int blue_max = 0;
    int blue_mult = 0;
    int gray_base_pixel = 0;
    int gray_max = 0;
    int gray_mult = 0;
    
    if (c == NULL) return dps_status_unregistered_context;
    d = c->displayInfo;

    (void) XDPSXIDFromContext(&dpy, context);

    if (flags & XDPSContextScreenDepth) {
	doDepth = True;

	if (DisplayOfScreen(screen) != dpy) {
	    return dps_status_illegal_value;
	}

	gc = DisplayInfoSharedGC(d, screen, depth);
	if (gc == NULL) return dps_status_illegal_value;

	gctx = XGContextFromGC(gc);
    }

    if (flags & XDPSContextDrawable) {
	doDrawable = True;
	if (drawable != None && height <= 0) return dps_status_illegal_value;
    }

    if (flags & XDPSContextRGBMap) {
	doRGB = True;
	if (rgbMap == NULL) {
	    XDPSGetDefaultColorMaps(dpy, screen, drawable, &cmap,
				    (XStandardColormap *) NULL);
	    rgb_base_pixel = cmap.base_pixel;
	    red_max = cmap.red_max;
	    red_mult = cmap.red_mult;
	    green_max = cmap.green_max;
	    green_mult = cmap.green_mult;
	    blue_max = cmap.blue_max;
	    blue_mult = cmap.blue_mult;
	    map = cmap.colormap;
	} else {
	    rgb_base_pixel = rgbMap->base_pixel;
	    red_max = rgbMap->red_max;
	    red_mult = rgbMap->red_mult;
	    green_max = rgbMap->green_max;
	    green_mult = rgbMap->green_mult;
	    blue_max = rgbMap->blue_max;
	    blue_mult = rgbMap->blue_mult;
	    map = rgbMap->colormap;
	}
    }

    if (flags & XDPSContextGrayMap) {
	doGray = True;
	if (grayMap == NULL) {
	    XDPSGetDefaultColorMaps(dpy, screen, drawable,
				    (XStandardColormap *) NULL, &cmap);
	    gray_base_pixel = cmap.base_pixel;
	    gray_max = cmap.red_max;
	    gray_mult = cmap.red_mult;
	    if (doRGB && map != cmap.colormap) {
		return dps_status_illegal_value;
	    } else map = cmap.colormap;
	} else {
	    gray_base_pixel = grayMap->base_pixel;
	    gray_max = grayMap->red_max;
	    gray_mult = grayMap->red_mult;
	    if (doRGB && map != grayMap->colormap) {
		return dps_status_illegal_value;
	    } else map = grayMap->colormap;
	}
    } 

    if (doDepth || doDrawable || doRGB || doGray) {
	_DPSSSetContextParameters(context, gctx, drawable, height, map,
				  rgb_base_pixel, red_max, red_mult,
				  green_max, green_mult, blue_max, blue_mult,
				  gray_base_pixel, gray_max, gray_mult,
				  doDepth, doDrawable, doRGB, doGray);
    }
    return dps_status_success;
}

int XDPSPushContextParameters(
    DPSContext context,
    Screen *screen,
    int depth,
    Drawable drawable,
    int height,
    XDPSStandardColormap *rgbMap,
    XDPSStandardColormap *grayMap,
    unsigned int flags,
    DPSPointer *pushCookieReturn)
{
    ContextInfo c = FindContextInfo(context);
    int status;

    if (c == NULL) return dps_status_unregistered_context;

    DPSgsave(context);

    status = XDPSSetContextParameters(context, screen, depth, drawable, height,
				      rgbMap, grayMap, flags);

    *pushCookieReturn = (DPSPointer) context;
    return status;
}

int XDPSPopContextParameters(DPSPointer pushCookie)
{
    DPSContext context = (DPSContext) pushCookie;
    ContextInfo c = FindContextInfo(context);

    if (c == NULL) return dps_status_illegal_value;

    DPSgrestore(context);

    return dps_status_success;
}
    
int XDPSCaptureContextGState(DPSContext context, DPSGState *gsReturn)
{
    *gsReturn = DPSNewUserObjectIndex();
    /* We want to keep 0 as an unassigned value */
    if (*gsReturn == 0) *gsReturn = DPSNewUserObjectIndex();

    _DPSSCaptureGState(context, *gsReturn);

    return dps_status_success;
}

int XDPSUpdateContextGState(DPSContext context, DPSGState gs)
{
    _DPSSUpdateGState(context, gs);

    return dps_status_success;
}

int XDPSFreeContextGState(DPSContext context, DPSGState gs)
{
    _DPSSUndefineUserObject(context, gs);

    return dps_status_success;
}

int XDPSSetContextGState(
    DPSContext context,
    DPSGState gs)
{
    _DPSSRestoreGState(context, gs);

    return dps_status_success;
}

int XDPSPushContextGState(
    DPSContext context,
    DPSGState gs,
    DPSPointer *pushCookieReturn)
{
    int status;

    DPSgsave(context);

    status = XDPSSetContextGState(context, gs);
    *pushCookieReturn = (DPSPointer) context;
    return status;
}

int XDPSPopContextGState(DPSPointer pushCookie)
{
    DPSContext context = (DPSContext) pushCookie;

    DPSgrestore(context);
    return dps_status_success;
}

void XDPSRegisterContext(DPSContext context, Bool makeSharedContext)
{
    Display *display;
    Bool inited;
    ContextInfo c;
    
    /* Get the display */
    (void) XDPSXIDFromContext(&display, context);

    if (makeSharedContext) {	/* Install as shared ctxt for this display */
        c = LookupContext(display, context);
	c->displayInfo->defaultContext = context;
    } else {			/* Just add to the context list */
        c = LookupContext(display, context);
    }

    c->displayInfo->extensionPresent = ext_yes;

    (void) _XDPSTestComponentInitialized(context, dps_init_bit_share, &inited);
    if (!inited) {
	(void) _XDPSSetComponentInitialized(context, dps_init_bit_share);
	_DPSSInstallDPSlibDict(context);
    }
}

DPSContext XDPSGetSharedContext(Display *display)
{
    DisplayInfo d = LookupDisplayInfo(display);
    ContextInfo c;
    DPSContext context;

    if (d->extensionPresent == ext_no) return NULL;

    if (d->defaultContext != NULL) context = d->defaultContext;
    else {
	context = XDPSCreateSimpleContext(display,
					  None, None, 0, 0,
					  DPSDefaultTextBackstop,
					  DPSDefaultErrorProc, NULL);
	if (context != NULL) {
	    c = AllocContextInfo(context);
	    d->defaultContext = context;
	    c->displayInfo = d;
	    (void) _XDPSSetComponentInitialized(context, dps_init_bit_share);
	    _DPSSInstallDPSlibDict(context);
	    (void) XDPSSetContextDepth(context,
				       DefaultScreenOfDisplay(display),
				       DefaultDepth(display,
						    DefaultScreen(display)));
	}
    }

    if (context == NULL) d->extensionPresent = ext_no;
    else d->extensionPresent = ext_yes;

    return context;
}

void XDPSDestroySharedContext(DPSContext context)
{
    ContextInfo c = RemoveContextInfo(context);

    if (c == NULL) return;

    if (c->displayInfo->defaultContext == context) {
	c->displayInfo->defaultContext = NULL;
    }
    DPSDestroySpace(DPSSpaceFromContext(context));	/* Also gets context */
    if (c->text != NULL) DPSDestroySpace(DPSSpaceFromContext(c->text));
    free((char *) c);
}

void XDPSUnregisterContext(DPSContext context)
{
    ContextInfo c = RemoveContextInfo(context);

    if (c == NULL) return;

    if (c->displayInfo->defaultContext == context) {
	c->displayInfo->defaultContext = NULL;
    }
    if (c->text != NULL) DPSDestroySpace(DPSSpaceFromContext(c->text));
    free((char *) c);
}

void XDPSFreeDisplayInfo(Display *display)
{
    DisplayInfo *dp = &displayList;
    DisplayInfo d;
    register int i, j;

    while (*dp != NULL && (*dp)->display != display) dp = &((*dp)->next);

    if (*dp == NULL) return;

    d = *dp;
    *dp = d->next;	/* remove from list */

    for (i = 0; i < ScreenCount(display); i++) {
#ifdef NO_XLISTDEPTHS
	free((char *) d->validDepths[i]);
#else
	XFree((char *) d->validDepths[i]);
#endif
	for (j = 0; j < d->depthsForScreen[i]; j++) {
	    if (d->gcForDepth[i][j] != 0) {
		XFreeGC(display, d->gcForDepth[i][j]);
	    }
	}
    }

    free((char *) d->depthsForScreen);
    free((char *) d->validDepths);
    free((char *) d->gcForDepth);
    free((char *) d);
}

int XDPSChainTextContext(DPSContext context, Bool enable)
{
    ContextInfo c = FindContextInfo(context);

    if (c == NULL) return dps_status_unregistered_context;

    /* Check if already in desired state */

    if (c->enableText == enable) return dps_status_success;

    if (enable) {
	if (c->text == NULL) {
	    c->text = DPSCreateTextContext(DPSDefaultTextBackstop,
					   DPSDefaultErrorProc);
	    if (c->text == NULL) return dps_status_no_extension;
	}
	DPSChainContext(context, c->text);
	c->enableText = True;
	return dps_status_success;
    }

    /* disabling, currently enabled */

    DPSUnchainContext(c->text);
    c->enableText = False;
    return dps_status_success;
}

Bool XDPSExtensionPresent(Display *display)
{
    DisplayInfo d = LookupDisplayInfo(display);

    if (d->extensionPresent != ext_no_idea) {
	return (d->extensionPresent == ext_yes);
    }

    /* Check if the extension is present by trying to initialize it */

    if (XDPSLInit(display, (int *) NULL, (char **) NULL) == -1) {
	d->extensionPresent = ext_no;
    } else d->extensionPresent = ext_yes;

    return (d->extensionPresent == ext_yes);
}

int PSDefineAsUserObj(void)
{
    return DPSDefineAsUserObj(DPSGetCurrentContext());
}

void PSRedefineUserObj(int uo)
{
    DPSRedefineUserObj(DPSGetCurrentContext(), uo);
}

void PSUndefineUserObj(int uo)
{
    DPSUndefineUserObj(DPSGetCurrentContext(), uo);
}

int DPSDefineAsUserObj(DPSContext ctxt)
{
    int out = DPSNewUserObjectIndex();
    /* We want to keep 0 as an unassigned value */
    if (out == 0) out = DPSNewUserObjectIndex();

    _DPSSDefineUserObject(ctxt, out);
    return out;
}

void DPSRedefineUserObj(DPSContext ctxt, int uo)
{
    _DPSSDefineUserObject(ctxt, uo);
}

void DPSUndefineUserObj(DPSContext ctxt, int uo)
{
    _DPSSUndefineUserObject(ctxt, uo);
}

int PSReserveUserObjIndices(int number)
{
    return DPSReserveUserObjIndices(DPSGetCurrentContext(), number);
}

int DPSReserveUserObjIndices(DPSContext ctxt, int number)
{
    int out = DPSNewUserObjectIndex();

    /* We want to keep 0 as an unassigned value */
    if (out == 0) out = DPSNewUserObjectIndex();

    number--;
    while (number-- > 0) (void) DPSNewUserObjectIndex();
    return out;
}

void PSReturnUserObjIndices(int start, int number)
{
    DPSReturnUserObjIndices(DPSGetCurrentContext(), start, number);
}

void DPSReturnUserObjIndices(DPSContext ctxt, int start, int number)
{
    /* Nothing left any more */
}

#ifdef NO_XLISTDEPTHS
/* This function copyright 1989 Massachusetts Institute of Technology */

/*
 * XListDepths - return info from connection setup
 */
int *XListDepths (
    Display *dpy,
    int scrnum,
    int *countp)
{
    Screen *scr;
    int count;
    int *depths;

    if (scrnum < 0 || scrnum >= dpy->nscreens) return NULL;

    scr = &dpy->screens[scrnum];
    if ((count = scr->ndepths) > 0) {
	register Depth *dp;
	register int i;

	depths = (int *) malloc (count * sizeof(int));
	if (!depths) return NULL;
	for (i = 0, dp = scr->depths; i < count; i++, dp++) 
	  depths[i] = dp->depth;
    } else {
	/* a screen must have a depth */
	return NULL;
    }
    *countp = count;
    return depths;
}
#endif /* NO_XLISTDEPTHS */
