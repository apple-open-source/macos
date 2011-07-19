/*
 * $XdotOrg: grid.c,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $
 * $Xorg: grid.c,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $
 *
 * 
Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 * *
 * Author:  Jim Fulton, MIT X Consortium
 */
/* $XFree86: xc/programs/xfd/grid.c,v 1.9 2002/07/06 00:46:42 keithp Exp $ */


#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/SimpleP.h>
#include <X11/Xmu/Converters.h>
#include <X11/Xos.h>
#include "gridP.h"

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#else
#define	XkbBI_MinorError		2
#define	XkbBI_Ignore			11
#endif

#ifdef XKB
#define Bell(w,n) XkbStdBell(XtDisplay(w), XtWindow(w), 50, n)
#else
#define Bell(w,n) XBell(XtDisplay(w), 0)
#endif

static GC get_gc(FontGridWidget fgw, Pixel fore);
static void ClassInitialize(void);
static void Initialize(Widget request, Widget new, ArgList args, 
		       Cardinal *num_args);
static void Realize(Widget gw, Mask *valueMask, 
		    XSetWindowAttributes *attributes);
static void Destroy(Widget gw);
static void Resize(Widget gw);
static void Redisplay(Widget gw, XEvent *event, Region region);
static void paint_grid(FontGridWidget fgw, int col, int row, 
		       int ncols, int nrows);
static Boolean SetValues(Widget current, Widget request, Widget new, 
			 ArgList args, Cardinal *num_args);
static void Notify(Widget gw, XEvent *event, String *params, 
		   Cardinal *nparams);

#define Offset(field) XtOffsetOf(FontGridRec, fontgrid.field)

static XtResource resources[] = {
    { XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	Offset(text_font), XtRString, (XtPointer) NULL },
    { XtNcellColumns, XtCCellColumns, XtRInt, sizeof(int),
	Offset(cell_cols), XtRImmediate, (XtPointer) 0 },
    { XtNcellRows, XtCCellRows, XtRInt, sizeof(int),
	Offset(cell_rows), XtRImmediate, (XtPointer) 0 },
    { XtNcellWidth, XtCCellWidth, XtRInt, sizeof(int),
	Offset(cell_width), XtRImmediate, (XtPointer) 0 },
    { XtNcellHeight, XtCCellHeight, XtRInt, sizeof(int),
	Offset(cell_height), XtRImmediate, (XtPointer) 0 },
    { XtNstartChar, XtCStartChar, XtRLong, sizeof(long),
	Offset(start_char), XtRImmediate, (XtPointer) 0xffffffff },
#ifndef XRENDER
    { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	Offset(foreground_pixel), XtRString, (XtPointer) XtDefaultForeground },
#endif
    { XtNcenterChars, XtCCenterChars, XtRBoolean, sizeof(Boolean),
	Offset(center_chars), XtRImmediate, (XtPointer) FALSE },
    { XtNboxChars, XtCBoxChars, XtRBoolean, sizeof(Boolean),
	Offset(box_chars), XtRImmediate, (XtPointer) FALSE },
    { XtNboxColor, XtCForeground, XtRPixel, sizeof(Pixel),
	Offset(box_pixel), XtRString, (XtPointer) XtDefaultForeground },
    { XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	Offset(callbacks), XtRCallback, (XtPointer) NULL },
    { XtNinternalPad, XtCInternalPad, XtRInt, sizeof(int),
	Offset(internal_pad), XtRImmediate, (XtPointer) 4 },
    { XtNgridWidth, XtCGridWidth, XtRInt, sizeof(int),
	Offset(grid_width), XtRImmediate, (XtPointer) 1 },
#ifdef XRENDER
    {XtNforeground, XtCForeground, XtRXftColor, sizeof(XftColor),
        Offset(fg_color), XtRString, XtDefaultForeground},
    {XtNface, XtCFace, XtRXftFont, sizeof (XftFont *),
	Offset (text_face), XtRString, NULL},
#endif
};

#undef Offset

static char defaultTranslations[] = 
  "<ButtonPress>:  notify()";

static XtActionsRec actions_list[] = {
    { "notify",		Notify },
};

FontGridClassRec fontgridClassRec = {
  { /* core fields */
    /* superclass               */      (WidgetClass) &simpleClassRec,
    /* class_name               */      "FontGrid",
    /* widget_size              */      sizeof(FontGridRec),
    /* class_initialize         */      ClassInitialize,
    /* class_part_initialize    */      NULL,
    /* class_inited             */      FALSE,
    /* initialize               */      Initialize,
    /* initialize_hook          */      NULL,
    /* realize                  */      Realize,
    /* actions                  */      actions_list,
    /* num_actions              */      XtNumber(actions_list),
    /* resources                */      resources,
    /* num_resources            */      XtNumber(resources),
    /* xrm_class                */      NULLQUARK,
    /* compress_motion          */      TRUE,
    /* compress_exposure        */      TRUE,
    /* compress_enterleave      */      TRUE,
    /* visible_interest         */      FALSE,
    /* destroy                  */      Destroy,
    /* resize                   */      Resize,
    /* expose                   */      Redisplay,
    /* set_values               */      SetValues,
    /* set_values_hook          */      NULL,
    /* set_values_almost        */      XtInheritSetValuesAlmost,
    /* get_values_hook          */      NULL,
    /* accept_focus             */      NULL,
    /* version                  */      XtVersion,
    /* callback_private         */      NULL,
    /* tm_table                 */      defaultTranslations,
    /* query_geometry           */      XtInheritQueryGeometry,
    /* display_accelerator      */      XtInheritDisplayAccelerator,
    /* extension                */      NULL
  },
  { /* simple fields */
    /* change_sensitive		*/	XtInheritChangeSensitive
  }
};

WidgetClass fontgridWidgetClass = (WidgetClass) &fontgridClassRec;


long
GridFirstChar (Widget w)
{
    FontGridWidget	fgw = (FontGridWidget) w;
    XFontStruct		*fs = fgw->fontgrid.text_font;
#ifdef XRENDER
    XftFont		*xft = fgw->fontgrid.text_face;
    if (xft)
    {
	FcChar32    map[FC_CHARSET_MAP_SIZE];
	FcChar32    next;
	FcChar32    first;
	int	    i;

	first = FcCharSetFirstPage (xft->charset, map, &next);
	for (i = 0; i < FC_CHARSET_MAP_SIZE; i++)
	    if (map[i])
	    {
		FcChar32    bits = map[i];
		first += i * 32;
		while (!(bits & 0x1))
		{
		    bits >>= 1;
		    first++;
		}
		break;
	    }
	return first;
    }
    else
#endif
    if (fs)
    {
	return (fs->min_byte1 << 8) | (fs->min_char_or_byte2);
    }
    else
	return 0;
}

long
GridLastChar (Widget w)
{
    FontGridWidget	fgw = (FontGridWidget) w;
    XFontStruct		*fs = fgw->fontgrid.text_font;
#ifdef XRENDER
    XftFont		*xft = fgw->fontgrid.text_face;
    if (xft)
    {
	FcChar32    this, last, next;
	FcChar32    map[FC_CHARSET_MAP_SIZE];
	int	    i;
	last = FcCharSetFirstPage (xft->charset, map, &next);
	while ((this = FcCharSetNextPage (xft->charset, map, &next)) != FC_CHARSET_DONE)
	    last = this;
	last &= ~0xff;
	for (i = FC_CHARSET_MAP_SIZE - 1; i >= 0; i--)
	    if (map[i])
	    {
		FcChar32    bits = map[i];
		last += i * 32 + 31;
		while (!(bits & 0x80000000))
		{
		    last--;
		    bits <<= 1;
		}
		break;
	    }
	return (long) last;
    }
    else
#endif
    if (fs)
    {
	return (fs->max_byte1 << 8) | (fs->max_char_or_byte2);
    }
    else
	return 0;
}

/*
 * CI_GET_CHAR_INFO_1D - return the charinfo struct for the indicated 8bit
 * character.  If the character is in the column and exists, then return the
 * appropriate metrics (note that fonts with common per-character metrics will
 * return min_bounds).
 */

#define CI_NONEXISTCHAR(cs) (((cs)->width == 0) && \
			     (((cs)->rbearing|(cs)->lbearing| \
			       (cs)->ascent|(cs)->descent) == 0))

#define CI_GET_CHAR_INFO_1D(fs,col,cs) \
{ \
    cs = NULL; \
    if (col >= fs->min_char_or_byte2 && col <= fs->max_char_or_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[(col - fs->min_char_or_byte2)]; \
	} \
	if (CI_NONEXISTCHAR(cs)) \
	    cs = NULL; \
    } \
}

/*
 * CI_GET_CHAR_INFO_2D - return the charinfo struct for the indicated row and 
 * column.  This is used for fonts that have more than row zero.
 */
#define CI_GET_CHAR_INFO_2D(fs,row,col,cs) \
{ \
    cs = NULL; \
    if (row >= fs->min_byte1 && row <= fs->max_byte1 && \
	col >= fs->min_char_or_byte2 && col <= fs->max_char_or_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[((row - fs->min_byte1) * \
			        (fs->max_char_or_byte2 - \
				 fs->min_char_or_byte2 + 1)) + \
			       (col - fs->min_char_or_byte2)]; \
        } \
	if (CI_NONEXISTCHAR(cs)) \
	    cs = NULL; \
    } \
}

static Boolean
GridHasChar (Widget w, long ch)
{
    FontGridWidget	fgw = (FontGridWidget) w;
#ifdef XRENDER
    XftFont		*xft = fgw->fontgrid.text_face;
    if (xft)
    {
	return FcCharSetHasChar (xft->charset, (FcChar32) ch);
    }
    else
#endif
    {
	XFontStruct	*fs = fgw->fontgrid.text_font;
	XCharStruct	*cs;
	
	if (!fs)
	    return False;
	if (fs->max_byte1 == 0)
	{
	    CI_GET_CHAR_INFO_1D (fs, ch, cs);
	}
	else
	{
	    unsigned int	r = (ch >> 8);
	    unsigned int	c = (ch & 0xff);
	    CI_GET_CHAR_INFO_2D (fs, r, c, cs);
	}
	return cs != NULL;
    }
}

/*
 * public routines
 */

void 
GetFontGridCellDimensions(Widget w, long *startp, 
			  int *ncolsp, int *nrowsp)
{
    FontGridWidget fgw = (FontGridWidget) w;
    *startp = fgw->fontgrid.start_char;
    *ncolsp = fgw->fontgrid.cell_cols;
    *nrowsp = fgw->fontgrid.cell_rows;
}


void 
GetPrevNextStates(Widget w, Bool *prevvalidp, Bool *nextvalidp,
		  Bool *prev16validp, Bool *next16validp)
{
    FontGridWidget fgw = (FontGridWidget) w;
    long minn = (long) GridFirstChar (w);
    long maxn = (long) GridLastChar (w);

    *prev16validp = (fgw->fontgrid.start_char - 0xf00 > minn);
    *prevvalidp = (fgw->fontgrid.start_char > minn);
    *nextvalidp = (fgw->fontgrid.start_char +
		   (fgw->fontgrid.cell_cols * fgw->fontgrid.cell_rows)
		   < maxn);
    *next16validp =((fgw->fontgrid.start_char + 0xf00 +
		    (fgw->fontgrid.cell_cols * fgw->fontgrid.cell_rows))
		   < maxn);
}



/*
 * private routines and methods
 */


static GC 
get_gc(FontGridWidget fgw, Pixel fore)
{
    XtGCMask mask;
    XGCValues gcv;

    mask = (GCForeground | GCBackground | GCFunction);
    gcv.foreground = fore;
    gcv.background = fgw->core.background_pixel;
    gcv.function = GXcopy;
    if (fgw->fontgrid.text_font)
    {
	mask |= GCFont;
	gcv.font = fgw->fontgrid.text_font->fid;
    }
    gcv.cap_style = CapProjecting;
    mask |= GCCapStyle;
    if (fgw->fontgrid.grid_width > 0) {
	mask |= GCLineWidth;
	gcv.line_width = ((fgw->fontgrid.grid_width < 2) ? 0 : 
			  fgw->fontgrid.grid_width);
    }
    return (XtGetGC ((Widget) fgw, mask, &gcv));
}


#ifdef XRENDER
static XtConvertArgRec xftColorConvertArgs[] = {
    {XtWidgetBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.screen),
     sizeof(Screen *)},
    {XtWidgetBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.colormap),
     sizeof(Colormap)}
};

#define	donestr(type, value, tstr) \
	{							\
	    if (toVal->addr != NULL) {				\
		if (toVal->size < sizeof(type)) {		\
		    toVal->size = sizeof(type);			\
		    XtDisplayStringConversionWarning(dpy, 	\
			(char*) fromVal->addr, tstr);		\
		    return False;				\
		}						\
		*(type*)(toVal->addr) = (value);		\
	    }							\
	    else {						\
		static type static_val;				\
		static_val = (value);				\
		toVal->addr = (XPointer)&static_val;		\
	    }							\
	    toVal->size = sizeof(type);				\
	    return True;					\
	}

static void
XmuFreeXftColor (XtAppContext app, XrmValuePtr toVal, XtPointer closure,
		 XrmValuePtr args, Cardinal *num_args)
{
    Screen	*screen;
    Colormap	colormap;
    XftColor	*color;
    
    if (*num_args != 2)
    {
	XtAppErrorMsg (app,
		       "freeXftColor", "wrongParameters",
		       "XtToolkitError",
		       "Freeing an XftColor requires screen and colormap arguments",
		       (String *) NULL, (Cardinal *)NULL);
	return;
    }

    screen = *((Screen **) args[0].addr);
    colormap = *((Colormap *) args[1].addr);
    color = (XftColor *) toVal->addr;
    XftColorFree (DisplayOfScreen (screen),
		  DefaultVisual (DisplayOfScreen (screen),
				 XScreenNumberOfScreen (screen)),
		  colormap, color);
}
    
static Boolean
XmuCvtStringToXftColor(Display *dpy,
		       XrmValue *args, Cardinal *num_args,
		       XrmValue *fromVal, XrmValue *toVal,
		       XtPointer *converter_data)
{
    char	    *spec;
    XRenderColor    renderColor;
    XftColor	    xftColor;
    Screen	    *screen;
    Colormap	    colormap;
    
    if (*num_args != 2)
    {
	XtAppErrorMsg (XtDisplayToApplicationContext (dpy),
		       "cvtStringToXftColor", "wrongParameters",
		       "XtToolkitError",
		       "String to render color conversion needs screen and colormap arguments",
		       (String *) NULL, (Cardinal *)NULL);
	return False;
    }

    screen = *((Screen **) args[0].addr);
    colormap = *((Colormap *) args[1].addr);

    spec = (char *) fromVal->addr;
    if (strcasecmp (spec, XtDefaultForeground) == 0)
    {
	renderColor.red = 0;
	renderColor.green = 0;
	renderColor.blue = 0;
	renderColor.alpha = 0xffff;
    }
    else if (strcasecmp (spec, XtDefaultBackground) == 0)
    {
	renderColor.red = 0xffff;
	renderColor.green = 0xffff;
	renderColor.blue = 0xffff;
	renderColor.alpha = 0xffff;
    }
    else if (!XRenderParseColor (dpy, spec, &renderColor))
	return False;
    if (!XftColorAllocValue (dpy, 
			     DefaultVisual (dpy,
					    XScreenNumberOfScreen (screen)),
			     colormap,
			     &renderColor,
			     &xftColor))
	return False;
    
    donestr (XftColor, xftColor, XtRXftColor);
}

static void
XmuFreeXftFont (XtAppContext app, XrmValuePtr toVal, XtPointer closure,
		XrmValuePtr args, Cardinal *num_args)
{
    Screen  *screen;
    XftFont *font;
    
    if (*num_args != 1)
    {
	XtAppErrorMsg (app,
		       "freeXftFont", "wrongParameters",
		       "XtToolkitError",
		       "Freeing an XftFont requires screen argument",
		       (String *) NULL, (Cardinal *)NULL);
	return;
    }

    screen = *((Screen **) args[0].addr);
    font = *((XftFont **) toVal->addr);
    if (font)
	XftFontClose (DisplayOfScreen (screen), font);
}

static Boolean
XmuCvtStringToXftFont(Display *dpy,
		      XrmValue *args, Cardinal *num_args,
		      XrmValue *fromVal, XrmValue *toVal,
		      XtPointer *converter_data)
{
    char    *name;
    XftFont *font;
    Screen  *screen;
    
    if (*num_args != 1)
    {
	XtAppErrorMsg (XtDisplayToApplicationContext (dpy),
		       "cvtStringToXftFont", "wrongParameters",
		       "XtToolkitError",
		       "String to XftFont conversion needs screen argument",
		       (String *) NULL, (Cardinal *)NULL);
	return False;
    }

    screen = *((Screen **) args[0].addr);
    name = (char *) fromVal->addr;

    font = NULL;
    if (name)
    {
	font = XftFontOpenName (dpy,
				XScreenNumberOfScreen (screen),
				name);
	if (!font)
	{
	    XtDisplayStringConversionWarning(dpy, (char *) fromVal->addr, XtRXftFont);
	    return False;
	}
    }
    donestr (XftFont *, font, XtRXftFont);
}

static XtConvertArgRec xftFontConvertArgs[] = {
    {XtWidgetBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.screen),
     sizeof(Screen *)},
};

#endif

static void 
ClassInitialize(void)
{
    XtAddConverter (XtRString, XtRLong, XmuCvtStringToLong, NULL, 0);
#ifdef XRENDER
    XtSetTypeConverter (XtRString, XtRXftColor, 
			XmuCvtStringToXftColor, 
			xftColorConvertArgs, XtNumber(xftColorConvertArgs),
			XtCacheByDisplay, XmuFreeXftColor);
    XtSetTypeConverter (XtRString, XtRXftFont,
			XmuCvtStringToXftFont,
			xftFontConvertArgs, XtNumber(xftFontConvertArgs),
			XtCacheByDisplay, XmuFreeXftFont);
#endif
}


static void 
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    FontGridWidget reqfg = (FontGridWidget) request;
    FontGridWidget newfg = (FontGridWidget) new;
    XFontStruct *fs = newfg->fontgrid.text_font;
#ifdef XRENDER
    XftFont *xft = newfg->fontgrid.text_face;
#endif
    unsigned maxn;

    if (reqfg->fontgrid.cell_cols <= 0)
      newfg->fontgrid.cell_cols = 16;

    if (reqfg->fontgrid.cell_rows <= 0) {
#ifdef XRENDER
	if (xft)
	    newfg->fontgrid.cell_rows = 16;
	else
#endif
	if (fs && fs->max_byte1 == 0) {
	    newfg->fontgrid.cell_rows = (fs->max_char_or_byte2 / 
					 newfg->fontgrid.cell_cols) + 1;
	    if (newfg->fontgrid.cell_rows > 16)
	      newfg->fontgrid.cell_rows = 16;
	} else
	  newfg->fontgrid.cell_rows = 16;
    }

    if (reqfg->fontgrid.cell_width <= 0)
      newfg->fontgrid.cell_width = DefaultCellWidth (newfg);
    if (reqfg->fontgrid.cell_height <= 0)
      newfg->fontgrid.cell_height = DefaultCellHeight (newfg);

    /* give a nice size that fits one screen full */
    if (newfg->core.width == 0)
      newfg->core.width = (newfg->fontgrid.cell_width *
			   newfg->fontgrid.cell_cols +
			   newfg->fontgrid.grid_width *
			   (newfg->fontgrid.cell_cols + 1));

    if (newfg->core.height == 0) 
      newfg->core.height = (newfg->fontgrid.cell_height * 
			    newfg->fontgrid.cell_rows +
			    newfg->fontgrid.grid_width *
			    (newfg->fontgrid.cell_rows + 1));

    /*
     * select the first character
     */

    if (newfg->fontgrid.start_char == 0xffffffff)
	newfg->fontgrid.start_char = GridFirstChar(new) & ~0xff;
    maxn = GridLastChar (new);
    if (newfg->fontgrid.start_char > maxn) 
	newfg->fontgrid.start_char = (maxn + 1 - 
				      (newfg->fontgrid.cell_cols * 
				       newfg->fontgrid.cell_rows));
}

static void 
Realize(Widget gw, Mask *valueMask, XSetWindowAttributes *attributes)
{
    FontGridWidget fgw = (FontGridWidget) gw;
    FontGridPart *p = &fgw->fontgrid;

    p->text_gc = get_gc (fgw, GridForeground (fgw));
    p->box_gc = get_gc (fgw, p->box_pixel);
    Resize (gw);

    (*(XtSuperclass(gw)->core_class.realize)) (gw, valueMask, attributes);
#ifdef XRENDER
    p->draw = XftDrawCreate (XtDisplay (gw), XtWindow (gw),
			     DefaultVisual (XtDisplay (gw),
					    DefaultScreen(XtDisplay (gw))),
			     fgw->core.colormap);
#endif
    return;
}



static void 
Destroy(Widget gw)
{
    FontGridWidget fgw = (FontGridWidget) gw;

    XtReleaseGC (gw, fgw->fontgrid.text_gc);
    XtReleaseGC (gw, fgw->fontgrid.box_gc);
}


static void 
Resize(Widget gw)
{
    FontGridWidget fgw = (FontGridWidget) gw;

    /* recompute in case we've changed size */
    fgw->fontgrid.cell_width = CellWidth (fgw);
    if (fgw->fontgrid.cell_width <= 0)
	fgw->fontgrid.cell_width = 1;
    fgw->fontgrid.cell_height = CellHeight (fgw);
    if (fgw->fontgrid.cell_height <= 0)
	fgw->fontgrid.cell_height = 1;
    fgw->fontgrid.xoff = (fgw->fontgrid.cell_width -
			    DefaultCellWidth (fgw)) / 2;
    fgw->fontgrid.yoff = (fgw->fontgrid.cell_height -
			    DefaultCellHeight (fgw)) / 2;

}


/* ARGSUSED */
static void 
Redisplay(Widget gw, XEvent *event, Region region)
{
    FontGridWidget fgw = (FontGridWidget) gw;
    XRectangle rect;			/* bounding rect for region */
    int left, right, top, bottom;	/* which cells were damaged */
    int cw, ch;				/* cell size */

#ifdef XRENDER
    if (!fgw->fontgrid.text_face)
#endif
    if (!fgw->fontgrid.text_font) {
	Bell (gw, XkbBI_BadValue);
	return;
    }

    /*
     * compute the left, right, top, and bottom cells that were damaged
     */
    XClipBox (region, &rect);
    cw = fgw->fontgrid.cell_width + fgw->fontgrid.grid_width;
    ch = fgw->fontgrid.cell_height + fgw->fontgrid.grid_width;
    if ((left = (((int) rect.x) / cw)) < 0) left = 0;
    right = (((int) (rect.x + rect.width - 1)) / cw);
    if ((top = (((int) rect.y) / ch)) < 0) top = 0;
    bottom = (((int) (rect.y + rect.height - 1)) / ch);

    paint_grid (fgw, left, top, right - left + 1, bottom - top + 1);
}


static void 
paint_grid(FontGridWidget fgw, 		/* widget in which to draw */ 
	   int col, int row, 		/* where to start */          
	   int ncols, int nrows)	/* number of cells */         
{
    FontGridPart *p = &fgw->fontgrid;
    int i, j;
    Display *dpy = XtDisplay(fgw);
    Window wind = XtWindow(fgw);
    int cw = p->cell_width + p->grid_width;
    int ch = p->cell_height + p->grid_width;
    int tcols = p->cell_cols;
    int trows = p->cell_rows;
    int x1, y1, x2, y2, x, y;
    unsigned maxn = GridLastChar ((Widget) fgw);
    unsigned n, prevn;
    int startx;

    if (col + ncols >= tcols) {
	ncols = tcols - col;
	if (ncols < 1) return;
    }

    if (row + nrows >= trows) {
	nrows = trows - row;
	if (nrows < 1) return;
    }

    /*
     * paint the grid lines for the indicated rows 
     */
    if (p->grid_width > 0) {
	int half_grid_width = p->grid_width >> 1;
	x1 = col * cw + half_grid_width;
	y1 = row * ch + half_grid_width;
	x2 = x1 + ncols * cw;
	y2 = y1 + nrows * ch;
	for (i = 0, x = x1; i <= ncols; i++, x += cw) {
	    XDrawLine (dpy, wind, p->box_gc, x, y1, x, y2);
	}
	for (i = 0, y = y1; i <= nrows; i++, y += ch) {
	    XDrawLine (dpy, wind, p->box_gc, x1, y, x2, y);
	}
    }
    /*
     * Draw a character in every box; treat all fonts as if they were 16bit
     * fonts.  Store the high eight bits in byte1 and the low eight bits in 
     * byte2.
     */
    prevn = p->start_char + col + row * tcols;
    startx = col * cw + p->internal_pad + p->grid_width;
    for (j = 0,
	 y = row * ch + p->internal_pad + p->grid_width + GridFontAscent (fgw);
	 j < nrows; j++, y += ch) {
	n = prevn;
	for (i = 0, x = startx; i < ncols; i++, x += cw) {
	    int xoff = p->xoff, yoff = p->yoff;
	    if (n > maxn) goto done;	/* no break out of nested */

#ifdef XRENDER
	    if (fgw->fontgrid.text_face)
	    {
		XftFont *xft = p->text_face;
		FcChar32    c = n;
		XGlyphInfo  extents;
		XftTextExtents32 (dpy, xft, &c, 1, &extents);
		if (p->center_chars)
		{
		    xoff = (p->cell_width - extents.width) / 2 - extents.x;
		    yoff = (p->cell_height - extents.height) / 2 - extents.y;
		}
		if (extents.width && extents.height)
		{
		    XClearArea (dpy, wind, x + xoff - extents.x, 
				y + yoff - extents.y,
				extents.width, extents.height, False);
		    if (p->box_chars)
			XDrawRectangle (dpy, wind, p->box_gc,
					x + xoff - extents.x, 
					y + yoff - extents.y,
					extents.width - 1,
					extents.height - 1);
		}
		XftDrawString32 (p->draw, &p->fg_color, xft,
				 x + xoff, y + yoff, &c, 1);
	    }
	    else
#endif
	    {
	    XChar2b thechar;

	    thechar.byte1 = (n >> 8);	/* high eight bits */
	    thechar.byte2 = (n & 255);	/* low eight bits */
	    if (p->box_chars || p->center_chars) {
		XCharStruct metrics;
		int direction, fontascent, fontdescent;

		XTextExtents16 (p->text_font, &thechar, 1, &direction,
				&fontascent, &fontdescent, &metrics);

		if (p->center_chars) {
		    /*
		     * We want to move the origin by enough to center the ink
		     * within the cell.  The left edge will then be at 
		     * (cell_width - (rbearing - lbearing)) / 2; so we subtract
		     * the lbearing to find the origin.  Ditto for vertical.
		     */
		    xoff = (((p->cell_width -
			      (metrics.rbearing - metrics.lbearing)) / 2) -
			    p->internal_pad - metrics.lbearing);
		    yoff = (((p->cell_height - 
			      (metrics.descent + metrics.ascent)) / 2) -
			    p->internal_pad -
			    p->text_font->ascent + metrics.ascent);
		}
		if (p->box_chars) {
		    XDrawRectangle (dpy, wind, p->box_gc,
				    x + xoff, y + yoff - p->text_font->ascent, 
				    metrics.width - 1,
				    fontascent + fontdescent - 1);
		}
	    }
	    XDrawString16 (dpy, wind, p->text_gc, x + xoff, y + yoff,
			   &thechar, 1);
	    }
	    n++;
	}
	prevn += tcols;
    }

  done:
    /*
     * paint the grid lines for the indicated rows 
     */
    if (p->grid_width > 0) {
	int half_grid_width = p->grid_width >> 1;
	x1 = col * cw + half_grid_width;
	y1 = row * ch + half_grid_width;
	x2 = x1 + ncols * cw;
	y2 = y1 + nrows * ch;
	for (i = 0, x = x1; i <= ncols; i++, x += cw) {
	    XDrawLine (dpy, wind, p->box_gc, x, y1, x, y2);
	}
	for (i = 0, y = y1; i <= nrows; i++, y += ch) {
	    XDrawLine (dpy, wind, p->box_gc, x1, y, x2, y);
	}
    }

	
    return;
}

static Boolean
PageBlank (Widget w, long first, long last)
{
    while (first <= last)
    {
	if (GridHasChar (w, first))
	    return False;
	first++;
    }
    return True;
}

/*ARGSUSED*/
static Boolean 
SetValues(Widget current, Widget request, Widget new, 
	  ArgList args, Cardinal *num_args)
{
    FontGridWidget curfg = (FontGridWidget) current;
    FontGridWidget newfg = (FontGridWidget) new;
    Boolean redisplay = FALSE;

    if (curfg->fontgrid.text_font != newfg->fontgrid.text_font ||
	curfg->fontgrid.internal_pad != newfg->fontgrid.internal_pad) {
	newfg->fontgrid.cell_width = DefaultCellWidth (newfg);
	newfg->fontgrid.cell_height = DefaultCellHeight (newfg);
	redisplay = TRUE;
    }

    if (GridForeground(curfg) != GridForeground (newfg)) {
	XtReleaseGC (new, curfg->fontgrid.text_gc);
	newfg->fontgrid.text_gc = get_gc (newfg, GridForeground (newfg));
	redisplay = TRUE;
    }

    if (curfg->fontgrid.box_pixel != newfg->fontgrid.box_pixel) {
	XtReleaseGC (new, curfg->fontgrid.text_gc);
	newfg->fontgrid.box_gc = get_gc (newfg, newfg->fontgrid.box_pixel);
	redisplay = TRUE;
    }

    if (curfg->fontgrid.center_chars != newfg->fontgrid.center_chars ||
	curfg->fontgrid.box_chars != newfg->fontgrid.box_chars)
      redisplay = TRUE;

    if (curfg->fontgrid.start_char != newfg->fontgrid.start_char) {
	long maxn = GridLastChar (new);
	long page = newfg->fontgrid.cell_cols * newfg->fontgrid.cell_rows;
	long dir = page;
	long start = newfg->fontgrid.start_char;

	if (start < curfg->fontgrid.start_char)
	    dir = -page;

	if (start < 0)
	    start = 0;
	if (start > maxn) 
	  start = (maxn / page) * page;
	
	while (PageBlank (new, start, start + page - 1))
	{
	    long    next = start + dir;

	    if (next < 0 || maxn < next)
		break;
	    start = next;
	}

	newfg->fontgrid.start_char = start;
	redisplay = (curfg->fontgrid.start_char != newfg->fontgrid.start_char);
    }

    return redisplay;
}


/* ARGSUSED */
static void 
Notify(Widget gw, XEvent *event, String *params, Cardinal *nparams)
{
    FontGridWidget fgw = (FontGridWidget) gw;
    int x, y;				/* where the event happened */
    FontGridCharRec rec;		/* callback data */

    /*
     * only allow events with (x,y)
     */
    switch (event->type) {
      case KeyPress:
      case KeyRelease:
	x = event->xkey.x;
	y = event->xkey.y;
	break;
      case ButtonPress:
      case ButtonRelease:
	x = event->xbutton.x;
	y = event->xbutton.y;
	break;
      case MotionNotify:
	x = event->xmotion.x;
	y = event->xmotion.y;
	break;
      default:
	Bell (gw, XkbBI_Ignore);
	return;
    }

    /*
     * compute the callback data
     */
    {
	int cw = fgw->fontgrid.cell_width + fgw->fontgrid.grid_width;
	int ch = fgw->fontgrid.cell_height + fgw->fontgrid.grid_width;
	unsigned n;

	if (x > (fgw->fontgrid.cell_cols * cw)) {
	    Bell (gw, XkbBI_InvalidLocation);
	    return;
	}

	n= (fgw->fontgrid.start_char + 
	    ((y / ch) * fgw->fontgrid.cell_cols) + (x / cw));

	rec.thefont = fgw->fontgrid.text_font;
#ifdef XRENDER
	rec.theface = fgw->fontgrid.text_face;
#endif
	rec.thechar = n;
    }

    XtCallCallbacks (gw, XtNcallback, (XtPointer) &rec);
}

