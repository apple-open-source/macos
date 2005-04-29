/*
 * XXX So far, for GXxor this is about 40% of the speed of SW, but CPU
 * utilisation falls from 95% to < 5%.
 */
#ifndef DO_SCANLINE_IMAGE_WRITE
#define DO_SCANLINE_IMAGE_WRITE 0
#endif

/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_accel.c,v 1.9 2003/12/22 12:27:07 alanh Exp $ */

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */

#include "xf86_ansic.h"
#include "xf86.h"
#include "xaarop.h"
#include "i830.h"
#include "i810_reg.h"

int
I830WaitLpRing(ScrnInfoPtr pScrn, int n, int timeout_millis)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830RingBuffer *ring = &(pI830->LpRing);
   int iters = 0;
   int start = 0;
   int now = 0;
   int last_head = 0;
   int first = 0;

   /* If your system hasn't moved the head pointer in 2 seconds, I'm going to
    * call it crashed.
    */
   if (timeout_millis == 0)
      timeout_millis = 2000;

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL) {
      ErrorF("I830WaitLpRing %d\n", n);
      first = GetTimeInMillis();
   }

   while (ring->space < n) {
      ring->head = INREG(LP_RING + RING_HEAD) & I830_HEAD_MASK;
      ring->space = ring->head - (ring->tail + 8);

      if (ring->space < 0)
	 ring->space += ring->mem.Size;

      iters++;
      now = GetTimeInMillis();
      if (start == 0 || now < start || ring->head != last_head) {
	 if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
	    if (now > start)
	       ErrorF("space: %d wanted %d\n", ring->space, n);
	 start = now;
	 last_head = ring->head;
      } else if (now - start > timeout_millis) {
	 ErrorF("Error in I830WaitLpRing(), now is %d, start is %d\n", now,
		start);
	 I830PrintErrorState(pScrn);
	 ErrorF("space: %d wanted %d\n", ring->space, n);
#ifdef XF86DRI
	 if (pI830->directRenderingEnabled) {
	    DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
	    DRICloseScreen(screenInfo.screens[pScrn->scrnIndex]);
	 }
#endif
	 pI830->AccelInfoRec = NULL;	/* Stops recursive behavior */
	 FatalError("lockup\n");
      }

      DELAY(10);
   }

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL) {
      now = GetTimeInMillis();
      if (now - first) {
	 ErrorF("Elapsed %d ms\n", now - first);
	 ErrorF("space: %d wanted %d\n", ring->space, n);
      }
   }

   return iters;
}

void
I830Sync(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & (DEBUG_VERBOSE_ACCEL | DEBUG_VERBOSE_SYNC))
      ErrorF("I830Sync\n");

#ifdef XF86DRI
   /* VT switching tries to do this.
    */
   if (!pI830->LockHeld && pI830->directRenderingEnabled) {
      return;
   }
#endif

   /* Send a flush instruction and then wait till the ring is empty.
    * This is stronger than waiting for the blitter to finish as it also
    * flushes the internal graphics caches.
    */
   {
      BEGIN_LP_RING(2);
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(MI_NOOP);		/* pad to quadword */
      ADVANCE_LP_RING();
   }

   I830WaitLpRing(pScrn, pI830->LpRing.mem.Size - 8, 0);

   pI830->LpRing.space = pI830->LpRing.mem.Size - 8;
   pI830->nextColorExpandBuf = 0;
}

void
I830EmitFlush(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   BEGIN_LP_RING(2);
   OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
   OUT_RING(MI_NOOP);		/* pad to quadword */
   ADVANCE_LP_RING();
}

void
I830SelectBuffer(ScrnInfoPtr pScrn, int buffer)
{
   I830Ptr pI830 = I830PTR(pScrn);

   switch (buffer) {
#ifdef XF86DRI
   case I830_SELECT_BACK:
      pI830->bufferOffset = pI830->BackBuffer.Start;
      break;
   case I830_SELECT_DEPTH:
      pI830->bufferOffset = pI830->DepthBuffer.Start;
      break;
#endif
   default:
   case I830_SELECT_FRONT:
      pI830->bufferOffset = pI830->FrontBuffer.Start;
      break;
   }

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SelectBuffer %d --> offset %x\n",
	     buffer, pI830->bufferOffset);
}

void
I830RefreshRing(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->LpRing.head = INREG(LP_RING + RING_HEAD) & I830_HEAD_MASK;
   pI830->LpRing.tail = INREG(LP_RING + RING_TAIL);
   pI830->LpRing.space = pI830->LpRing.head - (pI830->LpRing.tail + 8);
   if (pI830->LpRing.space < 0)
      pI830->LpRing.space += pI830->LpRing.mem.Size;

   if (pI830->AccelInfoRec)
      pI830->AccelInfoRec->NeedToSync = TRUE;
}

/* I830 Accel Functions */

static void I830SetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
					   int pattx, int patty,
					   int fg, int bg, int rop,
					   unsigned int planemask);
static void I830SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
						 int pattx, int patty,
						 int x, int y, int w, int h);

static void I830SetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
							   int fg, int bg,
							   int rop,
							   unsigned int mask);

static void I830SubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr
							     pScrn, int x,
							     int y, int w,
							     int h,
							     int skipleft);

static void I830SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);

#if DO_SCANLINE_IMAGE_WRITE
static void I830SetupForScanlineImageWrite(ScrnInfoPtr pScrn, int rop,
					   unsigned int planemask,
					   int trans_color, int bpp,
					   int depth);
static void I830SubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn,
						 int x, int y, int w, int h,
						 int skipleft);
static void I830SubsequentImageWriteScanline(ScrnInfoPtr pScrn, int bufno);
#endif


/* The following function sets up the supported acceleration. Call it
 * from the FbInit() function in the SVGA driver, or before ScreenInit
 * in a monolithic server.
 */
Bool
I830AccelInit(ScreenPtr pScreen)
{
   XAAInfoRecPtr infoPtr;
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830AccelInit\n");

   pI830->AccelInfoRec = infoPtr = XAACreateInfoRec();
   if (!infoPtr)
      return FALSE;

   pI830->bufferOffset = 0;
   infoPtr->Flags = LINEAR_FRAMEBUFFER | OFFSCREEN_PIXMAPS | PIXMAP_CACHE;

   /* Use the same sync function as the I830.
    */
   infoPtr->Sync = I830Sync;

   /* Everything else is different enough to justify different functions */
   {
      infoPtr->SolidFillFlags = NO_PLANEMASK;
      infoPtr->SetupForSolidFill = I830SetupForSolidFill;
      infoPtr->SubsequentSolidFillRect = I830SubsequentSolidFillRect;
   }

   {
      infoPtr->ScreenToScreenCopyFlags = (NO_PLANEMASK | NO_TRANSPARENCY);

      infoPtr->SetupForScreenToScreenCopy = I830SetupForScreenToScreenCopy;
      infoPtr->SubsequentScreenToScreenCopy =
	    I830SubsequentScreenToScreenCopy;
   }

   {
      infoPtr->SetupForMono8x8PatternFill = I830SetupForMono8x8PatternFill;
      infoPtr->SubsequentMono8x8PatternFillRect =
	    I830SubsequentMono8x8PatternFillRect;

      infoPtr->Mono8x8PatternFillFlags = (HARDWARE_PATTERN_PROGRAMMED_BITS |
					  HARDWARE_PATTERN_SCREEN_ORIGIN |
					  HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
					  BIT_ORDER_IN_BYTE_MSBFIRST |
					  NO_PLANEMASK);

   }

   if (pI830->Scratch.Size != 0) {
      int i;
      int width = ((pScrn->displayWidth + 31) & ~31) / 8;
      int nr_buffers = pI830->Scratch.Size / width;
      unsigned char *ptr = pI830->FbBase + pI830->Scratch.Start;

      pI830->NumScanlineColorExpandBuffers = nr_buffers;
      pI830->ScanlineColorExpandBuffers = (unsigned char **)
	    xnfcalloc(nr_buffers, sizeof(unsigned char *));

      for (i = 0; i < nr_buffers; i++, ptr += width)
	 pI830->ScanlineColorExpandBuffers[i] = ptr;

      infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
	    (NO_PLANEMASK | ROP_NEEDS_SOURCE | BIT_ORDER_IN_BYTE_MSBFIRST);

      infoPtr->ScanlineColorExpandBuffers = (unsigned char **)
	    xnfcalloc(1, sizeof(unsigned char *));
      infoPtr->NumScanlineColorExpandBuffers = 1;

      infoPtr->ScanlineColorExpandBuffers[0] =
	    pI830->ScanlineColorExpandBuffers[0];
      pI830->nextColorExpandBuf = 0;

      infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
	    I830SetupForScanlineCPUToScreenColorExpandFill;

      infoPtr->SubsequentScanlineCPUToScreenColorExpandFill =
	    I830SubsequentScanlineCPUToScreenColorExpandFill;

      infoPtr->SubsequentColorExpandScanline =
	    I830SubsequentColorExpandScanline;

#if DO_SCANLINE_IMAGE_WRITE
      infoPtr->NumScanlineImageWriteBuffers = 1;
      infoPtr->ScanlineImageWriteBuffers = infoPtr->ScanlineColorExpandBuffers;
      infoPtr->SetupForScanlineImageWrite = I830SetupForScanlineImageWrite;
      infoPtr->SubsequentScanlineImageWriteRect =
	    I830SubsequentScanlineImageWriteRect;
      infoPtr->SubsequentImageWriteScanline = I830SubsequentImageWriteScanline;
      infoPtr->ScanlineImageWriteFlags = NO_GXCOPY |
					 NO_PLANEMASK |
					 ROP_NEEDS_SOURCE |
					 SCANLINE_PAD_DWORD;
#endif
   }

   I830SelectBuffer(pScrn, I830_SELECT_FRONT);

   return XAAInit(pScreen, infoPtr);
}

void
I830SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
		      unsigned int planemask)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SetupForFillRectSolid color: %x rop: %x mask: %x\n",
	     color, rop, planemask);

   pI830->BR[13] = ((XAAPatternROP[rop] << 16) |
		    (pScrn->displayWidth * pI830->cpp));

   pI830->BR[16] = color;

   switch (pScrn->bitsPerPixel) {
   case 8:
      break;
   case 16:
      pI830->BR[13] |= (1 << 24);
      break;
   case 32:
      pI830->BR[13] |= ((1 << 25) | (1 << 24));
      break;
   }
}

void
I830SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentFillRectSolid %d,%d %dx%d\n", x, y, w, h);

   {
      BEGIN_LP_RING(6);

      if (pScrn->bitsPerPixel == 32) {
	 OUT_RING(COLOR_BLT_CMD | COLOR_BLT_WRITE_ALPHA |
		  COLOR_BLT_WRITE_RGB);
      } else {
	 OUT_RING(COLOR_BLT_CMD);
      }
      OUT_RING(pI830->BR[13]);
      OUT_RING((h << 16) | (w * pI830->cpp));
      OUT_RING(pI830->bufferOffset + (y * pScrn->displayWidth + x) *
	       pI830->cpp);
      OUT_RING(pI830->BR[16]);
      OUT_RING(0);

      ADVANCE_LP_RING();
   }
}

void
I830SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
			       unsigned int planemask, int transparency_color)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SetupForScreenToScreenCopy %d %d %x %x %d\n",
	     xdir, ydir, rop, planemask, transparency_color);

   pI830->BR[13] = (pScrn->displayWidth * pI830->cpp);
   pI830->BR[13] |= XAACopyROP[rop] << 16;

   switch (pScrn->bitsPerPixel) {
   case 8:
      break;
   case 16:
      pI830->BR[13] |= (1 << 24);
      break;
   case 32:
      pI830->BR[13] |= ((1 << 25) | (1 << 24));
      break;
   }

}

void
I830SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int src_x1, int src_y1,
				 int dst_x1, int dst_y1, int w, int h)
{
   I830Ptr pI830 = I830PTR(pScrn);
   int dst_x2, dst_y2;

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentScreenToScreenCopy %d,%d - %d,%d %dx%d\n",
	     src_x1, src_y1, dst_x1, dst_y1, w, h);

   dst_x2 = dst_x1 + w;
   dst_y2 = dst_y1 + h;

   {
      BEGIN_LP_RING(8);

      if (pScrn->bitsPerPixel == 32) {
	 OUT_RING(XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_WRITE_ALPHA |
		  XY_SRC_COPY_BLT_WRITE_RGB);
      } else {
	 OUT_RING(XY_SRC_COPY_BLT_CMD);
      }
      OUT_RING(pI830->BR[13]);
      OUT_RING((dst_y1 << 16) | (dst_x1 & 0xffff));
      OUT_RING((dst_y2 << 16) | (dst_x2 & 0xffff));
      OUT_RING(pI830->bufferOffset);
      OUT_RING((src_y1 << 16) | (src_x1 & 0xffff));
      OUT_RING(pI830->BR[13] & 0xFFFF);
      OUT_RING(pI830->bufferOffset);

      ADVANCE_LP_RING();
   }
}

static void
I830SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int pattx, int patty,
			       int fg, int bg, int rop,
			       unsigned int planemask)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SetupForMono8x8PatternFill\n");

   pI830->BR[16] = pattx;
   pI830->BR[17] = patty;
   pI830->BR[18] = bg;
   pI830->BR[19] = fg;

   pI830->BR[13] = (pScrn->displayWidth * pI830->cpp);	/* In bytes */
   pI830->BR[13] |= XAAPatternROP[rop] << 16;
   if (bg == -1)
      pI830->BR[13] |= (1 << 28);

   switch (pScrn->bitsPerPixel) {
   case 8:
      break;
   case 16:
      pI830->BR[13] |= (1 << 24);
      break;
   case 32:
      pI830->BR[13] |= ((1 << 25) | (1 << 24));
      break;
   }

}

static void
I830SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int pattx, int patty,
				     int x, int y, int w, int h)
{
   I830Ptr pI830 = I830PTR(pScrn);
   int x1, x2, y1, y2;

   x1 = x;
   x2 = x + w;
   y1 = y;
   y2 = y + h;

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentMono8x8PatternFillRect\n");

   {
      BEGIN_LP_RING(10);

      if (pScrn->bitsPerPixel == 32) {
	 OUT_RING(XY_MONO_PAT_BLT_CMD | XY_MONO_PAT_BLT_WRITE_ALPHA |
		  XY_MONO_PAT_BLT_WRITE_RGB |
		  ((patty << 8) & XY_MONO_PAT_VERT_SEED) |
		  ((pattx << 12) & XY_MONO_PAT_HORT_SEED));
      } else {
	 OUT_RING(XY_MONO_PAT_BLT_CMD |
		  ((patty << 8) & XY_MONO_PAT_VERT_SEED) |
		  ((pattx << 12) & XY_MONO_PAT_HORT_SEED));
      }
      OUT_RING(pI830->BR[13]);
      OUT_RING((y1 << 16) | x1);
      OUT_RING((y2 << 16) | x2);
      OUT_RING(pI830->bufferOffset);
      OUT_RING(pI830->BR[18]);		/* bg */
      OUT_RING(pI830->BR[19]);		/* fg */
      OUT_RING(pI830->BR[16]);		/* pattern data */
      OUT_RING(pI830->BR[17]);
      OUT_RING(0);
      ADVANCE_LP_RING();
   }
}

static void
I830GetNextScanlineColorExpandBuffer(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   XAAInfoRecPtr infoPtr = pI830->AccelInfoRec;

   if (pI830->nextColorExpandBuf == pI830->NumScanlineColorExpandBuffers)
      I830Sync(pScrn);

   infoPtr->ScanlineColorExpandBuffers[0] =
	 pI830->ScanlineColorExpandBuffers[pI830->nextColorExpandBuf];

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("using color expand buffer %d\n", pI830->nextColorExpandBuf);

   pI830->nextColorExpandBuf++;
}

static void
I830SetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
					       int fg, int bg, int rop,
					       unsigned int planemask)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SetupForScanlineScreenToScreenColorExpand %d %d %x %x\n",
	     fg, bg, rop, planemask);

   /* Fill out register values */
   pI830->BR[13] = (pScrn->displayWidth * pI830->cpp);
   pI830->BR[13] |= XAACopyROP[rop] << 16;
   if (bg == -1)
      pI830->BR[13] |= (1 << 29);

   switch (pScrn->bitsPerPixel) {
   case 8:
      break;
   case 16:
      pI830->BR[13] |= (1 << 24);
      break;
   case 32:
      pI830->BR[13] |= ((1 << 25) | (1 << 24));
      break;
   }

   pI830->BR[18] = bg;
   pI830->BR[19] = fg;

   I830GetNextScanlineColorExpandBuffer(pScrn);
}

static void
I830SubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
						 int x, int y,
						 int w, int h, int skipleft)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentScanlineCPUToScreenColorExpandFill "
	     "%d,%d %dx%x %d\n", x, y, w, h, skipleft);

   /* Fill out register values */
   pI830->BR[9] = (pI830->bufferOffset +
		   (y * pScrn->displayWidth + x) * pI830->cpp);
   pI830->BR[11] = ((1 << 16) | w);
}

static void
I830SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->BR[12] = (pI830->AccelInfoRec->ScanlineColorExpandBuffers[0] -
		    pI830->FbBase);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentColorExpandScanline %d (addr %x)\n",
	     bufno, pI830->BR[12]);

   {
      BEGIN_LP_RING(8);

      if (pScrn->bitsPerPixel == 32) {
	 OUT_RING(XY_MONO_SRC_BLT_CMD | XY_MONO_SRC_BLT_WRITE_ALPHA |
		  XY_MONO_SRC_BLT_WRITE_RGB);
      } else {
	 OUT_RING(XY_MONO_SRC_BLT_CMD);
      }
      OUT_RING(pI830->BR[13]);
      OUT_RING(0);			/* x1 = 0, y1 = 0 */
      OUT_RING(pI830->BR[11]);		/* x2 = w, y2 = 1 */
      OUT_RING(pI830->BR[9]);		/* dst addr */
      OUT_RING(pI830->BR[12]);		/* src addr */
      OUT_RING(pI830->BR[18]);		/* bg */
      OUT_RING(pI830->BR[19]);		/* fg */

      ADVANCE_LP_RING();
   }

   /* Advance to next scanline.
    */
   pI830->BR[9] += pScrn->displayWidth * pI830->cpp;
   I830GetNextScanlineColorExpandBuffer(pScrn);
}

#if DO_SCANLINE_IMAGE_WRITE
static void
I830SetupForScanlineImageWrite(ScrnInfoPtr pScrn, int rop,
			       unsigned int planemask, int trans_color,
			       int bpp, int depth)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SetupForScanlineImageWrite %x %x\n", rop, planemask);

   /* Fill out register values */
   pI830->BR[13] = (pScrn->displayWidth * pI830->cpp);
   pI830->BR[13] |= XAACopyROP[rop] << 16;

   switch (pScrn->bitsPerPixel) {
   case 8:
      break;
   case 16:
      pI830->BR[13] |= (1 << 24);
      break;
   case 32:
      pI830->BR[13] |= ((1 << 25) | (1 << 24));
      break;
   }

   I830GetNextScanlineColorExpandBuffer(pScrn);
}

static void
I830SubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn, int x, int y,
				     int w, int h, int skipleft)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentScanlineImageWriteRect "
	     "%d,%d %dx%x %d\n", x, y, w, h, skipleft);

   /* Fill out register values */
   pI830->BR[9] = (pI830->bufferOffset +
		   (y * pScrn->displayWidth + x) * pI830->cpp);
   pI830->BR[11] = ((1 << 16) | w);
}

static void
I830SubsequentImageWriteScanline(ScrnInfoPtr pScrn, int bufno)
{
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->BR[12] = (pI830->AccelInfoRec->ScanlineColorExpandBuffers[0] -
		    pI830->FbBase);

   if (I810_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SubsequentImageWriteScanline %d (addr %x)\n",
	     bufno, pI830->BR[12]);

   {
      BEGIN_LP_RING(8);

      if (pScrn->bitsPerPixel == 32) {
	 OUT_RING(XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_WRITE_ALPHA |
		  XY_SRC_COPY_BLT_WRITE_RGB);
      } else {
	 OUT_RING(XY_SRC_COPY_BLT_CMD);
      }
      OUT_RING(pI830->BR[13]);
      OUT_RING(0);			/* x1 = 0, y1 = 0 */
      OUT_RING(pI830->BR[11]);		/* x2 = w, y2 = 1 */
      OUT_RING(pI830->BR[9]);		/* dst addr */
      OUT_RING(0);			/* source origin (0,0) */
      OUT_RING(pI830->BR[11] & 0xffff);	/* source pitch */
      OUT_RING(pI830->BR[12]);		/* src addr */

      ADVANCE_LP_RING();
   }

   /* Advance to next scanline.
    */
   pI830->BR[9] += pScrn->displayWidth * pI830->cpp;
   I830GetNextScanlineColorExpandBuffer(pScrn);
}

#endif
