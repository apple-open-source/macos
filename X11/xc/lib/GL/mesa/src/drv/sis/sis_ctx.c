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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_ctx.c,v 1.3 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#ifdef XFree86Server
# define PSZ 8
# include "cfb.h"
# undef PSZ
# include "cfb16.h"
# include "cfb24.h"
# include "cfb32.h"
# include "cfb24_32.h"
/* for SISPtr */
# include "xf86.h"
# include "xf86_OSproc.h"
# include "xf86Resources.h"
# include "xf86Version.h"
# include "xf86PciInfo.h"
# include "xf86Pci.h"
# include "xf86cmap.h"
# include "vgaHW.h"
# include "xf86RAC.h"
# include "sis_regs.h"
# include "sis.h"
# include "dristruct.h"
# include "dri.h"
#else
#include "sis_dri.h"
#endif

#include "extensions.h"

#include "sis_ctx.h"
#include "sis_mesa.h"

int GlobalCurrentHwcx = -1;
int GlobalHwcxCountBase = 1;
int GlobalCmdQueueLen = 0;

void
WaitEngIdle (__GLSiScontext * hwcx)
{
  BYTE *IOBase = GET_IOBase (hwcx);
  BYTE cEngineState;

  cEngineState = *((BYTE volatile *) (IOBase + 0x8243));
  while (((cEngineState & 0x80) == 0) ||
	 ((cEngineState & 0x40) == 0) || ((cEngineState & 0x20) == 0))
    {
      cEngineState = *((BYTE volatile *) (IOBase + 0x8243));
    }
}

void
Wait2DEngIdle (__GLSiScontext * hwcx)
{
  BYTE *IOBase = GET_IOBase (hwcx);
  BYTE cEngineState;

  cEngineState = *((BYTE volatile *) (IOBase + 0x8243));
  while (!(cEngineState & 0x80))
    {
      cEngineState = *((BYTE volatile *) (IOBase + 0x8243));
    }
}

static void
sis_init_opengl_state (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *current = &hwcx->current;
  __GLSiSHardware *prev = &(hwcx->prev);

  /*
  prev->hwCapEnable = INIT_6327_CapsEnable ;  
  prev->hwCapEnable = 0x000000a0;
  */
  /* add Texture Perspective Enable */
  prev->hwCapEnable = 0x000002a0;

  /*
  prev->hwCapEnable2 = INIT_6327_CapsEnable2;  
  prev->hwCapEnable2 = 0x00aa0080;
  */
  /* if multi-texture enabled, disable Z pre-test */
  prev->hwCapEnable2 = 0x00000000;

  /* Z test mode is LE */
  prev->hwZ = INIT_6327_ZSet;

  /* TODO : maybe call sis_DepthFunc to update is a better way */
  ctx->Driver.DepthFunc (ctx, ctx->Depth.Func);

  /* Depth mask */
  prev->hwZMask = INIT_6327_ZMask;

  /* Alpha test mode is ALWAYS, Alpha ref value is 0 */
  prev->hwAlpha = INIT_6327_AlphaSet;

  /* ROP2 is COPYPEN */
  prev->hwDstSet = INIT_6327_DstSet;

  /* color mask */
  prev->hwDstMask = INIT_6327_DstMask;

  /* LinePattern is 0, Repeat Factor is 0 */
  prev->hwLinePattern = 0x00008000;

  /* Fog mode is Linear Fog, Fog color is (0, 0, 0) */
  prev->hwFog = INIT_6327_FogSet;

  /* Src blend is BLEND_ONE, Dst blend is D3DBLEND_ZERO */
  prev->hwDstSrcBlend = INIT_6327_BlendMode;

  /* Texture mapping mode is Tile */
#if 0
  prev->texture[0].hwTextqureSet = INIT_6327_TextureSet;
#endif
  /* Magnified & minified texture filter is NEAREST */
#if 0
  prev->texture[0].hwTextureMip = INIT_6327_TextureMip;
#endif

  /* Texture Blending seeting */
  prev->hwTexBlendClr0 = INIT_6327_TextureColorBlend0;

  prev->hwTexBlendClr1 = INIT_6327_TextureColorBlend1;

  prev->hwTexBlendAlpha0 = INIT_6327_TextureAlphaBlend0;

  prev->hwTexBlendAlpha1 = INIT_6327_TextureAlphaBlend1;

  memcpy (current, prev, sizeof (__GLSiSHardware));

  /* Init the texture transparency color high range value */
#if 0
  lpdwRegIO = ((LPDWORD)hwcx->lpEngIO + REG_3D_TransparencyColorHigh);
  prev->hwTextureClrHigh = INIT_6326_TextureClrHigh;
  *(lpdwRegIO) = INIT_6327_TextureClrHigh;
#endif
}

static void
sis_init_user_setting (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  /* disable all unsupported per-pixel extensions */  
  gl_extensions_disable (ctx, "GL_EXT_blend_color");
  gl_extensions_disable (ctx, "GL_EXT_blend_minmax");
  gl_extensions_disable (ctx, "GL_EXT_blend_logic_op");
  gl_extensions_disable (ctx, "GL_EXT_blend_subtract");
  gl_extensions_disable (ctx, "GL_EXT_paletted_texture");
  gl_extensions_disable (ctx, "GL_EXT_point_parameters");
  gl_extensions_disable (ctx, "GL_EXT_texture3D");
  gl_extensions_disable (ctx, "GL_INGR_blend_func_separate");
  gl_extensions_disable (ctx, "GL_PGI_misc_hints");
  gl_extensions_disable (ctx, "GL_EXT_clip_volume_hint");
  gl_extensions_disable (ctx, "GL_EXT_texture_env_add");
    
  /* TODO: driver doesn't handle this */
  if (getenv ("SIS_SINGLE_TEXTURE"))
    gl_extensions_disable (ctx, "GL_ARB_multitexture");

  /* turning off the extension has more speed */
  /* if mesa supports indirect VB rendering, remove it */
  
  /* if disable it, quake3 will have broken triangle */
  /*  gl_extensions_disable (ctx, "GL_EXT_compiled_vertex_array"); */

  /* debug */
  if(getenv ("SIS_NO_AGP_CMDS"))
    hwcx->AGPCmdModeEnabled = GL_FALSE;

#if SIS_STEREO
  if(getenv ("SIS_STEREO") && hwcx->irqEnabled)
    hwcx->useStereo = GL_TRUE;
  else
    hwcx->useStereo = GL_FALSE;

  {
    float val;
    char *str;
    
    /* TODO: error check */
    if((str=getenv("SIS_STEREO_OFFSET"))){
      val= atof(str);
      if(val>0 && val<1){
        StereoInitOffset = val;
      }
    }

    if((str=getenv("SIS_STEREO_SCALE"))){
      val= atof(str);
      if(val>0){      
        StereoInitScale = val;
      }
    }  
  }
  
  if(getenv("SIS_STEREO_DYNAMIC_Z"))
    StereoDynamicZ = GL_TRUE;
#endif
}

void
SiSCreateContext (XMesaContext xmesa)
{
  GLcontext *ctx = xmesa->gl_ctx;
  __GLSiScontext *hwcx;

  int i;

  hwcx = (__GLSiScontext *) calloc (1, sizeof (__GLSiScontext));
  if (!hwcx)
    {
      fprintf (stderr, "SIS Driver : allocating context fails\n");
      sis_fatal_error ();
      return;
    }

  /* set gc */
  hwcx->gc = ctx;
  xmesa->private = hwcx;

  /* set static part in ctx->Driver */
  sis_init_driver (ctx);

  /* Set 2D data (from X-Server) */
  /* i assume the data will not change during X-server's lifetime */
#ifdef XFree86Server
  {
    ScrnInfoPtr pScrn = xf86Screens[xmesa->display->myNum];
    SISPtr pSiS = SISPTR (pScrn);

    hwcx->virtualX = pSiS->pScrn->virtualX;
    hwcx->virtualY = pSiS->pScrn->virtualY;
    hwcx->bytesPerPixel = (pSiS->pScrn->bitsPerPixel + 7) / 8;
    hwcx->IOBase = pSiS->IOBase;
    hwcx->FbBase = pSiS->FbBase;
    hwcx->displayWidth = pSiS->pScrn->displayWidth * hwcx->bytesPerPixel;
    hwcx->pitch = pSiS->scrnOffset;
    hwcx->Chipset = pSiS->Chipset;
    hwcx->drmSubFD = pSiS->drmSubFD;
#if SIS_STEREO
    hwcx->irqEnabled = pSiS->irqEnabled;
#endif
  }
#else
  {
    __DRIscreenPrivate *psp = xmesa->driContextPriv->driScreenPriv;
    SISDRIPtr priv = (SISDRIPtr) psp->pDevPriv;

    hwcx->virtualX = priv->width;
    hwcx->virtualY = priv->height;
    hwcx->bytesPerPixel = priv->bytesPerPixel;
    hwcx->IOBase = priv->regs.map;
    hwcx->FbBase = psp->pFB;
    hwcx->displayWidth = psp->fbWidth;
    hwcx->pitch = psp->fbStride;
    hwcx->Chipset = priv->deviceID;
    /* TODO: make sure psp->fd is sub-driver's fd */
    hwcx->drmSubFD = psp->fd;
#if SIS_STEREO
    hwcx->irqEnabled = priv->irqEnabled;
#endif
  }
#endif

#if defined(SIS_DUMP)
  IOBase4Debug = GET_IOBase (hwcx);
#endif

  /* support ARGB8888 and RGB565 */
  switch (hwcx->bytesPerPixel)
    {
    case 4:
      hwcx->redMask = 0x00ff0000;
      hwcx->greenMask = 0x0000ff00;
      hwcx->blueMask = 0x000000ff;
      hwcx->alphaMask = 0xff000000;
      hwcx->colorFormat = DST_FORMAT_ARGB_8888;
      break;
    case 2:
      hwcx->redMask = 0xf800;
      hwcx->greenMask = 0x07e0;
      hwcx->blueMask = 0x001f;
      hwcx->alphaMask = 0;
      hwcx->colorFormat = DST_FORMAT_RGB_565;
      break;
    default:
      assert (0);
    }
   
    sis_sw_init_driver (ctx);

  /* TODO: index mode */
  
#if defined(XFree86Server)
  {
#if defined(XF86DRI)
    ScreenPtr pScreen = xmesa->display;
    ScrnInfoPtr pScrn = xf86Screens[xmesa->display->myNum];
    SISPtr pSiS = SISPTR (pScrn);

    if (pSiS->directRenderingEnabled)
      {
	SISSAREAPriv *saPriv = (SISSAREAPriv *) DRIGetSAREAPrivate (pScreen);

	drmContextPtr contextPtr;

	/* in DR, the action is done by DRI */
	hwcx->pDRIContextPriv = DRICreateContextPriv (pScreen, contextPtr, 0);
	if (!contextPtr)
	  {
	    /* TODO */
	    assert(0);
	  }

	hwcx->serialNumber = (int) *contextPtr;
	hwcx->CurrentHwcxPtr = &(saPriv->CtxOwner);
	hwcx->CurrentQueueLenPtr = pSiS->cmdQueueLenPtr;
        /* hwcx->FrameCountPtr = */

	/* what does this do? */
	/*
	drmFreeReservedContextList (contextPtr);
        */
        
	/* TODO, set AGP command buffer */
	hwcx->AGPCmdModeEnabled = GL_FALSE;
      }
    else
#endif
      {
	hwcx->serialNumber = GlobalHwcxCountBase++;
	hwcx->CurrentHwcxPtr = &GlobalCurrentHwcx;
	hwcx->CurrentQueueLenPtr = pSiS->cmdQueueLenPtr;
        /* hwcx->FrameCountPtr = */

	/* TODO, set AGP command buffer */
	hwcx->AGPCmdModeEnabled = GL_FALSE;
      }
  }
#else
  {
    __DRIscreenPrivate *psp = xmesa->driContextPriv->driScreenPriv;
    SISDRIPtr priv = (SISDRIPtr) psp->pDevPriv;
    SISSAREAPriv *saPriv = (SISSAREAPriv *) (((char *) psp->pSAREA) +
					     sizeof (XF86DRISAREARec));

    /* or use xmesa->driContextPriv->contextID
     * use hHWContext is better, but limit ID to [0..2^31-1] (modify driver)
     * hHWContext is CARD32
     */
    hwcx->serialNumber = xmesa->driContextPriv->hHWContext;
    hwcx->CurrentHwcxPtr = &(saPriv->CtxOwner);
    hwcx->CurrentQueueLenPtr = &(saPriv->QueueLength);
    hwcx->FrameCountPtr = &(saPriv->FrameCount);

    /* set AGP */
    hwcx->AGPSize = priv->agp.size;
    hwcx->AGPBase = priv->agp.map;    
    hwcx->AGPAddr = priv->agp.handle;        
    
    /* set AGP command buffer */
    hwcx->AGPCmdModeEnabled = GL_FALSE;
    if (hwcx->AGPSize){	
	if(priv->AGPCmdBufSize){
          hwcx->AGPCmdBufBase = hwcx->AGPBase + priv->AGPCmdBufOffset;
          hwcx->AGPCmdBufAddr = hwcx->AGPAddr + priv->AGPCmdBufOffset;
          hwcx->AGPCmdBufSize = priv->AGPCmdBufSize;

          hwcx->pAGPCmdBufNext = (DWORD *)&(saPriv->AGPCmdBufNext);
          hwcx->AGPCmdModeEnabled = GL_TRUE;
        }
      }
  }
#endif

  hwcx->GlobalFlag = 0L;

  hwcx->swRenderFlag = 0;
  hwcx->swForceRender = GL_FALSE;
  hwcx->Primitive = 0;
  hwcx->useFastPath = GL_FALSE;

  /* TODO */
  /* hwcx->blockWrite = SGRAMbw = IsBlockWrite (); */
  hwcx->blockWrite = GL_FALSE;

  /* this function will over-write AGPCmdModeEnabled */
  /* TODO: pay attention to side-effect */
  sis_init_user_setting (ctx);

  sis_init_opengl_state (ctx);
  sis_set_buffer_static (ctx);
  set_color_pattern (hwcx, 0, 0, 0, 0);
  set_z_stencil_pattern (hwcx, 1.0, 0);

  /* TODO: need to clear cache? */
  hwcx->clearTexCache = GL_TRUE;
 
  hwcx->AGPParseSet = 0x00000040;
  hwcx->dwPrimitiveSet = 0x00060000;

  for (i = 0; i < SIS_MAX_TEXTURES; i++)
    {
      hwcx->TexStates[i] = 0;
      hwcx->PrevTexFormat[i] = 0;
    }

#if SIS_STEREO
  hwcx->isFullScreen = GL_FALSE;
  hwcx->stereoEnabled = GL_FALSE;
#endif
}

void
SiSDestroyContext (XMesaContext xmesa)
{
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

#if defined(XFree86Server) && defined(XF86DRI)
  DRIDestroyContextPriv ((DRIContextPrivPtr)hwcx->pDRIContextPriv);
#endif
  
  /* 
   * TODO: if the context ID given by kernel will be recycled,
   *       then, the current ID will set to -1 if the current ID
   *       is equal to my id
   */
  free (hwcx);
}

void
sis_update_render_state (__GLSiScontext * hwcx, GLuint stateType)
{
  __GLSiSHardware *prev = &hwcx->prev;

  mWait3DCmdQueue (45);

  if (hwcx->GlobalFlag & GFLAG_ENABLESETTING)
    {
      if (!hwcx->clearTexCache)
	{
	  MMIO (REG_3D_TEnable, prev->hwCapEnable);
	}
      else
	{
	  MMIO (REG_3D_TEnable, prev->hwCapEnable | MASK_TextureCacheClear);
	  MMIO (REG_3D_TEnable, prev->hwCapEnable);
	  hwcx->clearTexCache = GL_FALSE;
	}
    }

  if (hwcx->GlobalFlag & GFLAG_ENABLESETTING2)
    {
      MMIO (REG_3D_TEnable2, prev->hwCapEnable2);
    }

  /* Z Setting */
  if (hwcx->GlobalFlag & GFLAG_ZSETTING)
    {
      MMIO (REG_3D_ZSet, prev->hwZ);
      MMIO (REG_3D_ZStWriteMask, prev->hwZMask);
      MMIO (REG_3D_ZAddress, prev->hwOffsetZ);
    }

  /* Alpha Setting */
  if (hwcx->GlobalFlag & GFLAG_ALPHASETTING)
    {
      MMIO (REG_3D_AlphaSet, prev->hwAlpha);
    }

  if (hwcx->GlobalFlag & GFLAG_DESTSETTING)
    {
      MMIO (REG_3D_DstSet, prev->hwDstSet);
      MMIO (REG_3D_DstAlphaWriteMask, prev->hwDstMask);
      MMIO (REG_3D_DstAddress, prev->hwOffsetDest);
    }

  /* Line Setting */
#if 0
     if (hwcx->GlobalFlag & GFLAG_LINESETTING) 
     {
     MMIO(REG_3D_LinePattern, prev->hwLinePattern);
     }
#endif

  /* Fog Setting */
  if (hwcx->GlobalFlag & GFLAG_FOGSETTING)
    {
      MMIO (REG_3D_FogSet, prev->hwFog);
      MMIO (REG_3D_FogInverseDistance, prev->hwFogInverse);
      MMIO (REG_3D_FogFarDistance, prev->hwFogFar);
      MMIO (REG_3D_FogFactorDensity, prev->hwFogDensity);
    }

  /* Stencil Setting */
  if (hwcx->GlobalFlag & GFLAG_STENCILSETTING)
    {
      MMIO (REG_3D_StencilSet, prev->hwStSetting);
      MMIO (REG_3D_StencilSet2, prev->hwStSetting2);
    }

  /* Miscellaneous Setting */
  if (hwcx->GlobalFlag & GFLAG_DSTBLEND)
    {
      MMIO (REG_3D_DstBlendMode, prev->hwDstSrcBlend);
    }
  if (hwcx->GlobalFlag & GFLAG_CLIPPING)
    {
      MMIO (REG_3D_ClipTopBottom, prev->clipTopBottom);
      MMIO (REG_3D_ClipLeftRight, prev->clipLeftRight);
    }

  hwcx->GlobalFlag &= ~GFLAG_RENDER_STATES;
}

void
sis_update_texture_state (__GLSiScontext * hwcx)
{
  __GLSiSHardware *prev = &hwcx->prev;

  mWait3DCmdQueue (55);

  if (hwcx->clearTexCache)
    {
      MMIO (REG_3D_TEnable, prev->hwCapEnable | MASK_TextureCacheClear);
      MMIO (REG_3D_TEnable, prev->hwCapEnable);
      hwcx->clearTexCache = GL_FALSE;
    }

  /* Texture Setting */
  if (hwcx->GlobalFlag & CFLAG_TEXTURERESET)
    {
      MMIO (REG_3D_TextureSet, prev->texture[0].hwTextureSet);
    }
  if (hwcx->GlobalFlag & GFLAG_TEXTUREMIPMAP)
    {
      MMIO (REG_3D_TextureMip, prev->texture[0].hwTextureMip);
    }

  /*
  MMIO(REG_3D_TextureTransparencyColorHigh, prev->texture[0].hwTextureClrHigh);
  MMIO(REG_3D_TextureTransparencyColorLow, prev->texture[0].hwTextureClrLow);
  */

  if (hwcx->GlobalFlag & GFLAG_TEXBORDERCOLOR)
    {
      MMIO (REG_3D_TextureBorderColor, prev->texture[0].hwTextureBorderColor);
    }
  if (hwcx->GlobalFlag & GFLAG_TEXTUREADDRESS)
    {
      MMIO (REG_3D_TEnable, prev->hwCapEnable | MASK_TextureCacheClear);
      MMIO (REG_3D_TEnable, prev->hwCapEnable);

      switch ((prev->texture[0].hwTextureSet & MASK_TextureLevel) >> 8)
	{
	case 11:		
	  MMIO (REG_3D_TextureAddress11, prev->texture[0].texOffset11);
	case 10:		
	  MMIO (REG_3D_TextureAddress10, prev->texture[0].texOffset10);
	  MMIO (REG_3D_TexturePitch10, prev->texture[0].texPitch10);
	case 9:		
	  MMIO (REG_3D_TextureAddress9, prev->texture[0].texOffset9);
	case 8:		
	  MMIO (REG_3D_TextureAddress8, prev->texture[0].texOffset8);
	  MMIO (REG_3D_TexturePitch8, prev->texture[0].texPitch89);
	case 7:		
	  MMIO (REG_3D_TextureAddress7, prev->texture[0].texOffset7);
	case 6:		
	  MMIO (REG_3D_TextureAddress6, prev->texture[0].texOffset6);
	  MMIO (REG_3D_TexturePitch6, prev->texture[0].texPitch67);
	case 5:		
	  MMIO (REG_3D_TextureAddress5, prev->texture[0].texOffset5);
	case 4:		
	  MMIO (REG_3D_TextureAddress4, prev->texture[0].texOffset4);
	  MMIO (REG_3D_TexturePitch4, prev->texture[0].texPitch45);
	case 3:		
	  MMIO (REG_3D_TextureAddress3, prev->texture[0].texOffset3);
	case 2:		
	  MMIO (REG_3D_TextureAddress2, prev->texture[0].texOffset2);
	  MMIO (REG_3D_TexturePitch2, prev->texture[0].texPitch23);
	case 1:		
	  MMIO (REG_3D_TextureAddress1, prev->texture[0].texOffset1);
	case 0:		
	  MMIO (REG_3D_TextureAddress0, prev->texture[0].texOffset0);
	  MMIO (REG_3D_TexturePitch0, prev->texture[0].texPitch01);
	}
    }
  if (hwcx->GlobalFlag & CFLAG_TEXTURERESET_1)
    {
      MMIO (REG_3D_Texture1Set, prev->texture[1].hwTextureSet);
    }
  if (hwcx->GlobalFlag & GFLAG_TEXTUREMIPMAP_1)
    {
      MMIO (REG_3D_Texture1Mip, prev->texture[1].hwTextureMip);
    }

  if (hwcx->GlobalFlag & GFLAG_TEXBORDERCOLOR_1)
    {
      MMIO (REG_3D_Texture1BorderColor,
	    prev->texture[1].hwTextureBorderColor);
    }
  if (hwcx->GlobalFlag & GFLAG_TEXTUREADDRESS_1)
    {
      switch ((prev->texture[1].hwTextureSet & MASK_TextureLevel) >> 8)
	{
	case 11:		
	  MMIO (REG_3D_Texture1Address11, prev->texture[1].texOffset11);
	case 10:		
	  MMIO (REG_3D_Texture1Address10, prev->texture[1].texOffset10);
	  MMIO (REG_3D_Texture1Pitch10, prev->texture[1].texPitch10);
	case 9:		
	  MMIO (REG_3D_Texture1Address9, prev->texture[1].texOffset9);
	case 8:		
	  MMIO (REG_3D_Texture1Address8, prev->texture[1].texOffset8);
	  MMIO (REG_3D_Texture1Pitch8, prev->texture[1].texPitch89);
	case 7:		
	  MMIO (REG_3D_Texture1Address7, prev->texture[1].texOffset7);
	case 6:		
	  MMIO (REG_3D_Texture1Address6, prev->texture[1].texOffset6);
	  MMIO (REG_3D_Texture1Pitch6, prev->texture[1].texPitch67);
	case 5:		
	  MMIO (REG_3D_Texture1Address5, prev->texture[1].texOffset5);
	case 4:		
	  MMIO (REG_3D_Texture1Address4, prev->texture[1].texOffset4);
	  MMIO (REG_3D_Texture1Pitch4, prev->texture[1].texPitch45);
	case 3:		
	  MMIO (REG_3D_Texture1Address3, prev->texture[1].texOffset3);
	case 2:		
	  MMIO (REG_3D_Texture1Address2, prev->texture[1].texOffset2);
	  MMIO (REG_3D_Texture1Pitch2, prev->texture[1].texPitch23);
	case 1:		
	  MMIO (REG_3D_Texture1Address1, prev->texture[1].texOffset1);
	case 0:		
	  MMIO (REG_3D_Texture1Address0, prev->texture[1].texOffset0);
	  MMIO (REG_3D_Texture1Pitch0, prev->texture[1].texPitch01);
	}
    }

  /* texture environment */
  if (hwcx->GlobalFlag & GFLAG_TEXTUREENV)
    {
      MMIO (REG_3D_TextureBlendFactor, prev->hwTexEnvColor);
      MMIO (REG_3D_TextureColorBlendSet0, prev->hwTexBlendClr0);
      MMIO (REG_3D_TextureAlphaBlendSet0, prev->hwTexBlendAlpha0);
    }
  if (hwcx->GlobalFlag & GFLAG_TEXTUREENV_1)
    {
      MMIO (REG_3D_TextureBlendFactor, prev->hwTexEnvColor);
      MMIO (REG_3D_TextureColorBlendSet1, prev->hwTexBlendClr1);
      MMIO (REG_3D_TextureAlphaBlendSet1, prev->hwTexBlendAlpha1);
    }

  hwcx->GlobalFlag &= ~GFLAG_TEXTURE_STATES;
}

void
sis_validate_all_state (__GLSiScontext * hwcx)
{
  __GLSiSHardware *prev = &hwcx->prev;

  mEndPrimitive ();
  mWait3DCmdQueue (40);

  /* Enable Setting */
  MMIO (REG_3D_TEnable, prev->hwCapEnable);
  MMIO (REG_3D_TEnable2, prev->hwCapEnable2);

  /* Z Setting */
  /* if (prev->hwCapEnable & MASK_ZTestEnable) { */
  MMIO (REG_3D_ZSet, prev->hwZ);
  MMIO (REG_3D_ZStWriteMask, prev->hwZMask);
  MMIO (REG_3D_ZAddress, prev->hwOffsetZ);
  /* } */

  /* Alpha Setting */
  if (prev->hwCapEnable & MASK_AlphaTestEnable)
    {
      MMIO (REG_3D_AlphaSet, prev->hwAlpha);
    }

  /* Destination Setting */
  MMIO (REG_3D_DstSet, prev->hwDstSet);
  MMIO (REG_3D_DstAlphaWriteMask, prev->hwDstMask);
  MMIO (REG_3D_DstAddress, prev->hwOffsetDest);

  /* Line Setting */
#if 0
  if (prev->hwCapEnable2 & MASK_LinePatternEnable) {
        MMIO(REG_3D_LinePattern, prev->hwLinePattern);
  }
#endif

  /* Fog Setting */
  if (prev->hwCapEnable & MASK_FogEnable)
    {
      MMIO (REG_3D_FogSet, prev->hwFog);
      MMIO (REG_3D_FogInverseDistance, prev->hwFogInverse);
      MMIO (REG_3D_FogFarDistance, prev->hwFogFar);
      MMIO (REG_3D_FogFactorDensity, prev->hwFogDensity);
    }

  /* Stencil Setting */
  if (prev->hwCapEnable & MASK_StencilTestEnable)
    {
      MMIO (REG_3D_StencilSet, prev->hwStSetting);
      MMIO (REG_3D_StencilSet2, prev->hwStSetting2);
    }

  /* Miscellaneous Setting */
  if (prev->hwCapEnable & MASK_BlendEnable)
    {
      MMIO (REG_3D_DstBlendMode, prev->hwDstSrcBlend);
    }

  MMIO (REG_3D_ClipTopBottom, prev->clipTopBottom);
  MMIO (REG_3D_ClipLeftRight, prev->clipLeftRight);

  /* TODO */
  /* Texture Setting */
  /* if (prev->hwCapEnable & MASK_TextureEnable) */
  {
  MMIO (REG_3D_TEnable, prev->hwCapEnable | MASK_TextureCacheClear);

  MMIO (REG_3D_TEnable, prev->hwCapEnable);

  MMIO (REG_3D_TextureSet, prev->texture[0].hwTextureSet);
  MMIO (REG_3D_TextureMip, prev->texture[0].hwTextureMip);
  /*
  MMIO(REG_3D_TextureTransparencyColorHigh, prev->texture[0].hwTextureClrHigh);
  MMIO(REG_3D_TextureTransparencyColorLow, prev->texture[0].hwTextureClrLow);
  */
  MMIO (REG_3D_TextureBorderColor, prev->texture[0].hwTextureBorderColor);

  switch ((prev->texture[0].hwTextureSet & MASK_TextureLevel) >> 8)
    {
    case 11:			
      MMIO (REG_3D_TextureAddress11, prev->texture[0].texOffset11);
    case 10:			
      MMIO (REG_3D_TextureAddress10, prev->texture[0].texOffset10);
      MMIO (REG_3D_TexturePitch10, prev->texture[0].texPitch10);
    case 9:			
      MMIO (REG_3D_TextureAddress9, prev->texture[0].texOffset9);
    case 8:			
      MMIO (REG_3D_TextureAddress8, prev->texture[0].texOffset8);
      MMIO (REG_3D_TexturePitch8, prev->texture[0].texPitch89);
    case 7:			
      MMIO (REG_3D_TextureAddress7, prev->texture[0].texOffset7);
    case 6:			
      MMIO (REG_3D_TextureAddress6, prev->texture[0].texOffset6);
      MMIO (REG_3D_TexturePitch6, prev->texture[0].texPitch67);
    case 5:			
      MMIO (REG_3D_TextureAddress5, prev->texture[0].texOffset5);
    case 4:			
      MMIO (REG_3D_TextureAddress4, prev->texture[0].texOffset4);
      MMIO (REG_3D_TexturePitch4, prev->texture[0].texPitch45);
    case 3:			
      MMIO (REG_3D_TextureAddress3, prev->texture[0].texOffset3);
    case 2:			
      MMIO (REG_3D_TextureAddress2, prev->texture[0].texOffset2);
      MMIO (REG_3D_TexturePitch2, prev->texture[0].texPitch23);
    case 1:			
      MMIO (REG_3D_TextureAddress1, prev->texture[0].texOffset1);
    case 0:			
      MMIO (REG_3D_TextureAddress0, prev->texture[0].texOffset0);
      MMIO (REG_3D_TexturePitch0, prev->texture[0].texPitch01);
    }

  /* TODO */
  /* if (hwcx->ctx->Texture.Unit[1].ReallyEnabled) */
  {
    MMIO (REG_3D_Texture1Set, prev->texture[1].hwTextureSet);
    MMIO (REG_3D_Texture1Mip, prev->texture[1].hwTextureMip);
    /*
    MMIO(REG_3D_Texture1TransparencyColorHigh, prev->texture[1].hwTextureClrHigh);
    MMIO(REG_3D_Texture1TransparencyColorLow, prev->texture[1].hwTextureClrLow);
    */
    MMIO (REG_3D_Texture1BorderColor, prev->texture[1].hwTextureBorderColor);

    switch ((prev->texture[1].hwTextureSet & MASK_TextureLevel) >> 8)
      {
      case 11:			
	MMIO (REG_3D_Texture1Address11, prev->texture[1].texOffset11);
      case 10:			
	MMIO (REG_3D_Texture1Address10, prev->texture[1].texOffset10);
	MMIO (REG_3D_Texture1Pitch10, prev->texture[1].texPitch10);
      case 9:			
	MMIO (REG_3D_Texture1Address9, prev->texture[1].texOffset9);
      case 8:			
	MMIO (REG_3D_Texture1Address8, prev->texture[1].texOffset8);
	MMIO (REG_3D_Texture1Pitch8, prev->texture[1].texPitch89);
      case 7:			
	MMIO (REG_3D_Texture1Address7, prev->texture[1].texOffset7);
      case 6:			
	MMIO (REG_3D_Texture1Address6, prev->texture[1].texOffset6);
	MMIO (REG_3D_Texture1Pitch6, prev->texture[1].texPitch67);
      case 5:			
	MMIO (REG_3D_Texture1Address5, prev->texture[1].texOffset5);
      case 4:			
	MMIO (REG_3D_Texture1Address4, prev->texture[1].texOffset4);
	MMIO (REG_3D_Texture1Pitch4, prev->texture[1].texPitch45);
      case 3:			
	MMIO (REG_3D_Texture1Address3, prev->texture[1].texOffset3);
      case 2:			
	MMIO (REG_3D_Texture1Address2, prev->texture[1].texOffset2);
	MMIO (REG_3D_Texture1Pitch2, prev->texture[1].texPitch23);
      case 1:			
	MMIO (REG_3D_Texture1Address1, prev->texture[1].texOffset1);
      case 0:			
	MMIO (REG_3D_Texture1Address0, prev->texture[1].texOffset0);
	MMIO (REG_3D_Texture1Pitch0, prev->texture[1].texPitch01);
      }
  }

  /* texture environment */
  MMIO (REG_3D_TextureBlendFactor, prev->hwTexEnvColor);
  MMIO (REG_3D_TextureColorBlendSet0, prev->hwTexBlendClr0);
  MMIO (REG_3D_TextureColorBlendSet1, prev->hwTexBlendClr1);
  MMIO (REG_3D_TextureAlphaBlendSet0, prev->hwTexBlendAlpha0);
  MMIO (REG_3D_TextureAlphaBlendSet1, prev->hwTexBlendAlpha1);
  }

  hwcx->GlobalFlag = 0;
}

void
sis_fatal_error (void)
{
  /* free video memory, or the framebuffer device will do it automatically */

#ifdef XFree86Server
  FatalError ("Fatal errors in libGLcore.a\n");
#else
  fprintf(stderr, "Fatal errors in sis_dri.so\n");
  exit (-1);
#endif
}
