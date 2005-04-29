/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/imstt/imstt_driver.c,v 1.21 2003/08/23 15:03:02 dawes Exp $ */

/*
 *	Copyright 2000	Ani Joshi <ajoshi@unixbox.com>
 *
 *	XFree86 4.0 driver for the Integrated Micro Solutions
 *		Twin Turbo 128 chipset
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation and
 * that the name of Ani Joshi not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Ani Joshi makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * ANI JOSHI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ANI JOSHI BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *	Credits:
 *		Sigurdur Asgeirsson, Jeffrey Kuskin, Ryan Nielsen
 *		for their work on imsttfb
 *
 */


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86Version.h"
#include "xf86Resources.h"
#include "xf86fbman.h"
#include "compiler.h"
#include "xaa.h"
#include "mipointer.h"
#include "micmap.h"
#include "mibstore.h"
#include "fbdevhw.h"

#include "fb.h"

#include "xf86cmap.h"

#include "imstt.h"
#include "imstt_reg.h"


/* To get it to build on non-PPC */
#ifndef __powerpc__
#define eieio()
#endif

/*
 * prototypes
 */
static const OptionInfoRec * IMSTTAvailableOptions(int chipid, int busid);
static void IMSTTIdentify(int flags);
static Bool IMSTTProbe(DriverPtr drv, int flags);
static Bool IMSTTPreInit(ScrnInfoPtr pScrn, int flags);

#if 0
static Bool IMSTTEnterVT(int scrnIndex, int flags);
static void IMSTTLeaveVT(int scrnIndex, int flags);
static void IMSTTSave(ScrnInfoPtr pScrn);
#endif
static Bool IMSTTScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
			    char **argv);
#if 0
static int IMSTTInternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static ModeStatus IMSTTValidMode(int index, DisplayModePtr mode,
				 Bool verbose, int flags);
#endif

static Bool IMSTTMapMem(ScrnInfoPtr pScrn);
static void IMSTTUnmapMem(ScrnInfoPtr pScrn);
static Bool IMSTTModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void IMSTTAdjustFrame(int scrnIndex, int x, int y, int flags);
Bool IMSTTCloseScreen(int scrnIndex, ScreenPtr pScreen);
Bool IMSTTSaveScreen(ScreenPtr pScreen, int mode);
#if 0
static void IMSTTLoadPalette(ScrnInfoPtr pScrn, int numColors,
			     int *indicies, LOCO *colors,
			     VisualPtr pVisual);
#endif
static void IMSTTGetVideoMemSize(ScrnInfoPtr pScrn);
static void IMSTTSetClock(ScrnInfoPtr pScrn, unsigned long mhz);
static void IMSTTWriteMode(ScrnInfoPtr pScrn);

#define DRIVER_NAME	"imstt"
#define DRIVER_VERSION	"1.0.0"
#define VERSION_MAJOR	1
#define VERSION_MINOR	0
#define PATCHLEVEL	0
#define IMSTT_VERSION	((VERSION_MAJOR << 24) | \
			 (VERSION_MINOR << 16) | \
			 PATCHLEVEL)


DriverRec IMSTT =
{
	IMSTT_VERSION,
	DRIVER_NAME,
	IMSTTIdentify,
	IMSTTProbe,
	IMSTTAvailableOptions,
	NULL,
	0
};


/* supported chipsets */
static SymTabRec IMSTTChipsets[] = {
	{ PCI_IMSTT128,	"imstt128" },
	{ PCI_IMSTT3D,	"imstt3d" },
	{ -1,		NULL }
};

static PciChipsets IMSTTPciChipsets[] = {
	{ PCI_IMSTT128,	PCI_IMSTT128,	RES_SHARED_VGA },
	{ PCI_IMSTT3D,	PCI_IMSTT3D,	RES_SHARED_VGA },
	{ -1,		-1,		RES_UNDEFINED }
};

typedef enum {
	OPTION_NOACCEL,
	OPTION_SWCURSOR,
	OPTION_INITDAC,
	OPTION_FBDEV
} IMSTTOpts;

static const OptionInfoRec IMSTTOptions[] =
{
	{ OPTION_NOACCEL, "noaccel", OPTV_BOOLEAN, {0}, FALSE },
	{ OPTION_SWCURSOR, "swcursor", OPTV_BOOLEAN, {0}, FALSE },
	{ OPTION_INITDAC, "initdac", OPTV_BOOLEAN, {0}, FALSE },
	{ OPTION_FBDEV,	"UseFBDev", OPTV_BOOLEAN, {0}, FALSE },
	{ -1, NULL, OPTV_NONE, {0}, FALSE }
};

static const char *fbSymbols[] = {
	"fbPictureInit",
	"fbScreenInit",
	NULL
};

static const char *xaaSymbols[] = {
	"XAACreateInfoRec",
	"XAAInit",
	NULL
};


static const char *fbdevHWSymbols[] = {
	"fbdevHWAdjustFrame",
	"fbdevHWEnterVT",
	"fbdevHWGetVidmem",
	"fbdevHWInit",
	"fbdevHWLeaveVT",
	"fbdevHWLoadPalette",
	"fbdevHWMapVidmem",
	"fbdevHWModeInit",
	"fbdevHWSave",
	"fbdevHWSwitchMode",
	"fbdevHWUnmapMMIO",
	"fbdevHWUnmapVidmem",
	"fbdevHWUseBuildinMode",
	"fbdevHWValidMode",
	NULL
};


#ifdef XFree86LOADER

MODULESETUPPROTO(IMSTTSetup);

/*
pointer IMSTTSetup(pointer module, pointer opts, int *errmaj,
			  int *errmin);
*/

static XF86ModuleVersionInfo IMSTTVersRec = {
	"imstt",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0, 0, 0, 0}
};

XF86ModuleData imsttModuleData = { &IMSTTVersRec, IMSTTSetup, NULL };

pointer IMSTTSetup(pointer module, pointer opts, int *errmaj,
			  int *errmin)
{
	static Bool setupDone = FALSE;

	IMSTTTRACE("IMSTTSetup -- begin\n");
	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&IMSTT, module, 0);
		LoaderRefSymLists(fbSymbols, xaaSymbols, fbdevHWSymbols, NULL);
		return (pointer) 1;
	} else {
		if (errmaj)
			*errmaj = LDR_ONCEONLY;
		return NULL;
	}
	IMSTTTRACE("IMSTTSetup -- end\n");
}


#endif /* XFree86LOADER */


static Bool IMSTTGetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate)
		return TRUE;

	pScrn->driverPrivate = xnfcalloc(sizeof(IMSTTRec), 1);
	return TRUE;
}


static void IMSTTFreeRec(ScrnInfoPtr pScrn)
{
	if (!pScrn->driverPrivate)
		return;

	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
	IMSTTUnmapMem(pScrn);
}


static const OptionInfoRec * IMSTTAvailableOptions(int chipid, int busid)
{
	return IMSTTOptions;
}


static void IMSTTIdentify(int flags)
{
	xf86PrintChipsets("IMSTT", "driver (version " DRIVER_VERSION ") for IMS TwinTurbo chipsets ",
			  IMSTTChipsets);
}


static Bool IMSTTProbe(DriverPtr drv, int flags)
{
	int i;
	GDevPtr *devSections;
	int *usedChips;
	int numDevSections;
	int numUsed;
	Bool foundScreen = FALSE;

	IMSTTTRACE("IMSTTProbe begin\n");
	/* sanity checks */
	if ((numDevSections = xf86MatchDevice("imstt", &devSections)) <= 0)
		return FALSE;
	if (xf86GetPciVideoInfo() == NULL)
		return FALSE;

	numUsed = xf86MatchPciInstances("imstt", PCI_VENDOR_IMS,
					IMSTTChipsets, IMSTTPciChipsets,
					devSections, numDevSections, drv,
					&usedChips);

	xfree(devSections);

	if (numUsed <= 0)
		return FALSE;

	if (flags & PROBE_DETECT)
		foundScreen = TRUE;
	else for (i=0; i<numUsed; i++) {
		ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);

		pScrn->driverVersion = VERSION_MAJOR;
		pScrn->driverName = DRIVER_NAME;
		pScrn->name = "imstt";
		pScrn->Probe = IMSTTProbe;
		pScrn->PreInit = IMSTTPreInit;
		pScrn->ScreenInit = IMSTTScreenInit;
/*		pScrn->SwitchMode = IMSTTSwitchMode; */
		pScrn->AdjustFrame = IMSTTAdjustFrame;
/*		pScrn->EnterVT = IMSTTEnterVT;
		pScrn->LeaveVT = IMSTTLeaveVT; */
		pScrn->FreeScreen = NULL;
/*		pScrn->ValidMode = IMSTTValidMode; */
		foundScreen = TRUE;
		xf86ConfigActivePciEntity(pScrn, usedChips[i], IMSTTPciChipsets,
					  NULL, NULL, NULL, NULL, NULL);
	}


	IMSTTTRACE("IMSTTProbe end\n");

	xfree(usedChips);
	return foundScreen;
}


static Bool IMSTTPreInit(ScrnInfoPtr pScrn, int flags)
{
	EntityInfoPtr pEnt;
	IMSTTPtr iptr;
	int i;
	ClockRangePtr clockRanges;
	rgb zeros = {0, 0, 0};
	Gamma gzeros = {0.0, 0.0, 0.0};


	if (flags & PROBE_DETECT)
		return FALSE;

	pScrn->monitor = pScrn->confScreen->monitor;

	if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support24bppFb | Support32bppFb |
			     SupportConvert32to24 | PreferConvert32to24))
		return FALSE;
	else {
		switch (pScrn->depth) {
		case 8:
		case 15:
		case 16:
		case 24:
		case 32:
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

	if (pScrn->depth > 8) {
		if (!xf86SetWeight(pScrn, zeros, zeros))
			return FALSE;
	}

	pScrn->rgbBits = 8;

	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	pScrn->progClock = TRUE;

	if (!IMSTTGetRec(pScrn))
		return FALSE;
	iptr = IMSTTPTR(pScrn);

	xf86CollectOptions(pScrn, NULL);

	if (!(iptr->Options = xalloc(sizeof(IMSTTOptions))))
		return FALSE;
	memcpy(iptr->Options, IMSTTOptions, sizeof(IMSTTOptions));
	xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, iptr->Options);

	if (xf86ReturnOptValBool(iptr->Options, OPTION_NOACCEL, FALSE)) {
		iptr->NoAccel = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: NoAccel - acceleration disabled\n");
	} else
		iptr->NoAccel = FALSE;
#if 0
	if (xf86ReturnOptValBool(iptr->Options, OPTION_SWCURSOR, FALSE))
		iptr->HWCursor = FALSE;
	else
		iptr->HWCursor = TRUE;
#else
	/* HW cursor support not ready yet... */
	iptr->HWCursor = FALSE;
#endif

	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using %s cursor\n",
		   iptr->HWCursor ? "HW" : "SW");

	if (xf86ReturnOptValBool(iptr->Options, OPTION_INITDAC, FALSE)) {
		iptr->InitDAC = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: not initalizing DACn");
	} else {
		iptr->InitDAC = FALSE;
	}

	if (xf86ReturnOptValBool(iptr->Options, OPTION_FBDEV, FALSE)) {
		iptr->FBDev = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "Using framebuffer device\n");
	} else {
		iptr->FBDev = FALSE;
	}

	/* hack */
	iptr->FBDev = TRUE;

	if (iptr->FBDev) {
		if (!xf86LoadSubModule(pScrn, "fbdevhw"))
			return FALSE;
		xf86LoaderReqSymLists(fbdevHWSymbols, NULL);
		if (!fbdevHWInit(pScrn, iptr->PciInfo, NULL))
			return FALSE;
		pScrn->SwitchMode = fbdevHWSwitchMode;
		pScrn->AdjustFrame = fbdevHWAdjustFrame;
		pScrn->EnterVT = fbdevHWEnterVT;
		pScrn->LeaveVT = fbdevHWLeaveVT;
		pScrn->ValidMode = fbdevHWValidMode;
	}

	if (pScrn->numEntities > 1) {
		IMSTTFreeRec(pScrn);
		return FALSE;
	}

	pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
	if (pEnt->resources) {
		xfree(pEnt);
		IMSTTFreeRec(pScrn);
		return FALSE;
	}

	iptr->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
	xf86RegisterResources(pEnt->index, NULL, ResNone);
	xf86SetOperatingState(resVgaIo, pEnt->index, ResUnusedOpr);
	xf86SetOperatingState(resVgaMem, pEnt->index, ResDisableOpr);

	if (pEnt->device->chipset && *pEnt->device->chipset) {
		pScrn->chipset = pEnt->device->chipset;
		iptr->Chipset = xf86StringToToken(IMSTTChipsets, pScrn->chipset);
	} else if (pEnt->device->chipID >= 0) {
		iptr->Chipset = pEnt->device->chipID;
		pScrn->chipset = (char *)xf86TokenToString(IMSTTChipsets,
							   iptr->Chipset);
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
			   iptr->Chipset);
	} else {
		iptr->Chipset = iptr->PciInfo->chipType;
		pScrn->chipset = (char *)xf86TokenToString(IMSTTChipsets,
							   iptr->Chipset);
	}

	if (pEnt->device->chipRev >= 0) {
		iptr->ChipRev = pEnt->device->chipRev;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
			   iptr->ChipRev);
	} else
		iptr->ChipRev = iptr->PciInfo->chipRev;

	xfree(pEnt);

	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Chipset: \"%s\"\n", pScrn->chipset);

	iptr->PciTag = pciTag(iptr->PciInfo->bus, iptr->PciInfo->device,
			      iptr->PciInfo->func);

	if (!xf86SetGamma(pScrn, gzeros))
		return FALSE;

	if (iptr->Chipset == PCI_IMSTT3D) {
		iptr->ramdac = RAMDAC_TVP;
		iptr->videoRam = 0x800000;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "using %s ramdac\n", iptr->ramdac == RAMDAC_TVP ? "TVP" : "IBM");

	if (!IMSTTMapMem(pScrn)) {
		IMSTTFreeRec(pScrn);
		return FALSE;
	}

	iptr->rev = (INREG(IMSTT_SSTATUS) & 0x0f00) >> 8;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "revision %d\n", iptr->rev);

	if (!pScrn->videoRam) {
		pScrn->videoRam = iptr->videoRam / 1024;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			   "probed videoram = %dk\n",
			    pScrn->videoRam);
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "videoram = %dk\n",
			   pScrn->videoRam / 1024);
	}

	/* XXX this is sorta a guess, got some info from the TVP3030 manual */
	pScrn->numClocks = 2;
	pScrn->clock[0] = 110000;
	pScrn->clock[1] = 220000;

	clockRanges = xnfcalloc(sizeof(ClockRange), 1);
	clockRanges->next = NULL;
	clockRanges->minClock = 20000;
	clockRanges->maxClock = 120000;	/* i don't want to blow up anything */
	clockRanges->clockIndex = -1;
	clockRanges->interlaceAllowed = FALSE;	/* ? */
	clockRanges->doubleScanAllowed = FALSE; /* ? */

	i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			      pScrn->display->modes, clockRanges,
			      NULL, 256, 2048, pScrn->bitsPerPixel,
			      128, 2048, pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      iptr->videoRam, LOOKUP_BEST_REFRESH);

	if (i < 1 && iptr->FBDev) {
		fbdevHWUseBuildinMode(pScrn);
		pScrn->displayWidth = pScrn->virtualX;	/* XXX */
		i = i;
	}

	if (i == -1) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "no valid modes left\n");
		IMSTTFreeRec(pScrn);
		return FALSE;
	}

	xf86PruneDriverModes(pScrn);

	if (i == 0 || pScrn->modes == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
		IMSTTFreeRec(pScrn);
		return FALSE;
	}

	xf86SetCrtcForModes(pScrn, 0);
	pScrn->currentMode = pScrn->modes;
	xf86PrintModes(pScrn);
	xf86SetDpi(pScrn, 0, 0);

	if (!xf86LoadSubModule(pScrn, "fb"))
		return FALSE;

	xf86LoaderReqSymLists(fbSymbols, NULL);

	if (!xf86LoadSubModule(pScrn, "xaa"))
		return FALSE;

	xf86LoaderReqSymLists(xaaSymbols, NULL);

	IMSTTTRACE("PreInit -- END\n");

	return TRUE;
}



static Bool IMSTTMapMem(ScrnInfoPtr pScrn)
{
	IMSTTPtr iptr;

	iptr = IMSTTPTR(pScrn);

	iptr->MMIOBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, iptr->PciTag,
				       iptr->PciInfo->memBase[0] + 0x800000,
				       0x41000);
	if (!iptr->MMIOBase) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Internal error: could not map MMIO\n");
		return FALSE;
	}

	IMSTTTRACE1("Mapped MMIO @ 0x%x with size 0x1000\n", iptr->PciInfo->memBase[0] + 0x800000);

	IMSTTGetVideoMemSize(pScrn);

	if (iptr->FBDev) {
		iptr->FBBase = fbdevHWMapVidmem(pScrn);
	} else {
		iptr->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER, iptr->PciTag,
					     iptr->PciInfo->memBase[0],
					     iptr->videoRam);
	}
	if (!iptr->FBBase) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Internal error: could not map framebuffer\n");
		return FALSE;
	}

	if (iptr->InitDAC) {
		iptr->CMAPBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, iptr->PciTag,
					       iptr->PciInfo->memBase[0] + 0x840000,
					       0x1000);
		if (!iptr->CMAPBase) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Internal error: could not map CMAP\n");
			return FALSE;
		}
	}

	pScrn->fbOffset = 0;

	return TRUE;
}


static void IMSTTUnmapMem(ScrnInfoPtr pScrn)
{
	IMSTTPtr iptr;

	iptr = IMSTTPTR(pScrn);

	if (iptr->FBDev) {
		fbdevHWUnmapMMIO(pScrn);
	} else {
		xf86UnMapVidMem(pScrn->scrnIndex, (pointer)iptr->MMIOBase,
				0x1000);
	}

	if (iptr->InitDAC) {
		xf86UnMapVidMem(pScrn->scrnIndex, (pointer)iptr->CMAPBase,
				0x1000);
	}

	if (iptr->FBDev) {
		fbdevHWUnmapVidmem(pScrn);
	} else {
		xf86UnMapVidMem(pScrn->scrnIndex, (pointer)iptr->FBBase,
				iptr->videoRam);
	}

	return;
}



static void IMSTTGetVideoMemSize(ScrnInfoPtr pScrn)
{
	IMSTTPtr iptr = IMSTTPTR(pScrn);
	unsigned long tmp;

	if (iptr->FBDev) {
		iptr->videoRam = fbdevHWGetVidmem(pScrn);
		return;
	}

	tmp = INREG(IMSTT_PRC);
	if (iptr->ramdac == RAMDAC_IBM)
		iptr->videoRam = (tmp & 0x0004) ? 0x400000 : 0x200000;
	else
		iptr->videoRam = 0x800000;

	return;
}


static Bool IMSTTScreenInit(int scrnIndex, ScreenPtr pScreen,
			    int argc, char **argv)
{
	ScrnInfoPtr pScrn;
	IMSTTPtr iptr;
	unsigned long tmp;
	VisualPtr visual;
	int r = TRUE;

	IMSTTTRACE("ScreenInit -- Begin\n");

	pScrn = xf86Screens[pScreen->myNum];

	iptr = IMSTTPTR(pScrn);

	if (!iptr->FBDev) {
		/* initialize the card */
		tmp = INREG(IMSTT_STGCTL);

		OUTREG(IMSTT_STGCTL, tmp & ~0x1);
		OUTREG(IMSTT_SSR, 0);

		if (iptr->InitDAC) {
			/* set default values for DAC registers */
			if (iptr->ramdac == RAMDAC_IBM) {
				iptr->CMAPBase[IBM624_PPMASK] = 0xff;	eieio();
				iptr->CMAPBase[IBM624_PIDXHI] = 0;	eieio();
				OUTREGPI(IBM624_CLKCTL, 0x21);
				OUTREGPI(IBM624_SYNCCTL, 0x00);
				OUTREGPI(IBM624_HSYNCPOS, 0x00);
				OUTREGPI(IBM624_PWRMNGMT, 0x00);
				OUTREGPI(IBM624_DACOP, 0x02);
				OUTREGPI(IBM624_PALETCTL, 0x00);
				OUTREGPI(IBM624_SYSCLKCTL, 0x01);
				OUTREGPI(IBM624_BPP8, 0x00);
				OUTREGPI(IBM624_BPP16, 0x01);
				OUTREGPI(IBM624_BPP24, 0x00);
				OUTREGPI(IBM624_BPP32, 0x00);
				OUTREGPI(IBM624_PIXCTL1, 0x05);
				OUTREGPI(IBM624_PIXCTL2, 0x00);
				OUTREGPI(IBM624_SYSCLKN, 0x08);
				OUTREGPI(IBM624_SYSCLKM, 0x4f);
				OUTREGPI(IBM624_SYSCLKP, 0x00);	
				OUTREGPI(IBM624_SYSCLKC, 0x00);
				OUTREGPI(IBM624_CURSCTL, 0x00);
				OUTREGPI(IBM624_CURSACCTL, 0x01);
				OUTREGPI(IBM624_CURSACATTR, 0xa8);
				OUTREGPI(IBM624_CURS1R, 0xff);
				OUTREGPI(IBM624_CURS1G, 0xff);
				OUTREGPI(IBM624_CURS1B, 0xff);
				OUTREGPI(IBM624_CURS2R, 0xff);
				OUTREGPI(IBM624_CURS2G, 0xff);
				OUTREGPI(IBM624_CURS2B, 0xff);
				OUTREGPI(IBM624_CURS3R, 0xff);
				OUTREGPI(IBM624_CURS3G, 0xff);
				OUTREGPI(IBM624_CURS3B, 0xff);
				OUTREGPI(IBM624_BORDR, 0xff);
				OUTREGPI(IBM624_BORDG, 0xff);
				OUTREGPI(IBM624_BORDB, 0xff);
				OUTREGPI(IBM624_MISCTL1, 0x01);
				OUTREGPI(IBM624_MISCTL2, 0x45);
				OUTREGPI(IBM624_MISCTL3, 0x00);
				OUTREGPI(IBM624_KEYCTL, 0x00);
			} else {
				OUTREGPT(TVP_IRICC, 0x00);
				OUTREGPT(TVP_IRBRC, 0xe4);
				OUTREGPT(TVP_IRLAC, 0x06);
				OUTREGPT(TVP_IRTCC, 0x80);
				OUTREGPT(TVP_IRMXC, 0x4d);
				OUTREGPT(TVP_IRCLS, 0x05);
				OUTREGPT(TVP_IRPPG, 0x00);
				OUTREGPT(TVP_IRGEC, 0x00);
				OUTREGPT(TVP_IRMIC, 0x08);
				OUTREGPT(TVP_IRCKL, 0xff);
				OUTREGPT(TVP_IRCKH, 0xff);
				OUTREGPT(TVP_IRCRL, 0xff);
				OUTREGPT(TVP_IRCRH, 0xff);
				OUTREGPT(TVP_IRCGL, 0xff);
				OUTREGPT(TVP_IRCGH, 0xff);
				OUTREGPT(TVP_IRCBL, 0xff);
				OUTREGPT(TVP_IRCBH, 0xff);
				OUTREGPT(TVP_IRCKC, 0x00);
				OUTREGPT(TVP_IRPLA, 0x00);
				OUTREGPT(TVP_IRPPD, 0xc0);
				OUTREGPT(TVP_IRPPD, 0xd5);
				OUTREGPT(TVP_IRPPD, 0xea);
				OUTREGPT(TVP_IRPLA, 0x00);
				OUTREGPT(TVP_IRMPD, 0xb9);
				OUTREGPT(TVP_IRMPD, 0x3a);
				OUTREGPT(TVP_IRMPD, 0xb1);
				OUTREGPT(TVP_IRPLA, 0x00);
				OUTREGPT(TVP_IRLPD, 0xc1);
				OUTREGPT(TVP_IRLPD, 0x3d);
				OUTREGPT(TVP_IRLPD, 0xf3);
			}
		}
	}

	iptr->pitch = pScrn->displayWidth;

	if (iptr->FBDev) {
		if (!fbdevHWModeInit(pScrn, pScrn->currentMode))
			return FALSE;
	} else {
		if (!IMSTTModeInit(pScrn, pScrn->currentMode))
			return FALSE;
	}

	pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);	

	miClearVisualTypes();

	if (pScrn->bitsPerPixel > 8) {
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
				      pScrn->rgbBits, TrueColor))
			return FALSE;
	} else {
		if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
				      pScrn->rgbBits, pScrn->defaultVisual))
			return FALSE;
	}

	miSetPixmapDepths ();
	
	r = fbScreenInit(pScreen, iptr->FBBase, pScrn->virtualX,
			 pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
			 pScrn->displayWidth, pScrn->bitsPerPixel);

	if (!r) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "ScreenInit failed\n");
		return FALSE;
	}

	if (pScrn->bitsPerPixel > 8) {
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

	fbPictureInit (pScreen, 0, 0);

	xf86SetBlackWhitePixels(pScreen);
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	if (!iptr->NoAccel) {
		if (IMSTTAccelInit(pScreen)) {
			xf86DrvMsg(scrnIndex, X_INFO, "Acceleration enabled\n");
		} else {
			xf86DrvMsg(scrnIndex, X_ERROR, "Acceleration initailizatino failed\n");
			xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
		}
	} else {
		xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
	}

	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	if (!miCreateDefColormap(pScreen))
		return FALSE;

	if (!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPalette,
				 NULL, CMAP_PALETTED_TRUECOLOR))
		return FALSE;

	if (serverGeneration == 1)
		xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

	pScreen->SaveScreen = IMSTTSaveScreen;
	pScreen->CloseScreen = IMSTTCloseScreen;

	IMSTTTRACE("ScreenInit -- End\n");


	return TRUE;
}




static Bool IMSTTModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode) 
{
	IMSTTPtr iptr;
	unsigned long mhz;
	unsigned short hes, heb, veb, htp, vtp;

	iptr = IMSTTPTR(pScrn);

	if (iptr->ramdac == RAMDAC_IBM) {
		switch (pScrn->displayWidth) {
			case 640:
				hes = 0x0008;
				heb = 0x0012;
				veb = 0x002a;
				htp = 10;
				vtp = 2;
				mhz = 30;
				break;
			case 832:
				hes = 0x0005;
				heb = 0x0020;
				veb = 0x0028;
				htp = 8;
				vtp = 3;
				mhz = 57;
				break;
			case 1024:
				hes = 0x000a;
				heb = 0x001c;
				veb = 0x0020;
				htp = 8;
				vtp = 3;
				mhz = 80;
				break;
			case 1152:
				hes = 0x0012;
				heb = 0x0022;
				veb = 0x0031;
				htp = 4;
				vtp = 3;
				mhz = 101;
				break;
			case 1280:
				hes = 0x0012;
				heb = 0x002f;
				veb = 0x0029;
				htp = 4;
				vtp = 1;
				mhz = 135;	/* XXX check for 1280x960 */
				break;
			case 1600:
				hes = 0x0018;
				heb = 0x0040;
				veb = 0x002a;
				htp = 4;
				vtp = 3;
				mhz = 200;
				break;
			default:
				return FALSE;
		}

		IMSTTSetClock(pScrn, mhz);

		iptr->hes = hes;
		iptr->heb = heb;
		iptr->hsb = heb + (pScrn->displayWidth >> 3);
		iptr->ht = iptr->hsb + htp;
		iptr->ves = 0x0003;
		iptr->veb = veb;
		iptr->vsb = veb + pScrn->virtualY;
		iptr->vt = iptr->vsb + vtp;
		iptr->vil = iptr->vsb;
		iptr->pitch = pScrn->displayWidth;
	} else {
		iptr->pitch = pScrn->displayWidth;
		switch (pScrn->displayWidth) {
			case 640:
				iptr->hes = 0x0004;
				iptr->heb = 0x0009;
				iptr->hsb = 0x0031;
				iptr->ht = 0x0036;
				iptr->ves = 0x0003;
				iptr->veb = 0x002a;
				iptr->vsb = 0x020a;
				iptr->vt = 0x020d;
				iptr->vil = 0x020a;
				iptr->pclk_m = 0xef;
				iptr->pclk_n = 0x2e;
				iptr->pclk_p = 0xb2;
				iptr->mlc[0] = 0x39;
				iptr->mlc[1] = 0x39;
				iptr->mlc[2] = 0x38;
				iptr->lckl_p[0] = 0xf3;
				iptr->lckl_p[1] = 0xf3;
				iptr->lckl_p[2] = 0xf3;
			case 800:
				iptr->hes = 0x0005;
				iptr->heb = 0x000e;
				iptr->hsb = 0x0040;
				iptr->ht = 0x0042;
				iptr->ves = 0x0003;
				iptr->veb = 0x0018;
				iptr->vsb = 0x0270;
				iptr->vt = 0x0271;
				iptr->vil = 0x0270;
				iptr->pclk_m = 0xf6;
				iptr->pclk_n = 0x2e;
				iptr->pclk_p = 0xf2;
				iptr->mlc[0] = 0x3a;
				iptr->mlc[1] = 0x39;
				iptr->mlc[2] = 0x38;
				iptr->lckl_p[0] = 0xf3;
				iptr->lckl_p[1] = 0xf3;
				iptr->lckl_p[2] = 0xf3;
			case 832:
				iptr->hes = 0x0004;
				iptr->heb = 0x0011;
				iptr->hsb = 0x0045;
				iptr->ht = 0x0048;
				iptr->ves = 0x0003;
				iptr->veb = 0x002a;
				iptr->vsb = 0x029a;
				iptr->vt = 0x029b;
				iptr->vil = 0x0000;
				iptr->pclk_m = 0xfe;
				iptr->pclk_n = 0x3e;
				iptr->pclk_p = 0xf1;
				iptr->mlc[0] = 0x39;
				iptr->mlc[1] = 0x38;
				iptr->mlc[2] = 0x38;
				iptr->lckl_p[0] = 0xf3;
				iptr->lckl_p[1] = 0xf3;
				iptr->lckl_p[2] = 0xf2;
			case 1024:
				iptr->hes = 0x0006;
				iptr->heb = 0x0210;
				iptr->hsb = 0x0250;
				iptr->ht = 0x0053;
				iptr->ves = 0x1003;
				iptr->veb = 0x0021;
				iptr->vsb = 0x0321;
				iptr->vt = 0x0324;
				iptr->vil = 0x0000;
				iptr->pclk_m = 0xfc;
				iptr->pclk_n = 0x3a;
				iptr->pclk_p = 0xf1;
				iptr->mlc[0] = 0x39;
				iptr->mlc[1] = 0x38;
				iptr->mlc[2] = 0x38;
				iptr->lckl_p[0] = 0xf3;
				iptr->lckl_p[1] = 0xf3;
				iptr->lckl_p[2] = 0xf2;
			case 1152:
				iptr->hes = 0x0009;
				iptr->heb = 0x0011;
				iptr->hsb = 0x0059;
				iptr->ht = 0x005b;
				iptr->ves = 0x0003;
				iptr->veb = 0x0031;
				iptr->vsb = 0x0397;
				iptr->vt = 0x039a;
				iptr->vil = 0x0000;
				iptr->pclk_m = 0xfd;
				iptr->pclk_n = 0x3a;
				iptr->pclk_p = 0xf1;
				iptr->mlc[0] = 0x39;
				iptr->mlc[1] = 0x38;
				iptr->mlc[2] = 0x38;
				iptr->lckl_p[0] = 0xf3;
				iptr->lckl_p[1] = 0xf3;
				iptr->lckl_p[2] = 0xf2;
			case 1280:
				iptr->hes = 0x0009;
				iptr->heb = 0x0018;
				iptr->hsb = 0x0068;
				iptr->ht = 0x006a;
				iptr->ves = 0x0003;
				iptr->veb = 0x0029;
				iptr->vsb = 0x0429;
				iptr->vt = 0x042a;
				iptr->vil = 0x0000;
				iptr->pclk_m = 0xf0;
				iptr->pclk_n = 0x2d;
				iptr->pclk_p = 0xf0;
				iptr->mlc[0] = 0x38;
				iptr->mlc[1] = 0x38;
				iptr->mlc[2] = 0x38;
				iptr->lckl_p[0] = 0xf3;
				iptr->lckl_p[1] = 0xf2;
				iptr->lckl_p[2] = 0xf1;
			default:
				return FALSE;
		}
	}

	/* do it! */
	IMSTTWriteMode(pScrn);
	IMSTTAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

	return TRUE;
}




static void IMSTTSetClock(ScrnInfoPtr pScrn, unsigned long mhz)
{
	IMSTTPtr iptr;
	unsigned long clk_m, clk_n, clk_p, x, stage, spilled;

	iptr = IMSTTPTR(pScrn);
	clk_m = clk_n = clk_p = 0;
	stage = spilled = 0;

	for (;;) {
		switch (stage) {
			case 0:
				clk_m++;
				break;
			case 1:
				clk_n++;
				break;
		}
		x = 20 * (clk_m + 1) / ((clk_n + 1) * (clk_p ? 2 * clk_p : 1));
		if (x == mhz)
			break;
		if (x > mhz) {
			spilled = 1;
			stage = 1;
		} else if (spilled && x < mhz) {
			stage = 0;
		}
	}

	iptr->pclk_m = clk_m;
	iptr->pclk_n = clk_n;
	iptr->pclk_p = clk_p;
}


static void IMSTTWriteMode(ScrnInfoPtr pScrn)
{
	IMSTTPtr iptr;
	unsigned char pixformat;
	unsigned long ctl = 0, pitch = 0, byteswap = 0, scr = 0;
	unsigned char tcc = 0, mxc = 0, lckl_n = 0, mic, mlc = 0, lckl_p = 0;

	iptr = IMSTTPTR(pScrn);

	pixformat = (pScrn->bitsPerPixel >> 3) + 2;

	if (iptr->InitDAC && (pScrn->bitsPerPixel == 16)) {
		if (iptr->ramdac == RAMDAC_IBM) {
			iptr->CMAPBase[IBM624_PIDXHI] = 0;		eieio();
			iptr->CMAPBase[IBM624_PIDXLO] = IBM624_BPP16;	eieio();
			iptr->CMAPBase[IBM624_PIDXDATA] = 0x03;		eieio();
		} else {
			iptr->CMAPBase[TVP_ADDRW] = TVP_IRTCC;		eieio();
			iptr->CMAPBase[TVP_IDATA] = 0x45;		eieio();
		}
	}

	/* XXX do for 15bpp */

	if ((iptr->ramdac == RAMDAC_IBM) && (iptr->InitDAC)) {
		iptr->CMAPBase[IBM624_PIDXHI] = 0;			eieio();
		iptr->CMAPBase[IBM624_PIDXLO] = IBM624_PIXM0;		eieio();
		iptr->CMAPBase[IBM624_PIDXDATA] = iptr->pclk_m;		eieio();
		iptr->CMAPBase[IBM624_PIDXLO] = IBM624_PIXN0;		eieio();
		iptr->CMAPBase[IBM624_PIDXDATA] = iptr->pclk_n;		eieio();
		iptr->CMAPBase[IBM624_PIDXLO] = IBM624_PIXP0;		eieio();
		iptr->CMAPBase[IBM624_PIDXDATA] = iptr->pclk_p;		eieio();
		iptr->CMAPBase[IBM624_PIDXLO] = IBM624_PIXC0;		eieio();
		iptr->CMAPBase[IBM624_PIDXDATA] = 0x02;			eieio();
		iptr->CMAPBase[IBM624_PIDXLO] = IBM624_PIXFMT;		eieio();
		iptr->CMAPBase[IBM624_PIDXDATA] = pixformat;		eieio();
	}

	if ((iptr->ramdac == RAMDAC_TVP) && (iptr->InitDAC)) {
		switch (pScrn->bitsPerPixel) {
			case 8:
				tcc = 0x80;
				mxc = 0x4d;
				lckl_n = 0xc1;
				mlc = iptr->mlc[0];
				lckl_p = iptr->lckl_p[0];
				break;
			case 16:
				tcc = 0x44;
				mxc = 0x55;
				lckl_n = 0xe1;
				mlc = iptr->mlc[1];
				lckl_p = iptr->lckl_p[1];
				break;
			case 24:
				tcc = 0x5e;
				mxc = 0x5d;
				lckl_n = 0xf1;
				mlc = iptr->mlc[2];
				lckl_p = iptr->lckl_p[2];
				break;
			case 32:
				tcc = 0x46;
				mxc = 0x5d;
				lckl_n = 0xf1;
				mlc = iptr->mlc[2];
				lckl_p = iptr->lckl_p[2];
				break;
		}

		mic = 0x08;

		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPLA;			eieio();
		iptr->CMAPBase[TVP_IDATA] = 0x00;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPPD;			eieio();
		iptr->CMAPBase[TVP_IDATA] = iptr->pclk_m;		eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPPD;			eieio();
		iptr->CMAPBase[TVP_IDATA] = iptr->pclk_n;		eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPPD;			eieio();
		iptr->CMAPBase[TVP_IDATA] = iptr->pclk_p;		eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRTCC;			eieio();
		iptr->CMAPBase[TVP_IDATA] = tcc;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRMXC;			eieio();
		iptr->CMAPBase[TVP_IDATA] = mxc;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRMIC;			eieio();
		iptr->CMAPBase[TVP_IDATA] = mic;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPLA;			eieio();
		iptr->CMAPBase[TVP_IDATA] = 0x00;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRLPD;			eieio();
		iptr->CMAPBase[TVP_IDATA] = lckl_n;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPLA;			eieio();
		iptr->CMAPBase[TVP_IDATA] = 0x15;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRMLC;			eieio();
		iptr->CMAPBase[TVP_IDATA] = mlc;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRPLA;			eieio();
		iptr->CMAPBase[TVP_IDATA] = 0x2a;			eieio();
		iptr->CMAPBase[TVP_ADDRW] = TVP_IRLPD;			eieio();
		iptr->CMAPBase[TVP_IDATA] = lckl_p;			eieio();
	}

	switch (pScrn->bitsPerPixel) {
		case 8:
			ctl = 0x17b1;
			pitch = iptr->pitch >> 2;
			byteswap = 0x000;
			break;
		case 16:
			ctl = 0x17b3;
			pitch = iptr->pitch >> 1;
			byteswap = 0x100;
			break;
		case 24:
			ctl = 0x17b9;
			pitch = iptr->pitch - (iptr->pitch >> 2);
			byteswap = 0x200;
			break;
		case 32:
			ctl = 0x17b5;
			pitch = iptr->pitch;
			byteswap = 0x300;
			break;
	}

	if (iptr->ramdac == RAMDAC_TVP)
		ctl -= 0x30;

	OUTREG(IMSTT_HES, iptr->hes);
	OUTREG(IMSTT_HEB, iptr->heb);
	OUTREG(IMSTT_HSB, iptr->hsb);
	OUTREG(IMSTT_HT, iptr->ht);
	OUTREG(IMSTT_VES, iptr->ves);
	OUTREG(IMSTT_VEB, iptr->veb);
	OUTREG(IMSTT_VSB, iptr->vsb);
	OUTREG(IMSTT_VT, iptr->vt);
	OUTREG(IMSTT_VIL, iptr->vil);
	OUTREG(IMSTT_HCIV, 1);
	OUTREG(IMSTT_VCIV, 1);
	OUTREG(IMSTT_TCDR, 4);
	OUTREG(IMSTT_RRCIV, 1);
	OUTREG(IMSTT_RRSC, 0x980);
	OUTREG(IMSTT_RRCR, 0x11);

	if (iptr->ramdac == RAMDAC_IBM) {
		OUTREG(IMSTT_HRIR, 0x0100);
		OUTREG(IMSTT_CMR, 0x00ff);
		OUTREG(IMSTT_SRGCTL, 0x0073);
	} else {
		OUTREG(IMSTT_HRIR, 0x0200);
		OUTREG(IMSTT_CMR, 0x01ff);
		OUTREG(IMSTT_SRGCTL, 0x0003);
	}

	switch (iptr->videoRam) {
		case 0x200000:
			scr = 0x059d | byteswap;
			break;
		default:
			/* 0x400000 and 0x800000 */
			pitch >>= 1;
			scr = 0x150dd | byteswap;
			break;
	}

	OUTREG(IMSTT_SCR, scr);
	OUTREG(IMSTT_SPR, pitch);
	OUTREG(IMSTT_STGCTL, ctl);

	return;
}



static void IMSTTAdjustFrame(int scrnIndex, int x, int y, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	IMSTTPtr iptr;
	unsigned long offset;

	iptr = IMSTTPTR(pScrn);

	offset = y * pScrn->displayWidth + x;
	offset &= ~7;

	OUTREG(IMSTT_SSR, offset);

	return;
}


Bool IMSTTCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	IMSTTPtr iptr = IMSTTPTR(pScrn);

	if (iptr->FBDev)
		fbdevHWSave(pScrn);

	return TRUE;
}


Bool IMSTTSaveScreen(ScreenPtr pScreen, int mode)
{
	return TRUE;
}
