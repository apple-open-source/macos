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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_fastpath.c,v 1.3 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

/* 
 *  FOR 
 *  1. triangle/strip with 2-texture with no other capability  
 *     (depth-offset, edge-flag...)
 *  2. smooth shading
 *  3. render to backbuffer
 *  4. use AGP command mode   
 */   

#ifdef PC_HEADER
#include "all.h"
#else
#include "glheader.h"
#include "clip.h"
#include "context.h"
#include "light.h"
#include "lines.h"
#include "macros.h"
#include "matrix.h"
#include "pb.h"
#include "points.h"
#include "pipeline.h"
#include "stages.h"
#include "types.h"
#include "vb.h"
#include "vbcull.h"
#include "vbrender.h"
#include "vbindirect.h"
#include "xform.h"
#endif

#include "sis_ctx.h"
#include "sis_mesa.h"

#define WRITE_SMOOTH_W_T2(v) \
do{ \
  DWORD dcSARGB; \
  \
  AGP_CurrentPtr[0] = VB->Win.data[v][0] - 0.5; \
  AGP_CurrentPtr[1] = Y_FLIP (VB->Win.data[v][1]) + 0.5; \
  AGP_CurrentPtr[2] = VB->Win.data[v][2] / 65535.0; \
  AGP_CurrentPtr[3] = VB->Win.data[v][3]; \
  RGBA8ConvertToBGRA8 (&dcSARGB, VB->ColorPtr->data[v]); \
  ((DWORD *)AGP_CurrentPtr)[4] = dcSARGB; \
  AGP_CurrentPtr[5] = VB->TexCoordPtr[0]->data[v][0]; \
  AGP_CurrentPtr[6] = VB->TexCoordPtr[0]->data[v][1]; \
  AGP_CurrentPtr[7] = VB->TexCoordPtr[1]->data[v][0]; \
  AGP_CurrentPtr[8] = VB->TexCoordPtr[1]->data[v][1]; \
  AGP_CurrentPtr+=9; \
}while(0)

  /* TODO or use for loop and let compiler unroll it */
#define COPY_SMOOTH_W_T2(i) \
do{ \
  AGP_CurrentPtr[0] = (AGP_CurrentPtr+(i)*9)[0]; \
  AGP_CurrentPtr[1] = (AGP_CurrentPtr+(i)*9)[1]; \
  AGP_CurrentPtr[2] = (AGP_CurrentPtr+(i)*9)[2]; \
  AGP_CurrentPtr[3] = (AGP_CurrentPtr+(i)*9)[3]; \
  AGP_CurrentPtr[4] = (AGP_CurrentPtr+(i)*9)[4]; \
  AGP_CurrentPtr[5] = (AGP_CurrentPtr+(i)*9)[5]; \
  AGP_CurrentPtr[6] = (AGP_CurrentPtr+(i)*9)[6]; \
  AGP_CurrentPtr[7] = (AGP_CurrentPtr+(i)*9)[7]; \
  AGP_CurrentPtr[8] = (AGP_CurrentPtr+(i)*9)[8]; \
  AGP_CurrentPtr+=9; \
}while(0)

#if defined(SIS_USE_FASTPATH)
static void sis_render_vb( struct vertex_buffer *VB )
{
   GLcontext *ctx = VB->ctx;
   GLuint i, next, prim;
   GLuint parity = VB->Parity;
   render_func *tab;
   GLuint count = VB->Count;

   XMesaContext xmesa = (XMesaContext) ctx->DriverCtx;
   __GLSiScontext *hwcx = (__GLSiScontext *) xmesa->private;
  
   if(!hwcx->useFastPath){
     gl_render_vb(VB);
     return;
   } 
      
   if (VB->Indirect) { 
      return; 
#ifndef SIS_USE_HW_CULL
   } else if (VB->CullMode & CULL_MASK_ACTIVE) {
      tab = ctx->Driver.RenderVBCulledTab;

      /* TODO: check */
      if (!VB->CullDone)
         gl_fast_copy_vb( VB );
#endif
   } else if (VB->CullMode & CLIP_MASK_ACTIVE) {
      tab = ctx->Driver.RenderVBClippedTab;
   } else {
      tab = ctx->Driver.RenderVBRawTab;
   }

   /* TODO: know what does it do */
   gl_import_client_data( VB, ctx->RenderFlags,
			  (VB->ClipOrMask
			   ? VEC_WRITABLE|VEC_GOOD_STRIDE
			   : VEC_GOOD_STRIDE));

   ctx->Driver.RenderStart( ctx );

   if(tab != ctx->Driver.RenderVBRawTab){
      for ( i= VB->CopyStart ; i < count ; parity = 0, i = next ) 
      {
	 prim = VB->Primitive[i];
	 next = VB->NextPrimitive[i];

	 tab[prim]( VB, i, next, parity );
      }          
   }
   else{
     for ( i= VB->CopyStart ; i < count ; parity = 0, i = next ) 
      {
        prim = VB->Primitive[i];
        next = VB->NextPrimitive[i];

        if(prim == GL_TRIANGLE_STRIP)
          {
            int j;
            
            /* assume size >= 3 */
            /* assert(next-i+1 >= 3); */

            if(i+2 >=next)
              break;

            if(parity){
              WRITE_SMOOTH_W_T2(i+1);
              WRITE_SMOOTH_W_T2(i);
              WRITE_SMOOTH_W_T2(i+2);        
            }
            else{
              WRITE_SMOOTH_W_T2(i);
              WRITE_SMOOTH_W_T2(i+1);
              WRITE_SMOOTH_W_T2(i+2);        
            }

            for(j=i+3; j<next; j++){
              if(parity){
                COPY_SMOOTH_W_T2(-3);
                COPY_SMOOTH_W_T2(-2);
                WRITE_SMOOTH_W_T2(j);
              }
              else{
                COPY_SMOOTH_W_T2(-1);
                COPY_SMOOTH_W_T2(-3);
                WRITE_SMOOTH_W_T2(j);
              }              
              parity ^= 1;
            }	
          }
        else
          {
            tab[prim]( VB, i, next, parity );
          }
      }
   }

   ctx->Driver.RenderFinish( ctx );
}
#endif

GLuint sis_RegisterPipelineStages (struct gl_pipeline_stage *out,
				   const struct gl_pipeline_stage *in,
				   GLuint nr)
{
   GLuint i, o;

   for (i = o = 0 ; i < nr ; i++) {
      switch (in[i].ops) {
      case PIPE_OP_RENDER:
	 out[o] = in[i];
#if defined(SIS_USE_FASTPATH)
	 if (in[i].run == gl_render_vb) {
  	    out[o].run = sis_render_vb; 
	 }
#endif
	 o++;
	 break;
      default:
	 out[o++] = in[i];
	 break;
      }
   }

   return o;
}				   
