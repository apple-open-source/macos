
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
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 */

#include "glheader.h"
#include "context.h"
#include "macros.h"
#include "mem.h"

#include "m_matrix.h"
#include "m_vertices.h"
#include "m_xform.h"

#include "m_debug.h"
#include "m_debug_util.h"


#ifdef DEBUG  /* This code only used for debugging */


#define NUM_V16_FUNCS	4

#ifdef RUN_DEBUG_BENCHMARK
static char *v16_strings[NUM_V16_FUNCS] = {
   "_mesa_xform_points3_v16_general",
   "_mesa_cliptest_points4_v16",
   "_mesa_project_v16",
   "_mesa_project_clipped_v16"
};
#endif


/* =============================================================
 * Reference transformations
 */

static void ref_transform_v16( GLfloat *f,
			       const GLfloat *m,
			       const GLfloat *obj,
			       GLuint obj_stride,
			       GLuint count )
{
   GLuint i;

   for ( i = 0 ; i < count ; i++, STRIDE_F(obj, obj_stride), f += 16 ) {
      const GLfloat ox = obj[0], oy = obj[1], oz = obj[2];
      f[0] = m[0] * ox + m[4] * oy + m[8]  * oz + m[12];
      f[1] = m[1] * ox + m[5] * oy + m[9]  * oz + m[13];
      f[2] = m[2] * ox + m[6] * oy + m[10] * oz + m[14];
      f[3] = m[3] * ox + m[7] * oy + m[11] * oz + m[15];
   }
}

static void ref_cliptest_v16( GLfloat *first,
			      GLfloat *last,
			      GLubyte *p_clipOr,
			      GLubyte *p_clipAnd,
			      GLubyte *clipmask )
{
   GLubyte clipAnd = (GLubyte) ~0;
   GLubyte clipOr = 0;
   GLfloat *f = first;
   static int i;
   i = 0;

   for ( ; f != last ; f += 16, clipmask++, i++ ) {
      const GLfloat cx = f[0];
      const GLfloat cy = f[1];
      const GLfloat cz = f[2];
      const GLfloat cw = f[3];
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

static void ref_project_verts( GLfloat *first,
			       GLfloat *last,
			       const GLfloat *m,
			       GLuint stride )
{
   const GLfloat sx = m[0], sy = m[5], sz = m[10];
   const GLfloat tx = m[12], ty = m[13], tz = m[14];
   GLfloat *f;

   for (f = first; f != last; STRIDE_F(f, stride)) {
      const GLfloat oow = 1.0F / f[3];
      f[0] = sx * f[0] * oow + tx;
      f[1] = sy * f[1] * oow + ty;
      f[2] = sz * f[2] * oow + tz;
      f[3] = oow;
   }
}

static void ref_project_clipped_verts( GLfloat *first,
				       GLfloat *last,
				       const GLfloat *m,
				       GLuint stride,
				       const GLubyte *clipmask )
{
   const GLfloat sx = m[0], sy = m[5], sz = m[10];
   const GLfloat tx = m[12], ty = m[13], tz = m[14];
   GLfloat *f;

   for ( f = first ; f != last ; STRIDE_F(f, stride), clipmask++ ) {
      if (!(*clipmask)) {
	 const GLfloat oow = 1.0F / f[3];
	 f[0] = sx * f[0] * oow + tx;
	 f[1] = sy * f[1] * oow + ty;
	 f[2] = sz * f[2] * oow + tz;
	 f[3] = oow;
      }
   }
}



/* =============================================================
 * Vertex transformation, clipping etc tests
 */

static GLfloat ALIGN16(s[TEST_COUNT][4]);
static GLfloat ALIGN16(d[TEST_COUNT][16]);
static GLfloat ALIGN16(r[TEST_COUNT][16]);

static int test_transform_function( long *cycles )
{
   GLvector4f source[1];
   GLfloat *m;
   int i, j;
#ifdef  RUN_DEBUG_BENCHMARK
   int cycle_i;                /* the counter for the benchmarks we run */
#endif

   m = (GLfloat *) ALIGN_MALLOC( 16 * sizeof(GLfloat), 16 );

   m[0] = 63.0; m[4] = 43.0; m[ 8] = 29.0; m[12] = 43.0;
   m[1] = 55.0; m[5] = 17.0; m[ 9] = 31.0; m[13] =  7.0;
   m[2] = 44.0; m[6] =  9.0; m[10] =  7.0; m[14] =  3.0;
   m[3] = 11.0; m[7] = 23.0; m[11] = 91.0; m[15] =  9.0;

   for ( i = 0 ; i < TEST_COUNT ; i++ ) {
      d[i][0] = s[i][0] = 0.0;
      d[i][1] = s[i][1] = 0.0;
      d[i][2] = s[i][2] = 0.0;
      d[i][3] = s[i][3] = 1.0;
      for ( j = 0 ; j < 3 ; j++ )
         s[i][j] = rnd();
   }

   source->data = (GLfloat(*)[4])s;
   source->start = (GLfloat *)s;
   source->count = TEST_COUNT;
   source->stride = sizeof(s[0]);
   source->size = 4;
   source->flags = 0;

   ref_transform_v16( (GLfloat *)r,
		      m,
		      source->start,
		      source->stride,
		      TEST_COUNT );

   if ( mesa_profile ) {
      BEGIN_RACE( *cycles );
      _mesa_xform_points3_v16_general( (GLfloat *)d,
                                       m,
                                       source->start,
                                       source->stride,
                                       TEST_COUNT );
      END_RACE( *cycles );
   } else {
      _mesa_xform_points3_v16_general( (GLfloat *)d,
                                       m,
                                       source->start,
                                       source->stride,
                                       TEST_COUNT );
   }

   ALIGN_FREE( m );

   for ( i = 0 ; i < TEST_COUNT ; i++ ) {
      for ( j = 0 ; j < 4 ; j++ ) {
         if ( significand_match( d[i][j], r[i][j] ) < REQUIRED_PRECISION ) {
            printf( "-----------------------------\n" );
            printf( "(i = %i, j = %i)\n", i, j );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][0], r[i][0], r[i][0]-d[i][0],
		    MAX_PRECISION - significand_match( d[i][0], r[i][0] ) );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][1], r[i][1], r[i][1]-d[i][1],
		    MAX_PRECISION - significand_match( d[i][1], r[i][1] ) );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][2], r[i][2], r[i][2]-d[i][2],
		    MAX_PRECISION - significand_match( d[i][2], r[i][2] ) );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][3], r[i][3], r[i][3]-d[i][3],
		    MAX_PRECISION - significand_match( d[i][3], r[i][3] ) );
	    return 0;
         }
      }
   }

   return 1;
}

static int test_cliptest_function( long *cycles )
{
   GLvector4f source[1];
   GLubyte dco, dca, rco, rca;
   GLubyte dm[TEST_COUNT], rm[TEST_COUNT];
   GLfloat *m;
   int i, j;
#ifdef  RUN_DEBUG_BENCHMARK
   int cycle_i;                /* the counter for the benchmarks we run */
#endif

   m = (GLfloat *) ALIGN_MALLOC( 16 * sizeof(GLfloat), 16 );

   init_matrix( m );

   for ( i = 0 ; i < TEST_COUNT ; i++ ) {
      d[i][0] = s[i][0] = 0.0;
      d[i][1] = s[i][1] = 0.0;
      d[i][2] = s[i][2] = 0.0;
      d[i][3] = s[i][3] = 1.0;
      for ( j = 0 ; j < 3 ; j++ )
         s[i][j] = rnd();
   }

   source->data = (GLfloat(*)[4])s;
   source->start = (GLfloat *)s;
   source->count = TEST_COUNT;
   source->stride = sizeof(s[0]);
   source->size = 4;
   source->flags = 0;

   /* Build up a reference list of transformed points
    */
   ref_transform_v16( (GLfloat *)r,
		      m,
		      source->start,
		      source->stride,
		      TEST_COUNT );

   dca = rca = ~0;
   dco = rco = 0;

   ref_cliptest_v16( (GLfloat *)r,
		     (GLfloat *)&r[TEST_COUNT][0],
		     &rco,
		     &rca,
		     &rm[0] );

   if ( mesa_profile ) {
      BEGIN_RACE( *cycles );
      _mesa_cliptest_points4_v16( (GLfloat *)r,
			       (GLfloat *)&r[TEST_COUNT][0],
			       &dco,
			       &dca,
			       &dm[0] );
      END_RACE( *cycles );
   } else {
      _mesa_cliptest_points4_v16( (GLfloat *)r,
			       (GLfloat *)&r[TEST_COUNT][0],
			       &dco,
			       &dca,
			       dm );
   }

   ALIGN_FREE( m );

   if ( dco != rco ) {
      printf( "-----------------------------\n" );
      printf( "dco = 0x%02x   rco = 0x%02x\n", dco, rco );
      return 0;
   }
   if ( dca != rca ) {
      printf( "-----------------------------\n" );
      printf( "dca = 0x%02x   rca = 0x%02x\n", dca, rca );
      return 0;
   }
   for ( i = 0 ; i < TEST_COUNT ; i++ ) {
      if ( dm[i] != rm[i] ) {
	 printf( "-----------------------------\n" );
	 printf( "(i = %i)\n", i );
	 printf( "dm = 0x%02x   rm = 0x%02x\n", dm[i], rm[i] );
	 return 0;
      }
   }

   return 1;
}

static int test_project_function( long *cycles, int clipped )
{
   GLvector4f source[1];
   GLubyte co, ca;
   GLubyte mask[TEST_COUNT];
   GLfloat *m;
   int i, j;
#ifdef  RUN_DEBUG_BENCHMARK
   int cycle_i;                /* the counter for the benchmarks we run */
#endif

   m = (GLfloat *) ALIGN_MALLOC( 16 * sizeof(GLfloat), 16 );

   init_matrix( m );

   for ( i = 0 ; i < TEST_COUNT ; i++ ) {
      d[i][0] = s[i][0] = 0.0;
      d[i][1] = s[i][1] = 0.0;
      d[i][2] = s[i][2] = 0.0;
      d[i][3] = s[i][3] = 1.0;
      for ( j = 0 ; j < 3 ; j++ )
         s[i][j] = rnd();
   }

   source->data = (GLfloat(*)[4])s;
   source->start = (GLfloat *)s;
   source->count = TEST_COUNT;
   source->stride = sizeof(s[0]);
   source->size = 4;
   source->flags = 0;

   /* Build up a reference list of transformed points
    */
   ref_transform_v16( (GLfloat *)d,
		      m,
		      source->start,
		      source->stride,
		      TEST_COUNT );
   ref_transform_v16( (GLfloat *)r,
		      m,
		      source->start,
		      source->stride,
		      TEST_COUNT );

   if ( clipped ) {
      ca = ~0;
      co = 0;
      ref_cliptest_v16( (GLfloat *)r,
			(GLfloat *)&r[TEST_COUNT][0],
			&co,
			&ca,
			mask );
      ref_project_clipped_verts( (GLfloat *)r,
				 (GLfloat *)&r[TEST_COUNT][0],
				 m,
				 16 * 4,
				 mask );
   } else {
      ref_project_verts( (GLfloat *)r,
			 (GLfloat *)&r[TEST_COUNT][0],
			 m,
			 16 * 4 );
   }

   if ( mesa_profile ) {
      if ( clipped ) {
	 /*BEGIN_RACE( *cycles );*/
	 _mesa_project_clipped_v16( (GLfloat *)d,
				 (GLfloat *)&d[TEST_COUNT][0],
				 m,
				 16 * 4,
				 mask );
	 /*END_RACE( *cycles );*/
      } else {
	 /*BEGIN_RACE( *cycles );*/
	 _mesa_project_v16( (GLfloat *)d,
			 (GLfloat *)&d[TEST_COUNT][0],
			 m,
			 16 * 4 );
	 /*END_RACE( *cycles );*/
      }
   } else {
      if ( clipped ) {
	 _mesa_project_clipped_v16( (GLfloat *)d,
				 (GLfloat *)&d[TEST_COUNT][0],
				 m,
				 16 * 4,
				 mask );
      } else {
	 _mesa_project_v16( (GLfloat *)d,
			 (GLfloat *)&d[TEST_COUNT][0],
			 m,
			 16 * 4 );
      }
   }

   ALIGN_FREE( m );

   for ( i = 0 ; i < TEST_COUNT ; i++ ) {
      for ( j = 0 ; j < 4 ; j++ ) {
         if ( significand_match( d[i][j], r[i][j] ) < REQUIRED_PRECISION ) {
            printf( "-----------------------------\n" );
            printf( "(i = %i, j = %i)\n", i, j );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][0], r[i][0], r[i][0]-d[i][0],
		    MAX_PRECISION - significand_match( d[i][0], r[i][0] ) );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][1], r[i][1], r[i][1]-d[i][1],
		    MAX_PRECISION - significand_match( d[i][1], r[i][1] ) );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][2], r[i][2], r[i][2]-d[i][2],
		    MAX_PRECISION - significand_match( d[i][2], r[i][2] ) );
            printf( "%f \t %f \t [diff = %e - %i bit missed]\n",
		    d[i][3], r[i][3], r[i][3]-d[i][3],
		    MAX_PRECISION - significand_match( d[i][3], r[i][3] ) );
	    return 0;
         }
      }
   }

   return 1;
}

void _math_test_all_vertex_functions( char *description )
{
   long benchmark_tab[NUM_V16_FUNCS];
   long *cycles;
   static int first_time = 1;

   if ( first_time ) {
      first_time = 0;
      mesa_profile = getenv( "MESA_PROFILE" );
   }

#ifdef RUN_DEBUG_BENCHMARK
   if ( mesa_profile ) {
      int i;
      if ( !counter_overhead ) {
	 INIT_COUNTER();
	 printf( "counter overhead: %ld cycles\n\n", counter_overhead );
      }
      printf( "fastpath vertex results after hooking in %s functions:\n",
	      description );
      printf( "\n--------------------------------------------------------\n" );

      for ( i = 0 ; i < NUM_V16_FUNCS ; i++ ) {
	 benchmark_tab[i] = 0;
      }
   }
#endif

   /* Test fastpath transformation
    */
   cycles = &benchmark_tab[0];

   if ( test_transform_function( cycles ) == 0 ) {
      char buf[100];
      sprintf( buf, "_mesa_xform_points3_v16_general failed test (%s)",
	       description );
      _mesa_problem( NULL, buf );
   }

   /* Test fastpath clipping
    */
   cycles = &benchmark_tab[1];

   if ( test_cliptest_function( cycles ) == 0 ) {
      char buf[100];
      sprintf( buf, "_mesa_cliptest_points4_v16 failed test (%s)",
	       description );
      _mesa_problem( NULL, buf );
   }

   /* Test fastpath projection
    */
   cycles = &benchmark_tab[2];

   if ( test_project_function( cycles, 0 ) == 0 ) {
      char buf[100];
      sprintf( buf, "_mesa_project_v16 failed test (%s)",
	       description );
      _mesa_problem( NULL, buf );
   }

   cycles = &benchmark_tab[3];

   if ( test_project_function( cycles, 1 ) == 0 ) {
      char buf[100];
      sprintf( buf, "_mesa_project_clipped_v16 failed test (%s)",
	       description );
      _mesa_problem( NULL, buf );
   }


#ifdef RUN_DEBUG_BENCHMARK
   if ( mesa_profile ) {
      int i;
      for ( i = 0 ; i < NUM_V16_FUNCS ; i++ ) {
	 printf( " %li\t | [%s]\n", benchmark_tab[i], v16_strings[i] );
      }
      printf( "\n" );
   }
#endif
}


#endif /* DEBUG */
