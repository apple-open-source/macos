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
/* $XFree86: xc/lib/GL/mesa/src/drv/i810/i810context.c,v 1.3 2002/10/30 12:51:33 alanh Exp $ */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
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

#include "i810screen.h"
#include "i810_dri.h"

#include "i810state.h"
#include "i810tex.h"
#include "i810span.h"
#include "i810tris.h"
#include "i810vb.h"
#include "i810ioctl.h"

#include <X11/Xlibint.h>
#include <stdio.h>


static const GLubyte *i810GetString( GLcontext *ctx, GLenum name )
{
   switch (name) {
   case GL_VENDOR:
      return (GLubyte *)"Keith Whitwell";
   case GL_RENDERER:
      return (GLubyte *)"Mesa DRI I810 20020221";
   default:
      return 0;
   }
}

static void i810BufferSize(GLframebuffer *buffer, GLuint *width, GLuint *height)
{
   GET_CURRENT_CONTEXT(ctx);
   i810ContextPtr imesa = I810_CONTEXT(ctx);

   /* Need to lock to make sure the driDrawable is uptodate.  This
    * information is used to resize Mesa's software buffers, so it has
    * to be correct.
    */
   LOCK_HARDWARE(imesa);
   *width = imesa->driDrawable->w;
   *height = imesa->driDrawable->h;
   UNLOCK_HARDWARE(imesa);
}

static void i810InitExtensions( GLcontext *ctx )
{
   _mesa_enable_imaging_extensions( ctx);
   _mesa_enable_extension( ctx, "GL_ARB_multitexture" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_EXT_stencil_wrap" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_lod_bias" );
}

extern const struct gl_pipeline_stage _i810_render_stage;

static const struct gl_pipeline_stage *i810_pipeline[] = {
   &_tnl_vertex_transform_stage,
   &_tnl_normal_transform_stage,
   &_tnl_lighting_stage,
   &_tnl_fog_coordinate_stage,
   &_tnl_texgen_stage,
   &_tnl_texture_transform_stage,
				/* REMOVE: point attenuation stage */
#if 1
   &_i810_render_stage,		/* ADD: unclipped rastersetup-to-dma */
#endif
   &_tnl_render_stage,
   0,
};


GLboolean
i810CreateContext( Display *dpy, const __GLcontextModes *mesaVis,
                   __DRIcontextPrivate *driContextPriv,
                   void *sharedContextPrivate )
{
   GLcontext *ctx, *shareCtx;
   i810ContextPtr imesa;
   __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
   i810ScreenPrivate *i810Screen = (i810ScreenPrivate *)sPriv->private;
   I810SAREAPtr saPriv = (I810SAREAPtr)
      (((GLubyte *)sPriv->pSAREA) + i810Screen->sarea_priv_offset);

   /* Allocate i810 context */
   imesa = (i810ContextPtr) CALLOC_STRUCT(i810_context_t);
   if (!imesa) {
      return GL_FALSE;
   }

   /* Allocate the Mesa context */
   if (sharedContextPrivate)
      shareCtx = ((i810ContextPtr) sharedContextPrivate)->glCtx;
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
   if (i810Screen->textureSize < 2*1024*1024) {
      ctx->Const.MaxTextureLevels = 9;
   } else if (i810Screen->textureSize < 8*1024*1024) {
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
   ctx->Const.MaxPointSize = 3.0;
   ctx->Const.MaxPointSizeAA = 3.0;
   ctx->Const.PointSizeGranularity = 1.0;

   ctx->Driver.GetBufferSize = i810BufferSize;
   ctx->Driver.ResizeBuffers = _swrast_alloc_buffers;
   ctx->Driver.GetString = i810GetString;

   /* Who owns who?
    */
   ctx->DriverCtx = (void *) imesa;
   imesa->glCtx = ctx;

   /* Initialize the software rasterizer and helper modules.
    */
   _swrast_CreateContext( ctx );
   _ac_CreateContext( ctx );
   _tnl_CreateContext( ctx );
   _swsetup_CreateContext( ctx );

   /* Install the customized pipeline:
    */
   _tnl_destroy_pipeline( ctx );
   _tnl_install_pipeline( ctx, i810_pipeline );

   /* Configure swrast to match hardware characteristics:
    */
   _swrast_allow_pixel_fog( ctx, GL_FALSE );
   _swrast_allow_vertex_fog( ctx, GL_TRUE );

   /* Dri stuff
    */
   imesa->display = dpy;
   imesa->hHWContext = driContextPriv->hHWContext;
   imesa->driFd = sPriv->fd;
   imesa->driHwLock = &sPriv->pSAREA->lock;

   imesa->i810Screen = i810Screen;
   imesa->driScreen = sPriv;
   imesa->sarea = saPriv;
   imesa->glBuffer = NULL;

   imesa->texHeap = mmInit( 0, i810Screen->textureSize );
   imesa->stipple_in_hw = 1;
   imesa->RenderIndex = ~0;
   imesa->dirty = I810_UPLOAD_CTX|I810_UPLOAD_BUFFERS;
   imesa->upload_cliprects = GL_TRUE;

   make_empty_list(&imesa->TexObjList);
   make_empty_list(&imesa->SwappedOut);

   imesa->CurrentTexObj[0] = 0;
   imesa->CurrentTexObj[1] = 0;

   _math_matrix_ctr( &imesa->ViewportMatrix );

   i810InitExtensions( ctx );
   i810InitStateFuncs( ctx );
   i810InitTextureFuncs( ctx );
   i810InitTriFuncs( ctx );
   i810InitSpanFuncs( ctx );
   i810InitIoctlFuncs( ctx );
   i810InitVB( ctx );
   i810InitState( ctx );

   return GL_TRUE;
}

void
i810DestroyContext(__DRIcontextPrivate *driContextPriv)
{
   i810ContextPtr imesa = (i810ContextPtr) driContextPriv->driverPrivate;

   assert(imesa); /* should never be null */
   if (imesa) {

      _swsetup_DestroyContext( imesa->glCtx );
      _tnl_DestroyContext( imesa->glCtx );
      _ac_DestroyContext( imesa->glCtx );
      _swrast_DestroyContext( imesa->glCtx );

      i810FreeVB( imesa->glCtx );

      /* free the Mesa context */
      imesa->glCtx->DriverCtx = NULL;
      _mesa_destroy_context(imesa->glCtx);

      Xfree(imesa);
   }
}


void i810XMesaSetFrontClipRects( i810ContextPtr imesa )
{
   __DRIdrawablePrivate *dPriv = imesa->driDrawable;

   imesa->numClipRects = dPriv->numClipRects;
   imesa->pClipRects = dPriv->pClipRects;
   imesa->drawX = dPriv->x;
   imesa->drawY = dPriv->y;

   i810EmitDrawingRectangle( imesa );
   imesa->upload_cliprects = GL_TRUE;
}


void i810XMesaSetBackClipRects( i810ContextPtr imesa )
{
   __DRIdrawablePrivate *dPriv = imesa->driDrawable;

   if (dPriv->numBackClipRects == 0)
   {
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

   i810EmitDrawingRectangle( imesa );
   imesa->upload_cliprects = GL_TRUE;
}


static void i810XMesaWindowMoved( i810ContextPtr imesa )
{
   switch (imesa->glCtx->Color.DriverDrawBuffer) {
   case GL_BACK_LEFT:
      i810XMesaSetBackClipRects( imesa );
      break;
   case GL_FRONT_LEFT:
   default:
      i810XMesaSetFrontClipRects( imesa );
      break;
   }

}


GLboolean
i810UnbindContext(__DRIcontextPrivate *driContextPriv)
{
   i810ContextPtr imesa = (i810ContextPtr) driContextPriv->driverPrivate;
   if (imesa) {
      imesa->dirty = I810_UPLOAD_CTX|I810_UPLOAD_BUFFERS;
      if (imesa->CurrentTexObj[0]) imesa->dirty |= I810_UPLOAD_TEX0;
      if (imesa->CurrentTexObj[1]) imesa->dirty |= I810_UPLOAD_TEX1;
   }

   return GL_TRUE;
}


GLboolean
i810MakeCurrent(__DRIcontextPrivate *driContextPriv,
                __DRIdrawablePrivate *driDrawPriv,
                __DRIdrawablePrivate *driReadPriv)
{
   if (driContextPriv) {
      i810ContextPtr imesa = (i810ContextPtr) driContextPriv->driverPrivate;

      /* Shouldn't the readbuffer be stored also?
       */
      imesa->driDrawable = driDrawPriv;

      _mesa_make_current2(imesa->glCtx,
                          (GLframebuffer *) driDrawPriv->driverPrivate,
                          (GLframebuffer *) driReadPriv->driverPrivate);

      /* Are these necessary?
       */
      i810XMesaWindowMoved( imesa );
      if (!imesa->glCtx->Viewport.Width)
	 _mesa_set_viewport(imesa->glCtx, 0, 0,
                            driDrawPriv->w, driDrawPriv->h);
   }
   else {
      _mesa_make_current(0,0);
   }

   return GL_TRUE;
}


void i810GetLock( i810ContextPtr imesa, GLuint flags )
{
   __DRIdrawablePrivate *dPriv = imesa->driDrawable;
   __DRIscreenPrivate *sPriv = imesa->driScreen;
   I810SAREAPtr sarea = imesa->sarea;
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
      imesa->dirty = I810_UPLOAD_CTX|I810_UPLOAD_BUFFERS;
      if (imesa->CurrentTexObj[0]) imesa->dirty |= I810_UPLOAD_TEX0;
      if (imesa->CurrentTexObj[1]) imesa->dirty |= I810_UPLOAD_TEX1;
      sarea->ctxOwner = me;
   }

   /* Shared texture managment - if another client has played with
    * texture space, figure out which if any of our textures have been
    * ejected, and update our global LRU.
    */
   if (sarea->texAge != imesa->texAge) {
      int sz = 1 << (imesa->i810Screen->logTextureGranularity);
      int idx, nr = 0;

      /* Have to go right round from the back to ensure stuff ends up
       * LRU in our local list...
       */
      for (idx = sarea->texList[I810_NR_TEX_REGIONS].prev ;
	   idx != I810_NR_TEX_REGIONS && nr < I810_NR_TEX_REGIONS ;
	   idx = sarea->texList[idx].prev, nr++)
      {
	 if (sarea->texList[idx].age > imesa->texAge)
	    i810TexturesGone(imesa, idx * sz, sz, sarea->texList[idx].in_use);
      }

      if (nr == I810_NR_TEX_REGIONS) {
	 i810TexturesGone(imesa, 0, imesa->i810Screen->textureSize, 0);
	 i810ResetGlobalLRU( imesa );
      }

      imesa->texAge = sarea->texAge;
   }

   if (imesa->lastStamp != dPriv->lastStamp) {
      i810XMesaWindowMoved( imesa );
      imesa->lastStamp = dPriv->lastStamp;
   }
}


void
i810SwapBuffers(Display *dpy, void *drawablePrivate)
{
   __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;
   (void) dpy;

   if (dPriv->driContextPriv && dPriv->driContextPriv->driverPrivate) {
      i810ContextPtr imesa;
      GLcontext *ctx;
      imesa = (i810ContextPtr) dPriv->driContextPriv->driverPrivate;
      ctx = imesa->glCtx;
      if (ctx->Visual.doubleBufferMode) {
         _mesa_swapbuffers( ctx );  /* flush pending rendering comands */
         if ( imesa->doPageFlip ) {
            i810PageFlip( dPriv );
         }
         else {
            i810CopyBuffer( dPriv );
         }
      }
   }
   else {
      /* XXX this shouldn't be an error but we can't handle it for now */
      _mesa_problem(NULL, "i810SwapBuffers: drawable has no context!\n");
   }
}
