/*
 * dpsXcmu.c -- Simple color management/allocation utility
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
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
 * Portions Copyright 1989 by the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided 
 * that the above copyright notice appear in all copies and that both that 
 * copyright notice and this permission notice appear in supporting 
 * documentation, and that the name of M.I.T. not be used in advertising
 * or publicity pertaining to distribution of the software without specific, 
 * written prior permission. M.I.T. makes no representations about the 
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Adobe Systems Incorporated and Donna Converse, MIT X Consortium
 */
/* $XFree86: xc/lib/dps/dpsXcmu.c,v 1.5 2001/07/25 15:04:54 dawes Exp $ */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xos.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>

#include "dpsassert.h"
#include "cslibint.h"

/* Defines for standard colormap routines */

#define PrivSort qsort
#include <stddef.h>

static char redsName[] = "reds";
static char greensName[] = "greens";
static char bluesName[] = "blues";
static char graysName[] = "grays";

/* Database containing DPS default color cube values */

typedef struct _dpyRec {
    Display *dpy;
    XrmDatabase db;
    Atom XA_GRAY_DEFAULT_MAP;
    struct _dpyRec *next;
} DpyRec;

static DpyRec *dpyRec = NULL;
static DpyRec *curDpyRec;
static DpyRec *FindDpyRec(Display *);

typedef struct {
    unsigned long *pixels;
    int npixels;
} PixelRec;

static Bool AllocateColor(Display *, Colormap, XColor *);
static Bool AllocateColormap(Display *, XStandardColormap *, XVisualInfo *, int *, PixelRec *, int *, int *, unsigned long);
static Bool CheckCube(XColor *, XColor *, XStandardColormap *);
static Bool CubicCube(XStandardColormap *);
static Bool GetColorCubeFromProperty(Display *, XVisualInfo *, XStandardColormap *, XStandardColormap **, int *);
static Bool GetGrayRampFromProperty(Display *, XVisualInfo *, XStandardColormap *, XStandardColormap **, int *);
Status XDPSCreateStandardColormaps(Display *, Drawable, Visual *, int, int, int, int, XStandardColormap *, XStandardColormap *, Bool);
static Status contiguous(unsigned long *, int, int *, unsigned long, int *, int *);
static XVisualInfo *PickCorrectVisual(Display *, XVisualInfo *, int, Colormap);
static int FindRampSize(XColor *, XColor *);
static long NumColors(char *, char *, char *);
static void AllocateColorCube(Display *, XVisualInfo *, XStandardColormap *, PixelRec *);
static void AllocateGrayRamp(Display *, XVisualInfo *, XStandardColormap *, XStandardColormap *, PixelRec *);
static void ColorValuesFromMask(unsigned long, unsigned long *, unsigned long *);
static void CreateDefaultsDb(Display *);
static void DefineProperty(Display *, XStandardColormap *, XVisualInfo *, XStandardColormap *, int, Atom);
static void FindStaticColorCube(Display *, XVisualInfo *, XStandardColormap *);
static void FindStaticGrayRamp(Display *, XVisualInfo *, XStandardColormap *, XStandardColormap *);
static void GetDatabaseValues(Display *, XVisualInfo *, XStandardColormap *, XStandardColormap *);
static void GetHomeDir(char *);
static void SetRamp(XColor *, XColor *, int, int *, unsigned long *);
static void ShrinkMapToFit(XStandardColormap *, int *, XVisualInfo *);
static void UseGrayCorners(XStandardColormap *, XStandardColormap *);
static void UseGrayDiagonal(XStandardColormap *, XStandardColormap *);

#define SCALE 65535
#undef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))

void XDPSGetDefaultColorMaps(
    Display *dpy,
    Screen *screen,
    Drawable drawable,
    XStandardColormap *colorCube,
    XStandardColormap *grayRamp)
{
    Window root;
    Visual *visual;
    XStandardColormap g;

    /* If there is a screen specified, use it; otherwise use the drawable */

    if (screen == NULL) {
        if (drawable == None || ScreenCount(dpy) == 1) {
            root = DefaultRootWindow(dpy);
            screen = DefaultScreenOfDisplay(dpy);
        } else {
            /* Have to get the root for this drawable */
            int x, y;
            int i;
            unsigned int width, height, bwidth, depth;
            if (!XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height,
                              &bwidth, &depth)) root = DefaultRootWindow(dpy);
            for (i = 0;
                 i < ScreenCount(dpy) && root != RootWindow(dpy, i);
                 i++) {}
            screen = ScreenOfDisplay(dpy, i);
        }
    } else root = RootWindowOfScreen(screen);

    if (grayRamp == NULL) grayRamp = &g;

    visual = DefaultVisualOfScreen(screen);
    grayRamp->colormap = DefaultColormapOfScreen(screen);
    if (colorCube != NULL) colorCube->colormap = grayRamp->colormap;

    (void) XDPSCreateStandardColormaps(dpy, root, visual,
				       0, 0, 0, 0, colorCube, grayRamp, True);
}

Status XDPSCreateStandardColormaps(
    Display *dpy,
    Drawable drawable,
    Visual *visual,
    int reds, int greens, int blues, int grays,
    XStandardColormap *colorCube,
    XStandardColormap *grayRamp,
    Bool retain)
{
    XVisualInfo vtemp, *vinfo;
    int nvis;
    XStandardColormap *propCube = NULL, *propRamp = NULL;
    int nPropCube = 0, nPropRamp = 0;
    Bool gotCube = False, gotRamp;
    PixelRec pixels;

    if (grayRamp == NULL) return 0;
    if (colorCube != NULL &&
	colorCube->colormap != grayRamp->colormap) return 0;

    if (dpy == NULL || (drawable == None && visual == NULL)) return 0;

    if (visual == NULL) {
	XWindowAttributes attr;
	if (XGetWindowAttributes(dpy, drawable, &attr) == 0) return 0;
	visual = attr.visual;
    }

    if (grayRamp->colormap == None && drawable == None) return 0;

    curDpyRec = FindDpyRec(dpy);
    if (curDpyRec == NULL) return 0;

    vtemp.visualid = XVisualIDFromVisual(visual);
    vinfo = XGetVisualInfo(dpy, VisualIDMask, &vtemp, &nvis);
    if (nvis == 0) return 0;

    if (nvis > 1) {
	vinfo = PickCorrectVisual(dpy, vinfo, nvis, grayRamp->colormap);
    }
    if (vinfo == NULL) return 0;

    if (grays <= 1) grayRamp->red_max = 0;
    else grayRamp->red_max = grays - 1;

    if (colorCube != NULL) {
	if (reds <= 1) colorCube->red_max = 0;
	else colorCube->red_max = reds - 1;
	if (greens <= 1) colorCube->green_max = 0;
	else colorCube->green_max = greens - 1;
	if (blues <= 1) colorCube->blue_max = 0;
	else colorCube->blue_max = blues - 1;
    }

    if ((vinfo->class == StaticGray || vinfo->class == GrayScale) &&
	colorCube != NULL) {
	/* Can't do a color cube in a gray visual! */
	colorCube->red_max = colorCube->green_max = colorCube->blue_max =
	colorCube->red_mult = colorCube->green_mult = colorCube->blue_mult =
	colorCube->base_pixel = 0;
	colorCube = NULL;
    }

    if (retain) {
	Display *newDpy = XOpenDisplay(XDisplayString(dpy));
	if (newDpy == NULL) retain = False;
	else dpy = newDpy;
	XGrabServer(dpy);
    }

    if (grayRamp->colormap == None) {
	grayRamp->colormap = XCreateColormap(dpy, drawable, vinfo->visual,
					     AllocNone);
	if (colorCube != NULL) colorCube->colormap = grayRamp->colormap;
    }

    if (colorCube != NULL) {
	gotCube = GetColorCubeFromProperty(dpy, vinfo, colorCube,
					   &propCube, &nPropCube);
    }
    gotRamp = GetGrayRampFromProperty(dpy, vinfo, grayRamp,
				      &propRamp, &nPropRamp);

    if (!gotRamp || (colorCube != NULL && !gotCube)) {
	/* Couldn't find at least one thing we wanted, so let's look in the
	   database */

	GetDatabaseValues(dpy, vinfo, colorCube, grayRamp);

	pixels.pixels = NULL;
	pixels.npixels = 0;

	if (colorCube != NULL) {
	    if (colorCube->red_max != 0) {
		AllocateColorCube(dpy, vinfo, colorCube, &pixels);
	    }
	    if (colorCube->red_max == 0) {
		colorCube->green_max = colorCube->blue_max = 
		colorCube->red_mult = colorCube->green_mult =
		colorCube->blue_mult = 0;
	    }
	}

	if (grayRamp->red_max != 0) {
	    AllocateGrayRamp(dpy, vinfo, grayRamp, colorCube, &pixels);
	}

	if (pixels.pixels != NULL) {
	    if (pixels.npixels != 0) {
		XFreeColors(dpy, grayRamp->colormap,
			    pixels.pixels, pixels.npixels, 0);
	    }
	    free((char *) pixels.pixels);
	}

	if (retain) {
	    Pixmap p;

	    /* Create something to put in killid field so the entries can
	       be deleted sometime */
	    p = XCreatePixmap(dpy, RootWindow(dpy, vinfo->screen), 1, 1, 1);
	    if (colorCube != NULL && !gotCube && colorCube->red_max != 0) {
		colorCube->visualid = vinfo->visualid;
		colorCube->killid = p;
		DefineProperty(dpy, colorCube, vinfo, propCube, nPropCube,
			       XA_RGB_DEFAULT_MAP);
	    }
	    if (!gotRamp && grayRamp->red_max != 0) {
		grayRamp->visualid = vinfo->visualid;
		grayRamp->killid = p;
		DefineProperty(dpy, grayRamp, vinfo, propRamp, nPropRamp,
			       (vinfo->class == GrayScale ? XA_RGB_GRAY_MAP :
				curDpyRec->XA_GRAY_DEFAULT_MAP));
	    }
	    XSetCloseDownMode(dpy, RetainTemporary);
	}
    }

    if (grayRamp->red_max == 0) {
	/* Use special magic values.  If this is a default colormap,
	   the server recognizes a gray ramp with red_max=1, red_mult=1,
	   base_pixel=0 to mean a 2 gray ramp with BlackPixel being 
	   the lowest intensity gray and WhitePixel being the highest
           intensity gray.  If it's not a default colormap, then the
	   server will either generate a BadValue error, or just happily
	   use pixel values 0 and 1; either is better than the alternative,
	   silently converting into a null device. */
	grayRamp->red_max = 1;
	grayRamp->red_mult = 1;
	grayRamp->base_pixel = 0;
    }

    if (retain) {
	XUngrabServer(dpy);
	XCloseDisplay(dpy);
    }
    if (propCube != NULL) XFree((void *) propCube);
    if (propRamp != NULL) XFree((void *) propRamp);
    XFree((void *) vinfo);
    return 1;
}

static DpyRec *FindDpyRec(Display *dpy)
{
    DpyRec *d;

    for (d = dpyRec; d != NULL; d = d->next) {
	if (d->dpy == dpy) return d;
    }
  
    d = (DpyRec *) malloc(sizeof(DpyRec));
    if (d == NULL) return NULL;
    d->XA_GRAY_DEFAULT_MAP = XInternAtom(dpy, "DEFAULT_GRAY", False);
    d->db = NULL;
    d->next = dpyRec;
    dpyRec = d;
    return d;
}

static XVisualInfo *PickCorrectVisual(
    Display *dpy,
    XVisualInfo *vlist,
    int n,
    Colormap cmap)
{
    register int i;
    register int screen_number;
    Bool def_cmap = False;

    /* A visual id may be valid on multiple screens.  Also, there may 
     * be multiple visuals with identical visual ids at different depths.  
     * If the colormap is the Default Colormap, use the Default Visual.
     * Otherwise, arbitrarily, use the deepest visual.
     */

    for (screen_number = ScreenCount(dpy); --screen_number >= 0; /**/) {
	if (cmap == DefaultColormap(dpy, screen_number)) {
	    def_cmap = True;
	    break;
	}
    }

    if (def_cmap) {
	for (i = 0; i < n; i++, vlist++) {
	    if (vlist->visual == DefaultVisual(dpy, screen_number)) {
		return vlist;
	    }
	}
	return NULL;	/* Visual does not match colormap */
    } else {
	int maxdepth = 0;
	XVisualInfo *v = 0;

	for (i = 0; i < n; i++, vlist++) {
	    if (vlist->depth > maxdepth) {
		maxdepth = vlist->depth;
		v = vlist;
	    }
	}
	return v;
    }
}

/* Do some rudimentary checking of the properties to avoid obviously bad ones.
   How did they get there, anyway? */

static Bool ValidCube(
    XStandardColormap *c,
    XVisualInfo *vinfo)
{
    unsigned long max = 1 << vinfo->depth;
    unsigned long pixel;

    if (c->red_max < 1 || c->green_max < 1 || c->blue_max < 1) return False;
    if (c->base_pixel > max) return False;
    pixel = (c->red_max * c->red_mult + c->green_max * c->green_mult +
	     c->blue_max * c->blue_mult + c->base_pixel) & 0xFFFFFFFF;
    if (pixel > max) return False;

    return True;
}

static Bool ValidRamp(
    XStandardColormap *c,
    XVisualInfo *vinfo)
{
    unsigned long max = 1 << vinfo->depth;
    unsigned long pixel;

    if (c->red_max < 1) return False;
    if (c->base_pixel > max) return False;
    pixel = (c->red_max * c->red_mult + c->base_pixel) & 0xFFFFFFFF;
    if (pixel > max) return False;

    return True;
}

static Bool GetColorCubeFromProperty(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *colorCube,
    XStandardColormap **cube,
    int *ncube)
{
    int gotCube;
    int i;
    register XStandardColormap *c;

    gotCube = XGetRGBColormaps(dpy, RootWindow(dpy, vinfo->screen), cube,
			       ncube, XA_RGB_DEFAULT_MAP);
    
    if (gotCube) {
	/* Try to find a match with the visual */
	c = *cube;
	for (i = 0; i < *ncube; i++) {
	    if (c->colormap == colorCube->colormap &&
		c->visualid == vinfo->visualid &&
		ValidCube(c, vinfo)) {
		colorCube->red_max = c->red_max;
		colorCube->red_mult = c->red_mult;
		colorCube->green_max = c->green_max;
		colorCube->green_mult = c->green_mult;
		colorCube->blue_max = c->blue_max;
		colorCube->blue_mult = c->blue_mult;
		colorCube->base_pixel = c->base_pixel;
		colorCube->visualid = c->visualid;
		colorCube->killid = c->killid;
		break;
	    }
	    c++;
	}
	if (i == *ncube) gotCube = False;
    }
    return gotCube;
}

static Bool GetGrayRampFromProperty(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *grayRamp,
    XStandardColormap **ramp,
    int *nramp)
{
    int gotRamp;
    int i;
    Atom grayAtom;
    register XStandardColormap *c;

    if (vinfo->class == GrayScale) grayAtom = XA_RGB_GRAY_MAP;
    else grayAtom = curDpyRec->XA_GRAY_DEFAULT_MAP;

    gotRamp = XGetRGBColormaps(dpy, RootWindow(dpy, vinfo->screen), ramp,
			       nramp, grayAtom);
    
    if (gotRamp) {
	/* Try to find a match with the visual */
	c = *ramp;
	for (i = 0; i < *nramp; i++) {
	    if (c->colormap == grayRamp->colormap &&
		c->visualid == vinfo->visualid &&
		ValidRamp(c, vinfo)) {
		grayRamp->red_max = c->red_max;
		grayRamp->red_mult = c->red_mult;
		grayRamp->base_pixel = c->base_pixel;
		grayRamp->visualid = c->visualid;
		grayRamp->killid = c->killid;
		break;
	    }
	    c++;
	}
	if (i == *nramp) gotRamp = False;
    }
    return gotRamp;
}

static void GetDatabaseValues(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *colorCube,
    XStandardColormap *grayRamp)
{
    char *class, *depth;
    char namePrefix[40], classPrefix[40];
    unsigned long max;
    XStandardColormap fakeCube;

    switch (vinfo->class) {
	default:
        case StaticGray:  class = "StaticGray."; break;
        case GrayScale:   class = "GrayScale."; break;
        case StaticColor: class = "StaticColor."; break;
        case PseudoColor: class = "PseudoColor."; break;
        case TrueColor:   class = "TrueColor."; break;
        case DirectColor: class = "DirectColor."; break;
    }

    if      (vinfo->depth >= 24) depth = "24.";
    else if (vinfo->depth >= 12) depth = "12.";
    else if (vinfo->depth >= 8) depth = "8.";
    else if (vinfo->depth >= 4) depth = "4.";
    else if (vinfo->depth >= 2) depth = "2.";
    else depth = "1.";

    (void) strcpy(namePrefix, "dpsColorCube.");
    (void) strcat(strcat(namePrefix, class), depth);
    (void) strcpy(classPrefix, "DPSColorCube.");
    (void) strcat(strcat(classPrefix, class), depth);

    CreateDefaultsDb(dpy);

    if (colorCube == NULL && vinfo->class == TrueColor) {
	/* We'll need the color cube information to compute the gray ramp,
	   even if it wasn't asked for, so make colorCube point to a
	   temporary structure */
	colorCube = &fakeCube;
    }

    if (colorCube != NULL) {
	switch (vinfo->class) {
	    case StaticGray:
	    case GrayScale:
	        /* We can't do a color cube for these visuals */
		break;

	    case TrueColor:
		/* Rewrite whatever was there before with real values */
		ColorValuesFromMask(vinfo->red_mask, &colorCube->red_max,
				    &colorCube->red_mult);
		ColorValuesFromMask(vinfo->green_mask, &colorCube->green_max,
				    &colorCube->green_mult);
		ColorValuesFromMask(vinfo->blue_mask, &colorCube->blue_max,
				    &colorCube->blue_mult);
		colorCube->base_pixel = 0;
		break;

	    case DirectColor:
		/* Get the mults from the masks; ignore maxes */
		ColorValuesFromMask(vinfo->red_mask, &max,
				    &colorCube->red_mult);
		ColorValuesFromMask(vinfo->green_mask, &max,
				    &colorCube->green_mult);
		ColorValuesFromMask(vinfo->blue_mask, &max,
				    &colorCube->blue_mult);
		/* Get the maxes from the database */
		if (colorCube->red_max == 0) {
		    colorCube->red_max =
			    NumColors(namePrefix, classPrefix, redsName) - 1;
		}
		if (colorCube->green_max == 0) {
		    colorCube->green_max =
			    NumColors(namePrefix, classPrefix, greensName) - 1;
		}
		if (colorCube->blue_max == 0) {
		    colorCube->blue_max =
			    NumColors(namePrefix, classPrefix, bluesName) - 1;
		}
		colorCube->base_pixel = 0;
		break;

	    case PseudoColor:
		if (colorCube->red_max == 0) {
		    colorCube->red_max =
			    NumColors(namePrefix, classPrefix, redsName) - 1;
		}
		if (colorCube->green_max == 0) {
		    colorCube->green_max =
			    NumColors(namePrefix, classPrefix, greensName) - 1;
		}
		if (colorCube->blue_max == 0) {
		    colorCube->blue_max =
			    NumColors(namePrefix, classPrefix, bluesName) - 1;
		}
		colorCube->red_mult = (colorCube->green_max + 1) *
			(colorCube->blue_max + 1);
		colorCube->green_mult = colorCube->blue_max + 1;
		colorCube->blue_mult = 1;
		break;

	    case StaticColor:
		FindStaticColorCube(dpy, vinfo, colorCube);
		break;
	}
    }

    switch (vinfo->class) {
	case GrayScale:
	case PseudoColor:
	case DirectColor:
	    if (grayRamp->red_max == 0) {
		grayRamp->red_max =
			NumColors(namePrefix, classPrefix, graysName) - 1;
	    }
	    grayRamp->red_mult = 1;
	    break;

	case TrueColor:
	    /* If the color cube is truly a cube, use its diagonal.  Otherwise
	       were SOL and have to use a two-element ramp. */
	    if (CubicCube(colorCube)) UseGrayDiagonal(colorCube, grayRamp);
	    else UseGrayCorners(colorCube, grayRamp);
	    break;

	case StaticColor:
	case StaticGray:
	    FindStaticGrayRamp(dpy, vinfo, grayRamp, colorCube);
	    break;
    }
}

static Bool CubicCube(XStandardColormap *cube)
{
    return cube->red_max == cube->green_max && cube->red_max ==
	    cube->blue_max;
}

static void UseGrayDiagonal(XStandardColormap *cube, XStandardColormap *ramp)
{
    ramp->red_max = cube->red_max;
    ramp->red_mult = cube->red_mult + cube->green_mult + cube->blue_mult;
    ramp->base_pixel = cube->base_pixel;
}

static void UseGrayCorners(XStandardColormap *cube, XStandardColormap *ramp)
{
    ramp->red_max = 1;
    ramp->red_mult = (cube->red_max + 1) * (cube->green_max + 1) *
	    (cube->blue_max + 1) - 1;
    if (* (int *) &(cube->red_mult) < 0) ramp->red_mult *= -1;
    ramp->base_pixel = cube->base_pixel;
}

static void ColorValuesFromMask(
    unsigned long	mask,
    unsigned long	*maxColor,
    unsigned long	*mult)
{
    *mult = 1;
    while ((mask & 1) == 0) {
	*mult <<= 1;
	mask >>= 1;
    }
    *maxColor = mask;
}

/* 
Resource definitions for default color cube / gray ramp sizes
are based on visual class and depth. Working from least choices
to most, here's motivation for the defaults:

If unspecified, default is 0 values for red, green, and blue,
and 2 (black and white) for grays. This covers StaticGray, StaticColor,
and depths less than 4 of the other visual classes.

If we have a choice, we try to allocate a gray ramp with an odd number
of colors; this is so 50% gray can be rendered without dithering.
In general we don't want to allocate a large cube (even when many
colormap entries are available) because allocation of each entry
requires a round-trip to the server (entries allocated read-only
via XAllocColor).

For GrayScale, any depth less than 4 is treated as monochrome.

PseudoColor depth 4 we try for a 2x2x2 cube with the gray ramp on
the diagonal. Depth 8 uses a 4x4x4 cube with a separate 9 entry
gray ramp. Depth 12 uses a 6x6x5 "cube" with a separate 17 entry gray
ramp. The cube is non-symmetrical; we don't want to use the diagonal
for a gray ramp and we can get by with fewer blues than reds or greens.

For DirectColor, allocating a gray ramp separate from the color cube
is wasteful of map entries, so we specify a symmetrical cube and
share the diagonal entries for the gray ramp. 

For TrueColor, # color shades is set equal to the # shades / primary;
we don't actually allocate map entries, but it's handy to be able to
do the resource lookup blindly and get the right value.
*/

static char dpsDefaults[] = "\
*reds: 0\n\
*greens: 0\n\
*blues: 0\n\
*grays: 2\n\
\
*GrayScale.4.grays: 9\n\
*GrayScale.8.grays: 17\n\
\
*PseudoColor.4.reds: 2\n\
*PseudoColor.4.greens: 2\n\
*PseudoColor.4.blues: 2\n\
*PseudoColor.4.grays: 2\n\
*PseudoColor.8.reds: 4\n\
*PseudoColor.8.greens: 4\n\
*PseudoColor.8.blues: 4\n\
*PseudoColor.8.grays: 9\n\
*PseudoColor.12.reds: 6\n\
*PseudoColor.12.greens: 6\n\
*PseudoColor.12.blues: 5\n\
*PseudoColor.12.grays: 17\n\
\
*DirectColor.8.reds: 4\n\
*DirectColor.8.greens: 4\n\
*DirectColor.8.blues: 4\n\
*DirectColor.8.grays: 4\n\
*DirectColor.12.reds: 6\n\
*DirectColor.12.greens: 6\n\
*DirectColor.12.blues: 6\n\
*DirectColor.12.grays: 6\n\
*DirectColor.24.reds: 7\n\
*DirectColor.24.greens: 7\n\
*DirectColor.24.blues: 7\n\
*DirectColor.24.grays: 7\n\
\
*TrueColor.12.reds: 16\n\
*TrueColor.12.greens: 16\n\
*TrueColor.12.blues: 16\n\
*TrueColor.12.grays: 16\n\
*TrueColor.24.reds: 256\n\
*TrueColor.24.greens: 256\n\
*TrueColor.24.blues: 256\n\
*TrueColor.24.grays: 256\n\
";

static XrmDatabase defaultDB = NULL;

static void CreateDefaultsDb(Display *dpy)
{
    char home[256], *dpyDefaults;

    if (defaultDB == NULL) defaultDB = XrmGetStringDatabase(dpsDefaults);

    if (curDpyRec->db != NULL) return;

    dpyDefaults = XResourceManagerString(dpy);
    if (dpyDefaults != NULL) {
	curDpyRec->db = XrmGetStringDatabase(dpyDefaults);
    } 

    if (curDpyRec->db == NULL) {
	GetHomeDir(home);
	strcpy(home, "/.Xdefaults");
	curDpyRec->db = XrmGetFileDatabase(home);
    }
}

static void GetHomeDir(char *buf)
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
            if ((ptr = getenv("USER")) != 0) pw = getpwnam(ptr);
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
     return;
}

static long NumColors(char *namePrefix, char *classPrefix, char *color)
{
    char name[40], class[40];
    XrmValue rtnValue;
    char *rtnType;
    long value;

    (void) strcpy(name, namePrefix);
    (void) strcpy(class, classPrefix);
    if (! XrmGetResource(curDpyRec->db, strcat(name, color), 
			 strcat(class, color), &rtnType, &rtnValue)) {
	if (! XrmGetResource(defaultDB, name, class, &rtnType, &rtnValue)) {
	    /* This should never happen, as our defaults cover all cases */
	    return 0;
	}
    }

    /* Resource value is number of shades of specified color.  If value
       is not an integer, atoi returns 0, so we return 0. If value
       is less than 2, it is invalid (need at least 2 shades of a color).
       Explicitly setting 0 is ok for colors (means to not use a color
       cube) but merits a warning for gray. */

    if (strcmp(rtnValue.addr, "0") == 0 && strcmp(color, "grays") != 0) {
	return 0;
    }

    value = atol(rtnValue.addr);
    if (value < 2) {
	char mbuf[512];
	sprintf(mbuf, "%% Value '%s' is invalid for %s resource\n",
		rtnValue.addr, name);
	DPSWarnProc(NULL, mbuf);
    }
    return value;
}

/* Query the entire colormap in the static color case, then try to find
   a color cube.  Check pairs of black and white cells trying to find
   a cube between them and take the first one you find. */

static void FindStaticColorCube(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *colorCube)
{
    XColor *ramp, *black, *white, *altBlack, *altWhite;
    int i, entries;

    entries = 1 << vinfo->depth;
    ramp = (XColor *) calloc(entries, sizeof(XColor));

    if (ramp == NULL) {
	colorCube->red_max = 0;
	return;
    }

    /* Query the colormap */
    for (i = 0; i < entries; i++) ramp[i].pixel = i;
    XQueryColors(dpy, colorCube->colormap, ramp, entries);

    /* Find the white and black entries */

    black = white = altBlack = altWhite = NULL;
    for (i = 0; i < entries; i++) {
	if (ramp[i].flags != (DoRed | DoBlue | DoGreen)) continue;
	if (ramp[i].red == 0 && ramp[i].blue == 0 &&
	    ramp[i].green == 0) {
	    if (black == NULL) black = ramp+i;
	    else if (altBlack == NULL) altBlack = ramp+i;
	} else if (ramp[i].red == SCALE && ramp[i].blue == SCALE &&
		   ramp[i].green == SCALE) {
	    if (white == NULL) white = ramp+i;
	    else if (altWhite == NULL) altWhite = ramp+i;
	}
    }

    if (black == NULL || white == NULL) {
	colorCube->red_max = 0;
	free(ramp);
	return;
    }

    /* Look for cubes between pairs of black & white */
    if (!CheckCube(black, white, colorCube) &&
	!CheckCube(altBlack, white, colorCube) &&
	!CheckCube(black, altWhite, colorCube) &&
	!CheckCube(altBlack, altWhite, colorCube)) {
	colorCube->red_max = 0;
    }

    free(ramp);
}

#define R 1
#define G 2
#define B 4
#define C 8
#define M 16
#define Y 32

#define SMALLSCALE 255
#define CheckColor(color,r,g,b) ((((color)->red >> 8) == (r) * SMALLSCALE) && \
	(((color)->green >> 8) == (g) * SMALLSCALE) && \
	(((color)->blue >> 8) == (b) * SMALLSCALE))

static Bool CheckCube(
    XColor *black,
    XColor *white,
    XStandardColormap *cube)
{
    int r = 0, g = 0, b = 0, c = 0, m = 0, y = 0, k, w;
    XColor *color;
    unsigned int found = 0;
    int small, middle, large;
    int smallMult, smallMax, middleMult, middleMax, largeMult, largeMax;
    Bool backwards = False;
    int mult = 1;
    XStandardColormap test;	/* Test cube */
    int i;
    int size;

    if (black == NULL || white == NULL) return False;

    k = black->pixel;
    w = white->pixel - k;

    size = ABS(w);
    if (w < 0) {
	backwards = True;
	mult = -1;
    }

    for (i = 1; i < size; i++) {
	color = black + i*mult;
	if (color->flags != (DoRed | DoBlue | DoGreen)) return False;

	/* If black or white is in the middle of the cube, can't work */
	if (CheckColor(color, 0, 0, 0)) return False;
	if (CheckColor(color, 1, 1, 1)) return False;

	/* Check for red, green, blue, cyan, magenta, and yellow */
	if (CheckColor(color, 1, 0, 0)) {r = color->pixel-k; found |= R;}
	else if (CheckColor(color, 0, 1, 0)) {g = color->pixel-k; found |= G;}
	else if (CheckColor(color, 0, 0, 1)) {b = color->pixel-k; found |= B;}
	else if (CheckColor(color, 0, 1, 1)) {c = color->pixel-k; found |= C;}
	else if (CheckColor(color, 1, 0, 1)) {m = color->pixel-k; found |= M;}
	else if (CheckColor(color, 1, 1, 0)) {y = color->pixel-k; found |= Y;}
    }

    /* If any color is missing no cube is possible */
    if (found != (R | G | B | C | M | Y)) return False;

    /* Next test.  Make sure B + G = C, R + B = M, R + G = Y,
       and R + G + B = W */
    if (b + g != c) return False;
    if (r + b != m) return False;
    if (r + g != y) return False;
    if (r + g + b != w) return False;

    /* Looking good!  Compensate for backwards cubes */
    if (backwards) {
	w = ABS(w);
	r = ABS(r);
	g = ABS(g);
	b = ABS(b);
    }

    /* Find the smallest, middle, and largest difference */
    if (r < b && b < g) {
	small = r; middle = b; large = g;
    } else if (r < g && g < b) {
	small = r; middle = g; large = b;
    } else if (b < r && r < g) {
	small = b; middle = r; large = g;
    } else if (b < g && g < r) {
	small = b; middle = g; large = r;
    } else if (g < r && r < b) {
	small = g; middle = r; large = b;
    } else {
	small = g; middle = b; large = r;
    }

    /* The smallest must divide the middle, and the middle the large */
    if ((middle % (small + 1)) != 0) return False;
    if ((large % (small + middle + 1)) != 0) return False;

    /* OK, we believe we have a cube.  Compute the description */
    smallMult = 1;
    smallMax = small;
    middleMult = small + 1;
    middleMax = middle / middleMult;
    largeMult = small + middle + 1;
    largeMax = large / largeMult;
	
    if (small == r) {
	test.red_max = smallMax; test.red_mult = smallMult;
	if (middle == b) {
	    test.blue_max = middleMax; test.blue_mult = middleMult;
	    test.green_max = largeMax; test.green_mult = largeMult;
	} else {
	    test.green_max = middleMax; test.green_mult = middleMult;
	    test.blue_max = largeMax; test.blue_mult = largeMult;
	}
    } else if (small == g) {
	test.green_max = smallMax; test.green_mult = smallMult;
	if (middle == b) {
	    test.blue_max = middleMax; test.blue_mult = middleMult;
	    test.red_max = largeMax; test.red_mult = largeMult;
	} else {
	    test.red_max = middleMax; test.red_mult = middleMult;
	    test.blue_max = largeMax; test.blue_mult = largeMult;
	}
    } else {	/* small == b */
	test.blue_max = smallMax; test.blue_mult = smallMult;
	if (middle == r) {
	    test.red_max = middleMax; test.red_mult = middleMult;
	    test.green_max = largeMax; test.green_mult = largeMult;
	} else {
	    test.green_max = middleMax; test.green_mult = middleMult;
	    test.red_max = largeMax; test.red_mult = largeMult;
	}
    }
    
    /* Re-compensate for backwards cube */
    if (backwards) {
	test.red_mult *= -1;
	test.green_mult *= -1;
	test.blue_mult *= -1;
    }

    /* Finally, test the hypothesis!  The answer must be correct within 1
       bit.  Only look at the top 8 bits; the others are too noisy */

    for (i = 1; i < size; i++) {
#define calc(i, max, mult) ((((i / test.mult) % \
			      (test.max + 1)) * SCALE) / test.max)
	r = ((unsigned short) calc(i, red_max, red_mult) >> 8) -
		(black[i*mult].red >> 8);
	g = ((unsigned short) calc(i, green_max, green_mult) >> 8) -
		(black[i*mult].green >> 8);
	b = ((unsigned short) calc(i, blue_max, blue_mult) >> 8) -
		(black[i*mult].blue >> 8);
#undef calc
	if (ABS(r) > 2 || ABS(g) > 2 || ABS(b) > 2) return False;
    }
    cube->red_max = test.red_max;
    cube->red_mult = test.red_mult;
    cube->green_max = test.green_max;
    cube->green_mult = test.green_mult;
    cube->blue_max = test.blue_max;
    cube->blue_mult = test.blue_mult;
    cube->base_pixel = k;
    return True;
}

#undef R
#undef G
#undef B
#undef C
#undef M
#undef Y

/* Query the entire colormap in the static gray case, then try to find
   a gray ramp.  This handles there being 2 white or black entries
   in the colormap and finds the longest linear ramp between pairs of
   white and black.  If there is a color cube, also check its diagonal and
   use its corners if we need to */

static void FindStaticGrayRamp(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *grayRamp,
    XStandardColormap *colorCube)
{
    XColor *ramp, *black, *white, *altBlack, *altWhite;
    int i, r0, r1, r2, r3, size, entries, redMult;
    unsigned long base;

    entries = 1 << vinfo->depth;
    ramp = (XColor *) calloc(entries, sizeof(XColor));

    if (ramp == NULL) {
	grayRamp->red_max = 0;
	return;
    }

    /* Query the colormap */
    for (i = 0; i < entries; i++) ramp[i].pixel = i;
    XQueryColors(dpy, grayRamp->colormap, ramp, entries);

    /* Find the white and black entries */

    black = white = altBlack = altWhite = NULL;
    for (i = 0; i < entries; i++) {
	if (ramp[i].flags != (DoRed | DoBlue | DoGreen)) continue;
	if (CheckColor(ramp+i, 0, 0, 0)) {
	    if (black == NULL) black = ramp+i;
	    else if (altBlack == NULL) altBlack = ramp+i;
	} else if (CheckColor(ramp+i, 1, 1, 1)) {
	    if (white == NULL) white = ramp+i;
	    else if (altWhite == NULL) altWhite = ramp+i;
	}
    }

    if (black == NULL || white == NULL) {
	grayRamp->red_max = 0;
	free(ramp);
	return;
    }

    /* Find out how large a ramp exists between pairs of black & white */
    r0 = FindRampSize(black, white);
    r1 = FindRampSize(altBlack, white);
    r2 = FindRampSize(black, altWhite);
    r3 = FindRampSize(altBlack, altWhite);

    size = r0;
    if (r1 > size) size = r1;
    if (r2 > size) size = r2;
    if (r3 > size) size = r3;
    if (size == r0) SetRamp(black, white, size, &redMult, &base);
    else if (size == r1) SetRamp(altBlack, white, size, &redMult, &base);
    else if (size == r2) SetRamp(black, altWhite, size, &redMult, &base);
    else if (size == r3) SetRamp(altBlack, altWhite, size, &redMult, &base);

    if (colorCube != NULL && CubicCube(colorCube) &&
	colorCube->red_max > size) {
	UseGrayDiagonal(colorCube, grayRamp);
    } else {
	grayRamp->red_max = size;
	grayRamp->red_mult = redMult;
	grayRamp->base_pixel = base;
    }

    free(ramp);
}

static int FindRampSize(XColor *black, XColor *white)
{
    XColor *c;
    int r;
    int mult = 1;
    int i, size;

    if (black == NULL || white == NULL) return 0;
    size = ABS(white - black);

    /* See if we have a backwards ramp */
    if (black > white) mult = -1;

    /* See if all cells between black and white are linear, to within 1 bit.
       Only look at the high order 8 bits */

    for (i = 1; i < size; i++) {
	c = &black[i*mult];
	if (c->red != c->blue || c->red != c->green) return 1;
	r = ((unsigned short) ((i * SCALE) / size) >> 8) - (c->red >> 8);
	if (ABS(r) > 2) return 1;
    }
    return size;
}

static void SetRamp(
    XColor *black,
    XColor *white,
    int size,
    int *mult,
    unsigned long *base)
{
    *base = black->pixel;
    *mult = (white - black) / size;
}

#define lowbit(x) ((x) & (~(x) + 1))

static unsigned long shiftdown(unsigned long x)
{
    while ((x & 1) == 00) {
	x = x >> 1;
    }
    return x;
}

static void AllocateColorCube(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *colorCube,
    PixelRec *pixels)
{
    int count, first, remain, n, j;
    unsigned long i;
    Colormap cmap = colorCube->colormap;
    unsigned long delta;
    XColor color;

    /* We do no allocation for TrueColor or StaticColor */
    if (vinfo->class == TrueColor || vinfo->class == StaticColor) return;

    if (vinfo->class == DirectColor) {
	if ((i = shiftdown(vinfo->red_mask)) > colorCube->red_max)
		colorCube->red_max = i;
	if ((i = shiftdown(vinfo->green_mask)) > colorCube->green_max)
		colorCube->green_max = i;
	if ((i = shiftdown(vinfo->blue_mask)) > colorCube->blue_max)
		colorCube->blue_max = i;

	/* We only handle symmetric DirectColor */
	count = colorCube->red_max + 1;
	if (colorCube->blue_max + 1 < count) count = colorCube->blue_max + 1;
	if (colorCube->green_max + 1 < count) count = colorCube->green_max + 1;
	colorCube->red_max = colorCube->blue_max = colorCube->green_max =
		count - 1;

	delta = lowbit(vinfo->red_mask) + lowbit(vinfo->green_mask) +
		lowbit(vinfo->blue_mask);
    } else {
	count = (colorCube->red_max + 1) * (colorCube->blue_max + 1) *
		(colorCube->green_max + 1);
	delta = 1;
    }

    colorCube->base_pixel = 0; /* temporary, may change */

    pixels->pixels = (unsigned long *) calloc(vinfo->colormap_size,
					      sizeof(unsigned long));
    if (pixels->pixels == NULL) {
	colorCube->red_max = 0;
	return;
    }

    if (!AllocateColormap(dpy, colorCube, vinfo, &count, pixels,
			  &first, &remain, delta)) {
	free((char *) pixels->pixels);
	pixels->pixels = NULL;
	colorCube->red_max = 0;
	return;
    }

    colorCube->base_pixel = pixels->pixels[first];
    color.flags = DoRed | DoGreen | DoBlue;

    /* Define colors */
    for (n = 0, j = 0; j < count; ++j, n += delta) {
	color.pixel = n + pixels->pixels[first];
	if (vinfo->class == PseudoColor) {
#define calc(i, max, mult) ((((i / colorCube->mult) % \
			      (colorCube->max + 1)) * SCALE) / colorCube->max)
	    color.red = (unsigned short) calc(n, red_max, red_mult);
	    color.green = (unsigned short) calc(n, green_max, green_mult);
	    color.blue = (unsigned short) calc(n, blue_max, blue_mult);
#undef calc
	} else {
	    color.red = color.green = color.blue =
		    (j * SCALE) / colorCube->red_max;
	}
	if (!AllocateColor(dpy, cmap, &color)) {
	    XFreeColors(dpy, cmap, pixels->pixels, count+first+remain, 0);
	    free((char *) pixels->pixels);
	    pixels->pixels = NULL;
	    colorCube->red_max = 0;
	    return;
	}
    }

    /* Smush down unused pixels, if any */

    for (j = 0; j < remain; j++) {
	pixels->pixels[first+j] = pixels->pixels[first+count+j];
    }
    pixels->npixels -= count;
}

static void AllocateGrayRamp(
    Display *dpy,
    XVisualInfo *vinfo,
    XStandardColormap *grayRamp,
    XStandardColormap *colorCube,
    PixelRec *pixels)
{
    int count, first, remain, n, i;
    Colormap cmap = grayRamp->colormap;
    XColor color;
    unsigned long delta;

    /* Allocate cells in read/write visuals only */
    if (vinfo->class != PseudoColor && vinfo->class != GrayScale &&
	vinfo->class != DirectColor) return;

    if (vinfo->class == DirectColor) {
	delta = lowbit(vinfo->red_mask) + lowbit(vinfo->green_mask) +
		lowbit(vinfo->blue_mask);
    } else delta = 1;

    /* First of all see if there's a usable gray ramp in the color cube */

    if (colorCube != NULL) {
	if (CubicCube(colorCube)) {
	    if (colorCube->red_max >= grayRamp->red_max) {
		/* diagonal is long enough!  use it */
		UseGrayDiagonal(colorCube, grayRamp);
		return;
	    }
	}
    }

    grayRamp->base_pixel = 0; /* temporary, may change */

    count = grayRamp->red_max + 1;

    if (pixels->pixels == NULL) {
	pixels->pixels = (unsigned long *) calloc(vinfo->colormap_size,
						  sizeof(unsigned long));
	if (pixels->pixels == NULL) {
	    grayRamp->red_max = 0;
	    return;
	}
    }

    if (!AllocateColormap(dpy, grayRamp, vinfo, &count, pixels,
			  &first, &remain, delta)) {
	/* Last gasp:  try any diagonal or the corners of the color cube */
	if (colorCube != NULL) {
	    if (CubicCube(colorCube)) UseGrayDiagonal(colorCube, grayRamp);
	    else UseGrayCorners(colorCube, grayRamp);
	} else {
	    grayRamp->red_max = 0;
	}
	return;
    }

    grayRamp->base_pixel = pixels->pixels[first];
    color.flags = DoRed | DoGreen | DoBlue;

    /* Define colors */
    for (n = 0, i = 0; i < count; ++i, n += delta) {
	color.pixel = n + pixels->pixels[first];
	color.red = (unsigned short)((n * SCALE) / (grayRamp->red_max));
	color.green = color.red;
	color.blue = color.red;

	if (!AllocateColor(dpy, cmap, &color)) {
	    /* Don't need to free pixels here; we'll do it on return */
	    grayRamp->red_max = 0;
	    return;
	}
    }

    /* Smush down unused pixels, if any */

    for (i = 0; i < remain; i++) {
	pixels->pixels[first+i] = pixels->pixels[first+count+i];
    }
    pixels->npixels -= count;
}

static int compare(const void *a1, const void *a2)
{
    register unsigned long *e1 = (unsigned long *) a1,
			   *e2 = (unsigned long *) a2;

    if (*e1 < *e2)	return -1;
    if (*e1 > *e2)	return 1;
    return 0;
}

static Bool AllocateColormap(
    Display *dpy,
    XStandardColormap *map,
    XVisualInfo *vinfo,
    int *count,
    PixelRec *pixels,
    int *first, int *remain,
    unsigned long delta)
{
    Colormap cmap = map->colormap;
    int npixels, ok, i;
    Bool success = False;

    if (pixels->npixels == 0) {
	/* First try to allocate the entire colormap */
	npixels = vinfo->colormap_size;
	ok = XAllocColorCells(dpy, cmap, 1, NULL, 0, pixels->pixels, npixels);
	if (ok) success = True;
	else {
	    int total;
	    int top, mid = 0;

	    /* If it's a gray ramp or direct color we need at least 2;
	       others 8 */
	    if (map->blue_max == 0 || vinfo->class == DirectColor) total = 2;
	    else total = 8;

	    /* Allocate all available cells, using binary backoff */
	    top = vinfo->colormap_size - 1;
	    while (total <= top) {
		mid = total + ((top - total + 1) / 2);
		ok = XAllocColorCells(dpy, cmap, 1, NULL, 0,
				      pixels->pixels, mid);
		if (ok) {
		    if (mid == top) {
			success = True;
			break;
		    } else {
			XFreeColors(dpy, cmap, pixels->pixels, mid, 0);
			total = mid;
		    }
		} else top = mid - 1;
	    }
	    if (success) npixels = mid;
	    else npixels = 0;
	}
    } else {
	/* We must be in the gray ramp case, so we need at least 2 entries */
	npixels = pixels->npixels;
	if (map->blue_max != 0 || npixels >= 2) success = True;
    }

    if (success) {
	/* Avoid pessimal case by testing to see if already sorted */
	for (i = 0; i < npixels-1; ++i) {
	    if (pixels->pixels[i] != pixels->pixels[i+1]-1) break;
	}

	if (i < npixels-1) {
	    PrivSort((char *)pixels->pixels, npixels,
		     sizeof(unsigned long), compare);
	}
    
	if (!contiguous(pixels->pixels, npixels, count, delta,
			first, remain)) {
	    /* If there are enough free cells, shrink the map to fit.
	       Otherwise fail; we'll free the pixels later */
	    if (((map->blue_max == 0 || vinfo->class == DirectColor) &&
		 *count >= 2) || *count >=8) {
		ShrinkMapToFit(map, count, vinfo);
		*remain = npixels - *first - *count;
	    } else success = False;
	}
    }

    pixels->npixels = npixels;
    return success;
}

static Bool contiguous(
    unsigned long	pixels[],	/* specifies allocated pixels */
    int			npixels,	/* specifies count of alloc'd pixels */
    int			*ncolors,	/* specifies needed sequence length
					   If not available, returns max 
					   available contiguous sequence */
    unsigned long	delta,
    int			*first,		/* returns first index of sequence */
    int			*rem)		/* returns first index after sequence,
					 * or 0, if none follow */
{
    register int i = 1;		/* walking index into the pixel array */
    register int count = 1;	/* length of sequence discovered so far */
    int max = 1;		/* longest sequence we found */
    int maxstart = 0;

    *first = 0;
    while (count < *ncolors && i < npixels) {
	if (pixels[i-1] + delta == pixels[i]) count++;
	else {
	    if (count > max) {
		max = count;
		maxstart = *first;
	    }
	    count = 1;
	    *first = i;
	}
	i++;
    }
    if (i == npixels && count > max) {
	max = count;
	maxstart = *first;
    }
    *rem = npixels - i;
    if (count != *ncolors) {
	*ncolors = max;
	*first = maxstart;
	return False;
    } return True;
}

static Bool AllocateColor(
    Display *dpy,
    Colormap cmap,
    XColor *color)
{
    unsigned long pix = color->pixel;
    XColor request;
    int ok;

    request = *color;

    /* Free RW, Alloc RO, if fails, try RW */
    XFreeColors(dpy, cmap, &pix, 1, 0);
    ok = XAllocColor(dpy, cmap, &request);

    /* If the pixel we get back isn't the request one, probably RO
       White or Black, so shove it in RW so our cube is correct.
       If alloc fails, try RW. */

    if (!ok || request.pixel != color->pixel) {
	ok = XAllocColorCells(dpy, cmap, 0, NULL, 0, &pix, 1);

	if (pix != color->pixel) XFreeColors(dpy, cmap, &pix, 1, 0);
	if (!ok || pix != color->pixel) {
	    return False;
	}
	request = *color;
	XStoreColor(dpy, cmap, &request);
    }
    return True;
}

static void ShrinkMapToFit(
    XStandardColormap *map,
    int *space,
    XVisualInfo *vinfo)
{
    if (map->blue_max == 0) map->red_max = *space - 1;
    else if (vinfo->class == DirectColor) {
	if (map->red_max > *space - 1) map->red_max = *space - 1;
	if (map->green_max > *space - 1) map->green_max = *space - 1;
	if (map->blue_max > *space - 1) map->blue_max = *space - 1;
    } else {
	int which = 2;
	while ((map->red_max + 1) * (map->green_max + 1) *
	       (map->blue_max + 1) > *space) {
	    if (which == 0) {
		if (map->red_max > 1) map->red_max--;
		which = 1;
	    } else if (which == 1) {
		if (map->green_max > 1) map->green_max--;
		which = 2;
	    } else {
		if (map->blue_max > 1) map->blue_max--;
		which = 0;
	    }
	}
	*space = (map->red_max + 1) * (map->green_max + 1) *
		(map->blue_max + 1);

	map->red_mult = (map->green_max + 1) * (map->blue_max + 1);
	map->green_mult = map->blue_max + 1;
	map->blue_mult = 1;
    }
}

static void DefineProperty(
    Display *dpy,
    XStandardColormap *map,
    XVisualInfo *vinfo,
    XStandardColormap *prop,
    int nProp,
    Atom atom)
{
    XStandardColormap *copy;
    int i;

    if (nProp == 0) {
	XSetRGBColormaps(dpy, RootWindow(dpy, vinfo->screen), map, 1, atom);
	return;
    }

    copy = (XStandardColormap *) calloc(nProp+1, sizeof(XStandardColormap));

    /* Hm.  If I can't allocate the list, is it better to just put our
       property on, or to leave the ones there?  I'll guess the latter... */
    if (copy == NULL) return;

    if (vinfo->visual == DefaultVisual(dpy, vinfo->screen) &&
	map->colormap == DefaultColormap(dpy, vinfo->screen)) {
	/* Put new entry first; it's more likely to be useful */
	for (i = 0; i < nProp; i++) copy[i+1] = prop[i];
	i = 0;
    } else {
	/* Put it at the end */
	for (i = 0; i < nProp; i++) copy[i] = prop[i];
	/* i = nProp; (it does already) */
    }

    copy[i] = *map;
    XSetRGBColormaps(dpy, RootWindow(dpy, vinfo->screen), copy, nProp+1, atom);
    
    free((void *) copy);
}
