/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_tex.c,v 1.2 2002/11/05 17:46:08 tsi Exp $ */
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
#include "r200_state.h"
#include "r200_ioctl.h"
#include "r200_swtcl.h"
#include "r200_tex.h"

#include "colormac.h"
#include "context.h"
#include "enums.h"
#include "image.h"
#include "mem.h"
#include "mmath.h"
#include "simple_list.h"
#include "texformat.h"
#include "texstore.h"
#include "texutil.h"


/* =============================================================
 * Utility functions:
 */

static void r200SetTexWrap( r200TexObjPtr t, GLenum swrap, GLenum twrap )
{
   t->pp_txfilter &= ~(R200_CLAMP_S_MASK | R200_CLAMP_T_MASK);

   switch ( swrap ) {
   case GL_REPEAT:
      t->pp_txfilter |= R200_CLAMP_S_WRAP;
      break;
   case GL_CLAMP:
      t->pp_txfilter |= R200_CLAMP_S_CLAMP_LAST;
      break;
   case GL_CLAMP_TO_EDGE:
      t->pp_txfilter |= R200_CLAMP_S_CLAMP_LAST;
      break;
   }

   switch ( twrap ) {
   case GL_REPEAT:
      t->pp_txfilter |= R200_CLAMP_T_WRAP;
      break;
   case GL_CLAMP:
      t->pp_txfilter |= R200_CLAMP_T_CLAMP_LAST;
      break;
   case GL_CLAMP_TO_EDGE:
      t->pp_txfilter |= R200_CLAMP_T_CLAMP_LAST;
      break;
   }
}

static void r200SetTexMaxAnisotropy( r200TexObjPtr t, GLfloat max )
{
   t->pp_txfilter &= ~R200_MAX_ANISO_MASK;

   if ( max == 1.0 ) {
      t->pp_txfilter |= R200_MAX_ANISO_1_TO_1;
   } else if ( max <= 2.0 ) {
      t->pp_txfilter |= R200_MAX_ANISO_2_TO_1;
   } else if ( max <= 4.0 ) {
      t->pp_txfilter |= R200_MAX_ANISO_4_TO_1;
   } else if ( max <= 8.0 ) {
      t->pp_txfilter |= R200_MAX_ANISO_8_TO_1;
   } else {
      t->pp_txfilter |= R200_MAX_ANISO_16_TO_1;
   }
}

static void r200SetTexFilter( r200TexObjPtr t, GLenum minf, GLenum magf )
{
   GLuint anisotropy = (t->pp_txfilter & R200_MAX_ANISO_MASK);

   t->pp_txfilter &= ~(R200_MIN_FILTER_MASK | R200_MAG_FILTER_MASK);

   if ( anisotropy == R200_MAX_ANISO_1_TO_1 ) {
      switch ( minf ) {
      case GL_NEAREST:
	 t->pp_txfilter |= R200_MIN_FILTER_NEAREST;
	 break;
      case GL_LINEAR:
	 t->pp_txfilter |= R200_MIN_FILTER_LINEAR;
	 break;
      case GL_NEAREST_MIPMAP_NEAREST:
	 t->pp_txfilter |= R200_MIN_FILTER_NEAREST_MIP_NEAREST;
	 break;
      case GL_NEAREST_MIPMAP_LINEAR:
	 t->pp_txfilter |= R200_MIN_FILTER_LINEAR_MIP_NEAREST;
	 break;
      case GL_LINEAR_MIPMAP_NEAREST:
	 t->pp_txfilter |= R200_MIN_FILTER_NEAREST_MIP_LINEAR;
	 break;
      case GL_LINEAR_MIPMAP_LINEAR:
	 t->pp_txfilter |= R200_MIN_FILTER_LINEAR_MIP_LINEAR;
	 break;
      }
   } else {
      switch ( minf ) {
      case GL_NEAREST:
	 t->pp_txfilter |= R200_MIN_FILTER_ANISO_NEAREST;
	 break;
      case GL_LINEAR:
	 t->pp_txfilter |= R200_MIN_FILTER_ANISO_LINEAR;
	 break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
	 t->pp_txfilter |= R200_MIN_FILTER_ANISO_NEAREST_MIP_NEAREST;
	 break;
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
	 t->pp_txfilter |= R200_MIN_FILTER_ANISO_NEAREST_MIP_LINEAR;
	 break;
      }
   }

   switch ( magf ) {
   case GL_NEAREST:
      t->pp_txfilter |= R200_MAG_FILTER_NEAREST;
      break;
   case GL_LINEAR:
      t->pp_txfilter |= R200_MAG_FILTER_LINEAR;
      break;
   }
}

static void r200SetTexBorderColor( r200TexObjPtr t, GLubyte c[4] )
{
   t->pp_border_color = r200PackColor( 4, c[0], c[1], c[2], c[3] );
}


static r200TexObjPtr r200AllocTexObj( struct gl_texture_object *texObj )
{
   r200TexObjPtr t;

   t = CALLOC_STRUCT( r200_tex_obj );
   if (!t)
      return NULL;

   if ( R200_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, texObj, t );
   }

   t->tObj = texObj;
   make_empty_list( t );

   /* Initialize non-image-dependent parts of the state:
    */
   r200SetTexWrap( t, texObj->WrapS, texObj->WrapT );
   r200SetTexMaxAnisotropy( t, texObj->MaxAnisotropy );
   r200SetTexFilter( t, texObj->MinFilter, texObj->MagFilter );
   r200SetTexBorderColor( t, texObj->BorderColor );
   return t;
}


static const struct gl_texture_format *
r200ChooseTextureFormat( GLcontext *ctx, GLint internalFormat,
                           GLenum format, GLenum type )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   const GLboolean do32bpt = ( rmesa->r200Screen->cpp == 4 );

   switch ( internalFormat ) {
   case 4:
   case GL_RGBA:
      if ( format == GL_BGRA ) {
	 if ( type == GL_UNSIGNED_INT_8_8_8_8_REV ) {
	    return &_mesa_texformat_argb8888;
	 }
         else if ( type == GL_UNSIGNED_SHORT_4_4_4_4_REV ) {
            return &_mesa_texformat_argb4444;
	 }
         else if ( type == GL_UNSIGNED_SHORT_1_5_5_5_REV ) {
	    return &_mesa_texformat_argb1555;
	 }
      }
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_argb4444;

   case 3:
   case GL_RGB:
      if ( format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5 ) {
	 return &_mesa_texformat_rgb565;
      }
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_rgb565;

   case GL_RGBA8:
   case GL_RGB10_A2:
   case GL_RGBA12:
   case GL_RGBA16:
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_argb4444;

   case GL_RGBA4:
   case GL_RGBA2:
      return &_mesa_texformat_argb4444;

   case GL_RGB5_A1:
      return &_mesa_texformat_argb1555;

   case GL_RGB8:
   case GL_RGB10:
   case GL_RGB12:
   case GL_RGB16:
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_rgb565;

   case GL_RGB5:
   case GL_RGB4:
   case GL_R3_G3_B2:
      return &_mesa_texformat_rgb565;

   case GL_ALPHA:
   case GL_ALPHA4:
   case GL_ALPHA8:
   case GL_ALPHA12:
   case GL_ALPHA16:
      return &_mesa_texformat_al88;

   case 1:
   case GL_LUMINANCE:
   case GL_LUMINANCE4:
   case GL_LUMINANCE8:
   case GL_LUMINANCE12:
   case GL_LUMINANCE16:
      return &_mesa_texformat_al88;

   case 2:
   case GL_LUMINANCE_ALPHA:
   case GL_LUMINANCE4_ALPHA4:
   case GL_LUMINANCE6_ALPHA2:
   case GL_LUMINANCE8_ALPHA8:
   case GL_LUMINANCE12_ALPHA4:
   case GL_LUMINANCE12_ALPHA12:
   case GL_LUMINANCE16_ALPHA16:
      return &_mesa_texformat_al88;

   case GL_INTENSITY:
   case GL_INTENSITY4:
   case GL_INTENSITY8:
   case GL_INTENSITY12:
   case GL_INTENSITY16:
      /* At the moment, glean & conform both fail using the i8 internal
       * format.
       */
      return &_mesa_texformat_al88;
/*       return &_mesa_texformat_i8; */

   case GL_YCBCR_MESA:
      if (type == GL_UNSIGNED_SHORT_8_8_APPLE ||
	  type == GL_UNSIGNED_BYTE)
         return &_mesa_texformat_ycbcr;
      else
         return &_mesa_texformat_ycbcr_rev;

   default:
      _mesa_problem(ctx, "unexpected texture format in r200ChoosTexFormat");
      return NULL;
   }

   return NULL; /* never get here */
}


static GLboolean
r200ValidateClientStorage( GLcontext *ctx, GLenum target,
			   GLint internalFormat,
			   GLint srcWidth, GLint srcHeight, 
                           GLenum format, GLenum type,  const void *pixels,
			   const struct gl_pixelstore_attrib *packing,
			   struct gl_texture_object *texObj,
			   struct gl_texture_image *texImage)

{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   int texelBytes;

   if (0)
      fprintf(stderr, "intformat %s format %s type %s\n",
	      _mesa_lookup_enum_by_nr( internalFormat ),
	      _mesa_lookup_enum_by_nr( format ),
	      _mesa_lookup_enum_by_nr( type ));

   if (!ctx->Unpack.ClientStorage)
      return 0;

   if (ctx->_ImageTransferState ||
       texImage->IsCompressed ||
       texObj->GenerateMipmap)
      return 0;


   /* This list is incomplete, may be different on ppc???
    */
   switch ( internalFormat ) {
   case GL_RGBA:
      if ( format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8_REV ) {
	 texImage->TexFormat = &_mesa_texformat_argb8888;
	 texelBytes = 4;
      }
      else
	 return 0;
      break;

   case GL_YCBCR_MESA:
      if ( format == GL_YCBCR_MESA && 
	   type == GL_UNSIGNED_SHORT_8_8_REV_APPLE ) {
	 texImage->TexFormat = &_mesa_texformat_ycbcr_rev;
	 texelBytes = 2;
      }
      else if ( format == GL_YCBCR_MESA && 
		(type == GL_UNSIGNED_SHORT_8_8_APPLE || 
		 type == GL_UNSIGNED_BYTE)) {
	 texImage->TexFormat = &_mesa_texformat_ycbcr;
	 texelBytes = 2;
      }
      else
	 return 0;
      break;
      
	 
   default:
      return 0;
   }

   /* Could deal with these packing issues, but currently don't:
    */
   if (packing->SkipPixels || 
       packing->SkipRows || 
       packing->SwapBytes ||
       packing->LsbFirst) {
      return 0;
   }

   {      
      GLint srcRowStride = _mesa_image_row_stride(packing, srcWidth,
						  format, type);

      
      if (0)
	 fprintf(stderr, "%s: srcRowStride %d/%x\n", 
		 __FUNCTION__, srcRowStride, srcRowStride);

      /* Could check this later in upload, pitch restrictions could be
       * relaxed, but would need to store the image pitch somewhere,
       * as packing details might change before image is uploaded:
       */
      if (!r200IsAgpMemory( rmesa, pixels, srcHeight * srcRowStride ) ||
	  (srcRowStride & 63))
	 return 0;


      /* Have validated that _mesa_transfer_teximage would be a straight
       * memcpy at this point.  NOTE: future calls to TexSubImage will
       * overwrite the client data.  This is explicitly mentioned in the
       * extension spec.
       */
      texImage->Data = (void *)pixels;
      texImage->IsClientData = GL_TRUE;
      texImage->RowStride = srcRowStride / texelBytes;
      return 1;
   }
}


static void r200TexImage1D( GLcontext *ctx, GLenum target, GLint level,
                              GLint internalFormat,
                              GLint width, GLint border,
                              GLenum format, GLenum type, const GLvoid *pixels,
                              const struct gl_pixelstore_attrib *packing,
                              struct gl_texture_object *texObj,
                              struct gl_texture_image *texImage )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200TexObjPtr t = (r200TexObjPtr) texObj->DriverData;

   if ( t ) {
      r200SwapOutTexObj( rmesa, t );
   }
   else {
      t = r200AllocTexObj( texObj );
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage1D");
         return;
      }
      texObj->DriverData = t;
   }

   /* Note, this will call r200ChooseTextureFormat */
   _mesa_store_teximage1d(ctx, target, level, internalFormat,
                          width, border, format, type, pixels,
                          &ctx->Unpack, texObj, texImage);

   t->dirty_images |= (1 << level);
}


static void r200TexSubImage1D( GLcontext *ctx, GLenum target, GLint level,
                                 GLint xoffset,
                                 GLsizei width,
                                 GLenum format, GLenum type,
                                 const GLvoid *pixels,
                                 const struct gl_pixelstore_attrib *packing,
                                 struct gl_texture_object *texObj,
                                 struct gl_texture_image *texImage )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200TexObjPtr t = (r200TexObjPtr)texObj->DriverData;

   assert( t ); /* this _should_ be true */
   if ( t ) {
      r200SwapOutTexObj( rmesa, t );
      t->dirty_images |= (1 << level);
   }
   else {
      t = r200AllocTexObj(texObj);
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
}


static void r200TexImage2D( GLcontext *ctx, GLenum target, GLint level,
                              GLint internalFormat,
                              GLint width, GLint height, GLint border,
                              GLenum format, GLenum type, const GLvoid *pixels,
                              const struct gl_pixelstore_attrib *packing,
                              struct gl_texture_object *texObj,
                              struct gl_texture_image *texImage )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200TexObjPtr t = (r200TexObjPtr)texObj->DriverData;

   if ( t ) {
      r200SwapOutTexObj( rmesa, t );
   }
   else {
      t = r200AllocTexObj( texObj );
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
         return;
      }
      texObj->DriverData = t;
   }

   texImage->IsClientData = GL_FALSE;

   if (r200ValidateClientStorage( ctx, target, 
				  internalFormat, 
				  width, height, 
				  format, type, pixels, 
				  packing, texObj, texImage)) {
      if (R200_DEBUG & DEBUG_TEXTURE)
	 fprintf(stderr, "%s: Using client storage\n", __FUNCTION__); 
   }
   else {
      if (R200_DEBUG & DEBUG_TEXTURE)
	 fprintf(stderr, "%s: Using normal storage\n", __FUNCTION__); 

      /* Normal path: copy (to cached memory) and eventually upload
       * via another copy to agp memory and then a blit...  Could
       * eliminate one copy by going straight to (permanent) agp.
       *
       * Note, this will call r200ChooseTextureFormat.
       */
      _mesa_store_teximage2d(ctx, target, level, internalFormat,
			     width, height, border, format, type, pixels,
			     &ctx->Unpack, texObj, texImage);
      
      t->dirty_images |= (1 << level);
   }
}


static void r200TexSubImage2D( GLcontext *ctx, GLenum target, GLint level,
                                 GLint xoffset, GLint yoffset,
                                 GLsizei width, GLsizei height,
                                 GLenum format, GLenum type,
                                 const GLvoid *pixels,
                                 const struct gl_pixelstore_attrib *packing,
                                 struct gl_texture_object *texObj,
                                 struct gl_texture_image *texImage )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200TexObjPtr t = (r200TexObjPtr) texObj->DriverData;

/*     fprintf(stderr, "%s\n", __FUNCTION__); */

   assert( t ); /* this _should_ be true */
   if ( t ) {
      r200SwapOutTexObj( rmesa, t );
   }
   else {
      t = r200AllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexSubImage2D");
         return;
      }
      texObj->DriverData = t;
   }

   _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width,
			     height, format, type, pixels, packing, texObj,
			     texImage);

   t->dirty_images |= (1 << level);
}


static void r200TexEnv( GLcontext *ctx, GLenum target,
			  GLenum pname, const GLfloat *param )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLuint unit = ctx->Texture.CurrentUnit;
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];

   if ( R200_DEBUG & DEBUG_STATE ) {
      fprintf( stderr, "%s( %s )\n",
	       __FUNCTION__, _mesa_lookup_enum_by_nr( pname ) );
   }

   /* This is incorrect: Need to maintain this data for each of
    * GL_TEXTURE_{123}D, GL_TEXTURE_RECTANGLE_NV, etc, and switch
    * between them according to _ReallyEnabled.
    */
   switch ( pname ) {
   case GL_TEXTURE_ENV_COLOR: {
      GLubyte c[4];
      GLuint envColor;
      UNCLAMPED_FLOAT_TO_RGBA_CHAN( c, texUnit->EnvColor );
      envColor = r200PackColor( 4, c[0], c[1], c[2], c[3] );
      if ( rmesa->hw.tf.cmd[TF_TFACTOR_0 + unit] != envColor ) {
	 R200_STATECHANGE( rmesa, tf );
	 rmesa->hw.tf.cmd[TF_TFACTOR_0 + unit] = envColor;
      }
      break;
   }

   case GL_TEXTURE_LOD_BIAS_EXT: {
      GLfloat bias;
      GLuint b;
      const int fixed_one = 0x8000000;

      /* The R200's LOD bias is a signed 2's complement value with a
       * range of -16.0 <= bias < 16.0. 
       *
       * NOTE: Add a small bias to the bias for conform mipsel.c test.
       */
      bias = *param + .01;
      bias = CLAMP( bias, -16.0, 16.0 );
      b = (int)(bias * fixed_one) & R200_LOD_BIAS_MASK;
      
      if ( (rmesa->hw.tex[unit].cmd[TEX_PP_TXFORMAT_X] & R200_LOD_BIAS_MASK) != b ) {
	 R200_STATECHANGE( rmesa, tex[unit] );
	 rmesa->hw.tex[unit].cmd[TEX_PP_TXFORMAT_X] &= ~R200_LOD_BIAS_MASK;
	 rmesa->hw.tex[unit].cmd[TEX_PP_TXFORMAT_X] |= b;
      }
      break;
   }

   default:
      return;
   }
}

static void r200TexParameter( GLcontext *ctx, GLenum target,
				struct gl_texture_object *texObj,
				GLenum pname, const GLfloat *params )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200TexObjPtr t = (r200TexObjPtr) texObj->DriverData;

   if ( R200_DEBUG & (DEBUG_STATE|DEBUG_TEXTURE) ) {
      fprintf( stderr, "%s( %s )\n", __FUNCTION__,
	       _mesa_lookup_enum_by_nr( pname ) );
   }

   if (!t)
      return;

   switch ( pname ) {
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
   case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      r200SetTexMaxAnisotropy( t, texObj->MaxAnisotropy );
      r200SetTexFilter( t, texObj->MinFilter, texObj->MagFilter );
      break;

   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
      r200SetTexWrap( t, texObj->WrapS, texObj->WrapT );
      break;

   case GL_TEXTURE_BORDER_COLOR:
      r200SetTexBorderColor( t, texObj->BorderColor );
      break;

   case GL_TEXTURE_BASE_LEVEL:
   case GL_TEXTURE_MAX_LEVEL:
   case GL_TEXTURE_MIN_LOD:
   case GL_TEXTURE_MAX_LOD:
      /* This isn't the most efficient solution but there doesn't appear to
       * be a nice alternative for R200.  Since there's no LOD clamping,
       * we just have to rely on loading the right subset of mipmap levels
       * to simulate a clamped LOD.
       */
      r200SwapOutTexObj( rmesa, t );
      break;

   default:
      return;
   }

   /* Mark this texobj as dirty (one bit per tex unit)
    */
   t->dirty_state = TEX_ALL;
}



static void r200BindTexture( GLcontext *ctx, GLenum target,
			       struct gl_texture_object *texObj )
{
   r200TexObjPtr t = (r200TexObjPtr) texObj->DriverData;
   GLuint unit = ctx->Texture.CurrentUnit;

   if ( R200_DEBUG & (DEBUG_STATE|DEBUG_TEXTURE) ) {
      fprintf( stderr, "%s( %p ) unit=%d\n", __FUNCTION__, texObj, unit );
   }

   if ( target == GL_TEXTURE_2D || target == GL_TEXTURE_1D ) {
      if ( !t ) {
	 t = r200AllocTexObj( texObj );
	 texObj->DriverData = t;
      }
   }
}

static void r200DeleteTexture( GLcontext *ctx,
				 struct gl_texture_object *texObj )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200TexObjPtr t = (r200TexObjPtr) texObj->DriverData;

   if ( R200_DEBUG & (DEBUG_STATE|DEBUG_TEXTURE) ) {
      fprintf( stderr, "%s( %p )\n", __FUNCTION__, texObj );
   }

   if ( t ) {
      if ( rmesa ) {
         R200_FIREVERTICES( rmesa );
      }
      r200DestroyTexObj( rmesa, t );
      texObj->DriverData = NULL;
   }
}

static GLboolean r200IsTextureResident( GLcontext *ctx,
					  struct gl_texture_object *texObj )
{
   r200TexObjPtr t = (r200TexObjPtr) texObj->DriverData;

   return ( t && t->memBlock );
}


static void r200InitTextureObjects( GLcontext *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   struct gl_texture_object *texObj;
   GLuint tmp = ctx->Texture.CurrentUnit;

   ctx->Texture.CurrentUnit = 0;

   texObj = ctx->Texture.Unit[0].Current1D;
   r200BindTexture( ctx, GL_TEXTURE_1D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (r200TexObjPtr)texObj->DriverData );

   texObj = ctx->Texture.Unit[0].Current2D;
   r200BindTexture( ctx, GL_TEXTURE_2D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (r200TexObjPtr)texObj->DriverData );

   ctx->Texture.CurrentUnit = 1;

   texObj = ctx->Texture.Unit[1].Current1D;
   r200BindTexture( ctx, GL_TEXTURE_1D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (r200TexObjPtr)texObj->DriverData );

   texObj = ctx->Texture.Unit[1].Current2D;
   r200BindTexture( ctx, GL_TEXTURE_2D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (r200TexObjPtr)texObj->DriverData );

   ctx->Texture.CurrentUnit = tmp;
}

/* Need:  
 *  - Same GEN_MODE for all active bits
 *  - Same EyePlane/ObjPlane for all active bits when using Eye/Obj
 *  - STRQ presumably all supported (matrix means incoming R values
 *    can end up in STQ, this has implications for vertex support,
 *    presumably ok if maos is used, though?)
 *  
 * Basically impossible to do this on the fly - just collect some
 * basic info & do the checks from ValidateState().
 */
static void r200TexGen( GLcontext *ctx,
			  GLenum coord,
			  GLenum pname,
			  const GLfloat *params )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLuint unit = ctx->Texture.CurrentUnit;
   rmesa->recheck_texgen[unit] = GL_TRUE;
}


void r200InitTextureFuncs( GLcontext *ctx )
{
   ctx->Driver.ChooseTextureFormat	= r200ChooseTextureFormat;
   ctx->Driver.TexImage1D		= r200TexImage1D;
   ctx->Driver.TexImage2D		= r200TexImage2D;
   ctx->Driver.TexImage3D		= _mesa_store_teximage3d;
   ctx->Driver.TexSubImage1D		= r200TexSubImage1D;
   ctx->Driver.TexSubImage2D		= r200TexSubImage2D;
   ctx->Driver.TexSubImage3D		= _mesa_store_texsubimage3d;
   ctx->Driver.CopyTexImage1D		= _swrast_copy_teximage1d;
   ctx->Driver.CopyTexImage2D		= _swrast_copy_teximage2d;
   ctx->Driver.CopyTexSubImage1D	= _swrast_copy_texsubimage1d;
   ctx->Driver.CopyTexSubImage2D	= _swrast_copy_texsubimage2d;
   ctx->Driver.CopyTexSubImage3D 	= _swrast_copy_texsubimage3d;
   ctx->Driver.TestProxyTexImage	= _mesa_test_proxy_teximage;

   ctx->Driver.BindTexture		= r200BindTexture;
   ctx->Driver.CreateTexture		= NULL; /* FIXME: Is this used??? */
   ctx->Driver.DeleteTexture		= r200DeleteTexture;
   ctx->Driver.IsTextureResident	= r200IsTextureResident;
   ctx->Driver.PrioritizeTexture	= NULL;
   ctx->Driver.ActiveTexture		= NULL;
   ctx->Driver.UpdateTexturePalette	= NULL;

   ctx->Driver.TexEnv			= r200TexEnv;
   ctx->Driver.TexParameter		= r200TexParameter;
   ctx->Driver.TexGen                   = r200TexGen;

   r200InitTextureObjects( ctx );
}
