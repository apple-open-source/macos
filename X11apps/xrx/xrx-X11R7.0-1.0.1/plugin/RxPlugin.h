/* $Xorg: RxPlugin.h,v 1.4 2001/02/09 02:05:57 xorgcvs Exp $ */
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
 * RX plug-in header file, based on the UnixTemplate file provided by Netcape.
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

#ifndef _RxPlugin_h
#define _RxPLugin_h

#include "npapi.h"
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/Xag.h>
#include <X11/extensions/security.h>
#include <X11/ICE/ICElib.h>
#include <stdio.h>
#include "Rx.h"
#include "Prefs.h"

/***********************************************************************
 * Instance state information about the plugin.
 *
 * PLUGIN DEVELOPERS:
 *	Use this struct to hold per-instance information that you'll
 *	need in the various functions in this file.
 ***********************************************************************/

typedef enum {
	RxpNoflags	= 0,
	RxpWmDelWin	= (1 << 0),
	RxpMapped	= (1 << 4)
} WindowFlags;

typedef struct {
    Window win;
    Position x,y;
    Dimension width,height;
    Dimension border_width;
    WindowFlags flags;
    Colormap colormap;
} windowrec;

typedef enum { LOADING, STARTING, WAITING, RUNNING } PluginState;

typedef struct _PluginInstance
{
    NPP instance;
    int16 argc;			/* HTML arguments given by Netscape */
    char **argn;
    char **argv;
    short parse_reply;		/* 0 - no
				   1 - look for status line
				   2 - done */ 
    short status;		/* returned application status */
    RxBool dont_reparent;	/* whether client windows need reparent*/
    char *query;
    PluginState state;
    Widget status_widget;
    Widget plugin_widget;
    Dimension width, height;
    /* The following fields need to be taken care by RxpNew & RxpDestroy */
    XSecurityAuthorization x_ui_auth_id;
    XSecurityAuthorization x_print_auth_id;
    XAppGroup app_group;
    Widget toplevel_widget;
    windowrec *client_windows;
    int nclient_windows;
} PluginInstance;

typedef struct _PluginGlobal {
    Boolean inited;
    RxBool has_appgroup;
    RxBool has_real_server;
    RxBool has_ui_lbx;
    RxBool has_print_lbx;
    RxBool has_printer;
    RxBool has_ui_fwp;
    RxBool has_print_fwp;
    char *pdpy_name;
    char *printer_name;
    char *fwp_dpyname;
    char *pfwp_dpyname;
    struct _IceConn* ice_conn;
    int pm_opcode;
    Preferences prefs;
    Boolean get_prefs;
    Display* dpy;
    Display* pdpy;
    Atom wm_delete_window;
    Atom wm_protocols;
} PluginGlobal;

extern PluginGlobal RxGlobal;


#define PLUGIN_NAME             "RX Plug-in"
#define PLUGIN_DESCRIPTION      "X Remote Activation Plug-in"
#define PLUGIN_MIME_DESCRIPTION \
	"application/x-rx:xrx:X Remote Activation Plug-in"


/* functions to init and free private members */
extern void RxpNew(PluginInstance*);
extern void RxpDestroy(PluginInstance*);

extern int
RxpProcessParams(PluginInstance*, RxParams*, RxReturnParams*);

extern void
RxpSetStatusWidget(PluginInstance*, PluginState);

extern void
RxpSetupPluginEventHandlers(PluginInstance*);

extern void
RxpTeardown (PluginInstance*);

extern void
RxpWmDelWinHandler (Widget, XtPointer, XEvent*, Boolean*);

extern void
RxpRemoveDestroyCallback (PluginInstance*);

#endif
