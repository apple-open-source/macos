/*
 * Copyright 1997,1998 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *           Matthew Grossman, <mattg@oz.net> - acceleration and misc fixes
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga_driver.c,v 1.58 2001/11/21 22:33:00 alanh Exp $ */

/* everybody includes these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

/* PCI headers */
#include "xf86PciInfo.h"
#include "xf86Pci.h"

/* module versioning */
#include "xf86Version.h"

/* RAC stuff */
#include "xf86Resources.h"

/*  #include "vgaHW.h" */

/* software cursor */
#include "mipointer.h"
/* backing store */
#include "mibstore.h"

/*  #include "mibank.h" */
/* colormap manipulation */
#include "micmap.h"

#include "fb.h"

/* more RAC stuff */
#include "xf86RAC.h"

/* Gamma Correction? */
#include "xf86cmap.h"

#include "tga_regs.h"
#include "BT.h"
#include "tga.h"

#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

#ifdef XvExtension
#include "xf86xv.h"
#include "Xv.h"
#endif

static const OptionInfoRec * TGAAvailableOptions(int chipid, int busid);
static void	TGAIdentify(int flags);
static Bool	TGAProbe(DriverPtr drv, int flags);
static Bool	TGAPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	TGAScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	TGAEnterVT(int scrnIndex, int flags);
static void	TGALeaveVT(int scrnIndex, int flags);
static Bool	TGACloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	TGASaveScreen(ScreenPtr pScreen, int mode);

/* Required if the driver supports mode switching */
static Bool	TGASwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
/* Required if the driver supports moving the viewport */
static void	TGAAdjustFrame(int scrnIndex, int x, int y, int flags);

/* Optional functions */
static void	TGAFreeScreen(int scrnIndex, int flags);
static int	TGAValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
			     int flags);

/* Internally used functions */
static Bool	TGAMapMem(ScrnInfoPtr pScrn);
static Bool	TGAUnmapMem(ScrnInfoPtr pScrn);
static void	TGASave(ScrnInfoPtr pScrn);
static void	TGARestore(ScrnInfoPtr pScrn);
static Bool	TGAModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);

static void     TGARestoreHWCursor(ScrnInfoPtr pScrn);

static void TGADisplayPowerManagementSet(ScrnInfoPtr pScrn,
					 int PowerManagementMode,
					 int flags);

void TGASync(ScrnInfoPtr pScrn);

#define VERSION 4000
#define TGA_NAME "TGA"
#define TGA_DRIVER_NAME "tga"
#define TGA_MAJOR_VERSION 1
#define TGA_MINOR_VERSION 0
#define TGA_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec TGA = {
    VERSION,
    TGA_DRIVER_NAME,
    TGAIdentify,
    TGAProbe,
    TGAAvailableOptions,
    NULL,
    0
};

static SymTabRec TGAChipsets[] = {
    { PCI_CHIP_DEC21030,		"tga" },
    { PCI_CHIP_TGA2,			"tga2" },
    { -1,				NULL }
};

static PciChipsets TGAPciChipsets[] = {
    { PCI_CHIP_DEC21030,	PCI_CHIP_DEC21030,	NULL },
    { PCI_CHIP_TGA2,		PCI_CHIP_TGA2,		NULL },
    { -1,			-1,			RES_UNDEFINED }
};

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_PCI_RETRY,
    OPTION_RGB_BITS,
    OPTION_NOACCEL,
    OPTION_SYNC_ON_GREEN,
    OPTION_DAC_6_BIT,
    OPTION_NOXAAPOLYSEGMENT
} TGAOpts;

static const OptionInfoRec TGAOptions[] = {
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_HW_CURSOR,		"HWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_PCI_RETRY,		"PciRetry",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_RGB_BITS,		"RGBbits",	OPTV_INTEGER,	{0}, FALSE },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SYNC_ON_GREEN,     "SyncOnGreen",  OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_DAC_6_BIT,         "Dac6Bit",      OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOXAAPOLYSEGMENT,  "NoXaaPolySegment",OPTV_BOOLEAN,{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

static RamDacSupportedInfoRec BTramdacs[] = {
    { BT485_RAMDAC },
    { -1 }
};

static const char *ramdacSymbols[] = {
    "BTramdacProbe",
    "RamDacCreateInfoRec",
    "RamDacDestroyInfoRec",
    "RamDacFreeRec",
    "RamDacGetHWIndex",
    "RamDacHandleColormaps",
    "RamDacInit",
    "xf86CreateCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAGCIndex",
    "XAAInit",
    "XAAScreenIndex",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(tgaSetup);

static XF86ModuleVersionInfo tgaVersRec =
{
	"tga",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	TGA_MAJOR_VERSION, TGA_MINOR_VERSION, TGA_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,			/* This is a video driver */
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData tgaModuleData = { &tgaVersRec, tgaSetup, NULL };

pointer
tgaSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&TGA, module, 0);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */

	LoaderRefSymLists(ramdacSymbols, fbSymbols, xaaSymbols, NULL);

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer)1;
    } else {
	if (errmaj) *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

#endif /* XFree86LOADER */

static unsigned int fb_offset_presets[4] = {
	TGA_8PLANE_FB_OFFSET,
	TGA_24PLANE_FB_OFFSET,
	0xffffffff,
	TGA_24PLUSZ_FB_OFFSET
};

static char *tga_cardnames[4] = {
	"TGA 8 Plane",
	"TGA 24 Plane",
	NULL,
	"TGA 24 Plane 3D"
};

static Bool
TGAGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an TGARec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(TGARec), 1);
    /* Initialise it */


    return TRUE;
}

static void
TGAFreeRec(ScrnInfoPtr pScrn)
{
    TGAPtr pTga;

    if (pScrn->driverPrivate == NULL)
	return;

    pTga = TGAPTR(pScrn);

    if(pTga->buffers[0])
      free(pTga->buffers[0]);

    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    return;
}

static const OptionInfoRec *
TGAAvailableOptions(int chipid, int busid)
{
    return TGAOptions;
}

/* Mandatory */
static void
TGAIdentify(int flags)
{
    xf86PrintChipsets(TGA_NAME, "driver for Digital chipsets", TGAChipsets);
    return;
}


/* Mandatory */
static Bool
TGAProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;

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

    if ((numDevSections = xf86MatchDevice(TGA_DRIVER_NAME,
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

    /*
     * All of the cards this driver supports are PCI, so the "probing" just
     * amounts to checking the PCI data that the server has already collected.
     */
    if (xf86GetPciVideoInfo() == NULL) {
	/*
	 * We won't let anything in the config file override finding no
	 * PCI video cards at all.  This seems reasonable now, but we'll see.
	 */
	return FALSE;
    }

    numUsed = xf86MatchPciInstances(TGA_NAME, PCI_VENDOR_DIGITAL,
		   TGAChipsets, TGAPciChipsets, devSections, numDevSections,
		   drv, &usedChips);
				    
    xfree(devSections);
    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
	/*
	 * Check that nothing else has claimed the slots.
	 */
	ScrnInfoPtr pScrn = NULL;
	    
	/* Allocate a ScrnInfoRec and claim the slot */
	if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
					       TGAPciChipsets, NULL, NULL,
					       NULL, NULL, NULL))) {
	    /* Fill in what we can of the ScrnInfoRec */
	    pScrn->driverVersion = VERSION;
	    pScrn->driverName	 = TGA_DRIVER_NAME;
	    pScrn->name		 = TGA_NAME;
	    pScrn->Probe	 = TGAProbe;
	    pScrn->PreInit	 = TGAPreInit;
	    pScrn->ScreenInit	 = TGAScreenInit;
  	    pScrn->SwitchMode	 = TGASwitchMode;
  	    pScrn->AdjustFrame	 = TGAAdjustFrame;
	    pScrn->EnterVT	 = TGAEnterVT;
	    pScrn->LeaveVT	 = TGALeaveVT;
	    pScrn->FreeScreen	 = TGAFreeScreen;
	    pScrn->ValidMode	 = TGAValidMode;
	    foundScreen = TRUE;
	}
    }
    xfree(usedChips);
    return foundScreen;
}

#if 0
/*
 * GetAccelPitchValues -
 *
 * This function returns a list of display width (pitch) values that can
 * be used in accelerated mode.
 */
static int *
GetAccelPitchValues(ScrnInfoPtr pScrn)
{
    int *linePitches = NULL;
    int i, n = 0;
    int *linep = NULL;
    /*     TGAPtr pTga = TGAPTR(pScrn); */
	
    for (i = 0; linep[i] != 0; i++) {
	if (linep[i] != -1) {
	    n++;
	    linePitches = xnfrealloc(linePitches, n * sizeof(int));
	    linePitches[n - 1] = i << 5;
	}
    }

    /* Mark the end of the list */
    if (n > 0) {
	linePitches = xnfrealloc(linePitches, (n + 1) * sizeof(int));
	linePitches[n] = 0;
    }
    return linePitches;
}
#endif /* 0 */

/* Mandatory */
static Bool
TGAPreInit(ScrnInfoPtr pScrn, int flags)
{
    pciVideoPtr pciPtr;
    TGAPtr pTga;
    MessageType from;
    int i;
    ClockRangePtr clockRanges;
    pointer Base;

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

    /* The ramdac module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "ramdac"))
	return FALSE;

    xf86LoaderReqSymLists(ramdacSymbols, NULL);

    /* Allocate the TGARec driverPrivate */
    if (!TGAGetRec(pScrn)) {
	return FALSE;
    }
    pTga = TGAPTR(pScrn);
    
    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*********************
    Handle pci and chipset stuff
    *********************/
		      
    
    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i < pScrn->numEntities; i++) {
	pTga->pEnt = xf86GetEntityInfo(pScrn->entityList[i]);
	if (pTga->pEnt->resources) return FALSE;
	pTga->Chipset = pTga->pEnt->chipset;
	pScrn->chipset = (char *)xf86TokenToString(TGAChipsets,
						   pTga->pEnt->chipset);

	/* TGA is purely PCI */
	if (pTga->pEnt->location.type == BUS_PCI) {
	    pciPtr = xf86GetPciInfoForEntity(pTga->pEnt->index);
	    pTga->PciInfo = pciPtr;
	    pTga->PciTag = pciTag(pTga->PciInfo->bus, 
				  pTga->PciInfo->device,
				  pTga->PciInfo->func);
	}
	else
	    return FALSE;
    }

    /*
     * This shouldn't happen because such problems should be caught in
     * TGAProbe(), but check it just in case.
     */
    if (pScrn->chipset == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", pTga->Chipset);
	return FALSE;
    }
    if (pTga->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
	return FALSE;
    }

    from = X_PROBED;
    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);

    pTga->PciTag = pciTag(pTga->PciInfo->bus, pTga->PciInfo->device,
			  pTga->PciInfo->func);

 

    /*********************
    deal with depth and framebuffer size
    *********************/
    
    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	case 24:
	    /* OK */
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }

    /* we can do option processing now */

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);
    /* Process the options */
    if (!(pTga->Options = xalloc(sizeof(TGAOptions))))
	return FALSE;
    memcpy(pTga->Options, TGAOptions, sizeof(TGAOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pTga->Options);
    if (xf86ReturnOptValBool(pTga->Options, OPTION_PCI_RETRY, FALSE)) {
	pTga->UsePCIRetry = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry enabled\n");
    }

    if(xf86ReturnOptValBool(pTga->Options, OPTION_SYNC_ON_GREEN, FALSE)) {
	pTga->SyncOnGreen = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Sync-on-Green enabled\n");
    }

    if(xf86ReturnOptValBool(pTga->Options, OPTION_DAC_6_BIT, FALSE)) {
	pTga->Dac6Bit = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "6 bit DAC enabled\n");
    }
    
    if(xf86ReturnOptValBool(pTga->Options, OPTION_NOXAAPOLYSEGMENT, FALSE)) {
	pTga->NoXaaPolySegment = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "XAA PolySegment() disabled\n");
    }

    /* end option processing */
    
    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	/* The defaults are OK for us */
	rgb zeros = {0, 0, 0};

	if (!xf86SetWeight(pScrn, zeros, zeros)) {
	    return FALSE;
	} else {
	    /* XXX check that weight returned is supported */
            ;
        }
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	return FALSE;
    } else {
	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported at depth %d\n",
		       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
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
    if (pScrn->depth == 8) {
	/* Default to 8 */
	pScrn->rgbBits = 8;
	if(pTga->Dac6Bit)
	    pScrn->rgbBits = 6;
    }
    from = X_DEFAULT;

    /* determine whether we use hardware or software cursor */
    
    pTga->HWCursor = TRUE;
    if (xf86GetOptValBool(pTga->Options, OPTION_HW_CURSOR, &pTga->HWCursor))
	from = X_CONFIG;
    if (xf86ReturnOptValBool(pTga->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pTga->HWCursor = FALSE;
    }
    
    if(pScrn->depth != 8) {
      pTga->HWCursor = FALSE;
      from = X_WARNING;
      xf86DrvMsg(pScrn->scrnIndex, from,
		 "Hardware cursor currently only works with BT485 ramdac\n");
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pTga->HWCursor ? "HW" : "SW");

    if (xf86ReturnOptValBool(pTga->Options, OPTION_NOACCEL, FALSE)) {
	pTga->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }
        
    if (pTga->pEnt->device->MemBase != 0) {
	pTga->CardAddress = pTga->pEnt->device->MemBase;
	from = X_CONFIG;
    } else {
      pTga->CardAddress = pTga->PciInfo->memBase[0] & 0xFFC00000;/*??*/
    }

    pTga->FbAddress = pTga->CardAddress;
    /* Adjust MMIO region */
    pTga->IOAddress = pTga->CardAddress + TGA_REGS_OFFSET;

    
    /*********************
    determine what sort of TGA card we have -- the only differences are
    framebuffer size and ramdac type, all TGA cards use 21030 chips
    *********************/

    /* check what the user has specified in XF86Config */
    if(pTga->pEnt->device->videoRam) {
      switch(pTga->pEnt->device->videoRam) {
      case 2048:
	pTga->CardType = TYPE_TGA_8PLANE;
	break;
      case 8192:
	pTga->CardType = TYPE_TGA_24PLANE;
	break;
      case 16384:
	pTga->CardType = TYPE_TGA_24PLUSZ;
	break;
      default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "%d KB video RAM specified, driver only supports 2048, 8192, or 16384 KB cards\n",
		   pTga->pEnt->device->videoRam);
	return FALSE;
      }
    }
    else { /* try to divine the amount of RAM */
      switch (pTga->Chipset)
	{
	case PCI_CHIP_TGA2:
	  Base = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO_32BIT,
			       pTga->PciTag, pTga->IOAddress, 0x1000);
	  pTga->CardType = (*(unsigned int *)((char *)Base+TGA_REVISION_REG) >> 21) & 0x3;
	  pTga->CardType ^= (pTga->CardType == 1) ? 0 : 3;
	  xf86UnMapVidMem(pScrn->scrnIndex, Base, 0x1000);
	  break;
	case PCI_CHIP_DEC21030:
	  Base = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO_32BIT,
			       pTga->PciTag, pTga->FbAddress, 4);
	  pTga->CardType = (*(unsigned int *)Base >> 12) & 0xf;
	  xf86UnMapVidMem(pScrn->scrnIndex, Base, 4);
	  break;
	}
    }

    switch (pTga->CardType) {
        case TYPE_TGA_8PLANE:
        case TYPE_TGA_24PLANE:
        case TYPE_TGA_24PLUSZ:
            xf86DrvMsg(pScrn->scrnIndex, from, "Card Name: \"%s\"\n", 
			tga_cardnames[pTga->CardType]);
	    break;
	default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                 "Card \"0x%02x\" is not recognised\n", pTga->CardType);
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Assuming 8 plane TGA with 2MB frame buffer\n");
	    pTga->CardType = TYPE_TGA_8PLANE;
	    break;
    }
    
    /* Adjust framebuffer for card type */
    pTga->FbAddress += fb_offset_presets[pTga->CardType];

    if (!(((pScrn->depth ==  8) && (pTga->CardType == TYPE_TGA_8PLANE)) ||
	  ((pScrn->depth == 24) && (pTga->CardType == TYPE_TGA_24PLANE)) ||
	  ((pScrn->depth == 24) && (pTga->CardType == TYPE_TGA_24PLUSZ)))) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Given depth (%d) is not supported by this card\n",
		   pScrn->depth);
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	       (unsigned long)pTga->FbAddress);

    xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX\n",
	       (unsigned long)pTga->IOAddress);

    /* RAC stuff: we don't have any resources we need to reserve,
       but we should do this here anyway */
    if (xf86RegisterResources(pTga->pEnt->index, NULL, ResExclusive)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "xf86RegisterResources() found resource conflicts\n");
      TGAFreeRec(pScrn);
      return FALSE;
    }

    

    /* HW bpp matches reported bpp */
    pTga->HwBpp = pScrn->bitsPerPixel;

    if (pTga->pEnt->device->videoRam != 0) {
	pScrn->videoRam = pTga->pEnt->device->videoRam;
	from = X_CONFIG;
    } else {
      switch (pTga->CardType) {
      case TYPE_TGA_8PLANE:
	pScrn->videoRam = 2*1024;
	break;
      case TYPE_TGA_24PLANE:
	pScrn->videoRam = 8*1024;
	break;
      case TYPE_TGA_24PLUSZ:
	pScrn->videoRam = 16*1024;
	break;
      }	  
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d kByte\n",
               pScrn->videoRam);

    pTga->FbMapSize = pScrn->videoRam * 1024;

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	TGAFreeRec(pScrn);
	return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);

    /* Load XAA if needed */
    if (!pTga->NoAccel || pTga->HWCursor) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    TGAFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    
    /*********************    
    Let's check what type of DAC we have and reject if necessary
    *********************/
    
    pTga->RamDac = NULL;
    
    if (pTga->CardType != TYPE_TGA_8PLANE) {
        pTga->RamDacRec = NULL;
	pTga->RamDac = NULL;
    } else {

        pTga->RamDacRec = RamDacCreateInfoRec();
        switch (pTga->Chipset)
	{
	case PCI_CHIP_DEC21030:
	    pTga->RamDacRec->ReadDAC = tgaBTInIndReg;
	    pTga->RamDacRec->WriteDAC = tgaBTOutIndReg;
	    pTga->RamDacRec->ReadAddress = tgaBTReadAddress;
	    pTga->RamDacRec->WriteAddress = tgaBTWriteAddress;
	    pTga->RamDacRec->ReadData = tgaBTReadData;
	    pTga->RamDacRec->WriteData = tgaBTWriteData;
	    break;
	case PCI_CHIP_TGA2:
	    pTga->RamDacRec->ReadDAC = tga2BTInIndReg;
	    pTga->RamDacRec->WriteDAC = tga2BTOutIndReg;
	    pTga->RamDacRec->ReadAddress = tga2BTReadAddress;
	    pTga->RamDacRec->WriteAddress = tga2BTWriteAddress;
	    pTga->RamDacRec->ReadData = tga2BTReadData;
	    pTga->RamDacRec->WriteData = tga2BTWriteData;
	    break;
	}

	if (!RamDacInit(pScrn, pTga->RamDacRec)) {
	    RamDacDestroyInfoRec(pTga->RamDacRec);
	    return FALSE;
	}

	TGAMapMem(pScrn);
	    
	pTga->RamDac = BTramdacProbe(pScrn, BTramdacs);

	TGAUnmapMem(pScrn);

	if (pTga->RamDac == NULL)
	    return FALSE;
    }
    
    /*********************
    set up clock and mode stuff
    *********************/
    
    pScrn->progClock = TRUE;
    
    /* Set the min pixel clock */
    pTga->MinClock = 16250;	/* XXX Guess, need to check this */
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n",
	       pTga->MinClock / 1000);

    /*
     * If the user has specified ramdac speed in the XF86Config
     * file, we respect that setting.
     */
    if (pTga->pEnt->device->dacSpeeds[0]) {
	int speed = 0;

	switch (pScrn->bitsPerPixel) {
	case 8:
	   speed = pTga->pEnt->device->dacSpeeds[DAC_BPP8];
	   break;
	case 32:
	   speed = pTga->pEnt->device->dacSpeeds[DAC_BPP32];
	   break;
	}
	if (speed == 0)
	    pTga->MaxClock = pTga->pEnt->device->dacSpeeds[0];
	else
	    pTga->MaxClock = speed;
	from = X_CONFIG;
    } else {
	switch (pTga->Chipset) {
	case PCI_CHIP_DEC21030:
		pTga->MaxClock = 135000;
		break;
	case PCI_CHIP_TGA2:
		pTga->MaxClock = 170000;
		break;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Max pixel clock is %d MHz\n",
	       pTga->MaxClock / 1000);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pTga->MinClock;
    clockRanges->maxClock = pTga->MaxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = FALSE;	/* XXX check this */
    clockRanges->doubleScanAllowed = FALSE;	/* XXX check this */

    if(pScrn->display->virtualX || pScrn->display->virtualY) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "TGA does not support a virtual desktop\n");
	pScrn->display->virtualX = 0;
	pScrn->display->virtualY = 0;
    }
    
    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our TGAValidMode() already takes
     * care of this, we don't worry about setting them here.
     */
    /* Select valid modes from those available */
    /*
     * XXX Assuming min pitch 256, max 2048
     * XXX Assuming min height 128, max 2048
     */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			  pScrn->display->modes, clockRanges,
			  NULL, 256, 2048,
			  pScrn->bitsPerPixel, 128, 2048,
			  pScrn->display->virtualX,
			  pScrn->display->virtualY,
			  pTga->FbMapSize,
			  LOOKUP_BEST_REFRESH);

    if (i == -1) {
	TGAFreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	TGAFreeRec(pScrn);
	return FALSE;
    }

    if(i > 1) {
	DisplayModePtr mp1 = NULL, mp2 = NULL;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "TGA only supports one mode, using first mode.\n");
	mp1 = pScrn->modes->next;
	mp2 = mp1;
	while(mp1 && mp1->next != mp1) {
	    mp1 = mp1->next;
	    xf86DeleteMode(&(pScrn->modes), mp2);
	    mp2 = mp1;
	}
    }
      
    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /*
     This is a bit of a hack; we seem to have to init
     the TGA2 chipset knowing what the mode is, so we
     do this now as soon as we know it...
    */
    if (pTga->Chipset == PCI_CHIP_TGA2) {
      TGA2SetupMode(pScrn);
    }

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    return TRUE;
}


/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
TGAMapMem(ScrnInfoPtr pScrn)
{
    TGAPtr pTga;

    pTga = TGAPTR(pScrn);

    /*
     * Map IO registers to virtual address space
     */ 

    /* TGA doesn't need a sparse memory mapping, because all register
       accesses are doublewords */
    
    pTga->IOBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO_32BIT,
				      pTga->PciTag,
				      pTga->IOAddress, 0x100000);
    if (pTga->IOBase == NULL)
	return FALSE;

    pTga->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				 pTga->PciTag,
				 (unsigned long)pTga->FbAddress,
				 pTga->FbMapSize);
    if (pTga->FbBase == NULL)
	return FALSE;

    if (pTga->Chipset == PCI_CHIP_DEC21030)
	return TRUE;

    pTga->ClkBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO_32BIT,
			pTga->PciTag,
			(unsigned long)pTga->CardAddress + TGA2_CLOCK_OFFSET,
			0x10000);
    if (pTga->ClkBase == NULL)
	return FALSE;

    pTga->DACBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO_32BIT,
			pTga->PciTag,
			(unsigned long)pTga->CardAddress + TGA2_RAMDAC_OFFSET,
			0x10000);
    if (pTga->DACBase == NULL)
	return FALSE;

    /*
     * This is a hack specifically for the TGA2 code, as it sometimes
     * calculates/uses addresses in TGA2 memory which are NOT mmapped
     * by the normal framebuffer code above. This most frequently occurs
     * when displaying something close to the top-left corner (in the
     * routines CopyLine{Forwards,Backwards}.
     *
     * This could most likely also be fixed by further modifying the
     * code, but it (the code) is ugly enough already... ;-}
     *
     * So, the workaround is to simply mmap an additional PAGE of
     * framebuffer memory in front of the normal mmap to prevent
     * SEGVs from happening.
     */
    pTga->HACKBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				 pTga->PciTag,
				 (unsigned long)pTga->FbAddress - getpagesize(),
				 getpagesize());
    if (pTga->HACKBase == NULL)
	return FALSE;

    return TRUE;
}


/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
TGAUnmapMem(ScrnInfoPtr pScrn)
{
    TGAPtr pTga;

    pTga = TGAPTR(pScrn);

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTga->IOBase, 0x100000);
    pTga->IOBase = NULL;

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTga->FbBase, pTga->FbMapSize);
    pTga->FbBase = NULL;

    if (pTga->Chipset == PCI_CHIP_DEC21030)
	return TRUE;

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTga->ClkBase, 0x10000);
    pTga->ClkBase = NULL;

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTga->DACBase, 0x10000);
    pTga->DACBase = NULL;

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTga->HACKBase, getpagesize());
    pTga->HACKBase = NULL;

    return TRUE;
}


/*
 * This function saves the video state.
 */
static void
TGASave(ScrnInfoPtr pScrn)
{
    TGAPtr pTga;
    TGARegPtr tgaReg;
    RamDacHWRecPtr pBT;
    RamDacRegRecPtr BTreg;

    pTga = TGAPTR(pScrn);
    tgaReg = &pTga->SavedReg;

    DEC21030Save(pScrn, tgaReg);
    if (pTga->RamDac) { /* must be BT485... */
        pBT = RAMDACHWPTR(pScrn);
	BTreg = &pBT->SavedReg;
	(*pTga->RamDac->Save)(pScrn, pTga->RamDacRec, BTreg);
    } else switch (pTga->Chipset)
    {
    case PCI_CHIP_TGA2:
        IBM561ramdacSave(pScrn, pTga->Ibm561saveReg);
	break;
    case PCI_CHIP_DEC21030:
        BT463ramdacSave(pScrn, pTga->Bt463saveReg);
	break;
    }
}


/*
 * Initialise a new mode.  This is currently still using the old
 * "initialise struct, restore/write struct to HW" model.  That could
 * be changed.
 */

static Bool
TGAModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    int ret = -1;
    TGAPtr pTga;
    TGARegPtr tgaReg;
    RamDacHWRecPtr pBT;
    RamDacRegRecPtr BTreg;

    pTga = TGAPTR(pScrn);

    pScrn->vtSema = TRUE;

    ret = DEC21030Init(pScrn, mode);

    if (pTga->Chipset == PCI_CHIP_TGA2 && pTga->RamDac == NULL)
        IBM561ramdacHWInit(pScrn);

    if (!ret)
	return FALSE;

    /* Program the registers */
    tgaReg = &pTga->ModeReg;

    DEC21030Restore(pScrn, tgaReg);

    if (pTga->RamDac != NULL) {
        pBT = RAMDACHWPTR(pScrn);
	BTreg = &pBT->ModeReg;
	(*pTga->RamDac->Restore)(pScrn, pTga->RamDacRec, BTreg);
	if (pTga->Chipset == PCI_CHIP_TGA2) {
	    pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0x00, 0x01);
	    pTga->RamDacRec->WriteDAC(pScrn, BT_STATUS_REG, 0x00, 0x0c);
	}
	pTga->RamDacRec->WriteDAC(pScrn, BT_PIXEL_MASK, 0x00, 0xff);
    } else {
        switch (pTga->Chipset) {
	case PCI_CHIP_TGA2:
	    IBM561ramdacRestore(pScrn, pTga->Ibm561modeReg);
	    break;
	case PCI_CHIP_DEC21030:
	    BT463ramdacRestore(pScrn, pTga->Bt463modeReg);
	    break;
	}
    }
    return TRUE;
}

/*
 * Restore the initial (text) mode.
 */
static void 
TGARestore(ScrnInfoPtr pScrn)
{
    TGAPtr pTga;
    TGARegPtr tgaReg;
    RamDacHWRecPtr pBT;
    RamDacRegRecPtr BTreg;

    pTga = TGAPTR(pScrn);
    tgaReg = &pTga->SavedReg;

    /* Initial Text mode clock */
    tgaReg->tgaRegs[0x0A] = 25175;

    DEC21030Restore(pScrn, tgaReg);

    if (pTga->RamDac != NULL) {
        pBT = RAMDACHWPTR(pScrn);
	BTreg = &pBT->SavedReg;
	(*pTga->RamDac->Restore)(pScrn, pTga->RamDacRec, BTreg);
	if (pTga->Chipset == PCI_CHIP_TGA2) {
	    pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0x00, 0x01);
	    pTga->RamDacRec->WriteDAC(pScrn, BT_STATUS_REG, 0x00, 0x00);
	}
	pTga->RamDacRec->WriteDAC(pScrn, BT_PIXEL_MASK, 0x00, 0xff);
    } else switch (pTga->Chipset) {
    case PCI_CHIP_TGA2:
        IBM561ramdacRestore(pScrn, pTga->Ibm561saveReg);
	break;
    case PCI_CHIP_DEC21030:
        BT463ramdacRestore(pScrn, pTga->Bt463saveReg);
	break;
    }

 if (pTga->HWCursor)
     TGARestoreHWCursor(pScrn);
}


/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
TGAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    TGAPtr pTga;
    int ret;
    VisualPtr visual;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];
    pTga = TGAPTR(pScrn);

    /* Map the TGA memory and MMIO areas */
    if (!TGAMapMem(pScrn))
	return FALSE;

#if 1
    /* dump original register contents */
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "MODE 0x%x\n",
	       TGA_READ_REG(TGA_MODE_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "VALID 0x%x\n",
	       TGA_READ_REG(TGA_VALID_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "DEEP 0x%x\n",
	       TGA_READ_REG(TGA_DEEP_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PIXSH 0x%x\n",
	       TGA_READ_REG(TGA_PIXELSHIFT_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ROP 0x%x\n",
	       TGA_READ_REG(TGA_RASTEROP_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "HORIZ 0x%x\n",
	       TGA_READ_REG(TGA_HORIZ_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "VERT 0x%x\n",
	       TGA_READ_REG(TGA_VERT_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PIXMSK 0x%x\n",
	       TGA_READ_REG(TGA_PIXELMASK_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "REV 0x%x\n",
	       TGA_READ_REG(TGA_REVISION_REG));
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "VADDR 0x%x\n",
	       TGA_READ_REG(TGA_BASE_ADDR_REG));
#endif

    /* Save the current state */
    TGASave(pScrn);

    /* Initialise the first mode */
    TGAModeInit(pScrn, pScrn->currentMode);

    /* Darken the screen for aesthetic reasons and set the viewport */
    TGASaveScreen(pScreen, SCREEN_SAVER_ON);

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     *
     * For most PC hardware at depths >= 8, the defaults that fb uses
     * are not appropriate.  In this driver, we fixup the visuals after.
     */

    /*
     * Reset visual list.
     */
    miClearVisualTypes();

    /* Setup the visuals we support. */

    /*
     * For bpp > 8, the default visuals are not acceptable because we only
     * support TrueColor and not DirectColor.  To deal with this, call
     * miSetVisualTypes for each visual supported.
     */

    if (pScrn->bitsPerPixel > 8) {
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
				pScrn->defaultVisual))
	    return FALSE;
    } else {
	if (!miSetVisualTypes(pScrn->depth,
			      miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
	    return FALSE;
    }

    miSetPixmapDepths ();

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    switch (pScrn->bitsPerPixel) {
    case 8:
    case 32:
	ret = fbScreenInit(pScreen, pTga->FbBase, pScrn->virtualX,
			pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
			pScrn->displayWidth, pScrn->bitsPerPixel);
	break;
    default:
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Internal error: invalid bpp (%d) in TGAScrnInit\n",
		   pScrn->bitsPerPixel);
	    ret = FALSE;
	break;
    }
    if (!ret)
	return FALSE;

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

    /* must be after RGB ordering fixed */
    
    fbPictureInit (pScreen, 0, 0);
    
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* we should ALWAYS do this */
    if (pScrn->bitsPerPixel == 8) {
      TGA_WRITE_REG(SIMPLE | X11 | BPP8PACKED, TGA_MODE_REG);
      TGA_WRITE_REG(0x3 | BPP8PACKED, TGA_RASTEROP_REG);
      if (pTga->Chipset == PCI_CHIP_TGA2)
	TGA_WRITE_REG(2 << 28, TGA_DEEP_REG);
    } else {
      TGA_WRITE_REG(SIMPLE | X11 | BPP24, TGA_MODE_REG);
      TGA_WRITE_REG(0x3 | BPP24, TGA_RASTEROP_REG);
      if (pTga->Chipset == PCI_CHIP_TGA2)
	TGA_WRITE_REG((7 << 2) | 1 | (2 << 28), TGA_DEEP_REG);
    }
    TGA_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);
    TGA_WRITE_REG(0xFFFFFFFF, TGA_PIXELMASK_REG);

    if (!pTga->NoAccel) {
        switch (pTga->Chipset)
        {
	case PCI_CHIP_TGA2:
	case PCI_CHIP_DEC21030:
	  if(DEC21030AccelInit(pScreen) == FALSE) {
	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			 "XAA Initialization failed\n");
	      return(FALSE);
	  }
	  break;
        }
    }

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
       Must follow software cursor initialization*/
    if (pTga->HWCursor) { 
      if(!TGAHWCursorInit(pScreen)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		   "Hardware cursor initialization failed\n");
	return(FALSE);
      }
    }


    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    if ((pScrn->bitsPerPixel==8) && 
        (!RamDacHandleColormaps(pScreen, 256, pScrn->rgbBits,
	 CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR)))
	return FALSE;

    pTga->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = TGACloseScreen;
    pScreen->SaveScreen = TGASaveScreen;

    if(xf86DPMSInit(pScreen, TGADisplayPowerManagementSet, 0) == FALSE)
      ErrorF("DPMS initialization failed!\n");

#ifdef XvExtension
    {
      XF86VideoAdaptorPtr *ptr;
      int n;

      pScrn->memPhysBase = pTga->FbAddress;
      pScrn->fbOffset = 0;

      n = xf86XVListGenericAdaptors(pScrn,&ptr);

      if(n) {
	xf86XVScreenInit(pScreen, ptr, n);
      }

    }
#endif
    
    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* unblank the screen */
    TGASaveScreen(pScreen, SCREEN_SAVER_OFF);
    
    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
TGASwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return TGAModeInit(xf86Screens[scrnIndex], mode);
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
static void 
TGAAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    /* we don't support virtual desktops, because TGA doesn't have the
       ability to set the start of the visible framebuffer at an arbitrary
       pixel */
    return;
}

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */

/* Mandatory */
static Bool
TGAEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    /* Should we re-save the text mode on each VT enter? */
    if (!TGAModeInit(pScrn, pScrn->currentMode))
	return FALSE;

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
TGALeaveVT(int scrnIndex, int flags)
{
    TGAPtr pTga;
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    pTga = TGAPTR(pScrn);
    TGARestore(pScrn);

    /* no longer necessary with new VT switching code */
/*      memset(pTga->FbBase, 0, pTga->FbMapSize); */
    return;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.
 */

/* Mandatory */
static Bool
TGACloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TGAPtr pTga = TGAPTR(pScrn);

    TGARestore(pScrn);
    /*    memset(pTga->FbBase, 0, pScrn->videoRam * 1024); */
    TGASync(pScrn);
    TGAUnmapMem(pScrn);

    if(pTga->AccelInfoRec)
	XAADestroyInfoRec(pTga->AccelInfoRec);
    pScrn->vtSema = FALSE;
    
    pScreen->CloseScreen = pTga->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any per-generation data structures */

/* Optional */
static void
TGAFreeScreen(int scrnIndex, int flags)
{
    RamDacFreeRec(xf86Screens[scrnIndex]);
    TGAFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
TGAValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    if (mode->Flags & V_INTERLACE)
	return(MODE_BAD);

    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
TGASaveScreen(ScreenPtr pScreen, int mode)
    /* this function should blank the screen when unblank is FALSE and
       unblank it when unblank is TRUE -- it doesn't actually seem to be
       used for much though */
{
    TGAPtr pTga;
    ScrnInfoPtr pScrn;
    int valid_reg = 0;
    Bool unblank;

    pScrn = xf86Screens[pScreen->myNum];
    pTga = TGAPTR(pScrn);
    valid_reg = TGA_READ_REG(TGA_VALID_REG);
    valid_reg &= 0xFFFFFFFC;

    unblank = xf86IsUnblank(mode);

    if(unblank == FALSE)
	valid_reg |= 0x3;
    else /* this function is sometimes called w/1 || 2 as TRUE */
	valid_reg |= 0x1;

    TGA_WRITE_REG(valid_reg, TGA_VALID_REG);

/*      ErrorF("TGASaveScreen called\n"); */
    
    return TRUE;
}


/*
 * TGADisplayPowerManagementSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 */
static void
TGADisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
			     int flags)
{
  TGAPtr pTga;
  int valid_reg = 0;

  pTga = TGAPTR(pScrn);
  valid_reg = TGA_READ_REG(TGA_VALID_REG);
  valid_reg &= 0xFFFFFFFC;
  
  switch(PowerManagementMode) {
  case DPMSModeOn:
    /* HSync: On, VSync: On */
    valid_reg |= 0x1;
    break;
  case DPMSModeStandby: 
  case DPMSModeSuspend:
    /* TGA gives us a function to blank the screen while maintaining sync...
       I guess we can just use that here... */
    valid_reg |= 0x3;
    break;
  case DPMSModeOff:
    valid_reg |= 0x2;
    break;
  default:
    ErrorF("Invalid PowerManagementMode %d passed to TGADisplayPowerManagementSet\n", PowerManagementMode);
    break;
  }
  
  TGA_WRITE_REG(valid_reg, TGA_VALID_REG);
  return;
}

static void
TGARestoreHWCursor(ScrnInfoPtr pScrn)
     /*
       from tga.c in the linux kernel...may not work for BSD...
       when the cursor is restored, it is one line down from where it should
       be...this is disconcerting, but purely cosmetic.  Unfortunately reading
       in the cursor framebuffer doesn't seem to work, I get a bunch of junk
       at the beginning...other than that, see tga_cursor.c
       I believe this to be a problem with the linux kernel code.
       Hmm...this seems to be a 2.0.* problem, 2.2 works ok
     */
{
  unsigned char *p = NULL;
  int i = 0;
  TGAPtr pTga;

  /* Making this static prevents EGCS from compiling memset code
     to initialize it, which was causing a problem. */
  static const CARD32 tga_cursor_source[128] = {
    0x000000ff, 0x00000000, 0x000000ff, 0x00000000, 0x000000ff,
    0x00000000, 0x000000ff, 0x00000000, 0x000000ff, 0x00000000,
    0x000000ff, 0x00000000, 0x000000ff, 0x00000000, 0x000000ff,
    0x00000000, 0x000000ff, 0x00000000, 0x000000ff, 0x00000000,
    0x000000ff, 0x00000000, 0x000000ff, 0x00000000, 0x000000ff,
    0x00000000, 0x000000ff, 0x00000000, 0x000000ff, 0x00000000,
    0x000000ff, 0x00000000,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
  };

  /* this is the linux console hw cursor...what about the bsd console? */
  /* what about tgafb? */
  pTga = TGAPTR(pScrn);

  /* we want to move the cursor off the screen before we do anything with it
     otherwise, there is a "ghost cursor" that shows up */

  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_X_LOW, 0x00, 0);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_X_HIGH, 0xF0, 0);

  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_Y_LOW, 0x00, 0);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_Y_HIGH, 0xF0, 0);

  
  /* set a windows cursor -- oddly, this doesn't seem necessary */
  pTga->RamDacRec->WriteDAC(pScrn, BT_COMMAND_REG_2, 0xFC, 0x02);
  
  /* set a 64 bit cursor */
/*    pTga->RamDacRec->WriteDAC(pScrn, BT_COMMAND_REG_0, 0x7F, 0x80); */
/*    pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0x00, 0x01); */
/*    pTga->RamDacRec->WriteDAC(pScrn, BT_STATUS_REG, 0xF8, 0x04); */

  /* set the colors */
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_WR_ADDR, 0xFC, 0x01);
  
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0xaa);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0xaa);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0xaa);

  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);

  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);

  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, 0x00);
 
  
  /* load the console cursor */
  pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0xFC, 0x00);
  p = (unsigned char *)tga_cursor_source;
  for(i = 0; i < 512; i++)
    pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_RAM_DATA, 0x00, *p++);
  for(i = 0; i < 512; i++)
    pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_RAM_DATA, 0x00, 0xff);

  return;
}


/*
 * This is the implementation of the Sync() function.
 */
void
TGASync(ScrnInfoPtr pScrn)
{
    TGAPtr pTga = TGAPTR(pScrn);
    unsigned int stat;

    switch (pTga->Chipset)
    {
    case PCI_CHIP_TGA2:
      /* This code is weird, but then so is TGA2... ;-} */
	mem_barrier();
	while((stat = TGA_READ_REG(TGA_CMD_STAT_REG))) {
	    if (((stat >> 8) & 0xff) == ((stat >> 16) & 0xff)) {
	        TGA_WRITE_REG(0, TGA_CMD_STAT_REG);
		mem_barrier();
#if 0
ErrorF("TGASync: writing CMD_STATUS\n");
#endif
	    }
	    usleep(1000);
	}
	break;

    case PCI_CHIP_DEC21030:
#if 0
        /* I'm experiencing lockups which could be due to this function.
	   We don't seem to need it anyway...
	*/
        while (TGA_READ_REG(TGA_CMD_STAT_REG) & 0x01);
#endif
	break;
    }

    return;
}

