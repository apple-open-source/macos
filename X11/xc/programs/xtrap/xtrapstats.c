/* $XFree86: xc/programs/xtrap/xtrapstats.c,v 1.2 2002/09/18 17:11:57 tsi Exp $ */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * Log: xtrapstats.c,v $
 * Revision 1.1.4.2  1993/12/14  12:37:41  Kenneth_Miller
 * 	ANSI-standardize code and turn client build on
 * 	[1993/12/09  20:16:19  Kenneth_Miller]
 *
 * Revision 1.1.2.2  1992/04/27  13:52:12  Leela_Obilichetti
 * 	initial load of xtrap clients - from silver BL6 pool
 * 	[92/04/27  13:49:48  Leela_Obilichetti]
 * 
 * EndLog$
 */
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
#include <ctype.h>

int
main(int argc, char *argv[])
{
    XETrapGetAvailRep     ret_avail;
    XETrapGetStatsRep     ret_stats;
    Widget  appW;
    XtAppContext app;
    char *tmp = NULL;
    XETC    *tc;
    Display *dpy;
    Bool done;
    char    buffer[10];
    ReqFlags   requests;
    EventFlags events;
    int i;

    /* Connect to Server */
    appW = XtAppInitialize(&app,"XTrap",NULL,(Cardinal)0L,
        (int *)&argc, (String *)argv, (String *)NULL,(ArgList)&tmp,
        (Cardinal)NULL);
    dpy = XtDisplay(appW);
#ifdef DEBUG
    XSynchronize(dpy, True);
#endif
    printf("Display:  %s \n", DisplayString(dpy));
    if ((tc = XECreateTC(dpy,0L, NULL)) == False)
    {
        fprintf(stderr,"%s: could not initialize extension\n",argv[0]);
        exit(1L);
    }

    (void)XEGetAvailableRequest(tc,&ret_avail);
    if (BitIsFalse(ret_avail.valid, XETrapStatistics))
    {
        printf("\nStatistics not available from '%s'.\n",
            DisplayString(dpy));
        exit(1L);
    }
    XETrapSetStatistics(tc, True);
    for (i=0; i<256L; i++)
    {
        BitTrue(requests, i);
    }
    XETrapSetRequests(tc, True, requests);
    for (i=KeyPress; i<=MotionNotify; i++)
    {
        BitTrue(events, i);
    }
    XETrapSetEvents(tc, True, events);
    done = False;
    while(done == False)
    {
        fprintf(stderr,"Stats Command (Zero, Quit, [Show])? ");
        fgets(buffer, sizeof(buffer), stdin);
        switch(toupper(buffer[0]))
        {
            case '\n':  /* Default command */
            case 'S':   /* Request fresh counters & display */
                (void)XEGetStatisticsRequest(tc,&ret_stats);
                (void)XEPrintStatistics(stdout,&ret_stats,tc);
                break;
            case 'Z':  /* Zero out counters */
                XETrapSetStatistics(tc, False);
                break;
            case 'Q':
                done = True;
                break;
            default:
                printf("Invalid command, reenter!\n");
                break;
        }
    }
    (void)XEFreeTC(tc);
    (void)XCloseDisplay(dpy);
    exit(0L);
}
