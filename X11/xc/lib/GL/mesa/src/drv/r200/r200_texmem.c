/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_texmem.c,v 1.5 2002/12/17 00:32:56 dawes Exp $ */
/**************************************************************************

Copyright (C) Tungsten Graphics 2002.  All Rights Reserved.  
The Weather Channel, Inc. funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86
license. This notice must be preserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation on the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NON-INFRINGEMENT. IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR THEIR
SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

#include "radeon_reg.h"
#include "r200_context.h"
#include "r200_state.h"
#include "r200_ioctl.h"
#include "r200_swtcl.h"
#include "r200_tex.h"

#include "context.h"
#include "colormac.h"
#include "mmath.h"
#include "macros.h"
#include "simple_list.h"
#include "enums.h"
#include "mem.h"

#include <unistd.h>	/* for usleep */

/* Destroy hardware state associated with texture `t'.
 */
void r200DestroyTexObj( r200ContextPtr rmesa, r200TexObjPtr t )
{
   if ( !t )
      return;

   if ( R200_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, t, t->tObj );
   }

   if ( t->memBlock ) {
      mmFreeMem( t->memBlock );
      t->memBlock = NULL;
   }

   if ( t->tObj )
      t->tObj->DriverData = NULL;

   if ( rmesa ) {
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
void r200SwapOutTexObj( r200ContextPtr rmesa, r200TexObjPtr t )
{
   if ( R200_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, t, t->tObj );
   }

   if ( t->memBlock ) {
      mmFreeMem( t->memBlock );
      t->memBlock = NULL;
   }

   t->dirty_images = ~0;
   move_to_tail( &rmesa->texture.swapped, t );
}

/* Print out debugging information about texture LRU.
 */
void r200PrintLocalLRU( r200ContextPtr rmesa, int heap )
{
   r200TexObjPtr t;
   int sz = 1 << (rmesa->r200Screen->logTexGranularity[heap]);

   fprintf( stderr, "\nLocal LRU, heap %d:\n", heap );

   foreach ( t, &rmesa->texture.objects[heap] ) {
      if (!t->memBlock)
	 continue;
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

void r200PrintGlobalLRU( r200ContextPtr rmesa, int heap )
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
static void r200ResetGlobalLRU( r200ContextPtr rmesa, int heap )
{
   radeon_tex_region_t *list = rmesa->sarea->texList[heap];
   int sz = 1 << rmesa->r200Screen->logTexGranularity[heap];
   int i;

   /*
    * (Re)initialize the global circular LRU list.  The last element in
    * the array (RADEON_NR_TEX_REGIONS) is the sentinal.  Keeping it at
    * the end of the array allows it to be addressed rationally when
    * looking up objects at a particular location in texture memory.
    */
   for ( i = 0 ; (i+1) * sz <= rmesa->r200Screen->texSize[heap] ; i++ ) {
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
void r200UpdateTexLRU(r200ContextPtr rmesa, r200TexObjPtr t )
{
   int heap = t->heap;
   radeon_tex_region_t *list = rmesa->sarea->texList[heap];
   int sz = rmesa->r200Screen->logTexGranularity[heap];
   int i, start, end;

   rmesa->texture.age[heap] = ++rmesa->sarea->texAge[heap];

   if ( !t->memBlock ) 
      return;

   start = t->memBlock->ofs >> sz;
   end = (t->memBlock->ofs + t->memBlock->size-1) >> sz;

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
      r200PrintGlobalLRU( rmesa, t->heap );
      r200PrintLocalLRU( rmesa, t->heap );
   }
}

/* Update our notion of what textures have been changed since we last
 * held the lock.  This pertains to both our local textures and the
 * textures belonging to other clients.  Keep track of other client's
 * textures by pushing a placeholder texture onto the LRU list -- these
 * are denoted by (tObj == NULL).
 */
static void r200TexturesGone( r200ContextPtr rmesa, int heap,
				int offset, int size, int in_use )
{
   r200TexObjPtr t, tmp;

   foreach_s ( t, tmp, &rmesa->texture.objects[heap] ) {
      if ( !t->memBlock ||
	   t->memBlock->ofs >= offset + size ||
	   t->memBlock->ofs + t->memBlock->size <= offset )
	 continue;

      /* It overlaps - kick it out.  Need to hold onto the currently
       * bound objects, however.
       */
      r200SwapOutTexObj( rmesa, t );
   }

   if ( in_use ) {
      t = (r200TexObjPtr) CALLOC( sizeof(*t) );
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
void r200AgeTextures( r200ContextPtr rmesa, int heap )
{
   RADEONSAREAPrivPtr sarea = rmesa->sarea;

   if ( sarea->texAge[heap] != rmesa->texture.age[heap] ) {
      int sz = 1 << rmesa->r200Screen->logTexGranularity[heap];
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
	 if ( idx * sz > rmesa->r200Screen->texSize[heap] ) {
	    nr = RADEON_NR_TEX_REGIONS;
	    break;
	 }

	 if ( sarea->texList[heap][idx].age > rmesa->texture.age[heap] ) {
	    r200TexturesGone( rmesa, heap, idx * sz, sz,
				sarea->texList[heap][idx].in_use );
	 }
      }

      if ( nr == RADEON_NR_TEX_REGIONS ) {
	 r200TexturesGone( rmesa, heap, 0,
			     rmesa->r200Screen->texSize[heap], 0 );
	 r200ResetGlobalLRU( rmesa, heap );
      }

      rmesa->texture.age[heap] = sarea->texAge[heap];
   }
}


/* ------------------------------------------------------------
 * Texture image conversions
 */


static void r200UploadAGPClientSubImage( r200ContextPtr rmesa,
					 r200TexObjPtr t, 
					 struct gl_texture_image *texImage,
					 GLint hwlevel,
					 GLint x, GLint y, 
					 GLint width, GLint height )
{
   const struct gl_texture_format *texFormat = texImage->TexFormat;
   GLuint pitch = t->image[0].width * texFormat->TexelBytes;
   int blit_format;
   int srcOffset;


   switch ( texFormat->TexelBytes ) {
   case 1:
      blit_format = R200_CP_COLOR_FORMAT_CI8;
      break;
   case 2:
      blit_format = R200_CP_COLOR_FORMAT_RGB565;
      break;
   case 4:
      blit_format = R200_CP_COLOR_FORMAT_ARGB8888;
      break;
   default:
      return;
   }

   t->image[hwlevel].data = texImage->Data;
   srcOffset = r200AgpOffsetFromVirtual( rmesa, texImage->Data );

   assert( srcOffset != ~0 );

   /* Don't currently need to cope with small pitches?
    */
   width = texImage->Width;
   height = texImage->Height;

   r200EmitWait( rmesa, RADEON_WAIT_3D );

   r200EmitBlit( rmesa, blit_format, 
		 pitch,  
		 srcOffset,   
		 t->image[0].width * texFormat->TexelBytes, /* dst pitch! */
		 t->bufAddr,
		 x, 
		 y, 
		 t->image[hwlevel].x + x,
		 t->image[hwlevel].y + y, 
		 width,
		 height );

   r200EmitWait( rmesa, RADEON_WAIT_2D );
}

static void r200UploadRectSubImage( r200ContextPtr rmesa,
				    r200TexObjPtr t, 
				    struct gl_texture_image *texImage,
				    GLint x, GLint y, 
				    GLint width, GLint height )
{
   const struct gl_texture_format *texFormat = texImage->TexFormat;
   int blit_format, blit_pitch, done;

   switch ( texFormat->TexelBytes ) {
   case 1:
      blit_format = R200_CP_COLOR_FORMAT_CI8;
      break;
   case 2:
      blit_format = R200_CP_COLOR_FORMAT_RGB565;
      break;
   case 4:
      blit_format = R200_CP_COLOR_FORMAT_ARGB8888;
      break;
   default:
      return;
   }

   t->image[0].data = texImage->Data;

   /* Currently don't need to cope with small pitches.
    */
   width = texImage->Width;
   height = texImage->Height;
   blit_pitch = t->pp_txpitch + 32;

   if (rmesa->prefer_agp_client_texturing && texImage->IsClientData) {
      /* In this case, could also use agp texturing.  This is
       * currently disabled, but has been tested & works.
       */
      t->pp_txoffset = r200AgpOffsetFromVirtual( rmesa, texImage->Data );
      t->pp_txpitch = texImage->RowStride * texFormat->TexelBytes - 32;

      if (R200_DEBUG & DEBUG_TEXTURE)
	 fprintf(stderr, 
		 "Using agp texturing for rectangular client texture\n");

      /* Release FB memory allocated for this image:
       */
      if ( t->memBlock ) {
	 mmFreeMem( t->memBlock );
	 t->memBlock = NULL;
      }

   }
   else if (texImage->IsClientData) {
      /* Data already in agp memory, with usable pitch.
       */
      r200EmitBlit( rmesa, 
		    blit_format, 
		    texImage->RowStride * texFormat->TexelBytes, 
		    r200AgpOffsetFromVirtual( rmesa, texImage->Data ),   
		    blit_pitch, t->bufAddr,
		    0, 0, 
		    0, 0, 
		    width, height );
   }
   else {
      /* Data not in agp memory, or bad pitch.
       */
      for (done = 0; done < height ; ) {
	 struct r200_dma_region region;
	 int lines = MIN2( height - done, RADEON_BUFFER_SIZE / blit_pitch );
	 int src_pitch = texImage->RowStride * texFormat->TexelBytes;
	 char *tex = (char *)texImage->Data + done * src_pitch;

	 memset(&region, 0, sizeof(region));
	 r200AllocDmaRegion( rmesa, &region, lines * blit_pitch, 64 );

	 /* Copy texdata to dma:
	  */
	 if (0)
	    fprintf(stderr, "%s: src_pitch %d blit_pitch %d\n",
		    __FUNCTION__, src_pitch, blit_pitch);

	 if (src_pitch == blit_pitch) {
	    memcpy( region.address, tex, lines * src_pitch );
	 } 
	 else {
	    char *buf = region.address;
	    int i;
	    for (i = 0 ; i < lines ; i++) {
	       memcpy( buf, tex, src_pitch );
	       buf += blit_pitch;
	       tex += src_pitch;
	    }
	 }

	 r200EmitWait( rmesa, RADEON_WAIT_3D );

	 /* Blit to framebuffer
	  */
	 r200EmitBlit( rmesa, 
		       blit_format, 
		       blit_pitch, GET_START( &region ),   
		       blit_pitch, t->bufAddr,
		       0, 0, 
		       0, done, 
		       width, lines );
	 
	 r200EmitWait( rmesa, RADEON_WAIT_2D );

	 r200ReleaseDmaRegion( rmesa, &region, __FUNCTION__ );
	 done += lines;
      }
   }
}


/* Upload the texture image associated with texture `t' at level `level'
 * at the address relative to `start'.
 */
static void r200UploadSubImage( r200ContextPtr rmesa,
				r200TexObjPtr t, 
				GLint hwlevel,
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
   int level = hwlevel + t->firstLevel;

   if ( R200_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s level %d %dx%d\n", __FUNCTION__,
	       level, width, height);
   }

   /* Ensure we have a valid texture to upload */
   if ( ( hwlevel < 0 ) || ( hwlevel >= RADEON_MAX_TEXTURE_LEVELS ) ) {
      _mesa_problem(NULL, "bad texture level in r200UploadSubimage");
      return;
   }

   texImage = t->tObj->Image[level];
   if ( !texImage ) {
      if ( R200_DEBUG & DEBUG_TEXTURE )
	 fprintf( stderr, "%s: texImage %d is NULL!\n", __FUNCTION__, level );
      return;
   }
   if ( !texImage->Data ) {
      if ( R200_DEBUG & DEBUG_TEXTURE )
	 fprintf( stderr, "%s: image data is NULL!\n", __FUNCTION__ );
      return;
   }


   if (t->tObj->Target == GL_TEXTURE_RECTANGLE_NV) {
      assert(level == 0);
      assert(hwlevel == 0);
      if ( R200_DEBUG & DEBUG_TEXTURE )
	 fprintf( stderr, "%s: image data is rectangular\n", __FUNCTION__);
      r200UploadRectSubImage( rmesa, t, texImage, x, y, width, height );
      return;
   }
   else if (texImage->IsClientData) {
      if ( R200_DEBUG & DEBUG_TEXTURE )
	 fprintf( stderr, "%s: image data is in agp client storage\n",
		  __FUNCTION__);
      r200UploadAGPClientSubImage( rmesa, t, texImage, hwlevel,
				   x, y, width, height );
      return;
   }
   else if ( R200_DEBUG & DEBUG_TEXTURE )
      fprintf( stderr, "%s: image data is in normal memory\n",
	       __FUNCTION__);
      

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

   format = t->pp_txformat & R200_TXFORMAT_FORMAT_MASK;

   imageWidth = texImage->Width;
   imageHeight = texImage->Height;

   offset = t->bufAddr;
   pitch = (t->image[0].width * texFormat->TexelBytes) / 64;


   if ( R200_DEBUG & (DEBUG_TEXTURE|DEBUG_IOCTL) )
   {
      GLint imageX = 0;
      GLint imageY = 0;
      GLint blitX = t->image[hwlevel].x;
      GLint blitY = t->image[hwlevel].y;
      GLint blitWidth = t->image[hwlevel].width;
      GLint blitHeight = t->image[hwlevel].height;
      fprintf( stderr, "   upload image: %d,%d at %d,%d\n",
	       imageWidth, imageHeight, imageX, imageY );
      fprintf( stderr, "   upload  blit: %d,%d at %d,%d\n",
	       blitWidth, blitHeight, blitX, blitY );
      fprintf( stderr, "       blit ofs: 0x%07x pitch: 0x%x "
	       "level: %d/%d format: %x\n",
	       (GLuint)offset, (GLuint)pitch, hwlevel, level, format );
   }

   t->image[hwlevel].data = texImage->Data;

   tex.offset = offset;
   tex.pitch = pitch;
   tex.format = format;
   tex.width = imageWidth;
   tex.height = imageHeight;
   tex.image = &tmp;

   memcpy( &tmp, &t->image[hwlevel], sizeof(drmRadeonTexImage) );

   LOCK_HARDWARE( rmesa );
   do {
      ret = drmCommandWriteRead( rmesa->dri.fd, DRM_RADEON_TEXTURE,
                                 &tex, sizeof(drmRadeonTexture) );
      if (ret) {
	 if (R200_DEBUG & DEBUG_IOCTL)
	    fprintf(stderr, "DRM_RADEON_TEXTURE:  again!\n");
	 usleep(1);
      }
   } while ( ret && errno == EAGAIN );

   UNLOCK_HARDWARE( rmesa );

   if ( ret ) {
      fprintf( stderr, "DRM_R200_TEXTURE: return = %d\n", ret );
      fprintf( stderr, "   offset=0x%08x pitch=0x%x format=%d\n",
	       offset, pitch, format );
      fprintf( stderr, "   image width=%d height=%d\n",
	       imageWidth, imageHeight );
      fprintf( stderr, "    blit width=%d height=%d data=%p\n",
	       t->image[hwlevel].width, t->image[hwlevel].height,
	       t->image[hwlevel].data );
      exit( 1 );
   }
}



/* Upload the texture images associated with texture `t'.  This might
 * require removing our own and/or other client's texture objects to
 * make room for these images.
 */
int r200UploadTexImages( r200ContextPtr rmesa, r200TexObjPtr t )
{
   const int numLevels = t->lastLevel - t->firstLevel + 1;
   int heap;
   r200TexObjPtr t0 = rmesa->state.texture.unit[0].texobj;
   r200TexObjPtr t1 = rmesa->state.texture.unit[1].texobj;

   if ( R200_DEBUG & (DEBUG_TEXTURE|DEBUG_IOCTL) ) {
      fprintf( stderr, "%s( %p, %p ) sz=%d lvls=%d-%d\n", __FUNCTION__,
	       rmesa->glCtx, t->tObj, t->totalSize,
	       t->firstLevel, t->lastLevel );
   }

   if ( !t || t->totalSize == 0 )
      return 0;

   if (R200_DEBUG & DEBUG_SYNC) {
      fprintf(stderr, "\nSyncing\n\n");
      R200_FIREVERTICES( rmesa );
      r200Finish( rmesa->glCtx );
   }

   LOCK_HARDWARE( rmesa );

   /* Choose the heap appropriately */
   heap = t->heap = RADEON_CARD_HEAP;

   /* Do we need to eject LRU texture objects? */
   if ( !t->memBlock ) {
      /* Allocate a memory block on a 1k boundary (1<<10 == 1024) */
      t->memBlock = mmAllocMem( rmesa->texture.heap[heap],
				t->totalSize, 10, 0 );


      /* Kick out textures until the requested texture fits */
      while ( !t->memBlock ) {
	 if ( rmesa->texture.objects[heap].prev == t0 ||
	      rmesa->texture.objects[heap].prev == t1 ) {
	    fprintf( stderr,
		     "r200UploadTexImages: ran into bound texture\n" );
	    UNLOCK_HARDWARE( rmesa );
	    return -1;
	 }
	 if ( rmesa->texture.objects[heap].prev ==
	      &rmesa->texture.objects[heap] ) {
	    if ( rmesa->r200Screen->IsPCI ) {
	       fprintf( stderr, "r200UploadTexImages: upload texture "
			"failure on local texture heaps, sz=%d\n",
			t->totalSize );
	       UNLOCK_HARDWARE( rmesa );
	       return -1;
	    } else {
	       fprintf( stderr, "r200UploadTexImages: upload texture "
			"failure on both local and AGP texture heaps, "
			"sz=%d\n",
			t->totalSize );
	       UNLOCK_HARDWARE( rmesa );
	       return -1;
	    }
	 }

	 r200SwapOutTexObj( rmesa, rmesa->texture.objects[heap].prev );

	 t->memBlock = mmAllocMem( rmesa->texture.heap[heap],
				   t->totalSize, 12, 0 );
      }

      /* Set the base offset of the texture image */
      t->bufAddr = rmesa->r200Screen->texOffset[heap] + t->memBlock->ofs;
      t->pp_txoffset = t->bufAddr;

      /* Mark this texobj as dirty on all units:
       */
      t->dirty_state = TEX_ALL;
   }

   /* Let the world know we've used this memory recently */
   r200UpdateTexLRU( rmesa, t );
   UNLOCK_HARDWARE( rmesa );

   /* Upload any images that are new */
   if (t->dirty_images) {
      int hwlevel;
      for ( hwlevel = 0 ; hwlevel < numLevels ; hwlevel++ ) {
         if ( t->dirty_images & (1 << (hwlevel+t->firstLevel)) ) {
            r200UploadSubImage( rmesa, t, hwlevel, 
				0, 0,
				t->image[hwlevel].width, 
				t->image[hwlevel].height );
         }
      }
      t->dirty_images = 0;
   }



   if (R200_DEBUG & DEBUG_SYNC) {
      fprintf(stderr, "\nSyncing\n\n");
      r200Finish( rmesa->glCtx );
   }

   return 0;
}
