/* $Xorg: RxPlugin.h,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
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
#include <stdio.h>
#include "Rx.h"


/***********************************************************************
 * Instance state information about the plugin.
 *
 * PLUGIN DEVELOPERS:
 *	Use this struct to hold per-instance information that you'll
 *	need in the various functions in this file.
 ***********************************************************************/

typedef enum { LOADING, /* STARTING, */ WAITING, RUNNING} PluginState;

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
    Window window;
    pid_t child_pid;
    int display_num;		/* Xnest display number */
    Widget toplevel_widget;
} PluginInstance;

#define PLUGIN_NAME             "RX Xnest Plug-in"
#define PLUGIN_DESCRIPTION      "This X Remote Activation Plug-in uses the \
Xnest program to perform embedding of X applications."
#define PLUGIN_MIME_DESCRIPTION \
	"application/x-rx:xrx:X Remote Activation Plug-in"


/* functions to init and free private members */
extern void RxpNew(PluginInstance *This);
extern void RxpDestroy(PluginInstance *This);

extern int
RxpProcessParams(PluginInstance* This, RxParams *in, RxReturnParams *out);

extern void
RxpSetStatusWidget(PluginInstance*, PluginState);

extern void RxpInitXnestDisplayNumbers();
extern int RxpXnestDisplayNumber();
extern void RxpFreeXnestDisplayNumber(int i);
extern char * RxpXnestDisplay(int display_number);

#endif
