/* $Xorg: LED.c,v 1.3 2000/08/17 19:54:51 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
/* $XFree86: xc/programs/xkbutils/LED.c,v 1.4 2001/01/17 23:46:13 dawes Exp $ */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <X11/Xaw/XawInit.h>
#include "LEDP.h"
#include <stdio.h>
#include <ctype.h>
/* needed for abs() */
#include <stdlib.h>

#define streq(a,b) (strcmp( (a), (b) ) == 0)

#ifdef CRAY
#define WORD64
#endif

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

#define offset(field) XtOffsetOf(LEDRec, field)
static XtResource resources[] = {
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(led.foreground), XtRString, XtDefaultForeground},
    {XtNon, XtCOn, XtRBoolean, sizeof(Boolean),
	offset(led.on), XtRImmediate, (XtPointer)False},
    {XtNtopColor, XtCTopColor, XtRPixel, sizeof(Pixel),
	offset(led.top_color), XtRString, "black"},
    {XtNbottomColor, XtCBottomColor, XtRPixel, sizeof(Pixel),
	offset(led.bottom_color), XtRString, "white"},
    {XtNonColor, XtCOnColor, XtRPixel, sizeof(Pixel),
	offset(led.on_color), XtRString, "green"},
    {XtNoffColor, XtCOffColor, XtRPixel, sizeof(Pixel),
	offset(led.off_color), XtRString, "#005000"},
    {XtNbevel, XtCBevel, XtRDimension, sizeof(Dimension),
	offset(led.bevel), XtRImmediate, (XtPointer)1},
    {XtNledWidth, XtCLedWidth, XtRDimension, sizeof(Dimension),
	offset(led.led_width), XtRImmediate, (XtPointer)6},
    {XtNledHeight, XtCLedHeight, XtRDimension, sizeof(Dimension),
	offset(led.led_height), XtRImmediate, (XtPointer)12}
};
#undef offset

static void ClassInitialize ( void );
static void Initialize ( Widget request, Widget new, ArgList args, 
			 Cardinal *num_args );
static void Realize ( Widget w, Mask * mask, XSetWindowAttributes * xswa );
static void Resize ( Widget w );
static Boolean SetValues ( Widget current, Widget request, Widget new, 
			   ArgList args, Cardinal *num_args );
static void Destroy ( Widget w );
static XtGeometryResult QueryGeometry ( Widget w, XtWidgetGeometry *intended, 
					XtWidgetGeometry *preferred );

LEDClassRec ledClassRec = {
  {
/* core_class fields */	
    /* superclass	  	*/	(WidgetClass) &simpleClassRec,
    /* class_name	  	*/	"LED",
    /* widget_size	  	*/	sizeof(LEDRec),
    /* class_initialize   	*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited       	*/	FALSE,
    /* initialize	  	*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize		  	*/	Realize,
    /* actions		  	*/	NULL,
    /* num_actions	  	*/	0,
    /* resources	  	*/	resources,
    /* num_resources	  	*/	XtNumber(resources),
    /* xrm_class	  	*/	NULLQUARK,
    /* compress_motion	  	*/	TRUE,
    /* compress_exposure  	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest	  	*/	FALSE,
    /* destroy		  	*/	Destroy,
    /* resize		  	*/	Resize,
    /* expose		  	*/	XtInheritExpose,
    /* set_values	  	*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus	 	*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private   	*/	NULL,
    /* tm_table		   	*/	NULL,
    /* query_geometry		*/	QueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
/* Simple class fields initialization */
  {
    /* change_sensitive		*/	XtInheritChangeSensitive
  },
/* LED class fields initialization */
  {
    /* ignore 			*/	0
  }
};
WidgetClass ledWidgetClass = (WidgetClass)&ledClassRec;
/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

static void 
ClassInitialize(void)
{
    XawInitializeWidgetSet();
}

static void 
GetPixmaps(LEDWidget lw)
{
    XGCValues	values;
    GC		gc;
    Display *	dpy;
    Window	root;
    Pixmap	pix,on_pixmap,off_pixmap;
    Dimension	bevel,width,height;

    dpy= XtDisplay((Widget)lw);
    root= RootWindowOfScreen(XtScreen((Widget)lw));
    if (lw->led.on_pixmap!=None) {
	XFreePixmap(dpy,lw->led.on_pixmap);
	lw->led.on_pixmap= None;
    }
    if (lw->led.off_pixmap!=None) {
	XFreePixmap(dpy,lw->led.off_pixmap);
	lw->led.off_pixmap= None;
    }
    lw->led.on_pixmap= on_pixmap= XCreatePixmap(dpy,root,
				 lw->core.width,lw->core.height,lw->core.depth);
    lw->led.off_pixmap= off_pixmap= XCreatePixmap(dpy,root,
				 lw->core.width,lw->core.height,lw->core.depth);

    values.foreground	= lw->led.top_color;
    gc= XCreateGC(dpy,lw->led.on_pixmap,(unsigned)GCForeground,&values);
    bevel= lw->led.bevel;
    width= lw->core.width;
    height= lw->core.height;
    XFillRectangle(dpy,on_pixmap,gc,0,0,width,height);
    XFillRectangle(dpy,off_pixmap,gc,0,0,width,height);
    XSetForeground(dpy,gc,lw->led.bottom_color);
    XFillRectangle(dpy,on_pixmap,gc,bevel,bevel,width-bevel,height-bevel);
    XFillRectangle(dpy,off_pixmap,gc,bevel,bevel,width-bevel,height-bevel);
    XSetForeground(dpy,gc,lw->led.on_color);
    XFillRectangle(dpy,on_pixmap,gc,bevel,bevel,width-2*bevel,height-2*bevel);
    XSetForeground(dpy,gc,lw->led.off_color);
    XFillRectangle(dpy,off_pixmap,gc,bevel,bevel,width-2*bevel,height-2*bevel);
    XFreeGC(dpy,gc);
    if (lw->led.on)	pix= on_pixmap;
    else		pix= off_pixmap;
    if (XtWindow((Widget)lw)!=None)
	XSetWindowBackgroundPixmap(dpy,XtWindow((Widget)lw),pix);
    return;
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    LEDWidget lw = (LEDWidget) new;

    if (lw->core.height == 0)
        lw->core.height = lw->led.led_height;
    if (lw->core.width == 0)
        lw->core.width = lw->led.led_width;
    lw->core.border_width= 0;
    if (lw->led.bevel==0)
	lw->led.bevel= 1;
    lw->led.on_pixmap= lw->led.off_pixmap= None;
    (*XtClass(new)->core_class.resize) ((Widget)lw);
    GetPixmaps(lw);
} /* Initialize */

static void
Realize(Widget w, Mask *mask, XSetWindowAttributes *xswa)
{
    LEDWidget 	lw = (LEDWidget)w;
    WidgetClass super = simpleWidgetClass;
    Pixmap	pix;

    (*super->core_class.realize)(w,mask,xswa);
    if (lw->led.on)	pix= lw->led.on_pixmap;
    else		pix= lw->led.off_pixmap;
    XSetWindowBackgroundPixmap(XtDisplay(w),XtWindow(w),pix);
    return;
}

static void 
Resize(Widget w)
{
    GetPixmaps((LEDWidget)w);
    return;
}

/*
 * Set specified arguments into widget
 */

static Boolean 
SetValues(Widget current, Widget request, Widget new, 
	  ArgList args, Cardinal *num_args)
{
    LEDWidget curlw = (LEDWidget) current;
    LEDWidget newlw = (LEDWidget) new;
    Boolean changed;

    changed= FALSE;
    if (curlw->led.foreground		!= newlw->led.foreground
	|| curlw->core.background_pixel != newlw->core.background_pixel 
	|| curlw->led.on_color		!= newlw->led.on_color
	|| curlw->led.off_color		!= newlw->led.off_color
	|| curlw->core.width		!= curlw->core.width
	|| curlw->core.height		!= curlw->core.height) {
	GetPixmaps(newlw);
	changed= TRUE;
    }
    if (curlw->led.on!=newlw->led.on) {
	Pixmap pix;

	if (newlw->led.on)	pix= newlw->led.on_pixmap;
	else			pix= newlw->led.off_pixmap;

	if (XtWindow(newlw)!=None) 
	    XSetWindowBackgroundPixmap(XtDisplay(newlw),XtWindow(newlw),pix);
	changed= TRUE;
    }
    return changed;
}

static void 
Destroy(Widget w)
{
    LEDWidget lw = (LEDWidget)w;

    if (lw->led.on_pixmap!=None) {
	XFreePixmap(XtDisplay(w),lw->led.on_pixmap);
	lw->led.on_pixmap= None;
    }
    if (lw->led.off_pixmap!=None) {
	XFreePixmap(XtDisplay(w),lw->led.off_pixmap);
	lw->led.off_pixmap= None;
    }
    return;
}


static XtGeometryResult 
QueryGeometry(Widget w, XtWidgetGeometry *intended, 
	      XtWidgetGeometry *preferred)
{
    LEDWidget lw = (LEDWidget)w;

    preferred->request_mode = CWWidth | CWHeight;
    preferred->width = lw->led.led_height;
    preferred->height = lw->led.led_width;
    if (  ((intended->request_mode & (CWWidth | CWHeight))
	   	== (CWWidth | CWHeight)) &&
	  intended->width == preferred->width &&
	  intended->height == preferred->height)
	return XtGeometryYes;
    else if (preferred->width == w->core.width &&
	     preferred->height == w->core.height)
	return XtGeometryNo;
    else
	return XtGeometryAlmost;
}
