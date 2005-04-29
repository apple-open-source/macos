/*
 * Copyright 1996-1997  David J. McKay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID J. MCKAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Hacked together from mga driver and 3.3.4 NVIDIA driver by Jarno Paananen
   <jpaana@s2.org> */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_driver.c,v 1.6 2004/01/10 22:31:53 mvojkovi Exp $ */

#include "riva_include.h"

#include "xf86int10.h"

/*
 * Forward definitions for the functions that make up the driver.
 */
/* Mandatory functions */
static Bool    RivaPreInit(ScrnInfoPtr pScrn, int flags);
static Bool    RivaScreenInit(int Index, ScreenPtr pScreen, int argc,
                            char **argv);
static Bool    RivaEnterVT(int scrnIndex, int flags);
static Bool    RivaEnterVTFBDev(int scrnIndex, int flags);
static void    RivaLeaveVT(int scrnIndex, int flags);
static Bool    RivaCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool    RivaSaveScreen(ScreenPtr pScreen, int mode);

/* Optional functions */
static void    RivaFreeScreen(int scrnIndex, int flags);
static ModeStatus RivaValidMode(int scrnIndex, DisplayModePtr mode,
				Bool verbose, int flags);

/* Internally used functions */

static Bool	RivaMapMem(ScrnInfoPtr pScrn);
static Bool	RivaMapMemFBDev(ScrnInfoPtr pScrn);
static Bool	RivaUnmapMem(ScrnInfoPtr pScrn);
static void	RivaSave(ScrnInfoPtr pScrn);
static void	RivaRestore(ScrnInfoPtr pScrn);
static Bool	RivaModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);


/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.
 */

static const char *vgahwSymbols[] = {
    "vgaHWUnmapMem",
    "vgaHWDPMSSet",
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWMapMem",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAFallbackOps",
    "XAAInit",
    "XAAPatternROP",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86DoEDID_DDC2",
    "xf86SetDDCproperties",
    NULL
};

#ifdef XFree86LOADER
static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeFree",
    "vbeDoEDID",
    NULL
};
#endif

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

static const char *fbdevHWSymbols[] = {
    "fbdevHWInit",
    "fbdevHWUseBuildinMode",

    "fbdevHWGetVidmem",

    /* colormap */
    "fbdevHWLoadPalette",

    /* ScrnInfo hooks */
    "fbdevHWAdjustFrame",
    "fbdevHWEnterVT",
    "fbdevHWLeaveVT",
    "fbdevHWModeInit",
    "fbdevHWSave",
    "fbdevHWSwitchMode",
    "fbdevHWValidMode",

    "fbdevHWMapMMIO",
    "fbdevHWMapVidmem",

    NULL
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};


#ifdef XFree86LOADER

static MODULESETUPPROTO(rivaSetup);

static XF86ModuleVersionInfo rivaVersRec =
{
    "riva",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    RIVA_MAJOR_VERSION, RIVA_MINOR_VERSION, RIVA_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,                     /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0,0,0,0}
};

XF86ModuleData riva128ModuleData = { &rivaVersRec, rivaSetup, NULL };
#endif


typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL,
    OPTION_SHOWCACHE,
    OPTION_SHADOW_FB,
    OPTION_FBDEV,
    OPTION_ROTATE
} RivaOpts;


static const OptionInfoRec RivaOptions[] = {
    { OPTION_SW_CURSOR,         "SWcursor",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_HW_CURSOR,         "HWcursor",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOACCEL,           "NoAccel",      OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_SHOWCACHE,         "ShowCache",    OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_SHADOW_FB,         "ShadowFB",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_FBDEV,             "UseFBDev",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_ROTATE,		"Rotate",	OPTV_ANYSTR,	{0}, FALSE },
    { -1,                       NULL,           OPTV_NONE,      {0}, FALSE }
};

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

/* 
 * ramdac info structure initialization
 */
static RivaRamdacRec DacInit = {
        FALSE, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL,
        0, NULL, NULL, NULL, NULL
}; 



static Bool
RivaGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an RivaRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
        return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(RivaRec), 1);
    /* Initialise it */

    RivaPTR(pScrn)->Dac = DacInit;
    return TRUE;
}

static void
RivaFreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
        return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}


#ifdef XFree86LOADER

static pointer
rivaSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    /* This module should be loaded only once, but check to be sure. */

    if (!setupDone) {
        setupDone = TRUE;

        LoaderRefSymLists(vgahwSymbols, xaaSymbols, fbSymbols,
                          ramdacSymbols, shadowSymbols,
                          i2cSymbols, ddcSymbols, vbeSymbols,
                          fbdevHWSymbols, int10Symbols, NULL);
    } 
    return (pointer)1;
}


#endif /* XFree86LOADER */

const OptionInfoRec *
RivaAvailableOptions(int chipid, int busid)
{
    return RivaOptions;
}


Bool
RivaGetScrnInfoRec(PciChipsets *chips, int chip)
{
    ScrnInfoPtr pScrn;

    pScrn = xf86ConfigPciEntity(NULL, 0, chip,
                                chips, NULL, NULL, NULL,
                                NULL, NULL);

    if(!pScrn) return FALSE;

    pScrn->driverVersion    = RIVA_VERSION;
    pScrn->driverName       = RIVA_DRIVER_NAME;
    pScrn->name             = RIVA_NAME;

    pScrn->Probe            = NULL;
    pScrn->PreInit          = RivaPreInit;
    pScrn->ScreenInit       = RivaScreenInit;
    pScrn->SwitchMode       = RivaSwitchMode;
    pScrn->AdjustFrame      = RivaAdjustFrame;
    pScrn->EnterVT          = RivaEnterVT;
    pScrn->LeaveVT          = RivaLeaveVT;
    pScrn->FreeScreen       = RivaFreeScreen;
    pScrn->ValidMode        = RivaValidMode;

    return TRUE;
}

/* Usually mandatory */
Bool
RivaSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return RivaModeInit(xf86Screens[scrnIndex], mode);
}

/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
void 
RivaAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    int startAddr;
    RivaPtr pRiva = RivaPTR(pScrn);
    RivaFBLayout *pLayout = &pRiva->CurrentLayout;

    if(pRiva->ShowCache && y && pScrn->vtSema) 
	y += pScrn->virtualY - 1;	

    startAddr = (((y*pLayout->displayWidth)+x)*(pLayout->bitsPerPixel/8));
    pRiva->riva.SetStartAddress(&pRiva->riva, startAddr);
}


/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */

/* Mandatory */
static Bool
RivaEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    if (!RivaModeInit(pScrn, pScrn->currentMode))
        return FALSE;
    RivaAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    return TRUE;
}

static Bool
RivaEnterVTFBDev(int scrnIndex, int flags)
{
    fbdevHWEnterVT(scrnIndex,flags);
    return TRUE;
}

/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 * We may wish to remap video/MMIO memory too.
 */

/* Mandatory */
static void
RivaLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    RivaPtr pRiva = RivaPTR(pScrn);

    RivaRestore(pScrn);
    pRiva->riva.LockUnlock(&pRiva->riva, 1);
}



/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
RivaCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    RivaPtr pRiva = RivaPTR(pScrn);

    if (pScrn->vtSema) {
        RivaRestore(pScrn);
        pRiva->riva.LockUnlock(&pRiva->riva, 1);
    }

    RivaUnmapMem(pScrn);
    vgaHWUnmapMem(pScrn);
    if (pRiva->AccelInfoRec)
        XAADestroyInfoRec(pRiva->AccelInfoRec);
    if (pRiva->CursorInfoRec)
        xf86DestroyCursorInfoRec(pRiva->CursorInfoRec);
    if (pRiva->ShadowPtr)
        xfree(pRiva->ShadowPtr);
    if (pRiva->DGAModes)
        xfree(pRiva->DGAModes);
    if ( pRiva->expandBuffer )
        xfree(pRiva->expandBuffer);

    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = pRiva->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/* Free up any persistent data structures */

/* Optional */
static void
RivaFreeScreen(int scrnIndex, int flags)
{
    /*
     * This only gets called when a screen is being deleted.  It does not
     * get called routinely at the end of a server generation.
     */
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    RivaFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static ModeStatus
RivaValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    return (MODE_OK);
}

static void
rivaProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}


Bool RivaI2CInit(ScrnInfoPtr pScrn)
{
    char *mod = "i2c";

    if (xf86LoadSubModule(pScrn, mod)) {
        xf86LoaderReqSymLists(i2cSymbols,NULL);

        mod = "ddc";
        if(xf86LoadSubModule(pScrn, mod)) {
            xf86LoaderReqSymLists(ddcSymbols, NULL);
            return RivaDACi2cInit(pScrn);
        } 
    }

    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
              "Couldn't load %s module.  DDC probing can't be done\n", mod);

    return FALSE;
}

/* Mandatory */
Bool
RivaPreInit(ScrnInfoPtr pScrn, int flags)
{
    RivaPtr pRiva;
    MessageType from;
    int i;
    ClockRangePtr clockRanges;
    const char *s;

    if (flags & PROBE_DETECT) {
        rivaProbeDDC( pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index );
	return TRUE;
    }

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

    /* Check the number of entities, and fail if it isn't one. */
    if (pScrn->numEntities != 1)
	return FALSE;

    /* Allocate the RivaRec driverPrivate */
    if (!RivaGetRec(pScrn)) {
	return FALSE;
    }
    pRiva = RivaPTR(pScrn);

    /* Get the entity, and make sure it is PCI. */
    pRiva->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pRiva->pEnt->location.type != BUS_PCI)
	return FALSE;
 
    /* Find the PCI info for this screen */
    pRiva->PciInfo = xf86GetPciInfoForEntity(pRiva->pEnt->index);
    pRiva->PciTag = pciTag(pRiva->PciInfo->bus, pRiva->PciInfo->device,
			  pRiva->PciInfo->func);

    pRiva->Primary = xf86IsPrimaryPci(pRiva->PciInfo);

    /* Initialize the card through int10 interface if needed */
    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86LoaderReqSymLists(int10Symbols, NULL);
#if !defined(__alpha__) 
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing int10\n");
        pRiva->pInt = xf86InitInt10(pRiva->pEnt->index);
#endif
    }
   
    xf86SetOperatingState(resVgaIo, pRiva->pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMem, pRiva->pEnt->index, ResDisableOpr);

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    pRiva->ChipRev = pRiva->PciInfo->chipRev;
    if((pRiva->PciInfo->vendor != 0x12D2) || (pRiva->PciInfo->chipType != 0x0018))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "This is not a RIVA 128\n");
        xf86FreeInt10(pRiva->pInt);
        return FALSE;
    }

    pScrn->chipset = "RIVA 128";

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     */

    if (!xf86SetDepthBpp(pScrn, 15, 0, 0, Support32bppFb)) {
	xf86FreeInt10(pRiva->pInt);
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
            case 8:
            case 15:
            case 24:
                break;
            default:
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Given depth (%d) is not supported by this driver\n",
                    pScrn->depth);
		xf86FreeInt10(pRiva->pInt);
                return FALSE;
	}
    }
    xf86PrintDepthBpp(pScrn);

    /* Get the depth24 pixmap format */
    if (pScrn->depth == 24 && pix24bpp == 0)
	pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	/* The defaults are OK for us */
	rgb zeros = {0, 0, 0};

	if (!xf86SetWeight(pScrn, zeros, zeros)) {
	    xf86FreeInt10(pRiva->pInt);
	    return FALSE;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	xf86FreeInt10(pRiva->pInt);
	return FALSE;
    } else {
	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && (pScrn->defaultVisual != TrueColor)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported at depth %d\n",
		       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	    xf86FreeInt10(pRiva->pInt);
	    return FALSE;
	}
    }

    /* The vgahw module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "vgahw")) {
	xf86FreeInt10(pRiva->pInt);
	return FALSE;
    }
    
    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn)) {
	xf86FreeInt10(pRiva->pInt);
	return FALSE;
    }
    
    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pRiva->Options = xalloc(sizeof(RivaOptions))))
	return FALSE;
    memcpy(pRiva->Options, RivaOptions, sizeof(RivaOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pRiva->Options);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
	pScrn->rgbBits = 8;

    from = X_DEFAULT;
    pRiva->HWCursor = TRUE;
    /*
     * The preferred method is to use the "hw cursor" option as a tri-state
     * option, with the default set above.
     */
    if (xf86GetOptValBool(pRiva->Options, OPTION_HW_CURSOR, &pRiva->HWCursor)) {
	from = X_CONFIG;
    }
    /* For compatibility, accept this too (as an override) */
    if (xf86ReturnOptValBool(pRiva->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pRiva->HWCursor = FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pRiva->HWCursor ? "HW" : "SW");
    if (xf86ReturnOptValBool(pRiva->Options, OPTION_NOACCEL, FALSE)) {
	pRiva->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }
    if (xf86ReturnOptValBool(pRiva->Options, OPTION_SHOWCACHE, FALSE)) {
	pRiva->ShowCache = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShowCache enabled\n");
    }
    if (xf86ReturnOptValBool(pRiva->Options, OPTION_SHADOW_FB, FALSE)) {
	pRiva->ShadowFB = TRUE;
	pRiva->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Using \"Shadow Framebuffer\" - acceleration disabled\n");
    }
    if (xf86ReturnOptValBool(pRiva->Options, OPTION_FBDEV, FALSE)) {
	pRiva->FBDev = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Using framebuffer device\n");
    }
    if (pRiva->FBDev) {
	/* check for linux framebuffer device */
	if (!xf86LoadSubModule(pScrn, "fbdevhw")) {
	    xf86FreeInt10(pRiva->pInt);
	    return FALSE;
	}
	
	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);
	if (!fbdevHWInit(pScrn, pRiva->PciInfo, NULL)) {
	    xf86FreeInt10(pRiva->pInt);
	    return FALSE;
	}
	pScrn->SwitchMode    = fbdevHWSwitchMode;
	pScrn->AdjustFrame   = fbdevHWAdjustFrame;
	pScrn->EnterVT       = RivaEnterVTFBDev;
	pScrn->LeaveVT       = fbdevHWLeaveVT;
	pScrn->ValidMode     = fbdevHWValidMode;
    }
    pRiva->Rotate = 0;
    if ((s = xf86GetOptValString(pRiva->Options, OPTION_ROTATE))) {
      if(!xf86NameCmp(s, "CW")) {
	pRiva->ShadowFB = TRUE;
	pRiva->NoAccel = TRUE;
	pRiva->HWCursor = FALSE;
	pRiva->Rotate = 1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Rotating screen clockwise - acceleration disabled\n");
      } else
      if(!xf86NameCmp(s, "CCW")) {
	pRiva->ShadowFB = TRUE;
	pRiva->NoAccel = TRUE;
	pRiva->HWCursor = FALSE;
	pRiva->Rotate = -1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Rotating screen counter clockwise - acceleration disabled\n");
      } else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"\"%s\" is not a valid value for Option \"Rotate\"\n", s);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		"Valid options are \"CW\" or \"CCW\"\n");
      }
    }

    if (pRiva->pEnt->device->MemBase != 0) {
	/* Require that the config file value matches one of the PCI values. */
	if (!xf86CheckPciMemBase(pRiva->PciInfo, pRiva->pEnt->device->MemBase)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"MemBase 0x%08lX doesn't match any PCI base register.\n",
		pRiva->pEnt->device->MemBase);
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
	pRiva->FbAddress = pRiva->pEnt->device->MemBase;
	from = X_CONFIG;
    } else {
	int i = 1;
	pRiva->FbBaseReg = i;
	if (pRiva->PciInfo->memBase[i] != 0) {
	    pRiva->FbAddress = pRiva->PciInfo->memBase[i] & 0xff800000;
	    from = X_PROBED;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No valid FB address in PCI config space\n");
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	       (unsigned long)pRiva->FbAddress);

    if (pRiva->pEnt->device->IOBase != 0) {
	/* Require that the config file value matches one of the PCI values. */
	if (!xf86CheckPciMemBase(pRiva->PciInfo, pRiva->pEnt->device->IOBase)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"IOBase 0x%08lX doesn't match any PCI base register.\n",
		pRiva->pEnt->device->IOBase);
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
	pRiva->IOAddress = pRiva->pEnt->device->IOBase;
	from = X_CONFIG;
    } else {
	int i = 0;
	if (pRiva->PciInfo->memBase[i] != 0) {
	    pRiva->IOAddress = pRiva->PciInfo->memBase[i] & 0xffffc000;
	    from = X_PROBED;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"No valid MMIO address in PCI config space\n");
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX\n",
	       (unsigned long)pRiva->IOAddress);
     
    if (xf86RegisterResources(pRiva->pEnt->index, NULL, ResExclusive)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"xf86RegisterResources() found resource conflicts\n");
	xf86FreeInt10(pRiva->pInt);
	RivaFreeRec(pScrn);
	return FALSE;
    }

    Riva3Setup(pScrn);

    /*
     * If the user has specified the amount of memory in the XF86Config
     * file, we respect that setting.
     */
    if (pRiva->pEnt->device->videoRam != 0) {
	pScrn->videoRam = pRiva->pEnt->device->videoRam;
	from = X_CONFIG;
    } else {
	if (pRiva->FBDev) {
	    pScrn->videoRam = fbdevHWGetVidmem(pScrn)/1024;
	} else {
            pScrn->videoRam = pRiva->riva.RamAmountKBytes;
	}
	from = X_PROBED;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d kBytes\n",
               pScrn->videoRam);
	
    pRiva->FbMapSize = pScrn->videoRam * 1024;

    /*
     * If the driver can do gamma correction, it should call xf86SetGamma()
     * here.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    xf86FreeInt10(pRiva->pInt);
	    return FALSE;
	}
    }

    pRiva->FbUsableSize = pRiva->FbMapSize - (32 * 1024);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */

    pRiva->MinClock = 12000;
    pRiva->MaxClock = pRiva->riva.MaxVClockFreqKHz;

    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pRiva->MinClock;
    clockRanges->maxClock = pRiva->MaxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = TRUE;


    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our RivaValidMode() already takes
     * care of this, we don't worry about setting them here.
     */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                          pScrn->display->modes, clockRanges,
                          NULL, 256, 2048,
                          32 * pScrn->bitsPerPixel, 128, 2048,
                          pScrn->display->virtualX,
                          pScrn->display->virtualY,
                          pRiva->FbUsableSize,
                          LOOKUP_BEST_REFRESH);

    if (i < 1 && pRiva->FBDev) {
	fbdevHWUseBuildinMode(pScrn);
	pScrn->displayWidth = pScrn->virtualX; /* FIXME: might be wrong */
	i = 1;
    }
    if (i == -1) {
	xf86FreeInt10(pRiva->pInt);
	RivaFreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	xf86FreeInt10(pRiva->pInt);
	RivaFreeRec(pScrn);
	return FALSE;
    }

    /*
     * Set the CRTC parameters for all of the modes based on the type
     * of mode, and the chipset's interlace requirements.
     *
     * Calling this is required if the mode->Crtc* values are used by the
     * driver and if the driver doesn't provide code to set them.  They
     * are not pre-initialised at all.
     */
    xf86SetCrtcForModes(pScrn, 0);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);


    /*
     * XXX This should be taken into account in some way in the mode valdation
     * section.
     */

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	xf86FreeInt10(pRiva->pInt);
	RivaFreeRec(pScrn);
	return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);
    
    /* Load XAA if needed */
    if (!pRiva->NoAccel) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    /* Load ramdac if needed */
    if (pRiva->HWCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    /* Load shadowfb if needed */
    if (pRiva->ShadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    xf86FreeInt10(pRiva->pInt);
	    RivaFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    pRiva->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pRiva->CurrentLayout.depth = pScrn->depth;
    pRiva->CurrentLayout.displayWidth = pScrn->displayWidth;
    pRiva->CurrentLayout.weight.red = pScrn->weight.red;
    pRiva->CurrentLayout.weight.green = pScrn->weight.green;
    pRiva->CurrentLayout.weight.blue = pScrn->weight.blue;
    pRiva->CurrentLayout.mode = pScrn->currentMode;

    xf86FreeInt10(pRiva->pInt);

    pRiva->pInt = NULL;
    return TRUE;
}


/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
RivaMapMem(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva;
        
    pRiva = RivaPTR(pScrn);

    /*
     * Map IO registers to virtual address space
     */ 
    pRiva->IOBase = xf86MapPciMem(pScrn->scrnIndex,
                                VIDMEM_MMIO | VIDMEM_READSIDEEFFECT,
                                pRiva->PciTag, pRiva->IOAddress, 0x1000000);
    if (pRiva->IOBase == NULL)
	return FALSE;

    pRiva->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				 pRiva->PciTag, pRiva->FbAddress,
				 pRiva->FbMapSize);
    if (pRiva->FbBase == NULL)
	return FALSE;

    pRiva->FbStart = pRiva->FbBase;

    return TRUE;
}

Bool
RivaMapMemFBDev(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva;

    pRiva = RivaPTR(pScrn);

    pRiva->FbBase = fbdevHWMapVidmem(pScrn);
    if (pRiva->FbBase == NULL)
        return FALSE;

    pRiva->IOBase = fbdevHWMapMMIO(pScrn);
    if (pRiva->IOBase == NULL)
        return FALSE;

    pRiva->FbStart = pRiva->FbBase;

    return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
RivaUnmapMem(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva;
    
    pRiva = RivaPTR(pScrn);

    /*
     * Unmap IO registers to virtual address space
     */ 
    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pRiva->IOBase, 0x1000000);
    pRiva->IOBase = NULL;

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pRiva->FbBase, pRiva->FbMapSize);
    pRiva->FbBase = NULL;
    pRiva->FbStart = NULL;

    return TRUE;
}


/*
 * Initialise a new mode. 
 */

static Bool
RivaModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg;
    RivaPtr pRiva = RivaPTR(pScrn);
    RivaRegPtr rivaReg;

    
    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, mode))
	return FALSE;
    pScrn->vtSema = TRUE;

    vgaReg = &hwp->ModeReg;
    rivaReg = &pRiva->ModeReg;

    if(!(*pRiva->ModeInit)(pScrn, mode))
        return FALSE;

    pRiva->riva.LockUnlock(&pRiva->riva, 0);

    /* Program the registers */
    vgaHWProtect(pScrn, TRUE);

    (*pRiva->Restore)(pScrn, vgaReg, rivaReg, FALSE);

    RivaResetGraphics(pScrn);

    vgaHWProtect(pScrn, FALSE);

    pRiva->CurrentLayout.mode = mode;

    return TRUE;
}

/*
 * Restore the initial (text) mode.
 */
static void 
RivaRestore(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg = &hwp->SavedReg;
    RivaPtr pRiva = RivaPTR(pScrn);
    RivaRegPtr rivaReg = &pRiva->SavedReg;


    pRiva->riva.LockUnlock(&pRiva->riva, 0);

    /* Only restore text mode fonts/text for the primary card */
    vgaHWProtect(pScrn, TRUE);
    (*pRiva->Restore)(pScrn, vgaReg, rivaReg, pRiva->Primary);
    vgaHWProtect(pScrn, FALSE);
}

static void
RivaDPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
  unsigned char crtc1A;
  vgaHWPtr hwp = VGAHWPTR(pScrn);

  if (!pScrn->vtSema) return;

  crtc1A = hwp->readCrtc(hwp, 0x1A) & ~0xC0;

  switch (PowerManagementMode) {
  case DPMSModeStandby:  /* HSync: Off, VSync: On */
    crtc1A |= 0x80;
    break;
  case DPMSModeSuspend:  /* HSync: On, VSync: Off */
    crtc1A |= 0x40;
    break;
  case DPMSModeOff:      /* HSync: Off, VSync: Off */
    crtc1A |= 0xC0;
    break;
  case DPMSModeOn:       /* HSync: On, VSync: On */
  default:
    break;
  }

  /* vgaHWDPMSSet will merely cut the dac output */
  vgaHWDPMSSet(pScrn, PowerManagementMode, flags);

  hwp->writeCrtc(hwp, 0x1A, crtc1A);
}


/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
RivaScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    RivaPtr pRiva;
    RivaRamdacPtr Rivadac;
    int ret;
    VisualPtr visual;
    unsigned char *FBStart;
    int width, height, displayWidth;
    BoxRec AvailFBArea;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];


    hwp = VGAHWPTR(pScrn);
    pRiva = RivaPTR(pScrn);
    Rivadac = &pRiva->Dac;

    /* Map the Riva memory and MMIO areas */
    if (pRiva->FBDev) {
	if (!RivaMapMemFBDev(pScrn))
	    return FALSE;
    } else {
	if (!RivaMapMem(pScrn))
	    return FALSE;
    }
    
    /* Map the VGA memory when the primary video */
    if (pRiva->Primary && !pRiva->FBDev) {
	hwp->MapSize = 0x10000;
	if (!vgaHWMapMem(pScrn))
	    return FALSE;
    }

    if (pRiva->FBDev) {
	fbdevHWSave(pScrn);
	if (!fbdevHWModeInit(pScrn, pScrn->currentMode))
	    return FALSE;
    } else {
	/* Save the current state */
	RivaSave(pScrn);
	/* Initialise the first mode */
	if (!RivaModeInit(pScrn, pScrn->currentMode))
	    return FALSE;
    }


    /* Darken the screen for aesthetic reasons and set the viewport */
    RivaSaveScreen(pScreen, SCREEN_SAVER_ON);
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);


    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     *
     * For most PC hardware at depths >= 8, the defaults that cfb uses
     * are not appropriate.  In this driver, we fixup the visuals after.
     */

    /*
     * Reset the visual list.
     */
    miClearVisualTypes();

    /* Setup the visuals we support. */

    if (pScrn->bitsPerPixel > 8) {
          if (!miSetVisualTypes(pScrn->depth, TrueColorMask, 8,
                                pScrn->defaultVisual))
              return FALSE;
    } else {
          if (!miSetVisualTypes(pScrn->depth, 
                                miGetDefaultVisualMask(pScrn->depth), 8,
                                pScrn->defaultVisual))
	  return FALSE;
     }
    if (!miSetPixmapDepths ()) return FALSE;


    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    width = pScrn->virtualX;
    height = pScrn->virtualY;
    displayWidth = pScrn->displayWidth;


    if(pRiva->Rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    }

    if(pRiva->ShadowFB) {
 	pRiva->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pRiva->ShadowPtr = xalloc(pRiva->ShadowPitch * height);
	displayWidth = pRiva->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pRiva->ShadowPtr;
    } else {
	pRiva->ShadowPtr = NULL;
	FBStart = pRiva->FbStart;
    }

    switch (pScrn->bitsPerPixel) {
        case 8:
        case 16:
        case 32:
            ret = fbScreenInit(pScreen, FBStart, width, height,
                               pScrn->xDpi, pScrn->yDpi,
                               displayWidth, pScrn->bitsPerPixel);
            break;
        default:
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "Internal error: invalid bpp (%d) in RivaScreenInit\n",
                       pScrn->bitsPerPixel);
            ret = FALSE;
            break;
    }
    if (!ret)
	return FALSE;


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

    fbPictureInit (pScreen, 0, 0);
    
    xf86SetBlackWhitePixels(pScreen);


    if(!pRiva->ShadowFB) /* hardware cursor needs to wrap this layer */
	RivaDGAInit(pScreen);

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = (min(pRiva->FbUsableSize, 32*1024*1024)) / 
                     (pScrn->displayWidth * pScrn->bitsPerPixel / 8);
    xf86InitFBManager(pScreen, &AvailFBArea);
    
    if (!pRiva->NoAccel)
	RivaAccelInit(pScreen);
    
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);


    /* Initialize software cursor.  
	Must precede creation of the default colormap */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());


    /* Initialize HW cursor layer. 
	Must follow software cursor initialization*/
    if (pRiva->HWCursor) { 
	if(!RivaCursorInit(pScreen))
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		"Hardware cursor initialization failed\n");
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    
    /* Initialize colormap layer.  
	Must follow initialization of the default colormap */
    if(!xf86HandleColormaps(pScreen, 256, 8,
	(pRiva->FBDev ? fbdevHWLoadPalette : Rivadac->LoadPalette), 
	NULL, CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR))
	return FALSE;

    
    if(pRiva->ShadowFB) {
	RefreshAreaFuncPtr refreshArea = RivaRefreshArea;

	if(pRiva->Rotate) {
   	   pRiva->PointerMoved = pScrn->PointerMoved;
	   pScrn->PointerMoved = RivaPointerMoved;

	   switch(pScrn->bitsPerPixel) {
               case 8:	refreshArea = RivaRefreshArea8;	break;
               case 16:	refreshArea = RivaRefreshArea16;	break;
               case 32:	refreshArea = RivaRefreshArea32;	break;
	   }
           xf86DisableRandR();
           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                      "Driver rotation enabled, RandR disabled\n");
	}

	ShadowFBInit(pScreen, refreshArea);
    }

    xf86DPMSInit(pScreen, RivaDPMSSet, 0);

    
    pScrn->memPhysBase = pRiva->FbAddress;
    pScrn->fbOffset = 0;

    pScreen->SaveScreen = RivaSaveScreen;

    /* Wrap the current CloseScreen function */
    pRiva->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = RivaCloseScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }
    /* Done */
    return TRUE;
}

/* Free up any persistent data structures */


/* Do screen blanking */

/* Mandatory */
static Bool
RivaSaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}

static void
RivaSave(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    RivaRegPtr rivaReg = &pRiva->SavedReg;
    vgaHWPtr pVga = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg = &pVga->SavedReg;

    (*pRiva->Save)(pScrn, vgaReg, rivaReg, pRiva->Primary);
}

