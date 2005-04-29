/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_driver.c,v 1.49 2003/11/06 18:38:06 tsi Exp $ */
/*
 * vim: sw=4 ts=8 ai ic:
 *
 *	XFree86 4.0 S3 Savage driver
 *
 *	Tim Roberts <timr@probo.com>
 *	Ani Joshi <ajoshi@unixbox.com>
 *
 *	TODO:  add credits for the 3.3.x authors...
 *
 */


#include "xf86RAC.h"
#include "shadowfb.h"

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

#include "xf86xv.h"

#include "savage_driver.h"
#include "savage_bci.h"


/*
 * prototypes
 */
static void SavageEnableMMIO(ScrnInfoPtr pScrn);
static void SavageDisableMMIO(ScrnInfoPtr pScrn);

static const OptionInfoRec * SavageAvailableOptions(int chipid, int busid);
static void SavageIdentify(int flags);
static Bool SavageProbe(DriverPtr drv, int flags);
static Bool SavagePreInit(ScrnInfoPtr pScrn, int flags);

static Bool SavageEnterVT(int scrnIndex, int flags);
static void SavageLeaveVT(int scrnIndex, int flags);
static void SavageSave(ScrnInfoPtr pScrn);
static void SavageWriteMode(ScrnInfoPtr pScrn, vgaRegPtr, SavageRegPtr, Bool);

static Bool SavageScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
			     char **argv);
static int SavageInternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static ModeStatus SavageValidMode(int index, DisplayModePtr mode,
				  Bool verbose, int flags);

void SavageDGAInit(ScreenPtr);
static Bool SavageMapMMIO(ScrnInfoPtr pScrn);
static Bool SavageMapFB(ScrnInfoPtr pScrn);
static void SavageUnmapMem(ScrnInfoPtr pScrn, int All);
static Bool SavageModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool SavageCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool SavageSaveScreen(ScreenPtr pScreen, int mode);
static void SavageLoadPalette(ScrnInfoPtr pScrn, int numColors,
			      int *indicies, LOCO *colors,
			      VisualPtr pVisual);
static void SavageLoadPaletteSavage4(ScrnInfoPtr pScrn, int numColors,
			      int *indicies, LOCO *colors,
			      VisualPtr pVisual);
static void SavageCalcClock(long freq, int min_m, int min_n1, int max_n1,
			   int min_n2, int max_n2, long freq_min,
			   long freq_max, unsigned int *mdiv,
			   unsigned int *ndiv, unsigned int *r);
void SavageGEReset(ScrnInfoPtr pScrn, int from_timeout, int line, char *file);
void SavagePrintRegs(ScrnInfoPtr pScrn);
static void SavageDPMS(ScrnInfoPtr pScrn, int mode, int flags);
static Bool SavageDDC1(int scrnIndex);
static unsigned int SavageDDC1Read(ScrnInfoPtr pScrn);
static void SavageProbeDDC(ScrnInfoPtr pScrn, int index);
static void SavageGetTvMaxSize(SavagePtr psav);
static Bool SavagePanningCheck(ScrnInfoPtr pScrn);

extern ScrnInfoPtr gpScrn;

#define iabs(a)	((int)(a)>0?(a):(-(a)))

#define DRIVER_NAME	"savage"
#define DRIVER_VERSION	"1.1.27"
#define VERSION_MAJOR	1
#define VERSION_MINOR	1
#define PATCHLEVEL	27
#define SAVAGE_VERSION	((VERSION_MAJOR << 24) | \
			 (VERSION_MINOR << 16) | \
			 PATCHLEVEL)


/*#define TRACEON*/
#ifdef TRACEON
#define TRACE(prms)	ErrorF prms
#else
#define TRACE(prms)  
#endif

DriverRec SAVAGE =
{
    SAVAGE_VERSION,
    DRIVER_NAME,
    SavageIdentify,
    SavageProbe,
    SavageAvailableOptions,
    NULL,
    0
};


/* Supported chipsets */

static SymTabRec SavageChips[] = {
    { PCI_CHIP_SAVAGE4,		"Savage4" },
    { PCI_CHIP_SAVAGE3D,	"Savage3D" },
    { PCI_CHIP_SAVAGE3D_MV,	"Savage3D-MV" },
    { PCI_CHIP_SAVAGE2000,	"Savage2000" },
    { PCI_CHIP_SAVAGE_MX_MV,	"Savage/MX-MV" },
    { PCI_CHIP_SAVAGE_MX,	"Savage/MX" },
    { PCI_CHIP_SAVAGE_IX_MV,	"Savage/IX-MV" },
    { PCI_CHIP_SAVAGE_IX,	"Savage/IX" },
    { PCI_CHIP_PROSAVAGE_PM,	"ProSavage PM133" },
    { PCI_CHIP_PROSAVAGE_KM,	"ProSavage KM133" },
    { PCI_CHIP_S3TWISTER_P,	"ProSavage PN133" },
    { PCI_CHIP_S3TWISTER_K,	"ProSavage KN133" },
    { PCI_CHIP_SUPSAV_MX128,	"SuperSavage/MX 128" },
    { PCI_CHIP_SUPSAV_MX64,	"SuperSavage/MX 64" },
    { PCI_CHIP_SUPSAV_MX64C,	"SuperSavage/MX 64C" },
    { PCI_CHIP_SUPSAV_IX128SDR,	"SuperSavage/IX 128" },
    { PCI_CHIP_SUPSAV_IX128DDR,	"SuperSavage/IX 128" },
    { PCI_CHIP_SUPSAV_IX64SDR,	"SuperSavage/IX 64" },
    { PCI_CHIP_SUPSAV_IX64DDR,	"SuperSavage/IX 64" },
    { PCI_CHIP_SUPSAV_IXCSDR,	"SuperSavage/IXC 64" },
    { PCI_CHIP_SUPSAV_IXCDDR,	"SuperSavage/IXC 64" },
    { PCI_CHIP_PROSAVAGE_DDR,	"ProSavage DDR" },
    { PCI_CHIP_PROSAVAGE_DDRK,	"ProSavage DDR-K" },
    { -1,			NULL }
};

static SymTabRec SavageChipsets[] = {
    { S3_SAVAGE3D,	"Savage3D" },
    { S3_SAVAGE4,	"Savage4" },
    { S3_SAVAGE2000,	"Savage2000" },
    { S3_SAVAGE_MX,	"MobileSavage" },
    { S3_PROSAVAGE,	"ProSavage" },
    { S3_SUPERSAVAGE,   "SuperSavage" },
    { -1,		NULL }
};

/* This table maps a PCI device ID to a chipset family identifier. */

static PciChipsets SavagePciChipsets[] = {
    { S3_SAVAGE3D,	PCI_CHIP_SAVAGE3D,	RES_SHARED_VGA },
    { S3_SAVAGE3D,	PCI_CHIP_SAVAGE3D_MV, 	RES_SHARED_VGA },
    { S3_SAVAGE4,	PCI_CHIP_SAVAGE4,	RES_SHARED_VGA },
    { S3_SAVAGE2000,	PCI_CHIP_SAVAGE2000,	RES_SHARED_VGA },
    { S3_SAVAGE_MX,	PCI_CHIP_SAVAGE_MX_MV,	RES_SHARED_VGA },
    { S3_SAVAGE_MX,	PCI_CHIP_SAVAGE_MX,	RES_SHARED_VGA },
    { S3_SAVAGE_MX,	PCI_CHIP_SAVAGE_IX_MV,	RES_SHARED_VGA },
    { S3_SAVAGE_MX,	PCI_CHIP_SAVAGE_IX,	RES_SHARED_VGA },
    { S3_PROSAVAGE,	PCI_CHIP_PROSAVAGE_PM,	RES_SHARED_VGA },
    { S3_PROSAVAGE,	PCI_CHIP_PROSAVAGE_KM,	RES_SHARED_VGA },
    { S3_PROSAVAGE,	PCI_CHIP_S3TWISTER_P,	RES_SHARED_VGA },
    { S3_PROSAVAGE,	PCI_CHIP_S3TWISTER_K,	RES_SHARED_VGA },
    { S3_PROSAVAGE,	PCI_CHIP_PROSAVAGE_DDR,	RES_SHARED_VGA },
    { S3_PROSAVAGE,	PCI_CHIP_PROSAVAGE_DDRK,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_MX128,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_MX64,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_MX64C,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_IX128SDR,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_IX128DDR,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_IX64SDR,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_IX64DDR,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_IXCSDR,	RES_SHARED_VGA },
    { S3_SUPERSAVAGE,	PCI_CHIP_SUPSAV_IXCDDR,	RES_SHARED_VGA },
    { -1,		-1,			RES_UNDEFINED }
};

typedef enum {
     OPTION_PCI_BURST
    ,OPTION_PCI_RETRY
    ,OPTION_NOACCEL
    ,OPTION_LCD_CENTER
    ,OPTION_LCDCLOCK
    ,OPTION_MCLK
    ,OPTION_REFCLK
    ,OPTION_SHOWCACHE
    ,OPTION_SWCURSOR
    ,OPTION_HWCURSOR
    ,OPTION_SHADOW_FB
    ,OPTION_ROTATE
    ,OPTION_USEBIOS
    ,OPTION_SHADOW_STATUS
    ,OPTION_CRT_ONLY
    ,OPTION_TV_ON
    ,OPTION_TV_PAL
    ,OPTION_FORCE_INIT
} SavageOpts;


static const OptionInfoRec SavageOptions[] =
{
    { OPTION_NOACCEL,	"NoAccel",	OPTV_BOOLEAN, {0}, FALSE  },
    { OPTION_HWCURSOR,	"HWCursor",	OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SWCURSOR,	"SWCursor",	OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SHADOW_FB,	"ShadowFB",	OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_ROTATE,	"Rotate",	OPTV_ANYSTR, {0}, FALSE },
    { OPTION_USEBIOS,	"UseBIOS",	OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_LCDCLOCK,	"LCDClock",	OPTV_FREQ,    {0}, FALSE },
    { OPTION_SHADOW_STATUS, "ShadowStatus", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_CRT_ONLY,  "CrtOnly",      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_TV_ON,     "TvOn",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_TV_PAL,    "PAL",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_FORCE_INIT,"ForceInit",    OPTV_BOOLEAN, {0}, FALSE },
    { -1,		NULL,		OPTV_NONE,    {0}, FALSE }
};


static const char *vgaHWSymbols[] = {
    "vgaHWBlankScreen",
    "vgaHWCopyReg",
    "vgaHWGetHWRec",
    "vgaHWGetIOBase",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWLock",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    "vgaHWSetMmioFuncs",
    "vgaHWSetStdFuncs",
    "vgaHWUnmapMem",
    "vgaHWddc1SetSpeed",
#if 0
    "vgaHWFreeHWRec",
    "vgaHWMapMem",
    "vgaHWUnlock",
#endif
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
#if 0
    "xf86DestroyCursorInfoRec",
#endif
    "xf86InitCursor",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
#if 0
    "vbeFree",
#endif
    NULL
};

#ifdef XFree86LOADER
static const char *vbeOptSymbols[] = {
    "vbeModeInit",
    "VBESetVBEMode",
    "VBEGetVBEInfo",
    "VBEFreeVBEInfo",
    NULL
};
#endif

static const char *ddcSymbols[] = {
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    "xf86PrintEDID",
    "xf86SetDDCproperties",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACopyROP_PM",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAHelpPatternROP",
    "XAAHelpSolidROP",
    "XAAInit",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

static const char *int10Symbols[] = {
    "xf86ExecX86int10",
#if 0
    "xf86FreeInt10",
#endif
    "xf86InitInt10",
    "xf86Int10AllocPages",
    "xf86Int10FreePages",
    "xf86int10Addr",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(SavageSetup);

static XF86ModuleVersionInfo SavageVersRec = {
    "savage",
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

XF86ModuleData savageModuleData = { &SavageVersRec, SavageSetup, NULL };

static pointer SavageSetup(pointer module, pointer opts, int *errmaj,
			   int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&SAVAGE, module, 0);
	LoaderRefSymLists(vgaHWSymbols, fbSymbols, ramdacSymbols, 
			  xaaSymbols, shadowSymbols, vbeSymbols, vbeOptSymbols,
			  int10Symbols, i2cSymbols, ddcSymbols, NULL);
	return (pointer) 1;
    } else {
	if (errmaj)
	    *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

#endif /* XFree86LOADER */


/*
 * I'd rather have these wait macros be inline, but S3 has made it 
 * darned near impossible.  The bit fields are in a different place in
 * all three families, the status register has a different address in the
 * three families, and even the idle vs busy sense flipped in the Sav2K.
 */

static void
ResetBCI2K( SavagePtr psav )
{
    CARD32 cob = INREG( 0x48c18 );
    /* if BCI is enabled and BCI is busy... */

    if( 
	(cob & 0x00000008) &&
	! (ALT_STATUS_WORD0 & 0x00200000)
    )
    {
	ErrorF( "Resetting BCI, stat = %08lx...\n",
		(unsigned long) ALT_STATUS_WORD0);
	/* Turn off BCI */
	OUTREG( 0x48c18, cob & ~8 );
	usleep(10000);
	/* Turn it back on */
	OUTREG( 0x48c18, cob );
	usleep(10000);
    }
}

static Bool
ShadowWait( SavagePtr psav )
{
    BCI_GET_PTR;
    int loop = 0;

    if( !psav->NoPCIRetry )
	return 0;

    psav->ShadowCounter = (psav->ShadowCounter + 1) & 0x7fff;
    BCI_SEND( psav->dwBCIWait2DIdle );
    BCI_SEND( 0x98000000 + psav->ShadowCounter );

    while(
	(psav->ShadowVirtual[1] & 0x7fff) != psav->ShadowCounter  &&
	(loop++ < MAXLOOP)
    )
	;

    return loop >= MAXLOOP;
}

static Bool
ShadowWait1( SavagePtr psav, int v )
{
    return ShadowWait( psav );
}


/* Wait until "v" queue entries are free */

static int
WaitQueue3D( SavagePtr psav, int v )
{
    int loop = 0;
    int slots = MAXFIFO - v;

    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitQueue = ShadowWait1;
	return ShadowWait(psav);
    }
    else
    {
	loop &= STATUS_WORD0;
	while( ((STATUS_WORD0 & 0x0000ffff) > slots) && (loop++ < MAXLOOP))
	    ;
    }
    return loop >= MAXLOOP;
}

static int
WaitQueue4( SavagePtr psav, int v )
{
    int loop = 0;
    int slots = MAXFIFO - v;

    if( !psav->NoPCIRetry )
	return 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitQueue = ShadowWait1;
	return ShadowWait(psav);
    }
    else
	while( ((ALT_STATUS_WORD0 & 0x001fffff) > slots) && (loop++ < MAXLOOP))
	    ;
    return loop >= MAXLOOP;
}

static int
WaitQueue2K( SavagePtr psav, int v )
{
    int loop = 0;
    int slots = MAXFIFO - v;

    if( !psav->NoPCIRetry )
	return 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitQueue = ShadowWait1;
	return ShadowWait(psav);
    }
    else
	while( ((ALT_STATUS_WORD0 & 0x000fffff) > slots) && (loop++ < MAXLOOP))
	    ;
    if( loop >= MAXLOOP )
	ResetBCI2K(psav);
    return loop >= MAXLOOP;
}

/* Wait until GP is idle and queue is empty */

static int
WaitIdleEmpty3D(SavagePtr psav)
{
    int loop = 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitIdleEmpty = ShadowWait;
	return ShadowWait(psav);
    }
    loop &= STATUS_WORD0;
    while( ((STATUS_WORD0 & 0x0008ffff) != 0x80000) && (loop++ < MAXLOOP) )
	;
    return loop >= MAXLOOP;
}

static int
WaitIdleEmpty4(SavagePtr psav)
{
    int loop = 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitIdleEmpty = ShadowWait;
	return ShadowWait(psav);
    }
    while( ((ALT_STATUS_WORD0 & 0x00a1ffff) != 0x00a00000) && (loop++ < MAXLOOP) )
	;
    return loop >= MAXLOOP;
}

static int
WaitIdleEmpty2K(SavagePtr psav)
{
    int loop = 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitIdleEmpty = ShadowWait;
	return ShadowWait(psav);
    }
    loop &= ALT_STATUS_WORD0;
    while( ((ALT_STATUS_WORD0 & 0x009fffff) != 0) && (loop++ < MAXLOOP) )
	;
    if( loop >= MAXLOOP )
	ResetBCI2K(psav);
    return loop >= MAXLOOP;
}

/* Wait until GP is idle */

static int
WaitIdle3D(SavagePtr psav)
{
    int loop = 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitIdle = ShadowWait;
	return ShadowWait(psav);
    }
    while( (!(STATUS_WORD0 & 0x00080000)) && (loop++ < MAXLOOP) )
	;
    return loop >= MAXLOOP;
}

static int
WaitIdle4(SavagePtr psav)
{
    int loop = 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitIdle = ShadowWait;
	return ShadowWait(psav);
    }
    while( (!(ALT_STATUS_WORD0 & 0x00800000)) && (loop++ < MAXLOOP) )
	;
    return loop >= MAXLOOP;
}

static int
WaitIdle2K(SavagePtr psav)
{
    int loop = 0;
    mem_barrier();
    if( psav->ShadowVirtual )
    {
	psav->WaitIdle = ShadowWait;
	return ShadowWait(psav);
    }
    loop &= ALT_STATUS_WORD0;
    while( (ALT_STATUS_WORD0 & 0x00900000) && (loop++ < MAXLOOP) )
	;
    return loop >= MAXLOOP;
}


static Bool SavageGetRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(SavageRec), 1);
    return TRUE;
}


static void SavageFreeRec(ScrnInfoPtr pScrn)
{
    TRACE(( "SavageFreeRec(%x)\n", pScrn->driverPrivate ));
    if (!pScrn->driverPrivate)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
    SavageUnmapMem(pScrn, 1);
}


static const OptionInfoRec * SavageAvailableOptions(int chipid, int busid)
{
    return SavageOptions;
}


static void SavageIdentify(int flags)
{
    xf86PrintChipsets("SAVAGE", 
		      "driver (version " DRIVER_VERSION ") for S3 Savage chipsets",
		      SavageChips);
}


static Bool SavageProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections = NULL;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;

    /* sanity checks */
    if ((numDevSections = xf86MatchDevice("savage", &devSections)) <= 0)
        return FALSE;
    if (xf86GetPciVideoInfo() == NULL) {
        if (devSections)
	    xfree(devSections);
        return FALSE;
    }

    numUsed = xf86MatchPciInstances("SAVAGE", PCI_VENDOR_S3,
				    SavageChipsets, SavagePciChipsets,
				    devSections, numDevSections, drv,
				    &usedChips);
    if (devSections)
	xfree(devSections);
    devSections = NULL;
    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else
	for (i=0; i<numUsed; i++) {
	    ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);

	    pScrn->driverVersion = SAVAGE_VERSION;
	    pScrn->driverName = DRIVER_NAME;
	    pScrn->name = "SAVAGE";
	    pScrn->Probe = SavageProbe;
	    pScrn->PreInit = SavagePreInit;
	    pScrn->ScreenInit = SavageScreenInit;
	    pScrn->SwitchMode = SavageSwitchMode;
	    pScrn->AdjustFrame = SavageAdjustFrame;
	    pScrn->EnterVT = SavageEnterVT;
	    pScrn->LeaveVT = SavageLeaveVT;
	    pScrn->FreeScreen = NULL;
	    pScrn->ValidMode = SavageValidMode;
	    foundScreen = TRUE;
	    xf86ConfigActivePciEntity(pScrn, usedChips[i], SavagePciChipsets,
				     NULL, NULL, NULL, NULL, NULL);
	}

    xfree(usedChips);
    return foundScreen;
}

static int LookupChipID( PciChipsets* pset, int ChipID )
{
    /* Is there a function to do this for me? */
    while( pset->numChipset >= 0 )
    {
        if( pset->PCIid == ChipID )
	    return pset->numChipset;
	pset++;
    }

    return -1;
}

static Bool SavagePreInit(ScrnInfoPtr pScrn, int flags)
{
    EntityInfoPtr pEnt;
    SavagePtr psav;
    MessageType from = X_DEFAULT;
    int i;
    ClockRangePtr clockRanges;
    char *s = NULL;
    unsigned char config1, m, n, n1, n2, sr8, cr66 = 0, tmp;
    int mclk;
    vgaHWPtr hwp;
    int vgaCRIndex, vgaCRReg;
    pointer ddc;

    TRACE(("SavagePreInit(%d)\n", flags));

    gpScrn = pScrn;

    if (flags & PROBE_DETECT) {
	SavageProbeDDC( pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index );
	return TRUE;
    }

    if (!xf86LoadSubModule(pScrn, "vgahw"))
	return FALSE;

    xf86LoaderReqSymLists(vgaHWSymbols, NULL);
    if (!vgaHWGetHWRec(pScrn))
	return FALSE;

#if 0
    /* Here we can alter the number of registers saved and restored by the
     * standard vgaHWSave and Restore routines.
     */
    vgaHWSetRegCounts( pScrn, VGA_NUM_CRTC, VGA_NUM_SEQ, VGA_NUM_GFX, VGA_NUM_ATTR );
#endif

    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * We support depths of 8, 15, 16 and 24.
     * We support bpp of 8, 16, and 32.
     */

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb))
	return FALSE;
    else {
        int requiredBpp;
	int altBpp = 0;

	switch (pScrn->depth) {
	case 8:
	case 16:
	    requiredBpp = pScrn->depth;
	    break;
	case 15:
	    requiredBpp = 16;
	    break;
	case 24:
	    requiredBpp = 32;
	    altBpp = 24;
	    break;

	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
			pScrn->depth);
	    return FALSE;
	}

	if( 
	    (pScrn->bitsPerPixel != requiredBpp) &&
	    (pScrn->bitsPerPixel != altBpp) 
	) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Depth %d must specify %d bpp; %d was given\n",
		       pScrn->depth, requiredBpp, pScrn->bitsPerPixel );
	    return FALSE;
	}
    }

    xf86PrintDepthBpp(pScrn);

    if (pScrn->depth > 8) {
	rgb zeros = {0, 0, 0};

	if (!xf86SetWeight(pScrn, zeros, zeros))
	    return FALSE;
	else {
	    /* TODO check weight returned is supported */
	    ;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	return FALSE;
    } else {
	/* We don't currently support DirectColor at 16bpp */
	if (pScrn->bitsPerPixel == 16 && pScrn->defaultVisual != TrueColor) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported at depth %d\n",
		       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	    return FALSE;
	}
    }

    pScrn->progClock = TRUE;

    if (!SavageGetRec(pScrn))
	return FALSE;
    psav = SAVPTR(pScrn);

    hwp = VGAHWPTR(pScrn);
    vgaHWGetIOBase(hwp);
    psav->vgaIOBase = hwp->IOBase;

    xf86CollectOptions(pScrn, NULL);

    if (pScrn->depth == 8)
	pScrn->rgbBits = 8/*6*/;

    if (!(psav->Options = xalloc(sizeof(SavageOptions))))
	return FALSE;
    memcpy(psav->Options, SavageOptions, sizeof(SavageOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, psav->Options);

    xf86GetOptValBool(psav->Options, OPTION_PCI_BURST, &psav->pci_burst);

    if (psav->pci_burst) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "Option: pci_burst - PCI burst read enabled\n");
    }

    psav->NoPCIRetry = 1;		/* default */
    if (xf86ReturnOptValBool(psav->Options, OPTION_PCI_RETRY, FALSE)) {
	if (xf86ReturnOptValBool(psav->Options, OPTION_PCI_BURST, FALSE)) {
	    psav->NoPCIRetry = 0;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_retry\n");
	} else
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"pci_retry\" option requires \"pci_burst\"\n");
    }

    xf86GetOptValBool( psav->Options, OPTION_SHADOW_FB, &psav->shadowFB );
    if (psav->shadowFB) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: shadow FB enabled\n");
    }

    if ((s = xf86GetOptValString(psav->Options, OPTION_ROTATE))) {
	if(!xf86NameCmp(s, "CW")) {
	    /* accel is disabled below for shadowFB */
	    psav->shadowFB = TRUE;
	    psav->rotate = 1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		       "Rotating screen clockwise - acceleration disabled\n");
	} else if(!xf86NameCmp(s, "CCW")) {
	    psav->shadowFB = TRUE;
	    psav->rotate = -1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
		       "counter clockwise - acceleration disabled\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
		       "value for Option \"Rotate\"\n", s);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		       "Valid options are \"CW\" or \"CCW\"\n");
	}
    }

    if (xf86GetOptValBool(psav->Options, OPTION_NOACCEL, &psav->NoAccel))
	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
		    "Option: NoAccel - Acceleration Disabled\n");

    if (psav->shadowFB && !psav->NoAccel) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "HW acceleration not supported with \"shadowFB\".\n");
	psav->NoAccel = TRUE;
    }

    if (pScrn->bitsPerPixel == 24 && !psav->NoAccel) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "HW acceleration not possible with depth 32 and bpp 24.\n");
	psav->NoAccel = TRUE;
    }


    /*
     * The SWCursor setting takes priority over HWCursor.  The default
     * if neither is specified is HW, unless ShadowFB is specified,
     * then SW.
     */

    from = X_DEFAULT;
    psav->hwcursor = psav->shadowFB ? FALSE : TRUE;
    if (xf86GetOptValBool(psav->Options, OPTION_HWCURSOR, &psav->hwcursor))
	from = X_CONFIG;
    if (xf86ReturnOptValBool(psav->Options, OPTION_SWCURSOR, FALSE)) {
	psav->hwcursor = FALSE;
	from = X_CONFIG;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
        psav->hwcursor ? "HW" : "SW");

    from = X_DEFAULT;
    psav->UseBIOS = TRUE;
    if (xf86GetOptValBool(psav->Options, OPTION_USEBIOS, &psav->UseBIOS) )
	from = X_CONFIG;
    xf86DrvMsg(pScrn->scrnIndex, from, "%ssing video BIOS to set modes\n",
        psav->UseBIOS ? "U" : "Not u" );

    psav->LCDClock = 0.0;
    if( xf86GetOptValFreq( psav->Options, OPTION_LCDCLOCK, OPTUNITS_MHZ, &psav->LCDClock ) )
	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG, 
		    "Option: LCDClock %1.2f MHz\n", psav->LCDClock );

    if( xf86GetOptValBool( psav->Options, OPTION_SHADOW_STATUS, &psav->ShadowStatus))
	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
		    "Option: ShadowStatus enabled\n" );


    if( xf86GetOptValBool( psav->Options, OPTION_CRT_ONLY, &psav->CrtOnly))
	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
		    "Option: CrtOnly enabled\n" );

    if( xf86GetOptValBool( psav->Options, OPTION_TV_ON, &psav->TvOn)) {
        psav->PAL = FALSE;
        SavageGetTvMaxSize(psav);
    }

    if( xf86GetOptValBool( psav->Options, OPTION_TV_PAL, &psav->PAL)) {
        SavageGetTvMaxSize(psav);
	psav->TvOn = TRUE;
    }

    if( psav->TvOn )
	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
		    "TV enabled in %s format\n",
		    psav->PAL ? "PAL" : "NTSC" );

    psav->ForceInit = 0;
    if( xf86GetOptValBool( psav->Options, OPTION_FORCE_INIT, &psav->ForceInit))
	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
		    "Option: ForceInit enabled\n" );

    /* Add more options here. */

    if (pScrn->numEntities > 1) {
	SavageFreeRec(pScrn);
	return FALSE;
    }

    pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pEnt->resources) {
	xfree(pEnt);
	SavageFreeRec(pScrn);
	return FALSE;
    }
    psav->EntityIndex = pEnt->index;

    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86LoaderReqSymLists(int10Symbols, NULL);
	psav->pInt10 = xf86InitInt10(pEnt->index);
    }

    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86LoaderReqSymLists(vbeSymbols, NULL);
	psav->pVbe = VBEInit(psav->pInt10, pEnt->index);
    }


    psav->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
    xf86RegisterResources(pEnt->index, NULL, ResNone);
    xf86SetOperatingState(resVgaIo, pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMem, pEnt->index, ResDisableOpr);

    from = X_DEFAULT;
    if (pEnt->device->chipset && *pEnt->device->chipset) {
	pScrn->chipset = pEnt->device->chipset;
	psav->ChipId = pEnt->device->chipID;
	psav->Chipset = xf86StringToToken(SavageChipsets, pScrn->chipset);
	from = X_CONFIG;
    } else if (pEnt->device->chipID >= 0) {
	psav->ChipId = pEnt->device->chipID;
	psav->Chipset = LookupChipID(SavagePciChipsets, psav->ChipId);
	pScrn->chipset = (char *)xf86TokenToString(SavageChipsets,
						   psav->Chipset);
	from = X_CONFIG;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
		   pEnt->device->chipID);
    } else {
	from = X_PROBED;
	psav->ChipId = psav->PciInfo->chipType;
	psav->Chipset = LookupChipID(SavagePciChipsets, psav->ChipId);
	pScrn->chipset = (char *)xf86TokenToString(SavageChipsets,
						   psav->Chipset);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Chip: id %04x, \"%s\"\n",
	       psav->ChipId, xf86TokenToString( SavageChips, psav->ChipId ) );

    if (pEnt->device->chipRev >= 0) {
	psav->ChipRev = pEnt->device->chipRev;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
		   psav->ChipRev);
    } else
	psav->ChipRev = psav->PciInfo->chipRev;

    if (pEnt->device->videoRam != 0)
    	pScrn->videoRam = pEnt->device->videoRam;

    xfree(pEnt);

    /* maybe throw in some more sanity checks here */

    xf86DrvMsg(pScrn->scrnIndex, from, "Engine: \"%s\"\n", pScrn->chipset);

    psav->PciTag = pciTag(psav->PciInfo->bus, psav->PciInfo->device,
			  psav->PciInfo->func);


    if (!SavageMapMMIO(pScrn)) {
	SavageFreeRec(pScrn);
        vbeFree(psav->pVbe);
	return FALSE;
    }

    vgaCRIndex = psav->vgaIOBase + 4;
    vgaCRReg = psav->vgaIOBase + 5;

    xf86EnableIO();
    /* unprotect CRTC[0-7] */
    VGAOUT8(vgaCRIndex, 0x11);
    tmp = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, tmp & 0x7f);

    /* unlock extended regs */
    VGAOUT16(vgaCRIndex, 0x4838);
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

    VGAOUT8(vgaCRIndex, 0x40);
    tmp = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, tmp & ~0x01);

    /* unlock sys regs */
    VGAOUT8(vgaCRIndex, 0x38);
    VGAOUT8(vgaCRReg, 0x48);

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    vbeFree(psav->pVbe);
	    SavageFreeRec(pScrn);
	    return FALSE;
	}
    }

    /* Unlock system registers. */
    VGAOUT16(vgaCRIndex, 0x4838);

    /* Next go on to detect amount of installed ram */

    VGAOUT8(vgaCRIndex, 0x36);            /* for register CR36 (CONFG_REG1), */
    config1 = VGAIN8(vgaCRReg);           /* get amount of vram installed */

    /* Compute the amount of video memory and offscreen memory. */

    if (!pScrn->videoRam) {
	static unsigned char RamSavage3D[] = { 8, 4, 4, 2 };
	static unsigned char RamSavage4[] =  { 2, 4, 8, 12, 16, 32, 64, 32 };
	static unsigned char RamSavageMX[] = { 2, 8, 4, 16, 8, 16, 4, 16 };
	static unsigned char RamSavageNB[] = { 0, 2, 4, 8, 16, 32, 16, 2 };

	switch( psav->Chipset ) {
	case S3_SAVAGE3D:
	    pScrn->videoRam = RamSavage3D[ (config1 & 0xC0) >> 6 ] * 1024;
	    break;

	case S3_SAVAGE4:
	    /* 
	     * The Savage4 has one ugly special case to consider.  On
	     * systems with 4 banks of 2Mx32 SDRAM, the BIOS says 4MB
	     * when it really means 8MB.  Why do it the same when you
	     * can do it different...
	     */
	    VGAOUT8(vgaCRIndex, 0x68);	/* memory control 1 */
	    if( (VGAIN8(vgaCRReg) & 0xC0) == (0x01 << 6) )
		RamSavage4[1] = 8;

	    /*FALLTHROUGH*/

	case S3_SAVAGE2000:
	    pScrn->videoRam = RamSavage4[ (config1 & 0xE0) >> 5 ] * 1024;
	    break;

	case S3_SAVAGE_MX:
	case S3_SUPERSAVAGE:
	    pScrn->videoRam = RamSavageMX[ (config1 & 0x0E) >> 1 ] * 1024;
	    break;

	case S3_PROSAVAGE:
	    pScrn->videoRam = RamSavageNB[ (config1 & 0xE0) >> 5 ] * 1024;
	    break;

	default:
	    /* How did we get here? */
	    pScrn->videoRam = 0;
	    break;
	}

	psav->videoRambytes = pScrn->videoRam * 1024;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, 
		"probed videoram:  %dk\n",
		pScrn->videoRam);
    } else {
	psav->videoRambytes = pScrn->videoRam * 1024;

	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	       "videoram =  %dk\n",
		pScrn->videoRam);
    }

    if( !pScrn->videoRam && psav->pVbe )
    {
        /* If VBE is available, ask it about onboard memory. */

	VbeInfoBlock* vib;

	vib = VBEGetVBEInfo( psav->pVbe );
	pScrn->videoRam = vib->TotalMemory * 64;
	VBEFreeVBEInfo( vib );

	/* VBE often cuts 64k off of the RAM total. */

	if( pScrn->videoRam & 64 )
	    pScrn->videoRam += 64;

	psav->videoRambytes = pScrn->videoRam * 1024;
    }

    /*
     * If we're running with acceleration, compute the command overflow 
     * buffer location.  The command overflow buffer must END at a
     * 4MB boundary; for all practical purposes, that means the very
     * end of the frame buffer.
     */

    if( psav->NoAccel ) {
	psav->CursorKByte = pScrn->videoRam - 4;
	psav->cobIndex = 0;
	psav->cobSize = 0;
	psav->cobOffset = psav->videoRambytes;
    }
    else if( (S3_SAVAGE4_SERIES(psav->Chipset)) ||
             (S3_SUPERSAVAGE == psav->Chipset) ) {
	/*
	 * The Savage4 and ProSavage have COB coherency bugs which render 
	 * the buffer useless.  COB seems to make the SuperSavage slower.
         * We disable it.
	 */
	psav->CursorKByte = pScrn->videoRam - 4;
	psav->cobIndex = 2;
	psav->cobSize = 0x8000 << psav->cobIndex;
	psav->cobOffset = psav->videoRambytes;
    }
    else
    {
	/* We use 128kB for the COB on all other chips. */

	psav->cobSize = 1 << 17;
	if (psav->Chipset == S3_SUPERSAVAGE) {
	    psav->cobIndex = 2;
	}
	else {
	    psav->cobIndex = 7;
	}
	psav->cobOffset = psav->videoRambytes - psav->cobSize;
    }

    /* 
     * We place the cursor in high memory, just before the command overflow
     * buffer.  The cursor must be aligned on a 4k boundary.
     */

    psav->CursorKByte = (psav->cobOffset >> 10)  - 4;

    /* reset graphics engine to avoid memory corruption */
    VGAOUT8(vgaCRIndex, 0x66);
    cr66 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr66 | 0x02);
    usleep(10000);

    VGAOUT8(vgaCRIndex, 0x66);
    VGAOUT8(vgaCRReg, cr66 & ~0x02);	/* clear reset flag */
    usleep(10000);

    /* Set status word positions based on chip type. */

    switch( psav->Chipset ) {
	case S3_SAVAGE3D:
	case S3_SAVAGE_MX:
	    psav->WaitQueue	= WaitQueue3D;
	    psav->WaitIdle	= WaitIdle3D;
	    psav->WaitIdleEmpty	= WaitIdleEmpty3D;
	    break;

	case S3_SAVAGE4:
	case S3_PROSAVAGE:
	case S3_SUPERSAVAGE:
	    psav->WaitQueue	= WaitQueue4;
	    psav->WaitIdle	= WaitIdle4;
	    psav->WaitIdleEmpty	= WaitIdleEmpty4;
	    break;

	case S3_SAVAGE2000:
	    psav->WaitQueue	= WaitQueue2K;
	    psav->WaitIdle	= WaitIdle2K;
	    psav->WaitIdleEmpty	= WaitIdleEmpty2K;
	    break;
    }

    /* Do the DDC dance. */

    if( psav->Chipset != S3_PROSAVAGE ) {
	ddc = xf86LoadSubModule(pScrn, "ddc");
	if (ddc) {
#if 0
	    xf86MonPtr pMon = NULL;
#endif
	   
	    xf86LoaderReqSymLists(ddcSymbols, NULL);
#if 0
/*
 * On many machines, the attempt to read DDC information via VBE puts the
 * BIOS access into a state which prevents me from reading mode information.
 * This is a complete mystery to me.
 */
	    if ((psav->pVbe) 
	       && ((pMon = xf86PrintEDID(vbeDoEDID(psav->pVbe, ddc))) != NULL))
	       xf86SetDDCproperties(pScrn,pMon);
	    else 
#endif
	    if (!SavageDDC1(pScrn->scrnIndex)) {
		if ( xf86LoadSubModule(pScrn, "i2c") ) {
		    xf86LoaderReqSymLists(i2cSymbols,NULL);
		    if (SavageI2CInit(pScrn)) {
			unsigned char tmp;

			InI2CREG(psav,tmp);
			OutI2CREG(psav,tmp | 0x13);
			xf86SetDDCproperties(pScrn,xf86PrintEDID(
			    xf86DoEDID_DDC2(pScrn->scrnIndex,psav->I2C)));
			OutI2CREG(psav,tmp);
		    }
		}
	    }
	}
    }

    /* Savage ramdac speeds */
    pScrn->numClocks = 4;
    pScrn->clock[0] = 250000;
    pScrn->clock[1] = 250000;
    pScrn->clock[2] = 220000;
    pScrn->clock[3] = 220000;

    if (psav->dacSpeedBpp <= 0) {
	if (pScrn->bitsPerPixel > 24)
	    psav->dacSpeedBpp = pScrn->clock[3];
	else if (pScrn->bitsPerPixel >= 24)
	    psav->dacSpeedBpp = pScrn->clock[2];
	else if (pScrn->bitsPerPixel > 8)
	    psav->dacSpeedBpp = pScrn->clock[1];
	else psav->dacSpeedBpp = pScrn->clock[0];
    }

    /* Set ramdac limits */
    psav->maxClock = psav->dacSpeedBpp;

    /* detect current mclk */
    VGAOUT8(0x3c4, 0x08);
    sr8 = VGAIN8(0x3c5);
    VGAOUT8(0x3c5, 0x06);
    VGAOUT8(0x3c4, 0x10);
    n = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x11);
    m = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x08);
    VGAOUT8(0x3c5, sr8);
    m &= 0x7f;
    n1 = n & 0x1f;
    n2 = (n >> 5) & 0x03;
    mclk = ((1431818 * (m+2)) / (n1+2) / (1 << n2) + 50) / 100;
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected current MCLK value of %1.3f MHz\n",
	       mclk / 1000.0);

    psav->minClock = 10000;

    pScrn->maxHValue = 2048 << 3;	/* 11 bits of h_total 8-pixel units */
    pScrn->maxVValue = 2048;		/* 11 bits of v_total */
    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    /* Check LCD panel information */

    if( S3_SAVAGE_MOBILE_SERIES(psav->Chipset) && !psav->CrtOnly )
    {
	unsigned char cr6b = hwp->readCrtc( hwp, 0x6b );

	int panelX = (hwp->readSeq(hwp, 0x61) + 
	    ((hwp->readSeq(hwp, 0x66) & 0x02) << 7) + 1) * 8;
	int panelY = hwp->readSeq(hwp, 0x69) + 
	    ((hwp->readSeq(hwp, 0x6e) & 0x70) << 4) + 1;

	char * sTechnology = "Unknown";

	/* OK, I admit it.  I don't know how to limit the max dot clock
	 * for LCD panels of various sizes.  I thought I copied the formula
	 * from the BIOS, but many users have informed me of my folly.
	 *
	 * Instead, I'll abandon any attempt to automatically limit the 
	 * clock, and add an LCDClock option to XF86Config.  Some day,
	 * I should come back to this.
	 */

	enum ACTIVE_DISPLAYS { /* These are the bits in CR6B */
	    ActiveCRT = 0x01,
	    ActiveLCD = 0x02,
	    ActiveTV = 0x04,
	    ActiveCRT2 = 0x20,
	    ActiveDUO = 0x80
	};

	if( (hwp->readSeq( hwp, 0x39 ) & 0x03) == 0 )
	{
	    sTechnology = "TFT";
	}
	else if( (hwp->readSeq( hwp, 0x30 ) & 0x01) == 0 )
	{
	    sTechnology = "DSTN";
	}
	else
	{
	    sTechnology = "STN";
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		   "%dx%d %s LCD panel detected %s\n", 
		   panelX, panelY, sTechnology,
		   cr6b & ActiveLCD ? "and active" : "but not active");

	if( cr6b & ActiveLCD ) {
	    /* If the LCD is active and panel expansion is enabled, */
	    /* we probably want to kill the HW cursor. */

	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       "- Limiting video mode to %dx%d\n",
		       panelX, panelY );
	    
	    psav->PanelX = panelX;
	    psav->PanelY = panelY;

	    if( psav->LCDClock > 0.0 )
	    {
		psav->maxClock = psav->LCDClock * 1000.0;
		xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
			    "- Limiting dot clock to %1.2f MHz\n",
			    psav->LCDClock );
	    }
	}
    }
  
    clockRanges = xnfcalloc(sizeof(ClockRange),1);
    clockRanges->next = NULL;
    clockRanges->minClock = psav->minClock;
    clockRanges->maxClock = psav->maxClock;
    clockRanges->clockIndex = -1;
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = TRUE;
    clockRanges->ClockDivFactor = 1.0;
    clockRanges->ClockMulFactor = 1.0;

    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			  pScrn->display->modes, clockRanges, NULL, 
			  256, 2048, 16 * pScrn->bitsPerPixel,
			  128, 2048, 
			  pScrn->virtualX, pScrn->virtualY,
			  psav->videoRambytes, LOOKUP_BEST_REFRESH);

    if (i == -1) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86ValidateModes failure\n");
	SavageFreeRec(pScrn);
	vbeFree(psav->pVbe);
	return FALSE;
    }

    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	SavageFreeRec(pScrn);
	vbeFree(psav->pVbe);
	return FALSE;
    }

    if( psav->UseBIOS )
    {
	/* Go probe the BIOS for all the modes and refreshes at this depth. */

	if( psav->ModeTable )
	{
	    SavageFreeBIOSModeTable( psav, &psav->ModeTable );
	}

	psav->ModeTable = SavageGetBIOSModeTable( psav, pScrn->bitsPerPixel );

	if( !psav->ModeTable || !psav->ModeTable->NumModes ) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		       "Failed to fetch any BIOS modes.  Disabling BIOS.\n");
	    psav->UseBIOS = FALSE;
	}
	else
	/*if( xf86Verbose )*/
	{
	    SavageModeEntryPtr pmt;

	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       "Found %d modes at this depth:\n",
		       psav->ModeTable->NumModes);

	    for(
		i = 0, pmt = psav->ModeTable->Modes; 
		i < psav->ModeTable->NumModes; 
		i++, pmt++ )
	    {
		int j;
		ErrorF( "    [%03x] %d x %d", 
			pmt->VesaMode, pmt->Width, pmt->Height );
		for( j = 0; j < pmt->RefreshCount; j++ )
		{
		    ErrorF( ", %dHz", pmt->RefreshRate[j] );
		}
		ErrorF( "\n");
	    }
	}
    }

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);
    pScrn->currentMode = pScrn->modes;
    xf86PrintModes(pScrn);
    xf86SetDpi(pScrn, 0, 0);

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	SavageFreeRec(pScrn);
	vbeFree(psav->pVbe);
	return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);

    if( !psav->NoAccel ) {
	if( !xf86LoadSubModule(pScrn, "xaa") ) {
	    SavageFreeRec(pScrn);
	    vbeFree(psav->pVbe);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL );
    }

    if (psav->hwcursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    SavageFreeRec(pScrn);
	    vbeFree(psav->pVbe);
	    return FALSE;
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    if (psav->shadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    SavageFreeRec(pScrn);
	    vbeFree(psav->pVbe);
	    return FALSE;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }
    vbeFree(psav->pVbe);

    return TRUE;
}


static Bool SavageEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    TRACE(("SavageEnterVT(%d)\n", flags));

    gpScrn = pScrn;
    SavageEnableMMIO(pScrn);
    SavageSave(pScrn);
    return SavageModeInit(pScrn, pScrn->currentMode);
}


static void SavageLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    vgaRegPtr vgaSavePtr = &hwp->SavedReg;
    SavageRegPtr SavageSavePtr = &psav->SavedReg;

    TRACE(("SavageLeaveVT(%d)\n", flags));
    gpScrn = pScrn;
    SavageWriteMode(pScrn, vgaSavePtr, SavageSavePtr, FALSE);
    SavageDisableMMIO(pScrn);
}


static void SavageSave(ScrnInfoPtr pScrn)
{
    unsigned char cr3a, cr53, cr66;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaSavePtr = &hwp->SavedReg;
    SavagePtr psav = SAVPTR(pScrn);
    SavageRegPtr save = &psav->SavedReg;
    unsigned short vgaCRReg = psav->vgaIOBase + 5;
    unsigned short vgaCRIndex = psav->vgaIOBase + 4;

    TRACE(("SavageSave()\n"));

    VGAOUT16(vgaCRIndex, 0x4838);
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

    VGAOUT8(vgaCRIndex, 0x66);
    cr66 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr66 | 0x80);
    VGAOUT8(vgaCRIndex, 0x3a);
    cr3a = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr3a | 0x80);
    VGAOUT8(vgaCRIndex, 0x53);
    cr53 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr53 & 0x7f);

    if (xf86IsPrimaryPci(psav->PciInfo))
	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_ALL);
    else
	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_MODE);

    VGAOUT8(vgaCRIndex, 0x66);
    VGAOUT8(vgaCRReg, cr66);
    VGAOUT8(vgaCRIndex, 0x3a);
    VGAOUT8(vgaCRReg, cr3a);

    VGAOUT8(vgaCRIndex, 0x66);
    VGAOUT8(vgaCRReg, cr66);
    VGAOUT8(vgaCRIndex, 0x3a);
    VGAOUT8(vgaCRReg, cr3a);

    /* unlock extended seq regs */
    VGAOUT8(0x3c4, 0x08);
    save->SR08 = VGAIN8(0x3c5);
    VGAOUT8(0x3c5, 0x06);

    /* now save all the extended regs we need */
    VGAOUT8(vgaCRIndex, 0x31);
    save->CR31 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x32);
    save->CR32 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x34);
    save->CR34 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x36);
    save->CR36 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x3a);
    save->CR3A = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x40);
    save->CR40 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x42);
    save->CR42 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x45);
    save->CR45 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x50);
    save->CR50 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x51);
    save->CR51 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x53);
    save->CR53 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x58);
    save->CR58 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x60);
    save->CR60 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x66);
    save->CR66 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x67);
    save->CR67 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x68);
    save->CR68 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x69);
    save->CR69 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x6f);
    save->CR6F = VGAIN8(vgaCRReg);

    VGAOUT8(vgaCRIndex, 0x33);
    save->CR33 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x86);
    save->CR86 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x88);
    save->CR88 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x90);
    save->CR90 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x91);
    save->CR91 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0xb0);
    save->CRB0 = VGAIN8(vgaCRReg) | 0x80;

    /* extended mode timing regs */
    VGAOUT8(vgaCRIndex, 0x3b);
    save->CR3B = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x3c);
    save->CR3C = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x43);
    save->CR43 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x5d);
    save->CR5D = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x5e);
    save->CR5E = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRIndex, 0x65);
    save->CR65 = VGAIN8(vgaCRReg);

    /* save seq extended regs for DCLK PLL programming */
    VGAOUT8(0x3c4, 0x0e);
    save->SR0E = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x0f);
    save->SR0F = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x10);
    save->SR10 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x11);
    save->SR11 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x12);
    save->SR12 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x13);
    save->SR13 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x29);
    save->SR29 = VGAIN8(0x3c5);

    VGAOUT8(0x3c4, 0x15);
    save->SR15 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x30);
    save->SR30 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x18);
    save->SR18 = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x1b);
    save->SR1B = VGAIN8(0x3c5);

    /* Save flat panel expansion regsters. */

    if( S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ) {
	int i;
	for( i = 0; i < 8; i++ ) {
	    VGAOUT8(0x3c4, 0x54+i);
	    save->SR54[i] = VGAIN8(0x3c5);
	}
    }

    VGAOUT8(vgaCRIndex, 0x66);
    cr66 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr66 | 0x80);
    VGAOUT8(vgaCRIndex, 0x3a);
    cr3a = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr3a | 0x80);

    /* now save MIU regs */
    if( ! S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ) {
	save->MMPR0 = INREG(FIFO_CONTROL_REG);
	save->MMPR1 = INREG(MIU_CONTROL_REG);
	save->MMPR2 = INREG(STREAMS_TIMEOUT_REG);
	save->MMPR3 = INREG(MISC_TIMEOUT_REG);
    }

    VGAOUT8(vgaCRIndex, 0x3a);
    VGAOUT8(vgaCRReg, cr3a);
    VGAOUT8(vgaCRIndex, 0x66);
    VGAOUT8(vgaCRReg, cr66);

    if (!psav->ModeStructInit) {
	vgaHWCopyReg(&hwp->ModeReg, vgaSavePtr);
	memcpy(&psav->ModeReg, save, sizeof(SavageRegRec));
	psav->ModeStructInit = TRUE;
    }

#if 0
    if (xf86GetVerbosity() > 1)
	SavagePrintRegs(pScrn);
#endif

    return;
}


static void SavageWriteMode(ScrnInfoPtr pScrn, vgaRegPtr vgaSavePtr,
			    SavageRegPtr restore, Bool Entering)
{
    unsigned char tmp, cr3a, cr66;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    int vgaCRIndex, vgaCRReg, vgaIOBase;


    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;
    
    TRACE(("SavageWriteMode(%x)\n", restore->mode));

    if( Entering && 
	(!S3_SAVAGE_MOBILE_SERIES(psav->Chipset) || (psav->ForceInit))
    )
	SavageInitialize2DEngine(pScrn);

    /*
     * If we figured out a VESA mode number for this timing, just use
     * the S3 BIOS to do the switching, with a few additional tweaks.
     */

    if( psav->UseBIOS && restore->mode > 0x13 )
    {
	int width;
	unsigned short cr6d;
	unsigned short cr79 = 0;

	/* Set up the mode.  Don't clear video RAM. */
	SavageSetVESAMode( psav, restore->mode | 0x8000, restore->refresh );

	/* Restore the DAC. */
	vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_CMAP);

	/* Unlock the extended registers. */

#if 0
	/* Which way is better? */
	hwp->writeCrtc( hwp, 0x38, 0x48 );
	hwp->writeCrtc( hwp, 0x39, 0xa0 );
	hwp->writeSeq( hwp, 0x08, 0x06 );
#endif

	VGAOUT16(vgaCRIndex, 0x4838);
	VGAOUT16(vgaCRIndex, 0xA039);
	VGAOUT16(0x3c4, 0x0608);

	/* Enable linear addressing. */

	VGAOUT16(vgaCRIndex, 0x1358);

	/* Disable old MMIO. */

	VGAOUT8(vgaCRIndex, 0x53);
	VGAOUT8(vgaCRReg, VGAIN8(vgaCRReg) & ~0x10);

	/* Set the color mode. */

	VGAOUT8(vgaCRIndex, 0x67);
	VGAOUT8(vgaCRReg, restore->CR67);

	/* Enable gamma correction. */

	VGAOUT8(0x3c4, 0x1b);
	if( (pScrn->bitsPerPixel == 32) && !psav->DGAactive )
		VGAOUT8(0x3c5, 0x28 );
	else
		VGAOUT8(0x3c5, 0x00 );

	/* We may need TV/panel fixups here.  See s3bios.c line 2904. */

	/* Set FIFO fetch delay. */
	VGAOUT8(vgaCRIndex, 0x85);
	VGAOUT8(vgaCRReg, (VGAIN8(vgaCRReg) & 0xf8) | 0x03);

	/* Patch CR79.  These values are magical. */

	if( !S3_SAVAGE_MOBILE_SERIES(psav->Chipset) )
	{
	    VGAOUT8(vgaCRIndex, 0x6d);
	    cr6d = VGAIN8(vgaCRReg);

	    cr79 = 0x04;

	    if( pScrn->displayWidth >= 1024 )
	    {
		if(pScrn->bitsPerPixel == 32 )
		{
		    if( restore->refresh >= 130 )
			cr79 = 0x03;
		    else if( pScrn->displayWidth >= 1280 )
			cr79 = 0x02;
		    else if(
			(pScrn->displayWidth == 1024) &&
			(restore->refresh >= 75)
		    )
		    {
			if( cr6d && LCD_ACTIVE )
			    cr79 = 0x05;
			else
			    cr79 = 0x08;
		    }
		}
		else if( pScrn->bitsPerPixel == 16)
		{

/* The windows driver uses 0x13 for 16-bit 130Hz, but I see terrible
 * screen artifacts with that value.  Let's keep it low for now.
 *		if( restore->refresh >= 130 )
 *		    cr79 = 0x13;
 *		else
 */
		    if( pScrn->displayWidth == 1024 )
		    {
			if( cr6d && LCD_ACTIVE )
			    cr79 = 0x08;
			else
			    cr79 = 0x0e;
		    }
		}
	    }
	}

        if( (psav->Chipset != S3_SAVAGE2000) && 
	    !S3_SAVAGE_MOBILE_SERIES(psav->Chipset) )
	    VGAOUT16(vgaCRIndex, (cr79 << 8) | 0x79);

	/* Make sure 16-bit memory access is enabled. */

	VGAOUT16(vgaCRIndex, 0x0c31);

	/* Enable the graphics engine. */

	VGAOUT16(vgaCRIndex, 0x0140);

	/* Handle the pitch. */

        VGAOUT8(vgaCRIndex, 0x50);
        VGAOUT8(vgaCRReg, VGAIN8(vgaCRReg) | 0xC1);

	width = (pScrn->displayWidth * (pScrn->bitsPerPixel / 8)) >> 3;
	VGAOUT16(vgaCRIndex, ((width & 0xff) << 8) | 0x13 );
	VGAOUT16(vgaCRIndex, ((width & 0x300) << 4) | 0x51 );

	/* Some non-S3 BIOSes enable block write even on non-SGRAM devices. */

	switch( psav->Chipset )
	{
	    case S3_SAVAGE2000:
		VGAOUT8(vgaCRIndex, 0x73);
		VGAOUT8(vgaCRReg, VGAIN8(vgaCRReg) & 0xdf );
		break;

	    case S3_SAVAGE3D:
	    case S3_SAVAGE4:
		VGAOUT8(vgaCRIndex, 0x68);
		if( !(VGAIN8(vgaCRReg) & 0x80) )
		{
		    /* Not SGRAM; disable block write. */
		    VGAOUT8(vgaCRIndex, 0x88);
		    VGAOUT8(vgaCRReg, VGAIN8(vgaCRReg) | 0x10);
		}
		break;
	}

	/* set the correct clock for some BIOSes */
	VGAOUT8(VGA_MISC_OUT_W, 
		VGAIN8(VGA_MISC_OUT_R) | 0x0C);
	/* Some BIOSes turn on clock doubling on non-doubled modes */
	if (pScrn->bitsPerPixel < 24) {
	    VGAOUT8(vgaCRIndex, 0x67);
	    if (!(VGAIN8(vgaCRReg) & 0x10)) {
		VGAOUT8(0x3c4, 0x15);
		VGAOUT8(0x3c5, VGAIN8(0x3C5) & ~0x10);
		VGAOUT8(0x3c4, 0x18);
		VGAOUT8(0x3c5, VGAIN8(0x3c5) & ~0x80);
	    }
	}

	SavageInitialize2DEngine(pScrn);
	SavageSetGBD(pScrn);

	VGAOUT16(vgaCRIndex, 0x0140);

	SavageSetGBD(pScrn);

	return;
    }

    VGAOUT8(0x3c2, 0x23);
    VGAOUT16(vgaCRIndex, 0x4838);
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

    vgaHWProtect(pScrn, TRUE);

    /* will we be reenabling STREAMS for the new mode? */
    psav->STREAMSRunning = 0;

    /* reset GE to make sure nothing is going on */
    VGAOUT8(vgaCRIndex, 0x66);
    if(VGAIN8(vgaCRReg) & 0x01)
	SavageGEReset(pScrn,0,__LINE__,__FILE__);

    /*
     * Some Savage/MX and /IX systems go nuts when trying to exit the
     * server after WindowMaker has displayed a gradient background.  I
     * haven't been able to find what causes it, but a non-destructive
     * switch to mode 3 here seems to eliminate the issue.
     */

    if( ((restore->CR31 & 0x0a) == 0) && psav->pInt10 ) {
	SavageSetTextMode( psav );
    }

    VGAOUT8(vgaCRIndex, 0x67);
    (void) VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, restore->CR67 & ~0x0c); /* no STREAMS yet */

    /* restore extended regs */
    VGAOUT8(vgaCRIndex, 0x66);
    VGAOUT8(vgaCRReg, restore->CR66);
    VGAOUT8(vgaCRIndex, 0x3a);
    VGAOUT8(vgaCRReg, restore->CR3A);
    VGAOUT8(vgaCRIndex, 0x31);
    VGAOUT8(vgaCRReg, restore->CR31);
    VGAOUT8(vgaCRIndex, 0x32);
    VGAOUT8(vgaCRReg, restore->CR32);
    VGAOUT8(vgaCRIndex, 0x58);
    VGAOUT8(vgaCRReg, restore->CR58);
    VGAOUT8(vgaCRIndex, 0x53);
    VGAOUT8(vgaCRReg, restore->CR53 & 0x7f);

    VGAOUT16(0x3c4, 0x0608);

    /* Restore DCLK registers. */

    VGAOUT8(0x3c4, 0x0e);
    VGAOUT8(0x3c5, restore->SR0E);
    VGAOUT8(0x3c4, 0x0f);
    VGAOUT8(0x3c5, restore->SR0F);
    VGAOUT8(0x3c4, 0x29);
    VGAOUT8(0x3c5, restore->SR29);
    VGAOUT8(0x3c4, 0x15);
    VGAOUT8(0x3c5, restore->SR15);

    /* Restore flat panel expansion regsters. */
    if( S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ) {
	int i;
	for( i = 0; i < 8; i++ ) {
	    VGAOUT8(0x3c4, 0x54+i);
	    VGAOUT8(0x3c5, restore->SR54[i]);
	}
    }

    /* restore the standard vga regs */
    if (xf86IsPrimaryPci(psav->PciInfo))
	vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_ALL);
    else
	vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_MODE);

    /* extended mode timing registers */
    VGAOUT8(vgaCRIndex, 0x53);
    VGAOUT8(vgaCRReg, restore->CR53);
    VGAOUT8(vgaCRIndex, 0x5d);
    VGAOUT8(vgaCRReg, restore->CR5D);
    VGAOUT8(vgaCRIndex, 0x5e);
    VGAOUT8(vgaCRReg, restore->CR5E);
    VGAOUT8(vgaCRIndex, 0x3b);
    VGAOUT8(vgaCRReg, restore->CR3B);
    VGAOUT8(vgaCRIndex, 0x3c);
    VGAOUT8(vgaCRReg, restore->CR3C);
    VGAOUT8(vgaCRIndex, 0x43);
    VGAOUT8(vgaCRReg, restore->CR43);
    VGAOUT8(vgaCRIndex, 0x65);
    VGAOUT8(vgaCRReg, restore->CR65);

    /* restore the desired video mode with cr67 */
    VGAOUT8(vgaCRIndex, 0x67);
    VGAOUT8(vgaCRReg, restore->CR67 & ~0x0c); /* no STREAMS yet */

    /* other mode timing and extended regs */
    VGAOUT8(vgaCRIndex, 0x34);
    VGAOUT8(vgaCRReg, restore->CR34);
    VGAOUT8(vgaCRIndex, 0x40);
    VGAOUT8(vgaCRReg, restore->CR40);
    VGAOUT8(vgaCRIndex, 0x42);
    VGAOUT8(vgaCRReg, restore->CR42);
    VGAOUT8(vgaCRIndex, 0x45);
    VGAOUT8(vgaCRReg, restore->CR45);
    VGAOUT8(vgaCRIndex, 0x50);
    VGAOUT8(vgaCRReg, restore->CR50);
    VGAOUT8(vgaCRIndex, 0x51);
    VGAOUT8(vgaCRReg, restore->CR51);

    /* memory timings */
    VGAOUT8(vgaCRIndex, 0x36);
    VGAOUT8(vgaCRReg, restore->CR36);
    VGAOUT8(vgaCRIndex, 0x60);
    VGAOUT8(vgaCRReg, restore->CR60);
    VGAOUT8(vgaCRIndex, 0x68);
    VGAOUT8(vgaCRReg, restore->CR68);
    VGAOUT8(vgaCRIndex, 0x69);
    VGAOUT8(vgaCRReg, restore->CR69);
    VGAOUT8(vgaCRIndex, 0x6f);
    VGAOUT8(vgaCRReg, restore->CR6F);

    VGAOUT8(vgaCRIndex, 0x33);
    VGAOUT8(vgaCRReg, restore->CR33);
    VGAOUT8(vgaCRIndex, 0x86);
    VGAOUT8(vgaCRReg, restore->CR86);
    VGAOUT8(vgaCRIndex, 0x88);
    VGAOUT8(vgaCRReg, restore->CR88);
    VGAOUT8(vgaCRIndex, 0x90);
    VGAOUT8(vgaCRReg, restore->CR90);
    VGAOUT8(vgaCRIndex, 0x91);
    VGAOUT8(vgaCRReg, restore->CR91);
    if( psav->Chipset == S3_SAVAGE4 )
    {
	VGAOUT8(vgaCRIndex, 0xb0);
	VGAOUT8(vgaCRReg, restore->CRB0);
    }

    VGAOUT8(vgaCRIndex, 0x32);
    VGAOUT8(vgaCRReg, restore->CR32);

    /* unlock extended seq regs */
    VGAOUT8(0x3c4, 0x08);
    VGAOUT8(0x3c5, 0x06);

    /* Restore extended sequencer regs for MCLK. SR10 == 255 indicates that 
     * we should leave the default SR10 and SR11 values there.
     */
    if (restore->SR10 != 255) {
	VGAOUT8(0x3c4, 0x10);
	VGAOUT8(0x3c5, restore->SR10);
	VGAOUT8(0x3c4, 0x11);
	VGAOUT8(0x3c5, restore->SR11);
    }

    /* restore extended seq regs for dclk */
    VGAOUT8(0x3c4, 0x0e);
    VGAOUT8(0x3c5, restore->SR0E);
    VGAOUT8(0x3c4, 0x0f);
    VGAOUT8(0x3c5, restore->SR0F);
    VGAOUT8(0x3c4, 0x12);
    VGAOUT8(0x3c5, restore->SR12);
    VGAOUT8(0x3c4, 0x13);
    VGAOUT8(0x3c5, restore->SR13);
    VGAOUT8(0x3c4, 0x29);
    VGAOUT8(0x3c5, restore->SR29);

    VGAOUT8(0x3c4, 0x18);
    VGAOUT8(0x3c5, restore->SR18);
    VGAOUT8(0x3c4, 0x1b);
    if( psav->DGAactive )
	VGAOUT8(0x3c5, restore->SR1B & ~0x28);
    else
	VGAOUT8(0x3c5, restore->SR1B);

    /* load new m, n pll values for dclk & mclk */
    VGAOUT8(0x3c4, 0x15);
    tmp = VGAIN8(0x3c5) & ~0x21;

    VGAOUT8(0x3c5, tmp | 0x03);
    VGAOUT8(0x3c5, tmp | 0x23);
    VGAOUT8(0x3c5, tmp | 0x03);
    VGAOUT8(0x3c5, restore->SR15);
    usleep( 100 );

    VGAOUT8(0x3c4, 0x30);
    VGAOUT8(0x3c5, restore->SR30);
    VGAOUT8(0x3c4, 0x08);
    VGAOUT8(0x3c5, restore->SR08);

    /* now write out cr67 in full, possibly starting STREAMS */
    VerticalRetraceWait(psav);
    VGAOUT8(vgaCRIndex, 0x67);
#if 0
    VGAOUT8(vgaCRReg, 0x50);
    usleep(10000);
    VGAOUT8(vgaCRIndex, 0x67);
#endif
    VGAOUT8(vgaCRReg, restore->CR67);

    VGAOUT8(vgaCRIndex, 0x66);
    cr66 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr66 | 0x80);
    VGAOUT8(vgaCRIndex, 0x3a);
    cr3a = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, cr3a | 0x80);

    if (Entering)
	SavageGEReset(pScrn,0,__LINE__,__FILE__);

    if( !S3_SAVAGE_MOBILE_SERIES(psav->Chipset) )
    {
	VerticalRetraceWait(psav);
	OUTREG(FIFO_CONTROL_REG, restore->MMPR0);
	OUTREG(MIU_CONTROL_REG, restore->MMPR1);
	OUTREG(STREAMS_TIMEOUT_REG, restore->MMPR2);
	OUTREG(MISC_TIMEOUT_REG, restore->MMPR3);
    }

    /* If we're going into graphics mode and acceleration was enabled, */
    /* go set up the BCI buffer and the global bitmap descriptor. */

#if 0
    if( Entering && (!psav->NoAccel) )
    {
	VGAOUT8(vgaCRIndex, 0x50);
	VGAOUT8(vgaCRReg, VGAIN8(vgaCRReg) | 0xC1);
	SavageInitialize2DEngine(pScrn);
    }
#endif

    VGAOUT8(vgaCRIndex, 0x66);
    VGAOUT8(vgaCRReg, cr66);
    VGAOUT8(vgaCRIndex, 0x3a);
    VGAOUT8(vgaCRReg, cr3a);

    if( Entering ) {
    	/* We reinit the engine here just as in the UseBIOS case
	 * as otherwise we lose performance because the engine
	 * isn't setup properly (Alan Hourihane - alanh@fairlite.demon.co.uk).
	 */
	SavageInitialize2DEngine(pScrn);
	SavageSetGBD(pScrn);

	VGAOUT16(vgaCRIndex, 0x0140);

	SavageSetGBD(pScrn);
    }

    vgaHWProtect(pScrn, FALSE);

    return;
}


static Bool SavageMapMMIO(ScrnInfoPtr pScrn)
{
    SavagePtr psav;

    TRACE(("SavageMapMMIO()\n"));

    psav = SAVPTR(pScrn);

    if( S3_SAVAGE3D_SERIES(psav->Chipset) ) {
	psav->MmioBase = psav->PciInfo->memBase[0] + SAVAGE_NEWMMIO_REGBASE_S3;
	psav->FrameBufferBase = psav->PciInfo->memBase[0];
    }
    else {
	psav->MmioBase = psav->PciInfo->memBase[0] + SAVAGE_NEWMMIO_REGBASE_S4;
	psav->FrameBufferBase = psav->PciInfo->memBase[1];
    }

    xf86DrvMsg( pScrn->scrnIndex, X_PROBED,
	"mapping MMIO @ 0x%lx with size 0x%x\n",
	psav->MmioBase, SAVAGE_NEWMMIO_REGSIZE);

    psav->MapBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, psav->PciTag,
				  psav->MmioBase,
				  SAVAGE_NEWMMIO_REGSIZE);
#if 0
    psav->MapBaseDense = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO_32BIT,
				       psav->PciTag,
				       psav->PciInfo->memBase[0],
				       0x8000);
#endif
    if (!psav->MapBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Internal error: cound not map registers\n");
	return FALSE;
    }

    psav->BciMem = psav->MapBase + 0x10000;

    SavageEnableMMIO(pScrn);

    return TRUE;
}



static Bool SavageMapFB(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);

    TRACE(("SavageMapFB()\n"));

    xf86DrvMsg( pScrn->scrnIndex, X_PROBED,
	"mapping framebuffer @ 0x%lx with size 0x%x\n", 
	psav->FrameBufferBase, psav->videoRambytes);

    if (psav->videoRambytes) {
	psav->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				     psav->PciTag, psav->FrameBufferBase,
				     psav->videoRambytes);
	if (!psav->FBBase) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Internal error: could not map framebuffer\n");
	    return FALSE;
	}
	psav->FBStart = psav->FBBase;
    }
    pScrn->memPhysBase = psav->PciInfo->memBase[0];
    pScrn->fbOffset = 0;

    return TRUE;
}


static void SavageUnmapMem(ScrnInfoPtr pScrn, int All)
{
    SavagePtr psav;

    psav = SAVPTR(pScrn);

    TRACE(("SavageUnmapMem(%x,%x)\n", psav->MapBase, psav->FBBase));

    if (psav->PrimaryVidMapped) {
	vgaHWUnmapMem(pScrn);
	psav->PrimaryVidMapped = FALSE;
    }

    SavageDisableMMIO(pScrn);

    if (All && psav->MapBase) {
	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)psav->MapBase,
			SAVAGE_NEWMMIO_REGSIZE);
	psav->MapBase = 0;
    }

    if (psav->FBBase) {
	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)psav->FBBase,
			psav->videoRambytes);
	psav->FBBase = 0;
    }

#if 0
    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)psav->MapBaseDense,
		    0x8000);
#endif

    return;
}


static Bool SavageScreenInit(int scrnIndex, ScreenPtr pScreen,
			     int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    SavagePtr psav;
    EntityInfoPtr pEnt;
    int ret;

    TRACE(("SavageScreenInit()\n"));

    pScrn = xf86Screens[pScreen->myNum];
    psav = SAVPTR(pScrn);

    pEnt = xf86GetEntityInfo(pScrn->entityList[0]); 
    psav->pVbe = VBEInit(NULL, pEnt->index);
 
    SavageEnableMMIO(pScrn);

    if (!SavageMapFB(pScrn))
	return FALSE;
 
    if( psav->ShadowStatus ) {
	psav->ShadowPhysical = 
	    psav->FrameBufferBase + psav->CursorKByte*1024 + 4096 - 32;
	
	psav->ShadowVirtual = (CARD32 *)
	    (psav->FBBase + psav->CursorKByte*1024 + 4096 - 32);
	
	xf86DrvMsg( pScrn->scrnIndex, X_PROBED,
		    "Shadow area physical %08lx, linear %p\n",
		    psav->ShadowPhysical, (void *)psav->ShadowVirtual );

	psav->WaitQueue = ShadowWait1;
	psav->WaitIdle = ShadowWait;
	psav->WaitIdleEmpty = ShadowWait;

	if( psav->Chipset == S3_SAVAGE2000 )
	    psav->dwBCIWait2DIdle = 0xc0040000;
	else
	    psav->dwBCIWait2DIdle = 0xc0020000;
    }
    psav->ShadowCounter = 0;

    SavageSave(pScrn);

    vgaHWBlankScreen(pScrn, TRUE);

    if (!SavageModeInit(pScrn, pScrn->currentMode))
	return FALSE;

    miClearVisualTypes();

    if (pScrn->bitsPerPixel == 16) {
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
			      pScrn->rgbBits, pScrn->defaultVisual))
	    return FALSE;
	if (!miSetPixmapDepths ())
	    return FALSE;
    } else {
	if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
	    return FALSE;
	if (!miSetPixmapDepths ())
	    return FALSE;
    }

    ret = SavageInternalScreenInit(scrnIndex, pScreen);
    if (!ret)
	return FALSE;

    xf86SetBlackWhitePixels(pScreen);

    if (pScrn->bitsPerPixel > 8) {
	VisualPtr visual;

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

    if( !psav->NoAccel ) {
	SavageInitAccel(pScreen);
    }

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    if( !psav->shadowFB )
	SavageDGAInit(pScreen);

    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    if (psav->hwcursor)
	if (!SavageHWCursorInit(pScreen))
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	       "Hardware cursor initialization failed\n");

    if (psav->shadowFB) {
	RefreshAreaFuncPtr refreshArea = SavageRefreshArea;
      
	if(psav->rotate) {
	    if (!psav->PointerMoved) {
		psav->PointerMoved = pScrn->PointerMoved;
		pScrn->PointerMoved = SavagePointerMoved;
	    }

	    switch(pScrn->bitsPerPixel) {
	    case 8:	refreshArea = SavageRefreshArea8;	break;
	    case 16:	refreshArea = SavageRefreshArea16;	break;
	    case 24:	refreshArea = SavageRefreshArea24;	break;
	    case 32:	refreshArea = SavageRefreshArea32;	break;
	    }
	}
      
	ShadowFBInit(pScreen, refreshArea);
    }

    if (!miCreateDefColormap(pScreen))
	    return FALSE;

    if (psav->Chipset == S3_SAVAGE4) {
        if (!xf86HandleColormaps(pScreen, 256, 6, SavageLoadPaletteSavage4,
				 NULL, 
				 CMAP_RELOAD_ON_MODE_SWITCH
				 | CMAP_PALETTED_TRUECOLOR
				 ))
	    return FALSE;
    } else {
        if (!xf86HandleColormaps(pScreen, 256, 6, SavageLoadPalette, NULL,
				 CMAP_RELOAD_ON_MODE_SWITCH
				 | CMAP_PALETTED_TRUECOLOR
				 ))
	    return FALSE;
    }

    vgaHWBlankScreen(pScrn, FALSE);

    psav->CloseScreen = pScreen->CloseScreen;
    pScreen->SaveScreen = SavageSaveScreen;
    pScreen->CloseScreen = SavageCloseScreen;

    if (xf86DPMSInit(pScreen, SavageDPMS, 0) == FALSE)
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "DPMS initialization failed\n");

    if( !psav->NoAccel && !SavagePanningCheck(pScrn) )
	SavageInitVideo( pScreen );

    if (serverGeneration == 1)
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    return TRUE;
}


static int SavageInternalScreenInit(int scrnIndex, ScreenPtr pScreen)
{
    int ret = TRUE;
    ScrnInfoPtr pScrn;
    SavagePtr psav;
    int width, height, displayWidth;
    unsigned char *FBStart;

    TRACE(("SavageInternalScreenInit()\n"));

    pScrn = xf86Screens[pScreen->myNum];
    psav = SAVPTR(pScrn);

    displayWidth = pScrn->displayWidth;

    if (psav->rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    } else {
	width = pScrn->virtualX;
	height = pScrn->virtualY;
    }
  
  
    if(psav->shadowFB) {
	psav->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
	psav->ShadowPtr = xalloc(psav->ShadowPitch * height);
	displayWidth = psav->ShadowPitch / (pScrn->bitsPerPixel >> 3);
	FBStart = psav->ShadowPtr;
    } else {
	psav->ShadowPtr = NULL;
	FBStart = psav->FBStart;
    }

    ret = fbScreenInit(pScreen, FBStart, width, height,
		       pScrn->xDpi, pScrn->yDpi,
		       displayWidth,
		       pScrn->bitsPerPixel);
    return ret;
}


static ModeStatus SavageValidMode(int index, DisplayModePtr pMode,
				  Bool verbose, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    SavagePtr psav = SAVPTR(pScrn);

    TRACE(("SavageValidMode\n"));

    /* We prohibit modes bigger than the LCD panel. */
    /* TODO We should do this only if the panel is active. */

    if( psav->TvOn )
    {
	if( pMode->HDisplay > psav->TVSizeX )
	    return MODE_VIRTUAL_X;

	if( pMode->VDisplay > psav->TVSizeY )
	    return MODE_VIRTUAL_Y;

    }
    if( 
	!psav->CrtOnly &&
	psav->PanelX &&
	( 
	    (pMode->HDisplay > psav->PanelX) ||
	    (pMode->VDisplay > psav->PanelY)
	)
    )
	    return MODE_PANEL;

    return MODE_OK;
}

static Bool SavageModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    int width, dclk, i, j; /*, refresh; */
    unsigned int m, n, r;
    unsigned char tmp = 0;
    SavageRegPtr new = &psav->ModeReg;
    vgaRegPtr vganew = &hwp->ModeReg;
    int vgaCRIndex, vgaCRReg, vgaIOBase;

    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    TRACE(("SavageModeInit(%dx%d, %dHz)\n", 
	mode->HDisplay, mode->VDisplay, mode->Clock));

#if 0
    ErrorF("Clock = %d, HDisplay = %d, HSStart = %d\n",
	    mode->Clock, mode->HDisplay, mode->HSyncStart);
    ErrorF("HSEnd = %d, HSkew = %d\n",
	    mode->HSyncEnd, mode->HSkew);
    ErrorF("VDisplay - %d, VSStart = %d, VSEnd = %d\n",
	    mode->VDisplay, mode->VSyncStart, mode->VSyncEnd);
    ErrorF("VTotal = %d\n",
	    mode->VTotal);
    ErrorF("HDisplay = %d, HSStart = %d\n",
	    mode->CrtcHDisplay, mode->CrtcHSyncStart);
    ErrorF("HSEnd = %d, HSkey = %d\n",
	    mode->CrtcHSyncEnd, mode->CrtcHSkew);
    ErrorF("VDisplay - %d, VSStart = %d, VSEnd = %d\n",
	    mode->CrtcVDisplay, mode->CrtcVSyncStart, mode->CrtcVSyncEnd);
    ErrorF("VTotal = %d\n",
	    mode->CrtcVTotal);
#endif



    psav->HorizScaleFactor = 1;

    if (!vgaHWInit(pScrn, mode))
	return FALSE;

    new->mode = 0;

    /* We need to set CR67 whether or not we use the BIOS. */

    dclk = mode->Clock;
    new->CR67 = 0x00;

    switch( pScrn->depth ) {
    case 8:
	if( (psav->Chipset == S3_SAVAGE2000) && (dclk >= 230000) )
	    new->CR67 = 0x10;	/* 8bpp, 2 pixels/clock */
	else
	    new->CR67 = 0x00;	/* 8bpp, 1 pixel/clock */
	break;
    case 15:
	if( 
	    S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ||
	    ((psav->Chipset == S3_SAVAGE2000) && (dclk >= 230000))
	)
	    new->CR67 = 0x30;	/* 15bpp, 2 pixel/clock */
	else
	    new->CR67 = 0x20;	/* 15bpp, 1 pixels/clock */
	break;
    case 16:
	if( 
	    S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ||
	    ((psav->Chipset == S3_SAVAGE2000) && (dclk >= 230000))
	)
	    new->CR67 = 0x50;	/* 16bpp, 2 pixel/clock */
	else
	    new->CR67 = 0x40;	/* 16bpp, 1 pixels/clock */
	break;
    case 24:
	if (pScrn->bitsPerPixel == 24 )
	    new->CR67 = 0x70;
	else
	    new->CR67 = 0xd0;
	break;
    }

    if( psav->UseBIOS ) {
	int refresh;
	SavageModeEntryPtr pmt;

	/* Scan through our BIOS list to locate the closest valid mode. */

	/* If we ever break 4GHz clocks on video boards, we'll need to
	 * change this.
	 */

        refresh = (mode->Clock * 1000) / (mode->HTotal * mode->VTotal);

#ifdef EXTENDED_DEBUG
	ErrorF( "Desired refresh rate = %dHz\n", refresh );
#endif

	for( i = 0, pmt = psav->ModeTable->Modes; 
	    i < psav->ModeTable->NumModes;
	    i++, pmt++ )
	{
	    if( (pmt->Width == mode->HDisplay) && 
	        (pmt->Height == mode->VDisplay) )
	    {
		int jDelta = 99;
		int jBest = 0;

		/* We have an acceptable mode.  Find a refresh rate. */

		new->mode = pmt->VesaMode;
		for( j = 0; j < pmt->RefreshCount; j++ )
		{
		    if( pmt->RefreshRate[j] == refresh )
		    {
			/* Exact match. */
			jBest = j;
			break;
		    }
		    else if( iabs(pmt->RefreshRate[j] - refresh) < jDelta )
		    {
			jDelta = iabs(pmt->RefreshRate[j] - refresh);
			jBest = j;
		    }
		}

		new->refresh = pmt->RefreshRate[jBest];
		break;
	    }
	}

	if( new->mode ) {
	    /* Success: we found a match in the BIOS. */
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, 
		      "Chose mode %x at %dHz.\n", new->mode, new->refresh );
	}
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, 
		      "No suitable BIOS mode found for %dx%d %dMHz.\n",
		      mode->HDisplay, mode->VDisplay, mode->Clock/1000 );
	}
    }

    if( !new->mode ) {
	/* 
	 * Either BIOS use is disabled, or we failed to find a suitable
	 * match.  Fall back to traditional register-crunching.
	 */

	VGAOUT8(vgaCRIndex, 0x3a);
	tmp = VGAIN8(vgaCRReg);
	if (psav->pci_burst)
	    new->CR3A = (tmp & 0x7f) | 0x15;
	else
	    new->CR3A = tmp | 0x95;

	new->CR53 = 0x00;
	new->CR31 = 0x8c;
	new->CR66 = 0x89;

	VGAOUT8(vgaCRIndex, 0x58);
	new->CR58 = VGAIN8(vgaCRReg) & 0x80;
	new->CR58 |= 0x13;

#if 0
	VGAOUT8(vgaCRIndex, 0x55);
	new->CR55 = VGAIN8(vgaCRReg);
	if (psav->hwcursor)
		new->CR55 |= 0x10;
#endif

	new->SR15 = 0x03 | 0x80;
	new->SR18 = 0x00;

/*	VGAOUT8(0x3c4, 0x1b);
	new->SR1B = VGAIN8(0x3c5);
	if( pScrn->depth == 24 )
		new->SR1B |= 0x28;
*/
	if( pScrn->depth == 24 )
	    new->SR1B = 0x28;
	else
	    new->SR1B = 0x00;


	new->CR43 = new->CR45 = new->CR65 = 0x00;

	VGAOUT8(vgaCRIndex, 0x40);
	new->CR40 = VGAIN8(vgaCRReg) & ~0x01;

	new->MMPR0 = 0x010400;
	new->MMPR1 = 0x00;
	new->MMPR2 = 0x0808;
	new->MMPR3 = 0x08080810;

	if (psav->fifo_aggressive || psav->fifo_moderate ||
	    psav->fifo_conservative) {
		new->MMPR1 = 0x0200;
		new->MMPR2 = 0x1808;
		new->MMPR3 = 0x08081810;
	}

	if (psav->MCLK <= 0) {
		new->SR10 = 255;
		new->SR11 = 255;
	}

	psav->NeedSTREAMS = FALSE;

	SavageCalcClock(dclk, 1, 1, 127, 0, 4, 180000, 360000,
			&m, &n, &r);
	new->SR12 = (r << 6) | (n & 0x3f);
	new->SR13 = m & 0xff;
	new->SR29 = (r & 4) | (m & 0x100) >> 5 | (n & 0x40) >> 2;

	if (psav->fifo_moderate) {
	    if (pScrn->bitsPerPixel < 24)
		new->MMPR0 -= 0x8000;
	    else
		new->MMPR0 -= 0x4000;
	} else if (psav->fifo_aggressive) {
	    if (pScrn->bitsPerPixel < 24)
		new->MMPR0 -= 0xc000;
	    else
		new->MMPR0 -= 0x6000;
	}

	if (mode->Flags & V_INTERLACE)
	    new->CR42 = 0x20;
	else
	    new->CR42 = 0x00;

	new->CR34 = 0x10;

	i = ((((mode->CrtcHTotal >> 3) - 5) & 0x100) >> 8) |
	    ((((mode->CrtcHDisplay >> 3) - 1) & 0x100) >> 7) |
	    ((((mode->CrtcHSyncStart >> 3) - 1) & 0x100) >> 6) |
	    ((mode->CrtcHSyncStart & 0x800) >> 7);

	if ((mode->CrtcHSyncEnd >> 3) - (mode->CrtcHSyncStart >> 3) > 64)
	    i |= 0x08;
	if ((mode->CrtcHSyncEnd >> 3) - (mode->CrtcHSyncStart >> 3) > 32)
	    i |= 0x20;
	j = (vganew->CRTC[0] + ((i & 0x01) << 8) +
	     vganew->CRTC[4] + ((i & 0x10) << 4) + 1) / 2;
	if (j - (vganew->CRTC[4] + ((i & 0x10) << 4)) < 4) {
	    if (vganew->CRTC[4] + ((i & 0x10) << 4) + 4 <= 
	        vganew->CRTC[0] + ((i & 0x01) << 8))
		j = vganew->CRTC[4] + ((i & 0x10) << 4) + 4;
	    else
		j = vganew->CRTC[0] + ((i & 0x01) << 8) + 1;
	}

	new->CR3B = j & 0xff;
	i |= (j & 0x100) >> 2;
	new->CR3C = (vganew->CRTC[0] + ((i & 0x01) << 8)) / 2;
	new->CR5D = i;
	new->CR5E = (((mode->CrtcVTotal - 2) & 0x400) >> 10) |
		    (((mode->CrtcVDisplay - 1) & 0x400) >> 9) |
		    (((mode->CrtcVSyncStart) & 0x400) >> 8) |
		    (((mode->CrtcVSyncStart) & 0x400) >> 6) | 0x40;
	width = (pScrn->displayWidth * (pScrn->bitsPerPixel / 8)) >> 3;
	new->CR91 = vganew->CRTC[19] = 0xff & width;
	new->CR51 = (0x300 & width) >> 4;
	new->CR90 = 0x80 | (width >> 8);
	vganew->MiscOutReg |= 0x0c;

	/* Set frame buffer description. */

	if (pScrn->bitsPerPixel <= 8)
	    new->CR50 = 0;
	else if (pScrn->bitsPerPixel <= 16)
	    new->CR50 = 0x10;
	else
	    new->CR50 = 0x30;

	if (pScrn->displayWidth == 640)
	    new->CR50 |= 0x40;
	else if (pScrn->displayWidth == 800)
	    new->CR50 |= 0x80;
	else if (pScrn->displayWidth == 1024)
	    new->CR50 |= 0x00;
	else if (pScrn->displayWidth == 1152)
	    new->CR50 |= 0x01;
	else if (pScrn->displayWidth == 1280)
	    new->CR50 |= 0xc0;
	else if (pScrn->displayWidth == 1600)
	    new->CR50 |= 0x81;
	else
	    new->CR50 |= 0xc1;	/* Use GBD */

	if( S3_SAVAGE_MOBILE_SERIES(psav->Chipset) )
	    new->CR33 = 0x00;
	else
	    new->CR33 = 0x08;
	     
	vganew->CRTC[0x17] = 0xeb;

	new->CR67 |= 1;

	VGAOUT8(vgaCRIndex, 0x36);
	new->CR36 = VGAIN8(vgaCRReg);
	VGAOUT8(vgaCRIndex, 0x68);
	new->CR68 = VGAIN8(vgaCRReg);
	new->CR69 = 0;
	VGAOUT8(vgaCRIndex, 0x6f);
	new->CR6F = VGAIN8(vgaCRReg);
	VGAOUT8(vgaCRIndex, 0x88);
	new->CR86 = VGAIN8(vgaCRReg) | 0x08;
	VGAOUT8(vgaCRIndex, 0xb0);
	new->CRB0 = VGAIN8(vgaCRReg) | 0x80;
    }

    pScrn->vtSema = TRUE;

    /* do it! */
    SavageWriteMode(pScrn, vganew, new, TRUE);
    SavageAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    return TRUE;
}


static Bool SavageCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    vgaRegPtr vgaSavePtr = &hwp->SavedReg;
    SavageRegPtr SavageSavePtr = &psav->SavedReg;

    TRACE(("SavageCloseScreen\n"));

    if (psav->pVbe)
      vbeFree(psav->pVbe);
    psav->pVbe = NULL;

    if( psav->AccelInfoRec ) {
        XAADestroyInfoRec( psav->AccelInfoRec );
	psav->AccelInfoRec = NULL;
    }

    if( psav->DGAModes ) {
	xfree( psav->DGAModes );
	psav->DGAModes = NULL;
	psav->numDGAModes = 0;
    }

    if (pScrn->vtSema) {
	SavageWriteMode(pScrn, vgaSavePtr, SavageSavePtr, FALSE);
	vgaHWLock(hwp);
	SavageUnmapMem(pScrn, 0);
    }

    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = psav->CloseScreen;

    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


static Bool SavageSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRACE(("SavageSaveScreen(0x%x)\n", mode));

    if( pScrn->vtSema && SAVPTR(pScrn)->hwcursor && SAVPTR(pScrn)->hwc_on) {

	if( xf86IsUnblank(mode) )
	    SavageShowCursor( pScrn );
	else
	    SavageHideCursor( pScrn );
	SAVPTR(pScrn)->hwc_on = TRUE;
    }

	return vgaHWSaveScreen(pScreen, mode);
}


void SavageAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    int Base;
    int vgaCRIndex, vgaCRReg, vgaIOBase;
    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    TRACE(("SavageAdjustFrame(%d,%d,%x)\n", x, y, flags));

    if (psav->ShowCache && y)
	y += pScrn->virtualY - 1;

    Base = ((y * pScrn->displayWidth + (x&~1)) *
	    (pScrn->bitsPerPixel / 8)) >> 2;
    /* now program the start address registers */
    VGAOUT16(vgaCRIndex, (Base & 0x00ff00) | 0x0c);
    VGAOUT16(vgaCRIndex, ((Base & 0x00ff) << 8) | 0x0d);
    VGAOUT8(vgaCRIndex, 0x69);
    VGAOUT8(vgaCRReg, (Base & 0x7f0000) >> 16);

    return;
}


Bool SavageSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    TRACE(("SavageSwitchMode\n"));
    return SavageModeInit(xf86Screens[scrnIndex], mode);
}


void SavageEnableMMIO(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    int vgaCRIndex, vgaCRReg;
    unsigned char val;

    TRACE(("SavageEnableMMIO\n"));

    vgaHWSetStdFuncs(hwp);
    vgaHWSetMmioFuncs(hwp, psav->MapBase, 0x8000);
    val = VGAIN8(0x3c3);
    VGAOUT8(0x3c3, val | 0x01);
    val = VGAIN8(VGA_MISC_OUT_R);
    VGAOUT8(VGA_MISC_OUT_W, val | 0x01);
    vgaCRIndex = psav->vgaIOBase + 4;
    vgaCRReg = psav->vgaIOBase + 5;

    if( psav->Chipset >= S3_SAVAGE4 )
    {
	VGAOUT8(vgaCRIndex, 0x40);
	val = VGAIN8(vgaCRReg);
	VGAOUT8(vgaCRReg, val | 1);
    }

    return;
}


void SavageDisableMMIO(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    int vgaCRIndex, vgaCRReg;
    unsigned char val;

    TRACE(("SavageDisableMMIO\n"));

    vgaCRIndex = psav->vgaIOBase + 4;
    vgaCRReg = psav->vgaIOBase + 5;

    if( psav->Chipset >= S3_SAVAGE4 )
    {
	VGAOUT8(vgaCRIndex, 0x40);
	val = VGAIN8(vgaCRReg);
	VGAOUT8(vgaCRReg, val | 1);
    }

    vgaHWSetStdFuncs(hwp);

    return;
}

void SavageLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
		       LOCO *colors, VisualPtr pVisual)
{
    SavagePtr psav = SAVPTR(pScrn);
    int i, index;

    for (i=0; i<numColors; i++) {
	index = indicies[i];
	VGAOUT8(0x3c8, index);
	VGAOUT8(0x3c9, colors[index].red);
	VGAOUT8(0x3c9, colors[index].green);
	VGAOUT8(0x3c9, colors[index].blue);
    }
}

#define inStatus1() (hwp->readST01( hwp ))

void SavageLoadPaletteSavage4(ScrnInfoPtr pScrn, int numColors, int *indicies,
		       LOCO *colors, VisualPtr pVisual)
{
    SavagePtr psav = SAVPTR(pScrn);
    int i, index;

    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VerticalRetraceWait(psav);

    for (i=0; i<numColors; i++) {
          if (!(inStatus1()) & 0x08)
  	    VerticalRetraceWait(psav); 
	index = indicies[i];
	VGAOUT8(0x3c8, index);
	VGAOUT8(0x3c9, colors[index].red);
	VGAOUT8(0x3c9, colors[index].green);
	VGAOUT8(0x3c9, colors[index].blue);
    }
}



static void SavageCalcClock(long freq, int min_m, int min_n1, int max_n1,

	/* Make sure linear addressing is enabled after the BIOS call. */
	/* Note that we must use an I/O port to do this. */
			   int min_n2, int max_n2, long freq_min,
			   long freq_max, unsigned int *mdiv,
			   unsigned int *ndiv, unsigned int *r)
{
    double ffreq, ffreq_min, ffreq_max;
    double div, diff, best_diff;
    unsigned int m;
    unsigned char n1, n2, best_n1=16+2, best_n2=2, best_m=125+2;

    ffreq = freq / 1000.0 / BASE_FREQ;
    ffreq_max = freq_max / 1000.0 / BASE_FREQ;
    ffreq_min = freq_min / 1000.0 / BASE_FREQ;

    if (ffreq < ffreq_min / (1 << max_n2)) {
	    ErrorF("invalid frequency %1.3f Mhz\n",
		   ffreq*BASE_FREQ);
	    ffreq = ffreq_min / (1 << max_n2);
    }
    if (ffreq > ffreq_max / (1 << min_n2)) {
	    ErrorF("invalid frequency %1.3f Mhz\n",
		   ffreq*BASE_FREQ);
	    ffreq = ffreq_max / (1 << min_n2);
    }

    /* work out suitable timings */

    best_diff = ffreq;

    for (n2=min_n2; n2<=max_n2; n2++) {
	for (n1=min_n1+2; n1<=max_n1+2; n1++) {
	    m = (int)(ffreq * n1 * (1 << n2) + 0.5);
	    if (m < min_m+2 || m > 127+2)
		continue;
	    div = (double)(m) / (double)(n1);
	    if ((div >= ffreq_min) &&
		(div <= ffreq_max)) {
		diff = ffreq - div / (1 << n2);
		if (diff < 0.0)
			diff = -diff;
		if (diff < best_diff) {
		    best_diff = diff;
		    best_m = m;
		    best_n1 = n1;
		    best_n2 = n2;
		}
	    }
	}
    }

    *ndiv = best_n1 - 2;
    *r = best_n2;
    *mdiv = best_m - 2;
}


void SavageGEReset(ScrnInfoPtr pScrn, int from_timeout, int line, char *file)
{
    unsigned char cr66;
    int r, success = 0;
    CARD32 fifo_control = 0, miu_control = 0;
    CARD32 streams_timeout = 0, misc_timeout = 0;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SavagePtr psav = SAVPTR(pScrn);
    int vgaCRIndex, vgaCRReg, vgaIOBase;

    TRACE(("SavageGEReset(%d,%s)\n", line, file));

    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    if (from_timeout) {
	if (psav->GEResetCnt++ < 10 || xf86GetVerbosity() > 1)
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "SavageGEReset called from %s line %d\n", file, line);
    } else
	psav->WaitIdleEmpty(psav);

    if (from_timeout && !S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ) {
	fifo_control = INREG(FIFO_CONTROL_REG);
	miu_control = INREG(MIU_CONTROL_REG);
	streams_timeout = INREG(STREAMS_TIMEOUT_REG);
	misc_timeout = INREG(MISC_TIMEOUT_REG);
    }

    VGAOUT8(vgaCRIndex, 0x66);
    cr66 = VGAIN8(vgaCRReg);

    usleep(10000);
    for (r=1; r<10; r++) {
	VGAOUT8(vgaCRReg, cr66 | 0x02);
	usleep(10000);
	VGAOUT8(vgaCRReg, cr66 & ~0x02);
	usleep(10000);

	if (!from_timeout)
	    psav->WaitIdleEmpty(psav);
	OUTREG(DEST_SRC_STR, psav->Bpl << 16 | psav->Bpl);

	usleep(10000);
	switch(psav->Chipset) {
	    case S3_SAVAGE3D:
	    case S3_SAVAGE_MX:
	      success = (STATUS_WORD0 & 0x0008ffff) == 0x00080000;
	      break;
	    case S3_SAVAGE4:
	    case S3_PROSAVAGE:
	    case S3_SUPERSAVAGE:
	      success = (ALT_STATUS_WORD0 & 0x0081ffff) == 0x00800000;
	      break;
	    case S3_SAVAGE2000:
	      success = (ALT_STATUS_WORD0 & 0x008fffff) == 0;
	      break;
	}	
	if(!success) {
	    usleep(10000);
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"restarting S3 graphics engine reset %2d ...\n", r);
	}
	else
	    break;
    }

    /* At this point, the FIFO is empty and the engine is idle. */

    if (from_timeout && !S3_SAVAGE_MOBILE_SERIES(psav->Chipset) ) {
	OUTREG(FIFO_CONTROL_REG, fifo_control);
	OUTREG(MIU_CONTROL_REG, miu_control);
	OUTREG(STREAMS_TIMEOUT_REG, streams_timeout);
	OUTREG(MISC_TIMEOUT_REG, misc_timeout);
    }

    OUTREG(SRC_BASE, 0);
    OUTREG(DEST_BASE, 0);
    OUTREG(CLIP_L_R, ((0) << 16) | pScrn->displayWidth);
    OUTREG(CLIP_T_B, ((0) << 16) | psav->ScissB);
    OUTREG(MONO_PAT_0, ~0);
    OUTREG(MONO_PAT_1, ~0);

    SavageSetGBD(pScrn);
}



/* This function is used to debug, it prints out the contents of s3 regs */

void
SavagePrintRegs(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char i;
    int vgaCRIndex = 0x3d4;
    int vgaCRReg = 0x3d5;

    ErrorF( "SR    x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF" );

    for( i = 0; i < 0x70; i++ ) {
	if( !(i % 16) )
	    ErrorF( "\nSR%xx ", i >> 4 );
	VGAOUT8( 0x3c4, i );
	ErrorF( " %02x", VGAIN8(0x3c5) );
    }

    ErrorF( "\n\nCR    x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF" );

    for( i = 0; i < 0xB7; i++ ) {
	if( !(i % 16) )
	    ErrorF( "\nCR%xx ", i >> 4 );
	VGAOUT8( vgaCRIndex, i );
	ErrorF( " %02x", VGAIN8(vgaCRReg) );
    }

    ErrorF("\n\n");
}


static void SavageDPMS(ScrnInfoPtr pScrn, int mode, int flags)
{
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char sr8 = 0x00, srd = 0x00;

    TRACE(("SavageDPMS(%d,%x)\n", mode, flags));

    VGAOUT8(0x3c4, 0x08);
    sr8 = VGAIN8(0x3c5);
    sr8 |= 0x06;
    VGAOUT8(0x3c5, sr8);

    VGAOUT8(0x3c4, 0x0d);
    srd = VGAIN8(0x3c5);

    srd &= 0x03;

    switch (mode) {
	case DPMSModeOn:
	    break;
	case DPMSModeStandby:
	    srd |= 0x10;
	    break;
	case DPMSModeSuspend:
	    srd |= 0x40;
	    break;
	case DPMSModeOff:
	    srd |= 0x50;
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid DPMS mode %d\n", mode);
	    break;
    }

    VGAOUT8(0x3c4, 0x0d);
    VGAOUT8(0x3c5, srd);

    return;
}


static unsigned int
SavageDDC1Read(ScrnInfoPtr pScrn)
{
    register vgaHWPtr hwp = VGAHWPTR(pScrn);
    register unsigned char tmp;
    SavagePtr psav = SAVPTR(pScrn);

    VerticalRetraceWait(psav);

    InI2CREG(psav,tmp);
    while (hwp->readST01(hwp)&0x8) {};
    while (!(hwp->readST01(hwp)&0x8)) {};

    return ((unsigned int) (tmp & 0x08));
}

static Bool
SavageDDC1(int scrnIndex)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char tmp;
    Bool success = FALSE;
    xf86MonPtr pMon;
    
    /* initialize chipset */
    InI2CREG(psav,tmp);
    OutI2CREG(psav,tmp | 0x12);
    
    if ((pMon = xf86PrintEDID(
	xf86DoEDID_DDC1(scrnIndex,vgaHWddc1SetSpeed,SavageDDC1Read))) != NULL)
	success = TRUE;
    xf86SetDDCproperties(pScrn,pMon);

    /* undo initialization */
    OutI2CREG(psav,tmp);
    return success;
}


static void
SavageProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;
    if (xf86LoadSubModule(pScrn, "vbe")) {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}


static void
SavageGetTvMaxSize(SavagePtr psav)
{
    if( psav->PAL ) {
	psav->TVSizeX = 800;
	psav->TVSizeY = 600;
    }
    else {
	psav->TVSizeX = 640;
	psav->TVSizeY = 480;
    }
}


static Bool
SavagePanningCheck(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    DisplayModePtr pMode;

    pMode = pScrn->currentMode;
    psav->iResX = pMode->CrtcHDisplay;
    psav->iResY = pMode->CrtcVDisplay;
    if( psav->iResX < pScrn->virtualX || psav->iResY < pScrn->virtualY )
	return TRUE;
    else
	return FALSE;
}


