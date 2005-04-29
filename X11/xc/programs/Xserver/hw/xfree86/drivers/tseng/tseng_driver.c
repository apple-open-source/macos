/*
 * $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_driver.c,v 1.98 2003/11/03 05:11:44 tsi Exp $ 
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Roell not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Roell makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS ROELL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS ROELL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Thomas Roell, roell@informatik.tu-muenchen.de
 *          ET6000 and ET4000W32 16/24/32 bpp and acceleration support by Koen Gadeyne
 *
 * Large parts rewritten for XFree86 4.0 by Koen Gadeyne.
 */
/* $XConsortium: et4_driver.c /main/27 1996/10/28 04:48:15 kaleb $ */





/*** Generic includes ***/

#include "tseng.h"		       /* this includes most of the generic ones as well */
#include "tseng_acl.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

#include "fb.h"

#include "xf86RAC.h"
#include "xf86Resources.h"
#include "xf86int10.h"

#include "xf86xv.h"
#include "Xv.h"

/*** Chip-specific includes ***/

/* #include "tseng_acl.h" */

/*
 * Forward definitions for the functions that make up the driver.
 */

/* Mandatory functions */
static const OptionInfoRec * TsengAvailableOptions(int chipid, int busid);
static void TsengIdentify(int flags);
static Bool TsengProbe(DriverPtr drv, int flags);
static Bool TsengPreInit(ScrnInfoPtr pScrn, int flags);
static Bool TsengScreenInit(int Index, ScreenPtr pScreen, int argc,
    char **argv);
static Bool TsengEnterVT(int scrnIndex, int flags);
static void TsengLeaveVT(int scrnIndex, int flags);
static Bool TsengCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool TsengSaveScreen(ScreenPtr pScreen, int mode);

/* Required if the driver supports mode switching */
static Bool TsengSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);

/* Optional functions */
static void TsengFreeScreen(int scrnIndex, int flags);
static ModeStatus TsengValidMode(int scrnIndex, DisplayModePtr mode,
    Bool verbose, int flags);

/* If driver-specific config file entries are needed, this must be defined */
/*static Bool   TsengParseConfig(ParseInfoPtr raw); */

/* Internally used functions (some are defined in tseng.h) */
static Bool TsengMapMem(ScrnInfoPtr pScrn);
static Bool TsengUnmapMem(ScrnInfoPtr pScrn);
static void TsengSave(ScrnInfoPtr pScrn);
static void TsengRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, TsengRegPtr tsengReg, int flags);
static void TsengUnlock(void);
static void TsengLock(void);

static Bool ET4000DetailedProbe(t_tseng_type * chiptype, t_w32_revid * rev);

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;
 
#define VERSION 4000
#define TSENG_NAME "TSENG"
#define TSENG_DRIVER_NAME "tseng"
#define TSENG_MAJOR_VERSION 1
#define TSENG_MINOR_VERSION 0
#define TSENG_PATCHLEVEL 0

/* CRTC timing limits */
#define Tseng_HMAX (4096-8)
#define Tseng_VMAX (2048-1)

/* 
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec TSENG =
{
    VERSION,
    TSENG_DRIVER_NAME,
    TsengIdentify,
    TsengProbe,
    TsengAvailableOptions,
    NULL,
    0
};

/* sub-revisions are now dealt with in the ChipRev variable */
static SymTabRec TsengChipsets[] =
{
    {TYPE_ET4000,	"ET4000"},
    {TYPE_ET4000W32,	"ET4000W32"},
    {TYPE_ET4000W32I,	"ET4000W32i"},
    {TYPE_ET4000W32P,	"ET4000W32p"},
    {TYPE_ET6000,	"ET6000"},
    {TYPE_ET6100,	"ET6100"},
    {TYPE_TSENG,	""},
    {-1, NULL}
};

/* Convert PCI ID to chipset name */
static PciChipsets TsengPciChipsets[] =
{
    {TYPE_ET4000W32P,	PCI_CHIP_ET4000_W32P_A,		RES_SHARED_VGA},
    {TYPE_ET4000W32P,	PCI_CHIP_ET4000_W32P_B,		RES_SHARED_VGA},
    {TYPE_ET4000W32P,	PCI_CHIP_ET4000_W32P_C,		RES_SHARED_VGA},
    {TYPE_ET4000W32P,	PCI_CHIP_ET4000_W32P_D,		RES_SHARED_VGA},
    {TYPE_ET6000,	PCI_CHIP_ET6000,		RES_SHARED_VGA},
    {-1,			-1,			RES_UNDEFINED}
};
    
static IsaChipsets TsengIsaChipsets[] =
{
    {TYPE_ET4000,	RES_EXCLUSIVE_VGA},
    {TYPE_ET4000W32,	RES_EXCLUSIVE_VGA},
    {TYPE_ET4000W32I,	RES_EXCLUSIVE_VGA},
    {TYPE_TSENG,        RES_EXCLUSIVE_VGA},
    {-1,		RES_UNDEFINED}
};

typedef enum {
    OPTION_HIBIT_HIGH,
    OPTION_HIBIT_LOW,
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_PCI_BURST,
    OPTION_SLOW_DRAM,
    OPTION_MED_DRAM,
    OPTION_FAST_DRAM,
    OPTION_W32_INTERLEAVE,
    OPTION_NOACCEL,
    OPTION_NOCLOCKCHIP,
    OPTION_LINEAR,
    OPTION_SHOWCACHE,
    OPTION_LEGEND,
    OPTION_PCI_RETRY,
    OPTION_SET_MCLK
} TsengOpts;

static const OptionInfoRec TsengOptions[] =
{
    {OPTION_HIBIT_HIGH, "hibit_high", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_HIBIT_LOW, "hibit_low", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_SW_CURSOR, "SWcursor", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_HW_CURSOR, "HWcursor", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_PCI_BURST, "pci_burst", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_SLOW_DRAM, "slow_dram", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_MED_DRAM, "med_dram", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_FAST_DRAM, "fast_dram", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_W32_INTERLEAVE, "w32_interleave", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_NOACCEL, "NoAccel", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_NOCLOCKCHIP, "NoClockchip", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_LINEAR, "Linear", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_SHOWCACHE, "ShowCache", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_LEGEND, "Legend", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_PCI_RETRY, "PciRetry", OPTV_BOOLEAN,
	{0}, FALSE},
    {OPTION_SET_MCLK, "SetMClk", OPTV_FREQ,
	{0}, FALSE},
    {-1, NULL, OPTV_NONE,
	{0}, FALSE}
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

static const char *vgaHWSymbols[] = {
  "vgaHWFreeHWRec",
  "vgaHWGetHWRec",
  "vgaHWGetIOBase",
  "vgaHWGetIndex",
  "vgaHWHandleColormaps",
  "vgaHWInit",
  "vgaHWLock",
  "vgaHWMapMem",
  "vgaHWProtect",
  "vgaHWRestore",
  "vgaHWSave", 
  "vgaHWSaveScreen",
  "vgaHWUnlock",
  "vgaHWUnmapMem",
  NULL
};

#ifdef XFree86LOADER
static const char* miscfbSymbols[] = {
  "xf1bppScreenInit",
  "xf4bppScreenInit",
  NULL
};
#endif

static const char* fbSymbols[] = {
  "fbPictureInit",
  "fbScreenInit",
  NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(tsengSetup);

static XF86ModuleVersionInfo tsengVersRec =
{
    "tseng",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    TSENG_MAJOR_VERSION, TSENG_MINOR_VERSION, TSENG_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,		       /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

/*
 * This is the module init data for XFree86 modules.
 *
 * Its name has to be the driver name followed by ModuleData.
 */
XF86ModuleData tsengModuleData = { &tsengVersRec, tsengSetup, NULL };

static pointer
tsengSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&TSENG, module, 0);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */
	/*
	 * Tell the loader about symbols from other modules that this module
	 * might refer to.
	 */
	LoaderRefSymLists(vgaHWSymbols, miscfbSymbols, fbSymbols, xaaSymbols,
			  int10Symbols, ramdacSymbols,  NULL);

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer) 1;
    } else {
	if (errmaj)
	    *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

#endif /* XFree86LOADER */

static Bool
TsengGetRec(ScrnInfoPtr pScrn)
{
    PDEBUG("	TsengGetRec\n");
    /*
     * Allocate an TsengRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(TsengRec), 1);
    /* Initialise it here when needed (or possible) */

    return TRUE;
}

static void
TsengFreeRec(ScrnInfoPtr pScrn)
{
    PDEBUG("	TsengFreeRec\n");
    if (pScrn->driverPrivate == NULL)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static Bool
TsengPCI2Type(ScrnInfoPtr pScrn, int ChipID)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    switch (ChipID) {
    case PCI_CHIP_ET4000_W32P_A:
	pTseng->ChipType = TYPE_ET4000W32P;
	pTseng->ChipRev = W32REVID_A;
	break;
    case PCI_CHIP_ET4000_W32P_B:
	pTseng->ChipType = TYPE_ET4000W32P;
	pTseng->ChipRev = W32REVID_B;
	break;
    case PCI_CHIP_ET4000_W32P_C:
	pTseng->ChipType = TYPE_ET4000W32P;
	pTseng->ChipRev = W32REVID_C;
	break;
    case PCI_CHIP_ET4000_W32P_D:
	pTseng->ChipType = TYPE_ET4000W32P;
	pTseng->ChipRev = W32REVID_D;
	break;
    case PCI_CHIP_ET6000:
	pTseng->ChipType = TYPE_ET6000;
	pTseng->ChipRev = pTseng->PciInfo->chipRev;	/* ET6000 != ET6100 */
	if (pTseng->ChipRev >= ET6100REVID)
	    pTseng->ChipType = TYPE_ET6100;
	break;
    default:
	xf86Msg(X_ERROR, "%s: Unknown Tseng PCI ID: %X\n", TSENG_NAME, ChipID);
	return FALSE;
    }
    return TRUE;
}

static const OptionInfoRec *
TsengAvailableOptions(int chipid, int busid)
{
    return TsengOptions;
}

static void
TsengIdentify(int flags)
{
    PDEBUG("	TsengIdentify\n");
    xf86PrintChipsets(TSENG_NAME, "driver for Tseng Labs chipsets",
	TsengChipsets);
}

static void
TsengAssignFPtr(ScrnInfoPtr pScrn)
{
    pScrn->driverVersion = VERSION;
    pScrn->driverName = TSENG_DRIVER_NAME;
    pScrn->name = TSENG_NAME;
    pScrn->Probe = TsengProbe;
    pScrn->PreInit = TsengPreInit;
    pScrn->ScreenInit = TsengScreenInit;
    pScrn->SwitchMode = TsengSwitchMode;
    pScrn->AdjustFrame = TsengAdjustFrame;
    pScrn->EnterVT = TsengEnterVT;
    pScrn->LeaveVT = TsengLeaveVT;
    pScrn->FreeScreen = TsengFreeScreen;
    pScrn->ValidMode = TsengValidMode;
}

/* unlock ET4000 using KEY register */
static void
TsengUnlock(void)
{
    unsigned char temp;
    int iobase = VGAHW_GET_IOBASE();

    PDEBUG("	TsengUnlock\n");
    outb(0x3BF, 0x03);
    outb(iobase + 8, 0xA0);
    outb(iobase + 4, 0x11);
    temp = inb(iobase + 5);
    outb(iobase + 5, temp & 0x7F);
}

/* lock ET4000 using KEY register. FIXME: should restore old lock status instead */
static void
TsengLock(void)
{
    unsigned char temp;
    int iobase = VGAHW_GET_IOBASE();

    PDEBUG("	TsengLock\n");
    outb(iobase + 4, 0x11);
    temp = inb(iobase + 5);
    outb(iobase + 5, temp | 0x80);
    outb(iobase + 8, 0x00);
    outb(0x3D8, 0x29);
    outb(0x3BF, 0x01);
}

/*
 * ET4000AutoDetect -- Old-style autodetection code (by register probing)
 *
 * This code is only called when the chipset is not given beforehand,
 * and if the PCI code hasn't detected one previously.
 */
#if 1
static Bool
ET4000MinimalProbe(void)
{
    unsigned char origVal, newVal;
    int iobase;

    PDEBUG("	ET4000MinimalProbe\n");
    /*
     * Note, the vgaHW module cannot be used here, but there are
     * some macros in vgaHW.h that can be used.
     */
    iobase = VGAHW_GET_IOBASE();

    /*
     * Check first that there is a ATC[16] register and then look at
     * CRTC[33]. If both are R/W correctly it's a ET4000 !
     */
    (void) inb(iobase + 0x0A);
    TsengUnlock();		       /* only ATC 0x16 is protected by KEY */
    outb(0x3C0, 0x16 | 0x20);
    origVal = inb(0x3C1);
    outb(0x3C0, origVal ^ 0x10);
    outb(0x3C0, 0x16 | 0x20);
    newVal = inb(0x3C1);
    outb(0x3C0, origVal);
/*    TsengLock();    FIXME: RESTORE OLD CONTENTS INSTEAD ! */
    if (newVal != (origVal ^ 0x10)) {
	return (FALSE);
    }
    outb(iobase + 0x04, 0x33);
    origVal = inb(iobase + 0x05);
    outb(iobase + 0x05, origVal ^ 0x0F);
    newVal = inb(iobase + 0x05);
    outb(iobase + 0x05, origVal);
    if (newVal != (origVal ^ 0x0F)) {
	return (FALSE);
    }
    return TRUE;
}
#endif

static int
TsengFindIsaDevice(GDevPtr dev)
{
    /* XXX Need to implement this */
    if (ET4000MinimalProbe())
	return TYPE_TSENG;

    return -1;
}

static Bool
TsengProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int numDevSections;
    int numUsed;
    int *usedChips = NULL;
    Bool foundScreen = FALSE;
    
    
    PDEBUG("	TsengProbe\n");
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
     * Find the config file Device sections that match this
     * driver, and return if there are none.
     */
    if ((numDevSections = xf86MatchDevice(TSENG_DRIVER_NAME,
		&devSections)) <= 0) {
	return FALSE;
    }

    /* XXX maybe this can go some time soon */
    /*
     * for the Tseng server, there can only be one matching
     * device section. So issue a warning if more than one show up.
     * Multiple Tseng cards in the same machine are not possible.
     */
    /*
     * If this is a PCI card, "probing" just amounts to checking the PCI
     * data that the server has already collected.  If there is none,
     * check for non-PCI boards.
     *
     * The provided xf86MatchPciInstances() helper takes care of
     * the details.
     */
    numUsed = 0;
    if (xf86GetPciVideoInfo() != NULL) {
	numUsed = xf86MatchPciInstances(TSENG_NAME, PCI_VENDOR_TSENG,
					TsengChipsets, TsengPciChipsets, 
					devSections,numDevSections, drv,
					&usedChips);
	if (numUsed > 0) {
	    if (flags & PROBE_DETECT)
		foundScreen = TRUE;
	    else for (i = 0; i < numUsed; i++) {
		/* Allocate a ScrnInfoRec  */
		ScrnInfoPtr pScrn = NULL;
		if ((pScrn = xf86ConfigPciEntity(pScrn,0,usedChips[i],
						       TsengPciChipsets,NULL,
						       NULL,NULL,NULL,NULL))) {
		    TsengAssignFPtr(pScrn);
		    foundScreen = TRUE;
		}
	    }
	    xfree(usedChips);
	}
    }
    
    /* Check for non-PCI cards */
    numUsed = xf86MatchIsaInstances(TSENG_NAME, TsengChipsets,
			TsengIsaChipsets,drv, TsengFindIsaDevice, devSections,
			numDevSections, &usedChips);
    if (numUsed > 0)  {
	if (flags & PROBE_DETECT)
	    foundScreen = TRUE;
	else for (i = 0; i < numUsed; i++) {
		ScrnInfoPtr pScrn = NULL;
		if ((pScrn = xf86ConfigIsaEntity(pScrn,0,usedChips[i],
						       TsengIsaChipsets,NULL,
						       NULL,NULL,NULL,NULL))) {
		    TsengAssignFPtr(pScrn);
		    foundScreen = TRUE;
		}
	}
	xfree(usedChips);
    }
    xfree(devSections);
    return foundScreen;
}

/* The PCI part of TsengPreInit() */
static Bool
TsengPreInitPCI(ScrnInfoPtr pScrn)
{
    MessageType from;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengPreInitPCI\n");
    /*
     * * Set the ChipType and ChipRev, allowing config file entries to
     * * override.
     */
    if (pTseng->pEnt->device->chipset && *pTseng->pEnt->device->chipset) {
	/* chipset given as a string in the config file */
	pScrn->chipset = pTseng->pEnt->device->chipset;
	pTseng->ChipType =
	    (t_tseng_type)xf86StringToToken(TsengChipsets, pScrn->chipset);
	/* FIXME: still need to probe for W32p revision here */
	from = X_CONFIG;
    } else if (pTseng->pEnt->device->chipID >= 0) {
	/* chipset given as a PCI ID in the config file */
	if (!TsengPCI2Type(pScrn, pTseng->pEnt->device->chipID))
	    return FALSE;
	pScrn->chipset = (char *)xf86TokenToString(TsengChipsets, pTseng->ChipType);
	from = X_CONFIG;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
	    pTseng->ChipType);
    } else {
	/* probe for chipset type */
	from = X_PROBED;
	if (!TsengPCI2Type(pScrn, pTseng->PciInfo->chipType))
	    return FALSE;
	pScrn->chipset = (char *)xf86TokenToString(TsengChipsets, pTseng->ChipType);
    }

    if (pTseng->pEnt->device->chipRev >= 0) {
	pTseng->ChipRev = pTseng->pEnt->device->chipRev;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
	    pTseng->ChipRev);
	if ((pTseng->ChipType == TYPE_ET6000) && (pTseng->ChipRev >= ET6100REVID))
	    pTseng->ChipType = TYPE_ET6100;
    } else {
	t_tseng_type dum_chiptype;
	t_w32_revid rev;
	if (Is_ET6K) {
	    pTseng->ChipRev = pTseng->PciInfo->chipRev;
	} else {
	    /*
	     * on W32p cards, the PCI ChipRev field is always 0 (it is not
	     * implemented).  We will use the ChipRev field to distinguish
	     * between revisions A through D, so we set it up with the
	     * standard register-probing "Detailed Probe" instead.
	     */
	    ET4000DetailedProbe(&dum_chiptype, &rev);
	    pTseng->ChipRev = rev;
	}
    }

    if (Is_ET6K) {
	xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);
    } else {
	char ch_rev;
	switch (pTseng->ChipRev) {
	case W32REVID_A:
	    ch_rev = 'A';
	    break;
	case W32REVID_B:
	    ch_rev = 'B';
	    break;
	case W32REVID_C:
	    ch_rev = 'C';
	    break;
	case W32REVID_D:
	    ch_rev = 'D';
	    break;
	default:
	    ch_rev = 'X';
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\" (rev %c)\n",
	    pScrn->chipset, ch_rev);
    }

    pTseng->PciTag = pciTag(pTseng->PciInfo->bus, pTseng->PciInfo->device,
	pTseng->PciInfo->func);

    /* only the ET6000 implements a PCI IO address */
    if (Is_ET6K) {
	if (pTseng->pEnt->device->IOBase != 0) {
	    pTseng->IOAddress = pTseng->pEnt->device->IOBase;
	    from = X_CONFIG;
	} else {
	    if ((pTseng->PciInfo->ioBase[1]) != 0) {
		pTseng->IOAddress = pTseng->PciInfo->ioBase[1];
		from = X_PROBED;
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "No valid PCI I/O address in PCI config space\n");
		return FALSE;
	    }
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "PCI I/O registers at 0x%lX\n",
	    (unsigned long)pTseng->IOAddress);
    }

    pTseng->LinFbAddressMask = 0xFF000000;

    return TRUE;
}

/*
 * This is a more detailed probe. We already know it's a Tseng chip from
 * TsengPreInit() and ET4000MinimalProbe().
 */

static Bool
ET4000DetailedProbe(t_tseng_type * chiptype, t_w32_revid * rev)
{
    unsigned char temp, origVal, newVal;

    PDEBUG("	ET4000DetailedProbe\n");

    /*
     * We know it's an ET4000, now check for an ET4000/W32.
     * Test for writability of 0x3cb.
     */

    origVal = inb(0x3cb);
    outb(0x3cb, 0x33);		       /* Arbitrary value */
    newVal = inb(0x3cb);
    outb(0x3cb, origVal);
    if (newVal != 0x33) {	       /* not a W32; so it's a standard ET4000 */
	*chiptype = TYPE_ET4000;
	*rev = TSENGNOREV;
	return TRUE;
    }
    /* We have an ET4000/W32. Now determine the type. */
    outb(0x217a, 0xec);
    temp = inb(0x217b) >> 4;
    switch (temp) {
    case 0:			       /* ET4000/W32 */
	*chiptype = TYPE_ET4000W32;
	break;
    case 1:			       /* ET4000/W32i */
	*chiptype = TYPE_ET4000W32I;
	*rev = W32REVID_A;
	break;
    case 3:			       /* ET4000/W32i rev b */
	*chiptype = TYPE_ET4000W32I;
	*rev = W32REVID_B;
	break;
    case 11:			       /* ET4000/W32i rev c */
	*chiptype = TYPE_ET4000W32I;
	*rev = W32REVID_C;
	break;
    case 2:			       /* ET4000/W32p rev a */
	*chiptype = TYPE_ET4000W32P;
	*rev = W32REVID_A;
	break;
    case 5:			       /* ET4000/W32p rev b */
	*chiptype = TYPE_ET4000W32P;
	*rev = W32REVID_B;
	break;
    case 6:			       /* ET4000/W32p rev d */
	*chiptype = TYPE_ET4000W32P;
	*rev = W32REVID_D;
	break;
    case 7:			       /* ET4000/W32p rev c */
	*chiptype = TYPE_ET4000W32P;
	*rev = W32REVID_C;
	break;
    default:
	return (FALSE);
    }

    return (TRUE);
}

/*
 * TsengFindBusType --
 *      determine bus interface type
 *      (also determines Lin Mem address mask, because that depends on bustype)
 *
 * We don't need to bother with PCI buses here: TsengPreInitPCI() took care
 * of that. This code isn't called if it's a PCI bus anyway.
 */

static void
TsengFindNonPciBusType(ScrnInfoPtr pScrn)
{
    unsigned char bus;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengFindNonPciBusType\n");
    pTseng->Bustype = T_BUS_ISA;
    pTseng->Linmem_1meg = FALSE;
    pTseng->LinFbAddressMask = 0;

    switch (pTseng->ChipType) {
    case TYPE_ET4000:
	break;
    case TYPE_ET4000W32:
    case TYPE_ET4000W32I:
	/*
	 * Notation: SMx = bit x of Segment Map Comparator (CRTC index 0x30)
	 *
	 * We assume the driver code disables the image port (which it does)
	 *
	 * ISA:      [ A23==SEGE, A22, A21, A20 ] ==      [ SM1, SM0, 0, 0 ]
	 * MCA: [ A24, A23, A22, A21, A20 ] == [ SM2, SM1, SM0, 0, 0 ]
	 * VLB: [ /A26, /A25, /A24, A23, A22, A21, A20 ] ==   ("/" means inverted!)
	 *       [ SM4,  SM3,  SM2, SM1, SM0, 0  , 0   ]
	 */
	outb(0x217A, 0xEF);
	bus = inb(0x217B) & 0x60;      /* Determine bus type */
	switch (bus) {
	case 0x40:
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32/W32i bus type: MCA\n");
	    pTseng->Bustype = T_BUS_MCA;
	    pTseng->LinFbAddressMask = 0x01C00000;	/* MADE24, A23 and A22 are decoded */
	    break;
	case 0x60:
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32/W32i bus type: Local Bus\n");
	    pTseng->Bustype = T_BUS_VLB;
	    pTseng->LinFbAddressMask = 0x07C00000;	/* A26..A22 are decoded */
	    break;
	case 0x00:
	case 0x20:
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32/W32i bus type (0x%02X): ISA\n", bus);
	    pTseng->Bustype = T_BUS_ISA;
	    pTseng->LinFbAddressMask = 0x00C00000;	/* SEGE and A22 are decoded */
	    break;
	}
	break;
    case TYPE_ET4000W32P:
	outb(0x217A, 0xEF);
	bus = inb(0x217B) >> 3;	       /* Determine bus type */
	switch (bus) {
	case 0x1C:		       /* PCI case already handled in TsengPreInitPCI() */
	    pTseng->Bustype = T_BUS_VLB;
	    pTseng->LinFbAddressMask = 0x3FC00000;	/* A29..A22 */
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32p bus type: Local Buffered Bus\n");
	    pTseng->Linmem_1meg = TRUE;		/* IMA bus support allows for only 1M linear memory */
	    break;
	case 0x13:
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32p bus type: Local Bus (option 1a)\n");
	    pTseng->Bustype = T_BUS_VLB;
	    if (pTseng->ChipRev == W32REVID_A)
		pTseng->LinFbAddressMask = 0x07C00000;
	    else
		pTseng->LinFbAddressMask = 0x1FC00000;	/* SEGI,A27..A22 */
	    break;
	case 0x11:
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32p bus type: Local Bus (option 1b)\n");
	    pTseng->Bustype = T_BUS_VLB;
	    pTseng->LinFbAddressMask = 0x00C00000;	/* SEGI,A22 */
	    pTseng->Linmem_1meg = TRUE;		/* IMA bus support allows for only 1M linear memory */
	    break;
	case 0x08:
	case 0x0B:
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected W32p bus type: Local Bus (option 2)\n");
	    pTseng->Bustype = T_BUS_VLB;
	    pTseng->LinFbAddressMask = 0x3FC00000;	/* A29..A22 */
	    break;
	}
	if (Is_W32p_cd && (pTseng->LinFbAddressMask == 0x3FC00000))
	    pTseng->LinFbAddressMask |= 0xC0000000;	/* A31,A30 decoded from PCI config space */
	break;
    case TYPE_ET6000:
    case TYPE_ET6100:
	pTseng->Bustype = T_BUS_PCI;
	pTseng->LinFbAddressMask = 0xFF000000;
	break;
	case TYPE_TSENG: /* generic */
	    break;
    }
}

/* The TsengPreInit() part for non-PCI buses */
static Bool
TsengPreInitNoPCI(ScrnInfoPtr pScrn)
{
    MessageType from;
    t_w32_revid rev = TSENGNOREV;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengPreInitNoPCI\n");
    /*
     * Set the ChipType and ChipRev, allowing config file entries to
     * override.
     */
    if (pTseng->pEnt->device->chipset && *pTseng->pEnt->device->chipset) {
	/* chipset given as a string in the config file */
	pScrn->chipset = pTseng->pEnt->device->chipset;
	pTseng->ChipType =
	    (t_tseng_type)xf86StringToToken(TsengChipsets, pScrn->chipset);
	from = X_CONFIG;
    } else if (pTseng->pEnt->device->chipID > 0) {
	/* chipset given as a PCI ID in the config file */
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "ChipID override only possible for PCI cards\n");
	return FALSE;
    } else {
	from = X_PROBED;
	if (!ET4000DetailedProbe(&(pTseng->ChipType), &rev)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Unknown Tseng chip detected. Try chipset override.\n");
	    return FALSE;
	}
	pScrn->chipset = (char *)xf86TokenToString(TsengChipsets, pTseng->ChipType);
    }
    if (pTseng->pEnt->device->chipRev >= 0) {
	pTseng->ChipRev = pTseng->pEnt->device->chipRev;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
	    pTseng->ChipRev);
    } else {
	pTseng->ChipRev = rev;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);

    TsengFindNonPciBusType(pScrn);

    return TRUE;
}

/*
 * The 8*32kb ET6000 MDRAM granularity causes the more general probe to
 * detect too much memory in some configurations, because that code has a
 * 8-bank (=256k) granularity. E.g. it fails to recognize 2.25 MB of memory
 * (detects 2.5 instead). This function goes to check if the RAM is actually
 * there. MDRAM comes in multiples of 4 banks (16, 24, 32, 36, 40, 64, 72,
 * 80, ... 32kb-banks), so checking each 64k block should be enough granularity.
 *
 * No more than the amount of refreshed RAM is checked. Non-refreshed RAM
 * won't work anyway.
 *
 * The same code could be used on other Tseng chips, or even on ANY
 * VGA board, but probably only in case of trouble.
 *
 * FIXME: this should be done using linear memory
 */
#define VIDMEM ((volatile CARD32*)check_vgabase)
#define SEGSIZE (64)		       /* kb */

#define ET6K_SETSEG(seg) \
	outb(0x3CB, ((seg) & 0x30) | ((seg) >> 4)); \
	outb(0x3CD, ((seg) & 0x0f) | ((seg) << 4));

static int
et6000_check_videoram(ScrnInfoPtr pScrn, int ram)
{
    unsigned char oldSegSel1, oldSegSel2, oldGR5, oldGR6, oldSEQ2, oldSEQ4;
    int segment, i;
    int real_ram = 0;
    pointer check_vgabase;
    Bool fooled = FALSE;
    int save_vidmem;

    PDEBUG("	et6000_check_videoram\n");
    if (ram > 4096) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    "Detected more than 4096 kb of video RAM. Clipped to 4096kb\n");
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	    "    (Tseng VGA chips can only use 4096kb).\n");
	ram = 4096;
    }
    if (!vgaHWMapMem(pScrn)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    "Could not map VGA memory to check for video memory.\n");
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    "    Detected amount may be wrong.\n");
	return ram;
    }
    check_vgabase = (VGAHWPTR(pScrn)->Base);

    /*
     * We need to set the VGA controller in VGA graphics mode, or else we won't
     * be able to access the full 4MB memory range. First, we save the
     * registers we modify, of course.
     */

    oldSegSel1 = inb(0x3CD);
    oldSegSel2 = inb(0x3CB);
    outb(0x3CE, 5);
    oldGR5 = inb(0x3CF);
    outb(0x3CE, 6);
    oldGR6 = inb(0x3CF);
    outb(0x3C4, 2);
    oldSEQ2 = inb(0x3C5);
    outb(0x3C4, 4);
    oldSEQ4 = inb(0x3C5);

    /* set graphics mode */
    outb(0x3CE, 6);
    outb(0x3CF, 5);
    outb(0x3CE, 5);
    outb(0x3CF, 0x40);
    outb(0x3C4, 2);
    outb(0x3C5, 0x0f);
    outb(0x3C4, 4);
    outb(0x3C5, 0x0e);

    /*
     * count down from presumed amount of memory in SEGSIZE steps, and
     * look at each segment for real RAM.
     *
     * To select a segment, we cannot use ET4000W32SetReadWrite(), since
     * that requires the ScreenPtr, which we don't have here.
     */

    for (segment = (ram / SEGSIZE) - 1; segment >= 0; segment--) {
	/* select the segment */
	ET6K_SETSEG(segment);

	/* save contents of memory probing location */
	save_vidmem = *(VIDMEM);

	/* test with pattern */
	*VIDMEM = 0xAAAA5555;
	if (*VIDMEM != 0xAAAA5555) {
	    *VIDMEM = save_vidmem;
	    continue;
	}
	/* test with inverted pattern */
	*VIDMEM = 0x5555AAAA;
	if (*VIDMEM != 0x5555AAAA) {
	    *VIDMEM = save_vidmem;
	    continue;
	}
	/*
	 * If we get here, the memory seems to be writable/readable
	 * Now check if we aren't fooled by address wrapping (mirroring)
	 */
	fooled = FALSE;
	for (i = segment - 1; i >= 0; i--) {
	    /* select the segment */
	    ET6K_SETSEG(i);
	    outb(0x3CB, (i & 0x30) | (i >> 4));
	    outb(0x3CD, (i & 0x0f) | (i << 4));
	    if (*VIDMEM == 0x5555AAAA) {
		/*
		 * Seems like address wrap, but there could of course be
		 * 0x5555AAAA in here by accident, so we check with another
		 * pattern again.
		 */
		ET6K_SETSEG(segment);
		/* test with other pattern again */
		*VIDMEM = 0xAAAA5555;
		ET6K_SETSEG(i);
		if (*VIDMEM == 0xAAAA5555) {
		    /* now we're sure: this is not real memory */
		    fooled = TRUE;
		    break;
		}
	    }
	}
	if (!fooled) {
	    real_ram = (segment + 1) * SEGSIZE;
	    break;
	}
	/* restore old contents again */
	ET6K_SETSEG(segment);
	*VIDMEM = save_vidmem;
    }

    /* restore original register contents */
    outb(0x3CD, oldSegSel1);
    outb(0x3CB, oldSegSel2);
    outb(0x3CE, 5);
    outb(0x3CF, oldGR5);
    outb(0x3CE, 6);
    outb(0x3CF, oldGR6);
    outb(0x3C4, 2);
    outb(0x3C5, oldSEQ2);
    outb(0x3C4, 4);
    outb(0x3C5, oldSEQ4);

    vgaHWUnmapMem(pScrn);
    return real_ram;
}

/*
 * Handle amount of allowed memory: some combinations can't use all
 * available memory. Should we still allow the user to override this?
 *
 * This must be called AFTER the decision has been made to use linear mode
 * and/or acceleration, or the memory limit code won't be able to work.
 */

static int
TsengDoMemLimit(ScrnInfoPtr pScrn, int ram, int limit, char *reason)
{
    if (ram > limit) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Only %d kb of memory can be used %s.\n",
	    limit, reason);
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Reducing video memory to %d kb.\n", limit);
	ram = limit;
    }
    return ram;
}

static int
TsengLimitMem(ScrnInfoPtr pScrn, int ram)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    if (pTseng->UseLinMem && pTseng->Linmem_1meg) {
	ram = TsengDoMemLimit(pScrn, ram, 1024,
			      "in linear mode on "
			      "this VGA board/bus configuration");
    }
    if (pTseng->UseAccel && pTseng->UseLinMem) {
	if (Is_W32_any) {
	    /* <= W32p_ab :
	     *   2 MB direct access + 2*512kb via apertures MBP0 and MBP1
	     * == W32p_cd :
	     *   2*1MB via apertures MBP0 and MBP1
	     */
	    if (Is_W32p_cd)
		ram = TsengDoMemLimit(pScrn, ram, 2048,
				      "in linear + accelerated mode "
				      "on W32p rev c and d");

	    ram = TsengDoMemLimit(pScrn, ram, 2048 + 1024,
				  "in linear + accelerated mode "
				  "on W32/W32i/W32p");

	    /*
	     * upper 516kb of 4MB linear map used for
	     *  "externally mapped registers"
	     */
	    ram = TsengDoMemLimit(pScrn, ram, 4096 - 516,
				  "in linear + accelerated mode "
				  "on W32/W32i/W32p");
	}
	if (Is_ET6K) {
	    /*
	     * upper 8kb used for externally mapped and
	     * memory mapped registers
	     */
	    ram = TsengDoMemLimit(pScrn, ram, 4096 - 8,
				  "in linear + accelerated mode "
				  "on ET6000/6100");
	}
    }
    ram = TsengDoMemLimit(pScrn, ram, 4096, "on any Tseng card");
    return ram;
}

/*
 * TsengDetectMem --
 *      try to find amount of video memory installed.
 *
 */

static int
TsengDetectMem(ScrnInfoPtr pScrn)
{
    unsigned char config;
    int ramtype = 0;
    int ram = 0;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengDetectMem\n");
    if (Is_ET6K) {
	ramtype = inb(0x3C2) & 0x03;
	switch (ramtype) {
	case 0x03:		       /* MDRAM */
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Video memory type: Multibank DRAM (MDRAM).\n");
	    ram = ((inb(pTseng->IOAddress + 0x47) & 0x07) + 1) * 8 * 32;	/* number of 8 32kb banks  */
	    if (inb(pTseng->IOAddress + 0x45) & 0x04) {
		ram <<= 1;
	    }
	    /*
	     * 8*32kb MDRAM refresh control granularity in the ET6000 fails to
	     * recognize 2.25 MB of memory (detects 2.5 instead)
	     */
	    ram = et6000_check_videoram(pScrn, ram);
	    break;
	case 0x00:		       /* DRAM -- VERY unlikely on ET6000 cards, IMPOSSIBLE on ET6100 */
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Video memory type: Standard DRAM.\n");
	    ram = 1024 << (inb(pTseng->IOAddress + 0x45) & 0x03);
	    break;
	default:		       /* unknown RAM type */
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"Unknown ET6000 video memory type %d -- assuming 1 MB (unless specified)\n",
		ramtype);
	    ram = 1024;
	}
    } else {			       /* pre-ET6000 devices */
	int iobase = VGAHWPTR(pScrn)->IOBase;

	outb(iobase + 0x04, 0x37);
	config = inb(iobase + 0x05);

	ram = 128 << (config & 0x03);

	if (config & 0x80)
	    ram <<= 1;

	/* Check for interleaving on W32i/p. */
	if (Is_W32i || Is_W32p) {
	    outb(iobase + 0x04, 0x32);
	    config = inb(iobase + 0x05);
	    if (config & 0x80) {
		ram <<= 1;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Video memory type: Interleaved DRAM.\n");
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Video memory type: Standard DRAM.\n");
	    }
	}
    }
    return ram;
}

static Bool
TsengProcessHibit(ScrnInfoPtr pScrn)
{
    MessageType from = X_CONFIG;
    int hibit_mode_width;
    int iobase;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengProcessHibit\n");
    if (xf86IsOptionSet(pTseng->Options, OPTION_HIBIT_HIGH)) {
	if (xf86IsOptionSet(pTseng->Options, OPTION_HIBIT_LOW)) {
	    xf86Msg(X_ERROR, "\nOptions \"hibit_high\" and \"hibit_low\" are incompatible;\n");
	    xf86Msg(X_ERROR, "    specify only one (not both) in XFree86 configuration file\n");
	    return FALSE;
	}
	pTseng->save_divide = 0x40;
    } else if (xf86IsOptionSet(pTseng->Options, OPTION_HIBIT_HIGH)) {
	pTseng->save_divide = 0;
    } else {
	from = X_PROBED;
	/* first check to see if hibit is probed from low-res mode */
	iobase = VGAHWPTR(pScrn)->IOBase;
	outb(iobase + 4, 1);
	hibit_mode_width = inb(iobase + 5) + 1;
	if (hibit_mode_width > 82) {
	    xf86Msg(X_WARNING, "Non-standard VGA text or graphics mode while probing for hibit:\n");
	    xf86Msg(X_WARNING, "    probed 'hibit' value may be wrong.\n");
	    xf86Msg(X_WARNING, "    Preferably run probe from 80x25 textmode,\n");
	    xf86Msg(X_WARNING, "    or specify correct value in XFree86 configuration file.\n");
	}
	/* Check for initial state of divide flag */
	outb(0x3C4, 7);
	pTseng->save_divide = inb(0x3C5) & 0x40;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Initial ET4000 hibit state: %s\n",
	pTseng->save_divide & 0x40 ? "high" : "low");
    return TRUE;
}

static Bool
TsengProcessOptions(ScrnInfoPtr pScrn)
{
    MessageType from;
    double real;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengProcessOptions\n");

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pTseng->Options = xalloc(sizeof(TsengOptions))))
	return FALSE;
    memcpy(pTseng->Options, TsengOptions, sizeof(TsengOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pTseng->Options);

    from = X_DEFAULT;
    pTseng->HWCursor = FALSE;	       /* default */
    if (xf86GetOptValBool(pTseng->Options, OPTION_HW_CURSOR, &pTseng->HWCursor))
	from = X_CONFIG;
    if (xf86ReturnOptValBool(pTseng->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pTseng->HWCursor = FALSE;
    }
    if (pTseng->HWCursor && !Is_ET6K) {
	xf86DrvMsg(pScrn->scrnIndex, from,
		"Hardware Cursor not supported on this chipset\n");
	pTseng->HWCursor = FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
	pTseng->HWCursor ? "HW" : "SW");

    if (pScrn->bitsPerPixel >= 8) {
        if (pTseng->ChipType != TYPE_ET4000)
	    pTseng->UseAccel = TRUE;
	if (xf86ReturnOptValBool(pTseng->Options, OPTION_NOACCEL, FALSE)) {
	    pTseng->UseAccel = FALSE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
	}
    } else
	pTseng->UseAccel = FALSE;  /* 1bpp and 4bpp are always non-accelerated */

    pTseng->SlowDram = FALSE;
    if (xf86IsOptionSet(pTseng->Options, OPTION_SLOW_DRAM)) {
	pTseng->SlowDram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using slow DRAM access\n");
    }
    pTseng->MedDram = FALSE;
    if (xf86IsOptionSet(pTseng->Options, OPTION_MED_DRAM)) {
	pTseng->MedDram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using Medium-speed DRAM access\n");
    }
    pTseng->FastDram = FALSE;
    if (xf86IsOptionSet(pTseng->Options, OPTION_FAST_DRAM)) {
	pTseng->FastDram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using fast DRAM access\n");
    }
    if ((pTseng->SetW32Interleave = 
	xf86GetOptValBool(pTseng->Options, OPTION_W32_INTERLEAVE, &pTseng->W32Interleave)) )
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forcing W32p memory interleave %s.\n",
	    pTseng->W32Interleave ? "ON" : "OFF");
    if ((pTseng->SetPCIBurst = 
	xf86GetOptValBool(pTseng->Options, OPTION_PCI_BURST, &pTseng->PCIBurst)) )
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forcing PCI burst mode %s.\n",
	    pTseng->PCIBurst ? "ON" : "OFF");
    from = X_CONFIG;
    if (xf86GetOptValBool(pTseng->Options, OPTION_LINEAR, &pTseng->UseLinMem)) {
	/* check if linear mode is allowed */
	if (pTseng->UseLinMem) {
	    if (!CHIP_SUPPORTS_LINEAR) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Linear memory not supported on chipset \"%s\".\n",
		    pScrn->chipset);
		pTseng->UseLinMem = FALSE;
	    } else if (!xf86LinearVidMem()) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "This operating system does not support a linear framebuffer.\n");
		pTseng->UseLinMem = FALSE;
	    }
	}
    } else {
	/* option not specified: use defaults */
	from = X_DEFAULT;
	if (pTseng->PciTag)
	    pTseng->UseLinMem = TRUE;      /* use linear memory by default on PCI systems */
	else
	    pTseng->UseLinMem = FALSE;     /* ... and banked on non-PCI systems */
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s Memory.\n",
	(pTseng->UseLinMem) ? "linear" : "banked");

    pTseng->ShowCache = FALSE;
    if (xf86ReturnOptValBool(pTseng->Options, OPTION_SHOWCACHE, FALSE)) {
	pTseng->ShowCache = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "(for debugging only:) Visible off-screen memory\n");
    }
    pTseng->Legend = FALSE;
    if (xf86ReturnOptValBool(pTseng->Options, OPTION_LEGEND, FALSE)) {
	pTseng->Legend = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using Legend pixel clock selection.\n");
    }
    pTseng->NoClockchip = FALSE;
    if (xf86ReturnOptValBool(pTseng->Options, OPTION_NOCLOCKCHIP, FALSE)) {
	pTseng->NoClockchip = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Disabling clockchip programming.\n");
    }
    /*
     * Check_Tseng_ramdac() already set pScrn->progClock according to probed
     * values. Override it if requested.
     */
    if (pTseng->NoClockchip)
	pScrn->progClock = FALSE;

    pTseng->UsePCIRetry = FALSE;
    if (xf86ReturnOptValBool(pTseng->Options, OPTION_PCI_RETRY, FALSE)) {
	pTseng->UsePCIRetry = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry enabled\n");
    }
    pTseng->MemClk = 0;
    if (xf86GetOptValFreq(pTseng->Options, OPTION_SET_MCLK, OPTUNITS_MHZ, &real))
	pTseng->MemClk = (int)(real * 1000.0);
    return TRUE;
}

static Bool
TsengGetLinFbAddress(ScrnInfoPtr pScrn)
{
    MessageType from;
    TsengPtr pTseng = TsengPTR(pScrn);
    resRange range[] = { {ResExcMemBlock|ResBus,0,0},_END };

    PDEBUG("	TsengGetLinFbAddress\n");

    /* let config file override Base address */
    if (pTseng->pEnt->device->MemBase != 0) {
	pTseng->LinFbAddress = pTseng->pEnt->device->MemBase;
	from = X_CONFIG;
	/* check for possible errors in given linear base address */
	if ((pTseng->LinFbAddress & (~pTseng->LinFbAddressMask)) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"MemBase out of range. Must be <= 0x%lx on 0x%lx boundary.\n",
		pTseng->LinFbAddressMask, ~(pTseng->LinFbAddressMask | 0xFF000000) + 1);
	    pTseng->LinFbAddress &= ~pTseng->LinFbAddressMask;
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "    Clipping MemBase to: 0x%lx.\n",
		pTseng->LinFbAddress);
	    range[0].rBegin = pTseng->LinFbAddress;
	    range[0].rEnd = pTseng->LinFbAddress + 16 * 1024 * 1024;
	    if (xf86RegisterResources(pTseng->pEnt->index,range,ResNone)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "    Cannot register linear memory."
			   " Using banked mode instead.\n");
		pTseng->UseLinMem = FALSE;
		return TRUE;
	    }
	}
    } else {
	from = X_PROBED;
	if (pTseng->PciTag) {
	    /*
	     * base0 is the framebuffer and base1 is the PCI IO space.
	     */
	    if ((pTseng->PciInfo->memBase[0]) != 0) {
		pTseng->LinFbAddress = pTseng->PciInfo->memBase[0];
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "No valid Framebuffer address in PCI config space;\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "    Falling back to banked mode.\n");
		pTseng->UseLinMem = FALSE;
		return TRUE;
	    }
	    if (xf86RegisterResources(pTseng->pEnt->index,NULL,ResNone)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "    Cannot register linear memory."
			   " Using banked mode instead.\n");
		pTseng->UseLinMem = FALSE;
		return TRUE;
	    }
	} else {		       /* non-PCI boards */
	    /* W32p cards can give us their Lin. memory address through the PCI
	     * configuration. For W32i, this is not possible (VL-bus, MCA or ISA). W32i
	     * cards have three extra external "address" lines, SEG2..SEG0 which _can_
	     * be connected to any set of address lines in addition to the already
	     * connected A23..A0. SEG2..SEG0 are either for three extra address lines
	     * or to connect an external address decoder (mostly an 74F27). It is NOT
	     * possible to know how SEG2..SEG0 are connected. We _assume_ they are
	     * connected to A26..A24 (most likely case). This means linear memory can
	     * be decoded into any 4MB block in the address range 0..128MB.
	     *
	     * For non-PCI cards (especially VLB), most motherboards don't decode all
	     * 32 address bits. The weird default memory base below will always end up
	     * at the end of the decoded address space -- independent of the number of
	     * address bits that are decoded.
	     */
#define DEFAULT_LIN_MEMBASE ( (256 + 128 + 64 + 32 + 16 + 8 + 4) * 1024*1024 )
#define DEFAULT_LIN_MEMBASE_PCI (DEFAULT_LIN_MEMBASE & 0xFF000000)

	    switch (pTseng->ChipType) {
	    case TYPE_ET4000W32:
	    case TYPE_ET4000W32I:
	    case TYPE_ET4000W32P:     /* A31,A30 are decoded as 00 (=always mapped below 512 MB) */
		pTseng->LinFbAddress = DEFAULT_LIN_MEMBASE;
		if (pTseng->LinFbAddress > pTseng->LinFbAddressMask)	/* ... except if we can't decode that much */
		    pTseng->LinFbAddress = pTseng->LinFbAddressMask - 4 * 1024 * 1024;	/* top of decodable memory */
		break;
	    default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "TsengNonPciLinMem(): Internal error. This should not happen: please report to XFree86@XFree86.Org\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "    Falling back to banked mode.\n");
		pTseng->UseLinMem = FALSE;
		return TRUE;
	    }
	    pTseng->LinFbAddress &= pTseng->LinFbAddressMask;

	    /* One final check for a valid MemBase */
	    if (pTseng->LinFbAddress < 4096 * 1024) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Invalid MemBase for linear mode:\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "    please define a non-zero MemBase in XF86Config.\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "    See README.tseng or tseng.sgml for more information.\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "    Using banked mode instead.\n");
		pTseng->UseLinMem = FALSE;
		return TRUE;
	    }
	    range[0].type |= ResBios;
	    range[0].rBegin = pTseng->LinFbAddress;
	    range[0].rEnd = pTseng->LinFbAddress + 16 * 1024 * 1024;
	    if (xf86RegisterResources(pTseng->pEnt->index,range,ResNone)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "    Cannot register linear memory."
			   " Using banked mode instead.\n");
		pTseng->UseLinMem = FALSE;
		return TRUE;
	    }
	}
    }

    /* The W32 linear map address space is always 4Mb (mainly because the
     * memory-mapped registers are located near the top of the 4MB area). 
     * The ET6000 maps out 16 Meg, but still uses only 4Mb of that. 
     * However, since all mmap()-ed space is also reflected in the "ps"
     * listing for the Xserver, many users will be worried by a server that
     * always eats 16MB of memory, even if it's not "real" memory, just
     * address space. Not mapping all of the 16M may be a potential problem
     * though: if another board is mapped on top of the remaining part of
     * the 16M... Boom!
     */
    if (Is_ET6K)
	pTseng->FbMapSize = 16384 * 1024;
    else
	pTseng->FbMapSize = 4096 * 1024;

    xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	(unsigned long)pTseng->LinFbAddress);

    return TRUE;
}

static Bool
TsengPreInit(ScrnInfoPtr pScrn, int flags)
{
    TsengPtr pTseng;
    MessageType from;
    int i;

    if (flags & PROBE_DETECT) return FALSE;

    PDEBUG("	TsengPreInit\n");
    
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

    /* The vgahw module should be loaded here when needed */
    
    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1) 
	    return FALSE;

    /* Allocate the TsengRec driverPrivate */
    if (!TsengGetRec(pScrn)) {
	return FALSE;
    }
    pTseng = TsengPTR(pScrn);

    /* This is the general case */
    pTseng->pEnt = xf86GetEntityInfo(*pScrn->entityList);

#if 1
    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86Int10InfoPtr pInt;
	xf86LoaderReqSymLists(int10Symbols, NULL);
#if 1
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,"initializing int10\n");
	pInt = xf86InitInt10(pTseng->pEnt->index);
	xf86FreeInt10(pInt);
#endif
    }
#endif
    
    if (!xf86LoadSubModule(pScrn, "vgahw"))
	return FALSE;
    xf86LoaderReqSymLists(vgaHWSymbols, NULL);
    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn))
	return FALSE;

    vgaHWGetIOBase(VGAHWPTR(pScrn));
    /*
     * Since, the capabilities are determined by the chipset, the very first
     * thing to do is to figure out the chipset and its capabilities.
     */

    TsengUnlock();

    /*
     * Find the bus slot for this screen (PCI or other). This also finds the
     * exact chipset type.
     */
    /* This driver can handle ISA and PCI buses */
    if (pTseng->pEnt->location.type == BUS_PCI) {
	pTseng->PciInfo = xf86GetPciInfoForEntity(pTseng->pEnt->index);
	if (!TsengPreInitPCI(pScrn)) {
	    TsengFreeRec(pScrn);
	    return FALSE;
	}
    } else if (!TsengPreInitNoPCI(pScrn)) {
	TsengFreeRec(pScrn);
	return FALSE;
    }

    /*
     * Find RAMDAC and CLOCKCHIP type and fill Tsengdac struct
     */
    if (!Check_Tseng_Ramdac(pScrn))
	return FALSE;

    /* check for clockchip */
    if (!Tseng_check_clockchip(pScrn)) {
	return FALSE;
    }
    /*
     * Now we can check what depth we support.
     */

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * Our default depth is 8, so pass it to the helper function.
     * Our preference for depth 24 is 24bpp, so tell it that too.
     */
    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, Support24bppFb | Support32bppFb |
				SupportConvert32to24 | PreferConvert32to24)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	Bool CanDo16bpp = FALSE, CanDo24bpp = FALSE, CanDo32bpp = FALSE;
	Bool CanDoThis = FALSE;

	switch (pTseng->DacInfo.DacType) {
	case ET6000_DAC:
	case ICS5341_DAC:
	case STG1703_DAC:
	case STG1702_DAC:
	    CanDo16bpp = TRUE;
	    CanDo24bpp = TRUE;
	    CanDo32bpp = TRUE;
	    break;
	case ATT20C490_DAC:
	case ATT20C491_DAC:
	case ATT20C492_DAC:
	case ATT20C493_DAC:
	case ICS5301_DAC:
	case MUSIC4910_DAC:
	    CanDo16bpp = TRUE;
	    CanDo24bpp = TRUE;
	    break;
	case CH8398_DAC:
	    CanDo16bpp = TRUE;
	    CanDo24bpp = TRUE;
	    break;
	case STG1700_DAC:	       /* can't do packed 24bpp over a 16-bit bus */
	    CanDo16bpp = TRUE;	       /* FIXME: can do it over 8 bit bus */
	    CanDo32bpp = TRUE;
	    break;
	default:		       /* default: only 1, 4, 8 bpp */
	    break;
	}

	switch (pScrn->depth) {
	case 1:
	case 4:
	case 8:
	    CanDoThis = TRUE;
	    break;
	case 16:
	    CanDoThis = CanDo16bpp;
	    break;
	case 24:
	    CanDoThis = (CanDo24bpp || CanDo32bpp);
	    break;
	}
	if (!CanDoThis) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Given depth (%d) is not supported by this chipset/RAMDAC\n",
		pScrn->depth);
	    return FALSE;
	}
	if ((pScrn->bitsPerPixel == 32) && (!CanDo32bpp)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Given bpp (%d) is not supported by this chipset/RAMDAC\n",
		pScrn->bitsPerPixel);
	    return FALSE;
	}
    }
    xf86PrintDepthBpp(pScrn);

    /* Get the depth24 pixmap format */
    if (pScrn->depth == 24 && pix24bpp == 0)
	pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    if (pScrn->bitsPerPixel > 8)
	pTseng->Bytesperpixel = pScrn->bitsPerPixel / 8;
    else
	pTseng->Bytesperpixel = 1;  /* this is fake for < 8bpp, but simplifies other code */

    /* hardware limits */
    pScrn->maxHValue = Tseng_HMAX;
    pScrn->maxVValue = Tseng_VMAX;

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */

    /* Set weight/mask/offset for depth > 8 */
    if (pScrn->depth > 8) {
	/* The defaults are OK for us */
	rgb zeros = {0, 0, 0};
	rgb mask;
	
	/* 
	 * Initialize mask here. 
	 * Currently we only treat the ICS5341 RAMDAC special
	 */
	if ((pScrn->depth == 24) && (pScrn->bitsPerPixel == 24))
	    mask = pTseng->DacInfo.rgb24packed;
	else
	    mask = zeros;

	if (!xf86SetWeight(pScrn, zeros, mask)) {
	    return FALSE;
	} else {
	    /* XXX check that weight returned is supported */
	    ;
	}
    }
    /* Set the default visual. */
    if (!xf86SetDefaultVisual(pScrn, -1)) 
	return FALSE;

    /* The gamma fields must be initialised when using the new cmap code */
    if (pScrn->depth > 1) {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8) {
	/* Default to 6, because most Tseng chips/RAMDACs don't support it */
	pScrn->rgbBits = 6;
    }
    if (!TsengProcessOptions(pScrn))   /* must be done _after_ we know what chip this is */
	return FALSE;

    if (pTseng->UseLinMem) {
	if (!TsengGetLinFbAddress(pScrn))
	    return FALSE;
    }

    if (pTseng->UseAccel)
	VGAHWPTR(pScrn)->MapSize = 0x20000;  /* accelerator apertures and MMIO */
    else
	VGAHWPTR(pScrn)->MapSize = 0x10000;

    if (pTseng->UseLinMem) {
	/*
	 * XXX At least part of this range does appear to be disabled,
	 * but to play safe, it is marked as "unused" for now.
	 * Changed this to "disable". Otherwise it might interfere with DGA.
	 */
	xf86SetOperatingState(resVgaMem, pTseng->pEnt->index, ResDisableOpr);
    }
    
    /* hibit processing (TsengProcessOptions() must have been called first) */
    pTseng->save_divide = 0x40;	       /* default */
    if (!Is_ET6K) {
	if (!TsengProcessHibit(pScrn))
	    return FALSE;
    }
    /*
     * If the user has specified the amount of memory in the XF86Config
     * file, we respect that setting.
     */
    if (pTseng->pEnt->device->videoRam != 0) {
	pScrn->videoRam = pTseng->pEnt->device->videoRam;
	from = X_CONFIG;
    } else {
	from = X_PROBED;
	pScrn->videoRam = TsengDetectMem(pScrn);
    }
    pScrn->videoRam = TsengLimitMem(pScrn, pScrn->videoRam);

    xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d kByte.\n",
	pScrn->videoRam);

    /* do all clock-related setup */
    tseng_clock_setup(pScrn);
    
    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our TsengValidMode() already takes
     * care of this, we don't worry about setting them here.
     */

    /* Select valid modes from those available */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
	pScrn->display->modes, pTseng->clockRange[0],
	NULL, 32, pScrn->maxHValue, 8*pTseng->Bytesperpixel, /* H limits */
	0, pScrn->maxVValue,	       /* V limits */
	pScrn->display->virtualX,
	pScrn->display->virtualY,
	pTseng->FbMapSize,
	LOOKUP_BEST_REFRESH);	       /* LOOKUP_CLOSEST_CLOCK | LOOKUP_CLKDIV2 when no programmable clock ? */

    if (i == -1) {
	TsengFreeRec(pScrn);
	return FALSE;
    }
    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	TsengFreeRec(pScrn);
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

    /* Load bpp-specific modules */
    switch (pScrn->bitsPerPixel) {
    case 1:
	if (xf86LoadSubModule(pScrn, "xf1bpp") == NULL) {
	  TsengFreeRec(pScrn);
	  return FALSE;
	}
	xf86LoaderReqSymbols("xf1bppScreenInit", NULL);
	break;
    case 4:
	if (xf86LoadSubModule(pScrn, "xf4bpp") == NULL) {
	  TsengFreeRec(pScrn);
	  return FALSE;
	}
	xf86LoaderReqSymbols("xf4bppScreenInit", NULL);
	break;
    default:
	if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	  TsengFreeRec(pScrn);
	  return FALSE;
	}
	xf86LoaderReqSymLists(fbSymbols, NULL);
	break;
    }

    /* Load XAA if needed */
    if (pTseng->UseAccel) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    TsengFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }
    /* Load ramdac if needed */
    if (pTseng->HWCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    TsengFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }
/*    TsengLock(); */

    return TRUE;
}

static void 
TsengSetupAccelMemory(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);
    int offscreen_videoram, videoram_end, req_videoram;
    int i;
    int v;

    /* XXX Hack to suppress messages in subsequent generations. */
    if (serverGeneration == 1)
	v = 1;
    else
	v = 100;
    /*
     * The accelerator requires free off-screen video memory to operate. The
     * more there is, the more it can accelerate.
     */

    videoram_end = pScrn->videoRam * 1024;
    offscreen_videoram = videoram_end -
	pScrn->displayWidth * pScrn->virtualY * pTseng->Bytesperpixel;
    xf86DrvMsgVerb(scrnIndex, X_INFO, v, "Available off-screen memory: %d bytes.\n",
	offscreen_videoram);

    /*
     * The HW cursor requires 1kb of off-screen memory, aligned to 1kb
     * (256 DWORDS). Setting up its memory first ensures the alignment.
     */
    if (pTseng->HWCursor) {
	req_videoram = 1024;
	if (offscreen_videoram < req_videoram) {
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_WARNING, v,
		"Hardware Cursor disabled. It requires %d bytes of free video memory\n",
		req_videoram);
	    pTseng->HWCursor = FALSE;
	    pTseng->HWCursorBufferOffset = 0;
	} else {
	    offscreen_videoram -= req_videoram;
	    videoram_end -= req_videoram;
	    pTseng->HWCursorBufferOffset = videoram_end;
	}
    } else {
	pTseng->HWCursorBufferOffset = 0;
    }

    /*
     * Acceleration memory setup. Do this only if acceleration is enabled.
     */
    if (!pTseng->UseAccel) return;

    /*
     * Basic acceleration needs storage for FG, BG and PAT colors in
     * off-screen memory. Each color requires 2(ping-pong)*8 bytes.
     */
    req_videoram = 2 * 8 * 3;
    if (offscreen_videoram < req_videoram) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_WARNING, v,
	    "Acceleration disabled. It requires AT LEAST %d bytes of free video memory\n",
	    req_videoram);
	pTseng->UseAccel = FALSE;
	pTseng->AccelColorBufferOffset = 0;
	goto end_memsetup;	      /* no basic acceleration means none at all */
    } else {
	offscreen_videoram -= req_videoram;
	videoram_end -= req_videoram;
	pTseng->AccelColorBufferOffset = videoram_end;
    }

    /*
     * Color expansion (using triple buffering) requires 3 non-expanded
     * scanlines, DWORD padded.
     */
    req_videoram = 3 * ((pScrn->virtualX + 31) / 32) * 4;
    if (offscreen_videoram < req_videoram) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_WARNING, v,
	    "Accelerated color expansion disabled (%d more bytes of free video memory required)\n",
	    req_videoram - offscreen_videoram);
	pTseng->AccelColorExpandBufferOffsets[0] = 0;
    } else {
	offscreen_videoram -= req_videoram;
	for (i = 0; i < 3; i++) {
	    videoram_end -= req_videoram / 3;
	    pTseng->AccelColorExpandBufferOffsets[i] = videoram_end;
	}
    }

    /*
     * XAA ImageWrite support needs two entire line buffers. The
     * current code assumes buffer 1 lies in the same 8kb aperture as
     * buffer 0.
     *
     * [ FIXME: aren't we forgetting the DWORD padding here ? ]
     * [ FIXME: why here double-buffering and in colexp triple-buffering? ]
     */
    req_videoram = 2 * (pScrn->virtualX * pTseng->Bytesperpixel);
    /* banked mode uses an 8kb aperture for imagewrite */
    if ((req_videoram > 8192) && (!pTseng->UseLinMem)) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_WARNING, v,
	    "Accelerated ImageWrites disabled (banked %dbpp virtual width must be <= %d)\n",
	    pScrn->bitsPerPixel, 8192 / (2 * pTseng->Bytesperpixel));
	pTseng->AccelImageWriteBufferOffsets[0] = 0;
    } else if (offscreen_videoram < req_videoram) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_WARNING, v,
	    "Accelerated ImageWrites disabled (%d more bytes of free video memory required)\n",
	    req_videoram - offscreen_videoram);
	pTseng->AccelImageWriteBufferOffsets[0] = 0;
    } else {
	offscreen_videoram -= req_videoram;
	for (i = 0; i < 2; i++) {
	    videoram_end -= req_videoram / 2;
	    pTseng->AccelImageWriteBufferOffsets[i] = videoram_end;
	}
    }

    xf86DrvMsgVerb(scrnIndex, X_INFO, v,
	"Remaining off-screen memory available for pixmap cache: %d bytes.\n",
	offscreen_videoram);

end_memsetup:
    pScrn->videoRam = videoram_end / 1024;
}

static Bool
TsengScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    TsengPtr pTseng;
    int ret;
    VisualPtr visual;
    
    PDEBUG("	TsengScreenInit\n");

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    pTseng = TsengPTR(pScrn);
    /* Map the Tseng memory areas */
    if (!TsengMapMem(pScrn))
	return FALSE;

    /* Save the current state */
    TsengSave(pScrn);

    /* Initialise the first mode */
    TsengModeInit(pScrn, pScrn->currentMode);

    /* Darken the screen for aesthetic reasons and set the viewport */
    TsengSaveScreen(pScreen, SCREEN_SAVER_ON);

    TsengAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    /* XXX Fill the screen with black */

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
    if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth), 
			  pScrn->rgbBits, pScrn->defaultVisual))
      return FALSE;

    miSetPixmapDepths ();

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    switch (pScrn->bitsPerPixel) {
    case 1:
	ret = xf1bppScreenInit(pScreen, pTseng->FbBase,
			pScrn->virtualX, pScrn->virtualY,
			pScrn->xDpi, pScrn->yDpi,
			pScrn->displayWidth);
	break;
    case 4:
	ret = xf4bppScreenInit(pScreen, pTseng->FbBase,
			pScrn->virtualX, pScrn->virtualY,
			pScrn->xDpi, pScrn->yDpi,
			pScrn->displayWidth);
	break;
    default:
        ret  = fbScreenInit(pScreen, pTseng->FbBase,
			pScrn->virtualX, pScrn->virtualY,
			pScrn->xDpi, pScrn->yDpi,
			pScrn->displayWidth, pScrn->bitsPerPixel);
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
    if (pScrn->bitsPerPixel > 4)
	fbPictureInit(pScreen, 0, 0);

    if (pScrn->depth >= 8)
        TsengDGAInit(pScreen);

    /*
     * If banking is needed, initialise an miBankInfoRec (defined in
     * "mibank.h"), and call miInitializeBanking().
     */

    if (!pTseng->UseLinMem) {
	if (!Is_stdET4K && (pScrn->videoRam > 1024)) {
	    pTseng->BankInfo.SetSourceBank = ET4000W32SetRead;
	    pTseng->BankInfo.SetDestinationBank = ET4000W32SetWrite;
	    pTseng->BankInfo.SetSourceAndDestinationBanks = ET4000W32SetReadWrite;
	} else {
	    pTseng->BankInfo.SetSourceBank = ET4000SetRead;
	    pTseng->BankInfo.SetDestinationBank = ET4000SetWrite;
	    pTseng->BankInfo.SetSourceAndDestinationBanks = ET4000SetReadWrite;
	}
	pTseng->BankInfo.pBankA = pTseng->FbBase;
	pTseng->BankInfo.pBankB = pTseng->FbBase;
	pTseng->BankInfo.BankSize = 0x10000;
	pTseng->BankInfo.nBankDepth = (pScrn->depth == 4) ? 1 : pScrn->depth;

	if (!miInitializeBanking(pScreen, pScrn->virtualX, pScrn->virtualY,
		pScrn->displayWidth, &pTseng->BankInfo)) {
	    return FALSE;
	}
    }

    /*
     * Initialize the acceleration interface.
     */
    TsengSetupAccelMemory(scrnIndex, pScreen);
    if (pTseng->UseAccel) {
	tseng_init_acl(pScrn);	/* set up accelerator */
	if (!TsengXAAInit(pScreen)) {	/* set up XAA interface */
	    return FALSE;
	}
    }

    miInitializeBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);
    /* Initialise cursor functions */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Hardware Cursor layer */
    if (pTseng->HWCursor) {
	if (!TsengHWCursorInit(pScreen))
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Hardware cursor initialization failed\n");
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    if (pScrn->depth == 4 || pScrn->depth == 8) { /* fb and xf4bpp */
	vgaHWHandleColormaps(pScreen);
    }
    pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
    pScrn->racMemFlags = pScrn->racIoFlags;

    /* Wrap the current CloseScreen and SaveScreen functions */
    pScreen->SaveScreen = TsengSaveScreen;

    /* Support for DPMS, the ET4000W32Pc and newer uses a different and
     * simpler method than the older cards.
     */
    if (Is_W32p_cd || Is_ET6K) {
	xf86DPMSInit(pScreen, (DPMSSetProcPtr)TsengCrtcDPMSSet, 0);
    } else {
	xf86DPMSInit(pScreen, (DPMSSetProcPtr)TsengHVSyncDPMSSet, 0);
    }

    pTseng->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = TsengCloseScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }
    /* Done */
    return TRUE;
}

static Bool
TsengEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengEnterVT\n");

    vgaHWUnlock(VGAHWPTR(pScrn));
    TsengUnlock();

    if (!TsengModeInit(pScrn, pScrn->currentMode))
        return FALSE;
    if (pTseng->UseAccel) {
	tseng_init_acl(pScrn);	/* set up accelerator */
    }
    return TRUE;
}

static void
TsengLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengLeaveVT\n");
    TsengRestore(pScrn, &(VGAHWPTR(pScrn)->SavedReg),
		 &pTseng->SavedReg,VGA_SR_ALL);

    TsengLock();
    vgaHWLock(VGAHWPTR(pScrn));
}

static Bool
TsengCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengCloseScreen\n");

    if (pScrn->vtSema) {
    TsengRestore(pScrn, &(VGAHWPTR(pScrn)->SavedReg),
		 &(pTseng->SavedReg),VGA_SR_ALL);
    TsengUnmapMem(pScrn);
    }
    if (pTseng->AccelInfoRec)
	XAADestroyInfoRec(pTseng->AccelInfoRec);
    if (pTseng->CursorInfoRec)
	xf86DestroyCursorInfoRec(pTseng->CursorInfoRec);

    pScrn->vtSema = FALSE;

    pScreen->CloseScreen = pTseng->CloseScreen;
    return (*pScreen->CloseScreen) (scrnIndex, pScreen);
}

/*
 * SaveScreen --
 *
 *   perform a sequencer reset.
 *
 * The ET4000 "Video System Configuration 1" register (CRTC index 0x36),
 * which is used to set linear memory mode and MMU-related stuff, is
 * partially reset to "0" when TS register index 0 bit 1 is set (synchronous
 * reset): bits 3..5 are reset during a sync. reset.
 *
 * We therefor do _not_ call vgaHWSaveScreen here, since it does a sequencer
 * reset. Instead, we do the same as in vgaHWSaveScreen except for the seq. reset.
 *
 * If this is not done, the higher level code will not be able to access the
 * framebuffer (because it is temporarily in banked mode instead of linear
 * mode) as long as SaveScreen is active (=in between a
 * SaveScreen(FALSE)/SaveScreen(TRUE) pair)
 */

static Bool
TsengSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);
    Bool unblank;

    PDEBUG("	TsengSaveScreen\n");

    unblank = xf86IsUnblank(mode);

    if (Is_ET6K) {
	return vgaHWSaveScreen(pScreen, unblank);
    } else {
       if (unblank)
	  SetTimeSinceLastInputEvent();

       if (pScrn->vtSema) {
           TsengBlankScreen(pScrn, unblank);
       }
       return (TRUE);
    }
}

static Bool
TsengMapMem(ScrnInfoPtr pScrn)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengMapMem\n");

    /* Map the VGA memory */

    if (!vgaHWMapMem(pScrn)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    "Could not mmap standard VGA memory aperture.\n");
	return FALSE;
    }

    if (pTseng->UseLinMem) {
	pTseng->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
	    pTseng->PciTag,
	    (unsigned long)pTseng->LinFbAddress,
	    pTseng->FbMapSize);
	if (pTseng->FbBase == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Could not mmap linear video memory.\n");
	    return FALSE;
	}
	if (pTseng->UseAccel) {
	  pTseng->MMioBase = xf86MapPciMem(pScrn->scrnIndex, 
					   VIDMEM_MMIO,
					   pTseng->PciTag,
					   (unsigned long)pTseng->LinFbAddress,
					   pTseng->FbMapSize);
	  if (!pTseng->MMioBase) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Could not mmap mmio memory.\n");
	    return FALSE;
	  }
	  pTseng->MMioBase += 0x3FFF00L;
	}
    } else {
        vgaHWPtr hwp = VGAHWPTR(pScrn);
	pTseng->FbBase = hwp->Base;
	if (pTseng->UseAccel) {
	    pTseng->MMioBase = xf86MapPciMem(pScrn->scrnIndex, 
					     VIDMEM_MMIO,
					     pTseng->PciTag,
					     (unsigned long)hwp->MapPhys,
					     hwp->MapSize);
	    if (!pTseng->MMioBase) {
	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			 "Could not mmap mmio memory.\n");
	      return FALSE;
	    }
	    pTseng->MMioBase += 0x1FF00L;
	}
    }
    
    if (pTseng->FbBase == NULL)
	return FALSE;

    return TRUE;
}

static Bool
TsengUnmapMem(ScrnInfoPtr pScrn)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	TsengUnmapMem\n");

    if (pTseng->UseLinMem) {
	xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pTseng->FbBase, pTseng->FbMapSize);
    }
    vgaHWUnmapMem(pScrn);

    pTseng->FbBase = NULL;

    return TRUE;
}

static void
TsengFreeScreen(int scrnIndex, int flags)
{
    PDEBUG("	TsengFreeScreen\n");
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    TsengFreeRec(xf86Screens[scrnIndex]);
}

Bool
TsengModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp;
    TsengPtr pTseng = TsengPTR(pScrn);
    TsengRegPtr new = &(pTseng->ModeReg);
    TsengRegPtr initial = &(pTseng->SavedReg);
    int row_offset;
    int min_n2;
    int hdiv = 1, hmul = 1;

    PDEBUG("	TsengModeInit\n");

    switch (mode->PrivFlags) {
	case TSENG_MODE_PIXMUX:
	case TSENG_MODE_DACBUS16:
	    hdiv = pTseng->clockRange[1]->ClockDivFactor;
	    hmul = pTseng->clockRange[1]->ClockMulFactor;
	    break;
	default:
	    hdiv = pTseng->clockRange[0]->ClockDivFactor;
	    hmul = pTseng->clockRange[0]->ClockMulFactor;
    }

    /*
     * Modify mode timings accordingly
     */
    if (!mode->CrtcHAdjusted) {
	/* now divide and multiply the horizontal timing parameters as required */
	mode->CrtcHTotal = (mode->CrtcHTotal * hmul) / hdiv;
	mode->CrtcHDisplay = (mode->CrtcHDisplay * hmul) / hdiv;
	mode->CrtcHSyncStart = (mode->CrtcHSyncStart * hmul) / hdiv;
	mode->CrtcHSyncEnd = (mode->CrtcHSyncEnd * hmul) / hdiv;
	mode->CrtcHBlankStart = (mode->CrtcHBlankStart * hmul) / hdiv;
	mode->CrtcHBlankEnd = (mode->CrtcHBlankEnd * hmul) / hdiv;
	mode->CrtcHSkew = (mode->CrtcHSkew * hmul) / hdiv;
	if (pScrn->bitsPerPixel == 24) {
	    int rgb_skew;

	    /*
	     * in 24bpp, the position of the BLANK signal determines the
	     * phase of the R,G and B values. XFree86 sets blanking equal to
	     * the Sync, so setting the Sync correctly will also set the
	     * BLANK corectly, and thus also the RGB phase
	     */
	    rgb_skew = (mode->CrtcHTotal / 8 - mode->CrtcHBlankEnd / 8 - 1) % 3;
	    mode->CrtcHBlankEnd += rgb_skew * 8 + 24;
	    /* HBlankEnd must come BEFORE HTotal */
	    if (mode->CrtcHBlankEnd > mode->CrtcHTotal)
		mode->CrtcHBlankEnd -= 24;
	}
	mode->CrtcHAdjusted = TRUE;
    }
    
    /* set clockIndex to "2" for programmable clocks */
    if (pScrn->progClock)
	mode->ClockIndex = 2;

    /* prepare standard VGA register contents */
    hwp = VGAHWPTR(pScrn);
    if (!vgaHWInit(pScrn, mode))
	return (FALSE);
    pScrn->vtSema = TRUE;

    /* prepare extended (Tseng) register contents */
    /* 
     * Start by copying all the saved registers in the "new" data, so we
     * only have to modify those that need to change.
     */

    memcpy(new, initial, sizeof(TsengRegRec));

    if (pScrn->bitsPerPixel < 8) {
	/* Don't ask me why this is needed on the ET6000 and not on the others */
	if (Is_ET6K)
	    hwp->ModeReg.Sequencer[1] |= 0x04;
	row_offset = hwp->ModeReg.CRTC[19];
    } else {
	hwp->ModeReg.Attribute[16] = 0x01;	/* use the FAST 256 Color Mode */
	row_offset = pScrn->displayWidth >> 3;	/* overruled by 16/24/32 bpp code */
    }

    hwp->ModeReg.CRTC[20] = 0x60;
    hwp->ModeReg.CRTC[23] = 0xAB;
    new->ExtTS[6] = 0x00;
    new->ExtTS[7] = 0xBC;
    new->ExtCRTC[0x33] = 0x00;

    new->ExtCRTC[0x35] = (mode->Flags & V_INTERLACE ? 0x80 : 0x00)
	| 0x10
	| ((mode->CrtcVSyncStart & 0x400) >> 7)
	| (((mode->CrtcVDisplay - 1) & 0x400) >> 8)
	| (((mode->CrtcVTotal - 2) & 0x400) >> 9)
	| (((mode->CrtcVBlankStart - 1) & 0x400) >> 10);

    if (pScrn->bitsPerPixel < 8)
	new->ExtATC = 0x00;
    else
	new->ExtATC = 0x80;

    if (pScrn->bitsPerPixel >= 8) {
	if (pTseng->FastDram && !Is_ET6K) {
	    /*
	     *  make sure Trsp is no more than 75ns
	     *            Tcsw is 25ns
	     *            Tcsp is 25ns
	     *            Trcd is no more than 50ns
	     * Timings assume SCLK = 40MHz
	     *
	     * Note, this is experimental, but works for me (DHD)
	     */
	    /* Tcsw, Tcsp, Trsp */
	    new->ExtCRTC[0x32] &= ~0x1F;
	    if (new->ExtCRTC[0x32] & 0x18)
		new->ExtCRTC[0x32] |= 0x08;
	    /* Trcd */
	    new->ExtCRTC[0x32] &= ~0x20;
	}
    }
    /*
     * Here we make sure that CRTC regs 0x34 and 0x37 are untouched, except for 
     * some bits we want to change. 
     * Notably bit 7 of CRTC 0x34, which changes RAS setup time from 4 to 0 ns 
     * (performance),
     * and bit 7 of CRTC 0x37, which changes the CRTC FIFO low treshold control.
     * At really high pixel clocks, this will avoid lots of garble on the screen 
     * when something is being drawn. This only happens WAY beyond 80 MHz 
     * (those 135 MHz ramdac's...)
     */
    if (Is_W32i || Is_W32p) {
	if (!pTseng->SlowDram)
	    new->ExtCRTC[0x34] = (new->ExtCRTC[0x34] & 0x7F) | 0x80;
	if ((mode->Clock * pTseng->Bytesperpixel) > 80000)
	    new->ExtCRTC[0x37] = (new->ExtCRTC[0x37] & 0x7f) | 0x80;
	/*
	 * now on to the memory interleave setting (CR32 bit 7)
	 */
	if (pTseng->SetW32Interleave) {
	    if (pTseng->W32Interleave)
		new->ExtCRTC[0x32] |= 0x80;
	    else
		new->ExtCRTC[0x32] &= 0x7F;
	}
    }
    if (Is_W32p) {
	/*
	 * CR34 bit 4 controls the PCI Burst option
	 */
	if (pTseng->SetPCIBurst) {
	    if (pTseng->PCIBurst)
		new->ExtCRTC[0x34] |= 0x10;
	    else
		new->ExtCRTC[0x34] &= 0xEF;
	}
    }

    /* prepare clock-related registers when not Legend.
     * cannot really SET the clock here yet, since the ET4000Save()
     * is called LATER, so it would save the wrong state...
     * ET4000Restore() is used to actually SET vga regs.
     */

    if (STG170x_programmable_clock || Gendac_programmable_clock) {
	if (mode->PrivFlags == TSENG_MODE_PIXMUX)
	    /* pixmux requires a post-div of 4 on ICS GenDAC clock generator */
	    min_n2 = 2;
	else
	    min_n2 = 0;
	TsengcommonCalcClock(mode->SynthClock, 1, 1, 31, min_n2, 3,
	    100000, pTseng->max_vco_freq,
	    &(new->pll.f2_M), &(new->pll.f2_N));

	new->pll.w_idx = 0;
	new->pll.r_idx = 0;

	/* memory clock */
	if (Gendac_programmable_clock && pTseng->MClkInfo.Set) {
	    TsengcommonCalcClock(pTseng->MClkInfo.MemClk, 1, 1, 31, 1, 3, 100000, pTseng->MaxClock * 2 + 1,
		&(new->pll.MClkM), &(new->pll.MClkN));
	}
    } else if (ICD2061a_programmable_clock) {
#ifdef TODO
	/* FIXME: icd2061_dwv not used anywhere ... */
	pTseng->icd2061_dwv = AltICD2061CalcClock(mode->SynthClock * 1000);
	/* Tseng_ICD2061AClockSelect(mode->SynthClock); */
#endif
    } else if (CH8398_programmable_clock) {
#ifdef TODO
	Chrontel8391CalcClock(mode->SynthClock, &temp1, &temp2, &temp3);
	new->pll.f2_N = (unsigned char)(temp2);
	new->pll.f2_M = (unsigned char)(temp1 | (temp3 << 6));
	/* ok LSB=f2_N and MSB=f2_M            */
	/* now set the Clock Select Register(CSR)      */
	new->pll.ctrl = (new->pll.ctrl | 0x90) & 0xF0;
	new->pll.timingctrl &= 0x1F;
	new->pll.r_idx = 0;
	new->pll.w_idx = 0;
#endif
    } else if (Is_ET6K) {
	/* setting min_n2 to "1" will ensure a more stable clock ("0" is allowed though) */
	TsengcommonCalcClock(mode->SynthClock, 1, 1, 31, 1, 3, 100000,
	    pTseng->max_vco_freq,
	    &(new->pll.f2_M), &(new->pll.f2_N));
	/* above 130MB/sec, we enable the "LOW FIFO threshold" */
	if (mode->Clock * pTseng->Bytesperpixel > 130000) {
	    new->ExtET6K[0x41] |= 0x10;
	    if (Is_ET6100)
		new->ExtET6K[0x46] |= 0x04;
	} else {
	    new->ExtET6K[0x41] &= ~0x10;
	    if (Is_ET6100)
		new->ExtET6K[0x46] &= ~0x04;
	}

	if (pTseng->MClkInfo.Set) {
	    /* according to Tseng Labs, N1 must be <= 4, and N2 should always be 1 for MClk */
	    TsengcommonCalcClock(pTseng->MClkInfo.MemClk, 1, 1, 4, 1, 1,
		100000, pTseng->MaxClock * 2,
		&(new->pll.MClkM), &(new->pll.MClkN));
	}
	/* 
	 * Even when we don't allow setting the MClk value as described
	 * above, we can use the FAST/MED/SLOW DRAM options to set up
	 * the RAS/CAS delays as decided by the value of ExtET6K[0x44].
	 * This is also a more correct use of the flags, as it describes
	 * how fast the RAM works. [HNH].
	 */
	if (pTseng->FastDram)
	    new->ExtET6K[0x44] = 0x04; /* Fastest speed(?) */
	else if (pTseng->MedDram)
	    new->ExtET6K[0x44] = 0x15; /* Medium speed */
	else if (pTseng->SlowDram)
	    new->ExtET6K[0x44] = 0x35; /* Slow speed */
	else
	    ;		               /* keep current value */
    }
    /*
     * Set the clock selection bits. Because of the odd mapping between
     * Tseng clock select bits and what XFree86 does, "CSx" refers to a
     * register bit with the same name in the Tseng data books.
     *
     * XFree86 uses the following mapping:
     *
     *  Tseng register bit name		XFree86 clock select bit
     *	    CS0				    0
     *      CS1				    1
     *      CS2				    2
     *      MCLK/2			    3
     *      CS3				    4
     *      CS4				    not used
     */
    if (mode->ClockIndex >= 0) {
	/* CS0 and CS1 are set by standard VGA code (vgaHW) */
	/* CS2 = CRTC 0x34 bit 1 */
	new->ExtCRTC[0x34] = (new->ExtCRTC[0x34] & 0xFD) |
	    ((mode->ClockIndex & 0x04) >> 1);
	/* for programmable clocks: disable MCLK/2 and MCLK/4 independent of hibit */
	new->ExtTS[7] = (new->ExtTS[7] & 0xBE);
	if (!pScrn->progClock) {
	    /* clock select bit 3 = MCLK/2 disable/enable */
	    new->ExtTS[7] |= (pTseng->save_divide ^ ((mode->ClockIndex & 0x08) << 3));
	}
	/* clock select bit 4 = CS3 , clear CS4 */
	new->ExtCRTC[0x31] = ((mode->ClockIndex & 0x10) << 2) | (new->ExtCRTC[0x31] & 0x3F);
    }
    /*
     * linear mode handling
     */

    if (Is_ET6K) {
	if (pTseng->UseLinMem) {
	    new->ExtET6K[0x13] = pTseng->LinFbAddress >> 24;
	    new->ExtET6K[0x40] |= 0x09;
	} else {
	    new->ExtET6K[0x40] &= ~0x09;
	}
    } else {			       /* et4000 style linear memory */
	if (pTseng->UseLinMem) {
	    new->ExtCRTC[0x36] |= 0x10;
	    if (Is_W32p || Is_ET6K)
		new->ExtCRTC[0x30] = (pTseng->LinFbAddress >> 22) & 0xFF;
	    else
		new->ExtCRTC[0x30] = ((pTseng->LinFbAddress >> 22) & 0x1F) ^ 0x1c;	/* invert bits 4..2 */
	    hwp->ModeReg.Graphics[6] &= ~0x0C;
	    new->ExtIMACtrl &= ~0x01;  /* disable IMA port (to get >1MB lin mem) */
	} else {
	    new->ExtCRTC[0x36] &= ~0x10;
	    if (pTseng->ChipType < TYPE_ET4000W32P)
		new->ExtCRTC[0x30] = 0x1C;	/* default value */
	    else
		new->ExtCRTC[0x30] = 0x00;
	}
    }

    /*
     * 16/24/32 bpp handling.
     */

    if (pScrn->bitsPerPixel >= 8) {
	tseng_set_ramdac_bpp(pScrn, mode);
	row_offset *= pTseng->Bytesperpixel;
    }
    /*
     * Horizontal overflow settings: for modes with > 2048 pixels per line
     */

    hwp->ModeReg.CRTC[19] = row_offset;
    new->ExtCRTC[0x3F] = ((((mode->CrtcHTotal >> 3) - 5) & 0x100) >> 8)
	| ((((mode->CrtcHDisplay >> 3) - 1) & 0x100) >> 7)
	| ((((mode->CrtcHBlankStart >> 3) - 1) & 0x100) >> 6)
	| (((mode->CrtcHSyncStart >> 3) & 0x100) >> 4)
	| ((row_offset & 0x200) >> 3)
	| ((row_offset & 0x100) >> 1);

    /*
     * Enable memory mapped IO registers when acceleration is needed.
     */

    if (pTseng->UseAccel) {
	if (Is_ET6K) {
	    if (pTseng->UseLinMem)
		new->ExtET6K[0x40] |= 0x02;	/* MMU can't be used here (causes system hang...) */
	    else
		new->ExtET6K[0x40] |= 0x06;	/* MMU is needed in banked accelerated mode */
	} else {
	    new->ExtCRTC[0x36] |= 0x28;
	}
    }
    vgaHWUnlock(hwp);		       /* TODO: is this needed (tsengEnterVT does this) */
    /* Program the registers */
    TsengRestore(pScrn, &hwp->ModeReg, new, VGA_SR_MODE);
    return TRUE;
}

static Bool
TsengSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    PDEBUG("	TsengSwitchMode\n");
    return TsengModeInit(xf86Screens[scrnIndex], mode);
}

/*
 * adjust the current video frame (viewport) to display the mousecursor.
 */
void
TsengAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TsengPtr pTseng = TsengPTR(pScrn);
    int iobase = VGAHWPTR(pScrn)->IOBase;
    int Base;

    PDEBUG("	TsengAdjustFrame\n");

    if (pTseng->ShowCache) {
	if (y)
	    y += 256;
    }
    if (pScrn->bitsPerPixel < 8)
	Base = (y * pScrn->displayWidth + x + 3) >> 3;
    else {
	Base = ((y * pScrn->displayWidth + x + 1) * pTseng->Bytesperpixel) >> 2;
	/* adjust Base address so it is a non-fractional multiple of pTseng->Bytesperpixel */
	Base -= (Base % pTseng->Bytesperpixel);
    }

    outw(iobase + 4, (Base & 0x00FF00) | 0x0C);
    outw(iobase + 4, ((Base & 0x00FF) << 8) | 0x0D);
    outw(iobase + 4, ((Base & 0x0F0000) >> 8) | 0x33);

}

static ModeStatus
TsengValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{

    PDEBUG("	TsengValidMode\n");

#ifdef FIXME
  is this needed? xf86ValidMode gets HMAX and VMAX variables, so it could deal with this.
  need to recheck hsize with mode->Htotal*mulFactor/divFactor
    /* Check for CRTC timing bits overflow. */
    if (mode->HTotal > Tseng_HMAX) {
	return MODE_BAD_HVALUE;
    }
    if (mode->VTotal > Tseng_VMAX) {
	return MODE_BAD_VVALUE;
    }
#endif

    return MODE_OK;
}

/*
 * TsengSave --
 *      save the current video mode
 */

static void
TsengSave(ScrnInfoPtr pScrn)
{
    unsigned char temp, saveseg1 = 0, saveseg2 = 0;
    TsengPtr pTseng = TsengPTR(pScrn);
    vgaRegPtr vgaReg;
    TsengRegPtr tsengReg;
    int iobase = VGAHWPTR(pScrn)->IOBase;

    PDEBUG("	TsengSave\n");

    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    tsengReg = &pTseng->SavedReg;

    /*
     * This function will handle creating the data structure and filling
     * in the generic VGA portion.
     */
    vgaHWSave(pScrn, vgaReg, VGA_SR_ALL);

    /*
     * we need this here , cause we MUST disable the ROM SYNC feature
     * this bit changed with W32p_rev_c...
     */
    outb(iobase + 4, 0x34);
    temp = inb(iobase + 5);
    tsengReg->ExtCRTC[0x34] = temp;
    if (Is_stdET4K || Is_W32_W32i || Is_W32p_ab) {
#ifdef OLD_CODE
	outb(iobase + 5, temp & 0x1F);
#else
	/* data books say translation ROM is controlled by bits 4 and 5 */
	outb(iobase + 5, temp & 0xCF);
#endif
    }

    saveseg1 = inb(0x3CD);
    outb(0x3CD, 0x00);		       /* segment select 1 */
    if (!Is_stdET4K) {
	saveseg2 = inb(0x3CB);
	outb(0x3CB, 0x00);	       /* segment select 2 */
    }
    tsengReg->ExtSegSel[0] = saveseg1;
    tsengReg->ExtSegSel[1] = saveseg2;

    outb(iobase + 4, 0x33);
    tsengReg->ExtCRTC[0x33] = inb(iobase + 5);
    outb(iobase + 4, 0x35);
    tsengReg->ExtCRTC[0x35] = inb(iobase + 5);
    if (Is_W32_any) {
	outb(iobase + 4, 0x36);
	tsengReg->ExtCRTC[0x36] = inb(iobase + 5);
	outb(iobase + 4, 0x37);
	tsengReg->ExtCRTC[0x37] = inb(iobase + 5);
	outb(0x217a, 0xF7);
	tsengReg->ExtIMACtrl = inb(0x217b);
    }
    if (!Is_ET6K) {
	outb(iobase + 4, 0x32);
	tsengReg->ExtCRTC[0x32] = inb(iobase + 5);
    }
    outb(0x3C4, 6);
    tsengReg->ExtTS[6] = inb(0x3C5);
    outb(0x3C4, 7);
    tsengReg->ExtTS[7] = inb(0x3C5);
    tsengReg->ExtTS[7] |= 0x14;
    temp = inb(iobase + 0x0A);	       /* reset flip-flop */
    outb(0x3C0, 0x36);
    tsengReg->ExtATC = inb(0x3C1);
    outb(0x3C0, tsengReg->ExtATC);
    if (DAC_is_GenDAC) {
	/* Save GenDAC Command and PLL registers */
	outb(iobase + 4, 0x31);
	temp = inb(iobase + 5);
	outb(iobase + 5, temp | 0x40);

	tsengReg->pll.cmd_reg = inb(0x3c6);	/* Enhanced command register */
	tsengReg->pll.w_idx = inb(0x3c8);	/* PLL write index */
	tsengReg->pll.r_idx = inb(0x3c7);	/* PLL read index */
	if (Gendac_programmable_clock) {
	    outb(0x3c7, 2);	       /* index to f2 reg */
	    tsengReg->pll.f2_M = inb(0x3c9);	/* f2 PLL M divider */
	    tsengReg->pll.f2_N = inb(0x3c9);	/* f2 PLL N1/N2 divider */
	    outb(0x3c7, 10);	       /* index to Mclk reg */
#ifdef TODO
	    tsengReg->MClkInfo.MClkM = inb(0x3c9);	/* MClk PLL M divider */
	    tsengReg->MClkInfo.MClkN = inb(0x3c9);	/* MClk PLL N1/N2 divider */
#endif
	}
	outb(0x3c7, 0x0e);	       /* index to PLL control */
	tsengReg->pll.ctrl = inb(0x3c9);	/* PLL control */
	outb(iobase + 4, 0x31);
	outb(iobase + 5, temp & ~0x40);
    }
    if ((pTseng->DacInfo.DacType == STG1702_DAC) || (pTseng->DacInfo.DacType == STG1703_DAC)
	|| (pTseng->DacInfo.DacType == STG1700_DAC)) {
#ifdef TODO
	/* Save STG 1703 GenDAC Command and PLL registers 
	 * unfortunately we reuse the gendac data structure, so the 
	 * field names are not really good.
	 */

	tseng_dactopel();
	tsengReg->pll.cmd_reg = tseng_getdaccomm();	/* Enhanced command register */
	if (STG170x_programmable_clock) {
	    tsengReg->pll.f2_M = STG1703getIndex(0x24);		/* f2 PLL M divider */
	    tsengReg->pll.f2_N = inb(0x3c6);	/* f2 PLL N1/N2 divider */
	}
	tsengReg->pll.ctrl = STG1703getIndex(0x03);	/* pixel mode select control */
	tsengReg->pll.timingctrl = STG1703getIndex(0x05);	/* pll timing control */
#endif
    }
    if (DAC_IS_CHRONTEL) {
	tseng_dactopel();
	tsengReg->pll.cmd_reg = tseng_getdaccomm();
	if (CH8398_programmable_clock) {
	    inb(0x3c8);
	    inb(0x3c6);
	    inb(0x3c6);
	    inb(0x3c6);
	    inb(0x3c6);
	    inb(0x3c6);
	    tsengReg->pll.timingctrl = inb(0x3c6);
	    /* Save PLL */
	    outb(iobase + 4, 0x31);
	    temp = inb(iobase + 5);
	    outb(iobase + 5, temp | (1 << 6));	/* set RS2 through CS3 */
	    /* We are in ClockRAM mode 0x3c7 = CRA, 0x3c8 = CWA, 0x3c9 = CDR */
	    tsengReg->pll.r_idx = inb(0x3c7);
	    tsengReg->pll.w_idx = inb(0x3c8);
	    outb(0x3c7, 10);
	    tsengReg->pll.f2_N = inb(0x3c9);
	    tsengReg->pll.f2_M = inb(0x3c9);
	    outb(0x3c7, tsengReg->pll.r_idx);
	    inb(0x3c8);		       /* loop to Clock Select Register */
	    inb(0x3c8);
	    inb(0x3c8);
	    inb(0x3c8);
	    tsengReg->pll.ctrl = inb(0x3c8);
	    outb(iobase + 4, 0x31);
	    outb(iobase + 5, temp);
	}
    }
    if (ET6000_programmable_clock) {
	/* Save ET6000 CLKDAC PLL registers */
	temp = inb(pTseng->IOAddress + 0x67);	/* remember old CLKDAC index register pointer */
	outb(pTseng->IOAddress + 0x67, 2);
	tsengReg->pll.f2_M = inb(pTseng->IOAddress + 0x69);
	tsengReg->pll.f2_N = inb(pTseng->IOAddress + 0x69);
	/* save MClk values */
	outb(pTseng->IOAddress + 0x67, 10);
	tsengReg->pll.MClkM = inb(pTseng->IOAddress + 0x69);
	tsengReg->pll.MClkN = inb(pTseng->IOAddress + 0x69);
	/* restore old index register */
	outb(pTseng->IOAddress + 0x67, temp);
    }
    if (DAC_IS_ATT49x)
	tsengReg->ATTdac_cmd = tseng_getdaccomm();

    if (Is_ET6K) {
	tsengReg->ExtET6K[0x13] = inb(pTseng->IOAddress + 0x13);
	tsengReg->ExtET6K[0x40] = inb(pTseng->IOAddress + 0x40);
	tsengReg->ExtET6K[0x58] = inb(pTseng->IOAddress + 0x58);
	tsengReg->ExtET6K[0x41] = inb(pTseng->IOAddress + 0x41);
	tsengReg->ExtET6K[0x44] = inb(pTseng->IOAddress + 0x44);
	tsengReg->ExtET6K[0x46] = inb(pTseng->IOAddress + 0x46);
    }
    outb(iobase + 4, 0x30);
    tsengReg->ExtCRTC[0x30] = inb(iobase + 5);
    outb(iobase + 4, 0x31);
    tsengReg->ExtCRTC[0x31] = inb(iobase + 5);
    outb(iobase + 4, 0x3F);
    tsengReg->ExtCRTC[0x3F] = inb(iobase + 5);
}

/*
 * TsengRestore --
 *      restore a video mode
 */

static void
TsengRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, TsengRegPtr tsengReg,
	     int flags)
{
    TsengPtr pTseng;
    unsigned char tmp;
    int iobase = VGAHWPTR(pScrn)->IOBase;

    PDEBUG("	TsengRestore\n");

    pTseng = TsengPTR(pScrn);

    TsengProtect(pScrn, TRUE);

    outb(0x3CD, 0x00);		       /* segment select bits 0..3 */
    if (!Is_stdET4K)
	outb(0x3CB, 0x00);	       /* segment select bits 4,5 */

    if (DAC_is_GenDAC) {
	/* Restore GenDAC Command and PLL registers */
	outb(iobase + 4, 0x31);
	tmp = inb(iobase + 5);
	outb(iobase + 5, tmp | 0x40);

	outb(0x3c6, tsengReg->pll.cmd_reg);	/* Enhanced command register */

	if (Gendac_programmable_clock) {
	    outb(0x3c8, 2);	       /* index to f2 reg */
	    outb(0x3c9, tsengReg->pll.f2_M);	/* f2 PLL M divider */
	    outb(0x3c9, tsengReg->pll.f2_N);	/* f2 PLL N1/N2 divider */
#ifdef TODO
	    if (pTseng->MClkInfo.Set) {
		outb(0x3c7, 10);                /* index to Mclk reg */
		outb(0x3c9, tsengReg->MClkM);	/* MClk PLL M divider */
		outb(0x3c9, tsengReg->MClkN);	/* MClk PLL N1/N2 divider */
	    }
#endif
	}
	outb(0x3c8, 0x0e);                      /* index to PLL control */
	outb(0x3c9, tsengReg->pll.ctrl);	/* PLL control */
	outb(0x3c8, tsengReg->pll.w_idx);	/* PLL write index */
	outb(0x3c7, tsengReg->pll.r_idx);	/* PLL read index */

	outb(iobase + 4, 0x31);
	outb(iobase + 5, tmp & ~0x40);
    }
    if (DAC_is_STG170x) {
#ifdef TODO
	/* Restore STG 170x GenDAC Command and PLL registers 
	 * we share one data structure with the gendac code, so the names
	 * are not too good.
	 */

	if (STG170x_programmable_clock) {
	    STG1703setIndex(0x24, tsengReg->pll.f2_M);
	    outb(0x3c6, tsengReg->pll.f2_N);	/* use autoincrement */
	}
	STG1703setIndex(0x03, tsengReg->pll.ctrl);	/* primary pixel mode */
	outb(0x3c6, tsengReg->pll.ctrl);	/* secondary pixel mode */
	outb(0x3c6, tsengReg->pll.timingctrl);	/* pipeline timing control */
	usleep(500);		       /* 500 usec PLL settling time required */

	STG1703magic(0);
	tseng_dactopel();
	tseng_setdaccomm(tsengReg->pll.cmd_reg);	/* write enh command reg */
#endif
    }
    if (DAC_IS_CHRONTEL) {
	tseng_dactopel();
	tseng_setdaccomm(tsengReg->pll.cmd_reg);
	inb(0x3c8);
	inb(0x3c6);
	inb(0x3c6);
	inb(0x3c6);
	inb(0x3c6);
	inb(0x3c6);
	outb(0x3c6, tsengReg->pll.timingctrl);
	if (CH8398_programmable_clock) {
	    outb(iobase + 4, 0x31);
	    tmp = inb(iobase + 5);
	    outb(iobase + 5, tmp | (1 << 6));		/* Set RS2 through CS3 */
	    /* We are in ClockRAM mode 0x3c7 = CRA, 0x3c8 = CWA, 0x3c9 = CDR */
	    outb(0x3c7, tsengReg->pll.r_idx);
	    outb(0x3c8, 10);
	    outb(0x3c9, tsengReg->pll.f2_N);
	    outb(0x3c9, tsengReg->pll.f2_M);
	    outb(0x3c8, tsengReg->pll.w_idx);
	    usleep(500);
	    inb(0x3c7);		       /* reset sequence */
	    inb(0x3c8);		       /* loop to Clock Select Register */
	    inb(0x3c8);
	    inb(0x3c8);
	    inb(0x3c8);
	    outb(0x3c8, tsengReg->pll.ctrl);
	    outb(iobase + 4, 0x31);
	    outb(iobase + 5, (tmp & 0x3F));
	}
    }
    if (ET6000_programmable_clock) {
	/* Restore ET6000 CLKDAC PLL registers */
	tmp = inb(pTseng->IOAddress + 0x67);	/* remember old CLKDAC index register pointer */
	outb(pTseng->IOAddress + 0x67, 2);
	outb(pTseng->IOAddress + 0x69, tsengReg->pll.f2_M);
	outb(pTseng->IOAddress + 0x69, tsengReg->pll.f2_N);
	/* set MClk values if needed, but don't touch them if not needed */
	if (pTseng->MClkInfo.Set) {
	    /*
	     * Since setting the MClk to highly illegal value results in a
	     * total system crash, we'd better play it safe here.
	     * N1 must be <= 4, and N2 should always be 1
	     */
	    if ((tsengReg->pll.MClkN & 0xf8) != 0x20) {
		xf86Msg(X_ERROR, "Internal Error in MClk registers: MClkM=0x%x, MClkN=0x%x\n",
		    tsengReg->pll.MClkM, tsengReg->pll.MClkN);
	    } else {
		outb(pTseng->IOAddress + 0x67, 10);
		outb(pTseng->IOAddress + 0x69, tsengReg->pll.MClkM);
		outb(pTseng->IOAddress + 0x69, tsengReg->pll.MClkN);
	    }
	}
	/* restore old index register */
	outb(pTseng->IOAddress + 0x67, tmp);
    }
    if (DAC_IS_ATT49x)
	tseng_setdaccomm(tsengReg->ATTdac_cmd);

    if (Is_ET6K) {
	outb(pTseng->IOAddress + 0x13, tsengReg->ExtET6K[0x13]);
	outb(pTseng->IOAddress + 0x40, tsengReg->ExtET6K[0x40]);
	outb(pTseng->IOAddress + 0x58, tsengReg->ExtET6K[0x58]);
	outb(pTseng->IOAddress + 0x41, tsengReg->ExtET6K[0x41]);
	outb(pTseng->IOAddress + 0x44, tsengReg->ExtET6K[0x44]);
	outb(pTseng->IOAddress + 0x46, tsengReg->ExtET6K[0x46]);
    }
    outw(iobase + 4, (tsengReg->ExtCRTC[0x3F] << 8) | 0x3F);
    outw(iobase + 4, (tsengReg->ExtCRTC[0x30] << 8) | 0x30);
    outw(iobase + 4, (tsengReg->ExtCRTC[0x31] << 8) | 0x31);
    vgaHWRestore(pScrn, vgaReg, flags); /* TODO: does this belong HERE, in the middle? */
    outw(0x3C4, (tsengReg->ExtTS[6] << 8) | 0x06);
    outw(0x3C4, (tsengReg->ExtTS[7] << 8) | 0x07);
    tmp = inb(iobase + 0x0A);	       /* reset flip-flop */
    outb(0x3C0, 0x36);
    outb(0x3C0, tsengReg->ExtATC);
    outw(iobase + 4, (tsengReg->ExtCRTC[0x33] << 8) | 0x33);
    outw(iobase + 4, (tsengReg->ExtCRTC[0x34] << 8) | 0x34);
    outw(iobase + 4, (tsengReg->ExtCRTC[0x35] << 8) | 0x35);
    if (Is_W32_any) {
	outw(iobase + 4, (tsengReg->ExtCRTC[0x37] << 8) | 0x37);
	outw(0x217a, (tsengReg->ExtIMACtrl << 8) | 0xF7);
    }
    if (!Is_ET6K) {
	outw(iobase + 4, (tsengReg->ExtCRTC[0x32] << 8) | 0x32);
    }
    outb(0x3CD, tsengReg->ExtSegSel[0]);
    if (pTseng->ChipType > TYPE_ET4000)
	outb(0x3CB, tsengReg->ExtSegSel[1]);

#ifdef TODO
    /*
     * This might be required for the Legend clock setting method, but
     * should not be used for the "normal" case because the high order
     * bits are not set in ClockIndex when returning to text mode.
     */
    if (pTseng->Legend) {
	if (tsengReg->ClockIndex >= 0) {
	    vgaProtect(TRUE);
	    (ClockSelect) (tsengReg->ClockIndex);
	}
#endif

    TsengProtect(pScrn, FALSE);

    /* 
     * We must change CRTC 0x36 only OUTSIDE the TsengProtect(pScrn,
     * TRUE)/TsengProtect(pScrn, FALSE) pair, because the sequencer reset
     * also resets the linear mode bits in CRTC 0x36.
     */
    if (Is_W32_any) {
	outw(iobase + 4, (tsengReg->ExtCRTC[0x36] << 8) | 0x36);
    }
}

/* replacement of vgaHWBlankScreen(pScrn, unblank) without seq reset */
void
TsengBlankScreen(ScrnInfoPtr pScrn, Bool unblank)
{
    unsigned char scrn;
    PDEBUG("	TsengBlankScreen\n");

    outb(0x3C4,1);
    scrn = inb(0x3C5);

    if(unblank) {
      scrn &= 0xDF;			/* enable screen */
    }else {
      scrn |= 0x20;			/* blank screen */
    }

/*    vgaHWSeqReset(hwp, TRUE);*/
    outw(0x3C4, (scrn << 8) | 0x01); /* change mode */
/*    vgaHWSeqReset(hwp, FALSE);*/
}


void
TsengProtect(ScrnInfoPtr pScrn, Bool on)
{
    PDEBUG("	TsengProtect\n");
    vgaHWProtect(pScrn, on);
}


/* 
 * The rest below is stuff from the old driver, which still needs to be
 * checked and integrated in the ND
 */

#ifdef OLD_DRIVER

#include "tseng_cursor.h"
extern vgaHWCursorRec vgaHWCursor;

/*
 * ET4000Probe --
 *      check whether a Et4000 based board is installed
 */

static Bool
ET4000Probe()
{
    int numClocks;
    Bool autodetect = TRUE;

    ...

	if (pScrn->bitsPerPixel >= 8) {

	...

	/*
	 * Acceleration is only supported on W32 or newer chips.
	 *
	 * Also, some bus configurations only allow for a 1MB linear memory
	 * aperture instead of the default 4M aperture used on all Tseng devices.
	 * If acceleration is also enabled, you only get 512k + (with some aperture
	 * tweaking) 2*128k for a total of max 768 kb of memory. This just isn't
	 * worth having a lot of conditionals in the accelerator code (the
	 * memory-mapped registers move to the top of the 1M aperture), so we
	 * simply don't allow acceleration and linear mode combined on these cards.
	 * 
	 */

	    if ((pTseng->ChipType < TYPE_ET4000W32) || (pTseng->Linmem_1meg && pTseng->UseLinMem)) {
	    tseng_use_ACL = FALSE;
	} else {
	    /* enable acceleration-related options */
	    OFLG_SET(OPTION_NOACCEL, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_PCI_RETRY, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_SHOWCACHE, &TSENG.ChipOptionFlags);

	    tseng_use_ACL = !OFLG_ISSET(OPTION_NOACCEL, &vga256InfoRec.options);
	}

	...

	/* Hardware Cursor support */
#ifdef W32_HW_CURSOR_FIXED
	    if (pTseng->ChipType >= TYPE_ET4000W32P)
#else
	    if (Is_ET6K)
#endif
	{
	    /* Set HW Cursor option valid */
	    OFLG_SET(OPTION_HW_CURSOR, &TSENG.ChipOptionFlags);
	}
    }
    /* if (pScrn->bitsPerPixel >= 8) */
    else {
	OFLG_CLR(OPTION_HW_CURSOR, &vga256InfoRec.options);
	pTseng->UseLinMem = FALSE;
	tseng_use_ACL = FALSE;
    }

    if (!Is_ET6K) {
	/* Initialize option flags allowed for this driver */
	OFLG_SET(OPTION_LEGEND, &TSENG.ChipOptionFlags);
	OFLG_SET(OPTION_HIBIT_HIGH, &TSENG.ChipOptionFlags);
	OFLG_SET(OPTION_HIBIT_LOW, &TSENG.ChipOptionFlags);
	if (pScrn->bitsPerPixel >= 8) {
	    OFLG_SET(OPTION_PCI_BURST_ON, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_PCI_BURST_OFF, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_W32_INTERLEAVE_ON, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_W32_INTERLEAVE_OFF, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_SLOW_DRAM, &TSENG.ChipOptionFlags);
	    OFLG_SET(OPTION_FAST_DRAM, &TSENG.ChipOptionFlags);
	}
/*
 * because of some problems with W32 cards, SLOW_DRAM is _always_ enabled
 * for those cards
 */
	if (pTseng->ChipType <= TYPE_ET4000W32) {
	    ErrorF("%s %s: option \"slow_dram\" is enabled by default on this card.\n",
		XCONFIG_PROBED, vga256InfoRec.name);
	    OFLG_SET(OPTION_SLOW_DRAM, &vga256InfoRec.options);
	}
	
	...
	
    vga256InfoRec.bankedMono = TRUE;

  ...


    return (TRUE);
}

#endif
