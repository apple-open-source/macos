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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_mesa.c,v 1.7 2002/10/30 12:52:00 alanh Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"
#include "sis_lock.h"

#include "state.h"

void
sis_RenderStart (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  LOCK_HARDWARE ();

  if (hwcx->Primitive & hwcx->swRenderFlag)
    {
      WaitEngIdle (hwcx);
    }

  if (ctx->Texture.ReallyEnabled)
    {
      sis_validate_texture (ctx);
      if (hwcx->swRenderFlag & SIS_SW_TEXTURE)
        {
          hwcx->swForceRender = GL_TRUE;
          gl_update_state(ctx);
          hwcx->swForceRender = GL_FALSE;
        }
      else
        {
          if (hwcx->GlobalFlag & GFLAG_TEXTURE_STATES)
            sis_update_texture_state (hwcx);
        }
    }

  if (hwcx->GlobalFlag & GFLAG_RENDER_STATES)
    {
      sis_update_render_state (hwcx, 0);
    }

  if (hwcx->UseAGPCmdMode)
    {
      sis_StartAGP (ctx);
    }
  
#if defined(SIS_DUMP)
   d2f_once (ctx);
#endif
}

void
sis_RenderFinish (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  if (hwcx->UseAGPCmdMode)
    {
      sis_FlushAGP (ctx);
    }

  UNLOCK_HARDWARE ();
}

void
sis_ReducedPrimitiveChange (GLcontext * ctx, GLenum primitive)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  
  /* it is possible several primitive types per VB */
  if (hwcx->UseAGPCmdMode)
    {
      sis_FlushAGP (ctx);
      sis_StartAGP (ctx);
    }
  else
    {
      mEndPrimitive();
      
      /* 
       * do nothing now because each hw's primitive-type will be set 
       * per primitive 
       */
      
      /* TODO: if above rule changes, remember to modify */
    }

  hwcx->AGPParseSet &= ~0xf;
  switch (primitive)
    {
    case GL_POINT:
    case GL_POINTS:
      hwcx->Primitive = SIS_SW_POINT;
      hwcx->AGPParseSet |= 0x0;
      break;
    case GL_LINE:
    case GL_LINES:
      hwcx->Primitive = SIS_SW_LINE;
      hwcx->AGPParseSet |= 0x4;
      break;
    case GL_POLYGON:
      hwcx->Primitive = SIS_SW_TRIANGLE;
      hwcx->AGPParseSet |= 0x8;
      break;
    }
}

void
sis_init_driver (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  ctx->Driver.UpdateState = sis_UpdateState;

  ctx->Driver.RenderStart = sis_RenderStart;
  ctx->Driver.RenderFinish = sis_RenderFinish;
  ctx->Driver.ReducedPrimitiveChange = sis_ReducedPrimitiveChange;
  ctx->Driver.Finish = sis_Finish;
  ctx->Driver.Flush = sis_Flush;

  ctx->Driver.AlphaFunc = sis_AlphaFunc;
  ctx->Driver.BlendEquation = sis_BlendEquation;
  ctx->Driver.BlendFunc = sis_BlendFunc;
  ctx->Driver.BlendFuncSeparate = sis_BlendFuncSeparate;
  ctx->Driver.ClearDepth = sis_ClearDepth;
  ctx->Driver.CullFace = sis_CullFace;
  ctx->Driver.FrontFace = sis_FrontFace;
  ctx->Driver.DepthFunc = sis_DepthFunc;
  ctx->Driver.DepthMask = sis_DepthMask;
  ctx->Driver.DepthRange = sis_DepthRange;
  ctx->Driver.Enable = sis_Enable;
  ctx->Driver.Fogfv = sis_Fogfv;
  ctx->Driver.Hint = sis_Hint;
  ctx->Driver.Lightfv = sis_Lightfv;
  ctx->Driver.LightModelfv = sis_LightModelfv;
  ctx->Driver.PolygonMode = sis_PolygonMode;
  ctx->Driver.Scissor = sis_Scissor;
  ctx->Driver.ShadeModel = sis_ShadeModel;
  ctx->Driver.ClearStencil = sis_ClearStencil;
  ctx->Driver.StencilFunc = sis_StencilFunc;
  ctx->Driver.StencilMask = sis_StencilMask;
  ctx->Driver.StencilOp = sis_StencilOp;
  ctx->Driver.Viewport = sis_Viewport;

  ctx->Driver.Clear = sis_Clear;

  ctx->Driver.TexEnv = sis_TexEnv;
  ctx->Driver.TexImage = sis_TexImage;
  ctx->Driver.TexSubImage = sis_TexSubImage;
  ctx->Driver.TexParameter = sis_TexParameter;
  ctx->Driver.BindTexture = sis_BindTexture;
  ctx->Driver.DeleteTexture = sis_DeleteTexture;
  ctx->Driver.UpdateTexturePalette = sis_UpdateTexturePalette;
  ctx->Driver.ActiveTexture = sis_ActiveTexture;
  ctx->Driver.IsTextureResident = sis_IsTextureResident;
  ctx->Driver.PrioritizeTexture = sis_PrioritizeTexture;

  ctx->Driver.ClearColor = sis_ClearColor;
  ctx->Driver.SetDrawBuffer = sis_SetDrawBuffer;
  ctx->Driver.SetReadBuffer = sis_SetReadBuffer;
  ctx->Driver.GetBufferSize = sis_GetBufferSize;
  ctx->Driver.GetString = sis_GetString;
  ctx->Driver.ColorMask = sis_ColorMask;
  ctx->Driver.LogicOp = sis_LogicOp;
  ctx->Driver.Dither = sis_Dither;
  ctx->Driver.GetParameteri = sis_GetParameteri;
  ctx->Driver.DrawPixels = sis_DrawPixels;
  ctx->Driver.Bitmap = sis_Bitmap;
  
  /* Optimization */
#ifdef NOT_DONE
  ctx->Driver.RasterSetup = sis_ChooseRasterSetupFunc(ctx);
  ctx->Driver.RegisterVB = sis_RegisterVB;
  ctx->Driver.UnregisterVB = sis_UnregisterVB;
  ctx->Driver.ResetVB = sis_ResetVB;
  ctx->Driver.ResetCvaVB = sis_ResetCvaVB;
#endif

 /* Fast Path */
 ctx->Driver.RegisterPipelineStages = sis_RegisterPipelineStages;

 /* driver-specific */
 hwcx->SwapBuffers = sis_swap_buffers;

#ifdef SIS_USE_HW_CULL
  /* set capability flag */
  ctx->Driver.TriangleCaps = DD_TRI_CULL;
#endif
}

void
sis_UpdateState (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  sis_set_render_func (ctx);

  /* ?? duplicate in sis_Enable */
  if (ctx->NewState & NEW_FOG)
    {
      if (ctx->RasterMask & FOG_BIT)
	current->hwCapEnable |= MASK_FogEnable;
      else
	current->hwCapEnable &= ~MASK_FogEnable;
    }

  /* TODO: mesa->NewState. In sis_Enable? */
  if (ctx->RasterMask & STENCIL_BIT)
    {
      current->hwCapEnable |=
	(MASK_StencilTestEnable | MASK_StencilWriteEnable);
    }
  else
    {
      current->hwCapEnable &=
	~(MASK_StencilTestEnable | MASK_StencilWriteEnable);
    }

  /* NEW_TEXTURE_ENABLE depends on glEnable() instead of ReallyEnabled */
  /* if (ctx->NewState & NEW_TEXTURE_ENABLE) */
  if(1)
    {
      if (ctx->Texture.ReallyEnabled &
	  (TEXTURE0_1D | TEXTURE0_2D | TEXTURE1_1D | TEXTURE1_2D))
	{
	  current->hwCapEnable |= MASK_TextureEnable;

	  current->hwCapEnable &= ~MASK_TextureNumUsed;
	  if (ctx->Texture.ReallyEnabled & TEXTURE1_ANY)
	    current->hwCapEnable |= 0x00002000;
	  else
	    current->hwCapEnable |= 0x00001000;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_TextureEnable;
	}

#if 1
    /* TODO : if unmark these, error in multitexture */
    if(ctx->NewState & NEW_TEXTURE_ENABLE)
      {
	int i;
	for (i = 0; i < SIS_MAX_TEXTURES; i++)
	  {
	    hwcx->TexStates[i] |= (NEW_TEXTURING | NEW_TEXTURE_ENV);
	  }
      }
#endif
    }

  /* enable setting 1 */
  if (current->hwCapEnable ^ prev->hwCapEnable)
    {
      prev->hwCapEnable = current->hwCapEnable;
      hwcx->GlobalFlag |= GFLAG_ENABLESETTING;
    }

  /* enable setting 2 */
  if (current->hwCapEnable2 ^ prev->hwCapEnable2)
    {
      prev->hwCapEnable2 = current->hwCapEnable2;
      hwcx->GlobalFlag |= GFLAG_ENABLESETTING2;
    }

  /* TODO: if fog disable, don't check */
  if (current->hwCapEnable & MASK_FogEnable)
    {
      /* fog setting */
      if (current->hwFog ^ prev->hwFog)
	{
	  prev->hwFog = current->hwFog;
	  hwcx->GlobalFlag |= GFLAG_FOGSETTING;
	}
      if (current->hwFogFar ^ prev->hwFogFar)
	{
	  prev->hwFogFar = current->hwFogFar;
	  hwcx->GlobalFlag |= GFLAG_FOGSETTING;
	}
      if (current->hwFogInverse ^ prev->hwFogInverse)
	{
	  prev->hwFogInverse = current->hwFogInverse;
	  hwcx->GlobalFlag |= GFLAG_FOGSETTING;
	}
      if (current->hwFogDensity ^ prev->hwFogDensity)
	{
	  prev->hwFogDensity = current->hwFogDensity;
	  hwcx->GlobalFlag |= GFLAG_FOGSETTING;
	}
    }

#ifdef NOT_DONE
  sis_set_render_vb_tabs(ctx);
#endif

  /* TODO: assume when isFullScreen/DrawBuffer changed, UpdateState 
   *       will be called 
   */
#if SIS_STEREO
  if(hwcx->isFullScreen && 
     (ctx->Color.DriverDrawBuffer == GL_BACK_LEFT) &&
     hwcx->useStereo)
  {
    if(!hwcx->stereoEnabled){
      sis_init_stereo(ctx);
    }
  }
  else{
    if(hwcx->stereoEnabled){
      sis_final_stereo(ctx);
    }    
  }
#endif
  
  /* TODO : 1. where to handle SIS_SW_TEXTURE?
   *        2. sw<->hw
   *        3. sw-render only if next primitve need to do
   */
}

void
sis_set_buffer_static (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  GLvisual *visual = ctx->Visual;

  switch (hwcx->bytesPerPixel)
    {
    case 2:
      /* TODO: don't check
      if (hwcx->redMask == 0xf800 && hwcx->greenMask == 0x07e0 &&
          hwcx->blueMask == 0x001f)
      */
      {
	current->hwDstSet |= DST_FORMAT_RGB_565;
      }
      break;
    case 4:
      /*
      if (hwcx->redMask == 0xff0000 && hwcx->greenMask == 0xff00 &&
          hwcx->blueMask == 0xff)
      */
      {
	switch (visual->AlphaBits)
	  {
	  case 0:
	  case 1:
	  case 2:
	  case 4:
	  case 8:
	    /* TODO */
	    current->hwDstSet |= DST_FORMAT_ARGB_8888;
	    break;
	  }
      }
      break;
    default:
      assert (0);
    }

  switch (visual->DepthBits)
    {
    case 0:
      current->hwCapEnable &= ~MASK_ZWriteEnable;
    case 16:
      hwcx->zFormat = Z_16;
      current->hwCapEnable |= MASK_ZWriteEnable;
      break;
    case 32:
      hwcx->zFormat = Z_32;
      current->hwCapEnable |= MASK_ZWriteEnable;
      break;
    case 24:
      assert (visual->StencilBits);
      hwcx->zFormat = S_8_Z_24;
      current->hwCapEnable |= MASK_StencilBufferEnable;
      current->hwCapEnable |= MASK_ZWriteEnable;
      break;
    }

  current->hwZ &= ~MASK_ZBufferFormat;
  current->hwZ |= hwcx->zFormat;

  /* Destination Color Format */
  if (current->hwDstSet ^ prev->hwDstSet)
    {
      prev->hwDstSet = current->hwDstSet;
      hwcx->GlobalFlag |= GFLAG_DESTSETTING;
    }

  /* Z Buffer Data Format */
  if (current->hwZ ^ prev->hwZ)
    {
      prev->hwZ = current->hwZ;
      hwcx->GlobalFlag |= GFLAG_ZSETTING;
    }

  sis_sw_set_zfuncs_static (ctx);
}

void
sis_Finish (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  sis_Flush (ctx);

  WaitEngIdle (hwcx);
}

void
sis_Flush (GLcontext * ctx)
{
  /* do nothing now */
}

void
sis_AlphaFunc (GLcontext * ctx, GLenum func, GLclampf ref)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  /* TODO: ref is type GLclampf, but mesa has scaled it to 0 - 255.0 */
  current->hwAlpha = ((unsigned char) ref) << 16;

  /* Alpha Test function */
  current->hwAlpha &= ~0x07000000;
  switch (func)
    {
    case GL_NEVER:
      current->hwAlpha |= SiS_ALPHA_NEVER;
      break;
    case GL_LESS:
      current->hwAlpha |= SiS_ALPHA_LESS;
      break;
    case GL_EQUAL:
      current->hwAlpha |= SiS_ALPHA_EQUAL;
      break;
    case GL_LEQUAL:
      current->hwAlpha |= SiS_ALPHA_LEQUAL;
      break;
    case GL_GREATER:
      current->hwAlpha |= SiS_ALPHA_GREATER;
      break;
    case GL_NOTEQUAL:
      current->hwAlpha |= SiS_ALPHA_NOTEQUAL;
      break;
    case GL_GEQUAL:
      current->hwAlpha |= SiS_ALPHA_GEQUAL;
      break;
    case GL_ALWAYS:
      current->hwAlpha |= SiS_ALPHA_ALWAYS;
      break;
    }

  prev->hwAlpha = current->hwAlpha;
  hwcx->GlobalFlag |= GFLAG_ALPHASETTING;
}

void
sis_BlendEquation (GLcontext * ctx, GLenum mode)
{
  /* version 1.2 specific */
  /* TODO: 300 don't support, fall back? */
}

void
sis_BlendFunc (GLcontext * ctx, GLenum sfactor, GLenum dfactor)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  /* TODO: in ICD, if no blend, it will reset these value */
  /* blending enable */
  current->hwDstSrcBlend = 0x10000;	/* Default destination alpha */

  switch (dfactor)
    {
    case GL_ZERO:
      current->hwDstSrcBlend |= SiS_D_ZERO;
      break;
    case GL_ONE:
      current->hwDstSrcBlend |= SiS_D_ONE;
      break;
    case GL_SRC_COLOR:
      current->hwDstSrcBlend |= SiS_D_SRC_COLOR;
      break;
    case GL_ONE_MINUS_SRC_COLOR:
      current->hwDstSrcBlend |= SiS_D_ONE_MINUS_SRC_COLOR;
      break;
    case GL_SRC_ALPHA:
      current->hwDstSrcBlend |= SiS_D_SRC_ALPHA;
      break;
    case GL_ONE_MINUS_SRC_ALPHA:
      current->hwDstSrcBlend |= SiS_D_ONE_MINUS_SRC_ALPHA;
      break;
    case GL_DST_ALPHA:
      current->hwDstSrcBlend |= SiS_D_DST_ALPHA;
      break;
    case GL_ONE_MINUS_DST_ALPHA:
      current->hwDstSrcBlend |= SiS_D_ONE_MINUS_DST_ALPHA;
      break;
    }

  switch (sfactor)
    {
    case GL_ZERO:
      current->hwDstSrcBlend |= SiS_S_ZERO;
      break;
    case GL_ONE:
      current->hwDstSrcBlend |= SiS_S_ONE;
      break;
    case GL_SRC_ALPHA:
      current->hwDstSrcBlend |= SiS_S_SRC_ALPHA;
      break;
    case GL_ONE_MINUS_SRC_ALPHA:
      current->hwDstSrcBlend |= SiS_S_ONE_MINUS_SRC_ALPHA;
      break;
    case GL_DST_ALPHA:
      current->hwDstSrcBlend |= SiS_S_DST_ALPHA;
      break;
    case GL_ONE_MINUS_DST_ALPHA:
      current->hwDstSrcBlend |= SiS_S_ONE_MINUS_DST_ALPHA;
      break;
    case GL_DST_COLOR:
      current->hwDstSrcBlend |= SiS_S_DST_COLOR;
      break;
    case GL_ONE_MINUS_DST_COLOR:
      current->hwDstSrcBlend |= SiS_S_ONE_MINUS_DST_COLOR;
      break;
    case GL_SRC_ALPHA_SATURATE:
      current->hwDstSrcBlend |= SiS_S_SRC_ALPHA_SATURATE;
      break;
    }

  if (current->hwDstSrcBlend ^ prev->hwDstSrcBlend)
    {
      prev->hwDstSrcBlend = current->hwDstSrcBlend;
      hwcx->GlobalFlag |= GFLAG_DSTBLEND;
    }
}

void
sis_BlendFuncSeparate (GLcontext * ctx, GLenum sfactorRGB,
		       GLenum dfactorRGB, GLenum sfactorA, GLenum dfactorA)
{
}

void
sis_CullFace (GLcontext * ctx, GLenum mode)
{
#ifdef SIS_USE_HW_CULL
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  hwcx->AGPParseSet &= ~0x00008000;
  hwcx->dwPrimitiveSet &= ~0x08000000;
  
  /* TODO : GL_FRONT_AND_BACK must be handled elsewhere */
  if((mode == GL_FRONT && ctx->Polygon.FrontFace == GL_CCW) ||
     (mode == GL_BACK && ctx->Polygon.FrontFace == GL_CW))
    {
      hwcx->AGPParseSet |= 0x00008000;
      hwcx->dwPrimitiveSet |= 0x08000000;
    }
#endif
}

void
sis_FrontFace (GLcontext * ctx, GLenum mode)
{
#ifdef SIS_USE_HW_CULL
  sis_CullFace (ctx, ctx->Polygon.CullFaceMode);
#endif
}

void
sis_DepthFunc (GLcontext * ctx, GLenum func)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  GLuint hwZFunc = 0;

  switch (func)
    {
    case GL_LESS:
      hwZFunc = SiS_Z_COMP_S_LT_B;
      break;
    case GL_GEQUAL:
      hwZFunc = SiS_Z_COMP_S_GE_B;
      break;
    case GL_LEQUAL:
      hwZFunc = SiS_Z_COMP_S_LE_B;
      break;
    case GL_GREATER:
      hwZFunc = SiS_Z_COMP_S_GT_B;
      break;
    case GL_NOTEQUAL:
      hwZFunc = SiS_Z_COMP_S_NE_B;
      break;
    case GL_EQUAL:
      hwZFunc = SiS_Z_COMP_S_EQ_B;
      break;
    case GL_ALWAYS:
      hwZFunc = SiS_Z_COMP_ALWAYS;
      break;
    case GL_NEVER:
      hwZFunc = SiS_Z_COMP_NEVER;
      break;
    }
  current->hwZ &= ~MASK_ZTestMode;
  current->hwZ |= hwZFunc;

  if (current->hwZ ^ prev->hwZ)
    {
      prev->hwZ = current->hwZ;

      hwcx->GlobalFlag |= GFLAG_ZSETTING;
    }
}

void
sis_DepthMask (GLcontext * ctx, GLboolean flag)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  if (ctx->Visual->StencilBits)
    {
      if (flag || ctx->Stencil.WriteMask)
	{
	  current->hwCapEnable |= MASK_ZWriteEnable;
	  if (flag && ctx->Stencil.WriteMask == 0xff)
	    {
	      current->hwCapEnable2 &= ~MASK_ZMaskWriteEnable;
	    }
	  else
	    {
	      current->hwCapEnable2 |= MASK_ZMaskWriteEnable;
	      current->hwZMask = ((DWORD) ctx->Stencil.WriteMask << 24) |
		((flag) ? 0x00ffffff : 0);

	      if (current->hwZMask ^ prev->hwZMask)
		{
		  prev->hwZMask = current->hwZMask;
		  hwcx->GlobalFlag |= GFLAG_ZSETTING;
		}
	    }
	}
      else
	{
	  current->hwCapEnable &= ~MASK_ZWriteEnable;
	}
    }
  else
    {
      if (flag)
	{
	  current->hwCapEnable |= MASK_ZWriteEnable;
	  current->hwCapEnable2 &= ~MASK_ZMaskWriteEnable;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_ZWriteEnable;
	}
    }
}

void
sis_DepthRange (GLcontext * ctx, GLclampd nearval, GLclampd farval)
{
  /* mesa handles it? */
}

void
sis_Enable (GLcontext * ctx, GLenum cap, GLboolean state)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *current = &hwcx->current;

  switch (cap)
    {
    case GL_ALPHA_TEST:
      if (state)
	{
	  current->hwCapEnable |= MASK_AlphaTestEnable;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_AlphaTestEnable;
	}
      break;
/*
    case GL_AUTO_NORMAL:
      break;
*/
    case GL_BLEND:
      /* TODO: */
      if(state)
      /* if (state & !ctx->Color.ColorLogicOpEnabled) */
	{
	  current->hwCapEnable |= MASK_BlendEnable;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_BlendEnable;
	}
      break;
/*
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
      break;
    case GL_COLOR_MATERIAL:
      break;
*/
    case GL_CULL_FACE:
#ifdef SIS_USE_HW_CULL
      if (state)
	{
	  current->hwCapEnable |= MASK_CullEnable;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_CullEnable;
	}
#endif
      break;
    case GL_DEPTH_TEST:
      if (state && xmesa->xm_buffer->depthbuffer)
	{
	  current->hwCapEnable |= MASK_ZTestEnable;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_ZTestEnable;
	}
      break;
    case GL_DITHER:
      if (state)
	{
	  current->hwCapEnable |= MASK_DitherEnable;
	}
      else
	{
	  current->hwCapEnable &= ~MASK_DitherEnable;
	}
      break;
/*
    case GL_FOG:
      break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
      break;
    case GL_LIGHTING:
      break;
    case GL_LINE_SMOOTH:
      break;
    case GL_LINE_STIPPLE:
      break;
    case GL_INDEX_LOGIC_OP:
      break;
*/
    case GL_COLOR_LOGIC_OP:
      if (state)
	{
	  sis_LogicOp (ctx, ctx->Color.LogicOp);
	}
      else
	{
	  sis_LogicOp (ctx, GL_COPY);
	}
      break;
/*
    case GL_MAP1_COLOR_4:
      break;
    case GL_MAP1_INDEX:
      break;
    case GL_MAP1_NORMAL:
      break;
    case GL_MAP1_TEXTURE_COORD_1:
      break;
    case GL_MAP1_TEXTURE_COORD_2:
      break;
    case GL_MAP1_TEXTURE_COORD_3:
      break;
    case GL_MAP1_TEXTURE_COORD_4:
      break;
    case GL_MAP1_VERTEX_3:
      break;
    case GL_MAP1_VERTEX_4:
      break;
    case GL_MAP2_COLOR_4:
      break;
    case GL_MAP2_INDEX:
      break;
    case GL_MAP2_NORMAL:
      break;
    case GL_MAP2_TEXTURE_COORD_1:
      break;
    case GL_MAP2_TEXTURE_COORD_2:
      break;
    case GL_MAP2_TEXTURE_COORD_3:
      break;
    case GL_MAP2_TEXTURE_COORD_4:
      break;
    case GL_MAP2_VERTEX_3:
      break;
    case GL_MAP2_VERTEX_4:
      break;
    case GL_NORMALIZE:
      break;
    case GL_POINT_SMOOTH:
      break;
    case GL_POLYGON_SMOOTH:
      break;
    case GL_POLYGON_STIPPLE:
      break;
    case GL_POLYGON_OFFSET_POINT:
      break;
    case GL_POLYGON_OFFSET_LINE:
      break;
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_EXT:
     break;
    case GL_RESCALE_NORMAL_EXT:
      break;
*/
    case GL_SCISSOR_TEST:
      sis_set_scissor (ctx);
      break;
/*
    case GL_SHARED_TEXTURE_PALETTE_EXT:
      break;
*/
    case GL_STENCIL_TEST:
      if (state)
	{
	  current->hwCapEnable |= (MASK_StencilTestEnable |
				   MASK_StencilWriteEnable);
	}
      else
	{
	  current->hwCapEnable &= ~(MASK_StencilTestEnable |
				    MASK_StencilWriteEnable);
	}
      break;
/*
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
    case GL_TEXTURE_3D:
      break;
    case GL_TEXTURE_GEN_Q:
      break;
    case GL_TEXTURE_GEN_R:
      break;
    case GL_TEXTURE_GEN_S:
      break;
    case GL_TEXTURE_GEN_T:
      break;

    case GL_VERTEX_ARRAY:
      break;
    case GL_NORMAL_ARRAY:
      break;
    case GL_COLOR_ARRAY:
      break;
    case GL_INDEX_ARRAY:
      break;
    case GL_TEXTURE_COORD_ARRAY:
      break;
    case GL_EDGE_FLAG_ARRAY:
      break;
*/
    }
}

void
sis_Hint (GLcontext * ctx, GLenum target, GLenum mode)
{
#if 0
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  __GLSiSHardware *current = &hwcx->current;
#endif

  switch (target)
    {
    case GL_FOG_HINT:
      switch (mode)
	{
	case GL_DONT_CARE:
	  break;
	case GL_FASTEST:
	  /* vertex fog */
	  /* TODO */
	  break;
	case GL_NICEST:
	  /* pixel fog */
	  break;
	}
      break;
    case GL_LINE_SMOOTH_HINT:
      break;
    case GL_PERSPECTIVE_CORRECTION_HINT:
#if 0
      if (mode == GL_NICEST)
	current->hwCapEnable |= MASK_TexturePerspectiveEnable;
      else if (!(ctx->RasterMask & FOG_BIT))
	current->hwCapEnable &= ~MASK_TexturePerspectiveEnable;
#endif
      break;
    case GL_POINT_SMOOTH_HINT:
      break;
    case GL_POLYGON_SMOOTH_HINT:
      break;
    }
}

void
sis_Lightfv (GLcontext * ctx, GLenum light,
	     GLenum pname, const GLfloat * params, GLint nparams)
{
}

void
sis_LightModelfv (GLcontext * ctx, GLenum pname, const GLfloat * params)
{
}

void
sis_PolygonMode (GLcontext * ctx, GLenum face, GLenum mode)
{
}

void
sis_Scissor (GLcontext * ctx, GLint x, GLint y, GLsizei w, GLsizei h)
{
  if (ctx->Scissor.Enabled)
    sis_set_scissor (ctx);
}

void
sis_ShadeModel (GLcontext * ctx, GLenum mode)
{
}

void
sis_Viewport (GLcontext * ctx, GLint x, GLint y, GLsizei w, GLsizei h)
{
}

void
sis_ClearColor (GLcontext * ctx, GLubyte red, GLubyte green,
		GLubyte blue, GLubyte alpha)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  set_color_pattern (hwcx, red, green, blue, alpha);

}

/* TODO */
void
sis_ClearIndex (GLcontext * ctx, GLuint index)
{
}

void sis_set_render_pos(GLcontext * ctx, GLubyte *base, GLuint pitch)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  assert (base != NULL);

  if (SIS_VERBOSE&VERBOSE_SIS_BUFFER){
    fprintf(stderr, "set drawing position: base=%lx, pitch=%lu\n", 
            (long)base, (long)pitch);
  }

  /* software render */
  hwcx->swRenderBase = base;
  hwcx->swRenderPitch = pitch;

  current->hwOffsetDest = ((DWORD) base - (DWORD) GET_FbBase (hwcx)) >> 1;
  current->hwDstSet &= ~MASK_DstBufferPitch;
  current->hwDstSet |= pitch >> 2;

  /* destination setting */
  if (current->hwDstSet ^ prev->hwDstSet)
    {
      prev->hwDstSet = current->hwDstSet;
      hwcx->GlobalFlag |= GFLAG_DESTSETTING;
    }

  if (current->hwOffsetDest ^ prev->hwOffsetDest)
    {
      prev->hwOffsetDest = current->hwOffsetDest;
      hwcx->GlobalFlag |= GFLAG_DESTSETTING;
    }
}

GLboolean
sis_SetDrawBuffer (GLcontext * ctx, GLenum mode)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;
  GLubyte *base;
  GLuint pitch;
  GLboolean retval = GL_FALSE;

  /* if new context bind, the function will be call? */
  switch (mode)
    {
    case GL_BACK_LEFT:	  
      base = (GLubyte *) xm_buffer->backimage->data;
      pitch = xm_buffer->backimage->bytes_per_line;
            
      retval = GL_TRUE;
      break;
    case GL_FRONT_LEFT:
      base = sis_get_drawable_pos (xmesa);
      pitch = GET_PITCH (hwcx);
      retval = GL_TRUE;
      break;
    case GL_BACK_RIGHT:
    case GL_FRONT_RIGHT:
    default:
      assert (0);
      return GL_FALSE;
    }

  sis_set_render_pos(ctx, base, pitch);
  
  return retval;
}

void
sis_SetReadBuffer (GLcontext *ctx, GLframebuffer *colorBuffer,
                        GLenum buffer)
{
  /* TODO */
}                        

GLboolean
sis_ColorMask (GLcontext * ctx,
	       GLboolean rmask, GLboolean gmask,
	       GLboolean bmask, GLboolean amask)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;


  if (rmask & gmask & bmask &
      (!ctx->Visual->AlphaBits | amask))
    {
      current->hwCapEnable2 &= ~(MASK_AlphaMaskWriteEnable |
				 MASK_ColorMaskWriteEnable);
    }
  else
    {
      current->hwCapEnable2 |= (MASK_AlphaMaskWriteEnable |
				MASK_ColorMaskWriteEnable);

      current->hwDstMask = (rmask) ? GET_RMASK (hwcx) : 0 |
	                   (gmask) ? GET_GMASK (hwcx) : 0 |
	                   (bmask) ? GET_BMASK (hwcx) : 0 | 
	                   (amask) ? GET_AMASK (hwcx) : 0;
    }

  if (current->hwDstMask ^ prev->hwDstMask)
    {
      prev->hwDstMask = current->hwDstMask;
      hwcx->GlobalFlag |= GFLAG_DESTSETTING;
    }

  return GL_TRUE;
}

GLboolean
sis_LogicOp (GLcontext * ctx, GLenum op)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  if (ctx->Color.ColorLogicOpEnabled)
    {
      current->hwDstSet &= ~MASK_ROP2;
      switch (op)
	{
	case GL_CLEAR:
	  current->hwDstSet |= LOP_CLEAR;
	  break;
	case GL_SET:
	  current->hwDstSet |= LOP_SET;
	  break;
	case GL_COPY:
	  current->hwDstSet |= LOP_COPY;
	  break;
	case GL_COPY_INVERTED:
	  current->hwDstSet |= LOP_COPY_INVERTED;
	  break;
	case GL_NOOP:
	  current->hwDstSet |= LOP_NOOP;
	  break;
	case GL_INVERT:
	  current->hwDstSet |= LOP_INVERT;
	  break;
	case GL_AND:
	  current->hwDstSet |= LOP_AND;
	  break;
	case GL_NAND:
	  current->hwDstSet |= LOP_NAND;
	  break;
	case GL_OR:
	  current->hwDstSet |= LOP_OR;
	  break;
	case GL_NOR:
	  current->hwDstSet |= LOP_NOR;
	  break;
	case GL_XOR:
	  current->hwDstSet |= LOP_XOR;
	  break;
	case GL_EQUIV:
	  current->hwDstSet |= LOP_EQUIV;
	  break;
	case GL_AND_REVERSE:
	  current->hwDstSet |= LOP_AND_REVERSE;
	  break;
	case GL_AND_INVERTED:
	  current->hwDstSet |= LOP_AND_INVERTED;
	  break;
	case GL_OR_REVERSE:
	  current->hwDstSet |= LOP_OR_REVERSE;
	  break;
	case GL_OR_INVERTED:
	  current->hwDstSet |= LOP_OR_INVERTED;
	  break;
	}

      if (current->hwDstSet ^ prev->hwDstSet)
	{
	  prev->hwDstSet = current->hwDstSet;
	  hwcx->GlobalFlag |= GFLAG_DESTSETTING;
	}
    }
  return GL_TRUE;
}

void
sis_Dither (GLcontext * ctx, GLboolean enable)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *current = &hwcx->current;

  if (enable)
    {
      current->hwCapEnable |= MASK_DitherEnable;
    }
  else
    {
      current->hwCapEnable &= ~MASK_DitherEnable;
    }
}

GLint
sis_GetParameteri (const GLcontext * ctx, GLint param)
{
  switch (param)
    {
    case DD_HAVE_HARDWARE_FOG:
      /* 
       * fragment fogging is not free when compared with hardware 
       * vertex fogging because of the overhead of W
       */
      return 0;
/*
    case DD_MAX_TEXTURE_SIZE:
      return SIS_MAX_TEXTURE_SIZE;
    case DD_MAX_TEXTURES:
      return SIS_MAX_TEXTURES;
*/
    }
  return 0;
}

/* just sync between CPU and rendering-engine */
GLboolean sis_DrawPixels (GLcontext * ctx,
			  GLint x, GLint y, GLsizei width, GLsizei height,
			  GLenum format, GLenum type,
			  const struct gl_pixelstore_attrib *unpack,
			  const GLvoid * pixels)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  WaitEngIdle (hwcx);
  return 0;
}

GLboolean sis_Bitmap (GLcontext * ctx,
		      GLint x, GLint y, GLsizei width, GLsizei height,
		      const struct gl_pixelstore_attrib *unpack,
		      const GLubyte * bitmap)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  ctx->Driver.RenderStart( ctx );  
  
  WaitEngIdle (hwcx);
  /* TODO: use tdfx's Bitmap */

  ctx->Driver.RenderFinish( ctx );

  return 0;		      
}

void
set_color_pattern (__GLSiScontext * hwcx, GLubyte red, GLubyte green,
		   GLubyte blue, GLubyte alpha)
{
  /* XXX only RGB565 and ARGB8888 */
  switch (GET_ColorFormat (hwcx))
    {
    case DST_FORMAT_ARGB_8888:
      hwcx->clearColorPattern = (alpha << 24) +
	(red << 16) + (green << 8) + (blue);
      break;
    case DST_FORMAT_RGB_565:
      hwcx->clearColorPattern = ((red >> 3) << 11) +
	((green >> 2) << 5) + (blue >> 3);
      hwcx->clearColorPattern |= hwcx->clearColorPattern << 16;
      break;
    default:
      assert (0);
    }
}

void
set_z_stencil_pattern (__GLSiScontext * hwcx, GLclampd z, int stencil)
{
  GLuint zPattern, stencilPattern;
  GLboolean dword_pattern;

  if (z <= (float) 0.0)
    zPattern = 0x0;
  else if (z >= (float) 1.0)
    zPattern = 0xFFFFFFFF;
  else
    zPattern = doFPtoFixedNoRound (*(DWORD *) & z, 32);

  stencilPattern = 0;

  switch (hwcx->zFormat)
    {
    case Z_16:
      zPattern = zPattern >> 16;
      zPattern &= 0x0000FFFF;
      dword_pattern = GL_FALSE;
      break;
    case S_8_Z_24:
      zPattern = zPattern >> 8;
      zPattern &= 0x00FFFFFF;
      stencilPattern = (stencilPattern << 24);
      dword_pattern = GL_TRUE;
      break;
    case Z_32:
      dword_pattern = GL_TRUE;
      break;
    default:
      assert (0);
    }
  hwcx->clearZStencilPattern = zPattern | stencilPattern;
  /* ?? */
  if (!dword_pattern)
    hwcx->clearZStencilPattern |= (zPattern | stencilPattern) << 16;
}

void 
sis_update_drawable_state (GLcontext * ctx)
{
  const XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *current = &hwcx->current;
  __GLSiSHardware *prev = &hwcx->prev;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;
  GLuint z_depth;  

  sis_SetDrawBuffer (ctx, ctx->Color.DriverDrawBuffer);
  /* TODO: call sis_SetReadBuffer? */

  switch (hwcx->zFormat)
    {
      case Z_16:
        z_depth = 2;
	break;
      case Z_32:
      case S_8_Z_24:
	z_depth = 4;
	break;
      default:
	assert (0);
    }

  current->hwZ &= ~MASK_ZBufferPitch;
  current->hwZ |= xm_buffer->width * z_depth >> 2;
  /* TODO, in xfree 3.9.18, no ctx->Buffer */
  current->hwOffsetZ = ((DWORD) (xm_buffer->depthbuffer) -
		       (DWORD) GET_FbBase (hwcx)) >> 2;

  if ((current->hwOffsetZ ^ prev->hwOffsetZ)
      || (current->hwZ ^ prev->hwZ))
    {
      prev->hwOffsetZ = current->hwOffsetZ;
      prev->hwZ = current->hwZ;

      /* Z setting */
      hwcx->GlobalFlag |= GFLAG_ZSETTING;
    }
}

void
sis_GetBufferSize (GLframebuffer *buffer, GLuint * width, GLuint * height)
{
  GET_CURRENT_CONTEXT(ctx);
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *current = &hwcx->current;
  __GLSiSHardware *prev = &hwcx->prev;

  GLuint winwidth, winheight;
  XMesaBuffer xm_buffer = xmesa->xm_buffer;

#ifndef XFree86Server
  SIS_VALIDATE_DRAWABLE_INFO (xmesa->display, 
                              xmesa->driContextPriv->driScreenPriv, 
                              xmesa->driContextPriv->driDrawablePriv);
#endif
  sis_get_drawable_size (xmesa, &winwidth, &winheight);

  *width = winwidth;
  *height = winheight;

  if (winwidth != xm_buffer->width || winheight != xm_buffer->height)
    {
      GLuint z_depth;

      if (SIS_VERBOSE&VERBOSE_SIS_BUFFER)
      {
        fprintf(stderr, "GetBufferSize: before=(%d,%d), after=(%d,%d)\n",
                xm_buffer->width, xm_buffer->height, winwidth, winheight);
      }
      
      xm_buffer->width = winwidth;
      xm_buffer->height = winheight;
      
      /* update hwcx->isFullScreen */
      /* TODO: Does X-server have exclusive-mode? */      
      /* TODO: physical screen width/height will be changed dynamicly */

#if SIS_STEREO
      if((hwcx->virtualX == winwidth) && (hwcx->virtualY == winheight))
        hwcx->isFullScreen = GL_TRUE;
      else        
        hwcx->isFullScreen = GL_FALSE;
#endif

      if (xm_buffer->db_state)
	{
          sisBufferInfo *priv = (sisBufferInfo *) xm_buffer->private;
	  
	  sis_alloc_back_image (ctx, xm_buffer->backimage, &priv->bbFree,
	                        &priv->cbClearPacket);
	}

      if (ctx->Visual->DepthBits)
	sis_alloc_z_stencil_buffer (ctx);

      switch (hwcx->zFormat)
	{
	case Z_16:
	  z_depth = 2;
	  break;
	case Z_32:
	case S_8_Z_24:
	  z_depth = 4;
	  break;
	default:
	  assert (0);
	}

      sis_SetDrawBuffer (ctx, ctx->Color.DriverDrawBuffer);
      /* TODO: call sis_SetReadBuffer? */

      current->hwZ &= ~MASK_ZBufferPitch;
      current->hwZ |= xm_buffer->width * z_depth >> 2;
      /* TODO, in xfree 3.9.18, no ctx->Buffer */
      current->hwOffsetZ = ((DWORD) (xm_buffer->depthbuffer) -
			    (DWORD) GET_FbBase (hwcx)) >> 2;

      if ((current->hwOffsetZ ^ prev->hwOffsetZ)
	  || (current->hwZ ^ prev->hwZ))
	{
	  prev->hwOffsetZ = current->hwOffsetZ;
	  prev->hwZ = current->hwZ;

	  /* Z setting */
	  hwcx->GlobalFlag |= GFLAG_ZSETTING;
	}
    }

  /* Needed by Y_FLIP macro */
  xm_buffer->bottom = (int) winheight - 1;

  sis_set_scissor (ctx);
}

const GLubyte *
sis_GetString (GLcontext * ctx, GLenum name)
{
  (void) ctx;  

  switch (name)
    {
    case GL_RENDERER:
#ifdef XFree86Server
      return (GLubyte *)"SIS 300/630/530 IR Mode";
#else
      return (GLubyte *)"SiS 300/630/530 DR Mode";
#endif
    default:
      return NULL;
    }
}

const char *
sis_ExtensionString (GLcontext * ctx)
{
  return NULL;
}

void
sis_set_scissor (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *prev = &hwcx->prev;
  __GLSiSHardware *current = &hwcx->current;

  GLint x1, y1, x2, y2;

  /* ?? from context.c, do it 2 times */

  x1 = 0;
  y1 = 0;
  x2 = xmesa->xm_buffer->width - 1;
  y2 = xmesa->xm_buffer->height - 1;

  if (ctx->Scissor.Enabled)
    {
      if (ctx->Scissor.X > x1)
	{
	  x1 = ctx->Scissor.X;
	}
      if (ctx->Scissor.Y > y1)
	{
	  y1 = ctx->Scissor.Y;
	}
      if (ctx->Scissor.X + ctx->Scissor.Width - 1 < x2)
	{
	  x2 = ctx->Scissor.X + ctx->Scissor.Width - 1;
	}
      if (ctx->Scissor.Y + ctx->Scissor.Height - 1 < y2)
	{
	  y2 = ctx->Scissor.Y + ctx->Scissor.Height - 1;
	}
    }

  y1 = Y_FLIP (y1);
  y2 = Y_FLIP (y2);

  current->clipTopBottom = (y2 << 13) | y1;
  current->clipLeftRight = (x1 << 13) | x2;

  if ((current->clipTopBottom ^ prev->clipTopBottom) ||
      (current->clipLeftRight ^ prev->clipLeftRight))
    {
      prev->clipTopBottom = current->clipTopBottom;
      prev->clipLeftRight = current->clipLeftRight;
      hwcx->GlobalFlag |= GFLAG_CLIPPING;
    }
}
