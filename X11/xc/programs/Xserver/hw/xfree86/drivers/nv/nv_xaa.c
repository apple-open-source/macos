/* $XConsortium: nv_driver.c /main/3 1996/10/28 05:13:37 kaleb $ */
 /***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/

/* Hacked together from mga driver and 3.3.4 NVIDIA driver by
   Jarno Paananen <jpaana@s2.org> */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_xaa.c,v 1.29 2003/02/12 21:26:27 mvojkovi Exp $ */

#include "nv_include.h"
#include "xaalocal.h"
#include "xaarop.h"

#include "miline.h"

static void
NVSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    int height = y2-y1 + 1;
    int width  = x2-x1 + 1;
    NVPtr pNv = NVPTR(pScrn);

    RIVA_FIFO_FREE(pNv->riva, Clip, 2);
    pNv->riva.Clip->TopLeft     = (y1     << 16) | (x1 & 0xffff);
    pNv->riva.Clip->WidthHeight = (height << 16) | width;
}


static void
NVDisableClipping(ScrnInfoPtr pScrn)
{
    NVSetClippingRectangle(pScrn, 0, 0, 0x7fff, 0x7fff);
}

/*
 * Set pattern. Internal routine. The upper bits of the colors
 * are the ALPHA bits.  0 == transparency.
 */
static void
NVSetPattern(NVPtr pNv, int clr0, int clr1, int pat0, int pat1)
{
    RIVA_FIFO_FREE(pNv->riva, Patt, 5);
    pNv->riva.Patt->Shape         = 0; /* 0 = 8X8, 1 = 64X1, 2 = 1X64 */
    pNv->riva.Patt->Color0        = clr0;
    pNv->riva.Patt->Color1        = clr1;
    pNv->riva.Patt->Monochrome[0] = pat0;
    pNv->riva.Patt->Monochrome[1] = pat1;
}

/*
 * Set ROP.  Translate X rop into ROP3.  Internal routine.
 */
static void
NVSetRopSolid(NVPtr pNv, int rop)
{    
    if (pNv->currentRop != rop)
    {
        if (pNv->currentRop > 16)
            NVSetPattern(pNv, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
        pNv->currentRop = rop;
        RIVA_FIFO_FREE(pNv->riva, Rop, 1);
        pNv->riva.Rop->Rop3 = XAACopyROP[rop];
    }
}

static void
NVSetRopPattern(NVPtr pNv, int rop)
{
    if (pNv->currentRop != rop + 16)
    {
        pNv->currentRop = rop + 16; /* +16 is important */
        RIVA_FIFO_FREE(pNv->riva, Rop, 1);
        pNv->riva.Rop->Rop3 = XAAPatternROP[rop];
    }
}

/*
 * Fill solid rectangles.
 */
static
void NVSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
                         unsigned planemask)
{
    NVPtr pNv = NVPTR(pScrn);

    NVSetRopSolid(pNv, rop);
    RIVA_FIFO_FREE(pNv->riva, Bitmap, 1);
    pNv->riva.Bitmap->Color1A = color;
}

static void
NVSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    NVPtr pNv = NVPTR(pScrn);
    
    RIVA_FIFO_FREE(pNv->riva, Bitmap, 2);
    pNv->riva.Bitmap->UnclippedRectangle[0].TopLeft     = (x << 16) | y; 
    write_mem_barrier();
    pNv->riva.Bitmap->UnclippedRectangle[0].WidthHeight = (w << 16) | h;
    write_mem_barrier();
}

/*
 * Screen to screen BLTs.
 */
static void
NVSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
                             unsigned planemask, int transparency_color)
{
    NVSetRopSolid(NVPTR(pScrn), rop);
}

static void
NVSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
                               int x2, int y2, int w, int h)
{
    NVPtr pNv = NVPTR(pScrn);

    RIVA_FIFO_FREE(pNv->riva, Blt, 3);
    pNv->riva.Blt->TopLeftSrc  = (y1 << 16) | x1;
    pNv->riva.Blt->TopLeftDst  = (y2 << 16) | x2;
    write_mem_barrier();
    pNv->riva.Blt->WidthHeight = (h  << 16) | w;
    write_mem_barrier();
}


/*
 * Fill 8x8 monochrome pattern rectangles.  patternx and patterny are
 * the overloaded pattern bits themselves. The pattern colors don't
 * support 565, only 555. Hack around it.
 */
static void
NVSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patternx, int patterny,
                             int fg, int bg, int rop, unsigned planemask)
{
    NVPtr pNv = NVPTR(pScrn);

    NVSetRopPattern(pNv, rop);
    if (pScrn->depth == 16)
    {
        fg = ((fg & 0x0000F800) << 8)
           | ((fg & 0x000007E0) << 5)
           | ((fg & 0x0000001F) << 3)
           |        0xFF000000;
        if (bg != -1)
            bg = ((bg & 0x0000F800) << 8)
               | ((bg & 0x000007E0) << 5)
               | ((bg & 0x0000001F) << 3)
               |        0xFF000000;
        else
            bg = 0;
    }
    else
    {
	fg |= pNv->opaqueMonochrome;
	bg  = (bg == -1) ? 0 : bg | pNv->opaqueMonochrome;
    };
    NVSetPattern(pNv, bg, fg, patternx, patterny);
    RIVA_FIFO_FREE(pNv->riva, Bitmap, 1);
    pNv->riva.Bitmap->Color1A = fg;
}

static void
NVSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
                                   int patternx, int patterny,
                                   int x, int y, int w, int h)
{
    NVPtr pNv = NVPTR(pScrn);

    RIVA_FIFO_FREE(pNv->riva, Bitmap, 2);
    pNv->riva.Bitmap->UnclippedRectangle[0].TopLeft     = (x << 16) | y;
    write_mem_barrier();
    pNv->riva.Bitmap->UnclippedRectangle[0].WidthHeight = (w << 16) | h;
    write_mem_barrier();
}


void
NVResetGraphics(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);

    if(pNv->NoAccel) return;

    pNv->currentRop = -1;
    NVSetRopPattern(pNv, GXcopy); 
}



/*
 * Synchronise with graphics engine.  Make sure it is idle before returning.
 * Should attempt to yield CPU if busy for awhile.
 */
void NVSync(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    RIVA_BUSY(pNv->riva);
}

/* Color expansion */
static void
NVSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                             int fg, int bg, int rop, 
                                             unsigned int planemask)
{
    NVPtr pNv = NVPTR(pScrn);

    NVSetRopSolid(pNv, rop);

    if ( bg == -1 )
    {
        /* Transparent case */
        bg = 0x80000000;
        pNv->expandFifo = (unsigned char*)&pNv->riva.Bitmap->MonochromeData1C;
    }
    else
    {
        pNv->expandFifo = (unsigned char*)&pNv->riva.Bitmap->MonochromeData01E;
        if (pScrn->depth == 16)
        {
            bg = ((bg & 0x0000F800) << 8)
               | ((bg & 0x000007E0) << 5)
               | ((bg & 0x0000001F) << 3)
               |        0xFF000000;
        }
        else
        {
            bg  |= pNv->opaqueMonochrome;
        };
    }
    pNv->FgColor = fg;
    pNv->BgColor = bg;
}

static void
NVSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    NVPtr pNv = NVPTR(pScrn);

    int t = pNv->expandWidth;
    CARD32 *pbits = (CARD32*)pNv->expandBuffer;
    CARD32 *d = (CARD32*)pNv->expandFifo;
    
    while(t >= 16) 
    {
	RIVA_FIFO_FREE(pNv->riva, Bitmap, 16);
	d[0]  = pbits[0];
	d[1]  = pbits[1];
	d[2]  = pbits[2];
	d[3]  = pbits[3];
	d[4]  = pbits[4];
	d[5]  = pbits[5];
	d[6]  = pbits[6];
	d[7]  = pbits[7];
	d[8]  = pbits[8];
	d[9]  = pbits[9];
	d[10] = pbits[10];
	d[11] = pbits[11];
	d[12] = pbits[12];
	d[13] = pbits[13];
	d[14] = pbits[14];
	d[15] = pbits[15];
	t -= 16; pbits += 16;
    }
    if(t) {
	RIVA_FIFO_FREE(pNv->riva, Bitmap, t);
	while(t >= 4) 
	{
	    d[0]  = pbits[0];
	    d[1]  = pbits[1];
	    d[2]  = pbits[2];
	    d[3]  = pbits[3];
	    t -= 4; pbits += 4;
	}
	while(t--) 
	    *(d++) = *(pbits++); 
    }

    if (!(--pNv->expandRows)) { /* hardware bug workaround */
       RIVA_FIFO_FREE(pNv->riva, Blt, 1);
       write_mem_barrier();
       pNv->riva.Blt->TopLeftSrc = 0;
    }
    write_mem_barrier();
}

static void
NVSubsequentColorExpandScanlineFifo(ScrnInfoPtr pScrn, int bufno)
{
    NVPtr pNv = NVPTR(pScrn);

    if ( --pNv->expandRows ) {
       RIVA_FIFO_FREE(pNv->riva, Bitmap, pNv->expandWidth);
    } else { /* hardware bug workaround */
       RIVA_FIFO_FREE(pNv->riva, Blt, 1);
       write_mem_barrier();
       pNv->riva.Blt->TopLeftSrc = 0;
    }
    write_mem_barrier();
}

static void
NVSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x,
                                               int y, int w, int h,
                                               int skipleft)
{
    int bw;
    NVPtr pNv = NVPTR(pScrn);
    
    bw = (w + 31) & ~31;
    pNv->expandWidth = bw >> 5;

    if ( pNv->BgColor == 0x80000000 )
    {
        /* Use faster transparent method */
        RIVA_FIFO_FREE(pNv->riva, Bitmap, 5);
        pNv->riva.Bitmap->ClipC.TopLeft     = (y << 16) | ((x+skipleft)
                                                           & 0xFFFF);
        pNv->riva.Bitmap->ClipC.BottomRight = ((y+h) << 16) | ((x+w)&0xffff);
        pNv->riva.Bitmap->Color1C           = pNv->FgColor;
        pNv->riva.Bitmap->WidthHeightC      = (h << 16) | bw;
        write_mem_barrier();
        pNv->riva.Bitmap->PointC            = (y << 16) | (x & 0xFFFF);
        write_mem_barrier();
    }
    else
    {
        /* Opaque */
        RIVA_FIFO_FREE(pNv->riva, Bitmap, 7);
        pNv->riva.Bitmap->ClipE.TopLeft     = (y << 16) | ((x+skipleft)
                                                           & 0xFFFF);
        pNv->riva.Bitmap->ClipE.BottomRight = ((y+h) << 16) | ((x+w)&0xffff);
        pNv->riva.Bitmap->Color0E           = pNv->BgColor;
        pNv->riva.Bitmap->Color1E           = pNv->FgColor;
        pNv->riva.Bitmap->WidthHeightInE  = (h << 16) | bw;
        pNv->riva.Bitmap->WidthHeightOutE = (h << 16) | bw;
        write_mem_barrier();
        pNv->riva.Bitmap->PointE          = (y << 16) | (x & 0xFFFF);
        write_mem_barrier();
    }

    pNv->expandRows = h;

    if(pNv->expandWidth > (pNv->riva.FifoEmptyCount >> 2)) {
	pNv->AccelInfoRec->ScanlineColorExpandBuffers = &pNv->expandBuffer;
	pNv->AccelInfoRec->SubsequentColorExpandScanline = 
				NVSubsequentColorExpandScanline;
    } else {
	pNv->AccelInfoRec->ScanlineColorExpandBuffers = &pNv->expandFifo;
	pNv->AccelInfoRec->SubsequentColorExpandScanline = 
				NVSubsequentColorExpandScanlineFifo;
	RIVA_FIFO_FREE(pNv->riva, Bitmap, pNv->expandWidth);
    }
}

static void
NVSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop, unsigned planemask)
{
    NVPtr pNv = NVPTR(pScrn);

    NVSetRopSolid(pNv, rop);
    pNv->FgColor = color;
}

static void 
NVSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len, int dir)
{
    NVPtr pNv = NVPTR(pScrn);

    RIVA_FIFO_FREE(pNv->riva, Line, 3);
    pNv->riva.Line->Color = pNv->FgColor;
    pNv->riva.Line->Lin[0].point0 = ((y << 16) | ( x & 0xffff));
    write_mem_barrier();
    if ( dir ==DEGREES_0 )
        pNv->riva.Line->Lin[0].point1 = ((y << 16) | (( x + len ) & 0xffff));
    else
        pNv->riva.Line->Lin[0].point1 = (((y + len) << 16) | ( x & 0xffff));
    write_mem_barrier();
}

static void 
NVSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1,
                              int x2, int y2, int flags)
{
    NVPtr pNv = NVPTR(pScrn);
    Bool  lastPoint = !(flags & OMIT_LAST);

    RIVA_FIFO_FREE(pNv->riva, Line, lastPoint ? 5 : 3);
    pNv->riva.Line->Color = pNv->FgColor;
    pNv->riva.Line->Lin[0].point0 = ((y1 << 16) | (x1 & 0xffff));
    write_mem_barrier();
    pNv->riva.Line->Lin[0].point1 = ((y2 << 16) | (x2 & 0xffff));
    write_mem_barrier();
    if (lastPoint)
    {
        pNv->riva.Line->Lin[1].point0 = ((y2 << 16) | (x2 & 0xffff));
        write_mem_barrier();
        pNv->riva.Line->Lin[1].point1 = (((y2 + 1) << 16) | (x2 & 0xffff));
        write_mem_barrier();
    }
}

static void
NVValidatePolyArc(
   GCPtr        pGC,
   unsigned long changes,
   DrawablePtr pDraw
){
   if(pGC->planemask != ~0) return;

   if(!pGC->lineWidth && 
	((pGC->alu != GXcopy) || (pGC->lineStyle != LineSolid)))
   {
        pGC->ops->PolyArc = miZeroPolyArc;
   }
}

static void
NVValidatePolyPoint(
   GCPtr        pGC,
   unsigned long changes,
   DrawablePtr pDraw
){
   pGC->ops->PolyPoint = XAAFallbackOps.PolyPoint;

   if(pGC->planemask != ~0) return;

   if(pGC->alu != GXcopy)
        pGC->ops->PolyPoint = miPolyPoint;
}

/* Initialize XAA acceleration info */
Bool
NVAccelInit(ScreenPtr pScreen) 
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    Bool lowClocks;

    /* The hardware POSTs with clocks too low to support some acceleration
       on NV20 and higher and we don't know enough about timing particulars
       to raise them */

    lowClocks = ((pNv->Chipset & 0x0ff0) >= 0x0200);
    
    pNv->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if(!infoPtr) return FALSE;

    /* fill out infoPtr here */
    infoPtr->Flags = LINEAR_FRAMEBUFFER | PIXMAP_CACHE | OFFSCREEN_PIXMAPS;

    /* sync */
    infoPtr->Sync = NVSync;

    /* solid fills */
    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = NVSetupForSolidFill;
    infoPtr->SubsequentSolidFillRect = NVSubsequentSolidFillRect;

    if(lowClocks)
       infoPtr->SolidFillFlags |= GXCOPY_ONLY;

    /* screen to screen copy */
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY | NO_PLANEMASK;
    infoPtr->SetupForScreenToScreenCopy = NVSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = NVSubsequentScreenToScreenCopy;

    /* 8x8 mono patterns */
    /*
     * Set pattern opaque bits based on pixel format.
     */
    pNv->opaqueMonochrome = ~((1 << pScrn->depth) - 1);

    pNv->currentRop = -1;

    infoPtr->Mono8x8PatternFillFlags = HARDWARE_PATTERN_SCREEN_ORIGIN |
				       HARDWARE_PATTERN_PROGRAMMED_BITS |
				       NO_PLANEMASK;
    infoPtr->SetupForMono8x8PatternFill = NVSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect =
        NVSubsequentMono8x8PatternFillRect;

    /* Color expansion */
    infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
#if X_BYTE_ORDER == X_BIG_ENDIAN
				BIT_ORDER_IN_BYTE_MSBFIRST | 
#else
				BIT_ORDER_IN_BYTE_LSBFIRST | 
#endif
				NO_PLANEMASK | 
				CPU_TRANSFER_PAD_DWORD |
				LEFT_EDGE_CLIPPING | 		
				LEFT_EDGE_CLIPPING_NEGATIVE_X;

    infoPtr->NumScanlineColorExpandBuffers = 1;

    if(!lowClocks) {
       infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
           NVSetupForScanlineCPUToScreenColorExpandFill;
       infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
           NVSubsequentScanlineCPUToScreenColorExpandFill;
    }

    pNv->expandFifo = (unsigned char*)&pNv->riva.Bitmap->MonochromeData01E;
    
    /* Allocate buffer for color expansion and also image writes in the
       future */
    pNv->expandBuffer = xnfalloc(((pScrn->virtualX*pScrn->bitsPerPixel)/8) + 8);


    infoPtr->ScanlineColorExpandBuffers = &pNv->expandBuffer;
    infoPtr->SubsequentColorExpandScanline = NVSubsequentColorExpandScanline;

    infoPtr->SolidLineFlags = infoPtr->SolidFillFlags;
    infoPtr->SetupForSolidLine = NVSetupForSolidLine;
    infoPtr->SubsequentSolidHorVertLine =
		NVSubsequentSolidHorVertLine;
    infoPtr->SubsequentSolidTwoPointLine = 
		NVSubsequentSolidTwoPointLine;
    infoPtr->SetClippingRectangle = NVSetClippingRectangle;
    infoPtr->DisableClipping = NVDisableClipping;
    infoPtr->ClippingFlags = HARDWARE_CLIP_SOLID_LINE;
    miSetZeroLineBias(pScreen, OCTANT1 | OCTANT3 | OCTANT4 | OCTANT6);

    infoPtr->ValidatePolyArc = NVValidatePolyArc;
    infoPtr->PolyArcMask = GCFunction | GCLineWidth | GCPlaneMask;
    infoPtr->ValidatePolyPoint = NVValidatePolyPoint;
    infoPtr->PolyPointMask = GCFunction | GCPlaneMask;
   
    NVDisableClipping(pScrn);

    return(XAAInit(pScreen, infoPtr));
}

