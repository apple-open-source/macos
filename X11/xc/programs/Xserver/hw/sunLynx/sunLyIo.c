/* $Xorg: sunLyIo.c,v 1.3 2000/08/17 19:48:37 cpqbld Exp $ */
/*-
 * This is sunIo.c modified for LynxOS
 * Copyright 1996 by Thomas Mueller
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Mueller not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Mueller makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS MUELLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS MUELLER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/sunLynx/sunLyIo.c,v 3.5 2001/05/25 18:40:59 dawes Exp $ */

 /*
 *
 * Copyright 1987 by the Regents of the University of California
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 */

/************************************************************
Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.

                    All Rights Reserved

Permission  to  use,  copy,  modify,  and  distribute   this
software  and  its documentation for any purpose and without
fee is hereby granted, provided that the above copyright no-
tice  appear  in all copies and that both that copyright no-
tice and this permission notice appear in  supporting  docu-
mentation,  and  that the names of Sun or The Open Group
not be used in advertising or publicity pertaining to 
distribution  of  the software  without specific prior 
written permission. Sun and The Open Group make no 
representations about the suitability of this software for 
any purpose. It is provided "as is" without any express or 
implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

#define NEED_EVENTS
#include    "sun.h"
#include    <stat.h>

/*-
 *-----------------------------------------------------------------------
 * ProcessInputEvents --
 *	Retrieve all waiting input events and pass them to DIX in their
 *	correct chronological order. Only reads from the system pointer
 *	and keyboard.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Events are passed to the DIX layer.
 *
 *-----------------------------------------------------------------------
 */
void
ProcessInputEvents ()
{
    mieqProcessInputEvents ();
    miPointerUpdate ();
}

/*
 *-----------------------------------------------------------------------
 * sunEnqueueEvents
 *	When a SIGIO is received, read device hard events and
 *	enqueue them using the mi event queue
 */

void sunEnqueueEvents (
#if NeedFunctionPrototypes
    void
#endif
)
{
    Firm_event	*ptrEvents,    	/* Current pointer event */
		*kbdEvents;    	/* Current keyboard event */
    int		numPtrEvents, 	/* Number of remaining pointer events */
		numKbdEvents;   /* Number of remaining keyboard events */
    int		nPE,   	    	/* Original number of pointer events */
		nKE;   	    	/* Original number of keyboard events */
    Bool	PtrAgain,	/* need to (re)read */
		KbdAgain;	/* need to (re)read */
    DeviceIntPtr	pPointer;
    DeviceIntPtr	pKeyboard;
    sunKbdPrivPtr       kbdPriv;
    sunPtrPrivPtr       ptrPriv;

    pPointer = (DeviceIntPtr)LookupPointerDevice();
    pKeyboard = (DeviceIntPtr)LookupKeyboardDevice();
    ptrPriv = (sunPtrPrivPtr) pPointer->public.devicePrivate;
    kbdPriv = (sunKbdPrivPtr) pKeyboard->public.devicePrivate;
    if (!pPointer->public.on || !pKeyboard->public.on)
	return;

    numPtrEvents = 0;
    PtrAgain = TRUE;
    numKbdEvents = 0;
    KbdAgain = TRUE;

    /*
     * So long as one event from either device remains unprocess, we loop:
     * Take the oldest remaining event and pass it to the proper module
     * for processing. The DDXEvent will be sent to ProcessInput by the
     * function called.
     */
    while (1) {
	/*
	 * Get events from both the pointer and the keyboard, storing the number
	 * of events gotten in nPE and nKE and keeping the start of both arrays
	 * in pE and kE
	 */
	if ((numPtrEvents == 0) && PtrAgain) {
	    ptrEvents = sunMouseGetEvents (ptrPriv->fd, pPointer->public.on, 
					   &nPE, &PtrAgain);
	    numPtrEvents = nPE;
	}
	if ((numKbdEvents == 0) && KbdAgain) {
	    kbdEvents = sunKbdGetEvents (kbdPriv->fd, pKeyboard->public.on,
					 &nKE, &KbdAgain);
	    numKbdEvents = nKE;
	}
	if ((numPtrEvents == 0) && (numKbdEvents == 0))
	    break;
	if (numPtrEvents && numKbdEvents) {
	    if (timercmp (&kbdEvents->time, &ptrEvents->time, <)) {
		sunKbdEnqueueEvent (pKeyboard, kbdEvents);
		numKbdEvents--;
		kbdEvents++;
	    } else {
		sunMouseEnqueueEvent (pPointer, ptrEvents);
		numPtrEvents--;
		ptrEvents++;
	    }
	} else if (numKbdEvents) {
	    sunKbdEnqueueEvent (pKeyboard, kbdEvents);
	    numKbdEvents--;
	    kbdEvents++;
	} else {
	    sunMouseEnqueueEvent (pPointer, ptrEvents);
	    numPtrEvents--;
	    ptrEvents++;
	}
    }
}

void sunEnqueueKbdEvents (
#if NeedFunctionPrototypes
    void
#endif
)
{
    Firm_event	*kbdEvents;    	/* Current keyboard event */
    int		numKbdEvents;   /* Number of remaining keyboard events */
    int		nKE;   	    	/* Original number of keyboard events */
    Bool	KbdAgain;	/* need to (re)read */
    DeviceIntPtr	pKeyboard;
    sunKbdPrivPtr       kbdPriv;

    pKeyboard = (DeviceIntPtr)LookupKeyboardDevice();
    kbdPriv = (sunKbdPrivPtr) pKeyboard->public.devicePrivate;
    if (!pKeyboard->public.on)
	return;

    numKbdEvents = 0;
    KbdAgain = TRUE;

    /*
     * So long as one event from the device remains unprocess, we loop:
     * Take the oldest remaining event and pass it to the proper module
     * for processing. The DDXEvent will be sent to ProcessInput by the
     * function called.
     */
    while (1) {
	/*
	 * Get events from both the keyboard, storing the number
	 * of events gotten in nKE and keeping the start of the array
	 * in kE
	 */
	if ((numKbdEvents == 0) && KbdAgain) {
	    kbdEvents = sunKbdGetEvents (kbdPriv->fd, pKeyboard->public.on,
					 &nKE, &KbdAgain);
	    numKbdEvents = nKE;
	}
	if (numKbdEvents == 0)
	    break;
	sunKbdEnqueueEvent (pKeyboard, kbdEvents);
	numKbdEvents--;
	kbdEvents++;
    }
}

void sunEnqueueMseEvents (
#if NeedFunctionPrototypes
    void
#endif
)
{
    Firm_event	*ptrEvents;    	/* Current pointer event */
    int		numPtrEvents; 	/* Number of remaining pointer events */
    int		nPE;   	    	/* Original number of pointer events */
    Bool	PtrAgain;	/* need to (re)read */
    DeviceIntPtr	pPointer;
    sunPtrPrivPtr       ptrPriv;

    pPointer = (DeviceIntPtr)LookupPointerDevice();
    ptrPriv = (sunPtrPrivPtr) pPointer->public.devicePrivate;
    if (!pPointer->public.on)
	return;

    numPtrEvents = 0;
    PtrAgain = TRUE;

    /*
     * So long as one events from the device remain unprocess, we loop:
     * Take the oldest remaining event and pass it to the proper module
     * for processing. The DDXEvent will be sent to ProcessInput by the
     * function called.
     */
    while (1) {
	/*
	 * Get events from the pointer, storing the number
	 * of events gotten in nPE and keeping the start of the arrays
	 * in pE
	 */
	if ((numPtrEvents == 0) && PtrAgain) {
	    ptrEvents = sunMouseGetEvents (ptrPriv->fd, pPointer->public.on, 
					   &nPE, &PtrAgain);
	    numPtrEvents = nPE;
	}
	if (numPtrEvents == 0)
	    break;

	sunMouseEnqueueEvent (pPointer, ptrEvents);
	numPtrEvents--;
	ptrEvents++;
    }
}

/*
 * DDX - specific abort routine.  Called by AbortServer().
 */
void AbortDDX()
{
    int		i;
    ScreenPtr	pScreen;
    DevicePtr	devPtr;

    (void) OsSignal (SIGIO, SIG_IGN);
    if (sunKbdPriv.fd != -1) {
        if (devPtr = LookupKeyboardDevice())
	    (void) sunChangeKbdTranslation (((sunKbdPrivPtr)(devPtr->devicePrivate))->fd, FALSE);
    }
    sunNonBlockConsoleOff ();
    for (i = 0; i < screenInfo.numScreens; i++)
    {
	pScreen = screenInfo.screens[i];
	(*pScreen->SaveScreen) (pScreen, SCREEN_SAVER_OFF);
	sunDisableCursor (pScreen);
    }
}

/* Called by GiveUp(). */
void
ddxGiveUp()
{
    AbortDDX ();
}

int
ddxProcessArgument (argc, argv, i)
    int	argc;
    char *argv[];
    int	i;
{
    extern void UseMsg();
    extern Bool onConsole;
    struct stat s0, scon;

    if (fstat(0, &s0) != -1 && stat("/dev/con", &scon) != -1)
	onConsole = !memcmp(&s0, &scon, sizeof(s0));

#ifndef XKB
    if (strcmp (argv[i], "-ar1") == 0) {	/* -ar1 int */
	if (++i >= argc) UseMsg ();
	sunAutoRepeatInitiate = 1000 * (long)atoi(argv[i]);
	if (sunAutoRepeatInitiate > 1000000)
	    sunAutoRepeatInitiate =  999000;
	return 2;
    }
    if (strcmp (argv[i], "-ar2") == 0) {	/* -ar2 int */
	if (++i >= argc) UseMsg ();
	sunAutoRepeatDelay = 1000 * (long)atoi(argv[i]);
	if (sunAutoRepeatDelay > 1000000)
	    sunAutoRepeatDelay =  999000;
	return 2;
    }
#endif
    if (strcmp (argv[i], "-swapLkeys") == 0) {	/* -swapLkeys */
	sunSwapLkeys = TRUE;
	return 1;
    }
    if (strcmp (argv[i], "-debug") == 0) {	/* -debug */
	return 1;
    }
    if (strcmp (argv[i], "-dev") == 0) {	/* -dev /dev/mumble */
	if (++i >= argc) UseMsg ();
	return 2;
    }
    if (strcmp (argv[i], "-mono") == 0) {	/* -mono */
	return 1;
    }
    if (strcmp (argv[i], "-zaphod") == 0) {	/* -zaphod */
	sunActiveZaphod = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-flipPixels") == 0) {	/* -flipPixels */
	sunFlipPixels = TRUE;
	return 1;
    }
    if (strcmp (argv[i], "-fbinfo") == 0) {	/* -fbinfo */
	sunFbInfo = TRUE;
	return 1;
    }
    if (strcmp (argv[i], "-kbd") == 0) {	/* -kbd */
	if (++i >= argc) UseMsg();
	return 2;
    }
    if (strcmp (argv[i], "-protect") == 0) {	/* -protect */
	if (++i >= argc) UseMsg();
	return 2;
    }
    if (strcmp (argv[i], "-noGX") == 0) {
	sunNoGX = TRUE;
	return 1;
    }
    return 0;
}

void
ddxUseMsg()
{
#ifndef XKB
    ErrorF("-ar1 int            set autorepeat initiate time\n");
    ErrorF("-ar2 int            set autorepeat interval time\n");
#endif
    ErrorF("-swapLkeys          swap keysyms on L1..L10\n");
    ErrorF("-debug              disable non-blocking console mode\n");
    ErrorF("-dev fn[:fn][:fn]   name of device[s] to open\n");
    ErrorF("-mono               force monochrome-only screen\n");
    ErrorF("-zaphod             disable active Zaphod mode\n");
    ErrorF("-fbinfo             tell more about the found frame buffer(s)\n");
#ifdef UNDOCUMENTED
    ErrorF("-noGX               treat the GX as a dumb frame buffer\n");
#endif
}
