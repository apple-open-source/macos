/* dpsexec.c
 *
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/programs/dpsexec/dpsexec.c,v 1.7 2002/03/05 21:50:15 herrb Exp $ */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <X11/X.h>
#include <DPS/XDPS.h>
#include <X11/Xlib.h>
#include <DPS/XDPSlib.h>
#include <DPS/dpsXclient.h>

#ifdef __QNX__
#include <sys/select.h>
#endif

#define W_HEIGHT	512
#define W_WIDTH		512

#ifdef _NO_PROTO
#define ARGCAST int
#else
#define ARGCAST void *
#endif

static void MyStatusProc (ctxt, code)
    DPSContext ctxt;
    int code;
{
    if (code == PSZOMBIE) {
	/* Zombie event means context died */
	exit(0);
    }
}

int main(argc, argv)
    int argc;
    char **argv;
{
    char *displayname = "";
    Display *dpy;
    int i;
    char buf[1000];
    XEvent ev;
    GC gc;
    long mask;
    int sync = 0;
    int backingStore = 0;
    int exe = 1;
    Window win;
    XSetWindowAttributes xswa;
    DPSContext ctxt;
    XWMHints *hints;
    int root = 0;
    int width = W_WIDTH;
    int height = W_HEIGHT;
    Drawable drawable = None;

    for (i = 1;  i < argc;  i++) {
	if (strncmp(argv[i], "-display", strlen(argv[i])) == 0) {
	    i++;
	    displayname = argv[i];
	} else if (strncmp(argv[i], "-sync", strlen(argv[i])) == 0)
	    sync = 1;
	else if (strncmp(argv[i], "-backup", strlen(argv[i])) == 0)
	    backingStore = 1;
	else if (strncmp(argv[i], "-noexec", strlen(argv[i])) == 0)
	    exe = 0;
	else if (strncmp(argv[i], "-root", strlen(argv[i])) == 0)
	    root = 1;
	else if (strncmp(argv[i], "-width", strlen(argv[i])) == 0)
	    width = atoi(argv[++i]);
	else if (strncmp(argv[i], "-height", strlen(argv[i])) == 0)
	    height = atoi(argv[++i]);
	else if (strncmp(argv[i], "-drawable", strlen(argv[i])) == 0)
	    drawable = (Drawable) atoi(argv[++i]);
	else {
	    fprintf(stderr,
	       "usage: %s [-display displayname][-sync][-backup][-noexec]\n",
		    argv[0]);
	    fprintf(stderr,
		    "       [-root][-width w][-height h][-drawable xid]\n");
	    exit(1);
	}
    }

    dpy = XOpenDisplay(displayname);
    if (dpy == NULL) {
	fprintf(stderr, "%s: Can't open display %s!\n", argv[0], displayname);
	exit(1);
    }
    
    if (sync) (void) XSynchronize(dpy, True);

    gc = XCreateGC(dpy, drawable != None ? drawable :
		   RootWindow (dpy, DefaultScreen (dpy)), 0, NULL);
    XSetForeground(dpy, gc, BlackPixel (dpy, DefaultScreen (dpy)));
    XSetBackground(dpy, gc, WhitePixel (dpy, DefaultScreen (dpy)));

    if (root) {
	win = DefaultRootWindow(dpy);
	height = DisplayHeight(dpy, DefaultScreen(dpy));
    } else if (drawable != None) {				
	Window root;
	int x, y;
	unsigned int wwidth, wheight, border, depth;
	win = drawable;
	(void) XGetGeometry(dpy, win, &root, &x, &y, &wwidth,
			    &wheight, &border, &depth);
	height = wheight;
    } else {
	win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
				  3, 385, width, height, 1,
				  BlackPixel(dpy, DefaultScreen(dpy)),
				  WhitePixel(dpy, DefaultScreen(dpy)));

	hints = XAllocWMHints();
	if (hints != NULL) {
	    hints->flags = InputHint;
	    hints->input = False;
	    XSetWMHints(dpy, win, hints);
	    XFree((char *) hints);
	}

	XStoreName(dpy, win, "Display PostScript Executive");
	XSetIconName(dpy, win, "DPS Exec");

	xswa.bit_gravity = SouthWestGravity;
	if (backingStore) xswa.backing_store = WhenMapped;
	else xswa.backing_store = NotUseful;
	xswa.event_mask = 0x0;
	mask = CWBitGravity | CWBackingStore | CWEventMask;
	XChangeWindowAttributes(dpy, win, mask, &xswa);

	XMapWindow(dpy, win);
    }

    /* Make it possible for this client to start a DPS NX agent,
       if "dpsnx.agent" is on the executable search path. */

    (void) XDPSNXSetClientArg(XDPSNX_AUTO_LAUNCH, (ARGCAST) True);

    ctxt = XDPSCreateSimpleContext(dpy, win, gc, 0, height,
				   DPSDefaultTextBackstop,
				   DPSDefaultErrorProc, NULL);

    if (ctxt == NULL) {
	fprintf (stderr, "\ndpsexec: DPS is not available\n");
	fprintf (stderr,
	  "You need an X server with the DPS extension, or a DPS NX agent.\n");
	exit (1);
    }

    DPSSetContext(ctxt);

    /* Allow zombie events to be delivered so application can exit
       if context dies. Detach context so it doesn't wait for a join 
       if it dies */

    XDPSRegisterStatusProc(ctxt, MyStatusProc);
    XDPSSetStatusMask(ctxt, PSZOMBIEMASK, 0, 0);
    DPSPrintf(ctxt, "currentcontext detach ");
    
    if (exe) DPSPrintf(ctxt, "executive");
    DPSPrintf(ctxt, "\n");
    DPSFlushContext(ctxt);
    DPSSuppressBinaryConversion(ctxt, True);
    
    while (1) {
	fd_set fdmask;
	FD_ZERO(&fdmask);
	FD_SET(0, &fdmask);
	FD_SET(ConnectionNumber(dpy), &fdmask);
	DPSFlushContext(ctxt);
	if (select(ConnectionNumber(dpy)+1, &fdmask, NULL, NULL, NULL) < 0)
	    fprintf(stderr, "select() error %d\n", errno);
	else if (FD_ISSET(0, &fdmask)) {
	    /* Read from command line, send to context */
	    if (fgets(buf, 1000, stdin) == NULL) break;
	    DPSWriteData(ctxt, buf, strlen(buf));
	}
	    
        while (XPending(dpy) > 0) {
	    /* No special event handling - just throw them away.
	       Must call XNextEvent to allow DPS status events to
	       be dispatched. */
            XNextEvent(dpy, &ev);
	}
    }

    DPSDestroySpace(DPSSpaceFromContext(ctxt));
    XFlush(dpy);
    return 0;
}
