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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_render.c,v 1.5 2000/09/26 15:56:49 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"

#define SIS_SMOOTH 0x1
#define SIS_USE_W 0x2
#define SIS_TEXTURE0 0x4
#define SIS_TEXTURE1 0x8
#define SIS_TRI_OFFSET 0x10
#define SIS_USE_Z 0x11
#define SIS_FALLBACK 0x80000000

#define SIS_DEPTH_SCALE 1.0

/* 
 * TODO: assert(hwcx->AGPCmdBufSize % AGP_ALLOC_SIZE == 0) 
 *       depends on VB_SIZE is better
 */
#define AGP_ALLOC_SIZE 0x10000
/* #define AGP_ALLOC_SIZE (VB_SIZE/3*4 * 9) */
static DWORD AGP_EngineOffset;
static DWORD *AGP_StartPtr;
/* export to sis_fastpath.c */
float *AGP_CurrentPtr;

#define SIS_TAG(x) x##_flat
#define SIS_STATES (0)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth
#define SIS_STATES (SIS_SMOOTH)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_w
#define SIS_STATES (SIS_USE_W)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_w
#define SIS_STATES (SIS_SMOOTH | SIS_USE_W)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_t0
#define SIS_STATES (SIS_TEXTURE0)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_t0
#define SIS_STATES (SIS_SMOOTH | SIS_TEXTURE0)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_w_t0
#define SIS_STATES (SIS_USE_W | SIS_TEXTURE0)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_w_t0
#define SIS_STATES (SIS_SMOOTH | SIS_USE_W | SIS_TEXTURE0)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_t1
#define SIS_STATES (SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_t1
#define SIS_STATES (SIS_SMOOTH | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_w_t1
#define SIS_STATES (SIS_USE_W | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_w_t1
#define SIS_STATES (SIS_SMOOTH | SIS_USE_W | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_t2
#define SIS_STATES (SIS_TEXTURE0 | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_t2
#define SIS_STATES (SIS_SMOOTH | SIS_TEXTURE0 | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_flat_w_t2
#define SIS_STATES (SIS_USE_W | SIS_TEXTURE0 | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

#define SIS_TAG(x) x##_smooth_w_t2
#define SIS_STATES (SIS_SMOOTH | SIS_USE_W | SIS_TEXTURE0 | SIS_TEXTURE1)
#include "sis_linefunc.h"
#include "sis_trifunc.h"

static line_func sis_draw_line_func[32] = {
  sis_line_flat,
  sis_line_smooth,
  sis_line_flat_w,
  sis_line_smooth_w,
  sis_line_flat_t0,
  sis_line_smooth_t0,
  sis_line_flat_w_t0,
  sis_line_smooth_w_t0,
  sis_line_flat_t1,
  sis_line_smooth_t1,
  sis_line_flat_w_t1,
  sis_line_smooth_w_t1,
  sis_line_flat_t2,
  sis_line_smooth_t2,
  sis_line_flat_w_t2,
  sis_line_smooth_w_t2,
};

static line_func sis_agp_draw_line_func[32] = {
  sis_agp_line_flat,
  sis_agp_line_smooth,
  sis_agp_line_flat_w,
  sis_agp_line_smooth_w,
  sis_agp_line_flat_t0,
  sis_agp_line_smooth_t0,
  sis_agp_line_flat_w_t0,
  sis_agp_line_smooth_w_t0,
  sis_agp_line_flat_t1,
  sis_agp_line_smooth_t1,
  sis_agp_line_flat_w_t1,
  sis_agp_line_smooth_w_t1,
  sis_agp_line_flat_t2,
  sis_agp_line_smooth_t2,
  sis_agp_line_flat_w_t2,
  sis_agp_line_smooth_w_t2,
};

/* TODO: get another path if not clipped */
static void
sis_line_clip (GLcontext * ctx, GLuint vert0, GLuint vert1, GLuint pv)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  BoxPtr pExtents;
  int count;
  GLuint x, y;
  
  sis_get_drawable_origin (xmesa, &x, &y);
  sis_get_clip_rects (xmesa, &pExtents, &count);

  while (count--)
    {
      DWORD clipTopBottom, clipLeftRight;

      clipTopBottom = ((pExtents->y1 - y) << 13) | (pExtents->y2 - y - 1);
      clipLeftRight = ((pExtents->x1 - x) << 13) | (pExtents->x2 - x - 1);

      mWait3DCmdQueue (5);

      MMIO (REG_3D_ClipTopBottom, clipTopBottom);
      MMIO (REG_3D_ClipLeftRight, clipLeftRight);

      (hwcx->LineFunc) (ctx, vert0, vert1, pv);
      mEndPrimitive ();

      pExtents++;
    }
  hwcx->GlobalFlag |= GFLAG_CLIPPING;
}

static triangle_func sis_fill_triangle_func[32] = {
  sis_tri_flat,
  sis_tri_smooth,
  sis_tri_flat_w,
  sis_tri_smooth_w,
  sis_tri_flat_t0,
  sis_tri_smooth_t0,
  sis_tri_flat_w_t0,
  sis_tri_smooth_w_t0,
  sis_tri_flat_t1,
  sis_tri_smooth_t1,
  sis_tri_flat_w_t1,
  sis_tri_smooth_w_t1,
  sis_tri_flat_t2,
  sis_tri_smooth_t2,
  sis_tri_flat_w_t2,
  sis_tri_smooth_w_t2,
};

static triangle_func sis_agp_fill_triangle_func[32] = {
  sis_agp_tri_flat,
  sis_agp_tri_smooth,
  sis_agp_tri_flat_w,
  sis_agp_tri_smooth_w,
  sis_agp_tri_flat_t0,
  sis_agp_tri_smooth_t0,
  sis_agp_tri_flat_w_t0,
  sis_agp_tri_smooth_w_t0,
  sis_agp_tri_flat_t1,
  sis_agp_tri_smooth_t1,
  sis_agp_tri_flat_w_t1,
  sis_agp_tri_smooth_w_t1,
  sis_agp_tri_flat_t2,
  sis_agp_tri_smooth_t2,
  sis_agp_tri_flat_w_t2,
  sis_agp_tri_smooth_w_t2,
};

#define USE_XYZ 0x08000000
#define USE_W   0x04000000
#define USE_RGB 0x01000000
#define USE_UV1 0x00400000
#define USE_UV2 0x00200000
#define USE_FLAT 0x0001000
#define USE_SMOOTH 0x0004000

static DWORD AGPParsingValues[32] = {
  (4 << 28) | USE_XYZ | USE_RGB | USE_FLAT,
  (4 << 28) | USE_XYZ | USE_RGB | USE_SMOOTH,
  (5 << 28) | USE_XYZ | USE_W | USE_RGB | USE_FLAT,
  (5 << 28) | USE_XYZ | USE_W | USE_RGB | USE_SMOOTH,
  (6 << 28) | USE_XYZ | USE_RGB | USE_UV1 | USE_FLAT,
  (6 << 28) | USE_XYZ | USE_RGB | USE_UV1 | USE_SMOOTH,
  (7 << 28) | USE_XYZ | USE_W | USE_RGB | USE_UV1 | USE_FLAT,
  (7 << 28) | USE_XYZ | USE_W | USE_RGB | USE_UV1 | USE_SMOOTH,
  (6 << 28) | USE_XYZ | USE_RGB | USE_UV2 | USE_FLAT,
  (6 << 28) | USE_XYZ | USE_RGB | USE_UV2 | USE_SMOOTH,
  (7 << 28) | USE_XYZ | USE_W | USE_RGB | USE_UV2 | USE_FLAT,
  (7 << 28) | USE_XYZ | USE_W | USE_RGB | USE_UV2 | USE_SMOOTH,
  (8 << 28) | USE_XYZ | USE_RGB | USE_UV1 | USE_UV2 | USE_FLAT,
  (8 << 28) | USE_XYZ | USE_RGB | USE_UV1 | USE_UV2 | USE_SMOOTH,
  (9 << 28) | USE_XYZ | USE_W | USE_RGB | USE_UV1 | USE_UV2 | USE_FLAT,
  (9 << 28) | USE_XYZ | USE_W | USE_RGB | USE_UV1 | USE_UV2 | USE_SMOOTH,
};

static void
sis_tri_clip (GLcontext * ctx, GLuint v0, GLuint v1, GLuint v2, GLuint pv)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  BoxPtr pExtents;
  int count;
  GLuint x, y;

  sis_get_drawable_origin (xmesa, &x, &y);
  sis_get_clip_rects (xmesa, &pExtents, &count);

  while (count--)
    {
      DWORD clipTopBottom, clipLeftRight;

      clipTopBottom = ((pExtents->y1 - y) << 13) | (pExtents->y2 - y - 1);
      clipLeftRight = ((pExtents->x1 - x) << 13) | (pExtents->x2 - x - 1);

      mWait3DCmdQueue (5);

      MMIO (REG_3D_ClipTopBottom, clipTopBottom);
      MMIO (REG_3D_ClipLeftRight, clipLeftRight);

      (hwcx->TriangleFunc) (ctx, v0, v1, v2, pv);
      mEndPrimitive ();

      pExtents++;
    }
  hwcx->GlobalFlag |= GFLAG_CLIPPING;
}

void
sis_set_render_func (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  GLuint caps = ctx->TriangleCaps;
  int line_index = 0;
  int tri_index = 0;

  if(hwcx->swForceRender){
    ctx->Driver.LineFunc = NULL;
    ctx->Driver.TriangleFunc = NULL;    
    return;
  }
      
  ctx->IndirectTriangles &= ~(DD_LINE_SW_RASTERIZE | DD_TRI_SW_RASTERIZE);
  hwcx->swRenderFlag &= ~(SIS_SW_POINT | SIS_SW_LINE | SIS_SW_TRIANGLE);

/* 
 * TODO: Mesa 3.3 will set ctx->TriangleCaps to DD_SELECT
 */
#if 0
  if ((caps & (DD_SELECT | DD_FEEDBACK))
      /* Fixme */
      || (ctx->Texture.ReallyEnabled & (TEXTURE0_3D | TEXTURE1_3D)))
    {
      line_index |= SIS_FALLBACK;
      tri_index |= SIS_FALLBACK;
    }
#endif

  /* always set */
  hwcx->swRenderFlag |= SIS_SW_POINT;

  if ((caps & DD_LINE_STIPPLE) || (caps & DD_LINE_WIDTH))
    {
      line_index |= SIS_FALLBACK;
      hwcx->swRenderFlag |= SIS_SW_LINE;
    }

  if ((caps & DD_TRI_STIPPLE))
    {
      tri_index |= SIS_FALLBACK;
      hwcx->swRenderFlag |= SIS_SW_TRIANGLE;
    }

  if (ctx->Light.ShadeModel == GL_SMOOTH)
    {
      line_index |= SIS_SMOOTH;
      tri_index |= SIS_SMOOTH;
    }

  if (ctx->RasterMask & FOG_BIT)
    {
      line_index |= SIS_USE_W;
      tri_index |= SIS_USE_W;
    }

  if (ctx->Texture.ReallyEnabled)
    {
      line_index |= SIS_USE_W;
      tri_index |= SIS_USE_W;

      if (ctx->Texture.ReallyEnabled & TEXTURE0_ANY)
	{	  
	  line_index |= SIS_TEXTURE0;
	  tri_index |= SIS_TEXTURE0;
	}
      if (ctx->Texture.ReallyEnabled & TEXTURE1_ANY)
	{
	  line_index |= SIS_TEXTURE1;
	  tri_index |= SIS_TEXTURE1;
	}
    }

  /* TODO, use Pick */
  hwcx->UseAGPCmdMode = GL_FALSE;

  if (line_index & SIS_FALLBACK)
    {
      ctx->IndirectTriangles |= DD_LINE_SW_RASTERIZE;
      hwcx->LineFunc = NULL;
    }
  else
    {
      if ((ctx->Color.DriverDrawBuffer == GL_FRONT_LEFT) &&
	  sis_is_window (xmesa))
	{
	  hwcx->LineFunc = sis_draw_line_func[line_index];
	  ctx->Driver.LineFunc = sis_line_clip;
	}
      else
	{
	  if (hwcx->AGPCmdModeEnabled)
	    {
	      ctx->Driver.LineFunc = sis_agp_draw_line_func[line_index];
	      hwcx->UseAGPCmdMode = GL_TRUE;
	    }
	  else
	    {
	      ctx->Driver.LineFunc = sis_draw_line_func[line_index];
	    }
	}
    }

  if (tri_index & SIS_FALLBACK)
    {
      ctx->IndirectTriangles |= DD_TRI_SW_RASTERIZE;
      hwcx->TriangleFunc = NULL;
    }
  else
    {
      if ((ctx->Color.DriverDrawBuffer == GL_FRONT_LEFT) &&
	  sis_is_window (xmesa))
	{
	  hwcx->TriangleFunc = sis_fill_triangle_func[tri_index];
	  ctx->Driver.TriangleFunc = sis_tri_clip;
	}
      else
	{
	  if (hwcx->AGPCmdModeEnabled)
	    {
	      ctx->Driver.TriangleFunc = sis_agp_fill_triangle_func[tri_index];
	      hwcx->UseAGPCmdMode = GL_TRUE;
	    }
	  else
	    {
	      ctx->Driver.TriangleFunc = sis_fill_triangle_func[tri_index];
	    }
	}
    }
	
  /* fast path */
  if(!(ctx->TriangleCaps & (DD_TRI_UNFILLED | DD_TRI_LIGHT_TWOSIDE)) &&
     (ctx->Driver.TriangleFunc == sis_agp_tri_smooth_w_t2) &&
     (ctx->Color.DriverDrawBuffer == GL_BACK_LEFT)){
    hwcx->useFastPath = GL_TRUE;
  }
  else{
    hwcx->useFastPath = GL_FALSE;
  }
	   	       
  /* TODO: AGP and MMIO use different sis_set_render_state */
  hwcx->AGPParseSet &= ~0xffff7000;
  hwcx->AGPParseSet |= AGPParsingValues[line_index & ~SIS_FALLBACK];

  /* Debug, test sw-render
  ctx->Driver.LineFunc = NULL;
  ctx->Driver.TriangleFunc = NULL;  
  hwcx->swRenderFlag = ~0x0;  
  ctx->Visual->DepthMax = (sizeof(GLdepth)==2)?0xffff:0xffffffff;
  ctx->Visual->DepthMaxF = (double)(sizeof(GLdepth)==2)?0xffff:0xffffffff;
  */
}

void
sis_StartAGP (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  DWORD start, end;

  start = *(hwcx->pAGPCmdBufNext);
  end = start + AGP_ALLOC_SIZE;
  if (end >= hwcx->AGPCmdBufSize)
    {
      start = 0;
      end = AGP_ALLOC_SIZE;
    }

  /*
   * TODO: use AGP_EngineOffset to get a safe value and not query current
   * postion processed every time
   * ?? use < instead of <=
   */
  do
    {
      AGP_EngineOffset =
	*(DWORD volatile *) (GET_IOBase (hwcx) + REG_3D_AGPCmBase) -
	(DWORD) hwcx->AGPCmdBufAddr;
    }
  while ((AGP_EngineOffset <= end) && (AGP_EngineOffset >= start)
	 && ((*(GET_IOBase (hwcx) + 0x8243) & 0xe0) != 0xe0));

  AGP_StartPtr = (DWORD *) (start + hwcx->AGPCmdBufBase);
  AGP_CurrentPtr = (float *) AGP_StartPtr;
}

void
sis_FlushAGP (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  /* TODO: wait queue length */

  if((DWORD *)AGP_CurrentPtr == AGP_StartPtr)
    {
      return;
    }

  mWait3DCmdQueue (5);
  mEndPrimitive ();
  MMIO (REG_3D_AGPCmBase, ((DWORD) AGP_StartPtr - (DWORD) hwcx->AGPCmdBufBase)
	+ (DWORD) hwcx->AGPCmdBufAddr);
  MMIO (REG_3D_AGPTtDwNum,
	(((DWORD) AGP_CurrentPtr - (DWORD) AGP_StartPtr) >> 2) | 0x50000000);
  MMIO (REG_3D_ParsingSet, hwcx->AGPParseSet);

  MMIO (REG_3D_AGPCmFire, (DWORD) (-1));
  mEndPrimitive ();

  *(hwcx->pAGPCmdBufNext) =
    (DWORD) AGP_CurrentPtr - (DWORD) hwcx->AGPCmdBufBase;
  *(hwcx->pAGPCmdBufNext) = (*(hwcx->pAGPCmdBufNext) + 0xf) & ~0xf;
}
