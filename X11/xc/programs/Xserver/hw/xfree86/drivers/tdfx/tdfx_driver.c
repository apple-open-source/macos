#define USE_INT10 1
#define USE_PCIVGAIO 1

/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_driver.c,v 1.92 2002/11/25 14:05:00 eich Exp $ */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

/*
 * This server does not support these XFree 4.0 features yet
 * DDC2 (requires I2C)
 * shadowFb (if requested or acceleration is off)
 * Overlay planes
 */

/*
 * These are X and server generic header files.
 */
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86RAC.h"
#include "vbe.h"
#include "xf86cmap.h"

/* If the driver uses port I/O directly, it needs: */

#include "compiler.h"

/* Drivers using the mi implementation of backing store need: */

#include "mibstore.h"

/* All drivers using the vgahw module need this */
/* This driver needs to be modified to not use vgaHW for multihead operation */
#include "vgaHW.h"

/* Drivers using the mi SW cursor need: */

#include "mipointer.h"

/* Drivers using the mi colourmap code need: */

#include "micmap.h"

/* Required for line biases */
#include "miline.h"

#include "fb.h"

/* !!! These need to be checked !!! */
#if 0
#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"
#endif

/* The driver's own header file: */

#include "tdfx.h"

#include "miscstruct.h"
#include "dixstruct.h"

#include "xf86xv.h"
#include "Xv.h"

#ifdef XF86DRI
#include "dri.h"
#endif

/* Required Functions: */

static const OptionInfoRec *	TDFXAvailableOptions(int chipid, int busid);
/* Print a driver identifying message. */
static void TDFXIdentify(int flags);

/* Identify if there is any hardware present that I know how to drive. */
static Bool TDFXProbe(DriverPtr drv, int flags);

/* Process the config file and see if we have a valid configuration */
static Bool TDFXPreInit(ScrnInfoPtr pScrn, int flags);

/* Initialize a screen */
static Bool TDFXScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);

/* Enter from a virtual terminal */
static Bool TDFXEnterVT(int scrnIndex, int flags);

/* Leave to a virtual terminal */
static void TDFXLeaveVT(int scrnIndex, int flags);

/* Close down each screen we initialized */
static Bool TDFXCloseScreen(int scrnIndex, ScreenPtr pScreen);

/* Change screensaver state */
static Bool TDFXSaveScreen(ScreenPtr pScreen, int mode);

/* Cleanup server private data */
static void TDFXFreeScreen(int scrnIndex, int flags);

/* Check if a mode is valid on the hardware */
static int TDFXValidMode(int scrnIndex, DisplayModePtr mode, Bool
		       verbose, int flags);

static void TDFXBlockHandler(int, pointer, pointer, pointer);

/* Switch to various Display Power Management System levels */
static void TDFXDisplayPowerManagementSet(ScrnInfoPtr pScrn, 
					int PowerManagermentMode, int flags);

DriverRec TDFX = {
  TDFX_VERSION,
  TDFX_DRIVER_NAME,
  TDFXIdentify,
  TDFXProbe,
  TDFXAvailableOptions,
  NULL,
  0
};

/* Chipsets */
static SymTabRec TDFXChipsets[] = {
  { PCI_CHIP_BANSHEE, "3dfx Banshee"},
  { PCI_CHIP_VOODOO3, "3dfx Voodoo3"},
  { PCI_CHIP_VOODOO5, "3dfx Voodoo5"},
  { -1, NULL }
};

static PciChipsets TDFXPciChipsets[] = {
  { PCI_CHIP_BANSHEE, PCI_CHIP_BANSHEE, RES_SHARED_VGA },
  { PCI_CHIP_VOODOO3, PCI_CHIP_VOODOO3, RES_SHARED_VGA },
  { PCI_CHIP_VOODOO5, PCI_CHIP_VOODOO5, RES_SHARED_VGA },
  { -1, -1, RES_UNDEFINED }
};

/* !!! Do we want an option for alternate clocking? !!! */

typedef enum {
  OPTION_NOACCEL,
  OPTION_SW_CURSOR,
  OPTION_USE_PIO,
  OPTION_SHOWCACHE,
  OPTION_VIDEO_KEY,
  OPTION_NO_SLI,
  OPTION_TEXTURED_VIDEO,
  OPTION_DRI
} TDFXOpts;

static const OptionInfoRec TDFXOptions[] = {
  { OPTION_NOACCEL, "NoAccel", OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_SW_CURSOR, "SWcursor", OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_USE_PIO, "UsePIO", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_SHOWCACHE, "ShowCache", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_VIDEO_KEY, "VideoKey", OPTV_INTEGER, {0}, FALSE},
  { OPTION_NO_SLI, "NoSLI", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_TEXTURED_VIDEO, "TexturedVideo", OPTV_BOOLEAN, {1}, FALSE},
  { OPTION_DRI, "DRI", OPTV_BOOLEAN, {0}, FALSE},
  { -1, NULL, OPTV_NONE, {0}, FALSE}
};

static const char *vgahwSymbols[] = {
    "vgaHWEnable",
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
    "vgaHWSeqReset",
    "vgaHWUnlock",
    0
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86SetDDCproperties",
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
    "XAAReverseBitOrder",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

#ifdef XF86DRI
static const char *drmSymbols[] = {
    "drmAddMap",
    "drmFreeVersion",
    "drmGetVersion",
    NULL
};

static const char *driSymbols[] = {
    "DRICloseScreen",
    "DRICreateInfoRec",
    "DRIDestroyInfoRec",
    "DRIFinishScreenInit",
    "DRIGetSAREAPrivate",
    "DRILock",
    "DRIMoveBuffersHelper",
    "DRIQueryVersion",
    "DRIScreenInit",
    "DRIUnlock",
    "GlxSetVisualConfigs",
    NULL
};

#endif

#ifdef XFree86LOADER

static MODULESETUPPROTO(tdfxSetup);

static XF86ModuleVersionInfo tdfxVersRec =
{
  "tdfx",
  MODULEVENDORSTRING,
  MODINFOSTRING1,
  MODINFOSTRING2,
  XF86_VERSION_CURRENT,
  TDFX_MAJOR_VERSION, TDFX_MINOR_VERSION, TDFX_PATCHLEVEL,
  ABI_CLASS_VIDEODRV,
  ABI_VIDEODRV_VERSION,
  MOD_CLASS_VIDEODRV,
  {0,0,0,0}
};

XF86ModuleData tdfxModuleData = {&tdfxVersRec, tdfxSetup, 0};

static pointer
tdfxSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    /* This module should be loaded only once, but check to be sure. */

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&TDFX, module, 0);

	/*
	 * Modules that this driver always requires may be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * Tell the loader about symbols from other modules that this module
	 * might refer to.
	 */
	LoaderRefSymLists(vgahwSymbols, fbSymbols, xaaSymbols, 
			  ramdacSymbols, vbeSymbols, int10Symbols,
#ifdef XF86DRI
			  drmSymbols, driSymbols,
#endif
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

#endif

/*
 * TDFXGetRec and TDFXFreeRec --
 *
 * Private data for the driver is stored in the screen structure. 
 * These two functions create and destroy that private data.
 *
 */
static Bool
TDFXGetRec(ScrnInfoPtr pScrn) {
  if (pScrn->driverPrivate) return TRUE;

  pScrn->driverPrivate = xnfcalloc(sizeof(TDFXRec), 1);
  return TRUE;
}

static void
TDFXFreeRec(ScrnInfoPtr pScrn) {
  if (!pScrn) return;
  if (!pScrn->driverPrivate) return;
  xfree(pScrn->driverPrivate);
  pScrn->driverPrivate=0;
}

/*
 * TDFXIdentify --
 *
 * Returns the string name for the driver based on the chipset. In this
 * case it will always be an TDFX, so we can return a static string.
 * 
 */
static void
TDFXIdentify(int flags) {
  xf86PrintChipsets(TDFX_NAME, "Driver for 3dfx Banshee/Voodoo3 chipsets", TDFXChipsets);
}

static const OptionInfoRec *
TDFXAvailableOptions(int chipid, int busid)
{
    return TDFXOptions;
}

static void
TDFXProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;
    if (xf86LoadSubModule(pScrn, "vbe"))
    {
	pVbe =  VBEInit(NULL,index);
	ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}

/*
 * TDFXProbe --
 *
 * Look through the PCI bus to find cards that are TDFX boards.
 * Setup the dispatch table for the rest of the driver functions.
 *
 */
static Bool
TDFXProbe(DriverPtr drv, int flags) {
  int i, numUsed, numDevSections, *usedChips;
  GDevPtr *devSections;
  Bool foundScreen = FALSE;

  TDFXTRACE("TDFXProbe start\n");
  /*
   Find the config file Device sections that match this
   driver, and return if there are none.
   */
  if ((numDevSections = xf86MatchDevice(TDFX_DRIVER_NAME, &devSections))<=0) {
    return FALSE;
  }

  /* 
     Since these Probing is just checking the PCI data the server already
     collected.
  */
  if (!xf86GetPciVideoInfo()) return FALSE;

  numUsed = xf86MatchPciInstances(TDFX_NAME, PCI_VENDOR_3DFX,
				  TDFXChipsets, TDFXPciChipsets,
				  devSections, numDevSections,
				  drv, &usedChips);

  xfree(devSections);
  if (numUsed<=0) return FALSE;

  if (flags & PROBE_DETECT)
    foundScreen = TRUE;
  else for (i=0; i<numUsed; i++) {
    ScrnInfoPtr pScrn;

    /* Allocate new ScrnInfoRec and claim the slot */
    pScrn = NULL;
    if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
			   TDFXPciChipsets, NULL, NULL, NULL, NULL, NULL))) {

	pScrn->driverVersion = TDFX_VERSION;
	pScrn->driverName = TDFX_DRIVER_NAME;
	pScrn->name = TDFX_NAME;
	pScrn->Probe = TDFXProbe;
	pScrn->PreInit = TDFXPreInit;
	pScrn->ScreenInit = TDFXScreenInit;
	pScrn->SwitchMode = TDFXSwitchMode;
	pScrn->AdjustFrame = TDFXAdjustFrame;
	pScrn->EnterVT = TDFXEnterVT;
	pScrn->LeaveVT = TDFXLeaveVT;
	pScrn->FreeScreen = TDFXFreeScreen;
	pScrn->ValidMode = TDFXValidMode;
	foundScreen = TRUE;
    }
  }
  xfree(usedChips);

  return foundScreen;
}

static int
TDFXCountRam(ScrnInfoPtr pScrn) {
  TDFXPtr pTDFX;
  int vmemSize;
  int vmemType=-1; /* SDRAM or SGRAM */

  pTDFX = TDFXPTR(pScrn);
  TDFXTRACE("TDFXCountRam start\n");
  vmemSize=0;
  if (pTDFX->PIOBase[0]) {
    CARD32 
      partSize,                 /* size of SGRAM chips in Mbits */
      nChips,                   /* # chips of SDRAM/SGRAM */
      banks,			/* Number of banks of chips */
      dramInit0_strap,    
      dramInit1_strap,    
      dramInit1,
      miscInit1;

    /* determine memory type: SDRAM or SGRAM */
    vmemType = MEM_TYPE_SGRAM;
    dramInit1_strap = pTDFX->readLong(pTDFX, DRAMINIT1);
    dramInit1_strap &= SST_MCTL_TYPE_SDRAM;
    if (dramInit1_strap) vmemType = MEM_TYPE_SDRAM;

    /* set memory interface delay values and enable refresh */
    /* these apply to all RAM vendors */
    dramInit1 = 0x0;
    dramInit1 |= 2<<SST_SGRAM_OFLOP_DEL_ADJ_SHIFT;
    dramInit1 |= SST_SGRAM_CLK_NODELAY;
    dramInit1 |= SST_DRAM_REFRESH_EN;
    dramInit1 |= (0x18 << SST_DRAM_REFRESH_VALUE_SHIFT) & SST_DRAM_REFRESH_VALUE;  
    dramInit1 &= ~SST_MCTL_TYPE_SDRAM;
    dramInit1 |= dramInit1_strap;

    /* Some information about the timing register (for later debugging) */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
               "DRAMINIT1 read 0x%x, programming 0x%x (not Banshee)\n",
	       pTDFX->readLong(pTDFX, DRAMINIT1), dramInit1);

    /* 
     * Here we don't whack the timing register on the Banshee boards as the
     * BIOS has done a perfectly good job. As Banshee boards were made by
     * different manufacturers we don't touch this, but carry on whacking it 
     * for Voodoo3 and Voodoo5 boards, as we've never had a problem with them.
     * I guess this could be removed for all boards as we probably shouldn't
     * be doing this to the V3/V5 boards too.
     */
    if (pTDFX->ChipType != PCI_CHIP_BANSHEE)
    	pTDFX->writeLong(pTDFX, DRAMINIT1, dramInit1);

    /* determine memory size from strapping pins (dramInit0 and dramInit1) */
    dramInit0_strap = pTDFX->readLong(pTDFX, DRAMINIT0);

    if (pTDFX->ChipType<=PCI_CHIP_VOODOO3) { /* Banshee/V3 */
      if (vmemType == MEM_TYPE_SDRAM) {
	vmemSize = 16;
      } else {
	nChips = ((dramInit0_strap & SST_SGRAM_NUM_CHIPSETS) == 0) ? 4 : 8;
    
	if ( (dramInit0_strap & SST_SGRAM_TYPE) == SST_SGRAM_TYPE_8MBIT )  {
	  partSize = 8;
	} else if ( (dramInit0_strap & SST_SGRAM_TYPE) == SST_SGRAM_TYPE_16MBIT) {
	  partSize = 16;
	} else {
	  ErrorF("Invalid sgram type = 0x%x",
		 (dramInit0_strap & SST_SGRAM_TYPE) << SST_SGRAM_TYPE_SHIFT );
	  return 0;
	}
	vmemSize = (nChips * partSize) / 8;      /* in MBytes */
      }
    } else { /* V4, V5 */
      nChips = ((dramInit0_strap & SST_SGRAM_NUM_CHIPSETS)==0) ? 4 : 8;
      partSize=1<<((dramInit0_strap&0x38000000)>>28);
      banks=((dramInit0_strap&BIT(30))==0) ? 2 : 4;
      vmemSize=nChips*partSize*banks;
    }
    TDFXTRACEREG("dramInit0 = %x dramInit1 = %x\n", dramInit0_strap, dramInit1_strap);
    TDFXTRACEREG("MemConfig %d chips %d size %d total\n", nChips, partSize, vmemSize);

    /*
      disable block writes for SDRAM
    */
    miscInit1 = pTDFX->readLong(pTDFX, MISCINIT1);
    if ( vmemType == MEM_TYPE_SDRAM ) {
      miscInit1 |= SST_DISABLE_2D_BLOCK_WRITE;
    }
    miscInit1|=1;
    pTDFX->writeLong(pTDFX, MISCINIT1, miscInit1);
  }

  /* return # of KBytes of board memory */
  return vmemSize*1024;
}

#if 0
static int TDFXCfgToSize(int cfg)
{
  if (cfg<4) return 0x8000000<<cfg;
  return 0x4000000>>(cfg-4);
}
#endif

static int TDFXSizeToCfg(int size)
{
  switch (size) {
  case 0x40000000: return 3;
  case 0x20000000: return 2;
  case 0x10000000: return 1;
  case 0x08000000: return 0;
  case 0x04000000: return 4;
  case 0x02000000: return 5;
  case 0x01000000: return 6;
  case 0x00800000: return 7;
  case 0x00400000: return 8;
  default:
    return -1;
  }
}

static void
TDFXFindChips(ScrnInfoPtr pScrn, pciVideoPtr match)
{
  TDFXPtr pTDFX;
  pciVideoPtr *ppPci;

  pTDFX=TDFXPTR(pScrn);
  pTDFX->numChips=0;
  pTDFX->ChipType=match->chipType;
  for (ppPci = xf86GetPciVideoInfo(); *ppPci != NULL; ppPci++) {
    if ((*ppPci)->bus == match->bus && 
	(*ppPci)->device == match->device) {
      pTDFX->PciTag[pTDFX->numChips] = pciTag((*ppPci)->bus, 
					      (*ppPci)->device,
					      (*ppPci)->func);
      pTDFX->PIOBase[pTDFX->numChips] =
	pScrn->domainIOBase + ((*ppPci)->ioBase[2] & 0xFFFFFFFCU);
      pTDFX->numChips++;
    }
  }
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	"TDFXFindChips: found %d chip(s)\n", pTDFX->numChips);
  /* Disable the secondary chips for now */
  pTDFX->numChips=1;
}

static void
TDFXInitChips(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  int i, v, cfgbits, initbits;
  int mem0base, mem1base, mem0size, mem0bits, mem1size, mem1bits;

  pTDFX=TDFXPTR(pScrn);
  cfgbits=pciReadLong(pTDFX->PciTag[0], CFG_PCI_DECODE);
  mem0base=pciReadLong(pTDFX->PciTag[0], CFG_MEM0BASE);
  mem1base=pciReadLong(pTDFX->PciTag[0], CFG_MEM1BASE);
  initbits=pciReadLong(pTDFX->PciTag[0], CFG_INIT_ENABLE);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: numchips = %d\n", pTDFX->numChips);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: cfgbits = 0x%08lx, initbits = 0x%08lx\n",
		cfgbits, initbits);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: mem0base = 0x%08lx, mem1base = 0x%08lx\n",
		mem0base, mem1base);
  mem0size=32*1024*1024; /* Registers are always 32MB */
  mem1size=pScrn->videoRam*1024*2; /* Linear mapping is 2x memory */
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: mem0size = 0x%08lx, mem1size = 0x%08lx\n",
		mem0size, mem1size);
  mem0bits=TDFXSizeToCfg(mem0size);
  mem1bits=TDFXSizeToCfg(mem1size)<<4;
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: mem0bits = 0x%08lx, mem1bits = 0x%08lx\n",
		mem0bits, mem1bits);
  cfgbits=(cfgbits&~(0xFF))|mem0bits|mem1bits;
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: cfgbits = 0x%08lx\n", cfgbits);
  for (i=0; i<pTDFX->numChips; i++) {
    initbits|=BIT(10);
    pciWriteLong(pTDFX->PciTag[i], CFG_INIT_ENABLE, initbits);
    v=pciReadWord(pTDFX->PciTag[i], CFG_PCI_COMMAND);
#if 0
    if (!i)
      pciWriteWord(pTDFX->PciTag[i], CFG_PCI_COMMAND, v|0x3);
    else
      pciWriteWord(pTDFX->PciTag[i], CFG_PCI_COMMAND, v|0x2);
#endif
    pTDFX->MMIOAddr[i]=mem0base+i*mem0size;
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: MMIOAddr[%d] = 0x%08lx\n",
		i, pTDFX->MMIOAddr[i]);
    pciWriteLong(pTDFX->PciTag[i], CFG_MEM0BASE, pTDFX->MMIOAddr[i]);
    pTDFX->MMIOAddr[i]&=0xFFFFFF00;
    pTDFX->LinearAddr[i]=mem1base+i*mem1size;
    pciWriteLong(pTDFX->PciTag[i], CFG_MEM1BASE, pTDFX->LinearAddr[i]);
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"TDFXInitChips: LinearAddr[%d] = 0x%08lx\n",
		i, pTDFX->LinearAddr[i]);
    pTDFX->LinearAddr[i]&=0xFFFFFF00;
    pciWriteLong(pTDFX->PciTag[i], CFG_PCI_DECODE, cfgbits);
    initbits&=~BIT(10);
    pciWriteLong(pTDFX->PciTag[i], CFG_INIT_ENABLE, initbits);
  }
}

/*
 * TDFXPreInit --
 *
 * Do initial setup of the board before we know what resolution we will
 * be running at.
 *
 */
static Bool
TDFXPreInit(ScrnInfoPtr pScrn, int flags) 
{
  TDFXPtr pTDFX;
  ClockRangePtr clockRanges;
  int i;
  MessageType from;
  int flags24;
  rgb defaultWeight = {0, 0, 0};
  pciVideoPtr match;
  int availableMem;

  TDFXTRACE("TDFXPreInit start\n");
  if (pScrn->numEntities != 1) return FALSE;

  /* Allocate driverPrivate */
  if (!TDFXGetRec(pScrn)) {
    return FALSE;
  }

  pTDFX = TDFXPTR(pScrn);

  pTDFX->initDone=FALSE;
  pTDFX->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

  if (flags & PROBE_DETECT) {
#if !defined(__powerpc__)
    TDFXProbeDDC(pScrn, pTDFX->pEnt->index);
    return TRUE;
#else
    return FALSE;
#endif
  }

  if (pTDFX->pEnt->location.type != BUS_PCI) return FALSE;

  /* The vgahw module should be loaded here when needed */
  if (!xf86LoadSubModule(pScrn, "vgahw")) return FALSE;

  xf86LoaderReqSymLists(vgahwSymbols, NULL);

  /* Allocate a vgaHWRec */
  if (!vgaHWGetHWRec(pScrn)) return FALSE;

#if USE_INT10
#if !defined(__powerpc__)
  if (xf86LoadSubModule(pScrn, "int10")) {
    xf86Int10InfoPtr pInt;

    xf86LoaderReqSymLists(int10Symbols, NULL);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
               "Softbooting the board (through the int10 interface).\n");
    pInt = xf86InitInt10(pTDFX->pEnt->index);
    if (!pInt)
    {
      xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
                 "Softbooting the board failed.\n");
    }
    else
    {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
                 "Softbooting the board succeeded.\n");
      xf86FreeInt10(pInt);
    }
  }
#endif
#endif

  match=pTDFX->PciInfo=xf86GetPciInfoForEntity(pTDFX->pEnt->index);
  TDFXFindChips(pScrn, match);
  pTDFX->Primary = xf86IsPrimaryPci(pTDFX->PciInfo);

  if (xf86RegisterResources(pTDFX->pEnt->index, NULL, ResExclusive)) {
      TDFXFreeRec(pScrn);
      return FALSE;
  }
  /* 
   * We don't need VGA resources during OPERATING state. However I'm
   * not sure if they are disabled.
   */
  xf86SetOperatingState(resVgaIo, pTDFX->pEnt->index, ResDisableOpr);
#if 0
  pScrn->racIoFlags = RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
  pScrn->racMemFlags = RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
#endif
#if 0
#if 1

  /* Is VGA memory disabled during OPERATING state? */

  xf86SetOperatingState(resVgaMem, pTDFX->pEnt->index, ResDisableOpr);
#else
  pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
#endif
#endif

#if 1
    /* 
     * I'm sure we don't need to set these. All resources 
     * for these operations are exclusive.
     */
  if (pTDFX->usePIO) {
      pScrn->racMemFlags = RAC_FB;
      pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
  } else
      pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
#endif

  /* Set pScrn->monitor */
  pScrn->monitor = pScrn->confScreen->monitor;

  flags24=Support24bppFb | Support32bppFb | SupportConvert32to24;
  if (!xf86SetDepthBpp(pScrn, 8, 8, 8, flags24)) {
    return FALSE;
  } else {
    switch (pScrn->depth) {
    case 8:
    case 16:
    case 24:
      break;
    default:
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		 "Given depth (%d) is not supported by tdfx driver\n", 
		 pScrn->depth);
      return FALSE;
    }
  }
  xf86PrintDepthBpp(pScrn);

  pScrn->rgbBits=8;
  if (pScrn->depth>8) {
    if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight))
      return FALSE;
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

  /* We use a programmable clock */
  pScrn->progClock = TRUE;

  pTDFX->cpp = pScrn->bitsPerPixel/8;

  /* Process the options */
  xf86CollectOptions(pScrn, NULL);
  if (!(pTDFX->Options = xalloc(sizeof(TDFXOptions))))
    return FALSE;
  memcpy(pTDFX->Options, TDFXOptions, sizeof(TDFXOptions));
  xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pTDFX->Options);

  /*
   * Set the Chipset and ChipRev, allowing config file entries to
   * override.
   */
  if (pTDFX->pEnt->device->chipset && *pTDFX->pEnt->device->chipset) {
    pScrn->chipset = pTDFX->pEnt->device->chipset;
    from = X_CONFIG;
  } else if (pTDFX->pEnt->device->chipID >= 0) {
    pScrn->chipset = (char *)xf86TokenToString(TDFXChipsets, pTDFX->pEnt->device->chipID);
    from = X_CONFIG;
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
	       pTDFX->pEnt->device->chipID);
  } else {
    from = X_PROBED;
    pScrn->chipset = (char *)xf86TokenToString(TDFXChipsets, match->chipType);
  }
  if (pTDFX->pEnt->device->chipRev >= 0) {
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
	       pTDFX->pEnt->device->chipRev);
  }

  xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);

  if (pTDFX->pEnt->device->MemBase != 0) {
    pTDFX->LinearAddr[0] = pTDFX->pEnt->device->MemBase;
    from = X_CONFIG;
  } else {
    if (match->memBase[1] != 0) {
      pTDFX->LinearAddr[0] = match->memBase[1];
      from = X_PROBED;
    } else {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		 "No valid FB address in PCI config space\n");
      TDFXFreeRec(pScrn);
      return FALSE;
    }
  }
  xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	     (unsigned long)pTDFX->LinearAddr[0]);

  if (pTDFX->pEnt->device->IOBase != 0) {
    pTDFX->MMIOAddr[0] = pTDFX->pEnt->device->IOBase;
    from = X_CONFIG;
  } else {
    if (match->memBase[0]) {
      pTDFX->MMIOAddr[0] = match->memBase[0];
      from = X_PROBED;
    } else {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "No valid MMIO address in PCI config space\n");
      TDFXFreeRec(pScrn);
      return FALSE;
    }
  }
  xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at addr 0x%lX\n",
	     (unsigned long)pTDFX->MMIOAddr[0]);

  if (!match->ioBase[2]) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	       "No valid PIO address in PCI config space\n");
    TDFXFreeRec(pScrn);
    return FALSE;
  }
  xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "PIO registers at addr 0x%lX\n",
	     (unsigned long)pTDFX->PIOBase[0]);

  /* We have to use PIO to probe, because we haven't mappend yet */
  TDFXSetPIOAccess(pTDFX);

  /* Calculate memory */
  pScrn->videoRam = TDFXCountRam(pScrn);
  from = X_PROBED;
  if (pTDFX->pEnt->device->videoRam) {
    pScrn->videoRam = pTDFX->pEnt->device->videoRam;
    from = X_CONFIG;
  }

  TDFXInitChips(pScrn);

  /* Multiple by two because tiled access requires more address space */
  pTDFX->FbMapSize = pScrn->videoRam*1024*2;
  xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d kByte Mapping %d kByte\n",
	     pScrn->videoRam, pTDFX->FbMapSize/1024);

  /* Since we can do gamma correction, we call xf86SetGamma */
  {
    Gamma zeros = {0.0, 0.0, 0.0};
    
    if (!xf86SetGamma(pScrn, zeros)) {
      return FALSE;
    }
  }

  pTDFX->MaxClock = 0;
  if (pTDFX->pEnt->device->dacSpeeds[0]) {
    switch (pScrn->bitsPerPixel) {
    case 8:
      pTDFX->MaxClock = pTDFX->pEnt->device->dacSpeeds[DAC_BPP8];
      break;
    case 16:
      pTDFX->MaxClock = pTDFX->pEnt->device->dacSpeeds[DAC_BPP16];
      break;
    case 24:
      pTDFX->MaxClock = pTDFX->pEnt->device->dacSpeeds[DAC_BPP24];
      break;
    case 32:
      pTDFX->MaxClock = pTDFX->pEnt->device->dacSpeeds[DAC_BPP32];
      break;
    }
    if (!pTDFX->MaxClock)
      pTDFX->MaxClock = pTDFX->pEnt->device->dacSpeeds[0];
    from = X_CONFIG;
  } else {
    switch (pTDFX->ChipType) {
    case PCI_CHIP_BANSHEE:
      pTDFX->MaxClock = 270000;
      break;
    case PCI_CHIP_VOODOO3:
      switch(match->subsysCard) {
      case PCI_CARD_VOODOO3_2000:
	pTDFX->MaxClock = 300000;
	break;
      case PCI_CARD_VOODOO3_3000:
	pTDFX->MaxClock = 350000;
	break;
      default:
	pTDFX->MaxClock = 300000;
	break;
      }
      break;
    case PCI_CHIP_VOODOO5:
      pTDFX->MaxClock = 350000;
      break;
    }
  }
  clockRanges = xnfcalloc(sizeof(ClockRange), 1);
  clockRanges->next=NULL;
  clockRanges->minClock= 12000; /* !!! What's the min clock? !!! */
  clockRanges->maxClock=pTDFX->MaxClock;
  clockRanges->clockIndex = -1;
  clockRanges->interlaceAllowed = FALSE;
  clockRanges->doubleScanAllowed = TRUE;

  /*
   * Max memory available for the framebuffer is the total less the
   * HW cursor space and FIFO space.
   */
  availableMem = pScrn->videoRam - 4096 -
		 (((255 <= CMDFIFO_PAGES) ? 255 : CMDFIFO_PAGES) << 12);

  i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			pScrn->display->modes, clockRanges,
			0, 320, 2048, 16*pScrn->bitsPerPixel, 
			200, 2047,
			pScrn->display->virtualX, pScrn->display->virtualY,
			availableMem, LOOKUP_BEST_REFRESH);

  if (i==-1) {
    TDFXFreeRec(pScrn);
    return FALSE;
  }

  xf86PruneDriverModes(pScrn);

  if (!i || !pScrn->modes) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
    TDFXFreeRec(pScrn);
    return FALSE;
  }

  xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

  pScrn->currentMode = pScrn->modes;

  xf86PrintModes(pScrn);

  xf86SetDpi(pScrn, 0, 0);

  if (!xf86LoadSubModule(pScrn, "fb")) {
    TDFXFreeRec(pScrn);
    return FALSE;
  }
  xf86LoaderReqSymLists(fbSymbols, NULL);

  if (!xf86ReturnOptValBool(pTDFX->Options, OPTION_NOACCEL, FALSE)) {
    if (!xf86LoadSubModule(pScrn, "xaa")) {
      TDFXFreeRec(pScrn);
      return FALSE;
    }
    xf86LoaderReqSymLists(xaaSymbols, NULL);
  }

  if (!xf86GetOptValBool(pTDFX->Options, OPTION_SHOWCACHE, &(pTDFX->ShowCache))) {
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShowCache %s\n", pTDFX->ShowCache ? "Enabled" : "Disabled");
  } else {
    pTDFX->ShowCache = FALSE;
  }

  if (xf86GetOptValBool(pTDFX->Options, OPTION_TEXTURED_VIDEO, &(pTDFX->TextureXvideo))) {
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Texture Xvideo Adaptor %s\n", pTDFX->TextureXvideo ? "Enabled" : "Disabled");
  } else {
    pTDFX->TextureXvideo = FALSE;
  }

  if (xf86GetOptValInteger(pTDFX->Options, OPTION_VIDEO_KEY, &(pTDFX->videoKey))) {
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n", pTDFX->videoKey);
  } else {
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key default 0x%x\n", pTDFX->videoKey = 0x1E);
  }

  if (!xf86ReturnOptValBool(pTDFX->Options, OPTION_SW_CURSOR, FALSE)) {
    if (!xf86LoadSubModule(pScrn, "ramdac")) {
      TDFXFreeRec(pScrn);
      return FALSE;
    }
    xf86LoaderReqSymLists(ramdacSymbols, NULL);
  }

#if USE_INT10
#if !defined(__powerpc__)
  /* Load DDC if needed */
  /* This gives us DDC1 - we should be able to get DDC2B using i2c */
  if (!xf86LoadSubModule(pScrn, "ddc")) {
    TDFXFreeRec(pScrn);
    return FALSE;
  }
  xf86LoaderReqSymLists(ddcSymbols, NULL);

  /* Initialize DDC1 if possible */
  if (xf86LoadSubModule(pScrn, "vbe")) {
      xf86MonPtr pMon;
      vbeInfoPtr pVbe = VBEInit(NULL,pTDFX->pEnt->index);

      xf86LoaderReqSymLists(vbeSymbols, NULL);
      pMon = vbeDoEDID(pVbe, NULL);
      vbeFree(pVbe);
      xf86SetDDCproperties(pScrn,xf86PrintEDID(pMon));
  }
#endif
#endif

  if (xf86ReturnOptValBool(pTDFX->Options, OPTION_USE_PIO, FALSE)) {
    pTDFX->usePIO=TRUE;
  }

#if X_BYTE_ORDER == X_BIG_ENDIAN
  pTDFX->ModeReg.miscinit0 = pTDFX->readLong(pTDFX, MISCINIT0);
  pTDFX->SavedReg.miscinit0 = pTDFX->ModeReg.miscinit0;

  switch (pScrn->bitsPerPixel) {
  case 8:
    pTDFX->writeFifo = TDFXWriteFifo_8;
    pTDFX->ModeReg.miscinit0 &= ~BIT(30); /* LFB byte swizzle */
    pTDFX->ModeReg.miscinit0 &= ~BIT(31); /* LFB word swizzle */
    break;
  case 15:
  case 16:
    pTDFX->writeFifo = TDFXWriteFifo_16;
    pTDFX->ModeReg.miscinit0 |= BIT(30); /* LFB byte swizzle */
    pTDFX->ModeReg.miscinit0 |= BIT(31); /* LFB word swizzle */
    break;
  case 24:
  case 32:
    pTDFX->writeFifo = TDFXWriteFifo_24;
    pTDFX->ModeReg.miscinit0 |= BIT(30); /* LFB byte swizzle */
    pTDFX->ModeReg.miscinit0 &= ~BIT(31); /* LFB word swizzle */
    break;
  default:
    return FALSE;
    break;
  }
  pTDFX->writeLong(pTDFX, MISCINIT0, pTDFX->ModeReg.miscinit0);
#endif


  return TRUE;
}

static Bool
TDFXMapMem(ScrnInfoPtr pScrn)
{
  int mmioFlags, i;
  TDFXPtr pTDFX;

  TDFXTRACE("TDFXMapMem start\n");
  pTDFX = TDFXPTR(pScrn);

  mmioFlags = VIDMEM_MMIO | VIDMEM_READSIDEEFFECT;

  for (i=0; i<pTDFX->numChips; i++) {
    pTDFX->MMIOBase[i] = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, 
				       pTDFX->PciTag[i], 
				       pTDFX->MMIOAddr[i],
				       TDFXIOMAPSIZE);

    if (!pTDFX->MMIOBase[i]) return FALSE;
  }

  pTDFX->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				pTDFX->PciTag[0],
				pTDFX->LinearAddr[0],
				pTDFX->FbMapSize);
  if (!pTDFX->FbBase) return FALSE;

  return TRUE;
}

static Bool
TDFXUnmapMem(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  int i;

  TDFXTRACE("TDFXUnmapMem start\n");
  pTDFX = TDFXPTR(pScrn);

  for (i=0; i<pTDFX->numChips; i++) {
    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTDFX->MMIOBase[i], 
		    TDFXIOMAPSIZE);
    pTDFX->MMIOBase[i]=0;
  }

  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTDFX->FbBase, pTDFX->FbMapSize);
  pTDFX->FbBase = 0;
  return TRUE;
}

static void
PrintRegisters(ScrnInfoPtr pScrn, TDFXRegPtr regs)
{
#ifdef TRACE
  int i;
  TDFXPtr pTDFX;
  vgaHWPtr pHW = VGAHWPTR(pScrn);
  vgaRegPtr pVga = &VGAHWPTR(pScrn)->ModeReg;


  pTDFX = TDFXPTR(pScrn);
#if 1
  ErrorF("VGA Registers\n");
  ErrorF("MiscOutReg = %x versus %x\n", pHW->readMiscOut(pHW), pVga->MiscOutReg);
  ErrorF("EnableReg = %x\n", pHW->readEnable(pHW));
  for (i=0; i<25; i++) {
    int x;
    x = pHW->readCrtc(pHW, i);
    ErrorF("CRTC[%d]=%d versus %d\n", i, x, pVga->CRTC[i]);
  }
  for (i=0; i<21; i++) {
    ErrorF("Attribute[%d]=%d versus %d\n", i, pHW->readAttr(pHW, i), pVga->Attribute[i]);
  }
  for (i=0; i<9; i++) {
    ErrorF("Graphics[%d]=%d versus %d\n", i, pHW->readGr(pHW, i), pVga->Graphics[i]);
  }
  for (i=0; i<5; i++) {
    ErrorF("Sequencer[%d]=%d versus %d\n", i, pHW->readSeq(pHW, i), pVga->Sequencer[i]);
  }
#endif
#if 1
  ErrorF("Banshee Registers\n");
  ErrorF("VidCfg = %x versus %x\n",  pTDFX->readLong(pTDFX, VIDPROCCFG), regs->vidcfg);
  ErrorF("DACmode = %x versus %x\n", pTDFX->readLong(pTDFX, DACMODE), regs->dacmode);
  ErrorF("Vgainit0 = %x versus %x\n", pTDFX->readLong(pTDFX, VGAINIT0), regs->vgainit0);
  ErrorF("Vgainit1 = %x versus %x\n", pTDFX->readLong(pTDFX, VGAINIT1), regs->vgainit1);
  ErrorF("Miscinit0 = %x versus %x\n", pTDFX->readLong(pTDFX, MISCINIT0), regs->miscinit0);
  ErrorF("Miscinit1 = %x versus %x\n", pTDFX->readLong(pTDFX, MISCINIT1), regs->miscinit1);
  ErrorF("DramInit0 = %x\n", pTDFX->readLong(pTDFX, DRAMINIT0));
  ErrorF("DramInit1 = %x\n", pTDFX->readLong(pTDFX, DRAMINIT1));
  ErrorF("VidPLL = %x versus %x\n", pTDFX->readLong(pTDFX, PLLCTRL0), regs->vidpll);
  ErrorF("screensize = %x versus %x\n", pTDFX->readLong(pTDFX, VIDSCREENSIZE), regs->screensize);
  ErrorF("stride = %x versus %x\n", pTDFX->readLong(pTDFX, VIDDESKTOPOVERLAYSTRIDE), regs->stride);
  ErrorF("startaddr = %x versus %x\n", pTDFX->readLong(pTDFX, VIDDESKTOPSTARTADDR), regs->startaddr);
  ErrorF("Input Status 0 = %x\n", pTDFX->readLong(pTDFX, 0xc2));
  ErrorF("Input Status 1 = %x\n", pTDFX->readLong(pTDFX, 0xda));
  ErrorF("2D Status = %x\n", pTDFX->readLong(pTDFX, SST_2D_OFFSET));
  ErrorF("3D Status = %x\n", pTDFX->readLong(pTDFX, SST_3D_OFFSET));
#endif
#endif
}

/*
 * TDFXSave --
 *
 * This function saves the video state.  It reads all of the SVGA registers
 * into the vgaTDFXRec data structure.  There is in general no need to
 * mask out bits here - just read the registers.
 */
static void
DoSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, TDFXRegPtr tdfxReg, Bool saveFonts)
{
  TDFXPtr pTDFX;
  vgaHWPtr hwp;
  int i, dummy, count;

  TDFXTRACE("TDFXDoSave start\n");
  pTDFX = TDFXPTR(pScrn);
  hwp = VGAHWPTR(pScrn);

  /*
   * This function will handle creating the data structure and filling
   * in the generic VGA portion.
   */
  if (saveFonts && pTDFX->Primary) {
    /* Enable legacy VGA to access font memory. */
    dummy = pTDFX->readLong(pTDFX, VGAINIT0);
    pTDFX->writeLong(pTDFX, VGAINIT0, dummy & ~SST_VGA0_LEGACY_DECODE);
    vgaHWSave(pScrn, vgaReg, VGA_SR_MODE|VGA_SR_FONTS);
    pTDFX->writeLong(pTDFX, VGAINIT0, dummy);
  } else
    vgaHWSave(pScrn, vgaReg, VGA_SR_MODE);

  tdfxReg->ExtVga[0] = hwp->readCrtc(hwp, 0x1a);
  tdfxReg->ExtVga[1] = hwp->readCrtc(hwp, 0x1b);
  tdfxReg->miscinit1=pTDFX->readLong(pTDFX, MISCINIT1);
  tdfxReg->vidcfg=pTDFX->readLong(pTDFX, VIDPROCCFG);
  tdfxReg->vidpll=pTDFX->readLong(pTDFX, PLLCTRL0);
  tdfxReg->dacmode=pTDFX->readLong(pTDFX, DACMODE);
  tdfxReg->screensize=pTDFX->readLong(pTDFX, VIDSCREENSIZE);
  tdfxReg->stride=pTDFX->readLong(pTDFX, VIDDESKTOPOVERLAYSTRIDE);
  tdfxReg->cursloc=pTDFX->readLong(pTDFX, HWCURPATADDR);
  tdfxReg->startaddr=pTDFX->readLong(pTDFX, VIDDESKTOPSTARTADDR);
  tdfxReg->clip0min=TDFXReadLongMMIO(pTDFX, SST_2D_CLIP0MIN);
  tdfxReg->clip0max=TDFXReadLongMMIO(pTDFX, SST_2D_CLIP0MAX);
  tdfxReg->clip1min=TDFXReadLongMMIO(pTDFX, SST_2D_CLIP1MIN);
  tdfxReg->clip1max=TDFXReadLongMMIO(pTDFX, SST_2D_CLIP1MAX);
  tdfxReg->srcbaseaddr=TDFXReadLongMMIO(pTDFX, SST_2D_SRCBASEADDR);
  tdfxReg->dstbaseaddr=TDFXReadLongMMIO(pTDFX, SST_2D_DSTBASEADDR);
  for (i=0; i<512; i++) {
    count=0;
    do {
      TDFXWriteLongMMIO(pTDFX, DACADDR, i);
      dummy=TDFXReadLongMMIO(pTDFX, DACADDR);
    } while (count++<100 && dummy!=i);
    tdfxReg->dactable[i]=TDFXReadLongMMIO(pTDFX, DACDATA);
  }
  PrintRegisters(pScrn, tdfxReg);
}

static void
TDFXSave(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp;
  TDFXPtr pTDFX;

  TDFXTRACE("TDFXSave start\n");
  hwp = VGAHWPTR(pScrn);
  pTDFX = TDFXPTR(pScrn);
  /* Make sure VGA functionality is enabled */
  pTDFX->SavedReg.vgainit0=pTDFX->readLong(pTDFX, VGAINIT0);
#if 0
  pTDFX->SavedReg.vgainit1=pTDFX->readLong(pTDFX, VGAINIT1);
#endif
  pTDFX->writeLong(pTDFX, VGAINIT0, pTDFX->ModeReg.vgainit0);
#if 0
  pTDFX->writeLong(pTDFX, VGAINIT1, pTDFX->ModeReg.vgainit1);
#endif
  vgaHWEnable(hwp);

  DoSave(pScrn, &hwp->SavedReg, &pTDFX->SavedReg, TRUE);
}

static void
DoRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, TDFXRegPtr tdfxReg, 
	  Bool restoreFonts) {
  TDFXPtr pTDFX;
  vgaHWPtr hwp;
  int i, dummy, count;

  TDFXTRACE("TDFXDoRestore start\n");
  pTDFX = TDFXPTR(pScrn);
  hwp = VGAHWPTR(pScrn);

  pTDFX->sync(pScrn);

  vgaHWProtect(pScrn, TRUE);

  if (restoreFonts && pTDFX->Primary) {
    /* Enable legacy VGA to access font memory. */
    dummy = pTDFX->readLong(pTDFX, VGAINIT0);
    pTDFX->writeLong(pTDFX, VGAINIT0, dummy & ~SST_VGA0_LEGACY_DECODE);
    vgaHWRestore(pScrn, vgaReg, VGA_SR_FONTS|VGA_SR_MODE);
    pTDFX->writeLong(pTDFX, VGAINIT0, dummy);
  } else
    vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);

  hwp->writeCrtc(hwp, 0x1a, tdfxReg->ExtVga[0]);
  hwp->writeCrtc(hwp, 0x1b, tdfxReg->ExtVga[1]);
  pTDFX->writeLong(pTDFX, PLLCTRL0, tdfxReg->vidpll);
  pTDFX->writeLong(pTDFX, DACMODE, tdfxReg->dacmode);
  pTDFX->writeLong(pTDFX, VIDDESKTOPOVERLAYSTRIDE, tdfxReg->stride);
  pTDFX->writeLong(pTDFX, HWCURPATADDR, tdfxReg->cursloc);
  pTDFX->writeLong(pTDFX, VIDSCREENSIZE, tdfxReg->screensize);
  pTDFX->writeLong(pTDFX, VIDDESKTOPSTARTADDR, tdfxReg->startaddr);
  TDFXWriteLongMMIO(pTDFX, SST_2D_CLIP0MIN, tdfxReg->clip0min);
  TDFXWriteLongMMIO(pTDFX, SST_2D_CLIP0MAX, tdfxReg->clip0max);
  TDFXWriteLongMMIO(pTDFX, SST_2D_CLIP1MIN, tdfxReg->clip1min);
  TDFXWriteLongMMIO(pTDFX, SST_2D_CLIP1MAX, tdfxReg->clip1max);
#if X_BYTE_ORDER == X_BIG_ENDIAN
  pTDFX->writeLong(pTDFX, MISCINIT0, tdfxReg->miscinit0);
#endif
  pTDFX->writeLong(pTDFX, VIDPROCCFG, tdfxReg->vidcfg);
  TDFXWriteLongMMIO(pTDFX, SST_2D_SRCBASEADDR, tdfxReg->srcbaseaddr);
  TDFXWriteLongMMIO(pTDFX, SST_2D_DSTBASEADDR, tdfxReg->dstbaseaddr);
  for (i=0; i<512; i++) {
    count=0;
    do {
      TDFXWriteLongMMIO(pTDFX, DACADDR, i);
      dummy=TDFXReadLongMMIO(pTDFX, DACADDR);
    } while (count++<100 && dummy!=i);
    count=0;
    do {
      TDFXWriteLongMMIO(pTDFX, DACDATA, tdfxReg->dactable[i]);
      dummy=TDFXReadLongMMIO(pTDFX, DACDATA);
    } while (count++<100 && dummy!=tdfxReg->dactable[i]);
  }
  pTDFX->writeLong(pTDFX, VGAINIT0, tdfxReg->vgainit0);
#if 0
  pTDFX->writeLong(pTDFX, VGAINIT1, tdfxReg->vgainit1);
  pTDFX->writeLong(pTDFX, MISCINIT1, tdfxReg->miscinit1);
#endif
  vgaHWProtect(pScrn, FALSE);

  pTDFX->sync(pScrn);
  PrintRegisters(pScrn, tdfxReg);
}

static void
TDFXRestore(ScrnInfoPtr pScrn) {
  vgaHWPtr hwp;
  TDFXPtr pTDFX;

  TDFXTRACE("TDFXRestore start\n");
  hwp = VGAHWPTR(pScrn);
  pTDFX = TDFXPTR(pScrn);

  DoRestore(pScrn, &hwp->SavedReg, &pTDFX->SavedReg, TRUE);
}

#define REFFREQ 14318.18

static int
CalcPLL(int freq, int *f_out, int isBanshee) {
  int m, n, k, best_m, best_n, best_k, f_cur, best_error;
  int minm, maxm;

  TDFXTRACE("CalcPLL start\n");
  best_error=freq;
  best_n=best_m=best_k=0;
  if (isBanshee) {
    minm=24;
    maxm=24;
  } else {
    minm=1;
    maxm=57; /* This used to be 64, alas it seems the last 8 (funny that ?)
              * values cause jittering at lower resolutions. I've not done
              * any calculations to what the adjustment affects clock ranges,
              * but I can still run at 1600x1200@75Hz */
  }
  for (n=1; n<256; n++) {
    f_cur=REFFREQ*(n+2);
    if (f_cur<freq) {
      f_cur=f_cur/3;
      if (freq-f_cur<best_error) {
	best_error=freq-f_cur;
	best_n=n;
	best_m=1;
	best_k=0;
	continue;
      }
    }
    for (m=minm; m<maxm; m++) {
      for (k=0; k<4; k++) {
	f_cur=REFFREQ*(n+2)/(m+2)/(1<<k);
	if (abs(f_cur-freq)<best_error) {
	  best_error=abs(f_cur-freq);
	  best_n=n;
	  best_m=m;
	  best_k=k;
	}
      }
    }
  }
  n=best_n;
  m=best_m;
  k=best_k;
  *f_out=REFFREQ*(n+2)/(m+2)/(1<<k);
  return (n<<8)|(m<<2)|k;
}

static Bool
SetupVidPLL(ScrnInfoPtr pScrn, DisplayModePtr mode) {
  TDFXPtr pTDFX;
  TDFXRegPtr tdfxReg;
  int freq, f_out;

  TDFXTRACE("SetupVidPLL start\n");
  pTDFX = TDFXPTR(pScrn);
  tdfxReg = &pTDFX->ModeReg;
  freq=mode->Clock;
  tdfxReg->dacmode&=~SST_DAC_MODE_2X;
  tdfxReg->vidcfg&=~SST_VIDEO_2X_MODE_EN;
  if (freq>TDFX2XCUTOFF) {
    if (freq>pTDFX->MaxClock) {
      ErrorF("Overclocked PLLs\n");
      freq=pTDFX->MaxClock;
    }
    tdfxReg->dacmode|=SST_DAC_MODE_2X;
    tdfxReg->vidcfg|=SST_VIDEO_2X_MODE_EN;
  }
  tdfxReg->vidpll=CalcPLL(freq, &f_out, 0);
  TDFXTRACEREG("Vid PLL freq=%d f_out=%d reg=%x\n", freq, f_out, 
     tdfxReg->vidpll);
  return TRUE;
}

#if 0
static Bool
SetupMemPLL(int freq) {
  TDFXPtr pTDFX;
  vgaTDFXPtr tdfxReg;
  int f_out;

  TDFXTRACE("SetupMemPLL start\n");
  pTDFX=TDFXPTR();
  tdfxReg=(vgaTDFXPtr)vgaNewVideoState;
  tdfxReg->mempll=CalcPLL(freq, &f_out);
  pTDFX->writeLong(pTDFX, PLLCTRL1, tdfxReg->mempll);
  TDFXTRACEREG("Mem PLL freq=%d f_out=%d reg=%x\n", freq, f_out, 
     tdfxReg->mempll);
  return TRUE;
}

static Bool
SetupGfxPLL(int freq) {
  TDFXPtr pTDFX;
  vgaTDFXPtr tdfxReg;
  int f_out;

  TDFXTRACE("SetupGfxPLL start\n");
  pTDFX=TDFXPTR();
  tdfxReg=(vgaTDFXPtr)vgaNewVideoState;
  if (pTDFX->chipType==PCI_CHIP_BANSHEE)
    tdfxReg->gfxpll=CalcPLL(freq, &f_out, 1);
  else
    tdfxReg->gfxpll=CalcPLL(freq, &f_out, 0);
  pTDFX->writeLong(pTDFX, PLLCTRL2, tdfxReg->gfxpll);
  TDFXTRACEREG("Gfx PLL freq=%d f_out=%d reg=%x\n", freq, f_out, 
     tdfxReg->gfxpll);
  return TRUE;
}
#endif

#if 0
static Bool
TDFXInitWithBIOSData(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  unsigned char *bios = NULL;
  int offset1, offset2;
  int i;
  CARD32 uint[9];

  pTDFX=TDFXPTR(pScrn);

  if (pTDFX->initDone)
    return TRUE;

  if (pTDFX->Primary) {
    uint[0] = pTDFX->readLong(pTDFX, PCIINIT0);
    uint[1] = pTDFX->readLong(pTDFX, MISCINIT0);
    uint[2] = pTDFX->readLong(pTDFX, MISCINIT1);
    uint[3] = pTDFX->readLong(pTDFX, DRAMINIT0);
    uint[4] = pTDFX->readLong(pTDFX, DRAMINIT1);
    uint[5] = pTDFX->readLong(pTDFX, AGPINIT);
    uint[6] = pTDFX->readLong(pTDFX, PLLCTRL1);
    uint[7] = pTDFX->readLong(pTDFX, PLLCTRL2);
    for (i = 0; i < 8; i++) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Primary UINT32[%d] is 0x%08lx\n", i, uint[i]);
    }
    return TRUE;
  }

#define T_B_SIZE (64 * 1024)
  bios = xcalloc(T_B_SIZE, 1);
  if (!bios)
    return FALSE;

  if (!xf86ReadPciBIOS(0, pTDFX->PciTag[0], 1, bios, T_B_SIZE)) {
#if 0
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Bad BIOS read.\n");
    xfree(bios);
    return FALSE;
#endif
  }

  if (bios[0] != 0x55 || bios[1] != 0xAA) {
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Bad BIOS signature.\n");
    xfree(bios);
    return FALSE;
  }

  offset1 = bios[0x50] | (bios[0x51] << 8);
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Offset 1 is (0x%04x)\n", offset1);
  offset2 = bios[offset1] | (bios[offset1 + 1] << 8);
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Offset 2 is (0x%04x)\n", offset2);

  for (i = 0; i < 9; i++) {
    int o;

    o = offset2 + i * 4;
    uint[i] = bios[o] | (bios[o+1] << 8) | (bios[o+2] << 16) | (bios[o+3] << 24);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "UINT32[%d] is 0x%08lx\n", i, uint[i]);
  }

#if 1
  uint[0] = 0x0584fb04;
  uint[1] = 0x00000000;
  uint[2] = 0x03000001;
  uint[3] = 0x0c17a9e9;
  uint[4] = 0x00202031;
  uint[5] = 0x8000049e;
  uint[6] = 0x00003a05;
  uint[7] = 0x00000c00;
#endif

  pTDFX->writeLong(pTDFX, PCIINIT0, uint[0]);
  pTDFX->writeLong(pTDFX, MISCINIT0, uint[1]);
  pTDFX->writeLong(pTDFX, MISCINIT1, uint[2]);
  pTDFX->writeLong(pTDFX, DRAMINIT0, uint[3]);
  pTDFX->writeLong(pTDFX, DRAMINIT1, uint[4]);
  pTDFX->writeLong(pTDFX, AGPINIT, uint[5]);
  pTDFX->writeLong(pTDFX, PLLCTRL1, uint[6]);
  pTDFX->writeLong(pTDFX, PLLCTRL2, uint[7]);
#if 0
  pTDFX->writeLong(pTDFX, DRAMCOMMAND, uint[8]);
#endif

  /* reset */
  pTDFX->writeLong(pTDFX, MISCINIT0, 0xF3);
  pTDFX->writeLong(pTDFX, MISCINIT0, uint[1]);

  xfree(bios);
  return TRUE;
}
#endif


static Bool
TDFXInitVGA(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  TDFXRegPtr tdfxReg;

  TDFXTRACE("TDFXInitVGA start\n");
  pTDFX=TDFXPTR(pScrn);
  if (pTDFX->initDone) return TRUE;
  pTDFX->initDone=TRUE;

  tdfxReg = &pTDFX->ModeReg;
  tdfxReg->vgainit0 = 0;
  tdfxReg->vgainit0 |= SST_VGA0_EXTENSIONS;
  tdfxReg->vgainit0 |= SST_WAKEUP_3C3 << SST_VGA0_WAKEUP_SELECT_SHIFT;
#if USE_PCIVGAIO
  tdfxReg->vgainit0 |= SST_VGA0_LEGACY_DECODE;
#endif
  tdfxReg->vgainit0 |= SST_ENABLE_ALT_READBACK << SST_VGA0_CONFIG_READBACK_SHIFT;
  tdfxReg->vgainit0 |= SST_CLUT_SELECT_8BIT << SST_VGA0_CLUT_SELECT_SHIFT;

  tdfxReg->vgainit0 |= BIT(12);
#if 0
  tdfxReg->vgainit1 |= 0;
  tdfxReg->miscinit1 = 0;
#endif

  tdfxReg->vidcfg = SST_VIDEO_PROCESSOR_EN | SST_CURSOR_X11 | SST_DESKTOP_EN |
    (pTDFX->cpp-1)<<SST_DESKTOP_PIXEL_FORMAT_SHIFT;

  /* tdfxReg->vidcfg |= SST_DESKTOP_CLUT_BYPASS; */
   
  tdfxReg->stride = pTDFX->stride;

  tdfxReg->clip0min = tdfxReg->clip1min = 0;
  tdfxReg->clip0max = tdfxReg->clip1max = pTDFX->maxClip;

  return TRUE;
}  

static Bool
TDFXSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode) {
  TDFXPtr pTDFX;
  TDFXRegPtr tdfxReg;
  vgaRegPtr pVga;
  int hbs, hbe, vbs, vbe, hse, wd;
  int hd, hss, ht, vss, vt, vd, vse;

  TDFXTRACE("TDFXSetMode start\n");

  pTDFX = TDFXPTR(pScrn);
  tdfxReg = &pTDFX->ModeReg;
  pVga = &VGAHWPTR(pScrn)->ModeReg;

  if (pTDFX->cpp==4)
    wd = pScrn->displayWidth>>1;
  else
    wd = pScrn->displayWidth>>(4-pTDFX->cpp);

  /* Tell the board we're using a programmable clock */
  pVga->MiscOutReg |= 0xC;

  /* Calculate the CRTC values */
  hd = (mode->CrtcHDisplay>>3)-1;
  hss = (mode->CrtcHSyncStart>>3);
  hse = (mode->CrtcHSyncEnd>>3);
  ht = (mode->CrtcHTotal>>3)-5;
  hbs = (mode->CrtcHBlankStart>>3)-1;
  hbe = (mode->CrtcHBlankEnd>>3)-1;

  vd = mode->CrtcVDisplay-1;
  vss = mode->CrtcVSyncStart;
  vse = mode->CrtcVSyncEnd;
  vt = mode->CrtcVTotal-2;
  vbs = mode->CrtcVBlankStart-1;
  vbe = mode->CrtcVBlankEnd-1;

  /* Undo the "KGA fixes" */
  pVga->CRTC[3] = (hbe&0x1F)|0x80;
  pVga->CRTC[5] = (((hbe)&0x20)<<2) | (hse&0x1F);
  pVga->CRTC[22] =vbe&0xFF;

  /* Handle the higher resolution modes */
  tdfxReg->ExtVga[0] = (ht&0x100)>>8 | (hd&0x100)>>6 | (hbs&0x100)>>4 |
    (hbe&0x40)>>1 | (hss&0x100)>>2 | (hse&0x20)<<2; 

  tdfxReg->ExtVga[1] = (vt&0x400)>>10 | (vd&0x400)>>8 | (vbs&0x400)>>6 |
    (vbe&0x400)>>4;

  if (!SetupVidPLL(pScrn, mode)) return FALSE;

  /* Set the screen size */
  if (mode->Flags&V_DBLSCAN) {
    pVga->CRTC[9] |= 0x80;
    tdfxReg->screensize=mode->HDisplay|(mode->VDisplay<<13);
    tdfxReg->vidcfg |= SST_HALF_MODE;
  } else {
    tdfxReg->screensize=mode->HDisplay|(mode->VDisplay<<12);
    tdfxReg->vidcfg &= ~SST_HALF_MODE;
  }

  TDFXTRACEREG("cpp=%d Hdisplay=%d Vdisplay=%d stride=%d screensize=%x\n", 
	     pTDFX->cpp, mode->HDisplay, mode->VDisplay, tdfxReg->stride, 
	     tdfxReg->screensize);

  return TRUE;
}

static Bool
TDFXModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  vgaHWPtr hwp;
  TDFXPtr pTDFX;
  int hd, hbs, hss, hse, hbe, ht, hskew;
  Bool dbl;

  hd = hbs = hss = hse = hbe = ht = hskew = 0;
  hwp = VGAHWPTR(pScrn);
  pTDFX = TDFXPTR(pScrn);

  TDFXTRACE("TDFXModeInit start\n");
  dbl=FALSE;
  /* Check for 2x mode and halve all the timing values */
  if (mode->Clock>TDFX2XCUTOFF) {
    hd=mode->CrtcHDisplay;
    hbs=mode->CrtcHBlankStart;
    hss=mode->CrtcHSyncStart;
    hse=mode->CrtcHSyncEnd;
    hbe=mode->CrtcHBlankEnd;
    ht=mode->CrtcHTotal;
    hskew=mode->CrtcHSkew;
    mode->CrtcHDisplay=hd>>1;
    mode->CrtcHBlankStart=hbs>>1;
    mode->CrtcHSyncStart=hss>>1;
    mode->CrtcHSyncEnd=hse>>1;
    mode->CrtcHBlankEnd=hbe>>1;
    mode->CrtcHTotal=ht>>1;
    mode->CrtcHSkew=hskew>>1;
    dbl=TRUE;
  }

  vgaHWUnlock(hwp);

  if (!vgaHWInit(pScrn, mode)) return FALSE;

  pScrn->vtSema = TRUE;

  if (!TDFXSetMode(pScrn, mode)) return FALSE;

  if (dbl) {
    mode->CrtcHDisplay=hd;
    mode->CrtcHBlankStart=hbs;
    mode->CrtcHSyncStart=hss;
    mode->CrtcHSyncEnd=hse;
    mode->CrtcHBlankEnd=hbe;
    mode->CrtcHTotal=ht;
    mode->CrtcHSkew=hskew;
  }    

#ifdef XF86DRI
  if (pTDFX->directRenderingEnabled) {
    DRILock(screenInfo.screens[pScrn->scrnIndex], 0);
    TDFXSwapContextFifo(screenInfo.screens[pScrn->scrnIndex]);
  }
#endif
  DoRestore(pScrn, &hwp->ModeReg, &pTDFX->ModeReg, FALSE);
#ifdef XF86DRI
  if (pTDFX->directRenderingEnabled) {
    DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
  }
#endif

  return TRUE;
}

static void
TDFXLoadPalette16(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
		  VisualPtr pVisual) {
  TDFXPtr pTDFX;
  int i, j, index, v, repeat, max;

  TDFXTRACE("TDFXLoadPalette16 start\n");
  pTDFX = TDFXPTR(pScrn);

  for (i=0; i<numColors; i++) {
    index=indices[i];
    v=(colors[index/2].red<<16)|(colors[index].green<<8)|colors[index/2].blue;
    max=min((index+1)<<2, 256);
    for (j = index<<2; j < max; j++)
    {
      repeat=100;
      do {
	TDFXWriteLongMMIO(pTDFX, DACADDR, j);
      } while (--repeat && TDFXReadLongMMIO(pTDFX, DACADDR)!=j);
      if (!repeat) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Failed to set dac index, bypassing CLUT\n");
	pTDFX->ModeReg.vidcfg |= SST_DESKTOP_CLUT_BYPASS;
	return;
      }

      repeat=100;
      do {
	TDFXWriteLongMMIO(pTDFX, DACDATA, v);
      } while (--repeat && TDFXReadLongMMIO(pTDFX, DACDATA)!=v);
      if (!repeat) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Failed to set dac value, bypassing CLUT\n");
	pTDFX->ModeReg.vidcfg |= SST_DESKTOP_CLUT_BYPASS;
	return;
      }
    }
  }
}

static void
TDFXLoadPalette24(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
		  VisualPtr pVisual) {
  TDFXPtr pTDFX;
  int i, index, v, repeat;

  TDFXTRACE("TDFXLoadPalette24 start\n");
  pTDFX = TDFXPTR(pScrn);
  for (i=0; i<numColors; i++) {
    index=indices[i];
    v=(colors[index].red<<16)|(colors[index].green<<8)|colors[index].blue;
    repeat=100;
    do {
      TDFXWriteLongMMIO(pTDFX, DACADDR, index);
    } while (--repeat && TDFXReadLongMMIO(pTDFX, DACADDR)!=index);
    if (!repeat) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Failed to set dac index, "
		 "bypassing CLUT\n");
      pTDFX->ModeReg.vidcfg |= SST_DESKTOP_CLUT_BYPASS;
      return;
    }
    repeat=100;
    do {
      TDFXWriteLongMMIO(pTDFX, DACDATA, v);
    } while (--repeat && TDFXReadLongMMIO(pTDFX, DACDATA)!=v);
    if (!repeat) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Failed to set dac value, "
		 "bypassing CLUT\n");
      pTDFX->ModeReg.vidcfg |= SST_DESKTOP_CLUT_BYPASS;
      return;
    }
  }
}

#define TILE_WIDTH 128
#define TILE_HEIGHT 32

static int
calcBufferStride(int xres, Bool tiled, int cpp)
{
  int strideInTiles;

  if (tiled == TRUE) {
    /* Calculate tile width stuff */
    strideInTiles = (xres+TILE_WIDTH-1)/TILE_WIDTH;
    
    return strideInTiles*cpp*TILE_WIDTH;
  } else {
    return xres*cpp;
  }
} /* calcBufferStride */

static int
calcBufferHeightInTiles(int yres)
{
  int heightInTiles;            /* Height of buffer in tiles */


  /* Calculate tile height stuff */
  heightInTiles = yres >> 5;
  
  if (yres & (TILE_HEIGHT - 1))
    heightInTiles++;

  return heightInTiles;

} /* calcBufferHeightInTiles */

#if 0
static int
calcBufferSizeInTiles(int xres, int yres, int cpp) {
  int bufSizeInTiles;           /* Size of buffer in tiles */

  bufSizeInTiles =
    calcBufferHeightInTiles(yres) * (calcBufferStride(xres, TRUE, cpp) >> 7);

  return bufSizeInTiles;

} /* calcBufferSizeInTiles */
#endif

static int
calcBufferSize(int xres, int yres, Bool tiled, int cpp)
{
  int stride, height, bufSize;

  if (tiled) {
    stride = calcBufferStride(xres, tiled, cpp);
    height = TILE_HEIGHT * calcBufferHeightInTiles(yres);
  } else {
    stride = xres*cpp;
    height = yres;
  }

  bufSize = stride * height;
  
  return bufSize;

} /* calcBufferSize */

static void allocateMemory(ScrnInfoPtr pScrn) {
  TDFXPtr pTDFX;
  int memRemaining, fifoSize, screenSizeInTiles, cursorSize;
  int fbSize;
  int verb;
  char *str;

  pTDFX = TDFXPTR(pScrn);

  if (pTDFX->cpp!=3) {
    screenSizeInTiles=calcBufferSize(pScrn->virtualX, pScrn->virtualY,
				     TRUE, pTDFX->cpp);
  }
  else {
    /* cpp==3 needs to bump up to 4 */
    screenSizeInTiles=calcBufferSize(pScrn->virtualX, pScrn->virtualY,
				     TRUE, 4);
  }

  /*
   * Layout is:
   *    cursor, fifo, fb, tex, bb, db
   */

  fbSize = (pScrn->virtualY + pTDFX->pixmapCacheLinesMin) * pTDFX->stride;

  memRemaining=((pScrn->videoRam<<10) - 1) &~ 0xFFF;
  /* Note that a page is 4096 bytes, and a  */
  /* tile is 32 x 128 = 4096 bytes.  So,    */
  /* page and tile boundaries are the same  */
  /* Place the depth offset first, forcing  */
  /* it to be on an *odd* page boundary.    */
  pTDFX->depthOffset = (memRemaining - screenSizeInTiles) &~ 0xFFF;
  if ((pTDFX->depthOffset & (0x1 << 12)) == 0) {
#if	1
    if (pTDFX->depthOffset > 0) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                 "Changing depth offset from 0x%08x to 0x%08x\n",
                 pTDFX->depthOffset,
                 pTDFX->depthOffset - (0x1 << 12));
    }
#endif
      pTDFX->depthOffset -= (0x1 << 12);
  }
  /* Now, place the back buffer, forcing it */
  /* to be on an *even* page boundary.      */
  pTDFX->backOffset = (pTDFX->depthOffset - screenSizeInTiles) &~ 0xFFF;
  if (pTDFX->backOffset & (0x1 << 12)) {
#if	1
    if (pTDFX->backOffset > 0) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                 "Changing back offset from 0x%08x to 0x%08x\n",
                 pTDFX->backOffset,
                 pTDFX->backOffset - (0x1 << 12));
    }
#endif
      pTDFX->backOffset -= (0x1 << 12);
  }
  /* Give the cmd fifo at least             */
  /* CMDFIFO_PAGES pages, but no more than  */
  /* 64. NOTE: Don't go higher than 64, as  */
  /* there is suspect code in Glide3 !      */
  fifoSize = ((64 <= CMDFIFO_PAGES) ? 64 : CMDFIFO_PAGES) << 12;

  /* We give 4096 bytes to the cursor  */
  cursorSize = 4096;
  pTDFX->cursorOffset = 0;

  pTDFX->fifoOffset = pTDFX->cursorOffset + cursorSize;
  pTDFX->fifoSize = fifoSize;
  /* Now, place the front buffer, forcing   */
  /* it to be on a page boundary too, just  */
  /* for giggles.                           */
  pTDFX->fbOffset = pTDFX->fifoOffset + pTDFX->fifoSize;
  pTDFX->texOffset = pTDFX->fbOffset + fbSize;
  if (pTDFX->depthOffset <= pTDFX->texOffset ||
	pTDFX->backOffset <= pTDFX->texOffset) {
    /*
     * pTDFX->texSize < 0 means that the DRI is disabled.  pTDFX->backOffset
     * is used to calculate the maximum amount of memory available for
     * 2D offscreen use.  With DRI disabled, set this to the top of memory.
     */

    pTDFX->texSize = -1;
    pTDFX->backOffset = pScrn->videoRam * 1024;
    pTDFX->depthOffset = -1;
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	"Not enough video memory available for textures and depth buffer\n"
	"\tand/or back buffer.  Disabling DRI.  To use DRI try lower\n"
	"\tresolution modes and/or a smaller virtual screen size\n");
  } else {
    pTDFX->texSize = pTDFX->backOffset - pTDFX->texOffset;
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Textures Memory %0.02f MB\n",
		(float)pTDFX->texSize/1024.0/1024.0);
  }

/* This could be set to 2 or 3 */
#define OFFSET_VERB 1
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, OFFSET_VERB,
             "Cursor Offset: [0x%08X,0x%08X)\n",
             pTDFX->cursorOffset,
             pTDFX->cursorOffset + cursorSize);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, OFFSET_VERB,
             "Fifo Offset: [0x%08X, 0x%08X)\n",
             pTDFX->fifoOffset,
             pTDFX->fifoOffset + pTDFX->fifoSize);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, OFFSET_VERB,
             "Front Buffer Offset: [0x%08X, 0x%08X)\n",
             pTDFX->fbOffset,
             pTDFX->fbOffset +
		(pScrn->virtualY+pTDFX->pixmapCacheLinesMin)*pTDFX->stride);
  if (pTDFX->texSize > 0) {
    verb = OFFSET_VERB;
    str = "";
  } else {
    verb = 3;
    str = "(NOT USED) ";
  }
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verb,
             "%sTexture Offset: [0x%08X, 0x%08X)\n", str,
             pTDFX->texOffset,
             pTDFX->texOffset + pTDFX->texSize);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verb,
             "%sBackOffset: [0x%08X, 0x%08X)\n", str,
             pTDFX->backOffset,
             pTDFX->backOffset + screenSizeInTiles);
  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verb,
             "%sDepthOffset: [0x%08X, 0x%08X)\n", str,
             pTDFX->depthOffset,
             pTDFX->depthOffset + screenSizeInTiles);
}

static Bool
TDFXScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv) {
  ScrnInfoPtr pScrn;
  vgaHWPtr hwp;
  TDFXPtr pTDFX;
  VisualPtr visual;
  BoxRec MemBox;
#ifdef XF86DRI
  MessageType driFrom = X_DEFAULT;
#endif
  int scanlines;

  TDFXTRACE("TDFXScreenInit start\n");
  pScrn = xf86Screens[pScreen->myNum];
  pTDFX = TDFXPTR(pScrn);
  hwp = VGAHWPTR(pScrn);

  if (!TDFXMapMem(pScrn)) return FALSE;
  pScrn->memPhysBase = (int)pTDFX->LinearAddr[0];

  if (!pTDFX->usePIO) TDFXSetMMIOAccess(pTDFX);

#if USE_PCIVGAIO
  hwp->PIOOffset = pTDFX->PIOBase[0] - 0x300;
#endif
  vgaHWGetIOBase(hwp);
  /* Map VGA memory only for primary cards (to save/restore textmode data). */
  if (pTDFX->Primary) {
    if (!vgaHWMapMem(pScrn))
      return FALSE;
  }

  pTDFX->stride = pScrn->displayWidth*pTDFX->cpp;

  /* enough to do DVD */
  pTDFX->pixmapCacheLinesMin = ((720*480*pTDFX->cpp) + 
					pTDFX->stride - 1)/pTDFX->stride;

  if (pTDFX->ChipType > PCI_CHIP_VOODOO3) {
  	if ((pTDFX->pixmapCacheLinesMin + pScrn->virtualY) > 4095)
		pTDFX->pixmapCacheLinesMin = 4095 - pScrn->virtualY;
  } else {
  	if ((pTDFX->pixmapCacheLinesMin + pScrn->virtualY) > 2047)
		pTDFX->pixmapCacheLinesMin = 2047 - pScrn->virtualY;
  }

  allocateMemory(pScrn);

  pScrn->fbOffset = pTDFX->fbOffset;

#if 0
  if (pTDFX->numChips>1) {
    if (xf86ReturnOptValBool(pTDFX->Options, OPTION_NO_SLI, FALSE)) {
      TDFXSetupSLI(pScrn, FALSE, 0);
    } else {
      TDFXSetupSLI(pScrn, TRUE, 0);
    }
  }
#endif

  TDFXSetLFBConfig(pTDFX);

  /* We initialize in the state that our FIFO is up to date */
  pTDFX->syncDone=TRUE;
  if (!TDFXInitFifo(pScreen)) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to initialize private\n");
    return FALSE;
  }

  scanlines = (pTDFX->backOffset - pTDFX->fbOffset) / pTDFX->stride;
  if(pTDFX->ChipType < PCI_CHIP_VOODOO5) {
      if (scanlines > 2047) 
	scanlines = 2047;
  } else {
      /* MaxClip seems to have only 12 bits => 0->4095 */
      if (scanlines > 4095) 
	scanlines = 4095;
  }

  pTDFX->pixmapCacheLinesMax = scanlines - pScrn->virtualY;

  /*
   * Note, pTDFX->pixmapCacheLinesMax may be smaller than
   * pTDFX->pixmapCacheLinesMin when pTDFX->texSize < 0.  DRI is disabled
   * in that case, so pTDFX->pixmapCacheLinesMin isn't used when that's true.
   */
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
    "Minimum %d, Maximum %d lines of offscreen memory available\n",
	pTDFX->pixmapCacheLinesMin, pTDFX->pixmapCacheLinesMax);
     
  MemBox.y1 = 0;
  MemBox.x1 = 0;
  MemBox.x2 = pScrn->displayWidth;
  MemBox.y2 = scanlines;

  pTDFX->maxClip = MemBox.x2 | (MemBox.y2 << 16);

#if 0
  TDFXInitWithBIOSData(pScrn);
#endif
  TDFXInitVGA(pScrn);
  TDFXSave(pScrn);
  if (!TDFXModeInit(pScrn, pScrn->currentMode)) return FALSE;

  TDFXSetLFBConfig(pTDFX);

  miClearVisualTypes();

  if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
			pScrn->rgbBits, pScrn->defaultVisual))
    return FALSE;

  miSetPixmapDepths ();
    
  pTDFX->NoAccel=xf86ReturnOptValBool(pTDFX->Options, OPTION_NOACCEL, FALSE);
#ifdef XF86DRI
  /*
   * Setup DRI after visuals have been established, but before fbScreenInit
   * is called.   fbScreenInit will eventually call into the drivers
   * InitGLXVisuals call back.
   */
  if (!xf86ReturnOptValBool(pTDFX->Options, OPTION_DRI, TRUE) || pTDFX->NoAccel) {
      pTDFX->directRenderingEnabled = FALSE;
      driFrom = X_CONFIG;
  } else if (pTDFX->texSize < 0) {
      pTDFX->directRenderingEnabled = FALSE;
      driFrom = X_PROBED;
  } else {
      pTDFX->directRenderingEnabled = TDFXDRIScreenInit(pScreen);
  }
#endif

  switch (pScrn->bitsPerPixel) {
  case 8:
  case 16:
  case 24:
  case 32:
    if (!fbScreenInit(pScreen, pTDFX->FbBase+pTDFX->fbOffset, 
		       pScrn->virtualX, pScrn->virtualY,
		       pScrn->xDpi, pScrn->yDpi,
		       pScrn->displayWidth, pScrn->bitsPerPixel))
      return FALSE;
    break;
  default:
    xf86DrvMsg(scrnIndex, X_ERROR,
	       "Internal error: invalid bpp (%d) in TDFXScrnInit\n",
	       pScrn->bitsPerPixel);
    return FALSE;
  }

  if (pScrn->bitsPerPixel>8) {
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

  xf86SetBlackWhitePixels(pScreen);

  TDFXDGAInit(pScreen);

  xf86InitFBManager(pScreen, &MemBox);

  if (!pTDFX->NoAccel) {
    if (!TDFXAccelInit(pScreen)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Hardware acceleration initialization failed\n");
    }
  }

  miInitializeBackingStore(pScreen);
  xf86SetBackingStore(pScreen);
  xf86SetSilkenMouse(pScreen);

  miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

  if (!xf86ReturnOptValBool(pTDFX->Options, OPTION_SW_CURSOR, FALSE)) {
    if (!TDFXCursorInit(pScreen)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Hardware cursor initialization failed\n");
    }
  }

  if (!miCreateDefColormap(pScreen)) return FALSE;

  if (pScrn->bitsPerPixel==16) {
    if (!xf86HandleColormaps(pScreen, 256, 8, TDFXLoadPalette16, 0,
			     CMAP_PALETTED_TRUECOLOR|CMAP_RELOAD_ON_MODE_SWITCH))
      return FALSE;
  } else {
    if (!xf86HandleColormaps(pScreen, 256, 8, TDFXLoadPalette24, 0,
			     CMAP_PALETTED_TRUECOLOR|CMAP_RELOAD_ON_MODE_SWITCH))
      return FALSE;
  }

  TDFXAdjustFrame(scrnIndex, 0, 0, 0);

  xf86DPMSInit(pScreen, TDFXDisplayPowerManagementSet, 0);

#ifdef XF86DRI
  if (pTDFX->directRenderingEnabled) {
	/* Now that mi, fb, drm and others have done their thing, 
         * complete the DRI setup.
         */
	pTDFX->directRenderingEnabled = TDFXDRIFinishScreenInit(pScreen);
  }
  if (pTDFX->directRenderingEnabled) {
	xf86DrvMsg(pScrn->scrnIndex, driFrom, "Direct rendering enabled\n");
  } else {
	xf86DrvMsg(pScrn->scrnIndex, driFrom, "Direct rendering disabled\n");
  }
#endif


#ifdef XvExtension
  /* Initialize Xv support */
  TDFXInitVideo (pScreen);
#endif

  pScreen->SaveScreen = TDFXSaveScreen;
  pTDFX->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = TDFXCloseScreen;

  pTDFX->BlockHandler = pScreen->BlockHandler;
  pScreen->BlockHandler = TDFXBlockHandler;

  if (serverGeneration == 1)
    xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

  return TRUE;
}

Bool
TDFXSwitchMode(int scrnIndex, DisplayModePtr mode, int flags) {
  ScrnInfoPtr pScrn;

  TDFXTRACE("TDFXSwitchMode start\n");
  pScrn=xf86Screens[scrnIndex];
  return TDFXModeInit(pScrn, mode);
}

void
TDFXAdjustFrame(int scrnIndex, int x, int y, int flags) {
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;
  TDFXRegPtr tdfxReg;

  TDFXTRACE("TDFXAdjustFrame start\n");
  pScrn = xf86Screens[scrnIndex];
  pTDFX = TDFXPTR(pScrn);

  if (pTDFX->ShowCache && y && pScrn->vtSema)
    y += pScrn->virtualY - 1;

  tdfxReg = &pTDFX->ModeReg;
  if(pTDFX->ShowCache && y && pScrn->vtSema) 
     y += pScrn->virtualY - 1;
  tdfxReg->startaddr = pTDFX->fbOffset+y*pTDFX->stride+(x*pTDFX->cpp);
  TDFXTRACE("TDFXAdjustFrame to x=%d y=%d offset=%d\n", x, y, tdfxReg->startaddr);
  pTDFX->writeLong(pTDFX, VIDDESKTOPSTARTADDR, tdfxReg->startaddr);
}

static Bool
TDFXEnterVT(int scrnIndex, int flags) {
  ScrnInfoPtr pScrn;
  ScreenPtr pScreen;
#ifdef XF86DRI
  TDFXPtr pTDFX;
#endif

  TDFXTRACE("TDFXEnterVT start\n");
  pScrn = xf86Screens[scrnIndex];
  pScreen = screenInfo.screens[scrnIndex];
  TDFXInitFifo(pScreen);
#ifdef XF86DRI
  pTDFX = TDFXPTR(pScrn);
  if (pTDFX->directRenderingEnabled) {
    DRIUnlock(pScreen);
  }
#endif
  if (!TDFXModeInit(pScrn, pScrn->currentMode)) return FALSE;
  TDFXAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
  return TRUE;
}

static void
TDFXLeaveVT(int scrnIndex, int flags) {
  ScrnInfoPtr pScrn;
  vgaHWPtr hwp;
  ScreenPtr pScreen;
  TDFXPtr pTDFX;

  TDFXTRACE("TDFXLeaveVT start\n");
  pScrn = xf86Screens[scrnIndex];
  hwp=VGAHWPTR(pScrn);
  TDFXRestore(pScrn);
  vgaHWLock(hwp);
  pScreen = screenInfo.screens[scrnIndex];
  pTDFX = TDFXPTR(pScrn);
  pTDFX->sync(pScrn);
  TDFXShutdownFifo(pScreen);
#ifdef XF86DRI
  if (pTDFX->directRenderingEnabled) {
    DRILock(pScreen, 0);
  }
#endif
}

static Bool
TDFXCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  vgaHWPtr hwp;
  TDFXPtr pTDFX;

  TDFXTRACE("TDFXCloseScreen start\n");
  pScrn = xf86Screens[scrnIndex];
  hwp = VGAHWPTR(pScrn);
  pTDFX = TDFXPTR(pScrn);

#ifdef XF86DRI
    if (pTDFX->directRenderingEnabled) {
	TDFXDRICloseScreen(pScreen);
	pTDFX->directRenderingEnabled=FALSE;
    }
#endif

  TDFXShutdownFifo(pScreen);

  if (pScrn->vtSema) {
      TDFXRestore(pScrn);
      vgaHWLock(hwp);
      TDFXUnmapMem(pScrn);
      vgaHWUnmapMem(pScrn);
  }
  
  if (pTDFX->AccelInfoRec) XAADestroyInfoRec(pTDFX->AccelInfoRec);
  pTDFX->AccelInfoRec=0;
  if (pTDFX->DGAModes) xfree(pTDFX->DGAModes);
  pTDFX->DGAModes=0;
  if (pTDFX->scanlineColorExpandBuffers[0])
    xfree(pTDFX->scanlineColorExpandBuffers[0]);
  pTDFX->scanlineColorExpandBuffers[0]=0;
  if (pTDFX->scanlineColorExpandBuffers[1])
    xfree(pTDFX->scanlineColorExpandBuffers[1]);
  pTDFX->scanlineColorExpandBuffers[1]=0;
  if (pTDFX->overlayAdaptor)
    xfree(pTDFX->overlayAdaptor);
  pTDFX->overlayAdaptor=0;
  if (pTDFX->textureAdaptor)
    xfree(pTDFX->textureAdaptor);
  pTDFX->textureAdaptor=0;

  pScrn->vtSema=FALSE;

  pScreen->BlockHandler = pTDFX->BlockHandler;
  pScreen->CloseScreen = pTDFX->CloseScreen;
  return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

static void
TDFXFreeScreen(int scrnIndex, int flags) {
  TDFXTRACE("TDFXFreeScreen start\n");
  TDFXFreeRec(xf86Screens[scrnIndex]);
  if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
    vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

static int
TDFXValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags) {
  TDFXTRACE("TDFXValidMode start\n");
  if ((mode->HDisplay>2048) || (mode->VDisplay>1536)) 
    return MODE_BAD;
  /* Banshee doesn't support interlace. Does V3? */
  if (mode->Flags&V_INTERLACE) 
    return MODE_BAD;
  /* In clock doubled mode widths must be divisible by 16 instead of 8 */
  if ((mode->Clock>TDFX2XCUTOFF) && (mode->HDisplay%16))
    return MODE_BAD;
  return MODE_OK;
}

/* replacement of vgaHWBlankScreen(pScrn, unblank) which doesn't unblank
 * the screen if it is already unblanked. */
static void
TDFXBlankScreen(ScrnInfoPtr pScrn, Bool unblank)
{
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  unsigned char scrn;

  TDFXTRACE("TDFXBlankScreen start\n");

  scrn = hwp->readSeq(hwp, 0x01);

  if (unblank) {
    if((scrn & 0x20) == 0) return;
    scrn &= ~0x20;                    /* enable screen */
  } else {
    scrn |= 0x20;                     /* blank screen */
  }

  vgaHWSeqReset(hwp, TRUE);
  hwp->writeSeq(hwp, 0x01, scrn);     /* change mode */
  vgaHWSeqReset(hwp, FALSE);
}

static Bool
TDFXSaveScreen(ScreenPtr pScreen, int mode)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  Bool unblank;

  TDFXTRACE("TDFXSaveScreen start\n");

  unblank = xf86IsUnblank(mode);

  if (unblank)
    SetTimeSinceLastInputEvent();

  if (pScrn->vtSema) {
    TDFXBlankScreen(pScrn, unblank);
  }
  return TRUE;
}                                                                             

static void
TDFXBlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
    ScreenPtr   pScreen = screenInfo.screens[i];
    ScrnInfoPtr pScrn   = xf86Screens[i];
    TDFXPtr     pTDFX   = TDFXPTR(pScrn);

    pScreen->BlockHandler = pTDFX->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = TDFXBlockHandler;

    if(pTDFX->VideoTimerCallback) {
        (*pTDFX->VideoTimerCallback)(pScrn, currentTime.milliseconds);
    }
}

static void
TDFXDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, 
			      int flags) {
  TDFXPtr pTDFX;
  int dacmode, state=0;

  TDFXTRACE("TDFXDPMS start\n");
  pTDFX = TDFXPTR(pScrn);
  dacmode=pTDFX->readLong(pTDFX, DACMODE);
  switch (PowerManagementMode) {
  case DPMSModeOn:
    /* Screen: On; HSync: On, VSync: On */
    state=0;
    break;
  case DPMSModeStandby:
    /* Screen: Off; HSync: Off, VSync: On */
    state=BIT(3);
    break;
  case DPMSModeSuspend:
    /* Screen: Off; HSync: On, VSync: Off */
    state=BIT(1);
    break;
  case DPMSModeOff:
    /* Screen: Off; HSync: Off, VSync: Off */
    state=BIT(1)|BIT(3);
    break;
  }
  dacmode&=~(BIT(1)|BIT(3));
  dacmode|=state;
  pTDFX->writeLong(pTDFX, DACMODE, dacmode);
}
