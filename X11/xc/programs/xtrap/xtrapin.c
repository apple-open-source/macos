/* $XFree86: xc/programs/xtrap/xtrapin.c,v 1.2 2001/12/12 00:43:50 dawes Exp $ */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * Log: xtrapin.c,v $
 * Revision 1.1.4.2  1993/12/14  12:37:20  Kenneth_Miller
 * 	ANSI-standardize code and turn client build on
 * 	[1993/12/09  20:15:45  Kenneth_Miller]
 *
 * Revision 1.1.2.2  1992/04/27  13:51:39  Leela_Obilichetti
 * 	Initial load of xtrap clients - from silver BL6
 * 	[92/04/27  13:49:16  Leela_Obilichetti]
 * 
 * EndLog$
 */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991 by Digital Equipment Corp., Maynard, MA

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
#define ProgName "xtrapin"
/*
**++
**  FACILITY:  xtrapin - Sample client to test input to XTrap extension
**
**  MODULE DESCRIPTION:
**
**      This is the main module for a sample/test client
**      for the XTrap X11 Server Extension.  It accepts  
**      a script file and a transport method as input  
**      in addition to the standard X arguments (-d, etc.).
**      If no script file is provided, stdin is the default
**      and can be piped from the companion "xtrapout"
**      client (normally used with the -e argument which  
**      sends all core input events to stdout).
**
**
**  AUTHORS:
**
**      Kenneth B. Miller
**
**  CREATION DATE:  December 15, 1990
**
**  DESIGN ISSUES:
**
**      See the companion "xtrapout" client.
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
#include <unistd.h>
#include <stdlib.h>

#ifndef vaxc
#define globalref extern
#endif

static Boolean grabFlag = False;

FILE *ifp;
XrmOptionDescRec optionTable [] = 
{
    {"-f",     "*script",    XrmoptionSepArg,  (caddr_t) NULL},
    {"-g",     "*grabServer",XrmoptionSkipArg, (caddr_t) NULL},
};

typedef struct
{   /* longword-align fields for arg passing */
    Time  ts;
    int   type;
    int   detail;
    int   x;
    int   y;
    int  screen; /* this will always be 0 till vectored events! */
} file_rec;

/* Forward declarations */
static Bool found_input_rec (FILE *ifp , file_rec *rec );

static Widget appW;
static Display *dpy;


int
main(int argc, char *argv[])
{
    XETrapGetAvailRep ret_avail;
    XETrapGetCurRep   ret_cur;
    XETC    *tc;
    XtAppContext app;
    char *tmp = NULL;
    INT16 ch;
    file_rec rec;
    Time last_time = 0L;
    int *popterr;
    char **poptarg;
#ifndef vms
    popterr = &opterr;
    poptarg = &optarg;
#else
    popterr = XEgetopterr();
    poptarg = XEgetoptarg();
#endif

    ifp = NULL;
    *popterr = 0; /* don't complain about -d for display */
    grabFlag = False;
    while ((ch = getopt(argc, argv, "d:f:g")) != EOF)
    {
        switch(ch)
        {
            case 'f':
                if ((ifp = fopen(*poptarg,"rb")) == NULL)
                {   /* can't open it */
                    fprintf(stderr,"%s: could not open output file '%s'!\n",
                        ProgName, *poptarg);
                }
                break;
            case 'd':   /* -display, let's let the toolkit parse it */
                break;
            case 'g':
                grabFlag = True;
            default:
                break;
        }
    }
    ifp = (ifp ? ifp : stdin);

    appW = XtAppInitialize(&app,"XTrap",optionTable,(Cardinal)1L,
        (int *)&argc, (String *)argv, (String *)NULL,(ArgList)&tmp,
        (Cardinal)NULL);

    dpy = XtDisplay(appW);
#ifdef DEBUG
    XSynchronize(dpy, True);
#endif
    printf("Display:  %s \n", DisplayString(dpy));

    if ((tc = XECreateTC(dpy,0L, NULL)) == False)
    {
        fprintf(stderr,"%s: could not initialize XTrap extension\n", ProgName);
        exit (1L);
    }
    (void)XEGetAvailableRequest(tc,&ret_avail);
    XEPrintAvail(stderr,&ret_avail);
    XEPrintTkFlags(stderr,tc);

    if (grabFlag == True)
    {   /* 
         * In order to ignore GrabServer's we must configure at least one 
         * trap.  Let's make it X_GrabServer.  We don't have to receive
         * a callback, though.
         */
        ReqFlags requests;
        (void)memset(requests,0L,sizeof(requests));
        BitTrue(requests, X_GrabServer);
        XETrapSetRequests(tc, True, requests);
        (void)XETrapSetGrabServer(tc, True);
    }

    (void)XEStartTrapRequest(tc);
    (void)XEGetCurrentRequest(tc,&ret_cur);
    XEPrintCurrent(stderr,&ret_cur);

    /* Open up script file */
    while (found_input_rec(ifp,&rec) == True)
    {
        /* if not pipe'd, delay time delta time recorded */
        if (ifp != stdin)
        {
            register INT32 delta, t1, t2;
            last_time = (last_time ? last_time : rec.ts);      /* first rec */
            rec.ts = (rec.ts ? rec.ts : last_time);    /* dual monitor bug! */
            t1 = rec.ts; t2 = last_time;        /* move to signed variables */
            delta = abs(t1 - t2);           /* protect from clock roll-over */
            msleep(delta);
            last_time = rec.ts;
        }
        XESimulateXEventRequest(tc, rec.type, rec.detail, rec.x, rec.y,
            rec.screen);
    }
      
    (void)XCloseDisplay(dpy);
    exit(0L);
}

static Bool found_input_rec(FILE *ifp, file_rec *rec)
{
    int found = False;
    char buff[BUFSIZ];
    char junk[16L];
    int  match;
    int  tmp[8L];

    while ((found != True) && (fgets(buff,BUFSIZ,ifp) != NULL))
    {
        if (!strncmp(buff, "Event:", strlen("Event:")))
        {   /* we want this record */
            if ((match = sscanf(buff,
             "Event: %s (%d):det=%d scr=%d (%d,%d) root=%d Msk=%d TS=%d\n",
                junk, &(tmp[0L]), &(tmp[1L]), &(tmp[2L]), &(tmp[3L]), 
                &(tmp[4L]), &(tmp[5L]), &(tmp[6L]), &(tmp[7L]))) != 9L)
            {
                fprintf(stderr, "%s:  Error parsing script input!\n\t'%s'\n",
                    ProgName, buff);
            }
            else
            {
                found = True;
                /* Sun's have problems with "byte" fields passed to scanf */
                rec->type   = tmp[0L];
                rec->detail = tmp[1L];
                rec->screen = tmp[2L];
                rec->x      = tmp[3L];
                rec->y      = tmp[4L];
                rec->ts     = tmp[7L];
            }
        }
        else if (!strncmp(buff, "Request:", strlen("Request:")))
        {   /* a valid thing to see */
            continue;
        }
        else
        {   /* this stuff doesn't look like what we'd expect */
            fprintf(stderr, "%s:  Not a valid script record!\n\t'%s'\n",
                ProgName, buff);
        }
    }

    return(found);
}

