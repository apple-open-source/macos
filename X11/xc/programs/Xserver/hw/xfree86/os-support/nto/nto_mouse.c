/*
 * Written by Frank Liu Oct 10, 2001
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/nto/nto_mouse.c,v 1.1 2001/11/16 16:47:56 dawes Exp $ */


#include "X.h"
#include "xf86.h"
#include "xf86Xinput.h"
#include "xf86OSmouse.h"

/* copied from mipointer.h */
extern int miPointerGetMotionEvents(
#if NeedFunctionPrototypes
    DeviceIntPtr /*pPtr*/,
    xTimecoord * /*coords*/,
    unsigned long /*start*/,
    unsigned long /*stop*/,
    ScreenPtr /*pScreen*/
#endif
);

#include <sys/dcmd_input.h>
#define NUMEVENTS    64   /* don't want to stuck in the mouse read loop */

/*
 * OsMouseReadInput --
 *      Get some events from our queue.  Process outstanding events now.
 */
static void
OsMouseReadInput(InputInfoPtr pInfo)
{
        int n = 0;
	int buttons, col, row;
        struct _mouse_packet mp;
	MouseDevPtr pMse;

	pMse = pInfo->private;

        while ( (read(pInfo->fd, &mp, sizeof(struct _mouse_packet)) > 0 )
                && (n < NUMEVENTS ) ) 
        {
              col = mp.dx;
              row = -mp.dy;
              buttons = mp.hdr.buttons;
              pMse->PostEvent(pInfo, buttons, col, row, 0, 0);
              n++;
        }
}

/*
 * OsMouseProc --
 *      Handle the initialization, etc. of a mouse
 */
static int
OsMouseProc(pPointer, what)
DeviceIntPtr pPointer;
int what;
{
	int nbuttons;
	unsigned char map[MSE_MAXBUTTONS + 1];
        MouseDevPtr pMse;
        InputInfoPtr pInfo;

        pInfo = pPointer->public.devicePrivate;
        pMse = pInfo->private;
        pMse->device = pPointer;

	switch (what) {
	case DEVICE_INIT:
		pPointer->public.on = FALSE;

                for (nbuttons = 0; nbuttons < MSE_MAXBUTTONS; ++nbuttons)
                    map[nbuttons + 1] = nbuttons + 1;

                InitPointerDeviceStruct((DevicePtr)pPointer,
                                        map,
                                        min(pMse->buttons, MSE_MAXBUTTONS),
                                        miPointerGetMotionEvents,
                                        pMse->Ctrl,
                                        miPointerGetMotionBufferSize());

                /* X valuator */
                xf86InitValuatorAxisStruct(pPointer, 0, 0, -1, 1, 0, 1);
                xf86InitValuatorDefaults(pPointer, 0);
                /* Y valuator */
                xf86InitValuatorAxisStruct(pPointer, 1, 0, -1, 1, 0, 1);
                xf86InitValuatorDefaults(pPointer, 1);
                xf86MotionHistoryAllocate(pInfo);
                break;

	case DEVICE_ON:
                pInfo->fd = xf86OpenSerial(pInfo->options);
                if (pInfo->fd == -1)
                    xf86Msg(X_WARNING, "%s: cannot open input device\n", pInfo->name);
                else {
                    AddEnabledDevice(pInfo->fd);
                }
                pMse->lastButtons = 0;
                pMse->emulateState = 0;
                pPointer->public.on = TRUE;
                break;

	case DEVICE_CLOSE:
	case DEVICE_OFF:
                if (pInfo->fd != -1) {
                    RemoveEnabledDevice(pInfo->fd);
                    xf86CloseSerial(pInfo->fd);
                    pInfo->fd = -1;
                }
                pPointer->public.on = FALSE;
                break;
	}
	return (Success);
}				

static int
SupportedInterfaces(void)
{
  /* FIXME: Is this correct? Should we just return MSE_MISC? */
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

    /* Check if the device can be opened. */
    pInfo->fd = xf86OpenSerial(pInfo->options);
    if (pInfo->fd == -1) {
        if (xf86GetAllowMouseOpenFail())
            xf86Msg(X_WARNING, "%s: cannot open input device\n", pInfo->name);
        else {
            xf86Msg(X_ERROR, "%s: cannot open input device\n", pInfo->name);
            xfree(pMse);
            return FALSE;
        }
    }
    xf86CloseSerial(pInfo->fd);
    pInfo->fd = -1;

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
