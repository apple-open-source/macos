/* $Xorg: helper.c,v 1.4 2001/02/09 02:05:57 xorgcvs Exp $ */
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

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/extensions/security.h>
#include <X11/Intrinsic.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>		/* for MAXHOSTNAMELEN */
/* and in case we didn't get it from the headers above */
#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN 256
#endif

#include "Rx.h"
#include "RxI.h"		/* for Strncasecmp */
#include "XUrls.h"
#include "GetUrl.h"
#include "XAuth.h"
#include "XDpyName.h"
#include "Prefs.h"


#define DEFAULT_TIMEOUT 300
#define NO_TIMEOUT 0

/* a few global */
Display *pdpy;
int security_event_type_base;
XSecurityAuthorization ui_auth_id;
XSecurityAuthorization print_auth_id;

#define RevokedEventType \
	(security_event_type_base + XSecurityAuthorizationRevoked)

/* read file in memory and return character stream */
static char *
ReadFile(char *filename)
{
    int fd;
    struct stat	st;
    char *stream;

    if ((fd = open(filename, O_RDONLY)) < 0)
	return(0);
    fstat(fd, &st);
    stream = (char *)malloc(st.st_size+1);
    if (stream == 0)
	return(0);
    if ((st.st_size = read(fd, stream, st.st_size)) < 0) {
	free(stream);
	return(0);
    }
    close(fd);
    stream[st.st_size] = '\0';
    return stream;
}


static Display *
OpenXPrintDisplay(Display *dpy, char **printer_return)
{
    char *pdpy_name;
    Display *pdpy;
    pdpy_name = GetXPrintDisplayName(printer_return);
    if (pdpy_name != NULL) {
	pdpy = XOpenDisplay(pdpy_name);
	free(pdpy_name);
    } else {
	/* no server specified,
	   let's see if the video server could do it */
	int dummy;
	if (XQueryExtension(dpy, "XpExtension", &dummy, &dummy, &dummy))
	    pdpy = dpy;
	else
	    pdpy = NULL;
    }
    return pdpy;
}

static void
CloseXPrintDisplay(Display *dpy, Display *pdpy)
{
    if (pdpy != NULL && pdpy != dpy)
        XCloseDisplay(pdpy);
}

/* process the given RxParams and make the RxReturnParams */
static int
ProcessUIParams(Display *dpy,
		Boolean trusted, Boolean use_fwp, Boolean use_lbx,
		RxParams *in, RxReturnParams *out, char **x_ui_auth_ret)
{
    char *fwp_dpyname = NULL;
    XSecurityAuthorization dum;
    char *x_ui_auth = NULL;
    Display *rdpy;
    char *real_display;

    if (in->x_ui_auth[0] != 0)
	GetXAuth(dpy, in->x_ui_auth[0], in->x_ui_auth_data[0],
		 trusted, None, DEFAULT_TIMEOUT, True,
		 &x_ui_auth, &ui_auth_id, &security_event_type_base);
    else if (in->x_auth[0] != 0)
	GetXAuth(dpy, in->x_auth[0], in->x_auth_data[0],
		 trusted, None, DEFAULT_TIMEOUT, True,
		 &x_ui_auth, &ui_auth_id, &security_event_type_base);

    /* make sure we use the server the user wants us to use */
    rdpy = dpy;
    real_display = getenv("XREALDISPLAY");
    if (real_display != NULL) {
	rdpy = XOpenDisplay(real_display);
	if (rdpy == NULL)
	    rdpy = dpy;
    }

    /* let's see whether we have a firewall proxy */
    if (use_fwp) {
	fwp_dpyname = GetXFwpDisplayName(DisplayString(rdpy));
	if (fwp_dpyname == NULL)
	    /*
	     * We were supposed to use the firewall proxy but we
	     * couldn't get a connection.  There is no need to
	     * continue.
	     */
	    return 1;
    }

    if (fwp_dpyname != NULL) {
	out->ui = GetXUrl(fwp_dpyname, x_ui_auth, in->action);
	free(fwp_dpyname);
    } else
	out->ui = GetXUrl(DisplayString(rdpy), x_ui_auth, in->action);

    if (in->x_ui_lbx == RxTrue) {
	if (use_lbx == True) {
	    int dummy;
	    /* let's see whether the server supports LBX or not */
	    if (XQueryExtension(rdpy, "LBX", &dummy, &dummy, &dummy)) {
		out->x_ui_lbx = RxTrue;

 		/* let's get a key for the proxy now */
		if (in->x_ui_lbx_auth[0] != 0) {
		    GetXAuth(dpy, in->x_ui_lbx_auth[0],
			     in->x_ui_lbx_auth_data[0],
			     trusted, None, DEFAULT_TIMEOUT, False,
			     &out->x_ui_lbx_auth, &dum, &dummy);
		} else if (in->x_auth[0] != 0)
		    GetXAuth(dpy, in->x_auth[0], in->x_auth_data[0],
			     trusted, None, DEFAULT_TIMEOUT, False,
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

    if (rdpy != dpy)
	XCloseDisplay(rdpy);

    *x_ui_auth_ret = x_ui_auth;

    return 0;
}


static int
ProcessPrintParams(Display *dpy,
		   Boolean trusted, Boolean use_fwp, Boolean use_lbx,
		   RxParams *in, RxReturnParams *out, char *x_ui_auth)
{
    char *printer = NULL;
    char *auth = NULL;
    XSecurityAuthorization dum;
    char *pfwp_dpyname = NULL;
    int dummy;

    pdpy = OpenXPrintDisplay(dpy, &printer);
    if (pdpy == NULL) {
	fprintf(stderr, "Warning: Cannot setup X printer as requested, \
no server found\n");
	return 0;
    }

    /* create a key only when the video server is not the print
       server or when we didn't create a key yet */
    if (pdpy != dpy || x_ui_auth == NULL) {
	/* if the application has a GUI we can't guess when it will really
	   connect to the print server so we need an auth which never expires,
	   on the other hand if the application happens not to have any GUI
	   we can expect it to connect to the print server pretty soon */
	unsigned int timeout = ui_auth_id != 0 ? NO_TIMEOUT : DEFAULT_TIMEOUT;
	if (in->x_print_auth[0] != 0)
	    GetXAuth(pdpy, in->x_print_auth[0], in->x_print_auth_data[0],
		     trusted, None, timeout, False,
		     &auth, &print_auth_id, &dummy);
	else if (in->x_auth[0] != 0)
	    GetXAuth(pdpy, in->x_auth[0], in->x_auth_data[0],
		     trusted, None, timeout, False,
		     &auth, &print_auth_id, &dummy);
    }
    /* let's see whether we have a firewall proxy */
    if (use_fwp) {
	pfwp_dpyname = GetXFwpDisplayName(DisplayString(pdpy));
	if (pfwp_dpyname == NULL)
	    /*
	     * We were supposed to use the firewall proxy but we
	     * couldn't get a connection.  There is no need to
	     * continue.
	     */
	    return 1;
    }

    if (pfwp_dpyname != NULL) {
	out->print = GetXPrintUrl(pfwp_dpyname, printer, auth, in->action);
	free(pfwp_dpyname);
    } else
	out->print = GetXPrintUrl(DisplayString(pdpy), printer, auth, in->action);

    if (auth != NULL)
	free(auth);

    if (in->x_print_lbx == RxTrue) {
	if (use_lbx == True) {
	    if (pdpy == dpy && in->x_ui_lbx == RxTrue) {
		/* the video server is the print server and we already
		   know whether it supports LBX or not */
		out->x_print_lbx = out->x_ui_lbx;
	    } else {
		/* let's see whether the server supports LBX or not */
		if (XQueryExtension(pdpy, "LBX", &dummy, &dummy, &dummy)) {
		    out->x_print_lbx = RxTrue;

		    /* let's get a key for the proxy now */
		    if (in->x_print_lbx_auth[0] != 0) {
			GetXAuth(pdpy, in->x_print_lbx_auth[0],
				 in->x_print_lbx_auth_data[0],
				 trusted, None, DEFAULT_TIMEOUT, False,
				 &out->x_print_lbx_auth, &dum, &dummy);
		    } else if (in->x_auth[0] != 0)
			GetXAuth(pdpy, in->x_auth[0], in->x_auth_data[0],
				 trusted, None, DEFAULT_TIMEOUT, False,
				 &out->x_print_lbx_auth, &dum, &dummy);
		} else {
		    out->x_print_lbx = RxFalse;
		    fprintf(stderr, "Warning: Cannot setup LBX as \
requested, LBX extension not supported\n");
		}
	    }
	} else
	    out->x_print_lbx = RxFalse;
    } else			/* it's either RxFalse or RxUndef */
	out->x_print_lbx = in->x_print_lbx;

    if (printer != NULL)
        free(printer);

    return 0;
}

static int
ProcessParams(Display *dpy, Preferences *prefs, RxParams *in,
	      RxReturnParams *out)
{
    char *x_ui_auth = NULL;
    char webserver[MAXHOSTNAMELEN];
    Boolean trusted, use_fwp, use_lbx;
    int return_value = 0;

    /* init return struture */
    memset(out, 0, sizeof(RxReturnParams));
    out->x_ui_lbx = RxUndef;
    out->x_print_lbx = RxUndef;
    out->action = in->action;

    if (in->embedded != RxUndef)
	out->embedded = RxFalse; /* we cannot perform embbeding from helper */
    else
	out->embedded = RxUndef;

    out->width = in->width;
    out->height = in->height;	

    ComputePreferences(prefs,
       ParseHostname(in->action, webserver, MAXHOSTNAMELEN) ? webserver : NULL,
       &trusted, &use_fwp, &use_lbx);

    if (in->ui[0] == XUI)	/* X display needed */
	return_value = ProcessUIParams(dpy, trusted, use_fwp, use_lbx, 
				in, out, &x_ui_auth);

    if (in->print[0] == XPrint) /* XPrint server needed */
	return_value = ProcessPrintParams(dpy, trusted, use_fwp, use_lbx, 
				in, out, x_ui_auth);
	
    if (x_ui_auth != NULL)
	free(x_ui_auth);

    return return_value;
}

#define CONTENT_TYPE "Content-type"
#define TEXT_PLAIN "text/plain"

/* parse CGI reply looking for error status line,
 * and return following message
 */
int
ParseReply(char *reply, int reply_len, char **reply_ret, int *reply_len_ret)
{
    char *ptr, *end;
    int status = 0;

    /* look for content-type field */
    end = reply + reply_len;
    ptr = reply;
    while (Strncasecmp(ptr, CONTENT_TYPE, sizeof(CONTENT_TYPE) - 1) != 0 &&
	   ptr < end) {
	/* goto the next line */
	while (*ptr != '\n' && ptr < end)
	    ptr++;
	if (ptr != end)
	    ptr++;
    }

    if (ptr < end) {		/* if we found it */
	/* skip to field value */
	ptr += sizeof(CONTENT_TYPE);
	while (isspace(*ptr) && ptr < end)
	    ptr++;
	if (Strncasecmp(ptr, TEXT_PLAIN, sizeof(TEXT_PLAIN) - 1) == 0) {
	    /* go to the next line */
	    while (*ptr != '\n' && ptr < end)
		ptr++;
	    if (ptr == end)	/* input truncated */
		goto exit;
	    ptr++;
	    /* skip the next line should be empty (except for \r) */
	    while (*ptr != '\n' && ptr < end)
		ptr++;
	    if (ptr == end)	/* input truncated */
		goto exit;
	    ptr++;
	    /* now should be the error code */
	    if (isdigit(*ptr)) {
		status = atoi(ptr);
		/* go to the next line */
		while (*ptr != '\n' && ptr < end)
		    ptr++;
		if (ptr == end)	/* input truncated */
		    goto exit;
		ptr++;
		goto exit;
	    } else		/* input truncated */
		goto exit;
	}
    }
exit:
    *reply_ret = ptr;
    *reply_len_ret = reply_len - (ptr - reply);
    return status;
}


/*
 * The following event dispatcher is in charge of revoking the print
 * authorization when the video authorization is (which means the application
 * using it is gone).
 * We can then exit since we do not have anything else to do.
 */
Boolean 
RevokeD(XEvent *xev)
{
    if (xev->type == RevokedEventType) { /* should always be true */
	XSecurityAuthorizationRevokedEvent *ev;
	ev = (XSecurityAuthorizationRevokedEvent *) xev;
	if (ev->auth_id == ui_auth_id) { /* should always be true */
	    XSecurityRevokeAuthorization(pdpy, print_auth_id);
	    exit(0);
	}
    }
    return True;
}

int
main(int argc, char *argv[])
{
    char *stream;
    char **rx_argn, **rx_argv;
    int rx_argc;
    RxParams params;
    RxReturnParams return_params;
    char *query, *reply, *msg;
    int reply_len, msg_len;
    Widget toplevel;
    XtAppContext app_context;
    Preferences prefs;

    /* init global variables */
    pdpy = NULL;
    security_event_type_base = 0;
    ui_auth_id = 0;
    print_auth_id = 0;

    rx_argc = 0;

    toplevel = XtAppInitialize(&app_context, "Xrx", 0, 0,
#if XtSpecificationRelease > 4
			       &argc,
#else
			       (Cardinal *)&argc,
#endif
			       argv, 0, 0, 0);

    if (argc < 2) {
	fprintf(stderr, "Usage: %s <file>\n", argv[0]);
	exit(1);
    }

    if ((stream = ReadFile(argv[1])) == 0) {
	fprintf(stderr, "%s: cannot open file %s\n", argv[0], argv[1]);
	exit(1);
    }

    if (RxReadParams(stream, &rx_argn, &rx_argv, &rx_argc) != 0) {
	fprintf(stderr, "%s: invalid file %s\n", argv[0], argv[1]);
	exit(1);
    }

    RxInitializeParams(&params);

    if (RxParseParams(rx_argn, rx_argv, rx_argc, &params, 0) != 0) {
	fprintf(stderr, "%s: invalid params\n", argv[0]);
	exit(1);
    }
    GetPreferences(toplevel, &prefs);

    /* set up return parameters */
    if (ProcessParams(XtDisplay(toplevel), &prefs,
		      &params, &return_params) != 0) {
	fprintf(stderr, "%s: failed to process params\n", argv[0]);
	exit(1);
    }

    /* make query */
    query = RxBuildRequest(&return_params);
    if (query == NULL) {
	fprintf(stderr, "%s: failed to make query\n", argv[0]);
	exit(1);
    }

    /* perform GET request */
    if (GetUrl(query, &reply, &reply_len) != 0) {
	fprintf(stderr, "%s: GET request failed\n", argv[0]);
	exit(1);
    }

    if (reply) {
	if (ParseReply(reply, reply_len, &msg, &msg_len) != 0) {
	    fprintf(stderr, "%s: Remote execution failed\n", argv[0]);
	    fwrite(msg, msg_len, 1, stderr);
	}
    }

    /* if we didn't create any authorization for printing or
       if the application is only using the print server, exit here */
    if (print_auth_id == 0 || ui_auth_id == 0)
	exit (0);

    /* otherwise, free as much as we can now */
    if (rx_argc != 0) {
	int i;
	for (i = 0; i < rx_argc; i++) {
	    free(rx_argn[i]);
	    free(rx_argv[i]);
	}
	free(rx_argn);
	free(rx_argv);
    }
    RxFreeParams(&params);
    RxFreeReturnParams(&return_params);

    free(stream);
    free(query);
    if (reply)
	free(reply);

    FreePreferences(&prefs);

    /* and setup event dispatcher so we catch the video key revocation */
    XtSetEventDispatcher(XtDisplay(toplevel), RevokedEventType, RevokeD);

    /* then wait for it... */
    XtAppMainLoop(app_context);
}
