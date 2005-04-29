/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_memory.c,v 1.28 2003/09/24 02:43:23 dawes Exp $ */
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

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"

#include "i810.h"
#include "i810_reg.h"

int
I810AllocLow(I810MemRange * result, I810MemRange * pool, int size)
{
   if (size > pool->Size)
      return 0;

   pool->Size -= size;
   result->Size = size;
   result->Start = pool->Start;
   result->End = pool->Start += size;

   return 1;
}

int
I810AllocHigh(I810MemRange * result, I810MemRange * pool, int size)
{
   if (size > pool->Size)
      return 0;

   pool->Size -= size;
   result->Size = size;
   result->End = pool->End;
   result->Start = pool->End -= size;

   return 1;
}

int
I810AllocateGARTMemory(ScrnInfoPtr pScrn)
{
   unsigned long size = pScrn->videoRam * 1024;
   I810Ptr pI810 = I810PTR(pScrn);
   int key;
   long tom = 0;
   unsigned long physical;

   if (!xf86AgpGARTSupported() || !xf86AcquireGART(pScrn->scrnIndex)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "AGP GART support is either not available or cannot be used.\n"
		 "\tMake sure your kernel has agpgart support or has the\n"
		 "\tagpgart module loaded.\n");
      return FALSE;
   }

   /* This allows the 2d only Xserver to regen */
   pI810->agpAcquired2d = TRUE;

   /*
    * I810/I815
    *
    * Treat the gart like video memory - we assume we own all that is
    * there, so ignore EBUSY errors.  Don't try to remove it on
    * failure, either, as other X server may be using it.
    */

   if ((key = xf86AllocateGARTMemory(pScrn->scrnIndex, size, 0, NULL)) == -1)
      return FALSE;

   pI810->VramOffset = 0;
   pI810->VramKey = key;

   if (!xf86BindGARTMemory(pScrn->scrnIndex, key, 0))
      return FALSE;

   pI810->SysMem.Start = 0;
   pI810->SysMem.Size = size;
   pI810->SysMem.End = size;
   pI810->SavedSysMem = pI810->SysMem;

   tom = pI810->SysMem.End;

   pI810->DcacheMem.Start = 0;
   pI810->DcacheMem.End = 0;
   pI810->DcacheMem.Size = 0;
   pI810->CursorPhysical = 0;

   /*
    * Dcache - half the speed of normal ram, so not really useful for
    * a 2d server.  Don't bother reporting its presence.  This is
    * mapped in addition to the requested amount of system ram.
    */

   size = 1024 * 4096;

   /*
    * Keep it 512K aligned for the sake of tiled regions.
    */

   tom += 0x7ffff;
   tom &= ~0x7ffff;

   if ((key = xf86AllocateGARTMemory(pScrn->scrnIndex, size, 1, NULL)) != -1) {
      pI810->DcacheOffset = tom;
      pI810->DcacheKey = key;
      if (!xf86BindGARTMemory(pScrn->scrnIndex, key, tom)) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Allocation of %ld bytes for DCACHE failed\n", size);
	 pI810->DcacheKey = -1;
      } else {
	 pI810->DcacheMem.Start = tom;
	 pI810->DcacheMem.Size = size;
	 pI810->DcacheMem.End = pI810->DcacheMem.Start + pI810->DcacheMem.Size;
	 tom = pI810->DcacheMem.End;
      }
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "No physical memory available for %ld bytes of DCACHE\n",
		 size);
      pI810->DcacheKey = -1;
   }

   /*
    * Mouse cursor -- The i810 (crazy) needs a physical address in
    * system memory from which to upload the cursor.  We get this from
    * the agpgart module using a special memory type.
    */

   /*
    * 4k for the cursor is excessive, I'm going to steal 3k for
    * overlay registers later
    */

   size = 4096;

   if ((key =
	xf86AllocateGARTMemory(pScrn->scrnIndex, size, 2, &physical)) == -1) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "No physical memory available for HW cursor\n");
      pI810->HwcursKey = -1;
      pI810->CursorStart = 0;
   } else {
      pI810->HwcursOffset = tom;
      pI810->HwcursKey = key;
      if (!xf86BindGARTMemory(pScrn->scrnIndex, key, tom)) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Allocation of %ld bytes for HW cursor failed\n", size);
	 pI810->HwcursKey = -1;
      } else {
	 pI810->CursorPhysical = physical;
	 pI810->CursorStart = tom;
	 tom += size;
      }
   }

   /*
    * Overlay register buffer -- Just like the cursor, the i810 needs a
    * physical address in system memory from which to upload the overlay
    * registers.
    */

   if (pI810->CursorStart != 0) {
      pI810->OverlayPhysical = pI810->CursorPhysical + 1024;
      pI810->OverlayStart = pI810->CursorStart + 1024;
   }

   pI810->GttBound = 1;

   return TRUE;
}

/* Tiled memory is good... really, really good...
 *
 * Need to make it less likely that we miss out on this - probably
 * need to move the frontbuffer away from the 'guarenteed' alignment
 * of the first memory segment, or perhaps allocate a discontigous
 * framebuffer to get more alignment 'sweet spots'.
 */
void
I810SetTiledMemory(ScrnInfoPtr pScrn, int nr, unsigned int start,
		   unsigned int pitch, unsigned int size)
{
   I810Ptr pI810 = I810PTR(pScrn);
   I810RegPtr i810Reg = &pI810->ModeReg;
   CARD32 val;
   CARD32 fence_mask = 0;

   if (nr < 0 || nr > 7) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex, "%s - fence %d out of range\n",
		 "I810SetTiledMemory", nr);
      return;
   }

   i810Reg->Fence[nr] = 0;

   fence_mask = ~FENCE_START_MASK;

   if (start & fence_mask) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "%s %d: start (%x) is not 512k aligned\n",
		 "I810SetTiledMemory", nr, start);
      return;
   }

   if (start % size) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "%s %d: start (%x) is not size (%x) aligned\n",
		 "I810SetTiledMemory", nr, start, size);
      return;
   }

   if (pitch & 127) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "%s %d: pitch (%x) not a multiple of 128 bytes\n",
		 "I810SetTiledMemory", nr, pitch);
      return;
   }

   val = (start | FENCE_X_MAJOR | FENCE_VALID);

   switch (size) {
   case KB(512):
      val |= FENCE_SIZE_512K;
      break;
   case MB(1):
      val |= FENCE_SIZE_1M;
      break;
   case MB(2):
      val |= FENCE_SIZE_2M;
      break;
   case MB(4):
      val |= FENCE_SIZE_4M;
      break;
   case MB(8):
      val |= FENCE_SIZE_8M;
      break;
   case MB(16):
      val |= FENCE_SIZE_16M;
      break;
   case MB(32):
      val |= FENCE_SIZE_32M;
      break;
   default:
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "%s %d: illegal size (0x%x)\n", "I810SetTiledMemory", nr,
		 size);
      return;
   }

   switch (pitch / 128) {
   case 1:
      val |= FENCE_PITCH_1;
      break;
   case 2:
      val |= FENCE_PITCH_2;
      break;
   case 4:
      val |= FENCE_PITCH_4;
      break;
   case 8:
      val |= FENCE_PITCH_8;
      break;
   case 16:
      val |= FENCE_PITCH_16;
      break;
   case 32:
      val |= FENCE_PITCH_32;
      break;
   default:
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "%s %d: illegal size (0x%x)\n", "I810SetTiledMemory", nr,
		 size);
      return;
   }

   i810Reg->Fence[nr] = val;
}

Bool
I810BindGARTMemory(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);

   if (xf86AgpGARTSupported() && !pI810->directRenderingEnabled
       && !pI810->GttBound) {
      if (!xf86AcquireGART(pScrn->scrnIndex))
	 return FALSE;

      if (pI810->VramKey != -1
	  && !xf86BindGARTMemory(pScrn->scrnIndex, pI810->VramKey,
				 pI810->VramOffset))
	 return FALSE;

      if (pI810->DcacheKey != -1
	  && !xf86BindGARTMemory(pScrn->scrnIndex, pI810->DcacheKey,
				 pI810->DcacheOffset))
	 return FALSE;

      if (pI810->HwcursKey != -1
	  && !xf86BindGARTMemory(pScrn->scrnIndex, pI810->HwcursKey,
				 pI810->HwcursOffset))
	 return FALSE;

      pI810->GttBound = 1;
   }

   return TRUE;
}

Bool
I810UnbindGARTMemory(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);

   if (xf86AgpGARTSupported() && !pI810->directRenderingEnabled
       && pI810->GttBound) {
      if (pI810->VramKey != -1
	  && !xf86UnbindGARTMemory(pScrn->scrnIndex, pI810->VramKey))
	 return FALSE;

      if (pI810->DcacheKey != -1
	  && !xf86UnbindGARTMemory(pScrn->scrnIndex, pI810->DcacheKey))
	 return FALSE;

      if (pI810->HwcursKey != -1
	  && !xf86UnbindGARTMemory(pScrn->scrnIndex, pI810->HwcursKey))
	 return FALSE;

      if (!xf86ReleaseGART(pScrn->scrnIndex))
	 return FALSE;

      pI810->GttBound = 0;
   }

   return TRUE;
}

int
I810CheckAvailableMemory(ScrnInfoPtr pScrn)
{
   AgpInfoPtr agpinf;
   int maxPages;

   if (!xf86AgpGARTSupported() ||
       !xf86AcquireGART(pScrn->scrnIndex) ||
       (agpinf = xf86GetAGPInfo(pScrn->scrnIndex)) == NULL ||
       !xf86ReleaseGART(pScrn->scrnIndex))
      return -1;

   maxPages = agpinf->totalPages - agpinf->usedPages;
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "%s: %dk available\n",
		  "I810CheckAvailableMemory", maxPages * 4);

   return maxPages * 4;
}
