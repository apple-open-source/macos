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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_linefunc.h,v 1.5 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#define SIS_TRI_FUNC 0

#define SIS_MMIO_WRITE_VERTEX(v, i) \
  if(SIS_STEREO && hwcx->stereoEnabled) \
  { \
    MMIOBase[(REG_3D_TSXa+(i)*0x30)/4] = VB->Win.data[v][0] - 0.5 + STEREO_OFFSET(v); \
    MMIOBase[(REG_3D_TSYa+(i)*0x30)/4] = Y_FLIP (VB->Win.data[v][1]) + 0.5; \
  } \
  else \
  { \
    MMIOBase[(REG_3D_TSXa+(i)*0x30)/4] = VB->Win.data[v][0] - 0.5 ; \
    MMIOBase[(REG_3D_TSYa+(i)*0x30)/4] = Y_FLIP (VB->Win.data[v][1]) + 0.5; \
  } \
\
  if (ctx->TriangleCaps & DD_TRI_OFFSET) \
    { \
      if(SIS_TRI_FUNC){ \
        MMIOBase[(REG_3D_TSZa+(i)*0x30)/4] = \
  	(VB->Win.data[v][2] + ctx->PolygonZoffset) / SIS_DEPTH_SCALE; \
      } \
      else{ \
        MMIOBase[(REG_3D_TSZa+(i)*0x30)/4] = \
  	(VB->Win.data[v][2] + ctx->LineZoffset) / SIS_DEPTH_SCALE; \
      } \
    } \
  else \
    { \
      MMIOBase[(REG_3D_TSZa+(i)*0x30)/4] = VB->Win.data[v][2] / SIS_DEPTH_SCALE; \
    } \
\
  if (SIS_STATES & SIS_TEXTURE0) \
    { \
      if(VB->TexCoordPtr[0]->size == 4){ \
        MMIOBase[(REG_3D_TSUAa+(i)*0x30)/4] = VB->TexCoordPtr[0]->data[v][0] / \
                                   VB->TexCoordPtr[0]->data[v][3]; \
        MMIOBase[(REG_3D_TSVAa+(i)*0x30)/4] = VB->TexCoordPtr[0]->data[v][1] / \
                                   VB->TexCoordPtr[0]->data[v][3]; \
      } \
      else{ \
        MMIOBase[(REG_3D_TSUAa+(i)*0x30)/4] = VB->TexCoordPtr[0]->data[v][0]; \
        MMIOBase[(REG_3D_TSVAa+(i)*0x30)/4] = VB->TexCoordPtr[0]->data[v][1]; \
      } \
    } \
\
  if (SIS_STATES & SIS_TEXTURE1) \
    { \
      if(VB->TexCoordPtr[1]->size == 4){ \
        MMIOBase[(REG_3D_TSUBa+(i)*0x30)/4] = VB->TexCoordPtr[1]->data[v][0] / \
                                   VB->TexCoordPtr[1]->data[v][3]; \
        MMIOBase[(REG_3D_TSVBa+(i)*0x30)/4] = VB->TexCoordPtr[1]->data[v][1] / \
                                   VB->TexCoordPtr[1]->data[v][3]; \
      } \
      else{ \
        MMIOBase[(REG_3D_TSUBa+(i)*0x30)/4] = VB->TexCoordPtr[1]->data[v][0]; \
        MMIOBase[(REG_3D_TSVBa+(i)*0x30)/4] = VB->TexCoordPtr[1]->data[v][1]; \
      } \
    } \
\
  if (SIS_STATES & (SIS_USE_W)) \
    { \
      if(VB->TexCoordPtr[0]->size == 4){ \
        MMIOBase[(REG_3D_TSWGa+(i)*0x30)/4] = VB->Win.data[v][3] * \
                                     VB->TexCoordPtr[0]->data[v][3]; \
      } \
      else{ \
        MMIOBase[(REG_3D_TSWGa+(i)*0x30)/4] = VB->Win.data[v][3]; \
      } \
    } \
\
  if (SIS_STATES & (SIS_SMOOTH)) \
    { \
      DWORD dcSARGB; \
\
      RGBA8ConvertToBGRA8 (&dcSARGB, VB->ColorPtr->data[v]); \
\
      ((DWORD *) MMIOBase)[(REG_3D_TSARGBa+(i)*0x30)/4] = dcSARGB; \
    } \
  else if(LAST_VERTEX) \
    { \
      DWORD dcSARGB; \
\
      RGBA8ConvertToBGRA8 (&dcSARGB, VB->ColorPtr->data[pv]); \
\
      ((DWORD *) MMIOBase)[(REG_3D_TSARGBa+(i)*0x30)/4] = dcSARGB; \
    } \

static void
SIS_TAG (sis_line) (GLcontext * ctx, GLuint vert0, GLuint vert1, GLuint pv)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  struct vertex_buffer *VB = ctx->VB;
  float *MMIOBase = (float *) GET_IOBase (hwcx);

  STEREO_SAMPLE(vert0);

  mWait3DCmdQueue (10 * 2 + 1);

  if (SIS_STATES & (SIS_SMOOTH))
    {
      hwcx->dwPrimitiveSet &= ~0x07001f07;
      hwcx->dwPrimitiveSet |= 
        (OP_3D_FIRE_TSARGBb | SHADE_GOURAUD | OP_3D_LINE_DRAW);

      ((DWORD *) MMIOBase)[REG_3D_PrimitiveSet / 4] = hwcx->dwPrimitiveSet;
    }
  else
    {
      hwcx->dwPrimitiveSet &= ~0x07001f07;
      hwcx->dwPrimitiveSet |= 
        (OP_3D_FIRE_TSARGBb | SHADE_FLAT_VertexB | OP_3D_LINE_DRAW);

      ((DWORD *) MMIOBase)[REG_3D_PrimitiveSet / 4] = hwcx->dwPrimitiveSet;
    }

#define LAST_VERTEX 0
  SIS_MMIO_WRITE_VERTEX (vert0, 0);
#undef LAST_VERTEX
#define LAST_VERTEX 1
  SIS_MMIO_WRITE_VERTEX (vert1, 1);
#undef LAST_VERTEX

  /* debug
  mEndPrimitive ();
  WaitEngIdle(hwcx);  
  d2f_once (ctx);
  */
}

#define SIS_AGP_WRITE_VERTEX(v) \
do{ \
if(SIS_STEREO && hwcx->stereoEnabled) \
{ \
  AGP_CurrentPtr[0] = VB->Win.data[v][0] - 0.5 + STEREO_OFFSET(v); \
} \
else \
{ \
  AGP_CurrentPtr[0] = VB->Win.data[v][0] - 0.5; \
} \
AGP_CurrentPtr[1] = Y_FLIP (VB->Win.data[v][1]) + 0.5; \
\
if (ctx->TriangleCaps & DD_TRI_OFFSET){ \
  if(SIS_TRI_FUNC){ \
    AGP_CurrentPtr[2] = (VB->Win.data[v][2] + ctx->PolygonZoffset) / SIS_DEPTH_SCALE; \
  } \
  else{ \
    AGP_CurrentPtr[2] = (VB->Win.data[v][2] + ctx->LineZoffset) / SIS_DEPTH_SCALE; \
  } \
} \
else{ \
  AGP_CurrentPtr[2] = VB->Win.data[v][2] / SIS_DEPTH_SCALE; \
}\
AGP_CurrentPtr+=3; \
if (SIS_STATES & (SIS_USE_W)) \
{ \
   if(VB->TexCoordPtr[0]->size == 4){ \
     AGP_CurrentPtr[0] = VB->Win.data[v][3] * \
                         VB->TexCoordPtr[0]->data[v][3]; \
   } \
   else{ \
     AGP_CurrentPtr[0] = VB->Win.data[v][3]; \
   } \
   AGP_CurrentPtr+=1; \
} \
\
if (SIS_STATES & (SIS_SMOOTH)) \
{ \
  RGBA8ConvertToBGRA8 (&dcSARGB, VB->ColorPtr->data[v]); \
  ((DWORD *)AGP_CurrentPtr)[0] = dcSARGB; \
} \
else if(FIRST_VERTEX) \
{ \
  RGBA8ConvertToBGRA8 (&dcSARGB, VB->ColorPtr->data[pv]); \
  ((DWORD *)AGP_CurrentPtr)[0] = dcSARGB; \
} \
else \
{ \
  ((DWORD *)AGP_CurrentPtr)[0] = dcSARGB; \
} \
AGP_CurrentPtr+=1; \
\
if (SIS_STATES & SIS_TEXTURE0) \
{ \
  if(VB->TexCoordPtr[0]->size == 4){ \
    AGP_CurrentPtr[0] = VB->TexCoordPtr[0]->data[v][0] / \
                        VB->TexCoordPtr[0]->data[v][3]; \
    AGP_CurrentPtr[1] = VB->TexCoordPtr[0]->data[v][1] / \
                        VB->TexCoordPtr[0]->data[v][3]; \
  } \
  else{ \
    AGP_CurrentPtr[0] = VB->TexCoordPtr[0]->data[v][0]; \
    AGP_CurrentPtr[1] = VB->TexCoordPtr[0]->data[v][1]; \
  } \
  AGP_CurrentPtr+=2; \
} \
\
if (SIS_STATES & SIS_TEXTURE1) \
{ \
  if(VB->TexCoordPtr[1]->size == 4){ \
    AGP_CurrentPtr[0] = VB->TexCoordPtr[1]->data[v][0] / \
                        VB->TexCoordPtr[1]->data[v][3]; \
    AGP_CurrentPtr[1] = VB->TexCoordPtr[1]->data[v][1] / \
                        VB->TexCoordPtr[1]->data[v][3]; \
  } \
  else{ \
    AGP_CurrentPtr[0] = VB->TexCoordPtr[1]->data[v][0]; \
    AGP_CurrentPtr[1] = VB->TexCoordPtr[1]->data[v][1]; \
  } \
  AGP_CurrentPtr+=2; \
} \
}while(0)

static void
SIS_TAG (sis_agp_line) (GLcontext * ctx, GLuint vert0, GLuint vert1,
			GLuint pv)
{
  XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
  __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;

  struct vertex_buffer *VB = ctx->VB;

  DWORD dcSARGB;

  STEREO_SAMPLE(vert0);

#if 0
  if ((DWORD) AGP_CurrentPtr - (DWORD) AGP_StartPtr >= (AGP_ALLOC_SIZE - 0x10))
    {
      sis_FlushAGP (ctx);
      sis_StartAGP (ctx);
    }
#endif

#define FIRST_VERTEX 1
  SIS_AGP_WRITE_VERTEX (vert0);
#undef FIRST_VERTEX
#define FIRST_VERTEX 0
  SIS_AGP_WRITE_VERTEX (vert1);
#undef FIRST_VERTEX
}

#undef SIS_TRI_FUNC
