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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_accel.c,v 1.11 2004/01/29 03:13:24 dawes Exp $ */

/*************************************************************************
 *
 *  File:       via_accel.c
 *  Content:    2D acceleration function for VIA/S3G UniChrom
 *
 ************************************************************************/

#include "Xarch.h"
#include "xaalocal.h"
#include "xaarop.h"
#include "miline.h"

#include "via_driver.h"
#include "via_regs.h"

/* Forward declaration of functions used in the driver */

static void VIASetupForScreenToScreenCopy(
    ScrnInfoPtr pScrn,
    int xdir,
    int ydir,
    int rop,
    unsigned planemask,
    int trans_color);

static void VIASubsequentScreenToScreenCopy(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2,
    int w,
    int h);

static void VIASetupForSolidFill(
    ScrnInfoPtr pScrn,
    int color,
    int rop,
    unsigned planemask);

static void VIASubsequentSolidFillRect(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h);

static void VIASetupForMono8x8PatternFill(
    ScrnInfoPtr pScrn,
    int pattern0,
    int pattern1,
    int fg,
    int bg,
    int rop,
    unsigned planemask);

static void VIASubsequentMono8x8PatternFillRect(
    ScrnInfoPtr pScrn,
    int patOffx,
    int patOffy,
    int x,
    int y,
    int w,
    int h);

static void VIASetupForColor8x8PatternFill(
    ScrnInfoPtr pScrn,
    int patternx,
    int patterny,
    int rop,
    unsigned planemask,
    int trans_color);

static void VIASubsequentColor8x8PatternFillRect(
    ScrnInfoPtr pScrn,
    int patOffx,
    int patOffy,
    int x,
    int y,
    int w,
    int h);

static void VIASetupForCPUToScreenColorExpandFill(
    ScrnInfoPtr pScrn,
    int fg,
    int bg,
    int rop,
    unsigned planemask);

static void VIASubsequentScanlineCPUToScreenColorExpandFill(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int skipleft);

static void VIASetupForScreenToScreenColorExpand(
    ScrnInfoPtr pScrn,
    int bg,
    int fg,
    int rop,
    unsigned planemask);

static void VIASubsequentScreenToScreenColorExpand(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int srcx,
    int srcy,
    int offset);

static void VIASetupForImageWrite(
    ScrnInfoPtr pScrn,
    int rop,
    unsigned planemask,
    int trans_color,
    int bpp,
    int depth);

static void VIASubsequentImageWriteRect(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int skipleft);

static void
VIASetupForSolidLine(
    ScrnInfoPtr pScrn,
    int color,
    int rop,
    unsigned int planemask);

static void
VIASubsequentSolidTwoPointLine(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2,
    int flags);

static void
VIASubsequentSolidHorVertLine(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int len,
    int dir);

static void
VIASetupForDashedLine(
    ScrnInfoPtr pScrn,
    int fg,
    int bg,
    int rop,
    unsigned int planemask,
    int length,
    unsigned char *pattern);

static void
VIASubsequentDashedTwoPointLine(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2,
    int flags,
    int phase);

static void VIASetClippingRectangle(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2);

static void VIADisableClipping( ScrnInfoPtr );


void
VIAInitialize2DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    CARD32  dwVQStartAddr, dwVQEndAddr;
    CARD32  dwVQLen, dwVQStartL, dwVQEndL, dwVQStartEndH;
    CARD32  dwGEMode;

    /* init 2D engine regs to reset 2D engine */
    VIASETREG(0x04, 0x0);
    VIASETREG(0x08, 0x0);
    VIASETREG(0x0c, 0x0);
    VIASETREG(0x10, 0x0);
    VIASETREG(0x14, 0x0);
    VIASETREG(0x18, 0x0);
    VIASETREG(0x1c, 0x0);
    VIASETREG(0x20, 0x0);
    VIASETREG(0x24, 0x0);
    VIASETREG(0x28, 0x0);
    VIASETREG(0x2c, 0x0);
    VIASETREG(0x30, 0x0);
    VIASETREG(0x34, 0x0);
    VIASETREG(0x38, 0x0);
    VIASETREG(0x3c, 0x0);
    VIASETREG(0x40, 0x0);

    /* Init AGP and VQ regs */
    VIASETREG(0x43c, 0x00100000);
    VIASETREG(0x440, 0x00000000);
    VIASETREG(0x440, 0x00333004);
    VIASETREG(0x440, 0x60000000);
    VIASETREG(0x440, 0x61000000);
    VIASETREG(0x440, 0x62000000);
    VIASETREG(0x440, 0x63000000);
    VIASETREG(0x440, 0x64000000);
    VIASETREG(0x440, 0x7D000000);

    VIASETREG(0x43c, 0xfe020000);
    VIASETREG(0x440, 0x00000000);

    if (pVia->VQStart != 0) {
        /* Enable VQ */
        dwVQStartAddr = pVia->VQStart;
        dwVQEndAddr = pVia->VQEnd;
        dwVQStartL = 0x50000000 | (dwVQStartAddr & 0xFFFFFF);
        dwVQEndL = 0x51000000 | (dwVQEndAddr & 0xFFFFFF);
        dwVQStartEndH = 0x52000000 | ((dwVQStartAddr & 0xFF000000) >> 24) |
                        ((dwVQEndAddr & 0xFF000000) >> 16);
        dwVQLen = 0x53000000 | (VIA_VQ_SIZE >> 3);

        VIASETREG(0x43c, 0x00fe0000);
        VIASETREG(0x440, 0x080003fe);
        VIASETREG(0x440, 0x0a00027c);
        VIASETREG(0x440, 0x0b000260);
        VIASETREG(0x440, 0x0c000274);
        VIASETREG(0x440, 0x0d000264);
        VIASETREG(0x440, 0x0e000000);
        VIASETREG(0x440, 0x0f000020);
        VIASETREG(0x440, 0x1000027e);
        VIASETREG(0x440, 0x110002fe);
        VIASETREG(0x440, 0x200f0060);

        VIASETREG(0x440, 0x00000006);
        VIASETREG(0x440, 0x40008c0f);
        VIASETREG(0x440, 0x44000000);
        VIASETREG(0x440, 0x45080c04);
        VIASETREG(0x440, 0x46800408);

        VIASETREG(0x440, dwVQStartEndH);
        VIASETREG(0x440, dwVQStartL);
        VIASETREG(0x440, dwVQEndL);
        VIASETREG(0x440, dwVQLen);
    }
    else {
        /* Diable VQ */
        VIASETREG(0x43c, 0x00fe0000);
        VIASETREG(0x440, 0x00000004);
        VIASETREG(0x440, 0x40008c0f);
        VIASETREG(0x440, 0x44000000);
        VIASETREG(0x440, 0x45080c04);
        VIASETREG(0x440, 0x46800408);
    }

    dwGEMode = 0;

    switch (pScrn->bitsPerPixel) {
    case 16:
        dwGEMode |= VIA_GEM_16bpp;
        break;
    case 32:
        dwGEMode |= VIA_GEM_32bpp;
    default:
        dwGEMode |= VIA_GEM_8bpp;
        break;
    }

#if 0
    switch (pScrn->displayWidth) {
    case 800:
        dwGEMode |= VIA_GEM_800;
        break;
    case 1024:
        dwGEMode |= VIA_GEM_1024;
        break;
    case 1280:
        dwGEMode |= VIA_GEM_1280;
        break;
    case 1600:
        dwGEMode |= VIA_GEM_1600;
        break;
    case 2048:
        dwGEMode |= VIA_GEM_2048;
        break;
    default:
        dwGEMode |= VIA_GEM_640;
        break;
    }
#endif

    /* Set BPP and Pitch */
    VIASETREG(VIA_REG_GEMODE, dwGEMode);

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));
}



/* Acceleration init function, sets up pointers to our accelerated functions */
Bool
VIAInitAccel(ScreenPtr pScreen)
{
    ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
    VIAPtr          pVia = VIAPTR(pScrn);
    XAAInfoRecPtr   xaaptr;
    BoxRec 	    AvailFBArea;
    unsigned long   cacheEnd;
    unsigned long   cacheEndDRI;

    pVia->VQStart = 0;
    if (((pVia->FBFreeEnd - pVia->FBFreeStart) >= VIA_VQ_SIZE) &&
        pVia->VQEnable) {
        /* Reserved space for VQ */
        pVia->VQStart = pVia->FBFreeEnd - VIA_VQ_SIZE;
        pVia->VQEnd = pVia->VQStart + VIA_VQ_SIZE - 1;
        pVia->FBFreeEnd -= VIA_VQ_SIZE;
    }
    if (pVia->hwcursor) {
		pVia->FBFreeEnd -= VIA_CURSOR_SIZE;
	}

    VIAInitialize2DEngine(pScrn);

    if (pScrn->depth == 8) {
        pVia->PlaneMask = 0xFF;
    }
    else if (pScrn->depth == 15) {
        pVia->PlaneMask = 0x7FFF;
    }
    else if (pScrn->depth == 16) {
        pVia->PlaneMask = 0xFFFF;
    }
    else if (pScrn->depth == 24) {
        pVia->PlaneMask = 0xFFFFFF;
    }

    /* General acceleration flags */
    if (!(xaaptr = pVia->AccelInfoRec = XAACreateInfoRec()))
        return FALSE;

    xaaptr->Flags = PIXMAP_CACHE |
                    OFFSCREEN_PIXMAPS |
                    LINEAR_FRAMEBUFFER |
                    MICROSOFT_ZERO_LINE_BIAS |
                    0;

    if (pScrn->bitsPerPixel == 8)
        xaaptr->CachePixelGranularity = 128;

    /* Clipping */
    xaaptr->SetClippingRectangle = VIASetClippingRectangle;
    xaaptr->DisableClipping = VIADisableClipping;
    xaaptr->ClippingFlags = HARDWARE_CLIP_SOLID_FILL |
                            HARDWARE_CLIP_SOLID_LINE |
                            HARDWARE_CLIP_DASHED_LINE |
                            HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
                            HARDWARE_CLIP_MONO_8x8_FILL |
                            HARDWARE_CLIP_COLOR_8x8_FILL |
                            HARDWARE_CLIP_SCREEN_TO_SCREEN_COLOR_EXPAND |
                            0;

    xaaptr->Sync = VIAAccelSync;

    /* ScreenToScreen copies */
    xaaptr->SetupForScreenToScreenCopy = VIASetupForScreenToScreenCopy;
    xaaptr->SubsequentScreenToScreenCopy = VIASubsequentScreenToScreenCopy;
    xaaptr->ScreenToScreenCopyFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    /* Solid filled rectangles */
    xaaptr->SetupForSolidFill = VIASetupForSolidFill;
    xaaptr->SubsequentSolidFillRect = VIASubsequentSolidFillRect;
    xaaptr->SolidFillFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    /* Mono 8x8 pattern fills */
    xaaptr->SetupForMono8x8PatternFill = VIASetupForMono8x8PatternFill;
    xaaptr->SubsequentMono8x8PatternFillRect =
            VIASubsequentMono8x8PatternFillRect;
    xaaptr->Mono8x8PatternFillFlags = NO_PLANEMASK |
                                      HARDWARE_PATTERN_PROGRAMMED_BITS |
				      ROP_NEEDS_SOURCE |
                                      BIT_ORDER_IN_BYTE_MSBFIRST |
                                      0;

    /* Color 8x8 pattern fills */
    xaaptr->SetupForColor8x8PatternFill = VIASetupForColor8x8PatternFill;
    xaaptr->SubsequentColor8x8PatternFillRect =
            VIASubsequentColor8x8PatternFillRect;
    xaaptr->Color8x8PatternFillFlags = NO_PLANEMASK |
                                       NO_TRANSPARENCY |
                                       HARDWARE_PATTERN_PROGRAMMED_BITS |
				       ROP_NEEDS_SOURCE |
                                       0;

    /*=* This function is only used in drawing check box when use RedHat 7.2
     * Raleigh Theme. The behavior is a little strange, so we temporarily
     * disable this function. *=*/
    /* Screen to Screen color expansion. */
    xaaptr->SetupForScreenToScreenColorExpandFill =
            VIASetupForScreenToScreenColorExpand;
    xaaptr->SubsequentScreenToScreenColorExpandFill =
            VIASubsequentScreenToScreenColorExpand;
    xaaptr->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK |
                                                 BIT_ORDER_IN_BYTE_MSBFIRST |
						 ROP_NEEDS_SOURCE |
						 0;

    /* Solid lines */
    xaaptr->SetupForSolidLine = VIASetupForSolidLine;
    xaaptr->SubsequentSolidTwoPointLine = VIASubsequentSolidTwoPointLine;
    xaaptr->SubsequentSolidHorVertLine = VIASubsequentSolidHorVertLine;
    xaaptr->SolidBresenhamLineErrorTermBits = 14;
    xaaptr->SolidLineFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    /* dashed line */
    xaaptr->SetupForDashedLine = VIASetupForDashedLine;
    xaaptr->SubsequentDashedTwoPointLine = VIASubsequentDashedTwoPointLine;
    xaaptr->DashPatternMaxLength = 8;
    xaaptr->DashedLineFlags = NO_PLANEMASK |
			      ROP_NEEDS_SOURCE |
                              LINE_PATTERN_POWER_OF_2_ONLY |
                              LINE_PATTERN_MSBFIRST_LSBJUSTIFIED |
                              0;

    /* CPU to Screen color expansion */
    xaaptr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
                                                      CPU_TRANSFER_PAD_DWORD |
                                                      SCANLINE_PAD_DWORD |
                                                      BIT_ORDER_IN_BYTE_MSBFIRST |
                                                      LEFT_EDGE_CLIPPING |
						      ROP_NEEDS_SOURCE |
                                                      0;

    xaaptr->SetupForScanlineCPUToScreenColorExpandFill =
            VIASetupForCPUToScreenColorExpandFill;
    xaaptr->SubsequentScanlineCPUToScreenColorExpandFill =
            VIASubsequentScanlineCPUToScreenColorExpandFill;
    xaaptr->ColorExpandBase = pVia->BltBase;
    xaaptr->ColorExpandRange = VIA_MMIO_BLTSIZE;

    /* ImageWrite */
    xaaptr->ImageWriteFlags = NO_PLANEMASK |
                              CPU_TRANSFER_PAD_DWORD |
                              SCANLINE_PAD_DWORD |
                              BIT_ORDER_IN_BYTE_MSBFIRST |
                              LEFT_EDGE_CLIPPING |
			      ROP_NEEDS_SOURCE |
                              /*NO_GXCOPY |*/
                              /*SYNC_AFTER_IMAGE_WRITE |*/
                              0;

    xaaptr->SetupForImageWrite = VIASetupForImageWrite;
    xaaptr->SubsequentImageWriteRect = VIASubsequentImageWriteRect;
    xaaptr->ImageWriteBase = pVia->BltBase;
    xaaptr->ImageWriteRange = VIA_MMIO_BLTSIZE;

    /* We reserve space for pixel cache */
    
    cacheEnd = pVia->FBFreeEnd / pVia->Bpl;
    cacheEndDRI = (pVia->FBFreeStart + VIA_PIXMAP_CACHE_SIZE + pVia->Bpl-1) / pVia->Bpl;
    
    /*
     *	Old DRI has some assumptions here that we need to work through
     *  and fix
     *  UPDATE: These assumptions are that pVia->FBFreestart should
     *  point to a free region in the framebuffer where it can place it's
     *  allocator.
     */
    if(cacheEnd > cacheEndDRI)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
        	"Clipping pixmap cache for old DRI/DRM module.\n");
        cacheEnd = cacheEndDRI;
    }
    
    /*
     *	Clip to the blitter limit
     */
    pVia->ScissB = cacheEnd;;

    if (pVia->ScissB > 2047)
        pVia->ScissB = 2047;

    /*
     * The free start IS where the cache Ends. We should not add here.
     */ 

    pVia->FBFreeStart = (pVia->ScissB + 1) *pVia->Bpl;

    /*
     * Finally, we set up the video memory space available to the pixmap
     * cache
     */

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = pVia->ScissB;

    /*
     * The pixmap cache must stay within the lowest 2048 lines due
     * to hardware blitting limits. The rest is available for offscreen
     * allocations unless DRI stole it.
     */
     
    xf86InitFBManager(pScreen, &AvailFBArea);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
					 "Using %d lines for offscreen memory.\n",
					 AvailFBArea.y2 - pScrn->virtualY ));

    return XAAInit(pScreen, xaaptr);
}

void VIAInitLinear(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
#ifdef XFREE86_44	
    /*
     * In the 44 path we must take care not to truncate offset and size so
     * that we get overlaps. If there is available memory below line 2048
     * we use it.
     */ 
    unsigned long offset = (pVia->FBFreeStart + pVia->Bpp - 1 ) / pVia->Bpp; 
    unsigned long size = pVia->FBFreeEnd / pVia->Bpp - offset;
    if (size > 0) xf86InitFBManagerLinear(pScreen, offset, size);
#else
    /*
     * In the 43 path we don't have to care about truncation. just use
     * all available memory, also below line 2048. The drm module uses 
     * pVia->FBFreeStart as offscreen available start. We do it to. 
     */
    unsigned long offset = pVia->FBFreeStart; 
    unsigned long size = pVia->FBFreeEnd - offset;
    if (size > 0 ) VIAInitPool(pVia, offset, size);
#endif	
}
    


/* The sync function for the GE */
void VIAAccelSync(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    WaitIdle();
}


/* These are the ScreenToScreen bitblt functions. We support all ROPs, all
 * directions.
 *
 */

static void
VIASetupForScreenToScreenCopy(
    ScrnInfoPtr pScrn,
    int xdir,
    int ydir,
    int rop,
    unsigned planemask,
    int trans_color)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    cmd = VIA_GEC_BLT | (XAACopyROP[rop] << 24);

    if (xdir < 0)
        cmd |= VIA_GEC_DECX;

    if (ydir < 0)
        cmd |= VIA_GEC_DECY;

    pVia->SavedCmd = cmd;

    if (trans_color != -1) {
        /* Transparent Bitblt */
        VIASETREG(VIA_REG_SRCCOLORKEY, trans_color);
        VIASETREG(VIA_REG_KEYCONTROL, 0x4000);
    }
    else {
        /* Disable Transparent Bitblt */
        VIASETREG(VIA_REG_KEYCONTROL, 0x0);
    }
}


static void
VIASubsequentScreenToScreenCopy(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2,
    int w,
    int h)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if (!w || !h)
        return;

    if (pVia->SavedCmd & VIA_GEC_DECX) {
        x1 += (w - 1);
        x2 += (w - 1);
    }

    if (pVia->SavedCmd & VIA_GEC_DECY) {
        y1 += (h - 1);
        y2 += (h - 1);
    }

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_SRCPOS, ((y1 << 16) | x1));
    VIASETREG(VIA_REG_DSTPOS, ((y2 << 16) | x2));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);
}


/*
 * SetupForSolidFill is also called to set up for lines.
 */

static void
VIASetupForSolidFill(
    ScrnInfoPtr pScrn,
    int color,
    int rop,
    unsigned planemask)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | (XAAPatternROP[rop] << 24);

    pVia->SavedCmd = cmd;
    pVia->SavedFgColor = color;
}


static void
VIASubsequentSolidFillRect(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if (!w || !h)
        return;

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_FGCOLOR, pVia->SavedFgColor);
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);
}


/*
 * The meaning of the two pattern paremeters to Setup & Subsequent for
 * Mono8x8Patterns varies depending on the flag bits.  We specify
 * HW_PROGRAMMED_BITS, which means our hardware can handle 8x8 patterns
 * without caching in the frame buffer.  Thus, Setup gets the pattern bits.
 * There is no way with BCI to rotate an 8x8 pattern, so we do NOT specify
 * HW_PROGRAMMED_ORIGIN.  XAA wil rotate it for us and pass the rotated
 * pattern to both Setup and Subsequent.  If we DID specify PROGRAMMED_ORIGIN,
 * then Setup would get the unrotated pattern, and Subsequent gets the
 * origin values.
 */

static void
VIASetupForMono8x8PatternFill(
    ScrnInfoPtr pScrn,
    int pattern0,
    int pattern1,
    int fg,
    int bg,
    int rop,
    unsigned planemask)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    cmd = VIA_GEC_BLT | VIA_GEC_PAT_REG | VIA_GEC_PAT_MONO |
          (XAAPatternROP[rop] << 24);

    if (bg == -1) {
        /* transparent mono pattern */
        cmd |= VIA_GEC_MPAT_TRANS;
    }

    pVia->SavedCmd = cmd;
    pVia->SavedFgColor = fg;
    pVia->SavedBgColor = bg;
    pVia->SavedPattern0 = pattern0;
    pVia->SavedPattern1 = pattern1;
}


static void
VIASubsequentMono8x8PatternFillRect(
    ScrnInfoPtr pScrn,
    int patOffx,
    int patOffy,
    int x,
    int y,
    int w,
    int h)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    CARD32  dwPatOffset;

    if (!w || !h)
        return;

    dwPatOffset = ((patOffy & 0x7)  << 29) | ((patOffx & 0x7) << 26);

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_PATADDR, dwPatOffset);
    VIASETREG(VIA_REG_FGCOLOR, pVia->SavedFgColor);
    VIASETREG(VIA_REG_BGCOLOR, pVia->SavedBgColor);
    VIASETREG(VIA_REG_MONOPAT0, pVia->SavedPattern0);
    VIASETREG(VIA_REG_MONOPAT1, pVia->SavedPattern1);
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);
}

static void
VIASetupForColor8x8PatternFill(
    ScrnInfoPtr pScrn,
    int patternx,
    int patterny,
    int rop,
    unsigned planemask,
    int trans_color)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    cmd = VIA_GEC_BLT | (XAAPatternROP[rop] << 24);

    pVia->SavedCmd = cmd;
    pVia->SavedPatternAddr = (patternx * pVia->Bpp + patterny * pVia->Bpl);
}


static void
VIASubsequentColor8x8PatternFillRect(
    ScrnInfoPtr pScrn,
    int patOffx,
    int patOffy,
    int x,
    int y,
    int w,
    int h)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    CARD32  dwPatAddr;

    if (!w || !h)
        return;

    dwPatAddr = (pVia->SavedPatternAddr >> 3) |
                ((patOffy & 0x7)  << 29) | ((patOffx & 0x7) << 26);

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_PATADDR, dwPatAddr);
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);
}

static void
VIASetupForCPUToScreenColorExpandFill(
    ScrnInfoPtr pScrn,
    int fg,
    int bg,
    int rop,
    unsigned planemask)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    cmd = VIA_GEC_BLT | VIA_GEC_SRC_SYS | VIA_GEC_SRC_MONO |
          (XAACopyROP[rop] << 24);

    if (bg == -1) {
        cmd |= VIA_GEC_MSRC_TRANS;
    }

    pVia->SavedCmd = cmd;
    pVia->SavedFgColor = fg;
    pVia->SavedBgColor = bg;

    /* Disable Transparent Bitblt */
    VIASETREG(VIA_REG_KEYCONTROL, 0x0);
}


static void
VIASubsequentScanlineCPUToScreenColorExpandFill(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int skipleft)
{
    VIAPtr pVia = VIAPTR(pScrn);

    /* XAA will be sending bitmap data next.  */
    /* We should probably wait for empty/idle here. */

    if (skipleft) {
        VIASetClippingRectangle(pScrn, (x + skipleft), y, (x + w - 1), (y + h -1));
    }

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_SRCPOS, 0);
    VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_FGCOLOR, pVia->SavedFgColor);
    VIASETREG(VIA_REG_BGCOLOR, pVia->SavedBgColor);
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);
}


static void
VIASetupForScreenToScreenColorExpand(
    ScrnInfoPtr pScrn,
    int fg,
    int bg,
    int rop,
    unsigned int planemask)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    cmd = VIA_GEC_BLT | VIA_GEC_SRC_MONO | (XAACopyROP[rop] << 24);

    if (bg == -1) {
        cmd |= VIA_GEC_MSRC_TRANS;
    }

    pVia->SavedCmd = cmd;
    pVia->SavedFgColor = fg;
    pVia->SavedBgColor = bg;

    /* Disable Transparent Bitblt */
    VIASETREG(VIA_REG_KEYCONTROL, 0x0);
}


static void
VIASubsequentScreenToScreenColorExpand(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int srcx,
    int srcy,
    int offset)
{
    VIAPtr pVia = VIAPTR(pScrn);

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_SRCBASE, ((((srcy * pScrn->displayWidth) + srcx) *
                            (pScrn->bitsPerPixel >> 3)) >> 3));
    srcy = 0;
    srcx = 0;

    VIASETREG(VIA_REG_SRCPOS, ((srcy << 16) |
              (srcx * pScrn->bitsPerPixel + offset)));
    VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_FGCOLOR, pVia->SavedFgColor);
    VIASETREG(VIA_REG_BGCOLOR, pVia->SavedBgColor);
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);

    VIASETREG(VIA_REG_SRCBASE, 0x0);
}

static void
VIASetupForImageWrite(
    ScrnInfoPtr pScrn,
    int rop,
    unsigned planemask,
    int trans_color,
    int bpp,
    int depth)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    /* We don't record bpp and depth because we assume bpp is equal to
       bpp of screen. Is this assume correct ? */

    cmd = VIA_GEC_BLT | VIA_GEC_SRC_SYS | (XAACopyROP[rop] << 24);

    pVia->SavedCmd = cmd;

    if (trans_color != -1) {
        /* Transparent Bitblt */
        VIASETREG(VIA_REG_SRCCOLORKEY, trans_color);
        VIASETREG(VIA_REG_KEYCONTROL, 0x4000);
    }
    else {
        /* Disable Transparent Bitblt */
        VIASETREG(VIA_REG_KEYCONTROL, 0x0);
    }

}


static void
VIASubsequentImageWriteRect(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int skipleft)
{
    VIAPtr  pVia = VIAPTR(pScrn);

   if (skipleft) {
        VIASetClippingRectangle(pScrn, (x + skipleft), y, (x + w - 1), (y + h -1));
    }

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    VIASETREG(VIA_REG_SRCPOS, 0);
    VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
    VIASETREG(VIA_REG_DIMENSION, (((h - 1) << 16) | (w - 1)));
    VIASETREG(VIA_REG_GECMD, pVia->SavedCmd);
}


/* Setup for XAA solid lines. */
static void
VIASetupForSolidLine(
    ScrnInfoPtr pScrn,
    int color,
    int rop,
    unsigned int planemask)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;

    /* we move VIA_GEC_LINE from here to the place firing command */
    cmd = VIA_GEC_FIXCOLOR_PAT | (XAAPatternROP[rop] << 24);

    pVia->SavedCmd = cmd;
    pVia->SavedFgColor = color;

    /* set solid line pattern */
    VIASETREG(VIA_REG_MONOPAT0, 0xFF);
    VIASETREG(VIA_REG_FGCOLOR, pVia->SavedFgColor);
}


static void
VIASubsequentSolidTwoPointLine(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2,
    int flags)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     dx, dy, cmd, tmp, error = 1;

    cmd = pVia->SavedCmd | VIA_GEC_LINE;

    dx = x2 - x1;
    if (dx < 0) {
        dx = -dx;
        cmd |= VIA_GEC_DECX;            /* line will be drawn from right */
        error = 0;
    }

    dy = y2 - y1;
    if (dy < 0) {
        dy = -dy;
        cmd |= VIA_GEC_DECY;            /* line will be drawn from bottom */
    }

    if (dy > dx) {
        tmp  = dy;
        dy = dx;
        dx = tmp;                       /* Swap 'dx' and 'dy' */
        cmd |= VIA_GEC_Y_MAJOR;         /* Y major line */
    }

    if (flags & OMIT_LAST) {
        cmd |= VIA_GEC_LASTPIXEL_OFF;
    }

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    /* major = 2*dmaj, minor = 2*dmin, err = -dmaj - ((bias >> octant) & 1) */
    /* K1 = 2*dmin K2 = 2*(dmin - dmax) */
    /* Error Term = (StartX<EndX) ? (2*dmin - dmax - 1) : (2*(dmin - dmax)) */
    VIASETREG(VIA_REG_LINE_K1K2, ((((dy << 1) & 0x3fff) << 16)|
              (((dy - dx) << 1) & 0x3fff)));
    VIASETREG(VIA_REG_LINE_XY, ((y1 << 16) | x1));
    VIASETREG(VIA_REG_DIMENSION, dx);
    VIASETREG(VIA_REG_LINE_ERROR, (((dy << 1) - dx - error) & 0x3fff));
    VIASETREG(VIA_REG_GECMD, cmd);

}


/* Subsequent XAA solid horizontal and vertical lines */
static void
VIASubsequentSolidHorVertLine(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int len,
    int dir)
{
    VIAPtr  pVia = VIAPTR(pScrn);

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    if (dir == DEGREES_0) {
        VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
        VIASETREG(VIA_REG_DIMENSION, (len - 1));
        VIASETREG(VIA_REG_GECMD, pVia->SavedCmd | VIA_GEC_BLT);
    }
    else {
        VIASETREG(VIA_REG_DSTPOS, ((y << 16) | x));
        VIASETREG(VIA_REG_DIMENSION, ((len - 1) << 16));
        VIASETREG(VIA_REG_GECMD, pVia->SavedCmd | VIA_GEC_BLT);
    }
}

static void
VIASetupForDashedLine(
    ScrnInfoPtr pScrn,
    int fg,
    int bg,
    int rop,
    unsigned int planemask,
    int length,
    unsigned char *pattern)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     cmd;
    CARD32  pat = *(CARD32 *)pattern;

    cmd = VIA_GEC_LINE | VIA_GEC_FIXCOLOR_PAT | (XAAPatternROP[rop] << 24);

    if (bg == -1) {
        /* transparent mono pattern */
        cmd |= VIA_GEC_MPAT_TRANS;
    }

    pVia->SavedCmd = cmd;
    pVia->SavedFgColor = fg;
    pVia->SavedBgColor = bg;

    switch (length) {
        case  2: pat |= pat <<  2; /* fall through */
        case  4: pat |= pat <<  4; /* fall through */
        case  8: pat |= pat <<  8; /* fall through */
        case 16: pat |= pat << 16;
    }

    pVia->SavedPattern0 = pat;

    VIASETREG(VIA_REG_FGCOLOR, pVia->SavedFgColor);
    VIASETREG(VIA_REG_BGCOLOR, pVia->SavedBgColor);
    VIASETREG(VIA_REG_MONOPAT0, pVia->SavedPattern0);
}


static void
VIASubsequentDashedTwoPointLine(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2,
    int flags,
    int phase)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int     dx, dy, cmd, tmp, error = 1;

    cmd = pVia->SavedCmd;

    dx = x2 - x1;
    if (dx < 0) {
        dx = -dx;
        cmd |= VIA_GEC_DECX;            /* line will be drawn from right */
        error = 0;
    }

    dy = y2 - y1;
    if (dy < 0) {
        dy = -dy;
        cmd |= VIA_GEC_DECY;            /* line will be drawn from bottom */
    }

    if (dy > dx) {
        tmp  = dy;
        dy = dx;
        dx = tmp;                       /* Swap 'dx' and 'dy' */
        cmd |= VIA_GEC_Y_MAJOR;         /* Y major line */
    }

    if (flags & OMIT_LAST) {
        cmd |= VIA_GEC_LASTPIXEL_OFF;
    }

    /* Set Src and Dst base address and pitch, pitch is qword */
    VIASETREG(VIA_REG_SRCBASE, 0x0);
    VIASETREG(VIA_REG_DSTBASE, 0x0);
    VIASETREG(VIA_REG_PITCH, VIA_PITCH_ENABLE |
              ((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) |
              (((pScrn->displayWidth * pScrn->bitsPerPixel >> 3) >> 3) << 16));

    /* major = 2*dmaj, minor = 2*dmin, err = -dmaj - ((bias >> octant) & 1) */
    /* K1 = 2*dmin K2 = 2*(dmin - dmax) */
    /* Error Term = (StartX<EndX) ? (2*dmin - dmax - 1) : (2*(dmin - dmax)) */
    VIASETREG(VIA_REG_LINE_K1K2, ((((dy << 1) & 0x3fff) << 16)|
              (((dy - dx) << 1) & 0x3fff)));
    VIASETREG(VIA_REG_LINE_XY, ((y1 << 16) | x1));
    VIASETREG(VIA_REG_DIMENSION, dx);
    VIASETREG(VIA_REG_LINE_ERROR, (((dy << 1) - dx - error) & 0x3fff) | 0xFF0000);
    VIASETREG(VIA_REG_GECMD, cmd);
}


static void
VIASetClippingRectangle(
    ScrnInfoPtr pScrn,
    int x1,
    int y1,
    int x2,
    int y2)
{
    VIAPtr pVia = VIAPTR(pScrn);

#ifdef DEBUG_EXTRA
    ErrorF("ClipRect, (%4d,%4d)-(%4d,%4d) \n", x1, y1, x2, y2 );
#endif

    VIASETREG(VIA_REG_CLIPTL, ((y1 << 16) | x1));
    VIASETREG(VIA_REG_CLIPBR, ((y2 << 16) | x2));
    pVia->SavedCmd |= VIA_GEC_CLIP_ENABLE;
}


static void VIADisableClipping(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

#ifdef DEBUG_EXTRA
    ErrorF("Kill ClipRect\n");
#endif

    pVia->SavedCmd &= ~VIA_GEC_CLIP_ENABLE;
}
