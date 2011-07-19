/*

Copyright 1991, 1998  The Open Group

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

#include "config.h"

#include <stdlib.h>		/* for exit() and abs() */
#include <stdio.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/Error.h>
#include "RootWin.h"
#include "Scale.h"
#include "CutPaste.h"

#define SRCWIDTH  64
#define SRCHEIGHT 64

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif



/* highlight interval (in milliseconds) */
#define HLINTERVAL  100

/* sleep between draw & erase of highlight
 * 20 milliseconds - enough for screen refresh - not too long to annoy users
 *  since we hold a server grab during this time
 */
#define HLSLEEPINTERVAL 20 /* milliseconds */

#ifdef HAVE_NANOSLEEP
#include <time.h>
#define HLSLEEP	    do { \
	struct timespec sleeptime = { 0 , HLSLEEPINTERVAL * 1000000 } ;	\
	nanosleep(&sleeptime, NULL); \
    } while(0)
#elif defined(HAVE_POLL)
#include <poll.h>
#define HLSLEEP	    poll(NULL, 0, HLSLEEPINTERVAL)
#elif defined(HAVE_SELECT)
#include <X11/Xpoll.h>
#define HLSLEEP	    do { \
	struct timeval sleeptime = { 0 , HLSLEEPINTERVAL * 1000 } ;	\
	select(0, NULL, NULL, NULL, &sleeptime); \
    } while(0)
#else
#define HLSLEEP	XSync(dpy, False)
#endif

/* highlight mode */
typedef enum { drag, resize, done } hlMode; 

/* highlight data structure */
typedef struct { 
  Boolean   newScale;
  hlMode    selectMode;
  GC        gc;
  XWindowAttributes win_info;   
  XImage     *image;
  Position  homeX, homeY, x, y;
  Dimension width, height;
  Widget    scaleShell, scaleInstance, pixShell, pixLabel, cmapWinList [2];
  } hlStruct, *hlPtr;



/* global variables */
static XtAppContext app;
static Cursor ulAngle, urAngle, lrAngle, llAngle;
static Display *dpy;
static int scr;
static GC selectGC; 
static XGCValues selectGCV;
static Widget toplevel, root;
static Atom wm_delete_window;
static int numXmags = 0;
static int srcStat, srcX, srcY;
static unsigned int srcWidth, srcHeight;

/* forward declarations */

static int Error(Display *, XErrorEvent *);
static void CloseAP(Widget, XEvent *, String *, Cardinal *);
static void SetCmapPropsAP(Widget, XEvent *, String *, Cardinal *);
static void UnsetCmapPropsAP(Widget, XEvent *, String *, Cardinal *);
static void NewAP(Widget, XEvent *, String *, Cardinal *);
static void ReplaceAP(Widget, XEvent *, String *, Cardinal *);
static void PopupPixelAP(Widget, XEvent *, String *, Cardinal *);
static void UpdatePixelAP(Widget, XEvent *, String *, Cardinal *);
static void PopdownPixelAP(Widget, XEvent *, String *, Cardinal *);
static void SelectRegionAP(Widget, XEvent *, String *, Cardinal *);
static void CheckPoints(Position *, Position *, Position *, Position *);
static void HighlightTO(XtPointer, XtIntervalId *);
static void CloseCB(Widget, XtPointer, XtPointer);
static void ReplaceCB(Widget, XtPointer, XtPointer);
static void NewCB(Widget, XtPointer, XtPointer);
static void SelectCB(Widget, XtPointer, XtPointer);
static void PasteCB(Widget, XtPointer, XtPointer);
static void SetupGC(void);
static Window FindWindow(int, int);
static void ResizeEH(Widget, XtPointer, XEvent *, Boolean *);
static void DragEH(Widget, XtPointer, XEvent *, Boolean *);
static void StartRootPtrGrab(int, hlPtr);
static void CreateRoot(void);
static void GetImageAndAttributes(Window, int, int, int, int, hlPtr);
static int Get_XColors(XWindowAttributes *, XColor **);
static Pixel GetMaxIntensity(hlPtr);
static Pixel GetMinIntensity(hlPtr);
static void PopupNewScale(hlPtr);
static void RedoOldScale(hlPtr);
static void InitCursors(void);
static void ParseSourceGeom(void);

/* application resources */

typedef struct { String geometry, source, mag, title; } OptionsRec;
static OptionsRec options;

#define Offset(field) XtOffsetOf(OptionsRec, field)
static XtResource resources[] = {
  {"geometry", "Geometry", XtRString, sizeof(String),
     Offset(geometry), XtRString, (XtPointer)NULL},
  {"mag", "Mag", XtRString, sizeof(String),
     Offset(mag), XtRString, (XtPointer)"5.0"},
  {"source", "Source", XtRString, sizeof(String),
     Offset(source), XtRString, (XtPointer)"SRCWIDTHxSRCHEIGHT"},
  {"title", XtCString, XtRString, sizeof(char *),
     Offset(title), XtRString, "xmag"},
};
#undef Offset

static XrmOptionDescRec optionDesc[] = {
  {"-bd",         "*borderColor", XrmoptionSepArg, (XtPointer)NULL},
  {"-bg",         "*background",   XrmoptionSepArg, (XtPointer)NULL},
  {"-bw",         "*borderWidth", XrmoptionSepArg, (XtPointer)NULL},
  
  {"-geometry", "*geometry", XrmoptionSepArg, (XtPointer)NULL},
  {"-mag",      "*mag",                XrmoptionSepArg, (XtPointer)NULL},
  {"-source",   "*source",             XrmoptionSepArg, (XtPointer)NULL},
  {"-title",    "*title",              XrmoptionSepArg, (XtPointer)NULL},
};



/* action table */

static XtActionsRec actions_table[] = {
  {"close", CloseAP},
  {"set-colors", SetCmapPropsAP},
  {"unset-colors", UnsetCmapPropsAP},
  {"new", NewAP},
  {"replace", ReplaceAP},
  {"popup-pixel", PopupPixelAP},
  {"update-pixel", UpdatePixelAP},
  {"popdown-pixel", PopdownPixelAP},
  {"select-region", SelectRegionAP}
};



/*
 * Error() -- Error handler:  Catch a bad match in magnifing an
 *            area that contains bits of different depths.
 */
static int 
Error(Display *dpy, XErrorEvent *err)
{
  (void) XmuPrintDefaultErrorMessage (dpy, err, stderr);
  return 0;
}


/*
 * CloseAP() -- Close this dialog.  If its the last one exit the program.
 *          
 */
static void			/* ARGSUSED */
CloseAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  Arg wargs[2]; int n; hlPtr data;
  if (!--numXmags) exit(0);
  if (event->type != ClientMessage) {
    n = 0;			/* get user data */
    XtSetArg(wargs[0], XtNuserData, &data); n++;
    XtGetValues(w, wargs, n); 
    w = data->scaleShell;
  }
  XtPopdown(w);
  XtDestroyWidget(w);
}



/*
 * SetCmapPropsAP() -- Put the scale widget first in WM_COLORMAP_WINDOWS
 *                     
 */
static void			/* ARGSUSED */
SetCmapPropsAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  Arg wargs[2]; int n; hlPtr data;
  n = 0;			/* get user data */
  XtSetArg(wargs[0], XtNuserData, &data); n++;
  XtGetValues(w, wargs, n);
  if (data->win_info.colormap != DefaultColormap(dpy, scr)) {
    data->cmapWinList[0] = data->scaleInstance;
    data->cmapWinList[1] = data->scaleShell;
    XtSetWMColormapWindows(data->scaleShell, data->cmapWinList, 2);
  }
}



/*
 * UnsetCmapPropsAP() -- Put the shell first in WM_COLORMAP_WINDOWS
 *                     
 */
static void			/* ARGSUSED */
UnsetCmapPropsAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  Arg wargs[2]; int n; hlPtr data;
  n = 0;			/* get user data */
  XtSetArg(wargs[0], XtNuserData, &data); n++;
  XtGetValues(w, wargs, n);
  if (data->win_info.colormap != DefaultColormap(dpy, scr)) {
    data->cmapWinList[0] = data->scaleShell;
    data->cmapWinList[1] = data->scaleInstance;
    XtSetWMColormapWindows(data->scaleShell, data->cmapWinList, 2);
  }
}



/*
 * NewAP() -- Create an additional xmag dialog. THIS IS A COPY OF NewEH
 *                                              FIND A BETTER WAY....
 */
static void			/* ARGSUSED */
NewAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  StartRootPtrGrab(True, NULL);
}



/*
 * ReplaceAP() -- Replace this particular xmag dialog.
 */
static void                     /* ARGSUSED */
ReplaceAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  Arg wargs[2]; int n; hlPtr data;
  n = 0;			/* get user data */
  XtSetArg(wargs[0], XtNuserData, &data); n++;
  XtGetValues(w, wargs, n); 
  StartRootPtrGrab(False, data);
}



/*
 * PopupPixelAP() -- Show pixel information.
 */
static void			/* ARGSUSED */
PopupPixelAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Position scale_x, scale_y;
    Dimension scale_height;
    Position label_x, label_y;
    Dimension label_height;
    int n;
    Arg wargs[3];
    hlPtr data;

    n = 0;			/* get user data */
    XtSetArg(wargs[0], XtNuserData, &data); n++;
    XtGetValues(w, wargs, n);

    n = 0;
    XtSetArg(wargs[n], XtNheight, &scale_height); n++;
    XtGetValues(w, wargs, n);
    XtTranslateCoords(w, -1, -1, &scale_x, &scale_y);
    
    XtRealizeWidget(data->pixShell); /* to get the right height  */

    n = 0;
    XtSetArg(wargs[n], XtNheight, &label_height); n++;
    XtGetValues(data->pixShell, wargs, n);
    
    if ((double) event->xbutton.y / (double) scale_height > 0.5) {
	label_x = scale_x;
	label_y = scale_y;
    }
    else {
	label_x = scale_x;
	label_y = scale_y + scale_height - label_height;
    }

    n = 0;
    XtSetArg(wargs[n], XtNx, label_x); n++;
    XtSetArg(wargs[n], XtNy, label_y); n++;
    XtSetValues(data->pixShell, wargs, n);

    UpdatePixelAP(w, event, NULL, NULL);
}



/*
 * UpdatePixelAP() -- Update pixel information.
 */
static void			/* ARGSUSED */
UpdatePixelAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Position x, y;
    Pixel pixel;
    XColor color;
    int n;
    Arg wargs[3];
    char string[80];
    hlPtr data;

    n = 0;
    XtSetArg(wargs[0], XtNuserData, &data); n++;
    XtGetValues(w, wargs, n);

    if (SWGetImagePixel(w, event->xbutton.x, event->xbutton.y, &x, &y, &pixel))
	XtPopdown(data->pixShell);
    else {
	color.pixel = pixel;
	XQueryColor(dpy, data->win_info.colormap, &color);
	sprintf(string, "Pixel %ld at (%d,%d) colored (%x,%x,%x).", 
		pixel, x + data->x, y + data->y,
		color.red, color.green, color.blue);
	n = 0;
	XtSetArg(wargs[n], XtNlabel, string); n++;    
	XtSetValues(data->pixLabel, wargs, n);
	XtPopup(data->pixShell, XtGrabNone);
    }
}



/*
 * PopdownPixelAP() -- Remove pixel info.
 */
static void			/* ARGSUSED */
PopdownPixelAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int n;
  Arg wargs[3];
  hlPtr data = NULL;
  
  n = 0;
  XtSetArg(wargs[0], XtNuserData, &data); n++;
  XtGetValues(w, wargs, n);

  if (data)
    XtPopdown(data->pixShell);
}



static void			/* ARGSUSED */
SelectRegionAP(Widget w, XEvent *event, String *params, Cardinal *num_params)
{    
/***** NOT SURE WHAT TO DO WITH THIS 
    if (app_resources.unmap)
	XtUnmapWidget(toplevel);
    Redisplay(XtDisplay(w), RootWindow(XtDisplay(w),
				       DefaultScreen(XtDisplay(w))),
	      source.width, source.height, 
	      app_resources.freq, app_resources.puls, 
	      ul_angle, lr_angle,
	      app_resources.grab);

    if (app_resources.unmap)
	XtMapWidget(toplevel);
******/
}



/* 
 * CheckPoints() -- Change the cursor for the correct quadrant.
 *                  Make sure the first point is less than the second 
 *                  for drawing the selection rectangle.
 *
 */
static void 
CheckPoints(Position *x1, Position *x2, Position *y1, Position *y2)
{
  Position tmp; 
  Boolean above, left;
  Cursor newC;
  above = (*y2 < *y1); left = (*x2 < *x1); 
  if (above&&left) newC = ulAngle;
  else if (above&&!left) newC = urAngle;
  else if (!above&&!left) newC = lrAngle;
  else newC = llAngle;
  XChangeActivePointerGrab
    (dpy, PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
     newC, CurrentTime);
  if (*x2 < *x1) { tmp = *x1; *x1 = *x2; *x2 = tmp; }
  if (*y2 < *y1) { tmp = *y1; *y1 = *y2; *y2 = tmp; }
}



/*
 * HighlightTO() -- Timer to highlight the selection box
 */
static void
HighlightTO(XtPointer closure, XtIntervalId *id)	/* ARGSUSED */
{
  hlPtr data = (hlPtr)closure;
  XGrabServer(dpy);
  if (data->selectMode == drag) {
    XDrawRectangle(dpy, DefaultRootWindow(dpy), data->gc, 
		   data->x, data->y, data->width, data->height);
    XFlush(dpy);
    HLSLEEP;
    XDrawRectangle(dpy, DefaultRootWindow(dpy), data->gc, 
		   data->x, data->y, data->width, data->height);
  }
  else if (data->selectMode == resize) {	
    Position x1 = data->homeX,
             x2 = data->x,
             y1 = data->homeY,
             y2 = data->y;
    CheckPoints(&x1, &x2, &y1, &y2);
    XDrawRectangle(dpy, DefaultRootWindow(dpy), data->gc, 
		   x1, y1, x2 - x1, y2 - y1);
    XFlush(dpy);
    HLSLEEP;
    XDrawRectangle(dpy, DefaultRootWindow(dpy), data->gc, 
		   x1, y1, x2 - x1, y2 - y1);
  }
  XUngrabServer(dpy);
  if (data->selectMode != done)
    XtAppAddTimeOut(app, HLINTERVAL, HighlightTO, (XtPointer)data);
}



/*
 * CloseCB() -- Delete this xmag dialog.  If its the only one on the screen
 *             then exit.
 */
static void			/* ARGSUSED */
CloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
  Widget shell = (Widget)clientData;
  if (!--numXmags) exit(0);
  XtPopdown(shell);
  XtDestroyWidget(shell);
}



/*
 * ReplaceCB() -- Replace this particular xmag dialog.
 */
static void                     /* ARGSUSED */
ReplaceCB(Widget w, XtPointer clientData, XtPointer callData)
{
  hlPtr data = (hlPtr)clientData;
  StartRootPtrGrab(False, data);
}



/*
 * NewCB() -- Create an additional xmag dialog.
 */
static void			/* ARGSUSED */
NewCB(Widget w, XtPointer clientData, XtPointer callData)
{
  StartRootPtrGrab(True, NULL);
}



/*
 * SelectCB() -- Own the primary selection.
 */
static void			/* ARGSUSED */
SelectCB(Widget w, XtPointer clientData, XtPointer callData)
{
  hlPtr data = (hlPtr)clientData;
  SWGrabSelection(data->scaleInstance, XtLastTimestampProcessed(dpy));
}



/*
 * PasteCB() -- Paste from the primary selectin into xmag.
 */
static void			/* ARGSUSED */
PasteCB(Widget w, XtPointer clientData, XtPointer callData)
{
  hlPtr data = (hlPtr)clientData;
  SWRequestSelection(data->scaleInstance, XtLastTimestampProcessed(dpy));
}



/*
 * SetupGC() -- Graphics context for magnification selection.
 */
static void 
SetupGC(void)
{
    selectGCV.function = GXxor;
    selectGCV.foreground = 0xffffffff;
    selectGCV.subwindow_mode = IncludeInferiors;
    selectGC = XtGetGC(toplevel, GCFunction|GCForeground|GCSubwindowMode,
		       &selectGCV);
}  



/*
 * FindWindow() -- Determin window the pointer is over.
 *
 */
static Window 
FindWindow(int x, int y)	/* Locatation of cursor */
{
  XWindowAttributes wa;
  Window findW = DefaultRootWindow(dpy), stopW, childW;

  /* Setup for first window find */
  stopW = findW;

  while (stopW) {
    XTranslateCoordinates(dpy, findW, stopW, 
			  x, y, &x, &y, &childW);
    findW = stopW;
    /* If child is not InputOutput (for example, InputOnly) */
    /* then don't continue, return the present findW which */
    /* can be the root, or a root child of class InputOutput */
    if (childW &&
	XGetWindowAttributes(dpy, childW, &wa) &&
	wa.class != InputOutput)
	break;
    stopW = childW;
  }
  return findW;
}



/*
 * ResizeEH() -- Event Handler for resize of selection box.
 */
static void 
ResizeEH(Widget w, XtPointer closure, XEvent *event, 
	 Boolean *continue_to_dispatch)	/* ARGSUSED */
{
  hlPtr data = (hlPtr)closure;
  switch (event->type) {
  case MotionNotify:
    data->x = event->xmotion.x_root;
    data->y = event->xmotion.y_root; 
    break;
  case ButtonRelease:
    GetImageAndAttributes(FindWindow(event->xmotion.x_root,
			event->xmotion.y_root),
	     min(data->homeX,event->xbutton.x_root),
	     min(data->homeY,event->xbutton.y_root),
	     abs(data->homeX - event->xbutton.x_root),
	     abs(data->homeY - event->xbutton.y_root),
	     data);
    if (data->newScale)
      PopupNewScale(data);
    else 
      SWSetImage(data->scaleInstance, data->image);
    XtUngrabPointer(w, CurrentTime);
/*****
    XtRemoveRawEventHandler(w, PointerMotionMask|ButtonReleaseMask,
			 True, ResizeEH, (XtPointer)data);
*****/
    XtRemoveEventHandler(w, PointerMotionMask|ButtonReleaseMask,
			 True, ResizeEH, (XtPointer)data);
    data->selectMode = done;
    break;
  }
}



/*
 * DragEH() -- Event Handler for draging selection box.
 */
static void 
DragEH(Widget w, XtPointer closure, XEvent *event, 
       Boolean *continue_to_dispatch) /* ARGSUSED */
{
  hlPtr data = (hlPtr)closure;
  switch (event->type) {
  case MotionNotify:		/* drag mode */
    data->x = event->xmotion.x_root;
    data->y = event->xmotion.y_root;
    break;
  case ButtonRelease:		/* end drag mode */
    if (event->xbutton.button == Button1) { /* get image */
      /* Problem: You can't get bits with XGetImage outside of its window.
       *          xmag will only do a GetImage on the actual window in the case
       *          where the depth of the window does not match the depth of
       *          the root window.
       */
      GetImageAndAttributes(FindWindow(event->xmotion.x_root, 
			  event->xmotion.y_root),
	       event->xbutton.x_root, 
	       event->xbutton.y_root,
	       srcWidth, srcHeight, data);
      if (data->newScale)
	PopupNewScale(data);
      else
	RedoOldScale(data);
      XtUngrabPointer(w, CurrentTime);
      XtRemoveRawEventHandler(w, PointerMotionMask|ButtonPressMask|
			      ButtonReleaseMask, True, DragEH,
			      (XtPointer)data);
      data->selectMode = done;
    }

    break;
  case ButtonPress:	
    if (event->xbutton.button == Button2) {	/* turn on resize mode */
      data->homeX = event->xbutton.x_root; 
      data->homeY = event->xbutton.y_root;
      data->x = event->xbutton.x_root + srcWidth;
      data->y = event->xbutton.y_root + srcHeight;      
      data->selectMode = resize;
      XtRemoveRawEventHandler(w, PointerMotionMask|ButtonPressMask|
			   ButtonReleaseMask, True, DragEH, (XtPointer)data);
      XChangeActivePointerGrab
	(dpy, PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
	 lrAngle, CurrentTime);
      XWarpPointer(dpy, None, None, 0, 0, 0, 0, 
		   srcWidth, srcHeight);
      XtAddEventHandler(w, PointerMotionMask|ButtonReleaseMask, 
			True, ResizeEH, (XtPointer)data);
    }
    break;
  }
}




/*
 * StartRootPtrGrab() -- Bring up the selection box.
 *              
 */
static void
StartRootPtrGrab(int new, 	/* do we cretate a new scale instance? */
		 hlPtr data)	/* highligh data */
{
  Window    rootR, childR;
  int       rootX, rootY, winX, winY;
  unsigned  int mask;
  hlPtr hlData;
  XtGrabPointer
    (root, False,
     PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
     GrabModeAsync, GrabModeAsync, None, ulAngle, CurrentTime);
  XQueryPointer(dpy, DefaultRootWindow(dpy), &rootR, &childR, 
		&rootX, &rootY, &winX, &winY, &mask);
  if (new) {
    numXmags++;
    hlData = (hlPtr)XtMalloc(sizeof(hlStruct));
  }
  else hlData = data;
  hlData->newScale   = new;
  hlData->selectMode = drag;
  hlData->x          = rootX;
  hlData->y          = rootY;
  hlData->gc         = selectGC;
  hlData->width      = srcWidth;
  hlData->height     = srcHeight;
  XtAddRawEventHandler
    (root, PointerMotionMask|ButtonPressMask|ButtonReleaseMask, 
     True, DragEH, (XtPointer)hlData);
  (void) XtAppAddTimeOut(app, HLINTERVAL, HighlightTO, (XtPointer)hlData);
}



/*
 * CreateRoot() -- Create a root window widget. If the user specified x and y
 *                 in his source geometry then use this to directly get the
 *                 image.
 */
static void
CreateRoot(void)
{
  hlPtr data;
  root = XtCreateWidget("root", rootWindowWidgetClass, toplevel, NULL, 0);
  XtRealizeWidget(root);
  if (XValue & srcStat && YValue &srcStat) { 
    numXmags = 1;
    data = (hlPtr)XtMalloc(sizeof(hlStruct));
    data = data;
    data->newScale   = True;
    data->selectMode = drag;
    data->x          = srcX;
    data->y          = srcY;
    data->gc         = selectGC;
    data->width      = srcWidth;
    data->height     = srcHeight;
    GetImageAndAttributes(RootWindow(dpy, scr), srcX, srcY, srcWidth, 
			  srcHeight, data);
    PopupNewScale(data);
    return;
  }
}


/* 
 * GetImageAndAttributes() -- Get the image bits from the screen.
 *               We will also determin here the colormap, depth, and
 *               visual to be used for the magnification image.  
 */
static void 
GetImageAndAttributes(Window w, int x, int y, int width, int height, 
		      hlPtr data)
{
    /* get parameters of window being magnified */
    XGetWindowAttributes(dpy, w, &data->win_info);

    if (data->win_info.depth == DefaultDepth(dpy, scr)) {
	/* avoid off screen pixels */
	if (x < 0)
	    x = 0;
	if (y < 0)
	    y = 0;
	if (x + width > DisplayWidth(dpy,scr))
	    x = DisplayWidth(dpy,scr) - width;
	if (y + height > DisplayHeight(dpy,scr))
	    y = DisplayHeight(dpy,scr) - height;
	data->x = x; data->y = y;
	/* get image pixels */
	data->image = XGetImage (dpy,
				 RootWindow(dpy, scr),
				 x, y,
				 width, height,
				 AllPlanes, ZPixmap);
    }
    else {
	int	t0, t1;
	int	x0, x1, y0, y1;
	int	xInWin, yInWin;
	Window	childWin;

	XTranslateCoordinates(dpy, DefaultRootWindow(dpy), w, x, y,
			      &xInWin, &yInWin, &childWin);

	/* Avoid off screen pixels. Assume this routine is not
	 * called for totally offscreen windows. */
	x0 = max(x, 0);
	y0 = max(y, 0);
	x1 = min(DisplayWidth(dpy, scr),
		 min(x0 + width, x0 + (data->win_info.width - xInWin)));
	y1 = min(DisplayHeight(dpy, scr),
		 min(y0 + height, y0 + (data->win_info.height - yInWin)));

	/* Try to use up to width x height pixels */
	if (x1 - x0 < width) {
	    t0 = x0;
	    t1 = max(0, x - xInWin + data->win_info.width -
		     DisplayWidth(dpy, scr));
	    x0 = max(0, x1 - min(width, data->win_info.width - t1));
	    xInWin -= t0 - x0;
	}
	if (y1 - y0 < height) {
	    t0 = y0;
	    t1 = max(0, y - yInWin + data->win_info.height -
		     DisplayHeight(dpy, scr));
	    y0 = max(0, y1 - min(height, data->win_info.height - t1));
	    yInWin -= t0 - y0;
	}

	data->x = x0;
	data->y = y0;
	data->width = x1 - x0;
	data->height = y1 - y0;

	data->image = XGetImage (dpy,
				 w,
				 xInWin, yInWin,
				 data->width, data->height,
				 AllPlanes, ZPixmap);

    }
}



/*
 * Get_XColors() Get the XColors of all pixels in image - returns # of colors
 *               This function was taken from xwd (thanks Bob...)
 */
#define lowbit(x) ((x) & (~(x) + 1))
static int 
Get_XColors(XWindowAttributes *win_info, XColor **colors)
{
    int i, ncolors;
 
    if (!win_info->colormap)
        return(0);
 
    ncolors = win_info->visual->map_entries;
    if (!(*colors = (XColor *) XtMalloc (sizeof(XColor) * ncolors)))
      XtError("Out of memory!");
 
    if (win_info->visual->class == DirectColor ||
        win_info->visual->class == TrueColor) {
        Pixel red, green, blue, red1, green1, blue1;
 
        red = green = blue = 0;
        red1 = lowbit(win_info->visual->red_mask);
        green1 = lowbit(win_info->visual->green_mask);
        blue1 = lowbit(win_info->visual->blue_mask);
        for (i=0; i<ncolors; i++) {
          (*colors)[i].pixel = red|green|blue;
          (*colors)[i].pad = 0;
          red += red1;
          if (red > win_info->visual->red_mask)
            red = 0;
          green += green1;
          if (green > win_info->visual->green_mask)
            green = 0;
          blue += blue1;
          if (blue > win_info->visual->blue_mask)
            blue = 0;
        }
    } else {
        for (i=0; i<ncolors; i++) {
          (*colors)[i].pixel = i;
          (*colors)[i].pad = 0;
        }
    }
 
    XQueryColors(dpy, win_info->colormap, *colors, ncolors);
 
    return(ncolors);
}



#define Intensity(cptr) (3.0*cptr->red+0.59*cptr->green+0.11*cptr->blue)

/*
 * GetMaxIntensity() -- Find the maximum intensity pixel value for a colormap.
 */
static Pixel
GetMaxIntensity(hlPtr data)
{
  XColor *colors = NULL, *mptr, *tptr;
  int i, ncolors;

  if (data->win_info.colormap == DefaultColormap(dpy, scr)) 
    return WhitePixel(dpy, scr);
  ncolors = Get_XColors(&data->win_info, &colors); 
  mptr = tptr = colors; tptr++;
  for (i=1; i<ncolors; i++) {
    if ((int)Intensity(mptr) < (int)Intensity(tptr)) 
      mptr = tptr;
    tptr++;
  }
  /* Null pointer protection */
  if(mptr)
    return mptr->pixel;
  else
    return WhitePixel(dpy, scr);
}

/*
 * GetMinIntensity() -- Find the minimum intensity pixel value for a colormap.
 */
static Pixel
GetMinIntensity(hlPtr data)
{
  XColor *colors = NULL, *mptr, *tptr;
  int i, ncolors;

  if (data->win_info.colormap == DefaultColormap(dpy, scr)) 
    return BlackPixel(dpy, scr);
  ncolors = Get_XColors(&data->win_info, &colors); 
  mptr = tptr = colors; tptr++;
  for (i=1; i<ncolors; i++)  {
    if ((int)Intensity(mptr) > (int)Intensity(tptr))
      mptr = tptr; 
    tptr++;
  }
  /* Null pointer protection */
  if(mptr)
    return mptr->pixel;
  else
    return BlackPixel(dpy, scr);
}




static Widget pane1, pane2, pane3, cclose, replace, new, select_w, paste;

/*
 * PopupNewScale() -- Create and popup a new scale composite.
 */
static void		
PopupNewScale(hlPtr data)
{
  Arg warg;

  data->scaleShell = 
    XtVaCreatePopupShell("xmag", topLevelShellWidgetClass, toplevel, 
			 XtNgeometry, (XtArgVal)options.geometry,
			 XtNtitle, (XtArgVal)options.title,
			 NULL);
  pane1 = XtCreateManagedWidget("pane1", panedWidgetClass, data->scaleShell,
				(Arg *) NULL, 0);
  pane2 = XtCreateManagedWidget("pane2", panedWidgetClass, pane1,
				(Arg *) NULL, 0);
  cclose = XtCreateManagedWidget("close", commandWidgetClass, pane2,
				 (Arg *) NULL, 0);
  XtAddCallback(cclose, XtNcallback, CloseCB, (XtPointer)data->scaleShell);
  replace = XtCreateManagedWidget("replace", commandWidgetClass, pane2,
				  (Arg *) NULL, 0);
  XtAddCallback(replace, XtNcallback, ReplaceCB, (XtPointer)data);
  new = XtCreateManagedWidget("new", commandWidgetClass, pane2,
			      (Arg *) NULL, 0);
  XtAddCallback(new, XtNcallback, NewCB, (XtPointer)NULL);
  select_w = XtCreateManagedWidget("select", commandWidgetClass, pane2,
			      (Arg *) NULL, 0);
  XtAddCallback(select_w, XtNcallback, SelectCB, (XtPointer)data);
  paste = XtCreateManagedWidget("paste", commandWidgetClass, pane2,
			      (Arg *) NULL, 0);
  XtAddCallback(paste, XtNcallback, PasteCB, (XtPointer)data);
  (void) XtCreateManagedWidget("helpLabel", labelWidgetClass, pane2,
			       (Arg *) NULL, 0);
  pane3 = XtCreateManagedWidget("pane2", panedWidgetClass, pane1,
				(Arg *) NULL, 0);
  data->scaleInstance = 
    XtVaCreateManagedWidget("scale", scaleWidgetClass, 
			    pane3,
			    XtNvisual, (XtArgVal)data->win_info.visual,
			    XtNcolormap, (XtArgVal)data->win_info.colormap,
			    XtNdepth, (XtArgVal)data->win_info.depth,
			    XtNscaleX, (XtArgVal)options.mag,
			    XtNscaleY, (XtArgVal)options.mag,
			    NULL);
  SWSetImage(data->scaleInstance, data->image);
  XtOverrideTranslations
    (data->scaleShell,
     XtParseTranslationTable ("<Message>WM_PROTOCOLS: close()"));
  XtSetArg(warg, XtNuserData, data);
  XtSetValues(data->scaleInstance, &warg, 1);
  data->pixShell = 
    XtVaCreatePopupShell("pixShell", overrideShellWidgetClass, 
			 toplevel,
			 XtNvisual, (XtArgVal)data->win_info.visual,
			 XtNcolormap, (XtArgVal)data->win_info.colormap,
			 XtNdepth, (XtArgVal)data->win_info.depth,
			 XtNborderWidth, (XtPointer)0,
			 NULL);
  data->pixLabel = 
    XtVaCreateManagedWidget("pixLabel", labelWidgetClass, 
			    data->pixShell, 
			    XtNforeground, (XtPointer)GetMaxIntensity(data),
			    XtNbackground, (XtPointer)GetMinIntensity(data),
			    XtNborderWidth, (XtPointer)0,
			    NULL);
  XtInstallAllAccelerators(pane1, pane1);	/* install accelerators */
  if (data->newScale) {
    XtPopup(data->scaleShell, XtGrabNone);
    (void) XSetWMProtocols	/* ICCCM delete window */
      (dpy, XtWindow(data->scaleShell), &wm_delete_window, 1);
  }
  if (data->win_info.colormap != DefaultColormap(dpy, scr)) {
    data->cmapWinList[0] = data->scaleShell; 
    data->cmapWinList[1] = data->scaleInstance;
    XtSetWMColormapWindows(data->scaleShell, data->cmapWinList, 2);
  }
}



/*
 * RedoOldScale() -- If the visual, depth, or colormap has changed, unrealize
 *                   the scale widget and change its colormap/depth/visual.
 *                   Then re-realize it.  Also do this for the pixel display
 *                   widget.
 */
static void
RedoOldScale(hlPtr data)
{
  Arg wargs[3];
  int n;
  Visual *oldVis;
  int oldDepth;
  Colormap oldCmap;

  n=0;
  XtSetArg(wargs[n], XtNvisual, &oldVis); n++;
  XtSetArg(wargs[n], XtNdepth, &oldDepth); n++;
  XtSetArg(wargs[n], XtNcolormap, &oldCmap); n++;
  XtGetValues(data->scaleInstance, wargs, n);  
  if (oldVis == data->win_info.visual && oldDepth == data->win_info.depth
      && oldCmap == data->win_info.colormap) {
    SWSetImage(data->scaleInstance, data->image);    
    return;
  }
  /* get width and height, save and reuse them */
  XtUnmanageChild(data->scaleInstance);
  XtUnrealizeWidget(data->scaleInstance);
  n=0;
  XtSetArg(wargs[n], XtNcolormap, data->win_info.colormap); n++;
  XtSetArg(wargs[n], XtNdepth, data->win_info.depth); n++;
  XtSetArg(wargs[n], XtNvisual, data->win_info.visual); n++;
  XtSetValues(data->scaleInstance, wargs, n);
  n=0;
  XtSetArg(wargs[n], XtNforeground, GetMaxIntensity(data)); n++;
  XtSetArg(wargs[n], XtNbackground, GetMinIntensity(data)); n++;
  XtSetValues(data->pixLabel, wargs, n);
  SWSetImage(data->scaleInstance, data->image); 
  XtRealizeWidget(data->scaleInstance);
  XtManageChild(data->scaleInstance);
}



/*
 * InitCursors() -- Create our cursors for area selection.
 */
static void
InitCursors(void)
{
  ulAngle = XCreateFontCursor(dpy, XC_ul_angle);
  urAngle = XCreateFontCursor(dpy, XC_ur_angle);
  lrAngle = XCreateFontCursor(dpy, XC_lr_angle);
  llAngle = XCreateFontCursor(dpy, XC_ll_angle);
}



/*
 * ParseSourceGeom() -- Determin dimensions of area to magnify from resources.
 */
static void 
ParseSourceGeom(void)
{
				/* source */
  srcStat = 
    XParseGeometry(options.source, &srcX, &srcY, &srcWidth, &srcHeight);
  if (!srcWidth) srcWidth = SRCWIDTH;
  if (!srcHeight) srcHeight = SRCHEIGHT;
  if (XNegative & srcStat) srcX = DisplayWidth(dpy, scr) + srcX - srcWidth;
  if (YNegative & srcStat) srcY = DisplayHeight(dpy, scr) + srcY - srcHeight;
				/* mag */
}



/*
 * Main program.
 */
int 
main(int argc, char *argv[])
{
  XSetErrorHandler(Error);
    
				/* SUPPRESS 594 */
  toplevel = XtAppInitialize(&app, "Xmag", optionDesc, XtNumber(optionDesc),
			     &argc, argv, NULL,
			     NULL, 0);

  dpy = XtDisplay(toplevel);
  scr = DefaultScreen(dpy);
  XtGetApplicationResources(toplevel, (XtPointer) &options, resources,
			    XtNumber(resources), NULL, 0);
  if (argc != 1) {
    fprintf (stderr,
	    "usage:  xmag [-source geom] [-mag magfactor] [-toolkitoption]\n");
    exit(1);
  }
  

  ParseSourceGeom();
  XtAppAddActions(app, actions_table, XtNumber(actions_table));
  InitCursors();
  SetupGC();
  CreateRoot();
  if (!(XValue & srcStat && YValue & srcStat))
    StartRootPtrGrab(True, (hlPtr)NULL);
  wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XtAppMainLoop(app);
  exit(0);
}
