/* $Xorg: Main.c,v 1.5 2001/02/09 02:05:57 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xrx/plugin/Main.c,v 1.8tsi Exp $ */

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

#ifdef USE_MOTIF
extern WidgetClass xmLabelGadgetClass;
extern WidgetClass xmPushButtonGadgetClass;
#else
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#endif /* USE_MOTIF */

#include <ctype.h>
#include <stdlib.h>

/***********************************************************************
 * Utility functions to deal with list of arguments
 ***********************************************************************/

/* Free list of arguments */
static void
FreeArgs(char* argn[], char* argv[], int argc)
{
    int i;
    if (argc != 0) {
	for (i = 0; i < argc; i++) {
	    NPN_MemFree(argn[i]);
	    NPN_MemFree(argv[i]);
	}
	NPN_MemFree(argn);
	NPN_MemFree(argv);
    }
}

/* Copy list 2 to list 1 */
static NPError
CopyArgs(char** argn1[], char** argv1[], int16* argc1,
	 char* argn2[], char* argv2[], int16 argc2)
{
    char **argn, **argv;
    int i;

    argn = (char **)NPN_MemAlloc(sizeof(char *) * argc2);
    if (!argn)
	return NPERR_OUT_OF_MEMORY_ERROR;
    argv = (char **)NPN_MemAlloc(sizeof(char *) * argc2);
    if (!argv) {
	NPN_MemFree(argn);
	return NPERR_OUT_OF_MEMORY_ERROR;
    }
    memset(argn, 0, sizeof(char *) * argc2);
    memset(argv, 0, sizeof(char *) * argc2);
    for (i = 0; i < argc2; i++) {
	char *name, *value;
	name = (char *)NPN_MemAlloc(strlen(argn2[i]) + 1);
	if (!name) {
	    FreeArgs(argn, argv, i - 1);
	    return NPERR_OUT_OF_MEMORY_ERROR;
	}
	strcpy(name, argn2[i]);
	value = (char *)NPN_MemAlloc(strlen(argv2[i]) + 1);
	if (!value) {
	    NPN_MemFree(name);
	    FreeArgs(argn, argv, i - 1);
	    return NPERR_OUT_OF_MEMORY_ERROR;
	}
	strcpy(value, argv2[i]);
	argn[i] = name;
	argv[i] = value;
    }
    *argc1 = argc2;
    *argn1 = argn;
    *argv1 = argv;

    return NPERR_NO_ERROR;
}


char*
NPP_GetMIMEDescription(void)
{
    return(PLUGIN_MIME_DESCRIPTION);
}

NPError
NPP_GetValue(void *future, NPPVariable variable, void *value)
{
    NPError err = NPERR_NO_ERROR;

    switch (variable) {
    case NPPVpluginNameString:
	*((char **)value) = PLUGIN_NAME;
	break;
    case NPPVpluginDescriptionString:
	*((char **)value) = PLUGIN_DESCRIPTION;
	break;
    default:
	err = NPERR_GENERIC_ERROR;
    }
    return err;
}

jref
NPP_GetJavaClass()
{
    return NULL;
}

NPError 
NPP_New(NPMIMEType pluginType,
	NPP instance,
	uint16 mode,
	int16 argc,
	char* argn[],
	char* argv[],
	NPSavedData* saved)
{
    PluginInstance* This;

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
	
    This = (PluginInstance*) instance->pdata;

    if (This == NULL)
	return NPERR_OUT_OF_MEMORY_ERROR;

    This->instance = instance;
    if (argc != 0) {		/* copy the arguments list */
	if (CopyArgs(&This->argn, &This->argv, &This->argc,
		     argn, argv, argc) == NPERR_OUT_OF_MEMORY_ERROR) {
	    NPN_MemFree(This);
	    return NPERR_OUT_OF_MEMORY_ERROR;
	}
    } else {
	This->argc = 0;
	This->argn = This->argv = NULL;
    }
    This->parse_reply = 0;
    This->status = 0;
    This->dont_reparent = RxUndef;
    This->state = LOADING;
    This->status_widget = NULL;
    This->plugin_widget = NULL;
    RxpNew(This);

    return NPERR_NO_ERROR;
}


NPError 
NPP_Destroy(NPP instance, NPSavedData** save)
{
    PluginInstance* This;

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    /* PLUGIN DEVELOPERS:
     *	If desired, call NP_MemAlloc to create a
     *	NPSavedDate structure containing any state information
     *	that you want restored if this plugin instance is later
     *	recreated.
     */

    if (This != NULL) {
	RxpDestroy(This);
	if (This->argc != 0)
	    FreeArgs(This->argn, This->argv, This->argc);
	if (This->query != NULL)
	    NPN_MemFree(This->query);
	NPN_MemFree(instance->pdata);
	instance->pdata = NULL;
    }

    return NPERR_NO_ERROR;
}


/* private buffer structure */
typedef struct {
    char *buf;
    uint32 size;
} RxStreamBuf;

NPError 
NPP_NewStream(NPP instance,
	      NPMIMEType type,
	      NPStream *stream, 
	      NPBool seekable,
	      uint16 *stype)
{
    PluginInstance* This;
    RxStreamBuf *streambuf;

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    if (This->parse_reply != 0)
	return NPERR_NO_ERROR;

    /* malloc structure to store RX document */
    streambuf = (RxStreamBuf *) NPN_MemAlloc(sizeof(RxStreamBuf));
    if (streambuf == NULL)
	return NPERR_OUT_OF_MEMORY_ERROR;

    streambuf->buf = NULL;
    streambuf->size = 0;
    stream->pdata = (void *) streambuf;

    return NPERR_NO_ERROR;
}


/* PLUGIN DEVELOPERS:
 *	These next 2 functions are directly relevant in a plug-in which
 *	handles the data in a streaming manner. If you want zero bytes
 *	because no buffer space is YET available, return 0. As long as
 *	the stream has not been written to the plugin, Navigator will
 *	continue trying to send bytes.  If the plugin doesn't want them,
 *	just return some large number from NPP_WriteReady(), and
 *	ignore them in NPP_Write().  For a NP_ASFILE stream, they are
 *	still called but can safely be ignored using this strategy.
 */

int32 STREAMBUFSIZE = 0X0FFFFFFF; /* If we are reading from a file in NPAsFile
				   * mode so we can take any size stream in our
				   * write call (since we ignore it) */

int32 
NPP_WriteReady(NPP instance, NPStream *stream)
{
    return STREAMBUFSIZE;
}

int32 
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buf)
{
    PluginInstance* This;

    if (instance == NULL)
	return len;

    This = (PluginInstance*) instance->pdata;

    if (This->parse_reply == 0) {
	/* copy stream buffer to private storage, concatenating if necessary:
	 * since Netscape doesn't provide an NPN_MemRealloc we must do
	 * a new malloc, copy, and free :-(
	 */
	RxStreamBuf *streambuf = (RxStreamBuf *) stream->pdata;
	uint32 size = streambuf->size;
	char *cbuf;

	/* if first chunk add 1 for null terminating character */
	if (size == 0)
	    size++;

	size += len;
	cbuf = (char *) NPN_MemAlloc(size);
	if (cbuf == NULL)
	    return -1;
	if (streambuf->size != 0) {
	    memcpy(cbuf, streambuf->buf, streambuf->size - 1);
	    memcpy(cbuf + streambuf->size - 1, ((char *)buf), len);
	    /* free old storage */
	    NPN_MemFree((void *) streambuf->buf);
	} else
	    memcpy(cbuf, ((char *)buf), len);
	cbuf[size - 1] = '\0';
	/* store new buffer */
	streambuf->buf = cbuf;
	streambuf->size = size;
#ifdef PLUGIN_TRACE
	fprintf(stderr, "write %s:\n", PLUGIN_NAME);
	fwrite(buf, len, 1, stderr);
	fprintf(stderr, "\n");
#endif
    } else {
	int l = len;
	if (This->parse_reply == 1) {
	    /* look for status line */
	    char *ptr = strchr(buf, '\n');
	    if (ptr != NULL && isdigit(((char *)buf)[0])) {
		This->status = (short) atoi((char *)buf);
		/* skip status line */
		l -= ptr - (char *)buf + 1;
		buf = ptr + 1;
		if (This->status != 0) {
		    fprintf(stderr,
			    "%s: Application failed to start properly\n",
			    PLUGIN_NAME);
		}
	    }
	    This->parse_reply = 2;
	}
	/* simply prints out whatever we get, and let netscape display it
	   in a dialog */
	fwrite(buf, l, 1, stderr);
    }

    return len;			/* The number of bytes accepted */
}

void
StartApplication(PluginInstance* This)
{
#ifndef NO_STARTING_STATE
    RxpSetStatusWidget(This, STARTING);
#else
    RxpSetStatusWidget(This, RUNNING);
#endif

    /* perform GET request
     * throwing away the response.
     */
    (void) NPN_GetURL(This->instance, This->query, NULL);
    This->parse_reply = 1;	/* we want to print out the answer  */
}

void
StartCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PluginInstance* This = (PluginInstance*) client_data;
#if 0
    XtUnmapWidget(widget);
#endif
    XtDestroyWidget(widget);
    StartApplication(This);
}


void
RxpSetStatusWidget(PluginInstance* This, PluginState state)
{
    Arg args[5];
    int n;
    XrmDatabase db;
    char* return_type;
    XrmValue return_value;

    if (This->status_widget) {
	XtDestroyWidget(This->status_widget);
	This->status_widget = NULL;
    }
    if (This->plugin_widget == NULL)
	return;

    db = XtDatabase (XtDisplay (This->plugin_widget));

    if (!XrmGetResource (db, "RxPlugin_BeenHere", "RxPlugin_BeenHere",
		    &return_type, &return_value)) {

	XrmPutStringResource (&db, "*Rx_Loading.labelString", "Loading...");
	XrmPutStringResource (&db, "*Rx_Starting.labelString", "Starting...");
	XrmPutStringResource (&db, "*Rx_Start.labelString", "Start");
	XrmPutStringResource (&db, "RxPlugin_BeenHere", "YES");
    }

    n = 0;
    XtSetArg(args[n], "shadowThickness", 1); n++;
    XtSetArg(args[n], XtNwidth, This->width); n++;
    XtSetArg(args[n], XtNheight, This->height); n++;
#ifdef USE_MOTIF
    if (state == LOADING) {
	/* create a label */
	This->status_widget =
	    XtCreateManagedWidget("Rx_Loading", xmLabelGadgetClass,
		This->plugin_widget, args, n);
#ifndef NO_STARTING_STATE
    } else if (state == STARTING) {
	/* create a label */
	This->status_widget =
	    XtCreateManagedWidget("Rx_Starting", xmLabelGadgetClass,
		This->plugin_widget, args, n);
#endif
    } else if (state == WAITING) {
	/* create a push button */
	This->status_widget =
	    XtCreateManagedWidget("Rx_Start", xmPushButtonGadgetClass,
		This->plugin_widget, args, n);
	XtAddCallback(This->status_widget, "activateCallback", StartCB, This);
    } else if (state == RUNNING) {
	/* nothing else to be done */
    }
#else
    if (state == LOADING) {
	/* create a label */
	This->status_widget =
	    XtCreateManagedWidget("Rx_Loading", labelWidgetClass,
		This->plugin_widget, args, n);
#ifndef NO_STARTING_STATE
    } else if (state == STARTING) {
	/* create a label */
	This->status_widget =
	    XtCreateManagedWidget("Rx_Starting", labelWidgetClass,
		This->plugin_widget, args, n);
#endif
    } else if (state == WAITING) {
	/* create a push button */
	This->status_widget =
	    XtCreateManagedWidget("Rx_Start", commandWidgetClass,
		This->plugin_widget, args, n);
	XtAddCallback(This->status_widget, XtNcallback, StartCB, This);
    } else if (state == RUNNING) {
	/* nothing else to be done */
    }
#endif /* USE_MOTIF */
    This->state = state;
}


NPError 
NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
    PluginInstance* This;
    RxStreamBuf *streambuf = (RxStreamBuf *) stream->pdata;
    char **rx_argn, **rx_argv;
    int rx_argc;
    RxParams params;
    RxReturnParams return_params;
    NPError status = NPERR_NO_ERROR;

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    if (This->parse_reply != 0) {
	fflush(stderr);
	if (This->status != 0)	/* if error occured change widget status */
	    RxpSetStatusWidget(This, WAITING);
	return NPRES_DONE;
    }

    memset(&params, 0, sizeof(RxParams));
    memset(&return_params, 0, sizeof(RxReturnParams));
    rx_argc = 0;

    if (reason != NPRES_DONE) {
	status = NPERR_GENERIC_ERROR;
	goto exit;
    }

    /* read params from stream */
    if (RxReadParams(streambuf->buf, &rx_argn, &rx_argv, &rx_argc) != 0) {
	fprintf(stderr, "%s: invalid file %s\n", PLUGIN_NAME, stream->url);
	status = NPERR_GENERIC_ERROR;
	goto exit;
    }

    RxInitializeParams(&params);

    /* parse RX params */
    if (RxParseParams(rx_argn, rx_argv, rx_argc, &params, 0) != 0) {
	fprintf(stderr, "%s: invalid RX params\n", PLUGIN_NAME);
	status = NPERR_GENERIC_ERROR;
	goto exit;
    }

    /* parse HTML params */
    if (RxParseParams(This->argn, This->argv, This->argc, &params, 0) != 0) {
	fprintf(stderr, "%s: invalid HTML params\n", PLUGIN_NAME);
	status = NPERR_GENERIC_ERROR;
	goto exit;
    }

    /* set up return parameters */
    if (RxpProcessParams(This, &params, &return_params) != 0) {
	fprintf(stderr, "%s: failed to process params\n", PLUGIN_NAME);
	status = NPERR_GENERIC_ERROR;
	goto exit;
    }

    /* make query */
    This->query = RxBuildRequest(&return_params);
    if (This->query == NULL) {
	fprintf(stderr, "%s: failed to make query\n", PLUGIN_NAME);
	status = NPERR_GENERIC_ERROR;
	goto exit;
    }

    if (params.auto_start != RxFalse) /* default is auto start */
	StartApplication(This);
    else
	RxpSetStatusWidget(This, WAITING);

exit:
    /* free all forms of params */
    FreeArgs(rx_argn, rx_argv, rx_argc);
    FreeArgs(This->argn, This->argv, This->argc);
    This->argc = 0;
    RxFreeParams(&params);
    RxFreeReturnParams(&return_params);
    /* free private storage */
    if (streambuf->buf != NULL)
	NPN_MemFree(streambuf->buf);
    NPN_MemFree(stream->pdata);

    return status;
}

void 
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
    /*
    PluginInstance* This;
    if (instance != NULL)
	This = (PluginInstance*) instance->pdata;
     */
}


void 
NPP_Print(NPP instance, NPPrint* printInfo)
{
    if(printInfo == NULL)
	return;

    if (instance != NULL) {
#if 0
	PluginInstance* This = (PluginInstance*) instance->pdata;
#endif
	
	if (printInfo->mode == NP_FULL) {
	    /*
	     * PLUGIN DEVELOPERS:
	     *	If your plugin would like to take over
	     *	printing completely when it is in full-screen mode,
	     *	set printInfo->pluginPrinted to TRUE and print your
	     *	plugin as you see fit.  If your plugin wants Netscape
	     *	to handle printing in this case, set
	     *	printInfo->pluginPrinted to FALSE (the default) and
	     *	do nothing.  If you do want to handle printing
	     *	yourself, printOne is true if the print button
	     *	(as opposed to the print menu) was clicked.
	     *	On the Macintosh, platformPrint is a THPrint; on
	     *	Windows, platformPrint is a structure
	     *	(defined in npapi.h) containing the printer name, port,
	     *	etc.
	     */

	    /*
	    void* platformPrint =
		printInfo->print.fullPrint.platformPrint;
	    NPBool printOne =
		printInfo->print.fullPrint.printOne;
	     */
			
	    /* Do the default*/
	    printInfo->print.fullPrint.pluginPrinted = FALSE;
	}
#if 0
	else {	/* If not fullscreen, we must be embedded */
	    /*
	     * PLUGIN DEVELOPERS:
	     *	If your plugin is embedded, or is full-screen
	     *	but you returned false in pluginPrinted above, NPP_Print
	     *	will be called with mode == NP_EMBED.  The NPWindow
	     *	in the printInfo gives the location and dimensions of
	     *	the embedded plugin on the printed page.  On the
	     *	Macintosh, platformPrint is the printer port; on
	     *	Windows, platformPrint is the handle to the printing
	     *	device context.
	     */

	    NPWindow* printWindow =
		&(printInfo->print.embedPrint.window);
	    void* platformPrint =
		printInfo->print.embedPrint.platformPrint;
	}
#endif
    }
}
