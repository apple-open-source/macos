
/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */



/*
 * Pixel buffer:
 *
 * As fragments are produced (by point, line, and bitmap drawing) they
 * are accumlated in a buffer.  When the buffer is full or has to be
 * flushed (glEnd), we apply all enabled rasterization functions to the
 * pixels and write the results to the display buffer.  The goal is to
 * maximize the number of pixels processed inside loops and to minimize
 * the number of function calls.
 */


#include "glheader.h"
#include "macros.h"
#include "mem.h"

#include "s_alpha.h"
#include "s_alphabuf.h"
#include "s_blend.h"
#include "s_context.h"
#include "s_depth.h"
#include "s_fog.h"
#include "s_logic.h"
#include "s_masking.h"
#include "s_pb.h"
#include "s_scissor.h"
#include "s_stencil.h"
#include "s_texture.h"



/*
 * Allocate and initialize a new pixel buffer structure.
 */
struct pixel_buffer *_mesa_alloc_pb(void)
{
   struct pixel_buffer *pb;
   pb = CALLOC_STRUCT(pixel_buffer);
   if (pb) {
      int i, j;
      /* set non-zero fields */
      pb->mono = GL_TRUE;

      /* Set all lambda values to 0.0 since we don't do mipmapping for
       * points or lines and want to use the level 0 texture image.
       */
      for (j=0;j<MAX_TEXTURE_UNITS;j++) {
         for (i=0; i<PB_SIZE; i++) {
            pb->lambda[j][i] = 0.0;
         }
      }
   }
   return pb;
}



/*
 * Draw to more than one color buffer (or none).
 */
static void multi_write_index_pixels( GLcontext *ctx, GLuint n,
                                      const GLint x[], const GLint y[],
                                      const GLuint indexes[],
                                      const GLubyte mask[] )
{
   SWcontext *swrast = SWRAST_CONTEXT(ctx);
   GLuint bufferBit;

   if (ctx->Color.DrawBuffer == GL_NONE)
      return;

   /* loop over four possible dest color buffers */
   for (bufferBit = 1; bufferBit <= 8; bufferBit = bufferBit << 1) {
      if (bufferBit & ctx->Color.DrawDestMask) {
         GLuint indexTmp[PB_SIZE];
         ASSERT(n < PB_SIZE);

         if (bufferBit == FRONT_LEFT_BIT)
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_FRONT_LEFT);
         else if (bufferBit == FRONT_RIGHT_BIT)
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_FRONT_RIGHT);
         else if (bufferBit == BACK_LEFT_BIT)
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_BACK_LEFT);
         else
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_BACK_RIGHT);

         /* make copy of incoming indexes */
         MEMCPY( indexTmp, indexes, n * sizeof(GLuint) );
         if (ctx->Color.IndexLogicOpEnabled) {
            _mesa_logicop_ci_pixels( ctx, n, x, y, indexTmp, mask );
         }
         if (ctx->Color.IndexMask != 0xffffffff) {
            _mesa_mask_index_pixels( ctx, n, x, y, indexTmp, mask );
         }
         (*swrast->Driver.WriteCI32Pixels)( ctx, n, x, y, indexTmp, mask );
      }
   }

   /* restore default dest buffer */
   (void) (*ctx->Driver.SetDrawBuffer)( ctx, ctx->Color.DriverDrawBuffer);
}



/*
 * Draw to more than one RGBA color buffer (or none).
 */
static void multi_write_rgba_pixels( GLcontext *ctx, GLuint n,
                                     const GLint x[], const GLint y[],
                                     CONST GLchan rgba[][4],
                                     const GLubyte mask[] )
{
   SWcontext *swrast = SWRAST_CONTEXT(ctx);
   GLuint bufferBit;

   if (ctx->Color.DrawBuffer == GL_NONE)
      return;

   /* loop over four possible dest color buffers */
   for (bufferBit = 1; bufferBit <= 8; bufferBit = bufferBit << 1) {
      if (bufferBit & ctx->Color.DrawDestMask) {
         GLchan rgbaTmp[PB_SIZE][4];
         ASSERT(n < PB_SIZE);

         if (bufferBit == FRONT_LEFT_BIT) {
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_FRONT_LEFT);
            ctx->DrawBuffer->Alpha = ctx->DrawBuffer->FrontLeftAlpha;
         }
         else if (bufferBit == FRONT_RIGHT_BIT) {
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_FRONT_RIGHT);
            ctx->DrawBuffer->Alpha = ctx->DrawBuffer->FrontRightAlpha;
         }
         else if (bufferBit == BACK_LEFT_BIT) {
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_BACK_LEFT);
            ctx->DrawBuffer->Alpha = ctx->DrawBuffer->BackLeftAlpha;
         }
         else {
            (void) (*ctx->Driver.SetDrawBuffer)( ctx, GL_BACK_RIGHT);
            ctx->DrawBuffer->Alpha = ctx->DrawBuffer->BackRightAlpha;
         }

         /* make copy of incoming colors */
         MEMCPY( rgbaTmp, rgba, 4 * n * sizeof(GLchan) );

         if (ctx->Color.ColorLogicOpEnabled) {
            _mesa_logicop_rgba_pixels( ctx, n, x, y, rgbaTmp, mask );
         }
         else if (ctx->Color.BlendEnabled) {
            _mesa_blend_pixels( ctx, n, x, y, rgbaTmp, mask );
         }
         if (*((GLuint *) &ctx->Color.ColorMask) != 0xffffffff) {
            _mesa_mask_rgba_pixels( ctx, n, x, y, rgbaTmp, mask );
         }

         (*swrast->Driver.WriteRGBAPixels)( ctx, n, x, y,
					 (const GLchan (*)[4])rgbaTmp, mask );
         if (SWRAST_CONTEXT(ctx)->_RasterMask & ALPHABUF_BIT) {
            _mesa_write_alpha_pixels( ctx, n, x, y,
                                      (const GLchan (*)[4])rgbaTmp, mask );
         }
      }
   }

   /* restore default dest buffer */
   (void) (*ctx->Driver.SetDrawBuffer)( ctx, ctx->Color.DriverDrawBuffer);
}



/*
 * Add specular color to primary color.  This is used only when
 * GL_LIGHT_MODEL_COLOR_CONTROL = GL_SEPARATE_SPECULAR_COLOR.
 */
static void add_colors( GLuint n, GLchan rgba[][4], CONST GLchan spec[][3] )
{
   GLuint i;
   for (i=0; i<n; i++) {
      GLint r = rgba[i][RCOMP] + spec[i][RCOMP];
      GLint g = rgba[i][GCOMP] + spec[i][GCOMP];
      GLint b = rgba[i][BCOMP] + spec[i][BCOMP];
      rgba[i][RCOMP] = MIN2(r, CHAN_MAX);
      rgba[i][GCOMP] = MIN2(g, CHAN_MAX);
      rgba[i][BCOMP] = MIN2(b, CHAN_MAX);
   }
}



/*
 * When the pixel buffer is full, or needs to be flushed, call this
 * function.  All the pixels in the pixel buffer will be subjected
 * to texturing, scissoring, stippling, alpha testing, stenciling,
 * depth testing, blending, and finally written to the frame buffer.
 */
void _mesa_flush_pb( GLcontext *ctx )
{
   SWcontext *swrast = SWRAST_CONTEXT(ctx);
   GLuint RasterMask = swrast->_RasterMask;

   /* Pixel colors may be changed if any of these raster ops enabled */
   const GLuint modBits = FOG_BIT | TEXTURE_BIT | BLEND_BIT |
                          MASKING_BIT | LOGIC_OP_BIT;
   struct pixel_buffer *PB = swrast->PB;
   GLubyte mask[PB_SIZE];

   if (PB->count == 0)
      goto CleanUp;

   /* initialize mask array and clip pixels simultaneously */
   {
      const GLint xmin = ctx->DrawBuffer->_Xmin;
      const GLint xmax = ctx->DrawBuffer->_Xmax;
      const GLint ymin = ctx->DrawBuffer->_Ymin;
      const GLint ymax = ctx->DrawBuffer->_Ymax;
      const GLuint n = PB->count;
      GLint *x = PB->x;
      GLint *y = PB->y;
      GLuint i;
      for (i = 0; i < n; i++) {
         mask[i] = (x[i] >= xmin) & (x[i] < xmax) & (y[i] >= ymin) & (y[i] < ymax);
      }
   }

   if (ctx->Visual.rgbMode) {
      /*
       * RGBA COLOR PIXELS
       */

      /* If each pixel can be of a different color... */
      if ((RasterMask & modBits) || !PB->mono) {

         if (PB->mono) {
            /* copy mono color into rgba array */
            GLuint i;
            for (i = 0; i < PB->count; i++) {
               COPY_CHAN4(PB->rgba[i], PB->currentColor);
            }
         }

	 if (ctx->Texture._ReallyEnabled) {
            GLchan primary_rgba[PB_SIZE][4];
            GLuint texUnit;

            /* must make a copy of primary colors since they may be modified */
            MEMCPY(primary_rgba, PB->rgba, 4 * PB->count * sizeof(GLchan));

            for (texUnit = 0; texUnit < ctx->Const.MaxTextureUnits; texUnit++){
               _swrast_texture_fragments( ctx, texUnit, PB->count,
                                          PB->s[texUnit], PB->t[texUnit],
                                          PB->u[texUnit], PB->lambda[texUnit],
                                          (CONST GLchan (*)[4]) primary_rgba,
                                          PB->rgba );
            }
	 }

         if ((ctx->Fog.ColorSumEnabled ||
              (ctx->Light.Model.ColorControl == GL_SEPARATE_SPECULAR_COLOR
               && ctx->Light.Enabled)) && PB->haveSpec) {
            /* add specular color to primary color */
            add_colors( PB->count, PB->rgba, (const GLchan (*)[3]) PB->spec );
         }

	 if (ctx->Fog.Enabled) {
	    if (swrast->_PreferPixelFog)
	       _mesa_depth_fog_rgba_pixels( ctx, PB->count, PB->z, PB->rgba );
	    else
	       _mesa_fog_rgba_pixels( ctx, PB->count, PB->fog, PB->rgba );
	 }

         /* Antialias coverage application */
         if (PB->haveCoverage) {
            const GLuint n = PB->count;
            GLuint i;
            for (i = 0; i < n; i++) {
               PB->rgba[i][ACOMP] = (GLchan) (PB->rgba[i][ACOMP] * PB->coverage[i]);
            }
         }

         /* Scissoring already done above */

	 if (ctx->Color.AlphaEnabled) {
	    if (_mesa_alpha_test( ctx, PB->count,
                                  (const GLchan (*)[4]) PB->rgba, mask )==0) {
	       goto CleanUp;
	    }
	 }

	 if (ctx->Stencil.Enabled) {
	    /* first stencil test */
	    if (_mesa_stencil_and_ztest_pixels(ctx, PB->count,
                                       PB->x, PB->y, PB->z, mask) == 0) {
	       goto CleanUp;
	    }
	 }
	 else if (ctx->Depth.Test) {
	    /* regular depth testing */
	    _mesa_depth_test_pixels( ctx, PB->count, PB->x, PB->y, PB->z, mask );
	 }


         if (RasterMask & MULTI_DRAW_BIT) {
            multi_write_rgba_pixels( ctx, PB->count, PB->x, PB->y,
                                     (const GLchan (*)[4])PB->rgba, mask );
         }
         else {
            /* normal case: write to exactly one buffer */
            const GLuint colorMask = *((GLuint *) ctx->Color.ColorMask);

            if (ctx->Color.ColorLogicOpEnabled) {
               _mesa_logicop_rgba_pixels( ctx, PB->count, PB->x, PB->y,
                                          PB->rgba, mask);
            }
            else if (ctx->Color.BlendEnabled) {
               _mesa_blend_pixels( ctx, PB->count, PB->x, PB->y, PB->rgba, mask);
            }
            if (colorMask == 0x0) {
               goto CleanUp;
            }
            else if (colorMask != 0xffffffff) {
               _mesa_mask_rgba_pixels(ctx, PB->count, PB->x, PB->y, PB->rgba, mask);
            }

            (*swrast->Driver.WriteRGBAPixels)( ctx, PB->count, PB->x, PB->y,
                                            (const GLchan (*)[4]) PB->rgba,
					    mask );
            if (RasterMask & ALPHABUF_BIT) {
               _mesa_write_alpha_pixels( ctx, PB->count, PB->x, PB->y,
				      (const GLchan (*)[4]) PB->rgba, mask );
            }
         }
      }
      else {
	 /* Same color for all pixels */

         /* Scissoring already done above */

	 if (ctx->Color.AlphaEnabled) {
	    if (_mesa_alpha_test( ctx, PB->count,
                                  (const GLchan (*)[4]) PB->rgba, mask )==0) {
	       goto CleanUp;
	    }
	 }

	 if (ctx->Stencil.Enabled) {
	    /* first stencil test */
	    if (_mesa_stencil_and_ztest_pixels(ctx, PB->count,
                                       PB->x, PB->y, PB->z, mask) == 0) {
	       goto CleanUp;
	    }
	 }
	 else if (ctx->Depth.Test) {
	    /* regular depth testing */
	    _mesa_depth_test_pixels( ctx, PB->count, PB->x, PB->y, PB->z, mask );
	 }

         if (ctx->Color.DrawBuffer == GL_NONE) {
            goto CleanUp;
         }

         if (RasterMask & MULTI_DRAW_BIT) {
            if (PB->mono) {
               /* copy mono color into rgba array */
               GLuint i;
               for (i = 0; i < PB->count; i++) {
                  COPY_CHAN4(PB->rgba[i], PB->currentColor);
               }
            }
            multi_write_rgba_pixels( ctx, PB->count, PB->x, PB->y,
                                     (const GLchan (*)[4]) PB->rgba, mask );
         }
         else {
            /* normal case: write to exactly one buffer */
            (*swrast->Driver.WriteMonoRGBAPixels)( ctx, PB->count, PB->x, PB->y,
                                                PB->currentColor, mask );
            if (RasterMask & ALPHABUF_BIT) {
               _mesa_write_mono_alpha_pixels( ctx, PB->count, PB->x, PB->y,
                                              PB->currentColor[ACOMP], mask );
            }
         }
         /*** ALL DONE ***/
      }
   }
   else {
      /*
       * COLOR INDEX PIXELS
       */

      /* If we may be writting pixels with different indexes... */
      if ((RasterMask & modBits) || !PB->mono) {

         if (PB->mono) {
            GLuint i;
            for (i = 0; i < PB->count; i++) {
               PB->index[i] = PB->currentIndex;
            }
         }
	 if (ctx->Fog.Enabled) {
	    if (swrast->_PreferPixelFog)
	       _mesa_depth_fog_ci_pixels( ctx, PB->count, PB->z, PB->index );
	    else
	       _mesa_fog_ci_pixels( ctx, PB->count, PB->fog, PB->index );
	 }

         /* Antialias coverage application */
         if (PB->haveCoverage) {
            const GLuint n = PB->count;
            GLuint i;
            for (i = 0; i < n; i++) {
               GLint frac = (GLint) (15.0 * PB->coverage[i]);
               PB->index[i] = (PB->index[i] & ~0xf) | frac;
            }
         }

         /* Scissoring already done above */

	 if (ctx->Stencil.Enabled) {
	    /* first stencil test */
	    if (_mesa_stencil_and_ztest_pixels(ctx, PB->count,
                                       PB->x, PB->y, PB->z, mask) == 0) {
	       goto CleanUp;
	    }
	 }
	 else if (ctx->Depth.Test) {
	    /* regular depth testing */
	    _mesa_depth_test_pixels( ctx, PB->count, PB->x, PB->y, PB->z, mask );
	 }
         if (RasterMask & MULTI_DRAW_BIT) {
            multi_write_index_pixels( ctx, PB->count, PB->x, PB->y, PB->index, mask );
         }
         else {
            /* normal case: write to exactly one buffer */

            if (ctx->Color.IndexLogicOpEnabled) {
               _mesa_logicop_ci_pixels(ctx, PB->count, PB->x, PB->y,
                                       PB->index, mask);
            }
            if (ctx->Color.IndexMask != 0xffffffff) {
               _mesa_mask_index_pixels(ctx, PB->count, PB->x, PB->y,
                                       PB->index, mask);
            }
            (*swrast->Driver.WriteCI32Pixels)( ctx, PB->count, PB->x, PB->y,
                                            PB->index, mask );
         }

         /*** ALL DONE ***/
      }
      else {
	 /* Same color index for all pixels */

         /* Scissoring already done above */

	 if (ctx->Stencil.Enabled) {
	    /* first stencil test */
	    if (_mesa_stencil_and_ztest_pixels(ctx, PB->count,
                                       PB->x, PB->y, PB->z, mask) == 0) {
	       goto CleanUp;
	    }
	 }
	 else if (ctx->Depth.Test) {
	    /* regular depth testing */
	    _mesa_depth_test_pixels(ctx, PB->count, PB->x, PB->y, PB->z, mask);
	 }

         if (RasterMask & MULTI_DRAW_BIT) {
            multi_write_index_pixels(ctx, PB->count, PB->x, PB->y,
                                     PB->index, mask);
         }
         else {
            /* normal case: write to exactly one buffer */
            (*swrast->Driver.WriteMonoCIPixels)(ctx, PB->count, PB->x, PB->y,
                                             PB->currentIndex, mask);
         }
      }
   }

CleanUp:
   PB->count = 0;
   PB->mono = GL_TRUE;
   PB->haveSpec = GL_FALSE;
   PB->haveCoverage = GL_FALSE;
}


void
_swrast_flush( GLcontext *ctx )
{
   if (SWRAST_CONTEXT(ctx)->PB->count > 0)
      _mesa_flush_pb(ctx);
}
