/**************************************************************************

Copyright 2001 2d3d Inc., Delray Beach, FL

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

/* $XFree86: xc/lib/GL/mesa/src/drv/i830/i830_texmem.c,v 1.3 2002/12/10 01:26:53 dawes Exp $ */

/*
 * Author:
 *   Jeff Hartmann <jhartmann@2d3d.com>
 *
 * Heavily based on the I810 driver, which was written by:
 *   Keith Whitwell <keithw@tungstengraphics.com>
 */

#include <stdlib.h>
#include <stdio.h>

#include "glheader.h"
#include "macros.h"
#include "mtypes.h"
#include "simple_list.h"
#include "enums.h"
#include "texformat.h"

#include "mm.h"

#include "i830_screen.h"
#include "i830_dri.h"

#include "i830_context.h"
#include "i830_tex.h"
#include "i830_state.h"
#include "i830_ioctl.h"


void i830DestroyTexObj(i830ContextPtr imesa, i830TextureObjectPtr t)
{
   if (!t) return;

   /* This is sad - need to sync *in case* we upload a texture
    * to this newly free memory...
    */
   if (t->MemBlock) {
      mmFreeMem(t->MemBlock);
      t->MemBlock = 0;

      if (imesa && t->age > imesa->dirtyAge)
	 imesa->dirtyAge = t->age;
   }

   if (t->globj)
      t->globj->DriverData = 0;

   if (imesa) {
      if (imesa->CurrentTexObj[0] == t) {
         imesa->CurrentTexObj[0] = 0;
         imesa->dirty &= ~I830_UPLOAD_TEX0;
      }

      if (imesa->CurrentTexObj[1] == t) {
         imesa->CurrentTexObj[1] = 0;
         imesa->dirty &= ~I830_UPLOAD_TEX1;
      }
   }

   remove_from_list(t);
   free(t);
}


void i830SwapOutTexObj(i830ContextPtr imesa, i830TextureObjectPtr t)
{
/*     fprintf(stderr, "%s\n", __FUNCTION__); */

   if (t->MemBlock) {
      mmFreeMem(t->MemBlock);
      t->MemBlock = 0;

      if (t->age > imesa->dirtyAge)
	 imesa->dirtyAge = t->age;
   }

   t->dirty_images = ~0;
   move_to_tail(&(imesa->SwappedOut), t);
}



/* Upload an image from mesa's internal copy.
 */
static void i830UploadTexLevel( i830TextureObjectPtr t, int hwlevel )
{
   int level = hwlevel + t->firstLevel;
   const struct gl_texture_image *image = t->image[hwlevel].image;
   int i,j;

   if (!image || !image->Data)
      return;

   if (0) fprintf(stderr, "Uploading level : %d\n", level);

   switch (image->TexFormat->MesaFormat) {
   case MESA_FORMAT_I8:
   case MESA_FORMAT_L8:
      {
	 GLubyte *dst = (GLubyte *)(t->BufAddr + t->image[hwlevel].offset);
	 GLubyte *src = (GLubyte *)image->Data;

	 for (j = 0 ; j < image->Height ; j++, dst += t->Pitch) {
	    for (i = 0 ; i < image->Width ; i++) {
	       dst[i] = src[0];
	       src++;
	    }
	 }
      }
      break;

   case MESA_FORMAT_AL88:
   case MESA_FORMAT_RGB565:
   case MESA_FORMAT_ARGB1555:
   case MESA_FORMAT_ARGB4444:
      {
	 GLushort *dst = (GLushort *)(t->BufAddr + t->image[hwlevel].offset);
	 GLushort *src = (GLushort *)image->Data;

	 for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	    for (i = 0 ; i < image->Width ; i++) {
	       dst[i] = src[0];
	       src++;
	    }
	 }
      }
      break;

   case MESA_FORMAT_ARGB8888:
      {
	 GLuint *dst = (GLuint *)(t->BufAddr + t->image[hwlevel].offset);
	 GLuint *src = (GLuint *)image->Data;

	 for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/4)) {
	    for (i = 0 ; i < image->Width ; i++) {
	       dst[i] = src[0];
	       src++;
	    }
	 }
      }
      break;

   default:
      fprintf(stderr, "Not supported texture format %s\n",
              _mesa_lookup_enum_by_nr(image->Format));
   }
}

void i830PrintLocalLRU( i830ContextPtr imesa )
{
   i830TextureObjectPtr t;
   int sz = 1 << (imesa->i830Screen->logTextureGranularity);

   foreach( t, &imesa->TexObjList ) {
      if (!t->globj)
	 fprintf(stderr, "Placeholder %d at %x sz %x\n",
		 t->MemBlock->ofs / sz,
		 t->MemBlock->ofs,
		 t->MemBlock->size);
      else
	 fprintf(stderr, "Texture at %x sz %x\n",
		 t->MemBlock->ofs,
		 t->MemBlock->size);

   }
}

void i830PrintGlobalLRU( i830ContextPtr imesa )
{
   int i, j;
   I830TexRegion *list = imesa->sarea->texList;

   for (i = 0, j = I830_NR_TEX_REGIONS ; i < I830_NR_TEX_REGIONS ; i++) {
      fprintf(stderr, "list[%d] age %d next %d prev %d\n",
	      j, list[j].age, list[j].next, list[j].prev);
      j = list[j].next;
      if (j == I830_NR_TEX_REGIONS) break;
   }

   if (j != I830_NR_TEX_REGIONS)
      fprintf(stderr, "Loop detected in global LRU\n");
}


void i830ResetGlobalLRU( i830ContextPtr imesa )
{
   I830TexRegion *list = imesa->sarea->texList;
   int sz = 1 << imesa->i830Screen->logTextureGranularity;
   int i;

   /* (Re)initialize the global circular LRU list.  The last element
    * in the array (I830_NR_TEX_REGIONS) is the sentinal.  Keeping it
    * at the end of the array allows it to be addressed rationally
    * when looking up objects at a particular location in texture
    * memory.
    */
   for (i = 0 ; (i+1) * sz <= imesa->i830Screen->textureSize ; i++) {
      list[i].prev = i-1;
      list[i].next = i+1;
      list[i].age = 0;
   }

   i--;
   list[0].prev = I830_NR_TEX_REGIONS;
   list[i].prev = i-1;
   list[i].next = I830_NR_TEX_REGIONS;
   list[I830_NR_TEX_REGIONS].prev = i;
   list[I830_NR_TEX_REGIONS].next = 0;
   imesa->sarea->texAge = 0;
}


void i830UpdateTexLRU( i830ContextPtr imesa, i830TextureObjectPtr t )
{
   int i;
   int logsz = imesa->i830Screen->logTextureGranularity;
   int start = t->MemBlock->ofs >> logsz;
   int end = (t->MemBlock->ofs + t->MemBlock->size - 1) >> logsz;
   I830TexRegion *list = imesa->sarea->texList;

   imesa->texAge = ++imesa->sarea->texAge;

   /* Update our local LRU
    */
   move_to_head( &(imesa->TexObjList), t );

   /* Update the global LRU
    */
   for (i = start ; i <= end ; i++) {

      list[i].in_use = 1;
      list[i].age = imesa->texAge;

      /* remove_from_list(i)
       */
      list[(unsigned)list[i].next].prev = list[i].prev;
      list[(unsigned)list[i].prev].next = list[i].next;

      /* insert_at_head(list, i)
       */
      list[i].prev = I830_NR_TEX_REGIONS;
      list[i].next = list[I830_NR_TEX_REGIONS].next;
      list[(unsigned)list[I830_NR_TEX_REGIONS].next].prev = i;
      list[I830_NR_TEX_REGIONS].next = i;
   }
}


/* Called for every shared texture region which has increased in age
 * since we last held the lock.
 *
 * Figures out which of our textures have been ejected by other clients,
 * and pushes a placeholder texture onto the LRU list to represent
 * the other client's textures.
 */
void i830TexturesGone( i830ContextPtr imesa,
					                 GLuint offset,
					                 GLuint size,
					                 GLuint in_use )
{   
   i830TextureObjectPtr t, tmp;

   if (I830_DEBUG&DEBUG_TEXTURE)
      fprintf(stderr, "%s\n", __FUNCTION__);

   foreach_s ( t, tmp, &imesa->TexObjList ) {
      if (t->MemBlock == 0 ||	
	  t->MemBlock->ofs >= offset + size ||
	  t->MemBlock->ofs + t->MemBlock->size <= offset)
	 continue;
		
      /* It overlaps - kick it off.  Need to hold onto the currently bound
       * objects, however.
       */
      if (t->bound)
	 i830SwapOutTexObj( imesa, t );
      else
	 i830DestroyTexObj( imesa, t );
   }

   if (in_use) {
      t = (i830TextureObjectPtr) calloc(1,sizeof(*t));
      if (!t) return;

      t->MemBlock = mmAllocMem( imesa->texHeap, size, 0, offset);
      insert_at_head( &imesa->TexObjList, t );
   }
}


/* This is called with the lock held.  May have to eject our own and/or
 * other client's texture objects to make room for the upload.
 */

int i830UploadTexImages( i830ContextPtr imesa, i830TextureObjectPtr t )
{
   int i;
   int ofs;
   int numLevels;

   /* Do we need to eject LRU texture objects?
    */
   if (!t->MemBlock) {
      while (1)
      {
	 t->MemBlock = mmAllocMem( imesa->texHeap, t->totalSize, 12, 0 );
	 if (t->MemBlock)
	    break;

/*
	 if (imesa->TexObjList.prev == imesa->CurrentTexObj[0] ||
	     imesa->TexObjList.prev == imesa->CurrentTexObj[1]) {
  	    fprintf(stderr, "Hit bound texture in upload\n");
 	    i830PrintLocalLRU( imesa ); 
	    return -1;
	 }
*/
	 if (imesa->TexObjList.prev == &(imesa->TexObjList)) {
 	    fprintf(stderr, "Failed to upload texture, sz %d\n", t->totalSize);
	    mmDumpMemInfo( imesa->texHeap );
	    return -1;
	 }

	 i830SwapOutTexObj( imesa, imesa->TexObjList.prev );
      }

      ofs = t->MemBlock->ofs;
      t->BufAddr = imesa->i830Screen->tex.map + ofs;
      t->Setup[I830_TEXREG_TM0S0] = (TM0S0_USE_FENCE |
				     (imesa->i830Screen->textureOffset + ofs));

      if (t == imesa->CurrentTexObj[0])
	 imesa->dirty |= I830_UPLOAD_TEX0;

      if (t == imesa->CurrentTexObj[1])
	 imesa->dirty |= I830_UPLOAD_TEX1;
#if 0
      if (t == imesa->CurrentTexObj[2])
	 I830_STATECHANGE(imesa, I830_UPLOAD_TEX2);

      if (t == imesa->CurrentTexObj[3])
	 I830_STATECHANGE(imesa, I830_UPLOAD_TEX3);
#endif
      if (t->MemBlock)
	 i830UpdateTexLRU( imesa, t );
   }

   if (imesa->dirtyAge >= GET_DISPATCH_AGE(imesa))
      i830WaitAgeLocked( imesa, imesa->dirtyAge );

   numLevels = t->lastLevel - t->firstLevel + 1;
   for (i = 0 ; i < numLevels ; i++)
      if (t->dirty_images & (1<<(i+t->firstLevel)))
	 i830UploadTexLevel( t, i );

   t->dirty_images = 0;
   imesa->sarea->perf_boxes |= I830_BOX_TEXTURE_LOAD;

   return 0;
}
