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

/*
 * Author:  Davor Matic, MIT X Consortium
 */




#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xmu/SysUtil.h>
#include "Bitmap.h"

#include <X11/bitmaps/xlogo16>

static char *usage = "[-options ...] filename\n\
\n\
where options include all standard toolkit options plus:\n\
     -size WIDTHxHEIGHT\n\
     -sw dimension\n\
     -sh dimension\n\
     -gt dimension\n\
     -grid, +grid\n\
     -axes, +axes\n\
     -dashed, +dashed\n\
     -stippled, +stippled\n\
     -proportional, +proportional\n\
     -basename basename\n\
     -dashes filename\n\
     -stipple filename\n\
     -hl color\n\
     -fr color\n\
\n\
The default WIDTHxHEIGHT is 16x16.\n";

static XrmOptionDescRec options[] = {
  { "-axes",	    "*bitmap.axes",	    XrmoptionNoArg,	"True"},
  { "+axes",	    "*bitmap.axes",	    XrmoptionNoArg,	"False"},
  { "-basename",    "*bitmap.basename",	    XrmoptionSepArg,	NULL},
  { "-dashed",	    "*bitmap.dashed",	    XrmoptionNoArg,	"True"},
  { "+dashed",	    "*bitmap.dashed",	    XrmoptionNoArg,	"False"},
  { "-dashes",	    "*bitmap.dashes",	    XrmoptionSepArg,	NULL},
  { "-fr",	    "*bitmap.frame",	    XrmoptionSepArg,	NULL},
  { "-gt",	    "*bitmap.gridTolerance",XrmoptionSepArg,	NULL},
  { "-grid",	    "*bitmap.grid",	    XrmoptionNoArg,	"True"},
  { "+grid",	    "*bitmap.grid",	    XrmoptionNoArg,	"False"},
  { "-hl",	    "*bitmap.highlight",    XrmoptionSepArg,	NULL},
  { "-proportional","*bitmap.proportional", XrmoptionNoArg,	"True"},
  { "+proportional","*bitmap.proportional", XrmoptionNoArg,	"False"},
  { "-size",	    "*bitmap.size",	    XrmoptionSepArg,	NULL},
  { "-sh",	    "*bitmap.squareHeight", XrmoptionSepArg,	NULL},
  { "-sw",	    "*bitmap.squareWidth",  XrmoptionSepArg,	NULL},
  { "-stipple",	    "*bitmap.stipple",	    XrmoptionSepArg,	NULL},
  { "-stippled",    "*bitmap.stippled",	    XrmoptionNoArg,	"True"},
  { "+stippled",    "*bitmap.stippled",	    XrmoptionNoArg,	"False"},
};

typedef struct {
  int             id;
  String          name;
  Boolean         trap;
  Widget          widget;
  } ButtonRec;

static ButtonRec file_menu[] = {
#define New 101
  {New, "new", True},
#define Load 102
  {Load, "load", True},
#define Insert 103
  {Insert, "insert", True},
#define Save 104
  {Save, "save", True},
#define SaveAs 105
  {SaveAs, "saveAs", True},
#define Resize 106
  {Resize, "resize", True},
#define Rescale 107
  {Rescale, "rescale", True},
#define Filename 108
  {Filename, "filename", True},
#define Basename 109
  {Basename, "basename", True},
#define Dummy -1
  {Dummy, "line", False},
#define Quit 110
  {Quit, "quit", True},
};

static ButtonRec edit_menu[] = {
#define Image 201
  {Image, "image", True},
#define Grid 203
  {Grid, "grid", True},
#define Dashed 204
  {Dashed, "dashed", True},
#define Axes 205
  {Axes, "axes", True},
#define Stippled 206
  {Stippled, "stippled", True},
#define Proportional 207
  {Proportional, "proportional", True},
#define Zoom 208
  {Zoom, "zoom", True},
/* Dummy */
  {Dummy, "line", False},
#define Cut 209
  {Cut, "cut", True},
#define Copy 210
  {Copy, "copy", True},
#define Paste 211
  {Paste, "paste", True},
};

static ButtonRec buttons[] = {
/*#define Clear 1*/
  {Clear, "clear", False},
/*#define Set 2*/
  {Set, "set", False},
/*#define Invert 3*/
  {Invert, "invert", False},
#define Mark 26
  {Mark, "mark", True},
#define Unmark 27
  {Unmark, "unmark", False},
#define CopyImm 4
  {CopyImm, "copy", True},
#define MoveImm 5
  {MoveImm, "move", True},
#define FlipHoriz 6
  {FlipHoriz, "flipHoriz", False},
#define Up 7
  {Up, "up", False},
#define FlipVert 8
  {FlipVert, "flipVert", False},
#define Left 9
  {Left, "left", False},
#define Fold 10
  {Fold, "fold", False},
#define Right 11
  {Right, "right", False},
#define RotateLeft 12
  {RotateLeft, "rotateLeft", False},
#define Down 13
  {Down, "down", False},
#define RotateRight 14
  {RotateRight, "rotateRight", False},
#define Point 15
  {Point, "point", True},
#define Curve 16
  {Curve, "curve", True},
#define Line 17
  {Line, "line", True},
#define Rectangle 18
  {Rectangle, "rectangle", True},
#define FilledRectangle 19
  {FilledRectangle, "filledRectangle", True},
#define Circle 20
  {Circle, "circle", True},
#define FilledCircle 21
  {FilledCircle, "filledCircle", True},
#define FloodFill 22
  {FloodFill, "floodFill", True},
#define SetHotSpot 23
  {SetHotSpot, "setHotSpot", True},
#define ClearHotSpot 24
  {ClearHotSpot, "clearHotSpot", False},
#define Undo 25
  {Undo, "undo", False},
};

#include "Dialog.h"

static Widget 
    top_widget, 
    parent_widget,
    formy_widget,
    fileMenu_widget,
    editMenu_widget,
    status_widget,
    pane_widget, 
    form_widget,
    bitmap_widget,
    image_shell,
    box_widget,
    normal_image_widget,
    inverted_image_widget;
static Boolean image_visible = False;
static Pixmap check_mark;
static Dialog input_dialog, error_dialog, qsave_dialog;
static Time btime;
static String filename = NULL, base_name = NULL, format;
static char message[80];


void FixMenu ( Widget w, XEvent *event, String *params, Cardinal *num_params );
void SwitchImage ( void );
void SwitchGrid( void );
void SwitchDashed( void );
void SwitchAxes( void );
void SwitchStippled( void );
void SwitchProportional( void );
void SwitchZoom( void );
void DoCut( void );
void DoCopy( void );
void DoPaste( void );
void DoNew( void );
void DoLoad( void );
void DoInsert( void );
void DoSave( void );
void DoSaveAs( void );
void DoResize( void );
void DoRescale( void );
void DoFilename( void );
void DoBasename( void );
void DoQuit ( Widget w, XEvent *event, String *params, Cardinal *num_params );

static XtActionsRec actions_table[] = {
  {"fix-menu", FixMenu},
  {"switch-image", (XtActionProc)SwitchImage},
  {"switch-grid", (XtActionProc)SwitchGrid},
  {"switch-dashed", (XtActionProc)SwitchDashed},
  {"switch-axes", (XtActionProc)SwitchAxes},
  {"switch-stippled", (XtActionProc)SwitchStippled},
  {"switch-proportional", (XtActionProc)SwitchProportional},
  {"switch-zoom", (XtActionProc)SwitchZoom},
  {"do-cut", (XtActionProc)DoCut},
  {"do-copy", (XtActionProc)DoCopy},
  {"do-paste", (XtActionProc)DoPaste},
  {"do-new", (XtActionProc)DoNew},
  {"do-load", (XtActionProc)DoLoad},
  {"do-insert", (XtActionProc)DoInsert},
  {"do-save", (XtActionProc)DoSave},
  {"do-save-as", (XtActionProc)DoSaveAs},
  {"do-resize", (XtActionProc)DoResize},
  {"do-rescale", (XtActionProc)DoRescale},
  {"do-filename", (XtActionProc)DoFilename},
  {"do-basename", (XtActionProc)DoBasename},
  {"do-quit", DoQuit}
};

static Atom wm_delete_window;

static void 
FixImage(void)
{
    Pixmap old_image, image;
    int n;
    Arg wargs[2];

    if (!image_visible) return;
    
    n=0;
    XtSetArg(wargs[n], XtNbitmap, &old_image); n++;
    XtGetValues(normal_image_widget, wargs, n);
    
    
    image = BWGetUnzoomedPixmap(bitmap_widget);
    
    n=0;
    XtSetArg(wargs[n], XtNbitmap, image); n++;
    XtSetValues(normal_image_widget, wargs, n);
    XtSetValues(inverted_image_widget, wargs, n);

    if (old_image != XtUnspecifiedPixmap)
	XFreePixmap(XtDisplay(bitmap_widget), old_image);
}

static void 
FixStatus(void)
{
    int n;
    Arg wargs[2];
    String str, label;

    str = BWUnparseStatus(bitmap_widget);

    n=0;
    XtSetArg(wargs[n], XtNlabel, &label); n++;
    XtGetValues(status_widget, wargs, n);

    if (strcmp(str, label)) {
      n = 0;
      XtSetArg(wargs[n], XtNlabel, str); n++;
      XtSetValues(status_widget, wargs, n);
    }

    /*XtFree(str); */
}

static void 
FixUp(void)
{
  FixImage();
  FixStatus();
}

static void 
FixEntry(Widget w, int *id)
{
    int n;
    Arg wargs[2];
    Time dummy = 0;

    n = 0;
    
    switch (*id) {
	
    case Image:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 image_visible ? check_mark : None); n++;
	break;
	
    case Grid:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 BWQueryGrid(bitmap_widget) ? check_mark : None); n++;
	break;
	
    case Dashed:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 BWQueryDashed(bitmap_widget) ? check_mark : None); n++;
	break;
	
    case Axes:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 BWQueryAxes(bitmap_widget) ? check_mark : None); n++;
	break;
	
    case Stippled:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 BWQueryStippled(bitmap_widget) ? check_mark : None); n++;
	break;
	
    case Proportional:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 BWQueryProportional(bitmap_widget) ? check_mark : None); n++;
	break;
	
    case Zoom:
	XtSetArg(wargs[n], XtNleftBitmap, 
		 BWQueryZooming(bitmap_widget) ? check_mark : None); n++;
	break;

    case Copy:
    case Cut:
	XtSetArg(wargs[n], XtNsensitive, BWQueryMarked(bitmap_widget)); n++;
	break;

    case Paste:
	XtSetArg(wargs[n], XtNsensitive, 
		 BWQuerySelection(bitmap_widget, dummy)); n++;
	break;

    default:
	return;
    }
    
    XtSetValues(w, wargs, n);
}

/* ARGSUSED */
void FixMenu(Widget w, 
	     XEvent *event, 
	     String *params, 
	     Cardinal *num_params)
{
    int i;

    btime = event->xbutton.time;
    
    for (i = 0; i < XtNumber(edit_menu); i++)
	FixEntry(edit_menu[i].widget, &edit_menu[i].id);
}

static int zero = 0;
#define Plain  (char *)&zero,sizeof(int)
/* ARGSUSED */
static void 
TheCallback(Widget w,	/* not used */
	    XtPointer clientData, 
	    XtPointer callData)
{
    int *id = (int *)clientData;
    switch (*id) {
	
    case New:
      DoNew();
      break;

    case Load:
      DoLoad();
      break;
	
    case Insert:
      DoInsert();
      break;
	
    case Save:
      DoSave();
      break;
	
    case SaveAs:
      DoSaveAs();
      break;
	
    case Resize:
      DoResize();
      break;

    case Rescale:
      DoRescale();
      break;

    case Filename:
      DoFilename();
      break;

    case Basename:
      DoBasename();
      break;
	
    case Image:
      SwitchImage();
      break;

    case Grid:
      SwitchGrid();
      break;
	
    case Dashed:
      SwitchDashed();
      break;

    case Axes:
      SwitchAxes();
      break;	

    case Stippled:
      SwitchStippled();
      break;
      
    case Proportional:
      SwitchProportional();
      break;
		
    case Zoom:
      SwitchZoom();
      break;
      
    case Cut:
      DoCut();
      break;
      
    case Copy:
      DoCopy();
      break;
      
    case Paste:
      DoPaste();
      break;
	
    case Clear:
	BWStoreToBuffer(bitmap_widget);
	BWClear(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case Set:
	BWStoreToBuffer(bitmap_widget);
	BWSet(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
break;
	
    case Invert:
	BWStoreToBuffer(bitmap_widget);
	BWInvert(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;

    case Mark:
        BWRemoveAllRequests(bitmap_widget);
        BWEngageRequest(bitmap_widget, MarkRequest, True, Plain);
        break;

    case Unmark:
        BWUnmark(bitmap_widget);
        break;

    case CopyImm:
	BWRemoveAllRequests(bitmap_widget);
	if (BWQueryMarked(bitmap_widget)) {
	    BWAddRequest(bitmap_widget, MarkRequest, False, Plain);
	    BWEngageRequest(bitmap_widget, CopyRequest, True, Plain);
	}
	else {
	    BWEngageRequest(bitmap_widget, MarkRequest, False, Plain);
	    BWAddRequest(bitmap_widget, CopyRequest, True, Plain);
	}
	break;
	
    case MoveImm:
	BWRemoveAllRequests(bitmap_widget);
	if (BWQueryMarked(bitmap_widget)) {
	    BWAddRequest(bitmap_widget, MarkRequest, False, Plain);
	    BWEngageRequest(bitmap_widget, MoveRequest, True, Plain);
	}
	else {
	    BWEngageRequest(bitmap_widget, MarkRequest, False, Plain);
	    BWAddRequest(bitmap_widget, MoveRequest, True, Plain);
	}
	break;
		
    case Up:
	BWStoreToBuffer(bitmap_widget);
	BWUp(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case Down:
	BWStoreToBuffer(bitmap_widget);
	BWDown(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case Left:
	BWStoreToBuffer(bitmap_widget);
	BWLeft(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case Right:
	BWStoreToBuffer(bitmap_widget);
	BWRight(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case Fold:
	BWStoreToBuffer(bitmap_widget);
	BWFold(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case FlipHoriz:
	BWStoreToBuffer(bitmap_widget);
	BWFlipHoriz(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case FlipVert:
	BWStoreToBuffer(bitmap_widget);
	BWFlipVert(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case RotateRight:
	BWStoreToBuffer(bitmap_widget);
	BWRotateRight(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case RotateLeft:
	BWStoreToBuffer(bitmap_widget);
	BWRotateLeft(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;
	
    case Point:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, PointRequest, True, Plain);
	break;

    case Curve:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, CurveRequest, True, Plain);
	break;
	
    case Line:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, LineRequest, True, Plain);
	break;
	
    case Rectangle:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, RectangleRequest, True, Plain);
	break;
	
    case FilledRectangle:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, FilledRectangleRequest, True, Plain);
	break;
	
    case Circle:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, CircleRequest, True, Plain);
	break;
	
    case FilledCircle:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, FilledCircleRequest, True, Plain);
	break;
	
    case FloodFill:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, FloodFillRequest, True, Plain);
	break;
	
    case SetHotSpot:
	BWRemoveAllRequests(bitmap_widget);
	BWEngageRequest(bitmap_widget, HotSpotRequest, True, Plain);
	break;
	
    case ClearHotSpot:
	BWStoreToBuffer(bitmap_widget);
	BWClearHotSpot(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;

    case Undo:
	BWUndo(bitmap_widget);
	BWChangeNotify(bitmap_widget);
	BWSetChanged(bitmap_widget);
	break;	

    case Quit:
      DoQuit(bitmap_widget, NULL, NULL, NULL);
      break;	
    } /* don't add anything below this line */
}

/* ARGSUSED */

void SwitchImage(void)
{
  if (image_visible) {
    XtPopdown(image_shell);
    image_visible = False;
  }
  else {
    Position image_x, image_y;
    int n;
    Arg wargs[3];
    
    XtTranslateCoords(bitmap_widget,
		      10, 10, &image_x, &image_y);
    
    n = 0;
    XtSetArg(wargs[n], XtNx, image_x); n++;
    XtSetArg(wargs[n], XtNy, image_y); n++;
    XtSetValues(image_shell, wargs, n);
    
    image_visible = True;
    
    FixImage();
    XtPopup(image_shell, XtGrabNone);
    FixImage();
  }
}

void SwitchGrid(void)
{
  BWSwitchGrid(bitmap_widget);
}

void SwitchDashed(void)
{
  BWSwitchDashed(bitmap_widget);
}

void SwitchAxes(void)
{
  BWSwitchAxes(bitmap_widget);
}

void SwitchStippled(void)
{
  BWSwitchStippled(bitmap_widget); 
}

void SwitchProportional(void)
{
  BWSwitchProportional(bitmap_widget);
}

void SwitchZoom(void)
{
  if (BWQueryZooming(bitmap_widget)) {
    BWZoomOut(bitmap_widget);
    BWChangeNotify(bitmap_widget);
  }
  else {
    if (BWQueryMarked(bitmap_widget)) {
      BWStoreToBuffer(bitmap_widget);
      BWZoomMarked(bitmap_widget);
      BWChangeNotify(bitmap_widget);
    }
    else {
      BWEngageRequest(bitmap_widget, ZoomInRequest, False, Plain);
    }
  }
}

void DoCut(void)
{
  BWStore(bitmap_widget);
  BWStoreToBuffer(bitmap_widget);
  BWClearMarked(bitmap_widget);
  BWUnmark(bitmap_widget);
  BWChangeNotify(bitmap_widget);
  BWSetChanged(bitmap_widget);
}

void DoCopy(void)
{
  BWStore(bitmap_widget);
  BWUnmark(bitmap_widget);
}

void DoPaste(void)
{
  BWRequestSelection(bitmap_widget, btime, TRUE); 
  BWEngageRequest(bitmap_widget, RestoreRequest, False, Plain);
}

void DoNew(void)
{
  BWGetFilename(bitmap_widget, &filename);
  if (PopupDialog(input_dialog, "New file:",
		  filename, &filename, XtGrabExclusive) == Okay) {
    BWChangeFilename(bitmap_widget, filename);
    BWChangeBasename(bitmap_widget, filename);
    BWStoreToBuffer(bitmap_widget);
    BWClear(bitmap_widget);
    BWClearHotSpot(bitmap_widget);
    BWChangeNotify(bitmap_widget);
    BWClearChanged(bitmap_widget);
    BWUnmark(bitmap_widget);
    FixStatus();
  }
}

void DoLoad(void)
{
  if (BWQueryChanged(bitmap_widget)) {
    BWGetFilename(bitmap_widget, &filename);
  RetryLoadSave:
    switch (PopupDialog(qsave_dialog, "Save file before loading?",
			filename, &filename, XtGrabExclusive)) {
    case Yes:
      if (BWWriteFile(bitmap_widget, filename, NULL)
	  != BitmapSuccess) {
	XmuSnprintf(message, sizeof(message), "Can't write file: %s", filename);
	if (PopupDialog(error_dialog, message,
			NULL, NULL, XtGrabExclusive) == Retry)
	  goto RetryLoadSave;
      }
      break;
      
    case Cancel:
      return;
    }
  }
  BWGetFilepath(bitmap_widget, &filename);
 RetryLoad:
  if (PopupDialog(input_dialog, "Load file:",
		  filename, &filename, XtGrabExclusive) == Okay) {
    if (BWReadFile(bitmap_widget, filename, NULL) != BitmapSuccess) {
      XmuSnprintf(message, sizeof(message), "Can't read file: %s", filename);
      if (PopupDialog(error_dialog, message,
		      NULL, NULL, XtGrabExclusive) == Retry)
	goto RetryLoad;
    }
    else {
      BWChangeNotify(bitmap_widget);
      BWClearChanged(bitmap_widget);
      FixStatus();
    }
  }
}

void DoInsert(void)
{
  BWGetFilepath(bitmap_widget, &filename);
 RetryInsert:
  if (PopupDialog(input_dialog, "Insert file:",
		  filename, &filename, XtGrabExclusive) == Okay) {
    if (BWStoreFile(bitmap_widget, filename, NULL) != BitmapSuccess) {
      XmuSnprintf(message, sizeof(message), "Can't read file: %s", filename);
      if (PopupDialog(error_dialog, message,
		      NULL, NULL, XtGrabExclusive) == Retry)
	goto RetryInsert;
    }
    else {
      BWEngageRequest(bitmap_widget, RestoreRequest, False, Plain);
    }
  }
}

void DoSave(void)
{
  BWGetFilename(bitmap_widget, &filename);
  if (!strcmp(filename, "")) 
    DoSaveAs();
  else if (BWWriteFile(bitmap_widget, NULL, NULL) != BitmapSuccess) {
    XmuSnprintf(message, sizeof(message), "Can't write file: %s", filename);
    if (PopupDialog(error_dialog, message,
		    NULL, NULL, XtGrabExclusive) == Retry) 
      DoSaveAs();
  }
  else {
    BWClearChanged(bitmap_widget);
  }
}

void DoSaveAs(void)
{
  BWGetFilename(bitmap_widget, &filename);
 RetrySave:
  if (PopupDialog(input_dialog, "Save file:",
		  filename, &filename, XtGrabExclusive) == Okay) {
    if (BWWriteFile(bitmap_widget, filename, NULL) != BitmapSuccess) {
      XmuSnprintf(message, sizeof(message), "Can't write file: %s", filename);
      if (PopupDialog(error_dialog, message,
		      NULL, NULL, XtGrabExclusive) == Retry)
	goto RetrySave;
    }
    else {
      BWClearChanged(bitmap_widget);
      FixStatus();
    }
  }
}

void DoResize(void)
{
  Dimension width, height;
  format = "";
 RetryResize:
  if (PopupDialog(input_dialog, "Resize to WIDTHxHEIGHT:",
		  format, &format, XtGrabExclusive) == Okay) {
    if (BWParseSize(format, &width, &height)) {
      BWResize(bitmap_widget, width, height);
      BWChangeNotify(bitmap_widget);
      BWSetChanged(bitmap_widget);
      FixStatus();
    }
    else {
      XmuSnprintf(message, sizeof(message), "Wrong format: %s", format);
      if (PopupDialog(error_dialog, message,
		      NULL, NULL, XtGrabExclusive) == Retry)
	goto RetryResize;
    }
  }
}

void DoRescale(void)
{
  Dimension width, height;

  format = "";
 RetryRescale:
  if (PopupDialog(input_dialog, "Rescale to WIDTHxHEIGHT:",
		  format, &format, XtGrabExclusive) == Okay) {
    if (BWParseSize(format, &width, &height)) {
      BWRescale(bitmap_widget, width, height);
      BWChangeNotify(bitmap_widget);
      BWSetChanged(bitmap_widget);
      FixStatus();
    }
    else {
      XmuSnprintf(message, sizeof(message), "Wrong format: %s", format);
      if (PopupDialog(error_dialog, message,
		      NULL, NULL, XtGrabExclusive) == Retry)
	goto RetryRescale;
    }
  }
}

void DoFilename(void)
{
  BWGetFilename(bitmap_widget, &filename);
  if (PopupDialog(input_dialog, "Change filename:",
		  filename, &filename, XtGrabExclusive) == Okay) {
    BWChangeFilename(bitmap_widget, filename);
    FixStatus();
  }
}

void DoBasename(void)
{  
  BWGetBasename(bitmap_widget, &base_name);
  if (PopupDialog(input_dialog, "Change basename:",
		  base_name, &base_name, XtGrabExclusive) == Okay) {
    BWChangeBasename(bitmap_widget, base_name);
    FixStatus();
  }
}

void DoQuit(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  if (BWQueryChanged(bitmap_widget)) {
    BWGetFilename(bitmap_widget, &filename);
  RetryQuit:
    switch (PopupDialog(qsave_dialog, "Save file before quitting?",
			filename, &filename, XtGrabExclusive)) {
    case Yes:
      if (BWWriteFile(bitmap_widget, filename, NULL) 
	  != BitmapSuccess) {
	XmuSnprintf(message, sizeof(message), "Can't write file: %s", filename);
	if (PopupDialog(error_dialog, message, 
			NULL, NULL, XtGrabExclusive) == Retry) 
	  goto RetryQuit;
	else return;
      }
      break;
      
    case Cancel:
      return;
    }
  }
  exit(0);
}

int main(int argc, char *argv[])
{
    int i, n;
    Arg wargs[2];
    Widget w;
    Widget radio_group = NULL; 
    XtPointer radio_data = NULL;
    
    top_widget = XtInitialize(NULL, "Bitmap", 
			      options, XtNumber(options), &argc, argv);

    if (argc > 2) {
	fprintf(stderr, usage);
	exit (0);
    }

    check_mark = XCreateBitmapFromData(XtDisplay(top_widget),
				      RootWindowOfScreen(XtScreen(top_widget)),
				      (char *) xlogo16_bits, 
				      xlogo16_width, 
				      xlogo16_height);

    XtAddActions(actions_table, XtNumber(actions_table));
    XtOverrideTranslations
      (top_widget,
       XtParseTranslationTable("<Message>WM_PROTOCOLS: do-quit()"));

    parent_widget = XtCreateManagedWidget("parent", panedWidgetClass,
					 top_widget, NULL, 0);

    formy_widget = XtCreateManagedWidget("formy", formWidgetClass,
				       parent_widget, NULL, 0);

    fileMenu_widget = XtCreatePopupShell("fileMenu", 
					 simpleMenuWidgetClass, 
					 formy_widget, NULL, 0);
    
    (void) XtCreateManagedWidget("fileButton",
				 menuButtonWidgetClass, 
				 formy_widget, NULL, 0);

    for (i = 0; i < XtNumber(file_menu); i++) {
	w = XtCreateManagedWidget(file_menu[i].name, 
				  (file_menu[i].trap ? 
				   smeBSBObjectClass : smeLineObjectClass),
				  fileMenu_widget, NULL, 0),
	XtAddCallback(w,
		      XtNcallback,
		      TheCallback,
		      (XtPointer)&file_menu[i].id);
	
	file_menu[i].widget = w;
    }
        
    editMenu_widget = XtCreatePopupShell("editMenu", 
					 simpleMenuWidgetClass, 
					 formy_widget, NULL, 0);
    
    (void) XtCreateManagedWidget("editButton", 
				 menuButtonWidgetClass, 
				 formy_widget, NULL, 0);

    for (i = 0; i < XtNumber(edit_menu); i++) {
	w = XtCreateManagedWidget(edit_menu[i].name, 
				  (edit_menu[i].trap ? 
				   smeBSBObjectClass : smeLineObjectClass),
				  editMenu_widget, NULL, 0),
	XtAddCallback(w,
		      XtNcallback,
		      TheCallback,
		      (XtPointer)&edit_menu[i].id);
	
	edit_menu[i].widget = w;
    }

    status_widget = XtCreateManagedWidget("status", labelWidgetClass,
					  formy_widget, NULL, 0);

    pane_widget = XtCreateManagedWidget("pane", panedWidgetClass,
					parent_widget, NULL, 0);

    form_widget = XtCreateManagedWidget("form", formWidgetClass, 
					pane_widget, NULL, 0);
        
    for (i = 0; i < XtNumber(buttons); i++) {
	w = XtCreateManagedWidget(buttons[i].name, 
				  (buttons[i].trap ? 
				   toggleWidgetClass : commandWidgetClass),
				  form_widget, NULL, 0);

	XtAddCallback(w,
		      XtNcallback,
		      TheCallback,
		      (XtPointer)&buttons[i].id);

	buttons[i].widget = w;

	if (buttons[i].id == Point) {
	    radio_group = buttons[i].widget;
	    radio_data  = buttons[i].name;
	}
    }
    bitmap_widget = XtCreateManagedWidget("bitmap", bitmapWidgetClass,
					  pane_widget, NULL, 0);
    XtRealizeWidget(top_widget);
    if (argc > 1)
      if (BWReadFile(bitmap_widget, argv[1], NULL)) 

    wm_delete_window = XInternAtom(XtDisplay(top_widget), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(top_widget), XtWindow(top_widget),
                            &wm_delete_window, 1);

    
    image_shell = XtCreatePopupShell("image", transientShellWidgetClass,
				     top_widget, NULL, 0);

    box_widget = XtCreateManagedWidget("box", boxWidgetClass,
				       image_shell, NULL, 0);

    normal_image_widget = XtCreateManagedWidget("normalImage", 
						labelWidgetClass,
						box_widget, NULL, 0);

    inverted_image_widget = XtCreateManagedWidget("invertedImage", 
						  labelWidgetClass,
						  box_widget, NULL, 0);
    
    n=0;
    XtSetArg(wargs[n], XtNbitmap, BWGetUnzoomedPixmap(bitmap_widget)); n++;
    XtSetValues(normal_image_widget, wargs, n);
    XtSetValues(inverted_image_widget, wargs, n);
    
    XtRealizeWidget(image_shell);

    BWNotify(bitmap_widget, (XtActionProc)FixUp);

    FixStatus();

    input_dialog = CreateDialog(top_widget, "input", Okay | Cancel);
    error_dialog = CreateDialog(top_widget, "error", Abort | Retry);    
    qsave_dialog = CreateDialog(top_widget, "qsave", Yes | No | Cancel);

    XawToggleSetCurrent(radio_group, radio_data);
    BWEngageRequest(bitmap_widget, PointRequest, True, Plain);

    XtMainLoop();
    exit(0);
}
