/**************************************************************************

Copyright 2001 2d3d Inc., Delray Beach, FL

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

/* $XFree86: xc/lib/GL/mesa/src/drv/i830/i830_texstate.c,v 1.3 2002/12/10 01:26:53 dawes Exp $ */

/*
 * Author:
 *   Jeff Hartmann <jhartmann@2d3d.com>
 *
 * Heavily based on the I810 driver, which was written by:
 *   Keith Whitwell <keithw@tungstengraphics.com>
 */
#include <stdlib.h>
#include <stdio.h>

#include "glheader.h"
#include "macros.h"
#include "mtypes.h"
#include "simple_list.h"
#include "enums.h"
#include "texformat.h"
#include "texstore.h"
#include "texutil.h"

#include "mm.h"

#include "i830_screen.h"
#include "i830_dri.h"

#include "i830_context.h"
#include "i830_tex.h"
#include "i830_state.h"
#include "i830_ioctl.h"

#define I830_TEX_UNIT_ENABLED(unit)		(1<<unit)

static void i830SetTexImages( i830ContextPtr imesa, 
			      struct gl_texture_object *tObj )
{
   GLuint height, width, pitch, i, textureFormat;
   i830TextureObjectPtr t = (i830TextureObjectPtr) tObj->DriverData;
   const struct gl_texture_image *baseImage = tObj->Image[tObj->BaseLevel];
   GLint firstLevel, lastLevel, numLevels;
   GLint log2Width, log2Height;

   switch( baseImage->TexFormat->MesaFormat ) {
   case MESA_FORMAT_L8:
      t->texelBytes = 1;
      textureFormat = MAPSURF_8BIT | MT_8BIT_L8;
      break;

   case MESA_FORMAT_I8:
      t->texelBytes = 1;
      textureFormat = MAPSURF_8BIT | MT_8BIT_I8;
      break;

   case MESA_FORMAT_AL88:
      t->texelBytes = 2;
      textureFormat = MAPSURF_16BIT | MT_16BIT_AY88;
      break;

   case MESA_FORMAT_RGB565:
      t->texelBytes = 2;
      textureFormat = MAPSURF_16BIT | MT_16BIT_RGB565;
      break;

   case MESA_FORMAT_ARGB1555:
      t->texelBytes = 2;
      textureFormat = MAPSURF_16BIT | MT_16BIT_ARGB1555;
      break;

   case MESA_FORMAT_ARGB4444:
      t->texelBytes = 2;
      textureFormat = MAPSURF_16BIT | MT_16BIT_ARGB4444;
      break;

   case MESA_FORMAT_ARGB8888:
      t->texelBytes = 4;
      textureFormat = MAPSURF_32BIT | MT_32BIT_ARGB8888;
      break;

   default:
      fprintf(stderr, "%s: bad image format\n", __FUNCTION__);
      free( t );
      return;
   }

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

   if (0) fprintf(stderr,
		  "%s : "
		  "NumLevels : %d\n"
		  "firstLevel : %d\n"
		  "lastLevel : %d\n",
		  __FUNCTION__,
		  numLevels,
		  firstLevel,
		  lastLevel);

   log2Width = tObj->Image[firstLevel]->WidthLog2;
   log2Height = tObj->Image[firstLevel]->HeightLog2;

   /* Figure out the amount of memory required to hold all the mipmap
    * levels.  Choose the smallest pitch to accomodate the largest
    * mipmap:
    */
   width = tObj->Image[firstLevel]->Width * t->texelBytes;

   pitch = width;

   /* All images must be loaded at this pitch.  Count the number of
    * lines required:
    */
   for ( height = i = 0 ; i < numLevels ; i++ ) {
      t->image[i].image = tObj->Image[firstLevel + i];
      if (!t->image[i].image) {
	 break;
      }
      t->image[i].offset = height * pitch;
      t->image[i].internalFormat = baseImage->Format;
      height += t->image[i].image->Height;
   }

   t->Pitch = pitch;
   t->totalSize = height*pitch;
   t->max_level = i-1;

   t->Setup[I830_TEXREG_TM0S1] = 
      ((((1 << log2Height) - 1) << TM0S1_HEIGHT_SHIFT) |
       (((1 << log2Width) - 1) << TM0S1_WIDTH_SHIFT) |
       textureFormat);
   t->Setup[I830_TEXREG_TM0S2] = 
      ((((pitch / 4) - 1) << TM0S2_PITCH_SHIFT));
   
   t->Setup[I830_TEXREG_TM0S3] &= ~TM0S3_MAX_MIP_MASK;
   t->Setup[I830_TEXREG_TM0S3] &= ~TM0S3_MIN_MIP_MASK;
   t->Setup[I830_TEXREG_TM0S3] |= 
	 ((numLevels - 1)*4) << TM0S3_MIN_MIP_SHIFT;

   t->dirty = I830_UPLOAD_TEX0 | I830_UPLOAD_TEX1;
   LOCK_HARDWARE( imesa );
   i830UploadTexImages( imesa, t );
   UNLOCK_HARDWARE( imesa );
}

/* ================================================================
 * Texture combine functions
 */
static __inline__ GLuint GetTexelOp(GLint unit)
{
   switch(unit) {
    case 0: return TEXBLENDARG_TEXEL0;
    case 1: return TEXBLENDARG_TEXEL1;
    case 2: return TEXBLENDARG_TEXEL2;
    case 3: return TEXBLENDARG_TEXEL3;
    default: return TEXBLENDARG_TEXEL0;
   }
}

static void i830SetBlend_GL1_2(i830ContextPtr imesa, int curTex, 
			       GLenum envMode, GLenum format)
{
   GLuint texel_op = GetTexelOp(curTex);

   if(I830_DEBUG&DEBUG_TEXTURE)
     fprintf(stderr, "%s %s %s unit (%d) texel_op(0x%x)\n",
	     __FUNCTION__,
	     _mesa_lookup_enum_by_nr(format),
	     _mesa_lookup_enum_by_nr(envMode),
	     curTex,
	     texel_op);

   switch(envMode) {
   case GL_REPLACE:
      switch(format) {
      case GL_ALPHA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      case GL_LUMINANCE:
      case GL_RGB:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;

      case GL_INTENSITY:
      case GL_LUMINANCE_ALPHA:
      case GL_RGBA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      default:
	 /* Always set to passthru if something is funny */
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      }
      break;

   case GL_MODULATE:
      switch(format) {
      case GL_ALPHA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 5;
	 break;

      case GL_LUMINANCE:
      case GL_RGB:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 5;
	 break;

      case GL_INTENSITY:
      case GL_LUMINANCE_ALPHA:
      case GL_RGBA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 6;
	 break;
      default:
	 /* Always set to passthru if something is funny */
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      }
      break;

   case GL_DECAL:
      switch(format) {
      case GL_RGB:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;

      case GL_RGBA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_BLEND);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG0 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_REPLICATE_ALPHA |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 6;
	 break;
      default:
	 /* Always set to passthru if something is funny */
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      }
      break;

   case GL_BLEND:
      switch(format) {
      case GL_ALPHA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 5;
	 break;

      case GL_LUMINANCE:
      case GL_RGB:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_BLEND);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG0 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_FACTOR_N);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 6;
	 break;

      case GL_INTENSITY:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_BLEND);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_BLEND);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG0 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_FACTOR_N);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG0 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][6] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_FACTOR_N);
	 imesa->TexBlend[curTex][7] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 8;
	 break;

      case GL_LUMINANCE_ALPHA:
      case GL_RGBA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_BLEND);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG0 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_FACTOR_N);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][6] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 7;
	 break;
      default:
	 /* Always set to passthru if something is funny */
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      }
      break;

   case GL_ADD:
      switch(format) {
      case GL_ALPHA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 5;
	 break;
      case GL_LUMINANCE:
      case GL_RGB:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ADD);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 5;
	 break;

      case GL_INTENSITY:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ADD);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ADD);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 6;
	 break;

      case GL_LUMINANCE_ALPHA:
      case GL_RGBA:
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ADD);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_MODULATE);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][4] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       texel_op);
	 imesa->TexBlend[curTex][5] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG2 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 6;
	 break;
      default:
	 /* Always set to passthru if something is funny */
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
      }
      break;
   default:
	 /* Always set to passthru if something is funny */
	 imesa->TexBlend[curTex][0] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_COLOR |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       DISABLE_TEX_CNTRL_STAGE |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][1] = (STATE3D_MAP_BLEND_OP_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       ENABLE_TEXOUTPUT_WRT_SEL |
				       TEXOP_OUTPUT_CURRENT |
				       TEXOP_SCALE_1X |
				       TEXOP_MODIFY_PARMS |
				       TEXBLENDOP_ARG1);
	 imesa->TexBlend[curTex][2] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_COLOR |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlend[curTex][3] = (STATE3D_MAP_BLEND_ARG_CMD(curTex) |
				       TEXPIPE_ALPHA |
				       TEXBLEND_ARG1 |
				       TEXBLENDARG_MODIFY_PARMS |
				       TEXBLENDARG_CURRENT);
	 imesa->TexBlendColorPipeNum[curTex] = 0;
	 imesa->TexBlendWordsUsed[curTex] = 4;
	 break;
   }

   if (I830_DEBUG&DEBUG_TEXTURE)
      fprintf(stderr, "%s\n", __FUNCTION__);
}

static void i830SetTexEnvCombine(i830ContextPtr imesa,
				 const struct gl_texture_unit *texUnit, 
				 GLint unit)
{
   GLuint blendop;
   GLuint ablendop;
   GLuint args_RGB[3];
   GLuint args_A[3];
   GLuint texel_op = GetTexelOp(unit);
   int i;

   if(I830_DEBUG&DEBUG_TEXTURE)
      fprintf(stderr, "%s\n", __FUNCTION__);

   switch(texUnit->CombineModeRGB) {
   case GL_REPLACE: 
      blendop = TEXBLENDOP_ARG1;
      break;
   case GL_MODULATE: 
      blendop = TEXBLENDOP_MODULATE;
      break;
   case GL_ADD: 
      blendop = TEXBLENDOP_ADD;
      break;
   case GL_ADD_SIGNED_ARB:
      blendop = TEXBLENDOP_ADDSIGNED; 
      break;
   case GL_INTERPOLATE_ARB:
      blendop = TEXBLENDOP_BLEND; 
      break;
   case GL_SUBTRACT_ARB: 
      blendop = TEXBLENDOP_SUBTRACT;
      break;
   default: 
      return;
   }

   switch(texUnit->CombineScaleShiftRGB) {
   case 0: 
      blendop |= TEXOP_SCALE_1X;
      break;
   case 1: 
      blendop |= TEXOP_SCALE_2X;
      break;
   case 2: 
      blendop |= TEXOP_SCALE_4X;
      break;
   default: 
      return;
   }

   switch(texUnit->CombineModeA) {
   case GL_REPLACE: 
      ablendop = TEXBLENDOP_ARG1;
      break;
   case GL_MODULATE: 
      ablendop = TEXBLENDOP_MODULATE;
      break;
   case GL_ADD: 
      ablendop = TEXBLENDOP_ADD;
      break;
   case GL_ADD_SIGNED_ARB:
      ablendop = TEXBLENDOP_ADDSIGNED; 
      break;
   case GL_INTERPOLATE_ARB:
      ablendop = TEXBLENDOP_BLEND; 
      break;
   case GL_SUBTRACT_ARB: 
      ablendop = TEXBLENDOP_SUBTRACT;
      break;
   default:
      return;
   }

   switch(texUnit->CombineScaleShiftA) {
   case 0: 
      ablendop |= TEXOP_SCALE_1X;
      break;
   case 1: 
      ablendop |= TEXOP_SCALE_2X;
      break;
   case 2: 
      ablendop |= TEXOP_SCALE_4X;
      break;
   default: 
      return;
   }

   /* Handle RGB args */
   for(i = 0; i < 3; i++) {
      switch(texUnit->CombineSourceRGB[i]) {
      case GL_TEXTURE: 
	 args_RGB[i] = texel_op;
	 break;
      case GL_CONSTANT_ARB:
	 args_RGB[i] = TEXBLENDARG_FACTOR_N; 
	 break;
      case GL_PRIMARY_COLOR_ARB:
	 args_RGB[i] = TEXBLENDARG_DIFFUSE;
	 break;
      case GL_PREVIOUS_ARB:
	 args_RGB[i] = TEXBLENDARG_CURRENT; 
	 break;
      default: 
	 return;
	 
      }

      switch(texUnit->CombineOperandRGB[i]) {
      case GL_SRC_COLOR: 
	 args_RGB[i] |= 0;
	 break;
      case GL_ONE_MINUS_SRC_COLOR: 
	 args_RGB[i] |= TEXBLENDARG_INV_ARG;
	 break;
      case GL_SRC_ALPHA: 
	 args_RGB[i] |= TEXBLENDARG_REPLICATE_ALPHA;
	 break;
      case GL_ONE_MINUS_SRC_ALPHA: 
	 args_RGB[i] |= (TEXBLENDARG_REPLICATE_ALPHA | 
			 TEXBLENDARG_INV_ARG);
	 break;
      default: 
	 return;
      }
   }

   /* Handle A args */
   for(i = 0; i < 3; i++) {
      switch(texUnit->CombineSourceA[i]) {
      case GL_TEXTURE: 
	 args_A[i] = texel_op;
	 break;
      case GL_CONSTANT_ARB:
	 args_A[i] = TEXBLENDARG_FACTOR_N; 
	 break;
      case GL_PRIMARY_COLOR_ARB:
	 args_A[i] = TEXBLENDARG_DIFFUSE; 
	 break;
      case GL_PREVIOUS_ARB:
	 args_A[i] = TEXBLENDARG_CURRENT; 
	 break;
      default: 
	 return;
	 
      }

      switch(texUnit->CombineOperandA[i]) {
      case GL_SRC_ALPHA: 
	 args_A[i] |= 0;
	 break;
      case GL_ONE_MINUS_SRC_ALPHA: 
	 args_A[i] |= TEXBLENDARG_INV_ARG;
	 break;
      default: 
	 return;
      }
   }

   /* Native Arg1 == Arg0 in GL_EXT_texture_env_combine spec */
   /* Native Arg2 == Arg1 in GL_EXT_texture_env_combine spec */
   /* Native Arg0 == Arg2 in GL_EXT_texture_env_combine spec */

   /* When we render we need to figure out which is the last really enabled
    * tex unit, and put last stage on it
    */

   imesa->TexBlendColorPipeNum[unit] = 0;

   /* Build color pipeline */

   imesa->TexBlend[unit][0] = (STATE3D_MAP_BLEND_OP_CMD(unit) |
			       TEXPIPE_COLOR |
			       ENABLE_TEXOUTPUT_WRT_SEL |
			       TEXOP_OUTPUT_CURRENT |
			       DISABLE_TEX_CNTRL_STAGE |
			       TEXOP_MODIFY_PARMS |
			       blendop);
   imesa->TexBlend[unit][1] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
			       TEXPIPE_COLOR |
			       TEXBLEND_ARG1 |
			       TEXBLENDARG_MODIFY_PARMS |
			       args_RGB[0]);
   imesa->TexBlend[unit][2] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
			       TEXPIPE_COLOR |
			       TEXBLEND_ARG2 |
			       TEXBLENDARG_MODIFY_PARMS |
			       args_RGB[1]);
   imesa->TexBlend[unit][3] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
			       TEXPIPE_COLOR |
			       TEXBLEND_ARG0 |
			       TEXBLENDARG_MODIFY_PARMS |
			       args_RGB[2]);

   /* Build Alpha pipeline */
   imesa->TexBlend[unit][4] = (STATE3D_MAP_BLEND_OP_CMD(unit) |
			       TEXPIPE_ALPHA |
			       ENABLE_TEXOUTPUT_WRT_SEL |
			       TEXOP_OUTPUT_CURRENT |
			       TEXOP_MODIFY_PARMS |
			       ablendop);
   imesa->TexBlend[unit][5] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
			       TEXPIPE_ALPHA |
			       TEXBLEND_ARG1 |
			       TEXBLENDARG_MODIFY_PARMS |
			       args_A[0]);
   imesa->TexBlend[unit][6] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
			       TEXPIPE_ALPHA |
			       TEXBLEND_ARG2 |
			       TEXBLENDARG_MODIFY_PARMS |
			       args_A[1]);
   imesa->TexBlend[unit][7] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
			       TEXPIPE_ALPHA |
			       TEXBLEND_ARG0 |
			       TEXBLENDARG_MODIFY_PARMS |
			       args_A[2]);

   {
      GLubyte r, g, b, a;
      GLfloat *fc = texUnit->EnvColor;

      FLOAT_COLOR_TO_UBYTE_COLOR(r, fc[RCOMP]);
      FLOAT_COLOR_TO_UBYTE_COLOR(g, fc[GCOMP]);
      FLOAT_COLOR_TO_UBYTE_COLOR(b, fc[BCOMP]);
      FLOAT_COLOR_TO_UBYTE_COLOR(a, fc[ACOMP]);

      imesa->TexBlend[unit][8] = STATE3D_COLOR_FACTOR_CMD(unit);
      imesa->TexBlend[unit][9] =  ((a << 24) |
				   (r << 16) |
				   (g << 8) |
				   b);
   }
   imesa->TexBlendWordsUsed[unit] = 10;
}




static void i830UpdateTexEnv( GLcontext *ctx, GLuint unit )
{
   i830ContextPtr imesa = I830_CONTEXT(ctx);
   const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];
   const struct gl_texture_object *tObj = texUnit->_Current;
   i830TextureObjectPtr t = (i830TextureObjectPtr)tObj->DriverData;
   GLuint col;

   imesa->TexBlendWordsUsed[unit] = 0;

   if (0) fprintf(stderr, "i830UpdateTexEnv called : %s\n",
		  _mesa_lookup_enum_by_nr(texUnit->EnvMode));

   if(texUnit->EnvMode == GL_COMBINE_ARB) {
      i830SetTexEnvCombine(imesa,
			   texUnit,
			   unit);
   } else {
      i830SetBlend_GL1_2(imesa,
			 unit,
			 texUnit->EnvMode,
			 t->image[0].internalFormat);

      /* add blend color */
      {
	 GLubyte r, g, b, a;
	 GLfloat *fc = texUnit->EnvColor;

	 FLOAT_COLOR_TO_UBYTE_COLOR(r, fc[RCOMP]);
	 FLOAT_COLOR_TO_UBYTE_COLOR(g, fc[GCOMP]);
	 FLOAT_COLOR_TO_UBYTE_COLOR(b, fc[BCOMP]);
	 FLOAT_COLOR_TO_UBYTE_COLOR(a, fc[ACOMP]);

	 col = ((a << 24) |
		(r << 16) |
		(g << 8) |
		b);
      }       

      {
	 int i;

	 i = imesa->TexBlendWordsUsed[unit];
	 imesa->TexBlend[unit][i++] = STATE3D_COLOR_FACTOR_CMD(unit);	  
	 imesa->TexBlend[unit][i++] = col;

	 imesa->TexBlendWordsUsed[unit] = i;
      }
   }

   I830_STATECHANGE( imesa, I830_UPLOAD_TEXBLEND_N(unit) );
}


static void i830TexSetUnit( i830TextureObjectPtr t, GLuint unit )
{
      if(I830_DEBUG&DEBUG_TEXTURE)
           fprintf(stderr, "%s unit(%d)\n", __FUNCTION__, unit);
   
      t->Setup[I830_TEXREG_TM0LI] = (STATE3D_LOAD_STATE_IMMEDIATE_2 | 
				     (LOAD_TEXTURE_MAP0 << unit) | 4);

      I830_SET_FIELD(t->Setup[I830_TEXREG_MCS], MAP_UNIT_MASK, MAP_UNIT(unit));
   
      t->current_unit = unit;
}

#define TEXCOORDTYPE_MASK	(~((1<<13)|(1<<12)|(1<<11)))

static void i830UpdateTexUnit( GLcontext *ctx, GLuint unit )
{
   i830ContextPtr imesa = I830_CONTEXT(ctx);
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];

   imesa->TexEnabledMask &= ~(I830_TEX_UNIT_ENABLED(unit));

   if (texUnit->_ReallyEnabled == TEXTURE0_2D)
   {
      struct gl_texture_object *tObj = texUnit->_Current;
      i830TextureObjectPtr t = (i830TextureObjectPtr)tObj->DriverData;
      GLuint mcs = t->Setup[I830_TEXREG_MCS] & TEXCOORDTYPE_MASK;

      /* Handle projective texturing */
      if (imesa->vertex_format & (1<<31)) {
	 mcs |= TEXCOORDTYPE_HOMOGENEOUS;
      } else {
	 mcs |= TEXCOORDTYPE_CARTESIAN;
      }

      /* Fallback if there's a texture border */
      if ( tObj->Image[tObj->BaseLevel]->Border > 0 ) {
	 FALLBACK( imesa, I830_FALLBACK_TEXTURE, GL_TRUE );
	 return;
      }

      /* Upload teximages (not pipelined)
       */
      if (t->dirty_images) {
/* 	 I830_FIREVERTICES(imesa); */
	 i830SetTexImages( imesa, tObj );
	 if (!t->MemBlock) {
	    FALLBACK( imesa, I830_FALLBACK_TEXTURE, GL_TRUE );
	    return;
	 }
      }

      /* Update state if this is a different texture object to last
       * time.
       */
      if (imesa->CurrentTexObj[unit] != t || 
	  mcs != t->Setup[I830_TEXREG_MCS]) {
	 I830_STATECHANGE(imesa, (I830_UPLOAD_TEX0<<unit));
	 t->Setup[I830_TEXREG_MCS] = mcs;
	 imesa->CurrentTexObj[unit] = t;
	 i830TexSetUnit(t, unit);
      }

      /* Update texture environment if texture object image format or 
       * texture environment state has changed. 
       *
       * KW: doesn't work -- change from tex0 only to tex0+tex1 gets
       * missed (need to update last stage flag?).  Call
       * i830UpdateTexEnv always.
       */
      if (tObj->Image[tObj->BaseLevel]->Format !=
	  imesa->TexEnvImageFmt[unit]) {
	 imesa->TexEnvImageFmt[unit] = tObj->Image[tObj->BaseLevel]->Format;
      }
      i830UpdateTexEnv( ctx, unit );
      imesa->TexEnabledMask |= I830_TEX_UNIT_ENABLED(unit);
   }
   else if (texUnit->_ReallyEnabled) {
      FALLBACK( imesa, I830_FALLBACK_TEXTURE, GL_TRUE );
   }
   else if (unit == 0) {
      /* This is happening too often.  I need to conditionally send diffuse
       * state to the card.  Perhaps a diffuse dirty flag of some kind.
       * Will need to change this logic if more than 2 texture units are
       * used.  We need to only do this up to the last unit enabled, or unit
       * one if nothing is enabled.
       */
      imesa->CurrentTexObj[unit] = 0;
      imesa->TexEnvImageFmt[unit] = 0;
      imesa->dirty &= ~(I830_UPLOAD_TEX_N(unit));

      imesa->TexBlend[unit][0] = (STATE3D_MAP_BLEND_OP_CMD(unit) |
				    TEXPIPE_COLOR |
				    ENABLE_TEXOUTPUT_WRT_SEL |
				    TEXOP_OUTPUT_CURRENT |
				    DISABLE_TEX_CNTRL_STAGE |
				    TEXOP_SCALE_1X |
				    TEXOP_MODIFY_PARMS |
				    TEXBLENDOP_ARG1);
      imesa->TexBlend[unit][1] = (STATE3D_MAP_BLEND_OP_CMD(unit) |
				    TEXPIPE_ALPHA |
				    ENABLE_TEXOUTPUT_WRT_SEL |
				    TEXOP_OUTPUT_CURRENT |
				    TEXOP_SCALE_1X |
				    TEXOP_MODIFY_PARMS |
				    TEXBLENDOP_ARG1);
      imesa->TexBlend[unit][2] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
				    TEXPIPE_COLOR |
				    TEXBLEND_ARG1 |
				    TEXBLENDARG_MODIFY_PARMS |
				    TEXBLENDARG_CURRENT);
      imesa->TexBlend[unit][3] = (STATE3D_MAP_BLEND_ARG_CMD(unit) |
				    TEXPIPE_ALPHA |
				    TEXBLEND_ARG1 |
				    TEXBLENDARG_MODIFY_PARMS |
				    TEXBLENDARG_CURRENT);
      imesa->TexBlendColorPipeNum[unit] = 0;
      imesa->TexBlendWordsUsed[unit] = 4;
      I830_STATECHANGE(imesa, (I830_UPLOAD_TEXBLEND_N(unit)));
   }
}

/* Called from vb code to update projective texturing properly */
void i830UpdateTexUnitProj( GLcontext *ctx, GLuint unit, GLboolean state )
{
   i830ContextPtr imesa = I830_CONTEXT(ctx);
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];

   if (texUnit->_ReallyEnabled == TEXTURE0_2D)
   {
      struct gl_texture_object *tObj = texUnit->_Current;
      i830TextureObjectPtr t = (i830TextureObjectPtr)tObj->DriverData;
      GLuint mcs = t->Setup[I830_TEXREG_MCS] & TEXCOORDTYPE_MASK;

      /* Handle projective texturing */
      if (state) {
	 mcs |= TEXCOORDTYPE_HOMOGENEOUS;
      } else {
	 mcs |= TEXCOORDTYPE_CARTESIAN;
      }

      if (mcs != t->Setup[I830_TEXREG_MCS]) {
	 I830_STATECHANGE(imesa, (I830_UPLOAD_TEX0<<unit));
	 t->Setup[I830_TEXREG_MCS] = mcs;
      }
   }
}

/* Only deal with unit 0 and 1 for right now */
void i830UpdateTextureState( GLcontext *ctx )
{
   i830ContextPtr imesa = I830_CONTEXT(ctx);
   int pipe_num = 0;

   if (0) fprintf(stderr, "%s\n", __FUNCTION__);
   FALLBACK( imesa, I830_FALLBACK_TEXTURE, GL_FALSE );

   i830UpdateTexUnit( ctx, 0 );
   i830UpdateTexUnit( ctx, 1 );
#if 0
   i830UpdateTexUnit( ctx, 2 );
   i830UpdateTexUnit( ctx, 3 );
#endif

   /* Make sure last stage is set correctly */
#if 0
   if(imesa->TexEnabledMask & I830_TEX_UNIT_ENABLED(3)) {
      pipe_num = imesa->TexBlendColorPipeNum[3];
      imesa->TexBlend[3][pipe_num] |= TEXOP_LAST_STAGE;
   } else if(imesa->TexEnabledMask & I830_TEX_UNIT_ENABLED(2)) {
      pipe_num = imesa->TexBlendColorPipeNum[2];
      imesa->TexBlend[2][pipe_num] |= TEXOP_LAST_STAGE;
   } else
#endif
   if(imesa->TexEnabledMask & I830_TEX_UNIT_ENABLED(1)) {
      pipe_num = imesa->TexBlendColorPipeNum[1];
      imesa->TexBlend[1][pipe_num] |= TEXOP_LAST_STAGE;
      if (0) fprintf(stderr, "Unit One is last enabled\n");
   } else {
      pipe_num = imesa->TexBlendColorPipeNum[0];
      imesa->TexBlend[0][pipe_num] |= TEXOP_LAST_STAGE;
      if (0) fprintf(stderr, "Unit Zero is last enabled\n");
   }
}



