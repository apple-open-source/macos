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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_fog.c,v 1.3 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include "sis_ctx.h"
#include "sis_mesa.h"

static DWORD convertFtToFogFt (DWORD dwInValue);

void
sis_Fogfv (GLcontext * ctx, GLenum pname, const GLfloat * params)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  __GLSiSHardware *current = &hwcx->current;

  GLubyte dwFogColor[4];
  DWORD dwArg;
  float fArg;

  switch (pname)
    {
    case GL_FOG_MODE:
      current->hwFog &= ~(FOGMODE_LINEAR | FOGMODE_EXP | FOGMODE_EXP2);
      switch (ctx->Fog.Mode)
	{
	case GL_LINEAR:
	  current->hwFog |= FOGMODE_LINEAR;
	  break;
	case GL_EXP:
	  current->hwFog |= FOGMODE_EXP;
	  break;
	case GL_EXP2:
	  current->hwFog |= FOGMODE_EXP2;
	  break;
	}
      break;
    case GL_FOG_DENSITY:
      dwArg = *(DWORD *) (&(ctx->Fog.Density));
      current->hwFogDensity = 0;
      current->hwFogDensity |= convertFtToFogFt (dwArg);
      break;
    case GL_FOG_START:
    case GL_FOG_END:
      fArg = 1.0 / (ctx->Fog.End - ctx->Fog.Start);
      current->hwFogInverse = doFPtoFixedNoRound (*(DWORD *) (&fArg), 10);
      if (pname == GL_FOG_END)
	{
	  dwArg = *(DWORD *) (&(ctx->Fog.End));
	  if (hwcx->Chipset == PCI_CHIP_SIS300)
	    {
	      current->hwFogFar = doFPtoFixedNoRound (dwArg, 10);
	    }
	  else
	    {
	      current->hwFogFar = doFPtoFixedNoRound (dwArg, 6);
	    }
	}
      break;
    case GL_FOG_INDEX:
      /* TODO */
      break;
    case GL_FOG_COLOR:
      *((DWORD *) dwFogColor) = 0;
      dwFogColor[2] =  (GLubyte)((ctx->Fog.Color[0]) * 255.0);
      dwFogColor[1] =  (GLubyte)((ctx->Fog.Color[1]) * 255.0);
      dwFogColor[0] =  (GLubyte)((ctx->Fog.Color[2]) * 255.0);
      current->hwFog &= 0xff000000;
      current->hwFog |= *((DWORD *) dwFogColor);
      break;
    }
}

DWORD
doFPtoFixedNoRound (DWORD dwInValue, int nFraction)
{
  DWORD dwMantissa;
  int nTemp;

  if (dwInValue == 0)
    return 0;
  nTemp = (int) (dwInValue & 0x7F800000) >> 23;
  nTemp = nTemp - 127 + nFraction - 23;
  dwMantissa = (dwInValue & 0x007FFFFF) | 0x00800000;

  if (nTemp < -25)
    return 0;
  if (nTemp > 0)
    {
      dwMantissa <<= nTemp;
    }
  else
    {
      nTemp = -nTemp;
      dwMantissa >>= nTemp;
    }
  if (dwInValue & 0x80000000)
    {
      dwMantissa = ~dwMantissa + 1;
    }
  return (dwMantissa);
}

/* s[8].23->s[7].10 */
static DWORD
convertFtToFogFt (DWORD dwInValue)
{
  DWORD dwMantissa, dwExp;
  DWORD dwRet;

  if (dwInValue == 0)
    return 0;

  /* ----- Standard float Format: s[8].23                          -----
   * -----     = (-1)^S * 2^(E      - 127) * (1 + M        / 2^23) -----
   * -----     = (-1)^S * 2^((E-63) -  64) * (1 + (M/2^13) / 2^10) -----
   * ----- Density float Format:  s[7].10                          -----
   * -----     New Exponential = E - 63                            -----
   * -----     New Mantissa    = M / 2^13                          -----
   * -----                                                         -----
   */

  dwExp = (dwInValue & 0x7F800000) >> 23;
  dwExp -= 63;

  if ((LONG) dwExp < 0)
    return 0;

  if (dwExp <= 0x7F)
    {
      dwMantissa = (dwInValue & 0x007FFFFF) >> (23 - 10);
    }
  else
    {
      /* ----- To Return +Max(or -Max) ----- */
      dwExp = 0x7F;
      dwMantissa = 0x3FF;
    }

  dwRet = (dwInValue & 0x80000000) >> (31 - 17);  /* Shift Sign Bit */

  dwRet |= (dwExp << 10) | dwMantissa;

  return (dwRet);
}
