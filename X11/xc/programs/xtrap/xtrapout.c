/* $XFree86: xc/programs/xtrap/xtrapout.c,v 1.2 2001/12/12 00:43:50 dawes Exp $ */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * Log: xtrapout.c,v $
 * Revision 1.1.4.2  1993/12/14  12:37:28  Kenneth_Miller
 * 	ANSI-standardize code and turn client build on
 * 	[1993/12/09  20:16:01  Kenneth_Miller]
 *
 * Revision 1.1.2.2  1992/04/27  13:51:57  Leela_Obilichetti
 * 	initial load of xtrap clients - from silver BL6 pool
 * 	[92/04/27  13:49:33  Leela_Obilichetti]
 * 
 * EndLog$
 */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1993 by Digital Equipment Corp., 
Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
/*
 *
 *  CONTRIBUTORS:
 *
 *      Dick Annicchiarico
 *      Robert Chesler
 *      Dan Coutu
 *      Gene Durso
 *      Marc Evans
 *      Alan Jamison
 *      Mark Henry
 *      Ken Miller
 *
 */
#define ProgName "xtrapout"
/*
**++
**  FACILITY:  xtrapout - Sample client to test output from XTrap extension
**
**  MODULE DESCRIPTION:
**
**      This is the main module for a sample/test client
**      for the XTrap X11 Server Extension.  It accepts  
**      a script output file, a transport method,
**      and an "events mode" flag (-e) as input,
**      in addition to the standard X arguments (-d, etc.).
**      If no script file is provided, stdout is the default
**      and can be piped to the companion "xtrapin"
**      client (normally used with the -e argument which  
**      sends all core input events to stdout).
**
**
**  AUTHORS:
**
**      Kenneth B. Miller
**
**  CREATION DATE:  March 28, 1990
**
**  DESIGN ISSUES:
**
**      See the companion "xtrapin" client.
**
**      Also, getopt() is used to parse the command
**      line arguments prior to calling XtAppInitialize().
**      This is because DECwindows appears to remove the user-
**      defined arguments from the argv[] vector without actually
**      acting upon them.
**
**
**--
*/

#include <stdio.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>
#include <signal.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <stdlib.h>

#include "XEKeybCtrl.h"

#ifndef vaxc
#define globalref extern
#endif


/* Forward declarations */
static void SetGlobalDone (void );
static void print_req_callback (XETC *tc , XETrapDatum *data , 
    char *my_buf );
static void print_evt_callback (XETC *tc , XETrapDatum *data , 
    char *my_buf );


FILE *ofp;
Bool GlobalDone = False;
XrmOptionDescRec optionTable [] = 
{
    {"-f",     "*script",    XrmoptionSepArg,  (caddr_t) NULL},
    {"-e",     "*eventFlag", XrmoptionSkipArg, (caddr_t) NULL},
    {"-v",     "*verbose",   XrmoptionSkipArg, (caddr_t) NULL},
};

static void SetGlobalDone(void)
{
    GlobalDone = 1L;
    fprintf(stderr,"Process Completed!\n");
    return;
}

static void print_req_callback(XETC *tc, XETrapDatum *data, char *my_buf)
{
    char *req_type;
    req_type = (data->u.req.reqType == XETrapGetExtOpcode(tc) ? "XTrap" :
        XERequestIDToString(data->u.req.reqType,tc));
    fprintf(ofp,"Request: %-19s (%d): length '%ld' client '%d' window=%ld\n", 
        req_type, data->u.req.reqType, (long)data->hdr.count, data->hdr.client, 
        (long)data->u.req.id);
}

static void print_evt_callback(XETC *tc, XETrapDatum *data, char *my_buf)
{
    static Time last_time = 0;
    int delta;

    delta = abs((int)last_time ? data->u.event.u.keyButtonPointer.time -
        (int)last_time  : (int)last_time);
    last_time = data->u.event.u.keyButtonPointer.time;

    /* The "screen" and "root" fields aren't valid until "event"
     * vectoring becomes a reality.  Currently, XTrap intercepts
     * the core events prior to when fields other than rootX, rootY,
     * type, detail, time, and state are filled in.  This will be
     * addressed in the next release of XTrap (3.2?).
     */
    fprintf(ofp,
        "Event: %-15s (%d):det=%d scr=%d (%d,%d) root=%d Msk=%d TS=%d\n",
        XEEventIDToString(data->u.event.u.u.type,tc), data->u.event.u.u.type,
        data->u.event.u.u.detail, data->hdr.screen, /* Not really valid yet */
        data->u.event.u.keyButtonPointer.rootX, 
        data->u.event.u.keyButtonPointer.rootY,
        (int)data->u.event.u.keyButtonPointer.root, /* Not really valid yet */
        (int)data->u.event.u.keyButtonPointer.state,
        (int)delta);
    fflush(ofp);
}
static Boolean eventFlag = False;
static Boolean verboseFlag = False;
static Widget appW;
static Display *dpy;

int
main(int argc, char *argv[])
{
    XETrapGetAvailRep ret_avail;
    XETrapGetCurRep   ret_cur;
    XETC    *tc;
    ReqFlags     requests;
    EventFlags   events;
    XtAppContext app;
    char *tmp = NULL;
    INT16 ch;
    int *popterr;
    char **poptarg;
#ifndef vms
    popterr = &opterr;
    poptarg = &optarg;
#else
    popterr = XEgetopterr();
    poptarg = XEgetoptarg();
#endif

    eventFlag = False;
    ofp = NULL;
    *popterr = 0; /* don't complain about -d (display) */
    while ((ch = getopt(argc, argv, "d:evf:")) != EOF)
    {
        switch(ch)
        {
            case 'e':
                eventFlag = True;
                break;
            case 'v':
                verboseFlag = True;
                break;
            case 'f':
                if ((ofp = fopen(*poptarg,"wb")) == NULL)
                {   /* can't open it */
                    fprintf(stderr,"%s: could not open output file '%s'!\n",
                        ProgName, *poptarg);
                }
                break;
            case 'd':   /* -display, let's let the toolkit parse it */
                break;
            default:
                break;
        }
    }
    ofp = (ofp ? ofp : stdout);

    appW = XtAppInitialize(&app,"XTrap",optionTable,(Cardinal)2L,
        (int *)&argc, (String *)argv, (String *)NULL,(ArgList)&tmp,
        (Cardinal)NULL);

    dpy = XtDisplay(appW);
#ifdef DEBUG
    XSynchronize(dpy, True);
#endif
    fprintf(stderr,"Display:  %s \n", DisplayString(dpy));
    if ((tc = XECreateTC(dpy,0L, NULL)) == False)
    {
        fprintf(stderr,"%s: could not initialize XTrap extension\n",ProgName);
        exit (1L);
    }
    XETrapSetTimestamps(tc,True, False);
    (void)XEGetAvailableRequest(tc,&ret_avail);
    XEPrintAvail(stderr,&ret_avail);
    XEPrintTkFlags(stderr,tc);

    /* Need to prime events/requests initially turning all off  */
    (void)memset(requests,0L,sizeof(requests));
    (void)memset(events,0L,sizeof(events));
    /* Now turn on the ones you really want */
    (void)memset(events,0xFFL,XETrapMaxEvent);
    if (eventFlag == False)
    {   /* doesn't want just events */
        (void)memset(requests,0xFFL,XETrapMaxRequest);
        /* Turn off XTrap Requests for multi-client regression tests & XLib */
        BitFalse(requests, XETrapGetExtOpcode(tc));
        /* Turn off noisy events */
        BitFalse(events, MotionNotify);
    }
    if (verboseFlag == True)
    {   /* want's *all* requests/events */
        (void)memset(requests,0xFFL,XETrapMaxRequest);
        (void)memset(events,0xFFL,XETrapMaxEvent);
    }
    /* Tell the TC about it */
    XETrapSetRequests(tc, True, requests);
    XETrapSetEvents(tc, True, events);
    XETrapSetMaxPacket(tc, True, XETrapMinPktSize); /* just get the minimum */
    
    /* Set up callbacks for data */
    XEAddRequestCBs(tc, requests, print_req_callback, NULL);
    XEAddEventCBs(tc, events, print_evt_callback, NULL);

    (void)XEStartTrapRequest(tc);
    (void)XEGetCurrentRequest(tc,&ret_cur);
    XEPrintCurrent(stderr,&ret_cur);

    /* Add signal handlers so that we clean up properly */
    _InitExceptionHandling((void_function)SetGlobalDone);
    (void)XEEnableCtrlKeys((void_function)SetGlobalDone);
             
    XETrapAppWhileLoop(app,tc,&GlobalDone);

    /* Make sure <CTRL> key is released */
    XESimulateXEventRequest(tc, KeyRelease, 
        XKeysymToKeycode(dpy, XK_Control_L), 0L, 0L, 0L);

    /* close down everything nicely */
    XEFreeTC(tc);
    (void)XCloseDisplay(dpy);
    (void)XEClearCtrlKeys();
    _ClearExceptionHandling();
    exit(0L);
}

