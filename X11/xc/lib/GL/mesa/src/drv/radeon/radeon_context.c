/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_context.c,v 1.7 2003/02/08 21:26:45 dawes Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     VA Linux Systems Inc., Fremont, California.

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

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */


#include "radeon_context.h"
#include "radeon_ioctl.h"
#include "radeon_state.h"
#include "radeon_span.h"
#include "radeon_tex.h"
#include "radeon_swtcl.h"
#include "radeon_tcl.h"
#include "radeon_vtxfmt.h"
#include "radeon_maos.h"

#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "array_cache/acache.h"

#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"

#include "api_arrayelt.h"
#include "context.h"
#include "simple_list.h"
#include "mem.h"
#include "matrix.h"
#include "extensions.h"
#if defined(USE_X86_ASM)
#include "X86/common_x86_asm.h"
#endif

#define RADEON_DATE	"20020611"

#ifndef RADEON_DEBUG
int RADEON_DEBUG = (0);
#endif



/* Return the width and height of the given buffer.
 */
static void radeonGetBufferSize( GLframebuffer *buffer,
				 GLuint *width, GLuint *height )
{
   GET_CURRENT_CONTEXT(ctx);
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);

   LOCK_HARDWARE( rmesa );
   *width  = rmesa->dri.drawable->w;
   *height = rmesa->dri.drawable->h;
   UNLOCK_HARDWARE( rmesa );
}

/* Return various strings for glGetString().
 */
static const GLubyte *radeonGetString( GLcontext *ctx, GLenum name )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   static char buffer[128];

   switch ( name ) {
   case GL_VENDOR:
      return (GLubyte *)"Tungsten Graphics, Inc.";

   case GL_RENDERER:
      sprintf( buffer, "Mesa DRI Radeon " RADEON_DATE);

      /* Append any chipset-specific information.  None yet.
       */

      /* Append any AGP-specific information.
       */
      switch ( rmesa->radeonScreen->AGPMode ) {
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

      if ( rmesa->dri.drmMinor < 3 ) {
	 strncat( buffer, " DRM-COMPAT", 11 );
      }
	 
      if ( !(rmesa->TclFallback & RADEON_TCL_FALLBACK_TCL_DISABLE) ) {
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


/* Extension strings exported by the R100 driver.
 */
static const char * const radeon_extensions[] =
{
    "GL_ARB_multisample",
    "GL_ARB_multitexture",
    "GL_ARB_texture_border_clamp",
    "GL_ARB_texture_compression",
    "GL_ARB_texture_env_add",
    "GL_ARB_texture_env_combine",
    "GL_ARB_texture_env_dot3",
    "GL_ARB_texture_mirrored_repeat",
    "GL_EXT_blend_logic_op",
    "GL_EXT_blend_subtract",
/*    "GL_EXT_fog_coord", */
    "GL_EXT_secondary_color",
    "GL_EXT_texture_env_add",
    "GL_EXT_texture_env_combine",
    "GL_EXT_texture_env_dot3",
    "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_lod_bias",
    "GL_ATI_texture_mirror_once",
    "GL_IBM_texture_mirrored_repeat",
    "GL_NV_blend_square",
    "GL_SGIS_generate_mipmap",
    "GL_SGIS_texture_border_clamp",
    NULL
};

/* Initialize the extensions supported by this driver.
 */
static void radeonInitExtensions( GLcontext *ctx )
{
   unsigned   i;
   _mesa_enable_imaging_extensions( ctx );

   for ( i = 0 ; radeon_extensions[i] != NULL ; i++ ) {
      _mesa_enable_extension( ctx, radeon_extensions[i] );
   }
}

extern const struct gl_pipeline_stage _radeon_render_stage;
extern const struct gl_pipeline_stage _radeon_tcl_stage;

static const struct gl_pipeline_stage *radeon_pipeline[] = {

   /* Try and go straight to t&l
    */
   &_radeon_tcl_stage,  

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
   &_radeon_render_stage,
   &_tnl_render_stage,		/* FALLBACK:  */
   0,
};



/* Initialize the driver's misc functions.
 */
static void radeonInitDriverFuncs( GLcontext *ctx )
{
    ctx->Driver.GetBufferSize		= radeonGetBufferSize;
    ctx->Driver.ResizeBuffers           = _swrast_alloc_buffers;
    ctx->Driver.GetString		= radeonGetString;

    ctx->Driver.Error			= NULL;
    ctx->Driver.DrawPixels		= NULL;
    ctx->Driver.Bitmap			= NULL;
}



/* Create the device specific context.
 */
static GLboolean
radeonCreateContext( Display *dpy, const __GLcontextModes *glVisual,
                     __DRIcontextPrivate *driContextPriv,
                     void *sharedContextPrivate)
{
   __DRIscreenPrivate *sPriv = driContextPriv->driScreenPriv;
   radeonScreenPtr radeonScreen = (radeonScreenPtr)(sPriv->private);
   radeonContextPtr rmesa;
   GLcontext *ctx, *shareCtx;
   int i;

   assert(dpy);
   assert(glVisual);
   assert(driContextPriv);
   assert(radeonScreen);

   /* Allocate the Radeon context */
   rmesa = (radeonContextPtr) CALLOC( sizeof(*rmesa) );
   if ( !rmesa )
      return GL_FALSE;

   /* Allocate the Mesa context */
   if (sharedContextPrivate)
      shareCtx = ((radeonContextPtr) sharedContextPrivate)->glCtx;
   else
      shareCtx = NULL;
   rmesa->glCtx = _mesa_create_context(glVisual, shareCtx, rmesa, GL_TRUE);
   if (!rmesa->glCtx) {
      FREE(rmesa);
      return GL_FALSE;
   }
   driContextPriv->driverPrivate = rmesa;

   /* Init radeon context data */
   rmesa->dri.display = dpy;
   rmesa->dri.context = driContextPriv;
   rmesa->dri.screen = sPriv;
   rmesa->dri.drawable = NULL; /* Set by XMesaMakeCurrent */
   rmesa->dri.hwContext = driContextPriv->hHWContext;
   rmesa->dri.hwLock = &sPriv->pSAREA->lock;
   rmesa->dri.fd = sPriv->fd;

   /* If we don't have 1.3, fallback to the 1.1 interfaces.
    */
   if (getenv("RADEON_COMPAT") || sPriv->drmMinor < 3 ) 
      rmesa->dri.drmMinor = 1;
   else
      rmesa->dri.drmMinor = sPriv->drmMinor;

   rmesa->radeonScreen = radeonScreen;
   rmesa->sarea = (RADEONSAREAPrivPtr)((GLubyte *)sPriv->pSAREA +
				       radeonScreen->sarea_priv_offset);


   rmesa->dma.buf0_address = rmesa->radeonScreen->buffers->list[0].address;

   for ( i = 0 ; i < radeonScreen->numTexHeaps ; i++ ) {
      make_empty_list( &rmesa->texture.objects[i] );
      rmesa->texture.heap[i] = mmInit( 0, radeonScreen->texSize[i] );
      rmesa->texture.age[i] = -1;
   }
   rmesa->texture.numHeaps = radeonScreen->numTexHeaps;
   make_empty_list( &rmesa->texture.swapped );

   rmesa->swtcl.RenderIndex = ~0;
   rmesa->lost_context = 1;

   /* KW: Set the maximum texture size small enough that we can
    * guarentee that both texture units can bind a maximal texture
    * and have them both in on-card memory at once.
    * Test for 2 textures * 4 bytes/texel * size * size.
    */
   ctx = rmesa->glCtx;
   if (radeonScreen->texSize[RADEON_CARD_HEAP] >= 2 * 4 * 2048 * 2048) {
      ctx->Const.MaxTextureLevels = 12; /* 2048x2048 */
   }
   else if (radeonScreen->texSize[RADEON_CARD_HEAP] >= 2 * 4 * 1024 * 1024) {
      ctx->Const.MaxTextureLevels = 11; /* 1024x1024 */
   }
   else if (radeonScreen->texSize[RADEON_CARD_HEAP] >= 2 * 4 * 512 * 512) {
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

   /* Set maxlocksize (and hence vb size) small enough to avoid
    * fallbacks in radeon_tcl.c.  ie. guarentee that all vertices can
    * fit in a single dma buffer for indexed rendering of quad strips,
    * etc.
    */
   ctx->Const.MaxArrayLockSize = 
      MIN2( ctx->Const.MaxArrayLockSize,
  	    RADEON_BUFFER_SIZE / RADEON_MAX_TCL_VERTSIZE );

   if (getenv("LIBGL_PERFORMANCE_BOXES"))
      rmesa->boxes = 1;
   else
      rmesa->boxes = 0;


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
   _tnl_install_pipeline( ctx, radeon_pipeline );

   /* Try and keep materials and vertices separate:
    */
   _tnl_isolate_materials( ctx, GL_TRUE );


/*     _mesa_allow_light_in_model( ctx, GL_FALSE ); */

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

   radeonInitExtensions( ctx );
   radeonInitDriverFuncs( ctx );
   radeonInitIoctlFuncs( ctx );
   radeonInitStateFuncs( ctx );
   radeonInitSpanFuncs( ctx );
   radeonInitTextureFuncs( ctx );
   radeonInitState( rmesa );
   radeonInitSwtcl( ctx );

   rmesa->do_irqs = (rmesa->radeonScreen->irq && !getenv("RADEON_NO_IRQS"));
   rmesa->irqsEmitted = 0;
   rmesa->iw.irq_seq = -1;

   rmesa->do_usleeps = !getenv("RADEON_NO_USLEEPS");
   
#if DO_DEBUG
   if (getenv("RADEON_DEBUG_FALLBACKS"))
      RADEON_DEBUG |= DEBUG_FALLBACKS;

   if (getenv("RADEON_DEBUG_TEXTURE"))
      RADEON_DEBUG |= DEBUG_TEXTURE;

   if (getenv("RADEON_DEBUG_IOCTL"))
      RADEON_DEBUG |= DEBUG_IOCTL;

   if (getenv("RADEON_DEBUG_PRIMS"))
      RADEON_DEBUG |= DEBUG_PRIMS;

   if (getenv("RADEON_DEBUG_VERTS"))
      RADEON_DEBUG |= DEBUG_VERTS;

   if (getenv("RADEON_DEBUG_STATE"))
      RADEON_DEBUG |= DEBUG_STATE;

   if (getenv("RADEON_DEBUG_CODEGEN"))
      RADEON_DEBUG |= DEBUG_CODEGEN;

   if (getenv("RADEON_DEBUG_VTXFMT"))
      RADEON_DEBUG |= DEBUG_VFMT;

   if (getenv("RADEON_DEBUG_VERBOSE"))
      RADEON_DEBUG |= DEBUG_VERBOSE;

   if (getenv("RADEON_DEBUG_DRI"))
      RADEON_DEBUG |= DEBUG_DRI;

   if (getenv("RADEON_DEBUG_DMA"))
      RADEON_DEBUG |= DEBUG_DMA;

   if (getenv("RADEON_DEBUG_SANITY"))
      RADEON_DEBUG |= DEBUG_SANITY;

   if (getenv("RADEON_DEBUG"))
   {
      const char *debug = getenv("RADEON_DEBUG");
      if (strstr(debug, "fall")) 
         RADEON_DEBUG |= DEBUG_FALLBACKS;

      if (strstr(debug, "tex")) 
         RADEON_DEBUG |= DEBUG_TEXTURE;

      if (strstr(debug, "ioctl")) 
         RADEON_DEBUG |= DEBUG_IOCTL;

      if (strstr(debug, "prim")) 
         RADEON_DEBUG |= DEBUG_PRIMS;

      if (strstr(debug, "vert")) 
         RADEON_DEBUG |= DEBUG_VERTS;

      if (strstr(debug, "state")) 
         RADEON_DEBUG |= DEBUG_STATE;

      if (strstr(debug, "code")) 
         RADEON_DEBUG |= DEBUG_CODEGEN;

      if (strstr(debug, "vfmt") || strstr(debug, "vtxf")) 
         RADEON_DEBUG |= DEBUG_VFMT;

      if (strstr(debug, "verb")) 
         RADEON_DEBUG |= DEBUG_VERBOSE;

      if (strstr(debug, "dri")) 
         RADEON_DEBUG |= DEBUG_DRI;

      if (strstr(debug, "dma")) 
         RADEON_DEBUG |= DEBUG_DMA;

      if (strstr(debug, "san")) 
         RADEON_DEBUG |= DEBUG_SANITY;
   }


#endif

   if (getenv("RADEON_NO_RAST")) {
      fprintf(stderr, "disabling 3D acceleration\n");
      FALLBACK(rmesa, RADEON_FALLBACK_DISABLE, 1); 
   }
   else if (getenv("RADEON_TCL_FORCE_ENABLE")) {
      fprintf(stderr, "Enabling TCL support...  this will probably crash\n");
      fprintf(stderr, "         your card if it isn't capable of TCL!\n");
      rmesa->radeonScreen->chipset |= RADEON_CHIPSET_TCL;
   } else if (getenv("RADEON_TCL_FORCE_DISABLE") ||
	    rmesa->dri.drmMinor < 3 ||
	    !(rmesa->radeonScreen->chipset & RADEON_CHIPSET_TCL)) {
      rmesa->radeonScreen->chipset &= ~RADEON_CHIPSET_TCL;
      fprintf(stderr, "disabling TCL support\n");
      TCL_FALLBACK(rmesa->glCtx, RADEON_TCL_FALLBACK_TCL_DISABLE, 1); 
   }

   if (rmesa->radeonScreen->chipset & RADEON_CHIPSET_TCL) {
      if (!getenv("RADEON_NO_VTXFMT"))
	 radeonVtxfmtInit( ctx );

      _tnl_need_dlist_norm_lengths( ctx, GL_FALSE );
   }
   return GL_TRUE;
}


/* Destroy the device specific context.
 */
/* Destroy the Mesa and driver specific context data.
 */
static void
radeonDestroyContext( __DRIcontextPrivate *driContextPriv )
{
   GET_CURRENT_CONTEXT(ctx);
   radeonContextPtr rmesa = (radeonContextPtr) driContextPriv->driverPrivate;
   radeonContextPtr current = ctx ? RADEON_CONTEXT(ctx) : NULL;

   /* check if we're deleting the currently bound context */
   if (rmesa == current) {
      RADEON_FIREVERTICES( rmesa );
      _mesa_make_current2(NULL, NULL, NULL);
   }

   /* Free radeon context resources */
   assert(rmesa); /* should never be null */
   if ( rmesa ) {
      if (rmesa->glCtx->Shared->RefCount == 1) {
         /* This share group is about to go away, free our private
          * texture object data.
          */
         radeonTexObjPtr t, next_t;
         int i;

         for ( i = 0 ; i < rmesa->texture.numHeaps ; i++ ) {
            foreach_s ( t, next_t, &rmesa->texture.objects[i] ) {
               radeonDestroyTexObj( rmesa, t );
            }
            mmDestroy( rmesa->texture.heap[i] );
	    rmesa->texture.heap[i] = NULL;
         }

         foreach_s ( t, next_t, &rmesa->texture.swapped ) {
            radeonDestroyTexObj( rmesa, t );
         }
      }

      _swsetup_DestroyContext( rmesa->glCtx );
      _tnl_DestroyContext( rmesa->glCtx );
      _ac_DestroyContext( rmesa->glCtx );
      _swrast_DestroyContext( rmesa->glCtx );

      radeonDestroySwtcl( rmesa->glCtx );

      radeonReleaseArrays( rmesa->glCtx, ~0 );
      if (rmesa->dma.current.buf) {
	 radeonReleaseDmaRegion( rmesa, &rmesa->dma.current, __FUNCTION__ );
	 radeonFlushCmdBuf( rmesa, __FUNCTION__ );
      }

      if (!rmesa->TclFallback & RADEON_TCL_FALLBACK_TCL_DISABLE)
	 if (!getenv("RADEON_NO_VTXFMT"))
	    radeonVtxfmtDestroy( rmesa->glCtx );

      /* free the Mesa context */
      rmesa->glCtx->DriverCtx = NULL;
      _mesa_destroy_context( rmesa->glCtx );

      if (rmesa->state.scissor.pClipRects) {
	 FREE(rmesa->state.scissor.pClipRects);
	 rmesa->state.scissor.pClipRects = 0;
      }

      FREE( rmesa );
   }

#if 0
   /* Use this to force shared object profiling. */
   glx_fini_prof();
#endif
}


/* Initialize the driver specific screen private data.
 */
static GLboolean
radeonInitDriver( __DRIscreenPrivate *sPriv )
{
   sPriv->private = (void *) radeonCreateScreen( sPriv );
   if ( !sPriv->private ) {
      radeonDestroyScreen( sPriv );
      return GL_FALSE;
   }

   return GL_TRUE;
}


/* Create and initialize the Mesa and driver specific pixmap buffer
 * data.
 */
static GLboolean
radeonCreateBuffer( Display *dpy,
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
radeonDestroyBuffer(__DRIdrawablePrivate *driDrawPriv)
{
   _mesa_destroy_framebuffer((GLframebuffer *) (driDrawPriv->driverPrivate));
}



static void
radeonSwapBuffers(Display *dpy, void *drawablePrivate)
{
   __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;
   (void) dpy;

   if (dPriv->driContextPriv && dPriv->driContextPriv->driverPrivate) {
      radeonContextPtr rmesa;
      GLcontext *ctx;
      rmesa = (radeonContextPtr) dPriv->driContextPriv->driverPrivate;
      ctx = rmesa->glCtx;
      if (ctx->Visual.doubleBufferMode) {
         _mesa_swapbuffers( ctx );  /* flush pending rendering comands */

         if ( rmesa->doPageFlip ) {
            radeonPageFlip( dPriv );
         }
         else {
            radeonCopyBuffer( dPriv );
         }
      }
   }
   else {
      /* XXX this shouldn't be an error but we can't handle it for now */
      _mesa_problem(NULL, "radeonSwapBuffers: drawable has no context!\n");
   }
}


/* Force the context `c' to be the current context and associate with it
 * buffer `b'.
 */
static GLboolean
radeonMakeCurrent( __DRIcontextPrivate *driContextPriv,
                   __DRIdrawablePrivate *driDrawPriv,
                   __DRIdrawablePrivate *driReadPriv )
{
   if ( driContextPriv ) {
      radeonContextPtr newRadeonCtx = 
	 (radeonContextPtr) driContextPriv->driverPrivate;

      if (RADEON_DEBUG & DEBUG_DRI)
	 fprintf(stderr, "%s ctx %p\n", __FUNCTION__, newRadeonCtx->glCtx);

      if ( newRadeonCtx->dri.drawable != driDrawPriv ) {
	 newRadeonCtx->dri.drawable = driDrawPriv;
	 radeonUpdateWindow( newRadeonCtx->glCtx );
	 radeonUpdateViewportOffset( newRadeonCtx->glCtx );
      }

      _mesa_make_current2( newRadeonCtx->glCtx,
			   (GLframebuffer *) driDrawPriv->driverPrivate,
			   (GLframebuffer *) driReadPriv->driverPrivate );

      if ( !newRadeonCtx->glCtx->Viewport.Width ) {
	 _mesa_set_viewport( newRadeonCtx->glCtx, 0, 0,
			     driDrawPriv->w, driDrawPriv->h );
      }

      if (newRadeonCtx->vb.enabled)
	 radeonVtxfmtMakeCurrent( newRadeonCtx->glCtx );

   } else {
      if (RADEON_DEBUG & DEBUG_DRI)
	 fprintf(stderr, "%s ctx %p\n", __FUNCTION__, NULL);
      _mesa_make_current( 0, 0 );
   }

   if (RADEON_DEBUG & DEBUG_DRI)
      fprintf(stderr, "End %s\n", __FUNCTION__);
   return GL_TRUE;
}

/* Force the context `c' to be unbound from its buffer.
 */
static GLboolean
radeonUnbindContext( __DRIcontextPrivate *driContextPriv )
{
   radeonContextPtr rmesa = (radeonContextPtr) driContextPriv->driverPrivate;

   if (RADEON_DEBUG & DEBUG_DRI)
      fprintf(stderr, "%s ctx %p\n", __FUNCTION__, rmesa->glCtx);

   radeonVtxfmtUnbindContext( rmesa->glCtx );
   return GL_TRUE;
}

/* Fullscreen mode isn't used for much -- could be a way to shrink
 * front/back buffers & get more texture memory if the client has
 * changed the video resolution.
 * 
 * Pageflipping is now done automatically whenever there is a single
 * 3d client.
 */
static GLboolean
radeonOpenCloseFullScreen( __DRIcontextPrivate *driContextPriv )
{
   return GL_TRUE;
}



/* This function is called by libGL.so as soon as libGL.so is loaded.
 * This is where we'd register new extension functions with the dispatcher.
 */
void
__driRegisterExtensions( void )
{
}



static struct __DriverAPIRec radeonAPI = {
   radeonInitDriver,
   radeonDestroyScreen,
   radeonCreateContext,
   radeonDestroyContext,
   radeonCreateBuffer,
   radeonDestroyBuffer,
   radeonSwapBuffers,
   radeonMakeCurrent,
   radeonUnbindContext,
   radeonOpenCloseFullScreen,
   radeonOpenCloseFullScreen
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
   psp = __driUtilCreateScreen(dpy, scrn, psc, numConfigs, config, &radeonAPI);
   return (void *) psp;
}
