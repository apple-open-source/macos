/*
 * BW2 framebuffer driver.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunbw2/bw2_driver.c,v 1.3 2001/05/04 19:05:45 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"

#include "xf1bpp.h"
#include "xf86cmap.h"
#include "bw2.h"

static const OptionInfoRec * BW2AvailableOptions(int chipid, int busid);
static void	BW2Identify(int flags);
static Bool	BW2Probe(DriverPtr drv, int flags);
static Bool	BW2PreInit(ScrnInfoPtr pScrn, int flags);
static Bool	BW2ScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	BW2EnterVT(int scrnIndex, int flags);
static void	BW2LeaveVT(int scrnIndex, int flags);
static Bool	BW2CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	BW2SaveScreen(ScreenPtr pScreen, int mode);

/* Required if the driver supports mode switching */
static Bool	BW2SwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
/* Required if the driver supports moving the viewport */
static void	BW2AdjustFrame(int scrnIndex, int x, int y, int flags);

/* Optional functions */
static void	BW2FreeScreen(int scrnIndex, int flags);
static int	BW2ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
			     int flags);

void BW2Sync(ScrnInfoPtr pScrn);

#define VERSION 4000
#define BW2_NAME "SUNBW2"
#define BW2_DRIVER_NAME "sunbw2"
#define BW2_MAJOR_VERSION 1
#define BW2_MINOR_VERSION 0
#define BW2_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec SUNBW2 = {
    VERSION,
    BW2_DRIVER_NAME,
    BW2Identify,
    BW2Probe,
    BW2AvailableOptions,
    NULL,
    0
};

static const OptionInfoRec BW2Options[] = {
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(bw2Setup);

static XF86ModuleVersionInfo sunbw2VersRec =
{
	"sunbw2",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	BW2_MAJOR_VERSION, BW2_MINOR_VERSION, BW2_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData sunbw2ModuleData = { &sunbw2VersRec, bw2Setup, NULL };

pointer
bw2Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&SUNBW2, module, 0);

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
BW2GetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an Bw2Rec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(Bw2Rec), 1);
    return TRUE;
}

static void
BW2FreeRec(ScrnInfoPtr pScrn)
{
    Bw2Ptr pBw2;

    if (pScrn->driverPrivate == NULL)
	return;

    pBw2 = GET_BW2_FROM_SCRN(pScrn);

    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    return;
}

static const OptionInfoRec *
BW2AvailableOptions(int chipid, int busid)
{
    return BW2Options;
}

/* Mandatory */
static void
BW2Identify(int flags)
{
    xf86Msg(X_INFO, "%s: driver for BWtwo\n", BW2_NAME);
}


/* Mandatory */
static Bool
BW2Probe(DriverPtr drv, int flags)
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

    if ((numDevSections = xf86MatchDevice(BW2_DRIVER_NAME,
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

    numUsed = xf86MatchSbusInstances(BW2_NAME, SBUS_DEVICE_BW2,
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
	    pScrn->driverName	 = BW2_DRIVER_NAME;
	    pScrn->name		 = BW2_NAME;
	    pScrn->Probe	 = BW2Probe;
	    pScrn->PreInit	 = BW2PreInit;
	    pScrn->ScreenInit	 = BW2ScreenInit;
  	    pScrn->SwitchMode	 = BW2SwitchMode;
  	    pScrn->AdjustFrame	 = BW2AdjustFrame;
	    pScrn->EnterVT	 = BW2EnterVT;
	    pScrn->LeaveVT	 = BW2LeaveVT;
	    pScrn->FreeScreen	 = BW2FreeScreen;
	    pScrn->ValidMode	 = BW2ValidMode;
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
BW2PreInit(ScrnInfoPtr pScrn, int flags)
{
    Bw2Ptr pBw2;
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

    /* Allocate the Bw2Rec driverPrivate */
    if (!BW2GetRec(pScrn)) {
	return FALSE;
    }
    pBw2 = GET_BW2_FROM_SCRN(pScrn);
    
    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i < pScrn->numEntities; i++) {
	EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[i]);

	/* BW2 is purely SBUS */
	if (pEnt->location.type == BUS_SBUS) {
	    psdp = xf86GetSbusInfoForEntity(pEnt->index);
	    pBw2->psdp = psdp;
	} else
	    return FALSE;
    }

    /*********************
    deal with depth
    *********************/
    
    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, NoDepth24Support)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 1:
	    /* OK */
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1))
	return FALSE;

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

    if (xf86LoadSubModule(pScrn, "xf1bpp") == NULL) {
	BW2FreeRec(pScrn);
	return FALSE;
    }

    /*********************
    set up clock and mode stuff
    *********************/
    
    pScrn->progClock = TRUE;

    if(pScrn->display->virtualX || pScrn->display->virtualY) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "BW2 does not support a virtual desktop\n");
	pScrn->display->virtualX = 0;
	pScrn->display->virtualY = 0;
    }

    xf86SbusUseBuiltinMode(pScrn, pBw2->psdp);
    pScrn->currentMode = pScrn->modes;
    pScrn->displayWidth = pScrn->virtualX;

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    return TRUE;
}

/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
BW2ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    Bw2Ptr pBw2;
    int ret;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    pBw2 = GET_BW2_FROM_SCRN(pScrn);

    /* Map the BW2 memory */
    pBw2->fb =
	xf86MapSbusMem (pBw2->psdp, BW2_RAM_VOFF,
			(pBw2->psdp->width * pBw2->psdp->height / 8));

    if (! pBw2->fb)
	return FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    BW2SaveScreen(pScreen, SCREEN_SAVER_ON);

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

    /* Set the bits per RGB for 8bpp mode */
    pScrn->rgbBits = 8;

    /* Setup the visuals we support. */

    if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
			  pScrn->rgbBits, pScrn->defaultVisual))
	return FALSE;

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    ret = xf1bppScreenInit(pScreen, pBw2->fb, pScrn->virtualX,
			   pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
			   pScrn->virtualX);
    if (!ret)
	return FALSE;

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    xf86SetBlackWhitePixels(pScreen);

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    pBw2->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = BW2CloseScreen;
    pScreen->SaveScreen = BW2SaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* unblank the screen */
    BW2SaveScreen(pScreen, SCREEN_SAVER_OFF);

    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
BW2SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return TRUE;
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
static void 
BW2AdjustFrame(int scrnIndex, int x, int y, int flags)
{
    /* we don't support virtual desktops */
    return;
}

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 */

/* Mandatory */
static Bool
BW2EnterVT(int scrnIndex, int flags)
{
    return TRUE;
}


/*
 * This is called when VT switching away from the X server.
 */

/* Mandatory */
static void
BW2LeaveVT(int scrnIndex, int flags)
{
    return;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
BW2CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    Bw2Ptr pBw2 = GET_BW2_FROM_SCRN(pScrn);

    pScrn->vtSema = FALSE;
    xf86UnmapSbusMem(pBw2->psdp, pBw2->fb,
		     (pBw2->psdp->width * pBw2->psdp->height / 8));
    
    pScreen->CloseScreen = pBw2->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
    return FALSE;
}


/* Free up any per-generation data structures */

/* Optional */
static void
BW2FreeScreen(int scrnIndex, int flags)
{
    BW2FreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
BW2ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    if (mode->Flags & V_INTERLACE)
	return(MODE_BAD);

    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
BW2SaveScreen(ScreenPtr pScreen, int mode)
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
BW2Sync(ScrnInfoPtr pScrn)
{
    return;
}
