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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_swzfunc.h,v 1.3 2000/09/26 15:56:49 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

static void SIS_TAG (sis_WriteDepthSpan) (GLcontext * ctx, GLuint n, GLint x,
					  GLint y, const GLdepth depth[],
					  const GLubyte mask[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base = SIS_SW_Z_BASE (x, Y_FLIP (y));

  int i;
  for (i = 0; i < n; i++, base++)
    {
      if (mask[i])
	SIS_SW_I2D (depth[i], *base);
    }
}

static void SIS_TAG (sis_ReadDepthSpan) (GLcontext * ctx, GLuint n, GLint x,
					 GLint y, GLdepth depth[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base = SIS_SW_Z_BASE (x, Y_FLIP (y));

  int i;
  for (i = 0; i < n; i++, base++)
    {
      SIS_SW_D2I (*base, depth[i]);
    }
}

static void SIS_TAG (sis_WriteDepthPixels) (GLcontext * ctx, GLuint n,
					    const GLint x[], const GLint y[],
					    const GLdepth depth[],
					    const GLubyte mask[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base;

  int i;
  for (i = 0; i < n; i++, base++)
    {
      if (mask[i])
	{
	  base = SIS_SW_Z_BASE (x[i], Y_FLIP (y[i]));
	  SIS_SW_I2D (depth[i], *base);
	}
    }
}

static void SIS_TAG (sis_ReadDepthPixels) (GLcontext * ctx, GLuint n,
					   const GLint x[], const GLint y[],
					   GLdepth depth[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base;

  int i;
  for (i = 0; i < n; i++, base++)
    {
      base = SIS_SW_Z_BASE (x[i], Y_FLIP (y[i]));
      SIS_SW_D2I (*base, depth[i]);
    }
}

#ifdef SIS_SW_STENCIL_FUNC

static void SIS_TAG (sis_WriteStencilSpan) (GLcontext * ctx, GLuint n,
					    GLint x, GLint y,
					    const GLstencil depth[],
					    const GLubyte mask[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base = SIS_SW_Z_BASE (x, Y_FLIP (y));

  int i;
  for (i = 0; i < n; i++, base++)
    {
      if (mask[i])
	SIS_SW_I2S (depth[i], *base);
    }
}

static void SIS_TAG (sis_ReadStencilSpan) (GLcontext * ctx, GLuint n, GLint x,
					   GLint y, GLstencil depth[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base = SIS_SW_Z_BASE (x, Y_FLIP (y));

  int i;
  for (i = 0; i < n; i++, base++)
    {
      SIS_SW_S2I (*base, depth[i]);
    }
}

static void SIS_TAG (sis_WriteStencilPixels) (GLcontext * ctx, GLuint n,
					      const GLint x[],
					      const GLint y[],
					      const GLstencil depth[],
					      const GLubyte mask[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base;

  int i;
  for (i = 0; i < n; i++, base++)
    {
      if (mask[i])
	{
	  base = SIS_SW_Z_BASE (x[i], Y_FLIP (y[i]));
	  SIS_SW_I2S (depth[i], *base);
	}
    }
}

static void SIS_TAG (sis_ReadStencilPixels) (GLcontext * ctx, GLuint n,
					     const GLint x[], const GLint y[],
					     GLstencil depth[])
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  SIS_SW_DTYPE *base;

  int i;
  for (i = 0; i < n; i++, base++)
    {
      base = SIS_SW_Z_BASE (x[i], Y_FLIP (y[i]));
      SIS_SW_S2I (*base, depth[i]);
    }
}

# undef SIS_SW_S2I
# undef SIS_SW_I2S

#endif

#undef SIS_TAG
#undef SIS_SW_DTYPE
#undef SIS_SW_D2I
#undef SIS_SW_I2D
