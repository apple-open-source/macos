
/*
 * Leo (ZX) framebuffer driver.
 *
 * Copyright (C) 2000 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_driver.c,v 1.8 2001/10/01 13:44:10 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"

#define PSZ 32
#include "cfb.h"
#undef PSZ
#include "xf86cmap.h"
#include "leo.h"

static const OptionInfoRec * LeoAvailableOptions(int chipid, int busid);
static void	LeoIdentify(int flags);
static Bool	LeoProbe(DriverPtr drv, int flags);
static Bool	LeoPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	LeoScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	LeoEnterVT(int scrnIndex, int flags);
static void	LeoLeaveVT(int scrnIndex, int flags);
static Bool	LeoCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	LeoSaveScreen(ScreenPtr pScreen, int mode);

/* Required if the driver supports mode switching */
static Bool	LeoSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
/* Required if the driver supports moving the viewport */
static void	LeoAdjustFrame(int scrnIndex, int x, int y, int flags);

/* Optional functions */
static void	LeoFreeScreen(int scrnIndex, int flags);
static int	LeoValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
			     int flags);

void LeoSync(ScrnInfoPtr pScrn);

#define VERSION 4000
#define LEO_NAME "SUNLEO"
#define LEO_DRIVER_NAME "sunleo"
#define LEO_MAJOR_VERSION 1
#define LEO_MINOR_VERSION 0
#define LEO_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec SUNLEO = {
    VERSION,
    LEO_DRIVER_NAME,
    LeoIdentify,
    LeoProbe,
    LeoAvailableOptions,
    NULL,
    0
};

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL
} LeoOpts;

static const OptionInfoRec LeoOptions[] = {
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_HW_CURSOR,		"HWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(leoSetup);

static XF86ModuleVersionInfo sunleoVersRec =
{
	"sunleo",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	LEO_MAJOR_VERSION, LEO_MINOR_VERSION, LEO_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData sunleoModuleData = { &sunleoVersRec, leoSetup, NULL };

pointer
leoSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&SUNLEO, module, 0);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer)TRUE;
    } else {
	if (errmaj) *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

#endif /* XFree86LOADER */

static Bool
LeoGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an LeoRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(LeoRec), 1);
    return TRUE;
}

static void
LeoFreeRec(ScrnInfoPtr pScrn)
{
    LeoPtr pLeo;

    if (pScrn->driverPrivate == NULL)
	return;

    pLeo = GET_LEO_FROM_SCRN(pScrn);

    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    return;
}

static const OptionInfoRec *
LeoAvailableOptions(int chipid, int busid)
{
    return LeoOptions;
}

/* Mandatory */
static void
LeoIdentify(int flags)
{
    xf86Msg(X_INFO, "%s: driver for Leo (ZX)\n", LEO_NAME);
}


/* Mandatory */
static Bool
LeoProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;
    EntityInfoPtr pEnt;

    /*
     * The aim here is to find all cards that this driver can handle,
     * and for the ones not already claimed by another driver, claim the
     * slot, and allocate a ScrnInfoRec.
     *
     * This should be a minimal probe, and it should under no circumstances
     * change the state of the hardware.  Because a device is found, don't
     * assume that it will be used.  Don't do any initialisations other than
     * the required ScrnInfoRec initialisations.  Don't allocate any new
     * data structures.
     */

    /*
     * Next we check, if there has been a chipset override in the config file.
     * For this we must find out if there is an active device section which
     * is relevant, i.e., which has no driver specified or has THIS driver
     * specified.
     */

    if ((numDevSections = xf86MatchDevice(LEO_DRIVER_NAME,
					  &devSections)) <= 0) {
	/*
	 * There's no matching device section in the config file, so quit
	 * now.
	 */
	return FALSE;
    }

    /*
     * We need to probe the hardware first.  We then need to see how this
     * fits in with what is given in the config file, and allow the config
     * file info to override any contradictions.
     */

    numUsed = xf86MatchSbusInstances(LEO_NAME, SBUS_DEVICE_LEO,
		   devSections, numDevSections,
		   drv, &usedChips);
				    
    xfree(devSections);
    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
	pEnt = xf86GetEntityInfo(usedChips[i]);

	/*
	 * Check that nothing else has claimed the slots.
	 */
	if(pEnt->active) {
	    ScrnInfoPtr pScrn;
	    
	    /* Allocate a ScrnInfoRec and claim the slot */
	    pScrn = xf86AllocateScreen(drv, 0);

	    /* Fill in what we can of the ScrnInfoRec */
	    pScrn->driverVersion = VERSION;
	    pScrn->driverName	 = LEO_DRIVER_NAME;
	    pScrn->name		 = LEO_NAME;
	    pScrn->Probe	 = LeoProbe;
	    pScrn->PreInit	 = LeoPreInit;
	    pScrn->ScreenInit	 = LeoScreenInit;
  	    pScrn->SwitchMode	 = LeoSwitchMode;
  	    pScrn->AdjustFrame	 = LeoAdjustFrame;
	    pScrn->EnterVT	 = LeoEnterVT;
	    pScrn->LeaveVT	 = LeoLeaveVT;
	    pScrn->FreeScreen	 = LeoFreeScreen;
	    pScrn->ValidMode	 = LeoValidMode;
	    xf86AddEntityToScreen(pScrn, pEnt->index);
	    foundScreen = TRUE;
	}
	xfree(pEnt);
    }
    xfree(usedChips);
    return foundScreen;
}

/* Mandatory */
static Bool
LeoPreInit(ScrnInfoPtr pScrn, int flags)
{
    LeoPtr pLeo;
    sbusDevicePtr psdp;
    MessageType from;
    int i;

    if (flags & PROBE_DETECT) return FALSE;

    /*
     * Note: This function is only called once at server startup, and
     * not at the start of each server generation.  This means that
     * only things that are persistent across server generations can
     * be initialised here.  xf86Screens[] is (pScrn is a pointer to one
     * of these).  Privates allocated using xf86AllocateScrnInfoPrivateIndex()  
     * are too, and should be used for data that must persist across
     * server generations.
     *
     * Per-generation data should be allocated with
     * AllocateScreenPrivateIndex() from the ScreenInit() function.
     */

    /* Allocate the LeoRec driverPrivate */
    if (!LeoGetRec(pScrn)) {
	return FALSE;
    }
    pLeo = GET_LEO_FROM_SCRN(pScrn);
    
    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i < pScrn->numEntities; i++) {
	EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[i]);

	/* LEO is purely SBUS */
	if (pEnt->location.type == BUS_SBUS) {
	    psdp = xf86GetSbusInfoForEntity(pEnt->index);
	    pLeo->psdp = psdp;
	} else
	    return FALSE;
    }

    /*********************
    deal with depth
    *********************/
    
    if (!xf86SetDepthBpp(pScrn, 32, 0, 32, Support32bppFb)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 32:
	    /* OK */
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);
    /* Process the options */
    if (!(pLeo->Options = xalloc(sizeof(LeoOptions))))
	return FALSE;
    memcpy(pLeo->Options, LeoOptions, sizeof(LeoOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pLeo->Options);
    
    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	rgb weight = {10, 11, 11};
	rgb mask = {0xff, 0xff00, 0xff0000};

	if (!xf86SetWeight(pScrn, weight, mask)) {
	    return FALSE;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	return FALSE;
    } else {
	/* We don't currently support DirectColor */
	if (pScrn->defaultVisual != TrueColor) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported\n",
		       xf86GetVisualName(pScrn->defaultVisual));
	    return FALSE;
	}
    }

    /*
     * The new cmap code requires this to be initialised.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }

    /* Set the bits per RGB for 8bpp mode */
    from = X_DEFAULT;

    /* determine whether we use hardware or software cursor */
    
    pLeo->HWCursor = TRUE;
    if (xf86GetOptValBool(pLeo->Options, OPTION_HW_CURSOR, &pLeo->HWCursor))
	from = X_CONFIG;
    if (xf86ReturnOptValBool(pLeo->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pLeo->HWCursor = FALSE;
    }
    
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pLeo->HWCursor ? "HW" : "SW");

    if (xf86ReturnOptValBool(pLeo->Options, OPTION_NOACCEL, FALSE)) {
	pLeo->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }
        
    if (xf86LoadSubModule(pScrn, "cfb32") == NULL) {
	LeoFreeRec(pScrn);
	return FALSE;
    }

    if (pLeo->HWCursor && xf86LoadSubModule(pScrn, "ramdac") == NULL) {
	LeoFreeRec(pScrn);
	return FALSE;
    }

    /*********************
    set up clock and mode stuff
    *********************/
    
    pScrn->progClock = TRUE;

    if(pScrn->display->virtualX || pScrn->display->virtualY) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Leo does not support a virtual desktop\n");
	pScrn->display->virtualX = 0;
	pScrn->display->virtualY = 0;
    }

    xf86SbusUseBuiltinMode(pScrn, pLeo->psdp);
    pScrn->currentMode = pScrn->modes;
    pScrn->displayWidth = pScrn->virtualX;

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    return TRUE;
}

/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
LeoScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    LeoPtr pLeo;
    int ret;
    VisualPtr visual;
    extern Bool LeoAccelInit(ScreenPtr pScreen, LeoPtr pLeo);

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    pLeo = GET_LEO_FROM_SCRN(pScrn);

    /* Map the Leo memory */
    pLeo->fb =
	xf86MapSbusMem (pLeo->psdp, LEO_FB0_VOFF, 0x803000);

    if (! pLeo->fb)
	return FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    LeoSaveScreen(pScreen, SCREEN_SAVER_ON);

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     */

    /*
     * Reset visual list.
     */
    miClearVisualTypes();

    /* Setup the visuals we support. */

    if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
			  pScrn->defaultVisual))
	return FALSE;

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    ret = cfb32ScreenInit(pScreen, pLeo->fb, pScrn->virtualX,
			  pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
			  2048);
    if (!ret)
	return FALSE;

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    xf86SetBlackWhitePixels(pScreen);

    if (pScrn->bitsPerPixel > 8) {
        /* Fixup RGB ordering */
        visual = pScreen->visuals + pScreen->numVisuals;
        while (--visual >= pScreen->visuals) {
	    if ((visual->class | DynamicClass) == DirectColor) {
		visual->offsetRed = pScrn->offset.red;
		visual->offsetGreen = pScrn->offset.green;
		visual->offsetBlue = pScrn->offset.blue;
		visual->redMask = pScrn->mask.red;
		visual->greenMask = pScrn->mask.green;
		visual->blueMask = pScrn->mask.blue;
	    }
	}
    }

    if (!LeoAccelInit(pScreen, pLeo))
	return FALSE;

    if (!pLeo->NoAccel)
	xf86Msg(X_INFO, "%s: Using acceleration\n", pLeo->psdp->device);

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
       Must follow software cursor initialization*/
    if (pLeo->HWCursor) { 
	extern Bool LeoHWCursorInit(ScreenPtr pScreen);

	if(!LeoHWCursorInit(pScreen)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		       "Hardware cursor initialization failed\n");
	    return(FALSE);
	}
	xf86SbusHideOsHwCursor(pLeo->psdp);
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    pLeo->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = LeoCloseScreen;
    pScreen->SaveScreen = LeoSaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* unblank the screen */
    LeoSaveScreen(pScreen, SCREEN_SAVER_OFF);

    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
LeoSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return TRUE;
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
static void 
LeoAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    /* we don't support virtual desktops */
    return;
}

extern void LeoVtChange (ScreenPtr pScreen, int enter);

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 */

/* Mandatory */
static Bool
LeoEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);

    pLeo->vtSema = FALSE;
    LeoVtChange (pScrn->pScreen, TRUE);
    if (pLeo->HWCursor)
	xf86SbusHideOsHwCursor (pLeo->psdp);
    return TRUE;
}


/*
 * This is called when VT switching away from the X server.
 */

/* Mandatory */
static void
LeoLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);

    LeoVtChange (pScrn->pScreen, FALSE);
    pLeo->vtSema = TRUE;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
LeoCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);

    pScrn->vtSema = FALSE;
    xf86UnmapSbusMem(pLeo->psdp, pLeo->fb, 0x803000);

    if (pLeo->HWCursor)
	xf86SbusHideOsHwCursor (pLeo->psdp);

    pScreen->CloseScreen = pLeo->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
    return FALSE;
}


/* Free up any per-generation data structures */

/* Optional */
static void
LeoFreeScreen(int scrnIndex, int flags)
{
    LeoFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
LeoValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    if (mode->Flags & V_INTERLACE)
	return(MODE_BAD);

    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
LeoSaveScreen(ScreenPtr pScreen, int mode)
    /* this function should blank the screen when unblank is FALSE and
       unblank it when unblank is TRUE -- it doesn't actually seem to be
       used for much though */
{
    return TRUE;
}

/*
 * This is the implementation of the Sync() function.
 */
void
LeoSync(ScrnInfoPtr pScrn)
{
    return;
}
