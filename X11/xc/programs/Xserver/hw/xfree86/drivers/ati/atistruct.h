/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atistruct.h,v 1.37 2003/01/10 20:57:58 tsi Exp $ */
/*
 * Copyright 1999 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ___ATISTRUCT_H___
#define ___ATISTRUCT_H___ 1

#include "atibank.h"
#include "aticlock.h"
#include "atiregs.h"

#include "xaa.h"
#include "xf86Cursor.h"
#include "xf86Pci.h"
#include "xf86Resources.h"

#define CacheSlotOf(____Register) ((____Register) / UnitOf(DWORD_SELECT))

/*
 * This is probably as good a place as any to put this note, as it applies to
 * the entire driver, but especially here.  CARD8's are used rather than the
 * appropriate enum types because the latter would nearly quadruple storage
 * requirements (they are stored as int's).  This reduces the usefulness of
 * enum types to their ability to declare index values.  I've also elected to
 * forgo the strong typing capabilities of enum types.  C is not terribly adept
 * at strong typing anyway.
 */

/* A structure for local data related to video modes */
typedef struct _ATIHWRec
{
    /* Clock number for mode */
    CARD8 clock;

    /* The CRTC used to drive the screen (VGA, 8514, Mach64) */
    CARD8 crtc;

    /* Colour lookup table */
    CARD8 lut[256 * 3];

#ifndef AVOID_CPIO

    /* VGA registers */
    CARD8 genmo, crt[25], seq[5], gra[9], attr[21];

    /* VGA Wonder registers */
    CARD8             a3,         a6, a7,             ab, ac, ad, ae,
          b0, b1, b2, b3,     b5, b6,     b8, b9, ba,         bd, be, bf;

    /* Shadow VGA CRTC registers */
    CARD8 shadow_vga[25];

#endif /* AVOID_CPIO */

    /* Generic DAC registers */
    CARD8 dac_read, dac_write, dac_mask;

    /* IBM RGB 514 registers */
    CARD8 ibmrgb514[0x0092U];   /* All that's needed for now */

    /* Mach64 PLL registers */
    CARD8 pll_vclk_cntl, pll_vclk_post_div,
          pll_vclk0_fb_div, pll_vclk1_fb_div,
          pll_vclk2_fb_div, pll_vclk3_fb_div,
          pll_xclk_cntl, pll_ext_vpll_cntl;

    /* Mach64 CPIO registers */
    CARD32 crtc_h_total_disp, crtc_h_sync_strt_wid,
           crtc_v_total_disp, crtc_v_sync_strt_wid,
           crtc_off_pitch, crtc_gen_cntl, dsp_config, dsp_on_off,
           ovr_clr, ovr_wid_left_right, ovr_wid_top_bottom,
           cur_clr0, cur_clr1, cur_offset,
           cur_horz_vert_posn, cur_horz_vert_off,
           clock_cntl, bus_cntl, mem_cntl, mem_vga_wp_sel, mem_vga_rp_sel,
           dac_cntl, gen_test_cntl, config_cntl, mpp_config, mpp_strobe_seq,
           tvo_cntl;

    /* LCD registers */
    CARD32 lcd_index, config_panel, lcd_gen_ctrl,
           horz_stretching, vert_stretching, ext_vert_stretch;

    /* Shadow Mach64 CRTC registers */
    CARD32 shadow_h_total_disp, shadow_h_sync_strt_wid,
           shadow_v_total_disp, shadow_v_sync_strt_wid;

    /* Mach64 MMIO Block 0 registers and related subfields */
    CARD32 dst_off_pitch;
    CARD16 dst_x, dst_y, dst_height;
    CARD32 dst_bres_err, dst_bres_inc, dst_bres_dec, dst_cntl;
    CARD32 src_off_pitch;
    CARD16 src_x, src_y, src_width1, src_height1,
           src_x_start, src_y_start, src_width2, src_height2;
    CARD32 src_cntl;
    CARD32 host_cntl;
    CARD32 pat_reg0, pat_reg1, pat_cntl;
    CARD16 sc_left, sc_right, sc_top, sc_bottom;
    CARD32 dp_bkgd_clr, dp_frgd_clr, dp_write_mask, dp_chain_mask,
           dp_pix_width, dp_mix, dp_src;
    CARD32 clr_cmp_clr, clr_cmp_msk, clr_cmp_cntl;
    CARD32 context_mask, context_load_cntl;

    /* Mach64 MMIO Block 1 registers */
    CARD32 gui_cntl;

    /* Clock map pointers */
    const CARD8 *ClockMap, *ClockUnmap;

    /* Clock programming data */
    int FeedbackDivider, ReferenceDivider, PostDivider;

#ifndef AVOID_CPIO

    /* This is used by ATISwap() */
    pointer frame_buffer;
    ATIBankProcPtr SetBank;
    unsigned int nBank, nPlane;

#endif /* AVOID_CPIO */

} ATIHWRec;

/*
 * This structure defines the driver's private area.
 */
typedef struct _ATIRec
{
    /*
     * Definitions related to XF86Config "Chipset" specifications.
     */
    CARD8 Chipset;

    /*
     * Adapter-related definitions.
     */
    CARD8 Adapter;

#ifndef AVOID_CPIO

    CARD8 VGAAdapter;

#endif /* AVOID_CPIO */

    /*
     * Chip-related definitions.
     */
    CARD32 config_chip_id;
    CARD16 ChipType;
    CARD8 Chip;
    CARD8 ChipClass, ChipRevision, ChipRev, ChipVersion, ChipFoundry;

#ifndef AVOID_CPIO

    CARD8 Coprocessor, ChipHasSUBSYS_CNTL;

#endif /* AVOID_CPIO */

    /*
     * Processor I/O decoding definitions.
     */
    CARD8 CPIODecoding;
    IOADDRESS CPIOBase;

#ifndef AVOID_CPIO

    /*
     * Processor I/O port definition for VGA.
     */
    IOADDRESS CPIO_VGABase;

    /*
     * Processor I/O port definitions for VGA Wonder.
     */
    IOADDRESS CPIO_VGAWonder;
    CARD8 B2Reg;        /* The B2 mirror */
    CARD8 VGAOffset;    /* Low index for CPIO_VGAWonder */

#endif /* AVOID_CPIO */

    /*
     * DAC-related definitions.
     */

#ifndef AVOID_CPIO

    IOADDRESS CPIO_DAC_MASK, CPIO_DAC_DATA, CPIO_DAC_READ, CPIO_DAC_WRITE,
              CPIO_DAC_WAIT;

#endif /* AVOID_CPIO */

    CARD16 DAC;
    CARD8 rgbBits;

    /*
     * Definitions related to system bus interface.
     */
    pciVideoPtr PCIInfo;
    CARD8 BusType;
    CARD8 SharedAccelerator;

#ifndef AVOID_CPIO

    CARD8 SharedVGA;
    resRange VGAWonderResources[2];

#endif /* AVOID_CPIO */

    /*
     * Definitions related to video memory.
     */
    CARD8 MemoryType;
    int VideoRAM;

    /*
     * BIOS-related definitions.
     */
    unsigned long BIOSBase;

    /*
     * Definitions related to video memory apertures.
     */
    pointer pMemory, pShadow;
    unsigned long LinearBase;
    int LinearSize, FBPitch, FBBytesPerPixel;

#ifndef AVOID_CPIO

    /*
     * Banking interface.
     */
    miBankInfoRec BankInfo;
    pointer pBank;
    CARD8 UseSmallApertures;

#endif /* AVOID_CPIO */

    /*
     * Definitions related to MMIO register apertures.
     */
    pointer pMMIO, pBlock[2];
    unsigned long Block0Base, Block1Base;

    /*
     * XAA interface.
     */
    XAAInfoRecPtr pXAAInfo;
    int nAvailableFIFOEntries, nFIFOEntries, nHostFIFOEntries;
    CARD8 EngineIsBusy, EngineIsLocked, XModifier;
    CARD32 dst_cntl;    /* For SetupFor/Subsequent communication */
    CARD32 sc_left_right, sc_top_bottom;
    CARD16 sc_left, sc_right, sc_top, sc_bottom;        /* Current scissors */
    pointer pHOST_DATA; /* Current HOST_DATA_* transfer window address */
    CARD32 *ExpansionBitmapScanlinePtr[2];
    int ExpansionBitmapWidth;

    /*
     * Cursor-related definitions.
     */
    xf86CursorInfoPtr pCursorInfo;
    pointer pCursorPage, pCursorImage;
    unsigned long CursorBase;
    CARD32 CursorOffset;
    CARD16 CursorXOffset, CursorYOffset;
    CARD8 Cursor;

    /*
     * MMIO cache.
     */
    CARD32 MMIOCache[CacheSlotOf(DWORD_SELECT) + 1];
    CARD8  MMIOCached[(CacheSlotOf(DWORD_SELECT) + 8) >> 3];

    /*
     * Clock-related definitions.
     */
    int ClockNumberToProgramme, ReferenceNumerator, ReferenceDenominator;
    int ProgrammableClock, maxClock;
    ClockRec ClockDescriptor;
    CARD16 BIOSClocks[16];
    CARD8 Clock;

    /*
     * DSP register data.
     */
    int XCLKFeedbackDivider, XCLKReferenceDivider, XCLKPostDivider;
    CARD16 XCLKMaxRASDelay, XCLKPageFaultDelay,
           DisplayLoopLatency, DisplayFIFODepth;

    /*
     * LCD panel data.
     */
    int LCDPanelID, LCDClock, LCDHorizontal, LCDVertical;
    int LCDHSyncStart, LCDHSyncWidth, LCDHBlankWidth;
    int LCDVSyncStart, LCDVSyncWidth, LCDVBlankWidth;
    int LCDVBlendFIFOSize;

    /*
     * Data used by ATIAdjustFrame().
     */
    int AdjustDepth, AdjustMaxX, AdjustMaxY;
    unsigned long AdjustMask, AdjustMaxBase;

    /*
     * DGA and non-DGA common data.
     */
    DisplayModePtr currentMode;
    CARD8 depth, bitsPerPixel;
    short int displayWidth;
    int pitchInc;
    rgb weight;

#ifndef AVOID_DGA

    /*
     * DGA-related data.
     */
    DGAModePtr pDGAMode;
    DGAFunctionRec ATIDGAFunctions;
    int nDGAMode;

    /*
     * XAAForceTransBlit alters the behavior of 'SetupForScreenToScreenCopy',
     * such that ~0 is interpreted as a legitimate transparency key.
     */
    CARD8 XAAForceTransBlit;

#endif /* AVOID_DGA */

    /*
     * Data saved by ATIUnlock() and restored by ATILock().
     */
    struct
    {
        /* Mach64 registers */
        CARD32 crtc_int_cntl, crtc_gen_cntl, i2c_cntl_0, hw_debug,
               scratch_reg3, bus_cntl, lcd_index, mem_cntl, i2c_cntl_1,
               dac_cntl, gen_test_cntl, mpp_config, mpp_strobe_seq, tvo_cntl;

#ifndef AVOID_CPIO

        CARD32 config_cntl;

        /* Mach8/Mach32 registers */
        CARD16 clock_sel, misc_options, mem_bndry, mem_cfg;

        /* VGA Wonder registers */
        CARD8 a6, ab, b1, b4, b5, b6, b8, b9, be;

        /* VGA registers */
        CARD8 crt03, crt11;

        /* VGA shadow registers */
        CARD8 shadow_crt03, shadow_crt11;

#endif /* AVOID_CPIO */

    } LockData;

    /* Mode data */
    ATIHWRec OldHW, NewHW;
    int MaximumInterlacedPitch;
    Bool InterlacedSeen;

    /*
     * Resource Access Control entity index.
     */
    int iEntity;

    /*
     * Driver options.
     */
    CARD8 OptionAccel;          /* Use hardware draw engine */
    CARD8 OptionBlend;          /* Force horizontal blending */
    CARD8 OptionCRTDisplay;     /* Display on both CRT and digital panel */
    CARD8 OptionCSync;          /* Use composite sync */
    CARD8 OptionDevel;          /* Intentionally undocumented */

#ifndef AVOID_CPIO

    CARD8 OptionLinear;         /* Use linear fb aperture when available */

#endif /* AVOID_CPIO */

    CARD8 OptionMMIOCache;      /* Cache MMIO writes */
    CARD8 OptionPanelDisplay;   /* Prefer CRT over digital panel */
    CARD8 OptionProbeClocks;    /* Force probe for fixed clocks */
    CARD8 OptionShadowFB;       /* Use shadow frame buffer */
    CARD8 OptionSync;           /* Temporary */

    /*
     * State flags.
     */
    CARD8 Unlocked, Mapped, Closeable;
    CARD8 MMIOInLinear;

    /*
     * Wrapped functions.
     */
    CloseScreenProcPtr CloseScreen;
} ATIRec;

#define ATIPTR(_p) ((ATIPtr)((_p)->driverPrivate))

#endif /* ___ATISTRUCT_H___ */
