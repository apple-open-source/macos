/*
 * GLX Hardware Device Driver for Intel i810
 * Copyright (C) 1999 Keith Whitwell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * KEITH WHITWELL, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include "glheader.h"
#include "macros.h"
#include "mtypes.h"
#include "simple_list.h"
#include "enums.h"

#include "mm.h"

#include "i810screen.h"
#include "i810_dri.h"

#include "i810context.h"
#include "i810tex.h"
#include "i810state.h"
#include "i810ioctl.h"


void i810DestroyTexObj(i810ContextPtr imesa, i810TextureObjectPtr t)
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
         imesa->dirty &= ~I810_UPLOAD_TEX0;
      }

      if (imesa->CurrentTexObj[1] == t) {
         imesa->CurrentTexObj[1] = 0;
         imesa->dirty &= ~I810_UPLOAD_TEX1;
      }
   }

   remove_from_list(t);
   free(t);
}


void i810SwapOutTexObj(i810ContextPtr imesa, i810TextureObjectPtr t)
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
static void i810UploadTexLevel( i810TextureObjectPtr t, int level )
{
   const struct gl_texture_image *image = t->image[level].image;
   int i,j;

   switch (t->image[level].internalFormat) {
   case GL_RGB:
   {
      GLushort *dst = (GLushort *)(t->BufAddr + t->image[level].offset);
      GLubyte  *src = (GLubyte *)image->Data;

      for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	 for (i = 0 ; i < image->Width ; i++) {
	    dst[i] = PACK_COLOR_565( src[0], src[1], src[2] );
	    src += 3;
	 }
      }
   }
   break;

   case GL_RGBA:
   {
      GLushort *dst = (GLushort *)(t->BufAddr + t->image[level].offset);
      GLubyte  *src = (GLubyte *)image->Data;

      for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	 for (i = 0 ; i < image->Width ; i++) {
	    dst[i] = PACK_COLOR_4444( src[3], src[0], src[1], src[2] );
	    src += 4;
	 }
      }
   }
   break;

   case GL_LUMINANCE:
   {
      GLushort *dst = (GLushort *)(t->BufAddr + t->image[level].offset);
      GLubyte  *src = (GLubyte *)image->Data;

      for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	 for (i = 0 ; i < image->Width ; i++) {
	    dst[i] = PACK_COLOR_565( src[0], src[0], src[0] );
	    src ++;
	 }
      }
   }
   break;

   case GL_INTENSITY:
   {
      GLushort *dst = (GLushort *)(t->BufAddr + t->image[level].offset);
      GLubyte  *src = (GLubyte *)image->Data;
      int i;

      for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	 for (i = 0 ; i < image->Width ; i++) {
	    dst[i] = PACK_COLOR_4444( src[0], src[0], src[0], src[0] );
	    src ++;
	 }
      }
   }
   break;

   case GL_LUMINANCE_ALPHA:
   {
      GLushort *dst = (GLushort *)(t->BufAddr + t->image[level].offset);
      GLubyte  *src = (GLubyte *)image->Data;

      for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	 for (i = 0 ; i < image->Width ; i++) {
	    dst[i] = PACK_COLOR_4444( src[1], src[0], src[0], src[0] );
	    src += 2;
	 }
      }
   }
   break;

   case GL_ALPHA:
   {
      GLushort *dst = (GLushort *)(t->BufAddr + t->image[level].offset);
      GLubyte  *src = (GLubyte *)image->Data;

      for (j = 0 ; j < image->Height ; j++, dst += (t->Pitch/2)) {
	 for (i = 0 ; i < image->Width ; i++) {
	    dst[i] = PACK_COLOR_4444( src[0], 255, 255, 255 );
	    src += 1;
	 }
      }
   }
   break;

   /* TODO: Translate color indices *now*:
    */
   case GL_COLOR_INDEX:
      {
	 GLubyte *dst = (GLubyte *)(t->BufAddr + t->image[level].offset);
	 GLubyte *src = (GLubyte *)image->Data;

	 for (j = 0 ; j < image->Height ; j++, dst += t->Pitch) {
	    for (i = 0 ; i < image->Width ; i++) {
	       dst[i] = src[0];
	       src += 1;
	    }
	 }
      }
   break;

   default:
      fprintf(stderr, "Not supported texture format %s\n",
              _mesa_lookup_enum_by_nr(image->Format));
   }
}



void i810PrintLocalLRU( i810ContextPtr imesa )
{
   i810TextureObjectPtr t;
   int sz = 1 << (imesa->i810Screen->logTextureGranularity);

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

void i810PrintGlobalLRU( i810ContextPtr imesa )
{
   int i, j;
   I810TexRegionRec *list = imesa->sarea->texList;

   for (i = 0, j = I810_NR_TEX_REGIONS ; i < I810_NR_TEX_REGIONS ; i++) {
      fprintf(stderr, "list[%d] age %d next %d prev %d\n",
	      j, list[j].age, list[j].next, list[j].prev);
      j = list[j].next;
      if (j == I810_NR_TEX_REGIONS) break;
   }

   if (j != I810_NR_TEX_REGIONS)
      fprintf(stderr, "Loop detected in global LRU\n");
}


void i810ResetGlobalLRU( i810ContextPtr imesa )
{
   I810TexRegionRec *list = imesa->sarea->texList;
   int sz = 1 << imesa->i810Screen->logTextureGranularity;
   int i;

   /* (Re)initialize the global circular LRU list.  The last element
    * in the array (I810_NR_TEX_REGIONS) is the sentinal.  Keeping it
    * at the end of the array allows it to be addressed rationally
    * when looking up objects at a particular location in texture
    * memory.
    */
   for (i = 0 ; (i+1) * sz <= imesa->i810Screen->textureSize ; i++) {
      list[i].prev = i-1;
      list[i].next = i+1;
      list[i].age = 0;
   }

   i--;
   list[0].prev = I810_NR_TEX_REGIONS;
   list[i].prev = i-1;
   list[i].next = I810_NR_TEX_REGIONS;
   list[I810_NR_TEX_REGIONS].prev = i;
   list[I810_NR_TEX_REGIONS].next = 0;
   imesa->sarea->texAge = 0;
}


void i810UpdateTexLRU( i810ContextPtr imesa, i810TextureObjectPtr t )
{
   int i;
   int logsz = imesa->i810Screen->logTextureGranularity;
   int start = t->MemBlock->ofs >> logsz;
   int end = (t->MemBlock->ofs + t->MemBlock->size - 1) >> logsz;
   I810TexRegionRec *list = imesa->sarea->texList;

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
      list[i].prev = I810_NR_TEX_REGIONS;
      list[i].next = list[I810_NR_TEX_REGIONS].next;
      list[(unsigned)list[I810_NR_TEX_REGIONS].next].prev = i;
      list[I810_NR_TEX_REGIONS].next = i;
   }
}


/* Called for every shared texture region which has increased in age
 * since we last held the lock.
 *
 * Figures out which of our textures have been ejected by other clients,
 * and pushes a placeholder texture onto the LRU list to represent
 * the other client's textures.
 */
void i810TexturesGone( i810ContextPtr imesa,
		       GLuint offset,
		       GLuint size,
		       GLuint in_use )
{
   i810TextureObjectPtr t, tmp;

   foreach_s ( t, tmp, &imesa->TexObjList ) {

      if (t->MemBlock->ofs >= offset + size ||
	  t->MemBlock->ofs + t->MemBlock->size <= offset)
	 continue;

      /* It overlaps - kick it off.  Need to hold onto the currently bound
       * objects, however.
       */
      i810SwapOutTexObj( imesa, t );
   }

   if (in_use) {
      t = (i810TextureObjectPtr) calloc(1,sizeof(*t));
      if (!t) return;

      t->MemBlock = mmAllocMem( imesa->texHeap, size, 0, offset);
      insert_at_head( &imesa->TexObjList, t );
   }
}





/* This is called with the lock held.  May have to eject our own and/or
 * other client's texture objects to make room for the upload.
 */
void i810UploadTexImages( i810ContextPtr imesa, i810TextureObjectPtr t )
{
   int i;
   int ofs;
   int numLevels;

   LOCK_HARDWARE( imesa );

   /* Do we need to eject LRU texture objects?
    */
   if (!t->MemBlock) {
      while (1)
      {
	 t->MemBlock = mmAllocMem( imesa->texHeap, t->totalSize, 12, 0 );
	 if (t->MemBlock)
	    break;

	 if (imesa->TexObjList.prev == imesa->CurrentTexObj[0] ||
	     imesa->TexObjList.prev == imesa->CurrentTexObj[1]) {
  	    fprintf(stderr, "Hit bound texture in upload\n");
	    i810PrintLocalLRU( imesa );
	    return;
	 }

	 if (imesa->TexObjList.prev == &(imesa->TexObjList)) {
 	    fprintf(stderr, "Failed to upload texture, sz %d\n", t->totalSize);
	    mmDumpMemInfo( imesa->texHeap );
	    return;
	 }

	 i810SwapOutTexObj( imesa, imesa->TexObjList.prev );
      }

      ofs = t->MemBlock->ofs;
      t->BufAddr = imesa->i810Screen->tex.map + ofs;
      t->Setup[I810_TEXREG_MI3] = imesa->i810Screen->textureOffset + ofs;

      if (t == imesa->CurrentTexObj[0])
	 I810_STATECHANGE(imesa, I810_UPLOAD_TEX0);

      if (t == imesa->CurrentTexObj[1])
	 I810_STATECHANGE(imesa, I810_UPLOAD_TEX1);

      i810UpdateTexLRU( imesa, t );
   }

   if (imesa->dirtyAge >= GET_DISPATCH_AGE(imesa))
      i810WaitAgeLocked( imesa, imesa->dirtyAge );

   numLevels = t->lastLevel - t->firstLevel + 1;
   for (i = 0 ; i < numLevels ; i++)
      if (t->dirty_images & (1<<i))
	 i810UploadTexLevel( t, i );

   t->dirty_images = 0;

   UNLOCK_HARDWARE( imesa );
}
