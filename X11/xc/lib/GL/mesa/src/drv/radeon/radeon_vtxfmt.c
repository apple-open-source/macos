/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_vtxfmt.c,v 1.5 2002/12/16 16:18:59 dawes Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     Tungsten Graphics Inc., Cedar Park, Texas.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ATI, TUNGSTEN GRAPHICS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */
#include "glheader.h"
#include "radeon_context.h"
#include "radeon_state.h"
#include "radeon_ioctl.h"
#include "radeon_tex.h"
#include "radeon_tcl.h"
#include "radeon_vtxfmt.h"

#include "api_noop.h"
#include "api_arrayelt.h"
#include "context.h"
#include "mem.h"
#include "mmath.h"
#include "mtypes.h"
#include "enums.h"
#include "glapi.h"
#include "colormac.h"
#include "light.h"
#include "state.h"
#include "vtxfmt.h"

#include "tnl/tnl.h"
#include "tnl/t_context.h"
#include "tnl/t_array_api.h"

struct radeon_vb vb;

static void radeonFlushVertices( GLcontext *, GLuint );

static void count_func( const char *name,  struct dynfn *l )
{
   int i = 0;
   struct dynfn *f;
   foreach (f, l) i++;
   if (i) fprintf(stderr, "%s: %d\n", name, i );
}

static void count_funcs( radeonContextPtr rmesa )
{
   count_func( "Vertex2f", &rmesa->vb.dfn_cache.Vertex2f );
   count_func( "Vertex2fv", &rmesa->vb.dfn_cache.Vertex2fv );
   count_func( "Vertex3f", &rmesa->vb.dfn_cache.Vertex3f );
   count_func( "Vertex3fv", &rmesa->vb.dfn_cache.Vertex3fv );
   count_func( "Color4ub", &rmesa->vb.dfn_cache.Color4ub );
   count_func( "Color4ubv", &rmesa->vb.dfn_cache.Color4ubv );
   count_func( "Color3ub", &rmesa->vb.dfn_cache.Color3ub );
   count_func( "Color3ubv", &rmesa->vb.dfn_cache.Color3ubv );
   count_func( "Color4f", &rmesa->vb.dfn_cache.Color4f );
   count_func( "Color4fv", &rmesa->vb.dfn_cache.Color4fv );
   count_func( "Color3f", &rmesa->vb.dfn_cache.Color3f );
   count_func( "Color3fv", &rmesa->vb.dfn_cache.Color3fv );
   count_func( "SecondaryColor3f", &rmesa->vb.dfn_cache.SecondaryColor3fEXT );
   count_func( "SecondaryColor3fv", &rmesa->vb.dfn_cache.SecondaryColor3fvEXT );
   count_func( "SecondaryColor3ub", &rmesa->vb.dfn_cache.SecondaryColor3ubEXT );
   count_func( "SecondaryColor3ubv", &rmesa->vb.dfn_cache.SecondaryColor3ubvEXT );
   count_func( "Normal3f", &rmesa->vb.dfn_cache.Normal3f );
   count_func( "Normal3fv", &rmesa->vb.dfn_cache.Normal3fv );
   count_func( "TexCoord2f", &rmesa->vb.dfn_cache.TexCoord2f );
   count_func( "TexCoord2fv", &rmesa->vb.dfn_cache.TexCoord2fv );
   count_func( "TexCoord1f", &rmesa->vb.dfn_cache.TexCoord1f );
   count_func( "TexCoord1fv", &rmesa->vb.dfn_cache.TexCoord1fv );
   count_func( "MultiTexCoord2fARB", &rmesa->vb.dfn_cache.MultiTexCoord2fARB );
   count_func( "MultiTexCoord2fvARB", &rmesa->vb.dfn_cache.MultiTexCoord2fvARB );
   count_func( "MultiTexCoord1fARB", &rmesa->vb.dfn_cache.MultiTexCoord1fARB );
   count_func( "MultiTexCoord1fvARB", &rmesa->vb.dfn_cache.MultiTexCoord1fvARB );
}


void radeon_copy_to_current( GLcontext *ctx ) 
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);

   assert(ctx->Driver.NeedFlush & FLUSH_UPDATE_CURRENT);
   assert(vb.context == ctx);

   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_N0) {
      ctx->Current.Normal[0] = vb.normalptr[0];
      ctx->Current.Normal[1] = vb.normalptr[1];
      ctx->Current.Normal[2] = vb.normalptr[2];
   }

   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_PKCOLOR) {
      ctx->Current.Color[0] = UBYTE_TO_FLOAT( vb.colorptr->red );
      ctx->Current.Color[1] = UBYTE_TO_FLOAT( vb.colorptr->green );
      ctx->Current.Color[2] = UBYTE_TO_FLOAT( vb.colorptr->blue );
      ctx->Current.Color[3] = UBYTE_TO_FLOAT( vb.colorptr->alpha );
   } 
   
   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_FPCOLOR) {
      ctx->Current.Color[0] = vb.floatcolorptr[0];
      ctx->Current.Color[1] = vb.floatcolorptr[1];
      ctx->Current.Color[2] = vb.floatcolorptr[2];
   }

   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_FPALPHA)
      ctx->Current.Color[3] = vb.floatcolorptr[3];
      
   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_PKSPEC) {
      ctx->Current.SecondaryColor[0] = UBYTE_TO_FLOAT( vb.specptr->red );
      ctx->Current.SecondaryColor[1] = UBYTE_TO_FLOAT( vb.specptr->green );
      ctx->Current.SecondaryColor[2] = UBYTE_TO_FLOAT( vb.specptr->blue );
   } 

   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_ST0) {
      ctx->Current.Texcoord[0][0] = vb.texcoordptr[0][0];
      ctx->Current.Texcoord[0][1] = vb.texcoordptr[0][1];
      ctx->Current.Texcoord[0][2] = 0.0F;
      ctx->Current.Texcoord[0][3] = 1.0F;
   }

   if (rmesa->vb.vertex_format & RADEON_CP_VC_FRMT_ST1) {
      ctx->Current.Texcoord[1][0] = vb.texcoordptr[1][0];
      ctx->Current.Texcoord[1][1] = vb.texcoordptr[1][1];
      ctx->Current.Texcoord[1][2] = 0.0F;
      ctx->Current.Texcoord[1][3] = 1.0F;
   }

   ctx->Driver.NeedFlush &= ~FLUSH_UPDATE_CURRENT;
}

static GLboolean discreet_gl_prim[GL_POLYGON+1] = {
   1,				/* 0 points */
   1,				/* 1 lines */
   0,				/* 2 line_strip */
   0,				/* 3 line_loop */
   1,				/* 4 tris */
   0,				/* 5 tri_fan */
   0,				/* 6 tri_strip */
   1,				/* 7 quads */
   0,				/* 8 quadstrip */
   0,				/* 9 poly */
};

static void flush_prims( radeonContextPtr rmesa )
{
   int i,j;
   struct radeon_dma_region tmp = rmesa->dma.current;
   
   tmp.buf->refcount++;
   tmp.aos_size = vb.vertex_size;
   tmp.aos_stride = vb.vertex_size;
   tmp.aos_start = GET_START(&tmp);

   rmesa->dma.current.ptr = rmesa->dma.current.start += 
      (vb.initial_counter - vb.counter) * vb.vertex_size * 4; 

   rmesa->tcl.vertex_format = rmesa->vb.vertex_format;
   rmesa->tcl.aos_components[0] = &tmp;
   rmesa->tcl.nr_aos_components = 1;
   rmesa->dma.flush = 0;

   /* Optimize the primitive list:
    */
   if (rmesa->vb.nrprims > 1) {
      for (j = 0, i = 1 ; i < rmesa->vb.nrprims; i++) {
	 int pj = rmesa->vb.primlist[j].prim & 0xf;
	 int pi = rmesa->vb.primlist[i].prim & 0xf;
      
	 if (pj == pi && discreet_gl_prim[pj] &&
	     rmesa->vb.primlist[i].start == rmesa->vb.primlist[j].end) {
	    rmesa->vb.primlist[j].end = rmesa->vb.primlist[i].end;
	 }
	 else {
	    j++;
	    if (j != i) rmesa->vb.primlist[j] = rmesa->vb.primlist[i];
	 }
      }
      rmesa->vb.nrprims = j+1;
   }

   for (i = 0 ; i < rmesa->vb.nrprims; i++) {
      if (RADEON_DEBUG & DEBUG_PRIMS)
	 fprintf(stderr, "vtxfmt prim %d: %s %d..%d\n", i,
		 _mesa_lookup_enum_by_nr( rmesa->vb.primlist[i].prim & 
					  PRIM_MODE_MASK ),
		 rmesa->vb.primlist[i].start,
		 rmesa->vb.primlist[i].end);

      radeonEmitPrimitive( vb.context,
			   rmesa->vb.primlist[i].start,
			   rmesa->vb.primlist[i].end,
			   rmesa->vb.primlist[i].prim );
   }

   rmesa->vb.nrprims = 0;
   radeonReleaseDmaRegion( rmesa, &tmp, __FUNCTION__ );
}


static void start_prim( radeonContextPtr rmesa, GLuint mode )
{
   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s %d\n", __FUNCTION__, vb.initial_counter - vb.counter);

   rmesa->vb.primlist[rmesa->vb.nrprims].start = vb.initial_counter - vb.counter;
   rmesa->vb.primlist[rmesa->vb.nrprims].prim = mode;
}

static void note_last_prim( radeonContextPtr rmesa, GLuint flags )
{
   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s %d\n", __FUNCTION__, vb.initial_counter - vb.counter);

   if (rmesa->vb.prim[0] != GL_POLYGON+1) {
      rmesa->vb.primlist[rmesa->vb.nrprims].prim |= flags;
      rmesa->vb.primlist[rmesa->vb.nrprims].end = vb.initial_counter - vb.counter;

      if (++(rmesa->vb.nrprims) == RADEON_MAX_PRIMS)
	 flush_prims( rmesa );
   }
}


static void copy_vertex( radeonContextPtr rmesa, GLuint n, GLfloat *dst )
{
   GLuint i;
   GLfloat *src = (GLfloat *)(rmesa->dma.current.address + 
			      rmesa->dma.current.ptr + 
			      (rmesa->vb.primlist[rmesa->vb.nrprims].start + n) * 
			      vb.vertex_size * 4);

   if (RADEON_DEBUG & DEBUG_VFMT) 
      fprintf(stderr, "copy_vertex %d\n", rmesa->vb.primlist[rmesa->vb.nrprims].start + n);

   for (i = 0 ; i < vb.vertex_size; i++) {
      dst[i] = src[i];
   }
}

/* NOTE: This actually reads the copied vertices back from uncached
 * memory.  Could also use the counter/notify mechanism to populate
 * tmp on the fly as vertices are generated.  
 */
static GLuint copy_dma_verts( radeonContextPtr rmesa, GLfloat (*tmp)[15] )
{
   GLuint ovf, i;
   GLuint nr = (vb.initial_counter - vb.counter) - rmesa->vb.primlist[rmesa->vb.nrprims].start;

   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s %d verts\n", __FUNCTION__, nr);

   switch( rmesa->vb.prim[0] )
   {
   case GL_POINTS:
      return 0;
   case GL_LINES:
      ovf = nr&1;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( rmesa, nr-ovf+i, tmp[i] );
      return i;
   case GL_TRIANGLES:
      ovf = nr%3;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( rmesa, nr-ovf+i, tmp[i] );
      return i;
   case GL_QUADS:
      ovf = nr&3;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( rmesa, nr-ovf+i, tmp[i] );
      return i;
   case GL_LINE_STRIP:
      if (nr == 0) 
	 return 0;
      copy_vertex( rmesa, nr-1, tmp[0] );
      return 1;
   case GL_LINE_LOOP:
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      if (nr == 0) 
	 return 0;
      else if (nr == 1) {
	 copy_vertex( rmesa, 0, tmp[0] );
	 return 1;
      } else {
	 copy_vertex( rmesa, 0, tmp[0] );
	 copy_vertex( rmesa, nr-1, tmp[1] );
	 return 2;
      }
   case GL_TRIANGLE_STRIP:
      ovf = MIN2( nr-1, 2 );
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( rmesa, nr-ovf+i, tmp[i] );
      return i;
   case GL_QUAD_STRIP:
      ovf = MIN2( nr-1, 2 );
      if (nr > 2) ovf += nr&1;
      for (i = 0 ; i < ovf ; i++)
	 copy_vertex( rmesa, nr-ovf+i, tmp[i] );
      return i;
   default:
      assert(0);
      return 0;
   }
}

static void VFMT_FALLBACK_OUTSIDE_BEGIN_END( const char *caller )
{
   GLcontext *ctx = vb.context;
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);

   if (RADEON_DEBUG & (DEBUG_VFMT|DEBUG_FALLBACKS))
      fprintf(stderr, "%s from %s\n", __FUNCTION__, caller);

   if (ctx->Driver.NeedFlush) 
      radeonFlushVertices( ctx, ctx->Driver.NeedFlush );

   if (ctx->NewState)
      _mesa_update_state( ctx ); /* clear state so fell_back sticks */

   _tnl_wakeup_exec( ctx );

   assert( rmesa->dma.flush == 0 );
   rmesa->vb.fell_back = GL_TRUE;
   rmesa->vb.installed = GL_FALSE;
   vb.context = 0;
}


static void VFMT_FALLBACK( const char *caller )
{
   GLcontext *ctx = vb.context;
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLfloat tmp[3][15];
   GLuint i, prim;
   GLuint ind = rmesa->vb.vertex_format;
   GLuint nrverts;
   GLfloat alpha = 1.0;

   if (RADEON_DEBUG & (DEBUG_FALLBACKS|DEBUG_VFMT))
      fprintf(stderr, "%s from %s\n", __FUNCTION__, caller);

   if (rmesa->vb.prim[0] == GL_POLYGON+1) {
      VFMT_FALLBACK_OUTSIDE_BEGIN_END( __FUNCTION__ );
      return;
   }

   /* Copy vertices out of dma:
    */
   nrverts = copy_dma_verts( rmesa, tmp );

   /* Finish the prim at this point:
    */
   note_last_prim( rmesa, 0 );
   flush_prims( rmesa );

   /* Update ctx->Driver.CurrentExecPrimitive and swap in swtnl. 
    */
   prim = rmesa->vb.prim[0];
   ctx->Driver.CurrentExecPrimitive = GL_POLYGON+1;
   _tnl_wakeup_exec( ctx );

   assert(rmesa->dma.flush == 0);
   rmesa->vb.fell_back = GL_TRUE;
   rmesa->vb.installed = GL_FALSE;
   vb.context = 0;
   glBegin( prim );
   
   if (rmesa->vb.installed_color_3f_sz == 4)
      alpha = ctx->Current.Color[3];

   /* Replay saved vertices
    */
   for (i = 0 ; i < nrverts; i++) {
      GLuint offset = 3;
      if (ind & RADEON_CP_VC_FRMT_N0) {
	 glNormal3fv( &tmp[i][offset] ); 
	 offset += 3;
      }

      if (ind & RADEON_CP_VC_FRMT_PKCOLOR) {
	 radeon_color_t *col = (radeon_color_t *)&tmp[i][offset];
	 glColor4ub( col->red, col->green, col->blue, col->alpha );
	 offset++;
      }
      else if (ind & RADEON_CP_VC_FRMT_FPALPHA) {
	 glColor4fv( &tmp[i][offset] ); 
	 offset+=4;
      } 
      else if (ind & RADEON_CP_VC_FRMT_FPCOLOR) {
	 glColor3fv( &tmp[i][offset] ); 
	 offset+=3;
      }

      if (ind & RADEON_CP_VC_FRMT_PKSPEC) {
	 radeon_color_t *spec = (radeon_color_t *)&tmp[i][offset];
	 _glapi_Dispatch->SecondaryColor3ubEXT( spec->red, spec->green, spec->blue );
	 offset++;
      }

      if (ind & RADEON_CP_VC_FRMT_ST0) {
	 glTexCoord2fv( &tmp[i][offset] ); 
	 offset += 2;
      }

      if (ind & RADEON_CP_VC_FRMT_ST1) {
	 glMultiTexCoord2fvARB( GL_TEXTURE1_ARB, &tmp[i][offset] );
	 offset += 2;
      }
      glVertex3fv( &tmp[i][0] );
   }

   /* Replay current vertex
    */
   if (ind & RADEON_CP_VC_FRMT_N0) 
      glNormal3fv( vb.normalptr );

   if (ind & RADEON_CP_VC_FRMT_PKCOLOR)
      glColor4ub( vb.colorptr->red, vb.colorptr->green, vb.colorptr->blue, vb.colorptr->alpha );
   else if (ind & RADEON_CP_VC_FRMT_FPALPHA)
      glColor4fv( vb.floatcolorptr );
   else if (ind & RADEON_CP_VC_FRMT_FPCOLOR) {
      if (rmesa->vb.installed_color_3f_sz == 4 && alpha != 1.0)
	 glColor4f( vb.floatcolorptr[0],
		    vb.floatcolorptr[1],
		    vb.floatcolorptr[2],
		    alpha );
      else
	 glColor3fv( vb.floatcolorptr );
   }

   if (ind & RADEON_CP_VC_FRMT_PKSPEC) 
      _glapi_Dispatch->SecondaryColor3ubEXT( vb.specptr->red, vb.specptr->green, vb.specptr->blue ); 

   if (ind & RADEON_CP_VC_FRMT_ST0) 
      glTexCoord2fv( vb.texcoordptr[0] );

   if (ind & RADEON_CP_VC_FRMT_ST1) 
      glMultiTexCoord2fvARB( GL_TEXTURE1_ARB, vb.texcoordptr[1] );
}



static void wrap_buffer( void )
{
   GLcontext *ctx = vb.context;
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLfloat tmp[3][15];
   GLuint i, nrverts;

   if (RADEON_DEBUG & (DEBUG_VFMT|DEBUG_PRIMS))
      fprintf(stderr, "%s %d\n", __FUNCTION__, vb.initial_counter - vb.counter);

   /* Don't deal with parity.
    */
   if ((((vb.initial_counter - vb.counter) -  
	 rmesa->vb.primlist[rmesa->vb.nrprims].start) & 1)) {
      vb.counter++;
      vb.initial_counter++;
      return;
   }

   /* Copy vertices out of dma:
    */
   if (rmesa->vb.prim[0] == GL_POLYGON+1) 
      nrverts = 0;
   else {
      nrverts = copy_dma_verts( rmesa, tmp );

      if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "%d vertices to copy\n", nrverts);
   
      /* Finish the prim at this point:
       */
      note_last_prim( rmesa, 0 );
   }

   /* Fire any buffered primitives
    */
   flush_prims( rmesa );

   /* Get new buffer
    */
   radeonRefillCurrentDmaRegion( rmesa );

   /* Reset counter, dmaptr
    */
   vb.dmaptr = (int *)(rmesa->dma.current.ptr + rmesa->dma.current.address);
   vb.counter = (rmesa->dma.current.end - rmesa->dma.current.ptr) / 
      (vb.vertex_size * 4);
   vb.counter--;
   vb.initial_counter = vb.counter;
   vb.notify = wrap_buffer;

   rmesa->dma.flush = flush_prims;

   /* Restart wrapped primitive:
    */
   if (rmesa->vb.prim[0] != GL_POLYGON+1)
      start_prim( rmesa, rmesa->vb.prim[0] );

   /* Reemit saved vertices
    */
   for (i = 0 ; i < nrverts; i++) {
      if (RADEON_DEBUG & DEBUG_VERTS) {
	 int j;
	 fprintf(stderr, "re-emit vertex %d to %p\n", i, vb.dmaptr);
	 if (RADEON_DEBUG & DEBUG_VERBOSE)
	    for (j = 0 ; j < vb.vertex_size; j++) 
	       fprintf(stderr, "\t%08x/%f\n", *(int*)&tmp[i][j], tmp[i][j]);
      }

      memcpy( vb.dmaptr, tmp[i], vb.vertex_size * 4 );
      vb.dmaptr += vb.vertex_size;
      vb.counter--;
   }
}



static GLboolean check_vtx_fmt( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLuint ind = RADEON_CP_VC_FRMT_Z;

   if (rmesa->TclFallback || rmesa->vb.fell_back || ctx->CompileFlag)
      return GL_FALSE;

   if (ctx->Driver.NeedFlush & FLUSH_UPDATE_CURRENT) 
      ctx->Driver.FlushVertices( ctx, FLUSH_UPDATE_CURRENT );
   
   /* Make all this event-driven:
    */
   if (ctx->Light.Enabled) {
      ind |= RADEON_CP_VC_FRMT_N0;

      /* TODO: make this data driven: If we receive only ubytes, send
       * color as ubytes.  Also check if converting (with free
       * checking for overflow) is cheaper than sending floats
       * directly.
       */
      if (ctx->Light.ColorMaterialEnabled) {
	 ind |= RADEON_CP_VC_FRMT_FPCOLOR;
         if (ctx->Color.AlphaEnabled) {
	    ind |= RADEON_CP_VC_FRMT_FPALPHA;
         }
      }
   }
   else {
      /* TODO: make this data driven?
       */
      ind |= RADEON_CP_VC_FRMT_PKCOLOR;
	 
      if (ctx->_TriangleCaps & DD_SEPARATE_SPECULAR) {
	 ind |= RADEON_CP_VC_FRMT_PKSPEC;
      }
   }

   if (ctx->Texture.Unit[0]._ReallyEnabled) {
      if (ctx->Texture.Unit[0].TexGenEnabled) {
	 if (rmesa->TexGenNeedNormals[0]) {
	    ind |= RADEON_CP_VC_FRMT_N0;
	 }
      } else {
	 if (ctx->Current.Texcoord[0][2] != 0.0F ||
	     ctx->Current.Texcoord[0][3] != 1.0) {
	    if (RADEON_DEBUG & (DEBUG_VFMT|DEBUG_FALLBACKS))
	       fprintf(stderr, "%s: rq0\n", __FUNCTION__);
	    return GL_FALSE;
	 }
	 ind |= RADEON_CP_VC_FRMT_ST0;
      }
   }

   if (ctx->Texture.Unit[1]._ReallyEnabled) {
      if (ctx->Texture.Unit[1].TexGenEnabled) {
	 if (rmesa->TexGenNeedNormals[1]) {
	    ind |= RADEON_CP_VC_FRMT_N0;
	 }
      } else {
	 if (ctx->Current.Texcoord[1][2] != 0.0F ||
	     ctx->Current.Texcoord[1][3] != 1.0) {
	    if (RADEON_DEBUG & (DEBUG_VFMT|DEBUG_FALLBACKS))
	       fprintf(stderr, "%s: rq1\n", __FUNCTION__);
	    return GL_FALSE;
	 }
	 ind |= RADEON_CP_VC_FRMT_ST1;
      }
   }

   if (RADEON_DEBUG & (DEBUG_VFMT|DEBUG_STATE))
      fprintf(stderr, "%s: format: 0x%x\n", __FUNCTION__, ind );

   RADEON_NEWPRIM(rmesa);
   rmesa->vb.vertex_format = ind;
   vb.vertex_size = 3;
   rmesa->vb.prim = &ctx->Driver.CurrentExecPrimitive;

   vb.normalptr = ctx->Current.Normal;
   vb.colorptr = NULL;
   vb.floatcolorptr = ctx->Current.Color;
   vb.specptr = NULL;
   vb.floatspecptr = ctx->Current.SecondaryColor;
   vb.texcoordptr[0] = ctx->Current.Texcoord[0];
   vb.texcoordptr[1] = ctx->Current.Texcoord[1];

   /* Run through and initialize the vertex components in the order
    * the hardware understands:
    */
   if (ind & RADEON_CP_VC_FRMT_N0) {
      vb.normalptr = &vb.vertex[vb.vertex_size].f;
      vb.vertex_size += 3;
      vb.normalptr[0] = ctx->Current.Normal[0];
      vb.normalptr[1] = ctx->Current.Normal[1];
      vb.normalptr[2] = ctx->Current.Normal[2];
   }

   if (ind & RADEON_CP_VC_FRMT_PKCOLOR) {
      vb.colorptr = &vb.vertex[vb.vertex_size].color;
      vb.vertex_size += 1;
      UNCLAMPED_FLOAT_TO_CHAN( vb.colorptr->red,   ctx->Current.Color[0] );
      UNCLAMPED_FLOAT_TO_CHAN( vb.colorptr->green, ctx->Current.Color[1] );
      UNCLAMPED_FLOAT_TO_CHAN( vb.colorptr->blue,  ctx->Current.Color[2] );
      UNCLAMPED_FLOAT_TO_CHAN( vb.colorptr->alpha, ctx->Current.Color[3] );
   }

   if (ind & RADEON_CP_VC_FRMT_FPCOLOR) {
      assert(!(ind & RADEON_CP_VC_FRMT_PKCOLOR));
      vb.floatcolorptr = &vb.vertex[vb.vertex_size].f;
      vb.vertex_size += 3;
      vb.floatcolorptr[0] = ctx->Current.Color[0];
      vb.floatcolorptr[1] = ctx->Current.Color[1];
      vb.floatcolorptr[2] = ctx->Current.Color[2];

      if (ind & RADEON_CP_VC_FRMT_FPALPHA) {
	 vb.vertex_size += 1;
	 vb.floatcolorptr[3] = ctx->Current.Color[3];
      }
   }
   
   if (ind & RADEON_CP_VC_FRMT_PKSPEC) {
      vb.specptr = &vb.vertex[vb.vertex_size].color;
      vb.vertex_size += 1;
      UNCLAMPED_FLOAT_TO_CHAN( vb.specptr->red,   ctx->Current.SecondaryColor[0] );
      UNCLAMPED_FLOAT_TO_CHAN( vb.specptr->green, ctx->Current.SecondaryColor[1] );
      UNCLAMPED_FLOAT_TO_CHAN( vb.specptr->blue,  ctx->Current.SecondaryColor[2] );
   }


   if (ind & RADEON_CP_VC_FRMT_ST0) {
      vb.texcoordptr[0] = &vb.vertex[vb.vertex_size].f;
      vb.vertex_size += 2;
      vb.texcoordptr[0][0] = ctx->Current.Texcoord[0][0];
      vb.texcoordptr[0][1] = ctx->Current.Texcoord[0][1];   
   } 

   if (ind & RADEON_CP_VC_FRMT_ST1) {
      vb.texcoordptr[1] = &vb.vertex[vb.vertex_size].f;
      vb.vertex_size += 2;
      vb.texcoordptr[1][0] = ctx->Current.Texcoord[1][0];
      vb.texcoordptr[1][1] = ctx->Current.Texcoord[1][1];
   } 

   if (rmesa->vb.installed_vertex_format != rmesa->vb.vertex_format) {
      if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "reinstall on vertex_format change\n");
      _mesa_install_exec_vtxfmt( ctx, &rmesa->vb.vtxfmt );
      rmesa->vb.installed_vertex_format = rmesa->vb.vertex_format;
   }

   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s -- success\n", __FUNCTION__);
   
   return GL_TRUE;
}


void radeonVtxfmtInvalidate( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

   rmesa->vb.recheck = GL_TRUE;
   rmesa->vb.fell_back = GL_FALSE;
}


static void radeonNewList( GLcontext *ctx, GLuint list, GLenum mode )
{
   VFMT_FALLBACK_OUTSIDE_BEGIN_END( __FUNCTION__ );
}


static void radeonVtxfmtValidate( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s\n", __FUNCTION__);

   if (ctx->Driver.NeedFlush)
      ctx->Driver.FlushVertices( ctx, ctx->Driver.NeedFlush );

   rmesa->vb.recheck = GL_FALSE;

   if (check_vtx_fmt( ctx )) {
      if (!rmesa->vb.installed) {
	 if (RADEON_DEBUG & DEBUG_VFMT)
	    fprintf(stderr, "reinstall (new install)\n");

	 _mesa_install_exec_vtxfmt( ctx, &rmesa->vb.vtxfmt );
	 ctx->Driver.FlushVertices = radeonFlushVertices;
	 ctx->Driver.NewList = radeonNewList;
	 rmesa->vb.installed = GL_TRUE;
	 vb.context = ctx;
      }
      else if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "%s: already installed", __FUNCTION__);
   } 
   else {
      if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "%s: failed\n", __FUNCTION__);

      if (rmesa->vb.installed) {
	 if (rmesa->dma.flush)
	    rmesa->dma.flush( rmesa );
	 _tnl_wakeup_exec( ctx );
	 rmesa->vb.installed = GL_FALSE;
	 vb.context = 0;
      }
   }      
}



/* Materials:
 */
static void radeon_Materialfv( GLenum face, GLenum pname, 
			       const GLfloat *params )
{
   GLcontext *ctx = vb.context;
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s\n", __FUNCTION__);

   if (rmesa->vb.prim[0] != GL_POLYGON+1) {
      VFMT_FALLBACK( __FUNCTION__ );
      glMaterialfv( face, pname, params );
      return;
   }
   _mesa_noop_Materialfv( face, pname, params );
   radeonUpdateMaterial( vb.context );
}


/* Begin/End
 */
static void radeon_Begin( GLenum mode )
{
   GLcontext *ctx = vb.context;
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   
   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s\n", __FUNCTION__);

   if (mode > GL_POLYGON) {
      _mesa_error( ctx, GL_INVALID_ENUM, "glBegin" );
      return;
   }

   if (rmesa->vb.prim[0] != GL_POLYGON+1) {
      _mesa_error( ctx, GL_INVALID_OPERATION, "glBegin" );
      return;
   }
   
   if (ctx->NewState) 
      _mesa_update_state( ctx );

   if (rmesa->NewGLState)
      radeonValidateState( ctx );

   if (rmesa->vb.recheck) 
      radeonVtxfmtValidate( ctx );

   if (!rmesa->vb.installed) {
      glBegin( mode );
      return;
   }


   if (rmesa->dma.flush && vb.counter < 12) {
      if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "%s: flush almost-empty buffers\n", __FUNCTION__);
      flush_prims( rmesa );
   }

   /* Need to arrange to save vertices here?  Or always copy from dma (yuk)?
    */
   if (!rmesa->dma.flush) {
      if (rmesa->dma.current.ptr + 12*vb.vertex_size*4 > 
	  rmesa->dma.current.end) {
	 RADEON_NEWPRIM( rmesa );
	 radeonRefillCurrentDmaRegion( rmesa );
      }

      vb.dmaptr = (int *)(rmesa->dma.current.address + rmesa->dma.current.ptr);
      vb.counter = (rmesa->dma.current.end - rmesa->dma.current.ptr) / 
	 (vb.vertex_size * 4);
      vb.counter--;
      vb.initial_counter = vb.counter;
      vb.notify = wrap_buffer;
      rmesa->dma.flush = flush_prims;
      vb.context->Driver.NeedFlush |= FLUSH_STORED_VERTICES;
   }
   
   
   rmesa->vb.prim[0] = mode;
   start_prim( rmesa, mode | PRIM_BEGIN );
}



static void radeon_End( void )
{
   GLcontext *ctx = vb.context;
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);

   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s\n", __FUNCTION__);

   if (rmesa->vb.prim[0] == GL_POLYGON+1) {
      _mesa_error( ctx, GL_INVALID_OPERATION, "glEnd" );
      return;
   }
	  
   note_last_prim( rmesa, PRIM_END );
   rmesa->vb.prim[0] = GL_POLYGON+1;
}


/* Fallback on difficult entrypoints:
 */
#define PRE_LOOPBACK( FUNC )			\
do {						\
   if (RADEON_DEBUG & DEBUG_VFMT) 		\
      fprintf(stderr, "%s\n", __FUNCTION__);	\
   VFMT_FALLBACK( __FUNCTION__ );		\
} while (0)
#define TAG(x) radeon_fallback_##x
#include "vtxfmt_tmp.h"



static GLboolean radeonNotifyBegin( GLcontext *ctx, GLenum p )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );
   
   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s\n", __FUNCTION__);

   assert(!rmesa->vb.installed);

   if (ctx->NewState) 
      _mesa_update_state( ctx );

   if (rmesa->NewGLState)
      radeonValidateState( ctx );

   if (ctx->Driver.NeedFlush)
      ctx->Driver.FlushVertices( ctx, ctx->Driver.NeedFlush );

   if (rmesa->vb.recheck) 
      radeonVtxfmtValidate( ctx );

   if (!rmesa->vb.installed) {
      if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "%s -- failed\n", __FUNCTION__);
      return GL_FALSE;
   }

   radeon_Begin( p );
   return GL_TRUE;
}

static void radeonFlushVertices( GLcontext *ctx, GLuint flags )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

   if (RADEON_DEBUG & DEBUG_VFMT)
      fprintf(stderr, "%s\n", __FUNCTION__);

   assert(rmesa->vb.installed);
   assert(vb.context == ctx);

   if (flags & FLUSH_UPDATE_CURRENT) {
      radeon_copy_to_current( ctx );
      if (RADEON_DEBUG & DEBUG_VFMT)
	 fprintf(stderr, "reinstall on update_current\n");
      _mesa_install_exec_vtxfmt( ctx, &rmesa->vb.vtxfmt );
      ctx->Driver.NeedFlush &= ~FLUSH_UPDATE_CURRENT;
   }

   if (flags & FLUSH_STORED_VERTICES) {
      radeonContextPtr rmesa = RADEON_CONTEXT( ctx );
      assert (rmesa->dma.flush == 0 ||
	      rmesa->dma.flush == flush_prims);
      if (rmesa->dma.flush == flush_prims)
	 flush_prims( RADEON_CONTEXT( ctx ) );
      ctx->Driver.NeedFlush &= ~FLUSH_STORED_VERTICES;
   }
}



/* At this point, don't expect very many versions of each function to
 * be generated, so not concerned about freeing them?
 */


void radeonVtxfmtInit( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );
   GLvertexformat *vfmt = &(rmesa->vb.vtxfmt);

   MEMSET( vfmt, 0, sizeof(GLvertexformat) );

   /* Hook in chooser functions for codegen, etc:
    */
   radeonVtxfmtInitChoosers( vfmt );

   /* Handled fully in supported states, but no codegen:
    */
   vfmt->Materialfv = radeon_Materialfv;
   vfmt->ArrayElement = _ae_loopback_array_elt;	        /* generic helper */
   vfmt->Rectf = _mesa_noop_Rectf;			/* generic helper */
   vfmt->Begin = radeon_Begin;
   vfmt->End = radeon_End;

   /* Fallback for performance reasons:  (Fix with cva/elt path here and
    * dmatmp2.h style primitive-merging)
    *
    * These should call NotifyBegin(), as should _tnl_EvalMesh, to allow
    * a driver-hook.
    */
   vfmt->DrawArrays = radeon_fallback_DrawArrays;
   vfmt->DrawElements = radeon_fallback_DrawElements;
   vfmt->DrawRangeElements = radeon_fallback_DrawRangeElements; 


   /* Not active in supported states; just keep ctx->Current uptodate:
    */
   vfmt->FogCoordfvEXT = _mesa_noop_FogCoordfvEXT;
   vfmt->FogCoordfEXT = _mesa_noop_FogCoordfEXT;
   vfmt->EdgeFlag = _mesa_noop_EdgeFlag;
   vfmt->EdgeFlagv = _mesa_noop_EdgeFlagv;
   vfmt->Indexi = _mesa_noop_Indexi;
   vfmt->Indexiv = _mesa_noop_Indexiv;


   /* Active but unsupported -- fallback if we receive these:
    */
   vfmt->CallList = radeon_fallback_CallList;
   vfmt->EvalCoord1f = radeon_fallback_EvalCoord1f;
   vfmt->EvalCoord1fv = radeon_fallback_EvalCoord1fv;
   vfmt->EvalCoord2f = radeon_fallback_EvalCoord2f;
   vfmt->EvalCoord2fv = radeon_fallback_EvalCoord2fv;
   vfmt->EvalMesh1 = radeon_fallback_EvalMesh1;
   vfmt->EvalMesh2 = radeon_fallback_EvalMesh2;
   vfmt->EvalPoint1 = radeon_fallback_EvalPoint1;
   vfmt->EvalPoint2 = radeon_fallback_EvalPoint2;
   vfmt->TexCoord3f = radeon_fallback_TexCoord3f;
   vfmt->TexCoord3fv = radeon_fallback_TexCoord3fv;
   vfmt->TexCoord4f = radeon_fallback_TexCoord4f;
   vfmt->TexCoord4fv = radeon_fallback_TexCoord4fv;
   vfmt->MultiTexCoord3fARB = radeon_fallback_MultiTexCoord3fARB;
   vfmt->MultiTexCoord3fvARB = radeon_fallback_MultiTexCoord3fvARB;
   vfmt->MultiTexCoord4fARB = radeon_fallback_MultiTexCoord4fARB;
   vfmt->MultiTexCoord4fvARB = radeon_fallback_MultiTexCoord4fvARB;
   vfmt->Vertex4f = radeon_fallback_Vertex4f;
   vfmt->Vertex4fv = radeon_fallback_Vertex4fv;

   (void)radeon_fallback_vtxfmt;

   TNL_CONTEXT(ctx)->Driver.NotifyBegin = radeonNotifyBegin;

   vb.context = ctx;
   rmesa->vb.enabled = 1;
   rmesa->vb.prim = &ctx->Driver.CurrentExecPrimitive;
   rmesa->vb.primflags = 0;

   make_empty_list( &rmesa->vb.dfn_cache.Vertex2f );
   make_empty_list( &rmesa->vb.dfn_cache.Vertex2fv );
   make_empty_list( &rmesa->vb.dfn_cache.Vertex3f );
   make_empty_list( &rmesa->vb.dfn_cache.Vertex3fv );
   make_empty_list( &rmesa->vb.dfn_cache.Color4ub );
   make_empty_list( &rmesa->vb.dfn_cache.Color4ubv );
   make_empty_list( &rmesa->vb.dfn_cache.Color3ub );
   make_empty_list( &rmesa->vb.dfn_cache.Color3ubv );
   make_empty_list( &rmesa->vb.dfn_cache.Color4f );
   make_empty_list( &rmesa->vb.dfn_cache.Color4fv );
   make_empty_list( &rmesa->vb.dfn_cache.Color3f );
   make_empty_list( &rmesa->vb.dfn_cache.Color3fv );
   make_empty_list( &rmesa->vb.dfn_cache.SecondaryColor3fEXT );
   make_empty_list( &rmesa->vb.dfn_cache.SecondaryColor3fvEXT );
   make_empty_list( &rmesa->vb.dfn_cache.SecondaryColor3ubEXT );
   make_empty_list( &rmesa->vb.dfn_cache.SecondaryColor3ubvEXT );
   make_empty_list( &rmesa->vb.dfn_cache.Normal3f );
   make_empty_list( &rmesa->vb.dfn_cache.Normal3fv );
   make_empty_list( &rmesa->vb.dfn_cache.TexCoord2f );
   make_empty_list( &rmesa->vb.dfn_cache.TexCoord2fv );
   make_empty_list( &rmesa->vb.dfn_cache.TexCoord1f );
   make_empty_list( &rmesa->vb.dfn_cache.TexCoord1fv );
   make_empty_list( &rmesa->vb.dfn_cache.MultiTexCoord2fARB );
   make_empty_list( &rmesa->vb.dfn_cache.MultiTexCoord2fvARB );
   make_empty_list( &rmesa->vb.dfn_cache.MultiTexCoord1fARB );
   make_empty_list( &rmesa->vb.dfn_cache.MultiTexCoord1fvARB );

   radeonInitCodegen( &rmesa->vb.codegen );
}

static void free_funcs( struct dynfn *l )
{
   struct dynfn *f, *tmp;
   foreach_s (f, tmp, l) {
      remove_from_list( f );
      ALIGN_FREE( f->code );
      FREE( f );
   }
}

void radeonVtxfmtUnbindContext( GLcontext *ctx )
{
   if (RADEON_CONTEXT(ctx)->vb.installed) {
      assert(vb.context == ctx);
      VFMT_FALLBACK_OUTSIDE_BEGIN_END( __FUNCTION__ );
   }

   TNL_CONTEXT(ctx)->Driver.NotifyBegin = 0;
}


void radeonVtxfmtMakeCurrent( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

#if defined(THREADS)
   static GLboolean ThreadSafe = GL_FALSE;  /* In thread-safe mode? */
   if (!ThreadSafe) {
      static unsigned long knownID;
      static GLboolean firstCall = GL_TRUE;
      if (firstCall) {
         knownID = _glthread_GetID();
         firstCall = GL_FALSE;
      }
      else if (knownID != _glthread_GetID()) {
         ThreadSafe = GL_TRUE;

	 if (RADEON_DEBUG & (DEBUG_DRI|DEBUG_VFMT))
	    fprintf(stderr, "**** Multithread situation!\n");
      }
   }
   if (ThreadSafe) 
      return;
#endif

   if (rmesa->vb.enabled) {
      TNL_CONTEXT(ctx)->Driver.NotifyBegin = radeonNotifyBegin;
   }
}


void radeonVtxfmtDestroy( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

   count_funcs( rmesa );
   free_funcs( &rmesa->vb.dfn_cache.Vertex2f );
   free_funcs( &rmesa->vb.dfn_cache.Vertex2fv );
   free_funcs( &rmesa->vb.dfn_cache.Vertex3f );
   free_funcs( &rmesa->vb.dfn_cache.Vertex3fv );
   free_funcs( &rmesa->vb.dfn_cache.Color4ub );
   free_funcs( &rmesa->vb.dfn_cache.Color4ubv );
   free_funcs( &rmesa->vb.dfn_cache.Color3ub );
   free_funcs( &rmesa->vb.dfn_cache.Color3ubv );
   free_funcs( &rmesa->vb.dfn_cache.Color4f );
   free_funcs( &rmesa->vb.dfn_cache.Color4fv );
   free_funcs( &rmesa->vb.dfn_cache.Color3f );
   free_funcs( &rmesa->vb.dfn_cache.Color3fv );
   free_funcs( &rmesa->vb.dfn_cache.SecondaryColor3ubEXT );
   free_funcs( &rmesa->vb.dfn_cache.SecondaryColor3ubvEXT );
   free_funcs( &rmesa->vb.dfn_cache.SecondaryColor3fEXT );
   free_funcs( &rmesa->vb.dfn_cache.SecondaryColor3fvEXT );
   free_funcs( &rmesa->vb.dfn_cache.Normal3f );
   free_funcs( &rmesa->vb.dfn_cache.Normal3fv );
   free_funcs( &rmesa->vb.dfn_cache.TexCoord2f );
   free_funcs( &rmesa->vb.dfn_cache.TexCoord2fv );
   free_funcs( &rmesa->vb.dfn_cache.TexCoord1f );
   free_funcs( &rmesa->vb.dfn_cache.TexCoord1fv );
   free_funcs( &rmesa->vb.dfn_cache.MultiTexCoord2fARB );
   free_funcs( &rmesa->vb.dfn_cache.MultiTexCoord2fvARB );
   free_funcs( &rmesa->vb.dfn_cache.MultiTexCoord1fARB );
   free_funcs( &rmesa->vb.dfn_cache.MultiTexCoord1fvARB );
}

