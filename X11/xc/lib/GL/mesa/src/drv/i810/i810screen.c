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
/* $XFree86: xc/lib/GL/mesa/src/drv/i810/i810screen.c,v 1.2 2002/10/30 12:51:33 alanh Exp $ */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */


#include <X11/Xlibint.h>
#include <stdio.h>

#include "glheader.h"
#include "context.h"
#include "matrix.h"
#include "simple_list.h"

#include "i810screen.h"
#include "i810_dri.h"

#include "i810state.h"
#include "i810tex.h"
#include "i810span.h"
#include "i810tris.h"
#include "i810ioctl.h"



/*  static int i810_malloc_proxy_buf(drmBufMapPtr buffers) */
/*  { */
/*     char *buffer; */
/*     drmBufPtr buf; */
/*     int i; */

/*     buffer = Xmalloc(I810_DMA_BUF_SZ); */
/*     if(buffer == NULL) return -1; */
/*     for(i = 0; i < I810_DMA_BUF_NR; i++) { */
/*        buf = &(buffers->list[i]); */
/*        buf->address = (drmAddress)buffer; */
/*     } */
/*     return 0; */
/*  } */

static drmBufMapPtr i810_create_empty_buffers(void)
{
   drmBufMapPtr retval;

   retval = (drmBufMapPtr)Xmalloc(sizeof(drmBufMap));
   if(retval == NULL) return NULL;
   memset(retval, 0, sizeof(drmBufMap));
   retval->list = (drmBufPtr)Xmalloc(sizeof(drmBuf) * I810_DMA_BUF_NR);
   if(retval->list == NULL) {
      Xfree(retval);
      return NULL;
   }
   memset(retval->list, 0, sizeof(drmBuf) * I810_DMA_BUF_NR);
   return retval;
}


static GLboolean
i810InitDriver(__DRIscreenPrivate *sPriv)
{
   i810ScreenPrivate *i810Screen;
   I810DRIPtr         gDRIPriv = (I810DRIPtr)sPriv->pDevPriv;

   /* Check the DRI version */
   {
      int major, minor, patch;
      if (XF86DRIQueryVersion(sPriv->display, &major, &minor, &patch)) {
         if (major != 4 || minor < 0) {
            __driUtilMessage("i810 DRI driver expected DRI version 4.0.x but got version %d.%d.%d", major, minor, patch);
            return GL_FALSE;
         }
      }
   }

   /* Check that the DDX driver version is compatible */
   if (sPriv->ddxMajor != 1 ||
       sPriv->ddxMinor < 0) {
      __driUtilMessage("i810 DRI driver expected DDX driver version 1.0.x but got version %d.%d.%d", sPriv->ddxMajor, sPriv->ddxMinor, sPriv->ddxPatch);
      return GL_FALSE;
   }

   /* Check that the DRM driver version is compatible */
   if (sPriv->drmMajor != 1 ||
       sPriv->drmMinor < 2) {
      __driUtilMessage("i810 DRI driver expected DRM driver version 1.2.x but got version %d.%d.%d", sPriv->drmMajor, sPriv->drmMinor, sPriv->drmPatch);
      return GL_FALSE;
   }

   /* Allocate the private area */
   i810Screen = (i810ScreenPrivate *)Xmalloc(sizeof(i810ScreenPrivate));
   if (!i810Screen) {
      __driUtilMessage("i810InitDriver: alloc i810ScreenPrivate struct failed");
      return GL_FALSE;
   }

   i810Screen->driScrnPriv = sPriv;
   sPriv->private = (void *)i810Screen;

   i810Screen->deviceID=gDRIPriv->deviceID;
   i810Screen->width=gDRIPriv->width;
   i810Screen->height=gDRIPriv->height;
   i810Screen->mem=gDRIPriv->mem;
   i810Screen->cpp=gDRIPriv->cpp;
   i810Screen->fbStride=gDRIPriv->fbStride;
   i810Screen->fbOffset=gDRIPriv->fbOffset;

   if (gDRIPriv->bitsPerPixel == 15)
      i810Screen->fbFormat = DV_PF_555;
   else
      i810Screen->fbFormat = DV_PF_565;

   i810Screen->backOffset=gDRIPriv->backOffset;
   i810Screen->depthOffset=gDRIPriv->depthOffset;
   i810Screen->backPitch = gDRIPriv->auxPitch;
   i810Screen->backPitchBits = gDRIPriv->auxPitchBits;
   i810Screen->textureOffset=gDRIPriv->textureOffset;
   i810Screen->textureSize=gDRIPriv->textureSize;
   i810Screen->logTextureGranularity = gDRIPriv->logTextureGranularity;

   i810Screen->bufs = i810_create_empty_buffers();
   if (i810Screen->bufs == NULL) {
      __driUtilMessage("i810InitDriver: i810_create_empty_buffers() failed");
      Xfree(i810Screen);
      return GL_FALSE;
   }

   i810Screen->back.handle = gDRIPriv->backbuffer;
   i810Screen->back.size = gDRIPriv->backbufferSize;

   if (drmMap(sPriv->fd,
	      i810Screen->back.handle,
	      i810Screen->back.size,
	      (drmAddress *)&i810Screen->back.map) != 0) {
      Xfree(i810Screen);
      sPriv->private = NULL;
      __driUtilMessage("i810InitDriver: drmMap failed");
      return GL_FALSE;
   }

   i810Screen->depth.handle = gDRIPriv->depthbuffer;
   i810Screen->depth.size = gDRIPriv->depthbufferSize;

   if (drmMap(sPriv->fd,
	      i810Screen->depth.handle,
	      i810Screen->depth.size,
	      (drmAddress *)&i810Screen->depth.map) != 0) {
      Xfree(i810Screen);
      drmUnmap(i810Screen->back.map, i810Screen->back.size);
      sPriv->private = NULL;
      __driUtilMessage("i810InitDriver: drmMap (2) failed");
      return GL_FALSE;
   }

   i810Screen->tex.handle = gDRIPriv->textures;
   i810Screen->tex.size = gDRIPriv->textureSize;

   if (drmMap(sPriv->fd,
	      i810Screen->tex.handle,
	      i810Screen->tex.size,
	      (drmAddress *)&i810Screen->tex.map) != 0) {
      Xfree(i810Screen);
      drmUnmap(i810Screen->back.map, i810Screen->back.size);
      drmUnmap(i810Screen->depth.map, i810Screen->depth.size);
      sPriv->private = NULL;
      __driUtilMessage("i810InitDriver: drmMap (3) failed");
      return GL_FALSE;
   }

   i810Screen->sarea_priv_offset = gDRIPriv->sarea_priv_offset;

   return GL_TRUE;
}

static void
i810DestroyScreen(__DRIscreenPrivate *sPriv)
{
   i810ScreenPrivate *i810Screen = (i810ScreenPrivate *)sPriv->private;

   /* Need to unmap all the bufs and maps here:
    */
   drmUnmap(i810Screen->back.map, i810Screen->back.size);
   drmUnmap(i810Screen->depth.map, i810Screen->depth.size);
   drmUnmap(i810Screen->tex.map, i810Screen->tex.size);

   Xfree(i810Screen);
   sPriv->private = NULL;
}


static GLboolean
i810CreateBuffer( Display *dpy,
                  __DRIscreenPrivate *driScrnPriv,
                  __DRIdrawablePrivate *driDrawPriv,
                  const __GLcontextModes *mesaVis,
                  GLboolean isPixmap )
{
   if (isPixmap) {
      return GL_FALSE; /* not implemented */
   }
   else {
      driDrawPriv->driverPrivate = (void *)
         _mesa_create_framebuffer(mesaVis,
                                  GL_FALSE,  /* software depth buffer? */
                                  mesaVis->stencilBits > 0,
                                  mesaVis->accumRedBits > 0,
                                  GL_FALSE /* s/w alpha planes */);
      return (driDrawPriv->driverPrivate != NULL);
   }
}


static void
i810DestroyBuffer(__DRIdrawablePrivate *driDrawPriv)
{
   _mesa_destroy_framebuffer((GLframebuffer *) (driDrawPriv->driverPrivate));
}


#if 0
/* Initialize the fullscreen mode.
 */
GLboolean
XMesaOpenFullScreen( __DRIcontextPrivate *driContextPriv )
{
   i810ContextPtr imesa = (i810ContextPtr)driContextPriv->driverPrivate;
   imesa->doPageFlip = 1;
   imesa->currentPage = 0;
   return GL_TRUE;
}

/* Shut down the fullscreen mode.
 */
GLboolean
XMesaCloseFullScreen( __DRIcontextPrivate *driContextPriv )
{
   i810ContextPtr imesa = (i810ContextPtr)driContextPriv->driverPrivate;

   if (imesa->currentPage == 1) {
      /* Move the frontbuffer image to page zero? */
/*        i810SwapBuffers( imesa ); */
      i810PageFlip( imesa );
      imesa->currentPage = 0;
   }

   imesa->doPageFlip = GL_FALSE;
   imesa->Setup[I810_DESTREG_DI0] = imesa->driScreen->front_offset;
   return GL_TRUE;
}

#else

static GLboolean
i810OpenFullScreen(__DRIcontextPrivate *driContextPriv)
{
    return GL_TRUE;
}

static GLboolean
i810CloseFullScreen(__DRIcontextPrivate *driContextPriv)
{
    return GL_TRUE;
}

#endif


static struct __DriverAPIRec i810API = {
   i810InitDriver,
   i810DestroyScreen,
   i810CreateContext,
   i810DestroyContext,
   i810CreateBuffer,
   i810DestroyBuffer,
   i810SwapBuffers,
   i810MakeCurrent,
   i810UnbindContext,
   i810OpenFullScreen,
   i810CloseFullScreen
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
   psp = __driUtilCreateScreen(dpy, scrn, psc, numConfigs, config, &i810API);
   return (void *) psp;
}
