/*
 * CG14 framebuffer driver.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suncg14/cg14_driver.c,v 1.6 2001/10/01 13:44:10 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"

#include "fb.h"
#include "xf86cmap.h"
#include "cg14.h"

static const OptionInfoRec * CG14AvailableOptions(int chipid, int busid);
static void	CG14Identify(int flags);
static Bool	CG14Probe(DriverPtr drv, int flags);
static Bool	CG14PreInit(ScrnInfoPtr pScrn, int flags);
static Bool	CG14ScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	CG14EnterVT(int scrnIndex, int flags);
static void	CG14LeaveVT(int scrnIndex, int flags);
static Bool	CG14CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	CG14SaveScreen(ScreenPtr pScreen, int mode);
static void	CG14InitCplane24(ScrnInfoPtr pScrn);
static void	CG14ExitCplane24(ScrnInfoPtr pScrn);

/* Required if the driver supports mode switching */
static Bool	CG14SwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
/* Required if the driver supports moving the viewport */
static void	CG14AdjustFrame(int scrnIndex, int x, int y, int flags);

/* Optional functions */
static void	CG14FreeScreen(int scrnIndex, int flags);
static int	CG14ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
			     int flags);

void CG14Sync(ScrnInfoPtr pScrn);

#define VERSION 4000
#define CG14_NAME "SUNCG14"
#define CG14_DRIVER_NAME "suncg14"
#define CG14_MAJOR_VERSION 1
#define CG14_MINOR_VERSION 0
#define CG14_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec SUNCG14 = {
    VERSION,
    CG14_DRIVER_NAME,
    CG14Identify,
    CG14Probe,
    CG14AvailableOptions,
    NULL,
    0
};

static const OptionInfoRec CG14Options[] = {
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(cg14Setup);

static XF86ModuleVersionInfo suncg14VersRec =
{
	"suncg14",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	CG14_MAJOR_VERSION, CG14_MINOR_VERSION, CG14_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData suncg14ModuleData = { &suncg14VersRec, cg14Setup, NULL };

pointer
cg14Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&SUNCG14, module, 0);

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
CG14GetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an Cg14Rec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(Cg14Rec), 1);
    return TRUE;
}

static void
CG14FreeRec(ScrnInfoPtr pScrn)
{
    Cg14Ptr pCg14;

    if (pScrn->driverPrivate == NULL)
	return;

    pCg14 = GET_CG14_FROM_SCRN(pScrn);

    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    return;
}

static const OptionInfoRec *
CG14AvailableOptions(int chipid, int busid)
{
    return CG14Options;
}

/* Mandatory */
static void
CG14Identify(int flags)
{
    xf86Msg(X_INFO, "%s: driver for CG14\n", CG14_NAME);
}


/* Mandatory */
static Bool
CG14Probe(DriverPtr drv, int flags)
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

    if ((numDevSections = xf86MatchDevice(CG14_DRIVER_NAME,
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

    numUsed = xf86MatchSbusInstances(CG14_NAME, SBUS_DEVICE_CG14,
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
	    pScrn->driverName	 = CG14_DRIVER_NAME;
	    pScrn->name		 = CG14_NAME;
	    pScrn->Probe	 = CG14Probe;
	    pScrn->PreInit	 = CG14PreInit;
	    pScrn->ScreenInit	 = CG14ScreenInit;
  	    pScrn->SwitchMode	 = CG14SwitchMode;
  	    pScrn->AdjustFrame	 = CG14AdjustFrame;
	    pScrn->EnterVT	 = CG14EnterVT;
	    pScrn->LeaveVT	 = CG14LeaveVT;
	    pScrn->FreeScreen	 = CG14FreeScreen;
	    pScrn->ValidMode	 = CG14ValidMode;
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
CG14PreInit(ScrnInfoPtr pScrn, int flags)
{
    Cg14Ptr pCg14;
    sbusDevicePtr psdp = NULL;
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

    /* Allocate the Cg14Rec driverPrivate */
    if (!CG14GetRec(pScrn)) {
	return FALSE;
    }
    pCg14 = GET_CG14_FROM_SCRN(pScrn);
    
    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i < pScrn->numEntities; i++) {
	EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[i]);

	/* CG14 is purely AFX, but we handle it like SBUS */
	if (pEnt->location.type == BUS_SBUS) {
	    psdp = xf86GetSbusInfoForEntity(pEnt->index);
	    pCg14->psdp = psdp;
	} else
	    return FALSE;
    }
    if (psdp == NULL)
	return FALSE;

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
    if (!(pCg14->Options = xalloc(sizeof(CG14Options))))
	return FALSE;
    memcpy(pCg14->Options, CG14Options, sizeof(CG14Options));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pCg14->Options);

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
                                                                           
    if (!xf86SetDefaultVisual(pScrn, -1))
	return FALSE;
    else if (pScrn->depth > 8) {
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

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	CG14FreeRec(pScrn);
	return FALSE;
    }

    /*********************
    set up clock and mode stuff
    *********************/
    
    pScrn->progClock = TRUE;

    if(pScrn->display->virtualX || pScrn->display->virtualY) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "CG14 does not support a virtual desktop\n");
	pScrn->display->virtualX = 0;
	pScrn->display->virtualY = 0;
    }

    xf86SbusUseBuiltinMode(pScrn, pCg14->psdp);
    pScrn->currentMode = pScrn->modes;
    pScrn->displayWidth = pScrn->virtualX;

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    return TRUE;
}

/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
CG14ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    Cg14Ptr pCg14;
    VisualPtr visual;
    int ret;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    pCg14 = GET_CG14_FROM_SCRN(pScrn);

    /* Map the CG14 memory */
    pCg14->fb = xf86MapSbusMem (pCg14->psdp, CG14_BGR_VOFF, 4 *
				(pCg14->psdp->width * pCg14->psdp->height));
    pCg14->x32 = xf86MapSbusMem (pCg14->psdp, CG14_X32_VOFF,
				 (pCg14->psdp->width * pCg14->psdp->height));
    pCg14->xlut = xf86MapSbusMem (pCg14->psdp, CG14_XLUT_VOFF, 4096);

    if (! pCg14->fb || !pCg14->x32 || !pCg14->xlut)
	return FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    CG14SaveScreen(pScreen, SCREEN_SAVER_ON);

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

    if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
			  pScrn->rgbBits, pScrn->defaultVisual))
	return FALSE;

    miSetPixmapDepths ();

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    CG14InitCplane24(pScrn);
    ret = fbScreenInit(pScreen, pCg14->fb, pScrn->virtualX,
		       pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
		       pScrn->virtualX, pScrn->bitsPerPixel);

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

#ifdef RENDER
    /* must be after RGB ordering fixed */
    fbPictureInit (pScreen, 0, 0);
#endif

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    pCg14->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = CG14CloseScreen;
    pScreen->SaveScreen = CG14SaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* unblank the screen */
    CG14SaveScreen(pScreen, SCREEN_SAVER_OFF);

    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
CG14SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return TRUE;
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
static void 
CG14AdjustFrame(int scrnIndex, int x, int y, int flags)
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
CG14EnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    CG14InitCplane24 (pScrn);
    return TRUE;
}


/*
 * This is called when VT switching away from the X server.
 */

/* Mandatory */
static void
CG14LeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    CG14ExitCplane24 (pScrn);
    return;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
CG14CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    Cg14Ptr pCg14 = GET_CG14_FROM_SCRN(pScrn);

    pScrn->vtSema = FALSE;
    xf86UnmapSbusMem(pCg14->psdp, pCg14->fb,
		     (pCg14->psdp->width * pCg14->psdp->height * 4));
    xf86UnmapSbusMem(pCg14->psdp, pCg14->x32,
		     (pCg14->psdp->width * pCg14->psdp->height));
    xf86UnmapSbusMem(pCg14->psdp, pCg14->xlut, 4096);
    
    pScreen->CloseScreen = pCg14->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
    return FALSE;
}


/* Free up any per-generation data structures */

/* Optional */
static void
CG14FreeScreen(int scrnIndex, int flags)
{
    CG14FreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
CG14ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    if (mode->Flags & V_INTERLACE)
	return(MODE_BAD);

    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
CG14SaveScreen(ScreenPtr pScreen, int mode)
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
CG14Sync(ScrnInfoPtr pScrn)
{
    return;
}

/*
 * This initializes the card for 24 bit mode.
 */
static void
CG14InitCplane24(ScrnInfoPtr pScrn)
{
  Cg14Ptr pCg14 = GET_CG14_FROM_SCRN(pScrn);
  int size, bpp;
              
  size = pScrn->virtualX * pScrn->virtualY;
  bpp = 32;
  ioctl (pCg14->psdp->fd, CG14_SET_PIXELMODE, &bpp);
  memset (pCg14->fb, 0, size * 4);
  memset (pCg14->x32, 0, size);
  memset (pCg14->xlut, 0, 0x200);
}                                                  

/*
 * This initializes the card for 8 bit mode.
 */
static void
CG14ExitCplane24(ScrnInfoPtr pScrn)
{
  Cg14Ptr pCg14 = GET_CG14_FROM_SCRN(pScrn);
  int bpp = 8;
              
  ioctl (pCg14->psdp->fd, CG14_SET_PIXELMODE, &bpp);
}                                                  
