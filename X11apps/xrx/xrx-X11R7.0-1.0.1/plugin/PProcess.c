/* $Xorg: PProcess.c,v 1.6 2001/02/09 02:05:57 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/
/* $XFree86: xc/programs/xrx/plugin/PProcess.c,v 1.6tsi Exp $ */

#include "RxPlugin.h"
#include "XUrls.h"
#include "XAuth.h"
#include "XDpyName.h"
#include "Prefs.h"
#include <X11/StringDefs.h>

#include <limits.h>		/* for MAXHOSTNAMELEN */
/* and in case we didn't get it from the headers above */
#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN 256
#endif

#ifdef XUSE_XTREGISTERWINDOW
extern void _XtRegisterWindow (Window window, Widget widget);
#define XtRegisterDrawable(d,win,wid) _XtRegisterWindow(win,wid)
extern void _XtUnregisterWindow (Window window, Widget widget);
#define UnregisterDrawable(win,wid) _XtUnregisterWindow(win,wid)
#else
#define UnregisterDrawable(win,wid) XtUnregisterDrawable(XtDisplay(wid),win)
#endif

/* timeout for authorizations */
#define DEFAULT_TIMEOUT 300
#define NO_TIMEOUT 0

/***********************************************************************
 * Try and do something sensible about window geometry
 ***********************************************************************/
static void
GetWindowGeometry(
    Display* dpy,
    Window win,
    Position* x,
    Position* y,
    Dimension* width,
    Dimension* height,
    Dimension* border_width,
    /* the following doesn't really belong here but it saves us from doing
       another XGetWindowAttributes later on */
    Colormap *cmap)
{
    long mask;
    XSizeHints* sizehints = XAllocSizeHints();
    XWindowAttributes wattr;

    if (XGetWindowAttributes (dpy, win, &wattr)) {
	*x = wattr.x;
	*y = wattr.y;
	*width = wattr.width;
	*height = wattr.height;
	*border_width = wattr.border_width;
	*cmap = wattr.colormap;
    }
    if (sizehints) {
	XGetWMNormalHints (dpy, win, sizehints, &mask);

	if (mask & (USPosition|PPosition)) {
	    *x = sizehints->x;
	    *y = sizehints->y;
	    *width = sizehints->width;
	    *height = sizehints->height;
	    XFree ((char*) sizehints);
	    return;
	}
	XFree ((char*) sizehints);
    }
    *x = 0;
    *y = 0;
    *width = 0;
    *height = 0;
}

/***********************************************************************
 * a set of utility functions to manipulate Windows lists
 ***********************************************************************/
static Bool
IsInWinList(Window *list, int count, Window win)
{
    int i;
    for (i = 0; i < count; i++, list++)
	if (*list == win)
	    return True;
    return False;
}

#ifdef UNUSED
static void
AppendToWinList(Window **new_list, int *new_count,
		Window *list, int count, Window win)
{
    *new_count = count + 1;
    *new_list = (Window*) malloc(sizeof(Window) * *new_count);
    memcpy(*new_list, list, sizeof(Window) * count);
    (*new_list)[count] = win;
}
#endif

static void
PrependToWinList(Window **new_list, int *new_count,
		 Window *list, int count, Window win)
{
    *new_count = count + 1;
    *new_list = (Window*) malloc(sizeof(Window) * *new_count);
    (*new_list)[0] = win;
    memcpy(*new_list + 1, list, sizeof(Window) * count);
}

/* rotate the list so the given window is at the beginning of it */
static void
SetFirstWinList(Window *list, int count, Window win)
{
    int i;

    /* look for the given window starting from the end */
    list += count - 1;
    for (i = 0; i < count; i++, list--)
	if (*list == win)
	    break;
    if (i < count) {		/* when we found it rotate from there */
	/* shift every element to the right */
	for (i++; i < count; i++) {
	    list--;
	    list[1] = list[0];
	}
	/* and set the first one */
	*list = win;
    }
}

/* rotate the list so the given window which should be the first item is
   at the end of it */
static void
SetLastWinList(Window *list, int count, Window win)
{
    if (*list == win) {
	int i;
	/* shift every element to the left */
	for (i = 0; i < count - 1; i++, list++)
	    list[0] = list[1];
	/* and set the last one */
	*list = win;
    }
}

static void
RemoveFromWinList(Window **wlist, int *count, Window win)
{
    Window *list = *wlist;
    int i;
    /* look for the window to remove */
    for (i = 0; i < *count; i++, list++)
        if (*list == win) {
            (*count)--;
            break;
        }
    /* then simply shift following elements to the left */
    for (; i < *count; i++, list++)
        list[0] = list[1];
}

#ifdef UNUSED
static void
ConcatWinLists(Window **list, int *count,
	       Window *list1, int count1,
	       Window *list2, int count2)
{
    *count = count1 + count2;
    *list = (Window*) malloc(sizeof(Window) * *count);
    memcpy(*list, list1, sizeof(Window) * count1);
    memcpy(*list + count1, list2, sizeof(Window) * count2);
}

static void
SubstractWinLists(Window **wlist, int *count,
		  Window *list1, int count1)
{
    Window *list = *wlist;
    int i, j;
    /* look for the beginning of the list to remove */
    for (i = 0; i < *count; i++, list++)
	if (*list == *list1)
	    break;
    /* skip the list to remove stopping at the end of one of the lists
       or at the first alien element */
    for (j = 0; j < count1 && i + j < *count; j++, list1++)
	if (list[j] != *list1)
	    break;
    /* then shift following elements */
    *count -= j;
    for (; i < *count; i++, list++)
	list[0] = list[j];
}
#endif

/***********************************************************************
 * Add window to the WM_COLORMAP_WINDOWS property on the Netscape
 * toplevel widget if necessary
 ***********************************************************************/
static void
SetWMColormap(PluginInstance* This, Window win)
{
    int i;
    Colormap top_cmap;
    Arg arg;

    /* get window's record */
    for (i = 0; i < This->nclient_windows; i++)
	if ((This->client_windows[i].win = win))
	    break;

    if (i == This->nclient_windows)
	return;

    /* if window's colormap is different from toplevel's one set property */
    XtSetArg(arg, XtNcolormap, &top_cmap);
    XtGetValues(This->toplevel_widget, &arg, 1);
    if (This->client_windows[i].colormap != top_cmap) {
	Window *cur_list;
	int cur_count = 0;

	/* if there is already a non empty list we need to update it */
	if (XGetWMColormapWindows(RxGlobal.dpy, XtWindow(This->toplevel_widget),
				  &cur_list, &cur_count) == True &&
	    cur_count != 0) {

	    if (IsInWinList(cur_list, cur_count, win)) {
		/* window is already in the list just move it in first place */
		SetFirstWinList(cur_list, cur_count, win);
		XSetWMColormapWindows(RxGlobal.dpy,
				      XtWindow(This->toplevel_widget),
				      cur_list, cur_count);
	    } else {
		/* window is not in the list add it in first place */
		Window *new_list;
		int new_count;

		PrependToWinList(&new_list, &new_count,
				 cur_list, cur_count, win);
		XSetWMColormapWindows(RxGlobal.dpy,
				      XtWindow(This->toplevel_widget),
				      new_list, new_count);
		free(new_list);
	    }
	} else {		/* no list yet so lets make one */
	    Window list[2];

	    list[0] = win;
	    list[1] = XtWindow(This->toplevel_widget);
	    XSetWMColormapWindows(RxGlobal.dpy, XtWindow(This->toplevel_widget),
				  list, 2);
	}
	if (cur_count != 0)
	    XFree(cur_list);
    }
}

/***********************************************************************
 * Move window at the end of the WM_COLORMAP_WINDOWS property list on
 * the Netscape toplevel widget
 ***********************************************************************/
static void
UnsetWMColormap(PluginInstance* This, Window win)
{
    Window *list;
    int count = 0;

    if (XGetWMColormapWindows(RxGlobal.dpy, XtWindow(This->toplevel_widget),
			      &list, &count) == True && count != 0) {
	SetLastWinList(list, count, win);
	XSetWMColormapWindows(RxGlobal.dpy, XtWindow(This->toplevel_widget),
			      list, count);
    }
    if (count != 0)
	XFree(list);
}

/***********************************************************************
 * Remove window from the WM_COLORMAP_WINDOWS property on the Netscape
 * toplevel widget
 ***********************************************************************/
static void
ResetWMColormap(PluginInstance* This, Window win)
{
    Window *list;
    int count = 0;

    if (XGetWMColormapWindows(RxGlobal.dpy, XtWindow(This->toplevel_widget),
			      &list, &count) == True && count != 0) {
	RemoveFromWinList(&list, &count, win);

	if (count > 1)
	    XSetWMColormapWindows(RxGlobal.dpy, XtWindow(This->toplevel_widget),
				  list, count);
	else {			/* remove list when it becomes useless */
	    Atom prop;

	    prop = XInternAtom (RxGlobal.dpy, "WM_COLORMAP_WINDOWS", False);
	    XDeleteProperty(RxGlobal.dpy, XtWindow(This->toplevel_widget), prop);
	}
    }
    if (count != 0)
	XFree(list);
}

/***********************************************************************
 * Event Handler to reparent client window under plugin window 
 ***********************************************************************/
/* static */ void
SubstructureRedirectHandler (
    Widget widget, 
    XtPointer client_data, 
    XEvent* event, 
    Boolean* cont)
{
    windowrec* new_list;
    PluginInstance* This = (PluginInstance*) client_data;

#ifdef PLUGIN_TRACE
    fprintf (stderr, "%s\n", "SubstructureRedirectHandler");
    fprintf (stderr, "This: 0x%x\n", This);
#endif

    switch (event->type) {
    case ConfigureRequest:
	{
	    XWindowChanges config;
	    config.x = event->xconfigurerequest.x;
	    config.y = event->xconfigurerequest.y;
	    config.width = event->xconfigurerequest.width;
	    config.height = event->xconfigurerequest.height;
	    config.border_width = event->xconfigurerequest.border_width;
	    config.sibling = event->xconfigurerequest.above;
	    config.stack_mode = event->xconfigurerequest.detail;
#if 0
	    fprintf (stderr, "configuring at %dx%d+%d+%d\n", 
		     config.width, config.height, config.x, config.y);
#endif
	    XConfigureWindow (RxGlobal.dpy,
			      event->xconfigurerequest.window,
			      event->xconfigurerequest.value_mask,
			      &config);
	}
	break;

    case MapRequest:

	RxpSetStatusWidget(This, RUNNING);

	{
	    Window for_win;
	    int i;

	    if (XGetTransientForHint (RxGlobal.dpy, event->xmaprequest.window,
				      &for_win)) {
		for (i = 0; i < This->nclient_windows; i++)
		    if (for_win == This->client_windows[i].win)
			XMapWindow (RxGlobal.dpy, event->xmaprequest.window);
		return;
	    }
	}
	new_list = (windowrec*) 
	    NPN_MemAlloc (sizeof (windowrec) * (This->nclient_windows + 1));
	if (new_list) {
	    Position x, y;
	    Dimension width, height;
	    Dimension border_width;
	    Colormap cmap;
	    int n;
	    Atom* wm_proto;
	    windowrec* wp;
	    Window destwin = XtWindow (This->plugin_widget);

	    This->nclient_windows++;
	    if (This->nclient_windows > 1)
		memcpy ((void*) new_list, (void*) This->client_windows,
			(This->nclient_windows - 1) * sizeof (windowrec));
	    if (This->client_windows)
		NPN_MemFree (This->client_windows);
	    This->client_windows = new_list;

	    x = y = 0;
	    width = height = border_width = 0;
	    GetWindowGeometry (RxGlobal.dpy, event->xmaprequest.window, 
			       &x, &y, &width, &height, &border_width, &cmap);

	    wp = &This->client_windows[This->nclient_windows - 1];
	    wp->win = event->xmaprequest.window;
	    wp->x = x; wp->y = y;
	    wp->width = width; wp->height = height;
	    wp->border_width = border_width;
	    wp->flags = RxpMapped;
	    wp->colormap = cmap;

	    if (XGetWMProtocols (RxGlobal.dpy, wp->win, &wm_proto, &n)) {
		int i;
		Atom* ap;

		for (i = 0, ap = wm_proto; i < n; i++, ap++) {
		    if (*ap == RxGlobal.wm_delete_window)
			wp->flags |= RxpWmDelWin;
		}
		if (wm_proto) XFree ((char*) wm_proto);
	    }

	    XSelectInput(RxGlobal.dpy, wp->win,
			 EnterWindowMask | LeaveWindowMask);
	    XtRegisterDrawable (RxGlobal.dpy, wp->win, This->plugin_widget);
	    XReparentWindow (RxGlobal.dpy, wp->win, destwin, wp->x, wp->y);
	    XMapWindow (RxGlobal.dpy, wp->win);
	}
	break;
    }
}

/***********************************************************************
 * Event Handler to forward WM_DELETE_WINDOW events to the client windows
 ***********************************************************************/
void
RxpWmDelWinHandler (
    Widget widget,
    XtPointer client_data,
    XEvent* event,
    Boolean* cont)
{
    PluginInstance* This = (PluginInstance*) client_data;
    int i;

    if (event == NULL ||
	(event->type == ClientMessage &&
	 event->xclient.message_type == RxGlobal.wm_protocols &&
	 event->xclient.data.l[0] == RxGlobal.wm_delete_window)) {
	for (i = 0; i < This->nclient_windows; i++) {
	    if (This->client_windows[i].flags & RxpWmDelWin) {
		XClientMessageEvent ev;

		ev.type = ClientMessage;
		ev.window = This->client_windows[i].win;
		ev.message_type = RxGlobal.wm_protocols;
		ev.format = 32;
		ev.data.l[0] = RxGlobal.wm_delete_window;
		ev.data.l[1] = XtLastTimestampProcessed (XtDisplay (widget));
		XSendEvent (RxGlobal.dpy, ev.window, FALSE, 0L, (XEvent*) &ev);
	    }
	}
    }
}

/***********************************************************************
 * Event Handler to forward ConfigureNotify events to the client windows
 ***********************************************************************/
static void
StructureNotifyHandler (
    Widget widget, 
    XtPointer client_data, 
    XEvent* event, 
    Boolean* cont)
{
    PluginInstance* This = (PluginInstance*) client_data;

#ifdef PLUGIN_TRACE
    fprintf (stderr, "%s\n", "StructureNotifyHandler");
#endif

    switch (event->type) {

    /*
     * For the testplugin, which uses a ScrolledWindow, the clipped
     * window, i.e. This->plugin, is "configured" when the user pans
     * around the ScrolledWindow. The Netscape scrolled-window is
     * different. It moves-and-resizes the clip window, causing the
     * child, i.e. This->plugin, to be "dragged" up by win-gravity.
     */
    case ConfigureNotify:
    case GravityNotify:
	if (This->plugin_widget == NULL)
	    return;

	{
	    int i;
	    Position x, y;
	    XConfigureEvent sendev;

	    XtTranslateCoords (This->plugin_widget, 0, 0, &x, &y);
	    for (i = 0; i < This->nclient_windows; i++) {
		sendev.type = ConfigureNotify;
		sendev.send_event = True;
		sendev.event = sendev.window = This->client_windows[i].win;
		sendev.x = x + This->client_windows[i].x;
		sendev.y = y + This->client_windows[i].y;
		sendev.width = This->client_windows[i].width;
		sendev.height = This->client_windows[i].height;
		sendev.border_width = This->client_windows[i].border_width;
		sendev.above = None;
		sendev.override_redirect = False;
		if (!XSendEvent (RxGlobal.dpy, This->client_windows[i].win,
				 False, StructureNotifyMask,
				 (XEvent*) &sendev))
		    (void) fprintf (stderr, "%s\n", "XSendEvent Failed");
	    }
	}
	break;

    default:
	break;
    }
}

/***********************************************************************
 * Event Handler to detect the destruction of a client window
 ***********************************************************************/
static void
SubstructureNotifyHandler (
    Widget widget, 
    XtPointer client_data, 
    XEvent* event, 
    Boolean* cont)
{
    PluginInstance* This = (PluginInstance*) client_data;

#ifdef PLUGIN_TRACE
    fprintf (stderr, "%s\n", "SubstructureNotifyHandler");
#endif

    if (event->type == DestroyNotify) {
	int i;
#ifdef PLUGIN_TRACE
	fprintf (stderr, "%s\n", "DestroyNotify");
#endif
	for (i = 0; i < This->nclient_windows; i++)
	    if (This->client_windows[i].win == event->xdestroywindow.window) {
		This->nclient_windows--;
		if (This->nclient_windows > 0) {
		    /* remove this window from the list */
		    for (; i < This->nclient_windows; i++)
			This->client_windows[i] = This->client_windows[i + 1];
		} else {	/* no more client windows! */
		    /* get back to user to restart the application */
		    RxpSetStatusWidget(This, WAITING);
		}
		ResetWMColormap(This, event->xdestroywindow.window);
		UnregisterDrawable(event->xdestroywindow.window,
				   This->plugin_widget);
		break;
	    }

    }
}

/***********************************************************************
 * Arrange to receive  (synthetic) ConfigureNotify events on the proper
 * windows of this instance and relay them to the embedded apps
 ***********************************************************************/
static void
SetupStructureNotify (PluginInstance* This)
{
    /* Get ConfigureNotify when the browser is moved */
    XtAddRawEventHandler (This->toplevel_widget,
			  StructureNotifyMask,
			  False,
			  StructureNotifyHandler,
			  (XtPointer) This);

    XtAddRawEventHandler (This->toplevel_widget,
			  NoEventMask,
			  True,
			  RxpWmDelWinHandler,
			  (XtPointer) This);
#if 0
    XmAddWMProtocolCallback (This->toplevel_widget,
			     RxGlobal.wm_delete_window,
			     RxpWmDelWinHandler,
			     (XtPointer) This);
#endif
}

/***********************************************************************
 * Event Handler to deal with colormap settings
 ***********************************************************************/
static void
CrossingHandler (
    Widget widget, 
    XtPointer client_data, 
    XEvent* event, 
    Boolean* cont)
{
    PluginInstance* This = (PluginInstance*) client_data;

#ifdef PLUGIN_TRACE
    fprintf (stderr, "%s: 0x%x\n", "CrossingHandler", event->xany.window);
#endif

    if (event->xany.window != XtWindow(This->plugin_widget) &&
	event->xcrossing.detail != NotifyInferior) {
	if (event->type == EnterNotify) {
#ifdef PLUGIN_TRACE
	    fprintf (stderr, "%s\n", "EnterNotify");
#endif
	    SetWMColormap(This, event->xany.window);
	} else if (event->type == LeaveNotify) {
#ifdef PLUGIN_TRACE
	    fprintf (stderr, "%s\n", "LeaveNotify");
#endif
	    UnsetWMColormap(This, event->xany.window);
	}
    }
}

/***********************************************************************
 * Setup various event handlers on the plugin widget
 ***********************************************************************/
void
RxpSetupPluginEventHandlers (PluginInstance* This)
{
    int i;

    /* Get ConfigureNotify and GravityNotify on the plugin */
    XtAddEventHandler (This->plugin_widget,
		       StructureNotifyMask,
		       False,
		       StructureNotifyHandler,
		       (XtPointer) This);
    /* Arrange to receive DestroyNotify events on the clients windows. */
    XtAddEventHandler (This->plugin_widget,
		       SubstructureNotifyMask,
		       False,
		       SubstructureNotifyHandler,
		       (XtPointer) This);
    /* Arrange to receive MapRequest and ConfigureRequest events on the
     * netscape plug-in widget. */
    XtAddRawEventHandler(This->plugin_widget,
			 SubstructureRedirectMask, 
			 False, 
			 SubstructureRedirectHandler,
			 (XtPointer) This);
    XtRegisterDrawable (RxGlobal.dpy, This->app_group, This->plugin_widget);

    /* Arrange to receive Enter and Leave Notify events on application's
       toplevel windows */
    XtAddRawEventHandler(This->plugin_widget,
			 EnterWindowMask | LeaveWindowMask, 
			 False, 
			 CrossingHandler,
			 (XtPointer) This);
    for (i = 0; i < This->nclient_windows; i++) {
	XtRegisterDrawable (RxGlobal.dpy,
			    This->client_windows[i].win, This->plugin_widget);
    }
}

/***********************************************************************
 * The instance is gone. Remove Event Handlers so that they aren't
 * called with a reference to the old instance.
 ***********************************************************************/
void
RxpTeardown (PluginInstance* This)
{
    if (This->toplevel_widget != NULL) {
#if 0 /* this crashes mozilla/firefox  */
	/* ConfigureNotify on top level */
	XtRemoveRawEventHandler (This->toplevel_widget,
				 StructureNotifyMask,
				 False, 
				 StructureNotifyHandler,
				 (XtPointer) This);
	XtRemoveRawEventHandler (This->toplevel_widget,
				 NoEventMask,
				 True,
				 RxpWmDelWinHandler,
				 (XtPointer) This);
#endif
#if 0
	XmRemoveWMProtocolCallback (This->toplevel_widget,
				    RxGlobal.wm_delete_window,
				    RxpWmDelWinHandler,
				    (XtPointer) This);
#endif
    }
}

/***********************************************************************
 * Process the given RxParams and make the RxReturnParams
 ***********************************************************************/

static int
ProcessUIParams(PluginInstance* This,
		Boolean trusted, Boolean use_fwp, Boolean use_lbx,
		RxParams *in, RxReturnParams *out, char **x_ui_auth_ret)
{
    XSecurityAuthorization dum;
    int dummy;
    char *display_name;

    This->app_group = None;
    if (out->embedded != RxFalse) {	/* default is embedded */
	/* let's see whether the server supports AppGroups or not */
	if (RxGlobal.has_appgroup == RxUndef) {
	    if (XQueryExtension(RxGlobal.dpy, "XC-APPGROUP",
				&dummy, &dummy, &dummy) &&
		XagQueryVersion (RxGlobal.dpy, &dummy, &dummy))
		RxGlobal.has_appgroup = RxTrue;
	    else
		RxGlobal.has_appgroup = RxFalse;
	}
	if (RxGlobal.has_appgroup == RxTrue) {
	    Screen *scr;
	    Colormap cmap;
	    Arg arg;

	    /* use plugin's colormap as the default colormap */
	    XtSetArg(arg, XtNcolormap, &cmap);
	    XtGetValues(This->plugin_widget, &arg, 1);
	    scr = XtScreen(This->plugin_widget);
	    if (cmap == DefaultColormapOfScreen(scr)) {
		XagCreateEmbeddedApplicationGroup (RxGlobal.dpy, None,
						   cmap,
						   BlackPixelOfScreen(scr),
						   WhitePixelOfScreen(scr),
						   &This->app_group);
	    } else {
		XColor black, white;
		Pixel pixels[2];

		black.red = black.green = black.blue = 0;
		XAllocColor(RxGlobal.dpy, cmap, &black);
		white.red = white.green = white.blue = 65535;
		XAllocColor(RxGlobal.dpy, cmap, &white);
		XagCreateEmbeddedApplicationGroup (RxGlobal.dpy, None,
						   cmap,
						   pixels[0] = black.pixel,
						   pixels[1] = white.pixel,
						   &This->app_group);
		XFreeColors(RxGlobal.dpy, cmap, pixels, 2, 0);
	    }
	    SetupStructureNotify (This);
	    RxpSetupPluginEventHandlers (This);
	} else {		/* too bad */
	    out->embedded = RxFalse;
	    fprintf(stderr, "Warning: Cannot perform embedding as \
requested, APPGROUP extension not supported\n");
	}
    }

    if (in->x_ui_auth[0] != 0) {
	GetXAuth(RxGlobal.dpy, in->x_ui_auth[0], in->x_ui_auth_data[0],
		 trusted, This->app_group, False, DEFAULT_TIMEOUT,
		 x_ui_auth_ret, &This->x_ui_auth_id, &dummy);
    } else if (in->x_auth[0] != 0)
	GetXAuth(RxGlobal.dpy, in->x_auth[0], in->x_auth_data[0], 
		 trusted, This->app_group, False, DEFAULT_TIMEOUT,
		 x_ui_auth_ret, &This->x_ui_auth_id, &dummy);

    /* make sure we use the server the user wants us to use */
    if (RxGlobal.has_real_server == RxUndef) {
	Display *rdpy = RxGlobal.dpy;
	char *real_display = getenv("XREALDISPLAY");
	RxGlobal.has_real_server = RxFalse;
	if (real_display != NULL) {
	    rdpy = XOpenDisplay(real_display);
	    if (rdpy == NULL)
		rdpy = RxGlobal.dpy;
	    else
		RxGlobal.has_real_server = RxTrue;
	}
	/* let's see now whether the server supports LBX or not */
	if (XQueryExtension(rdpy, "LBX", &dummy, &dummy, &dummy))
	    RxGlobal.has_ui_lbx = RxTrue;
	else
	    RxGlobal.has_ui_lbx = RxFalse;

	if (rdpy != RxGlobal.dpy)
	    XCloseDisplay(rdpy);
    }
    if (RxGlobal.has_real_server == RxTrue)
	display_name = getenv("XREALDISPLAY");
    else
	display_name = DisplayString(RxGlobal.dpy);

    /* let's see whether we have a firewall proxy */
    if (use_fwp == True && RxGlobal.has_ui_fwp == RxUndef) {
	RxGlobal.fwp_dpyname = GetXFwpDisplayName(display_name);
	if (RxGlobal.fwp_dpyname != NULL)
	    RxGlobal.has_ui_fwp = RxTrue;
	else {
	    /*
	     * We were supposed to use the firewall proxy but we
	     * couldn't get a connection.  There is no need to
	     * continue.
	     */
	    return 1;
	}
    }
    if (use_fwp == True && RxGlobal.has_ui_fwp == RxTrue)
	out->ui = GetXUrl(RxGlobal.fwp_dpyname, *x_ui_auth_ret, in->action);
    else
	out->ui = GetXUrl(display_name, *x_ui_auth_ret, in->action);

    if (in->x_ui_lbx == RxTrue) {
	if (use_lbx == True) {
	    if (RxGlobal.has_ui_lbx == RxTrue) {
		out->x_ui_lbx = RxTrue;

		/* let's get a key for the proxy now */
		if (in->x_ui_lbx_auth[0] != 0) {
		    GetXAuth(RxGlobal.dpy, in->x_ui_lbx_auth[0],
			     in->x_ui_lbx_auth_data[0],
			     trusted, None, False, DEFAULT_TIMEOUT,
			     &out->x_ui_lbx_auth, &dum, &dummy);
		} else if (in->x_auth[0] != 0)
		    GetXAuth(RxGlobal.dpy, in->x_auth[0], in->x_auth_data[0],
			     trusted, None, False, DEFAULT_TIMEOUT,
			     &out->x_ui_lbx_auth, &dum, &dummy);
	    } else {
		out->x_ui_lbx = RxFalse;
		fprintf(stderr, "Warning: Cannot setup LBX as requested, \
LBX extension not supported\n");
	    }
	} else
	    out->x_ui_lbx = RxFalse;
    } else			/* it's either RxFalse or RxUndef */
	out->x_ui_lbx = in->x_ui_lbx;

    return 0;
}

static int
ProcessPrintParams(PluginInstance* This,
		   Boolean trusted, Boolean use_fwp, Boolean use_lbx,
		   RxParams *in, RxReturnParams *out, char *x_ui_auth)
{
    char *auth = NULL;
    XSecurityAuthorization dum;
    int dummy;

    /* let's find out if we have a print server */
    if (RxGlobal.has_printer == RxUndef) {
	RxGlobal.pdpy_name = GetXPrintDisplayName(&RxGlobal.printer_name);
	if (RxGlobal.pdpy_name != NULL) {
	    /* open connection to the print server */
	    RxGlobal.pdpy = XOpenDisplay(RxGlobal.pdpy_name);
	    if (RxGlobal.pdpy != NULL)
		RxGlobal.has_printer = RxTrue;
	    else
		RxGlobal.has_printer = RxFalse;
	} else {
	    /* no server specified,
	       let's see if the video server could do it */
	    if (XQueryExtension(RxGlobal.dpy, "XpExtension",
				&dummy, &dummy, &dummy)) {
		RxGlobal.has_printer = RxTrue;
		RxGlobal.pdpy = RxGlobal.dpy;
	    } else
		RxGlobal.has_printer = RxFalse;
	}
    }
    if (RxGlobal.has_printer == RxFalse) {
	fprintf(stderr, "Warning: Cannot setup X printer as requested, \
no server found\n");
	return 0;
    }

    /* create a key only when the video server is not the print
       server or when we didn't create a key yet */
    if (RxGlobal.pdpy != RxGlobal.dpy || x_ui_auth == NULL) {
	if (in->x_print_auth[0] != 0)
	    GetXAuth(RxGlobal.pdpy, in->x_print_auth[0],
		     in->x_print_auth_data[0],
		     trusted, None, False, NO_TIMEOUT,
		     &auth, &This->x_print_auth_id, &dummy);
	else if (in->x_auth[0] != 0)
	    GetXAuth(RxGlobal.pdpy, in->x_auth[0], in->x_auth_data[0],
		     trusted, None, False, NO_TIMEOUT,
		     &auth, &This->x_print_auth_id, &dummy);
    }

    /* let's see whether we have a firewall proxy */
    if (use_fwp == True && RxGlobal.has_print_fwp == RxUndef) {
	RxGlobal.pfwp_dpyname = GetXFwpDisplayName(DisplayString(RxGlobal.pdpy));
	if (RxGlobal.pfwp_dpyname != NULL)
	    RxGlobal.has_print_fwp = RxTrue;
	else {
	    /*
	     * We were supposed to use the firewall proxy but we
	     * couldn't get a connection.  There is no need to
	     * continue.
	     */
	    return 1;
	}
    }
    if (use_fwp == True && RxGlobal.has_print_fwp == RxTrue)
	out->print = GetXPrintUrl(RxGlobal.pfwp_dpyname,
				  RxGlobal.printer_name, auth,
				  in->action);
    else
	out->print = GetXPrintUrl(DisplayString(RxGlobal.pdpy),
				  RxGlobal.printer_name, auth,
				  in->action);

    if (auth != NULL)
	NPN_MemFree(auth);

    if (in->x_print_lbx == RxTrue) {
	if (use_lbx == True) {
	    /* let's see whether the server supports LBX or not */
	    if (RxGlobal.has_print_lbx == RxUndef) {
		if (RxGlobal.pdpy == RxGlobal.dpy &&
		    RxGlobal.has_ui_lbx != RxUndef) {
		    /* the video server is the print server and we already
		       know whether it supports LBX or not */
		    RxGlobal.has_print_lbx = RxGlobal.has_ui_lbx;
		} else {
		    if (XQueryExtension(RxGlobal.pdpy, "LBX",
					&dummy, &dummy, &dummy))
			RxGlobal.has_print_lbx = RxTrue;
		    else
			RxGlobal.has_print_lbx = RxFalse;
		}
	    }
	    if (RxGlobal.has_print_lbx == RxTrue) {
		out->x_print_lbx = RxTrue;
		if (RxGlobal.pdpy != RxGlobal.dpy) {
		    /* let's get a key for the proxy now */
		    if (in->x_print_lbx_auth[0] != 0) {
			GetXAuth(RxGlobal.pdpy, in->x_print_lbx_auth[0],
				 in->x_print_lbx_auth_data[0],
				 trusted, None, False, DEFAULT_TIMEOUT,
				 &out->x_print_lbx_auth, &dum, &dummy);
		    } else if (in->x_auth[0] != 0)
			GetXAuth(RxGlobal.pdpy, in->x_auth[0],
				 in->x_auth_data[0],
				 trusted, None, False, DEFAULT_TIMEOUT,
				 &out->x_print_lbx_auth, &dum, &dummy);
		}
	    } else {
		out->x_print_lbx = RxFalse;
		fprintf(stderr, "Warning: Cannot setup LBX as \
requested, LBX extension not supported\n");
	    }
	} else
	    out->x_print_lbx = RxFalse;
    } else		/* it's either RxFalse or RxUndef */
	out->x_print_lbx = in->x_print_lbx;

    return 0;
}

int
RxpProcessParams(PluginInstance* This, RxParams *in, RxReturnParams *out)
{
    char *x_ui_auth = NULL;
    char webserver[MAXHOSTNAMELEN];
    Boolean trusted, use_fwp, use_lbx;
    int return_value = 0;

#ifdef PLUGIN_TRACE
    fprintf (stderr, "%s\n", "RxpProcessParams");
    fprintf (stderr, "This: 0x%x\n", This);
#endif

    /* init return struture */
    memset(out, 0, sizeof(RxReturnParams));
    out->x_ui_lbx = RxUndef;
    out->x_print_lbx = RxUndef;
    out->action = in->action;

    if (in->embedded != RxUndef)
	out->embedded = in->embedded;
    else
	out->embedded = RxUndef;

    out->width = in->width;
    out->height = in->height;	

    if (RxGlobal.get_prefs == True) {
	GetPreferences(This->toplevel_widget, &RxGlobal.prefs);
	RxGlobal.get_prefs = False;
    }
    ComputePreferences(&RxGlobal.prefs,
       ParseHostname(in->action, webserver, MAXHOSTNAMELEN) ? webserver : NULL,
       &trusted, &use_fwp, &use_lbx);

    if (in->ui[0] == XUI)	/* X display needed */
	return_value = ProcessUIParams(This, trusted, use_fwp, use_lbx, 
			   in, out, &x_ui_auth);

    if (in->print[0] == XPrint) /* XPrint server needed */
	return_value = ProcessPrintParams(This, trusted, use_fwp, use_lbx,
			   in, out, x_ui_auth);

    if (x_ui_auth != NULL)
	NPN_MemFree(x_ui_auth);

    return return_value;
}
