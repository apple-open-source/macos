/* $XFree86: xc/programs/xtrap/xtrapproto.c,v 1.3 2002/09/18 17:11:57 tsi Exp $ */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * Log: xtrapproto.c,v $
 * Revision 1.1.2.2  1993/12/14  12:37:32  Kenneth_Miller
 * 	ANSI-standardize code and turn client build on
 * 	[1993/12/09  20:16:08  Kenneth_Miller]
 *
 * EndLog$
 */
#if !defined(lint) && 0
static char *rcsid = "@(#)RCSfile: xtrapproto.c,v $ Revision: 1.1.2.2 $ (DEC) Date: 1993/12/14 12:37:32 $";
#endif
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1993 by Digital Equipment Corp., 
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
#include <stdio.h>
#include <stdlib.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>


int
main(int argc, char *argv[])
{
    static Widget appW;
    XtAppContext app;
    char *tmp = NULL;
    XETrapGetAvailRep     ret_avail;
    XETrapGetCurRep       ret_cur;
    XETrapGetStatsRep     ret_stats;
    XETrapGetVersRep      ret_vers;
    XETrapGetLastInpTimeRep ret_time;
    XETC    *tc;
    Display *dpy;
    Boolean status = True;

    /* Connect to Server */
    appW = XtAppInitialize(&app,"XTrap",NULL,(Cardinal)0L,
        (int *)&argc, (String *)argv, (String *)NULL,(ArgList)&tmp,
        (Cardinal)NULL);
    dpy = XtDisplay(appW);
    printf("Display:  %s \n", DisplayString(dpy));
    if ((tc = XECreateTC(dpy,0L, NULL)) == False)
    {
        fprintf(stderr,"%s: could not initialize extension\n",argv[0]);
        exit(1L);
    }
    XSynchronize(dpy, True);
    status = XEResetRequest(tc);
    status = XEGetAvailableRequest(tc,&ret_avail);
    XEPrintAvail(stdout,&ret_avail);
    status = XEGetCurrentRequest(tc,&ret_cur);
    XEPrintCurrent(stderr,&ret_cur);
    XETrapSetStatistics(tc, True);  /* trigger config and def stats */
    status = XEFlushConfig(tc);
    status = XEGetStatisticsRequest(tc, &ret_stats);
    XEPrintStatistics(stdout, &ret_stats,tc);
    status = XEStartTrapRequest(tc);
    status = XESimulateXEventRequest(tc, MotionNotify, 0, 10L, 20L, 0L);
    status = XEStopTrapRequest(tc);
    if (tc->protocol == 31)
    {   /* didn't work in V3.1 */
        printf("XEGetVersionRequest() & XEGetLastInpTimeRequest() are\n");
        printf("broken using the V3.1 protocol!\n");
    }
    else
    {
        status = XEGetVersionRequest(tc,&ret_vers);
        status = XEGetLastInpTimeRequest(tc, &ret_time);
    }
    XEFreeTC(tc);
    (void)XCloseDisplay(dpy);
    exit(0L);
}
