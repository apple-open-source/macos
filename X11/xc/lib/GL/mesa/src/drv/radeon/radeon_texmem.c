/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_texmem.c,v 1.7 2002/12/16 16:18:59 dawes Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     VA Linux Systems Inc., Fremont, California.

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
ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

#include "radeon_context.h"
#include "radeon_tex.h"

#include "context.h"
#include "simple_list.h"
#include "mem.h"



/* Destroy hardware state associated with texture `t'.
 */
void radeonDestroyTexObj( radeonContextPtr rmesa, radeonTexObjPtr t )
{
   if ( !t )
      return;

   if ( RADEON_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, t, t->tObj );
   }

   if ( t->memBlock ) {
      mmFreeMem( t->memBlock );
      t->memBlock = NULL;
   }

   if ( t->tObj )
      t->tObj->DriverData = NULL;

   if ( rmesa ) {
      /* Bump the performace counter */
      rmesa->c_textureSwaps++;

      if ( t == rmesa->state.texture.unit[0].texobj ) {
         rmesa->state.texture.unit[0].texobj = NULL;
	 remove_from_list( &rmesa->hw.tex[0] );
	 make_empty_list( &rmesa->hw.tex[0] );
      }

      if ( t == rmesa->state.texture.unit[1].texobj ) {
         rmesa->state.texture.unit[1].texobj = NULL;
	 remove_from_list( &rmesa->hw.tex[1] );
	 make_empty_list( &rmesa->hw.tex[1] );
      }
   }

   remove_from_list( t );
   FREE( t );
}


/* Keep track of swapped out texture objects.
 */
void radeonSwapOutTexObj( radeonContextPtr rmesa, radeonTexObjPtr t )
{
   if ( RADEON_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, t, t->tObj );
   }

   /* Bump the performace counter */
   rmesa->c_textureSwaps++;

   if ( t->memBlock ) {
      mmFreeMem( t->memBlock );
      t->memBlock = NULL;
   }

   t->dirty_images = ~0;
   move_to_tail( &rmesa->texture.swapped, t );
}

/* Print out debugging information about texture LRU.
 */
void radeonPrintLocalLRU( radeonContextPtr rmesa, int heap )
{
   radeonTexObjPtr t;
   int sz = 1 << (rmesa->radeonScreen->logTexGranularity[heap]);

   fprintf( stderr, "\nLocal LRU, heap %d:\n", heap );

   foreach ( t, &rmesa->texture.objects[heap] ) {
      if (!t->tObj) {
	 fprintf( stderr, "Placeholder %d at 0x%x sz 0x%x\n",
		  t->memBlock->ofs / sz,
		  t->memBlock->ofs,
		  t->memBlock->size );
      } else {
	 fprintf( stderr, "Texture at 0x%x sz 0x%x\n",
		  t->memBlock->ofs,
		  t->memBlock->size );
      }
   }

   fprintf( stderr, "\n" );
}

void radeonPrintGlobalLRU( radeonContextPtr rmesa, int heap )
{
   radeon_tex_region_t *list = rmesa->sarea->texList[heap];
   int i, j;

   fprintf( stderr, "\nGlobal LRU, heap %d list %p:\n", heap, list );

   for ( i = 0, j = RADEON_NR_TEX_REGIONS ; i < RADEON_NR_TEX_REGIONS ; i++ ) {
      fprintf( stderr, "list[%d] age %d next %d prev %d\n",
	       j, list[j].age, list[j].next, list[j].prev );
      j = list[j].next;
      if ( j == RADEON_NR_TEX_REGIONS ) break;
   }

   if ( j != RADEON_NR_TEX_REGIONS ) {
      fprintf( stderr, "Loop detected in global LRU\n" );
      for ( i = 0 ; i < RADEON_NR_TEX_REGIONS ; i++ ) {
	 fprintf( stderr, "list[%d] age %d next %d prev %d\n",
		  i, list[i].age, list[i].next, list[i].prev );
      }
   }

   fprintf( stderr, "\n" );
}

/* Reset the global texture LRU.
 */
static void radeonResetGlobalLRU( radeonContextPtr rmesa, int heap )
{
   radeon_tex_region_t *list = rmesa->sarea->texList[heap];
   int sz = 1 << rmesa->radeonScreen->logTexGranularity[heap];
   int i;

   /*
    * (Re)initialize the global circular LRU list.  The last element in
    * the array (RADEON_NR_TEX_REGIONS) is the sentinal.  Keeping it at
    * the end of the array allows it to be addressed rationally when
    * looking up objects at a particular location in texture memory.
    */
   for ( i = 0 ; (i+1) * sz <= rmesa->radeonScreen->texSize[heap] ; i++ ) {
      list[i].prev = i-1;
      list[i].next = i+1;
      list[i].age = 0;
   }

   i--;
   list[0].prev = RADEON_NR_TEX_REGIONS;
   list[i].prev = i-1;
   list[i].next = RADEON_NR_TEX_REGIONS;
   list[RADEON_NR_TEX_REGIONS].prev = i;
   list[RADEON_NR_TEX_REGIONS].next = 0;
   rmesa->sarea->texAge[heap] = 0;
}

/* Update the local and glock texture LRUs.
 */
void radeonUpdateTexLRU(radeonContextPtr rmesa, radeonTexObjPtr t )
{
   int heap = t->heap;
   radeon_tex_region_t *list = rmesa->sarea->texList[heap];
   int sz = rmesa->radeonScreen->logTexGranularity[heap];
   int start = t->memBlock->ofs >> sz;
   int end = (t->memBlock->ofs + t->memBlock->size-1) >> sz;
   int i;

   rmesa->texture.age[heap] = ++rmesa->sarea->texAge[heap];

   if ( !t->memBlock ) {
      fprintf( stderr, "no memblock\n\n" );
      return;
   }

   /* Update our local LRU */
   move_to_head( &rmesa->texture.objects[heap], t );

   /* Update the global LRU */
   for ( i = start ; i <= end ; i++ ) {
      list[i].in_use = 1;
      list[i].age = rmesa->texture.age[heap];

      /* remove_from_list(i) */
      list[(CARD32)list[i].next].prev = list[i].prev;
      list[(CARD32)list[i].prev].next = list[i].next;

      /* insert_at_head(list, i) */
      list[i].prev = RADEON_NR_TEX_REGIONS;
      list[i].next = list[RADEON_NR_TEX_REGIONS].next;
      list[(CARD32)list[RADEON_NR_TEX_REGIONS].next].prev = i;
      list[RADEON_NR_TEX_REGIONS].next = i;
   }

   if ( 0 ) {
      radeonPrintGlobalLRU( rmesa, t->heap );
      radeonPrintLocalLRU( rmesa, t->heap );
   }
}

/* Update our notion of what textures have been changed since we last
 * held the lock.  This pertains to both our local textures and the
 * textures belonging to other clients.  Keep track of other client's
 * textures by pushing a placeholder texture onto the LRU list -- these
 * are denoted by (tObj == NULL).
 */
static void radeonTexturesGone( radeonContextPtr rmesa, int heap,
				int offset, int size, int in_use )
{
   radeonTexObjPtr t, tmp;

   foreach_s ( t, tmp, &rmesa->texture.objects[heap] ) {
      if ( t->memBlock->ofs >= offset + size ||
	   t->memBlock->ofs + t->memBlock->size <= offset )
	 continue;

      /* It overlaps - kick it out.  Need to hold onto the currently
       * bound objects, however.
       */
      radeonSwapOutTexObj( rmesa, t );
   }

   if ( in_use ) {
      t = (radeonTexObjPtr) CALLOC( sizeof(*t) );
      if ( !t ) return;

      t->memBlock = mmAllocMem( rmesa->texture.heap[heap], size, 0, offset );
      if ( !t->memBlock ) {
	 fprintf( stderr, "Couldn't alloc placeholder sz %x ofs %x\n",
		  (int)size, (int)offset );
	 mmDumpMemInfo( rmesa->texture.heap[heap] );
	 return;
      }
      insert_at_head( &rmesa->texture.objects[heap], t );
   }
}

/* Update our client's shared texture state.  If another client has
 * modified a region in which we have textures, then we need to figure
 * out which of our textures has been removed, and update our global
 * LRU.
 */
void radeonAgeTextures( radeonContextPtr rmesa, int heap )
{
   RADEONSAREAPrivPtr sarea = rmesa->sarea;

   if ( sarea->texAge[heap] != rmesa->texture.age[heap] ) {
      int sz = 1 << rmesa->radeonScreen->logTexGranularity[heap];
      int nr = 0;
      int idx;

      for ( idx = sarea->texList[heap][RADEON_NR_TEX_REGIONS].prev ;
	    idx != RADEON_NR_TEX_REGIONS && nr < RADEON_NR_TEX_REGIONS ;
	    idx = sarea->texList[heap][idx].prev, nr++ )
      {
	 /* If switching texturing schemes, then the SAREA might not
	  * have been properly cleared, so we need to reset the
	  * global texture LRU.
	  */
	 if ( idx * sz > rmesa->radeonScreen->texSize[heap] ) {
	    nr = RADEON_NR_TEX_REGIONS;
	    break;
	 }

	 if ( sarea->texList[heap][idx].age > rmesa->texture.age[heap] ) {
	    radeonTexturesGone( rmesa, heap, idx * sz, sz,
				sarea->texList[heap][idx].in_use );
	 }
      }

      if ( nr == RADEON_NR_TEX_REGIONS ) {
	 radeonTexturesGone( rmesa, heap, 0,
			     rmesa->radeonScreen->texSize[heap], 0 );
	 radeonResetGlobalLRU( rmesa, heap );
      }

      rmesa->texture.age[heap] = sarea->texAge[heap];
   }
}


/* =============================================================
 * Texture image conversions
 */

/* Upload the texture image associated with texture `t' at level `level'
 * at the address relative to `start'.
 */
static void radeonUploadSubImage( radeonContextPtr rmesa,
				  radeonTexObjPtr t, GLint level,
				  GLint x, GLint y, GLint width, GLint height )
{
   struct gl_texture_image *texImage;
   const struct gl_texture_format *texFormat;
   GLint texelsPerDword = 0;
   GLuint format, pitch, offset;
   GLint imageWidth, imageHeight;
   GLint ret;
   drmRadeonTexture tex;
   drmRadeonTexImage tmp;

   if ( RADEON_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, t, t->tObj );
   }

   /* Ensure we have a valid texture to upload */
   level += t->firstLevel;
   if ( ( level < 0 ) || ( level >= RADEON_MAX_TEXTURE_LEVELS ) ) {
      _mesa_problem(NULL, "bad texture level in radeonUploadSubimage");
      return;
   }

   texImage = t->tObj->Image[level];
   if ( !texImage ) {
      if ( RADEON_DEBUG & DEBUG_TEXTURE )
	 fprintf( stderr, "%s: texImage %d is NULL!\n", __FUNCTION__, level );
      return;
   }
   if ( !texImage->Data ) {
      if ( RADEON_DEBUG & DEBUG_TEXTURE )
	 fprintf( stderr, "%s: image data is NULL!\n", __FUNCTION__ );
      return;
   }

   texFormat = texImage->TexFormat;

   switch ( texFormat->TexelBytes ) {
   case 1:
      texelsPerDword = 4;
      break;
   case 2:
      texelsPerDword = 2;
      break;
   case 4:
      texelsPerDword = 1;
      break;
   }

   format = t->pp_txformat & RADEON_TXFORMAT_FORMAT_MASK;

   imageWidth = texImage->Width;
   imageHeight = texImage->Height;

   offset = t->bufAddr;
   pitch = (t->image[0].width * texFormat->TexelBytes) / 64;

#if 0
   /* Bump the performace counter */
   rmesa->c_textureBytes += (dwords << 2);
#endif

   if ( RADEON_DEBUG & (DEBUG_TEXTURE|DEBUG_IOCTL) ) {
      GLint imageX = 0;
      GLint imageY = 0;
      GLint blitX = t->image[level].x;
      GLint blitY = t->image[level].y;
      GLint blitWidth = t->image[level].width;
      GLint blitHeight = t->image[level].height;
      fprintf( stderr, "   upload image: %d,%d at %d,%d\n",
	       imageWidth, imageHeight, imageX, imageY );
      fprintf( stderr, "   upload  blit: %d,%d at %d,%d\n",
	       blitWidth, blitHeight, blitX, blitY );
      fprintf( stderr, "       blit ofs: 0x%07x pitch: 0x%x "
	       "level: %d format: %x\n",
	       (GLuint)offset, (GLuint)pitch, level, format );
   }

   t->image[level].data = texImage->Data;

   tex.offset = offset;
   tex.pitch = pitch;
   tex.format = format;
   tex.width = imageWidth;
   tex.height = imageHeight;
   tex.image = &tmp;

   memcpy( &tmp, &t->image[level], sizeof(drmRadeonTexImage) );

   do {
      ret = drmCommandWriteRead( rmesa->dri.fd, DRM_RADEON_TEXTURE,
                                 &tex, sizeof(drmRadeonTexture) );
   } while ( ret && errno == EAGAIN );

   if ( ret ) {
      UNLOCK_HARDWARE( rmesa );
      fprintf( stderr, "DRM_RADEON_TEXTURE: return = %d\n", ret );
      fprintf( stderr, "   offset=0x%08x pitch=0x%x format=%d\n",
	       offset, pitch, format );
      fprintf( stderr, "   image width=%d height=%d\n",
	       imageWidth, imageHeight );
      fprintf( stderr, "    blit width=%d height=%d data=%p\n",
	       t->image[level].width, t->image[level].height,
	       t->image[level].data );
      exit( 1 );
   }
}

/* Upload the texture images associated with texture `t'.  This might
 * require removing our own and/or other client's texture objects to
 * make room for these images.
 */
int radeonUploadTexImages( radeonContextPtr rmesa, radeonTexObjPtr t )
{
   const int numLevels = t->lastLevel - t->firstLevel + 1;
   int i;
   int heap;
   radeonTexObjPtr t0 = rmesa->state.texture.unit[0].texobj;
   radeonTexObjPtr t1 = rmesa->state.texture.unit[1].texobj;

   if ( RADEON_DEBUG & (DEBUG_TEXTURE|DEBUG_IOCTL) ) {
      fprintf( stderr, "%s( %p, %p ) sz=%d lvls=%d-%d\n", __FUNCTION__,
	       rmesa->glCtx, t->tObj, t->totalSize,
	       t->firstLevel, t->lastLevel );
   }

   if ( !t || t->totalSize == 0 )
      return 0;

   LOCK_HARDWARE( rmesa );

   /* Choose the heap appropriately */
   heap = t->heap = RADEON_CARD_HEAP;
#if 0
   if ( !rmesa->radeonScreen->IsPCI &&
	t->totalSize > rmesa->radeonScreen->texSize[heap] ) {
      heap = t->heap = RADEON_AGP_HEAP;
   }
#endif

   /* Do we need to eject LRU texture objects? */
   if ( !t->memBlock ) {
      /* Allocate a memory block on a 4k boundary (1<<12 == 4096) */
      t->memBlock = mmAllocMem( rmesa->texture.heap[heap],
				t->totalSize, 12, 0 );

#if 0
      /* Try AGP before kicking anything out of local mem */
      if ( !t->memBlock && heap == RADEON_CARD_HEAP ) {
	 t->memBlock = mmAllocMem( rmesa->texture.heap[RADEON_AGP_HEAP],
				   t->totalSize, 12, 0 );

	 if ( t->memBlock )
	    heap = t->heap = RADEON_AGP_HEAP;
      }
#endif

      /* Kick out textures until the requested texture fits */
      while ( !t->memBlock ) {
	 if ( rmesa->texture.objects[heap].prev == t0 ||
	      rmesa->texture.objects[heap].prev == t1 ) {
	    fprintf( stderr,
		     "radeonUploadTexImages: ran into bound texture\n" );
	    UNLOCK_HARDWARE( rmesa );
	    return -1;
	 }
	 if ( rmesa->texture.objects[heap].prev ==
	      &rmesa->texture.objects[heap] ) {
	    if ( rmesa->radeonScreen->IsPCI ) {
	       fprintf( stderr, "radeonUploadTexImages: upload texture "
			"failure on local texture heaps, sz=%d\n",
			t->totalSize );
	       UNLOCK_HARDWARE( rmesa );
	       return -1;
#if 0
	    } else if ( heap == RADEON_CARD_HEAP ) {
	       heap = t->heap = RADEON_AGP_HEAP;
	       continue;
#endif
	    } else {
	       fprintf( stderr, "radeonUploadTexImages: upload texture "
			"failure on both local and AGP texture heaps, "
			"sz=%d\n",
			t->totalSize );
	       UNLOCK_HARDWARE( rmesa );
	       return -1;
	    }
	 }

	 radeonSwapOutTexObj( rmesa, rmesa->texture.objects[heap].prev );

	 t->memBlock = mmAllocMem( rmesa->texture.heap[heap],
				   t->totalSize, 12, 0 );
      }

      /* Set the base offset of the texture image */
      t->bufAddr = rmesa->radeonScreen->texOffset[heap] + t->memBlock->ofs;
      t->pp_txoffset = t->bufAddr;

#if 0
      /* Fix AGP texture offsets */
      if ( heap == RADEON_AGP_HEAP ) {
	  t->setup.pp_tx_offset += RADEON_AGP_TEX_OFFSET +
	      rmesa->radeonScreen->agpTexOffset;
      }
#endif

      /* Mark this texobj as dirty on all units:
       */
      t->dirty_state = TEX_ALL;
   }

   /* Let the world know we've used this memory recently */
   radeonUpdateTexLRU( rmesa, t );

   /* Upload any images that are new */
   if (t->dirty_images) {
      for ( i = 0 ; i < numLevels ; i++ ) {
         if ( t->dirty_images & (1 << i) ) {
            radeonUploadSubImage( rmesa, t, i, 0, 0,
                                  t->image[i].width, t->image[i].height );
         }
      }
      t->dirty_images = 0;
   }


   UNLOCK_HARDWARE( rmesa );

   return 0;
}
