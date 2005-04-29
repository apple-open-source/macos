/*
 * Driver for CL-GD5480.
 * Itai Nahshon.
 *
 * Support for the CL-GD7548: David Monniaux
 *
 * This is mainly a cut & paste from the MGA driver.
 * Original autors and contributors list include:
 *	Radoslaw Kapitan, Andrew Vanderstock, Dirk Hohndel,
 *	David Dawes, Andrew E. Mileski, Leonard N. Zubkoff,
 *	Guy DESBIEF
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cirrus/cir_driver.c,v 1.69 2003/06/18 16:17:38 eich Exp $ */

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "xf86Resources.h"

/* All drivers need this */
#include "xf86_ansic.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

#include "cir.h"
#include "alp.h"
#include "lg.h"

#include "vbe.h"

/*
 * Forward definitions for the functions that make up the driver.
 */

/* Mandatory functions */

static const OptionInfoRec *	CIRAvailableOptions(int chipid, int busid);
static void	CIRIdentify(int flags);
static Bool	CIRProbe(DriverPtr drv, int flags);

static Bool lg_loaded = FALSE;
static Bool alp_loaded = FALSE;

#define VERSION 4000
#define CIR_NAME "CIRRUS"
#define CIR_DRIVER_NAME "cirrus"
#define CIR_MAJOR_VERSION 1
#define CIR_MINOR_VERSION 0
#define CIR_PATCHLEVEL 0

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added to the driver list by
 * the Module Setup function in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec CIRRUS = {
	VERSION,
	CIR_DRIVER_NAME,
	CIRIdentify,
	CIRProbe,
	CIRAvailableOptions,
	NULL,
	0
};

/* Supported chipsets */
SymTabRec CIRChipsets[] = {
	{ PCI_CHIP_GD5430,		"CLGD5430" },
	{ PCI_CHIP_GD5434_4,	"CLGD5434-4" },
	{ PCI_CHIP_GD5434_8,	"CLGD5434-8" },
	{ PCI_CHIP_GD5436,		"CLGD5436" },
/*  { PCI_CHIP_GD5440,		"CLGD5440" }, */
	{ PCI_CHIP_GD5446,		"CLGD5446" },
	{ PCI_CHIP_GD5480,		"CLGD5480" },
	{ PCI_CHIP_GD5462,		"CL-GD5462" },
	{ PCI_CHIP_GD5464,		"CL-GD5464" },
	{ PCI_CHIP_GD5464BD,	"CL-GD5464BD" },
	{ PCI_CHIP_GD5465,		"CL-GD5465" },
	{ PCI_CHIP_GD7548,              "CL-GD7548" },
	{-1,					NULL }
};

/* List of PCI chipset names */
PciChipsets CIRPciChipsets[] = {
	{ PCI_CHIP_GD5430,	PCI_CHIP_GD5430,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5434_4,PCI_CHIP_GD5434_4,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5434_8,PCI_CHIP_GD5434_8,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5436,	PCI_CHIP_GD5436,	RES_SHARED_VGA },
/*  { PCI_CHIP_GD5440,	PCI_CHIP_GD5440,	RES_SHARED_VGA }, */
	{ PCI_CHIP_GD5446,	PCI_CHIP_GD5446,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5480,	PCI_CHIP_GD5480,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5462,	PCI_CHIP_GD5462,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5464,	PCI_CHIP_GD5464,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5464BD,PCI_CHIP_GD5464BD,	RES_SHARED_VGA },
	{ PCI_CHIP_GD5465,	PCI_CHIP_GD5465,	RES_SHARED_VGA },
	{ PCI_CHIP_GD7548,	PCI_CHIP_GD7548,	RES_SHARED_VGA },
	{ -1,				-1,					RES_UNDEFINED}
};

/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose of this is to avoid warnings about
 * unresolved symbols that are not required.
 */

static const char *alpSymbols[] = {
	"AlpAvailableOptions",
	"AlpProbe",
	NULL
};
static const char *lgSymbols[] = {
	"LgAvailableOptions",
	"LgProbe",
	NULL
};

static const char *vbeSymbols[] = {
	"VBEInit",
	"vbeDoEDID",
	"vbeFree",
	NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(cirSetup);

static XF86ModuleVersionInfo cirVersRec =
{
	"cirrus",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	CIR_MAJOR_VERSION, CIR_MINOR_VERSION, CIR_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,			/* This is a video driver */
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

/*
 * This is the module init data.
 * Its name has to be the driver name followed by ModuleData.
 */
XF86ModuleData cirrusModuleData = { &cirVersRec, cirSetup, NULL };

static pointer
cirSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	/* This module should be loaded only once, but check to be sure. */

	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&CIRRUS, module, 0);

		LoaderRefSymLists(alpSymbols, lgSymbols, vbeSymbols, NULL);
		return (pointer)1;
	}
	if (errmaj) *errmaj = LDR_ONCEONLY;
	return NULL;
}

#endif /* XFree86LOADER */

/* Mandatory */
static void
CIRIdentify(int flags)
{
	xf86PrintChipsets(CIR_NAME, "driver for Cirrus chipsets", CIRChipsets);
}

static const OptionInfoRec *
CIRAvailableOptions(int chipid, int busid)
{
	int chip = chipid & 0xffff;

        switch (chip)
	{
	case PCI_CHIP_GD5462:
	case PCI_CHIP_GD5464:
	case PCI_CHIP_GD5464BD:
	case PCI_CHIP_GD5465:
		if (lg_loaded)
			return LgAvailableOptions(chipid);
		else
			return NULL;

	default:
		if (alp_loaded)
			return AlpAvailableOptions(chipid);
		else
			return NULL;
	}
}

static Bool
CIRProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    pciVideoPtr pPci;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;
    ScrnInfoPtr (*subProbe)(int entity);
    ScrnInfoPtr pScrn;

#ifdef CIR_DEBUG
    ErrorF("CirProbe\n");
#endif
  
    /*
     * For PROBE_DETECT, make sure both sub-modules are loaded before
     * calling xf86MatchPciInstances(), because the AvailableOptions()
     * functions may be called before xf86MatchPciInstances() returns.
     */
    
    if (flags & PROBE_DETECT) {
	if (!lg_loaded) {
	    if (xf86LoadDrvSubModule(drv, "cirrus_laguna")) {
		xf86LoaderReqSymLists(lgSymbols, NULL);
		lg_loaded = TRUE;
	    }
	}
	if (!alp_loaded) {
	    if (xf86LoadDrvSubModule(drv, "cirrus_alpine")) {
		xf86LoaderReqSymLists(alpSymbols, NULL);
		alp_loaded = TRUE;
	    }
	}
    }

    if ((numDevSections = xf86MatchDevice(CIR_DRIVER_NAME,
					  &devSections)) <= 0) {
	return FALSE;
    }
    
    if (xf86GetPciVideoInfo() == NULL) {
	/*
	 * We won't let anything in the config file override finding no
	 * PCI video cards at all.  This seems reasonable now, but we'll see.
	 */
	return FALSE;
    }
  
    numUsed = xf86MatchPciInstances(CIR_NAME, PCI_VENDOR_CIRRUS,
				    CIRChipsets, CIRPciChipsets, devSections,
 				    numDevSections, drv, &usedChips);
    /* Free it since we don't need that list after this */
    xfree(devSections);
    if (numUsed <= 0)
 	return FALSE;
    if (flags & PROBE_DETECT)
 	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
 	/* The Laguna family of chips is so different from the Alpine
 	   family that we won't share even the highest-level of
 	   functions.  But, the Laguna chips /are/ Cirrus chips, so
 	   they should be handled in this driver (as opposed to their
 	   own driver). */
	pPci = xf86GetPciInfoForEntity(usedChips[i]);
 	if (pPci && (pPci->chipType == PCI_CHIP_GD5462 ||
 	    pPci->chipType == PCI_CHIP_GD5464 ||
 	    pPci->chipType == PCI_CHIP_GD5464BD ||
 	    pPci->chipType == PCI_CHIP_GD5465)) {
 	    
 	    if (!lg_loaded) {
 		if (!xf86LoadDrvSubModule(drv, "cirrus_laguna")) 
		    continue;
 		xf86LoaderReqSymLists(lgSymbols, NULL);
 		lg_loaded = TRUE;
 	    }
 	    subProbe = LgProbe;
 	} else {
 	    if (!alp_loaded) {
 		if (!xf86LoadDrvSubModule(drv, "cirrus_alpine")) 
 		    continue;
 		xf86LoaderReqSymLists(alpSymbols, NULL);
 		alp_loaded = TRUE;
 	    }
 	    subProbe = AlpProbe;
 	}
 	pScrn = NULL;
 	
 	if ((pScrn = subProbe(usedChips[i]))) {
 	    foundScreen = TRUE;
 	    /* Fill in what we can of the ScrnInfoRec */
 	    pScrn->driverVersion = VERSION;
 	    pScrn->driverName	 = CIR_DRIVER_NAME;
 	    pScrn->name		 = CIR_NAME;
 	    pScrn->Probe	 = NULL;
 	}
    }
    xfree(usedChips);
     
    return foundScreen;
}

/*
 * Map the framebuffer and MMIO memory.
 */

Bool
CirMapMem(CirPtr pCir, int scrnIndex)
{
	int mmioFlags;

#ifdef CIR_DEBUG
	ErrorF("CirMapMem\n");
#endif

	/*
	 * Map the frame buffer.
	 */
	if (pCir->FbMapSize) {
	    
	    pCir->FbBase = xf86MapPciMem(scrnIndex, VIDMEM_FRAMEBUFFER,
					 pCir->PciTag, pCir->FbAddress,
					 pCir->FbMapSize);
	    if (pCir->FbBase == NULL)
		return FALSE;
	}
	
#ifdef CIR_DEBUG
	ErrorF("CirMapMem pCir->FbBase=0x%08x\n", pCir->FbBase);
#endif

	/*
	 * Map IO registers to virtual address space
	 */
	if (pCir->IOAddress == 0) {
		pCir->IOBase = NULL; /* Until we are ready to use MMIO */
	} else {
		mmioFlags = VIDMEM_MMIO;
		/*
		 * For Alpha, we need to map SPARSE memory, since we need
		 * byte/short access.  Common-level will automatically use
		 * sparse mapping for MMIO.
		 */
		pCir->IOBase =
		  xf86MapPciMem(scrnIndex, mmioFlags, pCir->PciTag,
		       	        pCir->IOAddress, pCir->IoMapSize);
		if (pCir->IOBase == NULL)
			return FALSE;
	}

#ifdef CIR_DEBUG
	ErrorF("CirMapMem pCir->IOBase=0x%08x [length=%08x] from PCI=%08x\n",
	       pCir->IOBase, pCir->IoMapSize, pCir->IOAddress);
	ErrorF("MMIO[GR31] = %2X\n", (int)
	       ((volatile unsigned char*) pCir->IOBase)[0x40]);
#endif

	return TRUE;
}


/*
 * Unmap the framebuffer and MMIO memory.
 */

Bool
CirUnmapMem(CirPtr pCir, int scrnIndex)
{
#ifdef CIR_DEBUG
	ErrorF("CirUnmapMem\n");
#endif

	if (pCir->IOBase != NULL) {
		/*
		 * Unmap IO registers to virtual address space
		 */
		xf86UnMapVidMem(scrnIndex, (pointer)pCir->IOBase, pCir->IoMapSize);
		pCir->IOBase = NULL;
	}

	xf86UnMapVidMem(scrnIndex, (pointer)pCir->FbBase, pCir->FbMapSize);
	pCir->FbBase = NULL;
	return TRUE;
}

void
cirProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86LoaderReqSymLists(vbeSymbols,NULL);
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}
