/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64accel.c,v 1.2 2004/01/05 16:42:02 tsi Exp $ */
/*
 * Copyright 2003 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
/*
 * Copyright 1999-2000 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ati.h"
#include "atichip.h"
#include "atimach64accel.h"
#include "atimach64io.h"
#include "atipriv.h"
#include "atiregs.h"

#include "miline.h"

/* Used to test MMIO cache integrity in ATIMach64Sync() */
#define TestRegisterCaching(_Register)                   \
    if (RegisterIsCached(_Register) &&                   \
        (CacheSlot(_Register) != inm(_Register)))        \
    {                                                    \
        UncacheRegister(_Register);                      \
        xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,    \
            #_Register " MMIO write cache disabled!\n"); \
    }

/*
 * X-to-Mach64 mix translation table.
 */
static CARD8 ATIMach64ALU[16] =
{
    MIX_0,                       /* GXclear */
    MIX_AND,                     /* GXand */
    MIX_SRC_AND_NOT_DST,         /* GXandReverse */
    MIX_SRC,                     /* GXcopy */
    MIX_NOT_SRC_AND_DST,         /* GXandInverted */
    MIX_DST,                     /* GXnoop */
    MIX_XOR,                     /* GXxor */
    MIX_OR,                      /* GXor */
    MIX_NOR,                     /* GXnor */
    MIX_XNOR,                    /* GXequiv */
    MIX_NOT_DST,                 /* GXinvert */
    MIX_SRC_OR_NOT_DST,          /* GXorReverse */
    MIX_NOT_SRC,                 /* GXcopyInverted */
    MIX_NOT_SRC_OR_DST,          /* GXorInverted */
    MIX_NAND,                    /* GXnand */
    MIX_1                        /* GXset */
};

/*
 * ATIMach64ValidateClip --
 *
 * This function ensures the current scissor settings do not interfere with
 * the current draw request.
 */
static void
ATIMach64ValidateClip
(
    ATIPtr pATI,
    int    sc_left,
    int    sc_right,
    int    sc_top,
    int    sc_bottom
)
{
    if ((sc_left < (int)pATI->sc_left) || (sc_right > (int)pATI->sc_right))
    {
        outf(SC_LEFT_RIGHT, pATI->sc_left_right);
        pATI->sc_left = pATI->NewHW.sc_left;
        pATI->sc_right = pATI->NewHW.sc_right;
    }

    if ((sc_top < (int)pATI->sc_top) || (sc_bottom > (int)pATI->sc_bottom))
    {
        outf(SC_TOP_BOTTOM, pATI->sc_top_bottom);
        pATI->sc_top = pATI->NewHW.sc_top;
        pATI->sc_bottom = pATI->NewHW.sc_bottom;
    }
}

/*
 * ATIMach64Sync --
 *
 * This is called to wait for the draw engine to become idle.
 */
void
ATIMach64Sync
(
    ScrnInfoPtr pScreenInfo
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForIdle(pATI);
    if (pATI->pXAAInfo)
        pATI->pXAAInfo->NeedToSync = FALSE;

    if (pATI->OptionMMIOCache && pATI->OptionTestMMIOCache)
    {
        /*
         * For debugging purposes, attempt to verify that each cached register
         * should actually be cached.
         */
        TestRegisterCaching(SRC_CNTL);

        TestRegisterCaching(HOST_CNTL);

        TestRegisterCaching(PAT_REG0);
        TestRegisterCaching(PAT_REG1);
        TestRegisterCaching(PAT_CNTL);

        if (RegisterIsCached(SC_LEFT_RIGHT) &&  /* Special case */
            (CacheSlot(SC_LEFT_RIGHT) !=
             (SetWord(inm(SC_RIGHT), 1) | SetWord(inm(SC_LEFT), 0))))
        {
            UncacheRegister(SC_LEFT_RIGHT);
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "SC_LEFT_RIGHT write cache disabled!\n");
        }

        if (RegisterIsCached(SC_TOP_BOTTOM) &&  /* Special case */
            (CacheSlot(SC_TOP_BOTTOM) !=
             (SetWord(inm(SC_BOTTOM), 1) | SetWord(inm(SC_TOP), 0))))
        {
            UncacheRegister(SC_TOP_BOTTOM);
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "SC_TOP_BOTTOM write cache disabled!\n");
        }

        TestRegisterCaching(DP_BKGD_CLR);
        TestRegisterCaching(DP_FRGD_CLR);
        TestRegisterCaching(DP_WRITE_MASK);
        TestRegisterCaching(DP_MIX);

        TestRegisterCaching(CLR_CMP_CLR);
        TestRegisterCaching(CLR_CMP_MSK);
        TestRegisterCaching(CLR_CMP_CNTL);

        if (pATI->Block1Base)
        {
            TestRegisterCaching(OVERLAY_Y_X_START);
            TestRegisterCaching(OVERLAY_Y_X_END);

            TestRegisterCaching(OVERLAY_GRAPHICS_KEY_CLR);
            TestRegisterCaching(OVERLAY_GRAPHICS_KEY_MSK);

            TestRegisterCaching(OVERLAY_KEY_CNTL);

            TestRegisterCaching(OVERLAY_SCALE_INC);
            TestRegisterCaching(OVERLAY_SCALE_CNTL);

            TestRegisterCaching(SCALER_HEIGHT_WIDTH);

            TestRegisterCaching(SCALER_TEST);

            TestRegisterCaching(VIDEO_FORMAT);

            if (pATI->Chip < ATI_CHIP_264VTB)
            {
                TestRegisterCaching(BUF0_OFFSET);
                TestRegisterCaching(BUF0_PITCH);
                TestRegisterCaching(BUF1_OFFSET);
                TestRegisterCaching(BUF1_PITCH);
            }
            else
            {
                TestRegisterCaching(SCALER_BUF0_OFFSET);
                TestRegisterCaching(SCALER_BUF1_OFFSET);
                TestRegisterCaching(SCALER_BUF_PITCH);

                TestRegisterCaching(OVERLAY_EXCLUSIVE_HORZ);
                TestRegisterCaching(OVERLAY_EXCLUSIVE_VERT);

                if (pATI->Chip >= ATI_CHIP_264GTPRO)
                {
                    TestRegisterCaching(SCALER_COLOUR_CNTL);

                    TestRegisterCaching(SCALER_H_COEFF0);
                    TestRegisterCaching(SCALER_H_COEFF1);
                    TestRegisterCaching(SCALER_H_COEFF2);
                    TestRegisterCaching(SCALER_H_COEFF3);
                    TestRegisterCaching(SCALER_H_COEFF4);

                    TestRegisterCaching(SCALER_BUF0_OFFSET_U);
                    TestRegisterCaching(SCALER_BUF0_OFFSET_V);
                    TestRegisterCaching(SCALER_BUF1_OFFSET_U);
                    TestRegisterCaching(SCALER_BUF1_OFFSET_V);
                }
            }
        }
    }

    /*
     * For VTB's and later, the first CPU read of the framebuffer will return
     * zeroes, so do it here.  This appears to be due to some kind of engine
     * caching of framebuffer data I haven't found any way of disabling, or
     * otherwise circumventing.  Thanks to Mark Vojkovich for the suggestion.
     */
    pATI = *(volatile ATIPtr *)pATI->pMemory;
}

/*
 * ATIMach64SetupForScreenToScreenCopy --
 *
 * This function sets up the draw engine for a series of screen-to-screen copy
 * operations.
 */
static void
ATIMach64SetupForScreenToScreenCopy
(
    ScrnInfoPtr  pScreenInfo,
    int          xdir,
    int          ydir,
    int          rop,
    unsigned int planemask,
    int          TransparencyColour
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForFIFO(pATI, 3);
    outf(DP_WRITE_MASK, planemask);
    outf(DP_SRC, DP_MONO_SRC_ALLONES |
        SetBits(SRC_BLIT, DP_FRGD_SRC) | SetBits(SRC_BKGD, DP_BKGD_SRC));
    outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX));

#ifdef AVOID_DGA

    if (TransparencyColour == -1)

#else /* AVOID_DGA */

    if (!pATI->XAAForceTransBlit && (TransparencyColour == -1))

#endif /* AVOID_DGA */

    {
        outf(CLR_CMP_CNTL, CLR_CMP_FN_FALSE);
    }
    else
    {
        ATIMach64WaitForFIFO(pATI, 2);
        outf(CLR_CMP_CLR, TransparencyColour);
        outf(CLR_CMP_CNTL, CLR_CMP_FN_EQUAL | CLR_CMP_SRC_2D);
    }

    pATI->dst_cntl = 0;

    if (ydir > 0)
        pATI->dst_cntl |= DST_Y_DIR;
    if (xdir > 0)
        pATI->dst_cntl |= DST_X_DIR;

    if (pATI->XModifier == 1)
        outf(DST_CNTL, pATI->dst_cntl);
    else
        pATI->dst_cntl |= DST_24_ROT_EN;
}

/*
 * ATIMach64SubsequentScreenToScreenCopy --
 *
 * This function performs a screen-to-screen copy operation.
 */
static void
ATIMach64SubsequentScreenToScreenCopy
(
    ScrnInfoPtr pScreenInfo,
    int         xSrc,
    int         ySrc,
    int         xDst,
    int         yDst,
    int         w,
    int         h
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    xSrc *= pATI->XModifier;
    xDst *= pATI->XModifier;
    w    *= pATI->XModifier;

    /* Disable clipping if it gets in the way */
    ATIMach64ValidateClip(pATI, xDst, xDst + w - 1, yDst, yDst + h - 1);

    if (!(pATI->dst_cntl & DST_X_DIR))
    {
        xSrc += w - 1;
        xDst += w - 1;
    }

    if (!(pATI->dst_cntl & DST_Y_DIR))
    {
        ySrc += h - 1;
        yDst += h - 1;
    }

    if (pATI->XModifier != 1)
        outf(DST_CNTL, pATI->dst_cntl | SetBits((xDst / 4) % 6, DST_24_ROT));

    ATIMach64WaitForFIFO(pATI, 4);
    outf(SRC_Y_X, SetWord(xSrc, 1) | SetWord(ySrc, 0));
    outf(SRC_WIDTH1, w);
    outf(DST_Y_X, SetWord(xDst, 1) | SetWord(yDst, 0));
    outf(DST_HEIGHT_WIDTH, SetWord(w, 1) | SetWord(h, 0));
}

/*
 * ATIMach64SetupForSolidFill --
 *
 * This function sets up the draw engine for a series of solid fills.
 */
static void
ATIMach64SetupForSolidFill
(
    ScrnInfoPtr  pScreenInfo,
    int          colour,
    int          rop,
    unsigned int planemask
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForFIFO(pATI, 5);
    outf(DP_WRITE_MASK, planemask);
    outf(DP_SRC, DP_MONO_SRC_ALLONES |
        SetBits(SRC_FRGD, DP_FRGD_SRC) | SetBits(SRC_BKGD, DP_BKGD_SRC));
    outf(DP_FRGD_CLR, colour);
    outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX));

    outf(CLR_CMP_CNTL, CLR_CMP_FN_FALSE);

    if (pATI->XModifier == 1)
        outf(DST_CNTL, DST_X_DIR | DST_Y_DIR);
}

/*
 * ATIMach64SubsequentSolidFillRect --
 *
 * This function performs a solid rectangle fill.
 */
static void
ATIMach64SubsequentSolidFillRect
(
    ScrnInfoPtr pScreenInfo,
    int         x,
    int         y,
    int         w,
    int         h
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    if (pATI->XModifier != 1)
    {
        x *= pATI->XModifier;
        w *= pATI->XModifier;

        outf(DST_CNTL, SetBits((x / 4) % 6, DST_24_ROT) |
            (DST_X_DIR | DST_Y_DIR | DST_24_ROT_EN));
    }

    /* Disable clipping if it gets in the way */
    ATIMach64ValidateClip(pATI, x, x + w - 1, y, y + h - 1);

    ATIMach64WaitForFIFO(pATI, 2);
    outf(DST_Y_X, SetWord(x, 1) | SetWord(y, 0));
    outf(DST_HEIGHT_WIDTH, SetWord(w, 1) | SetWord(h, 0));
}

/*
 * ATIMach64SetupForSolidLine --
 *
 * This function sets up the draw engine for a series of solid lines.  It is
 * not used for 24bpp because the engine doesn't support it.
 */
static void
ATIMach64SetupForSolidLine
(
    ScrnInfoPtr  pScreenInfo,
    int          colour,
    int          rop,
    unsigned int planemask
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForFIFO(pATI, 5);
    outf(DP_WRITE_MASK, planemask);
    outf(DP_SRC, DP_MONO_SRC_ALLONES |
        SetBits(SRC_FRGD, DP_FRGD_SRC) | SetBits(SRC_BKGD, DP_BKGD_SRC));
    outf(DP_FRGD_CLR, colour);
    outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX));

    outf(CLR_CMP_CNTL, CLR_CMP_FN_FALSE);

    ATIMach64ValidateClip(pATI, pATI->NewHW.sc_left, pATI->NewHW.sc_right,
        pATI->NewHW.sc_top, pATI->NewHW.sc_bottom);
}

/*
 * ATIMach64SubsequentSolidHorVertLine --
 *
 * This is called to draw a solid horizontal or vertical line.  This does a
 * one-pixel wide solid fill.
 */
static void
ATIMach64SubsequentSolidHorVertLine
(
    ScrnInfoPtr pScreenInfo,
    int         x,
    int         y,
    int         len,
    int         dir
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForFIFO(pATI, 3);
    outf(DST_CNTL, DST_X_DIR | DST_Y_DIR);
    outf(DST_Y_X, SetWord(x, 1) | SetWord(y, 0));

    if (dir == DEGREES_0)
        outf(DST_HEIGHT_WIDTH, SetWord(len, 1) | SetWord(1, 0));
    else /* if (dir == DEGREES_270) */
        outf(DST_HEIGHT_WIDTH, SetWord(1, 1) | SetWord(len, 0));
}

/*
 * ATIMach64SubsequentSolidBresenhamLine --
 *
 * This function draws a line using the Bresenham line engine.
 */
static void
ATIMach64SubsequentSolidBresenhamLine
(
    ScrnInfoPtr pScreenInfo,
    int         x,
    int         y,
    int         major,
    int         minor,
    int         err,
    int         len,
    int         octant
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);
    CARD32 dst_cntl = DST_LAST_PEL;

    if (octant & YMAJOR)
        dst_cntl |= DST_Y_MAJOR;

    if (!(octant & XDECREASING))
        dst_cntl |= DST_X_DIR;

    if (!(octant & YDECREASING))
        dst_cntl |= DST_Y_DIR;

    ATIMach64WaitForFIFO(pATI, 6);
    outf(DST_CNTL, dst_cntl);
    outf(DST_Y_X, SetWord(x, 1) | SetWord(y, 0));
    outf(DST_BRES_ERR, minor + err);
    outf(DST_BRES_INC, minor);
    outf(DST_BRES_DEC, minor - major);
    outf(DST_BRES_LNTH, len);
}

/*
 * ATIMach64SetupForMono8x8PatternFill --
 *
 * This function sets up the draw engine for a series of 8x8 1bpp pattern
 * fills.
 */
static void
ATIMach64SetupForMono8x8PatternFill
(
    ScrnInfoPtr  pScreenInfo,
    int          patx,
    int          paty,
    int          fg,
    int          bg,
    int          rop,
    unsigned int planemask
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForFIFO(pATI, 3);
    outf(DP_WRITE_MASK, planemask);
    outf(DP_SRC, DP_MONO_SRC_PATTERN |
        SetBits(SRC_FRGD, DP_FRGD_SRC) | SetBits(SRC_BKGD, DP_BKGD_SRC));
    outf(DP_FRGD_CLR, fg);

    if (bg == -1)
    {
        outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX) |
            SetBits(MIX_DST, DP_BKGD_MIX));
    }
    else
    {
        ATIMach64WaitForFIFO(pATI, 2);
        outf(DP_BKGD_CLR, bg);
        outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX) |
            SetBits(ATIMach64ALU[rop], DP_BKGD_MIX));
    }

    ATIMach64WaitForFIFO(pATI, 4);
    outf(PAT_REG0, patx);
    outf(PAT_REG1, paty);
    outf(PAT_CNTL, PAT_MONO_EN);

    outf(CLR_CMP_CNTL, CLR_CMP_FN_FALSE);

    if (pATI->XModifier == 1)
        outf(DST_CNTL, DST_X_DIR | DST_Y_DIR);
}

/*
 * ATIMach64SubsequentMono8x8PatternFillRect --
 *
 * This function performs an 8x8 1bpp pattern fill.
 */
static void
ATIMach64SubsequentMono8x8PatternFillRect
(
    ScrnInfoPtr pScreenInfo,
    int         patx,
    int         paty,
    int         x,
    int         y,
    int         w,
    int         h
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    if (pATI->XModifier != 1)
    {
        x *= pATI->XModifier;
        w *= pATI->XModifier;

        outf(DST_CNTL, SetBits((x / 4) % 6, DST_24_ROT) |
            (DST_X_DIR | DST_Y_DIR | DST_24_ROT_EN));
    }

    /* Disable clipping if it gets in the way */
    ATIMach64ValidateClip(pATI, x, x + w - 1, y, y + h - 1);

    ATIMach64WaitForFIFO(pATI, 2);
    outf(DST_Y_X, SetWord(x, 1) | SetWord(y, 0));
    outf(DST_HEIGHT_WIDTH, SetWord(w, 1) | SetWord(h, 0));
}

/*
 * ATIMach64SetupForScanlineCPUToScreenColorExpandFill --
 *
 * This function sets up the engine for a series of colour expansion fills.
 */
static void
ATIMach64SetupForScanlineCPUToScreenColorExpandFill
(
    ScrnInfoPtr  pScreenInfo,
    int          fg,
    int          bg,
    int          rop,
    unsigned int planemask
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    ATIMach64WaitForFIFO(pATI, 3);
    outf(DP_WRITE_MASK, planemask);
    outf(DP_SRC, DP_MONO_SRC_HOST |
        SetBits(SRC_FRGD, DP_FRGD_SRC) | SetBits(SRC_BKGD, DP_BKGD_SRC));
    outf(DP_FRGD_CLR, fg);

    if (bg == -1)
    {
        outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX) |
            SetBits(MIX_DST, DP_BKGD_MIX));
    }
    else
    {
        ATIMach64WaitForFIFO(pATI, 2);
        outf(DP_BKGD_CLR, bg);
        outf(DP_MIX, SetBits(ATIMach64ALU[rop], DP_FRGD_MIX) |
            SetBits(ATIMach64ALU[rop], DP_BKGD_MIX));
    }

    outf(CLR_CMP_CNTL, CLR_CMP_FN_FALSE);

    if (pATI->XModifier == 1)
        outf(DST_CNTL, DST_X_DIR | DST_Y_DIR);
}

/*
 * ATIMach64SubsequentScanlineCPUToScreenColorExpandFill --
 *
 * This function sets up the engine for a single colour expansion fill.
 */
static void
ATIMach64SubsequentScanlineCPUToScreenColorExpandFill
(
    ScrnInfoPtr pScreenInfo,
    int         x,
    int         y,
    int         w,
    int         h,
    int         skipleft
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    if (pATI->XModifier != 1)
    {
        x *= pATI->XModifier;
        w *= pATI->XModifier;
        skipleft *= pATI->XModifier;

        outf(DST_CNTL, SetBits((x / 4) % 6, DST_24_ROT) |
            (DST_X_DIR | DST_Y_DIR | DST_24_ROT_EN));
    }

    pATI->ExpansionBitmapWidth = (w + 31) / 32;

    ATIMach64WaitForFIFO(pATI, 3);
    pATI->sc_left = x + skipleft;
    pATI->sc_right = x + w - 1;
    outf(SC_LEFT_RIGHT,
        SetWord(pATI->sc_right, 1) | SetWord(pATI->sc_left, 0));
    outf(DST_Y_X, SetWord(x, 1) | SetWord(y, 0));
    outf(DST_HEIGHT_WIDTH,
        SetWord(pATI->ExpansionBitmapWidth * 32, 1) | SetWord(h, 0));
}

/*
 * ATIMach64SubsequentColorExpandScanline --
 *
 * This function feeds a bitmap scanline to the engine for a colour expansion
 * fill.  This is written to do burst transfers for those platforms that can do
 * them, and to improve CPU/engine concurrency.
 */
static void
ATIMach64SubsequentColorExpandScanline
(
    ScrnInfoPtr pScreenInfo,
    int         iBuffer
)
{
    ATIPtr          pATI         = ATIPTR(pScreenInfo);
    CARD32          *pBitmapData = pATI->ExpansionBitmapScanlinePtr[iBuffer];
    int             w            = pATI->ExpansionBitmapWidth;
    int             nDWord;

    while (w > 0)
    {
        /*
         * Transfers are done in chunks of up to 64 bytes in length (32 on
         * earlier controllers).
         */
        nDWord = w;
        if (nDWord > pATI->nHostFIFOEntries)
            nDWord = pATI->nHostFIFOEntries;

        /* Make enough FIFO slots available */
        ATIMach64WaitForFIFO(pATI, nDWord);

        /*
         * Always start transfers on a chuck-sized boundary.  Note that
         * HOST_DATA_0 is actually on a 512-byte boundary, but *pBitmapData can
         * only be guaranteed to be on a chunk-sized boundary.
         *
         * Transfer current chunk.  With any luck, the compiler won't mangle
         * this too badly...
         */

#       if defined(ATIMove32)

        {
            ATIMove32(pATI->pHOST_DATA, pBitmapData, nDWord);
        }

#       else

        {
            volatile CARD32 *pDst;
            CARD32          *pSrc;
            unsigned int    iDWord;

            iDWord = 16 - nDWord;
            pDst = (volatile CARD32 *)pATI->pHOST_DATA - iDWord;
            pSrc = pBitmapData - iDWord;

            switch (iDWord)
            {
                case  0:  MMIO_MOVE32(pDst +  0, 0, *(pSrc +  0));
                case  1:  MMIO_MOVE32(pDst +  1, 0, *(pSrc +  1));
                case  2:  MMIO_MOVE32(pDst +  2, 0, *(pSrc +  2));
                case  3:  MMIO_MOVE32(pDst +  3, 0, *(pSrc +  3));
                case  4:  MMIO_MOVE32(pDst +  4, 0, *(pSrc +  4));
                case  5:  MMIO_MOVE32(pDst +  5, 0, *(pSrc +  5));
                case  6:  MMIO_MOVE32(pDst +  6, 0, *(pSrc +  6));
                case  7:  MMIO_MOVE32(pDst +  7, 0, *(pSrc +  7));
                case  8:  MMIO_MOVE32(pDst +  8, 0, *(pSrc +  8));
                case  9:  MMIO_MOVE32(pDst +  9, 0, *(pSrc +  9));
                case 10:  MMIO_MOVE32(pDst + 10, 0, *(pSrc + 10));
                case 11:  MMIO_MOVE32(pDst + 11, 0, *(pSrc + 11));
                case 12:  MMIO_MOVE32(pDst + 12, 0, *(pSrc + 12));
                case 13:  MMIO_MOVE32(pDst + 13, 0, *(pSrc + 13));
                case 14:  MMIO_MOVE32(pDst + 14, 0, *(pSrc + 14));
                case 15:  MMIO_MOVE32(pDst + 15, 0, *(pSrc + 15));

                default:    /* Muffle compiler */
                    break;
            }
        }

#       endif

        /* Step to next chunk */
        pBitmapData += nDWord;
        w -= nDWord;
        pATI->nAvailableFIFOEntries -= nDWord;
    }

    pATI->EngineIsBusy = TRUE;
}

/*
 * ATIMach64AccelInit --
 *
 * This function fills in structure fields needed for acceleration on Mach64
 * variants.
 */
int
ATIMach64AccelInit
(
    ATIPtr        pATI,
    XAAInfoRecPtr pXAAInfo
)
{
    /* This doesn't seem quite right... */
    if (pATI->XModifier == 1)
    {
        pXAAInfo->Flags = PIXMAP_CACHE | OFFSCREEN_PIXMAPS;

#ifndef AVOID_CPIO

        if (!pATI->BankInfo.BankSize)

#endif /* AVOID_CPIO */

        {
            pXAAInfo->Flags |= LINEAR_FRAMEBUFFER;
        }
    }

    /* Sync */
    pXAAInfo->Sync = ATIMach64Sync;

    /* Screen-to-screen copy */
    pXAAInfo->SetupForScreenToScreenCopy = ATIMach64SetupForScreenToScreenCopy;
    pXAAInfo->SubsequentScreenToScreenCopy =
        ATIMach64SubsequentScreenToScreenCopy;

    /* Solid fills */
    pXAAInfo->SetupForSolidFill = ATIMach64SetupForSolidFill;
    pXAAInfo->SubsequentSolidFillRect = ATIMach64SubsequentSolidFillRect;

    /* 8x8 mono pattern fills */
    pXAAInfo->Mono8x8PatternFillFlags =

#if X_BYTE_ORDER != X_LITTLE_ENDIAN

        BIT_ORDER_IN_BYTE_MSBFIRST |

#endif /* X_BYTE_ORDER */

        HARDWARE_PATTERN_PROGRAMMED_BITS | HARDWARE_PATTERN_SCREEN_ORIGIN;
    pXAAInfo->SetupForMono8x8PatternFill = ATIMach64SetupForMono8x8PatternFill;
    pXAAInfo->SubsequentMono8x8PatternFillRect =
        ATIMach64SubsequentMono8x8PatternFillRect;

    /*
     * Use scanline version of colour expansion, not only for the non-ix86
     * case, but also to avoid PCI retries.
     */
    pXAAInfo->ScanlineCPUToScreenColorExpandFillFlags =
        LEFT_EDGE_CLIPPING | LEFT_EDGE_CLIPPING_NEGATIVE_X |
        CPU_TRANSFER_PAD_DWORD | SCANLINE_PAD_DWORD;
    if (pATI->XModifier != 1)
        pXAAInfo->ScanlineCPUToScreenColorExpandFillFlags |= TRIPLE_BITS_24BPP;
    pXAAInfo->NumScanlineColorExpandBuffers = 1;

    /* Align bitmap data on a 64-byte boundary */
    pATI->ExpansionBitmapWidth =        /* DWord size in bits */
        ((pATI->displayWidth * pATI->XModifier) + 31) & ~31U;
    pATI->ExpansionBitmapScanlinePtr[1] =
        (CARD32 *)xnfalloc((pATI->ExpansionBitmapWidth >> 3) + 63);
    pATI->ExpansionBitmapScanlinePtr[0] =
        (pointer)(((unsigned long)pATI->ExpansionBitmapScanlinePtr[1] + 63) &
                  ~63UL);
    pXAAInfo->ScanlineColorExpandBuffers =
        (CARD8 **)pATI->ExpansionBitmapScanlinePtr;
    pXAAInfo->SetupForScanlineCPUToScreenColorExpandFill =
        ATIMach64SetupForScanlineCPUToScreenColorExpandFill;
    pXAAInfo->SubsequentScanlineCPUToScreenColorExpandFill =
        ATIMach64SubsequentScanlineCPUToScreenColorExpandFill;
    pXAAInfo->SubsequentColorExpandScanline =
        ATIMach64SubsequentColorExpandScanline;

    /* The engine does not support the following primitives for 24bpp */
    if (pATI->XModifier != 1)
        return ATIMach64MaxY;

    /* Solid lines */
    pXAAInfo->SetupForSolidLine = ATIMach64SetupForSolidLine;
    pXAAInfo->SubsequentSolidHorVertLine = ATIMach64SubsequentSolidHorVertLine;
    pXAAInfo->SubsequentSolidBresenhamLine =
        ATIMach64SubsequentSolidBresenhamLine;

    return ATIMach64MaxY;
}
