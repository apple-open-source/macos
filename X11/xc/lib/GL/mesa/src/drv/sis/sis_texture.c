/**************************************************************************

Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_texture.c,v 1.5 2000/09/26 15:56:49 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"

static void
sis_set_texture_env0 (GLcontext * ctx, GLtextureObject * object, int unit);

static void
sis_set_texture_env1 (GLcontext * ctx, GLtextureObject * object, int unit);

static void
sis_set_texobj_parm (GLcontext * ctx, GLtextureObject * object, int hw_unit);

static void sis_reset_texture_env (GLcontext * ctx, int hw_unit);

static DWORD TransferTexturePitch (DWORD dwPitch);

void
sis_validate_texture (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  struct gl_texture_unit *tex_unit;
  struct gl_texture_object *tObj;
  sisTexobjInfo *texObjArea;

  if(ctx->Texture.ReallyEnabled & (TEXTURE0_3D | TEXTURE1_3D)) 
    {
      hwcx->swRenderFlag |= SIS_SW_TEXTURE_DIM;
      return;        
    }
  else
    {
      hwcx->swRenderFlag &= ~SIS_SW_TEXTURE_DIM;    
    }

  if ((ctx->Texture.ReallyEnabled & TEXTURE0_ANY) &&
      (ctx->Texture.ReallyEnabled & TEXTURE1_ANY))
    {
      int unit;

      for (unit = 0; unit <= 1; unit++)
	{
	  tex_unit = &ctx->Texture.Unit[unit];
	  tObj = tex_unit->Current;
	  texObjArea = (sisTexobjInfo *) tObj->DriverData;

	  if (hwcx->TexStates[unit] & NEW_TEXTURING)
	  {
            hwcx->swRenderFlag &= ~(SIS_SW_TEXTURE_OBJ << unit);
	    sis_set_texobj_parm (ctx, tObj, unit);
          }
          
	  if (hwcx->TexStates[unit] & NEW_TEXTURE_ENV)
	    {
              hwcx->swRenderFlag &= ~(SIS_SW_TEXTURE_ENV << unit);
	      if (unit == 0)
		sis_set_texture_env0 (ctx, tObj, unit);
	      else
		sis_set_texture_env1 (ctx, tObj, unit);
	    }

	  hwcx->TexStates[unit] = 0;
	}
    }
  else
    {
      int unit = (ctx->Texture.ReallyEnabled & TEXTURE0_ANY) ? 0 : 1;

      tex_unit = &ctx->Texture.Unit[unit];
      tObj = tex_unit->Current;
      texObjArea = (sisTexobjInfo *) tObj->DriverData;

      if (hwcx->TexStates[unit] & NEW_TEXTURING)
	sis_set_texobj_parm (ctx, tObj, unit);

      if (hwcx->TexStates[unit] & NEW_TEXTURE_ENV)
	{
	  if(unit == 0){
	    sis_set_texture_env0 (ctx, tObj, unit);
	    sis_reset_texture_env (ctx, 1);
	  }
	  else{
	    sis_set_texture_env1 (ctx, tObj, unit);
	    sis_reset_texture_env (ctx, 0);
	  }
	}

      hwcx->TexStates[unit] = 0;
    }
}

void 
sis_TexEnv( GLcontext *ctx, GLenum target, GLenum pname,
            const GLfloat *param )
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  hwcx->TexStates[ctx->Texture.CurrentUnit] |= NEW_TEXTURE_ENV;
}

void
sis_TexImage (GLcontext * ctx, GLenum target,
	      struct gl_texture_object *tObj, GLint level,
	      GLint internalFormat, const struct gl_texture_image *image)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIStextureArea *area;

  if (target == GL_TEXTURE_3D || image->Border)
    return;
  
  if (!tObj->DriverData)
    {
      sisTexobjInfo *info;
      
      info = (sisTexobjInfo *)calloc (1, sizeof (sisTexobjInfo));
      
      /* TODO */
      assert(info);
      
      info->prev = info->next = NULL;
      info->valid = GL_FALSE;
      tObj->DriverData = info;      
    }

  if (image->DriverData)
    {
      /* image has a copy in video memory, and it may be in the cache */

      ((sisTexobjInfo *) tObj->DriverData)->dirtyFlag |= SIS_TEX_ALL;
    }
  else
    {
      /* Optimize */    
      ((sisTexobjInfo *) tObj->DriverData)->dirtyFlag |= (SIS_TEX_PARAMETER
							  | SIS_TEX_ENV);
    }

  sis_alloc_texture_image (ctx, (struct gl_texture_image *) image);

  area = (SIStextureArea *) image->DriverData;
  assert (area->Data);

  if (area->Format == GL_RGB8)
    {
      int i;
      GLbyte *src = (GLbyte *)image->Data;
      GLbyte *dst = area->Data;

      for (i = 0; i < area->Size / 4; i++)
	{
          *(DWORD *)dst = *(DWORD *)src & 0x00ffffff;
	  src += 3;
	  dst += 4;
	}
    }
  else
    {
      memcpy (area->Data, image->Data, area->Size);
    }

  if (hwcx->PrevTexFormat[ctx->Texture.CurrentUnit] != area->Format)
    {
      hwcx->TexStates[ctx->Texture.CurrentUnit] |= NEW_TEXTURE_ENV;
      hwcx->PrevTexFormat[ctx->Texture.CurrentUnit] = area->Format;
    }
  hwcx->TexStates[ctx->Texture.CurrentUnit] |= NEW_TEXTURING;
}

void
sis_TexSubImage (GLcontext * ctx, GLenum target,
		 struct gl_texture_object *tObj, GLint level, GLint xoffset,
		 GLint yoffset, GLsizei width, GLsizei height, GLint
		 internalFormat, const struct gl_texture_image *image)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIStextureArea *area;
  int i, j;
  GLbyte *src, *dst;
  GLuint soffset, doffset;

  area = (SIStextureArea *) image->DriverData;

  hwcx->clearTexCache = GL_TRUE;

  {
    /*
     * Optimize
     */
    WaitEngIdle (hwcx);
  }

  if (area->Format == GL_RGB8)
    {
      src = (GLbyte *)image->Data + (xoffset + yoffset * image->Width) * 3;
      dst = area->Data + (xoffset + yoffset * image->Width) * 4;
      soffset = (image->Width - width) * 3;
      doffset = (image->Width - width) * 4;
      for (j = yoffset; j < yoffset + height; j++)
	{
	  for (i = xoffset; i < xoffset + width; i++)
	    {
              *(DWORD *)dst = *(DWORD *)src & 0x00ffffff;
	      src += 3;
	      dst += 4;
	    }
	  src += soffset;
	  dst += doffset;
	}
    }
  else
    {
      GLuint texelSize = area->texelSize;
      GLuint copySize = texelSize * width;

      src = (GLbyte *)image->Data +
	(xoffset + yoffset * image->Width) * texelSize;
      dst = area->Data + (xoffset + yoffset * image->Width) * texelSize;
      soffset = image->Width * texelSize;

      for (j = yoffset; j < yoffset + height; j++)
	{
	  memcpy (dst, src, copySize);
	  src += soffset;
	  dst += soffset;
	}
    }
}

void
sis_TexParameter (GLcontext * ctx, GLenum target,
		  struct gl_texture_object *tObj, GLenum pname, const
		  GLfloat * params)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  if (tObj->DriverData)
    ((sisTexobjInfo *) tObj->DriverData)->dirtyFlag |= SIS_TEX_PARAMETER;

  hwcx->TexStates[ctx->Texture.CurrentUnit] |= NEW_TEXTURING;
}

void
sis_BindTexture (GLcontext * ctx, GLenum target,
		 struct gl_texture_object *tObj)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  SIStextureArea *area;

  if (!tObj->Image[0])
    return;

  if (!(area = (SIStextureArea *) tObj->Image[0]->DriverData))
    return;

  if (hwcx->PrevTexFormat[ctx->Texture.CurrentUnit] != area->Format)
    {
      hwcx->TexStates[ctx->Texture.CurrentUnit] |= NEW_TEXTURE_ENV;
      hwcx->PrevTexFormat[ctx->Texture.CurrentUnit] = area->Format;
    }
  hwcx->TexStates[ctx->Texture.CurrentUnit] |= NEW_TEXTURING;
}

void
sis_DeleteTexture (GLcontext * ctx, struct gl_texture_object *tObj)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  int i;

  for (i = 0; i < MAX_TEXTURE_LEVELS; i++)
    {
      if (tObj->Image[i])
	sis_free_texture_image (tObj->Image[i]);
    }

  if (tObj->DriverData)
    {
      free (tObj->DriverData);
      tObj->DriverData = NULL;
    }
  else
    {
      /* 
       * this shows the texture is default object and never be a 
       * argument of sis_TexImage
       */
    }

  hwcx->clearTexCache = GL_TRUE;
}

void
sis_UpdateTexturePalette (GLcontext * ctx, struct gl_texture_object *tObj)
{

}

void
sis_UseGlobalTexturePalette (GLcontext * ctx, GLboolean state)
{

}

void
sis_ActiveTexture (GLcontext * ctx, GLuint texUnitNumber)
{
}

GLboolean sis_IsTextureResident (GLcontext * ctx, struct gl_texture_object *t)
{
  return GL_TRUE;
}

void
sis_PrioritizeTexture (GLcontext * ctx,
		       struct gl_texture_object *t, GLclampf priority)
{
}

static void
sis_set_texture_env0 (GLcontext * ctx, GLtextureObject * object, int unit)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  struct gl_texture_unit *texture_unit = &ctx->Texture.Unit[unit];

  GLtextureImage *image = object->Image[0];
  SIStextureArea *area = (SIStextureArea *) image->DriverData;

  /*
     current->hwTexBlendClr1 = RGB_STAGE1; 
     current->hwTexBlendAlpha1 = A_STAGE1;
   */

  switch (texture_unit->EnvMode)
    {
    case GL_REPLACE:
      switch (area->Format)
	{
	case GL_ALPHA8:
	  current->hwTexBlendClr0 = A_REPLACE_RGB_STAGE0;
	  current->hwTexBlendAlpha0 = A_REPLACE_A_STAGE0;
	  break;
	case GL_INTENSITY8:
	case GL_RGB8:
	  current->hwTexBlendClr0 = RGB_REPLACE__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGB_REPLACE__A_STAGE0;
	  break;
	case GL_LUMINANCE8:
	case GL_LUMINANCE8_ALPHA8:
	case GL_RGBA8:
	  current->hwTexBlendClr0 = RGBA_REPLACE__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGBA_REPLACE__A_STAGE0;
	  break;
	}
      break;

    case GL_MODULATE:
      switch (area->Format)
	{
	case GL_ALPHA8:
	  current->hwTexBlendClr0 = A_MODULATE_RGB_STAGE0;
	  current->hwTexBlendAlpha0 = A_MODULATE_A_STAGE0;
	  break;
	case GL_LUMINANCE8:
	case GL_RGB8:
	  current->hwTexBlendClr0 = RGB_MODULATE__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGB_MODULATE__A_STAGE0;
	  break;
	case GL_INTENSITY8:
	case GL_LUMINANCE8_ALPHA8:
	case GL_RGBA8:
	  current->hwTexBlendClr0 = RGBA_MODULATE__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGBA_MODULATE__A_STAGE0;
	  break;
	}
      break;

    case GL_DECAL:
      switch (area->Format)
	{
	case GL_RGB8:
	  current->hwTexBlendClr0 = RGB_DECAL__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGB_DECAL__A_STAGE0;
	  break;
	case GL_RGBA8:
	  current->hwTexBlendClr0 = RGBA_DECAL__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGBA_DECAL__A_STAGE0;
	  break;
	}
      break;

    case GL_BLEND:
      current->hwTexEnvColor =
	((DWORD) (texture_unit->EnvColor[3])) << 24 |
	((DWORD) (texture_unit->EnvColor[0])) << 16 |
	((DWORD) (texture_unit->EnvColor[1])) << 8 |
	((DWORD) (texture_unit->EnvColor[2]));
      switch (area->Format)
	{
	case GL_ALPHA8:
	  current->hwTexBlendClr0 = A_BLEND_RGB_STAGE0;
	  current->hwTexBlendAlpha0 = A_BLEND_A_STAGE0;
	  break;
	case GL_LUMINANCE8:
	case GL_RGB8:
	  current->hwTexBlendClr0 = RGB_BLEND__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGB_BLEND__A_STAGE0;
	  break;
	case GL_INTENSITY8:
	  current->hwTexBlendClr0 = I_BLEND__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = I_BLEND__A_STAGE0;
	  break;
	case GL_LUMINANCE8_ALPHA8:
	case GL_RGBA8:
	  current->hwTexBlendClr0 = RGBA_BLEND__RGB_STAGE0;
	  current->hwTexBlendAlpha0 = RGBA_BLEND__A_STAGE0;
	  break;
	}
      break;
    }

  if ((current->hwTexBlendClr0 ^ prev->hwTexBlendClr0) ||
      (current->hwTexBlendAlpha0 ^ prev->hwTexBlendAlpha0) ||
      (current->hwTexEnvColor ^ prev->hwTexEnvColor))
    {
      prev->hwTexEnvColor = current->hwTexEnvColor;
      prev->hwTexBlendClr0 = current->hwTexBlendClr0;
      prev->hwTexBlendAlpha0 = current->hwTexBlendAlpha0;
      hwcx->GlobalFlag |= GFLAG_TEXTUREENV;
    }
}

void
sis_set_texture_env1 (GLcontext * ctx, GLtextureObject * object, int unit)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  struct gl_texture_unit *texture_unit = &ctx->Texture.Unit[unit];
  GLtextureImage *image = object->Image[0];
  SIStextureArea *area = (SIStextureArea *) image->DriverData;

  /*
     current->hwTexBlendClr1 = RGB_STAGE1; current->hwTexBlendAlpha1 =
     A_STAGE1;
   */

  switch (texture_unit->EnvMode)
    {
    case GL_REPLACE:
      switch (area->Format)
	{
	case GL_ALPHA8:
	  current->hwTexBlendClr1 = A_REPLACE_RGB_STAGE1;
	  current->hwTexBlendAlpha1 = A_REPLACE_A_STAGE1;
	  break;
	case GL_INTENSITY8:
	case GL_RGB8:
	  current->hwTexBlendClr1 = RGB_REPLACE__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGB_REPLACE__A_STAGE1;
	  break;
	case GL_LUMINANCE8:
	case GL_LUMINANCE8_ALPHA8:
	case GL_RGBA8:
	  current->hwTexBlendClr1 = RGBA_REPLACE__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGBA_REPLACE__A_STAGE1;
	  break;
	}
      break;

    case GL_MODULATE:
      switch (area->Format)
	{
	case GL_ALPHA8:
	  current->hwTexBlendClr1 = A_MODULATE_RGB_STAGE1;
	  current->hwTexBlendAlpha1 = A_MODULATE_A_STAGE1;
	  break;
	case GL_LUMINANCE8:
	case GL_RGB8:
	  current->hwTexBlendClr1 = RGB_MODULATE__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGB_MODULATE__A_STAGE1;
	  break;
	case GL_INTENSITY8:
	case GL_LUMINANCE8_ALPHA8:
	case GL_RGBA8:
	  current->hwTexBlendClr1 = RGBA_MODULATE__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGBA_MODULATE__A_STAGE1;
	  break;
	}
      break;

    case GL_DECAL:

      switch (area->Format)
	{
	case GL_RGB8:
	  current->hwTexBlendClr1 = RGB_DECAL__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGB_DECAL__A_STAGE1;
	  break;
	case GL_RGBA8:
	  current->hwTexBlendClr1 = RGBA_DECAL__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGBA_DECAL__A_STAGE1;
	  break;
	}
      break;

    case GL_BLEND:
      current->hwTexEnvColor =
	((DWORD) (texture_unit->EnvColor[3])) << 24 |
	((DWORD) (texture_unit->EnvColor[0])) << 16 |
	((DWORD) (texture_unit->EnvColor[1])) << 8 |
	((DWORD) (texture_unit->EnvColor[2]));
      switch (area->Format)
	{
	case GL_ALPHA8:
	  current->hwTexBlendClr1 = A_BLEND_RGB_STAGE1;
	  current->hwTexBlendAlpha1 = A_BLEND_A_STAGE1;
	  break;
	case GL_LUMINANCE8:
	case GL_RGB8:
	  current->hwTexBlendClr1 = RGB_BLEND__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGB_BLEND__A_STAGE1;
	  break;
	case GL_INTENSITY8:
	  current->hwTexBlendClr1 = I_BLEND__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = I_BLEND__A_STAGE1;
	  break;
	case GL_LUMINANCE8_ALPHA8:
	case GL_RGBA8:
	  current->hwTexBlendClr1 = RGBA_BLEND__RGB_STAGE1;
	  current->hwTexBlendAlpha1 = RGBA_BLEND__A_STAGE1;
	  break;
	}
      break;
    }

  if ((current->hwTexBlendClr1 ^ prev->hwTexBlendClr1) ||
      (current->hwTexBlendAlpha1 ^ prev->hwTexBlendAlpha1) ||
      (current->hwTexEnvColor ^ prev->hwTexEnvColor))
    {
      prev->hwTexBlendClr1 = current->hwTexBlendClr1;
      prev->hwTexBlendAlpha1 = current->hwTexBlendAlpha1;
      prev->hwTexEnvColor = current->hwTexEnvColor;
      hwcx->GlobalFlag |= GFLAG_TEXTUREENV_1;
    }
}

static void
sis_set_texobj_parm (GLcontext * ctx, GLtextureObject * object, int hw_unit)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  GLtextureImage *image = object->Image[0];
  SIStextureArea *area = (SIStextureArea *) image->DriverData;

  GLint TxLevel;
  GLint i;

  current->texture[hw_unit].hwTextureSet = 0UL;
  current->texture[hw_unit].hwTextureMip = 0UL;

  switch (area->Format)
    {
    case GL_RGBA8:
      current->texture[hw_unit].hwTextureSet |= TEXEL_ABGR_8888_32;
      break;
    case GL_INTENSITY8:
      current->texture[hw_unit].hwTextureSet |= TEXEL_I8;
      break;
    case GL_ALPHA8:
      current->texture[hw_unit].hwTextureSet |= TEXEL_A8;
      break;
    case GL_LUMINANCE8:
      current->texture[hw_unit].hwTextureSet |= TEXEL_L8;
      break;
    case GL_LUMINANCE8_ALPHA8:
      current->texture[hw_unit].hwTextureSet |= TEXEL_AL88;
      break;
    case GL_RGB8:
      current->texture[hw_unit].hwTextureSet |= TEXEL_ABGR_0888_32;
      break;
    default:
      hwcx->swRenderFlag |= SIS_SW_TEXTURE_OBJ;
    }

  if ((object->MinFilter == GL_NEAREST) || (object->MinFilter == GL_LINEAR))
    {
      TxLevel = 0;
    }
  else
    {
      TxLevel = object->P;
    }

  if (TxLevel < SIS_MAX_MIPMAP_LEVEL)
    {
      current->texture[hw_unit].hwTextureSet |= (TxLevel << 8);
    }
  else
    {
      /* can i limit capacity */
      hwcx->swRenderFlag |= SIS_SW_TEXTURE_OBJ;
    }

  switch (object->MagFilter)
    {
    case GL_NEAREST:
      current->texture[hw_unit].hwTextureMip |= TEXTURE_FILTER_NEAREST;
      break;

    case GL_LINEAR:
      current->texture[hw_unit].hwTextureMip |= (TEXTURE_FILTER_LINEAR << 3);
      break;
    }

  switch (object->MinFilter)
    {
    case GL_NEAREST:
      current->texture[hw_unit].hwTextureMip |= TEXTURE_FILTER_NEAREST;
      break;

    case GL_LINEAR:
      current->texture[hw_unit].hwTextureMip |= TEXTURE_FILTER_LINEAR;
      break;

    case GL_NEAREST_MIPMAP_NEAREST:
      current->texture[hw_unit].hwTextureMip |=
	TEXTURE_FILTER_NEAREST_MIP_NEAREST;
      break;

    case GL_NEAREST_MIPMAP_LINEAR:
      current->texture[hw_unit].hwTextureMip |=
	TEXTURE_FILTER_NEAREST_MIP_LINEAR;
      break;

    case GL_LINEAR_MIPMAP_NEAREST:
      current->texture[hw_unit].hwTextureMip |=
	TEXTURE_FILTER_LINEAR_MIP_NEAREST;
      break;

    case GL_LINEAR_MIPMAP_LINEAR:
      current->texture[hw_unit].hwTextureMip |=
	TEXTURE_FILTER_LINEAR_MIP_LINEAR;
      break;
    }

  switch (object->WrapS)
    {
    case GL_REPEAT:
      current->texture[hw_unit].hwTextureSet |= MASK_TextureWrapU;
      break;
    case GL_CLAMP:
      current->texture[hw_unit].hwTextureSet |= MASK_TextureClampU;
      break;
    case GL_CLAMP_TO_EDGE:
      /* 
       * ?? not support yet 
       */
      break;
    }

  switch (object->WrapT)
    {
    case GL_REPEAT:
      current->texture[hw_unit].hwTextureSet |= MASK_TextureWrapV;
      break;
    case GL_CLAMP:
      current->texture[hw_unit].hwTextureSet |= MASK_TextureClampV;
      break;
    case GL_CLAMP_TO_EDGE:
      /* 
       * ?? not support yet 
       */
      break;
    }

/*
      if (current->texture[hw_unit].hwTextureSet & MASK_TextureClampU) {
	  current->texture[hw_unit].hwTextureSet &= ~MASK_TextureClampU;
	  current->texture[hw_unit].hwTextureSet |= MASK_TextureBorderU;
	}

      if (current->texture[hw_unit].hwTextureSet & MASK_TextureClampV) {
	  current->texture[hw_unit].hwTextureSet &= ~MASK_TextureClampV;
	  current->texture[hw_unit].hwTextureSet |= MASK_TextureBorderV;
	}
*/
      current->texture[hw_unit].hwTextureBorderColor = 
        ((GLuint) object->BorderColor[3] << 24) + 
        ((GLuint) object->BorderColor[0] << 16) + 
        ((GLuint) object->BorderColor[1] << 8) + 
        ((GLuint) object->BorderColor[2]);

      if (current->texture[hw_unit].hwTextureBorderColor ^
	  prev->texture[hw_unit].hwTextureBorderColor) 
	{
	  prev->texture[hw_unit].hwTextureBorderColor =
	  current->texture[hw_unit].hwTextureBorderColor; 
	  if (hw_unit == 1)
	    hwcx->GlobalFlag |= GFLAG_TEXBORDERCOLOR_1; 
	  else
	    hwcx->GlobalFlag |= GFLAG_TEXBORDERCOLOR;
	}

  current->texture[hw_unit].hwTextureSet |= (image->WidthLog2 << 4);
  current->texture[hw_unit].hwTextureSet |= (image->HeightLog2);

  {
    if (hw_unit == 0)
      hwcx->GlobalFlag |= GFLAG_TEXTUREADDRESS;
    else
      hwcx->GlobalFlag |= GFLAG_TEXTUREADDRESS_1;

    for (i = 0; i < TxLevel + 1; i++)
      {
	SIStextureArea *area = (SIStextureArea *) object->Image[i]->DriverData;
        GLuint texOffset;
	GLuint texPitch = TransferTexturePitch (area->Pitch);

	switch(area->memType){
	case VIDEO_TYPE:
	  texOffset = ((char *) area->Data - (char *) GET_FbBase (hwcx));
	  break;
	case AGP_TYPE:
	  texOffset = ((char *) area->Data - (char *) GET_AGPBase (hwcx)) +
	               (unsigned long) hwcx->AGPAddr;
          current->texture[hw_unit].hwTextureMip |= (0x40000 << i);
          break;
        default:
          assert(0);
        }

	switch (i)
	  {
	  case 0:
	    prev->texture[hw_unit].texOffset0 = texOffset;
	    prev->texture[hw_unit].texPitch01 = texPitch << 16;
	    break;
	  case 1:
	    prev->texture[hw_unit].texOffset1 = texOffset;
	    prev->texture[hw_unit].texPitch01 |= texPitch;
	    break;
	  case 2:
	    prev->texture[hw_unit].texOffset2 = texOffset;
	    prev->texture[hw_unit].texPitch23 = texPitch << 16;
	    break;
	  case 3:
	    prev->texture[hw_unit].texOffset3 = texOffset;
	    prev->texture[hw_unit].texPitch23 |= texPitch;
	    break;
	  case 4:
	    prev->texture[hw_unit].texOffset4 = texOffset;
	    prev->texture[hw_unit].texPitch45 = texPitch << 16;
	    break;
	  case 5:
	    prev->texture[hw_unit].texOffset5 = texOffset;
	    prev->texture[hw_unit].texPitch45 |= texPitch;
	    break;
	  case 6:
	    prev->texture[hw_unit].texOffset6 = texOffset;
	    prev->texture[hw_unit].texPitch67 = texPitch << 16;
	    break;
	  case 7:
	    prev->texture[hw_unit].texOffset7 = texOffset;
	    prev->texture[hw_unit].texPitch67 |= texPitch;
	    break;
	  case 8:
	    prev->texture[hw_unit].texOffset8 = texOffset;
	    prev->texture[hw_unit].texPitch89 = texPitch << 16;
	    break;
	  case 9:
	    prev->texture[hw_unit].texOffset9 = texOffset;
	    prev->texture[hw_unit].texPitch89 |= texPitch;
	    break;
	  case 10:
	    prev->texture[hw_unit].texOffset10 = texOffset;
	    prev->texture[hw_unit].texPitch10 = texPitch << 16;
	    break;
	  case 11:
	    prev->texture[hw_unit].texOffset11 = texOffset;
	    prev->texture[hw_unit].texPitch10 |= texPitch;
	    break;
	  }
      }
  }

  if (current->texture[hw_unit].hwTextureSet ^ 
      prev->texture[hw_unit].hwTextureSet)
    {
      prev->texture[hw_unit].hwTextureSet =
	current->texture[hw_unit].hwTextureSet;
      if (hw_unit == 1)
	hwcx->GlobalFlag |= CFLAG_TEXTURERESET_1;
      else
	hwcx->GlobalFlag |= CFLAG_TEXTURERESET;
    }
  if (current->texture[hw_unit].hwTextureMip ^ 
      prev->texture[hw_unit].hwTextureMip)
    {
      prev->texture[hw_unit].hwTextureMip =
	current->texture[hw_unit].hwTextureMip;
      if (hw_unit == 1)
	hwcx->GlobalFlag |= GFLAG_TEXTUREMIPMAP_1;
      else
	hwcx->GlobalFlag |= GFLAG_TEXTUREMIPMAP;
    }
}

static void
sis_reset_texture_env (GLcontext * ctx, int hw_unit)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  if (hw_unit == 1)
    {
      current->hwTexBlendClr1 = RGB_STAGE1;
      current->hwTexBlendAlpha1 = A_STAGE1;

      if ((current->hwTexBlendClr1 ^ prev->hwTexBlendClr1) ||
	  (current->hwTexBlendAlpha1 ^ prev->hwTexBlendAlpha1) ||
	  (current->hwTexEnvColor ^ prev->hwTexEnvColor))
	{
	  prev->hwTexBlendClr1 = current->hwTexBlendClr1;
	  prev->hwTexBlendAlpha1 = current->hwTexBlendAlpha1;
	  prev->hwTexEnvColor = current->hwTexEnvColor;
	  hwcx->GlobalFlag |= GFLAG_TEXTUREENV_1;
	}
    }
  else
    {
      current->hwTexBlendClr0 = RGB_STAGE1;
      current->hwTexBlendAlpha0 = A_STAGE1;

      if ((current->hwTexBlendClr0 ^ prev->hwTexBlendClr0) ||
	  (current->hwTexBlendAlpha0 ^ prev->hwTexBlendAlpha0) ||
	  (current->hwTexEnvColor ^ prev->hwTexEnvColor))
	{
	  prev->hwTexBlendClr0 = current->hwTexBlendClr0;
	  prev->hwTexBlendAlpha0 = current->hwTexBlendAlpha0;
	  prev->hwTexEnvColor = current->hwTexEnvColor;
	  hwcx->GlobalFlag |= GFLAG_TEXTUREENV;
	}
    }
}

static DWORD
BitScanForward (WORD w)
{
  DWORD i;

  for (i = 0; i < 16; i++)
    {
      if (w & (1 << i))
	break;
    }
  return (i);
}

static DWORD
TransferTexturePitch (DWORD dwPitch)
{
  DWORD dwRet, i;

  i = BitScanForward ((WORD) dwPitch);
  dwRet = dwPitch >> i;
  dwRet |= i << 9;
  return (dwRet);
}
