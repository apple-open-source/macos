/**********************************************************************
Copyright 1998, 1999 by Precision Insight, Inc., Cedar Park, Texas.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Precision Insight not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Precision Insight
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

PRECISION INSIGHT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/

/*
 * Copyright 1998, 1999 Egbert Eich
 * Copyright 2000, 2001 SuSE GmbH, Author: Egbert Eich
 * Copyright 2002 SuSE Linux AG, Author: Egbert Eich
 * Copyright 2002 Shigehiro Nomura
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_driver.c,v 1.66 2002/12/09 11:32:48 eich Exp $ */

/*
 * The original Precision Insight driver for
 * XFree86 v.3.3 has been sponsored by Red Hat.
 *
 * Authors:
 *   Jens Owen (jens@tungstengraphics.com)
 *   Kevin E. Martin (kevin@precisioninsight.com)
 *
 * Port to Xfree86 v.4.0
 *   1998, 1999 by Egbert Eich (Egbert.Eich@Physik.TU-Darmstadt.DE)
 */

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

/* Everything using inb/outb, etc needs "compiler.h" */
#include "compiler.h"

#include "xf86Resources.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* This is used for module versioning */
#include "xf86Version.h"

/* All drivers using the vgahw module need this */
#include "vgaHW.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

/* All drivers using the mi banking wrapper need this */
#include "mibank.h"

/* All drivers using the mi colormap manipulation need this */
#include "micmap.h"

#include "xf86cmap.h"

#include "fb.h"

/* Needed by Resources Access Control (RAC) */
#include "xf86RAC.h"

/* int10 */
#include "xf86int10.h"
#include "vbe.h"

/* Needed for Device Data Channel (DDC) support */
#include "xf86DDC.h"

#include "picturestr.h"

#ifdef XvExtension
#include "xf86xv.h"
#include "Xv.h"
#endif

/*
 * Driver data structures.
 */
#include "neo.h"
#include "neo_reg.h"
#include "neo_macros.h"

/* These need to be checked */
#include "X.h"
#include "Xproto.h"
#include "scrnintstr.h"
#include "servermd.h"
#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"

/* Mandatory functions */
static const OptionInfoRec *	NEOAvailableOptions(int chipid, int busid);
static void     NEOIdentify(int flags);
static Bool     NEOProbe(DriverPtr drv, int flags);
static Bool     NEOPreInit(ScrnInfoPtr pScrn, int flags);
static Bool     NEOScreenInit(int Index, ScreenPtr pScreen, int argc,
                                  char **argv);
static Bool     NEOEnterVT(int scrnIndex, int flags);
static void     NEOLeaveVT(int scrnIndex, int flags);
static Bool     NEOCloseScreen(int scrnIndex, ScreenPtr pScreen);
static void     NEOFreeScreen(int scrnIndex, int flags);
static int      NEOValidMode(int scrnIndex, DisplayModePtr mode,
                                 Bool verbose, int flags);

/* Internally used functions */
static int      neoFindIsaDevice(GDevPtr dev);
static Bool     neoModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void     neoSave(ScrnInfoPtr pScrn);
static void     neoRestore(ScrnInfoPtr pScrn, vgaRegPtr VgaReg,
				 NeoRegPtr NeoReg, Bool restoreText);
static void     neoLock(ScrnInfoPtr pScrn);
static void     neoUnlock(ScrnInfoPtr pScrn);
static Bool	neoMapMem(ScrnInfoPtr pScrn);
static Bool	neoUnmapMem(ScrnInfoPtr pScrn);
static void     neoProgramShadowRegs(ScrnInfoPtr pScrn, vgaRegPtr VgaReg,
				     NeoRegPtr restore);
static void     neoCalcVCLK(ScrnInfoPtr pScrn, long freq);
static xf86MonPtr  neo_ddc1(int scrnIndex);
static Bool     neoDoDDC1(ScrnInfoPtr pScrn);
static Bool     neoDoDDC2(ScrnInfoPtr pScrn);
static Bool     neoDoDDCVBE(ScrnInfoPtr pScrn);
static void     neoProbeDDC(ScrnInfoPtr pScrn, int index);
static void     NeoDisplayPowerManagementSet(ScrnInfoPtr pScrn,
				int PowerManagementMode, int flags);
static int      neoFindMode(int xres, int yres, int depth);

#define VERSION 4000
#define NEO_NAME "NEOMAGIC"
#define NEO_DRIVER_NAME "neomagic"

#define NEO_MAJOR_VERSION 1
#define NEO_MINOR_VERSION 0
#define NEO_PATCHLEVEL 0

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;


static biosMode bios8[] = {	
    { 320, 240, 0x40 },
    { 300, 400, 0x42 },    
    { 640, 400, 0x20 },
    { 640, 480, 0x21 },
    { 800, 600, 0x23 },
    { 1024, 768, 0x25 }
};

static biosMode bios15[] = {
    { 320, 200, 0x2D },
    { 640, 480, 0x30 },
    { 800, 600, 0x33 },
    { 1024, 768, 0x36 }
};

static biosMode bios16[] = {
    { 320, 200, 0x2e },
    { 320, 240, 0x41 },
    { 300, 400, 0x43 },
    { 640, 480, 0x31 },
    { 800, 600, 0x34 },
    { 1024, 768, 0x37 }
};

static biosMode bios24[] = {
    { 640, 480, 0x32 },
    { 800, 600, 0x35 },
    { 1024, 768, 0x38 }
};

static DisplayModeRec neo800x480Mode = {
	NULL,           /* prev */
	NULL,           /* next */   
	"800x480",      /* identifier of this mode */
	MODE_OK,        /* mode status */
	M_T_BUILTIN,    /* mode type */
	35260,		/* Clock frequency */
	800,		/* HDisplay */
	856,		/* HSyncStart */
	1040,		/* HSyncEnd */
	1056,		/* HTotal */
	0,		/* HSkew */
	480,		/* VDisplay */
	480,		/* VSyncStart */
	486,		/* VSyncEnd */
	488,		/* VTotal */
	0,		/* VScan */
	V_PHSYNC | V_PVSYNC,	/* Flags */
	-1,		/* ClockIndex */
	35260,		/* SynthClock */
	800,		/* CRTC HDisplay */
	800,            /* CRTC HBlankStart */
	856,            /* CRTC HSyncStart */
	1040,           /* CRTC HSyncEnd */
	872,            /* CRTC HBlankEnd */
	1048,           /* CRTC HTotal */
	0,              /* CRTC HSkew */
	480,		/* CRTC VDisplay */
	480,		/* CRTC VBlankStart */
	480,		/* CRTC VSyncStart */
	486,		/* CRTC VSyncEnd */
	487,		/* CRTC VBlankEnd */
	488,		/* CRTC VTotal */
	FALSE,		/* CrtcHAdjusted */
	FALSE,		/* CrtcVAdjusted */
	0,		/* PrivSize */
	NULL,		/* Private */
	0.0,		/* HSync */
	0.0		/* VRefresh */
};

static DisplayModeRec neo1024x480Mode = {
	NULL,           /* prev */
	NULL,           /* next */   
	"1024x480",      /* identifier of this mode */
	MODE_OK,        /* mode status */
	M_T_BUILTIN,    /* mode type */
	45900,		/* Clock frequency */
	1024,		/* HDisplay */
	1048,		/* HSyncStart */
	1184,		/* HSyncEnd */
	1344,		/* HTotal */
	0,		/* HSkew */
	480,		/* VDisplay */
	480,		/* VSyncStart */
	486,		/* VSyncEnd */
	488,		/* VTotal */
	0,		/* VScan */
	V_PHSYNC | V_PVSYNC,	/* Flags */
	-1,		/* ClockIndex */
	45900,		/* SynthClock */
	1024,		/* CRTC HDisplay */
	1024,            /* CRTC HBlankStart */
	1048,            /* CRTC HSyncStart */
	1184,           /* CRTC HSyncEnd */
	1072,            /* CRTC HBlankEnd */
	1344,           /* CRTC HTotal */
	0,              /* CRTC HSkew */
	480,		/* CRTC VDisplay */
	480,		/* CRTC VBlankStart */
	480,		/* CRTC VSyncStart */
	486,		/* CRTC VSyncEnd */
	487,		/* CRTC VBlankEnd */
	488,		/* CRTC VTotal */
	FALSE,		/* CrtcHAdjusted */
	FALSE,		/* CrtcVAdjusted */
	0,		/* PrivSize */
	NULL,		/* Private */
	0.0,		/* HSync */
	0.0		/* VRefresh */
};

/*
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec NEOMAGIC = {
    VERSION,
    NEO_DRIVER_NAME,
    NEOIdentify,
    NEOProbe,
    NEOAvailableOptions,
    NULL,
    0
};

static SymTabRec NEOChipsets[] = {
    { NM2070,   "neo2070" },
    { NM2090,   "neo2090" },
    { NM2093,   "neo2093" },
    { NM2097,   "neo2097" },
    { NM2160,   "neo2160" },
    { NM2200,   "neo2200" },
    { NM2230,   "neo2230" },
    { NM2360,   "neo2360" },
    { NM2380,   "neo2380" },
    { -1,		 NULL }
};

/* Conversion PCI ID to chipset name */
static PciChipsets NEOPCIchipsets[] = {
    { NM2070,  PCI_CHIP_NM2070,  RES_SHARED_VGA },
    { NM2090,  PCI_CHIP_NM2090,  RES_SHARED_VGA },
    { NM2093,  PCI_CHIP_NM2093,  RES_SHARED_VGA },
    { NM2097,  PCI_CHIP_NM2097,  RES_SHARED_VGA },
    { NM2160,  PCI_CHIP_NM2160,  RES_SHARED_VGA },
    { NM2200,  PCI_CHIP_NM2200,  RES_SHARED_VGA },
    { NM2230,  PCI_CHIP_NM2230,  RES_SHARED_VGA },
    { NM2360,  PCI_CHIP_NM2360,  RES_SHARED_VGA },
    { NM2380,  PCI_CHIP_NM2380,  RES_SHARED_VGA },
    { -1,	     -1,	     RES_UNDEFINED}
};

static IsaChipsets NEOISAchipsets[] = {
    { NM2070,               RES_EXCLUSIVE_VGA },
    { NM2090,               RES_EXCLUSIVE_VGA },
    { NM2093,               RES_EXCLUSIVE_VGA },
    { NM2097,               RES_EXCLUSIVE_VGA },
    { NM2160,               RES_EXCLUSIVE_VGA },
    { NM2200,               RES_EXCLUSIVE_VGA },
    { -1,			RES_UNDEFINED }
};

/* The options supported by the Neomagic Driver */
typedef enum {
    OPTION_NOLINEAR_MODE,
    OPTION_NOACCEL,
    OPTION_SW_CURSOR,
    OPTION_NO_MMIO,
    OPTION_INTERN_DISP,
    OPTION_EXTERN_DISP,
    OPTION_LCD_CENTER,
    OPTION_LCD_STRETCH,
    OPTION_SHADOW_FB,
    OPTION_PCI_BURST,
    OPTION_PROG_LCD_MODE_REGS,
    OPTION_PROG_LCD_MODE_STRETCH,
    OPTION_OVERRIDE_VALIDATE_MODE,
    OPTION_SHOWCACHE,
    OPTION_ROTATE,
    OPTION_VIDEO_KEY,
    OPTION_OVERLAYMEM,
    OPTION_VIDEO_INTERLACE,
    OPTION_DISPLAY_HEIGHT_480,
    OPTION_STRANGE_LOCKUPS
} NEOOpts;

static const OptionInfoRec NEO_2070_Options[] = {
    { OPTION_NOACCEL,	"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SW_CURSOR,	"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NO_MMIO,	"noMMIO",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_INTERN_DISP,"internDisp",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_EXTERN_DISP,"externDisp",  OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_LCD_CENTER, "LcdCenter",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_LCD_STRETCH, "NoStretch",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SHADOW_FB,   "ShadowFB",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_PCI_BURST,	 "pciBurst",	OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_SHOWCACHE,  "ShowCache",   OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_ROTATE, 	 "Rotate",	OPTV_ANYSTR,	{0}, FALSE },
    { OPTION_PROG_LCD_MODE_REGS, "progLcdModeRegs",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_PROG_LCD_MODE_STRETCH, "progLcdModeStretch",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_OVERRIDE_VALIDATE_MODE, "overrideValidateMode",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_VIDEO_KEY, "VideoKey",     OPTV_INTEGER,   {0}, FALSE },
    { OPTION_OVERLAYMEM, "OverlayMem",  OPTV_INTEGER,   {0}, FALSE },
    { OPTION_VIDEO_INTERLACE, "Interlace",
      OPTV_INTEGER,   {0}, FALSE },
    { -1,                  NULL,           OPTV_NONE,	{0}, FALSE }
};

static const OptionInfoRec NEOOptions[] = {
    { OPTION_NOLINEAR_MODE,"NoLinear",  OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOACCEL,	"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SW_CURSOR,	"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NO_MMIO,	"noMMIO",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_INTERN_DISP,"internDisp",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_EXTERN_DISP,"externDisp",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_LCD_CENTER, "LcdCenter",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SHADOW_FB,  "ShadowFB",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_LCD_STRETCH,"NoStretch",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_PCI_BURST,	 "pciBurst",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SHOWCACHE,  "ShowCache",   OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_ROTATE, 	 "Rotate",	OPTV_ANYSTR,	{0}, FALSE },
    { OPTION_STRANGE_LOCKUPS, "StrangeLockups", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DISPLAY_HEIGHT_480, "DisplayHeight480",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_PROG_LCD_MODE_REGS, "progLcdModeRegs",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_PROG_LCD_MODE_STRETCH, "progLcdModeStretch",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_OVERRIDE_VALIDATE_MODE, "overrideValidateMode",
      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_VIDEO_KEY, "VideoKey",     OPTV_INTEGER,   {0}, FALSE },
    { OPTION_OVERLAYMEM, "OverlayMem",  OPTV_INTEGER,   {0}, FALSE },
    { OPTION_VIDEO_INTERLACE, "Interlace",
      OPTV_INTEGER,   {0}, FALSE },
    { -1,                  NULL,           OPTV_NONE,	{0}, FALSE }
};

/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.
 */

static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIOBase",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWLock",
    "vgaHWMapMem",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    "vgaHWSetStdFuncs",
    "vgaHWUnlock",
    "vgaHWddc1SetSpeed",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
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

static const char *shadowSymbols[] = {
    "shadowInit",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    "xf86PrintEDID",
    "xf86SetDDCproperties",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(neoSetup);

static XF86ModuleVersionInfo neoVersRec =
{
	"neomagic",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	NEO_MAJOR_VERSION, NEO_MINOR_VERSION, NEO_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

/*
 * This is the module init data.
 * Its name has to be the driver name followed by ModuleData
 */
XF86ModuleData neomagicModuleData = { &neoVersRec, neoSetup, NULL };

static pointer
neoSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
        xf86AddDriver(&NEOMAGIC, module, 0);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * Tell the loader about symbols from other modules that this module
	 * might refer to.
	 */
	LoaderRefSymLists(vgahwSymbols, fbSymbols, xaaSymbols,
			  ramdacSymbols, shadowSymbols,
			  ddcSymbols, vbeSymbols, i2cSymbols, NULL);
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

static Bool
NEOGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate a NEORec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(NEORec), 1);

    if (pScrn->driverPrivate == NULL)
	return FALSE;
        return TRUE;
}

static void
NEOFreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static const OptionInfoRec *
NEOAvailableOptions(int chipid, int busid)
{
    int chip = (chipid & 0x0000ffff);

    if (chip == PCI_CHIP_NM2070)
	return NEO_2070_Options;
    else
    	return NEOOptions;
}

/* Mandatory */
static void
NEOIdentify(int flags)
{
    xf86PrintChipsets(NEO_NAME, "Driver for Neomagic chipsets",
			NEOChipsets);
}

/* Mandatory */
static Bool
NEOProbe(DriverPtr drv, int flags)
{
    Bool foundScreen = FALSE;
    int numDevSections, numUsed;
    GDevPtr *devSections;
    int *usedChips;
    int i;

    /*
     * Find the config file Device sections that match this
     * driver, and return if there are none.
     */
    if ((numDevSections = xf86MatchDevice(NEO_DRIVER_NAME,
					  &devSections)) <= 0) {
	return FALSE;
    }
  
    /* PCI BUS */
    if (xf86GetPciVideoInfo() ) {
	numUsed = xf86MatchPciInstances(NEO_NAME, PCI_VENDOR_NEOMAGIC,
					NEOChipsets, NEOPCIchipsets, 
					devSections,numDevSections,
					drv, &usedChips);

	if (numUsed > 0) {
	    if (flags & PROBE_DETECT)
		foundScreen = TRUE;
	    else for (i = 0; i < numUsed; i++) {
		ScrnInfoPtr pScrn = NULL;
		/* Allocate a ScrnInfoRec and claim the slot */
		if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
						       NEOPCIchipsets,NULL, NULL,
						       NULL, NULL, NULL))) {
		    pScrn->driverVersion = VERSION;
		    pScrn->driverName    = NEO_DRIVER_NAME;
		    pScrn->name          = NEO_NAME;
		    pScrn->Probe         = NEOProbe;
		    pScrn->PreInit       = NEOPreInit;
		    pScrn->ScreenInit    = NEOScreenInit;
		    pScrn->SwitchMode    = NEOSwitchMode;
		    pScrn->AdjustFrame   = NEOAdjustFrame;
		    pScrn->EnterVT       = NEOEnterVT;
		    pScrn->LeaveVT       = NEOLeaveVT;
		    pScrn->FreeScreen    = NEOFreeScreen;
		    pScrn->ValidMode     = NEOValidMode;
		    foundScreen = TRUE;
		}
	    }
	    xfree(usedChips);
	}
    }
    
    /* Isa Bus */

    numUsed = xf86MatchIsaInstances(NEO_NAME,NEOChipsets,NEOISAchipsets,
				     drv,neoFindIsaDevice,devSections,
				     numDevSections,&usedChips);
    if (numUsed > 0) {
      if (flags & PROBE_DETECT)
	foundScreen = TRUE;
      else for (i = 0; i < numUsed; i++) {
	ScrnInfoPtr pScrn = NULL;
	if ((pScrn = xf86ConfigIsaEntity(pScrn, 0, usedChips[i],
					 NEOISAchipsets, NULL, NULL,
					 NULL, NULL, NULL))) {
	    pScrn->driverVersion = VERSION;
	    pScrn->driverName    = NEO_DRIVER_NAME;
	    pScrn->name          = NEO_NAME;
	    pScrn->Probe         = NEOProbe;
	    pScrn->PreInit       = NEOPreInit;
	    pScrn->ScreenInit    = NEOScreenInit;
	    pScrn->SwitchMode    = NEOSwitchMode;
	    pScrn->AdjustFrame   = NEOAdjustFrame;
	    pScrn->EnterVT       = NEOEnterVT;
	    pScrn->LeaveVT       = NEOLeaveVT;
	    pScrn->FreeScreen    = NEOFreeScreen;
	    pScrn->ValidMode     = NEOValidMode;
	    foundScreen = TRUE;
	}
      }
      xfree(usedChips);
    }

    xfree(devSections);
    return foundScreen;
}

static int
neoFindIsaDevice(GDevPtr dev)
{
    unsigned int vgaIOBase;
    unsigned char id;
    
    vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
    /* §§§ Too intrusive ? */
    outw(GRAX, 0x2609); /* Unlock NeoMagic registers */

    outb(vgaIOBase + 4, 0x1A);
    id = inb(vgaIOBase + 5);

    outw(GRAX, 0x0009); /* Lock NeoMagic registers */

    switch (id) {
    case PROBED_NM2070 :
	return NM2070;
    case PROBED_NM2090 :
	return NM2090;
    case PROBED_NM2093 :
	return NM2093;
    default :
	return -1;
    }
}


/* Mandatory */
Bool
NEOPreInit(ScrnInfoPtr pScrn, int flags)
{
    ClockRangePtr clockRanges;
    int i;
    NEOPtr nPtr;
    vgaHWPtr hwp;
    int bppSupport = NoDepth24Support;
    int videoRam = 896;
    int maxClock = 65000;
    int CursorMem = 1024;
    int CursorOff = 0x100;
    int linearSize = 1024;
    int maxWidth = 1024;
    int maxHeight = 1024;
    unsigned char type, dpy;
    int w;
    int apertureSize;
    Bool height_480 = FALSE;
    Bool lcdCenterOptSet = FALSE;
    char *s;
    
    if (flags & PROBE_DETECT)  {
	neoProbeDDC( pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index );
	return TRUE;
    }
    
    /* The vgahw module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "vgahw"))
	return FALSE;

    xf86LoaderReqSymLists(vgahwSymbols, NULL);    

    /*
     * Allocate a vgaHWRec.
     */
    if (!vgaHWGetHWRec(pScrn))
	return FALSE;
    hwp = VGAHWPTR(pScrn);

    /* Allocate the NeoRec driverPrivate */
    if (!NEOGetRec(pScrn)) {
	return FALSE;
    }
# define RETURN \
    { NEOFreeRec(pScrn);\
			    return FALSE;\
					     }
    
    nPtr = NEOPTR(pScrn);

    /* Since, the capabilities are determined by the chipset the very
     * first thing to do is, figure out the chipset and its capabilities
     */
    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	RETURN;

    /* This is the general case */
    for (i = 0; i<pScrn->numEntities; i++) {
	nPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[i]);
	if (nPtr->pEnt->resources) return FALSE;
	nPtr->NeoChipset = nPtr->pEnt->chipset;
	pScrn->chipset = (char *)xf86TokenToString(NEOChipsets,
						   nPtr->pEnt->chipset);
	/* This driver can handle ISA and PCI buses */
	if (nPtr->pEnt->location.type == BUS_PCI) {
	    nPtr->PciInfo = xf86GetPciInfoForEntity(nPtr->pEnt->index);
	    nPtr->PciTag = pciTag(nPtr->PciInfo->bus, 
				  nPtr->PciInfo->device,
				  nPtr->PciInfo->func);
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Chipset is a ");
    switch(nPtr->NeoChipset){
    case NM2070:
	xf86ErrorF("MagicGraph 128 (NM2070)");
	break;
    case NM2090 :
	xf86ErrorF("MagicGraph 128V (NM2090)");
	break;
    case NM2093 :
	xf86ErrorF("MagicGraph 128ZV (NM2093)");
	break;
    case NM2097 :
	xf86ErrorF("MagicGraph 128ZV+ (NM2097)");
	break;
    case NM2160 :
	xf86ErrorF("MagicGraph 128XD (NM2160)");
	break;
    case NM2200 :
        xf86ErrorF("MagicMedia 256AV (NM2200)");
	break;
    case NM2230 :
        xf86ErrorF("MagicMedia 256AV+ (NM2230)");
	break;
    case NM2360 :
        xf86ErrorF("MagicMedia 256ZX (NM2360)");
	break;
    case NM2380 :
        xf86ErrorF("MagicMedia 256XL+ (NM2380)");
	break;
    }
    xf86ErrorF("\n");

    vgaHWGetIOBase(hwp);
    nPtr->vgaIOBase = hwp->IOBase;
    vgaHWSetStdFuncs(hwp);

    /* Determine the panel type */
    VGAwGR(0x09,0x26);
    type = VGArGR(0x21);
    dpy = VGArGR(0x20);
    
    /* Determine panel width -- used in NeoValidMode. */
    w = VGArGR(0x20);
    VGAwGR(0x09,0x00);
    switch ((w & 0x18) >> 3) {
    case 0x00 :
	nPtr->NeoPanelWidth  = 640;
	nPtr->NeoPanelHeight = 480;
	break;
    case 0x01 :
	nPtr->NeoPanelWidth  = 800;
	nPtr->NeoPanelHeight = 600;
	break;
    case 0x02 :
	nPtr->NeoPanelWidth  = 1024;
	nPtr->NeoPanelHeight = 768;
	break;
    case 0x03 :
        /* 1280x1024 panel support needs to be added */
#ifdef NOT_DONE
	nPtr->NeoPanelWidth  = 1280;
	nPtr->NeoPanelHeight = 1024;
	break;
#else
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		     "Only 640x480,\n"
                     "     800x600,\n"
                     " and 1024x768 panels are currently supported\n");
	return FALSE;
#endif
    default :
	nPtr->NeoPanelWidth  = 640;
	nPtr->NeoPanelHeight = 480;
	break;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Panel is a %dx%d %s %s display\n",
	       nPtr->NeoPanelWidth,
	       nPtr->NeoPanelHeight,
	       (type & 0x02) ? "color" : "monochrome",
	       (type & 0x10) ? "TFT" : "dual scan");


    switch (nPtr->NeoChipset){
    case NM2070:
	bppSupport = NoDepth24Support;
	videoRam   = 896;
	maxClock   = 65000;
	CursorMem  = 2048;
	CursorOff  = 0x100;
	linearSize = 1024;
	maxWidth   = 1024;
	maxHeight  = 1024;
	break;
    case NM2090:
    case NM2093:
	bppSupport = Support24bppFb | Support32bppFb |
	    SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 1152;
	maxClock   = 80000;
	CursorMem  = 2048;
	CursorOff  = 0x100;
	linearSize = 2048;
	maxWidth   = 1024;
	maxHeight  = 1024;
	break;
    case NM2097:
	bppSupport = Support24bppFb | Support32bppFb |
	  SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 1152;
	maxClock   = 80000;
	CursorMem  = 1024;
	CursorOff  = 0x100;
	linearSize = 2048;
	maxWidth   = 1024;
	maxHeight  = 1024;
	break;
    case NM2160:
	bppSupport = Support24bppFb | Support32bppFb |
	    SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 2048;
	maxClock   = 90000;
	CursorMem  = 1024;
	CursorOff  = 0x100;
	linearSize = 2048;
	maxWidth   = 1024;
	maxHeight  = 1024;
	break;
    case NM2200:
	bppSupport = Support24bppFb | Support32bppFb |
	    SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 2560;
	maxClock   = 110000;
	CursorMem  = 1024;
	CursorOff  = 0x1000;
	linearSize = 4096;
	maxWidth   = 1280;
	maxHeight  = 1024;  /* ???? */
	break;
    case NM2230:
	bppSupport = Support24bppFb | Support32bppFb |
	    SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 3008;
	maxClock   = 110000;
	CursorMem  = 1024;
	CursorOff  = 0x1000;
	linearSize = 4096;
	maxWidth   = 1280;
	maxHeight  = 1024;  /* ???? */
	break;
    case NM2360:
	bppSupport = Support24bppFb | Support32bppFb |
	    SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 4096;
	maxClock   = 110000;
	CursorMem  = 1024;
	CursorOff  = 0x1000;
	linearSize = 4096;
	maxWidth   = 1280;
	maxHeight  = 1024;  /* ???? */
	break;
    case NM2380:
	bppSupport = Support24bppFb | Support32bppFb |
	    SupportConvert32to24 | PreferConvert32to24;
	videoRam   = 6144;
	maxClock   = 110000;
	CursorMem  = 1024;
	CursorOff  = 0x1000;
	linearSize = 8192;
	maxWidth   = 1280;
	maxHeight  = 1024;  /* ???? */
	break;
    }

    pScrn->monitor = pScrn->confScreen->monitor;

    if (xf86LoadSubModule(pScrn, "ddc")) {
        xf86LoaderReqSymLists(ddcSymbols, NULL);
#if 1 /* for DDC1 testing */
	if (!neoDoDDCVBE(pScrn))
	  if (!neoDoDDC2(pScrn))
#endif
	      neoDoDDC1(pScrn);
    }

    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, bppSupport ))
	return FALSE;
    else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	case 15:
	case 16:
	    break;
	case 24:
	    if (nPtr->NeoChipset != NM2070)
		break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }
    xf86PrintDepthBpp(pScrn);
    if (pScrn->depth == 8)
	pScrn->rgbBits = 6;

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
	    return FALSE;
	} else {
	    /* XXX check that weight returned is supported */
	    ;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) 
	return FALSE;

    if (pScrn->depth > 1) {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros))
	    return FALSE;
    }

    nPtr->strangeLockups = TRUE;
    
    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);
    /* Process the options */
    if (nPtr->NeoChipset == NM2070) {
	if (!(nPtr->Options = xalloc(sizeof(NEO_2070_Options))))
	    return FALSE;
	memcpy(nPtr->Options, NEO_2070_Options, sizeof(NEO_2070_Options));
    } else {
	if (!(nPtr->Options = xalloc(sizeof(NEOOptions))))
	    return FALSE;
	memcpy(nPtr->Options, NEOOptions, sizeof(NEOOptions));
    }

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, nPtr->Options);

    xf86GetOptValBool(nPtr->Options, OPTION_NOLINEAR_MODE,&nPtr->noLinear);
    xf86GetOptValBool(nPtr->Options, OPTION_SW_CURSOR,&nPtr->swCursor);
    xf86GetOptValBool(nPtr->Options, OPTION_NO_MMIO,&nPtr->noMMIO);
    xf86GetOptValBool(nPtr->Options, OPTION_INTERN_DISP,&nPtr->internDisp);
    xf86GetOptValBool(nPtr->Options, OPTION_EXTERN_DISP,&nPtr->externDisp);
    if (xf86GetOptValBool(nPtr->Options, OPTION_LCD_CENTER,&nPtr->lcdCenter))
	lcdCenterOptSet = TRUE;
    xf86GetOptValBool(nPtr->Options, OPTION_LCD_STRETCH,&nPtr->noLcdStretch);
    xf86GetOptValBool(nPtr->Options, OPTION_SHADOW_FB,&nPtr->shadowFB);
    xf86GetOptValBool(nPtr->Options, OPTION_SHOWCACHE,&nPtr->showcache);
    nPtr->onPciBurst = TRUE;
    xf86GetOptValBool(nPtr->Options, OPTION_PCI_BURST,&nPtr->onPciBurst);
    xf86GetOptValBool(nPtr->Options,
		      OPTION_PROG_LCD_MODE_REGS,&nPtr->progLcdRegs);
    if (xf86GetOptValBool(nPtr->Options,
		      OPTION_PROG_LCD_MODE_STRETCH,&nPtr->progLcdStretch))
	nPtr->progLcdStretchOpt = TRUE;
    xf86GetOptValBool(nPtr->Options,
		      OPTION_OVERRIDE_VALIDATE_MODE, &nPtr->overrideValidate);
    xf86GetOptValBool(nPtr->Options, OPTION_DISPLAY_HEIGHT_480,&height_480);
    xf86GetOptValBool(nPtr->Options, OPTION_STRANGE_LOCKUPS,
		      &nPtr->strangeLockups);
    nPtr->noAccelSet =
	xf86GetOptValBool(nPtr->Options, OPTION_NOACCEL,&nPtr->noAccel);
    
    nPtr->rotate = 0;
    if ((s = xf86GetOptValString(nPtr->Options, OPTION_ROTATE))) {
	if(!xf86NameCmp(s, "CW")) {
	    /* accel is disabled below for shadowFB */
	    nPtr->shadowFB = TRUE;
	    nPtr->swCursor = TRUE;
	    nPtr->rotate = 1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		       "Rotating screen clockwise - acceleration disabled\n");
	} else if(!xf86NameCmp(s, "CCW")) {
	    nPtr->shadowFB = TRUE;
	    nPtr->swCursor = TRUE;
	    nPtr->rotate = -1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
		       "counter clockwise - acceleration disabled\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
		       "value for Option \"Rotate\"\n", s);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		       "Valid options are \"CW\" or \"CCW\"\n");
      }
    }
#ifdef XvExtension
    if(xf86GetOptValInteger(nPtr->Options,
			    OPTION_VIDEO_KEY, &(nPtr->videoKey))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
		   nPtr->videoKey);
    } else {
        nPtr->videoKey = (1 << pScrn->offset.red) | 
	    (1 << pScrn->offset.green) |
	    (((pScrn->mask.blue >> pScrn->offset.blue) - 1)
	     << pScrn->offset.blue); 
    }
    if(xf86GetOptValInteger(nPtr->Options, OPTION_OVERLAYMEM,
			    &(nPtr->overlay))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "reserve %d bytes for overlay.\n", nPtr->overlay);
    } else {
	nPtr->overlay = 0;
    }
    nPtr->interlace = 0;
    if(xf86GetOptValInteger(nPtr->Options, OPTION_VIDEO_INTERLACE,
			    &(nPtr->interlace))) {
	if (nPtr->interlace >= 0  &&  nPtr->interlace <= 2){
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "interlace flag = %d\n",
		       nPtr->interlace);
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "\"%s\" is not a valid value for "
		       "Option \"Interlaced\"\n", s);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Valid options are  0..2\n");
        }
    }
#endif /* XvExtension */


    if (height_480
	&& (nPtr->NeoPanelWidth == 800 || nPtr->NeoPanelWidth == 1024)) {
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Overriding Panel height: Set to 480\n");
	nPtr->NeoPanelHeight = 480;
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Disabling LCD stretching for panel height 480\n");
	nPtr->noLcdStretch = TRUE;
	if (!lcdCenterOptSet)
	    nPtr->lcdCenter = TRUE;
    }
    
    if (nPtr->internDisp && nPtr->externDisp)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Simultaneous LCD/CRT display mode\n");
    else if (nPtr->externDisp)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "External CRT only display mode\n");
    else  if (nPtr->internDisp)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Internal LCD only display mode\n");
    else {
	nPtr->internDisp = ((dpy & 0x02) == 0x02);
    	nPtr->externDisp = ((dpy & 0x01) == 0x01);
	if (nPtr->internDisp && nPtr->externDisp)
	    xf86DrvMsg(pScrn->scrnIndex,X_PROBED,
		       "Simultaneous LCD/CRT display mode\n");
	else if (nPtr->externDisp)
	    xf86DrvMsg(pScrn->scrnIndex,X_PROBED,
		       "External CRT only display mode\n");
	else if (nPtr->internDisp)
	    xf86DrvMsg(pScrn->scrnIndex,X_PROBED,
		       "Internal LCD only display mode\n");
	else {
	    /* this is a fallback if probed values are bogus */
	    nPtr->internDisp = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex,X_DEFAULT,
		       "Internal LCD only display mode\n");
	}
    }

    if (nPtr->noLcdStretch)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Low resolution video modes are not stretched\n");
    if (nPtr->lcdCenter)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Video modes are centered on the display\n");
    if (nPtr->noLinear)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG, "using nonlinear mode\n");
    else
	xf86DrvMsg(pScrn->scrnIndex,X_DEFAULT, "using linear mode\n");
    if (nPtr->swCursor)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG, "using sofware cursor\n");
    if (nPtr->noMMIO)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG, "MMIO mode disabled\n");
    if (nPtr->onPciBurst)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG, "using PCI Burst mode\n");
    if (nPtr->strangeLockups)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Option StrangeLockups set: disabling some acceleration\n");
    if (nPtr->showcache)
	xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,
		   "Show chache for debugging\n");
    if (nPtr->shadowFB) {
	if (nPtr->noLinear) {
    	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Option \"ShadowFB\" ignored. Not supported without"
		       " linear addressing\n");
	    nPtr->shadowFB = FALSE;
	    nPtr->rotate = 0;
	} else {
	    nPtr->noAccel = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		    "Using \"Shadow Framebuffer\" - acceleration disabled\n");
	}
    }

    nPtr->NeoFbMapSize = linearSize * 1024;
    nPtr->NeoCursorOffset = CursorOff;

    if (nPtr->pEnt->device->MemBase) {
	/* XXX Check this matches a PCI base address */
	nPtr->NeoLinearAddr = nPtr->pEnt->device->MemBase;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "FB base address is set at 0x%X.\n",
		   nPtr->NeoLinearAddr);
    } else {
	nPtr->NeoLinearAddr = 0;
    }

    nPtr->NeoMMIOAddr2 = 0;
    nPtr->NeoMMIOBase2 = NULL;
    if (nPtr->pEnt->device->IOBase && !nPtr->noMMIO) {
	/* XXX Check this matches a PCI base address */
	nPtr->NeoMMIOAddr = nPtr->pEnt->device->IOBase;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "MMIO base address is set at 0x%X.\n",
		   nPtr->NeoMMIOAddr);
    } else {
	nPtr->NeoMMIOAddr = 0;
    }
	
    if (nPtr->pEnt->location.type == BUS_PCI) {
	if (!nPtr->NeoLinearAddr) {
	    nPtr->NeoLinearAddr = nPtr->PciInfo->memBase[0];
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       "FB base address is set at 0x%X.\n",
		       nPtr->NeoLinearAddr);
	}
	if (!nPtr->NeoMMIOAddr && !nPtr->noMMIO) {
	    switch (nPtr->NeoChipset) {
	    case NM2070 :
		nPtr->NeoMMIOAddr = nPtr->NeoLinearAddr + 0x100000;
		break;
	    case NM2090:
	    case NM2093:
		nPtr->NeoMMIOAddr = nPtr->NeoLinearAddr + 0x200000;
		break;
	    case NM2160:
	    case NM2097:
	    case NM2200:
	    case NM2230:
	    case NM2360:
	    case NM2380:
		nPtr->NeoMMIOAddr = nPtr->PciInfo->memBase[1];
		nPtr->NeoMMIOAddr2 = nPtr->PciInfo->memBase[2];
		break;
	    }
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       "MMIO base address is set at 0x%X.\n",
		       nPtr->NeoMMIOAddr);
	    if (nPtr->NeoMMIOAddr2 != 0){
	        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		           "MMIO base address2 is set at 0x%X.\n",
		           nPtr->NeoMMIOAddr2);
	    }
	}
	/* XXX What about VGA resources in OPERATING mode? */
	if (xf86RegisterResources(nPtr->pEnt->index, NULL, ResExclusive))
	    RETURN;
	    
    } else if (nPtr->pEnt->location.type == BUS_ISA) {
	unsigned int addr;
	resRange linearRes[] = { {ResExcMemBlock|ResBios|ResBus,0,0},_END };
	
	if (!nPtr->NeoLinearAddr) {
	    VGAwGR(0x09,0x26);
	    addr = VGArGR(0x13);
	    VGAwGR(0x09,0x00);
	    nPtr->NeoLinearAddr = addr << 20;
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       "FB base address is set at 0x%X.\n",
		       nPtr->NeoLinearAddr);
	}
	if (!nPtr->NeoMMIOAddr && !nPtr->noMMIO) {
	    nPtr->NeoMMIOAddr = nPtr->NeoLinearAddr + 0x100000;
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       "MMIO base address is set at 0x%X.\n",
		       nPtr->NeoMMIOAddr);
	}
	linearRes[0].rBegin = nPtr->NeoLinearAddr;
	linearRes[1].rEnd = nPtr->NeoLinearAddr + nPtr->NeoFbMapSize - 1;
	if (xf86RegisterResources(nPtr->pEnt->index,linearRes,ResNone)) {
	    nPtr->noLinear = TRUE; /* XXX */
	}
    } else
	RETURN;

    if (nPtr->pEnt->device->videoRam != 0) {
	pScrn->videoRam = nPtr->pEnt->device->videoRam;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "VideoRAM: %d kByte\n",
		   pScrn->videoRam);
    } else {
	pScrn->videoRam = videoRam;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "VideoRAM: %d kByte\n",
		   pScrn->videoRam);
    }
    
    if (nPtr->pEnt->device->dacSpeeds[0] != 0) {
	maxClock = nPtr->pEnt->device->dacSpeeds[0];
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Max Clock: %d kHz\n",
		   maxClock);
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Max Clock: %d kHz\n",
		   maxClock);
    }

    pScrn->progClock = TRUE;
    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = (ClockRangePtr)xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->ClockMulFactor = 1;
    clockRanges->minClock = 11000;   /* guessed §§§ */
    clockRanges->maxClock = maxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    if (!nPtr->internDisp && nPtr->externDisp) 
	clockRanges->interlaceAllowed = TRUE; 
    else
	clockRanges->interlaceAllowed = FALSE; 
    clockRanges->doubleScanAllowed = TRUE;

    /* Subtract memory for HW cursor */
    if (!nPtr->swCursor)
	nPtr->NeoCursorMem = CursorMem;
    else
	nPtr->NeoCursorMem = 0;
    apertureSize = (pScrn->videoRam * 1024) - nPtr->NeoCursorMem;

   if ((nPtr->NeoPanelWidth == 800) && (nPtr->NeoPanelHeight == 480)) {
       neo800x480Mode.next = pScrn->monitor->Modes;
       pScrn->monitor->Modes = &neo800x480Mode;
   }
   if ((nPtr->NeoPanelWidth == 1024) && (nPtr->NeoPanelHeight == 480)) {
       neo1024x480Mode.next = pScrn->monitor->Modes;
       pScrn->monitor->Modes = &neo1024x480Mode;
   }
    /*
     * For external displays, limit the width to 1024 pixels or less.
     */
    {
       i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			  pScrn->display->modes, clockRanges,
			  NULL, 256, maxWidth,(8 * pScrn->bitsPerPixel),/*§§§*/
			  128, maxHeight, pScrn->display->virtualX,
			  pScrn->display->virtualY, apertureSize,
			  LOOKUP_BEST_REFRESH);

       if (i == -1)
           RETURN;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	RETURN;
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

    /* If monitor resolution is set on the command line, use it */
    xf86SetDpi(pScrn, 0, 0);

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	RETURN;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);

    if (!nPtr->noLinear) {
	if (!xf86LoadSubModule(pScrn, "xaa")) 
	    RETURN;
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    if (nPtr->shadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadow")) {
	    RETURN;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }
    
    if (!nPtr->swCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac"))
	    RETURN;
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }
    return TRUE;
}
#undef RETURN

/* Mandatory */
static Bool
NEOEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NEOPtr nPtr = NEOPTR(pScrn);
    
    /* Should we re-save the text mode on each VT enter? */
    if(!neoModeInit(pScrn, pScrn->currentMode))
      return FALSE;
#ifdef XvExtension
    if (nPtr->video)
	NEOResetVideo(pScrn);
#endif
    if (nPtr->NeoHWCursorShown) 
	NeoShowCursor(pScrn);
    NEOAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);    

    return TRUE;
}

/* Mandatory */
static void
NEOLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NEOPtr nPtr = NEOPTR(pScrn);
   
    /* Invalidate the cached acceleration registers */
    if (nPtr->NeoHWCursorShown) 
	NeoHideCursor(pScrn);
    neoRestore(pScrn, &(VGAHWPTR(pScrn))->SavedReg, &nPtr->NeoSavedReg, TRUE);
    neoLock(pScrn);
    
}

static void
NEOLoadPalette(
   ScrnInfoPtr pScrn,
   int numColors,
   int *indices,
   LOCO *colors,
   VisualPtr pVisual
){
   int i, index, shift, Gshift;
   vgaHWPtr hwp = VGAHWPTR(pScrn);

   switch(pScrn->depth) {
   case 15:	
	shift = Gshift = 1;
	break;
   case 16:
	shift = 0; 
        Gshift = 0;
	break;
   default:
	shift = Gshift = 0;
	break;
   }

   for(i = 0; i < numColors; i++) {
        index = indices[i];
        hwp->writeDacWriteAddr(hwp, index);
	DACDelay(hwp);
        hwp->writeDacData(hwp, colors[index].red << shift);
	DACDelay(hwp);
        hwp->writeDacData(hwp, colors[index].green << Gshift);
	DACDelay(hwp);
        hwp->writeDacData(hwp, colors[index].blue << shift);
	DACDelay(hwp);
   } 
}

/* Mandatory */
static Bool
NEOScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    NEOPtr nPtr;
    NEOACLPtr nAcl;
    int ret;
    VisualPtr visual;
    int allocatebase, freespace, currentaddr;
    unsigned int racflag = RAC_FB;
    unsigned char *FBStart;
    int height, width, displayWidth;
    
    /*
     * we need to get the ScrnInfoRec for this screen, so let's allocate
     * one first thing
     */
    pScrn = xf86Screens[pScreen->myNum];
    nPtr = NEOPTR(pScrn);
    nAcl = NEOACLPTR(pScrn);

    hwp = VGAHWPTR(pScrn);
    hwp->MapSize = 0x10000;		/* Standard 64k VGA window */
    /* Map the VGA memory */
    if (!vgaHWMapMem(pScrn))
	return FALSE;

    /* Map the Neo memory and possible MMIO areas */
    if (!neoMapMem(pScrn))
	return FALSE;
    
    /*
     * next we save the current state and setup the first mode
     */
    neoSave(pScrn);
    
    if (!neoModeInit(pScrn,pScrn->currentMode))
	return FALSE;
    vgaHWSaveScreen(pScreen,SCREEN_SAVER_ON);
    NEOAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /*
     * Reset visual list.
     */
    miClearVisualTypes();
    
    /* Setup the visuals we support. */
    
    if (!miSetVisualTypes(pScrn->depth,
      		      miGetDefaultVisualMask(pScrn->depth),
		      pScrn->rgbBits, pScrn->defaultVisual))
         return FALSE;

    if (!miSetPixmapDepths ()) return FALSE;

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */
    displayWidth = pScrn->displayWidth;
    if (nPtr->rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    } else {
	width = pScrn->virtualX;
	height = pScrn->virtualY;
    }
    
    if(nPtr->shadowFB) {
	nPtr->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
	nPtr->ShadowPtr = xalloc(nPtr->ShadowPitch * height);
	displayWidth = nPtr->ShadowPitch / (pScrn->bitsPerPixel >> 3);
	FBStart = nPtr->ShadowPtr;
    } else {
	nPtr->ShadowPtr = NULL;
	FBStart = nPtr->NeoFbBase;
    }
    
    ret = fbScreenInit(pScreen, FBStart,
			    width, height,
			    pScrn->xDpi, pScrn->yDpi,
			    displayWidth, pScrn->bitsPerPixel);
    if (!ret)
	return FALSE;

    if (pScrn->depth > 8) {
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
    fbPictureInit(pScreen, 0, 0);

    xf86SetBlackWhitePixels(pScreen);

    if (!nPtr->shadowFB)
	NEODGAInit(pScreen);
    
    nPtr->NeoHWCursorShown = FALSE;
    nPtr->NeoHWCursorInitialized = FALSE;
    nAcl->UseHWCursor = FALSE;
    nAcl->CursorAddress = -1;
    
    if (nPtr->noLinear) {
	miBankInfoPtr pBankInfo;

	/* Setup the vga banking variables */
	pBankInfo = (miBankInfoPtr)xnfcalloc(sizeof(miBankInfoRec),1);
	if (pBankInfo == NULL)
	    return FALSE;
	
	pBankInfo->pBankA = hwp->Base;
	pBankInfo->pBankB = (unsigned char *)hwp->Base;
	pBankInfo->BankSize = 0x10000;
	pBankInfo->nBankDepth = pScrn->depth;
	
	pBankInfo->SetSourceBank = (miBankProcPtr)NEOSetRead;
	pBankInfo->SetDestinationBank =
	    (miBankProcPtr)NEOSetWrite;
	pBankInfo->SetSourceAndDestinationBanks =
	    (miBankProcPtr)NEOSetReadWrite;
	if (!miInitializeBanking(pScreen, pScrn->virtualX, pScrn->virtualY,
				 pScrn->displayWidth, pBankInfo)) {
	    xfree(pBankInfo);
	    pBankInfo = NULL;
	    return FALSE;
	}
	xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Using nonlinear mode\n");
	xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Using software cursor in "
		   "nonlinear mode\n");
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);
        xf86SetSilkenMouse(pScreen);

	/* Initialise cursor functions */
	miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    } else {
	nAcl->cacheStart = -1;
	nAcl->cacheEnd = -1;
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,
		   "Using linear framebuffer at: 0x%08lX\n",
		   nPtr->NeoLinearAddr);
	/* Setup pointers to free space in video ram */
	allocatebase = (pScrn->videoRam << 10);
	freespace = allocatebase - pScrn->displayWidth *
	    pScrn->virtualY * (pScrn->bitsPerPixel >> 3);
	currentaddr = allocatebase;
	xf86DrvMsg(scrnIndex, X_PROBED,
		   "%d bytes off-screen memory available\n", freespace);

	if (nPtr->swCursor || !nPtr->NeoMMIOBase) {
	    xf86DrvMsg(scrnIndex, X_CONFIG,
		       "Using Software Cursor.\n");
	} else if (nPtr->NeoCursorMem <= freespace) {
	    currentaddr -= nPtr->NeoCursorMem;
	    freespace  -= nPtr->NeoCursorMem;
	    /* alignment */
	    freespace  -= currentaddr & 0x3FF;
	    currentaddr &= 0xfffffc00;
	    nAcl->CursorAddress = currentaddr;
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Using H/W Cursor.\n"); 
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Too little space for H/W cursor.\n");
	}
	
	if (!nPtr->noAccel && !nPtr->NeoMMIOBase)
	  xf86DrvMsg(pScrn->scrnIndex,X_INFO,
		     "Acceleration disabled when not using MMIO\n");

#ifdef XvExtension
	if (nPtr->overlay > 0){
	    if (nPtr->overlay > freespace){
		xf86DrvMsg(pScrn->scrnIndex,X_INFO,
			   "Can not reserve %d bytes for overlay. "
			   "Resize to %d bytes.\n",
			   nPtr->overlay, freespace);
		nPtr->overlay = freespace;
	    }
	    currentaddr -= nPtr->overlay;
	    freespace -= nPtr->overlay;
	    nPtr->overlay_offset = currentaddr;
	    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"Overlay at 0x%x\n",
		       nPtr->overlay_offset);
	}
#endif /* XvExtension */
	nAcl->cacheStart = currentaddr - freespace;
	nAcl->cacheEnd = currentaddr;
	freespace = 0;
	if (nAcl->cacheStart < nAcl->cacheEnd) {
	    BoxRec AvailFBArea;
	    int lines = nAcl->cacheEnd /
		(pScrn->displayWidth * (pScrn->bitsPerPixel >> 3));
	    if (!nPtr->noAccel && nPtr->NeoMMIOBase && lines > 1024) 
		lines = 1024;
	    AvailFBArea.x1 = 0;
	    AvailFBArea.y1 = 0;
	    AvailFBArea.x2 = pScrn->displayWidth;
	    AvailFBArea.y2 = lines;
	    xf86InitFBManager(pScreen, &AvailFBArea); 
	    
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		       "Using %i scanlines of offscreen memory \n",
		       lines - pScrn->virtualY);
	}

	/* Setup the acceleration primitives */
	if (!nPtr->noAccel && nPtr->NeoMMIOBase) {
	    Bool ret = FALSE;
	    if (nAcl->cacheStart >= nAcl->cacheEnd) {
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "Too little space for pixmap cache.\n");
	    } 	    
	    switch(nPtr->NeoChipset) {
	    case NM2070 :
		ret = Neo2070AccelInit(pScreen);
		break;
	    case NM2090 :
	    case NM2093 :
		ret = Neo2090AccelInit(pScreen);
		break;
	    case NM2097 :
	    case NM2160 :
		ret = Neo2097AccelInit(pScreen);
		break;
	    case NM2200 :
	    case NM2230 :
	    case NM2360 :
	    case NM2380 :
	        ret = Neo2200AccelInit(pScreen);
		break;
	    }
	    xf86DrvMsg(pScrn->scrnIndex,X_INFO,
		       "Acceleration %s Initialized\n",ret ? "" : "not");
	} 

	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);
        xf86SetSilkenMouse(pScreen);
	
	/* Initialise cursor functions */
	miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    }

    if (nAcl->CursorAddress != -1) {
      /* HW cursor functions */
      if (!NeoCursorInit(pScreen)) {
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Hardware cursor initialization failed\n");
	return FALSE;
      }
      nAcl->UseHWCursor = TRUE;
      nPtr->NeoHWCursorInitialized = TRUE;
    } else
      nAcl->UseHWCursor = FALSE;
    
    if (nPtr->shadowFB) {
        nPtr->refreshArea = neoRefreshArea;

	if(nPtr->rotate) {
	    if (!nPtr->PointerMoved) {
		nPtr->PointerMoved = pScrn->PointerMoved;
		pScrn->PointerMoved = neoPointerMoved;
	    }
	    
	   switch(pScrn->bitsPerPixel) {
	   case 8:	nPtr->refreshArea = neoRefreshArea8;	break;
	   case 16:	nPtr->refreshArea = neoRefreshArea16;	break;
	   case 24:	nPtr->refreshArea = neoRefreshArea24;	break;
	   case 32:	nPtr->refreshArea = neoRefreshArea32;	break;
	   }
	}
#if 0
	ShadowFBInit(pScreen, nPtr->refreshArea);
#else
	shadowInit (pScreen, neoShadowUpdate, 0);
#endif
    }
    
    /* Initialise default colourmap */
    if(!miCreateDefColormap(pScreen))
	return FALSE;

    if (!xf86HandleColormaps(pScreen, 256, pScrn->rgbBits,
                         NEOLoadPalette, NULL, 
                         CMAP_PALETTED_TRUECOLOR | CMAP_RELOAD_ON_MODE_SWITCH))
	return FALSE;

    racflag |= RAC_COLORMAP;
    if (nPtr->NeoHWCursorInitialized)
        racflag |= RAC_CURSOR;

    pScrn->racIoFlags = pScrn->racMemFlags = racflag;

    NEOInitVideo(pScreen);

    pScreen->SaveScreen = vgaHWSaveScreen;

    /* Setup DPMS mode */
    if (nPtr->NeoChipset != NM2070)
	xf86DPMSInit(pScreen, (DPMSSetProcPtr)NeoDisplayPowerManagementSet,
		     0);

    if (!nPtr->noLinear) {
        pScrn->memPhysBase = (unsigned long)nPtr->NeoFbBase;
	pScrn->fbOffset = 0;
    }
    
    /* Wrap the current CloseScreen function */
    nPtr->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = NEOCloseScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    return TRUE;
}

/* Mandatory */
Bool
NEOSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return neoModeInit(xf86Screens[scrnIndex], mode);
}

/* Mandatory */
void
NEOAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn;
    NEOPtr nPtr;
    vgaHWPtr hwp;
    int oldExtCRTDispAddr;
    int Base; 

    pScrn = xf86Screens[scrnIndex];
    hwp = VGAHWPTR(pScrn);
    nPtr = NEOPTR(pScrn);

    if (nPtr->showcache && y) {
	int lastline = nPtr->NeoFbMapSize / 
	    ((pScrn->displayWidth * pScrn->bitsPerPixel) / 8);
	
	lastline -= pScrn->currentMode->VDisplay;
	y += pScrn->virtualY - 1;
        if (y > lastline) y = lastline;
    }

    Base = (y * pScrn->displayWidth + x) >> 2;

    /* Scale Base by the number of bytes per pixel. */
    switch (pScrn->depth) {
    case  8 :
	break;
    case 15 :
    case 16 :
	Base *= 2;
	break;
    case 24 :
	Base *= 3;
	break;
    default :
	break;
    }
    /*
     * These are the generic starting address registers.
     */
    VGAwCR(0x0C, (Base & 0x00FF00) >> 8);
    VGAwCR(0x0D, (Base & 0x00FF));

    /*
     * Make sure we don't clobber some other bits that might already
     * have been set. NOTE: NM2200 has a writable bit 3, but it shouldn't
     * be needed.
     */
    oldExtCRTDispAddr = VGArGR(0x0E);
    VGAwGR(0x0E,(((Base >> 16) & 0x07) | (oldExtCRTDispAddr & 0xf8)));
#if 0
    /*
     * This is a workaround for a higher level bug that causes the cursor
     * to be at the wrong position after a virtual screen resolution change
     */
    if (nPtr->NeoHWCursorInitialized) { /*§§§ do we still need this?*/
	NeoRepositionCursor();
    }
#endif
}

/* Mandatory */
static Bool
NEOCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NEOPtr nPtr = NEOPTR(pScrn);

    if(pScrn->vtSema){
	if (nPtr->NeoHWCursorShown)
	    NeoHideCursor(pScrn);
	neoRestore(pScrn, &(VGAHWPTR(pScrn))->SavedReg, &nPtr->NeoSavedReg, TRUE);

	neoLock(pScrn);
	neoUnmapMem(pScrn);
    }
    if (nPtr->AccelInfoRec)
	XAADestroyInfoRec(nPtr->AccelInfoRec);
    if (nPtr->CursorInfo)
	xf86DestroyCursorInfoRec(nPtr->CursorInfo);
    if (nPtr->ShadowPtr)
	xfree(nPtr->ShadowPtr);

    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = nPtr->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/* Optional */
static void
NEOFreeScreen(int scrnIndex, int flags)
{
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    NEOFreeRec(xf86Screens[scrnIndex]);
}

/* Optional */
static int
NEOValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NEOPtr nPtr = NEOPTR(pScrn);

    /*
     * Limit the modes to just those allowed by the various NeoMagic
     * chips.  
     */

    if (nPtr->overrideValidate) {
	xf86DrvMsg(scrnIndex, X_WARNING, "display mode validation disabled\n");
    } else {
	/*
	 * When the LCD is active, only allow modes that are (1) equal to
	 * or smaller than the size of the panel and (2) are one of the
	 * following sizes: 1024x768, 800x600, 640x480.
	 */
	if (nPtr->internDisp || !nPtr->externDisp) {
	    /* Is the mode larger than the LCD panel? */
	    if ((mode->HDisplay > nPtr->NeoPanelWidth) ||
		(mode->VDisplay > nPtr->NeoPanelHeight)) {
		xf86DrvMsg(scrnIndex,X_INFO, "Removing mode (%dx%d) "
			   "larger than the LCD panel (%dx%d)\n",
			   mode->HDisplay,
			   mode->VDisplay,
			   nPtr->NeoPanelWidth,
			   nPtr->NeoPanelHeight);
		return(MODE_BAD);
	    }

	    /* Is the mode one of the acceptable sizes? */
	    switch (mode->HDisplay) {
	    case 1280:
		if (mode->VDisplay == 1024)
		    return(MODE_OK);
		break;
	    case 1024 :
		if (mode->VDisplay == 768)
		    return(MODE_OK);
		if ((mode->VDisplay == 480) && (nPtr->NeoPanelHeight == 480))
		    return(MODE_OK);
		break;
	    case  800 :
		if (mode->VDisplay == 600) 
		    return(MODE_OK);
		if ((mode->VDisplay == 480) && (nPtr->NeoPanelHeight == 480))
		    return(MODE_OK);
		break;
	    case  640 :
		if (mode->VDisplay == 480)
		    return(MODE_OK);
		break;
#if 1
	    case 320:
		if (mode->VDisplay == 240)
		    return(MODE_OK);
		break;
#endif
	    default:
		break;
	    }

	    xf86DrvMsg(scrnIndex, X_INFO, "Removing mode (%dx%d) that won't "
		       "display properly on LCD\n",
		       mode->HDisplay,
		       mode->VDisplay);
	    return(MODE_BAD);
	}
    }
    return(MODE_OK);
}

static void
neoLock(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    
    VGAwGR(0x09,0x00);
    vgaHWLock(hwp);
}

static void
neoUnlock(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    
    vgaHWUnlock(hwp);
    VGAwGR(0x09,0x26);
}

static Bool
neoMapMem(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    if (!nPtr->noLinear) {
	if (!nPtr->noMMIO) {
	    if (nPtr->pEnt->location.type == BUS_PCI){
		nPtr->NeoMMIOBase =
		    xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
				  nPtr->PciTag, nPtr->NeoMMIOAddr,
				  0x200000L);
		if (nPtr->NeoMMIOAddr2 != 0){
		    nPtr->NeoMMIOBase2 =
		        xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
				      nPtr->PciTag, nPtr->NeoMMIOAddr2,
				      0x100000L);
		}
	    } else
		nPtr->NeoMMIOBase =
		    xf86MapVidMem(pScrn->scrnIndex,
				  VIDMEM_MMIO, nPtr->NeoMMIOAddr,
				  0x200000L);
	    if (nPtr->NeoMMIOBase == NULL)
	        return FALSE;
	}

	if (nPtr->pEnt->location.type == BUS_PCI)
	    nPtr->NeoFbBase =
		xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
			      nPtr->PciTag,
			      (unsigned long)nPtr->NeoLinearAddr,
			      nPtr->NeoFbMapSize);
	else
	    nPtr->NeoFbBase =
		xf86MapVidMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
			      (unsigned long)nPtr->NeoLinearAddr,
			      nPtr->NeoFbMapSize);
	if (nPtr->NeoFbBase == NULL)
	    return FALSE;
    } else {
	/* In paged mode Base is the VGA window at 0xA0000 */
	nPtr->NeoFbBase = hwp->Base;
    }
    return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
neoUnmapMem(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);

    if (!nPtr->noLinear) {
      if (nPtr->NeoMMIOBase)
	  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)nPtr->NeoMMIOBase,
			  0x200000L);
      nPtr->NeoMMIOBase = NULL;
      if (nPtr->NeoMMIOBase2)
	  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)nPtr->NeoMMIOBase2,
			  0x100000L);
      nPtr->NeoMMIOBase2 = NULL;
      xf86UnMapVidMem(pScrn->scrnIndex, (pointer)nPtr->NeoFbBase,
		    nPtr->NeoFbMapSize); 
    }
    nPtr->NeoFbBase = NULL;
    
    return TRUE;
}

static void
neoSave(ScrnInfoPtr pScrn)
{
    vgaRegPtr VgaSave = &VGAHWPTR(pScrn)->SavedReg;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    NEOPtr nPtr = NEOPTR(pScrn);
    NeoRegPtr save;
    int i;

    save = &nPtr->NeoSavedReg;

    VGAwGR(0x09,0x26);
    /*
     * Whatever code is needed to get back to bank zero goes here.
     */
    VGAwGR(0x15,0x00);
    
    /* get generic registers */
    vgaHWSave(pScrn, VgaSave, VGA_SR_ALL);

    /*
     * The port I/O code necessary to read in the extended registers 
     * into the fields of the vgaNeoRec structure goes here.
     */

    save->GeneralLockReg = VGArGR(0x0A);
    
    save->ExtCRTDispAddr = VGArGR(0x0E);
    if (nPtr->NeoChipset != NM2070) {
	save->ExtCRTOffset = VGArGR(0x0F);
    }
    save->SysIfaceCntl1 = VGArGR(0x10);
    save->SysIfaceCntl2 = VGArGR(0x11);
    save->SingleAddrPage = VGArGR(0x15);
    save->DualAddrPage = VGArGR(0x16);
    save->PanelDispCntlReg1 = VGArGR(0x20);
    save->PanelDispCntlReg2 = VGArGR(0x25);
    save->PanelDispCntlReg3 = VGArGR(0x30);
    save->PanelVertCenterReg1 = VGArGR(0x28);
    save->PanelVertCenterReg2 = VGArGR(0x29);
    save->PanelVertCenterReg3 = VGArGR(0x2A);
    if (nPtr->NeoChipset != NM2070) {
        save->PanelVertCenterReg4 = VGArGR(0x32);
	save->PanelHorizCenterReg1 = VGArGR(0x33);
	save->PanelHorizCenterReg2 = VGArGR(0x34);
	save->PanelHorizCenterReg3 = VGArGR(0x35);
    }
    if (nPtr->NeoChipset == NM2160) {
        save->PanelHorizCenterReg4 = VGArGR(0x36);
    }
    if (nPtr->NeoChipset == NM2200 || nPtr->NeoChipset == NM2230
	|| nPtr->NeoChipset == NM2360 || nPtr->NeoChipset == NM2380) {
	save->PanelHorizCenterReg4 = VGArGR(0x36);
	save->PanelVertCenterReg5  = VGArGR(0x37);
	save->PanelHorizCenterReg5 = VGArGR(0x38);
    }
    save->ExtColorModeSelect = VGArGR(0x90);
    save->VCLK3NumeratorLow = VGArGR(0x9B);
    if (nPtr->NeoChipset == NM2200 || nPtr->NeoChipset == NM2230
	|| nPtr->NeoChipset == NM2360 || nPtr->NeoChipset == NM2380)
	save->VCLK3NumeratorHigh = VGArGR(0x8F);
    save->VCLK3Denominator = VGArGR(0x9F);
    save->ProgramVCLK = TRUE;
    
    if (save->reg == NULL)
        save->reg = (regSavePtr)xnfcalloc(sizeof(regSaveRec), 1);
    else
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Non-NULL reg in NeoSave: reg=0x%08X\n", save->reg);

    save->reg->CR[0x23] = VGArCR(0x23);
    save->reg->CR[0x25] = VGArCR(0x25);
    save->reg->CR[0x2F] = VGArCR(0x2F);
    for (i = 0x40; i <= 0x59; i++) {
	save->reg->CR[i] = VGArCR(i);
    }
    for (i = 0x60; i <= 0x69; i++) {
	save->reg->CR[i] = VGArCR(i);
    }
    for (i = 0x70; i <= NEO_EXT_CR_MAX; i++) {
	save->reg->CR[i] = VGArCR(i);
    }

    for (i = 0x0A; i <= NEO_EXT_GR_MAX; i++) {
        save->reg->GR[i] = VGArGR(i);
    }
}

/*
 * neoProgramShadowRegs
 *
 * Setup the shadow registers to their default values.  The NeoSave
 * routines will restore the proper values on server exit.
 */
static void
neoProgramShadowRegs(ScrnInfoPtr pScrn, vgaRegPtr VgaReg, NeoRegPtr restore)
{
    int i;
    Bool noProgramShadowRegs;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    NEOPtr nPtr = NEOPTR(pScrn);
    Bool prog_lcd;

    /*
     * If display is external only and we want internal
     * we need to program the shadow registers.
     */
    prog_lcd = (((VGArGR(0x20) & 0x3) == 0x1) && nPtr->internDisp);

    
    /*
     * Convoluted logic for shadow register programming.
     *
     * As far as we know, shadow programming is needed for the 2070,
     * but not in stretched modes.  Special case this code.
     */
    switch (nPtr->NeoChipset) {
    case NM2070:
	/* Program the shadow regs by default */
	noProgramShadowRegs = FALSE;
	if (!nPtr->progLcdRegs && !prog_lcd)
	    noProgramShadowRegs = TRUE;

	if (restore->PanelDispCntlReg2 & 0x84) {
	    /* Don't program by default if in stretch mode */
	    noProgramShadowRegs = TRUE;
	    if (nPtr->progLcdStretch)  
		noProgramShadowRegs = FALSE;
	}
	break;
    case NM2090:
    case NM2093:
    case NM2097:
    case NM2160:
    case NM2200:
    case NM2230:
    case NM2360:
    case NM2380:
    default:
	/* Don't program the shadow regs by default */
	noProgramShadowRegs = TRUE;
	if (nPtr->progLcdRegs || prog_lcd)
	    noProgramShadowRegs = FALSE;

	if (restore->PanelDispCntlReg2 & 0x84) {
	    /* Only change the behavior if an option is set */
	    if (nPtr->progLcdStretchOpt)
		noProgramShadowRegs = !nPtr->progLcdStretch;
	}
	break;
    }

    if (noProgramShadowRegs) {
	xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,5,"Not programming shadow registers\n");
	if (nPtr->NeoSavedReg.reg){
	    for (i = 0x40; i <= 0x59; i++) {
		VGAwCR(i, nPtr->NeoSavedReg.reg->CR[i]);
	    }
	    for (i = 0x60; i <= 0x64; i++) {
		VGAwCR(i, nPtr->NeoSavedReg.reg->CR[i]);
	    } 
	} 
    } else {
	/*
	 * Program the shadow regs based on the panel width.  This works
	 * fine for normal sized panels, but what about the odd ones like
	 * the Libretto 100 which has an 800x480 panel???
	 */
	switch (nPtr->NeoPanelWidth) {
	case 640 :
	    VGAwCR(0x40,0x5F);
	    VGAwCR(0x41,0x50);
	    VGAwCR(0x42,0x02);
	    VGAwCR(0x43,0x55);
	    VGAwCR(0x44,0x81);
	    VGAwCR(0x45,0x0B);
	    VGAwCR(0x46,0x2E);
	    VGAwCR(0x47,0xEA);
	    VGAwCR(0x48,0x0C);
	    VGAwCR(0x49,0xE7);
	    VGAwCR(0x4A,0x04);
	    VGAwCR(0x4B,0x2D);
	    VGAwCR(0x4C,0x28);
	    VGAwCR(0x4D,0x90);
	    VGAwCR(0x4E,0x2B);
	    VGAwCR(0x4F,0xA0);
	    break;
	case 800 :
	    switch (nPtr->NeoPanelHeight) {
	    case 600:
		VGAwCR(0x40,0x7F);
		VGAwCR(0x41,0x63);
		VGAwCR(0x42,0x02);
		VGAwCR(0x43,0x6C);
		VGAwCR(0x44,0x1C);
		VGAwCR(0x45,0x72);
		VGAwCR(0x46,0xE0);
		VGAwCR(0x47,0x58);
		VGAwCR(0x48,0x0C);
		VGAwCR(0x49,0x57);
		VGAwCR(0x4A,0x73);
		VGAwCR(0x4B,0x3D);
		VGAwCR(0x4C,0x31);
		VGAwCR(0x4D,0x01);
		VGAwCR(0x4E,0x36);
		VGAwCR(0x4F,0x1E);
		if (nPtr->NeoChipset != NM2070) {
		    VGAwCR(0x50,0x6B);
		    VGAwCR(0x51,0x4F);
		    VGAwCR(0x52,0x0E);
		    VGAwCR(0x53,0x58);
		    VGAwCR(0x54,0x88);
		    VGAwCR(0x55,0x33);
		    VGAwCR(0x56,0x27);
		    VGAwCR(0x57,0x16);
		    VGAwCR(0x58,0x2C);
		    VGAwCR(0x59,0x94);
		}
		break;
	    case 480:
		VGAwCR(0x40,0x7F);
		VGAwCR(0x41,0x63);
		VGAwCR(0x42,0x02);
		VGAwCR(0x43,0x6B);
		VGAwCR(0x44,0x1B);
		VGAwCR(0x45,0x72);
		VGAwCR(0x46,0xE0);
		VGAwCR(0x47,0x1C);
		VGAwCR(0x48,0x00);
		VGAwCR(0x49,0x57);
		VGAwCR(0x4A,0x73);
		VGAwCR(0x4B,0x3E);
		VGAwCR(0x4C,0x31);
		VGAwCR(0x4D,0x01);
		VGAwCR(0x4E,0x36);
		VGAwCR(0x4F,0x1E);
		VGAwCR(0x50,0x6B);
		VGAwCR(0x51,0x4F);
		VGAwCR(0x52,0x0E);
		VGAwCR(0x53,0x57);
		VGAwCR(0x54,0x87);
		VGAwCR(0x55,0x33);
		VGAwCR(0x56,0x27);
		VGAwCR(0x57,0x16);
		VGAwCR(0x58,0x2C);
		VGAwCR(0x59,0x94);
		break;
		break;
		/* Not done */
	    }
	    break;
	case 1024 :
	    switch (nPtr->NeoPanelHeight) {
	    case 768:
		VGAwCR(0x40,0xA3);
		VGAwCR(0x41,0x7F);
		VGAwCR(0x42,0x06);
		VGAwCR(0x43,0x85);
		VGAwCR(0x44,0x96);
		VGAwCR(0x45,0x24);
		VGAwCR(0x46,0xE5);
		VGAwCR(0x47,0x02);
		VGAwCR(0x48,0x08);
		VGAwCR(0x49,0xFF);
		VGAwCR(0x4A,0x25);
		VGAwCR(0x4B,0x4F);
		VGAwCR(0x4C,0x40);
		VGAwCR(0x4D,0x00);
		VGAwCR(0x4E,0x44);
		VGAwCR(0x4F,0x0C);
		VGAwCR(0x50,0x7A);
		VGAwCR(0x51,0x56);
		VGAwCR(0x52,0x00);
		VGAwCR(0x53,0x5D);
		VGAwCR(0x54,0x0E);
		VGAwCR(0x55,0x3B);
		VGAwCR(0x56,0x2B);
		VGAwCR(0x57,0x00);
		VGAwCR(0x58,0x2F);
		VGAwCR(0x59,0x18);
		VGAwCR(0x60,0x88);
		VGAwCR(0x61,0x63);
		VGAwCR(0x62,0x0B);
		VGAwCR(0x63,0x69);
		VGAwCR(0x64,0x1A);
		break;
	    case 480:
		VGAwCR(0x40,0xA3);
		VGAwCR(0x41,0x7F);
		VGAwCR(0x42,0x1B);
		VGAwCR(0x43,0x89);
		VGAwCR(0x44,0x16);
		VGAwCR(0x45,0x0B);
		VGAwCR(0x46,0x2C);
		VGAwCR(0x47,0xE8);
		VGAwCR(0x48,0x0C);
		VGAwCR(0x49,0xE7);
		VGAwCR(0x4A,0x09);
		VGAwCR(0x4B,0x4F);
		VGAwCR(0x4C,0x40);
		VGAwCR(0x4D,0x00);
		VGAwCR(0x4E,0x44);
		VGAwCR(0x4F,0x0C);
		VGAwCR(0x50,0x7A);
		VGAwCR(0x51,0x56);
		VGAwCR(0x52,0x00);
		VGAwCR(0x53,0x5D);
		VGAwCR(0x54,0x0E);
		VGAwCR(0x55,0x3B);
		VGAwCR(0x56,0x2A);
		VGAwCR(0x57,0x00);
		VGAwCR(0x58,0x2F);
		VGAwCR(0x59,0x18);
		VGAwCR(0x60,0x88);
		VGAwCR(0x61,0x63);
		VGAwCR(0x62,0x0B);
		VGAwCR(0x63,0x69);
		VGAwCR(0x64,0x1A);
		break;
	    }
	    break;
	case 1280:
#ifdef NOT_DONE
	    VGAwCR(0x40,0x?? );
            .
		.
		.
		VGAwCR(0x64,0x?? );
		break;
#else
		/* Probe should prevent this case for now */
		FatalError("1280 panel support incomplete\n");
#endif
	}
    }
}

static void 
neoRestore(ScrnInfoPtr pScrn, vgaRegPtr VgaReg, NeoRegPtr restore,
	     Bool restoreText)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    unsigned char temp;
    int i;
    Bool clock_hi = FALSE;

    vgaHWProtect(pScrn,TRUE);		/* Blank the screen */
    
    VGAwGR(0x09,0x26);
    
    /* Init the shadow registers if necessary */
    neoProgramShadowRegs(pScrn, VgaReg, restore);

    VGAwGR(0x15,0x00);

    VGAwGR(0x0A,restore->GeneralLockReg);

    /*
     * The color mode needs to be set before calling vgaHWRestore
     * to ensure the DAC is initialized properly.
     *
     * NOTE: Make sure we don't change bits make sure we don't change
     * any reserved bits.
     */
    temp = VGArGR(0x90);

    switch (nPtr->NeoChipset) {
    case NM2070 :
	temp &= 0xF0; /* Save bits 7:4 */
	temp |= (restore->ExtColorModeSelect & ~0xF0);
	break;
    case NM2090 :
    case NM2093 :
    case NM2097 :
    case NM2160 :
    case NM2200 :
    case NM2230 :
    case NM2360 :
    case NM2380 :
	temp &= 0x70; /* Save bits 6:4 */
	temp |= (restore->ExtColorModeSelect & ~0x70);
	break;
    }
    VGAwGR(0x90,temp);

    /*
     * In some rare cases a lockup might occur if we don't delay
     * here. (Reported by Miles Lane)
     */
    xf86UDelay(200000);
    /*
     * Disable horizontal and vertical graphics and text expansions so
     * that vgaHWRestore works properly.
     */
    temp = VGArGR(0x25);
    temp &= 0x39;
    VGAwGR(0x25, temp);

    /*
     * Sleep for 200ms to make sure that the two operations above have
     * had time to take effect.
     */
    xf86UDelay(200000);
    /*
     * This function handles restoring the generic VGA registers.  */
    vgaHWRestore(pScrn, VgaReg,
		 VGA_SR_MODE  
		 | (restoreText ? (VGA_SR_FONTS | VGA_SR_CMAP) : 0));

    VGAwGR(0x0E, restore->ExtCRTDispAddr);
    VGAwGR(0x0F, restore->ExtCRTOffset);
    temp = VGArGR(0x10);
    temp &= 0x0F; /* Save bits 3:0 */
    temp |= (restore->SysIfaceCntl1 & ~0x0F);
    VGAwGR(0x10, temp);

    VGAwGR(0x11, restore->SysIfaceCntl2);
    VGAwGR(0x15, restore->SingleAddrPage);
    VGAwGR(0x16, restore->DualAddrPage);

    temp = VGArGR(0x20);
    switch (nPtr->NeoChipset) {
    case NM2070 :
	temp &= 0xFC; /* Save bits 7:2 */
	temp |= (restore->PanelDispCntlReg1 & ~0xFC);
	break;
    case NM2090 :
    case NM2093 :
    case NM2097 :
    case NM2160 :
	temp &= 0xDC; /* Save bits 7:6,4:2 */
	temp |= (restore->PanelDispCntlReg1 & ~0xDC);
	break;
    case NM2200 :
    case NM2230 :
    case NM2360 :
    case NM2380 :
	temp &= 0x98; /* Save bits 7,4:3 */
	temp |= (restore->PanelDispCntlReg1 & ~0x98);
	break;
    }
    VGAwGR(0x20, temp);

    temp = VGArGR(0x25);
    temp &= 0x38; /* Save bits 5:3 */
    temp |= (restore->PanelDispCntlReg2 & ~0x38);
    VGAwGR(0x25, temp);

    if (nPtr->NeoChipset != NM2070) {
	temp = VGArGR(0x30);
	temp &= 0xEF; /* Save bits 7:5 and bits 3:0 */
	temp |= (restore->PanelDispCntlReg3 & ~0xEF);
	VGAwGR(0x30, temp);
    }

    VGAwGR(0x28, restore->PanelVertCenterReg1);
    VGAwGR(0x29, restore->PanelVertCenterReg2);
    VGAwGR(0x2a, restore->PanelVertCenterReg3);

    if (nPtr->NeoChipset != NM2070) {
	VGAwGR(0x32, restore->PanelVertCenterReg4);
	VGAwGR(0x33, restore->PanelHorizCenterReg1);
	VGAwGR(0x34, restore->PanelHorizCenterReg2);
	VGAwGR(0x35, restore->PanelHorizCenterReg3);
    }

    if (nPtr->NeoChipset == NM2160) {
	VGAwGR(0x36, restore->PanelHorizCenterReg4);
    }

    if (nPtr->NeoChipset == NM2200 || nPtr->NeoChipset == NM2230
	|| nPtr->NeoChipset == NM2360 || nPtr->NeoChipset == NM2380) {
        VGAwGR(0x36, restore->PanelHorizCenterReg4);
        VGAwGR(0x37, restore->PanelVertCenterReg5);
        VGAwGR(0x38, restore->PanelHorizCenterReg5);
    }
    if (nPtr->NeoChipset == NM2200 || nPtr->NeoChipset == NM2230
	|| nPtr->NeoChipset == NM2360 || nPtr->NeoChipset == NM2380)
	clock_hi = TRUE;
    
    /* Program VCLK3 if needed. */
    if (restore->ProgramVCLK
	 && ((VGArGR(0x9B) != restore->VCLK3NumeratorLow)
	    || (VGArGR(0x9F) !=  restore->VCLK3Denominator)
	    || (clock_hi && ((VGArGR(0x8F) & ~0x0f)
			     != (restore->VCLK3NumeratorHigh & ~0x0F))))) {
	VGAwGR(0x9B, restore->VCLK3NumeratorLow);
	if (clock_hi) {
	    temp = VGArGR(0x8F);
	    temp &= 0x0F; /* Save bits 3:0 */
	    temp |= (restore->VCLK3NumeratorHigh & ~0x0F);
	    VGAwGR(0x8F, temp);
	}
	VGAwGR(0x9F, restore->VCLK3Denominator);
    }
    if (restore->biosMode)
	VGAwCR(0x23,restore->biosMode);

    if (restore->reg) {
	VGAwCR(0x23,restore->reg->CR[0x23]);
	VGAwCR(0x25,restore->reg->CR[0x25]);
	VGAwCR(0x2F,restore->reg->CR[0x2F]);
	for (i = 0x40; i <= 0x59; i++) {
	    VGAwCR(i, restore->reg->CR[i]);
	}
	for (i = 0x60; i <= 0x69; i++) {
	    VGAwCR(i, restore->reg->CR[i]);
	}
	for (i = 0x70; i <= NEO_EXT_CR_MAX; i++) {
	    VGAwCR(i, restore->reg->CR[i]);
	}

	for (i = 0x0a; i <= 0x3f; i++) {
	    VGAwGR(i, restore->reg->GR[i]);
	}
	for (i = 0x90; i <= NEO_EXT_GR_MAX; i++) {
	    VGAwGR(i, restore->reg->GR[i]);
	}
    }

    /* Program vertical extension register */
    if (nPtr->NeoChipset == NM2200 || nPtr->NeoChipset == NM2230
	|| nPtr->NeoChipset == NM2360 || nPtr->NeoChipset == NM2380) {
	VGAwCR(0x70, restore->VerticalExt);
    }
    
    vgaHWProtect(pScrn, FALSE);		/* Turn on screen */

}
    
static Bool
neoModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);
    int hoffset, voffset;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    NeoRegPtr NeoNew = &nPtr->NeoModeReg;
    vgaRegPtr NeoStd = &hwp->ModeReg;
    Bool noLcdStretch = nPtr->noLcdStretch;
    int clockMul = 1;
    
    neoUnlock(pScrn);

    /*
     * This will allocate the datastructure and initialize all of the
     * generic VGA registers.
     */

    if (!vgaHWInit(pScrn, mode))
	return(FALSE);

    /*
     * Several registers need to be corrected from the default values
     * assigned by vgaHWinit().
     */
    pScrn->vtSema = TRUE;

    /*
     * The default value assigned by vgaHW.c is 0x41, but this does
     * not work for NeoMagic.
     */
    NeoStd->Attribute[16] = 0x01;

    switch (pScrn->depth) {
    case  8 :
	NeoStd->CRTC[0x13] = pScrn->displayWidth >> 3;
	NeoNew->ExtCRTOffset   = pScrn->displayWidth >> 11;
	NeoNew->ExtColorModeSelect = 0x11;
	break;
    case 15 :
        NeoNew->ExtColorModeSelect = 0x12;
	NeoStd->CRTC[0x13] = pScrn->displayWidth >> 2;
	NeoNew->ExtCRTOffset   = pScrn->displayWidth >> 10;
        break;
    case 16 :
        NeoNew->ExtColorModeSelect = 0x13;
	NeoStd->CRTC[0x13] = pScrn->displayWidth >> 2;
	NeoNew->ExtCRTOffset   = pScrn->displayWidth >> 10;
	break;
    case 24 :
	NeoStd->CRTC[0x13] = (pScrn->displayWidth * 3) >> 3;
	NeoNew->ExtCRTOffset   = (pScrn->displayWidth * 3) >> 11;
	NeoNew->ExtColorModeSelect = 0x14;
	break;
    default :
	break;
    }
	
    NeoNew->ExtCRTDispAddr = 0x10;

    /* Vertical Extension */
    NeoNew->VerticalExt = (((mode->CrtcVTotal -2) & 0x400) >> 10 )
      | (((mode->CrtcVDisplay -1) & 0x400) >> 9 )
        | (((mode->CrtcVSyncStart) & 0x400) >> 8 )
          | (((mode->CrtcVSyncStart) & 0x400) >> 7 );

    /* Fast write bursts on unless disabled. */
    if (nPtr->onPciBurst) {
	NeoNew->SysIfaceCntl1 = 0x30; 
    } else {
	NeoNew->SysIfaceCntl1 = 0x00; 
    }

    /* If they are used, enable linear addressing and/or enable MMIO. */
    NeoNew->SysIfaceCntl2 = 0x00;
    if (!nPtr->noLinear)
	NeoNew->SysIfaceCntl2 |= 0x80;
    if (!nPtr->noMMIO)
	NeoNew->SysIfaceCntl2 |= 0x40;

    /* Enable any user specified display devices. */
    NeoNew->PanelDispCntlReg1 = 0x00;
    if (nPtr->internDisp) {
	NeoNew->PanelDispCntlReg1 |= 0x02;
    }
    if (nPtr->externDisp) {
	NeoNew->PanelDispCntlReg1 |= 0x01;
    }

#if 0
    /*
     * This was replaced: if no devices are specified take the
     * probed settings. If the probed settings are bogus fallback
     * to internal only.
     */
    /* If the user did not specify any display devices, then... */
    if (NeoNew->PanelDispCntlReg1 == 0x00) {
	/* Default to internal (i.e., LCD) only. */
	NeoNew->PanelDispCntlReg1 |= 0x02;
    }
#endif
    /* If we are using a fixed mode, then tell the chip we are. */
    switch (mode->HDisplay) {
    case 1280:
	NeoNew->PanelDispCntlReg1 |= 0x60;
        break;
    case 1024:
	NeoNew->PanelDispCntlReg1 |= 0x40;
        break;
    case 800:
	NeoNew->PanelDispCntlReg1 |= 0x20;
        break;
    case 640:
    default:
        break;
    }

    /* Setup shadow register locking. */
    switch (NeoNew->PanelDispCntlReg1 & 0x03) {
    case 0x01 : /* External CRT only mode: */
	NeoNew->GeneralLockReg = 0x00;
	/* We need to program the VCLK for external display only mode. */
	NeoNew->ProgramVCLK = TRUE;
	break;
    case 0x02 : /* Internal LCD only mode: */
    case 0x03 : /* Simultaneous internal/external (LCD/CRT) mode: */
	NeoNew->GeneralLockReg = 0x01;
	/* Don't program the VCLK when using the LCD. */
	NeoNew->ProgramVCLK = FALSE;
	break;
    }

    /*
     * If the screen is to be stretched, turn on stretching for the
     * various modes.
     *
     * OPTION_LCD_STRETCH means stretching should be turned off!
     */
    NeoNew->PanelDispCntlReg2 = 0x00;
    NeoNew->PanelDispCntlReg3 = 0x00;
    nAcl->NoCursorMode = FALSE;

    if ((!noLcdStretch) &&
	(NeoNew->PanelDispCntlReg1 & 0x02)) {
	if (mode->HDisplay == nPtr->NeoPanelWidth) {
	    /*
	     * Don't disable the flag.  It will be needed if another mode
	     * is selected.
	     */
	    /*
	     * No stretching required when the requested display width
	     * equals the panel width.
	     */
	    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"Stretching disabled\n");
	        noLcdStretch = TRUE;
	} else {

	    switch (mode->HDisplay) {
	    case  320 : /* Needs testing.  KEM -- 24 May 98 */
	    case  400 : /* Needs testing.  KEM -- 24 May 98 */
	    case  640 :
	    case  800 :
	    case 1024 :
		NeoNew->PanelDispCntlReg2 |= 0xC6;
		nAcl->NoCursorMode = TRUE;
		break;
	    default   :
		/* No stretching in these modes. */
		xf86DrvMsg(pScrn->scrnIndex,X_INFO,
			   "Stretching disabled not supported in this mode\n");
		noLcdStretch = TRUE;
		break;
	    }
	}
    } else if (mode->Flags & V_DBLSCAN) {
	nAcl->NoCursorMode = TRUE;
    }
        
    /*
     * If the screen is to be centerd, turn on the centering for the
     * various modes.
     */
    NeoNew->PanelVertCenterReg1  = 0x00;
    NeoNew->PanelVertCenterReg2  = 0x00;
    NeoNew->PanelVertCenterReg3  = 0x00;
    NeoNew->PanelVertCenterReg4  = 0x00;
    NeoNew->PanelVertCenterReg5  = 0x00;
    NeoNew->PanelHorizCenterReg1 = 0x00;
    NeoNew->PanelHorizCenterReg2 = 0x00;
    NeoNew->PanelHorizCenterReg3 = 0x00;
    NeoNew->PanelHorizCenterReg4 = 0x00;
    NeoNew->PanelHorizCenterReg5 = 0x00;
    
    if (nPtr->lcdCenter &&
	(NeoNew->PanelDispCntlReg1 & 0x02)) {
	Bool doCenter = FALSE;
	
	hoffset = 0;
	voffset = 0;
	if (mode->HDisplay == nPtr->NeoPanelWidth) {
	    /*
	     * No centering required when the requested display width
	     * equals the panel width.
	     */
	} else {
	    NeoNew->PanelDispCntlReg3 |= 0x10;
	    if (noLcdStretch) {
		/* Calculate the horizontal offsets. */
		int HDisplay = mode->HDisplay
		    << ((mode->VDisplay < 480) ? 1 : 0);
		hoffset = ((nPtr->NeoPanelWidth - HDisplay) >> 4) - 1;
		if (mode->VDisplay < 480)
		    hoffset >>= 1;
		doCenter = TRUE;
	    } else {
		/* Stretched modes cannot be centered. */
		hoffset = 0;
	    }
	}
	if (mode->VDisplay == nPtr->NeoPanelHeight) {
	    /*
	     * No centering required when the requested display width
	     * equals the panel width.
	     */
	} else {	
	    NeoNew->PanelDispCntlReg2 |= 0x01;
	    if (noLcdStretch) {
		/* Calculate the vertical offsets. */
		int VDisplay = mode->VDisplay
		    << ((mode->Flags & V_DBLSCAN) ? 1 : 0);
		voffset = ((nPtr->NeoPanelHeight - VDisplay) >> 1) - 2;
		doCenter = TRUE;
	    } else {
		/* Stretched modes cannot be centered. */
		voffset = 0;
	    }
	}

	if (doCenter) {
	    switch (mode->HDisplay) {
	    case  320 : /* Needs testing.  KEM -- 24 May 98 */
  		NeoNew->PanelHorizCenterReg3 = hoffset;
		NeoNew->PanelVertCenterReg3  = voffset;
		break;
	    case  400 : /* Needs testing.  KEM -- 24 May 98 */
		NeoNew->PanelHorizCenterReg4 = hoffset;
		NeoNew->PanelVertCenterReg1  = voffset;
		break;
	    case  640 :
		NeoNew->PanelHorizCenterReg1 = hoffset;
		NeoNew->PanelVertCenterReg3  = voffset;
		break;
	    case  800 :
		NeoNew->PanelHorizCenterReg2 = hoffset;
		switch (mode->VDisplay) {
		case 600:
		    NeoNew->PanelVertCenterReg4  = voffset;
		    break;
		case 480:
		    /* Not sure if this is correct */
		    NeoNew->PanelVertCenterReg3  = voffset;
		    break;
		}
		break;
	    case 1024 :
		NeoNew->PanelHorizCenterReg5 = hoffset;
		NeoNew->PanelVertCenterReg5  = voffset;
		break;
	    case 1280 :
	    default   :
		/* No centering in these modes. */
		break;
	    }
	}
    }
    
#ifdef XvExtension
    if (!noLcdStretch &&
	(NeoNew->PanelDispCntlReg1 & 0x02))  {
	if (mode->HDisplay != nPtr->NeoPanelWidth)
	    nPtr->videoHZoom = (double)nPtr->NeoPanelWidth/mode->HDisplay;
	if (mode->VDisplay != nPtr->NeoPanelHeight)
	    nPtr->videoVZoom = (double)nPtr->NeoPanelHeight/mode->VDisplay;
    } else {
	nPtr->videoHZoom = 1.0;
	nPtr->videoVZoom = 1.0;
    }
#endif

    if (mode->VDisplay < 480) {
	NeoStd->Sequencer[1] |= 0x8;
	clockMul = 2;
    }
    
    NeoNew->biosMode = neoFindMode(mode->HDisplay,mode->VDisplay,pScrn->depth);
    
    /*
     * New->reg should be empty.  Just in
     * case it isn't, warn us and clear it anyway.
     */
    if (NeoNew->reg) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Non-NULL reg in NeoInit: reg=0x%08X\n", NeoNew->reg);
	xfree(NeoNew->reg);
	NeoNew->reg = NULL;
    }

    /*
     * Calculate the VCLK that most closely matches the requested dot
     * clock.
     */
    neoCalcVCLK(pScrn, mode->SynthClock*clockMul);

    /* Since we program the clocks ourselves, always use VCLK3. */
    NeoStd->MiscOutReg |= 0x0C;

    neoRestore(pScrn, NeoStd, NeoNew, FALSE);
    
    return(TRUE);
}

/*
 * neoCalcVCLK --
 *
 * Determine the closest clock frequency to the one requested.
 */
#define REF_FREQ 14.31818
#define MAX_N 127
#define MAX_D 31
#define MAX_F 1

static void
neoCalcVCLK(ScrnInfoPtr pScrn, long freq)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    
    int n, d, f;
    double f_out;
    double f_diff;
    int n_best = 0, d_best = 0, f_best = 0;
    double f_best_diff = 999999.0;
    double f_target = freq/1000.0;

    for (f = 0; f <= MAX_F; f++)
	for (n = 0; n <= MAX_N; n++)
	    for (d = 0; d <= MAX_D; d++) {
		f_out = (n+1.0)/((d+1.0)*(1<<f))*REF_FREQ;
		f_diff = abs(f_out-f_target);
		if (f_diff < f_best_diff) {
		    f_best_diff = f_diff;
		    n_best = n;
		    d_best = d;
		    f_best = f;
		}
	    }

    if (nPtr->NeoChipset == NM2200 || nPtr->NeoChipset == NM2230
	|| nPtr->NeoChipset == NM2360 || nPtr->NeoChipset == NM2380) {
        /* NOT_DONE:  We are trying the full range of the 2200 clock.
           We should be able to try n up to 2047 */
	nPtr->NeoModeReg.VCLK3NumeratorLow  = n_best;
	nPtr->NeoModeReg.VCLK3NumeratorHigh = (f_best << 7);
    }
    else {
	nPtr->NeoModeReg.VCLK3NumeratorLow  = n_best | (f_best << 7);
    }
    nPtr->NeoModeReg.VCLK3Denominator = d_best;
#ifdef DEBUG
    ErrorF("neoVCLK: f:%f NumLow=%i NumHi=%i Den=%i Df=%f\n",
	   f_target,
	   nPtr->NeoModeReg.VCLK3NumeratorLow,
	   nPtr->NeoModeReg.VCLK3NumeratorHigh,
	   nPtr->NeoModeReg.VCLK3Denominator,
	   f_best_diff);
#endif
}

/*
 * NeoDisplayPowerManagementSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 */
static void
NeoDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
			     int flags)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    unsigned char SEQ01 = 0;
    unsigned char LogicPowerMgmt = 0;
    unsigned char LCD_on = 0;

    if (!pScrn->vtSema)
	return;

    switch (PowerManagementMode) {
    case DPMSModeOn:
	/* Screen: On; HSync: On, VSync: On */
	SEQ01 = 0x00;
	LogicPowerMgmt = 0x00;
	if (nPtr->internDisp || ! nPtr->externDisp)
	    LCD_on = 0x02;
	else
	    LCD_on = 0x00;
	break;
    case DPMSModeStandby:
	/* Screen: Off; HSync: Off, VSync: On */
	SEQ01 = 0x20;
	LogicPowerMgmt = 0x10;
	LCD_on = 0x00;
	break;
    case DPMSModeSuspend:
	/* Screen: Off; HSync: On, VSync: Off */
	SEQ01 = 0x20;
	LogicPowerMgmt = 0x20;
	LCD_on = 0x00;
	break;
    case DPMSModeOff:
	/* Screen: Off; HSync: Off, VSync: Off */
	SEQ01 = 0x20;
	LogicPowerMgmt = 0x30;
	LCD_on = 0x00;
	break;
    }

    /* Turn the screen on/off */
    SEQ01 |= VGArSR(0x01) & ~0x20;
    VGAwSR(0x01, SEQ01);

    /* Turn the LCD on/off */
    LCD_on |= VGArGR(0x20) & ~0x02;
    VGAwGR(0x20, LCD_on);

    /* Set the DPMS mode */
    LogicPowerMgmt |= 0x80;
    LogicPowerMgmt |= VGArGR(0x01) & ~0xF0;
    VGAwGR(0x01,LogicPowerMgmt);
}

static unsigned int
neo_ddc1Read(ScrnInfoPtr pScrn)
{
    register vgaHWPtr hwp = VGAHWPTR(pScrn);
    register unsigned int tmp;

    /* This needs to be investigated: we may have to swap this around */
    while (!(hwp->readST01(hwp)&0x8)) {};
    while (hwp->readST01(hwp)&0x8) {};
    
    tmp = (VGArGR(0xA1) & 0x08);
    
    return (tmp);
}

static xf86MonPtr
neo_ddc1(int scrnIndex)
{
    vgaHWPtr hwp = VGAHWPTR(xf86Screens[scrnIndex]);
    unsigned int reg1, reg2, reg3;
    xf86MonPtr ret;

    /* initialize chipset */
    reg1 = VGArCR(0x21);
    reg2 = VGArCR(0x1D); 
    reg3 = VGArCR(0x1A);
    VGAwCR(0x21,0x00);
    VGAwCR(0x1D,0x01);  /* some Voodoo */ 
    VGAwGR(0xA1,0x2F);
    ret =  xf86DoEDID_DDC1(scrnIndex,vgaHWddc1SetSpeed,neo_ddc1Read);
    /* undo initialization */
    VGAwCR(0x21,reg1);
    VGAwCR(0x1D,reg2);
    return ret;
}

static Bool
neoDoDDC1(ScrnInfoPtr pScrn)
{
    Bool ret = FALSE;
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    VGAwGR(0x09,0x26);
    ret = xf86SetDDCproperties(pScrn,
				xf86PrintEDID(neo_ddc1(pScrn->scrnIndex)));
    VGAwGR(0x09,0x00);

    return ret;
}

static Bool
neoDoDDC2(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);    
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    Bool ret = FALSE;

    VGAwGR(0x09,0x26);
    if (xf86LoadSubModule(pScrn, "i2c")) {
        xf86LoaderReqSymLists(i2cSymbols, NULL);
	if (neo_I2CInit(pScrn)) {
	    ret = xf86SetDDCproperties(pScrn,xf86PrintEDID(xf86DoEDID_DDC2(
					      pScrn->scrnIndex,nPtr->I2C)));
	}
    }
    VGAwGR(0x09,0x00);

    return ret;
}

static Bool
neoDoDDCVBE(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);    
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vbeInfoPtr pVbe;
    Bool ret = FALSE;

    VGAwGR(0x09,0x26);
    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86LoaderReqSymLists(vbeSymbols, NULL);
        if ((pVbe = VBEInit(NULL,nPtr->pEnt->index))) {
	  ret = xf86SetDDCproperties(
				     pScrn,xf86PrintEDID(vbeDoEDID(pVbe,NULL)));
	  vbeFree(pVbe);
	}
    }
    VGAwGR(0x09,0x00);
    return ret;
}

static int
neoFindMode(int xres, int yres, int depth)
{
    int xres_s;
    int i, size;
    biosMode *mode;

    switch (depth) {
    case 8:
	size = sizeof(bios8) / sizeof(biosMode);
	mode = bios8;
	break;
    case 15:
	size = sizeof(bios15) / sizeof(biosMode);
	mode = bios15;
	break;
    case 16:
	size = sizeof(bios16) / sizeof(biosMode);
	mode = bios16;
	break;
    case 24:
	size = sizeof(bios24) / sizeof(biosMode);
	mode = bios24;
	break;
    default:
	return 0;
    }

    for (i = 0; i < size; i++) {
	if (xres <= mode[i].x_res) {
	    xres_s = mode[i].x_res;
	    for (; i < size; i++) {
		if (mode[i].x_res != xres_s)
		    return mode[i-1].mode;
		if (yres <= mode[i].y_res)
		    return mode[i].mode;
	    }
	}
    }
    return mode[size - 1].mode;
    
}

static void
neoProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
        if ((pVbe = VBEInit(NULL,index))) {
	    ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	    vbeFree(pVbe);
	}
    }
}
