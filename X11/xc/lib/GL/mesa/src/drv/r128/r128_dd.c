/* $XFree86: xc/lib/GL/mesa/src/drv/r128/r128_dd.c,v 1.15 2002/10/30 12:51:38 alanh Exp $ */
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
 *   Gareth Hughes <gareth@valinux.com>
 *   Kevin E. Martin <martin@valinux.com>
 *
 */

#include "r128_context.h"
#include "r128_ioctl.h"
#include "r128_state.h"
#include "r128_vb.h"
#include "r128_dd.h"

#include "context.h"
#include "extensions.h"
#if defined(USE_X86_ASM)
#include "X86/common_x86_asm.h"
#endif

#define R128_DATE	"20020221"


/* Return the width and height of the current color buffer.
 */
static void r128DDGetBufferSize( GLframebuffer *buffer,
				 GLuint *width, GLuint *height )
{
   GET_CURRENT_CONTEXT(ctx);
   r128ContextPtr rmesa = R128_CONTEXT(ctx);

   LOCK_HARDWARE( rmesa );
   *width  = rmesa->driDrawable->w;
   *height = rmesa->driDrawable->h;
   UNLOCK_HARDWARE( rmesa );
}

/* Return various strings for glGetString().
 */
static const GLubyte *r128DDGetString( GLcontext *ctx, GLenum name )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   static char buffer[128];

   switch ( name ) {
   case GL_VENDOR:
      return (GLubyte *)"VA Linux Systems, Inc.";

   case GL_RENDERER:
      sprintf( buffer, "Mesa DRI Rage128 " R128_DATE );

      /* Append any chipset-specific information.
       */
      if ( R128_IS_PRO( rmesa ) ) {
	 strncat( buffer, " Pro", 4 );
      }
      if ( R128_IS_MOBILITY( rmesa ) ) {
	 strncat( buffer, " M3", 3 );
      }

      /* Append any AGP-specific information.
       */
      switch ( rmesa->r128Screen->AGPMode ) {
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
      return (GLubyte *)buffer;

   default:
      return NULL;
   }
}

/* Send all commands to the hardware.  If vertex buffers or indirect
 * buffers are in use, then we need to make sure they are sent to the
 * hardware.  All commands that are normally sent to the ring are
 * already considered `flushed'.
 */
static void r128DDFlush( GLcontext *ctx )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);

   FLUSH_BATCH( rmesa );

#if ENABLE_PERF_BOXES
   if ( rmesa->boxes ) {
      LOCK_HARDWARE( rmesa );
      r128PerformanceBoxesLocked( rmesa );
      UNLOCK_HARDWARE( rmesa );
   }

   /* Log the performance counters if necessary */
   r128PerformanceCounters( rmesa );
#endif
}

/* Make sure all commands have been sent to the hardware and have
 * completed processing.
 */
static void r128DDFinish( GLcontext *ctx )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);

#if ENABLE_PERF_BOXES
   /* Bump the performance counter */
   rmesa->c_drawWaits++;
#endif

   r128DDFlush( ctx );
   r128WaitForIdle( rmesa );
}


/* Initialize the extensions supported by this driver.
 */
void r128DDInitExtensions( GLcontext *ctx )
{
   _mesa_enable_extension( ctx, "GL_ARB_multitexture" );
   _mesa_enable_extension( ctx, "GL_ARB_texture_env_add" );
   _mesa_enable_extension( ctx, "GL_EXT_texture_env_add" );
   _mesa_enable_imaging_extensions( ctx );
}

/* Initialize the driver's misc functions.
 */
void r128DDInitDriverFuncs( GLcontext *ctx )
{
   ctx->Driver.GetBufferSize	= r128DDGetBufferSize;
   ctx->Driver.ResizeBuffers    = _swrast_alloc_buffers;
   ctx->Driver.GetString	= r128DDGetString;
   ctx->Driver.Finish		= r128DDFinish;
   ctx->Driver.Flush		= r128DDFlush;
   ctx->Driver.Error		= NULL;
}
