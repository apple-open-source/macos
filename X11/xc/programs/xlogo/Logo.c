/* $Xorg: Logo.c,v 1.4 2001/02/09 02:05:54 xorgcvs Exp $ */
/*

Copyright 1988, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xlogo/Logo.c,v 1.6 2002/05/23 23:53:59 keithp Exp $ */

#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/Xmu/Drawing.h>
#include "LogoP.h"
#include <X11/extensions/shape.h>

#ifdef XRENDER
#include "RenderLogo.h"
#endif

static XtResource resources[] = {
    {XtNshapeWindow, XtCShapeWindow, XtRBoolean, sizeof (Boolean),
       XtOffsetOf(LogoRec,logo.shape_window), XtRImmediate, 
       (XtPointer) FALSE},
#ifdef XRENDER
    {XtNrender, XtCBoolean, XtRBoolean, sizeof(Boolean),
       XtOffsetOf(LogoRec,logo.render), XtRImmediate,
       (XtPointer) FALSE },
    {XtNsharp, XtCBoolean, XtRBoolean, sizeof(Boolean),
       XtOffsetOf(LogoRec,logo.sharp), XtRImmediate,
       (XtPointer) FALSE },
    {XtNforeground, XtCForeground, XtRXftColor, sizeof(XftColor),
       XtOffsetOf(LogoRec, logo.fg), XtRString,
       (XtPointer) XtDefaultForeground},
    {XtNbackground, XtCForeground, XtRXftColor, sizeof(XftColor),
       XtOffsetOf(LogoRec, logo.bg), XtRString,
       (XtPointer) XtDefaultBackground},
#else
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        XtOffsetOf(LogoRec,logo.fgpixel), XtRString,
       (XtPointer) XtDefaultForeground},
#endif
};

static void ClassInitialize ( void );
static void Initialize ( Widget request, Widget new, ArgList args, 
			 Cardinal *num_args );
static void Destroy ( Widget gw );
static void Realize ( Widget gw, XtValueMask *valuemaskp, 
		      XSetWindowAttributes *attr );
static void Resize ( Widget gw );
static void Redisplay ( Widget gw, XEvent *event, Region region );
static Boolean SetValues ( Widget gcurrent, Widget grequest, Widget gnew, 
			   ArgList args, Cardinal *num_args );

LogoClassRec logoClassRec = {
    { /* core fields */
    /* superclass		*/	(WidgetClass) &simpleClassRec,
    /* class_name		*/	"Logo",
    /* widget_size		*/	sizeof(LogoRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* resource_count		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	Resize,
    /* expose			*/	Redisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
    },
    { /* simple fields */
    /* change_sensitive         */      XtInheritChangeSensitive
    },
    { /* logo fields */
    /* ignore                   */      0
    }
};

WidgetClass logoWidgetClass = (WidgetClass) &logoClassRec;


/*****************************************************************************
 *									     *
 *			   private utility routines			     *
 *									     *
 *****************************************************************************/

static void 
create_gcs(LogoWidget w)
{
    XGCValues v;

#ifdef XRENDER
    w->logo.fgpixel = w->logo.fg.pixel;
#endif

    v.foreground = w->logo.fgpixel;
    w->logo.foreGC = XtGetGC ((Widget) w, GCForeground, &v);
    v.foreground = w->core.background_pixel;
    w->logo.backGC = XtGetGC ((Widget) w, GCForeground, &v);
}

static void 
check_shape(LogoWidget w)
{
    if (w->logo.shape_window) {
	int event_base, error_base;

	if (!XShapeQueryExtension (XtDisplay (w), &event_base, &error_base))
	  w->logo.shape_window = FALSE;
    }
}

/* ARGSUSED */
static void 
unset_shape(LogoWidget w)
{
    XSetWindowAttributes attr;
    unsigned long mask;
    Display *dpy = XtDisplay ((Widget) w);
    Window win = XtWindow ((Widget) w);

    if (w->core.background_pixmap != None && 
	w->core.background_pixmap != XtUnspecifiedPixmap) {
	attr.background_pixmap = w->core.background_pixmap;
	mask = CWBackPixmap;
    } else {
	attr.background_pixel = w->core.background_pixel;
	mask = CWBackPixel;
    }
    XChangeWindowAttributes (dpy, win, mask, &attr);
    XShapeCombineMask (dpy, win, ShapeBounding, 0, 0, None, ShapeSet);
    if (!w->logo.foreGC) create_gcs (w);
    w->logo.need_shaping = w->logo.shape_window;
}

static void 
set_shape(LogoWidget w)
{
    GC ones, zeros;
    Display *dpy = XtDisplay ((Widget) w);
    Window win = XtWindow ((Widget) w);
    unsigned int width = (unsigned int) w->core.width;
    unsigned int height = (unsigned int) w->core.height;
    Pixmap pm = XCreatePixmap (dpy, win, width, height, (unsigned int) 1);
    XGCValues v;

    v.foreground = (Pixel) 1;
    v.background = (Pixel) 0;
    ones = XCreateGC (dpy, pm, (GCForeground | GCBackground), &v);
    v.foreground = (Pixel) 0;
    v.background = (Pixel) 1;
    zeros = XCreateGC (dpy, pm, (GCForeground | GCBackground), &v);

    if (pm && ones && zeros) {
	int x = 0, y = 0;
	Widget parent;

	XmuDrawLogo (dpy, pm, ones, zeros, 0, 0, width, height);
	for (parent = (Widget) w; XtParent(parent);
	     parent = XtParent(parent)) {
	    x += parent->core.x + parent->core.border_width;
	    y += parent->core.y + parent->core.border_width;
	}
	XShapeCombineMask (dpy, XtWindow (parent), ShapeBounding,
			   x, y, pm, ShapeSet);
	w->logo.need_shaping = FALSE;
    } else {
	unset_shape (w);
    }
    if (ones) XFreeGC (dpy, ones);
    if (zeros) XFreeGC (dpy, zeros);
    if (pm) XFreePixmap (dpy, pm);
}


/*****************************************************************************
 *									     *
 *				 class methods				     *
 *									     *
 *****************************************************************************/

#ifdef XRENDER

static void
RenderPrepare (LogoWidget w)
{
    if (!w->logo.draw)
    {
	w->logo.draw = XftDrawCreate (XtDisplay (w), XtWindow (w),
				       DefaultVisual (XtDisplay (w),
						      DefaultScreen(XtDisplay (w))),
				       w->core.colormap);
    }
}

XtConvertArgRec xftColorConvertArgs[] = {
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


#endif

static void 
ClassInitialize(void)
{
#ifdef XRENDER
    XtSetTypeConverter (XtRString, XtRXftColor, 
			XmuCvtStringToXftColor, 
			xftColorConvertArgs, XtNumber(xftColorConvertArgs),
			XtCacheByDisplay, XmuFreeXftColor);
#endif
}

/* ARGSUSED */
static void 
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    LogoWidget w = (LogoWidget)new;

#ifdef XRENDER
    w->logo.draw = 0;
    w->logo.fgpixel = w->logo.fg.pixel;
#endif
    if (w->core.width < 1) w->core.width = 100;
    if (w->core.height < 1) w->core.height = 100;

    w->logo.foreGC = (GC) NULL;
    w->logo.backGC = (GC) NULL;
    check_shape (w);
    w->logo.need_shaping = w->logo.shape_window;
}

static void 
Destroy(Widget gw)
{
    LogoWidget w = (LogoWidget) gw;
    if (w->logo.foreGC) {
	XtReleaseGC (gw, w->logo.foreGC);
	w->logo.foreGC = (GC) NULL;
    }
    if (w->logo.backGC) {
	XtReleaseGC (gw, w->logo.backGC);
	w->logo.backGC = (GC) NULL;
    }
}

static void 
Realize(Widget gw, XtValueMask *valuemaskp, XSetWindowAttributes *attr)
{
    LogoWidget w = (LogoWidget) gw;

    if (w->logo.shape_window) {
	attr->background_pixel = w->logo.fgpixel;  /* going to shape */
	*valuemaskp |= CWBackPixel;
    } else
      create_gcs (w);
    (*logoWidgetClass->core_class.superclass->core_class.realize)
	(gw, valuemaskp, attr);
}

static void 
Resize(Widget gw)
{
    LogoWidget w = (LogoWidget) gw;

    if (w->logo.shape_window && XtIsRealized(gw)) set_shape (w);
}

/* ARGSUSED */
static void 
Redisplay(Widget gw, XEvent *event, Region region)
{
    LogoWidget w = (LogoWidget) gw;

    if (w->logo.shape_window) {
	if (w->logo.need_shaping) set_shape (w);  /* may change shape flag */
    }
    if (!w->logo.shape_window) {
#ifdef XRENDER
	if (w->logo.render)
	{
	    RenderPrepare (w);

	    XClearWindow (XtDisplay(w), XtWindow(w));
	    RenderLogo (XtDisplay(w), PictOpOver,
			XftDrawSrcPicture (w->logo.draw, &w->logo.fg),
			XftDrawPicture (w->logo.draw),
			XRenderFindStandardFormat (XtDisplay (w),
						   w->logo.sharp ?
						   PictStandardA1:
						   PictStandardA8),
			0, 0, (unsigned int) w->core.width,
			(unsigned int) w->core.height);
	}
	else
#endif
	{
	    XmuDrawLogo (XtDisplay(w), XtWindow(w), w->logo.foreGC, w->logo.backGC,
			 0, 0, (unsigned int) w->core.width,
			 (unsigned int) w->core.height);
	}
    }
}

/* ARGSUSED */
static Boolean 
SetValues (Widget gcurrent, Widget grequest, Widget gnew, 
	   ArgList args, Cardinal *num_args)
{
    LogoWidget current = (LogoWidget) gcurrent;
    LogoWidget new = (LogoWidget) gnew;
    Boolean redisplay = FALSE;

    if (new->logo.shape_window &&
	new->logo.shape_window != current->logo.shape_window)
	check_shape (new);			/* validate shape_window */

    if ((new->logo.fgpixel != current->logo.fgpixel) ||
	(new->core.background_pixel != current->core.background_pixel)) {
	Destroy (gnew);
	if (!new->logo.shape_window) create_gcs (new);
	redisplay = TRUE;
    }
   
   if (new->logo.shape_window != current->logo.shape_window) {
       if (new->logo.shape_window) {
	   Destroy (gnew);
	   if (XtIsRealized(gnew))
	       set_shape (new);
	   else
	       new->logo.need_shaping = True;
	   redisplay = FALSE;
       } else {
	   if (XtIsRealized(gnew))
	       unset_shape (new);		/* creates new GCs */
	   redisplay = TRUE;
       }
   }

   return (redisplay);
}
