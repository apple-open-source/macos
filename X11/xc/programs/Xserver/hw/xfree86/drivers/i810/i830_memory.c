/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_memory.c,v 1.10 2003/10/21 02:17:52 dawes Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 by David Dawes.

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
IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR
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
 *   David Dawes <dawes@xfree86.org>
 *
 */

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"

#include "i830.h"
#include "i810_reg.h"

/*
 * Allocate memory from the given pool.  Grow the pool if needed and if
 * possible.
 */
static unsigned long
AllocFromPool(ScrnInfoPtr pScrn, I830MemRange *result, I830MemPool *pool,
	      unsigned long size, unsigned long alignment, int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long needed, start, end;
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);

   if (!result || !pool || !size)
      return 0;

   /* Calculate how much space is needed. */
   if (alignment <= GTT_PAGE_SIZE)
      needed = size;
   else {
      if (flags & ALLOCATE_AT_BOTTOM) {
	 start = ROUND_TO(pool->Free.Start, alignment);
	 if (flags & ALIGN_BOTH_ENDS)
	    end = ROUND_TO(start + size, alignment);
	 else
	    end = start + size;
	 needed = end - pool->Free.Start;
      } else {				/* allocate at top */
	 if (flags & ALIGN_BOTH_ENDS)
	    end = ROUND_DOWN_TO(pool->Free.End, alignment);
	 else
	    end = pool->Free.End;

	 start = ROUND_DOWN_TO(end - size, alignment);
	 needed = pool->Free.End - start;
      }
   }
   if (needed > pool->Free.Size) {
      unsigned long extra;
      /* See if the pool can be grown. */
      if (pI830->StolenOnly && !dryrun)
	 return 0;
      extra = needed - pool->Free.Size;
      extra = ROUND_TO_PAGE(extra);
      if (extra > pI830->FreeMemory) {
	 if (dryrun)
	    pI830->FreeMemory = extra;
	 else
	    return 0;
      }

      if (!dryrun && (extra > pI830->MemoryAperture.Size))
	 return 0;

      pool->Free.Size += extra;
      pool->Free.End += extra;
      pool->Total.Size += extra;
      pool->Total.End += extra;
      pI830->FreeMemory -= extra;
      pI830->MemoryAperture.Start += extra;
      pI830->MemoryAperture.Size -= extra;
   }
   if (flags & ALLOCATE_AT_BOTTOM) {
      result->Start = ROUND_TO(pool->Free.Start, alignment);
      pool->Free.Start += needed;
      result->End = pool->Free.Start;
   } else {
      result->Start = ROUND_DOWN_TO(pool->Free.End - size, alignment) -
			pool->Total.End;
      result->End = pool->Free.End - pool->Total.End;
      pool->Free.End -= needed;
   }
   pool->Free.Size = pool->Free.End - pool->Free.Start;
   result->Size = result->End - result->Start;
   result->Pool = pool;
   result->Alignment = alignment;
   return needed;
}

static unsigned long
AllocFromAGP(ScrnInfoPtr pScrn, I830MemRange *result, unsigned long size,
	     unsigned long alignment, int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long start, end;
   unsigned long newApStart, newApEnd;
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);

   if (!result || !size)
      return 0;

   if ((flags & ALLOCATE_AT_BOTTOM) && pI830->StolenMemory.Size != 0) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "AllocFromAGP(): can't allocate from "
		 "bottom when there is stolen memory\n");
      return 0;
   }

   if (size > pI830->FreeMemory) {
      if (dryrun)
	 pI830->FreeMemory = size;
      else
	 return 0;
   }

   /* Calculate offset */
   if (flags & ALLOCATE_AT_BOTTOM) {
      start = ROUND_TO(pI830->MemoryAperture.Start, alignment);
      if (flags & ALIGN_BOTH_ENDS)
	 end = ROUND_TO(start + size, alignment);
      else
	 end = start + size;
      newApStart = end;
      newApEnd = pI830->MemoryAperture.End;
   } else {
      if (flags & ALIGN_BOTH_ENDS)
	 end = ROUND_DOWN_TO(pI830->MemoryAperture.End, alignment);
      else
	 end = pI830->MemoryAperture.End;
      start = ROUND_DOWN_TO(end - size, alignment);
      newApStart = pI830->MemoryAperture.Start;
      newApEnd = start;
   }

   if (!dryrun) {
      if (newApStart > newApEnd)
	 return 0;

      if (flags & NEED_PHYSICAL_ADDR) {
	 result->Key = xf86AllocateGARTMemory(pScrn->scrnIndex, size, 2,
					      &(result->Physical));
      } else {
	 result->Key = xf86AllocateGARTMemory(pScrn->scrnIndex, size, 0, NULL);
      }
      if (result->Key == -1)
	 return 0;
   }

   pI830->allocatedMemory += size;
   pI830->MemoryAperture.Start = newApStart;
   pI830->MemoryAperture.End = newApEnd;
   pI830->MemoryAperture.Size = newApEnd - newApStart;
   pI830->FreeMemory -= size;
   result->Start = start;
   result->End = start + size;
   result->Size = size;
   result->Offset = start;
   result->Alignment = alignment;
   result->Pool = NULL;

   return size;
}


unsigned long
I830AllocVidMem(ScrnInfoPtr pScrn, I830MemRange *result, I830MemPool *pool,
		unsigned long size, unsigned long alignment, int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);

   if (!result)
      return 0;

   /* Make sure these are initialised. */
   result->Size = 0;
   result->Key = -1;

   if (!size) {
      return 0;
   }

   switch (flags & FROM_MASK) {
   case FROM_POOL_ONLY:
      return AllocFromPool(pScrn, result, pool, size, alignment, flags);
   case FROM_NEW_ONLY:
      if (!dryrun && (pI830->StolenOnly || (pI830->FreeMemory <= 0)))
	 return 0;
      return AllocFromAGP(pScrn, result, size, alignment, flags);
   case FROM_ANYWHERE:
      if ((!(flags & ALLOCATE_AT_BOTTOM) && (pI830->FreeMemory >= size)) ||
	  (flags & NEED_PHYSICAL_ADDR))
	 return AllocFromAGP(pScrn, result, size, alignment, flags);
      else
	 return AllocFromPool(pScrn, result, pool, size, alignment, flags);
   default:
      /* Shouldn't happen. */
      return 0;
   }
}

static Bool
AllocateRingBuffer(ScrnInfoPtr pScrn, int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long size, alloced;
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);
   int verbosity = dryrun ? 4 : 1;
   const char *s = dryrun ? "[dryrun] " : "";

   /* Clear ring buffer  info */
   memset(&(pI830->LpRing), 0, sizeof(pI830->LpRing));
   pI830->LpRing.mem.Key = -1;

   if (pI830->noAccel)
      return TRUE;

   /* Ring buffer */
   size = PRIMARY_RINGBUFFER_SIZE;
   if (flags & FORCE_LOW)
      flags |= FROM_POOL_ONLY | ALLOCATE_AT_BOTTOM;
   else
      flags |= FROM_ANYWHERE | ALLOCATE_AT_TOP;

   alloced = I830AllocVidMem(pScrn, &(pI830->LpRing.mem),
				&(pI830->StolenPool), size,
				GTT_PAGE_SIZE, flags);
   if (alloced < size) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to allocate Ring Buffer space\n");
      }
      return FALSE;
   }
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		  "%sAllocated %ld kB for the ring buffer at 0x%lx\n", s,
		  alloced / 1024, pI830->LpRing.mem.Start);
   pI830->LpRing.tail_mask = pI830->LpRing.mem.Size - 1;
   return TRUE;
}

#ifdef I830_XV
/*
 * Note, the FORCE_LOW flag is currently not used or supported.
 */
static Bool
AllocateOverlay(ScrnInfoPtr pScrn, int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long size, alloced;
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);
   int verbosity = dryrun ? 4 : 1;
   const char *s = dryrun ? "[dryrun] " : "";

   /* Clear overlay info */
   memset(&(pI830->OverlayMem), 0, sizeof(pI830->OverlayMem));
   pI830->OverlayMem.Key = -1;

   if (!pI830->XvEnabled)
      return TRUE;

   /*
    * The overlay register space needs a physical address in
    * system memory.  We get this from the agpgart module using
    * a special memory type.
    */

   size = OVERLAY_SIZE;
   if (flags & FORCE_LOW)
      flags |= FROM_POOL_ONLY | ALLOCATE_AT_BOTTOM | NEED_PHYSICAL_ADDR;
   else
      flags |= FROM_ANYWHERE | ALLOCATE_AT_TOP | NEED_PHYSICAL_ADDR;

   alloced = I830AllocVidMem(pScrn, &(pI830->OverlayMem),
			     &(pI830->StolenPool), size, GTT_PAGE_SIZE, flags);

   /*
    * XXX For testing only.  Don't enable this unless you know how to set
    * physBase.
    */
   if (flags & FORCE_LOW) {
      ErrorF("AllocateOverlay() doesn't support setting FORCE_LOW\n");
      return FALSE;
   }

   if (!dryrun && (alloced < size)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "Failed to allocate Overlay register space.\n");
	 /* This failure isn't fatal. */
   } else {
      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		     "%sAllocated %ld kB for Overlay registers at 0x%lx "
		     "(0x%08lx).\n", s,
		     alloced / 1024, pI830->OverlayMem.Start,
		     pI830->OverlayMem.Physical);
   }
   return TRUE;
}
#endif

static unsigned long
GetFreeSpace(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long extra = 0;

   /* First check for free space in StolenPool. */
   if (pI830->StolenPool.Free.Size > 0)
      extra = pI830->StolenPool.Free.Size;
   /* Next check for unallocated space. */
   if (pI830->FreeMemory > 0)
      extra += pI830->FreeMemory;

   return extra;
}

static Bool
IsTileable(int pitch)
{
   /*
    * Allow tiling for pitches that are a power of 2 multiple of 128 bytes,
    * up to 64 * 128 (= 8192) bytes.
    */
   switch (pitch) {
   case 128 * 1:
   case 128 * 2:
   case 128 * 4:
   case 128 * 8:
   case 128 * 16:
   case 128 * 32:
   case 128 * 64:
      return TRUE;
   default:
      return FALSE;
   }
}

/*
 * Allocate memory for 2D operation.  This includes the (front) framebuffer,
 * ring buffer, scratch memory, HW cursor.
 */

Bool
I830Allocate2DMemory(ScrnInfoPtr pScrn, const int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long size, alloced;
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);
   int verbosity = dryrun ? 4 : 1;
   const char *s = dryrun ? "[dryrun] " : "";
   Bool tileable;
   int align, alignflags;

   DPRINTF(PFX, "I830Allocate2DMemory: inital is %s\n",
	   BOOLTOSTRING(flags & ALLOC_INITIAL));

   if (!pI830->StolenOnly &&
       (!xf86AgpGARTSupported() || !xf86AcquireGART(pScrn->scrnIndex))) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "AGP GART support is either not available or cannot "
		    "be used.\n"
		    "\tMake sure your kernel has agpgart support or has the\n"
		    "\tagpgart module loaded.\n");
      }
      return FALSE;
   }


   /*
    * The I830 is slightly different from the I830/I815, it has no
    * dcache and it has stolen memory by default in its gtt.  All
    * additional memory must go after it.
    */

   DPRINTF(PFX,
	   "size == %luk (%lu bytes == pScrn->videoRam)\n"
	   "pI830->StolenSize == %luk (%lu bytes)\n",
	   pScrn->videoRam, pScrn->videoRam * 1024,
	   pI830->StolenPool.Free.Size / 1024,
	   pI830->StolenPool.Free.Size);

   if (flags & ALLOC_INITIAL) {
      unsigned long minspace, avail, lineSize;
      int cacheLines, maxCacheLines;

      if (pI830->NeedRingBufferLow)
	 AllocateRingBuffer(pScrn, flags | FORCE_LOW);

      /* Clear everything first. */
      memset(&(pI830->FbMemBox), 0, sizeof(pI830->FbMemBox));
      memset(&(pI830->FrontBuffer), 0, sizeof(pI830->FrontBuffer));
      pI830->FrontBuffer.Key = -1;

      pI830->FbMemBox.x1 = 0;
      pI830->FbMemBox.x2 = pScrn->displayWidth;
      pI830->FbMemBox.y1 = 0;
      pI830->FbMemBox.y2 = pScrn->virtualY;

      /*
       * Calculate how much framebuffer memory to allocate.  For the
       * initial allocation, calculate a reasonable minimum.  This is
       * enough for the virtual screen size, plus some pixmap cache
       * space.
       */

      lineSize = pScrn->displayWidth * pI830->cpp;
      minspace = lineSize * pScrn->virtualY;
      avail = pScrn->videoRam * 1024;
      maxCacheLines = (avail - minspace) / lineSize;
      /* This shouldn't happen. */
      if (maxCacheLines < 0) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Internal Error: "
		    "maxCacheLines < 0 in I830Allocate2DMemory()\n");
	 maxCacheLines = 0;
      }
      if (maxCacheLines > (MAX_DISPLAY_HEIGHT - pScrn->virtualY))
	 maxCacheLines = MAX_DISPLAY_HEIGHT - pScrn->virtualY;

      if (pI830->CacheLines >= 0) {
	 cacheLines = pI830->CacheLines;
      } else {
#if 1
	 /* Make sure there is enough for two DVD sized YUV buffers */
	 cacheLines = (pScrn->depth == 24) ? 256 : 384;
	 if (pScrn->displayWidth <= 1024)
	    cacheLines *= 2;
#else
	 /*
	  * Make sure there is enough for two DVD sized YUV buffers.
	  * Make that 1.5MB, which is around what was allocated with
	  * the old algorithm
	  */
	 cacheLines = (MB(1) + KB(512)) / pI830->cpp / pScrn->displayWidth;
#endif
      }
      if (cacheLines > maxCacheLines)
	 cacheLines = maxCacheLines;

      pI830->FbMemBox.y2 += cacheLines;

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		     "%sAllocating at least %d scanlines for pixmap cache\n",
		     s, cacheLines);

      tileable = !(flags & ALLOC_NO_TILING) && pI830->allowPageFlip &&
		 IsTileable(pScrn->displayWidth * pI830->cpp);
      if (tileable) {
	 align = KB(512);
	 alignflags = ALIGN_BOTH_ENDS;
      } else {
	 align = KB(64);
	 alignflags = 0;
      }

      size = lineSize * (pScrn->virtualY + cacheLines);
      size = ROUND_TO_PAGE(size);
      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		     "%sInitial framebuffer allocation size: %ld kByte\n", s,
		     size / 1024);
      alloced = I830AllocVidMem(pScrn, &(pI830->FrontBuffer),
				&(pI830->StolenPool), size, align,
				flags | alignflags |
				FROM_ANYWHERE | ALLOCATE_AT_BOTTOM);
      if (alloced < size) {
	 if (!dryrun) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Failed to allocate framebuffer.\n");
	 }
	 return FALSE;
      }
   } else {
      unsigned long lineSize;
      unsigned long extra = 0;
      unsigned long maxFb = 0;

      /*
       * XXX Need to "free" up any 3D allocations if the DRI ended up
       * and make them available for 2D.  The best way to do this would
       * be position all of those regions contiguously at the end of the
       * StolenPool.
       */
      extra = GetFreeSpace(pScrn);

      if (extra == 0)
	 return TRUE;

      maxFb = pI830->FrontBuffer.Size + extra;
      lineSize = pScrn->displayWidth * pI830->cpp;
      maxFb = ROUND_DOWN_TO(maxFb, lineSize);
      if (maxFb > lineSize * MAX_DISPLAY_HEIGHT)
	 maxFb = lineSize * MAX_DISPLAY_HEIGHT;
      if (maxFb > pI830->FrontBuffer.Size) {
	 unsigned long oldsize;
	 /*
	  * Sanity check -- the fb should be the last thing allocated at
	  * the bottom of the stolen pool.
	  */
	 if (pI830->StolenPool.Free.Start != pI830->FrontBuffer.End) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Internal error in I830Allocate2DMemory():\n\t"
		       "Framebuffer isn't the last allocation at the bottom"
		       " of StolenPool\n\t(%lx != %lx).\n",
		       pI830->FrontBuffer.End,
		       pI830->StolenPool.Free.Start);
	    return FALSE;
	 }
	 /*
	  * XXX Maybe should have a "Free" function.  This should be
	  * the only place where a region is resized, and we know that
	  * the fb is always at the bottom of the aperture/stolen pool,
	  * and is the only region that is allocated bottom-up.
	  * Allowing for more general realloction would require a smarter
	  * allocation system.
	  */
	 oldsize = pI830->FrontBuffer.Size;
	 pI830->StolenPool.Free.Size += pI830->FrontBuffer.Size;
	 pI830->StolenPool.Free.Start -= pI830->FrontBuffer.Size;
	 xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
			"%sUpdated framebuffer allocation size from %ld "
			"to %ld kByte\n", s, oldsize / 1024, maxFb / 1024);
	 xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
			"%sUpdated pixmap cache from %ld scanlines to %ld "
			"scanlines\n", s,
			oldsize / lineSize - pScrn->virtualY,
			maxFb / lineSize - pScrn->virtualY);
	 pI830->FbMemBox.y2 = maxFb / lineSize;
	 tileable = !(flags & ALLOC_NO_TILING) && pI830->allowPageFlip &&
		 IsTileable(pScrn->displayWidth * pI830->cpp);
	 if (tileable) {
	    align = KB(512);
	    alignflags = ALIGN_BOTH_ENDS;
	 } else {
	    align = KB(64);
	    alignflags = 0;
	 }
	 alloced = I830AllocVidMem(pScrn, &(pI830->FrontBuffer),
				   &(pI830->StolenPool), maxFb, align,
				   flags | alignflags |
				   FROM_ANYWHERE | ALLOCATE_AT_BOTTOM);
	 if (alloced < maxFb) {
	    if (!dryrun) {
	       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			  "Failed to re-allocate framebuffer\n");
	    }
	    return FALSE;
	 }
      }
      return TRUE;
   }

#if REMAP_RESERVED
   /*
    * Allocate a dummy page to pass when attempting to rebind the
    * pre-allocated region.
    */
   if (!dryrun) {
      memset(&(pI830->Dummy), 0, sizeof(pI830->Dummy));
      pI830->Dummy.Key =
	   xf86AllocateGARTMemory(pScrn->scrnIndex, size, 0, NULL);
      pI830->Dummy.Offset = 0;
   }
#endif

   /* Clear cursor info */
   memset(&(pI830->CursorMem), 0, sizeof(pI830->CursorMem));
   pI830->CursorMem.Key = -1;

   if (!pI830->SWCursor) {
      int cursFlags = 0;
      /*
       * Mouse cursor -- The i810-i830 need a physical address in system
       * memory from which to upload the cursor.  We get this from
       * the agpgart module using a special memory type.
       */

      size = HWCURSOR_SIZE;
      cursFlags = FROM_ANYWHERE | ALLOCATE_AT_TOP;
      if (pI830->CursorNeedsPhysical)
	 cursFlags |= NEED_PHYSICAL_ADDR;

      alloced = I830AllocVidMem(pScrn, &(pI830->CursorMem),
				&(pI830->StolenPool), size,
				GTT_PAGE_SIZE, flags | cursFlags);
      if (alloced < size) {
	 if (!dryrun) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Failed to allocate HW cursor space.\n");
	 }
      } else {
	 xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
			"%sAllocated %ld kB for HW cursor at 0x%lx", s,
			alloced / 1024, pI830->CursorMem.Start);
	 if (pI830->CursorNeedsPhysical)
	    xf86ErrorFVerb(verbosity, " (0x%08lx)", pI830->CursorMem.Physical);
	 xf86ErrorFVerb(verbosity, "\n");
      }
   }

#ifdef I830_XV
   AllocateOverlay(pScrn, flags);
#endif

   if (!pI830->NeedRingBufferLow)
      AllocateRingBuffer(pScrn, flags);

   /* Clear scratch info */
   memset(&(pI830->Scratch), 0, sizeof(pI830->Scratch));
   pI830->Scratch.Key = -1;

   if (!pI830->noAccel) {
      size = MAX_SCRATCH_BUFFER_SIZE;
      alloced = I830AllocVidMem(pScrn, &(pI830->Scratch), &(pI830->StolenPool),
				size, GTT_PAGE_SIZE,
				flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
      if (alloced < size) {
	 size = MIN_SCRATCH_BUFFER_SIZE;
         alloced = I830AllocVidMem(pScrn, &(pI830->Scratch),
				   &(pI830->StolenPool), size,
				   GTT_PAGE_SIZE,
				   flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
      }
      if (alloced < size) {
	 if (!dryrun) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Failed to allocate scratch buffer space\n");
	 }
	 return FALSE;
      }
      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		    "%sAllocated %ld kB for the scratch buffer at 0x%lx\n", s,
		    alloced / 1024, pI830->Scratch.Start);
   }
   return TRUE;
}

#ifndef ALLOCATE_ALL_BIOSMEM
#define ALLOCATE_ALL_BIOSMEM 1
#endif

void
I830ResetAllocations(ScrnInfoPtr pScrn, const int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->MemoryAperture.Start = pI830->StolenMemory.End;
   pI830->MemoryAperture.End = pI830->FbMapSize;
   pI830->MemoryAperture.Size = pI830->FbMapSize - pI830->StolenMemory.Size;
   pI830->StolenPool.Fixed = pI830->StolenMemory;
   pI830->StolenPool.Total = pI830->StolenMemory;
#if ALLOCATE_ALL_BIOSMEM
   if (pI830->overrideBIOSMemSize &&
       pI830->BIOSMemorySize > pI830->StolenMemory.Size) {
      pI830->StolenPool.Total.End = pI830->BIOSMemorySize;
      pI830->StolenPool.Total.Size = pI830->BIOSMemorySize;
   }
#endif
   pI830->StolenPool.Free = pI830->StolenPool.Total;
   pI830->FreeMemory = pI830->TotalVideoRam - pI830->StolenPool.Total.Size;
   pI830->allocatedMemory = 0;
}

long
I830GetExcessMemoryAllocations(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long allocated;

   allocated = pI830->StolenPool.Total.Size + pI830->allocatedMemory;
   if (allocated > pI830->TotalVideoRam)
      return allocated - pI830->TotalVideoRam;
   else
      return 0;
}

#ifdef XF86DRI
static unsigned long
GetBestTileAlignment(unsigned long size)
{
   unsigned long i;

   for (i = KB(512); i < size; i <<= 1)
      ;

   if (i > MB(64))
      i = MB(64);

   return i;
}

static unsigned int
myLog2(unsigned int n)
{
   unsigned int log2 = 1;

   while (n > 1) {
      n >>= 1;
      log2++;
   }
   return log2;
}

Bool
I830Allocate3DMemory(ScrnInfoPtr pScrn, const int flags)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned long size, alloced, align = 0;
   int i;
   Bool tileable;
   Bool dryrun = ((flags & ALLOCATE_DRY_RUN) != 0);
   int verbosity = dryrun ? 4 : 1;
   const char *s = dryrun ? "[dryrun] " : "";
   int lines;

   DPRINTF(PFX, "I830Allocate3DMemory\n");

   /* Back Buffer */
   memset(&(pI830->BackBuffer), 0, sizeof(pI830->BackBuffer));
   pI830->BackBuffer.Key = -1;
   tileable = !(flags & ALLOC_NO_TILING) &&
	      IsTileable(pScrn->displayWidth * pI830->cpp);
   if (tileable) {
      /* Make the height a multiple of the tile height (16) */
      lines = (pScrn->virtualY + 15) / 16 * 16;
   } else {
      lines = pScrn->virtualY;
   }

   size = ROUND_TO_PAGE(pScrn->displayWidth * lines * pI830->cpp);
   /*
    * Try to allocate on the best tile-friendly boundaries.
    */
   alloced = 0;
   if (tileable) {
      align = GetBestTileAlignment(size);
      for (align = GetBestTileAlignment(size); align >= KB(512); align >>= 1) {
	 alloced = I830AllocVidMem(pScrn, &(pI830->BackBuffer),
				   &(pI830->StolenPool), size, align,
				   flags | FROM_ANYWHERE | ALLOCATE_AT_TOP |
				   ALIGN_BOTH_ENDS);
	 if (alloced >= size)
	    break;
      }
   }
   if (alloced < size) {
      /* Give up on trying to tile */
      tileable = FALSE;
      size = ROUND_TO_PAGE(pScrn->displayWidth * pScrn->virtualY * pI830->cpp);
      align = GTT_PAGE_SIZE;
      alloced = I830AllocVidMem(pScrn, &(pI830->BackBuffer),
				&(pI830->StolenPool), size, align,
				flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
   }
   if (alloced < size) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to allocate back buffer space.\n");
      }
      return FALSE;
   }
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		  "%sAllocated %ld kB for the back buffer at 0x%lx.\n", s,
		  alloced / 1024, pI830->BackBuffer.Start);

   /* Depth Buffer -- same size as the back buffer */
   memset(&(pI830->DepthBuffer), 0, sizeof(pI830->DepthBuffer));
   pI830->DepthBuffer.Key = -1;
   /*
    * Try to allocate on the best tile-friendly boundaries.
    */
   alloced = 0;
   if (tileable) {
      /* Start with the previous align value. */
      for (; align >= KB(512); align >>= 1) {
	 alloced = I830AllocVidMem(pScrn, &(pI830->DepthBuffer),
				   &(pI830->StolenPool), size, align,
				   flags | FROM_ANYWHERE | ALLOCATE_AT_TOP |
				   ALIGN_BOTH_ENDS);
	 if (alloced >= size)
	    break;
      }
   }
   if (alloced < size) {
      /* Give up on trying to tile */
      tileable = FALSE;
      size = ROUND_TO_PAGE(pScrn->displayWidth * pScrn->virtualY * pI830->cpp);
      align = GTT_PAGE_SIZE;
      alloced = I830AllocVidMem(pScrn, &(pI830->DepthBuffer),
				&(pI830->StolenPool), size, align,
				flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
   }
   if (alloced < size) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to allocate depth buffer space.\n");
      }
      return FALSE;
   }
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		  "%sAllocated %ld kB for the depth buffer at 0x%lx.\n", s,
		  alloced / 1024, pI830->DepthBuffer.Start);

   /* Space for logical context.  32k is fine for right now. */
   memset(&(pI830->ContextMem), 0, sizeof(pI830->ContextMem));
   pI830->ContextMem.Key = -1;
   size = KB(32);
   alloced = I830AllocVidMem(pScrn, &(pI830->ContextMem),
			     &(pI830->StolenPool), size, GTT_PAGE_SIZE,
			     flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
   if (alloced < size) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to allocate logical context space.\n");
      }
      return FALSE;
   }
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		  "%sAllocated %ld kB for the logical context at 0x%lx.\n", s,
		  alloced / 1024, pI830->ContextMem.Start);

   /*
    * Space for DMA buffers, only if there's enough free for at least 1MB
    * of texture space.
    */
   memset(&(pI830->BufferMem), 0, sizeof(pI830->BufferMem));
   pI830->BufferMem.Key = -1;
   /* This should already be a page multiple */
   size = I830_DMA_BUF_NR * I830_DMA_BUF_SZ;
   if (dryrun || (GetFreeSpace(pScrn) >= size + MB(1))) {
      alloced = I830AllocVidMem(pScrn, &(pI830->BufferMem),
				&(pI830->StolenPool), size,
				GTT_PAGE_SIZE,
				flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
      if (alloced < size) {
	 if (!dryrun) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Failed to allocate DMA buffer space.\n");
	 }
	 return FALSE;
      }
      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		     "%sAllocated %ld kB for the DMA buffers at 0x%lx.\n", s,
		     alloced / 1024, pI830->BufferMem.Start);
   } else {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Not enough free space for DMA buffers.\n");
      }
      return FALSE;
   }

   /* Allocate the remaining space for textures. */
   memset(&(pI830->TexMem), 0, sizeof(pI830->TexMem));
   pI830->TexMem.Key = -1;
   size = GetFreeSpace(pScrn);
   if (dryrun && (size < MB(1)))
      size = MB(1);
   i = myLog2(size / I830_NR_TEX_REGIONS);
   if (i < I830_LOG_MIN_TEX_REGION_SIZE)
      i = I830_LOG_MIN_TEX_REGION_SIZE;
   pI830->TexGranularity = i;
   /* Truncate size */
   size >>= i;
   size <<= i;
   if (size < KB(512)) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Less than %ld kBytes for texture space.\n", size / 1024);
      }
      return FALSE;
   }
   alloced = I830AllocVidMem(pScrn, &(pI830->TexMem),
			     &(pI830->StolenPool), size, GTT_PAGE_SIZE,
			     flags | FROM_ANYWHERE | ALLOCATE_AT_TOP);
   if (alloced < size) {
      if (!dryrun) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to allocate texture space.\n");
      }
      return FALSE;
   }
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		  "%sAllocated %ld kB for textures at 0x%lx\n", s,
		  alloced / 1024, pI830->TexMem.Start);

   return TRUE;
}
#endif

/* Allocate pool space that isn't pre-allocated */
Bool
I830DoPoolAllocation(ScrnInfoPtr pScrn, I830MemPool *pool)
{
   I830Ptr pI830 = I830PTR(pScrn);

   DPRINTF(PFX, "I830DoPoolAllocation\n");

   if (!pool)
      return FALSE;

   /*
    * Sanity check: there shouldn't be an allocation required when
    * there is only stolen memory.
    */
   if (pI830->StolenOnly && (pool->Total.Size > pool->Fixed.Size)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "I830DoPoolAllocation(): pool size is greater than the "
		 "preallocated size,\n\t"
		 "and there is no allocatable memory.\n");
      return FALSE;
   }

   if (pool->Total.Size > pool->Fixed.Size) {
      pool->Allocated.Size = pool->Total.Size - pool->Fixed.Size;
      pool->Allocated.Key =
	    xf86AllocateGARTMemory(pScrn->scrnIndex, pool->Allocated.Size,
				   0, NULL);
      if (pool->Allocated.Key == -1) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Pool allocation failed\n");
	 return FALSE;
      }
      pool->Allocated.Start = pool->Fixed.End;
      pool->Allocated.End = pool->Total.Size;
      pool->Allocated.Offset = pool->Allocated.Start;
   } else
      pool->Allocated.Key = -1;
   return TRUE;
}

static unsigned long topOfMem = 0;

/*
 * These modify the way memory is positioned within the aperture.
 *
 * By default, memory allocated from the bottom or specifically within
 * the pool at the bottom gets allocated from the "stolen pool", which is
 * actually the stolen memory plus any extra allocated to make it a larger
 * contiguous region.  Memory allocated from the AGP is allocated top-down
 * from the end of the aperture space.  Memory allocated "from top" defaults
 * to AGP if there is enough "free space".  The total allocation (stolen +
 * extra) doesn't exceed the orignal pScrn->videoRam amount (this isn't true
 * when memory allocated from AGP gets moved into the pool by one of the
 * following options.
 *
 * XXX Write a better description.
 *
 */
#define PACK_RANGES 0
#define POOL_RANGES 0

static Bool
FixOffset(ScrnInfoPtr pScrn, I830MemRange *mem)
{
#if POOL_RANGES
   I830Ptr pI830 = I830PTR(pScrn);
#endif

   if (!mem)
      return FALSE;

   if (mem->Pool && mem->Key == -1 && mem->Start < 0) {
      mem->Start = mem->Pool->Total.End + mem->Start;
      mem->End = mem->Start + mem->Size;
   }
#if PACK_RANGES
   /*
    * Map AGP-allocated areas at the top of the stolen area, resulting in
    * a contiguous region in the aperture.  Normally most AGP-allocated areas
    * will be at the top of the aperture, making alignment requirements
    * easier to achieve.  This optin is primarily for debugging purposes,
    * and using this option can break any special alignment requirements.
    */
   if (!mem->Pool && mem->Start != 0 && mem->Key != -1 && mem->Physical == 0 &&
	mem->Offset != 0) {
      long diff;
      if (mem->Offset != mem->Start)
	 ErrorF("mem %p, Offset != Start\n", mem);
      diff = mem->Offset - topOfMem;
      mem->Start -= diff;
      mem->End -= diff;
      mem->Offset -= diff;
      topOfMem += mem->Size;
   }
#elif POOL_RANGES
   /*
    * Move AGP-allocated regions (that don't need a physical address) into
    * the pre-allocated pool when there's enough space to do so.  Note: the
    * AGP-allocated areas aren't freed.  This option is primarily for
    * debugging purposes, and using it can break any special alignment
    * requirements.
    */
   if (!mem->Pool && mem->Start >= pI830->StolenPool.Free.End &&
       mem->Key != -1 && mem->Physical == 0 && mem->Offset != 0 &&
       pI830->StolenPool.Free.Size >= mem->Size) {
      long diff;
      if (mem->Offset != mem->Start)
	 ErrorF("mem %p, Offset != Start\n", mem);
      diff = mem->Offset - pI830->StolenPool.Free.Start;
      mem->Start -= diff;
      mem->End -= diff;
      mem->Offset -= diff;
      mem->Key = -1;
      pI830->StolenPool.Free.Start += mem->Size;
      pI830->StolenPool.Free.Size -= mem->Size;
   }
#endif
   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "%p: Memory at offset 0x%08lx, size %ld kBytes\n", (void *)mem,
	      mem->Start, mem->Size / 1024);
   return TRUE;
}

Bool
I830FixupOffsets(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   DPRINTF(PFX, "I830FixupOffsets\n");

   topOfMem = pI830->StolenPool.Total.End;
   FixOffset(pScrn, &(pI830->FrontBuffer));
   FixOffset(pScrn, &(pI830->CursorMem));
   FixOffset(pScrn, &(pI830->LpRing.mem));
   FixOffset(pScrn, &(pI830->Scratch));
#ifdef I830_XV
   if (pI830->XvEnabled) {
      FixOffset(pScrn, &(pI830->OverlayMem));
   }
#endif
#ifdef XF86DRI
   if (pI830->directRenderingEnabled) {
      FixOffset(pScrn, &(pI830->BackBuffer));
      FixOffset(pScrn, &(pI830->DepthBuffer));
      FixOffset(pScrn, &(pI830->ContextMem));
      FixOffset(pScrn, &(pI830->BufferMem));
      FixOffset(pScrn, &(pI830->TexMem));
   }
#endif
   return TRUE;
}

#ifdef XF86DRI
/* Tiled memory is good... really, really good...
 *
 * Need to make it less likely that we miss out on this - probably
 * need to move the frontbuffer away from the 'guarenteed' alignment
 * of the first memory segment, or perhaps allocate a discontigous
 * framebuffer to get more alignment 'sweet spots'.
 */
static void
SetFence(ScrnInfoPtr pScrn, int nr, unsigned int start, unsigned int pitch,
         unsigned int size)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830RegPtr i830Reg = &pI830->ModeReg;
   CARD32 val;
   CARD32 fence_mask = 0;

   DPRINTF(PFX, "SetFence: %d, 0x%08x, %d, %d kByte\n",
	   nr, start, pitch, size / 1024);

   if (nr < 0 || nr > 7) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "SetFence: fence %d out of range\n", nr);
      return;
   }

   i830Reg->Fence[nr] = 0;

   fence_mask = ~I830_FENCE_START_MASK;

   if (start & fence_mask) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "SetFence: %d: start (0x%08x) is not 512k aligned\n",
		 nr, start);
      return;
   }

   if (start % size) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "SetFence: %d: start (0x%08x) is not size (%dk) aligned\n",
		 nr, start, size / 1024);
      return;
   }

   if (pitch & 127) {
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "SetFence: %d: pitch (%d) not a multiple of 128 bytes\n",
		 nr, pitch);
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
   case MB(64):
      val |= FENCE_SIZE_64M;
      break;
   default:
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "SetFence: %d: illegal size (%d kByte)\n", nr, size / 1024);
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
   case 64:
      val |= FENCE_PITCH_64;
      break;
   default:
      xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
		 "SetFence: %d: illegal pitch (%d)\n", nr, pitch);
      return;
   }

   i830Reg->Fence[nr] = val;
}

static Bool
MakeTiles(ScrnInfoPtr pScrn, I830MemRange *pMem)
{
   I830Ptr pI830 = I830PTR(pScrn);
   int pitch, ntiles, i;
   static int nextTile = 0;
   static int tileGeneration = -1;

#if 0
   /* Hack to "improve" the alignment of the front buffer.
    */
   while (!(pMem->Start & ~pMem->Alignment) && pMem->Alignment < 0x00400000 )
      pMem->Alignment <<= 1;
#endif

   if (tileGeneration != serverGeneration) {
      tileGeneration = serverGeneration;
      nextTile = 0;
   }

   pitch = pScrn->displayWidth * pI830->cpp;
   /*
    * Simply try to break the region up into at most four pieces of size
    * equal to the alignment.
    */
   ntiles = ROUND_TO(pMem->Size, pMem->Alignment) / pMem->Alignment;
   if (ntiles >= 4) {
      return FALSE;
   }

   for (i = 0; i < ntiles; i++, nextTile++) {
      SetFence(pScrn, nextTile, pMem->Start + i * pMem->Alignment,
	       pitch, pMem->Alignment);
   }
   return TRUE;
}

void
I830SetupMemoryTiling(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   /* We currently only attempt to tile the back and depth buffers. */
   if (!pI830->directRenderingEnabled)
      return;

   if (!IsTileable(pScrn->displayWidth * pI830->cpp)) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "I830SetupMemoryTiling: Not tileable 0x%x\n",
		 pScrn->displayWidth * pI830->cpp);
      pI830->allowPageFlip = FALSE;
      return;
   }

   if (pI830->allowPageFlip) {
      if (pI830->allowPageFlip && pI830->FrontBuffer.Alignment >= KB(512)) {
	 if (MakeTiles(pScrn, &(pI830->FrontBuffer))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Activating tiled memory for the FRONT buffer\n");
	 } else {
	    pI830->allowPageFlip = FALSE;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "MakeTiles failed for the FRONT buffer\n");
	 }
      } else {
	 pI830->allowPageFlip = FALSE;
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "Alignment bad for the FRONT buffer\n");
      }
   }

   /*
    * We tried to get the best alignment during the allocation.  Check
    * the alignment values to tell.  If well-aligned allocations were
    * successful, the address range reserved is a multiple of the align
    * value.
    */
   if (pI830->BackBuffer.Alignment >= KB(512)) {
      if (MakeTiles(pScrn, &(pI830->BackBuffer))) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Activating tiled memory for the back buffer.\n");
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "MakeTiles failed for the back buffer.\n");
	 pI830->allowPageFlip = FALSE;
      }
   }

   if (pI830->DepthBuffer.Alignment >= KB(512)) {
      if (MakeTiles(pScrn, &(pI830->DepthBuffer))) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Activating tiled memory for the depth buffer.\n");
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "MakeTiles failed for the back buffer.\n");
      }
   }

}
#endif /* XF86DRI */

static Bool
BindMemRange(ScrnInfoPtr pScrn, I830MemRange *mem)
{
   if (!mem)
      return FALSE;

   if (mem->Key == -1)
      return TRUE;

   return xf86BindGARTMemory(pScrn->scrnIndex, mem->Key, mem->Offset);
}

Bool
I830BindGARTMemory(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   DPRINTF(PFX,
	   "I830BindGARTMemory: StolenOnly is %s, pI830->GttBound is %s\n",
	   BOOLTOSTRING(pI830->StolenOnly), BOOLTOSTRING(pI830->GttBound));

   if (pI830->StolenOnly == TRUE)
      return TRUE;

   if (xf86AgpGARTSupported() && !pI830->GttBound) {
      if (!xf86AcquireGART(pScrn->scrnIndex))
	 return FALSE;

#if REMAP_RESERVED
      /* Rebind the pre-allocated region. */
      BindMemRange(pScrn, &(pI830->Dummy));
#endif

      if (!BindMemRange(pScrn, &(pI830->StolenPool.Allocated)))
	 return FALSE;
      if (!BindMemRange(pScrn, &(pI830->FrontBuffer)))
	 return FALSE;
      if (!BindMemRange(pScrn, &(pI830->CursorMem)))
	 return FALSE;
      if (!BindMemRange(pScrn, &(pI830->LpRing.mem)))
	 return FALSE;
      if (!BindMemRange(pScrn, &(pI830->Scratch)))
	 return FALSE;
#ifdef I830_XV
      if (!BindMemRange(pScrn, &(pI830->OverlayMem)))
	 return FALSE;
#endif
#ifdef XF86DRI
      if (pI830->directRenderingEnabled) {
	 if (!BindMemRange(pScrn, &(pI830->BackBuffer)))
	    return FALSE;
	 if (!BindMemRange(pScrn, &(pI830->DepthBuffer)))
	    return FALSE;
	 if (!BindMemRange(pScrn, &(pI830->ContextMem)))
	    return FALSE;
	 if (!BindMemRange(pScrn, &(pI830->BufferMem)))
	    return FALSE;
	 if (!BindMemRange(pScrn, &(pI830->TexMem)))
	    return FALSE;
      }
#endif
      pI830->GttBound = 1;
   }

   return TRUE;
}

static Bool
UnbindMemRange(ScrnInfoPtr pScrn, I830MemRange *mem)
{
   if (!mem)
      return FALSE;

   if (mem->Key == -1)
      return TRUE;

   return xf86UnbindGARTMemory(pScrn->scrnIndex, mem->Key);
}


Bool
I830UnbindGARTMemory(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   DPRINTF(PFX,
	   "I830UnbindGARTMemory: StolenOnly is %s, pI830->GttBound is %s\n",
	   BOOLTOSTRING(pI830->StolenOnly), BOOLTOSTRING(pI830->GttBound));

   if (pI830->StolenOnly == TRUE)
      return TRUE;

   if (xf86AgpGARTSupported() && pI830->GttBound) {

#if REMAP_RESERVED
      /* "unbind" the pre-allocated region. */
      UnbindMemRange(pScrn, &(pI830->Dummy));
#endif

      if (!UnbindMemRange(pScrn, &(pI830->StolenPool.Allocated)))
	 return FALSE;
      if (!UnbindMemRange(pScrn, &(pI830->FrontBuffer)))
	 return FALSE;
      if (!UnbindMemRange(pScrn, &(pI830->CursorMem)))
	 return FALSE;
      if (!UnbindMemRange(pScrn, &(pI830->LpRing.mem)))
	 return FALSE;
      if (!UnbindMemRange(pScrn, &(pI830->Scratch)))
	 return FALSE;
#ifdef I830_XV
      if (!UnbindMemRange(pScrn, &(pI830->OverlayMem)))
	 return FALSE;
#endif
#ifdef XF86DRI
      if (pI830->directRenderingEnabled) {
	 if (!UnbindMemRange(pScrn, &(pI830->BackBuffer)))
	    return FALSE;
	 if (!UnbindMemRange(pScrn, &(pI830->DepthBuffer)))
	    return FALSE;
	 if (!UnbindMemRange(pScrn, &(pI830->ContextMem)))
	    return FALSE;
	 if (!UnbindMemRange(pScrn, &(pI830->BufferMem)))
	    return FALSE;
	 if (!UnbindMemRange(pScrn, &(pI830->TexMem)))
	    return FALSE;
      }
#endif
      if (!xf86ReleaseGART(pScrn->scrnIndex))
	 return FALSE;

      pI830->GttBound = 0;
   }

   return TRUE;
}

long
I830CheckAvailableMemory(ScrnInfoPtr pScrn)
{
   AgpInfoPtr agpinf;
   int maxPages;

   if (!xf86AgpGARTSupported() ||
       !xf86AcquireGART(pScrn->scrnIndex) ||
       (agpinf = xf86GetAGPInfo(pScrn->scrnIndex)) == NULL ||
       !xf86ReleaseGART(pScrn->scrnIndex))
      return -1;

   maxPages = agpinf->totalPages - agpinf->usedPages;
   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "%s: %d kB available\n",
		  "I830CheckAvailableMemory", maxPages * 4);

   return maxPages * 4;
}
