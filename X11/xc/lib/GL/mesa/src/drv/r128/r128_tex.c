/* $XFree86: xc/lib/GL/mesa/src/drv/r128/r128_tex.c,v 1.14 2002/11/05 17:46:08 tsi Exp $ */
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
#include "r128_texobj.h"

#include "context.h"
#include "macros.h"
#include "mmath.h"
#include "simple_list.h"
#include "enums.h"
#include "texstore.h"
#include "texformat.h"
#include "mem.h"

#define TEX_0	1
#define TEX_1	2


static void r128SetTexWrap( r128TexObjPtr t, GLenum swrap, GLenum twrap )
{
   t->setup.tex_cntl &= ~(R128_TEX_CLAMP_S_MASK | R128_TEX_CLAMP_T_MASK);

   switch ( swrap ) {
   case GL_CLAMP:
      t->setup.tex_cntl |= R128_TEX_CLAMP_S_BORDER_COLOR;
      break;
   case GL_CLAMP_TO_EDGE:
      t->setup.tex_cntl |= R128_TEX_CLAMP_S_CLAMP;
      break;
   case GL_REPEAT:
      t->setup.tex_cntl |= R128_TEX_CLAMP_S_WRAP;
      break;
   }

   switch ( twrap ) {
   case GL_CLAMP:
      t->setup.tex_cntl |= R128_TEX_CLAMP_T_BORDER_COLOR;
      break;
   case GL_CLAMP_TO_EDGE:
      t->setup.tex_cntl |= R128_TEX_CLAMP_T_CLAMP;
      break;
   case GL_REPEAT:
      t->setup.tex_cntl |= R128_TEX_CLAMP_T_WRAP;
      break;
   }
}

static void r128SetTexFilter( r128TexObjPtr t, GLenum minf, GLenum magf )
{
   t->setup.tex_cntl &= ~(R128_MIN_BLEND_MASK | R128_MAG_BLEND_MASK);

   switch ( minf ) {
   case GL_NEAREST:
      t->setup.tex_cntl |= R128_MIN_BLEND_NEAREST;
      break;
   case GL_LINEAR:
      t->setup.tex_cntl |= R128_MIN_BLEND_LINEAR;
      break;
   case GL_NEAREST_MIPMAP_NEAREST:
      t->setup.tex_cntl |= R128_MIN_BLEND_MIPNEAREST;
      break;
   case GL_LINEAR_MIPMAP_NEAREST:
      t->setup.tex_cntl |= R128_MIN_BLEND_MIPLINEAR;
      break;
   case GL_NEAREST_MIPMAP_LINEAR:
      t->setup.tex_cntl |= R128_MIN_BLEND_LINEARMIPNEAREST;
      break;
   case GL_LINEAR_MIPMAP_LINEAR:
      t->setup.tex_cntl |= R128_MIN_BLEND_LINEARMIPLINEAR;
      break;
   }

   switch ( magf ) {
   case GL_NEAREST:
      t->setup.tex_cntl |= R128_MAG_BLEND_NEAREST;
      break;
   case GL_LINEAR:
      t->setup.tex_cntl |= R128_MAG_BLEND_LINEAR;
      break;
   }
}

static void r128SetTexBorderColor( r128TexObjPtr t, GLubyte c[4] )
{
   t->setup.tex_border_color = r128PackColor( 4, c[0], c[1], c[2], c[3] );
}


static r128TexObjPtr r128AllocTexObj( struct gl_texture_object *texObj )
{
   r128TexObjPtr t;

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %p )\n", __FUNCTION__, texObj );
   }

   t = (r128TexObjPtr) CALLOC_STRUCT( r128_tex_obj );
   if (!t)
      return NULL;

   /* Initialize non-image-dependent parts of the state:
    */
   t->tObj = texObj;
   t->dirty_images = ~0;

   make_empty_list( t );

   r128SetTexWrap( t, texObj->WrapS, texObj->WrapT );
   /*r128SetTexMaxAnisotropy( t, texObj->MaxAnisotropy );*/
   r128SetTexFilter( t, texObj->MinFilter, texObj->MagFilter );
   r128SetTexBorderColor( t, texObj->BorderColor );

   return t;
}


/* Called by the _mesa_store_teximage[123]d() functions. */
static const struct gl_texture_format *
r128ChooseTextureFormat( GLcontext *ctx, GLint internalFormat,
                         GLenum format, GLenum type )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   (void) format;
   (void) type;

   switch ( internalFormat ) {
   case GL_ALPHA:
   case GL_ALPHA4:
   case GL_ALPHA8:
   case GL_ALPHA12:
   case GL_ALPHA16:
   case 2:
   case GL_LUMINANCE_ALPHA:
   case GL_LUMINANCE4_ALPHA4:
   case GL_LUMINANCE6_ALPHA2:
   case GL_LUMINANCE8_ALPHA8:
   case GL_LUMINANCE12_ALPHA4:
   case GL_LUMINANCE12_ALPHA12:
   case GL_LUMINANCE16_ALPHA16:
   case 4:
   case GL_RGBA:
   case GL_RGBA2:
   case GL_RGB5_A1:
   case GL_RGBA8:
   case GL_RGB10_A2:
   case GL_RGBA12:
   case GL_RGBA16:
      if (rmesa->r128Screen->cpp == 4)
         return &_mesa_texformat_argb8888;
      else
         return &_mesa_texformat_argb4444;
   case GL_RGBA4:
         return &_mesa_texformat_argb4444;

   case 3:
   case GL_RGB:
   case GL_R3_G3_B2:
   case GL_RGB4:
   case GL_RGB5:
   case GL_RGB8:
   case GL_RGB10:
   case GL_RGB12:
   case GL_RGB16:
      if (rmesa->r128Screen->cpp == 4)
         return &_mesa_texformat_argb8888;
      else
         return &_mesa_texformat_rgb565;

   case 1:
   case GL_LUMINANCE:
   case GL_LUMINANCE4:
   case GL_LUMINANCE8:
   case GL_LUMINANCE12:
   case GL_LUMINANCE16:
      if (rmesa->r128Screen->cpp == 4)
         return &_mesa_texformat_argb8888; /* inefficient but accurate */
      else
         return &_mesa_texformat_rgb565;

   case GL_INTENSITY4:
      return &_mesa_texformat_argb4444;
   case GL_INTENSITY:
   case GL_INTENSITY8:
   case GL_INTENSITY12:
   case GL_INTENSITY16:
      if (rmesa->r128Screen->cpp == 4)
         return &_mesa_texformat_argb8888; /* inefficient but accurate */
      else
         return &_mesa_texformat_argb4444;

   case GL_COLOR_INDEX:
   case GL_COLOR_INDEX1_EXT:
   case GL_COLOR_INDEX2_EXT:
   case GL_COLOR_INDEX4_EXT:
   case GL_COLOR_INDEX8_EXT:
   case GL_COLOR_INDEX12_EXT:
   case GL_COLOR_INDEX16_EXT:
      return &_mesa_texformat_ci8;

   default:
      _mesa_problem( ctx, "unexpected format in r128ChooseTextureFormat" );
      return NULL;
   }
}


static void r128TexImage1D( GLcontext *ctx, GLenum target, GLint level,
			    GLint internalFormat,
			    GLint width, GLint border,
			    GLenum format, GLenum type, const GLvoid *pixels,
			    const struct gl_pixelstore_attrib *packing,
			    struct gl_texture_object *texObj,
			    struct gl_texture_image *texImage )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   r128TexObjPtr t = (r128TexObjPtr) texObj->DriverData;

   if ( t ) {
      r128SwapOutTexObj( R128_CONTEXT(ctx), t );
   }
   else {
      t = r128AllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage1D");
         return;
      }
      texObj->DriverData = t;
   }

   /* Note, this will call r128ChooseTextureFormat */
   _mesa_store_teximage1d( ctx, target, level, internalFormat,
			   width, border, format, type,
			   pixels, packing, texObj, texImage );

   t->dirty_images |= (1 << level);
   rmesa->new_state |= R128_NEW_TEXTURE;
}


static void r128TexSubImage1D( GLcontext *ctx,
			       GLenum target,
			       GLint level,
			       GLint xoffset,
			       GLsizei width,
			       GLenum format, GLenum type,
			       const GLvoid *pixels,
			       const struct gl_pixelstore_attrib *packing,
			       struct gl_texture_object *texObj,
			       struct gl_texture_image *texImage )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   r128TexObjPtr t = (r128TexObjPtr) texObj->DriverData;

   assert( t ); /* this _should_ be true */
   if ( t ) {
      r128SwapOutTexObj( R128_CONTEXT(ctx), t );
      t->dirty_images |= (1 << level);
   }
   else {
      t = r128AllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexSubImage1D");
         return;
      }
      texObj->DriverData = t;
   }

   _mesa_store_texsubimage1d(ctx, target, level, xoffset, width,
			     format, type, pixels, packing, texObj,
			     texImage);

   t->dirty_images |= (1 << level);
   rmesa->new_state |= R128_NEW_TEXTURE;
}


static void r128TexImage2D( GLcontext *ctx, GLenum target, GLint level,
			    GLint internalFormat,
			    GLint width, GLint height, GLint border,
			    GLenum format, GLenum type, const GLvoid *pixels,
			    const struct gl_pixelstore_attrib *packing,
			    struct gl_texture_object *texObj,
			    struct gl_texture_image *texImage )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   r128TexObjPtr t = (r128TexObjPtr) texObj->DriverData;

   if ( t ) {
      r128SwapOutTexObj( R128_CONTEXT(ctx), t );
   }
   else {
      t = r128AllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
         return;
      }
      texObj->DriverData = t;
   }

   /* Note, this will call r128ChooseTextureFormat */
   _mesa_store_teximage2d(ctx, target, level, internalFormat,
                          width, height, border, format, type, pixels,
                          &ctx->Unpack, texObj, texImage);

   t->dirty_images |= (1 << level);
   rmesa->new_state |= R128_NEW_TEXTURE;
}


static void r128TexSubImage2D( GLcontext *ctx,
			       GLenum target,
			       GLint level,
			       GLint xoffset, GLint yoffset,
			       GLsizei width, GLsizei height,
			       GLenum format, GLenum type,
			       const GLvoid *pixels,
			       const struct gl_pixelstore_attrib *packing,
			       struct gl_texture_object *texObj,
			       struct gl_texture_image *texImage )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   r128TexObjPtr t = (r128TexObjPtr) texObj->DriverData;

   assert( t ); /* this _should_ be true */
   if ( t ) {
      r128SwapOutTexObj( R128_CONTEXT(ctx), t );
   }
   else {
      t = r128AllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
         return;
      }
      texObj->DriverData = t;
   }

   _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width,
			     height, format, type, pixels, packing, texObj,
			     texImage);
   t->dirty_images |= (1 << level);
   rmesa->new_state |= R128_NEW_TEXTURE;
}


static void r128DDTexEnv( GLcontext *ctx, GLenum target,
			  GLenum pname, const GLfloat *param )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   struct gl_texture_unit *texUnit;
   GLubyte c[4];

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %s )\n",
	       __FUNCTION__, _mesa_lookup_enum_by_nr( pname ) );
   }

   switch ( pname ) {
   case GL_TEXTURE_ENV_MODE:
      FLUSH_BATCH( rmesa );
      rmesa->new_state |= R128_NEW_TEXTURE | R128_NEW_ALPHA;
      break;

   case GL_TEXTURE_ENV_COLOR:
      texUnit = &ctx->Texture.Unit[ctx->Texture.CurrentUnit];
      CLAMPED_FLOAT_TO_UBYTE( c[0], texUnit->EnvColor[0] );
      CLAMPED_FLOAT_TO_UBYTE( c[1], texUnit->EnvColor[1] );
      CLAMPED_FLOAT_TO_UBYTE( c[2], texUnit->EnvColor[2] );
      CLAMPED_FLOAT_TO_UBYTE( c[3], texUnit->EnvColor[3] );
      rmesa->env_color = r128PackColor( 4, c[0], c[1], c[2], c[3] );
      if ( rmesa->setup.constant_color_c != rmesa->env_color ) {
	 FLUSH_BATCH( rmesa );
	 rmesa->setup.constant_color_c = rmesa->env_color;

	 rmesa->new_state |= R128_NEW_TEXTURE;

	 /* More complex multitexture/multipass fallbacks for GL_BLEND
	  * can be done later, but this allows a single pass GL_BLEND
	  * in some cases (ie. Performer town demo).  This is only
	  * applicable to the regular Rage 128, as the Pro and M3 can
	  * handle true single-pass GL_BLEND texturing.
	  */
	 rmesa->blend_flags &= ~R128_BLEND_ENV_COLOR;
	 if ( R128_IS_PLAIN( rmesa ) &&
	      rmesa->env_color != 0x00000000 &&
	      rmesa->env_color != 0xff000000 &&
	      rmesa->env_color != 0x00ffffff &&
	      rmesa->env_color != 0xffffffff ) {
	    rmesa->blend_flags |= R128_BLEND_ENV_COLOR;
	 }
      }
      break;

   case GL_TEXTURE_LOD_BIAS_EXT:
      do {
	 CARD32 t = rmesa->setup.tex_cntl_c;
	 GLint bias;
	 CARD32 b;

	 /* GTH: This isn't exactly correct, but gives good results up to a
	  * certain point.  It is better than completely ignoring the LOD
	  * bias.  Unfortunately there isn't much range in the bias, the
	  * spec mentions strides that vary between 0.5 and 2.0 but these
	  * numbers don't seem to relate the the GL LOD bias value at all.
	  */
	 if ( param[0] >= 1.0 ) {
	    bias = -128;
	 } else if ( param[0] >= 0.5 ) {
	    bias = -64;
	 } else if ( param[0] >= 0.25 ) {
	    bias = 0;
	 } else if ( param[0] >= 0.0 ) {
	    bias = 63;
	 } else {
	    bias = 127;
	 }

	 b = (CARD32)bias & 0xff;
	 t &= ~R128_LOD_BIAS_MASK;
	 t |= (b << R128_LOD_BIAS_SHIFT);

	 if ( rmesa->setup.tex_cntl_c != t ) {
	    FLUSH_BATCH( rmesa );
	    rmesa->setup.tex_cntl_c = t;
	    rmesa->dirty |= R128_UPLOAD_CONTEXT;
	 }
      } while (0);
      break;

   default:
      return;
   }
}


static void r128DDTexParameter( GLcontext *ctx, GLenum target,
				struct gl_texture_object *tObj,
				GLenum pname, const GLfloat *params )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   r128TexObjPtr t = (r128TexObjPtr)tObj->DriverData;

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %s )\n",
	       __FUNCTION__, _mesa_lookup_enum_by_nr( pname ) );
   }

   if ( ( target != GL_TEXTURE_2D ) && ( target != GL_TEXTURE_1D ) )
      return;

   if (!t) {
      t = r128AllocTexObj(tObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexParameter");
         return;
      }
      tObj->DriverData = t;
   }

   switch ( pname ) {
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
      if ( t->bound ) FLUSH_BATCH( rmesa );
      r128SetTexFilter( t, tObj->MinFilter, tObj->MagFilter );
      break;

   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
      if ( t->bound ) FLUSH_BATCH( rmesa );
      r128SetTexWrap( t, tObj->WrapS, tObj->WrapT );
      break;

   case GL_TEXTURE_BORDER_COLOR:
      if ( t->bound ) FLUSH_BATCH( rmesa );
      r128SetTexBorderColor( t, tObj->BorderColor );
      break;

   case GL_TEXTURE_BASE_LEVEL:
   case GL_TEXTURE_MAX_LEVEL:
   case GL_TEXTURE_MIN_LOD:
   case GL_TEXTURE_MAX_LOD:
      /* This isn't the most efficient solution but there doesn't appear to
       * be a nice alternative for R128.  Since there's no LOD clamping,
       * we just have to rely on loading the right subset of mipmap levels
       * to simulate a clamped LOD.
       */
      if ( t->bound ) FLUSH_BATCH( rmesa );
      r128SwapOutTexObj( rmesa, t );
      break;

   default:
      return;
   }

   rmesa->new_state |= R128_NEW_TEXTURE;
}

static void r128DDBindTexture( GLcontext *ctx, GLenum target,
			       struct gl_texture_object *tObj )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   GLint unit = ctx->Texture.CurrentUnit;

   if ( R128_DEBUG & DEBUG_VERBOSE_API ) {
      fprintf( stderr, "%s( %p ) unit=%d\n",
	       __FUNCTION__, tObj, unit );
   }

   FLUSH_BATCH( rmesa );

   if ( rmesa->CurrentTexObj[unit] ) {
      rmesa->CurrentTexObj[unit]->bound &= ~(1 << unit);
      rmesa->CurrentTexObj[unit] = NULL;
   }

   rmesa->new_state |= R128_NEW_TEXTURE;
}

static void r128DDDeleteTexture( GLcontext *ctx,
				 struct gl_texture_object *tObj )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   r128TexObjPtr t = (r128TexObjPtr)tObj->DriverData;

   if ( t ) {
      if ( t->bound ) {
	 FLUSH_BATCH( rmesa );

	 if ( t->bound & TEX_0 ) rmesa->CurrentTexObj[0] = NULL;
	 if ( t->bound & TEX_1 ) rmesa->CurrentTexObj[1] = NULL;
	 rmesa->new_state |= R128_NEW_TEXTURE;
      }

      r128DestroyTexObj( rmesa, t );
      tObj->DriverData = NULL;
   }
}

static GLboolean r128DDIsTextureResident( GLcontext *ctx,
					  struct gl_texture_object *tObj )
{
   r128TexObjPtr t = (r128TexObjPtr)tObj->DriverData;

   return ( t && t->memBlock );
}



void r128DDInitTextureFuncs( GLcontext *ctx )
{
   ctx->Driver.TexEnv			= r128DDTexEnv;
   ctx->Driver.ChooseTextureFormat	= r128ChooseTextureFormat;
   ctx->Driver.TexImage1D		= r128TexImage1D;
   ctx->Driver.TexSubImage1D		= r128TexSubImage1D;
   ctx->Driver.TexImage2D		= r128TexImage2D;
   ctx->Driver.TexSubImage2D		= r128TexSubImage2D;
   ctx->Driver.TexImage3D               = _mesa_store_teximage3d;
   ctx->Driver.TexSubImage3D            = _mesa_store_texsubimage3d;
   ctx->Driver.CopyTexImage1D           = _swrast_copy_teximage1d;
   ctx->Driver.CopyTexImage2D           = _swrast_copy_teximage2d;
   ctx->Driver.CopyTexSubImage1D        = _swrast_copy_texsubimage1d;
   ctx->Driver.CopyTexSubImage2D        = _swrast_copy_texsubimage2d;
   ctx->Driver.CopyTexSubImage3D        = _swrast_copy_texsubimage3d;
   ctx->Driver.TestProxyTexImage        = _mesa_test_proxy_teximage;
   ctx->Driver.TexParameter		= r128DDTexParameter;
   ctx->Driver.BindTexture		= r128DDBindTexture;
   ctx->Driver.DeleteTexture		= r128DDDeleteTexture;
   ctx->Driver.UpdateTexturePalette	= NULL;
   ctx->Driver.ActiveTexture		= NULL;
   ctx->Driver.IsTextureResident	= r128DDIsTextureResident;
   ctx->Driver.PrioritizeTexture	= NULL;
}
