/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_mouse.c,v 3.17 2002/05/31 18:46:02 dawes Exp $ */
/*
 * (c) Copyright 1994,1999,2000 by Holger Veit
 *			<Holger.Veit@gmd.de>
 * Modified (c) 1996 Sebastien Marineau <marineau@genie.uottawa.ca>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
 * HOLGER VEIT  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Holger Veit shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Holger Veit.
 *
 */
/* $XConsortium: os2_mouse.c /main/10 1996/10/27 11:48:51 kaleb $ */

#define I_NEED_OS2_H
#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "inputstr.h"
#include "scrnintstr.h"

#include "compiler.h"

#define INCL_DOSFILEMGR
#define INCL_DOSQUEUES
#define INCL_MOU
#undef RT_FONT
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "xf86Config.h"

#include "xf86Xinput.h"
#include "xf86OSmouse.h"
#include "mipointer.h"

static int
SupportedInterfaces(void)
{
	return MSE_MISC;
}

static const char* internalNames[] = {
	"OS2Mouse",
	NULL
};

static const char**
BuiltinNames(void)
{
	return internalNames;
}

static Bool
CheckProtocol(const char *protocol)
{
	int i;

	for (i = 0; internalNames[i]; i++)
	if (xf86NameCmp(protocol, internalNames[i]) == 0)
		return TRUE;
	return FALSE;
}

static const char *
DefaultProtocol(void)
{
	return "OS2Mouse";
}

static const char *
SetupAuto(InputInfoPtr pInfo, int *protoPara)
{
	return "OS2Mouse";
}

HMOU hMouse=65535;
HEV hMouseSem;
HQUEUE hMouseQueue;
InputInfoPtr iinfoPtr;
int MouseTid;
BOOL HandleValid=FALSE;
extern BOOL SwitchedToWPS;
extern CARD32 LastSwitchTime;
void os2MouseEventThread(void* arg);

static void
os2MouseReadInput(InputInfoPtr pInfo)
{
	APIRET rc;
	ULONG postCount,dataLength;
	PVOID dummy;
	int buttons;
	int state;
	int i, dx,dy;
	BYTE elemPriority;
	REQUESTDATA requestData;

	MouseDevPtr pMse = pInfo->private;

	if (!HandleValid) return;
	while((rc = DosReadQueue(hMouseQueue,
				 &requestData,&dataLength,&dummy, 
				 0L,1L,&elemPriority,hMouseSem)) == 0) {
		dx = requestData.ulData;
		(void)DosReadQueue(hMouseQueue,
				  &requestData,&dataLength,&dummy,
				  0L,1L,&elemPriority,hMouseSem);
		dy = requestData.ulData;
		(void)DosReadQueue(hMouseQueue,
				  &requestData,&dataLength,&dummy,
				  0L,1L,&elemPriority,hMouseSem);
		state = requestData.ulData;
		(void)DosReadQueue(hMouseQueue,
				  &requestData,&dataLength,&dummy,
				  0L,1L,&elemPriority,hMouseSem);
		if (requestData.ulData != 0xFFFFFFFF) 
			xf86Msg(X_ERROR,
				"Unexpected mouse event tag, %d\n",
				requestData.ulData);

		/* Contrary to other systems, OS/2 has mouse buttons *
		 * in the proper order, so we reverse them before    *
		 * sending the event.                                */
		 
		buttons = ((state & 0x06) ? 4 : 0) |
			  ((state & 0x18) ? 1 : 0) |
			  ((state & 0x60) ? 2 : 0);
		pMse->PostEvent(pInfo, buttons, dx, dy, 0, 0);
	}
        DosResetEventSem(hMouseSem,&postCount);
}

int os2MouseProc(DeviceIntPtr pPointer, int what)
{
    APIRET rc = 0;
    USHORT nbuttons, state;
    unsigned char map[MSE_MAXBUTTONS + 1];
    int i;

    InputInfoPtr pInfo = pPointer->public.devicePrivate;
    MouseDevPtr pMse = pInfo->private;
    pMse->device = pPointer;

    switch (what) {
    case DEVICE_INIT: 
	pPointer->public.on = FALSE;
	if (hMouse == 65535) 
	    rc = MouOpen((PSZ)0, &hMouse);
	if (rc != 0)
	    xf86Msg(X_WARNING,"%s: cannot open mouse, rc=%d\n", 
		    pInfo->name,rc);
	else {
	    pInfo->fd = hMouse;

	    /* flush mouse queue */
	    MouFlushQue(hMouse);

	    /* check buttons */
	    rc = MouGetNumButtons(&nbuttons, hMouse);
	    if (rc == 0)
		xf86Msg(X_INFO,"%s: Mouse has %d button(s).\n",
			pInfo->name,nbuttons);
	    if (nbuttons==2) nbuttons++;

	    for (i = 1; i<=nbuttons; i++)
		map[i] = i;

	    InitPointerDeviceStruct((DevicePtr)pPointer, map, nbuttons,
		miPointerGetMotionEvents, pMse->Ctrl,
		miPointerGetMotionBufferSize());

	    /* X valuator */
	    xf86InitValuatorAxisStruct(pPointer, 0, 0, -1, 1, 0, 1);
	    xf86InitValuatorDefaults(pPointer, 0);
	    /* y Valuator */
	    InitValuatorAxisStruct(pPointer, 1, 0, -1, 1, 0, 1);
	    xf86InitValuatorDefaults(pPointer, 1);
	    xf86MotionHistoryAllocate(pInfo);

	    /* OK, we are ready to start up the mouse thread ! */
	    if (!HandleValid) {
		rc = DosCreateEventSem(NULL,&hMouseSem,DC_SEM_SHARED,TRUE);
		if (rc != 0)
		    xf86Msg(X_ERROR,"%s: could not create mouse queue semaphore, rc=%d\n",
			    pInfo->name,rc);
		MouseTid = _beginthread(os2MouseEventThread,NULL,0x4000,(void *)pInfo);
		xf86Msg(X_INFO,
			"%s: Started Mouse event thread, Tid=%d\n",
			pInfo->name, MouseTid);
		DosSetPriority(2,3,0,MouseTid);
	    }
	    HandleValid=TRUE;
	}
	break;
      
    case DEVICE_ON:
	if (!HandleValid) return -1;
	pMse->lastButtons = 0;
	pMse->emulateState = 0;
	pPointer->public.on = TRUE;
	state = 0x300;
	rc = MouSetDevStatus(&state,hMouse);
	state = 0x7f;
	rc = MouSetEventMask(&state,hMouse);
	MouFlushQue(hMouse);
	break;

    case DEVICE_CLOSE:
    case DEVICE_OFF:
	if (!HandleValid) return -1;
	pPointer->public.on = FALSE;
	state = 0x300;
	MouSetDevStatus(&state,hMouse);
	state = 0;
	MouSetEventMask(&state,hMouse);
	if (what == DEVICE_CLOSE) {
/* Comment out for now as this seems to break server */
#if 0
	    MouClose(hMouse);
	    hMouse = 65535;
	    pInfo->fd = -1;
	    HandleValid = FALSE;
#endif
	}
	break;
    }
    return Success;
}

int os2MouseQueueQuery()
{
    /* Now we check for activity on mouse handles */
    ULONG numElements,postCount;

    if (!HandleValid) return(1);
    DosResetEventSem(hMouseSem,&postCount);
    (void)DosQueryQueue(hMouseQueue,&numElements);
    if (numElements>0) {     /* Something in mouse queue! */
	return 0;  /* Will this work? */
    }
    return 1;
}

void os2MouseEventThread(void *arg)
{
    APIRET rc;
    MOUEVENTINFO mev;
    ULONG queueParam;
    USHORT waitflg;
    char queueName[128];
    MouseDevPtr pMse;

    iinfoPtr = (InputInfoPtr)arg;
    pMse = iinfoPtr->private;

    sprintf(queueName,"\\QUEUES\\XF86MOU\\%d",getpid());
    rc = DosCreateQueue(&hMouseQueue,0L,queueName);
    xf86Msg(X_INFO,"Mouse Queue created, rc=%d\n",rc);
    (void)DosPurgeQueue(hMouseQueue);

    while(1) {
	waitflg = 1;
	rc = MouReadEventQue(&mev,&waitflg,hMouse);
	if (rc) {
	    xf86Msg(X_ERROR,
		    "Bad return code from mouse driver, rc=%d\n",
		    rc);
	    xf86Msg(X_ERROR,"Mouse aborting!\n");
	    break;
	}

	queueParam = mev.col;
	if ((rc = DosWriteQueue(hMouseQueue,queueParam,0L,NULL,0L)))
		break;
	queueParam = mev.row;
	if ((rc = DosWriteQueue(hMouseQueue,queueParam,0L,NULL,0L)))
 		break;
        queueParam = mev.fs;
	if ((rc = DosWriteQueue(hMouseQueue,queueParam,0L,NULL,0L)))
		break;
	queueParam = 0xFFFFFFFF;
	if ((rc = DosWriteQueue(hMouseQueue,queueParam,0L,NULL,0L)))
		break;
    }
    xf86Msg(X_ERROR,
        "An unrecoverable error in mouse queue has occured, rc=%d. Mouse is shutting down.\n",
        rc);
    DosCloseQueue(hMouseQueue);
}


static Bool
os2MousePreInit(InputInfoPtr pInfo, const char* protocol, int flags) 
{
    MouseDevPtr pMse = pInfo->private;

    pMse->protocol = protocol;
    xf86Msg(X_CONFIG, "%s: Protocol: %s\n", pInfo->name, protocol);

    /* Collect the options, and process the common options. */
    xf86CollectInputOptions(pInfo, NULL, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options);

    /* Process common mouse options (like Emulate3Buttons, etc). */
    pMse->CommonOptions(pInfo);

    /* Setup the local procs. */
    pInfo->device_control = os2MouseProc;
    pInfo->read_input = os2MouseReadInput;

    pInfo->flags |= XI86_CONFIGURED;
    return TRUE;
}

OSMouseInfoPtr
xf86OSMouseInit(int flags)
{
    OSMouseInfoPtr p;

    p = xcalloc(sizeof(OSMouseInfoRec), 1);
    if (!p)
        return NULL;
    p->SupportedInterfaces = SupportedInterfaces;
    p->BuiltinNames = BuiltinNames;
    p->DefaultProtocol = DefaultProtocol;
    p->CheckProtocol = CheckProtocol;
    p->PreInit = os2MousePreInit;
    p->SetupAuto = SetupAuto;
    return p;
}

void xf86OsMouseEvents()
{
	APIRET rc;
	ULONG postCount,dataLength;
	PVOID dummy;
	int buttons;
	int state;
	int i, dx,dy;
	BYTE elemPriority;
	REQUESTDATA requestData;

	MouseDevPtr pMse = iinfoPtr->private;

	if (!HandleValid) return;
	while((rc = DosReadQueue(hMouseQueue,
				 &requestData,&dataLength,&dummy, 
				 0L,1L,&elemPriority,hMouseSem)) == 0) {
		dx = requestData.ulData;
		(void)DosReadQueue(hMouseQueue,
				  &requestData,&dataLength,&dummy,
				  0L,1L,&elemPriority,hMouseSem);
		dy = requestData.ulData;
		(void)DosReadQueue(hMouseQueue,
				  &requestData,&dataLength,&dummy,
				  0L,1L,&elemPriority,hMouseSem);
		state = requestData.ulData;
		(void)DosReadQueue(hMouseQueue,
				  &requestData,&dataLength,&dummy,
				  0L,1L,&elemPriority,hMouseSem);
		if (requestData.ulData != 0xFFFFFFFF) 
			xf86Msg(X_ERROR,
				"Unexpected mouse event tag, %d\n",
				requestData.ulData);

		/* Contrary to other systems, OS/2 has mouse buttons *
		 * in the proper order, so we reverse them before    *
		 * sending the event.                                */
		 
		buttons = ((state & 0x06) ? 4 : 0) |
			  ((state & 0x18) ? 1 : 0) |
			  ((state & 0x60) ? 2 : 0);
		pMse->PostEvent(iinfoPtr, buttons, dx, dy, 0, 0);
	}
        DosResetEventSem(hMouseSem,&postCount);
}
