/*
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* $XFree86: xc/programs/dpsinfo/dpsinfo.c,v 1.3 2001/04/26 21:09:46 dawes Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/X.h>
#include <DPS/XDPS.h>
#include <X11/Xlib.h>
#include <DPS/XDPSlib.h>
#include <DPS/dpsXclient.h>
#include <DPS/psops.h>

#include "iwraps.h"

char *ProgramName;

#define BUFSIZE 512
char buf[BUFSIZE + 1];
#define NUMFONTTYPES 64
int fonttypes[NUMFONTTYPES];

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-display dpy] [-debug]\n",
            ProgramName);
    exit(1);
}

int 
main(int argc, char *argv[])
{
    Display *dpy;
    GC gc;
    DPSContext ctx, tctx;
    char *displayname = NULL;
    int debug = 0;
    int i;

    ProgramName = argv[0];
    buf[BUFSIZE] = '\0';

    for(i = 1; i < argc; i++) {
	if (!strcmp("-display", argv[i])) {
	    if (++i >= argc) usage();
	    displayname = argv[i];
        }else if (!strcmp("-debug", argv[i])) {
            debug = 1;
	} else {
	    usage();
        }
    }

    dpy = XOpenDisplay(displayname);
    if (dpy == NULL) {
	fprintf(stderr, "%s: Can't open display.\n", 
                ProgramName);
	exit(1);
    }

    gc = XCreateGC(dpy, RootWindow (dpy, DefaultScreen (dpy)), 0, NULL);

    XDPSNXSetClientArg(XDPSNX_AUTO_LAUNCH, (void*)True);
    
    ctx = XDPSCreateSimpleContext(dpy, RootWindow(dpy, DefaultScreen(dpy)), 
                                  gc, 0, 0,
                                  DPSDefaultTextBackstop,
                                  DPSDefaultErrorProc, NULL);
    
    if (ctx == NULL) {
	fprintf (stderr, "%s: no DPS extension or agent found.\n",
                 ProgramName);
        exit(1);
    }

    if(debug) {
        tctx = DPSCreateTextContext(DPSDefaultTextBackstop,
                                    DPSDefaultErrorProc);
        DPSChainContext(ctx, tctx);
    }

    DPSSetContext(ctx);

    printf("DPS protocol version: %d\n", XDPSGetProtocolVersion(dpy));
    PSlanguagelevel(&i);
    printf("Interpreter language level: %d\n", i);
    memset(buf, 0, BUFSIZE);
    PSproductReturn(BUFSIZE, buf);
    printf("Product: %s, ", buf);
    memset(buf, 0, BUFSIZE);
    PSversion(BUFSIZE, buf);
    printf("version %s, ", buf);
    PSserialnumber(&i);
    printf("serial number: %d\n", i);
    PSfonttypes(NUMFONTTYPES, fonttypes);
    printf("Supported font types: ");
    for(i=0; i<NUMFONTTYPES; i++) {
        if(fonttypes[i] < 0) {
            break;
        } else {
            printf("%d ", fonttypes[i]);
        }
    }
    printf("\n");
    return 0;
}

    
    
