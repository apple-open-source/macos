/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_driver.c,v 1.118 2004/02/26 04:25:29 martin Exp $ */
/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * Credits:
 *
 *   Thanks to Ani Joshi <ajoshi@shell.unixbox.com> for providing source
 *   code to his Radeon driver.  Portions of this file are based on the
 *   initialization code for that driver.
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 * This server does not yet support these XFree86 4.0 features:
 * !!!! FIXME !!!!
 *   DDC1 & DDC2
 *   shadowfb (Note: dri uses shadowfb for another purpose in radeon_dri.c)
 *   overlay planes
 *
 * Modified by Marc Aurele La France (tsi@xfree86.org) for ATI driver merge.
 */

				/* Driver data structures */
#include "radeon.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_reg.h"
#include "radeon_version.h"

#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "radeon_dri.h"
#include "radeon_sarea.h"
#endif

#include "fb.h"

				/* colormap initialization */
#include "micmap.h"
#include "dixstruct.h"

				/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86PciInfo.h"
#include "xf86RAC.h"
#include "xf86Resources.h"
#include "xf86cmap.h"
#include "vbe.h"

				/* fbdevhw * vgaHW definitions */
#include "fbdevhw.h"
#include "vgaHW.h"

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

				/* Forward definitions for driver functions */
static Bool RADEONCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool RADEONSaveScreen(ScreenPtr pScreen, int mode);
static void RADEONSave(ScrnInfoPtr pScrn);
static void RADEONRestore(ScrnInfoPtr pScrn);
static Bool RADEONModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void RADEONDisplayPowerManagementSet(ScrnInfoPtr pScrn,
					    int PowerManagementMode,
					    int flags);
static void RADEONInitDispBandwidth(ScrnInfoPtr pScrn);

typedef enum {
    OPTION_NOACCEL,
    OPTION_SW_CURSOR,
    OPTION_DAC_6BIT,
    OPTION_DAC_8BIT,
#ifdef XF86DRI
    OPTION_IS_PCI,
    OPTION_BUS_TYPE,
    OPTION_CP_PIO,
    OPTION_USEC_TIMEOUT,
    OPTION_AGP_MODE,
    OPTION_AGP_FW,
    OPTION_GART_SIZE,
    OPTION_RING_SIZE,
    OPTION_BUFFER_SIZE,
    OPTION_DEPTH_MOVE,
    OPTION_PAGE_FLIP,
    OPTION_NO_BACKBUFFER,
#endif
    OPTION_PANEL_OFF,
    OPTION_DDC_MODE,
    OPTION_MONITOR_LAYOUT,
    OPTION_IGNORE_EDID,
    OPTION_CRTC2_OVERLAY,
    OPTION_CLONE_MODE,
    OPTION_CLONE_HSYNC,
    OPTION_CLONE_VREFRESH,
    OPTION_FBDEV,
    OPTION_VIDEO_KEY,
    OPTION_DISP_PRIORITY,
    OPTION_PANEL_SIZE,
    OPTION_MIN_DOTCLOCK
} RADEONOpts;

const OptionInfoRec RADEONOptions[] = {
    { OPTION_NOACCEL,        "NoAccel",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SW_CURSOR,      "SWcursor",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DAC_6BIT,       "Dac6Bit",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DAC_8BIT,       "Dac8Bit",          OPTV_BOOLEAN, {0}, TRUE  },
#ifdef XF86DRI
    { OPTION_IS_PCI,         "ForcePCIMode",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_BUS_TYPE,       "BusType",          OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_CP_PIO,         "CPPIOMode",        OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_USEC_TIMEOUT,   "CPusecTimeout",    OPTV_INTEGER, {0}, FALSE },
    { OPTION_AGP_MODE,       "AGPMode",          OPTV_INTEGER, {0}, FALSE },
    { OPTION_AGP_FW,         "AGPFastWrite",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_GART_SIZE,      "AGPSize",          OPTV_INTEGER, {0}, FALSE },
    { OPTION_GART_SIZE,      "GARTSize",         OPTV_INTEGER, {0}, FALSE },
    { OPTION_RING_SIZE,      "RingSize",         OPTV_INTEGER, {0}, FALSE },
    { OPTION_BUFFER_SIZE,    "BufferSize",       OPTV_INTEGER, {0}, FALSE },
    { OPTION_DEPTH_MOVE,     "EnableDepthMoves", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_PAGE_FLIP,      "EnablePageFlip",   OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_NO_BACKBUFFER,  "NoBackBuffer",     OPTV_BOOLEAN, {0}, FALSE },
#endif
    { OPTION_PANEL_OFF,      "PanelOff",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DDC_MODE,       "DDCMode",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_MONITOR_LAYOUT, "MonitorLayout",    OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_IGNORE_EDID,    "IgnoreEDID",       OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_CRTC2_OVERLAY , "OverlayOnCRTC2",   OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_CLONE_MODE,     "CloneMode",        OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_CLONE_HSYNC,    "CloneHSync",       OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_CLONE_VREFRESH, "CloneVRefresh",    OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_FBDEV,          "UseFBDev",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_VIDEO_KEY,      "VideoKey",         OPTV_INTEGER, {0}, FALSE },
    { OPTION_DISP_PRIORITY,  "DisplayPriority",  OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_PANEL_SIZE,     "PanelSize",        OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_MIN_DOTCLOCK,   "ForceMinDotClock", OPTV_FREQ,    {0}, FALSE },
    { -1,                    NULL,               OPTV_NONE,    {0}, FALSE }
};

static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIndex",
    "vgaHWLock",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWUnlock",
    "vgaHWGetIOBase",
    NULL
};

static const char *fbdevHWSymbols[] = {
    "fbdevHWInit",
    "fbdevHWUseBuildinMode",

    "fbdevHWGetVidmem",

    "fbdevHWDPMSSet",

    /* colormap */
    "fbdevHWLoadPalette",
    /* ScrnInfo hooks */
    "fbdevHWAdjustFrame",
    "fbdevHWEnterVT",
    "fbdevHWLeaveVT",
    "fbdevHWModeInit",
    "fbdevHWRestore",
    "fbdevHWSave",
    "fbdevHWSwitchMode",
    "fbdevHWValidMode",

    "fbdevHWMapMMIO",
    "fbdevHWMapVidmem",
    "fbdevHWUnmapMMIO",
    "fbdevHWUnmapVidmem",

    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    NULL
};

static const char *fbSymbols[] = {
    "fbScreenInit",
    "fbPictureInit",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    NULL
};

#if 0
static const char *xf8_32bppSymbols[] = {
    "xf86Overlay8Plus32Init",
    NULL
};
#endif

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86ForceHWCursor",
    "xf86InitCursor",
    NULL
};

#ifdef XF86DRI
static const char *drmSymbols[] = {
    "drmGetInterruptFromBusID",
    "drmCtlInstHandler",
    "drmCtlUninstHandler",
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
    "drmAgpUnbind",
    "drmAgpVendorId",
    "drmCommandNone",
    "drmCommandRead",
    "drmCommandWrite",
    "drmCommandWriteRead",
    "drmDMA",
    "drmFreeVersion",
    "drmGetLibVersion",
    "drmGetVersion",
    "drmMap",
    "drmMapBufs",
    "drmRadeonCleanupCP",
    "drmRadeonClear",
    "drmRadeonFlushIndirectBuffer",
    "drmRadeonInitCP",
    "drmRadeonResetCP",
    "drmRadeonStartCP",
    "drmRadeonStopCP",
    "drmRadeonWaitForIdleCP",
    "drmScatterGatherAlloc",
    "drmScatterGatherFree",
    "drmUnmap",
    "drmUnmapBufs",
    NULL
};

static const char *driSymbols[] = {
    "DRICloseScreen",
    "DRICreateInfoRec",
    "DRIDestroyInfoRec",
    "DRIFinishScreenInit",
    "DRIGetContext",
    "DRIGetDeviceInfo",
    "DRIGetSAREAPrivate",
    "DRILock",
    "DRIQueryVersion",
    "DRIScreenInit",
    "DRIUnlock",
    "GlxSetVisualConfigs",
    NULL
};

static const char *driShadowFBSymbols[] = {
    "ShadowFBInit",
    NULL
};
#endif

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    NULL
};

static const char *int10Symbols[] = {
    "xf86InitInt10",
    "xf86FreeInt10",
    "xf86int10Addr",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

void RADEONLoaderRefSymLists(void)
{
    /*
     * Tell the loader about symbols from other modules that this module might
     * refer to.
     */
    xf86LoaderRefSymLists(vgahwSymbols,
			  fbSymbols,
			  xaaSymbols,
#if 0
			  xf8_32bppSymbols,
#endif
			  ramdacSymbols,
#ifdef XF86DRI
			  drmSymbols,
			  driSymbols,
			  driShadowFBSymbols,
#endif
			  fbdevHWSymbols,
			  vbeSymbols,
			  int10Symbols,
			  i2cSymbols,
			  ddcSymbols,
			  NULL);
}

/* Established timings from EDID standard */
static struct
{
    int hsize;
    int vsize;
    int refresh;
} est_timings[] = {
    {1280, 1024, 75},
    {1024, 768, 75},
    {1024, 768, 70},
    {1024, 768, 60},
    {1024, 768, 87},
    {832, 624, 75},
    {800, 600, 75},
    {800, 600, 72},
    {800, 600, 60},
    {800, 600, 56},
    {640, 480, 75},
    {640, 480, 72},
    {640, 480, 67},
    {640, 480, 60},
    {720, 400, 88},
    {720, 400, 70},
};

static const RADEONTMDSPll default_tmds_pll[CHIP_FAMILY_LAST][4] =
{
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_UNKNOW*/
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_LEGACY*/
    {{12000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RADEON*/
    {{12000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV100*/
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_RS100*/
    {{15000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV200*/
    {{12000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RS200*/
    {{15000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_R200*/
    {{15500, 0x81b}, {0xffffffff, 0x83f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV250*/
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_RS300*/
    {{13000, 0x400f4}, {15000, 0x400f7}, {0xffffffff, 0x400f7/*0x40111*/}, {0, 0}},	/*CHIP_FAMILY_RV280*/
    {{0xffffffff, 0xb01cb}, {0, 0}, {0, 0}, {0, 0}},		/*CHIP_FAMILY_R300*/
    {{0xffffffff, 0xb01cb}, {0, 0}, {0, 0}, {0, 0}},		/*CHIP_FAMILY_R350*/
    {{15000, 0xb0155}, {0xffffffff, 0xb01cb}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV350*/
};

extern int gRADEONEntityIndex;

struct RADEONInt10Save {
	CARD32 MEM_CNTL;
	CARD32 MEMSIZE;
	CARD32 MPP_TB_CONFIG;
};

static Bool RADEONMapMMIO(ScrnInfoPtr pScrn);
static Bool RADEONUnmapMMIO(ScrnInfoPtr pScrn);

static RADEONEntPtr RADEONEntPriv(ScrnInfoPtr pScrn)
{
    DevUnion     *pPriv;
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    pPriv = xf86GetEntityPrivate(info->pEnt->index,
                                 gRADEONEntityIndex);
    return pPriv->ptr;
}

static void
RADEONPreInt10Save(ScrnInfoPtr pScrn, void **pPtr)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 CardTmp;
    static struct RADEONInt10Save SaveStruct = { 0, 0, 0 };

    /* Save the values and zap MEM_CNTL */
    SaveStruct.MEM_CNTL = INREG(RADEON_MEM_CNTL);
    SaveStruct.MEMSIZE = INREG(RADEON_CONFIG_MEMSIZE);
    SaveStruct.MPP_TB_CONFIG = INREG(RADEON_MPP_TB_CONFIG);

    /*
     * Zap MEM_CNTL and set MPP_TB_CONFIG<31:24> to 4
     */
    OUTREG(RADEON_MEM_CNTL, 0);
    CardTmp = SaveStruct.MPP_TB_CONFIG & 0x00ffffffu;
    CardTmp |= 0x04 << 24;
    OUTREG(RADEON_MPP_TB_CONFIG, CardTmp);

    *pPtr = (void *)&SaveStruct;
}

static void
RADEONPostInt10Check(ScrnInfoPtr pScrn, void *ptr)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    struct RADEONInt10Save *pSave = ptr;
    CARD32 CardTmp;

    /* If we don't have a valid (non-zero) saved MEM_CNTL, get out now */
    if (!pSave || !pSave->MEM_CNTL)
	return;

    /*
     * If either MEM_CNTL is currently zero or inconistent (configured for
     * two channels with the two channels configured differently), restore
     * the saved registers.
     */
    CardTmp = INREG(RADEON_MEM_CNTL);
    if (!CardTmp ||
	((CardTmp & 1) &&
	 (((CardTmp >> 8) & 0xff) != ((CardTmp >> 24) & 0xff)))) {
	/* Restore the saved registers */
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Restoring MEM_CNTL (%08lx), setting to %08lx\n",
		   (unsigned long)CardTmp, (unsigned long)pSave->MEM_CNTL);
	OUTREG(RADEON_MEM_CNTL, pSave->MEM_CNTL);

	CardTmp = INREG(RADEON_CONFIG_MEMSIZE);
	if (CardTmp != pSave->MEMSIZE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Restoring CONFIG_MEMSIZE (%08lx), setting to %08lx\n",
		       (unsigned long)CardTmp, (unsigned long)pSave->MEMSIZE);
	    OUTREG(RADEON_CONFIG_MEMSIZE, pSave->MEMSIZE);
	}
    }

    CardTmp = INREG(RADEON_MPP_TB_CONFIG);
    if ((CardTmp & 0xff000000u) != (pSave->MPP_TB_CONFIG & 0xff000000u)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Restoring MPP_TB_CONFIG<31:24> (%02lx), setting to %02lx\n",
		   (unsigned long)CardTmp >> 24,
		   (unsigned long)pSave->MPP_TB_CONFIG >> 24);
	CardTmp &= 0x00ffffffu;
	CardTmp |= (pSave->MPP_TB_CONFIG & 0xff000000u);
	OUTREG(RADEON_MPP_TB_CONFIG, CardTmp);
    }
}

/* Allocate our private RADEONInfoRec */
static Bool RADEONGetRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate) return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(RADEONInfoRec), 1);
    return TRUE;
}

/* Free our private RADEONInfoRec */
static void RADEONFreeRec(ScrnInfoPtr pScrn)
{
    if (!pScrn || !pScrn->driverPrivate) return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

/* Memory map the MMIO region.  Used during pre-init and by RADEONMapMem,
 * below
 */
static Bool RADEONMapMMIO(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev) {
	info->MMIO = fbdevHWMapMMIO(pScrn);
    } else {
	info->MMIO = xf86MapPciMem(pScrn->scrnIndex,
				   VIDMEM_MMIO | VIDMEM_READSIDEEFFECT,
				   info->PciTag,
				   info->MMIOAddr,
				   RADEON_MMIOSIZE);
    }

    if (!info->MMIO) return FALSE;
    return TRUE;
}

/* Unmap the MMIO region.  Used during pre-init and by RADEONUnmapMem,
 * below
 */
static Bool RADEONUnmapMMIO(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev)
	fbdevHWUnmapMMIO(pScrn);
    else {
	xf86UnMapVidMem(pScrn->scrnIndex, info->MMIO, RADEON_MMIOSIZE);
    }
    info->MMIO = NULL;
    return TRUE;
}

/* Memory map the frame buffer.  Used by RADEONMapMem, below. */
static Bool RADEONMapFB(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev) {
	info->FB = fbdevHWMapVidmem(pScrn);
    } else {
	info->FB = xf86MapPciMem(pScrn->scrnIndex,
				 VIDMEM_FRAMEBUFFER,
				 info->PciTag,
				 info->LinearAddr,
				 info->FbMapSize);
    }

    if (!info->FB) return FALSE;
    return TRUE;
}

/* Unmap the frame buffer.  Used by RADEONUnmapMem, below. */
static Bool RADEONUnmapFB(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev)
	fbdevHWUnmapVidmem(pScrn);
    else
	xf86UnMapVidMem(pScrn->scrnIndex, info->FB, info->FbMapSize);
    info->FB = NULL;
    return TRUE;
}

/* Memory map the MMIO region and the frame buffer */
static Bool RADEONMapMem(ScrnInfoPtr pScrn)
{
    if (!RADEONMapMMIO(pScrn)) return FALSE;
    if (!RADEONMapFB(pScrn)) {
	RADEONUnmapMMIO(pScrn);
	return FALSE;
    }
    return TRUE;
}

/* Unmap the MMIO region and the frame buffer */
static Bool RADEONUnmapMem(ScrnInfoPtr pScrn)
{
    if (!RADEONUnmapMMIO(pScrn) || !RADEONUnmapFB(pScrn)) return FALSE;
    return TRUE;
}

/* This function is required to workaround a hardware bug in some (all?)
 * revisions of the R300.  This workaround should be called after every
 * CLOCK_CNTL_INDEX register access.  If not, register reads afterward
 * may not be correct.
 */
void R300CGWorkaround(ScrnInfoPtr pScrn) {
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32         save, tmp;

    save = INREG(RADEON_CLOCK_CNTL_INDEX);
    tmp = save & ~(0x3f | RADEON_PLL_WR_EN);
    OUTREG(RADEON_CLOCK_CNTL_INDEX, tmp);
    tmp = INREG(RADEON_CLOCK_CNTL_DATA);
    OUTREG(RADEON_CLOCK_CNTL_INDEX, save);
}

/* Read PLL information */
unsigned RADEONINPLL(ScrnInfoPtr pScrn, int addr)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32         data;

    OUTREG8(RADEON_CLOCK_CNTL_INDEX, addr & 0x3f);
    data = INREG(RADEON_CLOCK_CNTL_DATA);
    if (info->R300CGWorkaround) R300CGWorkaround(pScrn);

    return data;
}

#if 0
/* Read PAL information (only used for debugging) */
static int RADEONINPAL(int idx)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_PALETTE_INDEX, idx << 16);
    return INREG(RADEON_PALETTE_DATA);
}
#endif

/* Wait for vertical sync on primary CRTC */
void RADEONWaitForVerticalSync(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i;

    /* Clear the CRTC_VBLANK_SAVE bit */
    OUTREG(RADEON_CRTC_STATUS, RADEON_CRTC_VBLANK_SAVE_CLEAR);

    /* Wait for it to go back up */
    for (i = 0; i < RADEON_TIMEOUT/1000; i++) {
	if (INREG(RADEON_CRTC_STATUS) & RADEON_CRTC_VBLANK_SAVE) break;
	usleep(1);
    }
}

/* Wait for vertical sync on secondary CRTC */
void RADEONWaitForVerticalSync2(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i;

    /* Clear the CRTC2_VBLANK_SAVE bit */
    OUTREG(RADEON_CRTC2_STATUS, RADEON_CRTC2_VBLANK_SAVE_CLEAR);

    /* Wait for it to go back up */
    for (i = 0; i < RADEON_TIMEOUT/1000; i++) {
	if (INREG(RADEON_CRTC2_STATUS) & RADEON_CRTC2_VBLANK_SAVE) break;
	usleep(1);
    }
}

/* Blank screen */
static void RADEONBlank(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (!info->IsSecondary) {
	switch(info->DisplayType) {
	case MT_LCD:
	case MT_CRT:
	case MT_DFP:
	    OUTREGP(RADEON_CRTC_EXT_CNTL,
		    RADEON_CRTC_DISPLAY_DIS,
		    ~(RADEON_CRTC_DISPLAY_DIS));
	    break;

	case MT_NONE:
	default:
	    break;
	}
	if (info->Clone)
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    RADEON_CRTC2_DISP_DIS,
		    ~(RADEON_CRTC2_DISP_DIS));
    } else {
	OUTREGP(RADEON_CRTC2_GEN_CNTL,
		RADEON_CRTC2_DISP_DIS,
		~(RADEON_CRTC2_DISP_DIS));
    }
}

/* Unblank screen */
static void RADEONUnblank(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (!info->IsSecondary) {
	switch (info->DisplayType) {
	case MT_LCD:
	case MT_CRT:
	case MT_DFP:
	    OUTREGP(RADEON_CRTC_EXT_CNTL,
		    RADEON_CRTC_CRT_ON,
		    ~(RADEON_CRTC_DISPLAY_DIS));
	    break;

	case MT_NONE:
	default:
	    break;
	}
	if (info->Clone)
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    0,
		    ~(RADEON_CRTC2_DISP_DIS));
    } else {
	switch (info->DisplayType) {
	case MT_LCD:
	case MT_DFP:
	case MT_CRT:
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    0,
		    ~(RADEON_CRTC2_DISP_DIS));
	    break;

	case MT_NONE:
	default:
	    break;
	}
    }
}

/* Compute log base 2 of val */
int RADEONMinBits(int val)
{
    int  bits;

    if (!val) return 1;
    for (bits = 0; val; val >>= 1, ++bits);
    return bits;
}

/* Compute n/d with rounding */
static int RADEONDiv(int n, int d)
{
    return (n + (d / 2)) / d;
}

static RADEONMonitorType RADEONDisplayDDCConnected(ScrnInfoPtr pScrn, RADEONDDCType DDCType, xf86MonPtr* MonInfo)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long DDCReg;
    RADEONMonitorType MonType = MT_NONE;
    int i, j;

    DDCReg = info->DDCReg;
    switch(DDCType)
    {
    case DDC_MONID:
	info->DDCReg = RADEON_GPIO_MONID;
	break;
    case DDC_DVI:
	info->DDCReg = RADEON_GPIO_DVI_DDC;
	break;
    case DDC_VGA:
	info->DDCReg = RADEON_GPIO_VGA_DDC;
	break;
    case DDC_CRT2:
	info->DDCReg = RADEON_GPIO_CRT2_DDC;
	break;
    default:
	info->DDCReg = DDCReg;
	return MT_NONE;
    }

    /* Read and output monitor info using DDC2 over I2C bus */
    if (info->pI2CBus && info->ddc2) {
	OUTREG(info->DDCReg, INREG(info->DDCReg) &
	       (CARD32)~(RADEON_GPIO_A_0 | RADEON_GPIO_A_1));

	/* For some old monitors (like Compaq Presario FP500), we need
	 * following process to initialize/stop DDC
	 */
	OUTREG(info->DDCReg, INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	for (j = 0; j < 3; j++) {
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(13000);

	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 10; i++) {
		usleep(15000);
		if (INREG(info->DDCReg) & RADEON_GPIO_Y_1)
		    break;
	    }
	    if (i == 10) continue;

	    usleep(15000);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(15000);
	    *MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, info->pI2CBus);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 5; i++) {
		usleep(15000);
		if (INREG(info->DDCReg) & RADEON_GPIO_Y_1)
		    break;
	    }
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(15000);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    if(*MonInfo) break;
	}
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DDC2/I2C is not properly initialized\n");
	MonType = MT_NONE;
    }

    if (*MonInfo) {
	if ((*MonInfo)->rawData[0x14] & 0x80) {
	    if (INREG(RADEON_LVDS_GEN_CNTL) & RADEON_LVDS_ON) MonType = MT_LCD;
	    else MonType = MT_DFP;
	} else MonType = MT_CRT;
    } else MonType = MT_NONE;

    info->DDCReg = DDCReg;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "DDC Type: %d, Detected Type: %d\n", DDCType, MonType);

    return MonType;
}

static RADEONMonitorType
RADEONCrtIsPhysicallyConnected(ScrnInfoPtr pScrn, int IsCrtDac)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int		  bConnected = 0;

    /* the monitor either wasn't connected or it is a non-DDC CRT.
     * try to probe it
     */
    if(IsCrtDac) {
	unsigned long ulOrigVCLK_ECP_CNTL;
	unsigned long ulOrigDAC_CNTL;
	unsigned long ulOrigDAC_EXT_CNTL;
	unsigned long ulOrigCRTC_EXT_CNTL;
	unsigned long ulData;
	unsigned long ulMask;

	ulOrigVCLK_ECP_CNTL = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);

	ulData              = ulOrigVCLK_ECP_CNTL;
	ulData             &= ~(RADEON_PIXCLK_ALWAYS_ONb
				| RADEON_PIXCLK_DAC_ALWAYS_ONb);
	ulMask              = ~(RADEON_PIXCLK_ALWAYS_ONb
				|RADEON_PIXCLK_DAC_ALWAYS_ONb);
	OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL, ulData, ulMask);

	ulOrigCRTC_EXT_CNTL = INREG(RADEON_CRTC_EXT_CNTL);
	ulData              = ulOrigCRTC_EXT_CNTL;
	ulData             |= RADEON_CRTC_CRT_ON;
	OUTREG(RADEON_CRTC_EXT_CNTL, ulData);

	ulOrigDAC_EXT_CNTL = INREG(RADEON_DAC_EXT_CNTL);
	ulData             = ulOrigDAC_EXT_CNTL;
	ulData            &= ~RADEON_DAC_FORCE_DATA_MASK;
	ulData            |=  (RADEON_DAC_FORCE_BLANK_OFF_EN
			       |RADEON_DAC_FORCE_DATA_EN
			       |RADEON_DAC_FORCE_DATA_SEL_MASK);
	if ((info->ChipFamily == CHIP_FAMILY_RV250) ||
	    (info->ChipFamily == CHIP_FAMILY_RV280))
	    ulData |= (0x01b6 << RADEON_DAC_FORCE_DATA_SHIFT);
	else
	    ulData |= (0x01ac << RADEON_DAC_FORCE_DATA_SHIFT);

	OUTREG(RADEON_DAC_EXT_CNTL, ulData);

	ulOrigDAC_CNTL     = INREG(RADEON_DAC_CNTL);
	ulData             = ulOrigDAC_CNTL;
	ulData            |= RADEON_DAC_CMP_EN;
	ulData            &= ~(RADEON_DAC_RANGE_CNTL_MASK
			       | RADEON_DAC_PDWN);
	ulData            |= 0x2;
	OUTREG(RADEON_DAC_CNTL, ulData);

	usleep(1000);

	ulData     = INREG(RADEON_DAC_CNTL);
	bConnected =  (RADEON_DAC_CMP_OUTPUT & ulData)?1:0;

	ulData    = ulOrigVCLK_ECP_CNTL;
	ulMask    = 0xFFFFFFFFL;
	OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL, ulData, ulMask);

	OUTREG(RADEON_DAC_CNTL,      ulOrigDAC_CNTL     );
	OUTREG(RADEON_DAC_EXT_CNTL,  ulOrigDAC_EXT_CNTL );
	OUTREG(RADEON_CRTC_EXT_CNTL, ulOrigCRTC_EXT_CNTL);
    } else { /* TV DAC */

        /* This doesn't seem to work reliably (maybe worse on some OEM cards),
           for now we always return false. If one wants to connected a
           non-DDC monitor on the DVI port when CRT port is also connected,
           he will need to explicitly tell the driver in the config file
           with Option MonitorLayout.
        */
        bConnected = FALSE;

#if 0
	if (info->ChipFamily == CHIP_FAMILY_R200) {

	    unsigned long ulOrigGPIO_MONID;
	    unsigned long ulOrigFP2_GEN_CNTL;
	    unsigned long ulOrigDISP_OUTPUT_CNTL;
	    unsigned long ulOrigCRTC2_GEN_CNTL;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_A;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_B;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_C;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_D;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_E;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_F;
	    unsigned long ulOrigCRTC2_H_TOTAL_DISP;
	    unsigned long ulOrigCRTC2_V_TOTAL_DISP;
	    unsigned long ulOrigCRTC2_H_SYNC_STRT_WID;
	    unsigned long ulOrigCRTC2_V_SYNC_STRT_WID;
	    unsigned long ulData, i;

	    ulOrigGPIO_MONID = INREG(RADEON_GPIO_MONID);
	    ulOrigFP2_GEN_CNTL = INREG(RADEON_FP2_GEN_CNTL);
	    ulOrigDISP_OUTPUT_CNTL = INREG(RADEON_DISP_OUTPUT_CNTL);
	    ulOrigCRTC2_GEN_CNTL = INREG(RADEON_CRTC2_GEN_CNTL);
	    ulOrigDISP_LIN_TRANS_GRPH_A = INREG(RADEON_DISP_LIN_TRANS_GRPH_A);
	    ulOrigDISP_LIN_TRANS_GRPH_B = INREG(RADEON_DISP_LIN_TRANS_GRPH_B);
	    ulOrigDISP_LIN_TRANS_GRPH_C = INREG(RADEON_DISP_LIN_TRANS_GRPH_C);
	    ulOrigDISP_LIN_TRANS_GRPH_D = INREG(RADEON_DISP_LIN_TRANS_GRPH_D);
	    ulOrigDISP_LIN_TRANS_GRPH_E = INREG(RADEON_DISP_LIN_TRANS_GRPH_E);
	    ulOrigDISP_LIN_TRANS_GRPH_F = INREG(RADEON_DISP_LIN_TRANS_GRPH_F);

	    ulOrigCRTC2_H_TOTAL_DISP = INREG(RADEON_CRTC2_H_TOTAL_DISP);
	    ulOrigCRTC2_V_TOTAL_DISP = INREG(RADEON_CRTC2_V_TOTAL_DISP);
	    ulOrigCRTC2_H_SYNC_STRT_WID = INREG(RADEON_CRTC2_H_SYNC_STRT_WID);
	    ulOrigCRTC2_V_SYNC_STRT_WID = INREG(RADEON_CRTC2_V_SYNC_STRT_WID);

	    ulData     = INREG(RADEON_GPIO_MONID);
	    ulData    &= ~RADEON_GPIO_A_0;
	    OUTREG(RADEON_GPIO_MONID, ulData);

	    OUTREG(RADEON_FP2_GEN_CNTL, 0x0a000c0c);

	    OUTREG(RADEON_DISP_OUTPUT_CNTL, 0x00000012);

	    OUTREG(RADEON_CRTC2_GEN_CNTL, 0x06000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_A, 0x00000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_B, 0x000003f0);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_C, 0x00000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_D, 0x000003f0);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_E, 0x00000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_F, 0x000003f0);
	    OUTREG(RADEON_CRTC2_H_TOTAL_DISP, 0x01000008);
	    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, 0x00000800);
	    OUTREG(RADEON_CRTC2_V_TOTAL_DISP, 0x00080001);
	    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, 0x00000080);

	    for (i = 0; i < 200; i++) {
		ulData     = INREG(RADEON_GPIO_MONID);
		bConnected = (ulData & RADEON_GPIO_Y_0)?1:0;
		if (!bConnected) break;

		usleep(1000);
	    }

	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_A, ulOrigDISP_LIN_TRANS_GRPH_A);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_B, ulOrigDISP_LIN_TRANS_GRPH_B);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_C, ulOrigDISP_LIN_TRANS_GRPH_C);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_D, ulOrigDISP_LIN_TRANS_GRPH_D);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_E, ulOrigDISP_LIN_TRANS_GRPH_E);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_F, ulOrigDISP_LIN_TRANS_GRPH_F);
	    OUTREG(RADEON_CRTC2_H_TOTAL_DISP, ulOrigCRTC2_H_TOTAL_DISP);
	    OUTREG(RADEON_CRTC2_V_TOTAL_DISP, ulOrigCRTC2_V_TOTAL_DISP);
	    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, ulOrigCRTC2_H_SYNC_STRT_WID);
	    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, ulOrigCRTC2_V_SYNC_STRT_WID);
	    OUTREG(RADEON_CRTC2_GEN_CNTL, ulOrigCRTC2_GEN_CNTL);
	    OUTREG(RADEON_DISP_OUTPUT_CNTL, ulOrigDISP_OUTPUT_CNTL);
	    OUTREG(RADEON_FP2_GEN_CNTL, ulOrigFP2_GEN_CNTL);
	    OUTREG(RADEON_GPIO_MONID, ulOrigGPIO_MONID);
        } else {
	    unsigned long ulOrigPIXCLKSDATA;
	    unsigned long ulOrigTV_MASTER_CNTL;
	    unsigned long ulOrigTV_DAC_CNTL;
	    unsigned long ulOrigTV_PRE_DAC_MUX_CNTL;
	    unsigned long ulOrigDAC_CNTL2;
	    unsigned long ulData;
	    unsigned long ulMask;

	    ulOrigPIXCLKSDATA = INPLL(pScrn, RADEON_PIXCLKS_CNTL);

	    ulData            = ulOrigPIXCLKSDATA;
	    ulData           &= ~(RADEON_PIX2CLK_ALWAYS_ONb
				  | RADEON_PIX2CLK_DAC_ALWAYS_ONb);
	    ulMask            = ~(RADEON_PIX2CLK_ALWAYS_ONb
			  | RADEON_PIX2CLK_DAC_ALWAYS_ONb);
	    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, ulData, ulMask);

	    ulOrigTV_MASTER_CNTL = INREG(RADEON_TV_MASTER_CNTL);
	    ulData               = ulOrigTV_MASTER_CNTL;
	    ulData              &= ~RADEON_TVCLK_ALWAYS_ONb;
	    OUTREG(RADEON_TV_MASTER_CNTL, ulData);

	    ulOrigDAC_CNTL2 = INREG(RADEON_DAC_CNTL2);
	    ulData          = ulOrigDAC_CNTL2;
	    ulData          &= ~RADEON_DAC2_DAC2_CLK_SEL;
	    OUTREG(RADEON_DAC_CNTL2, ulData);

	    ulOrigTV_DAC_CNTL = INREG(RADEON_TV_DAC_CNTL);

	    ulData  = 0x00880213;
	    OUTREG(RADEON_TV_DAC_CNTL, ulData);

	    ulOrigTV_PRE_DAC_MUX_CNTL = INREG(RADEON_TV_PRE_DAC_MUX_CNTL);

	    ulData  =  (RADEON_Y_RED_EN
			| RADEON_C_GRN_EN
			| RADEON_CMP_BLU_EN
			| RADEON_RED_MX_FORCE_DAC_DATA
			| RADEON_GRN_MX_FORCE_DAC_DATA
			| RADEON_BLU_MX_FORCE_DAC_DATA);
            if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		(info->ChipFamily == CHIP_FAMILY_R350) ||
		(info->ChipFamily == CHIP_FAMILY_RV350))
		ulData |= 0x180 << RADEON_TV_FORCE_DAC_DATA_SHIFT;
	    else
		ulData |= 0x1f5 << RADEON_TV_FORCE_DAC_DATA_SHIFT;
	    OUTREG(RADEON_TV_PRE_DAC_MUX_CNTL, ulData);

	    usleep(1000);

	    ulData     = INREG(RADEON_TV_DAC_CNTL);
	    bConnected = (ulData & RADEON_TV_DAC_CMPOUT)?1:0;

	    ulData    = ulOrigPIXCLKSDATA;
	    ulMask    = 0xFFFFFFFFL;
	    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, ulData, ulMask);

	    OUTREG(RADEON_TV_MASTER_CNTL, ulOrigTV_MASTER_CNTL);
	    OUTREG(RADEON_DAC_CNTL2, ulOrigDAC_CNTL2);
	    OUTREG(RADEON_TV_DAC_CNTL, ulOrigTV_DAC_CNTL);
	    OUTREG(RADEON_TV_PRE_DAC_MUX_CNTL, ulOrigTV_PRE_DAC_MUX_CNTL);
	}
#endif
    }

    return(bConnected ? MT_CRT : MT_NONE);
}

static void RADEONQueryConnectedDisplays(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    RADEONInfoPtr info        = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    const char *s;
    Bool ignore_edid = FALSE, ddc_crt2_used = FALSE;

#define RADEON_BIOS8(v)  (info->VBIOS[v])
#define RADEON_BIOS16(v) (info->VBIOS[v] | \
			  (info->VBIOS[(v) + 1] << 8))
#define RADEON_BIOS32(v) (info->VBIOS[v] | \
			  (info->VBIOS[(v) + 1] << 8) | \
			  (info->VBIOS[(v) + 2] << 16) | \
			  (info->VBIOS[(v) + 3] << 24))

    pRADEONEnt->MonType1 = MT_NONE;
    pRADEONEnt->MonType2 = MT_NONE;
    pRADEONEnt->MonInfo1 = NULL;
    pRADEONEnt->MonInfo2 = NULL;
    pRADEONEnt->ReversedDAC = FALSE;
    pRADEONEnt->ReversedTMDS = FALSE;

    /* IgnoreEDID option is different from NoDDC options used by DDC module
     * When IgnoreEDID is used, monitor detection will still use DDC
     * detection, but all EDID data will not be used in mode validation.
     */
    if (xf86GetOptValBool(info->Options, OPTION_IGNORE_EDID, &ignore_edid)) {
	if (ignore_edid)
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "IgnoreEDID is specified, EDID data will be ignored\n");
    }

    /*
     * MonitorLayout option takes a string for two monitors connected in following format:
     * Option "MonitorLayout" "primary-port-display, secondary-port-display"
     * primary and secondary port displays can have one of following:
     *    NONE, CRT, LVDS, TMDS
     * With this option, driver will bring up monitors as specified,
     * not using auto-detection routines to probe monitors.
     */

    /* current monitor mapping scheme:
     *  Two displays connected:
     *     Primary Port:
     *     CRTC1 -> FP/TMDS  -> DVI port -> TMDS panel  --> Primary or
     *     CRTC1 -> FP/LVDS  -> Int. LCD -> LVDS panel  --> Primary or
     *     CRTC1 -> TV DAC   -> DVI port -> CRT monitor --> Primary
     *
     *     Secondary Port
     *     CRTC2 -> CRT DAC  -> VGA port -> CRT monitor --> Secondary or
     *     CRTC2 -> FP2/Ext. -> DVI port -> TMDS panel  --> Secondary
     *
     *  Only DVI (or Int. LDC) conneced:
     *     CRTC1 -> FP/TMDS  -> DVI port -> TMDS panel  --> Primary or
     *     CRTC1 -> FP/LVDS  -> Int. LCD -> LVDS panel  --> Primary or
     *     CRTC1 -> TV DAC   -> DVI port -> CRT monitor --> Primary
     *
     *  Only VGA (can be DVI on some dual-DVI boards) connected:
     *     CRTC1 -> CRT DAC  -> VGA port -> CRT monitor --> Primary or
     *     CRTC1 -> FP2/Ext. -> DVI port -> TMDS panel  --> Primary (not supported)
     *
     * Note, this is different from Windows scheme where
     *   if a digital panel is connected to DVI port, DVI will be the 1st port
     *   otherwise, VGA port will be treated as 1st port
     *
     *   Here we always treat DVI port as primary if both ports are connected.
     *   When only one port is connected, it will be treated as
     *   primary regardless which port or what type of display is involved.
     */

    if ((s = xf86GetOptValString(info->Options, OPTION_MONITOR_LAYOUT))) {
	char s1[5], s2[5];
	int i = 0, second = 0;

	/* When using user specified monitor types, we will not do DDC detection
	 *
	 */
	do {
	    switch(*s)
            {
            case ',':
		s1[i] = '\0';
		i = 0;
		second = 1;
		break;
	    case ' ':
	    case '\t':
	    case '\n':
	    case '\r':
		break;
	    default:
		if (second)
		    s2[i] = *s;
		else
		    s1[i] = *s;
		i++;
		if (i == 4) break;
	    }
	} while(*s++);
	s2[i] = '\0';

	if (strcmp(s1, "NONE") == 0)
	    pRADEONEnt->MonType1 = MT_NONE;
	else if (strcmp(s1, "CRT") == 0)
	    pRADEONEnt->MonType1 = MT_CRT;
	else if (strcmp(s1, "TMDS") == 0)
	    pRADEONEnt->MonType1 = MT_DFP;
	else if (strcmp(s1, "LVDS") == 0)
	    pRADEONEnt->MonType1 = MT_LCD;
	else
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Invalid Monitor type specified for 1st port \n");
	if (strcmp(s2, "NONE") == 0)
	    pRADEONEnt->MonType2 = MT_NONE;
	else if (strcmp(s2, "CRT") == 0)
	    pRADEONEnt->MonType2 = MT_CRT;
	else if (strcmp(s2, "TMDS") == 0)
	    pRADEONEnt->MonType2 = MT_DFP;
	else if (strcmp(s2, "LVDS") == 0)
	    pRADEONEnt->MonType2 = MT_LCD;
	else
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Invalid Monitor type specified for 2nd port \n");

	if (!ignore_edid) {
	    if (pRADEONEnt->MonType1)  /* assuming the first port using DDC_DVI */
		if(!RADEONDisplayDDCConnected(pScrn, DDC_DVI, &pRADEONEnt->MonInfo1)) {
		    RADEONDisplayDDCConnected(pScrn, DDC_CRT2, &pRADEONEnt->MonInfo1);
		    ddc_crt2_used = TRUE;
		}
	    if (pRADEONEnt->MonType2) {  /* assuming the second port using DDC_VGA/DDC_CRT2 */
		if(!RADEONDisplayDDCConnected(pScrn, DDC_VGA, &pRADEONEnt->MonInfo2))
		    if (!ddc_crt2_used)
			RADEONDisplayDDCConnected(pScrn, DDC_CRT2, &pRADEONEnt->MonInfo2);
	    }
	}

	if (!pRADEONEnt->MonType1) {
	    if (pRADEONEnt->MonType2) {
		pRADEONEnt->MonType1 = pRADEONEnt->MonType2;
		pRADEONEnt->MonInfo1 = pRADEONEnt->MonInfo2;
	    } else {
		pRADEONEnt->MonType1 = MT_CRT;
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                       "No valid monitor specified, force to CRT on 1st port\n");
	    }
	    pRADEONEnt->MonType2 = MT_NONE;
	    pRADEONEnt->MonInfo2 = NULL;
	}
    } else {
	/* Auto detection */
	int i;
	CARD32 tmp;

	/* Old single head radeon cards */
        if(!info->HasCRTC2) {
	    if((pRADEONEnt->MonType1 = RADEONDisplayDDCConnected(pScrn, DDC_DVI, &pRADEONEnt->MonInfo1)));
	    else if((pRADEONEnt->MonType1 = RADEONDisplayDDCConnected(pScrn, DDC_VGA, &pRADEONEnt->MonInfo1)));
	    else if((pRADEONEnt->MonType1 = RADEONDisplayDDCConnected(pScrn, DDC_CRT2, &pRADEONEnt->MonInfo1)));
	    else if (pInt10) {
		if (xf86LoadSubModule(pScrn, "vbe")) {
		    vbeInfoPtr  pVbe;
		    pVbe = VBEInit(pInt10, info->pEnt->index);
		    if (pVbe) {
			for (i = 0; i < 5; i++) {
			    pRADEONEnt->MonInfo1 = vbeDoEDID(pVbe, NULL);
			}
			if (pRADEONEnt->MonInfo1->rawData[0x14] & 0x80)
			    pRADEONEnt->MonType1 = MT_DFP;
			else pRADEONEnt->MonType1 = MT_CRT;
		    }
		}
	    } else
		pRADEONEnt->MonType1 = MT_CRT;

	    pRADEONEnt->HasSecondary = FALSE;
	    if (!ignore_edid) {
		if (pRADEONEnt->MonInfo1) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Monitor1 EDID data ---------------------------\n");
		    xf86PrintEDID( pRADEONEnt->MonInfo1 );
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "End of Monitor1 EDID data --------------------\n");
		}
	    }
	    return;
	}

	/* Normally the port uses DDC_DVI connected with TVDAC,
	 * But this is not true for OEM cards which have TVDAC and CRT DAC reversed.
	 * If that's the case, we need also reverse the port arrangement.
	 * BIOS settings are supposed report this correctly, work fine for all cards tested.
	 * But there may be some exceptions, in that case, user can reverse their monitor
	 * definition in config file to correct the problem.
	 */
	if (info->VBIOS && (tmp = RADEON_BIOS16(info->FPBIOSstart + 0x50))) {
	    for (i = 1; i < 4; i++) {
		unsigned int tmp0;
		if (!RADEON_BIOS8(tmp + i*2) && i > 1) break;
		tmp0 = RADEON_BIOS16(tmp + i*2);
		if ((!(tmp0 & 0x01)) && (((tmp0 >> 8) & 0xf) == DDC_DVI)) {
		    pRADEONEnt->ReversedDAC = TRUE;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Reversed DACs detected\n");
		}
		if ((((tmp0 >> 8) & 0x0f) == DDC_DVI ) && ((tmp0 >> 4) & 0x1)) {
		    pRADEONEnt->ReversedTMDS = TRUE;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Reversed TMDS detected\n");
		}
	    }
	}

	/* Primary Head (DVI or Laptop Int. panel)*/
	/* A ddc capable display connected on DVI port */
	if((pRADEONEnt->MonType1 = RADEONDisplayDDCConnected(pScrn, DDC_DVI, &pRADEONEnt->MonInfo1)));
	else if((pRADEONEnt->MonType1 =
		 RADEONDisplayDDCConnected(pScrn, DDC_CRT2, &pRADEONEnt->MonInfo1))) {
	  ddc_crt2_used = TRUE;
	} else if ((info->IsMobility) &&
		   (info->VBIOS && (INREG(RADEON_BIOS_4_SCRATCH) & 4))) {
	    /* non-DDC laptop panel connected on primary */
	    pRADEONEnt->MonType1 = MT_LCD;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Non-DDC laptop panel detected\n");
	} else {
	    /* CRT on DVI, TODO: not reliable, make it always return false for now*/
	    pRADEONEnt->MonType1 = RADEONCrtIsPhysicallyConnected(pScrn, pRADEONEnt->ReversedDAC);
	}

	/* Secondary Head (mostly VGA, can be DVI on some OEM boards)*/
	if((pRADEONEnt->MonType2 =
	    RADEONDisplayDDCConnected(pScrn, DDC_VGA, &pRADEONEnt->MonInfo2)));
	else if(!ddc_crt2_used)
	  pRADEONEnt->MonType2 =
		 RADEONDisplayDDCConnected(pScrn, DDC_CRT2, &pRADEONEnt->MonInfo2);
	if (!pRADEONEnt->MonType2)
	    pRADEONEnt->MonType2 = RADEONCrtIsPhysicallyConnected(pScrn, !pRADEONEnt->ReversedDAC);

	if(pRADEONEnt->ReversedTMDS) {
	    /* always keep internal TMDS as primary head */
	    if (pRADEONEnt->MonType1 == MT_DFP ||
		pRADEONEnt->MonType2 == MT_DFP) {
		int tmp1 = pRADEONEnt->MonType1;
		xf86MonPtr MonInfo = pRADEONEnt->MonInfo1;
		pRADEONEnt->MonInfo1 = pRADEONEnt->MonInfo2;
		pRADEONEnt->MonInfo2 = MonInfo;
		pRADEONEnt->MonType1 = pRADEONEnt->MonType2;
		pRADEONEnt->MonType2 = tmp1;
		if ((pRADEONEnt->MonType1 == MT_CRT) ||
		    (pRADEONEnt->MonType2 == MT_CRT)) {
		    pRADEONEnt->ReversedDAC ^= 1;
		}
	    }
	}

	/* no display detected on DVI port*/
	if (pRADEONEnt->MonType1 == MT_NONE) {
	    if (pRADEONEnt->MonType2 != MT_NONE) {
		/* Only one detected on VGA, let it to be primary */
		pRADEONEnt->MonType1 = pRADEONEnt->MonType2;
		pRADEONEnt->MonInfo1 = pRADEONEnt->MonInfo2;
		pRADEONEnt->MonType2 = MT_NONE;
		pRADEONEnt->MonInfo2 = NULL;
	    } else {
		/* Non detected, Default to a CRT connected */
		pRADEONEnt->MonType1 = MT_CRT;
	    }
	}
    }

    if(s) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Displays Configured by MonitorLayout: \n\tMonitor1--Type %d, Monitor2--Type %d\n\n",
		   pRADEONEnt->MonType1, pRADEONEnt->MonType2);
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Displays Detected: Monitor1--Type %d, Monitor2--Type %d\n\n",
		   pRADEONEnt->MonType1, pRADEONEnt->MonType2);
    }

    if(ignore_edid) {
	pRADEONEnt->MonInfo1 = NULL;
	pRADEONEnt->MonInfo2 = NULL;
    }

    if (!ignore_edid) {
	if (pRADEONEnt->MonInfo1) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Monitor1 EDID data ---------------------------\n");
	    xf86PrintEDID( pRADEONEnt->MonInfo1 );
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "End of Monitor1 EDID data --------------------\n");
	}
	if (pRADEONEnt->MonInfo2) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Monitor2 EDID data ---------------------------\n");
	    xf86PrintEDID( pRADEONEnt->MonInfo2 );
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "End of Monitor2 EDID data --------------------\n");
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "\n");

    info->OverlayOnCRTC2 = FALSE;
   if (xf86ReturnOptValBool(info->Options, OPTION_CRTC2_OVERLAY, FALSE)) {
	info->OverlayOnCRTC2 = TRUE;
    }

    if (pRADEONEnt->MonType2 == MT_NONE)
	pRADEONEnt->HasSecondary = FALSE;
}


/* Read the Video BIOS block and the FP registers (if applicable). */
static Bool RADEONGetBIOSParameters(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    RADEONInfoPtr  info            = RADEONPTR(pScrn);
    unsigned long  tmp, i;

    if (!(info->VBIOS = xalloc(RADEON_VBIOS_SIZE))) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Cannot allocate space for hold Video BIOS!\n");
	return FALSE;
    }

    if (pInt10) {
	info->BIOSAddr = pInt10->BIOSseg << 4;
	(void)memcpy(info->VBIOS, xf86int10Addr(pInt10, info->BIOSAddr),
		     RADEON_VBIOS_SIZE);
    } else {
	xf86ReadPciBIOS(0, info->PciTag, 0, info->VBIOS, RADEON_VBIOS_SIZE);
	if (info->VBIOS[0] != 0x55 || info->VBIOS[1] != 0xaa) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Video BIOS not detected in PCI space!\n");
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Attempting to read Video BIOS from "
		       "legacy ISA space!\n");
	    info->BIOSAddr = 0x000c0000;
	    xf86ReadDomainMemory(info->PciTag, info->BIOSAddr,
				 RADEON_VBIOS_SIZE, info->VBIOS);
	}
    }

    if (info->VBIOS[0] != 0x55 || info->VBIOS[1] != 0xaa) {
	xfree(info->VBIOS);
	info->FPBIOSstart = 0;
	info->VBIOS = NULL;
	info->BIOSAddr = 0x00000000;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Video BIOS not found!\n");
    } else
	info->FPBIOSstart = RADEON_BIOS16(0x48);
    info->OverlayOnCRTC2 = FALSE;

    if (!info->IsSecondary)
	RADEONQueryConnectedDisplays(pScrn, pInt10);

    {
	RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

	info->Clone = FALSE;
	info->CloneType = MT_NONE;

	if(info->HasCRTC2) {
	    if(info->IsSecondary) {
		info->DisplayType = (RADEONMonitorType)pRADEONEnt->MonType2;
		if(info->DisplayType == MT_NONE) return FALSE;
	    } else {
		info->DisplayType = (RADEONMonitorType)pRADEONEnt->MonType1;

		if(!pRADEONEnt->HasSecondary) {
		    info->CloneType = (RADEONMonitorType)pRADEONEnt->MonType2;
		    if (info->CloneType != MT_NONE)
			info->Clone = TRUE;
		}
	    }
	} else {
	    info->DisplayType = (RADEONMonitorType)pRADEONEnt->MonType1;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s Display == Type %d\n",
		   (info->IsSecondary ? "Secondary" : "Primary"),
		   info->DisplayType);

	if (info->Clone)
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Clone Display == Type %d\n",
		       info->CloneType);

	info->HBlank     = 0;
	info->HOverPlus  = 0;
	info->HSyncWidth = 0;
	info->VBlank     = 0;
	info->VOverPlus  = 0;
	info->VSyncWidth = 0;
	info->DotClock   = 0;
	info->UseBiosDividers = FALSE;

	if (info->DisplayType == MT_LCD && info->VBIOS &&
	    !(xf86GetOptValString(info->Options, OPTION_PANEL_SIZE))) {
	    tmp = RADEON_BIOS16(info->FPBIOSstart + 0x40);
            if (!tmp) {
		info->PanelPwrDly = 200;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                           "No Panel Info Table found in BIOS!\n");
            } else {
		char  stmp[30];
		int   tmp0;

		for (i = 0; i < 24; i++)
		    stmp[i] = RADEON_BIOS8(tmp+i+1);
		stmp[24] = 0;

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                           "Panel ID string: %s\n", stmp);

		info->PanelXRes = RADEON_BIOS16(tmp+25);
		info->PanelYRes = RADEON_BIOS16(tmp+27);
		xf86DrvMsg(0, X_INFO, "Panel Size from BIOS: %dx%d\n",
			   info->PanelXRes, info->PanelYRes);

		info->PanelPwrDly = RADEON_BIOS16(tmp+44);
		if (info->PanelPwrDly > 2000 || info->PanelPwrDly < 0)
		    info->PanelPwrDly = 2000;

		/* some panels only work well with certain divider combinations.
		 */
		info->RefDivider = RADEON_BIOS16(tmp+46);
		info->PostDivider = RADEON_BIOS8(tmp+48);
		info->FeedbackDivider = RADEON_BIOS16(tmp+49);
		if ((info->RefDivider != 0) &&
		    (info->FeedbackDivider > 3)) {
		  info->UseBiosDividers = TRUE;
		  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			     "BIOS provided dividers will be used.\n");
		}

		/* We don't use a while loop here just in case we have a corrupted BIOS image.
		   The max number of table entries is 23 at present, but may grow in future.
		   To ensure it works with future revisions we loop it to 32.
		*/
		for (i = 0; i < 32; i++) {
		    tmp0 = RADEON_BIOS16(tmp+64+i*2);
		    if (tmp0 == 0) break;
		    if ((RADEON_BIOS16(tmp0) == info->PanelXRes) &&
			(RADEON_BIOS16(tmp0+2) == info->PanelYRes)) {
			info->HBlank     = (RADEON_BIOS16(tmp0+17) -
					    RADEON_BIOS16(tmp0+19)) * 8;
			info->HOverPlus  = (RADEON_BIOS16(tmp0+21) -
					    RADEON_BIOS16(tmp0+19) - 1) * 8;
			info->HSyncWidth = RADEON_BIOS8(tmp0+23) * 8;
			info->VBlank     = (RADEON_BIOS16(tmp0+24) -
					    RADEON_BIOS16(tmp0+26));
			info->VOverPlus  = ((RADEON_BIOS16(tmp0+28) & 0x7ff) -
					    RADEON_BIOS16(tmp0+26));
			info->VSyncWidth = ((RADEON_BIOS16(tmp0+28) & 0xf800) >> 11);
			info->DotClock   = RADEON_BIOS16(tmp0+9) * 10;
			info->Flags = 0;
		    }
		}

		if (info->DotClock == 0) {
		    DisplayModePtr  tmp_mode = NULL;
		    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			       "No valid timing info from BIOS.\n");
		    /* No timing information for the native mode,
		       use whatever specified in the Modeline.
		       If no Modeline specified, we'll just pick
		       the VESA mode at 60Hz refresh rate which
		       is likely to be the best for a flat panel.
		    */
		    tmp_mode = pScrn->monitor->Modes;
		    while(tmp_mode) {
			if ((tmp_mode->HDisplay == info->PanelXRes) &&
			    (tmp_mode->VDisplay == info->PanelYRes)) {

			    float  refresh =
				(float)tmp_mode->Clock * 1000.0 / tmp_mode->HTotal / tmp_mode->VTotal;
			    if ((abs(60.0 - refresh) < 1.0) ||
				(tmp_mode->type == 0)) {
				info->HBlank     = tmp_mode->HTotal - tmp_mode->HDisplay;
				info->HOverPlus  = tmp_mode->HSyncStart - tmp_mode->HDisplay;
				info->HSyncWidth = tmp_mode->HSyncEnd - tmp_mode->HSyncStart;
				info->VBlank     = tmp_mode->VTotal - tmp_mode->VDisplay;
				info->VOverPlus  = tmp_mode->VSyncStart - tmp_mode->VDisplay;
				info->VSyncWidth = tmp_mode->VSyncEnd - tmp_mode->VSyncStart;
				info->DotClock   = tmp_mode->Clock;
				info->Flags = 0;
				break;
			    }
			    tmp_mode = tmp_mode->next;
			}
		    }
		    if ((info->DotClock == 0) && !pRADEONEnt->MonInfo1) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Panel size is not correctly detected.\n"
				   "Please try to use PanelSize option for correct settings.\n");
			return FALSE;
		    }
		}
	    }
	}
    }

    if (info->VBIOS) {
	tmp = RADEON_BIOS16(info->FPBIOSstart + 0x30);
	info->sclk = RADEON_BIOS16(tmp + 8) / 100.0;
	info->mclk = RADEON_BIOS16(tmp + 10) / 100.0;
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "No valid info for SCLK/MCLK for display bandwidth calculation.\n");
	info->sclk = 200.00;
	info->mclk = 200.00;
    }

    return TRUE;
}

static Bool RADEONProbePLLParameters(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONPLLPtr  pll  = &info->pll;
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned char ppll_div_sel;
    unsigned Nx, M;
    unsigned xclk, tmp, ref_div;
    int hTotal, vTotal, num, denom, m, n;
    float hz, vclk, xtal;
    long start_secs, start_usecs, stop_secs, stop_usecs, total_usecs;
    int i;

    for(i=0; i<1000000; i++)
	if (((INREG(RADEON_CRTC_VLINE_CRNT_VLINE) >> 16) & 0x3ff) == 0)
	    break;

    xf86getsecs(&start_secs, &start_usecs);

    for(i=0; i<1000000; i++)
	if (((INREG(RADEON_CRTC_VLINE_CRNT_VLINE) >> 16) & 0x3ff) != 0)
	    break;

    for(i=0; i<1000000; i++)
	if (((INREG(RADEON_CRTC_VLINE_CRNT_VLINE) >> 16) & 0x3ff) == 0)
	    break;

    xf86getsecs(&stop_secs, &stop_usecs);

    total_usecs = abs(stop_usecs - start_usecs);
    hz = 1000000/total_usecs;

    hTotal = ((INREG(RADEON_CRTC_H_TOTAL_DISP) & 0x1ff) + 1) * 8;
    vTotal = ((INREG(RADEON_CRTC_V_TOTAL_DISP) & 0x3ff) + 1);
    vclk = (float)(hTotal * (float)(vTotal * hz));

    switch((INPLL(pScrn, RADEON_PPLL_REF_DIV) & 0x30000) >> 16) {
    case 0:
    default:
        num = 1;
        denom = 1;
        break;
    case 1:
        n = ((INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) >> 16) & 0xff);
        m = (INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) & 0xff);
        num = 2*n;
        denom = 2*m;
        break;
    case 2:
        n = ((INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) >> 8) & 0xff);
        m = (INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) & 0xff);
        num = 2*n;
        denom = 2*m;
        break;
     }

    OUTREG(RADEON_CLOCK_CNTL_INDEX, 1);
    ppll_div_sel = INREG8(RADEON_CLOCK_CNTL_DATA + 1) & 0x3;

    n = (INPLL(pScrn, RADEON_PPLL_DIV_0 + ppll_div_sel) & 0x7ff);
    m = (INPLL(pScrn, RADEON_PPLL_REF_DIV) & 0x3ff);

    num *= n;
    denom *= m;

    switch ((INPLL(pScrn, RADEON_PPLL_DIV_0 + ppll_div_sel) >> 16) & 0x7) {
    case 1:
        denom *= 2;
        break;
    case 2:
        denom *= 4;
        break;
    case 3:
        denom *= 8;
        break;
    case 4:
        denom *= 3;
        break;
    case 6:
        denom *= 6;
        break;
    case 7:
        denom *= 12;
        break;
    }

    xtal = (int)(vclk *(float)denom/(float)num);

    if ((xtal > 26900000) && (xtal < 27100000))
        xtal = 2700;
    else if ((xtal > 14200000) && (xtal < 14400000))
        xtal = 1432;
    else if ((xtal > 29400000) && (xtal < 29600000))
        xtal = 2950;
    else
	return FALSE;

    tmp = INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV);
    ref_div = INPLL(pScrn, RADEON_PPLL_REF_DIV) & 0x3ff;

    Nx = (tmp & 0xff00) >> 8;
    M = (tmp & 0xff);
    xclk = RADEONDiv((2 * Nx * xtal), (2 * M));

    /* we're done, hopefully these are sane values */
    pll->reference_div = ref_div;
    pll->xclk = xclk;
    pll->reference_freq = xtal;

    return TRUE;
}

static void RADEONGetTMDSInfo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    CARD32 tmp;
    int i, n;

    for (i=0; i<4; i++) {
	info->tmds_pll[i].value = 0;
	info->tmds_pll[i].freq = 0;
    }

    if (info->VBIOS) {
	tmp = RADEON_BIOS16(info->FPBIOSstart + 0x34);
	if (tmp) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "DFP table revision: %d\n", RADEON_BIOS8(tmp));
	    if (RADEON_BIOS8(tmp) == 3) {
		n = RADEON_BIOS8(tmp + 5) + 1;
		if (n > 4) n = 4;
		for (i=0; i<n; i++) {
		    info->tmds_pll[i].value = RADEON_BIOS32(tmp+i*10+0x08);
		    info->tmds_pll[i].freq = RADEON_BIOS16(tmp+i*10+0x10);
		}
		return;
	    }

	    /* revision 4 has some problem as it appears in RV280,
	       comment it off for new, use default instead */
            /*
	    else if (RADEON_BIOS8(tmp) == 4) {
		int stride = 0;
		n = RADEON_BIOS8(tmp + 5) + 1;
		if (n > 4) n = 4;
		for (i=0; i<n; i++) {
		    info->tmds_pll[i].value = RADEON_BIOS32(tmp+stride+0x08);
		    info->tmds_pll[i].freq = RADEON_BIOS16(tmp+stride+0x10);
		    if (i == 0) stride += 10;
		    else stride += 6;
		}
		return;
	    }
	    */
	}
    }

    for (i=0; i<4; i++) {
	info->tmds_pll[i].value = default_tmds_pll[info->ChipFamily][i].value;
	info->tmds_pll[i].freq = default_tmds_pll[info->ChipFamily][i].freq;
    }
}

/* Read PLL parameters from BIOS block.  Default to typical values if
 * there is no BIOS.
 */
static Bool RADEONGetPLLParameters(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    RADEONPLLPtr   pll  = &info->pll;
    CARD16         bios_header;
    CARD16         pll_info_block;
    double         min_dotclock;

    if (!info->VBIOS) {

	pll->min_pll_freq   = 12500;
	pll->max_pll_freq   = 35000;


	if (!RADEONProbePLLParameters(pScrn)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Video BIOS not detected, using default PLL parameters!\n");

	    switch (info->Chipset) {
		case PCI_CHIP_R200_QL:
		case PCI_CHIP_R200_QN:
		case PCI_CHIP_R200_QO:
		case PCI_CHIP_R200_BB:
		    pll->reference_freq = 2700;
		    pll->reference_div  = 12;
		    pll->xclk           = 27500;
		    break;
		case PCI_CHIP_RV250_Id:
		case PCI_CHIP_RV250_Ie:
		case PCI_CHIP_RV250_If:
		case PCI_CHIP_RV250_Ig:
		    pll->reference_freq = 2700;
		    pll->reference_div  = 12;
		    pll->xclk           = 24975;
		    break;
		case PCI_CHIP_RV200_QW:
		    pll->reference_freq = 2700;
		    pll->reference_div  = 12;
		    pll->xclk           = 23000;
		    break;
		default:
		    pll->reference_freq = 2700;
		    pll->reference_div  = 67;
		    pll->xclk           = 16615;
		    break;
	    }
        }
    } else {
	bios_header    = RADEON_BIOS16(0x48);
	pll_info_block = RADEON_BIOS16(bios_header + 0x30);
	RADEONTRACE(("Header at 0x%04x; PLL Information at 0x%04x\n",
		     bios_header, pll_info_block));

	pll->reference_freq = RADEON_BIOS16(pll_info_block + 0x0e);
	pll->reference_div  = RADEON_BIOS16(pll_info_block + 0x10);
	pll->min_pll_freq   = RADEON_BIOS32(pll_info_block + 0x12);
	pll->max_pll_freq   = RADEON_BIOS32(pll_info_block + 0x16);
	pll->xclk           = RADEON_BIOS16(pll_info_block + 0x08);
    }

    /* (Some?) Radeon BIOSes seem too lie about their minimum dot
     * clocks.  Allow users to override the detected minimum dot clock
     * value (e.g., and allow it to be suitable for TV sets).
     */
    if (xf86GetOptValFreq(info->Options, OPTION_MIN_DOTCLOCK,
			  OPTUNITS_MHZ, &min_dotclock)) {
	if (min_dotclock < 12 || min_dotclock*100 >= pll->max_pll_freq) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Illegal minimum dotclock specified %.2f MHz "
		       "(option ignored)\n",
		       min_dotclock);
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Forced minimum dotclock to %.2f MHz "
		       "(instead of detected %.2f MHz)\n",
		       min_dotclock, ((double)pll->min_pll_freq/1000));
	    pll->min_pll_freq = min_dotclock * 1000;
	}
    }

    return TRUE;
}

/* This is called by RADEONPreInit to set up the default visual */
static Bool RADEONPreInitVisual(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb))
	return FALSE;

    switch (pScrn->depth) {
    case 8:
    case 15:
    case 16:
    case 24:
	break;

    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Given depth (%d) is not supported by %s driver\n",
		   pScrn->depth, RADEON_DRIVER_NAME);
	return FALSE;
    }

    xf86PrintDepthBpp(pScrn);

    info->fifo_slots                 = 0;
    info->pix24bpp                   = xf86GetBppFromDepth(pScrn,
							   pScrn->depth);
    info->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    info->CurrentLayout.depth        = pScrn->depth;
    info->CurrentLayout.pixel_bytes  = pScrn->bitsPerPixel / 8;
    info->CurrentLayout.pixel_code   = (pScrn->bitsPerPixel != 16
				       ? pScrn->bitsPerPixel
				       : pScrn->depth);

    if (info->pix24bpp == 24) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Radeon does NOT support 24bpp\n");
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Pixel depth = %d bits stored in %d byte%s (%d bpp pixmaps)\n",
	       pScrn->depth,
	       info->CurrentLayout.pixel_bytes,
	       info->CurrentLayout.pixel_bytes > 1 ? "s" : "",
	       info->pix24bpp);

    if (!xf86SetDefaultVisual(pScrn, -1)) return FALSE;

    if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Default visual (%s) is not supported at depth %d\n",
		   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	return FALSE;
    }
    return TRUE;
}

/* This is called by RADEONPreInit to handle all color weight issues */
static Bool RADEONPreInitWeight(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

				/* Save flag for 6 bit DAC to use for
				   setting CRTC registers.  Otherwise use
				   an 8 bit DAC, even if xf86SetWeight sets
				   pScrn->rgbBits to some value other than
				   8. */
    info->dac6bits = FALSE;

    if (pScrn->depth > 8) {
	rgb  defaultWeight = { 0, 0, 0 };

	if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight)) return FALSE;
    } else {
	pScrn->rgbBits = 8;
	if (xf86ReturnOptValBool(info->Options, OPTION_DAC_6BIT, FALSE)) {
	    pScrn->rgbBits = 6;
	    info->dac6bits = TRUE;
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d bits per RGB (%d bit DAC)\n",
	       pScrn->rgbBits, info->dac6bits ? 6 : 8);

    return TRUE;
}

static void RADEONGetVRamType(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 tmp;

    if (info->IsIGP || (info->ChipFamily >= CHIP_FAMILY_R300) ||
	(INREG(RADEON_MEM_SDRAM_MODE_REG) & (1<<30)))
	info->IsDDR = TRUE;
    else
	info->IsDDR = FALSE;

    tmp = INREG(RADEON_MEM_CNTL);
    if ((info->ChipFamily == CHIP_FAMILY_R300) ||
	(info->ChipFamily == CHIP_FAMILY_R350) ||
	(info->ChipFamily == CHIP_FAMILY_RV350)) {
	tmp &=  R300_MEM_NUM_CHANNELS_MASK;
	switch (tmp) {
	case 0: info->RamWidth = 64; break;
	case 1: info->RamWidth = 128; break;
	case 2: info->RamWidth = 256; break;
	default: info->RamWidth = 128; break;
	}
    } else if ((info->ChipFamily == CHIP_FAMILY_RV100) ||
	       (info->ChipFamily == CHIP_FAMILY_RS100) ||
	       (info->ChipFamily == CHIP_FAMILY_RS200)){
	if (tmp & RV100_HALF_MODE) info->RamWidth = 32;
	else info->RamWidth = 64;
    } else {
	if (tmp & RADEON_MEM_NUM_CHANNELS_MASK) info->RamWidth = 128;
	else info->RamWidth = 64;
    }

    /* This may not be correct, as some cards can have half of channel disabled
     * ToDo: identify these cases
     */
}

/* This is called by RADEONPreInit to handle config file overrides for
 * things like chipset and memory regions.  Also determine memory size
 * and type.  If memory type ever needs an override, put it in this
 * routine.
 */
static Bool RADEONPreInitConfig(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    EntityInfoPtr  pEnt   = info->pEnt;
    GDevPtr        dev    = pEnt->device;
    MessageType    from;
    unsigned char *RADEONMMIO = info->MMIO;
#ifdef XF86DRI
    const char    *s;
    CARD32         agpCommand;
#endif

				/* Chipset */
    from = X_PROBED;
    if (dev->chipset && *dev->chipset) {
	info->Chipset  = xf86StringToToken(RADEONChipsets, dev->chipset);
	from           = X_CONFIG;
    } else if (dev->chipID >= 0) {
	info->Chipset  = dev->chipID;
	from           = X_CONFIG;
    } else {
	info->Chipset = info->PciInfo->chipType;
    }

    pScrn->chipset = (char *)xf86TokenToString(RADEONChipsets, info->Chipset);
    if (!pScrn->chipset) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04x is not recognized\n", info->Chipset);
	return FALSE;
    }
    if (info->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognized\n", pScrn->chipset);
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Chipset: \"%s\" (ChipID = 0x%04x)\n",
	       pScrn->chipset,
	       info->Chipset);

    info->HasCRTC2 = TRUE;
    info->IsMobility = FALSE;
    info->IsIGP = FALSE;
    switch (info->Chipset) {
    case PCI_CHIP_RADEON_LY:
    case PCI_CHIP_RADEON_LZ:
	info->IsMobility = TRUE;
	info->ChipFamily = CHIP_FAMILY_RV100;
	break;

    case PCI_CHIP_RV100_QY:
    case PCI_CHIP_RV100_QZ:
	info->ChipFamily = CHIP_FAMILY_RV100;
	break;

    case PCI_CHIP_RS100_4336:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS100_4136:
	info->ChipFamily = CHIP_FAMILY_RS100;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_RS200_4337:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS200_4137:
	info->ChipFamily = CHIP_FAMILY_RS200;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_RS250_4437:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS250_4237:
	info->ChipFamily = CHIP_FAMILY_RS200;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_R200_BB:
    case PCI_CHIP_R200_BC:
    case PCI_CHIP_R200_QH:
    case PCI_CHIP_R200_QL:
    case PCI_CHIP_R200_QM:
	info->ChipFamily = CHIP_FAMILY_R200;
	break;

    case PCI_CHIP_RADEON_LW:
    case PCI_CHIP_RADEON_LX:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV200_QW: /* RV200 desktop */
    case PCI_CHIP_RV200_QX:
	info->ChipFamily = CHIP_FAMILY_RV200;
	break;

    case PCI_CHIP_RV250_Ld:
    case PCI_CHIP_RV250_Lf:
    case PCI_CHIP_RV250_Lg:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV250_If:
    case PCI_CHIP_RV250_Ig:
	info->ChipFamily = CHIP_FAMILY_RV250;
	break;

    case PCI_CHIP_RS300_5835:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS300_5834:
	info->ChipFamily = CHIP_FAMILY_RS300;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_RV280_5C61:
    case PCI_CHIP_RV280_5C63:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV280_5960:
    case PCI_CHIP_RV280_5961:
    case PCI_CHIP_RV280_5962:
    case PCI_CHIP_RV280_5964:
	info->ChipFamily = CHIP_FAMILY_RV280;
	break;

    case PCI_CHIP_R300_AD:
    case PCI_CHIP_R300_AE:
    case PCI_CHIP_R300_AF:
    case PCI_CHIP_R300_AG:
    case PCI_CHIP_R300_ND:
    case PCI_CHIP_R300_NE:
    case PCI_CHIP_R300_NF:
    case PCI_CHIP_R300_NG:
	info->ChipFamily = CHIP_FAMILY_R300;
        break;

    case PCI_CHIP_RV350_NP:
    case PCI_CHIP_RV350_NQ:
    case PCI_CHIP_RV350_NR:
    case PCI_CHIP_RV350_NS:
    case PCI_CHIP_RV350_NT:
    case PCI_CHIP_RV350_NV:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV350_AP:
    case PCI_CHIP_RV350_AQ:
    case PCI_CHIP_RV360_AR:
    case PCI_CHIP_RV350_AS:
    case PCI_CHIP_RV350_AT:
    case PCI_CHIP_RV350_AV:
	info->ChipFamily = CHIP_FAMILY_RV350;
        break;

    case PCI_CHIP_R350_AH:
    case PCI_CHIP_R350_AI:
    case PCI_CHIP_R350_AJ:
    case PCI_CHIP_R350_AK:
    case PCI_CHIP_R350_NH:
    case PCI_CHIP_R350_NI:
    case PCI_CHIP_R350_NK:
    case PCI_CHIP_R360_NJ:
	info->ChipFamily = CHIP_FAMILY_R350;
        break;

    default:
	/* Original Radeon/7200 */
	info->ChipFamily = CHIP_FAMILY_RADEON;
	info->HasCRTC2 = FALSE;
    }

				/* Framebuffer */

    from               = X_PROBED;
    info->LinearAddr   = info->PciInfo->memBase[0] & 0xfe000000;
    pScrn->memPhysBase = info->LinearAddr;
    if (dev->MemBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Linear address override, using 0x%08lx instead of 0x%08lx\n",
		   dev->MemBase,
		   info->LinearAddr);
	info->LinearAddr = dev->MemBase;
	from             = X_CONFIG;
    } else if (!info->LinearAddr) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "No valid linear framebuffer address\n");
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Linear framebuffer at 0x%08lx\n", info->LinearAddr);

				/* BIOS */
    from              = X_PROBED;
    info->BIOSAddr    = info->PciInfo->biosBase & 0xfffe0000;
    if (dev->BiosBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "BIOS address override, using 0x%08lx instead of 0x%08lx\n",
		   dev->BiosBase,
		   info->BIOSAddr);
	info->BIOSAddr = dev->BiosBase;
	from           = X_CONFIG;
    }
    if (info->BIOSAddr) {
	xf86DrvMsg(pScrn->scrnIndex, from,
		   "BIOS at 0x%08lx\n", info->BIOSAddr);
    }

				/* Read registers used to determine options */
    from                     = X_PROBED;
    if (info->FBDev)
	pScrn->videoRam      = fbdevHWGetVidmem(pScrn) / 1024;
    else if ((info->ChipFamily == CHIP_FAMILY_RS100) ||
	     (info->ChipFamily == CHIP_FAMILY_RS200) ||
	     (info->ChipFamily == CHIP_FAMILY_RS300)) {
        CARD32 tom = INREG(RADEON_NB_TOM);
        pScrn->videoRam = (((tom >> 16) -
			    (tom & 0xffff) + 1) << 6);
	OUTREG(RADEON_MC_FB_LOCATION, tom);
	OUTREG(RADEON_DISPLAY_BASE_ADDR, (tom & 0xffff) << 16);
	OUTREG(RADEON_DISPLAY2_BASE_ADDR, (tom & 0xffff) << 16);
	OUTREG(RADEON_OV0_BASE_ADDR, (tom & 0xffff) << 16);

	/* This is supposed to fix the crtc2 noise problem.
	*/
	OUTREG(RADEON_GRPH2_BUFFER_CNTL,
	       INREG(RADEON_GRPH2_BUFFER_CNTL) & ~0x7f0000);

	if ((info->ChipFamily == CHIP_FAMILY_RS100) ||
	    (info->ChipFamily == CHIP_FAMILY_RS200)) {
	    /* This is to workaround the asic bug for RMX, some versions
	       of BIOS dosen't have this register initialized correctly.
	    */
	    OUTREGP(RADEON_CRTC_MORE_CNTL, RADEON_CRTC_H_CUTOFF_ACTIVE_EN,
		    ~RADEON_CRTC_H_CUTOFF_ACTIVE_EN);
	}

    }
    else
	pScrn->videoRam      = INREG(RADEON_CONFIG_MEMSIZE) / 1024;

    /* Some production boards of m6 will return 0 if it's 8 MB */
    if (pScrn->videoRam == 0) pScrn->videoRam = 8192;

    if (info->IsSecondary) {
	/* FIXME: For now, split FB into two equal sections. This should
	 * be able to be adjusted by user with a config option. */
        RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
	RADEONInfoPtr  info1;

	pScrn->videoRam /= 2;
	pRADEONEnt->pPrimaryScrn->videoRam = pScrn->videoRam;

	info1 = RADEONPTR(pRADEONEnt->pPrimaryScrn);
	info1->FbMapSize  = pScrn->videoRam * 1024;
	info->LinearAddr += pScrn->videoRam * 1024;
	info1->Clone = FALSE;
	info1->CurCloneMode = NULL;
    }

    info->R300CGWorkaround =
	(info->ChipFamily == CHIP_FAMILY_R300 &&
	 (INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK)
	 == RADEON_CFG_ATI_REV_A11);

    info->MemCntl            = INREG(RADEON_SDRAM_MODE_REG);
    info->BusCntl            = INREG(RADEON_BUS_CNTL);

    RADEONGetVRamType(pScrn);

    if (dev->videoRam) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Video RAM override, using %d kB instead of %d kB\n",
		   dev->videoRam,
		   pScrn->videoRam);
	from             = X_CONFIG;
	pScrn->videoRam  = dev->videoRam;
    }
    pScrn->videoRam  &= ~1023;
    info->FbMapSize  = pScrn->videoRam * 1024;
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "VideoRAM: %d kByte (%d bit %s SDRAM)\n", pScrn->videoRam, info->RamWidth, info->IsDDR?"DDR":"SDR");

#ifdef XF86DRI
				/* AGP/PCI */

    /* There are signatures in BIOS and PCI-SSID for a PCI card, but
     * they are not very reliable.  Following detection method works for
     * all cards tested so far.  Note, checking AGP_ENABLE bit after
     * drmAgpEnable call can also give the correct result.  However,
     * calling drmAgpEnable on a PCI card can cause some strange lockup
     * when the server restarts next time.
     */

    agpCommand = pciReadLong(info->PciTag, RADEON_AGP_COMMAND_PCI_CONFIG);
    pciWriteLong(info->PciTag, RADEON_AGP_COMMAND_PCI_CONFIG,
		 agpCommand | RADEON_AGP_ENABLE);
    if (pciReadLong(info->PciTag, RADEON_AGP_COMMAND_PCI_CONFIG)
	& RADEON_AGP_ENABLE) {
	info->IsPCI = FALSE; 
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "AGP card detected\n");
    } else {
	info->IsPCI = TRUE; 
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "PCI card detected\n");
    }
    pciWriteLong(info->PciTag, RADEON_AGP_COMMAND_PCI_CONFIG, agpCommand);

    if ((s = xf86GetOptValString(info->Options, OPTION_BUS_TYPE))) {
	if (strcmp(s, "AGP") == 0) {
	    info->IsPCI = FALSE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into AGP mode\n");
	} else if (strcmp(s, "PCI") == 0) {
	    info->IsPCI = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into PCI mode\n");
	} else if (strcmp(s, "PCIE") == 0) {
	    info->IsPCI = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "PCI Express not supported yet, using PCI mode\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Invalid BusType option, using detected type\n");
	}
    } else if (xf86ReturnOptValBool(info->Options, OPTION_IS_PCI, FALSE)) {
	info->IsPCI = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into PCI mode\n");
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "ForcePCIMode is deprecated -- "
		   "use BusType option instead\n");
    }
#endif

    return TRUE;
}

static void RADEONI2CGetBits(I2CBusPtr b, int *Clock, int *data)
{
    ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned long  val;
    unsigned char *RADEONMMIO = info->MMIO;

    /* Get the result */
    val = INREG(info->DDCReg);

    *Clock = (val & RADEON_GPIO_Y_1) != 0;
    *data  = (val & RADEON_GPIO_Y_0) != 0;
}

static void RADEONI2CPutBits(I2CBusPtr b, int Clock, int data)
{
    ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned long  val;
    unsigned char *RADEONMMIO = info->MMIO;

    val = INREG(info->DDCReg) & (CARD32)~(RADEON_GPIO_EN_0 | RADEON_GPIO_EN_1);
    val |= (Clock ? 0:RADEON_GPIO_EN_1);
    val |= (data ? 0:RADEON_GPIO_EN_0);
    OUTREG(info->DDCReg, val);

    /* read back to improve reliability on some cards. */
    val = INREG(info->DDCReg);
}

static Bool RADEONI2cInit(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    info->pI2CBus = xf86CreateI2CBusRec();
    if (!info->pI2CBus) return FALSE;

    info->pI2CBus->BusName    = "DDC";
    info->pI2CBus->scrnIndex  = pScrn->scrnIndex;
    info->pI2CBus->I2CPutBits = RADEONI2CPutBits;
    info->pI2CBus->I2CGetBits = RADEONI2CGetBits;
    info->pI2CBus->AcknTimeout = 5;

    if (!xf86I2CBusInit(info->pI2CBus)) return FALSE;
    return TRUE;
}

static void RADEONPreInitDDC(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
 /* vbeInfoPtr     pVbe; */

    info->ddc1     = FALSE;
    info->ddc_bios = FALSE;
    if (!xf86LoadSubModule(pScrn, "ddc")) {
	info->ddc2 = FALSE;
    } else {
	xf86LoaderReqSymLists(ddcSymbols, NULL);
	info->ddc2 = TRUE;
    }

    /* DDC can use I2C bus */
    /* Load I2C if we have the code to use it */
    if (info->ddc2) {
	if (xf86LoadSubModule(pScrn, "i2c")) {
	    xf86LoaderReqSymLists(i2cSymbols,NULL);
	    info->ddc2 = RADEONI2cInit(pScrn);
	}
	else info->ddc2 = FALSE;
    }
}


/* BIOS may not have right panel size, we search through all supported
 * DDC modes looking for the maximum panel size.
 */
static void RADEONUpdatePanelSize(ScrnInfoPtr pScrn)
{
    int             j;
    RADEONInfoPtr   info = RADEONPTR (pScrn);
    xf86MonPtr      ddc  = pScrn->monitor->DDC;
    DisplayModePtr  p;

    /* Go thru detailed timing table first */
    for (j = 0; j < 4; j++) {
	if (ddc->det_mon[j].type == 0) {
	    struct detailed_timings *d_timings =
		&ddc->det_mon[j].section.d_timings;
	    if (info->PanelXRes < d_timings->h_active &&
		info->PanelYRes < d_timings->v_active) {

		info->PanelXRes  = d_timings->h_active;
		info->PanelYRes  = d_timings->v_active;
		info->DotClock   = d_timings->clock / 1000;
		info->HOverPlus  = d_timings->h_sync_off;
		info->HSyncWidth = d_timings->h_sync_width;
		info->HBlank     = d_timings->h_blanking;
		info->VOverPlus  = d_timings->v_sync_off;
		info->VSyncWidth = d_timings->v_sync_width;
		info->VBlank     = d_timings->v_blanking;
	    }
	}
    }

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
	if ((info->PanelXRes < ddc->timings2[j].hsize) &&
	    (info->PanelYRes < ddc->timings2[j].vsize)) {
	    for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
		if ((ddc->timings2[j].hsize == p->HDisplay) &&
		    (ddc->timings2[j].vsize == p->VDisplay)) {
		    float  refresh =
			(float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		    if (abs((float)ddc->timings2[j].refresh - refresh) < 1.0) {
			/* Is this good enough? */
			info->PanelXRes  = ddc->timings2[j].hsize;
			info->PanelYRes  = ddc->timings2[j].vsize;
			info->HBlank     = p->HTotal - p->HDisplay;
			info->HOverPlus  = p->HSyncStart - p->HDisplay;
			info->HSyncWidth = p->HSyncEnd - p->HSyncStart;
			info->VBlank     = p->VTotal - p->VDisplay;
			info->VOverPlus  = p->VSyncStart - p->VDisplay;
			info->VSyncWidth = p->VSyncEnd - p->VSyncStart;
			info->DotClock   = p->Clock;
			info->Flags      =
			    (ddc->det_mon[j].section.d_timings.interlaced
			     ? V_INTERLACE
			     : 0);
			if (ddc->det_mon[j].section.d_timings.sync == 3) {
			    switch (ddc->det_mon[j].section.d_timings.misc) {
			    case 0: info->Flags |= V_NHSYNC | V_NVSYNC; break;
			    case 1: info->Flags |= V_PHSYNC | V_NVSYNC; break;
			    case 2: info->Flags |= V_NHSYNC | V_PVSYNC; break;
			    case 3: info->Flags |= V_PHSYNC | V_PVSYNC; break;
			    }
			}
		    }
		}
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel size found from DDC: %dx%d\n",
	       info->PanelXRes, info->PanelYRes);
}

/* This function will sort all modes according to their resolution.
 * Highest resolution first.
 */
static void RADEONSortModes(DisplayModePtr *new, DisplayModePtr *first,
			    DisplayModePtr *last)
{
    DisplayModePtr  p;

    p = *last;
    while (p) {
	if ((((*new)->HDisplay < p->HDisplay) &&
	     ((*new)->VDisplay < p->VDisplay)) ||
	    (((*new)->HDisplay == p->HDisplay) &&
	     ((*new)->VDisplay == p->VDisplay) &&
	     ((*new)->Clock < p->Clock))) {

	    if (p->next) p->next->prev = *new;
	    (*new)->prev = p;
	    (*new)->next = p->next;
	    p->next = *new;
	    if (!((*new)->next)) *last = *new;
	    break;
	}
	if (!p->prev) {
	    (*new)->prev = NULL;
	    (*new)->next = p;
	    p->prev = *new;
	    *first = *new;
	    break;
	}
	p = p->prev;
    }

    if (!*first) {
	*first = *new;
	(*new)->prev = NULL;
	(*new)->next = NULL;
	*last = *new;
    }
}

static void RADEONSetPitch (ScrnInfoPtr pScrn)
{
    int  dummy = pScrn->virtualX;

    /* FIXME: May need to validate line pitch here */
    switch (pScrn->depth / 8) {
    case 1: dummy = (pScrn->virtualX + 127) & ~127; break;
    case 2: dummy = (pScrn->virtualX +  31) &  ~31; break;
    case 3:
    case 4: dummy = (pScrn->virtualX +  15) &  ~15; break;
    }
    pScrn->displayWidth = dummy;
}

/* When no mode provided in config file, this will add all modes supported in
 * DDC date the pScrn->modes list
 */
static DisplayModePtr RADEONDDCModes(ScrnInfoPtr pScrn)
{
    DisplayModePtr  p;
    DisplayModePtr  last  = NULL;
    DisplayModePtr  new   = NULL;
    DisplayModePtr  first = NULL;
    int             count = 0;
    int             j, tmp;
    char            stmp[32];
    xf86MonPtr      ddc   = pScrn->monitor->DDC;

    /* Go thru detailed timing table first */
    for (j = 0; j < 4; j++) {
	if (ddc->det_mon[j].type == 0) {
	    struct detailed_timings *d_timings =
		&ddc->det_mon[j].section.d_timings;

	    if (d_timings->h_active == 0 || d_timings->v_active == 0) break;

	    new = xnfcalloc(1, sizeof (DisplayModeRec));
	    memset(new, 0, sizeof (DisplayModeRec));

	    new->HDisplay   = d_timings->h_active;
	    new->VDisplay   = d_timings->v_active;

	    sprintf(stmp, "%dx%d", new->HDisplay, new->VDisplay);
	    new->name       = xnfalloc(strlen(stmp) + 1);
	    strcpy(new->name, stmp);

	    new->HTotal     = new->HDisplay + d_timings->h_blanking;
	    new->HSyncStart = new->HDisplay + d_timings->h_sync_off;
	    new->HSyncEnd   = new->HSyncStart + d_timings->h_sync_width;
	    new->VTotal     = new->VDisplay + d_timings->v_blanking;
	    new->VSyncStart = new->VDisplay + d_timings->v_sync_off;
	    new->VSyncEnd   = new->VSyncStart + d_timings->v_sync_width;
	    new->Clock      = d_timings->clock / 1000;
	    new->Flags      = (d_timings->interlaced ? V_INTERLACE : 0);
	    new->status     = MODE_OK;
	    new->type       = M_T_DEFAULT;

	    if (d_timings->sync == 3) {
		switch (d_timings->misc) {
		case 0: new->Flags |= V_NHSYNC | V_NVSYNC; break;
		case 1: new->Flags |= V_PHSYNC | V_NVSYNC; break;
		case 2: new->Flags |= V_NHSYNC | V_PVSYNC; break;
		case 3: new->Flags |= V_PHSYNC | V_PVSYNC; break;
		}
	    }
	    count++;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Valid Mode from Detailed timing table: %s\n",
		       new->name);

	    RADEONSortModes(&new, &first, &last);
	}
    }

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
	for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
	    /* Ignore all double scan modes */
	    if ((ddc->timings2[j].hsize == p->HDisplay) &&
		(ddc->timings2[j].vsize == p->VDisplay)) {
		float  refresh =
		    (float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		if (abs((float)ddc->timings2[j].refresh - refresh) < 1.0) {
		    /* Is this good enough? */
		    new = xnfcalloc(1, sizeof (DisplayModeRec));
		    memcpy(new, p, sizeof(DisplayModeRec));
		    new->name = xnfalloc(strlen(p->name) + 1);
		    strcpy(new->name, p->name);
		    new->status = MODE_OK;
		    new->type   = M_T_DEFAULT;

		    count++;

		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "Valid Mode from standard timing table: %s\n",
			       new->name);

		    RADEONSortModes(&new, &first, &last);
		    break;
		}
	    }
	}
    }

    /* Search thru established modes from EDID */
    tmp = (ddc->timings1.t1 << 8) | ddc->timings1.t2;
    for (j = 0; j < 16; j++) {
	if (tmp & (1 << j)) {
	    for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
		if ((est_timings[j].hsize == p->HDisplay) &&
		    (est_timings[j].vsize == p->VDisplay)) {
		    float  refresh =
			(float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		    if (abs((float)est_timings[j].refresh - refresh) < 1.0) {
			/* Is this good enough? */
			new = xnfcalloc(1, sizeof (DisplayModeRec));
			memcpy(new, p, sizeof(DisplayModeRec));
			new->name = xnfalloc(strlen(p->name) + 1);
			strcpy(new->name, p->name);
			new->status = MODE_OK;
			new->type   = M_T_DEFAULT;

			count++;

			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				   "Valid Mode from established timing "
				   "table: %s\n", new->name);

			RADEONSortModes(&new, &first, &last);
			break;
		    }
		}
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total of %d mode(s) found.\n", count);

    return first;
}

/* XFree86's xf86ValidateModes routine doesn't work well with DDC modes,
 * so here is our own validation routine.
 */
static int RADEONValidateDDCModes(ScrnInfoPtr pScrn, char **ppModeName,
				  RADEONMonitorType DisplayType)
{
    RADEONInfoPtr   info       = RADEONPTR(pScrn);
    DisplayModePtr  p;
    DisplayModePtr  last       = NULL;
    DisplayModePtr  first      = NULL;
    DisplayModePtr  ddcModes   = NULL;
    int             count      = 0;
    int             i, width, height;

    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    if (pScrn->monitor->DDC && !info->UseBiosDividers) {
	int  maxVirtX = pScrn->virtualX;
	int  maxVirtY = pScrn->virtualY;

	if ((DisplayType != MT_CRT) && !info->IsSecondary) {
	    /* The panel size we collected from BIOS may not be the
	     * maximum size supported by the panel.  If not, we update
	     * it now.  These will be used if no matching mode can be
	     * found from EDID data.
	     */
	    RADEONUpdatePanelSize(pScrn);
	}

	/* Collect all of the DDC modes */
	first = last = ddcModes = RADEONDDCModes(pScrn);

	for (p = ddcModes; p; p = p->next) {

	    /* If primary head is a flat panel, use RMX by default */
	    if ((!info->IsSecondary && DisplayType != MT_CRT) &&
		!info->ddc_mode) {
		/* These values are effective values after expansion.
		 * They are not really used to set CRTC registers.
		 */
		p->HTotal     = info->PanelXRes + info->HBlank;
		p->HSyncStart = info->PanelXRes + info->HOverPlus;
		p->HSyncEnd   = p->HSyncStart + info->HSyncWidth;
		p->VTotal     = info->PanelYRes + info->VBlank;
		p->VSyncStart = info->PanelYRes + info->VOverPlus;
		p->VSyncEnd   = p->VSyncStart + info->VSyncWidth;
		p->Clock      = info->DotClock;

		p->Flags     |= RADEON_USE_RMX;
	    }

	    maxVirtX = MAX(maxVirtX, p->HDisplay);
	    maxVirtY = MAX(maxVirtY, p->VDisplay);
	    count++;

	    last = p;
	}

	/* Match up modes that are specified in the XF86Config file */
	if (ppModeName[0]) {
	    DisplayModePtr  next;

	    /* Reset the max virtual dimensions */
	    maxVirtX = pScrn->virtualX;
	    maxVirtY = pScrn->virtualY;

	    /* Reset list */
	    first = last = NULL;

	    for (i = 0; ppModeName[i]; i++) {
		/* FIXME: Use HDisplay and VDisplay instead of mode string */
		if (sscanf(ppModeName[i], "%dx%d", &width, &height) == 2) {
		    for (p = ddcModes; p; p = next) {
			next = p->next;

			if (p->HDisplay == width && p->VDisplay == height) {
			    /* We found a DDC mode that matches the one
                               requested in the XF86Config file */
			    p->type |= M_T_USERDEF;

			    /* Update  the max virtual setttings */
			    maxVirtX = MAX(maxVirtX, width);
			    maxVirtY = MAX(maxVirtY, height);

			    /* Unhook from DDC modes */
			    if (p->prev) p->prev->next = p->next;
			    if (p->next) p->next->prev = p->prev;
			    if (p == ddcModes) ddcModes = p->next;

			    /* Add to used modes */
			    if (last) {
				last->next = p;
				p->prev = last;
			    } else {
				first = p;
				p->prev = NULL;
			    }
			    p->next = NULL;
			    last = p;

			    break;
			}
		    }
		}
	    }

	    /*
	     * Add remaining DDC modes if they're smaller than the user
	     * specified modes
	     */
	    for (p = ddcModes; p; p = next) {
		next = p->next;
		if (p->HDisplay <= maxVirtX && p->VDisplay <= maxVirtY) {
		    /* Unhook from DDC modes */
		    if (p->prev) p->prev->next = p->next;
		    if (p->next) p->next->prev = p->prev;
		    if (p == ddcModes) ddcModes = p->next;

		    /* Add to used modes */
		    if (last) {
			last->next = p;
			p->prev = last;
		    } else {
			first = p;
			p->prev = NULL;
		    }
		    p->next = NULL;
		    last = p;
		}
	    }

	    /* Delete unused modes */
	    while (ddcModes)
		xf86DeleteMode(&ddcModes, ddcModes);
	} else {
	    /*
	     * No modes were configured, so we make the DDC modes
	     * available for the user to cycle through.
	     */
	    for (p = ddcModes; p; p = p->next)
		p->type |= M_T_USERDEF;
	}

	pScrn->virtualX = pScrn->display->virtualX = maxVirtX;
	pScrn->virtualY = pScrn->display->virtualY = maxVirtY;
    }

    /* Close the doubly-linked mode list, if we found any usable modes */
    if (last) {
	last->next   = first;
	first->prev  = last;
	pScrn->modes = first;
	RADEONSetPitch(pScrn);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid DDC mode(s) found: %d\n", count);

    return count;
}

/* This is used only when no mode is specified for FP and no ddc is
 * available.  We force it to native mode, if possible.
 */
static DisplayModePtr RADEONFPNativeMode(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr   info  = RADEONPTR(pScrn);
    DisplayModePtr  new   = NULL;
    char            stmp[32];

    if (info->PanelXRes != 0 &&
	info->PanelYRes != 0 &&
	info->DotClock != 0) {

	/* Add native panel size */
	new             = xnfcalloc(1, sizeof (DisplayModeRec));
	sprintf(stmp, "%dx%d", info->PanelXRes, info->PanelYRes);
	new->name       = xnfalloc(strlen(stmp) + 1);
	strcpy(new->name, stmp);
	new->HDisplay   = info->PanelXRes;
	new->VDisplay   = info->PanelYRes;

	new->HTotal     = new->HDisplay + info->HBlank;
	new->HSyncStart = new->HDisplay + info->HOverPlus;
	new->HSyncEnd   = new->HSyncStart + info->HSyncWidth;
	new->VTotal     = new->VDisplay + info->VBlank;
	new->VSyncStart = new->VDisplay + info->VOverPlus;
	new->VSyncEnd   = new->VSyncStart + info->VSyncWidth;

	new->Clock      = info->DotClock;
	new->Flags      = 0;
	new->type       = M_T_USERDEF;

	new->next       = NULL;
	new->prev       = NULL;

	pScrn->display->virtualX =
	    pScrn->virtualX = MAX(pScrn->virtualX, info->PanelXRes);
	pScrn->display->virtualY =
	    pScrn->virtualY = MAX(pScrn->virtualY, info->PanelYRes);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "No valid mode specified, force to native mdoe\n");
    }

    return new;
}

/* FP mode initialization routine for using on-chip RMX to scale
 */
static int RADEONValidateFPModes(ScrnInfoPtr pScrn, char **ppModeName)
{
    RADEONInfoPtr   info       = RADEONPTR(pScrn);
    DisplayModePtr  last       = NULL;
    DisplayModePtr  new        = NULL;
    DisplayModePtr  first      = NULL;
    DisplayModePtr  p, tmp;
    int             count      = 0;
    int             i, width, height;

    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    /* We have a flat panel connected to the primary display, and we
     * don't have any DDC info.
     */
    for (i = 0; ppModeName[i] != NULL; i++) {

	if (sscanf(ppModeName[i], "%dx%d", &width, &height) != 2) continue;

	/* Note: We allow all non-standard modes as long as they do not
	 * exceed the native resolution of the panel.  Since these modes
	 * need the internal RMX unit in the video chips (and there is
	 * only one per card), this will only apply to the primary head.
	 */
	if (width < 320 || width > info->PanelXRes ||
	    height < 200 || height > info->PanelYRes) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Mode %s is out of range.\n", ppModeName[i]);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Valid modes must be between 320x200-%dx%d\n",
		       info->PanelXRes, info->PanelYRes);
	    continue;
	}

	new             = xnfcalloc(1, sizeof(DisplayModeRec));
	new->name       = xnfalloc(strlen(ppModeName[i]) + 1);
	strcpy(new->name, ppModeName[i]);
	new->HDisplay   = width;
	new->VDisplay   = height;

	/* These values are effective values after expansion They are
	 * not really used to set CRTC registers.
	 */
	new->HTotal     = info->PanelXRes + info->HBlank;
	new->HSyncStart = info->PanelXRes + info->HOverPlus;
	new->HSyncEnd   = new->HSyncStart + info->HSyncWidth;
	new->VTotal     = info->PanelYRes + info->VBlank;
	new->VSyncStart = info->PanelYRes + info->VOverPlus;
	new->VSyncEnd   = new->VSyncStart + info->VSyncWidth;
	new->Clock      = info->DotClock;
	new->Flags     |= RADEON_USE_RMX;

	new->type      |= M_T_USERDEF;

	new->next       = NULL;
	new->prev       = last;

	if (last) last->next = new;
	last = new;
	if (!first) first = new;

	pScrn->display->virtualX =
	    pScrn->virtualX = MAX(pScrn->virtualX, width);
	pScrn->display->virtualY =
	    pScrn->virtualY = MAX(pScrn->virtualY, height);
	count++;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Valid mode using on-chip RMX: %s\n", new->name);
    }

    /* If all else fails, add the native mode */
    if (!count) {
	first = last = RADEONFPNativeMode(pScrn);
	if (first) count = 1;
    }

    /* add in all default vesa modes smaller than panel size, used for randr*/
    for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
	if ((p->HDisplay <= info->PanelXRes) && (p->VDisplay <= info->PanelYRes)) {
	    tmp = first;
	    while (tmp) {
		if ((p->HDisplay == tmp->HDisplay) && (p->VDisplay == tmp->VDisplay)) break;
		tmp = tmp->next;
	    }
	    if (!tmp) {
		new             = xnfcalloc(1, sizeof(DisplayModeRec));
		new->name       = xnfalloc(strlen(p->name) + 1);
		strcpy(new->name, p->name);
		new->HDisplay   = p->HDisplay;
		new->VDisplay   = p->VDisplay;

		/* These values are effective values after expansion They are
		 * not really used to set CRTC registers.
		 */
		new->HTotal     = info->PanelXRes + info->HBlank;
		new->HSyncStart = info->PanelXRes + info->HOverPlus;
		new->HSyncEnd   = new->HSyncStart + info->HSyncWidth;
		new->VTotal     = info->PanelYRes + info->VBlank;
		new->VSyncStart = info->PanelYRes + info->VOverPlus;
		new->VSyncEnd   = new->VSyncStart + info->VSyncWidth;
		new->Clock      = info->DotClock;
		new->Flags     |= RADEON_USE_RMX;

		new->type      |= M_T_DEFAULT;

		new->next       = NULL;
		new->prev       = last;

		last->next = new;
		last = new;
	    }
	}
    }

    /* Close the doubly-linked mode list, if we found any usable modes */
    if (last) {
	last->next   = first;
	first->prev  = last;
	pScrn->modes = first;
	RADEONSetPitch(pScrn);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid FP mode(s) found: %d\n", count);

    return count;
}

/* This is called by RADEONPreInit to initialize gamma correction */
static Bool RADEONPreInitGamma(ScrnInfoPtr pScrn)
{
    Gamma  zeros = { 0.0, 0.0, 0.0 };

    if (!xf86SetGamma(pScrn, zeros)) return FALSE;
    return TRUE;
}

static void RADEONSetSyncRangeFromEdid(ScrnInfoPtr pScrn, int flag)
{
    MonPtr      mon = pScrn->monitor;
    xf86MonPtr  ddc = mon->DDC;
    int         i;

    if (flag) { /* HSync */
	for (i = 0; i < 4; i++) {
	    if (ddc->det_mon[i].type == DS_RANGES) {
		mon->nHsync = 1;
		mon->hsync[0].lo = ddc->det_mon[i].section.ranges.min_h;
		mon->hsync[0].hi = ddc->det_mon[i].section.ranges.max_h;
		return;
	    }
	}
	/* If no sync ranges detected in detailed timing table, let's
	 * try to derive them from supported VESA modes.  Are we doing
	 * too much here!!!?  */
	i = 0;
	if (ddc->timings1.t1 & 0x02) { /* 800x600@56 */
	    mon->hsync[i].lo = mon->hsync[i].hi = 35.2;
	    i++;
	}
	if (ddc->timings1.t1 & 0x04) { /* 640x480@75 */
	    mon->hsync[i].lo = mon->hsync[i].hi = 37.5;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x08) || (ddc->timings1.t1 & 0x01)) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 37.9;
	    i++;
	}
	if (ddc->timings1.t2 & 0x40) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 46.9;
	    i++;
	}
	if ((ddc->timings1.t2 & 0x80) || (ddc->timings1.t2 & 0x08)) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 48.1;
	    i++;
	}
	if (ddc->timings1.t2 & 0x04) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 56.5;
	    i++;
	}
	if (ddc->timings1.t2 & 0x02) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 60.0;
	    i++;
	}
	if (ddc->timings1.t2 & 0x01) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 64.0;
	    i++;
	}
	mon->nHsync = i;
    } else {  /* Vrefresh */
	for (i = 0; i < 4; i++) {
	    if (ddc->det_mon[i].type == DS_RANGES) {
		mon->nVrefresh = 1;
		mon->vrefresh[0].lo = ddc->det_mon[i].section.ranges.min_v;
		mon->vrefresh[0].hi = ddc->det_mon[i].section.ranges.max_v;
		return;
	    }
	}

	i = 0;
	if (ddc->timings1.t1 & 0x02) { /* 800x600@56 */
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 56;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x01) || (ddc->timings1.t2 & 0x08)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 60;
	    i++;
	}
	if (ddc->timings1.t2 & 0x04) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 70;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x08) || (ddc->timings1.t2 & 0x80)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 72;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x04) || (ddc->timings1.t2 & 0x40) ||
	    (ddc->timings1.t2 & 0x02) || (ddc->timings1.t2 & 0x01)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 75;
	    i++;
	}
	mon->nVrefresh = i;
    }
}

static int RADEONValidateCloneModes(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr   info             = RADEONPTR(pScrn);
    ClockRangePtr   clockRanges;
    DisplayModePtr  tmp_mode         = NULL;
    DisplayModePtr  clone_mode, save_mode;
    int             modesFound       = 0;
    int             count            = 0;
    int             tmp_hdisplay     = 0;
    int             tmp_vdisplay     = 0;
    int             i, save_n_hsync, save_n_vrefresh;
    range           save_hsync, save_vrefresh;
    char            *s;
    char            **clone_mode_names = NULL;
    Bool            ddc_mode         = info->ddc_mode;
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

    /* Save all infomations that will be changed by clone mode validateion */
    save_mode = pScrn->modes;
    pScrn->modes = NULL;

    /* Clone display mode names, duplicate all mode names for primary
     * head.  Allocate one more, in case pScrn->display->modes[0] ==
     * NULL */
    while (pScrn->display->modes[count]) count++;
    clone_mode_names = xnfalloc((count+2) * sizeof(char*));
    for (i = 0; i < count; i++) {
	clone_mode_names[i] = xnfalloc(strlen(pScrn->display->modes[i]) + 1);
	strcpy(clone_mode_names[i], pScrn->display->modes[i]);
    }
    clone_mode_names[count]   = NULL;
    clone_mode_names[count+1] = NULL;

    pScrn->progClock = TRUE;

    clockRanges                    = xnfcalloc(sizeof(*clockRanges), 1);
    clockRanges->next              = NULL;
    clockRanges->minClock          = info->pll.min_pll_freq;
    clockRanges->maxClock          = info->pll.max_pll_freq * 10;
    clockRanges->clockIndex        = -1;
    clockRanges->interlaceAllowed  = (info->CloneType == MT_CRT);
    clockRanges->doubleScanAllowed = (info->CloneType == MT_CRT);

    /* Only take one clone mode from config file for now, rest of clone
     * modes will copy from primary head.
     */
    if ((s = xf86GetOptValString(info->Options, OPTION_CLONE_MODE))) {
	if (sscanf(s, "%dx%d", &tmp_hdisplay, &tmp_vdisplay) == 2) {
	    if(count > 0) free(clone_mode_names[0]);
	    else count++;
	    clone_mode_names[0] = xnfalloc(strlen(s)+1);
	    sprintf(clone_mode_names[0], "%dx%d", tmp_hdisplay, tmp_vdisplay);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Clone mode %s in config file is used\n", clone_mode_names[0]);
	}
    }

    for (i = 0; i < count; i++) {
	if (sscanf(clone_mode_names[i], "%dx%d",
		   &tmp_hdisplay, &tmp_vdisplay) == 2) {
	    if (pScrn->display->virtualX < tmp_hdisplay)
		pScrn->display->virtualX = tmp_hdisplay;
	    if (pScrn->display->virtualY < tmp_vdisplay)
		pScrn->display->virtualY = tmp_vdisplay;
	}
    }

    save_hsync      = pScrn->monitor->hsync[0];
    save_vrefresh   = pScrn->monitor->vrefresh[0];
    save_n_hsync    = pScrn->monitor->nHsync;
    save_n_vrefresh = pScrn->monitor->nVrefresh;

    pScrn->monitor->DDC       = NULL;
    pScrn->monitor->nHsync    = 0;
    pScrn->monitor->nVrefresh = 0;

    if ((s = xf86GetOptValString(info->Options, OPTION_CLONE_HSYNC))) {
	if (sscanf(s, "%f-%f", &pScrn->monitor->hsync[0].lo,
		   &pScrn->monitor->hsync[0].hi) == 2) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "HSync for CloneMode from config file: %s\n", s);
	    pScrn->monitor->nHsync = 1;
	} else {
	    pScrn->monitor->nHsync = 0;
	}
    }

    if ((s = xf86GetOptValString(info->Options, OPTION_CLONE_VREFRESH))) {
	if (sscanf(s, "%f-%f", &pScrn->monitor->vrefresh[0].lo,
		   &pScrn->monitor->vrefresh[0].hi) == 2) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "VRefresh for CloneMode from config file: %s\n", s);
	    pScrn->monitor->nVrefresh = 1;
	} else {
	    pScrn->monitor->nVrefresh = 0;
	}
    }

    pScrn->monitor->DDC = pRADEONEnt->MonInfo2;
    if (pScrn->monitor->DDC) {
        if ((pScrn->monitor->nVrefresh == 0) || (pScrn->monitor->nHsync == 0)) {
            if (pScrn->monitor->nHsync == 0)
                RADEONSetSyncRangeFromEdid(pScrn, 1);
            if (pScrn->monitor->nVrefresh == 0)
                RADEONSetSyncRangeFromEdid(pScrn, 0);
        }
    } else if (info->ddc_mode) {
        ddc_mode = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "No DDC data available for clone mode, "
                   "DDCMode option is dismissed\n");
    }

    if (info->CloneType == MT_CRT && !ddc_mode) {
	modesFound =
	    xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			      clone_mode_names,
			      clockRanges,
			      NULL,                     /* linePitches */
			      8 * 64,                   /* minPitch */
			      8 * 1024,                 /* maxPitch */
			      64 * pScrn->bitsPerPixel, /* pitchInc */
			      128,                      /* minHeight */
			      2048,                     /* maxHeight */
			      pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      info->FbMapSize,
			      LOOKUP_BEST_REFRESH);
    } else {
	/* Try to add DDC modes */
	info->IsSecondary = TRUE; /*fake secondary head*/
	modesFound = RADEONValidateDDCModes(pScrn, clone_mode_names,
					    info->CloneType);
	info->IsSecondary = FALSE;

	/* If that fails and we're connect to a flat panel, then try to
         * add the flat panel modes
	 */
	if (modesFound < 1 && info->CloneType != MT_CRT) {
	    modesFound =
		xf86ValidateModes(pScrn, pScrn->monitor->Modes,
				  clone_mode_names,
				  clockRanges,
				  NULL,                     /* linePitches */
				  8 * 64,                   /* minPitch */
				  8 * 1024,                 /* maxPitch */
				  64 * pScrn->bitsPerPixel, /* pitchInc */
				  128,                      /* minHeight */
				  2048,                     /* maxHeight */
				  pScrn->display->virtualX,
				  pScrn->display->virtualY,
				  info->FbMapSize,
				  LOOKUP_BEST_REFRESH);
        }
    }

    if (modesFound > 0) {
        int valid = 0;
        save_mode = pScrn->modes;
	xf86SetCrtcForModes(pScrn, 0);
	xf86PrintModes(pScrn);
	for (i = 0; i < modesFound; i++) {

	    while (pScrn->modes->status != MODE_OK) {
		pScrn->modes = pScrn->modes->next;
	    }
	    if (!pScrn->modes) break;

	    if (pScrn->modes->Clock != 0.0) {

		clone_mode = xnfcalloc (1, sizeof (DisplayModeRec));
		if (!clone_mode) break;
		memcpy(clone_mode, pScrn->modes, sizeof(DisplayModeRec));
		clone_mode->name = xnfalloc(strlen(pScrn->modes->name) + 1);
		strcpy(clone_mode->name, pScrn->modes->name);

		if (!info->CurCloneMode) {
		    info->CloneModes = clone_mode;
		    info->CurCloneMode = clone_mode;
		    clone_mode->prev = NULL;
		} else {
		    clone_mode->prev = tmp_mode;
		    clone_mode->prev->next = clone_mode;
		}
		valid++;

		tmp_mode = clone_mode;
		clone_mode->next = NULL;
	    }
	    pScrn->modes = pScrn->modes->next;
	}

	/* no longer needed, free it */
	pScrn->modes = save_mode;
	while (pScrn->modes)
	  xf86DeleteMode(&pScrn->modes, pScrn->modes);
	pScrn->modes = NULL;

	/* modepool is no longer needed, free it */
	while (pScrn->modePool)
	  xf86DeleteMode(&pScrn->modePool, pScrn->modePool);
	pScrn->modePool = NULL;

	modesFound = valid;
    }

    /* Clone_mode_names list is no longer needed, free it. */
    if (clone_mode_names) {
	for (i = 0; clone_mode_names[i]; i++) {
	    free(clone_mode_names[i]);
	    clone_mode_names[i] = NULL;
	}

	free(clone_mode_names);
	clone_mode_names = NULL;
    }

    /* We need to restore all changed info for the primary head */

    pScrn->monitor->hsync[0]    = save_hsync;
    pScrn->monitor->vrefresh[0] = save_vrefresh;
    pScrn->monitor->nHsync      = save_n_hsync;
    pScrn->monitor->nVrefresh   = save_n_vrefresh;

    /*
     * Also delete the clockRanges (if it was setup) since it will be
     * set up during the primary head initialization.
     */
    while (pScrn->clockRanges) {
	ClockRangesPtr CRtmp = pScrn->clockRanges;
	pScrn->clockRanges = pScrn->clockRanges->next;
	xfree(CRtmp);
    }


    return modesFound;
}

/* This is called by RADEONPreInit to validate modes and compute
 * parameters for all of the valid modes.
 */
static Bool RADEONPreInitModes(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    ClockRangePtr  clockRanges;
    int            modesFound;
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    char           *s;

    /* This option has two purposes:
     *
     * 1. For CRT, if this option is on, xf86ValidateModes (to
     *    LOOKUP_BEST_REFRESH) is not going to be used for mode
     *    validation.  Instead, we'll validate modes by matching exactly
     *    the modes supported from the DDC data.  This option can be
     *    used (a) to enable non-standard modes listed in the Detailed
     *    Timings block of EDID, like 2048x1536 (not included in
     *    xf86DefModes), (b) to avoid unstable modes for some flat
     *    panels working in analog mode (some modes validated by
     *    xf86ValidateModes don't really work with these panels).
     *
     * 2. For DFP on primary head, with this option on, the validation
     *    routine will try to use supported modes from DDC data first
     *    before trying on-chip RMX streching.  By default, native mode
     *    + RMX streching is used for all non-native modes, it appears
     *    more reliable. Some non-native modes listed in the DDC data
     *    may not work properly if they are used directly. This seems to
     *    only happen to a few panels (haven't nailed this down yet, it
     *    may related to the incorrect setting in TMDS_PLL_CNTL when
     *    pixel clock is changed).  Use this option may give you better
     *    refresh rate for some non-native modes.  The 2nd DVI port will
     *    always use DDC modes directly (only have one on-chip RMX
     *    unit).
     *
     * Note: This option will be dismissed if no DDC data is available.
     */
    info->ddc_mode =
	xf86ReturnOptValBool(info->Options, OPTION_DDC_MODE, FALSE);

    /* don't use RMX if we have a dual-tdms panels */
   if (pRADEONEnt->MonType2 == MT_DFP)
	info->ddc_mode = TRUE;

    /* Here is a hack for cloning first display on the second head.  If
     * we don't do this, when both heads are connected, the same CRTC
     * will be used to drive them according to the capability of the
     * primary head.  This can cause an unstable or blank screen, or
     * even worse it can damage a monitor.  This feature is also
     * important for laptops (using M6, M7), where the panel can't be
     * disconnect when one wants to use the CRT port.  Although 2
     * Screens can be set up in the config file for displaying same
     * content on two monitors, it has problems with cursor, overlay,
     * DRI.
     */
    if (info->HasCRTC2) {
	if (info->Clone) {

	    /* If we have 2 screens from the config file, we don't need
	     * to do clone thing, let each screen handles one head.
	     */
	    if (!pRADEONEnt->HasSecondary) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Clone modes validation ------------ \n");

		modesFound = RADEONValidateCloneModes(pScrn);
		if (modesFound < 1) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "No valid mode found for CRTC2 clone\n");
		    info->Clone = FALSE;
		    info->CurCloneMode = NULL;
		}
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Total of %d clone modes found ------------ \n\n",
			   modesFound);
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Validating modes on %s head ---------\n",
	       info->IsSecondary ? "Secondary" : "Primary");

    if (info->IsSecondary)
        pScrn->monitor->DDC = pRADEONEnt->MonInfo2;
    else
        pScrn->monitor->DDC = pRADEONEnt->MonInfo1;

    if (!pScrn->monitor->DDC && info->ddc_mode) {
	info->ddc_mode = FALSE;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "No DDC data available, DDCMode option is dismissed\n");
    }

    if ((info->DisplayType == MT_DFP) ||
	(info->DisplayType == MT_LCD)) {
	if ((s = xf86GetOptValString(info->Options, OPTION_PANEL_SIZE))) {
	    int PanelX, PanelY;
	    DisplayModePtr  tmp_mode         = NULL;
	    if (sscanf(s, "%dx%d", &PanelX, &PanelY) == 2) {
		info->PanelXRes = PanelX;
		info->PanelYRes = PanelY;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "Panel size is forced to: %s\n", s);

		/* We can't trust BIOS or DDC timings anymore,
		   Use whatever specified in the Modeline.
		   If no Modeline specified, we'll just pick the VESA mode at
		   60Hz refresh rate which is likely to be the best for a flat panel.
		*/
		info->ddc_mode = FALSE;
		pScrn->monitor->DDC = NULL;
		tmp_mode = pScrn->monitor->Modes;
		while(tmp_mode) {
		    if ((tmp_mode->HDisplay == PanelX) &&
			(tmp_mode->VDisplay == PanelY)) {

			float  refresh =
			    (float)tmp_mode->Clock * 1000.0 / tmp_mode->HTotal / tmp_mode->VTotal;
			if ((abs(60.0 - refresh) < 1.0) ||
			    (tmp_mode->type == 0)) {
			    info->HBlank     = tmp_mode->HTotal - tmp_mode->HDisplay;
			    info->HOverPlus  = tmp_mode->HSyncStart - tmp_mode->HDisplay;
			    info->HSyncWidth = tmp_mode->HSyncEnd - tmp_mode->HSyncStart;
			    info->VBlank     = tmp_mode->VTotal - tmp_mode->VDisplay;
			    info->VOverPlus  = tmp_mode->VSyncStart - tmp_mode->VDisplay;
			    info->VSyncWidth = tmp_mode->VSyncEnd - tmp_mode->VSyncStart;
			    info->DotClock   = tmp_mode->Clock;
			    info->Flags = 0;
			    break;
			}
		    }
		    tmp_mode = tmp_mode->next;
		}
		if (info->DotClock == 0) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "No valid timing info for specified panel size.\n"
			       "Please specify the Modeline for this panel\n");
		    return FALSE;
		}
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "Invalid PanelSize value: %s\n", s);
	    }
	}
    }

    if (pScrn->monitor->DDC) {
        /* If we still don't know sync range yet, let's try EDID.
         *
         * Note that, since we can have dual heads, Xconfigurator
         * may not be able to probe both monitors correctly through
         * vbe probe function (RADEONProbeDDC). Here we provide an
         * additional way to auto-detect sync ranges if they haven't
         * been added to XF86Config manually.
         */
        if (pScrn->monitor->nHsync <= 0)
            RADEONSetSyncRangeFromEdid(pScrn, 1);
        if (pScrn->monitor->nVrefresh <= 0)
            RADEONSetSyncRangeFromEdid(pScrn, 0);
    }

    /* Get mode information */
    pScrn->progClock               = TRUE;
    clockRanges                    = xnfcalloc(sizeof(*clockRanges), 1);
    clockRanges->next              = NULL;
    clockRanges->minClock          = info->pll.min_pll_freq;
    clockRanges->maxClock          = info->pll.max_pll_freq * 10;
    clockRanges->clockIndex        = -1;
    clockRanges->interlaceAllowed  = (info->DisplayType == MT_CRT);
    clockRanges->doubleScanAllowed = (info->DisplayType == MT_CRT);

    /* We'll use our own mode validation routine for DFP/LCD, since
     * xf86ValidateModes does not work correctly with the DFP/LCD modes
     * 'stretched' from their native mode.
     */
    if (info->DisplayType == MT_CRT && !info->ddc_mode) {

	modesFound =
	    xf86ValidateModes(pScrn,
			      pScrn->monitor->Modes,
			      pScrn->display->modes,
			      clockRanges,
			      NULL,                  /* linePitches */
			      8 * 64,                /* minPitch */
			      8 * 1024,              /* maxPitch */
			      64 * pScrn->bitsPerPixel, /* pitchInc */
			      128,                   /* minHeight */
			      2048,                  /* maxHeight */
			      pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      info->FbMapSize,
			      LOOKUP_BEST_REFRESH);

	if (modesFound < 1 && info->FBDev) {
	    fbdevHWUseBuildinMode(pScrn);
	    pScrn->displayWidth = pScrn->virtualX; /* FIXME: might be wrong */
	    modesFound = 1;
	}

	if (modesFound == -1) return FALSE;

	xf86PruneDriverModes(pScrn);
	if (!modesFound || !pScrn->modes) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	    return FALSE;
	}

    } else {
	/* First, free any allocated modes during configuration, since
	 * we don't need them
	 */
	while (pScrn->modes)
	    xf86DeleteMode(&pScrn->modes, pScrn->modes);
	while (pScrn->modePool)
	    xf86DeleteMode(&pScrn->modePool, pScrn->modePool);

	/* Next try to add DDC modes */
	modesFound = RADEONValidateDDCModes(pScrn, pScrn->display->modes,
					    info->DisplayType);

	/* If that fails and we're connect to a flat panel, then try to
         * add the flat panel modes
	 */
	if (info->DisplayType != MT_CRT) {

	    /* some panels have DDC, but don't have internal scaler.
	     * in this case, we need to validate additional modes
	     * by using on-chip RMX.
	     */
	    int user_modes_asked = 0, user_modes_found = 0, i;
	    DisplayModePtr  tmp_mode = pScrn->modes;
	    while (pScrn->display->modes[user_modes_asked]) user_modes_asked++;
	    if (tmp_mode) {
		for (i = 0; i < modesFound; i++) {
		    if (tmp_mode->type & M_T_USERDEF) user_modes_found++;
		    tmp_mode = tmp_mode->next;
		}
	    }

	    if ((modesFound <= 1) || (user_modes_found < user_modes_asked)) {
		/* when panel size is not valid, try to validate
		 * mode using xf86ValidateModes routine
		 * This can happen when DDC is disabled.
		 */
		if (info->PanelXRes < 320 || info->PanelYRes < 200)
		    modesFound =
			xf86ValidateModes(pScrn,
					  pScrn->monitor->Modes,
					  pScrn->display->modes,
					  clockRanges,
					  NULL,                  /* linePitches */
					  8 * 64,                /* minPitch */
					  8 * 1024,              /* maxPitch */
					  64 * pScrn->bitsPerPixel, /* pitchInc */
					  128,                   /* minHeight */
					  2048,                  /* maxHeight */
					  pScrn->display->virtualX,
					  pScrn->display->virtualY,
					  info->FbMapSize,
					  LOOKUP_BEST_REFRESH);
		else if (!info->IsSecondary)
		    modesFound = RADEONValidateFPModes(pScrn, pScrn->display->modes);
	    }
        }

	/* Setup the screen's clockRanges for the VidMode extension */
	if (!pScrn->clockRanges) {
	    pScrn->clockRanges = xnfcalloc(sizeof(*(pScrn->clockRanges)), 1);
	    memcpy(pScrn->clockRanges, clockRanges, sizeof(*clockRanges));
	    pScrn->clockRanges->strategy = LOOKUP_BEST_REFRESH;
	}

	/* Fail if we still don't have any valid modes */
	if (modesFound < 1) {
	    if (info->DisplayType == MT_CRT) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No valid DDC modes found for this CRT\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Try turning off the \"DDCMode\" option\n");
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No valid mode found for this DFP/LCD\n");
	    }
	    return FALSE;
	}
    }

    xf86SetCrtcForModes(pScrn, 0);

    /* We need to adjust virtual size if the clone modes have larger
     * display size.
     */
    if (info->Clone && info->CloneModes) {
	DisplayModePtr  clone_mode = info->CloneModes;
	while (1) {
	    if ((clone_mode->HDisplay > pScrn->virtualX) ||
		(clone_mode->VDisplay > pScrn->virtualY)) {
		pScrn->virtualX =
		    pScrn->display->virtualX = clone_mode->HDisplay;
		pScrn->virtualY =
		    pScrn->display->virtualY = clone_mode->VDisplay;
		RADEONSetPitch(pScrn);
	    }
	    if (!clone_mode->next) break;
	    clone_mode = clone_mode->next;
	}
    }

    pScrn->currentMode = pScrn->modes;
    xf86PrintModes(pScrn);

				/* Set DPI */
    xf86SetDpi(pScrn, 0, 0);

				/* Get ScreenInit function */
    if (!xf86LoadSubModule(pScrn, "fb")) return FALSE;

    xf86LoaderReqSymLists(fbSymbols, NULL);

    info->CurrentLayout.displayWidth = pScrn->displayWidth;
    info->CurrentLayout.mode = pScrn->currentMode;

    return TRUE;
}

/* This is called by RADEONPreInit to initialize the hardware cursor */
static Bool RADEONPreInitCursor(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86ReturnOptValBool(info->Options, OPTION_SW_CURSOR, FALSE)) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) return FALSE;
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }
    return TRUE;
}

/* This is called by RADEONPreInit to initialize hardware acceleration */
static Bool RADEONPreInitAccel(ScrnInfoPtr pScrn)
{
#ifdef XFree86LOADER
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	int errmaj = 0, errmin = 0;

	info->xaaReq.majorversion = 1;
	info->xaaReq.minorversion = 1;

	if (!LoadSubModule(pScrn->module, "xaa", NULL, NULL, NULL,
			   &info->xaaReq, &errmaj, &errmin)) {
	    info->xaaReq.minorversion = 0;

	    if (!LoadSubModule(pScrn->module, "xaa", NULL, NULL, NULL,
			       &info->xaaReq, &errmaj, &errmin)) {
		LoaderErrorMsg(NULL, "xaa", errmaj, errmin);
		return FALSE;
	    }
	}
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }
#endif

    return TRUE;
}

static Bool RADEONPreInitInt10(ScrnInfoPtr pScrn, xf86Int10InfoPtr *ppInt10)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

#if !defined(__powerpc__)
    if (xf86LoadSubModule(pScrn, "int10")) {
	xf86LoaderReqSymLists(int10Symbols, NULL);
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,"initializing int10\n");
	*ppInt10 = xf86InitInt10(info->pEnt->index);
    }
#endif
    return TRUE;
}

#ifdef XF86DRI
static Bool RADEONPreInitDRI(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (xf86ReturnOptValBool(info->Options, OPTION_CP_PIO, FALSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forcing CP into PIO mode\n");
	info->CPMode = RADEON_DEFAULT_CP_PIO_MODE;
    } else {
	info->CPMode = RADEON_DEFAULT_CP_BM_MODE;
    }

    info->agpMode       = RADEON_DEFAULT_AGP_MODE;
    info->gartSize      = RADEON_DEFAULT_GART_SIZE;
    info->ringSize      = RADEON_DEFAULT_RING_SIZE;
    info->bufSize       = RADEON_DEFAULT_BUFFER_SIZE;
    info->gartTexSize   = RADEON_DEFAULT_GART_TEX_SIZE;
    info->agpFastWrite  = RADEON_DEFAULT_AGP_FAST_WRITE;

    info->CPusecTimeout = RADEON_DEFAULT_CP_TIMEOUT;

    if (!info->IsPCI) {
	if (xf86GetOptValInteger(info->Options,
				 OPTION_AGP_MODE, &(info->agpMode))) {
	    if (info->agpMode < 1 || info->agpMode > RADEON_AGP_MAX_MODE) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Illegal AGP Mode: %d\n", info->agpMode);
		return FALSE;
	    }
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Using AGP %dx mode\n", info->agpMode);
	}

	if ((info->agpFastWrite = xf86ReturnOptValBool(info->Options,
						       OPTION_AGP_FW,
						       FALSE))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Enabling AGP Fast Write\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "AGP Fast Write disabled by default\n");
	}
    }

    if (xf86GetOptValInteger(info->Options,
			     OPTION_GART_SIZE, (int *)&(info->gartSize))) {
	switch (info->gartSize) {
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
	case 256:
	    break;

	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal GART size: %d MB\n", info->gartSize);
	    return FALSE;
	}
    }

    if (xf86GetOptValInteger(info->Options,
			     OPTION_RING_SIZE, &(info->ringSize))) {
	if (info->ringSize < 1 || info->ringSize >= (int)info->gartSize) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal ring buffer size: %d MB\n",
		       info->ringSize);
	    return FALSE;
	}
    }

    if (xf86GetOptValInteger(info->Options,
			     OPTION_BUFFER_SIZE, &(info->bufSize))) {
	if (info->bufSize < 1 || info->bufSize >= (int)info->gartSize) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal vertex/indirect buffers size: %d MB\n",
		       info->bufSize);
	    return FALSE;
	}
	if (info->bufSize > 2) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal vertex/indirect buffers size: %d MB\n",
		       info->bufSize);
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Clamping vertex/indirect buffers size to 2 MB\n");
	    info->bufSize = 2;
	}
    }

    if (info->ringSize + info->bufSize + info->gartTexSize >
	(int)info->gartSize) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Buffers are too big for requested GART space\n");
	return FALSE;
    }

    info->gartTexSize = info->gartSize - (info->ringSize + info->bufSize);

    if (xf86GetOptValInteger(info->Options, OPTION_USEC_TIMEOUT,
			     &(info->CPusecTimeout))) {
	/* This option checked by the RADEON DRM kernel module */
    }

    /* Depth moves are disabled by default since they are extremely slow */
    if ((info->depthMoves = xf86ReturnOptValBool(info->Options,
						 OPTION_DEPTH_MOVE, FALSE))) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Enabling depth moves\n");
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Depth moves disabled by default\n");
    }

    /* Two options to try and squeeze as much texture memory as possible
     * for dedicated 3d rendering boxes
     */
    info->noBackBuffer = xf86ReturnOptValBool(info->Options,
					      OPTION_NO_BACKBUFFER,
					      FALSE);

    if (info->noBackBuffer) {
	info->allowPageFlip = 0;
    } else if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	info->allowPageFlip = 0;
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Couldn't load shadowfb module:\n");
    } else {
	xf86LoaderReqSymLists(driShadowFBSymbols, NULL);

	info->allowPageFlip = xf86ReturnOptValBool(info->Options,
						   OPTION_PAGE_FLIP,
						   FALSE);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Page flipping %sabled\n",
	       info->allowPageFlip ? "en" : "dis");

    return TRUE;
}
#endif

static void
RADEONProbeDDC(ScrnInfoPtr pScrn, int indx)
{
    vbeInfoPtr  pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
	pVbe = VBEInit(NULL,indx);
	ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
    }
}

/* RADEONPreInit is called once at server startup */
Bool RADEONPreInit(ScrnInfoPtr pScrn, int flags)
{
    RADEONInfoPtr     info;
    xf86Int10InfoPtr  pInt10 = NULL;
    void *int10_save = NULL;
    const char *s;

    RADEONTRACE(("RADEONPreInit\n"));
    if (pScrn->numEntities != 1) return FALSE;

    if (!RADEONGetRec(pScrn)) return FALSE;

    info               = RADEONPTR(pScrn);
    info->IsSecondary  = FALSE;
    info->Clone        = FALSE;
    info->CurCloneMode = NULL;
    info->CloneModes   = NULL;
    info->IsSwitching  = FALSE;
    info->MMIO         = NULL;

    info->pEnt         = xf86GetEntityInfo(pScrn->entityList[pScrn->numEntities - 1]);
    if (info->pEnt->location.type != BUS_PCI) goto fail;

    info->PciInfo = xf86GetPciInfoForEntity(info->pEnt->index);
    info->PciTag  = pciTag(info->PciInfo->bus,
			   info->PciInfo->device,
			   info->PciInfo->func);
    info->MMIOAddr   = info->PciInfo->memBase[2] & 0xffffff00;
    if (info->pEnt->device->IOBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "MMIO address override, using 0x%08lx instead of 0x%08lx\n",
		   info->pEnt->device->IOBase,
		   info->MMIOAddr);
	info->MMIOAddr = info->pEnt->device->IOBase;
    } else if (!info->MMIOAddr) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid MMIO address\n");
	goto fail1;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "MMIO registers at 0x%08lx\n", info->MMIOAddr);

    if(!RADEONMapMMIO(pScrn)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Memory map the MMIO region failed\n");
	goto fail1;
    }

#if !defined(__alpha__)
    if (xf86GetPciDomain(info->PciTag) ||
	!xf86IsPrimaryPci(info->PciInfo))
	RADEONPreInt10Save(pScrn, &int10_save);
#else
    /* [Alpha] On the primary, the console already ran the BIOS and we're
     *         going to run it again - so make sure to "fix up" the card
     *         so that (1) we can read the BIOS ROM and (2) the BIOS will
     *         get the memory config right.
     */
    RADEONPreInt10Save(pScrn, &int10_save);
#endif

    if (xf86IsEntityShared(info->pEnt->index)) {
	if (xf86IsPrimInitDone(info->pEnt->index)) {

	    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

	    info->IsSecondary = TRUE;
	    if (!pRADEONEnt->HasSecondary) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Only one monitor detected, Second screen "
			   "will NOT be created\n");
		goto fail2;
	    }
	    pRADEONEnt->pSecondaryScrn = pScrn;
	} else {
	    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

	    xf86SetPrimInitDone(info->pEnt->index);

	    pRADEONEnt->pPrimaryScrn        = pScrn;
	    pRADEONEnt->RestorePrimary      = FALSE;
	    pRADEONEnt->IsSecondaryRestored = FALSE;
	}
    }

    if (flags & PROBE_DETECT) {
	RADEONProbeDDC(pScrn, info->pEnt->index);
	RADEONPostInt10Check(pScrn, int10_save);
	if(info->MMIO) RADEONUnmapMMIO(pScrn);
	return TRUE;
    }

    if (!xf86LoadSubModule(pScrn, "vgahw")) return FALSE;
    xf86LoaderReqSymLists(vgahwSymbols, NULL);
    if (!vgaHWGetHWRec(pScrn)) {
	RADEONFreeRec(pScrn);
	goto fail2;
    }

    vgaHWGetIOBase(VGAHWPTR(pScrn));

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "PCI bus %d card %d func %d\n",
	       info->PciInfo->bus,
	       info->PciInfo->device,
	       info->PciInfo->func);

    if (xf86RegisterResources(info->pEnt->index, 0, ResExclusive))
	goto fail;

    if (xf86SetOperatingState(resVga, info->pEnt->index, ResUnusedOpr))
	goto fail;

    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_VIEWPORT | RAC_CURSOR;
    pScrn->monitor     = pScrn->confScreen->monitor;

    if (!RADEONPreInitVisual(pScrn))
	goto fail;

				/* We can't do this until we have a
				   pScrn->display. */
    xf86CollectOptions(pScrn, NULL);
    if (!(info->Options = xalloc(sizeof(RADEONOptions))))
	goto fail;

    memcpy(info->Options, RADEONOptions, sizeof(RADEONOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, info->Options);

    if (!RADEONPreInitWeight(pScrn))
	goto fail;

    if (xf86GetOptValInteger(info->Options, OPTION_VIDEO_KEY,
			     &(info->videoKey))) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
		   info->videoKey);
    } else {
	info->videoKey = 0x1E;
    }

    info->DispPriority = 1;
    if ((s = xf86GetOptValString(info->Options, OPTION_DISP_PRIORITY))) {
	if (strcmp(s, "AUTO") == 0) {
	    info->DispPriority = 1;
	} else if (strcmp(s, "BIOS") == 0) {
	    info->DispPriority = 0;
	} else if (strcmp(s, "HIGH") == 0) {
	    info->DispPriority = 2;
	} else
	    info->DispPriority = 1;
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_FBDEV, FALSE)) {
	/* check for Linux framebuffer device */

	if (xf86LoadSubModule(pScrn, "fbdevhw")) {
	    xf86LoaderReqSymLists(fbdevHWSymbols, NULL);

	    if (fbdevHWInit(pScrn, info->PciInfo, NULL)) {
		pScrn->ValidMode     = fbdevHWValidMode;
		info->FBDev = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "Using framebuffer device\n");
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "fbdevHWInit failed, not using framebuffer device\n");
	    }
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Couldn't load fbdevhw module, not using framebuffer device\n");
	}
    }

    if (!info->FBDev)
	if (!RADEONPreInitInt10(pScrn, &pInt10))
	    goto fail;

    RADEONPostInt10Check(pScrn, int10_save);

    if (!RADEONPreInitConfig(pScrn))
	goto fail;

    RADEONPreInitDDC(pScrn);

    if (!RADEONGetBIOSParameters(pScrn, pInt10))
	goto fail;

    if (info->DisplayType == MT_DFP)
	RADEONGetTMDSInfo(pScrn);

    if (!RADEONGetPLLParameters(pScrn))          goto fail;

    if (!RADEONPreInitGamma(pScrn))              goto fail;

    if (!RADEONPreInitModes(pScrn, pInt10))      goto fail;

    if (!RADEONPreInitCursor(pScrn))             goto fail;

    if (!RADEONPreInitAccel(pScrn))              goto fail;

#ifdef XF86DRI
    if (!RADEONPreInitDRI(pScrn))                goto fail;
#endif

				/* Free the video bios (if applicable) */
    if (info->VBIOS) {
	xfree(info->VBIOS);
	info->VBIOS = NULL;
    }

				/* Free int10 info */
    if (pInt10)
	xf86FreeInt10(pInt10);

    if(info->MMIO) RADEONUnmapMMIO(pScrn);
    info->MMIO = NULL;

    xf86DrvMsg(pScrn->scrnIndex, X_NOTICE,
	       "For information on using the multimedia capabilities\n of this"
	       " adapter, please see http://gatos.sf.net.\n");

    return TRUE;

fail:
				/* Pre-init failed. */
    if (info->IsSecondary) {
        RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
	pRADEONEnt->HasSecondary = FALSE;
    }
				/* Free the video bios (if applicable) */
    if (info->VBIOS) {
	xfree(info->VBIOS);
	info->VBIOS = NULL;
    }

				/* Free int10 info */
    if (pInt10)
	xf86FreeInt10(pInt10);

    vgaHWFreeHWRec(pScrn);

 fail2:
    if(info->MMIO) RADEONUnmapMMIO(pScrn);
    info->MMIO = NULL;
 fail1:
    RADEONFreeRec(pScrn);

    return FALSE;
}

/* Load a palette */
static void RADEONLoadPalette(ScrnInfoPtr pScrn, int numColors,
			      int *indices, LOCO *colors, VisualPtr pVisual)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i;
    int            idx, j;
    unsigned char  r, g, b;

#ifdef XF86DRI
    if (info->CPStarted) DRILock(pScrn->pScreen, 0);
#endif

    if (info->accelOn) info->accel->Sync(pScrn);

    if (info->FBDev) {
	fbdevHWLoadPalette(pScrn, numColors, indices, colors, pVisual);
    } else {
	/* If the second monitor is connected, we also need to deal with
	 * the secondary palette
	 */
	if (info->IsSecondary) j = 1;
	else j = 0;

	PAL_SELECT(j);

	if (info->CurrentLayout.depth == 15) {
	    /* 15bpp mode.  This sends 32 values. */
	    for (i = 0; i < numColors; i++) {
		idx = indices[i];
		r   = colors[idx].red;
		g   = colors[idx].green;
		b   = colors[idx].blue;
		OUTPAL(idx * 8, r, g, b);
	    }
	} else if (info->CurrentLayout.depth == 16) {
	    /* 16bpp mode.  This sends 64 values.
	     *
	     * There are twice as many green values as there are values
	     * for red and blue.  So, we take each red and blue pair,
	     * and combine it with each of the two green values.
	     */
	    for (i = 0; i < numColors; i++) {
		idx = indices[i];
		r   = colors[idx / 2].red;
		g   = colors[idx].green;
		b   = colors[idx / 2].blue;
		RADEONWaitForFifo(pScrn, 32); /* delay */
		OUTPAL(idx * 4, r, g, b);

		/* AH - Added to write extra green data - How come this isn't
		 * needed on R128?  We didn't load the extra green data in the
		 * other routine
		 */
		if (idx <= 31) {
		    r   = colors[idx].red;
		    g   = colors[(idx * 2) + 1].green;
		    b   = colors[idx].blue;
		    RADEONWaitForFifo(pScrn, 32); /* delay */
		    OUTPAL(idx * 8, r, g, b);
		}
	    }
	} else {
	    /* 8bpp mode.  This sends 256 values. */
	    for (i = 0; i < numColors; i++) {
		idx = indices[i];
		r   = colors[idx].red;
		b   = colors[idx].blue;
		g   = colors[idx].green;
		RADEONWaitForFifo(pScrn, 32); /* delay */
		OUTPAL(idx, r, g, b);
	    }
	}

	if (info->Clone) {
	    PAL_SELECT(1);
	    if (info->CurrentLayout.depth == 15) {
		/* 15bpp mode.  This sends 32 values. */
		for (i = 0; i < numColors; i++) {
		    idx = indices[i];
		    r   = colors[idx].red;
		    g   = colors[idx].green;
		    b   = colors[idx].blue;
		    OUTPAL(idx * 8, r, g, b);
		}
	    } else if (info->CurrentLayout.depth == 16) {
		/* 16bpp mode.  This sends 64 values.
		 *
		 * There are twice as many green values as there are values
		 * for red and blue.  So, we take each red and blue pair,
		 * and combine it with each of the two green values.
		 */
		for (i = 0; i < numColors; i++) {
		    idx = indices[i];
		    r   = colors[idx / 2].red;
		    g   = colors[idx].green;
		    b   = colors[idx / 2].blue;
		    OUTPAL(idx * 4, r, g, b);

		    /* AH - Added to write extra green data - How come
		     * this isn't needed on R128?  We didn't load the
		     * extra green data in the other routine.
		     */
		    if (idx <= 31) {
			r   = colors[idx].red;
			g   = colors[(idx * 2) + 1].green;
			b   = colors[idx].blue;
			OUTPAL(idx * 8, r, g, b);
		    }
		}
	    } else {
		/* 8bpp mode.  This sends 256 values. */
		for (i = 0; i < numColors; i++) {
		    idx = indices[i];
		    r   = colors[idx].red;
		    b   = colors[idx].blue;
		    g   = colors[idx].green;
		    OUTPAL(idx, r, g, b);
		}
	    }
	}
    }

#ifdef XF86DRI
    if (info->CPStarted) DRIUnlock(pScrn->pScreen);
#endif
}

static void RADEONBlockHandler(int i, pointer blockData,
			       pointer pTimeout, pointer pReadmask)
{
    ScreenPtr      pScreen = screenInfo.screens[i];
    ScrnInfoPtr    pScrn   = xf86Screens[i];
    RADEONInfoPtr  info    = RADEONPTR(pScrn);

#ifdef XF86DRI
    if (info->directRenderingEnabled)
	FLUSH_RING();
#endif

    pScreen->BlockHandler = info->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = RADEONBlockHandler;

    if (info->VideoTimerCallback)
	(*info->VideoTimerCallback)(pScrn, currentTime.milliseconds);
}

/* Called at the start of each server generation. */
Bool RADEONScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    BoxRec         MemBox;
    int            y2;

    RADEONTRACE(("RADEONScreenInit %x %d\n",
		 pScrn->memPhysBase, pScrn->fbOffset));

#ifdef XF86DRI
				/* Turn off the CP for now. */
    info->CPInUse      = FALSE;
    info->CPStarted    = FALSE;
    info->directRenderingEnabled = FALSE;
#endif
    info->accelOn      = FALSE;
    pScrn->fbOffset    = 0;
    if (info->IsSecondary) pScrn->fbOffset = pScrn->videoRam * 1024;
    if (!RADEONMapMem(pScrn)) return FALSE;

#ifdef XF86DRI
    info->fbX = 0;
    info->fbY = 0;
#endif

    info->PaletteSavedOnVT = FALSE;

    RADEONSave(pScrn);
    if (info->FBDev) {
	unsigned char *RADEONMMIO = info->MMIO;

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) return FALSE;
	info->ModeReg.surface_cntl = INREG(RADEON_SURFACE_CNTL);
    } else {
	if (!RADEONModeInit(pScrn, pScrn->currentMode)) return FALSE;
    }

    RADEONSaveScreen(pScreen, SCREEN_SAVER_ON);

    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    if (info->CurCloneMode) {
	info->CloneFrameX0 =
	    (pScrn->virtualX - info->CurCloneMode->HDisplay) / 2;
	info->CloneFrameY0 =
	    (pScrn->virtualY - info->CurCloneMode->VDisplay) / 2;
	RADEONDoAdjustFrame(pScrn, info->CloneFrameX0, info->CloneFrameY0, TRUE);
    }

				/* Visual setup */
    miClearVisualTypes();
    if (!miSetVisualTypes(pScrn->depth,
			  miGetDefaultVisualMask(pScrn->depth),
			  pScrn->rgbBits,
			  pScrn->defaultVisual)) return FALSE;
    miSetPixmapDepths ();

#ifdef XF86DRI
				/* Setup DRI after visuals have been
				   established, but before fbScreenInit is
				   called.  fbScreenInit will eventually
				   call the driver's InitGLXVisuals call
				   back. */
    {
	/* FIXME: When we move to dynamic allocation of back and depth
	 * buffers, we will want to revisit the following check for 3
	 * times the virtual size of the screen below.
	 */
	int  width_bytes = (pScrn->displayWidth *
			    info->CurrentLayout.pixel_bytes);
	int  maxy        = info->FbMapSize / width_bytes;

	if (xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "Acceleration disabled, not initializing the DRI\n");
	    info->directRenderingEnabled = FALSE;
	} else if (maxy <= pScrn->virtualY * 3) {
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "Static buffer allocation failed -- "
		       "need at least %d kB video memory\n",
		       (pScrn->displayWidth * pScrn->virtualY *
			info->CurrentLayout.pixel_bytes * 3 + 1023) / 1024);
	    info->directRenderingEnabled = FALSE;
	} else if ((info->ChipFamily == CHIP_FAMILY_RS100) ||
		   (info->ChipFamily == CHIP_FAMILY_RS200) ||
		   (info->ChipFamily == CHIP_FAMILY_RS300)) {
	    info->directRenderingEnabled = FALSE;
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "Direct rendering not yet supported on "
		       "IGP320/330/340/350, 7000, 9000 integrated chips\n");
	} else if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		   (info->ChipFamily == CHIP_FAMILY_R350) ||
		   (info->ChipFamily == CHIP_FAMILY_RV350)) {
	    info->directRenderingEnabled = FALSE;
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "Direct rendering not yet supported on "
		       "Radeon 9500/9700 and newer cards\n");
	} else {
	    if (info->IsSecondary)
		info->directRenderingEnabled = FALSE;
	    else {
		/* Xinerama has sync problem with DRI, disable it for now */
		if (xf86IsEntityShared(info->pEnt->index)) {
		    info->directRenderingEnabled = FALSE;
		    xf86DrvMsg(scrnIndex, X_WARNING,
			       "Direct Rendering Disabled -- "
			       "Dual-head configuration is not working with "
			       "DRI at present.\n"
			       "Please use only one Device/Screen "
			       "section in your XFConfig file.\n");
		} else {
		    info->directRenderingEnabled =
			RADEONDRIScreenInit(pScreen);
		}
	    }
	}
    }
#endif

    if (!fbScreenInit(pScreen, info->FB,
		      pScrn->virtualX, pScrn->virtualY,
		      pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth,
		      pScrn->bitsPerPixel))
	return FALSE;

    xf86SetBlackWhitePixels(pScreen);

    if (pScrn->bitsPerPixel > 8) {
	VisualPtr  visual;

	visual = pScreen->visuals + pScreen->numVisuals;
	while (--visual >= pScreen->visuals) {
	    if ((visual->class | DynamicClass) == DirectColor) {
		visual->offsetRed   = pScrn->offset.red;
		visual->offsetGreen = pScrn->offset.green;
		visual->offsetBlue  = pScrn->offset.blue;
		visual->redMask     = pScrn->mask.red;
		visual->greenMask   = pScrn->mask.green;
		visual->blueMask    = pScrn->mask.blue;
	    }
	}
    }

    /* Must be after RGB order fixed */
    fbPictureInit (pScreen, 0, 0);

#ifdef RENDER
    if (PictureGetSubpixelOrder (pScreen) == SubPixelUnknown)
    {
	int subPixelOrder;

	switch (info->DisplayType) {
	case MT_NONE:	subPixelOrder = SubPixelUnknown; break;
	case MT_LCD:	subPixelOrder = SubPixelHorizontalRGB; break;
	case MT_DFP:	subPixelOrder = SubPixelHorizontalRGB; break;
	default:	subPixelOrder = SubPixelNone; break;
	}
	PictureSetSubpixelOrder (pScreen, subPixelOrder);
    }
#endif
				/* Memory manager setup */
#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	FBAreaPtr  fbarea;
	int        width_bytes = (pScrn->displayWidth *
				  info->CurrentLayout.pixel_bytes);
	int        cpp         = info->CurrentLayout.pixel_bytes;
	int        bufferSize  = ((pScrn->virtualY * width_bytes
				   + RADEON_BUFFER_ALIGN)
				  & ~RADEON_BUFFER_ALIGN);
	int        depthSize   = ((((pScrn->virtualY+15) & ~15) * width_bytes
				   + RADEON_BUFFER_ALIGN)
				  & ~RADEON_BUFFER_ALIGN);
	int        l;
	int        scanlines;

	info->frontOffset = 0;
	info->frontPitch = pScrn->displayWidth;

	switch (info->CPMode) {
	case RADEON_DEFAULT_CP_PIO_MODE:
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CP in PIO mode\n");
	    break;
	case RADEON_DEFAULT_CP_BM_MODE:
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CP in BM mode\n");
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CP in UNKNOWN mode\n");
	    break;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB GART aperture\n", info->gartSize);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB for the ring buffer\n", info->ringSize);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB for vertex/indirect buffers\n", info->bufSize);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB for GART textures\n", info->gartTexSize);

	/* Try for front, back, depth, and three framebuffers worth of
	 * pixmap cache.  Should be enough for a fullscreen background
	 * image plus some leftovers.
	 */
	info->textureSize = info->FbMapSize - 5 * bufferSize - depthSize;

	/* If that gives us less than half the available memory, let's
	 * be greedy and grab some more.  Sorry, I care more about 3D
	 * performance than playing nicely, and you'll get around a full
	 * framebuffer's worth of pixmap cache anyway.
	 */
	if (info->textureSize < (int)info->FbMapSize / 2) {
	    info->textureSize = info->FbMapSize - 4 * bufferSize - depthSize;
	}
	if (info->textureSize < (int)info->FbMapSize / 2) {
	    info->textureSize = info->FbMapSize - 3 * bufferSize - depthSize;
	}
	/* If there's still no space for textures, try without pixmap cache */
	if (info->textureSize < 0) {
	    info->textureSize = info->FbMapSize - 2 * bufferSize - depthSize
				- 64/4*64;
	}

	/* Check to see if there is more room available after the 8192nd
	   scanline for textures */
	if ((int)info->FbMapSize - 8192*width_bytes - bufferSize - depthSize
	    > info->textureSize) {
	    info->textureSize =
		info->FbMapSize - 8192*width_bytes - bufferSize - depthSize;
	}

	/* If backbuffer is disabled, don't allocate memory for it */
	if (info->noBackBuffer) {
	   info->textureSize += bufferSize;
	}

	if (info->textureSize > 0) {
	    l = RADEONMinBits((info->textureSize-1) / RADEON_NR_TEX_REGIONS);
	    if (l < RADEON_LOG_TEX_GRANULARITY) l = RADEON_LOG_TEX_GRANULARITY;

	    /* Round the texture size up to the nearest whole number of
	     * texture regions.  Again, be greedy about this, don't
	     * round down.
	     */
	    info->log2TexGran = l;
	    info->textureSize = (info->textureSize >> l) << l;
	} else {
	    info->textureSize = 0;
	}

	/* Set a minimum usable local texture heap size.  This will fit
	 * two 256x256x32bpp textures.
	 */
	if (info->textureSize < 512 * 1024) {
	    info->textureOffset = 0;
	    info->textureSize = 0;
	}

				/* Reserve space for textures */
	info->textureOffset = ((info->FbMapSize - info->textureSize +
				RADEON_BUFFER_ALIGN) &
			       ~(CARD32)RADEON_BUFFER_ALIGN);

				/* Reserve space for the shared depth
                                 * buffer.
				 */
	info->depthOffset = ((info->textureOffset - depthSize +
			      RADEON_BUFFER_ALIGN) &
			     ~(CARD32)RADEON_BUFFER_ALIGN);
	info->depthPitch = pScrn->displayWidth;

				/* Reserve space for the shared back buffer */
	if (info->noBackBuffer) {
	   info->backOffset = info->depthOffset;
	   info->backPitch = pScrn->displayWidth;
	} else {
	   info->backOffset = ((info->depthOffset - bufferSize +
				RADEON_BUFFER_ALIGN) &
			       ~(CARD32)RADEON_BUFFER_ALIGN);
	   info->backPitch = pScrn->displayWidth;
	}

	info->backY = info->backOffset / width_bytes;
	info->backX = (info->backOffset - (info->backY * width_bytes)) / cpp;

	scanlines = info->FbMapSize / width_bytes;
	if (scanlines > 8191) scanlines = 8191;

	MemBox.x1 = 0;
	MemBox.y1 = 0;
	MemBox.x2 = pScrn->displayWidth;
	MemBox.y2 = scanlines;

	if (!xf86InitFBManager(pScreen, &MemBox)) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Memory manager initialization to "
		       "(%d,%d) (%d,%d) failed\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    return FALSE;
	} else {
	    int  width, height;

	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Memory manager initialized to (%d,%d) (%d,%d)\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						    pScrn->displayWidth,
						    2, 0, NULL, NULL,
						    NULL))) {
		xf86DrvMsg(scrnIndex, X_INFO,
			   "Reserved area from (%d,%d) to (%d,%d)\n",
			   fbarea->box.x1, fbarea->box.y1,
			   fbarea->box.x2, fbarea->box.y2);
	    } else {
		xf86DrvMsg(scrnIndex, X_ERROR, "Unable to reserve area\n");
	    }
	    if (xf86QueryLargestOffscreenArea(pScreen, &width,
					      &height, 0, 0, 0)) {
		xf86DrvMsg(scrnIndex, X_INFO,
			   "Largest offscreen area available: %d x %d\n",
			   width, height);

		/* Lines in offscreen area needed for depth buffer and
		 * textures
		 */
		info->depthTexLines = (scanlines
				       - info->depthOffset / width_bytes);
		info->backLines	    = (scanlines
				       - info->backOffset / width_bytes
				       - info->depthTexLines);
		info->backArea	    = NULL;
	    } else {
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "Unable to determine largest offscreen area "
			   "available\n");
		return FALSE;
	    }
	}

	xf86DrvMsg(scrnIndex, X_INFO,
		   "Will use back buffer at offset 0x%x\n",
		   info->backOffset);
	xf86DrvMsg(scrnIndex, X_INFO,
		   "Will use depth buffer at offset 0x%x\n",
		   info->depthOffset);
	xf86DrvMsg(scrnIndex, X_INFO,
		   "Will use %d kb for textures at offset 0x%x\n",
		   info->textureSize/1024, info->textureOffset);

	info->frontPitchOffset = (((info->frontPitch * cpp / 64) << 22) |
				  (info->frontOffset >> 10));

	info->backPitchOffset = (((info->backPitch * cpp / 64) << 22) |
				 (info->backOffset >> 10));

	info->depthPitchOffset = (((info->depthPitch * cpp / 64) << 22) |
				  (info->depthOffset >> 10));
    } else
#endif
    {
	MemBox.x1 = 0;
	MemBox.y1 = 0;
	MemBox.x2 = pScrn->displayWidth;
	y2        = (info->FbMapSize
		     / (pScrn->displayWidth *
			info->CurrentLayout.pixel_bytes));
	if (y2 >= 32768) y2 = 32767; /* because MemBox.y2 is signed short */
	MemBox.y2 = y2;

				/* The acceleration engine uses 14 bit
				   signed coordinates, so we can't have any
				   drawable caches beyond this region. */
	if (MemBox.y2 > 8191) MemBox.y2 = 8191;

	if (!xf86InitFBManager(pScreen, &MemBox)) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Memory manager initialization to "
		       "(%d,%d) (%d,%d) failed\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    return FALSE;
	} else {
	    int       width, height;
	    FBAreaPtr fbarea;

	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Memory manager initialized to (%d,%d) (%d,%d)\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						    pScrn->displayWidth,
						    2, 0, NULL, NULL,
						    NULL))) {
		xf86DrvMsg(scrnIndex, X_INFO,
			   "Reserved area from (%d,%d) to (%d,%d)\n",
			   fbarea->box.x1, fbarea->box.y1,
			   fbarea->box.x2, fbarea->box.y2);
	    } else {
		xf86DrvMsg(scrnIndex, X_ERROR, "Unable to reserve area\n");
	    }
	    if (xf86QueryLargestOffscreenArea(pScreen, &width, &height,
					      0, 0, 0)) {
		xf86DrvMsg(scrnIndex, X_INFO,
			   "Largest offscreen area available: %d x %d\n",
			   width, height);
	    }
	}
    }

				/* Acceleration setup */
    if (!xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	if (RADEONAccelInit(pScreen)) {
	    xf86DrvMsg(scrnIndex, X_INFO, "Acceleration enabled\n");
	    info->accelOn = TRUE;

	    /* FIXME: Figure out why this was added because it shouldn't be! */
	    /* This is needed by the DRI and XAA code for shared entities */
	    pScrn->pScreen = pScreen;
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Acceleration initialization failed\n");
	    xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
	    info->accelOn = FALSE;
	}
    } else {
	xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
	info->accelOn = FALSE;
    }

				/* DGA setup */
    RADEONDGAInit(pScreen);

				/* Backing store setup */
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

				/* Set Silken Mouse */
    xf86SetSilkenMouse(pScreen);

				/* Cursor setup */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

				/* Hardware cursor setup */
    if (!xf86ReturnOptValBool(info->Options, OPTION_SW_CURSOR, FALSE)) {
	if (RADEONCursorInit(pScreen)) {
	    int  width, height;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Using hardware cursor (scanline %ld)\n",
		       info->cursor_start / pScrn->displayWidth
		       / info->CurrentLayout.pixel_bytes);
	    if (xf86QueryLargestOffscreenArea(pScreen, &width, &height,
					      0, 0, 0)) {
		xf86DrvMsg(scrnIndex, X_INFO,
			   "Largest offscreen area available: %d x %d\n",
			   width, height);
	    }
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Hardware cursor initialization failed\n");
	    xf86DrvMsg(scrnIndex, X_INFO, "Using software cursor\n");
	}
    } else {
	info->cursor_start = 0;
	xf86DrvMsg(scrnIndex, X_INFO, "Using software cursor\n");
    }

				/* Colormap setup */
    if (!miCreateDefColormap(pScreen)) return FALSE;
    if (!xf86HandleColormaps(pScreen, 256, info->dac6bits ? 6 : 8,
			     RADEONLoadPalette, NULL,
			     CMAP_PALETTED_TRUECOLOR
#if 0 /* This option messes up text mode! (eich@suse.de) */
			     | CMAP_LOAD_EVEN_IF_OFFSCREEN
#endif
			     | CMAP_RELOAD_ON_MODE_SWITCH)) return FALSE;

				/* DPMS setup */
    xf86DPMSInit(pScreen, RADEONDisplayPowerManagementSet, 0);

    RADEONInitVideo(pScreen);

				/* Provide SaveScreen */
    pScreen->SaveScreen  = RADEONSaveScreen;

				/* Wrap CloseScreen */
    info->CloseScreen    = pScreen->CloseScreen;
    pScreen->CloseScreen = RADEONCloseScreen;

				/* Note unused options */
    if (serverGeneration == 1)
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

#ifdef XF86DRI
				/* DRI finalization */
    if (info->directRenderingEnabled) {
				/* Now that mi, fb, drm and others have
				   done their thing, complete the DRI
				   setup. */
	info->directRenderingEnabled = RADEONDRIFinishScreenInit(pScreen);
    }
    if (info->directRenderingEnabled) {
	if ((info->DispPriority == 1) && (!info->IsPCI)) {
	    /* we need to re-calculate bandwidth because of AGPMode difference. */
	    RADEONInitDispBandwidth(pScrn);
	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering enabled\n");
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering disabled\n");
    }
#endif

    info->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = RADEONBlockHandler;

    return TRUE;
}

/* Write common registers (initialized to 0) */
static void RADEONRestoreCommonRegisters(ScrnInfoPtr pScrn,
					 RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_OVR_CLR,            restore->ovr_clr);
    OUTREG(RADEON_OVR_WID_LEFT_RIGHT, restore->ovr_wid_left_right);
    OUTREG(RADEON_OVR_WID_TOP_BOTTOM, restore->ovr_wid_top_bottom);
    OUTREG(RADEON_OV0_SCALE_CNTL,     restore->ov0_scale_cntl);
    OUTREG(RADEON_SUBPIC_CNTL,        restore->subpic_cntl);
    OUTREG(RADEON_VIPH_CONTROL,       restore->viph_control);
    OUTREG(RADEON_I2C_CNTL_1,         restore->i2c_cntl_1);
    OUTREG(RADEON_GEN_INT_CNTL,       restore->gen_int_cntl);
    OUTREG(RADEON_CAP0_TRIG_CNTL,     restore->cap0_trig_cntl);
    OUTREG(RADEON_CAP1_TRIG_CNTL,     restore->cap1_trig_cntl);
    OUTREG(RADEON_BUS_CNTL,           restore->bus_cntl);
    OUTREG(RADEON_SURFACE_CNTL,       restore->surface_cntl);

    /* Workaround for the VT switching problem in dual-head mode.  This
     * problem only occurs on RV style chips, typically when a FP and
     * CRT are connected.
     */
    if (info->HasCRTC2 &&
	!info->IsSwitching &&
	info->ChipFamily != CHIP_FAMILY_R200 &&
	info->ChipFamily != CHIP_FAMILY_R300 &&
	info->ChipFamily != CHIP_FAMILY_R350 &&
	info->ChipFamily != CHIP_FAMILY_RV350) {
	CARD32        tmp;
        RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

	if (pRADEONEnt->HasSecondary || info->Clone) {
	    tmp = INREG(RADEON_DAC_CNTL2);
	    OUTREG(RADEON_DAC_CNTL2, tmp & ~RADEON_DAC2_DAC_CLK_SEL);
	    usleep(100000);
	}
    }
}

/* Write miscellaneous registers which might have been destroyed by an fbdevHW
 * call
 */
static void RADEONRestoreFBDevRegisters(ScrnInfoPtr pScrn,
					 RADEONSavePtr restore)
{
#ifdef XF86DRI
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    /* Restore register for vertical blank interrupts */
    if (info->irq) {
	OUTREG(RADEON_GEN_INT_CNTL, restore->gen_int_cntl);
    }

    /* Restore registers for page flipping */
    if (info->allowPageFlip) {
	OUTREG(RADEON_CRTC_OFFSET_CNTL, restore->crtc_offset_cntl);
	if (info->HasCRTC2) {
	    OUTREG(RADEON_CRTC2_OFFSET_CNTL, restore->crtc2_offset_cntl);
	}
    }
#endif
}

/* Write CRTC registers */
static void RADEONRestoreCrtcRegisters(ScrnInfoPtr pScrn,
				       RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_CRTC_GEN_CNTL, restore->crtc_gen_cntl);

    OUTREGP(RADEON_CRTC_EXT_CNTL,
	    restore->crtc_ext_cntl,
	    RADEON_CRTC_VSYNC_DIS |
	    RADEON_CRTC_HSYNC_DIS |
	    RADEON_CRTC_DISPLAY_DIS);

    OUTREGP(RADEON_DAC_CNTL,
	    restore->dac_cntl,
	    RADEON_DAC_RANGE_CNTL |
	    RADEON_DAC_BLANKING);

    OUTREG(RADEON_CRTC_H_TOTAL_DISP,    restore->crtc_h_total_disp);
    OUTREG(RADEON_CRTC_H_SYNC_STRT_WID, restore->crtc_h_sync_strt_wid);
    OUTREG(RADEON_CRTC_V_TOTAL_DISP,    restore->crtc_v_total_disp);
    OUTREG(RADEON_CRTC_V_SYNC_STRT_WID, restore->crtc_v_sync_strt_wid);
    OUTREG(RADEON_CRTC_OFFSET,          restore->crtc_offset);
    OUTREG(RADEON_CRTC_OFFSET_CNTL,     restore->crtc_offset_cntl);
    OUTREG(RADEON_CRTC_PITCH,           restore->crtc_pitch);
    OUTREG(RADEON_DISP_MERGE_CNTL,      restore->disp_merge_cntl);
    OUTREG(RADEON_CRTC_MORE_CNTL,       restore->crtc_more_cntl);
}

/* Write CRTC2 registers */
static void RADEONRestoreCrtc2Registers(ScrnInfoPtr pScrn,
					RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREGP(RADEON_CRTC2_GEN_CNTL,
	    restore->crtc2_gen_cntl,
	    RADEON_CRTC2_VSYNC_DIS |
	    RADEON_CRTC2_HSYNC_DIS |
	    RADEON_CRTC2_DISP_DIS);

    OUTREG(RADEON_DAC_CNTL2, restore->dac2_cntl);

    OUTREG(RADEON_TV_DAC_CNTL, 0x00280203);
    if ((info->ChipFamily == CHIP_FAMILY_R200) ||
	(info->ChipFamily == CHIP_FAMILY_R300) ||
	(info->ChipFamily == CHIP_FAMILY_R350) ||
	(info->ChipFamily == CHIP_FAMILY_RV350)) {
	OUTREG(RADEON_DISP_OUTPUT_CNTL, restore->disp_output_cntl);
    } else {
	OUTREG(RADEON_DISP_HW_DEBUG, restore->disp_hw_debug);
    }

    OUTREG(RADEON_CRTC2_H_TOTAL_DISP,    restore->crtc2_h_total_disp);
    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, restore->crtc2_h_sync_strt_wid);
    OUTREG(RADEON_CRTC2_V_TOTAL_DISP,    restore->crtc2_v_total_disp);
    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, restore->crtc2_v_sync_strt_wid);
    OUTREG(RADEON_CRTC2_OFFSET,          restore->crtc2_offset);
    OUTREG(RADEON_CRTC2_OFFSET_CNTL,     restore->crtc2_offset_cntl);
    OUTREG(RADEON_CRTC2_PITCH,           restore->crtc2_pitch);
    OUTREG(RADEON_DISP2_MERGE_CNTL,      restore->disp2_merge_cntl);

    if ((info->DisplayType == MT_DFP && info->IsSecondary) ||
	info->CloneType == MT_DFP) {
	OUTREG(RADEON_FP_H2_SYNC_STRT_WID, restore->fp2_h_sync_strt_wid);
	OUTREG(RADEON_FP_V2_SYNC_STRT_WID, restore->fp2_v_sync_strt_wid);
	OUTREG(RADEON_FP2_GEN_CNTL,        restore->fp2_gen_cntl);
    }
#if 0
    /* Hack for restoring text mode -- fixed elsewhere */
    usleep(100000);
#endif
}

/* Write flat panel registers */
static void RADEONRestoreFPRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long  tmp;

    OUTREG(RADEON_FP_CRTC_H_TOTAL_DISP, restore->fp_crtc_h_total_disp);
    OUTREG(RADEON_FP_CRTC_V_TOTAL_DISP, restore->fp_crtc_v_total_disp);
    OUTREG(RADEON_FP_H_SYNC_STRT_WID,   restore->fp_h_sync_strt_wid);
    OUTREG(RADEON_FP_V_SYNC_STRT_WID,   restore->fp_v_sync_strt_wid);
    OUTREG(RADEON_TMDS_PLL_CNTL,        restore->tmds_pll_cntl);
    OUTREG(RADEON_TMDS_TRANSMITTER_CNTL,restore->tmds_transmitter_cntl);
    OUTREG(RADEON_FP_HORZ_STRETCH,      restore->fp_horz_stretch);
    OUTREG(RADEON_FP_VERT_STRETCH,      restore->fp_vert_stretch);
    OUTREG(RADEON_FP_GEN_CNTL,          restore->fp_gen_cntl);

    /* old AIW Radeon has some BIOS initialization problem
     * with display buffer underflow, only occurs to DFP
     */
    if (!info->HasCRTC2)
	OUTREG(RADEON_GRPH_BUFFER_CNTL,
	       INREG(RADEON_GRPH_BUFFER_CNTL) & ~0x7f0000);

    if (info->DisplayType != MT_DFP) {
	unsigned long tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
        OUTREG(RADEON_BIOS_5_SCRATCH, restore->bios_5_scratch);

	if (info->IsMobility || info->IsIGP) {
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	    */
	    if (!(restore->lvds_gen_cntl & RADEON_LVDS_ON)) {
		OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	    }
	}

	tmp = INREG(RADEON_LVDS_GEN_CNTL);
	if ((tmp & (RADEON_LVDS_ON | RADEON_LVDS_BLON)) ==
	    (restore->lvds_gen_cntl & (RADEON_LVDS_ON | RADEON_LVDS_BLON))) {
	    OUTREG(RADEON_LVDS_GEN_CNTL, restore->lvds_gen_cntl);
	} else {
	    if (restore->lvds_gen_cntl & (RADEON_LVDS_ON | RADEON_LVDS_BLON)) {
		usleep(RADEONPTR(pScrn)->PanelPwrDly * 1000);
		OUTREG(RADEON_LVDS_GEN_CNTL, restore->lvds_gen_cntl);
	    } else {
		OUTREG(RADEON_LVDS_GEN_CNTL,
		       restore->lvds_gen_cntl | RADEON_LVDS_BLON);
		usleep(RADEONPTR(pScrn)->PanelPwrDly * 1000);
		OUTREG(RADEON_LVDS_GEN_CNTL, restore->lvds_gen_cntl);
	    }
	}

	if (info->IsMobility || info->IsIGP) {
	    if (!(restore->lvds_gen_cntl & RADEON_LVDS_ON)) {
		OUTPLL(RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	    }
	}
    }
}

static void RADEONPLLWaitForReadUpdateComplete(ScrnInfoPtr pScrn)
{
    int i = 0;

    /* FIXME: Certain revisions of R300 can't recover here.  Not sure of
       the cause yet, but this workaround will mask the problem for now.
       Other chips usually will pass at the very first test, so the
       workaround shouldn't have any effect on them. */
    for (i = 0;
	 (i < 10000 &&
	  INPLL(pScrn, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE_R);
	 i++);
}

static void RADEONPLLWriteUpdate(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    while (INPLL(pScrn, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE_R);

    OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
	    RADEON_PPLL_ATOMIC_UPDATE_W,
	    ~(RADEON_PPLL_ATOMIC_UPDATE_W));
}

static void RADEONPLL2WaitForReadUpdateComplete(ScrnInfoPtr pScrn)
{
    int i = 0;

    /* FIXME: Certain revisions of R300 can't recover here.  Not sure of
       the cause yet, but this workaround will mask the problem for now.
       Other chips usually will pass at the very first test, so the
       workaround shouldn't have any effect on them. */
    for (i = 0;
	 (i < 10000 &&
	  INPLL(pScrn, RADEON_P2PLL_REF_DIV) & RADEON_P2PLL_ATOMIC_UPDATE_R);
	 i++);
}

static void RADEONPLL2WriteUpdate(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    while (INPLL(pScrn, RADEON_P2PLL_REF_DIV) & RADEON_P2PLL_ATOMIC_UPDATE_R);

    OUTPLLP(pScrn, RADEON_P2PLL_REF_DIV,
	    RADEON_P2PLL_ATOMIC_UPDATE_W,
	    ~(RADEON_P2PLL_ATOMIC_UPDATE_W));
}

/* Write PLL registers */
static void RADEONRestorePLLRegisters(ScrnInfoPtr pScrn,
				      RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (info->IsMobility) {
        /* A temporal workaround for the occational blanking on certain laptop panels.
           This appears to related to the PLL divider registers (fail to lock?).
	   It occurs even when all dividers are the same with their old settings.
           In this case we really don't need to fiddle with PLL registers.
           By doing this we can avoid the blanking problem with some panels.
        */
        if ((restore->ppll_ref_div == (INPLL(pScrn, RADEON_PPLL_REF_DIV) & RADEON_PPLL_REF_DIV_MASK)) &&
	    (restore->ppll_div_3 == (INPLL(pScrn, RADEON_PPLL_DIV_3) & (RADEON_PPLL_POST3_DIV_MASK | RADEON_PPLL_FB3_DIV_MASK))))
            return;
    }

    OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL,
	    RADEON_VCLK_SRC_SEL_CPUCLK,
	    ~(RADEON_VCLK_SRC_SEL_MASK));

    OUTPLLP(pScrn,
	    RADEON_PPLL_CNTL,
	    RADEON_PPLL_RESET
	    | RADEON_PPLL_ATOMIC_UPDATE_EN
	    | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN,
	    ~(RADEON_PPLL_RESET
	      | RADEON_PPLL_ATOMIC_UPDATE_EN
	      | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN));

    OUTREGP(RADEON_CLOCK_CNTL_INDEX,
	    RADEON_PLL_DIV_SEL,
	    ~(RADEON_PLL_DIV_SEL));

    if ((info->ChipFamily == CHIP_FAMILY_R300) ||
	(info->ChipFamily == CHIP_FAMILY_RS300) ||
	(info->ChipFamily == CHIP_FAMILY_R350) ||
	(info->ChipFamily == CHIP_FAMILY_RV350)) {
	if (restore->ppll_ref_div & R300_PPLL_REF_DIV_ACC_MASK) {
	    /* When restoring console mode, use saved PPLL_REF_DIV
	     * setting.
	     */
	    OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
		    restore->ppll_ref_div,
		    0);
	} else {
	    /* R300 uses ref_div_acc field as real ref divider */
	    OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
		    (restore->ppll_ref_div << R300_PPLL_REF_DIV_ACC_SHIFT),
		    ~R300_PPLL_REF_DIV_ACC_MASK);
	}
    } else {
	OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
		restore->ppll_ref_div,
		~RADEON_PPLL_REF_DIV_MASK);
    }

    OUTPLLP(pScrn, RADEON_PPLL_DIV_3,
	    restore->ppll_div_3,
	    ~RADEON_PPLL_FB3_DIV_MASK);

    OUTPLLP(pScrn, RADEON_PPLL_DIV_3,
	    restore->ppll_div_3,
	    ~RADEON_PPLL_POST3_DIV_MASK);

    RADEONPLLWriteUpdate(pScrn);
    RADEONPLLWaitForReadUpdateComplete(pScrn);

    OUTPLL(RADEON_HTOTAL_CNTL, restore->htotal_cntl);

    OUTPLLP(pScrn, RADEON_PPLL_CNTL,
	    0,
	    ~(RADEON_PPLL_RESET
	      | RADEON_PPLL_SLEEP
	      | RADEON_PPLL_ATOMIC_UPDATE_EN
	      | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN));

    xf86DrvMsg(0, X_INFO, "Wrote: rd=%d, fd=%d, pd=%d\n",
	       restore->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
	       restore->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
	       (restore->ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16);

    RADEONTRACE(("Wrote: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
	       restore->ppll_ref_div,
	       restore->ppll_div_3,
	       restore->htotal_cntl,
	       INPLL(pScrn, RADEON_PPLL_CNTL)));
    RADEONTRACE(("Wrote: rd=%d, fd=%d, pd=%d\n",
	       restore->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
	       restore->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
	       (restore->ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16));

    usleep(5000); /* Let the clock to lock */

    OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL,
	    RADEON_VCLK_SRC_SEL_PPLLCLK,
	    ~(RADEON_VCLK_SRC_SEL_MASK));
}


/* Write PLL2 registers */
static void RADEONRestorePLL2Registers(ScrnInfoPtr pScrn,
				       RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL,
	    RADEON_PIX2CLK_SRC_SEL_CPUCLK,
	    ~(RADEON_PIX2CLK_SRC_SEL_MASK));

    OUTPLLP(pScrn,
	    RADEON_P2PLL_CNTL,
	    RADEON_P2PLL_RESET
	    | RADEON_P2PLL_ATOMIC_UPDATE_EN
	    | RADEON_P2PLL_VGA_ATOMIC_UPDATE_EN,
	    ~(RADEON_P2PLL_RESET
	      | RADEON_P2PLL_ATOMIC_UPDATE_EN
	      | RADEON_P2PLL_VGA_ATOMIC_UPDATE_EN));

    OUTPLLP(pScrn, RADEON_P2PLL_REF_DIV,
	    restore->p2pll_ref_div,
	    ~RADEON_P2PLL_REF_DIV_MASK);

    OUTPLLP(pScrn, RADEON_P2PLL_DIV_0,
	    restore->p2pll_div_0,
	    ~RADEON_P2PLL_FB0_DIV_MASK);

    OUTPLLP(pScrn, RADEON_P2PLL_DIV_0,
	    restore->p2pll_div_0,
	    ~RADEON_P2PLL_POST0_DIV_MASK);

    RADEONPLL2WriteUpdate(pScrn);
    RADEONPLL2WaitForReadUpdateComplete(pScrn);

    OUTPLL(RADEON_HTOTAL2_CNTL, restore->htotal_cntl2);

    OUTPLLP(pScrn, RADEON_P2PLL_CNTL,
	    0,
	    ~(RADEON_P2PLL_RESET
	      | RADEON_P2PLL_SLEEP
	      | RADEON_P2PLL_ATOMIC_UPDATE_EN
	      | RADEON_P2PLL_VGA_ATOMIC_UPDATE_EN));

    RADEONTRACE(("Wrote: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
	       restore->p2pll_ref_div,
	       restore->p2pll_div_0,
	       restore->htotal_cntl2,
	       INPLL(pScrn, RADEON_P2PLL_CNTL)));
    RADEONTRACE(("Wrote: rd=%d, fd=%d, pd=%d\n",
	       restore->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
	       restore->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK,
	       (restore->p2pll_div_0 & RADEON_P2PLL_POST0_DIV_MASK) >>16));

    usleep(5000); /* Let the clock to lock */

    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL,
	    RADEON_PIX2CLK_SRC_SEL_P2PLLCLK,
	    ~(RADEON_PIX2CLK_SRC_SEL_MASK));
}

#if 0
/* Write palette data */
static void RADEONRestorePalette(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i;

    if (!restore->palette_valid) return;

    PAL_SELECT(1);
    OUTPAL_START(0);
    for (i = 0; i < 256; i++) {
	RADEONWaitForFifo(pScrn, 32); /* delay */
	OUTPAL_NEXT_CARD32(restore->palette2[i]);
    }

    PAL_SELECT(0);
    OUTPAL_START(0);
    for (i = 0; i < 256; i++) {
	RADEONWaitForFifo(pScrn, 32); /* delay */
	OUTPAL_NEXT_CARD32(restore->palette[i]);
    }
}
#endif

/* Write out state to define a new video mode */
static void RADEONRestoreMode(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr      info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    static RADEONSaveRec  restore0;

    /* For Non-dual head card, we don't have private field in the Entity */
    if (!info->HasCRTC2) {
	RADEONRestoreCommonRegisters(pScrn, restore);
	RADEONRestoreCrtcRegisters(pScrn, restore);
	RADEONRestoreFPRegisters(pScrn, restore);
	RADEONRestorePLLRegisters(pScrn, restore);
	return;
    }

    RADEONTRACE(("RADEONRestoreMode(%p)\n", restore));

    /* When changing mode with Dual-head card, care must be taken for
     * the special order in setting registers. CRTC2 has to be set
     * before changing CRTC_EXT register.  In the dual-head setup, X
     * server calls this routine twice with primary and secondary pScrn
     * pointers respectively. The calls can come with different
     * order. Regardless the order of X server issuing the calls, we
     * have to ensure we set registers in the right order!!!  Otherwise
     * we may get a blank screen.
     */
    if (info->IsSecondary) {
	if (!pRADEONEnt->RestorePrimary  && !info->IsSwitching)
	    RADEONRestoreCommonRegisters(pScrn, restore);
	RADEONRestoreCrtc2Registers(pScrn, restore);
	RADEONRestorePLL2Registers(pScrn, restore);

	if(info->IsSwitching) return;

	pRADEONEnt->IsSecondaryRestored = TRUE;

	if (pRADEONEnt->RestorePrimary) {
	    pRADEONEnt->RestorePrimary = FALSE;

	    RADEONRestoreCrtcRegisters(pScrn, &restore0);
	    RADEONRestoreFPRegisters(pScrn, &restore0);
	    RADEONRestorePLLRegisters(pScrn, &restore0);
	    pRADEONEnt->IsSecondaryRestored = FALSE;
	}
    } else {
	if (!pRADEONEnt->IsSecondaryRestored)
	    RADEONRestoreCommonRegisters(pScrn, restore);

	if (info->Clone) {
	    RADEONRestoreCrtc2Registers(pScrn, restore);
	    RADEONRestorePLL2Registers(pScrn, restore);
	}

	if (!pRADEONEnt->HasSecondary || pRADEONEnt->IsSecondaryRestored ||
	    info->IsSwitching) {
	    pRADEONEnt->IsSecondaryRestored = FALSE;

	    RADEONRestoreCrtcRegisters(pScrn, restore);
	    RADEONRestoreFPRegisters(pScrn, restore);
	    RADEONRestorePLLRegisters(pScrn, restore);
	} else {
	    memcpy(&restore0, restore, sizeof(restore0));
	    pRADEONEnt->RestorePrimary = TRUE;
	}
    }

#if 0
    RADEONRestorePalette(pScrn, &info->SavedReg);
#endif
}

/* Read common registers */
static void RADEONSaveCommonRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->ovr_clr            = INREG(RADEON_OVR_CLR);
    save->ovr_wid_left_right = INREG(RADEON_OVR_WID_LEFT_RIGHT);
    save->ovr_wid_top_bottom = INREG(RADEON_OVR_WID_TOP_BOTTOM);
    save->ov0_scale_cntl     = INREG(RADEON_OV0_SCALE_CNTL);
    save->subpic_cntl        = INREG(RADEON_SUBPIC_CNTL);
    save->viph_control       = INREG(RADEON_VIPH_CONTROL);
    save->i2c_cntl_1         = INREG(RADEON_I2C_CNTL_1);
    save->gen_int_cntl       = INREG(RADEON_GEN_INT_CNTL);
    save->cap0_trig_cntl     = INREG(RADEON_CAP0_TRIG_CNTL);
    save->cap1_trig_cntl     = INREG(RADEON_CAP1_TRIG_CNTL);
    save->bus_cntl           = INREG(RADEON_BUS_CNTL);
    save->surface_cntl	     = INREG(RADEON_SURFACE_CNTL);
    save->grph_buffer_cntl   = INREG(RADEON_GRPH_BUFFER_CNTL);
    save->grph2_buffer_cntl  = INREG(RADEON_GRPH2_BUFFER_CNTL);
}

/* Read miscellaneous registers which might be destroyed by an fbdevHW call */
static void RADEONSaveFBDevRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
#ifdef XF86DRI
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    /* Save register for vertical blank interrupts */
    if (info->irq) {
	save->gen_int_cntl = INREG(RADEON_GEN_INT_CNTL);
    }

    /* Save registers for page flipping */
    if (info->allowPageFlip) {
	save->crtc_offset_cntl = INREG(RADEON_CRTC_OFFSET_CNTL);
	if (info->HasCRTC2) {
	    save->crtc2_offset_cntl = INREG(RADEON_CRTC2_OFFSET_CNTL);
	}
    }
#endif
}

/* Read CRTC registers */
static void RADEONSaveCrtcRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->crtc_gen_cntl        = INREG(RADEON_CRTC_GEN_CNTL);
    save->crtc_ext_cntl        = INREG(RADEON_CRTC_EXT_CNTL);
    save->dac_cntl             = INREG(RADEON_DAC_CNTL);
    save->crtc_h_total_disp    = INREG(RADEON_CRTC_H_TOTAL_DISP);
    save->crtc_h_sync_strt_wid = INREG(RADEON_CRTC_H_SYNC_STRT_WID);
    save->crtc_v_total_disp    = INREG(RADEON_CRTC_V_TOTAL_DISP);
    save->crtc_v_sync_strt_wid = INREG(RADEON_CRTC_V_SYNC_STRT_WID);
    save->crtc_offset          = INREG(RADEON_CRTC_OFFSET);
    save->crtc_offset_cntl     = INREG(RADEON_CRTC_OFFSET_CNTL);
    save->crtc_pitch           = INREG(RADEON_CRTC_PITCH);
    save->disp_merge_cntl      = INREG(RADEON_DISP_MERGE_CNTL);
    save->crtc_more_cntl       = INREG(RADEON_CRTC_MORE_CNTL);
}

/* Read flat panel registers */
static void RADEONSaveFPRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->fp_crtc_h_total_disp = INREG(RADEON_FP_CRTC_H_TOTAL_DISP);
    save->fp_crtc_v_total_disp = INREG(RADEON_FP_CRTC_V_TOTAL_DISP);
    save->fp_gen_cntl          = INREG(RADEON_FP_GEN_CNTL);
    save->fp_h_sync_strt_wid   = INREG(RADEON_FP_H_SYNC_STRT_WID);
    save->fp_horz_stretch      = INREG(RADEON_FP_HORZ_STRETCH);
    save->fp_v_sync_strt_wid   = INREG(RADEON_FP_V_SYNC_STRT_WID);
    save->fp_vert_stretch      = INREG(RADEON_FP_VERT_STRETCH);
    save->lvds_gen_cntl        = INREG(RADEON_LVDS_GEN_CNTL);
    save->lvds_pll_cntl        = INREG(RADEON_LVDS_PLL_CNTL);
    save->tmds_pll_cntl        = INREG(RADEON_TMDS_PLL_CNTL);
    save->tmds_transmitter_cntl= INREG(RADEON_TMDS_TRANSMITTER_CNTL);
    save->bios_5_scratch       = INREG(RADEON_BIOS_5_SCRATCH);

    if (info->ChipFamily == CHIP_FAMILY_RV280) {
	/* bit 22 of TMDS_PLL_CNTL is read-back inverted */
	save->tmds_pll_cntl ^= (1 << 22);
    }
}

/* Read CRTC2 registers */
static void RADEONSaveCrtc2Registers(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->dac2_cntl             = INREG(RADEON_DAC_CNTL2);
    save->disp_output_cntl      = INREG(RADEON_DISP_OUTPUT_CNTL);
    save->disp_hw_debug         = INREG (RADEON_DISP_HW_DEBUG);

    save->crtc2_gen_cntl        = INREG(RADEON_CRTC2_GEN_CNTL);
    save->crtc2_h_total_disp    = INREG(RADEON_CRTC2_H_TOTAL_DISP);
    save->crtc2_h_sync_strt_wid = INREG(RADEON_CRTC2_H_SYNC_STRT_WID);
    save->crtc2_v_total_disp    = INREG(RADEON_CRTC2_V_TOTAL_DISP);
    save->crtc2_v_sync_strt_wid = INREG(RADEON_CRTC2_V_SYNC_STRT_WID);
    save->crtc2_offset          = INREG(RADEON_CRTC2_OFFSET);
    save->crtc2_offset_cntl     = INREG(RADEON_CRTC2_OFFSET_CNTL);
    save->crtc2_pitch           = INREG(RADEON_CRTC2_PITCH);

    save->fp2_h_sync_strt_wid   = INREG (RADEON_FP_H2_SYNC_STRT_WID);
    save->fp2_v_sync_strt_wid   = INREG (RADEON_FP_V2_SYNC_STRT_WID);
    save->fp2_gen_cntl          = INREG (RADEON_FP2_GEN_CNTL);
    save->disp2_merge_cntl      = INREG(RADEON_DISP2_MERGE_CNTL);
}

/* Read PLL registers */
static void RADEONSavePLLRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    save->ppll_ref_div = INPLL(pScrn, RADEON_PPLL_REF_DIV);
    save->ppll_div_3   = INPLL(pScrn, RADEON_PPLL_DIV_3);
    save->htotal_cntl  = INPLL(pScrn, RADEON_HTOTAL_CNTL);

    RADEONTRACE(("Read: 0x%08x 0x%08x 0x%08x\n",
		 save->ppll_ref_div,
		 save->ppll_div_3,
		 save->htotal_cntl));
    RADEONTRACE(("Read: rd=%d, fd=%d, pd=%d\n",
		 save->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
		 save->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
		 (save->ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16));
}

/* Read PLL registers */
static void RADEONSavePLL2Registers(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    save->p2pll_ref_div = INPLL(pScrn, RADEON_P2PLL_REF_DIV);
    save->p2pll_div_0   = INPLL(pScrn, RADEON_P2PLL_DIV_0);
    save->htotal_cntl2  = INPLL(pScrn, RADEON_HTOTAL2_CNTL);

    RADEONTRACE(("Read: 0x%08x 0x%08x 0x%08x\n",
		 save->p2pll_ref_div,
		 save->p2pll_div_0,
		 save->htotal_cntl2));
    RADEONTRACE(("Read: rd=%d, fd=%d, pd=%d\n",
		 save->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
		 save->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK,
		 (save->p2pll_div_0 & RADEON_P2PLL_POST0_DIV_MASK) >> 16));
}

/* Read palette data */
static void RADEONSavePalette(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i;

#ifdef ENABLE_FLAT_PANEL
    /* Select palette 0 (main CRTC) if using FP-enabled chip */
 /* if (info->Port1 == MT_DFP) PAL_SELECT(1); */
#endif
    PAL_SELECT(1);
    INPAL_START(0);
    for (i = 0; i < 256; i++) save->palette2[i] = INPAL_NEXT();
    PAL_SELECT(0);
    INPAL_START(0);
    for (i = 0; i < 256; i++) save->palette[i] = INPAL_NEXT();
    save->palette_valid = TRUE;
}

/* Save state that defines current video mode */
static void RADEONSaveMode(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    RADEONTRACE(("RADEONSaveMode(%p)\n", save));
    RADEONSaveCommonRegisters(pScrn, save);
    if (info->IsSecondary) {
	RADEONSaveCrtc2Registers(pScrn, save);
	RADEONSavePLL2Registers(pScrn, save);
    } else {
	RADEONSavePLLRegisters(pScrn, save);
	RADEONSaveCrtcRegisters(pScrn, save);
	RADEONSaveFPRegisters(pScrn, save);

	if (info->Clone) {
	    RADEONSaveCrtc2Registers(pScrn, save);
	    RADEONSavePLL2Registers(pScrn, save);
	}
     /* RADEONSavePalette(pScrn, save); */
    }

    RADEONTRACE(("RADEONSaveMode returns %p\n", save));
}

/* Save everything needed to restore the original VC state */
static void RADEONSave(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr  save       = &info->SavedReg;
    vgaHWPtr       hwp        = VGAHWPTR(pScrn);

    RADEONTRACE(("RADEONSave\n"));
    if (info->FBDev) {
	fbdevHWSave(pScrn);
	return;
    }

    if (!info->IsSecondary) {
	vgaHWUnlock(hwp);
#if defined(__powerpc__)
	/* temporary hack to prevent crashing on PowerMacs when trying to
	 * read VGA fonts and colormap, will find a better solution
	 * in the future
	 */
	vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_MODE); /* Save mode only */
#else
	vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_MODE | VGA_SR_FONTS); /* Save mode
						       * & fonts & cmap
						       */
#endif
	vgaHWLock(hwp);
	save->dp_datatype      = INREG(RADEON_DP_DATATYPE);
	save->rbbm_soft_reset  = INREG(RADEON_RBBM_SOFT_RESET);
	save->clock_cntl_index = INREG(RADEON_CLOCK_CNTL_INDEX);
	if (info->R300CGWorkaround) R300CGWorkaround(pScrn);
    }

    RADEONSaveMode(pScrn, save);
}

/* Restore the original (text) mode */
static void RADEONRestore(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr  restore    = &info->SavedReg;
    vgaHWPtr       hwp        = VGAHWPTR(pScrn);

    RADEONTRACE(("RADEONRestore\n"));

#if X_BYTE_ORDER == X_BIG_ENDIAN
    RADEONWaitForFifo(pScrn, 1);
    OUTREG(RADEON_RBBM_GUICNTL, RADEON_HOST_DATA_SWAP_NONE);
#endif

    if (info->FBDev) {
	fbdevHWRestore(pScrn);
	return;
    }
    RADEONBlank(pScrn);

    OUTREG(RADEON_CLOCK_CNTL_INDEX, restore->clock_cntl_index);
    if (info->R300CGWorkaround) R300CGWorkaround(pScrn);
    OUTREG(RADEON_RBBM_SOFT_RESET,  restore->rbbm_soft_reset);
    OUTREG(RADEON_DP_DATATYPE,      restore->dp_datatype);
    OUTREG(RADEON_GRPH_BUFFER_CNTL, restore->grph_buffer_cntl);
    OUTREG(RADEON_GRPH2_BUFFER_CNTL, restore->grph2_buffer_cntl);

#if 0
    /* M6 card has trouble restoring text mode for its CRT.
     * This is fixed elsewhere and will be removed in the future.
     */
    if ((xf86IsEntityShared(info->pEnt->index) || info->Clone)
	&& info->IsM6)
	OUTREG(RADEON_DAC_CNTL2, restore->dac2_cntl);
#endif

    RADEONRestoreMode(pScrn, restore);

#if 0
    /* Temp fix to "solve" VT switch problems.  When switching VTs on
     * some systems, the console can either hang or the fonts can be
     * corrupted.  This hack solves the problem 99% of the time.  A
     * correct fix is being worked on.
     */
    usleep(100000);
#endif

    if (!info->IsSecondary) {
	vgaHWUnlock(hwp);
#if defined(__powerpc__)
	/* Temporary hack to prevent crashing on PowerMacs when trying to
	 * write VGA fonts, will find a better solution in the future
	 */
	vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_MODE );
#else
	vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_MODE | VGA_SR_FONTS );
#endif
	vgaHWLock(hwp);
    } else {
        RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
	ScrnInfoPtr   pScrn0;
	vgaHWPtr      hwp0;

	pScrn0 = pRADEONEnt->pPrimaryScrn;
	hwp0   = VGAHWPTR(pScrn0);
	vgaHWUnlock(hwp0);
	vgaHWRestore(pScrn0, &hwp0->SavedReg, VGA_SR_MODE | VGA_SR_FONTS );
	vgaHWLock(hwp0);
    }
    RADEONUnblank(pScrn);

#if 0
    RADEONWaitForVerticalSync(pScrn);
#endif
}

/* Define common registers for requested video mode */
static void RADEONInitCommonRegisters(RADEONSavePtr save, RADEONInfoPtr info)
{
    save->ovr_clr            = 0;
    save->ovr_wid_left_right = 0;
    save->ovr_wid_top_bottom = 0;
    save->ov0_scale_cntl     = 0;
    save->subpic_cntl        = 0;
    save->viph_control       = 0;
    save->i2c_cntl_1         = 0;
    save->rbbm_soft_reset    = 0;
    save->cap0_trig_cntl     = 0;
    save->cap1_trig_cntl     = 0;
    save->bus_cntl           = info->BusCntl;
    /*
     * If bursts are enabled, turn on discards
     * Radeon doesn't have write bursts
     */
    if (save->bus_cntl & (RADEON_BUS_READ_BURST))
	save->bus_cntl |= RADEON_BUS_RD_DISCARD_EN;
}

/* Calculate display buffer watermark to prevent buffer underflow */
static void RADEONInitDispBandwidth(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONInfoPtr  info2 = NULL;

    DisplayModePtr mode1, mode2;

    CARD32 temp, data, mem_trcd, mem_trp, mem_tras, mem_trbs=0;
    float mem_tcas;
    int k1, c;
    CARD32 MemTrcdExtMemCntl[4]     = {1, 2, 3, 4};
    CARD32 MemTrpExtMemCntl[4]      = {1, 2, 3, 4};
    CARD32 MemTrasExtMemCntl[8]     = {1, 2, 3, 4, 5, 6, 7, 8};

    CARD32 MemTrcdMemTimingCntl[8]     = {1, 2, 3, 4, 5, 6, 7, 8};
    CARD32 MemTrpMemTimingCntl[8]      = {1, 2, 3, 4, 5, 6, 7, 8};
    CARD32 MemTrasMemTimingCntl[16]    = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

    float MemTcas[8]  = {0, 1, 2, 3, 0, 1.5, 2.5, 0};
    float MemTcas2[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    float MemTrbs[8]  = {1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5};

    float mem_bw, peak_disp_bw;
    float min_mem_eff = 0.8;
    float sclk_eff, sclk_delay;
    float mc_latency_mclk, mc_latency_sclk, cur_latency_mclk, cur_latency_sclk;
    float disp_latency, disp_latency_overhead, disp_drain_rate, disp_drain_rate2;
    float pix_clk, pix_clk2; /* in MHz */
    int cur_size = 16;       /* in octawords */
    int critical_point, critical_point2;
    int stop_req, max_stop_req;
    float read_return_rate, time_disp1_drop_priority;

    if (pRADEONEnt->pSecondaryScrn) {
	if (info->IsSecondary) return;
	info2 = RADEONPTR(pRADEONEnt->pSecondaryScrn);
    }  else if (info->Clone) info2 = info;

    /*
     * Determine if there is enough bandwidth for current display mode
     */
    mem_bw = info->mclk * (info->RamWidth / 8) * (info->IsDDR ? 2 : 1);

    mode1 = info->CurrentLayout.mode;
    if (info->Clone)
	mode2 = info->CurCloneMode;
    else if ((pRADEONEnt->HasSecondary) && info2)
	mode2 = info2->CurrentLayout.mode;
    else
	mode2 = NULL;

    pix_clk = mode1->Clock/1000.0;
    if (mode2)
	pix_clk2 = mode2->Clock/1000.0;
    else
	pix_clk2 = 0;

    peak_disp_bw = (pix_clk * info->CurrentLayout.pixel_bytes);
    if (info2)
	peak_disp_bw +=	(pix_clk2 * info2->CurrentLayout.pixel_bytes);

    if (peak_disp_bw >= mem_bw * min_mem_eff) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "You may not have enough display bandwidth for current mode\n"
		   "If you have flickering problem, try to lower resolution, refresh rate, or color depth\n");
    }

    /*  CRTC1
        Set GRPH_BUFFER_CNTL register using h/w defined optimal values.
	GRPH_STOP_REQ <= MIN[ 0x7C, (CRTC_H_DISP + 1) * (bit depth) / 0x10 ]
    */
    stop_req = mode1->HDisplay * info->CurrentLayout.pixel_bytes / 16;

    /* setup Max GRPH_STOP_REQ default value */
    if ((info->ChipFamily == CHIP_FAMILY_RV100) ||
	(info->ChipFamily == CHIP_FAMILY_RV200) ||
	(info->ChipFamily == CHIP_FAMILY_RV250) ||
	(info->ChipFamily == CHIP_FAMILY_RV280) ||
	(info->ChipFamily == CHIP_FAMILY_RS100) ||
	(info->ChipFamily == CHIP_FAMILY_RS200) ||
	(info->ChipFamily == CHIP_FAMILY_RS300))
	max_stop_req = 0x5c;
    else
	max_stop_req  = 0x7c;
    if (stop_req > max_stop_req)
	stop_req = max_stop_req;

    /*  Get values from the EXT_MEM_CNTL register...converting its contents. */
    temp = INREG(RADEON_MEM_TIMING_CNTL);
    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	mem_trcd      = MemTrcdExtMemCntl[(temp & 0x0c) >> 2];
	mem_trp       = MemTrpExtMemCntl[ (temp & 0x03) >> 0];
	mem_tras      = MemTrasExtMemCntl[(temp & 0x70) >> 4];
    } else { /* RV200 and later */
	mem_trcd      = MemTrcdMemTimingCntl[(temp & 0x07) >> 0];
	mem_trp       = MemTrpMemTimingCntl[ (temp & 0x700) >> 8];
	mem_tras      = MemTrasMemTimingCntl[(temp & 0xf000) >> 12];
    }

    /* Get values from the MEM_SDRAM_MODE_REG register...converting its */
    temp = INREG(RADEON_MEM_SDRAM_MODE_REG);
    data = (temp & (7<<20)) >> 20;
    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	mem_tcas = MemTcas [data];
    } else {
	mem_tcas = MemTcas2 [data];
    }

    if ((info->ChipFamily == CHIP_FAMILY_R300) ||
	(info->ChipFamily == CHIP_FAMILY_R350) ||
	(info->ChipFamily == CHIP_FAMILY_RV350)) {

	/* on the R300, Tcas is included in Trbs.
	*/
	temp = INREG(RADEON_MEM_CNTL);
	data = (R300_MEM_NUM_CHANNELS_MASK & temp);
	if (data == 2) {
	    if (R300_MEM_USE_CD_CH_ONLY & temp) {
		temp  = INREG(R300_MC_IND_INDEX);
		temp &= ~R300_MC_IND_ADDR_MASK;
		temp |= R300_MC_READ_CNTL_CD_mcind;
		OUTREG(R300_MC_IND_INDEX, temp);
		temp  = INREG(R300_MC_IND_DATA);
		data = (R300_MEM_RBS_POSITION_C_MASK & temp);
	    } else {
		temp = INREG(R300_MC_READ_CNTL_AB);
		data = (R300_MEM_RBS_POSITION_A_MASK & temp);
	    }
	} else {
	    temp = INREG(R300_MC_READ_CNTL_AB);
	    data = (R300_MEM_RBS_POSITION_A_MASK & temp);
	}

	mem_trbs = MemTrbs[data];
	mem_tcas += mem_trbs;
    }

    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	/* DDR64 SCLK_EFF = SCLK for analysis */
	sclk_eff = info->sclk;
    } else {
#ifdef XF86DRI
	if (info->directRenderingEnabled)
	    sclk_eff = info->sclk - (info->agpMode * 50.0 / 3.0);
	else
#endif
	    sclk_eff = info->sclk;
    }

    /* Find the memory controller latency for the display client.
    */
    if ((info->ChipFamily == CHIP_FAMILY_R300) ||
	(info->ChipFamily == CHIP_FAMILY_R350) ||
	(info->ChipFamily == CHIP_FAMILY_RV350)) {
	/*not enough for R350 ???*/
	/*
	if (!mode2) sclk_delay = 150;
	else {
	    if (info->RamWidth == 256) sclk_delay = 87;
	    else sclk_delay = 97;
	}
	*/
	sclk_delay = 250;
    } else {
	if ((info->ChipFamily == CHIP_FAMILY_RV100) ||
	    info->IsIGP) {
	    if (info->IsDDR) sclk_delay = 41;
	    else sclk_delay = 33;
	} else {
	    if (info->RamWidth == 128) sclk_delay = 57;
	    else sclk_delay = 41;
	}
    }

    mc_latency_sclk = sclk_delay / sclk_eff;

    if (info->IsDDR) {
	if (info->RamWidth == 32) {
	    k1 = 40;
	    c  = 3;
	} else {
	    k1 = 20;
	    c  = 1;
	}
    } else {
	k1 = 40;
	c  = 3;
    }
    mc_latency_mclk = ((2.0*mem_trcd + mem_tcas*c + 4.0*mem_tras + 4.0*mem_trp + k1) /
		       info->mclk) + (4.0 / sclk_eff);

    /*
      HW cursor time assuming worst case of full size colour cursor.
    */
    cur_latency_mclk = (mem_trp + MAX(mem_tras, (mem_trcd + 2*(cur_size - (info->IsDDR+1))))) / info->mclk;
    cur_latency_sclk = cur_size / sclk_eff;

    /*
      Find the total latency for the display data.
    */
    disp_latency_overhead = 8.0 / info->sclk;
    mc_latency_mclk = mc_latency_mclk + disp_latency_overhead + cur_latency_mclk;
    mc_latency_sclk = mc_latency_sclk + disp_latency_overhead + cur_latency_sclk;
    disp_latency = MAX(mc_latency_mclk, mc_latency_sclk);

    /*
      Find the drain rate of the display buffer.
    */
    disp_drain_rate = pix_clk / (16.0/info->CurrentLayout.pixel_bytes);
    if (info2)
	disp_drain_rate2 = pix_clk2 / (16.0/info2->CurrentLayout.pixel_bytes);
    else
	disp_drain_rate2 = 0;

    /*
      Find the critical point of the display buffer.
    */
    critical_point= (CARD32)(disp_drain_rate * disp_latency + 0.5);

    /* ???? */
    /*
    temp = (info->SavedReg.grph_buffer_cntl & RADEON_GRPH_CRITICAL_POINT_MASK) >> RADEON_GRPH_CRITICAL_POINT_SHIFT;
    if (critical_point < temp) critical_point = temp;
    */
    if (info->DispPriority == 2) {
	if (mode2) {
	    /*??some R300 cards have problem with this set to 0, when CRTC2 is enabled.*/
	    if (info->ChipFamily == CHIP_FAMILY_R300)
		critical_point += 0x10;
	    else
		critical_point = 0;
	}
	else
	    critical_point = 0;
    }

    /*
      The critical point should never be above max_stop_req-4.  Setting
      GRPH_CRITICAL_CNTL = 0 will thus force high priority all the time.
    */
    if (max_stop_req - critical_point < 4) critical_point = 0;

    temp = info->SavedReg.grph_buffer_cntl;
    temp &= ~(RADEON_GRPH_STOP_REQ_MASK);
    temp |= (stop_req << RADEON_GRPH_STOP_REQ_SHIFT);
    temp &= ~(RADEON_GRPH_START_REQ_MASK);
    if ((info->ChipFamily == CHIP_FAMILY_R350) &&
	(stop_req > 0x15)) {
	stop_req -= 0x10;
    }
    temp |= (stop_req << RADEON_GRPH_START_REQ_SHIFT);

    temp |= RADEON_GRPH_BUFFER_SIZE;
    temp &= ~(RADEON_GRPH_CRITICAL_CNTL   |
	      RADEON_GRPH_CRITICAL_AT_SOF |
	      RADEON_GRPH_STOP_CNTL);
    /*
      Write the result into the register.
    */
    OUTREG(RADEON_GRPH_BUFFER_CNTL, ((temp & ~RADEON_GRPH_CRITICAL_POINT_MASK) |
				     (critical_point << RADEON_GRPH_CRITICAL_POINT_SHIFT)));

    RADEONTRACE(("GRPH_BUFFER_CNTL from %x to %x\n",
	       info->SavedReg.grph_buffer_cntl, INREG(RADEON_GRPH_BUFFER_CNTL)));

    if (mode2) {
	stop_req = mode2->HDisplay * info2->CurrentLayout.pixel_bytes / 16;

	if (stop_req > max_stop_req) stop_req = max_stop_req;

	temp = info->SavedReg.grph2_buffer_cntl;
	temp &= ~(RADEON_GRPH_STOP_REQ_MASK);
	temp |= (stop_req << RADEON_GRPH_STOP_REQ_SHIFT);
	temp &= ~(RADEON_GRPH_START_REQ_MASK);
	if ((info->ChipFamily == CHIP_FAMILY_R350) &&
	    (stop_req > 0x15)) {
	    stop_req -= 0x10;
	}
	temp |= (stop_req << RADEON_GRPH_START_REQ_SHIFT);
	temp |= RADEON_GRPH_BUFFER_SIZE;
	temp &= ~(RADEON_GRPH_CRITICAL_CNTL   |
		  RADEON_GRPH_CRITICAL_AT_SOF |
		  RADEON_GRPH_STOP_CNTL);

	if ((info->ChipFamily == CHIP_FAMILY_RS100) ||
	    (info->ChipFamily == CHIP_FAMILY_RS200))
	    critical_point2 = 0;
	else {
	    read_return_rate = MIN(info->sclk, info->mclk*(info->RamWidth*(info->IsDDR+1)/128));
	    time_disp1_drop_priority = critical_point / (read_return_rate - disp_drain_rate);

	    critical_point2 = (CARD32)((disp_latency + time_disp1_drop_priority +
					disp_latency) * disp_drain_rate2 + 0.5);

	    if (info->DispPriority == 2) {
		if (info->ChipFamily == CHIP_FAMILY_R300)
		    critical_point2 += 0x10;
		else
		    critical_point2 = 0;
	    }

	    if (max_stop_req - critical_point2 < 4) critical_point2 = 0;

	}

	OUTREG(RADEON_GRPH2_BUFFER_CNTL, ((temp & ~RADEON_GRPH_CRITICAL_POINT_MASK) |
					  (critical_point2 << RADEON_GRPH_CRITICAL_POINT_SHIFT)));

	RADEONTRACE(("GRPH2_BUFFER_CNTL from %x to %x\n",
		     info->SavedReg.grph2_buffer_cntl, INREG(RADEON_GRPH2_BUFFER_CNTL)));
    }
}

/* Define CRTC registers for requested video mode */
static Bool RADEONInitCrtcRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save,
				  DisplayModePtr mode, RADEONInfoPtr info)
{
    unsigned char *RADEONMMIO = info->MMIO;

    int  format;
    int  hsync_start;
    int  hsync_wid;
    int  hsync_fudge;
    int  vsync_wid;
    int  hsync_fudge_default[] = { 0x00, 0x12, 0x09, 0x09, 0x06, 0x05 };
    int  hsync_fudge_fp[]      = { 0x02, 0x02, 0x00, 0x00, 0x05, 0x05 };

    switch (info->CurrentLayout.pixel_code) {
    case 4:  format = 1; break;
    case 8:  format = 2; break;
    case 15: format = 3; break;      /*  555 */
    case 16: format = 4; break;      /*  565 */
    case 24: format = 5; break;      /*  RGB */
    case 32: format = 6; break;      /* xRGB */
    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Unsupported pixel depth (%d)\n",
		   info->CurrentLayout.bitsPerPixel);
	return FALSE;
    }

    if ((info->DisplayType == MT_DFP) ||
	(info->DisplayType == MT_LCD)) {
	hsync_fudge = hsync_fudge_fp[format-1];
	if (mode->Flags & RADEON_USE_RMX) {
#if 0
	    mode->CrtcHDisplay   = info->PanelXRes;
	    mode->CrtcVDisplay   = info->PanelYRes;
#endif
	    mode->CrtcHTotal     = mode->CrtcHDisplay + info->HBlank;
	    mode->CrtcHSyncStart = mode->CrtcHDisplay + info->HOverPlus;
	    mode->CrtcHSyncEnd   = mode->CrtcHSyncStart + info->HSyncWidth;
	    mode->CrtcVTotal     = mode->CrtcVDisplay + info->VBlank;
	    mode->CrtcVSyncStart = mode->CrtcVDisplay + info->VOverPlus;
	    mode->CrtcVSyncEnd   = mode->CrtcVSyncStart + info->VSyncWidth;
	    mode->Clock          = info->DotClock;
	    mode->Flags          = info->Flags | RADEON_USE_RMX;
	}
    } else {
	hsync_fudge = hsync_fudge_default[format-1];
    }

    save->crtc_gen_cntl = (RADEON_CRTC_EXT_DISP_EN
			   | RADEON_CRTC_EN
			   | (format << 8)
			   | ((mode->Flags & V_DBLSCAN)
			      ? RADEON_CRTC_DBL_SCAN_EN
			      : 0)
			   | ((mode->Flags & V_CSYNC)
			      ? RADEON_CRTC_CSYNC_EN
			      : 0)
			   | ((mode->Flags & V_INTERLACE)
			      ? RADEON_CRTC_INTERLACE_EN
			      : 0));

    if ((info->DisplayType == MT_DFP) ||
	(info->DisplayType == MT_LCD)) {
	save->crtc_ext_cntl = RADEON_VGA_ATI_LINEAR | RADEON_XCRT_CNT_EN;
	save->crtc_gen_cntl &= ~(RADEON_CRTC_DBL_SCAN_EN |
				 RADEON_CRTC_CSYNC_EN |
				 RADEON_CRTC_INTERLACE_EN);
    } else {
	save->crtc_ext_cntl = (RADEON_VGA_ATI_LINEAR |
			       RADEON_XCRT_CNT_EN |
			       RADEON_CRTC_CRT_ON);
    }

    save->dac_cntl = (RADEON_DAC_MASK_ALL
		      | RADEON_DAC_VGA_ADR_EN
		      | (info->dac6bits ? 0 : RADEON_DAC_8BIT_EN));

    save->crtc_h_total_disp = ((((mode->CrtcHTotal / 8) - 1) & 0x3ff)
			       | ((((mode->CrtcHDisplay / 8) - 1) & 0x1ff)
				  << 16));

    hsync_wid = (mode->CrtcHSyncEnd - mode->CrtcHSyncStart) / 8;
    if (!hsync_wid) hsync_wid = 1;
    hsync_start = mode->CrtcHSyncStart - 8 + hsync_fudge;

    save->crtc_h_sync_strt_wid = ((hsync_start & 0x1fff)
				  | ((hsync_wid & 0x3f) << 16)
				  | ((mode->Flags & V_NHSYNC)
				     ? RADEON_CRTC_H_SYNC_POL
				     : 0));

#if 1
				/* This works for double scan mode. */
    save->crtc_v_total_disp = (((mode->CrtcVTotal - 1) & 0xffff)
			       | ((mode->CrtcVDisplay - 1) << 16));
#else
				/* This is what cce/nbmode.c example code
				 * does -- is this correct?
				 */
    save->crtc_v_total_disp = (((mode->CrtcVTotal - 1) & 0xffff)
			       | ((mode->CrtcVDisplay
				   * ((mode->Flags & V_DBLSCAN) ? 2 : 1) - 1)
				  << 16));
#endif

    vsync_wid = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;
    if (!vsync_wid) vsync_wid = 1;

    save->crtc_v_sync_strt_wid = (((mode->CrtcVSyncStart - 1) & 0xfff)
				  | ((vsync_wid & 0x1f) << 16)
				  | ((mode->Flags & V_NVSYNC)
				     ? RADEON_CRTC_V_SYNC_POL
				     : 0));

    save->crtc_offset      = 0;
    save->crtc_offset_cntl = INREG(RADEON_CRTC_OFFSET_CNTL);

    save->crtc_pitch  = (((pScrn->displayWidth * pScrn->bitsPerPixel) +
			  ((pScrn->bitsPerPixel * 8) -1)) /
			 (pScrn->bitsPerPixel * 8));
    save->crtc_pitch |= save->crtc_pitch << 16;

    /* Some versions of BIOS setup CRTC_MORE_CNTL for a DFP, if we
       have a CRT here, it should be cleared to avoild a blank screen.
    */
    if (info->DisplayType == MT_CRT)
	save->crtc_more_cntl = (info->SavedReg.crtc_more_cntl &
				~(RADEON_CRTC_H_CUTOFF_ACTIVE_EN |
				  RADEON_CRTC_V_CUTOFF_ACTIVE_EN));
    else
	save->crtc_more_cntl = info->SavedReg.crtc_more_cntl;

    save->surface_cntl = 0;
    save->disp_merge_cntl = info->SavedReg.disp_merge_cntl;
    save->disp_merge_cntl &= ~RADEON_DISP_RGB_OFFSET_EN;

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* Alhought we current onlu use aperture 0, also setting aperture 1 should not harm -ReneR */
    switch (pScrn->bitsPerPixel) {
    case 16:
	save->surface_cntl |= RADEON_NONSURF_AP0_SWP_16BPP;
	save->surface_cntl |= RADEON_NONSURF_AP1_SWP_16BPP;
	break;

    case 32:
	save->surface_cntl |= RADEON_NONSURF_AP0_SWP_32BPP;
	save->surface_cntl |= RADEON_NONSURF_AP1_SWP_32BPP;
	break;
    }
#endif

    RADEONTRACE(("Pitch = %d bytes (virtualX = %d, displayWidth = %d)\n",
		 save->crtc_pitch, pScrn->virtualX,
		 info->CurrentLayout.displayWidth));
    return TRUE;
}

/* Define CRTC2 registers for requested video mode */
static Bool RADEONInitCrtc2Registers(ScrnInfoPtr pScrn, RADEONSavePtr save,
				     DisplayModePtr mode, RADEONInfoPtr info)
{
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);

    int  format;
    int  hsync_start;
    int  hsync_wid;
    int  hsync_fudge;
    int  vsync_wid;
    int  hsync_fudge_default[] = { 0x00, 0x12, 0x09, 0x09, 0x06, 0x05 };

    switch (info->CurrentLayout.pixel_code) {
    case 4:  format = 1; break;
    case 8:  format = 2; break;
    case 15: format = 3; break;      /*  555 */
    case 16: format = 4; break;      /*  565 */
    case 24: format = 5; break;      /*  RGB */
    case 32: format = 6; break;      /* xRGB */
    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Unsupported pixel depth (%d)\n",
		   info->CurrentLayout.bitsPerPixel);
	return FALSE;
    }

    hsync_fudge = hsync_fudge_default[format-1];

    save->crtc2_gen_cntl = (RADEON_CRTC2_EN
			    | RADEON_CRTC2_CRT2_ON
			    | (format << 8)
			    | ((mode->Flags & V_DBLSCAN)
			       ? RADEON_CRTC2_DBL_SCAN_EN
			       : 0)
			    | ((mode->Flags & V_CSYNC)
			       ? RADEON_CRTC2_CSYNC_EN
			       : 0)
			    | ((mode->Flags & V_INTERLACE)
			       ? RADEON_CRTC2_INTERLACE_EN
			       : 0));

    /* Turn CRT on in case the first head is a DFP */
    save->crtc_ext_cntl |= RADEON_CRTC_CRT_ON;
    save->dac2_cntl = info->SavedReg.dac2_cntl;
    /* always let TVDAC drive CRT2, we don't support tvout yet */
    save->dac2_cntl |= RADEON_DAC2_DAC2_CLK_SEL;
    save->disp_output_cntl = info->SavedReg.disp_output_cntl;
    if (info->ChipFamily == CHIP_FAMILY_R200 ||
	info->ChipFamily == CHIP_FAMILY_R300 ||
	info->ChipFamily == CHIP_FAMILY_R350 ||
	info->ChipFamily == CHIP_FAMILY_RV350) {
	save->disp_output_cntl &= ~(RADEON_DISP_DAC_SOURCE_MASK |
				    RADEON_DISP_DAC2_SOURCE_MASK);
	if (pRADEONEnt->MonType1 != MT_CRT) {
	    save->disp_output_cntl |= (RADEON_DISP_DAC_SOURCE_CRTC2 |
				       RADEON_DISP_DAC2_SOURCE_CRTC2);
	} else {
	    if (pRADEONEnt->ReversedDAC) {
		save->disp_output_cntl |= RADEON_DISP_DAC2_SOURCE_CRTC2;
	    } else {
		save->disp_output_cntl |= RADEON_DISP_DAC_SOURCE_CRTC2;
	    }
	}
    } else {
	save->disp_hw_debug = info->SavedReg.disp_hw_debug;
	/* Turn on 2nd CRT */
	if (pRADEONEnt->MonType1 != MT_CRT) {
	    /* This is for some sample boards with the VGA port
	       connected to the TVDAC, but BIOS doesn't reflect this.
	       Here we configure both DACs to use CRTC2.
	       Not sure if this happens in any retail board.
	    */
	    save->disp_hw_debug &= ~RADEON_CRT2_DISP1_SEL;
	    save->dac2_cntl |= RADEON_DAC2_DAC_CLK_SEL;
	} else {
	    if (pRADEONEnt->ReversedDAC) {
		save->disp_hw_debug &= ~RADEON_CRT2_DISP1_SEL;
		save->dac2_cntl &= ~RADEON_DAC2_DAC_CLK_SEL;
	    } else {
		save->disp_hw_debug |= RADEON_CRT2_DISP1_SEL;
		save->dac2_cntl |= RADEON_DAC2_DAC_CLK_SEL;
	    }
	}
    }

    save->crtc2_h_total_disp =
	((((mode->CrtcHTotal / 8) - 1) & 0x3ff)
	 | ((((mode->CrtcHDisplay / 8) - 1) & 0x1ff) << 16));

    hsync_wid = (mode->CrtcHSyncEnd - mode->CrtcHSyncStart) / 8;
    if (!hsync_wid) hsync_wid = 1;
    hsync_start = mode->CrtcHSyncStart - 8 + hsync_fudge;

    save->crtc2_h_sync_strt_wid = ((hsync_start & 0x1fff)
				   | ((hsync_wid & 0x3f) << 16)
				   | ((mode->Flags & V_NHSYNC)
				      ? RADEON_CRTC_H_SYNC_POL
				      : 0));

#if 1
				/* This works for double scan mode. */
    save->crtc2_v_total_disp = (((mode->CrtcVTotal - 1) & 0xffff)
				| ((mode->CrtcVDisplay - 1) << 16));
#else
				/* This is what cce/nbmode.c example code
				 * does -- is this correct?
				 */
    save->crtc2_v_total_disp = (((mode->CrtcVTotal - 1) & 0xffff)
				| ((mode->CrtcVDisplay
				    * ((mode->Flags & V_DBLSCAN) ? 2 : 1) - 1)
				   << 16));
#endif

    vsync_wid = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;
    if (!vsync_wid) vsync_wid = 1;

    save->crtc2_v_sync_strt_wid = (((mode->CrtcVSyncStart - 1) & 0xfff)
				   | ((vsync_wid & 0x1f) << 16)
				   | ((mode->Flags & V_NVSYNC)
				      ? RADEON_CRTC2_V_SYNC_POL
				      : 0));

    save->crtc2_offset      = 0;
    save->crtc2_offset_cntl = INREG(RADEON_CRTC2_OFFSET_CNTL);

    save->crtc2_pitch  = (((pScrn->displayWidth * pScrn->bitsPerPixel) +
			   ((pScrn->bitsPerPixel * 8) -1)) /
			  (pScrn->bitsPerPixel * 8));
    save->crtc2_pitch |= save->crtc2_pitch << 16;
    save->disp2_merge_cntl = info->SavedReg.disp2_merge_cntl;
    save->disp2_merge_cntl &= ~(RADEON_DISP2_RGB_OFFSET_EN);

    if ((info->DisplayType == MT_DFP && info->IsSecondary) ||
	info->CloneType == MT_DFP) {
	save->crtc2_gen_cntl      = (RADEON_CRTC2_EN | (format << 8));
	save->fp2_h_sync_strt_wid = save->crtc2_h_sync_strt_wid;
	save->fp2_v_sync_strt_wid = save->crtc2_v_sync_strt_wid;
	save->fp2_gen_cntl        = (RADEON_FP2_PANEL_FORMAT |
				     RADEON_FP2_ON);
	if (info->ChipFamily >= CHIP_FAMILY_R200) {
	    save->fp2_gen_cntl |= RADEON_FP2_DV0_EN;
	}

	if (info->ChipFamily == CHIP_FAMILY_R200 ||
	    info->ChipFamily == CHIP_FAMILY_R300 ||
	    info->ChipFamily == CHIP_FAMILY_R350 ||
	    info->ChipFamily == CHIP_FAMILY_RV350) {
	    save->fp2_gen_cntl &= ~RADEON_FP2_SOURCE_SEL_MASK;
	    save->fp2_gen_cntl |= RADEON_FP2_SOURCE_SEL_CRTC2;
	} else {
	    save->fp2_gen_cntl &= ~RADEON_FP2_SRC_SEL_MASK;
	    save->fp2_gen_cntl |= RADEON_FP2_SRC_SEL_CRTC2;
	}

	if (pScrn->rgbBits == 8)
	    save->fp2_gen_cntl |= RADEON_FP2_PANEL_FORMAT; /* 24 bit format */
	else
	    save->fp2_gen_cntl &= ~RADEON_FP2_PANEL_FORMAT;/* 18 bit format */

	/* FIXME: When there are two DFPs, the 2nd DFP is driven by the
	 *        external TMDS transmitter.  It may have a problem at
	 *        high dot clock for certain panels.
	 */

	/* If BIOS has not turned it on, we'll keep it on so that we'll
	 * have a valid VGA screen even after X quits or VT is switched
	 * to the console mode.
	 */
	info->SavedReg.fp2_gen_cntl = RADEON_FP2_ON;
    }

    RADEONTRACE(("Pitch = %d bytes (virtualX = %d, displayWidth = %d)\n",
		 save->crtc2_pitch, pScrn->virtualX,
		 info->CurrentLayout.displayWidth));

    return TRUE;
}

/* Define CRTC registers for requested video mode */
static void RADEONInitFPRegisters(ScrnInfoPtr pScrn, RADEONSavePtr orig,
				  RADEONSavePtr save, DisplayModePtr mode,
				  RADEONInfoPtr info)
{
    int    xres = mode->HDisplay;
    int    yres = mode->VDisplay;
    float  Hratio, Vratio;

    /* If the FP registers have been initialized before for a panel,
     * but the primary port is a CRT, we need to reinitialize
     * FP registers in order for CRT to work properly
     */

    if ((info->DisplayType != MT_DFP) && (info->DisplayType != MT_LCD)) {
        save->fp_crtc_h_total_disp = orig->fp_crtc_h_total_disp;
        save->fp_crtc_v_total_disp = orig->fp_crtc_v_total_disp;
        save->fp_gen_cntl          = 0;
        save->fp_h_sync_strt_wid   = orig->fp_h_sync_strt_wid;
        save->fp_horz_stretch      = 0;
        save->fp_v_sync_strt_wid   = orig->fp_v_sync_strt_wid;
        save->fp_vert_stretch      = 0;
        save->lvds_gen_cntl        = orig->lvds_gen_cntl;
        save->lvds_pll_cntl        = orig->lvds_pll_cntl;
        save->tmds_pll_cntl        = orig->tmds_pll_cntl;
        save->tmds_transmitter_cntl= orig->tmds_transmitter_cntl;

        save->lvds_gen_cntl |= ( RADEON_LVDS_DISPLAY_DIS | (1 << 23));
        save->lvds_gen_cntl &= ~(RADEON_LVDS_BLON | RADEON_LVDS_ON);
        save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);

        return;
    }

    if (info->PanelXRes == 0 || info->PanelYRes == 0) {
	Hratio = 1.0;
	Vratio = 1.0;
    } else {
	if (xres > info->PanelXRes) xres = info->PanelXRes;
	if (yres > info->PanelYRes) yres = info->PanelYRes;

	Hratio = (float)xres/(float)info->PanelXRes;
	Vratio = (float)yres/(float)info->PanelYRes;
    }

    if (Hratio == 1.0 || !(mode->Flags & RADEON_USE_RMX)) {
	save->fp_horz_stretch = orig->fp_horz_stretch;
	save->fp_horz_stretch &= ~(RADEON_HORZ_STRETCH_BLEND |
				   RADEON_HORZ_STRETCH_ENABLE);
	save->fp_horz_stretch &= ~(RADEON_HORZ_AUTO_RATIO |
				   RADEON_HORZ_PANEL_SIZE);
	save->fp_horz_stretch |= ((xres/8-1)<<16);

    } else {
	save->fp_horz_stretch =
	    ((((unsigned long)(Hratio * RADEON_HORZ_STRETCH_RATIO_MAX +
			       0.5)) & RADEON_HORZ_STRETCH_RATIO_MASK)) |
	    (orig->fp_horz_stretch & (RADEON_HORZ_PANEL_SIZE |
				      RADEON_HORZ_FP_LOOP_STRETCH |
				      RADEON_HORZ_AUTO_RATIO_INC));
	save->fp_horz_stretch |= (RADEON_HORZ_STRETCH_BLEND |
				  RADEON_HORZ_STRETCH_ENABLE);

	save->fp_horz_stretch &= ~(RADEON_HORZ_AUTO_RATIO |
				   RADEON_HORZ_PANEL_SIZE);
	save->fp_horz_stretch |= ((info->PanelXRes / 8 - 1) << 16);

    }

    if (Vratio == 1.0 || !(mode->Flags & RADEON_USE_RMX)) {
	save->fp_vert_stretch = orig->fp_vert_stretch;
	save->fp_vert_stretch &= ~(RADEON_VERT_STRETCH_ENABLE|
				   RADEON_VERT_STRETCH_BLEND);
	save->fp_vert_stretch &= ~(RADEON_VERT_AUTO_RATIO_EN |
				   RADEON_VERT_PANEL_SIZE);
	save->fp_vert_stretch |= ((yres-1) << 12);
    } else {
	save->fp_vert_stretch =
	    (((((unsigned long)(Vratio * RADEON_VERT_STRETCH_RATIO_MAX +
				0.5)) & RADEON_VERT_STRETCH_RATIO_MASK)) |
	     (orig->fp_vert_stretch & (RADEON_VERT_PANEL_SIZE |
				       RADEON_VERT_STRETCH_RESERVED)));
	save->fp_vert_stretch |= (RADEON_VERT_STRETCH_ENABLE |
				  RADEON_VERT_STRETCH_BLEND);

	save->fp_vert_stretch &= ~(RADEON_VERT_AUTO_RATIO_EN |
				   RADEON_VERT_PANEL_SIZE);
	save->fp_vert_stretch |= ((info->PanelYRes-1) << 12);

    }

    save->fp_gen_cntl = (orig->fp_gen_cntl & (CARD32)
			 ~(RADEON_FP_SEL_CRTC2 |
			   RADEON_FP_RMX_HVSYNC_CONTROL_EN |
			   RADEON_FP_DFP_SYNC_SEL |
			   RADEON_FP_CRT_SYNC_SEL |
			   RADEON_FP_CRTC_LOCK_8DOT |
			   RADEON_FP_USE_SHADOW_EN |
			   RADEON_FP_CRTC_USE_SHADOW_VEND |
			   RADEON_FP_CRT_SYNC_ALT));
    save->fp_gen_cntl |= (RADEON_FP_CRTC_DONT_SHADOW_VPAR |
			  RADEON_FP_CRTC_DONT_SHADOW_HEND );

    if (pScrn->rgbBits == 8)
        save->fp_gen_cntl |= RADEON_FP_PANEL_FORMAT;  /* 24 bit format */
    else
        save->fp_gen_cntl &= ~RADEON_FP_PANEL_FORMAT;/* 18 bit format */

    save->lvds_gen_cntl = orig->lvds_gen_cntl;
    save->lvds_pll_cntl = orig->lvds_pll_cntl;

    info->PanelOff = FALSE;
    /* This option is used to force the ONLY DEVICE in XFConfig to use
     * CRT port, instead of default DVI port.
     */
    if (xf86ReturnOptValBool(info->Options, OPTION_PANEL_OFF, FALSE)) {
	info->PanelOff = TRUE;
    }

    save->tmds_pll_cntl = orig->tmds_pll_cntl;
    save->tmds_transmitter_cntl= orig->tmds_transmitter_cntl;
    if (info->PanelOff && info->Clone) {
	info->OverlayOnCRTC2 = TRUE;
	if (info->DisplayType == MT_LCD) {
	    /* Turning off LVDS_ON seems to make panel white blooming.
	     * For now we just turn off display data ???
	     */
	    save->lvds_gen_cntl |= (RADEON_LVDS_DISPLAY_DIS);
	    save->lvds_gen_cntl &= ~(RADEON_LVDS_BLON | RADEON_LVDS_ON);

	} else if (info->DisplayType == MT_DFP)
	    save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
    } else {
	if (info->DisplayType == MT_LCD) {
	    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

	    /* BIOS will use this setting to reset displays upon lid close/open.
	     * Here we let BIOS controls LCD, but the driver will control the external CRT.
	     */
	    if (info->Clone || pRADEONEnt->HasSecondary)
		save->bios_5_scratch = 0x01020201;
	    else
		save->bios_5_scratch = orig->bios_5_scratch;

	    save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
	    save->fp_gen_cntl   &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);

	} else if (info->DisplayType == MT_DFP) {
	    int i;
	    CARD32 tmp = orig->tmds_pll_cntl & 0xfffff;
	    for (i=0; i<4; i++) {
		if (info->tmds_pll[i].freq == 0) break;
		if (save->dot_clock_freq < info->tmds_pll[i].freq) {
		    tmp = info->tmds_pll[i].value ;
		    break;
		}
	    }
	    if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		(info->ChipFamily == CHIP_FAMILY_R350) ||
		(info->ChipFamily == CHIP_FAMILY_RV350) ||
		(info->ChipFamily == CHIP_FAMILY_RV280)) {
		if (tmp & 0xfff00000)
		    save->tmds_pll_cntl = tmp;
		else
		    save->tmds_pll_cntl = (orig->tmds_pll_cntl & 0xfff00000) | tmp;
	    } else save->tmds_pll_cntl = tmp;

	    RADEONTRACE(("TMDS_PLL from %x to %x\n",
			 orig->tmds_pll_cntl,
			 save->tmds_pll_cntl));

            save->tmds_transmitter_cntl &= ~(RADEON_TMDS_TRANSMITTER_PLLRST);
            if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		(info->ChipFamily == CHIP_FAMILY_R350) ||
		(info->ChipFamily == CHIP_FAMILY_RV350) ||
		(info->ChipFamily == CHIP_FAMILY_R200) || !info->HasCRTC2)
		save->tmds_transmitter_cntl &= ~(RADEON_TMDS_TRANSMITTER_PLLEN);
            else /* weird, RV chips got this bit reversed? */
                save->tmds_transmitter_cntl |= (RADEON_TMDS_TRANSMITTER_PLLEN);

	    save->fp_gen_cntl   |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
        }
    }

    save->fp_crtc_h_total_disp = save->crtc_h_total_disp;
    save->fp_crtc_v_total_disp = save->crtc_v_total_disp;
    save->fp_h_sync_strt_wid   = save->crtc_h_sync_strt_wid;
    save->fp_v_sync_strt_wid   = save->crtc_v_sync_strt_wid;
}

/* Define PLL registers for requested video mode */
static void RADEONInitPLLRegisters(RADEONSavePtr save, RADEONPLLPtr pll,
				   double dot_clock)
{
    unsigned long  freq = dot_clock * 100;

    struct {
	int divider;
	int bitvalue;
    } *post_div, post_divs[]   = {
				/* From RAGE 128 VR/RAGE 128 GL Register
				 * Reference Manual (Technical Reference
				 * Manual P/N RRG-G04100-C Rev. 0.04), page
				 * 3-17 (PLL_DIV_[3:0]).
				 */
	{  1, 0 },              /* VCLK_SRC                 */
	{  2, 1 },              /* VCLK_SRC/2               */
	{  4, 2 },              /* VCLK_SRC/4               */
	{  8, 3 },              /* VCLK_SRC/8               */
	{  3, 4 },              /* VCLK_SRC/3               */
	{ 16, 5 },              /* VCLK_SRC/16              */
	{  6, 6 },              /* VCLK_SRC/6               */
	{ 12, 7 },              /* VCLK_SRC/12              */
	{  0, 0 }
    };

    if (freq > pll->max_pll_freq)      freq = pll->max_pll_freq;
    if (freq * 12 < pll->min_pll_freq) freq = pll->min_pll_freq / 12;

    for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
	save->pll_output_freq = post_div->divider * freq;

	if (save->pll_output_freq >= pll->min_pll_freq
	    && save->pll_output_freq <= pll->max_pll_freq) break;
    }

    if (!post_div->divider) {
	save->pll_output_freq = freq;
	post_div = &post_divs[0];
    }

    save->dot_clock_freq = freq;
    save->feedback_div   = RADEONDiv(pll->reference_div
				     * save->pll_output_freq,
				     pll->reference_freq);
    save->post_div       = post_div->divider;

    RADEONTRACE(("dc=%d, of=%d, fd=%d, pd=%d\n",
	       save->dot_clock_freq,
	       save->pll_output_freq,
	       save->feedback_div,
	       save->post_div));

    save->ppll_ref_div   = pll->reference_div;
    save->ppll_div_3     = (save->feedback_div | (post_div->bitvalue << 16));
    save->htotal_cntl    = 0;
}

/* Define PLL2 registers for requested video mode */
static void RADEONInitPLL2Registers(RADEONSavePtr save, RADEONPLLPtr pll,
				    double dot_clock)
{
    unsigned long  freq = dot_clock * 100;

    struct {
	int divider;
	int bitvalue;
    } *post_div, post_divs[]   = {
				/* From RAGE 128 VR/RAGE 128 GL Register
				 * Reference Manual (Technical Reference
				 * Manual P/N RRG-G04100-C Rev. 0.04), page
				 * 3-17 (PLL_DIV_[3:0]).
				 */
	{  1, 0 },              /* VCLK_SRC                 */
	{  2, 1 },              /* VCLK_SRC/2               */
	{  4, 2 },              /* VCLK_SRC/4               */
	{  8, 3 },              /* VCLK_SRC/8               */
	{  3, 4 },              /* VCLK_SRC/3               */
	{  6, 6 },              /* VCLK_SRC/6               */
	{ 12, 7 },              /* VCLK_SRC/12              */
	{  0, 0 }
    };

    if (freq > pll->max_pll_freq)      freq = pll->max_pll_freq;
    if (freq * 12 < pll->min_pll_freq) freq = pll->min_pll_freq / 12;

    for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
	save->pll_output_freq_2 = post_div->divider * freq;
	if (save->pll_output_freq_2 >= pll->min_pll_freq
	    && save->pll_output_freq_2 <= pll->max_pll_freq) break;
    }

    if (!post_div->divider) {
	save->pll_output_freq_2 = freq;
	post_div = &post_divs[0];
    }

    save->dot_clock_freq_2 = freq;
    save->feedback_div_2   = RADEONDiv(pll->reference_div
				       * save->pll_output_freq_2,
				       pll->reference_freq);
    save->post_div_2       = post_div->divider;

    RADEONTRACE(("dc=%d, of=%d, fd=%d, pd=%d\n",
	       save->dot_clock_freq_2,
	       save->pll_output_freq_2,
	       save->feedback_div_2,
	       save->post_div_2));

    save->p2pll_ref_div    = pll->reference_div;
    save->p2pll_div_0      = (save->feedback_div_2 |
			      (post_div->bitvalue << 16));
    save->htotal_cntl2     = 0;
}

#if 0
/* Define initial palette for requested video mode.  This doesn't do
 * anything for XFree86 4.0.
 */
static void RADEONInitPalette(RADEONSavePtr save)
{
    save->palette_valid = FALSE;
}
#endif

/* Define registers for a requested video mode */
static Bool RADEONInit(ScrnInfoPtr pScrn, DisplayModePtr mode,
		       RADEONSavePtr save)
{
    RADEONInfoPtr  info      = RADEONPTR(pScrn);
    double         dot_clock = mode->Clock/1000.0;

#if RADEON_DEBUG
    ErrorF("%-12.12s %7.2f  %4d %4d %4d %4d  %4d %4d %4d %4d (%d,%d)",
	   mode->name,
	   dot_clock,

	   mode->HDisplay,
	   mode->HSyncStart,
	   mode->HSyncEnd,
	   mode->HTotal,

	   mode->VDisplay,
	   mode->VSyncStart,
	   mode->VSyncEnd,
	   mode->VTotal,
	   pScrn->depth,
	   pScrn->bitsPerPixel);
    if (mode->Flags & V_DBLSCAN)   ErrorF(" D");
    if (mode->Flags & V_CSYNC)     ErrorF(" C");
    if (mode->Flags & V_INTERLACE) ErrorF(" I");
    if (mode->Flags & V_PHSYNC)    ErrorF(" +H");
    if (mode->Flags & V_NHSYNC)    ErrorF(" -H");
    if (mode->Flags & V_PVSYNC)    ErrorF(" +V");
    if (mode->Flags & V_NVSYNC)    ErrorF(" -V");
    ErrorF("\n");
    ErrorF("%-12.12s %7.2f  %4d %4d %4d %4d  %4d %4d %4d %4d (%d,%d)",
	   mode->name,
	   dot_clock,

	   mode->CrtcHDisplay,
	   mode->CrtcHSyncStart,
	   mode->CrtcHSyncEnd,
	   mode->CrtcHTotal,

	   mode->CrtcVDisplay,
	   mode->CrtcVSyncStart,
	   mode->CrtcVSyncEnd,
	   mode->CrtcVTotal,
	   pScrn->depth,
	   pScrn->bitsPerPixel);
    if (mode->Flags & V_DBLSCAN)   ErrorF(" D");
    if (mode->Flags & V_CSYNC)     ErrorF(" C");
    if (mode->Flags & V_INTERLACE) ErrorF(" I");
    if (mode->Flags & V_PHSYNC)    ErrorF(" +H");
    if (mode->Flags & V_NHSYNC)    ErrorF(" -H");
    if (mode->Flags & V_PVSYNC)    ErrorF(" +V");
    if (mode->Flags & V_NVSYNC)    ErrorF(" -V");
    ErrorF("\n");
#endif

    info->Flags = mode->Flags;

    RADEONInitCommonRegisters(save, info);
    if (info->IsSecondary) {
	if (!RADEONInitCrtc2Registers(pScrn, save, mode, info))
	    return FALSE;
	RADEONInitPLL2Registers(save, &info->pll, dot_clock);
    } else {
	if (!RADEONInitCrtcRegisters(pScrn, save, mode, info))
	    return FALSE;
	dot_clock = mode->Clock/1000.0;
	if (dot_clock) {
            if (info->UseBiosDividers) {
                save->ppll_ref_div = info->RefDivider;
                save->ppll_div_3   = info->FeedbackDivider | (info->PostDivider << 16);
                save->htotal_cntl  = 0;
            }
            else
		RADEONInitPLLRegisters(save, &info->pll, dot_clock);
	} else {
	    save->ppll_ref_div = info->SavedReg.ppll_ref_div;
	    save->ppll_div_3   = info->SavedReg.ppll_div_3;
	    save->htotal_cntl  = info->SavedReg.htotal_cntl;
	}

	if (info->Clone && info->CurCloneMode) {
	    RADEONInitCrtc2Registers(pScrn, save, info->CurCloneMode, info);
	    dot_clock = info->CurCloneMode->Clock / 1000.0;
	    RADEONInitPLL2Registers(save, &info->pll, dot_clock);
	}
	/* Not used for now: */
     /* if (!info->PaletteSavedOnVT) RADEONInitPalette(save); */
    }

    RADEONInitFPRegisters(pScrn, &info->SavedReg, save, mode, info);

    RADEONTRACE(("RADEONInit returns %p\n", save));
    return TRUE;
}

/* Initialize a new mode */
static Bool RADEONModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!RADEONInit(pScrn, mode, &info->ModeReg)) return FALSE;

    pScrn->vtSema = TRUE;
    RADEONBlank(pScrn);
    RADEONRestoreMode(pScrn, &info->ModeReg);
    RADEONUnblank(pScrn);

    info->CurrentLayout.mode = mode;

    if (info->DispPriority)
	RADEONInitDispBandwidth(pScrn);

    return TRUE;
}

static Bool RADEONSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr  pScrn = xf86Screens[pScreen->myNum];
    Bool         unblank;

    unblank = xf86IsUnblank(mode);
    if (unblank) SetTimeSinceLastInputEvent();

    if ((pScrn != NULL) && pScrn->vtSema) {
	if (unblank)  RADEONUnblank(pScrn);
	else          RADEONBlank(pScrn);
    }
    return TRUE;
}

Bool RADEONSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr    pScrn       = xf86Screens[scrnIndex];
    RADEONInfoPtr  info        = RADEONPTR(pScrn);
    Bool           ret;
#ifdef XF86DRI
    Bool           CPStarted   = info->CPStarted;

    if (CPStarted) {
	DRILock(pScrn->pScreen, 0);
	RADEONCP_STOP(pScrn, info);
    }
#endif

    if (info->accelOn) info->accel->Sync(pScrn);

    if (info->FBDev) {
	RADEONSaveFBDevRegisters(pScrn, &info->ModeReg);

	ret = fbdevHWSwitchMode(scrnIndex, mode, flags);

	RADEONRestoreFBDevRegisters(pScrn, &info->ModeReg);
    } else {
	info->IsSwitching = TRUE;
	if (info->Clone && info->CloneModes) {
	    DisplayModePtr  clone_mode = info->CloneModes;

	    /* Try to match a mode on primary head
	     * FIXME: This may not be good if both heads don't have
	     *        exactly the same list of mode.
	     */
	    while (1) {
		if ((clone_mode->HDisplay == mode->HDisplay) &&
		    (clone_mode->VDisplay == mode->VDisplay) &&
		    (!info->PanelOff)) {
		    info->CloneFrameX0 = (info->CurCloneMode->HDisplay +
					  info->CloneFrameX0 -
					  clone_mode->HDisplay - 1) / 2;
		    info->CloneFrameY0 =
			(info->CurCloneMode->VDisplay + info->CloneFrameY0 -
			 clone_mode->VDisplay - 1) / 2;
		    info->CurCloneMode = clone_mode;
		    break;
		}

		if (!clone_mode->next) {
		    info->CurCloneMode = info->CloneModes;
		    break;
		}

		clone_mode = clone_mode->next;
	    }
	}
	ret = RADEONModeInit(xf86Screens[scrnIndex], mode);

	if (info->CurCloneMode) {
	    if (info->CloneFrameX0 + info->CurCloneMode->HDisplay >=
		pScrn->virtualX)
		info->CloneFrameX0 =
		    pScrn->virtualX - info->CurCloneMode->HDisplay;
	    else if (info->CloneFrameX0 < 0)
		info->CloneFrameX0 = 0;

	    if (info->CloneFrameY0 + info->CurCloneMode->VDisplay >=
		pScrn->virtualY)
		info->CloneFrameY0 =
		    pScrn->virtualY - info->CurCloneMode->VDisplay;
	    else if (info->CloneFrameY0 < 0)
		info->CloneFrameY0 = 0;

	    RADEONDoAdjustFrame(pScrn, info->CloneFrameX0, info->CloneFrameY0,
				TRUE);
	}

	info->IsSwitching = FALSE;
    }

    if (info->accelOn) {
	info->accel->Sync(pScrn);
	RADEONEngineRestore(pScrn);
    }

#ifdef XF86DRI
    if (CPStarted) {
	RADEONCP_START(pScrn, info);
	DRIUnlock(pScrn->pScreen);
    }
#endif

    return ret;
}

#ifdef X_XF86MiscPassMessage
Bool RADEONHandleMessage(int scrnIndex, const char* msgtype,
			 const char* msgval, char** retmsg)
{
    ErrorF("RADEONHandleMessage(%d, \"%s\", \"%s\", retmsg)\n", scrnIndex,
		    msgtype, msgval);
    *retmsg = "";
    return 0;
}
#endif

/* Used to disallow modes that are not supported by the hardware */
ModeStatus RADEONValidMode(int scrnIndex, DisplayModePtr mode,
			   Bool verbose, int flag)
{
    /* There are problems with double scan mode at high clocks
     * They're likely related PLL and display buffer settings.
     * Disable these modes for now.
     */
    if (mode->Flags & V_DBLSCAN) {
	if ((mode->CrtcHDisplay >= 1024) || (mode->CrtcVDisplay >= 768))
	    return MODE_CLOCK_RANGE;
    }
    return MODE_OK;
}

/* Adjust viewport into virtual desktop such that (0,0) in viewport
 * space is (x,y) in virtual space.
 */
void RADEONDoAdjustFrame(ScrnInfoPtr pScrn, int x, int y, int clone)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            reg, Base = y * info->CurrentLayout.displayWidth + x;
#ifdef XF86DRI
    RADEONSAREAPrivPtr pSAREAPriv;
#endif

    switch (info->CurrentLayout.pixel_code) {
    case 15:
    case 16: Base *= 2; break;
    case 24: Base *= 3; break;
    case 32: Base *= 4; break;
    }

    Base &= ~7;                 /* 3 lower bits are always 0 */

    if (clone || info->IsSecondary) {
	Base += pScrn->fbOffset;
	reg = RADEON_CRTC2_OFFSET;
    } else {
	reg = RADEON_CRTC_OFFSET;
    }

#ifdef XF86DRI
    if (info->directRenderingEnabled) {

	pSAREAPriv = DRIGetSAREAPrivate(pScrn->pScreen);

	if (clone || info->IsSecondary) {
	    pSAREAPriv->crtc2_base = Base;
	}

	if (pSAREAPriv->pfCurrentPage == 1) {
	    Base += info->backOffset;
	}
    }
#endif

    OUTREG(reg, Base);
}

void RADEONAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr    pScrn      = xf86Screens[scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

#ifdef XF86DRI
    if (info->CPStarted) DRILock(pScrn->pScreen, 0);
#endif

    if (info->accelOn) info->accel->Sync(pScrn);

    if (info->FBDev) {
	fbdevHWAdjustFrame(scrnIndex, x, y, flags);
    } else {
	RADEONDoAdjustFrame(pScrn, x, y, FALSE);
    }

#ifdef XF86DRI
	if (info->CPStarted) DRIUnlock(pScrn->pScreen);
#endif
}

/* Called when VT switching back to the X server.  Reinitialize the
 * video mode.
 */
Bool RADEONEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr    pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    RADEONTRACE(("RADEONEnterVT\n"));

    if (info->FBDev) {
	unsigned char *RADEONMMIO = info->MMIO;
	if (!fbdevHWEnterVT(scrnIndex,flags)) return FALSE;
	info->PaletteSavedOnVT = FALSE;
	info->ModeReg.surface_cntl = INREG(RADEON_SURFACE_CNTL);

	RADEONRestoreFBDevRegisters(pScrn, &info->ModeReg);
    } else
	if (!RADEONModeInit(pScrn, pScrn->currentMode)) return FALSE;

#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	/* get the Radeon back into shape after resume */
	RADEONDRIResume(pScrn->pScreen);
    }
#endif
    /* this will get XVideo going again, but only if XVideo was initialised
       during server startup (hence the info->adaptor if). */
    if (info->adaptor)
	RADEONResetVideo(pScrn);

    if (info->accelOn)
	RADEONEngineRestore(pScrn);

#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	RADEONCP_START(pScrn, info);
	DRIUnlock(pScrn->pScreen);
    }
#endif

    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    if (info->CurCloneMode) {
	RADEONDoAdjustFrame(pScrn, info->CloneFrameX0, info->CloneFrameY0, TRUE);
    }

    return TRUE;
}

/* Called when VT switching away from the X server.  Restore the
 * original text mode.
 */
void RADEONLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr    pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    RADEONSavePtr  save  = &info->ModeReg;

    RADEONTRACE(("RADEONLeaveVT\n"));
#ifdef XF86DRI
    if (RADEONPTR(pScrn)->directRenderingEnabled) {
	DRILock(pScrn->pScreen, 0);
	RADEONCP_STOP(pScrn, info);
    }
#endif

    if (info->FBDev) {
	RADEONSavePalette(pScrn, save);
	info->PaletteSavedOnVT = TRUE;

	RADEONSaveFBDevRegisters(pScrn, &info->ModeReg);

	fbdevHWLeaveVT(scrnIndex,flags);
    }

    RADEONRestore(pScrn);
}

/* Called at the end of each server generation.  Restore the original
 * text mode, unmap video memory, and unwrap and call the saved
 * CloseScreen function.
 */
static Bool RADEONCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    RADEONTRACE(("RADEONCloseScreen\n"));

#ifdef XF86DRI
				/* Disable direct rendering */
    if (info->directRenderingEnabled) {
	RADEONDRICloseScreen(pScreen);
	info->directRenderingEnabled = FALSE;
    }
#endif

    if (pScrn->vtSema) {
	RADEONRestore(pScrn);
    }
    RADEONUnmapMem(pScrn);

    if (info->accel) XAADestroyInfoRec(info->accel);
    info->accel = NULL;

    if (info->scratch_save) xfree(info->scratch_save);
    info->scratch_save = NULL;

    if (info->cursor) xf86DestroyCursorInfoRec(info->cursor);
    info->cursor = NULL;

    if (info->DGAModes) xfree(info->DGAModes);
    info->DGAModes = NULL;

    pScrn->vtSema = FALSE;

    xf86ClearPrimInitDone(info->pEnt->index);

    pScreen->BlockHandler = info->BlockHandler;
    pScreen->CloseScreen = info->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

void RADEONFreeScreen(int scrnIndex, int flags)
{
    ScrnInfoPtr  pScrn = xf86Screens[scrnIndex];

    RADEONTRACE(("RADEONFreeScreen\n"));

    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(pScrn);
    RADEONFreeRec(pScrn);
}

/* Sets VESA Display Power Management Signaling (DPMS) Mode */
static void RADEONDisplayPowerManagementSet(ScrnInfoPtr pScrn,
					    int PowerManagementMode,
					    int flags)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

#ifdef XF86DRI
    if (info->CPStarted) DRILock(pScrn->pScreen, 0);
#endif

    if (info->accelOn) info->accel->Sync(pScrn);

    if (info->FBDev) {
	fbdevHWDPMSSet(pScrn, PowerManagementMode, flags);
    } else {
	int             mask1     = (RADEON_CRTC_DISPLAY_DIS |
				     RADEON_CRTC_HSYNC_DIS |
				     RADEON_CRTC_VSYNC_DIS);
	int             mask2     = (RADEON_CRTC2_DISP_DIS |
				     RADEON_CRTC2_VSYNC_DIS |
				     RADEON_CRTC2_HSYNC_DIS);

	/* TODO: additional handling for LCD ? */

	switch (PowerManagementMode) {
	case DPMSModeOn:
	    /* Screen: On; HSync: On, VSync: On */
	    if (info->IsSecondary)
		OUTREGP(RADEON_CRTC2_GEN_CNTL, 0, ~mask2);
	    else {
		if (info->Clone)
		    OUTREGP(RADEON_CRTC2_GEN_CNTL, 0, ~mask2);
		OUTREGP(RADEON_CRTC_EXT_CNTL, 0, ~mask1);
	    }
	    break;

	case DPMSModeStandby:
	    /* Screen: Off; HSync: Off, VSync: On */
	    if (info->IsSecondary)
		OUTREGP(RADEON_CRTC2_GEN_CNTL,
			RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS,
			~mask2);
	    else {
		if (info->Clone)
		    OUTREGP(RADEON_CRTC2_GEN_CNTL,
			    RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS,
			    ~mask2);
		OUTREGP(RADEON_CRTC_EXT_CNTL,
			RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS,
			~mask1);
	    }
	    break;

	case DPMSModeSuspend:
	    /* Screen: Off; HSync: On, VSync: Off */
	    if (info->IsSecondary)
		OUTREGP(RADEON_CRTC2_GEN_CNTL,
			RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS,
			~mask2);
	    else {
		if (info->Clone)
		    OUTREGP(RADEON_CRTC2_GEN_CNTL,
			    RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS,
			    ~mask2);
		OUTREGP(RADEON_CRTC_EXT_CNTL,
			RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS,
			~mask1);
	    }
	    break;

	case DPMSModeOff:
	    /* Screen: Off; HSync: Off, VSync: Off */
	    if (info->IsSecondary)
		OUTREGP(RADEON_CRTC2_GEN_CNTL, mask2, ~mask2);
	    else {
		if (info->Clone)
		    OUTREGP(RADEON_CRTC2_GEN_CNTL, mask2, ~mask2);
		OUTREGP(RADEON_CRTC_EXT_CNTL, mask1, ~mask1);
	    }
	    break;
	}

	if (PowerManagementMode == DPMSModeOn) {
	    if (info->IsSecondary) {
		if (info->DisplayType == MT_DFP) {
		    OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_BLANK_EN);
		    OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_ON, ~RADEON_FP2_ON);
		    if (info->ChipFamily >= CHIP_FAMILY_R200) {
			OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_DV0_EN, ~RADEON_FP2_DV0_EN);
		    }
		}
	    } else {
		if ((info->Clone) && (info->CloneType == MT_DFP)) {
		    OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_BLANK_EN);
		    OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_ON, ~RADEON_FP2_ON);
		    if (info->ChipFamily >= CHIP_FAMILY_R200) {
			OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_DV0_EN, ~RADEON_FP2_DV0_EN);
		    }
		}
		if (info->DisplayType == MT_DFP) {
		    OUTREGP (RADEON_FP_GEN_CNTL, (RADEON_FP_FPON | RADEON_FP_TMDS_EN),
			     ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN));
		} else if (info->DisplayType == MT_LCD) {

		    OUTREGP (RADEON_LVDS_GEN_CNTL, RADEON_LVDS_BLON, ~RADEON_LVDS_BLON);
		    usleep (info->PanelPwrDly * 1000);
		    OUTREGP (RADEON_LVDS_GEN_CNTL, RADEON_LVDS_ON, ~RADEON_LVDS_ON);
		}
	    }
	} else if ((PowerManagementMode == DPMSModeOff) ||
		   (PowerManagementMode == DPMSModeSuspend) ||
		   (PowerManagementMode == DPMSModeStandby)) {
	    if (info->IsSecondary) {
		if (info->DisplayType == MT_DFP) {
		    OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_BLANK_EN, ~RADEON_FP2_BLANK_EN);
		    OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_ON);
		    if (info->ChipFamily >= CHIP_FAMILY_R200) {
			OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_DV0_EN);
		    }
		}
	    } else {
		if ((info->Clone) && (info->CloneType == MT_DFP)) {
		    OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_BLANK_EN, ~RADEON_FP2_BLANK_EN);
		    OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_ON);
		    if (info->ChipFamily >= CHIP_FAMILY_R200) {
			OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_DV0_EN);
		    }
		}
		if (info->DisplayType == MT_DFP) {
		    OUTREGP (RADEON_FP_GEN_CNTL, 0, ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN));
		} else if (info->DisplayType == MT_LCD) {
		    unsigned long tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);

		    if (info->IsMobility || info->IsIGP) {
			/* Asic bug, when turning off LVDS_ON, we have to make sure
			   RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
			*/
			OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
		    }

		    OUTREGP (RADEON_LVDS_GEN_CNTL, 0,
			     ~(RADEON_LVDS_BLON | RADEON_LVDS_ON));

		    if (info->IsMobility || info->IsIGP) {
			OUTPLL(RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
		    }
		}
	    }
	}
    }

#ifdef XF86DRI
    if (info->CPStarted) DRIUnlock(pScrn->pScreen);
#endif
}
