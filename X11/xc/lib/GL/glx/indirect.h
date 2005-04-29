/* $XFree86: xc/lib/GL/glx/indirect.h,v 1.6 2004/01/28 18:11:41 alanh Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
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

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *
 */

#ifndef _INDIRECT_H_
#define _INDIRECT_H_

#include "indirect_wrap.h"

/* NOTE: This file could be automatically generated */

void __indirect_glAccum(GLenum op, GLfloat value);
void __indirect_glAlphaFunc(GLenum func, GLclampf ref);
GLboolean __indirect_glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences);
GLboolean __indirect_glAreTexturesResidentEXT(GLsizei n, const GLuint *textures, GLboolean *residences);
void __indirect_glArrayElement(GLint i);
void __indirect_glBegin(GLenum mode);
void __indirect_glBindTexture(GLenum target, GLuint texture);
void __indirect_glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
void __indirect_glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void __indirect_glBlendEquation(GLenum mode);
void __indirect_glBlendFunc(GLenum sfactor, GLenum dfactor);
void __indirect_glCallList(GLuint list);
void __indirect_glCallLists(GLsizei n, GLenum type, const GLvoid *lists);
void __indirect_glClear(GLbitfield mask);
void __indirect_glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void __indirect_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void __indirect_glClearDepth(GLclampd depth);
void __indirect_glClearIndex(GLfloat c);
void __indirect_glClearStencil(GLint s);
void __indirect_glClipPlane(GLenum plane, const GLdouble *equation);
void __indirect_glColor3b(GLbyte red, GLbyte green, GLbyte blue);
void __indirect_glColor3bv(const GLbyte *v);
void __indirect_glColor3d(GLdouble red, GLdouble green, GLdouble blue);
void __indirect_glColor3dv(const GLdouble *v);
void __indirect_glColor3f(GLfloat red, GLfloat green, GLfloat blue);
void __indirect_glColor3fv(const GLfloat *v);
void __indirect_glColor3i(GLint red, GLint green, GLint blue);
void __indirect_glColor3iv(const GLint *v);
void __indirect_glColor3s(GLshort red, GLshort green, GLshort blue);
void __indirect_glColor3sv(const GLshort *v);
void __indirect_glColor3ub(GLubyte red, GLubyte green, GLubyte blue);
void __indirect_glColor3ubv(const GLubyte *v);
void __indirect_glColor3ui(GLuint red, GLuint green, GLuint blue);
void __indirect_glColor3uiv(const GLuint *v);
void __indirect_glColor3us(GLushort red, GLushort green, GLushort blue);
void __indirect_glColor3usv(const GLushort *v);
void __indirect_glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
void __indirect_glColor4bv(const GLbyte *v);
void __indirect_glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
void __indirect_glColor4dv(const GLdouble *v);
void __indirect_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void __indirect_glColor4fv(const GLfloat *v);
void __indirect_glColor4i(GLint red, GLint green, GLint blue, GLint alpha);
void __indirect_glColor4iv(const GLint *v);
void __indirect_glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha);
void __indirect_glColor4sv(const GLshort *v);
void __indirect_glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void __indirect_glColor4ubv(const GLubyte *v);
void __indirect_glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha);
void __indirect_glColor4uiv(const GLuint *v);
void __indirect_glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha);
void __indirect_glColor4usv(const GLushort *v);
void __indirect_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void __indirect_glColorMaterial(GLenum face, GLenum mode);
void __indirect_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void __indirect_glColorSubTable(GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *table);
void __indirect_glColorTable(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
void __indirect_glColorTableParameterfv(GLenum target, GLenum pname, const GLfloat *params);
void __indirect_glColorTableParameteriv(GLenum target, GLenum pname, const GLint *params);
void __indirect_glConvolutionFilter1D(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glConvolutionFilter2D(GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glConvolutionParameterf(GLenum target, GLenum pname, GLfloat params);
void __indirect_glConvolutionParameterfv(GLenum target, GLenum pname, const GLfloat *params);
void __indirect_glConvolutionParameteri(GLenum target, GLenum pname, GLint params);
void __indirect_glConvolutionParameteriv(GLenum target, GLenum pname, const GLint *params);
void __indirect_glCopyConvolutionFilter1D(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
void __indirect_glCopyConvolutionFilter2D(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
void __indirect_glCopyColorTable(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
void __indirect_glCopyColorSubTable(GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);
void __indirect_glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void __indirect_glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
void __indirect_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void __indirect_glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void __indirect_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void __indirect_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void __indirect_glCullFace(GLenum mode);
void __indirect_glDeleteLists(GLuint list, GLsizei range);
void __indirect_glDeleteTextures(GLsizei n, const GLuint *textures);
void __indirect_glDeleteTexturesEXT(GLsizei n, const GLuint *textures);
void __indirect_glDepthFunc(GLenum func);
void __indirect_glDepthMask(GLboolean flag);
void __indirect_glDepthRange(GLclampd zNear, GLclampd zFar);
void __indirect_glDisable(GLenum cap);
void __indirect_glDisableClientState(GLenum array);
void __indirect_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void __indirect_glDrawBuffer(GLenum mode);
void __indirect_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void __indirect_glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
void __indirect_glEdgeFlag(GLboolean flag);
void __indirect_glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer);
void __indirect_glEdgeFlagv(const GLboolean *flag);
void __indirect_glEnable(GLenum cap);
void __indirect_glEnableClientState(GLenum array);
void __indirect_glEnd(void);
void __indirect_glEndList(void);
void __indirect_glEvalCoord1d(GLdouble u);
void __indirect_glEvalCoord1dv(const GLdouble *u);
void __indirect_glEvalCoord1f(GLfloat u);
void __indirect_glEvalCoord1fv(const GLfloat *u);
void __indirect_glEvalCoord2d(GLdouble u, GLdouble v);
void __indirect_glEvalCoord2dv(const GLdouble *u);
void __indirect_glEvalCoord2f(GLfloat u, GLfloat v);
void __indirect_glEvalCoord2fv(const GLfloat *u);
void __indirect_glEvalMesh1(GLenum mode, GLint i1, GLint i2);
void __indirect_glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void __indirect_glEvalPoint1(GLint i);
void __indirect_glEvalPoint2(GLint i, GLint j);
void __indirect_glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer);
void __indirect_glFinish(void);
void __indirect_glFlush(void);
void __indirect_glFogf(GLenum pname, GLfloat param);
void __indirect_glFogfv(GLenum pname, const GLfloat *params);
void __indirect_glFogi(GLenum pname, GLint param);
void __indirect_glFogiv(GLenum pname, const GLint *params);
void __indirect_glFrontFace(GLenum mode);
void __indirect_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint __indirect_glGenLists(GLsizei range);
void __indirect_glGenTextures(GLsizei n, GLuint *textures);
void __indirect_glGenTexturesEXT(GLsizei n, GLuint *textures);
void __indirect_glGetBooleanv(GLenum val, GLboolean *b);
void __indirect_glGetClipPlane(GLenum plane, GLdouble *equation);
void __indirect_glGetColorTable(GLenum target, GLenum format, GLenum type, GLvoid *table);
void __indirect_glGetColorTableParameterfv(GLenum target, GLenum pname, GLfloat *params);
void __indirect_glGetColorTableParameteriv(GLenum target, GLenum pname, GLint *params);
void __indirect_glGetConvolutionFilter(GLenum target, GLenum format, GLenum type, GLvoid *image);
void __indirect_glGetConvolutionParameterfv(GLenum target, GLenum pname, GLfloat *params);
void __indirect_glGetConvolutionParameteriv(GLenum target, GLenum pname, GLint *params);
void __indirect_glGetDoublev(GLenum val, GLdouble *d);
GLenum __indirect_glGetError(void);
void __indirect_glGetFloatv(GLenum val, GLfloat *f);
void __indirect_glGetHistogram(GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
void __indirect_glGetHistogramParameterfv(GLenum target, GLenum pname, GLfloat *params);
void __indirect_glGetHistogramParameteriv(GLenum target, GLenum pname, GLint *params);
void __indirect_glGetIntegerv(GLenum val, GLint *i);
void __indirect_glGetLightfv(GLenum light, GLenum pname, GLfloat *params);
void __indirect_glGetLightiv(GLenum light, GLenum pname, GLint *params);
void __indirect_glGetMapdv(GLenum target, GLenum query, GLdouble *v);
void __indirect_glGetMapfv(GLenum target, GLenum query, GLfloat *v);
void __indirect_glGetMapiv(GLenum target, GLenum query, GLint *v);
void __indirect_glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params);
void __indirect_glGetMaterialiv(GLenum face, GLenum pname, GLint *params);
void __indirect_glGetMinmax(GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
void __indirect_glGetMinmaxParameterfv(GLenum target, GLenum pname, GLfloat *params);
void __indirect_glGetMinmaxParameteriv(GLenum target, GLenum pname, GLint *params);
void __indirect_glGetPixelMapfv(GLenum map, GLfloat *values);
void __indirect_glGetPixelMapuiv(GLenum map, GLuint *values);
void __indirect_glGetPixelMapusv(GLenum map, GLushort *values);
void __indirect_glGetPointerv(GLenum pname, void **params);
void __indirect_glGetPolygonStipple(GLubyte *mask);
const GLubyte *__indirect_glGetString(GLenum name);
void __indirect_glGetSeparableFilter(GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span);
void __indirect_glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params);
void __indirect_glGetTexEnviv(GLenum target, GLenum pname, GLint *params);
void __indirect_glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params);
void __indirect_glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params);
void __indirect_glGetTexGeniv(GLenum coord, GLenum pname, GLint *params);
void __indirect_glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *texels);
void __indirect_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params);
void __indirect_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params);
void __indirect_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
void __indirect_glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
void __indirect_glHint(GLenum target, GLenum mode);
void __indirect_glHistogram(GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
void __indirect_glIndexMask(GLuint mask);
void __indirect_glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer);
void __indirect_glIndexd(GLdouble c);
void __indirect_glIndexdv(const GLdouble *c);
void __indirect_glIndexf(GLfloat c);
void __indirect_glIndexfv(const GLfloat *c);
void __indirect_glIndexi(GLint c);
void __indirect_glIndexiv(const GLint *c);
void __indirect_glIndexs(GLshort c);
void __indirect_glIndexsv(const GLshort *c);
void __indirect_glIndexub(GLubyte c);
void __indirect_glIndexubv(const GLubyte *c);
void __indirect_glInitNames(void);
void __indirect_glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer);
GLboolean __indirect_glIsEnabled(GLenum cap);
GLboolean __indirect_glIsList(GLuint list);
GLboolean __indirect_glIsTexture(GLuint texture);
GLboolean __indirect_glIsTextureEXT(GLuint texture);
void __indirect_glLightModelf(GLenum pname, GLfloat param);
void __indirect_glLightModelfv(GLenum pname, const GLfloat *params);
void __indirect_glLightModeli(GLenum pname, GLint param);
void __indirect_glLightModeliv(GLenum pname, const GLint *params);
void __indirect_glLightf(GLenum light, GLenum pname, GLfloat param);
void __indirect_glLightfv(GLenum light, GLenum pname, const GLfloat *params);
void __indirect_glLighti(GLenum light, GLenum pname, GLint param);
void __indirect_glLightiv(GLenum light, GLenum pname, const GLint *params);
void __indirect_glLineStipple(GLint factor, GLushort pattern);
void __indirect_glLineWidth(GLfloat width);
void __indirect_glListBase(GLuint base);
void __indirect_glLoadIdentity(void);
void __indirect_glLoadMatrixd(const GLdouble *m);
void __indirect_glLoadMatrixf(const GLfloat *m);
void __indirect_glLoadName(GLuint name);
void __indirect_glLogicOp(GLenum opcode);
void __indirect_glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *pnts);
void __indirect_glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *pnts);
void __indirect_glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustr, GLint uord, GLdouble v1, GLdouble v2, GLint vstr, GLint vord, const GLdouble *pnts);
void __indirect_glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustr, GLint uord, GLfloat v1, GLfloat v2, GLint vstr, GLint vord, const GLfloat *pnts);
void __indirect_glMapGrid1d(GLint un, GLdouble u1, GLdouble u2);
void __indirect_glMapGrid1f(GLint un, GLfloat u1, GLfloat u2);
void __indirect_glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
void __indirect_glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void __indirect_glMaterialf(GLenum face, GLenum pname, GLfloat param);
void __indirect_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void __indirect_glMateriali(GLenum face, GLenum pname, GLint param);
void __indirect_glMaterialiv(GLenum face, GLenum pname, const GLint *params);
void __indirect_glMatrixMode(GLenum mode);
void __indirect_glMinmax(GLenum target, GLenum internalformat, GLboolean sink);
void __indirect_glMultMatrixd(const GLdouble *m);
void __indirect_glMultMatrixf(const GLfloat *m);
void __indirect_glNewList(GLuint list, GLenum mode);
void __indirect_glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz);
void __indirect_glNormal3bv(const GLbyte *v);
void __indirect_glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz);
void __indirect_glNormal3dv(const GLdouble *v);
void __indirect_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
void __indirect_glNormal3fv(const GLfloat *v);
void __indirect_glNormal3i(GLint nx, GLint ny, GLint nz);
void __indirect_glNormal3iv(const GLint *v);
void __indirect_glNormal3s(GLshort nx, GLshort ny, GLshort nz);
void __indirect_glNormal3sv(const GLshort *v);
void __indirect_glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer);
void __indirect_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void __indirect_glPassThrough(GLfloat token);
void __indirect_glPixelMapfv(GLenum map, GLint mapsize, const GLfloat *values);
void __indirect_glPixelMapuiv(GLenum map, GLint mapsize, const GLuint *values);
void __indirect_glPixelMapusv(GLenum map, GLint mapsize, const GLushort *values);
void __indirect_glPixelStoref(GLenum pname, GLfloat param);
void __indirect_glPixelStorei(GLenum pname, GLint param);
void __indirect_glPixelTransferf(GLenum pname, GLfloat param);
void __indirect_glPixelTransferi(GLenum pname, GLint param);
void __indirect_glPixelZoom(GLfloat xfactor, GLfloat yfactor);
void __indirect_glPointSize(GLfloat size);
void __indirect_glPolygonMode(GLenum face, GLenum mode);
void __indirect_glPolygonOffset(GLfloat factor, GLfloat units);
void __indirect_glPolygonStipple(const GLubyte *mask);
void __indirect_glPopAttrib(void);
void __indirect_glPopClientAttrib(void);
void __indirect_glPopMatrix(void);
void __indirect_glPopName(void);
void __indirect_glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities);
void __indirect_glPushAttrib(GLbitfield mask);
void __indirect_glPushClientAttrib(GLuint mask);
void __indirect_glPushMatrix(void);
void __indirect_glPushName(GLuint name);
void __indirect_glRasterPos2d(GLdouble x, GLdouble y);
void __indirect_glRasterPos2dv(const GLdouble *v);
void __indirect_glRasterPos2f(GLfloat x, GLfloat y);
void __indirect_glRasterPos2fv(const GLfloat *v);
void __indirect_glRasterPos2i(GLint x, GLint y);
void __indirect_glRasterPos2iv(const GLint *v);
void __indirect_glRasterPos2s(GLshort x, GLshort y);
void __indirect_glRasterPos2sv(const GLshort *v);
void __indirect_glRasterPos3d(GLdouble x, GLdouble y, GLdouble z);
void __indirect_glRasterPos3dv(const GLdouble *v);
void __indirect_glRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
void __indirect_glRasterPos3fv(const GLfloat *v);
void __indirect_glRasterPos3i(GLint x, GLint y, GLint z);
void __indirect_glRasterPos3iv(const GLint *v);
void __indirect_glRasterPos3s(GLshort x, GLshort y, GLshort z);
void __indirect_glRasterPos3sv(const GLshort *v);
void __indirect_glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void __indirect_glRasterPos4dv(const GLdouble *v);
void __indirect_glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void __indirect_glRasterPos4fv(const GLfloat *v);
void __indirect_glRasterPos4i(GLint x, GLint y, GLint z, GLint w);
void __indirect_glRasterPos4iv(const GLint *v);
void __indirect_glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w);
void __indirect_glRasterPos4sv(const GLshort *v);
void __indirect_glReadBuffer(GLenum mode);
void __indirect_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void __indirect_glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
void __indirect_glRectdv(const GLdouble *v1, const GLdouble *v2);
void __indirect_glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void __indirect_glRectfv(const GLfloat *v1, const GLfloat *v2);
void __indirect_glRecti(GLint x1, GLint y1, GLint x2, GLint y2);
void __indirect_glRectiv(const GLint *v1, const GLint *v2);
void __indirect_glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void __indirect_glRectsv(const GLshort *v1, const GLshort *v2);
GLint __indirect_glRenderMode(GLenum mode);
void __indirect_glResetHistogram(GLenum target);
void __indirect_glResetMinmax(GLenum target);
void __indirect_glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void __indirect_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void __indirect_glScaled(GLdouble x, GLdouble y, GLdouble z);
void __indirect_glScalef(GLfloat x, GLfloat y, GLfloat z);
void __indirect_glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void __indirect_glSelectBuffer(GLsizei numnames, GLuint *buffer);
void __indirect_glSeparableFilter2D(GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column);
void __indirect_glShadeModel(GLenum mode);
void __indirect_glStencilFunc(GLenum func, GLint ref, GLuint mask);
void __indirect_glStencilMask(GLuint mask);
void __indirect_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
void __indirect_glTexCoord1d(GLdouble s);
void __indirect_glTexCoord1dv(const GLdouble *v);
void __indirect_glTexCoord1f(GLfloat s);
void __indirect_glTexCoord1fv(const GLfloat *v);
void __indirect_glTexCoord1i(GLint s);
void __indirect_glTexCoord1iv(const GLint *v);
void __indirect_glTexCoord1s(GLshort s);
void __indirect_glTexCoord1sv(const GLshort *v);
void __indirect_glTexCoord2d(GLdouble s, GLdouble t);
void __indirect_glTexCoord2dv(const GLdouble *v);
void __indirect_glTexCoord2f(GLfloat s, GLfloat t);
void __indirect_glTexCoord2fv(const GLfloat *v);
void __indirect_glTexCoord2i(GLint s, GLint t);
void __indirect_glTexCoord2iv(const GLint *v);
void __indirect_glTexCoord2s(GLshort s, GLshort t);
void __indirect_glTexCoord2sv(const GLshort *v);
void __indirect_glTexCoord3d(GLdouble s, GLdouble t, GLdouble r);
void __indirect_glTexCoord3dv(const GLdouble *v);
void __indirect_glTexCoord3f(GLfloat s, GLfloat t, GLfloat r);
void __indirect_glTexCoord3fv(const GLfloat *v);
void __indirect_glTexCoord3i(GLint s, GLint t, GLint r);
void __indirect_glTexCoord3iv(const GLint *v);
void __indirect_glTexCoord3s(GLshort s, GLshort t, GLshort r);
void __indirect_glTexCoord3sv(const GLshort *v);
void __indirect_glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void __indirect_glTexCoord4dv(const GLdouble *v);
void __indirect_glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void __indirect_glTexCoord4fv(const GLfloat *v);
void __indirect_glTexCoord4i(GLint s, GLint t, GLint r, GLint q);
void __indirect_glTexCoord4iv(const GLint *v);
void __indirect_glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q);
void __indirect_glTexCoord4sv(const GLshort *v);
void __indirect_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void __indirect_glTexEnvf(GLenum target, GLenum pname, GLfloat param);
void __indirect_glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);
void __indirect_glTexEnvi(GLenum target, GLenum pname, GLint param);
void __indirect_glTexEnviv(GLenum target, GLenum pname, const GLint *params);
void __indirect_glTexGend(GLenum coord, GLenum pname, GLdouble param);
void __indirect_glTexGendv(GLenum coord, GLenum pname, const GLdouble *params);
void __indirect_glTexGenf(GLenum coord, GLenum pname, GLfloat param);
void __indirect_glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
void __indirect_glTexGeni(GLenum coord, GLenum pname, GLint param);
void __indirect_glTexGeniv(GLenum coord, GLenum pname, const GLint *params);
void __indirect_glTexImage1D(GLenum target, GLint level, GLint components, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glTexImage2D(GLenum target, GLint level, GLint components, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void __indirect_glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
void __indirect_glTexParameteri(GLenum target, GLenum pname, GLint param);
void __indirect_glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
void __indirect_glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *image);
void __indirect_glTranslated(GLdouble x, GLdouble y, GLdouble z);
void __indirect_glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void __indirect_glVertex2d(GLdouble x, GLdouble y);
void __indirect_glVertex2dv(const GLdouble *v);
void __indirect_glVertex2f(GLfloat x, GLfloat y);
void __indirect_glVertex2fv(const GLfloat *v);
void __indirect_glVertex2i(GLint x, GLint y);
void __indirect_glVertex2iv(const GLint *v);
void __indirect_glVertex2s(GLshort x, GLshort y);
void __indirect_glVertex2sv(const GLshort *v);
void __indirect_glVertex3d(GLdouble x, GLdouble y, GLdouble z);
void __indirect_glVertex3dv(const GLdouble *v);
void __indirect_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void __indirect_glVertex3fv(const GLfloat *v);
void __indirect_glVertex3i(GLint x, GLint y, GLint z);
void __indirect_glVertex3iv(const GLint *v);
void __indirect_glVertex3s(GLshort x, GLshort y, GLshort z);
void __indirect_glVertex3sv(const GLshort *v);
void __indirect_glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void __indirect_glVertex4dv(const GLdouble *v);
void __indirect_glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void __indirect_glVertex4fv(const GLfloat *v);
void __indirect_glVertex4i(GLint x, GLint y, GLint z, GLint w);
void __indirect_glVertex4iv(const GLint *v);
void __indirect_glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w);
void __indirect_glVertex4sv(const GLshort *v);
void __indirect_glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void __indirect_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);


void __indirect_glActiveTextureARB(GLenum texture);
void __indirect_glClientActiveTextureARB(GLenum texture);
void __indirect_glMultiTexCoord1dARB(GLenum target, GLdouble s);
void __indirect_glMultiTexCoord1dvARB(GLenum target, const GLdouble *v);
void __indirect_glMultiTexCoord1fARB(GLenum target, GLfloat s);
void __indirect_glMultiTexCoord1fvARB(GLenum target, const GLfloat *v);
void __indirect_glMultiTexCoord1iARB(GLenum target, GLint s);
void __indirect_glMultiTexCoord1ivARB(GLenum target, const GLint *v);
void __indirect_glMultiTexCoord1sARB(GLenum target, GLshort s);
void __indirect_glMultiTexCoord1svARB(GLenum target, const GLshort *v);
void __indirect_glMultiTexCoord2dARB(GLenum target, GLdouble s, GLdouble t);
void __indirect_glMultiTexCoord2dvARB(GLenum target, const GLdouble *v);
void __indirect_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t);
void __indirect_glMultiTexCoord2fvARB(GLenum target, const GLfloat *v);
void __indirect_glMultiTexCoord2iARB(GLenum target, GLint s, GLint t);
void __indirect_glMultiTexCoord2ivARB(GLenum target, const GLint *v);
void __indirect_glMultiTexCoord2sARB(GLenum target, GLshort s, GLshort t);
void __indirect_glMultiTexCoord2svARB(GLenum target, const GLshort *v);
void __indirect_glMultiTexCoord3dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r);
void __indirect_glMultiTexCoord3dvARB(GLenum target, const GLdouble *v);
void __indirect_glMultiTexCoord3fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r);
void __indirect_glMultiTexCoord3fvARB(GLenum target, const GLfloat *v);
void __indirect_glMultiTexCoord3iARB(GLenum target, GLint s, GLint t, GLint r);
void __indirect_glMultiTexCoord3ivARB(GLenum target, const GLint *v);
void __indirect_glMultiTexCoord3sARB(GLenum target, GLshort s, GLshort t, GLshort r);
void __indirect_glMultiTexCoord3svARB(GLenum target, const GLshort *v);
void __indirect_glMultiTexCoord4dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void __indirect_glMultiTexCoord4dvARB(GLenum target, const GLdouble *v);
void __indirect_glMultiTexCoord4fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void __indirect_glMultiTexCoord4fvARB(GLenum target, const GLfloat *v);
void __indirect_glMultiTexCoord4iARB(GLenum target, GLint s, GLint t, GLint r, GLint q);
void __indirect_glMultiTexCoord4ivARB(GLenum target, const GLint *v);
void __indirect_glMultiTexCoord4sARB(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
void __indirect_glMultiTexCoord4svARB(GLenum target, const GLshort *v);


void __indirect_glLoadTransposeMatrixfARB(const GLfloat *m);
void __indirect_glMultTransposeMatrixfARB(const GLfloat *m);
void __indirect_glLoadTransposeMatrixdARB(const GLdouble *m);
void __indirect_glMultTransposeMatrixdARB(const GLdouble *m);

void __indirect_glSampleCoverageARB( GLfloat value, GLboolean invert );

void __indirect_glPointParameterfARB(GLenum pname, GLfloat param);
void __indirect_glPointParameterfvARB(GLenum pname, const GLfloat *params);
void __indirect_glPointParameteri(GLenum, GLint);
void __indirect_glPointParameteriv(GLenum, const GLint *);

void __indirect_glActiveStencilFaceEXT(GLenum mode);

void __indirect_glWindowPos2dARB(GLdouble x, GLdouble y);
void __indirect_glWindowPos2iARB(GLint x, GLint y);
void __indirect_glWindowPos2fARB(GLfloat x, GLfloat y);
void __indirect_glWindowPos2sARB(GLshort x, GLshort y);
void __indirect_glWindowPos2dvARB(const GLdouble * p);
void __indirect_glWindowPos2fvARB(const GLfloat * p);
void __indirect_glWindowPos2ivARB(const GLint * p);
void __indirect_glWindowPos2svARB(const GLshort * p);
void __indirect_glWindowPos3dARB(GLdouble x, GLdouble y, GLdouble z);
void __indirect_glWindowPos3fARB(GLfloat x, GLfloat y, GLfloat z);
void __indirect_glWindowPos3iARB(GLint x, GLint y, GLint z);
void __indirect_glWindowPos3sARB(GLshort x, GLshort y, GLshort z);
void __indirect_glWindowPos3dvARB(const GLdouble * p);
void __indirect_glWindowPos3fvARB(const GLfloat * p);
void __indirect_glWindowPos3ivARB(const GLint * p);
void __indirect_glWindowPos3svARB(const GLshort * p);

void __indirect_glMultiDrawArrays(GLenum mode, GLint *first, GLsizei *count, GLsizei primcount);
void __indirect_glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid ** indices, GLsizei primcount);
void __indirect_glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum);

void __indirect_glSampleMaskSGIS( GLfloat value, GLboolean invert );
void __indirect_glSamplePatternSGIS( GLenum pass );

/* 145. GL_EXT_secondary_color / GL 1.4 */

void __indirect_glSecondaryColorPointer (GLint, GLenum, GLsizei, const GLvoid *);
void __indirect_glSecondaryColor3b(GLbyte red, GLbyte green, GLbyte blue);
void __indirect_glSecondaryColor3bv(const GLbyte *v);
void __indirect_glSecondaryColor3d(GLdouble red, GLdouble green, GLdouble blue);
void __indirect_glSecondaryColor3dv(const GLdouble *v);
void __indirect_glSecondaryColor3f(GLfloat red, GLfloat green, GLfloat blue);
void __indirect_glSecondaryColor3fv(const GLfloat *v);
void __indirect_glSecondaryColor3i(GLint red, GLint green, GLint blue);
void __indirect_glSecondaryColor3iv(const GLint *v);
void __indirect_glSecondaryColor3s(GLshort red, GLshort green, GLshort blue);
void __indirect_glSecondaryColor3sv(const GLshort *v);
void __indirect_glSecondaryColor3ub(GLubyte red, GLubyte green, GLubyte blue);
void __indirect_glSecondaryColor3ubv(const GLubyte *v);
void __indirect_glSecondaryColor3ui(GLuint red, GLuint green, GLuint blue);
void __indirect_glSecondaryColor3uiv(const GLuint *v);
void __indirect_glSecondaryColor3us(GLushort red, GLushort green, GLushort blue);
void __indirect_glSecondaryColor3usv(const GLushort *v);

/* 149. GL_EXT_fog_coord / GL 1.4 */

void __indirect_glFogCoordPointer (GLenum, GLsizei, const GLvoid *);
void __indirect_glFogCoordd(GLdouble f);
void __indirect_glFogCoorddv(const GLdouble *v);
void __indirect_glFogCoordf(GLfloat f);
void __indirect_glFogCoordfv(const GLfloat *v);

#endif /* _INDIRECT_H_ */
