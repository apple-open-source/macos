/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_driver.c,v 1.91 2003/02/25 03:50:15 dawes Exp $ */
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

#define USE_FB                  /* not until overlays */
#ifdef USE_FB
#include "fb.h"
#else

				/* CFB support */
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb16.h"
#include "cfb24.h"
#include "cfb32.h"
#endif

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

				/* Forward definitions for driver functions */
static Bool RADEONCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool RADEONSaveScreen(ScreenPtr pScreen, int mode);
static void RADEONSave(ScrnInfoPtr pScrn);
static void RADEONRestore(ScrnInfoPtr pScrn);
static Bool RADEONModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void RADEONDisplayPowerManagementSet(ScrnInfoPtr pScrn,
					    int PowerManagementMode,
					    int flags);

typedef enum {
    OPTION_NOACCEL,
    OPTION_SW_CURSOR,
    OPTION_DAC_6BIT,
    OPTION_DAC_8BIT,
#ifdef XF86DRI
    OPTION_IS_PCI,
    OPTION_CP_PIO,
    OPTION_USEC_TIMEOUT,
    OPTION_AGP_MODE,
    OPTION_AGP_FW,
    OPTION_AGP_SIZE,
    OPTION_RING_SIZE,
    OPTION_BUFFER_SIZE,
    OPTION_DEPTH_MOVE,
    OPTION_PAGE_FLIP,
    OPTION_NO_BACKBUFFER,
#endif
    OPTION_PANEL_OFF,
    OPTION_DDC_MODE,
    OPTION_CLONE_DISPLAY,
    OPTION_CLONE_MODE,
    OPTION_CLONE_HSYNC,
    OPTION_CLONE_VREFRESH,
    OPTION_FBDEV,
    OPTION_VIDEO_KEY
} RADEONOpts;

const OptionInfoRec RADEONOptions[] = {
    { OPTION_NOACCEL,        "NoAccel",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SW_CURSOR,      "SWcursor",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DAC_6BIT,       "Dac6Bit",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DAC_8BIT,       "Dac8Bit",          OPTV_BOOLEAN, {0}, TRUE  },
#ifdef XF86DRI
    { OPTION_IS_PCI,         "ForcePCIMode",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_CP_PIO,         "CPPIOMode",        OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_USEC_TIMEOUT,   "CPusecTimeout",    OPTV_INTEGER, {0}, FALSE },
    { OPTION_AGP_MODE,       "AGPMode",          OPTV_INTEGER, {0}, FALSE },
    { OPTION_AGP_FW,         "AGPFastWrite",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_AGP_SIZE,       "AGPSize",          OPTV_INTEGER, {0}, FALSE },
    { OPTION_RING_SIZE,      "RingSize",         OPTV_INTEGER, {0}, FALSE },
    { OPTION_BUFFER_SIZE,    "BufferSize",       OPTV_INTEGER, {0}, FALSE },
    { OPTION_DEPTH_MOVE,     "EnableDepthMoves", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_PAGE_FLIP,      "EnablePageFlip",   OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_NO_BACKBUFFER,  "NoBackBuffer",     OPTV_BOOLEAN, {0}, FALSE },
#endif
    { OPTION_PANEL_OFF,      "PanelOff",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DDC_MODE,       "DDCMode",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_CLONE_DISPLAY,  "CloneDisplay",     OPTV_INTEGER, {0}, FALSE },
    { OPTION_CLONE_MODE,     "CloneMode",        OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_CLONE_HSYNC,    "CloneHSync",       OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_CLONE_VREFRESH, "CloneVRefresh",    OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_FBDEV,          "UseFBDev",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_VIDEO_KEY,      "VideoKey",         OPTV_INTEGER, {0}, FALSE },
    { -1,                    NULL,               OPTV_NONE,    {0}, FALSE }
};

RADEONRAMRec RADEONRAM[] = {    /* Memory Specifications
				   From Radeon Manual */
    { 4, 4, 1, 2, 1, 2, 1, 16, 12, "64-bit SDR SDRAM" },
    { 4, 4, 3, 3, 2, 3, 1, 16, 12, "64-bit DDR SDRAM" },
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
    "cfb32ScreenInit",
    NULL
};
#endif

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
#ifdef USE_FB
			  fbSymbols,
#else
			  cfbSymbols,
#endif
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

extern int gRADEONEntityIndex;

struct RADEONInt10Save {
	CARD32 MEM_CNTL;
	CARD32 MEMSIZE;
	CARD32 MPP_TB_CONFIG;
};

static Bool RADEONMapMMIO(ScrnInfoPtr pScrn);
static Bool RADEONUnmapMMIO(ScrnInfoPtr pScrn);

static void
RADEONPreInt10Save(ScrnInfoPtr pScrn, void **pPtr)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO;
    int            mapped = 0;
    CARD32 CardTmp;
    static struct RADEONInt10Save SaveStruct = { 0, 0, 0 };

    /*
     * First make sure we have the pci and mmio info and that mmio is mapped
     */
    if (!info->PciInfo)
	info->PciInfo = xf86GetPciInfoForEntity(info->pEnt->index);
    if (!info->PciTag)
	info->PciTag = pciTag(info->PciInfo->bus, info->PciInfo->device,
			      info->PciInfo->func);
    if (!info->MMIOAddr)
	info->MMIOAddr = info->PciInfo->memBase[2] & 0xffffff00;
    if (!info->MMIO) {
	RADEONMapMMIO(pScrn);
	mapped = 1;
    }
    RADEONMMIO = info->MMIO;

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

    /* Unmap mmio space if we mapped it */
    if (mapped)
	RADEONUnmapMMIO(pScrn);
}

static void
RADEONPostInt10Check(ScrnInfoPtr pScrn, void *ptr)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO;
    struct RADEONInt10Save *pSave = ptr;
    CARD32 CardTmp;
    int            mapped = 0;

    /* If we don't have a valid (non-zero) saved MEM_CNTL, get out now */
    if (!pSave || !pSave->MEM_CNTL)
	return;

    /* First make sure that mmio is mapped */
    if (!info->MMIO) {
	RADEONMapMMIO(pScrn);
	mapped = 1;
    }
    RADEONMMIO = info->MMIO;

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
		   "Restoring MEM_CNTL (%08x), setting to %08x\n",
		   CardTmp, pSave->MEM_CNTL);
	OUTREG(RADEON_MEM_CNTL, pSave->MEM_CNTL);

	CardTmp = INREG(RADEON_CONFIG_MEMSIZE);
	if (CardTmp != pSave->MEMSIZE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Restoring CONFIG_MEMSIZE (%08x), setting to %08x\n",
		       CardTmp, pSave->MEMSIZE);
	    OUTREG(RADEON_CONFIG_MEMSIZE, pSave->MEMSIZE);
	}
    }

    CardTmp = INREG(RADEON_MPP_TB_CONFIG);
    if ((CardTmp & 0xff000000u) != (pSave->MPP_TB_CONFIG & 0xff000000u)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	           "Restoring MPP_TB_CONFIG<31:24> (%02x), setting to %02x\n",
	 	   CardTmp >> 24, pSave->MPP_TB_CONFIG >> 24);
	CardTmp &= 0x00ffffffu;
	CardTmp |= (pSave->MPP_TB_CONFIG & 0xff000000u);
	OUTREG(RADEON_MPP_TB_CONFIG, CardTmp);
    }

    /* Unmap mmio space if we mapped it */
    if (mapped)
	RADEONUnmapMMIO(pScrn);
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
		    RADEON_CRTC_DISPLAY_DIS |
		    RADEON_CRTC_VSYNC_DIS |
		    RADEON_CRTC_HSYNC_DIS,
		    ~(RADEON_CRTC_DISPLAY_DIS |
		      RADEON_CRTC_VSYNC_DIS |
		      RADEON_CRTC_HSYNC_DIS));
	    break;

	case MT_NONE:
	default:
	    break;
	}
	if (info->Clone)
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    RADEON_CRTC2_DISP_DIS |
		    RADEON_CRTC2_VSYNC_DIS |
		    RADEON_CRTC2_HSYNC_DIS,
		    ~(RADEON_CRTC2_DISP_DIS |
		      RADEON_CRTC2_VSYNC_DIS |
		      RADEON_CRTC2_HSYNC_DIS));
    } else {
	OUTREGP(RADEON_CRTC2_GEN_CNTL,
		RADEON_CRTC2_DISP_DIS |
		RADEON_CRTC2_VSYNC_DIS |
		RADEON_CRTC2_HSYNC_DIS,
		~(RADEON_CRTC2_DISP_DIS |
		  RADEON_CRTC2_VSYNC_DIS |
		  RADEON_CRTC2_HSYNC_DIS));
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
		    ~(RADEON_CRTC_DISPLAY_DIS |
		      RADEON_CRTC_VSYNC_DIS |
		      RADEON_CRTC_HSYNC_DIS));
	    break;

	case MT_NONE:
	default:
	    break;
	}
	if (info->Clone)
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    0,
		    ~(RADEON_CRTC2_DISP_DIS |
		      RADEON_CRTC2_VSYNC_DIS |
		      RADEON_CRTC2_HSYNC_DIS));
    } else {
	switch (info->DisplayType) {
	case MT_LCD:
	case MT_DFP:
	case MT_CRT:
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    0,
		    ~(RADEON_CRTC2_DISP_DIS |
		      RADEON_CRTC2_VSYNC_DIS |
		      RADEON_CRTC2_HSYNC_DIS));
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

/* Read the Video BIOS block and the FP registers (if applicable) */
static Bool RADEONGetBIOSParameters(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    RADEONInfoPtr  info            = RADEONPTR(pScrn);
    unsigned long  tmp, i;
    unsigned char *RADEONMMIO;
    Bool           BypassSecondary = FALSE;
    int            CloneDispOption;

#define RADEON_BIOS8(v)  (info->VBIOS[v])
#define RADEON_BIOS16(v) (info->VBIOS[v] | \
			  (info->VBIOS[(v) + 1] << 8))
#define RADEON_BIOS32(v) (info->VBIOS[v] | \
			  (info->VBIOS[(v) + 1] << 8) | \
			  (info->VBIOS[(v) + 2] << 16) | \
			  (info->VBIOS[(v) + 3] << 24))

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
	info->VBIOS = NULL;
	info->BIOSAddr = 0x00000000;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Video BIOS not found!\n");
	return TRUE;
    }

    info->FPBIOSstart = RADEON_BIOS16(0x48);
    info->OverlayOnCRTC2 = FALSE;

    RADEONMapMMIO(pScrn);
    RADEONMMIO = info->MMIO;

    /* FIXME: using BIOS scratch registers to detect connected monitors
     * may not be a reliable way.... should use EDID data.  Also it only
     * works with for VE/M6, no such registers in regular RADEON!!!
     */

    /* VE and M6 have both DVI and CRT ports (for M6 DVI port can be
     * switch to DFP port). The DVI port can also be conneted to a CRT
     * with an adapter.  Here is the definition of ports for this
     * driver:
     *
     * (1) If both port are connected, DVI port will be treated as the
     * Primary port (first screen in XF86Config, uses CRTC1) and CRT
     * port will be treated as the Secondary port (second screen in
     * XF86Config, uses CRTC2)
     *
     * (2) If only one screen specified in XF86Config, it will be used
     * for DVI port if a monitor is connected to DVI port, otherwise
     * (only one monitor is connected the CRT port) it will be used for
     * CRT port.
     */

    if (info->HasCRTC2) {
	/* FIXME: this may not be reliable */
	tmp = INREG(RADEON_BIOS_4_SCRATCH);

	if (info->IsSecondary) {
	    /* Check Port2 (CRT port) -- for the existing boards (VE &
	     * M6), this port can only be connected to a CRT
	     */
	    if (tmp & 0x02)        info->DisplayType = MT_CRT;
	    else if (tmp & 0x800)  info->DisplayType = MT_DFP;
	    else if (tmp & 0x400)  info->DisplayType = MT_LCD;
	    else if (tmp & 0x1000) info->DisplayType = MT_CTV;
	    else if (tmp & 0x2000) info->DisplayType = MT_STV;
	    else                   info->DisplayType = MT_CRT;

	} else {
	    info->Clone = FALSE;
	    info->CloneType = MT_NONE;

	    /* Check Primary (DVI/DFP port) */
	    if (tmp & 0x08)        info->DisplayType = MT_DFP;
	    else if (tmp & 0x04)   info->DisplayType = MT_LCD;
	    else if (tmp & 0x0200) info->DisplayType = MT_CRT;
	    else if (tmp & 0x10)   info->DisplayType = MT_CTV;
	    else if (tmp & 0x20)   info->DisplayType = MT_STV;
	    else                   info->DisplayType = MT_NONE;

	    if (info->DisplayType == MT_NONE) {
		/* DVI port has no monitor connected, try CRT port.
		 * If something on CRT port, treat it as primary
		 */
		if (xf86IsEntityShared(pScrn->entityList[0])) {
		    DevUnion     *pPriv;
		    RADEONEntPtr  pRADEONEnt;

		    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
						 gRADEONEntityIndex);
		    pRADEONEnt = pPriv->ptr;
		    pRADEONEnt->BypassSecondary = TRUE;
		}

		if (tmp & 0x02)        info->DisplayType = MT_CRT;
		else if (tmp & 0x800)  info->DisplayType = MT_DFP;
		else if (tmp & 0x400)  info->DisplayType = MT_LCD;
		else if (tmp & 0x1000) info->DisplayType = MT_CTV;
		else if (tmp & 0x2000) info->DisplayType = MT_STV;
		else {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "No monitor detected!!!\n");
		    return FALSE;
		}
		BypassSecondary = TRUE;
	    } else {
		if (tmp & 0x02) {
		    info->CloneType = MT_CRT;
		    info->Clone = TRUE;
		} else if (tmp & 0x800) {
		    info->CloneType = MT_DFP;
		    info->Clone = TRUE;
		}
	    }

	    /* FIXME: This option is too complicated.  We need to
	     *        find a better way to handle all cases.
	     *
	     * CloneDisplay options:
	     *  0      -- disable
	     *  1      -- auto-detect (default)
	     *  2      -- force on
	     *  3      -- auto-detect + 2nd head overlay.
	     *  4      -- force on + 2nd head overlay.
	     *  others -- auto-detect
	     *
	     * Force on: it will force the clone mode on even no display
	     * is detected. With this option together with the proper
	     * CloneHSync and CloneVRefresh options, we can turn on the
	     * CRT ouput on the 2nd head regardless if a monitor is
	     * connected there.  This way, we can plug in a CRT to the
	     * second head later after X server has started.
	     *
	     * 2nd head overlay: it will force the hardware overlay on
	     * CRTC2 (used by 2nd head). Since we only have one overlay,
	     * we have to decide which head to use it (the overlay space
	     * on the other head will be blank). 2nd head overlay is on
	     * automatically when PanelOff option is effective.
	     */
	    if (xf86GetOptValInteger(info->Options, OPTION_CLONE_DISPLAY,
				     &(CloneDispOption))) {
		char *s = NULL;

		if (CloneDispOption < 0 || CloneDispOption > 4) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "Illegal CloneDisplay Option set, "
			       "using default\n");
		    CloneDispOption = 1;
		}

		switch (CloneDispOption) {
		case 0: s = "Disable"; break;
		case 1: s = "Auto-detect"; break;
		case 2: s = "Force On"; break;
		case 3: s = "Auto-detect -- use 2nd head overlay"; break;
		case 4: s = "Force On -- use 2nd head overlay"; break;
		}
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "CloneDisplay option: %s (%d)\n",
			   s, CloneDispOption);
	    } else {
		/* Default to auto-detect */
		CloneDispOption = 1;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "CloneDisplay option not set -- "
			   "defaulting to auto-detect\n");
	    }

	    if (CloneDispOption == 0) {
		info->Clone = FALSE;
	    } else if ((CloneDispOption == 2 || CloneDispOption == 4)
		       && !info->Clone) {
		info->CloneType = MT_CRT;
		info->Clone = TRUE;
	    }

	    /* This will be used to set OV0_SCALAR_CNTL */
	    if (info->Clone && (CloneDispOption == 3 || CloneDispOption == 4))
		info->OverlayOnCRTC2 = TRUE; 
	}
    } else {
	/* Regular Radeon ASIC, only one CRTC, but it could be used for
	 * DFP with a DVI output, like AIW board
	 */
	tmp = INREG(RADEON_FP_GEN_CNTL);
	if (tmp & RADEON_FP_EN_TMDS) info->DisplayType = MT_DFP;
	else                         info->DisplayType = MT_CRT;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s Display == Type %d\n",
	       (info->IsSecondary ? "Secondary" : "Primary"),
	       info->DisplayType);

    RADEONMMIO = NULL;
    RADEONUnmapMMIO(pScrn);

    info->HBlank     = 0;
    info->HOverPlus  = 0;
    info->HSyncWidth = 0;
    info->VBlank     = 0;
    info->VOverPlus  = 0;
    info->VSyncWidth = 0;
    info->DotClock   = 0;

    if (info->DisplayType == MT_LCD) {
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
	    for (i = 0; i < 20; i++) {
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
		    info->VSyncWidth = ((RADEON_BIOS16(tmp0+28) & 0xf800)
					>> 11);
		    info->DotClock   = RADEON_BIOS16(tmp0+9) * 10;
		    info->Flags      = 0;
		}
	    }
	}
    } else if ((info->DisplayType == MT_DFP) && info->HasCRTC2) {
	tmp = RADEON_BIOS16(info->FPBIOSstart + 0x34);
	if (tmp != 0) {
	    tmp = RADEON_BIOS16(tmp + 2);
	    if (tmp != 0) {
		/* 18 bytes of EDID data should be here */
		info->DotClock = RADEON_BIOS16(tmp) * 10;
		info->PanelXRes =
		    ((RADEON_BIOS8(tmp + 4) & 0xf0) << 4) +
		    RADEON_BIOS8(tmp + 2);
		info->HBlank =
		    ((RADEON_BIOS8(tmp + 4) & 0x0f) << 8) +
		    RADEON_BIOS8(tmp + 3);
		info->PanelYRes =
		    ((RADEON_BIOS8(tmp + 7) & 0xf0) << 4) +
		    RADEON_BIOS8(tmp + 5);
		info->VBlank =
		    ((RADEON_BIOS8(tmp + 7) & 0x0f) << 8) +
		    RADEON_BIOS8(tmp + 6);
		info->HOverPlus =
		    ((RADEON_BIOS8(tmp + 11) & 0xc0) << 2) +
		    RADEON_BIOS8(tmp + 8);
		info->HSyncWidth =
		    ((RADEON_BIOS8(tmp + 11) & 0x30) << 4) +
		    RADEON_BIOS8(tmp + 9);
		info->VOverPlus =
		    ((RADEON_BIOS8(tmp + 11) & 0x0c) << 2) +
		    ((RADEON_BIOS8(tmp + 10) & 0xf0) >> 4);
		info->VSyncWidth =
		    ((RADEON_BIOS8(tmp + 11) & 0x03) << 4) +
		    (RADEON_BIOS8(tmp + 10) & 0x0f);
		info->Flags = 0;
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "No DFP timing table detected\n");
	    }
	}

	RADEONTRACE(("DFP Info: ----------------------\n"
		     "pixel clock: %d KHz\n"
		     "panel size: %dx%d\n"
		     "H. Blanking: %d\n"
		     "H. Sync. Offset: %d\n"
		     "H. Sync. Width: %d\n"
		     "V. Blanking: %d\n"
		     "V. Sync. Offset: %d\n"
		     "V. Sync. Width: %d\n",
		     info->DotClock,
		     info->PanelXRes, info->PanelYRes,
		     info->HBlank,
		     info->HOverPlus,
		     info->HSyncWidth,
		     info->VBlank, info->VOverPlus, info->VSyncWidth));
    }

    /* Detect connector type from BIOS, used for I2C/DDC qeurying EDID,
     * Only available for VE or newer cards */

    /* DELL OEM card doesn't seem to follow the conviention for BIOS's
     * DDC type, we have to make a special case.  Following hard coded
     * type works with both CRT+CRT and DVI+DVI cases
     */
    if (info->IsDell && info->DellType == 2) {
	if (info->IsSecondary)
	    info->DDCType = DDC_CRT2;
	else
	    info->DDCType = DDC_DVI;
	info->CloneDDCType = DDC_CRT2;
    } else if ((tmp = RADEON_BIOS16(info->FPBIOSstart + 0x50))) {
	for (i = 1; i < 4; i++) {
	    unsigned int tmp0;
	    if (!RADEON_BIOS8(tmp + i*2) && i > 1) break;

	    /* Note: Secondary port (CRT port) actually uses primary DAC */
	    tmp0 = RADEON_BIOS16(tmp + i*2);
	    if (tmp0 & 0x01) {
		if (!info->IsSecondary && !BypassSecondary)
		    info->DDCType = (tmp0 & 0x0f00) >> 8;
	    } else { /* Primary DAC */
		if (info->Clone)
		    info->CloneDDCType = (tmp0 & 0x0f00) >> 8;
		else if (info->IsSecondary ||
			 BypassSecondary ||
			 !info->HasCRTC2) {
		    info->DDCType = (tmp0 & 0x0f00) >> 8;
		}
	    }
	}
    } else {
	/* Orignal radeon cards, set it to DDC_VGA, this will not work
	 * with AIW, it should be DDC_DVI, let it fall back to VBE calls
	 * for AIW
	 */
	info->DDCType = DDC_VGA;
    }

    return TRUE;
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

    if (!info->VBIOS) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Video BIOS not detected, using default PLL parameters!\n");
				/* These probably aren't going to work for
				   the card you are using.  Specifically,
				   reference freq can be 29.50MHz,
				   28.63MHz, or 14.32MHz.  YMMV. */

	/* These are somewhat sane defaults for Mac boards, we will need
	 * to find a good way of getting these from OpenFirmware
	 */
	pll->reference_freq = 2700;
	pll->reference_div  = 67;
	pll->min_pll_freq   = 12500;
	pll->max_pll_freq   = 35000;
	pll->xclk           = 16615;
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

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "PLL parameters: rf=%d rd=%d min=%d max=%d; xclk=%d\n",
	       pll->reference_freq,
	       pll->reference_div,
	       pll->min_pll_freq,
	       pll->max_pll_freq,
	       pll->xclk);

    return TRUE;
}

/* This is called by RADEONPreInit to set up the default visual */
static Bool RADEONPreInitVisual(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, Support32bppFb))
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
    int            offset = 0; /* RAM Type */
    MessageType    from;
    unsigned char *RADEONMMIO;

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
    switch (info->Chipset) {
    case PCI_CHIP_RADEON_LY:
    case PCI_CHIP_RADEON_LZ:
	info->ChipFamily = CHIP_FAMILY_M6;
	break;

    case PCI_CHIP_RV100_QY:
    case PCI_CHIP_RV100_QZ:
	info->ChipFamily = CHIP_FAMILY_VE;
	break;

    case PCI_CHIP_R200_BB:
    case PCI_CHIP_R200_QH:
    case PCI_CHIP_R200_QI:
    case PCI_CHIP_R200_QJ:
    case PCI_CHIP_R200_QK:
    case PCI_CHIP_R200_QL:
    case PCI_CHIP_R200_QM:
    case PCI_CHIP_R200_QN:
    case PCI_CHIP_R200_QO:
    case PCI_CHIP_R200_Qh:
    case PCI_CHIP_R200_Qi:
    case PCI_CHIP_R200_Qj:
    case PCI_CHIP_R200_Qk:
    case PCI_CHIP_R200_Ql:
	info->ChipFamily = CHIP_FAMILY_R200;
	break;

    case PCI_CHIP_RV200_QW: /* RV200 desktop */
    case PCI_CHIP_RV200_QX:
	info->ChipFamily = CHIP_FAMILY_RV200;
	break;

    case PCI_CHIP_RADEON_LW:
    case PCI_CHIP_RADEON_LX:
	info->ChipFamily = CHIP_FAMILY_M7;
	break;

    case PCI_CHIP_RV250_Id:
    case PCI_CHIP_RV250_Ie:
    case PCI_CHIP_RV250_If:
    case PCI_CHIP_RV250_Ig:
	info->ChipFamily = CHIP_FAMILY_RV250;
	break;

    case PCI_CHIP_RV250_Ld:
    case PCI_CHIP_RV250_Le:
    case PCI_CHIP_RV250_Lf:
    case PCI_CHIP_RV250_Lg:
	info->ChipFamily = CHIP_FAMILY_M9;
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

    default:
	/* Original Radeon/7200 */
	info->ChipFamily = CHIP_FAMILY_RADEON;
	info->HasCRTC2 = FALSE;
    }

    /* Here is the special case for DELL's VE card.
     * It needs some special handlings for it's 2nd head to work.
     */
    info->IsDell = FALSE;
    if (info->ChipFamily == CHIP_FAMILY_VE &&
	info->PciInfo->subsysVendor == PCI_VENDOR_ATI && 
	info->PciInfo->subsysCard & (1 << 12)) { /* DELL's signature */
	if (info->PciInfo->subsysCard & 0xb00) {
	    info->IsDell = TRUE;
	    info->DellType = 2;	/* DVI+DVI config, this seems to be the
				 * only known type for now, can be
				 * connected to both DVI+DVI and VGA+VGA
				 * dongles.
				 */
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "DELL OEM Card detected with %s (type %d)\n",
		       (info->DellType == 2) ? "DVI+DVI / VGA+VGA" : "VGA+VGA",
		       info->DellType);
	} else {
	    info->DellType = 0;	/* Unknown */
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Unknown type of DELL's Card (SSCID %x), "
		       "treated as normal type\n",
		       info->PciInfo->subsysCard);
	}
    }

				/* Framebuffer */

    from               = X_PROBED;
    info->LinearAddr   = info->PciInfo->memBase[0] & 0xfc000000;
    pScrn->memPhysBase = info->LinearAddr;
    if (dev->MemBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Linear address override, using 0x%08x instead of 0x%08x\n",
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

				/* MMIO registers */
    from             = X_PROBED;
    info->MMIOAddr   = info->PciInfo->memBase[2] & 0xffffff00;
    if (dev->IOBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "MMIO address override, using 0x%08x instead of 0x%08x\n",
		   dev->IOBase,
		   info->MMIOAddr);
	info->MMIOAddr = dev->IOBase;
	from           = X_CONFIG;
    } else if (!info->MMIOAddr) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid MMIO address\n");
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "MMIO registers at 0x%08lx\n", info->MMIOAddr);

				/* BIOS */
    from              = X_PROBED;
    info->BIOSAddr    = info->PciInfo->biosBase & 0xfffe0000;
    if (dev->BiosBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "BIOS address override, using 0x%08x instead of 0x%08x\n",
		   dev->BiosBase,
		   info->BIOSAddr);
	info->BIOSAddr = dev->BiosBase;
	from           = X_CONFIG;
    }
    if (info->BIOSAddr) {
	xf86DrvMsg(pScrn->scrnIndex, from,
		   "BIOS at 0x%08lx\n", info->BIOSAddr);
    }

    RADEONMapMMIO(pScrn);
    RADEONMMIO               = info->MMIO;

				/* Read registers used to determine options */
    from                     = X_PROBED;
    if (info->FBDev)
	pScrn->videoRam      = fbdevHWGetVidmem(pScrn) / 1024;
    else
	pScrn->videoRam      = INREG(RADEON_CONFIG_MEMSIZE) / 1024;

    /* Some production boards of m6 will return 0 if it's 8 MB */
    if (pScrn->videoRam == 0) pScrn->videoRam = 8192;

    if (info->IsSecondary) {
	/* FIXME: For now, split FB into two equal sections. This should
	 * be able to be adjusted by user with a config option. */
	DevUnion      *pPriv;
	RADEONEntPtr   pRADEONEnt;
	RADEONInfoPtr  info1;

	pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gRADEONEntityIndex);
	pRADEONEnt = pPriv->ptr;
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
    RADEONMMIO               = NULL;
    RADEONUnmapMMIO(pScrn);

				/* RAM */
    switch (info->MemCntl >> 30) {
    case 0:  offset = 0; break; /*  64-bit SDR SDRAM */
    case 1:  offset = 1; break; /*  64-bit DDR SDRAM */
    default: offset = 0;
    }
    info->ram = &RADEONRAM[offset];

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
	       "VideoRAM: %d kByte (%s)\n", pScrn->videoRam, info->ram->name);

#ifdef XF86DRI
				/* AGP/PCI */
    if (xf86ReturnOptValBool(info->Options, OPTION_IS_PCI, FALSE)) {
	info->IsPCI = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into PCI-only mode\n");
    } else {
	switch (info->Chipset) {
#if 0
	case PCI_CHIP_RADEON_XX: info->IsPCI = TRUE;  break;
#endif
	case PCI_CHIP_RV100_QY:
	case PCI_CHIP_RV100_QZ:
	case PCI_CHIP_RADEON_LW:
	case PCI_CHIP_RADEON_LX:
	case PCI_CHIP_RADEON_LY:
	case PCI_CHIP_RADEON_LZ:
	case PCI_CHIP_RADEON_QD:
	case PCI_CHIP_RADEON_QE:
	case PCI_CHIP_RADEON_QF:
	case PCI_CHIP_RADEON_QG:
	case PCI_CHIP_R200_BB:
	case PCI_CHIP_R200_QH:
	case PCI_CHIP_R200_QI:
	case PCI_CHIP_R200_QJ:
	case PCI_CHIP_R200_QK:
	case PCI_CHIP_R200_QL:
	case PCI_CHIP_R200_QM:
	case PCI_CHIP_R200_QN:
	case PCI_CHIP_R200_QO:
	case PCI_CHIP_R200_Qh:
	case PCI_CHIP_R200_Qi:
	case PCI_CHIP_R200_Qj:
	case PCI_CHIP_R200_Qk:
	case PCI_CHIP_R200_Ql:
	case PCI_CHIP_RV200_QW:
	case PCI_CHIP_RV200_QX:
	case PCI_CHIP_RV250_Id:
	case PCI_CHIP_RV250_Ie:
	case PCI_CHIP_RV250_If:
	case PCI_CHIP_RV250_Ig:
	case PCI_CHIP_RV250_Ld:
	case PCI_CHIP_RV250_Le:
	case PCI_CHIP_RV250_Lf:
	case PCI_CHIP_RV250_Lg:
	case PCI_CHIP_R300_AD:
	case PCI_CHIP_R300_AE:
	case PCI_CHIP_R300_AF:
	case PCI_CHIP_R300_AG:
	case PCI_CHIP_R300_ND:
	case PCI_CHIP_R300_NE:
	case PCI_CHIP_R300_NF:
	case PCI_CHIP_R300_NG:
	default:                 info->IsPCI = FALSE; break;
	}
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

    switch (info->DDCType) {
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
	return FALSE;
    }

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

static xf86MonPtr RADEONDoDDC(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    xf86MonPtr     MonInfo = NULL;
    unsigned char *RADEONMMIO;
    int            i;

    /* We'll use DDC2, BIOS EDID can only detect the monitor connected
     * to one port. For VE, BIOS EDID detects the monitor connected to
     * DVI port by default. If no monitor their, it will try CRT port
     */

    /* Read and output monitor info using DDC2 over I2C bus */
    if (info->pI2CBus && info->ddc2) {
	int  j;

	if (!RADEONMapMMIO(pScrn)) return NULL;
	RADEONMMIO = info->MMIO;
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
	    MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, info->pI2CBus);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 50; i++) {
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
	    if (MonInfo)
		break;
	}

	RADEONUnmapMMIO(pScrn);
    }

    if (!MonInfo && pInt10 && (info->DDCReg == RADEON_GPIO_VGA_DDC)) {
	if (xf86LoadSubModule(pScrn, "vbe")) {
	    vbeInfoPtr  pVbe;
	    pVbe = VBEInit(pInt10, info->pEnt->index);
	    if (pVbe) {
		for (i = 0; i < 5; i++) {
		    MonInfo = vbeDoEDID(pVbe, NULL);
		    info->ddc_bios = TRUE;
		    if (MonInfo)
			break;
		}
	    } else
		info->ddc_bios = FALSE;
	}
    }

    if (MonInfo) {
	if (info->ddc2)
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "I2C EDID Info:\n");
	else if (info->ddc_bios)
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS  EDID Info:\n");
	else return NULL;

	xf86PrintEDID(MonInfo);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "End of DDC Monitor info\n\n");

	xf86SetDDCproperties(pScrn, MonInfo);
	return MonInfo;
    }
    else return NULL;
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

    if (pScrn->monitor->DDC) {
	int  maxVirtX = pScrn->virtualX;
	int  maxVirtY = pScrn->virtualY;

	if (DisplayType != MT_CRT) {
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

/* XFree86's xf86ValidateModes routine doesn't work well with DFPs, so
 * here is our own validation routine.
 */
static int RADEONValidateFPModes(ScrnInfoPtr pScrn, char **ppModeName)
{
    RADEONInfoPtr   info       = RADEONPTR(pScrn);
    DisplayModePtr  last       = NULL;
    DisplayModePtr  new        = NULL;
    DisplayModePtr  first      = NULL;
    int             count      = 0;
    int             i, width, height;

    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    /* We have a flat panel connected to the primary display, and we
     * don't have any DDC info.
     */
    for (i = 0; ppModeName[i] != NULL; i++) {
	/* FIXME: Use HDisplay and VDisplay instead of mode string */
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
    char           *s;
    char          **clone_mode_names = NULL;
    Bool            ddc_mode         = info->ddc_mode;

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
    clockRanges->interlaceAllowed  = FALSE;
    clockRanges->doubleScanAllowed = FALSE;

    /* Only take one clone mode from config file for now, rest of clone
     * modes will copy from primary head.
     */
    if ((s = xf86GetOptValString(info->Options, OPTION_CLONE_MODE))) {
	if (sscanf(s, "%dx%d", &tmp_hdisplay, &tmp_vdisplay) == 2) {
	    if(count > 0) free(clone_mode_names[0]);
	    else count++;
	    clone_mode_names[0] = xnfalloc(strlen(s)+1);
	    sprintf(clone_mode_names[0], "%dx%d", tmp_hdisplay, tmp_vdisplay);
	    xf86DrvMsg(0, X_INFO, "Clone mode %s in config file is used\n");
	}
    }

    if (pScrn->display->virtualX < tmp_hdisplay)
	pScrn->display->virtualX = tmp_hdisplay;
    if (pScrn->display->virtualY < tmp_vdisplay)
	pScrn->display->virtualY = tmp_vdisplay;

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

    if ((pScrn->monitor->nVrefresh == 0) || (pScrn->monitor->nHsync == 0) ||
	(info->CloneType != MT_CRT) || info->ddc_mode) {
	unsigned int  save_ddc_reg;
	save_ddc_reg = info->DDCReg;
	switch (info->CloneDDCType) {
	case DDC_MONID: info->DDCReg = RADEON_GPIO_MONID;    break;
	case DDC_DVI:   info->DDCReg = RADEON_GPIO_DVI_DDC;  break;
	case DDC_VGA:   info->DDCReg = RADEON_GPIO_VGA_DDC;  break;
	case DDC_CRT2:  info->DDCReg = RADEON_GPIO_CRT2_DDC; break;
	default:        info->DDCReg = 0;                    break;
	}
	
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "DDC detection (type %d) for clone modes\n",
		   info->CloneDDCType);

	/* When primary head has an invalid DDC type, I2C is not
         * initialized, so we do it here.
	 */
	if (!info->ddc2) info->ddc2 = xf86I2CBusInit(info->pI2CBus);

	pScrn->monitor->DDC = RADEONDoDDC(pScrn, NULL);
	if (pScrn->monitor->DDC) {
	    if (info->CloneType == MT_CRT) {
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
	info->DDCReg = save_ddc_reg;
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
	info->IsSecondary = TRUE; /* Fake it */
	modesFound = RADEONValidateDDCModes(pScrn, clone_mode_names,
					    info->CloneType);
	info->IsSecondary = FALSE; /* Restore it!!! */

	/* If that fails and we're connect to a flat panel, then try to
         * add the flat panel modes
	 */
	if (modesFound < 1 && info->DisplayType != MT_CRT)
	    modesFound = RADEONValidateFPModes(pScrn, clone_mode_names);
    }

    if (modesFound > 0) {
	xf86SetCrtcForModes(pScrn, 0);
	xf86PrintModes(pScrn);
	for (i = 0; i < modesFound; i++) {
	    while (pScrn->modes->status != MODE_OK) {
		pScrn->modes = pScrn->modes->next;
	    }
	    if (!pScrn->modes) break;

	    clone_mode = xnfcalloc (1, sizeof (DisplayModeRec));
	    if (!clone_mode || !pScrn->modes) break;
	    memcpy(clone_mode, pScrn->modes, sizeof(DisplayModeRec));
	    clone_mode->name = xnfalloc(strlen(pScrn->modes->name) + 1);
	    strcpy(clone_mode->name, pScrn->modes->name);

	    if (i == 0) {
		info->CloneModes = clone_mode;
		info->CurCloneMode = clone_mode;
	    } else {
		clone_mode->prev = tmp_mode;
		clone_mode->prev->next = clone_mode;
	    }

	    tmp_mode = clone_mode;
	    clone_mode->next = NULL;
	    pScrn->modes = pScrn->modes->next;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Valid Clone Mode: %s\n", clone_mode->name);
	}
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
    pScrn->modes = save_mode;

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

    /* modePool is no longer needed, free it */
    while (pScrn->modePool)
	xf86DeleteMode(&pScrn->modePool, pScrn->modePool);
    pScrn->modePool = NULL;

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
    char          *mod = NULL;
#ifndef USE_FB
    const char    *Sym = NULL;
#endif

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
	    DevUnion     *pPriv;
	    RADEONEntPtr  pRADEONEnt;
	    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
					 gRADEONEntityIndex);
	    pRADEONEnt = pPriv->ptr;

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
	       "Validating modes on %s head (DDCType: %d) ---------\n", 
	       info->IsSecondary ? "Secondary" : "Primary",
	       info->DDCType);

    pScrn->monitor->DDC = RADEONDoDDC(pScrn, pInt10);
    if (!pScrn->monitor->DDC && info->ddc_mode) {
	info->ddc_mode = FALSE;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "No DDC data available, DDCMode option is dismissed\n");
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
	if (modesFound < 1 && info->DisplayType != MT_CRT)
	    modesFound = RADEONValidateFPModes(pScrn, pScrn->display->modes);

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

	/* Setup the screen's clockRanges for the VidMode extension */
	pScrn->clockRanges = xnfcalloc(sizeof(*(pScrn->clockRanges)), 1);
	memcpy(pScrn->clockRanges, clockRanges, sizeof(*clockRanges));
	pScrn->clockRanges->strategy = LOOKUP_BEST_REFRESH;
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
#ifdef USE_FB
    mod = "fb";
#else
    switch (pScrn->bitsPerPixel) {
    case  8: mod = "cfb";   Sym = "cfbScreenInit";   break;
    case 16: mod = "cfb16"; Sym = "cfb16ScreenInit"; break;
    case 32: mod = "cfb32"; Sym = "cfb32ScreenInit"; break;
    }
#endif

    if (mod && !xf86LoadSubModule(pScrn, mod)) return FALSE;

#ifdef USE_FB
    xf86LoaderReqSymLists(fbSymbols, NULL);
#else
    xf86LoaderReqSymbols(Sym, NULL);
#endif

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
    info->agpSize       = RADEON_DEFAULT_AGP_SIZE;
    info->ringSize      = RADEON_DEFAULT_RING_SIZE;
    info->bufSize       = RADEON_DEFAULT_BUFFER_SIZE;
    info->agpTexSize    = RADEON_DEFAULT_AGP_TEX_SIZE;
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

	if (xf86GetOptValInteger(info->Options,
				 OPTION_AGP_SIZE, (int *)&(info->agpSize))) {
	    switch (info->agpSize) {
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
			   "Illegal AGP size: %d MB\n", info->agpSize);
		return FALSE;
	    }
	}

	if (xf86GetOptValInteger(info->Options,
				 OPTION_RING_SIZE, &(info->ringSize))) {
	    if (info->ringSize < 1 || info->ringSize >= (int)info->agpSize) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Illegal ring buffer size: %d MB\n",
			   info->ringSize);
		return FALSE;
	    }
	}

	if (xf86GetOptValInteger(info->Options,
				 OPTION_BUFFER_SIZE, &(info->bufSize))) {
	    if (info->bufSize < 1 || info->bufSize >= (int)info->agpSize) {
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

	if (info->ringSize + info->bufSize + info->agpTexSize >
	    (int)info->agpSize) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Buffers are too big for requested AGP space\n");
	    return FALSE;
	}

	info->agpTexSize = info->agpSize - (info->ringSize + info->bufSize);
    }

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
    
    RADEONTRACE(("RADEONPreInit\n"));
    if (pScrn->numEntities != 1) return FALSE;

    if (!RADEONGetRec(pScrn)) return FALSE;

    info               = RADEONPTR(pScrn);
    info->IsSecondary  = FALSE;
    info->Clone        = FALSE;
    info->CurCloneMode = NULL;
    info->CloneModes   = NULL;
    info->IsSwitching  = FALSE;

    info->pEnt         = xf86GetEntityInfo(pScrn->entityList[0]);
    if (info->pEnt->location.type != BUS_PCI) goto fail;

    info->PciInfo = xf86GetPciInfoForEntity(info->pEnt->index);
    info->PciTag  = pciTag(info->PciInfo->bus,
			   info->PciInfo->device,
			   info->PciInfo->func);

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

    if (xf86IsEntityShared(pScrn->entityList[0])) {
	if (xf86IsPrimInitDone(pScrn->entityList[0])) {
	    DevUnion     *pPriv;
	    RADEONEntPtr  pRADEONEnt;

	    info->IsSecondary = TRUE;
	    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
					 gRADEONEntityIndex);
	    pRADEONEnt = pPriv->ptr;
	    if (pRADEONEnt->BypassSecondary) {
		pRADEONEnt->HasSecondary = FALSE;
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Only one monitor detected, Second screen "
			   "will NOT be created\n");
		return FALSE;
	    }
	    pRADEONEnt->pSecondaryScrn = pScrn;
	} else {
	    DevUnion     *pPriv;
	    RADEONEntPtr  pRADEONEnt;

	    xf86SetPrimInitDone(pScrn->entityList[0]);
	    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
					 gRADEONEntityIndex);

	    pRADEONEnt = pPriv->ptr;
	    pRADEONEnt->pPrimaryScrn        = pScrn;
	    pRADEONEnt->IsDRIEnabled        = FALSE;
	    pRADEONEnt->BypassSecondary     = FALSE;
	    pRADEONEnt->RestorePrimary      = FALSE;
	    pRADEONEnt->IsSecondaryRestored = FALSE;
	}
    }

    if (flags & PROBE_DETECT) {
	RADEONProbeDDC(pScrn, info->pEnt->index);
	RADEONPostInt10Check(pScrn, int10_save);
	return TRUE;
    }

    if (!xf86LoadSubModule(pScrn, "vgahw")) return FALSE;
    xf86LoaderReqSymLists(vgahwSymbols, NULL);
    if (!vgaHWGetHWRec(pScrn)) {
	RADEONFreeRec(pScrn);
	return FALSE;
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

#if !defined(__powerpc__)
    if (!RADEONGetBIOSParameters(pScrn, pInt10))
	goto fail;
#else
    /* Force type to CRT since we currently can't read BIOS to tell us
     * what kind of heads we have.
     */
    info->DisplayType = MT_CRT;
#endif

    RADEONPreInitDDC(pScrn);

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

    xf86DrvMsg(pScrn->scrnIndex, X_NOTICE,
	       "For information on using the multimedia capabilities\n of this"
	       " adapter, please see http://gatos.sf.net.\n");

    return TRUE;

fail:
				/* Pre-init failed. */

				/* Free the video bios (if applicable) */
    if (info->VBIOS) {
	xfree(info->VBIOS);
	info->VBIOS = NULL;
    }

				/* Free int10 info */
    if (pInt10)
	xf86FreeInt10(pInt10);

    vgaHWFreeHWRec(pScrn);
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
				   established, but before cfbScreenInit is
				   called.  cfbScreenInit will eventually
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
	} else if (info->ChipFamily >= CHIP_FAMILY_R300) {
	    info->directRenderingEnabled = FALSE;
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "Direct rendering not yet supported on "
		       "Radeon 9500/9700 and newer cards\n");
	} else {
	    if (info->IsSecondary)
		info->directRenderingEnabled = FALSE;
	    else {
		/* Xinerama has sync problem with DRI, disable it for now */
		if (xf86IsEntityShared(pScrn->entityList[0])) {
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

		if (xf86IsEntityShared(pScrn->entityList[0])) {
		    DevUnion     *pPriv;
		    RADEONEntPtr  pRADEONEnt;

		    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
						 gRADEONEntityIndex);
		    pRADEONEnt = pPriv->ptr;
		    pRADEONEnt->IsDRIEnabled = info->directRenderingEnabled;
		}
	    }
	}
    }
#endif

#ifdef USE_FB
    if (!fbScreenInit(pScreen, info->FB,
		      pScrn->virtualX, pScrn->virtualY,
		      pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth,
		      pScrn->bitsPerPixel))
	return FALSE;
#else
    switch (pScrn->bitsPerPixel) {
    case 8:
	if (!cfbScreenInit(pScreen, info->FB,
			   pScrn->virtualX, pScrn->virtualY,
			   pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth))
	    return FALSE;
	break;
    case 16:
	if (!cfb16ScreenInit(pScreen, info->FB,
			     pScrn->virtualX, pScrn->virtualY,
			     pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth))
	    return FALSE;
	break;
    case 32:
	if (!cfb32ScreenInit(pScreen, info->FB,
			     pScrn->virtualX, pScrn->virtualY,
			     pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth))
	    return FALSE;
	break;
    default:
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Invalid bpp (%d)\n", pScrn->bitsPerPixel);
	return FALSE;
    }
#endif

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

#ifdef USE_FB
    /* Must be after RGB order fixed */
    fbPictureInit (pScreen, 0, 0);
#endif

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
		   "Using %d MB AGP aperture\n", info->agpSize);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB for the ring buffer\n", info->ringSize);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB for vertex/indirect buffers\n", info->bufSize);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Using %d MB for AGP textures\n", info->agpTexSize);

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
		       "Using hardware cursor (scanline %d)\n",
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
#ifdef DPMSExtension
    xf86DPMSInit(pScreen, RADEONDisplayPowerManagementSet, 0);
#endif

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
				/* Now that mi, cfb, drm and others have
				   done their thing, complete the DRI
				   setup. */
	info->directRenderingEnabled = RADEONDRIFinishScreenInit(pScreen);
    }
    if (info->directRenderingEnabled) {
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
	info->ChipFamily != CHIP_FAMILY_R300) {
	DevUnion     *pPriv;
	RADEONEntPtr  pRADEONEnt;
	CARD32        tmp;

	pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gRADEONEntityIndex);
	pRADEONEnt = pPriv->ptr;

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

    if (info->ChipFamily == CHIP_FAMILY_R200 ||
	info->ChipFamily == CHIP_FAMILY_R300) {
	OUTREG(RADEON_DISP_OUTPUT_CNTL, restore->disp_output_cntl);
    } else {
	OUTREG(RADEON_DISP_HW_DEBUG, restore->disp_hw_debug);
	if (info->IsDell) {
	    /* Workaround for DELL card. BIOS doesn't initialize
	     * TV_DAC_CNTL to a correct value which causes too high
	     * contrast for the second CRT (using TV_DAC).
	     */
	    OUTREG(RADEON_TV_DAC_CNTL, 0x00280203);
	}
    }

    OUTREG(RADEON_CRTC2_H_TOTAL_DISP,    restore->crtc2_h_total_disp);
    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, restore->crtc2_h_sync_strt_wid);
    OUTREG(RADEON_CRTC2_V_TOTAL_DISP,    restore->crtc2_v_total_disp);
    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, restore->crtc2_v_sync_strt_wid);
    OUTREG(RADEON_CRTC2_OFFSET,          restore->crtc2_offset);
    OUTREG(RADEON_CRTC2_OFFSET_CNTL,     restore->crtc2_offset_cntl);
    OUTREG(RADEON_CRTC2_PITCH,           restore->crtc2_pitch);

    if (info->DisplayType == MT_DFP || info->CloneType == MT_DFP) {	
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
    OUTREG(RADEON_FP_HORZ_STRETCH,      restore->fp_horz_stretch);
    OUTREG(RADEON_FP_VERT_STRETCH,      restore->fp_vert_stretch);
    OUTREG(RADEON_FP_GEN_CNTL,          restore->fp_gen_cntl);

    if (info->DisplayType == MT_LCD) {
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

    if (info->ChipFamily == CHIP_FAMILY_R300) {
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
    RADEONInfoPtr         info = RADEONPTR(pScrn);
    DevUnion             *pPriv;
    RADEONEntPtr          pRADEONEnt;
    static RADEONSaveRec  restore0;

    /* For Non-dual head card, we don't have private field in the Entity */
    if (!info->HasCRTC2) {
	RADEONRestoreCommonRegisters(pScrn, restore);
	RADEONRestoreCrtcRegisters(pScrn, restore);
	if ((info->DisplayType == MT_DFP) ||
	    (info->DisplayType == MT_LCD)) {
	    RADEONRestoreFPRegisters(pScrn, restore);
	}
	RADEONRestorePLLRegisters(pScrn, restore);
	return;
    }

    pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gRADEONEntityIndex);
    pRADEONEnt = pPriv->ptr;

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
	if (!pRADEONEnt->RestorePrimary)
	    RADEONRestoreCommonRegisters(pScrn, restore);
	RADEONRestoreCrtc2Registers(pScrn, restore);
	RADEONRestorePLL2Registers(pScrn, restore);

	if(info->IsSwitching) return;

	pRADEONEnt->IsSecondaryRestored = TRUE;

	if (pRADEONEnt->RestorePrimary) {
	    RADEONInfoPtr info0 = RADEONPTR(pRADEONEnt->pPrimaryScrn);
	    pRADEONEnt->RestorePrimary = FALSE;

	    RADEONRestoreCrtcRegisters(pScrn, &restore0);
	    if ((info0->DisplayType == MT_DFP) ||
		(info0->DisplayType == MT_LCD)) {
		RADEONRestoreFPRegisters(pScrn, &restore0);
	    }

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
	    if ((info->DisplayType == MT_DFP) ||
		(info->DisplayType == MT_LCD)) {
		RADEONRestoreFPRegisters(pScrn, restore);
	    }
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
    if (info->IsSecondary) {
	RADEONSaveCrtc2Registers(pScrn, save);
	RADEONSavePLL2Registers(pScrn, save);
    } else {
	RADEONSavePLLRegisters(pScrn, save);
	RADEONSaveCommonRegisters(pScrn, save);
	RADEONSaveCrtcRegisters(pScrn, save);

	if ((info->DisplayType == MT_DFP) ||
	    (info->DisplayType == MT_LCD)) {
	    RADEONSaveFPRegisters(pScrn, save);
	}

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
	vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_ALL); /* Save mode
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

#if 0
    /* M6 card has trouble restoring text mode for its CRT.
     * This is fixed elsewhere and will be removed in the future.
     */
    if ((xf86IsEntityShared(pScrn->entityList[0]) || info->Clone)
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
	DevUnion     *pPriv;
	RADEONEntPtr  pRADEONEnt;
	ScrnInfoPtr   pScrn0;
	vgaHWPtr      hwp0;

	pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
				     gRADEONEntityIndex);
	pRADEONEnt = pPriv->ptr;

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
    int  bytpp;
    int  hsync_fudge_default[] = { 0x00, 0x12, 0x09, 0x09, 0x06, 0x05 };
    int  hsync_fudge_fp[]      = { 0x02, 0x02, 0x00, 0x00, 0x05, 0x05 };

    switch (info->CurrentLayout.pixel_code) {
    case 4:  format = 1; bytpp = 0; break;
    case 8:  format = 2; bytpp = 1; break;
    case 15: format = 3; bytpp = 2; break;      /*  555 */
    case 16: format = 4; bytpp = 2; break;      /*  565 */
    case 24: format = 5; bytpp = 3; break;      /*  RGB */
    case 32: format = 6; bytpp = 4; break;      /* xRGB */
    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Unsupported pixel depth (%d)\n",
		   info->CurrentLayout.bitsPerPixel);
	return FALSE;
    }
    RADEONTRACE(("Format = %d (%d bytes per pixel)\n", format, bytpp));

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

    save->surface_cntl = 0;

#if X_BYTE_ORDER == X_BIG_ENDIAN
    switch (pScrn->bitsPerPixel) {
    case 16:
	save->surface_cntl |= RADEON_NONSURF_AP0_SWP_16BPP;
	break;

    case 32:
	save->surface_cntl |= RADEON_NONSURF_AP0_SWP_32BPP;
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

    int  format;
    int  hsync_start;
    int  hsync_wid;
    int  hsync_fudge;
    int  vsync_wid;
    int  bytpp;
    int  hsync_fudge_default[] = { 0x00, 0x12, 0x09, 0x09, 0x06, 0x05 };

    switch (info->CurrentLayout.pixel_code) {
    case 4:  format = 1; bytpp = 0; break;
    case 8:  format = 2; bytpp = 1; break;
    case 15: format = 3; bytpp = 2; break;      /*  555 */
    case 16: format = 4; bytpp = 2; break;      /*  565 */
    case 24: format = 5; bytpp = 3; break;      /*  RGB */
    case 32: format = 6; bytpp = 4; break;      /* xRGB */
    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Unsupported pixel depth (%d)\n",
		   info->CurrentLayout.bitsPerPixel);
	return FALSE;
    }
    RADEONTRACE(("Format = %d (%d bytes per pixel)\n", format, bytpp));

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
    if (info->ChipFamily == CHIP_FAMILY_R200 ||
	info->ChipFamily == CHIP_FAMILY_R300) {
	save->disp_output_cntl =
	    ((info->SavedReg.disp_output_cntl
	      & ~(CARD32)RADEON_DISP_DAC_SOURCE_MASK)
	     | RADEON_DISP_DAC_SOURCE_CRTC2);
    } else {
	save->disp_hw_debug = info->SavedReg.disp_hw_debug;
	if (info->IsDell && info->DellType == 2) {
	    if (info->DisplayType == MT_CRT || info->CloneType == MT_CRT) {
		/* Turn on 2nd CRT */
		save->dac2_cntl &= ~RADEON_DAC2_DAC_CLK_SEL;
		save->dac2_cntl |= RADEON_DAC2_DAC2_CLK_SEL;
		save->disp_hw_debug &= ~RADEON_CRT2_DISP1_SEL; 

		/* This will make 2nd CRT stay on in console */
		info->SavedReg.dac2_cntl = save->dac2_cntl;
		info->SavedReg.disp_hw_debug |= RADEON_CRT2_DISP1_SEL;
		info->SavedReg.crtc2_gen_cntl |= RADEON_CRTC2_CRT2_ON;
	    }
	} else save->dac2_cntl |= RADEON_DAC2_DAC_CLK_SEL;
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

    if (info->DisplayType == MT_DFP || info->CloneType == MT_DFP) {
	save->crtc2_gen_cntl      = (RADEON_CRTC2_EN | (format << 8));
	save->fp2_h_sync_strt_wid = save->crtc2_h_sync_strt_wid;
	save->fp2_v_sync_strt_wid = save->crtc2_v_sync_strt_wid;
	save->fp2_gen_cntl        = (RADEON_FP2_SEL_CRTC2 |
				     RADEON_FP2_PANEL_FORMAT |
				     RADEON_FP2_ON);

	if (pScrn->rgbBits == 8) 
	    save->fp2_gen_cntl |= RADEON_FP2_PANEL_FORMAT; /* 24 bit format */
	else
	    save->fp2_gen_cntl &= ~RADEON_FP2_PANEL_FORMAT;/* 18 bit format */

	/* FIXME: When there are two DFPs, the 2nd DFP is driven by the
	 *        external TMDS transmitter.  It may have a problem at
	 *        high dot clock for certain panels.  Since we don't
	 *        know how to control the external TMDS transmitter, not
	 *        much we can do here.
	 */
#if 0
	if (save->dot_clock_freq > 15000)
	    save->tmds_pll_cntl = 0xA3F;
	else if(save->tmds_pll_cntl != 0xA3F)
	    save->tmds_pll_cntl = info->SavedReg.tmds_pll_cntl;
#endif

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

    /* This is needed for some panel at high resolution (>=1600x1200) 
     */
    if ((save->dot_clock_freq > 15000) &&
	(info->ChipFamily != CHIP_FAMILY_R300))
	save->tmds_pll_cntl = 0xA3F;
    else
	save->tmds_pll_cntl = orig->tmds_pll_cntl;

    info->PanelOff = FALSE;
    /* This option is used to force the ONLY DEVICE in XFConfig to use
     * CRT port, instead of default DVI port.
     */
    if (xf86ReturnOptValBool(info->Options, OPTION_PANEL_OFF, FALSE)) {
	info->PanelOff = TRUE;
    }

    if (info->PanelOff && info->Clone) {
	info->OverlayOnCRTC2 = TRUE;
	if (info->DisplayType == MT_LCD) {
	    /* Turning off LVDS_ON seems to make panel white blooming.
	     * For now we just turn off display data ???
	     */
	    save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_DISPLAY_DIS);
	    save->lvds_gen_cntl &= ~(RADEON_LVDS_BLON);

	} else if (info->DisplayType == MT_DFP)
	    save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
    } else {
	if (info->DisplayType == MT_LCD) {
	    save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
	    save->fp_gen_cntl   &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
	} else if (info->DisplayType == MT_DFP)
	    save->fp_gen_cntl   |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
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

    if (info->IsSecondary) {
	if (!RADEONInitCrtc2Registers(pScrn, save, mode, info))
	    return FALSE;
	RADEONInitPLL2Registers(save, &info->pll, dot_clock);
    } else {
	RADEONInitCommonRegisters(save, info);
	if (!RADEONInitCrtcRegisters(pScrn, save, mode, info))
	    return FALSE;
	dot_clock = mode->Clock/1000.0;
	if (dot_clock) {
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

    if (((info->DisplayType == MT_DFP) ||
	 (info->DisplayType == MT_LCD))) {
	RADEONInitFPRegisters(pScrn, &info->SavedReg, save, mode, info);
    }

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

/* Used to disallow modes that are not supported by the hardware */
int RADEONValidMode(int scrnIndex, DisplayModePtr mode,
		    Bool verbose, int flag)
{
    /* Searching for native mode timing table embedded in BIOS image.
     * Not working yet. Currently we calculate from FP registers
     */

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

	if (pSAREAPriv->pfCurrentPage == 1) {
	    Base += info->backOffset;
	}

	if (clone || info->IsSecondary) {
	    pSAREAPriv->crtc2_base = Base;
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
	RADEONUnmapMem(pScrn);
    }

    if (info->accel) XAADestroyInfoRec(info->accel);
    info->accel = NULL;

    if (info->scratch_save) xfree(info->scratch_save);
    info->scratch_save = NULL;

    if (info->cursor) xf86DestroyCursorInfoRec(info->cursor);
    info->cursor = NULL;

    if (info->DGAModes) xfree(info->DGAModes);
    info->DGAModes = NULL;

    if (info->CloneModes)
	while (info->CloneModes)
	    xf86DeleteMode(&info->CloneModes, info->CloneModes);

    pScrn->vtSema = FALSE;

    xf86ClearPrimInitDone(pScrn->entityList[0]);

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
    }

#ifdef XF86DRI
    if (info->CPStarted) DRIUnlock(pScrn->pScreen);
#endif
}
