/* $XFree86: xc/lib/GL/mesa/src/drv/r128/r128_texmem.c,v 1.1 2002/02/22 21:44:58 dawes Exp $ */
/**************************************************************************

Copyright 1999, 2000 ATI Technologies Inc. and Precision Insight, Inc.,
                                               Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ATI, PRECISION INSIGHT AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 *   Kevin E. Martin <martin@valinux.com>
 *   Brian Paul <brianp@valinux.com>
 */

#include "r128_context.h"
#include "r128_state.h"
#include "r128_ioctl.h"
#include "r128_vb.h"
#include "r128_tris.h"
#include "r128_tex.h"

#include "context.h"
#include "macros.h"
#include "mmath.h"
#include "simple_list.h"
#include "texformat.h"
#include "mem.h"

#define TEX_0	1
#define TEX_1	2


/* Destroy hardware state associated with texture `t'.
 */
void r128DestroyTexObj( r128ContextPtr rmesa, r128TexObjPtr t )
{
#if ENABLE_PERF_BOXES
   /* Bump the performace counter */
   rmesa->c_textureSwaps++;
#endif
   if ( !t ) return;

   if ( t->memBlock ) {
      mmFreeMem( t->memBlock );
      t->memBlock = NULL;
   }

   if ( t->tObj )
      t->tObj->DriverData = NULL;

   if ( t->bound & TEX_0 ) rmesa->CurrentTexObj[0] = NULL;
   if ( t->bound & TEX_1 ) rmesa->CurrentTexObj[1] = NULL;

   remove_from_list( t );
   FREE( t );
}

/* Keep track of swapped out texture objects.
 */
void r128SwapOutTexObj( r128ContextPtr rmesa, r128TexObjPtr t )
{
#if ENABLE_PERF_BOXES
   /* Bump the performace counter */
   rmesa->c_textureSwaps++;
#endif
   if ( t->memBlock ) {
      mmFreeMem( t->memBlock );
      t->memBlock = NULL;
   }

   t->dirty_images = ~0;
   move_to_tail( &rmesa->SwappedOut, t );
}

/* Print out debugging information about texture LRU.
 */
void r128PrintLocalLRU( r128ContextPtr rmesa, int heap )
{
   r128TexObjPtr t;
   int sz = 1 << (rmesa->r128Screen->logTexGranularity[heap]);

   fprintf( stderr, "\nLocal LRU, heap %d:\n", heap );

   foreach ( t, &rmesa->TexObjList[heap] ) {
      if ( !t->tObj ) {
	 fprintf( stderr, "Placeholder %d at 0x%x sz 0x%x\n",
		  t->memBlock->ofs / sz,
		  t->memBlock->ofs,
		  t->memBlock->size );
      } else {
	 fprintf( stderr, "Texture (bound %d) at 0x%x sz 0x%x\n",
		  t->bound,
		  t->memBlock->ofs,
		  t->memBlock->size );
      }
   }

   fprintf( stderr, "\n" );
}

void r128PrintGlobalLRU( r128ContextPtr rmesa, int heap )
{
   r128_tex_region_t *list = rmesa->sarea->texList[heap];
   int i, j;

   fprintf( stderr, "\nGlobal LRU, heap %d list %p:\n", heap, list );

   for ( i = 0, j = R128_NR_TEX_REGIONS ; i < R128_NR_TEX_REGIONS ; i++ ) {
      fprintf( stderr, "list[%d] age %d next %d prev %d\n",
	       j, list[j].age, list[j].next, list[j].prev );
      j = list[j].next;
      if ( j == R128_NR_TEX_REGIONS ) break;
   }

   if ( j != R128_NR_TEX_REGIONS ) {
      fprintf( stderr, "Loop detected in global LRU\n" );
      for ( i = 0 ; i < R128_NR_TEX_REGIONS ; i++ ) {
	 fprintf( stderr, "list[%d] age %d next %d prev %d\n",
		  i, list[i].age, list[i].next, list[i].prev );
      }
   }

   fprintf( stderr, "\n" );
}

/* Reset the global texture LRU.
 */
static void r128ResetGlobalLRU( r128ContextPtr rmesa, int heap )
{
   r128_tex_region_t *list = rmesa->sarea->texList[heap];
   int sz = 1 << rmesa->r128Screen->logTexGranularity[heap];
   int i;

   /* (Re)initialize the global circular LRU list.  The last element in
    * the array (R128_NR_TEX_REGIONS) is the sentinal.  Keeping it at
    * the end of the array allows it to be addressed rationally when
    * looking up objects at a particular location in texture memory.
    */
   for ( i = 0 ; (i+1) * sz <= rmesa->r128Screen->texSize[heap] ; i++ ) {
      list[i].prev = i-1;
      list[i].next = i+1;
      list[i].age = 0;
   }

   i--;
   list[0].prev = R128_NR_TEX_REGIONS;
   list[i].prev = i-1;
   list[i].next = R128_NR_TEX_REGIONS;
   list[R128_NR_TEX_REGIONS].prev = i;
   list[R128_NR_TEX_REGIONS].next = 0;
   rmesa->sarea->texAge[heap] = 0;
}

/* Update the local and glock texture LRUs.
 */
void r128UpdateTexLRU( r128ContextPtr rmesa, r128TexObjPtr t )
{
   int heap = t->heap;
   r128_tex_region_t *list = rmesa->sarea->texList[heap];
   int log2sz = rmesa->r128Screen->logTexGranularity[heap];
   int start = t->memBlock->ofs >> log2sz;
   int end = (t->memBlock->ofs + t->memBlock->size - 1) >> log2sz;
   int i;

   rmesa->lastTexAge[heap] = ++rmesa->sarea->texAge[heap];

   if ( !t->memBlock ) {
      fprintf( stderr, "no memblock\n\n" );
      return;
   }

   /* Update our local LRU */
   move_to_head( &rmesa->TexObjList[heap], t );

   /* Update the global LRU */
   for ( i = start ; i <= end ; i++ ) {
      list[i].in_use = 1;
      list[i].age = rmesa->lastTexAge[heap];

      /* remove_from_list(i) */
      list[(CARD32)list[i].next].prev = list[i].prev;
      list[(CARD32)list[i].prev].next = list[i].next;

      /* insert_at_head(list, i) */
      list[i].prev = R128_NR_TEX_REGIONS;
      list[i].next = list[R128_NR_TEX_REGIONS].next;
      list[(CARD32)list[R128_NR_TEX_REGIONS].next].prev = i;
      list[R128_NR_TEX_REGIONS].next = i;
   }

   if ( 0 ) {
      r128PrintGlobalLRU( rmesa, t->heap );
      r128PrintLocalLRU( rmesa, t->heap );
   }
}

/* Update our notion of what textures have been changed since we last
 * held the lock.  This pertains to both our local textures and the
 * textures belonging to other clients.  Keep track of other client's
 * textures by pushing a placeholder texture onto the LRU list -- these
 * are denoted by (tObj == NULL).
 */
static void r128TexturesGone( r128ContextPtr rmesa, int heap,
			      int offset, int size, int in_use )
{
   r128TexObjPtr t, tmp;

   foreach_s ( t, tmp, &rmesa->TexObjList[heap] ) {
      if ( t->memBlock->ofs >= offset + size ||
	   t->memBlock->ofs + t->memBlock->size <= offset )
	 continue;

      /* It overlaps - kick it out.  Need to hold onto the currently
       * bound objects, however.
       */
      if ( t->bound ) {
	 r128SwapOutTexObj( rmesa, t );
      } else {
	 r128DestroyTexObj( rmesa, t );
      }
   }

   if ( in_use ) {
      t = (r128TexObjPtr) CALLOC( sizeof(*t) );
      if ( !t ) return;

      t->memBlock = mmAllocMem( rmesa->texHeap[heap], size, 0, offset );
      if ( !t->memBlock ) {
	 fprintf( stderr, "Couldn't alloc placeholder sz %x ofs %x\n",
		  (int)size, (int)offset );
	 mmDumpMemInfo( rmesa->texHeap[heap] );
	 return;
      }
      insert_at_head( &rmesa->TexObjList[heap], t );
   }
}

/* Update our client's shared texture state.  If another client has
 * modified a region in which we have textures, then we need to figure
 * out which of our textures has been removed, and update our global
 * LRU.
 */
void r128AgeTextures( r128ContextPtr rmesa, int heap )
{
   R128SAREAPrivPtr sarea = rmesa->sarea;

   if ( sarea->texAge[heap] != rmesa->lastTexAge[heap] ) {
      int sz = 1 << rmesa->r128Screen->logTexGranularity[heap];
      int nr = 0;
      int idx;

      /* Have to go right round from the back to ensure stuff ends up
       * LRU in our local list...  Fix with a cursor pointer.
       */
      for ( idx = sarea->texList[heap][R128_NR_TEX_REGIONS].prev ;
	    idx != R128_NR_TEX_REGIONS && nr < R128_NR_TEX_REGIONS ;
	    idx = sarea->texList[heap][idx].prev, nr++ )
      {
	 /* If switching texturing schemes, then the SAREA might not
	  * have been properly cleared, so we need to reset the
	  * global texture LRU.
	  */
	 if ( idx * sz > rmesa->r128Screen->texSize[heap] ) {
	    nr = R128_NR_TEX_REGIONS;
	    break;
	 }

	 if ( sarea->texList[heap][idx].age > rmesa->lastTexAge[heap] ) {
	    r128TexturesGone( rmesa, heap, idx * sz, sz,
			      sarea->texList[heap][idx].in_use );
	 }
      }

      /* If switching texturing schemes, then the SAREA might not
       * have been properly cleared, so we need to reset the
       * global texture LRU.
       */
      if ( nr == R128_NR_TEX_REGIONS ) {
	 r128TexturesGone( rmesa, heap, 0,
			   rmesa->r128Screen->texSize[heap], 0 );
	 r128ResetGlobalLRU( rmesa, heap );
      }

      if ( 0 ) {
	 r128PrintGlobalLRU( rmesa, heap );
	 r128PrintLocalLRU( rmesa, heap );
      }

      rmesa->dirty |= (R128_UPLOAD_CONTEXT |
		       R128_UPLOAD_TEX0IMAGES |
		       R128_UPLOAD_TEX1IMAGES);
      rmesa->lastTexAge[heap] = sarea->texAge[heap];
   }
}


/* Upload the texture image associated with texture `t' at level `level'
 * at the address relative to `start'.
 */
static void r128UploadSubImage( r128ContextPtr rmesa,
				r128TexObjPtr t, int level,
				int x, int y, int width, int height )
{
   struct gl_texture_image *image;
   int texelsPerDword = 0;
   int imageWidth, imageHeight;
   int remaining, rows;
   int format, dwords;
   CARD32 pitch, offset;
   int i;

   /* Ensure we have a valid texture to upload */
   if ( ( level < 0 ) || ( level > R128_MAX_TEXTURE_LEVELS ) )
      return;

   image = t->tObj->Image[level];
   if ( !image )
      return;

   switch ( image->TexFormat->TexelBytes ) {
   case 1: texelsPerDword = 4; break;
   case 2: texelsPerDword = 2; break;
   case 4: texelsPerDword = 1; break;
   }

#if 1
   /* FIXME: The subimage index calcs are wrong... */
   x = 0;
   y = 0;
   width = image->Width;
   height = image->Height;
#endif

   imageWidth  = image->Width;
   imageHeight = image->Height;

   format = t->textureFormat >> 16;

   /* The texel upload routines have a minimum width, so force the size
    * if needed.
    */
   if ( imageWidth < texelsPerDword ) {
      int factor;

      factor = texelsPerDword / imageWidth;
      imageWidth = texelsPerDword;
      imageHeight /= factor;
      if ( imageHeight == 0 ) {
	 /* In this case, the texel converter will actually walk a
	  * texel or two off the end of the image, but normal malloc
	  * alignment should prevent it from ever causing a fault.
	  */
	 imageHeight = 1;
      }
   }

   /* We can't upload to a pitch less than 8 texels so we will need to
    * linearly upload all modified rows for textures smaller than this.
    * This makes the x/y/width/height different for the blitter and the
    * texture walker.
    */
   if ( imageWidth >= 8 ) {
      /* The texture walker and the blitter look identical */
      pitch = imageWidth >> 3;
   } else {
      int factor;
      int y2;
      int start, end;

      start = (y * imageWidth) & ~7;
      end = (y + height) * imageWidth;

      if ( end - start < 8 ) {
	 /* Handle the case where the total number of texels
	  * uploaded is < 8.
	  */
	 x = 0;
	 y = start / 8;
	 width = end - start;
	 height = 1;
      } else {
	 /* Upload some number of full 8 texel blit rows */
	 factor = 8 / imageWidth;

	 y2 = y + height - 1;
	 y /= factor;
	 y2 /= factor;

	 x = 0;
	 width = 8;
	 height = y2 - y + 1;
      }

      /* Fixed pitch of 8 */
      pitch = 1;
   }

   dwords = width * height / texelsPerDword;
   offset = t->bufAddr + t->image[level - t->firstLevel].offset;

#if ENABLE_PERF_BOXES
   /* Bump the performace counter */
   rmesa->c_textureBytes += (dwords << 2);
#endif

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "r128UploadSubImage: %d,%d of %d,%d at %d,%d\n",
	       width, height, image->Width, image->Height, x, y );
      fprintf( stderr, "          blit ofs: 0x%07x pitch: 0x%x dwords: %d "
	       "level: %d format: %x\n",
	       (GLuint)offset, (GLuint)pitch, dwords, level, format );
   }

   /* Subdivide the texture if required */
   if ( dwords <= R128_BUFFER_MAX_DWORDS / 2 ) {
      rows = height;
   } else {
      rows = (R128_BUFFER_MAX_DWORDS * texelsPerDword) / (2 * width);
   }

   for ( i = 0, remaining = height ;
	 remaining > 0 ;
	 remaining -= rows, y += rows, i++ )
   {
      CARD32 *dst;
      drmBufPtr buffer;

      height = MIN2(remaining, rows);

      /* Grab the indirect buffer for the texture blit */
      buffer = r128GetBufferLocked( rmesa );

      dst = (CARD32 *)((char *)buffer->address + R128_HOSTDATA_BLIT_OFFSET);

      assert(image->Data);

      /* Copy the next chunck of the texture image into the blit buffer */
      {
         const GLubyte *src = (const GLubyte *) image->Data +
            (y * image->Width + x) * image->TexFormat->TexelBytes;
         const GLuint bytes = width * height * image->TexFormat->TexelBytes;
         memcpy(dst, src, bytes);
      }

      r128FireBlitLocked( rmesa, buffer,
			  offset, pitch, format,
			  x, y, width, height );
   }

   rmesa->new_state |= R128_NEW_CONTEXT;
   rmesa->dirty |= R128_UPLOAD_CONTEXT | R128_UPLOAD_MASKS;
}


/* Upload the texture images associated with texture `t'.  This might
 * require removing our own and/or other client's texture objects to
 * make room for these images.
 */
/* NOTE: This function is only called while holding the hardware lock */
void r128UploadTexImages( r128ContextPtr rmesa, r128TexObjPtr t )
{
   const GLint numLevels = t->lastLevel - t->firstLevel + 1;
   GLint i, heap;

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %p, %p )\n",
	       __FUNCTION__, rmesa->glCtx, t );
   }

   assert(t);

   /* Choose the heap appropriately */
   heap = t->heap = R128_CARD_HEAP;
   if ( !rmesa->r128Screen->IsPCI &&
	t->totalSize > rmesa->r128Screen->texSize[heap] ) {
      heap = t->heap = R128_AGP_HEAP;
   }

   /* Do we need to eject LRU texture objects? */
   if ( !t->memBlock ) {
      /* Allocate a memory block on a 4k boundary (1<<12 == 4096) */
      t->memBlock = mmAllocMem( rmesa->texHeap[heap], t->totalSize, 12, 0 );

      /* Try AGP before kicking anything out of local mem */
      if ( !t->memBlock && heap == R128_CARD_HEAP ) {
	 t->memBlock = mmAllocMem( rmesa->texHeap[R128_AGP_HEAP],
				   t->totalSize, 12, 0 );

	 if ( t->memBlock )
	    heap = t->heap = R128_AGP_HEAP;
      }

      /* Kick out textures until the requested texture fits */
      while ( !t->memBlock ) {
	 if ( rmesa->TexObjList[heap].prev->bound ) {
	    fprintf( stderr,
		     "r128UploadTexImages: ran into bound texture\n" );
	    return;
	 }
	 if ( rmesa->TexObjList[heap].prev == &rmesa->TexObjList[heap] ) {
	    if ( rmesa->r128Screen->IsPCI ) {
	       fprintf( stderr, "r128UploadTexImages: upload texture "
			"failure on local texture heaps, sz=%d\n",
			t->totalSize );
	       return;
	    } else if ( heap == R128_CARD_HEAP ) {
	       heap = t->heap = R128_AGP_HEAP;
	       continue;
	    } else {
	       fprintf( stderr, "r128UploadTexImages: upload texture "
			"failure on both local and AGP texture heaps, "
			"sz=%d\n",
			t->totalSize );
	       return;
	    }
	 }

	 r128SwapOutTexObj( rmesa, rmesa->TexObjList[heap].prev );

	 t->memBlock = mmAllocMem( rmesa->texHeap[heap],
				   t->totalSize, 12, 0 );
      }

      /* Set the base offset of the texture image */
      t->bufAddr = rmesa->r128Screen->texOffset[heap] + t->memBlock->ofs;

      /* Set texture offsets for each mipmap level */
      if ( t->setup.tex_cntl & R128_MIP_MAP_DISABLE ) {
	 for ( i = 0 ; i < R128_MAX_TEXTURE_LEVELS ; i++ ) {
	    t->setup.tex_offset[i] = t->bufAddr;
	 }
      } else {
         for ( i = 0; i < numLevels; i++ ) {
            const int j = numLevels - i - 1;
            t->setup.tex_offset[j] = t->bufAddr + t->image[i].offset;
         }
      }

      /* Force loading the new state into the hardware */
      switch ( t->bound ) {
      case 1:
	 rmesa->dirty |= R128_UPLOAD_CONTEXT | R128_UPLOAD_TEX0;
	 break;

      case 2:
	 rmesa->dirty |= R128_UPLOAD_CONTEXT | R128_UPLOAD_TEX1;
	 break;

      default:
	 return;
      }
   }

   /* Let the world know we've used this memory recently */
   r128UpdateTexLRU( rmesa, t );

   /* Upload any images that are new */
   if ( t->dirty_images ) {
      for ( i = 0 ; i < numLevels; i++ ) {
         const GLint j = t->firstLevel + i;  /* the texObj's level */
	 if ( t->dirty_images & (1 << j) ) {
	    r128UploadSubImage( rmesa, t, j, 0, 0,
				t->image[i].width, t->image[i].height );
	 }
      }

      rmesa->setup.tex_cntl_c |= R128_TEX_CACHE_FLUSH;
      rmesa->dirty |= R128_UPLOAD_CONTEXT;
      t->dirty_images = 0;
   }
}
