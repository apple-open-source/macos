/* $XFree86: xc/lib/GL/mesa/src/drv/r128/r128_context.c,v 1.8 2002/10/30 12:51:38 alanh Exp $ */
/**************************************************************************

Copyright 1999, 2000 ATI Technologies Inc. and Precision Insight, Inc.,
                                               Cedar Park, Texas.
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
ATI, PRECISION INSIGHT AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
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

#include <stdlib.h>

#include "r128_context.h"
#include "r128_ioctl.h"
#include "r128_dd.h"
#include "r128_state.h"
#include "r128_span.h"
#include "r128_tex.h"
#include "r128_tris.h"
#include "r128_vb.h"


#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "array_cache/acache.h"

#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"

#include "context.h"
#include "simple_list.h"
#include "mem.h"
#include "matrix.h"

#ifndef R128_DEBUG
int R128_DEBUG = (0
/*		  | DEBUG_ALWAYS_SYNC */
/*		  | DEBUG_VERBOSE_API */
/*		  | DEBUG_VERBOSE_MSG */
/*		  | DEBUG_VERBOSE_LRU */
/*		  | DEBUG_VERBOSE_DRI */
/*		  | DEBUG_VERBOSE_IOCTL */
/*		  | DEBUG_VERBOSE_2D */
   );
#endif

/* Create the device specific context.
 */
GLboolean r128CreateContext( Display *dpy, const __GLcontextModes *glVisual,
			     __DRIcontextPrivate *driContextPriv,
                             void *sharedContextPrivate )
{
   GLcontext *ctx, *shareCtx;
   __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
   r128ContextPtr rmesa;
   r128ScreenPtr r128scrn;
   int i;

   /* Allocate the r128 context */
   rmesa = (r128ContextPtr) CALLOC( sizeof(*rmesa) );
   if ( !rmesa )
      return GL_FALSE;

   /* Allocate the Mesa context */
   if (sharedContextPrivate)
      shareCtx = ((r128ContextPtr) sharedContextPrivate)->glCtx;
   else 
      shareCtx = NULL;
   rmesa->glCtx = _mesa_create_context(glVisual, shareCtx, rmesa, GL_TRUE);
   if (!rmesa->glCtx) {
      FREE(rmesa);
      return GL_FALSE;
   }
   driContextPriv->driverPrivate = rmesa;
   ctx = rmesa->glCtx;

   rmesa->display = dpy;
   rmesa->driContext = driContextPriv;
   rmesa->driScreen = sPriv;
   rmesa->driDrawable = NULL;
   rmesa->hHWContext = driContextPriv->hHWContext;
   rmesa->driHwLock = &sPriv->pSAREA->lock;
   rmesa->driFd = sPriv->fd;

   r128scrn = rmesa->r128Screen = (r128ScreenPtr)(sPriv->private);

   rmesa->sarea = (R128SAREAPrivPtr)((char *)sPriv->pSAREA +
				     r128scrn->sarea_priv_offset);

   rmesa->CurrentTexObj[0] = NULL;
   rmesa->CurrentTexObj[1] = NULL;

   make_empty_list( &rmesa->SwappedOut );

   for ( i = 0 ; i < r128scrn->numTexHeaps ; i++ ) {
      make_empty_list( &rmesa->TexObjList[i] );
      rmesa->texHeap[i] = mmInit( 0, r128scrn->texSize[i] );
      rmesa->lastTexAge[i] = -1;
   }
   rmesa->lastTexHeap = r128scrn->numTexHeaps;

   rmesa->RenderIndex = -1;		/* Impossible value */
   rmesa->vert_buf = NULL;
   rmesa->num_verts = 0;

   /* KW: Set the maximum texture size small enough that we can
    * guarentee that both texture units can bind a maximal texture
    * and have them both in on-card memory at once.  (Kevin or
    * Gareth: Please check these numbers are OK)
    */
   if ( r128scrn->texSize[0] < 2*1024*1024 ) {
      ctx->Const.MaxTextureLevels = 9;
   } else if ( r128scrn->texSize[0] < 8*1024*1024 ) {
      ctx->Const.MaxTextureLevels = 10;
   } else {
      ctx->Const.MaxTextureLevels = 11;
   }

   ctx->Const.MaxTextureUnits = 2;

   /* No wide points.
    */
   ctx->Const.MinPointSize = 1.0;
   ctx->Const.MinPointSizeAA = 1.0;
   ctx->Const.MaxPointSize = 1.0;
   ctx->Const.MaxPointSizeAA = 1.0;

   /* No wide lines.
    */
   ctx->Const.MinLineWidth = 1.0;
   ctx->Const.MinLineWidthAA = 1.0;
   ctx->Const.MaxLineWidth = 1.0;
   ctx->Const.MaxLineWidthAA = 1.0;
   ctx->Const.LineWidthGranularity = 1.0;

#if ENABLE_PERF_BOXES
   if ( getenv( "LIBGL_PERFORMANCE_BOXES" ) ) {
      rmesa->boxes = 1;
   } else {
      rmesa->boxes = 0;
   }
#endif

   /* Initialize the software rasterizer and helper modules.
    */
   _swrast_CreateContext( ctx );
   _ac_CreateContext( ctx );
   _tnl_CreateContext( ctx );
   _swsetup_CreateContext( ctx );

   /* Install the customized pipeline:
    */
/*     _tnl_destroy_pipeline( ctx ); */
/*     _tnl_install_pipeline( ctx, r128_pipeline ); */

   /* Configure swrast to match hardware characteristics:
    */
   _swrast_allow_pixel_fog( ctx, GL_FALSE );
   _swrast_allow_vertex_fog( ctx, GL_TRUE );

   r128InitVB( ctx );
   r128InitTriFuncs( ctx );
   r128DDInitExtensions( ctx );
   r128DDInitDriverFuncs( ctx );
   r128DDInitIoctlFuncs( ctx );
   r128DDInitStateFuncs( ctx );
   r128DDInitSpanFuncs( ctx );
   r128DDInitTextureFuncs( ctx );
   r128DDInitState( rmesa );

   driContextPriv->driverPrivate = (void *)rmesa;

   return GL_TRUE;
}

/* Destroy the device specific context.
 */
void r128DestroyContext( __DRIcontextPrivate *driContextPriv  )
{
   r128ContextPtr rmesa = (r128ContextPtr) driContextPriv->driverPrivate;

   assert(rmesa);  /* should never be null */
   if ( rmesa ) {
      if (rmesa->glCtx->Shared->RefCount == 1) {
         /* This share group is about to go away, free our private
          * texture object data.
          */
         r128TexObjPtr t, next_t;
         int i;

         for ( i = 0 ; i < rmesa->r128Screen->numTexHeaps ; i++ ) {
            foreach_s ( t, next_t, &rmesa->TexObjList[i] ) {
               r128DestroyTexObj( rmesa, t );
            }
            mmDestroy( rmesa->texHeap[i] );
	    rmesa->texHeap[i] = NULL;
         }

         foreach_s ( t, next_t, &rmesa->SwappedOut ) {
            r128DestroyTexObj( rmesa, t );
         }
      }

      _swsetup_DestroyContext( rmesa->glCtx );
      _tnl_DestroyContext( rmesa->glCtx );
      _ac_DestroyContext( rmesa->glCtx );
      _swrast_DestroyContext( rmesa->glCtx );

      r128FreeVB( rmesa->glCtx );

      /* free the Mesa context */
      rmesa->glCtx->DriverCtx = NULL;
      _mesa_destroy_context(rmesa->glCtx);

      FREE( rmesa );
   }

#if 0
   /* Use this to force shared object profiling. */
   glx_fini_prof();
#endif
}


/* Force the context `c' to be the current context and associate with it
 * buffer `b'.
 */
GLboolean
r128MakeCurrent( __DRIcontextPrivate *driContextPriv,
                 __DRIdrawablePrivate *driDrawPriv,
                 __DRIdrawablePrivate *driReadPriv )
{
   if ( driContextPriv ) {
      GET_CURRENT_CONTEXT(ctx);
      r128ContextPtr oldR128Ctx = ctx ? R128_CONTEXT(ctx) : NULL;
      r128ContextPtr newR128Ctx = (r128ContextPtr) driContextPriv->driverPrivate;

      if ( newR128Ctx != oldR128Ctx ) {
	 newR128Ctx->new_state |= R128_NEW_CONTEXT;
	 newR128Ctx->dirty = R128_UPLOAD_ALL;
      }

      newR128Ctx->driDrawable = driDrawPriv;

      _mesa_make_current2( newR128Ctx->glCtx,
                           (GLframebuffer *) driDrawPriv->driverPrivate,
                           (GLframebuffer *) driReadPriv->driverPrivate );


      newR128Ctx->new_state |= R128_NEW_WINDOW | R128_NEW_CLIP;

      if ( !newR128Ctx->glCtx->Viewport.Width ) {
	 _mesa_set_viewport(newR128Ctx->glCtx, 0, 0,
                            driDrawPriv->w, driDrawPriv->h);
      }
   } else {
      _mesa_make_current( 0, 0 );
   }

   return GL_TRUE;
}


/* Force the context `c' to be unbound from its buffer.
 */
GLboolean
r128UnbindContext( __DRIcontextPrivate *driContextPriv )
{
   return GL_TRUE;
}
