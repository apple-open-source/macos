/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86PM.c,v 3.10 2004/02/13 23:58:38 dawes Exp $ */
/*
 * Copyright (c) 2000-2002 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "X.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Xinput.h"

int (*xf86PMGetEventFromOs)(int fd,pmEvent *events,int num) = NULL;
pmWait (*xf86PMConfirmEventToOs)(int fd,pmEvent event) = NULL;

static Bool suspended = FALSE;

static char *
eventName(pmEvent event)
{
    switch(event) {
    case XF86_APM_SYS_STANDBY: return ("System Standby Request");
    case XF86_APM_SYS_SUSPEND: return ("System Suspend Request");
    case XF86_APM_CRITICAL_SUSPEND: return ("Critical Suspend");
    case XF86_APM_USER_STANDBY: return ("User System Standby Request");
    case XF86_APM_USER_SUSPEND: return ("User System Suspend Request");
    case XF86_APM_STANDBY_RESUME: return ("System Standby Resume");
    case XF86_APM_NORMAL_RESUME: return ("Normal Resume System");
    case XF86_APM_CRITICAL_RESUME: return ("Critical Resume System");
    case XF86_APM_LOW_BATTERY: return ("Battery Low");
    case XF86_APM_POWER_STATUS_CHANGE: return ("Power Status Change");
    case XF86_APM_UPDATE_TIME: return ("Update Time");
    case XF86_APM_CAPABILITY_CHANGED: return ("Capability Changed");
    case XF86_APM_STANDBY_FAILED: return ("Standby Request Failed");
    case XF86_APM_SUSPEND_FAILED: return ("Suspend Request Failed");
    default: return ("Unknown Event");
    }
}

static void
suspend (pmEvent event, Bool undo)
{
    int i;
    InputInfoPtr pInfo;

   xf86inSuspend = TRUE;
    
    for (i = 0; i < xf86NumScreens; i++) {
        xf86EnableAccess(xf86Screens[i]);
	if (xf86Screens[i]->EnableDisableFBAccess)
	    (*xf86Screens[i]->EnableDisableFBAccess) (i, FALSE);
    }
#if !defined(__EMX__)
    pInfo = xf86InputDevs;
    while (pInfo) {
	DisableDevice(pInfo->dev);
	pInfo = pInfo->next;
    }
#endif
    xf86EnterServerState(SETUP);
    for (i = 0; i < xf86NumScreens; i++) {
        xf86EnableAccess(xf86Screens[i]);
	if (xf86Screens[i]->PMEvent)
	    xf86Screens[i]->PMEvent(i,event,undo);
	else {
	    xf86Screens[i]->LeaveVT(i, 0);
	    xf86Screens[i]->vtSema = FALSE;
	}
    }
    xf86AccessLeave();      
    xf86AccessLeaveState(); 
}

static void
resume(pmEvent event, Bool undo)
{
    int i;
    InputInfoPtr pInfo;

    xf86AccessEnter();
    xf86EnterServerState(SETUP);
    for (i = 0; i < xf86NumScreens; i++) {
        xf86EnableAccess(xf86Screens[i]);
	if (xf86Screens[i]->PMEvent)
	    xf86Screens[i]->PMEvent(i,event,undo);
	else {
	    xf86Screens[i]->vtSema = TRUE;
	    xf86Screens[i]->EnterVT(i, 0);
	}
    }
    xf86EnterServerState(OPERATING);
    for (i = 0; i < xf86NumScreens; i++) {
        xf86EnableAccess(xf86Screens[i]);
	if (xf86Screens[i]->EnableDisableFBAccess)
	    (*xf86Screens[i]->EnableDisableFBAccess) (i, TRUE);
    }
    SaveScreens(SCREEN_SAVER_FORCER, ScreenSaverReset);
#if !defined(__EMX__)
    pInfo = xf86InputDevs;
    while (pInfo) {
	EnableDevice(pInfo->dev);
	pInfo = pInfo->next;
    }
#endif
    xf86inSuspend = FALSE;
}

static void
DoApmEvent(pmEvent event, Bool undo)
{
    /* 
     * we leave that as a global function for now. I don't know if 
     * this might cause problems in the future. It is a global server 
     * variable therefore it needs to be in a server info structure
     */
    int i;
    
    switch(event) {
    case XF86_APM_SYS_STANDBY:
    case XF86_APM_SYS_SUSPEND:
    case XF86_APM_CRITICAL_SUSPEND: /*do we want to delay a critical suspend?*/
    case XF86_APM_USER_STANDBY:
    case XF86_APM_USER_SUSPEND:
	/* should we do this ? */
	if (!undo && !suspended) {
	    suspend(event,undo);
	    suspended = TRUE;
	} else if (undo && suspended) {
	    resume(event,undo);
	    suspended = FALSE;
	}
	break;
    case XF86_APM_STANDBY_RESUME:
    case XF86_APM_NORMAL_RESUME:
    case XF86_APM_CRITICAL_RESUME:
	if (suspended) {
	    resume(event,undo);
	    suspended = FALSE;
	}
	break;
    default:
	xf86EnterServerState(SETUP);
	for (i = 0; i < xf86NumScreens; i++) {
	    xf86EnableAccess(xf86Screens[i]);
	    if (xf86Screens[i]->PMEvent)
		xf86Screens[i]->PMEvent(i,event,undo);
	}
	xf86EnterServerState(OPERATING);
	break;
    }
}

#define MAX_NO_EVENTS 8

void
xf86HandlePMEvents(int fd, pointer data)
{
    pmEvent events[MAX_NO_EVENTS];
    int i,n;
    Bool wait = FALSE;

    if (!xf86PMGetEventFromOs)
	return;

    if ((n = xf86PMGetEventFromOs(fd,events,MAX_NO_EVENTS))) {
	do {
	    for (i = 0; i < n; i++) {
		xf86MsgVerb(X_INFO,3,"PM Event received: %s\n",
			    eventName(events[i]));
		DoApmEvent(events[i],FALSE);
		switch (xf86PMConfirmEventToOs(fd,events[i])) {
		case PM_WAIT:
		    wait = TRUE;
		    break;
		case PM_CONTINUE:
		    wait = FALSE;
		    break;
		case PM_FAILED:
		    DoApmEvent(events[i],TRUE);
		    wait = FALSE;
		    break;
		default:
		    break;
		}
	    }
	    if (wait)
		n = xf86PMGetEventFromOs(fd,events,MAX_NO_EVENTS);
	    else
		break;
	} while (1);
    }
}
