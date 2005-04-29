/*
 * GX and Turbo GX framebuffer driver.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suncg6/cg6_driver.c,v 1.8 2003/10/30 17:37:12 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"

#include "fb.h"
#include "xf86cmap.h"
#include "cg6.h"

static const OptionInfoRec * CG6AvailableOptions(int chipid, int busid);
static void	CG6Identify(int flags);
static Bool	CG6Probe(DriverPtr drv, int flags);
static Bool	CG6PreInit(ScrnInfoPtr pScrn, int flags);
static Bool	CG6ScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	CG6EnterVT(int scrnIndex, int flags);
static void	CG6LeaveVT(int scrnIndex, int flags);
static Bool	CG6CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	CG6SaveScreen(ScreenPtr pScreen, int mode);

/* Required if the driver supports mode switching */
static Bool	CG6SwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
/* Required if the driver supports moving the viewport */
static void	CG6AdjustFrame(int scrnIndex, int x, int y, int flags);

/* Optional functions */
static void	CG6FreeScreen(int scrnIndex, int flags);
static ModeStatus CG6ValidMode(int scrnIndex, DisplayModePtr mode,
			       Bool verbose, int flags);

void CG6Sync(ScrnInfoPtr pScrn);

#define VERSION 4000
#define CG6_NAME "SUNCG6"
#define CG6_DRIVER_NAME "suncg6"
#define CG6_MAJOR_VERSION 1
#define CG6_MINOR_VERSION 0
#define CG6_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec SUNCG6 = {
    VERSION,
    CG6_DRIVER_NAME,
    CG6Identify,
    CG6Probe,
    CG6AvailableOptions,
    NULL,
    0
};

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL
} CG6Opts;

static const OptionInfoRec CG6Options[] = {
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_HW_CURSOR,		"HWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(cg6Setup);

static XF86ModuleVersionInfo suncg6VersRec =
{
	"suncg6",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	CG6_MAJOR_VERSION, CG6_MINOR_VERSION, CG6_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData suncg6ModuleData = { &suncg6VersRec, cg6Setup, NULL };

pointer
cg6Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&SUNCG6, module, 0);

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
CG6GetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an Cg6Rec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(Cg6Rec), 1);
    return TRUE;
}

static void
CG6FreeRec(ScrnInfoPtr pScrn)
{
    Cg6Ptr pCg6;

    if (pScrn->driverPrivate == NULL)
	return;

    pCg6 = GET_CG6_FROM_SCRN(pScrn);

    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    return;
}

static const OptionInfoRec *
CG6AvailableOptions(int chipid, int busid)
{
    return CG6Options;
}

/* Mandatory */
static void
CG6Identify(int flags)
{
    xf86Msg(X_INFO, "%s: driver for CGsix (GX and Turbo GX)\n", CG6_NAME);
}


/* Mandatory */
static Bool
CG6Probe(DriverPtr drv, int flags)
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

    if ((numDevSections = xf86MatchDevice(CG6_DRIVER_NAME,
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

    numUsed = xf86MatchSbusInstances(CG6_NAME, SBUS_DEVICE_CG6,
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
	    pScrn->driverName	 = CG6_DRIVER_NAME;
	    pScrn->name		 = CG6_NAME;
	    pScrn->Probe	 = CG6Probe;
	    pScrn->PreInit	 = CG6PreInit;
	    pScrn->ScreenInit	 = CG6ScreenInit;
  	    pScrn->SwitchMode	 = CG6SwitchMode;
  	    pScrn->AdjustFrame	 = CG6AdjustFrame;
	    pScrn->EnterVT	 = CG6EnterVT;
	    pScrn->LeaveVT	 = CG6LeaveVT;
	    pScrn->FreeScreen	 = CG6FreeScreen;
	    pScrn->ValidMode	 = CG6ValidMode;
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
CG6PreInit(ScrnInfoPtr pScrn, int flags)
{
    Cg6Ptr pCg6;
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

    /* Allocate the Cg6Rec driverPrivate */
    if (!CG6GetRec(pScrn)) {
	return FALSE;
    }
    pCg6 = GET_CG6_FROM_SCRN(pScrn);
    
    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i < pScrn->numEntities; i++) {
	EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[i]);

	/* CG6 is purely SBUS */
	if (pEnt->location.type == BUS_SBUS) {
	    psdp = xf86GetSbusInfoForEntity(pEnt->index);
	    pCg6->psdp = psdp;
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
	case 8:
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
    if (!(pCg6->Options = xalloc(sizeof(CG6Options))))
	return FALSE;
    memcpy(pCg6->Options, CG6Options, sizeof(CG6Options));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pCg6->Options);
    
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

    /* determine whether we use hardware or software cursor */
    
    pCg6->HWCursor = TRUE;
    if (xf86GetOptValBool(pCg6->Options, OPTION_HW_CURSOR, &pCg6->HWCursor))
	from = X_CONFIG;
    if (xf86ReturnOptValBool(pCg6->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pCg6->HWCursor = FALSE;
    }
    
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pCg6->HWCursor ? "HW" : "SW");

    if (xf86ReturnOptValBool(pCg6->Options, OPTION_NOACCEL, FALSE)) {
	pCg6->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }
        
    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	CG6FreeRec(pScrn);
	return FALSE;
    }

    if (pCg6->HWCursor && xf86LoadSubModule(pScrn, "ramdac") == NULL) {
	CG6FreeRec(pScrn);
	return FALSE;
    }

    /*********************
    set up clock and mode stuff
    *********************/
    
    pScrn->progClock = TRUE;

    if(pScrn->display->virtualX || pScrn->display->virtualY) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "CG6 does not support a virtual desktop\n");
	pScrn->display->virtualX = 0;
	pScrn->display->virtualY = 0;
    }

    xf86SbusUseBuiltinMode(pScrn, pCg6->psdp);
    pScrn->currentMode = pScrn->modes;
    pScrn->displayWidth = pScrn->virtualX;

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    return TRUE;
}

/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
CG6ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    Cg6Ptr pCg6;
    int ret;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    pCg6 = GET_CG6_FROM_SCRN(pScrn);

    /* Map the CG6 memory */
    pCg6->fbc =
	xf86MapSbusMem (pCg6->psdp, CG6_FBC_VOFF,
			CG6_RAM_VOFF - CG6_FBC_VOFF +
			(pCg6->psdp->width * pCg6->psdp->height));

    if (! pCg6->fbc)
	return FALSE;

    pCg6->fb = (unsigned char *)pCg6->fbc + CG6_RAM_VOFF - CG6_FBC_VOFF;
    pCg6->thc = (Cg6ThcPtr)((char *)pCg6->fbc + CG6_THC_VOFF - CG6_FBC_VOFF);

    /* Darken the screen for aesthetic reasons and set the viewport */
    CG6SaveScreen(pScreen, SCREEN_SAVER_ON);

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

    miSetPixmapDepths ();
	
    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    ret = fbScreenInit(pScreen, pCg6->fb, pScrn->virtualX,
		       pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
		       pScrn->virtualX, 8);
    if (!ret)
	return FALSE;

#ifdef RENDER
    fbPictureInit (pScreen, 0, 0);
#endif

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    xf86SetBlackWhitePixels(pScreen);

#if 0
    if (!pCg6->NoAccel) {
	extern Bool CG6AccelInit(ScreenPtr pScreen, Cg6Ptr pCg6);

	if (!CG6AccelInit(pScreen, pCg6))
	    return FALSE;
	xf86Msg(X_INFO, "%s: Using acceleration\n", pCg6->psdp->device);
    }
#endif

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
       Must follow software cursor initialization*/
    if (pCg6->HWCursor) { 
	extern Bool CG6HWCursorInit(ScreenPtr pScreen);

	if(!CG6HWCursorInit(pScreen)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		       "Hardware cursor initialization failed\n");
	    return(FALSE);
	}
	xf86SbusHideOsHwCursor(pCg6->psdp);
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    if(!xf86SbusHandleColormaps(pScreen, pCg6->psdp))
	return FALSE;

    pCg6->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = CG6CloseScreen;
    pScreen->SaveScreen = CG6SaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* unblank the screen */
    CG6SaveScreen(pScreen, SCREEN_SAVER_OFF);

    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
CG6SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return TRUE;
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
static void 
CG6AdjustFrame(int scrnIndex, int x, int y, int flags)
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
CG6EnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);

    if (pCg6->HWCursor) {
	xf86SbusHideOsHwCursor (pCg6->psdp);
	pCg6->CursorFg = 0;
	pCg6->CursorBg = 0;
    }
    return TRUE;
}


/*
 * This is called when VT switching away from the X server.
 */

/* Mandatory */
static void
CG6LeaveVT(int scrnIndex, int flags)
{
    return;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
CG6CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);

    pScrn->vtSema = FALSE;

    xf86UnmapSbusMem(pCg6->psdp, pCg6->fbc,
		     CG6_RAM_VOFF - CG6_FBC_VOFF +
		     (pCg6->psdp->width * pCg6->psdp->height));
    
    if (pCg6->HWCursor)
    	xf86SbusHideOsHwCursor(pCg6->psdp);

    pScreen->CloseScreen = pCg6->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
    return FALSE;
}


/* Free up any per-generation data structures */

/* Optional */
static void
CG6FreeScreen(int scrnIndex, int flags)
{
    CG6FreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static ModeStatus
CG6ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    if (mode->Flags & V_INTERLACE)
	return(MODE_BAD);

    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
CG6SaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);
    unsigned int tmp = pCg6->thc->thc_misc;

    switch(mode)
    {
    case SCREEN_SAVER_ON:
    case SCREEN_SAVER_CYCLE:
       tmp &= ~CG6_THC_MISC_SYNC_ENAB;
       break;
    case SCREEN_SAVER_OFF:
    case SCREEN_SAVER_FORCER:
       tmp |= CG6_THC_MISC_SYNC_ENAB;
       break;
    default:
       return FALSE;
    }

    pCg6->thc->thc_misc = tmp;
    return TRUE;
}

/*
 * This is the implementation of the Sync() function.
 */
void
CG6Sync(ScrnInfoPtr pScrn)
{
    return;
}
