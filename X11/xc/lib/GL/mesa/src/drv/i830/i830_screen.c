/**************************************************************************
 * 
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * **************************************************************************/
/* $XFree86: xc/lib/GL/mesa/src/drv/i830/i830_screen.c,v 1.3 2002/12/10 01:26:53 dawes Exp $ */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 * Adapted for use on the I830M:
 *   Jeff Hartmann <jhartmann@2d3d.com>
 */


#include <X11/Xlibint.h>
#include <stdio.h>


#include <X11/Xlibint.h>
#include <stdio.h>

#include "glheader.h"
#include "context.h"
#include "matrix.h"
#include "simple_list.h"

#include "i830_screen.h"
#include "i830_dri.h"

#include "i830_state.h"
#include "i830_tex.h"
#include "i830_span.h"
#include "i830_tris.h"
#include "i830_ioctl.h"

#include "i830_dri.h"


static int i830_malloc_proxy_buf(drmBufMapPtr buffers)
{
   char *buffer;
   drmBufPtr buf;
   int i;

   buffer = Xmalloc(I830_DMA_BUF_SZ);
   if(buffer == NULL) return -1;
   for(i = 0; i < I830_DMA_BUF_NR; i++) {
      buf = &(buffers->list[i]);
      buf->address = (drmAddress)buffer;
   }

   return 0;
}

static drmBufMapPtr i830_create_empty_buffers(void)
{
   drmBufMapPtr retval;

   retval = (drmBufMapPtr)Xmalloc(sizeof(drmBufMap));
   if(retval == NULL) return NULL;
   memset(retval, 0, sizeof(drmBufMap));
   retval->list = (drmBufPtr)Xmalloc(sizeof(drmBuf) * I830_DMA_BUF_NR);
   if(retval->list == NULL) {
      Xfree(retval);
      return NULL;
   }

   memset(retval->list, 0, sizeof(drmBuf) * I830_DMA_BUF_NR);
   return retval;
}

static void i830PrintDRIInfo(i830ScreenPrivate *i830Screen,
			     __DRIscreenPrivate *sPriv,
			     I830DRIPtr gDRIPriv)
{
   GLuint size = (gDRIPriv->ringSize +
		  i830Screen->textureSize +
		  i830Screen->depth.size +
		  i830Screen->back.size +
		  sPriv->fbSize +
		  I830_DMA_BUF_NR * I830_DMA_BUF_SZ +
		  32768 /* Context Memory */ +
		  16*4096 /* Ring buffer */ +
		  64*1024 /* Scratch buffer */ +
		  4096 /* Cursor */);
   GLuint size_low = (gDRIPriv->ringSize +
		      i830Screen->textureSize +
		      sPriv->fbSize +
		      I830_DMA_BUF_NR * I830_DMA_BUF_SZ +
		      32768 /* Context Memory */ +
		      16*4096 /* Ring buffer */ +
		      64*1024 /* Scratch buffer */);

   fprintf(stderr, "\nFront size : 0x%x\n", sPriv->fbSize);
   fprintf(stderr, "Front offset : 0x%x\n", i830Screen->fbOffset);
   fprintf(stderr, "Back size : 0x%x\n", i830Screen->back.size);
   fprintf(stderr, "Back offset : 0x%x\n", i830Screen->backOffset);
   fprintf(stderr, "Depth size : 0x%x\n", i830Screen->depth.size);
   fprintf(stderr, "Depth offset : 0x%x\n", i830Screen->depthOffset);
   fprintf(stderr, "Texture size : 0x%x\n", i830Screen->textureSize);
   fprintf(stderr, "Texture offset : 0x%x\n", i830Screen->textureOffset);
   fprintf(stderr, "Ring offset : 0x%x\n", gDRIPriv->ringOffset);
   fprintf(stderr, "Ring size : 0x%x\n", gDRIPriv->ringSize);
   fprintf(stderr, "Memory : 0x%x\n", gDRIPriv->mem);
   fprintf(stderr, "Used Memory : low(0x%x) high(0x%x)\n", size_low, size);
}

static GLboolean i830InitDriver(__DRIscreenPrivate *sPriv)
{
   i830ScreenPrivate *i830Screen;
   I830DRIPtr         gDRIPriv = (I830DRIPtr)sPriv->pDevPriv;

   /* Check the DRI version */
   {
      int major, minor, patch;
      if (XF86DRIQueryVersion(sPriv->display, &major, &minor, &patch)) {
	 if (major != 4 || minor < 0) {
	    __driUtilMessage("i830 DRI driver expected DRI version 4.0.x but got version %d.%d.%d", major, minor, patch);
	    return GL_FALSE;
	 }
      }
   }
   
   /* Check that the DDX driver version is compatible */
   if (sPriv->ddxMajor != 1 || sPriv->ddxMinor < 0) {
      __driUtilMessage("i830 DRI driver expected DDX driver version 1.0.x but got version %d.%d.%d", sPriv->ddxMajor, sPriv->ddxMinor, sPriv->ddxPatch);
      return GL_FALSE;
   }
		
   /* Check that the DRM driver version is compatible */
   if (sPriv->drmMajor != 1 || sPriv->drmMinor < 3) {
      __driUtilMessage("i830 DRI driver expected DRM driver version 1.3.x but got version %d.%d.%d", sPriv->drmMajor, sPriv->drmMinor, sPriv->drmPatch);
      return GL_FALSE;
   }

   /* Allocate the private area */
   i830Screen = (i830ScreenPrivate *)Xmalloc(sizeof(i830ScreenPrivate));
   if (!i830Screen) {
      fprintf(stderr,"\nERROR!  Allocating private area failed\n");
      return GL_FALSE;
   }

   i830Screen->driScrnPriv = sPriv;
   sPriv->private = (void *)i830Screen;

   i830Screen->deviceID = gDRIPriv->deviceID;
   i830Screen->width = gDRIPriv->width;
   i830Screen->height = gDRIPriv->height;
   i830Screen->mem = gDRIPriv->mem;
   i830Screen->cpp = gDRIPriv->cpp;
   i830Screen->fbStride = gDRIPriv->fbStride;
   i830Screen->fbOffset = gDRIPriv->fbOffset;
			 
   switch (gDRIPriv->bitsPerPixel) {
   case 15: i830Screen->fbFormat = DV_PF_555; break;
   case 16: i830Screen->fbFormat = DV_PF_565; break;
   case 32: i830Screen->fbFormat = DV_PF_8888; break;
   }
			 
   i830Screen->backOffset = gDRIPriv->backOffset;
   i830Screen->depthOffset = gDRIPriv->depthOffset;
   i830Screen->backPitch = gDRIPriv->auxPitch;
   i830Screen->backPitchBits = gDRIPriv->auxPitchBits;
   i830Screen->textureOffset = gDRIPriv->textureOffset;
   i830Screen->textureSize = gDRIPriv->textureSize;
   i830Screen->logTextureGranularity = gDRIPriv->logTextureGranularity;
			 			    			 

   i830Screen->bufs = i830_create_empty_buffers();
   if(i830Screen->bufs == NULL) {
      fprintf(stderr,"\nERROR: Failed to create empty buffers in %s \n",
	      __FUNCTION__);
      Xfree(i830Screen);
      return GL_FALSE;
   }

   /* Check if you need to create a fake buffer */
   if(i830_check_copy(sPriv->fd) == 1) {
      i830_malloc_proxy_buf(i830Screen->bufs);
      i830Screen->use_copy_buf = 1;
   } else {
      i830Screen->use_copy_buf = 0;
   }

   i830Screen->back.handle = gDRIPriv->backbuffer;
   i830Screen->back.size = gDRIPriv->backbufferSize;
			 
   if (drmMap(sPriv->fd,
	      i830Screen->back.handle,
	      i830Screen->back.size,
	      (drmAddress *)&i830Screen->back.map) != 0) {
      fprintf(stderr, "\nERROR: line %d, Function %s, File %s\n",
	      __LINE__, __FUNCTION__, __FILE__);
      Xfree(i830Screen);
      sPriv->private = NULL;
      return GL_FALSE;
   }

   i830Screen->depth.handle = gDRIPriv->depthbuffer;
   i830Screen->depth.size = gDRIPriv->depthbufferSize;

   if (drmMap(sPriv->fd, 
	      i830Screen->depth.handle,
	      i830Screen->depth.size,
	      (drmAddress *)&i830Screen->depth.map) != 0) {
      fprintf(stderr, "\nERROR: line %d, Function %s, File %s\n", 
	      __LINE__, __FUNCTION__, __FILE__);
      Xfree(i830Screen);
      drmUnmap(i830Screen->back.map, i830Screen->back.size);
      sPriv->private = NULL;
      return GL_FALSE;
   }

   i830Screen->tex.handle = gDRIPriv->textures;
   i830Screen->tex.size = gDRIPriv->textureSize;

   if (drmMap(sPriv->fd,
	      i830Screen->tex.handle,
	      i830Screen->tex.size,
	      (drmAddress *)&i830Screen->tex.map) != 0) {
      fprintf(stderr, "\nERROR: line %d, Function %s, File %s\n",
	      __LINE__, __FUNCTION__, __FILE__);
      Xfree(i830Screen);
      drmUnmap(i830Screen->back.map, i830Screen->back.size);
      drmUnmap(i830Screen->depth.map, i830Screen->depth.size);
      sPriv->private = NULL;
      return GL_FALSE;
   }
			 
   i830Screen->sarea_priv_offset = gDRIPriv->sarea_priv_offset;
   
   if (0) i830PrintDRIInfo(i830Screen, sPriv, gDRIPriv);

   i830Screen->drmMinor = sPriv->drmMinor;

   if (sPriv->drmMinor >= 3) {
      int ret;
      drmI830GetParam gp;

      gp.param = I830_PARAM_IRQ_ACTIVE;
      gp.value = &i830Screen->irq_active;

      ret = drmCommandWriteRead( sPriv->fd, DRM_I830_GETPARAM,
				 &gp, sizeof(gp));
      if (ret) {
	 fprintf(stderr, "drmI830GetParam: %d\n", ret);
	 return GL_FALSE;
      }
   }

   return GL_TRUE;
}
		
		
static void i830DestroyScreen(__DRIscreenPrivate *sPriv)
{
   i830ScreenPrivate *i830Screen = (i830ScreenPrivate *)sPriv->private;

   /* Need to unmap all the bufs and maps here:
    */
   drmUnmap(i830Screen->back.map, i830Screen->back.size);
   drmUnmap(i830Screen->depth.map, i830Screen->depth.size);
   drmUnmap(i830Screen->tex.map, i830Screen->tex.size);
   Xfree(i830Screen);
   sPriv->private = NULL;
}

static GLboolean i830CreateBuffer( Display *dpy,
				  __DRIscreenPrivate *driScrnPriv,
				  __DRIdrawablePrivate *driDrawPriv,
				  const __GLcontextModes *mesaVis,
				  GLboolean isPixmap )
{
   if (isPixmap) {
      return GL_FALSE; /* not implemented */
   } else {
#if 0
      GLboolean swStencil = (mesaVis->stencilBits > 0 && 
			     mesaVis->depthBits != 24);
#else
      GLboolean swStencil = mesaVis->stencilBits > 0;
#endif
      driDrawPriv->driverPrivate = (void *) 
	 _mesa_create_framebuffer(mesaVis,
				  GL_FALSE,  /* software depth buffer? */
				  swStencil,
				  mesaVis->accumRedBits > 0,
				  GL_FALSE /* s/w alpha planes */);
      
      return (driDrawPriv->driverPrivate != NULL);
   }
}

static void i830DestroyBuffer(__DRIdrawablePrivate *driDrawPriv)
{
   _mesa_destroy_framebuffer((GLframebuffer *) (driDrawPriv->driverPrivate));
}

static GLboolean i830OpenFullScreen (__DRIcontextPrivate *driContextPriv)
{
   return GL_TRUE;  
}

static GLboolean i830CloseFullScreen (__DRIcontextPrivate *driContextPriv)
{
   return GL_TRUE;  
}

static struct __DriverAPIRec i830API = {
   i830InitDriver,
   i830DestroyScreen,
   i830CreateContext,
   i830DestroyContext,
   i830CreateBuffer,
   i830DestroyBuffer,
   i830SwapBuffers,
   i830MakeCurrent,
   i830UnbindContext,
   i830OpenFullScreen,
   i830CloseFullScreen
};

/*
 * This is the bootstrap function for the driver.
 * The __driCreateScreen name is the symbol that libGL.so fetches.
 * Return:  pointer to a __DRIscreenPrivate.
 */
void *__driCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
			int numConfigs, __GLXvisualConfig *config)
{
   __DRIscreenPrivate *psp;
   psp = __driUtilCreateScreen(dpy, scrn, psc, numConfigs, config, &i830API);
   return (void *) psp;
}
	     

/* This function is called by libGL.so as soon as libGL.so is loaded.
 * This is where we'd register new extension functions with the dispatcher.
 *
 * Note: Most of these are probably already registered - just doing
 * this for the benefit of old libGL.so's out there.
 */
#include "glapioffsets.h"

void __driRegisterExtensions( void )
{
   int i;
   static struct { const char *name; int offset; } funcs[] = {
	{ "glSecondaryColor3bEXT", _gloffset_SecondaryColor3bEXT },
	{ "glSecondaryColor3dEXT", _gloffset_SecondaryColor3dEXT },
	{ "glSecondaryColor3fEXT", _gloffset_SecondaryColor3fEXT },
	{ "glSecondaryColor3iEXT", _gloffset_SecondaryColor3iEXT },
	{ "glSecondaryColor3sEXT", _gloffset_SecondaryColor3sEXT },
	{ "glSecondaryColor3ubEXT", _gloffset_SecondaryColor3ubEXT },
	{ "glSecondaryColor3uiEXT", _gloffset_SecondaryColor3uiEXT },
	{ "glSecondaryColor3usEXT", _gloffset_SecondaryColor3usEXT },
	{ "glSecondaryColor3bvEXT", _gloffset_SecondaryColor3bvEXT },
	{ "glSecondaryColor3dvEXT", _gloffset_SecondaryColor3dvEXT },
	{ "glSecondaryColor3fvEXT", _gloffset_SecondaryColor3fvEXT },
	{ "glSecondaryColor3ivEXT", _gloffset_SecondaryColor3ivEXT },
	{ "glSecondaryColor3svEXT", _gloffset_SecondaryColor3svEXT },
	{ "glSecondaryColor3ubvEXT", _gloffset_SecondaryColor3ubvEXT },
	{ "glSecondaryColor3uivEXT", _gloffset_SecondaryColor3uivEXT },
	{ "glSecondaryColor3usvEXT", _gloffset_SecondaryColor3usvEXT },
	{ "glSecondaryColorPointerEXT", _gloffset_SecondaryColorPointerEXT }
   };

   for (i = 0 ; i < sizeof(funcs) / sizeof(*funcs) ; i++ ) 
      _glapi_add_entrypoint( funcs[i].name, funcs[i].offset );
}

