/* $Xorg: SetWin.c,v 1.4 2001/02/09 02:05:57 xorgcvs Exp $ */
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

/*
 * RX plug-in module based on the UnixTemplate file provided by Netcape.
 */

/* -*- Mode: C; tab-width: 4; -*- */
/******************************************************************************
 * Copyright 1996 Netscape Communications. All rights reserved.
 ******************************************************************************/
/*
 * UnixShell.c
 *
 * Netscape Client Plugin API
 * - Function that need to be implemented by plugin developers
 *
 * This file defines a "Template" plugin that plugin developers can use
 * as the basis for a real plugin.  This shell just provides empty
 * implementations of all functions that the plugin can implement
 * that will be called by Netscape (the NPP_xxx methods defined in 
 * npapi.h). 
 *
 * dp Suresh <dp@netscape.com>
 *
 */

#include "RxPlugin.h"
#include <X11/StringDefs.h>

/***********************************************************************
 * Sometimes the plugin widget gets stupidly destroyed, that is whenever
 * Netscape relayouts the page. This callback reparents the client
 * windows to the root window so they do not get destroyed as well.
 * Eventually the NPP_SetWindow function should be called and we'll
 * reparent them back under the plugin.
 ***********************************************************************/
static void
DestroyCB (Widget widget, XtPointer client_data, XtPointer call_data)
{
    PluginInstance* This = (PluginInstance*) client_data;
    int i;
#ifdef PLUGIN_TRACE
    fprintf (stderr, "DestroyCB, This: 0x%x\n", This);
#endif
    if (widget == This->plugin_widget) {
	This->plugin_widget = NULL;
	This->status_widget = NULL;
    }
    if (This->dont_reparent == RxFalse) {
	for (i = 0; i < This->nclient_windows; i++) {
	    XUnmapWindow (RxGlobal.dpy, This->client_windows[i].win);
	    This->client_windows[i].flags &= ~RxpMapped;

	    XReparentWindow (RxGlobal.dpy, This->client_windows[i].win,
			     (XtScreen (widget))->root, 0, 0);
	}
	This->dont_reparent = RxTrue;
    } else
	This->dont_reparent = RxFalse;
    /* 
     * not worth removing event handlers on this widget since it's
     * about to be destroyed anyway.
     */
}


/***********************************************************************
 * Sometimes the plugin widget gets stupidly resized, because of poor
 * geometry when its child (that is the status widget) gets destroyed.
 * So this callback resizes it back to the right size.
 * Note that this could lead to an endless battle, but it appears that
 * it doesn't so far...
 ***********************************************************************/
static void
ResizeCB (Widget widget, XtPointer client_data, XtPointer call_data)
{
    PluginInstance* This = (PluginInstance*) client_data;
    Arg args[5];
    int n;

#ifdef PLUGIN_TRACE
    fprintf (stderr, "ResizeCB, This: 0x%x\n", This);
#endif
    /* make sure plugin widget gets the same size back */
    n = 0;
    XtSetArg(args[n], XtNwidth, This->width); n++;
    XtSetArg(args[n], XtNheight, This->height); n++;
    XtSetValues(This->plugin_widget, args, n);
}

static Widget
FindToplevel(Widget widget)
{
    while (XtParent(widget) != NULL && !XtIsTopLevelShell(widget))
	widget = XtParent(widget);

    return widget;
}

/***********************************************************************
 * This function gets called first when the plugin widget is created and
 * then whenever the plugin is changed.
 ***********************************************************************/
NPError 
NPP_SetWindow(NPP instance, NPWindow* window)
{
    PluginInstance* This;
    Widget netscape_widget;

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    if (window == NULL)
	return NPERR_NO_ERROR;

    /*
     * PLUGIN DEVELOPERS:
     *	Before setting window to point to the
     *	new window, you may wish to compare the new window
     *	info to the previous window (if any) to note window
     *	size changes, etc.
     */
    This = (PluginInstance*) instance->pdata;
#ifdef PLUGIN_TRACE
    fprintf(stderr, "SetWindow 0x%x.\n", (Window) window->window);
    fprintf(stderr, "This: 0x%x\n", This);
    if (This->plugin_widget)
	fprintf(stderr, "This->plugin_widget: 0x%x\n", This->plugin_widget);
#endif
    if (RxGlobal.dpy == NULL) {
	RxGlobal.dpy = ((NPSetWindowCallbackStruct *)window->ws_info)->display;
	RxGlobal.wm_delete_window =
	    XInternAtom (RxGlobal.dpy, "WM_DELETE_WINDOW", TRUE);
	RxGlobal.wm_protocols = XInternAtom (RxGlobal.dpy, "WM_PROTOCOLS", TRUE);
    }
    netscape_widget = XtWindowToWidget(RxGlobal.dpy, (Window) window->window);
    if (This->toplevel_widget == NULL)
	This->toplevel_widget = FindToplevel(netscape_widget);

    if (This->plugin_widget != netscape_widget) {

	/* We have a new widget store it */
	This->plugin_widget = netscape_widget;
	This->width = window->width;
	This->height = window->height;

	XtAddCallback (This->plugin_widget, XtNdestroyCallback, 
		       DestroyCB, (XtPointer) This);
	XtAddCallback (This->plugin_widget, "resizeCallback", 
		       ResizeCB, (XtPointer) This);

	if (This->app_group)
	    RxpSetupPluginEventHandlers (This);

	if (This->nclient_windows > 0) {
	    int i;

	    /* We already have the client, so we need to reparent it to the
	       new window */
	    for (i = 0; i < This->nclient_windows; i++) {
		XReparentWindow(RxGlobal.dpy, This->client_windows[i].win,
				XtWindow(netscape_widget), 
				This->client_windows[i].x,
				This->client_windows[i].y);
		if (This->dont_reparent == RxTrue) {
		    XMapWindow (RxGlobal.dpy, This->client_windows[i].win);
		    This->client_windows[i].flags |= RxpMapped;
		}
	    }
	} else			/* no client window, display status widget */
	    RxpSetStatusWidget(This, This->state);
	if (This->dont_reparent != RxFalse) /* can be True or Undef */
	    This->dont_reparent = RxFalse;
	else
	    This->dont_reparent = RxTrue;
    }
    return NPERR_NO_ERROR;
}

void
RxpRemoveDestroyCallback(PluginInstance *This)
{
    if (This->plugin_widget != NULL)
	XtRemoveCallback(This->plugin_widget, XtNdestroyCallback, 
			 DestroyCB, (XtPointer) This);
}
