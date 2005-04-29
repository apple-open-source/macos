/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_driver.c-arc   1.42   03 Jan 2001 13:52:16   Frido  $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of The XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from The XFree86 Project or Silicon Motion.
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_driver.c,v 1.37 2003/11/06 18:38:08 tsi Exp $ */

#include "xf86Resources.h"
#include "xf86RAC.h"
#include "xf86DDC.h"
#include "xf86int10.h"
#include "vbe.h"
#include "shadowfb.h"

#include "smi.h"

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

/*
 * Internals
 */
static void SMI_EnableMmio(ScrnInfoPtr pScrn);
static void SMI_DisableMmio(ScrnInfoPtr pScrn);

/*
 * Forward definitions for the functions that make up the driver.
 */

static const OptionInfoRec * SMI_AvailableOptions(int chipid, int busid);
static void SMI_Identify(int flags);
static Bool SMI_Probe(DriverPtr drv, int flags);
static Bool SMI_PreInit(ScrnInfoPtr pScrn, int flags);
static Bool SMI_EnterVT(int scrnIndex, int flags);
static void SMI_LeaveVT(int scrnIndex, int flags);
static void SMI_Save (ScrnInfoPtr pScrn);
static void SMI_WriteMode (ScrnInfoPtr pScrn, vgaRegPtr, SMIRegPtr);
static Bool SMI_ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
                           char **argv);
static int SMI_InternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static void SMI_PrintRegs(ScrnInfoPtr);
static ModeStatus SMI_ValidMode(int scrnIndex, DisplayModePtr mode,
                                Bool verbose, int flags);
static void SMI_DisableVideo(ScrnInfoPtr pScrn);
static void SMI_EnableVideo(ScrnInfoPtr pScrn);
static Bool SMI_MapMem(ScrnInfoPtr pScrn);
static void SMI_UnmapMem(ScrnInfoPtr pScrn);
static Bool SMI_ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool SMI_CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool SMI_SaveScreen(ScreenPtr pScreen, int mode);
static void SMI_LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
                            LOCO *colors, VisualPtr pVisual);
static void SMI_DisplayPowerManagementSet(ScrnInfoPtr pScrn,
                                          int PowerManagementMode, int flags);
static Bool SMI_ddc1(int scrnIndex);
static unsigned int SMI_ddc1Read(ScrnInfoPtr pScrn);
static void SMI_FreeScreen(int ScrnIndex, int flags);
static void SMI_ProbeDDC(ScrnInfoPtr pScrn, int index);


#define SILICONMOTION_NAME          "Silicon Motion"
#define SILICONMOTION_DRIVER_NAME   "siliconmotion"
#define SILICONMOTION_VERSION_NAME  "1.3.1"
#define SILICONMOTION_VERSION_MAJOR 1
#define SILICONMOTION_VERSION_MINOR 3
#define SILICONMOTION_PATCHLEVEL    1
#define SILICONMOTION_DRIVER_VERSION ((SILICONMOTION_VERSION_MAJOR << 24) | \
                                      (SILICONMOTION_VERSION_MINOR << 16) | \
                                      (SILICONMOTION_PATCHLEVEL))

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec SILICONMOTION =
{
	SILICONMOTION_DRIVER_VERSION,
	SILICONMOTION_DRIVER_NAME,
	SMI_Identify,
	SMI_Probe,
	SMI_AvailableOptions,
	NULL,
	0
};

/* Supported chipsets */
static SymTabRec SMIChipsets[] =
{
	{ PCI_CHIP_SMI910, "Lynx"    },
	{ PCI_CHIP_SMI810, "LynxE"   },
	{ PCI_CHIP_SMI820, "Lynx3D"  },
	{ PCI_CHIP_SMI710, "LynxEM"  },
	{ PCI_CHIP_SMI712, "LynxEM+" },
	{ PCI_CHIP_SMI720, "Lynx3DM" },
	{ PCI_CHIP_SMI731, "Cougar3DR" },
	{ -1,             NULL      }
};

static PciChipsets SMIPciChipsets[] =
{
	/* numChipset,		PciID,				Resource */
	{ PCI_CHIP_SMI910,	PCI_CHIP_SMI910,	RES_SHARED_VGA },
	{ PCI_CHIP_SMI810,	PCI_CHIP_SMI810,	RES_SHARED_VGA },
	{ PCI_CHIP_SMI820,	PCI_CHIP_SMI820,	RES_SHARED_VGA },
	{ PCI_CHIP_SMI710,	PCI_CHIP_SMI710,	RES_SHARED_VGA },
	{ PCI_CHIP_SMI712,	PCI_CHIP_SMI712,	RES_SHARED_VGA },
	{ PCI_CHIP_SMI720,	PCI_CHIP_SMI720,	RES_SHARED_VGA },
	{ PCI_CHIP_SMI731,	PCI_CHIP_SMI731,	RES_SHARED_VGA },
	{ -1,				-1,					RES_UNDEFINED  }
};

typedef enum
{
	OPTION_PCI_BURST,
	OPTION_FIFO_CONSERV,
	OPTION_FIFO_MODERATE,
	OPTION_FIFO_AGGRESSIVE,
	OPTION_PCI_RETRY,
	OPTION_NOACCEL,
	OPTION_MCLK,
	OPTION_SHOWCACHE,
	OPTION_SWCURSOR,
	OPTION_HWCURSOR,
	OPTION_SHADOW_FB,
	OPTION_ROTATE,
	OPTION_VIDEOKEY,
	OPTION_BYTESWAP,
	/* CZ 26.10.2001: interlaced video */
	OPTION_INTERLACED,
	/* end CZ */
	OPTION_USEBIOS,
	OPTION_ZOOMONLCD,
	NUMBER_OF_OPTIONS

} SMIOpts;

static const OptionInfoRec SMIOptions[] =
{
   { OPTION_PCI_BURST,	     "pci_burst",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_FIFO_CONSERV,    "fifo_conservative", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_FIFO_MODERATE,   "fifo_moderate",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_FIFO_AGGRESSIVE, "fifo_aggressive",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_PCI_RETRY,	     "pci_retry",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_NOACCEL,	     "NoAccel",		  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_MCLK,	     "set_mclk",	  OPTV_FREQ,	{0}, FALSE },
   { OPTION_SHOWCACHE,	     "show_cache",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_HWCURSOR,	     "HWCursor",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_SWCURSOR,	     "SWCursor",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_SHADOW_FB,	     "ShadowFB",	  OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_ROTATE,	     "Rotate",		  OPTV_ANYSTR,  {0}, FALSE },
   { OPTION_VIDEOKEY,	     "VideoKey",	  OPTV_INTEGER, {0}, FALSE },
   { OPTION_BYTESWAP,	     "ByteSwap",	  OPTV_BOOLEAN, {0}, FALSE },
    /* CZ 26.10.2001: interlaced video */
   { OPTION_INTERLACED,	     "Interlaced",        OPTV_BOOLEAN, {0}, FALSE },
   /* end CZ */
   { OPTION_USEBIOS,	     "UseBIOS",		  OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_ZOOMONLCD,	     "ZoomOnLCD",	  OPTV_BOOLEAN,	{0}, FALSE },
   { -1,		     NULL,		  OPTV_NONE,	{0}, FALSE }
};

/*
 * Lists of symbols that may/may not be required by this driver.
 * This allows the loader to know which ones to issue warnings for.
 *
 * Note that vgahwSymbols and xaaSymbols are referenced outside the
 * XFree86LOADER define in later code, so are defined outside of that
 * define here also.
 */

static const char *vgahwSymbols[] =
{
	"vgaHWCopyReg",
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
	"vgaHWSetStdFuncs",
	"vgaHWUnmapMem",
	"vgaHWddc1SetSpeed",
	NULL
};

static const char *xaaSymbols[] =
{
	"XAACopyROP",
	"XAACreateInfoRec",
	"XAADestroyInfoRec",
	"XAAFallbackOps",
	"XAAInit",
	"XAAPatternROP",
	NULL
};

static const char *ramdacSymbols[] =
{
	"xf86CreateCursorInfoRec",
	"xf86DestroyCursorInfoRec",
	"xf86InitCursor",
	NULL
};

static const char *ddcSymbols[] =
{
	"xf86PrintEDID",
	"xf86DoEDID_DDC1",
	"xf86DoEDID_DDC2",
	"xf86SetDDCproperties",
	NULL
};

static const char *i2cSymbols[] =
{
	"xf86CreateI2CBusRec",
	"xf86CreateI2CDevRec",
	"xf86DestroyI2CBusRec",
	"xf86DestroyI2CDevRec",
	"xf86I2CBusInit",
	"xf86I2CDevInit",
	"xf86I2CReadBytes",
	"xf86I2CWriteByte",
	NULL
};

static const char *shadowSymbols[] =
{
	"ShadowFBInit",
	NULL
};

static const char *int10Symbols[] =
{
	"xf86ExecX86int10",
	"xf86FreeInt10",
	"xf86InitInt10",
	NULL
};

static const char *vbeSymbols[] =
{
	"VBEInit",
	"vbeDoEDID",
	"vbeFree",
	NULL
};

static const char *fbSymbols[] =
{
#ifdef USE_FB
	"fbPictureInit",
	"fbScreenInit",
#else
	"cfbScreenInit",
	"cfb16ScreenInit",
	"cfb24ScreenInit",
	"cfb32ScreenInit",
	"cfb16BresS",
	"cfb24BresS",
#endif
	NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(siliconmotionSetup);

static XF86ModuleVersionInfo SMIVersRec =
{
	"siliconmotion",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	SILICONMOTION_VERSION_MAJOR,
	SILICONMOTION_VERSION_MINOR,
	SILICONMOTION_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0, 0, 0, 0}
};

/*
 * This is the module init data for XFree86 modules.
 *
 * Its name has to be the driver name followed by ModuleData.
 */
XF86ModuleData siliconmotionModuleData =
{
	&SMIVersRec,
	siliconmotionSetup,
	NULL
};

static pointer
siliconmotionSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone)
	{
		setupDone = TRUE;
		xf86AddDriver(&SILICONMOTION, module, 0);

		/*
		 * Modules that this driver always requires can be loaded here
		 * by calling LoadSubModule().
		 */

		/*
		 * Tell the loader about symbols from other modules that this module
		 * might refer to.
		 */
		LoaderRefSymLists(vgahwSymbols, fbSymbols, xaaSymbols, ramdacSymbols,
						  ddcSymbols, i2cSymbols, int10Symbols, vbeSymbols,
						  shadowSymbols, NULL);

		/*
		 * The return value must be non-NULL on success even though there
		 * is no TearDownProc.
		 */
		return (pointer) 1;
	}
	else
	{
		if (errmaj)
		{
			*errmaj = LDR_ONCEONLY;
		}
		return(NULL);
	}
}

#endif /* XFree86LOADER */

static Bool
SMI_GetRec(ScrnInfoPtr pScrn)
{
	ENTER_PROC("SMI_GetRec");

	/*
	 * Allocate an 'Chip'Rec, and hook it into pScrn->driverPrivate.
	 * pScrn->driverPrivate is initialised to NULL, so we can check if
	 * the allocation has already been done.
	 */
	if (pScrn->driverPrivate == NULL)
	{
		pScrn->driverPrivate = xnfcalloc(sizeof(SMIRec), 1);
	}

	LEAVE_PROC("SMI_GetRec");
	return(TRUE);
}

static void
SMI_FreeRec(ScrnInfoPtr pScrn)
{
	ENTER_PROC("SMI_FreeRec");

	if (pScrn->driverPrivate != NULL)
	{
		xfree(pScrn->driverPrivate);
		pScrn->driverPrivate = NULL;
	}

	LEAVE_PROC("SMI_FreeRec");
}

static const OptionInfoRec *
SMI_AvailableOptions(int chipid, int busid)
{
	ENTER_PROC("SMI_AvailableOptions");
	LEAVE_PROC("SMI_AvailableOptions");
	return(SMIOptions);
}

static void
SMI_Identify(int flags)
{
	ENTER_PROC("SMI_Identify");

	xf86PrintChipsets(SILICONMOTION_NAME, "driver (version "
			SILICONMOTION_VERSION_NAME ") for Silicon Motion Lynx chipsets",
			SMIChipsets);

	LEAVE_PROC("SMI_Identify");
}

static Bool
SMI_Probe(DriverPtr drv, int flags)
{
	int i;
	GDevPtr *devSections;
	int *usedChips;
	int numDevSections;
	int numUsed;
	Bool foundScreen = FALSE;

	ENTER_PROC("SMI_Probe");

	numDevSections = xf86MatchDevice(SILICONMOTION_DRIVER_NAME, &devSections);
	if (numDevSections <= 0)
	{
		/* There's no matching device section in the config file, so quit now.
		 */
		LEAVE_PROC("SMI_Probe");
		return(FALSE);
	}

	if (xf86GetPciVideoInfo() == NULL)
	{
		LEAVE_PROC("SMI_Probe");
		return(FALSE);
	}

	numUsed = xf86MatchPciInstances(SILICONMOTION_NAME, PCI_SMI_VENDOR_ID,
									SMIChipsets, SMIPciChipsets, devSections,
									numDevSections, drv, &usedChips);

	/* Free it since we don't need that list after this */
	xfree(devSections);
	if (numUsed <= 0)
	{
		LEAVE_PROC("SMI_Probe");
		return(FALSE);
	}

	if (flags & PROBE_DETECT)
	{
		foundScreen = TRUE;
	}
	else
	{
		for (i = 0; i < numUsed; i++)
		{
			/* Allocate a ScrnInfoRec and claim the slot */
			ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);

			/* Fill in what we can of the ScrnInfoRec */
			pScrn->driverVersion = SILICONMOTION_DRIVER_VERSION;
			pScrn->driverName	 = SILICONMOTION_DRIVER_NAME;
			pScrn->name			 = SILICONMOTION_NAME;
			pScrn->Probe		 = SMI_Probe;
			pScrn->PreInit		 = SMI_PreInit;
			pScrn->ScreenInit	 = SMI_ScreenInit;
			pScrn->SwitchMode	 = SMI_SwitchMode;
			pScrn->AdjustFrame	 = SMI_AdjustFrame;
			pScrn->EnterVT		 = SMI_EnterVT;
			pScrn->LeaveVT		 = SMI_LeaveVT;
			pScrn->FreeScreen	 = SMI_FreeScreen;
			pScrn->ValidMode	 = SMI_ValidMode;
			foundScreen			 = TRUE;

			xf86ConfigActivePciEntity(pScrn, usedChips[i], SMIPciChipsets, NULL,
									  NULL, NULL, NULL, NULL);
		}
	}
	xfree(usedChips);

	LEAVE_PROC("SMI_Probe");
	return(foundScreen);
}

static Bool
SMI_PreInit(ScrnInfoPtr pScrn, int flags)
{
	EntityInfoPtr pEnt;
	SMIPtr pSmi;
	MessageType from;
	int i;
	double real;
	ClockRangePtr clockRanges;
#ifndef USE_FB
	char *mod = NULL;
	const char *reqSym = NULL;
#endif
	char *s;
	unsigned char config, m, n, shift;
	int mclk;
	vgaHWPtr hwp;
	int vgaCRIndex, vgaIOBase;
	vbeInfoPtr pVbe = NULL;
	
	ENTER_PROC("SMI_PreInit");

	if (flags & PROBE_DETECT)
	{
		SMI_ProbeDDC(pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index);
		LEAVE_PROC("SMI_PreInit");
		return(TRUE);
	}

	/* Ignoring the Type list for now.  It might be needed when multiple cards
	 * are supported.
	 */
	if (pScrn->numEntities > 1)
	{
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	/* The vgahw module should be loaded here when needed */
	if (!xf86LoadSubModule(pScrn, "vgahw"))
	{
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	xf86LoaderReqSymLists(vgahwSymbols, NULL);

	/*
	 * Allocate a vgaHWRec
	 */
	if (!vgaHWGetHWRec(pScrn))
	{
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	/* Allocate the SMIRec driverPrivate */
	if (!SMI_GetRec(pScrn))
	{
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}
	pSmi = SMIPTR(pScrn);

	/* Set pScrn->monitor */
	pScrn->monitor = pScrn->confScreen->monitor;

	/*
	 * The first thing we should figure out is the depth, bpp, etc.
	 * We support only 24bpp layouts, so indicate that.
	 */
	if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support24bppFb))
	{
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	/* Check that the returned depth is one we support */
	switch (pScrn->depth)
	{
		case 8:
		case 16:
		case 24:
			/* OK */
			break;

		default:
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"Given depth (%d) is not supported by this driver\n",
					pScrn->depth);
			LEAVE_PROC("SMI_PreInit");
			return(FALSE);
	}
	xf86PrintDepthBpp(pScrn);

	/*
	 * This must happen after pScrn->display has been set because
	 * xf86SetWeight references it.
	 */
	if (pScrn->depth > 8)
	{
		/* The defaults are OK for us */
		rgb zeros = {0, 0, 0};

		if (!xf86SetWeight(pScrn, zeros, zeros))
		{
			LEAVE_PROC("SMI_PreInit");
			return(FALSE);
		}
	}

	if (!xf86SetDefaultVisual(pScrn, -1))
	{
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	/* We don't currently support DirectColor at > 8bpp */
	if ((pScrn->depth > 8) && (pScrn->defaultVisual != TrueColor))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual (%s) "
				"is not supported at depth %d\n",
				xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	/* We use a programmable clock */
	pScrn->progClock = TRUE;

	/* Collect all of the relevant option flags (fill in pScrn->options) */
	xf86CollectOptions(pScrn, NULL);

	/* Set the bits per RGB for 8bpp mode */
	if (pScrn->depth == 8)
	{
		pScrn->rgbBits = 6;
	}

	/* Process the options */
	if (!(pSmi->Options = xalloc(sizeof(SMIOptions))))
		return FALSE;
	memcpy(pSmi->Options, SMIOptions, sizeof(SMIOptions));
	xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pSmi->Options);

	if (xf86ReturnOptValBool(pSmi->Options, OPTION_PCI_BURST, FALSE))
	{
		pSmi->pci_burst = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_burst - PCI burst "
				"read enabled\n");
	}
	else
	{
		pSmi->pci_burst = FALSE;
	}

	pSmi->NoPCIRetry = TRUE;
	if (xf86ReturnOptValBool(pSmi->Options, OPTION_PCI_RETRY, FALSE))
	{
		if (xf86ReturnOptValBool(pSmi->Options, OPTION_PCI_BURST, FALSE))
		{
			pSmi->NoPCIRetry = FALSE;
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_retry\n");
		}
		else
		{
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"pci_retry\" option "
					"requires \"pci_burst\".\n");
		}
	}

	if (xf86IsOptionSet(pSmi->Options, OPTION_FIFO_CONSERV))
	{
		pSmi->fifo_conservative = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_conservative "
				"set\n");
	}
	else
	{
		pSmi->fifo_conservative = FALSE;
	}

	if (xf86IsOptionSet(pSmi->Options, OPTION_FIFO_MODERATE))
	{
		pSmi->fifo_moderate = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_moderate set\n");
	}
	else
	{
		pSmi->fifo_moderate = FALSE;
	}

	if (xf86IsOptionSet(pSmi->Options, OPTION_FIFO_AGGRESSIVE))
	{
		pSmi->fifo_aggressive = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_aggressive set\n");
	}
	else
	{
		pSmi->fifo_aggressive = FALSE;
	}

	if (xf86ReturnOptValBool(pSmi->Options, OPTION_NOACCEL, FALSE))
	{
		pSmi->NoAccel = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: NoAccel - Acceleration "
				"disabled\n");
	}
	else
	{
		pSmi->NoAccel = FALSE;
	}

	if (xf86ReturnOptValBool(pSmi->Options, OPTION_SHOWCACHE, FALSE))
	{
		pSmi->ShowCache = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: show_cache set\n");
	}
	else
	{
		pSmi->ShowCache = FALSE;
	}

	if (xf86GetOptValFreq(pSmi->Options, OPTION_MCLK, OPTUNITS_MHZ, &real))
	{
		pSmi->MCLK = (int)(real * 1000.0);
		if (pSmi->MCLK <= 120000)
		{
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: set_mclk set to "
					"%1.3f MHz\n", pSmi->MCLK / 1000.0);
		}
		else
		{
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Memory Clock value of "
					"%1.3f MHz is larger than limit of 120 MHz\n",
					pSmi->MCLK / 1000.0);
			pSmi->MCLK = 0;
		}
	}
	else
	{
		pSmi->MCLK = 0;
	}

	from = X_DEFAULT;
	pSmi->hwcursor = TRUE;
	if (xf86GetOptValBool(pSmi->Options, OPTION_HWCURSOR, &pSmi->hwcursor))
	{
		from = X_CONFIG;
	}
	if (xf86ReturnOptValBool(pSmi->Options, OPTION_SWCURSOR, FALSE))
	{
		pSmi->hwcursor = FALSE;
		from = X_CONFIG;
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "Using %s Cursor\n",
			pSmi->hwcursor ? "Hardware" : "Software");

	if (xf86GetOptValBool(pSmi->Options, OPTION_SHADOW_FB, &pSmi->shadowFB))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShadowFB %s.\n",
				pSmi->shadowFB ? "enabled" : "disabled");
	}

#if 1 /* PDR#932 */
	if ((pScrn->depth == 8) || (pScrn->depth == 16))
#endif /* PDR#932 */
	if ((s = xf86GetOptValString(pSmi->Options, OPTION_ROTATE)))
	{
		if(!xf86NameCmp(s, "CW"))
		{
			pSmi->shadowFB = TRUE;
			pSmi->rotate = SMI_ROTATE_CCW;
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rotating screen "
					"clockwise\n");
		}
		else if (!xf86NameCmp(s, "CCW"))
		{
			pSmi->shadowFB = TRUE;
			pSmi->rotate = SMI_ROTATE_CW;
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rotating screen counter "
					"clockwise\n");
		}
		else
		{
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid "
					"value for Option \"Rotate\"\n", s);
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Valid options are \"CW\" or "
					"\"CCW\"\n");
		}
	}

	if (pSmi->rotate)
	{
		/* Disable the RandR extension, it messes up the internal rotation stuff */
		xf86DisableRandR();
	}
		
	if (xf86GetOptValInteger(pSmi->Options, OPTION_VIDEOKEY, &pSmi->videoKey))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: Video key set to "
				"0x%08X\n", pSmi->videoKey);
	}
	else
	{
		pSmi->videoKey = (1 << pScrn->offset.red) | (1 << pScrn->offset.green)
				| (((pScrn->mask.blue >> pScrn->offset.blue) - 1)
						<< pScrn->offset.blue);
	}

	if (xf86ReturnOptValBool(pSmi->Options, OPTION_BYTESWAP, FALSE))
	{
		pSmi->ByteSwap = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: ByteSwap enabled.\n");
	}
	else
	{
		pSmi->ByteSwap = FALSE;
	}

	/* CZ 26.10.2001: interlaced video */
	if (xf86ReturnOptValBool(pSmi->Options, OPTION_INTERLACED, FALSE))
	{
		pSmi->interlaced = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: Interlaced enabled.\n");
	}
	else
	{
		pSmi->interlaced = FALSE;
	}
	/* end CZ */

	if (xf86GetOptValBool(pSmi->Options, OPTION_USEBIOS, &pSmi->useBIOS))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: UseBIOS %s.\n",
				pSmi->useBIOS ? "enabled" : "disabled");
	}
	else
	{
		/* Default to UseBIOS enabled. */
		pSmi->useBIOS = TRUE;
	}

	if (xf86GetOptValBool(pSmi->Options, OPTION_ZOOMONLCD, &pSmi->zoomOnLCD))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: ZoomOnLCD %s.\n",
				pSmi->zoomOnLCD ? "enabled" : "disabled");
	}
	else
	{
		/* Default to ZoomOnLCD enabled. */
		pSmi->zoomOnLCD = TRUE;
	}

	/* Find the PCI slot for this screen */
	pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
	if ((pEnt->location.type != BUS_PCI) || (pEnt->resources))
	{
		xfree(pEnt);
		SMI_FreeRec(pScrn);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	if (xf86LoadSubModule(pScrn,"int10")) {
	    xf86LoaderReqSymLists(int10Symbols,NULL);
	    pSmi->pInt10 = xf86InitInt10(pEnt->index);
	}
	
	if (pSmi->pInt10 && xf86LoadSubModule(pScrn, "vbe"))
	{
	    xf86LoaderReqSymLists(vbeSymbols, NULL);
	    pVbe = VBEInit(pSmi->pInt10, pEnt->index);
	}

	pSmi->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
	xf86RegisterResources(pEnt->index, NULL, ResExclusive);
/*	xf86SetOperatingState(resVgaIo, pEnt->index, ResUnusedOpr); */
/*	xf86SetOperatingState(resVgaMem, pEnt->index, ResDisableOpr); */

	/*
	 * Set the Chipset and ChipRev, allowing config file entries to
	 * override.
	 */
	if (pEnt->device->chipset && *pEnt->device->chipset)
	{
		pScrn->chipset = pEnt->device->chipset;
		pSmi->Chipset = xf86StringToToken(SMIChipsets, pScrn->chipset);
		from = X_CONFIG;
	}
	else if (pEnt->device->chipID >= 0)
	{
		pSmi->Chipset = pEnt->device->chipID;
		pScrn->chipset = (char *) xf86TokenToString(SMIChipsets, pSmi->Chipset);
		from = X_CONFIG;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
				pSmi->Chipset);
	}
	else
	{
		from = X_PROBED;
		pSmi->Chipset = pSmi->PciInfo->chipType;
		pScrn->chipset = (char *) xf86TokenToString(SMIChipsets, pSmi->Chipset);
	}

	if (pEnt->device->chipRev >= 0)
	{
		pSmi->ChipRev = pEnt->device->chipRev;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
				pSmi->ChipRev);
	}
	else
	{
		pSmi->ChipRev = pSmi->PciInfo->chipRev;
	}
	xfree(pEnt);

	/*
	 * This shouldn't happen because such problems should be caught in
	 * SMI_Probe(), but check it just in case.
	 */
	if (pScrn->chipset == NULL)
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "ChipID 0x%04X is not "
				"recognised\n", pSmi->Chipset);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}
	if (pSmi->Chipset < 0)
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Chipset \"%s\" is not "
				"recognised\n", pScrn->chipset);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);

	pSmi->PciTag = pciTag(pSmi->PciInfo->bus, pSmi->PciInfo->device,
			pSmi->PciInfo->func);

	SMI_MapMem(pScrn);
	SMI_DisableVideo(pScrn);

	hwp = VGAHWPTR(pScrn);
	vgaIOBase  = hwp->IOBase;
	vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
	pSmi->PIOBase = hwp->PIOOffset;

	xf86ErrorFVerb(VERBLEV, "\tSMI_PreInit vgaCRIndex=%x, vgaIOBase=%x, "
			"MMIOBase=%p\n", vgaCRIndex, vgaIOBase, hwp->MMIOBase);

	/* Next go on to detect amount of installed ram */
	config = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x71);

	if (xf86LoadSubModule(pScrn, "i2c"))
	{
		xf86LoaderReqSymLists(i2cSymbols, NULL);
		SMI_I2CInit(pScrn);
	}
	if (xf86LoadSubModule(pScrn, "ddc"))
	{
		xf86MonPtr pMon = NULL;

		xf86LoaderReqSymLists(ddcSymbols, NULL);
#if 1 /* PDR#579 */
		if (pVbe)
		{
			pMon = vbeDoEDID(pVbe, NULL);
			if (pMon != NULL)
			{
				if (   (pMon->rawData[0] == 0x00)
					&& (pMon->rawData[1] == 0xFF)
					&& (pMon->rawData[2] == 0xFF)
					&& (pMon->rawData[3] == 0xFF)
					&& (pMon->rawData[4] == 0xFF)
					&& (pMon->rawData[5] == 0xFF)
					&& (pMon->rawData[6] == 0xFF)
					&& (pMon->rawData[7] == 0x00)
				)
				{
					pMon = xf86PrintEDID(pMon);
					if (pMon != NULL)
					{
						xf86SetDDCproperties(pScrn, pMon);
					}
				}
			}
		}
#else
		if (   (pVbe)
			&& ((pMon = xf86PrintEDID(vbeDoEDID(pVbe, NULL))) != NULL)
		)
		{
			xf86SetDDCproperties(pScrn, pMon);
		}
#endif
		else if (!SMI_ddc1(pScrn->scrnIndex))
		{
			if (pSmi->I2C)
			{
				xf86SetDDCproperties(pScrn,
						xf86PrintEDID(xf86DoEDID_DDC2(pScrn->scrnIndex,
								pSmi->I2C)));
			}
		}
	}

	vbeFree(pVbe);
	xf86FreeInt10(pSmi->pInt10);
	pSmi->pInt10 = NULL;

	/*
	 * If the driver can do gamma correction, it should call xf86SetGamma()
	 * here. (from MGA, no ViRGE gamma support yet, but needed for
	 * xf86HandleColormaps support.)
	 */
	{
		Gamma zeros = { 0.0, 0.0, 0.0 };

		if (!xf86SetGamma(pScrn, zeros))
		{
			LEAVE_PROC("SMI_PreInit");
			SMI_EnableVideo(pScrn);
			SMI_UnmapMem(pScrn);
			return(FALSE);
		}
	}

	/* And compute the amount of video memory and offscreen memory */
	pSmi->videoRAMKBytes = 0;

	if (!pScrn->videoRam)
	{
		switch (pSmi->Chipset)
		{
			default:
			{
				int mem_table[4] = { 1, 2, 4, 0 };
				pSmi->videoRAMKBytes = mem_table[(config >> 6)] * 1024;
				break;
			}

			case SMI_LYNX3D:
			{
				int mem_table[4] = { 0, 2, 4, 6 };
				pSmi->videoRAMKBytes = mem_table[(config >> 6)] * 1024 + 512;
				break;
			}

			case SMI_LYNX3DM:
			{
				int mem_table[4] = { 16, 2, 4, 8 };
				pSmi->videoRAMKBytes = mem_table[(config >> 6)] * 1024;
				break;
			}

			case SMI_COUGAR3DR:
			{
				/* DANGER - Cougar3DR BIOS is broken - hardcode video ram size */
				/* per instructions from Silicon Motion engineers */
				pSmi->videoRAMKBytes = 16 * 1024;
				break;
            }
		}
		pSmi->videoRAMBytes = pSmi->videoRAMKBytes * 1024;
		pScrn->videoRam     = pSmi->videoRAMKBytes;

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "videoram: %dkB\n",
				pSmi->videoRAMKBytes);
	}
	else
	{
		pSmi->videoRAMKBytes = pScrn->videoRam;
		pSmi->videoRAMBytes  = pScrn->videoRam * 1024;

		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "videoram: %dk\n",
				pSmi->videoRAMKBytes);
	}

	/* Lynx built-in ramdac speeds */
	pScrn->numClocks = 4;

	if ((pScrn->clock[3] <= 0) && (pScrn->clock[2] > 0))
	{
		pScrn->clock[3] = pScrn->clock[2];
	}

    if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR))
	{
		if (pScrn->clock[0] <= 0) pScrn->clock[0] = 200000;
		if (pScrn->clock[1] <= 0) pScrn->clock[1] = 200000;
		if (pScrn->clock[2] <= 0) pScrn->clock[2] = 200000;
		if (pScrn->clock[3] <= 0) pScrn->clock[3] = 200000;
	}
	else
	{
		if (pScrn->clock[0] <= 0) pScrn->clock[0] = 135000;
		if (pScrn->clock[1] <= 0) pScrn->clock[1] = 135000;
		if (pScrn->clock[2] <= 0) pScrn->clock[2] = 135000;
		if (pScrn->clock[3] <= 0) pScrn->clock[3] = 135000;
	}

	/* Now set RAMDAC limits */
	switch (pSmi->Chipset)
	{
		default:
			pSmi->minClock = 20000;
			pSmi->maxClock = 135000;
			break;
	}
	xf86ErrorFVerb(VERBLEV, "\tSMI_PreInit minClock=%d, maxClock=%d\n",
			pSmi->minClock, pSmi->maxClock);

	/* Detect current MCLK and print it for user */
	m = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6A);
	n = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6B);
	switch (n >> 6)
	{
		default:
			shift = 1;
			break;

		case 1:
			shift = 4;
			break;

		case 2:
			shift = 2;
			break;
	}
	n &= 0x3F;
	mclk = ((1431818 * m) / n / shift + 50) / 100;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected current MCLK value of "
			"%1.3f MHz\n", mclk / 1000.0);

	SMI_EnableVideo(pScrn);
	SMI_UnmapMem(pScrn);

	pScrn->virtualX = pScrn->display->virtualX;

	/*
	 * Setup the ClockRanges, which describe what clock ranges are available,
	 * and what sort of modes they can be used for.
	 */
	clockRanges = xnfcalloc(sizeof(ClockRange),1);
	clockRanges->next = NULL;
	clockRanges->minClock = pSmi->minClock;
	clockRanges->maxClock = pSmi->maxClock;
	clockRanges->clockIndex = -1;
	clockRanges->interlaceAllowed = FALSE;
	clockRanges->doubleScanAllowed = FALSE;
		
	i = xf86ValidateModes(
			pScrn,						/* Screen pointer					  */
			pScrn->monitor->Modes,		/* Available monitor modes			  */
			pScrn->display->modes,		/* req mode names for screen		  */
			clockRanges,				/* list of clock ranges allowed		  */
			NULL,						/* use min/max below				  */
			128,						/* min line pitch (width)			  */
			4096,						/* maximum line pitch (width)		  */
			128,						/* bits of granularity for line pitch */
										/* (width) above					  */
			128,						/* min virtual height				  */
			4096,						/* max virtual height				  */
			pScrn->display->virtualX,	/* force virtual x					  */
			pScrn->display->virtualY,	/* force virtual Y					  */
			pSmi->videoRAMBytes,		/* size of aperture used to access	  */
										/* video memory						  */
			LOOKUP_BEST_REFRESH);		/* how to pick modes				  */

	if (i == -1)
	{
		SMI_FreeRec(pScrn);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	/* Prune the modes marked as invalid */
	xf86PruneDriverModes(pScrn);

	if ((i == 0) || (pScrn->modes == NULL))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
		SMI_FreeRec(pScrn);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}
	xf86SetCrtcForModes(pScrn, 0);

	/* Set the current mode to the first in the list */
	pScrn->currentMode = pScrn->modes;

	/* Print the list of modes being used */
	xf86PrintModes(pScrn);

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

#ifdef USE_FB
	if ((xf86LoadSubModule(pScrn, "fb") == NULL))
	{
		SMI_FreeRec(pScrn);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	xf86LoaderReqSymLists(fbSymbols, NULL);
#else
	/* Load bpp-specific modules */
	switch (pScrn->bitsPerPixel)
	{
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
	}

	if (mod && (xf86LoadSubModule(pScrn, mod) == NULL))
	{
		SMI_FreeRec(pScrn);
		LEAVE_PROC("SMI_PreInit");
		return(FALSE);
	}

	xf86LoaderReqSymbols(reqSym, NULL);
#endif
	/* Load XAA if needed */
	if (!pSmi->NoAccel || pSmi->hwcursor)
	{
		if (!xf86LoadSubModule(pScrn, "xaa"))
		{
			SMI_FreeRec(pScrn);
			LEAVE_PROC("SMI_PreInit");
			return(FALSE);
		}
		xf86LoaderReqSymLists(xaaSymbols, NULL);
	}

	/* Load ramdac if needed */
	if (pSmi->hwcursor)
	{
		if (!xf86LoadSubModule(pScrn, "ramdac"))
		{
			SMI_FreeRec(pScrn);
			LEAVE_PROC("SMI_PreInit");
			return(FALSE);
		}
		xf86LoaderReqSymLists(ramdacSymbols, NULL);
	}

	if (pSmi->shadowFB)
	{
		if (!xf86LoadSubModule(pScrn, "shadowfb"))
		{
			SMI_FreeRec(pScrn);
			LEAVE_PROC("SMI_PreInit");
			return(FALSE);
		}
		xf86LoaderReqSymLists(shadowSymbols, NULL);
	}

	LEAVE_PROC("SMI_PreInit");
	return(TRUE);
}

/*
 * This is called when VT switching back to the X server.  Its job is to
 * reinitialise the video mode. We may wish to unmap video/MMIO memory too.
 */

static Bool
SMI_EnterVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	SMIPtr pSmi = SMIPTR(pScrn);
	Bool ret;

	ENTER_PROC("SMI_EnterVT");

	/* Enable MMIO and map memory */
	SMI_MapMem(pScrn);
	SMI_Save(pScrn);

	/* #670 */
	if (pSmi->shadowFB)
	{
		pSmi->FBOffset = pSmi->savedFBOffset;
		pSmi->FBReserved = pSmi->savedFBReserved;
	}

	ret = SMI_ModeInit(pScrn, pScrn->currentMode);

	/* #670 */
	if (ret && pSmi->shadowFB)
	{
		BoxRec box;

		/* #920 */
		if (pSmi->paletteBuffer)
		{
			int i;

			VGAOUT8(pSmi, VGA_DAC_WRITE_ADDR, 0);
			for(i = 0; i < 256 * 3; i++)
			{
				VGAOUT8(pSmi, VGA_DAC_DATA, pSmi->paletteBuffer[i]);
			}
			xfree(pSmi->paletteBuffer);
			pSmi->paletteBuffer = NULL;
		}

		if (pSmi->pSaveBuffer)
		{
			memcpy(pSmi->FBBase, pSmi->pSaveBuffer, pSmi->saveBufferSize);
			xfree(pSmi->pSaveBuffer);
			pSmi->pSaveBuffer = NULL;
		}

		box.x1 = 0;
		box.y1 = 0;
		box.x2 = pScrn->virtualY;
		box.y2 = pScrn->virtualX;
		if (pSmi->Chipset == SMI_COUGAR3DR)
		{
			SMI_RefreshArea730(pScrn, 1, &box);
		}
		else
		{
			SMI_RefreshArea(pScrn, 1, &box);
		}
	}

	/* Reset the grapics engine */
	if (!pSmi->NoAccel)
	    SMI_EngineReset(pScrn);

	LEAVE_PROC("SMI_EnterVT");
	return(ret);
}

/*
 * This is called when VT switching away from the X server.  Its job is to
 * restore the previous (text) mode. We may wish to remap video/MMIO memory
 * too.
 */

static void
SMI_LeaveVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	vgaRegPtr vgaSavePtr = &hwp->SavedReg;
	SMIRegPtr SMISavePtr = &pSmi->SavedReg;

	ENTER_PROC("SMI_LeaveVT");

	/* #670 */
	if (pSmi->shadowFB)
	{
		pSmi->pSaveBuffer = xnfalloc(pSmi->saveBufferSize);
		if (pSmi->pSaveBuffer)
		{
			memcpy(pSmi->pSaveBuffer, pSmi->FBBase, pSmi->saveBufferSize);
		}

		pSmi->savedFBOffset = pSmi->FBOffset;
		pSmi->savedFBReserved = pSmi->FBReserved;

		/* #920 */
		if (pSmi->Bpp == 1)
		{
			pSmi->paletteBuffer = xnfalloc(256 * 3);
			if (pSmi->paletteBuffer)
			{
				int i;

				VGAOUT8(pSmi, VGA_DAC_READ_ADDR, 0);
				for (i = 0; i < 256 * 3; i++)
				{
					pSmi->paletteBuffer[i] = VGAIN8(pSmi, VGA_DAC_DATA);
				}
			}
		}
	}

	memset(pSmi->FBBase, 0, 256 * 1024);	/* #689 */
	SMI_WriteMode(pScrn, vgaSavePtr, SMISavePtr);
	SMI_UnmapMem(pScrn);

	LEAVE_PROC("SMI_LeaveVT");
}

/*
 * This function performs the inverse of the restore function: It saves all the
 * standard and extended registers that we are going to modify to set up a video
 * mode.
 */

static void
SMI_Save(ScrnInfoPtr pScrn)
{
	int i;
	CARD32 offset;

	vgaHWPtr hwp         = VGAHWPTR(pScrn);
	vgaRegPtr vgaSavePtr = &hwp->SavedReg;
	SMIPtr pSmi          = SMIPTR(pScrn);
	SMIRegPtr save       = &pSmi->SavedReg;

	int vgaIOBase  = hwp->IOBase;
	int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
	int vgaCRData  = vgaIOBase + VGA_CRTC_DATA_OFFSET;

	ENTER_PROC("SMI_Save");

	/* Save the standard VGA registers */
	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_ALL);
	save->smiDACMask = VGAIN8(pSmi, VGA_DAC_MASK);
	VGAOUT8(pSmi, VGA_DAC_READ_ADDR, 0);
	for (i = 0; i < 256; i++)
	{
		save->smiDacRegs[i][0] = VGAIN8(pSmi, VGA_DAC_DATA);
		save->smiDacRegs[i][1] = VGAIN8(pSmi, VGA_DAC_DATA);
		save->smiDacRegs[i][2] = VGAIN8(pSmi, VGA_DAC_DATA);
	}
	for (i = 0, offset = 2; i < 8192; i++, offset += 8)
	{
		save->smiFont[i] = *(pSmi->FBBase + offset);
	}

	/* Now we save all the extended registers we need. */
	save->SR17 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x17);
	save->SR18 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18);
	save->SR21 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
	save->SR31 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
	save->SR32 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x32);
	save->SR6A = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6A);
	save->SR6B = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6B);
	save->SR81 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81);
	save->SRA0 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0xA0);

	if (SMI_LYNXM_SERIES(pSmi->Chipset))
	{
		/* Save primary registers */
		save->CR90[14] = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E);
		VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E,
				save->CR90[14] & ~0x20);

		for (i = 0; i < 16; i++)
		{
			save->CR90[i] = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x90 + i);
		}
		save->CR33 = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x33);
		save->CR3A = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x3A);
		for (i = 0; i < 14; i++)
		{
			save->CR40[i] = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x40 + i);
		}

		/* Save secondary registers */
		VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E, save->CR90[14] | 0x20);
		save->CR33_2 = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x33);
		for (i = 0; i < 14; i++)
		{
			save->CR40_2[i] = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData,
					0x40 + i);
		}
		save->CR9F_2 = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9F);

		/* Save common registers */
		for (i = 0; i < 14; i++)
		{
			save->CRA0[i] = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0xA0 + i);
		}

		/* PDR#1069 */
		VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E, save->CR90[14]);
	}
	else
	{
		save->CR33 = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x33);
		save->CR3A = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x3A);
		for (i = 0; i < 14; i++)
		{
			save->CR40[i] = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x40 + i);
		}
	}

	/* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
	if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR)) {
		save->CCR66 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66);
	}
	/* end CZ */

	save->DPR10 = READ_DPR(pSmi, 0x10);
	save->DPR1C = READ_DPR(pSmi, 0x1C);
	save->DPR20 = READ_DPR(pSmi, 0x20);
	save->DPR24 = READ_DPR(pSmi, 0x24);
	save->DPR28 = READ_DPR(pSmi, 0x28);
	save->DPR2C = READ_DPR(pSmi, 0x2C);
	save->DPR30 = READ_DPR(pSmi, 0x30);
	save->DPR3C = READ_DPR(pSmi, 0x3C);
	save->DPR40 = READ_DPR(pSmi, 0x40);
	save->DPR44 = READ_DPR(pSmi, 0x44);

	save->VPR00 = READ_VPR(pSmi, 0x00);
	save->VPR0C = READ_VPR(pSmi, 0x0C);
	save->VPR10 = READ_VPR(pSmi, 0x10);

	if (pSmi->Chipset == SMI_COUGAR3DR)
	{
		save->FPR00_ = READ_FPR(pSmi, FPR00); 
		save->FPR0C_ = READ_FPR(pSmi, FPR0C);
		save->FPR10_ = READ_FPR(pSmi, FPR10);
	}

	save->CPR00 = READ_CPR(pSmi, 0x00);

	if (!pSmi->ModeStructInit)
	{
		/* XXX Should check the return value of vgaHWCopyReg() */
		vgaHWCopyReg(&hwp->ModeReg, vgaSavePtr);
		memcpy(&pSmi->ModeReg, save, sizeof(SMIRegRec));
		pSmi->ModeStructInit = TRUE;
	}

	if (pSmi->useBIOS && (pSmi->pInt10 != NULL))
	{
		pSmi->pInt10->num = 0x10;
		pSmi->pInt10->ax = 0x0F00;
		xf86ExecX86int10(pSmi->pInt10);
		save->mode = pSmi->pInt10->ax & 0x007F;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Current mode 0x%02X.\n",
				save->mode);
	}

	if (xf86GetVerbosity() > 1)
	{
		xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
				"Saved current video mode.  Register dump:\n");
		SMI_PrintRegs(pScrn);
	}

	LEAVE_PROC("SMI_Save");
}

/*
 * This function is used to restore a video mode. It writes out all of the
 * standard VGA and extended registers needed to setup a video mode.
 */

static void
SMI_WriteMode(ScrnInfoPtr pScrn, vgaRegPtr vgaSavePtr, SMIRegPtr restore)
{
	int i;
	CARD8 tmp;
	CARD32 offset;

	vgaHWPtr hwp   = VGAHWPTR(pScrn);
	SMIPtr pSmi    = SMIPTR(pScrn);
	int vgaIOBase  = hwp->IOBase;
	int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
	int vgaCRData  = vgaIOBase + VGA_CRTC_DATA_OFFSET;

	ENTER_PROC("SMI_WriteMode");

	vgaHWProtect(pScrn, TRUE);

	/* Wait for engine to become idle */
	WaitIdle();

	if (pSmi->useBIOS && (pSmi->pInt10 != NULL)
	    && (restore->mode != 0))
	{
		pSmi->pInt10->num = 0x10;
		pSmi->pInt10->ax = restore->mode | 0x80;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Setting mode 0x%02X\n",
				restore->mode);
		xf86ExecX86int10(pSmi->pInt10);

		/* Enable linear mode. */
		outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
		tmp = inb(pSmi->PIOBase + VGA_SEQ_DATA);
		outb(pSmi->PIOBase + VGA_SEQ_DATA, tmp | 0x01);

		/* Enable DPR/VPR registers. */
		tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03);
	}
	else
	{
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x17, restore->SR17);
		tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18) & ~0x1F;
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18, tmp |
				(restore->SR18 & 0x1F));
		tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03);
		tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31) & ~0xC0;
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp |
				(restore->SR31 & 0xC0));
		tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x32) & ~0x07;
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x32, tmp |
				(restore->SR32 & 0x07));
		if (restore->SR6B != 0xFF)
		{
			VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6A,
					restore->SR6A);
			VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6B,
					restore->SR6B);
		}
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, restore->SR81);
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0xA0, restore->SRA0);

		/* Restore the standard VGA registers */
		vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_ALL);
		if (restore->smiDACMask)
		{
			VGAOUT8(pSmi, VGA_DAC_MASK, restore->smiDACMask);
		}
		else
		{
			VGAOUT8(pSmi, VGA_DAC_MASK, 0xFF);
		}
		VGAOUT8(pSmi, VGA_DAC_WRITE_ADDR, 0);
		for (i = 0; i < 256; i++)
		{
			VGAOUT8(pSmi, VGA_DAC_DATA, restore->smiDacRegs[i][0]);
			VGAOUT8(pSmi, VGA_DAC_DATA, restore->smiDacRegs[i][1]);
			VGAOUT8(pSmi, VGA_DAC_DATA, restore->smiDacRegs[i][2]);
		}
		for (i = 0, offset = 2; i < 8192; i++, offset += 8)
		{
			*(pSmi->FBBase + offset) = restore->smiFont[i];
		}

		if (SMI_LYNXM_SERIES(pSmi->Chipset))
		{
			/* Restore secondary registers */
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E,
					restore->CR90[14] | 0x20);

			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x33, restore->CR33_2);
			for (i = 0; i < 14; i++)
			{
				VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x40 + i,
						restore->CR40_2[i]);
			}
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9F, restore->CR9F_2);

			/* Restore primary registers */
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E,
					restore->CR90[14] & ~0x20);

			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x33, restore->CR33);
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x3A, restore->CR3A);
			for (i = 0; i < 14; i++)
			{
				VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x40 + i,
						restore->CR40[i]);
			}
			for (i = 0; i < 16; i++)
			{
				if (i != 14)
				{
					VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x90 + i,
							restore->CR90[i]);
				}
			}
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E, restore->CR90[14]);

			/* Restore common registers */
			for (i = 0; i < 14; i++)
			{
				VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0xA0 + i,
						restore->CRA0[i]);
			}
		}

		/* Restore the standard VGA registers */
		if (xf86IsPrimaryPci(pSmi->PciInfo)) {
		    vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_CMAP 
				 | VGA_SR_FONTS);
		} 

		if (restore->modeInit)
		    vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_ALL);

		if (!SMI_LYNXM_SERIES(pSmi->Chipset))
		{
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x33, restore->CR33);
			VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x3A, restore->CR3A);
			for (i = 0; i < 14; i++)
			{
				VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x40 + i,
						restore->CR40[i]);
			}
		}
	}

	/* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
	if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR)) {
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66, restore->CCR66);
	}
	/* end CZ */

	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, 0x00);

	WRITE_DPR(pSmi, 0x10, restore->DPR10);
	WRITE_DPR(pSmi, 0x1C, restore->DPR1C);
	WRITE_DPR(pSmi, 0x20, restore->DPR20);
	WRITE_DPR(pSmi, 0x24, restore->DPR24);
	WRITE_DPR(pSmi, 0x28, restore->DPR28);
	WRITE_DPR(pSmi, 0x2C, restore->DPR2C);
	WRITE_DPR(pSmi, 0x30, restore->DPR30);
	WRITE_DPR(pSmi, 0x3C, restore->DPR3C);
	WRITE_DPR(pSmi, 0x40, restore->DPR40);
	WRITE_DPR(pSmi, 0x44, restore->DPR44);

	WRITE_VPR(pSmi, 0x00, restore->VPR00);
	WRITE_VPR(pSmi, 0x0C, restore->VPR0C);
	WRITE_VPR(pSmi, 0x10, restore->VPR10);

	if(pSmi->Chipset == SMI_COUGAR3DR)
	{
		WRITE_FPR(pSmi, FPR00, restore->FPR00_);
		WRITE_FPR(pSmi, FPR0C, restore->FPR0C_);
		WRITE_FPR(pSmi, FPR10, restore->FPR10_);
	}

	WRITE_CPR(pSmi, 0x00, restore->CPR00);

	if (xf86GetVerbosity() > 1)
	{
		xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
				"Done restoring mode.  Register dump:\n");
		SMI_PrintRegs(pScrn);
	}

	vgaHWProtect(pScrn, FALSE);

	LEAVE_PROC("SMI_WriteMode");
}

static Bool
SMI_MapMem(ScrnInfoPtr pScrn)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	vgaHWPtr hwp;
	CARD32 memBase;

	ENTER_PROC("SMI_MapMem");

	/* Map the Lynx register space */
	switch (pSmi->Chipset)
	{
		default:
			memBase = pSmi->PciInfo->memBase[0] + 0x400000;
			pSmi->MapSize = 0x10000;
			break;

		case SMI_COUGAR3DR:
			memBase = pSmi->PciInfo->memBase[1];
			pSmi->MapSize = 0x200000;
			break;

		case SMI_LYNX3D:
			memBase = pSmi->PciInfo->memBase[0] + 0x680000;
			pSmi->MapSize = 0x180000;
			break;

		case SMI_LYNXEM:
		case SMI_LYNXEMplus:
			memBase = pSmi->PciInfo->memBase[0] + 0x400000;
			pSmi->MapSize = 0x400000;
			break;

		case SMI_LYNX3DM:
			memBase = pSmi->PciInfo->memBase[0];
			pSmi->MapSize = 0x200000;
			break;
	}
	pSmi->MapBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pSmi->PciTag,
			memBase, pSmi->MapSize);

	if (pSmi->MapBase == NULL)
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Internal error: could not map "
				"MMIO registers.\n");
		LEAVE_PROC("SMI_MapMem");
		return(FALSE);
	}

	switch (pSmi->Chipset)
	{
		default:
			pSmi->DPRBase = pSmi->MapBase + 0x8000;
			pSmi->VPRBase = pSmi->MapBase + 0xC000;
			pSmi->CPRBase = pSmi->MapBase + 0xE000;
			pSmi->IOBase  = NULL;
			pSmi->DataPortBase = pSmi->MapBase;
			pSmi->DataPortSize = 0x8000;
			break;

		case SMI_COUGAR3DR:
			pSmi->DPRBase = pSmi->MapBase + 0x000000;
			pSmi->VPRBase = pSmi->MapBase + 0x000800;
			pSmi->CPRBase = pSmi->MapBase + 0x001000;
			pSmi->FPRBase = pSmi->MapBase + 0x005800;
			pSmi->IOBase  = pSmi->MapBase + 0x0C0000;
			pSmi->DataPortBase = pSmi->MapBase + 0x100000;
			pSmi->DataPortSize = 0x100000;
			break;

		case SMI_LYNX3D:
			pSmi->DPRBase = pSmi->MapBase + 0x000000;
			pSmi->VPRBase = pSmi->MapBase + 0x000800;
			pSmi->CPRBase = pSmi->MapBase + 0x001000;
			pSmi->IOBase  = pSmi->MapBase + 0x040000;
			pSmi->DataPortBase = pSmi->MapBase + 0x080000;
			pSmi->DataPortSize = 0x100000;
			break;

		case SMI_LYNXEM:
		case SMI_LYNXEMplus:
			pSmi->DPRBase = pSmi->MapBase + 0x008000;
			pSmi->VPRBase = pSmi->MapBase + 0x00C000;
			pSmi->CPRBase = pSmi->MapBase + 0x00E000;
			pSmi->IOBase  = pSmi->MapBase + 0x300000;
			pSmi->DataPortBase = pSmi->MapBase /*+ 0x100000*/;
			pSmi->DataPortSize = 0x8000 /*0x200000*/;
			break;

		case SMI_LYNX3DM:
			pSmi->DPRBase = pSmi->MapBase + 0x000000;
			pSmi->VPRBase = pSmi->MapBase + 0x000800;
			pSmi->CPRBase = pSmi->MapBase + 0x001000;
			pSmi->IOBase  = pSmi->MapBase + 0x0C0000;
			pSmi->DataPortBase = pSmi->MapBase + 0x100000;
			pSmi->DataPortSize = 0x100000;
			break;
	}
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"Physical MMIO at 0x%08lX\n", (unsigned long)memBase);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"Logical MMIO at %p - %p\n", pSmi->MapBase,
			pSmi->MapBase + pSmi->MapSize - 1);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"DPR=%p, VPR=%p, IOBase=%p\n",
			pSmi->DPRBase, pSmi->VPRBase, pSmi->IOBase);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"DataPort=%p - %p\n", pSmi->DataPortBase,
			pSmi->DataPortBase + pSmi->DataPortSize - 1);

	/* Map the frame buffer */
	if (pSmi->Chipset == SMI_LYNX3DM)
	{
		pScrn->memPhysBase = pSmi->PciInfo->memBase[0] + 0x200000;
	}
	else
	{
		pScrn->memPhysBase = pSmi->PciInfo->memBase[0];
	}
	if (pSmi->videoRAMBytes)
	{
		pSmi->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				pSmi->PciTag, pScrn->memPhysBase, pSmi->videoRAMBytes);

		if (pSmi->FBBase == NULL)
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Internal error: could not "
					"map framebuffer.\n");
			LEAVE_PROC("SMI_MapMem");
			return(FALSE);
		}
	}
	pSmi->FBOffset = pScrn->fbOffset = 0;
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"Physical frame buffer at 0x%08lX\n", pScrn->memPhysBase);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"Logical frame buffer at %p - %p\n", pSmi->FBBase,
			pSmi->FBBase + pSmi->videoRAMBytes - 1);

	SMI_EnableMmio(pScrn);

	/* Set up offset to hwcursor memory area.  It's a 1K chunk at the end of
	 * the frame buffer.  Also set up the reserved memory space.
	 */
	pSmi->FBCursorOffset = pSmi->videoRAMBytes - 1024;
	if (VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x30) & 0x01)/* #1074 */
	{
		CARD32 fifoOffset = 0;
		fifoOffset |= VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x46)
				<< 3;
		fifoOffset |= VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x47)
				<< 11;
		fifoOffset |= (VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x49)
				& 0x1C) << 17;
		pSmi->FBReserved = fifoOffset;	/* PDR#1074 */
	}
	else
	{
		pSmi->FBReserved = pSmi->videoRAMBytes - 2048;
	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Cursor Offset: %08lX Reserved: %08lX\n",
			(unsigned long)pSmi->FBCursorOffset,
			   (unsigned long)pSmi->FBReserved);
	/* panel size detection ... requires BIOS call on 730 hardware */
	if (pSmi->Chipset == SMI_COUGAR3DR)
	{
		if (pSmi->pInt10 != NULL)
		{
			pSmi->pInt10->num = 0x10;
			pSmi->pInt10->ax  = 0x5F00;
			pSmi->pInt10->bx  = 0;
			pSmi->pInt10->cx  = 0;
			pSmi->pInt10->dx  = 0;
			xf86ExecX86int10(pSmi->pInt10);
			if (pSmi->pInt10->ax == 0x005F)
			{
				switch (pSmi->pInt10->cx & 0x0F)
				{
					case PANEL_640x480:
						pSmi->lcdWidth  = 640;
						pSmi->lcdHeight = 480;
						break;
					
					case PANEL_800x600:
						pSmi->lcdWidth  = 800;
						pSmi->lcdHeight = 600;
						break;
					
					case PANEL_1024x768:
						pSmi->lcdWidth  = 1024;
						pSmi->lcdHeight = 768;
						break;
					
					case PANEL_1280x1024:
						pSmi->lcdWidth  = 1280;
						pSmi->lcdHeight = 1024;
						break;
					
					case PANEL_1600x1200:
						pSmi->lcdWidth  = 1600;
						pSmi->lcdHeight = 1200;
						break;
					
					case PANEL_1400x1050:
						pSmi->lcdWidth  = 1400;
						pSmi->lcdHeight = 1050;
						break;
				}
	
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Detected panel size via BIOS: %d x %d\n",
					   pSmi->lcdWidth, pSmi->lcdHeight);
			}
			else
			{
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS error during 730 panel detection!\n");
				pSmi->lcdWidth  = pScrn->virtualX;
				pSmi->lcdHeight = pScrn->virtualY;
			}
		}
		else 
		{
			/* int10 support isn't setup on the second call to this function,
			   so if this is the second call, don't do detection again */
			if (pSmi->lcd == 0)
			{
				/* If we get here, int10 support is not loaded or not working */ 
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "No BIOS support for 730 panel detection!\n");
				pSmi->lcdWidth  = pScrn->virtualX;
				pSmi->lcdHeight = pScrn->virtualY;
			}
		}

		/* Set this to indicate that we've done the detection */
		pSmi->lcd = 1;
	}
	else /* panel size detection for hardware other than 730 */
	{
		pSmi->lcd = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31) & 0x01;
	
		if (VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x30) & 0x01)
		{
			pSmi->lcd <<= 1;
		}
		switch (VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x30) & 0x0C)
		{
			case 0x00:
				pSmi->lcdWidth  = 640;
				pSmi->lcdHeight = 480;
				break;
	
			case 0x04:
				pSmi->lcdWidth  = 800;
				pSmi->lcdHeight = 600;
				break;
	
			case 0x08:
				if (VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x74) & 0x02)
				{
					pSmi->lcdWidth  = 1024;
					pSmi->lcdHeight = 600;
				}
				else
				{
					pSmi->lcdWidth  = 1024;
					pSmi->lcdHeight = 768;
				}
				break;
	
			case 0x0C:
				pSmi->lcdWidth  = 1280;
				pSmi->lcdHeight = 1024;
				break;
		}
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s Panel Size = %dx%d\n",
			(pSmi->lcd == 0) ? "OFF" : (pSmi->lcd == 1) ? "TFT" : "DSTN",
			pSmi->lcdWidth, pSmi->lcdHeight);

	/* Assign hwp->MemBase & IOBase here */
	hwp = VGAHWPTR(pScrn);
	if (pSmi->IOBase != NULL)
	{
		vgaHWSetMmioFuncs(hwp, pSmi->MapBase, pSmi->IOBase - pSmi->MapBase);
	}
	vgaHWGetIOBase(hwp);

	/* Map the VGA memory when the primary video */
	if (xf86IsPrimaryPci(pSmi->PciInfo))
	{
		hwp->MapSize = 0x10000;
		if (!vgaHWMapMem(pScrn))
		{
			LEAVE_PROC("SMI_MapMem");
			return(FALSE);
		}
		pSmi->PrimaryVidMapped = TRUE;
	}

	LEAVE_PROC("SMI_MapMem");
	return(TRUE);
}

/* UnMapMem - contains half of pre-4.0 EnterLeave function.  The EnterLeave
 * function which en/disable access to IO ports and ext. regs
 */

static void
SMI_UnmapMem(ScrnInfoPtr pScrn)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_UnmapMem");

	/* Unmap VGA mem if mapped. */
	if (pSmi->PrimaryVidMapped)
	{
		vgaHWUnmapMem(pScrn);
		pSmi->PrimaryVidMapped = FALSE;
	}

	SMI_DisableMmio(pScrn);

	xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pSmi->MapBase, pSmi->MapSize);
	if (pSmi->FBBase != NULL)
	{
		xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pSmi->FBBase,
				pSmi->videoRAMBytes);
	}

	LEAVE_PROC("SMI_UnmapMem");
}

/* This gets called at the start of each server generation. */

static Bool
SMI_ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	SMIPtr pSmi = SMIPTR(pScrn);
	EntityInfoPtr pEnt;
	
	ENTER_PROC("SMI_ScreenInit");

	/* Map MMIO regs and framebuffer */
	if (!SMI_MapMem(pScrn))
	{
		LEAVE_PROC("SMI_ScreenInit");
		return(FALSE);
	}

	pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
	
	if (!pSmi->pInt10) {
	    pSmi->pInt10 = xf86InitInt10(pEnt->index);
	}

	/* Save the chip/graphics state */
	SMI_Save(pScrn);
	
	/* Zero the frame buffer, #258 */
	memset(pSmi->FBBase, 0, pSmi->videoRAMBytes);

	/* Initialize the first mode */
	if (!SMI_ModeInit(pScrn, pScrn->currentMode))
	{
		LEAVE_PROC("SMI_ScreenInit");
		return(FALSE);
	}

	/*
	 * The next step is to setup the screen's visuals, and initialise the
	 * framebuffer code.  In cases where the framebuffer's default choises for
	 * things like visual layouts and bits per RGB are OK, this may be as simple
	 * as calling the framebuffer's ScreenInit() function.  If not, the visuals
	 * will need to be setup before calling a fb ScreenInit() function and fixed
	 * up after.
	 *
	 * For most PC hardware at depths >= 8, the defaults that cfb uses are not
	 * appropriate.  In this driver, we fixup the visuals after.
	 */

	/*
	 * Reset the visual list.
	 */
	miClearVisualTypes();

	/* Setup the visuals we support. */

	/*
	 * For bpp > 8, the default visuals are not acceptable because we only
	 * support TrueColor and not DirectColor.  To deal with this, call
	 * miSetVisualTypes with the appropriate visual mask.
	 */
#ifndef USE_FB
	if (pScrn->bitsPerPixel > 8)
	{
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
				pScrn->defaultVisual))
		{
			LEAVE_PROC("SMI_ScreenInit");
			return(FALSE);
		}
	}
	else
#endif
	{
		if (!miSetVisualTypes(pScrn->depth,
				miGetDefaultVisualMask(pScrn->depth), pScrn->rgbBits,
				pScrn->defaultVisual))
		{
			LEAVE_PROC("SMI_ScreenInit");
			return(FALSE);
		}
	}
#ifdef USE_FB
	if (!miSetPixmapDepths ()) return FALSE;
#endif

	if (!SMI_InternalScreenInit(scrnIndex, pScreen))
	{
		LEAVE_PROC("SMI_ScreenInit");
		return(FALSE);
	}

	xf86SetBlackWhitePixels(pScreen);

	if (pScrn->bitsPerPixel > 8)
	{
		VisualPtr visual;
		/* Fixup RGB ordering */
		visual = pScreen->visuals + pScreen->numVisuals;
		while (--visual >= pScreen->visuals)
		{
			if ((visual->class | DynamicClass) == DirectColor)
			{
				visual->offsetRed   = pScrn->offset.red;
				visual->offsetGreen = pScrn->offset.green;
				visual->offsetBlue  = pScrn->offset.blue;
				visual->redMask     = pScrn->mask.red;
				visual->greenMask   = pScrn->mask.green;
				visual->blueMask    = pScrn->mask.blue;
			}
		}
	}

#ifdef USE_FB
	/* must be after RGB ordering fixed */
	fbPictureInit(pScreen, 0, 0);
#endif
 
	/* CZ 18.06.2001: moved here from smi_accel.c to have offscreen
	   framebuffer in NoAccel mode */
	{
	    int numLines, maxLines;
	    BoxRec AvailFBArea;
 
	    maxLines = pSmi->FBReserved / (pSmi->width * pSmi->Bpp);
	    if (pSmi->rotate) {
		numLines = maxLines;
	    } else {
		/* CZ 3.11.2001: What does the following code? see also smi_video.c aaa line 1226 */
/*#if SMI_USE_VIDEO */
#if 0
		numLines = ((pSmi->FBReserved - pSmi->width * pSmi->Bpp
			     * pSmi->height) * 25 / 100 + pSmi->width
			    * pSmi->Bpp - 1) / (pSmi->width * pSmi->Bpp);
		numLines += pSmi->height;
#else
		numLines = maxLines;
#endif
	    }
 
	    AvailFBArea.x1 = 0;
	    AvailFBArea.y1 = 0;
	    AvailFBArea.x2 = pSmi->width;
	    AvailFBArea.y2 = numLines;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "FrameBuffer Box: %d,%d - %d,%d\n",
		       AvailFBArea.x1, AvailFBArea.y1, AvailFBArea.x2,
		       AvailFBArea.y2);
	    xf86InitFBManager(pScreen, &AvailFBArea);
	}
	/* end CZ */
	
	
	/* Initialize acceleration layer */
	if (!pSmi->NoAccel) {
		if (!SMI_AccelInit(pScreen)) {
		    LEAVE_PROC("SMI_ScreenInit");
		    return(FALSE);
		}
	}
	
	miInitializeBackingStore(pScreen);
	
	/* hardware cursor needs to wrap this layer */
  	SMI_DGAInit(pScreen);
	
	/* Initialise cursor functions */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	/* Initialize HW cursor layer.  Must follow software cursor
	 * initialization.
	 */
	if (pSmi->hwcursor)
	{
		if (!SMI_HWCursorInit(pScreen))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Hardware cursor "
					"initialization failed\n");
		}
	}

	if (pSmi->shadowFB)
	{
		RefreshAreaFuncPtr refreshArea;

		if (pSmi->Chipset == SMI_COUGAR3DR)
		{
			refreshArea = SMI_RefreshArea730;
		}
		else
		{
			refreshArea = SMI_RefreshArea;
		}

		if (pSmi->rotate)
		{
			if (pSmi->PointerMoved == NULL)
			{
				pSmi->PointerMoved  = pScrn->PointerMoved;
				pScrn->PointerMoved = SMI_PointerMoved;
			}
		}

		ShadowFBInit(pScreen, refreshArea);
	}

	/* Initialise default colormap */
	if (!miCreateDefColormap(pScreen))
	{
		LEAVE_PROC("SMI_ScreenInit");
		return(FALSE);
	}

	/* Initialize colormap layer.  Must follow initialization of the default
	 * colormap.  And SetGamma call, else it will load palette with solid white.
	 */
    /* CZ 2.11.2001: CMAP_PALETTED_TRUECOLOR for gamma correction */
    if (!xf86HandleColormaps(pScreen, 256, pScrn->rgbBits, SMI_LoadPalette, NULL,
            CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR))
	{
		LEAVE_PROC("SMI_ScreenInit");
		return(FALSE);
	}

	pScreen->SaveScreen = SMI_SaveScreen;
	pSmi->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = SMI_CloseScreen;

	if (!xf86DPMSInit(pScreen, SMI_DisplayPowerManagementSet, 0))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "DPMS initialization failed!\n");
	}

  	SMI_InitVideo(pScreen);

	/* Report any unused options (only for the first generation) */
	if (serverGeneration == 1)
	{
		xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
	}
	
	LEAVE_PROC("SMI_ScreenInit");
	return(TRUE);
}

/* Common init routines needed in EnterVT and ScreenInit */

static int
SMI_InternalScreenInit(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	SMIPtr pSmi = SMIPTR(pScrn);
	int width, height, displayWidth;
	int bytesPerPixel = pScrn->bitsPerPixel / 8;
	int xDpi, yDpi;
	int ret;

	ENTER_PROC("SMI_InternalScreenInit");

	if (pSmi->rotate)
	{
		width        = pScrn->virtualY;
		height       = pScrn->virtualX;
		xDpi         = pScrn->yDpi;
		yDpi         = pScrn->xDpi;
		displayWidth = ((width * bytesPerPixel + 15) & ~15) / bytesPerPixel;
	}
	else
	{
		width        = pScrn->virtualX;
		height       = pScrn->virtualY;
		xDpi		 = pScrn->xDpi;
		yDpi		 = pScrn->yDpi;
		displayWidth = pScrn->displayWidth;
	}

	if (pSmi->shadowFB)
	{
		pSmi->ShadowWidth      = width;
		pSmi->ShadowHeight     = height;
		pSmi->ShadowWidthBytes = (width * bytesPerPixel + 15) & ~15;
		if (bytesPerPixel == 3)
		{
			pSmi->ShadowPitch = ((height * 3) << 16)
							  | pSmi->ShadowWidthBytes;
		}
		else
		{
			pSmi->ShadowPitch = (height << 16)
							  | (pSmi->ShadowWidthBytes / bytesPerPixel);
		}

		pSmi->saveBufferSize = pSmi->ShadowWidthBytes * pSmi->ShadowHeight;
		pSmi->FBReserved -= pSmi->saveBufferSize;
		pSmi->FBReserved &= ~0x15;
		WRITE_VPR(pSmi, 0x0C, (pSmi->FBOffset = pSmi->FBReserved) >> 3);
		if(pSmi->Chipset == SMI_COUGAR3DR)
		{
			WRITE_FPR(pSmi, FPR0C, (pSmi->FBOffset = pSmi->FBReserved) >> 3);
		}

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"Shadow: width=%d height=%d "
				"offset=0x%08lX pitch=0x%08X\n",
				pSmi->ShadowWidth, pSmi->ShadowHeight,
				(unsigned long)pSmi->FBOffset,
				pSmi->ShadowPitch);
	}
	else
	{
		pSmi->FBOffset = 0;
	}

	/*
	 * Call the framebuffer layer's ScreenInit function, and fill in other
	 * pScreen fields.
	 */

	DEBUG((VERBLEV, "\tInitializing FB @ 0x%08X for %dx%d (%d)\n",
			pSmi->FBBase, width, height, displayWidth));
	switch (pScrn->bitsPerPixel)
	{
#ifdef USE_FB
	case 8:
	case 16:
	case 24:
	  ret = fbScreenInit(pScreen, pSmi->FBBase, width, height, xDpi,
			     yDpi, displayWidth,pScrn->bitsPerPixel);
	  break;
#else
	case 8:
	  ret = cfbScreenInit(pScreen, pSmi->FBBase, width, height, xDpi,
			      yDpi, displayWidth);
	  break;

	case 16:
	  ret = cfb16ScreenInit(pScreen, pSmi->FBBase, width, height, xDpi,
					yDpi, displayWidth);
	  break;

	case 24:
	  ret = cfb24ScreenInit(pScreen, pSmi->FBBase, width, height, xDpi,
				yDpi, displayWidth);
	  break;
#endif
	default:
	  xf86DrvMsg(scrnIndex, X_ERROR, "Internal error: invalid bpp (%d) "
		     "in SMI_InternalScreenInit\n", pScrn->bitsPerPixel);
	  LEAVE_PROC("SMI_InternalScreenInit");
	  return(FALSE);
	}
	
	LEAVE_PROC("SMI_InternalScreenInit");
	return(ret);
}

/* Checks if a mode is suitable for the selected configuration. */
static ModeStatus
SMI_ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	SMIPtr pSmi = SMIPTR(pScrn);
	float refresh;

	ENTER_PROC("SMI_ValidMode");
	refresh = (mode->VRefresh > 0) ? mode->VRefresh 
	  : mode->Clock * 1000.0 / mode->VTotal / mode->HTotal;
	xf86DrvMsg(scrnIndex, X_INFO, "Mode: %dx%d %d-bpp, %fHz\n", mode->HDisplay,
			mode->VDisplay, pScrn->bitsPerPixel, refresh);

	if (pSmi->shadowFB)
	{
		int mem;

		if (pScrn->bitsPerPixel == 24)
		{
			LEAVE_PROC("SMI_ValidMode");
			return(MODE_BAD);
		}

		mem  = (pScrn->virtualX * pScrn->bitsPerPixel / 8 + 15) & ~15;
		mem *= pScrn->virtualY * 2;

		if (mem > pSmi->FBReserved)	/* PDR#1074 */
		{
			LEAVE_PROC("SMI_ValidMode");
			return(MODE_MEM);
		}
	}

	if (!pSmi->useBIOS || pSmi->lcd)
	{
#if 1 /* PDR#983 */
		if (pSmi->zoomOnLCD)
		{
			if (   (mode->HDisplay > pSmi->lcdWidth)
				|| (mode->VDisplay > pSmi->lcdHeight)
			)
			{
				LEAVE_PROC("SMI_ValidMode");
				return(MODE_PANEL);
			}
		}
		else
#endif
		{
			if (   (mode->HDisplay != pSmi->lcdWidth)
				|| (mode->VDisplay != pSmi->lcdHeight)
			)
			{
				LEAVE_PROC("SMI_ValidMode");
				return(MODE_PANEL);
			}
		}

	}

#if 1 /* PDR#944 */
	if (pSmi->rotate)
	{
		if (   (mode->HDisplay != pSmi->lcdWidth)
			|| (mode->VDisplay != pSmi->lcdHeight)
		)
		{
			LEAVE_PROC("SMI_ValidMode");
			return(MODE_PANEL);
		}
	}
#endif

	LEAVE_PROC("SMI_ValidMode");
	return(MODE_OK);
}

static Bool
SMI_ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	unsigned char tmp;
	int panelIndex, modeIndex, i;
	int xyAddress[] = { 320, 400, 512, 640, 800, 1024, 1280, 1600, 2048 };
	CARD32 DEDataFormat = 0;

	/* Store values to current mode register structs */
	SMIRegPtr new = &pSmi->ModeReg;
	vgaRegPtr vganew = &hwp->ModeReg;
	
	ENTER_PROC("SMI_ModeInit");

	if(!vgaHWInit(pScrn, mode))
	{
		LEAVE_PROC("SMI_ModeInit");
		return(FALSE);
	}

	new->modeInit = TRUE;

	if (pSmi->rotate)
	{
		pSmi->width  = pScrn->virtualY;
		pSmi->height = pScrn->virtualX;
	}
	else
	{
		pSmi->width  = pScrn->virtualX;
		pSmi->height = pScrn->virtualY;
	}
	pSmi->Bpp    = pScrn->bitsPerPixel / 8;

	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x17);
	tmp = inb(pSmi->PIOBase + VGA_SEQ_DATA);
	if (pSmi->pci_burst)
	{
		new->SR17 = tmp | 0x20;
	}
	else
	{
		new->SR17 = tmp & ~0x20;
	}

	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
	new->SR18 = inb(pSmi->PIOBase + VGA_SEQ_DATA) | 0x11;

	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x21);
	new->SR21 = inb(pSmi->PIOBase + VGA_SEQ_DATA) & ~0x03;

	if (pSmi->Chipset != SMI_COUGAR3DR)
	{
		outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x31);
		new->SR31 = inb(pSmi->PIOBase + VGA_SEQ_DATA) & ~0xC0;

		outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x32);
		new->SR32 = inb(pSmi->PIOBase + VGA_SEQ_DATA) & ~0x07;

		if (SMI_LYNXM_SERIES(pSmi->Chipset))
		{
			new->SR32 |= 0x04;
		}
	}

	new->SRA0 = new->CR33 = new->CR3A = 0x00;

	if (pSmi->lcdWidth == 640)
	{
		panelIndex = 0;
	}
	else if (pSmi->lcdWidth == 800)
	{
		panelIndex = 1;
	}
	else
	{
		panelIndex = 2;
	}

	if (mode->HDisplay == 640)
	{
		modeIndex = 0;
	}
	else if (mode->HDisplay == 800)
	{
		modeIndex = 1;
	}
	else
	{
		modeIndex = 2;
	}

	if (SMI_LYNXM_SERIES(pSmi->Chipset))
	{
		static unsigned char PanelTable[3][14] =
		{
			{ 0x5F, 0x4F, 0x00, 0x52, 0x1E, 0x0B, 0xDF, 0x00, 0xE9, 0x0B, 0x2E,
			  0x00, 0x4F, 0xDF },
			{ 0x7F, 0x63, 0x00, 0x69, 0x19, 0x72, 0x57, 0x00, 0x58, 0x0C, 0xA2,
			  0x20, 0x4F, 0xDF },
			{ 0xA3, 0x7F, 0x00, 0x83, 0x14, 0x24, 0xFF, 0x00, 0x02, 0x08, 0xA7,
			  0xE0, 0x4F, 0xDF },
		};

		for (i = 0; i < 14; i++)
		{
			new->CR40[i] = PanelTable[panelIndex][i];
		}
		new->CR90[14] = 0x03;
		new->CR90[15] = 0x00;
		if (mode->VDisplay < pSmi->lcdHeight)
		{
			new->CRA0[6] = (pSmi->lcdHeight - mode->VDisplay) / 8;
		}
		else
		{
			new->CRA0[6] = 0;
		}

		if (mode->HDisplay < pSmi->lcdWidth)
		{
			new->CRA0[7] = (pSmi->lcdWidth - mode->HDisplay) / 16;
		}
		else
		{
			new->CRA0[7] = 0;
		}
	}
	else
	{
		static unsigned char PanelTable[3][3][14] =
		{
			{ /* 640x480 panel */
		        { 0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
		          0x2E, 0x00, 0x4F, 0xDF },
		        { 0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
		          0x2E, 0x00, 0x4F, 0xDF },
		        { 0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
		          0x2E, 0x00, 0x4F, 0xDF },
			},
			{ /* 800x600 panel */
		        { 0x7F, 0x59, 0x19, 0x5E, 0x8E, 0x72, 0x1C, 0x37, 0x1D, 0x00,
		          0xA2, 0x20, 0x4F, 0xDF },
		        { 0x7F, 0x63, 0x00, 0x68, 0x18, 0x72, 0x58, 0x00, 0x59, 0x0C,
		          0xE0, 0x20, 0x63, 0x57 },
		        { 0x7F, 0x63, 0x00, 0x68, 0x18, 0x72, 0x58, 0x00, 0x59, 0x0C,
		          0xE0, 0x20, 0x63, 0x57 },
			},
			{ /* 1024x768 panel */
		        { 0xA3, 0x67, 0x0F, 0x6D, 0x1D, 0x24, 0x70, 0x95, 0x72, 0x07,
		          0xA3, 0x20, 0x4F, 0xDF },
		        { 0xA3, 0x71, 0x19, 0x77, 0x07, 0x24, 0xAC, 0xD1, 0xAE, 0x03,
		          0xE1, 0x20, 0x63, 0x57 },
		        { 0xA3, 0x7F, 0x00, 0x85, 0x15, 0x24, 0xFF, 0x00, 0x01, 0x07,
		          0xE5, 0x20, 0x7F, 0xFF },
			},
		};

		for (i = 0; i < 14; i++)
		{
			new->CR40[i] = PanelTable[panelIndex][modeIndex][i];
		}
	}

	/* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
	new->CCR66 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66);
	if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR)){
	    switch (pScrn->bitsPerPixel) {
	    case 8:
		new->CCR66 = (new->CCR66 & 0xF3) | 0x00; /* 6 bits-RAM */
		break;
	    case 16:
		new->CCR66 = (new->CCR66 & 0xF3) | 0x00; /* 6 bits-RAM */
		/* no Gamma correction in 16 Bit mode (s. Release.txt 1.3.1) */
		break;
	    case 24:
	    case 32:
		new->CCR66 = (new->CCR66 & 0xF3) | 0x04; /* Gamma correct ON */
		break;
	    default:
		LEAVE_PROC("SMI_ModeInit");
		return(FALSE);
	    }
	}

	if (pSmi->Chipset != SMI_COUGAR3DR)
	{
		outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x30);
		if (inb(pSmi->PIOBase + VGA_SEQ_DATA) & 0x01)
		{
			new->SR21 = 0x00;
		}
	}

	if (pSmi->MCLK > 0)
	{
		SMI_CommonCalcClock(pScrn->scrnIndex, pSmi->MCLK,
				    1, 1, 31, 0, 2, pSmi->minClock,
				    pSmi->maxClock, &new->SR6A, &new->SR6B);
	}
	else
	{
		new->SR6B = 0xFF;
	}

	if ((mode->HDisplay == 640) && SMI_LYNXM_SERIES(pSmi->Chipset))
	{
		vganew->MiscOutReg &= ~0x0C;
	}
	else
	{
		vganew->MiscOutReg |= 0x0C;
	}
	vganew->MiscOutReg |= 0xE0;
	if (mode->HDisplay == 800)
	{
		vganew->MiscOutReg &= ~0xC0;
	}
	if ((mode->HDisplay == 1024) && SMI_LYNXM_SERIES(pSmi->Chipset))
	{
		vganew->MiscOutReg &= ~0xC0;
	}

	/* Set DPR registers */
	pSmi->Stride = (pSmi->width * pSmi->Bpp + 15) & ~15;
	switch (pScrn->bitsPerPixel)
	{
		case 8:
			DEDataFormat = 0x00000000;
			break;

		case 16:
			pSmi->Stride >>= 1;
			DEDataFormat = 0x00100000;
			break;

		case 24:
			DEDataFormat = 0x00300000;
			break;

		case 32:
			pSmi->Stride >>= 2;
			DEDataFormat = 0x00200000;
			break;
	}
	for (i = 0; i < sizeof(xyAddress) / sizeof(xyAddress[0]); i++)
	{
		if (pSmi->rotate)
		{
			if (xyAddress[i] == pSmi->height)
			{
				DEDataFormat |= i << 16;
				break;
			}
		}
		else
		{
			if (xyAddress[i] == pSmi->width)
			{
				DEDataFormat |= i << 16;
				break;
			}
		}
	}
	new->DPR10 = (pSmi->Stride << 16) | pSmi->Stride;
	new->DPR1C = DEDataFormat;
	new->DPR20 = 0;
	new->DPR24 = 0xFFFFFFFF;
	new->DPR28 = 0xFFFFFFFF;
	new->DPR2C = 0;
	new->DPR30 = 0;
	new->DPR3C = (pSmi->Stride << 16) | pSmi->Stride;
	new->DPR40 = 0;
	new->DPR44 = 0;

	/* Set VPR registers (and FPR registers for SM731) */
	switch (pScrn->bitsPerPixel)
	{
		case 8:
			new->VPR00 = 0x00000000;
			new->FPR00_= 0x00080000;
			break;

		case 16:
			new->VPR00 = 0x00020000;
			new->FPR00_= 0x000A0000;
			break;

		case 24:
			new->VPR00 = 0x00040000;
			new->FPR00_= 0x000C0000;
			break;

		case 32:
			new->VPR00 = 0x00030000;
			new->FPR00_= 0x000B0000;
			break;
	}
	new->VPR0C = pSmi->FBOffset >> 3;
	if (pSmi->rotate)
	{
		new->VPR10 = (((( pSmi->height * pSmi->Bpp) >> 3)
				+ 2) << 16) | ((pSmi->height * pSmi->Bpp) >> 3);
	}
	else
	{
		new->VPR10 = ((((pSmi->width * pSmi->Bpp) >> 3)
				+ 2) << 16) | ((pSmi->width * pSmi->Bpp) >> 3);
	}

	new->FPR0C_ = new->VPR0C;
	new->FPR10_ = new->VPR10;

	/* Set CPR registers */
	new->CPR00 = 0x00000000;

	pScrn->vtSema = TRUE;

	/* Find the INT 10 mode number */
	{
		static struct
		{
			int x, y, bpp;
			CARD16 mode;

		} modeTable[] =
		{
			{  640,  480,  8, 0x50 },
			{  640,  480, 16, 0x52 },
			{  640,  480, 24, 0x53 },
			{  640,  480, 32, 0x54 },
			{  800,  600,  8, 0x55 },
			{  800,  600, 16, 0x57 },
			{  800,  600, 24, 0x58 },
			{  800,  600, 32, 0x59 },
			{ 1024,  768,  8, 0x60 },
			{ 1024,  768, 16, 0x62 },
			{ 1024,  768, 24, 0x63 },
			{ 1024,  768, 32, 0x64 },
			{ 1280, 1024,  8, 0x65 },
			{ 1280, 1024, 16, 0x67 },
			{ 1280, 1024, 24, 0x68 },
			{ 1280, 1024, 32, 0x69 },
		};

		new->mode = 0;
		for (i = 0; i < sizeof(modeTable) / sizeof(modeTable[0]); i++)
		{
			if (   (modeTable[i].x == mode->HDisplay)
				&& (modeTable[i].y == mode->VDisplay)
				&& (modeTable[i].bpp == pScrn->bitsPerPixel)
			)
			{
				new->mode = modeTable[i].mode;
				break;
			}
		}
	}

	/* Zero the font memory */
	memset(new->smiFont, 0, sizeof(new->smiFont));

	/* Write the mode registers to hardware */
	SMI_WriteMode(pScrn, vganew, new);

	/* Adjust the viewport */
	SMI_AdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

	LEAVE_PROC("SMI_ModeInit");
	return(TRUE);
}

/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free any
 * per-generation data allocated by the driver.  It should finish by unwrapping
 * and calling the saved CloseScreen function.
 */

static Bool
SMI_CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	vgaRegPtr vgaSavePtr = &hwp->SavedReg;
	SMIRegPtr SMISavePtr = &pSmi->SavedReg;
	Bool ret;
	
	ENTER_PROC("SMI_CloseScreen");

	if (pScrn->vtSema)
	{
		SMI_WriteMode(pScrn, vgaSavePtr, SMISavePtr);
		vgaHWLock(hwp);
		SMI_UnmapMem(pScrn);
	}

	if (pSmi->AccelInfoRec != NULL)
	{
		XAADestroyInfoRec(pSmi->AccelInfoRec);
	}
	if (pSmi->CursorInfoRec != NULL)
	{
		xf86DestroyCursorInfoRec(pSmi->CursorInfoRec);
	}
	if (pSmi->DGAModes != NULL)
	{
		xfree(pSmi->DGAModes);
	}
	if (pSmi->pInt10 != NULL)
	{
		xf86FreeInt10(pSmi->pInt10);
		pSmi->pInt10 = NULL;
	}
	if (pSmi->ptrAdaptor != NULL)
	{
		xfree(pSmi->ptrAdaptor);
	}
	if (pSmi->BlockHandler != NULL)
	{
		pScreen->BlockHandler = pSmi->BlockHandler;
	}
	/* #670 */
	if (pSmi->pSaveBuffer)
	{
		xfree(pSmi->pSaveBuffer);
	}
	/* #920 */
	if (pSmi->paletteBuffer)
	{
		xfree(pSmi->paletteBuffer);
	}

	pScrn->vtSema = FALSE;
	pScreen->CloseScreen = pSmi->CloseScreen;
	ret = (*pScreen->CloseScreen)(scrnIndex, pScreen);

	LEAVE_PROC("SMI_CloseScreen");
	return(ret);
}

static void
SMI_FreeScreen(int scrnIndex, int flags)
{
	SMI_FreeRec(xf86Screens[scrnIndex]);
}

static Bool
SMI_SaveScreen(ScreenPtr pScreen, int mode)
{
	Bool ret;

	ENTER_PROC("SMI_SaveScreen");

	ret = vgaHWSaveScreen(pScreen, mode);

	LEAVE_PROC("SMI_SaveScreen");
	return(ret);
}

void
SMI_AdjustFrame(int scrnIndex, int x, int y, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD32 Base;

	ENTER_PROC("SMI_AdjustFrame");

	if (pSmi->ShowCache && y)
	{
		y += pScrn->virtualY - 1;
	}

	Base = pSmi->FBOffset + (x + y * pScrn->virtualX) * pSmi->Bpp;
	if (SMI_LYNX3D_SERIES(pSmi->Chipset) || SMI_COUGAR_SERIES(pSmi->Chipset))
	{
		Base = (Base + 15) & ~15;
#if 1 /* PDR#1058 */
		while ((Base % pSmi->Bpp) > 0)
		{
			Base -= 16;
		}
#endif
	}
	else
	{
		Base = (Base + 7) & ~7;
#if 1 /* PDR#1058 */
		while ((Base % pSmi->Bpp) > 0)
		{
			Base -= 8;
		}
#endif
	}

	WRITE_VPR(pSmi, 0x0C, Base >> 3);
	if(pSmi->Chipset == SMI_COUGAR3DR)
	{
		WRITE_FPR(pSmi, FPR0C, Base >> 3);
	}

	LEAVE_PROC("SMI_AdjustFrame");
}

Bool
SMI_SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
	Bool ret;

	ENTER_PROC("SMI_SwitchMode");

	ret = SMI_ModeInit(xf86Screens[scrnIndex], mode);

	LEAVE_PROC("SMI_SwitchMode");
	return(ret);
}

void
SMI_LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies, LOCO *colors,
				VisualPtr pVisual)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	int i;

	ENTER_PROC("SMI_LoadPalette");

	/* Enable both the CRT and LCD DAC RAM paths, so both palettes are updated */
    if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR)) 
	{
		CARD8 ccr66;

		ccr66  = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66);
		ccr66 &= 0x0f;
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66, ccr66);
	}

	for(i = 0; i < numColors; i++)
	{
        DEBUG((VERBLEV, "pal[%d] = %d %d %d\n", indicies[i],
           colors[indicies[i]].red, colors[indicies[i]].green, colors[indicies[i]].blue));
		VGAOUT8(pSmi, VGA_DAC_WRITE_ADDR, indicies[i]);
		VGAOUT8(pSmi, VGA_DAC_DATA, colors[indicies[i]].red);
		VGAOUT8(pSmi, VGA_DAC_DATA, colors[indicies[i]].green);
		VGAOUT8(pSmi, VGA_DAC_DATA, colors[indicies[i]].blue);
	}

	LEAVE_PROC("SMI_LoadPalette");
}

static void
SMI_DisableVideo(ScrnInfoPtr pScrn)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD8 tmp;

	if (!(tmp = VGAIN8(pSmi, VGA_DAC_MASK)))
	  return;
	pSmi->DACmask = tmp;
	VGAOUT8(pSmi, VGA_DAC_MASK, 0);
}

static void
SMI_EnableVideo(ScrnInfoPtr pScrn)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	VGAOUT8(pSmi, VGA_DAC_MASK, pSmi->DACmask);
}


void
SMI_EnableMmio(ScrnInfoPtr pScrn)
{
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD8 tmp;

	ENTER_PROC("SMI_EnableMmio");

	/*
	 * Enable chipset (seen on uninitialized secondary cards) might not be
	 * needed once we use the VGA softbooter
	 */
	vgaHWSetStdFuncs(hwp);

	/* Enable linear mode */
	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
	tmp = inb(pSmi->PIOBase + VGA_SEQ_DATA);
	pSmi->SR18Value = tmp;					/* PDR#521 */
	outb(pSmi->PIOBase + VGA_SEQ_DATA, tmp | 0x11);

	/* Enable 2D/3D Engine and Video Processor */
	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x21);
	tmp = inb(pSmi->PIOBase + VGA_SEQ_DATA);
	pSmi->SR21Value = tmp;					/* PDR#521 */
	outb(pSmi->PIOBase + VGA_SEQ_DATA, tmp & ~0x03);

	LEAVE_PROC("SMI_EnableMmio");
}

void
SMI_DisableMmio(ScrnInfoPtr pScrn)
{
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_DisableMmio");

	vgaHWSetStdFuncs(hwp);

	/* Disable 2D/3D Engine and Video Processor */
	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x21);
	outb(pSmi->PIOBase + VGA_SEQ_DATA, pSmi->SR21Value);	/* PDR#521 */

	/* Disable linear mode */
	outb(pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
	outb(pSmi->PIOBase + VGA_SEQ_DATA, pSmi->SR18Value);	/* PDR#521 */

	LEAVE_PROC("SMI_DisableMmio");
}

/* This function is used to debug, it prints out the contents of Lynx regs */
static void
SMI_PrintRegs(ScrnInfoPtr pScrn)
{
	unsigned char i;
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	int vgaCRIndex = hwp->IOBase + VGA_CRTC_INDEX_OFFSET;
	int vgaCRReg   = hwp->IOBase + VGA_CRTC_DATA_OFFSET;
	int vgaStatus  = hwp->IOBase + VGA_IN_STAT_1_OFFSET;

	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"START register dump ------------------\n");

	xf86ErrorFVerb(VERBLEV, "MISCELLANEOUS OUTPUT\n    %02X\n",
			VGAIN8(pSmi, VGA_MISC_OUT_R));

	xf86ErrorFVerb(VERBLEV, "\nSEQUENCER\n"
			"    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
	for (i = 0x00; i <= 0xAF; i++)
	{
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		if ((i & 0x3) == 0x0) xf86ErrorFVerb(VERBLEV, " ");
		xf86ErrorFVerb(VERBLEV, "%02X ",
				VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, i));
	}

	xf86ErrorFVerb(VERBLEV, "\n\nCRT CONTROLLER\n"
			"    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
	for (i = 0x00; i <= 0xAD; i++)
	{
		if (i == 0x20) i = 0x30;
		if (i == 0x50) i = 0x90;
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		if ((i & 0x3) == 0x0) xf86ErrorFVerb(VERBLEV, " ");
		xf86ErrorFVerb(VERBLEV, "%02X ",
				VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRReg, i));
	}

	xf86ErrorFVerb(VERBLEV, "\n\nGRAPHICS CONTROLLER\n"
			"    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
	for (i = 0x00; i <= 0x08; i++)
	{
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		if ((i & 0x3) == 0x0) xf86ErrorFVerb(VERBLEV, " ");
		xf86ErrorFVerb(VERBLEV, "%02X ",
				VGAIN8_INDEX(pSmi, VGA_GRAPH_INDEX, VGA_GRAPH_DATA, i));
	}

	xf86ErrorFVerb(VERBLEV, "\n\nATTRIBUTE 0CONTROLLER\n"
			"    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
	for (i = 0x00; i <= 0x14; i++)
	{
		(void) VGAIN8(pSmi, vgaStatus);
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		if ((i & 0x3) == 0x0) xf86ErrorFVerb(VERBLEV, " ");
		xf86ErrorFVerb(VERBLEV, "%02X ",
				VGAIN8_INDEX(pSmi, VGA_ATTR_INDEX, VGA_ATTR_DATA_R, i));
	}
	(void) VGAIN8(pSmi, vgaStatus);
	VGAOUT8(pSmi, VGA_ATTR_INDEX, 0x20);

	xf86ErrorFVerb(VERBLEV, "\n\nDPR    x0       x4       x8       xC");
	for (i = 0x00; i <= 0x44; i += 4)
	{
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		xf86ErrorFVerb(VERBLEV, " %08lX", (unsigned long)READ_DPR(pSmi, i));
	}

	xf86ErrorFVerb(VERBLEV, "\n\nVPR    x0       x4       x8       xC");
	for (i = 0x00; i <= 0x60; i += 4)
	{
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		xf86ErrorFVerb(VERBLEV, " %08lX", (unsigned long)READ_VPR(pSmi, i));
	}

	xf86ErrorFVerb(VERBLEV, "\n\nCPR    x0       x4       x8       xC");
	for (i = 0x00; i <= 0x18; i += 4)
	{
		if ((i & 0xF) == 0x0) xf86ErrorFVerb(VERBLEV, "\n%02X|", i);
		xf86ErrorFVerb(VERBLEV, " %08lX", (unsigned long)READ_CPR(pSmi, i));
	}

	xf86ErrorFVerb(VERBLEV, "\n\n");
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
			"END register dump --------------------\n");
}

/*
 * SMI_DisplayPowerManagementSet -- Sets VESA Display Power Management
 * Signaling (DPMS) Mode.
 */
static void
SMI_DisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
							  int flags)
{
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD8 SR01, SR20, SR21, SR22, SR23, SR24, SR31, SR34;

	ENTER_PROC("SMI_DisplayPowerManagementSet");

	/* If we already are in the requested DPMS mode, just return */
	if (pSmi->CurrentDPMS == PowerManagementMode)
	{
		LEAVE_PROC("SMI_DisplayPowerManagementSet");
		return;
	}

#if 1 /* PDR#735 */
	if (pSmi->pInt10 != NULL)
	{
		pSmi->pInt10->ax = 0x4F10;
		switch (PowerManagementMode)
		{
			case DPMSModeOn:
				pSmi->pInt10->bx = 0x0001;
				break;

			case DPMSModeStandby:
				pSmi->pInt10->bx = 0x0101;
				break;

			case DPMSModeSuspend:
				pSmi->pInt10->bx = 0x0201;
				break;

			case DPMSModeOff:
				pSmi->pInt10->bx = 0x0401;
				break;
		}
		pSmi->pInt10->cx = 0x0000;
		pSmi->pInt10->num = 0x10;
		xf86ExecX86int10(pSmi->pInt10);
		if (pSmi->pInt10->ax == 0x004F)
		{
			pSmi->CurrentDPMS = PowerManagementMode;
#if 1 /* PDR#835 */
			if (PowerManagementMode == DPMSModeOn)
			{
				SR01 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01);
				VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01,
						SR01 & ~0x20);
			}
#endif
			LEAVE_PROC("SMI_DisplayPowerManagementSet");
			return;
		}
	}
#endif

	/* Save the current SR registers */
	if (pSmi->CurrentDPMS == DPMSModeOn)
	{
		pSmi->DPMS_SR20 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x20);
		pSmi->DPMS_SR21 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
		pSmi->DPMS_SR31 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
		pSmi->DPMS_SR34 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x34);
	}

	/* Read the required SR registers for the DPMS handler */
	SR01 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01);
	SR20 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x20);
	SR21 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
	SR22 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x22);
	SR23 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x23);
	SR24 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x24);
	SR31 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
	SR34 = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x34);

	switch (PowerManagementMode)
	{
		case DPMSModeOn:
			/* Screen On: HSync: On, VSync : On */
			SR01 &= ~0x20;
			SR20  = pSmi->DPMS_SR20;
			SR21  = pSmi->DPMS_SR21;
			SR22 &= ~0x30;
			SR23 &= ~0xC0;
			SR24 |= 0x01;
			SR31  = pSmi->DPMS_SR31;
			SR34  = pSmi->DPMS_SR34;
			break;

		case DPMSModeStandby:
			/* Screen: Off; HSync: Off, VSync: On */
			SR01 |= 0x20;
			SR20  = (SR20 & ~0xB0) | 0x10;
			SR21 |= 0x88;
			SR22  = (SR22 & ~0x30) | 0x10;
			SR23  = (SR23 & ~0x07) | 0xD8;
			SR24 &= ~0x01;
			SR31  = (SR31 & ~0x07) | 0x00;
			SR34 |= 0x80;
			break;

		case DPMSModeSuspend:
			/* Screen: Off; HSync: On, VSync: Off */
			SR01 |= 0x20;
			SR20  = (SR20 & ~0xB0) | 0x10;
			SR21 |= 0x88;
			SR22  = (SR22 & ~0x30) | 0x20;
			SR23  = (SR23 & ~0x07) | 0xD8;
			SR24 &= ~0x01;
			SR31  = (SR31 & ~0x07) | 0x00;
			SR34 |= 0x80;
			break;

		case DPMSModeOff:
			/* Screen: Off; HSync: Off, VSync: Off */
			SR01 |= 0x20;
			SR20  = (SR20 & ~0xB0) | 0x10;
			SR21 |= 0x88;
			SR22  = (SR22 & ~0x30) | 0x30;
			SR23  = (SR23 & ~0x07) | 0xD8;
			SR24 &= ~0x01;
			SR31  = (SR31 & ~0x07) | 0x00;
			SR34 |= 0x80;
			break;

		default:
			xf86ErrorFVerb(VERBLEV, "Invalid PowerManagementMode %d passed to "
					"SMI_DisplayPowerManagementSet\n", PowerManagementMode);
			LEAVE_PROC("SMI_DisplayPowerManagementSet");
			return;
	}

	/* Wait for vertical retrace */
	while (hwp->readST01(hwp) & 0x8) ;
	while (!(hwp->readST01(hwp) & 0x8)) ;

	/* Write the registers */
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01, SR01);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x34, SR34);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, SR31);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x20, SR20);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x22, SR22);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x23, SR23);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, SR21);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x24, SR24);

	/* Save the current power state */
	pSmi->CurrentDPMS = PowerManagementMode;

	LEAVE_PROC("SMI_DisplayPowerManagementSet");
}

static void
SMI_ProbeDDC(ScrnInfoPtr pScrn, int index)
{
	vbeInfoPtr pVbe;
	if (xf86LoadSubModule(pScrn, "vbe"))
	{
		pVbe = VBEInit(NULL, index);
		ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
		vbeFree(pVbe);
	}
}

static unsigned int
SMI_ddc1Read(ScrnInfoPtr pScrn)
{
	register vgaHWPtr hwp = VGAHWPTR(pScrn);
	SMIPtr pSmi = SMIPTR(pScrn);
	unsigned int ret;

	ENTER_PROC("SMI_ddc1Read");

	while (hwp->readST01(hwp) & 0x8) ;
	while (!(hwp->readST01(hwp) & 0x8)) ;

	ret = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72) & 0x08;

	LEAVE_PROC("SMI_ddc1Read");
	return(ret);
}

static Bool
SMI_ddc1(int scrnIndex)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	SMIPtr pSmi = SMIPTR(pScrn);
	Bool success = FALSE;
	xf86MonPtr pMon;
	unsigned char tmp;

	ENTER_PROC("SMI_ddc1");

	tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72, tmp | 0x20);

	pMon = xf86PrintEDID(xf86DoEDID_DDC1(scrnIndex, vgaHWddc1SetSpeed,
			SMI_ddc1Read));
	if (pMon != NULL)
	{
		success = TRUE;
	}
	xf86SetDDCproperties(pScrn, pMon);

	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72, tmp);

	LEAVE_PROC("SMI_ddc1");
	return(success);
}
