/*
 * Copyright 1995-2000 by Robin Cutshaw <robin@XFree86.Org>
 * Copyright 1998 by Number Nine Visual Technology, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Robin Cutshaw not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Robin Cutshaw and Number Nine make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * ROBIN CUTSHAW AND NUMBER NINE DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL ROBIN CUTSHAW OR NUMBER NINE BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/i128_driver.c,v 1.29 2003/02/17 16:08:28 dawes Exp $ */


/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"

/* All drivers need this */
#include "xf86_ansic.h"

#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* vgaHW module is only used to save/restore fonts by this driver */
#include "vgaHW.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"
#include "micmap.h"

#include "xf86DDC.h"
#include "xf86RAC.h"
#include "vbe.h"

#include "xaa.h"
#include "xf86cmap.h"
#include "fb.h"

#include "xf86xv.h"
#include "Xv.h"

/* driver specific includes */
#include "i128.h"
#include "i128reg.h"

/*
 * Forward definitions for the functions that make up the driver.
 */

/* Mandatory functions */
static const OptionInfoRec *	I128AvailableOptions(int chipid, int busid);
static void	I128Identify(int flags);
static Bool	I128Probe(DriverPtr drv, int flags);
static Bool	I128PreInit(ScrnInfoPtr pScrn, int flags);
static Bool	I128ScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	I128EnterVT(int scrnIndex, int flags);
static void	I128LeaveVT(int scrnIndex, int flags);
static Bool	I128CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	I128SaveScreen(ScreenPtr pScreen, int mode);

/* Optional functions */
static void	I128FreeScreen(int scrnIndex, int flags);
static int	I128ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
			     int flags);
static void	I128DisplayPowerManagementSet(ScrnInfoPtr pScrn,
					     int PowerManagementMode,
					     int flags);

/* Internally used functions */
static Bool	I128GetRec(ScrnInfoPtr pScrn);
static void	I128FreeRec(ScrnInfoPtr pScrn);
static Bool	I128MapMem(ScrnInfoPtr pScrn);
static Bool	I128UnmapMem(ScrnInfoPtr pScrn);
static void	I128Save(ScrnInfoPtr pScrn);
static void	I128Restore(ScrnInfoPtr pScrn);
static Bool	I128ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static int	I128CountRam(ScrnInfoPtr pScrn);
static void	I128SoftReset(ScrnInfoPtr pScrn);
static Bool     I128I2CInit(ScrnInfoPtr pScrn);
static xf86MonPtr I128getDDC(ScrnInfoPtr pScrn);
#if 0
static unsigned int I128DDC1Read(ScrnInfoPtr pScrn);
#endif

#define I128_VERSION 4000
#define I128_NAME "I128"
#define I128_DRIVER_NAME "i128"
#define I128_MAJOR_VERSION 1
#define I128_MINOR_VERSION 0
#define I128_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec I128 = {
    I128_VERSION,
    I128_DRIVER_NAME,
    I128Identify,
    I128Probe,
    I128AvailableOptions,
    NULL,
    0
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(i128Setup);

static XF86ModuleVersionInfo i128VersRec =
{
	"i128",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	I128_MAJOR_VERSION, I128_MINOR_VERSION, I128_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,			/* This is a video driver */
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

/*
 * XF86ModuleData structure is the first part of the driver that is used
 * by the module loader.  It provides the XF86ModuleVersionInfo structure
 * used to verify that the module version is compatable with the loader
 * version.  It also provides a pointer to the module specific
 * ModuleSetupProc() and ModuleTearDownProc() functions.
 */

XF86ModuleData i128ModuleData = { &i128VersRec, i128Setup, NULL };

#endif


/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.  These are provided to the
 * LoaderRefSymLists() function in the module specific Setup() function.
 */

static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIOBase",  
    "vgaHWGetIndex",  
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    NULL
};      

static const char *fbSymbols[] = {
    "fbScreenInit",
    "fbPictureInit",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    "xf86PrintEDID",
    "xf86SetDDCproperties",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

#ifdef XFree86LOADER
/* XXX The vbe module isn't currently loaded. */
static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    NULL
};

/* XXX The int10 module isn't currently loaded. */
static const char *int10Symbols[] = {
    "xf86InitInt10",
    "xf86FreeInt10",
    NULL
};
#endif


#ifdef XFree86LOADER

/* Mandatory
 *
 * The Setup() function is the first entry point called once that the
 * module has been linked into the server.  It adds this driver to
 * the driver list and lets the server know which symbols it might use.
 * This is only called once, not called with each server generation.
 *
 * Arguments:
 *		pointer module - module being loaded, passed to xf86AddDriver()
 *		pointer opts   - unused but contains options from config file
 *		int *errmaj    - if function error returns major error value
 *		int *errmin    - if function error returns minor error value
 * Returns:
 *		pointer to TearDownData which is passed to TearDownProc()
 *		or NULL for failure.
 */

static pointer
i128Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    /* This module should be loaded only once, but check to be sure. */

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&I128, module, 0);

	/*
	 * Modules that this driver always requires may be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * Tell the loader about symbols from other modules that this module
	 * might refer to.
	 */
	LoaderRefSymLists(fbSymbols,
			  xaaSymbols, 
			  ramdacSymbols,
			  ddcSymbols,
			  ddcSymbols,
			  i2cSymbols,
			  vbeSymbols,
			  int10Symbols,
			  vgahwSymbols,
			  NULL);

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


/* Define supported chipsets.  Used by Probe(). */

static SymTabRec I128Chipsets[] = {
    { PCI_CHIP_I128,		"i128" },
    { PCI_CHIP_I128_2,		"i128v2" },
    { PCI_CHIP_I128_T2R,	"i128t2r" },
    { PCI_CHIP_I128_T2R4,	"i128t2r4" },
    {-1,			NULL }
};

static PciChipsets I128PciChipsets[] = {
    { PCI_CHIP_I128,		PCI_CHIP_I128,		NULL },
    { PCI_CHIP_I128_2,		PCI_CHIP_I128_2,	NULL },
    { PCI_CHIP_I128_T2R,	PCI_CHIP_I128_T2R,	NULL },
    { PCI_CHIP_I128_T2R4,	PCI_CHIP_I128_T2R4,	NULL },
    { -1,			-1,			RES_UNDEFINED }
};

/* Mandatory
 *
 * The Probe() function is the second entry point called once that the
 * module has been linked into the server.  This function finds all
 * instances of hardware that it supports and allocates a ScrnInfoRec
 * using xf86ConfigPciEntity() for each unclaimed slot.  This should be
 * a minimal probe and under no circumstances should it leave the hardware
 * state changed.  No initialisations other than the required ScrnInfoRec
 * should be done and no data structures should be allocated.
 *
 * Arguments:
 *		DriverPtr drv - pointer to the driver structure
 *		int flags     - PROBE_DEFAULT for normal function
 *		                PROBE_DETECT for use with "-config" and "-probe"
 * Returns:
 *		Bool TRUE if a screen was allocated, FALSE otherwise
 */

static Bool
I128Probe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;

    /*
     * Check if there has been a chipset override in the config file.
     * For this we must find out if there is an active device section which
     * is relevant, i.e., which has no driver specified or has THIS driver
     * specified.
     */

    if ((numDevSections = xf86MatchDevice(I128_DRIVER_NAME,
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

    numUsed = xf86MatchPciInstances(I128_NAME, PCI_VENDOR_NUMNINE,
			I128Chipsets, I128PciChipsets, devSections,
			numDevSections, drv, &usedChips);

    /* Free it since we don't need that list after this */
    xfree(devSections);

    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT) {
	xfree(usedChips);
	return FALSE;
    }

    for (i = 0; i < numUsed; i++) {
	ScrnInfoPtr pScrn = NULL;
	
	/* Allocate a ScrnInfoRec and claim the slot */
        if ((pScrn = xf86ConfigPciEntity(pScrn, 0,usedChips[i],
                                         I128PciChipsets, NULL, NULL,
                                         NULL, NULL, NULL)) == NULL)
		continue;

	
	/* Fill in what we can of the ScrnInfoRec */
	pScrn->driverVersion	= I128_VERSION;
	pScrn->driverName	= I128_DRIVER_NAME;
	pScrn->name		= I128_NAME;
	pScrn->Probe		= I128Probe;
	pScrn->PreInit		= I128PreInit;
	pScrn->ScreenInit	= I128ScreenInit;
	pScrn->SwitchMode	= I128SwitchMode;
	pScrn->AdjustFrame	= I128AdjustFrame;
	pScrn->EnterVT		= I128EnterVT;
	pScrn->LeaveVT		= I128LeaveVT;
	pScrn->FreeScreen	= I128FreeScreen;
	pScrn->ValidMode	= I128ValidMode;
	foundScreen = TRUE;
    }

    xfree(usedChips);

    return foundScreen;
}


/* Mandatory
 *
 * The Identify() function is the third entry point called once that the
 * module has been linked into the server.  This function prints driver
 * identity information.
 *
 * Arguments:
 *		int flags     - currently unused
 * Returns:
 *		no return
 */

static void
I128Identify(int flags)
{
    xf86PrintChipsets(I128_NAME, "driver for Number Nine I128 chipsets",
	I128Chipsets);
}


/*
 * Define options that this driver will accept.  Used by AvailableOptions().
 */

typedef enum {
    OPTION_FLATPANEL,
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_SYNC_ON_GREEN,
    OPTION_NOACCEL,
    OPTION_SHOWCACHE,
    OPTION_DAC6BIT,
    OPTION_DEBUG
} I128Opts;

static const OptionInfoRec I128Options[] = {
    { OPTION_FLATPANEL,		"FlatPanel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_HW_CURSOR,		"HWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SYNC_ON_GREEN,	"SyncOnGreen",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SHOWCACHE,		"ShowCache",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_DAC6BIT,		"Dac6Bit",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_DEBUG,		"Debug",	OPTV_BOOLEAN,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};


/* Mandatory
 *
 * The AvailableOptions() function is called to provide the options that
 * this driver will accept.  This is used with the "-configure" server option.
 *
 * Arguments:
 *		int chipid  - currently unused
 *		int busid   - currently unused
 * Returns:
 *		const OptionInfoRec * - all accepted options
 */

static const OptionInfoRec *
I128AvailableOptions(int chipid, int busid)
{
    return I128Options;
}


/* Mandatory
 *
 * The PreInit() function called after the Probe() function once at
 * server startup and not at each server generation.  Only things that
 * are persistent across server generations can be initialized here.
 * This function determines if the configuration is usable and, if so,
 * initializes those parts of the ScrnInfoRec that can be set at the
 * beginning of the first server generation.  This should be done in
 * the least intrusive way possible.  Note that although the ScrnInfoRec
 * has been allocated, the ScreenRec has not.
 *
 * Use xf86AllocateScrnInfoPrivateIndex() for persistent data across
 * screen generations and AllocateScreenprivateIndex() in ScreenInit()
 * for per-generation data.
 *
 * Arguments:
 *		ScrnInfoPtr pScrn - 
 *		int flags     - PROBE_DEFAULT for normal function
 *		                PROBE_DETECT for use with "-config" and "-probe"
 * Returns:
 *		Bool TRUE if ScrnInfoRec was initialized, FALSE otherwise
 */

static Bool
I128PreInit(ScrnInfoPtr pScrn, int flags)
{
    I128Ptr pI128;
    vgaHWPtr hwp;
    int i;
    int bytesPerPixel;
    ClockRangePtr clockRanges;
    MessageType from;
    IOADDRESS iobase;
    char *ramdac = NULL;
    CARD32 tmpl, tmph, tmp;
    unsigned char n, m, p, mdc, df;
    float mclk;
    xf86MonPtr mon;

    /* Check the number of entities, and fail if it isn't one. */
    if (pScrn->numEntities != 1)
	return FALSE;

    /* Allocate the I128Rec driverPrivate */
    I128GetRec(pScrn);

    pI128 = I128PTR(pScrn);

    /* Get the entity, and make sure it is PCI. */
    pI128->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pI128->pEnt->location.type != BUS_PCI)
	return FALSE;

    if (flags & PROBE_DETECT) {
	/* I128ProbeDDC(pScrn, pI128->pEnt->index); */
	return TRUE;
    }

    /* Find the PCI info for this screen */
    pI128->PciInfo = xf86GetPciInfoForEntity(pI128->pEnt->index);
    pI128->PciTag = pciTag(pI128->PciInfo->bus, pI128->PciInfo->device,
			  pI128->PciInfo->func);

    pI128->Primary = xf86IsPrimaryPci(pI128->PciInfo);

    /* The vgahw module should be allocated here when needed */
    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;
    
    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn))
        return FALSE;
 
    hwp = VGAHWPTR(pScrn);
    vgaHWGetIOBase(hwp);

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * Our default depth is 8, so pass it to the helper function.
     * We support both 24bpp and 32bpp layouts, so indicate that.
     */

    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, Support32bppFb)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	case 15:
	case 16:
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
    xf86PrintDepthBpp(pScrn);

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

    bytesPerPixel = pScrn->bitsPerPixel / 8;

    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pI128->Options = xalloc(sizeof(I128Options))))
	return FALSE;
    memcpy(pI128->Options, I128Options, sizeof(I128Options));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pI128->Options);

    if (pScrn->depth == 8)
	pScrn->rgbBits = 8;

    /*
     * The preferred method is to use the "hw cursor" option as a tri-state
     * option, with the default set above.
     */
    from = X_DEFAULT;
    pI128->HWCursor = TRUE;
    if (xf86GetOptValBool(pI128->Options, OPTION_HW_CURSOR, &pI128->HWCursor)) {
	from = X_CONFIG;
    }
    /* For compatibility, accept this too (as an override) */
    if (xf86ReturnOptValBool(pI128->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pI128->HWCursor = FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pI128->HWCursor ? "HW" : "SW");
    if (xf86ReturnOptValBool(pI128->Options, OPTION_NOACCEL, FALSE)) {
	pI128->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    } else pI128->NoAccel = FALSE;
    if (xf86ReturnOptValBool(pI128->Options, OPTION_SYNC_ON_GREEN, FALSE)) {
	pI128->DACSyncOnGreen = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Sync-on-Green enabled\n");
    } else pI128->DACSyncOnGreen = FALSE;
    if (xf86ReturnOptValBool(pI128->Options, OPTION_SHOWCACHE, FALSE)) {
	pI128->ShowCache = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShowCache enabled\n");
    } else pI128->ShowCache = FALSE;
    if (xf86ReturnOptValBool(pI128->Options, OPTION_DAC6BIT, FALSE)) {
	pI128->DAC8Bit = FALSE;
	pScrn->rgbBits = 6;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Dac6Bit enabled\n");
    } else pI128->DAC8Bit = TRUE;
    if (xf86ReturnOptValBool(pI128->Options, OPTION_DEBUG, FALSE)) {
	pI128->Debug = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Debug enabled\n");
    } else pI128->Debug = FALSE;
    if (xf86ReturnOptValBool(pI128->Options, OPTION_FLATPANEL, FALSE)) {
	pI128->FlatPanel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "FlatPanel forced\n");
    } else pI128->FlatPanel = FALSE;

    /*
     * Set the Chipset and ChipRev.
     */
    from = X_PROBED;
    pI128->Chipset = pI128->PciInfo->chipType;
    pScrn->chipset = (char *)xf86TokenToString(I128Chipsets, pI128->Chipset);
    pI128->ChipRev = pI128->PciInfo->chipRev;

    /*
     * This shouldn't happen because such problems should be caught in
     * I128Probe(), but check it just in case.
     */
    if (pScrn->chipset == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", pI128->Chipset);
	return FALSE;
    }
    if (pI128->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);
    if (pI128->PciInfo->subsysVendor == 0x105D)
        xf86DrvMsg(pScrn->scrnIndex, from, "Subsystem Vendor: \"Number Nine\"\n");
    else if (pI128->PciInfo->subsysVendor == 0x10F0)
        xf86DrvMsg(pScrn->scrnIndex, from, "Subsystem Vendor: \"Peritek\"\n");
    else
        xf86DrvMsg(pScrn->scrnIndex, from, "Subsystem Vendor: \"%x\"\n",
    	    pI128->PciInfo->subsysVendor);

    iobase = (pI128->PciInfo->ioBase[5] & 0xFFFFFF00) + hwp->PIOOffset;
    pI128->RegRec.iobase = iobase;

    pI128->io.rbase_g = inl(iobase)        & 0xFFFFFF00;
    pI128->io.rbase_w = inl(iobase + 0x04) & 0xFFFFFF00;
    pI128->io.rbase_a = inl(iobase + 0x08) & 0xFFFFFF00;
    pI128->io.rbase_b = inl(iobase + 0x0C) & 0xFFFFFF00;
    pI128->io.rbase_i = inl(iobase + 0x10) & 0xFFFFFF00;
    pI128->io.rbase_e = inl(iobase + 0x14) & 0xFFFF8003;
    pI128->io.id =      inl(iobase + 0x18) & /* 0x7FFFFFFF */ 0xFFFFFFFF;
    pI128->io.config1 = inl(iobase + 0x1C) & /* 0xF3333F1F */ 0xFF133706;
    pI128->io.config2 = inl(iobase + 0x20) & 0xC1F70FFF;
    pI128->io.sgram   = inl(iobase + 0x24) & 0xFFFFFFFF;
    pI128->io.soft_sw = inl(iobase + 0x28) & 0x0000FFFF;
    pI128->io.vga_ctl = inl(iobase + 0x30) & 0x0000FFFF;

    if (pI128->Debug)
	I128DumpBaseRegisters(pScrn);

    pI128->RegRec.config1 = pI128->io.config1;
    pI128->RegRec.config2 = pI128->io.config2;
    pI128->RegRec.sgram = pI128->io.sgram;
    if (pI128->Chipset == PCI_CHIP_I128_T2R4)
	pI128->io.sgram = 0x211BF030;
    else
	pI128->io.sgram = 0x21089030;
    /* vga_ctl is saved later */

    /* enable all of the memory mapped windows */

    pI128->io.config1 &= 0x3300001F;
    pI128->io.config1 |= 0x00331F10;
    outl(iobase + 0x1C, pI128->io.config1);

    pI128->MemoryType = I128_MEMORY_UNKNOWN;

    if (pI128->Chipset == PCI_CHIP_I128_T2R4)
	pI128->MemoryType = I128_MEMORY_SGRAM;
    else if (pI128->Chipset == PCI_CHIP_I128_T2R) {
	if ((pI128->io.config2&6) == 2)
		pI128->MemoryType = I128_MEMORY_SGRAM;
	else
		pI128->MemoryType = I128_MEMORY_WRAM;
    } else if (pI128->Chipset == PCI_CHIP_I128_2) {
   	if (((((pciConfigPtr)pI128->PciInfo->thisCard)->pci_command & 0x03)
	    == 0x03) && (pI128->PciInfo->subsysCard == 0x08))
   	   pI128->MemoryType = I128_MEMORY_DRAM;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Memory type %s\n",
	pI128->MemoryType == I128_MEMORY_SGRAM ? "SGRAM" :
	pI128->MemoryType == I128_MEMORY_DRAM ? "DRAM" :
	pI128->MemoryType == I128_MEMORY_WRAM ? "WRAM" : "UNKNOWN");

    pI128->io.config2 &= 0xFF0FFF7F;
    pI128->io.config2 |= 0x00100000;
    if (pI128->MemoryType != I128_MEMORY_SGRAM)
   	pI128->io.config2 |= 0x00400000;
    outl(pI128->RegRec.iobase + 0x20, pI128->io.config2);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Linear framebuffer at 0x%lX\n",
	(unsigned long)pI128->PciInfo->memBase[0]);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "MMIO registers at 0x%lX\n",
	(unsigned long)pI128->PciInfo->ioBase[5]);

    if (xf86RegisterResources(pI128->pEnt->index, NULL, ResExclusive)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"xf86RegisterResources() found resource conflicts\n");
	I128FreeRec(pScrn);
	return FALSE;
    }

    /* HW bpp matches reported bpp */
    pI128->bitsPerPixel = pScrn->bitsPerPixel;
    pI128->depth = pScrn->depth;
    pI128->weight.red =  pScrn->weight.red;
    pI128->weight.green =  pScrn->weight.green;
    pI128->weight.blue =  pScrn->weight.blue;
    pI128->mode = pScrn->modes;

    pScrn->videoRam = I128CountRam(pScrn);
    pI128->MemorySize = pScrn->videoRam;

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "VideoRAM: %d kByte\n",
               pScrn->videoRam);

	
    /*
     * If the driver can do gamma correction, it should call xf86SetGamma()
     * here.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }

    if (!I128MapMem(pScrn))
	return FALSE;

    /*
     * Reset card if it isn't primary one (must be done after config1 is set)
     */
    if (!pI128->Primary)
        I128SoftReset(pScrn);

    if (pI128->Chipset != PCI_CHIP_I128) {
	pI128->ddc1Read = NULL /*I128DDC1Read*/;
	pI128->i2cInit = I128I2CInit;
    }

    /* Load DDC if we have the code to use it */
    /* This gives us DDC1 */
    if (pI128->ddc1Read || pI128->i2cInit) {
        if (xf86LoadSubModule(pScrn, "ddc")) {
          xf86LoaderReqSymLists(ddcSymbols, NULL);
        } else {
          /* ddc module not found, we can do without it */
          pI128->ddc1Read = NULL;

          /* Without DDC, we have no use for the I2C bus */
          pI128->i2cInit = NULL;
        }
    }
    /* - DDC can use I2C bus */
    /* Load I2C if we have the code to use it */
    if (pI128->i2cInit) {
      if ( xf86LoadSubModule(pScrn, "i2c") ) {
        xf86LoaderReqSymLists(i2cSymbols,NULL);
      } else {
        /* i2c module not found, we can do without it */
        pI128->i2cInit = NULL;
        pI128->I2C = NULL;
      }
    }

    /* Read and print the Monitor DDC info */
    mon = I128getDDC(pScrn);
    pScrn->monitor->DDC = mon;

    /* see if we can find a flatpanel */
    if (!pI128->FlatPanel && mon) {
        for (i=0; i<4; i++)
    	    if (mon->det_mon[i].type == DS_NAME) {
		if (strncmp((char *)mon->det_mon[i].section.name,
			    "SGI 1600SW FP", 13) == 0) {
			pI128->FlatPanel = TRUE;
    			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
				"Found FlatPanel via DDC2\n");
		}
		break;
	    }
    }

    pI128->maxClock = 175000;

    switch (pI128->Chipset) {
    	case PCI_CHIP_I128:
	    if (pI128->io.id & 0x0400)       /* 2 banks VRAM   */
		pI128->RamdacType = IBM528_DAC;
	    else
		pI128->RamdacType = TI3025_DAC;
	    break;
    	case PCI_CHIP_I128_2:
	    if (pI128->io.id & 0x0400)       /* 2 banks VRAM   */
		pI128->RamdacType = IBM528_DAC;
	    else
		pI128->RamdacType = IBM526_DAC;
	    pI128->maxClock = 220000;
	    break;
    	case PCI_CHIP_I128_T2R:
	    pI128->RamdacType = IBM526_DAC;
	    pI128->maxClock = 220000;
	    break;
    	case PCI_CHIP_I128_T2R4:
	    pI128->RamdacType = SILVER_HAMMER_DAC;
	    pI128->maxClock = 270000;
	    break;
	default:
    	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Unknown I128 chipset: %d\n",
               		pI128->Chipset);
            return(FALSE);
    }

    if ((pI128->maxClock == 175000) && (pI128->MemorySize == 8192))
	pI128->maxClock = 220000;

    switch(pI128->RamdacType) {
       case TI3025_DAC:
          /* verify that the ramdac is a TVP3025 */

          pI128->mem.rbase_g[INDEX_TI] = TI_ID;				MB;
          if ((pI128->mem.rbase_g[DATA_TI]&0xFF) != TI_VIEWPOINT25_ID) {
    	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Ti3025 Ramdac not found\n");
             return(FALSE);
          }
          ramdac = "TI3025";

          pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
          pI128->mem.rbase_g[DATA_TI] = 0x00;				MB;
          pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;		MB;
          n = pI128->mem.rbase_g[DATA_TI]&0x7f;
          pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
          pI128->mem.rbase_g[DATA_TI] = 0x01;				MB;
          pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;		MB;
          m = pI128->mem.rbase_g[DATA_TI]&0x7f;
          pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
          pI128->mem.rbase_g[DATA_TI] = 0x02;				MB;
          pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;		MB;
          p = pI128->mem.rbase_g[DATA_TI]&0x03;
          pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_DCLK_CONTROL;		MB;
          mdc = pI128->mem.rbase_g[DATA_TI]&0xFF;
          if (mdc&0x08)
	    mdc = (mdc&0x07)*2 + 2;
          else
	    mdc = 1;
          mclk = ((1431818 * ((m+2) * 8)) / (n+2) / (1 << p) / mdc + 50) / 100;

    	  xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Using TI 3025 programmable clock (MCLK %1.3f MHz)\n",
			mclk / 1000.0);
	  pI128->minClock = 20000;
	  pI128->ProgramDAC = I128ProgramTi3025;
	  break;

       case IBM524_DAC:
    	  xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"IBM524 Ramdac not supported\n");
          return(FALSE);

       case IBM526_DAC:
          /* verify that the ramdac is an IBM526 */

          ramdac = "IBM526";
	  tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
	  tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;
          pI128->mem.rbase_g[IDXH_I] = 0;				MB;
          pI128->mem.rbase_g[IDXL_I] = IBMRGB_id;			MB;
	  tmp = pI128->mem.rbase_g[DATA_I] & 0xFF;

          pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;		MB;
	  n = pI128->mem.rbase_g[DATA_I] & 0x1f;
          pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;		MB;
	  m = pI128->mem.rbase_g[DATA_I];
	  df = m>>6;
	  m &= 0x3f;
	  if (n == 0) { m=0; n=1; }
	  mclk = ((2517500 * (m+65)) / n / (8>>df) + 50) / 100;

	  pI128->mem.rbase_g[IDXL_I] = tmpl;				MB;
	  pI128->mem.rbase_g[IDXH_I] = tmph;				MB;
          if (tmp != 2) {
    	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"IBM526 Ramdac not found\n");
             return(FALSE);
          }

    	  xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Using IBM 526 programmable clock (MCLK %1.3f MHz)\n",
			mclk / 1000.0);
	  pI128->minClock = 25000;
	  pI128->ProgramDAC = I128ProgramIBMRGB;
          break;

       case IBM528_DAC:
          /* verify that the ramdac is an IBM528 */

          ramdac = "IBM528";
	  tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
	  tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;
          pI128->mem.rbase_g[IDXH_I] = 0;				MB;
          pI128->mem.rbase_g[IDXL_I] = IBMRGB_id;			MB;
	  tmp = pI128->mem.rbase_g[DATA_I] & 0xFF;

          pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;		MB;
	  n = pI128->mem.rbase_g[DATA_I] & 0x1f;
          pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;		MB;
	  m = pI128->mem.rbase_g[DATA_I] & 0xFF;
	  df = m>>6;
	  m &= 0x3f;
	  if (n == 0) { m=0; n=1; }
	  mclk = ((2517500 * (m+65)) / n / (8>>df) + 50) / 100;

	  pI128->mem.rbase_g[IDXL_I] = tmpl;				MB;
	  pI128->mem.rbase_g[IDXH_I] = tmph;				MB;
          if (tmp != 2) {
    	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"IBM528 Ramdac not found\n");
             return(FALSE);
          }

    	  xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Using IBM 528 programmable clock (MCLK %1.3f MHz)\n",
			mclk / 1000.0);
	  pI128->minClock = 25000;
	  pI128->ProgramDAC = I128ProgramIBMRGB;
          break;

       case SILVER_HAMMER_DAC:
          /* verify that the ramdac is a Silver Hammer */

          ramdac = "SilverHammer";
	  tmph = pI128->mem.rbase_g[IDXH_I] & 0xFF;
	  tmpl = pI128->mem.rbase_g[IDXL_I] & 0xFF;
	  tmp = pI128->mem.rbase_g[DATA_I] & 0xFF;

          pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;		MB;
	  n = pI128->mem.rbase_g[DATA_I] & 0x1f;
          pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;		MB;
	  m = pI128->mem.rbase_g[DATA_I];
	  df = m>>6;
	  m &= 0x3f;
	  if (n == 0) { m=0; n=1; }
	  mclk = ((3750000 * (m+65)) / n / (8>>df) + 50) / 100;

	  pI128->mem.rbase_g[IDXL_I] = tmpl;				MB;
	  pI128->mem.rbase_g[IDXH_I] = tmph;				MB;
          if (pI128->Chipset != PCI_CHIP_I128_T2R4) {
    	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"SilverHammer Ramdac not found\n");
             return(FALSE);
          }

	  if (pI128->mem.rbase_g[CRT_1CON] & 0x00000100) {
             pI128->FlatPanel = TRUE;
    	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Digital flat panel detected\n");
          } else if (pI128->FlatPanel)
    	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			"Digital flat panel forced\n");

    	  xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Using SilverHammer programmable clock (MCLK %1.3f MHz)\n",
			mclk / 1000.0);
	  pI128->minClock = 25000;
	  pI128->ProgramDAC = I128ProgramSilverHammer;
          break;

       default:
    	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Ramdac Unknown\n");
          return(FALSE);
     }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Ramdac Type min/max speed: %s %d/%d MHz\n",
		ramdac, pI128->minClock/1000, pI128->maxClock/1000);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange),1);
    clockRanges->next = NULL;
    clockRanges->minClock = pI128->minClock;
    clockRanges->maxClock = pI128->maxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = TRUE;
    clockRanges->ClockMulFactor = 1;
    clockRanges->ClockDivFactor = 1;

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our I128ValidMode() already takes
     * care of this, we don't worry about setting them here.
     */
    {
	int *linePitches = NULL;
	int minPitch = 256;
	int maxPitch = 2048;	
	int pitchAlignment = 256;	
        
	if (pI128->MemoryType == I128_MEMORY_WRAM)
	   pitchAlignment = (128 * 8);
	pitchAlignment /= pI128->bitsPerPixel;

	i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			      pScrn->display->modes, clockRanges,
			      linePitches, minPitch, maxPitch,
			      pitchAlignment * pI128->bitsPerPixel,
			      128, 2048,
			      pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      pI128->MemorySize,
			      LOOKUP_BEST_REFRESH);

	pI128->displayWidth = pScrn->virtualX;

	if ((pScrn->virtualX % pitchAlignment) != 0)
	   pI128->displayWidth += pitchAlignment -
				  (pScrn->virtualX % pitchAlignment);
    }

    if (i == -1) {
	I128FreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	I128FreeRec(pScrn);
	return FALSE;
    }

    if ((pI128->MemorySize > 4096) &&
        (pI128->MemoryType != I128_MEMORY_DRAM) &&
        (pI128->MemoryType != I128_MEMORY_SGRAM))
        pI128->displayOffset = 0x400000L %
		          (pI128->displayWidth * (pI128->bitsPerPixel/8));
    else
        pI128->displayOffset = 0;

    pI128->MemoryPtr =
	    (pointer)&((char *)pI128->MemoryPtr)[pI128->displayOffset];

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    if (!xf86LoadSubModule(pScrn, "fb")) {
	I128FreeRec(pScrn);
	return FALSE;
    }
    xf86LoaderReqSymLists(fbSymbols, NULL);

    /* Load XAA if needed */
    if (!pI128->NoAccel) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    I128FreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    /* Load ramdac if needed */
    if (pI128->HWCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    I128FreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    I128UnmapMem(pScrn);

    if (pI128->Debug)
    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "PreInit complete\n");
    return TRUE;
}


static Bool
I128GetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an I128Rec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(I128Rec), 1);

    return TRUE;
}

static void
I128FreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}



/*
 * I128SoftReset --
 *
 * Resets drawing engine
 */
static void
I128SoftReset(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Performing soft reset\n");
    pI128->io.config1 |= 0x00000002;
    outl(pI128->RegRec.iobase + 0x1C, pI128->io.config1);
    usleep(10000);
    pI128->io.config1 &= 0xFFFFFFFD;
    outl(pI128->RegRec.iobase + 0x1C, pI128->io.config1);
}

/*
 * I128CountRAM --
 *
 * Counts amount of installed RAM 
 */
static int
I128CountRam(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);
    int SizeFound = 0;

    SizeFound = 0;

    switch(pI128->Chipset) {
    case PCI_CHIP_I128_T2R4:
      /* Use the subsystem ID to determine the memory size */
      switch ((pI128->PciInfo->subsysCard) & 0x0007) {
         case 0x00:      /* 4MB card */
	    SizeFound = 4 * 1024; break;
         case 0x01:      /* 8MB card */
	    SizeFound = 8 * 1024; break;
         case 0x02:      /* 12MB card */
            SizeFound = 12 * 1024; break;
         case 0x03:      /* 16MB card */
	    SizeFound = 16 * 1024; break;
         case 0x04:      /* 20MB card */
	    SizeFound = 20 * 1024; break;
         case 0x05:      /* 24MB card */
	    SizeFound = 24 * 1024; break;
         case 0x06:      /* 28MB card */
	    SizeFound = 28 * 1024; break;
         case 0x07:      /* 32MB card */
	    SizeFound = 32 * 1024; break;
         default: /* Unknown board... */
            break;
      }
      break;
    case PCI_CHIP_I128_T2R:
      switch ((pI128->PciInfo->subsysCard) & 0xFFF7) {
	 case 0x00:	/* 4MB card, no daughtercard */
	    SizeFound = 4 * 1024; break;
	 case 0x01:	/* 4MB card, 4MB daughtercard */
	 case 0x04:	/* 8MB card, no daughtercard */
	    SizeFound = 8 * 1024; break;
	 case 0x02:	/* 4MB card, 8MB daughtercard */
	 case 0x05:	/* 8MB card, 4MB daughtercard */
	    SizeFound = 12 * 1024; break;
	 case 0x06:	/* 8MB card, 8MB daughtercard */
	    SizeFound = 16 * 1024; break;
	 case 0x03:	/* 4MB card, 16 daughtercard */
	    SizeFound = 20 * 1024; break;
	 case 0x07:	/* 8MB card, 16MB daughtercard */
	    SizeFound = 24 * 1024; break;
	 default:
	    break;
      }
    }

    if (SizeFound == 0) {
      SizeFound = 2048;  /* default to 2MB */
      if (pI128->io.config1 & 0x04)    /* 128 bit mode   */
         SizeFound <<= 1;
      if (pI128->io.id & 0x0400)       /* 2 banks VRAM   */
         SizeFound <<= 1;
    }
    return SizeFound;
}


/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
I128MapMem(ScrnInfoPtr pScrn)
{
    I128Ptr pI128;

    pI128 = I128PTR(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Mapping memory\n");

    if (pI128->mem.rbase_g != NULL)
	return TRUE;

    /*
     * Map IO registers to virtual address space
     */ 

    pI128->mem.mw0_ad = (unsigned char *)xf86MapPciMem(pScrn->scrnIndex,
				VIDMEM_FRAMEBUFFER,
				pI128->PciTag,
				pI128->PciInfo->memBase[0] & 0xFFC00000,
				pI128->MemorySize*1024);
    if (pI128->mem.mw0_ad == NULL)
	return FALSE;

    pI128->MemoryPtr = pI128->mem.mw0_ad;

    pI128->mem.rbase_g = (CARD32 *)xf86MapPciMem(pScrn->scrnIndex,
				VIDMEM_MMIO | VIDMEM_MMIO_32BIT,
				pI128->PciTag,
				pI128->PciInfo->memBase[4] & 0xFFFF0000,
				64*1024);
    if (pI128->mem.rbase_g == NULL)
	return FALSE;

    pI128->mem.rbase_w = pI128->mem.rbase_g + ( 8 * 1024)/4;
    pI128->mem.rbase_a = pI128->mem.rbase_g + (16 * 1024)/4;
    pI128->mem.rbase_b = pI128->mem.rbase_g + (24 * 1024)/4;
    pI128->mem.rbase_i = pI128->mem.rbase_g + (32 * 1024)/4;

    return TRUE;
}


/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
I128UnmapMem(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Unmapping memory\n");

    if (pI128->mem.rbase_g == NULL)
	return TRUE;

    /*
     * Unmap IO registers to virtual address space
     */ 
    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pI128->mem.mw0_ad,
	pI128->MemorySize*1024);
    pI128->mem.mw0_ad = NULL;
    pI128->MemoryPtr = NULL;

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pI128->mem.rbase_g, 64*1024);
    pI128->mem.rbase_g = NULL;
    pI128->mem.rbase_w = NULL;
    pI128->mem.rbase_a = NULL;
    pI128->mem.rbase_b = NULL;
    pI128->mem.rbase_i = NULL;

    return TRUE;
}


/*
 * This function saves the video state.
 */
static void
I128Save(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);
    vgaHWPtr vgaHWP = VGAHWPTR(pScrn);

    if (pI128->Primary)
	vgaHWSave(pScrn, &vgaHWP->SavedReg, VGA_SR_ALL);

    I128SaveState(pScrn);
}

/*
 * Restore the initial (text) mode.
 */
static void 
I128Restore(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);
    vgaHWPtr vgaHWP = VGAHWPTR(pScrn);

    I128RestoreState(pScrn);

    if (pI128->Primary) {
	vgaHWProtect(pScrn, TRUE);
	vgaHWRestore(pScrn, &vgaHWP->SavedReg, VGA_SR_ALL);
	vgaHWProtect(pScrn, FALSE);
    }
}

/* Usually mandatory */
Bool
I128SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return I128ModeInit(xf86Screens[scrnIndex], mode);
}


static Bool
I128ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    I128Ptr pI128 = I128PTR(pScrn);

    if (pI128->Debug)
    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "ModeInit start\n");

    /* Initialise the ModeReg values */
    pScrn->vtSema = TRUE;

    if (!I128Init(pScrn, mode))
	return FALSE;

    pI128->ModeSwitched = TRUE;

    pI128->mode = mode;

    if (pI128->Debug)
    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "ModeInit complete\n");

    return TRUE;
}


/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
I128ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    I128Ptr pI128;
    int ret;
    VisualPtr visual;
    unsigned char *FBStart;
    int width, height, displayWidth;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    pI128 = I128PTR(pScrn);

    if (pI128->Debug)
    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "ScreenInit start\n");

    /* Map the I128 memory and MMIO areas */
    if (!I128MapMem(pScrn))
        return FALSE;

    pI128->MemoryPtr =
	    (pointer)&((char *)pI128->MemoryPtr)[pI128->displayOffset];

    /* Save the current state */
    I128Save(pScrn);

    /* Initialise the first mode */
    if (!I128ModeInit(pScrn, pScrn->currentMode))
        return FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    I128SaveScreen(pScreen, SCREEN_SAVER_ON);
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

    if (!miSetVisualTypes(pScrn->depth,
			  miGetDefaultVisualMask(pScrn->depth),
			  pScrn->rgbBits, pScrn->defaultVisual))
	return FALSE;

    if (!miSetPixmapDepths())
	return FALSE;


    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    width = pScrn->virtualX;
    height = pScrn->virtualY;
    displayWidth = pScrn->displayWidth;

    FBStart = pI128->MemoryPtr;

    ret = fbScreenInit(pScreen, FBStart,
			width, height,
			pScrn->xDpi, pScrn->yDpi,
			displayWidth, pScrn->bitsPerPixel);
    if (!ret)
	return FALSE;

    fbPictureInit(pScreen, 0, 0);

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

    xf86SetBlackWhitePixels(pScreen);

    if (!pI128->NoAccel)
	I128DGAInit(pScreen);

    if (!pI128->NoAccel)
	I128AccelInit(pScreen);

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* Initialize software cursor.  
	Must precede creation of the default colormap */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
	Must follow software cursor initialization*/
    if (pI128->HWCursor) { 
	ret = TRUE;
    	switch(pI128->RamdacType) {
	       case TI3025_DAC:
		  ret = I128TIHWCursorInit(pScrn); break;
	       case IBM524_DAC:
	       case IBM526_DAC:
	       case IBM528_DAC:
		  ret = I128IBMHWCursorInit(pScrn); break;
	       case SILVER_HAMMER_DAC:
		  ret = I128IBMHWCursorInit(pScrn); break;
	       default:
		  break;
	    }
	if(!ret)
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		"Hardware cursor initialization failed\n");
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    /* Initialize colormap layer.  
	Must follow initialization of the default colormap */
    if(!xf86HandleColormaps(pScreen, 256, 8, 
	I128LoadPalette, NULL,
	CMAP_PALETTED_TRUECOLOR | CMAP_RELOAD_ON_MODE_SWITCH))	
	return FALSE;

    xf86DPMSInit(pScreen, I128DisplayPowerManagementSet, 0);

    pScrn->memPhysBase = (unsigned long)pI128->MemoryPtr;
    pScrn->fbOffset = 0;

    pScreen->SaveScreen = I128SaveScreen;

    /* Wrap the current CloseScreen function */
    pI128->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = I128CloseScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    if (pI128->Debug)
    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "ScreenInit complete\n");

    /* Done */
    return TRUE;
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
void
I128AdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn;
    int   Base;
    I128Ptr pI128;
#define I128_PAN_MASK 0x01FFFFE0

    pScrn = xf86Screens[scrnIndex];
    pI128 = I128PTR(pScrn);

    if (pI128->ShowCache && y && pScrn->vtSema)
        y += pScrn->virtualY - 1;

    if (x > (pI128->displayWidth - pI128->mode->HDisplay))
        x = pI128->displayWidth - pI128->mode->HDisplay;

    Base = ((y*pI128->displayWidth + x) * (pI128->bitsPerPixel/8));
    pI128->mem.rbase_g[DB_ADR] =
	(Base & I128_PAN_MASK) + pI128->displayOffset; MB;

    /* now warp the cursor after the screen move */
    pI128->AdjustCursorXPos = (Base - (Base & I128_PAN_MASK))
                             / (pI128->bitsPerPixel/8);
}

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 */

/* Mandatory */
static Bool
I128EnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    if (!I128ModeInit(pScrn, pScrn->currentMode))
	return FALSE;
    I128AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return TRUE;
}

/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 */

/* Mandatory */
static void
I128LeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    I128Restore(pScrn);
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
I128CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    I128Ptr pI128 = I128PTR(pScrn);

    if (pScrn->vtSema) {
	I128Restore(pScrn);
	I128UnmapMem(pScrn);
    }
    if (pI128->AccelInfoRec)
	XAADestroyInfoRec(pI128->AccelInfoRec);
    if (pI128->CursorInfoRec)
    	xf86DestroyCursorInfoRec(pI128->CursorInfoRec);
    if (pI128->DGAModes)
    	xfree(pI128->DGAModes);
    pScrn->vtSema = FALSE;

    pScreen->CloseScreen = pI128->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any persistent data structures */

/* Optional */
static void
I128FreeScreen(int scrnIndex, int flags)
{
    /*
     * This only gets called when a screen is being deleted.  It does not
     * get called routinely at the end of a server generation.
     */
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
        vgaHWFreeHWRec(xf86Screens[scrnIndex]);

    I128FreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
I128ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    int lace;

    lace = 1 + ((mode->Flags & V_INTERLACE) != 0);

    if ((mode->CrtcHDisplay <= 2048) &&
	(mode->CrtcHSyncStart <= 4096) && 
	(mode->CrtcHSyncEnd <= 4096) && 
	(mode->CrtcHTotal <= 4096) &&
	(mode->CrtcVDisplay <= 2048 * lace) &&
	(mode->CrtcVSyncStart <= 4096 * lace) &&
	(mode->CrtcVSyncEnd <= 4096 * lace) &&
	(mode->CrtcVTotal <= 4096 * lace)) {
	return(MODE_OK);
    } else {
	return(MODE_BAD);
    }
}


/* Do screen blanking */

/* Mandatory */
static Bool
I128SaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = NULL;
    I128Ptr pI128;
    Bool on;

    if (pScreen != NULL)
	pScrn = xf86Screens[pScreen->myNum];

    on = xf86IsUnblank(mode);

    if ((pScrn != NULL) && pScrn->vtSema) {
        pI128 = I128PTR(pScrn);
        if (on) {
	    pI128->mem.rbase_g[CRT_1CON] |= 0x40;                           MB;
	} else {
	    pI128->mem.rbase_g[CRT_1CON] &= ~0x40;                          MB;
	}
    }
    return TRUE;
}


static const int DDC_SDA_IN_SHIFT = 1;
static const int DDC_SDA_OUT_SHIFT = 2;
static const int DDC_SCL_IN_SHIFT = 3;
static const int DDC_SCL_OUT_SHIFT = 0;

static const int DDC_SDA_IN_MASK = 1 << 1;
static const int DDC_SDA_OUT_MASK = 1 << 2;
static const int DDC_SCL_IN_MASK = 1 << 3;
static const int DDC_SCL_OUT_MASK = 1 << 0;

static const int DDC_MODE_SHIFT = 8;
static const int DDC_MODE_MASK = 3 << 8;
static const int DDC_MODE_DIS  = 0;
static const int DDC_MODE_DDC1 = 1 << 8;
static const int DDC_MODE_DDC2 = 2 << 8;

#if 0
static unsigned int
I128DDC1Read(ScrnInfoPtr pScrn)
{
  I128Ptr pI128 = I128PTR(pScrn);
  unsigned char val;
  unsigned long tmp, ddc;
  IOADDRESS iobase;

  iobase = pI128->RegRec.iobase;
  ddc = inl(iobase + 0x2C);
  if ((ddc & DDC_MODE_MASK) != DDC_MODE_DDC1) {
      outl(iobase + 0x2C, DDC_MODE_DDC1);
      xf86usleep(40);
  }

  /* wait for Vsync */
  do {
      tmp = inl(iobase + 0x2C);
  } while (tmp & 1);
  do {
      tmp = inl(iobase + 0x2C);
  } while (!(tmp & 1));

  /* Get the result */
  tmp = inl(iobase + 0x2C);
  val = tmp & DDC_SDA_IN_MASK;

  if ((ddc & DDC_MODE_MASK) != DDC_MODE_DDC1) {
      outl(iobase + 0x2C, ~DDC_MODE_MASK & ddc);
      xf86usleep(40);
  }

  return val;
}
#endif

static void
I128I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
  I128Ptr pI128 = I128PTR(xf86Screens[b->scrnIndex]);
  unsigned long ddc;
  IOADDRESS iobase;
#if 0
  static int lastclock = -1, lastdata = -1;
#endif

  /* Get the result. */
  iobase = pI128->RegRec.iobase;
  ddc = inl(iobase + 0x2C);

  *clock = (ddc & DDC_SCL_IN_MASK) != 0;
  *data  = (ddc & DDC_SDA_IN_MASK) != 0;

#if 0
  if (pI128->Debug && ((lastclock != *clock) || (lastdata != *data))) {
    xf86DrvMsg(b->scrnIndex, X_INFO, "i2c> c %d d %d\n", *clock, *data);
    lastclock = *clock;
    lastdata = *data;
  }
#endif
}   

static void
I128I2CPutBits(I2CBusPtr b, int clock, int data)
{   
  I128Ptr pI128 = I128PTR(xf86Screens[b->scrnIndex]);
  unsigned char drv, val;
  unsigned long ddc;
  unsigned long tmp;
  IOADDRESS iobase;

  iobase = pI128->RegRec.iobase;
  ddc = inl(iobase + 0x2C);

  val = (clock ? DDC_SCL_IN_MASK : 0) | (data ? DDC_SDA_IN_MASK : 0);
  drv = ((clock) ? DDC_SCL_OUT_MASK : 0) | ((data) ? DDC_SDA_OUT_MASK : 0);

  tmp = (DDC_MODE_MASK & ddc) | val | drv;
  outl(iobase + 0x2C, tmp);
#if 0
  if (pI128->Debug)
    xf86DrvMsg(b->scrnIndex, X_INFO, "i2c> 0x%x\n", tmp);
#endif
}   


static Bool    
I128I2CInit(ScrnInfoPtr pScrn)
{       
    I128Ptr pI128 = I128PTR(pScrn);
    I2CBusPtr I2CPtr;
    IOADDRESS iobase;
    unsigned long soft_sw, ddc;
     
    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;
 
    pI128->I2C = I2CPtr;
 
    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex; 
    I2CPtr->I2CPutBits = I128I2CPutBits; 
    I2CPtr->I2CGetBits = I128I2CGetBits;
    I2CPtr->BitTimeout = 4;
    I2CPtr->ByteTimeout = 4;
    I2CPtr->AcknTimeout = 4;
    I2CPtr->StartTimeout = 4;

    /* soft switch register bits 1,0 control I2C channel */
    iobase = pI128->RegRec.iobase;
    soft_sw = inl(iobase + 0x28);
    soft_sw &= 0xfffffffc;
    soft_sw |= 0x00000001;
    outl(iobase + 0x28, soft_sw);
    xf86usleep(1000);

    /* set default as ddc2 mode */
    ddc = inl(iobase + 0x2C);
    ddc &= ~DDC_MODE_MASK;
    ddc |= DDC_MODE_DDC2;
    outl(iobase + 0x2C, ddc);
    xf86usleep(40);

    if (!xf86I2CBusInit(I2CPtr)) {
        return FALSE;
    }
    return TRUE;
} 


static xf86MonPtr
I128getDDC(ScrnInfoPtr pScrn)
{
  I128Ptr pI128 = I128PTR(pScrn);
  xf86MonPtr MonInfo = NULL;

  /* Initialize I2C bus - used by DDC if available */
  if (pI128->i2cInit) {
    pI128->i2cInit(pScrn);
  }
  /* Read and output monitor info using DDC2 over I2C bus */
  if (pI128->I2C) {
    MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, pI128->I2C);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "I2C Monitor info: %p\n", MonInfo);
    xf86PrintEDID(MonInfo);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "end of I2C Monitor info\n");
  }
  if (!MonInfo) {
    /* Read and output monitor info using DDC1 */
    if (pI128->ddc1Read) {
      MonInfo = xf86DoEDID_DDC1(pScrn->scrnIndex, NULL, pI128->ddc1Read ) ;
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DDC Monitor info: %p\n", MonInfo);
      xf86PrintEDID(MonInfo);
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "end of DDC Monitor info\n");
    }
  }

  if (MonInfo)
    xf86SetDDCproperties(pScrn, MonInfo);

  return MonInfo;
}


/*
 * I128DisplayPowerManagementSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 */
void
I128DisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
			     int flags)
{
    I128Ptr pI128 = I128PTR(pScrn);
    CARD32 snc;

    if (pI128->Debug)
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128DisplayPowerManagementSet: %d\n", PowerManagementMode);

    if (pI128->RamdacType == TI3025_DAC) return;

    snc = pI128->mem.rbase_g[CRT_1CON];

    switch (PowerManagementMode)
    {
    case DPMSModeOn:
	/* HSync: On, VSync: On */
	snc |= 0x30;
	break;
    case DPMSModeStandby:
	/* HSync: Off, VSync: On */
	snc = (snc & ~0x10) | 0x20;
	break;
    case DPMSModeSuspend:
	/* HSync: On, VSync: Off */
	snc = (snc & ~0x20) | 0x10;
	break;
    case DPMSModeOff:
	/* HSync: Off, VSync: Off */
	snc &= ~0x30;
	break;
    }
    pI128->mem.rbase_g[CRT_1CON] = snc;					MB;
}

void
I128DumpBaseRegisters(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"  PCI Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    MW0_AD    0x%08x  addr 0x%08x  %spre-fetchable\n",
	    pI128->PciInfo->memBase[0],
	    pI128->PciInfo->memBase[0] & 0xFFC00000,
	    pI128->PciInfo->memBase[0] & 0x8 ? "" : "not-");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    MW1_AD    0x%08x  addr 0x%08x  %spre-fetchable\n",
	    pI128->PciInfo->memBase[1],
	    pI128->PciInfo->memBase[1] & 0xFFC00000,
	    pI128->PciInfo->memBase[1] & 0x8 ? "" : "not-");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    XYW_AD(A) 0x%08x  addr 0x%08x\n",
	    pI128->PciInfo->memBase[2],
	    pI128->PciInfo->memBase[2] & 0xFFC00000);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    XYW_AD(B) 0x%08x  addr 0x%08x\n",
	    pI128->PciInfo->memBase[3],
	    pI128->PciInfo->memBase[3] & 0xFFC00000);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_G   0x%08x  addr 0x%08x\n",
	    pI128->PciInfo->memBase[4],
	    pI128->PciInfo->memBase[4] & 0xFFFF0000);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    IO        0x%08x  addr 0x%08x\n",
	    pI128->PciInfo->ioBase[5],
	    pI128->PciInfo->ioBase[5] & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    SSC       0x%08x  addr 0x%08x\n",
    	    pI128->PciInfo->subsysCard,
    	    pI128->PciInfo->subsysCard & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    SSV       0x%08x  addr 0x%08x\n",
    	    pI128->PciInfo->subsysVendor,
    	    pI128->PciInfo->subsysVendor & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_E   0x%08x  addr 0x%08x  %sdecode-enabled\n\n",
    	    pI128->PciInfo->biosBase,
	    pI128->PciInfo->biosBase & 0xFFFF8000,
	    pI128->PciInfo->biosBase & 0x1 ? "" : "not-");

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    PCICMDST  0x%08x       0x%08x\n",
   	    ((pciConfigPtr)pI128->PciInfo->thisCard)->pci_command,
   	    ((pciConfigPtr)pI128->PciInfo->thisCard)->pci_status);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"  IO Mapped Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_G   0x%08x  addr 0x%08x\n",
	    pI128->io.rbase_g, pI128->io.rbase_g & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_W   0x%08x  addr 0x%08x\n",
	    pI128->io.rbase_w, pI128->io.rbase_w & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_A   0x%08x  addr 0x%08x\n",
	    pI128->io.rbase_a, pI128->io.rbase_a & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_B   0x%08x  addr 0x%08x\n",
	    pI128->io.rbase_b, pI128->io.rbase_b & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_I   0x%08x  addr 0x%08x\n",
	    pI128->io.rbase_i, pI128->io.rbase_i & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    RBASE_E   0x%08x  addr 0x%08x  size 0x%x\n\n",
	    pI128->io.rbase_e, pI128->io.rbase_e & 0xFFFF8000,
	    pI128->io.rbase_e & 0x7);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"  Miscellaneous IO Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    ID        0x%08x\n", pI128->io.id);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    CONFIG1   0x%08x\n", pI128->io.config1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    CONFIG2   0x%08x\n", pI128->io.config2);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    SGRAM     0x%08x\n", pI128->io.sgram);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    SOFT_SW   0x%08x\n", pI128->io.soft_sw);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	"    VGA_CTL   0x%08x\n", pI128->io.vga_ctl);
}


void
I128DumpActiveRegisters(ScrnInfoPtr pScrn)
{
    I128Ptr pI128 = I128PTR(pScrn);
    IOADDRESS iobase;
    unsigned long rbase_g, rbase_w, rbase_a, rbase_b, rbase_i, rbase_e;
    unsigned long id, config1, config2, sgram, soft_sw, ddc, vga_ctl;
    volatile CARD32 *vrba, *vrbg, *vrbw;

    vrba = pI128->mem.rbase_a;
    vrbg = pI128->mem.rbase_g;
    vrbw = pI128->mem.rbase_w;

    iobase = pI128->RegRec.iobase;
    rbase_g = inl(iobase);
    rbase_w = inl(iobase + 0x04);
    rbase_a = inl(iobase + 0x08);
    rbase_b = inl(iobase + 0x0C);
    rbase_i = inl(iobase + 0x10);
    rbase_e = inl(iobase + 0x14);
    id = inl(iobase + 0x18);
    config1 = inl(iobase + 0x1C);
    config2 = inl(iobase + 0x20);
    sgram = inl(iobase + 0x24);
    soft_sw = inl(iobase + 0x28);
    ddc = inl(iobase + 0x2C);
    vga_ctl = inl(iobase + 0x30);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "IO Mapped Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RBASE_G   0x%08x  addr 0x%08x\n",
       		rbase_g, rbase_g & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RBASE_W   0x%08x  addr 0x%08x\n",
       		rbase_w, rbase_w & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RBASE_A   0x%08x  addr 0x%08x\n",
       		rbase_a, rbase_a & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RBASE_B   0x%08x  addr 0x%08x\n",
       		rbase_b, rbase_b & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RBASE_I   0x%08x  addr 0x%08x\n",
       		rbase_i, rbase_i & 0xFFFFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RBASE_E   0x%08x  addr 0x%08x  size 0x%x\n",
       		rbase_e, rbase_e & 0xFFFF8000, rbase_e & 0x7);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Miscellaneous IO Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  ID        0x%08x\n", id);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    REV  %d  HBT %d  BASE0 %d  VDEN %d  VB %d  BASE1 %d  BASE2 %d  DS %d\n",
    	id&7, (id>>3)&3, (id>>6)&3, (id>>8)&3, (id>>10)&1,
    	(id>>11)&3, (id>>13)&3, (id>>15)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    DDEN %d  DB  %d  BASE3 %d  BASER %d  MDEN %d  TR %d  VS    %d\n",
    	(id>>16)&3, (id>>18)&1, (id>>19)&3, (id>>21)&7, (id>>24)&3,
	(id>>26)&1, (id>>27)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    CLASS %d  EE %d\n",
	(id>>28)&3, (id>>30)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CONFIG1   0x%08x\n", config1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    VE %d  SFT_RST %d  ONE28 %d  VS %d\n",
    	config1&1, (config1>>1)&1,
    	(config1>>2)&1, (config1>>3)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    G %d  W %d  A %d  B %d  I %d  E %d  W0 %d  W1 %d  XA %d  XB %d\n",
    	(config1>>8)&1, (config1>>9)&1,
    	(config1>>10)&1, (config1>>11)&1,
    	(config1>>12)&1, (config1>>13)&1,
    	(config1>>16)&1, (config1>>17)&1,
    	(config1>>20)&1, (config1>>21)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    HBPRI %d  VBPRI %d  DE1PRI %d  ISAPRI %d\n",
    	(config1>>24)&3, (config1>>26)&3,
    	(config1>>28)&3, (config1>>30)&3);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CONFIG2   0x%08x\n", config2);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    DWT %x  EWS %x  DWS %x  MC %x  FBB %d  IOB %d  FST %d  CNT %d  DEC %d\n",
    	config2&0x3, (config2>>8)&0xF,
    	(config2>>16)&0x7, (config2>>20)&0xF,
    	(config2>>24)&1, (config2>>25)&1,
    	(config2>>26)&1, (config2>>27)&1,
    	(config2>>28)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    PRE %d  RVD %d  SDAC %d\n",
	(config2>>29)&1, (config2>>30)&1, (config2>>31)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  SGRAM     0x%08x\n", sgram);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  SOFT_SW   0x%08x\n", soft_sw);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DDC       0x%08x\n", ddc);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  VGA_CTL   0x%08x\n", vga_ctl);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    MEMMUX %d  VGADEC %d  VIDMUX %d  ENA %d  BUFSEL %d  STR %d\n",
    	vga_ctl&1, (vga_ctl>>1)&1,
    	(vga_ctl>>2)&1, (vga_ctl>>3)&1,
    	(vga_ctl>>4)&1, (vga_ctl>>5)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    3C2 %d  DACDEC %d  MSK 0x%02x\n",
    	(vga_ctl>>6)&1,
    	(vga_ctl>>7)&1,
    	(vga_ctl>>8)&0xff);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  INT_VCNT 0x%08x  (%d)\n",
    	vrbg[0x20/4]&0xFF, vrbg[0x20/4]&0xFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  INT_HCNT 0x%08x  (%d)\n",
    	vrbg[0x24/4]&0xFFF, vrbg[0x24/4]&0xFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DB_ADR   0x%08x  (%d)\n",
    	vrbg[0x28/4]&0x01FFFFF0, vrbg[0x28/4]&0x01FFFFF0);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DB_PTCH  0x%08x  (%d)\n",
    	vrbg[0x2C/4]&0xFFF0, vrbg[0x2C/4]&0xFFF0);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_HAC  0x%08x  (%d)\n",
    	vrbg[0x30/4]&0x3FFF, vrbg[0x30/4]&0x3FFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_HBL  0x%08x  (%d)\n",
    	vrbg[0x34/4]&0x3FFF, vrbg[0x34/4]&0x3FFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_HFP  0x%08x  (%d)\n",
    	vrbg[0x38/4]&0x3FFF, vrbg[0x38/4]&0x3FFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_HS   0x%08x  (%d)\n",
    	vrbg[0x3C/4]&0x3FFF, vrbg[0x3C/4]&0x3FFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_VAC  0x%08x  (%d)\n",
    	vrbg[0x40/4]&0xFFF, vrbg[0x40/4]&0xFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_VBL  0x%08x  (%d)\n",
    	vrbg[0x44/4]&0xFFF, vrbg[0x44/4]&0xFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_VFP  0x%08x  (%d)\n",
    	vrbg[0x48/4]&0xFFF, vrbg[0x48/4]&0xFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_VS   0x%08x  (%d)\n",
    	vrbg[0x4C/4]&0xFFF, vrbg[0x4C/4]&0xFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_LCNT 0x%08x\n", vrbg[0x50/4]&0x0FFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_ZOOM 0x%08x\n", vrbg[0x54/4]&0xF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_1CON 0x%08x  PH %d  PV %d  CS %d INL %d H/VSE %d/%d VE %d BTS %d\n",
        vrbg[0x58/4],
    	vrbg[0x58/4]&1, (vrbg[0x58/4]>>1)&1, (vrbg[0x58/4]>>2)&1,
    	(vrbg[0x58/4]>>3)&1, (vrbg[0x58/4]>>4)&1, (vrbg[0x58/4]>>5)&1,
    	(vrbg[0x58/4]>>6)&1, (vrbg[0x58/4]>>8)&1);
    
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CRT_2CON 0x%08x  MEM %d  RFR %d  TRD %d  SPL %d\n",
        vrbg[0x5C/4],
    	vrbg[0x5C/4]&7, (vrbg[0x5C/4]>>8)&1,
    	(vrbg[0x5C/4]>>16)&7, (vrbg[0x5C/4]>>24)&1);
    
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Memory Windows Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW0_CTRL 0x%08x\n", vrbw[0x00]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    AMV %d  MP %d  AMD %d  SEN %d  BSY %d  MDM %d  DEN %d  PSZ %d\n",
    	(vrbw[0x00]>>1)&1, (vrbw[0x00]>>2)&1, (vrbw[0x00]>>3)&1,
    	(vrbw[0x00]>>4)&3, (vrbw[0x00]>>8)&1, (vrbw[0x00]>>21)&3,
    	(vrbw[0x00]>>24)&3, (vrbw[0x00]>>26)&3);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "M/V/DSE %d/%d/%d\n",
	(vrbw[0x00]>>28)&1, (vrbw[0x00]>>29)&1, (vrbw[0x00]>>30)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW0_AD    0x%08x  MW0_SZ    0x%08x  MW0_PGE   0x%08x\n",
    	vrbw[0x04/4]&0xFFFFF000, vrbw[0x08/4]&0x0000000F,
    	vrbw[0x0C/4]&0x000F001F);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW0_ORG10 0x%08x  MW0_ORG14 0x%08x  MW0_MSRC  0x%08x\n",
    	vrbw[0x10/4]&0x01FFF000, vrbw[0x14/4]&0x01FFF000,
    	vrbw[0x18/4]&0x00FFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW0_WKEY  0x%08x  MW0_KYDAT 0x%08x  MW0_MASK  0x%08x\n",
    	vrbw[0x1C/4], vrbw[0x20/4]&0x000F000F, vrbw[0x24/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW1_CTRL 0x%08x\n", vrbw[0x28/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    AMV %d  MP %d  AMD %d  SEN %d  BSY %d  MDM %d  DEN %d  PSZ %d\n",
    	(vrbw[0x28/4]>>1)&1, (vrbw[0x28/4]>>2)&1, (vrbw[0x28/4]>>3)&1,
    	(vrbw[0x28/4]>>4)&3, (vrbw[0x28/4]>>8)&1, (vrbw[0x28/4]>>21)&3,
    	(vrbw[0x28/4]>>24)&3, (vrbw[0x28/4]>>26)&3);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "M/V/DSE %d/%d/%d\n",
	(vrbw[0x28/4]>>28)&1, (vrbw[0x28/4]>>29)&1, (vrbw[0x28/4]>>30)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW1_AD    0x%08x  MW1_SZ    0x%08x  MW1_PGE   0x%08x\n",
    	vrbw[0x2C/4]&0xFFFFF000, vrbw[0x30/4]&0x0000000F,
    	vrbw[0x34/4]&0x000F001F);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW1_ORG10 0x%08x  MW1_ORG14 0x%08x  MW1_MSRC  0x%08x\n",
    	vrbw[0x38/4]&0x01FFF000, vrbw[0x3c/4]&0x01FFF000,
    	vrbw[0x40/4]&0x00FFFF00);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MW1_WKEY  0x%08x  MW1_KYDAT 0x%08x  MW1_MASK  0x%08x\n",
    	vrbw[0x44/4], vrbw[0x48/4]&0x000F000F, vrbw[0x4C/4]);
    
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Engine A Registers\n");
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  INTP      0x%08x\n", vrba[0x00/4]&0x03);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  INTM      0x%08x\n", vrba[0x04/4]&0x03);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  FLOW      0x%08x\n", vrba[0x08/4]&0x0F);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  BUSY      0x%08x\n", vrba[0x0C/4]&0x01);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XYW_AD    0x%08x  SIZE 0x%x  ADDR 0x%x\n",
    	vrba[0x10/4]&0xFFFFFF00, (vrba[0x10/4]>>8)&0x0F,
    	vrba[0x10/4]&0xFFFFF000);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  ZCTL      0x%08x\n", vrba[0x18/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  BUF_CTRL  0x%08x\n", vrba[0x20/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    AMV %d  MP %d  AMD %d  SEN %d  DEN %d  DSE %d  VSE %d  MSE %d\n",
    	(vrba[0x20/4]>>1)&1, (vrba[0x20/4]>>2)&1, (vrba[0x20/4]>>3)&1,
    	(vrba[0x20/4]>>8)&3, (vrba[0x20/4]>>10)&3, (vrba[0x20/4]>>12)&1,
    	(vrba[0x20/4]>>13)&1, (vrba[0x20/4]>>14)&1);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    PS %d  MDM %d  PSIZE %d  CRCO %d\n",
	(vrba[0x20/4]>>16)&0x1F,
    	(vrba[0x20/4]>>21)&3, (vrba[0x20/4]>>24)&3, (vrba[0x20/4]>>30)&3);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_PGE    0x%08x  DVPGE 0x%x  MPGE 0x%x\n",
    	vrba[0x24/4]&0x000F001F, (vrba[0x24/4]>>8)&0x01F,
    	(vrba[0x24/4]&0x000F0000)>>16);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_SORG   0x%08x\n", vrba[0x28/4]&0x0FFFFFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_DORG   0x%08x\n", vrba[0x2C/4]&0x0FFFFFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_MSRC   0x%08x\n", vrba[0x30/4]&0x03FFFFF0);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_WKEY   0x%08x\n", vrba[0x38/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_ZPTCH  0x%08x\n", vrba[0x3C/4]&0x000FFFF0);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_SPTCH  0x%08x\n", vrba[0x40/4]&0x0000FFF0);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  DE_DPTCH  0x%08x\n", vrba[0x44/4]&0x0000FFF0);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD       0x%08x\n", vrba[0x48/4]&0x7FFFFFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    OPC 0x%02x  ROP 0x%02x  STYLE 0x%02x  CLP 0x%x  PATRN 0x%x  HDF %d\n",
    	vrba[0x48/4]&0xFF, (vrba[0x48/4]>>8)&0xFF, (vrba[0x48/4]>>16)&0x1F,
    	(vrba[0x48/4]>>21)&7, (vrba[0x48/4]>>24)&0xF, (vrba[0x48/4]>>28)&7);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_SHADE 0x%02x\n", vrba[0x4C/4]&0xFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_OPC   0x%02x\n", vrba[0x50/4]&0xFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_ROP   0x%02x\n", vrba[0x54/4]&0xFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_STYLE 0x%02x\n", vrba[0x58/4]&0x1F);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_PATRN 0x%02x\n", vrba[0x5C/4]&0x0F);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_CLP   0x%02x\n", vrba[0x60/4]&0x07);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CMD_HDF   0x%02x\n", vrba[0x64/4]&0x07);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  FORE      0x%08x\n", vrba[0x68/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  BACK      0x%08x\n", vrba[0x6C/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  MASK      0x%08x\n", vrba[0x70/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  RMSK      0x%08x\n", vrba[0x74/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  LPAT      0x%08x\n", vrba[0x78/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  PCTRL     0x%08x\n", vrba[0x7C/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "    PLEN 0x%02d  PSCL 0x%02d  SPTR 0x%02d  SSCL 0x%x  STATE 0x%04x\n",
    	vrba[0x7C/4]&0x1F, (vrba[0x7C/4]>>5)&7, (vrba[0x7C/4]>>8)&0x1F,
    	(vrba[0x7C/4]>>13)&7, (vrba[0x7C/4]>>16)&0xFFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CLPTL     0x%08x  CLPTLY 0x%04x  CLPTLX 0x%04x\n",
    	vrba[0x80/4], vrba[0x80/4]&0xFFFF, (vrba[0x80/4]>>16)&0xFFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  CLPBR     0x%08x  CLPBRY 0x%04x  CLPBRX 0x%04x\n",
    	vrba[0x84/4], vrba[0x84/4]&0xFFFF, (vrba[0x84/4]>>16)&0xFFFF);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY0       0x%08x\n", vrba[0x88/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY1       0x%08x\n", vrba[0x8C/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY2       0x%08x\n", vrba[0x90/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY3       0x%08x\n", vrba[0x94/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY4       0x%08x\n", vrba[0x98/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY5       0x%08x\n", vrba[0x9C/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY6       0x%08x\n", vrba[0xA0/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY7       0x%08x\n", vrba[0xA4/4]);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "  XY8       0x%08x\n", vrba[0xA8/4]);
    if (pI128->RamdacType != TI3025_DAC)
	I128DumpIBMDACRegisters(pScrn, vrbg);
}

static unsigned char ibm52Xmask[0xA0] = {
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   /* 00-07 */
0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,   /* 08-0F */
0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00,   /* 10-17 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 18-1F */
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,   /* 20-27 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 28-2F */
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,   /* 30-37 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 38-3F */
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   /* 40-47 */
0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 48-4F */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 58-5F */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 58-5F */
0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 60-67 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 68-6F */
0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,   /* 70-77 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 78-7F */
0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,   /* 80-87 */
0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,   /* 88-8F */
0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,   /* 90-97 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 98-9F */
};

void
I128DumpIBMDACRegisters(ScrnInfoPtr pScrn, volatile CARD32 *vrbg)
{
	unsigned char ibmr[0x100];
	char buf[128], tbuf[10];
	int i;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "IBM52X Registers\n");

	vrbg[IDXH_I] = 0x00;
	vrbg[IDXH_I] = 0x00;
	vrbg[IDXCTL_I] = 0x01;
	buf[0] = '\0';

	for (i=0; i<0xA0; i++) {
		if ((i%16 == 0) && (i != 0)) {
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "%s\n", buf);
			buf[0] = '\0';
		}
		if (ibm52Xmask[i] == 0x00) {
			strcat(buf, " ..");
		} else {
			vrbg[IDXL_I] = i;
			ibmr[i] = vrbg[DATA_I] & 0xFF;
			ibmr[i] &= ibm52Xmask[i];
			sprintf(tbuf, " %02x", ibmr[i]);
			strcat(buf, tbuf);
		}
	}
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "%s\n", buf);
}

