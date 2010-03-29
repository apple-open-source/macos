/* This file was automatically generated with apple_api_generator.tcl. */

/*
 Copyright (c) 2008 Apple Inc.
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name(s) of the above
 copyright holders shall not be used in advertising or otherwise to
 promote the sale, use or other dealings in this Software without
 prior written authorization.
*/

#include <dlfcn.h>
#include "glxclient.h"
#include "apple_api.h"
#include "apple_glx_context.h"
struct apple_api __gl_api;
void glAccum(GLenum a, GLfloat b) {
	 __gl_api.Accum(a, b);
}
void glAlphaFunc(GLenum a, GLclampf b) {
	 __gl_api.AlphaFunc(a, b);
}
GLboolean glAreTexturesResident(GLsizei a, const GLuint * b, GLboolean * c) {
	return  __gl_api.AreTexturesResident(a, b, c);
}
void glArrayElement(GLint a) {
	 __gl_api.ArrayElement(a);
}
void glBegin(GLenum a) {
	 __gl_api.Begin(a);
}
void glBindTexture(GLenum a, GLuint b) {
	 __gl_api.BindTexture(a, b);
}
void glBitmap(GLsizei a, GLsizei b, GLfloat c, GLfloat d, GLfloat e, GLfloat f, const GLubyte * g) {
	 __gl_api.Bitmap(a, b, c, d, e, f, g);
}
void glBlendColor(GLclampf a, GLclampf b, GLclampf c, GLclampf d) {
	 __gl_api.BlendColor(a, b, c, d);
}
void glBlendEquation(GLenum a) {
	 __gl_api.BlendEquation(a);
}
void glBlendEquationSeparate(GLenum a, GLenum b) {
	 __gl_api.BlendEquationSeparate(a, b);
}
void glBlendFunc(GLenum a, GLenum b) {
	 __gl_api.BlendFunc(a, b);
}
void glCallList(GLuint a) {
	 __gl_api.CallList(a);
}
void glCallLists(GLsizei a, GLenum b, const GLvoid * c) {
	 __gl_api.CallLists(a, b, c);
}
void glClear(GLbitfield a) {
	 __gl_api.Clear(a);
}
void glClearAccum(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.ClearAccum(a, b, c, d);
}
void glClearColor(GLclampf a, GLclampf b, GLclampf c, GLclampf d) {
	 __gl_api.ClearColor(a, b, c, d);
}
void glClearDepth(GLclampd a) {
	 __gl_api.ClearDepth(a);
}
void glClearIndex(GLfloat a) {
	 __gl_api.ClearIndex(a);
}
void glClearStencil(GLint a) {
	 __gl_api.ClearStencil(a);
}
void glClipPlane(GLenum a, const GLdouble * b) {
	 __gl_api.ClipPlane(a, b);
}
void glColor3b(GLbyte a, GLbyte b, GLbyte c) {
	 __gl_api.Color3b(a, b, c);
}
void glColor3bv(const GLbyte * a) {
	 __gl_api.Color3bv(a);
}
void glColor3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.Color3d(a, b, c);
}
void glColor3dv(const GLdouble * a) {
	 __gl_api.Color3dv(a);
}
void glColor3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.Color3f(a, b, c);
}
void glColor3fv(const GLfloat * a) {
	 __gl_api.Color3fv(a);
}
void glColor3i(GLint a, GLint b, GLint c) {
	 __gl_api.Color3i(a, b, c);
}
void glColor3iv(const GLint * a) {
	 __gl_api.Color3iv(a);
}
void glColor3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.Color3s(a, b, c);
}
void glColor3sv(const GLshort * a) {
	 __gl_api.Color3sv(a);
}
void glColor3ub(GLubyte a, GLubyte b, GLubyte c) {
	 __gl_api.Color3ub(a, b, c);
}
void glColor3ubv(const GLubyte * a) {
	 __gl_api.Color3ubv(a);
}
void glColor3ui(GLuint a, GLuint b, GLuint c) {
	 __gl_api.Color3ui(a, b, c);
}
void glColor3uiv(const GLuint * a) {
	 __gl_api.Color3uiv(a);
}
void glColor3us(GLushort a, GLushort b, GLushort c) {
	 __gl_api.Color3us(a, b, c);
}
void glColor3usv(const GLushort * a) {
	 __gl_api.Color3usv(a);
}
void glColor4b(GLbyte a, GLbyte b, GLbyte c, GLbyte d) {
	 __gl_api.Color4b(a, b, c, d);
}
void glColor4bv(const GLbyte * a) {
	 __gl_api.Color4bv(a);
}
void glColor4d(GLdouble a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.Color4d(a, b, c, d);
}
void glColor4dv(const GLdouble * a) {
	 __gl_api.Color4dv(a);
}
void glColor4f(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.Color4f(a, b, c, d);
}
void glColor4fv(const GLfloat * a) {
	 __gl_api.Color4fv(a);
}
void glColor4i(GLint a, GLint b, GLint c, GLint d) {
	 __gl_api.Color4i(a, b, c, d);
}
void glColor4iv(const GLint * a) {
	 __gl_api.Color4iv(a);
}
void glColor4s(GLshort a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.Color4s(a, b, c, d);
}
void glColor4sv(const GLshort * a) {
	 __gl_api.Color4sv(a);
}
void glColor4ub(GLubyte a, GLubyte b, GLubyte c, GLubyte d) {
	 __gl_api.Color4ub(a, b, c, d);
}
void glColor4ubv(const GLubyte * a) {
	 __gl_api.Color4ubv(a);
}
void glColor4ui(GLuint a, GLuint b, GLuint c, GLuint d) {
	 __gl_api.Color4ui(a, b, c, d);
}
void glColor4uiv(const GLuint * a) {
	 __gl_api.Color4uiv(a);
}
void glColor4us(GLushort a, GLushort b, GLushort c, GLushort d) {
	 __gl_api.Color4us(a, b, c, d);
}
void glColor4usv(const GLushort * a) {
	 __gl_api.Color4usv(a);
}
void glColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d) {
	 __gl_api.ColorMask(a, b, c, d);
}
void glColorMaterial(GLenum a, GLenum b) {
	 __gl_api.ColorMaterial(a, b);
}
void glColorPointer(GLint a, GLenum b, GLsizei c, const GLvoid * d) {
	 __gl_api.ColorPointer(a, b, c, d);
}
void glColorSubTable(GLenum a, GLsizei b, GLsizei c, GLenum d, GLenum e, const GLvoid * f) {
	 __gl_api.ColorSubTable(a, b, c, d, e, f);
}
void glColorTable(GLenum a, GLenum b, GLsizei c, GLenum d, GLenum e, const GLvoid * f) {
	 __gl_api.ColorTable(a, b, c, d, e, f);
}
void glColorTableParameterfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.ColorTableParameterfv(a, b, c);
}
void glColorTableParameteriv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.ColorTableParameteriv(a, b, c);
}
void glConvolutionFilter1D(GLenum a, GLenum b, GLsizei c, GLenum d, GLenum e, const GLvoid * f) {
	 __gl_api.ConvolutionFilter1D(a, b, c, d, e, f);
}
void glConvolutionFilter2D(GLenum a, GLenum b, GLsizei c, GLsizei d, GLenum e, GLenum f, const GLvoid * g) {
	 __gl_api.ConvolutionFilter2D(a, b, c, d, e, f, g);
}
void glConvolutionParameterf(GLenum a, GLenum b, GLfloat c) {
	 __gl_api.ConvolutionParameterf(a, b, c);
}
void glConvolutionParameterfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.ConvolutionParameterfv(a, b, c);
}
void glConvolutionParameteri(GLenum a, GLenum b, GLint c) {
	 __gl_api.ConvolutionParameteri(a, b, c);
}
void glConvolutionParameteriv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.ConvolutionParameteriv(a, b, c);
}
void glCopyColorSubTable(GLenum a, GLsizei b, GLint c, GLint d, GLsizei e) {
	 __gl_api.CopyColorSubTable(a, b, c, d, e);
}
void glCopyColorTable(GLenum a, GLenum b, GLint c, GLint d, GLsizei e) {
	 __gl_api.CopyColorTable(a, b, c, d, e);
}
void glCopyConvolutionFilter1D(GLenum a, GLenum b, GLint c, GLint d, GLsizei e) {
	 __gl_api.CopyConvolutionFilter1D(a, b, c, d, e);
}
void glCopyConvolutionFilter2D(GLenum a, GLenum b, GLint c, GLint d, GLsizei e, GLsizei f) {
	 __gl_api.CopyConvolutionFilter2D(a, b, c, d, e, f);
}
void glCopyPixels(GLint a, GLint b, GLsizei c, GLsizei d, GLenum e) {
	 __gl_api.CopyPixels(a, b, c, d, e);
}
void glCopyTexImage1D(GLenum a, GLint b, GLenum c, GLint d, GLint e, GLsizei f, GLint g) {
	 __gl_api.CopyTexImage1D(a, b, c, d, e, f, g);
}
void glCopyTexImage2D(GLenum a, GLint b, GLenum c, GLint d, GLint e, GLsizei f, GLsizei g, GLint h) {
	 __gl_api.CopyTexImage2D(a, b, c, d, e, f, g, h);
}
void glCopyTexSubImage1D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLsizei f) {
	 __gl_api.CopyTexSubImage1D(a, b, c, d, e, f);
}
void glCopyTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLint f, GLsizei g, GLsizei h) {
	 __gl_api.CopyTexSubImage2D(a, b, c, d, e, f, g, h);
}
void glCopyTexSubImage3D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLsizei h, GLsizei i) {
	 __gl_api.CopyTexSubImage3D(a, b, c, d, e, f, g, h, i);
}
void glCullFace(GLenum a) {
	 __gl_api.CullFace(a);
}
void glDeleteLists(GLuint a, GLsizei b) {
	 __gl_api.DeleteLists(a, b);
}
void glDeleteTextures(GLsizei a, const GLuint * b) {
	 __gl_api.DeleteTextures(a, b);
}
void glDepthFunc(GLenum a) {
	 __gl_api.DepthFunc(a);
}
void glDepthMask(GLboolean a) {
	 __gl_api.DepthMask(a);
}
void glDepthRange(GLclampd a, GLclampd b) {
	 __gl_api.DepthRange(a, b);
}
void glDisable(GLenum a) {
	 __gl_api.Disable(a);
}
void glDisableClientState(GLenum a) {
	 __gl_api.DisableClientState(a);
}
void glDrawArrays(GLenum a, GLint b, GLsizei c) {
	 __gl_api.DrawArrays(a, b, c);
}
void glDrawBuffer(GLenum a) {
	 __gl_api.DrawBuffer(a);
}
void glDrawElements(GLenum a, GLsizei b, GLenum c, const GLvoid * d) {
	 __gl_api.DrawElements(a, b, c, d);
}
void glDrawPixels(GLsizei a, GLsizei b, GLenum c, GLenum d, const GLvoid * e) {
	 __gl_api.DrawPixels(a, b, c, d, e);
}
void glDrawRangeElements(GLenum a, GLuint b, GLuint c, GLsizei d, GLenum e, const GLvoid * f) {
	 __gl_api.DrawRangeElements(a, b, c, d, e, f);
}
void glEdgeFlag(GLboolean a) {
	 __gl_api.EdgeFlag(a);
}
void glEdgeFlagPointer(GLsizei a, const GLvoid * b) {
	 __gl_api.EdgeFlagPointer(a, b);
}
void glEdgeFlagv(const GLboolean * a) {
	 __gl_api.EdgeFlagv(a);
}
void glEnable(GLenum a) {
	 __gl_api.Enable(a);
}
void glEnableClientState(GLenum a) {
	 __gl_api.EnableClientState(a);
}
void glEnd() {
	 __gl_api.End();
}
void glEndList() {
	 __gl_api.EndList();
}
void glEvalCoord1d(GLdouble a) {
	 __gl_api.EvalCoord1d(a);
}
void glEvalCoord1dv(const GLdouble * a) {
	 __gl_api.EvalCoord1dv(a);
}
void glEvalCoord1f(GLfloat a) {
	 __gl_api.EvalCoord1f(a);
}
void glEvalCoord1fv(const GLfloat * a) {
	 __gl_api.EvalCoord1fv(a);
}
void glEvalCoord2d(GLdouble a, GLdouble b) {
	 __gl_api.EvalCoord2d(a, b);
}
void glEvalCoord2dv(const GLdouble * a) {
	 __gl_api.EvalCoord2dv(a);
}
void glEvalCoord2f(GLfloat a, GLfloat b) {
	 __gl_api.EvalCoord2f(a, b);
}
void glEvalCoord2fv(const GLfloat * a) {
	 __gl_api.EvalCoord2fv(a);
}
void glEvalMesh1(GLenum a, GLint b, GLint c) {
	 __gl_api.EvalMesh1(a, b, c);
}
void glEvalMesh2(GLenum a, GLint b, GLint c, GLint d, GLint e) {
	 __gl_api.EvalMesh2(a, b, c, d, e);
}
void glEvalPoint1(GLint a) {
	 __gl_api.EvalPoint1(a);
}
void glEvalPoint2(GLint a, GLint b) {
	 __gl_api.EvalPoint2(a, b);
}
void glFeedbackBuffer(GLsizei a, GLenum b, GLfloat * c) {
	 __gl_api.FeedbackBuffer(a, b, c);
}
void glFinish() {
	 __gl_api.Finish();
}
void glFlush() {
	 __gl_api.Flush();
}
void glFogf(GLenum a, GLfloat b) {
	 __gl_api.Fogf(a, b);
}
void glFogfv(GLenum a, const GLfloat * b) {
	 __gl_api.Fogfv(a, b);
}
void glFogi(GLenum a, GLint b) {
	 __gl_api.Fogi(a, b);
}
void glFogiv(GLenum a, const GLint * b) {
	 __gl_api.Fogiv(a, b);
}
void glFrontFace(GLenum a) {
	 __gl_api.FrontFace(a);
}
void glFrustum(GLdouble a, GLdouble b, GLdouble c, GLdouble d, GLdouble e, GLdouble f) {
	 __gl_api.Frustum(a, b, c, d, e, f);
}
GLuint glGenLists(GLsizei a) {
	return  __gl_api.GenLists(a);
}
void glGenTextures(GLsizei a, GLuint * b) {
	 __gl_api.GenTextures(a, b);
}
void glGetBooleanv(GLenum a, GLboolean * b) {
	 __gl_api.GetBooleanv(a, b);
}
void glGetClipPlane(GLenum a, GLdouble * b) {
	 __gl_api.GetClipPlane(a, b);
}
void glGetColorTable(GLenum a, GLenum b, GLenum c, GLvoid * d) {
	 __gl_api.GetColorTable(a, b, c, d);
}
void glGetColorTableParameterfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetColorTableParameterfv(a, b, c);
}
void glGetColorTableParameteriv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetColorTableParameteriv(a, b, c);
}
void glGetConvolutionFilter(GLenum a, GLenum b, GLenum c, GLvoid * d) {
	 __gl_api.GetConvolutionFilter(a, b, c, d);
}
void glGetConvolutionParameterfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetConvolutionParameterfv(a, b, c);
}
void glGetConvolutionParameteriv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetConvolutionParameteriv(a, b, c);
}
void glGetDoublev(GLenum a, GLdouble * b) {
	 __gl_api.GetDoublev(a, b);
}
GLenum glGetError() {
	return  __gl_api.GetError();
}
void glGetFloatv(GLenum a, GLfloat * b) {
	 __gl_api.GetFloatv(a, b);
}
void glGetHistogram(GLenum a, GLboolean b, GLenum c, GLenum d, GLvoid * e) {
	 __gl_api.GetHistogram(a, b, c, d, e);
}
void glGetHistogramParameterfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetHistogramParameterfv(a, b, c);
}
void glGetHistogramParameteriv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetHistogramParameteriv(a, b, c);
}
void glGetIntegerv(GLenum a, GLint * b) {
	 __gl_api.GetIntegerv(a, b);
}
void glGetLightfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetLightfv(a, b, c);
}
void glGetLightiv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetLightiv(a, b, c);
}
void glGetMapdv(GLenum a, GLenum b, GLdouble * c) {
	 __gl_api.GetMapdv(a, b, c);
}
void glGetMapfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetMapfv(a, b, c);
}
void glGetMapiv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetMapiv(a, b, c);
}
void glGetMaterialfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetMaterialfv(a, b, c);
}
void glGetMaterialiv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetMaterialiv(a, b, c);
}
void glGetMinmax(GLenum a, GLboolean b, GLenum c, GLenum d, GLvoid * e) {
	 __gl_api.GetMinmax(a, b, c, d, e);
}
void glGetMinmaxParameterfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetMinmaxParameterfv(a, b, c);
}
void glGetMinmaxParameteriv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetMinmaxParameteriv(a, b, c);
}
void glGetPixelMapfv(GLenum a, GLfloat * b) {
	 __gl_api.GetPixelMapfv(a, b);
}
void glGetPixelMapuiv(GLenum a, GLuint * b) {
	 __gl_api.GetPixelMapuiv(a, b);
}
void glGetPixelMapusv(GLenum a, GLushort * b) {
	 __gl_api.GetPixelMapusv(a, b);
}
void glGetPointerv(GLenum a, GLvoid * * b) {
	 __gl_api.GetPointerv(a, b);
}
void glGetPolygonStipple(GLubyte * a) {
	 __gl_api.GetPolygonStipple(a);
}
void glGetSeparableFilter(GLenum a, GLenum b, GLenum c, GLvoid * d, GLvoid * e, GLvoid * f) {
	 __gl_api.GetSeparableFilter(a, b, c, d, e, f);
}
const GLubyte * glGetString(GLenum a) {
	return  __gl_api.GetString(a);
}
void glGetTexEnvfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetTexEnvfv(a, b, c);
}
void glGetTexEnviv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetTexEnviv(a, b, c);
}
void glGetTexGendv(GLenum a, GLenum b, GLdouble * c) {
	 __gl_api.GetTexGendv(a, b, c);
}
void glGetTexGenfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetTexGenfv(a, b, c);
}
void glGetTexGeniv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetTexGeniv(a, b, c);
}
void glGetTexImage(GLenum a, GLint b, GLenum c, GLenum d, GLvoid * e) {
	 __gl_api.GetTexImage(a, b, c, d, e);
}
void glGetTexLevelParameterfv(GLenum a, GLint b, GLenum c, GLfloat * d) {
	 __gl_api.GetTexLevelParameterfv(a, b, c, d);
}
void glGetTexLevelParameteriv(GLenum a, GLint b, GLenum c, GLint * d) {
	 __gl_api.GetTexLevelParameteriv(a, b, c, d);
}
void glGetTexParameterfv(GLenum a, GLenum b, GLfloat * c) {
	 __gl_api.GetTexParameterfv(a, b, c);
}
void glGetTexParameteriv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetTexParameteriv(a, b, c);
}
void glHint(GLenum a, GLenum b) {
	 __gl_api.Hint(a, b);
}
void glHistogram(GLenum a, GLsizei b, GLenum c, GLboolean d) {
	 __gl_api.Histogram(a, b, c, d);
}
void glIndexMask(GLuint a) {
	 __gl_api.IndexMask(a);
}
void glIndexPointer(GLenum a, GLsizei b, const GLvoid * c) {
	 __gl_api.IndexPointer(a, b, c);
}
void glIndexd(GLdouble a) {
	 __gl_api.Indexd(a);
}
void glIndexdv(const GLdouble * a) {
	 __gl_api.Indexdv(a);
}
void glIndexf(GLfloat a) {
	 __gl_api.Indexf(a);
}
void glIndexfv(const GLfloat * a) {
	 __gl_api.Indexfv(a);
}
void glIndexi(GLint a) {
	 __gl_api.Indexi(a);
}
void glIndexiv(const GLint * a) {
	 __gl_api.Indexiv(a);
}
void glIndexs(GLshort a) {
	 __gl_api.Indexs(a);
}
void glIndexsv(const GLshort * a) {
	 __gl_api.Indexsv(a);
}
void glIndexub(GLubyte a) {
	 __gl_api.Indexub(a);
}
void glIndexubv(const GLubyte * a) {
	 __gl_api.Indexubv(a);
}
void glInitNames() {
	 __gl_api.InitNames();
}
void glInterleavedArrays(GLenum a, GLsizei b, const GLvoid * c) {
	 __gl_api.InterleavedArrays(a, b, c);
}
GLboolean glIsEnabled(GLenum a) {
	return  __gl_api.IsEnabled(a);
}
GLboolean glIsList(GLuint a) {
	return  __gl_api.IsList(a);
}
GLboolean glIsTexture(GLuint a) {
	return  __gl_api.IsTexture(a);
}
void glLightModelf(GLenum a, GLfloat b) {
	 __gl_api.LightModelf(a, b);
}
void glLightModelfv(GLenum a, const GLfloat * b) {
	 __gl_api.LightModelfv(a, b);
}
void glLightModeli(GLenum a, GLint b) {
	 __gl_api.LightModeli(a, b);
}
void glLightModeliv(GLenum a, const GLint * b) {
	 __gl_api.LightModeliv(a, b);
}
void glLightf(GLenum a, GLenum b, GLfloat c) {
	 __gl_api.Lightf(a, b, c);
}
void glLightfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.Lightfv(a, b, c);
}
void glLighti(GLenum a, GLenum b, GLint c) {
	 __gl_api.Lighti(a, b, c);
}
void glLightiv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.Lightiv(a, b, c);
}
void glLineStipple(GLint a, GLushort b) {
	 __gl_api.LineStipple(a, b);
}
void glLineWidth(GLfloat a) {
	 __gl_api.LineWidth(a);
}
void glListBase(GLuint a) {
	 __gl_api.ListBase(a);
}
void glLoadIdentity() {
	 __gl_api.LoadIdentity();
}
void glLoadMatrixd(const GLdouble * a) {
	 __gl_api.LoadMatrixd(a);
}
void glLoadMatrixf(const GLfloat * a) {
	 __gl_api.LoadMatrixf(a);
}
void glLoadName(GLuint a) {
	 __gl_api.LoadName(a);
}
void glLogicOp(GLenum a) {
	 __gl_api.LogicOp(a);
}
void glMap1d(GLenum a, GLdouble b, GLdouble c, GLint d, GLint e, const GLdouble * f) {
	 __gl_api.Map1d(a, b, c, d, e, f);
}
void glMap1f(GLenum a, GLfloat b, GLfloat c, GLint d, GLint e, const GLfloat * f) {
	 __gl_api.Map1f(a, b, c, d, e, f);
}
void glMap2d(GLenum a, GLdouble b, GLdouble c, GLint d, GLint e, GLdouble f, GLdouble g, GLint h, GLint i, const GLdouble * j) {
	 __gl_api.Map2d(a, b, c, d, e, f, g, h, i, j);
}
void glMap2f(GLenum a, GLfloat b, GLfloat c, GLint d, GLint e, GLfloat f, GLfloat g, GLint h, GLint i, const GLfloat * j) {
	 __gl_api.Map2f(a, b, c, d, e, f, g, h, i, j);
}
void glMapGrid1d(GLint a, GLdouble b, GLdouble c) {
	 __gl_api.MapGrid1d(a, b, c);
}
void glMapGrid1f(GLint a, GLfloat b, GLfloat c) {
	 __gl_api.MapGrid1f(a, b, c);
}
void glMapGrid2d(GLint a, GLdouble b, GLdouble c, GLint d, GLdouble e, GLdouble f) {
	 __gl_api.MapGrid2d(a, b, c, d, e, f);
}
void glMapGrid2f(GLint a, GLfloat b, GLfloat c, GLint d, GLfloat e, GLfloat f) {
	 __gl_api.MapGrid2f(a, b, c, d, e, f);
}
void glMaterialf(GLenum a, GLenum b, GLfloat c) {
	 __gl_api.Materialf(a, b, c);
}
void glMaterialfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.Materialfv(a, b, c);
}
void glMateriali(GLenum a, GLenum b, GLint c) {
	 __gl_api.Materiali(a, b, c);
}
void glMaterialiv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.Materialiv(a, b, c);
}
void glMatrixMode(GLenum a) {
	 __gl_api.MatrixMode(a);
}
void glMinmax(GLenum a, GLenum b, GLboolean c) {
	 __gl_api.Minmax(a, b, c);
}
void glMultMatrixd(const GLdouble * a) {
	 __gl_api.MultMatrixd(a);
}
void glMultMatrixf(const GLfloat * a) {
	 __gl_api.MultMatrixf(a);
}
void glNewList(GLuint a, GLenum b) {
	 __gl_api.NewList(a, b);
}
void glNormal3b(GLbyte a, GLbyte b, GLbyte c) {
	 __gl_api.Normal3b(a, b, c);
}
void glNormal3bv(const GLbyte * a) {
	 __gl_api.Normal3bv(a);
}
void glNormal3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.Normal3d(a, b, c);
}
void glNormal3dv(const GLdouble * a) {
	 __gl_api.Normal3dv(a);
}
void glNormal3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.Normal3f(a, b, c);
}
void glNormal3fv(const GLfloat * a) {
	 __gl_api.Normal3fv(a);
}
void glNormal3i(GLint a, GLint b, GLint c) {
	 __gl_api.Normal3i(a, b, c);
}
void glNormal3iv(const GLint * a) {
	 __gl_api.Normal3iv(a);
}
void glNormal3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.Normal3s(a, b, c);
}
void glNormal3sv(const GLshort * a) {
	 __gl_api.Normal3sv(a);
}
void glNormalPointer(GLenum a, GLsizei b, const GLvoid * c) {
	 __gl_api.NormalPointer(a, b, c);
}
void glOrtho(GLdouble a, GLdouble b, GLdouble c, GLdouble d, GLdouble e, GLdouble f) {
	 __gl_api.Ortho(a, b, c, d, e, f);
}
void glPassThrough(GLfloat a) {
	 __gl_api.PassThrough(a);
}
void glPixelMapfv(GLenum a, GLint b, const GLfloat * c) {
	 __gl_api.PixelMapfv(a, b, c);
}
void glPixelMapuiv(GLenum a, GLint b, const GLuint * c) {
	 __gl_api.PixelMapuiv(a, b, c);
}
void glPixelMapusv(GLenum a, GLint b, const GLushort * c) {
	 __gl_api.PixelMapusv(a, b, c);
}
void glPixelStoref(GLenum a, GLfloat b) {
	 __gl_api.PixelStoref(a, b);
}
void glPixelStorei(GLenum a, GLint b) {
	 __gl_api.PixelStorei(a, b);
}
void glPixelTransferf(GLenum a, GLfloat b) {
	 __gl_api.PixelTransferf(a, b);
}
void glPixelTransferi(GLenum a, GLint b) {
	 __gl_api.PixelTransferi(a, b);
}
void glPixelZoom(GLfloat a, GLfloat b) {
	 __gl_api.PixelZoom(a, b);
}
void glPointSize(GLfloat a) {
	 __gl_api.PointSize(a);
}
void glPolygonMode(GLenum a, GLenum b) {
	 __gl_api.PolygonMode(a, b);
}
void glPolygonOffset(GLfloat a, GLfloat b) {
	 __gl_api.PolygonOffset(a, b);
}
void glPolygonStipple(const GLubyte * a) {
	 __gl_api.PolygonStipple(a);
}
void glPopAttrib() {
	 __gl_api.PopAttrib();
}
void glPopClientAttrib() {
	 __gl_api.PopClientAttrib();
}
void glPopMatrix() {
	 __gl_api.PopMatrix();
}
void glPopName() {
	 __gl_api.PopName();
}
void glPrioritizeTextures(GLsizei a, const GLuint * b, const GLclampf * c) {
	 __gl_api.PrioritizeTextures(a, b, c);
}
void glPushAttrib(GLbitfield a) {
	 __gl_api.PushAttrib(a);
}
void glPushClientAttrib(GLbitfield a) {
	 __gl_api.PushClientAttrib(a);
}
void glPushMatrix() {
	 __gl_api.PushMatrix();
}
void glPushName(GLuint a) {
	 __gl_api.PushName(a);
}
void glRasterPos2d(GLdouble a, GLdouble b) {
	 __gl_api.RasterPos2d(a, b);
}
void glRasterPos2dv(const GLdouble * a) {
	 __gl_api.RasterPos2dv(a);
}
void glRasterPos2f(GLfloat a, GLfloat b) {
	 __gl_api.RasterPos2f(a, b);
}
void glRasterPos2fv(const GLfloat * a) {
	 __gl_api.RasterPos2fv(a);
}
void glRasterPos2i(GLint a, GLint b) {
	 __gl_api.RasterPos2i(a, b);
}
void glRasterPos2iv(const GLint * a) {
	 __gl_api.RasterPos2iv(a);
}
void glRasterPos2s(GLshort a, GLshort b) {
	 __gl_api.RasterPos2s(a, b);
}
void glRasterPos2sv(const GLshort * a) {
	 __gl_api.RasterPos2sv(a);
}
void glRasterPos3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.RasterPos3d(a, b, c);
}
void glRasterPos3dv(const GLdouble * a) {
	 __gl_api.RasterPos3dv(a);
}
void glRasterPos3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.RasterPos3f(a, b, c);
}
void glRasterPos3fv(const GLfloat * a) {
	 __gl_api.RasterPos3fv(a);
}
void glRasterPos3i(GLint a, GLint b, GLint c) {
	 __gl_api.RasterPos3i(a, b, c);
}
void glRasterPos3iv(const GLint * a) {
	 __gl_api.RasterPos3iv(a);
}
void glRasterPos3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.RasterPos3s(a, b, c);
}
void glRasterPos3sv(const GLshort * a) {
	 __gl_api.RasterPos3sv(a);
}
void glRasterPos4d(GLdouble a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.RasterPos4d(a, b, c, d);
}
void glRasterPos4dv(const GLdouble * a) {
	 __gl_api.RasterPos4dv(a);
}
void glRasterPos4f(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.RasterPos4f(a, b, c, d);
}
void glRasterPos4fv(const GLfloat * a) {
	 __gl_api.RasterPos4fv(a);
}
void glRasterPos4i(GLint a, GLint b, GLint c, GLint d) {
	 __gl_api.RasterPos4i(a, b, c, d);
}
void glRasterPos4iv(const GLint * a) {
	 __gl_api.RasterPos4iv(a);
}
void glRasterPos4s(GLshort a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.RasterPos4s(a, b, c, d);
}
void glRasterPos4sv(const GLshort * a) {
	 __gl_api.RasterPos4sv(a);
}
void glReadBuffer(GLenum a) {
	 __gl_api.ReadBuffer(a);
}
void glReadPixels(GLint a, GLint b, GLsizei c, GLsizei d, GLenum e, GLenum f, GLvoid * g) {
	 __gl_api.ReadPixels(a, b, c, d, e, f, g);
}
void glRectd(GLdouble a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.Rectd(a, b, c, d);
}
void glRectdv(const GLdouble * a, const GLdouble * b) {
	 __gl_api.Rectdv(a, b);
}
void glRectf(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.Rectf(a, b, c, d);
}
void glRectfv(const GLfloat * a, const GLfloat * b) {
	 __gl_api.Rectfv(a, b);
}
void glRecti(GLint a, GLint b, GLint c, GLint d) {
	 __gl_api.Recti(a, b, c, d);
}
void glRectiv(const GLint * a, const GLint * b) {
	 __gl_api.Rectiv(a, b);
}
void glRects(GLshort a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.Rects(a, b, c, d);
}
void glRectsv(const GLshort * a, const GLshort * b) {
	 __gl_api.Rectsv(a, b);
}
GLint glRenderMode(GLenum a) {
	return  __gl_api.RenderMode(a);
}
void glResetHistogram(GLenum a) {
	 __gl_api.ResetHistogram(a);
}
void glResetMinmax(GLenum a) {
	 __gl_api.ResetMinmax(a);
}
void glRotated(GLdouble a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.Rotated(a, b, c, d);
}
void glRotatef(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.Rotatef(a, b, c, d);
}
void glScaled(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.Scaled(a, b, c);
}
void glScalef(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.Scalef(a, b, c);
}
void glScissor(GLint a, GLint b, GLsizei c, GLsizei d) {
	 __gl_api.Scissor(a, b, c, d);
}
void glSelectBuffer(GLsizei a, GLuint * b) {
	 __gl_api.SelectBuffer(a, b);
}
void glSeparableFilter2D(GLenum a, GLenum b, GLsizei c, GLsizei d, GLenum e, GLenum f, const GLvoid * g, const GLvoid * h) {
	 __gl_api.SeparableFilter2D(a, b, c, d, e, f, g, h);
}
void glShadeModel(GLenum a) {
	 __gl_api.ShadeModel(a);
}
void glStencilFunc(GLenum a, GLint b, GLuint c) {
	 __gl_api.StencilFunc(a, b, c);
}
void glStencilMask(GLuint a) {
	 __gl_api.StencilMask(a);
}
void glStencilOp(GLenum a, GLenum b, GLenum c) {
	 __gl_api.StencilOp(a, b, c);
}
void glTexCoord1d(GLdouble a) {
	 __gl_api.TexCoord1d(a);
}
void glTexCoord1dv(const GLdouble * a) {
	 __gl_api.TexCoord1dv(a);
}
void glTexCoord1f(GLfloat a) {
	 __gl_api.TexCoord1f(a);
}
void glTexCoord1fv(const GLfloat * a) {
	 __gl_api.TexCoord1fv(a);
}
void glTexCoord1i(GLint a) {
	 __gl_api.TexCoord1i(a);
}
void glTexCoord1iv(const GLint * a) {
	 __gl_api.TexCoord1iv(a);
}
void glTexCoord1s(GLshort a) {
	 __gl_api.TexCoord1s(a);
}
void glTexCoord1sv(const GLshort * a) {
	 __gl_api.TexCoord1sv(a);
}
void glTexCoord2d(GLdouble a, GLdouble b) {
	 __gl_api.TexCoord2d(a, b);
}
void glTexCoord2dv(const GLdouble * a) {
	 __gl_api.TexCoord2dv(a);
}
void glTexCoord2f(GLfloat a, GLfloat b) {
	 __gl_api.TexCoord2f(a, b);
}
void glTexCoord2fv(const GLfloat * a) {
	 __gl_api.TexCoord2fv(a);
}
void glTexCoord2i(GLint a, GLint b) {
	 __gl_api.TexCoord2i(a, b);
}
void glTexCoord2iv(const GLint * a) {
	 __gl_api.TexCoord2iv(a);
}
void glTexCoord2s(GLshort a, GLshort b) {
	 __gl_api.TexCoord2s(a, b);
}
void glTexCoord2sv(const GLshort * a) {
	 __gl_api.TexCoord2sv(a);
}
void glTexCoord3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.TexCoord3d(a, b, c);
}
void glTexCoord3dv(const GLdouble * a) {
	 __gl_api.TexCoord3dv(a);
}
void glTexCoord3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.TexCoord3f(a, b, c);
}
void glTexCoord3fv(const GLfloat * a) {
	 __gl_api.TexCoord3fv(a);
}
void glTexCoord3i(GLint a, GLint b, GLint c) {
	 __gl_api.TexCoord3i(a, b, c);
}
void glTexCoord3iv(const GLint * a) {
	 __gl_api.TexCoord3iv(a);
}
void glTexCoord3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.TexCoord3s(a, b, c);
}
void glTexCoord3sv(const GLshort * a) {
	 __gl_api.TexCoord3sv(a);
}
void glTexCoord4d(GLdouble a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.TexCoord4d(a, b, c, d);
}
void glTexCoord4dv(const GLdouble * a) {
	 __gl_api.TexCoord4dv(a);
}
void glTexCoord4f(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.TexCoord4f(a, b, c, d);
}
void glTexCoord4fv(const GLfloat * a) {
	 __gl_api.TexCoord4fv(a);
}
void glTexCoord4i(GLint a, GLint b, GLint c, GLint d) {
	 __gl_api.TexCoord4i(a, b, c, d);
}
void glTexCoord4iv(const GLint * a) {
	 __gl_api.TexCoord4iv(a);
}
void glTexCoord4s(GLshort a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.TexCoord4s(a, b, c, d);
}
void glTexCoord4sv(const GLshort * a) {
	 __gl_api.TexCoord4sv(a);
}
void glTexCoordPointer(GLint a, GLenum b, GLsizei c, const GLvoid * d) {
	 __gl_api.TexCoordPointer(a, b, c, d);
}
void glTexEnvf(GLenum a, GLenum b, GLfloat c) {
	 __gl_api.TexEnvf(a, b, c);
}
void glTexEnvfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.TexEnvfv(a, b, c);
}
void glTexEnvi(GLenum a, GLenum b, GLint c) {
	 __gl_api.TexEnvi(a, b, c);
}
void glTexEnviv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.TexEnviv(a, b, c);
}
void glTexGend(GLenum a, GLenum b, GLdouble c) {
	 __gl_api.TexGend(a, b, c);
}
void glTexGendv(GLenum a, GLenum b, const GLdouble * c) {
	 __gl_api.TexGendv(a, b, c);
}
void glTexGenf(GLenum a, GLenum b, GLfloat c) {
	 __gl_api.TexGenf(a, b, c);
}
void glTexGenfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.TexGenfv(a, b, c);
}
void glTexGeni(GLenum a, GLenum b, GLint c) {
	 __gl_api.TexGeni(a, b, c);
}
void glTexGeniv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.TexGeniv(a, b, c);
}
void glTexImage1D(GLenum a, GLint b, GLenum c, GLsizei d, GLint e, GLenum f, GLenum g, const GLvoid * h) {
	 __gl_api.TexImage1D(a, b, c, d, e, f, g, h);
}
void glTexImage2D(GLenum a, GLint b, GLenum c, GLsizei d, GLsizei e, GLint f, GLenum g, GLenum h, const GLvoid * i) {
	 __gl_api.TexImage2D(a, b, c, d, e, f, g, h, i);
}
void glTexImage3D(GLenum a, GLint b, GLenum c, GLsizei d, GLsizei e, GLsizei f, GLint g, GLenum h, GLenum i, const GLvoid * j) {
	 __gl_api.TexImage3D(a, b, c, d, e, f, g, h, i, j);
}
void glTexParameterf(GLenum a, GLenum b, GLfloat c) {
	 __gl_api.TexParameterf(a, b, c);
}
void glTexParameterfv(GLenum a, GLenum b, const GLfloat * c) {
	 __gl_api.TexParameterfv(a, b, c);
}
void glTexParameteri(GLenum a, GLenum b, GLint c) {
	 __gl_api.TexParameteri(a, b, c);
}
void glTexParameteriv(GLenum a, GLenum b, const GLint * c) {
	 __gl_api.TexParameteriv(a, b, c);
}
void glTexSubImage1D(GLenum a, GLint b, GLint c, GLsizei d, GLenum e, GLenum f, const GLvoid * g) {
	 __gl_api.TexSubImage1D(a, b, c, d, e, f, g);
}
void glTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLenum h, const GLvoid * i) {
	 __gl_api.TexSubImage2D(a, b, c, d, e, f, g, h, i);
}
void glTexSubImage3D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLsizei f, GLsizei g, GLsizei h, GLenum i, GLenum j, const GLvoid * k) {
	 __gl_api.TexSubImage3D(a, b, c, d, e, f, g, h, i, j, k);
}
void glTranslated(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.Translated(a, b, c);
}
void glTranslatef(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.Translatef(a, b, c);
}
void glVertex2d(GLdouble a, GLdouble b) {
	 __gl_api.Vertex2d(a, b);
}
void glVertex2dv(const GLdouble * a) {
	 __gl_api.Vertex2dv(a);
}
void glVertex2f(GLfloat a, GLfloat b) {
	 __gl_api.Vertex2f(a, b);
}
void glVertex2fv(const GLfloat * a) {
	 __gl_api.Vertex2fv(a);
}
void glVertex2i(GLint a, GLint b) {
	 __gl_api.Vertex2i(a, b);
}
void glVertex2iv(const GLint * a) {
	 __gl_api.Vertex2iv(a);
}
void glVertex2s(GLshort a, GLshort b) {
	 __gl_api.Vertex2s(a, b);
}
void glVertex2sv(const GLshort * a) {
	 __gl_api.Vertex2sv(a);
}
void glVertex3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.Vertex3d(a, b, c);
}
void glVertex3dv(const GLdouble * a) {
	 __gl_api.Vertex3dv(a);
}
void glVertex3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.Vertex3f(a, b, c);
}
void glVertex3fv(const GLfloat * a) {
	 __gl_api.Vertex3fv(a);
}
void glVertex3i(GLint a, GLint b, GLint c) {
	 __gl_api.Vertex3i(a, b, c);
}
void glVertex3iv(const GLint * a) {
	 __gl_api.Vertex3iv(a);
}
void glVertex3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.Vertex3s(a, b, c);
}
void glVertex3sv(const GLshort * a) {
	 __gl_api.Vertex3sv(a);
}
void glVertex4d(GLdouble a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.Vertex4d(a, b, c, d);
}
void glVertex4dv(const GLdouble * a) {
	 __gl_api.Vertex4dv(a);
}
void glVertex4f(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.Vertex4f(a, b, c, d);
}
void glVertex4fv(const GLfloat * a) {
	 __gl_api.Vertex4fv(a);
}
void glVertex4i(GLint a, GLint b, GLint c, GLint d) {
	 __gl_api.Vertex4i(a, b, c, d);
}
void glVertex4iv(const GLint * a) {
	 __gl_api.Vertex4iv(a);
}
void glVertex4s(GLshort a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.Vertex4s(a, b, c, d);
}
void glVertex4sv(const GLshort * a) {
	 __gl_api.Vertex4sv(a);
}
void glVertexPointer(GLint a, GLenum b, GLsizei c, const GLvoid * d) {
	 __gl_api.VertexPointer(a, b, c, d);
}
void glViewport(GLint a, GLint b, GLsizei c, GLsizei d) {
	 __gl_api.Viewport(a, b, c, d);
}
void glSampleCoverage(GLclampf a, GLboolean b) {
	 __gl_api.SampleCoverage(a, b);
}
void glSamplePass(GLenum a) {
	 __gl_api.SamplePass(a);
}
void glLoadTransposeMatrixf(const GLfloat * a) {
	 __gl_api.LoadTransposeMatrixf(a);
}
void glLoadTransposeMatrixd(const GLdouble * a) {
	 __gl_api.LoadTransposeMatrixd(a);
}
void glMultTransposeMatrixf(const GLfloat * a) {
	 __gl_api.MultTransposeMatrixf(a);
}
void glMultTransposeMatrixd(const GLdouble * a) {
	 __gl_api.MultTransposeMatrixd(a);
}
void glCompressedTexImage3D(GLenum a, GLint b, GLenum c, GLsizei d, GLsizei e, GLsizei f, GLint g, GLsizei h, const GLvoid * i) {
	 __gl_api.CompressedTexImage3D(a, b, c, d, e, f, g, h, i);
}
void glCompressedTexImage2D(GLenum a, GLint b, GLenum c, GLsizei d, GLsizei e, GLint f, GLsizei g, const GLvoid * h) {
	 __gl_api.CompressedTexImage2D(a, b, c, d, e, f, g, h);
}
void glCompressedTexImage1D(GLenum a, GLint b, GLenum c, GLsizei d, GLint e, GLsizei f, const GLvoid * g) {
	 __gl_api.CompressedTexImage1D(a, b, c, d, e, f, g);
}
void glCompressedTexSubImage3D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLsizei f, GLsizei g, GLsizei h, GLenum i, GLsizei j, const GLvoid * k) {
	 __gl_api.CompressedTexSubImage3D(a, b, c, d, e, f, g, h, i, j, k);
}
void glCompressedTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLsizei h, const GLvoid * i) {
	 __gl_api.CompressedTexSubImage2D(a, b, c, d, e, f, g, h, i);
}
void glCompressedTexSubImage1D(GLenum a, GLint b, GLint c, GLsizei d, GLenum e, GLsizei f, const GLvoid * g) {
	 __gl_api.CompressedTexSubImage1D(a, b, c, d, e, f, g);
}
void glGetCompressedTexImage(GLenum a, GLint b, GLvoid * c) {
	 __gl_api.GetCompressedTexImage(a, b, c);
}
void glActiveTexture(GLenum a) {
	 __gl_api.ActiveTexture(a);
}
void glClientActiveTexture(GLenum a) {
	 __gl_api.ClientActiveTexture(a);
}
void glMultiTexCoord1d(GLenum a, GLdouble b) {
	 __gl_api.MultiTexCoord1d(a, b);
}
void glMultiTexCoord1dv(GLenum a, const GLdouble * b) {
	 __gl_api.MultiTexCoord1dv(a, b);
}
void glMultiTexCoord1f(GLenum a, GLfloat b) {
	 __gl_api.MultiTexCoord1f(a, b);
}
void glMultiTexCoord1fv(GLenum a, const GLfloat * b) {
	 __gl_api.MultiTexCoord1fv(a, b);
}
void glMultiTexCoord1i(GLenum a, GLint b) {
	 __gl_api.MultiTexCoord1i(a, b);
}
void glMultiTexCoord1iv(GLenum a, const GLint * b) {
	 __gl_api.MultiTexCoord1iv(a, b);
}
void glMultiTexCoord1s(GLenum a, GLshort b) {
	 __gl_api.MultiTexCoord1s(a, b);
}
void glMultiTexCoord1sv(GLenum a, const GLshort * b) {
	 __gl_api.MultiTexCoord1sv(a, b);
}
void glMultiTexCoord2d(GLenum a, GLdouble b, GLdouble c) {
	 __gl_api.MultiTexCoord2d(a, b, c);
}
void glMultiTexCoord2dv(GLenum a, const GLdouble * b) {
	 __gl_api.MultiTexCoord2dv(a, b);
}
void glMultiTexCoord2f(GLenum a, GLfloat b, GLfloat c) {
	 __gl_api.MultiTexCoord2f(a, b, c);
}
void glMultiTexCoord2fv(GLenum a, const GLfloat * b) {
	 __gl_api.MultiTexCoord2fv(a, b);
}
void glMultiTexCoord2i(GLenum a, GLint b, GLint c) {
	 __gl_api.MultiTexCoord2i(a, b, c);
}
void glMultiTexCoord2iv(GLenum a, const GLint * b) {
	 __gl_api.MultiTexCoord2iv(a, b);
}
void glMultiTexCoord2s(GLenum a, GLshort b, GLshort c) {
	 __gl_api.MultiTexCoord2s(a, b, c);
}
void glMultiTexCoord2sv(GLenum a, const GLshort * b) {
	 __gl_api.MultiTexCoord2sv(a, b);
}
void glMultiTexCoord3d(GLenum a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.MultiTexCoord3d(a, b, c, d);
}
void glMultiTexCoord3dv(GLenum a, const GLdouble * b) {
	 __gl_api.MultiTexCoord3dv(a, b);
}
void glMultiTexCoord3f(GLenum a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.MultiTexCoord3f(a, b, c, d);
}
void glMultiTexCoord3fv(GLenum a, const GLfloat * b) {
	 __gl_api.MultiTexCoord3fv(a, b);
}
void glMultiTexCoord3i(GLenum a, GLint b, GLint c, GLint d) {
	 __gl_api.MultiTexCoord3i(a, b, c, d);
}
void glMultiTexCoord3iv(GLenum a, const GLint * b) {
	 __gl_api.MultiTexCoord3iv(a, b);
}
void glMultiTexCoord3s(GLenum a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.MultiTexCoord3s(a, b, c, d);
}
void glMultiTexCoord3sv(GLenum a, const GLshort * b) {
	 __gl_api.MultiTexCoord3sv(a, b);
}
void glMultiTexCoord4d(GLenum a, GLdouble b, GLdouble c, GLdouble d, GLdouble e) {
	 __gl_api.MultiTexCoord4d(a, b, c, d, e);
}
void glMultiTexCoord4dv(GLenum a, const GLdouble * b) {
	 __gl_api.MultiTexCoord4dv(a, b);
}
void glMultiTexCoord4f(GLenum a, GLfloat b, GLfloat c, GLfloat d, GLfloat e) {
	 __gl_api.MultiTexCoord4f(a, b, c, d, e);
}
void glMultiTexCoord4fv(GLenum a, const GLfloat * b) {
	 __gl_api.MultiTexCoord4fv(a, b);
}
void glMultiTexCoord4i(GLenum a, GLint b, GLint c, GLint d, GLint e) {
	 __gl_api.MultiTexCoord4i(a, b, c, d, e);
}
void glMultiTexCoord4iv(GLenum a, const GLint * b) {
	 __gl_api.MultiTexCoord4iv(a, b);
}
void glMultiTexCoord4s(GLenum a, GLshort b, GLshort c, GLshort d, GLshort e) {
	 __gl_api.MultiTexCoord4s(a, b, c, d, e);
}
void glMultiTexCoord4sv(GLenum a, const GLshort * b) {
	 __gl_api.MultiTexCoord4sv(a, b);
}
void glFogCoordf(GLfloat a) {
	 __gl_api.FogCoordf(a);
}
void glFogCoordfv(const GLfloat * a) {
	 __gl_api.FogCoordfv(a);
}
void glFogCoordd(GLdouble a) {
	 __gl_api.FogCoordd(a);
}
void glFogCoorddv(const GLdouble * a) {
	 __gl_api.FogCoorddv(a);
}
void glFogCoordPointer(GLenum a, GLsizei b, const GLvoid * c) {
	 __gl_api.FogCoordPointer(a, b, c);
}
void glSecondaryColor3b(GLbyte a, GLbyte b, GLbyte c) {
	 __gl_api.SecondaryColor3b(a, b, c);
}
void glSecondaryColor3bv(const GLbyte * a) {
	 __gl_api.SecondaryColor3bv(a);
}
void glSecondaryColor3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.SecondaryColor3d(a, b, c);
}
void glSecondaryColor3dv(const GLdouble * a) {
	 __gl_api.SecondaryColor3dv(a);
}
void glSecondaryColor3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.SecondaryColor3f(a, b, c);
}
void glSecondaryColor3fv(const GLfloat * a) {
	 __gl_api.SecondaryColor3fv(a);
}
void glSecondaryColor3i(GLint a, GLint b, GLint c) {
	 __gl_api.SecondaryColor3i(a, b, c);
}
void glSecondaryColor3iv(const GLint * a) {
	 __gl_api.SecondaryColor3iv(a);
}
void glSecondaryColor3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.SecondaryColor3s(a, b, c);
}
void glSecondaryColor3sv(const GLshort * a) {
	 __gl_api.SecondaryColor3sv(a);
}
void glSecondaryColor3ub(GLubyte a, GLubyte b, GLubyte c) {
	 __gl_api.SecondaryColor3ub(a, b, c);
}
void glSecondaryColor3ubv(const GLubyte * a) {
	 __gl_api.SecondaryColor3ubv(a);
}
void glSecondaryColor3ui(GLuint a, GLuint b, GLuint c) {
	 __gl_api.SecondaryColor3ui(a, b, c);
}
void glSecondaryColor3uiv(const GLuint * a) {
	 __gl_api.SecondaryColor3uiv(a);
}
void glSecondaryColor3us(GLushort a, GLushort b, GLushort c) {
	 __gl_api.SecondaryColor3us(a, b, c);
}
void glSecondaryColor3usv(const GLushort * a) {
	 __gl_api.SecondaryColor3usv(a);
}
void glSecondaryColorPointer(GLint a, GLenum b, GLsizei c, const GLvoid * d) {
	 __gl_api.SecondaryColorPointer(a, b, c, d);
}
void glPointParameterf(GLenum a, GLfloat b) {
	 __gl_api.PointParameterf(a, b);
}
void glPointParameterfv(GLenum a, const GLfloat * b) {
	 __gl_api.PointParameterfv(a, b);
}
void glPointParameteri(GLenum a, GLint b) {
	 __gl_api.PointParameteri(a, b);
}
void glPointParameteriv(GLenum a, const GLint * b) {
	 __gl_api.PointParameteriv(a, b);
}
void glBlendFuncSeparate(GLenum a, GLenum b, GLenum c, GLenum d) {
	 __gl_api.BlendFuncSeparate(a, b, c, d);
}
void glMultiDrawArrays(GLenum a, const GLint * b, const GLsizei * c, GLsizei d) {
	 __gl_api.MultiDrawArrays(a, b, c, d);
}
void glMultiDrawElements(GLenum a, const GLsizei * b, GLenum c, const GLvoid * * d, GLsizei e) {
	 __gl_api.MultiDrawElements(a, b, c, d, e);
}
void glWindowPos2d(GLdouble a, GLdouble b) {
	 __gl_api.WindowPos2d(a, b);
}
void glWindowPos2dv(const GLdouble * a) {
	 __gl_api.WindowPos2dv(a);
}
void glWindowPos2f(GLfloat a, GLfloat b) {
	 __gl_api.WindowPos2f(a, b);
}
void glWindowPos2fv(const GLfloat * a) {
	 __gl_api.WindowPos2fv(a);
}
void glWindowPos2i(GLint a, GLint b) {
	 __gl_api.WindowPos2i(a, b);
}
void glWindowPos2iv(const GLint * a) {
	 __gl_api.WindowPos2iv(a);
}
void glWindowPos2s(GLshort a, GLshort b) {
	 __gl_api.WindowPos2s(a, b);
}
void glWindowPos2sv(const GLshort * a) {
	 __gl_api.WindowPos2sv(a);
}
void glWindowPos3d(GLdouble a, GLdouble b, GLdouble c) {
	 __gl_api.WindowPos3d(a, b, c);
}
void glWindowPos3dv(const GLdouble * a) {
	 __gl_api.WindowPos3dv(a);
}
void glWindowPos3f(GLfloat a, GLfloat b, GLfloat c) {
	 __gl_api.WindowPos3f(a, b, c);
}
void glWindowPos3fv(const GLfloat * a) {
	 __gl_api.WindowPos3fv(a);
}
void glWindowPos3i(GLint a, GLint b, GLint c) {
	 __gl_api.WindowPos3i(a, b, c);
}
void glWindowPos3iv(const GLint * a) {
	 __gl_api.WindowPos3iv(a);
}
void glWindowPos3s(GLshort a, GLshort b, GLshort c) {
	 __gl_api.WindowPos3s(a, b, c);
}
void glWindowPos3sv(const GLshort * a) {
	 __gl_api.WindowPos3sv(a);
}
void glGenQueries(GLsizei a, GLuint * b) {
	 __gl_api.GenQueries(a, b);
}
void glDeleteQueries(GLsizei a, const GLuint * b) {
	 __gl_api.DeleteQueries(a, b);
}
GLboolean glIsQuery(GLuint a) {
	return  __gl_api.IsQuery(a);
}
void glBeginQuery(GLenum a, GLuint b) {
	 __gl_api.BeginQuery(a, b);
}
void glEndQuery(GLenum a) {
	 __gl_api.EndQuery(a);
}
void glGetQueryiv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetQueryiv(a, b, c);
}
void glGetQueryObjectiv(GLuint a, GLenum b, GLint * c) {
	 __gl_api.GetQueryObjectiv(a, b, c);
}
void glGetQueryObjectuiv(GLuint a, GLenum b, GLuint * c) {
	 __gl_api.GetQueryObjectuiv(a, b, c);
}
void glBindBuffer(GLenum a, GLuint b) {
	 __gl_api.BindBuffer(a, b);
}
void glDeleteBuffers(GLsizei a, const GLuint * b) {
	 __gl_api.DeleteBuffers(a, b);
}
void glGenBuffers(GLsizei a, GLuint * b) {
	 __gl_api.GenBuffers(a, b);
}
GLboolean glIsBuffer(GLuint a) {
	return  __gl_api.IsBuffer(a);
}
void glBufferData(GLenum a, GLsizeiptr b, const GLvoid * c, GLenum d) {
	 __gl_api.BufferData(a, b, c, d);
}
void glBufferSubData(GLenum a, GLintptr b, GLsizeiptr c, const GLvoid * d) {
	 __gl_api.BufferSubData(a, b, c, d);
}
void glGetBufferSubData(GLenum a, GLintptr b, GLsizeiptr c, GLvoid * d) {
	 __gl_api.GetBufferSubData(a, b, c, d);
}
GLvoid * glMapBuffer(GLenum a, GLenum b) {
	return  __gl_api.MapBuffer(a, b);
}
GLboolean glUnmapBuffer(GLenum a) {
	return  __gl_api.UnmapBuffer(a);
}
void glGetBufferParameteriv(GLenum a, GLenum b, GLint * c) {
	 __gl_api.GetBufferParameteriv(a, b, c);
}
void glGetBufferPointerv(GLenum a, GLenum b, GLvoid * * c) {
	 __gl_api.GetBufferPointerv(a, b, c);
}
void glDrawBuffers(GLsizei a, const GLenum * b) {
	 __gl_api.DrawBuffers(a, b);
}
void glVertexAttrib1d(GLuint a, GLdouble b) {
	 __gl_api.VertexAttrib1d(a, b);
}
void glVertexAttrib1dv(GLuint a, const GLdouble * b) {
	 __gl_api.VertexAttrib1dv(a, b);
}
void glVertexAttrib1f(GLuint a, GLfloat b) {
	 __gl_api.VertexAttrib1f(a, b);
}
void glVertexAttrib1fv(GLuint a, const GLfloat * b) {
	 __gl_api.VertexAttrib1fv(a, b);
}
void glVertexAttrib1s(GLuint a, GLshort b) {
	 __gl_api.VertexAttrib1s(a, b);
}
void glVertexAttrib1sv(GLuint a, const GLshort * b) {
	 __gl_api.VertexAttrib1sv(a, b);
}
void glVertexAttrib2d(GLuint a, GLdouble b, GLdouble c) {
	 __gl_api.VertexAttrib2d(a, b, c);
}
void glVertexAttrib2dv(GLuint a, const GLdouble * b) {
	 __gl_api.VertexAttrib2dv(a, b);
}
void glVertexAttrib2f(GLuint a, GLfloat b, GLfloat c) {
	 __gl_api.VertexAttrib2f(a, b, c);
}
void glVertexAttrib2fv(GLuint a, const GLfloat * b) {
	 __gl_api.VertexAttrib2fv(a, b);
}
void glVertexAttrib2s(GLuint a, GLshort b, GLshort c) {
	 __gl_api.VertexAttrib2s(a, b, c);
}
void glVertexAttrib2sv(GLuint a, const GLshort * b) {
	 __gl_api.VertexAttrib2sv(a, b);
}
void glVertexAttrib3d(GLuint a, GLdouble b, GLdouble c, GLdouble d) {
	 __gl_api.VertexAttrib3d(a, b, c, d);
}
void glVertexAttrib3dv(GLuint a, const GLdouble * b) {
	 __gl_api.VertexAttrib3dv(a, b);
}
void glVertexAttrib3f(GLuint a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.VertexAttrib3f(a, b, c, d);
}
void glVertexAttrib3fv(GLuint a, const GLfloat * b) {
	 __gl_api.VertexAttrib3fv(a, b);
}
void glVertexAttrib3s(GLuint a, GLshort b, GLshort c, GLshort d) {
	 __gl_api.VertexAttrib3s(a, b, c, d);
}
void glVertexAttrib3sv(GLuint a, const GLshort * b) {
	 __gl_api.VertexAttrib3sv(a, b);
}
void glVertexAttrib4Nbv(GLuint a, const GLbyte * b) {
	 __gl_api.VertexAttrib4Nbv(a, b);
}
void glVertexAttrib4Niv(GLuint a, const GLint * b) {
	 __gl_api.VertexAttrib4Niv(a, b);
}
void glVertexAttrib4Nsv(GLuint a, const GLshort * b) {
	 __gl_api.VertexAttrib4Nsv(a, b);
}
void glVertexAttrib4Nub(GLuint a, GLubyte b, GLubyte c, GLubyte d, GLubyte e) {
	 __gl_api.VertexAttrib4Nub(a, b, c, d, e);
}
void glVertexAttrib4Nubv(GLuint a, const GLubyte * b) {
	 __gl_api.VertexAttrib4Nubv(a, b);
}
void glVertexAttrib4Nuiv(GLuint a, const GLuint * b) {
	 __gl_api.VertexAttrib4Nuiv(a, b);
}
void glVertexAttrib4Nusv(GLuint a, const GLushort * b) {
	 __gl_api.VertexAttrib4Nusv(a, b);
}
void glVertexAttrib4bv(GLuint a, const GLbyte * b) {
	 __gl_api.VertexAttrib4bv(a, b);
}
void glVertexAttrib4d(GLuint a, GLdouble b, GLdouble c, GLdouble d, GLdouble e) {
	 __gl_api.VertexAttrib4d(a, b, c, d, e);
}
void glVertexAttrib4dv(GLuint a, const GLdouble * b) {
	 __gl_api.VertexAttrib4dv(a, b);
}
void glVertexAttrib4f(GLuint a, GLfloat b, GLfloat c, GLfloat d, GLfloat e) {
	 __gl_api.VertexAttrib4f(a, b, c, d, e);
}
void glVertexAttrib4fv(GLuint a, const GLfloat * b) {
	 __gl_api.VertexAttrib4fv(a, b);
}
void glVertexAttrib4iv(GLuint a, const GLint * b) {
	 __gl_api.VertexAttrib4iv(a, b);
}
void glVertexAttrib4s(GLuint a, GLshort b, GLshort c, GLshort d, GLshort e) {
	 __gl_api.VertexAttrib4s(a, b, c, d, e);
}
void glVertexAttrib4sv(GLuint a, const GLshort * b) {
	 __gl_api.VertexAttrib4sv(a, b);
}
void glVertexAttrib4ubv(GLuint a, const GLubyte * b) {
	 __gl_api.VertexAttrib4ubv(a, b);
}
void glVertexAttrib4uiv(GLuint a, const GLuint * b) {
	 __gl_api.VertexAttrib4uiv(a, b);
}
void glVertexAttrib4usv(GLuint a, const GLushort * b) {
	 __gl_api.VertexAttrib4usv(a, b);
}
void glVertexAttribPointer(GLuint a, GLint b, GLenum c, GLboolean d, GLsizei e, const GLvoid * f) {
	 __gl_api.VertexAttribPointer(a, b, c, d, e, f);
}
void glEnableVertexAttribArray(GLuint a) {
	 __gl_api.EnableVertexAttribArray(a);
}
void glDisableVertexAttribArray(GLuint a) {
	 __gl_api.DisableVertexAttribArray(a);
}
void glGetVertexAttribdv(GLuint a, GLenum b, GLdouble * c) {
	 __gl_api.GetVertexAttribdv(a, b, c);
}
void glGetVertexAttribfv(GLuint a, GLenum b, GLfloat * c) {
	 __gl_api.GetVertexAttribfv(a, b, c);
}
void glGetVertexAttribiv(GLuint a, GLenum b, GLint * c) {
	 __gl_api.GetVertexAttribiv(a, b, c);
}
void glGetVertexAttribPointerv(GLuint a, GLenum b, GLvoid * * c) {
	 __gl_api.GetVertexAttribPointerv(a, b, c);
}
void glDeleteShader(GLuint a) {
	 __gl_api.DeleteShader(a);
}
void glDetachShader(GLuint a, GLuint b) {
	 __gl_api.DetachShader(a, b);
}
GLuint glCreateShader(GLenum a) {
	return  __gl_api.CreateShader(a);
}
void glShaderSource(GLuint a, GLsizei b, const GLchar * * c, const GLint * d) {
	 __gl_api.ShaderSource(a, b, c, d);
}
void glCompileShader(GLuint a) {
	 __gl_api.CompileShader(a);
}
GLuint glCreateProgram() {
	return  __gl_api.CreateProgram();
}
void glAttachShader(GLuint a, GLuint b) {
	 __gl_api.AttachShader(a, b);
}
void glLinkProgram(GLuint a) {
	 __gl_api.LinkProgram(a);
}
void glUseProgram(GLuint a) {
	 __gl_api.UseProgram(a);
}
void glDeleteProgram(GLuint a) {
	 __gl_api.DeleteProgram(a);
}
void glValidateProgram(GLuint a) {
	 __gl_api.ValidateProgram(a);
}
void glUniform1f(GLint a, GLfloat b) {
	 __gl_api.Uniform1f(a, b);
}
void glUniform2f(GLint a, GLfloat b, GLfloat c) {
	 __gl_api.Uniform2f(a, b, c);
}
void glUniform3f(GLint a, GLfloat b, GLfloat c, GLfloat d) {
	 __gl_api.Uniform3f(a, b, c, d);
}
void glUniform4f(GLint a, GLfloat b, GLfloat c, GLfloat d, GLfloat e) {
	 __gl_api.Uniform4f(a, b, c, d, e);
}
void glUniform1i(GLint a, GLint b) {
	 __gl_api.Uniform1i(a, b);
}
void glUniform2i(GLint a, GLint b, GLint c) {
	 __gl_api.Uniform2i(a, b, c);
}
void glUniform3i(GLint a, GLint b, GLint c, GLint d) {
	 __gl_api.Uniform3i(a, b, c, d);
}
void glUniform4i(GLint a, GLint b, GLint c, GLint d, GLint e) {
	 __gl_api.Uniform4i(a, b, c, d, e);
}
void glUniform1fv(GLint a, GLsizei b, const GLfloat * c) {
	 __gl_api.Uniform1fv(a, b, c);
}
void glUniform2fv(GLint a, GLsizei b, const GLfloat * c) {
	 __gl_api.Uniform2fv(a, b, c);
}
void glUniform3fv(GLint a, GLsizei b, const GLfloat * c) {
	 __gl_api.Uniform3fv(a, b, c);
}
void glUniform4fv(GLint a, GLsizei b, const GLfloat * c) {
	 __gl_api.Uniform4fv(a, b, c);
}
void glUniform1iv(GLint a, GLsizei b, const GLint * c) {
	 __gl_api.Uniform1iv(a, b, c);
}
void glUniform2iv(GLint a, GLsizei b, const GLint * c) {
	 __gl_api.Uniform2iv(a, b, c);
}
void glUniform3iv(GLint a, GLsizei b, const GLint * c) {
	 __gl_api.Uniform3iv(a, b, c);
}
void glUniform4iv(GLint a, GLsizei b, const GLint * c) {
	 __gl_api.Uniform4iv(a, b, c);
}
void glUniformMatrix2fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix2fv(a, b, c, d);
}
void glUniformMatrix3fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix3fv(a, b, c, d);
}
void glUniformMatrix4fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix4fv(a, b, c, d);
}
GLboolean glIsShader(GLuint a) {
	return  __gl_api.IsShader(a);
}
GLboolean glIsProgram(GLuint a) {
	return  __gl_api.IsProgram(a);
}
void glGetShaderiv(GLuint a, GLenum b, GLint * c) {
	 __gl_api.GetShaderiv(a, b, c);
}
void glGetProgramiv(GLuint a, GLenum b, GLint * c) {
	 __gl_api.GetProgramiv(a, b, c);
}
void glGetAttachedShaders(GLuint a, GLsizei b, GLsizei * c, GLuint * d) {
	 __gl_api.GetAttachedShaders(a, b, c, d);
}
void glGetShaderInfoLog(GLuint a, GLsizei b, GLsizei * c, GLchar * d) {
	 __gl_api.GetShaderInfoLog(a, b, c, d);
}
void glGetProgramInfoLog(GLuint a, GLsizei b, GLsizei * c, GLchar * d) {
	 __gl_api.GetProgramInfoLog(a, b, c, d);
}
GLint glGetUniformLocation(GLuint a, const GLchar * b) {
	return  __gl_api.GetUniformLocation(a, b);
}
void glGetActiveUniform(GLuint a, GLuint b, GLsizei c, GLsizei * d, GLint * e, GLenum * f, GLchar * g) {
	 __gl_api.GetActiveUniform(a, b, c, d, e, f, g);
}
void glGetUniformfv(GLuint a, GLint b, GLfloat * c) {
	 __gl_api.GetUniformfv(a, b, c);
}
void glGetUniformiv(GLuint a, GLint b, GLint * c) {
	 __gl_api.GetUniformiv(a, b, c);
}
void glGetShaderSource(GLuint a, GLsizei b, GLsizei * c, GLchar * d) {
	 __gl_api.GetShaderSource(a, b, c, d);
}
void glBindAttribLocation(GLuint a, GLuint b, const GLchar * c) {
	 __gl_api.BindAttribLocation(a, b, c);
}
void glGetActiveAttrib(GLuint a, GLuint b, GLsizei c, GLsizei * d, GLint * e, GLenum * f, GLchar * g) {
	 __gl_api.GetActiveAttrib(a, b, c, d, e, f, g);
}
GLint glGetAttribLocation(GLuint a, const GLchar * b) {
	return  __gl_api.GetAttribLocation(a, b);
}
void glStencilFuncSeparate(GLenum a, GLenum b, GLint c, GLuint d) {
	 __gl_api.StencilFuncSeparate(a, b, c, d);
}
void glStencilOpSeparate(GLenum a, GLenum b, GLenum c, GLenum d) {
	 __gl_api.StencilOpSeparate(a, b, c, d);
}
void glStencilMaskSeparate(GLenum a, GLuint b) {
	 __gl_api.StencilMaskSeparate(a, b);
}
void glUniformMatrix2x3fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix2x3fv(a, b, c, d);
}
void glUniformMatrix3x2fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix3x2fv(a, b, c, d);
}
void glUniformMatrix2x4fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix2x4fv(a, b, c, d);
}
void glUniformMatrix4x2fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix4x2fv(a, b, c, d);
}
void glUniformMatrix3x4fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix3x4fv(a, b, c, d);
}
void glUniformMatrix4x3fv(GLint a, GLsizei b, GLboolean c, const GLfloat * d) {
	 __gl_api.UniformMatrix4x3fv(a, b, c, d);
}



void apple_api_init_direct(void) {

	void *h;
	
#ifndef LIBGLNAME
#define LIBGLNAME "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#endif LIBGLNAME

	/*warning: dlerror is known to not be thread-safe in POSIX. */
	(void)dlerror(); /*drain dlerror();*/

	h = dlopen(LIBGLNAME, RTLD_NOW);
	if(NULL == h) {
	    fprintf(stderr, "error: unable to dlopen " LIBGLNAME " : " "%s\n",
		    dlerror());
	    abort();
	}
    
	__gl_api.Accum = dlsym(h, "glAccum");
	if(NULL == __gl_api.Accum) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glAccum", dlerror());
		abort();
	}
	__gl_api.AlphaFunc = dlsym(h, "glAlphaFunc");
	if(NULL == __gl_api.AlphaFunc) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glAlphaFunc", dlerror());
		abort();
	}
	__gl_api.AreTexturesResident = dlsym(h, "glAreTexturesResident");
	if(NULL == __gl_api.AreTexturesResident) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glAreTexturesResident", dlerror());
		abort();
	}
	__gl_api.ArrayElement = dlsym(h, "glArrayElement");
	if(NULL == __gl_api.ArrayElement) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glArrayElement", dlerror());
		abort();
	}
	__gl_api.Begin = dlsym(h, "glBegin");
	if(NULL == __gl_api.Begin) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBegin", dlerror());
		abort();
	}
	__gl_api.BindTexture = dlsym(h, "glBindTexture");
	if(NULL == __gl_api.BindTexture) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBindTexture", dlerror());
		abort();
	}
	__gl_api.Bitmap = dlsym(h, "glBitmap");
	if(NULL == __gl_api.Bitmap) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBitmap", dlerror());
		abort();
	}
	__gl_api.BlendColor = dlsym(h, "glBlendColor");
	if(NULL == __gl_api.BlendColor) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBlendColor", dlerror());
		abort();
	}
	__gl_api.BlendEquation = dlsym(h, "glBlendEquation");
	if(NULL == __gl_api.BlendEquation) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBlendEquation", dlerror());
		abort();
	}
	__gl_api.BlendEquationSeparate = dlsym(h, "glBlendEquationSeparate");
	if(NULL == __gl_api.BlendEquationSeparate) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBlendEquationSeparate", dlerror());
		abort();
	}
	__gl_api.BlendFunc = dlsym(h, "glBlendFunc");
	if(NULL == __gl_api.BlendFunc) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBlendFunc", dlerror());
		abort();
	}
	__gl_api.CallList = dlsym(h, "glCallList");
	if(NULL == __gl_api.CallList) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCallList", dlerror());
		abort();
	}
	__gl_api.CallLists = dlsym(h, "glCallLists");
	if(NULL == __gl_api.CallLists) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCallLists", dlerror());
		abort();
	}
	__gl_api.Clear = dlsym(h, "glClear");
	if(NULL == __gl_api.Clear) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClear", dlerror());
		abort();
	}
	__gl_api.ClearAccum = dlsym(h, "glClearAccum");
	if(NULL == __gl_api.ClearAccum) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClearAccum", dlerror());
		abort();
	}
	__gl_api.ClearColor = dlsym(h, "glClearColor");
	if(NULL == __gl_api.ClearColor) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClearColor", dlerror());
		abort();
	}
	__gl_api.ClearDepth = dlsym(h, "glClearDepth");
	if(NULL == __gl_api.ClearDepth) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClearDepth", dlerror());
		abort();
	}
	__gl_api.ClearIndex = dlsym(h, "glClearIndex");
	if(NULL == __gl_api.ClearIndex) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClearIndex", dlerror());
		abort();
	}
	__gl_api.ClearStencil = dlsym(h, "glClearStencil");
	if(NULL == __gl_api.ClearStencil) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClearStencil", dlerror());
		abort();
	}
	__gl_api.ClipPlane = dlsym(h, "glClipPlane");
	if(NULL == __gl_api.ClipPlane) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClipPlane", dlerror());
		abort();
	}
	__gl_api.Color3b = dlsym(h, "glColor3b");
	if(NULL == __gl_api.Color3b) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3b", dlerror());
		abort();
	}
	__gl_api.Color3bv = dlsym(h, "glColor3bv");
	if(NULL == __gl_api.Color3bv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3bv", dlerror());
		abort();
	}
	__gl_api.Color3d = dlsym(h, "glColor3d");
	if(NULL == __gl_api.Color3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3d", dlerror());
		abort();
	}
	__gl_api.Color3dv = dlsym(h, "glColor3dv");
	if(NULL == __gl_api.Color3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3dv", dlerror());
		abort();
	}
	__gl_api.Color3f = dlsym(h, "glColor3f");
	if(NULL == __gl_api.Color3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3f", dlerror());
		abort();
	}
	__gl_api.Color3fv = dlsym(h, "glColor3fv");
	if(NULL == __gl_api.Color3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3fv", dlerror());
		abort();
	}
	__gl_api.Color3i = dlsym(h, "glColor3i");
	if(NULL == __gl_api.Color3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3i", dlerror());
		abort();
	}
	__gl_api.Color3iv = dlsym(h, "glColor3iv");
	if(NULL == __gl_api.Color3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3iv", dlerror());
		abort();
	}
	__gl_api.Color3s = dlsym(h, "glColor3s");
	if(NULL == __gl_api.Color3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3s", dlerror());
		abort();
	}
	__gl_api.Color3sv = dlsym(h, "glColor3sv");
	if(NULL == __gl_api.Color3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3sv", dlerror());
		abort();
	}
	__gl_api.Color3ub = dlsym(h, "glColor3ub");
	if(NULL == __gl_api.Color3ub) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3ub", dlerror());
		abort();
	}
	__gl_api.Color3ubv = dlsym(h, "glColor3ubv");
	if(NULL == __gl_api.Color3ubv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3ubv", dlerror());
		abort();
	}
	__gl_api.Color3ui = dlsym(h, "glColor3ui");
	if(NULL == __gl_api.Color3ui) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3ui", dlerror());
		abort();
	}
	__gl_api.Color3uiv = dlsym(h, "glColor3uiv");
	if(NULL == __gl_api.Color3uiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3uiv", dlerror());
		abort();
	}
	__gl_api.Color3us = dlsym(h, "glColor3us");
	if(NULL == __gl_api.Color3us) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3us", dlerror());
		abort();
	}
	__gl_api.Color3usv = dlsym(h, "glColor3usv");
	if(NULL == __gl_api.Color3usv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor3usv", dlerror());
		abort();
	}
	__gl_api.Color4b = dlsym(h, "glColor4b");
	if(NULL == __gl_api.Color4b) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4b", dlerror());
		abort();
	}
	__gl_api.Color4bv = dlsym(h, "glColor4bv");
	if(NULL == __gl_api.Color4bv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4bv", dlerror());
		abort();
	}
	__gl_api.Color4d = dlsym(h, "glColor4d");
	if(NULL == __gl_api.Color4d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4d", dlerror());
		abort();
	}
	__gl_api.Color4dv = dlsym(h, "glColor4dv");
	if(NULL == __gl_api.Color4dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4dv", dlerror());
		abort();
	}
	__gl_api.Color4f = dlsym(h, "glColor4f");
	if(NULL == __gl_api.Color4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4f", dlerror());
		abort();
	}
	__gl_api.Color4fv = dlsym(h, "glColor4fv");
	if(NULL == __gl_api.Color4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4fv", dlerror());
		abort();
	}
	__gl_api.Color4i = dlsym(h, "glColor4i");
	if(NULL == __gl_api.Color4i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4i", dlerror());
		abort();
	}
	__gl_api.Color4iv = dlsym(h, "glColor4iv");
	if(NULL == __gl_api.Color4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4iv", dlerror());
		abort();
	}
	__gl_api.Color4s = dlsym(h, "glColor4s");
	if(NULL == __gl_api.Color4s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4s", dlerror());
		abort();
	}
	__gl_api.Color4sv = dlsym(h, "glColor4sv");
	if(NULL == __gl_api.Color4sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4sv", dlerror());
		abort();
	}
	__gl_api.Color4ub = dlsym(h, "glColor4ub");
	if(NULL == __gl_api.Color4ub) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4ub", dlerror());
		abort();
	}
	__gl_api.Color4ubv = dlsym(h, "glColor4ubv");
	if(NULL == __gl_api.Color4ubv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4ubv", dlerror());
		abort();
	}
	__gl_api.Color4ui = dlsym(h, "glColor4ui");
	if(NULL == __gl_api.Color4ui) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4ui", dlerror());
		abort();
	}
	__gl_api.Color4uiv = dlsym(h, "glColor4uiv");
	if(NULL == __gl_api.Color4uiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4uiv", dlerror());
		abort();
	}
	__gl_api.Color4us = dlsym(h, "glColor4us");
	if(NULL == __gl_api.Color4us) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4us", dlerror());
		abort();
	}
	__gl_api.Color4usv = dlsym(h, "glColor4usv");
	if(NULL == __gl_api.Color4usv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColor4usv", dlerror());
		abort();
	}
	__gl_api.ColorMask = dlsym(h, "glColorMask");
	if(NULL == __gl_api.ColorMask) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorMask", dlerror());
		abort();
	}
	__gl_api.ColorMaterial = dlsym(h, "glColorMaterial");
	if(NULL == __gl_api.ColorMaterial) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorMaterial", dlerror());
		abort();
	}
	__gl_api.ColorPointer = dlsym(h, "glColorPointer");
	if(NULL == __gl_api.ColorPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorPointer", dlerror());
		abort();
	}
	__gl_api.ColorSubTable = dlsym(h, "glColorSubTable");
	if(NULL == __gl_api.ColorSubTable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorSubTable", dlerror());
		abort();
	}
	__gl_api.ColorTable = dlsym(h, "glColorTable");
	if(NULL == __gl_api.ColorTable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorTable", dlerror());
		abort();
	}
	__gl_api.ColorTableParameterfv = dlsym(h, "glColorTableParameterfv");
	if(NULL == __gl_api.ColorTableParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorTableParameterfv", dlerror());
		abort();
	}
	__gl_api.ColorTableParameteriv = dlsym(h, "glColorTableParameteriv");
	if(NULL == __gl_api.ColorTableParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glColorTableParameteriv", dlerror());
		abort();
	}
	__gl_api.ConvolutionFilter1D = dlsym(h, "glConvolutionFilter1D");
	if(NULL == __gl_api.ConvolutionFilter1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glConvolutionFilter1D", dlerror());
		abort();
	}
	__gl_api.ConvolutionFilter2D = dlsym(h, "glConvolutionFilter2D");
	if(NULL == __gl_api.ConvolutionFilter2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glConvolutionFilter2D", dlerror());
		abort();
	}
	__gl_api.ConvolutionParameterf = dlsym(h, "glConvolutionParameterf");
	if(NULL == __gl_api.ConvolutionParameterf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glConvolutionParameterf", dlerror());
		abort();
	}
	__gl_api.ConvolutionParameterfv = dlsym(h, "glConvolutionParameterfv");
	if(NULL == __gl_api.ConvolutionParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glConvolutionParameterfv", dlerror());
		abort();
	}
	__gl_api.ConvolutionParameteri = dlsym(h, "glConvolutionParameteri");
	if(NULL == __gl_api.ConvolutionParameteri) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glConvolutionParameteri", dlerror());
		abort();
	}
	__gl_api.ConvolutionParameteriv = dlsym(h, "glConvolutionParameteriv");
	if(NULL == __gl_api.ConvolutionParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glConvolutionParameteriv", dlerror());
		abort();
	}
	__gl_api.CopyColorSubTable = dlsym(h, "glCopyColorSubTable");
	if(NULL == __gl_api.CopyColorSubTable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyColorSubTable", dlerror());
		abort();
	}
	__gl_api.CopyColorTable = dlsym(h, "glCopyColorTable");
	if(NULL == __gl_api.CopyColorTable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyColorTable", dlerror());
		abort();
	}
	__gl_api.CopyConvolutionFilter1D = dlsym(h, "glCopyConvolutionFilter1D");
	if(NULL == __gl_api.CopyConvolutionFilter1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyConvolutionFilter1D", dlerror());
		abort();
	}
	__gl_api.CopyConvolutionFilter2D = dlsym(h, "glCopyConvolutionFilter2D");
	if(NULL == __gl_api.CopyConvolutionFilter2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyConvolutionFilter2D", dlerror());
		abort();
	}
	__gl_api.CopyPixels = dlsym(h, "glCopyPixels");
	if(NULL == __gl_api.CopyPixels) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyPixels", dlerror());
		abort();
	}
	__gl_api.CopyTexImage1D = dlsym(h, "glCopyTexImage1D");
	if(NULL == __gl_api.CopyTexImage1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyTexImage1D", dlerror());
		abort();
	}
	__gl_api.CopyTexImage2D = dlsym(h, "glCopyTexImage2D");
	if(NULL == __gl_api.CopyTexImage2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyTexImage2D", dlerror());
		abort();
	}
	__gl_api.CopyTexSubImage1D = dlsym(h, "glCopyTexSubImage1D");
	if(NULL == __gl_api.CopyTexSubImage1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyTexSubImage1D", dlerror());
		abort();
	}
	__gl_api.CopyTexSubImage2D = dlsym(h, "glCopyTexSubImage2D");
	if(NULL == __gl_api.CopyTexSubImage2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyTexSubImage2D", dlerror());
		abort();
	}
	__gl_api.CopyTexSubImage3D = dlsym(h, "glCopyTexSubImage3D");
	if(NULL == __gl_api.CopyTexSubImage3D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCopyTexSubImage3D", dlerror());
		abort();
	}
	__gl_api.CullFace = dlsym(h, "glCullFace");
	if(NULL == __gl_api.CullFace) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCullFace", dlerror());
		abort();
	}
	__gl_api.DeleteLists = dlsym(h, "glDeleteLists");
	if(NULL == __gl_api.DeleteLists) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDeleteLists", dlerror());
		abort();
	}
	__gl_api.DeleteTextures = dlsym(h, "glDeleteTextures");
	if(NULL == __gl_api.DeleteTextures) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDeleteTextures", dlerror());
		abort();
	}
	__gl_api.DepthFunc = dlsym(h, "glDepthFunc");
	if(NULL == __gl_api.DepthFunc) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDepthFunc", dlerror());
		abort();
	}
	__gl_api.DepthMask = dlsym(h, "glDepthMask");
	if(NULL == __gl_api.DepthMask) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDepthMask", dlerror());
		abort();
	}
	__gl_api.DepthRange = dlsym(h, "glDepthRange");
	if(NULL == __gl_api.DepthRange) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDepthRange", dlerror());
		abort();
	}
	__gl_api.Disable = dlsym(h, "glDisable");
	if(NULL == __gl_api.Disable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDisable", dlerror());
		abort();
	}
	__gl_api.DisableClientState = dlsym(h, "glDisableClientState");
	if(NULL == __gl_api.DisableClientState) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDisableClientState", dlerror());
		abort();
	}
	__gl_api.DrawArrays = dlsym(h, "glDrawArrays");
	if(NULL == __gl_api.DrawArrays) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDrawArrays", dlerror());
		abort();
	}
	__gl_api.DrawBuffer = dlsym(h, "glDrawBuffer");
	if(NULL == __gl_api.DrawBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDrawBuffer", dlerror());
		abort();
	}
	__gl_api.DrawElements = dlsym(h, "glDrawElements");
	if(NULL == __gl_api.DrawElements) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDrawElements", dlerror());
		abort();
	}
	__gl_api.DrawPixels = dlsym(h, "glDrawPixels");
	if(NULL == __gl_api.DrawPixels) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDrawPixels", dlerror());
		abort();
	}
	__gl_api.DrawRangeElements = dlsym(h, "glDrawRangeElements");
	if(NULL == __gl_api.DrawRangeElements) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDrawRangeElements", dlerror());
		abort();
	}
	__gl_api.EdgeFlag = dlsym(h, "glEdgeFlag");
	if(NULL == __gl_api.EdgeFlag) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEdgeFlag", dlerror());
		abort();
	}
	__gl_api.EdgeFlagPointer = dlsym(h, "glEdgeFlagPointer");
	if(NULL == __gl_api.EdgeFlagPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEdgeFlagPointer", dlerror());
		abort();
	}
	__gl_api.EdgeFlagv = dlsym(h, "glEdgeFlagv");
	if(NULL == __gl_api.EdgeFlagv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEdgeFlagv", dlerror());
		abort();
	}
	__gl_api.Enable = dlsym(h, "glEnable");
	if(NULL == __gl_api.Enable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEnable", dlerror());
		abort();
	}
	__gl_api.EnableClientState = dlsym(h, "glEnableClientState");
	if(NULL == __gl_api.EnableClientState) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEnableClientState", dlerror());
		abort();
	}
	__gl_api.End = dlsym(h, "glEnd");
	if(NULL == __gl_api.End) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEnd", dlerror());
		abort();
	}
	__gl_api.EndList = dlsym(h, "glEndList");
	if(NULL == __gl_api.EndList) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEndList", dlerror());
		abort();
	}
	__gl_api.EvalCoord1d = dlsym(h, "glEvalCoord1d");
	if(NULL == __gl_api.EvalCoord1d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord1d", dlerror());
		abort();
	}
	__gl_api.EvalCoord1dv = dlsym(h, "glEvalCoord1dv");
	if(NULL == __gl_api.EvalCoord1dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord1dv", dlerror());
		abort();
	}
	__gl_api.EvalCoord1f = dlsym(h, "glEvalCoord1f");
	if(NULL == __gl_api.EvalCoord1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord1f", dlerror());
		abort();
	}
	__gl_api.EvalCoord1fv = dlsym(h, "glEvalCoord1fv");
	if(NULL == __gl_api.EvalCoord1fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord1fv", dlerror());
		abort();
	}
	__gl_api.EvalCoord2d = dlsym(h, "glEvalCoord2d");
	if(NULL == __gl_api.EvalCoord2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord2d", dlerror());
		abort();
	}
	__gl_api.EvalCoord2dv = dlsym(h, "glEvalCoord2dv");
	if(NULL == __gl_api.EvalCoord2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord2dv", dlerror());
		abort();
	}
	__gl_api.EvalCoord2f = dlsym(h, "glEvalCoord2f");
	if(NULL == __gl_api.EvalCoord2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord2f", dlerror());
		abort();
	}
	__gl_api.EvalCoord2fv = dlsym(h, "glEvalCoord2fv");
	if(NULL == __gl_api.EvalCoord2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalCoord2fv", dlerror());
		abort();
	}
	__gl_api.EvalMesh1 = dlsym(h, "glEvalMesh1");
	if(NULL == __gl_api.EvalMesh1) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalMesh1", dlerror());
		abort();
	}
	__gl_api.EvalMesh2 = dlsym(h, "glEvalMesh2");
	if(NULL == __gl_api.EvalMesh2) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalMesh2", dlerror());
		abort();
	}
	__gl_api.EvalPoint1 = dlsym(h, "glEvalPoint1");
	if(NULL == __gl_api.EvalPoint1) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalPoint1", dlerror());
		abort();
	}
	__gl_api.EvalPoint2 = dlsym(h, "glEvalPoint2");
	if(NULL == __gl_api.EvalPoint2) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEvalPoint2", dlerror());
		abort();
	}
	__gl_api.FeedbackBuffer = dlsym(h, "glFeedbackBuffer");
	if(NULL == __gl_api.FeedbackBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFeedbackBuffer", dlerror());
		abort();
	}
	__gl_api.Finish = dlsym(h, "glFinish");
	if(NULL == __gl_api.Finish) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFinish", dlerror());
		abort();
	}
	__gl_api.Flush = dlsym(h, "glFlush");
	if(NULL == __gl_api.Flush) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFlush", dlerror());
		abort();
	}
	__gl_api.Fogf = dlsym(h, "glFogf");
	if(NULL == __gl_api.Fogf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogf", dlerror());
		abort();
	}
	__gl_api.Fogfv = dlsym(h, "glFogfv");
	if(NULL == __gl_api.Fogfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogfv", dlerror());
		abort();
	}
	__gl_api.Fogi = dlsym(h, "glFogi");
	if(NULL == __gl_api.Fogi) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogi", dlerror());
		abort();
	}
	__gl_api.Fogiv = dlsym(h, "glFogiv");
	if(NULL == __gl_api.Fogiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogiv", dlerror());
		abort();
	}
	__gl_api.FrontFace = dlsym(h, "glFrontFace");
	if(NULL == __gl_api.FrontFace) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFrontFace", dlerror());
		abort();
	}
	__gl_api.Frustum = dlsym(h, "glFrustum");
	if(NULL == __gl_api.Frustum) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFrustum", dlerror());
		abort();
	}
	__gl_api.GenLists = dlsym(h, "glGenLists");
	if(NULL == __gl_api.GenLists) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGenLists", dlerror());
		abort();
	}
	__gl_api.GenTextures = dlsym(h, "glGenTextures");
	if(NULL == __gl_api.GenTextures) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGenTextures", dlerror());
		abort();
	}
	__gl_api.GetBooleanv = dlsym(h, "glGetBooleanv");
	if(NULL == __gl_api.GetBooleanv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetBooleanv", dlerror());
		abort();
	}
	__gl_api.GetClipPlane = dlsym(h, "glGetClipPlane");
	if(NULL == __gl_api.GetClipPlane) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetClipPlane", dlerror());
		abort();
	}
	__gl_api.GetColorTable = dlsym(h, "glGetColorTable");
	if(NULL == __gl_api.GetColorTable) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetColorTable", dlerror());
		abort();
	}
	__gl_api.GetColorTableParameterfv = dlsym(h, "glGetColorTableParameterfv");
	if(NULL == __gl_api.GetColorTableParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetColorTableParameterfv", dlerror());
		abort();
	}
	__gl_api.GetColorTableParameteriv = dlsym(h, "glGetColorTableParameteriv");
	if(NULL == __gl_api.GetColorTableParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetColorTableParameteriv", dlerror());
		abort();
	}
	__gl_api.GetConvolutionFilter = dlsym(h, "glGetConvolutionFilter");
	if(NULL == __gl_api.GetConvolutionFilter) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetConvolutionFilter", dlerror());
		abort();
	}
	__gl_api.GetConvolutionParameterfv = dlsym(h, "glGetConvolutionParameterfv");
	if(NULL == __gl_api.GetConvolutionParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetConvolutionParameterfv", dlerror());
		abort();
	}
	__gl_api.GetConvolutionParameteriv = dlsym(h, "glGetConvolutionParameteriv");
	if(NULL == __gl_api.GetConvolutionParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetConvolutionParameteriv", dlerror());
		abort();
	}
	__gl_api.GetDoublev = dlsym(h, "glGetDoublev");
	if(NULL == __gl_api.GetDoublev) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetDoublev", dlerror());
		abort();
	}
	__gl_api.GetError = dlsym(h, "glGetError");
	if(NULL == __gl_api.GetError) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetError", dlerror());
		abort();
	}
	__gl_api.GetFloatv = dlsym(h, "glGetFloatv");
	if(NULL == __gl_api.GetFloatv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetFloatv", dlerror());
		abort();
	}
	__gl_api.GetHistogram = dlsym(h, "glGetHistogram");
	if(NULL == __gl_api.GetHistogram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetHistogram", dlerror());
		abort();
	}
	__gl_api.GetHistogramParameterfv = dlsym(h, "glGetHistogramParameterfv");
	if(NULL == __gl_api.GetHistogramParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetHistogramParameterfv", dlerror());
		abort();
	}
	__gl_api.GetHistogramParameteriv = dlsym(h, "glGetHistogramParameteriv");
	if(NULL == __gl_api.GetHistogramParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetHistogramParameteriv", dlerror());
		abort();
	}
	__gl_api.GetIntegerv = dlsym(h, "glGetIntegerv");
	if(NULL == __gl_api.GetIntegerv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetIntegerv", dlerror());
		abort();
	}
	__gl_api.GetLightfv = dlsym(h, "glGetLightfv");
	if(NULL == __gl_api.GetLightfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetLightfv", dlerror());
		abort();
	}
	__gl_api.GetLightiv = dlsym(h, "glGetLightiv");
	if(NULL == __gl_api.GetLightiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetLightiv", dlerror());
		abort();
	}
	__gl_api.GetMapdv = dlsym(h, "glGetMapdv");
	if(NULL == __gl_api.GetMapdv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMapdv", dlerror());
		abort();
	}
	__gl_api.GetMapfv = dlsym(h, "glGetMapfv");
	if(NULL == __gl_api.GetMapfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMapfv", dlerror());
		abort();
	}
	__gl_api.GetMapiv = dlsym(h, "glGetMapiv");
	if(NULL == __gl_api.GetMapiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMapiv", dlerror());
		abort();
	}
	__gl_api.GetMaterialfv = dlsym(h, "glGetMaterialfv");
	if(NULL == __gl_api.GetMaterialfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMaterialfv", dlerror());
		abort();
	}
	__gl_api.GetMaterialiv = dlsym(h, "glGetMaterialiv");
	if(NULL == __gl_api.GetMaterialiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMaterialiv", dlerror());
		abort();
	}
	__gl_api.GetMinmax = dlsym(h, "glGetMinmax");
	if(NULL == __gl_api.GetMinmax) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMinmax", dlerror());
		abort();
	}
	__gl_api.GetMinmaxParameterfv = dlsym(h, "glGetMinmaxParameterfv");
	if(NULL == __gl_api.GetMinmaxParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMinmaxParameterfv", dlerror());
		abort();
	}
	__gl_api.GetMinmaxParameteriv = dlsym(h, "glGetMinmaxParameteriv");
	if(NULL == __gl_api.GetMinmaxParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetMinmaxParameteriv", dlerror());
		abort();
	}
	__gl_api.GetPixelMapfv = dlsym(h, "glGetPixelMapfv");
	if(NULL == __gl_api.GetPixelMapfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetPixelMapfv", dlerror());
		abort();
	}
	__gl_api.GetPixelMapuiv = dlsym(h, "glGetPixelMapuiv");
	if(NULL == __gl_api.GetPixelMapuiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetPixelMapuiv", dlerror());
		abort();
	}
	__gl_api.GetPixelMapusv = dlsym(h, "glGetPixelMapusv");
	if(NULL == __gl_api.GetPixelMapusv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetPixelMapusv", dlerror());
		abort();
	}
	__gl_api.GetPointerv = dlsym(h, "glGetPointerv");
	if(NULL == __gl_api.GetPointerv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetPointerv", dlerror());
		abort();
	}
	__gl_api.GetPolygonStipple = dlsym(h, "glGetPolygonStipple");
	if(NULL == __gl_api.GetPolygonStipple) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetPolygonStipple", dlerror());
		abort();
	}
	__gl_api.GetSeparableFilter = dlsym(h, "glGetSeparableFilter");
	if(NULL == __gl_api.GetSeparableFilter) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetSeparableFilter", dlerror());
		abort();
	}
	__gl_api.GetString = dlsym(h, "glGetString");
	if(NULL == __gl_api.GetString) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetString", dlerror());
		abort();
	}
	__gl_api.GetTexEnvfv = dlsym(h, "glGetTexEnvfv");
	if(NULL == __gl_api.GetTexEnvfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexEnvfv", dlerror());
		abort();
	}
	__gl_api.GetTexEnviv = dlsym(h, "glGetTexEnviv");
	if(NULL == __gl_api.GetTexEnviv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexEnviv", dlerror());
		abort();
	}
	__gl_api.GetTexGendv = dlsym(h, "glGetTexGendv");
	if(NULL == __gl_api.GetTexGendv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexGendv", dlerror());
		abort();
	}
	__gl_api.GetTexGenfv = dlsym(h, "glGetTexGenfv");
	if(NULL == __gl_api.GetTexGenfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexGenfv", dlerror());
		abort();
	}
	__gl_api.GetTexGeniv = dlsym(h, "glGetTexGeniv");
	if(NULL == __gl_api.GetTexGeniv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexGeniv", dlerror());
		abort();
	}
	__gl_api.GetTexImage = dlsym(h, "glGetTexImage");
	if(NULL == __gl_api.GetTexImage) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexImage", dlerror());
		abort();
	}
	__gl_api.GetTexLevelParameterfv = dlsym(h, "glGetTexLevelParameterfv");
	if(NULL == __gl_api.GetTexLevelParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexLevelParameterfv", dlerror());
		abort();
	}
	__gl_api.GetTexLevelParameteriv = dlsym(h, "glGetTexLevelParameteriv");
	if(NULL == __gl_api.GetTexLevelParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexLevelParameteriv", dlerror());
		abort();
	}
	__gl_api.GetTexParameterfv = dlsym(h, "glGetTexParameterfv");
	if(NULL == __gl_api.GetTexParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexParameterfv", dlerror());
		abort();
	}
	__gl_api.GetTexParameteriv = dlsym(h, "glGetTexParameteriv");
	if(NULL == __gl_api.GetTexParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetTexParameteriv", dlerror());
		abort();
	}
	__gl_api.Hint = dlsym(h, "glHint");
	if(NULL == __gl_api.Hint) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glHint", dlerror());
		abort();
	}
	__gl_api.Histogram = dlsym(h, "glHistogram");
	if(NULL == __gl_api.Histogram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glHistogram", dlerror());
		abort();
	}
	__gl_api.IndexMask = dlsym(h, "glIndexMask");
	if(NULL == __gl_api.IndexMask) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexMask", dlerror());
		abort();
	}
	__gl_api.IndexPointer = dlsym(h, "glIndexPointer");
	if(NULL == __gl_api.IndexPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexPointer", dlerror());
		abort();
	}
	__gl_api.Indexd = dlsym(h, "glIndexd");
	if(NULL == __gl_api.Indexd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexd", dlerror());
		abort();
	}
	__gl_api.Indexdv = dlsym(h, "glIndexdv");
	if(NULL == __gl_api.Indexdv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexdv", dlerror());
		abort();
	}
	__gl_api.Indexf = dlsym(h, "glIndexf");
	if(NULL == __gl_api.Indexf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexf", dlerror());
		abort();
	}
	__gl_api.Indexfv = dlsym(h, "glIndexfv");
	if(NULL == __gl_api.Indexfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexfv", dlerror());
		abort();
	}
	__gl_api.Indexi = dlsym(h, "glIndexi");
	if(NULL == __gl_api.Indexi) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexi", dlerror());
		abort();
	}
	__gl_api.Indexiv = dlsym(h, "glIndexiv");
	if(NULL == __gl_api.Indexiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexiv", dlerror());
		abort();
	}
	__gl_api.Indexs = dlsym(h, "glIndexs");
	if(NULL == __gl_api.Indexs) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexs", dlerror());
		abort();
	}
	__gl_api.Indexsv = dlsym(h, "glIndexsv");
	if(NULL == __gl_api.Indexsv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexsv", dlerror());
		abort();
	}
	__gl_api.Indexub = dlsym(h, "glIndexub");
	if(NULL == __gl_api.Indexub) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexub", dlerror());
		abort();
	}
	__gl_api.Indexubv = dlsym(h, "glIndexubv");
	if(NULL == __gl_api.Indexubv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIndexubv", dlerror());
		abort();
	}
	__gl_api.InitNames = dlsym(h, "glInitNames");
	if(NULL == __gl_api.InitNames) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glInitNames", dlerror());
		abort();
	}
	__gl_api.InterleavedArrays = dlsym(h, "glInterleavedArrays");
	if(NULL == __gl_api.InterleavedArrays) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glInterleavedArrays", dlerror());
		abort();
	}
	__gl_api.IsEnabled = dlsym(h, "glIsEnabled");
	if(NULL == __gl_api.IsEnabled) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsEnabled", dlerror());
		abort();
	}
	__gl_api.IsList = dlsym(h, "glIsList");
	if(NULL == __gl_api.IsList) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsList", dlerror());
		abort();
	}
	__gl_api.IsTexture = dlsym(h, "glIsTexture");
	if(NULL == __gl_api.IsTexture) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsTexture", dlerror());
		abort();
	}
	__gl_api.LightModelf = dlsym(h, "glLightModelf");
	if(NULL == __gl_api.LightModelf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightModelf", dlerror());
		abort();
	}
	__gl_api.LightModelfv = dlsym(h, "glLightModelfv");
	if(NULL == __gl_api.LightModelfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightModelfv", dlerror());
		abort();
	}
	__gl_api.LightModeli = dlsym(h, "glLightModeli");
	if(NULL == __gl_api.LightModeli) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightModeli", dlerror());
		abort();
	}
	__gl_api.LightModeliv = dlsym(h, "glLightModeliv");
	if(NULL == __gl_api.LightModeliv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightModeliv", dlerror());
		abort();
	}
	__gl_api.Lightf = dlsym(h, "glLightf");
	if(NULL == __gl_api.Lightf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightf", dlerror());
		abort();
	}
	__gl_api.Lightfv = dlsym(h, "glLightfv");
	if(NULL == __gl_api.Lightfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightfv", dlerror());
		abort();
	}
	__gl_api.Lighti = dlsym(h, "glLighti");
	if(NULL == __gl_api.Lighti) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLighti", dlerror());
		abort();
	}
	__gl_api.Lightiv = dlsym(h, "glLightiv");
	if(NULL == __gl_api.Lightiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLightiv", dlerror());
		abort();
	}
	__gl_api.LineStipple = dlsym(h, "glLineStipple");
	if(NULL == __gl_api.LineStipple) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLineStipple", dlerror());
		abort();
	}
	__gl_api.LineWidth = dlsym(h, "glLineWidth");
	if(NULL == __gl_api.LineWidth) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLineWidth", dlerror());
		abort();
	}
	__gl_api.ListBase = dlsym(h, "glListBase");
	if(NULL == __gl_api.ListBase) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glListBase", dlerror());
		abort();
	}
	__gl_api.LoadIdentity = dlsym(h, "glLoadIdentity");
	if(NULL == __gl_api.LoadIdentity) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLoadIdentity", dlerror());
		abort();
	}
	__gl_api.LoadMatrixd = dlsym(h, "glLoadMatrixd");
	if(NULL == __gl_api.LoadMatrixd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLoadMatrixd", dlerror());
		abort();
	}
	__gl_api.LoadMatrixf = dlsym(h, "glLoadMatrixf");
	if(NULL == __gl_api.LoadMatrixf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLoadMatrixf", dlerror());
		abort();
	}
	__gl_api.LoadName = dlsym(h, "glLoadName");
	if(NULL == __gl_api.LoadName) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLoadName", dlerror());
		abort();
	}
	__gl_api.LogicOp = dlsym(h, "glLogicOp");
	if(NULL == __gl_api.LogicOp) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLogicOp", dlerror());
		abort();
	}
	__gl_api.Map1d = dlsym(h, "glMap1d");
	if(NULL == __gl_api.Map1d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMap1d", dlerror());
		abort();
	}
	__gl_api.Map1f = dlsym(h, "glMap1f");
	if(NULL == __gl_api.Map1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMap1f", dlerror());
		abort();
	}
	__gl_api.Map2d = dlsym(h, "glMap2d");
	if(NULL == __gl_api.Map2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMap2d", dlerror());
		abort();
	}
	__gl_api.Map2f = dlsym(h, "glMap2f");
	if(NULL == __gl_api.Map2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMap2f", dlerror());
		abort();
	}
	__gl_api.MapGrid1d = dlsym(h, "glMapGrid1d");
	if(NULL == __gl_api.MapGrid1d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMapGrid1d", dlerror());
		abort();
	}
	__gl_api.MapGrid1f = dlsym(h, "glMapGrid1f");
	if(NULL == __gl_api.MapGrid1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMapGrid1f", dlerror());
		abort();
	}
	__gl_api.MapGrid2d = dlsym(h, "glMapGrid2d");
	if(NULL == __gl_api.MapGrid2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMapGrid2d", dlerror());
		abort();
	}
	__gl_api.MapGrid2f = dlsym(h, "glMapGrid2f");
	if(NULL == __gl_api.MapGrid2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMapGrid2f", dlerror());
		abort();
	}
	__gl_api.Materialf = dlsym(h, "glMaterialf");
	if(NULL == __gl_api.Materialf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMaterialf", dlerror());
		abort();
	}
	__gl_api.Materialfv = dlsym(h, "glMaterialfv");
	if(NULL == __gl_api.Materialfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMaterialfv", dlerror());
		abort();
	}
	__gl_api.Materiali = dlsym(h, "glMateriali");
	if(NULL == __gl_api.Materiali) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMateriali", dlerror());
		abort();
	}
	__gl_api.Materialiv = dlsym(h, "glMaterialiv");
	if(NULL == __gl_api.Materialiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMaterialiv", dlerror());
		abort();
	}
	__gl_api.MatrixMode = dlsym(h, "glMatrixMode");
	if(NULL == __gl_api.MatrixMode) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMatrixMode", dlerror());
		abort();
	}
	__gl_api.Minmax = dlsym(h, "glMinmax");
	if(NULL == __gl_api.Minmax) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMinmax", dlerror());
		abort();
	}
	__gl_api.MultMatrixd = dlsym(h, "glMultMatrixd");
	if(NULL == __gl_api.MultMatrixd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultMatrixd", dlerror());
		abort();
	}
	__gl_api.MultMatrixf = dlsym(h, "glMultMatrixf");
	if(NULL == __gl_api.MultMatrixf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultMatrixf", dlerror());
		abort();
	}
	__gl_api.NewList = dlsym(h, "glNewList");
	if(NULL == __gl_api.NewList) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNewList", dlerror());
		abort();
	}
	__gl_api.Normal3b = dlsym(h, "glNormal3b");
	if(NULL == __gl_api.Normal3b) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3b", dlerror());
		abort();
	}
	__gl_api.Normal3bv = dlsym(h, "glNormal3bv");
	if(NULL == __gl_api.Normal3bv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3bv", dlerror());
		abort();
	}
	__gl_api.Normal3d = dlsym(h, "glNormal3d");
	if(NULL == __gl_api.Normal3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3d", dlerror());
		abort();
	}
	__gl_api.Normal3dv = dlsym(h, "glNormal3dv");
	if(NULL == __gl_api.Normal3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3dv", dlerror());
		abort();
	}
	__gl_api.Normal3f = dlsym(h, "glNormal3f");
	if(NULL == __gl_api.Normal3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3f", dlerror());
		abort();
	}
	__gl_api.Normal3fv = dlsym(h, "glNormal3fv");
	if(NULL == __gl_api.Normal3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3fv", dlerror());
		abort();
	}
	__gl_api.Normal3i = dlsym(h, "glNormal3i");
	if(NULL == __gl_api.Normal3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3i", dlerror());
		abort();
	}
	__gl_api.Normal3iv = dlsym(h, "glNormal3iv");
	if(NULL == __gl_api.Normal3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3iv", dlerror());
		abort();
	}
	__gl_api.Normal3s = dlsym(h, "glNormal3s");
	if(NULL == __gl_api.Normal3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3s", dlerror());
		abort();
	}
	__gl_api.Normal3sv = dlsym(h, "glNormal3sv");
	if(NULL == __gl_api.Normal3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormal3sv", dlerror());
		abort();
	}
	__gl_api.NormalPointer = dlsym(h, "glNormalPointer");
	if(NULL == __gl_api.NormalPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glNormalPointer", dlerror());
		abort();
	}
	__gl_api.Ortho = dlsym(h, "glOrtho");
	if(NULL == __gl_api.Ortho) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glOrtho", dlerror());
		abort();
	}
	__gl_api.PassThrough = dlsym(h, "glPassThrough");
	if(NULL == __gl_api.PassThrough) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPassThrough", dlerror());
		abort();
	}
	__gl_api.PixelMapfv = dlsym(h, "glPixelMapfv");
	if(NULL == __gl_api.PixelMapfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelMapfv", dlerror());
		abort();
	}
	__gl_api.PixelMapuiv = dlsym(h, "glPixelMapuiv");
	if(NULL == __gl_api.PixelMapuiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelMapuiv", dlerror());
		abort();
	}
	__gl_api.PixelMapusv = dlsym(h, "glPixelMapusv");
	if(NULL == __gl_api.PixelMapusv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelMapusv", dlerror());
		abort();
	}
	__gl_api.PixelStoref = dlsym(h, "glPixelStoref");
	if(NULL == __gl_api.PixelStoref) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelStoref", dlerror());
		abort();
	}
	__gl_api.PixelStorei = dlsym(h, "glPixelStorei");
	if(NULL == __gl_api.PixelStorei) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelStorei", dlerror());
		abort();
	}
	__gl_api.PixelTransferf = dlsym(h, "glPixelTransferf");
	if(NULL == __gl_api.PixelTransferf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelTransferf", dlerror());
		abort();
	}
	__gl_api.PixelTransferi = dlsym(h, "glPixelTransferi");
	if(NULL == __gl_api.PixelTransferi) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelTransferi", dlerror());
		abort();
	}
	__gl_api.PixelZoom = dlsym(h, "glPixelZoom");
	if(NULL == __gl_api.PixelZoom) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPixelZoom", dlerror());
		abort();
	}
	__gl_api.PointSize = dlsym(h, "glPointSize");
	if(NULL == __gl_api.PointSize) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPointSize", dlerror());
		abort();
	}
	__gl_api.PolygonMode = dlsym(h, "glPolygonMode");
	if(NULL == __gl_api.PolygonMode) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPolygonMode", dlerror());
		abort();
	}
	__gl_api.PolygonOffset = dlsym(h, "glPolygonOffset");
	if(NULL == __gl_api.PolygonOffset) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPolygonOffset", dlerror());
		abort();
	}
	__gl_api.PolygonStipple = dlsym(h, "glPolygonStipple");
	if(NULL == __gl_api.PolygonStipple) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPolygonStipple", dlerror());
		abort();
	}
	__gl_api.PopAttrib = dlsym(h, "glPopAttrib");
	if(NULL == __gl_api.PopAttrib) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPopAttrib", dlerror());
		abort();
	}
	__gl_api.PopClientAttrib = dlsym(h, "glPopClientAttrib");
	if(NULL == __gl_api.PopClientAttrib) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPopClientAttrib", dlerror());
		abort();
	}
	__gl_api.PopMatrix = dlsym(h, "glPopMatrix");
	if(NULL == __gl_api.PopMatrix) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPopMatrix", dlerror());
		abort();
	}
	__gl_api.PopName = dlsym(h, "glPopName");
	if(NULL == __gl_api.PopName) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPopName", dlerror());
		abort();
	}
	__gl_api.PrioritizeTextures = dlsym(h, "glPrioritizeTextures");
	if(NULL == __gl_api.PrioritizeTextures) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPrioritizeTextures", dlerror());
		abort();
	}
	__gl_api.PushAttrib = dlsym(h, "glPushAttrib");
	if(NULL == __gl_api.PushAttrib) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPushAttrib", dlerror());
		abort();
	}
	__gl_api.PushClientAttrib = dlsym(h, "glPushClientAttrib");
	if(NULL == __gl_api.PushClientAttrib) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPushClientAttrib", dlerror());
		abort();
	}
	__gl_api.PushMatrix = dlsym(h, "glPushMatrix");
	if(NULL == __gl_api.PushMatrix) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPushMatrix", dlerror());
		abort();
	}
	__gl_api.PushName = dlsym(h, "glPushName");
	if(NULL == __gl_api.PushName) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPushName", dlerror());
		abort();
	}
	__gl_api.RasterPos2d = dlsym(h, "glRasterPos2d");
	if(NULL == __gl_api.RasterPos2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2d", dlerror());
		abort();
	}
	__gl_api.RasterPos2dv = dlsym(h, "glRasterPos2dv");
	if(NULL == __gl_api.RasterPos2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2dv", dlerror());
		abort();
	}
	__gl_api.RasterPos2f = dlsym(h, "glRasterPos2f");
	if(NULL == __gl_api.RasterPos2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2f", dlerror());
		abort();
	}
	__gl_api.RasterPos2fv = dlsym(h, "glRasterPos2fv");
	if(NULL == __gl_api.RasterPos2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2fv", dlerror());
		abort();
	}
	__gl_api.RasterPos2i = dlsym(h, "glRasterPos2i");
	if(NULL == __gl_api.RasterPos2i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2i", dlerror());
		abort();
	}
	__gl_api.RasterPos2iv = dlsym(h, "glRasterPos2iv");
	if(NULL == __gl_api.RasterPos2iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2iv", dlerror());
		abort();
	}
	__gl_api.RasterPos2s = dlsym(h, "glRasterPos2s");
	if(NULL == __gl_api.RasterPos2s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2s", dlerror());
		abort();
	}
	__gl_api.RasterPos2sv = dlsym(h, "glRasterPos2sv");
	if(NULL == __gl_api.RasterPos2sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos2sv", dlerror());
		abort();
	}
	__gl_api.RasterPos3d = dlsym(h, "glRasterPos3d");
	if(NULL == __gl_api.RasterPos3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3d", dlerror());
		abort();
	}
	__gl_api.RasterPos3dv = dlsym(h, "glRasterPos3dv");
	if(NULL == __gl_api.RasterPos3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3dv", dlerror());
		abort();
	}
	__gl_api.RasterPos3f = dlsym(h, "glRasterPos3f");
	if(NULL == __gl_api.RasterPos3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3f", dlerror());
		abort();
	}
	__gl_api.RasterPos3fv = dlsym(h, "glRasterPos3fv");
	if(NULL == __gl_api.RasterPos3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3fv", dlerror());
		abort();
	}
	__gl_api.RasterPos3i = dlsym(h, "glRasterPos3i");
	if(NULL == __gl_api.RasterPos3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3i", dlerror());
		abort();
	}
	__gl_api.RasterPos3iv = dlsym(h, "glRasterPos3iv");
	if(NULL == __gl_api.RasterPos3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3iv", dlerror());
		abort();
	}
	__gl_api.RasterPos3s = dlsym(h, "glRasterPos3s");
	if(NULL == __gl_api.RasterPos3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3s", dlerror());
		abort();
	}
	__gl_api.RasterPos3sv = dlsym(h, "glRasterPos3sv");
	if(NULL == __gl_api.RasterPos3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos3sv", dlerror());
		abort();
	}
	__gl_api.RasterPos4d = dlsym(h, "glRasterPos4d");
	if(NULL == __gl_api.RasterPos4d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4d", dlerror());
		abort();
	}
	__gl_api.RasterPos4dv = dlsym(h, "glRasterPos4dv");
	if(NULL == __gl_api.RasterPos4dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4dv", dlerror());
		abort();
	}
	__gl_api.RasterPos4f = dlsym(h, "glRasterPos4f");
	if(NULL == __gl_api.RasterPos4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4f", dlerror());
		abort();
	}
	__gl_api.RasterPos4fv = dlsym(h, "glRasterPos4fv");
	if(NULL == __gl_api.RasterPos4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4fv", dlerror());
		abort();
	}
	__gl_api.RasterPos4i = dlsym(h, "glRasterPos4i");
	if(NULL == __gl_api.RasterPos4i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4i", dlerror());
		abort();
	}
	__gl_api.RasterPos4iv = dlsym(h, "glRasterPos4iv");
	if(NULL == __gl_api.RasterPos4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4iv", dlerror());
		abort();
	}
	__gl_api.RasterPos4s = dlsym(h, "glRasterPos4s");
	if(NULL == __gl_api.RasterPos4s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4s", dlerror());
		abort();
	}
	__gl_api.RasterPos4sv = dlsym(h, "glRasterPos4sv");
	if(NULL == __gl_api.RasterPos4sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRasterPos4sv", dlerror());
		abort();
	}
	__gl_api.ReadBuffer = dlsym(h, "glReadBuffer");
	if(NULL == __gl_api.ReadBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glReadBuffer", dlerror());
		abort();
	}
	__gl_api.ReadPixels = dlsym(h, "glReadPixels");
	if(NULL == __gl_api.ReadPixels) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glReadPixels", dlerror());
		abort();
	}
	__gl_api.Rectd = dlsym(h, "glRectd");
	if(NULL == __gl_api.Rectd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRectd", dlerror());
		abort();
	}
	__gl_api.Rectdv = dlsym(h, "glRectdv");
	if(NULL == __gl_api.Rectdv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRectdv", dlerror());
		abort();
	}
	__gl_api.Rectf = dlsym(h, "glRectf");
	if(NULL == __gl_api.Rectf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRectf", dlerror());
		abort();
	}
	__gl_api.Rectfv = dlsym(h, "glRectfv");
	if(NULL == __gl_api.Rectfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRectfv", dlerror());
		abort();
	}
	__gl_api.Recti = dlsym(h, "glRecti");
	if(NULL == __gl_api.Recti) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRecti", dlerror());
		abort();
	}
	__gl_api.Rectiv = dlsym(h, "glRectiv");
	if(NULL == __gl_api.Rectiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRectiv", dlerror());
		abort();
	}
	__gl_api.Rects = dlsym(h, "glRects");
	if(NULL == __gl_api.Rects) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRects", dlerror());
		abort();
	}
	__gl_api.Rectsv = dlsym(h, "glRectsv");
	if(NULL == __gl_api.Rectsv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRectsv", dlerror());
		abort();
	}
	__gl_api.RenderMode = dlsym(h, "glRenderMode");
	if(NULL == __gl_api.RenderMode) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRenderMode", dlerror());
		abort();
	}
	__gl_api.ResetHistogram = dlsym(h, "glResetHistogram");
	if(NULL == __gl_api.ResetHistogram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glResetHistogram", dlerror());
		abort();
	}
	__gl_api.ResetMinmax = dlsym(h, "glResetMinmax");
	if(NULL == __gl_api.ResetMinmax) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glResetMinmax", dlerror());
		abort();
	}
	__gl_api.Rotated = dlsym(h, "glRotated");
	if(NULL == __gl_api.Rotated) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRotated", dlerror());
		abort();
	}
	__gl_api.Rotatef = dlsym(h, "glRotatef");
	if(NULL == __gl_api.Rotatef) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glRotatef", dlerror());
		abort();
	}
	__gl_api.Scaled = dlsym(h, "glScaled");
	if(NULL == __gl_api.Scaled) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glScaled", dlerror());
		abort();
	}
	__gl_api.Scalef = dlsym(h, "glScalef");
	if(NULL == __gl_api.Scalef) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glScalef", dlerror());
		abort();
	}
	__gl_api.Scissor = dlsym(h, "glScissor");
	if(NULL == __gl_api.Scissor) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glScissor", dlerror());
		abort();
	}
	__gl_api.SelectBuffer = dlsym(h, "glSelectBuffer");
	if(NULL == __gl_api.SelectBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSelectBuffer", dlerror());
		abort();
	}
	__gl_api.SeparableFilter2D = dlsym(h, "glSeparableFilter2D");
	if(NULL == __gl_api.SeparableFilter2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSeparableFilter2D", dlerror());
		abort();
	}
	__gl_api.ShadeModel = dlsym(h, "glShadeModel");
	if(NULL == __gl_api.ShadeModel) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glShadeModel", dlerror());
		abort();
	}
	__gl_api.StencilFunc = dlsym(h, "glStencilFunc");
	if(NULL == __gl_api.StencilFunc) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glStencilFunc", dlerror());
		abort();
	}
	__gl_api.StencilMask = dlsym(h, "glStencilMask");
	if(NULL == __gl_api.StencilMask) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glStencilMask", dlerror());
		abort();
	}
	__gl_api.StencilOp = dlsym(h, "glStencilOp");
	if(NULL == __gl_api.StencilOp) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glStencilOp", dlerror());
		abort();
	}
	__gl_api.TexCoord1d = dlsym(h, "glTexCoord1d");
	if(NULL == __gl_api.TexCoord1d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1d", dlerror());
		abort();
	}
	__gl_api.TexCoord1dv = dlsym(h, "glTexCoord1dv");
	if(NULL == __gl_api.TexCoord1dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1dv", dlerror());
		abort();
	}
	__gl_api.TexCoord1f = dlsym(h, "glTexCoord1f");
	if(NULL == __gl_api.TexCoord1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1f", dlerror());
		abort();
	}
	__gl_api.TexCoord1fv = dlsym(h, "glTexCoord1fv");
	if(NULL == __gl_api.TexCoord1fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1fv", dlerror());
		abort();
	}
	__gl_api.TexCoord1i = dlsym(h, "glTexCoord1i");
	if(NULL == __gl_api.TexCoord1i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1i", dlerror());
		abort();
	}
	__gl_api.TexCoord1iv = dlsym(h, "glTexCoord1iv");
	if(NULL == __gl_api.TexCoord1iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1iv", dlerror());
		abort();
	}
	__gl_api.TexCoord1s = dlsym(h, "glTexCoord1s");
	if(NULL == __gl_api.TexCoord1s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1s", dlerror());
		abort();
	}
	__gl_api.TexCoord1sv = dlsym(h, "glTexCoord1sv");
	if(NULL == __gl_api.TexCoord1sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord1sv", dlerror());
		abort();
	}
	__gl_api.TexCoord2d = dlsym(h, "glTexCoord2d");
	if(NULL == __gl_api.TexCoord2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2d", dlerror());
		abort();
	}
	__gl_api.TexCoord2dv = dlsym(h, "glTexCoord2dv");
	if(NULL == __gl_api.TexCoord2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2dv", dlerror());
		abort();
	}
	__gl_api.TexCoord2f = dlsym(h, "glTexCoord2f");
	if(NULL == __gl_api.TexCoord2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2f", dlerror());
		abort();
	}
	__gl_api.TexCoord2fv = dlsym(h, "glTexCoord2fv");
	if(NULL == __gl_api.TexCoord2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2fv", dlerror());
		abort();
	}
	__gl_api.TexCoord2i = dlsym(h, "glTexCoord2i");
	if(NULL == __gl_api.TexCoord2i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2i", dlerror());
		abort();
	}
	__gl_api.TexCoord2iv = dlsym(h, "glTexCoord2iv");
	if(NULL == __gl_api.TexCoord2iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2iv", dlerror());
		abort();
	}
	__gl_api.TexCoord2s = dlsym(h, "glTexCoord2s");
	if(NULL == __gl_api.TexCoord2s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2s", dlerror());
		abort();
	}
	__gl_api.TexCoord2sv = dlsym(h, "glTexCoord2sv");
	if(NULL == __gl_api.TexCoord2sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord2sv", dlerror());
		abort();
	}
	__gl_api.TexCoord3d = dlsym(h, "glTexCoord3d");
	if(NULL == __gl_api.TexCoord3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3d", dlerror());
		abort();
	}
	__gl_api.TexCoord3dv = dlsym(h, "glTexCoord3dv");
	if(NULL == __gl_api.TexCoord3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3dv", dlerror());
		abort();
	}
	__gl_api.TexCoord3f = dlsym(h, "glTexCoord3f");
	if(NULL == __gl_api.TexCoord3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3f", dlerror());
		abort();
	}
	__gl_api.TexCoord3fv = dlsym(h, "glTexCoord3fv");
	if(NULL == __gl_api.TexCoord3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3fv", dlerror());
		abort();
	}
	__gl_api.TexCoord3i = dlsym(h, "glTexCoord3i");
	if(NULL == __gl_api.TexCoord3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3i", dlerror());
		abort();
	}
	__gl_api.TexCoord3iv = dlsym(h, "glTexCoord3iv");
	if(NULL == __gl_api.TexCoord3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3iv", dlerror());
		abort();
	}
	__gl_api.TexCoord3s = dlsym(h, "glTexCoord3s");
	if(NULL == __gl_api.TexCoord3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3s", dlerror());
		abort();
	}
	__gl_api.TexCoord3sv = dlsym(h, "glTexCoord3sv");
	if(NULL == __gl_api.TexCoord3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord3sv", dlerror());
		abort();
	}
	__gl_api.TexCoord4d = dlsym(h, "glTexCoord4d");
	if(NULL == __gl_api.TexCoord4d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4d", dlerror());
		abort();
	}
	__gl_api.TexCoord4dv = dlsym(h, "glTexCoord4dv");
	if(NULL == __gl_api.TexCoord4dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4dv", dlerror());
		abort();
	}
	__gl_api.TexCoord4f = dlsym(h, "glTexCoord4f");
	if(NULL == __gl_api.TexCoord4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4f", dlerror());
		abort();
	}
	__gl_api.TexCoord4fv = dlsym(h, "glTexCoord4fv");
	if(NULL == __gl_api.TexCoord4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4fv", dlerror());
		abort();
	}
	__gl_api.TexCoord4i = dlsym(h, "glTexCoord4i");
	if(NULL == __gl_api.TexCoord4i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4i", dlerror());
		abort();
	}
	__gl_api.TexCoord4iv = dlsym(h, "glTexCoord4iv");
	if(NULL == __gl_api.TexCoord4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4iv", dlerror());
		abort();
	}
	__gl_api.TexCoord4s = dlsym(h, "glTexCoord4s");
	if(NULL == __gl_api.TexCoord4s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4s", dlerror());
		abort();
	}
	__gl_api.TexCoord4sv = dlsym(h, "glTexCoord4sv");
	if(NULL == __gl_api.TexCoord4sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoord4sv", dlerror());
		abort();
	}
	__gl_api.TexCoordPointer = dlsym(h, "glTexCoordPointer");
	if(NULL == __gl_api.TexCoordPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexCoordPointer", dlerror());
		abort();
	}
	__gl_api.TexEnvf = dlsym(h, "glTexEnvf");
	if(NULL == __gl_api.TexEnvf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexEnvf", dlerror());
		abort();
	}
	__gl_api.TexEnvfv = dlsym(h, "glTexEnvfv");
	if(NULL == __gl_api.TexEnvfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexEnvfv", dlerror());
		abort();
	}
	__gl_api.TexEnvi = dlsym(h, "glTexEnvi");
	if(NULL == __gl_api.TexEnvi) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexEnvi", dlerror());
		abort();
	}
	__gl_api.TexEnviv = dlsym(h, "glTexEnviv");
	if(NULL == __gl_api.TexEnviv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexEnviv", dlerror());
		abort();
	}
	__gl_api.TexGend = dlsym(h, "glTexGend");
	if(NULL == __gl_api.TexGend) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexGend", dlerror());
		abort();
	}
	__gl_api.TexGendv = dlsym(h, "glTexGendv");
	if(NULL == __gl_api.TexGendv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexGendv", dlerror());
		abort();
	}
	__gl_api.TexGenf = dlsym(h, "glTexGenf");
	if(NULL == __gl_api.TexGenf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexGenf", dlerror());
		abort();
	}
	__gl_api.TexGenfv = dlsym(h, "glTexGenfv");
	if(NULL == __gl_api.TexGenfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexGenfv", dlerror());
		abort();
	}
	__gl_api.TexGeni = dlsym(h, "glTexGeni");
	if(NULL == __gl_api.TexGeni) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexGeni", dlerror());
		abort();
	}
	__gl_api.TexGeniv = dlsym(h, "glTexGeniv");
	if(NULL == __gl_api.TexGeniv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexGeniv", dlerror());
		abort();
	}
	__gl_api.TexImage1D = dlsym(h, "glTexImage1D");
	if(NULL == __gl_api.TexImage1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexImage1D", dlerror());
		abort();
	}
	__gl_api.TexImage2D = dlsym(h, "glTexImage2D");
	if(NULL == __gl_api.TexImage2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexImage2D", dlerror());
		abort();
	}
	__gl_api.TexImage3D = dlsym(h, "glTexImage3D");
	if(NULL == __gl_api.TexImage3D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexImage3D", dlerror());
		abort();
	}
	__gl_api.TexParameterf = dlsym(h, "glTexParameterf");
	if(NULL == __gl_api.TexParameterf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexParameterf", dlerror());
		abort();
	}
	__gl_api.TexParameterfv = dlsym(h, "glTexParameterfv");
	if(NULL == __gl_api.TexParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexParameterfv", dlerror());
		abort();
	}
	__gl_api.TexParameteri = dlsym(h, "glTexParameteri");
	if(NULL == __gl_api.TexParameteri) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexParameteri", dlerror());
		abort();
	}
	__gl_api.TexParameteriv = dlsym(h, "glTexParameteriv");
	if(NULL == __gl_api.TexParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexParameteriv", dlerror());
		abort();
	}
	__gl_api.TexSubImage1D = dlsym(h, "glTexSubImage1D");
	if(NULL == __gl_api.TexSubImage1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexSubImage1D", dlerror());
		abort();
	}
	__gl_api.TexSubImage2D = dlsym(h, "glTexSubImage2D");
	if(NULL == __gl_api.TexSubImage2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexSubImage2D", dlerror());
		abort();
	}
	__gl_api.TexSubImage3D = dlsym(h, "glTexSubImage3D");
	if(NULL == __gl_api.TexSubImage3D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTexSubImage3D", dlerror());
		abort();
	}
	__gl_api.Translated = dlsym(h, "glTranslated");
	if(NULL == __gl_api.Translated) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTranslated", dlerror());
		abort();
	}
	__gl_api.Translatef = dlsym(h, "glTranslatef");
	if(NULL == __gl_api.Translatef) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glTranslatef", dlerror());
		abort();
	}
	__gl_api.Vertex2d = dlsym(h, "glVertex2d");
	if(NULL == __gl_api.Vertex2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2d", dlerror());
		abort();
	}
	__gl_api.Vertex2dv = dlsym(h, "glVertex2dv");
	if(NULL == __gl_api.Vertex2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2dv", dlerror());
		abort();
	}
	__gl_api.Vertex2f = dlsym(h, "glVertex2f");
	if(NULL == __gl_api.Vertex2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2f", dlerror());
		abort();
	}
	__gl_api.Vertex2fv = dlsym(h, "glVertex2fv");
	if(NULL == __gl_api.Vertex2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2fv", dlerror());
		abort();
	}
	__gl_api.Vertex2i = dlsym(h, "glVertex2i");
	if(NULL == __gl_api.Vertex2i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2i", dlerror());
		abort();
	}
	__gl_api.Vertex2iv = dlsym(h, "glVertex2iv");
	if(NULL == __gl_api.Vertex2iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2iv", dlerror());
		abort();
	}
	__gl_api.Vertex2s = dlsym(h, "glVertex2s");
	if(NULL == __gl_api.Vertex2s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2s", dlerror());
		abort();
	}
	__gl_api.Vertex2sv = dlsym(h, "glVertex2sv");
	if(NULL == __gl_api.Vertex2sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex2sv", dlerror());
		abort();
	}
	__gl_api.Vertex3d = dlsym(h, "glVertex3d");
	if(NULL == __gl_api.Vertex3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3d", dlerror());
		abort();
	}
	__gl_api.Vertex3dv = dlsym(h, "glVertex3dv");
	if(NULL == __gl_api.Vertex3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3dv", dlerror());
		abort();
	}
	__gl_api.Vertex3f = dlsym(h, "glVertex3f");
	if(NULL == __gl_api.Vertex3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3f", dlerror());
		abort();
	}
	__gl_api.Vertex3fv = dlsym(h, "glVertex3fv");
	if(NULL == __gl_api.Vertex3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3fv", dlerror());
		abort();
	}
	__gl_api.Vertex3i = dlsym(h, "glVertex3i");
	if(NULL == __gl_api.Vertex3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3i", dlerror());
		abort();
	}
	__gl_api.Vertex3iv = dlsym(h, "glVertex3iv");
	if(NULL == __gl_api.Vertex3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3iv", dlerror());
		abort();
	}
	__gl_api.Vertex3s = dlsym(h, "glVertex3s");
	if(NULL == __gl_api.Vertex3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3s", dlerror());
		abort();
	}
	__gl_api.Vertex3sv = dlsym(h, "glVertex3sv");
	if(NULL == __gl_api.Vertex3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex3sv", dlerror());
		abort();
	}
	__gl_api.Vertex4d = dlsym(h, "glVertex4d");
	if(NULL == __gl_api.Vertex4d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4d", dlerror());
		abort();
	}
	__gl_api.Vertex4dv = dlsym(h, "glVertex4dv");
	if(NULL == __gl_api.Vertex4dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4dv", dlerror());
		abort();
	}
	__gl_api.Vertex4f = dlsym(h, "glVertex4f");
	if(NULL == __gl_api.Vertex4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4f", dlerror());
		abort();
	}
	__gl_api.Vertex4fv = dlsym(h, "glVertex4fv");
	if(NULL == __gl_api.Vertex4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4fv", dlerror());
		abort();
	}
	__gl_api.Vertex4i = dlsym(h, "glVertex4i");
	if(NULL == __gl_api.Vertex4i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4i", dlerror());
		abort();
	}
	__gl_api.Vertex4iv = dlsym(h, "glVertex4iv");
	if(NULL == __gl_api.Vertex4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4iv", dlerror());
		abort();
	}
	__gl_api.Vertex4s = dlsym(h, "glVertex4s");
	if(NULL == __gl_api.Vertex4s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4s", dlerror());
		abort();
	}
	__gl_api.Vertex4sv = dlsym(h, "glVertex4sv");
	if(NULL == __gl_api.Vertex4sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertex4sv", dlerror());
		abort();
	}
	__gl_api.VertexPointer = dlsym(h, "glVertexPointer");
	if(NULL == __gl_api.VertexPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexPointer", dlerror());
		abort();
	}
	__gl_api.Viewport = dlsym(h, "glViewport");
	if(NULL == __gl_api.Viewport) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glViewport", dlerror());
		abort();
	}
	__gl_api.SampleCoverage = dlsym(h, "glSampleCoverage");
	if(NULL == __gl_api.SampleCoverage) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSampleCoverage", dlerror());
		abort();
	}
	__gl_api.SamplePass = dlsym(h, "glSamplePass");
	if(NULL == __gl_api.SamplePass) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSamplePass", dlerror());
		abort();
	}
	__gl_api.LoadTransposeMatrixf = dlsym(h, "glLoadTransposeMatrixf");
	if(NULL == __gl_api.LoadTransposeMatrixf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLoadTransposeMatrixf", dlerror());
		abort();
	}
	__gl_api.LoadTransposeMatrixd = dlsym(h, "glLoadTransposeMatrixd");
	if(NULL == __gl_api.LoadTransposeMatrixd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLoadTransposeMatrixd", dlerror());
		abort();
	}
	__gl_api.MultTransposeMatrixf = dlsym(h, "glMultTransposeMatrixf");
	if(NULL == __gl_api.MultTransposeMatrixf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultTransposeMatrixf", dlerror());
		abort();
	}
	__gl_api.MultTransposeMatrixd = dlsym(h, "glMultTransposeMatrixd");
	if(NULL == __gl_api.MultTransposeMatrixd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultTransposeMatrixd", dlerror());
		abort();
	}
	__gl_api.CompressedTexImage3D = dlsym(h, "glCompressedTexImage3D");
	if(NULL == __gl_api.CompressedTexImage3D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompressedTexImage3D", dlerror());
		abort();
	}
	__gl_api.CompressedTexImage2D = dlsym(h, "glCompressedTexImage2D");
	if(NULL == __gl_api.CompressedTexImage2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompressedTexImage2D", dlerror());
		abort();
	}
	__gl_api.CompressedTexImage1D = dlsym(h, "glCompressedTexImage1D");
	if(NULL == __gl_api.CompressedTexImage1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompressedTexImage1D", dlerror());
		abort();
	}
	__gl_api.CompressedTexSubImage3D = dlsym(h, "glCompressedTexSubImage3D");
	if(NULL == __gl_api.CompressedTexSubImage3D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompressedTexSubImage3D", dlerror());
		abort();
	}
	__gl_api.CompressedTexSubImage2D = dlsym(h, "glCompressedTexSubImage2D");
	if(NULL == __gl_api.CompressedTexSubImage2D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompressedTexSubImage2D", dlerror());
		abort();
	}
	__gl_api.CompressedTexSubImage1D = dlsym(h, "glCompressedTexSubImage1D");
	if(NULL == __gl_api.CompressedTexSubImage1D) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompressedTexSubImage1D", dlerror());
		abort();
	}
	__gl_api.GetCompressedTexImage = dlsym(h, "glGetCompressedTexImage");
	if(NULL == __gl_api.GetCompressedTexImage) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetCompressedTexImage", dlerror());
		abort();
	}
	__gl_api.ActiveTexture = dlsym(h, "glActiveTexture");
	if(NULL == __gl_api.ActiveTexture) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glActiveTexture", dlerror());
		abort();
	}
	__gl_api.ClientActiveTexture = dlsym(h, "glClientActiveTexture");
	if(NULL == __gl_api.ClientActiveTexture) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glClientActiveTexture", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1d = dlsym(h, "glMultiTexCoord1d");
	if(NULL == __gl_api.MultiTexCoord1d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1d", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1dv = dlsym(h, "glMultiTexCoord1dv");
	if(NULL == __gl_api.MultiTexCoord1dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1dv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1f = dlsym(h, "glMultiTexCoord1f");
	if(NULL == __gl_api.MultiTexCoord1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1f", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1fv = dlsym(h, "glMultiTexCoord1fv");
	if(NULL == __gl_api.MultiTexCoord1fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1fv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1i = dlsym(h, "glMultiTexCoord1i");
	if(NULL == __gl_api.MultiTexCoord1i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1i", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1iv = dlsym(h, "glMultiTexCoord1iv");
	if(NULL == __gl_api.MultiTexCoord1iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1iv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1s = dlsym(h, "glMultiTexCoord1s");
	if(NULL == __gl_api.MultiTexCoord1s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1s", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord1sv = dlsym(h, "glMultiTexCoord1sv");
	if(NULL == __gl_api.MultiTexCoord1sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord1sv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2d = dlsym(h, "glMultiTexCoord2d");
	if(NULL == __gl_api.MultiTexCoord2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2d", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2dv = dlsym(h, "glMultiTexCoord2dv");
	if(NULL == __gl_api.MultiTexCoord2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2dv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2f = dlsym(h, "glMultiTexCoord2f");
	if(NULL == __gl_api.MultiTexCoord2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2f", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2fv = dlsym(h, "glMultiTexCoord2fv");
	if(NULL == __gl_api.MultiTexCoord2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2fv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2i = dlsym(h, "glMultiTexCoord2i");
	if(NULL == __gl_api.MultiTexCoord2i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2i", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2iv = dlsym(h, "glMultiTexCoord2iv");
	if(NULL == __gl_api.MultiTexCoord2iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2iv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2s = dlsym(h, "glMultiTexCoord2s");
	if(NULL == __gl_api.MultiTexCoord2s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2s", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord2sv = dlsym(h, "glMultiTexCoord2sv");
	if(NULL == __gl_api.MultiTexCoord2sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord2sv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3d = dlsym(h, "glMultiTexCoord3d");
	if(NULL == __gl_api.MultiTexCoord3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3d", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3dv = dlsym(h, "glMultiTexCoord3dv");
	if(NULL == __gl_api.MultiTexCoord3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3dv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3f = dlsym(h, "glMultiTexCoord3f");
	if(NULL == __gl_api.MultiTexCoord3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3f", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3fv = dlsym(h, "glMultiTexCoord3fv");
	if(NULL == __gl_api.MultiTexCoord3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3fv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3i = dlsym(h, "glMultiTexCoord3i");
	if(NULL == __gl_api.MultiTexCoord3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3i", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3iv = dlsym(h, "glMultiTexCoord3iv");
	if(NULL == __gl_api.MultiTexCoord3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3iv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3s = dlsym(h, "glMultiTexCoord3s");
	if(NULL == __gl_api.MultiTexCoord3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3s", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord3sv = dlsym(h, "glMultiTexCoord3sv");
	if(NULL == __gl_api.MultiTexCoord3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord3sv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4d = dlsym(h, "glMultiTexCoord4d");
	if(NULL == __gl_api.MultiTexCoord4d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4d", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4dv = dlsym(h, "glMultiTexCoord4dv");
	if(NULL == __gl_api.MultiTexCoord4dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4dv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4f = dlsym(h, "glMultiTexCoord4f");
	if(NULL == __gl_api.MultiTexCoord4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4f", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4fv = dlsym(h, "glMultiTexCoord4fv");
	if(NULL == __gl_api.MultiTexCoord4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4fv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4i = dlsym(h, "glMultiTexCoord4i");
	if(NULL == __gl_api.MultiTexCoord4i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4i", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4iv = dlsym(h, "glMultiTexCoord4iv");
	if(NULL == __gl_api.MultiTexCoord4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4iv", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4s = dlsym(h, "glMultiTexCoord4s");
	if(NULL == __gl_api.MultiTexCoord4s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4s", dlerror());
		abort();
	}
	__gl_api.MultiTexCoord4sv = dlsym(h, "glMultiTexCoord4sv");
	if(NULL == __gl_api.MultiTexCoord4sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiTexCoord4sv", dlerror());
		abort();
	}
	__gl_api.FogCoordf = dlsym(h, "glFogCoordf");
	if(NULL == __gl_api.FogCoordf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogCoordf", dlerror());
		abort();
	}
	__gl_api.FogCoordfv = dlsym(h, "glFogCoordfv");
	if(NULL == __gl_api.FogCoordfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogCoordfv", dlerror());
		abort();
	}
	__gl_api.FogCoordd = dlsym(h, "glFogCoordd");
	if(NULL == __gl_api.FogCoordd) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogCoordd", dlerror());
		abort();
	}
	__gl_api.FogCoorddv = dlsym(h, "glFogCoorddv");
	if(NULL == __gl_api.FogCoorddv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogCoorddv", dlerror());
		abort();
	}
	__gl_api.FogCoordPointer = dlsym(h, "glFogCoordPointer");
	if(NULL == __gl_api.FogCoordPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glFogCoordPointer", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3b = dlsym(h, "glSecondaryColor3b");
	if(NULL == __gl_api.SecondaryColor3b) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3b", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3bv = dlsym(h, "glSecondaryColor3bv");
	if(NULL == __gl_api.SecondaryColor3bv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3bv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3d = dlsym(h, "glSecondaryColor3d");
	if(NULL == __gl_api.SecondaryColor3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3d", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3dv = dlsym(h, "glSecondaryColor3dv");
	if(NULL == __gl_api.SecondaryColor3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3dv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3f = dlsym(h, "glSecondaryColor3f");
	if(NULL == __gl_api.SecondaryColor3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3f", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3fv = dlsym(h, "glSecondaryColor3fv");
	if(NULL == __gl_api.SecondaryColor3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3fv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3i = dlsym(h, "glSecondaryColor3i");
	if(NULL == __gl_api.SecondaryColor3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3i", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3iv = dlsym(h, "glSecondaryColor3iv");
	if(NULL == __gl_api.SecondaryColor3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3iv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3s = dlsym(h, "glSecondaryColor3s");
	if(NULL == __gl_api.SecondaryColor3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3s", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3sv = dlsym(h, "glSecondaryColor3sv");
	if(NULL == __gl_api.SecondaryColor3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3sv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3ub = dlsym(h, "glSecondaryColor3ub");
	if(NULL == __gl_api.SecondaryColor3ub) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3ub", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3ubv = dlsym(h, "glSecondaryColor3ubv");
	if(NULL == __gl_api.SecondaryColor3ubv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3ubv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3ui = dlsym(h, "glSecondaryColor3ui");
	if(NULL == __gl_api.SecondaryColor3ui) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3ui", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3uiv = dlsym(h, "glSecondaryColor3uiv");
	if(NULL == __gl_api.SecondaryColor3uiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3uiv", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3us = dlsym(h, "glSecondaryColor3us");
	if(NULL == __gl_api.SecondaryColor3us) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3us", dlerror());
		abort();
	}
	__gl_api.SecondaryColor3usv = dlsym(h, "glSecondaryColor3usv");
	if(NULL == __gl_api.SecondaryColor3usv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColor3usv", dlerror());
		abort();
	}
	__gl_api.SecondaryColorPointer = dlsym(h, "glSecondaryColorPointer");
	if(NULL == __gl_api.SecondaryColorPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glSecondaryColorPointer", dlerror());
		abort();
	}
	__gl_api.PointParameterf = dlsym(h, "glPointParameterf");
	if(NULL == __gl_api.PointParameterf) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPointParameterf", dlerror());
		abort();
	}
	__gl_api.PointParameterfv = dlsym(h, "glPointParameterfv");
	if(NULL == __gl_api.PointParameterfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPointParameterfv", dlerror());
		abort();
	}
	__gl_api.PointParameteri = dlsym(h, "glPointParameteri");
	if(NULL == __gl_api.PointParameteri) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPointParameteri", dlerror());
		abort();
	}
	__gl_api.PointParameteriv = dlsym(h, "glPointParameteriv");
	if(NULL == __gl_api.PointParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glPointParameteriv", dlerror());
		abort();
	}
	__gl_api.BlendFuncSeparate = dlsym(h, "glBlendFuncSeparate");
	if(NULL == __gl_api.BlendFuncSeparate) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBlendFuncSeparate", dlerror());
		abort();
	}
	__gl_api.MultiDrawArrays = dlsym(h, "glMultiDrawArrays");
	if(NULL == __gl_api.MultiDrawArrays) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiDrawArrays", dlerror());
		abort();
	}
	__gl_api.MultiDrawElements = dlsym(h, "glMultiDrawElements");
	if(NULL == __gl_api.MultiDrawElements) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMultiDrawElements", dlerror());
		abort();
	}
	__gl_api.WindowPos2d = dlsym(h, "glWindowPos2d");
	if(NULL == __gl_api.WindowPos2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2d", dlerror());
		abort();
	}
	__gl_api.WindowPos2dv = dlsym(h, "glWindowPos2dv");
	if(NULL == __gl_api.WindowPos2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2dv", dlerror());
		abort();
	}
	__gl_api.WindowPos2f = dlsym(h, "glWindowPos2f");
	if(NULL == __gl_api.WindowPos2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2f", dlerror());
		abort();
	}
	__gl_api.WindowPos2fv = dlsym(h, "glWindowPos2fv");
	if(NULL == __gl_api.WindowPos2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2fv", dlerror());
		abort();
	}
	__gl_api.WindowPos2i = dlsym(h, "glWindowPos2i");
	if(NULL == __gl_api.WindowPos2i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2i", dlerror());
		abort();
	}
	__gl_api.WindowPos2iv = dlsym(h, "glWindowPos2iv");
	if(NULL == __gl_api.WindowPos2iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2iv", dlerror());
		abort();
	}
	__gl_api.WindowPos2s = dlsym(h, "glWindowPos2s");
	if(NULL == __gl_api.WindowPos2s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2s", dlerror());
		abort();
	}
	__gl_api.WindowPos2sv = dlsym(h, "glWindowPos2sv");
	if(NULL == __gl_api.WindowPos2sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos2sv", dlerror());
		abort();
	}
	__gl_api.WindowPos3d = dlsym(h, "glWindowPos3d");
	if(NULL == __gl_api.WindowPos3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3d", dlerror());
		abort();
	}
	__gl_api.WindowPos3dv = dlsym(h, "glWindowPos3dv");
	if(NULL == __gl_api.WindowPos3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3dv", dlerror());
		abort();
	}
	__gl_api.WindowPos3f = dlsym(h, "glWindowPos3f");
	if(NULL == __gl_api.WindowPos3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3f", dlerror());
		abort();
	}
	__gl_api.WindowPos3fv = dlsym(h, "glWindowPos3fv");
	if(NULL == __gl_api.WindowPos3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3fv", dlerror());
		abort();
	}
	__gl_api.WindowPos3i = dlsym(h, "glWindowPos3i");
	if(NULL == __gl_api.WindowPos3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3i", dlerror());
		abort();
	}
	__gl_api.WindowPos3iv = dlsym(h, "glWindowPos3iv");
	if(NULL == __gl_api.WindowPos3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3iv", dlerror());
		abort();
	}
	__gl_api.WindowPos3s = dlsym(h, "glWindowPos3s");
	if(NULL == __gl_api.WindowPos3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3s", dlerror());
		abort();
	}
	__gl_api.WindowPos3sv = dlsym(h, "glWindowPos3sv");
	if(NULL == __gl_api.WindowPos3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glWindowPos3sv", dlerror());
		abort();
	}
	__gl_api.GenQueries = dlsym(h, "glGenQueries");
	if(NULL == __gl_api.GenQueries) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGenQueries", dlerror());
		abort();
	}
	__gl_api.DeleteQueries = dlsym(h, "glDeleteQueries");
	if(NULL == __gl_api.DeleteQueries) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDeleteQueries", dlerror());
		abort();
	}
	__gl_api.IsQuery = dlsym(h, "glIsQuery");
	if(NULL == __gl_api.IsQuery) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsQuery", dlerror());
		abort();
	}
	__gl_api.BeginQuery = dlsym(h, "glBeginQuery");
	if(NULL == __gl_api.BeginQuery) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBeginQuery", dlerror());
		abort();
	}
	__gl_api.EndQuery = dlsym(h, "glEndQuery");
	if(NULL == __gl_api.EndQuery) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEndQuery", dlerror());
		abort();
	}
	__gl_api.GetQueryiv = dlsym(h, "glGetQueryiv");
	if(NULL == __gl_api.GetQueryiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetQueryiv", dlerror());
		abort();
	}
	__gl_api.GetQueryObjectiv = dlsym(h, "glGetQueryObjectiv");
	if(NULL == __gl_api.GetQueryObjectiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetQueryObjectiv", dlerror());
		abort();
	}
	__gl_api.GetQueryObjectuiv = dlsym(h, "glGetQueryObjectuiv");
	if(NULL == __gl_api.GetQueryObjectuiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetQueryObjectuiv", dlerror());
		abort();
	}
	__gl_api.BindBuffer = dlsym(h, "glBindBuffer");
	if(NULL == __gl_api.BindBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBindBuffer", dlerror());
		abort();
	}
	__gl_api.DeleteBuffers = dlsym(h, "glDeleteBuffers");
	if(NULL == __gl_api.DeleteBuffers) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDeleteBuffers", dlerror());
		abort();
	}
	__gl_api.GenBuffers = dlsym(h, "glGenBuffers");
	if(NULL == __gl_api.GenBuffers) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGenBuffers", dlerror());
		abort();
	}
	__gl_api.IsBuffer = dlsym(h, "glIsBuffer");
	if(NULL == __gl_api.IsBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsBuffer", dlerror());
		abort();
	}
	__gl_api.BufferData = dlsym(h, "glBufferData");
	if(NULL == __gl_api.BufferData) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBufferData", dlerror());
		abort();
	}
	__gl_api.BufferSubData = dlsym(h, "glBufferSubData");
	if(NULL == __gl_api.BufferSubData) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBufferSubData", dlerror());
		abort();
	}
	__gl_api.GetBufferSubData = dlsym(h, "glGetBufferSubData");
	if(NULL == __gl_api.GetBufferSubData) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetBufferSubData", dlerror());
		abort();
	}
	__gl_api.MapBuffer = dlsym(h, "glMapBuffer");
	if(NULL == __gl_api.MapBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glMapBuffer", dlerror());
		abort();
	}
	__gl_api.UnmapBuffer = dlsym(h, "glUnmapBuffer");
	if(NULL == __gl_api.UnmapBuffer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUnmapBuffer", dlerror());
		abort();
	}
	__gl_api.GetBufferParameteriv = dlsym(h, "glGetBufferParameteriv");
	if(NULL == __gl_api.GetBufferParameteriv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetBufferParameteriv", dlerror());
		abort();
	}
	__gl_api.GetBufferPointerv = dlsym(h, "glGetBufferPointerv");
	if(NULL == __gl_api.GetBufferPointerv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetBufferPointerv", dlerror());
		abort();
	}
	__gl_api.DrawBuffers = dlsym(h, "glDrawBuffers");
	if(NULL == __gl_api.DrawBuffers) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDrawBuffers", dlerror());
		abort();
	}
	__gl_api.VertexAttrib1d = dlsym(h, "glVertexAttrib1d");
	if(NULL == __gl_api.VertexAttrib1d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib1d", dlerror());
		abort();
	}
	__gl_api.VertexAttrib1dv = dlsym(h, "glVertexAttrib1dv");
	if(NULL == __gl_api.VertexAttrib1dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib1dv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib1f = dlsym(h, "glVertexAttrib1f");
	if(NULL == __gl_api.VertexAttrib1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib1f", dlerror());
		abort();
	}
	__gl_api.VertexAttrib1fv = dlsym(h, "glVertexAttrib1fv");
	if(NULL == __gl_api.VertexAttrib1fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib1fv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib1s = dlsym(h, "glVertexAttrib1s");
	if(NULL == __gl_api.VertexAttrib1s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib1s", dlerror());
		abort();
	}
	__gl_api.VertexAttrib1sv = dlsym(h, "glVertexAttrib1sv");
	if(NULL == __gl_api.VertexAttrib1sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib1sv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib2d = dlsym(h, "glVertexAttrib2d");
	if(NULL == __gl_api.VertexAttrib2d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib2d", dlerror());
		abort();
	}
	__gl_api.VertexAttrib2dv = dlsym(h, "glVertexAttrib2dv");
	if(NULL == __gl_api.VertexAttrib2dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib2dv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib2f = dlsym(h, "glVertexAttrib2f");
	if(NULL == __gl_api.VertexAttrib2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib2f", dlerror());
		abort();
	}
	__gl_api.VertexAttrib2fv = dlsym(h, "glVertexAttrib2fv");
	if(NULL == __gl_api.VertexAttrib2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib2fv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib2s = dlsym(h, "glVertexAttrib2s");
	if(NULL == __gl_api.VertexAttrib2s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib2s", dlerror());
		abort();
	}
	__gl_api.VertexAttrib2sv = dlsym(h, "glVertexAttrib2sv");
	if(NULL == __gl_api.VertexAttrib2sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib2sv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib3d = dlsym(h, "glVertexAttrib3d");
	if(NULL == __gl_api.VertexAttrib3d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib3d", dlerror());
		abort();
	}
	__gl_api.VertexAttrib3dv = dlsym(h, "glVertexAttrib3dv");
	if(NULL == __gl_api.VertexAttrib3dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib3dv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib3f = dlsym(h, "glVertexAttrib3f");
	if(NULL == __gl_api.VertexAttrib3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib3f", dlerror());
		abort();
	}
	__gl_api.VertexAttrib3fv = dlsym(h, "glVertexAttrib3fv");
	if(NULL == __gl_api.VertexAttrib3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib3fv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib3s = dlsym(h, "glVertexAttrib3s");
	if(NULL == __gl_api.VertexAttrib3s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib3s", dlerror());
		abort();
	}
	__gl_api.VertexAttrib3sv = dlsym(h, "glVertexAttrib3sv");
	if(NULL == __gl_api.VertexAttrib3sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib3sv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Nbv = dlsym(h, "glVertexAttrib4Nbv");
	if(NULL == __gl_api.VertexAttrib4Nbv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Nbv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Niv = dlsym(h, "glVertexAttrib4Niv");
	if(NULL == __gl_api.VertexAttrib4Niv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Niv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Nsv = dlsym(h, "glVertexAttrib4Nsv");
	if(NULL == __gl_api.VertexAttrib4Nsv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Nsv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Nub = dlsym(h, "glVertexAttrib4Nub");
	if(NULL == __gl_api.VertexAttrib4Nub) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Nub", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Nubv = dlsym(h, "glVertexAttrib4Nubv");
	if(NULL == __gl_api.VertexAttrib4Nubv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Nubv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Nuiv = dlsym(h, "glVertexAttrib4Nuiv");
	if(NULL == __gl_api.VertexAttrib4Nuiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Nuiv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4Nusv = dlsym(h, "glVertexAttrib4Nusv");
	if(NULL == __gl_api.VertexAttrib4Nusv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4Nusv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4bv = dlsym(h, "glVertexAttrib4bv");
	if(NULL == __gl_api.VertexAttrib4bv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4bv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4d = dlsym(h, "glVertexAttrib4d");
	if(NULL == __gl_api.VertexAttrib4d) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4d", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4dv = dlsym(h, "glVertexAttrib4dv");
	if(NULL == __gl_api.VertexAttrib4dv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4dv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4f = dlsym(h, "glVertexAttrib4f");
	if(NULL == __gl_api.VertexAttrib4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4f", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4fv = dlsym(h, "glVertexAttrib4fv");
	if(NULL == __gl_api.VertexAttrib4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4fv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4iv = dlsym(h, "glVertexAttrib4iv");
	if(NULL == __gl_api.VertexAttrib4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4iv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4s = dlsym(h, "glVertexAttrib4s");
	if(NULL == __gl_api.VertexAttrib4s) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4s", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4sv = dlsym(h, "glVertexAttrib4sv");
	if(NULL == __gl_api.VertexAttrib4sv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4sv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4ubv = dlsym(h, "glVertexAttrib4ubv");
	if(NULL == __gl_api.VertexAttrib4ubv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4ubv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4uiv = dlsym(h, "glVertexAttrib4uiv");
	if(NULL == __gl_api.VertexAttrib4uiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4uiv", dlerror());
		abort();
	}
	__gl_api.VertexAttrib4usv = dlsym(h, "glVertexAttrib4usv");
	if(NULL == __gl_api.VertexAttrib4usv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttrib4usv", dlerror());
		abort();
	}
	__gl_api.VertexAttribPointer = dlsym(h, "glVertexAttribPointer");
	if(NULL == __gl_api.VertexAttribPointer) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glVertexAttribPointer", dlerror());
		abort();
	}
	__gl_api.EnableVertexAttribArray = dlsym(h, "glEnableVertexAttribArray");
	if(NULL == __gl_api.EnableVertexAttribArray) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glEnableVertexAttribArray", dlerror());
		abort();
	}
	__gl_api.DisableVertexAttribArray = dlsym(h, "glDisableVertexAttribArray");
	if(NULL == __gl_api.DisableVertexAttribArray) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDisableVertexAttribArray", dlerror());
		abort();
	}
	__gl_api.GetVertexAttribdv = dlsym(h, "glGetVertexAttribdv");
	if(NULL == __gl_api.GetVertexAttribdv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetVertexAttribdv", dlerror());
		abort();
	}
	__gl_api.GetVertexAttribfv = dlsym(h, "glGetVertexAttribfv");
	if(NULL == __gl_api.GetVertexAttribfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetVertexAttribfv", dlerror());
		abort();
	}
	__gl_api.GetVertexAttribiv = dlsym(h, "glGetVertexAttribiv");
	if(NULL == __gl_api.GetVertexAttribiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetVertexAttribiv", dlerror());
		abort();
	}
	__gl_api.GetVertexAttribPointerv = dlsym(h, "glGetVertexAttribPointerv");
	if(NULL == __gl_api.GetVertexAttribPointerv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetVertexAttribPointerv", dlerror());
		abort();
	}
	__gl_api.DeleteShader = dlsym(h, "glDeleteShader");
	if(NULL == __gl_api.DeleteShader) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDeleteShader", dlerror());
		abort();
	}
	__gl_api.DetachShader = dlsym(h, "glDetachShader");
	if(NULL == __gl_api.DetachShader) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDetachShader", dlerror());
		abort();
	}
	__gl_api.CreateShader = dlsym(h, "glCreateShader");
	if(NULL == __gl_api.CreateShader) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCreateShader", dlerror());
		abort();
	}
	__gl_api.ShaderSource = dlsym(h, "glShaderSource");
	if(NULL == __gl_api.ShaderSource) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glShaderSource", dlerror());
		abort();
	}
	__gl_api.CompileShader = dlsym(h, "glCompileShader");
	if(NULL == __gl_api.CompileShader) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCompileShader", dlerror());
		abort();
	}
	__gl_api.CreateProgram = dlsym(h, "glCreateProgram");
	if(NULL == __gl_api.CreateProgram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glCreateProgram", dlerror());
		abort();
	}
	__gl_api.AttachShader = dlsym(h, "glAttachShader");
	if(NULL == __gl_api.AttachShader) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glAttachShader", dlerror());
		abort();
	}
	__gl_api.LinkProgram = dlsym(h, "glLinkProgram");
	if(NULL == __gl_api.LinkProgram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glLinkProgram", dlerror());
		abort();
	}
	__gl_api.UseProgram = dlsym(h, "glUseProgram");
	if(NULL == __gl_api.UseProgram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUseProgram", dlerror());
		abort();
	}
	__gl_api.DeleteProgram = dlsym(h, "glDeleteProgram");
	if(NULL == __gl_api.DeleteProgram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glDeleteProgram", dlerror());
		abort();
	}
	__gl_api.ValidateProgram = dlsym(h, "glValidateProgram");
	if(NULL == __gl_api.ValidateProgram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glValidateProgram", dlerror());
		abort();
	}
	__gl_api.Uniform1f = dlsym(h, "glUniform1f");
	if(NULL == __gl_api.Uniform1f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform1f", dlerror());
		abort();
	}
	__gl_api.Uniform2f = dlsym(h, "glUniform2f");
	if(NULL == __gl_api.Uniform2f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform2f", dlerror());
		abort();
	}
	__gl_api.Uniform3f = dlsym(h, "glUniform3f");
	if(NULL == __gl_api.Uniform3f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform3f", dlerror());
		abort();
	}
	__gl_api.Uniform4f = dlsym(h, "glUniform4f");
	if(NULL == __gl_api.Uniform4f) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform4f", dlerror());
		abort();
	}
	__gl_api.Uniform1i = dlsym(h, "glUniform1i");
	if(NULL == __gl_api.Uniform1i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform1i", dlerror());
		abort();
	}
	__gl_api.Uniform2i = dlsym(h, "glUniform2i");
	if(NULL == __gl_api.Uniform2i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform2i", dlerror());
		abort();
	}
	__gl_api.Uniform3i = dlsym(h, "glUniform3i");
	if(NULL == __gl_api.Uniform3i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform3i", dlerror());
		abort();
	}
	__gl_api.Uniform4i = dlsym(h, "glUniform4i");
	if(NULL == __gl_api.Uniform4i) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform4i", dlerror());
		abort();
	}
	__gl_api.Uniform1fv = dlsym(h, "glUniform1fv");
	if(NULL == __gl_api.Uniform1fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform1fv", dlerror());
		abort();
	}
	__gl_api.Uniform2fv = dlsym(h, "glUniform2fv");
	if(NULL == __gl_api.Uniform2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform2fv", dlerror());
		abort();
	}
	__gl_api.Uniform3fv = dlsym(h, "glUniform3fv");
	if(NULL == __gl_api.Uniform3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform3fv", dlerror());
		abort();
	}
	__gl_api.Uniform4fv = dlsym(h, "glUniform4fv");
	if(NULL == __gl_api.Uniform4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform4fv", dlerror());
		abort();
	}
	__gl_api.Uniform1iv = dlsym(h, "glUniform1iv");
	if(NULL == __gl_api.Uniform1iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform1iv", dlerror());
		abort();
	}
	__gl_api.Uniform2iv = dlsym(h, "glUniform2iv");
	if(NULL == __gl_api.Uniform2iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform2iv", dlerror());
		abort();
	}
	__gl_api.Uniform3iv = dlsym(h, "glUniform3iv");
	if(NULL == __gl_api.Uniform3iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform3iv", dlerror());
		abort();
	}
	__gl_api.Uniform4iv = dlsym(h, "glUniform4iv");
	if(NULL == __gl_api.Uniform4iv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniform4iv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix2fv = dlsym(h, "glUniformMatrix2fv");
	if(NULL == __gl_api.UniformMatrix2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix2fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix3fv = dlsym(h, "glUniformMatrix3fv");
	if(NULL == __gl_api.UniformMatrix3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix3fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix4fv = dlsym(h, "glUniformMatrix4fv");
	if(NULL == __gl_api.UniformMatrix4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix4fv", dlerror());
		abort();
	}
	__gl_api.IsShader = dlsym(h, "glIsShader");
	if(NULL == __gl_api.IsShader) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsShader", dlerror());
		abort();
	}
	__gl_api.IsProgram = dlsym(h, "glIsProgram");
	if(NULL == __gl_api.IsProgram) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glIsProgram", dlerror());
		abort();
	}
	__gl_api.GetShaderiv = dlsym(h, "glGetShaderiv");
	if(NULL == __gl_api.GetShaderiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetShaderiv", dlerror());
		abort();
	}
	__gl_api.GetProgramiv = dlsym(h, "glGetProgramiv");
	if(NULL == __gl_api.GetProgramiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetProgramiv", dlerror());
		abort();
	}
	__gl_api.GetAttachedShaders = dlsym(h, "glGetAttachedShaders");
	if(NULL == __gl_api.GetAttachedShaders) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetAttachedShaders", dlerror());
		abort();
	}
	__gl_api.GetShaderInfoLog = dlsym(h, "glGetShaderInfoLog");
	if(NULL == __gl_api.GetShaderInfoLog) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetShaderInfoLog", dlerror());
		abort();
	}
	__gl_api.GetProgramInfoLog = dlsym(h, "glGetProgramInfoLog");
	if(NULL == __gl_api.GetProgramInfoLog) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetProgramInfoLog", dlerror());
		abort();
	}
	__gl_api.GetUniformLocation = dlsym(h, "glGetUniformLocation");
	if(NULL == __gl_api.GetUniformLocation) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetUniformLocation", dlerror());
		abort();
	}
	__gl_api.GetActiveUniform = dlsym(h, "glGetActiveUniform");
	if(NULL == __gl_api.GetActiveUniform) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetActiveUniform", dlerror());
		abort();
	}
	__gl_api.GetUniformfv = dlsym(h, "glGetUniformfv");
	if(NULL == __gl_api.GetUniformfv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetUniformfv", dlerror());
		abort();
	}
	__gl_api.GetUniformiv = dlsym(h, "glGetUniformiv");
	if(NULL == __gl_api.GetUniformiv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetUniformiv", dlerror());
		abort();
	}
	__gl_api.GetShaderSource = dlsym(h, "glGetShaderSource");
	if(NULL == __gl_api.GetShaderSource) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetShaderSource", dlerror());
		abort();
	}
	__gl_api.BindAttribLocation = dlsym(h, "glBindAttribLocation");
	if(NULL == __gl_api.BindAttribLocation) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glBindAttribLocation", dlerror());
		abort();
	}
	__gl_api.GetActiveAttrib = dlsym(h, "glGetActiveAttrib");
	if(NULL == __gl_api.GetActiveAttrib) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetActiveAttrib", dlerror());
		abort();
	}
	__gl_api.GetAttribLocation = dlsym(h, "glGetAttribLocation");
	if(NULL == __gl_api.GetAttribLocation) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glGetAttribLocation", dlerror());
		abort();
	}
	__gl_api.StencilFuncSeparate = dlsym(h, "glStencilFuncSeparate");
	if(NULL == __gl_api.StencilFuncSeparate) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glStencilFuncSeparate", dlerror());
		abort();
	}
	__gl_api.StencilOpSeparate = dlsym(h, "glStencilOpSeparate");
	if(NULL == __gl_api.StencilOpSeparate) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glStencilOpSeparate", dlerror());
		abort();
	}
	__gl_api.StencilMaskSeparate = dlsym(h, "glStencilMaskSeparate");
	if(NULL == __gl_api.StencilMaskSeparate) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glStencilMaskSeparate", dlerror());
		abort();
	}
	__gl_api.UniformMatrix2x3fv = dlsym(h, "glUniformMatrix2x3fv");
	if(NULL == __gl_api.UniformMatrix2x3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix2x3fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix3x2fv = dlsym(h, "glUniformMatrix3x2fv");
	if(NULL == __gl_api.UniformMatrix3x2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix3x2fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix2x4fv = dlsym(h, "glUniformMatrix2x4fv");
	if(NULL == __gl_api.UniformMatrix2x4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix2x4fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix4x2fv = dlsym(h, "glUniformMatrix4x2fv");
	if(NULL == __gl_api.UniformMatrix4x2fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix4x2fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix3x4fv = dlsym(h, "glUniformMatrix3x4fv");
	if(NULL == __gl_api.UniformMatrix3x4fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix3x4fv", dlerror());
		abort();
	}
	__gl_api.UniformMatrix4x3fv = dlsym(h, "glUniformMatrix4x3fv");
	if(NULL == __gl_api.UniformMatrix4x3fv) {
		fprintf(stderr, "symbol not found: %s.  Error: %s\n", "glUniformMatrix4x3fv", dlerror());
		abort();
	}
}

