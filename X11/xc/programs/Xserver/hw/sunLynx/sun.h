/* $Xorg: sun.h,v 1.3 2000/08/17 19:48:36 cpqbld Exp $ */

/* This is sun.h modified for LynxOS */

/*-
 * Copyright (c) 1987 by the Regents of the University of California
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */
/* $XFree86: xc/programs/Xserver/hw/sunLynx/sun.h,v 3.7 2001/07/25 15:05:02 dawes Exp $ */

#ifndef _SUN_H_ 
#define _SUN_H_

/* X headers */
#include "Xos.h"
#undef index /* don't mangle silly Sun structure member names */
#include "X.h"
#include "Xproto.h"

/* general system headers */
#ifndef NOSTDHDRS
# include <stdlib.h>
#else
# include <malloc.h>
extern char *getenv();
#endif

/* system headers common to both SunOS and Solaris */
#include <sys/param.h>
#include <sys/file.h>
#ifndef Lynx
# include <sys/filio.h>
#else
# include <sys/termio.h>	/* include this before ioctl.h ... */
#endif
#include <sys/ioctl.h>
#include <sys/resource.h>

#ifdef SVR4
# ifdef X_POSIX_C_SOURCE
#  define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#  include <signal.h>
#  undef _POSIX_C_SOURCE
# else
#  define _POSIX_SOURCE
#  include <signal.h>
#  undef _POSIX_SOURCE
# endif
#endif

#include <fcntl.h>

#ifndef __bsdi__
# if !defined(__NetBSD__) && !defined(__OpenBSD__)
#  ifndef Lynx
#   ifndef i386
#    include <poll.h>
#   else
#    include <sys/poll.h>
#   endif
#  endif
# endif
#else
# include <unistd.h>
#endif

#include <errno.h>
#include <memory.h>

/* 
 * Sun specific headers Sun moved in Solaris, and are different for NetBSD.
 *
 * Even if only needed by one source file, I have put them here 
 * to simplify finding them...
 */
#ifdef SVR4
# include <sys/fbio.h>
# include <sys/kbd.h>
# include <sys/kbio.h>
# include <sys/msio.h>
# include <sys/vuid_event.h>
# include <sys/memreg.h>
# include <stropts.h>
# define usleep(usec) poll((struct pollfd *) 0, (size_t) 0, usec / 1000)
#else
# if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(Lynx)
#  include <sun/fbio.h>
#  include <sundev/kbd.h>
#  include <sundev/kbio.h>
#  include <sundev/msio.h>
#  include <sundev/vuid_event.h>
#  include <pixrect/pixrect.h>
#  include <pixrect/memreg.h>
extern int ioctl();
extern int getrlimit();
extern int setrlimit();
extern int getpagesize();
# else
#  if defined (__NetBSD__) || defined (__OpenBSD__)
#   include <machine/fbio.h>
#   include <machine/kbd.h>
#   include <machine/kbio.h>
#   include <machine/vuid_event.h>
#  endif
#  ifdef __bsdi__
#   include <sys/fbio.h>
#   include </sys/sparc/dev/kbd.h>
#   include </sys/sparc/dev/kbio.h>
#   include </sys/sparc/dev/vuid_event.h>
#  endif
#  ifdef Lynx
#   ifndef __P
#    define __P(x)	x
#   endif
#   include <signal.h>
#   ifdef PATCHED_CONSOLE
#    include <kbio.h>
#    include <kbd.h>
#   else
#    define KB_SUN2		2		/* type 2 keyboard */
#    define KB_SUN3		3		/* type 3 keyboard */
#    define KB_SUN4		4		/* type 4 keyboard */
#    define KIOCSLED		TIOCLED
#    define LED_NUM_LOCK	NUM_LOCK
#    define LED_SCROLL_LOCK	SCROL_LOCK
#    define LED_CAPS_LOCK	CAPS_LOCK
#    define LED_COMPOSE		COMPOSE_LED
#   endif
#   include <smem.h>
#   include "fbio.h"
#   include "vuid_event.h"
#   define FBIORESET	_IO('F', 29)
#  endif
# endif
#endif
extern int gettimeofday();

/* 
 * Server specific headers
 */
#include "misc.h"
#undef abs /* don't munge function prototypes in headers, sigh */
#include "scrnintstr.h"
#ifdef NEED_EVENTS
# include "inputstr.h"
#endif
#include "input.h"
#include "colormapst.h"
#include "colormap.h"
#include "cursorstr.h"
#include "cursor.h"
#include "dixstruct.h"
#include "dix.h"
#include "opaque.h"
#include "resource.h"
#include "servermd.h"
#include "windowstr.h"

/* 
 * ddx specific headers 
 */
#ifndef PSZ
#define PSZ 8
#endif

#include "mi/mibstore.h"
#include "mi/mipointer.h"

extern int monitorResolution;


/* Frame buffer devices */
#ifdef SVR4
# define CGTWO0DEV	"/dev/fbs/cgtwo0"
# define CGTWO1DEV	"/dev/fbs/cgtwo1"
# define CGTWO2DEV	"/dev/fbs/cgtwo2"
# define CGTWO3DEV	"/dev/fbs/cgtwo3"
# define CGTHREE0DEV	"/dev/fbs/cgthree0"
# define CGTHREE1DEV	"/dev/fbs/cgthree1"
# define CGTHREE2DEV	"/dev/fbs/cgthree2"
# define CGTHREE3DEV	"/dev/fbs/cgthree3"
# define CGFOUR0DEV	"/dev/fbs/cgfour0"
# define CGSIX0DEV	"/dev/fbs/cgsix0"
# define CGSIX1DEV	"/dev/fbs/cgsix1"
# define CGSIX2DEV	"/dev/fbs/cgsix2"
# define CGSIX3DEV	"/dev/fbs/cgsix3"
# define BWTWO0DEV	"/dev/fbs/bwtwo0"
# define BWTWO1DEV	"/dev/fbs/bwtwo1"
# define BWTWO2DEV	"/dev/fbs/bwtwo2"
# define BWTWO3DEV	"/dev/fbs/bwtwo3"
# define CGEIGHT0DEV	"/dev/fbs/cgeight0"
#else
# define CGTWO0DEV	"/dev/cgtwo0"
# define CGTWO1DEV	"/dev/cgtwo1"
# define CGTWO2DEV	"/dev/cgtwo2"
# define CGTWO3DEV	"/dev/cgtwo3"
# define CGTHREE0DEV	"/dev/cgthree0"
# define CGTHREE1DEV	"/dev/cgthree1"
# define CGTHREE2DEV	"/dev/cgthree2"
# define CGTHREE3DEV	"/dev/cgthree3"
# define CGFOUR0DEV	"/dev/cgfour0"
# define CGSIX0DEV	"/dev/cgsix0"
# define CGSIX1DEV	"/dev/cgsix1"
# define CGSIX2DEV	"/dev/cgsix2"
# define CGSIX3DEV	"/dev/cgsix3"
# define BWTWO0DEV	"/dev/bwtwo0"
# define BWTWO1DEV	"/dev/bwtwo1"
# define BWTWO2DEV	"/dev/bwtwo2"
# define BWTWO3DEV	"/dev/bwtwo3"
# define CGEIGHT0DEV	"/dev/cgeight0"
#endif

/*
 * MAXEVENTS is the maximum number of events the mouse and keyboard functions
 * will read on a given call to their GetEvents vectors.
 */
#ifndef Lynx
#define MAXEVENTS 	32
#else
/* currently our mouse stuff is pretty naive, so we need a lot of events */
#define MAXEVENTS 	256
#endif

/*
 * Data private to any sun keyboard.
 */
typedef struct {
    int		fd;
    int		type;		/* Type of keyboard */
    int		layout;		/* The layout of the keyboard */
    int		click;		/* kbd click save state */
    Leds	leds;		/* last known LED state */
} sunKbdPrivRec, *sunKbdPrivPtr;

extern sunKbdPrivRec sunKbdPriv;

/*
 * Data private to any sun pointer device.
 */
typedef struct {
    int		fd;
    int		bmask;		/* last known button state */
} sunPtrPrivRec, *sunPtrPrivPtr;

extern sunPtrPrivRec sunPtrPriv;

typedef struct {
    BYTE	key;
    CARD8	modifiers;
} SunModmapRec;

typedef struct {
    int		    width, height;
    Bool	    has_cursor;
    CursorPtr	    pCursor;		/* current cursor */
} sunCursorRec, *sunCursorPtr;

typedef struct {
    ColormapPtr	    installedMap;
    CloseScreenProcPtr CloseScreen;
    void	    (*UpdateColormap)();
    sunCursorRec    hardwareCursor;
    Bool	    hasHardwareCursor;
} sunScreenRec, *sunScreenPtr;

#define GetScreenPrivate(s)   ((sunScreenPtr) ((s)->devPrivates[sunScreenIndex].ptr))
#define SetupScreen(s)	sunScreenPtr	pPrivate = GetScreenPrivate(s)

typedef struct {
#ifndef Lynx
    unsigned char*  fb;		/* Frame buffer itself */
#else
    unsigned char*  fbuf;	/* Frame buffer itself */
    void*  	    ramdac;	/* Color LookUp table */
    void*	    fb;		/* GX FB control registers: 
				 * sunGX.c relies on this name
				 */
    void*	    tec;	/* GX TEC registers */
    void*	    thc;	/* GX THC registers */
    void*	    fhc;	/* GX FHC registers */
#endif
    int		    fd;		/* frame buffer for ioctl()s, */
    struct fbtype   info;	/* Frame buffer characteristics */
    void	    (*EnterLeave)();/* screen switch */
    unsigned char*  fbPriv;	/* fbattr stuff, for the real type */
} fbFd;

typedef Bool (*sunFbInitProc)(
#if NeedFunctionPrototypes
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
#endif
);

typedef struct {
    sunFbInitProc	init;	/* init procedure for this fb */
    char*		name;	/* /usr/include/fbio names */
} sunFbDataRec;

#ifdef XKB
extern Bool		noXkbExtension;
#endif

extern Bool		sunAutoRepeatHandlersInstalled;
extern long		sunAutoRepeatInitiate;
extern long		sunAutoRepeatDelay;
extern sunFbDataRec	sunFbData[];
extern fbFd		sunFbs[];
extern Bool		sunSwapLkeys;
extern Bool		sunFlipPixels;
extern Bool		sunActiveZaphod;
extern Bool		sunFbInfo;
extern Bool		sunCG4Frob;
extern Bool		sunNoGX;
extern int		sunScreenIndex;
extern int*		sunProtected;

extern Bool sunCursorInitialize(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */
#endif
);

extern void sunDisableCursor(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */
#endif
);

extern int sunChangeKbdTranslation(
#if NeedFunctionPrototypes
    int /* fd */,
    Bool /* makeTranslated */
#endif
);

extern void sunNonBlockConsoleOff(
#if NeedFunctionPrototypes
#if defined(SVR4) || defined(CSRG_BASED) || defined(Lynx)
    void
#else
    char* /* arg */
#endif
#endif
);

extern void sunEnqueueEvents(
#if NeedFunctionPrototypes
    void
#endif
);

extern void sunEnqueueKbdEvents(
#if NeedFunctionPrototypes
    void
#endif
);

extern void sunEnqueueMseEvents(
#if NeedFunctionPrototypes
    void
#endif
);

extern int sunGXInit(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    fbFd* /* fb */
#endif
);

extern Bool sunSaveScreen(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    int /* on */
#endif
);

extern Bool sunScreenInit(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */
#endif
);

extern pointer sunMemoryMap(
#if NeedFunctionPrototypes
    size_t /* len */,
    off_t /* off */,
    int /* fd */
#ifdef Lynx
    , char * /* name */
#endif
#endif
);

extern Bool sunScreenAllocate(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */
#endif
);

extern Bool sunInitCommon(
#if NeedFunctionPrototypes
    int /* scrn */,
    ScreenPtr /* pScrn */,
    off_t /* offset */,
    Bool (* /* init1 */)(),
    void (* /* init2 */)(),
    Bool (* /* cr_cm */)(),
    Bool (* /* save */)(),
    int /* fb_off */
#endif
);

extern Firm_event* sunKbdGetEvents(
#if NeedFunctionPrototypes
    int /* fd */,
    Bool /* on */,
    int* /* pNumEvents */,
    Bool* /* pAgain */
#endif
);

extern Firm_event* sunMouseGetEvents(
#if NeedFunctionPrototypes
    int /* fd */,
    Bool /* on */,
    int* /* pNumEvents */,
    Bool* /* pAgain */
#endif
);

extern void sunKbdEnqueueEvent(
#if NeedFunctionPrototypes
    DeviceIntPtr /* device */,
    Firm_event* /* fe */
#endif
);

extern void sunMouseEnqueueEvent(
#if NeedFunctionPrototypes
    DeviceIntPtr /* device */,
    Firm_event* /* fe */
#endif
);

extern int sunKbdProc(
#if NeedFunctionPrototypes
    DeviceIntPtr /* pKeyboard */,
    int /* what */
#endif
);

extern int sunMouseProc(
#if NeedFunctionPrototypes
    DeviceIntPtr /* pMouse */,
    int /* what */
#endif
);

extern void sunKbdWait(
#if NeedFunctionPrototypes
    void
#endif
);

/*-
 * TVTOMILLI(tv)
 *	Given a struct timeval, convert its time into milliseconds...
 */
#define TVTOMILLI(tv)	(((tv).tv_usec/1000)+((tv).tv_sec*1000))

extern Bool sunCfbSetupScreen(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    pointer /* pbits */,	/* pointer to screen bitmap */
    int /* xsize */,		/* in pixels */
    int /* ysize */,
    int /* dpix */,		/* dots per inch */
    int /* dpiy */,		/* dots per inch */
    int /* width */,		/* pixel width of frame buffer */
    int	/* bpp */		/* bits per pixel of root */
#endif
);

extern Bool sunCfbFinishScreenInit(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    pointer /* pbits */,	/* pointer to screen bitmap */
    int /* xsize */,		/* in pixels */
    int /* ysize */,
    int /* dpix */,		/* dots per inch */
    int /* dpiy */,		/* dots per inch */
    int /* width */,		/* pixel width of frame buffer */
    int	/* bpp */		/* bits per pixel of root */
#endif
);

extern Bool sunCfbScreenInit(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    pointer /* pbits */,	/* pointer to screen bitmap */
    int /* xsize */,		/* in pixels */
    int /* ysize */,
    int /* dpix */,		/* dots per inch */
    int /* dpiy */,		/* dots per inch */
    int /* width */,		/* pixel width of frame buffer */
    int	/* bpp */		/* bits per pixel of root */
#endif
);

extern void sunInstallColormap(
#if NeedFunctionPrototypes
    ColormapPtr /* cmap */
#endif
);

extern void sunUninstallColormap(
#if NeedFunctionPrototypes
    ColormapPtr /* cmap */
#endif
);

extern int sunListInstalledColormaps(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    Colormap* /* pCmapList */
#endif
);

#endif
