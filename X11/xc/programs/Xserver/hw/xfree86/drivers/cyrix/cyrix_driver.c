/*
 * Copyright 2000 by Richard A. Hecker, California, United States
 * Copyright 2002 by Red Hat Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Richard Hecker not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Richard Hecker makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * RICHARD HECKER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Richard Hecker, hecker@cat.dfrc.nasa.gov
 *          Re-written for XFree86 v4.0
 *
 * Chunks re-written again for XFree86 v4.2
 *	    Alan Cox <alan@redhat.com>
 *		- Fixed cursor handling
 *		- Rewrote parts of the broken mode switch code
 *		- Added proper PCI detection
 *		- Added ShadowFB support
 *		- Added rotate support
 *		- Fixed palette loading/restore
 *		- Added "Nocompression" option
 *		- Fixed line length loading
 *		- Fixed panning logic
 *
 * Previous driver (pre-XFree86 v4.0) by
 *          Annius V. Groenink (A.V.Groenink@zfc.nl, avg@cwi.nl),
 *          Dirk H. Hohndel (hohndel@suse.de),
 *          Portions: the GGI project & confidential CYRIX databooks.
 *		(note that most of the data books have been released by
 *		 NatSemi and are downloadable for free as pdf files)
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cyrix/cyrix_driver.c,v 1.31 2003/11/06 18:38:02 tsi Exp $ */

#include "fb.h"
#include "mibank.h"
#include "micmap.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86cmap.h"
#include "shadowfb.h"
#include "vgaHW.h"
#include "xf86DDC.h"
#include "xf86RAC.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "xf86int10.h"
#include "vbe.h"

#include "cyrix.h"

#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"

#include "opaque.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

static const OptionInfoRec * CYRIXAvailableOptions(int chip, int busid);
static void	CYRIXIdentify(int flags);
static Bool	CYRIXProbe(DriverPtr drv, int flags);
static Bool	CYRIXPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	CYRIXScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	CYRIXEnterVT(int scrnIndex, int flags);
static void	CYRIXLeaveVT(int scrnIndex, int flags);
static Bool	CYRIXCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	CYRIXSaveScreen(ScreenPtr pScreen, int mode);

/* Required if the driver supports mode switching */
static Bool	CYRIXSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
/* Required if the driver supports moving the viewport */
static void	CYRIXAdjustFrame(int scrnIndex, int x, int y, int flags);

/* Optional functions */
static void	CYRIXFreeScreen(int scrnIndex, int flags);
static int	CYRIXFindIsaDevice(GDevPtr dev);
static ModeStatus CYRIXValidMode(int scrnIndex, DisplayModePtr mode,
				 Bool verbose, int flags);

/* Internally used functions */
static void	CYRIXSave(ScrnInfoPtr pScrn);
static void	CYRIXRestore(ScrnInfoPtr pScrn);
static Bool	CYRIXModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void	CYRIXRestorePalette(ScrnInfoPtr pScrn);
static void	CYRIXSavePalette(ScrnInfoPtr pScrn);

/* Misc additional routines */
void     CYRIXSetRead(int bank);
void     CYRIXSetWrite(int bank);
void     CYRIXSetReadWrite(int bank);

#define VERSION 4000
#define CYRIX_NAME "CYRIX"
#define CYRIX_DRIVER_NAME "cyrix"
#define CYRIX_MAJOR_VERSION 1
#define CYRIX_MINOR_VERSION 0
#define CYRIX_PATCHLEVEL 0
#define VGA_REGION 2

enum GenericTypes {
    CHIP_CYRIXmediagx
};

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the moduleSetup
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec CYRIX = {
    VERSION,
    CYRIX_DRIVER_NAME,
    CYRIXIdentify,
    CYRIXProbe,
    CYRIXAvailableOptions,
    NULL,
    0
};

static SymTabRec CYRIXChipsets[] = {
    { CHIP_CYRIXmediagx,		"mediagx" },
    { -1,				NULL }
};

static IsaChipsets CYRIXISAChipsets[] = {
    { CHIP_CYRIXmediagx,	RES_EXCLUSIVE_VGA },
    { -1,			0 }
};
    
typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL,
    OPTION_NOCOMPRESS,
    OPTION_SHADOW_FB,
    OPTION_ROTATE
} CYRIXOpts;

static const OptionInfoRec CYRIXOptions[] = {
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_HW_CURSOR,		"HWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOCOMPRESS,	"NoCompression",OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_SHADOW_FB,		"ShadowFB",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_ROTATE,		"Rotate",	OPTV_ANYSTR,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

static const char *vgahwSymbols[] = {
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
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

#ifdef XFree86LOADER
static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};
#endif

/* access to the MediaGX video hardware registers */


/* DEBUG variables & flags */
#define ENTER TRUE
#define LEAVE FALSE

#ifdef XFree86LOADER

static MODULESETUPPROTO(cyrixSetup);

static XF86ModuleVersionInfo cyrixVersRec =
{
	"cyrix",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	CYRIX_MAJOR_VERSION, CYRIX_MINOR_VERSION, CYRIX_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,			/* This is a video driver */
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData cyrixModuleData = { &cyrixVersRec, cyrixSetup, NULL };

pointer
cyrixSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&CYRIX, module, 0);
	LoaderRefSymLists(vgahwSymbols, fbSymbols, xaaSymbols, vbeSymbols, shadowSymbols, NULL);
	return (pointer)TRUE;
    } 

    if (errmaj) *errmaj = LDR_ONCEONLY;
    return NULL;
}

#endif /* XFree86LOADER */

static Bool
CYRIXGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate a CYRIXRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(CYRIXPrivate), 1);
    if (pScrn->driverPrivate == NULL)
	return FALSE;

    return TRUE;
}

static void
CYRIXFreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static void 
CYRIXDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    unsigned char DPMSCont, PMCont, temp;
    outb(0x3C4, 0x0E);
    temp = inb(0x3C5);
    outb(0x3C5, 0xC2);
    outb(0x83C8, 0x04); /* Read DPMS Control */
	PMCont = inb(0x83C6) & 0xFC;
	outb(0x3CE, 0x23);
	DPMSCont = inb(0x3CF) & 0xFC;
	switch (PowerManagementMode)
	{
	case DPMSModeOn:
		/* Screen: On, HSync: On, VSync: On */
		PMCont |= 0x03;
		DPMSCont |= 0x00;
		break;
	case DPMSModeStandby:
		/* Screen: Off, HSync: Off, VSync: On */
		PMCont |= 0x02;
		DPMSCont |= 0x01;
		break;
	case DPMSModeSuspend:
		/* Screen: Off, HSync: On, VSync: Off */
		PMCont |= 0x02;
		DPMSCont |= 0x02;
		break;
	case DPMSModeOff:
		/* Screen: Off, HSync: Off, VSync: Off */
		PMCont |= 0x00;
		DPMSCont |= 0x03;
		break;
	}
	outb(0x3CF, DPMSCont);
	outb(0x83C8, 0x04);
	outb(0x83C6, PMCont);
	outw(0x3C4, (temp<<8) | 0x0E);
}

/* Mandatory */
static void
CYRIXIdentify(int flags)
{
    xf86PrintChipsets(CYRIX_NAME, "driver for Cyrix MediaGX Processors", CYRIXChipsets);
}

static const OptionInfoRec *
CYRIXAvailableOptions(int chip, int busid)
{
    return CYRIXOptions;
}

#define PCI_CHIP_CYRIX5510	0x0000
#define PCI_CHIP_CYRIX5520	0x0002
#define PCI_CHIP_CYRIX5530	0x0104

/* Conversion PCI ID to chipset name */
static PciChipsets CYRIXPCIchipsets[] = {
	{ CHIP_CYRIXmediagx, PCI_CHIP_CYRIX5510, RES_EXCLUSIVE_VGA },
	{ CHIP_CYRIXmediagx, PCI_CHIP_CYRIX5520, RES_EXCLUSIVE_VGA },
	{ CHIP_CYRIXmediagx, PCI_CHIP_CYRIX5530, RES_EXCLUSIVE_VGA },
	{ -1,        -1,                 RES_UNDEFINED }
};

/* Mandatory */
static Bool
CYRIXProbe(DriverPtr drv, int flags)
{
    int i, numDevSections, numUsed, *usedChips;
    GDevPtr *devSections;
    ScrnInfoPtr pScrn;
    Bool foundScreen = FALSE;

    /*
     * The aim here is to find all cards that this driver can handle,
     * and for the ones not already claimed by another driver, claim the
     * slot, and allocate a ScrnInfoRec.
     *
     */

    /*
     * Look for config file Device sections with this driver specified.
     */
    if ((numDevSections = xf86MatchDevice(CYRIX_DRIVER_NAME,
						&devSections)) <= 0) {
#ifdef DEBUG
   	xf86ErrorFVerb(3,"%s: No Device section found.\n",CYRIX_NAME);
#endif
	/*
	 * There's no matching device section in the config file, so quit
	 * now.
	 */
	return FALSE;
    }
#ifdef DEBUG
    xf86ErrorFVerb(3,"%s: Device Sections found: %d\n",CYRIX_NAME, numDevSections);
#endif

    /* Should look like an ISA device */

    /* PCI BUS */
    if (xf86GetPciVideoInfo() ) {
	numUsed = xf86MatchPciInstances(CYRIX_NAME, PCI_VENDOR_CYRIX,
					CYRIXChipsets, CYRIXPCIchipsets, 
					devSections,numDevSections,
					drv, &usedChips);

	if (numUsed > 0) {
	    if (flags & PROBE_DETECT)
		foundScreen = TRUE;
	    else for (i = 0; i < numUsed; i++) {
		ScrnInfoPtr pScrn = NULL;
		/* Allocate a ScrnInfoRec and claim the slot */
		if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
						       CYRIXPCIchipsets,NULL, NULL,
						       NULL, NULL, NULL))) {
		    pScrn->driverVersion = VERSION;
		    pScrn->driverName    = CYRIX_DRIVER_NAME;
		    pScrn->name          = CYRIX_NAME;
		    pScrn->Probe         = CYRIXProbe;
		    pScrn->PreInit       = CYRIXPreInit;
		    pScrn->ScreenInit    = CYRIXScreenInit;
		    pScrn->SwitchMode    = CYRIXSwitchMode;
		    pScrn->AdjustFrame   = CYRIXAdjustFrame;
		    pScrn->LeaveVT       = CYRIXLeaveVT;
		    pScrn->EnterVT       = CYRIXEnterVT;
		    pScrn->FreeScreen    = CYRIXFreeScreen;
		    pScrn->ValidMode     = CYRIXValidMode;
		    foundScreen = TRUE;
		}
	    }
	    xfree(usedChips);
	}
    }
    

    numUsed = xf86MatchIsaInstances(CYRIX_NAME,CYRIXChipsets,
					CYRIXISAChipsets,drv,
					CYRIXFindIsaDevice,devSections,
					numDevSections,&usedChips);
    if(numUsed > 0) {
        foundScreen = TRUE;

        /* Free it since we don't need that list after this */
        xfree(devSections);

        if (!(flags & PROBE_DETECT)) {
          for (i=0; i < numUsed; i++) {

          /* Fill in what we can of the ScrnInfoRec */
	        pScrn = NULL;
	        if ((pScrn = xf86ConfigIsaEntity(pScrn, 0, usedChips[i],
						   CYRIXISAChipsets, NULL,
						   NULL, NULL, NULL, NULL))){
		    pScrn->driverVersion = VERSION;
		    pScrn->driverName    = CYRIX_DRIVER_NAME;
		    pScrn->name          = CYRIX_NAME;
		    pScrn->Probe         = CYRIXProbe;
		    pScrn->PreInit       = CYRIXPreInit;
		    pScrn->ScreenInit    = CYRIXScreenInit;
		    pScrn->SwitchMode    = CYRIXSwitchMode;
		    pScrn->AdjustFrame   = CYRIXAdjustFrame;
		    pScrn->LeaveVT       = CYRIXLeaveVT;
		    pScrn->EnterVT       = CYRIXEnterVT;
		    pScrn->FreeScreen    = CYRIXFreeScreen;
		    pScrn->ValidMode     = CYRIXValidMode;
	        }
	 }
      }
    }
    xfree(usedChips);
    return (foundScreen);
}

static int
CYRIXFindIsaDevice(GDevPtr dev)
{
    CARD32 CurrentValue, TestValue;
    unsigned char gcr;

    /* No need to unlock VGA CRTC registers here */

    /* VGA has one more read/write attribute register than EGA */
    /* use register probing to decide whether the chip is
     * `suitable' for us.
     */
    int vgaIOBase = VGAHW_GET_IOBASE();

    (void) inb(vgaIOBase + 0x0AU);  /* Reset flip-flop */
    outb(0x3C0, 0x14 | 0x20);
    CurrentValue = inb(0x3C1);
    outb(0x3C0, CurrentValue ^ 0x0F);
    outb(0x3C0, 0x14 | 0x20);
    TestValue = inb(0x3C1);
    outb(0x3C0, CurrentValue);

    /* Quit now if no VGA is present */
    if ((CurrentValue ^ 0x0F) != TestValue)
      return -1;

    /* the lock register should read 0xFF after we have
      written 0x00 to lock */
    outb(vgaIOBase + 4, CrtcExtendedRegisterLock);
    outb(vgaIOBase + 5, 0x00);

    if (inb(vgaIOBase + 5) != 0xFF) return -1;

    /* the lock register should read 0x00 after we have
	 written the magic word 'WL' to unlock */
    outb(vgaIOBase + 5, 0x57);
    outb(vgaIOBase + 5, 0x4C);

    /* GGI's idea to do two comparisons */
    if (inb(vgaIOBase + 5) != 0x00) goto fail;
    if (inb(vgaIOBase + 5) != 0x00) goto fail;

    /* OK, it's most likely a MediaGX.  Now check the scratchpad
     * size.  If it is zero, we're not using the MediaGX graphics
     * facilities. 
     */
    outb(GX_IOPORT_INDEX, GX_IOIDX_DIR0); /* doesn't work w/o that */
    outb(GX_IOPORT_INDEX, GX_IOIDX_GCR);
    gcr = inb(GX_IOPORT_DATA);

    /*      end GGI MediaGX driver based code */
    if (!(gcr & 12)) goto fail;

    /* Unprotect MediaGX extended registers */
    outb(vgaIOBase + 4, CrtcExtendedRegisterLock);
    outb(vgaIOBase + 5, 0x00);

    return (int)CHIP_CYRIXmediagx;

 fail:
    /* Protect MediaGX extended registers */
    outb(vgaIOBase + 4, CrtcExtendedRegisterLock);
    outb(vgaIOBase + 5, 0x00);
    return -1;
}

static void
CYRIXProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;
    if (xf86LoadSubModule(pScrn, "vbe")) {
	pVbe = VBEInit(NULL,index);
	ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}
	
/* Mandatory */
static Bool
CYRIXPreInit(ScrnInfoPtr pScrn, int flags)
{
    MessageType from;
    CYRIXPrvPtr pCyrix;
    int videoram;
    int i;
    ClockRangePtr clockRanges;
    unsigned int physbase, padsize;
    int device_step, device_revision;
    int vgaIOBase;
    unsigned char gcr;
    static int accelWidths[3]= {2,1024, 2048};
    const char *s;

    /* Allocate the CYRIXRec driverPrivate */
    if (!CYRIXGetRec(pScrn)) return FALSE;

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
    pCyrix = CYRIXPTR(pScrn);

    pCyrix->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

    if (pCyrix->pEnt->location.type != BUS_PCI)
      return FALSE;

    if (flags & PROBE_DETECT) {
	CYRIXProbeDDC(pScrn, pCyrix->pEnt->index);
	return TRUE;
    }

    /* The vgahw module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "vgahw"))
	return FALSE;
    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn))
	return FALSE;

    VGAHWPTR(pScrn)->MapSize = 0x10000;		/* Standard 64k VGA window */
    if (!vgaHWMapMem(pScrn))
	return FALSE;
    
    vgaHWGetIOBase(VGAHWPTR(pScrn));
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    
    
    /* Unprotect MediaGX extended registers */
    outb(vgaIOBase + 4, CrtcExtendedRegisterLock);
    outb(vgaIOBase + 5, 0x57);
    outb(vgaIOBase + 5, 0x4C);

    outb(GX_IOPORT_INDEX, GX_IOIDX_DIR0); /* doesn't work w/o that */
    outb(GX_IOPORT_INDEX, GX_IOIDX_GCR);
    gcr = inb(GX_IOPORT_DATA);

    physbase = (gcr & 3) << 30;
    padsize = (gcr & 12) ? (((gcr & 12) >> 2) + 1) : 0;

    /*      end GGI MediaGX driver based code */
    if (padsize == 0) return (FALSE);

    xf86ErrorF("%s: GX_BASE: 0x%x\n", CYRIX_NAME, physbase);
    xf86ErrorF("%s: Scratchpad size: %d kbytes\n", CYRIX_NAME, padsize);

    /* Probe for the MediaGX processor version details.  Older versions
     * use different op-codes for setting the organization of the
     * blit buffers within the scratch padarea.  We currently
     * also use this version ID to guess whether the chipset has
     * an external DAC (in which case we treat the colour maps
     * in a slightly different fashion) 
     */
    outb(0x22, 0xFF);
    device_step = device_revision = inb(0x23);
    device_step >>= 8;
    device_revision &= 0xFF;
    xf86ErrorF("%s: MediaGX processor ID %d revision %d\n", 
		CYRIX_NAME, device_step, device_revision);

    /* Some  MediaGX systems have different blit buffer offsets than
     * is  indicated by the scratchpad size.  Make sure that we have
     * the  right offsets by writing them into the corresponding CPU
     * registers.  The op-codes to use depend on the processor
     * revision.  The value `40' is a guess (awaiting details from Cyrix).
     */
    pCyrix->CYRIXbltBufSize = (padsize == 4) ? 1840 : (padsize == 3) ? 1328 : 816;
    pCyrix->CYRIXbltBuf1Address = 0x0E60 - pCyrix->CYRIXbltBufSize;
    pCyrix->CYRIXbltBuf0Address = pCyrix->CYRIXbltBuf1Address - pCyrix->CYRIXbltBufSize;

    /* map the entire area from GX_BASE (scratchpad area)
	up to the end of the control registers */
    pCyrix->GXregisters = (char*)xf86MapVidMem(pScrn->scrnIndex,
			VGA_REGION, (int )physbase, 0x9000);
    if (!pCyrix->GXregisters) {
	ErrorF("%s: Cannot map hardware registers\n", CYRIX_NAME);
	return FALSE;
    }

    /* This is the general case */
    for (i = 0; i<pScrn->numEntities; i++) {
	pCyrix->pEnt = xf86GetEntityInfo(pScrn->entityList[i]);
	if (pCyrix->pEnt->resources) return FALSE;
	pCyrix->Chipset = pCyrix->pEnt->chipset;
	pScrn->chipset = (char *)xf86TokenToString(CYRIXChipsets,
						pCyrix->pEnt->chipset);
    }

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * Our default depth is 8, so pass it to the helper function.
     */
    if (!xf86SetDepthBpp(pScrn, 8, 0, 0, Support24bppFb)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 1:
	case 4:
	case 8:
	case 15:
	case 16:
	    /* OK */
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }

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
    xf86PrintDepthBpp(pScrn);

    /*
     * The new cmap layer needs this to be initialised.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }

    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    /* Allocate the CYRIXRec driverPrivate */
    if (!CYRIXGetRec(pScrn)) {
	return FALSE;
    }
    pCyrix = CYRIXPTR(pScrn);

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pCyrix->Options = xalloc(sizeof(CYRIXOptions))))
	return FALSE;
    memcpy(pCyrix->Options, CYRIXOptions, sizeof(CYRIXOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pCyrix->Options);

    /* Set the bits per RGB for 8bpp mode */
    pScrn->rgbBits = 6;

    from = X_DEFAULT;
    pCyrix->HWCursor = FALSE;
    if (xf86IsOptionSet(pCyrix->Options, OPTION_HW_CURSOR)) {
	from = X_CONFIG;
	pCyrix->HWCursor = TRUE;
    }
    if (xf86IsOptionSet(pCyrix->Options, OPTION_SW_CURSOR)) {
	from = X_CONFIG;
	pCyrix->HWCursor = FALSE;
    }
    if (pCyrix->HWCursor == TRUE)
    	xf86DrvMsg(pScrn->scrnIndex, from, "Hardware cursor is disabled in this release\n");

    if (xf86ReturnOptValBool(pCyrix->Options, OPTION_SHADOW_FB, FALSE)) {
	pCyrix->ShadowFB = TRUE;
	pCyrix->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		"Using \"Shadow Framebuffer\" - acceleration disabled\n");
    }
    pCyrix->Rotate = 0;
    if ((s = xf86GetOptValString(pCyrix->Options, OPTION_ROTATE))) {
      if(!xf86NameCmp(s, "CW")) {
	pCyrix->ShadowFB = TRUE;
	pCyrix->NoAccel = TRUE;
	pCyrix->HWCursor = FALSE;
	pCyrix->Rotate = 1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		"Rotating screen clockwise - acceleration disabled\n");
      } else
      if(!xf86NameCmp(s, "CCW")) {
	pCyrix->ShadowFB = TRUE;
	pCyrix->NoAccel = TRUE;
	pCyrix->HWCursor = FALSE;
	pCyrix->Rotate = -1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		"Rotating screen counter clockwise - acceleration disabled\n");
      } else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		"\"%s\" is not a valid value for Option \"Rotate\"\n", s);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Valid options are \"CW\" or \"CCW\"\n");
      }
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pCyrix->HWCursor ? "HW" : "SW");
    if (xf86IsOptionSet(pCyrix->Options, OPTION_NOACCEL)) {
	pCyrix->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }

    pCyrix->NoCompress = FALSE;
    if (xf86IsOptionSet(pCyrix->Options, OPTION_NOCOMPRESS)) {
	pCyrix->NoCompress = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Compression disabled\n");
    }

    pCyrix->PciInfo = NULL;
    pCyrix->RamDac = -1;

    /*
     * This shouldn't happen because such problems should be caught in
     * CYRIXProbe(), but check it just in case.
     */
    if (pScrn->chipset == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", pCyrix->Chipset);
	return FALSE;
    }
    if (pCyrix->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
	return FALSE;
    }


    pCyrix->EngineOperation = 0x00;

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Found %s chip\n", pScrn->chipset);
    
    if (pCyrix->pEnt->device->MemBase != 0) {
	pCyrix->FbAddress = pCyrix->pEnt->device->MemBase;
	from = X_CONFIG;
    } else {
	from = X_PROBED;
	pCyrix->FbAddress = 0x40800000;  /* Hard coded for 1st try */
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	       (unsigned long)pCyrix->FbAddress);

    if (pCyrix->pEnt->device->IOBase != 0) {
	pCyrix->IOAccelAddress = pCyrix->pEnt->device->IOBase;
	from = X_CONFIG;
    } else {
	from = X_PROBED;
	pCyrix->IOAccelAddress = 0x40008100; /* Hard coded for 1st try */
    }

    xf86DrvMsg(pScrn->scrnIndex, from,"IO registers at 0x%lx\n",(unsigned long)pCyrix->IOAccelAddress);

    /* HW bpp matches reported bpp */
    pCyrix->HwBpp = pScrn->bitsPerPixel;

    if (pCyrix->pEnt->device->videoRam != 0) {
	pScrn->videoRam = pCyrix->pEnt->device->videoRam;
	from = X_CONFIG;
    } else {
	from = X_PROBED;
	outb(vgaIOBase + 4, CrtcGraphicsMemorySize);
	videoram = (inb(vgaIOBase + 5) * 64);
	pScrn->videoRam = videoram & 0xFFFFFC00; /* mask out the part we want */
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d kByte\n",
               pScrn->videoRam);

    pCyrix->FbMapSize = pScrn->videoRam * 1024;

    /* Set the min pixel clock */
    pCyrix->MinClock = 16250;	/* XXX Guess, need to check this */
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n",
	       pCyrix->MinClock / 1000);

    pCyrix->MaxClock = 135000;
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Max pixel clock is %d MHz\n",
	       pCyrix->MaxClock / 1000);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = (ClockRangePtr)xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pCyrix->MinClock;
    clockRanges->maxClock = pCyrix->MaxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = FALSE;	/* XXX check this */

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our CYRIXValidMode() already takes
     * care of this, we don't worry about setting them here.
     */

    /* Select valid modes from those available */
    /*
     * XXX Assuming min pitch 256, max 2048
     * XXX Assuming min height 128, max 2048
     */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			pScrn->display->modes, clockRanges,
			accelWidths, 256, 2048,
			pScrn->bitsPerPixel, 128, 2048,
			pScrn->display->virtualX,
			pScrn->display->virtualY,
			pCyrix->FbMapSize,
			LOOKUP_BEST_REFRESH);

    if (i == -1) {
	CYRIXFreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	CYRIXFreeRec(pScrn);
	return FALSE;
    }

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    switch (pScrn->bitsPerPixel) {
    case 1:
    case 4:
    case 8:
	pCyrix->EngineOperation |= 0x00;
	break;
    case 16:
	pCyrix->EngineOperation |= 0x01;
	break;
    }

    /* Load fb module */
    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	CYRIXFreeRec(pScrn);
	return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);

    /* Load XAA if needed */
    if (!pCyrix->NoAccel) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    CYRIXFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);

        switch (pScrn->displayWidth * pScrn->bitsPerPixel / 8) {
	    case 512:
		pCyrix->EngineOperation |= 0x00;
		break;
	    case 1024:
		pCyrix->EngineOperation |= 0x04;
		break;
	    case 2048:
		pCyrix->EngineOperation |= 0x08;
		break;
	}
    }

    /* Load shadowfb if needed */
    if (pCyrix->ShadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    CYRIXFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    return TRUE;
}


/*
 * This function saves the video state.
 */
static void
CYRIXSave(ScrnInfoPtr pScrn)
{
    vgaRegPtr vgaReg;
    CYRIXPrvPtr pCyrix;
    CYRIXRegPtr cyrixReg;

    pCyrix = CYRIXPTR(pScrn);
    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    cyrixReg = &pCyrix->SavedReg;

    vgaHWSave(pScrn, vgaReg, VGA_SR_ALL);
    CyrixSave(pScrn, cyrixReg); 
    CYRIXSavePalette(pScrn);
}


/*
 * Initialise a new mode.  This is currently still using the old
 * "initialise struct, restore/write struct to HW" model.  That could
 * be changed.
 */

static Bool
CYRIXModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    int ret = -1;
    vgaHWPtr hwp;
/*    vgaRegPtr vgaReg; */
    CYRIXPrvPtr pCyrix;
    CYRIXRegPtr cyrixReg;

    hwp = VGAHWPTR(pScrn);
    vgaHWUnlock(hwp);

    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, mode))
        return FALSE;
    pScrn->vtSema = TRUE;

    pCyrix = CYRIXPTR(pScrn);

    /* Do the guts of this work */
    ret = CyrixInit(pScrn, mode);

    if (!ret)
	return FALSE;

    /* Program the registers */
/*    vgaReg = &hwp->ModeReg; */
    cyrixReg = &pCyrix->ModeReg;

    CyrixRestore(pScrn, cyrixReg);
/*    vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);*/

    return TRUE;
}

/*
 * Restore the initial (text) mode.
 */
static void 
CYRIXRestore(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp;
    vgaRegPtr vgaReg;
    /*CYRIXPrvPtr pCyrix;*/
    /*CYRIXRegPtr cyrixReg*/;

    hwp = VGAHWPTR(pScrn);
    /*pCyrix = CYRIXPTR(pScrn);*/
    vgaReg = &hwp->SavedReg;
    /*cyrixReg = &pCyrix->SavedReg*/;

    vgaHWProtect(pScrn, TRUE);

    /*CyrixRestore(pScrn, cyrixReg);*/
    CYRIXRestorePalette(pScrn);
    vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);

    vgaHWProtect(pScrn, FALSE);
}

/* Needed because we are not VGA enough in all cases (eg 5530 LCD) */

static void
CYRIXLoadPalette(
   ScrnInfoPtr pScrn,
   int numColors,
   int *indices,
   LOCO *colors,
   VisualPtr pVisual
){
   int i;
   unsigned int locked;
   CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
   
   switch(pScrn->depth) {
   case 15:	
   case 16:
	return;
   }

   locked = GX_REG(DC_UNLOCK);
   GX_REG(DC_UNLOCK) = DC_UNLOCK_VALUE;
   
   for(i = 0; i < numColors; i++) {
        unsigned int red, green, blue;
        int index = indices[i];
        
        red = colors[index].red;
        green = colors[index].green;
        blue = colors[index].blue;;
        
        GX_REG(DC_PAL_ADDRESS) = index;
        GX_REG(DC_PAL_DATA) = (red << 12) | (green << 6) | blue;
   } 
   GX_REG(DC_UNLOCK) = locked;
}

static void CYRIXSavePalette(ScrnInfoPtr pScrn)
{
   int i;
   unsigned int locked;
   CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
   
   locked = GX_REG(DC_UNLOCK);
   GX_REG(DC_UNLOCK) = DC_UNLOCK_VALUE;
   
   for(i = 0; i < 256; i++) {
        GX_REG(DC_PAL_ADDRESS) = i;
        pCyrix->SavedReg.Colormap[i] = GX_REG(DC_PAL_DATA);
   } 
   GX_REG(DC_UNLOCK) = locked;
}

static void CYRIXRestorePalette(ScrnInfoPtr pScrn)
{
   int i;
   unsigned int locked;
   CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
   
   locked = GX_REG(DC_UNLOCK);
   GX_REG(DC_UNLOCK) = DC_UNLOCK_VALUE;
   
   for(i = 0; i < 256; i++) {
        GX_REG(DC_PAL_ADDRESS) = i;
        GX_REG(DC_PAL_DATA) = pCyrix->SavedReg.Colormap[i];
   } 
   GX_REG(DC_UNLOCK) = locked;
}

/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
CYRIXScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    /* The vgaHW references will disappear one day */
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    CYRIXPrvPtr pCyrix;
    int ret;
    VisualPtr visual;
    int width, height, displayWidth;
    unsigned char *FBStart;
    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    hwp = VGAHWPTR(pScrn);

    hwp->MapSize = 0x10000;		/* Standard 64k VGA window */

    pCyrix = CYRIXPTR(pScrn);

    /* Map the VGA memory, frambuffer, and get the VGA IO base */
    if (!vgaHWMapMem(pScrn))
	return FALSE;
    vgaHWGetIOBase(hwp);
    pCyrix->FbBase = xf86MapVidMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
	(unsigned long)pCyrix->FbAddress, pCyrix->FbMapSize);

    /* Save the current state */
    CYRIXSave(pScrn);

    /* Initialise the first mode */
    CYRIXModeInit(pScrn, pScrn->currentMode);

    /* Darken the screen for aesthetic reasons and set the viewport */
    CYRIXSaveScreen(pScreen, SCREEN_SAVER_ON);
    CYRIXAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    /* XXX Fill the screen with black */
#if 0
    CYRIXSaveScreen(pScreen, SCREEN_SAVER_OFF);
#endif

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     *
     */

    /*
     * Reset visual list.
     */
    if (pScrn->bitsPerPixel >= 8) miClearVisualTypes();

    /* Setup the visuals we support. */
    if (pScrn->bitsPerPixel > 8) {
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
			      pScrn->defaultVisual))
	    return FALSE;
    } else {
	if (!xf86SetDefaultVisual(pScrn, -1))
	    return FALSE;

	if (!miSetVisualTypes(pScrn->depth, 
			      miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
	    return FALSE;
    }

    miSetPixmapDepths ();
    
    width = pScrn->virtualX;
    height = pScrn->virtualY;
    displayWidth = pScrn->displayWidth;


    if(pCyrix->Rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    }

    if(pCyrix->ShadowFB) {
 	pCyrix->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
	pCyrix->ShadowPtr = xalloc(pCyrix->ShadowPitch * height);
	displayWidth = pCyrix->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pCyrix->ShadowPtr;
    } else {
	pCyrix->ShadowPtr = NULL;
	FBStart = pCyrix->FbBase;
    }
    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    switch (pScrn->bitsPerPixel) {
    case 1:
    case 4:
    case 8:
    case 16:
	ret = fbScreenInit(pScreen, FBStart, width,
			height, pScrn->xDpi, pScrn->yDpi, 
			displayWidth, pScrn->bitsPerPixel);
	break;
    default:
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Internal error: invalid bpp (%d) in CYRIXScreenInit\n",
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
    } else if (pScrn->depth == 1) {
	Cyrix1bppColorMap(pScrn);
    }

    /* must be after RGB ordering fixed */
    fbPictureInit (pScreen, 0, 0);
    
    if (pScrn->depth < 8) {
	miBankInfoPtr pBankInfo;

	/* Setup the vga banking variables */
	pBankInfo = (miBankInfoPtr)xnfcalloc(sizeof(miBankInfoRec),1);
	if (pBankInfo == NULL)
	    return FALSE;
	
	pBankInfo->pBankA = hwp->Base;
	pBankInfo->pBankB = hwp->Base;
	pBankInfo->BankSize = 0x10000;
	pBankInfo->nBankDepth = pScrn->depth;
	xf86EnableAccess(pScrn);

	pBankInfo->SetSourceBank = 
		(miBankProcPtr)CYRIXSetRead;
	pBankInfo->SetDestinationBank = 
		(miBankProcPtr)CYRIXSetWrite;
	pBankInfo->SetSourceAndDestinationBanks = 
		(miBankProcPtr)CYRIXSetReadWrite;
	if (!miInitializeBanking(pScreen, pScrn->virtualX, pScrn->virtualY,
				 pScrn->displayWidth, pBankInfo)) {
	    xfree(pBankInfo);
	    pBankInfo = NULL;
	    return FALSE;
	}
    }

    if (!pCyrix->NoAccel) 
	CYRIXAccelInit(pScreen);

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    xf86SetSilkenMouse(pScreen);

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());


    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    if (!xf86HandleColormaps(pScreen, 256, pScrn->rgbBits, 
    			CYRIXLoadPalette, NULL,
    			CMAP_RELOAD_ON_MODE_SWITCH))
	return FALSE;    			

    xf86DPMSInit(pScreen, (DPMSSetProcPtr)CYRIXDisplayPowerManagementSet, 0);

    pScrn->memPhysBase = pCyrix->FbAddress;
    pScrn->fbOffset = 0;

    if(pCyrix->ShadowFB) {
	RefreshAreaFuncPtr refreshArea = CYRIXRefreshArea;

	if(pCyrix->Rotate) {
	    if (!pCyrix->PointerMoved) {
		    pCyrix->PointerMoved = pScrn->PointerMoved;
		    pScrn->PointerMoved = CYRIXPointerMoved;
	    }

	   switch(pScrn->bitsPerPixel) {
	   case 8:	refreshArea = CYRIXRefreshArea8;	break;
	   case 16:	refreshArea = CYRIXRefreshArea16;	break;
	   }
	}

	ShadowFBInit(pScreen, refreshArea);
    }

    pCyrix->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = CYRIXCloseScreen;
    pScreen->SaveScreen = CYRIXSaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
CYRIXSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return CYRIXModeInit(xf86Screens[scrnIndex], mode);
}


/* Usually mandatory */
static void 
CYRIXAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp;
    CYRIXPrvPtr pCyrix;
    int base = y * pScrn->displayWidth + x;
    unsigned char temp;
    int vgaIOBase;
    
    hwp = VGAHWPTR(pScrn);
    vgaIOBase = hwp->IOBase;
    pCyrix = CYRIXPTR(pScrn);

    switch (pScrn->bitsPerPixel) {
	case 4:
/*	    base /= 2; */
	    base = base >> 4;
	    break;
	case 8:
	    base = (base & 0xFFFFFFF8) >> 2; 
	    break;
	case 16:
/*	    base *= (pScrn->bitsPerPixel / 8); */
	    base /= 2;
	    break;
    }

    GX_REG(DC_UNLOCK) = DC_UNLOCK_VALUE;
    
    /* If you switch to poking FB_ST_OFFSET directly it expects to be
       fed different values to the SoftVGA emulation code */
       
    /*GX_REG(DC_FB_ST_OFFSET) = base; */
    /* CRT bits 0-15 */
    outw(vgaIOBase + 4, (base & 0x00FF00) | 0x0C); 
    outw(vgaIOBase + 4, ((base & 0x00FF) << 8) | 0x0D); 
    /* CRT bit 16 */
    outb(vgaIOBase + 4, 0x1E); temp = inb(vgaIOBase + 5) & 0xDF; 
    outb(vgaIOBase + 5, temp | (base & 0x10000) >> 11); 
    GX_REG(DC_UNLOCK) = 0;
}


/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */

/* Mandatory */
static Bool
CYRIXEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    /* Should we re-save the text mode on each VT enter? */
    if (!CYRIXModeInit(pScrn, pScrn->currentMode))
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
CYRIXLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    CYRIXRestore(pScrn);
    vgaHWLock(hwp);
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
CYRIXCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CYRIXPrvPtr pCyrix;

    if (pScrn->vtSema) {
    	CYRIXRestore(pScrn);
    	vgaHWLock(hwp);
    }
    pCyrix = CYRIXPTR(pScrn);
    if(pCyrix->AccelInfoRec)
	XAADestroyInfoRec(pCyrix->AccelInfoRec);
    if (pCyrix->ShadowPtr)
	xfree(pCyrix->ShadowPtr);
    pScrn->vtSema = FALSE;
    
    pScreen->CloseScreen = pCyrix->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any per-generation data structures */

/* Optional */
static void
CYRIXFreeScreen(int scrnIndex, int flags)
{
    vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    CYRIXFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static ModeStatus
CYRIXValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
CYRIXSaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}

