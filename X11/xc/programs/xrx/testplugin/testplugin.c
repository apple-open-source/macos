/* $Xorg: testplugin.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
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
 * This is a (not so) basic program to "test" a netscape plugin.
 * It exercises the plugin in a way close (I hope) to how netscape does.
 * It is designed to allow minimal debugging of the plugin, with the
 * possibility of using purify.
 *
 * Arnaud Le Hors
 */

#include <stdio.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/ScrolledW.h>
#include <Xm/PushB.h>
#ifdef SUPPORT_EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#if defined(SYSV) || defined(SVR4)
#include <string.h>
#else
#include <strings.h>
#endif

#include "npapi.h"

/* default mime type */
#ifndef MIMETYPE
#define MIMETYPE "undefined"
#endif

/* define how many bytes should be read at most */
#ifndef READNBYTES
#define READNBYTES 100
#endif

/* default plugin size */
#define DEFAULT_WIDTH 50
#define DEFAULT_HEIGHT 50


#ifdef PLUGIN_TRACE
#define PRINTTRACE(msg) fprintf(stderr, msg)
#else
#define PRINTTRACE(msg)
#endif

/* different possible states */
typedef enum {
    SETUP, SETUPWINDOW, PROCESSINPUT, SETUPSTREAM, PROCESSREPLY, DONE
} State;

/* main structure */
typedef struct {
    State state;
    char **plugin_argn;
    char **plugin_argv;
    int plugin_argc;
    uint32 width;
    uint32 height;
    Widget plugin;
    char *src;
    NPStream **streams;
    int nstreams;
    int streams_len;
    Bool setup_window;
} AppData;

/* create plugin instance */
void
SetupPlugin(NPP instance)
{
    AppData *data = (AppData *)instance->ndata;
    NPError status;

    /* NPError NPP_Initialize(void) */
    PRINTTRACE(" NPP_Initialize\n");
    status = NPP_Initialize();

    /*
     * NPError NPP_New(NPMIMEType *pluginType, NPP instance, uint16 mode,
     *       int16 argc, char *argn[], char *argv[], NPSavedData *saved)
     */
    PRINTTRACE(" NPP_New\n");
    status = NPP_New(MIMETYPE, instance, 0,
		     data->plugin_argc, data->plugin_argn, data->plugin_argv,
		     NULL);
}


void
SetupPluginWindow(NPP instance)
{
    AppData *data = (AppData *)instance->ndata;
    NPError status;
    NPSetWindowCallbackStruct window_data;
    NPWindow window;

    /* setup window structure */
    window_data.type = 0;	/* what's this supposed to be ? */
    window_data.display = XtDisplay(data->plugin);
    window_data.visual = DefaultVisual(window_data.display,
				       DefaultScreen(window_data.display));
    window_data.colormap = DefaultColormap(window_data.display,
					   DefaultScreen(window_data.display));
    window_data.depth = DefaultDepth(window_data.display,
				     DefaultScreen(window_data.display));

    window.window = (void *) XtWindow(data->plugin);
    window.x = 0;
    window.y = 0;
    window.width = data->width;
    window.height = data->height;
    window.ws_info = (void *) &window_data;

    /* NPError NPP_SetWindow(NPP instance, NPWindow *window) */
    PRINTTRACE(" NPP_SetWindow\n");
    status = NPP_SetWindow(instance, &window);
}

void
MakeStream(NPP instance, const char *url, const char *window, FILE *fp)
{
    AppData *data = (AppData *)instance->ndata;
    NPStream *stream, **ptr;
    char *urlcopy;

    /* setup a stream */
    stream = (NPStream *) malloc(sizeof(NPStream));
    if (stream == NULL) {
	fprintf(stderr, "cannot malloc enough memory: %d\n",
		sizeof(NPStream));
	exit(1);
    }

    urlcopy = (char *) malloc(strlen(url) + 1);
    if (urlcopy == NULL) {
	fprintf(stderr, "cannot malloc enough memory: %d\n",
		strlen(url) + 1);
	exit(1);
    }
    strcpy(urlcopy, url);

    stream->ndata = (void *) fp;
    stream->pdata = NULL;
    stream->url = urlcopy;
    /* if target window different from plugin window throw reply away */
    stream->end = (window == NULL) ? 0 : -1;
    stream->lastmodified = 0;	/* I don't have this info */
    stream->notifyData = NULL;	/* I don't support this for now */

    /* add it to the queue */
    data->nstreams++;
    if (data->nstreams > data->streams_len) {
	ptr = (NPStream **) realloc(data->streams,
				    sizeof(NPStream *) * data->nstreams);
	if (ptr == NULL) {
	    fprintf(stderr, "cannot malloc enough memory: %d\n",
		    sizeof(NPStream *) * data->nstreams);
	    exit(1);
	}
	data->streams = ptr;
	data->streams_len = data->nstreams;
    }
    data->streams[data->nstreams - 1] = stream;
}

void
DestroyStream(NPP instance, int i)
{
    AppData *data = (AppData *)instance->ndata;
    /*
     * NPError NPP_DestroyStream(NPP instance, NPStream *stream,
     *      NPError reason)
     */
    PRINTTRACE(" NPP_DestroyStream\n");
    NPP_DestroyStream(instance, data->streams[i], NPRES_DONE);
    if (data->streams[i]->url != NULL)
	free((void *)data->streams[i]->url);
    free(data->streams[i]);
}

void
CloseStream(NPP instance, int i)
{
    AppData *data = (AppData *)instance->ndata;
    FILE *fp = (FILE *) data->streams[i]->ndata;

    DestroyStream(instance, 0);
    /* remove from queue */
    data->nstreams--;
    for (i = 0; i < data->nstreams; i++)
	data->streams[i] = data->streams[i + 1];
}

void
WriteStreamProc(NPP instance, int *fd, XtInputId *id)
{
    AppData *data = (AppData *)instance->ndata;
    FILE *fp = (FILE *) data->streams[0]->ndata;
    NPError status;
    char buf[BUFSIZ];
    int32 readysize, readb, len;

    /* int32 NPP_WriteReady(NPP instance, NPStream *stream) */
    PRINTTRACE(" NPP_WriteReady\n");
    readysize = NPP_WriteReady(instance, data->streams[0]);

    /* make sure we won't read more than our buf size */
    if (readysize > BUFSIZ)
	readysize = BUFSIZ;

    /* force a smaller read size so NPP_Write is called more than once,
       this is just for the purpose of testing! */
    if (readysize > READNBYTES)
	len = READNBYTES;
    else			/* take half of the given value */
	len = (readysize >> 1);

    /* read next chunk of data */
    len = fread(buf, sizeof(char), len, fp);

    if (len != 0) {		/* send it to plugin */
	/*
	 * int32 NPP_Write(NPP instance, NPStream *stream, int32 offset,
	 *     int32 len, void *buf);
	 */
	PRINTTRACE(" NPP_Write\n");
	readb = NPP_Write(instance, data->streams[0], data->streams[0]->end,
			  len, buf);

	/* plugin should have read all of it since we read less than readysize,
	 * if it read less than what it claimed it was ready to use bark */
	if (readb < len) {
	    fprintf(stderr,
		    "plugin claimed to be ready to read %d but only read %d\n",
		    readysize, readb);
	    exit(1);
	}
	data->streams[0]->end += len;
    } else {			/* no more to read */
	XtRemoveInput(*id);
	data->state = DONE;
    }
}

/* destroy plugin */
void
ShutdownPlugin(NPP instance)
{
    AppData *data = (AppData *)instance->ndata;
    NPSavedData *saved_data = NULL;

    if (data->streams != NULL) {
	int i;
	for (i = 0; i < data->nstreams; i++)
	    DestroyStream(instance, i);
	free(data->streams);
    }
    PRINTTRACE(" NPP_Destroy\n");
    NPP_Destroy(instance, &saved_data);
    if (saved_data != NULL)
	free(saved_data);	/* so this is not reported as memory leak */
    PRINTTRACE(" NPP_Shutdown\n");
    NPP_Shutdown();
}

/* quit callback: shutdown plugin and exit */
void
QuitCB(Widget widget, XtPointer closure, XtPointer call_data)
{
    ShutdownPlugin((NPP) closure);
    exit(0);
}

/* resize handler: like Netscape change the plugin widget */
void
ResizeEH(Widget toplevel, XtPointer client_data, XEvent *event, Boolean *cont)
{
    if (event->type == ConfigureNotify) {
	NPP instance = (NPP) client_data;
	AppData *data = (AppData *)instance->ndata;
	Arg args[10];
	int n;
	Widget oldplugin, scrolledW;
	Dimension width, height;

	/* remove the current plugin from its parent's geometry management */
	oldplugin = data->plugin;

	if (XtIsRealized(oldplugin) == False)
	    return;

	/* we only want to deal with resize */
	if (event->xconfigure.x != 0 || event->xconfigure.y != 0)
	    return;

	scrolledW = XtParent(XtParent(oldplugin));
	XtUnmanageChild(oldplugin);

	/* create a copy of the plugin widget */
	n = 0;
	XtSetArg(args[n], XmNwidth, &width); n++;
	XtSetArg(args[n], XmNheight, &height); n++;
	XtGetValues(oldplugin, args, n);

	n = 0;
	XtSetArg(args[n], XmNwidth, width); n++;
	XtSetArg(args[n], XmNheight, height); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	data->plugin = XmCreateDrawingArea(scrolledW, "plugin", args, n);

	n = 0;
	XtSetArg(args[n], XmNworkWindow, data->plugin); n++;
	XtSetValues(scrolledW, args, n);
	XtManageChild(data->plugin);

	/* then destroy the old plugin widget */
	XtDestroyWidget(oldplugin);

	data->setup_window = True;
    }
}

/* build a GUI with a form containing a ScrolledWindow over a DrawingArea for
   the plugin and a button to quit */
Widget
BuildGUI(Widget toplevel, Dimension width, Dimension height, NPP instance)
{
    Widget form, scrolledW, plugin, button;
    Arg args[10];
    int n;

    XtAddRawEventHandler(toplevel, StructureNotifyMask, False,
			 ResizeEH, instance);

    form = XmCreateForm(toplevel, "form", NULL, 0);

    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    button = XmCreatePushButton(form, "quit", args, n);
    XtAddCallback(button, XmNactivateCallback, QuitCB, instance);

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, button); n++;
    XtSetArg(args[n], XmNscrollBarDisplayPolicy, XmSTATIC); n++;
    XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
    scrolledW = XmCreateScrolledWindow(form, "scrolledWindow", args, n);

    n = 0;
    XtSetArg(args[n], XmNwidth, width); n++;
    XtSetArg(args[n], XmNheight, height); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
    plugin = XmCreateDrawingArea(scrolledW, "plugin", args, n);

    n = 0;
    XtSetArg(args[n], XmNworkWindow, plugin); n++;
    XtSetValues(scrolledW, args, n);

    XtManageChild(plugin);
    XtManageChild(scrolledW);
    XtManageChild(button);
    XtManageChild(form);
    return plugin;
}

/* parse program arguments and return plugin arguments list */
void
ParseProgramArgs(int pg_argc, char **pg_argv,
		 int *argc_ret, char ***argn_ret, char ***argv_ret)
{
    int argc, i;
    char **argn, **argv, *ptr;

    argn = (char **)malloc(sizeof(char *) * pg_argc);
    argv = (char **)malloc(sizeof(char *) * pg_argc);
    if (argn == NULL || argv == NULL) {
	fprintf(stderr, "cannot malloc enough memory: %d\n",
		sizeof(char *) * pg_argc);
	exit(1);
    }
    argc = 0;
    for (i = 0; i < pg_argc; i++) {
	/* look for arguments of the form "name=value", skip malformed ones */
	ptr = strchr(pg_argv[i], '=');
	if (ptr != NULL) {
	    /* "split" the string and store both parts */
	    *ptr = '\0';
	    argn[argc] = pg_argv[i];
	    argv[argc] = ptr + 1;
	    argc++;
	} else
	    fprintf(stderr, "invalid argument: %s\n", pg_argv[i]);
    }
    *argn_ret = argn;
    *argv_ret = argv;
    *argc_ret = argc;
}


/* retreive src, width, and height arguments value */
void
ParsePluginArgs(int argc, char **argn, char **argv,
		char **src_ret, int *width_ret, int *height_ret)
{
    int i;

    *src_ret = NULL;
    *width_ret = DEFAULT_WIDTH;
    *height_ret = DEFAULT_HEIGHT;
    /* dumb look up */
    for (i = 0; i < argc; i++) {
	if (strcasecmp(argn[i], "src") == 0)
	    *src_ret = argv[i];
	else if (strcasecmp(argn[i], "width") == 0)
	    *width_ret = atoi(argv[i]);
	else if (strcasecmp(argn[i], "height") == 0)
	    *height_ret = atoi(argv[i]);
    }
}

/* main routine exercising the plug-in within a state machine */
Boolean
MainWorkProc(XtPointer closure)
{
    NPP instance = (NPP) closure;
    AppData *data = (AppData *)instance->ndata;
    NPError status;

    switch (data->state) {
    case SETUP:
	/* create plugin instance */
	SetupPlugin(instance);
	data->state = SETUPWINDOW;
	break;

    case SETUPWINDOW:
	/* create plugin window */
	SetupPluginWindow(instance);
	data->state = PROCESSINPUT;
	break;

    case PROCESSINPUT:
	/* set default next state,
	   may be overriden by the call to GetURL */
	data->state = DONE;

	/* perform GET request on input data */
	if (data->src != NULL) {
	    status = NPN_GetURL(instance, data->src, NULL);
	    data->state = SETUPSTREAM;
	}
	break;

    case SETUPSTREAM:
	/* if there is a GET request in progress setup to process reply */
	if (data->streams != NULL && data->nstreams != 0) {
	    if (data->streams[0]->end == -1) /* throw it away */
		fprintf(stderr,
			"GetURL request response is thrown away (sorry)\n");
	    else {
		/*
		 * NPError NPP_NewStream(NPP instance, NPMIMEType type,
		 *       NPStream *stream, NPBool seekable, uint16* stype)
		 */
		PRINTTRACE(" NPP_NewStream\n");
		status = NPP_NewStream(instance, MIMETYPE, data->streams[0],
				       FALSE, (uint16*)NP_NORMAL);
		/* and add a handler for it */
		XtAppAddInput(XtWidgetToApplicationContext(data->plugin),
			      fileno((FILE *)(data->streams[0]->ndata)),
			      (XtPointer) XtInputReadMask,
			      (XtInputCallbackProc) WriteStreamProc,
			      (XtPointer) instance);
	    }
	}
	data->state = PROCESSREPLY;
	break;

    case PROCESSREPLY:
	/* if no more query responses are pending move to next state */
	if (data->streams == NULL || data->nstreams == 0 ||
	    data->streams[0]->end == -1)
	    data->state = DONE; /* no more to be read */
	break;

    case DONE:
	/* if there is a registered stream close it */
	if (data->streams != NULL && data->nstreams != 0) {
	    CloseStream(instance, 0);
	    if (data->nstreams > 0) {
		/* then call for process of next reply */
		data->state = SETUPSTREAM;
		return False;	/* work proc is to be called again */
	    }
	}
	/* then exit from state machine */
	return True;		/* work proc is to be removed */
    }
    return False;		/* work proc is to be called again */
}

main(int argc, char **argv)
{
    Widget toplevel, plugin;
    XtAppContext app_context;
    NPP_t instance;
    AppData app_data;
    char **plugin_argn, **plugin_argv;
    int plugin_argc;
    char *src;
    int width, height;
    XEvent event;

    if (argc < 2 || !strncmp(argv[1], "-h", 2) || !strncmp(argv[1], "-?", 2)) {
	fprintf(stderr,
		"Usage: %s src=url [width=w] [height=h] [name=value ...]\n",
		argv[0]); 
	exit(1);
    }

    /* build GUI */
    toplevel = XtAppInitialize(&app_context, "Plugin Test", 0, 0,
			       &argc, argv, 0, 0, 0);
#ifdef SUPPORT_EDITRES
    XtAddEventHandler(toplevel, 0, True, _XEditResCheckMessages, NULL);
#endif

    ParseProgramArgs(argc - 1, argv + 1,
		     &plugin_argc, &plugin_argn, &plugin_argv);

    ParsePluginArgs(plugin_argc, plugin_argn, plugin_argv,
		    &src, &width, &height);

    /* make sure width and height are valid */
    if (width < 0 || height < 0) {
	fprintf(stderr, "invalid size specification (< 0) width=%d height=%d",
		width, height); 
	exit(1);
    }

    if (src == NULL) {
	fprintf(stderr, "source argument required!\n"); 
	exit(1);
    }

    plugin =
	BuildGUI(toplevel, (Dimension)width, (Dimension)height, &instance);

    /* setup instance structure */
    app_data.state = SETUP;
    app_data.plugin_argn = plugin_argn;
    app_data.plugin_argv = plugin_argv;
    app_data.plugin_argc = plugin_argc;
    app_data.width = (uint32) width;
    app_data.height = (uint32) height;
    app_data.plugin = plugin;
    app_data.src = src;
    app_data.streams = NULL;
    app_data.nstreams = 0;
    app_data.streams_len = 0;
    app_data.setup_window = False;
    instance.ndata = (void *) &app_data;

    /* register main work proc (heart of the beast) */
    XtAppAddWorkProc(app_context, MainWorkProc, &instance);

    XtRealizeWidget(toplevel);

    /* wait for user to quit */
    for (;;) {
	XtAppNextEvent (app_context, &event);
	XtDispatchEvent (&event);
	if (app_data.setup_window) {
	    SetupPluginWindow(&instance);
	    app_data.setup_window = False;
	}
    }
}


/*
 * Netscape Methods
 */

void *
NPN_MemAlloc(uint32 size)
{
    return malloc(size);
}

uint32
NPN_MemFlush(uint32 size)
{
    /* do nothing */
    return 0;
}

void
NPN_MemFree(void *ptr)
{
    free(ptr);
}

/* This is an asynchronous function, so perform request, setup stream for
 * reply, and put it in the queue. */
NPError
NPN_GetURL(NPP instance, const char *url, const char *window)
{
    AppData *data = (AppData *)instance->ndata;
    char buf[BUFSIZ];
    FILE *fp;

    if (url == NULL)
	return NPERR_INVALID_URL;

    /* perform request using "www" */
    sprintf(buf, "www -source \"%s\"", url);
    fp = popen(buf, "r");
    if (fp == NULL) {
	fprintf(stderr, "GetURL request failed on: %s\n", url);
	return NPERR_INVALID_URL;
    }

    MakeStream(instance, url, window, fp);

    return NPERR_NO_ERROR;
}
