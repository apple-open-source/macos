/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_driver.c,v 1.86 2003/02/04 02:20:50 dawes Exp $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.

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

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/
#include "xf86Resources.h"
/* Needed by Resources Access Control (RAC) */
#include "xf86RAC.h"

#include "xf86DDC.h"
#include "vbe.h"

/* Needed by the Shadow Framebuffer */
#include "shadowfb.h"

/*
 * s3v_driver.c
 * Port to 4.0 design level
 *
 * S3 ViRGE driver
 *
 * 10/98 - 3/99 Kevin Brosius
 * based largely on the SVGA ViRGE driver from 3.3.3x,
 * Started 09/03/97 by S. Marineau
 *
 *
 */


	/* Most xf86 commons are already in s3v.h */
#include	"s3v.h"
		

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

/*
 * Internals
 */
static void S3VEnableMmio(ScrnInfoPtr pScrn);
static void S3VDisableMmio(ScrnInfoPtr pScrn);

/*
 * Forward definitions for the functions that make up the driver.
 */

/* Mandatory functions */
static const OptionInfoRec * S3VAvailableOptions(int chipid, int busid);
static void S3VIdentify(int flags);
static Bool S3VProbe(DriverPtr drv, int flags);
static Bool S3VPreInit(ScrnInfoPtr pScrn, int flags);

static Bool S3VEnterVT(int scrnIndex, int flags);
static void S3VLeaveVT(int scrnIndex, int flags);
static void S3VSave (ScrnInfoPtr pScrn);
static void S3VWriteMode (ScrnInfoPtr pScrn, vgaRegPtr, S3VRegPtr);

static void S3VSaveSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams);
static void S3VRestoreSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams);
static void S3VDisableSTREAMS(ScrnInfoPtr pScrn);
static Bool S3VScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv);
static int S3VInternalScreenInit( int scrnIndex, ScreenPtr pScreen);
static void S3VPrintRegs(ScrnInfoPtr);
static ModeStatus S3VValidMode(int index, DisplayModePtr mode, Bool verbose, int flags);

static Bool S3VMapMem(ScrnInfoPtr pScrn);
static void S3VUnmapMem(ScrnInfoPtr pScrn);
static Bool S3VModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool S3VCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool S3VSaveScreen(ScreenPtr pScreen, int mode);
static void S3VInitSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams, DisplayModePtr mode);
/* s3v.h - static void S3VAdjustFrame(int scrnIndex, int x, int y, int flags); */
/* s3v.h - static Bool S3VSwitchMode(int scrnIndex, DisplayModePtr mode, int flags); */
static void S3VLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies, LOCO *colors, VisualPtr pVisual);

static void S3VDisplayPowerManagementSet(ScrnInfoPtr pScrn,
					 int PowerManagementMode,
					 int flags);
static Bool S3Vddc1(int scrnIndex);
static Bool S3Vddc2(int scrnIndex);

static unsigned int S3Vddc1Read(ScrnInfoPtr pScrn);
static void S3VProbeDDC(ScrnInfoPtr pScrn, int index);

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;
 
#define S3VIRGE_NAME "S3VIRGE"
#define S3VIRGE_DRIVER_NAME "s3virge"
#define S3VIRGE_VERSION_NAME "1.8.6"
#define S3VIRGE_VERSION_MAJOR   1
#define S3VIRGE_VERSION_MINOR   8
#define S3VIRGE_PATCHLEVEL      6
#define S3VIRGE_DRIVER_VERSION ((S3VIRGE_VERSION_MAJOR << 24) | \
				(S3VIRGE_VERSION_MINOR << 16) | \
				S3VIRGE_PATCHLEVEL)

/* 
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec S3VIRGE =
{
    S3VIRGE_DRIVER_VERSION,
    S3VIRGE_DRIVER_NAME,
    S3VIdentify,
    S3VProbe,
    S3VAvailableOptions,
    NULL,
    0
};


/* Supported chipsets */
static SymTabRec S3VChipsets[] = {
				  	/* base (86C325) */
  { PCI_CHIP_VIRGE,			"virge" },
  { PCI_CHIP_VIRGE,			"86C325" },
  					/* VX (86C988) */
  { PCI_CHIP_VIRGE_VX,		"virge vx" },
  { PCI_CHIP_VIRGE_VX,		"86C988" },
  					/* DX (86C375) GX (86C385) */
  { PCI_CHIP_VIRGE_DXGX,	"virge dx" },
  { PCI_CHIP_VIRGE_DXGX,	"virge gx" },
  { PCI_CHIP_VIRGE_DXGX,	"86C375" },
  { PCI_CHIP_VIRGE_DXGX,	"86C385" },
                                        /* GX2 (86C357) */
  { PCI_CHIP_VIRGE_GX2,		"virge gx2" },
  { PCI_CHIP_VIRGE_GX2,		"86C357" },
  					/* MX (86C260) */
  { PCI_CHIP_VIRGE_MX,		"virge mx" },
  { PCI_CHIP_VIRGE_MX,		"86C260" },
  					/* MX+ (86C280) */
  { PCI_CHIP_VIRGE_MXP,		"virge mx+" },
  { PCI_CHIP_VIRGE_MXP,		"86C280" },
  					/* Trio3D (86C365) */
  { PCI_CHIP_Trio3D,		"trio 3d" },
  { PCI_CHIP_Trio3D,		"86C365" },
  					/* Trio3D/2x (86C362/86C368) */
  { PCI_CHIP_Trio3D_2X,		"trio 3d/2x" },
  { PCI_CHIP_Trio3D_2X,		"86C362" },
  { PCI_CHIP_Trio3D_2X,		"86C368" },
  {-1,			NULL }
};

static PciChipsets S3VPciChipsets[] = {
  /* numChipset,		PciID,			Resource */
  { PCI_CHIP_VIRGE,      PCI_CHIP_VIRGE,     	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_VX,   PCI_CHIP_VIRGE_VX,     RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_DXGX, PCI_CHIP_VIRGE_DXGX,	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_GX2,  PCI_CHIP_VIRGE_GX2,  	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_MX,   PCI_CHIP_VIRGE_MX,   	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_MXP,  PCI_CHIP_VIRGE_MXP,  	RES_SHARED_VGA },
  { PCI_CHIP_Trio3D,     PCI_CHIP_Trio3D,  	RES_SHARED_VGA },
  { PCI_CHIP_Trio3D_2X,  PCI_CHIP_Trio3D_2X,  	RES_SHARED_VGA },
  { -1,                       -1,   		RES_UNDEFINED }
};

typedef enum {		    
   OPTION_SLOW_EDODRAM, 	
   OPTION_SLOW_DRAM,
   OPTION_FAST_DRAM, 		
   OPTION_FPM_VRAM, 		
   OPTION_PCI_BURST, 	
   OPTION_FIFO_CONSERV, 	
   OPTION_FIFO_MODERATE, 	
   OPTION_FIFO_AGGRESSIVE, 	
   OPTION_PCI_RETRY, 		
   OPTION_NOACCEL, 		
   OPTION_EARLY_RAS_PRECHARGE, 	
   OPTION_LATE_RAS_PRECHARGE,
   OPTION_LCD_CENTER,
   OPTION_LCDCLOCK,
   OPTION_MCLK,
   OPTION_REFCLK,
   OPTION_SHOWCACHE,
   OPTION_SWCURSOR,
   OPTION_HWCURSOR,
   OPTION_SHADOW_FB,
   OPTION_ROTATE,
   OPTION_FB_DRAW,
   OPTION_MX_CR3A_FIX,
   OPTION_XVIDEO
} S3VOpts;

static const OptionInfoRec S3VOptions[] =
{  
  /*    int token, const char* name, OptionValueType type,
	ValueUnion value, Bool found.
  */
   { OPTION_SLOW_EDODRAM, 	"slow_edodram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_SLOW_DRAM, 		"slow_dram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_FAST_DRAM, 		"fast_dram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_FPM_VRAM, 		"fpm_vram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_PCI_BURST, 		"pci_burst",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_FIFO_CONSERV, 	"fifo_conservative", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_FIFO_MODERATE, 	"fifo_moderate", OPTV_BOOLEAN, 	{0}, FALSE },
   { OPTION_FIFO_AGGRESSIVE, 	"fifo_aggressive", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_PCI_RETRY, 		"pci_retry",	OPTV_BOOLEAN,	{0}, FALSE  },
   { OPTION_NOACCEL, 		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE  },
   { OPTION_EARLY_RAS_PRECHARGE, "early_ras_precharge",	OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_LATE_RAS_PRECHARGE, "late_ras_precharge", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_LCD_CENTER, 	"lcd_center", 	OPTV_BOOLEAN, 	{0}, FALSE },
   { OPTION_LCDCLOCK, 		"set_lcdclk", 	OPTV_INTEGER, 	{0}, FALSE },
   { OPTION_MCLK, 		"set_mclk", 	OPTV_FREQ, 	{0}, FALSE },
   { OPTION_REFCLK, 		"set_refclk", 	OPTV_FREQ, 	{0}, FALSE },
   { OPTION_SHOWCACHE,		"show_cache",   OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_HWCURSOR,		"HWCursor",     OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_SWCURSOR,		"SWCursor",     OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_SHADOW_FB,          "ShadowFB",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_ROTATE, 	        "Rotate",	OPTV_ANYSTR,	{0}, FALSE },
   { OPTION_FB_DRAW,            "UseFB",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_MX_CR3A_FIX,        "mxcr3afix",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_XVIDEO,             "XVideo",	OPTV_BOOLEAN,	{0}, FALSE },
   {-1, NULL, OPTV_NONE,	{0}, FALSE}
};


/*
 * Lists of symbols that may/may not be required by this driver.
 * This allows the loader to know which ones to issue warnings for.
 *
 * Note that vgahwSymbols and xaaSymbols are referenced outside the
 * XFree86LOADER define in later code, so are defined outside of that
 * define here also.
 */

static const char *vgahwSymbols[] = {
    "vgaHWBlankScreen",
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
   /* not used by ViRGE (at the moment :( ) */
   /*
    "vgaHWUnlock",
    "vgaHWFreeHWRec",
    */
    NULL
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACopyROP_PM",
    "XAADestroyInfoRec",
    "XAACreateInfoRec",
    "XAAFillSolidRects",
    "XAAHelpPatternROP",
    "XAAHelpSolidROP",
    "XAAInit",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86InitCursor",
#if 0
    "xf86DestroyCursorInfoRec",
#endif
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    "xf86SetDDCproperties",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

static const char *fbSymbols[] = {
  "fbPictureInit",
  "fbScreenInit",
  NULL
};

static const char *int10Symbols[] = {
    "xf86InitInt10",
    "xf86FreeInt10",
    NULL
};

static const char *cfbSymbols[] = {
    "cfbScreenInit",
    "cfb16ScreenInit",
    "cfb24ScreenInit",
    "cfb24_32ScreenInit",
    "cfb32ScreenInit",
    "cfBresS",
    "cfb16BresS",
    "cfb24BresS",
    "cfb32BresS",
    NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(s3virgeSetup);

static XF86ModuleVersionInfo S3VVersRec =
{
    "s3virge",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    S3VIRGE_VERSION_MAJOR, S3VIRGE_VERSION_MINOR, S3VIRGE_PATCHLEVEL,
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
XF86ModuleData s3virgeModuleData = { &S3VVersRec, s3virgeSetup, NULL };

static pointer
s3virgeSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&S3VIRGE, module, 0);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * Tell the loader about symbols from other modules that this module
	 * might refer to.
	 */
	LoaderRefSymLists(vgahwSymbols, cfbSymbols, xaaSymbols,
			  ramdacSymbols, ddcSymbols, i2cSymbols,
			  int10Symbols, vbeSymbols, shadowSymbols, 
			  fbSymbols, NULL);
			  
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


static unsigned char *find_bios_string(PCITAG Tag, int BIOSbase, char *match1, char *match2)
{
#define BIOS_BSIZE 1024
#define BIOS_BASE  0xc0000

   static unsigned char bios[BIOS_BSIZE];
   static int init=0;
   int i,j,l1,l2;

   if (!init) {
      init = 1;
      if (xf86ReadDomainMemory(Tag, BIOSbase, BIOS_BSIZE, bios) != BIOS_BSIZE)
	 return NULL;
      if ((bios[0] != 0x55) || (bios[1] != 0xaa))
	 return NULL;
   }
   if (match1 == NULL)
      return NULL;

   l1 = strlen(match1);
   if (match2 != NULL) 
      l2 = strlen(match2);
   else	/* for compiler-warnings */
      l2 = 0;

   for (i=0; i<BIOS_BSIZE-l1; i++)
       if (bios[i] == match1[0] && !memcmp(&bios[i],match1,l1)) {
	 if (match2 == NULL) 
	    return &bios[i+l1];
	 else
	    for(j=i+l1; (j<BIOS_BSIZE-l2) && bios[j]; j++) 
	       if (bios[j] == match2[0] && !memcmp(&bios[j],match2,l2))
		   return &bios[j+l2];
       }
   
   return NULL;
}


static Bool
S3VGetRec(ScrnInfoPtr pScrn)
{
    PVERB5("	S3VGetRec\n");
    /*
     * Allocate an 'Chip'Rec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(S3VRec), 1);
    /* Initialise it here when needed (or possible) */

    return TRUE;
}

static void
S3VFreeRec(ScrnInfoPtr pScrn)
{
    PVERB5("	S3VFreeRec\n");
    if (pScrn->driverPrivate == NULL)
	return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static const OptionInfoRec *
S3VAvailableOptions(int chipid, int busid)
{
    return S3VOptions;
}

static void
S3VIdentify(int flags)
{
    PVERB5("	S3VIdentify\n");
    xf86PrintChipsets(S3VIRGE_NAME, 
	"driver (version " S3VIRGE_VERSION_NAME ") for S3 ViRGE chipsets",
	S3VChipsets);
}


static Bool
S3VProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;
    
    PVERB5("	S3VProbe begin\n");

    if ((numDevSections = xf86MatchDevice(S3VIRGE_DRIVER_NAME,
					  &devSections)) <= 0) {
	/*
	 * There's no matching device section in the config file, so quit
	 * now.
	 */
	return FALSE;
    }
    if (xf86GetPciVideoInfo() == NULL) {
	return FALSE;
    }

    numUsed = xf86MatchPciInstances(S3VIRGE_NAME, PCI_S3_VENDOR_ID,
				    S3VChipsets, S3VPciChipsets, devSections,
				    numDevSections, drv, &usedChips);
    
    /* Free it since we don't need that list after this */
    xfree(devSections);
    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
	/* Allocate a ScrnInfoRec and claim the slot */
	ScrnInfoPtr pScrn = NULL;
	if ((pScrn = xf86ConfigPciEntity(pScrn,0,usedChips[i],
					       S3VPciChipsets,NULL,NULL, NULL,
					       NULL,NULL))) {
	    /* Fill in what we can of the ScrnInfoRec */
	    pScrn->driverVersion = S3VIRGE_DRIVER_VERSION;
	    pScrn->driverName	 = S3VIRGE_DRIVER_NAME;
	    pScrn->name		 = S3VIRGE_NAME;
	    pScrn->Probe	 = S3VProbe;
	    pScrn->PreInit	 = S3VPreInit;
	    pScrn->ScreenInit	 = S3VScreenInit;
	    pScrn->SwitchMode	 = S3VSwitchMode;
	    pScrn->AdjustFrame	 = S3VAdjustFrame;
	    pScrn->EnterVT	 = S3VEnterVT;
	    pScrn->LeaveVT	 = S3VLeaveVT;
	    pScrn->FreeScreen	 = NULL; /*S3VFreeScreen;*/
	    pScrn->ValidMode	 = S3VValidMode;
	    foundScreen = TRUE;
	}
    }
    xfree(usedChips);
    PVERB5("	S3VProbe end\n");
    return foundScreen;
}


/* Mandatory */
static Bool
S3VPreInit(ScrnInfoPtr pScrn, int flags)
{
    EntityInfoPtr pEnt;
    S3VPtr ps3v;
    MessageType from;
    int i;
    double real;
    ClockRangePtr clockRanges;
    char *mod = NULL;
    const char *reqSym = NULL;
    char *s;
    
    unsigned char config1, config2, m, n, n1, n2, cr66 = 0;
    int mclk;
    
    vgaHWPtr hwp;
    int vgaCRIndex, vgaCRReg, vgaIOBase;
   
    PVERB5("	S3VPreInit 1\n");

    if (flags & PROBE_DETECT) {
	  S3VProbeDDC( pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index );
      return TRUE;
      }
    	      
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
    
    if (!xf86LoadSubModule(pScrn, "vgahw"))
	return FALSE;
	   
    xf86LoaderReqSymLists(vgahwSymbols, NULL);
	
    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn))
	return FALSE;
    

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * Our default depth is 8, so pass it to the helper function.
     * We support both 24bpp and 32bpp layouts, so indicate that.
     */
    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, Support24bppFb | Support32bppFb |
				SupportConvert32to24 | PreferConvert32to24)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	case 15:
	case 16:
	case 24:
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
    } else {		/* editme - from MGA, does ViRGE? */
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

    /* Allocate the S3VRec driverPrivate */
    if (!S3VGetRec(pScrn)) {
	return FALSE;
    }
    ps3v = S3VPTR(pScrn);

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);
	       
    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8) {
    				/* ViRGE supports 6 RGB bits in depth 8 */
				/* modes (with 256 entry LUT) */
      pScrn->rgbBits = 6;
    }
    
    /* Process the options */
    if (!(ps3v->Options = xalloc(sizeof(S3VOptions))))
	return FALSE;
    memcpy(ps3v->Options, S3VOptions, sizeof(S3VOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, ps3v->Options);


    if (xf86ReturnOptValBool(ps3v->Options, OPTION_PCI_BURST, FALSE)) {
	ps3v->pci_burst = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_burst - PCI burst read enabled\n");
    } else
   	ps3v->pci_burst = FALSE;
					/* default */
    ps3v->NoPCIRetry = 1;
   					/* Set option */
    if (xf86ReturnOptValBool(ps3v->Options, OPTION_PCI_RETRY, FALSE)) {
      if (xf86ReturnOptValBool(ps3v->Options, OPTION_PCI_BURST, FALSE)) {
      	ps3v->NoPCIRetry = 0;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_retry\n");
	}
      else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"\"pci_retry\" option requires \"pci_burst\".\n");
	}
    }
    if (xf86IsOptionSet(ps3v->Options, OPTION_FIFO_CONSERV)) {
	ps3v->fifo_conservative = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_conservative set\n");
    } else
   	ps3v->fifo_conservative = FALSE;

    if (xf86IsOptionSet(ps3v->Options, OPTION_FIFO_MODERATE)) {
	ps3v->fifo_moderate = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_moderate set\n");
    } else
   	ps3v->fifo_moderate = FALSE;

    if (xf86IsOptionSet(ps3v->Options, OPTION_FIFO_AGGRESSIVE)) {
	ps3v->fifo_aggressive = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_aggressive set\n");
    } else
   	ps3v->fifo_aggressive = FALSE;

    if (xf86IsOptionSet(ps3v->Options, OPTION_SLOW_EDODRAM)) {
	ps3v->slow_edodram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: slow_edodram set\n");
    } else
   	ps3v->slow_edodram = FALSE;

    if (xf86IsOptionSet(ps3v->Options, OPTION_SLOW_DRAM)) {
	ps3v->slow_dram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: slow_dram set\n");
    } else
   	ps3v->slow_dram = FALSE;

    if (xf86IsOptionSet(ps3v->Options, OPTION_FAST_DRAM)) {
	ps3v->fast_dram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fast_dram set\n");
    } else
   	ps3v->fast_dram = FALSE;

    if (xf86IsOptionSet(ps3v->Options, OPTION_FPM_VRAM)) {
	ps3v->fpm_vram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fpm_vram set\n");
    } else
   	ps3v->fpm_vram = FALSE;

    if (xf86ReturnOptValBool(ps3v->Options, OPTION_NOACCEL, FALSE)) {
	ps3v->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: NoAccel - Acceleration disabled\n");
    } else
   	ps3v->NoAccel = FALSE;

    if (xf86ReturnOptValBool(ps3v->Options, OPTION_EARLY_RAS_PRECHARGE, FALSE)) {
	ps3v->early_ras_precharge = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: early_ras_precharge set\n");
    } else
   	ps3v->early_ras_precharge = FALSE;

    if (xf86ReturnOptValBool(ps3v->Options, OPTION_LATE_RAS_PRECHARGE, FALSE)) {
	ps3v->late_ras_precharge = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: late_ras_precharge set\n");
    } else
   	ps3v->late_ras_precharge = FALSE;
	       	   
    if (xf86ReturnOptValBool(ps3v->Options, OPTION_LCD_CENTER, FALSE)) {
	ps3v->lcd_center = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: lcd_center set\n");
    } else
   	ps3v->lcd_center = FALSE;

    if (xf86ReturnOptValBool(ps3v->Options, OPTION_SHOWCACHE, FALSE)) {
	ps3v->ShowCache = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: show_cache set\n");
    } else
   	ps3v->ShowCache = FALSE;

    if (xf86GetOptValInteger(ps3v->Options, OPTION_LCDCLOCK, &ps3v->LCDClk)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: lcd_setclk set to %1.3f Mhz\n",
		ps3v->LCDClk / 1000.0 );
    } else
   	ps3v->LCDClk = 0;
	
    if (xf86GetOptValFreq(ps3v->Options, OPTION_MCLK, OPTUNITS_MHZ, &real)) {
	ps3v->MCLK = (int)(real * 1000.0);
    	if (ps3v->MCLK <= 100000) {
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: set_mclk set to %1.3f Mhz\n",
		ps3v->MCLK / 1000.0 );
	} else {
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING
	  	, "Memory Clock value of %1.3f MHz is larger than limit of 100 MHz\n"
		, ps3v->MCLK/1000.0);
	  ps3v->MCLK = 0;
	}
    } else
   	ps3v->MCLK = 0;

    if (xf86GetOptValFreq(ps3v->Options, OPTION_REFCLK, OPTUNITS_MHZ, &real)) {
	ps3v->REFCLK = (int)(real * 1000.0);
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: set_refclk set to %1.3f Mhz\n",
		   ps3v->REFCLK / 1000.0 );
    } else
   	ps3v->REFCLK = 0;

    from = X_DEFAULT;
    ps3v->hwcursor = TRUE;
    if (xf86GetOptValBool(ps3v->Options, OPTION_HWCURSOR, &ps3v->hwcursor))
	  from = X_CONFIG;
    if (xf86ReturnOptValBool(ps3v->Options, OPTION_SWCURSOR, FALSE)) {
	  ps3v->hwcursor = FALSE;
	  from = X_CONFIG;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s Cursor\n",
		ps3v->hwcursor ? "HW" : "SW");

    if (xf86GetOptValBool(ps3v->Options, OPTION_SHADOW_FB,&ps3v->shadowFB))
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShadowFB %s.\n",
		   ps3v->shadowFB ? "enabled" : "disabled");

    if ((s = xf86GetOptValString(ps3v->Options, OPTION_ROTATE))) {
	if(!xf86NameCmp(s, "CW")) {
	    /* accel is disabled below for shadowFB */
	    ps3v->shadowFB = TRUE;
	    ps3v->rotate = 1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		       "Rotating screen clockwise - acceleration disabled\n");
	} else if(!xf86NameCmp(s, "CCW")) {
	    ps3v->shadowFB = TRUE;
	    ps3v->rotate = -1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
		       "counter clockwise - acceleration disabled\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
		       "value for Option \"Rotate\"\n", s);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		       "Valid options are \"CW\" or \"CCW\"\n");
	}
    }
    
    if (ps3v->shadowFB && !ps3v->NoAccel) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "HW acceleration not supported with \"shadowFB\".\n");
	ps3v->NoAccel = TRUE;
    }

    if (ps3v->rotate && ps3v->hwcursor) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "HW cursor not supported with \"rotate\".\n");
	ps3v->hwcursor = FALSE;
    }

    if (xf86IsOptionSet(ps3v->Options, OPTION_FB_DRAW)) 
      {
	if (xf86GetOptValBool(ps3v->Options, OPTION_FB_DRAW ,&ps3v->UseFB))
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using %s.\n",
		     ps3v->UseFB ? "fb (not cfb)" : "cfb (not fb)");
      }
    else
      {
	ps3v->UseFB = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Using fb.\n");
      }

    if (xf86IsOptionSet(ps3v->Options, OPTION_MX_CR3A_FIX)) 
      {
	if (xf86GetOptValBool(ps3v->Options, OPTION_MX_CR3A_FIX ,&ps3v->mx_cr3a_fix))
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "%s mx_cr3a_fix.\n",
		     ps3v->mx_cr3a_fix ? "Enabling (default)" : "Disabling");
      }
    else
      {
	ps3v->mx_cr3a_fix = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "mx_cr3a_fix.\n");
      }

    /* Find the PCI slot for this screen */
    /*
     * XXX Ignoring the Type list for now.  It might be needed when
     * multiple cards are supported.
     */
    if (pScrn->numEntities > 1) {
	S3VFreeRec(pScrn);
	return FALSE;
    }
    
    pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    
    if (pEnt->resources) {
	xfree(pEnt);
	S3VFreeRec(pScrn);
	return FALSE;
    }

#if 0
    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86Int10InfoPtr pInt;
 	xf86LoaderReqSymLists(int10Symbols, NULL);
#if 1
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,"initializing int10\n");
	pInt = xf86InitInt10(pEnt->index);
	xf86FreeInt10(pInt);
#endif
    }
#endif
    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86LoaderReqSymLists(vbeSymbols, NULL);
	ps3v->pVbe =  VBEInit(NULL,pEnt->index);
    }

    ps3v->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
    xf86RegisterResources(pEnt->index,NULL,ResNone);
    xf86SetOperatingState(resVgaIo, pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMem, pEnt->index, ResDisableOpr);

    /*
     * Set the Chipset and ChipRev, allowing config file entries to
     * override.
     */
    if (pEnt->device->chipset && *pEnt->device->chipset) {
	pScrn->chipset = pEnt->device->chipset;
        ps3v->Chipset = xf86StringToToken(S3VChipsets, pScrn->chipset);
        from = X_CONFIG;
    } else if (pEnt->device->chipID >= 0) {
	ps3v->Chipset = pEnt->device->chipID;
	pScrn->chipset = (char *)xf86TokenToString(S3VChipsets, ps3v->Chipset);
	from = X_CONFIG;
  	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
		   ps3v->Chipset);
    } else {
	from = X_PROBED;
	ps3v->Chipset = ps3v->PciInfo->chipType;
	pScrn->chipset = (char *)xf86TokenToString(S3VChipsets, ps3v->Chipset);
    }								    
    
    if (pEnt->device->chipRev >= 0) {
	ps3v->ChipRev = pEnt->device->chipRev;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
		   ps3v->ChipRev);
    } else {
	ps3v->ChipRev = ps3v->PciInfo->chipRev;
    }
    xfree(pEnt);
    
    /*
     * This shouldn't happen because such problems should be caught in
     * S3VProbe(), but check it just in case.
     */
    if (pScrn->chipset == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", ps3v->Chipset);
	vbeFree(ps3v->pVbe);
	ps3v->pVbe = NULL;
	return FALSE;
    }
    if (ps3v->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
	vbeFree(ps3v->pVbe);
	ps3v->pVbe = NULL;
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);
	
    ps3v->PciTag = pciTag(ps3v->PciInfo->bus, ps3v->PciInfo->device,
			  ps3v->PciInfo->func);

    /* Handle XVideo after we know chipset, so we can give an */
    /* intelligent comment about support */
    if (xf86IsOptionSet(ps3v->Options, OPTION_XVIDEO)) 
      {
	if(S3VQueryXvCapable(pScrn))
	  {
	    if (xf86GetOptValBool(ps3v->Options, OPTION_XVIDEO ,&ps3v->XVideo))
	      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "%s XVideo.\n",
			 ps3v->XVideo ? "Enabling (default)" : "Disabling");
	  }
	else
	  xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "XVideo not supported.\n");
      }
    else
      {
	ps3v->XVideo = S3VQueryXvCapable(pScrn);
	if(ps3v->XVideo)
	  xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "XVideo supported.\n");
	else
	  xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "XVideo not supported.\n");
      }

			  
  S3VMapMem(pScrn);
  hwp = VGAHWPTR(pScrn);
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

    xf86ErrorFVerb(VERBLEV, 
	"	S3VPreInit vgaCRIndex=%x, vgaIOBase=%x, MMIOBase=%x\n", 
	vgaCRIndex, vgaIOBase, hwp->MMIOBase );


#if 0	/* Not needed in 4.0 flavors */
   /* Unlock sys regs */
   VGAOUT8(vgaCRIndex, 0x38);
   VGAOUT8(vgaCRReg, 0x48);
#endif

   /* Next go on to detect amount of installed ram */

   VGAOUT8(vgaCRIndex, 0x36);           /* for register CR36 (CONFG_REG1),*/
   config1 = VGAIN8(vgaCRReg);          /* get amount of vram installed   */

   VGAOUT8(vgaCRIndex, 0x37);           /* for register CR37 (CONFG_REG2),*/
   config2 = VGAIN8(vgaCRReg);          /* get amount of off-screen ram   */

   if (xf86LoadSubModule(pScrn, "ddc")) {
       xf86MonPtr pMon = NULL;
       
       xf86LoaderReqSymLists(ddcSymbols, NULL);
       if ((ps3v->pVbe) 
	   && ((pMon = xf86PrintEDID(vbeDoEDID(ps3v->pVbe, NULL))) != NULL))
	   xf86SetDDCproperties(pScrn,pMon);
       else if (!S3Vddc1(pScrn->scrnIndex)) {
	   S3Vddc2(pScrn->scrnIndex);
       }
   }
   if (ps3v->pVbe) {
       vbeFree(ps3v->pVbe);
       ps3v->pVbe = NULL;
   }
   
   /*
    * If the driver can do gamma correction, it should call xf86SetGamma()
    * here. (from MGA, no ViRGE gamma support yet, but needed for 
    * xf86HandleColormaps support.)
    */
   {
       Gamma zeros = {0.0, 0.0, 0.0};
       
       if (!xf86SetGamma(pScrn, zeros)) {
	   return FALSE;
       }
   }
   
   /* And compute the amount of video memory and offscreen memory */
   ps3v->MemOffScreen = 0;

   if (!pScrn->videoRam) {
      if (ps3v->Chipset == S3_ViRGE_VX) {
	  switch((config2 & 0x60) >> 5) {
         case 1:
            ps3v->MemOffScreen = 4 * 1024;
            break;
         case 2:
            ps3v->MemOffScreen = 2 * 1024;
            break;
         }
         switch ((config1 & 0x60) >> 5) {
         case 0:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         case 1:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 2:
            ps3v->videoRamKbytes = 6 * 1024;
            break;
         case 3:
            ps3v->videoRamKbytes = 8 * 1024;
            break;
         }
         ps3v->videoRamKbytes -= ps3v->MemOffScreen;
      }
      else if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset)) {
         switch((config1 & 0xE0) >> 5) {
         case 0:  /* 8MB -- only 4MB usable for display/cursor */
            ps3v->videoRamKbytes = 4 * 1024;
            ps3v->MemOffScreen   = 4 * 1024;
            break;
         case 1:    /* 32 bit interface -- yuck */
	   xf86ErrorFVerb(VERBLEV, 
			  "	found 32 bit interface for video memory -- yuck:(\n");
         case 2:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 6:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         }
      }
      else if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
        switch((config1 & 0xE0) >> 5) {
        case 0:
        case 2:
           ps3v->videoRamKbytes = 4 * 1024;
           break;
        case 4:
           ps3v->videoRamKbytes = 2 * 1024;
           break;
        }
      }
      else if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
	  switch((config1 & 0xC0) >> 6) {
	  case 1:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 3:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         }
      }
      else {
         switch((config1 & 0xE0) >> 5) {
         case 0:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 4:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         case 6:
            ps3v->videoRamKbytes = 1 * 1024;
            break;
         }
      }
      					/* And save a byte value also */
      ps3v->videoRambytes = ps3v->videoRamKbytes * 1024;
      				       	/* Make sure the screen also */
					/* has correct videoRam setting */
      pScrn->videoRam = ps3v->videoRamKbytes;

      if (ps3v->MemOffScreen)
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	    	"videoram:  %dk (plus %dk off-screen)\n",
                ps3v->videoRamKbytes, ps3v->MemOffScreen);
      else
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "videoram:  %dk\n",
                ps3v->videoRamKbytes);
   } else {
   					/* Note: if ram is not probed then */
					/* ps3v->videoRamKbytes will not be init'd */
					/* should we? can do it here... */
					
					
      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "videoram:  %dk\n",
              	ps3v->videoRamKbytes);
   }

   /* reset S3 graphics engine to avoid memory corruption */
   if (ps3v->Chipset != S3_ViRGE_VX) {
      VGAOUT8(vgaCRIndex, 0x66);
      cr66 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRReg, cr66 | 0x02);
      usleep(10000);  /* wait a little bit... */
   }

   if (find_bios_string(ps3v->PciTag, BIOS_BASE, "S3 86C325",
			"MELCO WGP-VG VIDEO BIOS") != NULL) {
      if (xf86GetVerbosity())
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "MELCO BIOS found\n");
      if (ps3v->MCLK <= 0)      ps3v->MCLK      =  74000;
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 191500;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] = 162500;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] = 111500;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] =  83500;
   }

   if (ps3v->Chipset != S3_ViRGE_VX) {
      VGAOUT8(vgaCRIndex, 0x66);
      VGAOUT8(vgaCRReg, cr66 & ~0x02);  /* clear reset flag */
      usleep(10000);  /* wait a little bit... */
   }

   /* ViRGE built-in ramdac speeds */
   					/* ViRGE has four default clocks */
   pScrn->numClocks = 4;
   
   if (pScrn->clock[3] <= 0 && pScrn->clock[2] > 0)
      pScrn->clock[3] = pScrn->clock[2];

   if (ps3v->Chipset == S3_ViRGE_VX) {
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 220000;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] = 220000;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] = 135000;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] = 135000;
   }
   else if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset)) {
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 230000;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] = 230000;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] = 135000;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] = 135000;
   }
   else if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset)) {
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 170000;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] = 170000;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] = 135000;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] = 135000;
   }
   else if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 135000;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] = 135000;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] = 100000;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] = 100000;
   }
   else if(S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 230000;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] = 230000;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] = 135000;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] = 135000;
   }
   else {
      if (pScrn->clock[0] <= 0) pScrn->clock[0] = 135000;
      if (pScrn->clock[1] <= 0) pScrn->clock[1] =  95000;
      if (pScrn->clock[2] <= 0) pScrn->clock[2] =  57000;
      if (pScrn->clock[3] <= 0) pScrn->clock[3] =  57000;
   }
   
   if (ps3v->dacSpeedBpp <= 0) {
      if (pScrn->bitsPerPixel > 24 && pScrn->clock[3] > 0)
	 ps3v->dacSpeedBpp = pScrn->clock[3];
      else if (pScrn->bitsPerPixel >= 24 && pScrn->clock[2] > 0)
	 ps3v->dacSpeedBpp = pScrn->clock[2];
      else if (pScrn->bitsPerPixel > 8 && pScrn->bitsPerPixel < 24 && pScrn->clock[1] > 0)
	 ps3v->dacSpeedBpp = pScrn->clock[1];
      else if (pScrn->bitsPerPixel <= 8 && pScrn->clock[0] > 0)
	 ps3v->dacSpeedBpp = pScrn->clock[0];
   } 
/*cep*/
#if 0
   if (xf86Verbosity()) {
      xf86ErrorFVerb(VERBLEV, "%s %s: Ramdac speed: %d MHz",
	     OFLG_ISSET(XCONFIG_DACSPEED, &vga256InfoRec.xconfigFlag) ?
	     XCONFIG_GIVEN : XCONFIG_PROBED, vga256InfoRec.name,
	     pScrn->clock[0] / 1000);
      if (ps3v->dacSpeedBpp != pScrn->clock[0])
	 xf86ErrorFVerb(VERBLEV, "  (%d MHz for %d bpp)",ps3v->dacSpeedBpp / 1000, pScrn->bitsPerPixel);
      xf86ErrorFVerb(VERBLEV, "\n");
   }
#endif

   /* Now set RAMDAC limits */
   /*ps3v->maxClock = ps3v->dacSpeedBpp;*/
  if (ps3v->Chipset == S3_ViRGE_VX ) {
    ps3v->maxClock = 440000;
  } else {
    ps3v->maxClock = 270000;
  }
    
   /* Detect current MCLK and print it for user */
   VGAOUT8(0x3c4, 0x08);
   VGAOUT8(0x3c5, 0x06); 
   VGAOUT8(0x3c4, 0x10);
   n = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x11);
   m = VGAIN8(0x3c5);
   m &= 0x7f;
   n1 = n & 0x1f;
   n2 = (n>>5) & 0x03;
   mclk = ((1431818 * (m+2)) / (n1+2) / (1 << n2) + 50) / 100;
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      MessageType is_probed = X_PROBED;
      /* 
       * try to figure out which reference clock is used:
       * Toshiba Tecra 5x0/7x0 seems to use 28.636 MHz
       * Compaq Armada 7x00 uses 14.318 MHz
       */
      if (find_bios_string(ps3v->PciTag, BIOS_BASE, "COMPAQ M5 BIOS", NULL) != NULL) {
	 if (xf86GetVerbosity())
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "COMPAQ M5 BIOS found\n");
	 /* ps3v->refclk_fact = 1.0; */
      }
      else if (find_bios_string(ps3v->PciTag, BIOS_BASE, "TOSHIBA Video BIOS", NULL) != NULL) {
	 if (xf86GetVerbosity())
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "TOSHIBA Video BIOS found\n");
	 /* ps3v->refclk_fact = 2.0; */
      }
      /* else */ {  /* always use guessed value... */
	 if (mclk > 60000) 
	    ps3v->refclk_fact = 1.0;
	 else
	    ps3v->refclk_fact = 2.0;  /* don't know why ??? */
      }
      if (ps3v->REFCLK != 0) {
	 ps3v->refclk_fact = ps3v->REFCLK / 14318.0;
	 is_probed = X_CONFIG;
      }
      else
	 ps3v->REFCLK = (int)(14318.18 * ps3v->refclk_fact);

      mclk = (int)(mclk * ps3v->refclk_fact);
      xf86DrvMsg(pScrn->scrnIndex, is_probed, "assuming RefCLK value of %1.3f MHz\n",
		 ps3v->REFCLK / 1000.0);
   }
   xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected current MCLK value of %1.3f MHz\n",
	     mclk / 1000.0);

   if (S3_ViRGE_MX_SERIES(ps3v->Chipset) && xf86GetVerbosity()) {
       int lcdclk, h_lcd, v_lcd;
       if (ps3v->LCDClk) {
	  lcdclk = ps3v->LCDClk;
       } else {
	  unsigned char sr12, sr13, sr29;
          VGAOUT8(0x3c4, 0x12);
          sr12 = VGAIN8(0x3c5);
          VGAOUT8(0x3c4, 0x13);
          sr13 = VGAIN8(0x3c5) & 0x7f;
          VGAOUT8(0x3c4, 0x29);
          sr29 = VGAIN8(0x3c5);
    	  n1 = sr12 & 0x1f;
    	  n2 = ((sr12>>6) & 0x03) | ((sr29 & 0x01) << 2);
          lcdclk = ((int)(ps3v->refclk_fact * 1431818 * (sr13+2)) / (n1+2) / (1 << n2) + 50) / 100;
       }
       VGAOUT8(0x3c4, 0x61);
       h_lcd = VGAIN8(0x3c5);
       VGAOUT8(0x3c4, 0x66);
       h_lcd |= ((VGAIN8(0x3c5) & 0x02) << 7);
       h_lcd = (h_lcd+1) * 8;
       VGAOUT8(0x3c4, 0x69);
       v_lcd = VGAIN8(0x3c5);
       VGAOUT8(0x3c4, 0x6e);
       v_lcd |= ((VGAIN8(0x3c5) & 0x70) << 4);
       v_lcd++;
       xf86DrvMsg(pScrn->scrnIndex
	      , ps3v->LCDClk ? X_CONFIG : X_PROBED
       	      , "LCD size %dx%d, clock %1.3f MHz\n"
	      , h_lcd, v_lcd
	      , lcdclk / 1000.0);
   }

   S3VUnmapMem(pScrn);

   /* And finally set various possible option flags */

   ps3v->bankedMono = FALSE;


#if 0
   vga256InfoRec.directMode = XF86DGADirectPresent;
#endif


#if 0
  if (ps3v->Chipset == S3_ViRGE_VX ) {
    ps3v->minClock = 220000;
  } else {
    ps3v->minClock = 135000;
  }
#else
    ps3v->minClock = 10000;  /* cep */
#endif
  
    xf86ErrorFVerb(VERBLEV, 
	"	S3VPreInit minClock=%d, maxClock=%d\n",
		ps3v->minClock,
		ps3v->maxClock
		 );
  
    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  
     */

    /* todo -  The virge limit is 2048 vertical & horizontal */
    /* pixels, not clock register settings. */
			 	/* true for all ViRGE? */
  pScrn->maxHValue = 2048;
  pScrn->maxVValue = 2048;

    				/* Lower depths default to config file */
  pScrn->virtualX = pScrn->display->virtualX;
				/* Adjust the virtualX to meet ViRGE hardware */
				/* limits for depth 24, bpp 24 & 32.  This is */
				/* mostly for 32 bpp as 1024x768 is one pixel */
				/* larger than supported. */
  if (pScrn->depth == 24)
      if ( ((pScrn->bitsPerPixel/8) * pScrn->display->virtualX) > 4095 ) {
        pScrn->virtualX = 4095 / (pScrn->bitsPerPixel / 8);
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
	   "Virtual width adjusted, max for this depth & bpp is %d.\n",
	   pScrn->virtualX );
      }
  
    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = ps3v->minClock;
    clockRanges->maxClock = ps3v->maxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;	/* yes, S3V SVGA 3.3.2 */
    clockRanges->doubleScanAllowed = TRUE;
  
  					/* Screen pointer 		*/
    i = xf86ValidateModes(pScrn, 
  					/* Available monitor modes 	*/
					/* (DisplayModePtr availModes)  */
		pScrn->monitor->Modes,
					/* req mode names for screen 	*/
					/* (char **modesNames)  	*/
		pScrn->display->modes, 
					/* list of clock ranges allowed */
					/* (ClockRangePtr clockRanges) 	*/
		clockRanges,
					/* list of driver line pitches, */
					/* supply or NULL and use min/	*/
					/* max below			*/
					/* (int *linePitches)		*/
		NULL, 
					/* min lin pitch (width)	*/
					/* (int minPitch)		*/
		256, 
					/* max line pitch (width)	*/
					/* (int maxPitch)		*/
		2048,
					/* bits of granularity for line	*/
					/* pitch (width) above, reguired*/
					/* (int pitchInc)		*/
		pScrn->bitsPerPixel,
					/* min virt height, 0 no limit	*/
					/* (int minHeight)		*/
		128, 
					/* max virt height, 0 no limit	*/
					/* (int maxHeight)		*/
		2048,
					/* force virtX, 0 for auto 	*/
					/* (int VirtualX) 		*/
					/* value is adjusted above for  */
					/* hardware limits */
		pScrn->virtualX,
					/* force virtY, 0 for auto	*/
					/* (int VirtualY)		*/
		pScrn->display->virtualY,
					/* size (bytes) of aper used to	*/
					/* access video memory		*/
					/* (unsigned long apertureSize)	*/
		ps3v->videoRambytes,
					/* how to pick mode */
					/* (LookupModeFlags strategy)	*/
		LOOKUP_BEST_REFRESH);
  
    if (i == -1) {
	S3VFreeRec(pScrn);
	return FALSE;
    }
    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	S3VFreeRec(pScrn);
	return FALSE;
    }

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);
    /*xf86SetCrtcForModes(pScrn, 0);*/

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);
    					/* When running the STREAMS processor */
					/* the max. stride is limited to 4096-1 */
					/* so this is the virtualX limit. */
					/* STREAMS is needed for 24 & 32 bpp, */
					/* (all depth 24 modes) */
					/* This should never happen... we */
					/* checked it before ValidateModes */
    if ( ((pScrn->depth == 24) || (pScrn->depth == 16)) && 
         ((pScrn->bitsPerPixel/8) * pScrn->virtualX > 4095) ) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Virtual width to large for ViRGE\n");
      S3VFreeRec(pScrn);
      return FALSE;
    }

    /* Load bpp-specific modules */
    if( ps3v->UseFB )
      {
	if( xf86LoadSubModule(pScrn, "fb") == NULL )
	  {
	      S3VFreeRec(pScrn);
	      return FALSE;
	  }	       
	xf86LoaderReqSymLists(fbSymbols, NULL);       
      }
    else
      {
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
	  if (pix24bpp == 24) {
	    mod = "cfb24";
	    reqSym = "cfb24ScreenInit";
	  } else {
	    mod = "xf24_32bpp";
	    reqSym = "cfb24_32ScreenInit";
	  }
	  break;
	case 32:
	  mod = "cfb32";
	  reqSym = "cfb32ScreenInit";
	  break;
	}
	if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
	    S3VFreeRec(pScrn);
	    return FALSE;
	}	       
    
	xf86LoaderReqSymbols(reqSym, NULL);
      }

    /* Load XAA if needed */
    if (!ps3v->NoAccel || ps3v->hwcursor ) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    S3VFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    /* Load ramdac if needed */
    if (ps3v->hwcursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    S3VFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    if (ps3v->shadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    S3VFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    /* Setup WAITFIFO() for accel and ModeInit() */
    /* Needs to be done prior to first ModeInit call */
    /* and any accel activity. */
    switch(ps3v->Chipset) 
      {
	/* GX2_SERIES chips, GX2 & TRIO_3D_2X */
      case S3_ViRGE_GX2:
      case S3_TRIO_3D_2X:
	ps3v->pWaitFifo = S3VWaitFifoGX2;
	ps3v->pWaitCmd = S3VWaitCmdGX2;
	break;
      case S3_ViRGE:
      case S3_ViRGE_VX:
      default:
	ps3v->pWaitFifo = S3VWaitFifoMain;
	/* Do nothing... */
	ps3v->pWaitCmd = S3VWaitDummy;
	break;
      }

    return TRUE;
}


/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */


/* Mandatory */
static Bool
S3VEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
/*      ScreenPtr pScreen = xf86Screens[scrnIndex]->pScreen; */
    /*vgaHWPtr hwp = VGAHWPTR(pScrn);*/

    PVERB5("	S3VEnterVT\n");
    /*vgaHWUnlockMMIO(hwp);*/
    				/* Enable MMIO and map memory */
#ifdef unmap_always
    S3VMapMem(pScrn);
#endif
    S3VSave(pScrn);
    return S3VModeInit(pScrn, pScrn->currentMode);
}


/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 * We may wish to remap video/MMIO memory too.
 *
 */

/* Mandatory */
static void
S3VLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    S3VPtr ps3v = S3VPTR(pScrn);
    vgaRegPtr vgaSavePtr = &hwp->SavedReg;
    S3VRegPtr S3VSavePtr = &ps3v->SavedReg;
    
    PVERB5("	S3VLeaveVT\n");
    					/* Like S3VRestore, but uses passed */
					/* mode registers.		    */
    S3VWriteMode(pScrn, vgaSavePtr, S3VSavePtr);
    					/* Restore standard register access */
					/* and unmap memory.		    */
#ifdef unmap_always
    S3VUnmapMem(pScrn);
#endif
    /*vgaHWLockMMIO(hwp);*/

}


/* 
 * This function performs the inverse of the restore function: It saves all
 * the standard and extended registers that we are going to modify to set
 * up a video mode. Again, we also save the STREAMS context if it is needed.
 *
 * prototype
 *   void ChipSave(ScrnInfoPtr pScrn)
 *
 */

static void
S3VSave (ScrnInfoPtr pScrn)
{
  unsigned char cr3a, cr66;
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  S3VPtr ps3v = S3VPTR(pScrn);
  S3VRegPtr save = &ps3v->SavedReg;
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = 0;
    
  vgaCRReg = 0;

  vgaCRReg = vgaIOBase + 5;
  vgaCRIndex = vgaIOBase + 4;

    PVERB5("	S3VSave\n");

   /*
    * This function will handle creating the data structure and filling
    * in the generic VGA portion.
    */
	 
   VGAOUT8(vgaCRIndex, 0x66);
   cr66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr66 | 0x80);
   VGAOUT8(vgaCRIndex, 0x3a);
   cr3a = VGAIN8(vgaCRReg);
   save->CR3A = cr3a;

   VGAOUT8(vgaCRReg, cr3a | 0x80);

   /* VGA_SR_MODE saves mode info only, no fonts, no colormap */
					/* Save all for primary, anything */
					/* for secondary cards?, do MODE */
					/* for the moment. */
   if (xf86IsPrimaryPci(ps3v->PciInfo))
   	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_ALL);
   else
   	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_MODE);
   
   VGAOUT8(vgaCRIndex, 0x66);
   VGAOUT8(vgaCRReg, cr66);
   VGAOUT8(vgaCRIndex, 0x3a);             
   VGAOUT8(vgaCRReg, cr3a);

   /* First unlock extended sequencer regs */
   VGAOUT8(0x3c4, 0x08);
   save->SR08 = VGAIN8(0x3c5);
   VGAOUT8(0x3c5, 0x06); 

   /* Now we save all the s3 extended regs we need */
   VGAOUT8(vgaCRIndex, 0x31);             
   save->CR31 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x34);             
   save->CR34 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x36);             
   save->CR36 = VGAIN8(vgaCRReg);

   /* workaround cr3a corruption */
   if( !(ps3v->mx_cr3a_fix))
     {
       VGAOUT8(vgaCRIndex, 0x3a);             
       save->CR3A = VGAIN8(vgaCRReg);
     }

   if (!S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     VGAOUT8(vgaCRIndex, 0x40);
     save->CR40 = VGAIN8(vgaCRReg);
   }
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(vgaCRIndex, 0x41);
     save->CR41 = VGAIN8(vgaCRReg);
   }
   VGAOUT8(vgaCRIndex, 0x42);
   save->CR42 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x45);
   save->CR45 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x51);             
   save->CR51 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x53);             
   save->CR53 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x54);             
   save->CR54 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x55);
   save->CR55 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x58);             
   save->CR58 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x63);
   save->CR63 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x66);             
   save->CR66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x67);             
   save->CR67 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x68);             
   save->CR68 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x69);
   save->CR69 = VGAIN8(vgaCRReg);

   VGAOUT8(vgaCRIndex, 0x33);             
   save->CR33 = VGAIN8(vgaCRReg);
   if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) || S3_ViRGE_GX2_SERIES(ps3v->Chipset) 
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) ) 
   {
      VGAOUT8(vgaCRIndex, 0x85);
      save->CR85 = VGAIN8(vgaCRReg);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX) {
      VGAOUT8(vgaCRIndex, 0x86);
      save->CR86 = VGAIN8(vgaCRReg);
   }
   if ((ps3v->Chipset == S3_ViRGE_GX2) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset) ) {
      VGAOUT8(vgaCRIndex, 0x7B);
      save->CR7B = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x7D);
      save->CR7D = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x87);
      save->CR87 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x92);
      save->CR92 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x93);
      save->CR93 = VGAIN8(vgaCRReg);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) || 
       S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      VGAOUT8(vgaCRIndex, 0x90);
      save->CR90 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x91);
      save->CR91 = VGAIN8(vgaCRReg);
   }

   /* Extended mode timings regs */

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
   VGAOUT8(vgaCRIndex, 0x6d);
   save->CR6D = VGAIN8(vgaCRReg);


   /* Save sequencer extended regs for DCLK PLL programming */

   VGAOUT8(0x3c4, 0x10);
   save->SR10 = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x11);
   save->SR11 = VGAIN8(0x3c5);

   VGAOUT8(0x3c4, 0x12);
   save->SR12 = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x13);
   save->SR13 = VGAIN8(0x3c5);
   if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x29);
     save->SR29 = VGAIN8(0x3c5);
   }
        /* SR 54,55,56,57 undocumented for GX2.  Was this supposed to be CR? */
        /* (These used to be part of the above if() */
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x54);
     save->SR54 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x55);
     save->SR55 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x56);
     save->SR56 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x57);
     save->SR57 = VGAIN8(0x3c5);
   }

   VGAOUT8(0x3c4, 0x15);
   save->SR15 = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x18);
   save->SR18 = VGAIN8(0x3c5);
   if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x0a);
     save->SR0A = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x0F);
     save->SR0F = VGAIN8(0x3c5);
   }

   VGAOUT8(vgaCRIndex, 0x66);
   cr66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr66 | 0x80);
   VGAOUT8(vgaCRIndex, 0x3a);
   cr3a = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr3a | 0x80);

   /* And if streams is to be used, save that as well */

   if(ps3v->NeedSTREAMS) {
      S3VSaveSTREAMS(pScrn, save->STREAMS);
      }

   /* Now save Memory Interface Unit registers */
   if( S3_ViRGE_GX2_SERIES(ps3v->Chipset) 
      /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
     /* No MMPR regs on MX & GX2 */
     }
   else
     {
     save->MMPR0 = INREG(FIFO_CONTROL_REG);
     save->MMPR1 = INREG(MIU_CONTROL_REG);
     save->MMPR2 = INREG(STREAMS_TIMEOUT_REG);
     save->MMPR3 = INREG(MISC_TIMEOUT_REG);
     }

   if (xf86GetVerbosity() > 1) {
      /* Debug */
     /* Which chipsets? */
     if (
	 /* virge */
	 ps3v->Chipset == S3_ViRGE ||
	 /* VX */
	 S3_ViRGE_VX_SERIES(ps3v->Chipset) ||
	 /* DX & GX */
	 ps3v->Chipset == S3_ViRGE_DXGX ||
	 /* GX2 & Trio3D_2X */
	 /* S3_ViRGE_GX2_SERIES(ps3v->Chipset) || */
	 /* Trio3D_2X */
	 /* S3_TRIO_3D_2X_SERIES(ps3v->Chipset) */
	 /* MX & MX+ */
	 /* S3_ViRGE_MX_SERIES(ps3v->Chipset) || */
	 /* MX+ only */
	 /* S3_ViRGE_MXP_SERIES(ps3v->Chipset) || */
	 /* Trio3D */
	 ps3v->Chipset == S3_TRIO_3D
         ) 
       { 

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
         "MMPR regs: %08x %08x %08x %08x\n",
	     INREG(FIFO_CONTROL_REG), 
	     INREG(MIU_CONTROL_REG), 
	     INREG(STREAMS_TIMEOUT_REG), 
	     INREG(MISC_TIMEOUT_REG));
       }

      PVERB5("\n\nViRGE driver: saved current video mode. Register dump:\n\n");
   }

   VGAOUT8(vgaCRIndex, 0x3a);
   VGAOUT8(vgaCRReg, cr3a);
   VGAOUT8(vgaCRIndex, 0x66);
   VGAOUT8(vgaCRReg, cr66);
   				/* Dup the VGA & S3V state to the */
				/* new mode state, but only first time. */
   if( !ps3v->ModeStructInit ) {
     /* XXX Should check the return value of vgaHWCopyReg() */
     vgaHWCopyReg( &hwp->ModeReg, vgaSavePtr );
     memcpy( &ps3v->ModeReg, save, sizeof(S3VRegRec) );
     ps3v->ModeStructInit = TRUE;
   }
   			 
   if (xf86GetVerbosity() > 1) S3VPrintRegs(pScrn);

   return;
}


/* This function saves the STREAMS registers to our private structure */

static void
S3VSaveSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams)
{
  S3VPtr ps3v = S3VPTR(pScrn);

   streams[0] = INREG(PSTREAM_CONTROL_REG);
   streams[1] = INREG(COL_CHROMA_KEY_CONTROL_REG);
   streams[2] = INREG(SSTREAM_CONTROL_REG);
   streams[3] = INREG(CHROMA_KEY_UPPER_BOUND_REG);
   streams[4] = INREG(SSTREAM_STRETCH_REG);
   streams[5] = INREG(BLEND_CONTROL_REG);
   streams[6] = INREG(PSTREAM_FBADDR0_REG);
   streams[7] = INREG(PSTREAM_FBADDR1_REG);
   streams[8] = INREG(PSTREAM_STRIDE_REG);
   streams[9] = INREG(DOUBLE_BUFFER_REG);
   streams[10] = INREG(SSTREAM_FBADDR0_REG);
   streams[11] = INREG(SSTREAM_FBADDR1_REG);
   streams[12] = INREG(SSTREAM_STRIDE_REG);
   streams[13] = INREG(OPAQUE_OVERLAY_CONTROL_REG);
   streams[14] = INREG(K1_VSCALE_REG);
   streams[15] = INREG(K2_VSCALE_REG);
   streams[16] = INREG(DDA_VERT_REG);
   streams[17] = INREG(STREAMS_FIFO_REG);
   streams[18] = INREG(PSTREAM_START_REG);
   streams[19] = INREG(PSTREAM_WINDOW_SIZE_REG);
   streams[20] = INREG(SSTREAM_START_REG);
   streams[21] = INREG(SSTREAM_WINDOW_SIZE_REG);

}
       

/* 
 * This function is used to restore a video mode. It writes out all  
 * of the standard VGA and extended S3 registers needed to setup a 
 * video mode.
 *
 * Note that our life is made more difficult because of the STREAMS
 * processor which must be used for 24bpp. We need to disable STREAMS
 * before we switch video modes, or we risk locking up the machine. 
 * We also have to follow a certain order when reenabling it. 
 */
/* let's try restoring in the same order as in the 3.3.2.3 driver */
static void
S3VWriteMode (ScrnInfoPtr pScrn, vgaRegPtr vgaSavePtr, S3VRegPtr restore)
{
  unsigned char tmp, cr3a=0, cr66, cr67;
  
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;
  
    PVERB5("	S3VWriteMode\n");

  vgaHWProtect(pScrn, TRUE);
   
   /* Are we going to reenable STREAMS in this new mode? */
   ps3v->STREAMSRunning = restore->CR67 & 0x0c; 

   /* First reset GE to make sure nothing is going on */
   if(ps3v->Chipset == S3_ViRGE_VX) {
      VGAOUT8(vgaCRIndex, 0x63);
      if(VGAIN8(vgaCRReg) & 0x01) S3VGEReset(pScrn,0,__LINE__,__FILE__);
      }
   else {
      VGAOUT8(vgaCRIndex, 0x66);
      if(VGAIN8(vgaCRReg) & 0x01) S3VGEReset(pScrn,0,__LINE__,__FILE__);
      }

   /* As per databook, always disable STREAMS before changing modes */
   VGAOUT8(vgaCRIndex, 0x67);
   cr67 = VGAIN8(vgaCRReg);
   if ((cr67 & 0x0c) == 0x0c) {
      S3VDisableSTREAMS(pScrn);     /* If STREAMS was running, disable it */
      }

   /* Restore S3 extended regs */
   VGAOUT8(vgaCRIndex, 0x63);             
   VGAOUT8(vgaCRReg, restore->CR63);
   VGAOUT8(vgaCRIndex, 0x66);             
   VGAOUT8(vgaCRReg, restore->CR66);
   VGAOUT8(vgaCRIndex, 0x3a);             
   VGAOUT8(vgaCRReg, restore->CR3A);
   VGAOUT8(vgaCRIndex, 0x31);    
   VGAOUT8(vgaCRReg, restore->CR31);
   VGAOUT8(vgaCRIndex, 0x58);             
   VGAOUT8(vgaCRReg, restore->CR58);
   VGAOUT8(vgaCRIndex, 0x55);
   VGAOUT8(vgaCRReg, restore->CR55);

   /* Extended mode timings registers */  
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
   VGAOUT8(vgaCRIndex, 0x6d);
   VGAOUT8(vgaCRReg, restore->CR6D);

   /* Restore the desired video mode with CR67 */
        
   VGAOUT8(vgaCRIndex, 0x67);             
   cr67 = VGAIN8(vgaCRReg) & 0xf; /* Possible hardware bug on VX? */
   VGAOUT8(vgaCRReg, 0x50 | cr67); 
   usleep(10000);
   VGAOUT8(vgaCRIndex, 0x67);             
   VGAOUT8(vgaCRReg, restore->CR67 & ~0x0c); /* Don't enable STREAMS yet */

   /* Other mode timing and extended regs */
   VGAOUT8(vgaCRIndex, 0x34);             
   VGAOUT8(vgaCRReg, restore->CR34);
   if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	/* S3_ViRGE_MX_SERIES(ps3v->Chipset) || CR40 reserved on MX */
	S3_ViRGE_MXP_SERIES(ps3v->Chipset) ||
	S3_ViRGE_VX_SERIES(ps3v->Chipset) ||
	/* S3_TRIO_3D_2X_SERIES(ps3v->Chipset) * included in GX2 series */
	ps3v->Chipset == S3_ViRGE_DXGX ||
	ps3v->Chipset == S3_ViRGE 
	)
     {
       VGAOUT8(vgaCRIndex, 0x40);             
       VGAOUT8(vgaCRReg, restore->CR40);
     }
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(vgaCRIndex, 0x41);
     VGAOUT8(vgaCRReg, restore->CR41);
   }
   VGAOUT8(vgaCRIndex, 0x42);             
   VGAOUT8(vgaCRReg, restore->CR42);
   VGAOUT8(vgaCRIndex, 0x45);
   VGAOUT8(vgaCRReg, restore->CR45);
   VGAOUT8(vgaCRIndex, 0x51);             
   VGAOUT8(vgaCRReg, restore->CR51);
   VGAOUT8(vgaCRIndex, 0x54);             
   VGAOUT8(vgaCRReg, restore->CR54);
   
   /* Memory timings */
   VGAOUT8(vgaCRIndex, 0x36);             
   VGAOUT8(vgaCRReg, restore->CR36);
   VGAOUT8(vgaCRIndex, 0x68);             
   VGAOUT8(vgaCRReg, restore->CR68);
   VGAOUT8(vgaCRIndex, 0x69);
   VGAOUT8(vgaCRReg, restore->CR69);

   VGAOUT8(vgaCRIndex, 0x33);
   VGAOUT8(vgaCRReg, restore->CR33);
   if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) || S3_ViRGE_GX2_SERIES(ps3v->Chipset)
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) ) 
   {
      VGAOUT8(vgaCRIndex, 0x85);
      VGAOUT8(vgaCRReg, restore->CR85);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX) {
      VGAOUT8(vgaCRIndex, 0x86);
      VGAOUT8(vgaCRReg, restore->CR86);
   }
   if ( (ps3v->Chipset == S3_ViRGE_GX2) ||
	S3_ViRGE_MX_SERIES(ps3v->Chipset) ) {
      VGAOUT8(vgaCRIndex, 0x7B);
      VGAOUT8(vgaCRReg, restore->CR7B);
      VGAOUT8(vgaCRIndex, 0x7D);
      VGAOUT8(vgaCRReg, restore->CR7D);
      VGAOUT8(vgaCRIndex, 0x87);
      VGAOUT8(vgaCRReg, restore->CR87);
      VGAOUT8(vgaCRIndex, 0x92);
      VGAOUT8(vgaCRReg, restore->CR92);
      VGAOUT8(vgaCRIndex, 0x93);
      VGAOUT8(vgaCRReg, restore->CR93);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      VGAOUT8(vgaCRIndex, 0x90);
      VGAOUT8(vgaCRReg, restore->CR90);
      VGAOUT8(vgaCRIndex, 0x91);
      VGAOUT8(vgaCRReg, restore->CR91);
   }

   /* Unlock extended sequencer regs */
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

   /* Restore extended sequencer regs for DCLK */
   VGAOUT8(0x3c4, 0x12);
   VGAOUT8(0x3c5, restore->SR12);
   VGAOUT8(0x3c4, 0x13);
   VGAOUT8(0x3c5, restore->SR13);
   if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x29);
     VGAOUT8(0x3c5, restore->SR29);
   }
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x54);
     VGAOUT8(0x3c5, restore->SR54);
     VGAOUT8(0x3c4, 0x55);
     VGAOUT8(0x3c5, restore->SR55);
     VGAOUT8(0x3c4, 0x56);
     VGAOUT8(0x3c5, restore->SR56);
     VGAOUT8(0x3c4, 0x57);
     VGAOUT8(0x3c5, restore->SR57);
   }

   VGAOUT8(0x3c4, 0x18);
   VGAOUT8(0x3c5, restore->SR18); 

   /* Load new m,n PLL values for DCLK & MCLK */
   VGAOUT8(0x3c4, 0x15);
   tmp = VGAIN8(0x3c5) & ~0x21;

   /* databook either 0x3 or 0x20, but not both?? */
   VGAOUT8(0x3c5, tmp | 0x03);
   VGAOUT8(0x3c5, tmp | 0x23);
   VGAOUT8(0x3c5, tmp | 0x03);
   VGAOUT8(0x3c5, restore->SR15);
   if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x0a);
     VGAOUT8(0x3c5, restore->SR0A);
     VGAOUT8(0x3c4, 0x0f);
     VGAOUT8(0x3c5, restore->SR0F);
   }

   VGAOUT8(0x3c4, 0x08);
   VGAOUT8(0x3c5, restore->SR08); 


   /* Now write out CR67 in full, possibly starting STREAMS */

   VerticalRetraceWait();
   VGAOUT8(vgaCRIndex, 0x67);    
   VGAOUT8(vgaCRReg, 0x50);   /* For possible bug on VX?! */          
   usleep(10000);
   VGAOUT8(vgaCRIndex, 0x67);
   VGAOUT8(vgaCRReg, restore->CR67); 

   VGAOUT8(vgaCRIndex, 0x66);
   cr66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr66 | 0x80);
   VGAOUT8(vgaCRIndex, 0x3a);

   /* workaround cr3a corruption */
   if( ps3v->mx_cr3a_fix )
     {
       VGAOUT8(vgaCRReg, restore->CR3A | 0x80);
     }
   else
     {
       cr3a = VGAIN8(vgaCRReg);
       VGAOUT8(vgaCRReg, cr3a | 0x80);
     }

   /* And finally, we init the STREAMS processor if we have CR67 indicate 24bpp
    * We also restore FIFO and TIMEOUT memory controller registers. (later...)
    */
	 
   if (ps3v->NeedSTREAMS) {
      if(ps3v->STREAMSRunning) S3VRestoreSTREAMS(pScrn, restore->STREAMS);
      }

   /* Now, before we continue, check if this mode has the graphic engine ON 
    * If yes, then we reset it. 
    * This fixes some problems with corruption at 24bpp with STREAMS
    * Also restore the MIU registers. 
    */

#ifndef MetroLink
   if(ps3v->Chipset == S3_ViRGE_VX) {
      if(restore->CR63 & 0x01) S3VGEReset(pScrn,0,__LINE__,__FILE__);
      }
   else {
      if(restore->CR66 & 0x01) S3VGEReset(pScrn,0,__LINE__,__FILE__);
      }
#else
   S3VGEReset(pScrn,0,__LINE__,__FILE__);
#endif

   VerticalRetraceWait();
   if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) 
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
      VGAOUT8(vgaCRIndex, 0x85);    
      /* primary stream threshold */
      VGAOUT8(vgaCRReg, 0x1f ); 
     }
   else
     {
       OUTREG(FIFO_CONTROL_REG, restore->MMPR0);
     }
   if( !( S3_ViRGE_GX2_SERIES(ps3v->Chipset) 
	  /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) ))
   {
     WaitIdle();                  /* Don't ask... */
     OUTREG(MIU_CONTROL_REG, restore->MMPR1);
     WaitIdle();                  
     OUTREG(STREAMS_TIMEOUT_REG, restore->MMPR2);
     WaitIdle();
     OUTREG(MISC_TIMEOUT_REG, restore->MMPR3);
   }
 
   /* Restore the standard VGA registers */
   /* False indicates no fontinfo restore. */
   /* VGA_SR_MODE restores mode info only, no font, no colormap */
   					/* Do all for primary video */
   if (xf86IsPrimaryPci(ps3v->PciInfo))
     vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_ALL);
   					/* Mode only for non-primary? */
   else
     vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_MODE);
 		/* moved from before vgaHWRestore, to prevent segfault? */
   VGAOUT8(vgaCRIndex, 0x66);             
   VGAOUT8(vgaCRReg, cr66);
   VGAOUT8(vgaCRIndex, 0x3a);             

   /* workaround cr3a corruption */
   if( ps3v->mx_cr3a_fix )
     VGAOUT8(vgaCRReg, restore->CR3A);
   else
     VGAOUT8(vgaCRReg, cr3a);


   if (xf86GetVerbosity() > 1) {
      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV, 
         "ViRGE driver: done restoring mode, dumping CR registers:\n");
      S3VPrintRegs(pScrn);
   }
   
   vgaHWProtect(pScrn, FALSE);

   return;

}

			    
/* This function restores the saved STREAMS registers */

static void
S3VRestoreSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams)
{
  S3VPtr ps3v = S3VPTR(pScrn);


/* For now, set most regs to their default values for 24bpp 
 * Restore only those that are needed for width/height/stride
 * Otherwise, we seem to get lockups because some registers 
 * when saved have some reserved bits set.
 */

  OUTREG(PSTREAM_CONTROL_REG, streams[0] & 0x77000000);
  OUTREG(COL_CHROMA_KEY_CONTROL_REG, 0x00);
  OUTREG(SSTREAM_CONTROL_REG, 0x03000000);
  OUTREG(CHROMA_KEY_UPPER_BOUND_REG, 0x00);
  OUTREG(SSTREAM_STRETCH_REG, 0x00);
  OUTREG(BLEND_CONTROL_REG, 0x01000000);
  OUTREG(PSTREAM_FBADDR0_REG, 0x00);
  OUTREG(PSTREAM_FBADDR1_REG, 0x00);
  OUTREG(PSTREAM_STRIDE_REG, streams[8] & 0x0fff);
  OUTREG(DOUBLE_BUFFER_REG, 0x00);
  OUTREG(SSTREAM_FBADDR0_REG, 0x00);
  OUTREG(SSTREAM_FBADDR1_REG, 0x00);
  OUTREG(SSTREAM_STRIDE_REG, 0x01);
  OUTREG(OPAQUE_OVERLAY_CONTROL_REG, 0x40000000);
  OUTREG(K1_VSCALE_REG, 0x00);
  OUTREG(K2_VSCALE_REG, 0x00);
  OUTREG(DDA_VERT_REG, 0x00);
  OUTREG(PSTREAM_START_REG, 0x00010001);
  OUTREG(PSTREAM_WINDOW_SIZE_REG, streams[19] & 0x07ff07ff);
  OUTREG(SSTREAM_START_REG, 0x07ff07ff);
  OUTREG(SSTREAM_WINDOW_SIZE_REG, 0x00010001);


}




/* And this function disables the STREAMS processor as per databook.
 * This is usefull before we do a mode change 
 */

static void
S3VDisableSTREAMS(ScrnInfoPtr pScrn)
{
unsigned char tmp;
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

   VerticalRetraceWait();
   OUTREG(FIFO_CONTROL_REG, 0xC000);
   VGAOUT8(vgaCRIndex, 0x67);
   tmp = VGAIN8(vgaCRReg);
                         /* Disable STREAMS processor */
   VGAOUT8( vgaCRReg, tmp & ~0x0C );

   return;
}



/* MapMem - contains half of pre-4.0 EnterLeave function */
/* The EnterLeave function which en/dis access to IO ports and ext. regs */
                /********************************************************/
		/* Aaagh...  So many locations!  On my machine (KJB) the*/
		/* following is true. 					*/
		/* PciInfo->memBase[0] returns e400 0000 		*/
		/* From my ViRGE manual, the memory map looks like 	*/
		/* Linear mem - 16M  	000 0000 - 0ff ffff 		*/
		/* Image xfer - 32k  	100 0000 - 100 7fff 		*/
		/* PCI cnfg    		100 8000 - 100 8043 		*/
		/* ...				   			*/
		/* CRT VGA 3b? reg	100 83b0 - 			*/
		/* And S3_NEWMMIO_VGABASE = S3_NEWMMIO_REGBASE + 0x8000	*/
		/* where S3_NEWMMIO_REGBASE = 0x100 0000  ( 16MB )      */
		/* S3_NEWMMIO_REGSIZE = 0x1 0000  ( 64KB )		*/
		/* S3V_MMIO_REGSIZE = 0x8000 ( 32KB ) - above includes	*/
		/* the image transfer area, so this one is used instead.*/
		/* ps3v->IOBase is assinged the virtual address returned*/
		/* from MapPciMem, it is the address to base all 	*/
		/* register access. (It is a pointer.)  		*/
		/* hwp->MemBase is a CARD32, containing the register	*/
		/* base. (It's a conversion from IOBase above.) 	*/
                /********************************************************/


static Bool
S3VMapMem(ScrnInfoPtr pScrn)
{    
  S3VPtr ps3v;
  vgaHWPtr hwp;

    PVERB5("	S3VMapMem\n");
    
  ps3v = S3VPTR(pScrn);
   
    					/* Map the ViRGE register space */
					/* Starts with Image Transfer area */
					/* so that we can use registers map */
					/* structure - see newmmio.h */
					/* around 0x10000 from MemBase */
  ps3v->MapBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, ps3v->PciTag,
			ps3v->PciInfo->memBase[0] + S3_NEWMMIO_REGBASE,
			S3_NEWMMIO_REGSIZE);

  ps3v->MapBaseDense = xf86MapPciMem(pScrn->scrnIndex,
			VIDMEM_MMIO_32BIT,
			ps3v->PciTag,
			ps3v->PciInfo->memBase[0] + S3_NEWMMIO_REGBASE,
			0x8000);

  if( !ps3v->MapBase ) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	"Internal error: could not map registers.\n");
    return FALSE;
  }
					/* Map the framebuffer */
  if (ps3v->videoRambytes) { /* not set in PreInit() */
      ps3v->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER, 
				   ps3v->PciTag, ps3v->PciInfo->memBase[0],
				   ps3v->videoRambytes );

      if( !ps3v->FBBase ) {
	  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		     "Internal error: could not map framebuffer.\n");
	  return FALSE;
      }
  		       		/* Initially the visual display start */
				/* is the same as the mapped start. */
      ps3v->FBStart = ps3v->FBBase;
  }
  
  pScrn->memPhysBase = ps3v->PciInfo->memBase[0];
  pScrn->fbOffset = 0;

  				/* Set up offset to hwcursor memory area */
  				/* It's a 1K chunk at the end of the frame buffer */
  ps3v->FBCursorOffset = ps3v->videoRambytes - 1024;
  S3VEnableMmio( pScrn);
   					/* Assign hwp->MemBase & IOBase here */
  hwp = VGAHWPTR(pScrn);
					/* Sets MMIOBase and Offset, assigns */
					/* functions. Offset from map area   */
					/* to VGA reg area is 0x8000. */
  vgaHWSetMmioFuncs( hwp, ps3v->MapBase, S3V_MMIO_REGSIZE );
  					/* assigns hwp->IOBase to 3D0 or 3B0 */
					/* needs hwp->MMIOBase to work */
  vgaHWGetIOBase(hwp);
  
    					/* Map the VGA memory when the */
					/* primary video */
	  
  if (xf86IsPrimaryPci(ps3v->PciInfo)) {
    hwp->MapSize = 0x10000;
    if (!vgaHWMapMem(pScrn))
      return FALSE;
    ps3v->PrimaryVidMapped = TRUE;
  }
  
  return TRUE;
}



/* UnMapMem - contains half of pre-4.0 EnterLeave function */
/* The EnterLeave function which en/dis access to IO ports and ext. regs */

static void 
S3VUnmapMem(ScrnInfoPtr pScrn)
{
  S3VPtr ps3v;

  ps3v = S3VPTR(pScrn);
					/* Unmap VGA mem if mapped. */
  if( ps3v->PrimaryVidMapped ) {
    vgaHWUnmapMem( pScrn );
    ps3v->PrimaryVidMapped = FALSE;
  }

  S3VDisableMmio(pScrn);

  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)ps3v->MapBase,
		  S3_NEWMMIO_REGSIZE);
  if (ps3v->FBBase)
      xf86UnMapVidMem(pScrn->scrnIndex, (pointer)ps3v->FBBase,
		      ps3v->videoRambytes);
  xf86UnMapVidMem(pScrn->scrnIndex, (pointer)ps3v->MapBaseDense,
		  0x8000);

  return;
}



/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
S3VScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
  ScrnInfoPtr pScrn;
  S3VPtr ps3v;
  int ret;
  
  PVERB5("	S3VScreenInit\n");
                                        /* First get the ScrnInfoRec */
  pScrn = xf86Screens[pScreen->myNum];
  					/* Get S3V rec */
  ps3v = S3VPTR(pScrn);
  					/* Make sure we have card access */
/*  xf86EnableAccess(pScrn);*/
   					/* Map MMIO regs and framebuffer */
  if( !S3VMapMem(pScrn) )
    return FALSE;
    					/* Save the chip/graphics state */
  S3VSave(pScrn);
				 	/* Blank the screen during init */
  vgaHWBlankScreen(pScrn, TRUE );  
    					/* Initialise the first mode */
  if (!S3VModeInit(pScrn, pScrn->currentMode))
    return FALSE;
    
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
     * Reset the visual list.
     */
  miClearVisualTypes();

    /* Setup the visuals we support. */

    /*
     * For bpp > 8, the default visuals are not acceptable because we only
     * support TrueColor and not DirectColor.  To deal with this, call
     * miSetVisualTypes with the appropriate visual mask.
     */

  if (pScrn->bitsPerPixel > 8) {
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
				pScrn->defaultVisual))
	    return FALSE;

	if (!miSetPixmapDepths ())
	    return FALSE;
  } else {
	if (!miSetVisualTypes(pScrn->depth,
			      miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
	    return FALSE;

	if (!miSetPixmapDepths ())
	    return FALSE;
  }

  ret = S3VInternalScreenInit(scrnIndex, pScreen);

  if (!ret)
    return FALSE;
      
  xf86SetBlackWhitePixels(pScreen);
	 
  if (pScrn->bitsPerPixel > 8) {
    	VisualPtr visual;
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
  if (ps3v->UseFB)
    fbPictureInit (pScreen, 0, 0);
    
  	      				/* Initialize acceleration layer */
  if (!ps3v->NoAccel) {
    if(pScrn->bitsPerPixel == 32) {
      /* 32 bit Accel currently broken 
      if (!S3VAccelInit32(pScreen))
        return FALSE;
	*/
	;
    } else 
      if (!S3VAccelInit(pScreen))
        return FALSE;
  }
	
  miInitializeBackingStore(pScreen);
  xf86SetBackingStore(pScreen);
  xf86SetSilkenMouse(pScreen);
  						/* hardware cursor needs to wrap this layer */
  S3VDGAInit(pScreen);

    					/* Initialise cursor functions */
  miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
	Must follow software cursor initialization*/
  if (ps3v->hwcursor) { 
  if(!S3VHWCursorInit(pScreen)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		"Hardware cursor initialization failed\n");
		}
  }

  if (ps3v->shadowFB) {
      RefreshAreaFuncPtr refreshArea = s3vRefreshArea;
      
      if(ps3v->rotate) {
	  if (!ps3v->PointerMoved) {
	      ps3v->PointerMoved = pScrn->PointerMoved;
	      pScrn->PointerMoved = s3vPointerMoved;
	  }
	  
	  switch(pScrn->bitsPerPixel) {
	  case 8:	refreshArea = s3vRefreshArea8;	break;
	  case 16:	refreshArea = s3vRefreshArea16;	break;
	  case 24:	refreshArea = s3vRefreshArea24;	break;
	  case 32:	refreshArea = s3vRefreshArea32;	break;
	  }
      }
      
      ShadowFBInit(pScreen, refreshArea);
  }

    					/* Initialise default colourmap */
  if (!miCreateDefColormap(pScreen))
    return FALSE;
  					/* Initialize colormap layer.   */
					/* Must follow initialization   */
					/* of the default colormap. 	*/
					/* And SetGamma call, else it 	*/
					/* will load palette with solid */
					/* white. */
  if(!xf86HandleColormaps(pScreen, 256, 6, S3VLoadPalette, NULL,
			CMAP_RELOAD_ON_MODE_SWITCH ))
	return FALSE;
				    	/* All the ugly stuff is done, 	*/
					/* so re-enable the screen. 	*/
  vgaHWBlankScreen(pScrn, FALSE );  

#if 0
  pScrn->racMemFlags = RAC_COLORMAP | RAC_CURSOR | RAC_FB | RAC_VIEWPORT;
#endif
  pScreen->SaveScreen = S3VSaveScreen;

    					/* Wrap the current CloseScreen function */
  ps3v->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = S3VCloseScreen;

  if(xf86DPMSInit(pScreen, S3VDisplayPowerManagementSet, 0) == FALSE)
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "DPMS initialization failed!\n");
  
#ifndef XvExtension
    {
	XF86VideoAdaptorPtr *ptr;
	int n;

	n = xf86XVListGenericAdaptors(pScrn,&ptr);
	if (n) {
	    xf86XVScreenInit(pScreen, ptr, n);
	}
    }
#else
    S3VInitVideo(pScreen);
#endif
 
    /* Report any unused options (only for the first generation) */
  if (serverGeneration == 1) {
    xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
  }
    					/* Done */
  return TRUE;
}



/* Common init routines needed in EnterVT and ScreenInit */

static int
S3VInternalScreenInit( int scrnIndex, ScreenPtr pScreen)
{
  int ret = TRUE;
  ScrnInfoPtr pScrn;
  S3VPtr ps3v;
  int width, height, displayWidth;
  unsigned char* FBStart;

  					/* First get the ScrnInfoRec */
  pScrn = xf86Screens[pScreen->myNum];

  ps3v = S3VPTR(pScrn);

  displayWidth = pScrn->displayWidth;
  if (ps3v->rotate) {
      height = pScrn->virtualX;
      width = pScrn->virtualY;
  } else {
      width = pScrn->virtualX;
      height = pScrn->virtualY;
  }
  
  if(ps3v->shadowFB) {
      ps3v->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
      ps3v->ShadowPtr = xalloc(ps3v->ShadowPitch * height);
      displayWidth = ps3v->ShadowPitch / (pScrn->bitsPerPixel >> 3);
      FBStart = ps3v->ShadowPtr;
  } else {
      ps3v->ShadowPtr = NULL;
      FBStart = ps3v->FBStart;
  }
  
    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

  if( ps3v->UseFB )
    {
      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using FB\n");

      switch (pScrn->bitsPerPixel) 
	{
	case 8:
	case 16:
	case 24:
	  ret = fbScreenInit(pScreen, FBStart, pScrn->virtualX,
			     pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
			     displayWidth, pScrn->bitsPerPixel);
	  break;
	default:
	  xf86DrvMsg(scrnIndex, X_ERROR,
		     "Internal error: invalid bpp (%d) in S3VScreenInit\n",
		     pScrn->bitsPerPixel);
	  ret = FALSE;
	  break;
	}
    }
  else
    {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using CFB\n");
      switch (pScrn->bitsPerPixel) {
      case 8:
	ret = cfbScreenInit(pScreen, FBStart,
			    width,height,
			    pScrn->xDpi, pScrn->yDpi,
			    displayWidth);
	break;
      case 16:
	ret = cfb16ScreenInit(pScreen, FBStart,
			      width,height,
			      pScrn->xDpi, pScrn->yDpi,
			      displayWidth);
	break;
      case 24:
	if (pix24bpp ==24) {
	  ret = cfb24ScreenInit(pScreen, FBStart,
			    width,height,
				  pScrn->xDpi, pScrn->yDpi,
				  displayWidth);
	} else {
	  ret = cfb24_32ScreenInit(pScreen, FBStart,
				     width,height,
				     pScrn->xDpi, pScrn->yDpi,
				     displayWidth);
	}
	break;
      case 32:
	ret = cfb32ScreenInit(pScreen, FBStart,
			      width,height,
			      pScrn->xDpi, pScrn->yDpi,
			      displayWidth);
	break;
      default:
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Internal error: invalid bpp (%d) in S3VScreenInit\n",
		   pScrn->bitsPerPixel);
	ret = FALSE;
	break;
      } /*switch*/
    } /*if(fb)*/
  return ret;
}



/* Checks if a mode is suitable for the selected chipset. */

static ModeStatus
S3VValidMode(int index, DisplayModePtr mode, Bool verbose, int flags)
{

  return MODE_OK;
}



static Bool
S3VModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  int width, dclk;
  int i, j;
  unsigned char tmp = 0;
  
  		      		/* Store values to current mode register structs */
  S3VRegPtr new = &ps3v->ModeReg;
  vgaRegPtr vganew = &hwp->ModeReg;
  int vgaCRIndex, vgaCRReg, vgaIOBase;

  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

    PVERB5("	S3VModeInit\n");   

    /* Set scale factors for mode timings */

    if (ps3v->Chipset == S3_ViRGE_VX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) || 
	S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      ps3v->HorizScaleFactor = 1;
    }
    else if (pScrn->bitsPerPixel == 8) {
      ps3v->HorizScaleFactor = 1;
    }
    else if (pScrn->bitsPerPixel == 16) {
      if (S3_TRIO_3D_SERIES(ps3v->Chipset) && mode->Clock > 115000)
	ps3v->HorizScaleFactor = 1;
      else
	ps3v->HorizScaleFactor = 2;
    }
    else {
      ps3v->HorizScaleFactor = 1;
    }


   /* First we adjust the horizontal timings if needed */

   if(ps3v->HorizScaleFactor != 1)
      if (!mode->CrtcHAdjusted) {
             mode->CrtcHDisplay *= ps3v->HorizScaleFactor;
             mode->CrtcHSyncStart *= ps3v->HorizScaleFactor;
             mode->CrtcHSyncEnd *= ps3v->HorizScaleFactor;
             mode->CrtcHTotal *= ps3v->HorizScaleFactor;
             mode->CrtcHSkew *= ps3v->HorizScaleFactor;
             mode->CrtcHAdjusted = TRUE;
             }

   if(!vgaHWInit (pScrn, mode))
      return FALSE;
      
   /* Now we fill in the rest of the stuff we need for the virge */
   /* Start with MMIO, linear addr. regs */

   VGAOUT8(vgaCRIndex, 0x3a);
   tmp = VGAIN8(vgaCRReg);
   if( S3_ViRGE_GX2_SERIES(ps3v->Chipset) 
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
     if(ps3v->pci_burst)
       /*new->CR3A = (tmp & 0x38) | 0x10; / ENH 256, PCI burst */
       /* Don't clear reserved bits... */
        new->CR3A = (tmp & 0x7f) | 0x10; /* ENH 256, PCI burst */
     else 
        new->CR3A = tmp | 0x90;      /* ENH 256, no PCI burst! */
     }
   else
     {
     if(ps3v->pci_burst)
        new->CR3A = (tmp & 0x7f) | 0x15; /* ENH 256, PCI burst */
     else 
        new->CR3A = tmp | 0x95;      /* ENH 256, no PCI burst! */
     }
  

   VGAOUT8(vgaCRIndex, 0x55);
   new->CR55 = VGAIN8(vgaCRReg);
   if (ps3v->hwcursor) 
     new->CR55 |= 0x10;  /* Enables X11 hw cursor mode */
   if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     new->CR31 = 0x0c;               /* [trio3d] page 54 */
   } else {
     new->CR53 = 0x08;     /* Enables MMIO */
     new->CR31 = 0x8c;     /* Dis. 64k window, en. ENH maps */    
   }

   /* Enables S3D graphic engine and PCI disconnects */
   if(ps3v->Chipset == S3_ViRGE_VX){
      new->CR66 = 0x90;  
      new->CR63 = 0x09;
      }
   else {
     new->CR66 = 0x89;
     /* Set display fifo */
     if( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	 S3_ViRGE_MX_SERIES(ps3v->Chipset) )
       {
	 /* Changed from 0x08 based on reports that this */
	 /* prevents MX from running properly below 1024x768 */
	 new->CR63 = 0x10;
       }
     else
       {
	 new->CR63 = 0;
       }
      }    

  /* Now set linear addr. registers */
  /* LAW size: we have 2 cases, 2MB, 4MB or >= 4MB for VX */
   VGAOUT8(vgaCRIndex, 0x58);
   new->CR58 = VGAIN8(vgaCRReg) & 0x80;
   if(pScrn->videoRam == 2048){   
      new->CR58 |= 0x02 | 0x10; 
      }
   else if (pScrn->videoRam == 1024) {
      new->CR58 |= 0x01 | 0x10; 
   }
   else {
     if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) && pScrn->videoRam == 8192)
       new->CR58 |= 0x07 | 0x10; /* 8MB window on Trio3D/2X */
     else
       new->CR58 |= 0x03 | 0x10; /* 4MB window on virge, 8MB on VX */
      } 
   if(ps3v->Chipset == S3_ViRGE_VX)
      new->CR58 |= 0x40;
   if (ps3v->early_ras_precharge)
      new->CR58 |= 0x80;
   if (ps3v->late_ras_precharge)
      new->CR58 &= 0x7f;
      
  /* ** On PCI bus, no need to reprogram the linear window base address */
  
  /* Now do clock PLL programming. Use the s3gendac function to get m,n */
  /* Also determine if we need doubling etc. */

   dclk = mode->Clock;
   new->CR67 = 0x00;             /* Defaults */

   if (!S3_TRIO_3D_SERIES(ps3v->Chipset)) 
     new->SR15 = 0x03 | 0x80; 
   else {
     VGAOUT8(0x3c4, 0x15);
     new->SR15 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x0a);
     new->SR0A = VGAIN8(0x3c5);
     if (ps3v->slow_dram) {
       new->SR15 = 0x03;  /* 3 CYC MWR */
       new->SR0A &= 0x7F;
     } else if (ps3v->fast_dram) {
       new->SR15 = 0x03 | 0x80; /* 2 CYC MWR */
       new->SR0A |= 0x80;
     } else { /* keep BIOS init defaults */
       new->SR15 = (new->SR15 & 0x80) | 0x03;
     }
   }
   new->SR18 = 0x00;
   new->CR43 = 0x00;
   new->CR45 = 0x00;
   				/* Enable MMIO to RAMDAC registers */
   new->CR65 = 0x00;		/* CR65_2 must be zero, doc seems to be wrong */
   new->CR54 = 0x00;
   
   if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	/* S3_ViRGE_MX_SERIES(ps3v->Chipset) || CR40 reserved on MX */
	S3_ViRGE_MXP_SERIES(ps3v->Chipset) ||
	S3_ViRGE_VX_SERIES(ps3v->Chipset) ||
	/* S3_TRIO_3D_2X_SERIES(ps3v->Chipset) * included in GX2 series */
	ps3v->Chipset == S3_ViRGE_DXGX ||
	ps3v->Chipset == S3_ViRGE 
	) {
     VGAOUT8(vgaCRIndex, 0x40);
     new->CR40 = VGAIN8(vgaCRReg) & ~0x01;
   }

   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     /* fix problems with APM suspend/resume trashing CR90/91 */
     switch(pScrn->bitsPerPixel) {
       case  8: new->CR41 = 0x38; break;
       case 15: new->CR41 = 0x58; break;
       case 16: new->CR41 = 0x48; break;
       default: new->CR41 = 0x77;
     }
   }
   
    xf86ErrorFVerb(VERBLEV, "	S3VModeInit dclk=%i \n", 
   	dclk
	);
   
   /* Memory controller registers. Optimize for better graphics engine 
    * performance. These settings are adjusted/overridden below for other bpp/
    * XConfig options.The idea here is to give a longer number of contiguous
    * MCLK's to both refresh and the graphics engine, to diminish the 
    * relative penalty of 3 or 4 mclk's needed to setup memory transfers. 
    */
   new->MMPR0 = 0x010400; /* defaults */
   new->MMPR1 = 0x00;   
   new->MMPR2 = 0x0808;  
   new->MMPR3 = 0x08080810; 

   /*
    * These settings look like they ought to be better adjusted for depth,
    * so for problem modes running without any fifo_ option should be
    * usable.  Note that these adjust some memory timings and relate to
    * the boards MCLK setting.
    * */
    if( ps3v->fifo_aggressive || ps3v->fifo_moderate || 
       ps3v->fifo_conservative ) {
    
         new->MMPR1 = 0x0200;   /* Low P. stream waits before filling */
         new->MMPR2 = 0x1808;   /* Let the FIFO refill itself */
         new->MMPR3 = 0x08081810; /* And let the GE hold the bus for a while */
      } 

   /* And setup here the new value for MCLK. We use the XConfig 
    * option "set_mclk", whose value gets stored in ps3v->MCLK.
    * I'm not sure what the maximum "permitted" value should be, probably
    * 100 MHz is more than enough for now.  
    */

   if(ps3v->MCLK> 0) {
       if (S3_ViRGE_MX_SERIES(ps3v->Chipset))
	  S3VCommonCalcClock(pScrn, mode, 
			     (int)(ps3v->MCLK / ps3v->refclk_fact), 
			     1, 1, 31, 0, 3,
			     135000, 270000, &new->SR11, &new->SR10);
       else
	  S3VCommonCalcClock(pScrn, mode, ps3v->MCLK, 1, 1, 31, 0, 3,
			     135000, 270000, &new->SR11, &new->SR10);
       }
   else {
       new->SR10 = 255; /* This is a reserved value, so we use as flag */
       new->SR11 = 255; 
       }

   					/* most modes don't need STREAMS */
					/* processor, preset FALSE */
   /* support for XVideo needs streams, so added it to some modes */
   ps3v->NeedSTREAMS = FALSE;
   
   if(ps3v->Chipset == S3_ViRGE_VX){
       if (pScrn->bitsPerPixel == 8) {
          if (dclk <= 110000) new->CR67 = 0x00; /* 8bpp, 135MHz */
          else new->CR67 = 0x10;                /* 8bpp, 220MHz */
          }
       else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
          if (dclk <= 110000) new->CR67 = 0x20; /* 15bpp, 135MHz */
          else new->CR67 = 0x30;                /* 15bpp, 220MHz */
          } 
       else if (pScrn->bitsPerPixel == 16) {
          if (dclk <= 110000) new->CR67 = 0x40; /* 16bpp, 135MHz */
          else new->CR67 = 0x50;                /* 16bpp, 220MHz */
          }
       else if ((pScrn->bitsPerPixel == 24) || (pScrn->bitsPerPixel == 32)) {
          new->CR67 = 0xd0 | 0x0c;              /* 24bpp, 135MHz, STREAMS */
	  					/* Flag STREAMS proc. required */
          ps3v->NeedSTREAMS = TRUE;
          S3VInitSTREAMS(pScrn, new->STREAMS, mode);
          new->MMPR0 = 0xc098;            /* Adjust FIFO slots */
          }
       S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 4, 
	   220000, 440000, &new->SR13, &new->SR12);

      } /* end VX if() */
   else if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
       if (pScrn->bitsPerPixel == 8)
	  new->CR67 = 0x00;
       else if (pScrn->bitsPerPixel == 16) {
	 /* XV support needs STREAMS in depth 16 */
          ps3v->NeedSTREAMS = TRUE;
          S3VInitSTREAMS(pScrn, new->STREAMS, mode);
	  if (pScrn->weight.green == 5)
	     new->CR67 = 0x30 | 0x4;                  /* 15bpp */
	  else
	     new->CR67 = 0x50 | 0x4;                  /* 16bpp */
          }
       else if ((pScrn->bitsPerPixel == 24) ) {
	 new->CR67 = 0x74;              /* 24bpp, STREAMS */
	  					/* Flag STREAMS proc. required */
          ps3v->NeedSTREAMS = TRUE;
          S3VInitSTREAMS(pScrn, new->STREAMS, mode);
          }
       else if (pScrn->bitsPerPixel == 32) {
          new->CR67 = 0xd0;              /* 32bpp */
	  	/* Missing STREAMs and other stuff here? KJB */
          /* new->MMPR0 = 0xc098;            / Adjust FIFO slots */
          }
       {
         unsigned char ndiv;
	 if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
	   unsigned char sr8;
	   VGAOUT8(0x3c4, 0x08);  /* unlock extended SEQ regs */
	   sr8 = VGAIN8(0x3c5);
	   VGAOUT8(0x3c5, 0x06);
	   VGAOUT8(0x3c4, 0x31);
	   if (VGAIN8(0x3c5) & 0x10) { /* LCD on */
	     if (!ps3v->LCDClk) {  /* entered only once for first mode */
	       int h_lcd, v_lcd;
	       VGAOUT8(0x3c4, 0x61);
	       h_lcd = VGAIN8(0x3c5);
	       VGAOUT8(0x3c4, 0x66);
	       h_lcd |= ((VGAIN8(0x3c5) & 0x02) << 7);
	       h_lcd = (h_lcd+1) * 8;
	       VGAOUT8(0x3c4, 0x69);
	       v_lcd = VGAIN8(0x3c5);
	       VGAOUT8(0x3c4, 0x6e);
	       v_lcd |= ((VGAIN8(0x3c5) & 0x70) << 4);
	       v_lcd++;
	       
	       /* check if first mode has physical LCD resolution */
	       if (pScrn->modes->HDisplay == h_lcd && pScrn->modes->VDisplay == v_lcd)
		 ps3v->LCDClk = mode->Clock;
	       else {
		 int n1, n2, sr12, sr13, sr29;
		 VGAOUT8(0x3c4, 0x12);
		 sr12 = VGAIN8(0x3c5);
		 VGAOUT8(0x3c4, 0x13);
		 sr13 = VGAIN8(0x3c5) & 0x7f;
		 VGAOUT8(0x3c4, 0x29);
		 sr29 = VGAIN8(0x3c5);
		 n1 = sr12 & 0x1f;
		 n2 = ((sr12>>6) & 0x03) | ((sr29 & 0x01) << 2);
		 ps3v->LCDClk = ((int)(ps3v->refclk_fact * 1431818 * (sr13+2)) / (n1+2) / (1 << n2) + 50) / 100;
	       }
	     }
	     S3VCommonCalcClock(pScrn, mode, 
			     (int)(ps3v->LCDClk / ps3v->refclk_fact), 
			     1, 1, 31, 0, 4,
			     170000, 340000, &new->SR13, &ndiv);
	   }
	   else
	     S3VCommonCalcClock(pScrn, mode, 
			     (int)(dclk / ps3v->refclk_fact), 
			     1, 1, 31, 0, 4,
			     170000, 340000, &new->SR13, &ndiv);
	   VGAOUT8(0x3c4, 0x08);
	   VGAOUT8(0x3c5, sr8);
	 }
	 else  /* S3_ViRGE_GX2 */
	   S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 4,
			   170000, 340000, &new->SR13, &ndiv);
         new->SR29 = ndiv >> 7;
         new->SR12 = (ndiv & 0x1f) | ((ndiv & 0x60) << 1);
       }
   } /* end GX2 or MX if() */
   else if(S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      new->SR0F = 0x00;
      if (pScrn->bitsPerPixel == 8) {
         if(dclk > 115000) {                     /* We need pixmux */
            new->CR67 = 0x10;
            new->SR15 |= 0x10;                   /* Set DCLK/2 bit */
            new->SR18 = 0x80;                   /* Enable pixmux */
        }
      }
      else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
        if(dclk > 115000) {
           new->CR67 = 0x20;
           new->SR15 |= 0x10;
           new->SR18 = 0x80;
	   new->SR0F = 0x10;
        } else {
           new->CR67 = 0x30;                       /* 15bpp */
        }
      }
      else if (pScrn->bitsPerPixel == 16) {
        if(dclk > 115000) {
            new->CR67 = 0x40;
            new->SR15 |= 0x10;
            new->SR18 = 0x80;
	    new->SR0F = 0x10;
        } else {
           new->CR67 = 0x50;
        }
      }
      else if (pScrn->bitsPerPixel == 24) {
         new->CR67 = 0xd0 | 0x0c;
	 ps3v->NeedSTREAMS = TRUE;
         S3VInitSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
      }
      else if (pScrn->bitsPerPixel == 32) {
         new->CR67 = 0xd0 | 0x0c;
	 ps3v->NeedSTREAMS = TRUE;
         S3VInitSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0x10000;            /* Still more FIFO slots */
	 new->SR0F = 0x10;
      }
      S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 4,
                     230000, 460000, &new->SR13, &new->SR12);
   } /* end TRIO_3D if() */
   else if(ps3v->Chipset == S3_ViRGE_DXGX) {
      if (pScrn->bitsPerPixel == 8) {
         if(dclk > 80000) {                     /* We need pixmux */
            new->CR67 = 0x10;
            new->SR15 |= 0x10;                   /* Set DCLK/2 bit */
            new->SR18 = 0x80;                   /* Enable pixmux */
            }
         }
      else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
         new->CR67 = 0x30;                       /* 15bpp */
         }
      else if (pScrn->bitsPerPixel == 16) {
	if(mode->Flags & V_DBLSCAN)
	  {
	    new->CR67 = 0x50;
	  }
	else
	  {
	    new->CR67 = 0x50 | 0x0c;
	    /* Flag STREAMS proc. required */
	    /* XV support needs STREAMS in depth 16 */
	    ps3v->NeedSTREAMS = TRUE;
	    S3VInitSTREAMS(pScrn, new->STREAMS, mode);
	  }
	 if( ps3v->XVideo )
	   {
	     new->MMPR0 = 0x107c02;            /* Adjust FIFO slots, overlay */
	   }
	 else
	   {
	     new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
	   }
         }
      else if (pScrn->bitsPerPixel == 24) { 
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         S3VInitSTREAMS(pScrn, new->STREAMS, mode);
	 if( ps3v->XVideo )
	   {
	     new->MMPR0 = 0x107c02;            /* Adjust FIFO slots, overlay */
	   }
	 else
	   {
	     new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
	   }
         }
      else if (pScrn->bitsPerPixel == 32) { 
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         S3VInitSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0x10000;            /* Still more FIFO slots */
         }
      S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 3, 
	135000, 270000, &new->SR13, &new->SR12);
   } /* end DXGX if() */
   else {           /* Everything else ... (only ViRGE) */
      if (pScrn->bitsPerPixel == 8) {
         if(dclk > 80000) {                     /* We need pixmux */
            new->CR67 = 0x10;
            new->SR15 |= 0x10;                   /* Set DCLK/2 bit */
            new->SR18 = 0x80;                   /* Enable pixmux */
            }
         }
      else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
         new->CR67 = 0x30;                       /* 15bpp */
         }
      else if (pScrn->bitsPerPixel == 16) {
         new->CR67 = 0x50;
         }
      else if (pScrn->bitsPerPixel == 24) { 
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         S3VInitSTREAMS(pScrn, new->STREAMS, mode);
	 new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
         }
      else if (pScrn->bitsPerPixel == 32) { 
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         S3VInitSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0x10000;            /* Still more FIFO slots */
         }
      S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 3, 
	135000, 270000, &new->SR13, &new->SR12);
      } /* end great big if()... */


   /* Now adjust the value of the FIFO based upon options specified */
   if( ps3v->fifo_moderate ) {
      if(pScrn->bitsPerPixel < 24)
         new->MMPR0 -= 0x8000;
      else 
         new->MMPR0 -= 0x4000;
      }
   else if( ps3v->fifo_aggressive ) {
      if(pScrn->bitsPerPixel < 24)
         new->MMPR0 -= 0xc000;
      else 
         new->MMPR0 -= 0x6000;
      }
	     
   /* If we have an interlace mode, set the interlace bit. Note that mode
    * vertical timings are already adjusted by the standard VGA code 
    */
   if(mode->Flags & V_INTERLACE) {
        new->CR42 = 0x20; /* Set interlace mode */
        }
   else {
        new->CR42 = 0x00;
        }

   if(S3_ViRGE_GX2_SERIES(ps3v->Chipset) || 
      S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
       new->CR34 = 0;
     }
   else
     {
       /* Set display fifo */
       new->CR34 = 0x10;  
     }
   /* Now we adjust registers for extended mode timings */
   /* This is taken without change from the accel/s3_virge code */

   i = ((((mode->CrtcHTotal >> 3) - 5) & 0x100) >> 8) |
       ((((mode->CrtcHDisplay >> 3) - 1) & 0x100) >> 7) |
       ((((mode->CrtcHSyncStart >> 3) - 1) & 0x100) >> 6) |
       ((mode->CrtcHSyncStart & 0x800) >> 7);

   if ((mode->CrtcHSyncEnd >> 3) - (mode->CrtcHSyncStart >> 3) > 64)
      i |= 0x08;   /* add another 64 DCLKs to blank pulse width */

   if ((mode->CrtcHSyncEnd >> 3) - (mode->CrtcHSyncStart >> 3) > 32)
      i |= 0x20;   /* add another 32 DCLKs to hsync pulse width */

   /* video playback chokes if sync start and display end are equal */
   if (mode->CrtcHSyncStart - mode->CrtcHDisplay < ps3v->HorizScaleFactor) {
       int tmp = vganew->CRTC[4] + ((i&0x10)<<4) + ps3v->HorizScaleFactor;
       vganew->CRTC[4] = tmp & 0xff;
       i |= ((tmp >> 4) & 0x10);
   }
		      
   j = (  vganew->CRTC[0] + ((i&0x01)<<8)
        + vganew->CRTC[4] + ((i&0x10)<<4) + 1) / 2;

   if (j-(vganew->CRTC[4] + ((i&0x10)<<4)) < 4) {
      if (vganew->CRTC[4] + ((i&0x10)<<4) + 4 <= vganew->CRTC[0]+ ((i&0x01)<<8))
         j = vganew->CRTC[4] + ((i&0x10)<<4) + 4;
      else
         j = vganew->CRTC[0]+ ((i&0x01)<<8) + 1;
   }
   new->CR3B = j & 0xFF;
   i |= (j & 0x100) >> 2;
   new->CR3C = (vganew->CRTC[0] + ((i&0x01)<<8))/2;
   new->CR5D = i;

   new->CR5E = (((mode->CrtcVTotal - 2) & 0x400) >> 10)  |
               (((mode->CrtcVDisplay - 1) & 0x400) >> 9) |
               (((mode->CrtcVSyncStart) & 0x400) >> 8)   |
               (((mode->CrtcVSyncStart) & 0x400) >> 6)   | 0x40;

   
   width = (pScrn->displayWidth * (pScrn->bitsPerPixel / 8))>> 3;
   vganew->CRTC[19] = 0xFF & width;
   new->CR51 = (0x300 & width) >> 4; /* Extension bits */

   /* Set doublescan */
   if( mode->Flags & V_DBLSCAN)
     vganew->CRTC[9] |= 0x80;
  
   /* And finally, select clock source 2 for programmable PLL */
   vganew->MiscOutReg |= 0x0c;      


   new->CR33 = 0x20;
   if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) || S3_ViRGE_GX2_SERIES(ps3v->Chipset) 
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) ) 
   {
     new->CR85 = 0x12;  /* avoid sreen flickering */
      /* by increasing FIFO filling, larger # fills FIFO from memory earlier */
      /* on GX2 this affects all depths, not just those running STREAMS. */
      /* new, secondary stream settings. */
      new->CR87 = 0x10;
      /* gx2 - set up in XV init code */
      new->CR92 = 0x00;
      new->CR93 = 0x00;
      /* gx2 primary mclk timeout, def=0xb */
      new->CR7B = 0xb;
      /* gx2 secondary mclk timeout, def=0xb */
      new->CR7D = 0xb;
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      new->CR86 = 0x80;  /* disable DAC power saving to avoid bright left edge */
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset) || S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      int dbytes = pScrn->displayWidth * ((pScrn->bitsPerPixel+7)/8);
      new->CR91 =   (dbytes + 7) / 8;
      new->CR90 = (((dbytes + 7) / 8) >> 8) | 0x80;
   }
	 

   /* Now we handle various XConfig memory options and others */
   
   VGAOUT8(vgaCRIndex, 0x36);
   new->CR36 = VGAIN8(vgaCRReg);
   /* option "slow_edodram" sets EDO to 2 cycle mode on ViRGE */
   if (ps3v->Chipset == S3_ViRGE) {
      if( ps3v->slow_edodram )
         new->CR36 = (new->CR36 & 0xf3) | 0x08;
      else  
         new->CR36 &= 0xf3;
      }
   
   /* Option "fpm_vram" for ViRGE_VX sets memory in fast page mode */
   if (ps3v->Chipset == S3_ViRGE_VX) {
      if( ps3v->fpm_vram )
         new->CR36 |=  0x0c;
      else 
         new->CR36 &= ~0x0c;
   }
      
   				/* S3_INVERT_VCLK was defaulted to 0 	*/
				/* in 3.3.3 and never changed. 		*/
				/* Also, bit 0 is never set in 3.9Nm,	*/
				/* so I left this out for 4.0.			*/
#if 0
      if (mode->Private[0] & (1 << S3_INVERT_VCLK)) {
	 if (mode->Private[S3_INVERT_VCLK])
	    new->CR67 |= 1;
	 else
	    new->CR67 &= ~1;
      }
#endif
      				/* S3_BLANK_DELAY settings based on 	*/
				/* defaults only. From 3.3.3 		*/
   {
      int blank_delay;
      
      if(ps3v->Chipset == S3_ViRGE_VX)
	    /* these values need to be changed once CR67_1 is set
	       for gamma correction (see S3V server) ! */
	    if (pScrn->bitsPerPixel == 8)
	       blank_delay = 0x00;
	    else if (pScrn->bitsPerPixel == 16)
	       blank_delay = 0x00;
	    else
	       blank_delay = 0x51;
      else
	    if (pScrn->bitsPerPixel == 8)
	       blank_delay = 0x00;
	    else if (pScrn->bitsPerPixel == 16)
	       blank_delay = 0x02;
	    else
	       blank_delay = 0x04;
				
      if (ps3v->Chipset == S3_ViRGE_VX)
	    new->CR6D = blank_delay;
      else {
	    new->CR65 = (new->CR65 & ~0x38) 
	       | (blank_delay & 0x07) << 3;
	    VGAOUT8(vgaCRIndex, 0x6d);
	    new->CR6D = VGAIN8(vgaCRReg);
      }
   }
   				/* S3_EARLY_SC was defaulted to 0 	*/
				/* in 3.3.3 and never changed. 		*/
				/* Also, bit 1 is never set in 3.9Nm,	*/
				/* so I left this out for 4.0.			*/
#if 0
      if (mode->Private[0] & (1 << S3_EARLY_SC)) {
	 if (mode->Private[S3_EARLY_SC])
	    new->CR65 |= 2;
	 else
	    new->CR65 &= ~2;
      }
#endif
  
   VGAOUT8(vgaCRIndex, 0x68);
   new->CR68 = VGAIN8(vgaCRReg);
   new->CR69 = 0;
   
   /* Flat panel centering and expansion registers */
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset) && (ps3v->lcd_center)) {
     new->SR54 = 0x10 ;
     new->SR55 = 0x80 ;
     new->SR56 = 0x10 ;
     new->SR57 = 0x80 ;
   } else {
     new->SR54 = 0x1f ;
     new->SR55 = 0x9f ;
     new->SR56 = 0x1f ;
     new->SR57 = 0xff ;
   }
   
   pScrn->vtSema = TRUE;
   			    
   					/* Do it!  Write the mode registers */
					/* to hardware, start STREAMS if    */
					/* needed, etc.		    	    */
   S3VWriteMode( pScrn, vganew, new );
   					/* Adjust the viewport */
   S3VAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

   return TRUE;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
S3VCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  S3VRegPtr S3VSavePtr = &ps3v->SavedReg;

    					/* Like S3VRestore, but uses passed */
					/* mode registers.		    */
  if (pScrn->vtSema) {
      S3VWriteMode(pScrn, vgaSavePtr, S3VSavePtr);
      vgaHWLock(hwp);
      S3VUnmapMem(pScrn);
  }

  if (ps3v->AccelInfoRec)
    XAADestroyInfoRec(ps3v->AccelInfoRec);
  if (ps3v->DGAModes)
  	xfree(ps3v->DGAModes);

  pScrn->vtSema = FALSE;

  pScreen->CloseScreen = ps3v->CloseScreen;

  return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}




/* Do screen blanking */

/* Mandatory */
static Bool
S3VSaveScreen(ScreenPtr pScreen, int mode)
{
  return vgaHWSaveScreen(pScreen, mode);
}





/* This function inits the STREAMS processor variables. 
 * This has essentially been taken from the accel/s3_virge code and the databook.
 */
static void
S3VInitSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams, DisplayModePtr mode)
{
  PVERB5("	S3VInitSTREAMS\n");
  
  switch (pScrn->bitsPerPixel)
    {
    case 16:
      streams[0] = 0x05000000;
      break;
    case 24:
                         /* data format 8.8.8 (24 bpp) */
      streams[0] = 0x06000000;
      break;
    case 32:
                         /* one more bit for X.8.8.8, 32 bpp */
      streams[0] = 0x07000000;
      break;
    }
                         /* NO chroma keying... */
   streams[1] = 0x0;
                         /* Secondary stream format KRGB-16 */
                         /* data book suggestion... */
   streams[2] = 0x03000000;

   streams[3] = 0x0;

   streams[4] = 0x0;
                         /* use 0x01000000 for primary over second. */
                         /* use 0x0 for second over prim. */
   streams[5] = 0x01000000;

   streams[6] = 0x0;

   streams[7] = 0x0;
                                /* Stride is 3 bytes for 24 bpp mode and */
                                /* 4 bytes for 32 bpp. */
   switch(pScrn->bitsPerPixel)
     {
     case 16:
       streams[8] = 
	 pScrn->displayWidth * 2;
       break;
     case 24:
       streams[8] = 
	 pScrn->displayWidth * 3;
      break;
     case 32:
       streams[8] = 
	 pScrn->displayWidth * 4;
      break;
     }
                                /* Choose fbaddr0 as stream source. */
   streams[9] = 0x0;
   streams[10] = 0x0;
   streams[11] = 0x0;
   streams[12] = 0x1;

                                /* Set primary stream on top of secondary */
                                /* stream. */
   streams[13] = 0xc0000000;
                               /* Vertical scale factor. */
   streams[14] = 0x0;

   streams[15] = 0x0;
                                /* Vertical accum. initial value. */
   streams[16] = 0x0;
                                /* X and Y start coords + 1. */
   streams[18] =  0x00010001;

         /* Specify window Width -1 and Height of */
         /* stream. */
   streams[19] =
         (mode->HDisplay - 1) << 16 |
         (mode->VDisplay);
   
                                /* Book says 0x07ff07ff. */
   streams[20] = 0x07ff07ff;

   streams[21] = 0x00010001;
                            
}


   

/* Used to adjust start address in frame buffer. We use the new 
 * CR69 reg for this purpose instead of the older CR31/CR51 combo.
 * If STREAMS is running, we program the STREAMS start addr. registers. 
 */

void
S3VAdjustFrame(int scrnIndex, int x, int y, int flags)
{
   ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
   vgaHWPtr hwp = VGAHWPTR(pScrn);
   S3VPtr ps3v = S3VPTR(pScrn);
   int Base;
   int vgaCRIndex, vgaCRReg, vgaIOBase;
   vgaIOBase = hwp->IOBase;
   vgaCRIndex = vgaIOBase + 4;
   vgaCRReg = vgaIOBase + 5;

   if(ps3v->ShowCache && y)
	y += pScrn->virtualY - 1;

   if( (ps3v->STREAMSRunning == FALSE) ||
      S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      Base = ((y * pScrn->displayWidth + x)
		* (pScrn->bitsPerPixel / 8)) >> 2;
      if (pScrn->bitsPerPixel == 24) 
	Base = Base+2 - (Base+2) % 3;
      if (pScrn->bitsPerPixel == 16)
	if (S3_TRIO_3D_SERIES(ps3v->Chipset) && pScrn->modes->Clock > 115000)
	  Base &= ~1;

      /* Now program the start address registers */
      VGAOUT16(vgaCRIndex, (Base & 0x00FF00) | 0x0C);
      VGAOUT16(vgaCRIndex, ((Base & 0x00FF) << 8) | 0x0D);
      VGAOUT8(vgaCRIndex, 0x69);
      VGAOUT8(vgaCRReg, (Base & 0x0F0000) >> 16);   
      }
   else {          /* Change start address for STREAMS case */
      VerticalRetraceWait();
      if(ps3v->Chipset == S3_ViRGE_VX)
	OUTREG(PSTREAM_FBADDR0_REG,
		   ((y * pScrn->displayWidth + (x & ~7)) *
		    pScrn->bitsPerPixel / 8));
      else
	OUTREG(PSTREAM_FBADDR0_REG,
		   ((y * pScrn->displayWidth + (x & ~3)) *
		    pScrn->bitsPerPixel / 8));
      }

   return;
}




/* Usually mandatory */
Bool
S3VSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return S3VModeInit(xf86Screens[scrnIndex], mode);
}



void S3VLoadPalette(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indicies,
    LOCO *colors,
    VisualPtr pVisual
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int i, index;

    for(i = 0; i < numColors; i++) {
	index = indicies[i];
        VGAOUT8(0x3c8, index);
        VGAOUT8(0x3c9, colors[index].red);
        VGAOUT8(0x3c9, colors[index].green);
        VGAOUT8(0x3c9, colors[index].blue);
    }
}


/*
 * Functions to support getting a ViRGE card into MMIO mode if it fails to
 * default to MMIO enabled. 
 */

void
S3VEnableMmio(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp;
  S3VPtr ps3v;
  IOADDRESS vgaCRIndex, vgaCRReg;
  unsigned char val;
  
  PVERB5("	S3VEnableMmio\n");
  
  hwp = VGAHWPTR(pScrn);
  ps3v = S3VPTR(pScrn);
  /*
   * enable chipset (seen on uninitialized secondary cards)
   * might not be needed once we use the VGA softbooter
   * (EE 05/04/99)
   */
  vgaHWSetStdFuncs(hwp);
  /*
   * any access to the legacy VGA ports is done here.
   * If legacy VGA is inaccessable the MMIO base _has_
   * to be set correctly already and MMIO _has_ to be
   * enabled.
   */
  val = inb(hwp->PIOOffset + 0x3C3);               /*@@@EE*/
  outb(hwp->PIOOffset + 0x3C3, val | 0x01);
  /*
   * set CR registers to color mode
   * in mono mode extended CR registers
   * are not accessible. (EE 05/04/99)
   */
  val = inb(hwp->PIOOffset + VGA_MISC_OUT_R);      /*@@@EE*/
  outb(hwp->PIOOffset + VGA_MISC_OUT_W, val | 0x01);
  vgaHWGetIOBase(hwp);             	/* Get VGA I/O base */
  vgaCRIndex = hwp->PIOOffset + hwp->IOBase + 4;
  vgaCRReg = vgaCRIndex + 1;
#if 1
  /*
   * set linear base register to the PCI register values
   * some DX chipsets don´t seem to do it automatically
   * (EE 06/03/99)
   */
  outb(vgaCRIndex, 0x59);         /*@@@EE*/
  outb(vgaCRReg, ps3v->PciInfo->memBase[0] >> 24);  
  outb(vgaCRIndex, 0x5A);
  outb(vgaCRReg, ps3v->PciInfo->memBase[0] >> 16);
  outb(vgaCRIndex, 0x53);
#endif
  /* Save register for restore */
  ps3v->EnableMmioCR53 = inb(vgaCRReg);
  			      	/* Enable new MMIO, if TRIO mmio is already */
				/* enabled, then it stays enabled. */
  outb(vgaCRReg, ps3v->EnableMmioCR53 | 0x08);
  outb(hwp->PIOOffset + VGA_MISC_OUT_W, val);
  if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
    outb(vgaCRIndex, 0x40);
    val = inb(vgaCRReg);
    outb(vgaCRReg, val | 1);  
  }
}



void
S3VDisableMmio(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp;
  S3VPtr ps3v;
  IOADDRESS vgaCRIndex, vgaCRReg;
  
  PVERB5("	S3VDisableMmio\n");
  
  hwp = VGAHWPTR(pScrn);
  ps3v = S3VPTR(pScrn);

  vgaCRIndex = hwp->PIOOffset + hwp->IOBase + 4;
  vgaCRReg = vgaCRIndex + 1;
  outb(vgaCRIndex, 0x53);
				/* Restore register's original state */
  outb(vgaCRReg, ps3v->EnableMmioCR53);
  if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
    unsigned char val;
    outb(vgaCRIndex, 0x40);
    val = inb(vgaCRReg);
    outb(vgaCRReg, val | 1);  
  }
}



/* This function is used to debug, it prints out the contents of s3 regs */

static void
S3VPrintRegs(ScrnInfoPtr pScrn)
{
    unsigned char tmp1, tmp2;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    S3VPtr ps3v = S3VPTR(pScrn);
    int vgaCRIndex, vgaCRReg, vgaIOBase, vgaIR;
    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;
    vgaIR = vgaIOBase + 0xa;
       
/* All registers */
/* New formatted registers, matches s3rc (sort of) */
    xf86DrvMsgVerb( pScrn->scrnIndex, X_INFO, VERBLEV, "START register dump ------------------\n");
    xf86ErrorFVerb(VERBLEV, "Misc Out[3CC]\n  ");
    xf86ErrorFVerb(VERBLEV, "%02x\n",VGAIN8(0x3cc));

    xf86ErrorFVerb(VERBLEV, "\nCR[00-2f]\n  ");
    for(tmp1=0x0;tmp1<=0x2f;tmp1++){
	VGAOUT8(vgaCRIndex, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(vgaCRReg));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }
    
    xf86ErrorFVerb(VERBLEV, "\nSR[00-27]\n  ");
    for(tmp1=0x0;tmp1<=0x27;tmp1++){
	VGAOUT8(0x3c4, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(0x3c5));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }
    xf86ErrorFVerb(VERBLEV, "\n"); /* odd hex number of digits... */

    xf86ErrorFVerb(VERBLEV, "\nGr Cont GR[00-0f]\n  ");
    for(tmp1=0x0;tmp1<=0x0f;tmp1++){
	VGAOUT8(0x3ce, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(0x3cf));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }

    xf86ErrorFVerb(VERBLEV, "\nAtt Cont AR[00-1f]\n  ");
    VGAIN8(vgaIR); /* preset AR flip-flop by reading 3DA, ignore return value */
    tmp2=VGAIN8(0x3c0) & 0x20;
    for(tmp1=0x0;tmp1<=0x1f;tmp1++){
    VGAIN8(vgaIR); /* preset AR flip-flop by reading 3DA, ignore return value */
	VGAOUT8(0x3c0, (tmp1 & ~0x20) | tmp2);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(0x3c1));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }

    xf86ErrorFVerb(VERBLEV, "\nCR[30-6f]\n  ");
    for(tmp1=0x30;tmp1<=0x6f;tmp1++){
	VGAOUT8(vgaCRIndex, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(vgaCRReg));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }
    
    xf86ErrorFVerb(VERBLEV, "\n");
    xf86DrvMsgVerb( pScrn->scrnIndex, X_INFO, VERBLEV, "END register dump --------------------\n");
}

/* this is just a debugger hook */
/*
void print_subsys_stat(void *s3vMmioMem);
void
print_subsys_stat(void *s3vMmioMem)
{
  ErrorF("IN_SUBSYS_STAT() = %x\n", IN_SUBSYS_STAT());
  return;
}
*/

/*
 * S3VDisplayPowerManagementSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 */
static void
S3VDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
			     int flags)
{
  S3VPtr ps3v;
  unsigned char sr8 = 0x0, srd = 0x0;
  char modestr[][40] = { "On","Standby","Suspend","Off" };

  ps3v = S3VPTR(pScrn);
  
  /* unlock extended sequence registers */

  VGAOUT8(0x3c4, 0x08);
  sr8 = VGAIN8(0x3c5);
  sr8 |= 0x6;
  VGAOUT8(0x3c5, sr8);

  /* load SRD */
  VGAOUT8(0x3c4, 0x0d);
  srd = VGAIN8(0x3c5);
  
  srd &= 0x03; /* clear the sync control bits of srd */
  
  switch (PowerManagementMode) {
  case DPMSModeOn:
    /* Screen: On; HSync: On, VSync: On */
    break;
  case DPMSModeStandby:
    /* Screen: Off; HSync: Off, VSync: On */
    srd |= 0x10;
    break;
  case DPMSModeSuspend:
    /* Screen: Off; HSync: On, VSync: Off */
    srd |= 0x40;
    break;
  case DPMSModeOff:
    /* Screen: Off; HSync: Off, VSync: Off */
    srd |= 0x50;
    break;
  default:
    xf86ErrorFVerb(VERBLEV, "Invalid PowerManagementMode %d passed to S3VDisplayPowerManagementSet\n", PowerManagementMode);
    break;
  }

  VGAOUT8(0x3c4, 0x0d);
  VGAOUT8(0x3c5, srd);

  xf86ErrorFVerb(VERBLEV, "Power Manag: set:%s\n", 
		 modestr[PowerManagementMode]);
  
  return;
}

static unsigned int
S3Vddc1Read(ScrnInfoPtr pScrn)
{
    register vgaHWPtr hwp = VGAHWPTR(pScrn);
    register CARD32 tmp;
    S3VPtr ps3v = S3VPTR(pScrn);

    while (hwp->readST01(hwp)&0x8) {};
    while (!(hwp->readST01(hwp)&0x8)) {};

    tmp = (INREG(DDC_REG));
    return ((unsigned int) (tmp & 0x08));
}

static Bool
S3Vddc1(int scrnIndex)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    S3VPtr ps3v = S3VPTR(pScrn);
    CARD32 tmp;
    Bool success = FALSE;
    xf86MonPtr pMon;
    
    /* initialize chipset */
    tmp = INREG(DDC_REG);
    OUTREG(DDC_REG,(tmp | 0x12));
    
    if ((pMon = xf86PrintEDID(
	xf86DoEDID_DDC1(scrnIndex,vgaHWddc1SetSpeed,S3Vddc1Read))) != NULL)
	success = TRUE;
    xf86SetDDCproperties(pScrn,pMon);

    /* undo initialization */
    OUTREG(DDC_REG,(tmp));
    return success;
}

static Bool
S3Vddc2(int scrnIndex)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    S3VPtr ps3v = S3VPTR(pScrn);
    
    if ( xf86LoadSubModule(pScrn, "i2c") ) {
	xf86LoaderReqSymLists(i2cSymbols,NULL);
	if (S3V_I2CInit(pScrn)) {
	    CARD32 tmp = (INREG(DDC_REG));
	    OUTREG(DDC_REG,(tmp | 0x13));
	    xf86SetDDCproperties(pScrn,xf86PrintEDID(
		xf86DoEDID_DDC2(pScrn->scrnIndex,ps3v->I2C)));
	    OUTREG(DDC_REG,tmp);
	    return TRUE;
	}
    }
    return FALSE;
}

static void
S3VProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;
    if (xf86LoadSubModule(pScrn, "vbe")) {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}

/*EOF*/


