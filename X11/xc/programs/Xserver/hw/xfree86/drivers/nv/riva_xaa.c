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

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_xaa.c,v 1.1 2003/07/31 20:24:31 mvojkovi Exp $ */

#include "riva_include.h"
#include "xaalocal.h"
#include "xaarop.h"

#include "miline.h"

static void
RivaSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    int height = y2-y1 + 1;
    int width  = x2-x1 + 1;
    RivaPtr pRiva = RivaPTR(pScrn);

    RIVA_FIFO_FREE(pRiva->riva, Clip, 2);
    pRiva->riva.Clip->TopLeft     = (y1     << 16) | (x1 & 0xffff);
    pRiva->riva.Clip->WidthHeight = (height << 16) | width;
}


static void
RivaDisableClipping(ScrnInfoPtr pScrn)
{
    RivaSetClippingRectangle(pScrn, 0, 0, 0x7fff, 0x7fff);
}

/*
 * Set pattern. Internal routine. The upper bits of the colors
 * are the ALPHA bits.  0 == transparency.
 */
static void
RivaSetPattern(RivaPtr pRiva, int clr0, int clr1, int pat0, int pat1)
{
    RIVA_FIFO_FREE(pRiva->riva, Patt, 4);
    pRiva->riva.Patt->Color0        = clr0;
    pRiva->riva.Patt->Color1        = clr1;
    pRiva->riva.Patt->Monochrome[0] = pat0;
    pRiva->riva.Patt->Monochrome[1] = pat1;
}

/*
 * Set ROP.  Translate X rop into ROP3.  Internal routine.
 */
static void
RivaSetRopSolid(RivaPtr pRiva, int rop)
{    
    if (pRiva->currentRop != rop) {
        if (pRiva->currentRop >= 16)
            RivaSetPattern(pRiva, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
        pRiva->currentRop = rop;
        RIVA_FIFO_FREE(pRiva->riva, Rop, 1);
        pRiva->riva.Rop->Rop3 = XAACopyROP[rop];
    }
}

static void
RivaSetRopPattern(RivaPtr pRiva, int rop)
{
    if (pRiva->currentRop != (rop + 16)) {
        pRiva->currentRop = rop + 16; /* +16 is important */
        RIVA_FIFO_FREE(pRiva->riva, Rop, 1);
        pRiva->riva.Rop->Rop3 = XAAPatternROP[rop];
    }
}

/*
 * Fill solid rectangles.
 */
static
void RivaSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
                         unsigned planemask)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RivaSetRopSolid(pRiva, rop);
    RIVA_FIFO_FREE(pRiva->riva, Bitmap, 1);
    pRiva->riva.Bitmap->Color1A = color;
}

static void
RivaSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    
    RIVA_FIFO_FREE(pRiva->riva, Bitmap, 2);
    pRiva->riva.Bitmap->UnclippedRectangle[0].TopLeft     = (x << 16) | y; 
    write_mem_barrier();
    pRiva->riva.Bitmap->UnclippedRectangle[0].WidthHeight = (w << 16) | h;
    write_mem_barrier();
}

/*
 * Screen to screen BLTs.
 */
static void
RivaSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
                             unsigned planemask, int transparency_color)
{
    RivaSetRopSolid(RivaPTR(pScrn), rop);
}

static void
RivaSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
                               int x2, int y2, int w, int h)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RIVA_FIFO_FREE(pRiva->riva, Blt, 3);
    pRiva->riva.Blt->TopLeftSrc  = (y1 << 16) | x1;
    pRiva->riva.Blt->TopLeftDst  = (y2 << 16) | x2;
    write_mem_barrier();
    pRiva->riva.Blt->WidthHeight = (h  << 16) | w;
    write_mem_barrier();
}


/*
 * Fill 8x8 monochrome pattern rectangles.  patternx and patterny are
 * the overloaded pattern bits themselves. The pattern colors don't
 * support 565, only 555. Hack around it.
 */
static void
RivaSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patternx, int patterny,
                             int fg, int bg, int rop, unsigned planemask)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RivaSetRopPattern(pRiva, rop);
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
	fg |= pRiva->opaqueMonochrome;
	bg  = (bg == -1) ? 0 : bg | pRiva->opaqueMonochrome;
    };
    RivaSetPattern(pRiva, bg, fg, patternx, patterny);
    RIVA_FIFO_FREE(pRiva->riva, Bitmap, 1);
    pRiva->riva.Bitmap->Color1A = fg;
}

static void
RivaSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
                                   int patternx, int patterny,
                                   int x, int y, int w, int h)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RIVA_FIFO_FREE(pRiva->riva, Bitmap, 2);
    pRiva->riva.Bitmap->UnclippedRectangle[0].TopLeft     = (x << 16) | y;
    write_mem_barrier();
    pRiva->riva.Bitmap->UnclippedRectangle[0].WidthHeight = (w << 16) | h;
    write_mem_barrier();
}


void
RivaResetGraphics(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    if(pRiva->NoAccel) return;

    RIVA_FIFO_FREE(pRiva->riva, Patt, 1);
    pRiva->riva.Patt->Shape = 0; 
    RivaDisableClipping(pScrn);
    pRiva->currentRop = 16;  /* to force RivaSetRopSolid to reset the pattern */
    RivaSetRopSolid(pRiva, GXcopy);
}



/*
 * Synchronise with graphics engine.  Make sure it is idle before returning.
 * Should attempt to yield CPU if busy for awhile.
 */
void RivaSync(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    RIVA_BUSY(pRiva->riva);
}

/* Color expansion */
static void
RivaSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                             int fg, int bg, int rop, 
                                             unsigned int planemask)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RivaSetRopSolid(pRiva, rop);

    if ( bg == -1 )
    {
        /* Transparent case */
        bg = 0x80000000;
        pRiva->expandFifo = (unsigned char*)&pRiva->riva.Bitmap->MonochromeData1C;
    }
    else
    {
        pRiva->expandFifo = (unsigned char*)&pRiva->riva.Bitmap->MonochromeData01E;
        if (pScrn->depth == 16)
        {
            bg = ((bg & 0x0000F800) << 8)
               | ((bg & 0x000007E0) << 5)
               | ((bg & 0x0000001F) << 3)
               |        0xFF000000;
        }
        else
        {
            bg  |= pRiva->opaqueMonochrome;
        };
    }
    pRiva->FgColor = fg;
    pRiva->BgColor = bg;
}

static void
RivaSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    int t = pRiva->expandWidth;
    CARD32 *pbits = (CARD32*)pRiva->expandBuffer;
    CARD32 *d = (CARD32*)pRiva->expandFifo;
    
    while(t >= 16) 
    {
	RIVA_FIFO_FREE(pRiva->riva, Bitmap, 16);
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
	RIVA_FIFO_FREE(pRiva->riva, Bitmap, t);
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

    if (!(--pRiva->expandRows)) { /* hardware bug workaround */
       RIVA_FIFO_FREE(pRiva->riva, Blt, 1);
       write_mem_barrier();
       pRiva->riva.Blt->TopLeftSrc = 0;
    }
    write_mem_barrier();
}

static void
RivaSubsequentColorExpandScanlineFifo(ScrnInfoPtr pScrn, int bufno)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    if ( --pRiva->expandRows ) {
       RIVA_FIFO_FREE(pRiva->riva, Bitmap, pRiva->expandWidth);
    } else { /* hardware bug workaround */
       RIVA_FIFO_FREE(pRiva->riva, Blt, 1);
       write_mem_barrier();
       pRiva->riva.Blt->TopLeftSrc = 0;
    }
    write_mem_barrier();
}

static void
RivaSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x,
                                               int y, int w, int h,
                                               int skipleft)
{
    int bw;
    RivaPtr pRiva = RivaPTR(pScrn);
    
    bw = (w + 31) & ~31;
    pRiva->expandWidth = bw >> 5;

    if ( pRiva->BgColor == 0x80000000 )
    {
        /* Use faster transparent method */
        RIVA_FIFO_FREE(pRiva->riva, Bitmap, 5);
        pRiva->riva.Bitmap->ClipC.TopLeft     = (y << 16) | ((x+skipleft)
                                                           & 0xFFFF);
        pRiva->riva.Bitmap->ClipC.BottomRight = ((y+h) << 16) | ((x+w)&0xffff);
        pRiva->riva.Bitmap->Color1C           = pRiva->FgColor;
        pRiva->riva.Bitmap->WidthHeightC      = (h << 16) | bw;
        write_mem_barrier();
        pRiva->riva.Bitmap->PointC            = (y << 16) | (x & 0xFFFF);
        write_mem_barrier();
    }
    else
    {
        /* Opaque */
        RIVA_FIFO_FREE(pRiva->riva, Bitmap, 7);
        pRiva->riva.Bitmap->ClipE.TopLeft     = (y << 16) | ((x+skipleft)
                                                           & 0xFFFF);
        pRiva->riva.Bitmap->ClipE.BottomRight = ((y+h) << 16) | ((x+w)&0xffff);
        pRiva->riva.Bitmap->Color0E           = pRiva->BgColor;
        pRiva->riva.Bitmap->Color1E           = pRiva->FgColor;
        pRiva->riva.Bitmap->WidthHeightInE  = (h << 16) | bw;
        pRiva->riva.Bitmap->WidthHeightOutE = (h << 16) | bw;
        write_mem_barrier();
        pRiva->riva.Bitmap->PointE          = (y << 16) | (x & 0xFFFF);
        write_mem_barrier();
    }

    pRiva->expandRows = h;

    if(pRiva->expandWidth > (pRiva->riva.FifoEmptyCount >> 2)) {
	pRiva->AccelInfoRec->ScanlineColorExpandBuffers = &pRiva->expandBuffer;
	pRiva->AccelInfoRec->SubsequentColorExpandScanline = 
				RivaSubsequentColorExpandScanline;
    } else {
	pRiva->AccelInfoRec->ScanlineColorExpandBuffers = &pRiva->expandFifo;
	pRiva->AccelInfoRec->SubsequentColorExpandScanline = 
				RivaSubsequentColorExpandScanlineFifo;
	RIVA_FIFO_FREE(pRiva->riva, Bitmap, pRiva->expandWidth);
    }
}

static void
RivaSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop, unsigned planemask)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RivaSetRopSolid(pRiva, rop);
    pRiva->FgColor = color;
}

static void 
RivaSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len, int dir)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    RIVA_FIFO_FREE(pRiva->riva, Line, 3);
    pRiva->riva.Line->Color = pRiva->FgColor;
    pRiva->riva.Line->Lin[0].point0 = ((y << 16) | ( x & 0xffff));
    write_mem_barrier();
    if ( dir ==DEGREES_0 )
        pRiva->riva.Line->Lin[0].point1 = ((y << 16) | (( x + len ) & 0xffff));
    else
        pRiva->riva.Line->Lin[0].point1 = (((y + len) << 16) | ( x & 0xffff));
    write_mem_barrier();
}

static void 
RivaSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1,
                              int x2, int y2, int flags)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    Bool  lastPoint = !(flags & OMIT_LAST);

    RIVA_FIFO_FREE(pRiva->riva, Line, lastPoint ? 5 : 3);
    pRiva->riva.Line->Color = pRiva->FgColor;
    pRiva->riva.Line->Lin[0].point0 = ((y1 << 16) | (x1 & 0xffff));
    write_mem_barrier();
    pRiva->riva.Line->Lin[0].point1 = ((y2 << 16) | (x2 & 0xffff));
    write_mem_barrier();
    if (lastPoint)
    {
        pRiva->riva.Line->Lin[1].point0 = ((y2 << 16) | (x2 & 0xffff));
        write_mem_barrier();
        pRiva->riva.Line->Lin[1].point1 = (((y2 + 1) << 16) | (x2 & 0xffff));
        write_mem_barrier();
    }
}

static void
RivaValidatePolyArc(
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
RivaValidatePolyPoint(
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
RivaAccelInit(ScreenPtr pScreen) 
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RivaPtr pRiva = RivaPTR(pScrn);

    pRiva->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if(!infoPtr) return FALSE;

    /* fill out infoPtr here */
    infoPtr->Flags = LINEAR_FRAMEBUFFER | PIXMAP_CACHE | OFFSCREEN_PIXMAPS;

    /* sync */
    infoPtr->Sync = RivaSync;

    /* solid fills */
    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = RivaSetupForSolidFill;
    infoPtr->SubsequentSolidFillRect = RivaSubsequentSolidFillRect;

    /* screen to screen copy */
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY | NO_PLANEMASK;
    infoPtr->SetupForScreenToScreenCopy = RivaSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = RivaSubsequentScreenToScreenCopy;

    /* 8x8 mono patterns */
    /*
     * Set pattern opaque bits based on pixel format.
     */
    pRiva->opaqueMonochrome = ~((1 << pScrn->depth) - 1);

    infoPtr->Mono8x8PatternFillFlags = HARDWARE_PATTERN_SCREEN_ORIGIN |
				       HARDWARE_PATTERN_PROGRAMMED_BITS |
				       NO_PLANEMASK;
    infoPtr->SetupForMono8x8PatternFill = RivaSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect =
        RivaSubsequentMono8x8PatternFillRect;

    /* Color expansion */
    infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
				BIT_ORDER_IN_BYTE_LSBFIRST | 
				NO_PLANEMASK | 
				CPU_TRANSFER_PAD_DWORD |
				LEFT_EDGE_CLIPPING | 		
				LEFT_EDGE_CLIPPING_NEGATIVE_X;

    infoPtr->NumScanlineColorExpandBuffers = 1;

    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
        RivaSetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
        RivaSubsequentScanlineCPUToScreenColorExpandFill;

    pRiva->expandFifo = (unsigned char*)&pRiva->riva.Bitmap->MonochromeData01E;
    
    /* Allocate buffer for color expansion and also image writes in the
       future */
    pRiva->expandBuffer = xnfalloc(((pScrn->virtualX*pScrn->bitsPerPixel)/8) + 8);


    infoPtr->ScanlineColorExpandBuffers = &pRiva->expandBuffer;
    infoPtr->SubsequentColorExpandScanline = RivaSubsequentColorExpandScanline;

    infoPtr->SolidLineFlags = infoPtr->SolidFillFlags;
    infoPtr->SetupForSolidLine = RivaSetupForSolidLine;
    infoPtr->SubsequentSolidHorVertLine =
		RivaSubsequentSolidHorVertLine;
    infoPtr->SubsequentSolidTwoPointLine = 
		RivaSubsequentSolidTwoPointLine;
    infoPtr->SetClippingRectangle = RivaSetClippingRectangle;
    infoPtr->DisableClipping = RivaDisableClipping;
    infoPtr->ClippingFlags = HARDWARE_CLIP_SOLID_LINE;
    miSetZeroLineBias(pScreen, OCTANT1 | OCTANT3 | OCTANT4 | OCTANT6);

    infoPtr->ValidatePolyArc = RivaValidatePolyArc;
    infoPtr->PolyArcMask = GCFunction | GCLineWidth | GCPlaneMask;
    infoPtr->ValidatePolyPoint = RivaValidatePolyPoint;
    infoPtr->PolyPointMask = GCFunction | GCPlaneMask;
   
    RivaResetGraphics(pScrn);

    return(XAAInit(pScreen, infoPtr));
}

