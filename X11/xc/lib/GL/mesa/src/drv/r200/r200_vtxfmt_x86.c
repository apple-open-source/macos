/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_vtxfmt_x86.c,v 1.2 2002/12/16 16:18:56 dawes Exp $ */
/*
Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.

The Weather Channel (TM) funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86 license.
This notice must be preserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#include <stdio.h>
#include <assert.h>
#include "mem.h" 
#include "mmath.h" 
#include "simple_list.h" 
#include "r200_vtxfmt.h"

#if defined(USE_X86_ASM)

#define EXTERN( FUNC )		\
extern const char *FUNC;	\
extern const char *FUNC##_end

EXTERN ( _x86_Normal3fv );
EXTERN ( _x86_Normal3f );
EXTERN ( _x86_Vertex3fv_6 );
EXTERN ( _x86_Vertex3fv_8 );
EXTERN ( _x86_Vertex3fv );
EXTERN ( _x86_Vertex3f_4 );
EXTERN ( _x86_Vertex3f_6 );
EXTERN ( _x86_Vertex3f );
EXTERN ( _x86_Color4ubv_ub );
EXTERN ( _x86_Color4ubv_4f );
EXTERN ( _x86_Color4ub_ub );
EXTERN ( _x86_Color3fv_3f );
EXTERN ( _x86_Color3f_3f );
EXTERN ( _x86_TexCoord2fv );
EXTERN ( _x86_TexCoord2f );
EXTERN ( _x86_MultiTexCoord2fvARB );
EXTERN ( _x86_MultiTexCoord2fvARB_2 );
EXTERN ( _x86_MultiTexCoord2fARB );
EXTERN ( _x86_MultiTexCoord2fARB_2 );


/* Build specialized versions of the immediate calls on the fly for
 * the current state.  Generic x86 versions.
 */

struct dynfn *r200_makeX86Vertex3f( GLcontext *ctx, const int *key )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   struct dynfn *dfn = MALLOC_STRUCT( dynfn );

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x 0x%08x %d\n", __FUNCTION__, 
	      key[0], key[1], vb.vertex_size );

   switch (vb.vertex_size) {
   case 4: {

      DFN ( _x86_Vertex3f_4, rmesa->vb.dfn_cache.Vertex3f );
      FIXUP(dfn->code, 2, 0x0, (int)&vb.dmaptr);
      FIXUP(dfn->code, 25, 0x0, (int)&vb.vertex[3]);
      FIXUP(dfn->code, 36, 0x0, (int)&vb.counter);
      FIXUP(dfn->code, 46, 0x0, (int)&vb.dmaptr);
      FIXUP(dfn->code, 51, 0x0, (int)&vb.counter);
      FIXUP(dfn->code, 60, 0x0, (int)&vb.notify);
      break;
   }
   case 6: {

      DFN ( _x86_Vertex3f_6, rmesa->vb.dfn_cache.Vertex3f );
      FIXUP(dfn->code, 3, 0x0, (int)&vb.dmaptr);
      FIXUP(dfn->code, 28, 0x0, (int)&vb.vertex[3]);
      FIXUP(dfn->code, 34, 0x0, (int)&vb.vertex[4]);
      FIXUP(dfn->code, 40, 0x0, (int)&vb.vertex[5]);
      FIXUP(dfn->code, 57, 0x0, (int)&vb.counter);
      FIXUP(dfn->code, 63, 0x0, (int)&vb.dmaptr);
      FIXUP(dfn->code, 70, 0x0, (int)&vb.counter);
      FIXUP(dfn->code, 79, 0x0, (int)&vb.notify);
      break;
   }
   default: {

      DFN ( _x86_Vertex3f, rmesa->vb.dfn_cache.Vertex3f );
      FIXUP(dfn->code, 3, 0x0, (int)&vb.vertex[3]);
      FIXUP(dfn->code, 9, 0x0, (int)&vb.dmaptr);
      FIXUP(dfn->code, 37, 0x0, vb.vertex_size-3);
      FIXUP(dfn->code, 44, 0x0, (int)&vb.counter);
      FIXUP(dfn->code, 50, 0x0, (int)&vb.dmaptr);
      FIXUP(dfn->code, 56, 0x0, (int)&vb.counter);
      FIXUP(dfn->code, 67, 0x0, (int)&vb.notify);
   break;
   }
   }

   return dfn;
}



struct dynfn *r200_makeX86Vertex3fv( GLcontext *ctx, const int *key )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   struct dynfn *dfn = MALLOC_STRUCT( dynfn );

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x 0x%08x %d\n", __FUNCTION__, 
	      key[0], key[1], vb.vertex_size );

   switch (vb.vertex_size) {
   case 6: {

      DFN ( _x86_Vertex3fv_6, rmesa->vb.dfn_cache.Vertex3fv );
      FIXUP(dfn->code, 1, 0x00000000, (int)&vb.dmaptr);
      FIXUP(dfn->code, 27, 0x0000001c, (int)&vb.vertex[3]);
      FIXUP(dfn->code, 33, 0x00000020, (int)&vb.vertex[4]);
      FIXUP(dfn->code, 45, 0x00000024, (int)&vb.vertex[5]);
      FIXUP(dfn->code, 56, 0x00000000, (int)&vb.dmaptr);
      FIXUP(dfn->code, 61, 0x00000004, (int)&vb.counter);
      FIXUP(dfn->code, 67, 0x00000004, (int)&vb.counter);
      FIXUP(dfn->code, 76, 0x00000008, (int)&vb.notify);
      break;
   }
   

   case 8: {

      DFN ( _x86_Vertex3fv_8, rmesa->vb.dfn_cache.Vertex3fv );
      FIXUP(dfn->code, 1, 0x00000000, (int)&vb.dmaptr);
      FIXUP(dfn->code, 27, 0x0000001c, (int)&vb.vertex[3]);
      FIXUP(dfn->code, 33, 0x00000020, (int)&vb.vertex[4]);
      FIXUP(dfn->code, 45, 0x0000001c, (int)&vb.vertex[5]);
      FIXUP(dfn->code, 51, 0x00000020, (int)&vb.vertex[6]);
      FIXUP(dfn->code, 63, 0x00000024, (int)&vb.vertex[7]);
      FIXUP(dfn->code, 74, 0x00000000, (int)&vb.dmaptr);
      FIXUP(dfn->code, 79, 0x00000004, (int)&vb.counter);
      FIXUP(dfn->code, 85, 0x00000004, (int)&vb.counter);
      FIXUP(dfn->code, 94, 0x00000008, (int)&vb.notify);
      break;
   }
   


   default: {

      DFN ( _x86_Vertex3fv, rmesa->vb.dfn_cache.Vertex3fv );
      FIXUP(dfn->code, 8, 0x01010101, (int)&vb.dmaptr);
      FIXUP(dfn->code, 32, 0x00000006, vb.vertex_size-3);
      FIXUP(dfn->code, 37, 0x00000058, (int)&vb.vertex[3]);
      FIXUP(dfn->code, 45, 0x01010101, (int)&vb.dmaptr);
      FIXUP(dfn->code, 50, 0x02020202, (int)&vb.counter);
      FIXUP(dfn->code, 58, 0x02020202, (int)&vb.counter);
      FIXUP(dfn->code, 67, 0x0, (int)&vb.notify);
   break;
   }
   }

   return dfn;
}

struct dynfn *r200_makeX86Normal3fv( GLcontext *ctx, const int *key )
{
   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   int i = 0;

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x\n", __FUNCTION__, key[0] );

   DFN ( _x86_Normal3fv, rmesa->vb.dfn_cache.Normal3fv );

   FIXUP2(dfn->code, i, 0x0, (int)vb.normalptr); 
   FIXUP2(dfn->code, i, 0x4, 4+(int)vb.normalptr); 
   FIXUP2(dfn->code, i, 0x8, 8+(int)vb.normalptr); 
   fprintf(stderr, "%s done\n", __FUNCTION__);
   return dfn;
}

struct dynfn *r200_makeX86Normal3f( GLcontext *ctx, const int *key )
{
   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x\n", __FUNCTION__, key[0] );

   DFN ( _x86_Normal3f, rmesa->vb.dfn_cache.Normal3f );
   FIXUP(dfn->code, 1, 0x12345678, (int)vb.normalptr); 
   return dfn;
}

struct dynfn *r200_makeX86Color4ubv( GLcontext *ctx, const int *key )
{
   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);


   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x\n", __FUNCTION__, key[0] );

   if (VTX_COLOR(key[0],0) == R200_VTX_PK_RGBA) {
      DFN ( _x86_Color4ubv_ub, rmesa->vb.dfn_cache.Color4ubv);
      FIXUP(dfn->code, 5, 0x12345678, (int)vb.colorptr); 
      return dfn;
   } 
   else {

      DFN ( _x86_Color4ubv_4f, rmesa->vb.dfn_cache.Color4ubv);
      FIXUP(dfn->code, 2, 0x00000000, (int)_mesa_ubyte_to_float_color_tab); 
      FIXUP(dfn->code, 27, 0xdeadbeaf, (int)vb.floatcolorptr); 
      FIXUP(dfn->code, 33, 0xdeadbeaf, (int)vb.floatcolorptr+4); 
      FIXUP(dfn->code, 55, 0xdeadbeaf, (int)vb.floatcolorptr+8); 
      FIXUP(dfn->code, 61, 0xdeadbeaf, (int)vb.floatcolorptr+12); 
      return dfn;
   }
}

struct dynfn *r200_makeX86Color4ub( GLcontext *ctx, const int *key )
{
   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x\n", __FUNCTION__, key[0] );

   if (VTX_COLOR(key[0],0) == R200_VTX_PK_RGBA) {
      struct dynfn *dfn = MALLOC_STRUCT( dynfn );
      r200ContextPtr rmesa = R200_CONTEXT(ctx);

      DFN ( _x86_Color4ub_ub, rmesa->vb.dfn_cache.Color4ub );
      FIXUP(dfn->code, 18, 0x0, (int)vb.colorptr); 
      FIXUP(dfn->code, 24, 0x0, (int)vb.colorptr+1); 
      FIXUP(dfn->code, 30, 0x0, (int)vb.colorptr+2); 
      FIXUP(dfn->code, 36, 0x0, (int)vb.colorptr+3); 
      return dfn;
   }
   else
      return 0;
}


struct dynfn *r200_makeX86Color3fv( GLcontext *ctx, const int *key )
{
   if (VTX_COLOR(key[0],0) != R200_VTX_FP_RGB) 
      return 0;
   else
   {
      struct dynfn *dfn = MALLOC_STRUCT( dynfn );
      r200ContextPtr rmesa = R200_CONTEXT(ctx);

      if (R200_DEBUG & DEBUG_CODEGEN)
	 fprintf(stderr, "%s 0x%08x\n", __FUNCTION__, key[0] );

      DFN ( _x86_Color3fv_3f, rmesa->vb.dfn_cache.Color3fv );
      FIXUP(dfn->code, 5, 0x0, (int)vb.floatcolorptr); 
      return dfn;
   }
}

struct dynfn *r200_makeX86Color3f( GLcontext *ctx, const int *key )
{
   if (VTX_COLOR(key[0],0) != R200_VTX_FP_RGB) 
      return 0;
   else
   {
      struct dynfn *dfn = MALLOC_STRUCT( dynfn );
      r200ContextPtr rmesa = R200_CONTEXT(ctx);

      if (R200_DEBUG & DEBUG_CODEGEN)
	 fprintf(stderr, "%s 0x%08x\n", __FUNCTION__, key[0] );

      DFN ( _x86_Color3f_3f, rmesa->vb.dfn_cache.Color3f );
      FIXUP(dfn->code, 1, 0x12345678, (int)vb.floatcolorptr); 
      return dfn;
   }
}



struct dynfn *r200_makeX86TexCoord2fv( GLcontext *ctx, const int *key )
{

   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x 0x%08x\n", __FUNCTION__, key[0], key[1] );

   DFN ( _x86_TexCoord2fv, rmesa->vb.dfn_cache.TexCoord2fv );
   FIXUP(dfn->code, 5, 0x12345678, (int)vb.texcoordptr[0]); 
   return dfn;
}

struct dynfn *r200_makeX86TexCoord2f( GLcontext *ctx, const int *key )
{

   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x 0x%08x\n", __FUNCTION__, key[0], key[1] );

   DFN ( _x86_TexCoord2f, rmesa->vb.dfn_cache.TexCoord2f );
   FIXUP(dfn->code, 1, 0x12345678, (int)vb.texcoordptr[0]); 
   return dfn;
}

struct dynfn *r200_makeX86MultiTexCoord2fvARB( GLcontext *ctx, const int *key )
{
#if 0
   static  char temp[] = {
      0x8b, 0x44, 0x24, 0x04,          	/* mov    0x4(%esp,1),%eax */
      0x8b, 0x4c, 0x24, 0x08,          	/* mov    0x8(%esp,1),%ecx */
      0x2d, 0xc0, 0x84, 0x00, 0x00,    	/* sub    $0x84c0,%eax */
      0x83, 0xe0, 0x01,             	/* and    $0x1,%eax */
      0x8b, 0x11,                	/* mov    (%ecx),%edx */
      0xc1, 0xe0, 0x03,             	/* shl    $0x3,%eax */
      0x8b, 0x49, 0x04,             	/* mov    0x4(%ecx),%ecx */
      0x89, 0x90, 0, 0, 0, 0,/* mov    %edx,DEST(%eax) */
      0x89, 0x88, 0, 0, 0, 0,/* mov    %ecx,DEST+8(%eax) */
      0xc3,                     	/* ret     */
   };
   static char temp2[] = {
      0x8b, 0x44, 0x24, 0x04,          	/* mov    0x4(%esp,1),%eax */
      0x8b, 0x4c, 0x24, 0x08,          	/* mov    0x8(%esp,1),%ecx */
      0x2d, 0xc0, 0x84, 0x00, 0x00,    	/* sub    $0x84c0,%eax */
      0x83, 0xe0, 0x01,             	/* and    $0x1,%eax */
      0x8b, 0x14, 0x85, 0, 0, 0, 0, /* mov    DEST(,%eax,4),%edx */
      0x8b, 0x01,                	/* mov    (%ecx),%eax */
      0x89, 0x02,                	/* mov    %eax,(%edx) */
      0x8b, 0x41, 0x04,             	/* mov    0x4(%ecx),%eax */
      0x89, 0x42, 0x04,             	/* mov    %eax,0x4(%edx) */
      0xc3,                     	/* ret     */
   };
#endif

   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x 0x%08x\n", __FUNCTION__, key[0], key[1] );

   if (vb.texcoordptr[1] == vb.texcoordptr[0]+4) {
      DFN ( _x86_MultiTexCoord2fvARB, rmesa->vb.dfn_cache.MultiTexCoord2fvARB );
      FIXUP(dfn->code, 26, 0xdeadbeef, (int)vb.texcoordptr[0]);	
      FIXUP(dfn->code, 32, 0xdeadbeef, (int)vb.texcoordptr[0]+4);
   } else {
      DFN ( _x86_MultiTexCoord2fvARB_2, rmesa->vb.dfn_cache.MultiTexCoord2fvARB );
      FIXUP(dfn->code, 19, 0x0, (int)vb.texcoordptr);
   }
   return dfn;
}

struct dynfn *r200_makeX86MultiTexCoord2fARB( GLcontext *ctx, 
					      const int *key )
{
#if 0
   static  char temp[] = {
      0x8b, 0x44, 0x24, 0x04,          	/* mov    0x4(%esp,1),%eax */
      0x8b, 0x54, 0x24, 0x08,          	/* mov    0x8(%esp,1),%edx */
      0x2d, 0xc0, 0x84, 0x00, 0x00,    	/* sub    $0x84c0,%eax */
      0x8b, 0x4c, 0x24, 0x0c,          	/* mov    0xc(%esp,1),%ecx */
      0x83, 0xe0, 0x01,             	/* and    $0x1,%eax */
      0xc1, 0xe0, 0x03,             	/* shl    $0x3,%eax */
      0x89, 0x90, 0, 0, 0, 0,	/* mov    %edx,DEST(%eax) */
      0x89, 0x88, 0, 0, 0, 0,	/* mov    %ecx,DEST+8(%eax) */
      0xc3,                     	/* ret     */
   };

   static char temp2[] = {
      0x8b, 0x44, 0x24, 0x04,          	/* mov    0x4(%esp,1),%eax */
      0x8b, 0x54, 0x24, 0x08,          	/* mov    0x8(%esp,1),%edx */
      0x2d, 0xc0, 0x84, 0x00, 0x00,    	/* sub    $0x84c0,%eax */
      0x8b, 0x4c, 0x24, 0x0c,          	/* mov    0xc(%esp,1),%ecx */
      0x83, 0xe0, 0x01,             	/* and    $0x1,%eax */
      0x8b, 0x04, 0x85, 0, 0, 0, 0,     /* mov    DEST(,%eax,4),%eax */
      0x89, 0x10,                	/* mov    %edx,(%eax) */
      0x89, 0x48, 0x04,             	/* mov    %ecx,0x4(%eax) */
      0xc3,                   	        /* ret     */
   };
#endif
   struct dynfn *dfn = MALLOC_STRUCT( dynfn );
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_CODEGEN)
      fprintf(stderr, "%s 0x%08x 0x%08x\n", __FUNCTION__, key[0], key[1] );

   if (vb.texcoordptr[1] == vb.texcoordptr[0]+4) {
      DFN ( _x86_MultiTexCoord2fARB, rmesa->vb.dfn_cache.MultiTexCoord2fARB );
      FIXUP(dfn->code, 25, 0xdeadbeef, (int)vb.texcoordptr[0]); 
      FIXUP(dfn->code, 31, 0xdeadbeef, (int)vb.texcoordptr[0]+4); 
   }
   else {
      /* Note: this might get generated multiple times, even though the
       * actual emitted code is the same.
       */
      DFN ( _x86_MultiTexCoord2fARB_2, rmesa->vb.dfn_cache.MultiTexCoord2fARB );
      FIXUP(dfn->code, 23, 0x0, (int)vb.texcoordptr); 
   }      
   return dfn;
}


void r200InitX86Codegen( struct dfn_generators *gen )
{
   gen->Vertex3f = r200_makeX86Vertex3f;
   gen->Vertex3fv = r200_makeX86Vertex3fv;
   gen->Color4ub = r200_makeX86Color4ub; /* PKCOLOR only */
   gen->Color4ubv = r200_makeX86Color4ubv; /* PKCOLOR only */
   gen->Normal3f = r200_makeX86Normal3f;
   gen->Normal3fv = r200_makeX86Normal3fv;
   gen->TexCoord2f = r200_makeX86TexCoord2f;
   gen->TexCoord2fv = r200_makeX86TexCoord2fv;
   gen->MultiTexCoord2fARB = r200_makeX86MultiTexCoord2fARB;
   gen->MultiTexCoord2fvARB = r200_makeX86MultiTexCoord2fvARB;
   gen->Color3f = r200_makeX86Color3f;
   gen->Color3fv = r200_makeX86Color3fv;

   /* Not done:
    */
/*     gen->Vertex2f = r200_makeX86Vertex2f; */
/*     gen->Vertex2fv = r200_makeX86Vertex2fv; */
/*     gen->Color3ub = r200_makeX86Color3ub; */
/*     gen->Color3ubv = r200_makeX86Color3ubv; */
/*     gen->Color4f = r200_makeX86Color4f; */
/*     gen->Color4fv = r200_makeX86Color4fv; */
/*     gen->TexCoord1f = r200_makeX86TexCoord1f; */
/*     gen->TexCoord1fv = r200_makeX86TexCoord1fv; */
/*     gen->MultiTexCoord1fARB = r200_makeX86MultiTexCoord1fARB; */
/*     gen->MultiTexCoord1fvARB = r200_makeX86MultiTexCoord1fvARB; */
}


#else 

void r200InitX86Codegen( struct dfn_generators *gen )
{
   (void) gen;
}

#endif
