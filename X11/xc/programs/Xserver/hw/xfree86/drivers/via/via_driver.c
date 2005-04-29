/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_driver.c,v 1.29 2004/02/20 21:46:35 dawes Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*************************************************************************
 *
 *  File:       via_driver.c
 *  Content:    XFree86 4.0 for VIA/S3G UniChrom
 *
 ************************************************************************/

#include "xf86RAC.h"
#include "shadowfb.h"

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"


#include "via_driver.h"
#include "via_video.h"
#include "videodev.h"
#include "via_swov.h"

#include "ddmpeg.h"
#include "via_capture.h"
#include "via.h"
#ifdef XF86DRI
#include "dri.h"
#endif

/*
 * prototypes
 */

static void VIAIdentify(int flags);
static Bool VIAProbe(DriverPtr drv, int flags);
static Bool VIAPreInit(ScrnInfoPtr pScrn, int flags);
static Bool VIAEnterVT(int scrnIndex, int flags);
static void VIALeaveVT(int scrnIndex, int flags);
static void VIASave(ScrnInfoPtr pScrn);
static void VIAWriteMode(ScrnInfoPtr pScrn, vgaRegPtr, VIARegPtr);
static Bool VIAModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool VIACloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool VIASaveScreen(ScreenPtr pScreen, int mode);
static void VIALoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
                           LOCO *colors, VisualPtr pVisual);
static Bool VIAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
                          char **argv);
static int VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static void VIAFreeScreen(int scrnIndex, int flags);
static ModeStatus VIAValidMode(int index, DisplayModePtr mode,
                               Bool verbose, int flags);
static void VIADPMS(ScrnInfoPtr pScrn, int mode, int flags);
static const OptionInfoRec * VIAAvailableOptions(int chipid, int busid);


static void VIAEnableMMIO(ScrnInfoPtr pScrn);
static void VIADisableMMIO(ScrnInfoPtr pScrn);
static Bool VIAMapMMIO(ScrnInfoPtr pScrn);
static Bool VIAMapFB(ScrnInfoPtr pScrn);
static void VIAUnmapMem(ScrnInfoPtr pScrn);
Bool VIADeviceSelection(ScrnInfoPtr pScrn);
Bool VIADeviceDispatch(ScrnInfoPtr pScrn);

#ifdef XF86DRI
void VIAInitialize3DEngine(ScrnInfoPtr pScrn);
#endif

DriverRec VIA =
{
    VIA_VERSION,
    DRIVER_NAME,
    VIAIdentify,
    VIAProbe,
    VIAAvailableOptions,
    NULL,
    0
};


/* Supported chipsets */

static SymTabRec VIAChipsets[] = {
    {VIA_CLE266,   "CLE266"},
    {VIA_KM400,    "KM400"},
    {VIA_K8M800,   "K8M800"},
    {-1,            NULL }
};


/* This table maps a PCI device ID to a chipset family identifier. */

static PciChipsets VIAPciChipsets[] = {
    {VIA_CLE266,   PCI_CHIP_CLE3122,   RES_SHARED_VGA},
    {VIA_CLE266,   PCI_CHIP_CLE3022,   RES_SHARED_VGA},
    {VIA_KM400,    PCI_CHIP_VT7205,    RES_SHARED_VGA},
    {VIA_KM400,    PCI_CHIP_VT3205,    RES_SHARED_VGA},
    {VIA_K8M800,   PCI_CHIP_VT7204,    RES_SHARED_VGA},
    {VIA_K8M800,   PCI_CHIP_VT3204,    RES_SHARED_VGA},
    {-1,            -1,                RES_UNDEFINED}
};

int gVIAEntityIndex = -1;

typedef enum {
    OPTION_A2,
    OPTION_PCI_BURST,
    OPTION_PCI_RETRY,
    OPTION_NOACCEL,
    OPTION_SWCURSOR,
    OPTION_HWCURSOR,
    OPTION_SHADOW_FB,
    OPTION_ROTATE,
    OPTION_USEBIOS,
    OPTION_VIDEORAM,
    OPTION_ACTIVEDEVICE,
    OPTION_LCDDUALEDGE,
    OPTION_BUSWIDTH,
    OPTION_CENTER,
    OPTION_PANELSIZE,
    OPTION_TVDOTCRAWL,
    OPTION_TVTYPE,
    OPTION_TVOUTPUT,
    OPTION_TVVSCAN,
    OPTION_TVHSCALE,
    OPTION_TVENCODER,
    OPTION_REFRESH,
    OPTION_DISABLEVQ,
    OPTION_NODDCVALUE,
    OPTION_CAP0_DEINTERLACE,
    OPTION_CAP1_DEINTERLACE,
    OPTION_CAP0_FIELDSWAP,
    OPTION_DRIXINERAMA
} VIAOpts;


static OptionInfoRec VIAOptions[] =
{
    {OPTION_A2,         "A2",           OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_NOACCEL,    "NoAccel",      OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_HWCURSOR,   "HWCursor",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_SWCURSOR,   "SWCursor",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_SHADOW_FB,  "ShadowFB",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_ROTATE,     "Rotate",       OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_USEBIOS,    "UseBIOS",      OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_VIDEORAM,   "VideoRAM",     OPTV_INTEGER, {0}, FALSE},
    {OPTION_ACTIVEDEVICE,     "ActiveDevice",       OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_LCDDUALEDGE,    "LCDDualEdge",  OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_BUSWIDTH,	"BusWidth",  	OPTV_ANYSTR, {0}, FALSE},
    {OPTION_CENTER,     "Center",       OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_PANELSIZE,  "PanelSize",    OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVDOTCRAWL, "TVDotCrawl",   OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_TVTYPE,     "TVType",       OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVOUTPUT,   "TVOutput",     OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVVSCAN,    "TVVScan",      OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVHSCALE,   "TVHScale",     OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVENCODER,  "TVEncoder",    OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_REFRESH,    "Refresh",      OPTV_INTEGER, {0}, FALSE},
    {OPTION_DISABLEVQ,  "DisableVQ",    OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_NODDCVALUE, "NoDDCValue",   OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_CAP0_DEINTERLACE, "Cap0Deinterlace",    OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_CAP1_DEINTERLACE, "Cap1Deinterlace",    OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_CAP0_FIELDSWAP, "Cap0FieldSwap",    OPTV_BOOLEAN,  {0}, FALSE},
    {OPTION_DRIXINERAMA,  "DRIXINERAMA",    OPTV_BOOLEAN, {0}, FALSE},
    {-1,                NULL,           OPTV_NONE,    {0}, FALSE}
};


static const char *vgaHWSymbols[] = {
    "vgaHWGetHWRec",
    "vgaHWSetMmioFuncs",
    "vgaHWSetStdFuncs",
    "vgaHWGetIOBase",
    "vgaHWSave",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWMapMem",
    "vgaHWUnmapMem",
    "vgaHWInit",
    "vgaHWSaveScreen",
    "vgaHWLock",
    "vgaHWUnlock",
    "vgaHWFreeHWRec",
    NULL
};


static const char *ramdacSymbols[] = {
    "xf86InitCursor",
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86InterpretEDID",
    "xf86PrintEDID",
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    "xf86SetDDCproperties",
    NULL
};


static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    "xf86CreateI2CDevRec",
    "xf86I2CDevInit",
    "xf86I2CWriteRead",
    "xf86I2CProbeAddress",
    "xf86DestroyI2CDevRec",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACopyROP_PM",
    "XAAPatternROP",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    "XAAFillSolidRects",
    NULL
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

#ifdef USE_FB
static const char *fbSymbols[] = {
    "fbScreenInit",
    "fbPictureInit",
    NULL
};
#else
static const char *cfbSymbols[] = {
    "cfbScreenInit",
    "cfb16ScreenInit",
    "cfb24ScreenInit",
    "cfb24_32ScreenInit",
    "cfb32ScreenInit",
    "cfb16BresS",
    "cfb24BresS",
    NULL
};
#endif

#ifdef XFree86LOADER
#ifdef XF86DRI
static const char *drmSymbols[] = {
    "drmAddBufs",
    "drmAddMap",
    "drmAgpAcquire",
    "drmAgpAlloc",
    "drmAgpBase",
    "drmAgpBind",
    "drmAgpDeviceId",
    "drmAgpEnable",
    "drmAgpFree",
    "drmAgpGetMode",
    "drmAgpRelease",
    "drmAgpVendorId",
    "drmCtlInstHandler",
    "drmCtlUninstHandler",
    "drmCommandNone",
    "drmCommandWrite",
    "drmFreeVersion",
    "drmGetInterruptFromBusID",
    "drmGetLibVersion",
    "drmGetVersion",
    "drmMap",
    "drmMapBufs",
    "drmUnmap",
    "drmUnmapBufs",
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

static MODULESETUPPROTO(VIASetup);

static XF86ModuleVersionInfo VIAVersRec = {
    "via",
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

XF86ModuleData viaModuleData = {&VIAVersRec, VIASetup, NULL};


static pointer VIASetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
        setupDone = TRUE;
        xf86AddDriver(&VIA, module, 0);
        LoaderRefSymLists(vgaHWSymbols,
#ifdef USE_FB
                          fbSymbols,
#else
                          cfbSymbols,
#endif
                          ramdacSymbols,
                          xaaSymbols,
                          shadowSymbols,
                          vbeSymbols,
                          i2cSymbols,
                          ddcSymbols,
                          /*
                          mpegSymbols,
                          */
#ifdef XF86DRI
              			  drmSymbols,
              			  driSymbols,
#endif
                          NULL);

        return (pointer) 1;
    }
    else {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;

        return NULL;
    }
} /* VIASetup */

#endif /* XFree86LOADER */

static void viaFillGraphicInfo(ScrnInfoPtr pScrn)
{
	VIAPtr pVia = VIAPTR(pScrn);
	VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

        pVia->graphicInfo.TotalVRAM = pVia->videoRambytes;
        pVia->graphicInfo.VideoHeapBase = (unsigned long) pVia->FBFreeStart;
        pVia->graphicInfo.VideoHeapEnd = (unsigned long) (pVia->FBFreeEnd - 1);
        pVia->graphicInfo.dwWidth = pBIOSInfo->CrtcHDisplay;
        pVia->graphicInfo.dwHeight = pBIOSInfo->CrtcVDisplay;
        pVia->graphicInfo.dwBPP = pScrn->bitsPerPixel;
        pVia->graphicInfo.dwPitch = (((pScrn->virtualX) + 15) & ~15) * (pScrn->bitsPerPixel) / 8;
        pVia->graphicInfo.dwRefreshRate = (unsigned long)pBIOSInfo->FoundRefresh;
        pVia->graphicInfo.dwDVIOn = pBIOSInfo->DVIAttach;
        pVia->graphicInfo.dwExpand = pBIOSInfo->scaleY;
        pVia->graphicInfo.dwPanelWidth = pBIOSInfo->panelX;
        pVia->graphicInfo.dwPanelHeight = pBIOSInfo->panelY;
        pVia->graphicInfo.Cap0_Deinterlace = pVia->Cap0_Deinterlace;
        pVia->graphicInfo.Cap1_Deinterlace = pVia->Cap1_Deinterlace;
        pVia->graphicInfo.Cap0_FieldSwap = pVia->Cap0_FieldSwap;
        pVia->graphicInfo.RevisionID = pVia->ChipRev;

        /* for SAMM mode passing screen info */
        pVia->graphicInfo.HasSecondary = pBIOSInfo->HasSecondary;
        pVia->graphicInfo.IsSecondary = pBIOSInfo->IsSecondary;

        /* Added to pass DRM info to V4L */
#ifdef XF86DRI
        pVia->graphicInfo.DRMEnabled = pVia->directRenderingEnabled;
#endif
}

static int
WaitIdleCLE266(VIAPtr pVia)
{
    int loop = 0;

    mem_barrier();

    while (!(VIAGETREG(VIA_REG_STATUS) & VIA_VR_QUEUE_BUSY) && (loop++ < MAXLOOP))
        ;

    while ((VIAGETREG(VIA_REG_STATUS) &
          (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY | VIA_3D_ENG_BUSY)) &&
          (loop++ < MAXLOOP))
        ;

    return loop >= MAXLOOP;
}


static Bool VIAGetRec(ScrnInfoPtr pScrn)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAGetRec\n"));
    if (pScrn->driverPrivate)
        return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(VIARec), 1);
    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo =
        xnfcalloc(sizeof(VIABIOSInfoRec), 1);
    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->UserSetting =
        xnfcalloc(sizeof(VIAUserSettingRec), 1);
    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->pModeTable =
        xnfcalloc(sizeof(VIAModeTableRec), 1);

    /* initial value in VIARec */
    ((VIARec *)(pScrn->driverPrivate))->SavedReg.mode = 0xFF;
    ((VIARec *)(pScrn->driverPrivate))->ModeReg.mode = 0xFF;

    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->FirstInit = TRUE;

    return TRUE;

} /* VIAGetRec */


static void VIAFreeRec(ScrnInfoPtr pScrn)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAFreeRec\n"));
    if (!pScrn->driverPrivate)
        return;

    xfree(((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->pModeTable);
    xfree(((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->UserSetting);
    xfree(((VIARec *)(pScrn->driverPrivate))->pBIOSInfo);
    ViaTunerDestroy(pScrn);
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    VIAUnmapMem(pScrn);

} /* VIAFreeRec */


static const OptionInfoRec * VIAAvailableOptions(int chipid, int busid)
{

    return VIAOptions;

} /* VIAAvailableOptions */


static void VIAIdentify(int flags)
{
    xf86PrintChipsets(DRIVER_NAME,
                      "driver for VIA chipsets",
                      VIAChipsets);
} /* VIAIdentify */


static Bool VIAProbe(DriverPtr drv, int flags)
{
    GDevPtr *devSections;
    int     *usedChips;
    int     numDevSections;
    int     numUsed;
    Bool    foundScreen = FALSE;
    int     i;

    /* sanity checks */
    if ((numDevSections = xf86MatchDevice(DRIVER_NAME, &devSections)) <= 0)
        return FALSE;

    if (xf86GetPciVideoInfo() == NULL)
        return FALSE;

    numUsed = xf86MatchPciInstances(DRIVER_NAME,
                                    PCI_VIA_VENDOR_ID,
                                    VIAChipsets,
                                    VIAPciChipsets,
                                    devSections,
                                    numDevSections,
                                    drv,
                                    &usedChips);
    xfree(devSections);

    if (numUsed <= 0)
        return FALSE;

    if (flags & PROBE_DETECT) {
        foundScreen = TRUE;
    }
    else {
        for (i = 0; i < numUsed; i++) {
            ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);
            EntityInfoPtr pEnt;
            if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
                 VIAPciChipsets, 0, 0, 0, 0, 0)))
            {
                pScrn->driverVersion = VIA_VERSION;
                pScrn->driverName = DRIVER_NAME;
                pScrn->name = "VIA";
                pScrn->Probe = VIAProbe;
                pScrn->PreInit = VIAPreInit;
                pScrn->ScreenInit = VIAScreenInit;
                pScrn->SwitchMode = VIASwitchMode;
                pScrn->AdjustFrame = VIAAdjustFrame;
                pScrn->EnterVT = VIAEnterVT;
                pScrn->LeaveVT = VIALeaveVT;
                pScrn->FreeScreen = VIAFreeScreen;
                pScrn->ValidMode = VIAValidMode;
                foundScreen = TRUE;
            }
            /*
            xf86ConfigActivePciEntity(pScrn,
                                      usedChips[i],
                                      VIAPciChipsets,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
            */

            pEnt = xf86GetEntityInfo(usedChips[i]);

            /* CLE266 card support Dual-Head, mark the entity as sharable*/
            if(pEnt->chipset == VIA_CLE266 || pEnt->chipset == VIA_KM400)
            {
                static int instance = 0;
                DevUnion* pPriv;

                xf86SetEntitySharable(usedChips[i]);
                xf86SetEntityInstanceForScreen(pScrn,
                    pScrn->entityList[0], instance);

                if(gVIAEntityIndex < 0)
                {
                    gVIAEntityIndex = xf86AllocateEntityPrivateIndex();
                    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                            gVIAEntityIndex);

                    if (!pPriv->ptr)
                    {
                        VIAEntPtr pVIAEnt;
                        pPriv->ptr = xnfcalloc(sizeof(VIAEntRec), 1);
                        pVIAEnt = pPriv->ptr;
                        pVIAEnt->IsDRIEnabled = FALSE;
                        pVIAEnt->BypassSecondary = FALSE;
                        pVIAEnt->HasSecondary = FALSE;
                        pVIAEnt->IsSecondaryRestored = FALSE;
                    }
                }
                instance++;
            }
            xfree(pEnt);
        }
    }

    xfree(usedChips);

    return foundScreen;

} /* VIAProbe */


static int LookupChipID(PciChipsets* pset, int ChipID)
{
    /* Is there a function to do this for me? */
    while (pset->numChipset >= 0)
    {
        if (pset->PCIid == ChipID)
            return pset->numChipset;

        pset++;
    }

    return -1;

} /* LookupChipID */


static unsigned int
VIAddc1Read(ScrnInfoPtr pScrn)
{
    register vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    register CARD8 tmp;

    while (hwp->readST01(hwp)&0x8) {};
    while (!(hwp->readST01(hwp)&0x8)) {};

    VGAOUT8(0x3c4, 0x26);
    tmp = VGAIN8(0x3c5);
    return ((unsigned int) ((tmp & 0x08) >> 3));
}

static Bool
VIAddc1(int scrnIndex)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr pVia = VIAPTR(pScrn);
    xf86MonPtr pMon;
    CARD8 tmp;
    Bool success = FALSE;

    /* initialize chipset */
    VGAOUT8(0x3c4, 0x26);
    tmp = VGAIN8(0x3c5);
    VGAOUT8(0x3c4, 0x26);
    VGAOUT8(0x3c5, (tmp | 0x11));

    if ((pMon = xf86PrintEDID(
        xf86DoEDID_DDC1(scrnIndex,vgaHWddc1SetSpeed,VIAddc1Read))) != NULL)
        success = TRUE;
    xf86SetDDCproperties(pScrn,pMon);

    /* undo initialization */
    VGAOUT8(0x3c4, 0x26);
    VGAOUT8(0x3c5, tmp);
    return success;
}

static Bool
VIAddc2(int scrnIndex)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr pVia = VIAPTR(pScrn);
    xf86MonPtr pMon;
    CARD8 tmp;
    Bool success = FALSE;

    VGAOUT8(0x3c4, 0x26);
    tmp = VGAIN8(0x3c5);
    pMon = xf86DoEDID_DDC2(pScrn->scrnIndex, pVia->I2C_Port1);
    if (pMon)
        success = TRUE;
    pVia->DDC1 =  pMon;
    xf86PrintEDID(pMon);
    xf86SetDDCproperties(pScrn, pMon);
    VGAOUT8(0x3c4, 0x26);
    VGAOUT8(0x3c5, tmp);

    return success;
}

static void
VIAProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
    }
}


static Bool VIAPreInit(ScrnInfoPtr pScrn, int flags)
{
    EntityInfoPtr   pEnt;
    VIAPtr          pVia;
    VIABIOSInfoPtr  pBIOSInfo;
    MessageType     from = X_DEFAULT;
    ClockRangePtr   clockRanges;
    char            *s = NULL;
#ifndef USE_FB
    char            *mod = NULL;
    const char      *reqSym = NULL;
#endif
    vgaHWPtr        hwp;
    int             i, bMemSize = 0, tmp;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAPreInit\n"));

    if (pScrn->numEntities > 1)
        return FALSE;

    if (flags & PROBE_DETECT)
        return FALSE;

    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;

    xf86LoaderReqSymLists(vgaHWSymbols, NULL);
    if (!vgaHWGetHWRec(pScrn))
        return FALSE;

#if 0
    /* Here we can alter the number of registers saved and restored by the
     * standard vgaHWSave and Restore routines.
     */
    vgaHWSetRegCounts(pScrn, VGA_NUM_CRTC, VGA_NUM_SEQ, VGA_NUM_GFX, VGA_NUM_ATTR);
#endif

    if (!VIAGetRec(pScrn)) {
        return FALSE;
    }

    pVia = VIAPTR(pScrn);
    pBIOSInfo = pVia->pBIOSInfo;

    pVia->IsSecondary = FALSE;
    pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pEnt->resources) {
        xfree(pEnt);
        VIAFreeRec(pScrn);
        return FALSE;
    }

    pVia->EntityIndex = pEnt->index;

    if(xf86IsEntityShared(pScrn->entityList[0]))
    {
        if(xf86IsPrimInitDone(pScrn->entityList[0]))
        {
            DevUnion* pPriv;
            VIAEntPtr pVIAEnt;
            VIAPtr pVia1;

            pVia->IsSecondary = TRUE;
            pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                    gVIAEntityIndex);
            pVIAEnt = pPriv->ptr;
            if(pVIAEnt->BypassSecondary) return FALSE;
            pVIAEnt->pSecondaryScrn = pScrn;
            pVIAEnt->HasSecondary = TRUE;
            pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
            pVia1->HasSecondary = TRUE;
	    pVia->sharedData = pVia1->sharedData;
        }
        else
        {
            DevUnion* pPriv;
            VIAEntPtr pVIAEnt;

            xf86SetPrimInitDone(pScrn->entityList[0]);
            pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                    gVIAEntityIndex);
	    pVia->sharedData = xnfcalloc(sizeof(ViaSharedRec),1);
            pVIAEnt = pPriv->ptr;
            pVIAEnt->pPrimaryScrn = pScrn;
            pVIAEnt->IsDRIEnabled = FALSE;
            pVIAEnt->BypassSecondary = FALSE;
            pVIAEnt->HasSecondary = FALSE;
            pVIAEnt->RestorePrimary = FALSE;
            pVIAEnt->IsSecondaryRestored = FALSE;
        }
    } else {
	pVia->sharedData = xnfcalloc(sizeof(ViaSharedRec),1);
    }

    if (flags & PROBE_DETECT) {
        VIAProbeDDC(pScrn, pVia->EntityIndex);
        return TRUE;
    }

    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * We support depths of 8, 16 and 24.
     * We support bpp of 8, 16, and 32.
     */

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
        return FALSE;
    }
    else {
        switch (pScrn->depth) {
        case 8:
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

    if (pScrn->depth == 32) {
        pScrn->depth = 24;
    }

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
    }
    else {
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

    xf86CollectOptions(pScrn, NULL);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
        pScrn->rgbBits = 6;

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, VIAOptions);

#ifdef XF86DRI
    pVia->drixinerama = FALSE;
    if (xf86IsOptionSet(VIAOptions, OPTION_DRIXINERAMA))
        pVia->drixinerama = TRUE;
#else
    if (xf86IsOptionSet(VIAOptions, OPTION_DRIXINERAMA))
    	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
    		"Option: drixinerama ignored, no DRI support compiled into driver.\n");
#endif
    if (xf86ReturnOptValBool(VIAOptions, OPTION_PCI_BURST, FALSE)) {
        pVia->pci_burst = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Option: pci_burst - PCI burst read enabled\n");
    }
    else {
        pVia->pci_burst = FALSE;
    }

    pVia->NoPCIRetry = 1;       /* default */
    if (xf86ReturnOptValBool(VIAOptions, OPTION_PCI_RETRY, FALSE)) {
        if (xf86ReturnOptValBool(VIAOptions, OPTION_PCI_BURST, FALSE)) {
            pVia->NoPCIRetry = 0;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_retry\n");
        }
        else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "\"pci_retry\" option requires \"pci_burst\"\n");
        }
    }

    if (xf86IsOptionSet(VIAOptions, OPTION_SHADOW_FB)) {
        pVia->shadowFB = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: ShadowFB %s.\n",
                   pVia->shadowFB ? "enabled" : "disabled");
    }
    else {
        pVia->shadowFB = FALSE;
    }

    if ((s = xf86GetOptValString(VIAOptions, OPTION_ROTATE))) {
        if (!xf86NameCmp(s, "CW")) {
            /* accel is disabled below for shadowFB */
            pVia->shadowFB = TRUE;
            pVia->rotate = 1;
            pVia->hwcursor = FALSE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Rotating screen clockwise - acceleration disabled\n");
        }
        else if(!xf86NameCmp(s, "CCW")) {
            pVia->shadowFB = TRUE;
            pVia->rotate = -1;
            pVia->hwcursor = FALSE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
                       "counter clockwise - acceleration disabled\n");
        }
        else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
                       "value for Option \"Rotate\"\n", s);
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "Valid options are \"CW\" or \"CCW\"\n");
        }
    }

    if (xf86ReturnOptValBool(VIAOptions, OPTION_NOACCEL, FALSE)) {
        pVia->NoAccel = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Option: NoAccel -Acceleration Disabled\n");
    }
    else {
        pVia->NoAccel = FALSE;
    }

    if (pVia->shadowFB && !pVia->NoAccel) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "HW acceleration not supported with \"shadowFB\".\n");
        pVia->NoAccel = TRUE;
    }

    /*
     * The SWCursor setting takes priority over HWCursor.  The default
     * if neither is specified is HW.
     */

    from = X_DEFAULT;
    pVia->hwcursor = pVia->shadowFB ? FALSE : TRUE;
    if (xf86GetOptValBool(VIAOptions, OPTION_HWCURSOR, &pVia->hwcursor))
        from = X_CONFIG;

    if (xf86ReturnOptValBool(VIAOptions, OPTION_SWCURSOR, FALSE)) {
        pVia->hwcursor = FALSE;
        from = X_CONFIG;
    }

    if (pVia->IsSecondary) pVia->hwcursor = FALSE;

    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
               pVia->hwcursor ? "HW" : "SW");

    if (xf86ReturnOptValBool(VIAOptions, OPTION_A2, FALSE)) {
        pBIOSInfo->A2 = TRUE;
    }
    else {
        pBIOSInfo->A2 = FALSE;
    }

    from = X_DEFAULT;
    if (xf86ReturnOptValBool(VIAOptions, OPTION_USEBIOS, FALSE)) {
        from = X_CONFIG;
        pBIOSInfo->UseBIOS = TRUE;
    }
    else {
        pBIOSInfo->UseBIOS = FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "%ssing video BIOS to set modes\n",
               pBIOSInfo->UseBIOS ? "U" : "Not u" );

    pScrn->videoRam = 0;
    if(xf86GetOptValInteger(VIAOptions, OPTION_VIDEORAM, &pScrn->videoRam)) {
    	xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
    	            "Option: VideoRAM %dkB\n", pScrn->videoRam );
    }

    if (xf86ReturnOptValBool(VIAOptions, OPTION_DISABLEVQ, FALSE)) {
        pVia->VQEnable = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Option: DisableVQ -VQ Disabled\n");
    }
    else {
        pVia->VQEnable = TRUE;
    }

    /* ActiveDevice Option for device selection */
    pBIOSInfo->ActiveDevice = 0x00;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_ACTIVEDEVICE))) {
        if (!xf86NameCmp(s, "CRT,TV") || !xf86NameCmp(s, "TV,CRT")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_CRT1 | VIA_DEVICE_TV;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is CRT and TV.\n");
        }
        else if(!xf86NameCmp(s, "CRT,LCD") || !xf86NameCmp(s, "LCD,CRT")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_CRT1 | VIA_DEVICE_LCD;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is CRT and LCD.\n");
        }
        else if(!xf86NameCmp(s, "CRT,DFP") || !xf86NameCmp(s, "DFP,CRT")
                || !xf86NameCmp(s, "CRT,DVI") || !xf86NameCmp(s, "DVI,CRT")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_CRT1 | VIA_DEVICE_DFP;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is CRT and DFP.\n");
        }
        else if(!xf86NameCmp(s, "TV,DFP") || !xf86NameCmp(s, "DFP,TV")
                || !xf86NameCmp(s, "TV,DVI") || !xf86NameCmp(s, "DVI,TV")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_TV | VIA_DEVICE_DFP;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is TV and DFP.\n");
        }
#if 0
        else if(!xf86NameCmp(s, "DFP,LCD") || !xf86NameCmp(s, "LCD,DFP")
                || !xf86NameCmp(s, "LCD,DVI") || !xf86NameCmp(s, "DVI,LCD")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_DFP | VIA_DEVICE_LCD;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is DFP and LCD.\n");
        }
#endif
        else if(!xf86NameCmp(s, "CRT") || !xf86NameCmp(s, "CRT ONLY")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_CRT1;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is CRT Only.\n");
        }
        else if(!xf86NameCmp(s, "LCD") || !xf86NameCmp(s, "LCD ONLY")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_LCD;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is LCD Only.\n");
        }
        else if(!xf86NameCmp(s, "TV") || !xf86NameCmp(s, "TV ONLY")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_TV;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is TV Only.\n");
        }
        else if(!xf86NameCmp(s, "DFP") || !xf86NameCmp(s, "DFP ONLY")
                || !xf86NameCmp(s, "DVI") || !xf86NameCmp(s, "DVI ONLY")) {
            pBIOSInfo->ActiveDevice = VIA_DEVICE_DFP;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Active Device is DFP Only.\n");
        }
        else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option \"%s\" can't recognize!, Active Device by default.\n", s);
        }
    }

    /* NoDDCValue Option */
    if (xf86ReturnOptValBool(VIAOptions, OPTION_NODDCVALUE, FALSE)) {
        pVia->NoDDCValue = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
         "Option: Not using DDC probed value to set HorizSync & VertRefresh\n");
    }
    else {
        pVia->NoDDCValue = FALSE;
    }

    /* LCDDualEdge Option */
    pBIOSInfo->LCDDualEdge = FALSE;
    if (xf86ReturnOptValBool(VIAOptions, OPTION_LCDDUALEDGE, FALSE)) {
        pBIOSInfo->LCDDualEdge = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
         "Option: Using Dual Edge mode to set LCD\n");
    }
    else {
        pBIOSInfo->LCDDualEdge = FALSE;
    }

    /* Digital Output Bus Width Option */
    pBIOSInfo->BusWidth = VIA_DI_12BIT;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_BUSWIDTH))) {
        if (!xf86NameCmp(s, "12BIT")) {
            pBIOSInfo->BusWidth = VIA_DI_12BIT;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Digital Output Bus Width is 12BIT\n");
        }
        else if (!xf86NameCmp(s, "24BIT")) {
            pBIOSInfo->BusWidth = VIA_DI_24BIT;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Digital Output Bus Width is 24BIT\n");
        }
    }

    /* LCD Center/Expend Option */
    if (xf86ReturnOptValBool(VIAOptions, OPTION_CENTER, FALSE)) {
        pBIOSInfo->Center = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "DVI Center is On\n");
    }
    else {
        pBIOSInfo->Center = FALSE;
    }

    /* Panel Size Option */
    pBIOSInfo->PanelSize = VIA_PANEL_INVALID;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_PANELSIZE))) {
        if (!xf86NameCmp(s, "640x480")) {
            pBIOSInfo->PanelSize = VIA_PANEL6X4;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 640x480\n");
        }
        else if (!xf86NameCmp(s, "800x600")) {
            pBIOSInfo->PanelSize = VIA_PANEL8X6;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 800x600\n");
        }
        else if(!xf86NameCmp(s, "1024x768")) {
            pBIOSInfo->PanelSize = VIA_PANEL10X7;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1024x768\n");
        }
        else if (!xf86NameCmp(s, "1280x768")) {
            pBIOSInfo->PanelSize = VIA_PANEL12X7;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1280x768\n");
        }
        else if (!xf86NameCmp(s, "1280x1024")) {
            pBIOSInfo->PanelSize = VIA_PANEL12X10;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1280x1024\n");
        }
        else if (!xf86NameCmp(s, "1400x1050")) {
            pBIOSInfo->PanelSize = VIA_PANEL14X10;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1400x1050\n");
        }
    }

    /* TV DotCrawl Enable Option */
    if (xf86ReturnOptValBool(VIAOptions, OPTION_TVDOTCRAWL, FALSE)) {
        pBIOSInfo->TVDotCrawl = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "DotCrawl is Enable\n");
    }
    else {
        pBIOSInfo->TVDotCrawl = FALSE;
    }

    pBIOSInfo->TVType = TVTYPE_NONE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVTYPE))) {
        if (!xf86NameCmp(s, "NTSC")) {
            pBIOSInfo->TVType = TVTYPE_NTSC;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is NTSC\n");
        }
        else if(!xf86NameCmp(s, "PAL")) {
            pBIOSInfo->TVType = TVTYPE_PAL;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is PAL\n");
        }
    }

    /* TV out put signal Option */
    pBIOSInfo->TVOutput = TVOUTPUT_NONE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVOUTPUT))) {
        if (!xf86NameCmp(s, "S-Video")) {
            pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is S-Video\n");
        }
        else if(!xf86NameCmp(s, "Composite")) {
            pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is Composite\n");
        }
        else if(!xf86NameCmp(s, "SC")) {
            pBIOSInfo->TVOutput = TVOUTPUT_SC;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is SC\n");
        }
        else if(!xf86NameCmp(s, "RGB")) {
            pBIOSInfo->TVOutput = TVOUTPUT_RGB;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is RGB\n");
        }
        else if(!xf86NameCmp(s, "YCbCr")) {
            pBIOSInfo->TVOutput = TVOUTPUT_YCBCR;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is YCbCr\n");
        }
    }

    /* TV Standard Option */
    pBIOSInfo->TVVScan = VIA_TVNORMAL;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVVSCAN))) {
        if (!xf86NameCmp(s, "under")) {
            pBIOSInfo->TVVScan = VIA_TVNORMAL;
        }
        else if (!xf86NameCmp(s, "over")) {
            pBIOSInfo->TVVScan = VIA_TVOVER;
        }
    }

    pBIOSInfo->TVHScale = VIA_NO_TVHSCALE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVHSCALE))) {
#if 0
        if (!xf86NameCmp(s, "0")) {
            pBIOSInfo->TVHScale = VIA_TVHSCALE0;
        }
        else if (!xf86NameCmp(s, "1")) {
            pBIOSInfo->TVHScale = VIA_TVHSCALE1;
        }
        else if(!xf86NameCmp(s, "2")) {
            pBIOSInfo->TVHScale = VIA_TVHSCALE2;
        }
        else if (!xf86NameCmp(s, "3")) {
            pBIOSInfo->TVHScale = VIA_TVHSCALE3;
        }
        else if (!xf86NameCmp(s, "4")) {
            pBIOSInfo->TVHScale = VIA_TVHSCALE4;
        }
#endif
    }

    /* TV Encoder Type Option */
    pBIOSInfo->TVEncoder = VIA_NONETV;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVENCODER))) {
        if (!xf86NameCmp(s, "VT1621")) {
            pBIOSInfo->TVEncoder = VIA_TV2PLUS;
            pBIOSInfo->TVI2CAdd = 0x40;
        }
        else if(!xf86NameCmp(s, "VT1622")) {
            pBIOSInfo->TVEncoder = VIA_TV3;
            pBIOSInfo->TVI2CAdd = 0x40;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Encoder is VT1622!\n");
        }
        else if(!xf86NameCmp(s, "VT1622A")) {
            pBIOSInfo->TVEncoder = VIA_VT1622A;
            pBIOSInfo->TVI2CAdd = 0x40;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Encoder is VT1622!\n");
        }
        else if(!xf86NameCmp(s, "CH7019")) {
            pBIOSInfo->TVEncoder = VIA_CH7019;
            pBIOSInfo->TVI2CAdd = 0xEA;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Encoder is CH7019!\n");
        }
        else if(!xf86NameCmp(s, "SAA7108")) {
            pBIOSInfo->TVEncoder = VIA_SAA7108;
            pBIOSInfo->TVI2CAdd = 0x88;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Encoder is SAA7108!\n");
        }
        else if(!xf86NameCmp(s, "FS454")) {
            pBIOSInfo->TVEncoder = VIA_FS454;
            pBIOSInfo->TVI2CAdd = 0xD4;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Encoder is FS453/FS454!\n");
        }
    }

    if (xf86GetOptValInteger(VIAOptions, OPTION_REFRESH, &(pBIOSInfo->OptRefresh))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Refresh is %d\n", pBIOSInfo->OptRefresh);
    }

    if (xf86LoadSubModule(pScrn, "int10")) {
        xf86LoaderReqSymLists(int10Symbols, NULL);
        pVia->pInt10 = xf86InitInt10(pEnt->index);
    }

    if (pVia->pInt10 && xf86LoadSubModule(pScrn, "vbe")) {
        xf86LoaderReqSymLists(vbeSymbols, NULL);
        pVia->pVbe = VBEInit(pVia->pInt10, pVia->EntityIndex);
    }

    pVia->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
    xf86RegisterResources(pEnt->index, NULL, ResNone);
    /*
    xf86SetOperatingState(RES_SHARED_VGA, pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMemShared, pEnt->index, ResDisableOpr);
    */

    if (pEnt->device->chipset && *pEnt->device->chipset) {
        pScrn->chipset = pEnt->device->chipset;
        pVia->ChipId = pEnt->device->chipID;
        pVia->Chipset = xf86StringToToken(VIAChipsets, pScrn->chipset);
        from = X_CONFIG;
    } else if (pEnt->device->chipID >= 0) {
        pVia->ChipId = pEnt->device->chipID;
        pVia->Chipset = LookupChipID(VIAPciChipsets, pVia->ChipId);
        pScrn->chipset = (char *)xf86TokenToString(VIAChipsets,
                                                   pVia->Chipset);
        from = X_CONFIG;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
                   pEnt->device->chipID);
    } else {
        from = X_PROBED;
        pVia->ChipId = pVia->PciInfo->chipType;
        pVia->Chipset = LookupChipID(VIAPciChipsets, pVia->ChipId);
        pScrn->chipset = (char *)xf86TokenToString(VIAChipsets,
                                                   pVia->Chipset);
    }

    if (pEnt->device->chipRev >= 0) {
        pVia->ChipRev = pEnt->device->chipRev;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
                   pVia->ChipRev);
    }
    else {
        /*pVia->ChipRev = pVia->PciInfo->chipRev;*/
        /* Read PCI bus 0, dev 0, function 0, index 0xF6 to get chip rev. */
        pVia->ChipRev = pciReadByte(pciTag(0, 0, 0), 0xF6);
    }

    if (pEnt->device->videoRam != 0) {
        if (!pScrn->videoRam)
    	    pScrn->videoRam = pEnt->device->videoRam;
    	else {
        	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
        	"Video Memory Size in Option is %d KB, Detect is %d KB!",
            pScrn->videoRam, pEnt->device->videoRam);
    	}
    }
    pBIOSInfo->Chipset = pVia->Chipset;
    pBIOSInfo->ChipRev = pVia->ChipRev;

    xfree(pEnt);

    VIAvfInitHWDiff(pVia);

    /* maybe throw in some more sanity checks here */

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);

    pVia->PciTag = pciTag(pVia->PciInfo->bus, pVia->PciInfo->device,
                          pVia->PciInfo->func);

    switch (pVia->ChipRev) {
        case 2:
            pBIOSInfo->A2 = TRUE;
            break;
        default:
            pBIOSInfo->A2 = FALSE;
            break;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset Rev.: %d\n", pVia->ChipRev);

    hwp = VGAHWPTR(pScrn);
    vgaHWGetIOBase(hwp);

    if (!VIAMapMMIO(pScrn)) {
        vbeFree(pVia->pVbe);
        return FALSE;
    }

    /* Get BIOS ver. From BIOS Call Function */
    tmp = VIABIOS_GetBIOSVersion(pScrn);
    pBIOSInfo->BIOSMajorVersion = tmp >> 8;
    pBIOSInfo->BIOSMinorVersion = tmp & 0xFF;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS Version is = %d.%d\n", pBIOSInfo->BIOSMajorVersion, pBIOSInfo->BIOSMinorVersion));
    VIABIOS_GetBIOSDate(pScrn);

    if (pBIOSInfo->TVType == TVTYPE_NONE) {
        /* use jumper to determine TV Type */
        VGAOUT8(0x3D4, 0x3B);
        if (VGAIN8(0x3D5) & 0x02) {
            pBIOSInfo->TVType = TVTYPE_PAL;
        }
        else {
            pBIOSInfo->TVType = TVTYPE_NTSC;
        }
    }

    {
        Gamma zeros = {0.0, 0.0, 0.0};

        if (!xf86SetGamma(pScrn, zeros)) {
            vbeFree(pVia->pVbe);
            return FALSE;
        }
    }

    /* Next go on to detect amount of installed ram */
    if (pScrn->videoRam < 16384 || pScrn->videoRam > 65536) {
        bMemSize = VIABIOS_GetVideoMemSize(pScrn);
        if (bMemSize) {
            pScrn->videoRam = bMemSize << 6;
        }
        else {
            if(pVia->Chipset == VIA_CLE266)
                VGAOUT8(0x3C4, 0x34);
            else
            	VGAOUT8(0x3C4, 0x39);
            bMemSize = VGAIN8(0x3c5);
            if (bMemSize > 16 && bMemSize <= 128) {
                pScrn->videoRam = (bMemSize + 1) << 9;
            }
            else if (bMemSize > 0 && bMemSize < 31){
                pScrn->videoRam = bMemSize << 12;
            }
            else {
                DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                "bMemSize = %d\nGet Video Memory Size by default.\n", bMemSize));
                pScrn->videoRam = 16 << 10;	/* Assume the base 16Mb */
            }
        }
    }

    /* Split FB for SAMM */
    /* FIXME: For now, split FB into two equal sections. This should
     *        be able to be adjusted by user with a config option. */
    if (pVia->IsSecondary) {
        DevUnion* pPriv;
        VIAEntPtr pVIAEnt;
        VIAPtr    pVia1;

        pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
              gVIAEntityIndex);
        pVIAEnt = pPriv->ptr;
        pScrn->videoRam = pScrn->videoRam >> 1;
        pVIAEnt->pPrimaryScrn->videoRam = pScrn->videoRam;
        pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
        pVia1->videoRambytes = pScrn->videoRam << 10;
        pVia->FrameBufferBase += (pScrn->videoRam << 10);
    }

    pVia->videoRambytes = pScrn->videoRam << 10;
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,"videoram =  %dk\n",
               pScrn->videoRam);

    /* Set status word positions based on chip type. */

    switch (pVia->Chipset) {
        case VIA_CLE266:
            pVia->myWaitIdle = WaitIdleCLE266;
            break;
        default:
            pVia->myWaitIdle = WaitIdleCLE266;
            break;
    }

    if (!xf86LoadSubModule(pScrn, "i2c")) {
        VIAFreeRec(pScrn);
        return FALSE;
    }
    else {
        xf86LoaderReqSymLists(i2cSymbols,NULL);
        VIAI2CInit(pScrn);
    }

    if (!xf86LoadSubModule(pScrn, "ddc")) {
        VIAFreeRec(pScrn);
        return FALSE;
    }
    else {
        xf86MonPtr pMon = NULL;

        xf86LoaderReqSymLists(ddcSymbols, NULL);
        if ((pVia->pVbe)
            && ((pMon = xf86PrintEDID(vbeDoEDID(pVia->pVbe, NULL))) != NULL)) {
            pVia->DDC1 = pMon;
            xf86SetDDCproperties(pScrn,pMon);
        }
        else if (!VIAddc2(pScrn->scrnIndex)) {
            VIAddc1(pScrn->scrnIndex);
        }
    }

    /* Reset HorizSync & VertRefresh Rang Using DDC Value */
    if (pVia->DDC1 && !pVia->NoDDCValue) {
        int i;
        int h = 0;
        int v = 0;

        for (i = 0; i < DET_TIMINGS; i++) {
            if (pVia->DDC1->det_mon[i].type == DS_RANGES) {
                pScrn->monitor->hsync[h].lo
                    = pVia->DDC1->det_mon[i].section.ranges.min_h;
                pScrn->monitor->hsync[h++].hi
                    = pVia->DDC1->det_mon[i].section.ranges.max_h;
                pScrn->monitor->vrefresh[v].lo
                    = pVia->DDC1->det_mon[i].section.ranges.min_v;
                pScrn->monitor->vrefresh[v++].hi
                    = pVia->DDC1->det_mon[i].section.ranges.max_v;
                break;
            }
        }
        pScrn->monitor->nHsync = h;
        pScrn->monitor->nVrefresh = v;
    }

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */

    clockRanges = xnfalloc(sizeof(ClockRange));
    clockRanges->next = NULL;
    clockRanges->minClock = 20000;
    clockRanges->maxClock = 230000;

    clockRanges->clockIndex = -1;
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = FALSE;


    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our VIAValidMode() already takes
     * care of this, we don't worry about setting them here.
     */

    /* Select valid modes from those available */
    i = xf86ValidateModes(pScrn,
                          pScrn->monitor->Modes,    /* availModes */
                          pScrn->display->modes,    /* modeNames */
                          clockRanges,              /* list of clock ranges */
                          NULL,                     /* list of line pitches */
                          256,                      /* mini line pitch */
                          2048,                     /* max line pitch */
                          16 * pScrn->bitsPerPixel, /* pitch inc (bits) */
                          128,                      /* min height */
                          2048,                     /* max height */
                          pScrn->display->virtualX, /* virtual width */
                          pScrn->display->virtualY, /* virutal height */
                          pVia->videoRambytes,      /* size of video memory */
                          LOOKUP_BEST_REFRESH);     /* lookup mode flags */

    if (i == -1) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86ValidateModes failure\n");
        VIAFreeRec(pScrn);
        return FALSE;
    }

    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
        VIAFreeRec(pScrn);
        return FALSE;
    }
    
    /* Set up screen parameters. */
    pVia->Bpp = pScrn->bitsPerPixel >> 3;
    pVia->Bpl = pScrn->displayWidth * pVia->Bpp;

    if (!VIAGetBIOSTable(pBIOSInfo)) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    pBIOSInfo->I2C_Port1 = pVia->I2C_Port1;
    pBIOSInfo->I2C_Port2 = pVia->I2C_Port2;
    pBIOSInfo->DDC1 = pVia->DDC1;
    pBIOSInfo->DDC2 = pVia->DDC2;

    /* Detect TV Encoder */
    if (!pBIOSInfo->TVEncoder) {
        pBIOSInfo->TVEncoder = VIACheckTVExist(pBIOSInfo);
    }
    /* Detect TMDS/LVDS Encoder */
    VIAPostDVI(pBIOSInfo);
	/*VIAGetPanelInfo(pBIOSInfo);*/
    pBIOSInfo->ConnectedDevice = VIAGetDeviceDetect(pBIOSInfo);

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);
    pScrn->currentMode = pScrn->modes;
    xf86PrintModes(pScrn);
    xf86SetDpi(pScrn, 0, 0);

#ifdef USE_FB
    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);

#else
    /* load bpp-specific modules */
    switch (pScrn->bitsPerPixel) {
        case 8:
            mod = "cfb";
            reqSym = "cfbScreenInit";
            break;
        case 16:
            mod = "cfb16";
            reqSym = "cfb16ScreenInit";
            break;
        case 32:
            mod = "cfb32";
            reqSym = "cfb32ScreenInit";
            break;
    }

    if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    xf86LoaderReqSymbols(reqSym, NULL);
#endif

    if (!pVia->NoAccel) {
        if(!xf86LoadSubModule(pScrn, "xaa")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    if (pVia->hwcursor) {
        if (!xf86LoadSubModule(pScrn, "ramdac")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    if (pVia->shadowFB) {
        if (!xf86LoadSubModule(pScrn, "shadowfb")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    /* Capture option parameter */
    pVia->Cap0_Deinterlace = CAP_BOB;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_CAP0_DEINTERLACE))) {
        if (!xf86NameCmp(s, "Bob")) {
            pVia->Cap0_Deinterlace = CAP_BOB;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Capture 0 de-interlace mode is Bob\n");
        }
        else if(!xf86NameCmp(s, "Weave")) {
            pVia->Cap0_Deinterlace = CAP_WEAVE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Capture 0 de-interlace mode is Weave\n");
        }
    }

    pVia->Cap1_Deinterlace = CAP_BOB;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_CAP1_DEINTERLACE))) {
        if (!xf86NameCmp(s, "Bob")) {
            pVia->Cap1_Deinterlace = CAP_BOB;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Capture 1 de-interlace mode is Bob\n");
        }
        else if(!xf86NameCmp(s, "Weave")) {
            pVia->Cap1_Deinterlace = CAP_WEAVE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Capture 1 de-interlace mode is Weave\n");
        }
    }
    
    if (xf86ReturnOptValBool(VIAOptions, OPTION_CAP0_FIELDSWAP, FALSE)) {
        pVia->Cap0_FieldSwap = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Option: Cap0_FieldSwap Enabled\n");
    }
    else {
        pVia->Cap0_FieldSwap = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Option: Cap0_FieldSwap Disabled\n");
    }

    /* xf86LoaderReqSymLists(mpegSymbols, NULL); */

    VIADeviceSelection(pScrn);
    if (pVia->IsSecondary) {
        if (pBIOSInfo->SAMM)
            VIADeviceDispatch(pScrn);
        else
            return FALSE;
    }

    VIAUnmapMem(pScrn);

    return TRUE;
}


static Bool VIAEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr      pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    Bool        ret;

    /* FIXME: Rebind AGP memory here */
    /* FIXME: Unlock DRI here */
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAEnterVT\n"));
    VIASave(pScrn);
    vgaHWUnlock(hwp);

    ret = VIAModeInit(pScrn, pScrn->currentMode);

    /* Patch for APM suspend resume, HWCursor has garbage */
    if (pVia->hwcursor && pVia->CursorImage) {
        DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "Restore Cursor Image!\n"));
        memcpy(pVia->FBBase + pVia->CursorStart, pVia->CursorImage, 0x1000);
    	VIASETREG(VIA_REG_CURSOR_FG, pVia->CursorFG);
    	VIASETREG(VIA_REG_CURSOR_BG, pVia->CursorBG);
    	VIASETREG(VIA_REG_CURSOR_MODE, pVia->CursorMC);
        xfree(pVia->CursorImage);
        /*VIALoadCursorImage(pScrn, *pVia->CursorImage);*/
    }

    /* retore video status */
    if (!pVia->IsSecondary)
        viaRestoreVideo(pScrn);

    VIAAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return ret;
}


static void VIALeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    hwp = VGAHWPTR(pScrn);
    VIAPtr      pVia = VIAPTR(pScrn);
    vgaRegPtr   vgaSavePtr = &hwp->SavedReg;
    VIARegPtr   viaSavePtr = &pVia->SavedReg;

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIALeaveVT\n"));
    
    /* FIXME: take the DRI lock here to avoid accidents */
    /* FIXME: unbind the AGP memory ? */
    
    /* Wait Hardware Engine idle to exit graphicd mode */
    WaitIdle();

    if (pVia->VQEnable) {
        /* if we use VQ, disable it before we exit */
        VIASETREG(0x43c, 0x00fe0000);
        VIASETREG(0x440, 0x00000004);
    }

    /* Save video status and turn off all video activities */
    if (!pVia->IsSecondary)
        viaSaveVideo(pScrn);

    if (pVia->hwcursor) {
        pVia->CursorImage = xcalloc(1, 0x1000);
        memcpy(pVia->CursorImage, pVia->FBBase + pVia->CursorStart, 0x1000);
    	pVia->CursorFG = (CARD32)VIAGETREG(VIA_REG_CURSOR_FG);
    	pVia->CursorBG = (CARD32)VIAGETREG(VIA_REG_CURSOR_BG);
    	pVia->CursorMC = (CARD32)VIAGETREG(VIA_REG_CURSOR_MODE);
    }

    VIAWriteMode(pScrn, vgaSavePtr, viaSavePtr);
    vgaHWLock(hwp);
}


static void VIASave(ScrnInfoPtr pScrn)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    vgaRegPtr       vgaSavePtr = &hwp->SavedReg;
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    VIARegPtr       save = &pVia->SavedReg;
    int             vgaCRIndex, vgaCRReg, vgaIOBase;
    int             i;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[1];
    unsigned char   TVRegs[0xFF];

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIASave\n"));

    if(pVia->IsSecondary)
    {
        DevUnion* pPriv;
        VIAEntPtr pVIAEnt;
        VIAPtr   pVia1;
        vgaHWPtr hwp1;
        pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
              gVIAEntityIndex);
        pVIAEnt = pPriv->ptr;
        hwp1 = VGAHWPTR(pVIAEnt->pPrimaryScrn);
        pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
        hwp->SavedReg = hwp1->SavedReg;
        pVia->SavedReg = pVia1->SavedReg;
    }
    else {
        vgaHWProtect(pScrn, TRUE);

        if (xf86IsPrimaryPci(pVia->PciInfo)) {
            vgaHWSave(pScrn, vgaSavePtr, VGA_SR_ALL);
        }
        else {
            vgaHWSave(pScrn, vgaSavePtr, VGA_SR_MODE);
        }

        vgaIOBase = hwp->IOBase;
        vgaCRReg = vgaIOBase + 5;
        vgaCRIndex = vgaIOBase + 4;

        /* Unlock Extended Regs */
        outb(0x3c4, 0x10);
        outb(0x3c5, 0x01);

        VGAOUT8(0x3c4, 0x14);
        save->SR14 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x15);
        save->SR15 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x16);
        save->SR16 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x17);
        save->SR17 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x18);
        save->SR18 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x19);
        save->SR19 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x1a);
        save->SR1A = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x1b);
        save->SR1B = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x1c);
        save->SR1C = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x1d);
        save->SR1D = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x1e);
        save->SR1E = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x1f);
        save->SR1F = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x22);
        save->SR22 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x23);
        save->SR23 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x24);
        save->SR24 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x25);
        save->SR25 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x26);
        save->SR26 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x27);
        save->SR27 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x28);
        save->SR28 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x29);
        save->SR29 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x2a);
        save->SR2A = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x2b);
        save->SR2B = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x2e);
        save->SR2E = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x44);
        save->SR44 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x45);
        save->SR45 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x46);
        save->SR46 = VGAIN8(0x3c5);
        VGAOUT8(0x3c4, 0x47);
        save->SR47 = VGAIN8(0x3c5);

        VGAOUT8(vgaCRIndex, 0x13);
        save->CR13 = VGAIN8(vgaCRReg);
        VGAOUT8(vgaCRIndex, 0x32);
        save->CR32 = VGAIN8(vgaCRReg);
        VGAOUT8(vgaCRIndex, 0x33);
        save->CR33 = VGAIN8(vgaCRReg);
        VGAOUT8(vgaCRIndex, 0x34);
        save->CR34 = VGAIN8(vgaCRReg);
        VGAOUT8(vgaCRIndex, 0x35);
        save->CR35 = VGAIN8(vgaCRReg);
        VGAOUT8(vgaCRIndex, 0x36);
        save->CR36 = VGAIN8(vgaCRReg);

        /* Saving TV register status before set mode */
        switch (pBIOSInfo->TVEncoder) {
            case VIA_NONETV:
                break;
            case VIA_VT1623:
                VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
                for (i = 0; i < 0x6C; i++) {
                    VIAGPIOI2C_ReadByte(pBIOSInfo, i, (TVRegs + i));
                    save->TVRegs[i] = TVRegs[i];
                    /*DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Save TV Register[%d]0x%X\n", i, save->TVRegs[i]));*/
                }
                break;
            default:
                if (xf86I2CProbeAddress(pVia->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
                    dev = xf86CreateI2CDevRec();
                    dev->DevName = "TV";
                    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
                    dev->pI2CBus = pVia->I2C_Port2;

                    if (xf86I2CDevInit(dev)) {
                        W_Buffer[0] = 0;
                        xf86I2CWriteRead(dev, W_Buffer,1, TVRegs, 0xFF);
                        for (i = 0; i < 0xFF; i++) {
                            save->TVRegs[i] = TVRegs[i];
                        }
                        xf86DestroyI2CDevRec(dev,TRUE);
                    }
                    else
                        xf86DestroyI2CDevRec(dev,TRUE);
                }
                break;
        }

        /* Saving LVDS register status before set mode
        if (pBIOSInfo->LVDS == VIA_CH7019LVDS) {
            dev = xf86CreateI2CDevRec();
            dev->DevName = "LVDS";
            dev->SlaveAddr = 0xEA;
            dev->pI2CBus = pVia->I2C_Port2;

            if (xf86I2CDevInit(dev)) {
                for (i = 0; i < 0x40; i++) {
                    W_Buffer[0] = i + 0x40;
                    xf86I2CWriteRead(dev, W_Buffer, 1, save->LCDRegs + i, 1);
                }
                xf86DestroyI2CDevRec(dev,TRUE);
            }
            else
                xf86DestroyI2CDevRec(dev,TRUE);
        }*/

        /* Save LCD control regs */
        for (i = 0; i < 68; i++) {
            VGAOUT8(vgaCRIndex, i + 0x50);
            save->CRTCRegs[i] = VGAIN8(vgaCRReg);
        }

        if (!pVia->ModeStructInit) {
            vgaHWCopyReg(&hwp->ModeReg, vgaSavePtr);
            memcpy(&pVia->ModeReg, save, sizeof(VIARegRec));
            pVia->ModeStructInit = TRUE;
        }
        vgaHWProtect(pScrn, FALSE);
    }
    return;
}


static void VIAWriteMode(ScrnInfoPtr pScrn, vgaRegPtr vgaSavePtr,
                         VIARegPtr restore)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    int             vgaCRIndex, vgaCRReg, vgaIOBase;
/*    Bool            graphicsMode = FALSE;*/
    int             i;
    unsigned char   W_Buffer[3];
    I2CDevPtr       dev = NULL;
    CARD8           tmp;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAWriteMode\n"));
    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    /*
     * If we figured out a VESA mode number for this timing, just use
     * the VGA BIOS to do the switching, with a few additional tweaks.
     */
    if (restore->mode != 0xFF)
    {
        /* Set up the mode.  Don't clear video RAM. */
        if (!pVia->IsSecondary)
            VIASetModeUseBIOSTable(pBIOSInfo);
        else
            VIASetModeForMHS(pBIOSInfo);

        /* Restore the DAC.*/
        if (pBIOSInfo->FirstInit ) {
            vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_CMAP);
        }
        /*else {
            vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_MODE);
        }
        vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_CMAP);*/
        pBIOSInfo->FirstInit = FALSE;
        /* Enable the graphics engine. */
        if (!pVia->NoAccel)
            VIAInitialize2DEngine(pScrn);

#ifdef XF86DRI
      	VIAInitialize3DEngine(pScrn);
#endif
        return;
    }
    vgaHWProtect(pScrn, TRUE);
    /* Unlock Extended Regs */
    outb(0x3c4, 0x10);
    outb(0x3c5, 0x01);

    /* How can we know the mode is graphical mode or not ? */
    /* graphicsMode = (restore->mode == 0xFF) ? FALSE : TRUE; */

    VGAOUT8(vgaCRIndex, 0x6a);
    VGAOUT8(vgaCRReg, 0);
    VGAOUT8(vgaCRIndex, 0x6b);
    VGAOUT8(vgaCRReg, 0);
    VGAOUT8(vgaCRIndex, 0x6c);
    VGAOUT8(vgaCRReg, 0);

    switch (pBIOSInfo->TVEncoder) {
        case VIA_TV2PLUS:
        case VIA_TV3:
        case VIA_CH7009:
        case VIA_CH7019:
        case VIA_SAA7108:
        case VIA_CH7005:
        case VIA_VT1622A:
        dev = xf86CreateI2CDevRec();
        dev->DevName = "TV";
        dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
        dev->pI2CBus = pVia->I2C_Port2;
        if (xf86I2CDevInit(dev)) {
                for (i = 0; i < 0xFF; i++) {
                    W_Buffer[0] = (unsigned char)(i);
                    W_Buffer[1] = (unsigned char)(restore->TVRegs[i]);
                    xf86I2CWriteRead(dev, W_Buffer, 2, NULL,0);
                }
                xf86DestroyI2CDevRec(dev,TRUE);
            }
            else
                xf86DestroyI2CDevRec(dev,TRUE);
            break;
        case VIA_FS454:
            dev = xf86CreateI2CDevRec();
            dev->DevName = "TV";
            dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
            dev->pI2CBus = pVia->I2C_Port2;
            if (xf86I2CDevInit(dev)) {

                /* QPR programming */
                W_Buffer[0] = 0xC4;
                W_Buffer[1] = (unsigned char)(restore->TVRegs[0xC4]);
                W_Buffer[2] = (unsigned char)(restore->TVRegs[0xC5]);
                xf86I2CWriteRead(dev, W_Buffer,3, NULL,0);

                /* Restore TV Regs */
                for (i = 0; i < 0xFF; i++) {
                    W_Buffer[0] = (unsigned char)(i);
                    W_Buffer[1] = (unsigned char)(restore->TVRegs[i]);
                    if (i == 0xC4)
                        i++;
                    else
                        xf86I2CWriteRead(dev, W_Buffer, 2, NULL,0);
                }
            xf86DestroyI2CDevRec(dev,TRUE);
        }
        else
            xf86DestroyI2CDevRec(dev,TRUE);
            break;
        case VIA_VT1623:
            VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
            for (i = 0; i < 0x6C; i++) {
                VIAGPIOI2C_Write(pBIOSInfo, i, restore->TVRegs[i]);
                /*DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Write Back Register[%d]:0x%X\n", i, restore->TVRegs[i]));*/
            }
            break;
        default:
            break;
    }

    /* restore the standard vga regs */
    if (xf86IsPrimaryPci(pVia->PciInfo))
        vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_ALL);
    else
        vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_MODE);

    /* restore extended regs */
    VGAOUT8(0x3c4, 0x14);
    VGAOUT8(0x3c5, restore->SR14);
    VGAOUT8(0x3c4, 0x15);
    VGAOUT8(0x3c5, restore->SR15);
    VGAOUT8(0x3c4, 0x16);
    VGAOUT8(0x3c5, restore->SR16);
    VGAOUT8(0x3c4, 0x17);
    VGAOUT8(0x3c5, restore->SR17);
    VGAOUT8(0x3c4, 0x18);
    VGAOUT8(0x3c5, restore->SR18);
    VGAOUT8(0x3c4, 0x19);
    VGAOUT8(0x3c5, restore->SR19);
    VGAOUT8(0x3c4, 0x1a);
    VGAOUT8(0x3c5, restore->SR1A);
    VGAOUT8(0x3c4, 0x1b);
    VGAOUT8(0x3c5, restore->SR1B);
    VGAOUT8(0x3c4, 0x1c);
    VGAOUT8(0x3c5, restore->SR1C);
    VGAOUT8(0x3c4, 0x1d);
    VGAOUT8(0x3c5, restore->SR1D);
    VGAOUT8(0x3c4, 0x1e);
    VGAOUT8(0x3c5, restore->SR1E);
    VGAOUT8(0x3c4, 0x1f);
    VGAOUT8(0x3c5, restore->SR1F);
    VGAOUT8(0x3c4, 0x22);
    VGAOUT8(0x3c5, restore->SR22);
    VGAOUT8(0x3c4, 0x23);
    VGAOUT8(0x3c5, restore->SR23);
    VGAOUT8(0x3c4, 0x24);
    VGAOUT8(0x3c5, restore->SR24);
    VGAOUT8(0x3c4, 0x25);
    VGAOUT8(0x3c5, restore->SR25);
    VGAOUT8(0x3c4, 0x26);
    VGAOUT8(0x3c5, restore->SR26);
    VGAOUT8(0x3c4, 0x27);
    VGAOUT8(0x3c5, restore->SR27);
    VGAOUT8(0x3c4, 0x28);
    VGAOUT8(0x3c5, restore->SR28);
    VGAOUT8(0x3c4, 0x29);
    VGAOUT8(0x3c5, restore->SR29);
    VGAOUT8(0x3c4, 0x2a);
    VGAOUT8(0x3c5, restore->SR2A);
    VGAOUT8(0x3c4, 0x2b);
    VGAOUT8(0x3c5, restore->SR2B);
    VGAOUT8(0x3c4, 0x2e);
    VGAOUT8(0x3c5, restore->SR2E);
    VGAOUT8(0x3c4, 0x44);
    VGAOUT8(0x3c5, restore->SR44);
    VGAOUT8(0x3c4, 0x45);
    VGAOUT8(0x3c5, restore->SR45);
    VGAOUT8(0x3c4, 0x46);
    VGAOUT8(0x3c5, restore->SR46);
    VGAOUT8(0x3c4, 0x47);
    VGAOUT8(0x3c5, restore->SR47);

    VGAOUT8(vgaCRIndex, 0x13);
    VGAOUT8(vgaCRReg, restore->CR13);
    VGAOUT8(vgaCRIndex, 0x32);
    VGAOUT8(vgaCRReg, restore->CR32);
    VGAOUT8(vgaCRIndex, 0x33);
    VGAOUT8(vgaCRReg, restore->CR33);
    VGAOUT8(vgaCRIndex, 0x34);
    VGAOUT8(vgaCRReg, restore->CR34);
    VGAOUT8(vgaCRIndex, 0x35);
    VGAOUT8(vgaCRReg, restore->CR35);
    VGAOUT8(vgaCRIndex, 0x36);
    VGAOUT8(vgaCRReg, restore->CR36);

    /* Restore LCD control regs */
    for (i = 0; i < 68; i++) {
        VGAOUT8(vgaCRIndex, i + 0x50);
        VGAOUT8(vgaCRReg, restore->CRTCRegs[i]);
    }

    /*if (pBIOSInfo->LVDS == VIA_CH7019LVDS) {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "LVDS";
        dev->SlaveAddr = 0xEA;
        dev->pI2CBus = pVia->I2C_Port2;

        if (xf86I2CDevInit(dev)) {
            for (i = 0; i < 0x40; i++) {
                W_Buffer[0] = (unsigned char)(i + 0x40);
                W_Buffer[1] = (unsigned char)(restore->LCDRegs[i]);
                xf86I2CWriteRead(dev, W_Buffer, 2, NULL,0);
            }

            xf86DestroyI2CDevRec(dev,TRUE);
        }
        else
            xf86DestroyI2CDevRec(dev,TRUE);

        ActiveDevice = VIABIOS_GetActiveDevice(pScrn);
    }*/
    if (pBIOSInfo->DefaultActiveDevice & VIA_DEVICE_LCD)
        VIAEnableLCD(pBIOSInfo);

    VIADisabledExtendedFIFO(pBIOSInfo);
    /* Reset clock */
    tmp = VGAIN8(0x3cc);
    VGAOUT8(0x3c2, tmp);

    /* If we're going into graphics mode and acceleration was enabled, */
/*
    if (graphicsMode && (!pVia->NoAccel)) {
        VIAInitialize2DEngine(pScrn);
    }
*/
    vgaHWProtect(pScrn, FALSE);
    return;
}


static Bool VIAMapMMIO(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    vgaHWPtr hwp;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAMapMMIO\n"));

    pVia->FrameBufferBase = pVia->PciInfo->memBase[0];
    pVia->MmioBase = pVia->PciInfo->memBase[1];

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping MMIO @ 0x%lx with size 0x%x\n",
               pVia->MmioBase, VIA_MMIO_REGSIZE);

    pVia->MapBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                                  pVia->MmioBase,
                                  VIA_MMIO_REGSIZE);
    pBIOSInfo->MapBase = pVia->MapBase;

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping BitBlt MMIO @ 0x%lx with size 0x%x\n",
               pVia->MmioBase + VIA_MMIO_BLTBASE, VIA_MMIO_BLTSIZE);

    pVia->BltBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                                  pVia->MmioBase + VIA_MMIO_BLTBASE,
                                  VIA_MMIO_BLTSIZE);

    if (!pVia->MapBase || !pVia->BltBase) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Internal error: cound not map registers\n");
        return FALSE;
    }

    /* Memory mapped IO for Video Engine */
    pVia->VidMapBase = pVia->MapBase + 0x200;

    VIAEnableMMIO(pScrn);
    hwp = VGAHWPTR(pScrn);
    vgaHWGetIOBase(hwp);

    return TRUE;
}


static Bool VIAMapFB(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAMapFB\n"));
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping framebuffer @ 0x%lx with size 0x%lx\n",
               pVia->FrameBufferBase, pVia->videoRambytes);

    if (pVia->videoRambytes) {

	/*
	 * FIXME: This is a hack to get rid of offending wrongly sized
	 * MTRR regions set up by the VIA BIOS. Should be taken care of
	 * in the OS support layer.
	 */

        unsigned char *tmp; 
        tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
			    pVia->PciTag, pVia->FrameBufferBase,
			    pVia->videoRambytes);
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)tmp,
                        pVia->videoRambytes);

	/*
	 * End of hack.
	 */

        pVia->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                                     pVia->PciTag, pVia->FrameBufferBase,
                                     pVia->videoRambytes);
	    pBIOSInfo->FBBase = pVia->FBBase;
	    pBIOSInfo->videoRambytes = pVia->videoRambytes;

        if (!pVia->FBBase) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Internal error: could not map framebuffer\n");
            return FALSE;
        }

        pVia->FBStart = pVia->FBBase;
        pVia->FBFreeStart = (pScrn->displayWidth * pScrn->bitsPerPixel >> 3) *
                            pScrn->virtualY;
        pVia->FBFreeEnd = pVia->videoRambytes;

        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                   "Frame buffer start: %p, free start: 0x%x end: 0x%x\n",
                   pVia->FBStart, pVia->FBFreeStart, pVia->FBFreeEnd);
    }

    pScrn->memPhysBase = pVia->PciInfo->memBase[0];
    pScrn->fbOffset = 0;
    if(pVia->IsSecondary) pScrn->fbOffset = pScrn->videoRam << 10;

    return TRUE;
}


static void VIAUnmapMem(ScrnInfoPtr pScrn)
{
    VIAPtr pVia;

    pVia = VIAPTR(pScrn);

    VIADisableMMIO(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAUnmapMem\n"));

    if (pVia->MapBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pVia->MapBase,
                        VIA_MMIO_REGSIZE);
    }

    if (pVia->BltBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pVia->BltBase,
                        VIA_MMIO_BLTSIZE);
    }

    if (pVia->FBBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pVia->FBBase,
                        pVia->videoRambytes);
    }

    return;
}


static Bool VIAScreenInit(int scrnIndex, ScreenPtr pScreen,
                          int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    int ret;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAScreenInit\n"));
    vgaHWUnlock(hwp);

    if (!VIAMapFB(pScrn))
        return FALSE;

    if (!VIAMapMMIO(pScrn))
        return FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Mem Mapped\n"));

    VIASave(pScrn);

    vgaHWBlankScreen(pScrn, FALSE);

    if (!VIAModeInit(pScrn, pScrn->currentMode))
        return FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- State saved\n"));

    /* Darken the screen for aesthetic reasons and set the viewport */
    VIASaveScreen(pScreen, SCREEN_SAVER_ON);
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Blanked\n"));

    miClearVisualTypes();

    if (pScrn->bitsPerPixel > 8 && !pVia->IsSecondary) {
        if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
                              pScrn->rgbBits, pScrn->defaultVisual))
            return FALSE;
        if (!miSetPixmapDepths())
            return FALSE;
    } else {
        if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
                              pScrn->rgbBits, pScrn->defaultVisual))
            return FALSE;
        if (!miSetPixmapDepths())
            return FALSE;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Visuals set up\n"));

    ret = VIAInternalScreenInit(scrnIndex, pScreen);

    if (!ret)
        return FALSE;

    xf86SetBlackWhitePixels(pScreen);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- B & W\n"));

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

#ifdef USE_FB
    /* must be after RGB ordering fixed */
    fbPictureInit(pScreen, 0, 0);
#endif

    if (!pVia->NoAccel) {
        VIAInitAccel(pScreen);
    } 
#ifdef XFREE86_44
    else {
	/*
	 * This is needed because xf86InitFBManagerLinear in VIAInitLinear
	 * needs xf86InitFBManager to have been initialized, and 
	 * xf86InitFBManager needs at least one line of free memory to
	 * work. This is only for Xv in Noaccel part, and since Xv is in some
	 * sense accelerated, it might be a better idea to disable it
	 * altogether.
	 */ 
        BoxRec AvailFBArea;

        AvailFBArea.x1 = 0;
        AvailFBArea.y1 = 0;
        AvailFBArea.x2 = pScrn->displayWidth;
        AvailFBArea.y2 = pScrn->virtualY + 1;
	/* 
	 * Update FBFreeStart also for other memory managers, since 
	 * we steal one line to make xf86InitFBManager work.
	 */
	pVia->FBFreeStart = (AvailFBArea.y2 + 1) * pVia->Bpl;
	xf86InitFBManager(pScreen, &AvailFBArea);	
    }
#endif

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    /*xf86SetSilkenMouse(pScreen);*/
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Backing store set up\n"));

    if(!pVia->shadowFB)         /* hardware cursor needs to wrap this layer */
        VIADGAInit(pScreen);

    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- SW cursor set up\n"));

    if (pVia->hwcursor) {
        if (!VIAHWCursorInit(pScreen)) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Hardware cursor initialization failed\n");
        }
    }

    if (pVia->shadowFB) {
        RefreshAreaFuncPtr refreshArea = VIARefreshArea;

        if(pVia->rotate) {
            if (!pVia->PointerMoved) {
                pVia->PointerMoved = pScrn->PointerMoved;
                pScrn->PointerMoved = VIAPointerMoved;
            }

            switch(pScrn->bitsPerPixel) {
            case 8:
                refreshArea = VIARefreshArea8;
                break;
            case 16:
                refreshArea = VIARefreshArea16;
                break;
            case 32:
                refreshArea = VIARefreshArea32;
                break;
            }
        }

        ShadowFBInit(pScreen, refreshArea);
    }

    if (!miCreateDefColormap(pScreen))
        return FALSE;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Def Color map set up\n"));

    if (!xf86HandleColormaps(pScreen, 256, 6, VIALoadPalette, NULL,
                             CMAP_RELOAD_ON_MODE_SWITCH
                             | CMAP_PALETTED_TRUECOLOR))
        return FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Palette loaded\n"));

    vgaHWBlankScreen(pScrn, TRUE);

    pVia->CloseScreen = pScreen->CloseScreen;
    pScreen->SaveScreen = VIASaveScreen;
    pScreen->CloseScreen = VIACloseScreen;

    xf86DPMSInit(pScreen, VIADPMS, 0);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- DPMS set up\n"));

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Color maps etc. set up\n"));

#ifdef XF86DRI
    pVia->directRenderingEnabled = VIADRIScreenInit(pScreen);

    if (pVia->directRenderingEnabled) {
    pVia->directRenderingEnabled = VIADRIFinishScreenInit(pScreen);
    }
    if (pVia->directRenderingEnabled) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "direct rendering enabled\n");
    }
    else {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "direct rendering disabled\n");
    }
    if (!pVia->directRenderingEnabled)
	VIAInitLinear(pScreen);
#else    
    VIAInitLinear(pScreen);
#endif

    
    if (!pVia->IsSecondary) {
        /* The chipset is checked in viaInitVideo */
        viaFillGraphicInfo(pScrn);
       	viaInitVideo(pScreen);
    }

    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Done\n"));
    return TRUE;
}


static int VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr     pScrn;
    VIAPtr          pVia;
    int             width, height, displayWidth;
    unsigned char   *FBStart;
    int             ret = TRUE;

    xf86DrvMsg(scrnIndex, X_INFO, "VIAInternalScreenInit\n");

    pScrn = xf86Screens[pScreen->myNum];
    pVia = VIAPTR(pScrn);

    displayWidth = pScrn->displayWidth;

    if (pVia->rotate) {
        height = pScrn->virtualX;
        width = pScrn->virtualY;
    } else {
        width = pScrn->virtualX;
        height = pScrn->virtualY;
    }

    if (pVia->shadowFB) {
        pVia->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pVia->ShadowPtr = xalloc(pVia->ShadowPitch * height);
        displayWidth = pVia->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pVia->ShadowPtr;
    }
    else {
        pVia->ShadowPtr = NULL;
        FBStart = pVia->FBStart;
    }

#ifdef USE_FB
    ret = fbScreenInit(pScreen, FBStart, width, height,
                       pScrn->xDpi, pScrn->yDpi, displayWidth,
                       pScrn->bitsPerPixel);
#else
    switch (pScrn->bitsPerPixel) {
    case 8:
        ret = cfbScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
                            pScrn->yDpi, displayWidth);
        break;

    case 16:
        ret = cfb16ScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
                              pScrn->yDpi, displayWidth);
        break;

    case 32:
        ret = cfb32ScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
                              pScrn->yDpi, displayWidth);
        break;

    default:
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Internal error: invalid bpp (%d) in SavageScreenInit\n",
                   pScrn->bitsPerPixel);
        ret = FALSE;
        break;
    }
#endif

    return ret;
}


static ModeStatus VIAValidMode(int scrnIndex, DisplayModePtr mode,
                               Bool verbose, int flags)
{
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAValidMode\n"));
    /* TODO check modes */
    return MODE_OK;
}


static void VIABIOSInit(VIAPtr pVia, ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    int             i;
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

    pBIOSInfo->bitsPerPixel = pScrn->bitsPerPixel;
    pBIOSInfo->displayWidth = pScrn->displayWidth;
    pBIOSInfo->frameX1 = pScrn->frameX1;
    pBIOSInfo->frameY1 = pScrn->frameY1;
    pBIOSInfo->scrnIndex = pScrn->scrnIndex;

    pBIOSInfo->Clock = pMode->Clock;
    pBIOSInfo->HTotal = pMode->HTotal;
    pBIOSInfo->VTotal = pMode->VTotal;
    pBIOSInfo->HDisplay = pMode->HDisplay;
    pBIOSInfo->VDisplay = pMode->VDisplay;
    pBIOSInfo->CrtcHDisplay = pMode->CrtcHDisplay;
    pBIOSInfo->CrtcVDisplay = pMode->CrtcVDisplay;
    if (pBIOSInfo->FirstInit) {
        pBIOSInfo->SaveframeX1 = pScrn->frameX1;
        pBIOSInfo->SaveframeY1 = pScrn->frameY1;
        pBIOSInfo->SaveHDisplay = pMode->HDisplay;
        pBIOSInfo->SaveVDisplay = pMode->VDisplay;
        pBIOSInfo->SaveCrtcHDisplay = pMode->CrtcHDisplay;
        pBIOSInfo->SaveCrtcVDisplay = pMode->CrtcVDisplay;
    }

    pBIOSInfo->IsSecondary = pVia->IsSecondary;
    pBIOSInfo->HasSecondary = pVia->HasSecondary;

    for (i = 0; i < 0xFF; i++) {
        pBIOSInfo->TVRegs[i] = pVia->SavedReg.TVRegs[i];
    }
}


static void VIAPostFindMode(VIAPtr pVia, ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

    pVia->ModeReg.mode = pBIOSInfo->mode;
    pVia->ModeReg.resMode = pBIOSInfo->resMode;
    pVia->ModeReg.refresh = pBIOSInfo->refresh;
    pVia->ModeReg.offsetWidthByQWord = pBIOSInfo->offsetWidthByQWord;
    pVia->ModeReg.countWidthByQWord = pBIOSInfo->countWidthByQWord;

    pScrn->frameX1 = pBIOSInfo->frameX1;
    pScrn->frameY1 = pBIOSInfo->frameY1;
    pMode->HDisplay = pBIOSInfo->HDisplay;
    pMode->VDisplay = pBIOSInfo->VDisplay;

}


static Bool VIAModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    VIARegPtr       new = &pVia->ModeReg;
    vgaRegPtr       vganew = &hwp->ModeReg;
    /* DDUPDATEOVERLAY UpdateOverlay; */

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAModeInit\n"));


    if (!vgaHWInit(pScrn, mode)) {
        vgaHWBlankScreen(pScrn, TRUE);
        return FALSE;
    }

    pScrn->vtSema = TRUE;

    VIABIOSInit(pVia, pScrn, mode);

    if (!VIAFindModeUseBIOSTable(pBIOSInfo)) {
        vgaHWBlankScreen(pScrn, TRUE);
        VIAFreeRec(pScrn);
        return FALSE;
    }

    VIAPostFindMode(pVia, pScrn, mode);

    /* FIXME - need DRI lock for this bit - see i810 */
    /* do it! */
    VIAWriteMode(pScrn, vganew, new);

    /* pass graphic info to via_v4l kernel module */
    /* Coz mode changes, some member in pVia->graphicInfo need to modify */
    if (VIA_SERIES(pVia->Chipset) && !pVia->IsSecondary)
    {
        viaFillGraphicInfo(pScrn);

        DBG_DD(ErrorF("SWOV:  VIAVidSet2DInfo\n"));

        /* Save MCLK value*/
        VGAOUT8(0x3C4, 0x16); pVia->swov.Save_3C4_16 = VGAIN8(0x3C5);
        DBG_DD(ErrorF("        3c4.16 : %08x \n",VGAIN8(0x3C5)));
        VGAOUT8(0x3C4, 0x17); pVia->swov.Save_3C4_17 = VGAIN8(0x3C5);
        DBG_DD(ErrorF("        3c4.17 : %08x \n",VGAIN8(0x3C5)));
        VGAOUT8(0x3C4, 0x18); pVia->swov.Save_3C4_18 = VGAIN8(0x3C5);
        DBG_DD(ErrorF("        3c4.18 : %08x \n",VGAIN8(0x3C5)));
    }
    VIAAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return TRUE;
}


static Bool VIACloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    hwp = VGAHWPTR(pScrn);
    VIAPtr      pVia = VIAPTR(pScrn);
    vgaRegPtr   vgaSavePtr = &hwp->SavedReg;
    VIARegPtr   viaSavePtr = &pVia->SavedReg;
    CARD32      dwCursorMode;

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIACloseScreen\n"));

    /* Is the display currently visible ? */
    if(pScrn->vtSema)
    {
        /* Wait Hardware Engine idle to exit graphical mode */
        WaitIdle();
    
	/* Patch for normal log out and restart X, 3D application will hang */
	VIAWriteMode(pScrn, vgaSavePtr, viaSavePtr);

	if (!pVia->IsSecondary) {
            /* Turn off all video activities */
            viaExitVideo(pScrn);
            /* Diable Hardware Cursor */
            dwCursorMode = VIAGETREG(VIA_REG_CURSOR_MODE);
            VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode & 0xFFFFFFFE);
        }

        if (pVia->VQEnable) {
            /* if we use VQ, disable it before we exit */
            VIASETREG(0x43c, 0x00fe0000);
            VIASETREG(0x440, 0x00000004);
        }
    }
#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
	VIADRICloseScreen(pScreen);
    }
#endif
    if (pVia->AccelInfoRec) {
        XAADestroyInfoRec(pVia->AccelInfoRec);
        pVia->AccelInfoRec = NULL;
    }
    if (pVia->CursorInfoRec) {
        xf86DestroyCursorInfoRec(pVia->CursorInfoRec);
        pVia->CursorInfoRec = NULL;
    }
    if (pVia->ShadowPtr) {
        xfree(pVia->ShadowPtr);
        pVia->ShadowPtr = NULL;
    }
    if (pVia->DGAModes) {
        xfree(pVia->DGAModes);
        pVia->DGAModes = NULL;
    }
    if(pVia->pInt10) {
        xf86FreeInt10(pVia->pInt10);
        pVia->pInt10 = NULL;
    }

    if (pScrn->vtSema) {
        VIAWriteMode(pScrn, vgaSavePtr, viaSavePtr);
        vgaHWLock(hwp);
        VIAUnmapMem(pScrn);
    }
    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = pVia->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/*
 * This only gets called when a screen is being deleted.  It does not
 * get called routinely at the end of a server generation.
 */
static void VIAFreeScreen(int scrnIndex, int flags)
{
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAFreeScreen\n"));
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	    vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    VIAFreeRec(xf86Screens[scrnIndex]);
}

static Bool VIASaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}


void VIAAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    hwp = VGAHWPTR(pScrn);
    VIAPtr      pVia = VIAPTR(pScrn);
    int         Base, tmp;
    int         vgaCRIndex, vgaCRReg, vgaIOBase;
    DDLOCK      ddLock;
    LPDDLOCK    lpddLock = &ddLock;
    DDUPDATEOVERLAY UpdateOverlay;
    ADJUSTFRAME AdjustFrame;

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAAdjustFrame\n"));
    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

    Base = (y * pScrn->displayWidth + x) * (pScrn->bitsPerPixel / 8);

    /* now program the start address registers */
    if (pVia->IsSecondary) {
        Base = (Base + pScrn->fbOffset) >> 3;
        VGAOUT8(vgaCRIndex, 0x62);
        tmp = VGAIN8(vgaCRReg) & 0x01;
        tmp |= (Base & 0x7F) << 1;
        VGAOUT8(vgaCRReg, tmp);
        VGAOUT8(vgaCRIndex, 0x63);
        VGAOUT8(vgaCRReg, ((Base & 0x7F80) >> 7));
        VGAOUT8(vgaCRIndex, 0x64);
        VGAOUT8(vgaCRReg, ((Base & 0x7F8000) >> 15));
    }
    else {
        Base = Base >> 1;
        VGAOUT16(vgaCRIndex, (Base & 0x00ff00) | 0x0c);
        VGAOUT16(vgaCRIndex, ((Base & 0x00ff) << 8) | 0x0d);
        VGAOUT16(vgaCRIndex, ((Base & 0xff0000) >> 8) | 0x34);
    }

    /* Pass Panning (x, y) info to V4L */
    AdjustFrame.x = x;
    AdjustFrame.y = y;

    VIAVidAdjustFrame(pScrn,&AdjustFrame);

    /* Check if HW mpeg engine active */
    if (pVia->Video.VideoStatus & SW_VIDEO_ON) /* SW video case */
    {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                      " Call SW MPEG UpdateOverlay at panning mode.\n");

        ddLock.dwFourCC = FOURCC_YUY2;

        VIAVidLockSurface(pScrn, &ddLock);

        UpdateOverlay.dwFlags = DDOVER_SHOW | DDOVER_KEYDEST;
        UpdateOverlay.dwColorSpaceLowValue = VIAGETREG(0x220);
        UpdateOverlay.rSrc.left = 0;
        UpdateOverlay.rSrc.top = 0;
        UpdateOverlay.rSrc.right = 720;
        UpdateOverlay.rSrc.bottom = 480;

        UpdateOverlay.rDest.left = (int) lpddLock->SWDevice.gdwSWDstLeft;
        UpdateOverlay.rDest.top = (int) lpddLock->SWDevice.gdwSWDstTop;
        UpdateOverlay.rDest.right = UpdateOverlay.rDest.left + lpddLock->SWDevice.gdwSWDstWidth;
        UpdateOverlay.rDest.bottom = UpdateOverlay.rDest.top + lpddLock->SWDevice.gdwSWDstHeight;

        VIAVidUpdateOverlay(pScrn, &UpdateOverlay);
    }
    else if (VIAGETREG(0x310) & 0x1) /* capture 0 (TV0) case */
    {
        ddLock.dwFourCC = FOURCC_TV0;

        VIAVidLockSurface(pScrn, &ddLock);
    }

    if (VIAGETREG(0x354) & 0x1) /* capture 1 (TV1) case */
    {
        ddLock.dwFourCC = FOURCC_TV1;

        VIAVidLockSurface(pScrn, &ddLock);
    }

    return;
}


Bool VIASwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr      pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIASwitchMode\n"));
    /* Wait Hardware Engine idle to switch graphicd mode */
    WaitIdle();

    if (pVia->VQEnable) {
        /* if we use VQ, disable it before we exit */
        VIASETREG(0x43c, 0x00fe0000);
        VIASETREG(0x440, 0x00000004);
    }

    return VIAModeInit(xf86Screens[scrnIndex], mode);
}


void VIAEnableMMIO(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    unsigned char val;

    if (xf86IsPrimaryPci(pVia->PciInfo)) {
        /* If we are primary card, we still use std vga port. If we use
         * MMIO, system will hang in vgaHWSave when our card used in
         * PLE and KLE (integrated Trident MVP4)
         */
        vgaHWSetStdFuncs(hwp);
    }
    else {
        vgaHWSetMmioFuncs(hwp, pVia->MapBase, 0x8000);
    }

    val = VGAIN8(0x3c3);
    VGAOUT8(0x3c3, val | 0x01);
    val = VGAIN8(VGA_MISC_OUT_R);
    VGAOUT8(VGA_MISC_OUT_W, val | 0x01);

    /* Unlock Extended IO Space */
    VGAOUT8(0x3c4, 0x10);
    VGAOUT8(0x3c5, 0x01);

    /* Enable MMIO */
    if(!pVia->IsSecondary) {
	VGAOUT8(0x3c4, 0x1a);
	val = VGAIN8(0x3c5);
	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "primary val = %x\n", val));
	VGAOUT8(0x3c5, val | 0x68);
    }
    else {
	VGAOUT8(0x3c4, 0x1a);
	val = VGAIN8(0x3c5);
	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "secondary val = %x\n", val));
	VGAOUT8(0x3c5, val | 0x38);
    }

    return;
}


void VIADisableMMIO(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    unsigned char val;

    VGAOUT8(0x3c4, 0x1a);
    val = VGAIN8(0x3c5);
    VGAOUT8(0x3c5, val & 0x97);

    return;
}


void VIALoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
                    LOCO *colors, VisualPtr pVisual)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int i, index;
    int sr1a, sr1b, cr67, cr6a;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIALoadPalette\n"));

    if (pScrn->bitsPerPixel != 8)
        return;
    VGAOUT8(0x3C4, 0x1A);
    sr1a = VGAIN8(0x3C5);
    VGAOUT8(0x3C4, 0x1B);
    sr1b = VGAIN8(0x3C5);
    VGAOUT8(0x3D4, 0x67);
    cr67 = VGAIN8(0x3D5);
    VGAOUT8(0x3D4, 0x6A);
    cr6a = VGAIN8(0x3D5);

    if (pVia->IsSecondary) {
        VGAOUT8(0x3C4, 0x1A);
        VGAOUT8(0x3C5, sr1a | 0x01);
        VGAOUT8(0x3C4, 0x1B);
        VGAOUT8(0x3C5, sr1b | 0x80);
        VGAOUT8(0x3D4, 0x67);
        VGAOUT8(0x3D5, cr67 & 0x3F);
        VGAOUT8(0x3D4, 0x6A);
        VGAOUT8(0x3D5, cr6a | 0xC0);
    }

    for (i = 0; i < numColors; i++) {
        index = indicies[i];
        VGAOUT8(0x3c8, index);
        VGAOUT8(0x3c9, colors[index].red);
        VGAOUT8(0x3c9, colors[index].green);
        VGAOUT8(0x3c9, colors[index].blue);
    }

    if (pVia->IsSecondary) {
        VGAOUT8(0x3C4, 0x1A);
        VGAOUT8(0x3C5, sr1a);
        VGAOUT8(0x3C4, 0x1B);
        VGAOUT8(0x3C5, sr1b);
        VGAOUT8(0x3D4, 0x67);
        VGAOUT8(0x3D5, cr67);
        VGAOUT8(0x3D4, 0x6A);
        VGAOUT8(0x3D5, cr6a);

        /* Screen 0 palette was changed by mode setting of Screen 1,
         * so load again */
        for (i = 0; i < numColors; i++) {
            index = indicies[i];
            VGAOUT8(0x3c8, index);
            VGAOUT8(0x3c9, colors[index].red);
            VGAOUT8(0x3c9, colors[index].green);
            VGAOUT8(0x3c9, colors[index].blue);
        }

    }

}


static void VIADPMS(ScrnInfoPtr pScrn, int mode, int flags)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    int             vgaCRIndex, vgaCRReg;
    CARD8           val;

    I2CDevPtr       dev;
    unsigned char   W_Buffer[2];

    vgaCRIndex = hwp->IOBase + 4;
    vgaCRReg = hwp->IOBase + 5;

    /* Clear DPMS setting */
    VGAOUT8(vgaCRIndex, 0x36);
    val = VGAIN8(vgaCRReg);
    val &= 0xCF;
    /* Turn Off CRT, if user doesn't want crt on */
    if (!pVia->IsSecondary && !(pBIOSInfo->ActiveDevice & VIA_DEVICE_CRT1)) {
        val |= 0x30;
    }

    switch (mode) {
    case DPMSModeOn:
        if (pBIOSInfo->ActiveDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) {
            /* Enable LCD */
            VIAEnableLCD(pBIOSInfo);
        }

        if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
            dev = xf86CreateI2CDevRec();
            dev->DevName = "TV";
            dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
            dev->pI2CBus = pBIOSInfo->I2C_Port2;
            if (xf86I2CDevInit(dev)) {
                switch (pBIOSInfo->TVEncoder) {
                    case VIA_TV2PLUS:
                    case VIA_TV3:
                    case VIA_VT1622A:
                        W_Buffer[0] = 0x0E;
                        W_Buffer[1] = 0x0;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        break;
                    case VIA_VT1623:
                        VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
                        VIAGPIOI2C_Write(pBIOSInfo, 0x0E, 0);
                        break;
                    case VIA_CH7019:
                        W_Buffer[0] = 0x49;
                        W_Buffer[1] = 0x20;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        /*W_Buffer[0] = 0x1E;
                        W_Buffer[1] = 0xD0;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);*/
                        break;
                    case VIA_SAA7108:
                        W_Buffer[0] = 0x2D;
                        switch (pBIOSInfo->TVOutput) {
                            case TVOUTPUT_COMPOSITE:
                            case TVOUTPUT_SVIDEO:
                                W_Buffer[1] = 0xB4;
                                break;
                            case TVOUTPUT_RGB:
                                W_Buffer[1] = 0;
                                break;
                            case TVOUTPUT_YCBCR:
                                W_Buffer[1] = 0x84;
                                break;
                            default:
                                W_Buffer[1] = 0x08;
                                break;
                        }
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        break;
                    default:
                        break;
                }
                xf86DestroyI2CDevRec(dev,TRUE);
            }
            else
                xf86DestroyI2CDevRec(dev,TRUE);
        }
        VGAOUT8(vgaCRIndex, 0x36);
        VGAOUT8(vgaCRReg, val);
        break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
        if (pBIOSInfo->ActiveDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) {
            VIADisableLCD(pBIOSInfo);
        }
        if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
            dev = xf86CreateI2CDevRec();
            dev->DevName = "TV";
            dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
            dev->pI2CBus = pBIOSInfo->I2C_Port2;
            if (xf86I2CDevInit(dev)) {
                switch (pBIOSInfo->TVEncoder) {
                    case VIA_TV2PLUS:
                        W_Buffer[0] = 0x0E;
                        W_Buffer[1] = 0x03;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        break;
                    case VIA_TV3:
                    case VIA_VT1622A:
                        W_Buffer[0] = 0x0E;
                        W_Buffer[1] = 0x0F;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        break;
                    case VIA_VT1623:
                        VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
                        VIAGPIOI2C_Write(pBIOSInfo, 0x0E, 0x0F);
                        break;
                    case VIA_CH7019:
                        W_Buffer[0] = 0x49;
                        W_Buffer[1] = 0x3E;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        W_Buffer[0] = 0x1E;
                        W_Buffer[1] = 0xD0;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        break;
                    case VIA_SAA7108:
                        W_Buffer[0] = 0x2D;
                        W_Buffer[1] = 0x08;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        break;
                    default:
                        break;
                }
                xf86DestroyI2CDevRec(dev,TRUE);
            }
            else
                xf86DestroyI2CDevRec(dev,TRUE);
        }

        val |= 0x30;
        VGAOUT8(vgaCRIndex, 0x36);
        VGAOUT8(vgaCRReg, val);
        break;
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid DPMS mode %d\n", mode);
        break;
    }
    return;
}


/* Active Device according connected status */
Bool VIADeviceSelection(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    /*unsigned int  i;
    unsigned char   numDevice;*/
    pBIOSInfo->SAMM = FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIADeviceSelection\n"));
    pBIOSInfo->DefaultActiveDevice = VIABIOS_GetActiveDevice(pScrn);
    /* if XF86Config Option "ActiveDevice" hasn't set, active device according CMOS */
    if (!pBIOSInfo->ActiveDevice) {
        pBIOSInfo->ActiveDevice = pBIOSInfo->DefaultActiveDevice;
    }
    /* if XF86Config-4 set SAMM, and only active one device, active device according CMOS */
    else if (pVia->IsSecondary && (pBIOSInfo->ActiveDevice == VIA_DEVICE_CRT1 ||
            pBIOSInfo->ActiveDevice == VIA_DEVICE_LCD ||
            pBIOSInfo->ActiveDevice == VIA_DEVICE_TV ||
            pBIOSInfo->ActiveDevice == VIA_DEVICE_DFP ||
            pBIOSInfo->ActiveDevice == VIA_DEVICE_CRT2)){
        pBIOSInfo->ActiveDevice = VIABIOS_GetActiveDevice(pScrn);
    }
    if (((pBIOSInfo->ActiveDevice & pBIOSInfo->ConnectedDevice) == pBIOSInfo->ActiveDevice)
         && (pVia->IsSecondary)) {
        pBIOSInfo->SAMM = TRUE;
    }
    pBIOSInfo->ActiveDevice &= pBIOSInfo->ConnectedDevice;
    pVia->ActiveDevice = pBIOSInfo->ActiveDevice;

    /*if (pBIOSInfo->ActiveDevice & VIA_DEVICE_LCD) {
        pBIOSInfo->DVIEncoder = VIA_VT3191;
        numDevice = 0x02;
        i = VIABIOS_GetDisplayDeviceInfo(pScrn, &numDevice);
        if (i != 0xFFFF) {
            if (pBIOSInfo->PanelSize == VIA_PANEL_INVALID)
                pBIOSInfo->PanelSize = numDevice;
    }*/

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Connected Device is %d\n", pBIOSInfo->ConnectedDevice));
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Active Device is %d\n", pBIOSInfo->ActiveDevice));

    return TRUE;
}

/* SAMM device dispatch */
Bool VIADeviceDispatch(ScrnInfoPtr pScrn)
{
    DevUnion* pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gVIAEntityIndex);
    VIAEntPtr pVIAEnt = pPriv->ptr;
    VIAPtr pVia1 = VIAPTR(pScrn), pVia0 = VIAPTR(pVIAEnt->pPrimaryScrn);
    VIABIOSInfoPtr pBIOSInfo1 = pVia1->pBIOSInfo, pBIOSInfo0 = pVia0->pBIOSInfo;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIADeviceDispatch\n"));
    pBIOSInfo0->SAMM = TRUE;
    switch (pBIOSInfo1->ActiveDevice) {
        case (VIA_DEVICE_CRT1 | VIA_DEVICE_LCD) :       /* If CRT + LCD, CRT is IGA1 */
            pBIOSInfo0->ActiveDevice = VIA_DEVICE_CRT1;
            pBIOSInfo1->ActiveDevice = VIA_DEVICE_LCD;
            break;
        case (VIA_DEVICE_CRT1 | VIA_DEVICE_TV) :        /* If CRT + TV, CRT is IGA1 */
            pBIOSInfo0->ActiveDevice = VIA_DEVICE_CRT1;
            pBIOSInfo1->ActiveDevice = VIA_DEVICE_TV;
            break;
        case (VIA_DEVICE_CRT1 | VIA_DEVICE_DFP) :       /* If CRT + DFP, CRT is IGA1 */
            pBIOSInfo0->ActiveDevice = VIA_DEVICE_CRT1;
            pBIOSInfo1->ActiveDevice = VIA_DEVICE_DFP;
            break;
        case (VIA_DEVICE_LCD | VIA_DEVICE_TV) :         /* If LCD + TV, TV is IGA1 */
            pBIOSInfo0->ActiveDevice = VIA_DEVICE_TV;
            pBIOSInfo1->ActiveDevice = VIA_DEVICE_LCD;
            break;
        case (VIA_DEVICE_LCD | VIA_DEVICE_DFP) :        /* If LCD + DFP, DFP is IGA1 */
            pBIOSInfo0->ActiveDevice = VIA_DEVICE_DFP;
            pBIOSInfo1->ActiveDevice = VIA_DEVICE_LCD;
            break;
        case (VIA_DEVICE_TV | VIA_DEVICE_DFP) :         /* If TV + DFP, TV is IGA1 */
            pBIOSInfo0->ActiveDevice = VIA_DEVICE_TV;
            pBIOSInfo1->ActiveDevice = VIA_DEVICE_DFP;
            break;
        default :
            return FALSE;
            break;
    }
    DEBUG(xf86DrvMsg(pVIAEnt->pPrimaryScrn->scrnIndex, X_INFO, "Active Device is %d\n", pBIOSInfo0->ActiveDevice));
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Active Device is %d\n", pBIOSInfo1->ActiveDevice));
    return TRUE;
}

#ifdef XF86DRI
void
VIAInitialize3DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int i;

    if (!pVia->sharedData->b3DRegsInitialized)
    {

        VIASETREG(0x43C, 0x00010000);

        for (i = 0; i <= 0x7D; i++)
        {
            VIASETREG(0x440, (CARD32) i << 24);
        }

        VIASETREG(0x43C, 0x00020000);

        for (i = 0; i <= 0x94; i++)
        {
            VIASETREG(0x440, (CARD32) i << 24);
        }

        VIASETREG(0x440, 0x82400000);

        VIASETREG(0x43C, 0x01020000);


        for (i = 0; i <= 0x94; i++)
        {
            VIASETREG(0x440, (CARD32) i << 24);
        }

        VIASETREG(0x440, 0x82400000);
        VIASETREG(0x43C, 0xfe020000);

        for (i = 0; i <= 0x03; i++)
        {
            VIASETREG(0x440, (CARD32) i << 24);
        }

        VIASETREG(0x43C, 0x00030000);

        for (i = 0; i <= 0xff; i++)
        {
            VIASETREG(0x440, 0);
        }
        VIASETREG(0x43C, 0x00100000);
        VIASETREG(0x440, 0x00333004);
        VIASETREG(0x440, 0x10000002);
        VIASETREG(0x440, 0x60000000);
        VIASETREG(0x440, 0x61000000);
        VIASETREG(0x440, 0x62000000);
        VIASETREG(0x440, 0x63000000);
        VIASETREG(0x440, 0x64000000);

        VIASETREG(0x43C, 0x00fe0000);

        if (pVia->ChipRev >= 3 )
            VIASETREG(0x440,0x40008c0f);
        else
            VIASETREG(0x440,0x4000800f);

        VIASETREG(0x440,0x44000000);
        VIASETREG(0x440,0x45080C04);
        VIASETREG(0x440,0x46800408);
        VIASETREG(0x440,0x50000000);
        VIASETREG(0x440,0x51000000);
        VIASETREG(0x440,0x52000000);
        VIASETREG(0x440,0x53000000);

        pVia->sharedData->b3DRegsInitialized = 1;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
            "3D Engine has been initialized.\n");
    }

    VIASETREG(0x43C,0x00fe0000);
    VIASETREG(0x440,0x08000001);
    VIASETREG(0x440,0x0A000183);
    VIASETREG(0x440,0x0B00019F);
    VIASETREG(0x440,0x0C00018B);
    VIASETREG(0x440,0x0D00019B);
    VIASETREG(0x440,0x0E000000);
    VIASETREG(0x440,0x0F000000);
    VIASETREG(0x440,0x10000000);
    VIASETREG(0x440,0x11000000);
    VIASETREG(0x440,0x20000000);
}
#endif
