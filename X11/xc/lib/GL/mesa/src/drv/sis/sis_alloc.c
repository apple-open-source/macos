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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_alloc.c,v 1.7 2001/01/08 01:07:29 martin Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include <assert.h>

#include "sis_ctx.h"
#include "sis_mesa.h"

#if defined(XFree86Server) && !defined(XF86DRI)
# include "xf86fbman.h"
#else
# define CONFIG_DRM_SIS
# include "drm.h"
# undef CONFIG_DRM_SIS
# include "sis_drm.h"
# include <sys/ioctl.h>
#endif

#define Z_BUFFER_HW_ALIGNMENT 16
#define Z_BUFFER_HW_PLUS (16 + 4)

/* 3D engine uses 2, and bitblt uses 4 */
#define DRAW_BUFFER_HW_ALIGNMENT 16
#define DRAW_BUFFER_HW_PLUS (16 + 4)

#define TEXTURE_HW_ALIGNMENT 4
#define TEXTURE_HW_PLUS (4 + 4)

#ifdef ROUNDUP
#undef ROUNDUP
#endif
#define ROUNDUP(nbytes, pad) (((nbytes)+(pad-1))/(pad))

#ifdef ALIGNMENT
#undef ALIGNMENT
#endif
#define ALIGNMENT(value, align) (ROUNDUP((value),(align))*(align))

#if defined(XFree86Server) && !defined(XF86DRI)

static void *
sis_alloc_fb (__GLSiScontext * hwcx, GLuint size, void **free)
{
  GLcontext *ctx = hwcx->gc;
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;

  ScreenPtr pScreen = xmesa->display;

  GLuint offset;
  BoxPtr pBox;

  size = ROUNDUP (size, GET_DEPTH (hwcx));
  *free = xf86AllocateLinearOffscreenArea (pScreen, size, 1,
					   NULL, NULL, NULL);

  if (!*free)
    return NULL;

  pBox = &((FBAreaPtr) (*free))->box;
  offset = pBox->y1 * GET_PITCH (hwcx) + pBox->x1 * GET_DEPTH (hwcx);

  return GET_FbBase (hwcx) + offset;
}

static void
sis_free_fb (int hHWContext, void *free)
{
  xf86FreeOffscreenArea ((FBAreaPtr) free);
}

#else

int gDRMSubFD = -1;

/* debug */
#if 1

static int _total_video_memory_used = 0;
static int _total_video_memory_count = 0;

static void *
sis_alloc_fb (__GLSiScontext * hwcx, GLuint size, void **free)
{
  GLcontext *ctx = hwcx->gc;
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;

  drm_sis_mem_t fb;

  _total_video_memory_used += size;

  fb.context = xmesa->driContextPriv->hHWContext;
  fb.size = size;
  if(ioctl(hwcx->drmSubFD, SIS_IOCTL_FB_ALLOC, &fb) || !fb.offset)
    return NULL;
  *free = (void *)fb.free;

  /* debug */
  /* memset(fb.offset + GET_FbBase(hwcx), 0xff, size); */

  if (SIS_VERBOSE&VERBOSE_SIS_MEMORY)
  {
    fprintf(stderr, "sis_alloc_fb: size=%u, offset=%lu, pid=%lu, count=%d\n", 
           size, (DWORD)fb.offset, (DWORD)getpid(), 
           ++_total_video_memory_count);
  }

  return (void *)(fb.offset + GET_FbBase(hwcx));
}

static void
sis_free_fb (int hHWContext, void *free)
{
  drm_sis_mem_t fb;

  if (SIS_VERBOSE&VERBOSE_SIS_MEMORY)
  {
    fprintf(stderr, "sis_free_fb: free=%lu, pid=%lu, count=%d\n", 
            (DWORD)free, (DWORD)getpid(), --_total_video_memory_count);
  }
  
  fb.context = hHWContext;
  fb.free = (unsigned long)free;
  ioctl(gDRMSubFD, SIS_IOCTL_FB_FREE, &fb);
}

#else

static void *
sis_alloc_fb (__GLSiScontext * hwcx, GLuint size, void **free)
{
  static char *vidmem_base = 0x400000;
  char *rval = vidmem_base;

  vidmem_base += size;
  if(vidmem_base >= 31*0x100000)
    return NULL;
  
  *free = rval + (DWORD)hwcx->FbBase;

  return rval + (DWORD)hwcx->FbBase;
}

static void
sis_free_fb (int hHWContext, void *free)
{
  return;
}

#endif

#endif

static void *
sis_alloc_agp (__GLSiScontext * hwcx, GLuint size, void **free)
{
  GLcontext *ctx = hwcx->gc;
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;

  drm_sis_mem_t agp;
  
  if(!hwcx->AGPSize)
    return NULL;

  agp.context = xmesa->driContextPriv->hHWContext;
  agp.size = size;
  if(ioctl(hwcx->drmSubFD, SIS_IOCTL_AGP_ALLOC, &agp) || !agp.offset)
    return NULL;
  *free = (void *)agp.free;

  if (SIS_VERBOSE&VERBOSE_SIS_MEMORY)
  {
    fprintf(stderr, "sis_alloc_agp: size=%u, offset=%lu, pid=%lu, count=%d\n", 
           size, (DWORD)agp.offset, (DWORD)getpid(), 
           ++_total_video_memory_count);
  }

  return (void *)(agp.offset + GET_AGPBase(hwcx));
}

static void
sis_free_agp (int hHWContext, void *free)
{
  drm_sis_mem_t agp;

  if (SIS_VERBOSE&VERBOSE_SIS_MEMORY)
  {
    fprintf(stderr, "sis_free_agp: free=%lu, pid=%lu, count=%d\n", 
            (DWORD)free, (DWORD)getpid(), --_total_video_memory_count);
  }
  
  agp.context = hHWContext;
  agp.free = (unsigned long)free;
  ioctl(gDRMSubFD, SIS_IOCTL_AGP_FREE, &agp);
}

/* debug */
static unsigned int Total_Real_Textures_Used = 0;
static unsigned int Total_Textures_Used = 0;

void
sis_alloc_z_stencil_buffer (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;
  sisBufferInfo *priv = (sisBufferInfo *) xm_buffer->private;

  GLuint z_depth;
  GLuint totalBytes;
  int width2;

  GLubyte *addr;

  z_depth = (ctx->Visual->DepthBits + ctx->Visual->StencilBits) / 8;

  width2 = ALIGNMENT (xm_buffer->width * z_depth, 4);

  totalBytes = xm_buffer->height * width2 + Z_BUFFER_HW_PLUS;

  if (xm_buffer->depthbuffer)
    {
      sis_free_z_stencil_buffer (xm_buffer);
    }

  addr = sis_alloc_fb (hwcx, totalBytes, &priv->zbFree);
  if (!addr)
    {
      fprintf (stderr, "SIS driver : out of video memory\n");
      sis_fatal_error ();
    }

  if (SIS_VERBOSE&VERBOSE_SIS_BUFFER)
  {
    fprintf(stderr, "sis_alloc_z_stencil_buffer: addr=%lu\n", (DWORD)addr);
  }

  addr = (GLubyte *) ALIGNMENT ((unsigned long) addr, Z_BUFFER_HW_ALIGNMENT);

  xm_buffer->depthbuffer = (void *) addr;

  /* software render */
  hwcx->swZBase = addr;
  hwcx->swZPitch = width2;

  /* set pZClearPacket */
  memset (priv->pZClearPacket, 0, sizeof (ENGPACKET));

  priv->pZClearPacket->dwSrcPitch = (z_depth == 2) ? 0x80000000 : 0xf0000000;
  priv->pZClearPacket->dwDestBaseAddr =
    (DWORD) addr - (DWORD) GET_FbBase (hwcx);
  priv->pZClearPacket->wDestPitch = width2;
  priv->pZClearPacket->stdwDestPos.wY = 0;
  priv->pZClearPacket->stdwDestPos.wX = 0;

  priv->pZClearPacket->wDestHeight = hwcx->virtualY;
  priv->pZClearPacket->stdwDim.wWidth = (WORD) width2 / z_depth;
  priv->pZClearPacket->stdwDim.wHeight = (WORD) xm_buffer->height;
  priv->pZClearPacket->stdwCmd.cRop = 0xf0;

  if (hwcx->blockWrite)
    {
      priv->pZClearPacket->stdwCmd.cCmd0 = (BYTE) (CMD0_PAT_FG_COLOR);
      priv->pZClearPacket->stdwCmd.cCmd1 =
	(BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
    }
  else
    {
      priv->pZClearPacket->stdwCmd.cCmd0 = 0;
      priv->pZClearPacket->stdwCmd.cCmd1 =
	(BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
    }
}

void
sis_free_z_stencil_buffer (XMesaBuffer buf)
{
  sisBufferInfo *priv = (sisBufferInfo *) buf->private;
  XMesaContext xmesa = buf->xm_context;

  sis_free_fb (xmesa->driContextPriv->hHWContext, priv->zbFree);
  priv->zbFree = NULL;
  buf->depthbuffer = NULL;
}

void
sis_alloc_back_image (GLcontext * ctx, XMesaImage *image, void **free,
                      ENGPACKET *packet)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  XMesaBuffer xm_buffer = xmesa->xm_buffer;

  GLuint depth = GET_DEPTH (hwcx);
  GLuint size, width2;

  GLbyte *addr;

  if (image->data)
    {
      sis_free_back_image (xm_buffer, image, *free);
      *free = NULL;
    }

  width2 = (depth == 2) ? ALIGNMENT (xm_buffer->width, 2) : xm_buffer->width;
  size = width2 * xm_buffer->height * depth + DRAW_BUFFER_HW_PLUS;

  /* Fixme: unique context alloc/free back-buffer? */
  addr = sis_alloc_fb (hwcx, size, free);
  if (!addr)
    {
      fprintf (stderr, "SIS driver : out of video memory\n");
      sis_fatal_error ();
    }

  addr = (GLbyte *) ALIGNMENT ((unsigned long) addr, DRAW_BUFFER_HW_ALIGNMENT);

  image->data = (char *)addr;

  image->bytes_per_line = width2 * depth;
  image->bits_per_pixel = depth * 8;

  memset (packet, 0, sizeof (ENGPACKET));

  packet->dwSrcPitch = (depth == 2) ? 0x80000000 : 0xf0000000;
  packet->dwDestBaseAddr =
    (DWORD) addr - (DWORD) GET_FbBase (hwcx);
  packet->wDestPitch = image->bytes_per_line;
  packet->stdwDestPos.wY = 0;
  packet->stdwDestPos.wX = 0;

  packet->wDestHeight = hwcx->virtualY;
  packet->stdwDim.wWidth = (WORD) width2;
  packet->stdwDim.wHeight = (WORD) xm_buffer->height;
  packet->stdwCmd.cRop = 0xf0;

  if (hwcx->blockWrite)
    {
      packet->stdwCmd.cCmd0 = (BYTE) (CMD0_PAT_FG_COLOR);
      packet->stdwCmd.cCmd1 =
	(BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
    }
  else
    {
      packet->stdwCmd.cCmd0 = 0;
      packet->stdwCmd.cCmd1 = (BYTE) (CMD1_DIR_X_INC | CMD1_DIR_Y_INC);
    }
}

void
sis_free_back_image (XMesaBuffer buf, XMesaImage *image, void *free)
{
  XMesaContext xmesa = buf->xm_context;

  sis_free_fb (xmesa->driContextPriv->hHWContext, free);
  image->data = NULL; 
}

void
sis_alloc_texture_image (GLcontext * ctx, GLtextureImage * image)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  GLuint size;

  SIStextureArea *area = image->DriverData;
  char *addr;

  GLuint texel_size;
  GLenum driver_format;

  if (area)
    sis_free_texture_image (image);

  area = calloc (1, sizeof (SIStextureArea));
  if (!area){
    fprintf (stderr, "SIS Driver : allocating context fails\n");
    sis_fatal_error ();
    return;
  }

  switch (image->IntFormat)
    {
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
      texel_size = 1;
      driver_format = GL_ALPHA8;
      break;
    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:
      texel_size = 1;
      driver_format = GL_LUMINANCE8;
      break;
    case 2:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16:
      texel_size = 2;
      driver_format = GL_LUMINANCE8_ALPHA8;
      break;
    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
      texel_size = 1;
      driver_format = GL_INTENSITY8;
      break;
    case 3:
    case GL_RGB:
    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
      texel_size = 4;
      driver_format = GL_RGB8;
      break;
    case 4:
    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
      texel_size = 4;
      driver_format = GL_RGBA8;
      break;
    default:
      assert(0);
      return;
    }

  size = image->Width * image->Height * texel_size + TEXTURE_HW_PLUS;

  do{
    addr = sis_alloc_fb (hwcx, size, &area->free);
    area->memType = VIDEO_TYPE;
    if(addr) break;
    
    /* TODO: swap to agp memory*/
    /* video memory allocation fails */
    addr = sis_alloc_agp(hwcx, size, &area->free);
    area->memType = AGP_TYPE;
    if(addr) break;
    
    /* TODO: swap to system memory */
  }  
  while(0);

  if (!addr){
    fprintf (stderr, "SIS driver : out of video/agp memory\n");
    sis_fatal_error ();
    return;
  }

  area->Data =
    (GLbyte *) ALIGNMENT ((unsigned long) addr, TEXTURE_HW_ALIGNMENT);
  area->Pitch = image->Width * texel_size;
  area->Format = driver_format;
  area->Size = image->Width * image->Height * texel_size;
  area->texelSize = texel_size;
  area->hHWContext = xmesa->driContextPriv->hHWContext;

  /* debug */
  area->realSize = area->Size;
  Total_Real_Textures_Used += area->realSize;
  Total_Textures_Used++;

  image->DriverData = area;
}

void
sis_free_texture_image (GLtextureImage * image)
{
  SIStextureArea *area = (SIStextureArea *) image->DriverData;

  /* debug */
  Total_Real_Textures_Used -= area->realSize;
  Total_Textures_Used--;

  if (!area)
    return;
  
  if (area->Data)
    switch(area->memType){
    case VIDEO_TYPE:  
      sis_free_fb (area->hHWContext, area->free);
      break;
    case AGP_TYPE:  
      sis_free_agp (area->hHWContext, area->free);
      break;
    default:
      assert(0);
    }

  free (area);
  image->DriverData = NULL;
}
