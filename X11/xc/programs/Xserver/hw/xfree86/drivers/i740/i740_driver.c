
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740_driver.c,v 1.50 2003/11/03 05:11:16 tsi Exp $ */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

/*
 * This server does not support these XFree86 4.0 features yet
 * DDC1 & DDC2 (requires I2C)
 * shadowFb (if requested or acceleration is off)
 * Overlay planes
 * DGA
 */

#ifndef USE_DDC2
#define USE_DDC2 0
#endif

/*
 * These are X and server generic header files.
 */
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86RAC.h"
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

#define USE_FB

#ifdef USE_FB
#include "fb.h"
#else
/* Drivers using cfb need: */

#define PSZ 8
#include "cfb.h"
#undef PSZ

/* Drivers supporting bpp 16, 24 or 32 with cfb need one or more of: */

#include "cfb16.h"
#include "cfb24.h"
#include "cfb32.h"
#endif

/* The driver's own header file: */


#include "regionstr.h"

#include "xf86xv.h"
#include "Xv.h"

#include "vbe.h"
#include "i740_dga.h"
#include "i740.h"


/* Required Functions: */
static const OptionInfoRec * I740AvailableOptions(int chipid, int busid);

/* Print a driver identifying message. */
static void I740Identify(int flags);

/* Identify if there is any hardware present that I know how to drive. */
static Bool I740Probe(DriverPtr drv, int flags);

/* Process the config file and see if we have a valid configuration */
static Bool I740PreInit(ScrnInfoPtr pScrn, int flags);

/* Initialize a screen */
static Bool I740ScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);

/* Enter from a virtual terminal */
static Bool I740EnterVT(int scrnIndex, int flags);

/* Leave to a virtual terminal */
static void I740LeaveVT(int scrnIndex, int flags);

/* Close down each screen we initialized */
static Bool I740CloseScreen(int scrnIndex, ScreenPtr pScreen);

/* Change screensaver state */
static Bool I740SaveScreen(ScreenPtr pScreen, int mode);

/* Cleanup server private data */
static void I740FreeScreen(int scrnIndex, int flags);

/* Check if a mode is valid on the hardware */
static ModeStatus I740ValidMode(int scrnIndex, DisplayModePtr mode,
				Bool verbose, int flags);

/* Switch to various Display Power Management System levels */
static void I740DisplayPowerManagementSet(ScrnInfoPtr pScrn, 
					int PowerManagermentMode, int flags);

static void I740ProbeDDC(ScrnInfoPtr pScrn, int index);

static Bool I740MapMem(ScrnInfoPtr pScrn);
static Bool I740UnmapMem(ScrnInfoPtr pScrn);


#define VERSION 4000
#define I740_NAME "I740"
#define I740_DRIVER_NAME "i740"
#define I740_MAJOR_VERSION 1
#define I740_MINOR_VERSION 0
#define I740_PATCHLEVEL 0

DriverRec I740 = {
  VERSION,
  I740_DRIVER_NAME,
  I740Identify,
  I740Probe,
  I740AvailableOptions,
  NULL,
  0
};

/* Chipsets */
static SymTabRec I740Chipsets[] = {
  { PCI_CHIP_I740_AGP, "i740 (agp)"},
  { PCI_CHIP_I740_PCI, "i740 (pci)"},
  { -1, NULL }
};

static PciChipsets I740PciChipsets[] = {
  { PCI_CHIP_I740_AGP, PCI_CHIP_I740_AGP, RES_SHARED_VGA },
  { PCI_CHIP_I740_PCI, PCI_CHIP_I740_PCI, RES_SHARED_VGA },
  { -1, -1, RES_UNDEFINED }
};

typedef enum {
  OPTION_NOACCEL,
  OPTION_SW_CURSOR,
  OPTION_SDRAM,
  OPTION_SGRAM,
  OPTION_SLOW_RAM,
  OPTION_DAC_6BIT,
  OPTION_USE_PIO,
  OPTION_VGACOMPAT
} I740Opts;

static const OptionInfoRec I740Options[] = {
  { OPTION_NOACCEL, "NoAccel", OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_SW_CURSOR, "SWcursor", OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_SDRAM, "SDRAM", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_SGRAM, "SGRAM", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_SLOW_RAM, "SlowRam", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_DAC_6BIT, "Dac6Bit", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_USE_PIO, "UsePIO", OPTV_BOOLEAN, {0}, FALSE},
  { OPTION_VGACOMPAT, "VGACompat", OPTV_BOOLEAN, {0}, FALSE},
  { -1, NULL, OPTV_NONE, {0}, FALSE}
};

static const char *vgahwSymbols[] = {
    "vgaHWGetHWRec",
    "vgaHWSave", /* Added */
    "vgaHWRestore", /* Added */
    "vgaHWProtect",
    "vgaHWInit",
    "vgaHWMapMem",
    "vgaHWSetMmioFuncs",
    "vgaHWGetIOBase",
    "vgaHWLock",
    "vgaHWUnlock",
    "vgaHWFreeHWRec",
    "vgaHWSaveScreen",
    "vgaHWHandleColormaps",
    0
};

#ifdef XFree86LOADER
static const char *fbSymbols[] = {
#ifdef USE_FB
    "fbScreenInit",
    "fbPictureInit",
#else
    "cfbScreenInit",
    "cfb16ScreenInit",
    "cfb24ScreenInit",
    "cfb32ScreenInit",
#endif
    "cfb8_32ScreenInit",
    "cfb24_32ScreenInit",
    NULL
};
#endif

static const char *xaaSymbols[] = {
    "XAADestroyInfoRec",
    "XAACreateInfoRec",
    "XAAInit",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86InitCursor",
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
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

#if USE_DDC2
static const char *ddcSymbols[] = {
  "xf86PrintEDID",
  "xf86DoEDID_DDC1",
  "xf86DoEDID_DDC2",
    NULL
};

static const char *i2cSymbols[] = {
  "xf86CreateI2CBusRec",
  "xf86I2CBusInit",
    NULL
};
#endif


#ifdef XFree86LOADER

static MODULESETUPPROTO(i740Setup);

static XF86ModuleVersionInfo i740VersRec =
{
  "i740",
  MODULEVENDORSTRING,
  MODINFOSTRING1,
  MODINFOSTRING2,
  XF86_VERSION_CURRENT,
  I740_MAJOR_VERSION, I740_MINOR_VERSION, I740_PATCHLEVEL,
  ABI_CLASS_VIDEODRV,
  ABI_VIDEODRV_VERSION,
  MOD_CLASS_VIDEODRV,
  {0,0,0,0}
};

XF86ModuleData i740ModuleData = {&i740VersRec, i740Setup, 0};

static pointer
i740Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    /* This module should be loaded only once, but check to be sure. */

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&I740, module, 0);

	/*
	 * Modules that this driver always requires may be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * Tell the loader about symbols from other modules that this module
	 * might refer to.
	 */
	LoaderRefSymLists(vgahwSymbols, fbSymbols, xaaSymbols, 
			  ramdacSymbols, vbeSymbols,
#if USE_DDC2
			  ddcSymbols, i2cSymbols,
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
 * I740GetRec and I740FreeRec --
 *
 * Private data for the driver is stored in the screen structure. 
 * These two functions create and destroy that private data.
 *
 */
static Bool
I740GetRec(ScrnInfoPtr pScrn) {
  if (pScrn->driverPrivate) return TRUE;

  pScrn->driverPrivate = xnfcalloc(sizeof(I740Rec), 1);
  return TRUE;
}

static void
I740FreeRec(ScrnInfoPtr pScrn) {
  if (!pScrn) return;
  if (!pScrn->driverPrivate) return;
  xfree(pScrn->driverPrivate);
  pScrn->driverPrivate=0;
}

static const OptionInfoRec *
I740AvailableOptions(int chipid, int busid) 
{
    return I740Options;
}

/*
 * I740Identify --
 *
 * Returns the string name for the driver based on the chipset. In this
 * case it will always be an I740, so we can return a static string.
 * 
 */
static void
I740Identify(int flags) {
  xf86PrintChipsets(I740_NAME, "Driver for Intel i740 chipset", I740Chipsets);
}

/*
 * I740Probe --
 *
 * Look through the PCI bus to find cards that are I740 boards.
 * Setup the dispatch table for the rest of the driver functions.
 *
 */
static Bool
I740Probe(DriverPtr drv, int flags) {
  int i, numUsed, numDevSections, *usedChips;
  GDevPtr *devSections;
  Bool foundScreen = FALSE;

  /*
   Find the config file Device sections that match this
   driver, and return if there are none.
   */
  if ((numDevSections = xf86MatchDevice(I740_DRIVER_NAME, &devSections))<=0) {
    return FALSE;
  }

  /* 
     Since these Probing is just checking the PCI data the server already
     collected.
  */
  if (!xf86GetPciVideoInfo()) return FALSE;
 
  /* Look for Intel based chips */
  numUsed = xf86MatchPciInstances(I740_NAME, PCI_VENDOR_INTEL,
				  I740Chipsets, I740PciChipsets,
				  devSections, numDevSections,
				  drv, &usedChips);

  if (numUsed > 0) {
      if (flags & PROBE_DETECT)
	  foundScreen = TRUE;
      else for (i=0; i<numUsed; i++) {
	  ScrnInfoPtr pScrn = NULL;
	  /* Allocate new ScrnInfoRec and claim the slot */
	  if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
						 I740PciChipsets, 0, 0, 0, 0, 0))) {
	      pScrn->driverVersion = VERSION;
	      pScrn->driverName = I740_DRIVER_NAME;
	      pScrn->name = I740_NAME;
	      pScrn->Probe = I740Probe;
	      pScrn->PreInit = I740PreInit;
	      pScrn->ScreenInit = I740ScreenInit;
	      pScrn->SwitchMode = I740SwitchMode;
	      pScrn->AdjustFrame = I740AdjustFrame;
	      pScrn->EnterVT = I740EnterVT;
	      pScrn->LeaveVT = I740LeaveVT;
	      pScrn->FreeScreen = I740FreeScreen;
	      pScrn->ValidMode = I740ValidMode;
	      foundScreen = TRUE;
	      
	  }
      }
  }
  

  /* Look for Real3D based chips */
  numUsed = xf86MatchPciInstances(I740_NAME, PCI_VENDOR_REAL3D,
				  I740Chipsets, I740PciChipsets,
				  devSections, numDevSections,
				  drv, &usedChips);

  if (numUsed > 0) {
      if (flags & PROBE_DETECT)
	  foundScreen = TRUE;
      else for (i=0; i<numUsed; i++) {
	  ScrnInfoPtr pScrn = NULL;
	  if ((pScrn  = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
						  I740PciChipsets, 0, 0, 0, 0, 0))) {
	      pScrn->driverVersion = VERSION;
	      pScrn->driverName = I740_DRIVER_NAME;
	      pScrn->name = I740_NAME;
	      pScrn->Probe = I740Probe;
	      pScrn->PreInit = I740PreInit;
	      pScrn->ScreenInit = I740ScreenInit;
	      pScrn->SwitchMode = I740SwitchMode;
	      pScrn->AdjustFrame = I740AdjustFrame;
	      pScrn->EnterVT = I740EnterVT;
	      pScrn->LeaveVT = I740LeaveVT;
	      pScrn->FreeScreen = I740FreeScreen;
	      pScrn->ValidMode = I740ValidMode;
	      foundScreen = TRUE;
	  }
      }
  }
  
  xfree(devSections);
  xfree(usedChips);
  
  return foundScreen;
}

static void
I740ProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;
    if (xf86LoadSubModule(pScrn, "vbe")) {
	pVbe = VBEInit(NULL,index);
	ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}

/*
 * I740PreInit --
 *
 * Do initial setup of the board before we know what resolution we will
 * be running at.
 *
 */
static Bool
I740PreInit(ScrnInfoPtr pScrn, int flags) {
  I740Ptr pI740;
  ClockRangePtr clockRanges;
  int i;
  MessageType from;
  int temp;
#ifndef USE_FB
  char *mod=0, *reqSym=0;
#endif
  int flags24;
  rgb defaultWeight = {0, 0, 0};

  if (pScrn->numEntities != 1) return FALSE;

  /* Allocate driverPrivate */
  if (!I740GetRec(pScrn)) {
    return FALSE;
  }

  pI740 = I740PTR(pScrn);

  pI740->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
  if (pI740->pEnt->location.type != BUS_PCI) return FALSE;

  /*I740ProbeDDC(pScrn, pI740->pEnt->index);*/
  if (flags & PROBE_DETECT) {
	I740ProbeDDC(pScrn, pI740->pEnt->index);
	return TRUE;
  }

  /* The vgahw module should be loaded here when needed */
  if (!xf86LoadSubModule(pScrn, "vgahw")) return FALSE;

  xf86LoaderReqSymLists(vgahwSymbols, NULL);

  /* Allocate a vgaHWRec */
  if (!vgaHWGetHWRec(pScrn)) return FALSE;

  pI740->PciInfo = xf86GetPciInfoForEntity(pI740->pEnt->index);
  pI740->PciTag = pciTag(pI740->PciInfo->bus, pI740->PciInfo->device,
			 pI740->PciInfo->func);

  if (xf86RegisterResources(pI740->pEnt->index, 0, ResNone))
      return FALSE;
  if (pI740->usePIO)
    pScrn->racIoFlags = RAC_FB | RAC_COLORMAP;
  else
    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP;

  /* Set pScrn->monitor */
  pScrn->monitor = pScrn->confScreen->monitor;


  flags24=Support24bppFb | Support32bppFb | SupportConvert32to24;
  if (!xf86SetDepthBpp(pScrn, 0, 0, 0, flags24)) {
    return FALSE;
  } else {
    switch (pScrn->depth) {
    case 8:
    case 15:
    case 16:
    case 24:
      break;
    default:
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		 "Given depth (%d) is not supported by i740 driver\n", 
		 pScrn->depth);
      return FALSE;
    }
  }
  /*xf86PrintDepthBpp(pScrn);*/

  if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight))
    return FALSE;

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

  /* We use a programamble clock */
  pScrn->progClock = TRUE;

  pI740->cpp = pScrn->bitsPerPixel/8;

  /* Process the options */
  xf86CollectOptions(pScrn, NULL);
  if (!(pI740->Options = xalloc(sizeof(I740Options))))
    return FALSE;
  memcpy(pI740->Options, I740Options, sizeof(I740Options));
  xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pI740->Options);

  /* 6-BIT dac isn't reasonable for modes with > 8bpp */
  if (xf86ReturnOptValBool(pI740->Options, OPTION_DAC_6BIT, FALSE) &&
      pScrn->bitsPerPixel>8) {
    OptionInfoPtr ptr;
    ptr=xf86TokenToOptinfo(pI740->Options, OPTION_DAC_6BIT);
    ptr->found=FALSE;
  }

  if (xf86ReturnOptValBool(pI740->Options, OPTION_DAC_6BIT, FALSE))
    pScrn->rgbBits=8;
  else
    pScrn->rgbBits=6;

  /* We have to use PIO to probe, because we haven't mappend yet */
  I740SetPIOAccess(pI740);

  /*
   * Set the Chipset and ChipRev, allowing config file entries to
   * override.
   */
  if (pI740->pEnt->device->chipset && *pI740->pEnt->device->chipset) {
    pScrn->chipset = pI740->pEnt->device->chipset;
    from = X_CONFIG;
  } else if (pI740->pEnt->device->chipID >= 0) {
    pScrn->chipset = (char *)xf86TokenToString(I740Chipsets, pI740->pEnt->device->chipID);
    from = X_CONFIG;
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
	       pI740->pEnt->device->chipID);
  } else {
    from = X_PROBED;
    pScrn->chipset = (char *)xf86TokenToString(I740Chipsets, pI740->PciInfo->chipType);
  }
  if (pI740->pEnt->device->chipRev >= 0) {
    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
	       pI740->pEnt->device->chipRev);
  }

  xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", (pScrn->chipset!=NULL)?pScrn->chipset:"Unknown i740");

  if (pI740->pEnt->device->MemBase != 0) {
    pI740->LinearAddr = pI740->pEnt->device->MemBase;
    from = X_CONFIG;
  } else {
    if (pI740->PciInfo->memBase[1] != 0) {
      pI740->LinearAddr = pI740->PciInfo->memBase[0]&0xFF000000;
      from = X_PROBED;
    } else {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		 "No valid FB address in PCI config space\n");
      I740FreeRec(pScrn);
      return FALSE;
    }
  }
  xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	     pI740->LinearAddr);

  if (pI740->pEnt->device->IOBase != 0) {
    pI740->MMIOAddr = pI740->pEnt->device->IOBase;
    from = X_CONFIG;
  } else {
    if (pI740->PciInfo->memBase[1]) {
      pI740->MMIOAddr = pI740->PciInfo->memBase[1]&0xFFF80000;
      from = X_PROBED;
    } else {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "No valid MMIO address in PCI config space\n");
      I740FreeRec(pScrn);
      return FALSE;
    }
  }
  xf86DrvMsg(pScrn->scrnIndex, from, "IO registers at addr 0x%lX\n",
	     pI740->MMIOAddr);

  /* Calculate memory */
  if (pI740->pEnt->device->videoRam) {
    pScrn->videoRam = pI740->pEnt->device->videoRam;
    from = X_CONFIG;
  } else {
    if ((pI740->readControl(pI740, XRX, DRAM_ROW_TYPE)&DRAM_ROW_1)==DRAM_ROW_1_SDRAM)
      pScrn->videoRam=pI740->readControl(pI740, XRX, DRAM_ROW_BNDRY_1);
    else
      pScrn->videoRam=pI740->readControl(pI740, XRX, DRAM_ROW_BNDRY_0);
    pScrn->videoRam = (pScrn->videoRam&0x0F)*1024;
    from = X_PROBED;
  }

  temp=pI740->readControl(pI740, XRX, DRAM_ROW_CNTL_LO);
  pI740->HasSGRAM = !((temp&DRAM_RAS_TIMING)||(temp&DRAM_RAS_PRECHARGE));
  if (xf86IsOptionSet(pI740->Options, OPTION_SDRAM)) {
    if (xf86IsOptionSet(pI740->Options, OPTION_SGRAM)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		 "It is nonsensical to set both SDRAM and SGRAM options\n");
      return FALSE;
    }
    if (xf86ReturnOptValBool(pI740->Options, OPTION_SDRAM, FALSE)) {
      pI740->HasSGRAM = FALSE;
    } else {
      pI740->HasSGRAM = TRUE;
    }
  } else {
    if (xf86IsOptionSet(pI740->Options, OPTION_SDRAM)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		 "It is nonsensical to set both SDRAM and SGRAM options\n");
      return FALSE;
    }
    if (xf86ReturnOptValBool(pI740->Options, OPTION_SGRAM, FALSE)) {
      pI740->HasSGRAM = TRUE;
    } else {
      pI740->HasSGRAM = FALSE;
    }
  }

  xf86DrvMsg(pScrn->scrnIndex, from, "Steve was here! VideoRAM: %d kByte %s\n",
	     pScrn->videoRam, (pI740->HasSGRAM)?"SGRAM":"SDRAM");
  pI740->FbMapSize = pScrn->videoRam*1024;

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

  pI740->MaxClock = 0;
  if (pI740->pEnt->device->dacSpeeds[0]) {
    switch (pScrn->bitsPerPixel) {
    case 8:
      pI740->MaxClock = pI740->pEnt->device->dacSpeeds[DAC_BPP8];
      break;
    case 16:
      pI740->MaxClock = pI740->pEnt->device->dacSpeeds[DAC_BPP16];
      break;
    case 24:
      pI740->MaxClock = pI740->pEnt->device->dacSpeeds[DAC_BPP24];
      break;
    case 32:
      pI740->MaxClock = pI740->pEnt->device->dacSpeeds[DAC_BPP32];
      break;
    }
    if (!pI740->MaxClock)
      pI740->MaxClock = pI740->pEnt->device->dacSpeeds[0];
    from = X_CONFIG;
  } else {
    switch (pScrn->bitsPerPixel) {
    case 8:
      pI740->MaxClock = 203000;
      break;
    case 16:
      pI740->MaxClock = 163000;
      break;
    case 24:
      if (pI740->HasSGRAM)
	pI740->MaxClock = 136000;
      else
	pI740->MaxClock = 128000;
      break;
    case 32:
      pI740->MaxClock = 86000;
    }
  }
  clockRanges = xnfcalloc(sizeof(ClockRange), 1);
  clockRanges->next=NULL;
  clockRanges->minClock= 12000; /* !!! What's the min clock? !!! */
  clockRanges->maxClock=pI740->MaxClock;
  clockRanges->clockIndex = -1;
  clockRanges->interlaceAllowed = FALSE; /*PL*/
  clockRanges->doubleScanAllowed = TRUE; /*PL*/

  i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			pScrn->display->modes, clockRanges,
			0, 320, 1600,
			8, 200, 1200,
			pScrn->display->virtualX, pScrn->display->virtualY,
			pI740->FbMapSize, LOOKUP_BEST_REFRESH);

  if (i==-1) {
    I740FreeRec(pScrn);
    return FALSE;
  }

  xf86PruneDriverModes(pScrn);

  if (!i || !pScrn->modes) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
    I740FreeRec(pScrn);
    return FALSE;
  }

  xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

  pScrn->currentMode = pScrn->modes;

  xf86PrintModes(pScrn);

  xf86SetDpi(pScrn, 0, 0);

#ifdef USE_FB
  if (!xf86LoadSubModule(pScrn, "fb")) {
    I740FreeRec(pScrn);
    return FALSE;
  }
  xf86LoaderReqSymbols("fbScreenInit","fbPictureInit", NULL);
#else
  switch (pScrn->bitsPerPixel) {
  case 8:
    mod = "cfb";
    reqSym = "cfbScreenInit";
    break;
  case 16:
    mod = "cfb16";
    reqSym = "cfb16ScreenInit";
    break;
  case 24:
    mod = "cfb24";
    reqSym = "cfb24ScreenInit";
    break;
  case 32:
    mod = "cfb32";
    reqSym = "cfb32ScreenInit";
    break;
  }
  if (mod && !xf86LoadSubModule(pScrn, mod)) {
    I740FreeRec(pScrn);
    return FALSE;
  }
  xf86LoaderReqSymbols(reqSym, NULL);
#endif

  if (!xf86ReturnOptValBool(pI740->Options, OPTION_NOACCEL, FALSE)) {
    if (!xf86LoadSubModule(pScrn, "xaa")) {
      I740FreeRec(pScrn);
      return FALSE;
    }
    xf86LoaderReqSymLists(xaaSymbols, NULL);
  }

  if (!xf86ReturnOptValBool(pI740->Options, OPTION_SW_CURSOR, FALSE)) {
    if (!xf86LoadSubModule(pScrn, "ramdac")) {
      I740FreeRec(pScrn);
      return FALSE;
    }
    xf86LoaderReqSymLists(ramdacSymbols, NULL);
  }

  /*  We wont be using the VGA access after the probe */
  if (!xf86ReturnOptValBool(pI740->Options, OPTION_USE_PIO, FALSE)) {
    resRange vgaio[] = { {ResShrIoBlock,0x3B0,0x3BB},
			 {ResShrIoBlock,0x3C0,0x3DF},
			 _END };
    resRange vgamem[] = {{ResShrMemBlock,0xA0000,0xAFFFF},
			 {ResShrMemBlock,0xB8000,0xBFFFF},
			 {ResShrMemBlock,0xB0000,0xB7FFF},
			 _END };

    pI740->usePIO=FALSE;
    I740SetMMIOAccess(pI740);
    xf86SetOperatingState(vgaio, pI740->pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(vgamem, pI740->pEnt->index, ResDisableOpr);
  } else {
    pI740->usePIO=TRUE;
  }

  if(xf86IsOptionSet(pI740->Options, OPTION_VGACOMPAT))
    pI740->usevgacompat=TRUE;
  else
    pI740->usevgacompat=FALSE;


#if USE_DDC2 /*DDC2*/
  { /*PL*/

   if (xf86LoadSubModule(pScrn, "ddc")) {
     xf86LoaderReqSymLists(ddcSymbols, NULL);
     if ( xf86LoadSubModule(pScrn, "i2c") ) {
       xf86LoaderReqSymLists(i2cSymbols,NULL);

       if (I740MapMem(pScrn))
	 {
	   if (I740_I2CInit(pScrn))
	     {
	       xf86MonPtr MonInfo;

	       if ((MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex,pI740->rc_i2c))) {
		 xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DDC Monitor info: %p\n",
			    MonInfo);
		 xf86PrintEDID( MonInfo );
		 xf86DrvMsg(pScrn->scrnIndex, X_INFO, "end of DDC Monitor "
			    "info\n\n");
		 xf86SetDDCproperties(pScrn,MonInfo);
	       }

	       //xf86SetDDCproperties(pScrn,xf86PrintEDID(  xf86DoEDID_DDC2(pScrn->scrnIndex,pI740->rc_i2c)));
	     }
	   else
	     xf86DrvMsg(pScrn->scrnIndex,X_ERROR,"I2C initialization failed\n");
	   
	   I740UnmapMem(pScrn);
	 }
     }
   }
  }
#endif /*DDC2*/

  { /* Overlay */
    pI740->colorKey = (1 << pScrn->offset.red) | (1 << pScrn->offset.green) |
      (((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue);

    pI740->colorKey &= ((1 << pScrn->depth) - 1);

    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video overlay key set to 0x%x\n", pI740->colorKey);
  }


  return TRUE;
}

static Bool I740MapMem(ScrnInfoPtr pScrn)
{
  int mmioFlags;
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);

  mmioFlags = VIDMEM_MMIO | VIDMEM_READSIDEEFFECT;

  pI740->MMIOBase = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, 
				      pI740->PciTag, 
				      pI740->MMIOAddr,
				      0x80000);
  if (!pI740->MMIOBase) return FALSE;

  pI740->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				pI740->PciTag,
				pI740->LinearAddr,
				pI740->FbMapSize);
  if (!pI740->FbBase) return FALSE;

  return TRUE;
}

static Bool I740UnmapMem(ScrnInfoPtr pScrn)
{
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);

  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pI740->MMIOBase, 0x80000);
  pI740->MMIOBase=0;

  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pI740->FbBase, pI740->FbMapSize);
  pI740->FbBase = 0;
  return TRUE;
}

/*
 * I740Save --
 *
 * This function saves the video state.  It reads all of the SVGA registers
 * into the vgaI740Rec data structure.  There is in general no need to
 * mask out bits here - just read the registers.
 */
static void
DoSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, I740RegPtr i740Reg, Bool saveFonts)
{
  I740Ptr pI740;
  vgaHWPtr hwp;

  pI740 = I740PTR(pScrn);
  hwp = VGAHWPTR(pScrn);

  /*
   * This function will handle creating the data structure and filling
   * in the generic VGA portion.
   */
  if (saveFonts)
    vgaHWSave(pScrn, vgaReg, VGA_SR_MODE|VGA_SR_FONTS);
  else
    vgaHWSave(pScrn, vgaReg, VGA_SR_MODE);

  /*
   * The port I/O code necessary to read in the extended registers 
   * into the fields of the vgaI740Rec structure goes here.
   */

  i740Reg->IOControl = pI740->readControl(pI740, XRX, IO_CTNL);
  i740Reg->AddressMapping = pI740->readControl(pI740, XRX, ADDRESS_MAPPING);
  i740Reg->BitBLTControl = pI740->readControl(pI740, XRX, BITBLT_CNTL);
  i740Reg->VideoClk2_M = pI740->readControl(pI740, XRX, VCLK2_VCO_M);
  i740Reg->VideoClk2_N = pI740->readControl(pI740, XRX, VCLK2_VCO_N);
  i740Reg->VideoClk2_MN_MSBs = pI740->readControl(pI740, XRX, VCLK2_VCO_MN_MSBS);
  i740Reg->VideoClk2_DivisorSel = pI740->readControl(pI740, XRX, VCLK2_VCO_DIV_SEL);
  i740Reg->PLLControl = pI740->readControl(pI740, XRX, PLL_CNTL);

  i740Reg->ExtVertTotal=hwp->readCrtc(hwp, EXT_VERT_TOTAL);
  i740Reg->ExtVertDispEnd=hwp->readCrtc(hwp, EXT_VERT_DISPLAY);
  i740Reg->ExtVertSyncStart=hwp->readCrtc(hwp, EXT_VERT_SYNC_START);
  i740Reg->ExtVertBlankStart=hwp->readCrtc(hwp, EXT_VERT_BLANK_START);
  i740Reg->ExtHorizTotal=hwp->readCrtc(hwp, EXT_HORIZ_TOTAL);
  i740Reg->ExtHorizBlank=hwp->readCrtc(hwp, EXT_HORIZ_BLANK);
  i740Reg->ExtOffset=hwp->readCrtc(hwp, EXT_OFFSET);
  i740Reg->InterlaceControl=hwp->readCrtc(hwp, INTERLACE_CNTL);

  i740Reg->PixelPipeCfg0 = pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  i740Reg->PixelPipeCfg1 = pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_1);
  i740Reg->PixelPipeCfg2 = pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_2);
  i740Reg->DisplayControl = pI740->readControl(pI740, XRX, DISPLAY_CNTL);
  
  i740Reg->LMI_FIFO_Watermark = INREG(FWATER_BLC);
}

static void
I740Save(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp;
  I740Ptr pI740;

  hwp = VGAHWPTR(pScrn);
  pI740 = I740PTR(pScrn);
  DoSave(pScrn, &hwp->SavedReg, &pI740->SavedReg, TRUE);
}

static void
DoRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, I740RegPtr i740Reg, 
	  Bool restoreFonts) {
  I740Ptr pI740;
  vgaHWPtr hwp;
  unsigned char temp;
  unsigned int  itemp;

  pI740 = I740PTR(pScrn);
  hwp = VGAHWPTR(pScrn);

  vgaHWProtect(pScrn, TRUE);
#if 0
  temp=hwp->readCrtc(hwp, VERT_SYNC_END);
  hwp->writeCrtc(hwp, VERT_SYNC_END, temp&0x7F);
#endif

  temp = pI740->readControl(pI740, MRX, ACQ_CNTL_2); 
  if ((temp & FRAME_CAP_MODE) == SINGLE_CAP_MODE) {
    temp=pI740->readControl(pI740, MRX, COL_KEY_CNTL_1);
    temp |= BLANK_DISP_OVERLAY; /* Disable the overlay */
    pI740->writeControl(pI740, MRX, COL_KEY_CNTL_1, temp);
  } else {
    temp &= ~FRAME_CAP_MODE;
    pI740->writeControl(pI740, MRX, ACQ_CNTL_2, temp);
  }
  usleep(50000);

  /* Turn off DRAM Refresh */
  pI740->writeControl(pI740, XRX, DRAM_EXT_CNTL, DRAM_REFRESH_DISABLE);

  usleep(1000); /* Wait 1 ms */

  /* Write the M, N and P values */
  pI740->writeControl(pI740, XRX, VCLK2_VCO_M, i740Reg->VideoClk2_M);
  pI740->writeControl(pI740, XRX, VCLK2_VCO_N, i740Reg->VideoClk2_N);
  pI740->writeControl(pI740, XRX, VCLK2_VCO_MN_MSBS, i740Reg->VideoClk2_MN_MSBs);
  pI740->writeControl(pI740, XRX, VCLK2_VCO_DIV_SEL, i740Reg->VideoClk2_DivisorSel);

  /*
   * Turn on 8 bit dac mode, if requested.  This is needed to make
   * sure that vgaHWRestore writes the values into the DAC properly.
   * The problem occurs if 8 bit dac mode is requested and the HW is
   * in 6 bit dac mode.  If this happens, all the values are
   * automatically shifted left twice by the HW and incorrect colors
   * will be displayed on the screen.  The only time this can happen
   * is at server startup time and when switching back from a VT.
   */
  temp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  temp &= 0x7F; /* Save all but the 8 bit dac mode bit */
  temp |= (i740Reg->PixelPipeCfg0 & DAC_8_BIT);
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_0, temp);

  /*
   * Code to restore any SVGA registers that have been saved/modified
   * goes here.  Note that it is allowable, and often correct, to 
   * only modify certain bits in a register by a read/modify/write cycle.
   *
   * A special case - when using an external clock-setting program,
   * this function must not change bits associated with the clock
   * selection.  This condition can be checked by the condition:
   *
   *	if (restore->std.NoClock >= 0)
   *		restore clock-select bits.
   */
  if (restoreFonts)
    vgaHWRestore(pScrn, vgaReg, VGA_SR_FONTS|VGA_SR_MODE);
  else
    vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);

  hwp->writeCrtc(hwp, EXT_VERT_TOTAL, i740Reg->ExtVertTotal);
  hwp->writeCrtc(hwp, EXT_VERT_DISPLAY, i740Reg->ExtVertDispEnd);
  hwp->writeCrtc(hwp, EXT_VERT_SYNC_START, i740Reg->ExtVertSyncStart);
  hwp->writeCrtc(hwp, EXT_VERT_BLANK_START, i740Reg->ExtVertBlankStart);
  hwp->writeCrtc(hwp, EXT_HORIZ_TOTAL, i740Reg->ExtHorizTotal);
  hwp->writeCrtc(hwp, EXT_HORIZ_BLANK, i740Reg->ExtHorizBlank);
  hwp->writeCrtc(hwp, EXT_OFFSET, i740Reg->ExtOffset);

  temp=hwp->readCrtc(hwp, INTERLACE_CNTL);
  temp &= ~INTERLACE_ENABLE;
  temp |= i740Reg->InterlaceControl;
  hwp->writeCrtc(hwp, INTERLACE_CNTL, temp);

  temp=pI740->readControl(pI740, XRX, ADDRESS_MAPPING);
  temp &= 0xE0; /* Save reserved bits 7:5 */
  temp |= i740Reg->AddressMapping;
  pI740->writeControl(pI740, XRX, ADDRESS_MAPPING, temp);

  temp=pI740->readControl(pI740, XRX, BITBLT_CNTL);
  temp &= ~COLEXP_MODE;
  temp |= i740Reg->BitBLTControl;
  pI740->writeControl(pI740, XRX, BITBLT_CNTL, temp);

  temp=pI740->readControl(pI740, XRX, DISPLAY_CNTL);
  temp &= ~(VGA_WRAP_MODE | GUI_MODE);
  temp |= i740Reg->DisplayControl;
  pI740->writeControl(pI740, XRX, DISPLAY_CNTL, temp);

  temp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  temp &= 0x64; /* Save reserved bits 6:5,2 */
  temp |= i740Reg->PixelPipeCfg0;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_0, temp);

  temp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_2);
  temp &= 0xF3; /* Save reserved bits 7:4,1:0 */
  temp |= i740Reg->PixelPipeCfg2;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_2, temp);

  temp=pI740->readControl(pI740, XRX, PLL_CNTL);
  temp = i740Reg->PLLControl; /* To fix the 2.3X BIOS problem */
  pI740->writeControl(pI740, XRX, PLL_CNTL, temp);

  temp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_1);
  temp &= ~DISPLAY_COLOR_MODE;
  temp |= i740Reg->PixelPipeCfg1;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_1, temp);

  itemp = INREG(FWATER_BLC);
  itemp &= ~(LMI_BURST_LENGTH | LMI_FIFO_WATERMARK);
  itemp |= i740Reg->LMI_FIFO_Watermark;
  OUTREG(FWATER_BLC, itemp);

  /* Turn on DRAM Refresh */
  pI740->writeControl(pI740, XRX, DRAM_EXT_CNTL, DRAM_REFRESH_60HZ);

  temp=pI740->readControl(pI740, MRX, COL_KEY_CNTL_1);
  temp &= ~BLANK_DISP_OVERLAY; /* Re-enable the overlay */
  pI740->writeControl(pI740, MRX, COL_KEY_CNTL_1, temp);

  if (!(vgaReg->Attribute[0x10] & 0x1)) {
    usleep(50000);
    if (restoreFonts)
      vgaHWRestore(pScrn, vgaReg, VGA_SR_FONTS|VGA_SR_MODE);
    else
      vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);
  }

  vgaHWProtect(pScrn, FALSE);
  temp=pI740->readControl(pI740, XRX, IO_CTNL);
  temp &= ~(EXTENDED_ATTR_CNTL | EXTENDED_CRTC_CNTL);
  temp |= i740Reg->IOControl;
  pI740->writeControl(pI740, XRX, IO_CTNL, temp);
#if 0
  temp=hwp->readCrtc(hwp, VERT_SYNC_END);
  hwp->writeCrtc(hwp, VERT_SYNC_END, temp|0x80);
#endif
}

static void
I740Restore(ScrnInfoPtr pScrn) {
  vgaHWPtr hwp;
  I740Ptr pI740;

  hwp = VGAHWPTR(pScrn);
  pI740 = I740PTR(pScrn);

  DoRestore(pScrn, &hwp->SavedReg, &pI740->SavedReg, TRUE);
}

/*
 * I740CalcFIFO --
 *
 * Calculate burst length and FIFO watermark.
 */

static unsigned int
I740CalcFIFO(ScrnInfoPtr pScrn, double freq)
{
  unsigned int wm = 0x18120000;
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);

  /*
   * Would like to calculate these values automatically, but a generic
   * algorithm does not seem possible.  Note: These FIFO water mark
   * values were tested on several cards and seem to eliminate the
   * all of the snow and vertical banding, but fine adjustments will
   * probably be required for other cards.
   */

  switch (pScrn->bitsPerPixel) {
  case 8:
    if (pI740->HasSGRAM) {
      if      (freq > 200) wm = 0x18120000;
      else if (freq > 175) wm = 0x16110000;
      else if (freq > 135) wm = 0x120E0000;
      else                 wm = 0x100D0000;
    } else {
      if      (freq > 200) wm = 0x18120000;
      else if (freq > 175) wm = 0x16110000;
      else if (freq > 135) wm = 0x120E0000;
      else                 wm = 0x100D0000;
    }
    break;
  case 16:
    if (pI740->HasSGRAM) {
      if      (freq > 140) wm = 0x2C1D0000;
      else if (freq > 120) wm = 0x2C180000;
      else if (freq > 100) wm = 0x24160000;
      else if (freq >  90) wm = 0x18120000;
      else if (freq >  50) wm = 0x16110000;
      else if (freq >  32) wm = 0x13100000;
      else                 wm = 0x120E0000;
    } else {
      if      (freq > 160) wm = 0x28200000;
      else if (freq > 140) wm = 0x2A1E0000;
      else if (freq > 130) wm = 0x2B1A0000;
      else if (freq > 120) wm = 0x2C180000;
      else if (freq > 100) wm = 0x24180000;
      else if (freq >  90) wm = 0x18120000;
      else if (freq >  50) wm = 0x16110000;
      else if (freq >  32) wm = 0x13100000;
      else                 wm = 0x120E0000;
    }
    break;
  case 24:
    if (pI740->HasSGRAM) {
      if      (freq > 130) wm = 0x31200000;
      else if (freq > 120) wm = 0x2E200000;
      else if (freq > 100) wm = 0x2C1D0000;
      else if (freq >  80) wm = 0x25180000;
      else if (freq >  64) wm = 0x24160000;
      else if (freq >  49) wm = 0x18120000;
      else if (freq >  32) wm = 0x16110000;
      else                 wm = 0x13100000;
    } else {
      if      (freq > 120) wm = 0x311F0000;
      else if (freq > 100) wm = 0x2C1D0000;
      else if (freq >  80) wm = 0x25180000;
      else if (freq >  64) wm = 0x24160000;
      else if (freq >  49) wm = 0x18120000;
      else if (freq >  32) wm = 0x16110000;
      else                 wm = 0x13100000;
    }
    break;
  case 32:
    if (pI740->HasSGRAM) {
      if      (freq >  80) wm = 0x2A200000;
      else if (freq >  60) wm = 0x281A0000;
      else if (freq >  49) wm = 0x25180000;
      else if (freq >  32) wm = 0x18120000;
      else                 wm = 0x16110000;
    } else {
      if      (freq >  80) wm = 0x29200000;
      else if (freq >  60) wm = 0x281A0000;
      else if (freq >  49) wm = 0x25180000;
      else if (freq >  32) wm = 0x18120000;
      else                 wm = 0x16110000;
    }
    break;
  }

  return wm;
}

/*
 * I740CalcVCLK --
 *
 * Determine the closest clock frequency to the one requested.
 */

#define MAX_VCO_FREQ 450.0
#define TARGET_MAX_N 30
#define REF_FREQ 66.66666666667

#define CALC_VCLK(m,n,p,d) \
    (double)m / ((double)n * (1 << p)) * (4 << (d << 1)) * REF_FREQ

static void
I740CalcVCLK(ScrnInfoPtr pScrn, double freq)
{
  I740Ptr pI740;
  I740RegPtr i740Reg;
  int m, n, p, d;
  double f_out;
  double f_err;
  double f_vco;
  int m_best = 0, n_best = 0, p_best = 0, d_best = 0;
  double f_target = freq;
  double err_max = 0.005;
  double err_target = 0.001;
  double err_best = 999999.0;

  pI740 = I740PTR(pScrn);
  i740Reg = &pI740->ModeReg;

  p_best = p = log(MAX_VCO_FREQ/f_target)/log((double)2);
  d_best = d = 0;

  f_vco = f_target * (1 << p);

  n = 2;
  do {
    n++;
    m = f_vco / (REF_FREQ / (double)n) / (double)4.0 + 0.5;
    if (m < 3) m = 3;
    f_out = CALC_VCLK(m,n,p,d);
    f_err = 1.0 - (f_target/f_out);
    if (fabs(f_err) < err_max) {
      m_best = m;
      n_best = n;
      err_best = f_err;
    }
  } while ((fabs(f_err) >= err_target) &&
	   ((n <= TARGET_MAX_N) || (fabs(err_best) > err_max)));

  if (fabs(f_err) < err_target) {
    m_best = m;
    n_best = n;
  }

  i740Reg->VideoClk2_M          = (m_best-2) & 0xFF;
  i740Reg->VideoClk2_N          = (n_best-2) & 0xFF;
  i740Reg->VideoClk2_MN_MSBs    = ((((n_best-2) >> 4) & VCO_N_MSBS) |
				   (((m_best-2) >> 8) & VCO_M_MSBS));
  i740Reg->VideoClk2_DivisorSel = ((p_best << 4) |
				   (d_best ? 4 : 0) |
				   REF_DIV_1);
}

static Bool
I740SetMode(ScrnInfoPtr pScrn, DisplayModePtr mode) {
  I740Ptr pI740;
  I740RegPtr i740Reg;
  vgaRegPtr pVga;
  double dclk = mode->Clock/1000.0;

  pI740 = I740PTR(pScrn);
  i740Reg = &pI740->ModeReg;
  pVga = &VGAHWPTR(pScrn)->ModeReg;

  switch (pScrn->bitsPerPixel) {
  case 8:
    pVga->CRTC[0x13] = pScrn->displayWidth >> 3;
    i740Reg->ExtOffset      = pScrn->displayWidth >> 11;
    i740Reg->PixelPipeCfg1 = DISPLAY_8BPP_MODE;
    i740Reg->BitBLTControl = COLEXP_8BPP;
    break;
  case 16:
    if (pScrn->weight.green == 5) {
      i740Reg->PixelPipeCfg1 = DISPLAY_15BPP_MODE;
    } else {
      i740Reg->PixelPipeCfg1 = DISPLAY_16BPP_MODE;
    }
    pVga->CRTC[0x13] = pScrn->displayWidth >> 2;
    i740Reg->ExtOffset      = pScrn->displayWidth >> 10;
    i740Reg->BitBLTControl = COLEXP_16BPP;
    break;
  case 24:
    pVga->CRTC[0x13] = (pScrn->displayWidth * 3) >> 3;
    i740Reg->ExtOffset      = (pScrn->displayWidth * 3) >> 11;
    i740Reg->PixelPipeCfg1 = DISPLAY_24BPP_MODE;
    i740Reg->BitBLTControl = COLEXP_24BPP;
    break;
  case 32:
    pVga->CRTC[0x13] = pScrn->displayWidth >> 1;
    i740Reg->ExtOffset      = pScrn->displayWidth >> 9;
    i740Reg->PixelPipeCfg1 = DISPLAY_32BPP_MODE;
    i740Reg->BitBLTControl = COLEXP_RESERVED; /* Not implemented on i740 */
    break;
  default:
    break;
  }

  /* Turn on 8 bit dac if requested */
  if (xf86ReturnOptValBool(pI740->Options, OPTION_DAC_6BIT, FALSE))
    i740Reg->PixelPipeCfg0 = DAC_6_BIT;
  else
    i740Reg->PixelPipeCfg0 = DAC_8_BIT;

  i740Reg->PixelPipeCfg2 = DISPLAY_GAMMA_ENABLE /*| OVERLAY_GAMMA_ENABLE*/;

  /* Turn on Extended VGA Interpretation */
  i740Reg->IOControl = EXTENDED_CRTC_CNTL;

  /* Turn on linear and page mapping */
  i740Reg->AddressMapping = LINEAR_MODE_ENABLE | PAGE_MAPPING_ENABLE;

  /* Turn on GUI mode */
  i740Reg->DisplayControl = HIRES_MODE;

  /* Set the MCLK freq */
  if (xf86ReturnOptValBool(pI740->Options, OPTION_SLOW_RAM, FALSE))
    i740Reg->PLLControl = PLL_MEMCLK__66667KHZ; /*  66 MHz */
  else
    i740Reg->PLLControl = PLL_MEMCLK_100000KHZ; /* 100 MHz -- use as default */

  /* Calculate the extended CRTC regs */
  i740Reg->ExtVertTotal = (mode->CrtcVTotal - 2) >> 8;
  i740Reg->ExtVertDispEnd = (mode->CrtcVDisplay - 1) >> 8;
  i740Reg->ExtVertSyncStart = mode->CrtcVSyncStart >> 8;
  i740Reg->ExtVertBlankStart = mode->CrtcVBlankStart >> 8;
  i740Reg->ExtHorizTotal = ((mode->CrtcHTotal >> 3) - 5) >> 8;
  /*
   * the KGA fix in vgaHW.c results in the first
   * scanline and the first character clock (8 pixels)
   * of each scanline thereafter on display with an i740
   * to be blank. Restoring CRTC 3, 5, & 22 to their
   * "theoretical" values corrects the problem. KAO.
   */
  i740Reg->ExtHorizBlank = vgaHWHBlankKGA(mode, pVga, 7, 0) << 6;
  vgaHWVBlankKGA(mode, pVga, 8, 0);

  /* Turn on interlaced mode if necessary */
  if (mode->Flags & V_INTERLACE)
    i740Reg->InterlaceControl = INTERLACE_ENABLE;
  else
    i740Reg->InterlaceControl = INTERLACE_DISABLE;

  /*
   * Set the overscan color to 0.
   * NOTE: This only affects >8bpp mode.
   */
  pVga->Attribute[0x11] = 0;

  /*
   * Calculate the VCLK that most closely matches the requested dot
   * clock.
   */
  I740CalcVCLK(pScrn, dclk);

  /* Since we program the clocks ourselves, always use VCLK2. */
  pVga->MiscOutReg |= 0x0C;

  /* Calculate the FIFO Watermark and Burst Length. */
  i740Reg->LMI_FIFO_Watermark = I740CalcFIFO(pScrn, dclk);

  /*-Overlay-*/
  pI740->ov_offset_x=((mode->CrtcHTotal-mode->CrtcHDisplay) & ~7)-9;
  pI740->ov_offset_y=mode->CrtcVTotal-mode->CrtcVSyncEnd-2;
  /*-*/

  return TRUE;
}

static Bool
I740ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode_src)
{
  vgaHWPtr hwp;
  I740Ptr pI740;
  struct _DisplayModeRec mode_dst;
  DisplayModePtr mode=&mode_dst;

  *mode=*mode_src;

  hwp = VGAHWPTR(pScrn);
  pI740 = I740PTR(pScrn);

  vgaHWUnlock(hwp);


  if(pI740->usevgacompat)
    { /* Try to get the same visual aspect as a S3 board */
      mode->CrtcHSyncStart+=16;
      mode->CrtcHSyncEnd  +=16;
    }

  if (!vgaHWInit(pScrn, mode)) return FALSE;

  pScrn->vtSema = TRUE;

  if (!I740SetMode(pScrn, mode)) return FALSE;

  DoRestore(pScrn, &hwp->ModeReg, &pI740->ModeReg, FALSE);

  return TRUE;
}

static void I740LoadPalette15(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors, VisualPtr pVisual)
{
  vgaHWPtr hwp;
  int i, index;
  unsigned char r, g, b;

  hwp = VGAHWPTR(pScrn);

  for (i=0; i<numColors; i++)
    {
      index=indices[i/2];
      r=colors[index].red;
      b=colors[index].blue;
      g=colors[index].green;

      hwp->writeDacWriteAddr(hwp, index<<2);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      i++;
      hwp->writeDacWriteAddr(hwp, index<<2);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);
    }
}

static void I740LoadPalette16(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors, VisualPtr pVisual)
{
  vgaHWPtr hwp;
  int i, index;
  unsigned char r, g, b;

  hwp = VGAHWPTR(pScrn);
  for (i=0; i<numColors; i++) {
    index=indices[i/2];
    r=colors[index].red;
    b=colors[index].blue;
    index=indices[i];
    g=colors[index].green;
    hwp->writeDacWriteAddr(hwp, index<<2);
    hwp->writeDacData(hwp, r);
    hwp->writeDacData(hwp, g);
    hwp->writeDacData(hwp, b);
    i++;
    index=indices[i];
    g=colors[index].green;
    hwp->writeDacWriteAddr(hwp, index<<2);
    hwp->writeDacData(hwp, r);
    hwp->writeDacData(hwp, g);
    hwp->writeDacData(hwp, b);
  }
}

static void
I740LoadPalette24(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
		  VisualPtr pVisual) {
  vgaHWPtr hwp;
  int i, index;
  unsigned char r, g, b;

  hwp = VGAHWPTR(pScrn);
  for (i=0; i<numColors; i++) {
    index=indices[i];
    r=colors[index].red;
    b=colors[index].blue;
    index=indices[i];
    g=colors[index].green;
    hwp->writeDacWriteAddr(hwp, index);
    hwp->writeDacData(hwp, r);
    hwp->writeDacData(hwp, g);
    hwp->writeDacData(hwp, b);
  }
}

static Bool
I740ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv) {
  ScrnInfoPtr pScrn;
  vgaHWPtr hwp;
  I740Ptr pI740;
  VisualPtr visual;

  pScrn = xf86Screens[pScreen->myNum];
  pI740 = I740PTR(pScrn);
  hwp = VGAHWPTR(pScrn);

  if (!I740MapMem(pScrn)) return FALSE;
  pScrn->memPhysBase = pI740->LinearAddr;
  pScrn->fbOffset = 0;

  if (!pI740->usePIO)
    vgaHWSetMmioFuncs(hwp, pI740->MMIOBase, 0);
  vgaHWGetIOBase(hwp);
  if (!vgaHWMapMem(pScrn)) return FALSE;

  I740Save(pScrn);
  if (!I740ModeInit(pScrn, pScrn->currentMode)) return FALSE;

  I740SaveScreen(pScreen, SCREEN_SAVER_ON);
  I740AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

  miClearVisualTypes();

  if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
			pScrn->rgbBits, pScrn->defaultVisual))
    return FALSE;
#ifdef USE_FB
	if (!miSetPixmapDepths ()) return FALSE;
#endif

  switch (pScrn->bitsPerPixel) {
#ifdef USE_FB
  case 8:
  case 16:
  case 24:
  case 32:
    if (!fbScreenInit(pScreen, pI740->FbBase, 
		       pScrn->virtualX, pScrn->virtualY,
		       pScrn->xDpi, pScrn->yDpi,
		       pScrn->displayWidth,pScrn->bitsPerPixel))
      return FALSE;
    break;
#else
  case 8:
    if (!cfbScreenInit(pScreen, pI740->FbBase, 
		       pScrn->virtualX, pScrn->virtualY,
		       pScrn->xDpi, pScrn->yDpi,
		       pScrn->displayWidth))
      return FALSE;
    break;
  case 16:
    if (!cfb16ScreenInit(pScreen, pI740->FbBase, 
			 pScrn->virtualX, pScrn->virtualY,
			 pScrn->xDpi, pScrn->yDpi,
			 pScrn->displayWidth))
      return FALSE;
    break;
  case 24:
    if (!cfb24ScreenInit(pScreen, pI740->FbBase, 
			 pScrn->virtualX, pScrn->virtualY,
			 pScrn->xDpi, pScrn->yDpi,
			 pScrn->displayWidth))
      return FALSE;
    break;
  case 32:
    if (!cfb32ScreenInit(pScreen, pI740->FbBase, 
			 pScrn->virtualX, pScrn->virtualY,
			 pScrn->xDpi, pScrn->yDpi,
			 pScrn->displayWidth))
      return FALSE;
    break;
#endif
  default:
    xf86DrvMsg(scrnIndex, X_ERROR,
	       "Internal error: invalid bpp (%d) in I740ScrnInit\n",
	       pScrn->bitsPerPixel);
    return FALSE;
  }
#ifdef USE_FB
  fbPictureInit(pScreen,0,0);
#endif

  xf86SetBlackWhitePixels(pScreen);

  memset(&(pI740->FbMemBox), 0, sizeof(BoxRec));
  pI740->FbMemBox.x1=0;
  pI740->FbMemBox.x2=pScrn->displayWidth;
  pI740->FbMemBox.y1=0;
  pI740->FbMemBox.y2=pI740->FbMapSize/(pScrn->displayWidth*pI740->cpp);

  I740DGAInit(pScreen);

  if (!xf86InitFBManager(pScreen, &pI740->FbMemBox)) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to init memory manager\n");
    return FALSE;
  }

  if (!xf86ReturnOptValBool(pI740->Options, OPTION_NOACCEL, FALSE)) {
    if (!I740AccelInit(pScreen)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Hardware acceleration initialization failed\n");
    }
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

  miInitializeBackingStore(pScreen);
  xf86SetBackingStore(pScreen);
  xf86SetSilkenMouse(pScreen);

  miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

  if (!xf86ReturnOptValBool(pI740->Options, OPTION_SW_CURSOR, FALSE)) {
    if (!I740CursorInit(pScreen)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Hardware cursor initialization failed\n");
    }
  }

  if (!miCreateDefColormap(pScreen)) return FALSE;

  if (pScrn->bitsPerPixel==16)
    {
      if (pScrn->weight.green == 5)
	{
	  if (!xf86HandleColormaps(pScreen, 256, 8, I740LoadPalette15, 0, CMAP_PALETTED_TRUECOLOR|CMAP_RELOAD_ON_MODE_SWITCH))
	    return FALSE;
	}
      else
	{
	  if (!xf86HandleColormaps(pScreen, 256, 8, I740LoadPalette16, 0, CMAP_PALETTED_TRUECOLOR|CMAP_RELOAD_ON_MODE_SWITCH))
	    return FALSE;
	}
    }
  else
    {
      if (!xf86HandleColormaps(pScreen, 256, 8, I740LoadPalette24, 0, CMAP_PALETTED_TRUECOLOR|CMAP_RELOAD_ON_MODE_SWITCH))
	return FALSE;
    }

  xf86DPMSInit(pScreen, I740DisplayPowerManagementSet, 0);

  pScreen->SaveScreen = I740SaveScreen;
  pI740->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = I740CloseScreen;

  if (serverGeneration == 1)
    xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

  /* Overlay */
  I740InitVideo(pScreen); pI740->OverlayStart = pI740->CursorStart + 1024;

  return TRUE;
}

Bool
I740SwitchMode(int scrnIndex, DisplayModePtr mode, int flags) {
  ScrnInfoPtr pScrn;

  pScrn=xf86Screens[scrnIndex];
  return I740ModeInit(pScrn, mode);
}

void
I740AdjustFrame(int scrnIndex, int x, int y, int flags) {
  ScrnInfoPtr pScrn;
  int Base;
  vgaHWPtr hwp;

  pScrn = xf86Screens[scrnIndex];
  hwp = VGAHWPTR(pScrn);

  Base = (y * pScrn->displayWidth + x) >> 2;
  switch (pScrn->bitsPerPixel) {
  case  8:	
    break;
  case 16:
    Base *= 2;
    break;
  case 24:
    /*
     * The last bit does not seem to have any effect on the start
     * address register in 24bpp mode, so...
     */
    Base &= 0xFFFFFFFE; /* ...ignore the last bit. */
    Base *= 3;
    break;
  case 32:
    Base *= 4;
    break;
  }

  hwp->writeCrtc(hwp, START_ADDR_LO, Base&0xFF);
  hwp->writeCrtc(hwp, START_ADDR_HI, (Base&0xFF00)>>8);
  hwp->writeCrtc(hwp, EXT_START_ADDR_HI, (Base&0x3FC00000)>>22);
  hwp->writeCrtc(hwp, EXT_START_ADDR, 
		 ((Base&0x00eF0000)>>16|EXT_START_ADDR_ENABLE));
}

static Bool
I740EnterVT(int scrnIndex, int flags) {
  ScrnInfoPtr pScrn;

  pScrn = xf86Screens[scrnIndex];
  if (!I740ModeInit(pScrn, pScrn->currentMode)) return FALSE;
  I740AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
  return TRUE;
}

static void
I740LeaveVT(int scrnIndex, int flags) {
  ScrnInfoPtr pScrn;
  vgaHWPtr hwp;

  pScrn = xf86Screens[scrnIndex];
  hwp=VGAHWPTR(pScrn);
  I740Restore(pScrn);
  vgaHWLock(hwp);
}

static Bool
I740CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  vgaHWPtr hwp;
  I740Ptr pI740;

  pScrn = xf86Screens[scrnIndex];
  hwp = VGAHWPTR(pScrn);
  pI740 = I740PTR(pScrn);

  if (pScrn->vtSema) {
      I740Restore(pScrn);
      vgaHWLock(hwp);
  }
  
  I740UnmapMem(pScrn);
  vgaHWUnmapMem(pScrn);
  if (pI740->AccelInfoRec)
    XAADestroyInfoRec(pI740->AccelInfoRec);
  pI740->AccelInfoRec=0;
  if (pI740->CursorInfoRec)
    xf86DestroyCursorInfoRec(pI740->CursorInfoRec);
  pI740->CursorInfoRec=0;
  pScrn->vtSema=FALSE;

  pScreen->CloseScreen = pI740->CloseScreen;
  return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

static void
I740FreeScreen(int scrnIndex, int flags) {
  I740FreeRec(xf86Screens[scrnIndex]);
  if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
    vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

static ModeStatus
I740ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags) {
  if (mode->Flags & V_INTERLACE) {
    if (verbose) {
      xf86DrvMsg(scrnIndex, X_PROBED, 
		 "Removing interlaced mode \"%s\"\n",
		 mode->name);
    }
    return MODE_BAD;
  }
  return MODE_OK;
}

static Bool
I740SaveScreen(ScreenPtr pScreen, int mode)
{
#if 0
  Bool unblack = xf86IsUnblank(mode);
  if (unblack) outw(SRX, 0x0300);
  else outw(SRX, 0x0100);
#endif
  return vgaHWSaveScreen(pScreen, mode);
}

static void
I740DisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, 
			      int flags) {
  I740Ptr pI740;
  unsigned char SEQ01=0;
  int DPMSSyncSelect=0;

  pI740 = I740PTR(pScrn);
  switch (PowerManagementMode) {
  case DPMSModeOn:
    /* Screen: On; HSync: On, VSync: On */
    SEQ01 = 0x00;
    DPMSSyncSelect = HSYNC_ON | VSYNC_ON;
    break;
  case DPMSModeStandby:
    /* Screen: Off; HSync: Off, VSync: On */
    SEQ01 = 0x20;
    DPMSSyncSelect = HSYNC_OFF | VSYNC_ON;
    break;
  case DPMSModeSuspend:
    /* Screen: Off; HSync: On, VSync: Off */
    SEQ01 = 0x20;
    DPMSSyncSelect = HSYNC_ON | VSYNC_OFF;
    break;
  case DPMSModeOff:
    /* Screen: Off; HSync: Off, VSync: Off */
    SEQ01 = 0x20;
    DPMSSyncSelect = HSYNC_OFF | VSYNC_OFF;
    break;
  }

  /* Turn the screen on/off */
  SEQ01 |= pI740->readControl(pI740, SRX, 0x01) & ~0x20;
  pI740->writeControl(pI740, SRX, 0x01, SEQ01);

  /* Set the DPMS mode */
  pI740->writeControl(pI740, XRX, DPMS_SYNC_SELECT, DPMSSyncSelect);
}
