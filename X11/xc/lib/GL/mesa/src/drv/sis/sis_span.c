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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_span.c,v 1.5 2001/03/21 16:14:26 dawes Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"

#define DBG 0

/* from mga */
/* TODO: should lock drawable in these routines because glBitmap will
 *       call this function without locking, or modify sis_Bitmap
 */

#define LOCAL_VARS					\
   XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;  \
   __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private; \
   GLuint pitch = hwcx->swRenderPitch;			\
   char *buf = (char *)hwcx->swRenderBase			

#define CLIPPIXEL(_x,_y) (_x >= minx && _x < maxx && \
			  _y >= miny && _y < maxy)

#define CLIPSPAN( _x, _y, _n, _x1, _n1, _i )				\
   if ( _y < miny || _y >= maxy ) {					\
      _n1 = 0, _x1 = x;							\
   } else {								\
      _n1 = _n;								\
      _x1 = _x;								\
      if ( _x1 < minx ) _i += (minx-_x1), n1 -= (minx-_x1), _x1 = minx; \
      if ( _x1 + _n1 >= maxx ) n1 -= (_x1 + n1 - maxx);		        \
   }

#define HW_LOCK() do{}while(0);

#define HW_CLIPLOOP()						\
  do {								\
    BoxPtr _pExtents;                                           \
    int _nc;                                                    \
    GLuint _x, _y;                    			        \
    sis_get_drawable_origin (xmesa, &_x, &_y);                  \
    sis_get_clip_rects (xmesa, &_pExtents, &_nc);               \
    while (_nc--) {						\
       int minx = _pExtents->x1 - _x;	\
       int miny = _pExtents->y1 - _y; 	\
       int maxx = _pExtents->x2 - _x;	\
       int maxy = _pExtents->y2 - _y;   \
       _pExtents++;
       
#define HW_ENDCLIPLOOP()			\
    }						\
  } while (0)

#define HW_UNLOCK() do{}while(0);

/* RGB565 */
#define INIT_MONO_PIXEL(p) \
   GLushort p = hwcx->pixelValue;

#define WRITE_RGBA( _x, _y, r, g, b, a )				\
   *(GLushort *)(buf + _x*2 + _y*pitch)  = ( ((r & 0xf8) << 8) |	\
		                             ((g & 0xfc) << 3) |	\
		                             (b >> 3))

#define WRITE_PIXEL( _x, _y, p )  \
   *(GLushort *)(buf + _x*2 + _y*pitch) = p

#define READ_RGBA( rgba, _x, _y )			\
do {							\
   GLushort p = *(GLushort *)(buf + _x*2 + _y*pitch);	\
   rgba[0] = (p & 0xf800) >> 8;				\
   rgba[1] = (p & 0x07e0) >> 3;			        \
   rgba[2] = (p & 0x001f) << 3;			        \
   rgba[3] = 0;			         		\
} while(0)

#define TAG(x) sis_##x##_565
#include "spantmp.h"
static void sis_Color_565( GLcontext *ctx,
                  GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha )
{
   XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
   __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
   
   hwcx->pixelValue = ((red & 0xf8) << 8) |
                      ((green & 0xfc) << 3) |
                      (blue >> 3);
}                  


/* ARGB8888 */
#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p) \
   GLuint p = hwcx->pixelValue;

#define WRITE_RGBA( _x, _y, r, g, b, a )			\
   *(GLuint *)(buf + _x*4 + _y*pitch)  = ( ((a) << 24) |        \
                                           ((r) << 16) |	\
		                           ((g) << 8) |		\
                         		   ((b)))

#define WRITE_PIXEL( _x, _y, p )  \
   *(GLuint *)(buf + _x*4 + _y*pitch)  = p

#define READ_RGBA( rgba, _x, _y )			\
do {							\
   GLuint p = *(GLuint *)(buf + _x*4 + _y*pitch);	\
   rgba[0] = (p >> 16) & 0xff;				\
   rgba[1] = (p >> 8) & 0xff;				\
   rgba[2] = (p >> 0) & 0xff;				\
   rgba[3] = (p >> 24) & 0xff;			        \
} while(0)

#define TAG(x) sis_##x##_8888
#include "spantmp.h"
static void sis_Color_8888( GLcontext *ctx,
                  GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha )
{
   XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
   __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
   
   hwcx->pixelValue = (red << 16) |
                      (green << 8) |
                      (blue) |
                      (alpha << 24);
}                  

void sis_sw_init_driver( GLcontext *ctx )
{
   XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
   __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

   if (hwcx->colorFormat == DST_FORMAT_RGB_565) {
      ctx->Driver.Color = sis_Color_565;      
      ctx->Driver.WriteRGBASpan = sis_WriteRGBASpan_565;
      ctx->Driver.WriteRGBSpan = sis_WriteRGBSpan_565;
      ctx->Driver.WriteMonoRGBASpan = sis_WriteMonoRGBASpan_565;
      ctx->Driver.WriteRGBAPixels = sis_WriteRGBAPixels_565;
      ctx->Driver.WriteMonoRGBAPixels = sis_WriteMonoRGBAPixels_565;
      ctx->Driver.ReadRGBASpan = sis_ReadRGBASpan_565;
      ctx->Driver.ReadRGBAPixels = sis_ReadRGBAPixels_565;
   } 
   else if(hwcx->colorFormat == DST_FORMAT_ARGB_8888){
      ctx->Driver.Color = sis_Color_8888;      
      ctx->Driver.WriteRGBASpan = sis_WriteRGBASpan_8888;
      ctx->Driver.WriteRGBSpan = sis_WriteRGBSpan_8888;
      ctx->Driver.WriteMonoRGBASpan = sis_WriteMonoRGBASpan_8888;
      ctx->Driver.WriteRGBAPixels = sis_WriteRGBAPixels_8888;
      ctx->Driver.WriteMonoRGBAPixels = sis_WriteMonoRGBAPixels_8888;
      ctx->Driver.ReadRGBASpan = sis_ReadRGBASpan_8888;
      ctx->Driver.ReadRGBAPixels = sis_ReadRGBAPixels_8888;
   }
   else{
     assert(0);
   }

   ctx->Driver.WriteCI8Span        =NULL;
   ctx->Driver.WriteCI32Span       =NULL;
   ctx->Driver.WriteMonoCISpan     =NULL;
   ctx->Driver.WriteCI32Pixels     =NULL;
   ctx->Driver.WriteMonoCIPixels   =NULL;
   ctx->Driver.ReadCI32Span        =NULL;
   ctx->Driver.ReadCI32Pixels      =NULL;
}

/* Depth/Stencil Functions
 * use sizeof(GLdepth) to know the Z rnage of mesa is
 * 0 ~ 2^16-1 or 0 ~ 2^32-1
 */
#define SIS_SW_Z_BASE(x,y) \
   ((SIS_SW_DTYPE *)(hwcx->swZBase + (x)*sizeof(SIS_SW_DTYPE) + \
       (y)*hwcx->swZPitch))

/* Z16 */
#define SIS_TAG(x) x##_Z16
#define SIS_SW_DTYPE GLushort
#define SIS_SW_D2I(D,I) \
  do{ \
    if(sizeof(GLdepth) == 2) \
      I = D; \
    else \
      I = D << 16; \
  }while(0)
#define SIS_SW_I2D(I,D) \
  do{ \
    if(sizeof(GLdepth) == 2) \
      D = I; \
    else \
      D = I >> 16; \
  }while(0)
#include "sis_swzfunc.h"

/* Z32 */
#define SIS_TAG(x) x##_Z32
#define SIS_SW_DTYPE GLuint
#define SIS_SW_D2I(D,I) \
  do{ \
    if(sizeof(GLdepth) == 4) \
      I = D; \
    else \
      I = D >> 16; \
  }while(0)
#define SIS_SW_I2D(I,D) \
  do{ \
    if(sizeof(GLdepth) == 4) \
      D = I; \
    else \
      D = I << 16; \
  }while(0)
#include "sis_swzfunc.h"

#define SIS_SW_STENCIL_FUNC

/* S8Z24 */
# define SIS_TAG(x) x##_S8Z24
# define SIS_SW_DTYPE GLuint
# define SIS_SW_D2I(D,I) \
   do{ \
    if(sizeof(GLdepth) == 4) \
      I = (D << 8); \
    else \
      I = (D >> 8); \
   }while(0)
# define SIS_SW_I2D(I,D) \
   do{ \
    if(sizeof(GLdepth) == 4){ \
      D &= 0xff000000; \
      D |= (I >> 8); \
    } \
    else{ \
      D &= 0xff000000; \
      D |= (I << 8); \
    } \
   }while(0)
# define SIS_SW_S2I(S,I) \
   do{ \
     I = (S >> 24); \
   }while(0)
# define SIS_SW_I2S(I,S) \
   do{ \
     S &= 0x00ffffff; \
     S |= (I << 24); \
   }while(0)
# include "sis_swzfunc.h"

#undef SIS_SW_STENCIL_FUNC

void
sis_sw_set_zfuncs_static (GLcontext * ctx)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  switch (hwcx->zFormat)
    {
    case Z_16:
      ctx->Driver.ReadDepthSpan = sis_ReadDepthSpan_Z16;
      ctx->Driver.ReadDepthPixels = sis_ReadDepthPixels_Z16;
      ctx->Driver.WriteDepthSpan = sis_WriteDepthSpan_Z16;
      ctx->Driver.WriteDepthPixels = sis_WriteDepthPixels_Z16;

      ctx->Driver.ReadStencilSpan = NULL;
      ctx->Driver.ReadStencilPixels = NULL;
      ctx->Driver.WriteStencilSpan = NULL;
      ctx->Driver.WriteStencilPixels = NULL;
      break;
    case Z_32:
      ctx->Driver.ReadDepthSpan = sis_ReadDepthSpan_Z32;
      ctx->Driver.ReadDepthPixels = sis_ReadDepthPixels_Z32;
      ctx->Driver.WriteDepthSpan = sis_WriteDepthSpan_Z32;
      ctx->Driver.WriteDepthPixels = sis_WriteDepthPixels_Z32;

      ctx->Driver.ReadStencilSpan = NULL;
      ctx->Driver.ReadStencilPixels = NULL;
      ctx->Driver.WriteStencilSpan = NULL;
      ctx->Driver.WriteStencilPixels = NULL;
      break;
    case S_8_Z_24:
      ctx->Driver.ReadDepthSpan = sis_ReadDepthSpan_S8Z24;
      ctx->Driver.ReadDepthPixels = sis_ReadDepthPixels_S8Z24;
      ctx->Driver.WriteDepthSpan = sis_WriteDepthSpan_S8Z24;
      ctx->Driver.WriteDepthPixels = sis_WriteDepthPixels_S8Z24;

      ctx->Driver.ReadStencilSpan = sis_ReadStencilSpan_S8Z24;
      ctx->Driver.ReadStencilPixels = sis_ReadStencilPixels_S8Z24;
      ctx->Driver.WriteStencilSpan = sis_WriteStencilSpan_S8Z24;
      ctx->Driver.WriteStencilPixels = sis_WriteStencilPixels_S8Z24;
      break;
    }
}
