/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_texstate.c,v 1.6 2002/12/16 16:18:59 dawes Exp $ */
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
 *
 */

#include "radeon_context.h"
#include "radeon_state.h"
#include "radeon_ioctl.h"
#include "radeon_swtcl.h"
#include "radeon_tex.h"
#include "radeon_tcl.h"

#include "context.h"
#include "enums.h"
#include "mem.h"
#include "texformat.h"


static void radeonSetTexImages( radeonContextPtr rmesa,
				struct gl_texture_object *tObj )
{
   radeonTexObjPtr t = (radeonTexObjPtr)tObj->DriverData;
   const struct gl_texture_image *baseImage = tObj->Image[tObj->BaseLevel];
   GLint totalSize;
   GLint texelsPerDword = 0, blitWidth = 0, blitPitch = 0;
   GLint x, y, width, height;
   GLint i;
   GLint firstLevel, lastLevel, numLevels;
   GLint log2Width, log2Height;
   GLuint txformat = 0;

   /* Set the hardware texture format
    */
   switch (baseImage->TexFormat->MesaFormat) {
   case MESA_FORMAT_I8:
      txformat = RADEON_TXFORMAT_I8;
      break;
   case MESA_FORMAT_AL88:
      txformat = RADEON_TXFORMAT_AI88;
      break;
   case MESA_FORMAT_RGBA8888:
      txformat = RADEON_TXFORMAT_RGBA8888;
      break;
   case MESA_FORMAT_ARGB8888:
      txformat = RADEON_TXFORMAT_ARGB8888;
      break;
   case MESA_FORMAT_RGB565:
      txformat = RADEON_TXFORMAT_RGB565;
      break;
   case MESA_FORMAT_ARGB1555:
      txformat = RADEON_TXFORMAT_ARGB1555;
      break;
   case MESA_FORMAT_ARGB4444:
      txformat = RADEON_TXFORMAT_ARGB4444;
      break;
   default:
      _mesa_problem(NULL, "unexpected texture format in radeonTexImage2D");
      return;
   }

   t->pp_txformat &= ~(RADEON_TXFORMAT_FORMAT_MASK |
		       RADEON_TXFORMAT_ALPHA_IN_MAP);
   t->pp_txformat |= txformat;

   if ( txformat == RADEON_TXFORMAT_RGBA8888 ||
	txformat == RADEON_TXFORMAT_ARGB4444 ||
	txformat == RADEON_TXFORMAT_ARGB1555 ||
	txformat == RADEON_TXFORMAT_AI88 ) {
      t->pp_txformat |= RADEON_TXFORMAT_ALPHA_IN_MAP;
   }

   /* The Radeon has a 64-byte minimum pitch for all blits.  We
    * calculate the equivalent number of texels to simplify the
    * calculation of the texture image area.
    */
   switch ( baseImage->TexFormat->TexelBytes ) {
   case 1:
      texelsPerDword = 4;
      blitPitch = 64;
      break;
   case 2:
      texelsPerDword = 2;
      blitPitch = 32;
      break;
   case 4:
      texelsPerDword = 1;
      blitPitch = 16;
      break;
   }

   /* Select the larger of the two widths for our global texture image
    * coordinate space.  As the Radeon has very strict offset rules, we
    * can't upload mipmaps directly and have to reference their location
    * from the aligned start of the whole image.
    */
   blitWidth = MAX2( baseImage->Width, blitPitch );

   /* Calculate mipmap offsets and dimensions.
    */
   totalSize = 0;
   x = 0;
   y = 0;

   /* Compute which mipmap levels we really want to send to the hardware.
    * This depends on the base image size, GL_TEXTURE_MIN_LOD,
    * GL_TEXTURE_MAX_LOD, GL_TEXTURE_BASE_LEVEL, and GL_TEXTURE_MAX_LEVEL.
    * Yes, this looks overly complicated, but it's all needed.
    */
   firstLevel = tObj->BaseLevel + (GLint) (tObj->MinLod + 0.5);
   firstLevel = MAX2(firstLevel, tObj->BaseLevel);
   lastLevel = tObj->BaseLevel + (GLint) (tObj->MaxLod + 0.5);
   lastLevel = MAX2(lastLevel, tObj->BaseLevel);
   lastLevel = MIN2(lastLevel, tObj->BaseLevel + baseImage->MaxLog2);
   lastLevel = MIN2(lastLevel, tObj->MaxLevel);
   lastLevel = MAX2(firstLevel, lastLevel); /* need at least one level */

   /* save these values */
   t->firstLevel = firstLevel;
   t->lastLevel = lastLevel;

   numLevels = lastLevel - firstLevel + 1;

   log2Width = tObj->Image[firstLevel]->WidthLog2;
   log2Height = tObj->Image[firstLevel]->HeightLog2;

   for ( i = 0 ; i < numLevels ; i++ ) {
      const struct gl_texture_image *texImage;
      GLuint size;

      texImage = tObj->Image[i + firstLevel];
      if ( !texImage )
	 break;

      width = texImage->Width;
      height = texImage->Height;

      /* Texture images have a minimum pitch of 32 bytes (half of the
       * 64-byte minimum pitch for blits).  For images that have a
       * width smaller than this, we must pad each texture image
       * scanline out to this amount.
       */
      if ( width < blitPitch / 2 ) {
	 width = blitPitch / 2;
      }

      size = width * height * baseImage->TexFormat->TexelBytes;
      totalSize += size;
      ASSERT( (totalSize & 31) == 0 );

      while ( width < blitWidth && height > 1 ) {
	 width *= 2;
	 height /= 2;
      }

      assert(i < RADEON_MAX_TEXTURE_LEVELS);
      t->image[i].x = x;
      t->image[i].y = y;

      t->image[i].width  = width;
      t->image[i].height = height;

      /* While blits must have a pitch of at least 64 bytes, mipmaps
       * must be aligned on a 32-byte boundary (just like each texture
       * image scanline).
       */
      if ( width >= blitWidth ) {
	 y += height;
      } else {
	 x += width;
	 if ( x >= blitWidth ) {
	    x = 0;
	    y++;
	 }
      }

      if ( 0 )
	 fprintf( stderr, "level=%d p=%d   %dx%d -> %dx%d at (%d,%d)\n",
		  i, blitWidth, baseImage->Width, baseImage->Height,
		  t->image[i].width, t->image[i].height,
		  t->image[i].x, t->image[i].y );
   }

   /* Align the total size of texture memory block.
    */
   t->totalSize = (totalSize + RADEON_OFFSET_MASK) & ~RADEON_OFFSET_MASK;

   /* Hardware state:
    */
   t->pp_txfilter &= ~RADEON_MAX_MIP_LEVEL_MASK;
   t->pp_txfilter |= (numLevels - 1) << RADEON_MAX_MIP_LEVEL_SHIFT;

   t->pp_txformat &= ~(RADEON_TXFORMAT_WIDTH_MASK |
		       RADEON_TXFORMAT_HEIGHT_MASK);
   t->pp_txformat |= ((log2Width << RADEON_TXFORMAT_WIDTH_SHIFT) |
		      (log2Height << RADEON_TXFORMAT_HEIGHT_SHIFT));

   t->dirty_state = TEX_ALL;

   radeonUploadTexImages( rmesa, t );
}



/* ================================================================
 * Texture combine functions
 */

#define RADEON_DISABLE		0
#define RADEON_REPLACE		1
#define RADEON_MODULATE		2
#define RADEON_DECAL		3
#define RADEON_BLEND		4
#define RADEON_ADD		5
#define RADEON_MAX_COMBFUNC	6

static GLuint radeon_color_combine[][RADEON_MAX_COMBFUNC] =
{
   /* Unit 0:
    */
   {
      /* Disable combiner stage
       */
      (RADEON_COLOR_ARG_A_ZERO |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_CURRENT_COLOR |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_REPLACE = 0x00802800
       */
      (RADEON_COLOR_ARG_A_ZERO |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_T0_COLOR |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_MODULATE = 0x00800142
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_T0_COLOR |
       RADEON_COLOR_ARG_C_ZERO |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_DECAL = 0x008c2d42
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_T0_COLOR |
       RADEON_COLOR_ARG_C_T0_ALPHA |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_BLEND = 0x008c2902
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_TFACTOR_COLOR |
       RADEON_COLOR_ARG_C_T0_COLOR |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_ADD = 0x00812802
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_T0_COLOR |
       RADEON_COMP_ARG_B |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),
   },

   /* Unit 1:
    */
   {
      /* Disable combiner stage
       */
      (RADEON_COLOR_ARG_A_ZERO |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_CURRENT_COLOR |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_REPLACE = 0x00803000
       */
      (RADEON_COLOR_ARG_A_ZERO |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_T1_COLOR |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_MODULATE = 0x00800182
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_T1_COLOR |
       RADEON_COLOR_ARG_C_ZERO |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_DECAL = 0x008c3582
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_T1_COLOR |
       RADEON_COLOR_ARG_C_T1_ALPHA |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_BLEND = 0x008c3102
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_TFACTOR_COLOR |
       RADEON_COLOR_ARG_C_T1_COLOR |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_ADD = 0x00813002
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_T1_COLOR |
       RADEON_COMP_ARG_B |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),
   },

   /* Unit 2:
    */
   {
      /* Disable combiner stage
       */
      (RADEON_COLOR_ARG_A_ZERO |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_CURRENT_COLOR |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_REPLACE = 0x00803800
       */
      (RADEON_COLOR_ARG_A_ZERO |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_T2_COLOR |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_MODULATE = 0x008001c2
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_T2_COLOR |
       RADEON_COLOR_ARG_C_ZERO |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_DECAL = 0x008c3dc2
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_T2_COLOR |
       RADEON_COLOR_ARG_C_T2_ALPHA |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_BLEND = 0x008c3902
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_TFACTOR_COLOR |
       RADEON_COLOR_ARG_C_T2_COLOR |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_ADD = 0x00813802
       */
      (RADEON_COLOR_ARG_A_CURRENT_COLOR |
       RADEON_COLOR_ARG_B_ZERO |
       RADEON_COLOR_ARG_C_T2_COLOR |
       RADEON_COMP_ARG_B |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),
   }
};

static GLuint radeon_alpha_combine[][RADEON_MAX_COMBFUNC] =
{
   /* Unit 0:
    */
   {
      /* Disable combiner stage
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_CURRENT_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_REPLACE = 0x00800500
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_T0_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_MODULATE = 0x00800051
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_T0_ALPHA |
       RADEON_ALPHA_ARG_C_ZERO |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_DECAL = 0x00800100
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_CURRENT_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_BLEND = 0x00800051
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_TFACTOR_ALPHA |
       RADEON_ALPHA_ARG_C_T0_ALPHA |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_ADD = 0x00800051
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_T0_ALPHA |
       RADEON_COMP_ARG_B |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),
   },

   /* Unit 1:
    */
   {
      /* Disable combiner stage
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_CURRENT_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_REPLACE = 0x00800600
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_T1_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_MODULATE = 0x00800061
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_T1_ALPHA |
       RADEON_ALPHA_ARG_C_ZERO |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_DECAL = 0x00800100
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_CURRENT_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_BLEND = 0x00800061
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_TFACTOR_ALPHA |
       RADEON_ALPHA_ARG_C_T1_ALPHA |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_ADD = 0x00800061
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_T1_ALPHA |
       RADEON_COMP_ARG_B |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),
   },

   /* Unit 2:
    */
   {
      /* Disable combiner stage
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_CURRENT_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_REPLACE = 0x00800700
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_T2_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_MODULATE = 0x00800071
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_T2_ALPHA |
       RADEON_ALPHA_ARG_C_ZERO |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_DECAL = 0x00800100
       */
      (RADEON_ALPHA_ARG_A_ZERO |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_CURRENT_ALPHA |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_BLEND = 0x00800071
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_TFACTOR_ALPHA |
       RADEON_ALPHA_ARG_C_T2_ALPHA |
       RADEON_BLEND_CTL_BLEND |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),

      /* GL_ADD = 0x00800021
       */
      (RADEON_ALPHA_ARG_A_CURRENT_ALPHA |
       RADEON_ALPHA_ARG_B_ZERO |
       RADEON_ALPHA_ARG_C_T2_ALPHA |
       RADEON_COMP_ARG_B |
       RADEON_BLEND_CTL_ADD |
       RADEON_SCALE_1X |
       RADEON_CLAMP_TX),
   }
};


/* GL_EXT_texture_env_combine support
 */

/* The color tables have combine functions for GL_SRC_COLOR,
 * GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA and GL_ONE_MINUS_SRC_ALPHA.
 */
static GLuint radeon_texture_color[][RADEON_MAX_TEXTURE_UNITS] =
{
   {
      RADEON_COLOR_ARG_A_T0_COLOR,
      RADEON_COLOR_ARG_A_T1_COLOR,
      RADEON_COLOR_ARG_A_T2_COLOR
   },
   {
      RADEON_COLOR_ARG_A_T0_COLOR | RADEON_COMP_ARG_A,
      RADEON_COLOR_ARG_A_T1_COLOR | RADEON_COMP_ARG_A,
      RADEON_COLOR_ARG_A_T2_COLOR | RADEON_COMP_ARG_A
   },
   {
      RADEON_COLOR_ARG_A_T0_ALPHA,
      RADEON_COLOR_ARG_A_T1_ALPHA,
      RADEON_COLOR_ARG_A_T2_ALPHA
   },
   {
      RADEON_COLOR_ARG_A_T0_ALPHA | RADEON_COMP_ARG_A,
      RADEON_COLOR_ARG_A_T1_ALPHA | RADEON_COMP_ARG_A,
      RADEON_COLOR_ARG_A_T2_ALPHA | RADEON_COMP_ARG_A
   },
};

static GLuint radeon_tfactor_color[] =
{
   RADEON_COLOR_ARG_A_TFACTOR_COLOR,
   RADEON_COLOR_ARG_A_TFACTOR_COLOR | RADEON_COMP_ARG_A,
   RADEON_COLOR_ARG_A_TFACTOR_ALPHA,
   RADEON_COLOR_ARG_A_TFACTOR_ALPHA | RADEON_COMP_ARG_A
};

static GLuint radeon_primary_color[] =
{
   RADEON_COLOR_ARG_A_DIFFUSE_COLOR,
   RADEON_COLOR_ARG_A_DIFFUSE_COLOR | RADEON_COMP_ARG_A,
   RADEON_COLOR_ARG_A_DIFFUSE_ALPHA,
   RADEON_COLOR_ARG_A_DIFFUSE_ALPHA | RADEON_COMP_ARG_A
};

static GLuint radeon_previous_color[] =
{
   RADEON_COLOR_ARG_A_CURRENT_COLOR,
   RADEON_COLOR_ARG_A_CURRENT_COLOR | RADEON_COMP_ARG_A,
   RADEON_COLOR_ARG_A_CURRENT_ALPHA,
   RADEON_COLOR_ARG_A_CURRENT_ALPHA | RADEON_COMP_ARG_A
};

/* The alpha tables only have GL_SRC_ALPHA and GL_ONE_MINUS_SRC_ALPHA.
 */
static GLuint radeon_texture_alpha[][RADEON_MAX_TEXTURE_UNITS] =
{
   {
      RADEON_ALPHA_ARG_A_T0_ALPHA,
      RADEON_ALPHA_ARG_A_T1_ALPHA,
      RADEON_ALPHA_ARG_A_T2_ALPHA
   },
   {
      RADEON_ALPHA_ARG_A_T0_ALPHA | RADEON_COMP_ARG_A,
      RADEON_ALPHA_ARG_A_T1_ALPHA | RADEON_COMP_ARG_A,
      RADEON_ALPHA_ARG_A_T2_ALPHA | RADEON_COMP_ARG_A
   },
};

static GLuint radeon_tfactor_alpha[] =
{
   RADEON_ALPHA_ARG_A_TFACTOR_ALPHA,
   RADEON_ALPHA_ARG_A_TFACTOR_ALPHA | RADEON_COMP_ARG_A
};

static GLuint radeon_primary_alpha[] =
{
   RADEON_ALPHA_ARG_A_DIFFUSE_ALPHA,
   RADEON_ALPHA_ARG_A_DIFFUSE_ALPHA | RADEON_COMP_ARG_A
};

static GLuint radeon_previous_alpha[] =
{
   RADEON_ALPHA_ARG_A_CURRENT_ALPHA,
   RADEON_ALPHA_ARG_A_CURRENT_ALPHA | RADEON_COMP_ARG_A
};


/* Extract the arg from slot A, shift it into the correct argument slot
 * and set the corresponding complement bit.
 */
#define RADEON_COLOR_ARG( n, arg )					\
do {									\
   color_combine |=							\
      ((color_arg[n] & RADEON_COLOR_ARG_MASK)				\
       << RADEON_COLOR_ARG_##arg##_SHIFT);				\
   color_combine |=							\
      ((color_arg[n] >> RADEON_COMP_ARG_SHIFT)				\
       << RADEON_COMP_ARG_##arg##_SHIFT);				\
} while (0)

#define RADEON_ALPHA_ARG( n, arg )					\
do {									\
   alpha_combine |=							\
      ((alpha_arg[n] & RADEON_ALPHA_ARG_MASK)				\
       << RADEON_ALPHA_ARG_##arg##_SHIFT);				\
   alpha_combine |=							\
      ((alpha_arg[n] >> RADEON_COMP_ARG_SHIFT)				\
       << RADEON_COMP_ARG_##arg##_SHIFT);				\
} while (0)


/* ================================================================
 * Texture unit state management
 */

static GLboolean radeonUpdateTextureEnv( GLcontext *ctx, int unit )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];
   const struct gl_texture_object *tObj = texUnit->_Current;
   const GLenum format = tObj->Image[tObj->BaseLevel]->Format;
   GLuint color_combine, alpha_combine;
   GLuint color_arg[3], alpha_arg[3];
   GLuint i, numColorArgs = 0, numAlphaArgs = 0;

   if ( RADEON_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %d ) format=%s\n", __FUNCTION__,
	       ctx, unit, _mesa_lookup_enum_by_nr( format ) );
   }

   /* Set the texture environment state.  Isn't this nice and clean?
    * The Radeon will automagically set the texture alpha to 0xff when
    * the texture format does not include an alpha component.  This
    * reduces the amount of special-casing we have to do, alpha-only
    * textures being a notable exception.
    */
   switch ( texUnit->EnvMode ) {
   case GL_REPLACE:
      switch ( format ) {
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
      case GL_INTENSITY:
	 color_combine = radeon_color_combine[unit][RADEON_REPLACE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_REPLACE];
	 break;
      case GL_ALPHA:
	 color_combine = radeon_color_combine[unit][RADEON_DISABLE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_REPLACE];
	 break;
      case GL_LUMINANCE:
      case GL_RGB:
	 color_combine = radeon_color_combine[unit][RADEON_REPLACE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_DISABLE];
         break;
      case GL_COLOR_INDEX:
      default:
	 return GL_FALSE;
      }
      break;

   case GL_MODULATE:
      switch ( format ) {
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
      case GL_INTENSITY:
	 color_combine = radeon_color_combine[unit][RADEON_MODULATE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_MODULATE];
	 break;
      case GL_ALPHA:
	 color_combine = radeon_color_combine[unit][RADEON_DISABLE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_MODULATE];
	 break;
      case GL_RGB:
      case GL_LUMINANCE:
	 color_combine = radeon_color_combine[unit][RADEON_MODULATE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_DISABLE];
	 break;
      case GL_COLOR_INDEX:
      default:
	 return GL_FALSE;
      }
      break;

   case GL_DECAL:
      switch ( format ) {
      case GL_RGBA:
      case GL_RGB:
	 color_combine = radeon_color_combine[unit][RADEON_DECAL];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_DISABLE];
	 break;
      case GL_ALPHA:
      case GL_LUMINANCE:
      case GL_LUMINANCE_ALPHA:
      case GL_INTENSITY:
	 color_combine = radeon_color_combine[unit][RADEON_DISABLE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_DISABLE];
	 break;
      case GL_COLOR_INDEX:
      default:
	 return GL_FALSE;
      }
      break;

   case GL_BLEND:
      switch ( format ) {
      case GL_RGBA:
      case GL_RGB:
      case GL_LUMINANCE:
      case GL_LUMINANCE_ALPHA:
	 color_combine = radeon_color_combine[unit][RADEON_BLEND];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_MODULATE];
	 break;
      case GL_ALPHA:
	 color_combine = radeon_color_combine[unit][RADEON_DISABLE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_MODULATE];
	 break;
      case GL_INTENSITY:
	 color_combine = radeon_color_combine[unit][RADEON_BLEND];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_BLEND];
	 break;
      case GL_COLOR_INDEX:
      default:
	 return GL_FALSE;
      }
      break;

   case GL_ADD:
      switch ( format ) {
      case GL_RGBA:
      case GL_RGB:
      case GL_LUMINANCE:
      case GL_LUMINANCE_ALPHA:
	 color_combine = radeon_color_combine[unit][RADEON_ADD];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_MODULATE];
	 break;
      case GL_ALPHA:
	 color_combine = radeon_color_combine[unit][RADEON_DISABLE];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_MODULATE];
	 break;
      case GL_INTENSITY:
	 color_combine = radeon_color_combine[unit][RADEON_ADD];
	 alpha_combine = radeon_alpha_combine[unit][RADEON_ADD];
	 break;
      case GL_COLOR_INDEX:
      default:
	 return GL_FALSE;
      }
      break;

   case GL_COMBINE_EXT:
      /* Don't cache these results.
       */
      rmesa->state.texture.unit[unit].format = 0;
      rmesa->state.texture.unit[unit].envMode = 0;

      /* Step 0:
       * Calculate how many arguments we need to process.
       */
      switch ( texUnit->CombineModeRGB ) {
      case GL_REPLACE:
	 numColorArgs = 1;
	 break;
      case GL_MODULATE:
      case GL_ADD:
      case GL_ADD_SIGNED:
      case GL_SUBTRACT:
      case GL_DOT3_RGB:
      case GL_DOT3_RGBA:
      case GL_DOT3_RGB_EXT:
      case GL_DOT3_RGBA_EXT:
	 numColorArgs = 2;
	 break;
      case GL_INTERPOLATE:
	 numColorArgs = 3;
	 break;
      default:
	 return GL_FALSE;
      }

      switch ( texUnit->CombineModeA ) {
      case GL_REPLACE:
	 numAlphaArgs = 1;
	 break;
      case GL_MODULATE:
      case GL_ADD:
      case GL_ADD_SIGNED:
      case GL_SUBTRACT:
	 numAlphaArgs = 2;
	 break;
      case GL_INTERPOLATE:
	 numAlphaArgs = 3;
	 break;
      default:
	 return GL_FALSE;
      }

      /* Step 1:
       * Extract the color and alpha combine function arguments.
       */
      for ( i = 0 ; i < numColorArgs ; i++ ) {
	 const GLuint op = texUnit->CombineOperandRGB[i] - GL_SRC_COLOR;
         ASSERT(op >= 0);
         ASSERT(op <= 3);
	 switch ( texUnit->CombineSourceRGB[i] ) {
	 case GL_TEXTURE:
	    color_arg[i] = radeon_texture_color[op][unit];
	    break;
	 case GL_CONSTANT:
	    color_arg[i] = radeon_tfactor_color[op];
	    break;
	 case GL_PRIMARY_COLOR:
	    color_arg[i] = radeon_primary_color[op];
	    break;
	 case GL_PREVIOUS:
	    color_arg[i] = radeon_previous_color[op];
	    break;
	 default:
	    return GL_FALSE;
	 }
      }

      for ( i = 0 ; i < numAlphaArgs ; i++ ) {
	 const GLuint op = texUnit->CombineOperandA[i] - GL_SRC_ALPHA;
         ASSERT(op >= 0);
         ASSERT(op <= 1);
	 switch ( texUnit->CombineSourceA[i] ) {
	 case GL_TEXTURE:
	    alpha_arg[i] = radeon_texture_alpha[op][unit];
	    break;
	 case GL_CONSTANT:
	    alpha_arg[i] = radeon_tfactor_alpha[op];
	    break;
	 case GL_PRIMARY_COLOR:
	    alpha_arg[i] = radeon_primary_alpha[op];
	    break;
	 case GL_PREVIOUS:
	    alpha_arg[i] = radeon_previous_alpha[op];
	    break;
	 default:
	    return GL_FALSE;
	 }
      }

      /* Step 2:
       * Build up the color and alpha combine functions.
       */
      switch ( texUnit->CombineModeRGB ) {
      case GL_REPLACE:
	 color_combine = (RADEON_COLOR_ARG_A_ZERO |
			  RADEON_COLOR_ARG_B_ZERO |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, C );
	 break;
      case GL_MODULATE:
	 color_combine = (RADEON_COLOR_ARG_C_ZERO |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, A );
	 RADEON_COLOR_ARG( 1, B );
	 break;
      case GL_ADD:
	 color_combine = (RADEON_COLOR_ARG_B_ZERO |
			  RADEON_COMP_ARG_B |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, A );
	 RADEON_COLOR_ARG( 1, C );
	 break;
      case GL_ADD_SIGNED:
	 color_combine = (RADEON_COLOR_ARG_B_ZERO |
			  RADEON_COMP_ARG_B |
			  RADEON_BLEND_CTL_ADDSIGNED |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, A );
	 RADEON_COLOR_ARG( 1, C );
	 break;
      case GL_SUBTRACT:
	 color_combine = (RADEON_COLOR_ARG_B_ZERO |
			  RADEON_COMP_ARG_B |
			  RADEON_BLEND_CTL_SUBTRACT |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, A );
	 RADEON_COLOR_ARG( 1, C );
	 break;
      case GL_INTERPOLATE:
	 color_combine = (RADEON_BLEND_CTL_BLEND |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, B );
	 RADEON_COLOR_ARG( 1, A );
	 RADEON_COLOR_ARG( 2, C );
	 break;

      case GL_DOT3_RGB:
      case GL_DOT3_RGBA:
	 if ( texUnit->CombineScaleShiftRGB 
	      != (RADEON_SCALE_1X >> RADEON_SCALE_SHIFT) )
	 {
	     return GL_FALSE;
	 }
	 /* FALLTHROUGH */

      case GL_DOT3_RGB_EXT:
      case GL_DOT3_RGBA_EXT:
	 color_combine = (RADEON_COLOR_ARG_C_ZERO |
			  RADEON_BLEND_CTL_DOT3 |
			  RADEON_CLAMP_TX);
	 RADEON_COLOR_ARG( 0, A );
	 RADEON_COLOR_ARG( 1, B );
	 break;
      default:
	 return GL_FALSE;
      }

      switch ( texUnit->CombineModeA ) {
      case GL_REPLACE:
	 alpha_combine = (RADEON_ALPHA_ARG_A_ZERO |
			  RADEON_ALPHA_ARG_B_ZERO |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	 RADEON_ALPHA_ARG( 0, C );
	 break;
      case GL_MODULATE:
	 alpha_combine = (RADEON_ALPHA_ARG_C_ZERO |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	 RADEON_ALPHA_ARG( 0, A );
	 RADEON_ALPHA_ARG( 1, B );
	 break;
      case GL_ADD:
	 alpha_combine = (RADEON_ALPHA_ARG_B_ZERO |
			  RADEON_COMP_ARG_B |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	 RADEON_ALPHA_ARG( 0, A );
	 RADEON_ALPHA_ARG( 1, C );
	 break;
      case GL_ADD_SIGNED:
	 alpha_combine = (RADEON_ALPHA_ARG_B_ZERO |
			  RADEON_COMP_ARG_B |
			  RADEON_BLEND_CTL_ADDSIGNED |
			  RADEON_CLAMP_TX);
	 RADEON_ALPHA_ARG( 0, A );
	 RADEON_ALPHA_ARG( 1, C );
	 break;
      case GL_SUBTRACT:
	 alpha_combine = (RADEON_COLOR_ARG_B_ZERO |
			  RADEON_COMP_ARG_B |
			  RADEON_BLEND_CTL_SUBTRACT |
			  RADEON_CLAMP_TX);
	 RADEON_ALPHA_ARG( 0, A );
	 RADEON_ALPHA_ARG( 1, C );
	 break;
      case GL_INTERPOLATE:
	 alpha_combine = (RADEON_BLEND_CTL_BLEND |
			  RADEON_CLAMP_TX);
	 RADEON_ALPHA_ARG( 0, B );
	 RADEON_ALPHA_ARG( 1, A );
	 RADEON_ALPHA_ARG( 2, C );
	 break;
      default:
	 return GL_FALSE;
      }

      if ( (texUnit->CombineModeRGB == GL_DOT3_RGB_EXT)
	   || (texUnit->CombineModeRGB == GL_DOT3_RGB_ARB) ) {
	 alpha_combine |= RADEON_DOT_ALPHA_DONT_REPLICATE;
      }

      /* Step 3:
       * Apply the scale factor.  The EXT version of the DOT3 extension does
       * not support the scale factor, but the ARB version (and the version in
       * OpenGL 1.3) does.  The catch is that the Radeon only supports a 1X
       * multiplier in hardware w/the ARB version.
       */
      if ( texUnit->CombineModeRGB != GL_DOT3_RGB_EXT &&
	   texUnit->CombineModeRGB != GL_DOT3_RGBA_EXT &&
	   texUnit->CombineModeRGB != GL_DOT3_RGB &&
	   texUnit->CombineModeRGB != GL_DOT3_RGBA ) {
	 color_combine |= (texUnit->CombineScaleShiftRGB << RADEON_SCALE_SHIFT);
	 alpha_combine |= (texUnit->CombineScaleShiftA << RADEON_SCALE_SHIFT);
      }
      else
      {
	 color_combine |= RADEON_SCALE_4X;
	 alpha_combine |= RADEON_SCALE_4X;
      }

      /* All done!
       */
      break;

   default:
      return GL_FALSE;
   }

   if ( rmesa->hw.tex[unit].cmd[TEX_PP_TXCBLEND] != color_combine ||
	rmesa->hw.tex[unit].cmd[TEX_PP_TXABLEND] != alpha_combine ) {
      RADEON_STATECHANGE( rmesa, tex[unit] );
      rmesa->hw.tex[unit].cmd[TEX_PP_TXCBLEND] = color_combine;
      rmesa->hw.tex[unit].cmd[TEX_PP_TXABLEND] = alpha_combine;
   }
    
   return GL_TRUE;
}

#define TEXOBJ_TXFILTER_MASK (RADEON_MAX_MIP_LEVEL_MASK |	\
			      RADEON_MIN_FILTER_MASK | 		\
			      RADEON_MAG_FILTER_MASK |		\
			      RADEON_MAX_ANISO_MASK |		\
			      RADEON_CLAMP_S_MASK | 		\
			      RADEON_CLAMP_T_MASK)

#define TEXOBJ_TXFORMAT_MASK (RADEON_TXFORMAT_WIDTH_MASK |	\
			      RADEON_TXFORMAT_HEIGHT_MASK |	\
			      RADEON_TXFORMAT_FORMAT_MASK |	\
			      RADEON_TXFORMAT_ALPHA_IN_MAP)


static void import_tex_obj_state( radeonContextPtr rmesa,
				  int unit,
				  radeonTexObjPtr texobj )
{
   GLuint *cmd = RADEON_DB_STATE( tex[unit] );

   cmd[TEX_PP_TXFILTER] &= ~TEXOBJ_TXFILTER_MASK;
   cmd[TEX_PP_TXFORMAT] &= ~TEXOBJ_TXFORMAT_MASK;
   cmd[TEX_PP_TXFILTER] |= texobj->pp_txfilter & TEXOBJ_TXFILTER_MASK;
   cmd[TEX_PP_TXFORMAT] |= texobj->pp_txformat & TEXOBJ_TXFORMAT_MASK;
   cmd[TEX_PP_TXOFFSET] = texobj->pp_txoffset;
   cmd[TEX_PP_BORDER_COLOR] = texobj->pp_border_color;
   texobj->dirty_state &= ~(1<<unit);

   RADEON_DB_STATECHANGE( rmesa, &rmesa->hw.tex[unit] );
}




static void set_texgen_matrix( radeonContextPtr rmesa, 
			       GLuint unit,
			       GLfloat *s_plane,
			       GLfloat *t_plane )
{
   static const GLfloat scale_identity[4] = { 1,1,1,1 };

   if (!TEST_EQ_4V( s_plane, scale_identity) ||
      !(TEST_EQ_4V( t_plane, scale_identity))) {
      rmesa->TexGenEnabled |= RADEON_TEXMAT_0_ENABLE<<unit;
      rmesa->TexGenMatrix[unit].m[0]  = s_plane[0];
      rmesa->TexGenMatrix[unit].m[4]  = s_plane[1];
      rmesa->TexGenMatrix[unit].m[8]  = s_plane[2];
      rmesa->TexGenMatrix[unit].m[12] = s_plane[3];

      rmesa->TexGenMatrix[unit].m[1]  = t_plane[0];
      rmesa->TexGenMatrix[unit].m[5]  = t_plane[1];
      rmesa->TexGenMatrix[unit].m[9]  = t_plane[2];
      rmesa->TexGenMatrix[unit].m[13] = t_plane[3];
      rmesa->NewGLState |= _NEW_TEXTURE_MATRIX;
   }
}

/* Ignoring the Q texcoord for now.
 *
 * Returns GL_FALSE if fallback required.  
 */
static GLboolean radeon_validate_texgen( GLcontext *ctx, GLuint unit )
{  
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];
   GLuint inputshift = RADEON_TEXGEN_0_INPUT_SHIFT + unit*4;
   GLuint tmp = rmesa->TexGenEnabled;

   rmesa->TexGenEnabled &= ~(RADEON_TEXGEN_TEXMAT_0_ENABLE<<unit);
   rmesa->TexGenEnabled &= ~(RADEON_TEXMAT_0_ENABLE<<unit);
   rmesa->TexGenEnabled &= ~(RADEON_TEXGEN_INPUT_MASK<<inputshift);
   rmesa->TexGenNeedNormals[unit] = 0;

   if (0)
   fprintf(stderr, "%s unit %d cleared texgenEnabled %x\n", __FUNCTION__,
	   unit, rmesa->TexGenEnabled);

   if ((texUnit->TexGenEnabled & (S_BIT|T_BIT)) == 0) {
      /* Disabled, no fallback:
       */
      rmesa->TexGenEnabled |= 
	 (RADEON_TEXGEN_INPUT_TEXCOORD_0+unit) << inputshift;
      return GL_TRUE;
   }
   else if (texUnit->TexGenEnabled & Q_BIT) {
      /* Very easy to do this, in fact would remove a fallback case
       * elsewhere, but I haven't done it yet...  Fallback: 
       */
      fprintf(stderr, "fallback Q_BIT\n");
      return GL_FALSE;
   }
   else if ((texUnit->TexGenEnabled & (S_BIT|T_BIT)) != (S_BIT|T_BIT) ||
	    texUnit->GenModeS != texUnit->GenModeT) {
      /* Mixed modes, fallback:
       */
/*        fprintf(stderr, "fallback mixed texgen\n"); */
      return GL_FALSE;
   }
   else
      rmesa->TexGenEnabled |= RADEON_TEXGEN_TEXMAT_0_ENABLE << unit;

   switch (texUnit->GenModeS) {
   case GL_OBJECT_LINEAR:
      rmesa->TexGenEnabled |= RADEON_TEXGEN_INPUT_OBJ << inputshift;
      set_texgen_matrix( rmesa, unit, 
			 texUnit->ObjectPlaneS,
			 texUnit->ObjectPlaneT);
      break;

   case GL_EYE_LINEAR:
      rmesa->TexGenEnabled |= RADEON_TEXGEN_INPUT_EYE << inputshift;
      set_texgen_matrix( rmesa, unit, 
			 texUnit->EyePlaneS,
			 texUnit->EyePlaneT);
      break;

   case GL_REFLECTION_MAP_NV:
      rmesa->TexGenNeedNormals[unit] = GL_TRUE;
      rmesa->TexGenEnabled |= RADEON_TEXGEN_INPUT_EYE_REFLECT<<inputshift;
      break;

   case GL_NORMAL_MAP_NV:
      rmesa->TexGenNeedNormals[unit] = GL_TRUE;
      rmesa->TexGenEnabled |= RADEON_TEXGEN_INPUT_EYE_NORMAL<<inputshift;
      break;

   case GL_SPHERE_MAP:
   default:
      /* Unsupported mode, fallback:
       */
      /*  fprintf(stderr, "fallback unsupported texgen\n"); */
      return GL_FALSE;
   }

   if (tmp != rmesa->TexGenEnabled) {
      rmesa->NewGLState |= _NEW_TEXTURE_MATRIX;
   }

/*     fprintf(stderr, "%s unit %d texgenEnabled %x\n", __FUNCTION__, */
/*  	   unit, rmesa->TexGenEnabled); */
   return GL_TRUE;
}




static GLboolean radeonUpdateTextureUnit( GLcontext *ctx, int unit )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];

/*     fprintf(stderr, "%s\n", __FUNCTION__); */

   if ( texUnit->_ReallyEnabled & (TEXTURE0_1D|TEXTURE0_2D) ) {
      struct gl_texture_object *tObj = texUnit->_Current;
      radeonTexObjPtr t = (radeonTexObjPtr) tObj->DriverData;
      GLenum format;

      /* Fallback if there's a texture border */
      if ( tObj->Image[tObj->BaseLevel]->Border > 0 )
         return GL_FALSE;

      /* Upload teximages (not pipelined)
       */
      if ( t->dirty_images ) {
	 RADEON_FIREVERTICES( rmesa );
	 radeonSetTexImages( rmesa, tObj );
	 /* Fallback if we can't upload:
	  */
	 if ( !t->memBlock ) 
	    return GL_FALSE;
      }

      /* Update state if this is a different texture object to last
       * time.
       */
      if ( rmesa->state.texture.unit[unit].texobj != t ) {
	 rmesa->state.texture.unit[unit].texobj = t;
	 t->dirty_state |= 1<<unit;
	 radeonUpdateTexLRU( rmesa, t ); /* XXX: should be locked! */
      }


      /* Newly enabled?
       */
      if ( !(rmesa->hw.ctx.cmd[CTX_PP_CNTL] & (RADEON_TEX_0_ENABLE<<unit))) {
	 RADEON_STATECHANGE( rmesa, ctx );
	 rmesa->hw.ctx.cmd[CTX_PP_CNTL] |= 
	    (RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE) << unit;

	 RADEON_STATECHANGE( rmesa, tcl );

	 if (unit == 0) 
	    rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] |= RADEON_TCL_VTX_ST0;
	 else 
	    rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] |= RADEON_TCL_VTX_ST1;

	 rmesa->recheck_texgen[unit] = GL_TRUE;
      }

      if (t->dirty_state & (1<<unit)) {
	 import_tex_obj_state( rmesa, unit, t );
      }
      
      if (rmesa->recheck_texgen[unit]) {
	 GLboolean fallback = !radeon_validate_texgen( ctx, unit );
	 TCL_FALLBACK( ctx, (RADEON_TCL_FALLBACK_TEXGEN_0<<unit), fallback);
	 rmesa->recheck_texgen[unit] = 0;
	 rmesa->NewGLState |= _NEW_TEXTURE_MATRIX;
      }

      format = tObj->Image[tObj->BaseLevel]->Format;
      if ( rmesa->state.texture.unit[unit].format != format ||
	   rmesa->state.texture.unit[unit].envMode != texUnit->EnvMode ) {
	 rmesa->state.texture.unit[unit].format = format;
	 rmesa->state.texture.unit[unit].envMode = texUnit->EnvMode;
	 if ( ! radeonUpdateTextureEnv( ctx, unit ) ) {
	    return GL_FALSE;
	 }
      }
   }
   else if ( texUnit->_ReallyEnabled ) {
      /* 3d textures, etc:
       */
      return GL_FALSE;
   }
   else if (rmesa->hw.ctx.cmd[CTX_PP_CNTL] & (RADEON_TEX_0_ENABLE<<unit)) {
      /* Texture unit disabled */
      rmesa->state.texture.unit[unit].texobj = 0;
      RADEON_STATECHANGE( rmesa, ctx );
      rmesa->hw.ctx.cmd[CTX_PP_CNTL] &= 
	 ~((RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE) << unit);

      RADEON_STATECHANGE( rmesa, tcl );
      switch (unit) {
      case 0:
	 rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] &= ~(RADEON_TCL_VTX_ST0 |
						   RADEON_TCL_VTX_Q0);
	    break;
      case 1:
	 rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] &= ~(RADEON_TCL_VTX_ST1 |
						   RADEON_TCL_VTX_Q1);
	 break;
      default:
	 break;
      }


      if (rmesa->TclFallback & (RADEON_TCL_FALLBACK_TEXGEN_0<<unit)) {
	 TCL_FALLBACK( ctx, (RADEON_TCL_FALLBACK_TEXGEN_0<<unit), GL_FALSE);
	 rmesa->recheck_texgen[unit] = GL_TRUE;
      }



      {
	 GLuint inputshift = RADEON_TEXGEN_0_INPUT_SHIFT + unit*4;
	 GLuint tmp = rmesa->TexGenEnabled;

	 rmesa->TexGenEnabled &= ~(RADEON_TEXGEN_TEXMAT_0_ENABLE<<unit);
	 rmesa->TexGenEnabled &= ~(RADEON_TEXMAT_0_ENABLE<<unit);
	 rmesa->TexGenEnabled &= ~(RADEON_TEXGEN_INPUT_MASK<<inputshift);
	 rmesa->TexGenNeedNormals[unit] = 0;
	 rmesa->TexGenEnabled |= 
	    (RADEON_TEXGEN_INPUT_TEXCOORD_0+unit) << inputshift;

	 if (tmp != rmesa->TexGenEnabled) {
	    rmesa->recheck_texgen[unit] = GL_TRUE;
	    rmesa->NewGLState |= _NEW_TEXTURE_MATRIX;
	 }
      }
   }

   return GL_TRUE;
}

void radeonUpdateTextureState( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLboolean ok;

   ok = (radeonUpdateTextureUnit( ctx, 0 ) &&
	 radeonUpdateTextureUnit( ctx, 1 ));

   FALLBACK( rmesa, RADEON_FALLBACK_TEXTURE, !ok );

   if (rmesa->TclFallback)
      radeonChooseVertexState( ctx );
}
