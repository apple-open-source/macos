/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_screen.c,v 1.2 2002/12/16 16:18:54 dawes Exp $ */
/*
Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.

The Weather Channel (TM) funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86 license.
This notice must be preserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#include <dlfcn.h>
#include <stdlib.h>

#include "r200_screen.h"
#include "r200_context.h"
#include "r200_ioctl.h"

#include "mem.h"
#include "context.h"

#if 1
/* Including xf86PciInfo.h introduces a bunch of errors...
 */
#define PCI_CHIP_R200_QD	0x5144
#define PCI_CHIP_R200_QE	0x5145
#define PCI_CHIP_R200_QF	0x5146
#define PCI_CHIP_R200_QG	0x5147
#define PCI_CHIP_R200_QY	0x5159
#define PCI_CHIP_R200_QZ	0x515A
#define PCI_CHIP_R200_LW	0x4C57 
#define PCI_CHIP_R200_LY	0x4C59
#define PCI_CHIP_R200_LZ	0x4C5A
#define PCI_CHIP_RV200_QW	0x5157 
#endif

static r200ScreenPtr __r200Screen;

/* Create the device specific screen private data struct.
 */
static r200ScreenPtr 
r200CreateScreen( __DRIscreenPrivate *sPriv )
{
   r200ScreenPtr r200Screen;
   RADEONDRIPtr r200DRIPriv = (RADEONDRIPtr)sPriv->pDevPriv;

   /* Check the DRI version */
   {
      int major, minor, patch;
      if ( XF86DRIQueryVersion( sPriv->display, &major, &minor, &patch ) ) {
         if ( major != 4 || minor < 0 ) {
            __driUtilMessage( "R200 DRI driver expected DRI version 4.0.x "
			      "but got version %d.%d.%d",
			      major, minor, patch );
            return NULL;
         }
      }
   }

   /* Check that the DDX driver version is compatible */
   if ( sPriv->ddxMajor != 4 ||
	sPriv->ddxMinor < 0 ) {
      __driUtilMessage( "R200 DRI driver expected DDX driver version 4.0.x "
			"but got version %d.%d.%d", 
			sPriv->ddxMajor, sPriv->ddxMinor, sPriv->ddxPatch );
      return NULL;
   }

   /* Check that the DRM driver version is compatible 
    *    -- R200 support added at 1.5.0.
    */
   if ( sPriv->drmMajor != 1 ||
	sPriv->drmMinor < 5) {
      __driUtilMessage( "R200 DRI driver expected DRM driver version 1.5.x "
			"but got version %d.%d.%d", 
			sPriv->drmMajor, sPriv->drmMinor, sPriv->drmPatch );
      return NULL;
   }



   /* Allocate the private area */
   r200Screen = (r200ScreenPtr) CALLOC( sizeof(*r200Screen) );
   if ( !r200Screen ) {
      __driUtilMessage("%s: CALLOC r200Screen struct failed",
		       __FUNCTION__);
      return NULL;
   }


   switch ( r200DRIPriv->deviceID ) {
   case PCI_CHIP_R200_QD:
   case PCI_CHIP_R200_QE:
   case PCI_CHIP_R200_QF:
   case PCI_CHIP_R200_QG:
   case PCI_CHIP_R200_QY:
   case PCI_CHIP_R200_QZ:
   case PCI_CHIP_RV200_QW:
   case PCI_CHIP_R200_LW:
   case PCI_CHIP_R200_LY:
   case PCI_CHIP_R200_LZ:
      __driUtilMessage("r200CreateScreen(): Device isn't an r200!\n");
      FREE( r200Screen );
      return NULL;      
   default:
      r200Screen->chipset = R200_CHIPSET_R200;
      break;
   }


   /* This is first since which regions we map depends on whether or
    * not we are using a PCI card.
    */
   r200Screen->IsPCI = r200DRIPriv->IsPCI;

   {
      int ret;
      drmRadeonGetParam gp;

      gp.param = RADEON_PARAM_AGP_BUFFER_OFFSET;
      gp.value = &r200Screen->agp_buffer_offset;

      ret = drmCommandWriteRead( sPriv->fd, DRM_RADEON_GETPARAM,
				 &gp, sizeof(gp));
      if (ret) {
	 FREE( r200Screen );
	 fprintf(stderr, "drmR200GetParam: %d\n", ret);
	 return NULL;
      }

      r200Screen->agp_texture_offset = 
	 r200Screen->agp_buffer_offset + 2*1024*1024;


      if (sPriv->drmMinor >= 6) {
	 gp.param = RADEON_PARAM_AGP_BASE;
	 gp.value = &r200Screen->agp_base;

	 ret = drmCommandWriteRead( sPriv->fd, DRM_RADEON_GETPARAM,
				    &gp, sizeof(gp));
	 if (ret) {
	    FREE( r200Screen );
	    fprintf(stderr,
		    "drmR200GetParam (RADEON_PARAM_AGP_BUFFER_OFFSET): %d\n",
		    ret);
	    return NULL;
	 }
      }

      if (sPriv->drmMinor >= 6) {
	 gp.param = RADEON_PARAM_IRQ_NR;
	 gp.value = &r200Screen->irq;

	 ret = drmCommandWriteRead( sPriv->fd, DRM_RADEON_GETPARAM,
				    &gp, sizeof(gp));
	 if (ret) {
	    FREE( r200Screen );
	    fprintf(stderr, "drmR200GetParam (RADEON_PARAM_IRQ_NR): %d\n", ret);
	    return NULL;
	 }
      }

   }

   r200Screen->mmio.handle = r200DRIPriv->registerHandle;
   r200Screen->mmio.size   = r200DRIPriv->registerSize;
   if ( drmMap( sPriv->fd,
		r200Screen->mmio.handle,
		r200Screen->mmio.size,
		&r200Screen->mmio.map ) ) {
      FREE( r200Screen );
      __driUtilMessage("r200CreateScreen(): drmMap failed\n");
      return NULL;
   }

   r200Screen->status.handle = r200DRIPriv->statusHandle;
   r200Screen->status.size   = r200DRIPriv->statusSize;
   if ( drmMap( sPriv->fd,
		r200Screen->status.handle,
		r200Screen->status.size,
		&r200Screen->status.map ) ) {
      drmUnmap( r200Screen->mmio.map, r200Screen->mmio.size );
      FREE( r200Screen );
      __driUtilMessage("r200CreateScreen(): drmMap (2) failed\n");
      return NULL;
   }
   r200Screen->scratch = (__volatile__ CARD32 *)
      ((GLubyte *)r200Screen->status.map + RADEON_SCRATCH_REG_OFFSET);

   r200Screen->buffers = drmMapBufs( sPriv->fd );
   if ( !r200Screen->buffers ) {
      drmUnmap( r200Screen->status.map, r200Screen->status.size );
      drmUnmap( r200Screen->mmio.map, r200Screen->mmio.size );
      FREE( r200Screen );
      __driUtilMessage("r200CreateScreen(): drmMapBufs failed\n");
      return NULL;
   }

   if ( !r200Screen->IsPCI ) {
      r200Screen->agpTextures.handle = r200DRIPriv->agpTexHandle;
      r200Screen->agpTextures.size   = r200DRIPriv->agpTexMapSize;
      if ( drmMap( sPriv->fd,
		   r200Screen->agpTextures.handle,
		   r200Screen->agpTextures.size,
		   (drmAddressPtr)&r200Screen->agpTextures.map ) ) {
	 drmUnmapBufs( r200Screen->buffers );
	 drmUnmap( r200Screen->status.map, r200Screen->status.size );
	 drmUnmap( r200Screen->mmio.map, r200Screen->mmio.size );
	 FREE( r200Screen );
         __driUtilMessage("r200CreateScreen(): IsPCI failed\n");
	 return NULL;
      }
   }



   r200Screen->cpp = r200DRIPriv->bpp / 8;
   r200Screen->AGPMode = r200DRIPriv->AGPMode;

   r200Screen->frontOffset	= r200DRIPriv->frontOffset;
   r200Screen->frontPitch	= r200DRIPriv->frontPitch;
   r200Screen->backOffset	= r200DRIPriv->backOffset;
   r200Screen->backPitch	= r200DRIPriv->backPitch;
   r200Screen->depthOffset	= r200DRIPriv->depthOffset;
   r200Screen->depthPitch	= r200DRIPriv->depthPitch;

   r200Screen->texOffset[RADEON_CARD_HEAP] = r200DRIPriv->textureOffset;
   r200Screen->texSize[RADEON_CARD_HEAP] = r200DRIPriv->textureSize;
   r200Screen->logTexGranularity[RADEON_CARD_HEAP] =
      r200DRIPriv->log2TexGran;

   if ( r200Screen->IsPCI ) {
      r200Screen->numTexHeaps = RADEON_NR_TEX_HEAPS - 1;
      r200Screen->texOffset[RADEON_AGP_HEAP] = 0;
      r200Screen->texSize[RADEON_AGP_HEAP] = 0;
      r200Screen->logTexGranularity[RADEON_AGP_HEAP] = 0;
   } else {
      r200Screen->numTexHeaps = RADEON_NR_TEX_HEAPS;
      r200Screen->texOffset[RADEON_AGP_HEAP] =
	 r200DRIPriv->agpTexOffset + R200_AGP_TEX_OFFSET;
      r200Screen->texSize[RADEON_AGP_HEAP] = r200DRIPriv->agpTexMapSize;
      r200Screen->logTexGranularity[RADEON_AGP_HEAP] =
	 r200DRIPriv->log2AGPTexGran;
   }


   r200Screen->driScreen = sPriv;
   r200Screen->sarea_priv_offset = r200DRIPriv->sarea_priv_offset;
   return r200Screen;
}

/* Destroy the device specific screen private data struct.
 */
static void 
r200DestroyScreen( __DRIscreenPrivate *sPriv )
{
   r200ScreenPtr r200Screen = (r200ScreenPtr)sPriv->private;

   if (!r200Screen)
      return;

   if ( !r200Screen->IsPCI ) {
      drmUnmap( r200Screen->agpTextures.map,
		r200Screen->agpTextures.size );
   }
   drmUnmapBufs( r200Screen->buffers );
   drmUnmap( r200Screen->status.map, r200Screen->status.size );
   drmUnmap( r200Screen->mmio.map, r200Screen->mmio.size );

   FREE( r200Screen );
   sPriv->private = NULL;
}


/* Initialize the driver specific screen private data.
 */
static GLboolean
r200InitDriver( __DRIscreenPrivate *sPriv )
{
   __r200Screen = r200CreateScreen( sPriv );

   sPriv->private = (void *) __r200Screen;

   return sPriv->private ? GL_TRUE : GL_FALSE;
}



/* Create and initialize the Mesa and driver specific pixmap buffer
 * data.
 */
static GLboolean
r200CreateBuffer( Display *dpy,
                    __DRIscreenPrivate *driScrnPriv,
                    __DRIdrawablePrivate *driDrawPriv,
                    const __GLcontextModes *mesaVis,
                    GLboolean isPixmap )
{
   if (isPixmap) {
      return GL_FALSE; /* not implemented */
   }
   else {
      const GLboolean swDepth = GL_FALSE;
      const GLboolean swAlpha = GL_FALSE;
      const GLboolean swAccum = mesaVis->accumRedBits > 0;
      const GLboolean swStencil = mesaVis->stencilBits > 0 &&
         mesaVis->depthBits != 24;
      driDrawPriv->driverPrivate = (void *)
         _mesa_create_framebuffer( mesaVis,
                                   swDepth,
                                   swStencil,
                                   swAccum,
                                   swAlpha );
      return (driDrawPriv->driverPrivate != NULL);
   }
}


static void
r200DestroyBuffer(__DRIdrawablePrivate *driDrawPriv)
{
   _mesa_destroy_framebuffer((GLframebuffer *) (driDrawPriv->driverPrivate));
}




/* Fullscreen mode isn't used for much -- could be a way to shrink
 * front/back buffers & get more texture memory if the client has
 * changed the video resolution.
 * 
 * Pageflipping is now done automatically whenever there is a single
 * 3d client.
 */
static GLboolean
r200OpenCloseFullScreen( __DRIcontextPrivate *driContextPriv )
{
   return GL_TRUE;
}

static struct __DriverAPIRec r200API = {
   r200InitDriver,
   r200DestroyScreen,
   r200CreateContext,
   r200DestroyContext,
   r200CreateBuffer,
   r200DestroyBuffer,
   r200SwapBuffers,
   r200MakeCurrent,
   r200UnbindContext,
   r200OpenCloseFullScreen,
   r200OpenCloseFullScreen
};



/*
 * This is the bootstrap function for the driver.
 * The __driCreateScreen name is the symbol that libGL.so fetches.
 * Return:  pointer to a __DRIscreenPrivate.
 *
 */
void *__driCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
                        int numConfigs, __GLXvisualConfig *config)
{
   __DRIscreenPrivate *psp;
   psp = __driUtilCreateScreen(dpy, scrn, psc, numConfigs, config, &r200API);
   return (void *) psp;
}


/* This function is called by libGL.so as soon as libGL.so is loaded.
 * This is where we'd register new extension functions with the dispatcher.
 */
void
__driRegisterExtensions( void )
{
   /* dlopen ourself */
   void *dll = dlopen(NULL, RTLD_GLOBAL);
   if (dll) {
      typedef void *(*registerFunc)(const char *funcName, void *funcAddr);
      typedef void (*registerString)(const char *extName);

      /* Get pointers to libGL's __glXRegisterGLXFunction
       * and __glXRegisterGLXExtensionString, if they exist.
       */
      registerFunc regFunc = (registerFunc) dlsym(dll, "__glXRegisterGLXFunction");
      registerString regString = (registerString) dlsym(dll, "__glXRegisterGLXExtensionString");

      if (regFunc) {
         /* register our GLX extensions with libGL */
         void *p;
         p = regFunc("glXAllocateMemoryNV", (void *) r200AllocateMemoryNV);
         if (p)
            ;  /* XXX already registered - what to do, wrap? */

         p = regFunc("glXFreeMemoryNV", (void *) r200FreeMemoryNV);
         if (p)
            ;  /* XXX already registered - what to do, wrap? */

         p = regFunc("glXGetAGPOffsetMESA", (void *) r200GetAGPOffset);
         if (p)
            ;  /* XXX already registered - what to do, wrap? */
      }

      if (regString) {
         regString("GLX_NV_vertex_array_range");
         regString("GLX_MESA_agp_offset");
      }

      dlclose(dll);
   }
}
