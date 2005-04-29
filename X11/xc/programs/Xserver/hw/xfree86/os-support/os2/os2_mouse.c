/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_mouse.c,v 3.18 2003/03/25 04:18:24 dawes Exp $ */
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

/* The following support code was copied from mouse.c */

/**********************************************************************
 *
 *  Emulate3Button support code
 *
 **********************************************************************/


/*
 * Lets create a simple finite-state machine for 3 button emulation:
 *
 * We track buttons 1 and 3 (left and right).  There are 11 states:
 *   0 ground           - initial state
 *   1 delayed left     - left pressed, waiting for right
 *   2 delayed right    - right pressed, waiting for left
 *   3 pressed middle   - right and left pressed, emulated middle sent
 *   4 pressed left     - left pressed and sent
 *   5 pressed right    - right pressed and sent
 *   6 released left    - left released after emulated middle
 *   7 released right   - right released after emulated middle
 *   8 repressed left   - left pressed after released left
 *   9 repressed right  - right pressed after released right
 *  10 pressed both     - both pressed, not emulating middle
 *
 * At each state, we need handlers for the following events
 *   0: no buttons down
 *   1: left button down
 *   2: right button down
 *   3: both buttons down
 *   4: emulate3Timeout passed without a button change
 * Note that button events are not deltas, they are the set of buttons being
 * pressed now.  It's possible (ie, mouse hardware does it) to go from (eg)
 * left down to right down without anything in between, so all cases must be
 * handled.
 *
 * a handler consists of three values:
 *   0: action1
 *   1: action2
 *   2: new emulation state
 *
 * action > 0: ButtonPress
 * action = 0: nothing
 * action < 0: ButtonRelease
 *
 * The comment preceeding each section is the current emulation state.
 * The comments to the right are of the form
 *      <button state> (<events>) -> <new emulation state>
 * which should be read as
 *      If the buttons are in <button state>, generate <events> then go to
 *      <new emulation state>.
 */
static signed char stateTab[11][5][3] = {
/* 0 ground */
  {
    {  0,  0,  0 },   /* nothing -> ground (no change) */
    {  0,  0,  1 },   /* left -> delayed left */
    {  0,  0,  2 },   /* right -> delayed right */
    {  2,  0,  3 },   /* left & right (middle press) -> pressed middle */
    {  0,  0, -1 }    /* timeout N/A */
  },
/* 1 delayed left */
  {
    {  1, -1,  0 },   /* nothing (left event) -> ground */
    {  0,  0,  1 },   /* left -> delayed left (no change) */
    {  1, -1,  2 },   /* right (left event) -> delayed right */
    {  2,  0,  3 },   /* left & right (middle press) -> pressed middle */
    {  1,  0,  4 },   /* timeout (left press) -> pressed left */
  },
/* 2 delayed right */
  {
    {  3, -3,  0 },   /* nothing (right event) -> ground */
    {  3, -3,  1 },   /* left (right event) -> delayed left (no change) */
    {  0,  0,  2 },   /* right -> delayed right (no change) */
    {  2,  0,  3 },   /* left & right (middle press) -> pressed middle */
    {  3,  0,  5 },   /* timeout (right press) -> pressed right */
  },
/* 3 pressed middle */
  {
    { -2,  0,  0 },   /* nothing (middle release) -> ground */
    {  0,  0,  7 },   /* left -> released right */
    {  0,  0,  6 },   /* right -> released left */
    {  0,  0,  3 },   /* left & right -> pressed middle (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 4 pressed left */
  {
    { -1,  0,  0 },   /* nothing (left release) -> ground */
    {  0,  0,  4 },   /* left -> pressed left (no change) */
    { -1,  0,  2 },   /* right (left release) -> delayed right */
    {  3,  0, 10 },   /* left & right (right press) -> pressed both */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 5 pressed right */
  {
    { -3,  0,  0 },   /* nothing (right release) -> ground */
    { -3,  0,  1 },   /* left (right release) -> delayed left */
    {  0,  0,  5 },   /* right -> pressed right (no change) */
    {  1,  0, 10 },   /* left & right (left press) -> pressed both */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 6 released left */
  {
    { -2,  0,  0 },   /* nothing (middle release) -> ground */
    { -2,  0,  1 },   /* left (middle release) -> delayed left */
    {  0,  0,  6 },   /* right -> released left (no change) */
    {  1,  0,  8 },   /* left & right (left press) -> repressed left */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 7 released right */
  {
    { -2,  0,  0 },   /* nothing (middle release) -> ground */
    {  0,  0,  7 },   /* left -> released right (no change) */
    { -2,  0,  2 },   /* right (middle release) -> delayed right */
    {  3,  0,  9 },   /* left & right (right press) -> repressed right */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 8 repressed left */
  {
    { -2, -1,  0 },   /* nothing (middle release, left release) -> ground */
    { -2,  0,  4 },   /* left (middle release) -> pressed left */
    { -1,  0,  6 },   /* right (left release) -> released left */
    {  0,  0,  8 },   /* left & right -> repressed left (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 9 repressed right */
  {
    { -2, -3,  0 },   /* nothing (middle release, right release) -> ground */
    { -3,  0,  7 },   /* left (right release) -> released right */
    { -2,  0,  5 },   /* right (middle release) -> pressed right */
    {  0,  0,  9 },   /* left & right -> repressed right (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 10 pressed both */
  {
    { -1, -3,  0 },   /* nothing (left release, right release) -> ground */
    { -3,  0,  4 },   /* left (right release) -> pressed left */
    { -1,  0,  5 },   /* right (left release) -> pressed right */
    {  0,  0, 10 },   /* left & right -> pressed both (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
};

/*
 * Table to allow quick reversal of natural button mapping to correct mapping
 */

/*
 * [JCH-96/01/21] The ALPS GlidePoint pad extends the MS protocol
 * with a fourth button activated by tapping the PAD.
 * The 2nd line corresponds to 4th button on; the drv sends
 * the buttons in the following map (MSBit described first) :
 * 0 | 4th | 1st | 2nd | 3rd
 * And we remap them (MSBit described first) :
 * 0 | 4th | 3rd | 2nd | 1st
 */
static char reverseMap[32] = { 0,  4,  2,  6,  1,  5,  3,  7,
			       8, 12, 10, 14,  9, 13, 11, 15,
			      16, 20, 18, 22, 17, 21, 19, 23,
			      24, 28, 26, 30, 25, 29, 27, 31};


static char hitachMap[16] = {  0,  2,  1,  3,
			       8, 10,  9, 11,
			       4,  6,  5,  7,
			      12, 14, 13, 15 };

#define reverseBits(map, b)	(((b) & ~0x0f) | map[(b) & 0x0f])

static CARD32
buttonTimer(InputInfoPtr pInfo)
{
    MouseDevPtr pMse;
    int	sigstate;
    int id;

    pMse = pInfo->private;

    sigstate = xf86BlockSIGIO ();

    pMse->emulate3Pending = FALSE;
    if ((id = stateTab[pMse->emulateState][4][0]) != 0) {
        xf86PostButtonEvent(pInfo->dev, 0, abs(id), (id >= 0), 0, 0);
        pMse->emulateState = stateTab[pMse->emulateState][4][2];
    } else {
        ErrorF("Got unexpected buttonTimer in state %d\n", pMse->emulateState);
    }

    xf86UnblockSIGIO (sigstate);
    return 0;
}

static Bool
Emulate3ButtonsSoft(InputInfoPtr pInfo)
{
    MouseDevPtr pMse = pInfo->private;

    if (!pMse->emulate3ButtonsSoft)
	return TRUE;

    pMse->emulate3Buttons = FALSE;

    if (pMse->emulate3Pending)
	buttonTimer(pInfo);

    xf86Msg(X_INFO,"3rd Button detected: disabling emulate3Button\n");

    return FALSE;
}

static void MouseBlockHandler(pointer data,
			      struct timeval **waitTime,
			      pointer LastSelectMask)
{
    InputInfoPtr    pInfo = (InputInfoPtr) data;
    MouseDevPtr	    pMse = (MouseDevPtr) pInfo->private;
    int		    ms;

    if (pMse->emulate3Pending)
    {
	ms = pMse->emulate3Expires - GetTimeInMillis ();
	if (ms <= 0)
	    ms = 0;
	AdjustWaitForDelay (waitTime, ms);
    }
}

static void MouseWakeupHandler(pointer data,
			       int i,
			       pointer LastSelectMask)
{
    InputInfoPtr    pInfo = (InputInfoPtr) data;
    MouseDevPtr	    pMse = (MouseDevPtr) pInfo->private;
    int		    ms;

    if (pMse->emulate3Pending)
    {
	ms = pMse->emulate3Expires - GetTimeInMillis ();
	if (ms <= 0)
	    buttonTimer (pInfo);
    }
}

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
	if (pMse->emulate3Buttons || pMse->emulate3ButtonsSoft)
	{
	    RegisterBlockAndWakeupHandlers (MouseBlockHandler, MouseWakeupHandler,
					    (pointer) pInfo);
	}
	break;

    case DEVICE_CLOSE:
    case DEVICE_OFF:
	if (!HandleValid) return -1;
        if (pMse->emulate3Buttons || pMse->emulate3ButtonsSoft)
        {
	    RemoveBlockAndWakeupHandlers (MouseBlockHandler, MouseWakeupHandler,
	    			      (pointer) pInfo);
        }
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
