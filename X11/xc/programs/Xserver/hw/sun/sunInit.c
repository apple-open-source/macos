/* $Xorg: sunInit.c,v 1.4 2000/08/17 19:48:29 cpqbld Exp $ */
/*
 * sunInit.c --
 *	Initialization functions for screen/keyboard/mouse, etc.
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
/* $XFree86: xc/programs/Xserver/hw/sun/sunInit.c,v 3.13 2003/11/17 22:20:36 dawes Exp $ */

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

*******************************************************/

#include    "sun.h"
#include    "gcstruct.h"
#include    "mi.h"
#include    "mibstore.h"
#include    "cfb.h"
#include    "cfb16.h"
#include    "cfb32.h"

/* maximum pixmap depth */
#ifndef SUNMAXDEPTH
#define SUNMAXDEPTH 8
#endif

#ifdef i386 /* { */
#define BW2I NULL
#else /* }{ */
extern Bool sunBW2Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define BW2I sunBW2Init
#endif /* } */

#ifdef LOWMEMFTPT
#define BW2I NULL
#endif /* ifdef LOWMEMFTPT */

#if SUNMAXDEPTH == 1 /* { */
#define CG2I NULL
#define CG3I NULL
#define CG4I NULL
#define CG6I NULL
#define CG8I NULL
#define TCXI NULL
#else /* }{ */
extern Bool sunCG3Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define CG3I sunCG3Init
#if defined(i386) || defined(__bsdi__) /* { */
#define CG2I NULL
#define CG4I NULL
#else /* }{ */
#ifdef INCLUDE_CG2_HEADER
extern Bool sunCG2Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define CG2I sunCG2Init
#endif /* INCLUDE_CG2_HEADER */
extern Bool sunCG4Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define CG4I sunCG4Init
#endif /* } */
#ifdef FBTYPE_SUNFAST_COLOR /* { */
extern Bool sunCG6Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define CG6I sunCG6Init
#else /* }{ */
#define CG6I NULL
#endif /* } */
#ifdef XFBTYPE_TCX  /* { */
extern Bool sunTCXInit(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define TCXI sunTCXInit
#else /* }{ */
#define TCXI NULL
#endif /* } */
#if SUNMAXDEPTH > 8 /* { */
#ifdef FBTYPE_MEMCOLOR /* { */
extern Bool sunCG8Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define CG8I sunCG8Init
#else /* }{ */
#define CG8I NULL
#endif /* } */
#else /* }{ */
#define CG8I NULL
#endif /* } */

#endif /* } */

extern KeySymsRec sunKeySyms[];
extern SunModmapRec *sunModMaps[];
extern int sunMaxLayout;
extern KeySym* sunType4KeyMaps[];
extern SunModmapRec* sunType4ModMaps[];

static Bool	sunDevsInited = FALSE;

Bool sunAutoRepeatHandlersInstalled;	/* FALSE each time InitOutput called */
Bool sunSwapLkeys = FALSE;
Bool sunFlipPixels = FALSE;
Bool sunFbInfo = FALSE;
Bool sunCG4Frob = FALSE;
Bool sunNoGX = FALSE;

sunKbdPrivRec sunKbdPriv = {
    -1,		/* fd */
    -1,		/* type */
    -1,		/* layout */
    0,		/* click */
    (Leds)0,	/* leds */
};

sunPtrPrivRec sunPtrPriv = {
    -1,		/* fd */
    0		/* Current button state */
};

/*
 * The name member in the following table corresponds to the 
 * FBTYPE_* macros defined in /usr/include/sun/fbio.h file
 */
sunFbDataRec sunFbData[XFBTYPE_LASTPLUSONE] = {
  { NULL, "SUN1BW        (bw1)" },
  { NULL, "SUN1COLOR     (cg1)" },
  { BW2I, "SUN2BW        (bw2)" },	
#ifdef INCLUDE_CG2_HEADER
  { CG2I, "SUN2COLOR     (cg2)" },
#endif /* INCLUDE_CG2_HEADER */
  { NULL, "SUN2GP        (gp1/gp2)" },
  { NULL, "SUN5COLOR     (cg5/386i accel)" },
  { CG3I, "SUN3COLOR     (cg3)" },
  { CG8I, "MEMCOLOR      (cg8)" },
  { CG4I, "SUN4COLOR     (cg4)" },
  { NULL, "NOTSUN1" },
  { NULL, "NOTSUN2" },
  { NULL, "NOTSUN3" }	
#ifndef i386 /* { */
 ,{ CG6I, "SUNFAST_COLOR (cg6/gx)" },
  { NULL, "SUNROP_COLOR  (cg9)" },
  { NULL, "SUNFB_VIDEO" },
  { NULL, "SUNGIFB" },
  { NULL, "SUNPLAS" },
#ifdef FBTYPE_SUNGP3
  { NULL, "SUNGP3        (cg12/gs)" },
#endif
#ifdef FBTYPE_SUNGT
  { NULL, "SUNGT         (gt)" },
#endif
#ifdef FBTYPE_SUNLEO
  { NULL, "SUNLEO        (zx)" },
#endif
#ifdef FBTYPE_MDICOLOR
  { NULL, "MDICOLOR      (cgfourteen)" },
#endif
#ifdef XFBTYPE_TCX
  { TCXI, "TCX           (tcx)" },
#endif
#endif /* } */
};

/*
 * a list of devices to try if there is no environment or command
 * line list of devices
 */
#if SUNMAXDEPTH == 1 /* { */
static char *fallbackList[] = {
    BWTWO0DEV, BWTWO1DEV, BWTWO2DEV
};
#else /* }{ */
static char *fallbackList[] = {
#ifndef i386 /* { */
    CGTWO0DEV, CGTWO1DEV, CGTWO2DEV,
#if (MAXSCREENS == 4)
    CGTWO3DEV,
#endif
#endif /* } */
    CGTHREE0DEV,
#ifndef i386 /* { */
    CGTHREE1DEV, CGTHREE2DEV,
#if (MAXSCREENS == 4)
    CGTHREE3DEV,
#endif
#endif /* } */
#ifdef FBTYPE_SUNFAST_COLOR /* { */
    CGSIX0DEV, CGSIX1DEV, CGSIX2DEV,
#if (MAXSCREENS == 4)
    CGSIX3DEV,
#endif
#endif /* } */
#ifndef i386 /* { */
    CGFOUR0DEV, BWTWO0DEV, BWTWO1DEV, BWTWO2DEV,
#if (MAXSCREENS == 4)
    BWTWO3DEV,
#endif
#endif /* } */
#if SUNMAXDEPTH > 8 /* { */
    CGEIGHT0DEV,
#if 0
#ifdef XFBTYPE_TCX
    TCX0DEV,
#endif
#endif
#endif /* } */
    "/dev/fb"
};
#endif /* } */

#define FALLBACK_LIST_LEN sizeof fallbackList / sizeof fallbackList[0]

fbFd sunFbs[MAXSCREENS];

static PixmapFormatRec	formats[] = {
    { 1, 1, BITMAP_SCANLINE_PAD	} /* 1-bit deep */
#if SUNMAXDEPTH > 1
    ,{ 8, 8, BITMAP_SCANLINE_PAD} /* 8-bit deep */
#if SUNMAXDEPTH > 8
    ,{ 12, 24, BITMAP_SCANLINE_PAD } /* 12-bit deep */
    ,{ 24, 32, BITMAP_SCANLINE_PAD } /* 24-bit deep */
#endif
#endif
};
#define NUMFORMATS	(sizeof formats)/(sizeof formats[0])

/*
 * OpenFrameBuffer --
 *	Open a frame buffer according to several rules.
 *	Find the device to use by looking in the sunFbData table,
 *	an XDEVICE envariable, a -dev switch or using /dev/fb if trying
 *	to open screen 0 and all else has failed.
 *
 * Results:
 *	The fd of the framebuffer.
 */
static int OpenFrameBuffer(device, screen)
    char		*device;	/* e.g. "/dev/cgtwo0" */
    int			screen;    	/* what screen am I going to be */
{
    int			ret = TRUE;
    struct fbgattr	*fbattr;
    static int		devFbUsed;

    sunFbs[screen].fd = -1;
    if (strcmp (device, "/dev/fb") == 0 && devFbUsed)
	return FALSE;
    if (access (device, R_OK | W_OK) == -1)
	return FALSE;
    if ((sunFbs[screen].fd = open(device, O_RDWR, 0)) == -1)
	ret = FALSE;
    else {
	fbattr = (struct fbgattr *) xalloc (sizeof (struct fbgattr));
	if (ioctl(sunFbs[screen].fd, FBIOGATTR, fbattr) == -1) {
	    /*
		This is probably a bwtwo; the $64,000 question is: 
		is it the mono plane of a cgfour, or is it really a 
		real bwtwo?  If there is only a cgfour in the box or 
		only a bwtwo in the box, then it isn't a problem.  If 
		it's a 3/60, which has a bwtwo on the mother board *and* 
		a cgfour, then things get tricky because there's no way 
		to tell if the bwtwo is really being emulated by the cgfour.
	    */
	    xfree (fbattr);
	    fbattr = NULL;
	    if (ioctl(sunFbs[screen].fd, FBIOGTYPE, &sunFbs[screen].info) == -1) {
		Error("unable to get frame buffer attributes");
		(void) close(sunFbs[screen].fd);
		sunFbs[screen].fd = -1;
		return FALSE; 
	    }
	}
	if (ret) {
	    devFbUsed = TRUE;
	    if (fbattr) {
		if (fbattr->fbtype.fb_type >= XFBTYPE_LASTPLUSONE) {
		    ErrorF ("%s is an unknown framebuffer type\n", device);
		    (void) close(sunFbs[screen].fd);
		    sunFbs[screen].fd = -1;
		    return FALSE;
		}
		sunFbs[screen].info = fbattr->fbtype;
	    }
	    sunFbs[screen].fbPriv = (pointer) fbattr;
	    if (fbattr && !sunFbData[fbattr->fbtype.fb_type].init) {
		int _i;
		ret = FALSE;
		for (_i = 0; _i < FB_ATTR_NEMUTYPES; _i++) {
		    if (sunFbData[fbattr->emu_types[_i]].init) {
			sunFbs[screen].info.fb_type = fbattr->emu_types[_i];
			ret = TRUE;
			if (sunFbInfo)
			    ErrorF ("%s is emulating a %s\n", device,
				sunFbData[fbattr->fbtype.fb_type].name);
			break;
		    }
		}
	    }
	    if (sunFbInfo) 
		ErrorF ("%s is really a %s\n", device, 
		    sunFbData[fbattr ? fbattr->fbtype.fb_type : sunFbs[screen].info.fb_type].name);
	}
    }
    if (!ret)
	sunFbs[screen].fd = -1;
    return ret;
}

/*-
 *-----------------------------------------------------------------------
 * SigIOHandler --
 *	Signal handler for SIGIO - input is available.
 *
 * Results:
 *	sunSigIO is set - ProcessInputEvents() will be called soon.
 *
 * Side Effects:
 *	None
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void SigIOHandler(sig)
    int		sig;
{
    int olderrno = errno;
    sunEnqueueEvents ();
    errno = olderrno;
}

/*-
 *-----------------------------------------------------------------------
 * sunNonBlockConsoleOff --
 *	Turn non-blocking mode on the console off, so you don't get logged
 *	out when the server exits.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
void sunNonBlockConsoleOff(
#if defined(SVR4) || defined(CSRG_BASED)
    void
#else
    char* arg
#endif
)
{
    register int i;

    i = fcntl(2, F_GETFL, 0);
    if (i >= 0)
	(void) fcntl(2, F_SETFL, i & ~FNDELAY);
}

static char** GetDeviceList (argc, argv)
    int		argc;
    char	**argv;
{
    int		i;
    char	*envList = NULL;
    char	*cmdList = NULL;
    char	**deviceList = (char **)NULL; 

    for (i = 1; i < argc; i++)
	if (strcmp (argv[i], "-dev") == 0 && i+1 < argc) {
	    cmdList = argv[i + 1];
	    break;
	}
    if (!cmdList)
	envList = getenv ("XDEVICE");

    if (cmdList || envList) {
	char	*_tmpa;
	char	*_tmpb;
	int	_i1;
	deviceList = (char **) xalloc ((MAXSCREENS + 1) * sizeof (char *));
	_tmpa = (cmdList) ? cmdList : envList;
	for (_i1 = 0; _i1 < MAXSCREENS; _i1++) {
	    _tmpb = strtok (_tmpa, ":");
	    if (_tmpb)
		deviceList[_i1] = _tmpb;
	    else
		deviceList[_i1] = NULL;
	    _tmpa = NULL;
	}
	deviceList[MAXSCREENS] = NULL;
    }
    if (!deviceList) {
	/* no environment and no cmdline, so default */
	deviceList = 
	    (char **) xalloc ((FALLBACK_LIST_LEN + 1) * sizeof (char *));
	for (i = 0; i < FALLBACK_LIST_LEN; i++)
	    deviceList[i] = fallbackList[i];
	deviceList[FALLBACK_LIST_LEN] = NULL;
    }
    return deviceList;
}

static void getKbdType()
{
/*
 * The Sun 386i has system include files that preclude this pre SunOS 4.1
 * test for the presence of a type 4 keyboard however it really doesn't
 * matter since no 386i has ever been shipped with a type 3 keyboard.
 * SunOS 4.1 no longer needs this kludge.
 */
#if !defined(i386) && !defined(KIOCGKEY)
#define TYPE4KEYBOARDOVERRIDE
#endif

    int ii;

    for (ii = 0; ii < 3; ii++) {
	sunKbdWait();
	(void) ioctl (sunKbdPriv.fd, KIOCTYPE, &sunKbdPriv.type);
#ifdef TYPE4KEYBOARDOVERRIDE
	/*
	 * Magic. Look for a key which is non-existent on a real type
	 * 3 keyboard but does exist on a type 4 keyboard.
	 */
	if (sunKbdPriv.type == KB_SUN3) {
	    struct kiockeymap key;

	    key.kio_tablemask = 0;
	    key.kio_station = 118;
	    if (ioctl(sunKbdPriv.fd, KIOCGKEY, &key) == -1) {
		Error( "ioctl KIOCGKEY" );
		FatalError("Can't KIOCGKEY on fd %d\n", sunKbdPriv.fd);
	    }
	    if (key.kio_entry != HOLE)
		sunKbdPriv.type = KB_SUN4;
	}
#endif
	switch (sunKbdPriv.type) {
	case KB_SUN2:
	case KB_SUN3:
	case KB_SUN4: return;
	default: 
	    sunChangeKbdTranslation(sunKbdPriv.fd, FALSE);
	    continue;
	}
    }
    FatalError ("Unsupported keyboard type %d\n", sunKbdPriv.type);
}

void OsVendorInit(
    void
)
{
    static int inited;
    if (!inited) {
#ifndef i386
	struct rlimit rl;

	/* 
	 * one per client, one per screen, one per listen endpoint,
	 * keyboard, mouse, and stderr
	 */
	int maxfds = MAXCLIENTS + MAXSCREENS + 5;

	if (getrlimit (RLIMIT_NOFILE, &rl) == 0) {
	    rl.rlim_cur = maxfds < rl.rlim_max ? maxfds : rl.rlim_max;
	    (void) setrlimit (RLIMIT_NOFILE, &rl);
	}
#endif
	sunKbdPriv.fd = open ("/dev/kbd", O_RDWR, 0);
	if (sunKbdPriv.fd < 0)
	    FatalError ("Cannot open /dev/kbd, error %d\n", errno);
	sunPtrPriv.fd = open ("/dev/mouse", O_RDWR, 0);
	if (sunPtrPriv.fd < 0)
	    FatalError ("Cannot open /dev/mouse, error %d\n", errno);
	getKbdType ();
	if (sunKbdPriv.type == KB_SUN4) {
	    (void) ioctl (sunKbdPriv.fd, KIOCLAYOUT, &sunKbdPriv.layout);
	    if (sunKbdPriv.layout < 0 ||
		sunKbdPriv.layout > sunMaxLayout ||
		sunType4KeyMaps[sunKbdPriv.layout] == NULL)
		FatalError ("Unsupported keyboard type 4 layout %d\n",
			    sunKbdPriv.layout);
	    sunKeySyms[KB_SUN4].map = sunType4KeyMaps[sunKbdPriv.layout];
	    sunModMaps[KB_SUN4] = sunType4ModMaps[sunKbdPriv.layout];
        }
	inited = 1;
    }
}

#ifdef DDXOSFATALERROR
void OsVendorFatalError(void)
{
}
#endif

/*-
 *-----------------------------------------------------------------------
 * InitOutput --
 *	Initialize screenInfo for all actually accessible framebuffers.
 *	The
 *
 * Results:
 *	screenInfo init proc field set
 *
 * Side Effects:
 *	None
 *
 *-----------------------------------------------------------------------
 */

void InitOutput(pScreenInfo, argc, argv)
    ScreenInfo 	  *pScreenInfo;
    int     	  argc;
    char    	  **argv;
{
    int     	i, scr;
    int		nonBlockConsole = 0;
    char	**devList;
    static int	setup_on_exit = 0;
    extern Bool	RunFromSmartParent;

    if (!monitorResolution)
	monitorResolution = 90;
    if (RunFromSmartParent)
	nonBlockConsole = 1;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-debug"))
	    nonBlockConsole = 0;
    }

    /*
     *	Writes to /dev/console can block - causing an
     *	excess of error messages to hang the server in
     *	deadlock.  So.......
     */
    if (nonBlockConsole) {
	if (!setup_on_exit) {
#if defined(SVR4) || defined(CSRG_BASED)
	    if (atexit(sunNonBlockConsoleOff))
#else
	    if (on_exit(sunNonBlockConsoleOff, (char *)0))
#endif
		ErrorF("InitOutput: can't register NBIO exit handler\n");

	    setup_on_exit = 1;
	}
	i = fcntl(2, F_GETFL, 0);
	if (i >= 0)
	    i = fcntl(2, F_SETFL, i | FNDELAY);
	if (i < 0) {
	    Error("fcntl");
	    ErrorF("InitOutput: can't put stderr in non-block mode\n");
	}
    }
    pScreenInfo->imageByteOrder = IMAGE_BYTE_ORDER;
    pScreenInfo->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
    pScreenInfo->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
    pScreenInfo->bitmapBitOrder = BITMAP_BIT_ORDER;

    pScreenInfo->numPixmapFormats = NUMFORMATS;
    for (i=0; i< NUMFORMATS; i++)
        pScreenInfo->formats[i] = formats[i];
#ifdef XKB
    if (noXkbExtension)
#endif
    sunAutoRepeatHandlersInstalled = FALSE;
    if (!sunDevsInited) {
	/* first time ever */
	for (scr = 0; scr < MAXSCREENS; scr++)
	    sunFbs[scr].fd = -1;
	devList = GetDeviceList (argc, argv);
	for (i = 0, scr = 0; devList[i] != NULL && scr < MAXSCREENS; i++)
	    if (OpenFrameBuffer (devList[i], scr))
		scr++;
	sunDevsInited = TRUE;
	xfree (devList);
    }
    for (scr = 0; scr < MAXSCREENS; scr++)
	if (sunFbs[scr].fd != -1)
	    (void) AddScreen (sunFbData[sunFbs[scr].info.fb_type].init, 
			      argc, argv);
    (void) OsSignal(SIGWINCH, SIG_IGN);
}

/*-
 *-----------------------------------------------------------------------
 * InitInput --
 *	Initialize all supported input devices...what else is there
 *	besides pointer and keyboard?
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Two DeviceRec's are allocated and registered as the system pointer
 *	and keyboard devices.
 *
 *-----------------------------------------------------------------------
 */
void InitInput(argc, argv)
    int     	  argc;
    char    	  **argv;
{
    pointer	p, k;
    extern Bool mieqInit();

    p = AddInputDevice(sunMouseProc, TRUE);
    k = AddInputDevice(sunKbdProc, TRUE);
    if (!p || !k)
	FatalError("failed to create input devices in InitInput");

    RegisterPointerDevice(p);
    RegisterKeyboardDevice(k);
    miRegisterPointerDevice(screenInfo.screens[0], p);
    (void) mieqInit (k, p);
#define SET_FLOW(fd) fcntl(fd, F_SETFL, FNDELAY | FASYNC)
#ifdef SVR4
    (void) OsSignal(SIGPOLL, SigIOHandler);
#define WANT_SIGNALS(fd) ioctl(fd, I_SETSIG, S_INPUT | S_HIPRI)
#else
    (void) OsSignal(SIGIO, SigIOHandler);
#define WANT_SIGNALS(fd) fcntl(fd, F_SETOWN, getpid())
#endif
    if (sunKbdPriv.fd >= 0) {
	if (SET_FLOW(sunKbdPriv.fd) == -1 || WANT_SIGNALS(sunKbdPriv.fd) == -1) {	
	    (void) close (sunKbdPriv.fd);
	    sunKbdPriv.fd = -1;
	    FatalError("Async kbd I/O failed in InitInput");
	}
    }
    if (sunPtrPriv.fd >= 0) {
	if (SET_FLOW(sunPtrPriv.fd) == -1 || WANT_SIGNALS(sunPtrPriv.fd) == -1) {	
	    (void) close (sunPtrPriv.fd);
	    sunPtrPriv.fd = -1;
	    FatalError("Async mouse I/O failed in InitInput");
	}
    }
}


#if SUNMAXDEPTH == 8

Bool
sunCfbSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int	bpp;			/* bits per pixel of root */
{
    return cfbSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy,
			  width);
}

Bool
sunCfbFinishScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int bpp;			/* bits per pixel of root */
{
    return cfbFinishScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy,
			       width);
}

Bool
sunCfbScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int bpp;			/* bits per pixel of root */
{
    return cfbScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width);
}

#else /* SUNMAXDEPTH != 8 */
#if SUNMAXDEPTH == 32

static Bool
sunCfbCreateGC(pGC)
    GCPtr   pGC;
{
    if (pGC->depth == 1)
    {
	return mfbCreateGC (pGC);
    }
    else if (pGC->depth <= 8)
    {
	return cfbCreateGC (pGC);
    }
    else if (pGC->depth <= 16)
    {
	return cfb16CreateGC (pGC);
    }
    else if (pGC->depth <= 32)
    {
	return cfb32CreateGC (pGC);
    }
    return FALSE;
}

static void
sunCfbGetSpans(pDrawable, wMax, ppt, pwidth, nspans, pdstStart)
    DrawablePtr		pDrawable;	/* drawable from which to get bits */
    int			wMax;		/* largest value of all *pwidths */
    register DDXPointPtr ppt;		/* points to start copying from */
    int			*pwidth;	/* list of number of bits to copy */
    int			nspans;		/* number of scanlines to copy */
    char		*pdstStart;	/* where to put the bits */
{
    switch (pDrawable->bitsPerPixel) {
    case 1:
	mfbGetSpans(pDrawable, wMax, ppt, pwidth, nspans, pdstStart);
	break;
    case 8:
	cfbGetSpans(pDrawable, wMax, ppt, pwidth, nspans, pdstStart);
	break;
    case 16:
	cfb16GetSpans(pDrawable, wMax, ppt, pwidth, nspans, pdstStart);
	break;
    case 32:
	cfb32GetSpans(pDrawable, wMax, ppt, pwidth, nspans, pdstStart);
	break;
    }
    return;
}

static void
sunCfbGetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine)
    DrawablePtr pDrawable;
    int		sx, sy, w, h;
    unsigned int format;
    unsigned long planeMask;
    char	*pdstLine;
{
    switch (pDrawable->bitsPerPixel)
    {
    case 1:
	mfbGetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);
	break;
    case 8:
	cfbGetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);
	break;
    case 16:
	cfb16GetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);
	break;
    case 32:
	cfb32GetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);
	break;
    }
}

Bool
sunCfbSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int	bpp;			/* bits per pixel of root */
{
    int ret;

    switch (bpp) {
    case 8:
	ret = cfbSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width);
	break;
    case 16:
	ret = cfb16SetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width);
	break;
    case 32:
	ret = cfb32SetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width);
	break;
    default:
	return FALSE;
    }
    pScreen->CreateGC = sunCfbCreateGC;
    pScreen->GetImage = sunCfbGetImage;
    pScreen->GetSpans = sunCfbGetSpans;
    return ret;
}

/* Adapt cfb logic */
#undef CFB_NEED_SCREEN_PRIVATE
#if !defined(SINGLEDEPTH) || defined(FORDE_SEPARATE_PRIVATE)
#define CFB_NEED_SCREEN_PRIVATE
#endif

extern BSFuncRec cfbBSFuncRec, cfb16BSFuncRec, cfb32BSFuncRec;
extern Bool	 cfbCloseScreen(), cfb16CloseScreen(), cfb32CloseScreen();
#ifdef CFB_NEED_SCREEN_PRIVATE
extern int	 cfb16ScreenPrivateIndex, cfb32ScreenPrivateIndex;
#endif

Bool
sunCfbFinishScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int bpp;
{
#ifdef CFB_NEED_SCREEN_PRIVATE
    pointer	oldDevPrivate;
#endif
    VisualPtr	visuals;
    int		nvisuals;
    DepthPtr	depths;
    int		ndepths;
    VisualID	defaultVisual;
    int		rootdepth = 0;

    if (!cfbInitVisuals(&visuals, &depths, &nvisuals, &ndepths,
			&rootdepth, &defaultVisual, 1 << (bpp - 1), 8))
	return FALSE;
#ifdef CFB_NEED_SCREEN_PRIVATE
    oldDevPrivate = pScreen->devPrivate;
#endif
    if (! miScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width,
			rootdepth, ndepths, depths,
			defaultVisual, nvisuals, visuals))
	return FALSE;
    switch (bpp)
    {
    case 8:
	pScreen->CloseScreen = cfbCloseScreen;
	pScreen->BackingStoreFuncs = cfbBSFuncRec;
	break;
    case 16:
	pScreen->CloseScreen = cfb16CloseScreen;
	pScreen->BackingStoreFuncs = cfb16BSFuncRec;
#ifdef CFB_NEED_SCREEN_PRIVATE
	pScreen->devPrivates[cfb16ScreenPrivateIndex].ptr =
	    pScreen->devPrivate;
	pScreen->devPrivate = oldDevPrivate;
#endif
	break;
    case 32:
	pScreen->CloseScreen = cfb32CloseScreen;
	pScreen->BackingStoreFuncs = cfb32BSFuncRec;
#ifdef CFB_NEED_SCREEN_PRIVATE
	pScreen->devPrivates[cfb32ScreenPrivateIndex].ptr =
	    pScreen->devPrivate;
	pScreen->devPrivate = oldDevPrivate;
#endif
	break;
    }
    return TRUE;
}


Bool
sunCfbScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int bpp;
{
    if (!sunCfbSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy,
			   width, bpp))
	return FALSE;
    return sunCfbFinishScreenInit(pScreen, pbits, xsize, ysize, dpix,
				  dpiy, width, bpp);
}

#endif  /* SUNMAXDEPTH == 32 */
#endif  /* SUNMAXDEPTH */

#ifdef DPMSExtension
/**************************************************************
 * DPMSSet(), DPMSGet(), DPMSSupported()
 *
 * stubs
 *
 ***************************************************************/

void DPMSSet (level)
    int level;
{
}

int DPMSGet (level)
    int* level;
{
    return -1;
}

Bool DPMSSupported ()
{
    return FALSE;
}
#endif
