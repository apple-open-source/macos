/* $XFree86$ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1994 by Digital Equipment Corp., 
Maynard, MA
X11R6 Changes Copyright (c) 1994 by Robert Chesler of Absol-Puter, Hudson, NH.

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL AND ABSOL-PUTER DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL DIGITAL OR ABSOL-PUTER BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*****************************************************************************/
#include <stdio.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>
#ifdef vms
#define IS_AT_OR_AFTER(t1, t2) (((t2).high > (t1).high) \
        || (((t2).high == (t1).high)&& ((t2).low >= (t1).low)))
typedef struct _vms_time {
     unsigned long low;
     unsigned long high;
}vms_time;                                      /* from IntrinsicP.h */
#ifdef VMSDW_V3
typedef struct _ModToKeysymTable {
    Modifiers mask;
    int count;
    int index;
} ModToKeysymTable;                             /* from TranslateI.h */
typedef struct _ConverterRec **ConverterTable;  /* from ConvertI.h */
#include "libdef.h"
typedef struct _CallbackRec *CallbackList;      /* from CallbackI.h */
typedef struct _XtGrabRec  *XtGrabList;         /* from EventI.h */
#include <X11/PassivGraI.h>
#include <X11/InitialI.h>
#else  /* VMSDW_V3 */
typedef struct _ModToKeysymTable {
    Modifiers mask;
    int count;
    int index;
} ModToKeysymTable;                             /* from TranslateI.h */
typedef struct _ConverterRec **ConverterTable;  /* from ConvertI.h */
#include "libdef.h"
#define NFDBITS	(sizeof(fd_mask) * 8)
typedef long  fd_mask;
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif /* howmany */
typedef	struct Fd_set {
	fd_mask	fds_bits[howmany(256, NFDBITS)];
} Fd_set;                                       /* from fd.h */
#include <X11/InitializeI.h>
#endif  /* VMSDW_V3 */
#else  /* !vms */
#include <X11/IntrinsicI.h>
#define IS_AT_OR_AFTER(t1, t2) (((t2).tv_sec > (t1).tv_sec) \
        || (((t2).tv_sec == (t1).tv_sec)&& ((t2).tv_usec >= (t1).tv_usec)))
#endif /* vms */

/* The following has been lifted from NextEvent.c  in X11R4 */

#ifndef NEEDS_NTPD_FIXUP
# ifdef sun
#  define NEEDS_NTPD_FIXUP 1
# else
#  define NEEDS_NTPD_FIXUP 0
# endif
#endif

#if NEEDS_NTPD_FIXUP
#define FIXUP_TIMEVAL(t) { \
        while ((t).tv_usec >= 1000000) { \
            (t).tv_usec -= 1000000; \
            (t).tv_sec++; \
        } \
        while ((t).tv_usec < 0) { \
            if ((t).tv_sec > 0) { \
                (t).tv_usec += 1000000; \
                (t).tv_sec--; \
            } else { \
                (t).tv_usec = 0; \
                break; \
            } \
        }}
#else
#define FIXUP_TIMEVAL(t)
#endif /*NEEDS_NTPD_FIXUP*/


/* The following code is required for the use of the XLIB transport of XTrap
 * events. This is in line with what MIT wants to see proper extension
 * implementations do, as compared to using one of the core input event masks.
 */

Boolean (*XETrapGetEventHandler(XETC *tc, CARD32 id))(XETrapDataEvent *event, XETC *tc)
{
    return((id < XETrapNumberEvents) ? tc->eventFunc[id] : NULL);
}

Boolean (*XETrapSetEventHandler(XETC *tc, CARD32 id,
			Boolean (*pfunc)(XETrapDataEvent *event, XETC *tc)))(XETrapDataEvent *event, XETC *tc)
{
    register Boolean (*rfunc)(XETrapDataEvent *event, XETC *tc) = NULL;

    if (id < XETrapNumberEvents)
    {
        rfunc = XETrapGetEventHandler(tc,id);
        tc->eventFunc[id] = pfunc;
    }
    return(rfunc);
}

Boolean XETrapDispatchEvent(XEvent *pevent, XETC *tc)
{
    Boolean status = False;
    register CARD32 id = pevent->type;
    register CARD32 firstEvent = tc->eventBase;
    register CARD32 lastEvent  = tc->eventBase + XETrapNumberEvents - 1L;

    /* If it is our extension event, handle it specially, otherwise, pass
     * it off to Xt.
     */
    if (firstEvent != 0 && id >= firstEvent && id <= lastEvent)
    {
        /* We may be ignoring the event */
        if (tc->eventFunc[id - firstEvent] != NULL)
        {
            status = (*tc->eventFunc[id - firstEvent])((XETrapDataEvent*)pevent,tc);
        }
    }
    else
    {
        status = XtDispatchEvent(pevent);
    }
    return(status);
}

XtInputMask XETrapAppPending(XtAppContext app)
{
    TimerEventRec *te_ptr;
#ifndef VMS
    struct timeval cur_time;
#else  /* vms */
    vms_time cur_time;
    long efnMask = 0L;
    int status;
#endif /* vms */
    XtInputMask retmask = XtAppPending(app);        /* Prime XtIMEvent */

    retmask &= ~(XtIMTimer | XtIMAlternateInput);   /* clear timer & input */
    /* Now test for timer */
    te_ptr = app->timerQueue;
    while (te_ptr != NULL)
    {
#ifndef vms
        (void)gettimeofday(&cur_time, NULL);
        FIXUP_TIMEVAL(cur_time);
#else
        sys$gettim(&cur_time);
#endif /* vms */
        if (IS_AT_OR_AFTER(te_ptr->te_timer_value, cur_time))
        {   /* this timer is due to fire */
            retmask |= XtIMTimer;
            break;
        }
        te_ptr = te_ptr->te_next;
    }

    /* Now test for alternate input */
#ifndef vms
    if (app->outstandingQueue != NULL)
    {
        retmask |= XtIMAlternateInput;
    }
#else /* vms */
    if ((app->Input_EF_Mask != 0L) && ((status=SYS$READEF(1,&efnMask)) == 1))
    {   /* we have input configured & retrieved the efn cluster 0 */
        efnMask &= app->Input_EF_Mask;  /* mask out non-input */
        if (efnMask)                    /* any left? */
        {                               /* yes, an alt-input efn is set */
            retmask |= XtIMAlternateInput;
        }
    }
#endif  /* vms */
    return(retmask);
}

void XETrapAppMainLoop(XtAppContext app, XETC *tc)
{
    XEvent event;
    XtInputMask imask;

    while (1)
    {
        imask = XETrapAppPending(app);
        /* Check to see what's going on so that we don't block
         * in either NextEvent or ProcessEvent since neither
         * of these routines can correctly deal with XTrap Events
         */
        if (imask & XtIMXEvent)
        {
            (void)XtAppNextEvent(app,&event);
            (void)XETrapDispatchEvent(&event,tc);
        }
        else if (imask & (XtIMTimer | XtIMAlternateInput))
        {
            XtAppProcessEvent(app, (XtIMTimer | XtIMAlternateInput));
        }
        else
        {   /* Nothing going on, so we need to block */
            (void)XETrapWaitForSomething(app);
        }
    }
}

int XETrapAppWhileLoop(XtAppContext app, XETC *tc, Bool *done)
{
    XEvent event;
    XtInputMask imask;
    int status = True;

    if(done)
    {
        while (!(*done))
        {
            imask = XETrapAppPending(app);
            /* Check to see what's going on so that we don't block
             * in either NextEvent or ProcessEvent since neither
             * of these routines can correctly deal with XTrap Events
             */
            if (imask & XtIMXEvent)
            {
                (void)XtAppNextEvent(app, &event);
                (void)XETrapDispatchEvent(&event,tc);
            }
            else if (imask & (XtIMTimer | XtIMAlternateInput))
            {
                XtAppProcessEvent(app, (XtIMTimer | XtIMAlternateInput));
            }
            else
            {   /* Nothing going on, so we need to block */
                (void)XETrapWaitForSomething(app);
            }
        }
    }
    else
    {
        status = False;
    }
    return(status);
}

/* Wait for either Timer, Alternate Input, or an X Event to arrive */
int XETrapWaitForSomething(XtAppContext app)
{
#ifndef vms
    return(_XtWaitForSomething(app, FALSE, FALSE, FALSE, FALSE, TRUE
#ifdef XTHREADS
    , FALSE
#endif /* XTHREADS */
    , 0L));
#else   /* vms */
#define IS_AFTER(t1,t2) (((t2).high > (t1).high) \
       ||(((t2).high == (t1).high)&& ((t2).low > (t1).low)))
    long retval = 0L;
    TimerEventRec *te_ptr;
    vms_time cur_time,result_time;
    int status = 0;
    long quotient, remainder = 0;
    int d;

    if (app->timerQueue!= NULL) 
    {   /* check timeout queue */
        cur_time.low = cur_time.high = result_time.low = result_time.high = 0;
        te_ptr = app->timerQueue;
        sys$gettim(&cur_time);
        if ((IS_AFTER(app->timerQueue->te_timer_value, cur_time))  &&
            (app->timerQueue->te_proc != 0)) 
        {   /* it's fired! return! */
            return(0);
        }
        /* Jump through hoops to get the time specified in the queue into
         * milliseconds 
         */
        status = lib$sub_times (&(te_ptr->te_timer_value.low), &cur_time,
                                &result_time);
        /*
         * See if this timer has expired.  A timer is considered expired
         * if it's value in the past (the NEGTIM case) or if there is
         * less than one integral milli second before it would go off.
         */

        if (status == LIB$_NEGTIM ||
            (result_time.high == -1 && result_time.low > -10000)) 
        {   /* We've got a timer and it's ready to fire! */
            return(0);
        }
        else if ((status & 1) == 1) 
        {
            lib$ediv (&(10000), &result_time, &quotient, &remainder);
            quotient *= -1;         /* flip the sign bit */

            return(XMultiplexInput(app->count, &(app->list[0L]),
                app->Input_EF_Mask, quotient, 0L, &retval));
        }
        else
        {
            status = -1;
        }
    }
     
    return((status == -1 ? -1 : XMultiplexInput(app->count, &(app->list[0L]),
           app->Input_EF_Mask, 0L, 0L, &retval)));
#endif  /* vms */
}
