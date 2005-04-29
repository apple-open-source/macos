/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86Privstr.h,v 1.42 2004/02/13 23:58:38 dawes Exp $ */

/*
 * Copyright (c) 1997-2003 by The XFree86 Project, Inc.
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

/*
 * This file contains definitions of the private XFree86 data structures/types.
 * None of the data structures here should be used by video drivers.
 */ 

#ifndef _XF86PRIVSTR_H
#define _XF86PRIVSTR_H

#include "xf86Pci.h"
#include "xf86str.h"

/* PCI probe flags */

typedef enum {
    PCIProbe1		= 0,
    PCIProbe2,
    PCIForceConfig1,
    PCIForceConfig2,
    PCIForceNone,
    PCIOsConfig
} PciProbeType;

typedef enum {
    LogNone,
    LogFlush,
    LogSync
} Log;

typedef enum {
    SKNever,
    SKWhenNeeded,
    SKAlways
} SpecialKeysInDDX;

/*
 * xf86InfoRec contains global parameters which the video drivers never
 * need to access.  Global parameters which the video drivers do need
 * should be individual globals.
 */

typedef struct {

    /* keyboard part */
    DeviceIntPtr	pKeyboard;
    DeviceProc		kbdProc;		/* procedure for initializing */
    void		(* kbdEvents)(void);	/* proc for processing events */
    int			consoleFd;
    int			kbdFd;
    int			vtno;
    int			kbdType;		/* AT84 / AT101 */
    int			kbdRate;
    int			kbdDelay;
    int			bell_pitch;
    int			bell_duration;
    Bool		autoRepeat;
    unsigned long	leds;
    unsigned long	xleds;
    char *		vtinit;
    int			scanPrefix;		/* scancode-state */
    Bool		capsLock;
    Bool		numLock;
    Bool		scrollLock;
    Bool		modeSwitchLock;
    Bool		composeLock;
    Bool		vtSysreq;
    SpecialKeysInDDX	ddxSpecialKeys;
    Bool		ActionKeyBindingsSet;
#if defined(SVR4) && defined(i386)
    Bool		panix106;
#endif  /* SVR4 && i386 */
#if defined(__OpenBSD__) || defined(__NetBSD__)
    int                 wsKbdType;
#endif

    /* mouse part */
    DeviceIntPtr	pMouse;
#ifdef XINPUT
    pointer		mouseLocal;
#endif

    /* event handler part */
    int			lastEventTime;
    Bool		vtRequestsPending;
    Bool		inputPending;
    Bool		dontVTSwitch;
    Bool		dontZap;
    Bool		dontZoom;
    Bool		notrapSignals;	/* don't exit cleanly - die at fault */
    Bool		caughtSignal;

    /* graphics part */
    Bool		sharedMonitor;
    ScreenPtr		currentScreen;
#ifdef CSRG_BASED
    int			screenFd;	/* fd for memory mapped access to
					 * vga card */
    int			consType;	/* Which console driver? */
#endif

#ifdef XKB
    /* 
     * would like to use an XkbComponentNamesRec here but can't without
     * pulling in a bunch of header files. :-(
     */
    char *		xkbkeymap;
    char *		xkbkeycodes;
    char *		xkbtypes;
    char *		xkbcompat;
    char *		xkbsymbols;
    char *		xkbgeometry;
    Bool		xkbcomponents_specified;
    char *		xkbrules;
    char *		xkbmodel;
    char *		xkblayout;
    char *		xkbvariant;
    char *		xkboptions;
#endif

    /* Other things */
    Bool		allowMouseOpenFail;
    Bool		vidModeEnabled;		/* VidMode extension enabled */
    Bool		vidModeAllowNonLocal;	/* allow non-local VidMode
						 * connections */
    Bool		miscModInDevEnabled;	/* Allow input devices to be
						 * changed */
    Bool		miscModInDevAllowNonLocal;
    PciProbeType	pciFlags;
    Pix24Flags		pixmap24;
    MessageType		pix24From;
#if defined(i386) || defined(__i386__)
    Bool		pc98;
#endif
    Bool		pmFlag;
    Log			log;
    int			estimateSizesAggressively;
    Bool		kbdCustomKeycodes;
    Bool		disableRandR;
    MessageType		randRFrom;
    struct {
	Bool		disabled;		/* enable/disable deactivating
						 * grabs or closing the
						 * connection to the grabbing
						 * client */
	ClientPtr	override;		/* client that disabled
						 * grab deactivation.
						 */
	Bool		allowDeactivate;
	Bool		allowClosedown;
	ServerGrabInfoRec server;
    } grabInfo;
} xf86InfoRec, *xf86InfoPtr;

#ifdef DPMSExtension
/* Private info for DPMS */
typedef struct {
    CloseScreenProcPtr	CloseScreen;
    Bool		Enabled;
    int			Flags;
} DPMSRec, *DPMSPtr;
#endif

#ifdef XF86VIDMODE
/* Private info for Video Mode Extentsion */
typedef struct {
    DisplayModePtr	First;
    DisplayModePtr	Next;
    int			Flags;
    CloseScreenProcPtr	CloseScreen;
} VidModeRec, *VidModePtr;
#endif

/* Information for root window properties. */
typedef struct _RootWinProp {
    struct _RootWinProp *	next;
    char *			name;
    Atom			type;
    short			format;
    long			size;
    pointer			data;
} RootWinProp, *RootWinPropPtr;

/* private resource types */
#define ResNoAvoid  ResBios

/* ISC's cc can't handle ~ of UL constants, so explicitly type cast them. */
#define XLED1   ((unsigned long) 0x00000001)
#define XLED2   ((unsigned long) 0x00000002)
#define XLED3   ((unsigned long) 0x00000004)
#define XLED4	((unsigned long) 0x00000008)
#define XCAPS   ((unsigned long) 0x20000000)
#define XNUM    ((unsigned long) 0x40000000)
#define XSCR    ((unsigned long) 0x80000000)
#define XCOMP	((unsigned long) 0x00008000)

/* BSD console driver types (consType) */
#ifdef CSRG_BASED
#define PCCONS		   0
#define CODRV011	   1
#define CODRV01X	   2
#define SYSCONS		   8
#define PCVT		  16
#define WSCONS		  32
#endif

#endif /* _XF86PRIVSTR_H */
