/**************************************************************************

Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_xmesa.c,v 1.10 2001/05/19 18:29:18 dawes Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include <unistd.h>
#include <sys/mman.h>
#include "sis_dri.h"

#include "sis_ctx.h"
#include "sis_mesa.h"

#include <X11/Xlibint.h>
#include "context.h"
#include "macros.h"
#include "matrix.h"
#include "mmath.h"
#include "vbxform.h"
#include "types.h"

#if 0
void *sis_debug_malloc(x){
  static char buf[2000000];
  static int count = 0;
  void *retval;
  
  retval = &(buf[count]);
  count += x;
  return retval;
}
#endif

static XMesaContext XMesa = NULL;  /* Current X/Mesa context pointer */

#ifndef SIS_VERBOSE
int SIS_VERBOSE = 0 
/*                | VERBOSE_SIS_MEMORY */
/*                | VERBOSE_SIS_BUFFER */
;
#endif

static GLboolean IsDriverInit = GL_FALSE;
static sisRegion global_regs, global_agp;
static GLubyte *global_FbBase;

GLboolean XMesaInitDriver (__DRIscreenPrivate * driScrnPriv)
{
  SISDRIPtr priv = (SISDRIPtr) driScrnPriv->pDevPriv;

  /* Check the DRI version */
  {
      int major, minor, patch;
      if (XF86DRIQueryVersion(driScrnPriv->display, &major, &minor, &patch)) {
         if (major != 4 || minor < 0) {
            char msg[1000];
            sprintf(msg, "sis DRI driver expected DRI version 4.0.x but got version %d.%d.%d", major, minor, patch);
            __driMesaMessage(msg);
            return GL_FALSE;
         }
      }
  }

  /* Check that the DDX driver version is compatible */
  if (driScrnPriv->ddxMajor != 0 ||
       driScrnPriv->ddxMinor != 1 ||
       driScrnPriv->ddxPatch < 0) {
      char msg[1000];
      sprintf(msg, "sis DRI driver expected DDX driver version 0.1.x but got version %d.%d.%d", driScrnPriv->ddxMajor, driScrnPriv->ddxMinor, driScrnPriv->ddxPatch);
      __driMesaMessage(msg);
      return GL_FALSE;
  }

  /* Check that the DRM driver version is compatible */
  if (driScrnPriv->drmMajor != 1 ||
       driScrnPriv->drmMinor != 0 ||
       driScrnPriv->drmPatch < 0) {
      char msg[1000];
      sprintf(msg, "sis DRI driver expected DRM driver version 1.0.x but got version %d.%d.%d", driScrnPriv->drmMajor, driScrnPriv->drmMinor, driScrnPriv->drmPatch);
      __driMesaMessage(msg);
      return GL_FALSE;
  }

  assert (driScrnPriv->devPrivSize == sizeof (SISDRIRec));

  /* Fixme: in quake3, when context changed, XMesaInitDriver is called
   *        but XMesaResetDriver isn't. so i must check if regions are
   *        mapped two times. 
   *        I can't use .map to know if they are mapped
   */  
  if(IsDriverInit){
    priv->regs = global_regs;
    priv->agp = global_agp;
    /* XXX */
    driScrnPriv->pFB = global_FbBase;
    return GL_TRUE;   
  }
      
  if (drmMap (driScrnPriv->fd, priv->regs.handle, priv->regs.size,
	      &priv->regs.map))
    {
        return GL_FALSE;
    }

  if (priv->agp.size)
    {
      if (drmMap (driScrnPriv->fd, priv->agp.handle, priv->agp.size,
	  	  &priv->agp.map))
	{
	  priv->agp.size = 0;
 	}
    }

  IsDriverInit = GL_TRUE;
  global_regs = priv->regs;
  global_agp = priv->agp;
  global_FbBase = driScrnPriv->pFB;

  gDRMSubFD = driScrnPriv->fd;
  
  return GL_TRUE;
}

void
XMesaResetDriver (__DRIscreenPrivate * driScrnPriv)
{
  SISDRIPtr priv = (SISDRIPtr) driScrnPriv->pDevPriv;

  drmUnmap (priv->regs.map, priv->regs.size);
  priv->regs.map = 0;

  if (priv->agp.size)
    {
      drmUnmap (priv->agp.map, priv->agp.size);
      priv->agp.map = 0;
    }

  IsDriverInit = GL_FALSE;
}

/* from tdfx */
extern void __driRegisterExtensions(void); /* silence compiler warning */

/* This function is called by libGL.so as soon as libGL.so is loaded.
 * This is where we'd register new extension functions with the dispatcher.
 */
void __driRegisterExtensions(void)
{
#if 0
   /* Example.  Also look in fxdd.c for more details. */
   {
      const int _gloffset_FooBarEXT = 555;  /* just an example number! */
      if (_glapi_add_entrypoint("glFooBarEXT", _gloffset_FooBarEXT)) {
         void *f = glXGetProcAddressARB("glFooBarEXT");
         assert(f);
      }
   }
#endif
}

GLvisual *XMesaCreateVisual(Display *dpy,
                            __DRIscreenPrivate *driScrnPriv,
                            const XVisualInfo *visinfo,
                            const __GLXvisualConfig *config)
{
   /* Drivers may change the args to _mesa_create_visual() in order to
    * setup special visuals.
    */
   return _mesa_create_visual( config->rgba,
                               config->doubleBuffer,
                               config->stereo,
                               _mesa_bitcount(visinfo->red_mask),
                               _mesa_bitcount(visinfo->green_mask),
                               _mesa_bitcount(visinfo->blue_mask),
                               config->alphaSize,
                               0, /* index bits */
                               config->depthSize,
                               config->stencilSize,
                               config->accumRedSize,
                               config->accumGreenSize,
                               config->accumBlueSize,
                               config->accumAlphaSize,
                               0 /* num samples */ );
}

GLboolean XMesaCreateContext(Display *dpy, GLvisual *mesaVis,
                             __DRIcontextPrivate *driContextPriv)
{
  XMesaContext c;

  if (SIS_VERBOSE){
    fprintf(stderr, "XMesaCreateContext\n");
  }

   c = (XMesaContext) calloc (1, sizeof (struct xmesa_context));
  if (!c)
    return GL_FALSE;

  c->xm_visual = (XMesaVisual) calloc (1, sizeof (struct xmesa_visual));  
  if (!c->xm_visual)
    return GL_FALSE;
  c->xm_visual->gl_visual = mesaVis;
  c->xm_visual->display = dpy;
  
  c->gl_ctx = driContextPriv->mesaContext;
  
  c->xm_buffer = NULL;
  c->display = dpy;

  c->gl_ctx->Driver.UpdateState = sis_UpdateState;

  c->driContextPriv = driContextPriv;

  c->gl_ctx->DriverCtx = (void *)c;
  
  SiSCreateContext (c);
  
  /* Fixme */
  if (c->gl_ctx->NrPipelineStages)
    c->gl_ctx->NrPipelineStages =
      sis_RegisterPipelineStages( c->gl_ctx->PipelineStage,
				  c->gl_ctx->PipelineStage,
				  c->gl_ctx->NrPipelineStages);

  driContextPriv->driverPrivate = (void *) c;
  
  /* TODO, to make VB->Win.data[][2] ranges 0 - 1.0 */
  /* Fixme, software render, z span seems all 0 */
  mesaVis->DepthMax = 1;
  mesaVis->DepthMaxF = 1.0f;

  return GL_TRUE;
}

void XMesaDestroyContext(__DRIcontextPrivate *driContextPriv)
{
  XMesaContext c = (XMesaContext) driContextPriv->driverPrivate;

  SiSDestroyContext (c);

  if (c->xm_buffer)
    c->xm_buffer->xm_context = NULL;

  if (XMesa == c)
    XMesa = NULL;

  free(c->xm_visual);
  free (c);
}

GLframebuffer *XMesaCreateWindowBuffer( Display *dpy,
                                        __DRIscreenPrivate *driScrnPriv,
                                        __DRIdrawablePrivate *driDrawPriv,
                                        GLvisual *mesaVis)
{
  if (mesaVis->RGBAflag){
    return gl_create_framebuffer (mesaVis, GL_FALSE, GL_FALSE,
        			  (mesaVis->AccumRedBits) ? GL_TRUE : GL_FALSE, 
				  GL_FALSE);
  }
  else{
    return gl_create_framebuffer (mesaVis,
				  (mesaVis->DepthBits) ? GL_TRUE : GL_FALSE,
				  (mesaVis->StencilBits) ? GL_TRUE : GL_FALSE,
				  (mesaVis->AccumRedBits) ? GL_TRUE : GL_FALSE, 
				  GL_FALSE);
  }
}

static XMesaBuffer SISCreateWindowBuffer ( Display *dpy,
                                           __DRIscreenPrivate *driScrnPriv,
                                           __DRIdrawablePrivate *driDrawPriv,
                                           GLvisual *mesaVis,
                                           XMesaContext xmesa)
{
  XMesaBuffer b = (XMesaBuffer) calloc (1, sizeof (struct xmesa_buffer));

  if (SIS_VERBOSE&VERBOSE_SIS_BUFFER){
    fprintf(stderr, "SISCreateWindowBuffer: drawable ID=%lu\n",
            (DWORD)driDrawPriv);
  }
  
  if (!b)
    return NULL;

  b->xm_context = NULL;
  b->xm_visual = xmesa->xm_visual;
  b->display = dpy;
  b->pixmap_flag = GL_FALSE;
  b->db_state = mesaVis->DBflag;
  b->gl_buffer = driDrawPriv->mesaBuffer;
  b->frontbuffer = driDrawPriv->draw;

  /* set 0 for buffer update */
  b->width = 0;
  b->height = 0;

  if (b->backimage)
    {
#if 0
      XMesaDestroyImage (b->backimage);
#else
      free(b->backimage);
#endif
      b->backimage = NULL;
    }

#if 0
  b->backimage = XCreateImage (b->display,
			       b->xm_visual->visinfo->visual,
			       GET_VISUAL_DEPTH (b->xm_visual), ZPixmap, 0,
			       NULL, b->width, b->height, 8, 0);
#else
  b->backimage = (XMesaImage *) calloc (1, sizeof (XImage));
#endif

  b->driDrawPriv = driDrawPriv;

  {
    sisBufferInfo *buf_info;
    
    b->private = calloc (1, sizeof (sisBufferInfo));
    buf_info = (sisBufferInfo *)(b->private);
    
    buf_info->pZClearPacket = &buf_info->zClearPacket;
    buf_info->pCbClearPacket = &buf_info->cbClearPacket;    
  }
  
  return b;
}

GLframebuffer *XMesaCreatePixmapBuffer( Display *dpy,
                                        __DRIscreenPrivate *driScrnPriv,
                                        __DRIdrawablePrivate *driDrawPriv,
                                        GLvisual *mesaVis)
{
  /* not implement yet */
  return NULL;
}

static void SISDestroyBuffer (XMesaBuffer b)
{  
  if (SIS_VERBOSE&VERBOSE_SIS_BUFFER){
    fprintf(stderr, "SISDestroyBuffer: b=%lu\n", (DWORD)b);
  }
  
  if (b->backimage && b->backimage->data)
    {
      sisBufferInfo *priv = (sisBufferInfo *) b->private;
      
      sis_free_back_image (b, b->backimage, priv->bbFree);
#if SIS_STEREO
      {
        XMesaContext xmesa = (XMesaContext) b->xm_context;
        __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
        GLcontext *ctx = xmesa->gl_ctx;

        if(hwcx->stereoEnabled){
          sis_final_stereo(ctx);
          hwcx->stereoEnabled = GL_FALSE;
        }
      }
#endif
    }

  if (b->depthbuffer)
    {
      sis_free_z_stencil_buffer (b);
    }

  assert (b->private);
  free (b->private);
  b->private = NULL;

  if (b->xm_context)
    b->xm_context->xm_buffer = NULL;

#if 0
  XMesaDestroyImage (b->backimage);
#else
  free(b->backimage);
#endif

  /* TODO : if b doesn't exist, do something */

  free (b);
}

void XMesaSwapBuffers(__DRIdrawablePrivate *driDrawPriv)
{
  __GLSiScontext *hwcx;
  
  if(!XMesa)
    return;
  
  FLUSH_VB( XMesa->gl_ctx, "swap buffers" );

  hwcx = (__GLSiScontext *) XMesa->private;    
  (hwcx->SwapBuffers)(XMesa->xm_buffer);
}

GLboolean XMesaUnbindContext(__DRIcontextPrivate *driContextPriv)
{
  /* TODO */
  return GL_TRUE;
}

GLboolean
XMesaOpenFullScreen(__DRIcontextPrivate *driContextPriv)
{
    return GL_TRUE;
}

GLboolean
XMesaCloseFullScreen(__DRIcontextPrivate *driContextPriv)
{
    return GL_TRUE;
}

GLboolean XMesaMakeCurrent(__DRIcontextPrivate *driContextPriv,
                           __DRIdrawablePrivate *driDrawPriv,
                           __DRIdrawablePrivate *driReadPriv)
{    
  if (driContextPriv)
    {
      XMesaContext c = (XMesaContext) driContextPriv->driverPrivate;      
      XMesaBuffer b; 

      /* TODO: ??? */
      if ((c->gl_ctx == gl_get_current_context ()) &&
          (driContextPriv->driDrawablePriv == driDrawPriv) &&
	  c->xm_buffer->wasCurrent)
	{
	  return GL_TRUE;
	}

      if (SIS_VERBOSE&VERBOSE_SIS_BUFFER){
        fprintf(stderr, "XMesaMakeCurrent: c=%lu, b=%lu\n", (DWORD)c, (DWORD)b);
        fprintf(stderr, "XMesaMakeCurrent: drawable ID=%lu\n", (DWORD)b->frontbuffer);
        fprintf(stderr, "XMesaMakeCurrent: width=%d, height=%d\n", 
                b->width, b->height);    
        {
          __DRIdrawablePrivate *dPriv = c->driContextPriv->driDrawablePriv;
          fprintf(stderr, "XMesaMakeCurrent: width=%d, height=%d\n", 
              dPriv->w, dPriv->h);      
        }
      }

      b = SISCreateWindowBuffer(c->display,
                                driContextPriv->driScreenPriv,
                                driDrawPriv,
                                c->gl_ctx->Visual,
                                c);

      if (c->xm_buffer){
	/* TODO: ??? */
	c->xm_buffer->xm_context = NULL;
        SISDestroyBuffer(c->xm_buffer);
      }

      b->xm_context = c;
      c->xm_buffer = b;

      gl_make_current (c->gl_ctx, b->gl_buffer);
      XMesa = c;

      if(b->width == 0){
        GLuint width, height;
        
        sis_GetBufferSize (c->gl_ctx, &width, &height);        
      }

      sis_update_drawable_state(c->gl_ctx);
      
      if (c->gl_ctx->Viewport.Width == 0)
	{
	  /* initialize viewport to window size */
	  gl_Viewport (c->gl_ctx, 0, 0, b->width, b->height);
	  c->gl_ctx->Scissor.Width = b->width;
	  c->gl_ctx->Scissor.Height = b->height;
	}

      c->xm_buffer->wasCurrent = GL_TRUE;
    }
  else
    {
      gl_make_current (NULL, NULL);
      XMesa = NULL;
    }

  return GL_TRUE;
}
