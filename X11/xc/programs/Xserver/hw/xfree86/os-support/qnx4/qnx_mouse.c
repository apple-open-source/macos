/*
 * (c) Copyright 1998 by Sebastien Marineau
 *			<sebastien@qnx.com>
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
 * SEBASTIEN MARINEAU BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/qnx4/qnx_mouse.c,v 1.4 2002/01/07 20:38:29 dawes Exp $
 */

/* This module contains the qnx-specific functions to access the keyboard
 * and the console.
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <i86.h>
#include <sys/mman.h>
#include <sys/dev.h>
#include <sys/mouse.h>
#include <sys/proxy.h>
#include <errno.h>

#include "X.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Xinput.h"
#include "xf86OSmouse.h"

extern int miPointerGetMotionEvents(
#if NeedFunctionPrototypes
    DeviceIntPtr /*pPtr*/,
    xTimecoord * /*coords*/,
    unsigned long /*start*/,
    unsigned long /*stop*/,
    ScreenPtr /*pScreen*/
#endif
);

struct _mouse_ctrl *QNX_mouse = NULL;
pid_t QNX_mouse_proxy = -1;
Bool QNX_mouse_event = FALSE;

/* the following function is converted from old void xf86OsMouseEvents() */
static void
OsMouseReadInput(InputInfoPtr pInfo)
{
	struct mouse_event events[16];
	int i, nEvents;
	int buttons, col, row;
	int armed = 0;
	MouseDevPtr pMse;

	pMse = pInfo->private;

	while ((nEvents = mouse_read(QNX_mouse, &events, 
		16, QNX_mouse_proxy, &armed) ) > 0) {
		/* ErrorF("Got mouse event, #%d!\n", nEvents);*/

		for (i = 0; i < nEvents; i ++){	
			col = events[i].dx;
			row = -events[i].dy;
			buttons = events[i].buttons; 
			pMse->PostEvent(pInfo, buttons, col, row, 0, 0);
			}
		}
	if (!armed) ErrorF("Drained mouse queue, armed = 0??\n");
	QNX_mouse_event = FALSE;
}

/* The main mouse setup proc */
static int
OsMouseProc(pPointer, what)
DeviceIntPtr pPointer;
int what;
{
	int i, ret, armed;
	int nbuttons;
	unsigned char *map;
	struct mouse_event mevent;
        MouseDevPtr pMse;
        InputInfoPtr pInfo;

        pInfo = pPointer->public.devicePrivate;
        pMse = pInfo->private;
        pMse->device = pPointer;

	switch (what) {
	case DEVICE_INIT:
		pPointer->public.on = FALSE;
		if (QNX_mouse_proxy == -1) {
			if((QNX_mouse_proxy = 
				qnx_proxy_attach(0, 0, 0, -1)) == -1){
				FatalError("xf86MouseOn: Could not create mouse proxy; %s\n", 
				strerror(errno));
				}
			}
		if (QNX_mouse == NULL) 	QNX_mouse = 
			mouse_open(0, NULL, xf86Info.consoleFd);
		if (QNX_mouse == NULL) {
                	if (xf86AllowMouseOpenFail) {
                        	ErrorF("Cannot open mouse (%s) - Continuing...\n",
                                	strerror(errno));
                        	return(-1);
                        	}
                	FatalError("Cannot open mouse (%s)\n", strerror(errno));
                	}
		/* Ok, so we have opened the channel to the mouse driver */
		ErrorF("Opened mouse: handle %d buttons\n", QNX_mouse->handle, 
			QNX_mouse->buttons);	
		pInfo->fd = QNX_mouse->fd;
        	mouse_flush(QNX_mouse);
		QNX_mouse_event = FALSE;
		/* How de we determine how many buttons we have?? */
		nbuttons = 3;
		map = (unsigned char *) xalloc(nbuttons + 1);
		if (map == (unsigned char *) NULL)
			FatalError("Failed to allocate memory for mouse structures\n");
		for(i=0;i <= nbuttons; i++)
			map[i] = i;			
		InitPointerDeviceStruct ((DevicePtr) pPointer, map, nbuttons, 
			miPointerGetMotionEvents, pMse->Ctrl,
			miPointerGetMotionBufferSize());

		/* X valuator */
		xf86InitValuatorAxisStruct(pPointer, 0, 0, -1, 1, 0, 1);
		xf86InitValuatorDefaults(pPointer, 0);
		/* Y valuator */
		xf86InitValuatorAxisStruct(pPointer, 1, 0, -1, 1, 0, 1);
		xf86InitValuatorDefaults(pPointer, 1);
		xf86MotionHistoryAllocate(pInfo);

		xfree(map);
		break;

	case DEVICE_ON:
		if(QNX_mouse == NULL) return(-1);
		pMse->lastButtons = 0;
		pMse->emulateState = 0;
		pPointer->public.on = TRUE;
        	mouse_flush(QNX_mouse);
		/* AddEnabledDevice(pInfo->fd); */
		ret = mouse_read(QNX_mouse, &mevent, 0, 
			QNX_mouse_proxy, NULL);
		ErrorF("MouseOn: armed proxy, %d, proxy pid %d\n", ret, 
			QNX_mouse_proxy);
		if (ret < 0) { 
			FatalError("xf86MouseOn: could not arm proxy; %s\n",
				strerror(errno));
			}
		break;

	case DEVICE_CLOSE:
	case DEVICE_OFF:
		if(QNX_mouse == NULL) return(-1);
		pPointer->public.on = FALSE;
		if (what == DEVICE_CLOSE){
			mouse_close (QNX_mouse);
			QNX_mouse = NULL;
			}
                pPointer->public.on = FALSE;
		break;
	}
	return (Success);
}				

static int
SupportedInterfaces(void)
{
    /* XXX Need to check this. */
    return MSE_SERIAL | MSE_BUS | MSE_PS2 | MSE_XPS2 | MSE_MISC | MSE_AUTO;
}

static const char *internalNames[] = {
        "OSMouse",
        NULL
};

static const char **
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

/* XXX Is this appropriate?  If not, this function should be removed. */
static const char *
DefaultProtocol(void)
{
    return "OSMouse";
}

static Bool
OsMousePreInit(InputInfoPtr pInfo, const char *protocol, int flags)
{
    MouseDevPtr pMse;

    /* This is called when the protocol is "OSMouse". */

    pMse = pInfo->private;
    pMse->protocol = protocol;
    xf86Msg(X_CONFIG, "%s: Protocol: %s\n", pInfo->name, protocol);

    /* Collect the options, and process the common options. */
    xf86CollectInputOptions(pInfo, NULL, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options);

    /* Process common mouse options (like Emulate3Buttons, etc). */
    pMse->CommonOptions(pInfo);

    /* Setup the local procs. */
    pInfo->device_control = OsMouseProc;
    pInfo->read_input = OsMouseReadInput;

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
    p->PreInit = OsMousePreInit;
    return p;
}
