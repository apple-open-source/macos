/*
 * Copyright 1992-2000 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *	    Re-written for XFree86 v4.0
 *
 * Previous driver (pre-XFree86 v4.0) by
 * 	    Alan Hourihane, alanh@fairlite.demon.co.uk
 *	    David Wexelblat (major contributor)
 *	    Massimiliano Ghilardi, max@Linuz.sns.it, some fixes to the
 *				   clockchip programming code.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_driver.c,v 1.176 2003/02/11 03:41:38 dawes Exp $ */

#include "xf1bpp.h"
#include "xf4bpp.h"
#include "fb.h"

#include "mibank.h"
#include "micmap.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86cmap.h"
#include "vgaHW.h"
#include "xf86RAC.h"
#include "vbe.h"
#include "dixstruct.h"
#include "compiler.h"

#include "mipointer.h"

#include "mibstore.h"
#include "shadow.h"
#include "trident.h"
#include "trident_regs.h"

#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

#ifdef XvExtension
#include "xf86xv.h"
#endif

static const OptionInfoRec * TRIDENTAvailableOptions(int chipid, int busid);
static void	TRIDENTIdentify(int flags);
static Bool	TRIDENTProbe(DriverPtr drv, int flags);
static Bool	TRIDENTPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	TRIDENTScreenInit(int Index, ScreenPtr pScreen, int argc,
			      char **argv);
static Bool	TRIDENTEnterVT(int scrnIndex, int flags);
static void	TRIDENTLeaveVT(int scrnIndex, int flags);
static Bool	TRIDENTCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	TRIDENTSaveScreen(ScreenPtr pScreen, int mode);

/* Optional functions */
static void	TRIDENTFreeScreen(int scrnIndex, int flags);
static int	TRIDENTValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
			     int flags);

/* Internally used functions */
static Bool	TRIDENTMapMem(ScrnInfoPtr pScrn);
static Bool	TRIDENTUnmapMem(ScrnInfoPtr pScrn);
static void	TRIDENTSave(ScrnInfoPtr pScrn);
static void	TRIDENTRestore(ScrnInfoPtr pScrn);
static Bool	TRIDENTModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void 	TRIDENTBlockHandler(int, pointer, pointer, pointer);

static void	TRIDENTEnableMMIO(ScrnInfoPtr pScrn);
static void	TRIDENTDisableMMIO(ScrnInfoPtr pScrn);
static void	PC98TRIDENTInit(ScrnInfoPtr pScrn);
static void	PC98TRIDENTEnable(ScrnInfoPtr pScrn);
static void	PC98TRIDENTDisable(ScrnInfoPtr pScrn);
static void	PC98TRIDENT96xxInit(ScrnInfoPtr pScrn);
static void	PC98TRIDENT96xxEnable(ScrnInfoPtr pScrn);
static void	PC98TRIDENT96xxDisable(ScrnInfoPtr pScrn);
static void	PC98TRIDENT9385Init(ScrnInfoPtr pScrn);
static void	PC98TRIDENT9385Enable(ScrnInfoPtr pScrn);
static void	PC98TRIDENT9385Disable(ScrnInfoPtr pScrn);

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;
 
#define VERSION 4000
#define TRIDENT_NAME "TRIDENT"
#define TRIDENT_DRIVER_NAME "trident"
#define TRIDENT_MAJOR_VERSION 1
#define TRIDENT_MINOR_VERSION 0
#define TRIDENT_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec TRIDENT = {
    VERSION,
    TRIDENT_DRIVER_NAME,
    TRIDENTIdentify,
    TRIDENTProbe,
    TRIDENTAvailableOptions,
    NULL,
    0
};

static SymTabRec TRIDENTChipsets[] = {
    { TVGA9000,			"tvga9000" },
    { TVGA9000i,		"tvga9000i" },
    { TVGA8900C,		"tvga8900c" },
    { TVGA8900D,		"tvga8900d" },
    { TVGA9200CXr,		"tvga9200cxr" },
    { TGUI9400CXi,		"tgui9400cxi" },
    { CYBER9320,		"cyber9320" },
    { CYBER9388,		"cyber9388" },
    { CYBER9397,		"cyber9397" },
    { CYBER9397DVD,		"cyber9397dvd" },
    { CYBER9520,		"cyber9520" },
    { CYBER9525DVD,		"cyber9525dvd" },
    { CYBERBLADEE4,		"cyberblade/e4" },
    { TGUI9420DGi,		"tgui9420dgi" },
    { TGUI9440AGi,		"tgui9440agi" },
    { TGUI9660,			"tgui9660" },
    { TGUI9680,			"tgui9680" },
    { PROVIDIA9682,		"providia9682" },
    { PROVIDIA9685,		"providia9685" },
    { CYBER9382,		"cyber9382" },
    { CYBER9385,		"cyber9385" },
    { IMAGE975,			"3dimage975" },
    { IMAGE985,			"3dimage985" },
    { BLADE3D,			"blade3d" },
    { CYBERBLADEI7,		"cyberbladei7" },
    { CYBERBLADEI7D,		"cyberbladei7d" },
    { CYBERBLADEI1,		"cyberbladei1" },
    { CYBERBLADEI1D,		"cyberbladei1d" },
    { CYBERBLADEAI1,		"cyberbladeAi1" },
    { CYBERBLADEAI1D,		"cyberbladeAi1d" },
    { BLADEXP,			"bladeXP" },
    { CYBERBLADEXPAI1,		"cyberbladeXPAi1" },
    { -1,				NULL }
};

static IsaChipsets TRIDENTISAchipsets[] = {
    { TVGA9000,			RES_EXCLUSIVE_VGA },
    { TVGA9000i,		RES_EXCLUSIVE_VGA },
    { TVGA8900C,		RES_EXCLUSIVE_VGA },
    { TVGA8900D,		RES_EXCLUSIVE_VGA },
    { TVGA9200CXr,		RES_EXCLUSIVE_VGA },
    { TGUI9400CXi,		RES_EXCLUSIVE_VGA },
    { CYBER9320,		RES_EXCLUSIVE_VGA },
    { TGUI9440AGi,		RES_EXCLUSIVE_VGA },
    { -1,			RES_UNDEFINED }
};

static PciChipsets TRIDENTPciChipsets[] = {
    { CYBER9320,	PCI_CHIP_9320,	RES_SHARED_VGA },
    { CYBER9388,	PCI_CHIP_9388,	RES_SHARED_VGA },
    { CYBER9397,	PCI_CHIP_9397,	RES_SHARED_VGA },
    { CYBER9397DVD,	PCI_CHIP_939A,	RES_SHARED_VGA },
    { CYBER9520,	PCI_CHIP_9520,	RES_SHARED_VGA },
    { CYBER9525DVD,	PCI_CHIP_9525,	RES_SHARED_VGA },
    { CYBERBLADEE4,	PCI_CHIP_9540,	RES_SHARED_VGA },
    { TGUI9420DGi,	PCI_CHIP_9420,	RES_SHARED_VGA },
    { TGUI9440AGi,	PCI_CHIP_9440,	RES_SHARED_VGA },
    { TGUI9660,		PCI_CHIP_9660,	RES_SHARED_VGA },
    { TGUI9680,		PCI_CHIP_9660,	RES_SHARED_VGA },
    { PROVIDIA9682,	PCI_CHIP_9660,	RES_SHARED_VGA },
    { PROVIDIA9685,	PCI_CHIP_9660,	RES_SHARED_VGA },
    { CYBER9382,	PCI_CHIP_9660,	RES_SHARED_VGA },
    { CYBER9385,	PCI_CHIP_9660,	RES_SHARED_VGA },
    { IMAGE975,		PCI_CHIP_9750,	RES_SHARED_VGA },
    { IMAGE985,		PCI_CHIP_9850,	RES_SHARED_VGA },
    { BLADE3D,		PCI_CHIP_9880,	RES_SHARED_VGA },
    { CYBERBLADEI7,	PCI_CHIP_8400,	RES_SHARED_VGA },
    { CYBERBLADEI7D,	PCI_CHIP_8420,	RES_SHARED_VGA },
    { CYBERBLADEI1,	PCI_CHIP_8500,	RES_SHARED_VGA },
    { CYBERBLADEI1D,	PCI_CHIP_8520,	RES_SHARED_VGA },
    { CYBERBLADEAI1,	PCI_CHIP_8600,	RES_SHARED_VGA },
    { CYBERBLADEAI1D,	PCI_CHIP_8620,	RES_SHARED_VGA },
    { BLADEXP,		PCI_CHIP_9910,	RES_SHARED_VGA },
    { CYBERBLADEXPAI1,	PCI_CHIP_8820,	RES_SHARED_VGA },
    { -1,		-1,		RES_UNDEFINED }
};
    
typedef enum {
    OPTION_SW_CURSOR,
    OPTION_PCI_RETRY,
    OPTION_RGB_BITS,
    OPTION_NOACCEL,
    OPTION_SETMCLK,
    OPTION_MUX_THRESHOLD,
    OPTION_SHADOW_FB,
    OPTION_ROTATE,
    OPTION_MMIO_ONLY,
    OPTION_VIDEO_KEY,
    OPTION_NOMMIO,
    OPTION_NOPCIBURST,
    OPTION_CYBER_SHADOW,
    OPTION_CYBER_STRETCH,
    OPTION_XV_HSYNC,
    OPTION_XV_VSYNC,
    OPTION_XV_BSKEW,
    OPTION_XV_RSKEW,
    OPTION_FP_DELAY,
    OPTION_1400_DISPLAY,
    OPTION_DISPLAY,
    OPTION_GB
} TRIDENTOpts;

static const OptionInfoRec TRIDENTOptions[] = {
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_PCI_RETRY,		"PciRetry",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_SETMCLK,		"SetMClk",	OPTV_FREQ,	{0}, FALSE },
    { OPTION_MUX_THRESHOLD,	"MUXThreshold",	OPTV_INTEGER,	{0}, FALSE },
    { OPTION_SHADOW_FB,		"ShadowFB",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_ROTATE,  	        "Rotate",	OPTV_ANYSTR,	{0}, FALSE },
    { OPTION_VIDEO_KEY,		"VideoKey",	OPTV_INTEGER,	{0}, FALSE },
    { OPTION_NOMMIO,		"NoMMIO",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_NOPCIBURST,	"NoPciBurst",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_MMIO_ONLY,		"MMIOonly",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_CYBER_SHADOW,	"CyberShadow",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_CYBER_STRETCH,	"CyberStretch",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_XV_HSYNC,          "XvHsync",      OPTV_INTEGER,   {0}, FALSE },
    { OPTION_XV_VSYNC,          "XvVsync",      OPTV_INTEGER,   {0}, FALSE },
    { OPTION_XV_BSKEW,          "XvBskew",      OPTV_INTEGER,   {0}, FALSE },
    { OPTION_XV_RSKEW,          "XvRskew",      OPTV_INTEGER,   {0}, FALSE },
    { OPTION_FP_DELAY,          "FpDelay",      OPTV_INTEGER,   {0}, FALSE },
    { OPTION_1400_DISPLAY,	"Display1400",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_DISPLAY,		"Display",	OPTV_ANYSTR,	{0}, FALSE },
    { OPTION_GB,		"GammaBrightness",	OPTV_ANYSTR,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

/* Clock Limits */
static int ClockLimit[] = {
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	80000,
	90000,
	90000,
	135000,
	135000,
	170000,
	170000,
	170000,
	170000,
	170000,
	170000,
	170000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
};

static int ClockLimit16bpp[] = {
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	40000,
	45000,
	45000,
	90000,
	90000,
	135000,
	135000,
	170000,
	170000,
	170000,
	170000,
	170000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
	230000,
}; 

static int ClockLimit24bpp[] = {
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	40000,
	40000,
	70000,
	70000,
	70000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
};

static int ClockLimit32bpp[] = {
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	25180,
	40000,
	40000,
	70000,
	70000,
	70000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
	115000,
};

/*
 * These are fixed modelines for all physical display dimensions the
 *  chipsets supports on FPs. Most of them are not tested yet.
 */
#if 0
tridentLCD LCD[] = {   /* 0    3    4    5   6    7    10   11  16 */
    { 0,"640x480",25200,0x5f,0x82,0x54,0x80,0xb,0x3e,0xea,0x8c,0xb,0x08},
    { 1,"800x600",40000,0x7f,0x99,0x69,0x99,0x72,0xf0,0x59,0x2d,0x5e,0x08},
    { 2,"1024x768",65000,0xa3,/*0x6*/ 0x98,0x8f,0xa0,0x24,0xf5,0x0f,0x25,0x96,0x08},
    { 3,"1024x768",65000,0xa3,/*0x6*/ 0x98,0x8f,0xa0,0x24,0xf5,0x0f,0x25,0x96,0x08}, /*0x96*/
    { 4,"1280x1024",108000,0xa3,0x6,0x8f,0xa0,0x24,0xf5,0x0f,0x25,0x96,0x08},
    { 5,"1024x600",50500 ,0xa3,0x6,0x8f,0xa0,0xb,0x3e,0xea,0x8c,0xb,0x08},
    { 0xff,"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
#else
#if 0
tridentLCD LCD[] = { 
    { 1,640,480,25200,0x5f,0x82,0x54,0x80,0xb,0x3e,0xea,0x8c,0xb,0x08},
    { 3,800,600,40000,0x7f,0x82,0x6b,0x1b,0x72,0xf8,0x58,0x8c,0x72,0x08},
    { 2,1024,768,65000,0xa3,/*0x6*/0x98,0x8f,0xa0,0x24,0xf5,0x0f,0x24,0x0a,0x08},
    { 0,1280,1024,108000,0xce,0x81,0xa6,0x9a,0x27,0x50,0x00,0x03,0x26,0xa8},
    { 0xff,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
#else
tridentLCD LCD[] = { 
    { 1,640,480,25200,0x5f,0x80,0x52,0x1e,0xb,0x3e,0xea,0x0c,0xb,0x08},
    { 3,800,600,40000,0x7f,0x00,0x69,0x7f,0x72,0xf0,0x59,0x0d,0x00,0x08},
    { 2,1024,768,65000,0xa3,0x00,0x84,0x94,0x24,0xf5,0x03,0x09,0x24,0x08},
    { 0,1280,1024,108000,0xce,0x91,0xa6,0x14,0x28,0x5a,0x01,0x04,0x28,0xa8},
    { 4,1400,1050,122000,0xe6,0xcd,0xba,0x1d,0x38,0x00,0x1c,0x28,0x28,0xf8},
    { 0xff,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
#endif
#endif

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAFillSolidRects",
    "XAAInit",
    "XAAPatternROP",
    "XAAScreenIndex",
    NULL
};

static const char *vgahwSymbols[] = {
    "vgaHWBlankScreen",
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
    NULL
};

static const char *miscfbSymbols[] = {
    "xf1bppScreenInit",
    "xf4bppScreenInit",
    NULL
};

static const char *fbSymbols[] = {
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

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86SetDDCproperties",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *int10Symbols[] = {
    "xf86ExecX86int10",
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

static const char *shadowSymbols[] = {
    "shadowInit",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(tridentSetup);

static XF86ModuleVersionInfo tridentVersRec =
{
	"trident",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	TRIDENT_MAJOR_VERSION, TRIDENT_MINOR_VERSION, TRIDENT_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,			/* This is a video driver */
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

XF86ModuleData tridentModuleData = { &tridentVersRec, tridentSetup, NULL };

pointer
tridentSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&TRIDENT, module, 0);
	LoaderRefSymLists(vgahwSymbols, fbSymbols, i2cSymbols, vbeSymbols,
			  miscfbSymbols, ramdacSymbols, int10Symbols,
			  xaaSymbols, shadowSymbols, NULL);
	return (pointer)TRUE;
    } 

    if (errmaj) *errmaj = LDR_ONCEONLY;
    return NULL;
}

#endif /* XFree86LOADER */

static Bool
TRIDENTGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an TRIDENTRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(TRIDENTRec), 1);
    /* Initialise it */

    return TRUE;
}

static void
TRIDENTFreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static void 
TRIDENTDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD8 DPMSCont, PMCont, temp;

    if (!pScrn->vtSema)
	return;

    OUTB(0x3C4, 0x0E);
    temp = INB(0x3C5);
    OUTB(0x3C5, 0xC2);
    OUTB(0x83C8, 0x04); /* Read DPMS Control */
    PMCont = INB(0x83C6) & 0xFC;
    OUTB(0x3CE, 0x23);
    DPMSCont = INB(0x3CF) & 0xFC;
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
    OUTB(0x3CF, DPMSCont);
    OUTB(0x83C8, 0x04);
    OUTB(0x83C6, PMCont);
    OUTW(0x3C4, (temp<<8) | 0x0E);
}

static void
TRIDENTBlockHandler (
    int i,
    pointer     blockData,
    pointer     pTimeout,
    pointer     pReadmask
){
    ScreenPtr      pScreen = screenInfo.screens[i];
    ScrnInfoPtr    pScrn = xf86Screens[i];
    TRIDENTPtr     pTrident = TRIDENTPTR(pScrn);

    pScreen->BlockHandler = pTrident->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = TRIDENTBlockHandler;

    if(pTrident->VideoTimerCallback) {
	UpdateCurrentTime();
	(*pTrident->VideoTimerCallback)(pScrn, currentTime.milliseconds);
    }
} 

static const OptionInfoRec *
TRIDENTAvailableOptions(int chipid, int busid)
{
    return TRIDENTOptions;
}

/* Mandatory */
static void
TRIDENTIdentify(int flags)
{
    xf86PrintChipsets(TRIDENT_NAME, "driver for Trident chipsets", TRIDENTChipsets);
}

static void
TRIDENTFix1bpp(ScrnInfoPtr pScrn) {
   vgaHWPtr hwp = VGAHWPTR(pScrn);
/* In 1 bpp we have color 0 at LUT 0 and color 1 at LUT 0x3f.
   This makes white and black look right (otherwise they were both
   black. I'm sure there's a better way to do that, just lazy to
   search the docs.  */

   (*hwp->writeDacWriteAddr)(hwp, 0x00);
   (*hwp->writeDacData)(hwp, 0x00);
   (*hwp->writeDacData)(hwp, 0x00);
   (*hwp->writeDacData)(hwp, 0x00);

   (*hwp->writeDacWriteAddr)(hwp, 0x3F);
   (*hwp->writeDacData)(hwp, 0x3F);
   (*hwp->writeDacData)(hwp, 0x3F);
   (*hwp->writeDacData)(hwp, 0x3F);
}

Bool
TRIDENTClockSelect(ScrnInfoPtr pScrn, int no)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    unsigned char temp;

    /*
     * CS0 and CS1 are in MiscOutReg
     *
     * For 8900B, 8900C, 8900CL and 9000, CS2 is bit 0 of
     * New Mode Control Register 2.
     *
     * For 8900CL, CS3 is bit 4 of Old Mode Control Register 1.
     *
     * For 9000, CS3 is bit 6 of New Mode Control Register 2.
     *
     * For TGUI, we don't use the ClockSelect function at all.
     */
     switch(no) {
	case CLK_REG_SAVE:
	    pTrident->SaveClock1 = INB(0x3CC);
	    if (pTrident->Chipset != TVGA8800CS) {
		if ( (pScrn->numClocks == 16) && 
		     (pTrident->Chipset != TVGA9000) &&
		     (pTrident->Chipset != TVGA9000i) )
		{
		    OUTW(0x3C4, 0x000B);	/* Switch to Old Mode */
		    OUTB(0x3C4, 0x0E); pTrident->SaveClock3 = INB(0x3C5);
		}
		OUTB(0x3C4, 0x0B);	
		INB(0x3C5);		/* Now to New Mode */
		OUTB(0x3C4, 0x0D); pTrident->SaveClock2 = INB(0x3C5);
	    }
	    break;
	case CLK_REG_RESTORE:
	    OUTB(0x3C2, pTrident->SaveClock1);
	    if (pTrident->Chipset != TVGA8800CS) {
		if ( (pScrn->numClocks == 16) && 
		     (pTrident->Chipset != TVGA9000) &&
		     (pTrident->Chipset != TVGA9000i) )
		{
			OUTW(0x3C4, 0x000B);	/* Switch to Old Mode */
			OUTW(0x3C4, (pTrident->SaveClock3 << 8) | 0x0E);
		}
		OUTB(0x3C4, 0x0B);	
		INB(0x3C5);		/* Now to New Mode */
		OUTW(0x3C4, (pTrident->SaveClock2 << 8) | 0x0D); 
	    }
	    break;
	default:
	    /*
	     * Do CS0 and CS1
	     */
	    temp = INB(0x3CC);
	    OUTB(0x3C2, (temp & 0xF3) | ((no << 2) & 0x0C));
	    if (pTrident->Chipset != TVGA8800CS) {
		if ( (pScrn->numClocks == 16) && 
		     (pTrident->Chipset != TVGA9000) &&
		     (pTrident->Chipset != TVGA9000i) )
		{
			/* 
			 * Go to Old Mode for CS3.
		 	 */
			OUTW(0x3C4, 0x000B);	/* Switch to Old Mode */
			OUTB(0x3C4, 0x0E);
			temp = INB(0x3C5) & 0xEF;
			temp |= (no & 0x08) << 1;
			OUTB(0x3C5, temp);
		}
		/* 
	  	 * Go to New Mode for CS2 and TVGA9000 CS3.
	  	 */
		OUTB(0x3C4, 0x0B);
		INB(0x3C5);		/* Now to New Mode */
		OUTB(0x3C4, 0x0D);
		/*
		 * Bits 1 & 2 are dividers - set to 0 to get no
		 * clock division.
		 */
		temp = INB(0x3C5) & 0xF8;
		temp |= (no & 0x04) >> 2;
		if ( (pTrident->Chipset == TVGA9000) ||
		     (pTrident->Chipset == TVGA9000i) )
		{
			temp &= ~0x40;
			temp |= (no & 0x08) << 3;
		}
		OUTB(0x3C5, temp);
	}
    }
    return(TRUE);
}

static int
TridentFindIsaDevice(GDevPtr dev)
{
    int found = -1;
    unsigned char temp, origVal, newVal;

    /* 
     * Check first that we have a Trident card.
     */
    outb(0x3C4, 0x0B);
    temp = inb(0x3C5);	/* Save old value */
    outb(0x3C4, 0x0B);	/* Switch to Old Mode */
    outb(0x3C5, 0x00);
    inb(0x3C5);		/* Now to New Mode */
    outb(0x3C4, 0x0E);
    origVal = inb(0x3C5);
    outb(0x3C5, 0x00);
    newVal = inb(0x3C5) & 0x0F;
    outb(0x3C5, (origVal ^ 0x02));

    /* 
     * Is it a Trident card ?? 
     */
    if (newVal != 2) {
	/*
	 * Nope, so quit
	 */
	outb(0x3C4, 0x0B);	/* Restore value of 0x0B */
	outb(0x3C5, temp);
	outb(0x3C4, 0x0E);
	outb(0x3C5, origVal);
	return found;
    }

    outb(0x3C4, 0x0B);
    temp = inb(0x3C5);
    switch (temp) {
	case 0x01:
	    found = TVGA8800BR;
	    break;
	case 0x02:
	    found = TVGA8800CS;
	    break;
	case 0x03:
	    found = TVGA8900B;
	    break;
	case 0x04:
	case 0x13:
	    found = TVGA8900C;
	    break;
	case 0x23:
	    found = TVGA9000;
	    break;
	case 0x33:
	    found = TVGA8900D;
	    break;
	case 0x43:
	    found = TVGA9000i;
	    break;
	case 0x53:
	    found = TVGA9200CXr;
	    break;
	case 0x63:
	    found = TVGA9100B;
	    break;
	case 0x73:
	case 0xC3:
	    found = TGUI9420DGi;
	    break;
	case 0x83:
	    found = TVGA8200LX;
	    break;
	case 0x93:
	    found = TGUI9400CXi;
	    break;
	case 0xA3:
	    found = CYBER9320;
	    break;
	case 0xD3:
	    found = TGUI9660;
	    break;
	case 0xE3:
	    found = TGUI9440AGi;
	    break;
	case 0xF3:
	    found = TGUI9430DGi;
	    break;
    }
    return found;
}


/* Mandatory */
static Bool
TRIDENTProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips = NULL;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;

    if ((numDevSections = xf86MatchDevice(TRIDENT_DRIVER_NAME,
					  &devSections)) <= 0) {
	/*
	 * There's no matching device section in the config file, so quit
	 * now.
	 */
	return FALSE;
    }

    /*
     * While we're VGA-dependent, can really only have one such instance, but
     * we'll ignore that.
     */

    /*
     * We need to probe the hardware first.  We then need to see how this
     * fits in with what is given in the config file, and allow the config
     * file info to override any contradictions.
     */

    /*
     * All of the cards this driver supports are PCI, so the "probing" just
     * amounts to checking the PCI data that the server has already collected.
     */
    if (xf86GetPciVideoInfo()) {
    	numUsed = xf86MatchPciInstances(TRIDENT_NAME, PCI_VENDOR_TRIDENT,
		   TRIDENTChipsets, TRIDENTPciChipsets, devSections,
		   numDevSections, drv, &usedChips);

	if (numUsed > 0) {
	    if (flags & PROBE_DETECT)
		foundScreen = TRUE;
	    else for (i = 0; i < numUsed; i++) {
		ScrnInfoPtr pScrn = NULL;
		
		if ((pScrn = xf86ConfigPciEntity(pScrn, 0,usedChips[i],
						       TRIDENTPciChipsets, NULL,
						       NULL, NULL, NULL, NULL))) {
		    /* Fill in what we can of the ScrnInfoRec */
		    pScrn->driverVersion = VERSION;
		    pScrn->driverName	 = TRIDENT_DRIVER_NAME;
		    pScrn->name		 = TRIDENT_NAME;
		    pScrn->Probe	 = TRIDENTProbe;
		    pScrn->PreInit	 = TRIDENTPreInit;
		    pScrn->ScreenInit	 = TRIDENTScreenInit;
		    pScrn->SwitchMode	 = TRIDENTSwitchMode;
		    pScrn->AdjustFrame	 = TRIDENTAdjustFrame;
		    pScrn->EnterVT	 = TRIDENTEnterVT;
		    pScrn->LeaveVT	 = TRIDENTLeaveVT;
		    pScrn->FreeScreen	 = TRIDENTFreeScreen;
		    pScrn->ValidMode	 = TRIDENTValidMode;
		    foundScreen = TRUE;
		}
	    }
	    xfree(usedChips);
	}
    }

    /* Isa Bus */
    numUsed = xf86MatchIsaInstances(TRIDENT_NAME,TRIDENTChipsets,
				     TRIDENTISAchipsets,
				     drv,TridentFindIsaDevice,devSections,
				     numDevSections,&usedChips);
    if (numUsed > 0) {
	if (flags & PROBE_DETECT)
	    foundScreen = TRUE;
	else 	for (i = 0; i < numUsed; i++) {
	    ScrnInfoPtr pScrn = NULL;
	    if ((pScrn = xf86ConfigIsaEntity(pScrn,0,usedChips[i],
						  TRIDENTISAchipsets,NULL,
						  NULL,NULL,NULL,NULL))) {
		pScrn->driverVersion = VERSION;
		pScrn->driverName    = TRIDENT_DRIVER_NAME;
		pScrn->name          = TRIDENT_NAME;
		pScrn->Probe         = TRIDENTProbe;
		pScrn->PreInit       = TRIDENTPreInit;
		pScrn->ScreenInit    = TRIDENTScreenInit;
		pScrn->SwitchMode    = TRIDENTSwitchMode;
		pScrn->AdjustFrame   = TRIDENTAdjustFrame;
		pScrn->EnterVT       = TRIDENTEnterVT;
		pScrn->LeaveVT       = TRIDENTLeaveVT;
		pScrn->FreeScreen    = TRIDENTFreeScreen;
		pScrn->ValidMode     = TRIDENTValidMode;
		foundScreen = TRUE;
	    }
	}
	xfree(usedChips);
    }
    
    xfree(devSections);
    return foundScreen;
}
	
/*
 * GetAccelPitchValues -
 *
 * This function returns a list of display width (pitch) values that can
 * be used in accelerated mode.
 */
static int *
GetAccelPitchValues(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int *linePitches = NULL;
    int lines[4] = { 512, 1024, 2048, 4096 }; /* 9440AGi */
    int i, n = 0;

    if (pTrident->Chipset >= BLADEXP) {
	lines[0] = 1024;
	lines[1] = 2048;
	lines[2] = 4096;
	lines[3] = 8192;
    }
	
    for (i = 0; i < 4; i++) {
	n++;
	linePitches = xnfrealloc(linePitches, n * sizeof(int));
	linePitches[n - 1] = lines[i];
    }

    /* Mark the end of the list */
    if (n > 0) {
	linePitches = xnfrealloc(linePitches, (n + 1) * sizeof(int));
	linePitches[n] = 0;
    }
    return linePitches;
}

static void
TRIDENTProbeDDC(ScrnInfoPtr pScrn, int index)
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
TRIDENTPreInit(ScrnInfoPtr pScrn, int flags)
{
    TRIDENTPtr pTrident;
    vgaHWPtr hwp;
    MessageType from;
    CARD8 videoram, videorammask;
    char *ramtype = NULL, *chipset = NULL;
    Bool Support24bpp;
    int vgaIOBase;
    float mclk;
    double real;
    int i, NoClocks = 16;
    CARD8 revision;
    ClockRangePtr clockRanges;
    char *mod = NULL;
    const char *Sym = "";
    Bool ddcLoaded = FALSE;
    char *s;

    /* Allocate the TRIDENTRec driverPrivate */
    if (!TRIDENTGetRec(pScrn)) {
	return FALSE;
    }
    pTrident = TRIDENTPTR(pScrn);
    pTrident->pScrn = pScrn;

    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i<pScrn->numEntities; i++) {
	pTrident->pEnt = xf86GetEntityInfo(pScrn->entityList[i]);
	if (pTrident->pEnt->resources) return FALSE;
	pTrident->Chipset = pTrident->pEnt->chipset;
	pScrn->chipset = (char *)xf86TokenToString(TRIDENTChipsets,
						   pTrident->pEnt->chipset);
	/* This driver can handle ISA and PCI buses */
	if (pTrident->pEnt->location.type == BUS_PCI) {
	    pTrident->PciInfo = xf86GetPciInfoForEntity(pTrident->pEnt->index);
	    pTrident->PciTag = pciTag(pTrident->PciInfo->bus, 
				  pTrident->PciInfo->device,
				  pTrident->PciInfo->func);
    	    pTrident->Linear = TRUE;
	} else {
    	    pTrident->Linear = FALSE;
	}
    }

    if (flags & PROBE_DETECT) {
	TRIDENTProbeDDC(pScrn, pTrident->pEnt->index);
	return TRUE;
    }

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * Our default depth is 8, so pass it to the helper function.
     * Our preference for depth 24 is 24bpp, so tell it that too.
     */
    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, Support24bppFb | Support32bppFb |
			    SupportConvert32to24 /*| PreferConvert32to24*/)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 1:
	case 4:
	case 8:
	    if (pScrn->bitsPerPixel != pScrn->depth) {
	        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	     "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
		       pScrn->depth, pScrn->bitsPerPixel);
		return FALSE;
	    }
	    break;
	case 15:
	case 16:
	    if (pScrn->bitsPerPixel != 16) {
	        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	     "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
		       pScrn->depth, pScrn->bitsPerPixel);
		return FALSE;
	    }
	    break;
	case 24:
	    if ((pScrn->bitsPerPixel != 24) && (pScrn->bitsPerPixel != 32)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	     "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
		       pScrn->depth, pScrn->bitsPerPixel);
		return FALSE;
	    }
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

    /* Get the depth24 pixmap format */
    if (pScrn->depth == 24 && pix24bpp == 0)
	pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    /* The vgahw module should be loaded here when needed */
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
    vgaIOBase = hwp->IOBase;
    pTrident->PIOBase = hwp->PIOOffset;

    xf86SetOperatingState(resVga, pTrident->pEnt->index, ResUnusedOpr);

    /* The ramdac module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "ramdac"))
	return FALSE;

    xf86LoaderReqSymLists(ramdacSymbols, NULL);

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

    /*
     * The new cmap layer needs this to be initialised.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pTrident->Options = xalloc(sizeof(TRIDENTOptions))))
	return FALSE;
    memcpy(pTrident->Options, TRIDENTOptions, sizeof(TRIDENTOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pTrident->Options);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth <= 8) {
	/* XXX This is here just to test options. */
	/* Default to 8 */
	pScrn->rgbBits = 6;
#if 0
	if (xf86GetOptValInteger(pTrident->Options, OPTION_RGB_BITS,
				 &pScrn->rgbBits)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Bits per RGB set to %d\n",
		       pScrn->rgbBits);
	}
#endif
    }
    from = X_DEFAULT;
    pTrident->HWCursor = TRUE;
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pTrident->HWCursor = FALSE;
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_NOACCEL, FALSE)) {
	pTrident->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_PCI_RETRY, FALSE)) {
	pTrident->UsePCIRetry = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry enabled\n");
    }
    pTrident->UsePCIBurst = TRUE;
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_NOPCIBURST, FALSE)) {
	pTrident->UsePCIBurst = FALSE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI Burst disbled\n");
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_1400_DISPLAY, FALSE)) {
	pTrident->displaySize = 1400;
    }
    if(xf86GetOptValInteger(pTrident->Options, OPTION_VIDEO_KEY,
						&(pTrident->videoKey))) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
							pTrident->videoKey);
    } else {
	pTrident->videoKey =  (1 << pScrn->offset.red) | 
			(1 << pScrn->offset.green) |
			(((pScrn->mask.blue >> pScrn->offset.blue) - 1)
			<< pScrn->offset.blue); 
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_NOMMIO, FALSE)) {
	pTrident->NoMMIO = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "MMIO Disabled\n");
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_MMIO_ONLY, FALSE)) {
	if (pTrident->NoMMIO)
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "MMIO only cannot be set"
		       " with NoMMIO\n");
	else {
	    pTrident->MMIOonly = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "MMIO only enabled\n");
	}
    }
    
    pTrident->dspOverride = 0;
    if ((s = xf86GetOptValString(pTrident->Options, OPTION_DISPLAY))) {
	if(!xf86NameCmp(s, "CRT")) {
	    pTrident->dspOverride = CRT_ACTIVE;
	    xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,"LCD off CRT on\n");
	} else if (!xf86NameCmp(s, "LCD")) {
	    pTrident->dspOverride = LCD_ACTIVE;
	    xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,"LCD on CRT off\n");
	} else if (!xf86NameCmp(s, "Dual")) {
	    pTrident->dspOverride = LCD_ACTIVE | CRT_ACTIVE;
	    xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,"LCD on CRT on\n");
	} else 
	    xf86DrvMsg(pScrn->scrnIndex,X_ERROR,
		       "%s is an unknown display option\n",s);
    }
    if ((s = xf86GetOptValString(pTrident->Options, OPTION_GB))) {
	int brightness = -1;
	double gamma = -1.0;
	Bool error = FALSE;
	int i;
	
	i = sscanf(s,"%lf %i",&gamma,&brightness);
	
	if (i != 2 || brightness == -1 || gamma == -1.0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Invalid Gamma/Brightness argument: %s\n",s);
	    error = TRUE;
	} else {
	    if (brightness < 0 || brightness > 128) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "brightness out of range [0,128]: %i\n",brightness);
		error = TRUE;
	    }
	    if (gamma <= 0.0 || gamma > 10.0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "gamma out of range (0,10.0]: %f\n",gamma);
		error = TRUE;
	    }
	}
	
	if (!error) {
	    pTrident->GammaBrightnessOn = TRUE;
	    pTrident->gamma = gamma;
	    pTrident->brightness = brightness;
	    xf86DrvMsg(pScrn->scrnIndex,X_CONFIG,"Gamma: %f Brightness: %i\n",
		       gamma,brightness);
	}
    }

    /* The following is a temporary hack */
    pTrident->FPDelay = 7; /* invalid value */
    if (xf86GetOptValInteger(pTrident->Options, OPTION_FP_DELAY,
			     &pTrident->FPDelay)) {
	if (pTrident->FPDelay < -2 || pTrident->FPDelay > 5) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "FPDelay %i out if range "
		       "(-2 < FPDelay < 5)\n",pTrident->FPDelay);
	    pTrident->FPDelay = 7;
	} else 
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "FP Delay set to %i\n",
		   pTrident->FPDelay);
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_CYBER_SHADOW, FALSE)) {
	pTrident->CyberShadow = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Cyber Shadow enabled\n");
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_CYBER_STRETCH, FALSE)) {
	pTrident->CyberStretch = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Cyber Stretch enabled\n");
    }

    pTrident->MUXThreshold = 90000; /* 90MHz */
    if (xf86GetOptValInteger(pTrident->Options, OPTION_MUX_THRESHOLD, 
						&pTrident->MUXThreshold)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "MUX Threshold set to %d\n",
						pTrident->MUXThreshold);
    }
    pTrident->OverrideHsync = 0;
    if (xf86GetOptValInteger(pTrident->Options, OPTION_XV_HSYNC, 
						&pTrident->OverrideHsync)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv Hsync set to %d\n",
						pTrident->OverrideHsync);
    }
    pTrident->OverrideVsync = 0;
    if (xf86GetOptValInteger(pTrident->Options, OPTION_XV_VSYNC, 
						&pTrident->OverrideVsync)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv Vsync set to %d\n",
						pTrident->OverrideVsync);
    }
    pTrident->OverrideHsync = 0;
    if (xf86GetOptValInteger(pTrident->Options, OPTION_XV_HSYNC, 
						&pTrident->OverrideHsync)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv Hsync set to %d\n",
						pTrident->OverrideHsync);
    }
    pTrident->OverrideVsync = 0;
    if (xf86GetOptValInteger(pTrident->Options, OPTION_XV_VSYNC, 
						&pTrident->OverrideVsync)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv Vsync set to %d\n",
						pTrident->OverrideVsync);
    }
    pTrident->OverrideRskew = 0;
    if (xf86GetOptValInteger(pTrident->Options, OPTION_XV_RSKEW, 
						&pTrident->OverrideRskew)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv Rskew set to %d\n",
						pTrident->OverrideRskew);
    }
    pTrident->OverrideBskew = 0;
    if (xf86GetOptValInteger(pTrident->Options, OPTION_XV_BSKEW, 
						&pTrident->OverrideBskew)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv Bskew set to %d\n",
						pTrident->OverrideBskew);
    }
    if (xf86ReturnOptValBool(pTrident->Options, OPTION_SHADOW_FB, FALSE)) {
        if (!pTrident->Linear) 
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Ignoring Option SHADOW_FB"
		       " in non-Linear Mode\n");
	else if (pScrn->depth < 8) 
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Tgnoring Option SHADOW_FB"
		       " when depth < 8");
	else {
	    pTrident->ShadowFB = TRUE;
	    pTrident->NoAccel = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		    "Using \"Shadow Framebuffer\" - acceleration disabled\n");
	}
    }
    pTrident->Rotate = 0;
    if ((s = xf86GetOptValString(pTrident->Options, OPTION_ROTATE))) {
        if (!pTrident->Linear) 
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Ignoring Option ROTATE "
		       "in non-Linear Mode\n");
	else if (pScrn->depth < 8) 
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Ignoring Option ROTATE "
		       "when depth < 8");
	else {
	    if(!xf86NameCmp(s, "CW")) {
	        /* accel is disabled below for shadowFB */
	        pTrident->ShadowFB = TRUE;
		pTrident->NoAccel = TRUE;
		pTrident->HWCursor = FALSE;
		pTrident->Rotate = 1;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		       "Rotating screen clockwise - acceleration disabled\n");
	    } else if(!xf86NameCmp(s, "CCW")) {
 	        pTrident->ShadowFB = TRUE;
		pTrident->NoAccel = TRUE;
		pTrident->HWCursor = FALSE;
		pTrident->Rotate = -1;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
			   "counter clockwise - acceleration disabled\n");
	    } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
			   "value for Option \"Rotate\"\n", s);
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
			   "Valid options are \"CW\" or \"CCW\"\n");
	    }
	}
    }

    /* FIXME ACCELERATION */
    if (!UseMMIO) pTrident->NoAccel = TRUE;

    if (pTrident->Linear) {
    	if (pTrident->pEnt->device->MemBase != 0) {
	    /*
	     * XXX Should check that the config file value matches one of the
	     * PCI base address values.
	     */
	    pTrident->FbAddress = pTrident->pEnt->device->MemBase;
	    from = X_CONFIG;
    	} else {
    	    if (IsPciCard)
	    	pTrident->FbAddress = pTrident->PciInfo->memBase[0]& 0xFFFFFFF0;
	    else
	    	pTrident->FbAddress = 0xA0000;
    	}

    	xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	       (unsigned long)pTrident->FbAddress);
    }

    if (UseMMIO) {
    	if (pTrident->pEnt->device->IOBase != 0) {
	    /*
	     * XXX Should check that the config file value matches one of the
	     * PCI base address values.
	     */
	    pTrident->IOAddress = pTrident->pEnt->device->IOBase;
	    from = X_CONFIG;
    	} else {
    	    if (IsPciCard)
	    	pTrident->IOAddress = pTrident->PciInfo->memBase[1]& 0xFFFFC000;
	    else
	    	/* FIXME - Multihead UNAWARE */
    	    	pTrident->IOAddress = 0xBF000;
    	}

    	xf86DrvMsg(pScrn->scrnIndex,X_PROBED,"IO registers at 0x%lX\n",
							pTrident->IOAddress);
    }

    /* Register the PCI-assigned resources. */
    if (xf86RegisterResources(pTrident->pEnt->index, NULL, ResExclusive)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "xf86RegisterResources() found resource conflicts\n");
	return FALSE;
    }

    /* Initialize VBE if possible 
     * Don't move this past MMIO enable!!
     * PIO access will be blocked
     * when MMIO is turned on!
     */

    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86MonPtr pMon;
	vbeInfoPtr pVbe;

        xf86LoaderReqSymLists(vbeSymbols, NULL);
	pVbe =  VBEInit(NULL,pTrident->pEnt->index);
	pMon = vbeDoEDID(pVbe, NULL);
#ifdef VBE_INFO
	{
	    VbeInfoBlock* vbeInfoBlockPtr;
	    if ((vbeInfoBlockPtr = VBEGetVBEInfo(pVbe))) {
		pTrident->vbeModes = VBEBuildVbeModeList(pVbe,vbeInfoBlockPtr);
		VBEFreeVBEInfo(vbeInfoBlockPtr);
	    }
	}
#endif
	vbeFree(pVbe);
	if (pMon) {
	    if (!xf86LoadSubModule(pScrn, "ddc")) {
		TRIDENTFreeRec(pScrn);
		return FALSE;
	    } else {
		xf86LoaderReqSymLists(ddcSymbols, NULL);
		xf86SetDDCproperties(pScrn,xf86PrintEDID(pMon));
		ddcLoaded = TRUE;
	    }
	}
	    
    }
    
    if (IsPciCard && UseMMIO) {
    	if (!TRIDENTMapMem(pScrn))
	    return FALSE;

    	TRIDENTEnableMMIO(pScrn);
    }

    OUTB(0x3C4, RevisionID); revision = INB(0x3C5);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Revision is %d\n",revision);

    pScrn->progClock = TRUE;
    pTrident->EngineOperation = 0x00;
    pTrident->IsCyber = FALSE;
    pTrident->HasSGRAM = FALSE;
    pTrident->NewClockCode = FALSE;
    pTrident->MUX = FALSE;
    Support24bpp = FALSE;

    OUTB(vgaIOBase + 4, InterfaceSel);

    switch (pTrident->Chipset) {
	case TVGA9000:
	case TVGA9000i:
	    pScrn->progClock = FALSE;
	    NoClocks = 16;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA9000/9000i";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TVGA9100B:
	    pScrn->progClock = FALSE;
	    NoClocks = 8;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA9100B";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TVGA8900B:
	    pScrn->progClock = FALSE;
	    NoClocks = 8;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA8900B";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TVGA8900C:
	    pScrn->progClock = FALSE;
	    NoClocks = 16;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA8900C";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TVGA8900D:
	    pScrn->progClock = FALSE;
	    NoClocks = 16;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA8900D";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TVGA9200CXr:
	    pScrn->progClock = FALSE;
	    NoClocks = 16;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA9200CXr";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TGUI9400CXi:
	    pScrn->progClock = FALSE;
	    NoClocks = 16;
	    pTrident->NoMMIO = TRUE;
	    pTrident->NoAccel = TRUE;
	    pTrident->HWCursor = FALSE;
	    chipset = "TVGA9200CXr";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case TGUI9440AGi:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    pTrident->HWCursor = FALSE;
	    chipset = "TGUI9440AGi";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    pTrident->frequency = NTSC;
	    break;
	case CYBER9320:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    chipset = "Cyber9320";
	    ramtype = "Standard DRAM";
	    if (pTrident->UsePCIRetry)
	    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
	    pTrident->UsePCIRetry = FALSE; /* Not Supported */
	    break;
	/* Trident didn't update the PCI ID's and so we have to detemine 
	 * which chips are right ! Then override pTrident->Chipset to 
	 * correct values */
	case TGUI9660:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C)
		ramtype = "Standard DRAM";
	    switch (revision) {
		case 0x00:
		    chipset = "TGUI9660";
		    pTrident->Chipset = TGUI9660;
		    if (pTrident->UsePCIRetry)
		    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
		    pTrident->UsePCIRetry = FALSE; /* Not Supported */
		    break;
		case 0x01:
		    chipset = "TGUI9680";
		    pTrident->Chipset = TGUI9680;
		    if (pTrident->UsePCIRetry)
		    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "PCI retry not supported, disabling\n");
		    pTrident->UsePCIRetry = FALSE; /* Not Supported */
		    break;
		case 0x10:
		    chipset = "ProVidia 9682";
		    Support24bpp = TRUE;
		    pTrident->Chipset = PROVIDIA9682;
		    break;
		case 0x21:
		    chipset = "ProVidia 9685";
		    Support24bpp = TRUE;
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = PROVIDIA9685;
		    break;
		case 0x22:
		case 0x23:
		    chipset = "Cyber 9397";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
			ramtype = "EDO Ram";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
			ramtype = "SDRAM";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
			pTrident->HasSGRAM = TRUE;
			ramtype = "SGRAM";
	    	    }
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = CYBER9397;
		    pTrident->IsCyber = TRUE;
		    break;
		case 0x2a:
		    chipset = "Cyber 9397/DVD";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
			ramtype = "EDO Ram";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
			ramtype = "SDRAM";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
			pTrident->HasSGRAM = TRUE;
			ramtype = "SGRAM";
	    	    }
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = CYBER9397DVD;
		    pTrident->IsCyber = TRUE;
		    break;
		case 0x30:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0xB3:
		    chipset = "Cyber 9385";
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = CYBER9385;
		    pTrident->IsCyber = TRUE;
		    break;
		case 0x38:
		case 0x3A:
		    chipset = "Cyber 9385-1";
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = CYBER9385;
		    pTrident->IsCyber = TRUE;
		    break;
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		    chipset = "Cyber 9382";
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = CYBER9382;
		    pTrident->IsCyber = TRUE;
		    break;
		case 0x4A:
		    chipset = "Cyber 9388";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
			ramtype = "EDO Ram";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
			ramtype = "SDRAM";
    	    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
			pTrident->HasSGRAM = TRUE;
			ramtype = "SGRAM";
	    	    }
		    pTrident->NewClockCode = TRUE;
		    pTrident->Chipset = CYBER9388;
		    pTrident->IsCyber = TRUE;
		    break;
		default:
		    chipset = "Unknown";
		    pTrident->Chipset = TGUI9660;
		    break;
	    }
	    break;
	case CYBER9388:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "Cyber 9388";
	    pTrident->NewClockCode = TRUE;
	    break;
	case CYBER9397:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "Cyber 9397";
	    pTrident->NewClockCode = TRUE;
	    break;
	case CYBER9397DVD:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "Cyber 9397/DVD";
	    pTrident->NewClockCode = TRUE;
	    break;
	case CYBER9520:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "Cyber 9520";
	    pTrident->NewClockCode = TRUE;
	    break;
	case CYBER9525DVD:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "Cyber 9525/DVD";
	    pTrident->NewClockCode = TRUE;
	    break;
	case CYBERBLADEE4:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBlade e4/128";
	    pTrident->NewClockCode = TRUE;
	    break;
	case IMAGE975:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    Support24bpp = TRUE;
	    chipset = "3DImage975";
	    pTrident->NewClockCode = TRUE;
	    break;
	case IMAGE985:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x04)
		ramtype = "EDO Ram";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    Support24bpp = TRUE;
	    chipset = "3DImage985";
	    pTrident->NewClockCode = TRUE;
	    break;
	case BLADE3D:
    	    pTrident->ddc1Read = Tridentddc1Read;
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x08)
		ramtype = "SDRAM";
    	    if ((INB(vgaIOBase + 5) & 0x0C) == 0x0C) {
		pTrident->HasSGRAM = TRUE;
		ramtype = "SGRAM";
	    }
	    Support24bpp = TRUE;
	    chipset = "Blade3D";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case CYBERBLADEI7:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    /* pTrident->IsCyber = TRUE; VIA MVP4 integrated Desktop version */
	    Support24bpp = TRUE;
	    chipset = "CyberBlade/i7/VIA MVP4";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case CYBERBLADEI7D:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBlade/DSTN/i7";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case CYBERBLADEI1:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBlade/i1";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case CYBERBLADEI1D:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBlade/DSTN/i1";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case CYBERBLADEAI1:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBlade/Ai1";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case CYBERBLADEAI1D:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SDRAM";
	    pTrident->IsCyber = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBlade/DSTN/Ai1";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
	case BLADEXP: /* 0x9910 */
	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SGRAM";
	    pTrident->HasSGRAM = TRUE;
	    Support24bpp = TRUE;
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    OUTB(0x3C4, 0x5D);
	    if (pTrident->PciInfo->subsysVendor != 0x1023) {
	    	chipset = "CyberBladeXP";
	    	pTrident->IsCyber = TRUE;
	    } else
	    if (!(INB(0x3C5) & 0x01)) {
	    	chipset = "BladeXP";
	    } else {
		CARD8 mem1, mem2;
		OUTB(vgaIOBase + 0x04, SPR);
		mem1 = INB(vgaIOBase + 5);
		OUTB(vgaIOBase + 0x04, 0xC1);
		mem2 = INB(vgaIOBase + 5);
		if ((mem1 & 0x0e) && (mem2 == 0x11)) {
	    	    chipset = "BladeT64";
		} else {
	    	    chipset = "BladeT16";
		}
	    }
	    break;
	case CYBERBLADEXPAI1:
    	    pTrident->ddc1Read = Tridentddc1Read;
	    ramtype = "SGRAM";
            pTrident->HasSGRAM = TRUE;
	    pTrident->IsCyber = TRUE;
	    pTrident->shadowNew = TRUE;
	    Support24bpp = TRUE;
	    chipset = "CyberBladeXPAi1";
	    pTrident->NewClockCode = TRUE;
	    pTrident->frequency = NTSC;
	    break;
    }

    if (!pScrn->progClock) {
	pScrn->numClocks = NoClocks;
	xf86GetClocks(pScrn, NoClocks, TRIDENTClockSelect,
			  vgaHWProtect, vgaHWBlankScreen,
			  vgaIOBase + 0x0A, 0x08, 1, 28322);
	from = X_PROBED;
	xf86ShowClocks(pScrn, from);
    }

    if (!chipset) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No support for \"%s\"\n",
			pScrn->chipset);
	if (IsPciCard && UseMMIO) {
    	    TRIDENTDisableMMIO(pScrn);
 	    TRIDENTUnmapMem(pScrn);
	}
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Found %s chip\n", chipset);
    if (ramtype)
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "RAM type is %s\n", ramtype);

    if (pScrn->bitsPerPixel == 24 && !Support24bpp) {
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "No support for 24bpp on this chipset, use -pixmap32.\n");
	if (IsPciCard && UseMMIO) {
    	    TRIDENTDisableMMIO(pScrn);
 	    TRIDENTUnmapMem(pScrn);
	}
	return FALSE;
    }

    /* HW bpp matches reported bpp */
    pTrident->HwBpp = pScrn->bitsPerPixel;

    /* Due to bugs in the chip, turn it off */
    if (pTrident->Chipset >= CYBERBLADEI7 && pTrident->Chipset <= CYBERBLADEAI1D)
	pTrident->HWCursor = FALSE;

    from = X_PROBED;
    if (pTrident->pEnt->device->videoRam != 0) {
	pScrn->videoRam = pTrident->pEnt->device->videoRam;
	from = X_CONFIG;

	/* Due to only 12bits of cursor location, if user has overriden
	 * disable the cursor automatically */
	if (pTrident->Chipset >= CYBER9397 && pTrident->Chipset < CYBERBLADEE4)
		if (pTrident->pEnt->device->videoRam > 4096)
			pTrident->HWCursor = FALSE;
    } else {
      if (pTrident->Chipset == CYBER9525DVD) {
	pScrn->videoRam = 2560;
      } else
      {
	OUTB(vgaIOBase + 4, SPR);
	videoram = INB(vgaIOBase + 5);
	if (pTrident->Chipset < TGUI9440AGi)
	    videorammask = 0x07;
	else
	    videorammask = 0x0F;
	switch (videoram & videorammask) {
  	case 0x01:
  	    pScrn->videoRam = 512;
  	    break;
 	case 0x02: /* XP */
 	    pScrn->videoRam = 6144;
 	    break;
  	case 0x03:
  	    pScrn->videoRam = 1024;
  	    break;
	case 0x04: 
	    /* 
	     * 8MB, but - hw cursor can't store above 4MB
	     * This only affects Image series chipsets, but for
	     * some reason, reports suggest that the 9397DVD isn't
	     * affected. XXX needs furthur investigation !
	     */
	    if (pTrident->HWCursor && (pTrident->Chipset != CYBER9397DVD) &&
	      			    (pTrident->Chipset < CYBERBLADEE4)) {
	    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, 
		       "Found 8MB board, using 4MB\n");
	    	pScrn->videoRam = 4096;
	    } else
	    	pScrn->videoRam = 8192;
	    break;
 	case 0x06: /* XP */
 	    pScrn->videoRam = 10240;
 	    break;
  	case 0x07:
  	    pScrn->videoRam = 2048;
  	    break;
 	case 0x08: /* XP */
 	    pScrn->videoRam = 12288;
 	    break;
 	case 0x0A: /* XP */
 	    pScrn->videoRam = 14336;
 	    break;
 	case 0x0C: /* XP */
 	    pScrn->videoRam = 16384;
 	    break;
 	case 0x0E: /* XP */
 	    OUTB(vgaIOBase + 4, 0xC1);
 	    switch (INB(vgaIOBase + 5) & 0x11) {
 		case 0x00:
 		    pScrn->videoRam = 20480;
 		    break;
 		case 0x01:
 		    pScrn->videoRam = 24576;
 		    break;
 		case 0x10:
 		    pScrn->videoRam = 28672;
 		    break;
 		case 0x11:
 		    pScrn->videoRam = 32768;
 		    break;
 	    }
 	    break;
 	case 0x0F:
  	    pScrn->videoRam = 4096;
  	    break;
	default:
	    pScrn->videoRam = 1024;
	    xf86DrvMsg(pScrn->scrnIndex, from, 
			"Unable to determine VideoRam, defaulting to 1MB\n",
				pScrn->videoRam);
	    break;
	}
      }
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pTrident->HWCursor ? "HW" : "SW");

    xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d kByte\n",
               pScrn->videoRam);

    if (pTrident->IsCyber) {
	unsigned char mod, dsp, dsp1;
	
	pTrident->lcdMode = 0xff;

	OUTB(0x3CE,0x42);
	dsp = INB(0x3CF);
	OUTB(0x3CE,0x43);
	dsp1 = INB(0x3CF);
	OUTB(0x3CE,0x52);
	mod = INB(0x3CF);
	/*
	 * Only allow display size override if 1280x1024 is detected 
	 * Currently 1400x1050 is supported - which is detected as
	 * 1280x1024
	 */
	if (pTrident->displaySize) {
	    if (((mod >> 4) & 3) == 0) {
		for (i = 0; LCD[i].mode != 0xff; i++) {
		    if (pTrident->displaySize == LCD[i].display_x)
			pTrident->lcdMode = LCD[i].mode;
		}
		xf86DrvMsg(pScrn->scrnIndex,
			   X_CONFIG,"%s Panel %ix%i found\n",
			   (dsp & 0x80) ? "TFT" :
			   ((dsp1 & 0x20) ? "DSTN" : "STN"), 
			   LCD[i].display_x,LCD[i].display_y);		
	    } else {
		xf86DrvMsg(pScrn->scrnIndex,X_WARNING,
			   "Display size override only for 1280x1024\n");
		pTrident->displaySize = 0;
	    }
	}
	
	if (!pTrident->displaySize) {
	    for (i = 0; LCD[i].mode != 0xff; i++) {
		if (LCD[i].mode == ((mod >> 4) & 3)) {
		    pTrident->lcdMode = i;
		    xf86DrvMsg(pScrn->scrnIndex,
			       X_PROBED,"%s Panel %ix%i found\n",
			       (dsp & 0x80) ? "TFT" :
			       ((dsp1 & 0x20) ? "DSTN" : "STN"), 
			       LCD[i].display_x,LCD[i].display_y);
		}
	    }
	}
	if (pTrident->dspOverride) {
	    if (pTrident->dspOverride & LCD_ACTIVE)
		pTrident->lcdActive = TRUE;
	    else
		pTrident->lcdActive = FALSE;
	} else {
	    OUTB(0x3CE, FPConfig);
	    pTrident->lcdActive = (INB(0x3CF) & 0x10);
	}
    }

    pTrident->MCLK = 0;
    mclk = CalculateMCLK(pScrn);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Memory Clock is %3.2f MHz\n", mclk);
    if (xf86GetOptValFreq(pTrident->Options, OPTION_SETMCLK, OPTUNITS_MHZ,
				&real)) {
	pTrident->MCLK = (int)(real * 1000.0);
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Setting new Memory Clock to %3.2f MHz\n",
						(float)(pTrident->MCLK / 1000));
    }
		
    /* Set the min pixel clock */
    pTrident->MinClock = 12000;	/* XXX Guess, need to check this */
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n",
	       pTrident->MinClock / 1000);

    /*
     * If the user has specified ramdac speed in the XF86Config
     * file, we respect that setting.
     */
    if (pTrident->pEnt->device->dacSpeeds[0]) {
	int speed = 0;

	switch (pScrn->bitsPerPixel) {
	case 8:
	   speed = pTrident->pEnt->device->dacSpeeds[DAC_BPP8];
	   break;
	case 16:
	   speed = pTrident->pEnt->device->dacSpeeds[DAC_BPP16];
	   break;
	case 24:
	   speed = pTrident->pEnt->device->dacSpeeds[DAC_BPP24];
	   break;
	case 32:
	   speed = pTrident->pEnt->device->dacSpeeds[DAC_BPP32];
	   break;
	}
	if (speed == 0)
	    pTrident->MaxClock = pTrident->pEnt->device->dacSpeeds[0];
	else
	    pTrident->MaxClock = speed;
	from = X_CONFIG;
    } else {
	switch (pScrn->bitsPerPixel) {
	    case 16:
		pTrident->MaxClock = ClockLimit16bpp[pTrident->Chipset];
		break;
	    case 24:
		pTrident->MaxClock = ClockLimit24bpp[pTrident->Chipset];
		break;
	    case 32:
		pTrident->MaxClock = ClockLimit32bpp[pTrident->Chipset];
		break;
	    default:
		pTrident->MaxClock = ClockLimit[pTrident->Chipset];
		break;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Max pixel clock is %d MHz\n",
	       pTrident->MaxClock / 1000);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    if (!pScrn->progClock) {
	if (pScrn->videoRam < 1024)
    	    clockRanges->ClockMulFactor = 2;
	if (pScrn->bitsPerPixel == 16)
    	    clockRanges->ClockMulFactor = 2;
    }
    clockRanges->minClock = pTrident->MinClock;
    clockRanges->maxClock = pTrident->MaxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = FALSE;	/* XXX check this */

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our TRIDENTValidMode() already takes
     * care of this, we don't worry about setting them here.
     */

    if ((pScrn->depth < 8) || 
        (pScrn->bitsPerPixel == 24)) {
    	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, 
		"Disabling Engine due to 24bpp or < 8bpp.\n");
	    pTrident->NoAccel = TRUE;
    }

    /* Select valid modes from those available */
    if (pTrident->NoAccel || Is3Dchip) {
	/*
	 * XXX Assuming min pitch 256, max 4096
	 * XXX Assuming min height 128, max 4096
	 */
	i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			      pScrn->display->modes, clockRanges,
			      NULL, 256, 4096,
			      pScrn->bitsPerPixel, 128, 4096,
			      pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      pTrident->FbMapSize,
			      LOOKUP_BEST_REFRESH);
    } else {
	/*
	 * XXX Assuming min height 128, max 2048
	 */
	i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			      pScrn->display->modes, clockRanges,
			      GetAccelPitchValues(pScrn), 0, 0,
			      pScrn->bitsPerPixel, 128, 2048,
			      pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      pTrident->FbMapSize,
			      LOOKUP_BEST_REFRESH);
    }

    if (i == -1) {
	if (IsPciCard && UseMMIO) {
    	    TRIDENTDisableMMIO(pScrn);
 	    TRIDENTUnmapMem(pScrn);
	}
	TRIDENTFreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	if (IsPciCard && UseMMIO) {
    	    TRIDENTDisableMMIO(pScrn);
 	    TRIDENTUnmapMem(pScrn);
	}
	TRIDENTFreeRec(pScrn);
	return FALSE;
    }

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    /* Load bpp-specific modules */
    switch (pScrn->bitsPerPixel) {
    case 1:
	pTrident->EngineOperation |= 0x00;
	mod = "xf1bpp";
	Sym = "xf1bppScreenInit";
	break;
    case 4:
	pTrident->EngineOperation |= 0x00;
	mod = "xf4bpp";
	Sym = "xf4bppScreenInit";
	break;
    case 8:
	pTrident->EngineOperation |= 0x00;
	mod = "fb";
	break;
    case 16:
	pTrident->EngineOperation |= 0x01;
	mod = "fb";
	break;
    case 24:
	pTrident->EngineOperation |= 0x03;
	mod = "fb";
	break;
    case 32:
	pTrident->EngineOperation |= 0x02;
	mod = "fb";
	break;
    }

    if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
	if (IsPciCard && UseMMIO) {
    	    TRIDENTDisableMMIO(pScrn);
 	    TRIDENTUnmapMem(pScrn);
	}
	TRIDENTFreeRec(pScrn);
	return FALSE;
    }

    if (mod) {
	if (Sym) {
	    xf86LoaderReqSymbols(Sym, NULL);
	} else {
	    xf86LoaderReqSymLists(fbSymbols, NULL);
	}
    }

    if (!xf86LoadSubModule(pScrn, "i2c")) {
	if (IsPciCard && UseMMIO) {
    	    TRIDENTDisableMMIO(pScrn);
 	    TRIDENTUnmapMem(pScrn);
	}
	TRIDENTFreeRec(pScrn);
	return FALSE;
    }

    xf86LoaderReqSymLists(i2cSymbols, NULL);

    /* Load shadow if needed */
    if (pTrident->ShadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadow")) {
	    TRIDENTFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    /* Load XAA if needed */
    if (!pTrident->NoAccel) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    if (IsPciCard && UseMMIO) {
    	    	TRIDENTDisableMMIO(pScrn);
 	    	TRIDENTUnmapMem(pScrn);
	    }
	    TRIDENTFreeRec(pScrn);
	    return FALSE;
	}

        xf86LoaderReqSymLists(xaaSymbols, NULL);

        switch (pScrn->displayWidth * pScrn->bitsPerPixel / 8) {
	    case 512:
	    case 8192:
		pTrident->EngineOperation |= 0x00;
		break;
	    case 1024:
		pTrident->EngineOperation |= 0x04;
		break;
	    case 2048:
		pTrident->EngineOperation |= 0x08;
		break;
	    case 4096:
		pTrident->EngineOperation |= 0x0C;
		break;
	}
    }

    /* Load DDC if needed */
    /* This gives us DDC1 - we should be able to get DDC2B using i2c */

    if (! ddcLoaded)
	if (!xf86LoadSubModule(pScrn, "ddc")) {
	    if (IsPciCard && UseMMIO) {
		TRIDENTDisableMMIO(pScrn);
		TRIDENTUnmapMem(pScrn);
	    }
	    TRIDENTFreeRec(pScrn);
	    return FALSE;
	}
    
    xf86LoaderReqSymLists(ddcSymbols, NULL);

    if (IsPciCard && UseMMIO) {
        TRIDENTDisableMMIO(pScrn);
	TRIDENTUnmapMem(pScrn);
    }


    pTrident->FbMapSize = pScrn->videoRam * 1024;
    
    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

    if (pTrident->IsCyber && pTrident->MMIOonly)
	pScrn->racIoFlags = 0;
    else 
	pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

    return TRUE;
}

/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
TRIDENTMapMem(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    int mapsize = 0x10000;

    if (Is3Dchip) mapsize = 0x20000;

    if (IsPciCard && UseMMIO)
    	pTrident->IOBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, 
		pTrident->PciTag, pTrident->IOAddress, mapsize);
    else {
    	pTrident->IOBase = xf86MapDomainMemory(pScrn->scrnIndex, VIDMEM_MMIO, 
		pTrident->PciTag, pTrident->IOAddress, 0x1000);
    	pTrident->IOBase += 0xF00;
    }

    if (pTrident->IOBase == NULL)
	return FALSE;

    if (pTrident->Linear) {
        if (pTrident->FbMapSize != 0) {
	    pTrident->FbBase = xf86MapPciMem(pScrn->scrnIndex, 
				VIDMEM_FRAMEBUFFER,
				 pTrident->PciTag,
				 (unsigned long)pTrident->FbAddress,
				 pTrident->FbMapSize);
    	if (pTrident->FbBase == NULL)
	    return FALSE;
    	}
    }
    else
	pTrident->FbBase = hwp->Base;

    return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
TRIDENTUnmapMem(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int mapsize = 0x10000;

    if (Is3Dchip) mapsize = 0x20000;

    /*
     * Unmap IO registers to virtual address space
     */ 
    if (IsPciCard && UseMMIO) 
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTrident->IOBase, mapsize);
    else {
    	pTrident->IOBase -= 0xF00;
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTrident->IOBase, 0x1000);
    }
    pTrident->IOBase = NULL;

    if (pTrident->Linear) {
    	if (pTrident->FbMapSize != 0) {
    	    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pTrident->FbBase, 
							pTrident->FbMapSize);
    	pTrident->FbBase = NULL;
        }
    }

    return TRUE;
}


/*
 * This function saves the video state.
 */
static void
TRIDENTSave(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident;
    vgaRegPtr vgaReg;
    TRIDENTRegPtr tridentReg;

    pTrident = TRIDENTPTR(pScrn);
    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    tridentReg = &pTrident->SavedReg;

    vgaHWSave(pScrn, vgaReg, VGA_SR_MODE | VGA_SR_CMAP |
			     (IsPrimaryCard ? VGA_SR_FONTS : 0));

    if (pScrn->progClock)
    	TridentSave(pScrn, tridentReg);
    else
    	TVGASave(pScrn, tridentReg);
}


/*
 * Initialise a new mode.  This is currently still using the old
 * "initialise struct, restore/write struct to HW" model.  That could
 * be changed.
 */

static Bool
TRIDENTModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    TRIDENTRegPtr tridentReg;
    int clock;

    TridentFindClock(pScrn,mode->Clock);

    clock = pTrident->currentClock;

    switch (pTrident->Chipset) {
	case TGUI9660:
	case TGUI9680:
	case PROVIDIA9682:
	case PROVIDIA9685:
	case IMAGE975:
	case IMAGE985:
	case BLADE3D:
	case CYBERBLADEI7:
	case CYBERBLADEI7D:
	case CYBERBLADEI1:
	case CYBERBLADEI1D:
	case CYBERBLADEAI1:
	case CYBERBLADEAI1D:
	case CYBER9520:
	case CYBER9525DVD:
	case CYBERBLADEE4:
	case CYBER9397:
	case CYBER9397DVD:
	case BLADEXP:
	case CYBERBLADEXPAI1:
	    /* Get ready for MUX mode */
	    if (pTrident->MUX && 
		pScrn->bitsPerPixel == 8 && 
		!mode->CrtcHAdjusted) {
		mode->CrtcHDisplay >>= 1;
		mode->CrtcHSyncStart >>= 1;
		mode->CrtcHSyncEnd >>= 1;
		mode->CrtcHBlankStart >>= 1;
		mode->CrtcHBlankEnd >>= 1;
		mode->CrtcHTotal >>= 1;
		mode->CrtcHAdjusted = TRUE;
	    }
	    break;
	default:
	    if (pScrn->videoRam < 1024 &&
		!mode->CrtcHAdjusted) {
		mode->CrtcHDisplay <<= 1;
		mode->CrtcHSyncStart <<= 1;
		mode->CrtcHSyncEnd <<= 1;
		mode->CrtcHBlankStart <<= 1;
		mode->CrtcHBlankEnd <<= 1;
		mode->CrtcHTotal <<= 1;
		mode->CrtcHAdjusted = TRUE;
	    }
	    break;
    }

    vgaHWUnlock(hwp);
    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, mode))
	return FALSE;
    pScrn->vtSema = TRUE;
    /*
     * We used to do this at a later time. 
     * Now since READOUT isn't defined any more
     * we do it here. 
     * The original NOTE read:
     * TridentInit() has to modify registers
     * that have already been set by vgaHWRestore().
     * So we call it _after_ vgaHWRestore() has
     * programmed these registers.
     */
    if (pScrn->progClock) {
    	if (!TridentInit(pScrn, mode))
	    return FALSE;
    } else {
    	if (!TVGAInit(pScrn, mode))
	    return FALSE;
    }

    /* Program the registers */
    vgaHWProtect(pScrn, TRUE);
    vgaReg = &hwp->ModeReg;
    tridentReg = &pTrident->ModeReg;

    vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);
    if (pScrn->progClock)
    	TridentRestore(pScrn, tridentReg);
    else
	TVGARestore(pScrn, tridentReg);

    vgaHWProtect(pScrn, FALSE);

    if (xf86IsPc98())
	PC98TRIDENTEnable(pScrn);

    return TRUE;
}

/*
 * Restore the initial (text) mode.
 */
static void 
TRIDENTRestore(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp;
    vgaRegPtr vgaReg;
    TRIDENTPtr pTrident;
    TRIDENTRegPtr tridentReg;

    hwp = VGAHWPTR(pScrn);
    pTrident = TRIDENTPTR(pScrn);
    vgaReg = &hwp->SavedReg;
    tridentReg = &pTrident->SavedReg;

    vgaHWProtect(pScrn, TRUE);

    if (pScrn->progClock)
    	TridentRestore(pScrn, tridentReg);
    else
	TVGARestore(pScrn, tridentReg);

    vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE | VGA_SR_CMAP |
				(IsPrimaryCard ? VGA_SR_FONTS : 0));

    vgaHWProtect(pScrn, FALSE);
}


/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
TRIDENTScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    /* The vgaHW references will disappear one day */
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    int vgaIOBase;
    TRIDENTPtr pTrident;
    int ret;
    VisualPtr visual;
    unsigned char *FBStart;
    int width, height, displayWidth;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];
    pTrident = TRIDENTPTR(pScrn);

    if (IsPrimaryCard) {
	if (!vgaHWMapMem(pScrn))
   	    return FALSE;
    }

    /* Map the TRIDENT memory and MMIO areas */
    if (!TRIDENTMapMem(pScrn))
	return FALSE;

    if (!xf86IsPc98()) {
#ifdef VBE_INFO
	if (pTrident->vbeModes) {
	    pTrident->pVbe = VBEInit(NULL,pTrident->pEnt->index);
	    pTrident->Int10 = pTrident->pVbe->pInt10;
	} else
#endif
	{
	    if (xf86LoadSubModule(pScrn, "int10")) {
		xf86LoaderReqSymLists(int10Symbols, NULL);
		xf86DrvMsg(pScrn->scrnIndex,X_INFO,"Initializing int10\n");
		pTrident->Int10 = xf86InitInt10(pTrident->pEnt->index);
	    }
	}
    }
    
    hwp = VGAHWPTR(pScrn);

    if (IsPciCard && UseMMIO) {
    	TRIDENTEnableMMIO(pScrn);

    	/* Initialize the MMIO vgahw functions */
    	vgaHWSetMmioFuncs(hwp, pTrident->IOBase, 0);
    }

    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    /* Save the current state */
    TRIDENTSave(pScrn);

    /*
     * Some Trident chip on PC-9821 needs setup,
     * because VGA chip is not initialized by VGA BIOS.
     */
    if (IsPciCard && xf86IsPc98()) {
	 PC98TRIDENTInit(pScrn);
    } else tridentSetModeBIOS(pScrn,pScrn->currentMode);

    /* Initialise the first mode */
    if (!TRIDENTModeInit(pScrn, pScrn->currentMode))
	return FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    TRIDENTSaveScreen(pScreen, SCREEN_SAVER_ON);
    TRIDENTAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

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
     * Reset visual list.
     */
    miClearVisualTypes();

    /* Setup the visuals we support. */

    if (!miSetVisualTypes(pScrn->depth, 
			  miGetDefaultVisualMask(pScrn->depth),
			  pScrn->rgbBits, pScrn->defaultVisual)) {
	if (pTrident->pVbe)
	    	vbeFree(pTrident->pVbe);
	else
	    xf86FreeInt10(pTrident->Int10);
      return FALSE;
    }

    miSetPixmapDepths ();

    /* FIXME - we don't do shadowfb for < 4 */
    displayWidth = pScrn->displayWidth;
    if (pTrident->Rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    } else {
	width = pScrn->virtualX;
	height = pScrn->virtualY;
    }

    if(pTrident->ShadowFB) {
 	pTrident->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pTrident->ShadowPtr = xalloc(pTrident->ShadowPitch * height);
	displayWidth = pTrident->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pTrident->ShadowPtr;
    } else {
	pTrident->ShadowFB = FALSE;
	pTrident->ShadowPtr = NULL;
	FBStart = pTrident->FbBase;
    }

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    switch (pScrn->bitsPerPixel) {
    case 1:
	ret = xf1bppScreenInit(pScreen, FBStart, width,
			height, pScrn->xDpi, pScrn->yDpi, 
			displayWidth);
	break;
    case 4:
	ret = xf4bppScreenInit(pScreen, FBStart, width,
			height, pScrn->xDpi, pScrn->yDpi, 
			displayWidth);
	break;
    case 8:
    case 16:
    case 24:
    case 32:
	ret = fbScreenInit(pScreen, FBStart, width,
			height, pScrn->xDpi, pScrn->yDpi, 
			displayWidth, pScrn->bitsPerPixel);
    
	break;
    default:
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Internal error: invalid bpp (%d) in TRIDENTScrnInit\n",
		   pScrn->bitsPerPixel);
	    ret = FALSE;
	break;
    }
    if (!ret) {
	if (pTrident->pVbe)
	    vbeFree(pTrident->pVbe);
	else
	    xf86FreeInt10(pTrident->Int10);
	return FALSE;
    }
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
    } else {
	if (pScrn->bitsPerPixel == 1) {
	    TRIDENTFix1bpp(pScrn);
	}
    }

    /* must be after RGB ordering fixed */
    if (pScrn->bitsPerPixel > 4)
	fbPictureInit (pScreen, 0, 0);

    xf86SetBlackWhitePixels(pScreen);

    pTrident->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = TRIDENTBlockHandler;

    if (!pTrident->ShadowFB)
	TRIDENTDGAInit(pScreen);

    if (!pTrident->Linear) {
	miBankInfoPtr pBankInfo;

	/* Setup the vga banking variables */
	pBankInfo = xnfcalloc(sizeof(miBankInfoRec),1);
	if (pBankInfo == NULL) {
	    if (pTrident->pVbe)
		vbeFree(pTrident->pVbe);
	    else
		xf86FreeInt10(pTrident->Int10);
	    return FALSE;
	}
	pBankInfo->pBankA = pTrident->FbBase;
	pBankInfo->pBankB = pTrident->FbBase;
	pBankInfo->BankSize = 0x10000;
	pBankInfo->nBankDepth = (pScrn->depth == 4) ? 1 : pScrn->depth;

	pBankInfo->SetSourceBank = 
		(miBankProcPtr)TVGA8900SetRead;
	pBankInfo->SetDestinationBank = 
		(miBankProcPtr)TVGA8900SetWrite;
	pBankInfo->SetSourceAndDestinationBanks = 
		(miBankProcPtr)TVGA8900SetReadWrite;
	if (!miInitializeBanking(pScreen, pScrn->virtualX, pScrn->virtualY,
				 pScrn->displayWidth, pBankInfo)) {
	    xfree(pBankInfo);
	    pBankInfo = NULL;
	    if (pTrident->pVbe)
	    	vbeFree(pTrident->pVbe);
	    else
		xf86FreeInt10(pTrident->Int10);
	    return FALSE;
	}
    }

    if (Is3Dchip) {
	if ((pTrident->Chipset == CYBERBLADEI7) ||
	    (pTrident->Chipset == CYBERBLADEI7D) ||
	    (pTrident->Chipset == CYBERBLADEI1) ||
	    (pTrident->Chipset == CYBERBLADEI1D) ||
	    (pTrident->Chipset == CYBERBLADEAI1) ||
	    (pTrident->Chipset == CYBERBLADEAI1D) ||
	    (pTrident->Chipset == CYBERBLADEE4) ||
	    (pTrident->Chipset == BLADE3D))
		BladeAccelInit(pScreen);
	    else
	    if (pTrident->Chipset >= BLADEXP)
		XPAccelInit(pScreen);
	    else
	    	ImageAccelInit(pScreen);
    } else {
    	TridentAccelInit(pScreen);
    }

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    if (pScrn->bitsPerPixel < 8) 
	pTrident->HWCursor = FALSE;

    if (pTrident->HWCursor) {
        xf86SetSilkenMouse(pScreen);
	TridentHWCursorInit(pScreen);
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen)) {
	if (pTrident->pVbe)
	    vbeFree(pTrident->pVbe);
	else
	    xf86FreeInt10(pTrident->Int10);
	return FALSE;
    }
    if(!xf86HandleColormaps(pScreen, 256, 6, TridentLoadPalette,
			    TridentSetOverscan, CMAP_RELOAD_ON_MODE_SWITCH|CMAP_PALETTED_TRUECOLOR)) {
	if (pTrident->pVbe)
	    vbeFree(pTrident->pVbe);
	else
	    xf86FreeInt10(pTrident->Int10);
	return FALSE;
    }
    if(pTrident->ShadowFB) {
        if(pTrident->Rotate) {
	    if (!pTrident->PointerMoved) {
	        pTrident->PointerMoved = pScrn->PointerMoved;
		pScrn->PointerMoved = TRIDENTPointerMoved;
	    }
	    switch (pScrn->bitsPerPixel) {
	    case 8:    pTrident->RefreshArea = TRIDENTRefreshArea8; break;
	    case 16:   pTrident->RefreshArea = TRIDENTRefreshArea16; break;
	    case 24:   pTrident->RefreshArea = TRIDENTRefreshArea24; break;
	    case 32:   pTrident->RefreshArea = TRIDENTRefreshArea32; break;
	    }
	} else {
	  pTrident->RefreshArea = TRIDENTRefreshArea;
	}
	shadowInit (pScreen, TRIDENTShadowUpdate, 0);
    }

    xf86DPMSInit(pScreen, (DPMSSetProcPtr)TRIDENTDisplayPowerManagementSet, 0);

    pScrn->memPhysBase = pTrident->FbAddress;
    pScrn->fbOffset = 0;

#ifdef XvExtension
    if (pTrident->Chipset >= TGUI9660)
	TRIDENTInitVideo(pScreen);
#endif

    pTrident->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = TRIDENTCloseScreen;
    pScreen->SaveScreen = TRIDENTSaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

#if 0
    TRIDENTI2CInit(pScreen);

    xf86PrintEDID(xf86DoEDID_DDC2(pScrn->scrnIndex,pTrident->DDC));
#endif

    return TRUE;
}

/* Usually mandatory */
Bool
TRIDENTSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return TRIDENTModeInit(xf86Screens[scrnIndex], mode);
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
void 
TRIDENTAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TRIDENTPtr pTrident;
    vgaHWPtr hwp;
    int base = y * pScrn->displayWidth + x;
    int vgaIOBase;
    CARD8 temp;

    hwp = VGAHWPTR(pScrn);
    pTrident = TRIDENTPTR(pScrn);
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    switch (pScrn->bitsPerPixel) {
	case 4:
	    base >>= 3;
	    break;
	case 8:
	    if (pScrn->progClock)
	    	base = (base & 0xFFFFFFF8) >> 2;
	    else
	    	base = (base & 0xFFFFFFF8) >> 3;
	    break;
	case 16:
	    base >>= 1;
	    break;
	case 24:
	    base = (((base + 1) & ~0x03) * 3) >> 2;
	    break;
	case 32:
	    break;
    }

    /* CRT bits 0-15 */
    OUTW(vgaIOBase + 4, (base & 0x00FF00) | 0x0C);
    OUTW(vgaIOBase + 4, ((base & 0x00FF) << 8) | 0x0D);
    /* CRT bit 16 */
    OUTB(vgaIOBase + 4, CRTCModuleTest); temp = INB(vgaIOBase + 5) & 0xDF;
    OUTB(vgaIOBase + 5, temp | ((base & 0x10000) >> 11));
    /* CRT bit 17-19 */
    OUTB(vgaIOBase + 4, CRTHiOrd); temp = INB(vgaIOBase + 5) & 0xF8;
    OUTB(vgaIOBase + 5, temp | ((base & 0xE0000) >> 17));
}


/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 */

/* Mandatory */
static Bool
TRIDENTEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (IsPciCard && UseMMIO) TRIDENTEnableMMIO(pScrn);

    /* Should we re-save the text mode on each VT enter? */
    if (!TRIDENTModeInit(pScrn, pScrn->currentMode))
	return FALSE;

    if (pTrident->InitializeAccelerator)
        pTrident->InitializeAccelerator(pScrn);

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
TRIDENTLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    TRIDENTRestore(pScrn);
    vgaHWLock(hwp);

    if (xf86IsPc98())
	PC98TRIDENTDisable(pScrn);

    if (IsPciCard && UseMMIO) TRIDENTDisableMMIO(pScrn);
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
TRIDENTCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (xf86IsPc98())
	PC98TRIDENTDisable(pScrn);

    if (pScrn->vtSema) {
    	TRIDENTRestore(pScrn);
    	vgaHWLock(hwp);
	if (IsPciCard && UseMMIO) TRIDENTDisableMMIO(pScrn);
    	TRIDENTUnmapMem(pScrn);
    }
    if (pTrident->AccelInfoRec)
	XAADestroyInfoRec(pTrident->AccelInfoRec);
    if (pTrident->CursorInfoRec)
	xf86DestroyCursorInfoRec(pTrident->CursorInfoRec);
    if (pTrident->ShadowPtr)
	xfree(pTrident->ShadowPtr);
    if (pTrident->DGAModes)
	xfree(pTrident->DGAModes);
    pScrn->vtSema = FALSE;

    if(pTrident->BlockHandler)
	pScreen->BlockHandler = pTrident->BlockHandler;
    
    if (pTrident->pVbe)
	vbeFree(pTrident->pVbe);
    else
	xf86FreeInt10(pTrident->Int10);
    pScreen->CloseScreen = pTrident->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any per-generation data structures */

/* Optional */
static void
TRIDENTFreeScreen(int scrnIndex, int flags)
{
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    TRIDENTFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
TRIDENTValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    
    if (pTrident->lcdActive && (pTrident->lcdMode != 0xff)){
	if (((mode->HDisplay > LCD[pTrident->lcdMode].display_x) 
	|| (mode->VDisplay > LCD[pTrident->lcdMode].display_y))) {
	    xf86DrvMsg(scrnIndex,X_INFO, "Removing mode (%dx%d) "
		       "larger than the LCD panel (%dx%d)\n",
		       mode->HDisplay,
		       mode->VDisplay,
		       LCD[pTrident->lcdMode].display_x,
		       LCD[pTrident->lcdMode].display_y);
	    return(MODE_BAD);
	}
	if (((float)mode->HDisplay/(float)mode->VDisplay) > 2.0) {
	    xf86DrvMsg(scrnIndex,X_INFO, "Removing mode (%dx%d) "
		       "unusual aspect ratio\n",
		       mode->HDisplay,
		       mode->VDisplay);
	    return(MODE_BAD);
	}
    }
    return (MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
TRIDENTSaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}

static void
TRIDENTEnableMMIO(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    IOADDRESS vgaIOBase = pTrident->PIOBase + VGAHWPTR(pScrn)->IOBase;
    CARD8 temp = 0, protect = 0;

    /*
     * Skip MMIO Enable in PC-9821 PCI Trident Card!!
     * Because of lack of non PCI VGA port
     */
    if (IsPciCard && xf86IsPc98())
      return;

    /* Goto New Mode */
    outb(pTrident->PIOBase + 0x3C4, 0x0B);
    inb(pTrident->PIOBase + 0x3C5);

    /* Unprotect registers */
    if (pTrident->Chipset > PROVIDIA9685) {
    	outb(pTrident->PIOBase + 0x3C4, Protection);
    	protect = inb(pTrident->PIOBase + 0x3C5);
    	outb(pTrident->PIOBase + 0x3C5, 0x92);
    }
    outb(pTrident->PIOBase + 0x3C4, NewMode1);
    temp = inb(pTrident->PIOBase + 0x3C5);
    outb(pTrident->PIOBase + 0x3C5, 0x80);

    /* Enable MMIO */
    outb(vgaIOBase + 4, PCIReg);
    pTrident->REGPCIReg = inb(vgaIOBase + 5);
    outb(vgaIOBase + 5, pTrident->REGPCIReg | 0x01); /* Enable it */

    /* Protect registers */
    if (pTrident->Chipset > PROVIDIA9685) {
    	OUTB(0x3C4, Protection);
    	OUTB(0x3C5, protect);
    }
    OUTB(0x3C4, NewMode1);
    OUTB(0x3C5, temp);
}

static void
TRIDENTDisableMMIO(ScrnInfoPtr pScrn)
{
    int vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    CARD8 temp = 0, protect = 0;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    /*
     * Skip MMIO Disable in PC-9821 PCI Trident Card!!
     * Because of lack of non PCI VGA port
     */
    if (IsPciCard && xf86IsPc98())
      return;

    /* Goto New Mode */
    OUTB(0x3C4, 0x0B); temp = INB(0x3C5);

    /* Unprotect registers */
    OUTB(0x3C4, NewMode1); temp = INB(0x3C5);
    OUTB(0x3C5, 0x80);
    if (pTrident->Chipset > PROVIDIA9685) {
    	OUTB(0x3C4, Protection);
    	protect = INB(0x3C5);
    	OUTB(0x3C5, 0x92);
    }

    /* Disable MMIO access */
    OUTB(vgaIOBase + 4, PCIReg); 
    pTrident->REGPCIReg = INB(vgaIOBase + 5);
    OUTB(vgaIOBase + 5, pTrident->REGPCIReg & 0xFE);

    /* Protect registers */
    if (pTrident->Chipset > PROVIDIA9685) {
    	outb(pTrident->PIOBase + 0x3C4, Protection);
    	outb(pTrident->PIOBase + 0x3C5, protect);
    }
    outb(pTrident->PIOBase + 0x3C4, NewMode1);
    outb(pTrident->PIOBase + 0x3C5, temp);
}

/* Initialize VGA Block for Trident Chip on PC-98x1 */
static void
PC98TRIDENTInit(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    switch (pTrident->Chipset) {
    case TGUI9660:
    case TGUI9680:
    case PROVIDIA9682:
	PC98TRIDENT96xxInit(pScrn);
	break;
    case CYBER9320:
    case CYBER9385:
	PC98TRIDENT9385Init(pScrn);
	break;
    default: /* Run 96xx code as default */
	PC98TRIDENT96xxInit(pScrn);
	break;
    }
}

static void
PC98TRIDENTEnable(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    switch (pTrident->Chipset) {
    case TGUI9660:
    case TGUI9680:
    case PROVIDIA9682:
	PC98TRIDENT96xxEnable(pScrn);
	break;
    case CYBER9320:
    case CYBER9385:
	PC98TRIDENT9385Enable(pScrn);
	break;
    default: /* Run 96xx code as default */
	PC98TRIDENT96xxEnable(pScrn);
	break;
    }
}

static void
PC98TRIDENTDisable(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    switch (pTrident->Chipset) {
    case TGUI9660:
    case TGUI9680:
    case PROVIDIA9682:
	PC98TRIDENT96xxDisable(pScrn);
	break;
    case CYBER9320:
    case CYBER9385:
	PC98TRIDENT9385Disable(pScrn);
	break;
    default: /* Run 96xx code as default */
	PC98TRIDENT96xxDisable(pScrn);
	break;
    }
}

/* Initialize VGA Block for Cyber9385 on PC-98x1 */
static void
PC98TRIDENT9385Init(ScrnInfoPtr pScrn)
{
/* Nothing to initialize */
}

static void
PC98TRIDENT9385Enable(ScrnInfoPtr pScrn)
{
    outb(0xFAC, 0x02);
}

static void
PC98TRIDENT9385Disable(ScrnInfoPtr pScrn)
{
    outb(0xFAC, 0x00);
}

/* Initialize VGA Block for Trident96xx on PC-98x1 */
static void
PC98TRIDENT96xxInit(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 temp = 0;

    vgaHWProtect(pScrn, TRUE);

    /* Video SusSystem Enable */
    temp = INB(0x3CC);
    OUTB(0x3C2, temp | 0xC3);
    /* Switch Old */
    OUTB(0x3C4, 0x0B); temp = INB(0x3C5);
    OUTW(0x3C4, 0x0B | (temp << 8));
    /* Select Configuration Port 1 */
    OUTB(0x3C4, 0x0E); temp = INB(0x3C5);
    OUTW(0x3C4, 0x0E | ((temp | 0x20) << 8));

    OUTB(0x3C4, 0x0c);
    if((INB(0x3C5) & 0x10) == 0x10)
    {
	OUTB(0x3C4, 0x0E | (temp << 8));
	OUTB(0x94,  0x00);
	OUTB(0x102, 0x01);
	OUTB(0x94,  0x20);
	temp = INB(0x3C3);
	OUTB(0x3C3, temp | 0x01);
    } else {
	OUTB(0x3C4, 0x0E | (temp << 8));
	OUTB(0x46E8, 0x10);
	OUTB(0x102,  0x01);
	OUTB(0x46E8, 0x08);
    }

    INB(0x3DA);
    OUTB(0x3C0,0x10);
    OUTB(0x3C0,0x41);
  
    /* Register Unlock */
    vgaHWUnlock(hwp);
    OUTB(0x3C4, 0x0B); temp = INB(0x3C5); /* Switch New */
    OUTB(0x3C4, 0x0E); temp = INB(0x3C5);
    OUTW(0x3C4, 0x0E | ((temp | 0x80) << 8));

    /* For Speed Up [Facoor 2 at Xengine] */
    OUTW(0x3D4, 0x3820); /* Command FIFO Register */
    OUTW(0x3D4, 0x2020); /* Command FIFO Register */
    /* Latency Control Registers 0x30 - 0x32 */
    /* Parameter Range 0x00 - 0x0F */
    /* Tune these parameter to avoid GE Timeout */
    OUTW(0x3D4, 0x0E30); /* Display Queue Latency Control */
    /* 8bpp GE No Timeout Parameter 0x0D - 0x0F for PC-9821Xa7 TGUi9680 */
    OUTW(0x3D4, 0x0031); /* Frame Buffer Latency Control */
    OUTW(0x3D4, 0x0032); /* Display & Frame Buffer Latency Control */
    OUTW(0x3D4, 0x213B); /* Clock and Tuning */

    /* MCLK Init */
    OUTB(0x43C6, 0xAF); OUTB(0x43C7, 0x00); /* 80.0MHz */
#if 0
    /* Sample MCLKs */
    OUTB(0x43C6, 0xAF); OUTB(0x43C7, 0x00); /* 80.0MHz */
    OUTB(0x43C6, 0xA7); OUTB(0x43C7, 0x00); /* 77.0MHz */
    OUTB(0x43C6, 0x8E); OUTB(0x43C7, 0x00); /* 75.0MHz */
    OUTB(0x43C6, 0x86); OUTB(0x43C7, 0x00); /* 72.0MHz */
    OUTB(0x43C6, 0x8F); OUTB(0x43C7, 0x00); /* 67.2MHz */
    OUTB(0x43C6, 0xD5); OUTB(0x43C7, 0x02); /* 61.6MHz */
#endif

    /* Register Lock */
    OUTB(0x3C4, 0x0B); temp = INB(0x3C5); /* Switch New */
    OUTB(0x3C4, 0x0E); temp = INB(0x3C5);
    OUTW(0x3C4, 0x0E | ((temp & 0x7F) << 8));
    vgaHWLock(hwp);

    vgaHWProtect(pScrn, FALSE);
}

static void
PC98TRIDENT96xxEnable(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD8 temp = 0;

    outb(0x68, 0x0E);
    outb(0x6A, 0x07);
    outb(0x6A, 0x8F);
    outb(0x6A, 0x06);

    vgaHWProtect(pScrn, TRUE);

    OUTB(0x3D4, 0x23); temp = INB(0x3D5);
    OUTW(0x3D4, 0x23 | ((temp & 0xDF) << 8));

    OUTB(0x3D4, 0x29); temp = INB(0x3D5);
    OUTW(0x3D4, 0x29 | ((temp | 0x04) << 8));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp | 0x06));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp | 0x08));

    OUTB(0x3CE, 0x23); temp = INB(0x3CF);
    OUTW(0x3CE, 0x23 | ((temp & 0xFC) << 8));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp | 0x01));

    OUTB(0x3C4, 0x01); temp = INB(0x3C5);
    OUTW(0x3C4, 0x01 | ((temp & 0xEF) << 8));

    vgaHWProtect(pScrn, FALSE);

    outb(0xFAC, 0x02);
}

static void
PC98TRIDENT96xxDisable(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD8 temp = 0;

    outb(0xFAC, 0x00);

    vgaHWProtect(pScrn, TRUE);

    OUTB(0x3C4, 0x01); temp = INB(0x3C5);
    OUTW(0x3C4, 0x01 | ((temp | 0x10) << 8));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp & 0xFE));

    OUTB(0x3CE, 0x23); temp = INB(0x3CF);
    OUTW(0x3CE, 0x23 | (((temp & 0xFC) | 0x01) << 8));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp & 0xFD));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp & 0xCF));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp & 0xF7));

    OUTB(0x83C8, 0x04); temp = INB(0x83c6);
    OUTB(0x83C8, 0x04); OUTB(0x83c6, (temp & 0xFB));

    OUTB(0x3D4, 0x29); temp = INB(0x3D5);
    OUTW(0x3D4, 0x29 | ((temp & 0xFB) << 8));

    OUTB(0x3D4, 0x23); temp = INB(0x3D5);
    OUTW(0x3D4, 0x23 | ((temp | 0x20) << 8));

    vgaHWProtect(pScrn, FALSE);

    outb(0x6A, 0x07);
    outb(0x6A, 0x8E);
    outb(0x6A, 0x06);
    outb(0x68, 0x0F);
}


/* 
 * This is a terrible hack! If we are on a notebook in a stretched
 * mode and don't want full screen we use the BIOS to set an unstreched
 * mode.
 */
void
tridentSetModeBIOS(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);


#ifdef VBE_INFO
    if (pTrident->vbeModes) {
	vbeSaveRestoreRec vbesr;
	vbesr.stateMode = VBECalcVbeModeIndex(pTrident->vbeModes,
					     mode, pScrn->bitsPerPixel);
	vbesr.pstate = NULL;
	if (vbesr.stateMode) {
	    if (IsPciCard && UseMMIO) 
		TRIDENTDisableMMIO(pScrn);
	    VBEVesaSaveRestore(pTrident->pVbe,&vbesr,MODE_RESTORE);
	    if (IsPciCard && UseMMIO) 
		TRIDENTEnableMMIO(pScrn);
	    return;
	} else
	    xf86DrvMsg(pScrn->scrnIndex,X_WARNING,"No BIOS Mode matches "
		       "%ix%I@%ibpp\n",mode->HDisplay,mode->VDisplay,
		       pScrn->bitsPerPixel);
    } 
#endif
    /* This function is only for LCD screens, and also when we have
     * int10 available */

    if (pTrident->IsCyber && pTrident->lcdMode && pTrident->Int10) {
        int i = pTrident->lcdMode;
	if ((pScrn->currentMode->HDisplay != LCD[i].display_x) /* !fullsize? */
	    || (pScrn->currentMode->VDisplay != LCD[i].display_y)) {
	    if (pTrident->lcdActive)  { /* LCD Active ?*/
	        int h_str, v_str;
		
		OUTB(0x3CE,HorStretch);  h_str = INB(0x3CF) & 0x01;
		OUTB(0x3CE,VertStretch); v_str = INB(0x3CF) & 0x01;
		if (h_str || v_str) {
		    OUTB(0x3C4, 0x11); OUTB(0x3C5, 0x92);
		    OUTW(0x3CE, BiosReg );
		    pTrident->Int10->ax = 0x3;
		    pTrident->Int10->num = 0x10;
		    if (IsPciCard && UseMMIO) 
			TRIDENTDisableMMIO(pScrn);
		    xf86ExecX86int10(pTrident->Int10);
		    if (IsPciCard && UseMMIO) 
			TRIDENTEnableMMIO(pScrn);
		}
	    }
	}
    }
}

