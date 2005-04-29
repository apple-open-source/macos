/* $XConsortium: nv_driver.c /main/3 1996/10/28 05:13:37 kaleb $ */
/*
 * Copyright 1996-1997  David J. McKay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID J. MCKAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Hacked together from mga driver and 3.3.4 NVIDIA driver by Jarno Paananen
   <jpaana@s2.org> */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_driver.c,v 1.123 2004/01/13 19:03:28 mvojkovi Exp $ */

#include "nv_include.h"

#include "xf86int10.h"

const   OptionInfoRec * RivaAvailableOptions(int chipid, int busid);
Bool    RivaGetScrnInfoRec(PciChipsets *chips, int chip);

/*
 * Forward definitions for the functions that make up the driver.
 */
/* Mandatory functions */
static const OptionInfoRec * NVAvailableOptions(int chipid, int busid);
static void    NVIdentify(int flags);
static Bool    NVProbe(DriverPtr drv, int flags);
static Bool    NVPreInit(ScrnInfoPtr pScrn, int flags);
static Bool    NVScreenInit(int Index, ScreenPtr pScreen, int argc,
                            char **argv);
static Bool    NVEnterVT(int scrnIndex, int flags);
static Bool    NVEnterVTFBDev(int scrnIndex, int flags);
static void    NVLeaveVT(int scrnIndex, int flags);
static Bool    NVCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool    NVSaveScreen(ScreenPtr pScreen, int mode);

/* Optional functions */
static void    NVFreeScreen(int scrnIndex, int flags);
static ModeStatus NVValidMode(int scrnIndex, DisplayModePtr mode,
			      Bool verbose, int flags);

/* Internally used functions */

static Bool	NVMapMem(ScrnInfoPtr pScrn);
static Bool	NVMapMemFBDev(ScrnInfoPtr pScrn);
static Bool	NVUnmapMem(ScrnInfoPtr pScrn);
static void	NVSave(ScrnInfoPtr pScrn);
static void	NVRestore(ScrnInfoPtr pScrn);
static Bool	NVModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);


/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec NV = {
        NV_VERSION,
	NV_DRIVER_NAME,
        NVIdentify,
        NVProbe,
	NVAvailableOptions,
        NULL,
        0
};

static SymTabRec NVKnownChipsets[] =
{
  { 0x12D20018, "RIVA 128" },
  { 0x10DE0020, "RIVA TNT" },
  { 0x10DE0028, "RIVA TNT2" },
  { 0x10DE002A, "Unknown TNT2" },
  { 0x10DE002C, "Vanta" },
  { 0x10DE0029, "RIVA TNT2 Ultra" },
  { 0x10DE002D, "RIVA TNT2 Model 64" },
  { 0x10DE00A0, "Aladdin TNT2" },
  { 0x10DE0100, "GeForce 256" },
  { 0x10DE0101, "GeForce DDR" },
  { 0x10DE0103, "Quadro" },
  { 0x10DE0110, "GeForce2 MX/MX 400" },
  { 0x10DE0111, "GeForce2 MX 100/200" },
  { 0x10DE0112, "GeForce2 Go" },
  { 0x10DE0113, "Quadro2 MXR/EX/Go" },
  { 0x10DE01A0, "GeForce2 Integrated GPU" },
  { 0x10DE0150, "GeForce2 GTS" },
  { 0x10DE0151, "GeForce2 Ti" },
  { 0x10DE0152, "GeForce2 Ultra" },
  { 0x10DE0153, "Quadro2 Pro" },
  { 0x10DE0170, "GeForce4 MX 460" },
  { 0x10DE0171, "GeForce4 MX 440" },
  { 0x10DE0172, "GeForce4 MX 420" },
  { 0x10DE0173, "GeForce4 MX 440-SE" },
  { 0x10DE0174, "GeForce4 440 Go" },
  { 0x10DE0175, "GeForce4 420 Go" },
  { 0x10DE0176, "GeForce4 420 Go 32M" },
  { 0x10DE0177, "GeForce4 460 Go" },
#if defined(__powerpc__)
  { 0x10DE0179, "GeForce4 MX (Mac)" },
#else
  { 0x10DE0179, "GeForce4 440 Go 64M" },
#endif
  { 0x10DE017D, "GeForce4 410 Go 16M" },
  { 0x10DE017C, "Quadro4 500 GoGL" },
  { 0x10DE0178, "Quadro4 550 XGL" },
  { 0x10DE017A, "Quadro4 NVS" },
  { 0x10DE0181, "GeForce4 MX 440 with AGP8X" },
  { 0x10DE0182, "GeForce4 MX 440SE with AGP8X" },
  { 0x10DE0183, "GeForce4 MX 420 with AGP8X" },
  { 0x10DE0186, "GeForce4 448 Go" },
  { 0x10DE0187, "GeForce4 488 Go" },
  { 0x10DE0188, "Quadro4 580 XGL" },
#if defined(__powerpc__)
  { 0x10DE0189, "GeForce4 MX with AGP8X (Mac)" },
#endif
  { 0x10DE018A, "Quadro4 280 NVS" },
  { 0x10DE018B, "Quadro4 380 XGL" },
  { 0x10DE01F0, "GeForce4 MX Integrated GPU" },
  { 0x10DE0200, "GeForce3" },
  { 0x10DE0201, "GeForce3 Ti 200" },
  { 0x10DE0202, "GeForce3 Ti 500" },
  { 0x10DE0203, "Quadro DCC" },
  { 0x10DE0250, "GeForce4 Ti 4600" },
  { 0x10DE0251, "GeForce4 Ti 4400" },
  { 0x10DE0252, "0x0252" },
  { 0x10DE0253, "GeForce4 Ti 4200" },
  { 0x10DE0258, "Quadro4 900 XGL" },
  { 0x10DE0259, "Quadro4 750 XGL" },
  { 0x10DE025B, "Quadro4 700 XGL" },
  { 0x10DE0280, "GeForce4 Ti 4800" },
  { 0x10DE0281, "GeForce4 Ti 4200 with AGP8X" },
  { 0x10DE0282, "GeForce4 Ti 4800 SE" },
  { 0x10DE0286, "GeForce4 4200 Go" },
  { 0x10DE028C, "Quadro4 700 GoGL" },
  { 0x10DE0288, "Quadro4 980 XGL" },
  { 0x10DE0289, "Quadro4 780 XGL" },
  { 0x10DE0301, "GeForce FX 5800 Ultra" },
  { 0x10DE0302, "GeForce FX 5800" },
  { 0x10DE0308, "Quadro FX 2000" },
  { 0x10DE0309, "Quadro FX 1000" },
  { 0x10DE0311, "GeForce FX 5600 Ultra" },
  { 0x10DE0312, "GeForce FX 5600" },
  { 0x10DE0313, "0x0313"},
  { 0x10DE0314, "GeForce FX 5600SE" },
  { 0x10DE0316, "0x0316" },
  { 0x10DE0317, "0x0317" },
  { 0x10DE031A, "GeForce FX Go5600" },
  { 0x10DE031B, "GeForce FX Go5650" },
  { 0x10DE031C, "Quadro FX Go700" },
  { 0x10DE031D, "0x031D" },
  { 0x10DE031E, "0x031E" },
  { 0x10DE031F, "0x031F" },
  { 0x10DE0320, "GeForce FX 5200" },
  { 0x10DE0321, "GeForce FX 5200 Ultra" },
  { 0x10DE0322, "GeForce FX 5200" },
  { 0x10DE0323, "GeForce FX 5200SE" },
  { 0x10DE0324, "GeForce FX Go5200" },
  { 0x10DE0325, "GeForce FX Go5250" },
  { 0x10DE0328, "GeForce FX Go5200 32M/64M" },
#if defined(__powerpc__)
  { 0x10DE0329, "GeForce FX 5200 (Mac)" },
#else
  { 0x10DE0329, "0x0329" },
#endif
  { 0x10DE032A, "Quadro NVS 280 PCI" },
  { 0x10DE032B, "Quadro FX 500" },
  { 0x10DE032C, "GeForce FX Go5300" },
  { 0x10DE032D, "GeForce FX Go5100" },
  { 0x10DE032F, "0x032F" },
  { 0x10DE0330, "GeForce FX 5900 Ultra" },
  { 0x10DE0331, "GeForce FX 5900" },
  { 0x10DE0332, "GeForce FX 5900XT" },
  { 0x10DE0333, "GeForce FX 5950 Ultra" },
  { 0x10DE0334, "0x0334" },
  { 0x10DE0338, "Quadro FX 3000" },
  { 0x10DE0341, "GeForce FX 5700 Ultra" },
  { 0x10DE0342, "GeForce FX 5700" },
  { 0x10DE0343, "GeForce FX 5700LE" },
  { 0x10DE0344, "GeForce FX 5700VE" },
  { 0x10DE0345, "0x0345" },
  { 0x10DE0347, "GeForce FX Go5700" },
  { 0x10DE0348, "GeForce FX Go5700" },
  { 0x10DE0349, "0x0349" },
  { 0x10DE034B, "0x034B" },
  { 0x10DE034C, "Quadro FX Go1000" },
  { 0x10DE034E, "Quadro FX 1100" },
  { 0x10DE034F, "0x034F" },
  {-1, NULL}
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
    "vgaHWUnmapMem",
    "vgaHWDPMSSet",
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWMapMem",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAFallbackOps",
    "XAAInit",
    "XAAPatternROP",
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
    "xf86DoEDID_DDC2",
    "xf86SetDDCproperties",
    NULL
};

#ifdef XFree86LOADER
static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeFree",
    "vbeDoEDID",
    NULL
};
#endif

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

static const char *fbdevHWSymbols[] = {
    "fbdevHWInit",
    "fbdevHWUseBuildinMode",

    "fbdevHWGetVidmem",

    /* colormap */
    "fbdevHWLoadPalette",

    /* ScrnInfo hooks */
    "fbdevHWAdjustFrame",
    "fbdevHWEnterVT",
    "fbdevHWLeaveVT",
    "fbdevHWModeInit",
    "fbdevHWSave",
    "fbdevHWSwitchMode",
    "fbdevHWValidMode",

    "fbdevHWMapMMIO",
    "fbdevHWMapVidmem",

    NULL
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

static const char *rivaSymbols[] = {
   "RivaGetScrnInfoRec",
   "RivaAvailableOptions",
    NULL
};


#ifdef XFree86LOADER

static MODULESETUPPROTO(nvSetup);

static XF86ModuleVersionInfo nvVersRec =
{
    "nv",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    NV_MAJOR_VERSION, NV_MINOR_VERSION, NV_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,                     /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0,0,0,0}
};

XF86ModuleData nvModuleData = { &nvVersRec, nvSetup, NULL };
#endif


typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL,
    OPTION_SHADOW_FB,
    OPTION_FBDEV,
    OPTION_ROTATE,
    OPTION_VIDEO_KEY,
    OPTION_FLAT_PANEL,
    OPTION_FP_DITHER,
    OPTION_CRTC_NUMBER
} NVOpts;


static const OptionInfoRec NVOptions[] = {
    { OPTION_SW_CURSOR,         "SWcursor",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_HW_CURSOR,         "HWcursor",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOACCEL,           "NoAccel",      OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_SHADOW_FB,         "ShadowFB",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_FBDEV,             "UseFBDev",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_ROTATE,		"Rotate",	OPTV_ANYSTR,	{0}, FALSE },
    { OPTION_VIDEO_KEY,		"VideoKey",	OPTV_INTEGER,	{0}, FALSE },
    { OPTION_FLAT_PANEL,	"FlatPanel",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_FP_DITHER, 	"FPDither",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_CRTC_NUMBER,	"CrtcNumber",	OPTV_INTEGER,	{0}, FALSE },
    { -1,                       NULL,           OPTV_NONE,      {0}, FALSE }
};

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;


static Bool
NVGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an NVRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
        return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(NVRec), 1);
    /* Initialise it */

    return TRUE;
}

static void
NVFreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
        return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}


#ifdef XFree86LOADER

static pointer
nvSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    /* This module should be loaded only once, but check to be sure. */

    if (!setupDone) {
        setupDone = TRUE;
        xf86AddDriver(&NV, module, 0);

        /*
         * Modules that this driver always requires may be loaded here
         * by calling LoadSubModule().
         */

        /*
         * Tell the loader about symbols from other modules that this module
         * might refer to.
         */
        LoaderRefSymLists(vgahwSymbols, xaaSymbols, fbSymbols,
                          ramdacSymbols, shadowSymbols, rivaSymbols,
                          i2cSymbols, ddcSymbols, vbeSymbols,
                          fbdevHWSymbols, int10Symbols, NULL);

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

static const OptionInfoRec *
NVAvailableOptions(int chipid, int busid)
{
    if(chipid == 0x12D20018) {
	if (!xf86LoadOneModule("riva128", NULL)) {
	    return NULL;
	} else
	    return RivaAvailableOptions(chipid, busid);
    }
    
    return NVOptions;
}

/* Mandatory */
static void
NVIdentify(int flags)
{
    xf86PrintChipsets(NV_NAME, "driver for NVIDIA chipsets", NVKnownChipsets);
}


static Bool
NVGetScrnInfoRec(PciChipsets *chips, int chip)
{
    ScrnInfoPtr pScrn;

    pScrn = xf86ConfigPciEntity(NULL, 0, chip,
                                chips, NULL, NULL, NULL,
                                NULL, NULL);

    if(!pScrn) return FALSE;

    pScrn->driverVersion    = NV_VERSION;
    pScrn->driverName       = NV_DRIVER_NAME;
    pScrn->name             = NV_NAME;

    pScrn->Probe            = NVProbe;
    pScrn->PreInit          = NVPreInit;
    pScrn->ScreenInit       = NVScreenInit;
    pScrn->SwitchMode       = NVSwitchMode;
    pScrn->AdjustFrame      = NVAdjustFrame;
    pScrn->EnterVT          = NVEnterVT;
    pScrn->LeaveVT          = NVLeaveVT;
    pScrn->FreeScreen       = NVFreeScreen;
    pScrn->ValidMode        = NVValidMode;

    return TRUE;
}

#define MAX_CHIPS MAXSCREENS

/* Mandatory */
static Bool
NVProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    SymTabRec NVChipsets[MAX_CHIPS + 1];
    PciChipsets NVPciChipsets[MAX_CHIPS + 1];
    pciVideoPtr *ppPci;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;


    if ((numDevSections = xf86MatchDevice(NV_DRIVER_NAME, &devSections)) <= 0) 
        return FALSE;  /* no matching device section */

    if (!(ppPci = xf86GetPciVideoInfo())) 
        return FALSE;  /* no PCI cards found */

    numUsed = 0;

    /* Create the NVChipsets and NVPciChipsets from found devices */
    while (*ppPci && (numUsed < MAX_CHIPS)) {
        if(((*ppPci)->vendor == PCI_VENDOR_NVIDIA_SGS) || 
           ((*ppPci)->vendor == PCI_VENDOR_NVIDIA)) 
        {
            SymTabRec *nvchips = NVKnownChipsets;
            int token = ((*ppPci)->vendor << 16) | (*ppPci)->chipType;

            while(nvchips->name) {
               if(token == nvchips->token)
                  break;
               nvchips++;
            }

            if(nvchips->name) { /* found one */
               NVChipsets[numUsed].token = nvchips->token;
               NVChipsets[numUsed].name = nvchips->name;
               NVPciChipsets[numUsed].numChipset = nvchips->token;
               NVPciChipsets[numUsed].PCIid = nvchips->token;
               NVPciChipsets[numUsed].resList = RES_SHARED_VGA;
               numUsed++;
            } else if ((*ppPci)->vendor == PCI_VENDOR_NVIDIA) {
               /* look for a compatible devices which may be newer than 
                  the NVKnownChipsets list above.  */
               switch(token & 0xfff0) {
               case 0x0170:   
               case 0x0180:
               case 0x0250:
               case 0x0280:
               case 0x0300:
               case 0x0310:
               case 0x0320:
               case 0x0330:
               case 0x0340:
                   NVChipsets[numUsed].token = token;
                   NVChipsets[numUsed].name = "Unknown NVIDIA chip";
                   NVPciChipsets[numUsed].numChipset = token;
                   NVPciChipsets[numUsed].PCIid = token;
                   NVPciChipsets[numUsed].resList = RES_SHARED_VGA;
                   numUsed++;
                   break;
               default:  break;  /* we don't recognize it */
               }
            }
        }
        ppPci++;
    }

    /* terminate the list */
    NVChipsets[numUsed].token = -1;
    NVChipsets[numUsed].name = NULL; 
    NVPciChipsets[numUsed].numChipset = -1;
    NVPciChipsets[numUsed].PCIid = -1;
    NVPciChipsets[numUsed].resList = RES_UNDEFINED;

    numUsed = xf86MatchPciInstances(NV_NAME, 0, NVChipsets, NVPciChipsets,
                                    devSections, numDevSections, drv,
                                    &usedChips);
                        
    if (numUsed <= 0) 
        return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
        pciVideoPtr pPci;

        pPci = xf86GetPciInfoForEntity(usedChips[i]);
        if(pPci->vendor == PCI_VENDOR_NVIDIA_SGS) {
            if (!xf86LoadDrvSubModule(drv, "riva128")) {
                  continue;
            }
            xf86LoaderReqSymLists(rivaSymbols, NULL);
            if(RivaGetScrnInfoRec(NVPciChipsets, usedChips[i]))
                foundScreen = TRUE;
        } else {
            if(NVGetScrnInfoRec(NVPciChipsets, usedChips[i])) 
	        foundScreen = TRUE;
	}    
    }

    xfree(devSections);
    xfree(usedChips);

    return foundScreen;
}

/* Usually mandatory */
Bool
NVSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return NVModeInit(xf86Screens[scrnIndex], mode);
}

/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
void 
NVAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    int startAddr;
    NVPtr pNv = NVPTR(pScrn);
    NVFBLayout *pLayout = &pNv->CurrentLayout;

    startAddr = (((y*pLayout->displayWidth)+x)*(pLayout->bitsPerPixel/8));
    NVSetStartAddress(pNv, startAddr);
}


/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */

/* Mandatory */
static Bool
NVEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NVPtr pNv = NVPTR(pScrn);

    if (!NVModeInit(pScrn, pScrn->currentMode))
        return FALSE;
    NVAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    if(pNv->overlayAdaptor)
        NVResetVideo(pScrn);
    return TRUE;
}

static Bool
NVEnterVTFBDev(int scrnIndex, int flags)
{
    fbdevHWEnterVT(scrnIndex,flags);
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
NVLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NVPtr pNv = NVPTR(pScrn);

    NVSync(pScrn);
    NVRestore(pScrn);
    NVLockUnlock(pNv, 1);
}



static void 
NVBlockHandler (
    int i, 
    pointer blockData, 
    pointer pTimeout,
    pointer pReadmask
)
{
    ScreenPtr     pScreen = screenInfo.screens[i];
    ScrnInfoPtr   pScrnInfo = xf86Screens[i];
    NVPtr         pNv = NVPTR(pScrnInfo);

    if (pNv->DMAKickoffCallback)
        (*pNv->DMAKickoffCallback)(pScrnInfo);
    
    pScreen->BlockHandler = pNv->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = NVBlockHandler;

    if (pNv->VideoTimerCallback) 
        (*pNv->VideoTimerCallback)(pScrnInfo, currentTime.milliseconds);

}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
NVCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NVPtr pNv = NVPTR(pScrn);

    if (pScrn->vtSema) {
        NVSync(pScrn);
        NVRestore(pScrn);
        NVLockUnlock(pNv, 1);
    }

    NVUnmapMem(pScrn);
    vgaHWUnmapMem(pScrn);
    if (pNv->AccelInfoRec)
        XAADestroyInfoRec(pNv->AccelInfoRec);
    if (pNv->CursorInfoRec)
        xf86DestroyCursorInfoRec(pNv->CursorInfoRec);
    if (pNv->ShadowPtr)
        xfree(pNv->ShadowPtr);
    if (pNv->DGAModes)
        xfree(pNv->DGAModes);
    if (pNv->overlayAdaptor)
	xfree(pNv->overlayAdaptor);
    if (pNv->blitAdaptor)
        xfree(pNv->blitAdaptor);

    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = pNv->CloseScreen;
    pScreen->BlockHandler = pNv->BlockHandler;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/* Free up any persistent data structures */

/* Optional */
static void
NVFreeScreen(int scrnIndex, int flags)
{
    /*
     * This only gets called when a screen is being deleted.  It does not
     * get called routinely at the end of a server generation.
     */
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    NVFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static ModeStatus
NVValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    return (MODE_OK);
}

static void
nvProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}


Bool NVI2CInit(ScrnInfoPtr pScrn)
{
    char *mod = "i2c";

    if (xf86LoadSubModule(pScrn, mod)) {
        xf86LoaderReqSymLists(i2cSymbols,NULL);

        mod = "ddc";
        if(xf86LoadSubModule(pScrn, mod)) {
            xf86LoaderReqSymLists(ddcSymbols, NULL);
            return NVDACi2cInit(pScrn);
        } 
    }

    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
              "Couldn't load %s module.  DDC probing can't be done\n", mod);

    return FALSE;
}

/* Mandatory */
Bool
NVPreInit(ScrnInfoPtr pScrn, int flags)
{
    NVPtr pNv;
    MessageType from;
    int i, max_width, max_height;
    ClockRangePtr clockRanges;
    const char *s;

    if (flags & PROBE_DETECT) {
        nvProbeDDC( pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index );
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

    /* Check the number of entities, and fail if it isn't one. */
    if (pScrn->numEntities != 1)
	return FALSE;

    /* Allocate the NVRec driverPrivate */
    if (!NVGetRec(pScrn)) {
	return FALSE;
    }
    pNv = NVPTR(pScrn);

    /* Get the entity, and make sure it is PCI. */
    pNv->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pNv->pEnt->location.type != BUS_PCI)
	return FALSE;
 
    /* Find the PCI info for this screen */
    pNv->PciInfo = xf86GetPciInfoForEntity(pNv->pEnt->index);
    pNv->PciTag = pciTag(pNv->PciInfo->bus, pNv->PciInfo->device,
			  pNv->PciInfo->func);

    pNv->Primary = xf86IsPrimaryPci(pNv->PciInfo);

    /* Initialize the card through int10 interface if needed */
    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86LoaderReqSymLists(int10Symbols, NULL);
#if !defined(__alpha__) && !defined(__powerpc__)
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing int10\n");
        pNv->pInt = xf86InitInt10(pNv->pEnt->index);
#endif
    }
   
    xf86SetOperatingState(resVgaIo, pNv->pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMem, pNv->pEnt->index, ResDisableOpr);

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * Set the Chipset and ChipRev, allowing config file entries to
     * override.
     */
    if (pNv->pEnt->device->chipset && *pNv->pEnt->device->chipset) {
	pScrn->chipset = pNv->pEnt->device->chipset;
        pNv->Chipset = xf86StringToToken(NVKnownChipsets, pScrn->chipset);
        from = X_CONFIG;
    } else if (pNv->pEnt->device->chipID >= 0) {
	pNv->Chipset = pNv->pEnt->device->chipID;
	pScrn->chipset = (char *)xf86TokenToString(NVKnownChipsets, 
                                                   pNv->Chipset);
	from = X_CONFIG;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
		   pNv->Chipset);
    } else {
	from = X_PROBED;
	pNv->Chipset = (pNv->PciInfo->vendor << 16) | pNv->PciInfo->chipType;
	pScrn->chipset = (char *)xf86TokenToString(NVKnownChipsets, 
                                                   pNv->Chipset);
        if(!pScrn->chipset)
          pScrn->chipset = "Unknown NVIDIA chipset";
    }
    if (pNv->pEnt->device->chipRev >= 0) {
	pNv->ChipRev = pNv->pEnt->device->chipRev;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
		   pNv->ChipRev);
    } else {
	pNv->ChipRev = pNv->PciInfo->chipRev;
    }

    /*
     * This shouldn't happen because such problems should be caught in
     * NVProbe(), but check it just in case.
     */
    if (pScrn->chipset == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", pNv->Chipset);
	xf86FreeInt10(pNv->pInt);
	return FALSE;
    }
    if (pNv->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
	xf86FreeInt10(pNv->pInt);
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);


    /*
     * The first thing we should figure out is the depth, bpp, etc.
     */

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
	xf86FreeInt10(pNv->pInt);
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
		xf86FreeInt10(pNv->pInt);
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
	    xf86FreeInt10(pNv->pInt);
	    return FALSE;
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	xf86FreeInt10(pNv->pInt);
	return FALSE;
    } else {
	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && (pScrn->defaultVisual != TrueColor)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported at depth %d\n",
		       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	    xf86FreeInt10(pNv->pInt);
	    return FALSE;
	}
    }

    /* The vgahw module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "vgahw")) {
	xf86FreeInt10(pNv->pInt);
	return FALSE;
    }
    
    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn)) {
	xf86FreeInt10(pNv->pInt);
	return FALSE;
    }
    
    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pNv->Options = xalloc(sizeof(NVOptions))))
	return FALSE;
    memcpy(pNv->Options, NVOptions, sizeof(NVOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pNv->Options);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
	pScrn->rgbBits = 8;

    from = X_DEFAULT;
    pNv->HWCursor = TRUE;
    /*
     * The preferred method is to use the "hw cursor" option as a tri-state
     * option, with the default set above.
     */
    if (xf86GetOptValBool(pNv->Options, OPTION_HW_CURSOR, &pNv->HWCursor)) {
	from = X_CONFIG;
    }
    /* For compatibility, accept this too (as an override) */
    if (xf86ReturnOptValBool(pNv->Options, OPTION_SW_CURSOR, FALSE)) {
	from = X_CONFIG;
	pNv->HWCursor = FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pNv->HWCursor ? "HW" : "SW");
    if (xf86ReturnOptValBool(pNv->Options, OPTION_NOACCEL, FALSE)) {
	pNv->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }
    if (xf86ReturnOptValBool(pNv->Options, OPTION_SHADOW_FB, FALSE)) {
	pNv->ShadowFB = TRUE;
	pNv->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Using \"Shadow Framebuffer\" - acceleration disabled\n");
    }
    if (xf86ReturnOptValBool(pNv->Options, OPTION_FBDEV, FALSE)) {
	pNv->FBDev = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Using framebuffer device\n");
    }
    if (pNv->FBDev) {
	/* check for linux framebuffer device */
	if (!xf86LoadSubModule(pScrn, "fbdevhw")) {
	    xf86FreeInt10(pNv->pInt);
	    return FALSE;
	}
	
	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);
	if (!fbdevHWInit(pScrn, pNv->PciInfo, NULL)) {
	    xf86FreeInt10(pNv->pInt);
	    return FALSE;
	}
	pScrn->SwitchMode    = fbdevHWSwitchMode;
	pScrn->AdjustFrame   = fbdevHWAdjustFrame;
	pScrn->EnterVT       = NVEnterVTFBDev;
	pScrn->LeaveVT       = fbdevHWLeaveVT;
	pScrn->ValidMode     = fbdevHWValidMode;
    }
    pNv->Rotate = 0;
    if ((s = xf86GetOptValString(pNv->Options, OPTION_ROTATE))) {
      if(!xf86NameCmp(s, "CW")) {
	pNv->ShadowFB = TRUE;
	pNv->NoAccel = TRUE;
	pNv->HWCursor = FALSE;
	pNv->Rotate = 1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Rotating screen clockwise - acceleration disabled\n");
      } else
      if(!xf86NameCmp(s, "CCW")) {
	pNv->ShadowFB = TRUE;
	pNv->NoAccel = TRUE;
	pNv->HWCursor = FALSE;
	pNv->Rotate = -1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Rotating screen counter clockwise - acceleration disabled\n");
      } else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"\"%s\" is not a valid value for Option \"Rotate\"\n", s);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		"Valid options are \"CW\" or \"CCW\"\n");
      }
    }
    if(xf86GetOptValInteger(pNv->Options, OPTION_VIDEO_KEY, &(pNv->videoKey))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
                                pNv->videoKey);
    } else {
        pNv->videoKey =  (1 << pScrn->offset.red) | 
                          (1 << pScrn->offset.green) |
        (((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue); 
    }

    if (xf86GetOptValBool(pNv->Options, OPTION_FLAT_PANEL, &(pNv->FlatPanel))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "forcing %s usage\n",
                   pNv->FlatPanel ? "DFP" : "CRTC");
    } else {
        pNv->FlatPanel = -1;   /* autodetect later */
    }

    pNv->FPDither = FALSE;
    if (xf86GetOptValBool(pNv->Options, OPTION_FP_DITHER, &(pNv->FPDither))) 
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "enabling flat panel dither\n");

    if (xf86GetOptValInteger(pNv->Options, OPTION_CRTC_NUMBER,
                             &pNv->CRTCnumber)) 
    {
	if((pNv->CRTCnumber < 0) || (pNv->CRTCnumber > 1)) {
           pNv->CRTCnumber = -1;
           xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
                      "Invalid CRTC number.  Must be 0 or 1\n");
        }
    } else {
        pNv->CRTCnumber = -1; /* autodetect later */
    }

    
    if (pNv->pEnt->device->MemBase != 0) {
	/* Require that the config file value matches one of the PCI values. */
	if (!xf86CheckPciMemBase(pNv->PciInfo, pNv->pEnt->device->MemBase)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"MemBase 0x%08lX doesn't match any PCI base register.\n",
		pNv->pEnt->device->MemBase);
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
	pNv->FbAddress = pNv->pEnt->device->MemBase;
	from = X_CONFIG;
    } else {
	if (pNv->PciInfo->memBase[1] != 0) {
	    pNv->FbAddress = pNv->PciInfo->memBase[1] & 0xff800000;
	    from = X_PROBED;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No valid FB address in PCI config space\n");
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
	       (unsigned long)pNv->FbAddress);

    if (pNv->pEnt->device->IOBase != 0) {
	/* Require that the config file value matches one of the PCI values. */
	if (!xf86CheckPciMemBase(pNv->PciInfo, pNv->pEnt->device->IOBase)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"IOBase 0x%08lX doesn't match any PCI base register.\n",
		pNv->pEnt->device->IOBase);
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
	pNv->IOAddress = pNv->pEnt->device->IOBase;
	from = X_CONFIG;
    } else {
	if (pNv->PciInfo->memBase[0] != 0) {
	    pNv->IOAddress = pNv->PciInfo->memBase[0] & 0xffffc000;
	    from = X_PROBED;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"No valid MMIO address in PCI config space\n");
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX\n",
	       (unsigned long)pNv->IOAddress);
     
    if (xf86RegisterResources(pNv->pEnt->index, NULL, ResExclusive)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"xf86RegisterResources() found resource conflicts\n");
	xf86FreeInt10(pNv->pInt);
	NVFreeRec(pScrn);
	return FALSE;
    }

    pNv->alphaCursor = ((pNv->Chipset & 0x0ff0) >= 0x0110);

    pNv->Architecture = (pNv->Chipset & 0x0f00) >> 4;

    if(pNv->Architecture < NV_ARCH_10)
        pNv->Architecture = NV_ARCH_04;

    NVCommonSetup(pScrn);

    if (pNv->FBDev) {
       pScrn->videoRam = fbdevHWGetVidmem(pScrn)/1024;
    } else {
       pScrn->videoRam = pNv->RamAmountKBytes;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "VideoRAM: %d kBytes\n",
               pScrn->videoRam);
	
    pNv->FbMapSize = pScrn->videoRam * 1024;

    /*
     * If the driver can do gamma correction, it should call xf86SetGamma()
     * here.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    xf86FreeInt10(pNv->pInt);
	    return FALSE;
	}
    }

    pNv->FbUsableSize = pNv->FbMapSize - (128 * 1024);
    pNv->ScratchBufferSize = (pNv->Architecture < NV_ARCH_10) ? 8192 : 16384;
    pNv->ScratchBufferStart = pNv->FbUsableSize - pNv->ScratchBufferSize;

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */

    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pNv->MinVClockFreqKHz;
    clockRanges->maxClock = pNv->MaxVClockFreqKHz;
    clockRanges->clockIndex = -1;		/* programmable */
    if(((pNv->Chipset & 0x0ff0) <= 0x0100) ||
       ((pNv->Chipset & 0x0ff0) == 0x0150) ||
       ((pNv->Chipset & 0x0ff0) >= 0x0300))
    {
       clockRanges->interlaceAllowed = TRUE;
    } else {
        /* No NV2x chips support interlaced modes and the only
           NV1x chips that do are NV10 and NV15 */
       clockRanges->interlaceAllowed = FALSE;
    }
    clockRanges->doubleScanAllowed = TRUE;

    if(pNv->FlatPanel == 1) {
       clockRanges->interlaceAllowed = FALSE;
       clockRanges->doubleScanAllowed = FALSE;
    }

    if(pNv->Architecture < NV_ARCH_10) {
       max_width = (pScrn->bitsPerPixel > 16) ? 2032 : 2048;
       max_height = 2048;
    } else {
       max_width = (pScrn->bitsPerPixel > 16) ? 4080 : 4096;
       max_height = 4096;
    }

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our NVValidMode() already takes
     * care of this, we don't worry about setting them here.
     */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                          pScrn->display->modes, clockRanges,
                          NULL, 256, max_width,
                          512, 128, max_height,
                          pScrn->display->virtualX,
                          pScrn->display->virtualY,
                          pNv->ScratchBufferStart,
                          LOOKUP_BEST_REFRESH);

    if (i < 1 && pNv->FBDev) {
	fbdevHWUseBuildinMode(pScrn);
	pScrn->displayWidth = pScrn->virtualX; /* FIXME: might be wrong */
	i = 1;
    }
    if (i == -1) {
	xf86FreeInt10(pNv->pInt);
	NVFreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	xf86FreeInt10(pNv->pInt);
	NVFreeRec(pScrn);
	return FALSE;
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

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);


    /*
     * XXX This should be taken into account in some way in the mode valdation
     * section.
     */

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	xf86FreeInt10(pNv->pInt);
	NVFreeRec(pScrn);
	return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);
    
    /* Load XAA if needed */
    if (!pNv->NoAccel) {
	if (!xf86LoadSubModule(pScrn, "xaa")) {
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    /* Load ramdac if needed */
    if (pNv->HWCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    /* Load shadowfb if needed */
    if (pNv->ShadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    xf86FreeInt10(pNv->pInt);
	    NVFreeRec(pScrn);
	    return FALSE;
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    pNv->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pNv->CurrentLayout.depth = pScrn->depth;
    pNv->CurrentLayout.displayWidth = pScrn->displayWidth;
    pNv->CurrentLayout.weight.red = pScrn->weight.red;
    pNv->CurrentLayout.weight.green = pScrn->weight.green;
    pNv->CurrentLayout.weight.blue = pScrn->weight.blue;
    pNv->CurrentLayout.mode = pScrn->currentMode;

    xf86FreeInt10(pNv->pInt);

    pNv->pInt = NULL;
    return TRUE;
}


/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
NVMapMem(ScrnInfoPtr pScrn)
{
    NVPtr pNv;
        
    pNv = NVPTR(pScrn);

    pNv->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
				 pNv->PciTag, pNv->FbAddress,
				 pNv->FbMapSize);
    if (pNv->FbBase == NULL)
	return FALSE;

    pNv->FbStart = pNv->FbBase;

    return TRUE;
}

Bool
NVMapMemFBDev(ScrnInfoPtr pScrn)
{
    NVPtr pNv;

    pNv = NVPTR(pScrn);

    pNv->FbBase = fbdevHWMapVidmem(pScrn);
    if (pNv->FbBase == NULL)
        return FALSE;

    pNv->FbStart = pNv->FbBase;

    return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
NVUnmapMem(ScrnInfoPtr pScrn)
{
    NVPtr pNv;
    
    pNv = NVPTR(pScrn);

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pNv->FbBase, pNv->FbMapSize);
    pNv->FbBase = NULL;
    pNv->FbStart = NULL;

    return TRUE;
}


/*
 * Initialise a new mode. 
 */

static Bool
NVModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg;
    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg;

    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, mode))
	return FALSE;
    pScrn->vtSema = TRUE;

    vgaReg = &hwp->ModeReg;
    nvReg = &pNv->ModeReg;

    if(!NVDACInit(pScrn, mode))
        return FALSE;

    NVLockUnlock(pNv, 0);
    if(pNv->twoHeads) {
        VGA_WR08(pNv->PCIO, 0x03D4, 0x44);
        VGA_WR08(pNv->PCIO, 0x03D5, nvReg->crtcOwner);
        NVLockUnlock(pNv, 0);
    }

    /* Program the registers */
    vgaHWProtect(pScrn, TRUE);

    NVDACRestore(pScrn, vgaReg, nvReg, FALSE);

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* turn on LFB swapping */
    {
	unsigned char tmp;

	VGA_WR08(pNv->PCIO, 0x3d4, 0x46);
	tmp = VGA_RD08(pNv->PCIO, 0x3d5);
	tmp |= (1 << 7);
	VGA_WR08(pNv->PCIO, 0x3d5, tmp);
    }
#endif

    NVResetGraphics(pScrn);

    vgaHWProtect(pScrn, FALSE);

    pNv->CurrentLayout.mode = mode;

    return TRUE;
}

/*
 * Restore the initial (text) mode.
 */
static void 
NVRestore(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg = &hwp->SavedReg;
    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg = &pNv->SavedReg;

    if(pNv->twoHeads) {
        VGA_WR08(pNv->PCIO, 0x03D4, 0x44);
        VGA_WR08(pNv->PCIO, 0x03D5, nvReg->crtcOwner);
        NVLockUnlock(pNv, 0);
    }

    NVLockUnlock(pNv, 0);

    /* Only restore text mode fonts/text for the primary card */
    vgaHWProtect(pScrn, TRUE);
    NVDACRestore(pScrn, vgaReg, nvReg, pNv->Primary);
    vgaHWProtect(pScrn, FALSE);
}

static void NVBacklightEnable(NVPtr pNv,  Bool on)
{
    /* This is done differently on each laptop.  Here we
       define the ones we know for sure. */

#if defined(__powerpc__)
    if((pNv->Chipset == 0x10DE0179) || 
       (pNv->Chipset == 0x10DE0189) || 
       (pNv->Chipset == 0x10DE0329))
    {
       /* NV17,18,34 Apple iMac, iBook, PowerBook */
      CARD32 tmp_pmc, tmp_pcrt;
      tmp_pmc = pNv->PMC[0x10F0/4] & 0x7FFFFFFF;
      tmp_pcrt = pNv->PCRTC0[0x081C/4] & 0xFFFFFFFC;
      if(on) {
          tmp_pmc |= (1 << 31);
          tmp_pcrt |= 0x1;
      }
      pNv->PMC[0x10F0/4] = tmp_pmc;
      pNv->PCRTC0[0x081C/4] = tmp_pcrt;
    }
#endif
}

static void
NVDPMSSetLCD(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
  NVPtr pNv = NVPTR(pScrn);

  if (!pScrn->vtSema) return;

  switch (PowerManagementMode) {
  case DPMSModeStandby:  /* HSync: Off, VSync: On */
  case DPMSModeSuspend:  /* HSync: On, VSync: Off */
  case DPMSModeOff:      /* HSync: Off, VSync: Off */
    NVBacklightEnable(pNv, 0);
    break;
  case DPMSModeOn:       /* HSync: On, VSync: On */
    NVBacklightEnable(pNv, 1);
  default:
    break;
  }
  vgaHWDPMSSet(pScrn, PowerManagementMode, flags);
}


static void
NVDPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
  unsigned char crtc1A;
  vgaHWPtr hwp = VGAHWPTR(pScrn);

  if (!pScrn->vtSema) return;

  crtc1A = hwp->readCrtc(hwp, 0x1A) & ~0xC0;

  switch (PowerManagementMode) {
  case DPMSModeStandby:  /* HSync: Off, VSync: On */
    crtc1A |= 0x80;
    break;
  case DPMSModeSuspend:  /* HSync: On, VSync: Off */
    crtc1A |= 0x40;
    break;
  case DPMSModeOff:      /* HSync: Off, VSync: Off */
    crtc1A |= 0xC0;
    break;
  case DPMSModeOn:       /* HSync: On, VSync: On */
  default:
    break;
  }

  /* vgaHWDPMSSet will merely cut the dac output */
  vgaHWDPMSSet(pScrn, PowerManagementMode, flags);

  hwp->writeCrtc(hwp, 0x1A, crtc1A);
}


/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
NVScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    NVPtr pNv;
    int ret;
    VisualPtr visual;
    unsigned char *FBStart;
    int width, height, displayWidth, offscreenHeight;
    BoxRec AvailFBArea;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    hwp = VGAHWPTR(pScrn);
    pNv = NVPTR(pScrn);

    /* Map the NV memory and MMIO areas */
    if (pNv->FBDev) {
        if (!NVMapMemFBDev(pScrn)) {
            return FALSE;
        }
    } else {
        if (!NVMapMem(pScrn)) {
            return FALSE;
         }
    }

    /* Map the VGA memory when the primary video */
    if (pNv->Primary && !pNv->FBDev) {
	hwp->MapSize = 0x10000;
	if (!vgaHWMapMem(pScrn))
	    return FALSE;
    }

    if (pNv->FBDev) {
	fbdevHWSave(pScrn);
	if (!fbdevHWModeInit(pScrn, pScrn->currentMode))
	    return FALSE;
    } else {
	/* Save the current state */
	NVSave(pScrn);
	/* Initialise the first mode */
	if (!NVModeInit(pScrn, pScrn->currentMode))
	    return FALSE;
    }

    /* Darken the screen for aesthetic reasons and set the viewport */
    NVSaveScreen(pScreen, SCREEN_SAVER_ON);
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

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

    if (!miSetVisualTypes(pScrn->depth, 
                          miGetDefaultVisualMask(pScrn->depth), 8,
                          pScrn->defaultVisual))
	  return FALSE;
    if (!miSetPixmapDepths ()) return FALSE;

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    width = pScrn->virtualX;
    height = pScrn->virtualY;
    displayWidth = pScrn->displayWidth;


    if(pNv->Rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    }

    if(pNv->ShadowFB) {
 	pNv->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pNv->ShadowPtr = xalloc(pNv->ShadowPitch * height);
	displayWidth = pNv->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pNv->ShadowPtr;
    } else {
	pNv->ShadowPtr = NULL;
	FBStart = pNv->FbStart;
    }

    switch (pScrn->bitsPerPixel) {
        case 8:
        case 16:
        case 32:
            ret = fbScreenInit(pScreen, FBStart, width, height,
                               pScrn->xDpi, pScrn->yDpi,
                               displayWidth, pScrn->bitsPerPixel);
            break;
        default:
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "Internal error: invalid bpp (%d) in NVScreenInit\n",
                       pScrn->bitsPerPixel);
            ret = FALSE;
            break;
    }
    if (!ret)
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

    fbPictureInit (pScreen, 0, 0);
    
    xf86SetBlackWhitePixels(pScreen);

    if(!pNv->ShadowFB) /* hardware cursor needs to wrap this layer */
	NVDGAInit(pScreen);

    offscreenHeight = pNv->ScratchBufferStart /
                     (pScrn->displayWidth * pScrn->bitsPerPixel >> 3);
    if(offscreenHeight > 32767)
        offscreenHeight = 32767;

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = offscreenHeight;
    xf86InitFBManager(pScreen, &AvailFBArea);
    
    if (!pNv->NoAccel)
	NVAccelInit(pScreen);
    
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* Initialize software cursor.  
	Must precede creation of the default colormap */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
	Must follow software cursor initialization*/
    if (pNv->HWCursor) { 
	if(!NVCursorInit(pScreen))
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		"Hardware cursor initialization failed\n");
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    /* Initialize colormap layer.  
	Must follow initialization of the default colormap */
    if(!xf86HandleColormaps(pScreen, 256, 8,
	(pNv->FBDev ? fbdevHWLoadPalette : NVDACLoadPalette), 
	NULL, CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR))
	return FALSE;

    if(pNv->ShadowFB) {
	RefreshAreaFuncPtr refreshArea = NVRefreshArea;

	if(pNv->Rotate) {
   	   pNv->PointerMoved = pScrn->PointerMoved;
	   pScrn->PointerMoved = NVPointerMoved;

	   switch(pScrn->bitsPerPixel) {
               case 8:	refreshArea = NVRefreshArea8;	break;
               case 16:	refreshArea = NVRefreshArea16;	break;
               case 32:	refreshArea = NVRefreshArea32;	break;
	   }
           xf86DisableRandR();
           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                      "Driver rotation enabled, RandR disabled\n");
	}

	ShadowFBInit(pScreen, refreshArea);
    }

    if(pNv->FlatPanel)
       xf86DPMSInit(pScreen, NVDPMSSetLCD, 0);
    else
       xf86DPMSInit(pScreen, NVDPMSSet, 0);
    
    pScrn->memPhysBase = pNv->FbAddress;
    pScrn->fbOffset = 0;

    if(pNv->Rotate == 0)
       NVInitVideo(pScreen);

    pScreen->SaveScreen = NVSaveScreen;

    /* Wrap the current CloseScreen function */
    pNv->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = NVCloseScreen;

    pNv->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = NVBlockHandler;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }
    return TRUE;
}

static Bool
NVSaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}

static void
NVSave(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg = &pNv->SavedReg;
    vgaHWPtr pVga = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg = &pVga->SavedReg;

    NVDACSave(pScrn, vgaReg, nvReg, pNv->Primary);
}

