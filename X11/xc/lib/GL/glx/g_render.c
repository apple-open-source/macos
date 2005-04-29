/* $XFree86: xc/lib/GL/glx/g_render.c,v 1.9 2004/02/02 00:01:18 alanh Exp $ */
/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: This software was created using the
** OpenGL(R) version 1.2.1 Sample Implementation published by SGI, but has
** not been independently verified as being compliant with the OpenGL(R)
** version 1.2.1 Specification.
*/

#include "packrender.h"
#include "size.h"

#define GLdouble_SIZE   8
#define GLclampd_SIZE   8
#define GLfloat_SIZE    4
#define GLclampf_SIZE   4
#define GLint_SIZE      4
#define GLuint_SIZE     4
#define GLenum_SIZE     4
#define GLbitfield_SIZE 4
#define GLshort_SIZE    2
#define GLushort_SIZE   2
#define GLbyte_SIZE     1
#define GLubyte_SIZE    1
#define GLboolean_SIZE  1

#define __GLX_PUT_GLdouble(offset,value)   __GLX_PUT_DOUBLE(offset,value)
#define __GLX_PUT_GLclampd(offset,value)   __GLX_PUT_DOUBLE(offset,value)
#define __GLX_PUT_GLfloat(offset,value)    __GLX_PUT_FLOAT(offset,value)
#define __GLX_PUT_GLclampf(offset,value)   __GLX_PUT_FLOAT(offset,value)
#define __GLX_PUT_GLint(offset,value)      __GLX_PUT_LONG(offset,value)
#define __GLX_PUT_GLuint(offset,value)     __GLX_PUT_LONG(offset,value)
#define __GLX_PUT_GLenum(offset,value)     __GLX_PUT_LONG(offset,value)
#define __GLX_PUT_GLbitfield(offset,value) __GLX_PUT_LONG(offset,value)
#define __GLX_PUT_GLshort(offset,value)    __GLX_PUT_SHORT(offset,value)
#define __GLX_PUT_GLushort(offset,value)   __GLX_PUT_SHORT(offset,value)
#define __GLX_PUT_GLbyte(offset,value)     __GLX_PUT_CHAR(offset,value)
#define __GLX_PUT_GLubyte(offset,value)    __GLX_PUT_CHAR(offset,value)
#define __GLX_PUT_GLboolean(offset,value)  __GLX_PUT_CHAR(offset,value)

#define __GLX_PUT_GLdouble_ARRAY(offset,ptr,count)   __GLX_PUT_DOUBLE_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLclampd_ARRAY(offset,ptr,count)   __GLX_PUT_DOUBLE_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLfloat_ARRAY(offset,ptr,count)    __GLX_PUT_FLOAT_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLclampf_ARRAY(offset,ptr,count)   __GLX_PUT_FLOAT_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLint_ARRAY(offset,ptr,count)      __GLX_PUT_LONG_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLuint_ARRAY(offset,ptr,count)     __GLX_PUT_LONG_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLenum_ARRAY(offset,ptr,count)     __GLX_PUT_LONG_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLshort_ARRAY(offset,ptr,count)    __GLX_PUT_SHORT_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLushort_ARRAY(offset,ptr,count)   __GLX_PUT_SHORT_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLbyte_ARRAY(offset,ptr,count)     __GLX_PUT_CHAR_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLubyte_ARRAY(offset,ptr,count)    __GLX_PUT_CHAR_ARRAY(offset,ptr,count)
#define __GLX_PUT_GLboolean_ARRAY(offset,ptr,count)  __GLX_PUT_CHAR_ARRAY(offset,ptr,count)

#define RENDER_SIZE(t,c)  (__GLX_PAD(4 + (t ## _SIZE * c)))

/* GLX protocol templates are named in the following manner.  All templates
 * begin with the string 'glxproto_'.  Following is an optional list of
 * scalar parameters.  The scalars are listed as type and number.  The most
 * common being \c enum1 (one scalar enum) and \c enum2 (two scalar enums).
 *
 * The final part of the name describes the number of named-type parameters
 * and how they are passed.
 * - One or more digits followed by the letter s means
 *   that the specified number of parameters are passed as scalars.  The macro
 *   \c glxproto_3s generates a function that takes 3 scalars, such as
 *   \c glVertex3f.
 * - A capital C follwed by a lower-case v means that a constant
 *   sized vector is passed.  Macros of this type take an extra parameter,
 *   which is the size of the vector.  The invocation
 *   'glxproto_Cv(Vertex3fv, X_GLrop_Vertexfv, GLfloat, 3)' would generate the
 *   correct protocol for the \c glVertex3fv function.
 * - A capital V followed by a lower-case v means that a variable sized
 *   vector is passed.  The function generated by these macros will call
 *   a co-function to determine the size of the vector.  The name of the
 *   co-function is generated by prepending \c __gl and appending \c _size
 *   to the base name of the function.  The invocation
 *   'glxproto_enum1_Vv(Fogiv, X_GLrop_Fogiv, GLint)' would generate the
 *   correct protocol for the \c glFogiv function.
 * - One or more digits without a following letter means that a function
 *   taking the specified number of scalar parameters and a function with a
 *   vector parameter of the specified size should be generated.  The letter
 *   v is automatically appended to the name of the vector-based function in
 *   this case.  The invocation
 *   'glxproto_3(Vertex3f, X_GLrop_Vertex3fv, GLfloat)' would generate the
 *   correct protocol for both \c glVertex3f and \c glVertex3fv.
 *
 * glxproto_void is a special case for functions that take no parameters
 * (i.e., glEnd).
 *
 * An addition form is 'glxvendr_'.  This is identical to the other forms
 * with the exception of taking an additional parameter (to the macro) which
 * is a vendor string to append to the function name.  The invocation
 * 'glxproto_3(Foo3f, X_GLrop_Foo3fv, GLfloat)' would generate the functions
 * 'glFoo3fv' and 'glFoo3f', and the invocation
 * 'glxvendr_3(Foo3f, X_GLrop_Foo3fv, GLfloat, EXT)' would generate the
 * functions 'glFoo3fvEXT' and 'glFoo3fEXT'.
 */

#define glxproto_Cv(name, rop, type, count) \
   void __indirect_gl ## name (const type * v) \
   { \
      __GLX_DECLARE_VARIABLES(); \
      __GLX_LOAD_VARIABLES(); \
      cmdlen = RENDER_SIZE(type, count); \
      __GLX_BEGIN(rop, cmdlen); \
      if (count <= 4) { \
	                  __GLX_PUT_ ## type (4 + (0 * type ## _SIZE), v[0]); \
	 if (count > 1) { __GLX_PUT_ ## type (4 + (1 * type ## _SIZE), v[1]); } \
	 if (count > 2) { __GLX_PUT_ ## type (4 + (2 * type ## _SIZE), v[2]); } \
	 if (count > 3) { __GLX_PUT_ ## type (4 + (3 * type ## _SIZE), v[3]); } \
      } else { \
	 __GLX_PUT_ ## type ## _ARRAY(4, v, count); \
      } \
      __GLX_END(cmdlen); \
   }

#define glxproto_1s(name, rop, type) \
   void __indirect_gl ## name (type v1) \
   { \
      __GLX_DECLARE_VARIABLES(); \
      __GLX_LOAD_VARIABLES(); \
      cmdlen = RENDER_SIZE(type, 1); \
      __GLX_BEGIN(rop, cmdlen); \
      __GLX_PUT_ ## type (4 + (0 * type ## _SIZE), v1); \
      __GLX_END(cmdlen); \
   }

#define glxproto_3s(name, rop, type) \
   void __indirect_gl ## name (type v1, type v2, type v3) \
   { \
      __GLX_DECLARE_VARIABLES(); \
      __GLX_LOAD_VARIABLES(); \
      cmdlen = RENDER_SIZE(type, 3); \
      __GLX_BEGIN(rop, cmdlen); \
      __GLX_PUT_ ## type (4 + (0 * type ## _SIZE), v1); \
      __GLX_PUT_ ## type (4 + (1 * type ## _SIZE), v2); \
      __GLX_PUT_ ## type (4 + (2 * type ## _SIZE), v3); \
      __GLX_END(cmdlen); \
   }

#define glxproto_4s(name, rop, type) \
   void __indirect_gl ## name (type v1, type v2, type v3, type v4) \
   { \
      __GLX_DECLARE_VARIABLES(); \
      __GLX_LOAD_VARIABLES(); \
      cmdlen = RENDER_SIZE(type, 4); \
      __GLX_BEGIN(rop, cmdlen); \
      __GLX_PUT_ ## type (4 + (0 * type ## _SIZE), v1); \
      __GLX_PUT_ ## type (4 + (1 * type ## _SIZE), v2); \
      __GLX_PUT_ ## type (4 + (2 * type ## _SIZE), v3); \
      __GLX_PUT_ ## type (4 + (3 * type ## _SIZE), v4); \
      __GLX_END(cmdlen); \
   }

#define glxproto_enum1_1s(name, rop, type) \
   void __indirect_gl ## name (GLenum e, type v1) \
   { \
      __GLX_DECLARE_VARIABLES(); \
      __GLX_LOAD_VARIABLES(); \
      cmdlen = 4 + RENDER_SIZE(type, 1); \
      __GLX_BEGIN(rop, cmdlen); \
      if (type ## _SIZE == 8) { \
	 __GLX_PUT_ ## type (4 + (0 * type ## _SIZE), v1); \
	 __GLX_PUT_LONG     (4 + (1 * type ## _SIZE), e); \
      } else { \
	 __GLX_PUT_LONG(4, e); \
	 __GLX_PUT_ ## type (8 + (0 * type ## _SIZE), v1); \
      } \
      __GLX_END(cmdlen); \
   }

#define glxproto_enum1_Vv(name, rop, type) \
   void __indirect_gl ## name (GLenum pname, const type * v) \
   { \
      __GLX_DECLARE_VARIABLES(); \
      __GLX_LOAD_VARIABLES(); \
      compsize = __gl ## name ## _size(pname); \
      cmdlen = 4 + RENDER_SIZE(type, compsize); \
      __GLX_BEGIN(rop, cmdlen); \
      __GLX_PUT_LONG(4, pname); \
      __GLX_PUT_ ## type ## _ARRAY(8, v, compsize); \
      __GLX_END(cmdlen); \
   }

#define glxproto_1(name, rop, type) \
   glxproto_1s(name,      rop, type) \
   glxproto_Cv(name ## v, rop, type, 1)

#define glxproto_3(name, rop, type) \
   glxproto_3s(name,      rop, type) \
   glxproto_Cv(name ## v, rop, type, 3)

#define glxproto_enum1_V(name, rop, type) \
   glxproto_enum1_1s(name,      rop,      type) \
   glxproto_enum1_Vv(name ## v, rop ## v, type)

void glCallList(GLuint list)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CallList,8);
	__GLX_PUT_LONG(4,list);
	__GLX_END(8);
}

void glListBase(GLuint base)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ListBase,8);
	__GLX_PUT_LONG(4,base);
	__GLX_END(8);
}

void glBegin(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Begin,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3bv,8);
	__GLX_PUT_CHAR(4,red);
	__GLX_PUT_CHAR(5,green);
	__GLX_PUT_CHAR(6,blue);
	__GLX_END(8);
}

void glColor3bv(const GLbyte *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3bv,8);
	__GLX_PUT_CHAR(4,v[0]);
	__GLX_PUT_CHAR(5,v[1]);
	__GLX_PUT_CHAR(6,v[2]);
	__GLX_END(8);
}

void glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3dv,28);
	__GLX_PUT_DOUBLE(4,red);
	__GLX_PUT_DOUBLE(12,green);
	__GLX_PUT_DOUBLE(20,blue);
	__GLX_END(28);
}

void glColor3dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3dv,28);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_END(28);
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3fv,16);
	__GLX_PUT_FLOAT(4,red);
	__GLX_PUT_FLOAT(8,green);
	__GLX_PUT_FLOAT(12,blue);
	__GLX_END(16);
}

void glColor3fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3fv,16);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_END(16);
}

void glColor3i(GLint red, GLint green, GLint blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3iv,16);
	__GLX_PUT_LONG(4,red);
	__GLX_PUT_LONG(8,green);
	__GLX_PUT_LONG(12,blue);
	__GLX_END(16);
}

void glColor3iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3iv,16);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_END(16);
}

void glColor3s(GLshort red, GLshort green, GLshort blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3sv,12);
	__GLX_PUT_SHORT(4,red);
	__GLX_PUT_SHORT(6,green);
	__GLX_PUT_SHORT(8,blue);
	__GLX_END(12);
}

void glColor3sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_END(12);
}

void glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3ubv,8);
	__GLX_PUT_CHAR(4,red);
	__GLX_PUT_CHAR(5,green);
	__GLX_PUT_CHAR(6,blue);
	__GLX_END(8);
}

void glColor3ubv(const GLubyte *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3ubv,8);
	__GLX_PUT_CHAR(4,v[0]);
	__GLX_PUT_CHAR(5,v[1]);
	__GLX_PUT_CHAR(6,v[2]);
	__GLX_END(8);
}

void glColor3ui(GLuint red, GLuint green, GLuint blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3uiv,16);
	__GLX_PUT_LONG(4,red);
	__GLX_PUT_LONG(8,green);
	__GLX_PUT_LONG(12,blue);
	__GLX_END(16);
}

void glColor3uiv(const GLuint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3uiv,16);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_END(16);
}

void glColor3us(GLushort red, GLushort green, GLushort blue)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3usv,12);
	__GLX_PUT_SHORT(4,red);
	__GLX_PUT_SHORT(6,green);
	__GLX_PUT_SHORT(8,blue);
	__GLX_END(12);
}

void glColor3usv(const GLushort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color3usv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_END(12);
}

void glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4bv,8);
	__GLX_PUT_CHAR(4,red);
	__GLX_PUT_CHAR(5,green);
	__GLX_PUT_CHAR(6,blue);
	__GLX_PUT_CHAR(7,alpha);
	__GLX_END(8);
}

void glColor4bv(const GLbyte *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4bv,8);
	__GLX_PUT_CHAR(4,v[0]);
	__GLX_PUT_CHAR(5,v[1]);
	__GLX_PUT_CHAR(6,v[2]);
	__GLX_PUT_CHAR(7,v[3]);
	__GLX_END(8);
}

void glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4dv,36);
	__GLX_PUT_DOUBLE(4,red);
	__GLX_PUT_DOUBLE(12,green);
	__GLX_PUT_DOUBLE(20,blue);
	__GLX_PUT_DOUBLE(28,alpha);
	__GLX_END(36);
}

void glColor4dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4dv,36);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_PUT_DOUBLE(28,v[3]);
	__GLX_END(36);
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4fv,20);
	__GLX_PUT_FLOAT(4,red);
	__GLX_PUT_FLOAT(8,green);
	__GLX_PUT_FLOAT(12,blue);
	__GLX_PUT_FLOAT(16,alpha);
	__GLX_END(20);
}

void glColor4fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4fv,20);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_PUT_FLOAT(16,v[3]);
	__GLX_END(20);
}

void glColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4iv,20);
	__GLX_PUT_LONG(4,red);
	__GLX_PUT_LONG(8,green);
	__GLX_PUT_LONG(12,blue);
	__GLX_PUT_LONG(16,alpha);
	__GLX_END(20);
}

void glColor4iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4iv,20);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_PUT_LONG(16,v[3]);
	__GLX_END(20);
}

void glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4sv,12);
	__GLX_PUT_SHORT(4,red);
	__GLX_PUT_SHORT(6,green);
	__GLX_PUT_SHORT(8,blue);
	__GLX_PUT_SHORT(10,alpha);
	__GLX_END(12);
}

void glColor4sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_PUT_SHORT(10,v[3]);
	__GLX_END(12);
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4ubv,8);
	__GLX_PUT_CHAR(4,red);
	__GLX_PUT_CHAR(5,green);
	__GLX_PUT_CHAR(6,blue);
	__GLX_PUT_CHAR(7,alpha);
	__GLX_END(8);
}

void glColor4ubv(const GLubyte *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4ubv,8);
	__GLX_PUT_CHAR(4,v[0]);
	__GLX_PUT_CHAR(5,v[1]);
	__GLX_PUT_CHAR(6,v[2]);
	__GLX_PUT_CHAR(7,v[3]);
	__GLX_END(8);
}

void glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4uiv,20);
	__GLX_PUT_LONG(4,red);
	__GLX_PUT_LONG(8,green);
	__GLX_PUT_LONG(12,blue);
	__GLX_PUT_LONG(16,alpha);
	__GLX_END(20);
}

void glColor4uiv(const GLuint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4uiv,20);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_PUT_LONG(16,v[3]);
	__GLX_END(20);
}

void glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4usv,12);
	__GLX_PUT_SHORT(4,red);
	__GLX_PUT_SHORT(6,green);
	__GLX_PUT_SHORT(8,blue);
	__GLX_PUT_SHORT(10,alpha);
	__GLX_END(12);
}

void glColor4usv(const GLushort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Color4usv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_PUT_SHORT(10,v[3]);
	__GLX_END(12);
}

glxproto_1(FogCoordf, X_GLrop_FogCoordfv, GLfloat)
glxproto_1(FogCoordd, X_GLrop_FogCoorddv, GLdouble)

glxproto_3(SecondaryColor3b,  X_GLrop_SecondaryColor3bv,  GLbyte)
glxproto_3(SecondaryColor3s,  X_GLrop_SecondaryColor3sv,  GLshort)
glxproto_3(SecondaryColor3i,  X_GLrop_SecondaryColor3iv,  GLint)
glxproto_3(SecondaryColor3ub, X_GLrop_SecondaryColor3ubv, GLubyte)
glxproto_3(SecondaryColor3us, X_GLrop_SecondaryColor3usv, GLushort)
glxproto_3(SecondaryColor3ui, X_GLrop_SecondaryColor3uiv, GLuint)
glxproto_3(SecondaryColor3f,  X_GLrop_SecondaryColor3fv,  GLfloat)
glxproto_3(SecondaryColor3d,  X_GLrop_SecondaryColor3dv,  GLdouble)

void glEdgeFlag(GLboolean flag)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EdgeFlagv,8);
	__GLX_PUT_CHAR(4,flag);
	__GLX_END(8);
}

void glEdgeFlagv(const GLboolean *flag)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EdgeFlagv,8);
	__GLX_PUT_CHAR(4,flag[0]);
	__GLX_END(8);
}

void glEnd(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_End,4);
	__GLX_END(4);
}

void glIndexd(GLdouble c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexdv,12);
	__GLX_PUT_DOUBLE(4,c);
	__GLX_END(12);
}

void glIndexdv(const GLdouble *c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexdv,12);
	__GLX_PUT_DOUBLE(4,c[0]);
	__GLX_END(12);
}

void glIndexf(GLfloat c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexfv,8);
	__GLX_PUT_FLOAT(4,c);
	__GLX_END(8);
}

void glIndexfv(const GLfloat *c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexfv,8);
	__GLX_PUT_FLOAT(4,c[0]);
	__GLX_END(8);
}

void glIndexi(GLint c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexiv,8);
	__GLX_PUT_LONG(4,c);
	__GLX_END(8);
}

void glIndexiv(const GLint *c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexiv,8);
	__GLX_PUT_LONG(4,c[0]);
	__GLX_END(8);
}

void glIndexs(GLshort c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexsv,8);
	__GLX_PUT_SHORT(4,c);
	__GLX_END(8);
}

void glIndexsv(const GLshort *c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexsv,8);
	__GLX_PUT_SHORT(4,c[0]);
	__GLX_END(8);
}

void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3bv,8);
	__GLX_PUT_CHAR(4,nx);
	__GLX_PUT_CHAR(5,ny);
	__GLX_PUT_CHAR(6,nz);
	__GLX_END(8);
}

void glNormal3bv(const GLbyte *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3bv,8);
	__GLX_PUT_CHAR(4,v[0]);
	__GLX_PUT_CHAR(5,v[1]);
	__GLX_PUT_CHAR(6,v[2]);
	__GLX_END(8);
}

void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3dv,28);
	__GLX_PUT_DOUBLE(4,nx);
	__GLX_PUT_DOUBLE(12,ny);
	__GLX_PUT_DOUBLE(20,nz);
	__GLX_END(28);
}

void glNormal3dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3dv,28);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_END(28);
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3fv,16);
	__GLX_PUT_FLOAT(4,nx);
	__GLX_PUT_FLOAT(8,ny);
	__GLX_PUT_FLOAT(12,nz);
	__GLX_END(16);
}

void glNormal3fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3fv,16);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_END(16);
}

void glNormal3i(GLint nx, GLint ny, GLint nz)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3iv,16);
	__GLX_PUT_LONG(4,nx);
	__GLX_PUT_LONG(8,ny);
	__GLX_PUT_LONG(12,nz);
	__GLX_END(16);
}

void glNormal3iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3iv,16);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_END(16);
}

void glNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3sv,12);
	__GLX_PUT_SHORT(4,nx);
	__GLX_PUT_SHORT(6,ny);
	__GLX_PUT_SHORT(8,nz);
	__GLX_END(12);
}

void glNormal3sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Normal3sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_END(12);
}

void glRasterPos2d(GLdouble x, GLdouble y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2dv,20);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_END(20);
}

void glRasterPos2dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2dv,20);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_END(20);
}

void glRasterPos2f(GLfloat x, GLfloat y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2fv,12);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_END(12);
}

void glRasterPos2fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2fv,12);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_END(12);
}

void glRasterPos2i(GLint x, GLint y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2iv,12);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_END(12);
}

void glRasterPos2iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2iv,12);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_END(12);
}

void glRasterPos2s(GLshort x, GLshort y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2sv,8);
	__GLX_PUT_SHORT(4,x);
	__GLX_PUT_SHORT(6,y);
	__GLX_END(8);
}

void glRasterPos2sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos2sv,8);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_END(8);
}

void glRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3dv,28);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_PUT_DOUBLE(20,z);
	__GLX_END(28);
}

void glRasterPos3dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3dv,28);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_END(28);
}

void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3fv,16);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_END(16);
}

void glRasterPos3fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3fv,16);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_END(16);
}

void glRasterPos3i(GLint x, GLint y, GLint z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3iv,16);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,z);
	__GLX_END(16);
}

void glRasterPos3iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3iv,16);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_END(16);
}

void glRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3sv,12);
	__GLX_PUT_SHORT(4,x);
	__GLX_PUT_SHORT(6,y);
	__GLX_PUT_SHORT(8,z);
	__GLX_END(12);
}

void glRasterPos3sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos3sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_END(12);
}

void glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4dv,36);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_PUT_DOUBLE(20,z);
	__GLX_PUT_DOUBLE(28,w);
	__GLX_END(36);
}

void glRasterPos4dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4dv,36);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_PUT_DOUBLE(28,v[3]);
	__GLX_END(36);
}

void glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4fv,20);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_PUT_FLOAT(16,w);
	__GLX_END(20);
}

void glRasterPos4fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4fv,20);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_PUT_FLOAT(16,v[3]);
	__GLX_END(20);
}

void glRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4iv,20);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,z);
	__GLX_PUT_LONG(16,w);
	__GLX_END(20);
}

void glRasterPos4iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4iv,20);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_PUT_LONG(16,v[3]);
	__GLX_END(20);
}

void glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4sv,12);
	__GLX_PUT_SHORT(4,x);
	__GLX_PUT_SHORT(6,y);
	__GLX_PUT_SHORT(8,z);
	__GLX_PUT_SHORT(10,w);
	__GLX_END(12);
}

void glRasterPos4sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_RasterPos4sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_PUT_SHORT(10,v[3]);
	__GLX_END(12);
}

void glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectdv,36);
	__GLX_PUT_DOUBLE(4,x1);
	__GLX_PUT_DOUBLE(12,y1);
	__GLX_PUT_DOUBLE(20,x2);
	__GLX_PUT_DOUBLE(28,y2);
	__GLX_END(36);
}

void glRectdv(const GLdouble *v1, const GLdouble *v2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectdv,36);
	__GLX_PUT_DOUBLE(4,v1[0]);
	__GLX_PUT_DOUBLE(12,v1[1]);
	__GLX_PUT_DOUBLE(20,v2[0]);
	__GLX_PUT_DOUBLE(28,v2[1]);
	__GLX_END(36);
}

void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectfv,20);
	__GLX_PUT_FLOAT(4,x1);
	__GLX_PUT_FLOAT(8,y1);
	__GLX_PUT_FLOAT(12,x2);
	__GLX_PUT_FLOAT(16,y2);
	__GLX_END(20);
}

void glRectfv(const GLfloat *v1, const GLfloat *v2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectfv,20);
	__GLX_PUT_FLOAT(4,v1[0]);
	__GLX_PUT_FLOAT(8,v1[1]);
	__GLX_PUT_FLOAT(12,v2[0]);
	__GLX_PUT_FLOAT(16,v2[1]);
	__GLX_END(20);
}

void glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectiv,20);
	__GLX_PUT_LONG(4,x1);
	__GLX_PUT_LONG(8,y1);
	__GLX_PUT_LONG(12,x2);
	__GLX_PUT_LONG(16,y2);
	__GLX_END(20);
}

void glRectiv(const GLint *v1, const GLint *v2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectiv,20);
	__GLX_PUT_LONG(4,v1[0]);
	__GLX_PUT_LONG(8,v1[1]);
	__GLX_PUT_LONG(12,v2[0]);
	__GLX_PUT_LONG(16,v2[1]);
	__GLX_END(20);
}

void glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectsv,12);
	__GLX_PUT_SHORT(4,x1);
	__GLX_PUT_SHORT(6,y1);
	__GLX_PUT_SHORT(8,x2);
	__GLX_PUT_SHORT(10,y2);
	__GLX_END(12);
}

void glRectsv(const GLshort *v1, const GLshort *v2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rectsv,12);
	__GLX_PUT_SHORT(4,v1[0]);
	__GLX_PUT_SHORT(6,v1[1]);
	__GLX_PUT_SHORT(8,v2[0]);
	__GLX_PUT_SHORT(10,v2[1]);
	__GLX_END(12);
}

void glTexCoord1d(GLdouble s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1dv,12);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_END(12);
}

void glTexCoord1dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1dv,12);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_END(12);
}

void glTexCoord1f(GLfloat s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1fv,8);
	__GLX_PUT_FLOAT(4,s);
	__GLX_END(8);
}

void glTexCoord1fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1fv,8);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_END(8);
}

void glTexCoord1i(GLint s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1iv,8);
	__GLX_PUT_LONG(4,s);
	__GLX_END(8);
}

void glTexCoord1iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1iv,8);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_END(8);
}

void glTexCoord1s(GLshort s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1sv,8);
	__GLX_PUT_SHORT(4,s);
	__GLX_END(8);
}

void glTexCoord1sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord1sv,8);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_END(8);
}

void glTexCoord2d(GLdouble s, GLdouble t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2dv,20);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_DOUBLE(12,t);
	__GLX_END(20);
}

void glTexCoord2dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2dv,20);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_END(20);
}

void glTexCoord2f(GLfloat s, GLfloat t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2fv,12);
	__GLX_PUT_FLOAT(4,s);
	__GLX_PUT_FLOAT(8,t);
	__GLX_END(12);
}

void glTexCoord2fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2fv,12);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_END(12);
}

void glTexCoord2i(GLint s, GLint t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2iv,12);
	__GLX_PUT_LONG(4,s);
	__GLX_PUT_LONG(8,t);
	__GLX_END(12);
}

void glTexCoord2iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2iv,12);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_END(12);
}

void glTexCoord2s(GLshort s, GLshort t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2sv,8);
	__GLX_PUT_SHORT(4,s);
	__GLX_PUT_SHORT(6,t);
	__GLX_END(8);
}

void glTexCoord2sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord2sv,8);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_END(8);
}

void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3dv,28);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_DOUBLE(12,t);
	__GLX_PUT_DOUBLE(20,r);
	__GLX_END(28);
}

void glTexCoord3dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3dv,28);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_END(28);
}

void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3fv,16);
	__GLX_PUT_FLOAT(4,s);
	__GLX_PUT_FLOAT(8,t);
	__GLX_PUT_FLOAT(12,r);
	__GLX_END(16);
}

void glTexCoord3fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3fv,16);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_END(16);
}

void glTexCoord3i(GLint s, GLint t, GLint r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3iv,16);
	__GLX_PUT_LONG(4,s);
	__GLX_PUT_LONG(8,t);
	__GLX_PUT_LONG(12,r);
	__GLX_END(16);
}

void glTexCoord3iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3iv,16);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_END(16);
}

void glTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3sv,12);
	__GLX_PUT_SHORT(4,s);
	__GLX_PUT_SHORT(6,t);
	__GLX_PUT_SHORT(8,r);
	__GLX_END(12);
}

void glTexCoord3sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord3sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_END(12);
}

void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4dv,36);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_DOUBLE(12,t);
	__GLX_PUT_DOUBLE(20,r);
	__GLX_PUT_DOUBLE(28,q);
	__GLX_END(36);
}

void glTexCoord4dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4dv,36);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_PUT_DOUBLE(28,v[3]);
	__GLX_END(36);
}

void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4fv,20);
	__GLX_PUT_FLOAT(4,s);
	__GLX_PUT_FLOAT(8,t);
	__GLX_PUT_FLOAT(12,r);
	__GLX_PUT_FLOAT(16,q);
	__GLX_END(20);
}

void glTexCoord4fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4fv,20);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_PUT_FLOAT(16,v[3]);
	__GLX_END(20);
}

void glTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4iv,20);
	__GLX_PUT_LONG(4,s);
	__GLX_PUT_LONG(8,t);
	__GLX_PUT_LONG(12,r);
	__GLX_PUT_LONG(16,q);
	__GLX_END(20);
}

void glTexCoord4iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4iv,20);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_PUT_LONG(16,v[3]);
	__GLX_END(20);
}

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4sv,12);
	__GLX_PUT_SHORT(4,s);
	__GLX_PUT_SHORT(6,t);
	__GLX_PUT_SHORT(8,r);
	__GLX_PUT_SHORT(10,q);
	__GLX_END(12);
}

void glTexCoord4sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexCoord4sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_PUT_SHORT(10,v[3]);
	__GLX_END(12);
}

void glVertex2d(GLdouble x, GLdouble y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2dv,20);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_END(20);
}

void glVertex2dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2dv,20);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_END(20);
}

void glVertex2f(GLfloat x, GLfloat y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2fv,12);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_END(12);
}

void glVertex2fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2fv,12);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_END(12);
}

void glVertex2i(GLint x, GLint y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2iv,12);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_END(12);
}

void glVertex2iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2iv,12);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_END(12);
}

void glVertex2s(GLshort x, GLshort y)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2sv,8);
	__GLX_PUT_SHORT(4,x);
	__GLX_PUT_SHORT(6,y);
	__GLX_END(8);
}

void glVertex2sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex2sv,8);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_END(8);
}

void glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3dv,28);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_PUT_DOUBLE(20,z);
	__GLX_END(28);
}

void glVertex3dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3dv,28);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_END(28);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3fv,16);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_END(16);
}

void glVertex3fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3fv,16);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_END(16);
}

void glVertex3i(GLint x, GLint y, GLint z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3iv,16);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,z);
	__GLX_END(16);
}

void glVertex3iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3iv,16);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_END(16);
}

void glVertex3s(GLshort x, GLshort y, GLshort z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3sv,12);
	__GLX_PUT_SHORT(4,x);
	__GLX_PUT_SHORT(6,y);
	__GLX_PUT_SHORT(8,z);
	__GLX_END(12);
}

void glVertex3sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex3sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_END(12);
}

void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4dv,36);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_PUT_DOUBLE(20,z);
	__GLX_PUT_DOUBLE(28,w);
	__GLX_END(36);
}

void glVertex4dv(const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4dv,36);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_PUT_DOUBLE(28,v[3]);
	__GLX_END(36);
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4fv,20);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_PUT_FLOAT(16,w);
	__GLX_END(20);
}

void glVertex4fv(const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4fv,20);
	__GLX_PUT_FLOAT(4,v[0]);
	__GLX_PUT_FLOAT(8,v[1]);
	__GLX_PUT_FLOAT(12,v[2]);
	__GLX_PUT_FLOAT(16,v[3]);
	__GLX_END(20);
}

void glVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4iv,20);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,z);
	__GLX_PUT_LONG(16,w);
	__GLX_END(20);
}

void glVertex4iv(const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4iv,20);
	__GLX_PUT_LONG(4,v[0]);
	__GLX_PUT_LONG(8,v[1]);
	__GLX_PUT_LONG(12,v[2]);
	__GLX_PUT_LONG(16,v[3]);
	__GLX_END(20);
}

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4sv,12);
	__GLX_PUT_SHORT(4,x);
	__GLX_PUT_SHORT(6,y);
	__GLX_PUT_SHORT(8,z);
	__GLX_PUT_SHORT(10,w);
	__GLX_END(12);
}

void glVertex4sv(const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Vertex4sv,12);
	__GLX_PUT_SHORT(4,v[0]);
	__GLX_PUT_SHORT(6,v[1]);
	__GLX_PUT_SHORT(8,v[2]);
	__GLX_PUT_SHORT(10,v[3]);
	__GLX_END(12);
}

void glClipPlane(GLenum plane, const GLdouble *equation)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ClipPlane,40);
	__GLX_PUT_DOUBLE(4,equation[0]);
	__GLX_PUT_DOUBLE(12,equation[1]);
	__GLX_PUT_DOUBLE(20,equation[2]);
	__GLX_PUT_DOUBLE(28,equation[3]);
	__GLX_PUT_LONG(36,plane);
	__GLX_END(40);
}

void glColorMaterial(GLenum face, GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ColorMaterial,12);
	__GLX_PUT_LONG(4,face);
	__GLX_PUT_LONG(8,mode);
	__GLX_END(12);
}

void glCullFace(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CullFace,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glFogf(GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Fogf,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_FLOAT(8,param);
	__GLX_END(12);
}

void glFogfv(GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glFogfv_size(pname);
	cmdlen = 8+compsize*4;
	__GLX_BEGIN(X_GLrop_Fogfv,cmdlen);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_FLOAT_ARRAY(8,params,compsize);
	__GLX_END(cmdlen);
}

void glFogi(GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Fogi,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_LONG(8,param);
	__GLX_END(12);
}

void glFogiv(GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glFogiv_size(pname);
	cmdlen = 8+compsize*4;
	__GLX_BEGIN(X_GLrop_Fogiv,cmdlen);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_LONG_ARRAY(8,params,compsize);
	__GLX_END(cmdlen);
}

void glFrontFace(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_FrontFace,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glHint(GLenum target, GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Hint,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,mode);
	__GLX_END(12);
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Lightf,16);
	__GLX_PUT_LONG(4,light);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT(12,param);
	__GLX_END(16);
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glLightfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_Lightfv,cmdlen);
	__GLX_PUT_LONG(4,light);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glLighti(GLenum light, GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Lighti,16);
	__GLX_PUT_LONG(4,light);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG(12,param);
	__GLX_END(16);
}

void glLightiv(GLenum light, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glLightiv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_Lightiv,cmdlen);
	__GLX_PUT_LONG(4,light);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glLightModelf(GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LightModelf,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_FLOAT(8,param);
	__GLX_END(12);
}

void glLightModelfv(GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glLightModelfv_size(pname);
	cmdlen = 8+compsize*4;
	__GLX_BEGIN(X_GLrop_LightModelfv,cmdlen);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_FLOAT_ARRAY(8,params,compsize);
	__GLX_END(cmdlen);
}

void glLightModeli(GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LightModeli,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_LONG(8,param);
	__GLX_END(12);
}

void glLightModeliv(GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glLightModeliv_size(pname);
	cmdlen = 8+compsize*4;
	__GLX_BEGIN(X_GLrop_LightModeliv,cmdlen);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_LONG_ARRAY(8,params,compsize);
	__GLX_END(cmdlen);
}

void glLineStipple(GLint factor, GLushort pattern)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LineStipple,12);
	__GLX_PUT_LONG(4,factor);
	__GLX_PUT_SHORT(8,pattern);
	__GLX_END(12);
}

void glLineWidth(GLfloat width)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LineWidth,8);
	__GLX_PUT_FLOAT(4,width);
	__GLX_END(8);
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Materialf,16);
	__GLX_PUT_LONG(4,face);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT(12,param);
	__GLX_END(16);
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glMaterialfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_Materialfv,cmdlen);
	__GLX_PUT_LONG(4,face);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glMateriali(GLenum face, GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Materiali,16);
	__GLX_PUT_LONG(4,face);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG(12,param);
	__GLX_END(16);
}

void glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glMaterialiv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_Materialiv,cmdlen);
	__GLX_PUT_LONG(4,face);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glPointSize(GLfloat size)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PointSize,8);
	__GLX_PUT_FLOAT(4,size);
	__GLX_END(8);
}

void glPolygonMode(GLenum face, GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PolygonMode,12);
	__GLX_PUT_LONG(4,face);
	__GLX_PUT_LONG(8,mode);
	__GLX_END(12);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Scissor,20);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,width);
	__GLX_PUT_LONG(16,height);
	__GLX_END(20);
}

void glShadeModel(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ShadeModel,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexParameterf,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT(12,param);
	__GLX_END(16);
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexParameterfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_TexParameterfv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexParameteri,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG(12,param);
	__GLX_END(16);
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexParameteriv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_TexParameteriv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexEnvf,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT(12,param);
	__GLX_END(16);
}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexEnvfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_TexEnvfv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexEnvi,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG(12,param);
	__GLX_END(16);
}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexEnviv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_TexEnviv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexGend,20);
	__GLX_PUT_DOUBLE(4,param);
	__GLX_PUT_LONG(12,coord);
	__GLX_PUT_LONG(16,pname);
	__GLX_END(20);
}

void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexGendv_size(pname);
	cmdlen = 12+compsize*8;
	__GLX_BEGIN(X_GLrop_TexGendv,cmdlen);
	__GLX_PUT_LONG(4,coord);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_DOUBLE_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexGenf,16);
	__GLX_PUT_LONG(4,coord);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT(12,param);
	__GLX_END(16);
}

void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexGenfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_TexGenfv,cmdlen);
	__GLX_PUT_LONG(4,coord);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_TexGeni,16);
	__GLX_PUT_LONG(4,coord);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG(12,param);
	__GLX_END(16);
}

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glTexGeniv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_TexGeniv,cmdlen);
	__GLX_PUT_LONG(4,coord);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glInitNames(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_InitNames,4);
	__GLX_END(4);
}

void glLoadName(GLuint name)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LoadName,8);
	__GLX_PUT_LONG(4,name);
	__GLX_END(8);
}

void glPassThrough(GLfloat token)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PassThrough,8);
	__GLX_PUT_FLOAT(4,token);
	__GLX_END(8);
}

void glPopName(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PopName,4);
	__GLX_END(4);
}

void glPushName(GLuint name)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PushName,8);
	__GLX_PUT_LONG(4,name);
	__GLX_END(8);
}

void glDrawBuffer(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_DrawBuffer,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glClear(GLbitfield mask)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Clear,8);
	__GLX_PUT_LONG(4,mask);
	__GLX_END(8);
}

void glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ClearAccum,20);
	__GLX_PUT_FLOAT(4,red);
	__GLX_PUT_FLOAT(8,green);
	__GLX_PUT_FLOAT(12,blue);
	__GLX_PUT_FLOAT(16,alpha);
	__GLX_END(20);
}

void glClearIndex(GLfloat c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ClearIndex,8);
	__GLX_PUT_FLOAT(4,c);
	__GLX_END(8);
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ClearColor,20);
	__GLX_PUT_FLOAT(4,red);
	__GLX_PUT_FLOAT(8,green);
	__GLX_PUT_FLOAT(12,blue);
	__GLX_PUT_FLOAT(16,alpha);
	__GLX_END(20);
}

void glClearStencil(GLint s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ClearStencil,8);
	__GLX_PUT_LONG(4,s);
	__GLX_END(8);
}

void glClearDepth(GLclampd depth)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ClearDepth,12);
	__GLX_PUT_DOUBLE(4,depth);
	__GLX_END(12);
}

void glStencilMask(GLuint mask)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_StencilMask,8);
	__GLX_PUT_LONG(4,mask);
	__GLX_END(8);
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ColorMask,8);
	__GLX_PUT_CHAR(4,red);
	__GLX_PUT_CHAR(5,green);
	__GLX_PUT_CHAR(6,blue);
	__GLX_PUT_CHAR(7,alpha);
	__GLX_END(8);
}

void glDepthMask(GLboolean flag)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_DepthMask,8);
	__GLX_PUT_CHAR(4,flag);
	__GLX_END(8);
}

void glIndexMask(GLuint mask)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_IndexMask,8);
	__GLX_PUT_LONG(4,mask);
	__GLX_END(8);
}

void glAccum(GLenum op, GLfloat value)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Accum,12);
	__GLX_PUT_LONG(4,op);
	__GLX_PUT_FLOAT(8,value);
	__GLX_END(12);
}

void glPopAttrib(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PopAttrib,4);
	__GLX_END(4);
}

void glPushAttrib(GLbitfield mask)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PushAttrib,8);
	__GLX_PUT_LONG(4,mask);
	__GLX_END(8);
}

void glMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MapGrid1d,24);
	__GLX_PUT_DOUBLE(4,u1);
	__GLX_PUT_DOUBLE(12,u2);
	__GLX_PUT_LONG(20,un);
	__GLX_END(24);
}

void glMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MapGrid1f,16);
	__GLX_PUT_LONG(4,un);
	__GLX_PUT_FLOAT(8,u1);
	__GLX_PUT_FLOAT(12,u2);
	__GLX_END(16);
}

void glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MapGrid2d,44);
	__GLX_PUT_DOUBLE(4,u1);
	__GLX_PUT_DOUBLE(12,u2);
	__GLX_PUT_DOUBLE(20,v1);
	__GLX_PUT_DOUBLE(28,v2);
	__GLX_PUT_LONG(36,un);
	__GLX_PUT_LONG(40,vn);
	__GLX_END(44);
}

void glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MapGrid2f,28);
	__GLX_PUT_LONG(4,un);
	__GLX_PUT_FLOAT(8,u1);
	__GLX_PUT_FLOAT(12,u2);
	__GLX_PUT_LONG(16,vn);
	__GLX_PUT_FLOAT(20,v1);
	__GLX_PUT_FLOAT(24,v2);
	__GLX_END(28);
}

void glEvalCoord1d(GLdouble u)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord1dv,12);
	__GLX_PUT_DOUBLE(4,u);
	__GLX_END(12);
}

void glEvalCoord1dv(const GLdouble *u)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord1dv,12);
	__GLX_PUT_DOUBLE(4,u[0]);
	__GLX_END(12);
}

void glEvalCoord1f(GLfloat u)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord1fv,8);
	__GLX_PUT_FLOAT(4,u);
	__GLX_END(8);
}

void glEvalCoord1fv(const GLfloat *u)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord1fv,8);
	__GLX_PUT_FLOAT(4,u[0]);
	__GLX_END(8);
}

void glEvalCoord2d(GLdouble u, GLdouble v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord2dv,20);
	__GLX_PUT_DOUBLE(4,u);
	__GLX_PUT_DOUBLE(12,v);
	__GLX_END(20);
}

void glEvalCoord2dv(const GLdouble *u)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord2dv,20);
	__GLX_PUT_DOUBLE(4,u[0]);
	__GLX_PUT_DOUBLE(12,u[1]);
	__GLX_END(20);
}

void glEvalCoord2f(GLfloat u, GLfloat v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord2fv,12);
	__GLX_PUT_FLOAT(4,u);
	__GLX_PUT_FLOAT(8,v);
	__GLX_END(12);
}

void glEvalCoord2fv(const GLfloat *u)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalCoord2fv,12);
	__GLX_PUT_FLOAT(4,u[0]);
	__GLX_PUT_FLOAT(8,u[1]);
	__GLX_END(12);
}

void glEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalMesh1,16);
	__GLX_PUT_LONG(4,mode);
	__GLX_PUT_LONG(8,i1);
	__GLX_PUT_LONG(12,i2);
	__GLX_END(16);
}

void glEvalPoint1(GLint i)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalPoint1,8);
	__GLX_PUT_LONG(4,i);
	__GLX_END(8);
}

void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalMesh2,24);
	__GLX_PUT_LONG(4,mode);
	__GLX_PUT_LONG(8,i1);
	__GLX_PUT_LONG(12,i2);
	__GLX_PUT_LONG(16,j1);
	__GLX_PUT_LONG(20,j2);
	__GLX_END(24);
}

void glEvalPoint2(GLint i, GLint j)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_EvalPoint2,12);
	__GLX_PUT_LONG(4,i);
	__GLX_PUT_LONG(8,j);
	__GLX_END(12);
}

void glAlphaFunc(GLenum func, GLclampf ref)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_AlphaFunc,12);
	__GLX_PUT_LONG(4,func);
	__GLX_PUT_FLOAT(8,ref);
	__GLX_END(12);
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_BlendFunc,12);
	__GLX_PUT_LONG(4,sfactor);
	__GLX_PUT_LONG(8,dfactor);
	__GLX_END(12);
}

glxproto_4s(BlendFuncSeparate, X_GLrop_BlendFuncSeparate, GLenum)

void glLogicOp(GLenum opcode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LogicOp,8);
	__GLX_PUT_LONG(4,opcode);
	__GLX_END(8);
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_StencilFunc,16);
	__GLX_PUT_LONG(4,func);
	__GLX_PUT_LONG(8,ref);
	__GLX_PUT_LONG(12,mask);
	__GLX_END(16);
}

void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_StencilOp,16);
	__GLX_PUT_LONG(4,fail);
	__GLX_PUT_LONG(8,zfail);
	__GLX_PUT_LONG(12,zpass);
	__GLX_END(16);
}

void glDepthFunc(GLenum func)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_DepthFunc,8);
	__GLX_PUT_LONG(4,func);
	__GLX_END(8);
}

void glPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PixelZoom,12);
	__GLX_PUT_FLOAT(4,xfactor);
	__GLX_PUT_FLOAT(8,yfactor);
	__GLX_END(12);
}

void glPixelTransferf(GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PixelTransferf,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_FLOAT(8,param);
	__GLX_END(12);
}

void glPixelTransferi(GLenum pname, GLint param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PixelTransferi,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_LONG(8,param);
	__GLX_END(12);
}

void glReadBuffer(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ReadBuffer,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyPixels,24);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,width);
	__GLX_PUT_LONG(16,height);
	__GLX_PUT_LONG(20,type);
	__GLX_END(24);
}

void glDepthRange(GLclampd zNear, GLclampd zFar)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_DepthRange,20);
	__GLX_PUT_DOUBLE(4,zNear);
	__GLX_PUT_DOUBLE(12,zFar);
	__GLX_END(20);
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Frustum,52);
	__GLX_PUT_DOUBLE(4,left);
	__GLX_PUT_DOUBLE(12,right);
	__GLX_PUT_DOUBLE(20,bottom);
	__GLX_PUT_DOUBLE(28,top);
	__GLX_PUT_DOUBLE(36,zNear);
	__GLX_PUT_DOUBLE(44,zFar);
	__GLX_END(52);
}

void glLoadIdentity(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LoadIdentity,4);
	__GLX_END(4);
}

void glLoadMatrixf(const GLfloat *m)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LoadMatrixf,68);
	__GLX_PUT_FLOAT_ARRAY(4,m,16);
	__GLX_END(68);
}

void glLoadMatrixd(const GLdouble *m)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_LoadMatrixd,132);
	__GLX_PUT_DOUBLE_ARRAY(4,m,16);
	__GLX_END(132);
}

void glMatrixMode(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MatrixMode,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glMultMatrixf(const GLfloat *m)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultMatrixf,68);
	__GLX_PUT_FLOAT_ARRAY(4,m,16);
	__GLX_END(68);
}

void glMultMatrixd(const GLdouble *m)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultMatrixd,132);
	__GLX_PUT_DOUBLE_ARRAY(4,m,16);
	__GLX_END(132);
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Ortho,52);
	__GLX_PUT_DOUBLE(4,left);
	__GLX_PUT_DOUBLE(12,right);
	__GLX_PUT_DOUBLE(20,bottom);
	__GLX_PUT_DOUBLE(28,top);
	__GLX_PUT_DOUBLE(36,zNear);
	__GLX_PUT_DOUBLE(44,zFar);
	__GLX_END(52);
}

void glPopMatrix(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PopMatrix,4);
	__GLX_END(4);
}

void glPushMatrix(void)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PushMatrix,4);
	__GLX_END(4);
}

void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rotated,36);
	__GLX_PUT_DOUBLE(4,angle);
	__GLX_PUT_DOUBLE(12,x);
	__GLX_PUT_DOUBLE(20,y);
	__GLX_PUT_DOUBLE(28,z);
	__GLX_END(36);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Rotatef,20);
	__GLX_PUT_FLOAT(4,angle);
	__GLX_PUT_FLOAT(8,x);
	__GLX_PUT_FLOAT(12,y);
	__GLX_PUT_FLOAT(16,z);
	__GLX_END(20);
}

void glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Scaled,28);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_PUT_DOUBLE(20,z);
	__GLX_END(28);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Scalef,16);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_END(16);
}

void glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Translated,28);
	__GLX_PUT_DOUBLE(4,x);
	__GLX_PUT_DOUBLE(12,y);
	__GLX_PUT_DOUBLE(20,z);
	__GLX_END(28);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Translatef,16);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_END(16);
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Viewport,20);
	__GLX_PUT_LONG(4,x);
	__GLX_PUT_LONG(8,y);
	__GLX_PUT_LONG(12,width);
	__GLX_PUT_LONG(16,height);
	__GLX_END(20);
}

void glPolygonOffset(GLfloat factor, GLfloat units)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PolygonOffset,12);
	__GLX_PUT_FLOAT(4,factor);
	__GLX_PUT_FLOAT(8,units);
	__GLX_END(12);
}

void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyTexImage1D,32);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,level);
	__GLX_PUT_LONG(12,internalformat);
	__GLX_PUT_LONG(16,x);
	__GLX_PUT_LONG(20,y);
	__GLX_PUT_LONG(24,width);
	__GLX_PUT_LONG(28,border);
	__GLX_END(32);
}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyTexImage2D,36);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,level);
	__GLX_PUT_LONG(12,internalformat);
	__GLX_PUT_LONG(16,x);
	__GLX_PUT_LONG(20,y);
	__GLX_PUT_LONG(24,width);
	__GLX_PUT_LONG(28,height);
	__GLX_PUT_LONG(32,border);
	__GLX_END(36);
}

void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyTexSubImage1D,28);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,level);
	__GLX_PUT_LONG(12,xoffset);
	__GLX_PUT_LONG(16,x);
	__GLX_PUT_LONG(20,y);
	__GLX_PUT_LONG(24,width);
	__GLX_END(28);
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyTexSubImage2D,36);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,level);
	__GLX_PUT_LONG(12,xoffset);
	__GLX_PUT_LONG(16,yoffset);
	__GLX_PUT_LONG(20,x);
	__GLX_PUT_LONG(24,y);
	__GLX_PUT_LONG(28,width);
	__GLX_PUT_LONG(32,height);
	__GLX_END(36);
}

void glBindTexture(GLenum target, GLuint texture)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_BindTexture,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,texture);
	__GLX_END(12);
}

void glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	if (n < 0) return;
	cmdlen = 8+n*4+n*4;
	__GLX_BEGIN(X_GLrop_PrioritizeTextures,cmdlen);
	__GLX_PUT_LONG(4,n);
	__GLX_PUT_LONG_ARRAY(8,textures,n);
	__GLX_PUT_FLOAT_ARRAY(8+n*4,priorities,n);
	__GLX_END(cmdlen);
}

void glIndexub(GLubyte c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexubv,8);
	__GLX_PUT_CHAR(4,c);
	__GLX_END(8);
}

void glIndexubv(const GLubyte *c)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Indexubv,8);
	__GLX_PUT_CHAR(4,c[0]);
	__GLX_END(8);
}

void glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_BlendColor,20);
	__GLX_PUT_FLOAT(4,red);
	__GLX_PUT_FLOAT(8,green);
	__GLX_PUT_FLOAT(12,blue);
	__GLX_PUT_FLOAT(16,alpha);
	__GLX_END(20);
}

void glBlendEquation(GLenum mode)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_BlendEquation,8);
	__GLX_PUT_LONG(4,mode);
	__GLX_END(8);
}

void glColorTableParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glColorTableParameterfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_ColorTableParameterfv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glColorTableParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glColorTableParameteriv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_ColorTableParameteriv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glCopyColorTable(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyColorTable,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,internalformat);
	__GLX_PUT_LONG(12,x);
	__GLX_PUT_LONG(16,y);
	__GLX_PUT_LONG(20,width);
	__GLX_END(24);
}

void glCopyColorSubTable(GLenum target, GLsizei start, GLint x, GLint y, GLsizei width)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyColorSubTable,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,start);
	__GLX_PUT_LONG(12,x);
	__GLX_PUT_LONG(16,y);
	__GLX_PUT_LONG(20,width);
	__GLX_END(24);
}

void glConvolutionParameterf(GLenum target, GLenum pname, GLfloat params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ConvolutionParameterf,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT(12,params);
	__GLX_END(16);
}

void glConvolutionParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glConvolutionParameterfv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_ConvolutionParameterfv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_FLOAT_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glConvolutionParameteri(GLenum target, GLenum pname, GLint params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ConvolutionParameteri,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG(12,params);
	__GLX_END(16);
}

void glConvolutionParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	compsize = __glConvolutionParameteriv_size(pname);
	cmdlen = 12+compsize*4;
	__GLX_BEGIN(X_GLrop_ConvolutionParameteriv,cmdlen);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,pname);
	__GLX_PUT_LONG_ARRAY(12,params,compsize);
	__GLX_END(cmdlen);
}

void glCopyConvolutionFilter1D(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyConvolutionFilter1D,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,internalformat);
	__GLX_PUT_LONG(12,x);
	__GLX_PUT_LONG(16,y);
	__GLX_PUT_LONG(20,width);
	__GLX_END(24);
}

void glCopyConvolutionFilter2D(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyConvolutionFilter2D,28);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,internalformat);
	__GLX_PUT_LONG(12,x);
	__GLX_PUT_LONG(16,y);
	__GLX_PUT_LONG(20,width);
	__GLX_PUT_LONG(24,height);
	__GLX_END(28);
}

void glHistogram(GLenum target, GLsizei width, GLenum internalformat, GLboolean sink)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Histogram,20);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,width);
	__GLX_PUT_LONG(12,internalformat);
	__GLX_PUT_CHAR(16,sink);
	__GLX_END(20);
}

void glMinmax(GLenum target, GLenum internalformat, GLboolean sink)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_Minmax,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,internalformat);
	__GLX_PUT_CHAR(12,sink);
	__GLX_END(16);
}

void glResetHistogram(GLenum target)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ResetHistogram,8);
	__GLX_PUT_LONG(4,target);
	__GLX_END(8);
}

void glResetMinmax(GLenum target)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ResetMinmax,8);
	__GLX_PUT_LONG(4,target);
	__GLX_END(8);
}

void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_CopyTexSubImage3D,40);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,level);
	__GLX_PUT_LONG(12,xoffset);
	__GLX_PUT_LONG(16,yoffset);
	__GLX_PUT_LONG(20,zoffset);
	__GLX_PUT_LONG(24,x);
	__GLX_PUT_LONG(28,y);
	__GLX_PUT_LONG(32,width);
	__GLX_PUT_LONG(36,height);
	__GLX_END(40);
}

void glActiveTextureARB(GLenum texture)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ActiveTextureARB,8);
	__GLX_PUT_LONG(4,texture);
	__GLX_END(8);
}

void glMultiTexCoord1dARB(GLenum target, GLdouble s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1dvARB,16);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_LONG(12,target);
	__GLX_END(16);
}

void glMultiTexCoord1dvARB(GLenum target, const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1dvARB,16);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_LONG(12,target);
	__GLX_END(16);
}

void glMultiTexCoord1fARB(GLenum target, GLfloat s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1fvARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,s);
	__GLX_END(12);
}

void glMultiTexCoord1fvARB(GLenum target, const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1fvARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,v[0]);
	__GLX_END(12);
}

void glMultiTexCoord1iARB(GLenum target, GLint s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1ivARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,s);
	__GLX_END(12);
}

void glMultiTexCoord1ivARB(GLenum target, const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1ivARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,v[0]);
	__GLX_END(12);
}

void glMultiTexCoord1sARB(GLenum target, GLshort s)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1svARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,s);
	__GLX_END(12);
}

void glMultiTexCoord1svARB(GLenum target, const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord1svARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,v[0]);
	__GLX_END(12);
}

void glMultiTexCoord2dARB(GLenum target, GLdouble s, GLdouble t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2dvARB,24);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_DOUBLE(12,t);
	__GLX_PUT_LONG(20,target);
	__GLX_END(24);
}

void glMultiTexCoord2dvARB(GLenum target, const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2dvARB,24);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_LONG(20,target);
	__GLX_END(24);
}

void glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2fvARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,s);
	__GLX_PUT_FLOAT(12,t);
	__GLX_END(16);
}

void glMultiTexCoord2fvARB(GLenum target, const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2fvARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,v[0]);
	__GLX_PUT_FLOAT(12,v[1]);
	__GLX_END(16);
}

void glMultiTexCoord2iARB(GLenum target, GLint s, GLint t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2ivARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,s);
	__GLX_PUT_LONG(12,t);
	__GLX_END(16);
}

void glMultiTexCoord2ivARB(GLenum target, const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2ivARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,v[0]);
	__GLX_PUT_LONG(12,v[1]);
	__GLX_END(16);
}

void glMultiTexCoord2sARB(GLenum target, GLshort s, GLshort t)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2svARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,s);
	__GLX_PUT_SHORT(10,t);
	__GLX_END(12);
}

void glMultiTexCoord2svARB(GLenum target, const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord2svARB,12);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,v[0]);
	__GLX_PUT_SHORT(10,v[1]);
	__GLX_END(12);
}

void glMultiTexCoord3dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3dvARB,32);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_DOUBLE(12,t);
	__GLX_PUT_DOUBLE(20,r);
	__GLX_PUT_LONG(28,target);
	__GLX_END(32);
}

void glMultiTexCoord3dvARB(GLenum target, const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3dvARB,32);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_PUT_LONG(28,target);
	__GLX_END(32);
}

void glMultiTexCoord3fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3fvARB,20);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,s);
	__GLX_PUT_FLOAT(12,t);
	__GLX_PUT_FLOAT(16,r);
	__GLX_END(20);
}

void glMultiTexCoord3fvARB(GLenum target, const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3fvARB,20);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,v[0]);
	__GLX_PUT_FLOAT(12,v[1]);
	__GLX_PUT_FLOAT(16,v[2]);
	__GLX_END(20);
}

void glMultiTexCoord3iARB(GLenum target, GLint s, GLint t, GLint r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3ivARB,20);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,s);
	__GLX_PUT_LONG(12,t);
	__GLX_PUT_LONG(16,r);
	__GLX_END(20);
}

void glMultiTexCoord3ivARB(GLenum target, const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3ivARB,20);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,v[0]);
	__GLX_PUT_LONG(12,v[1]);
	__GLX_PUT_LONG(16,v[2]);
	__GLX_END(20);
}

void glMultiTexCoord3sARB(GLenum target, GLshort s, GLshort t, GLshort r)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3svARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,s);
	__GLX_PUT_SHORT(10,t);
	__GLX_PUT_SHORT(12,r);
	__GLX_END(16);
}

void glMultiTexCoord3svARB(GLenum target, const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord3svARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,v[0]);
	__GLX_PUT_SHORT(10,v[1]);
	__GLX_PUT_SHORT(12,v[2]);
	__GLX_END(16);
}

void glMultiTexCoord4dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4dvARB,40);
	__GLX_PUT_DOUBLE(4,s);
	__GLX_PUT_DOUBLE(12,t);
	__GLX_PUT_DOUBLE(20,r);
	__GLX_PUT_DOUBLE(28,q);
	__GLX_PUT_LONG(36,target);
	__GLX_END(40);
}

void glMultiTexCoord4dvARB(GLenum target, const GLdouble *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4dvARB,40);
	__GLX_PUT_DOUBLE(4,v[0]);
	__GLX_PUT_DOUBLE(12,v[1]);
	__GLX_PUT_DOUBLE(20,v[2]);
	__GLX_PUT_DOUBLE(28,v[3]);
	__GLX_PUT_LONG(36,target);
	__GLX_END(40);
}

void glMultiTexCoord4fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4fvARB,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,s);
	__GLX_PUT_FLOAT(12,t);
	__GLX_PUT_FLOAT(16,r);
	__GLX_PUT_FLOAT(20,q);
	__GLX_END(24);
}

void glMultiTexCoord4fvARB(GLenum target, const GLfloat *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4fvARB,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_FLOAT(8,v[0]);
	__GLX_PUT_FLOAT(12,v[1]);
	__GLX_PUT_FLOAT(16,v[2]);
	__GLX_PUT_FLOAT(20,v[3]);
	__GLX_END(24);
}

void glMultiTexCoord4iARB(GLenum target, GLint s, GLint t, GLint r, GLint q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4ivARB,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,s);
	__GLX_PUT_LONG(12,t);
	__GLX_PUT_LONG(16,r);
	__GLX_PUT_LONG(20,q);
	__GLX_END(24);
}

void glMultiTexCoord4ivARB(GLenum target, const GLint *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4ivARB,24);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_LONG(8,v[0]);
	__GLX_PUT_LONG(12,v[1]);
	__GLX_PUT_LONG(16,v[2]);
	__GLX_PUT_LONG(20,v[3]);
	__GLX_END(24);
}

void glMultiTexCoord4sARB(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4svARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,s);
	__GLX_PUT_SHORT(10,t);
	__GLX_PUT_SHORT(12,r);
	__GLX_PUT_SHORT(14,q);
	__GLX_END(16);
}

void glMultiTexCoord4svARB(GLenum target, const GLshort *v)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_MultiTexCoord4svARB,16);
	__GLX_PUT_LONG(4,target);
	__GLX_PUT_SHORT(8,v[0]);
	__GLX_PUT_SHORT(10,v[1]);
	__GLX_PUT_SHORT(12,v[2]);
	__GLX_PUT_SHORT(14,v[3]);
	__GLX_END(16);
}

void glLoadTransposeMatrixfARB(const GLfloat *m)
{
	__GLX_DECLARE_VARIABLES();
        GLfloat t[16];
        int i, j;
	__GLX_LOAD_VARIABLES();
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                t[i*4+j] = m[j*4+i];
             }
         }
	__GLX_BEGIN(X_GLrop_LoadMatrixf,68);
	__GLX_PUT_FLOAT_ARRAY(4,t,16);
	__GLX_END(68);
}

void glMultTransposeMatrixfARB(const GLfloat *m)
{
	__GLX_DECLARE_VARIABLES();
        GLfloat t[16];
        int i, j;
	__GLX_LOAD_VARIABLES();
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                t[i*4+j] = m[j*4+i];
             }
         }
	__GLX_BEGIN(X_GLrop_MultMatrixf,68);
	__GLX_PUT_FLOAT_ARRAY(4,t,16);
	__GLX_END(68);
}

void glLoadTransposeMatrixdARB(const GLdouble *m)
{
	__GLX_DECLARE_VARIABLES();
        GLdouble t[16];
        int i, j;
	__GLX_LOAD_VARIABLES();
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                t[i*4+j] = m[j*4+i];
             }
         }
	__GLX_BEGIN(X_GLrop_LoadMatrixd,132);
	__GLX_PUT_DOUBLE_ARRAY(4,t,16);
	__GLX_END(132);
}

void glMultTransposeMatrixdARB(const GLdouble *m)
{
	__GLX_DECLARE_VARIABLES();
        GLdouble t[16];
        int i, j;
	__GLX_LOAD_VARIABLES();
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                t[i*4+j] = m[j*4+i];
             }
         }
	__GLX_BEGIN(X_GLrop_MultMatrixd,132);
	__GLX_PUT_DOUBLE_ARRAY(4,t,16);
	__GLX_END(132);
}


/*
 * New extension functions
 */

void glPointParameterfARB(GLenum pname, GLfloat param)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_PointParameterfARB,12);
	__GLX_PUT_LONG(4,pname);
	__GLX_PUT_FLOAT(8,param);
	__GLX_END(12);
}

void glPointParameterfvARB(GLenum pname, const GLfloat *params)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	if (pname == GL_POINT_DISTANCE_ATTENUATION_ARB) {
		/* params is float[3] */
		__GLX_BEGIN(X_GLrop_PointParameterfvARB,20);
		__GLX_PUT_LONG(4,pname);
		__GLX_PUT_FLOAT(8,params[0]);
		__GLX_PUT_FLOAT(12,params[1]);
		__GLX_PUT_FLOAT(16,params[2]);
		__GLX_END(20);
	}
	else {
		/* params is float[1] */
		__GLX_BEGIN(X_GLrop_PointParameterfvARB,12);
		__GLX_PUT_LONG(4,pname);
		__GLX_PUT_FLOAT(8,params[0]);
		__GLX_END(12);
	}
}

glxproto_enum1_V(PointParameteri, X_GLrop_PointParameteri, GLint)

void glWindowPos2dARB(GLdouble x, GLdouble y)
{
	glWindowPos3fARB(x, y, 0.0);
}

void glWindowPos2iARB(GLint x, GLint y)
{
	glWindowPos3fARB(x, y, 0.0);
}

void glWindowPos2fARB(GLfloat x, GLfloat y)
{
	glWindowPos3fARB(x, y, 0.0);
}

void glWindowPos2sARB(GLshort x, GLshort y)
{
	glWindowPos3fARB(x, y, 0.0);
}

void glWindowPos2dvARB(const GLdouble * p)
{
	glWindowPos3fARB(p[0], p[1], 0.0);
}

void glWindowPos2fvARB(const GLfloat * p)
{
	glWindowPos3fARB(p[0], p[1], 0.0);
}

void glWindowPos2ivARB(const GLint * p)
{
	glWindowPos3fARB(p[0], p[1], 0.0);
}

void glWindowPos2svARB(const GLshort * p)
{
	glWindowPos3fARB(p[0], p[1], 0.0);
}

void glWindowPos3dARB(GLdouble x, GLdouble y, GLdouble z)
{
	glWindowPos3fARB(x, y, z);
}

void glWindowPos3fARB(GLfloat x, GLfloat y, GLfloat z)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_WindowPos3fARB,16);
	__GLX_PUT_FLOAT(4,x);
	__GLX_PUT_FLOAT(8,y);
	__GLX_PUT_FLOAT(12,z);
	__GLX_END(16);
}

void glWindowPos3iARB(GLint x, GLint y, GLint z)
{
	glWindowPos3fARB(x, y, z);
}

void glWindowPos3sARB(GLshort x, GLshort y, GLshort z)
{
	glWindowPos3fARB(x, y, z);
}

void glWindowPos3dvARB(const GLdouble * p)
{
	glWindowPos3fARB(p[0], p[1], p[2]);
}

void glWindowPos3fvARB(const GLfloat * p)
{
	glWindowPos3fARB(p[0], p[1], p[2]);
}

void glWindowPos3ivARB(const GLint * p)
{
	glWindowPos3fARB(p[0], p[1], p[2]);
}

void glWindowPos3svARB(const GLshort * p)
{
	glWindowPos3fARB(p[0], p[1], p[2]);
}

void glActiveStencilFaceEXT(GLenum face)
{
	__GLX_DECLARE_VARIABLES();
	__GLX_LOAD_VARIABLES();
	__GLX_BEGIN(X_GLrop_ActiveStencilFaceEXT,8);
	__GLX_PUT_LONG(4,face);
	__GLX_END(8);
}
