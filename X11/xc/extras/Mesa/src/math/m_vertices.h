
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


#ifndef _M_VERTICES_H_
#define _M_VERTICES_H_

#ifdef USE_X86_ASM
#define _PROJAPI _ASMAPI
#define _PROJAPIP _ASMAPIP
#else
#define _PROJAPI
#define _PROJAPIP *
#endif


/*
 * Some handy functions for "fastpath" vertex processing in DRI drivers.
 */


typedef void (_PROJAPIP _mesa_transform_func)( GLfloat *first_vert,
                                               const GLfloat *m,
                                               const GLfloat *src,
                                               GLuint src_stride,
                                               GLuint count );

/* use count instead of last vert? */
typedef void (_PROJAPIP _mesa_cliptest_func)( GLfloat *first_vert,
                                              GLfloat *last_vert,
                                              GLubyte *or_mask,
                                              GLubyte *and_mask,
                                              GLubyte *clip_mask );

typedef void (_PROJAPIP _mesa_project_func)( GLfloat *first,
                                             GLfloat *last,
                                             const GLfloat *m,
                                             GLuint stride );

typedef void (_PROJAPIP _mesa_project_clipped_func)( GLfloat *first,
                                                     GLfloat *last,
                                                     const GLfloat *m,
                                                     GLuint stride,
                                                     const GLubyte *clipmask );

typedef void (_PROJAPIP gl_vertex_interp_func)( GLfloat t,
						GLfloat *result,
						const GLfloat *in,
						const GLfloat *out );


extern _mesa_transform_func       _mesa_xform_points3_v16_general;
extern _mesa_cliptest_func        _mesa_cliptest_points4_v16;
extern _mesa_project_func         _mesa_project_v16;
extern _mesa_project_clipped_func _mesa_project_clipped_v16;


extern void
_math_init_vertices( void );


#endif
