
/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "glheader.h"
#include "macros.h"

#include "m_vertices.h"
#include "m_xform.h"

#if defined(USE_X86_ASM)
#include "X86/common_x86_asm.h"
#endif


/* The start of a bunch of vertex oriented geometry routines.  These
 * are expected to support the production of driver-specific fast paths
 * for CVA and eventually normal processing.
 *
 * These have been taken from fxfastpath.c, and are now also used in
 * the mga driver.
 *
 * These should grow to include:
 *     - choice of 8/16 dword vertices
 *     - ??
 *     - use of portable assembly layouts.
 *
 * More tentatively:
 *     - more (all?) matrix types
 *     - more (?) vertex sizes
 *
 * -- Keith Whitwell.
 */

/* The inline 3dnow code seems to give problems with some peoples
 * compiler/binutils.
 */
/*  #undef USE_3DNOW_ASM */


#if defined(USE_X86_ASM) && defined(__GNUC__)


#endif


/*
 * Transform an array of coordinates by an arbitrary 4x4 matrix.
 * Output:  f - the results vector [][16]
 * Input:   m - the 4x4 matrix
 *          obj - pointer to first 4-component coordinate
 *          obj_stride - stride in bytes between input coordinates
 *          count - number of coordinates to transform
 */
static void _PROJAPI transform_v16( GLfloat *f,
				    const GLfloat *m,
				    const GLfloat *obj,
				    GLuint obj_stride,
				    GLuint count )
{
   GLuint i;

   for (i = 0; i < count; i++, STRIDE_F(obj, obj_stride), f += 16) {
      const GLfloat ox = obj[0], oy = obj[1], oz = obj[2];
      f[0] = m[0] * ox + m[4] * oy + m[8]  * oz + m[12];
      f[1] = m[1] * ox + m[5] * oy + m[9]  * oz + m[13];
      f[2] = m[2] * ox + m[6] * oy + m[10] * oz + m[14];
      f[3] = m[3] * ox + m[7] * oy + m[11] * oz + m[15];
   }
}


/*
 * Compute the view frustum clipmask for an array of vertices.
 * Input:  first - pointer to first vertex to process
 *                 (the stride must be 16 floats between coordinates)
 *         last - pointer to vertex just after the last vertex to process
 * Input/output:  p_clipOr - the bitwise-OR of all vertex clipmasks
 *                p_clipAnd - the bitwise-AND of all vertex clipmasks
 * Output:  clipmask - the clipmask for each vertex
 */
static void _PROJAPI cliptest_v16( GLfloat *firstVertex,
				   GLfloat *lastVertex,
				   GLubyte *p_clipOr,
				   GLubyte *p_clipAnd,
				   GLubyte *clipmask )
{
   GLubyte clipAnd = (GLubyte) ~0;
   GLubyte clipOr = 0;
   GLfloat *v = firstVertex;
   static int i;
   i = 0;

   for ( ; v != lastVertex; v += 16, clipmask++, i++) {
      const GLfloat cx = v[0];
      const GLfloat cy = v[1];
      const GLfloat cz = v[2];
      const GLfloat cw = v[3];
      GLubyte mask = 0;

      if (cx >  cw) mask |= CLIP_RIGHT_BIT;
      if (cx < -cw) mask |= CLIP_LEFT_BIT;
      if (cy >  cw) mask |= CLIP_TOP_BIT;
      if (cy < -cw) mask |= CLIP_BOTTOM_BIT;
      if (cz >  cw) mask |= CLIP_FAR_BIT;
      if (cz < -cw) mask |= CLIP_NEAR_BIT;

      *clipmask = mask;
      clipAnd &= mask;
      clipOr |= mask;
   }

   (*p_clipOr) |= clipOr;
   (*p_clipAnd) &= clipAnd;
}


/*
 * Project all vertices upto but not including last.  Guarenteed to be at
 * least one such vertex.
 * Input:  firstVertex - pointer to first vertex
 *         lastVertex - pointer to vertex just afte the last to transform
 *         m - 4x4 matrix, we only look at the scale and translate terms
 *         stride - stride in bytes between coordinates
 */
static void _PROJAPI project_verts( GLfloat *firstVertex,
				    GLfloat *lastVertex,
				    const GLfloat *m,
				    GLuint stride )
{
   const GLfloat sx = m[0], sy = m[5], sz = m[10];
   const GLfloat tx = m[12], ty = m[13], tz = m[14];
   GLfloat *v;

   for (v = firstVertex; v != lastVertex; STRIDE_F(v, stride)) {
      const GLfloat oow = 1.0F / v[3];
      v[0] = sx * v[0] * oow + tx;
      v[1] = sy * v[1] * oow + ty;
      v[2] = sz * v[2] * oow + tz;
      v[3] = oow;
   }
}

/*
 * Same as the above function, except only process coordinate [i] if
 * clipmask[i] is zero.
 */
static void _PROJAPI project_clipped_verts( GLfloat *firstVertex,
					    GLfloat *lastVertex,
					    const GLfloat *m,
					    GLuint stride,
					    const GLubyte clipmask[] )
{
   const GLfloat sx = m[0], sy = m[5], sz = m[10];
   const GLfloat tx = m[12], ty = m[13], tz = m[14];
   GLfloat *v;

   for (v = firstVertex; v != lastVertex; STRIDE_F(v, stride), clipmask++) {
      if (!(*clipmask)) {
	 const GLfloat oow = 1.0F / v[3];
	 v[0] = sx * v[0] * oow + tx;
	 v[1] = sy * v[1] * oow + ty;
	 v[2] = sz * v[2] * oow + tz;
	 v[3] = oow;
      }
   }
}


_mesa_transform_func		_mesa_xform_points3_v16_general = 0;
_mesa_cliptest_func		_mesa_cliptest_points4_v16 = 0;
_mesa_project_func		_mesa_project_v16 = 0;
_mesa_project_clipped_func	_mesa_project_clipped_v16 = 0;


void
_math_init_vertices( void )
{
   _mesa_xform_points3_v16_general	= transform_v16;
   _mesa_cliptest_points4_v16	= cliptest_v16;
   _mesa_project_v16		= project_verts;
   _mesa_project_clipped_v16	= project_clipped_verts;

#if 0
   /* GH: Add tests/benchmarks for the vertex asm */
   gl_test_all_vertex_functions( "default" );
#endif

#ifdef USE_X86_ASM
   _mesa_init_all_x86_vertex_asm();
#endif
}
