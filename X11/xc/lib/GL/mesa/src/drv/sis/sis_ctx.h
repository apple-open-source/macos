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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_ctx.h,v 1.5 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#ifndef _sis_ctx_h_
#define _sis_ctx_h_

#include "sis_common.h"

/* for GLboolean */
#include <GL/gl.h>

#define PCI_CHIP_SIS300		0x0300
#define PCI_CHIP_SIS630		0x6300
#define PCI_CHIP_SIS540		0x5300

#define SIS_STATE_TRIANGLE      1
#define SIS_STATE_LINE          2
#define SIS_STATE_POINT         3

/* for swRenderFlag */
#define SIS_SW_TRIANGLE 0x1
#define SIS_SW_LINE 0x2
#define SIS_SW_POINT 0x4
#define SIS_SW_TEXTURE_DIM 0x8
#define SIS_SW_TEXTURE_OBJ 0x10
#define SIS_SW_TEXTURE_OBJ1 0x20
#define SIS_SW_TEXTURE_ENV 0x100
#define SIS_SW_TEXTURE_ENV1 0x200

#define SIS_SW_TEXTURE (SIS_SW_TEXTURE_DIM | \
                        SIS_SW_TEXTURE_OBJ | SIS_SW_TEXTURE_OBJ1 | \
                        SIS_SW_TEXTURE_ENV | SIS_SW_TEXTURE_ENV1)
#define SIS_SW_ALL (SIS_SW_POINT | SIS_SW_LINE | SIS_SW_TRIANGLE | \
                    SIS_SW_TEXTURE)

/*
 ** Device dependent context state
 */
typedef struct __GLSiSTextureRec
{
  DWORD hwTextureSet;
  DWORD hwTextureMip;
  DWORD hwTextureClrHigh;
  DWORD hwTextureClrLow;
  DWORD hwTextureBorderColor;

  DWORD texOffset0;
  DWORD texOffset1;
  DWORD texOffset2;
  DWORD texOffset3;
  DWORD texOffset4;
  DWORD texOffset5;
  DWORD texOffset6;
  DWORD texOffset7;
  DWORD texOffset8;
  DWORD texOffset9;
  DWORD texOffset10;
  DWORD texOffset11;

  DWORD texPitch01;
  DWORD texPitch23;
  DWORD texPitch45;
  DWORD texPitch67;
  DWORD texPitch89;
  DWORD texPitch10;
}
__GLSiSTexture;

typedef struct __GLSiSHardwareRec
{
  DWORD hwCapEnable, hwCapEnable2;	/*  Enable Setting */

  DWORD hwOffsetZ, hwZ;		/* Z Setting */

  DWORD hwZBias, hwZMask;	/* Z Setting */

  DWORD hwAlpha;		/* Alpha Setting */

  DWORD hwDstSet, hwDstMask;	/* Destination Setting */

  DWORD hwOffsetDest;		/* Destination Setting */

  DWORD hwLinePattern;		/* Line Setting */

  DWORD hwFog;			/* Fog Setting */

  DWORD hwFogFar, hwFogInverse;	/* Fog Distance setting */

  DWORD hwFogDensity;		/* Fog factor & density */

  DWORD hwStSetting, hwStSetting2;	/* Stencil Setting */

  DWORD hwStOffset;		/* Stencil Setting */

  DWORD hwDstSrcBlend;		/* Blending mode Setting */

  DWORD clipTopBottom;		/* Clip for Top & Bottom */

  DWORD clipLeftRight;		/* Clip for Left & Right */

  struct __GLSiSTextureRec texture[2];

  DWORD hwTexEnvColor;		/* Texture Blending Setting */

  DWORD hwTexBlendClr0;
  DWORD hwTexBlendClr1;
  DWORD hwTexBlendAlpha0;
  DWORD hwTexBlendAlpha1;

}
__GLSiSHardware;

/* Device dependent context state */

typedef struct __GLSiScontextRec
{
  /* This must be first in this structure */
  GLcontext *gc;

  unsigned int virtualX, virtualY;
  unsigned int bytesPerPixel;
  unsigned char *IOBase;
  unsigned char *FbBase;
  unsigned int displayWidth;
  unsigned int pitch;

  /* For Software Renderer */
  GLubyte *swRenderBase;
  GLuint swRenderPitch;
  GLubyte *swZBase;
  GLuint swZPitch;
  GLuint pixelValue;
  GLboolean swForceRender;

  /* HW RGBA layout */
  unsigned int redMask, greenMask, blueMask, alphaMask;
  unsigned int colorFormat;

  /* Z format */
  unsigned int zFormat;

  /* Clear patterns, 4 bytes */
  unsigned int clearColorPattern;
  unsigned int clearZStencilPattern;

  /* Render Function */
  points_func PointsFunc;
  line_func LineFunc;
  triangle_func TriangleFunc;
  quad_func QuadFunc;
  rect_func RectFunc;

  /* DRM fd */
  int drmSubFD;
  
  /* AGP Memory */
  unsigned int AGPSize;
  unsigned char *AGPBase;
  unsigned int AGPAddr;
  
  /* AGP Command Buffer */
  /* TODO: use Global variables */
  unsigned char *AGPCmdBufBase;
  DWORD AGPCmdBufAddr;
  unsigned int AGPCmdBufSize;
  DWORD *pAGPCmdBufNext;
  GLboolean AGPCmdModeEnabled;
  GLboolean UseAGPCmdMode;

  /* register 0x89F4 */
  DWORD AGPParseSet;

  /* register 0x89F8 */
  DWORD dwPrimitiveSet;

  __GLSiSHardware prev, current;

  DWORD chipVer;
  int Chipset;

  DWORD drawableID;

  /* SGRAM block write */
  GLboolean blockWrite;

  GLuint swRenderFlag;
  GLenum Primitive;
  
  /* Fast Path */
  GLboolean useFastPath;

  DWORD GlobalFlag;

  DWORD rawLockMask;
  DWORD lockMask;

  void (*SwapBuffers)(XMesaBuffer b);

  /* Stereo */
  GLboolean isFullScreen;
  GLboolean useStereo;
  GLboolean stereoEnabled;
  int stereo_drawIndex;
  int stereo_drawSide;
  GLboolean irqEnabled;
      
  int serialNumber;

#if defined(XFree86Server) && defined(XF86DRI)
  void *pDRIContextPriv;
#endif

  GLboolean clearTexCache;

  GLuint TexStates[SIS_MAX_TEXTURES];
  GLuint PrevTexFormat[SIS_MAX_TEXTURES];

  int *CurrentHwcxPtr;
  int *CurrentQueueLenPtr;
  unsigned int *FrameCountPtr;
}
__GLSiScontext;

/* Macros */
#define GET_IOBase(x) ((x)->IOBase)
#define GET_FbBase(x) ((x)->FbBase)
#define GET_AGPBase(x) ((x)->AGPBase)
#define GET_DEPTH(x) ((x)->bytesPerPixel)
#define GET_WIDTH(x) ((x)->displayWidth)
#define GET_PITCH(x) ((x)->pitch)
#define GET_FbPos(hwcx,x,y) (GET_FbBase(hwcx)+(x)*GET_DEPTH(hwcx)\
                             +(y)*GET_PITCH(hwcx))

#define GET_ColorFormat(x) ((x)->colorFormat)

#define GET_RMASK(x) ((x)->redMask)
#define GET_GMASK(x) ((x)->greenMask)
#define GET_BMASK(x) ((x)->blueMask)
#define GET_AMASK(x) ((x)->alphaMask)

/* update to hwcx->prev */
extern void sis_update_drawable_state (GLcontext * ctx);

/* update to hw */
extern void sis_update_texture_state (__GLSiScontext * hwcx);
extern void sis_update_render_state (__GLSiScontext * hwcx, GLuint stateType);
extern void sis_validate_all_state (__GLSiScontext * hwcx);

extern void sis_set_scissor (GLcontext * gc);

/* AGP */
void sis_StartAGP (GLcontext * ctx);
void sis_FlushAGP (GLcontext * ctx);
extern float *AGP_CurrentPtr;

/* DRM FD */
extern int gDRMSubFD;

void sis_fatal_error (void);

#endif
