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
/* $XFree86: xc/lib/GL/mesa/src/drv/i830/i830_context.c,v 1.9 2003/02/06 04:18:00 dawes Exp $ */

/*
 * Authors:
 *   Jeff Hartmann <jhartmann@2d3d.com>
 *   Graeme Fisher <graeme@2d3d.co.za>
 *   Abraham vd Merwe <abraham@2d3d.co.za>
 *
 * Heavily Based on I810 driver written by:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#include "glheader.h"
#include "context.h"
#include "matrix.h"
#include "simple_list.h"
#include "extensions.h"
#include "mem.h"

#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "tnl/tnl.h"
#include "array_cache/acache.h"

#include "tnl/t_pipeline.h"

#include "i830_screen.h"
#include "i830_dri.h"

#include "i830_state.h"
#include "i830_tex.h"
#include "i830_span.h"
#include "i830_tris.h"
#include "i830_vb.h"
#include "i830_ioctl.h"

#include <X11/Xlibint.h>
#include <stdio.h>


#ifndef I830_DEBUG
int I830_DEBUG = (0);
#endif

/***************************************
 * Mesa's Driver Functions
 ***************************************/

#define DRIVER_DATE                     "20021115"

static const GLubyte *i830DDGetString( GLcontext *ctx, GLenum name )
{
   switch (I830_CONTEXT(ctx)->i830Screen->deviceID) {
   case PCI_CHIP_845_G:
      switch (name) {
      case GL_VENDOR:
	 return (GLubyte *)"2d3D, Inc";
      case GL_RENDERER:
	 return (GLubyte *)"Mesa DRI Intel(R) 845G " DRIVER_DATE;
      default:
	 return 0;
      }
      break;
   case PCI_CHIP_I830_M:
      switch (name) {
      case GL_VENDOR:
	 return (GLubyte *)"VA Linux, Inc";
      case GL_RENDERER:
	 return (GLubyte *)"Mesa DRI Intel(R) 830M " DRIVER_DATE;
      default:
	 return 0;
      }
      break;
   case PCI_CHIP_I855_GM:
      switch (name) {
      case GL_VENDOR:
	 return (GLubyte *)"Tungsten Graphics, Inc";
      case GL_RENDERER:
	 return (GLubyte *)"Mesa DRI Intel(R) 852GM/855GM " DRIVER_DATE;
      default:
	 return 0;
      }
      break;
   case PCI_CHIP_I865_G:
      switch (name) {
      case GL_VENDOR:
	 return (GLubyte *)"Tungsten Graphics, Inc";
      case GL_RENDERER:
	 return (GLubyte *)"Mesa DRI Intel(R) 865G " DRIVER_DATE;
      default:
	 return 0;
      }
      break;
   default:
      switch (name) {
      case GL_VENDOR:
	 return (GLubyte *)"Tungsten Graphics, Inc";
      case GL_RENDERER:
	 return (GLubyte *)"Mesa DRI Unknown Intel Chipset " DRIVER_DATE;
      default:
	 return 0;
      }
      break;
   }
}

static void i830BufferSize(GLframebuffer *buffer,
			   GLuint *width, GLuint *height)
{
   GET_CURRENT_CONTEXT(ctx);
   i830ContextPtr imesa = I830_CONTEXT(ctx);
   /* Need to lock to make sure the driDrawable is uptodate.  This
    * information is used to resize Mesa's software buffers, so it has
    * to be correct.
    */
   LOCK_HARDWARE(imesa);
   *width = imesa->driDrawable->w;
   *height = imesa->driDrawable->h;
   UNLOCK_HARDWARE(imesa);
}

/* Enable all the extensions we need */
static void i830InitExtensions( GLcontext *ctx )
{
   _mesa_enable_imaging_extensions( ctx );
   _mesa_enable_extension( ctx, "GL_ARB_multitexture" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_combine" ); 
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_combine" ); 
   _mesa_enable_extension( ctx, "GL_EXT_blend_color" );
   _mesa_enable_extension( ctx, "GL_EXT_blend_minmax" );
   _mesa_enable_extension( ctx, "GL_EXT_blend_subtract" );
   _mesa_enable_extension( ctx, "GL_EXT_blend_func_separate" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_lod_bias" );
   _mesa_enable_extension( ctx, "GL_EXT_secondary_color" );
   _mesa_enable_extension( ctx, "GL_EXT_fog_coord" );

   /* Leave this for later */
#if 0
   _mesa_enable_extension( ctx, "GL_EXT_stencil_wrap" );
#endif
}

extern const struct gl_pipeline_stage _i830_render_stage;

static const struct gl_pipeline_stage *i830_pipeline[] = {
   &_tnl_vertex_transform_stage,
   &_tnl_normal_transform_stage,
   &_tnl_lighting_stage,
   &_tnl_fog_coordinate_stage,
   &_tnl_texgen_stage,
   &_tnl_texture_transform_stage,
     				/* REMOVE: point attenuation stage */
#if 1
   &_i830_render_stage,     /* ADD: unclipped rastersetup-to-dma */
#endif
   &_tnl_render_stage,
   0,
};


#if DO_DEBUG
static void add_debug_flags( const char *debug )
{
   if (strstr(debug, "fall")) 
      I830_DEBUG |= DEBUG_FALLBACKS;

   if (strstr(debug, "tex")) 
      I830_DEBUG |= DEBUG_TEXTURE;

   if (strstr(debug, "ioctl")) 
      I830_DEBUG |= DEBUG_IOCTL;

   if (strstr(debug, "prim")) 
      I830_DEBUG |= DEBUG_PRIMS;

   if (strstr(debug, "vert")) 
      I830_DEBUG |= DEBUG_VERTS;

   if (strstr(debug, "state")) 
      I830_DEBUG |= DEBUG_STATE;

   if (strstr(debug, "verb")) 
      I830_DEBUG |= DEBUG_VERBOSE;

   if (strstr(debug, "dri")) 
      I830_DEBUG |= DEBUG_DRI;

   if (strstr(debug, "dma")) 
      I830_DEBUG |= DEBUG_DMA;

   if (strstr(debug, "san")) 
      I830_DEBUG |= DEBUG_SANITY;

   if (strstr(debug, "sync")) 
      I830_DEBUG |= DEBUG_SYNC;

   if (strstr(debug, "sleep")) 
      I830_DEBUG |= DEBUG_SLEEP;
}
#endif

GLboolean i830CreateContext( Display *dpy, const __GLcontextModes *mesaVis,
			    __DRIcontextPrivate *driContextPriv,
			    void *sharedContextPrivate)
{
   GLcontext *ctx , *shareCtx;
   i830ContextPtr imesa;
   __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
   i830ScreenPrivate *i830Screen = (i830ScreenPrivate *)sPriv->private;
   I830SAREAPtr saPriv=(I830SAREAPtr)
       (((GLubyte *)sPriv->pSAREA)+i830Screen->sarea_priv_offset);

   /* Allocate i830 context */
   imesa = (i830ContextPtr) CALLOC_STRUCT(i830_context_t);
   if (!imesa) return GL_FALSE;

   /* Allocate the Mesa context */
   if (sharedContextPrivate)
     shareCtx = ((i830ContextPtr) sharedContextPrivate)->glCtx;
   else
     shareCtx = NULL;
   
   imesa->glCtx = _mesa_create_context(mesaVis, shareCtx, imesa, GL_TRUE);
   if (!imesa->glCtx) {
      FREE(imesa);
      return GL_FALSE;
   }
   driContextPriv->driverPrivate = imesa;

   /* Set the maximum texture size small enough that we can guarentee
    * that both texture units can bind a maximal texture and have them
    * in memory at once.
    */
   ctx = imesa->glCtx;
   if (i830Screen->textureSize < 2*1024*1024) {
      ctx->Const.MaxTextureLevels = 9;
   } else if (i830Screen->textureSize < 8*1024*1024) {
      ctx->Const.MaxTextureLevels = 10;
   } else {
      ctx->Const.MaxTextureLevels = 11;
   }
   ctx->Const.MaxTextureUnits = 2;

   ctx->Const.MinLineWidth = 1.0;
   ctx->Const.MinLineWidthAA = 1.0;
   ctx->Const.MaxLineWidth = 3.0;
   ctx->Const.MaxLineWidthAA = 3.0;
   ctx->Const.LineWidthGranularity = 1.0;

   ctx->Const.MinPointSize = 1.0;
   ctx->Const.MinPointSizeAA = 1.0;
   ctx->Const.MaxPointSize = 255.0;
   ctx->Const.MaxPointSizeAA = 3.0;
   ctx->Const.PointSizeGranularity = 1.0;

   ctx->Driver.GetBufferSize = i830BufferSize;
   ctx->Driver.ResizeBuffers = _swrast_alloc_buffers;
   ctx->Driver.GetString = i830DDGetString;

   /* Who owns who? */
   ctx->DriverCtx = (void *) imesa;
   imesa->glCtx = ctx;

   /* Initialize the software rasterizer and helper modules. */
   _swrast_CreateContext( ctx );
   _ac_CreateContext( ctx );
   _tnl_CreateContext( ctx );
   _swsetup_CreateContext( ctx );

   /* Install the customized pipeline: */
   _tnl_destroy_pipeline( ctx );
   _tnl_install_pipeline( ctx, i830_pipeline );

   /* Configure swrast to match hardware characteristics: */
   _swrast_allow_pixel_fog( ctx, GL_FALSE );
   _swrast_allow_vertex_fog( ctx, GL_TRUE );

   /* Dri stuff */
   imesa->display = dpy;
   imesa->hHWContext = driContextPriv->hHWContext;
   imesa->driFd = sPriv->fd;
   imesa->driHwLock = &sPriv->pSAREA->lock;
   imesa->vertex_format = 0;

   imesa->hw_stencil = mesaVis->stencilBits && mesaVis->depthBits == 24;

   switch(mesaVis->depthBits) {
   case 16:
      imesa->depth_scale = 1.0/0xffff;
      imesa->depth_clear_mask = ~0;
      imesa->ClearDepth = 0xffff;
      break;
   case 24:
      imesa->depth_scale = 1.0/0xffffff;
      imesa->depth_clear_mask = 0x00ffffff;
      imesa->stencil_clear_mask = 0xff000000;
      imesa->ClearDepth = 0x00ffffff;
      break;
   case 32: /* Not supported */
   default:
      break;
   }
   /* Completely disable stenciling for now, there are some serious issues
    * with stencil.
    */
#if 1
   imesa->hw_stencil = 0;
#endif

   imesa->i830Screen = i830Screen;
   imesa->driScreen = sPriv;
   imesa->sarea = saPriv;
   imesa->glBuffer = NULL;

   imesa->texHeap = mmInit( 0, i830Screen->textureSize );
   imesa->RenderIndex = ~0;
   imesa->dirty = ~0;
   imesa->upload_cliprects = GL_TRUE;

   make_empty_list(&imesa->TexObjList);
   make_empty_list(&imesa->SwappedOut);

   imesa->CurrentTexObj[0] = 0;
   imesa->CurrentTexObj[1] = 0;

   imesa->do_irqs = (imesa->i830Screen->irq_active &&
		     !getenv("I830_NO_IRQS"));

   _math_matrix_ctr (&imesa->ViewportMatrix);

   i830InitExtensions (ctx);
   i830DDInitStateFuncs( ctx );
   i830DDInitTextureFuncs( ctx );
   i830InitTriFuncs (ctx);
   i830DDInitSpanFuncs( ctx );
   i830DDInitIoctlFuncs( ctx );
   i830InitVB (ctx);
   i830DDInitState (ctx);

#if DO_DEBUG
   if (getenv("INTEL_DEBUG"))
      add_debug_flags( getenv("INTEL_DEBUG") );
   if (getenv("I830_DEBUG"))
      add_debug_flags( getenv("I830_DEBUG") );
#endif

   if (getenv("I830_NO_RAST") || 
       getenv("INTEL_NO_RAST")) {
      fprintf(stderr, "disabling 3D rasterization\n");
      FALLBACK(imesa, I830_FALLBACK_USER, 1); 
   }


   return GL_TRUE;
}

void i830DestroyContext(__DRIcontextPrivate *driContextPriv)
{
   i830ContextPtr imesa = (i830ContextPtr) driContextPriv->driverPrivate;

   assert(imesa); /* should never be null */
   if (imesa) {
      _swsetup_DestroyContext (imesa->glCtx);
      _tnl_DestroyContext (imesa->glCtx);
      _ac_DestroyContext (imesa->glCtx);
      _swrast_DestroyContext (imesa->glCtx);

      i830FreeVB (imesa->glCtx);

      /* free the Mesa context */
      imesa->glCtx->DriverCtx = NULL;
      _mesa_destroy_context(imesa->glCtx);

      Xfree (imesa);
   }
}

void i830XMesaSetFrontClipRects( i830ContextPtr imesa )
{
   __DRIdrawablePrivate *dPriv = imesa->driDrawable;

   imesa->numClipRects = dPriv->numClipRects;
   imesa->pClipRects = dPriv->pClipRects;
   imesa->drawX = dPriv->x;
   imesa->drawY = dPriv->y;

   i830EmitDrawingRectangle( imesa );
   imesa->upload_cliprects = GL_TRUE;
}

void i830XMesaSetBackClipRects( i830ContextPtr imesa )
{
   __DRIdrawablePrivate *dPriv = imesa->driDrawable;

   if (imesa->sarea->pf_enabled == 0 && dPriv->numBackClipRects == 0) {
      imesa->numClipRects = dPriv->numClipRects;
      imesa->pClipRects = dPriv->pClipRects;
      imesa->drawX = dPriv->x;
      imesa->drawY = dPriv->y;
   } else {
      imesa->numClipRects = dPriv->numBackClipRects;
      imesa->pClipRects = dPriv->pBackClipRects;
      imesa->drawX = dPriv->backX;
      imesa->drawY = dPriv->backY;
   }

   i830EmitDrawingRectangle( imesa );
   imesa->upload_cliprects = GL_TRUE;
}

static void i830XMesaWindowMoved( i830ContextPtr imesa )
{
   switch (imesa->glCtx->Color.DriverDrawBuffer) {
   case GL_FRONT_LEFT:
      i830XMesaSetFrontClipRects( imesa );
      break;
   case GL_BACK_LEFT:
      i830XMesaSetBackClipRects( imesa );
      break;
   default:
      break;
   }
}

GLboolean i830UnbindContext(__DRIcontextPrivate *driContextPriv)
{
   i830ContextPtr imesa = (i830ContextPtr) driContextPriv->driverPrivate;
   if (imesa) {
      /* Might want to change this so texblend isn't always updated */
      imesa->dirty |= (I830_UPLOAD_CTX |
		       I830_UPLOAD_BUFFERS |
		       I830_UPLOAD_STIPPLE |
		       I830_UPLOAD_TEXBLEND0 |
		       I830_UPLOAD_TEXBLEND1);

      if (imesa->CurrentTexObj[0]) imesa->dirty |= I830_UPLOAD_TEX0;
      if (imesa->CurrentTexObj[1]) imesa->dirty |= I830_UPLOAD_TEX1;
   }
   return GL_TRUE;
}

GLboolean i830MakeCurrent(__DRIcontextPrivate *driContextPriv,
			  __DRIdrawablePrivate *driDrawPriv,
			  __DRIdrawablePrivate *driReadPriv)
{

   if (driContextPriv) {
      i830ContextPtr imesa = (i830ContextPtr) driContextPriv->driverPrivate;

      if ( imesa->driDrawable != driDrawPriv ) {
	 /* Shouldn't the readbuffer be stored also? */
	 imesa->driDrawable = driDrawPriv;
	 i830XMesaWindowMoved( imesa );
      }

      _mesa_make_current2(imesa->glCtx,
			  (GLframebuffer *) driDrawPriv->driverPrivate,
			  (GLframebuffer *) driReadPriv->driverPrivate);

      if (!imesa->glCtx->Viewport.Width)
	 _mesa_set_viewport(imesa->glCtx, 0, 0,
			    driDrawPriv->w, driDrawPriv->h);
   } else {
      _mesa_make_current(0,0);
   }

   return GL_TRUE;
}

/* Turn on/off page flipping according to the flags in the sarea:
 */
static void
i830UpdatePageFlipping( i830ContextPtr imesa )
{
   GLcontext *ctx = imesa->glCtx;
   int front = 0;

   switch (ctx->Color.DriverDrawBuffer) {
   case GL_FRONT_LEFT:
      front = 1;
      break;
   case GL_BACK_LEFT:
      front = 0;
      break;
   default:
      return;
   }

   if ( imesa->sarea->pf_current_page == 1 ) 
      front ^= 1;
   
   if (front) {
      imesa->BufferSetup[I830_DESTREG_CBUFADDR] = imesa->i830Screen->fbOffset;
      imesa->drawMap = (char *)imesa->driScreen->pFB;
      imesa->readMap = (char *)imesa->driScreen->pFB;
   } else {
      imesa->BufferSetup[I830_DESTREG_CBUFADDR] = imesa->i830Screen->backOffset;
      imesa->drawMap = imesa->i830Screen->back.map;
      imesa->readMap = imesa->i830Screen->back.map;
   }

   imesa->dirty |= I830_UPLOAD_BUFFERS;
}

void i830GetLock( i830ContextPtr imesa, GLuint flags )
{
   __DRIdrawablePrivate *dPriv = imesa->driDrawable;
   __DRIscreenPrivate *sPriv = imesa->driScreen;
   I830SAREAPtr sarea = imesa->sarea;
   int me = imesa->hHWContext;

   drmGetLock(imesa->driFd, imesa->hHWContext, flags);

   /* If the window moved, may need to set a new cliprect now.
    *
    * NOTE: This releases and regains the hw lock, so all state
    * checking must be done *after* this call:
    */
   DRI_VALIDATE_DRAWABLE_INFO(imesa->display, sPriv, dPriv);

   /* If we lost context, need to dump all registers to hardware.
    * Note that we don't care about 2d contexts, even if they perform
    * accelerated commands, so the DRI locking in the X server is even
    * more broken than usual.
    */

   if (sarea->ctxOwner != me) {
      imesa->upload_cliprects = GL_TRUE;
      imesa->dirty |= (I830_UPLOAD_CTX |
		       I830_UPLOAD_BUFFERS | 
		       I830_UPLOAD_STIPPLE);

      if(imesa->CurrentTexObj[0]) imesa->dirty |= I830_UPLOAD_TEX0;
      if(imesa->CurrentTexObj[1]) imesa->dirty |= I830_UPLOAD_TEX1;
      if(imesa->TexBlendWordsUsed[0]) imesa->dirty |= I830_UPLOAD_TEXBLEND0;
      if(imesa->TexBlendWordsUsed[1]) imesa->dirty |= I830_UPLOAD_TEXBLEND1;

      sarea->perf_boxes = imesa->perf_boxes | I830_BOX_LOST_CONTEXT;
      sarea->ctxOwner = me;
   }

   /* Shared texture managment - if another client has played with
    * texture space, figure out which if any of our textures have been
    * ejected, and update our global LRU.
    */
   if (sarea->texAge != imesa->texAge) {
      int sz = 1 << (imesa->i830Screen->logTextureGranularity);
      int idx, nr = 0;

      /* Have to go right round from the back to ensure stuff ends up
       * LRU in our local list...
       */
      for (idx = sarea->texList[I830_NR_TEX_REGIONS].prev ;
	   idx != I830_NR_TEX_REGIONS && nr < I830_NR_TEX_REGIONS ;
	   idx = sarea->texList[idx].prev, nr++) {
	 if (sarea->texList[idx].age > imesa->texAge)
	   i830TexturesGone(imesa, idx * sz, sz, sarea->texList[idx].in_use);
      }

      if (nr == I830_NR_TEX_REGIONS) {
	 i830TexturesGone(imesa, 0, imesa->i830Screen->textureSize, 0);
	 i830ResetGlobalLRU( imesa );
      }

      imesa->texAge = sarea->texAge;
   }

   if (imesa->lastStamp != dPriv->lastStamp) {
      i830UpdatePageFlipping( imesa );
      i830XMesaWindowMoved( imesa );
      imesa->lastStamp = dPriv->lastStamp;
   }

   sarea->last_quiescent = -1;  /* just kill it for now */
}

void i830SwapBuffers(Display *dpy, void *drawablePrivate)
{
   __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;

   if (dPriv->driContextPriv && dPriv->driContextPriv->driverPrivate) {
      i830ContextPtr imesa;
      GLcontext *ctx;
      imesa = (i830ContextPtr) dPriv->driContextPriv->driverPrivate;
      ctx = imesa->glCtx;
      if (ctx->Visual.doubleBufferMode) {
	 /* flush pending rendering comands */
	 _mesa_swapbuffers( ctx );
	 if ( imesa->sarea->pf_active && 
	      (dPriv->w * dPriv->h * imesa->i830Screen->cpp) > (300*300*4) ) {
	    i830PageFlip( dPriv );
	 } else {
	    i830CopyBuffer( dPriv );
	 }
      }
   } else {
      /* XXX this shouldn't be an error but we can't handle it for now */
      _mesa_problem(NULL, "i830SwapBuffers: drawable has no context!\n");
   }
}
