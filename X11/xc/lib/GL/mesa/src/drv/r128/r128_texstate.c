/* $XFree86: xc/lib/GL/mesa/src/drv/r128/r128_texstate.c,v 1.1 2002/02/22 21:44:58 dawes Exp $ */
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
 *   Brian Paul <brianp@valinux.com>
 */

#include "r128_context.h"
#include "r128_state.h"
#include "r128_ioctl.h"
#include "r128_vb.h"
#include "r128_tris.h"
#include "r128_tex.h"

#include "context.h"
#include "macros.h"
#include "mem.h"
#include "texformat.h"


static void r128SetTexImages( r128ContextPtr rmesa,
                              const struct gl_texture_object *tObj )
{
   r128TexObjPtr t = (r128TexObjPtr) tObj->DriverData;
   struct gl_texture_image *baseImage = tObj->Image[tObj->BaseLevel];
   int log2Pitch, log2Height, log2Size, log2MinSize;
   int totalSize;
   int i;
   GLint firstLevel, lastLevel;

   assert(t);
   assert(baseImage);

   if ( R128_DEBUG & DEBUG_VERBOSE_API )
      fprintf( stderr, "%s( %p )\n", __FUNCTION__, tObj );

   switch (baseImage->TexFormat->MesaFormat) {
   case MESA_FORMAT_ARGB8888:
      t->textureFormat = R128_DATATYPE_ARGB8888;
      break;
   case MESA_FORMAT_ARGB4444:
      t->textureFormat = R128_DATATYPE_ARGB4444;
      break;
   case MESA_FORMAT_RGB565:
      t->textureFormat = R128_DATATYPE_RGB565;
      break;
   case MESA_FORMAT_RGB332:
      t->textureFormat = R128_DATATYPE_RGB8;
      break;
   case MESA_FORMAT_CI8:
      t->textureFormat = R128_DATATYPE_CI8;
      break;
   default:
      _mesa_problem(rmesa->glCtx, "Bad texture format in r128SetTexImages");
   };

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

   log2Pitch = tObj->Image[firstLevel]->WidthLog2;
   log2Height = tObj->Image[firstLevel]->HeightLog2;
   log2Size = MAX2(log2Pitch, log2Height);
   log2MinSize = log2Size;

   t->dirty_images = 0;
   totalSize = 0;
   for ( i = firstLevel; i <= lastLevel; i++ ) {
      const struct gl_texture_image *texImage;

      texImage = tObj->Image[i];
      if ( !texImage || !texImage->Data ) {
         lastLevel = i - 1;
	 break;
      }

      log2MinSize = texImage->MaxLog2;

      t->image[i - firstLevel].offset = totalSize;
      t->image[i - firstLevel].width  = tObj->Image[i]->Width;
      t->image[i - firstLevel].height = tObj->Image[i]->Height;

      t->dirty_images |= (1 << i);

      totalSize += (tObj->Image[i]->Height *
		    tObj->Image[i]->Width *
		    tObj->Image[i]->TexFormat->TexelBytes);

      /* Offsets must be 32-byte aligned for host data blits and tiling */
      totalSize = (totalSize + 31) & ~31;
   }

   t->totalSize = totalSize;
   t->firstLevel = firstLevel;
   t->lastLevel = lastLevel;

   /* Set the texture format */
   t->setup.tex_cntl &= ~(0xf << 16);
   t->setup.tex_cntl |= t->textureFormat;

   t->setup.tex_combine_cntl = 0x00000000;  /* XXX is this right? */

   t->setup.tex_size_pitch = ((log2Pitch   << R128_TEX_PITCH_SHIFT) |
			      (log2Size    << R128_TEX_SIZE_SHIFT) |
			      (log2Height  << R128_TEX_HEIGHT_SHIFT) |
			      (log2MinSize << R128_TEX_MIN_SIZE_SHIFT));

   for ( i = 0 ; i < R128_MAX_TEXTURE_LEVELS ; i++ ) {
      t->setup.tex_offset[i]  = 0x00000000;
   }

   if (firstLevel == lastLevel)
      t->setup.tex_cntl |= R128_MIP_MAP_DISABLE;
   else
      t->setup.tex_cntl &= ~R128_MIP_MAP_DISABLE;

   /* XXX this is done in r128EmitHwStateLocked():
   r128UploadTexImages( rmesa, t );
   */
}


/* ================================================================
 * Texture combine functions
 */

#define COLOR_COMB_DISABLE		(R128_COMB_DIS |		\
					 R128_COLOR_FACTOR_TEX)
#define COLOR_COMB_COPY_INPUT		(R128_COMB_COPY_INP |		\
					 R128_COLOR_FACTOR_TEX)
#define COLOR_COMB_MODULATE		(R128_COMB_MODULATE |		\
					 R128_COLOR_FACTOR_TEX)
#define COLOR_COMB_MODULATE_NTEX	(R128_COMB_MODULATE |		\
					 R128_COLOR_FACTOR_NTEX)
#define COLOR_COMB_ADD			(R128_COMB_ADD |		\
					 R128_COLOR_FACTOR_TEX)
#define COLOR_COMB_BLEND_TEX		(R128_COMB_BLEND_TEXTURE |	\
					 R128_COLOR_FACTOR_TEX)
/* Rage 128 Pro/M3 only! */
#define COLOR_COMB_BLEND_COLOR		(R128_COMB_MODULATE2X |		\
					 R128_COMB_FCN_MSB |		\
					 R128_COLOR_FACTOR_CONST_COLOR)

#define ALPHA_COMB_DISABLE		(R128_COMB_ALPHA_DIS |		\
					 R128_ALPHA_FACTOR_TEX_ALPHA)
#define ALPHA_COMB_COPY_INPUT		(R128_COMB_ALPHA_COPY_INP |	\
					 R128_ALPHA_FACTOR_TEX_ALPHA)
#define ALPHA_COMB_MODULATE		(R128_COMB_ALPHA_MODULATE |	\
					 R128_ALPHA_FACTOR_TEX_ALPHA)
#define ALPHA_COMB_MODULATE_NTEX	(R128_COMB_ALPHA_MODULATE |	\
					 R128_ALPHA_FACTOR_NTEX_ALPHA)
#define ALPHA_COMB_ADD			(R128_COMB_ALPHA_ADD |		\
					 R128_ALPHA_FACTOR_TEX_ALPHA)

#define INPUT_INTERP			(R128_INPUT_FACTOR_INT_COLOR |	\
					 R128_INP_FACTOR_A_INT_ALPHA)
#define INPUT_PREVIOUS			(R128_INPUT_FACTOR_PREV_COLOR |	\
					 R128_INP_FACTOR_A_PREV_ALPHA)

static void r128UpdateTextureEnv( GLcontext *ctx, int unit )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   GLint source = rmesa->tmu_source[unit];
   const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[source];
   const struct gl_texture_object *tObj = texUnit->_Current;
   const GLenum format = tObj->Image[tObj->BaseLevel]->Format;
   GLuint combine;

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %p, %d )\n",
	       __FUNCTION__, ctx, unit );
   }

   if ( unit == 0 ) {
      combine = INPUT_INTERP;
   } else {
      combine = INPUT_PREVIOUS;
   }

   /* Set the texture environment state */
   switch ( texUnit->EnvMode ) {
   case GL_REPLACE:
      switch ( format ) {
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
      case GL_INTENSITY:
	 combine |= (COLOR_COMB_DISABLE |		/* C = Ct            */
		     ALPHA_COMB_DISABLE);		/* A = At            */
	 break;
      case GL_RGB:
      case GL_LUMINANCE:
	 combine |= (COLOR_COMB_DISABLE |		/* C = Ct            */
		     ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	 break;
      case GL_ALPHA:
	 combine |= (COLOR_COMB_COPY_INPUT |		/* C = Cf            */
		     ALPHA_COMB_DISABLE);		/* A = At            */
	 break;
      case GL_COLOR_INDEX:
      default:
	 return;
      }
      break;

   case GL_MODULATE:
      switch ( format ) {
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
      case GL_INTENSITY:
	 combine |= (COLOR_COMB_MODULATE |		/* C = CfCt          */
		     ALPHA_COMB_MODULATE);		/* A = AfAt          */
	 break;
      case GL_RGB:
      case GL_LUMINANCE:
	 combine |= (COLOR_COMB_MODULATE |		/* C = CfCt          */
		     ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	 break;
      case GL_ALPHA:
	 combine |= (COLOR_COMB_COPY_INPUT |		/* C = Cf            */
		     ALPHA_COMB_MODULATE);		/* A = AfAt          */
	 break;
      case GL_COLOR_INDEX:
      default:
	 return;
      }
      break;

   case GL_DECAL:
      switch ( format ) {
      case GL_RGBA:
	 combine |= (COLOR_COMB_BLEND_TEX |		/* C = Cf(1-At)+CtAt */
		     ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	 break;
      case GL_RGB:
	 combine |= (COLOR_COMB_DISABLE |		/* C = Ct            */
		     ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	 break;
      case GL_ALPHA:
      case GL_LUMINANCE:
      case GL_LUMINANCE_ALPHA:
      case GL_INTENSITY:
	 /* Undefined behaviour - just copy the incoming fragment */
	 combine |= (COLOR_COMB_COPY_INPUT |		/* C = undefined     */
		     ALPHA_COMB_COPY_INPUT);		/* A = undefined     */
	 break;
      case GL_COLOR_INDEX:
      default:
	 return;
      }
      break;

   case GL_BLEND:
      /* Rage 128 Pro and M3 can handle GL_BLEND texturing.
       */
      if ( !R128_IS_PLAIN( rmesa ) ) {
         /* XXX this hasn't been fully tested, I don't have a Pro card. -BP */
	 switch ( format ) {
	 case GL_RGBA:
	 case GL_LUMINANCE_ALPHA:
	    combine |= (COLOR_COMB_BLEND_COLOR |	/* C = Cf(1-Ct)+CcCt */
			ALPHA_COMB_MODULATE);		/* A = AfAt          */
	    break;

	 case GL_RGB:
	 case GL_LUMINANCE:
	    combine |= (COLOR_COMB_BLEND_COLOR |	/* C = Cf(1-Ct)+CcCt */
			ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	    break;

	 case GL_ALPHA:
	    combine |= (COLOR_COMB_COPY_INPUT |		/* C = Cf            */
			ALPHA_COMB_MODULATE);		/* A = AfAt          */
	    break;

	 case GL_INTENSITY:
	    /* GH: We could be smarter about this... */
	    switch ( rmesa->env_color & 0xff000000 ) {
	    case 0x00000000:
	       combine |= (COLOR_COMB_BLEND_COLOR |	/* C = Cf(1-It)+CcIt */
			   ALPHA_COMB_MODULATE_NTEX);	/* A = Af(1-It)      */
	    default:
	       combine |= (COLOR_COMB_MODULATE |	/* C = fallback      */
			   ALPHA_COMB_MODULATE);	/* A = fallback      */
	       FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
	       break;
	    }
	    break;

	 case GL_COLOR_INDEX:
	 default:
	    return;
	 }
	 break;
      }

      /* Rage 128 has to fake some cases of GL_BLEND, otherwise fallback
       * to software rendering.
       */
      if ( rmesa->blend_flags ) {
	 FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
      }
      switch ( format ) {
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
	 switch ( rmesa->env_color & 0x00ffffff ) {
	 case 0x00000000:
	    combine |= (COLOR_COMB_MODULATE_NTEX |	/* C = Cf(1-Ct)      */
			ALPHA_COMB_MODULATE);		/* A = AfAt          */
	    break;
#if 0
         /* This isn't right - BP */
	 case 0x00ffffff:
	    if ( unit == 0 ) {
	       combine |= (COLOR_COMB_MODULATE_NTEX |	/* C = Cf(1-Ct)      */
			   ALPHA_COMB_MODULATE);	/* A = AfAt          */
	    } else {
	       combine |= (COLOR_COMB_ADD |		/* C = Cf+Ct         */
			   ALPHA_COMB_COPY_INPUT);	/* A = Af            */
	    }
	    break;
#endif
	 default:
	    combine |= (COLOR_COMB_MODULATE |		/* C = fallback      */
			ALPHA_COMB_MODULATE);		/* A = fallback      */
	    FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
	    break;
	 }
	 break;
      case GL_RGB:
      case GL_LUMINANCE:
	 switch ( rmesa->env_color & 0x00ffffff ) {
	 case 0x00000000:
	    combine |= (COLOR_COMB_MODULATE_NTEX |	/* C = Cf(1-Ct)      */
			ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	    break;
#if 0
         /* This isn't right - BP */
	 case 0x00ffffff:
	    if ( unit == 0 ) {
	       combine |= (COLOR_COMB_MODULATE_NTEX |	/* C = Cf(1-Ct)      */
			   ALPHA_COMB_COPY_INPUT);	/* A = Af            */
	    } else {
	       combine |= (COLOR_COMB_ADD |		/* C = Cf+Ct         */
			   ALPHA_COMB_COPY_INPUT);	/* A = Af            */
	    }
	    break;
#endif
	 default:
	    combine |= (COLOR_COMB_MODULATE |		/* C = fallback      */
			ALPHA_COMB_COPY_INPUT);		/* A = fallback      */
	    FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
	    break;
	 }
	 break;
      case GL_ALPHA:
	 if ( unit == 0 ) {
	    combine |= (COLOR_COMB_COPY_INPUT |		/* C = Cf            */
			ALPHA_COMB_MODULATE);		/* A = AfAt          */
	 } else {
	    combine |= (COLOR_COMB_COPY_INPUT |		/* C = Cf            */
			ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	 }
	 break;
      case GL_INTENSITY:
	 switch ( rmesa->env_color & 0x00ffffff ) {
	 case 0x00000000:
	    combine |= COLOR_COMB_MODULATE_NTEX;	/* C = Cf(1-It)      */
	    break;
#if 0
         /* This isn't right - BP */
	 case 0x00ffffff:
	    if ( unit == 0 ) {
	       combine |= COLOR_COMB_MODULATE_NTEX;	/* C = Cf(1-It)      */
	    } else {
	       combine |= COLOR_COMB_ADD;		/* C = Cf+It         */
	    }
	    break;
#endif
	 default:
	    combine |= (COLOR_COMB_MODULATE |		/* C = fallback      */
			ALPHA_COMB_MODULATE);		/* A = fallback      */
	    FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
	    break;
	 }
	 switch ( rmesa->env_color & 0xff000000 ) {
	 case 0x00000000:
	    combine |= ALPHA_COMB_MODULATE_NTEX;	/* A = Af(1-It)      */
	    break;
#if 0
         /* This isn't right - BP */
	 case 0xff000000:
	    if ( unit == 0 ) {
	       combine |= ALPHA_COMB_MODULATE_NTEX;	/* A = Af(1-It)      */
	    } else {
	       combine |= ALPHA_COMB_ADD;		/* A = Af+It         */
	    }
	    break;
#endif
	 default:
	    combine |= (COLOR_COMB_MODULATE |		/* C = fallback      */
			ALPHA_COMB_MODULATE);		/* A = fallback      */
	    FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
	    break;
	 }
	 break;
      case GL_COLOR_INDEX:
      default:
	 return;
      }
      break;

   case GL_ADD:
      switch ( format ) {
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
	 combine |= (COLOR_COMB_ADD |			/* C = Cf+Ct         */
		     ALPHA_COMB_MODULATE);		/* A = AfAt          */
	 break;
      case GL_RGB:
      case GL_LUMINANCE:
	 combine |= (COLOR_COMB_ADD |			/* C = Cf+Ct         */
		     ALPHA_COMB_COPY_INPUT);		/* A = Af            */
	 break;
      case GL_ALPHA:
	 combine |= (COLOR_COMB_COPY_INPUT |		/* C = Cf            */
		     ALPHA_COMB_MODULATE);		/* A = AfAt          */
	 break;
      case GL_INTENSITY:
	 combine |= (COLOR_COMB_ADD |			/* C = Cf+Ct         */
		     ALPHA_COMB_ADD);			/* A = Af+At         */
	 break;
      case GL_COLOR_INDEX:
      default:
	 return;
      }
      break;

   default:
      return;
   }

   rmesa->tex_combine[unit] = combine;
}

static void r128UpdateTextureUnit( GLcontext *ctx, int unit )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   const int source = rmesa->tmu_source[unit];
   const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[source];
   const struct gl_texture_object *tObj = ctx->Texture.Unit[source]._Current;
   r128TexObjPtr t = tObj->DriverData;

   assert(unit == 0 || unit == 1);  /* only two tex units */

   if (texUnit->_ReallyEnabled & (TEXTURE0_1D | TEXTURE0_2D)) {

      assert(t);  /* should have driver tex data by now */

      /* Fallback if there's a texture border */
      if ( tObj->Image[tObj->BaseLevel]->Border > 0 ) {
         FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
         return;
      }

      /* Upload teximages */
      if (t->dirty_images) {
         r128SetTexImages( rmesa, tObj );
      }

      /* Bind to the given texture unit */
      rmesa->CurrentTexObj[unit] = t;
      t->bound |= (1 << unit);

      if ( t->memBlock )
         r128UpdateTexLRU( rmesa, t );

      /* register setup */
      if ( unit == 0 ) {
         rmesa->setup.tex_cntl_c       |= R128_TEXMAP_ENABLE;
         rmesa->setup.tex_size_pitch_c |= t->setup.tex_size_pitch << 0;
         rmesa->setup.scale_3d_cntl    &= ~R128_TEX_CACHE_SPLIT;
         t->setup.tex_cntl             &= ~R128_SEC_SELECT_SEC_ST;
      }
      else {
         rmesa->setup.tex_cntl_c       |= R128_SEC_TEXMAP_ENABLE;
         rmesa->setup.tex_size_pitch_c |= t->setup.tex_size_pitch << 16;
         rmesa->setup.scale_3d_cntl    |= R128_TEX_CACHE_SPLIT;
         t->setup.tex_cntl             |=  R128_SEC_SELECT_SEC_ST;
      }
   }
   else if (texUnit->_ReallyEnabled) {
      /* 3D or cube map texture enabled - fallback */
      FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_TRUE );
   }
   else {
      /* texture unit disabled */
   }
}


void r128UpdateTextureState( GLcontext *ctx )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %p ) enabled=0x%x\n",
	       __FUNCTION__, ctx, ctx->Texture._ReallyEnabled );
   }

   /* Clear any texturing fallbacks */
   FALLBACK( rmesa, R128_FALLBACK_TEXTURE, GL_FALSE );

   /* Unbind any currently bound textures */
   if ( rmesa->CurrentTexObj[0] )  rmesa->CurrentTexObj[0]->bound = 0;
   if ( rmesa->CurrentTexObj[1] )  rmesa->CurrentTexObj[1]->bound = 0;
   rmesa->CurrentTexObj[0] = NULL;
   rmesa->CurrentTexObj[1] = NULL;

   /* Disable all texturing until it is known to be good */
   rmesa->setup.tex_cntl_c &= ~(R128_TEXMAP_ENABLE |
				R128_SEC_TEXMAP_ENABLE);
   rmesa->setup.tex_size_pitch_c = 0x00000000;

   /*
    * Now examine texture enable state to do rest of setup.
    */
   rmesa->tmu_source[0] = 0;
   rmesa->tmu_source[1] = 1;
   rmesa->blend_flags &= ~R128_BLEND_MULTITEX;

   if (ctx->Texture._ReallyEnabled & TEXTURE1_ANY) {
      if (ctx->Texture._ReallyEnabled & TEXTURE0_ANY) {
         /* both texture 0 and 1 enabled */
	 if ( R128_IS_PLAIN( rmesa ) )
	    rmesa->blend_flags |= R128_BLEND_MULTITEX;
	 r128UpdateTextureUnit( ctx, 0 );
	 r128UpdateTextureEnv( ctx, 0 );
	 r128UpdateTextureUnit( ctx, 1 );
	 r128UpdateTextureEnv( ctx, 1 );
         rmesa->dirty |= (R128_UPLOAD_TEX0IMAGES | R128_UPLOAD_TEX0 |
                          R128_UPLOAD_TEX1IMAGES | R128_UPLOAD_TEX1);
      }
      else {
         /* only texture 1 enabled */
	 rmesa->tmu_source[0] = 1;
	 rmesa->tmu_source[1] = 0;
	 r128UpdateTextureUnit( ctx, 0 );
	 r128UpdateTextureEnv( ctx, 0 );
         rmesa->dirty |= (R128_UPLOAD_TEX0IMAGES | R128_UPLOAD_TEX0);
      }
   }
   else if (ctx->Texture._ReallyEnabled & TEXTURE0_ANY) {
      /* only texture 0 enabled */
      r128UpdateTextureUnit( ctx, 0 );
      r128UpdateTextureEnv( ctx, 0 );
      rmesa->dirty |= (R128_UPLOAD_TEX0IMAGES | R128_UPLOAD_TEX0);
   }

   rmesa->dirty |= R128_UPLOAD_CONTEXT;
}
