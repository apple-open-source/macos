/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/r128_driver.c,v 1.89 2004/01/29 03:37:16 dawes Exp $ */
/*
 * Copyright 1999, 2000 ATI Technologies Inc., Markham, Ontario,
 *                      Precision Insight, Inc., Cedar Park, Texas, and
 *                      VA Linux Systems Inc., Fremont, California.
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
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, PRECISION INSIGHT, VA LINUX
 * SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Rickard E. Faith <faith@valinux.com>
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 * Credits:
 *
 *   Thanks to Alan Hourihane <alanh@fairlite.demon..co.uk> and SuSE for
 *   providing source code to their 3.3.x Rage 128 driver.  Portions of
 *   this file are based on the initialization code for that driver.
 *
 * References:
 *
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 * This server does not yet support these XFree86 4.0 features:
 *   DDC1 & DDC2
 *   shadowfb
 *   overlay planes
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */


				/* Driver data structures */
#include "r128.h"
#include "r128_probe.h"
#include "r128_reg.h"
#include "r128_version.h"

#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "r128_dri.h"
#include "r128_sarea.h"
#endif

#include "fb.h"

				/* colormap initialization */
#include "micmap.h"

				/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86PciInfo.h"
#include "xf86RAC.h"
#include "xf86Resources.h"
#include "xf86cmap.h"
#include "xf86xv.h"
#include "vbe.h"

				/* fbdevhw & vgahw */
#include "fbdevhw.h"
#include "vgaHW.h"
#include "dixstruct.h"

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define USE_CRT_ONLY	0

				/* Forward definitions for driver functions */
static Bool R128CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool R128SaveScreen(ScreenPtr pScreen, int mode);
static void R128Save(ScrnInfoPtr pScrn);
static void R128Restore(ScrnInfoPtr pScrn);
static Bool R128ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void R128DisplayPowerManagementSet(ScrnInfoPtr pScrn,
					  int PowerManagementMode, int flags);
static void R128DisplayPowerManagementSetLCD(ScrnInfoPtr pScrn,
					  int PowerManagementMode, int flags);

typedef enum {
  OPTION_NOACCEL,
  OPTION_SW_CURSOR,
  OPTION_DAC_6BIT,
  OPTION_DAC_8BIT,
#ifdef XF86DRI
  OPTION_XV_DMA,
  OPTION_IS_PCI,
  OPTION_CCE_PIO,
  OPTION_NO_SECURITY,
  OPTION_USEC_TIMEOUT,
  OPTION_AGP_MODE,
  OPTION_AGP_SIZE,
  OPTION_RING_SIZE,
  OPTION_BUFFER_SIZE,
  OPTION_PAGE_FLIP,
#endif
#if USE_CRT_ONLY
  /* FIXME: Disable CRTOnly until it is tested */
  OPTION_CRT,
#endif
  OPTION_DISPLAY,
  OPTION_PANEL_WIDTH,
  OPTION_PANEL_HEIGHT,
  OPTION_PROG_FP_REGS,
  OPTION_FBDEV,
  OPTION_VIDEO_KEY,
  OPTION_SHOW_CACHE
} R128Opts;

const OptionInfoRec R128Options[] = {
  { OPTION_NOACCEL,      "NoAccel",          OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_SW_CURSOR,    "SWcursor",         OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_DAC_6BIT,     "Dac6Bit",          OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_DAC_8BIT,     "Dac8Bit",          OPTV_BOOLEAN, {0}, TRUE  },
#ifdef XF86DRI
  { OPTION_XV_DMA,       "DMAForXv",         OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_IS_PCI,       "ForcePCIMode",     OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_CCE_PIO,      "CCEPIOMode",       OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_NO_SECURITY,  "CCENoSecurity",    OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_USEC_TIMEOUT, "CCEusecTimeout",   OPTV_INTEGER, {0}, FALSE },
  { OPTION_AGP_MODE,     "AGPMode",          OPTV_INTEGER, {0}, FALSE },
  { OPTION_AGP_SIZE,     "AGPSize",          OPTV_INTEGER, {0}, FALSE },
  { OPTION_RING_SIZE,    "RingSize",         OPTV_INTEGER, {0}, FALSE },
  { OPTION_BUFFER_SIZE,  "BufferSize",       OPTV_INTEGER, {0}, FALSE },
  { OPTION_PAGE_FLIP,    "EnablePageFlip",   OPTV_BOOLEAN, {0}, FALSE },
#endif
  { OPTION_DISPLAY,      "Display",          OPTV_STRING,  {0}, FALSE },
  { OPTION_PANEL_WIDTH,  "PanelWidth",       OPTV_INTEGER, {0}, FALSE },
  { OPTION_PANEL_HEIGHT, "PanelHeight",      OPTV_INTEGER, {0}, FALSE },
  { OPTION_PROG_FP_REGS, "ProgramFPRegs",    OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_FBDEV,        "UseFBDev",         OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_VIDEO_KEY,    "VideoKey",         OPTV_INTEGER, {0}, FALSE },
  { OPTION_SHOW_CACHE,   "ShowCache",        OPTV_BOOLEAN, {0}, FALSE },
  { -1,                  NULL,               OPTV_NONE,    {0}, FALSE }
};

R128RAMRec R128RAM[] = {        /* Memory Specifications
				   From RAGE 128 Software Development
				   Manual (Technical Reference Manual P/N
				   SDK-G04000 Rev 0.01), page 3-21.  */
    { 4, 4, 3, 3, 1, 3, 1, 16, 12, "128-bit SDR SGRAM 1:1" },
    { 4, 8, 3, 3, 1, 3, 1, 17, 13, "64-bit SDR SGRAM 1:1" },
    { 4, 4, 1, 2, 1, 2, 1, 16, 12, "64-bit SDR SGRAM 2:1" },
    { 4, 4, 3, 3, 2, 3, 1, 16, 12, "64-bit DDR SGRAM" },
};

static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIndex",
    "vgaHWLock",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWUnlock",
    NULL
};

static const char *fbdevHWSymbols[] = {
    "fbdevHWInit",
    "fbdevHWUseBuildinMode",
    "fbdevHWGetLineLength",
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

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *xaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

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
    "drmAgpUnbind",
    "drmAgpVendorId",
    "drmAvailable",
    "drmCommandNone",
    "drmCommandRead",
    "drmCommandWrite",
    "drmCommandWriteRead",
    "drmCtlInstHandler",
    "drmCtlUninstHandler",
    "drmFreeBufs",
    "drmFreeVersion",
    "drmGetInterruptFromBusID",
    "drmGetLibVersion",
    "drmGetVersion",
    "drmMap",
    "drmMapBufs",
    "drmDMA",
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
    "vbeFree",
    NULL
};

static const char *int10Symbols[] = {
    "xf86InitInt10",
    "xf86FreeInt10",
    "xf86int10Addr",
    NULL
};

void R128LoaderRefSymLists(void)
{
    /*
     * Tell the loader about symbols from other modules that this module might
     * refer to.
     */
    xf86LoaderRefSymLists(vgahwSymbols,
		      fbSymbols,
		      xaaSymbols,
		      ramdacSymbols,
#ifdef XF86DRI
		      drmSymbols,
		      driSymbols,
		      driShadowFBSymbols,
#endif
		      fbdevHWSymbols,
		      int10Symbols,
		      vbeSymbols,
		      /* ddcsymbols, */
		      i2cSymbols,
		      /* shadowSymbols, */
		      NULL);
}

/* Allocate our private R128InfoRec. */
static Bool R128GetRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate) return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(R128InfoRec), 1);
    return TRUE;
}

/* Free our private R128InfoRec. */
static void R128FreeRec(ScrnInfoPtr pScrn)
{
    if (!pScrn || !pScrn->driverPrivate) return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

/* Memory map the MMIO region.  Used during pre-init and by R128MapMem,
   below. */
static Bool R128MapMMIO(ScrnInfoPtr pScrn)
{
    R128InfoPtr info          = R128PTR(pScrn);

    if (info->FBDev) {
	info->MMIO = fbdevHWMapMMIO(pScrn);
    } else {
	info->MMIO = xf86MapPciMem(pScrn->scrnIndex,
				   VIDMEM_MMIO | VIDMEM_READSIDEEFFECT,
				   info->PciTag,
				   info->MMIOAddr,
				   R128_MMIOSIZE);
    }

    if (!info->MMIO) return FALSE;
    return TRUE;
}

/* Unmap the MMIO region.  Used during pre-init and by R128UnmapMem,
   below. */
static Bool R128UnmapMMIO(ScrnInfoPtr pScrn)
{
    R128InfoPtr info          = R128PTR(pScrn);

    if (info->FBDev)
	fbdevHWUnmapMMIO(pScrn);
    else {
	xf86UnMapVidMem(pScrn->scrnIndex, info->MMIO, R128_MMIOSIZE);
    }
    info->MMIO = NULL;
    return TRUE;
}

/* Memory map the frame buffer.  Used by R128MapMem, below. */
static Bool R128MapFB(ScrnInfoPtr pScrn)
{
    R128InfoPtr info          = R128PTR(pScrn);

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

/* Unmap the frame buffer.  Used by R128UnmapMem, below. */
static Bool R128UnmapFB(ScrnInfoPtr pScrn)
{
    R128InfoPtr info          = R128PTR(pScrn);

    if (info->FBDev)
	fbdevHWUnmapVidmem(pScrn);
    else
	xf86UnMapVidMem(pScrn->scrnIndex, info->FB, info->FbMapSize);
    info->FB = NULL;
    return TRUE;
}

/* Memory map the MMIO region and the frame buffer. */
static Bool R128MapMem(ScrnInfoPtr pScrn)
{
    if (!R128MapMMIO(pScrn)) return FALSE;
    if (!R128MapFB(pScrn)) {
	R128UnmapMMIO(pScrn);
	return FALSE;
    }
    return TRUE;
}

/* Unmap the MMIO region and the frame buffer. */
static Bool R128UnmapMem(ScrnInfoPtr pScrn)
{
    if (!R128UnmapMMIO(pScrn) || !R128UnmapFB(pScrn)) return FALSE;
    return TRUE;
}

/* Read PLL information */
unsigned R128INPLL(ScrnInfoPtr pScrn, int addr)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTREG8(R128_CLOCK_CNTL_INDEX, addr & 0x1f);
    return INREG(R128_CLOCK_CNTL_DATA);
}

#if 0
/* Read PAL information (only used for debugging). */
static int R128INPAL(int idx)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTREG(R128_PALETTE_INDEX, idx << 16);
    return INREG(R128_PALETTE_DATA);
}
#endif

/* Wait for vertical sync. */
void R128WaitForVerticalSync(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           i;

    OUTREG(R128_GEN_INT_STATUS, R128_VSYNC_INT_AK);
    for (i = 0; i < R128_TIMEOUT; i++) {
	if (INREG(R128_GEN_INT_STATUS) & R128_VSYNC_INT) break;
    }
}

/* Blank screen. */
static void R128Blank(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    if(info->isDFP)
        OUTREGP(R128_FP_GEN_CNTL, R128_FP_BLANK_DIS, ~R128_FP_BLANK_DIS);
    else
        OUTREGP(R128_CRTC_EXT_CNTL, R128_CRTC_DISPLAY_DIS, ~R128_CRTC_DISPLAY_DIS);
}

/* Unblank screen. */
static void R128Unblank(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    if(info->isDFP)
        OUTREGP(R128_FP_GEN_CNTL, 0, ~R128_FP_BLANK_DIS);
    else
        OUTREGP(R128_CRTC_EXT_CNTL, 0, ~(R128_CRTC_DISPLAY_DIS |
					 R128_CRTC_VSYNC_DIS |
					 R128_CRTC_HSYNC_DIS));
}

/* Compute log base 2 of val. */
int R128MinBits(int val)
{
    int bits;

    if (!val) return 1;
    for (bits = 0; val; val >>= 1, ++bits);
    return bits;
}

/* Compute n/d with rounding. */
static int R128Div(int n, int d)
{
    return (n + (d / 2)) / d;
}

/* Read the Video BIOS block and the FP registers (if applicable). */
static Bool R128GetBIOSParameters(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    R128InfoPtr info = R128PTR(pScrn);
    int         i;
    int         FPHeader = 0;

#define R128_BIOS8(v)  (info->VBIOS[v])
#define R128_BIOS16(v) (info->VBIOS[v] | \
			(info->VBIOS[(v) + 1] << 8))
#define R128_BIOS32(v) (info->VBIOS[v] | \
			(info->VBIOS[(v) + 1] << 8) | \
			(info->VBIOS[(v) + 2] << 16) | \
			(info->VBIOS[(v) + 3] << 24))

    if (!(info->VBIOS = xalloc(R128_VBIOS_SIZE))) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Cannot allocate space for hold Video BIOS!\n");
	return FALSE;
    }

    if (pInt10) {
	info->BIOSAddr = pInt10->BIOSseg << 4;
	(void)memcpy(info->VBIOS, xf86int10Addr(pInt10, info->BIOSAddr),
		     R128_VBIOS_SIZE);
    } else {
	xf86ReadPciBIOS(0, info->PciTag, 0, info->VBIOS, R128_VBIOS_SIZE);
	if (info->VBIOS[0] != 0x55 || info->VBIOS[1] != 0xaa) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Video BIOS not detected in PCI space!\n");
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Attempting to read Video BIOS from legacy ISA space!\n");
	    info->BIOSAddr = 0x000c0000;
	    xf86ReadDomainMemory(info->PciTag, info->BIOSAddr, R128_VBIOS_SIZE, info->VBIOS);
	}
    }
    if (info->VBIOS[0] != 0x55 || info->VBIOS[1] != 0xaa) {
	info->BIOSAddr = 0x00000000;
	xfree(info->VBIOS);
	info->VBIOS = NULL;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Video BIOS not found!\n");
    }

    if (info->VBIOS && info->HasPanelRegs) {
	info->FPBIOSstart = 0;

	/* FIXME: There should be direct access to the start of the FP info
	   tables, but until we find out where that offset is stored, we
	   must search for the ATI signature string: "M3      ". */
	for (i = 4; i < R128_VBIOS_SIZE-8; i++) {
	    if (R128_BIOS8(i)   == 'M' &&
		R128_BIOS8(i+1) == '3' &&
		R128_BIOS8(i+2) == ' ' &&
		R128_BIOS8(i+3) == ' ' &&
		R128_BIOS8(i+4) == ' ' &&
		R128_BIOS8(i+5) == ' ' &&
		R128_BIOS8(i+6) == ' ' &&
		R128_BIOS8(i+7) == ' ') {
		FPHeader = i-2;
		break;
	    }
	}

	if (!FPHeader) return TRUE;

	/* Assume that only one panel is attached and supported */
	for (i = FPHeader+20; i < FPHeader+84; i += 2) {
	    if (R128_BIOS16(i) != 0) {
		info->FPBIOSstart = R128_BIOS16(i);
		break;
	    }
	}
	if (!info->FPBIOSstart) return TRUE;

	if (!info->PanelXRes)
	    info->PanelXRes = R128_BIOS16(info->FPBIOSstart+25);
	if (!info->PanelYRes)
	    info->PanelYRes = R128_BIOS16(info->FPBIOSstart+27);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel size: %dx%d\n",
		   info->PanelXRes, info->PanelYRes);

	info->PanelPwrDly = R128_BIOS8(info->FPBIOSstart+56);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel ID: ");
	for (i = 1; i <= 24; i++)
	    ErrorF("%c", R128_BIOS8(info->FPBIOSstart+i));
	ErrorF("\n");
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel Type: ");
	i = R128_BIOS16(info->FPBIOSstart+29);
	if (i & 1) ErrorF("Color, ");
	else       ErrorF("Monochrome, ");
	if (i & 2) ErrorF("Dual(split), ");
	else       ErrorF("Single, ");
	switch ((i >> 2) & 0x3f) {
	case 0:  ErrorF("STN");        break;
	case 1:  ErrorF("TFT");        break;
	case 2:  ErrorF("Active STN"); break;
	case 3:  ErrorF("EL");         break;
	case 4:  ErrorF("Plasma");     break;
	default: ErrorF("UNKNOWN");    break;
	}
	ErrorF("\n");
	if (R128_BIOS8(info->FPBIOSstart+61) & 1) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel Interface: LVDS\n");
	} else {
	    /* FIXME: Add Non-LVDS flat pael support */
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Non-LVDS panel interface detected!  "
		       "This support is untested and may not "
		       "function properly\n");
	}
    }

    if (!info->PanelXRes || !info->PanelYRes) {
        info->HasPanelRegs = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Can't determine panel dimensions, and none specified. \
			      Disabling programming of FP registers.\n");
    }

    return TRUE;
}

/* Read PLL parameters from BIOS block.  Default to typical values if there
   is no BIOS. */
static Bool R128GetPLLParameters(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info = R128PTR(pScrn);
    R128PLLPtr    pll  = &info->pll;

#if defined(__powerpc__) || defined(__alpha__)
    /* there is no bios under Linux PowerPC but Open Firmware
       does set up the PLL registers properly and we can use
       those to calculate xclk and find the reference divider */

    unsigned x_mpll_ref_fb_div;
    unsigned xclk_cntl;
    unsigned Nx, M;
    unsigned PostDivSet[] = {0, 1, 2, 4, 8, 3, 6, 12};

    /* Assume REF clock is 2950 (in units of 10khz) */
    /* and that all pllclk must be between 125 Mhz and 250Mhz */
    pll->reference_freq = 2950;
    pll->min_pll_freq   = 12500;
    pll->max_pll_freq   = 25000;

    /* need to memory map the io to use INPLL since it
       has not been done yet at this point in the startup */
    R128MapMMIO(pScrn);
    x_mpll_ref_fb_div = INPLL(pScrn, R128_X_MPLL_REF_FB_DIV);
    xclk_cntl = INPLL(pScrn, R128_XCLK_CNTL) & 0x7;
    pll->reference_div =
	INPLL(pScrn,R128_PPLL_REF_DIV) & R128_PPLL_REF_DIV_MASK;
    /* unmap it again */
    R128UnmapMMIO(pScrn);

    Nx = (x_mpll_ref_fb_div & 0x00FF00) >> 8;
    M =  (x_mpll_ref_fb_div & 0x0000FF);

    pll->xclk =  R128Div((2 * Nx * pll->reference_freq),
			 (M * PostDivSet[xclk_cntl]));

#else /* !defined(__powerpc__) */

    if (!info->VBIOS) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Video BIOS not detected, using default PLL parameters!\n");
				/* These probably aren't going to work for
				   the card you are using.  Specifically,
				   reference freq can be 29.50MHz,
				   28.63MHz, or 14.32MHz.  YMMV. */
	pll->reference_freq = 2950;
	pll->reference_div  = 65;
	pll->min_pll_freq   = 12500;
	pll->max_pll_freq   = 25000;
	pll->xclk           = 10300;
    } else {
	CARD16 bios_header    = R128_BIOS16(0x48);
	CARD16 pll_info_block = R128_BIOS16(bios_header + 0x30);
	R128TRACE(("Header at 0x%04x; PLL Information at 0x%04x\n",
		   bios_header, pll_info_block));

	pll->reference_freq = R128_BIOS16(pll_info_block + 0x0e);
	pll->reference_div  = R128_BIOS16(pll_info_block + 0x10);
	pll->min_pll_freq   = R128_BIOS32(pll_info_block + 0x12);
	pll->max_pll_freq   = R128_BIOS32(pll_info_block + 0x16);
	pll->xclk           = R128_BIOS16(pll_info_block + 0x08);
    }
#endif /* __powerpc__ */

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "PLL parameters: rf=%d rd=%d min=%d max=%d; xclk=%d\n",
	       pll->reference_freq,
	       pll->reference_div,
	       pll->min_pll_freq,
	       pll->max_pll_freq,
	       pll->xclk);

    return TRUE;
}

/* This is called by R128PreInit to set up the default visual. */
static Bool R128PreInitVisual(ScrnInfoPtr pScrn)
{
    R128InfoPtr info          = R128PTR(pScrn);

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, (Support24bppFb
					  | Support32bppFb
					  | SupportConvert32to24
					  )))
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
		   pScrn->depth, R128_DRIVER_NAME);
	return FALSE;
    }

    xf86PrintDepthBpp(pScrn);

    info->fifo_slots  = 0;
    info->pix24bpp    = xf86GetBppFromDepth(pScrn, pScrn->depth);
    info->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    info->CurrentLayout.depth        = pScrn->depth;
    info->CurrentLayout.pixel_bytes  = pScrn->bitsPerPixel / 8;
    info->CurrentLayout.pixel_code   = (pScrn->bitsPerPixel != 16
				       ? pScrn->bitsPerPixel
				       : pScrn->depth);

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

/* This is called by R128PreInit to handle all color weight issues. */
static Bool R128PreInitWeight(ScrnInfoPtr pScrn)
{
    R128InfoPtr info          = R128PTR(pScrn);

				/* Save flag for 6 bit DAC to use for
				   setting CRTC registers.  Otherwise use
				   an 8 bit DAC, even if xf86SetWeight sets
				   pScrn->rgbBits to some value other than
				   8. */
    info->dac6bits = FALSE;
    if (pScrn->depth > 8) {
	rgb defaultWeight = { 0, 0, 0 };
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

/* This is called by R128PreInit to handle config file overrides for things
   like chipset and memory regions.  Also determine memory size and type.
   If memory type ever needs an override, put it in this routine. */
static Bool R128PreInitConfig(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    EntityInfoPtr pEnt      = info->pEnt;
    GDevPtr       dev       = pEnt->device;
    int           offset    = 0;        /* RAM Type */
    MessageType   from;

				/* Chipset */
    from = X_PROBED;
    if (dev->chipset && *dev->chipset) {
	info->Chipset  = xf86StringToToken(R128Chipsets, dev->chipset);
	from           = X_CONFIG;
    } else if (dev->chipID >= 0) {
	info->Chipset  = dev->chipID;
	from           = X_CONFIG;
    } else {
	info->Chipset = info->PciInfo->chipType;
    }
    pScrn->chipset = (char *)xf86TokenToString(R128Chipsets, info->Chipset);

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

				/* Framebuffer */

    from             = X_PROBED;
    info->LinearAddr = info->PciInfo->memBase[0] & 0xfc000000;
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

				/* MMIO registers */
    from             = X_PROBED;
    info->MMIOAddr   = info->PciInfo->memBase[2] & 0xffffff00;
    if (dev->IOBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "MMIO address override, using 0x%08lx instead of 0x%08lx\n",
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

				/* Flat panel (part 1) */
    if (xf86GetOptValBool(info->Options, OPTION_PROG_FP_REGS,
			  &info->HasPanelRegs)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "Turned flat panel register programming %s\n",
		   info->HasPanelRegs ? "on" : "off");
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "\n\nWARNING: Forcing the driver to use/not use the flat panel registers\nmight damage your flat panel.  Use at your *OWN* *RISK*.\n\n");
    } else {
        info->isDFP = FALSE;
        info->isPro2 = FALSE;
	switch (info->Chipset) {
	/* R128 Pro and Pro2 can have DFP, we will deal with it.
	   No support for dual-head/xinerama yet.
           M3 can also have DFP, no support for now */
	case PCI_CHIP_RAGE128TF:
	case PCI_CHIP_RAGE128TL:
	case PCI_CHIP_RAGE128TR:
	/* FIXME: RAGE128 TS/TT/TU are assumed to be PRO2 as all 6 chips came
	 *        out at the same time, so are of the same family likely.
	 *        This requires confirmation however to be fully correct.
	 *        Mike A. Harris <mharris@redhat.com>
	 */
	case PCI_CHIP_RAGE128TS:
	case PCI_CHIP_RAGE128TT:
	case PCI_CHIP_RAGE128TU: info->isPro2 = TRUE;
	/* FIXME: RAGE128 P[ABCEGHIJKLMNOQSTUVWX] are assumed to have DFP
	 *        capability, as the comment at the top suggests.
	 *        This requires confirmation however to be fully correct.
	 *        Mike A. Harris <mharris@redhat.com>
	 */
	case PCI_CHIP_RAGE128PA:
	case PCI_CHIP_RAGE128PB:
	case PCI_CHIP_RAGE128PC:
	case PCI_CHIP_RAGE128PE:
	case PCI_CHIP_RAGE128PG:
	case PCI_CHIP_RAGE128PH:
	case PCI_CHIP_RAGE128PI:
	case PCI_CHIP_RAGE128PJ:
	case PCI_CHIP_RAGE128PK:
	case PCI_CHIP_RAGE128PL:
	case PCI_CHIP_RAGE128PM:
	case PCI_CHIP_RAGE128PN:
	case PCI_CHIP_RAGE128PO:
	case PCI_CHIP_RAGE128PQ:
	case PCI_CHIP_RAGE128PS:
	case PCI_CHIP_RAGE128PT:
	case PCI_CHIP_RAGE128PU:
	case PCI_CHIP_RAGE128PV:
	case PCI_CHIP_RAGE128PW:
	case PCI_CHIP_RAGE128PX:

	case PCI_CHIP_RAGE128PD:
	case PCI_CHIP_RAGE128PF:
	case PCI_CHIP_RAGE128PP:
	case PCI_CHIP_RAGE128PR: info->isDFP = TRUE; break;

	case PCI_CHIP_RAGE128LE:
	case PCI_CHIP_RAGE128LF:
	case PCI_CHIP_RAGE128MF:
	case PCI_CHIP_RAGE128ML: info->HasPanelRegs = TRUE;  break;
	case PCI_CHIP_RAGE128RE:
	case PCI_CHIP_RAGE128RF:
	case PCI_CHIP_RAGE128RG:
	case PCI_CHIP_RAGE128RK:
	case PCI_CHIP_RAGE128RL:
	case PCI_CHIP_RAGE128SM:
	/* FIXME: RAGE128 S[EFGHKLN] are assumed to be like the SM above as
	 *        all of them are listed as "Rage 128 4x" in ATI docs.
	 *        This requires confirmation however to be fully correct.
	 *        Mike A. Harris <mharris@redhat.com>
	 */
	case PCI_CHIP_RAGE128SE:
	case PCI_CHIP_RAGE128SF:
	case PCI_CHIP_RAGE128SG:
	case PCI_CHIP_RAGE128SH:
	case PCI_CHIP_RAGE128SK:
	case PCI_CHIP_RAGE128SL:
	case PCI_CHIP_RAGE128SN:
	default:                 info->HasPanelRegs = FALSE; break;
	}
    }

				/* Read registers used to determine options */
    from                      = X_PROBED;
    R128MapMMIO(pScrn);
    R128MMIO                  = info->MMIO;

    if (info->FBDev)
	pScrn->videoRam       = fbdevHWGetVidmem(pScrn) / 1024;
    else
	pScrn->videoRam       = INREG(R128_CONFIG_MEMSIZE) / 1024;

    info->MemCntl             = INREG(R128_MEM_CNTL);
    info->BusCntl             = INREG(R128_BUS_CNTL);

    /* On non-flat panel systems, the default is to display to the CRT,
       and on flat panel systems, the default is to display to the flat
       panel unless the user explicity chooses otherwise using the "Display"
       config file setting.  BIOS_5_SCRATCH holds the display device on flat
       panel systems only. */
    if (info->HasPanelRegs) {
        char *Display = xf86GetOptValString(info->Options, OPTION_DISPLAY);

	if (info->FBDev)
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		     "Option \"Display\" ignored "
		     "(framebuffer device determines display type)\n");
	else if (!Display || !xf86NameCmp(Display, "FP"))
	    info->BIOSDisplay = R128_BIOS_DISPLAY_FP;
	else if (!xf86NameCmp(Display, "BIOS"))
	    info->BIOSDisplay = INREG8(R128_BIOS_5_SCRATCH);
	else if (!xf86NameCmp(Display, "Mirror"))
	    info->BIOSDisplay = R128_BIOS_DISPLAY_FP_CRT;
	else if (!xf86NameCmp(Display, "CRT"))
	    info->BIOSDisplay = R128_BIOS_DISPLAY_CRT;
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Unsupported type \"%s\" specified for Option \"Display\".\n"
		"\tSupported types are: "
		"\"BIOS\", \"Mirror\", \"CRT\" and \"FP\"\n", Display);
	    return FALSE;
	}
    } else {
	info->BIOSDisplay     = R128_BIOS_DISPLAY_CRT;
    }

    R128MMIO                  = NULL;
    R128UnmapMMIO(pScrn);

				/* RAM */
    switch (info->MemCntl & 0x3) {
    case 0:                     /* SDR SGRAM 1:1 */
	switch (info->Chipset) {
	case PCI_CHIP_RAGE128TF:
	case PCI_CHIP_RAGE128TL:
	case PCI_CHIP_RAGE128TR:
	case PCI_CHIP_RAGE128LE:
	case PCI_CHIP_RAGE128LF:
	case PCI_CHIP_RAGE128MF:
	case PCI_CHIP_RAGE128ML:
	case PCI_CHIP_RAGE128RE:
	case PCI_CHIP_RAGE128RF:
	case PCI_CHIP_RAGE128RG: offset = 0; break; /* 128-bit SDR SGRAM 1:1 */
	case PCI_CHIP_RAGE128RK:
	case PCI_CHIP_RAGE128RL:
	case PCI_CHIP_RAGE128SM:
	default:                 offset = 1; break; /*  64-bit SDR SGRAM 1:1 */
	}
	break;
    case 1:                      offset = 2; break; /*  64-bit SDR SGRAM 2:1 */
    case 2:                      offset = 3; break; /*  64-bit DDR SGRAM     */
    default:                     offset = 1; break; /*  64-bit SDR SGRAM 1:1 */
    }
    info->ram = &R128RAM[offset];

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

				/* Flat panel (part 2) */
	switch (info->BIOSDisplay) {
	case R128_BIOS_DISPLAY_FP:
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Using flat panel for display\n");
	    break;
	case R128_BIOS_DISPLAY_CRT:
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Using external CRT for display\n");
	    break;
	case R128_BIOS_DISPLAY_FP_CRT:
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Using both flat panel and external CRT "
		       "for display\n");
	    break;
	}

    if (info->HasPanelRegs) {
				/* Panel width/height overrides */
	info->PanelXRes = 0;
	info->PanelYRes = 0;
	if (xf86GetOptValInteger(info->Options,
				 OPTION_PANEL_WIDTH, &(info->PanelXRes))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Flat panel width: %d\n", info->PanelXRes);
	}
	if (xf86GetOptValInteger(info->Options,
				 OPTION_PANEL_HEIGHT, &(info->PanelYRes))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Flat panel height: %d\n", info->PanelYRes);
	}
    }

#ifdef XF86DRI
				/* DMA for Xv */
    info->DMAForXv = xf86ReturnOptValBool(info->Options, OPTION_XV_DMA, FALSE);
    if (info->DMAForXv) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "Will try to use DMA for Xv image transfers\n");
    }

				/* AGP/PCI */
    if (xf86ReturnOptValBool(info->Options, OPTION_IS_PCI, FALSE)) {
	info->IsPCI = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into PCI-only mode\n");
    } else {
	switch (info->Chipset) {
	case PCI_CHIP_RAGE128LE:
	case PCI_CHIP_RAGE128RE:
	case PCI_CHIP_RAGE128RK:
	case PCI_CHIP_RAGE128PD:
	case PCI_CHIP_RAGE128PR:
	case PCI_CHIP_RAGE128PP: info->IsPCI = TRUE;  break;
	case PCI_CHIP_RAGE128LF:
	case PCI_CHIP_RAGE128MF:
	case PCI_CHIP_RAGE128ML:
	case PCI_CHIP_RAGE128PF:
	case PCI_CHIP_RAGE128RF:
	case PCI_CHIP_RAGE128RG:
	case PCI_CHIP_RAGE128RL:
	case PCI_CHIP_RAGE128SM:
	case PCI_CHIP_RAGE128TF:
	case PCI_CHIP_RAGE128TL:
	case PCI_CHIP_RAGE128TR:
	/* FIXME: Rage 128 S[EFGHKLN], T[STU], P[ABCEGHIJKLMNOQSTUVWX] are
	 * believed to be AGP, but need confirmation. <mharris@redhat.com>
	 */
	case PCI_CHIP_RAGE128PA:
	case PCI_CHIP_RAGE128PB:
	case PCI_CHIP_RAGE128PC:
	case PCI_CHIP_RAGE128PE:
	case PCI_CHIP_RAGE128PG:
	case PCI_CHIP_RAGE128PH:
	case PCI_CHIP_RAGE128PI:
	case PCI_CHIP_RAGE128PJ:
	case PCI_CHIP_RAGE128PK:
	case PCI_CHIP_RAGE128PL:
	case PCI_CHIP_RAGE128PM:
	case PCI_CHIP_RAGE128PN:
	case PCI_CHIP_RAGE128PO:
	case PCI_CHIP_RAGE128PQ:
	case PCI_CHIP_RAGE128PS:
	case PCI_CHIP_RAGE128PT:
	case PCI_CHIP_RAGE128PU:
	case PCI_CHIP_RAGE128PV:
	case PCI_CHIP_RAGE128PW:
	case PCI_CHIP_RAGE128PX:
	case PCI_CHIP_RAGE128TS:
	case PCI_CHIP_RAGE128TT:
	case PCI_CHIP_RAGE128TU:
	case PCI_CHIP_RAGE128SE:
	case PCI_CHIP_RAGE128SF:
	case PCI_CHIP_RAGE128SG:
	case PCI_CHIP_RAGE128SH:
	case PCI_CHIP_RAGE128SK:
	case PCI_CHIP_RAGE128SL:
	case PCI_CHIP_RAGE128SN:
	default:                 info->IsPCI = FALSE; break;
	}
    }
#endif

    return TRUE;
}

static Bool R128PreInitDDC(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10)
{
    R128InfoPtr   info = R128PTR(pScrn);
    vbeInfoPtr pVbe;

    if (!xf86LoadSubModule(pScrn, "ddc")) return FALSE;
    xf86LoaderReqSymLists(ddcSymbols, NULL);

#if defined(__powerpc__) || defined(__alpha__)
    /* Int10 is broken on PPC and some Alphas */
    return TRUE;
#else
    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86LoaderReqSymLists(vbeSymbols,NULL);
	pVbe = VBEInit(pInt10,info->pEnt->index);
	if (!pVbe) return FALSE;
        xf86SetDDCproperties(pScrn,xf86PrintEDID(vbeDoEDID(pVbe,NULL)));
	vbeFree(pVbe);
	return TRUE;
    } else
	return FALSE;
#endif
}

/* This is called by R128PreInit to initialize gamma correction. */
static Bool R128PreInitGamma(ScrnInfoPtr pScrn)
{
    Gamma zeros = { 0.0, 0.0, 0.0 };

    if (!xf86SetGamma(pScrn, zeros)) return FALSE;
    return TRUE;
}

static void
R128I2CGetBits(I2CBusPtr b, int *Clock, int *data)
{
    ScrnInfoPtr   pScrn       = xf86Screens[b->scrnIndex];
    R128InfoPtr info = R128PTR(pScrn);
    unsigned long val;
    unsigned char *R128MMIO = info->MMIO;

    /* Get the result. */
    val = INREG(info->DDCReg);
    *Clock = (val & R128_GPIO_MONID_Y_3) != 0;
    *data  = (val & R128_GPIO_MONID_Y_0) != 0;

}

static void
R128I2CPutBits(I2CBusPtr b, int Clock, int data)
{
    ScrnInfoPtr   pScrn       = xf86Screens[b->scrnIndex];
    R128InfoPtr info = R128PTR(pScrn);
    unsigned long val;
    unsigned char *R128MMIO = info->MMIO;

    val = INREG(info->DDCReg)
              & ~(CARD32)(R128_GPIO_MONID_EN_0 | R128_GPIO_MONID_EN_3);
    val |= (Clock ? 0:R128_GPIO_MONID_EN_3);
    val |= (data ? 0:R128_GPIO_MONID_EN_0);
    OUTREG(info->DDCReg, val);
}


static Bool
R128I2cInit(ScrnInfoPtr pScrn)
{
    R128InfoPtr info = R128PTR(pScrn);
    if ( xf86LoadSubModule(pScrn, "i2c") )
        xf86LoaderReqSymLists(i2cSymbols,NULL);
	else{
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
            "Failed to load i2c module\n");
		return FALSE;
    }

    info->pI2CBus = xf86CreateI2CBusRec();
    if(!info->pI2CBus) return FALSE;

    info->pI2CBus->BusName    = "DDC";
    info->pI2CBus->scrnIndex  = pScrn->scrnIndex;
    info->DDCReg = R128_GPIO_MONID;
    info->pI2CBus->I2CPutBits = R128I2CPutBits;
    info->pI2CBus->I2CGetBits = R128I2CGetBits;
    info->pI2CBus->AcknTimeout = 5;

    if (!xf86I2CBusInit(info->pI2CBus)) {
        return FALSE;
    }
    return TRUE;
}

/* return TRUE is a DFP is indeed connected to a DVI port */
static Bool R128GetDFPInfo(ScrnInfoPtr pScrn)
{
    R128InfoPtr info  = R128PTR(pScrn);
    int i;
    xf86MonPtr MonInfo = NULL;
    xf86MonPtr ddc;
    unsigned char *R128MMIO = info->MMIO;

    if(!R128I2cInit(pScrn)){
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                  "I2C initialization failed!\n");
    }

    OUTREG(info->DDCReg, (INREG(info->DDCReg)
           | R128_GPIO_MONID_MASK_0 | R128_GPIO_MONID_MASK_3));

    OUTREG(info->DDCReg, INREG(info->DDCReg)
           & ~(CARD32)(R128_GPIO_MONID_A_0 | R128_GPIO_MONID_A_3));

    MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, info->pI2CBus);
    if(!MonInfo) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "No DFP detected\n");
        return FALSE;
    }
    xf86SetDDCproperties(pScrn, MonInfo);
    ddc = pScrn->monitor->DDC;

    for(i=0; i<4; i++)
    {
        if(ddc->det_mon[i].type == 0)
        {
            info->PanelXRes =
                ddc->det_mon[i].section.d_timings.h_active;
            info->PanelYRes =
                ddc->det_mon[i].section.d_timings.v_active;

            info->HOverPlus =
                ddc->det_mon[i].section.d_timings.h_sync_off;
            info->HSyncWidth =
                ddc->det_mon[i].section.d_timings.h_sync_width;
            info->HBlank =
                ddc->det_mon[i].section.d_timings.h_blanking;
            info->VOverPlus =
                ddc->det_mon[i].section.d_timings.v_sync_off;
            info->VSyncWidth =
                ddc->det_mon[i].section.d_timings.v_sync_width;
            info->VBlank =
                ddc->det_mon[i].section.d_timings.v_blanking;
        }
    }
    return TRUE;
}


static void R128SetSyncRangeFromEdid(ScrnInfoPtr pScrn, int flag)
{
    int i;
    xf86MonPtr ddc = pScrn->monitor->DDC;
    if(flag)  /*HSync*/
    {
        for(i=0; i<4; i++)
        {
            if(ddc->det_mon[i].type == DS_RANGES)
            {
                pScrn->monitor->nHsync = 1;
                pScrn->monitor->hsync[0].lo =
                    ddc->det_mon[i].section.ranges.min_h;
                pScrn->monitor->hsync[0].hi =
                    ddc->det_mon[i].section.ranges.max_h;
                return;
            }
        }
        /*if no sync ranges detected in detailed timing table,
          let's try to derive them from supported VESA modes
          Are we doing too much here!!!?
        **/
        i = 0;
        if(ddc->timings1.t1 & 0x02) /*800x600@56*/
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 35.2;
            i++;
        }
        if(ddc->timings1.t1 & 0x04) /*640x480@75*/
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 37.5;
            i++;
        }
        if((ddc->timings1.t1 & 0x08) || (ddc->timings1.t1 & 0x01))
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 37.9;
            i++;
        }
        if(ddc->timings1.t2 & 0x40)
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 46.9;
            i++;
        }
        if((ddc->timings1.t2 & 0x80) || (ddc->timings1.t2 & 0x08))
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 48.1;
            i++;
        }
        if(ddc->timings1.t2 & 0x04)
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 56.5;
            i++;
        }
        if(ddc->timings1.t2 & 0x02)
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 60.0;
            i++;
        }
        if(ddc->timings1.t2 & 0x01)
        {
            pScrn->monitor->hsync[i].lo =
                pScrn->monitor->hsync[i].hi = 64.0;
            i++;
        }
        pScrn->monitor->nHsync = i;
    }
    else      /*Vrefresh*/
    {
        for(i=0; i<4; i++)
        {
            if(ddc->det_mon[i].type == DS_RANGES)
            {
                pScrn->monitor->nVrefresh = 1;
                pScrn->monitor->vrefresh[0].lo =
                    ddc->det_mon[i].section.ranges.min_v;
                pScrn->monitor->vrefresh[0].hi =
                    ddc->det_mon[i].section.ranges.max_v;
                return;
            }
        }
        i = 0;
        if(ddc->timings1.t1 & 0x02) /*800x600@56*/
        {
            pScrn->monitor->vrefresh[i].lo =
                pScrn->monitor->vrefresh[i].hi = 56;
            i++;
        }
        if((ddc->timings1.t1 & 0x01) || (ddc->timings1.t2 & 0x08))
        {
            pScrn->monitor->vrefresh[i].lo =
                pScrn->monitor->vrefresh[i].hi = 60;
            i++;
        }
        if(ddc->timings1.t2 & 0x04)
        {
            pScrn->monitor->vrefresh[i].lo =
                pScrn->monitor->vrefresh[i].hi = 70;
            i++;
        }
        if((ddc->timings1.t1 & 0x08) || (ddc->timings1.t2 & 0x80))
        {
            pScrn->monitor->vrefresh[i].lo =
                pScrn->monitor->vrefresh[i].hi = 72;
            i++;
        }
        if((ddc->timings1.t1 & 0x04) || (ddc->timings1.t2 & 0x40)
           || (ddc->timings1.t2 & 0x02) || (ddc->timings1.t2 & 0x01))
        {
            pScrn->monitor->vrefresh[i].lo =
                pScrn->monitor->vrefresh[i].hi = 75;
            i++;
        }
        pScrn->monitor->nVrefresh = i;
    }
}

/***********
   xfree's xf86ValidateModes routine deosn't work well with DFPs
   here is our own validation routine. All modes between
   640<=XRes<=MaxRes and 480<=YRes<=MaxYRes will be permitted.
   NOTE: RageProII doesn't support rmx, can only work with the
         standard modes the monitor can support (scale).
************/

static int R128ValidateFPModes(ScrnInfoPtr pScrn)
{
    int i, j, count=0, width, height;
    R128InfoPtr info = R128PTR(pScrn);
    DisplayModePtr last = NULL, new = NULL, first = NULL;
    xf86MonPtr ddc;

    /* Free any allocated modes during configuration. We don't need them*/
    while (pScrn->modes)
    {
	    xf86DeleteMode(&pScrn->modes, pScrn->modes);
    }
    while (pScrn->modePool)
    {
	    xf86DeleteMode(&pScrn->modePool, pScrn->modePool);
    }

    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    /* If no mode specified in config, we use native resolution*/
    if(!pScrn->display->modes[0])
    {
        pScrn->display->modes[0] = xnfalloc(16);
        sprintf(pScrn->display->modes[0], "%dx%d",
               info->PanelXRes, info->PanelYRes);
    }

    for(i=0; pScrn->display->modes[i] != NULL; i++)
    {
        if (sscanf(pScrn->display->modes[i], "%dx%d", &width, &height) == 2)
        {

            if(width < 640 || width > info->PanelXRes ||
               height < 480 || height > info->PanelYRes)
            {
                xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                    "Mode %s is out of range.\n"
                    "Valid mode should be between 640x480-%dx%d\n",
                    pScrn->display->modes[i], info->PanelXRes, info->PanelYRes);
                continue;
            }

            new = xnfcalloc(1, sizeof(DisplayModeRec));
            new->prev = last;
            new->name = xnfalloc(strlen(pScrn->display->modes[i]) + 1);
            strcpy(new->name, pScrn->display->modes[i]);
            new->HDisplay = new->CrtcHDisplay = width;
            new->VDisplay = new->CrtcVDisplay = height;

            ddc = pScrn->monitor->DDC;
            for(j=0; j<DET_TIMINGS; j++)
            {
                /*We use native mode clock only*/
                if(ddc->det_mon[j].type == 0){
                    new->Clock = ddc->det_mon[j].section.d_timings.clock / 1000;
                    break;
                }
            }

            if(new->prev) new->prev->next = new;
            last = new;
            if(!first) first = new;
            pScrn->display->virtualX =
            pScrn->virtualX = MAX(pScrn->virtualX, width);
            pScrn->display->virtualY =
            pScrn->virtualY = MAX(pScrn->virtualY, height);
            count++;
        }
        else
        {
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                "Mode name %s is invalid\n", pScrn->display->modes[i]);
            continue;
        }
   }

   if(last)
   {
       last->next = first;
       first->prev = last;
       pScrn->modes = first;

       /*FIXME: May need to validate line pitch here*/
       {
           int dummy = 0;
           switch(pScrn->depth / 8)
           {
              case 1:
                  dummy = 128 - pScrn->virtualX % 128;
                  break;
              case 2:
                  dummy = 32 - pScrn->virtualX % 32;
                  break;
              case 3:
              case 4:
                  dummy = 16 - pScrn->virtualX % 16;
           }
           pScrn->displayWidth = pScrn->virtualX + dummy;
       }

   }

   return count;
}


/* This is called by R128PreInit to validate modes and compute parameters
   for all of the valid modes. */
static Bool R128PreInitModes(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info = R128PTR(pScrn);
    ClockRangePtr clockRanges;
    int           modesFound;

    if(info->isDFP) {
        R128MapMem(pScrn);
        info->BIOSDisplay = R128_BIOS_DISPLAY_FP;
        /* validate if DFP really connected. */
        if(!R128GetDFPInfo(pScrn)) {
            info->isDFP = FALSE;
            info->BIOSDisplay = R128_BIOS_DISPLAY_CRT;
        } else if(!info->isPro2) {
            /* RageProII doesn't support rmx, we can't use native-mode
               stretching for other non-native modes. It will rely on
               whatever VESA modes monitor can support. */
            modesFound = R128ValidateFPModes(pScrn);
            if(modesFound < 1) {
                 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                     "No valid mode found for this DFP/LCD\n");
                 R128UnmapMem(pScrn);
                 return FALSE;

            }
        }
        R128UnmapMem(pScrn);
    }

    if(!info->isDFP || info->isPro2) {
				/* Get mode information */
        pScrn->progClock                   = TRUE;
        clockRanges                        = xnfcalloc(sizeof(*clockRanges), 1);
        clockRanges->next                  = NULL;
        clockRanges->minClock              = info->pll.min_pll_freq;
        clockRanges->maxClock              = info->pll.max_pll_freq * 10;
        clockRanges->clockIndex            = -1;
        if (info->HasPanelRegs || info->isDFP) {
            clockRanges->interlaceAllowed  = FALSE;
            clockRanges->doubleScanAllowed = FALSE;
        } else {
            clockRanges->interlaceAllowed  = TRUE;
            clockRanges->doubleScanAllowed = TRUE;
        }

        if(pScrn->monitor->DDC) {
        /*if we still don't know sync range yet, let's try EDID.
          Note that, since we can have dual heads, the Xconfigurator
          may not be able to probe both monitors correctly through
          vbe probe function (R128ProbeDDC). Here we provide an
          additional way to auto-detect sync ranges if they haven't
          been added to XF86Config manually.
        **/
            if(pScrn->monitor->nHsync <= 0)
                R128SetSyncRangeFromEdid(pScrn, 1);
            if(pScrn->monitor->nVrefresh <= 0)
                R128SetSyncRangeFromEdid(pScrn, 0);
        }

        modesFound = xf86ValidateModes(pScrn,
				   pScrn->monitor->Modes,
				   pScrn->display->modes,
				   clockRanges,
				   NULL,        /* linePitches */
				   8 * 64,      /* minPitch */
				   8 * 1024,    /* maxPitch */
				   8 * 64,      /* pitchInc */
				   128,         /* minHeight */
				   2048,        /* maxHeight */
				   pScrn->display->virtualX,
				   pScrn->display->virtualY,
				   info->FbMapSize,
				   LOOKUP_BEST_REFRESH);

        if (modesFound < 1 && info->FBDev) {
		fbdevHWUseBuildinMode(pScrn);
		pScrn->displayWidth = fbdevHWGetLineLength(pScrn)/(pScrn->bitsPerPixel/8);
		modesFound = 1;
        }

        if (modesFound == -1) return FALSE;
        xf86PruneDriverModes(pScrn);
        if (!modesFound || !pScrn->modes) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
            return FALSE;
        }
        xf86SetCrtcForModes(pScrn, 0);
    }
				/* Set DPI */
    pScrn->currentMode = pScrn->modes;
    xf86PrintModes(pScrn);

    xf86SetDpi(pScrn, 0, 0);

				/* Get ScreenInit function */
    if (!xf86LoadSubModule(pScrn, "fb")) return FALSE;
    xf86LoaderReqSymLists(fbSymbols, NULL);

    info->CurrentLayout.displayWidth = pScrn->displayWidth;
    info->CurrentLayout.mode = pScrn->currentMode;

    return TRUE;
}

/* This is called by R128PreInit to initialize the hardware cursor. */
static Bool R128PreInitCursor(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info = R128PTR(pScrn);

    if (!xf86ReturnOptValBool(info->Options, OPTION_SW_CURSOR, FALSE)) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) return FALSE;
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }
    return TRUE;
}

/* This is called by R128PreInit to initialize hardware acceleration. */
static Bool R128PreInitAccel(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info = R128PTR(pScrn);

    if (!xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	if (!xf86LoadSubModule(pScrn, "xaa")) return FALSE;
	xf86LoaderReqSymLists(xaaSymbols, NULL);
    }
    return TRUE;
}

static Bool R128PreInitInt10(ScrnInfoPtr pScrn, xf86Int10InfoPtr *ppInt10)
{
    R128InfoPtr   info = R128PTR(pScrn);
#if 1 && !defined(__alpha__)
    /* int10 is broken on some Alphas */
    if (xf86LoadSubModule(pScrn, "int10")) {
	xf86LoaderReqSymLists(int10Symbols, NULL);
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,"initializing int10\n");
	*ppInt10 = xf86InitInt10(info->pEnt->index);
    }
#endif
    return TRUE;
}

#ifdef XF86DRI
static Bool R128PreInitDRI(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info = R128PTR(pScrn);

    if (xf86ReturnOptValBool(info->Options, OPTION_CCE_PIO, FALSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forcing CCE into PIO mode\n");
	info->CCEMode = R128_DEFAULT_CCE_PIO_MODE;
    } else {
	info->CCEMode = R128_DEFAULT_CCE_BM_MODE;
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_NO_SECURITY, FALSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "WARNING!!!  CCE Security checks disabled!!! **********\n");
	info->CCESecure = FALSE;
    } else {
	info->CCESecure = TRUE;
    }

    info->agpMode        = R128_DEFAULT_AGP_MODE;
    info->agpSize        = R128_DEFAULT_AGP_SIZE;
    info->ringSize       = R128_DEFAULT_RING_SIZE;
    info->bufSize        = R128_DEFAULT_BUFFER_SIZE;
    info->agpTexSize     = R128_DEFAULT_AGP_TEX_SIZE;

    info->CCEusecTimeout = R128_DEFAULT_CCE_TIMEOUT;

    if (!info->IsPCI) {
	if (xf86GetOptValInteger(info->Options,
				 OPTION_AGP_MODE, &(info->agpMode))) {
	    if (info->agpMode < 1 || info->agpMode > R128_AGP_MAX_MODE) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Illegal AGP Mode: %d\n", info->agpMode);
		return FALSE;
	    }
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Using AGP %dx mode\n", info->agpMode);
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
			     &(info->CCEusecTimeout))) {
	/* This option checked by the R128 DRM kernel module */
    }

    if (!xf86LoadSubModule(pScrn, "shadowfb")) {
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
R128ProbeDDC(ScrnInfoPtr pScrn, int indx)
{
    vbeInfoPtr pVbe;
    if (xf86LoadSubModule(pScrn, "vbe")) {
	pVbe = VBEInit(NULL,indx);
	ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}

/* R128PreInit is called once at server startup. */
Bool R128PreInit(ScrnInfoPtr pScrn, int flags)
{
    R128InfoPtr      info;
    xf86Int10InfoPtr pInt10 = NULL;

    R128TRACE(("R128PreInit\n"));

    if (pScrn->numEntities != 1) return FALSE;

    if (!R128GetRec(pScrn)) return FALSE;

    info               = R128PTR(pScrn);

    info->pEnt         = xf86GetEntityInfo(pScrn->entityList[0]);
    if (info->pEnt->location.type != BUS_PCI) goto fail;

    if (flags & PROBE_DETECT) {
	R128ProbeDDC(pScrn, info->pEnt->index);
	return TRUE;
    }

    if (!xf86LoadSubModule(pScrn, "vgahw")) return FALSE;
    xf86LoaderReqSymLists(vgahwSymbols, NULL);
    if (!vgaHWGetHWRec(pScrn)) {
	R128FreeRec(pScrn);
	return FALSE;
    }

    info->PciInfo      = xf86GetPciInfoForEntity(info->pEnt->index);
    info->PciTag       = pciTag(info->PciInfo->bus,
				info->PciInfo->device,
				info->PciInfo->func);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "PCI bus %d card %d func %d\n",
	       info->PciInfo->bus,
	       info->PciInfo->device,
	       info->PciInfo->func);

    if (xf86RegisterResources(info->pEnt->index, 0, ResNone)) goto fail;
    if (xf86SetOperatingState(resVga, info->pEnt->index, ResUnusedOpr)) goto fail;

    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_VIEWPORT | RAC_CURSOR;
    pScrn->monitor     = pScrn->confScreen->monitor;

    if (!R128PreInitVisual(pScrn))    goto fail;

				/* We can't do this until we have a
				   pScrn->display. */
    xf86CollectOptions(pScrn, NULL);
    if (!(info->Options = xalloc(sizeof(R128Options))))    goto fail;
    memcpy(info->Options, R128Options, sizeof(R128Options));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, info->Options);

    if (!R128PreInitWeight(pScrn))    goto fail;

    if(xf86GetOptValInteger(info->Options, OPTION_VIDEO_KEY, &(info->videoKey))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
                                info->videoKey);
    } else {
        info->videoKey = 0x1E;
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_SHOW_CACHE, FALSE)) {
        info->showCache = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShowCache enabled\n");
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_FBDEV, FALSE)) {
	info->FBDev = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "Using framebuffer device\n");
    }

    if (info->FBDev) {
	/* check for linux framebuffer device */
	if (!xf86LoadSubModule(pScrn, "fbdevhw")) return FALSE;
	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);
	if (!fbdevHWInit(pScrn, info->PciInfo, NULL)) return FALSE;
	pScrn->SwitchMode    = fbdevHWSwitchMode;
	pScrn->AdjustFrame   = fbdevHWAdjustFrame;
	pScrn->ValidMode     = fbdevHWValidMode;
    }

    if (!info->FBDev)
	if (!R128PreInitInt10(pScrn, &pInt10)) goto fail;

    if (!R128PreInitConfig(pScrn))             goto fail;

    if (!R128GetBIOSParameters(pScrn, pInt10)) goto fail;

    if (!R128GetPLLParameters(pScrn))          goto fail;

    /* Don't fail on this one */
    R128PreInitDDC(pScrn, pInt10);

    if (!R128PreInitGamma(pScrn))              goto fail;

    if (!R128PreInitModes(pScrn))              goto fail;

    if (!R128PreInitCursor(pScrn))             goto fail;

    if (!R128PreInitAccel(pScrn))              goto fail;

#ifdef XF86DRI
    if (!R128PreInitDRI(pScrn))                goto fail;
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
    R128FreeRec(pScrn);
    return FALSE;
}

/* Load a palette. */
static void R128LoadPalette(ScrnInfoPtr pScrn, int numColors,
			    int *indices, LOCO *colors, VisualPtr pVisual)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           i;
    int           idx;
    unsigned char r, g, b;

    /* Select palette 0 (main CRTC) if using FP-enabled chip */
    if (info->HasPanelRegs || info->isDFP) PAL_SELECT(0);

    if (info->CurrentLayout.depth == 15) {
	/* 15bpp mode.  This sends 32 values. */
	for (i = 0; i < numColors; i++) {
	    idx = indices[i];
	    r   = colors[idx].red;
	    g   = colors[idx].green;
	    b   = colors[idx].blue;
	    OUTPAL(idx * 8, r, g, b);
	}
    }
    else if (info->CurrentLayout.depth == 16) {
	/* 16bpp mode.  This sends 64 values. */
				/* There are twice as many green values as
				   there are values for red and blue.  So,
				   we take each red and blue pair, and
				   combine it with each of the two green
				   values. */
	for (i = 0; i < numColors; i++) {
	    idx = indices[i];
	    r   = colors[idx / 2].red;
	    g   = colors[idx].green;
	    b   = colors[idx / 2].blue;
	    OUTPAL(idx * 4, r, g, b);
	}
    }
    else {
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

static void
R128BlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
    ScreenPtr   pScreen = screenInfo.screens[i];
    ScrnInfoPtr pScrn   = xf86Screens[i];
    R128InfoPtr info    = R128PTR(pScrn);

#ifdef XF86DRI
    if (info->directRenderingEnabled)
        FLUSH_RING();
#endif

    pScreen->BlockHandler = info->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = R128BlockHandler;

    if(info->VideoTimerCallback) {
        (*info->VideoTimerCallback)(pScrn, currentTime.milliseconds);
    }
}

/* Called at the start of each server generation. */
Bool R128ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn  = xf86Screens[pScreen->myNum];
    R128InfoPtr info   = R128PTR(pScrn);
    BoxRec      MemBox;
    int		y2;

    R128TRACE(("R128ScreenInit %x %d\n", pScrn->memPhysBase, pScrn->fbOffset));

#ifdef XF86DRI
				/* Turn off the CCE for now. */
    info->CCEInUse     = FALSE;
    info->indirectBuffer = NULL;
#endif

    if (!R128MapMem(pScrn)) return FALSE;
    pScrn->fbOffset    = 0;
#ifdef XF86DRI
    info->fbX          = 0;
    info->fbY          = 0;
    info->frontOffset  = 0;
    info->frontPitch   = pScrn->displayWidth;
#endif

    info->PaletteSavedOnVT = FALSE;

    R128Save(pScrn);
    if (info->FBDev) {
	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) return FALSE;
    } else {
	if (!R128ModeInit(pScrn, pScrn->currentMode)) return FALSE;
    }

    R128SaveScreen(pScreen, SCREEN_SAVER_ON);
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

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
	   buffers, we will want to revisit the following check for 3
	   times the virtual size of the screen below. */
	int width_bytes = (pScrn->displayWidth *
			   info->CurrentLayout.pixel_bytes);
	int maxy        = info->FbMapSize / width_bytes;

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
	} else {
	    info->directRenderingEnabled = R128DRIScreenInit(pScreen);
	}
    }
#endif

    if (!fbScreenInit (pScreen, info->FB,
		       pScrn->virtualX, pScrn->virtualY,
		       pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth,
		       pScrn->bitsPerPixel))
	return FALSE;

    xf86SetBlackWhitePixels(pScreen);

    if (pScrn->bitsPerPixel > 8) {
	VisualPtr visual;

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

    /* must be after RGB order fixed */
    fbPictureInit (pScreen, 0, 0);

				/* Memory manager setup */
#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	FBAreaPtr fbarea;
	int width_bytes = (pScrn->displayWidth *
			   info->CurrentLayout.pixel_bytes);
	int cpp = info->CurrentLayout.pixel_bytes;
	int bufferSize = pScrn->virtualY * width_bytes;
	int l, total;
	int scanlines;

	switch (info->CCEMode) {
	case R128_DEFAULT_CCE_PIO_MODE:
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CCE in PIO mode\n");
	    break;
	case R128_DEFAULT_CCE_BM_MODE:
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CCE in BM mode\n");
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CCE in UNKNOWN mode\n");
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

	/* Try for front, back, depth, and two framebuffers worth of
	 * pixmap cache.  Should be enough for a fullscreen background
	 * image plus some leftovers.
	 */
	info->textureSize = info->FbMapSize - 5 * bufferSize;

	/* If that gives us less than half the available memory, let's
	 * be greedy and grab some more.  Sorry, I care more about 3D
	 * performance than playing nicely, and you'll get around a full
	 * framebuffer's worth of pixmap cache anyway.
	 */
	if (info->textureSize < (int)info->FbMapSize / 2) {
	    info->textureSize = info->FbMapSize - 4 * bufferSize;
	}

	if (info->textureSize > 0) {
	    l = R128MinBits((info->textureSize-1) / R128_NR_TEX_REGIONS);
	    if (l < R128_LOG_TEX_GRANULARITY) l = R128_LOG_TEX_GRANULARITY;

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

	total = info->FbMapSize - info->textureSize;
	scanlines = total / width_bytes;
	if (scanlines > 8191) scanlines = 8191;

	/* Recalculate the texture offset and size to accomodate any
	 * rounding to a whole number of scanlines.
	 */
	info->textureOffset = scanlines * width_bytes;

	MemBox.x1 = 0;
	MemBox.y1 = 0;
	MemBox.x2 = pScrn->displayWidth;
	MemBox.y2 = scanlines;

	if (!xf86InitFBManager(pScreen, &MemBox)) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Memory manager initialization to (%d,%d) (%d,%d) failed\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    return FALSE;
	} else {
	    int width, height;

	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Memory manager initialized to (%d,%d) (%d,%d)\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						    pScrn->displayWidth,
						    2, 0, NULL, NULL, NULL))) {
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
	    }
	}

				/* Allocate the shared back buffer */
	if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						pScrn->virtualX,
						pScrn->virtualY,
						32, NULL, NULL, NULL))) {
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Reserved back buffer from (%d,%d) to (%d,%d)\n",
		       fbarea->box.x1, fbarea->box.y1,
		       fbarea->box.x2, fbarea->box.y2);

	    info->backX = fbarea->box.x1;
	    info->backY = fbarea->box.y1;
	    info->backOffset = (fbarea->box.y1 * width_bytes +
				fbarea->box.x1 * cpp);
	    info->backPitch = pScrn->displayWidth;
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR, "Unable to reserve back buffer\n");
	    info->backX = -1;
	    info->backY = -1;
	    info->backOffset = -1;
	    info->backPitch = -1;
	}

				/* Allocate the shared depth buffer */
	if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						pScrn->virtualX,
						pScrn->virtualY + 1,
						32, NULL, NULL, NULL))) {
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Reserved depth buffer from (%d,%d) to (%d,%d)\n",
		       fbarea->box.x1, fbarea->box.y1,
		       fbarea->box.x2, fbarea->box.y2);

	    info->depthX = fbarea->box.x1;
	    info->depthY = fbarea->box.y1;
	    info->depthOffset = (fbarea->box.y1 * width_bytes +
				 fbarea->box.x1 * cpp);
	    info->depthPitch = pScrn->displayWidth;
	    info->spanOffset = ((fbarea->box.y2 - 1) * width_bytes +
				fbarea->box.x1 * cpp);
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Reserved depth span from (%d,%d) offset 0x%x\n",
		       fbarea->box.x1, fbarea->box.y2 - 1, info->spanOffset);
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR, "Unable to reserve depth buffer\n");
	    info->depthX = -1;
	    info->depthY = -1;
	    info->depthOffset = -1;
	    info->depthPitch = -1;
	    info->spanOffset = -1;
	}

	xf86DrvMsg(scrnIndex, X_INFO,
		   "Reserved %d kb for textures at offset 0x%x\n",
		   info->textureSize/1024, info->textureOffset);
    }
    else
#endif
    {
	MemBox.x1 = 0;
	MemBox.y1 = 0;
	MemBox.x2 = pScrn->displayWidth;
	y2        = (info->FbMapSize
		     / (pScrn->displayWidth *
			info->CurrentLayout.pixel_bytes));
				/* The acceleration engine uses 14 bit
				   signed coordinates, so we can't have any
				   drawable caches beyond this region. */
	if (y2 > 8191) y2 = 8191;
	MemBox.y2 = y2;

	if (!xf86InitFBManager(pScreen, &MemBox)) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Memory manager initialization to (%d,%d) (%d,%d) failed\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    return FALSE;
	} else {
	    int       width, height;
	    FBAreaPtr fbarea;

	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Memory manager initialized to (%d,%d) (%d,%d)\n",
		       MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	    if ((fbarea = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
						    2, 0, NULL, NULL, NULL))) {
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
	if (R128AccelInit(pScreen)) {
	    xf86DrvMsg(scrnIndex, X_INFO, "Acceleration enabled\n");
	    info->accelOn = TRUE;
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
    R128DGAInit(pScreen);

				/* Backing store setup */
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

				/* Set Silken Mouse */
    xf86SetSilkenMouse(pScreen);

				/* Cursor setup */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

				/* Hardware cursor setup */
    if (!xf86ReturnOptValBool(info->Options, OPTION_SW_CURSOR, FALSE)) {
	if (R128CursorInit(pScreen)) {
	    int width, height;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Using hardware cursor (scanline %ld)\n",
		       info->cursor_start / pScrn->displayWidth);
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
			     (info->FBDev ? fbdevHWLoadPalette :
			     R128LoadPalette), NULL,
			     CMAP_PALETTED_TRUECOLOR
			     | CMAP_RELOAD_ON_MODE_SWITCH
#if 0 /* This option messes up text mode! (eich@suse.de) */
			     | CMAP_LOAD_EVEN_IF_OFFSCREEN
#endif
			     )) return FALSE;

    /* DPMS setup - FIXME: also for mirror mode in non-fbdev case? - Michel */
    if (info->FBDev)
	xf86DPMSInit(pScreen, fbdevHWDPMSSet, 0);
    else {
	if (!info->HasPanelRegs || info->BIOSDisplay == R128_BIOS_DISPLAY_CRT)
	    xf86DPMSInit(pScreen, R128DisplayPowerManagementSet, 0);
	else if (info->HasPanelRegs || info->BIOSDisplay == R128_BIOS_DISPLAY_FP)
	    xf86DPMSInit(pScreen, R128DisplayPowerManagementSetLCD, 0);
    }

    R128InitVideo(pScreen);

				/* Provide SaveScreen */
    pScreen->SaveScreen  = R128SaveScreen;

				/* Wrap CloseScreen */
    info->CloseScreen    = pScreen->CloseScreen;
    pScreen->CloseScreen = R128CloseScreen;

				/* Note unused options */
    if (serverGeneration == 1)
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

#ifdef XF86DRI
				/* DRI finalization */
    if (info->directRenderingEnabled) {
				/* Now that mi, fb, drm and others have
				   done their thing, complete the DRI
				   setup. */
	info->directRenderingEnabled = R128DRIFinishScreenInit(pScreen);
    }
    if (info->directRenderingEnabled) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering enabled\n");
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering disabled\n");
    }
#endif

    info->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = R128BlockHandler;

    return TRUE;
}

/* Write common registers (initialized to 0). */
static void R128RestoreCommonRegisters(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTREG(R128_FP_GEN_CNTL, restore->fp_gen_cntl | R128_FP_BLANK_DIS);

    OUTREG(R128_OVR_CLR,              restore->ovr_clr);
    OUTREG(R128_OVR_WID_LEFT_RIGHT,   restore->ovr_wid_left_right);
    OUTREG(R128_OVR_WID_TOP_BOTTOM,   restore->ovr_wid_top_bottom);
    OUTREG(R128_OV0_SCALE_CNTL,       restore->ov0_scale_cntl);
    OUTREG(R128_MPP_TB_CONFIG,        restore->mpp_tb_config );
    OUTREG(R128_MPP_GP_CONFIG,        restore->mpp_gp_config );
    OUTREG(R128_SUBPIC_CNTL,          restore->subpic_cntl);
    OUTREG(R128_VIPH_CONTROL,         restore->viph_control);
    OUTREG(R128_I2C_CNTL_1,           restore->i2c_cntl_1);
    OUTREG(R128_GEN_INT_CNTL,         restore->gen_int_cntl);
    OUTREG(R128_CAP0_TRIG_CNTL,       restore->cap0_trig_cntl);
    OUTREG(R128_CAP1_TRIG_CNTL,       restore->cap1_trig_cntl);
    OUTREG(R128_BUS_CNTL,             restore->bus_cntl);
    OUTREG(R128_CONFIG_CNTL,          restore->config_cntl);
}

/* Write CRTC registers. */
static void R128RestoreCrtcRegisters(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTREG(R128_CRTC_GEN_CNTL,        restore->crtc_gen_cntl);

    OUTREGP(R128_CRTC_EXT_CNTL, restore->crtc_ext_cntl,
	    R128_CRTC_VSYNC_DIS | R128_CRTC_HSYNC_DIS | R128_CRTC_DISPLAY_DIS);

    OUTREGP(R128_DAC_CNTL, restore->dac_cntl,
	    R128_DAC_RANGE_CNTL | R128_DAC_BLANKING);

    OUTREG(R128_CRTC_H_TOTAL_DISP,    restore->crtc_h_total_disp);
    OUTREG(R128_CRTC_H_SYNC_STRT_WID, restore->crtc_h_sync_strt_wid);
    OUTREG(R128_CRTC_V_TOTAL_DISP,    restore->crtc_v_total_disp);
    OUTREG(R128_CRTC_V_SYNC_STRT_WID, restore->crtc_v_sync_strt_wid);
    OUTREG(R128_CRTC_OFFSET,          restore->crtc_offset);
    OUTREG(R128_CRTC_OFFSET_CNTL,     restore->crtc_offset_cntl);
    OUTREG(R128_CRTC_PITCH,           restore->crtc_pitch);
}

/* Write flat panel registers */
static void R128RestoreFPRegisters(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    CARD32        tmp;


    /*OUTREG(R128_CRTC2_GEN_CNTL,       restore->crtc2_gen_cntl);*/
    OUTREG(R128_FP_HORZ_STRETCH,      restore->fp_horz_stretch);
    OUTREG(R128_FP_VERT_STRETCH,      restore->fp_vert_stretch);
    OUTREG(R128_FP_CRTC_H_TOTAL_DISP, restore->fp_crtc_h_total_disp);
    OUTREG(R128_FP_CRTC_V_TOTAL_DISP, restore->fp_crtc_v_total_disp);
    OUTREG(R128_FP_H_SYNC_STRT_WID,   restore->fp_h_sync_strt_wid);
    OUTREG(R128_FP_V_SYNC_STRT_WID,   restore->fp_v_sync_strt_wid);
    OUTREG(R128_TMDS_CRC,             restore->tmds_crc);
    OUTREG(R128_FP_PANEL_CNTL,        restore->fp_panel_cntl);
    OUTREG(R128_FP_GEN_CNTL, restore->fp_gen_cntl & ~(CARD32)R128_FP_BLANK_DIS);

    if(info->isDFP) return;

    tmp = INREG(R128_LVDS_GEN_CNTL);
    if ((tmp & (R128_LVDS_ON | R128_LVDS_BLON)) ==
	(restore->lvds_gen_cntl & (R128_LVDS_ON | R128_LVDS_BLON))) {
	OUTREG(R128_LVDS_GEN_CNTL, restore->lvds_gen_cntl);
    } else {
	if (restore->lvds_gen_cntl & (R128_LVDS_ON | R128_LVDS_BLON)) {
	    OUTREG(R128_LVDS_GEN_CNTL,
		   restore->lvds_gen_cntl & (CARD32)~R128_LVDS_BLON);
	    usleep(R128PTR(pScrn)->PanelPwrDly * 1000);
	    OUTREG(R128_LVDS_GEN_CNTL, restore->lvds_gen_cntl);
	} else {
	    OUTREG(R128_LVDS_GEN_CNTL, restore->lvds_gen_cntl | R128_LVDS_BLON);
	    usleep(R128PTR(pScrn)->PanelPwrDly * 1000);
	    OUTREG(R128_LVDS_GEN_CNTL, restore->lvds_gen_cntl);
	}
    }
}

static void R128PLLWaitForReadUpdateComplete(ScrnInfoPtr pScrn)
{
    while (INPLL(pScrn, R128_PPLL_REF_DIV) & R128_PPLL_ATOMIC_UPDATE_R);
}

static void R128PLLWriteUpdate(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTPLLP(pScrn, R128_PPLL_REF_DIV, R128_PPLL_ATOMIC_UPDATE_W, 0xffff);
}

/* Write PLL registers. */
static void R128RestorePLLRegisters(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTREGP(R128_CLOCK_CNTL_INDEX, R128_PLL_DIV_SEL, 0xffff);

    OUTPLLP(pScrn,
	    R128_PPLL_CNTL,
	    R128_PPLL_RESET
	    | R128_PPLL_ATOMIC_UPDATE_EN
	    | R128_PPLL_VGA_ATOMIC_UPDATE_EN,
	    0xffff);

    R128PLLWaitForReadUpdateComplete(pScrn);
    OUTPLLP(pScrn, R128_PPLL_REF_DIV,
	    restore->ppll_ref_div, ~R128_PPLL_REF_DIV_MASK);
    R128PLLWriteUpdate(pScrn);

    R128PLLWaitForReadUpdateComplete(pScrn);
    OUTPLLP(pScrn, R128_PPLL_DIV_3,
	    restore->ppll_div_3, ~R128_PPLL_FB3_DIV_MASK);
    R128PLLWriteUpdate(pScrn);
    OUTPLLP(pScrn, R128_PPLL_DIV_3,
	    restore->ppll_div_3, ~R128_PPLL_POST3_DIV_MASK);
    R128PLLWriteUpdate(pScrn);

    R128PLLWaitForReadUpdateComplete(pScrn);
    OUTPLL(R128_HTOTAL_CNTL, restore->htotal_cntl);
    R128PLLWriteUpdate(pScrn);

    OUTPLLP(pScrn, R128_PPLL_CNTL, 0, ~R128_PPLL_RESET);

    R128TRACE(("Wrote: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
	       restore->ppll_ref_div,
	       restore->ppll_div_3,
	       restore->htotal_cntl,
	       INPLL(pScrn, R128_PPLL_CNTL)));
    R128TRACE(("Wrote: rd=%d, fd=%d, pd=%d\n",
	       restore->ppll_ref_div & R128_PPLL_REF_DIV_MASK,
	       restore->ppll_div_3 & R128_PPLL_FB3_DIV_MASK,
	       (restore->ppll_div_3 & R128_PPLL_POST3_DIV_MASK) >> 16));
}

/* Write DDA registers. */
static void R128RestoreDDARegisters(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    OUTREG(R128_DDA_CONFIG, restore->dda_config);
    OUTREG(R128_DDA_ON_OFF, restore->dda_on_off);
}

/* Write palette data. */
static void R128RestorePalette(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           i;

    if (!restore->palette_valid) return;

    /* Select palette 0 (main CRTC) if using FP-enabled chip */
    if (info->HasPanelRegs || info->isDFP) PAL_SELECT(0);

    OUTPAL_START(0);
    for (i = 0; i < 256; i++) OUTPAL_NEXT_CARD32(restore->palette[i]);
}

/* Write out state to define a new video mode.  */
static void R128RestoreMode(ScrnInfoPtr pScrn, R128SavePtr restore)
{
    R128InfoPtr info = R128PTR(pScrn);

    R128TRACE(("R128RestoreMode(%p)\n", restore));
    R128RestoreCommonRegisters(pScrn, restore);
    R128RestoreCrtcRegisters(pScrn, restore);
    if (!(info->HasPanelRegs) || info->BIOSDisplay == R128_BIOS_DISPLAY_CRT){
        R128RestorePLLRegisters(pScrn, restore);
    }
    R128RestoreDDARegisters(pScrn, restore);
    if (info->HasPanelRegs || info->isDFP)
        R128RestoreFPRegisters(pScrn, restore);

    R128RestorePalette(pScrn, restore);
}

/* Read common registers. */
static void R128SaveCommonRegisters(ScrnInfoPtr pScrn, R128SavePtr save)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    save->ovr_clr            = INREG(R128_OVR_CLR);
    save->ovr_wid_left_right = INREG(R128_OVR_WID_LEFT_RIGHT);
    save->ovr_wid_top_bottom = INREG(R128_OVR_WID_TOP_BOTTOM);
    save->ov0_scale_cntl     = INREG(R128_OV0_SCALE_CNTL);
    save->mpp_tb_config      = INREG(R128_MPP_TB_CONFIG);
    save->mpp_gp_config      = INREG(R128_MPP_GP_CONFIG);
    save->subpic_cntl        = INREG(R128_SUBPIC_CNTL);
    save->viph_control       = INREG(R128_VIPH_CONTROL);
    save->i2c_cntl_1         = INREG(R128_I2C_CNTL_1);
    save->gen_int_cntl       = INREG(R128_GEN_INT_CNTL);
    save->cap0_trig_cntl     = INREG(R128_CAP0_TRIG_CNTL);
    save->cap1_trig_cntl     = INREG(R128_CAP1_TRIG_CNTL);
    save->bus_cntl           = INREG(R128_BUS_CNTL);
    save->config_cntl        = INREG(R128_CONFIG_CNTL);
}

/* Read CRTC registers. */
static void R128SaveCrtcRegisters(ScrnInfoPtr pScrn, R128SavePtr save)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    save->crtc_gen_cntl        = INREG(R128_CRTC_GEN_CNTL);
    save->crtc_ext_cntl        = INREG(R128_CRTC_EXT_CNTL);
    save->dac_cntl             = INREG(R128_DAC_CNTL);
    save->crtc_h_total_disp    = INREG(R128_CRTC_H_TOTAL_DISP);
    save->crtc_h_sync_strt_wid = INREG(R128_CRTC_H_SYNC_STRT_WID);
    save->crtc_v_total_disp    = INREG(R128_CRTC_V_TOTAL_DISP);
    save->crtc_v_sync_strt_wid = INREG(R128_CRTC_V_SYNC_STRT_WID);
    save->crtc_offset          = INREG(R128_CRTC_OFFSET);
    save->crtc_offset_cntl     = INREG(R128_CRTC_OFFSET_CNTL);
    save->crtc_pitch           = INREG(R128_CRTC_PITCH);
}

/* Read flat panel registers */
static void R128SaveFPRegisters(ScrnInfoPtr pScrn, R128SavePtr save)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    save->crtc2_gen_cntl       = INREG(R128_CRTC2_GEN_CNTL);
    save->fp_crtc_h_total_disp = INREG(R128_FP_CRTC_H_TOTAL_DISP);
    save->fp_crtc_v_total_disp = INREG(R128_FP_CRTC_V_TOTAL_DISP);
    save->fp_gen_cntl          = INREG(R128_FP_GEN_CNTL);
    save->fp_h_sync_strt_wid   = INREG(R128_FP_H_SYNC_STRT_WID);
    save->fp_horz_stretch      = INREG(R128_FP_HORZ_STRETCH);
    save->fp_panel_cntl        = INREG(R128_FP_PANEL_CNTL);
    save->fp_v_sync_strt_wid   = INREG(R128_FP_V_SYNC_STRT_WID);
    save->fp_vert_stretch      = INREG(R128_FP_VERT_STRETCH);
    save->lvds_gen_cntl        = INREG(R128_LVDS_GEN_CNTL);
    save->tmds_crc             = INREG(R128_TMDS_CRC);
    save->tmds_transmitter_cntl = INREG(R128_TMDS_TRANSMITTER_CNTL);
}

/* Read PLL registers. */
static void R128SavePLLRegisters(ScrnInfoPtr pScrn, R128SavePtr save)
{
    save->ppll_ref_div         = INPLL(pScrn, R128_PPLL_REF_DIV);
    save->ppll_div_3           = INPLL(pScrn, R128_PPLL_DIV_3);
    save->htotal_cntl          = INPLL(pScrn, R128_HTOTAL_CNTL);

    R128TRACE(("Read: 0x%08x 0x%08x 0x%08x\n",
	       save->ppll_ref_div,
	       save->ppll_div_3,
	       save->htotal_cntl));
    R128TRACE(("Read: rd=%d, fd=%d, pd=%d\n",
	       save->ppll_ref_div & R128_PPLL_REF_DIV_MASK,
	       save->ppll_div_3 & R128_PPLL_FB3_DIV_MASK,
	       (save->ppll_div_3 & R128_PPLL_POST3_DIV_MASK) >> 16));
}

/* Read DDA registers. */
static void R128SaveDDARegisters(ScrnInfoPtr pScrn, R128SavePtr save)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    save->dda_config           = INREG(R128_DDA_CONFIG);
    save->dda_on_off           = INREG(R128_DDA_ON_OFF);
}

/* Read palette data. */
static void R128SavePalette(ScrnInfoPtr pScrn, R128SavePtr save)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           i;

    /* Select palette 0 (main CRTC) if using FP-enabled chip */
    if (info->HasPanelRegs || info->isDFP) PAL_SELECT(0);

    INPAL_START(0);
    for (i = 0; i < 256; i++) save->palette[i] = INPAL_NEXT();
    save->palette_valid = TRUE;
}

/* Save state that defines current video mode. */
static void R128SaveMode(ScrnInfoPtr pScrn, R128SavePtr save)
{
    R128TRACE(("R128SaveMode(%p)\n", save));

    R128SaveCommonRegisters(pScrn, save);
    R128SaveCrtcRegisters(pScrn, save);
    if (R128PTR(pScrn)->HasPanelRegs || R128PTR(pScrn)->isDFP)
	R128SaveFPRegisters(pScrn, save);
    R128SavePLLRegisters(pScrn, save);
    R128SaveDDARegisters(pScrn, save);
    R128SavePalette(pScrn, save);

    R128TRACE(("R128SaveMode returns %p\n", save));
}

/* Save everything needed to restore the original VC state. */
static void R128Save(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    R128SavePtr   save      = &info->SavedReg;
    vgaHWPtr      hwp       = VGAHWPTR(pScrn);

    R128TRACE(("R128Save\n"));
    if (info->FBDev) {
	fbdevHWSave(pScrn);
	return;
    }
    vgaHWUnlock(hwp);
    vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_ALL); /* save mode, fonts, cmap */
    vgaHWLock(hwp);

    R128SaveMode(pScrn, save);

    save->dp_datatype      = INREG(R128_DP_DATATYPE);
    save->gen_reset_cntl   = INREG(R128_GEN_RESET_CNTL);
    save->clock_cntl_index = INREG(R128_CLOCK_CNTL_INDEX);
    save->amcgpio_en_reg   = INREG(R128_AMCGPIO_EN_REG);
    save->amcgpio_mask     = INREG(R128_AMCGPIO_MASK);
}

/* Restore the original (text) mode. */
static void R128Restore(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    R128SavePtr   restore   = &info->SavedReg;
    vgaHWPtr      hwp       = VGAHWPTR(pScrn);

    R128TRACE(("R128Restore\n"));
    if (info->FBDev) {
	fbdevHWRestore(pScrn);
	return;
    }

    R128Blank(pScrn);
    OUTREG(R128_AMCGPIO_MASK,     restore->amcgpio_mask);
    OUTREG(R128_AMCGPIO_EN_REG,   restore->amcgpio_en_reg);
    OUTREG(R128_CLOCK_CNTL_INDEX, restore->clock_cntl_index);
    OUTREG(R128_GEN_RESET_CNTL,   restore->gen_reset_cntl);
    OUTREG(R128_DP_DATATYPE,      restore->dp_datatype);

    R128RestoreMode(pScrn, restore);
    vgaHWUnlock(hwp);
    vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_MODE | VGA_SR_FONTS );
    vgaHWLock(hwp);

    R128WaitForVerticalSync(pScrn);
    R128Unblank(pScrn);
}

/* Define common registers for requested video mode. */
static void R128InitCommonRegisters(R128SavePtr save, R128InfoPtr info)
{
    save->ovr_clr            = 0;
    save->ovr_wid_left_right = 0;
    save->ovr_wid_top_bottom = 0;
    save->ov0_scale_cntl     = 0;
    save->mpp_tb_config      = 0;
    save->mpp_gp_config      = 0;
    save->subpic_cntl        = 0;
    save->viph_control       = 0;
    save->i2c_cntl_1         = 0;
#ifdef XF86DRI
    save->gen_int_cntl       = info->gen_int_cntl;
#else
    save->gen_int_cntl       = 0;
#endif
    save->cap0_trig_cntl     = 0;
    save->cap1_trig_cntl     = 0;
    save->bus_cntl           = info->BusCntl;
    /*
     * If bursts are enabled, turn on discards and aborts
     */
    if (save->bus_cntl & (R128_BUS_WRT_BURST|R128_BUS_READ_BURST))
	save->bus_cntl |= R128_BUS_RD_DISCARD_EN | R128_BUS_RD_ABORT_EN;
}

/* Define CRTC registers for requested video mode. */
static Bool R128InitCrtcRegisters(ScrnInfoPtr pScrn, R128SavePtr save,
				  DisplayModePtr mode, R128InfoPtr info)
{
    int    format;
    int    hsync_start;
    int    hsync_wid;
    int    hsync_fudge;
    int    vsync_wid;
    int    hsync_fudge_default[] = { 0x00, 0x12, 0x09, 0x09, 0x06, 0x05 };
    int    hsync_fudge_fp[]      = { 0x12, 0x11, 0x09, 0x09, 0x05, 0x05 };
    int    hsync_fudge_fp_crt[]  = { 0x12, 0x10, 0x08, 0x08, 0x04, 0x04 };

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

    switch (info->BIOSDisplay) {
    case R128_BIOS_DISPLAY_FP:
	hsync_fudge = hsync_fudge_fp[format-1];
	break;
    case R128_BIOS_DISPLAY_FP_CRT:
	hsync_fudge = hsync_fudge_fp_crt[format-1];
	break;
    case R128_BIOS_DISPLAY_CRT:
    default:
	hsync_fudge = hsync_fudge_default[format-1];
	break;
    }

    save->crtc_gen_cntl = (R128_CRTC_EXT_DISP_EN
			  | R128_CRTC_EN
			  | (format << 8)
			  | ((mode->Flags & V_DBLSCAN)
			     ? R128_CRTC_DBL_SCAN_EN
			     : 0)
			  | ((mode->Flags & V_INTERLACE)
			     ? R128_CRTC_INTERLACE_EN
			     : 0)
			  | ((mode->Flags & V_CSYNC)
			     ? R128_CRTC_CSYNC_EN
			     : 0));

    save->crtc_ext_cntl = R128_VGA_ATI_LINEAR | R128_XCRT_CNT_EN;
    save->dac_cntl      = (R128_DAC_MASK_ALL
			   | R128_DAC_VGA_ADR_EN
			   | (info->dac6bits ? 0 : R128_DAC_8BIT_EN));


    if(info->isDFP && !info->isPro2)
    {
        if(info->PanelXRes < mode->CrtcHDisplay)
            mode->HDisplay = mode->CrtcHDisplay = info->PanelXRes;
        if(info->PanelYRes < mode->CrtcVDisplay)
            mode->VDisplay = mode->CrtcVDisplay = info->PanelYRes;
        mode->CrtcHTotal = mode->CrtcHDisplay + info->HBlank;
        mode->CrtcHSyncStart = mode->CrtcHDisplay + info->HOverPlus;
        mode->CrtcHSyncEnd = mode->CrtcHSyncStart + info->HSyncWidth;
        mode->CrtcVTotal = mode->CrtcVDisplay + info->VBlank;
        mode->CrtcVSyncStart = mode->CrtcVDisplay + info->VOverPlus;
        mode->CrtcVSyncEnd = mode->CrtcVSyncStart + info->VSyncWidth;
    }

    save->crtc_h_total_disp = ((((mode->CrtcHTotal / 8) - 1) & 0xffff)
			      | (((mode->CrtcHDisplay / 8) - 1) << 16));

    hsync_wid = (mode->CrtcHSyncEnd - mode->CrtcHSyncStart) / 8;
    if (!hsync_wid)       hsync_wid = 1;
    if (hsync_wid > 0x3f) hsync_wid = 0x3f;

    hsync_start = mode->CrtcHSyncStart - 8 + hsync_fudge;

    save->crtc_h_sync_strt_wid = ((hsync_start & 0xfff)
				 | (hsync_wid << 16)
				 | ((mode->Flags & V_NHSYNC)
				    ? R128_CRTC_H_SYNC_POL
				    : 0));

#if 1
				/* This works for double scan mode. */
    save->crtc_v_total_disp = (((mode->CrtcVTotal - 1) & 0xffff)
			      | ((mode->CrtcVDisplay - 1) << 16));
#else
				/* This is what cce/nbmode.c example code
				   does -- is this correct? */
    save->crtc_v_total_disp = (((mode->CrtcVTotal - 1) & 0xffff)
			      | ((mode->CrtcVDisplay
				  * ((mode->Flags & V_DBLSCAN) ? 2 : 1) - 1)
				 << 16));
#endif

    vsync_wid = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;
    if (!vsync_wid)       vsync_wid = 1;
    if (vsync_wid > 0x1f) vsync_wid = 0x1f;

    save->crtc_v_sync_strt_wid = (((mode->CrtcVSyncStart - 1) & 0xfff)
				 | (vsync_wid << 16)
				 | ((mode->Flags & V_NVSYNC)
				    ? R128_CRTC_V_SYNC_POL
				    : 0));
    save->crtc_offset      = 0;
    save->crtc_offset_cntl = 0;
    save->crtc_pitch       = info->CurrentLayout.displayWidth / 8;

    R128TRACE(("Pitch = %d bytes (virtualX = %d, displayWidth = %d)\n",
	       save->crtc_pitch, pScrn->virtualX, info->CurrentLayout.displayWidth));

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* Change the endianness of the aperture */
    switch (info->CurrentLayout.pixel_code) {
    case 15:
    case 16: save->config_cntl |= APER_0_BIG_ENDIAN_16BPP_SWAP; break;
    case 32: save->config_cntl |= APER_0_BIG_ENDIAN_32BPP_SWAP; break;
    default: break;
    }
#endif

    return TRUE;
}

/* Define CRTC registers for requested video mode. */
static void R128InitFPRegisters(R128SavePtr orig, R128SavePtr save,
				DisplayModePtr mode, R128InfoPtr info)
{
    int   xres = mode->CrtcHDisplay;
    int   yres = mode->CrtcVDisplay;
    float Hratio, Vratio;

    if (info->BIOSDisplay == R128_BIOS_DISPLAY_CRT) {
        save->crtc_ext_cntl  |= R128_CRTC_CRT_ON;
        save->crtc2_gen_cntl  = 0;
        save->fp_gen_cntl     = orig->fp_gen_cntl;
        save->fp_gen_cntl    &= ~(R128_FP_FPON |
            R128_FP_CRTC_USE_SHADOW_VEND |
            R128_FP_CRTC_HORZ_DIV2_EN |
            R128_FP_CRTC_HOR_CRT_DIV2_DIS |
            R128_FP_USE_SHADOW_EN);
        save->fp_gen_cntl    |= (R128_FP_SEL_CRTC2 |
                                 R128_FP_CRTC_DONT_SHADOW_VPAR);
        save->fp_panel_cntl   = orig->fp_panel_cntl & (CARD32)~R128_FP_DIGON;
        save->lvds_gen_cntl   = orig->lvds_gen_cntl &
				    (CARD32)~(R128_LVDS_ON | R128_LVDS_BLON);
        return;
    }

    if (xres > info->PanelXRes) xres = info->PanelXRes;
    if (yres > info->PanelYRes) yres = info->PanelYRes;

    Hratio = (float)xres/(float)info->PanelXRes;
    Vratio = (float)yres/(float)info->PanelYRes;

    save->fp_horz_stretch =
	(((((int)(Hratio * R128_HORZ_STRETCH_RATIO_MAX + 0.5))
	   & R128_HORZ_STRETCH_RATIO_MASK) << R128_HORZ_STRETCH_RATIO_SHIFT) |
       (orig->fp_horz_stretch & (R128_HORZ_PANEL_SIZE |
                                 R128_HORZ_FP_LOOP_STRETCH |
                                 R128_HORZ_STRETCH_RESERVED)));
    save->fp_horz_stretch &= ~R128_HORZ_AUTO_RATIO_FIX_EN;
    save->fp_horz_stretch &= ~R128_AUTO_HORZ_RATIO;
    if (xres == info->PanelXRes)
         save->fp_horz_stretch &= ~(R128_HORZ_STRETCH_BLEND | R128_HORZ_STRETCH_ENABLE);
    else
         save->fp_horz_stretch |=  (R128_HORZ_STRETCH_BLEND | R128_HORZ_STRETCH_ENABLE);

    save->fp_vert_stretch =
	(((((int)(Vratio * R128_VERT_STRETCH_RATIO_MAX + 0.5))
	   & R128_VERT_STRETCH_RATIO_MASK) << R128_VERT_STRETCH_RATIO_SHIFT) |
	 (orig->fp_vert_stretch & (R128_VERT_PANEL_SIZE |
				   R128_VERT_STRETCH_RESERVED)));
    save->fp_vert_stretch &= ~R128_VERT_AUTO_RATIO_EN;
    if (yres == info->PanelYRes)
        save->fp_vert_stretch &= ~(R128_VERT_STRETCH_ENABLE | R128_VERT_STRETCH_BLEND);
    else
        save->fp_vert_stretch |=  (R128_VERT_STRETCH_ENABLE | R128_VERT_STRETCH_BLEND);

    save->fp_gen_cntl = (orig->fp_gen_cntl &
			 (CARD32)~(R128_FP_SEL_CRTC2 |
				   R128_FP_CRTC_USE_SHADOW_VEND |
				   R128_FP_CRTC_HORZ_DIV2_EN |
				   R128_FP_CRTC_HOR_CRT_DIV2_DIS |
				   R128_FP_USE_SHADOW_EN));

    save->fp_panel_cntl        = orig->fp_panel_cntl;
    save->lvds_gen_cntl        = orig->lvds_gen_cntl;
    save->tmds_crc             = orig->tmds_crc;

    /* Disable CRT output by disabling CRT output and setting the CRT
       DAC to use CRTC2, which we set to 0's.  In the future, we will
       want to use the dual CRTC capabilities of the R128 to allow both
       the flat panel and external CRT to either simultaneously display
       the same image or display two different images. */

    if(!info->isDFP){
        if (info->BIOSDisplay == R128_BIOS_DISPLAY_FP_CRT) {
		save->crtc_ext_cntl  |= R128_CRTC_CRT_ON;
        } else {
		save->crtc_ext_cntl  &= ~R128_CRTC_CRT_ON;
		save->dac_cntl       |= R128_DAC_CRT_SEL_CRTC2;
		save->crtc2_gen_cntl  = 0;
        }
    }

    /* WARNING: Be careful about turning on the flat panel */
    if(info->isDFP){
        save->fp_gen_cntl = orig->fp_gen_cntl;

        save->fp_gen_cntl &= ~(R128_FP_CRTC_USE_SHADOW_VEND |
                               R128_FP_CRTC_USE_SHADOW_ROWCUR |
                               R128_FP_CRTC_HORZ_DIV2_EN |
                               R128_FP_CRTC_HOR_CRT_DIV2_DIS |
                               R128_FP_CRT_SYNC_SEL |
                               R128_FP_USE_SHADOW_EN);

        save->fp_panel_cntl  |= (R128_FP_DIGON | R128_FP_BLON);
        save->fp_gen_cntl    |= (R128_FP_FPON | R128_FP_TDMS_EN |
             R128_FP_CRTC_DONT_SHADOW_VPAR | R128_FP_CRTC_DONT_SHADOW_HEND);
        save->tmds_transmitter_cntl = (orig->tmds_transmitter_cntl
            & ~(CARD32)R128_TMDS_PLLRST) | R128_TMDS_PLLEN;
    }
    else
        save->lvds_gen_cntl  |= (R128_LVDS_ON | R128_LVDS_BLON);

    save->fp_crtc_h_total_disp = save->crtc_h_total_disp;
    save->fp_crtc_v_total_disp = save->crtc_v_total_disp;
    save->fp_h_sync_strt_wid   = save->crtc_h_sync_strt_wid;
    save->fp_v_sync_strt_wid   = save->crtc_v_sync_strt_wid;
}

/* Define PLL registers for requested video mode. */
static void R128InitPLLRegisters(ScrnInfoPtr pScrn, R128SavePtr save,
				R128PLLPtr pll, double dot_clock)
{
    unsigned long freq = dot_clock * 100;
    struct {
	int divider;
	int bitvalue;
    } *post_div,
      post_divs[]   = {
				/* From RAGE 128 VR/RAGE 128 GL Register
				   Reference Manual (Technical Reference
				   Manual P/N RRG-G04100-C Rev. 0.04), page
				   3-17 (PLL_DIV_[3:0]).  */
	{  1, 0 },              /* VCLK_SRC                 */
	{  2, 1 },              /* VCLK_SRC/2               */
	{  4, 2 },              /* VCLK_SRC/4               */
	{  8, 3 },              /* VCLK_SRC/8               */

	{  3, 4 },              /* VCLK_SRC/3               */
				/* bitvalue = 5 is reserved */
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
    save->feedback_div   = R128Div(pll->reference_div * save->pll_output_freq,
				   pll->reference_freq);
    save->post_div       = post_div->divider;

    R128TRACE(("dc=%d, of=%d, fd=%d, pd=%d\n",
	       save->dot_clock_freq,
	       save->pll_output_freq,
	       save->feedback_div,
	       save->post_div));

    save->ppll_ref_div   = pll->reference_div;
    save->ppll_div_3     = (save->feedback_div | (post_div->bitvalue << 16));
    save->htotal_cntl    = 0;

}

/* Define DDA registers for requested video mode. */
static Bool R128InitDDARegisters(ScrnInfoPtr pScrn, R128SavePtr save,
				 R128PLLPtr pll, R128InfoPtr info,
                                 DisplayModePtr mode)
{
    int         DisplayFifoWidth = 128;
    int         DisplayFifoDepth = 32;
    int         XclkFreq;
    int         VclkFreq;
    int         XclksPerTransfer;
    int         XclksPerTransferPrecise;
    int         UseablePrecision;
    int         Roff;
    int         Ron;

    XclkFreq = pll->xclk;

    VclkFreq = R128Div(pll->reference_freq * save->feedback_div,
		       pll->reference_div * save->post_div);

    if(info->isDFP && !info->isPro2){
        if(info->PanelXRes != mode->CrtcHDisplay)
            VclkFreq = (VclkFreq * mode->CrtcHDisplay)/info->PanelXRes;
	}

    XclksPerTransfer = R128Div(XclkFreq * DisplayFifoWidth,
			       VclkFreq * (info->CurrentLayout.pixel_bytes * 8));

    UseablePrecision = R128MinBits(XclksPerTransfer) + 1;

    XclksPerTransferPrecise = R128Div((XclkFreq * DisplayFifoWidth)
				      << (11 - UseablePrecision),
				      VclkFreq * (info->CurrentLayout.pixel_bytes * 8));

    Roff  = XclksPerTransferPrecise * (DisplayFifoDepth - 4);

    Ron   = (4 * info->ram->MB
	     + 3 * MAX(info->ram->Trcd - 2, 0)
	     + 2 * info->ram->Trp
	     + info->ram->Twr
	     + info->ram->CL
	     + info->ram->Tr2w
	     + XclksPerTransfer) << (11 - UseablePrecision);

    if (Ron + info->ram->Rloop >= Roff) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "(Ron = %d) + (Rloop = %d) >= (Roff = %d)\n",
		   Ron, info->ram->Rloop, Roff);
	return FALSE;
    }

    save->dda_config = (XclksPerTransferPrecise
			| (UseablePrecision << 16)
			| (info->ram->Rloop << 20));

    save->dda_on_off = (Ron << 16) | Roff;

    R128TRACE(("XclkFreq = %d; VclkFreq = %d; per = %d, %d (useable = %d)\n",
	       XclkFreq,
	       VclkFreq,
	       XclksPerTransfer,
	       XclksPerTransferPrecise,
	       UseablePrecision));
    R128TRACE(("Roff = %d, Ron = %d, Rloop = %d\n",
	       Roff, Ron, info->ram->Rloop));

    return TRUE;
}


#if 0
/* Define initial palette for requested video mode.  This doesn't do
   anything for XFree86 4.0. */
static void R128InitPalette(R128SavePtr save)
{
    save->palette_valid = FALSE;
}
#endif

/* Define registers for a requested video mode. */
static Bool R128Init(ScrnInfoPtr pScrn, DisplayModePtr mode, R128SavePtr save)
{
    R128InfoPtr info      = R128PTR(pScrn);
    double      dot_clock = mode->Clock/1000.0;

#if R128_DEBUG
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

    R128InitCommonRegisters(save, info);
    if (!R128InitCrtcRegisters(pScrn, save, mode, info)) return FALSE;
    if (info->HasPanelRegs || info->isDFP)
	    R128InitFPRegisters(&info->SavedReg, save, mode, info);
    if(dot_clock > 0){
        R128InitPLLRegisters(pScrn, save, &info->pll, dot_clock);
        if (!R128InitDDARegisters(pScrn, save, &info->pll, info, mode))
		return FALSE;
    }
    else{
        save->ppll_ref_div         = info->SavedReg.ppll_ref_div;
        save->ppll_div_3           = info->SavedReg.ppll_div_3;
        save->htotal_cntl          = info->SavedReg.htotal_cntl;
        save->dda_config           = info->SavedReg.dda_config;
        save->dda_on_off           = info->SavedReg.dda_on_off;
    }

    R128TRACE(("R128Init returns %p\n", save));
    return TRUE;
}

/* Initialize a new mode. */
static Bool R128ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    R128InfoPtr info      = R128PTR(pScrn);

    if (!R128Init(pScrn, mode, &info->ModeReg)) return FALSE;
				/* FIXME?  DRILock/DRIUnlock here? */
    pScrn->vtSema = TRUE;
    R128Blank(pScrn);
    R128RestoreMode(pScrn, &info->ModeReg);
    R128Unblank(pScrn);

    info->CurrentLayout.mode = mode;

    return TRUE;
}

static Bool R128SaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr   pScrn = xf86Screens[pScreen->myNum];
    Bool unblank;

    unblank = xf86IsUnblank(mode);
    if (unblank)
	SetTimeSinceLastInputEvent();

    if ((pScrn != NULL) && pScrn->vtSema) {
	if (unblank)
		R128Unblank(pScrn);
	else
		R128Blank(pScrn);
    }
    return TRUE;
}

Bool R128SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return R128ModeInit(xf86Screens[scrnIndex], mode);
}

/* Used to disallow modes that are not supported by the hardware. */
ModeStatus R128ValidMode(int scrnIndex, DisplayModePtr mode,
			 Bool verbose, int flags)
{
    ScrnInfoPtr   pScrn = xf86Screens[scrnIndex];
    R128InfoPtr   info  = R128PTR(pScrn);

    if(info->isDFP) {
        if(info->PanelXRes < mode->CrtcHDisplay ||
           info->PanelYRes < mode->CrtcVDisplay)
            return MODE_NOMODE;
        else
            return MODE_OK;
    }

    if (info->HasPanelRegs) {
	if (mode->Flags & V_INTERLACE) return MODE_NO_INTERLACE;
	if (mode->Flags & V_DBLSCAN)   return MODE_NO_DBLESCAN;
    }

    if (info->HasPanelRegs &&
	info->BIOSDisplay != R128_BIOS_DISPLAY_CRT &&
	info->VBIOS) {
	int i;
	for (i = info->FPBIOSstart+64; R128_BIOS16(i) != 0; i += 2) {
	    int j = R128_BIOS16(i);

	    if (mode->CrtcHDisplay == R128_BIOS16(j) &&
		mode->CrtcVDisplay == R128_BIOS16(j+2)) {
		if ((flags & MODECHECK_FINAL) == MODECHECK_FINAL) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "Modifying mode according to VBIOS: %ix%i [pclk %.1f MHz] for FP to: ",
			       mode->CrtcHDisplay,mode->CrtcVDisplay,
			       (float)mode->Clock/1000);

		    /* Assume we are using expanded mode */
		    if (R128_BIOS16(j+5)) j  = R128_BIOS16(j+5);
		    else                  j += 9;

		    mode->Clock = (CARD32)R128_BIOS16(j) * 10;

		    mode->HDisplay   = mode->CrtcHDisplay   =
			((R128_BIOS16(j+10) & 0x01ff)+1)*8;
		    mode->HSyncStart = mode->CrtcHSyncStart =
			((R128_BIOS16(j+12) & 0x01ff)+1)*8;
		    mode->HSyncEnd   = mode->CrtcHSyncEnd   =
			mode->CrtcHSyncStart + (R128_BIOS8(j+14) & 0x1f);
		    mode->HTotal     = mode->CrtcHTotal     =
			((R128_BIOS16(j+8)  & 0x01ff)+1)*8;

		    mode->VDisplay   = mode->CrtcVDisplay   =
			(R128_BIOS16(j+17) & 0x07ff)+1;
		    mode->VSyncStart = mode->CrtcVSyncStart =
			(R128_BIOS16(j+19) & 0x07ff)+1;
		    mode->VSyncEnd   = mode->CrtcVSyncEnd   =
			mode->CrtcVSyncStart + ((R128_BIOS16(j+19) >> 11) & 0x1f);
		    mode->VTotal     = mode->CrtcVTotal     =
			(R128_BIOS16(j+15) & 0x07ff)+1;
		    xf86ErrorF("%ix%i [pclk %.1f MHz]\n",
			       mode->CrtcHDisplay,mode->CrtcVDisplay,
			       (float)mode->Clock/1000);
		}
		return MODE_OK;
	    }
	}
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5,
		       "Mode rejected for FP %ix%i [pclk: %.1f] "
		       "(not listed in VBIOS)\n",
		       mode->CrtcHDisplay, mode->CrtcVDisplay,
		       (float)mode->Clock / 1000);
	return MODE_NOMODE;
    }

    return MODE_OK;
}

/* Adjust viewport into virtual desktop such that (0,0) in viewport space
   is (x,y) in virtual space. */
void R128AdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr   pScrn     = xf86Screens[scrnIndex];
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           Base;

    if(info->showCache && y && pScrn->vtSema)
        y += pScrn->virtualY - 1;

    Base = y * info->CurrentLayout.displayWidth + x;

    switch (info->CurrentLayout.pixel_code) {
    case 15:
    case 16: Base *= 2; break;
    case 24: Base *= 3; break;
    case 32: Base *= 4; break;
    }

    Base &= ~7;                 /* 3 lower bits are always 0 */

    if (info->CurrentLayout.pixel_code == 24)
	Base += 8 * (Base % 3); /* Must be multiple of 8 and 3 */

    OUTREG(R128_CRTC_OFFSET, Base);
}

/* Called when VT switching back to the X server.  Reinitialize the video
   mode. */
Bool R128EnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    R128InfoPtr info  = R128PTR(pScrn);

    R128TRACE(("R128EnterVT\n"));
    if (info->FBDev) {
        if (!fbdevHWEnterVT(scrnIndex,flags)) return FALSE;
    } else
        if (!R128ModeInit(pScrn, pScrn->currentMode)) return FALSE;
    if (info->accelOn)
	R128EngineInit(pScrn);

#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	if (info->irq) {
	    /* Need to make sure interrupts are enabled */
	    unsigned char *R128MMIO = info->MMIO;
	    OUTREG(R128_GEN_INT_CNTL, info->gen_int_cntl);
	}
	R128CCE_START(pScrn, info);
	DRIUnlock(pScrn->pScreen);
    }
#endif

    info->PaletteSavedOnVT = FALSE;
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    return TRUE;
}

/* Called when VT switching away from the X server.  Restore the original
   text mode. */
void R128LeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    R128InfoPtr info  = R128PTR(pScrn);
    R128SavePtr save  = &info->ModeReg;

    R128TRACE(("R128LeaveVT\n"));
#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	DRILock(pScrn->pScreen, 0);
	R128CCE_STOP(pScrn, info);
    }
#endif
    R128SavePalette(pScrn, save);
    info->PaletteSavedOnVT = TRUE;
    if (info->FBDev)
        fbdevHWLeaveVT(scrnIndex,flags);
    else
        R128Restore(pScrn);
}


/* Called at the end of each server generation.  Restore the original text
   mode, unmap video memory, and unwrap and call the saved CloseScreen
   function.  */
static Bool R128CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    R128InfoPtr info  = R128PTR(pScrn);

    R128TRACE(("R128CloseScreen\n"));

#ifdef XF86DRI
				/* Disable direct rendering */
    if (info->directRenderingEnabled) {
	R128DRICloseScreen(pScreen);
	info->directRenderingEnabled = FALSE;
    }
#endif

    if (pScrn->vtSema) {
	R128Restore(pScrn);
	R128UnmapMem(pScrn);
    }

    if (info->accel)             XAADestroyInfoRec(info->accel);
    info->accel                  = NULL;

    if (info->scratch_save)      xfree(info->scratch_save);
    info->scratch_save           = NULL;

    if (info->cursor)            xf86DestroyCursorInfoRec(info->cursor);
    info->cursor                 = NULL;

    if (info->DGAModes)          xfree(info->DGAModes);
    info->DGAModes               = NULL;

    if (info->adaptor) {
        xfree(info->adaptor->pPortPrivates[0].ptr);
	xf86XVFreeVideoAdaptorRec(info->adaptor);
	info->adaptor = NULL;
    }

    pScrn->vtSema = FALSE;

    pScreen->BlockHandler = info->BlockHandler;
    pScreen->CloseScreen = info->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

void R128FreeScreen(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    R128TRACE(("R128FreeScreen\n"));
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(pScrn);
    R128FreeRec(pScrn);
}

/* Sets VESA Display Power Management Signaling (DPMS) Mode.  */
static void R128DisplayPowerManagementSet(ScrnInfoPtr pScrn,
					  int PowerManagementMode, int flags)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           mask      = (R128_CRTC_DISPLAY_DIS
			       | R128_CRTC_HSYNC_DIS
			       | R128_CRTC_VSYNC_DIS);

    switch (PowerManagementMode) {
    case DPMSModeOn:
	/* Screen: On; HSync: On, VSync: On */
	OUTREGP(R128_CRTC_EXT_CNTL, 0, ~mask);
	break;
    case DPMSModeStandby:
	/* Screen: Off; HSync: Off, VSync: On */
	OUTREGP(R128_CRTC_EXT_CNTL,
		R128_CRTC_DISPLAY_DIS | R128_CRTC_HSYNC_DIS, ~mask);
	break;
    case DPMSModeSuspend:
	/* Screen: Off; HSync: On, VSync: Off */
	OUTREGP(R128_CRTC_EXT_CNTL,
		R128_CRTC_DISPLAY_DIS | R128_CRTC_VSYNC_DIS, ~mask);
	break;
    case DPMSModeOff:
	/* Screen: Off; HSync: Off, VSync: Off */
	OUTREGP(R128_CRTC_EXT_CNTL, mask, ~mask);
	break;
    }
}

static int r128_set_backlight_enable(ScrnInfoPtr pScrn, int on);

static void R128DisplayPowerManagementSetLCD(ScrnInfoPtr pScrn,
					  int PowerManagementMode, int flags)
{
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;
    int           mask      = R128_LVDS_DISPLAY_DIS;

    switch (PowerManagementMode) {
    case DPMSModeOn:
	/* Screen: On; HSync: On, VSync: On */
	OUTREGP(R128_LVDS_GEN_CNTL, 0, ~mask);
        r128_set_backlight_enable(pScrn, 1);
	break;
    case DPMSModeStandby:
	/* Fall through */
    case DPMSModeSuspend:
	/* Fall through */
	break;
    case DPMSModeOff:
	/* Screen: Off; HSync: Off, VSync: Off */
	OUTREGP(R128_LVDS_GEN_CNTL, mask, ~mask);
        r128_set_backlight_enable(pScrn, 0);
	break;
    }
}

static int r128_set_backlight_enable(ScrnInfoPtr pScrn, int on)
{
        R128InfoPtr info        = R128PTR(pScrn);
        unsigned char *R128MMIO = info->MMIO;
	unsigned int lvds_gen_cntl = INREG(R128_LVDS_GEN_CNTL);

	lvds_gen_cntl |= (/*R128_LVDS_BL_MOD_EN |*/ R128_LVDS_BLON);
	if (on) {
		lvds_gen_cntl |= R128_LVDS_DIGON;
		if (!lvds_gen_cntl & R128_LVDS_ON) {
			lvds_gen_cntl &= ~R128_LVDS_BLON;
			OUTREG(R128_LVDS_GEN_CNTL, lvds_gen_cntl);
			(void)INREG(R128_LVDS_GEN_CNTL);
			usleep(10000);
			lvds_gen_cntl |= R128_LVDS_BLON;
			OUTREG(R128_LVDS_GEN_CNTL, lvds_gen_cntl);
		}
#if 0
		lvds_gen_cntl &= ~R128_LVDS_BL_MOD_LEVEL_MASK;
		lvds_gen_cntl |= (0xFF /* backlight_conv[level] */ <<
				  R128_LVDS_BL_MOD_LEVEL_SHIFT);
#endif
		lvds_gen_cntl |= (R128_LVDS_ON | R128_LVDS_EN);
		lvds_gen_cntl &= ~R128_LVDS_DISPLAY_DIS;
	} else {
#if 0
		lvds_gen_cntl &= ~R128_LVDS_BL_MOD_LEVEL_MASK;
		lvds_gen_cntl |= (0xFF /* backlight_conv[0] */ <<
				  R128_LVDS_BL_MOD_LEVEL_SHIFT);
#endif
		lvds_gen_cntl |= R128_LVDS_DISPLAY_DIS;
		OUTREG(R128_LVDS_GEN_CNTL, lvds_gen_cntl);
		usleep(10);
		lvds_gen_cntl &= ~(R128_LVDS_ON | R128_LVDS_EN | R128_LVDS_BLON
				   | R128_LVDS_DIGON);
	}

	OUTREG(R128_LVDS_GEN_CNTL, lvds_gen_cntl);

	return 0;
 }
