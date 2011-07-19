/* $XConsortium: Dvi.c,v 1.21 94/04/17 20:43:34 keith Exp $ */
/* $XdotOrg: $ */
/*

Copyright (c) 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xditview/Dvi.c,v 1.3 2001/08/01 00:45:03 tsi Exp $ */


/*
 * Dvi.c - Dvi display widget
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define XtStrlen(s)	((s) ? strlen(s) : 0)

  /* The following are defined for the reader's convenience.  Any
     Xt..Field macro in this code just refers to some field in
     one of the substructures of the WidgetRec.  */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Converters.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "DviP.h"

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */
/* Note: default_font_map was too long a token for some machines...
 *       therefor it has been split in to and assigned to resources
 *       in the ClassInitialize routine.
 */
static char default_font_map_1[] =  "\
R	-*-times-medium-r-normal--*-*-*-*-*-*-iso8859-1\n\
I	-*-times-medium-i-normal--*-*-*-*-*-*-iso8859-1\n\
B	-*-times-bold-r-normal--*-*-*-*-*-*-iso8859-1\n\
F	-*-times-bold-i-normal--*-*-*-*-*-*-iso8859-1\n\
TR	-*-times-medium-r-normal--*-*-*-*-*-*-iso8859-1\n\
TI	-*-times-medium-i-normal--*-*-*-*-*-*-iso8859-1\n\
TB	-*-times-bold-r-normal--*-*-*-*-*-*-iso8859-1\n\
TF	-*-times-bold-i-normal--*-*-*-*-*-*-iso8859-1\n\
BI	-*-times-bold-i-normal--*-*-*-*-*-*-iso8859-1\n\
C	-*-courier-medium-r-normal--*-*-*-*-*-*-iso8859-1\n\
CO	-*-courier-medium-o-normal--*-*-*-*-*-*-iso8859-1\n\
CB	-*-courier-bold-r-normal--*-*-*-*-*-*-iso8859-1\n\
CF	-*-courier-bold-o-normal--*-*-*-*-*-*-iso8859-1\n\
H	-*-helvetica-medium-r-normal--*-*-*-*-*-*-iso8859-1\n\
HO	-*-helvetica-medium-o-normal--*-*-*-*-*-*-iso8859-1\n\
HB	-*-helvetica-bold-r-normal--*-*-*-*-*-*-iso8859-1\n\
HF	-*-helvetica-bold-o-normal--*-*-*-*-*-*-iso8859-1\n\
";
static char default_font_map_2[] =  "\
N	-*-new century schoolbook-medium-r-normal--*-*-*-*-*-*-iso8859-1\n\
NI	-*-new century schoolbook-medium-i-normal--*-*-*-*-*-*-iso8859-1\n\
NB	-*-new century schoolbook-bold-r-normal--*-*-*-*-*-*-iso8859-1\n\
NF	-*-new century schoolbook-bold-i-normal--*-*-*-*-*-*-iso8859-1\n\
A	-*-charter-medium-r-normal--*-*-*-*-*-*-iso8859-1\n\
AI	-*-charter-medium-i-normal--*-*-*-*-*-*-iso8859-1\n\
AB	-*-charter-bold-r-normal--*-*-*-*-*-*-iso8859-1\n\
AF	-*-charter-bold-i-normal--*-*-*-*-*-*-iso8859-1\n\
S	-*-symbol-medium-r-normal--*-*-*-*-*-*-adobe-fontspecific\n\
S2	-*-symbol-medium-r-normal--*-*-*-*-*-*-adobe-fontspecific\n\
";

#define offset(field) XtOffsetOf(DviRec, field)

static XtResource resources[] = { 
	{XtNfontMap, XtCFontMap, XtRString, sizeof (char *),
	 offset(dvi.font_map_string), XtRString, NULL /* set in code */},
	{XtNforeground, XtCForeground, XtRPixel, sizeof (unsigned long),
	 offset(dvi.foreground), XtRString, XtDefaultForeground},
	{XtNpageNumber, XtCPageNumber, XtRInt, sizeof (int),
	 offset(dvi.requested_page), XtRImmediate, (XtPointer) 1},
	{XtNlastPageNumber, XtCLastPageNumber, XtRInt, sizeof (int),
	 offset (dvi.last_page), XtRImmediate, (XtPointer) 0},
	{XtNfile, XtCFile, XtRFile, sizeof (FILE *),
	 offset (dvi.file), XtRFile, (char *) 0},
	{XtNseek, XtCSeek, XtRBoolean, sizeof (Boolean),
	 offset(dvi.seek), XtRImmediate, (XtPointer) False},
	{XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
	 offset(dvi.default_font), XtRString, XtDefaultFont},
	{XtNbackingStore, XtCBackingStore, XtRBackingStore, sizeof (int),
	 offset(dvi.backing_store), XtRString, "default"},
	{XtNnoPolyText, XtCNoPolyText, XtRBoolean, sizeof (Boolean),
	 offset(dvi.noPolyText), XtRImmediate, (XtPointer) False},
	{XtNscreenResolution, XtCScreenResolution, XtRInt, sizeof (int),
	 offset(dvi.screen_resolution), XtRImmediate, (XtPointer) 75},
	{XtNpageWidth, XtCPageWidth, XtRFloat, sizeof (float),
	 offset(dvi.page_width), XtRString, "8.5"},
	{XtNpageHeight, XtCPageHeight, XtRFloat, sizeof (float),
	 offset(dvi.page_height), XtRString, "11"},
	{XtNsizeScale, XtCSizeScale, XtRInt, sizeof (int),
	 offset(dvi.size_scale_set), XtRImmediate, (XtPointer) 0},
};

#undef offset

static void		ClassInitialize(void);
static void		Initialize(Widget, Widget, ArgList, Cardinal *);
static void		Realize(Widget, XtValueMask *, XSetWindowAttributes *);
static void		Destroy(Widget);
static void		Redisplay(Widget, XEvent *, Region);
static Boolean		SetValues(Widget, Widget, Widget, ArgList , Cardinal *);
static Boolean		SetValuesHook(Widget, ArgList, Cardinal	*);
static XtGeometryResult	QueryGeometry(Widget,
				      XtWidgetGeometry *, XtWidgetGeometry *);
static void		RequestDesiredSize(DviWidget);
static void		ShowDvi(DviWidget);
static void		CloseFile(DviWidget);
static void		OpenFile(DviWidget);

#define SuperClass ((SimpleWidgetClass)&simpleClassRec)

DviClassRec dviClassRec = {
{
	(WidgetClass) SuperClass,	/* superclass		  */	
	"Dvi",				/* class_name		  */
	sizeof(DviRec),			/* size			  */
	ClassInitialize,		/* class_initialize	  */
	NULL,				/* class_part_initialize  */
	FALSE,				/* class_inited		  */
	Initialize,			/* initialize		  */
	NULL,				/* initialize_hook	  */
	Realize,			/* realize		  */
	NULL,				/* actions		  */
	0,				/* num_actions		  */
	resources,			/* resources		  */
	XtNumber(resources),		/* resource_count	  */
	NULLQUARK,			/* xrm_class		  */
	FALSE,				/* compress_motion	  */
	XtExposeCompressMaximal,    	/* compress_exposure	  */
	TRUE,				/* compress_enterleave    */
	FALSE,				/* visible_interest	  */
	Destroy,			/* destroy		  */
	NULL,				/* resize		  */
	Redisplay,			/* expose		  */
	SetValues,			/* set_values		  */
	SetValuesHook,			/* set_values_hook	  */
	XtInheritSetValuesAlmost,	/* set_values_almost	  */
	NULL,				/* get_values_hook	  */
	NULL,				/* accept_focus		  */
	XtVersion,			/* version		  */
	NULL,				/* callback_private	  */
	NULL,				/* tm_table		  */
	QueryGeometry,			/* query_geometry	  */
	XtInheritDisplayAccelerator,	/* display_accelerator	  */
	NULL,				/* extension		  */
},  /* CoreClass fields initialization */
{
    XtInheritChangeSensitive		/* change_sensitive	*/
},  /* SimpleClass fields initialization */
{
    0,                                     /* field not used    */
},  /* DviClass fields initialization */
};

WidgetClass dviWidgetClass = (WidgetClass) &dviClassRec;

static void
ClassInitialize (void)
{
  int len1 = strlen(default_font_map_1);
  int len2 = strlen(default_font_map_2);
  char *dfm = XtMalloc(len1 + len2 + 1);
  char *ptr = dfm;
  strcpy(ptr, default_font_map_1); ptr += len1;
  strcpy(ptr, default_font_map_2); 
  resources[0].default_addr = dfm;

  XtAddConverter( XtRString, XtRBackingStore, XmuCvtStringToBackingStore,
		 NULL, 0 );
}

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    DviWidget	dw = (DviWidget) new;

    dw->dvi.tmpFile = NULL;
    dw->dvi.readingTmp = 0;
    dw->dvi.ungot = 0;
    dw->dvi.normal_GC = NULL;
    dw->dvi.file_map = NULL;
    dw->dvi.fonts = NULL;
    dw->dvi.font_map = NULL;
    dw->dvi.current_page = 0;
    dw->dvi.font_size = 0;
    dw->dvi.font_number = 0;
    dw->dvi.device_resolution = 0;
    dw->dvi.line_width = 0;
    dw->dvi.line_style = 0;
    dw->dvi.font = NULL;
    dw->dvi.display_enable = 0;
    dw->dvi.scale = 0.0;
    dw->dvi.state = NULL;
    dw->dvi.cache.index = 0;
    dw->dvi.cache.font = NULL; 
    dw->dvi.size_scale = 0;
    dw->dvi.size_scale_set = 0;
    RequestDesiredSize (dw);
}

static void
Realize(Widget w, XtValueMask *valueMask, XSetWindowAttributes *attrs)
{
    DviWidget	dw = (DviWidget) w;
    XGCValues	values;

    if (dw->dvi.backing_store != Always + WhenMapped + NotUseful) {
	attrs->backing_store = dw->dvi.backing_store;
	*valueMask |= CWBackingStore;
    }
    XtCreateWindow (w, (unsigned)InputOutput, (Visual *) CopyFromParent,
		    *valueMask, attrs);
    values.foreground = dw->dvi.foreground;
    dw->dvi.normal_GC = XCreateGC (XtDisplay (w), XtWindow (w),
				    GCForeground, &values);
#ifdef USE_XFT
    {
	int		scr;
	Visual		*visual;
	Colormap	cmap;
	XRenderColor	black;

	scr = XScreenNumberOfScreen (dw->core.screen);
	visual = DefaultVisual (XtDisplay (w), scr);
	cmap = DefaultColormap (XtDisplay (w), scr);
	dw->dvi.draw = XftDrawCreate (XtDisplay (w), XtWindow (w), 
				      visual, cmap);

	black.red = black.green = black.blue = 0;
	black.alpha = 0xffff;
	XftColorAllocValue (XtDisplay (w), visual, cmap,
			    &black, &dw->dvi.black);
	dw->dvi.default_font = XftFontOpenName (XtDisplay (w),
						scr,
						"serif-12");
    }
#endif
    if (dw->dvi.file)
	OpenFile (dw);
    ParseFontMap (dw);
}

static void
Destroy(Widget w)
{
	DviWidget	dw = (DviWidget) w;

	XFreeGC (XtDisplay (w), dw->dvi.normal_GC);
	DestroyFontMap (dw->dvi.font_map);
	DestroyFileMap (dw->dvi.file_map);
}

/*
 * Repaint the widget window
 */

/* ARGSUSED */
static void
Redisplay(Widget w, XEvent *event, Region region)
{
	DviWidget	dw = (DviWidget) w;
#ifndef USE_XFT
	XRectangle	extents;
#endif
	
#ifdef USE_XFT
	XClearArea (XtDisplay (dw),
		    XtWindow (dw),
		    0, 0, 0, 0, False);
	dw->dvi.extents.x1 = 0;
	dw->dvi.extents.y1 = 0;
	dw->dvi.extents.x2 = dw->core.width;
	dw->dvi.extents.y2 = dw->core.height;
#else
	XClipBox (region, &extents);
	dw->dvi.extents.x1 = extents.x;
	dw->dvi.extents.y1 = extents.y;
	dw->dvi.extents.x2 = extents.x + extents.width;
	dw->dvi.extents.y2 = extents.y + extents.height;
#endif
	ShowDvi (dw);
}

static void
RequestDesiredSize (DviWidget dw)
{
    XtWidgetGeometry	req, rep;

    dw->dvi.desired_width = dw->dvi.page_width *
				 dw->dvi.screen_resolution;
    dw->dvi.desired_height = dw->dvi.page_height *
				  dw->dvi.screen_resolution;
    req.request_mode = CWWidth|CWHeight;
    req.width = dw->dvi.desired_width;
    req.height = dw->dvi.desired_height;
    XtMakeGeometryRequest ((Widget) dw, &req, &rep);
}

/*
 * Set specified arguments into widget
 */
/* ARGSUSED */
static Boolean
SetValues (Widget wcurrent, Widget wrequest, Widget wnew,
	   ArgList args, Cardinal *num_args)
{
    DviWidget	current = (DviWidget) wcurrent;
    DviWidget	request = (DviWidget) wrequest;
    DviWidget	new     = (DviWidget) wnew;
    Boolean	redisplay = FALSE;
    char	*new_map;
    int		cur, req;

    req = request->dvi.requested_page;
    cur = current->dvi.requested_page;
    if (cur != req) {
	    if (req < 1)
		req = 1;
	    if (request->dvi.file)
	    {
		if (current->dvi.last_page != 0 &&
		    req > current->dvi.last_page)
			req = current->dvi.last_page;
	    }
	    if (cur != req)
		redisplay = TRUE;
	    new->dvi.requested_page = req;
    }
    
    if (current->dvi.font_map_string != request->dvi.font_map_string) {
	    new_map = XtMalloc (strlen (request->dvi.font_map_string) + 1);
	    if (new_map) {
		    redisplay = TRUE;
		    strcpy (new_map, request->dvi.font_map_string);
		    new->dvi.font_map_string = new_map;
		    if (current->dvi.font_map_string)
			    XtFree (current->dvi.font_map_string);
		    current->dvi.font_map_string = NULL;
		    ParseFontMap (new);
	    }
    }
    if (current->dvi.screen_resolution != request->dvi.screen_resolution)
    {
	ResetFonts (new);
	new->dvi.line_width = -1;
    }
    if (request->dvi.device_resolution)
	new->dvi.scale = ((double) request->dvi.screen_resolution) /
			     ((double) request->dvi.device_resolution);
    if (current->dvi.page_width !=  request->dvi.page_width ||
	current->dvi.page_height != request->dvi.page_height ||
	current->dvi.screen_resolution != request->dvi.screen_resolution)
    {
	RequestDesiredSize (new);
	redisplay = TRUE;
    }
    return redisplay;
}

/*
 * use the set_values_hook entry to check when
 * the file is set
 */

static Boolean
SetValuesHook (Widget widget, ArgList args, Cardinal *num_argsp)
{
	DviWidget	dw = (DviWidget) widget;
	Cardinal	i;

	for (i = 0; i < *num_argsp; i++) {
		if (!strcmp (args[i].name, XtNfile)) {
			CloseFile (dw);
			OpenFile (dw);
			return TRUE;
		}
	}
	return FALSE;
}

static void
CloseFile (DviWidget dw)
{
    if (dw->dvi.tmpFile)
	fclose (dw->dvi.tmpFile);
    ForgetPagePositions (dw);
}

static void
OpenFile (DviWidget dw)
{
    char	tmpName[sizeof ("/tmp/dviXXXXXX")];
#ifdef HAS_MKSTEMP
    int fd;
#endif

    dw->dvi.tmpFile = NULL;
    if (!dw->dvi.seek) {
	strcpy (tmpName, "/tmp/dviXXXXXX");
#ifndef HAS_MKSTEMP
	mktemp (tmpName);
	dw->dvi.tmpFile = fopen (tmpName, "w+");
#else
	fd = mkstemp(tmpName);
	dw->dvi.tmpFile = fdopen(fd, "w+");
#endif
	unlink (tmpName);
    }
    if (dw->dvi.requested_page < 1)
	dw->dvi.requested_page = 1;
    dw->dvi.last_page = 0;
}

static XtGeometryResult
QueryGeometry (Widget w, XtWidgetGeometry *request,
	       XtWidgetGeometry *geometry_return)
{
	XtGeometryResult	ret;
	DviWidget		dw = (DviWidget) w;

	ret = XtGeometryYes;
	if ((int)request->width < dw->dvi.desired_width
	    || (int)request->height < dw->dvi.desired_height)
		ret = XtGeometryAlmost;
	geometry_return->width = dw->dvi.desired_width;
	geometry_return->height = dw->dvi.desired_height;
	geometry_return->request_mode = CWWidth|CWHeight;
	return ret;
}

void
SetDeviceResolution (DviWidget dw, int resolution)
{
    if (resolution != dw->dvi.device_resolution) {
	dw->dvi.device_resolution = resolution;
	dw->dvi.scale = ((double)  dw->dvi.screen_resolution) /
			((double) resolution);
    }
}

static void
ShowDvi (DviWidget dw)
{
	int	i;
	long	file_position;

	if (!dw->dvi.file) 
	  return;

	if (dw->dvi.requested_page < 1)
		dw->dvi.requested_page = 1;

	if (dw->dvi.last_page != 0 && dw->dvi.requested_page > dw->dvi.last_page)
		dw->dvi.requested_page = dw->dvi.last_page;

	file_position = SearchPagePosition (dw, dw->dvi.requested_page);
	if (file_position != -1) {
		FileSeek(dw, file_position);
		dw->dvi.current_page = dw->dvi.requested_page;
	} else {
		for (i=dw->dvi.requested_page; i > 0; i--) {
			file_position = SearchPagePosition (dw, i);
			if (file_position != -1)
				break;
		}
		if (file_position == -1)
			file_position = 0;
		FileSeek (dw, file_position);

		dw->dvi.current_page = i;
		
		dw->dvi.display_enable = 0;
		while (dw->dvi.current_page != dw->dvi.requested_page) {
			dw->dvi.current_page = ParseInput (dw);
			/*
			 * at EOF, seek back to the begining of this page.
			 */
			if (feof (dw->dvi.file)) {
				file_position = SearchPagePosition (dw,
						dw->dvi.current_page);
				if (file_position != -1)
					FileSeek (dw, file_position);
				break;
			}
		}
	}
	
	dw->dvi.display_enable = 1;
	ParseInput (dw);
	if (dw->dvi.last_page && dw->dvi.requested_page > dw->dvi.last_page)
		dw->dvi.requested_page = dw->dvi.last_page;
}
