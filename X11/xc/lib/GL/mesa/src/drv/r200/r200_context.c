/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_context.c,v 1.2 2002/12/16 16:18:53 dawes Exp $ */
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


#include "r200_context.h"
#include "r200_ioctl.h"
#include "r200_state.h"
#include "r200_span.h"
#include "r200_pixel.h"
#include "r200_tex.h"
#include "r200_swtcl.h"
#include "r200_tcl.h"
#include "r200_vtxfmt.h"
#include "r200_maos.h"

#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "array_cache/acache.h"

#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"

#include "attrib.h"
#include "api_arrayelt.h"
#include "context.h"
#include "simple_list.h"
#include "mem.h"
#include "matrix.h"
#include "state.h"
#include "extensions.h"
#include "state.h"
#if defined(USE_X86_ASM)
#include "X86/common_x86_asm.h"
#endif

#define R200_DATE	"20020827"

#ifndef R200_DEBUG
int R200_DEBUG = (0);
#endif



/* Return the width and height of the given buffer.
 */
static void r200GetBufferSize( GLframebuffer *buffer,
			       GLuint *width, GLuint *height )
{
   GET_CURRENT_CONTEXT(ctx);
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   LOCK_HARDWARE( rmesa );
   *width  = rmesa->dri.drawable->w;
   *height = rmesa->dri.drawable->h;
   UNLOCK_HARDWARE( rmesa );
}

/* Return various strings for glGetString().
 */
static const GLubyte *r200GetString( GLcontext *ctx, GLenum name )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   static char buffer[128];

   switch ( name ) {
   case GL_VENDOR:
      return (GLubyte *)"Tungsten Graphics, Inc.";

   case GL_RENDERER:
      sprintf( buffer, "Mesa DRI R200 " R200_DATE);

      /* Append any chipset-specific information.  None yet.
       */

      /* Append any AGP-specific information.
       */
      switch ( rmesa->r200Screen->AGPMode ) {
      case 1:
	 strncat( buffer, " AGP 1x", 7 );
	 break;
      case 2:
	 strncat( buffer, " AGP 2x", 7 );
	 break;
      case 4:
	 strncat( buffer, " AGP 4x", 7 );
	 break;
      }

      /* Append any CPU-specific information.
       */
#ifdef USE_X86_ASM
      if ( _mesa_x86_cpu_features ) {
	 strncat( buffer, " x86", 4 );
      }
#ifdef USE_MMX_ASM
      if ( cpu_has_mmx ) {
	 strncat( buffer, "/MMX", 4 );
      }
#endif
#ifdef USE_3DNOW_ASM
      if ( cpu_has_3dnow ) {
	 strncat( buffer, "/3DNow!", 7 );
      }
#endif
#ifdef USE_SSE_ASM
      if ( cpu_has_xmm ) {
	 strncat( buffer, "/SSE", 4 );
      }
#endif
#endif

      if ( !(rmesa->TclFallback & R200_TCL_FALLBACK_TCL_DISABLE) ) {
	 strncat( buffer, " TCL", 4 );
      }
      else {
	 strncat( buffer, " NO-TCL", 7 );
      }

      return (GLubyte *)buffer;

   default:
      return NULL;
   }
}



/* Initialize the extensions supported by this driver.
 */
static void r200InitExtensions( GLcontext *ctx )
{
   _mesa_enable_imaging_extensions( ctx );

   _mesa_enable_extension( ctx, "GL_ARB_multitexture" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_combine" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_dot3" );

   _mesa_enable_extension( ctx, "GL_EXT_blend_logic_op" );
   _mesa_enable_extension( ctx, "GL_EXT_stencil_wrap" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_combine" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_dot3" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_filter_anisotropic" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_lod_bias" );
   _mesa_enable_extension( ctx, "GL_EXT_secondary_color" );
   _mesa_enable_extension( ctx, "GL_EXT_blend_subtract" );
   _mesa_enable_extension( ctx, "GL_EXT_blend_minmax" );

/*     _mesa_enable_extension( ctx, "GL_EXT_fog_coord" ); */

   _mesa_enable_extension( ctx, "GL_MESA_pack_invert" );
   _mesa_enable_extension( ctx, "GL_MESA_ycbcr_texture" );
   _mesa_enable_extension( ctx, "GL_NV_texture_rectangle" );

}

extern const struct gl_pipeline_stage _r200_render_stage;
extern const struct gl_pipeline_stage _r200_tcl_stage;

static const struct gl_pipeline_stage *r200_pipeline[] = {

   /* Try and go straight to t&l
    */
   &_r200_tcl_stage,  

   /* Catch any t&l fallbacks
    */
   &_tnl_vertex_transform_stage,
   &_tnl_normal_transform_stage,
   &_tnl_lighting_stage,
   &_tnl_fog_coordinate_stage,
   &_tnl_texgen_stage,
   &_tnl_texture_transform_stage,

   /* Try again to go to tcl? 
    *     - no good for asymmetric-twoside (do with multipass)
    *     - no good for asymmetric-unfilled (do with multipass)
    *     - good for material
    *     - good for texgen
    *     - need to manipulate a bit of state
    *
    * - worth it/not worth it?
    */
			
   /* Else do them here.
    */
   &_r200_render_stage, 
   &_tnl_render_stage,		/* FALLBACK:  */
   0,
};



/* Initialize the driver's misc functions.
 */
static void r200InitDriverFuncs( GLcontext *ctx )
{
    ctx->Driver.GetBufferSize		= r200GetBufferSize;
    ctx->Driver.ResizeBuffers           = _swrast_alloc_buffers;
    ctx->Driver.GetString		= r200GetString;

    ctx->Driver.Error			= NULL;
    ctx->Driver.DrawPixels		= NULL;
    ctx->Driver.Bitmap			= NULL;
}

static void add_debug_flags( const char *debug )
{
   if (strstr(debug, "fall")) 
      R200_DEBUG |= DEBUG_FALLBACKS;

   if (strstr(debug, "tex")) 
      R200_DEBUG |= DEBUG_TEXTURE;

   if (strstr(debug, "ioctl")) 
      R200_DEBUG |= DEBUG_IOCTL;

   if (strstr(debug, "prim")) 
      R200_DEBUG |= DEBUG_PRIMS;

   if (strstr(debug, "vert")) 
      R200_DEBUG |= DEBUG_VERTS;

   if (strstr(debug, "state")) 
      R200_DEBUG |= DEBUG_STATE;

   if (strstr(debug, "code")) 
      R200_DEBUG |= DEBUG_CODEGEN;

   if (strstr(debug, "vfmt") || strstr(debug, "vtxf")) 
      R200_DEBUG |= DEBUG_VFMT;

   if (strstr(debug, "verb")) 
      R200_DEBUG |= DEBUG_VERBOSE;

   if (strstr(debug, "dri")) 
      R200_DEBUG |= DEBUG_DRI;

   if (strstr(debug, "dma")) 
      R200_DEBUG |= DEBUG_DMA;

   if (strstr(debug, "san")) 
      R200_DEBUG |= DEBUG_SANITY;

   if (strstr(debug, "sync")) 
      R200_DEBUG |= DEBUG_SYNC;

   if (strstr(debug, "pix")) 
      R200_DEBUG |= DEBUG_PIXEL;
}

/* Create the device specific context.
 */
GLboolean r200CreateContext( Display *dpy, const __GLcontextModes *glVisual,
			     __DRIcontextPrivate *driContextPriv,
			     void *sharedContextPrivate)
{
   __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
   r200ScreenPtr r200Screen = (r200ScreenPtr)(sPriv->private);
   r200ContextPtr rmesa;
   GLcontext *ctx, *shareCtx;
   int i;

   assert(dpy);
   assert(glVisual);
   assert(driContextPriv);
   assert(r200Screen);

   /* Allocate the R200 context */
   rmesa = (r200ContextPtr) CALLOC( sizeof(*rmesa) );
   if ( !rmesa )
      return GL_FALSE;

   /* Allocate the Mesa context */
   if (sharedContextPrivate)
      shareCtx = ((r200ContextPtr) sharedContextPrivate)->glCtx;
   else
      shareCtx = NULL;
   rmesa->glCtx = _mesa_create_context(glVisual, shareCtx, rmesa, GL_TRUE);
   if (!rmesa->glCtx) {
      FREE(rmesa);
      return GL_FALSE;
   }
   driContextPriv->driverPrivate = rmesa;

   /* Init r200 context data */
   rmesa->dri.display = dpy;
   rmesa->dri.context = driContextPriv;
   rmesa->dri.screen = sPriv;
   rmesa->dri.drawable = NULL; /* Set by XMesaMakeCurrent */
   rmesa->dri.hwContext = driContextPriv->hHWContext;
   rmesa->dri.hwLock = &sPriv->pSAREA->lock;
   rmesa->dri.fd = sPriv->fd;
   rmesa->dri.drmMinor = sPriv->drmMinor;

   rmesa->r200Screen = r200Screen;
   rmesa->sarea = (RADEONSAREAPrivPtr)((GLubyte *)sPriv->pSAREA +
				       r200Screen->sarea_priv_offset);


   rmesa->dma.buf0_address = rmesa->r200Screen->buffers->list[0].address;

   for ( i = 0 ; i < r200Screen->numTexHeaps ; i++ ) {
      make_empty_list( &rmesa->texture.objects[i] );
      rmesa->texture.heap[i] = mmInit( 0, r200Screen->texSize[i] );
      rmesa->texture.age[i] = -1;
   }
   rmesa->texture.numHeaps = r200Screen->numTexHeaps;
   make_empty_list( &rmesa->texture.swapped );

   rmesa->swtcl.RenderIndex = ~0;
   rmesa->lost_context = 1;

   /* KW: Set the maximum texture size small enough that we can
    * guarentee that both texture units can bind a maximal texture
    * and have them both in on-card memory at once.
    * Test for 2 textures * 4 bytes/texel * size * size.
    */
   ctx = rmesa->glCtx;
   if (r200Screen->texSize[RADEON_CARD_HEAP] >= 2 * 4 * 2048 * 2048) {
      ctx->Const.MaxTextureLevels = 12; /* 2048x2048 */
   }
   else if (r200Screen->texSize[RADEON_CARD_HEAP] >= 2 * 4 * 1024 * 1024) {
      ctx->Const.MaxTextureLevels = 11; /* 1024x1024 */
   }
   else if (r200Screen->texSize[RADEON_CARD_HEAP] >= 2 * 4 * 512 * 512) {
      ctx->Const.MaxTextureLevels = 10; /* 512x512 */
   }
   else {
      ctx->Const.MaxTextureLevels = 9; /* 256x256 */
   }

   ctx->Const.MaxTextureUnits = 2;
   ctx->Const.MaxTextureMaxAnisotropy = 16.0;

   /* No wide points.
    */
   ctx->Const.MinPointSize = 1.0;
   ctx->Const.MinPointSizeAA = 1.0;
   ctx->Const.MaxPointSize = 1.0;
   ctx->Const.MaxPointSizeAA = 1.0;

   ctx->Const.MinLineWidth = 1.0;
   ctx->Const.MinLineWidthAA = 1.0;
   ctx->Const.MaxLineWidth = 10.0;
   ctx->Const.MaxLineWidthAA = 10.0;
   ctx->Const.LineWidthGranularity = 0.0625;


   /* Initialize the software rasterizer and helper modules.
    */
   _swrast_CreateContext( ctx );
   _ac_CreateContext( ctx );
   _tnl_CreateContext( ctx );
   _swsetup_CreateContext( ctx );
   _ae_create_context( ctx );

   /* Install the customized pipeline:
    */
   _tnl_destroy_pipeline( ctx );
   _tnl_install_pipeline( ctx, r200_pipeline );

   /* Try and keep materials and vertices separate:
    */
   _tnl_isolate_materials( ctx, GL_TRUE );


   /* Configure swrast to match hardware characteristics:
    */
   _swrast_allow_pixel_fog( ctx, GL_FALSE );
   _swrast_allow_vertex_fog( ctx, GL_TRUE );


   _math_matrix_ctr( &rmesa->TexGenMatrix[0] );
   _math_matrix_ctr( &rmesa->TexGenMatrix[1] );
   _math_matrix_ctr( &rmesa->tmpmat );
   _math_matrix_set_identity( &rmesa->TexGenMatrix[0] );
   _math_matrix_set_identity( &rmesa->TexGenMatrix[1] );
   _math_matrix_set_identity( &rmesa->tmpmat );

   r200InitExtensions( ctx );
   r200InitDriverFuncs( ctx );
   r200InitIoctlFuncs( ctx );
   r200InitStateFuncs( ctx );
   r200InitSpanFuncs( ctx );
   r200InitPixelFuncs( ctx );
   r200InitTextureFuncs( ctx );
   r200InitState( rmesa );
   r200InitSwtcl( ctx );

   rmesa->iw.irq_seq = -1;
   rmesa->irqsEmitted = 0;
   rmesa->do_irqs = (rmesa->dri.drmMinor >= 6 && 
		     !getenv("R200_NO_IRQS") &&
		     rmesa->r200Screen->irq);

   if (!rmesa->do_irqs)
      fprintf(stderr, 
	      "IRQ's not enabled, falling back to busy waits: %d %d %d\n",
	      rmesa->dri.drmMinor,
	      !!getenv("R200_NO_IRQS"),
	      rmesa->r200Screen->irq);


   rmesa->do_usleeps = !getenv("R200_NO_USLEEPS");
   rmesa->prefer_agp_client_texturing = 
      (getenv("R200_AGP_CLIENT_TEXTURES") != 0);
   
   
#if DO_DEBUG
   if (getenv("R200_DEBUG"))
      add_debug_flags( getenv("R200_DEBUG") );
   if (getenv("RADEON_DEBUG"))
      add_debug_flags( getenv("RADEON_DEBUG") );
#endif

   if (getenv("R200_NO_RAST")) {
      fprintf(stderr, "disabling 3D acceleration\n");
      FALLBACK(rmesa, R200_FALLBACK_DISABLE, 1); 
   }
   else if (getenv("R200_NO_TCL")) {
      fprintf(stderr, "disabling TCL support\n");
      TCL_FALLBACK(rmesa->glCtx, R200_TCL_FALLBACK_TCL_DISABLE, 1); 
   }
   else {
      if (!getenv("R200_NO_VTXFMT")) {
	 r200VtxfmtInit( ctx );
      }
      _tnl_need_dlist_norm_lengths( ctx, GL_FALSE );
   }
   return GL_TRUE;
}


/* Destroy the device specific context.
 */
/* Destroy the Mesa and driver specific context data.
 */
void r200DestroyContext( __DRIcontextPrivate *driContextPriv )
{
   GET_CURRENT_CONTEXT(ctx);
   r200ContextPtr rmesa = (r200ContextPtr) driContextPriv->driverPrivate;
   r200ContextPtr current = ctx ? R200_CONTEXT(ctx) : NULL;

   /* check if we're deleting the currently bound context */
   if (rmesa == current) {
      R200_FIREVERTICES( rmesa );
      _mesa_make_current2(NULL, NULL, NULL);
   }

   /* Free r200 context resources */
   assert(rmesa); /* should never be null */
   if ( rmesa ) {
      if (rmesa->glCtx->Shared->RefCount == 1) {
         /* This share group is about to go away, free our private
          * texture object data.
          */
         r200TexObjPtr t, next_t;
         int i;

         for ( i = 0 ; i < rmesa->texture.numHeaps ; i++ ) {
            foreach_s ( t, next_t, &rmesa->texture.objects[i] ) {
               r200DestroyTexObj( rmesa, t );
            }
            mmDestroy( rmesa->texture.heap[i] );
	    rmesa->texture.heap[i] = NULL;
         }

         foreach_s ( t, next_t, &rmesa->texture.swapped ) {
            r200DestroyTexObj( rmesa, t );
         }
      }

      _swsetup_DestroyContext( rmesa->glCtx );
      _tnl_DestroyContext( rmesa->glCtx );
      _ac_DestroyContext( rmesa->glCtx );
      _swrast_DestroyContext( rmesa->glCtx );

      r200DestroySwtcl( rmesa->glCtx );

      r200ReleaseArrays( rmesa->glCtx, ~0 );

      if (rmesa->dma.current.buf) {
	 r200ReleaseDmaRegion( rmesa, &rmesa->dma.current, __FUNCTION__ );
	 r200FlushCmdBuf( rmesa, __FUNCTION__ );
      }

      if (!rmesa->TclFallback & R200_TCL_FALLBACK_TCL_DISABLE)
	 if (!getenv("R200_NO_VTXFMT"))
	    r200VtxfmtDestroy( rmesa->glCtx );

      /* free the Mesa context */
      rmesa->glCtx->DriverCtx = NULL;
      _mesa_destroy_context( rmesa->glCtx );

      if (rmesa->state.scissor.pClipRects) {
	 FREE(rmesa->state.scissor.pClipRects);
	 rmesa->state.scissor.pClipRects = 0;
      }

      FREE( rmesa );
   }
}




void
r200SwapBuffers(Display *dpy, void *drawablePrivate)
{
   __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;
   (void) dpy;

   if (dPriv->driContextPriv && dPriv->driContextPriv->driverPrivate) {
      r200ContextPtr rmesa;
      GLcontext *ctx;
      rmesa = (r200ContextPtr) dPriv->driContextPriv->driverPrivate;
      ctx = rmesa->glCtx;
      if (ctx->Visual.doubleBufferMode) {
         _mesa_swapbuffers( ctx );  /* flush pending rendering comands */

         if ( rmesa->doPageFlip ) {
            r200PageFlip( dPriv );
         }
         else {
            r200CopyBuffer( dPriv );
         }
      }
   }
   else {
      /* XXX this shouldn't be an error but we can't handle it for now */
      _mesa_problem(NULL, "r200SwapBuffers: drawable has no context!\n");
   }
}


/* Force the context `c' to be the current context and associate with it
 * buffer `b'.
 */
GLboolean
r200MakeCurrent( __DRIcontextPrivate *driContextPriv,
                   __DRIdrawablePrivate *driDrawPriv,
                   __DRIdrawablePrivate *driReadPriv )
{
   if ( driContextPriv ) {
      r200ContextPtr newR200Ctx = 
	 (r200ContextPtr) driContextPriv->driverPrivate;

      if (R200_DEBUG & DEBUG_DRI)
	 fprintf(stderr, "%s ctx %p\n", __FUNCTION__, newR200Ctx->glCtx);

      if ( newR200Ctx->dri.drawable != driDrawPriv ) {
	 newR200Ctx->dri.drawable = driDrawPriv;
	 r200UpdateWindow( newR200Ctx->glCtx );
	 r200UpdateViewportOffset( newR200Ctx->glCtx );
      }

      _mesa_make_current2( newR200Ctx->glCtx,
			   (GLframebuffer *) driDrawPriv->driverPrivate,
			   (GLframebuffer *) driReadPriv->driverPrivate );

      if ( !newR200Ctx->glCtx->Viewport.Width ) {
	 _mesa_set_viewport( newR200Ctx->glCtx, 0, 0,
			     driDrawPriv->w, driDrawPriv->h );
      }

      if (newR200Ctx->vb.enabled)
	 r200VtxfmtMakeCurrent( newR200Ctx->glCtx );

      _mesa_update_state( newR200Ctx->glCtx );
      r200ValidateState( newR200Ctx->glCtx );

   } else {
      if (R200_DEBUG & DEBUG_DRI)
	 fprintf(stderr, "%s ctx is null\n", __FUNCTION__);
      _mesa_make_current( 0, 0 );
   }

   if (R200_DEBUG & DEBUG_DRI)
      fprintf(stderr, "End %s\n", __FUNCTION__);
   return GL_TRUE;
}

/* Force the context `c' to be unbound from its buffer.
 */
GLboolean
r200UnbindContext( __DRIcontextPrivate *driContextPriv )
{
   r200ContextPtr rmesa = (r200ContextPtr) driContextPriv->driverPrivate;

   if (R200_DEBUG & DEBUG_DRI)
      fprintf(stderr, "%s ctx %p\n", __FUNCTION__, rmesa->glCtx);

   r200VtxfmtUnbindContext( rmesa->glCtx );
   return GL_TRUE;
}






