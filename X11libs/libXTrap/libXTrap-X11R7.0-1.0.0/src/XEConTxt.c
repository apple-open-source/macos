/* $XFree86: xc/lib/XTrap/XEConTxt.c,v 1.1 2001/11/02 23:29:27 dawes Exp $ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1994 by Digital Equipment Corp., 
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

#define NEED_REPLIES
#define NEED_EVENTS


#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>

#ifndef TRUE
# define TRUE 1L
#endif
#ifndef FALSE
# define FALSE 0L
#endif

extern char *extensionData;

static XETC     TC;

/*
 * This function is used to create a new XTrap context structure. The new
 * context is initialized to a hard coded default, then modified by the
 * valuemask and values passed in by the caller.
 */

XETC *XECreateTC(Display *dpy, CARD32 valuemask, XETCValues *value)
{
    static Bool firsttime = True;
    register XETC *tc     = &TC;
    register XETC *last_tc;
    XETrapGetAvailRep rep;

    /* If this is the first time here, then initialize the default TC */
    if (firsttime == True)
    {   
        firsttime = False;
        /* The first Trap Context is the Template (default) TC */
        (void)memset(tc,0L,sizeof(tc));
        tc->eventBase             = 0x7FFFFFFFL;
        tc->errorBase             = 0x7FFFFFFFL;
        tc->values.v.max_pkt_size = 0x7FFFL;
    }

    /* Position to the end of the list */
    for (;tc->next != NULL; tc = tc->next);

    /* Allocate memory for the new context */
    last_tc = tc;   /* save the address of the last on the list */
    if ((tc = (tc->next = (XETC *)XtMalloc(sizeof(*tc)))) == NULL)
    {   /* No memory to build TC, XtMalloc has already reported the error */
        return(NULL);
    }

    /* Use the original TC as the template to start from */
    (void)memcpy(tc,&TC,sizeof(TC));
    tc->next      = NULL;
    tc->dpy       = dpy;
    tc->xmax_size = XMaxRequestSize(tc->dpy);

    /* Initialize Extension */
    if (!XETrapQueryExtension(dpy,&(tc->eventBase),&(tc->errorBase),
        &(tc->extOpcode)))
    {
        char *params = XTrapExtName;
        unsigned int num_params = 1L;
        XtWarningMsg("CantLoadExt", "XECreateTC", "XTrapToolkitError",
            "Can't load %s extension", &params, &num_params);
        (void)XtFree((XtPointer)tc);
        last_tc->next = NULL;    /* Clear now nonexistant forward pointer */
        return(NULL);
    }

    /* Allocate memory for the XLIB transport */
    if ((tc->xbuff = (BYTE *)XtMalloc(tc->xmax_size * sizeof(CARD32) +
        SIZEOF(XETrapHeader))) == NULL)
    {   /* No memory to build TC, XtMalloc has already reported the error */
        (void)XtFree((XtPointer)tc);        /* free the allocated TC */
        last_tc->next = NULL;    /* Clear now nonexistant forward pointer */
        return(NULL);
    }

    /* Decide on a protocol version to communicate with */
    /* would *like* to use XEGetVersionRequest() but it's broken in V3.1 */
    if (XEGetAvailableRequest(tc,&rep) == True)
    {
        /* stow the protocol number */
        switch (rep.xtrap_protocol)
        {
            /* known acceptable protocols */
            case 31:
            case XETrapProtocol:
                tc->protocol = rep.xtrap_protocol;
                break;
            /* all else */
            default:    /* stay backwards compatible */
                tc->protocol = 31;
                break;
        }
        /* TC to contain *oldest* release/version/revision */
        if (XETrapGetAvailRelease(&rep) <= XETrapRelease)
        {
            tc->release = XETrapGetAvailRelease(&rep);
            if (XETrapGetAvailVersion(&rep) <= XETrapVersion)
            {
                tc->version = XETrapGetAvailVersion(&rep);
                tc->revision = (XETrapGetAvailRevision(&rep) <= XETrapRevision ?
                    XETrapGetAvailRevision(&rep) : XETrapRevision);
            }
            else
            {
                tc->version  = XETrapVersion;
                tc->revision = XETrapRevision;
            }
        }
        else
        {
                tc->release = XETrapRelease;
                tc->version  = XETrapVersion;
                tc->revision = XETrapRevision;
        }
    }
    else
    {   /* We can't seem to communicate with the extension! */
        char *params = XTrapExtName;
        unsigned int num_params = 1L;
        XtWarningMsg("CantComm", "XECreateTC", "XTrapToolkitError",
            "Can't communicate with extension %s", &params, &num_params);
        (void)XtFree((XtPointer)tc->xbuff);/* de-allocate memory just alloc'd */
        (void)XtFree((XtPointer)tc);           /* free the allocated TC */
        last_tc->next = NULL;       /* Clear now nonexistant forward pointer */
        return(NULL);
    }

    /* Assign the context values the caller provided */
    (void)XEChangeTC(tc, valuemask, value);

    return (tc);
}


static int CheckChangeBits(XETrapFlags *dest, XETrapFlags *src, INT32 bit)
{
    int chg_flag = False;

    if (!(BitIsFalse(dest->valid,bit) && BitIsFalse(src->valid,bit)) ||
        !(BitIsTrue(dest->valid,bit) && BitIsTrue(src->valid,bit)))
    {
        BitCopy(dest->valid, src->valid, bit);
        chg_flag = True;
    }
    if (!(BitIsFalse(dest->data,bit) && BitIsFalse(src->data,bit)) ||
        !(BitIsTrue(dest->data,bit) && BitIsTrue(src->data,bit)))
    {
        BitCopy(dest->data, src->data, bit);
        chg_flag = True;
    }
    return(chg_flag);
}

/*
 * This function is called to change one or more parameters used to define
 * a context in which XTrap is or will be running.
 */
int XEChangeTC(XETC *tc, CARD32 mask, XETCValues *values)
{
    int status = True;
    register XETCValues *tval = &(tc->values);
    register int i;

    if (mask & TCStatistics)
    {   /* Statistics need changing */
        if(CheckChangeBits(&(tval->v.flags), &(values->v.flags), 
            XETrapStatistics))
        {
            tc->dirty |= TCStatistics;
        }
    }
    if (mask & TCRequests)
    {   /* Requests need changing */
        CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapRequest);
        for (i=0; i<256L; i++)
        {
            XETrapSetCfgFlagReq(tval, i, BitValue(values->v.flags.req,i));
        }
        tc->dirty |= TCRequests;
    }
    if (mask & TCEvents)
    {   /* Events need changing */
        CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapEvent);
        for (i=KeyPress; i<=MotionNotify; i++)
        {
            XETrapSetCfgFlagEvt(tval, i, BitValue(values->v.flags.event,i));
        }
        tc->dirty |= TCEvents;
    }
    if (mask & TCMaxPacket)
    {   /* MaxPacket needs changing */
        CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapMaxPacket);
        XETrapSetCfgMaxPktSize(tval, values->v.max_pkt_size);
        tc->dirty |= TCMaxPacket;
    }
    if (mask & TCCmdKey)
    {   /* CmdKey needs changing */
        CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapCmd);
        tval->v.cmd_key = values->v.cmd_key;
        CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapCmdKeyMod);
        tc->dirty |= TCCmdKey;
    }
    if (mask & TCTimeStamps)
    {   /* TimeStamps needs changing */
        if (CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapTimestamp))
        {
            tc->dirty |= TCTimeStamps;
        }
        BitCopy(tval->tc_flags, values->tc_flags, XETCDeltaTimes);
    }
    if (mask & TCWinXY)
    {   /* Window XY's need changing */
        if (CheckChangeBits(&(tval->v.flags), &(values->v.flags), XETrapWinXY))
        {
            tc->dirty |= TCWinXY;
        }
    }
    if (mask & TCCursor)
    {   /* Window XY's need changing */
        if (CheckChangeBits(&(tval->v.flags), &(values->v.flags),XETrapCursor))
        {
            tc->dirty |= TCCursor;
        }
    }
    if (mask & TCXInput)
    {   /*  XInput flag needs changing */
        if (CheckChangeBits(&(tval->v.flags), &(values->v.flags),XETrapXInput))
        {
            tc->dirty |= TCXInput;
        }
    }
    if (mask & TCColorReplies)
    {   /*  ColorReplies flag needs changing */
        if (CheckChangeBits(&(tval->v.flags), &(values->v.flags),
            XETrapColorReplies))
        {
            tc->dirty |= TCColorReplies;
        }
    }
    if (mask & TCGrabServer )
    {   /* GrabServer flag needs changing */
        if (CheckChangeBits(&(tval->v.flags), &(values->v.flags),
            XETrapGrabServer ))
        {
            tc->dirty |= TCGrabServer;
        }
    }
    if (XETrapGetTCFlagTrapActive(tc))
    {
        status = XEFlushConfig(tc);
    }
#ifdef VMS
    sys$setast(True);   /* Make sure AST's are enabled */
#endif /* VMS */
    return(status);
}


void XEFreeTC(XETC *tc)
{
    register XETC *list = &TC;

    if (tc)
    {
        while(list->next != NULL)
        {
            if (list->next == tc)
                list->next = list->next->next;  /* Got it, remove from list */
            else
                list = list->next;              /* Update the list pointer */
        }    
        if (tc->values.req_cb)
        {
            XtFree((XtPointer)tc->values.req_cb);
        }
        if (tc->values.evt_cb)
        {
            XtFree((XtPointer)tc->values.evt_cb);
        }
        if (tc->xbuff != NULL)
        {
            XtFree((XtPointer)tc->xbuff);
        }

        XtFree((XtPointer)tc);
        if (extensionData)
        {
            XtFree(extensionData);
        }
    }
    return;
}

/* The following are Convenience routines for setting values within
 * the Trap Context.  These are analogous to the GC's Convenience
 * Functions such as XSetState & XSetForeground
 */
int XETrapSetMaxPacket(XETC *tc, Bool set_flag, CARD16 size)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagMaxPacket(&tcv, valid, True);
    XETrapSetCfgFlagMaxPacket(&tcv, data, set_flag);
    XETrapSetCfgMaxPktSize(&tcv, size);
    status = XEChangeTC(tc, TCMaxPacket, &tcv);
    return(status);
}
int XETrapSetCommandKey(XETC *tc, Bool set_flag, KeySym cmd_key, Bool mod_flag)
{
    XETCValues tcv;
    int status = True;
    KeyCode cmd_keycode;

    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagCmd(&tcv, valid, True);
    XETrapSetCfgFlagCmd(&tcv, data, set_flag);
    if (set_flag == True)
    {
        XETrapSetCfgFlagCmdKeyMod(&tcv, valid, True);
        XETrapSetCfgFlagCmdKeyMod(&tcv, data, mod_flag);
        if (!(cmd_keycode = XKeysymToKeycode(XETrapGetDpy(tc), cmd_key)))
        {
            status = False;
        }
        else
        {
            XETrapSetCfgCmdKey(&tcv, cmd_keycode);
        }
    }
    else
    {   /* Clear command key */
        XETrapSetCfgFlagCmdKeyMod(&tcv, valid, True);
        XETrapSetCfgFlagCmdKeyMod(&tcv, data, False);
        XETrapSetCfgCmdKey(&tcv, 0);
    }
    if (status == True)
    {
        status = XEChangeTC(tc, TCCmdKey, &tcv);
    }
    return(status);
}

int XETrapSetTimestamps(XETC *tc, Bool set_flag, Bool delta_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagTimestamp(&tcv, valid, True);
    XETrapSetCfgFlagTimestamp(&tcv, data, set_flag);
    XETrapSetValFlagDeltaTimes(&tcv, delta_flag);
    status = XEChangeTC(tc, TCTimeStamps, &tcv);
    return(status);
}

int XETrapSetWinXY(XETC *tc, Bool set_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagWinXY(&tcv, valid, True);
    XETrapSetCfgFlagWinXY(&tcv, data, set_flag);
    status = XEChangeTC(tc, TCWinXY, &tcv);
    return(status);
}

int XETrapSetCursor(XETC *tc, Bool set_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagCursor(&tcv, valid, True);
    XETrapSetCfgFlagCursor(&tcv, data, set_flag);
    status = XEChangeTC(tc, TCCursor, &tcv);
    return(status);
}

int XETrapSetXInput(XETC *tc, Bool set_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagXInput(&tcv, valid, True);
    XETrapSetCfgFlagXInput(&tcv, data, set_flag);
    status = XEChangeTC(tc, TCXInput, &tcv);
    return(status);
}

int XETrapSetColorReplies(XETC *tc, Bool set_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagColorReplies(&tcv, valid, True);
    XETrapSetCfgFlagColorReplies(&tcv, data, set_flag);
    status = XEChangeTC(tc, TCColorReplies, &tcv);
    return(status);
}

int XETrapSetGrabServer(XETC *tc, Bool set_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagGrabServer(&tcv, valid, True);
    XETrapSetCfgFlagGrabServer(&tcv, data, set_flag);
    status = XEChangeTC(tc, TCGrabServer, &tcv);
    return(status);
}

int XETrapSetStatistics(XETC *tc, Bool set_flag)
{
    XETCValues tcv;
    int status = True;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagStatistics(&tcv, valid, True);
    XETrapSetCfgFlagStatistics(&tcv, data, set_flag);
    status = XEChangeTC(tc, TCStatistics, &tcv);
    return(status);
}

int XETrapSetRequests(XETC *tc, Bool set_flag, ReqFlags requests)
{
    XETCValues tcv;
    int status = True;
    int i;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagRequest(&tcv, valid, True);
    XETrapSetCfgFlagRequest(&tcv, data, set_flag);
    for (i=0; i<256L; i++)
    {
        XETrapSetCfgFlagReq(&tcv, i, BitValue(requests, i));
    }
    status = XEChangeTC(tc, TCRequests, &tcv);
    return(status);
}

int XETrapSetEvents(XETC *tc, Bool set_flag, EventFlags events)
{
    XETCValues tcv;
    int status = True;
    int i;
    
    (void)memset((char *)&tcv,0L,sizeof(tcv));
    XETrapSetCfgFlagEvent(&tcv, valid, True);
    XETrapSetCfgFlagEvent(&tcv, data, set_flag);
    for (i=KeyPress; i<=MotionNotify; i++)
    {
        XETrapSetCfgFlagEvt(&tcv, i, BitValue(events, i));
    }
    status = XEChangeTC(tc, (CARD32)TCEvents, &tcv);
    return(status);
}

Bool XESetCmdGateState(XETC *tc, CARD8 type, Bool *gate_closed,
    CARD8 *next_key, Bool *key_ignore)
{
    
    *key_ignore = False;
    if (XETrapGetTCFlagCmdKeyMod(tc,data) == True)
    {
        switch (type) 
        {
            case KeyPress:
                if (*next_key == XEKeyIsEcho)
                {
                    break;
                }
                *gate_closed = True;
                *next_key = XEKeyIsClear;
                break;
    
            case KeyRelease:
                if (*next_key == XEKeyIsEcho)
                {
                    *next_key = XEKeyIsClear;
                    break;
                }
                if (*next_key == XEKeyIsClear)
                {
                    *next_key = XEKeyIsEcho;
                }
                else
                {   /* it's XEKeyIsOther, so Clear it */
                    *next_key = XEKeyIsClear;
                }
                *gate_closed = False;
                *key_ignore = True;
                break;
    
            default: break;
        }
    }
    else
    {
        switch (type)
        {
            case KeyPress:
                if (*next_key == XEKeyIsEcho)
                {
                    *gate_closed = False;
                    break;
                }
                /* Open gate on cmd key release */
                if ((*next_key == XEKeyIsOther) && 
                    *gate_closed == True)
                {
                    break;
                }
                *gate_closed = True;
                *next_key = XEKeyIsClear;
                break;

            case KeyRelease:
                if (*next_key == XEKeyIsClear)
                {
                    *next_key = XEKeyIsEcho;
                    break;
                }
    
                if (*next_key == XEKeyIsEcho)
                {
                    *next_key = XEKeyIsClear;
                    break;
                }
    
                *gate_closed = False;
                *key_ignore = True;
                *next_key = XEKeyIsClear;
                break;
    
            default: 
                break;
        }
    }

    return(*gate_closed);
}
