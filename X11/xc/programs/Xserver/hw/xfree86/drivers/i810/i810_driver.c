
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_driver.c,v 1.80 2003/02/26 04:19:36 dawes Exp $ */

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */

/*
 * This server does not support these XFree86 4.0 features yet
 * shadowFb (if requested or acceleration is off)
 * Overlay planes
 * DGA
 */

/*
 * These are X and server generic header files.
 */
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86RAC.h"
#include "xf86cmap.h"
#include "compiler.h"
#include "mibstore.h"
#include "vgaHW.h"
#include "mipointer.h"
#include "micmap.h"

#include "fb.h"
#include "miscstruct.h"
#include "xf86xv.h"
#include "Xv.h"
#include "vbe.h"

#include "i810.h"

#ifdef XF86DRI
#include "dri.h"
#endif

/* Required Functions: */

static void I810Identify(int flags);
static Bool I810Probe(DriverPtr drv, int flags);
#ifndef I830_ONLY
static Bool I810PreInit(ScrnInfoPtr pScrn, int flags);
static Bool I810ScreenInit(int Index, ScreenPtr pScreen, int argc,
			   char **argv);
static Bool I810EnterVT(int scrnIndex, int flags);
static void I810LeaveVT(int scrnIndex, int flags);
static Bool I810CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool I810SaveScreen(ScreenPtr pScreen, Bool unblank);
static void I810FreeScreen(int scrnIndex, int flags);
static void I810DisplayPowerManagementSet(ScrnInfoPtr pScrn,
					  int PowerManagermentMode,
					  int flags);
static int I810ValidMode(int scrnIndex, DisplayModePtr mode, Bool
			 verbose, int flags);
#endif /* I830_ONLY */


DriverRec I810 = {
   I810_VERSION,
   I810_DRIVER_NAME,
   I810Identify,
   I810Probe,
   I810AvailableOptions,
   NULL,
   0
};

/* *INDENT-OFF* */
/* Chipsets */
static SymTabRec I810Chipsets[] = {
#ifndef I830_ONLY
   {PCI_CHIP_I810,		"i810"},
   {PCI_CHIP_I810_DC100,	"i810-dc100"},
   {PCI_CHIP_I810_E,		"i810e"},
   {PCI_CHIP_I815,		"i815"},
#endif
   {PCI_CHIP_I830_M,		"i830M"},
   {PCI_CHIP_845_G,		"845G"},
   {PCI_CHIP_I855_GM,		"852GM/855GM"},
   {PCI_CHIP_I865_G,		"865G"},
   {-1,				NULL}
};

static PciChipsets I810PciChipsets[] = {
#ifndef I830_ONLY
   {PCI_CHIP_I810,		PCI_CHIP_I810,		RES_SHARED_VGA},
   {PCI_CHIP_I810_DC100,	PCI_CHIP_I810_DC100,	RES_SHARED_VGA},
   {PCI_CHIP_I810_E,		PCI_CHIP_I810_E,	RES_SHARED_VGA},
   {PCI_CHIP_I815,		PCI_CHIP_I815,		RES_SHARED_VGA},
#endif
   {PCI_CHIP_I830_M,		PCI_CHIP_I830_M,	RES_SHARED_VGA},
   {PCI_CHIP_845_G,		PCI_CHIP_845_G,		RES_SHARED_VGA},
   {PCI_CHIP_I855_GM,		PCI_CHIP_I855_GM,	RES_SHARED_VGA},
   {PCI_CHIP_I865_G,		PCI_CHIP_I865_G,	RES_SHARED_VGA},
   {-1,				-1, RES_UNDEFINED }
};

#ifndef I830_ONLY
typedef enum {
   OPTION_NOACCEL,
   OPTION_SW_CURSOR,
   OPTION_COLOR_KEY,
   OPTION_CACHE_LINES,
   OPTION_DAC_6BIT,
   OPTION_DRI,
   OPTION_NO_DDC,
   OPTION_SHOW_CACHE,
   OPTION_XVMC_SURFACES
} I810Opts;
 
static const OptionInfoRec I810Options[] = {
   {OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE},
   {OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE},
   {OPTION_COLOR_KEY,		"ColorKey",	OPTV_INTEGER,	{0}, FALSE},
   {OPTION_CACHE_LINES,		"CacheLines",	OPTV_INTEGER,	{0}, FALSE},
   {OPTION_DAC_6BIT,		"Dac6Bit",	OPTV_BOOLEAN,	{0}, FALSE},
   {OPTION_DRI,			"DRI",		OPTV_BOOLEAN,	{0}, FALSE},
   {OPTION_NO_DDC,		"NoDDC",	OPTV_BOOLEAN,	{0}, FALSE},
   {OPTION_SHOW_CACHE,		"ShowCache",	OPTV_BOOLEAN,	{0}, FALSE},
   {OPTION_XVMC_SURFACES,	"XvMCSurfaces",	OPTV_INTEGER,	{0}, FALSE},
   {-1,				NULL,		OPTV_NONE,	{0}, FALSE}
};
/* *INDENT-ON* */
#endif

const char *I810vgahwSymbols[] = {
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
   "vgaHWSetMmioFuncs",
   "vgaHWUnlock",
   "vgaHWUnmapMem",
   NULL
};

const char *I810fbSymbols[] = {
   "fbPictureInit",
   "fbScreenInit",
   NULL
};

const char *I810vbeSymbols[] = {
   "VBEFreeModeInfo",
   "VBEFreeVBEInfo",
   "VBEGetModeInfo",
   "VBEGetModePool",
   "VBEGetVBEInfo",
   "VBEGetVBEMode",
   "VBEInit",
   "VBEPrintModes",
   "VBESaveRestore",
   "VBESetDisplayStart",
   "VBESetGetDACPaletteFormat",
   "VBESetGetLogicalScanlineLength",
   "VBESetGetPaletteData",
   "VBESetModeNames",
   "VBESetModeParameters",
   "VBESetVBEMode",
   "VBEValidateModes",
   "vbeDoEDID",
   "vbeFree",
   NULL
};

#ifdef XFree86LOADER
static const char *vbeOptionalSymbols[] = {
   "VBEDPMSSet",
   "VBEGetPixelClock",
   NULL
};
#endif

const char *I810ddcSymbols[] = {
   "xf86PrintEDID",
   "xf86SetDDCproperties",
   NULL
};

const char *I810int10Symbols[] = {
   "xf86ExecX86int10",
   "xf86InitInt10",
   "xf86Int10AllocPages",
   "xf86int10Addr",
   NULL
};

const char *I810xaaSymbols[] = {
   "XAACreateInfoRec",
   "XAADestroyInfoRec",
   "XAAFillSolidRects",
   "XAAInit",
   NULL
};

const char *I810ramdacSymbols[] = {
   "xf86CreateCursorInfoRec",
   "xf86DestroyCursorInfoRec",
   "xf86InitCursor",
   NULL
};

#ifndef I830_ONLY
#ifdef XFree86LOADER
#ifdef XF86DRI
static const char *drmSymbols[] = {
   "drmAddBufs",
   "drmAddMap",
   "drmAgpAcquire",
   "drmAgpAlloc",
   "drmAgpBind",
   "drmAgpEnable",
   "drmAgpFree",
   "drmAgpRelease",
   "drmAuthMagic",
   "drmCommandWrite",
   "drmCreateContext",
   "drmCtlInstHandler",
   "drmDestroyContext",
   "drmFreeVersion",
   "drmGetInterruptFromBusID",
   "drmGetLibVersion",
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
   "DRIQueryVersion",
   "DRIScreenInit",
   "DRIUnlock",
   "GlxSetVisualConfigs",
   NULL
};

#endif
#endif

#ifdef XF86DRI
const char *I810shadowSymbols[] = {
   "shadowInit",
   "shadowSetup",
   "shadowAdd",
   NULL
};
#endif

#endif /* I830_ONLY */

#ifndef I810_DEBUG
int I810_DEBUG = (0
/*     		  | DEBUG_ALWAYS_SYNC  */
/*    		  | DEBUG_VERBOSE_ACCEL */
/*  		  | DEBUG_VERBOSE_SYNC */
/*  		  | DEBUG_VERBOSE_VGA */
/*  		  | DEBUG_VERBOSE_RING */
/*  		  | DEBUG_VERBOSE_OUTREG */
/*  		  | DEBUG_VERBOSE_MEMORY */
/*  		  | DEBUG_VERBOSE_CURSOR */
      );
#endif

#ifndef I830_ONLY
#ifdef XF86DRI
static int i810_pitches[] = {
   512,
   1024,
   2048,
   4096,
   0
};
#endif
#endif

#ifdef XFree86LOADER

static MODULESETUPPROTO(i810Setup);

static XF86ModuleVersionInfo i810VersRec = {
   "i810",
   MODULEVENDORSTRING,
   MODINFOSTRING1,
   MODINFOSTRING2,
   XF86_VERSION_CURRENT,
   I810_MAJOR_VERSION, I810_MINOR_VERSION, I810_PATCHLEVEL,
   ABI_CLASS_VIDEODRV,
   ABI_VIDEODRV_VERSION,
   MOD_CLASS_VIDEODRV,
   {0, 0, 0, 0}
};

XF86ModuleData i810ModuleData = { &i810VersRec, i810Setup, 0 };

static pointer
i810Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
   static Bool setupDone = 0;

   /* This module should be loaded only once, but check to be sure.
    */
   if (!setupDone) {
      setupDone = 1;
      xf86AddDriver(&I810, module, 0);

      /*
       * Tell the loader about symbols from other modules that this module
       * might refer to.
       */
      LoaderRefSymLists(I810vgahwSymbols,
			I810fbSymbols, I810xaaSymbols, I810ramdacSymbols,
#ifdef XF86DRI
			drmSymbols,
			driSymbols,
			I810shadowSymbols,
#endif
			I810vbeSymbols, vbeOptionalSymbols,
			I810ddcSymbols, I810int10Symbols, NULL);

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

#endif

#ifndef I830_ONLY
/*
 * I810GetRec and I810FreeRec --
 *
 * Private data for the driver is stored in the screen structure.
 * These two functions create and destroy that private data.
 *
 */
static Bool
I810GetRec(ScrnInfoPtr pScrn)
{
   if (pScrn->driverPrivate)
      return TRUE;

   pScrn->driverPrivate = xnfcalloc(sizeof(I810Rec), 1);
   return TRUE;
}

static void
I810FreeRec(ScrnInfoPtr pScrn)
{
   if (!pScrn)
      return;
   if (!pScrn->driverPrivate)
      return;
   xfree(pScrn->driverPrivate);
   pScrn->driverPrivate = 0;
}
#endif

/*
 * I810Identify --
 *
 * Returns the string name for the driver based on the chipset. In this
 * case it will always be an I810, so we can return a static string.
 *
 */
static void
I810Identify(int flags)
{
   xf86PrintChipsets(I810_NAME, "Driver for Intel Integrated Graphics Chipsets",
		     I810Chipsets);
}

const OptionInfoRec *
I810AvailableOptions(int chipid, int busid)
{
#ifndef I830_ONLY
   const OptionInfoRec *pOptions;

   if ((pOptions = I830BIOSAvailableOptions(chipid, busid)))
      return pOptions;
   return I810Options;
#else
   return I830BIOSAvailableOptions(chipid, busid);
#endif
}

/*
 * I810Probe --
 *
 * Look through the PCI bus to find cards that are I810 boards.
 * Setup the dispatch table for the rest of the driver functions.
 *
 */
static Bool
I810Probe(DriverPtr drv, int flags)
{
   int i, numUsed, numDevSections, *usedChips;
   GDevPtr *devSections;
   Bool foundScreen = FALSE;
   pciVideoPtr *VideoInfo;
   pciVideoPtr *ppPci;
   PciChipsets *id;

   /*
    * Find the config file Device sections that match this
    * driver, and return if there are none.
    */
   if ((numDevSections =
	xf86MatchDevice(I810_DRIVER_NAME, &devSections)) <= 0) {
      return FALSE;
   }

   /*
    * This probing is just checking the PCI data the server already
    * collected.
    */
   if (!(VideoInfo = xf86GetPciVideoInfo()))
      return FALSE;

   /*
    * Mobile platforms may have both function 0 and 1 active, but they
    * are handled as a single entity.  To make sure that the function 1
    * entity isn't assigned to a screen, check for and claim it here
    * first.
    *
    * XXX If function 1's resources are ever needed, they'll need to be
    * added to the screen and marked active.
    */
   for (ppPci = VideoInfo; ppPci != NULL && *ppPci != NULL; ppPci++) {
      if ((*ppPci)->vendor == PCI_VENDOR_INTEL &&
	  (*ppPci)->func == 1) {
	 for (id = I810PciChipsets; id->PCIid != -1; id++) {
	    if (id->PCIid == (*ppPci)->chipType) {
	       /* Claim slot */
	       if (xf86CheckPciSlot((*ppPci)->bus, (*ppPci)->device,
				    (*ppPci)->func)) {
	  	  xf86ClaimPciSlot((*ppPci)->bus, (*ppPci)->device,
				   (*ppPci)->func, drv, id->PCIid,
				   NULL, FALSE);
	       }
	       break;
	    }
	 }
      }
   }

   /* Look for Intel i8xx devices. */
   numUsed = xf86MatchPciInstances(I810_NAME, PCI_VENDOR_INTEL,
				   I810Chipsets, I810PciChipsets,
				   devSections, numDevSections,
				   drv, &usedChips);

   if (flags & PROBE_DETECT) {
      if (numUsed > 0)
	 foundScreen = TRUE;
   } else {
      for (i = 0; i < numUsed; i++) {
	 ScrnInfoPtr pScrn = NULL;

	 /* Allocate new ScrnInfoRec and claim the slot */
	 if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
					  I810PciChipsets, 0, 0, 0, 0, 0))) {
	    EntityInfoPtr pEnt;

	    pEnt = xf86GetEntityInfo(usedChips[i]);

	    pScrn->driverVersion = I810_VERSION;
	    pScrn->driverName = I810_DRIVER_NAME;
	    pScrn->name = I810_NAME;
	    pScrn->Probe = I810Probe;
	    foundScreen = TRUE;
	    switch (pEnt->chipset) {
	    case PCI_CHIP_I830_M:
	    case PCI_CHIP_845_G:
	    case PCI_CHIP_I855_GM:
	    case PCI_CHIP_I865_G:
	       I830InitpScrn(pScrn);
	       break;
#ifndef I830_ONLY
	    default:
	       pScrn->PreInit = I810PreInit;
	       pScrn->ScreenInit = I810ScreenInit;
	       pScrn->SwitchMode = I810SwitchMode;
	       pScrn->AdjustFrame = I810AdjustFrame;
	       pScrn->EnterVT = I810EnterVT;
	       pScrn->LeaveVT = I810LeaveVT;
	       pScrn->FreeScreen = I810FreeScreen;
	       pScrn->ValidMode = I810ValidMode;
	       break;
#endif
	    }
	 }
      }
   }

   xfree(usedChips);
   xfree(devSections);

   return foundScreen;
}

#ifndef I830_ONLY
static void
I810ProbeDDC(ScrnInfoPtr pScrn, int index)
{
   vbeInfoPtr pVbe;

   if (xf86LoadSubModule(pScrn, "vbe")) {
      pVbe = VBEInit(NULL, index);
      ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
      vbeFree(pVbe);
   }
}

static xf86MonPtr
I810DoDDC(ScrnInfoPtr pScrn, int index)
{
   vbeInfoPtr pVbe;
   xf86MonPtr MonInfo = NULL;
   I810Ptr pI810 = I810PTR(pScrn);

   /* Honour Option "noDDC" */
   if (xf86ReturnOptValBool(pI810->Options, OPTION_NO_DDC, FALSE)) {
      return MonInfo;
   }

   if (xf86LoadSubModule(pScrn, "vbe") && (pVbe = VBEInit(NULL, index))) {
      xf86LoaderReqSymLists(I810vbeSymbols, NULL);
      MonInfo = vbeDoEDID(pVbe, NULL);
      xf86PrintEDID(MonInfo);
      xf86SetDDCproperties(pScrn, MonInfo);
      vbeFree(pVbe);
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "this driver cannot do DDC without VBE\n");
   }

   return MonInfo;
}

/*
 * I810PreInit --
 *
 * Do initial setup of the board before we know what resolution we will
 * be running at.
 *
 */
static Bool
I810PreInit(ScrnInfoPtr pScrn, int flags)
{
   vgaHWPtr hwp;
   I810Ptr pI810;
   ClockRangePtr clockRanges;
   int i;
   MessageType from;
   int flags24;
   rgb defaultWeight = { 0, 0, 0 };
   int mem;

   if (pScrn->numEntities != 1)
      return FALSE;

   /* Allocate driverPrivate */
   if (!I810GetRec(pScrn))
      return FALSE;

   pI810 = I810PTR(pScrn);

   pI810->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
   if (pI810->pEnt->location.type != BUS_PCI)
      return FALSE;

   if (flags & PROBE_DETECT) {
      I810ProbeDDC(pScrn, pI810->pEnt->index);
      return TRUE;
   }

   /* The vgahw module should be loaded here when needed */
   if (!xf86LoadSubModule(pScrn, "vgahw"))
      return FALSE;

   xf86LoaderReqSymLists(I810vgahwSymbols, NULL);

   /* Allocate a vgaHWRec */
   if (!vgaHWGetHWRec(pScrn))
      return FALSE;
   hwp = VGAHWPTR(pScrn);
   pI810->ioBase = hwp->PIOOffset;

   pI810->PciInfo = xf86GetPciInfoForEntity(pI810->pEnt->index);
   pI810->PciTag = pciTag(pI810->PciInfo->bus, pI810->PciInfo->device,
			  pI810->PciInfo->func);

   if (xf86RegisterResources(pI810->pEnt->index, 0, ResNone))
      return FALSE;
   pScrn->racMemFlags = RAC_FB | RAC_COLORMAP;

   /* Set pScrn->monitor */
   pScrn->monitor = pScrn->confScreen->monitor;

   flags24 = Support24bppFb | PreferConvert32to24 | SupportConvert32to24;
   if (!xf86SetDepthBpp(pScrn, 8, 8, 8, flags24)) {
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
		    "Given depth (%d) is not supported by i810 driver\n",
		    pScrn->depth);
	 return FALSE;
      }
   }
   xf86PrintDepthBpp(pScrn);

   switch (pScrn->bitsPerPixel) {
   case 8:
   case 16:
   case 24:
      break;
   default:
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Given bpp (%d) is not supported by i810 driver\n",
		 pScrn->bitsPerPixel);
      return FALSE;
   }

   if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight))
      return FALSE;

   if (!xf86SetDefaultVisual(pScrn, -1))
      return FALSE;

   /* We use a programmable clock */
   pScrn->progClock = TRUE;

   pI810->cpp = pScrn->bitsPerPixel / 8;

   /* Process the options */
   xf86CollectOptions(pScrn, NULL);
   if (!(pI810->Options = xalloc(sizeof(I810Options))))
      return FALSE;
   memcpy(pI810->Options, I810Options, sizeof(I810Options));
   xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pI810->Options);

   pScrn->rgbBits = 8;
   if (xf86ReturnOptValBool(pI810->Options, OPTION_DAC_6BIT, FALSE))
      pScrn->rgbBits = 6;

   if (xf86ReturnOptValBool(pI810->Options, OPTION_SHOW_CACHE, FALSE))
     pI810->showCache = TRUE;
   else
     pI810->showCache = FALSE;

   /* 6-BIT dac isn't reasonable for modes with > 8bpp */
   if (xf86ReturnOptValBool(pI810->Options, OPTION_DAC_6BIT, FALSE) &&
       pScrn->bitsPerPixel > 8) {
      OptionInfoPtr ptr;

      ptr = xf86TokenToOptinfo(pI810->Options, OPTION_DAC_6BIT);
      ptr->found = FALSE;
   }

   /* Get DDC info from monitor */
   /* after xf86ProcessOptions,
    * because it is controlled by options [no]vbe and [no]ddc
    */
   pScrn->monitor->DDC = I810DoDDC(pScrn, pI810->pEnt->index);

   /* We have to use PIO to probe, because we haven't mapped yet */
   I810SetPIOAccess(pI810);

   /*
    * Set the Chipset and ChipRev, allowing config file entries to
    * override.
    */
   if (pI810->pEnt->device->chipset && *pI810->pEnt->device->chipset) {
      pScrn->chipset = pI810->pEnt->device->chipset;
      from = X_CONFIG;
   } else if (pI810->pEnt->device->chipID >= 0) {
      pScrn->chipset = (char *)xf86TokenToString(I810Chipsets,
						 pI810->pEnt->device->chipID);
      from = X_CONFIG;
      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
		 pI810->pEnt->device->chipID);
   } else {
      from = X_PROBED;
      pScrn->chipset = (char *)xf86TokenToString(I810Chipsets,
						 pI810->PciInfo->chipType);
   }
   if (pI810->pEnt->device->chipRev >= 0) {
      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
		 pI810->pEnt->device->chipRev);
   }

   xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n",
	      (pScrn->chipset != NULL) ? pScrn->chipset : "Unknown i810");

   if (pI810->pEnt->device->MemBase != 0) {
      pI810->LinearAddr = pI810->pEnt->device->MemBase;
      from = X_CONFIG;
   } else {
      if (pI810->PciInfo->memBase[1] != 0) {
	 pI810->LinearAddr = pI810->PciInfo->memBase[0] & 0xFF000000;
	 from = X_PROBED;
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "No valid FB address in PCI config space\n");
	 I810FreeRec(pScrn);
	 return FALSE;
      }
   }
   xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	      (unsigned long)pI810->LinearAddr);

   if (pI810->pEnt->device->IOBase != 0) {
      pI810->MMIOAddr = pI810->pEnt->device->IOBase;
      from = X_CONFIG;
   } else {
      if (pI810->PciInfo->memBase[1]) {
	 pI810->MMIOAddr = pI810->PciInfo->memBase[1] & 0xFFF80000;
	 from = X_PROBED;
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "No valid MMIO address in PCI config space\n");
	 I810FreeRec(pScrn);
	 return FALSE;
      }
   }
   xf86DrvMsg(pScrn->scrnIndex, from, "IO registers at addr 0x%lX\n",
	      (unsigned long)pI810->MMIOAddr);

   /* AGP GART support is required.  Don't proceed any further if it isn't
    * present.
    */
   if (!xf86AgpGARTSupported()) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "AGP GART support is not available.  Make sure your kernel has\n"
		 "\tagpgart support or that the agpgart kernel module is loaded.\n");
      return FALSE;
   }

   /* Find out memory bus frequency.
    */
   {
      unsigned long whtcfg_pamr_drp = pciReadLong(pI810->PciTag,
						  WHTCFG_PAMR_DRP);

      /* Need this for choosing watermarks.
       */
      if ((whtcfg_pamr_drp & LM_FREQ_MASK) == LM_FREQ_133)
	 pI810->LmFreqSel = 133;
      else
	 pI810->LmFreqSel = 100;
   }

   /* Default to 4MB framebuffer, which is sufficient for all
    * supported 2d resolutions.  If the user has specified a different
    * size in the XF86Config, use that amount instead.
    *
    *  Changed to 8 Meg so we can have acceleration by default (Mark).
    */
   pScrn->videoRam = 8192;
   from = X_DEFAULT;
   if (pI810->pEnt->device->videoRam) {
      pScrn->videoRam = pI810->pEnt->device->videoRam;
      from = X_CONFIG;
   }

   mem = I810CheckAvailableMemory(pScrn);
   if (mem > 0 && mem < pScrn->videoRam) {
      xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "%dk of memory was requested,"
		 " but the\n\t maximum AGP memory available is %dk.\n",
		 pScrn->videoRam, mem);
      from = X_PROBED;
      if (mem > (6 * 1024)) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Reducing video memory to 4MB\n");
	 pScrn->videoRam = 4096;
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Less than 6MB of AGP memory"
		    "is available. Cannot proceed.\n");
	 I810FreeRec(pScrn);
	 return FALSE;
      }
   }

   xf86DrvMsg(pScrn->scrnIndex, from,
	      "Will alloc AGP framebuffer: %d kByte\n", pScrn->videoRam);

   /* Calculate Fixed Offsets depending on graphics aperture size */
   {
      PCITAG bridge;
      long smram_miscc;

      bridge = pciTag(0, 0, 0);		/* This is always the host bridge */
      smram_miscc = pciReadLong(bridge, SMRAM_MISCC);
      if ((smram_miscc & GFX_MEM_WIN_SIZE) == GFX_MEM_WIN_32M) {
	 pI810->FbMapSize = 0x1000000;
	 pI810->DepthOffset = 0x1000000;
	 pI810->BackOffset = 0x1800000;
      } else {
	 pI810->FbMapSize = 0x3000000;
	 pI810->DepthOffset = 0x3000000;
	 pI810->BackOffset = 0x3800000;
      }
   }

   /*
    * If the driver can do gamma correction, it should call xf86SetGamma()
    * here.
    */
   {
      Gamma zeros = { 0.0, 0.0, 0.0 };

      if (!xf86SetGamma(pScrn, zeros)) {
	 return FALSE;
      }
   }

   pI810->MaxClock = 0;
   if (pI810->pEnt->device->dacSpeeds[0]) {
      switch (pScrn->bitsPerPixel) {
      case 8:
	 pI810->MaxClock = pI810->pEnt->device->dacSpeeds[DAC_BPP8];
	 break;
      case 16:
	 pI810->MaxClock = pI810->pEnt->device->dacSpeeds[DAC_BPP16];
	 break;
      case 24:
	 pI810->MaxClock = pI810->pEnt->device->dacSpeeds[DAC_BPP24];
	 break;
      case 32:				/* not supported */
	 pI810->MaxClock = pI810->pEnt->device->dacSpeeds[DAC_BPP32];
	 break;
      }
      if (!pI810->MaxClock)
	 pI810->MaxClock = pI810->pEnt->device->dacSpeeds[0];
      from = X_CONFIG;
   } else {
      switch (pScrn->bitsPerPixel) {
      case 8:
	 pI810->MaxClock = 203000;
	 break;
      case 16:
	 pI810->MaxClock = 163000;
	 break;
      case 24:
	 pI810->MaxClock = 136000;
	 break;
      case 32:				/* not supported */
	 pI810->MaxClock = 86000;
      }
   }
   clockRanges = xnfcalloc(sizeof(ClockRange), 1);
   clockRanges->next = NULL;
   clockRanges->minClock = 12000;	/* !!! What's the min clock? !!! */
   clockRanges->maxClock = pI810->MaxClock;
   clockRanges->clockIndex = -1;
   clockRanges->interlaceAllowed = TRUE;
   clockRanges->doubleScanAllowed = FALSE;

   i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			 pScrn->display->modes, clockRanges,
#ifndef XF86DRI
			 0, 320, 1600, 64 * pScrn->bitsPerPixel,
#else
			 i810_pitches, 0, 0, 64 * pScrn->bitsPerPixel,
#endif
			 200, 1200,
			 pScrn->display->virtualX, pScrn->display->virtualY,
			 pScrn->videoRam * 1024, LOOKUP_BEST_REFRESH);

   if (i == -1) {
      I810FreeRec(pScrn);
      return FALSE;
   }

   xf86PruneDriverModes(pScrn);

   if (!i || !pScrn->modes) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
      I810FreeRec(pScrn);
      return FALSE;
   }

   xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

   pScrn->currentMode = pScrn->modes;

   xf86PrintModes(pScrn);

   xf86SetDpi(pScrn, 0, 0);

   if (!xf86LoadSubModule(pScrn, "fb")) {
      I810FreeRec(pScrn);
      return FALSE;
   }
   xf86LoaderReqSymLists(I810fbSymbols, NULL);

   if (xf86ReturnOptValBool(pI810->Options, OPTION_NOACCEL, FALSE))
      pI810->noAccel = TRUE;

   if (!pI810->noAccel) {
      if (!xf86LoadSubModule(pScrn, "xaa")) {
	 I810FreeRec(pScrn);
	 return FALSE;
      }
      xf86LoaderReqSymLists(I810xaaSymbols, NULL);
   }

   if (!xf86ReturnOptValBool(pI810->Options, OPTION_SW_CURSOR, FALSE)) {
      if (!xf86LoadSubModule(pScrn, "ramdac")) {
	 I810FreeRec(pScrn);
	 return FALSE;
      }
      xf86LoaderReqSymLists(I810ramdacSymbols, NULL);
   }

   if (xf86GetOptValInteger
       (pI810->Options, OPTION_COLOR_KEY, &(pI810->colorKey))) {
      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		 "video overlay key set to 0x%x\n", pI810->colorKey);
   } else {
      pI810->colorKey = (1 << pScrn->offset.red) |
	    (1 << pScrn->offset.green) |
	    (((pScrn->mask.blue >> pScrn->offset.blue) -
	      1) << pScrn->offset.blue);
   }

   if (xf86GetOptValInteger(pI810->Options, OPTION_XVMC_SURFACES,
			    &(pI810->numSurfaces))) {
      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "%d XvMC Surfaces Requested.\n",
		 pI810->numSurfaces);
      if (pI810->numSurfaces > 7) {
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		    "Using 7 XvMC Surfaces (Maximum Allowed).\n");
	 pI810->numSurfaces = 7;
      }
      if (pI810->numSurfaces < 6) {
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		    "Using 6 XvMC Surfaces (Minimum Allowed).\n");
	 pI810->numSurfaces = 6;
      }
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "XvMC is Disabled: use XvMCSurfaces config option to enable.\n");
      pI810->numSurfaces = 0;
   }

   /*  We won't be using the VGA access after the probe */
   I810SetMMIOAccess(pI810);
   xf86SetOperatingState(resVgaIo, pI810->pEnt->index, ResUnusedOpr);
   xf86SetOperatingState(resVgaMem, pI810->pEnt->index, ResDisableOpr);

   return TRUE;
}

static Bool
I810MapMMIO(ScrnInfoPtr pScrn)
{
   int mmioFlags;
   I810Ptr pI810 = I810PTR(pScrn);

#if !defined(__alpha__)
   mmioFlags = VIDMEM_MMIO | VIDMEM_READSIDEEFFECT;
#else
   mmioFlags = VIDMEM_MMIO | VIDMEM_READSIDEEFFECT | VIDMEM_SPARSE;
#endif

   pI810->MMIOBase = xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
				   pI810->PciTag,
				   pI810->MMIOAddr, I810_REG_SIZE);
   if (!pI810->MMIOBase)
      return FALSE;
   return TRUE;
}

static Bool
I810MapMem(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);
   unsigned i;

   for (i = 2; i < pI810->FbMapSize; i <<= 1) ;
   pI810->FbMapSize = i;

   if (!I810MapMMIO(pScrn))
      return FALSE;

   pI810->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				 pI810->PciTag,
				 pI810->LinearAddr, pI810->FbMapSize);
   if (!pI810->FbBase)
      return FALSE;

   pI810->LpRing.virtual_start = pI810->FbBase + pI810->LpRing.mem.Start;

   return TRUE;
}

static void
I810UnmapMMIO(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);

   xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pI810->MMIOBase,
		   I810_REG_SIZE);
   pI810->MMIOBase = 0;
}

static Bool
I810UnmapMem(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);

   xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pI810->FbBase,
		   pI810->FbMapSize);
   pI810->FbBase = 0;
   I810UnmapMMIO(pScrn);
   return TRUE;
}

/* Famous last words
 */
void
I810PrintErrorState(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);

   ErrorF("pgetbl_ctl: 0x%lx pgetbl_err: 0x%lx\n",
	  INREG(PGETBL_CTL), INREG(PGE_ERR));

   ErrorF("ipeir: %lx iphdr: %lx\n", INREG(IPEIR), INREG(IPEHR));

   ErrorF("LP ring tail: %lx head: %lx len: %lx start %lx\n",
	  INREG(LP_RING + RING_TAIL),
	  INREG(LP_RING + RING_HEAD) & HEAD_ADDR,
	  INREG(LP_RING + RING_LEN), INREG(LP_RING + RING_START));

   ErrorF("eir: %x esr: %x emr: %x\n",
	  INREG16(EIR), INREG16(ESR), INREG16(EMR));

   ErrorF("instdone: %x instpm: %x\n", INREG16(INST_DONE), INREG8(INST_PM));

   ErrorF("memmode: %lx instps: %lx\n", INREG(MEMMODE), INREG(INST_PS));

   ErrorF("hwstam: %x ier: %x imr: %x iir: %x\n",
	  INREG16(HWSTAM), INREG16(IER), INREG16(IMR), INREG16(IIR));
}

/*
 * I810Save --
 *
 * This function saves the video state.  It reads all of the SVGA registers
 * into the vgaI810Rec data structure.  There is in general no need to
 * mask out bits here - just read the registers.
 */
static void
DoSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, I810RegPtr i810Reg,
       Bool saveFonts)
{
   I810Ptr pI810;
   vgaHWPtr hwp;
   int i;

   pI810 = I810PTR(pScrn);
   hwp = VGAHWPTR(pScrn);

   /*
    * This function will handle creating the data structure and filling
    * in the generic VGA portion.
    */
   if (saveFonts)
      vgaHWSave(pScrn, vgaReg, VGA_SR_MODE | VGA_SR_FONTS | VGA_SR_CMAP);
   else
      vgaHWSave(pScrn, vgaReg, VGA_SR_MODE | VGA_SR_CMAP);

   /*
    * The port I/O code necessary to read in the extended registers
    * into the fields of the vgaI810Rec structure goes here.
    */
   i810Reg->IOControl = hwp->readCrtc(hwp, IO_CTNL);
   i810Reg->AddressMapping = pI810->readControl(pI810, GRX, ADDRESS_MAPPING);
   i810Reg->BitBLTControl = INREG8(BITBLT_CNTL);
   i810Reg->VideoClk2_M = INREG16(VCLK2_VCO_M);
   i810Reg->VideoClk2_N = INREG16(VCLK2_VCO_N);
   i810Reg->VideoClk2_DivisorSel = INREG8(VCLK2_VCO_DIV_SEL);

   i810Reg->ExtVertTotal = hwp->readCrtc(hwp, EXT_VERT_TOTAL);
   i810Reg->ExtVertDispEnd = hwp->readCrtc(hwp, EXT_VERT_DISPLAY);
   i810Reg->ExtVertSyncStart = hwp->readCrtc(hwp, EXT_VERT_SYNC_START);
   i810Reg->ExtVertBlankStart = hwp->readCrtc(hwp, EXT_VERT_BLANK_START);
   i810Reg->ExtHorizTotal = hwp->readCrtc(hwp, EXT_HORIZ_TOTAL);
   i810Reg->ExtHorizBlank = hwp->readCrtc(hwp, EXT_HORIZ_BLANK);
   i810Reg->ExtOffset = hwp->readCrtc(hwp, EXT_OFFSET);
   i810Reg->InterlaceControl = hwp->readCrtc(hwp, INTERLACE_CNTL);

   i810Reg->PixelPipeCfg0 = INREG8(PIXPIPE_CONFIG_0);
   i810Reg->PixelPipeCfg1 = INREG8(PIXPIPE_CONFIG_1);
   i810Reg->PixelPipeCfg2 = INREG8(PIXPIPE_CONFIG_2);
   i810Reg->DisplayControl = INREG8(DISPLAY_CNTL);
   i810Reg->LMI_FIFO_Watermark = INREG(FWATER_BLC);

   for (i = 0; i < 8; i++)
      i810Reg->Fence[i] = INREG(FENCE + i * 4);

   i810Reg->LprbTail = INREG(LP_RING + RING_TAIL);
   i810Reg->LprbHead = INREG(LP_RING + RING_HEAD);
   i810Reg->LprbStart = INREG(LP_RING + RING_START);
   i810Reg->LprbLen = INREG(LP_RING + RING_LEN);

   if ((i810Reg->LprbTail & TAIL_ADDR) != (i810Reg->LprbHead & HEAD_ADDR) &&
       i810Reg->LprbLen & RING_VALID) {
      I810PrintErrorState(pScrn);
      FatalError("Active ring not flushed\n");
   }
}

static void
I810Save(ScrnInfoPtr pScrn)
{
   vgaHWPtr hwp;
   I810Ptr pI810;
   CARD32 temp;

   hwp = VGAHWPTR(pScrn);
   pI810 = I810PTR(pScrn);
   DoSave(pScrn, &hwp->SavedReg, &pI810->SavedReg, TRUE);

   temp = INREG(MEMMODE);
   temp |= 4;
   OUTREG(MEMMODE, temp);
}

static void
i810PrintMode(vgaRegPtr vgaReg, I810RegPtr mode)
{
   int i;

   ErrorF("   MiscOut: %x\n", vgaReg->MiscOutReg);

   ErrorF("SEQ: ");
   for (i = 0; i < vgaReg->numSequencer; i++) {
      if ((i & 7) == 0)
	 ErrorF("\n");
      ErrorF("   %d: %x", i, vgaReg->Sequencer[i]);
   }
   ErrorF("\n");

   ErrorF("CRTC: ");
   for (i = 0; i < vgaReg->numCRTC; i++) {
      if ((i & 3) == 0)
	 ErrorF("\n");
      ErrorF("   %d: %x", i, vgaReg->CRTC[i]);
   }
   ErrorF("\n");

   ErrorF("GFX: ");
   for (i = 0; i < vgaReg->numGraphics; i++) {
      if ((i & 7) == 0)
	 ErrorF("\n");
      ErrorF("   %d: %x", i, vgaReg->Graphics[i]);
   }
   ErrorF("\n");

   ErrorF("ATTR: ");
   for (i = 0; i < vgaReg->numAttribute; i++) {
      if ((i & 7) == 0)
	 ErrorF("\n");
      ErrorF("   %d: %x", i, vgaReg->Attribute[i]);
   }
   ErrorF("\n");

   ErrorF("   DisplayControl: %x\n", mode->DisplayControl);
   ErrorF("   PixelPipeCfg0: %x\n", mode->PixelPipeCfg0);
   ErrorF("   PixelPipeCfg1: %x\n", mode->PixelPipeCfg1);
   ErrorF("   PixelPipeCfg2: %x\n", mode->PixelPipeCfg2);
   ErrorF("   VideoClk2_M: %x\n", mode->VideoClk2_M);
   ErrorF("   VideoClk2_N: %x\n", mode->VideoClk2_N);
   ErrorF("   VideoClk2_DivisorSel: %x\n", mode->VideoClk2_DivisorSel);
   ErrorF("   AddressMapping: %x\n", mode->AddressMapping);
   ErrorF("   IOControl: %x\n", mode->IOControl);
   ErrorF("   BitBLTControl: %x\n", mode->BitBLTControl);
   ErrorF("   ExtVertTotal: %x\n", mode->ExtVertTotal);
   ErrorF("   ExtVertDispEnd: %x\n", mode->ExtVertDispEnd);
   ErrorF("   ExtVertSyncStart: %x\n", mode->ExtVertSyncStart);
   ErrorF("   ExtVertBlankStart: %x\n", mode->ExtVertBlankStart);
   ErrorF("   ExtHorizTotal: %x\n", mode->ExtHorizTotal);
   ErrorF("   ExtHorizBlank: %x\n", mode->ExtHorizBlank);
   ErrorF("   ExtOffset: %x\n", mode->ExtOffset);
   ErrorF("   InterlaceControl: %x\n", mode->InterlaceControl);
   ErrorF("   LMI_FIFO_Watermark: %x\n", mode->LMI_FIFO_Watermark);
   ErrorF("   LprbTail: %x\n", mode->LprbTail);
   ErrorF("   LprbHead: %x\n", mode->LprbHead);
   ErrorF("   LprbStart: %x\n", mode->LprbStart);
   ErrorF("   LprbLen: %x\n", mode->LprbLen);
}

static void
DoRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, I810RegPtr i810Reg,
	  Bool restoreFonts)
{
   I810Ptr pI810;
   vgaHWPtr hwp;
   unsigned char temp;
   unsigned int itemp;
   int i;

   pI810 = I810PTR(pScrn);
   hwp = VGAHWPTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_VGA) {
      ErrorF("Setting mode in I810Restore:\n");
      i810PrintMode(vgaReg, i810Reg);
   }

   vgaHWProtect(pScrn, TRUE);

   usleep(50000);

   /* Turn off DRAM Refresh */
   temp = INREG8(DRAM_ROW_CNTL_HI);
   temp &= ~DRAM_REFRESH_RATE;
   temp |= DRAM_REFRESH_DISABLE;
   OUTREG8(DRAM_ROW_CNTL_HI, temp);

   usleep(1000);			/* Wait 1 ms */

   /* Write the M, N and P values */
   OUTREG16(VCLK2_VCO_M, i810Reg->VideoClk2_M);
   OUTREG16(VCLK2_VCO_N, i810Reg->VideoClk2_N);
   OUTREG8(VCLK2_VCO_DIV_SEL, i810Reg->VideoClk2_DivisorSel);

   /*
    * Turn on 8 bit dac mode, if requested.  This is needed to make
    * sure that vgaHWRestore writes the values into the DAC properly.
    * The problem occurs if 8 bit dac mode is requested and the HW is
    * in 6 bit dac mode.  If this happens, all the values are
    * automatically shifted left twice by the HW and incorrect colors
    * will be displayed on the screen.  The only time this can happen
    * is at server startup time and when switching back from a VT.
    */
   temp = INREG8(PIXPIPE_CONFIG_0);
   temp &= 0x7F;			/* Save all but the 8 bit dac mode bit */
   temp |= (i810Reg->PixelPipeCfg0 & DAC_8_BIT);
   OUTREG8(PIXPIPE_CONFIG_0, temp);

   /*
    * Code to restore any SVGA registers that have been saved/modified
    * goes here.  Note that it is allowable, and often correct, to
    * only modify certain bits in a register by a read/modify/write cycle.
    *
    * A special case - when using an external clock-setting program,
    * this function must not change bits associated with the clock
    * selection.  This condition can be checked by the condition:
    *
    *   if (i810Reg->std.NoClock >= 0)
    *           restore clock-select bits.
    */
   if (restoreFonts)
      vgaHWRestore(pScrn, vgaReg, VGA_SR_FONTS | VGA_SR_MODE | VGA_SR_CMAP);
   else
      vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE | VGA_SR_CMAP);

   hwp->writeCrtc(hwp, EXT_VERT_TOTAL, i810Reg->ExtVertTotal);
   hwp->writeCrtc(hwp, EXT_VERT_DISPLAY, i810Reg->ExtVertDispEnd);
   hwp->writeCrtc(hwp, EXT_VERT_SYNC_START, i810Reg->ExtVertSyncStart);
   hwp->writeCrtc(hwp, EXT_VERT_BLANK_START, i810Reg->ExtVertBlankStart);
   hwp->writeCrtc(hwp, EXT_HORIZ_TOTAL, i810Reg->ExtHorizTotal);
   hwp->writeCrtc(hwp, EXT_HORIZ_BLANK, i810Reg->ExtHorizBlank);
   hwp->writeCrtc(hwp, EXT_OFFSET, i810Reg->ExtOffset);

   temp = hwp->readCrtc(hwp, INTERLACE_CNTL);
   temp &= ~INTERLACE_ENABLE;
   temp |= i810Reg->InterlaceControl;
   hwp->writeCrtc(hwp, INTERLACE_CNTL, temp);

   temp = pI810->readControl(pI810, GRX, ADDRESS_MAPPING);
   temp &= 0xE0;			/* Save reserved bits 7:5 */
   temp |= i810Reg->AddressMapping;
   pI810->writeControl(pI810, GRX, ADDRESS_MAPPING, temp);

   /* Setting the OVRACT Register for video overlay */
   {
       CARD32 LCD_TV_Control = INREG(LCD_TV_C);
       
       if(!(LCD_TV_Control & LCD_TV_ENABLE)
	  || (LCD_TV_Control & LCD_TV_VGAMOD)) {
	   OUTREG(LCD_TV_OVRACT,
		  (i810Reg->OverlayActiveEnd << 16)
		  | i810Reg->OverlayActiveStart);
       }
   }
   
   /* Turn on DRAM Refresh */
   temp = INREG8(DRAM_ROW_CNTL_HI);
   temp &= ~DRAM_REFRESH_RATE;
   temp |= DRAM_REFRESH_60HZ;
   OUTREG8(DRAM_ROW_CNTL_HI, temp);

   temp = INREG8(BITBLT_CNTL);
   temp &= ~COLEXP_MODE;
   temp |= i810Reg->BitBLTControl;
   OUTREG8(BITBLT_CNTL, temp);

   temp = INREG8(DISPLAY_CNTL);
   temp &= ~(VGA_WRAP_MODE | GUI_MODE);
   temp |= i810Reg->DisplayControl;
   OUTREG8(DISPLAY_CNTL, temp);

   temp = INREG8(PIXPIPE_CONFIG_0);
   temp &= 0x64;			/* Save reserved bits 6:5,2 */
   temp |= i810Reg->PixelPipeCfg0;
   OUTREG8(PIXPIPE_CONFIG_0, temp);

   temp = INREG8(PIXPIPE_CONFIG_2);
   temp &= 0xF3;			/* Save reserved bits 7:4,1:0 */
   temp |= i810Reg->PixelPipeCfg2;
   OUTREG8(PIXPIPE_CONFIG_2, temp);

   temp = INREG8(PIXPIPE_CONFIG_1);
   temp &= ~DISPLAY_COLOR_MODE;
   temp &= 0xEF;			/* Restore the CRT control bit */
   temp |= i810Reg->PixelPipeCfg1;
   OUTREG8(PIXPIPE_CONFIG_1, temp);

   OUTREG16(EIR, 0);

   itemp = INREG(FWATER_BLC);
   itemp &= ~(LM_BURST_LENGTH | LM_FIFO_WATERMARK |
	      MM_BURST_LENGTH | MM_FIFO_WATERMARK);
   itemp |= i810Reg->LMI_FIFO_Watermark;
   OUTREG(FWATER_BLC, itemp);

   for (i = 0; i < 8; i++) {
      OUTREG(FENCE + i * 4, i810Reg->Fence[i]);
      if (I810_DEBUG & DEBUG_VERBOSE_VGA)
	 ErrorF("Fence Register : %x\n", i810Reg->Fence[i]);
   }

   /* First disable the ring buffer (Need to wait for empty first?, if so
    * should probably do it before entering this section)
    */
   itemp = INREG(LP_RING + RING_LEN);
   itemp &= ~RING_VALID_MASK;
   OUTREG(LP_RING + RING_LEN, itemp);

   /* Set up the low priority ring buffer.
    */
   OUTREG(LP_RING + RING_TAIL, 0);
   OUTREG(LP_RING + RING_HEAD, 0);

   pI810->LpRing.head = 0;
   pI810->LpRing.tail = 0;

   itemp = INREG(LP_RING + RING_START);
   itemp &= ~(START_ADDR);
   itemp |= i810Reg->LprbStart;
   OUTREG(LP_RING + RING_START, itemp);

   itemp = INREG(LP_RING + RING_LEN);
   itemp &= ~(RING_NR_PAGES | RING_REPORT_MASK | RING_VALID_MASK);
   itemp |= i810Reg->LprbLen;
   OUTREG(LP_RING + RING_LEN, itemp);

   if (!(vgaReg->Attribute[0x10] & 0x1)) {
      usleep(50000);
      if (restoreFonts)
	 vgaHWRestore(pScrn, vgaReg,
		      VGA_SR_FONTS | VGA_SR_MODE | VGA_SR_CMAP);
      else
	 vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE | VGA_SR_CMAP);
   }

   vgaHWProtect(pScrn, FALSE);

   temp = hwp->readCrtc(hwp, IO_CTNL);
   temp &= ~(EXTENDED_ATTR_CNTL | EXTENDED_CRTC_CNTL);
   temp |= i810Reg->IOControl;
   hwp->writeCrtc(hwp, IO_CTNL, temp);
}

static void
I810SetRingRegs(ScrnInfoPtr pScrn)
{
   unsigned int itemp;
   I810Ptr pI810 = I810PTR(pScrn);

   OUTREG(LP_RING + RING_TAIL, 0);
   OUTREG(LP_RING + RING_HEAD, 0);

   itemp = INREG(LP_RING + RING_START);
   itemp &= ~(START_ADDR);
   itemp |= pI810->LpRing.mem.Start;
   OUTREG(LP_RING + RING_START, itemp);

   itemp = INREG(LP_RING + RING_LEN);
   itemp &= ~(RING_NR_PAGES | RING_REPORT_MASK | RING_VALID_MASK);
   itemp |= ((pI810->LpRing.mem.Size - 4096) | RING_NO_REPORT | RING_VALID);
   OUTREG(LP_RING + RING_LEN, itemp);
}

static void
I810Restore(ScrnInfoPtr pScrn)
{
   vgaHWPtr hwp;
   I810Ptr pI810;

   hwp = VGAHWPTR(pScrn);
   pI810 = I810PTR(pScrn);

   DoRestore(pScrn, &hwp->SavedReg, &pI810->SavedReg, TRUE);
}

/*
 * I810CalcVCLK --
 *
 * Determine the closest clock frequency to the one requested.
 */

#define MAX_VCO_FREQ 600.0
#define TARGET_MAX_N 30
#define REF_FREQ 24.0

#define CALC_VCLK(m,n,p) \
    (double)m / ((double)n * (1 << p)) * 4 * REF_FREQ

static void
I810CalcVCLK(ScrnInfoPtr pScrn, double freq)
{
   I810Ptr pI810 = I810PTR(pScrn);
   I810RegPtr i810Reg = &pI810->ModeReg;
   int m, n, p;
   double f_out, f_best;
   double f_err;
   double f_vco;
   int m_best = 0, n_best = 0, p_best = 0;
   double f_target = freq;
   double err_max = 0.005;
   double err_target = 0.001;
   double err_best = 999999.0;

   p_best = p = log(MAX_VCO_FREQ / f_target) / log((double)2);
   f_vco = f_target * (1 << p);

   n = 2;
   do {
      n++;
      m = f_vco / (REF_FREQ / (double)n) / (double)4.0 + 0.5;
      if (m < 3)
	 m = 3;
      f_out = CALC_VCLK(m, n, p);
      f_err = 1.0 - (f_target / f_out);
      if (fabs(f_err) < err_max) {
	 m_best = m;
	 n_best = n;
	 f_best = f_out;
	 err_best = f_err;
      }
   } while ((fabs(f_err) >= err_target) &&
	    ((n <= TARGET_MAX_N) || (fabs(err_best) > err_max)));

   if (fabs(f_err) < err_target) {
      m_best = m;
      n_best = n;
   }

   i810Reg->VideoClk2_M = (m_best - 2) & 0x3FF;
   i810Reg->VideoClk2_N = (n_best - 2) & 0x3FF;
   i810Reg->VideoClk2_DivisorSel = (p_best << 4);

   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		  "Setting dot clock to %.1lf MHz " "[ 0x%x 0x%x 0x%x ] "
		  "[ %d %d %d ]\n", CALC_VCLK(m_best, n_best, p_best),
		  i810Reg->VideoClk2_M, i810Reg->VideoClk2_N,
		  i810Reg->VideoClk2_DivisorSel, m_best, n_best, p_best);
}

static Bool
I810SetMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   I810Ptr pI810 = I810PTR(pScrn);
   I810RegPtr i810Reg = &pI810->ModeReg;
   vgaRegPtr pVga = &VGAHWPTR(pScrn)->ModeReg;
   double dclk = mode->Clock / 1000.0;

   switch (pScrn->bitsPerPixel) {
   case 8:
      pVga->CRTC[0x13] = pScrn->displayWidth >> 3;
      i810Reg->ExtOffset = pScrn->displayWidth >> 11;
      i810Reg->PixelPipeCfg1 = DISPLAY_8BPP_MODE;
      i810Reg->BitBLTControl = COLEXP_8BPP;
      break;
   case 16:
      if (pScrn->weight.green == 5) {
	 i810Reg->PixelPipeCfg1 = DISPLAY_15BPP_MODE;
      } else {
	 i810Reg->PixelPipeCfg1 = DISPLAY_16BPP_MODE;
      }
      pVga->CRTC[0x13] = pScrn->displayWidth >> 2;
      i810Reg->ExtOffset = pScrn->displayWidth >> 10;
      i810Reg->BitBLTControl = COLEXP_16BPP;

      /* Enable Palette Programming for Direct Color visuals. -jens */
      i810Reg->PixelPipeCfg2 = DISPLAY_GAMMA_ENABLE;
      break;
   case 24:
      pVga->CRTC[0x13] = (pScrn->displayWidth * 3) >> 3;
      i810Reg->ExtOffset = (pScrn->displayWidth * 3) >> 11;

      i810Reg->PixelPipeCfg1 = DISPLAY_24BPP_MODE;
      i810Reg->BitBLTControl = COLEXP_24BPP;

      /* Enable Palette Programming for Direct Color visuals. -jens */
      i810Reg->PixelPipeCfg2 = DISPLAY_GAMMA_ENABLE;
      break;
   default:
      break;
   }

   /* Turn on 8 bit dac if requested */
   if (xf86ReturnOptValBool(pI810->Options, OPTION_DAC_6BIT, FALSE))
      i810Reg->PixelPipeCfg0 = DAC_6_BIT;
   else
      i810Reg->PixelPipeCfg0 = DAC_8_BIT;

   /* Do not delay CRT Blank: needed for video overlay */
   i810Reg->PixelPipeCfg1 |= 0x10;

   /* Turn on Extended VGA Interpretation */
   i810Reg->IOControl = EXTENDED_CRTC_CNTL;

   /* Turn on linear and page mapping */
   i810Reg->AddressMapping = (LINEAR_MODE_ENABLE | GTT_MEM_MAP_ENABLE);

   /* Turn on GUI mode */
   i810Reg->DisplayControl = HIRES_MODE;

   /* Calculate the extended CRTC regs */
   i810Reg->ExtVertTotal = (mode->CrtcVTotal - 2) >> 8;
   i810Reg->ExtVertDispEnd = (mode->CrtcVDisplay - 1) >> 8;
   i810Reg->ExtVertSyncStart = mode->CrtcVSyncStart >> 8;
   i810Reg->ExtVertBlankStart = mode->CrtcVBlankStart >> 8;
   i810Reg->ExtHorizTotal = ((mode->CrtcHTotal >> 3) - 5) >> 8;
   i810Reg->ExtHorizBlank = (((mode->CrtcHBlankEnd >> 3) - 1) & 0x40) >> 6;

   /*
    * The following workarounds are needed to get video overlay working
    * at 1024x768 and 1280x1024 display resolutions.
    */
   if ((mode->CrtcVDisplay == 768) && (i810Reg->ExtVertBlankStart == 3)) {
      i810Reg->ExtVertBlankStart = 2;
   }
   if ((mode->CrtcVDisplay == 1024) && (i810Reg->ExtVertBlankStart == 4)) {
      i810Reg->ExtVertBlankStart = 3;
   }

   /* OVRACT Register */
   i810Reg->OverlayActiveStart = mode->CrtcHTotal - 32;
   i810Reg->OverlayActiveEnd = mode->CrtcHDisplay - 32;

   /* Turn on interlaced mode if necessary */
   if (mode->Flags & V_INTERLACE)
      i810Reg->InterlaceControl = INTERLACE_ENABLE;
   else
      i810Reg->InterlaceControl = INTERLACE_DISABLE;

   /*
    * Set the overscan color to 0.
    * NOTE: This only affects >8bpp mode.
    */
   pVga->Attribute[0x11] = 0;

   /*
    * Calculate the VCLK that most closely matches the requested dot
    * clock.
    */
   I810CalcVCLK(pScrn, dclk);

   /* Since we program the clocks ourselves, always use VCLK2. */
   pVga->MiscOutReg |= 0x0C;

   /* Calculate the FIFO Watermark and Burst Length. */
   i810Reg->LMI_FIFO_Watermark = I810CalcWatermark(pScrn, dclk, FALSE);

   /* Setup the ring buffer */
   i810Reg->LprbTail = 0;
   i810Reg->LprbHead = 0;
   i810Reg->LprbStart = pI810->LpRing.mem.Start;

   if (i810Reg->LprbStart)
      i810Reg->LprbLen = ((pI810->LpRing.mem.Size - 4096) |
			  RING_NO_REPORT | RING_VALID);
   else
      i810Reg->LprbLen = RING_INVALID;

   return TRUE;
}

static Bool
I810ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   vgaHWPtr hwp;
   I810Ptr pI810;
   vgaRegPtr pVga;

   hwp = VGAHWPTR(pScrn);
   pI810 = I810PTR(pScrn);

   vgaHWUnlock(hwp);

   if (!vgaHWInit(pScrn, mode))
      return FALSE;
   /*
    * the KGA fix in vgaHW.c results in the first
    * scanline and the first character clock (8 pixels)
    * of each scanline thereafter on display with an i810
    * to be blank. Restoring CRTC 3, 5, & 22 to their
    * "theoretical" values corrects the problem. KAO.
    */
   pVga = &VGAHWPTR(pScrn)->ModeReg;
   pVga->CRTC[3] = (((mode->CrtcHBlankEnd >> 3) - 1) & 0x1F) | 0x80;
   pVga->CRTC[5] = ((((mode->CrtcHBlankEnd >> 3) - 1) & 0x20) << 2)
	 | (((mode->CrtcHSyncEnd >> 3)) & 0x1F);
   pVga->CRTC[22] = (mode->CrtcVBlankEnd - 1) & 0xFF;

   pScrn->vtSema = TRUE;

   if (!I810SetMode(pScrn, mode))
      return FALSE;

#ifdef XF86DRI
   if (pI810->directRenderingEnabled) {
      DRILock(screenInfo.screens[pScrn->scrnIndex], 0);
      pI810->LockHeld = 1;
   }
#endif

   DoRestore(pScrn, &hwp->ModeReg, &pI810->ModeReg, FALSE);

#ifdef XF86DRI
   if (pI810->directRenderingEnabled) {
      DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
      pI810->LockHeld = 0;
   }
#endif

   return TRUE;
}

static void
I810LoadPalette15(ScrnInfoPtr pScrn, int numColors, int *indices,
		  LOCO * colors, VisualPtr pVisual)
{
   I810Ptr pI810;
   vgaHWPtr hwp;
   int i, j, index;
   unsigned char r, g, b;

   pI810 = I810PTR(pScrn);
   hwp = VGAHWPTR(pScrn);

   for (i = 0; i < numColors; i++) {
      index = indices[i];
      r = colors[index].red;
      g = colors[index].green;
      b = colors[index].blue;
      for (j = 0; j < 8; j++) {
	 hwp->writeDacWriteAddr(hwp, (index << 3) + j);
	 hwp->writeDacData(hwp, r);
	 hwp->writeDacData(hwp, g);
	 hwp->writeDacData(hwp, b);
      }
   }
}

static void
I810LoadPalette16(ScrnInfoPtr pScrn, int numColors, int *indices,
		  LOCO * colors, VisualPtr pVisual)
{
   I810Ptr pI810;
   vgaHWPtr hwp;
   int i, index;
   unsigned char r, g, b;

   pI810 = I810PTR(pScrn);
   hwp = VGAHWPTR(pScrn);

   /* Load all four entries in each of the 64 color ranges.  -jens */
   for (i = 0; i < numColors; i++) {
      index = indices[i / 2];
      r = colors[index].red;
      b = colors[index].blue;
      index = indices[i];
      g = colors[index].green;

      hwp->writeDacWriteAddr(hwp, index << 2);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      hwp->writeDacWriteAddr(hwp, (index << 2) + 1);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      hwp->writeDacWriteAddr(hwp, (index << 2) + 2);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      hwp->writeDacWriteAddr(hwp, (index << 2) + 3);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      i++;
      index = indices[i];
      g = colors[index].green;

      hwp->writeDacWriteAddr(hwp, index << 2);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      hwp->writeDacWriteAddr(hwp, (index << 2) + 1);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      hwp->writeDacWriteAddr(hwp, (index << 2) + 2);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);

      hwp->writeDacWriteAddr(hwp, (index << 2) + 3);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);
   }
}

static void
I810LoadPalette24(ScrnInfoPtr pScrn, int numColors, int *indices,
		  LOCO * colors, VisualPtr pVisual)
{
   I810Ptr pI810;
   vgaHWPtr hwp;
   int i, index;
   unsigned char r, g, b;

   pI810 = I810PTR(pScrn);
   hwp = VGAHWPTR(pScrn);

   for (i = 0; i < numColors; i++) {
      index = indices[i];
      r = colors[index].red;
      g = colors[index].green;
      b = colors[index].blue;
      hwp->writeDacWriteAddr(hwp, index);
      hwp->writeDacData(hwp, r);
      hwp->writeDacData(hwp, g);
      hwp->writeDacData(hwp, b);
   }
}

Bool
I810AllocateFront(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);
   int cache_lines = -1;

   if (pI810->DoneFrontAlloc)
      return TRUE;

   memset(&(pI810->FbMemBox), 0, sizeof(BoxRec));
   /* Alloc FrontBuffer/Ring/Accel memory */
   pI810->FbMemBox.x1 = 0;
   pI810->FbMemBox.x2 = pScrn->displayWidth;
   pI810->FbMemBox.y1 = 0;
   pI810->FbMemBox.y2 = pScrn->virtualY;

   xf86GetOptValInteger(pI810->Options, OPTION_CACHE_LINES, &cache_lines);

   if (cache_lines < 0) {
      /* make sure there is enough for two DVD sized YUV buffers */
      cache_lines = (pScrn->depth == 24) ? 256 : 384;
      if (pScrn->displayWidth <= 1024)
	 cache_lines *= 2;
   }
   /* Make sure there's enough space for cache_lines.
    *
    * Had a bug here where maxCacheLines was computed to be less than 0.
    * Not sure why 256 was initially subtracted from videoRam in the
    * maxCacheLines calculation, but that was causing a problem
    * for configurations that have exactly enough Ram for the framebuffer.
    * Common code should catch the case where there isn't enough space for
    * framebuffer, we'll just check for no space for cache_lines.  -jens
    *
    */
   {
      int maxCacheLines;

      maxCacheLines = (pScrn->videoRam * 1024 /
		       (pScrn->bitsPerPixel / 8) /
		       pScrn->displayWidth) - pScrn->virtualY;
      if (maxCacheLines < 0)
	 maxCacheLines = 0;
      if (cache_lines > maxCacheLines)
	 cache_lines = maxCacheLines;
   }
   pI810->FbMemBox.y2 += cache_lines;

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "Adding %i scanlines for pixmap caching\n", cache_lines);

   /* Reserve room for the framebuffer and pixcache.  Put at the top
    * of memory so we can have nice alignment for the tiled regions at
    * the start of memory.
    */

   if (!I810AllocLow(&(pI810->FrontBuffer),
		     &(pI810->SysMem),
		     ((pI810->FbMemBox.x2 *
		       pI810->FbMemBox.y2 * pI810->cpp) + 4095) & ~4095)) {
      xf86DrvMsg(pScrn->scrnIndex,
		 X_WARNING, "Framebuffer allocation failed\n");
      return FALSE;
   } else
      DPRINTF(PFX,
	      "Frame buffer at 0x%.8x (%luk, %lu bytes)\n",
	      pI810->FrontBuffer.Start,
	      pI810->FrontBuffer.Size / 1024, pI810->FrontBuffer.Size);

   memset(&(pI810->LpRing), 0, sizeof(I810RingBuffer));
   if (I810AllocLow(&(pI810->LpRing.mem), &(pI810->SysMem), 16 * 4096)) {
      DPRINTF(PFX,
	      "Ring buffer at 0x%.8x (%luk, %lu bytes)\n",
	      pI810->LpRing.mem.Start,
	      pI810->LpRing.mem.Size / 1024, pI810->LpRing.mem.Size);

      pI810->LpRing.tail_mask = pI810->LpRing.mem.Size - 1;
      pI810->LpRing.virtual_start = pI810->FbBase + pI810->LpRing.mem.Start;
      pI810->LpRing.head = 0;
      pI810->LpRing.tail = 0;
      pI810->LpRing.space = 0;
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Ring buffer allocation failed\n");
      return (FALSE);
   }

   if (I810AllocLow(&pI810->Scratch, &(pI810->SysMem), 64 * 1024) ||
       I810AllocLow(&pI810->Scratch, &(pI810->SysMem), 16 * 1024)) {
      DPRINTF(PFX,
	      "Scratch memory at 0x%.8x (%luk, %lu bytes)\n",
	      pI810->Scratch.Start,
	      pI810->Scratch.Size / 1024, pI810->Scratch.Size);

      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Allocated Scratch Memory\n");
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Scratch memory allocation failed\n");
      return (FALSE);
   }

   pI810->DoneFrontAlloc = TRUE;
   return TRUE;
}

static Bool
I810ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
   ScrnInfoPtr pScrn;
   vgaHWPtr hwp;
   I810Ptr pI810;
   VisualPtr visual;
   MessageType driFrom = X_DEFAULT;

   pScrn = xf86Screens[pScreen->myNum];
   pI810 = I810PTR(pScrn);
   hwp = VGAHWPTR(pScrn);

   miClearVisualTypes();

   /* Re-implemented Direct Color support, -jens */
   if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
			 pScrn->rgbBits, pScrn->defaultVisual))
      return FALSE;

   if (!miSetPixmapDepths())
      return FALSE;

   {
      I810RegPtr i810Reg = &pI810->ModeReg;
      int i;

      for (i = 0; i < 8; i++)
	 i810Reg->Fence[i] = 0;
   }

   /* Have to init the DRM earlier than in other drivers to get agp
    * memory.  Wonder if this is going to be a problem...
    */

#ifdef XF86DRI
   /*
    * Setup DRI after visuals have been established, but before cfbScreenInit
    * is called.   cfbScreenInit will eventually call into the drivers
    * InitGLXVisuals call back.
    */

   if (xf86ReturnOptValBool(pI810->Options, OPTION_NOACCEL, FALSE) ||
       !xf86ReturnOptValBool(pI810->Options, OPTION_DRI, TRUE)) {
      pI810->directRenderingEnabled = FALSE;
      driFrom = X_CONFIG;
   } else {
      pI810->directRenderingEnabled = I810DRIScreenInit(pScreen);
   }

#else
   pI810->directRenderingEnabled = FALSE;
   if (!I810AllocateGARTMemory(pScrn))
      return FALSE;
   if (!I810AllocateFront(pScrn))
      return FALSE;
#endif

   if (!I810MapMem(pScrn))
      return FALSE;

   pScrn->memPhysBase = (unsigned long)pI810->LinearAddr;
   pScrn->fbOffset = 0;

   vgaHWSetMmioFuncs(hwp, pI810->MMIOBase, 0);
   vgaHWGetIOBase(hwp);
   if (!vgaHWMapMem(pScrn))
      return FALSE;

   I810Save(pScrn);
   if (!I810ModeInit(pScrn, pScrn->currentMode))
      return FALSE;

   I810SaveScreen(pScreen, FALSE);
   I810AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

   if (!fbScreenInit(pScreen, pI810->FbBase + pScrn->fbOffset,
		     pScrn->virtualX, pScrn->virtualY,
		     pScrn->xDpi, pScrn->yDpi,
		     pScrn->displayWidth, pScrn->bitsPerPixel))
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

   fbPictureInit(pScreen, 0, 0);

   xf86SetBlackWhitePixels(pScreen);

#ifdef XF86DRI
   if (pI810->LpRing.mem.Start == 0 && pI810->directRenderingEnabled) {
      pI810->directRenderingEnabled = FALSE;
      driFrom = X_PROBED;
      I810DRICloseScreen(pScreen);
   }

   if (!pI810->directRenderingEnabled) {
      pI810->DoneFrontAlloc = FALSE;
      if (!I810AllocateGARTMemory(pScrn))
	 return FALSE;
      if (!I810AllocateFront(pScrn))
	 return FALSE;
   }
#endif

   I810DGAInit(pScreen);

   if (!xf86InitFBManager(pScreen, &(pI810->FbMemBox))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Failed to init memory manager\n");
      return FALSE;
   }

   if (!xf86ReturnOptValBool(pI810->Options, OPTION_NOACCEL, FALSE)) {
      if (pI810->LpRing.mem.Size != 0) {
	 I810SetRingRegs(pScrn);

	 if (!I810AccelInit(pScreen)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Hardware acceleration initialization failed\n");
	 }
      }
   }

   miInitializeBackingStore(pScreen);
   xf86SetBackingStore(pScreen);
   xf86SetSilkenMouse(pScreen);

   miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

   if (!xf86ReturnOptValBool(pI810->Options, OPTION_SW_CURSOR, FALSE)) {
      if (!I810CursorInit(pScreen)) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Hardware cursor initialization failed\n");
      }
   }

   if (!miCreateDefColormap(pScreen))
      return FALSE;

   /* Use driver specific palette load routines for Direct Color support. -jens */
   if (pScrn->bitsPerPixel == 16) {
      if (pScrn->depth == 15) {
	 if (!xf86HandleColormaps(pScreen, 256, 8, I810LoadPalette15, 0,
				  CMAP_PALETTED_TRUECOLOR |
				  CMAP_RELOAD_ON_MODE_SWITCH))
	    return FALSE;
      } else {
	 if (!xf86HandleColormaps(pScreen, 256, 8, I810LoadPalette16, 0,
				  CMAP_PALETTED_TRUECOLOR |
				  CMAP_RELOAD_ON_MODE_SWITCH))
	    return FALSE;
      }
   } else {
      if (!xf86HandleColormaps(pScreen, 256, 8, I810LoadPalette24, 0,
			       CMAP_PALETTED_TRUECOLOR |
			       CMAP_RELOAD_ON_MODE_SWITCH))
	 return FALSE;
   }

   xf86DPMSInit(pScreen, I810DisplayPowerManagementSet, 0);

   I810InitVideo(pScreen);

#ifdef XF86DRI
   if (pI810->directRenderingEnabled) {
      /* Now that mi, cfb, drm and others have done their thing,
       * complete the DRI setup.
       */
      pI810->directRenderingEnabled = I810DRIFinishScreenInit(pScreen);
   }
#ifdef XvMCExtension
   if ((pI810->directRenderingEnabled) && (pI810->numSurfaces)) {
      /* Initialize the hardware motion compensation code */
      I810InitMC(pScreen);
   }
#endif
#endif

   if (pI810->directRenderingEnabled) {
      xf86DrvMsg(pScrn->scrnIndex, driFrom, "Direct rendering enabled\n");
   } else {
      xf86DrvMsg(pScrn->scrnIndex, driFrom, "Direct rendering disabled\n");
   }

   pScreen->SaveScreen = I810SaveScreen;
   pI810->CloseScreen = pScreen->CloseScreen;
   pScreen->CloseScreen = I810CloseScreen;

   if (serverGeneration == 1)
      xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

   return TRUE;
}

Bool
I810SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
   ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

   if (I810_DEBUG & DEBUG_VERBOSE_CURSOR)
      ErrorF("I810SwitchMode %p %x\n", mode, flags);

   return I810ModeInit(pScrn, mode);
}

void
I810AdjustFrame(int scrnIndex, int x, int y, int flags)
{
   ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
   I810Ptr pI810 = I810PTR(pScrn);
   vgaHWPtr hwp = VGAHWPTR(pScrn);
   int Base;
#if 1
   if (pI810->showCache) {
     int lastline = pI810->FbMapSize / 
       ((pScrn->displayWidth * pScrn->bitsPerPixel) / 8);
     lastline -= pScrn->currentMode->VDisplay;
     if (y > 0)
       y += pScrn->currentMode->VDisplay;
     if (y > lastline) y = lastline;
   }
#endif
   Base = (y * pScrn->displayWidth + x) >> 2;

   if (I810_DEBUG & DEBUG_VERBOSE_CURSOR)
      ErrorF("I810AdjustFrame %d,%d %x\n", x, y, flags);

   switch (pScrn->bitsPerPixel) {
   case 8:
      break;
   case 16:
      Base *= 2;
      break;
   case 24:
      /* KW: Need to do 16-pixel alignment for i810, otherwise you
       * get bad watermark problems.  Need to fixup the mouse
       * pointer positioning to take this into account.
       */
      pI810->CursorOffset = (Base & 0x3) * 4;
      Base &= ~0x3;
      Base *= 3;
      break;
   case 32:
      Base *= 4;
      break;
   }

   hwp->writeCrtc(hwp, START_ADDR_LO, Base & 0xFF);
   hwp->writeCrtc(hwp, START_ADDR_HI, (Base & 0xFF00) >> 8);
   hwp->writeCrtc(hwp, EXT_START_ADDR_HI, (Base & 0x3FC00000) >> 22);
   hwp->writeCrtc(hwp, EXT_START_ADDR,
		  ((Base & 0x00eF0000) >> 16 | EXT_START_ADDR_ENABLE));
}

/* These functions are usually called with the lock **not held**.
 */
static Bool
I810EnterVT(int scrnIndex, int flags)
{
   ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

#ifdef XF86DRI
   I810Ptr pI810 = I810PTR(pScrn);
#endif

   if (I810_DEBUG & DEBUG_VERBOSE_DRI)
      ErrorF("\n\nENTER VT\n");

   if (!I810BindGARTMemory(pScrn))
      return FALSE;

#ifdef XF86DRI
   if (pI810->directRenderingEnabled) {
      if (I810_DEBUG & DEBUG_VERBOSE_DRI)
	 ErrorF("calling dri unlock\n");
      DRIUnlock(screenInfo.screens[scrnIndex]);
      pI810->LockHeld = 0;
   }
#endif

   if (!I810ModeInit(pScrn, pScrn->currentMode))
      return FALSE;
   I810AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
   return TRUE;
}

static void
I810LeaveVT(int scrnIndex, int flags)
{
   ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
   vgaHWPtr hwp = VGAHWPTR(pScrn);
   I810Ptr pI810 = I810PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_DRI)
      ErrorF("\n\n\nLeave VT\n");

#ifdef XF86DRI
   if (pI810->directRenderingEnabled) {
      if (I810_DEBUG & DEBUG_VERBOSE_DRI)
	 ErrorF("calling dri lock\n");
      DRILock(screenInfo.screens[scrnIndex], 0);
      pI810->LockHeld = 1;
   }
#endif

   if (pI810->AccelInfoRec != NULL) {
      I810RefreshRing(pScrn);
      I810Sync(pScrn);
      pI810->AccelInfoRec->NeedToSync = FALSE;
   }
   I810Restore(pScrn);

   if (!I810UnbindGARTMemory(pScrn))
      return;

   vgaHWLock(hwp);
}

static Bool
I810CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
   vgaHWPtr hwp = VGAHWPTR(pScrn);
   I810Ptr pI810 = I810PTR(pScrn);
   XAAInfoRecPtr infoPtr = pI810->AccelInfoRec;

   if (pScrn->vtSema == TRUE) {
      I810Restore(pScrn);
      vgaHWLock(hwp);
   }
#ifdef XF86DRI
   if (pI810->directRenderingEnabled) {
      I810DRICloseScreen(pScreen);
      pI810->directRenderingEnabled = FALSE;
   }
#endif

   if (pScrn->vtSema == TRUE) {
      I810UnbindGARTMemory(pScrn);
      I810Restore(pScrn);
      vgaHWLock(hwp);
   }

   I810UnmapMem(pScrn);
   vgaHWUnmapMem(pScrn);

   if (pI810->ScanlineColorExpandBuffers) {
      xfree(pI810->ScanlineColorExpandBuffers);
      pI810->ScanlineColorExpandBuffers = 0;
   }

   if (infoPtr) {
      if (infoPtr->ScanlineColorExpandBuffers)
	 xfree(infoPtr->ScanlineColorExpandBuffers);
      XAADestroyInfoRec(infoPtr);
      pI810->AccelInfoRec = 0;
   }

   if (pI810->CursorInfoRec) {
      xf86DestroyCursorInfoRec(pI810->CursorInfoRec);
      pI810->CursorInfoRec = 0;
   }

   /* Free all allocated video ram.
    */
   pI810->SysMem = pI810->SavedSysMem;
   pI810->DcacheMem = pI810->SavedDcacheMem;
   pI810->DoneFrontAlloc = FALSE;

   /* Need to actually close the gart fd, or the unbound memory will just sit
    * around.  Will prevent the Xserver from recycling.
    */
   xf86GARTCloseScreen(scrnIndex);

   pScrn->vtSema = FALSE;
   pScreen->CloseScreen = pI810->CloseScreen;
   return (*pScreen->CloseScreen) (scrnIndex, pScreen);
}

static void
I810FreeScreen(int scrnIndex, int flags)
{
   I810FreeRec(xf86Screens[scrnIndex]);
   if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
      vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

static int
I810ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
   if (mode->Flags & V_INTERLACE) {
      if (verbose) {
	 xf86DrvMsg(scrnIndex, X_PROBED,
		    "Removing interlaced mode \"%s\"\n", mode->name);
      }
      return MODE_BAD;
   }
   return MODE_OK;
}

static Bool
I810SaveScreen(ScreenPtr pScreen, Bool unblack)
{
   return vgaHWSaveScreen(pScreen, unblack);
}

static void
I810DisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
			      int flags)
{
   I810Ptr pI810;
   unsigned char SEQ01 = 0;
   int DPMSSyncSelect = 0;

   pI810 = I810PTR(pScrn);
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
   SEQ01 |= pI810->readControl(pI810, SRX, 0x01) & ~0x20;
   pI810->writeControl(pI810, SRX, 0x01, SEQ01);

   /* Set the DPMS mode */
   OUTREG8(DPMS_SYNC_SELECT, DPMSSyncSelect);
}
#endif /* I830_ONLY */
