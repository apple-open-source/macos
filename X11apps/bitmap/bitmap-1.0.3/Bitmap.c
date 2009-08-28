/* $Xorg: Bitmap.c,v 1.4 2001/02/09 02:05:28 xorgcvs Exp $ */
/*

Copyright 1989, 1998  The Open Group

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
/* $XFree86: Bitmap.c,v 1.5 2001/12/26 21:39:34 paulo Exp $ */

/*
 * Author:  Davor Matic, MIT X Consortium
 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/XawInit.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xmu/Drawing.h>
#include <X11/Xmu/SysUtil.h>
#include <X11/Xatom.h>
#include <X11/Xfuncs.h>
#include <X11/Xos.h>
#include "BitmapP.h"
#include "Bitmap.h"
    
#include <stdio.h>
#include <math.h>

#define min(x, y)                     ((((int)(x)) < (int)(y)) ? (x) : (y))
#define max(x, y)                     ((((int)(x)) > (int)(y)) ? (x) : (y))

Boolean DEBUG;

#define DefaultGridTolerance 8
#define DefaultBitmapSize    "16x16"
#define FallbackBitmapWidth  16
#define FallbackBitmapHeight 16
#define DefaultGrid          TRUE
#define DefaultDashed        TRUE
#define DefaultStippled      TRUE
#define DefaultProportional  TRUE
#define DefaultAxes          FALSE
#define DefaultMargin        16
#define DefaultSquareWidth   16
#define DefaultSquareHeight  16
#define DefaultFilename      ""

#define Offset(field) XtOffsetOf(BitmapRec, bitmap.field)

static XtResource resources[] = {
{XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
     Offset(foreground_pixel), XtRString, XtDefaultForeground},
{XtNhighlight, XtCHighlight, XtRPixel, sizeof(Pixel),
     Offset(highlight_pixel), XtRString, XtDefaultForeground},
{XtNframe, XtCFrame, XtRPixel, sizeof(Pixel),
     Offset(frame_pixel), XtRString, XtDefaultForeground},
{XtNgridTolerance, XtCGridTolerance, XtRDimension, sizeof(Dimension),
     Offset(grid_tolerance), XtRImmediate, (XtPointer) DefaultGridTolerance},
{XtNsize, XtCSize, XtRString, sizeof(String),
     Offset(size), XtRImmediate, (XtPointer) DefaultBitmapSize},
{XtNdashed, XtCDashed, XtRBoolean, sizeof(Boolean),
     Offset(dashed), XtRImmediate, (XtPointer) DefaultDashed},
{XtNgrid, XtCGrid, XtRBoolean, sizeof(Boolean),
     Offset(grid), XtRImmediate, (XtPointer) DefaultGrid},
{XtNstippled, XtCStippled, XtRBoolean, sizeof(Boolean),
     Offset(stippled), XtRImmediate, (XtPointer) DefaultStippled},
{XtNproportional, XtCProportional, XtRBoolean, sizeof(Boolean),
     Offset(proportional), XtRImmediate, (XtPointer) DefaultProportional},
{XtNaxes, XtCAxes, XtRBoolean, sizeof(Boolean),
     Offset(axes), XtRImmediate, (XtPointer) DefaultAxes},
{XtNsquareWidth, XtCSquareWidth, XtRDimension, sizeof(Dimension),
     Offset(squareW), XtRImmediate, (XtPointer) DefaultSquareWidth},
{XtNsquareHeight, XtCSquareHeight, XtRDimension, sizeof(Dimension),
     Offset(squareH), XtRImmediate, (XtPointer) DefaultSquareHeight},
{XtNmargin, XtCMargin, XtRDimension, sizeof(Dimension),
     Offset(margin), XtRImmediate, (XtPointer) DefaultMargin},
{XtNxHot, XtCXHot, XtRPosition, sizeof(Position),
     Offset(hot.x), XtRImmediate, (XtPointer) NotSet},
{XtNyHot, XtCYHot, XtRPosition, sizeof(Position),
     Offset(hot.y), XtRImmediate, (XtPointer) NotSet},
{XtNbutton1Function, XtCButton1Function, XtRButtonFunction, sizeof(int),
     Offset(button_function[0]), XtRImmediate, (XtPointer) Set},
{XtNbutton2Function, XtCButton2Function, XtRButtonFunction, sizeof(int),
     Offset(button_function[1]), XtRImmediate, (XtPointer) Invert},
{XtNbutton3Function, XtCButton3Function, XtRButtonFunction, sizeof(int),
     Offset(button_function[2]), XtRImmediate, (XtPointer) Clear},
{XtNbutton4Function, XtCButton4Function, XtRButtonFunction, sizeof(int),
     Offset(button_function[3]), XtRImmediate, (XtPointer) Clear},
{XtNbutton5Function, XtCButton5Function, XtRButtonFunction, sizeof(int),
     Offset(button_function[4]), XtRImmediate, (XtPointer) Clear},
{XtNfilename, XtCFilename, XtRString, sizeof(String),
     Offset(filename), XtRImmediate, (XtPointer) DefaultFilename},
{XtNbasename, XtCBasename, XtRString, sizeof(String),
     Offset(basename), XtRImmediate, (XtPointer) DefaultFilename},
{XtNdashes, XtCDashes, XtRBitmap, sizeof(Pixmap),
     Offset(dashes), XtRImmediate, (XtPointer) XtUnspecifiedPixmap},
{XtNstipple, XtCStipple, XtRBitmap, sizeof(Pixmap),
     Offset(stipple), XtRImmediate, (XtPointer) XtUnspecifiedPixmap},
};
#undef Offset


static XtActionsRec actions[] =
{
{"mark",               (XtActionProc)BWTMark},
{"mark-all",           (XtActionProc)BWTMarkAll},
{"unmark",             (XtActionProc)BWTUnmark},
{"paste",              (XtActionProc)BWTPaste},
{"bw-debug",           (XtActionProc)BWDebug},
{"abort",              (XtActionProc)BWAbort},
{"store-to-buffer",    (XtActionProc)BWStoreToBuffer},
{"change-notify",      (XtActionProc)BWChangeNotify},
{"set-changed",        (XtActionProc)BWSetChanged},
{"up",                 (XtActionProc)BWUp},
{"down",               (XtActionProc)BWDown},
{"left",               (XtActionProc)BWLeft},
{"right",              (XtActionProc)BWRight},
{"fold",               (XtActionProc)BWFold},
{"flip-horiz",         (XtActionProc)BWFlipHoriz},
{"flip-vert",          (XtActionProc)BWFlipVert},
{"rotate-right",       (XtActionProc)BWRotateRight},
{"rotate-left",        (XtActionProc)BWRotateLeft},
{"set",                (XtActionProc)BWSet},
{"clear",              (XtActionProc)BWClear},
{"invert",             (XtActionProc)BWInvert},
{"undo",               (XtActionProc)BWUndo},
{"redraw",             (XtActionProc)BWRedraw},
};

static char translations1[] =
"\
Shift<Btn1Down>: mark()\n\
Shift<Btn2Down>: mark-all()\n\
Shift<Btn3Down>: unmark()\n\
Ctrl<BtnDown>:   paste()\n\
Ctrl<Key>l: redraw()\n\
<Key>d:     bw-debug()\n\
<Key>a:     abort()\n\
<Key>Up:    store-to-buffer()\
            up()\
            change-notify()\
            set-changed()\n\
<Key>KP_Up: store-to-buffer()\
            up()\
            change-notify()\
            set-changed()\n\
<Key>Down:  store-to-buffer()\
            down()\
            change-notify()\
            set-changed()\n\
<Key>KP_Down: store-to-buffer()\
            down()\
            change-notify()\
            set-changed()\n\
<Key>Left:  store-to-buffer()\
            left()\
            change-notify()\
            set-changed()\n\
<Key>KP_Left: store-to-buffer()\
            left()\
            change-notify()\
            set-changed()\n\
<Key>Right: store-to-buffer()\
            right()\
            change-notify()\
            set-changed()\n\
<Key>KP_Right: store-to-buffer()\
            right()\
            change-notify()\
            set-changed()\n\
<Key>f:     store-to-buffer()\
            fold()\
            change-notify()\
            set-changed()\n\
<Key>h:     store-to-buffer()\
            flip-horiz()\
            change-notify()\
            set-changed()\n\
";

static char translations2[] =
"<Key>v:     store-to-buffer()\
            flip-vert()\
            change-notify()\
            set-changed()\n\
<Key>r:     store-to-buffer()\
            rotate-right()\
            change-notify()\
            set-changed()\n\
<Key>l:     store-to-buffer()\
            rotate-left()\
            change-notify()\
            set-changed()\n\
<Key>s:     store-to-buffer()\
            set()\
            change-notify()\
            set-changed()\n\
<Key>c:     store-to-buffer()\
            clear()\
            change-notify()\
            set-changed()\n\
<Key>i:     store-to-buffer()\
            invert()\
            change-notify()\
            set-changed()\n\
<Key>u:     undo()\
            change-notify()\
            set-changed()\n\
";

Atom targets[] = {
    XA_BITMAP,
    XA_PIXMAP
};

#include "Requests.h"


static BWRequestRec requests[] = 
{
{MarkRequest, sizeof(BWStatus),
     TwoPointsEngage, (XtPointer) BWDrawRectangle,
     TwoPointsTerminateTimed, (XtPointer) BWSelect,
     NULL, (XtPointer) NULL},
{RestoreRequest, sizeof(BWStatus),
     OnePointEngage, (XtPointer) BWDragStored,
     OnePointTerminate, (XtPointer) BWRestore,
     NULL, (XtPointer) NULL},
{ImmediateCopyRequest, sizeof(BWStatus),
     OnePointEngage, (XtPointer) BWDragMarked,
     OnePointTerminate, (XtPointer) BWCopy,
     NULL, (XtPointer) NULL},
{ImmediateMoveRequest, sizeof(BWStatus),
     OnePointEngage, (XtPointer) BWDragMarked,
     OnePointTerminate, (XtPointer) BWMove,
     NULL, (XtPointer) NULL},
{CopyRequest, sizeof(BWStatus),
     DragOnePointEngage, (XtPointer) Paste,
     DragOnePointTerminate, (XtPointer) ImmediateCopyRequest,
     Interface, (XtPointer) BWUnmark},
{MoveRequest, sizeof(BWStatus),
     DragOnePointEngage, (XtPointer) Paste,
     DragOnePointTerminate, (XtPointer) ImmediateMoveRequest,
     Interface, (XtPointer) BWUnmark},
{PointRequest, sizeof(BWStatus),
     DragOnePointEngage, (XtPointer) BWDrawPoint,
     DragOnePointTerminate, (XtPointer) BWDrawPoint,
     NULL, (XtPointer) NULL},
{CurveRequest, sizeof(BWStatus),
     DragTwoPointsEngage, (XtPointer) BWBlindLine,
     DragTwoPointsTerminate, (XtPointer) BWBlindLine,
     NULL, (XtPointer) NULL},
{LineRequest, sizeof(BWStatus), 
     TwoPointsEngage, (XtPointer) BWDrawLine, 
     TwoPointsTerminate, (XtPointer) BWDrawLine,
     NULL, (XtPointer) NULL},
{RectangleRequest, sizeof(BWStatus), 
     TwoPointsEngage, (XtPointer) BWDrawRectangle,
     TwoPointsTerminate, (XtPointer) BWDrawRectangle,
     NULL, (XtPointer) NULL},
{FilledRectangleRequest, sizeof(BWStatus), 
     TwoPointsEngage, (XtPointer) BWDrawRectangle,
     TwoPointsTerminate, (XtPointer) BWDrawFilledRectangle,
     NULL, (XtPointer) NULL},
{CircleRequest, sizeof(BWStatus), 
     TwoPointsEngage, (XtPointer) BWDrawCircle,
     TwoPointsTerminate, (XtPointer) BWDrawCircle,
     NULL, (XtPointer) NULL},
{FilledCircleRequest, sizeof(BWStatus), 
     TwoPointsEngage, (XtPointer) BWDrawCircle, 
     TwoPointsTerminate, (XtPointer) BWDrawFilledCircle,
     NULL, (XtPointer) NULL},
{FloodFillRequest, sizeof(BWStatus),
     OnePointEngage, (XtPointer) NULL,
     OnePointTerminate, (XtPointer) BWFloodFill,
     NULL, (XtPointer) NULL},
{HotSpotRequest, sizeof(BWStatus),
     OnePointEngage, (XtPointer) BWDrawHotSpot,
     OnePointTerminate, (XtPointer) BWDrawHotSpot,
     NULL, (XtPointer) NULL},
{ZoomInRequest, sizeof(BWStatus),
     TwoPointsEngage, (XtPointer) BWDrawRectangle,
     TwoPointsTerminate, (XtPointer) BWZoomIn,
     NULL, (XtPointer) NULL},
};

static void ClassInitialize(void);
static void Initialize(Widget wrequest, Widget wnew, 
		       ArgList argv, Cardinal *argc);
static void Redisplay(Widget w, XEvent *event, Region region);
static void Resize(Widget w);
static void Destroy(Widget w);
static void Refresh(BitmapWidget BW, Position x, Position y, 
		    Dimension width, Dimension height);
static Boolean SetValues(Widget old, Widget request, Widget new,
			 ArgList args, Cardinal *num_args);
 
BitmapClassRec bitmapClassRec = {
{   /* core fields */
    /* superclass		*/	(WidgetClass) &simpleClassRec,
    /* class_name		*/	"Bitmap",
    /* widget_size		*/	sizeof(BitmapRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	actions,
    /* num_actions		*/	XtNumber(actions),
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	FALSE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	TRUE,
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
    /* tm_table			*/	NULL , /* set in code */
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL,
  },
  { 
    /* empty			*/	XtInheritChangeSensitive,
  },
  {
    /* targets                  */      targets,
    /* num_trets                */      XtNumber(targets),
    /* requests                 */      requests,
    /* num_requests             */      XtNumber(requests),
  }
};
 
WidgetClass bitmapWidgetClass = (WidgetClass) &bitmapClassRec;
    
/* ARGSUSED */

void 
BWDebug(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    DEBUG ^= True;
}

Pixmap 
BWGetPixmap(Widget w) 
{
    BitmapWidget BW = (BitmapWidget) w;
 
    return GetPixmap(BW, BW->bitmap.zoom.image);
}

Pixmap 
BWGetUnzoomedPixmap(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    GC gc;
    Pixmap pix;
    
    if (BW->bitmap.zooming) {    
	pix = XCreatePixmap(XtDisplay(w), XtWindow(w), 
			    BW->bitmap.zoom.image->width, 
			    BW->bitmap.zoom.image->height, 1);
	if (!(gc = XCreateGC(XtDisplay(w), pix, 
			     (unsigned long) 0, (XGCValues *) 0)))
	    return (Pixmap) None;
	
	XPutImage(XtDisplay(w), pix, gc, 
		  BW->bitmap.zoom.image, 
		  0, 0, 0, 0, 
		  BW->bitmap.zoom.image->width, 
		  BW->bitmap.zoom.image->height);
	XPutImage(XtDisplay(w), pix, gc, 
		  BW->bitmap.image, 
		  0, 0, 
		  BW->bitmap.zoom.at_x,
		  BW->bitmap.zoom.at_y,
		  BW->bitmap.image->width, 
		  BW->bitmap.image->height);
    }
    else {
	pix = XCreatePixmap(XtDisplay(w), XtWindow(w), 
			    BW->bitmap.image->width, 
			    BW->bitmap.image->height, 1);
	if (! (gc = XCreateGC(XtDisplay(w), pix, 
			      (unsigned long) 0, (XGCValues *) 0)))
	    return (Pixmap) None;
	
	XPutImage(XtDisplay(w), pix, gc, 
		  BW->bitmap.image, 
		  0, 0, 0, 0,
		  BW->bitmap.image->width, 
		  BW->bitmap.image->height);
    }
    XFreeGC(XtDisplay(w), gc);
    return(pix);
}


XImage *
GetImage(BitmapWidget BW, Pixmap pixmap)
{
    Window root;
    int x, y;
    unsigned int width, height, border_width, depth;
    XImage *source, *image;

    XGetGeometry(XtDisplay(BW), pixmap, &root, &x, &y,
		 &width, &height, &border_width, &depth);

    source = XGetImage(XtDisplay(BW), pixmap, x, y, width, height,
		     1, XYPixmap);

    image = ConvertToBitmapImage(BW, source);

    return image;
}

XImage *
CreateBitmapImage(BitmapWidget BW, char *data, 
		  Dimension width, Dimension height)
{
    XImage *image = XCreateImage(XtDisplay(BW),
				 DefaultVisual(XtDisplay(BW), 
					       DefaultScreen(XtDisplay(BW))),
				 1, XYBitmap, 0, 
				 data, width, height,
				 8, ((int)width + 7) / 8);

    image->height = height;
    image->width = width;
    image->depth = 1;
    image->xoffset = 0;
    image->format = XYBitmap;
    image->data = (char *)data;
    image->byte_order = LSBFirst;
    image->bitmap_unit = 8;
    image->bitmap_bit_order = LSBFirst;
    image->bitmap_pad = 8;
    image->bytes_per_line = ((int)width + 7) / 8;

    return image;
}

void 
DestroyBitmapImage(XImage **image)
{
    /*XDestroyImage(*image);*/
    if (image) {
	if (*image) {
	    if ((*image)->data)
		XtFree((*image)->data);
	    XtFree((char *)*image);
	}
	*image = NULL;
    }
}

#if 0
XImage *
BWGetImage(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    BitmapWidget BW = (BitmapWidget) w;

    return BW->bitmap.image;
}
#endif

void 
BWChangeNotify(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    if (BW->bitmap.notify)
	(*BW->bitmap.notify)(w, NULL, NULL, NULL);
}

void 
BWNotify(Widget w, XtActionProc proc)
{
    BitmapWidget BW = (BitmapWidget) w;

    BW->bitmap.notify = proc;
}

void 
BWSetChanged(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
	
    BW->bitmap.changed = True;
}

Boolean 
BWQueryChanged(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
	
    return BW->bitmap.changed;
}

void 
BWClearChanged(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    BW->bitmap.changed = False;
}

Boolean 
BWQueryStored(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    return (BW->bitmap.storage != NULL);
}

Boolean 
BWQueryStippled(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return BW->bitmap.stippled;
}

static void 
RedrawStippled(BitmapWidget BW)
{
  XExposeEvent event;
  
  event.type = Expose;
  event.display = XtDisplay((Widget)BW);
  event.window = XtWindow((Widget)BW);
  event.x = 0;
  event.y = 0;
  event.width = BW->core.width;
  event.height = BW->core.height;
  event.count = 0;
  
  BWRedrawMark((Widget)BW);
  
  BW->bitmap.stipple_change_expose_event = True; 
  
  XtDispatchEvent((XEvent *)&event);
  
  BW->bitmap.stipple_change_expose_event = False;
}

void 
BWSwitchStippled(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    RedrawStippled(BW);

    BW->bitmap.stippled ^= True;
    XSetFillStyle(XtDisplay(BW), BW->bitmap.highlighting_gc,
		  (BW->bitmap.stippled ? FillStippled : FillSolid));

    RedrawStippled(BW);    
}

void 
BWSelect(Widget w, Position from_x, Position from_y, 
	 Position to_x, Position to_y, Time btime)
{
    BWMark(w, from_x, from_y, to_x, to_y);

    BWGrabSelection(w, btime);
}

Boolean 
BWQueryAxes(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return BW->bitmap.axes;
}

void 
BWSwitchAxes(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    BW->bitmap.axes ^= True;
    BWHighlightAxes(w);
}

void 
BWAxes(Widget w, Boolean _switch)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    if (BW->bitmap.axes != _switch)
	BWSwitchAxes(w);
}

void 
BWRedrawAxes(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    if (BW->bitmap.axes)
	BWHighlightAxes(w);
}

#if 0
void 
BWPutImage(BitmapWidget w, Display *display, Drawable drawable, GC gc, 
	   Position x, Position y)
{
    BitmapWidget BW = (BitmapWidget) w;

  XPutImage(display, drawable, gc, BW->bitmap.image,
	    0, 0, x, y, BW->bitmap.image->width, BW->bitmap.image->height);
}
#endif

static String 
StripFilename(String filename)
{
    char *begin = strrchr(filename, '/');
    char *end, *result;
    int length;
    
    if (filename) {
	begin = (begin ? begin + 1 : filename);
	end = strchr(begin, '.'); /* change to strrchr to allow longer names */
	length = (end ? (end - begin) : strlen (begin));
	result = (char *) XtMalloc (length + 1);
	strncpy (result, begin, length);
	result [length] = '\0';
	return (result);
    }
    else
	return (NULL);
}

static int 
XmuWriteBitmapDataToFile(String filename, String basename, 
			 int width, int height, char *datap, 
			 int x_hot, int y_hot)
{
    FILE *file;
    int i, data_length;
    
    data_length = Length(width, height);
    
    if(!filename || !strcmp(filename, "") || !strcmp(filename, "-")) {   
	file = stdout;
	filename = "dummy";
	}
    else
    	file = fopen(filename, "w+");
    
    if (!basename || !strcmp(basename, "") || !strcmp(basename, "-"))
	basename = StripFilename(filename);
    
    if (file) {
	fprintf(file, "#define %s_width %d\n", basename, width);
	fprintf(file, "#define %s_height %d\n", basename, height);
	if (QuerySet(x_hot, y_hot)) {
	    fprintf(file, "#define %s_x_hot %d\n", basename, x_hot);
	    fprintf(file, "#define %s_y_hot %d\n", basename, y_hot);
	}
	fprintf(file, "static unsigned char %s_bits[] = {\n   0x%02x",
		basename, (unsigned char) datap[0]);
	for(i = 1; i < data_length; i++) {
	    fprintf(file, ",");
	    fprintf(file, (i % 12) ? " " : "\n   ");
	    fprintf(file, "0x%02x", (unsigned char) datap[i]);
	}
	fprintf(file, "};\n");
	
	if (file != stdout)
	    fclose(file);

	return BitmapSuccess;
    }
    
    return 1;
}

/*
 *
 */

				/* ARGSUSED */
static void 
CvtStringToButtonFunction(XrmValuePtr args, /* not used */
			  Cardinal *num_args, /* not used */
			  XrmValuePtr from_val, 
			  XrmValuePtr to_val)
{
  static int button_function;
  char lower_name[80];
 
  XmuCopyISOLatin1Lowered (lower_name, (char*)from_val->addr);
  
  if (!strcmp(lower_name, XtClear)) {
    button_function = Clear;
    to_val->addr = (XPointer) &button_function;
    to_val->size = sizeof(button_function);
    return;
  }
  
  if (!strcmp(lower_name, XtSet)) {
    button_function = Set;
    to_val->addr = (XPointer) &button_function;
    to_val->size = sizeof(button_function);
    return;
  }

  if (!strcmp(lower_name, XtInvert)) {
    button_function = Invert;
    to_val->addr = (XPointer) &button_function;
    to_val->size = sizeof(button_function);
    return;
  }
  
  XtStringConversionWarning(from_val->addr, XtRButtonFunction);
  button_function = Clear;
  to_val->addr = (XPointer) &button_function;
  to_val->size = sizeof(button_function);
  
}

static void 
ClassInitialize(void)
{
  char *tm_table = XtMalloc(strlen(translations1) + strlen(translations2) + 1);
  strcpy(tm_table, translations1);
  strcat(tm_table, translations2);
  bitmapClassRec.core_class.tm_table = tm_table;

  XawInitializeWidgetSet();
  XtAddConverter(XtRString, XtRButtonFunction, CvtStringToButtonFunction,
		 NULL, 0);
  DEBUG = False;
}

static void 
SetSizeFromSizeResource(BitmapWidget bw)
{
  if (BWParseSize(bw->bitmap.size, 
		  &bw->bitmap.width,
		  &bw->bitmap.height)
      ==
      False) {
    bw->bitmap.width = FallbackBitmapWidth;
    bw->bitmap.height = FallbackBitmapHeight;
    XtWarning("Cannot parse the size resource.  BitmapWidget");
  }
}


/* ARGSUSED */
static void 
Initialize(Widget wrequest, Widget wnew, ArgList argv, Cardinal *argc)
{
    BitmapWidget new = (BitmapWidget) wnew;

    XGCValues  values;
    XtGCMask   mask;
    char *image_data, *buffer_data;

    new->bitmap.stipple_change_expose_event = False;
    new->bitmap.notify = NULL;
    new->bitmap.cardinal = 0;
    new->bitmap.current = 0;
    new->bitmap.fold = False;
    new->bitmap.changed = False;
    new->bitmap.zooming = False;
    new->bitmap.selection.own = False;
    new->bitmap.selection.limbo = False;

    new->bitmap.request_stack = (BWRequestStack *)
	XtMalloc(sizeof(BWRequestStack));

    new->bitmap.request_stack[0].request = NULL;
    new->bitmap.request_stack[0].call_data = NULL;
    new->bitmap.request_stack[0].trap = False;

    SetSizeFromSizeResource(new);

    new->core.width = new->bitmap.width * new->bitmap.squareW + 
	2 * new->bitmap.margin;
    new->core.height = new->bitmap.height * new->bitmap.squareH + 
	2 * new->bitmap.margin;
  
    new->bitmap.hot.x = new->bitmap.hot.y = NotSet;
    new->bitmap.buffer_hot.x = new->bitmap.buffer_hot.y = NotSet;
  
    new->bitmap.mark.from_x = new->bitmap.mark.from_y = NotSet;
    new->bitmap.mark.to_x = new->bitmap.mark.to_y = NotSet;
    new->bitmap.buffer_mark.from_x = new->bitmap.buffer_mark.from_y = NotSet;
    new->bitmap.buffer_mark.to_x = new->bitmap.buffer_mark.to_y = NotSet;

    values.foreground = new->bitmap.foreground_pixel;
    values.background = new->core.background_pixel;
    values.foreground ^= values.background;
    values.function = GXxor;
    mask = GCForeground | GCBackground | GCFunction;
    new->bitmap.drawing_gc = XCreateGC(XtDisplay(new), 
				       RootWindow(XtDisplay(new), 
				       DefaultScreen(XtDisplay(new))),
				       mask, &values);

    values.foreground = new->bitmap.highlight_pixel;
    values.background = new->core.background_pixel;
    values.foreground ^= values.background;
    values.function = GXxor;
    mask = GCForeground | GCBackground | GCFunction;
    if (new->bitmap.stipple != XtUnspecifiedPixmap)
    {
	values.stipple = new->bitmap.stipple;
	mask |= GCStipple | GCFillStyle;
    }
    values.fill_style = (new->bitmap.stippled ? FillStippled : FillSolid);

    new->bitmap.highlighting_gc = XCreateGC(XtDisplay(new), 
					    RootWindow(XtDisplay(new), 
					       DefaultScreen(XtDisplay(new))), 
					    mask, &values);


    values.foreground = new->bitmap.frame_pixel;
    values.background = new->core.background_pixel;
    values.foreground ^= values.background;
    mask = GCForeground | GCBackground | GCFunction;
    if (new->bitmap.dashes != XtUnspecifiedPixmap)
    {
	values.stipple = new->bitmap.dashes;
	mask |= GCStipple | GCFillStyle;
    }
    values.fill_style = (new->bitmap.dashed ? FillStippled : FillSolid);

    new->bitmap.frame_gc = XCreateGC(XtDisplay(new), 
				     RootWindow(XtDisplay(new), 
						DefaultScreen(XtDisplay(new))),
				     mask, &values);

    values.foreground = new->bitmap.highlight_pixel;
    values.background = new->core.background_pixel;
    values.foreground ^= values.background;
    mask = GCForeground | GCBackground | GCFunction;
    new->bitmap.axes_gc = XCreateGC(XtDisplay(new), 
				     RootWindow(XtDisplay(new), 
						DefaultScreen(XtDisplay(new))),
				     mask, &values);

    image_data = CreateCleanData(Length(new->bitmap.width, 
					new->bitmap.height));
    buffer_data = CreateCleanData(Length(new->bitmap.width, 
					 new->bitmap.height));

    new->bitmap.storage = NULL;
    
    new->bitmap.image = CreateBitmapImage(new, 
					  image_data,
					  new->bitmap.width,
					  new->bitmap.height);
    new->bitmap.buffer = CreateBitmapImage(new, 
					   buffer_data,
					   new->bitmap.width,
					   new->bitmap.height);

    /* Read file */
    {
	int status;
	XImage *image, *buffer;
	unsigned char *image_data;
	char *buffer_data;
	unsigned int width, height;
	int x_hot, y_hot;
	
	status = XmuReadBitmapDataFromFile(new->bitmap.filename, 
					   &width, &height, &image_data,
					   &x_hot, &y_hot);
	if (status == BitmapSuccess) {
	    
	    buffer_data = CreateCleanData(Length(width, height));
	    
	    image = CreateBitmapImage(new, (char *)image_data, width, height);
	    buffer = CreateBitmapImage(new, buffer_data, width, height);
	
	    TransferImageData(new->bitmap.image, buffer);
	
	    DestroyBitmapImage(&new->bitmap.image);
	    DestroyBitmapImage(&new->bitmap.buffer);
	
	    new->bitmap.image = image;
	    new->bitmap.buffer = buffer;
	    new->bitmap.width = width;
	    new->bitmap.height = height;
	    
	    new->bitmap.hot.x = x_hot;
	    new->bitmap.hot.y = y_hot;
	    
	    new->bitmap.changed = False;
	    new->bitmap.zooming = False;
	}

	new->bitmap.filename = XtNewString(new->bitmap.filename);
	
	if (!strcmp(new->bitmap.basename, "")) {
	    new->bitmap.basename = StripFilename(new->bitmap.filename);
	}
	else
	  new->bitmap.basename = XtNewString(new->bitmap.basename);
    }

    Resize((Widget)new);
}


/* returns False if the format is wrong */
Boolean 
BWParseSize(String size, Dimension *width, Dimension *height)
{
  int x, y;
  unsigned int w, h;
  int status;

  status = XParseGeometry(size, &x, &y, &w, &h);

  if (status & (WidthValue | HeightValue)) {
    *width = (Dimension) w;
    *height = (Dimension) h;
    return True;
  }
  else return False;

}


Boolean 
BWQueryMarked(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return QuerySet(BW->bitmap.mark.from_x, BW->bitmap.mark.from_y);
}

static void 
FixMark(BitmapWidget BW)
{
    if (QuerySet(BW->bitmap.mark.from_x, BW->bitmap.mark.from_y)) {
	BW->bitmap.mark.from_x = min(BW->bitmap.mark.from_x, 
				     BW->bitmap.image->width);
	BW->bitmap.mark.from_y = min(BW->bitmap.mark.from_y, 
				     BW->bitmap.image->height);
	BW->bitmap.mark.to_x = min(BW->bitmap.mark.to_x, 
				   BW->bitmap.image->width);
	BW->bitmap.mark.to_y = min(BW->bitmap.mark.to_y, 
				   BW->bitmap.image->height);
	
	if((BW->bitmap.mark.from_x == BW->bitmap.mark.from_y) &&
	   (BW->bitmap.mark.to_x   == BW->bitmap.mark.to_y))
	    BW->bitmap.mark.from_x = 
		BW->bitmap.mark.from_y =
		    BW->bitmap.mark.to_x = 
			BW->bitmap.mark.to_y = NotSet;
    }
}

/* ARGSUSED */
int 
BWStoreFile(Widget w, String filename, String *basename)
{
    BitmapWidget BW = (BitmapWidget) w;
    int status;
    unsigned char *storage_data;
    unsigned int width, height;
    int x_hot, y_hot;
    
    status = XmuReadBitmapDataFromFile(filename, &width, &height,
				       &storage_data, &x_hot, &y_hot);
    if (status == BitmapSuccess) {

	DestroyBitmapImage(&BW->bitmap.storage);
	
	BW->bitmap.storage = CreateBitmapImage(BW, (char *)storage_data, width, height);

	return BitmapSuccess;
    }
    else
	XtWarning(" read file failed.  BitmapWidget");
    
    return status;
}

String 
BWUnparseStatus(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    XmuSnprintf(BW->bitmap.status, sizeof(BW->bitmap.status),
	    "Filename: %s  Basename: %s  Size: %dx%d",
	    (strcmp(BW->bitmap.filename, "") ? BW->bitmap.filename : "<none>"),
	    (strcmp(BW->bitmap.basename, "") ? BW->bitmap.basename : "<none>"),
	    BW->bitmap.width, BW->bitmap.height);

    return BW->bitmap.status;
}

void 
BWChangeFilename(Widget w, String str)
{
  BitmapWidget BW = (BitmapWidget) w;
  
  if (str) {
    XtFree(BW->bitmap.filename);
    BW->bitmap.filename = XtNewString( str);
  }
}

void 
BWChangeBasename(Widget w, String str)
{
  BitmapWidget BW = (BitmapWidget) w;
  
  if (str) {
    XtFree(BW->bitmap.basename);
    BW->bitmap.basename = XtNewString(str);
  }
}


int 
BWReadFile(Widget w, String filename, String basename) /* ARGSUSED */
{
    BitmapWidget BW = (BitmapWidget) w;
    int status;
    XImage *image, *buffer;
    unsigned char *image_data;
    char *buffer_data;
    unsigned int width, height;
    int x_hot, y_hot;
    
    if (!filename)
	filename = BW->bitmap.filename;

    status = XmuReadBitmapDataFromFile(filename, &width, &height, &image_data,
				       &x_hot, &y_hot);
    if (status == BitmapSuccess) {
	
	buffer_data = CreateCleanData(Length(width, height));
	
	image = CreateBitmapImage(BW, (char *)image_data, width, height);
	buffer = CreateBitmapImage(BW, buffer_data, width, height);
	
	TransferImageData(BW->bitmap.image, buffer);
	
	DestroyBitmapImage(&BW->bitmap.image);
	DestroyBitmapImage(&BW->bitmap.buffer);
	
	BW->bitmap.image = image;
	BW->bitmap.buffer = buffer;
	BW->bitmap.width = width;
	BW->bitmap.height = height;
	
	BW->bitmap.hot.x = x_hot;
	BW->bitmap.hot.y = y_hot;
	
	BW->bitmap.changed = False;
	BW->bitmap.zooming = False;
	
	XtFree(BW->bitmap.filename);
	BW->bitmap.filename = XtNewString(filename);
	XtFree(BW->bitmap.basename);
	BW->bitmap.basename= XtNewString(StripFilename(filename));

	BWUnmark(w);
	
	Resize((Widget)BW);

	if (BW->core.visible) {
	    XClearArea(XtDisplay(BW), XtWindow(BW),
		       0, 0, 
		       BW->core.width, BW->core.height,
		       True);
	}
	
	return BitmapSuccess;
    }
    else
	XtWarning(" read file failed.  BitmapWidget");
    
    return status;
}

#if 0
void 
BWSetImage(Widget w, XImage *image)
{
    BitmapWidget BW = (BitmapWidget) w;
    XImage *buffer;
    char *buffer_data;
    
    buffer_data = CreateCleanData(Length(image->width, image->height));
    buffer = CreateBitmapImage(BW, buffer_data, 
			       (Dimension) image->width, 
			       (Dimension) image->height);
    
    TransferImageData(BW->bitmap.image, buffer);
    
    DestroyBitmapImage(&BW->bitmap.image);
    DestroyBitmapImage(&BW->bitmap.buffer);
    
    BW->bitmap.image = image;
    BW->bitmap.buffer = buffer;
    BW->bitmap.width = image->width;
    BW->bitmap.height = image->height;
    
    Resize((Widget)BW);
    
    if (BW->core.visible) {
	XClearArea(XtDisplay(BW), XtWindow(BW),
		   0, 0, 
		   BW->core.width, BW->core.height,
		   True);    
    }
}
#endif

int 
BWWriteFile(Widget w, String filename, String basename)
{
    BitmapWidget BW = (BitmapWidget) w;
    char *data;
    XImage *image;
    XPoint hot;
    int status;
    
    if (BW->bitmap.zooming) {
        data = XtMalloc(Length(BW->bitmap.zoom.image->width, 
			       BW->bitmap.zoom.image->height));
	memmove( data, BW->bitmap.zoom.image->data, 
	      Length(BW->bitmap.zoom.image->width, 
		     BW->bitmap.zoom.image->height));
	image = CreateBitmapImage(BW, data,
				  (Dimension) BW->bitmap.zoom.image->width,
				  (Dimension) BW->bitmap.zoom.image->height);
	CopyImageData(BW->bitmap.image, image, 
		      0, 0, 
		      BW->bitmap.image->width - 1,
		      BW->bitmap.image->height - 1,
		      BW->bitmap.zoom.at_x, BW->bitmap.zoom.at_y);
	
	if (QuerySet(BW->bitmap.hot.x, BW->bitmap.hot.y)) {
	    hot.x = BW->bitmap.hot.x + BW->bitmap.zoom.at_x;
	    hot.y = BW->bitmap.hot.y + BW->bitmap.zoom.at_y;
	}
	else
	    hot = BW->bitmap.zoom.hot;
    }
    else {
	image = BW->bitmap.image;
	hot = BW->bitmap.hot;
    }
    
    if (!filename) filename = BW->bitmap.filename;
    else {
	XtFree(BW->bitmap.filename);
	BW->bitmap.filename = XtNewString(filename);
	XtFree(BW->bitmap.basename);
	BW->bitmap.basename= XtNewString(StripFilename(filename));
    }
    if (!basename) basename = BW->bitmap.basename;
    else {
	XtFree(BW->bitmap.basename);
	BW->bitmap.basename = XtNewString(basename);
    }

    if (DEBUG)
	fprintf(stderr, "Saving filename: %s %s\n", filename, basename);

    status = XmuWriteBitmapDataToFile(filename, basename,
				      image->width, image->height, image->data,
				      hot.x, hot.y);
    if (BW->bitmap.zooming)
	DestroyBitmapImage(&image);
    
    if (status == BitmapSuccess)
	BW->bitmap.changed = False;
    
    return status;
}

String 
BWGetFilename(Widget w, String *str)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    *str = XtNewString(BW->bitmap.filename);

    return *str;
}

String 
BWGetFilepath(Widget w, String *str)
{
    BitmapWidget BW = (BitmapWidget) w;
    String end;

    *str = XtNewString(BW->bitmap.filename);
    end = strrchr(*str, '/');

    if (end)
	*(end + 1) = '\0';
    else 
	**str = '\0';

    return *str;
}


String 
BWGetBasename(Widget w, String *str)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    *str = XtNewString(BW->bitmap.basename);

    return *str;
}

static void 
FixHotSpot(BitmapWidget BW)
{
    if (!QueryInBitmap(BW, BW->bitmap.hot.x, BW->bitmap.hot.y))
	BW->bitmap.hot.x = BW->bitmap.hot.y = NotSet;
}

static void 
ZoomOut(BitmapWidget BW)
{
    CopyImageData(BW->bitmap.image, BW->bitmap.zoom.image, 
		  0, 0, 
		  BW->bitmap.image->width - 1,
		  BW->bitmap.image->height - 1,
		  BW->bitmap.zoom.at_x, BW->bitmap.zoom.at_y);
    
    DestroyBitmapImage(&BW->bitmap.image);
    DestroyBitmapImage(&BW->bitmap.buffer);
    
    BW->bitmap.image = BW->bitmap.zoom.image;
    BW->bitmap.buffer = BW->bitmap.zoom.buffer;
    BW->bitmap.width = BW->bitmap.image->width;
    BW->bitmap.height = BW->bitmap.image->height;
    BW->bitmap.fold = BW->bitmap.zoom.fold;
    BW->bitmap.changed |= BW->bitmap.zoom.changed;
    BW->bitmap.grid = BW->bitmap.zoom.grid;

    if (QuerySet(BW->bitmap.hot.x, BW->bitmap.hot.y)) {
	BW->bitmap.hot.x += BW->bitmap.zoom.at_x;
	BW->bitmap.hot.y += BW->bitmap.zoom.at_y;
    }
    else
	BW->bitmap.hot = BW->bitmap.zoom.hot;
    
    BW->bitmap.mark.from_x = NotSet;
    BW->bitmap.mark.from_y = NotSet;
    BW->bitmap.mark.to_x = NotSet;
    BW->bitmap.mark.to_y = NotSet;
    BW->bitmap.zooming = False;
}    

void 
BWZoomOut(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    if (BW->bitmap.zooming) {
	ZoomOut(BW);
	
	Resize((Widget)BW);
	if (BW->core.visible)
	    XClearArea(XtDisplay(BW), XtWindow(BW),
		       0, 0, 
		       BW->core.width, BW->core.height,
		       True);
    }
}


void 
BWZoomMarked(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    BWZoomIn(w, 
	     BW->bitmap.mark.from_x, BW->bitmap.mark.from_y,
	     BW->bitmap.mark.to_x,   BW->bitmap.mark.to_y);
}

void 
BWZoomIn(Widget w, 
	 Position from_x, Position from_y, 
	 Position to_x, Position to_y)
{
    BitmapWidget BW = (BitmapWidget) w;
    XImage *image, *buffer;    
    Dimension width, height;
    char *image_data, *buffer_data;
  
    if (BW->bitmap.zooming)
	ZoomOut(BW);
    
    QuerySwap(from_x, to_x);
    QuerySwap(from_y, to_y);
    from_x = max(0, from_x);
    from_y = max(0, from_y);
    to_x = min(BW->bitmap.width - 1, to_x);
    to_y = min(BW->bitmap.height - 1, to_y);
    
    width = to_x - from_x + 1;
    height = to_y - from_y + 1;

    image_data = CreateCleanData(Length(width, height));
    buffer_data = CreateCleanData(Length(width, height));

    image = CreateBitmapImage(BW, image_data, width, height);
    buffer = CreateBitmapImage(BW, buffer_data, width, height);

    CopyImageData(BW->bitmap.image, image, from_x, from_y, to_x, to_y, 0, 0);
    CopyImageData(BW->bitmap.buffer, buffer, from_x, from_y, to_x, to_y, 0, 0);
    
    BW->bitmap.zoom.image = BW->bitmap.image;
    BW->bitmap.zoom.buffer = BW->bitmap.buffer;
    BW->bitmap.zoom.at_x = from_x;
    BW->bitmap.zoom.at_y = from_y;
    BW->bitmap.zoom.fold = BW->bitmap.fold;
    BW->bitmap.zoom.changed = BW->bitmap.changed;
    BW->bitmap.zoom.hot = BW->bitmap.hot;
    BW->bitmap.zoom.grid = BW->bitmap.grid;

    BW->bitmap.image = image;
    BW->bitmap.buffer = buffer;
    BW->bitmap.width = width;
    BW->bitmap.height = height;
    BW->bitmap.changed = False;
    BW->bitmap.hot.x -= from_x;
    BW->bitmap.hot.y -= from_y;
    BW->bitmap.mark.from_x = NotSet;
    BW->bitmap.mark.from_y = NotSet;
    BW->bitmap.mark.to_x = NotSet;
    BW->bitmap.mark.to_y = NotSet;
    BW->bitmap.zooming = True;
    BW->bitmap.grid = True; /* potencially true, could use a resource here */

    FixHotSpot(BW);

    Resize((Widget)BW);
    if (BW->core.visible)
	XClearArea(XtDisplay(BW), XtWindow(BW),
		   0, 0, 
		   BW->core.width, BW->core.height,
		   True);
}


void 
BWRescale(Widget w, Dimension width, Dimension height)
{
    BitmapWidget BW = (BitmapWidget) w;
    XImage *image, *buffer;
    char *buffer_data;

    if (BW->bitmap.zooming)
	ZoomOut(BW);
        
    image = ScaleBitmapImage(BW, BW->bitmap.image, 
		       (double) width / (double) BW->bitmap.image->width,
		       (double) height / (double) BW->bitmap.image->height);

    buffer_data = CreateCleanData(Length(image->width, image->height));
    buffer = CreateBitmapImage(BW, buffer_data, 
			       (Dimension) image->width, 
			       (Dimension) image->height);
    
    TransferImageData(BW->bitmap.buffer, buffer);

    DestroyBitmapImage(&BW->bitmap.image);
    DestroyBitmapImage(&BW->bitmap.buffer);
    
    BW->bitmap.image = image;
    BW->bitmap.buffer = buffer;
    BW->bitmap.width = image->width;
    BW->bitmap.height = image->height;
    
    FixHotSpot(BW);
    FixMark(BW);

    Resize((Widget)BW);
    if (BW->core.visible)
	XClearArea(XtDisplay(BW), XtWindow(BW),
		   0, 0, 
		   BW->core.width, BW->core.height,
		   True);
}

Boolean 
BWQueryZooming(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return BW->bitmap.zooming;
}


static void 
ResizeGrid(BitmapWidget BW, Dimension width, Dimension height)
{
  XImage *image, *buffer;
  char *image_data, *buffer_data;
  
  if (BW->bitmap.zooming)
    ZoomOut(BW);
  
  image_data = CreateCleanData(Length(width, height));
  buffer_data = CreateCleanData(Length(width, height));
  
  image = CreateBitmapImage(BW, image_data, width, height);
  buffer = CreateBitmapImage(BW, buffer_data, width, height);
  
  TransferImageData(BW->bitmap.image, image);
  TransferImageData(BW->bitmap.buffer, buffer);
  
  DestroyBitmapImage(&BW->bitmap.image);
  DestroyBitmapImage(&BW->bitmap.buffer);
  
  BW->bitmap.image = image;
  BW->bitmap.buffer = buffer;
  BW->bitmap.width = width;
  BW->bitmap.height = height;
  
  FixHotSpot(BW);
  FixMark(BW);
}

void 
BWResize(Widget w, Dimension width, Dimension height)
{
    BitmapWidget BW = (BitmapWidget) w;

    ResizeGrid(BW, width, height);

    Resize((Widget)BW);
    if (BW->core.visible)
	XClearArea(XtDisplay(BW), XtWindow(BW),
		   0, 0, 
		   BW->core.width, BW->core.height,
		   True);
}

static void 
Destroy(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    XFreeGC(XtDisplay(w), BW->bitmap.drawing_gc);
    XFreeGC(XtDisplay(w), BW->bitmap.highlighting_gc);
    XFreeGC(XtDisplay(w), BW->bitmap.frame_gc);
    XFreeGC(XtDisplay(w), BW->bitmap.axes_gc);
    BWRemoveAllRequests(w);

    XtFree(BW->bitmap.filename);
    XtFree(BW->bitmap.basename);
}


static void 
Resize(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    Dimension squareW, squareH;

    squareW = max(1, ((int)BW->core.width - 2 * (int)BW->bitmap.margin) / 
		  (int)BW->bitmap.width);
    squareH = max(1, ((int)BW->core.height - 2 * (int)BW->bitmap.margin) / 
		  (int)BW->bitmap.height);

    if (BW->bitmap.proportional)
	BW->bitmap.squareW = BW->bitmap.squareH = min(squareW, squareH);
    else {
	BW->bitmap.squareW = squareW;
	BW->bitmap.squareH = squareH;
    }
    
    BW->bitmap.horizOffset = max((Position)BW->bitmap.margin, 
				 (Position)(BW->core.width - 
					    BW->bitmap.width * 
					    BW->bitmap.squareW) / 2);
    BW->bitmap.vertOffset = max((Position)BW->bitmap.margin, 
				(Position)(BW->core.height - 
					   BW->bitmap.height * 
					   BW->bitmap.squareH) / 2);

    BW->bitmap.grid &= ((BW->bitmap.squareW > BW->bitmap.grid_tolerance) && 
			(BW->bitmap.squareH > BW->bitmap.grid_tolerance));
}

/* ARGSUSED */
static void 
Redisplay(Widget w, XEvent *event, Region region)
{
     BitmapWidget BW = (BitmapWidget) w;

  if(event->type == Expose
     &&
     BW->core.visible)
    if (BW->bitmap.stipple_change_expose_event == False)
      Refresh(BW, 
	      event->xexpose.x, event->xexpose.y,
	      event->xexpose.width, event->xexpose.height);
}

void 
BWClip(Widget w, Position x, Position y, Dimension width, Dimension height)
{
    Position      from_x, from_y,
                  to_x, to_y;
    BitmapWidget BW = (BitmapWidget) w;
    XRectangle rectangle;
  
    from_x = InBitmapX(BW, x);
    from_y = InBitmapY(BW, y);
    to_x = InBitmapX(BW, x + width);
    to_y = InBitmapY(BW, y + height);
    QuerySwap(from_x, to_x);
    QuerySwap(from_y, to_y);
    from_x = max(0, from_x);
    from_y = max(0, from_y);
    to_x = min(BW->bitmap.width - 1, to_x);
    to_y = min(BW->bitmap.height - 1, to_y);

    rectangle.x = InWindowX(BW, from_x);
    rectangle.y = InWindowY(BW, from_y);
    rectangle.width = InWindowX(BW, to_x  + 1) - InWindowX(BW, from_x);
    rectangle.height = InWindowY(BW, to_y + 1) - InWindowY(BW, from_y);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.highlighting_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.drawing_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.frame_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.axes_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
}

void 
BWUnclip(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    XRectangle rectangle;
  
    rectangle.x = InWindowX(BW, 0);
    rectangle.y = InWindowY(BW, 0);
    rectangle.width = InWindowX(BW, BW->bitmap.width) - InWindowX(BW, 0);
    rectangle.height = InWindowY(BW, BW->bitmap.height) - InWindowY(BW, 0);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.highlighting_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.drawing_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.frame_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.axes_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);
}

static void 
Refresh(BitmapWidget BW, Position x, Position y, 
	Dimension width, Dimension height)
{
    XRectangle rectangle;

    rectangle.x = min(x, InWindowX(BW, InBitmapX(BW, x)));
    rectangle.y = min(y, InWindowY(BW, InBitmapY(BW, y)));
    rectangle.width = max(x + width,
		     InWindowX(BW, InBitmapX(BW, x + width)+1)) - rectangle.x;
    rectangle.height = max(y + height,
		     InWindowY(BW, InBitmapY(BW, y + height)+1)) - rectangle.y;
    
    XClearArea(XtDisplay(BW), XtWindow(BW),
	       rectangle.x, rectangle.y,
	       rectangle.width, rectangle.height,
	       False);

    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.frame_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);

    XDrawRectangle(XtDisplay(BW), XtWindow(BW),
		   BW->bitmap.frame_gc,
		   InWindowX(BW, 0) - 1, InWindowY(BW, 0) - 1,
		   InWindowX(BW, BW->bitmap.width) - InWindowX(BW, 0) + 1, 
		   InWindowY(BW, BW->bitmap.height) - InWindowY(BW, 0) + 1);

    BWClip((Widget) BW, x, y, width, height);

    BWRedrawGrid((Widget) BW, x, y, width, height);

    BWRedrawSquares((Widget) BW, x, y, width, height);

    BWRedrawMark((Widget) BW);
    BWRedrawHotSpot((Widget) BW);
    BWRedrawAxes((Widget) BW);
    BWUnclip((Widget) BW);
}

Boolean 
BWQueryGrid(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return BW->bitmap.grid;
}

void 
BWSwitchGrid(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    BW->bitmap.grid ^= TRUE;
    BWDrawGrid(w,
	       0, 0,
	       BW->bitmap.image->width - 1, BW->bitmap.image->height - 1);
}

void 
BWGrid(Widget w, Boolean _switch)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    if (BW->bitmap.grid != _switch)
	BWSwitchGrid(w);
}

Boolean 
BWQueryDashed(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return (BW->bitmap.dashed);
}

void 
BWSwitchDashed(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;
    XRectangle rectangle;

    BWRedrawGrid(w, 0, 0, BW->bitmap.width - 1, BW->bitmap.height - 1);

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = BW->core.width;
    rectangle.height = BW->core.height;

    XSetClipRectangles(XtDisplay(BW),
		       BW->bitmap.frame_gc,
		       0, 0,
		       &rectangle, 1,
		       Unsorted);

    XDrawRectangle(XtDisplay(BW), XtWindow(BW),
		   BW->bitmap.frame_gc,
		   InWindowX(BW, 0) - 1, InWindowY(BW, 0) - 1,
		   InWindowX(BW, BW->bitmap.width) - InWindowX(BW, 0) + 1, 
		   InWindowY(BW, BW->bitmap.height) - InWindowY(BW, 0) + 1);
    
    BW->bitmap.dashed ^= True;
    XSetFillStyle(XtDisplay(BW), BW->bitmap.frame_gc,
		  (BW->bitmap.dashed ? FillStippled : FillSolid));
 
    XDrawRectangle(XtDisplay(BW), XtWindow(BW),
		   BW->bitmap.frame_gc,
		   InWindowX(BW, 0) - 1, InWindowY(BW, 0) - 1,
		   InWindowX(BW, BW->bitmap.width) - InWindowX(BW, 0) + 1, 
		   InWindowY(BW, BW->bitmap.height) - InWindowY(BW, 0) + 1);

    BWUnclip(w);
   
    BWRedrawGrid(w, 0, 0, BW->bitmap.width - 1, BW->bitmap.height - 1);
}

void 
BWDashed(Widget w, Boolean _switch)
{
    BitmapWidget BW = (BitmapWidget) w;
    
    if (BW->bitmap.dashed != _switch)
	BWSwitchDashed(w);
}

static Boolean 
SetValues(Widget old, Widget request, Widget new, 
	  ArgList args, Cardinal *num_args) /* ARGSUSED */
{
  BitmapWidget oldbw = (BitmapWidget) old;
  BitmapWidget newbw = (BitmapWidget) new;
  Boolean resize = False;
  Boolean redisplay = False;

#define NE(field) (oldbw->field != newbw->field)

  if (NE(bitmap.grid))
    BWSwitchGrid(old);

  if (NE(bitmap.dashed))
    BWSwitchDashed(old);

  if (NE(bitmap.axes))
    BWSwitchAxes(old);

  if (NE(bitmap.stippled))
    BWSwitchStippled(old);

  if (NE(bitmap.proportional))
    resize = True;

  if (NE(bitmap.filename) || NE(bitmap.basename)  || NE(bitmap.size))
    BWChangeNotify(old);

  if (NE(bitmap.filename)) {
    if (newbw->bitmap.filename) {
      XtFree(oldbw->bitmap.filename);
      newbw->bitmap.filename = XtNewString(newbw->bitmap.filename);
    }
    else 
      newbw->bitmap.filename = oldbw->bitmap.filename;
  }

  if (NE(bitmap.basename)) {
    if (newbw->bitmap.basename) {
      XtFree(oldbw->bitmap.basename);
      newbw->bitmap.basename = XtNewString(newbw->bitmap.basename);
    }
    else 
      newbw->bitmap.basename = oldbw->bitmap.basename;
  }
  
  if (NE(bitmap.size)) {
    Dimension width, height;
    
    if (BWParseSize(newbw->bitmap.size, &width, &height)) { 
      ResizeGrid(newbw, width, height);
      resize = True;
    }
  }
  
  if (NE(bitmap.margin) || 
      NE(bitmap.grid_tolerance) ||
      NE(bitmap.squareW) ||
      NE(bitmap.squareH) ||
      NE(core.height) ||
      NE(core.width))
    resize = True;

  if (NE(bitmap.hot.x) || NE(bitmap.hot.y))
    BWSetHotSpot(old, newbw->bitmap.hot.x, newbw->bitmap.hot.y);
    
  if (NE(bitmap.foreground_pixel) || NE(core.background_pixel)) {
    XSetForeground(XtDisplay(new), 
		   newbw->bitmap.drawing_gc,
		   newbw->bitmap.foreground_pixel 
		   ^ 
		   newbw->core.background_pixel);
    redisplay = True;
  }

  if (NE(bitmap.frame_pixel) || NE(core.background_pixel)) {
    XSetForeground(XtDisplay(new), 
		   newbw->bitmap.frame_gc,
		   newbw->bitmap.frame_pixel
		   ^ 
		   newbw->core.background_pixel);
    redisplay = True;
  }

  if (NE(bitmap.dashes)) {
    XSetStipple(XtDisplay(new),
		newbw->bitmap.frame_gc,
		newbw->bitmap.dashes);
    redisplay = True;
  }

  if (NE(bitmap.highlight_pixel) || NE(core.background_pixel)) {
    RedrawStippled(newbw);
    XSetForeground(XtDisplay(new), 
		   newbw->bitmap.highlighting_gc,
		   newbw->bitmap.highlight_pixel
		   ^ 
		   newbw->core.background_pixel);
    RedrawStippled(newbw);
  }
 
  if (NE(bitmap.stipple)) {
    RedrawStippled(newbw);
    XSetStipple(XtDisplay(new),
		newbw->bitmap.highlighting_gc,
		newbw->bitmap.stipple);
    RedrawStippled(newbw);
  }
  
  if (resize) Resize((Widget)newbw);

    return (redisplay || resize);

#undef NE
}

Boolean 
BWQueryProportional(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    return (BW->bitmap.proportional);
}

void 
BWSwitchProportional(Widget w)
{
    BitmapWidget BW = (BitmapWidget) w;

    BW->bitmap.proportional ^= True;

    Resize((Widget)BW);
    if (BW->core.visible)
	XClearArea(XtDisplay(BW), XtWindow(BW),
		   0, 0, 
		   BW->core.width, BW->core.height,
		   True);
}

#if 0
void 
BWProportional(Widget w, Boolean _switch)
{
    BitmapWidget BW = (BitmapWidget) w;

    if (BW->bitmap.proportional != _switch)
	BWSwitchProportional(w);
}
#endif

void 
BWTPaste(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    BitmapWidget BW = (BitmapWidget) w;

    BWRequestSelection(w, event->xbutton.time, TRUE);

    if (!BWQueryStored(w))
	return;
    
    BWEngageRequest(w, RestoreRequest, False, 
		    (char *)&(event->xbutton.state), sizeof(int));
    
    OnePointHandler(w,
	       (BWStatus*) BW->bitmap.request_stack[BW->bitmap.current].status,
	       event, NULL);
}

void 
BWTMark(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    BitmapWidget BW = (BitmapWidget) w;

    BWEngageRequest(w, MarkRequest, False,
		    (char *)&(event->xbutton.state), sizeof(int));
    TwoPointsHandler(w,
            (BWStatus*) BW->bitmap.request_stack[BW->bitmap.current].status,
	     event, NULL);

}

void 
BWTMarkAll(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    BWMarkAll(w);

    BWGrabSelection(w, event->xkey.time);
}

void 
BWTUnmark(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    BWUnmark(w);
}

/*****************************************************************************/
