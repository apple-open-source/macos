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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_clear.c,v 1.5 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"
#include "sis_lock.h"

__inline__ static GLbitfield sis_3D_Clear (GLcontext * ctx, GLbitfield mask,
					   GLint x, GLint y, GLint width,
					   GLint height);
__inline__ static void sis_clear_color_buffer (GLcontext * ctx, GLint x,
					       GLint y, GLint width,
					       GLint height);
__inline__ static void sis_clear_z_stencil_buffer (GLcontext * ctx,
						   GLbitfield mask, GLint x,
						   GLint y, GLint width,
						   GLint height);

GLbitfield
sis_Clear (GLcontext * ctx, GLbitfield mask, GLboolean all,
	   GLint x, GLint y, GLint width, GLint height)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;
  GLint x1, y1, width1, height1;

  if (all)
    {
      GLframebuffer *buffer = ctx->DrawBuffer;

      x1 = 0;
      y1 = 0;
      width1 = buffer->Width;
      height1 = buffer->Height;
    }
  else
    {
      x1 = x;
      y1 = Y_FLIP(y+height-1);
      width1 = width;            
      height1 = height;
    }

  LOCK_HARDWARE ();

  /* 
   * TODO: no considering multiple-buffer and clear buffer
   * differs from current draw buffer 
   */

  if ((ctx->Visual->StencilBits &&
       ((mask | GL_DEPTH_BUFFER_BIT) ^ (mask | GL_STENCIL_BUFFER_BIT))) 
      || (*(DWORD *) (ctx->Color.ColorMask) != 0xffffffff)
    )
    {
      /* only Clear either depth or stencil buffer */ 
      mask = sis_3D_Clear (ctx, mask, x1, y1, width1, height1);
    }

  if (mask & ctx->Color.DrawDestMask)
    {
      sis_clear_color_buffer (ctx, x1, y1, width1, height1);
      mask &= ~ctx->Color.DrawDestMask;
    }
  if (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
    {
      if (xm_buffer->depthbuffer)
	sis_clear_z_stencil_buffer (ctx, mask, x1, y1, width1, height1);
      mask &= ~(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
  if (mask & GL_ACCUM_BUFFER_BIT)
    {
    }

  UNLOCK_HARDWARE ();

  return mask;
}

void
sis_ClearDepth (GLcontext * ctx, GLclampd d)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  set_z_stencil_pattern (hwcx, d, ctx->Stencil.Clear);
}

void
sis_ClearStencil (GLcontext * ctx, GLint s)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  set_z_stencil_pattern (hwcx, ctx->Depth.Clear, s);

}

#define MAKE_CLEAR_COLOR_8888(cc)   \
            ( (((DWORD)(((GLubyte *)(cc))[3] * 255.0 + 0.5))<<24) | \
              (((DWORD)(((GLubyte *)(cc))[0] * 255.0 + 0.5))<<16) | \
              (((DWORD)(((GLubyte *)(cc))[1] * 255.0 + 0.5))<<8) | \
               ((DWORD)(((GLubyte *)(cc))[2] * 255.0 + 0.5)) )

__inline__ static GLbitfield
sis_3D_Clear (GLcontext * ctx, GLbitfield mask,
	      GLint x, GLint y, GLint width, GLint height)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  __GLSiSHardware *current = &hwcx->current;

#define RESETSTATE (GFLAG_ENABLESETTING | GFLAG_ENABLESETTING2 | \
                    GFLAG_ZSETTING | GFLAG_DESTSETTING | \
                    GFLAG_STENCILSETTING | GFLAG_CLIPPING)
#define STEN_OP (SiS_SFAIL_REPLACE | SiS_SPASS_ZFAIL_REPLACE | \
                 SiS_SPASS_ZPASS_REPLACE)

  float left, top, right, bottom, zClearVal;
  DWORD dwColor=0;
  DWORD bClrColor, bClrDepth, bClrStencil;
  DWORD dwPrimitiveSet;
  DWORD dwEnable1, dwEnable2, dwDepthMask=0, dwSten1=0, dwSten2=0;

  int count;
  BoxPtr pExtents;

  bClrColor = 0;
  bClrDepth = (mask & GL_DEPTH_BUFFER_BIT) && 
              (ctx->Visual->DepthBits);
  bClrStencil = (mask & GL_STENCIL_BUFFER_BIT) && 
                (ctx->Visual->StencilBits);

  /* update HW state */
  /* TODO: if enclosing sis_Clear by sis_RenderStart and sis_RenderEnd is
   * uniform, but it seems needless to do so
   */
  if (hwcx->GlobalFlag)
    {
      sis_update_render_state (hwcx, 0);
    }

  dwEnable2 = 0;
  if (bClrColor)
    {
      dwColor = MAKE_CLEAR_COLOR_8888 (ctx->Color.ClearColor);
    }
  else
    {
      dwEnable2 |= 0x8000;
    }

  if (bClrDepth && bClrStencil)
    {
      DWORD wmask, smask;
      GLstencil sten;

      zClearVal = ctx->Depth.Clear;
      sten = ctx->Stencil.Clear;
      wmask = (DWORD) ctx->Stencil.WriteMask;
      smask = 0xff;

      dwEnable1 = MASK_ZWriteEnable | MASK_StencilTestEnable;
      dwEnable2 |= MASK_ZMaskWriteEnable;
      dwDepthMask = ((wmask << 24) | 0x00ffffff);
      dwSten1 = S_8 | (((DWORD) sten << 8) | smask) | SiS_STENCIL_ALWAYS;
      dwSten2 = STEN_OP;
    }
  else if (bClrDepth)
    {
      zClearVal = ctx->Depth.Clear;
      dwEnable1 = MASK_ZWriteEnable;
      dwEnable2 |= MASK_ZMaskWriteEnable;
      dwDepthMask = 0xffffff;
    }
  else if (bClrStencil)
    {
      DWORD wmask, smask;
      GLstencil sten;

      sten = (GLstencil) ctx->Stencil.Clear;
      wmask = (DWORD) ctx->Stencil.WriteMask;
      smask = 0xff;

      zClearVal = 0;
      dwEnable1 = MASK_ZWriteEnable | MASK_StencilTestEnable;
      dwEnable2 |= MASK_ZMaskWriteEnable;
      dwDepthMask = (wmask << 24) & 0xff000000;
      dwSten1 = S_8 | (((DWORD) sten << 8) | smask) | SiS_STENCIL_ALWAYS;
      dwSten2 = STEN_OP;
    }
  else
    {
      dwEnable2 &= ~MASK_ZMaskWriteEnable;
      dwEnable1 = 0L;
      zClearVal = 1;
    }

  mWait3DCmdQueue (35);
  MMIO (REG_3D_TEnable, dwEnable1);
  MMIO (REG_3D_TEnable2, dwEnable2);
  if (bClrDepth | bClrStencil)
    {
      MMIO (REG_3D_ZSet, current->hwZ);
      MMIO (REG_3D_ZStWriteMask, dwDepthMask);
      MMIO (REG_3D_ZAddress, current->hwOffsetZ);
    }
  if (bClrColor)
    {
      MMIO (REG_3D_DstSet, (current->hwDstSet & 0x00ffffff) | 0xc000000);
      MMIO (REG_3D_DstAddress, current->hwOffsetDest);
    }
  else
    {
      MMIO (REG_3D_DstAlphaWriteMask, 0L);
    }
  if (bClrStencil)
    {
      MMIO (REG_3D_StencilSet, dwSten1);
      MMIO (REG_3D_StencilSet2, dwSten2);
    }

  if (ctx->Color.DriverDrawBuffer == GL_FRONT_LEFT)
    {
      sis_get_clip_rects (xmesa, &pExtents, &count);
    }
  else
    {
      pExtents = NULL;
      count = 1;
    }

  while(count--)
    {
      left = x;
      right = x + width - 1;
      top = y;
      bottom = y + height - 1;

      if (pExtents)
	{
	  GLuint origin_x, origin_y;
	  GLuint x1, y1, x2, y2;

	  sis_get_drawable_origin (xmesa, &origin_x, &origin_y);

	  x1 = pExtents->x1 - origin_x;
	  y1 = pExtents->y1 - origin_y;
	  x2 = pExtents->x2 - origin_x - 1;
	  y2 = pExtents->y2 - origin_y - 1;

	  left = (left > x1) ? left : x1;
	  right = (right > x2) ? x2 : right;
	  top = (top > y1) ? top : y1;
	  bottom = (bottom > y2) ? y2 : bottom;
	  if (left > right || top > bottom)
	    continue;
	  pExtents++;
	}

      MMIO (REG_3D_ClipTopBottom, ((DWORD) top << 13) | (DWORD) bottom);
      MMIO (REG_3D_ClipLeftRight, ((DWORD) left << 13) | (DWORD) right);

      /* the first triangle */
      dwPrimitiveSet = (OP_3D_TRIANGLE_DRAW | OP_3D_FIRE_TSARGBc | 
                        SHADE_FLAT_VertexC);
      MMIO (REG_3D_PrimitiveSet, dwPrimitiveSet);

      MMIO (REG_3D_TSZa, *(DWORD *) & zClearVal);
      MMIO (REG_3D_TSXa, *(DWORD *) & right);
      MMIO (REG_3D_TSYa, *(DWORD *) & top);
      MMIO (REG_3D_TSARGBa, dwColor);

      MMIO (REG_3D_TSZb, *(DWORD *) & zClearVal);
      MMIO (REG_3D_TSXb, *(DWORD *) & left);
      MMIO (REG_3D_TSYb, *(DWORD *) & top);
      MMIO (REG_3D_TSARGBb, dwColor);

      MMIO (REG_3D_TSZc, *(DWORD *) & zClearVal);
      MMIO (REG_3D_TSXc, *(DWORD *) & left);
      MMIO (REG_3D_TSYc, *(DWORD *) & bottom);
      MMIO (REG_3D_TSARGBc, dwColor);

      /* second triangle */
      dwPrimitiveSet = (OP_3D_TRIANGLE_DRAW | OP_3D_FIRE_TSARGBb |
                        SHADE_FLAT_VertexB);
      MMIO (REG_3D_PrimitiveSet, dwPrimitiveSet);

      MMIO (REG_3D_TSZb, *(DWORD *) & zClearVal);
      MMIO (REG_3D_TSXb, *(DWORD *) & right);
      MMIO (REG_3D_TSYb, *(DWORD *) & bottom);
      MMIO (REG_3D_TSARGBb, dwColor);
    }

  mEndPrimitive ();

  hwcx->GlobalFlag |= RESETSTATE;

#undef RESETSTATE
#undef STEN_OP
  
  /* 
   * TODO: will mesa call several times if multiple draw buffer
   */
  return (mask & ~(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}

__inline__ static void
sis_bitblt_clear_cmd (__GLSiScontext * hwcx, ENGPACKET * pkt)
{
  LPDWORD lpdwDest, lpdwSrc;
  int i;

  lpdwSrc = (DWORD *) pkt + 1;
  lpdwDest = (DWORD *) (GET_IOBase (hwcx) + REG_SRC_ADDR) + 1;

  mWait3DCmdQueue (10);

  *lpdwDest++ = *lpdwSrc++;
  lpdwSrc++;
  lpdwDest++;
  for (i = 3; i < 8; i++)
    {
      *lpdwDest++ = *lpdwSrc++;
    }

  MMIO (REG_CMD0, *(DWORD *) & pkt->stdwCmd);
  MMIO (0x8240, -1);

}

__inline__ static void
sis_clear_color_buffer (GLcontext * ctx,
			GLint x, GLint y, GLint width, GLint height)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;
  sisBufferInfo *priv = (sisBufferInfo *) xm_buffer->private;

  int count;
  GLuint origin_x, origin_y;
  GLuint depth = GET_DEPTH (hwcx);
  BoxPtr pExtents = NULL;
  GLint xx, yy;
  GLint x0, y0, width0, height0;

  ENGPACKET stEngPacket;

  GLuint pitch;

  switch (ctx->Color.DriverDrawBuffer)
    {
    case GL_BACK_LEFT:
      priv->pCbClearPacket->stdwDestPos.wY = y;
      priv->pCbClearPacket->stdwDestPos.wX = x;
      priv->pCbClearPacket->stdwDim.wWidth = (WORD) width;
      priv->pCbClearPacket->stdwDim.wHeight = (WORD) height;
      priv->pCbClearPacket->dwFgRopColor = hwcx->clearColorPattern;

      sis_bitblt_clear_cmd (hwcx, priv->pCbClearPacket);
      return;
    case GL_FRONT_LEFT:
      x0 = x;
      y0 = y;
      width0 = width;
      height0 = height;

      pitch = GET_PITCH (hwcx);
      sis_get_drawable_origin (xmesa, &origin_x, &origin_y);
      sis_get_clip_rects (xmesa, &pExtents, &count);
      break;
    case GL_BACK_RIGHT:
    case GL_FRONT_RIGHT:
    default:
      assert (0);
      return;
    }

  memset (&stEngPacket, 0, sizeof (ENGPACKET));

  while (count--)
    {
      GLint x2 = pExtents->x1 - origin_x;
      GLint y2 = pExtents->y1 - origin_y;
      GLint xx2 = pExtents->x2 - origin_x;
      GLint yy2 = pExtents->y2 - origin_y;

      x = (x0 > x2) ? x0 : x2;
      y = (y0 > y2) ? y0 : y2;
      xx = ((x0 + width0) > (xx2)) ? xx2 : x0 + width0;
      yy = ((y0 + height0) > (yy2)) ? yy2 : y0 + height0;
      width = xx - x;
      height = yy - y;
      pExtents++;

      if (width <= 0 || height <= 0)
	continue;

      stEngPacket.dwSrcPitch = (depth == 2) ? 0x80000000 : 0xc0000000;
      stEngPacket.stdwDestPos.wY = y + origin_y;
      stEngPacket.stdwDestPos.wX = x + origin_x;
      stEngPacket.dwDestBaseAddr = (DWORD) 0;
      stEngPacket.wDestPitch = pitch;
      /* TODO: set maximum value? */
      stEngPacket.wDestHeight = hwcx->virtualY;
      stEngPacket.stdwDim.wWidth = (WORD) width;
      stEngPacket.stdwDim.wHeight = (WORD) height;
      stEngPacket.stdwCmd.cRop = 0xf0;
      stEngPacket.dwFgRopColor = hwcx->clearColorPattern;

      /* for SGRAM Block Write Enable */
      if (hwcx->blockWrite)
	{
	  stEngPacket.stdwCmd.cCmd0 = (BYTE) (CMD0_PAT_FG_COLOR);
	  stEngPacket.stdwCmd.cCmd1 =
	    (BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
	}
      else
	{
	  stEngPacket.stdwCmd.cCmd0 = 0;
	  stEngPacket.stdwCmd.cCmd1 =
	    (BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
	}

      sis_bitblt_clear_cmd (hwcx, &stEngPacket);
    }
}

__inline__ static void
sis_clear_z_stencil_buffer (GLcontext * ctx, GLbitfield mask,
			    GLint x, GLint y, GLint width, GLint height)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;
  sisBufferInfo *priv = (sisBufferInfo *) xmesa->xm_buffer->private;

  /* TODO: check write mask */

  if (!xm_buffer->depthbuffer)
    return;

  /* TODO: consider alignment of width, height? */
  priv->pZClearPacket->stdwDestPos.wY = y;
  priv->pZClearPacket->stdwDestPos.wX = x;
  priv->pZClearPacket->stdwDim.wWidth = (WORD) width;
  priv->pZClearPacket->stdwDim.wHeight = (WORD) height;
  priv->pZClearPacket->dwFgRopColor = hwcx->clearZStencilPattern;

  sis_bitblt_clear_cmd (hwcx, priv->pZClearPacket);
}

__inline__ static void
sis_bitblt_copy_cmd (__GLSiScontext * hwcx, ENGPACKET * pkt)
{
  LPDWORD lpdwDest, lpdwSrc;
  int i;

  lpdwSrc = (DWORD *) pkt;
  lpdwDest = (DWORD *) (GET_IOBase (hwcx) + REG_SRC_ADDR);

  mWait3DCmdQueue (10);

  for (i = 0; i < 7; i++)
    {
      *lpdwDest++ = *lpdwSrc++;
    }

  MMIO (REG_CMD0, *(DWORD *) & pkt->stdwCmd);
  MMIO (0x8240, -1);
}

__inline__ static void
sis_swap_image (XMesaBuffer b, XMesaDrawable d, XMesaImage * image)
{
  XMesaContext xmesa = b->xm_context;
  __GLSiScontext *hwcx = (__GLSiScontext *) b->xm_context->private;

  GLuint depth = GET_DEPTH (hwcx);
  ENGPACKET stEngPacket;
  DWORD src;
  GLuint srcPitch, dstPitch;

  BoxPtr pExtents;
  BoxRec box;
  int count;
  GLuint origin_x, origin_y;

  memset (&stEngPacket, 0, sizeof (ENGPACKET));

  if (!sis_get_clip_rects (xmesa, &pExtents, &count))
    {
      sis_get_drawable_box (xmesa, &box);
      pExtents = &box;
      count = 1;
    }

  src = (DWORD) image->data - (DWORD) GET_FbBase (hwcx);
  srcPitch = image->bytes_per_line;
  dstPitch = GET_PITCH (hwcx);
  sis_get_drawable_origin (xmesa, &origin_x, &origin_y);

  while(count --)
    {
      stEngPacket.dwSrcPitch = (depth == 2) ? 0x80000000 : 0xc0000000;

      stEngPacket.dwSrcBaseAddr = src;
      stEngPacket.dwSrcPitch |= srcPitch;

      stEngPacket.stdwSrcPos.wY = pExtents->y1 - origin_y;
      stEngPacket.stdwSrcPos.wX = pExtents->x1 - origin_x;
      stEngPacket.stdwDestPos.wY = pExtents->y1;
      stEngPacket.stdwDestPos.wX = pExtents->x1;
      stEngPacket.dwDestBaseAddr = (DWORD) 0;
      stEngPacket.wDestPitch = dstPitch;

      /* TODO: set maximum value? */
      stEngPacket.wDestHeight = hwcx->virtualY;
      stEngPacket.stdwDim.wWidth = (WORD) pExtents->x2 - pExtents->x1;
      stEngPacket.stdwDim.wHeight = (WORD) pExtents->y2 - pExtents->y1;
      stEngPacket.stdwCmd.cRop = 0xcc;

      if (hwcx->blockWrite)
	{
	  stEngPacket.stdwCmd.cCmd0 = (BYTE) (CMD0_PAT_FG_COLOR);
	  stEngPacket.stdwCmd.cCmd1 =
	    (BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
	}
      else
	{
	  stEngPacket.stdwCmd.cCmd0 = 0;
	  stEngPacket.stdwCmd.cCmd1 =
	    (BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
	}

      sis_bitblt_copy_cmd (hwcx, &stEngPacket);

      pExtents++;
    }
}

void
sis_swap_buffers (XMesaBuffer b)
{
  XMesaContext xmesa = b->xm_context;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  GLcontext *ctx = hwcx->gc;

  /* debug */
  /* return; */

  /* frame control */
  /* TODO: need lock? */
  
#if 1
  {
    int repeat = 0;
    
    while(((*hwcx->FrameCountPtr) - *(DWORD volatile *)(hwcx->IOBase+0x8a2c) 
          > SIS_MAX_FRAME_LENGTH) && 
          (repeat++ < 10));
  }
#endif

  LOCK_HARDWARE ();

  sis_swap_image (b, b->frontbuffer, b->backimage);
  *(DWORD *)(hwcx->IOBase+0x8a2c) = *hwcx->FrameCountPtr;
  (*hwcx->FrameCountPtr)++;  
   
  UNLOCK_HARDWARE ();
}
