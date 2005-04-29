/* $Xorg: sunLyInit.c,v 1.3 2000/08/17 19:48:36 cpqbld Exp $ */
/*
 * This is sunInit.c modified for LynxOS
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
/* $XFree86: xc/programs/Xserver/hw/sunLynx/sunLyInit.c,v 3.9 2003/11/17 22:20:37 dawes Exp $ */

/*
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

*******************************************************/

#include    "sun.h"
#include    "gcstruct.h"
#include    "mi.h"
#include    "mibstore.h"
#include    "cfb.h"

Bool onConsole = FALSE;		/* wether stdin is /dev/con */

/* maximum pixmap depth */
#ifndef SUNMAXDEPTH
#define SUNMAXDEPTH 8
#endif

extern Bool sunBW2Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define BW2I sunBW2Init
#if SUNMAXDEPTH == 1 /* { */
#define CG3I NULL
#define CG6I NULL
#else /* }{ */
extern Bool sunCG3Init(
    int /* screen */,
    ScreenPtr /* pScreen */,
    int /* argc */,
    char** /* argv */
);
#define CG3I sunCG3Init
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
sunFbDataRec sunFbData[FBTYPE_LASTPLUSONE] = {
  { NULL, "SUN1BW        (bw1)" },
  { NULL, "SUN1COLOR     (cg1)" },
  { BW2I, "SUN2BW        (bw2)" },	
  { NULL, "SUN2COLOR     (cg2)" },
  { NULL, "SUN2GP        (gp1/gp2)" },
  { NULL, "SUN5COLOR     (cg5/386i accel)" },
  { CG3I, "SUN3COLOR     (cg3)" },
  { NULL, "MEMCOLOR      (cg8)" },
  { NULL, "SUN4COLOR     (cg4)" },
  { NULL, "NOTSUN1" },
  { NULL, "NOTSUN2" },
  { NULL, "NOTSUN3" },
  { CG6I, "SUNFAST_COLOR (cg6/gx)" },		/* last we need */
};

/*
 * a list of devices to try if there is no environment or command
 * line list of devices
 */
#if SUNMAXDEPTH == 1 /* { */
static char *fallbackList[] = {
    "/dev/bwtwo",
};
#else /* }{ */
static char *fallbackList[] = {
    "/dev/bwtwo", "/dev/cgthree", "/dev/cgsix",
};
#endif /* } */

#define FALLBACK_LIST_LEN sizeof fallbackList / sizeof fallbackList[0]

fbFd sunFbs[MAXSCREENS];

static PixmapFormatRec	formats[] = {
    { 1, 1, BITMAP_SCANLINE_PAD	} /* 1-bit deep */
#if SUNMAXDEPTH > 1
    ,{ 8, 8, BITMAP_SCANLINE_PAD} /* 8-bit deep */
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
    int			ret;
    unsigned long dacoffset;
    struct fbgattr	*fbattr = NULL;
    static int		devFbUsed;
    static struct fbgattr bw2 = {
	0, 0,
        { FBTYPE_SUN2BW, 900, 1152, 1, 0, 0x00100000 },
        { 0, -1},
	-1
    };
    static struct fbgattr cg3 = {
	0, 0,
        { FBTYPE_SUN3COLOR, 900, 1152, 8, 256, 0x00100000 },
        { 0, -1},
 	-1
    };
    static struct fbgattr cg6 = {
	0, 0,
        { FBTYPE_SUNFAST_COLOR, 900, 1152, 8, 256, 0x00100000 },
        { 0, -1},
 	-1
    };
	    

    sunFbs[screen].fd = sunKbdPriv.fd;	/* /dev/con or /dev/kbd */
    devFbUsed = TRUE;
    /* apply some magic to work out what we're running on.
     * why couldn't they just spend some time on a little
     * FBIOGATTR ioctl()
     */

    ret = FALSE;
    if (ioctl(sunFbs[screen].fd, TIO_QUERYRAMDAC, &dacoffset) < 0)
        FatalError("can't query DAC addr\n");
    if (dacoffset == 0x400000) {
    	if (strcmp(device, "/dev/cgthree") == 0)
            fbattr = &cg3;
    }
    else if (dacoffset == 0x200000) {
        if (strcmp(device, "/dev/cgsix") == 0)
	   fbattr = &cg6;
    }
#ifdef PATCHED_CONSOLE
    else if (dacoffset == 0) {
    	if (strcmp(device, "/dev/bw2") == 0)
            fbattr = &bw2;
    }
#endif
    else
        ErrorF("bogus DAC addr 0x%x, maybe it's the silly BWTWO bug\n", dacoffset);
    	    
    if (fbattr) {
	ret = TRUE;
	sunFbs[screen].info = fbattr->fbtype;
    }
    sunFbs[screen].fbPriv = (pointer) fbattr;
    if (fbattr && fbattr->fbtype.fb_type < FBTYPE_LASTPLUSONE && 
	!sunFbData[fbattr->fbtype.fb_type].init) {
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
/*static*/ void SigIOHandler(sig)
    int		sig;
{
    int olderrno = errno;
    sunEnqueueEvents();
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
    void
)
{
    int i;

#if 0
    if (sunKbdPriv.fd >= 0) 
    {
	i = fcntl(sunKbdPriv.fd, F_GETFL, 0);
	if (i >= 0 && (i & FNDELAY)) {
	    (void) fcntl(sunKbdPriv.fd, F_SETFL, i & ~FNDELAY);
	}
    }
#endif
    for (i = 0; i < MAXSCREENS; i++) {
    	if (sunFbs[i].fbuf) {
    		smem_create(NULL, (char*)sunFbs[i].fbuf, 0, SM_DETACH);
    		smem_remove("FB");
		sunFbs[i].fbuf = NULL;
    	}
    	if (sunFbs[i].ramdac) {
    		smem_create(NULL, (char*)sunFbs[i].ramdac, 0, SM_DETACH);
    		smem_remove("DAC");
		sunFbs[i].ramdac = NULL;
    	}
    	if (sunFbs[i].fhc) {
    		smem_create(NULL, (char*)sunFbs[i].fhc, 0, SM_DETACH);
    		smem_remove("FHC_THC");
		sunFbs[i].fhc = NULL;
		sunFbs[i].thc = NULL;
    	}
    	if (sunFbs[i].fb) {
    		smem_create(NULL, (char*)sunFbs[i].fb, 0, SM_DETACH);
    		smem_remove("FBC_TEC");
		sunFbs[i].fb = NULL;
		sunFbs[i].tec = NULL;
    	}
    }
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
#if defined(PATCHED_CONSOLE)
    int ii;

    for (ii = 0; ii < 3; ii++) {
#if 0
	sunKbdWait();
#endif
	if (ioctl (sunKbdPriv.fd, KIOCTYPE, &sunKbdPriv.type) < 0 && errno == EINVAL) {
	    ErrorF("failed to get keyboard type, maybe wrong console driver:");
	    ErrorF(" assuming Type 4 keyboard\n");
	    sunKbdPriv.type = KB_SUN4;
	    return;
	}
	switch (sunKbdPriv.type) {
	case KB_SUN2:
	case KB_SUN3:
	case KB_SUN4:
	    return;
	default: 
	    sunChangeKbdTranslation(sunKbdPriv.fd, FALSE);
	    continue;
	}
    }
    FatalError ("Unsupported keyboard type %d\n", sunKbdPriv.type);
#else
    sunKbdPriv.type = KB_SUN4;
#endif
}

void OsVendorInit(
    void
)
{
    static int inited;
    if (!inited) {
	/* weird hack to prevent logout on X server shutdown */
	if (onConsole)
	    sunKbdPriv.fd = open ("/dev/con", O_RDWR, 0);
	else
	    sunKbdPriv.fd = open ("/dev/kbd", O_RDWR, 0);
	sunPtrPriv.fd = open ("/dev/mouse", O_RDWR, 0);

	getKbdType ();
	if (sunKbdPriv.type == KB_SUN4) {
#if defined(PATCHED_CONSOLE)
	    if ( ioctl (sunKbdPriv.fd, KIOCLAYOUT, &sunKbdPriv.layout) < 0 && errno == EINVAL) {
	    	ErrorF("failed to get keyboard layout, maybe wrong console driver:");
	    	ErrorF(" assuming layout 0\n");
	    	sunKbdPriv.layout = 0;
	    }
	    if (sunKbdPriv.layout < 0 ||
		sunKbdPriv.layout > sunMaxLayout ||
		sunType4KeyMaps[sunKbdPriv.layout] == NULL)
		FatalError ("Unsupported keyboard type 4 layout %d\n",
			    sunKbdPriv.layout);
#else
	    sunKbdPriv.layout = 0;	/* default: Type 4 */
#endif
	    sunKeySyms[KB_SUN4].map = sunType4KeyMaps[sunKbdPriv.layout];
	    sunModMaps[KB_SUN4] = sunType4ModMaps[sunKbdPriv.layout];
        }
	inited = 1;
    }
}

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
    char	**devList;
    static int	setup_on_exit = 0;
    extern Bool	RunFromSmartParent;

    if (!monitorResolution)
	monitorResolution = 90;

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
    DevicePtr p, k;
    extern Bool mieqInit();

    k = AddInputDevice(sunKbdProc, TRUE);
    p = AddInputDevice(sunMouseProc, TRUE);
    if (!p || !k)
	FatalError("failed to create input devices in InitInput");

    RegisterPointerDevice(p);
    RegisterKeyboardDevice(k);
    miRegisterPointerDevice(screenInfo.screens[0], p);
    (void) mieqInit (k, p);

#define SET_FLOW(fd) fcntl(fd, F_SETFL, FASYNC)
    (void) OsSignal(SIGIO, SigIOHandler);
#define WANT_SIGNALS(fd) fcntl(fd, F_SETOWN, getpid())
    if (sunKbdPriv.fd >= 0) {
	if (SET_FLOW(sunKbdPriv.fd) == -1 || WANT_SIGNALS(sunKbdPriv.fd) == -1) {	
	    (void) close (sunKbdPriv.fd);
	    sunKbdPriv.fd = -1;
	    FatalError("Async kbd I/O failed in InitInput");
	}
    }
    /* SIGIO doesn't work reliable for the mouse device,
     * esp. for the first server after a reboot. We enable it
     * anyway, gives smoother movements
     */
    if (sunPtrPriv.fd >= 0) {
	if (SET_FLOW(sunPtrPriv.fd) == -1 || WANT_SIGNALS(sunPtrPriv.fd) == -1) {
	    (void) close (sunPtrPriv.fd);
	    sunPtrPriv.fd = -1;
	    FatalError("ASYNC mouse I/O failed in InitInput");
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
