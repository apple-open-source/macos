/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_tcl.c,v 1.1 2002/10/30 12:51:57 alanh Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     Tungsten Graphics Inc., Austin, Texas.

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

#include "radeon_context.h"
#include "radeon_state.h"
#include "radeon_ioctl.h"
#include "radeon_tex.h"
#include "radeon_tcl.h"
#include "radeon_swtcl.h"
#include "radeon_maos.h"

#include "mmath.h"
#include "mtypes.h"
#include "enums.h"
#include "colormac.h"
#include "light.h"

#include "array_cache/acache.h"
#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"



/*
 * Render unclipped vertex buffers by emitting vertices directly to
 * dma buffers.  Use strip/fan hardware primitives where possible.
 * Try to simulate missing primitives with indexed vertices.
 */
#define HAVE_POINTS      1
#define HAVE_LINES       1
#define HAVE_LINE_LOOP   0
#define HAVE_LINE_STRIPS 1
#define HAVE_TRIANGLES   1
#define HAVE_TRI_STRIPS  1
#define HAVE_TRI_STRIP_1 0
#define HAVE_TRI_FANS    1
#define HAVE_QUADS       0
#define HAVE_QUAD_STRIPS 0
#define HAVE_POLYGONS    1
#define HAVE_ELTS        1


#define HW_POINTS           RADEON_CP_VC_CNTL_PRIM_TYPE_POINT
#define HW_LINES            RADEON_CP_VC_CNTL_PRIM_TYPE_LINE
#define HW_LINE_LOOP        0
#define HW_LINE_STRIP       RADEON_CP_VC_CNTL_PRIM_TYPE_LINE_STRIP
#define HW_TRIANGLES        RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_LIST
#define HW_TRIANGLE_STRIP_0 RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_STRIP
#define HW_TRIANGLE_STRIP_1 0
#define HW_TRIANGLE_FAN     RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN
#define HW_QUADS            0
#define HW_QUAD_STRIP       0
#define HW_POLYGON          RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN


static GLboolean discreet_prim[0x10] = {
   0,				/* none */
   1,				/* points */
   1,				/* lines */
   0,				/* line_strip */
   1,				/* tri_list */
   0,				/* tri_fan */
   0,				/* tri_type_2 */
   1,				/* rect list (unused) */
   1,				/* 3 vert point */
   1,				/* 3 vert line */
   0,
   0,
   0,
   0,
   0,
};
   

#define LOCAL_VARS radeonContextPtr rmesa = RADEON_CONTEXT(ctx)
#define ELTS_VARS  GLushort *dest

#define ELT_INIT(prim, hw_prim) \
   radeonTclPrimitive( ctx, prim, hw_prim | RADEON_CP_VC_CNTL_PRIM_WALK_IND )

#define GET_ELTS() rmesa->tcl.Elts


#define NEW_PRIMITIVE()  RADEON_NEWPRIM( rmesa )
#define NEW_BUFFER()  radeonRefillCurrentDmaRegion( rmesa )

/* Don't really know how many elts will fit in what's left of cmdbuf,
 * as there is state to emit, etc:
 */

#if 0
#define GET_CURRENT_VB_MAX_ELTS() \
   ((RADEON_CMD_BUF_SZ - (rmesa->store.cmd_used + 16)) / 2) 
#define GET_SUBSEQUENT_VB_MAX_ELTS() ((RADEON_CMD_BUF_SZ - 16) / 2) 
#else
/* Testing on isosurf shows a maximum around here.  Don't know if it's
 * the card or driver or kernel module that is causing the behaviour.
 */
#define GET_CURRENT_VB_MAX_ELTS() 300
#define GET_SUBSEQUENT_VB_MAX_ELTS() 300
#endif

#define RESET_STIPPLE() do {			\
   RADEON_STATECHANGE( rmesa, lin );		\
   radeonEmitState( rmesa );			\
} while (0)

#define AUTO_STIPPLE( mode )  do {		\
   RADEON_STATECHANGE( rmesa, lin );		\
   if (mode)					\
      rmesa->hw.lin.cmd[LIN_RE_LINE_PATTERN] |=	\
	 RADEON_LINE_PATTERN_AUTO_RESET;	\
   else						\
      rmesa->hw.lin.cmd[LIN_RE_LINE_PATTERN] &=	\
	 ~RADEON_LINE_PATTERN_AUTO_RESET;	\
   radeonEmitState( rmesa );			\
} while (0)


/* How do you extend an existing primitive?
 */
#define ALLOC_ELTS(nr)							\
do {									\
   if (rmesa->dma.flush == radeonFlushElts &&				\
       rmesa->store.cmd_used + nr*2 < RADEON_CMD_BUF_SZ) {		\
									\
      dest = (GLushort *)(rmesa->store.cmd_buf + 			\
			  rmesa->store.cmd_used);			\
      rmesa->store.cmd_used += nr*2;					\
   }									\
   else {								\
      if (rmesa->dma.flush)						\
	 rmesa->dma.flush( rmesa );					\
									\
      radeonEmitAOS( rmesa,						\
	  	     rmesa->tcl.aos_components,				\
		     rmesa->tcl.nr_aos_components,			\
		     0 );						\
									\
      dest = radeonAllocEltsOpenEnded( rmesa,				\
				       rmesa->tcl.vertex_format,	\
				       rmesa->tcl.hw_primitive,		\
				       nr );				\
   }									\
} while (0) 



/* TODO: Try to extend existing primitive if both are identical,
 * discreet and there are no intervening state changes.  (Somewhat
 * duplicates changes to DrawArrays code)
 */
static void EMIT_PRIM( GLcontext *ctx, 
		       GLenum prim, 
		       GLuint hwprim, 
		       GLuint start, 
		       GLuint count)	
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );
   radeonTclPrimitive( ctx, prim, hwprim );
   
   radeonEmitAOS( rmesa,
		  rmesa->tcl.aos_components,
		  rmesa->tcl.nr_aos_components,
		  start );
   
   /* Why couldn't this packet have taken an offset param?
    */
   radeonEmitVbufPrim( rmesa,
		       rmesa->tcl.vertex_format,
		       rmesa->tcl.hw_primitive,
		       count - start );
}



/* Try & join small primitives
 */
#if 0
#define PREFER_DISCRETE_ELT_PRIM( NR, PRIM ) 0
#else
#define PREFER_DISCRETE_ELT_PRIM( NR, PRIM )			\
  ((NR) < 20 ||							\
   ((NR) < 40 &&						\
    rmesa->tcl.hw_primitive == (PRIM|				\
			    RADEON_CP_VC_CNTL_PRIM_WALK_IND|	\
			    RADEON_CP_VC_CNTL_TCL_ENABLE)))
#endif

#ifdef MESA_BIG_ENDIAN
/* We could do without (most of) this ugliness if dest was always 32 bit word aligned... */
#define EMIT_ELT(offset, x) do {				\
	int off = offset + ( ( (GLuint)dest & 0x2 ) >> 1 );	\
	GLushort *des = (GLushort *)( (GLuint)dest & ~0x2 );	\
	(des)[ off + 1 - 2 * ( off & 1 ) ] = (GLushort)(x); } while (0)
#else
#define EMIT_ELT(offset, x) (dest)[offset] = (GLushort) (x)
#endif
#define EMIT_TWO_ELTS(offset, x, y)  *(GLuint *)(dest+offset) = ((y)<<16)|(x);
#define INCR_ELTS( nr ) dest += nr
#define RELEASE_ELT_VERTS() \
   radeonReleaseArrays( ctx, ~0 )



#define TAG(x) tcl_##x
#include "tnl_dd/t_dd_dmatmp2.h"

/**********************************************************************/
/*                          External entrypoints                     */
/**********************************************************************/

void radeonEmitPrimitive( GLcontext *ctx, 
			  GLuint first,
			  GLuint last,
			  GLuint flags )
{
   tcl_render_tab_verts[flags&PRIM_MODE_MASK]( ctx, first, last, flags );
}

void radeonEmitEltPrimitive( GLcontext *ctx, 
			     GLuint first,
			     GLuint last,
			     GLuint flags )
{
   tcl_render_tab_elts[flags&PRIM_MODE_MASK]( ctx, first, last, flags );
}

void radeonTclPrimitive( GLcontext *ctx, 
			 GLenum prim,
			 int hw_prim )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLuint se_cntl;
   GLuint newprim = hw_prim | RADEON_CP_VC_CNTL_TCL_ENABLE;

   if (newprim != rmesa->tcl.hw_primitive ||
       !discreet_prim[hw_prim&0xf]) {
      RADEON_NEWPRIM( rmesa );
      rmesa->tcl.hw_primitive = newprim;
   }

   se_cntl = rmesa->hw.set.cmd[SET_SE_CNTL];
   se_cntl &= ~RADEON_FLAT_SHADE_VTX_LAST;

   if (prim == GL_POLYGON && (ctx->_TriangleCaps & DD_FLATSHADE)) 
      se_cntl |= RADEON_FLAT_SHADE_VTX_0;
   else
      se_cntl |= RADEON_FLAT_SHADE_VTX_LAST;

   if (se_cntl != rmesa->hw.set.cmd[SET_SE_CNTL]) {
      RADEON_STATECHANGE( rmesa, set );
      rmesa->hw.set.cmd[SET_SE_CNTL] = se_cntl;
   }
}


/**********************************************************************/
/*                          Render pipeline stage                     */
/**********************************************************************/


/* TCL render.
 */
static GLboolean radeon_run_tcl_render( GLcontext *ctx,
					struct gl_pipeline_stage *stage )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLuint i,flags = 0,length;

   /* TODO: separate this from the swtnl pipeline 
    */
   if (rmesa->TclFallback)
      return GL_TRUE;	/* fallback to software t&l */

   if (VB->Count == 0)
      return GL_FALSE;

   radeonReleaseArrays( ctx, stage->changed_inputs );
   radeonEmitArrays( ctx, stage->inputs );

   rmesa->tcl.Elts = VB->Elts;

   for (i = VB->FirstPrimitive ; !(flags & PRIM_LAST) ; i += length)
   {
      flags = VB->Primitive[i];
      length = VB->PrimitiveLength[i];

      if (RADEON_DEBUG & DEBUG_PRIMS)
	 fprintf(stderr, "%s: prim %s %d..%d\n", 
		 __FUNCTION__,
		 _mesa_lookup_enum_by_nr(flags & PRIM_MODE_MASK), 
		 i, i+length);

      if (!length)
	 continue;

      if (rmesa->tcl.Elts)
	 radeonEmitEltPrimitive( ctx, i, i+length, flags );
      else
	 radeonEmitPrimitive( ctx, i, i+length, flags );
   }

   return GL_FALSE;		/* finished the pipe */
}



static void radeon_check_tcl_render( GLcontext *ctx,
				     struct gl_pipeline_stage *stage )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLuint inputs = VERT_OBJ;

   if (ctx->RenderMode == GL_RENDER) {
      /* Make all this event-driven:
       */
      if (ctx->Light.Enabled) {
	 inputs |= VERT_NORM;

	 if (ctx->Light.ColorMaterialEnabled) {
	    inputs |= VERT_RGBA;
	 }
      }
      else {
	 inputs |= VERT_RGBA;
	 
	 if (ctx->_TriangleCaps & DD_SEPARATE_SPECULAR) {
	    inputs |= VERT_SPEC_RGB;
	 }
      }

      if (ctx->Texture.Unit[0]._ReallyEnabled) {
	 if (ctx->Texture.Unit[0].TexGenEnabled) {
	    if (rmesa->TexGenNeedNormals[0]) {
	       inputs |= VERT_NORM;
	    }
	 } else {
	    inputs |= VERT_TEX(0);
	 }
      }

      if (ctx->Texture.Unit[1]._ReallyEnabled) {
	 if (ctx->Texture.Unit[1].TexGenEnabled) {
	    if (rmesa->TexGenNeedNormals[1]) {
	       inputs |= VERT_NORM;
	    }
	 } else {
	    inputs |= VERT_TEX(1);
	 }
      }

      stage->inputs = inputs;
      stage->active = 1;
   }
   else
      stage->active = 0;
}

static void radeon_init_tcl_render( GLcontext *ctx,
				    struct gl_pipeline_stage *stage )
{
   stage->check = radeon_check_tcl_render;
   stage->check( ctx, stage );
}

static void dtr( struct gl_pipeline_stage *stage )
{
   (void)stage;
}


/* Initial state for tcl stage.  
 */
const struct gl_pipeline_stage _radeon_tcl_stage =
{
   "radeon render",
   (_DD_NEW_SEPARATE_SPECULAR |
    _NEW_LIGHT|
    _NEW_TEXTURE|
    _NEW_FOG|
    _NEW_RENDERMODE),		/* re-check (new inputs) */
   0,				/* re-run (always runs) */
   GL_TRUE,			/* active */
   0, 0,			/* inputs (set in check_render), outputs */
   0, 0,			/* changed_inputs, private */
   dtr,				/* destructor */
   radeon_init_tcl_render,	/* check - initially set to alloc data */
   radeon_run_tcl_render	/* run */
};



/**********************************************************************/
/*                 Validate state at pipeline start                   */
/**********************************************************************/


/*-----------------------------------------------------------------------
 * Manage TCL fallbacks
 */


static void transition_to_swtnl( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   GLuint se_cntl;

   RADEON_NEWPRIM( rmesa );
   rmesa->swtcl.vertex_format = 0;

   radeonChooseVertexState( ctx );
   radeonChooseRenderState( ctx );

   _mesa_validate_all_lighting_tables( ctx ); 

   tnl->Driver.NotifyMaterialChange = 
      _mesa_validate_all_lighting_tables;

   radeonReleaseArrays( ctx, ~0 );

   se_cntl = rmesa->hw.set.cmd[SET_SE_CNTL];
   se_cntl |= RADEON_FLAT_SHADE_VTX_LAST;
	 
   if (se_cntl != rmesa->hw.set.cmd[SET_SE_CNTL]) {
      RADEON_STATECHANGE( rmesa, set );
      rmesa->hw.set.cmd[SET_SE_CNTL] = se_cntl;
   }
}


static void transition_to_hwtnl( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   GLuint se_coord_fmt = (RADEON_VTX_W0_IS_NOT_1_OVER_W0 |
			  RADEON_TEX1_W_ROUTING_USE_Q1);

   if ( se_coord_fmt != rmesa->hw.set.cmd[SET_SE_COORDFMT] ) {
      RADEON_STATECHANGE( rmesa, set );
      rmesa->hw.set.cmd[SET_SE_COORDFMT] = se_coord_fmt;
      _tnl_need_projected_coords( ctx, GL_FALSE );
   }

   radeonUpdateMaterial( ctx );

   tnl->Driver.NotifyMaterialChange = radeonUpdateMaterial;

   if ( rmesa->dma.flush )			
      rmesa->dma.flush( rmesa );	

   rmesa->dma.flush = 0;
   rmesa->swtcl.vertex_format = 0;
   
   if (rmesa->swtcl.indexed_verts.buf) 
      radeonReleaseDmaRegion( rmesa, &rmesa->swtcl.indexed_verts, 
			      __FUNCTION__ );

   if (RADEON_DEBUG & DEBUG_FALLBACKS) 
      fprintf(stderr, "Radeon end tcl fallback\n");
}

static char *fallbackStrings[] = {
   "Rasterization fallback",
   "Unfilled triangles",
   "Twosided lighting, differing materials",
   "Materials in VB (maybe between begin/end)",
   "Texgen unit 0",
   "Texgen unit 1",
   "Texgen unit 2",
   "User disable"
};


static char *getFallbackString(GLuint bit)
{
   int i = 0;
   while (bit > 1) {
      i++;
      bit >>= 1;
   }
   return fallbackStrings[i];
}



void radeonTclFallback( GLcontext *ctx, GLuint bit, GLboolean mode )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLuint oldfallback = rmesa->TclFallback;

   if (mode) {
      rmesa->TclFallback |= bit;
      if (oldfallback == 0) {
	 if (RADEON_DEBUG & DEBUG_FALLBACKS) 
	    fprintf(stderr, "Radeon begin tcl fallback %s\n",
		    getFallbackString( bit ));
	 transition_to_swtnl( ctx );
      }
   }
   else {
      rmesa->TclFallback &= ~bit;
      if (oldfallback == bit) {
	 if (RADEON_DEBUG & DEBUG_FALLBACKS) 
	    fprintf(stderr, "Radeon end tcl fallback %s\n",
		    getFallbackString( bit ));
	 transition_to_hwtnl( ctx );
      }
   }
}
