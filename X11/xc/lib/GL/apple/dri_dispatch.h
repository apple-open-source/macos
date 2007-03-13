/* dri_dispatch.h -- built automatically, DO NOT EDIT
   $Id: dri_dispatch.h,v 1.10 2006/09/06 21:22:28 jharper Exp $
   $XFree86: $ */

DEFUN_LOCAL_VOID (NewList,
    (void *rend, GLuint list, GLenum mode),
    (list, mode))

DEFUN_LOCAL_VOID (EndList,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (CallList,
    (void *rend, GLuint list),
    (list))

DEFUN_LOCAL_VOID (CallLists,
    (void *rend, GLsizei n, GLenum type, const GLvoid * lists),
    (n, type, lists))

DEFUN_LOCAL_VOID (DeleteLists,
    (void *rend, GLuint list, GLsizei range),
    (list, range))

DEFUN_LOCAL (GLuint, GenLists,
    (void *rend, GLsizei range),
    (range))

DEFUN_LOCAL_VOID (ListBase,
    (void *rend, GLuint base),
    (base))

DEFUN_LOCAL_VOID (Begin,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (Bitmap,
    (void *rend, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap),
    (width, height, xorig, yorig, xmove, ymove, bitmap))

DEFUN_LOCAL_VOID (Color3b,
    (void *rend, GLbyte red, GLbyte green, GLbyte blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3bv,
    (void *rend, const GLbyte * v),
    (v))

DEFUN_LOCAL_VOID (Color3d,
    (void *rend, GLdouble red, GLdouble green, GLdouble blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (Color3f,
    (void *rend, GLfloat red, GLfloat green, GLfloat blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (Color3i,
    (void *rend, GLint red, GLint green, GLint blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (Color3s,
    (void *rend, GLshort red, GLshort green, GLshort blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (Color3ub,
    (void *rend, GLubyte red, GLubyte green, GLubyte blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3ubv,
    (void *rend, const GLubyte * v),
    (v))

DEFUN_LOCAL_VOID (Color3ui,
    (void *rend, GLuint red, GLuint green, GLuint blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3uiv,
    (void *rend, const GLuint * v),
    (v))

DEFUN_LOCAL_VOID (Color3us,
    (void *rend, GLushort red, GLushort green, GLushort blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (Color3usv,
    (void *rend, const GLushort * v),
    (v))

DEFUN_LOCAL_VOID (Color4b,
    (void *rend, GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4bv,
    (void *rend, const GLbyte * v),
    (v))

DEFUN_LOCAL_VOID (Color4d,
    (void *rend, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (Color4f,
    (void *rend, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (Color4i,
    (void *rend, GLint red, GLint green, GLint blue, GLint alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (Color4s,
    (void *rend, GLshort red, GLshort green, GLshort blue, GLshort alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (Color4ub,
    (void *rend, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4ubv,
    (void *rend, const GLubyte * v),
    (v))

DEFUN_LOCAL_VOID (Color4ui,
    (void *rend, GLuint red, GLuint green, GLuint blue, GLuint alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4uiv,
    (void *rend, const GLuint * v),
    (v))

DEFUN_LOCAL_VOID (Color4us,
    (void *rend, GLushort red, GLushort green, GLushort blue, GLushort alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (Color4usv,
    (void *rend, const GLushort * v),
    (v))

DEFUN_LOCAL_VOID (EdgeFlag,
    (void *rend, GLboolean flag),
    (flag))

DEFUN_LOCAL_VOID (EdgeFlagv,
    (void *rend, const GLboolean * flag),
    (flag))

DEFUN_LOCAL_VOID (End,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (Indexd,
    (void *rend, GLdouble c),
    (c))

DEFUN_LOCAL_VOID (Indexdv,
    (void *rend, const GLdouble * c),
    (c))

DEFUN_LOCAL_VOID (Indexf,
    (void *rend, GLfloat c),
    (c))

DEFUN_LOCAL_VOID (Indexfv,
    (void *rend, const GLfloat * c),
    (c))

DEFUN_LOCAL_VOID (Indexi,
    (void *rend, GLint c),
    (c))

DEFUN_LOCAL_VOID (Indexiv,
    (void *rend, const GLint * c),
    (c))

DEFUN_LOCAL_VOID (Indexs,
    (void *rend, GLshort c),
    (c))

DEFUN_LOCAL_VOID (Indexsv,
    (void *rend, const GLshort * c),
    (c))

DEFUN_LOCAL_VOID (Normal3b,
    (void *rend, GLbyte nx, GLbyte ny, GLbyte nz),
    (nx, ny, nz))

DEFUN_LOCAL_VOID (Normal3bv,
    (void *rend, const GLbyte * v),
    (v))

DEFUN_LOCAL_VOID (Normal3d,
    (void *rend, GLdouble nx, GLdouble ny, GLdouble nz),
    (nx, ny, nz))

DEFUN_LOCAL_VOID (Normal3dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (Normal3f,
    (void *rend, GLfloat nx, GLfloat ny, GLfloat nz),
    (nx, ny, nz))

DEFUN_LOCAL_VOID (Normal3fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (Normal3i,
    (void *rend, GLint nx, GLint ny, GLint nz),
    (nx, ny, nz))

DEFUN_LOCAL_VOID (Normal3iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (Normal3s,
    (void *rend, GLshort nx, GLshort ny, GLshort nz),
    (nx, ny, nz))

DEFUN_LOCAL_VOID (Normal3sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos2d,
    (void *rend, GLdouble x, GLdouble y),
    (x, y))

DEFUN_LOCAL_VOID (RasterPos2dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos2f,
    (void *rend, GLfloat x, GLfloat y),
    (x, y))

DEFUN_LOCAL_VOID (RasterPos2fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos2i,
    (void *rend, GLint x, GLint y),
    (x, y))

DEFUN_LOCAL_VOID (RasterPos2iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos2s,
    (void *rend, GLshort x, GLshort y),
    (x, y))

DEFUN_LOCAL_VOID (RasterPos2sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos3d,
    (void *rend, GLdouble x, GLdouble y, GLdouble z),
    (x, y, z))

DEFUN_LOCAL_VOID (RasterPos3dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos3f,
    (void *rend, GLfloat x, GLfloat y, GLfloat z),
    (x, y, z))

DEFUN_LOCAL_VOID (RasterPos3fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos3i,
    (void *rend, GLint x, GLint y, GLint z),
    (x, y, z))

DEFUN_LOCAL_VOID (RasterPos3iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos3s,
    (void *rend, GLshort x, GLshort y, GLshort z),
    (x, y, z))

DEFUN_LOCAL_VOID (RasterPos3sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos4d,
    (void *rend, GLdouble x, GLdouble y, GLdouble z, GLdouble w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (RasterPos4dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos4f,
    (void *rend, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (RasterPos4fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos4i,
    (void *rend, GLint x, GLint y, GLint z, GLint w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (RasterPos4iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (RasterPos4s,
    (void *rend, GLshort x, GLshort y, GLshort z, GLshort w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (RasterPos4sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (Rectd,
    (void *rend, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2),
    (x1, y1, x2, y2))

DEFUN_LOCAL_VOID (Rectdv,
    (void *rend, const GLdouble * v1, const GLdouble * v2),
    (v1, v2))

DEFUN_LOCAL_VOID (Rectf,
    (void *rend, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2),
    (x1, y1, x2, y2))

DEFUN_LOCAL_VOID (Rectfv,
    (void *rend, const GLfloat * v1, const GLfloat * v2),
    (v1, v2))

DEFUN_LOCAL_VOID (Recti,
    (void *rend, GLint x1, GLint y1, GLint x2, GLint y2),
    (x1, y1, x2, y2))

DEFUN_LOCAL_VOID (Rectiv,
    (void *rend, const GLint * v1, const GLint * v2),
    (v1, v2))

DEFUN_LOCAL_VOID (Rects,
    (void *rend, GLshort x1, GLshort y1, GLshort x2, GLshort y2),
    (x1, y1, x2, y2))

DEFUN_LOCAL_VOID (Rectsv,
    (void *rend, const GLshort * v1, const GLshort * v2),
    (v1, v2))

DEFUN_LOCAL_VOID (TexCoord1d,
    (void *rend, GLdouble s),
    (s))

DEFUN_LOCAL_VOID (TexCoord1dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord1f,
    (void *rend, GLfloat s),
    (s))

DEFUN_LOCAL_VOID (TexCoord1fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord1i,
    (void *rend, GLint s),
    (s))

DEFUN_LOCAL_VOID (TexCoord1iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord1s,
    (void *rend, GLshort s),
    (s))

DEFUN_LOCAL_VOID (TexCoord1sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord2d,
    (void *rend, GLdouble s, GLdouble t),
    (s, t))

DEFUN_LOCAL_VOID (TexCoord2dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord2f,
    (void *rend, GLfloat s, GLfloat t),
    (s, t))

DEFUN_LOCAL_VOID (TexCoord2fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord2i,
    (void *rend, GLint s, GLint t),
    (s, t))

DEFUN_LOCAL_VOID (TexCoord2iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord2s,
    (void *rend, GLshort s, GLshort t),
    (s, t))

DEFUN_LOCAL_VOID (TexCoord2sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord3d,
    (void *rend, GLdouble s, GLdouble t, GLdouble r),
    (s, t, r))

DEFUN_LOCAL_VOID (TexCoord3dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord3f,
    (void *rend, GLfloat s, GLfloat t, GLfloat r),
    (s, t, r))

DEFUN_LOCAL_VOID (TexCoord3fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord3i,
    (void *rend, GLint s, GLint t, GLint r),
    (s, t, r))

DEFUN_LOCAL_VOID (TexCoord3iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord3s,
    (void *rend, GLshort s, GLshort t, GLshort r),
    (s, t, r))

DEFUN_LOCAL_VOID (TexCoord3sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord4d,
    (void *rend, GLdouble s, GLdouble t, GLdouble r, GLdouble q),
    (s, t, r, q))

DEFUN_LOCAL_VOID (TexCoord4dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord4f,
    (void *rend, GLfloat s, GLfloat t, GLfloat r, GLfloat q),
    (s, t, r, q))

DEFUN_LOCAL_VOID (TexCoord4fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord4i,
    (void *rend, GLint s, GLint t, GLint r, GLint q),
    (s, t, r, q))

DEFUN_LOCAL_VOID (TexCoord4iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (TexCoord4s,
    (void *rend, GLshort s, GLshort t, GLshort r, GLshort q),
    (s, t, r, q))

DEFUN_LOCAL_VOID (TexCoord4sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (Vertex2d,
    (void *rend, GLdouble x, GLdouble y),
    (x, y))

DEFUN_LOCAL_VOID (Vertex2dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (Vertex2f,
    (void *rend, GLfloat x, GLfloat y),
    (x, y))

DEFUN_LOCAL_VOID (Vertex2fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (Vertex2i,
    (void *rend, GLint x, GLint y),
    (x, y))

DEFUN_LOCAL_VOID (Vertex2iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (Vertex2s,
    (void *rend, GLshort x, GLshort y),
    (x, y))

DEFUN_LOCAL_VOID (Vertex2sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (Vertex3d,
    (void *rend, GLdouble x, GLdouble y, GLdouble z),
    (x, y, z))

DEFUN_LOCAL_VOID (Vertex3dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (Vertex3f,
    (void *rend, GLfloat x, GLfloat y, GLfloat z),
    (x, y, z))

DEFUN_LOCAL_VOID (Vertex3fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (Vertex3i,
    (void *rend, GLint x, GLint y, GLint z),
    (x, y, z))

DEFUN_LOCAL_VOID (Vertex3iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (Vertex3s,
    (void *rend, GLshort x, GLshort y, GLshort z),
    (x, y, z))

DEFUN_LOCAL_VOID (Vertex3sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (Vertex4d,
    (void *rend, GLdouble x, GLdouble y, GLdouble z, GLdouble w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (Vertex4dv,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (Vertex4f,
    (void *rend, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (Vertex4fv,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (Vertex4i,
    (void *rend, GLint x, GLint y, GLint z, GLint w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (Vertex4iv,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (Vertex4s,
    (void *rend, GLshort x, GLshort y, GLshort z, GLshort w),
    (x, y, z, w))

DEFUN_LOCAL_VOID (Vertex4sv,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (ClipPlane,
    (void *rend, GLenum plane, const GLdouble * equation),
    (plane, equation))

DEFUN_LOCAL_VOID (ColorMaterial,
    (void *rend, GLenum face, GLenum mode),
    (face, mode))

DEFUN_LOCAL_VOID (CullFace,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (Fogf,
    (void *rend, GLenum pname, GLfloat param),
    (pname, param))

DEFUN_LOCAL_VOID (Fogfv,
    (void *rend, GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_LOCAL_VOID (Fogi,
    (void *rend, GLenum pname, GLint param),
    (pname, param))

DEFUN_LOCAL_VOID (Fogiv,
    (void *rend, GLenum pname, const GLint * params),
    (pname, params))

DEFUN_LOCAL_VOID (FrontFace,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (Hint,
    (void *rend, GLenum target, GLenum mode),
    (target, mode))

DEFUN_LOCAL_VOID (Lightf,
    (void *rend, GLenum light, GLenum pname, GLfloat param),
    (light, pname, param))

DEFUN_LOCAL_VOID (Lightfv,
    (void *rend, GLenum light, GLenum pname, const GLfloat * params),
    (light, pname, params))

DEFUN_LOCAL_VOID (Lighti,
    (void *rend, GLenum light, GLenum pname, GLint param),
    (light, pname, param))

DEFUN_LOCAL_VOID (Lightiv,
    (void *rend, GLenum light, GLenum pname, const GLint * params),
    (light, pname, params))

DEFUN_LOCAL_VOID (LightModelf,
    (void *rend, GLenum pname, GLfloat param),
    (pname, param))

DEFUN_LOCAL_VOID (LightModelfv,
    (void *rend, GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_LOCAL_VOID (LightModeli,
    (void *rend, GLenum pname, GLint param),
    (pname, param))

DEFUN_LOCAL_VOID (LightModeliv,
    (void *rend, GLenum pname, const GLint * params),
    (pname, params))

DEFUN_LOCAL_VOID (LineStipple,
    (void *rend, GLint factor, GLushort pattern),
    (factor, pattern))

DEFUN_LOCAL_VOID (LineWidth,
    (void *rend, GLfloat width),
    (width))

DEFUN_LOCAL_VOID (Materialf,
    (void *rend, GLenum face, GLenum pname, GLfloat param),
    (face, pname, param))

DEFUN_LOCAL_VOID (Materialfv,
    (void *rend, GLenum face, GLenum pname, const GLfloat * params),
    (face, pname, params))

DEFUN_LOCAL_VOID (Materiali,
    (void *rend, GLenum face, GLenum pname, GLint param),
    (face, pname, param))

DEFUN_LOCAL_VOID (Materialiv,
    (void *rend, GLenum face, GLenum pname, const GLint * params),
    (face, pname, params))

DEFUN_LOCAL_VOID (PointSize,
    (void *rend, GLfloat size),
    (size))

DEFUN_LOCAL_VOID (PolygonMode,
    (void *rend, GLenum face, GLenum mode),
    (face, mode))

DEFUN_LOCAL_VOID (PolygonStipple,
    (void *rend, const GLubyte * mask),
    (mask))

DEFUN_LOCAL_VOID (Scissor,
    (void *rend, GLint x, GLint y, GLsizei width, GLsizei height),
    (x, y, width, height))

DEFUN_LOCAL_VOID (ShadeModel,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (TexParameterf,
    (void *rend, GLenum target, GLenum pname, GLfloat param),
    (target, pname, param))

DEFUN_LOCAL_VOID (TexParameterfv,
    (void *rend, GLenum target, GLenum pname, const GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (TexParameteri,
    (void *rend, GLenum target, GLenum pname, GLint param),
    (target, pname, param))

DEFUN_LOCAL_VOID (TexParameteriv,
    (void *rend, GLenum target, GLenum pname, const GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (TexImage1D,
    (void *rend, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalformat, width, border, format, type, pixels))

DEFUN_LOCAL_VOID (TexImage2D,
    (void *rend, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalformat, width, height, border, format, type, pixels))

DEFUN_LOCAL_VOID (TexEnvf,
    (void *rend, GLenum target, GLenum pname, GLfloat param),
    (target, pname, param))

DEFUN_LOCAL_VOID (TexEnvfv,
    (void *rend, GLenum target, GLenum pname, const GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (TexEnvi,
    (void *rend, GLenum target, GLenum pname, GLint param),
    (target, pname, param))

DEFUN_LOCAL_VOID (TexEnviv,
    (void *rend, GLenum target, GLenum pname, const GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (TexGend,
    (void *rend, GLenum coord, GLenum pname, GLdouble param),
    (coord, pname, param))

DEFUN_LOCAL_VOID (TexGendv,
    (void *rend, GLenum coord, GLenum pname, const GLdouble * params),
    (coord, pname, params))

DEFUN_LOCAL_VOID (TexGenf,
    (void *rend, GLenum coord, GLenum pname, GLfloat param),
    (coord, pname, param))

DEFUN_LOCAL_VOID (TexGenfv,
    (void *rend, GLenum coord, GLenum pname, const GLfloat * params),
    (coord, pname, params))

DEFUN_LOCAL_VOID (TexGeni,
    (void *rend, GLenum coord, GLenum pname, GLint param),
    (coord, pname, param))

DEFUN_LOCAL_VOID (TexGeniv,
    (void *rend, GLenum coord, GLenum pname, const GLint * params),
    (coord, pname, params))

DEFUN_LOCAL_VOID (FeedbackBuffer,
    (void *rend, GLsizei size, GLenum type, GLfloat * buffer),
    (size, type, buffer))

DEFUN_LOCAL_VOID (SelectBuffer,
    (void *rend, GLsizei size, GLuint * buffer),
    (size, buffer))

DEFUN_LOCAL (GLint, RenderMode,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (InitNames,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (LoadName,
    (void *rend, GLuint name),
    (name))

DEFUN_LOCAL_VOID (PassThrough,
    (void *rend, GLfloat token),
    (token))

DEFUN_LOCAL_VOID (PopName,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (PushName,
    (void *rend, GLuint name),
    (name))

DEFUN_LOCAL_VOID (DrawBuffer,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (Clear,
    (void *rend, GLbitfield mask),
    (mask))

DEFUN_LOCAL_VOID (ClearAccum,
    (void *rend, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (ClearIndex,
    (void *rend, GLfloat c),
    (c))

DEFUN_LOCAL_VOID (ClearColor,
    (void *rend, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (ClearStencil,
    (void *rend, GLint s),
    (s))

DEFUN_LOCAL_VOID (ClearDepth,
    (void *rend, GLclampd depth),
    (depth))

DEFUN_LOCAL_VOID (StencilMask,
    (void *rend, GLuint mask),
    (mask))

DEFUN_LOCAL_VOID (ColorMask,
    (void *rend, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (DepthMask,
    (void *rend, GLboolean flag),
    (flag))

DEFUN_LOCAL_VOID (IndexMask,
    (void *rend, GLuint mask),
    (mask))

DEFUN_LOCAL_VOID (Accum,
    (void *rend, GLenum op, GLfloat value),
    (op, value))

DEFUN_LOCAL_VOID (Disable,
    (void *rend, GLenum cap),
    (cap))

DEFUN_LOCAL_VOID (Enable,
    (void *rend, GLenum cap),
    (cap))

DEFUN_LOCAL_VOID (Finish,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (Flush,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (PopAttrib,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (PushAttrib,
    (void *rend, GLbitfield mask),
    (mask))

DEFUN_LOCAL_VOID (Map1d,
    (void *rend, GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble * points),
    (target, u1, u2, stride, order, points))

DEFUN_LOCAL_VOID (Map1f,
    (void *rend, GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat * points),
    (target, u1, u2, stride, order, points))

DEFUN_LOCAL_VOID (Map2d,
    (void *rend, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble * points),
    (target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points))

DEFUN_LOCAL_VOID (Map2f,
    (void *rend, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat * points),
    (target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points))

DEFUN_LOCAL_VOID (MapGrid1d,
    (void *rend, GLint un, GLdouble u1, GLdouble u2),
    (un, u1, u2))

DEFUN_LOCAL_VOID (MapGrid1f,
    (void *rend, GLint un, GLfloat u1, GLfloat u2),
    (un, u1, u2))

DEFUN_LOCAL_VOID (MapGrid2d,
    (void *rend, GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2),
    (un, u1, u2, vn, v1, v2))

DEFUN_LOCAL_VOID (MapGrid2f,
    (void *rend, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2),
    (un, u1, u2, vn, v1, v2))

DEFUN_LOCAL_VOID (EvalCoord1d,
    (void *rend, GLdouble u),
    (u))

DEFUN_LOCAL_VOID (EvalCoord1dv,
    (void *rend, const GLdouble * u),
    (u))

DEFUN_LOCAL_VOID (EvalCoord1f,
    (void *rend, GLfloat u),
    (u))

DEFUN_LOCAL_VOID (EvalCoord1fv,
    (void *rend, const GLfloat * u),
    (u))

DEFUN_LOCAL_VOID (EvalCoord2d,
    (void *rend, GLdouble u, GLdouble v),
    (u, v))

DEFUN_LOCAL_VOID (EvalCoord2dv,
    (void *rend, const GLdouble * u),
    (u))

DEFUN_LOCAL_VOID (EvalCoord2f,
    (void *rend, GLfloat u, GLfloat v),
    (u, v))

DEFUN_LOCAL_VOID (EvalCoord2fv,
    (void *rend, const GLfloat * u),
    (u))

DEFUN_LOCAL_VOID (EvalMesh1,
    (void *rend, GLenum mode, GLint i1, GLint i2),
    (mode, i1, i2))

DEFUN_LOCAL_VOID (EvalPoint1,
    (void *rend, GLint i),
    (i))

DEFUN_LOCAL_VOID (EvalMesh2,
    (void *rend, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2),
    (mode, i1, i2, j1, j2))

DEFUN_LOCAL_VOID (EvalPoint2,
    (void *rend, GLint i, GLint j),
    (i, j))

DEFUN_LOCAL_VOID (AlphaFunc,
    (void *rend, GLenum func, GLclampf ref),
    (func, ref))

DEFUN_LOCAL_VOID (BlendFunc,
    (void *rend, GLenum sfactor, GLenum dfactor),
    (sfactor, dfactor))

DEFUN_LOCAL_VOID (LogicOp,
    (void *rend, GLenum opcode),
    (opcode))

DEFUN_LOCAL_VOID (StencilFunc,
    (void *rend, GLenum func, GLint ref, GLuint mask),
    (func, ref, mask))

DEFUN_LOCAL_VOID (StencilOp,
    (void *rend, GLenum fail, GLenum zfail, GLenum zpass),
    (fail, zfail, zpass))

DEFUN_LOCAL_VOID (DepthFunc,
    (void *rend, GLenum func),
    (func))

DEFUN_LOCAL_VOID (PixelZoom,
    (void *rend, GLfloat xfactor, GLfloat yfactor),
    (xfactor, yfactor))

DEFUN_LOCAL_VOID (PixelTransferf,
    (void *rend, GLenum pname, GLfloat param),
    (pname, param))

DEFUN_LOCAL_VOID (PixelTransferi,
    (void *rend, GLenum pname, GLint param),
    (pname, param))

DEFUN_LOCAL_VOID (PixelStoref,
    (void *rend, GLenum pname, GLfloat param),
    (pname, param))

DEFUN_LOCAL_VOID (PixelStorei,
    (void *rend, GLenum pname, GLint param),
    (pname, param))

DEFUN_LOCAL_VOID (PixelMapfv,
    (void *rend, GLenum map, GLint mapsize, const GLfloat * values),
    (map, mapsize, values))

DEFUN_LOCAL_VOID (PixelMapuiv,
    (void *rend, GLenum map, GLint mapsize, const GLuint * values),
    (map, mapsize, values))

DEFUN_LOCAL_VOID (PixelMapusv,
    (void *rend, GLenum map, GLint mapsize, const GLushort * values),
    (map, mapsize, values))

DEFUN_LOCAL_VOID (ReadBuffer,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (CopyPixels,
    (void *rend, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type),
    (x, y, width, height, type))

DEFUN_LOCAL_VOID (ReadPixels,
    (void *rend, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels),
    (x, y, width, height, format, type, pixels))

DEFUN_LOCAL_VOID (DrawPixels,
    (void *rend, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels),
    (width, height, format, type, pixels))

DEFUN_LOCAL_VOID (GetBooleanv,
    (void *rend, GLenum pname, GLboolean * params),
    (pname, params))

DEFUN_LOCAL_VOID (GetClipPlane,
    (void *rend, GLenum plane, GLdouble * equation),
    (plane, equation))

DEFUN_LOCAL_VOID (GetDoublev,
    (void *rend, GLenum pname, GLdouble * params),
    (pname, params))

DEFUN_LOCAL (GLenum, GetError,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (GetFloatv,
    (void *rend, GLenum pname, GLfloat * params),
    (pname, params))

DEFUN_LOCAL_VOID (GetIntegerv,
    (void *rend, GLenum pname, GLint * params),
    (pname, params))

DEFUN_LOCAL_VOID (GetLightfv,
    (void *rend, GLenum light, GLenum pname, GLfloat * params),
    (light, pname, params))

DEFUN_LOCAL_VOID (GetLightiv,
    (void *rend, GLenum light, GLenum pname, GLint * params),
    (light, pname, params))

DEFUN_LOCAL_VOID (GetMapdv,
    (void *rend, GLenum target, GLenum query, GLdouble * v),
    (target, query, v))

DEFUN_LOCAL_VOID (GetMapfv,
    (void *rend, GLenum target, GLenum query, GLfloat * v),
    (target, query, v))

DEFUN_LOCAL_VOID (GetMapiv,
    (void *rend, GLenum target, GLenum query, GLint * v),
    (target, query, v))

DEFUN_LOCAL_VOID (GetMaterialfv,
    (void *rend, GLenum face, GLenum pname, GLfloat * params),
    (face, pname, params))

DEFUN_LOCAL_VOID (GetMaterialiv,
    (void *rend, GLenum face, GLenum pname, GLint * params),
    (face, pname, params))

DEFUN_LOCAL_VOID (GetPixelMapfv,
    (void *rend, GLenum map, GLfloat * values),
    (map, values))

DEFUN_LOCAL_VOID (GetPixelMapuiv,
    (void *rend, GLenum map, GLuint * values),
    (map, values))

DEFUN_LOCAL_VOID (GetPixelMapusv,
    (void *rend, GLenum map, GLushort * values),
    (map, values))

DEFUN_LOCAL_VOID (GetPolygonStipple,
    (void *rend, GLubyte * mask),
    (mask))

DEFUN_LOCAL (const GLubyte *, GetString,
    (void *rend, GLenum name),
    (name))

DEFUN_LOCAL_VOID (GetTexEnvfv,
    (void *rend, GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetTexEnviv,
    (void *rend, GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetTexGendv,
    (void *rend, GLenum coord, GLenum pname, GLdouble * params),
    (coord, pname, params))

DEFUN_LOCAL_VOID (GetTexGenfv,
    (void *rend, GLenum coord, GLenum pname, GLfloat * params),
    (coord, pname, params))

DEFUN_LOCAL_VOID (GetTexGeniv,
    (void *rend, GLenum coord, GLenum pname, GLint * params),
    (coord, pname, params))

DEFUN_LOCAL_VOID (GetTexImage,
    (void *rend, GLenum target, GLint level, GLenum format, GLenum type, GLvoid * pixels),
    (target, level, format, type, pixels))

DEFUN_LOCAL_VOID (GetTexParameterfv,
    (void *rend, GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetTexParameteriv,
    (void *rend, GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetTexLevelParameterfv,
    (void *rend, GLenum target, GLint level, GLenum pname, GLfloat * params),
    (target, level, pname, params))

DEFUN_LOCAL_VOID (GetTexLevelParameteriv,
    (void *rend, GLenum target, GLint level, GLenum pname, GLint * params),
    (target, level, pname, params))

DEFUN_LOCAL (GLboolean, IsEnabled,
    (void *rend, GLenum cap),
    (cap))

DEFUN_LOCAL (GLboolean, IsList,
    (void *rend, GLuint list),
    (list))

DEFUN_LOCAL_VOID (DepthRange,
    (void *rend, GLclampd zNear, GLclampd zFar),
    (zNear, zFar))

DEFUN_LOCAL_VOID (Frustum,
    (void *rend, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar),
    (left, right, bottom, top, zNear, zFar))

DEFUN_LOCAL_VOID (LoadIdentity,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (LoadMatrixf,
    (void *rend, const GLfloat * m),
    (m))

DEFUN_LOCAL_VOID (LoadMatrixd,
    (void *rend, const GLdouble * m),
    (m))

DEFUN_LOCAL_VOID (MatrixMode,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (MultMatrixf,
    (void *rend, const GLfloat * m),
    (m))

DEFUN_LOCAL_VOID (MultMatrixd,
    (void *rend, const GLdouble * m),
    (m))

DEFUN_LOCAL_VOID (Ortho,
    (void *rend, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar),
    (left, right, bottom, top, zNear, zFar))

DEFUN_LOCAL_VOID (PopMatrix,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (PushMatrix,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (Rotated,
    (void *rend, GLdouble angle, GLdouble x, GLdouble y, GLdouble z),
    (angle, x, y, z))

DEFUN_LOCAL_VOID (Rotatef,
    (void *rend, GLfloat angle, GLfloat x, GLfloat y, GLfloat z),
    (angle, x, y, z))

DEFUN_LOCAL_VOID (Scaled,
    (void *rend, GLdouble x, GLdouble y, GLdouble z),
    (x, y, z))

DEFUN_LOCAL_VOID (Scalef,
    (void *rend, GLfloat x, GLfloat y, GLfloat z),
    (x, y, z))

DEFUN_LOCAL_VOID (Translated,
    (void *rend, GLdouble x, GLdouble y, GLdouble z),
    (x, y, z))

DEFUN_LOCAL_VOID (Translatef,
    (void *rend, GLfloat x, GLfloat y, GLfloat z),
    (x, y, z))

DEFUN_LOCAL_VOID (Viewport,
    (void *rend, GLint x, GLint y, GLsizei width, GLsizei height),
    (x, y, width, height))

DEFUN_LOCAL_VOID (ArrayElement,
    (void *rend, GLint i),
    (i))

DEFUN_LOCAL_VOID (BindTexture,
    (void *rend, GLenum target, GLenum texture),
    (target, texture))

DEFUN_LOCAL_VOID (ColorPointer,
    (void *rend, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer),
    (size, type, stride, pointer))

DEFUN_LOCAL_VOID (DisableClientState,
    (void *rend, GLenum array),
    (array))

DEFUN_LOCAL_VOID (DrawArrays,
    (void *rend, GLenum mode, GLint first, GLsizei count),
    (mode, first, count))

DEFUN_LOCAL_VOID (DrawElements,
    (void *rend, GLenum mode, GLsizei count, GLenum type, const GLvoid * indices),
    (mode, count, type, indices))

DEFUN_LOCAL_VOID (EdgeFlagPointer,
    (void *rend, GLsizei stride, const GLvoid * pointer),
    (stride, pointer))

DEFUN_LOCAL_VOID (EnableClientState,
    (void *rend, GLenum array),
    (array))

DEFUN_LOCAL_VOID (IndexPointer,
    (void *rend, GLenum type, GLsizei stride, const GLvoid * pointer),
    (type, stride, pointer))

DEFUN_LOCAL_VOID (Indexub,
    (void *rend, GLubyte c),
    (c))

DEFUN_LOCAL_VOID (Indexubv,
    (void *rend, const GLubyte * c),
    (c))

DEFUN_LOCAL_VOID (InterleavedArrays,
    (void *rend, GLenum format, GLsizei stride, const GLvoid * pointer),
    (format, stride, pointer))

DEFUN_LOCAL_VOID (NormalPointer,
    (void *rend, GLenum type, GLsizei stride, const GLvoid * pointer),
    (type, stride, pointer))

DEFUN_LOCAL_VOID (PolygonOffset,
    (void *rend, GLfloat factor, GLfloat units),
    (factor, units))

DEFUN_LOCAL_VOID (TexCoordPointer,
    (void *rend, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer),
    (size, type, stride, pointer))

DEFUN_LOCAL_VOID (VertexPointer,
    (void *rend, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer),
    (size, type, stride, pointer))

DEFUN_LOCAL (GLboolean, AreTexturesResident,
    (void *rend, GLsizei n, const GLenum * textures, GLboolean * residences),
    (n, textures, residences))

DEFUN_LOCAL_VOID (CopyTexImage1D,
    (void *rend, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border),
    (target, level, internalformat, x, y, width, border))

DEFUN_LOCAL_VOID (CopyTexImage2D,
    (void *rend, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border),
    (target, level, internalformat, x, y, width, height, border))

DEFUN_LOCAL_VOID (CopyTexSubImage1D,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width),
    (target, level, xoffset, x, y, width))

DEFUN_LOCAL_VOID (CopyTexSubImage2D,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height),
    (target, level, xoffset, yoffset, x, y, width, height))

DEFUN_LOCAL_VOID (DeleteTextures,
    (void *rend, GLsizei n, const GLenum * textures),
    (n, textures))

DEFUN_LOCAL_VOID (GenTextures,
    (void *rend, GLsizei n, GLenum * textures),
    (n, textures))

DEFUN_LOCAL_VOID (GetPointerv,
    (void *rend, GLenum pname, GLvoid * * params),
    (pname, params))

DEFUN_LOCAL (GLboolean, IsTexture,
    (void *rend, GLenum texture),
    (texture))

DEFUN_LOCAL_VOID (PrioritizeTextures,
    (void *rend, GLsizei n, const GLenum * textures, const GLclampf * priorities),
    (n, textures, priorities))

DEFUN_LOCAL_VOID (TexSubImage1D,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, width, format, type, pixels))

DEFUN_LOCAL_VOID (TexSubImage2D,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, width, height, format, type, pixels))

DEFUN_LOCAL_VOID (PopClientAttrib,
    (void *rend),
    ())

DEFUN_LOCAL_VOID (PushClientAttrib,
    (void *rend, GLbitfield mask),
    (mask))

DEFUN_LOCAL_VOID (BlendColor,
    (void *rend, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha),
    (red, green, blue, alpha))

DEFUN_LOCAL_VOID (BlendEquation,
    (void *rend, GLenum mode),
    (mode))

DEFUN_LOCAL_VOID (DrawRangeElements,
    (void *rend, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid * indices),
    (mode, start, end, count, type, indices))

DEFUN_LOCAL_VOID (ColorTable,
    (void *rend, GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * table),
    (target, internalformat, width, format, type, table))

DEFUN_LOCAL_VOID (ColorTableParameterfv,
    (void *rend, GLenum target, GLenum pname, const GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (ColorTableParameteriv,
    (void *rend, GLenum target, GLenum pname, const GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (CopyColorTable,
    (void *rend, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width),
    (target, internalformat, x, y, width))

DEFUN_LOCAL_VOID (GetColorTable,
    (void *rend, GLenum target, GLenum format, GLenum type, GLvoid * table),
    (target, format, type, table))

DEFUN_LOCAL_VOID (GetColorTableParameterfv,
    (void *rend, GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetColorTableParameteriv,
    (void *rend, GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (ColorSubTable,
    (void *rend, GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid * data),
    (target, start, count, format, type, data))

DEFUN_LOCAL_VOID (CopyColorSubTable,
    (void *rend, GLenum target, GLsizei start, GLint x, GLint y, GLsizei width),
    (target, start, x, y, width))

DEFUN_LOCAL_VOID (ConvolutionFilter1D,
    (void *rend, GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * image),
    (target, internalformat, width, format, type, image))

DEFUN_LOCAL_VOID (ConvolutionFilter2D,
    (void *rend, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * image),
    (target, internalformat, width, height, format, type, image))

DEFUN_LOCAL_VOID (ConvolutionParameterf,
    (void *rend, GLenum target, GLenum pname, GLfloat params),
    (target, pname, params))

DEFUN_LOCAL_VOID (ConvolutionParameterfv,
    (void *rend, GLenum target, GLenum pname, const GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (ConvolutionParameteri,
    (void *rend, GLenum target, GLenum pname, GLint params),
    (target, pname, params))

DEFUN_LOCAL_VOID (ConvolutionParameteriv,
    (void *rend, GLenum target, GLenum pname, const GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (CopyConvolutionFilter1D,
    (void *rend, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width),
    (target, internalformat, x, y, width))

DEFUN_LOCAL_VOID (CopyConvolutionFilter2D,
    (void *rend, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height),
    (target, internalformat, x, y, width, height))

DEFUN_LOCAL_VOID (GetConvolutionFilter,
    (void *rend, GLenum target, GLenum format, GLenum type, GLvoid * image),
    (target, format, type, image))

DEFUN_LOCAL_VOID (GetConvolutionParameterfv,
    (void *rend, GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetConvolutionParameteriv,
    (void *rend, GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetSeparableFilter,
    (void *rend, GLenum target, GLenum format, GLenum type, GLvoid * row, GLvoid * column, GLvoid * span),
    (target, format, type, row, column, span))

DEFUN_LOCAL_VOID (SeparableFilter2D,
    (void *rend, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * row, const GLvoid * column),
    (target, internalformat, width, height, format, type, row, column))

DEFUN_LOCAL_VOID (GetHistogram,
    (void *rend, GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values),
    (target, reset, format, type, values))

DEFUN_LOCAL_VOID (GetHistogramParameterfv,
    (void *rend, GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetHistogramParameteriv,
    (void *rend, GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetMinmax,
    (void *rend, GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values),
    (target, reset, format, type, values))

DEFUN_LOCAL_VOID (GetMinmaxParameterfv,
    (void *rend, GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (GetMinmaxParameteriv,
    (void *rend, GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_LOCAL_VOID (Histogram,
    (void *rend, GLenum target, GLsizei width, GLenum internalformat, GLboolean sink),
    (target, width, internalformat, sink))

DEFUN_LOCAL_VOID (Minmax,
    (void *rend, GLenum target, GLenum internalformat, GLboolean sink),
    (target, internalformat, sink))

DEFUN_LOCAL_VOID (ResetHistogram,
    (void *rend, GLenum target),
    (target))

DEFUN_LOCAL_VOID (ResetMinmax,
    (void *rend, GLenum target),
    (target))

DEFUN_LOCAL_VOID (TexImage3D,
    (void *rend, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalformat, width, height, depth, border, format, type, pixels))

DEFUN_LOCAL_VOID (TexSubImage3D,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))

DEFUN_LOCAL_VOID (CopyTexSubImage3D,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height),
    (target, level, xoffset, yoffset, zoffset, x, y, width, height))

DEFUN_LOCAL_VOID (ActiveTextureARB,
    (void *rend, GLenum texture),
    (texture))

DEFUN_LOCAL_VOID (ClientActiveTextureARB,
    (void *rend, GLenum texture),
    (texture))

DEFUN_LOCAL_VOID (MultiTexCoord1dARB,
    (void *rend, GLenum target, GLdouble s),
    (target, s))

DEFUN_LOCAL_VOID (MultiTexCoord1dvARB,
    (void *rend, GLenum target, const GLdouble * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord1fARB,
    (void *rend, GLenum target, GLfloat s),
    (target, s))

DEFUN_LOCAL_VOID (MultiTexCoord1fvARB,
    (void *rend, GLenum target, const GLfloat * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord1iARB,
    (void *rend, GLenum target, GLint s),
    (target, s))

DEFUN_LOCAL_VOID (MultiTexCoord1ivARB,
    (void *rend, GLenum target, const GLint * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord1sARB,
    (void *rend, GLenum target, GLshort s),
    (target, s))

DEFUN_LOCAL_VOID (MultiTexCoord1svARB,
    (void *rend, GLenum target, const GLshort * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord2dARB,
    (void *rend, GLenum target, GLdouble s, GLdouble t),
    (target, s, t))

DEFUN_LOCAL_VOID (MultiTexCoord2dvARB,
    (void *rend, GLenum target, const GLdouble * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord2fARB,
    (void *rend, GLenum target, GLfloat s, GLfloat t),
    (target, s, t))

DEFUN_LOCAL_VOID (MultiTexCoord2fvARB,
    (void *rend, GLenum target, const GLfloat * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord2iARB,
    (void *rend, GLenum target, GLint s, GLint t),
    (target, s, t))

DEFUN_LOCAL_VOID (MultiTexCoord2ivARB,
    (void *rend, GLenum target, const GLint * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord2sARB,
    (void *rend, GLenum target, GLshort s, GLshort t),
    (target, s, t))

DEFUN_LOCAL_VOID (MultiTexCoord2svARB,
    (void *rend, GLenum target, const GLshort * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord3dARB,
    (void *rend, GLenum target, GLdouble s, GLdouble t, GLdouble r),
    (target, s, t, r))

DEFUN_LOCAL_VOID (MultiTexCoord3dvARB,
    (void *rend, GLenum target, const GLdouble * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord3fARB,
    (void *rend, GLenum target, GLfloat s, GLfloat t, GLfloat r),
    (target, s, t, r))

DEFUN_LOCAL_VOID (MultiTexCoord3fvARB,
    (void *rend, GLenum target, const GLfloat * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord3iARB,
    (void *rend, GLenum target, GLint s, GLint t, GLint r),
    (target, s, t, r))

DEFUN_LOCAL_VOID (MultiTexCoord3ivARB,
    (void *rend, GLenum target, const GLint * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord3sARB,
    (void *rend, GLenum target, GLshort s, GLshort t, GLshort r),
    (target, s, t, r))

DEFUN_LOCAL_VOID (MultiTexCoord3svARB,
    (void *rend, GLenum target, const GLshort * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord4dARB,
    (void *rend, GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q),
    (target, s, t, r, q))

DEFUN_LOCAL_VOID (MultiTexCoord4dvARB,
    (void *rend, GLenum target, const GLdouble * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord4fARB,
    (void *rend, GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q),
    (target, s, t, r, q))

DEFUN_LOCAL_VOID (MultiTexCoord4fvARB,
    (void *rend, GLenum target, const GLfloat * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord4iARB,
    (void *rend, GLenum target, GLint s, GLint t, GLint r, GLint q),
    (target, s, t, r, q))

DEFUN_LOCAL_VOID (MultiTexCoord4ivARB,
    (void *rend, GLenum target, const GLint * v),
    (target, v))

DEFUN_LOCAL_VOID (MultiTexCoord4sARB,
    (void *rend, GLenum target, GLshort s, GLshort t, GLshort r, GLshort q),
    (target, s, t, r, q))

DEFUN_LOCAL_VOID (MultiTexCoord4svARB,
    (void *rend, GLenum target, const GLshort * v),
    (target, v))

DEFUN_LOCAL_VOID (LoadTransposeMatrixfARB,
    (void *rend, const GLfloat * m),
    (m))

DEFUN_LOCAL_VOID (LoadTransposeMatrixdARB,
    (void *rend, const GLdouble * m),
    (m))

DEFUN_LOCAL_VOID (MultTransposeMatrixfARB,
    (void *rend, const GLfloat * m),
    (m))

DEFUN_LOCAL_VOID (MultTransposeMatrixdARB,
    (void *rend, const GLdouble * m),
    (m))

DEFUN_LOCAL_VOID (SampleCoverageARB,
    (void *rend, GLclampf value, GLboolean invert),
    (value, invert))

DEFUN_ALIAS_VOID (PolygonOffsetEXT, PolygonOffset,
    (GLfloat factor, GLfloat bias),
    (factor, bias))

DEFUN_EXTERN_VOID (GetTexFilterFuncSGIS,
    (GLenum target, GLenum filter, GLfloat * weights),
    (target, filter, weights))

DEFUN_EXTERN_VOID (TexFilterFuncSGIS,
    (GLenum target, GLenum filter, GLsizei n, const GLfloat * weights),
    (target, filter, n, weights))

DEFUN_ALIAS_VOID (GetHistogramEXT, GetHistogram,
    (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values),
    (target, reset, format, type, values))

DEFUN_ALIAS_VOID (GetHistogramParameterfvEXT, GetHistogramParameterfv,
    (GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetHistogramParameterivEXT, GetHistogramParameteriv,
    (GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetMinmaxEXT, GetMinmax,
    (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values),
    (target, reset, format, type, values))

DEFUN_ALIAS_VOID (GetMinmaxParameterfvEXT, GetMinmaxParameterfv,
    (GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetMinmaxParameterivEXT, GetMinmaxParameteriv,
    (GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetConvolutionFilterEXT, GetConvolutionFilter,
    (GLenum target, GLenum format, GLenum type, GLvoid * image),
    (target, format, type, image))

DEFUN_ALIAS_VOID (GetConvolutionParameterfvEXT, GetConvolutionParameterfv,
    (GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetConvolutionParameterivEXT, GetConvolutionParameteriv,
    (GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetSeparableFilterEXT, GetSeparableFilter,
    (GLenum target, GLenum format, GLenum type, GLvoid * row, GLvoid * column, GLvoid * span),
    (target, format, type, row, column, span))

DEFUN_ALIAS_VOID (GetColorTableSGI, GetColorTable,
    (GLenum target, GLenum format, GLenum type, GLvoid * table),
    (target, format, type, table))

DEFUN_ALIAS_VOID (GetColorTableParameterfvSGI, GetColorTableParameterfv,
    (GLenum target, GLenum pname, GLfloat * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (GetColorTableParameterivSGI, GetColorTableParameteriv,
    (GLenum target, GLenum pname, GLint * params),
    (target, pname, params))

DEFUN_EXTERN_VOID (PixelTexGenSGIX,
    (GLenum mode),
    (mode))

DEFUN_EXTERN_VOID (PixelTexGenParameteriSGIS,
    (GLenum pname, GLint param),
    (pname, param))

DEFUN_EXTERN_VOID (PixelTexGenParameterivSGIS,
    (GLenum pname, const GLint * params),
    (pname, params))

DEFUN_EXTERN_VOID (PixelTexGenParameterfSGIS,
    (GLenum pname, GLfloat param),
    (pname, param))

DEFUN_EXTERN_VOID (PixelTexGenParameterfvSGIS,
    (GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_EXTERN_VOID (GetPixelTexGenParameterivSGIS,
    (GLenum pname, GLint * params),
    (pname, params))

DEFUN_EXTERN_VOID (GetPixelTexGenParameterfvSGIS,
    (GLenum pname, GLfloat * params),
    (pname, params))

DEFUN_EXTERN_VOID (TexImage4DSGIS,
    (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLsizei size4d, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalformat, width, height, depth, size4d, border, format, type, pixels))

DEFUN_EXTERN_VOID (TexSubImage4DSGIS,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint woffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei size4d, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, zoffset, woffset, width, height, depth, size4d, format, type, pixels))

DEFUN_ALIAS (GLboolean, AreTexturesResidentEXT, AreTexturesResident,
    (GLsizei n, const GLenum * textures, GLboolean * residences),
    (n, textures, residences))

DEFUN_ALIAS_VOID (GenTexturesEXT, GenTextures,
    (GLsizei n, GLenum * textures),
    (n, textures))

DEFUN_EXTERN (GLboolean, IsTextureEXT,
    (GLenum texture),
    (texture))

DEFUN_EXTERN_VOID (DetailTexFuncSGIS,
    (GLenum target, GLsizei n, const GLfloat * points),
    (target, n, points))

DEFUN_EXTERN_VOID (GetDetailTexFuncSGIS,
    (GLenum target, GLfloat * points),
    (target, points))

DEFUN_EXTERN_VOID (SharpenTexFuncSGIS,
    (GLenum target, GLsizei n, const GLfloat * points),
    (target, n, points))

DEFUN_EXTERN_VOID (GetSharpenTexFuncSGIS,
    (GLenum target, GLfloat * points),
    (target, points))

DEFUN_EXTERN_VOID (SampleMaskSGIS,
    (GLclampf value, GLboolean invert),
    (value, invert))

DEFUN_EXTERN_VOID (SamplePatternSGIS,
    (GLenum pattern),
    (pattern))

DEFUN_EXTERN_VOID (ColorPointerEXT,
    (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid * pointer),
    (size, type, stride, count, pointer))

DEFUN_EXTERN_VOID (EdgeFlagPointerEXT,
    (GLsizei stride, GLsizei count, const GLboolean * pointer),
    (stride, count, pointer))

DEFUN_EXTERN_VOID (IndexPointerEXT,
    (GLenum type, GLsizei stride, GLsizei count, const GLvoid * pointer),
    (type, stride, count, pointer))

DEFUN_EXTERN_VOID (NormalPointerEXT,
    (GLenum type, GLsizei stride, GLsizei count, const GLvoid * pointer),
    (type, stride, count, pointer))

DEFUN_EXTERN_VOID (TexCoordPointerEXT,
    (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid * pointer),
    (size, type, stride, count, pointer))

DEFUN_EXTERN_VOID (VertexPointerEXT,
    (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid * pointer),
    (size, type, stride, count, pointer))

DEFUN_EXTERN_VOID (SpriteParameterfSGIX,
    (GLenum pname, GLfloat param),
    (pname, param))

DEFUN_EXTERN_VOID (SpriteParameterfvSGIX,
    (GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_EXTERN_VOID (SpriteParameteriSGIX,
    (GLenum pname, GLint param),
    (pname, param))

DEFUN_EXTERN_VOID (SpriteParameterivSGIX,
    (GLenum pname, const GLint * params),
    (pname, params))

DEFUN_LOCAL_VOID (PointParameterfEXT,
    (void *rend, GLenum pname, GLfloat param),
    (pname, param))

DEFUN_ALIAS_VOID (PointParameterfEXT, PointParameterf,
    (GLenum pname, GLfloat param),
    (pname, param))

DEFUN_LOCAL_VOID (PointParameterfvEXT,
    (void *rend, GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_ALIAS_VOID (PointParameterfvEXT, PointParameterfv,
    (GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_EXTERN (GLint, GetInstrumentsSGIX,
    (),
    ())

DEFUN_EXTERN_VOID (InstrumentsBufferSGIX,
    (GLsizei size, GLint * buffer),
    (size, buffer))

DEFUN_EXTERN (GLint, PollInstrumentsSGIX,
    (GLint * marker_p),
    (marker_p))

DEFUN_EXTERN_VOID (ReadInstrumentsSGIX,
    (GLint marker),
    (marker))

DEFUN_EXTERN_VOID (StartInstrumentsSGIX,
    (),
    ())

DEFUN_EXTERN_VOID (StopInstrumentsSGIX,
    (GLint marker),
    (marker))

DEFUN_EXTERN_VOID (FrameZoomSGIX,
    (GLint factor),
    (factor))

DEFUN_EXTERN_VOID (TagSampleBufferSGIX,
    (),
    ())

DEFUN_EXTERN_VOID (ReferencePlaneSGIX,
    (const GLdouble * equation),
    (equation))

DEFUN_EXTERN_VOID (FlushRasterSGIX,
    (),
    ())

DEFUN_EXTERN_VOID (GetListParameterfvSGIX,
    (GLuint list, GLenum pname, GLfloat * params),
    (list, pname, params))

DEFUN_EXTERN_VOID (GetListParameterivSGIX,
    (GLuint list, GLenum pname, GLint * params),
    (list, pname, params))

DEFUN_EXTERN_VOID (ListParameterfSGIX,
    (GLuint list, GLenum pname, GLfloat param),
    (list, pname, param))

DEFUN_EXTERN_VOID (ListParameterfvSGIX,
    (GLuint list, GLenum pname, const GLfloat * params),
    (list, pname, params))

DEFUN_EXTERN_VOID (ListParameteriSGIX,
    (GLuint list, GLenum pname, GLint param),
    (list, pname, param))

DEFUN_EXTERN_VOID (ListParameterivSGIX,
    (GLuint list, GLenum pname, const GLint * params),
    (list, pname, params))

DEFUN_EXTERN_VOID (FragmentColorMaterialSGIX,
    (GLenum face, GLenum mode),
    (face, mode))

DEFUN_EXTERN_VOID (FragmentLightfSGIX,
    (GLenum light, GLenum pname, GLfloat param),
    (light, pname, param))

DEFUN_EXTERN_VOID (FragmentLightfvSGIX,
    (GLenum light, GLenum pname, const GLfloat * params),
    (light, pname, params))

DEFUN_EXTERN_VOID (FragmentLightiSGIX,
    (GLenum light, GLenum pname, GLint param),
    (light, pname, param))

DEFUN_EXTERN_VOID (FragmentLightivSGIX,
    (GLenum light, GLenum pname, const GLint * params),
    (light, pname, params))

DEFUN_EXTERN_VOID (FragmentLightModelfSGIX,
    (GLenum pname, GLfloat param),
    (pname, param))

DEFUN_EXTERN_VOID (FragmentLightModelfvSGIX,
    (GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_EXTERN_VOID (FragmentLightModeliSGIX,
    (GLenum pname, GLint param),
    (pname, param))

DEFUN_EXTERN_VOID (FragmentLightModelivSGIX,
    (GLenum pname, const GLint * params),
    (pname, params))

DEFUN_EXTERN_VOID (FragmentMaterialfSGIX,
    (GLenum face, GLenum pname, GLfloat param),
    (face, pname, param))

DEFUN_EXTERN_VOID (FragmentMaterialfvSGIX,
    (GLenum face, GLenum pname, const GLfloat * params),
    (face, pname, params))

DEFUN_EXTERN_VOID (FragmentMaterialiSGIX,
    (GLenum face, GLenum pname, GLint param),
    (face, pname, param))

DEFUN_EXTERN_VOID (FragmentMaterialivSGIX,
    (GLenum face, GLenum pname, const GLint * params),
    (face, pname, params))

DEFUN_EXTERN_VOID (GetFragmentLightfvSGIX,
    (GLenum light, GLenum pname, GLfloat * params),
    (light, pname, params))

DEFUN_EXTERN_VOID (GetFragmentLightivSGIX,
    (GLenum light, GLenum pname, GLint * params),
    (light, pname, params))

DEFUN_EXTERN_VOID (GetFragmentMaterialfvSGIX,
    (GLenum face, GLenum pname, GLfloat * params),
    (face, pname, params))

DEFUN_EXTERN_VOID (GetFragmentMaterialivSGIX,
    (GLenum face, GLenum pname, GLint * params),
    (face, pname, params))

DEFUN_EXTERN_VOID (LightEnviSGIX,
    (GLenum pname, GLint param),
    (pname, param))

DEFUN_EXTERN_VOID (VertexWeightfEXT,
    (GLfloat weight),
    (weight))

DEFUN_EXTERN_VOID (VertexWeightfvEXT,
    (const GLfloat * weight),
    (weight))

DEFUN_EXTERN_VOID (VertexWeightPointerEXT,
    (GLsizei size, GLenum type, GLsizei stride, const GLvoid * pointer),
    (size, type, stride, pointer))

DEFUN_EXTERN_VOID (FlushVertexArrayRangeNV,
    (),
    ())

DEFUN_LOCAL_VOID (VertexArrayRangeNV,
    (void *rend, GLsizei length, const GLvoid * pointer),
    (length, pointer))

DEFUN_ALIAS_VOID (VertexArrayRangeNV, VertexArrayRangeAPPLE,
    (GLsizei length, const GLvoid * pointer),
    (length, pointer))

DEFUN_LOCAL_VOID (CombinerParameterfvNV,
    (void *rend, GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_LOCAL_VOID (CombinerParameterfNV,
    (void *rend, GLenum pname, GLfloat param),
    (pname, param))

DEFUN_LOCAL_VOID (CombinerParameterivNV,
    (void *rend, GLenum pname, const GLint * params),
    (pname, params))

DEFUN_LOCAL_VOID (CombinerParameteriNV,
    (void *rend, GLenum pname, GLint param),
    (pname, param))

DEFUN_LOCAL_VOID (CombinerInputNV,
    (void *rend, GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage),
    (stage, portion, variable, input, mapping, componentUsage))

DEFUN_LOCAL_VOID (CombinerOutputNV,
    (void *rend, GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum),
    (stage, portion, abOutput, cdOutput, sumOutput, scale, bias, abDotProduct, cdDotProduct, muxSum))

DEFUN_LOCAL_VOID (FinalCombinerInputNV,
    (void *rend, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage),
    (variable, input, mapping, componentUsage))

DEFUN_LOCAL_VOID (GetCombinerInputParameterfvNV,
    (void *rend, GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat * params),
    (stage, portion, variable, pname, params))

DEFUN_LOCAL_VOID (GetCombinerInputParameterivNV,
    (void *rend, GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint * params),
    (stage, portion, variable, pname, params))

DEFUN_LOCAL_VOID (GetCombinerOutputParameterfvNV,
    (void *rend, GLenum stage, GLenum portion, GLenum pname, GLfloat * params),
    (stage, portion, pname, params))

DEFUN_LOCAL_VOID (GetCombinerOutputParameterivNV,
    (void *rend, GLenum stage, GLenum portion, GLenum pname, GLint * params),
    (stage, portion, pname, params))

DEFUN_LOCAL_VOID (GetFinalCombinerInputParameterfvNV,
    (void *rend, GLenum variable, GLenum pname, GLfloat * params),
    (variable, pname, params))

DEFUN_LOCAL_VOID (GetFinalCombinerInputParameterivNV,
    (void *rend, GLenum variable, GLenum pname, GLint * params),
    (variable, pname, params))

DEFUN_EXTERN_VOID (ResizeBuffersMESA,
    (),
    ())

DEFUN_EXTERN_VOID (WindowPos2dMESA,
    (GLdouble x, GLdouble y),
    (x, y))

DEFUN_EXTERN_VOID (WindowPos2dvMESA,
    (const GLdouble * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos2fMESA,
    (GLfloat x, GLfloat y),
    (x, y))

DEFUN_EXTERN_VOID (WindowPos2fvMESA,
    (const GLfloat * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos2iMESA,
    (GLint x, GLint y),
    (x, y))

DEFUN_EXTERN_VOID (WindowPos2ivMESA,
    (const GLint * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos2sMESA,
    (GLshort x, GLshort y),
    (x, y))

DEFUN_EXTERN_VOID (WindowPos2svMESA,
    (const GLshort * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos3dMESA,
    (GLdouble x, GLdouble y, GLdouble z),
    (x, y, z))

DEFUN_EXTERN_VOID (WindowPos3dvMESA,
    (const GLdouble * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos3fMESA,
    (GLfloat x, GLfloat y, GLfloat z),
    (x, y, z))

DEFUN_EXTERN_VOID (WindowPos3fvMESA,
    (const GLfloat * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos3iMESA,
    (GLint x, GLint y, GLint z),
    (x, y, z))

DEFUN_EXTERN_VOID (WindowPos3ivMESA,
    (const GLint * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos3sMESA,
    (GLshort x, GLshort y, GLshort z),
    (x, y, z))

DEFUN_EXTERN_VOID (WindowPos3svMESA,
    (const GLshort * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos4dMESA,
    (GLdouble x, GLdouble y, GLdouble z, GLdouble w),
    (x, y, z, w))

DEFUN_EXTERN_VOID (WindowPos4dvMESA,
    (const GLdouble * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos4fMESA,
    (GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (x, y, z, w))

DEFUN_EXTERN_VOID (WindowPos4fvMESA,
    (const GLfloat * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos4iMESA,
    (GLint x, GLint y, GLint z, GLint w),
    (x, y, z, w))

DEFUN_EXTERN_VOID (WindowPos4ivMESA,
    (const GLint * v),
    (v))

DEFUN_EXTERN_VOID (WindowPos4sMESA,
    (GLshort x, GLshort y, GLshort z, GLshort w),
    (x, y, z, w))

DEFUN_EXTERN_VOID (WindowPos4svMESA,
    (const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (BlendFuncSeparateEXT,
    (void *rend, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),
    (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))

DEFUN_EXTERN_VOID (IndexMaterialEXT,
    (GLenum face, GLenum mode),
    (face, mode))

DEFUN_EXTERN_VOID (IndexFuncEXT,
    (GLenum func, GLclampf ref),
    (func, ref))

DEFUN_LOCAL_VOID (LockArraysEXT,
    (void *rend, GLint first, GLsizei count),
    (first, count))

DEFUN_LOCAL_VOID (UnlockArraysEXT,
    (void *rend),
    ())

DEFUN_EXTERN_VOID (CullParameterdvEXT,
    (GLenum pname, GLdouble * params),
    (pname, params))

DEFUN_EXTERN_VOID (CullParameterfvEXT,
    (GLenum pname, GLfloat * params),
    (pname, params))

DEFUN_EXTERN_VOID (HintPGI,
    (GLenum target, GLint mode),
    (target, mode))

DEFUN_LOCAL_VOID (FogCoordfEXT,
    (void *rend, GLfloat coord),
    (coord))

DEFUN_LOCAL_VOID (FogCoordfvEXT,
    (void *rend, const GLfloat * coord),
    (coord))

DEFUN_LOCAL_VOID (FogCoorddEXT,
    (void *rend, GLdouble coord),
    (coord))

DEFUN_LOCAL_VOID (FogCoorddvEXT,
    (void *rend, const GLdouble * coord),
    (coord))

DEFUN_LOCAL_VOID (FogCoordPointerEXT,
    (void *rend, GLenum type, GLsizei stride, const GLvoid * pointer),
    (type, stride, pointer))

DEFUN_EXTERN_VOID (TbufferMask3DFX,
    (GLuint mask),
    (mask))

DEFUN_LOCAL_VOID (CompressedTexImage3DARB,
    (void *rend, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * data),
    (target, level, internalformat, width, height, depth, border, imageSize, data))

DEFUN_LOCAL_VOID (CompressedTexImage2DARB,
    (void *rend, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * data),
    (target, level, internalformat, width, height, border, imageSize, data))

DEFUN_LOCAL_VOID (CompressedTexImage1DARB,
    (void *rend, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid * data),
    (target, level, internalformat, width, border, imageSize, data))

DEFUN_LOCAL_VOID (CompressedTexSubImage3DARB,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid * data),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data))

DEFUN_LOCAL_VOID (CompressedTexSubImage2DARB,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid * data),
    (target, level, xoffset, yoffset, width, height, format, imageSize, data))

DEFUN_LOCAL_VOID (CompressedTexSubImage1DARB,
    (void *rend, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid * data),
    (target, level, xoffset, width, format, imageSize, data))

DEFUN_LOCAL_VOID (GetCompressedTexImageARB,
    (void *rend, GLenum target, GLint level, void * img),
    (target, level, img))

DEFUN_LOCAL_VOID (SecondaryColor3bEXT,
    (void *rend, GLbyte red, GLbyte green, GLbyte blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3bvEXT,
    (void *rend, const GLbyte * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3dEXT,
    (void *rend, GLdouble red, GLdouble green, GLdouble blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3dvEXT,
    (void *rend, const GLdouble * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3fEXT,
    (void *rend, GLfloat red, GLfloat green, GLfloat blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3fvEXT,
    (void *rend, const GLfloat * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3iEXT,
    (void *rend, GLint red, GLint green, GLint blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3ivEXT,
    (void *rend, const GLint * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3sEXT,
    (void *rend, GLshort red, GLshort green, GLshort blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3svEXT,
    (void *rend, const GLshort * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3ubEXT,
    (void *rend, GLubyte red, GLubyte green, GLubyte blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3ubvEXT,
    (void *rend, const GLubyte * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3uiEXT,
    (void *rend, GLuint red, GLuint green, GLuint blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3uivEXT,
    (void *rend, const GLuint * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColor3usEXT,
    (void *rend, GLushort red, GLushort green, GLushort blue),
    (red, green, blue))

DEFUN_LOCAL_VOID (SecondaryColor3usvEXT,
    (void *rend, const GLushort * v),
    (v))

DEFUN_LOCAL_VOID (SecondaryColorPointerEXT,
    (void *rend, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer),
    (size, type, stride, pointer))

DEFUN_EXTERN (GLboolean, AreProgramsResidentNV,
    (GLsizei n, const GLuint * ids, GLboolean * residences),
    (n, ids, residences))

DEFUN_EXTERN_VOID (BindProgramNV,
    (GLenum target, GLuint id),
    (target, id))

DEFUN_EXTERN_VOID (DeleteProgramsNV,
    (GLsizei n, const GLuint * ids),
    (n, ids))

DEFUN_EXTERN_VOID (ExecuteProgramNV,
    (GLenum target, GLuint id, const GLfloat * params),
    (target, id, params))

DEFUN_EXTERN_VOID (GenProgramsNV,
    (GLsizei n, GLuint * ids),
    (n, ids))

DEFUN_EXTERN_VOID (GetProgramParameterdvNV,
    (GLenum target, GLuint index, GLenum pname, GLdouble * params),
    (target, index, pname, params))

DEFUN_EXTERN_VOID (GetProgramParameterfvNV,
    (GLenum target, GLuint index, GLenum pname, GLfloat * params),
    (target, index, pname, params))

DEFUN_EXTERN_VOID (GetProgramivNV,
    (GLuint id, GLenum pname, GLint * params),
    (id, pname, params))

DEFUN_EXTERN_VOID (GetProgramStringNV,
    (GLuint id, GLenum pname, GLubyte * program),
    (id, pname, program))

DEFUN_EXTERN_VOID (GetTrackMatrixivNV,
    (GLenum target, GLuint address, GLenum pname, GLint * params),
    (target, address, pname, params))

DEFUN_EXTERN_VOID (GetVertexAttribdvNV,
    (GLuint index, GLenum pname, GLdouble * params),
    (index, pname, params))

DEFUN_EXTERN_VOID (GetVertexAttribfvNV,
    (GLuint index, GLenum pname, GLfloat * params),
    (index, pname, params))

DEFUN_EXTERN_VOID (GetVertexAttribivNV,
    (GLuint index, GLenum pname, GLint * params),
    (index, pname, params))

DEFUN_EXTERN_VOID (GetVertexAttribPointervNV,
    (GLuint index, GLenum pname, GLvoid ** pointer),
    (index, pname, pointer))

DEFUN_EXTERN (GLboolean, IsProgramNV,
    (GLuint id),
    (id))

DEFUN_EXTERN_VOID (LoadProgramNV,
    (GLenum target, GLuint id, GLsizei len, const GLubyte * program),
    (target, id, len, program))

DEFUN_EXTERN_VOID (ProgramParameter4dNV,
    (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w),
    (target, index, x, y, z, w))

DEFUN_EXTERN_VOID (ProgramParameter4dvNV,
    (GLenum target, GLuint index, const GLdouble * params),
    (target, index, params))

DEFUN_EXTERN_VOID (ProgramParameter4fNV,
    (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (target, index, x, y, z, w))

DEFUN_EXTERN_VOID (ProgramParameter4fvNV,
    (GLenum target, GLuint index, const GLfloat * params),
    (target, index, params))

DEFUN_EXTERN_VOID (ProgramParameters4dvNV,
    (GLenum target, GLuint index, GLuint num, const GLdouble * params),
    (target, index, num, params))

DEFUN_EXTERN_VOID (ProgramParameters4fvNV,
    (GLenum target, GLuint index, GLuint num, const GLfloat * params),
    (target, index, num, params))

DEFUN_EXTERN_VOID (RequestResidentProgramsNV,
    (GLsizei n, const GLuint * ids),
    (n, ids))

DEFUN_EXTERN_VOID (TrackMatrixNV,
    (GLenum target, GLuint address, GLenum matrix, GLenum transform),
    (target, address, matrix, transform))

DEFUN_EXTERN_VOID (VertexAttribPointerNV,
    (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer),
    (index, size, type, stride, pointer))

DEFUN_EXTERN_VOID (VertexAttrib1dNV,
    (GLuint index, GLdouble x),
    (index, x))

DEFUN_EXTERN_VOID (VertexAttrib1dvNV,
    (GLuint index, const GLdouble * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib1fNV,
    (GLuint index, GLfloat x),
    (index, x))

DEFUN_EXTERN_VOID (VertexAttrib1fvNV,
    (GLuint index, const GLfloat * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib1sNV,
    (GLuint index, GLshort x),
    (index, x))

DEFUN_EXTERN_VOID (VertexAttrib1svNV,
    (GLuint index, const GLshort * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib2dNV,
    (GLuint index, GLdouble x, GLdouble y),
    (index, x, y))

DEFUN_EXTERN_VOID (VertexAttrib2dvNV,
    (GLuint index, const GLdouble * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib2fNV,
    (GLuint index, GLfloat x, GLfloat y),
    (index, x, y))

DEFUN_EXTERN_VOID (VertexAttrib2fvNV,
    (GLuint index, const GLfloat * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib2sNV,
    (GLuint index, GLshort x, GLshort y),
    (index, x, y))

DEFUN_EXTERN_VOID (VertexAttrib2svNV,
    (GLuint index, const GLshort * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib3dNV,
    (GLuint index, GLdouble x, GLdouble y, GLdouble z),
    (index, x, y, z))

DEFUN_EXTERN_VOID (VertexAttrib3dvNV,
    (GLuint index, const GLdouble * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib3fNV,
    (GLuint index, GLfloat x, GLfloat y, GLfloat z),
    (index, x, y, z))

DEFUN_EXTERN_VOID (VertexAttrib3fvNV,
    (GLuint index, const GLfloat * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib3sNV,
    (GLuint index, GLshort x, GLshort y, GLshort z),
    (index, x, y, z))

DEFUN_EXTERN_VOID (VertexAttrib3svNV,
    (GLuint index, const GLshort * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib4dNV,
    (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w),
    (index, x, y, z, w))

DEFUN_EXTERN_VOID (VertexAttrib4dvNV,
    (GLuint index, const GLdouble * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib4fNV,
    (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (index, x, y, z, w))

DEFUN_EXTERN_VOID (VertexAttrib4fvNV,
    (GLuint index, const GLfloat * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib4sNV,
    (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w),
    (index, x, y, z, w))

DEFUN_EXTERN_VOID (VertexAttrib4svNV,
    (GLuint index, const GLshort * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttrib4ubNV,
    (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w),
    (index, x, y, z, w))

DEFUN_EXTERN_VOID (VertexAttrib4ubvNV,
    (GLuint index, const GLubyte * v),
    (index, v))

DEFUN_EXTERN_VOID (VertexAttribs1dvNV,
    (GLuint index, GLsizei n, const GLdouble * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs1fvNV,
    (GLuint index, GLsizei n, const GLfloat * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs1svNV,
    (GLuint index, GLsizei n, const GLshort * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs2dvNV,
    (GLuint index, GLsizei n, const GLdouble * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs2fvNV,
    (GLuint index, GLsizei n, const GLfloat * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs2svNV,
    (GLuint index, GLsizei n, const GLshort * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs3dvNV,
    (GLuint index, GLsizei n, const GLdouble * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs3fvNV,
    (GLuint index, GLsizei n, const GLfloat * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs3svNV,
    (GLuint index, GLsizei n, const GLshort * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs4dvNV,
    (GLuint index, GLsizei n, const GLdouble * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs4fvNV,
    (GLuint index, GLsizei n, const GLfloat * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs4svNV,
    (GLuint index, GLsizei n, const GLshort * v),
    (index, n, v))

DEFUN_EXTERN_VOID (VertexAttribs4ubvNV,
    (GLuint index, GLsizei n, const GLubyte * v),
    (index, n, v))

DEFUN_LOCAL_VOID (PointParameteriNV,
    (void *rend, GLenum pname, GLint params),
    (pname, params))

DEFUN_LOCAL_VOID (PointParameterivNV,
    (void *rend, GLenum pname, const GLint * params),
    (pname, params))

DEFUN_LOCAL_VOID (MultiDrawArraysEXT,
    (void *rend, GLenum mode, GLint * first, GLsizei * count, GLsizei primcount),
    (mode, first, count, primcount))

DEFUN_LOCAL_VOID (MultiDrawElementsEXT,
    (void *rend, GLenum mode, const GLsizei * count, GLenum type, const GLvoid ** indices, GLsizei primcount),
    (mode, count, type, indices, primcount))

DEFUN_LOCAL_VOID (ActiveStencilFaceEXT,
    (void *rend, GLenum face),
    (face))

DEFUN_EXTERN_VOID (DeleteFencesNV,
    (GLsizei n, const GLuint * fences),
    (n, fences))

DEFUN_EXTERN_VOID (GenFencesNV,
    (GLsizei n, GLuint * fences),
    (n, fences))

DEFUN_EXTERN (GLboolean, IsFenceNV,
    (GLuint fence),
    (fence))

DEFUN_EXTERN (GLboolean, TestFenceNV,
    (GLuint fence),
    (fence))

DEFUN_EXTERN_VOID (GetFenceivNV,
    (GLuint fence, GLenum pname, GLint * params),
    (fence, pname, params))

DEFUN_EXTERN_VOID (FinishFenceNV,
    (GLuint fence),
    (fence))

DEFUN_EXTERN_VOID (SetFenceNV,
    (GLuint fence, GLenum condition),
    (fence, condition))

DEFUN_ALIAS_VOID (ArrayElementEXT, ArrayElement,
    (GLint i),
    (i))

DEFUN_ALIAS_VOID (BindTextureEXT, BindTexture,
    (GLenum target, GLuint texture),
    (target, texture))

DEFUN_ALIAS_VOID (BlendFuncSeparateINGR, BlendFuncSeparateEXT,
    (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),
    (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))

DEFUN_ALIAS_VOID (ColorTableParameterfvSGI, ColorTableParameterfv,
    (GLenum target, GLenum pname, const GLfloat * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (ColorTableParameterivSGI, ColorTableParameteriv,
    (GLenum target, GLenum pname, const GLint * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (ColorTableSGI, ColorTable,
    (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * table),
    (target, internalformat, width, format, type, table))

DEFUN_ALIAS_VOID (ConvolutionFilter1DEXT, ConvolutionFilter1D,
    (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * image),
    (target, internalformat, width, format, type, image))

DEFUN_ALIAS_VOID (ConvolutionFilter2DEXT, ConvolutionFilter2D,
    (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * image),
    (target, internalformat, width, height, format, type, image))

DEFUN_ALIAS_VOID (ConvolutionParameterfEXT, ConvolutionParameterf,
    (GLenum target, GLenum pname, GLfloat params),
    (target, pname, params))

DEFUN_ALIAS_VOID (ConvolutionParameterfvEXT, ConvolutionParameterfv,
    (GLenum target, GLenum pname, const GLfloat * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (ConvolutionParameteriEXT, ConvolutionParameteri,
    (GLenum target, GLenum pname, GLint params),
    (target, pname, params))

DEFUN_ALIAS_VOID (ConvolutionParameterivEXT, ConvolutionParameteriv,
    (GLenum target, GLenum pname, const GLint * params),
    (target, pname, params))

DEFUN_ALIAS_VOID (CopyColorSubTableEXT, CopyColorSubTable,
    (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width),
    (target, start, x, y, width))

DEFUN_ALIAS_VOID (CopyColorTableSGI, CopyColorTable,
    (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width),
    (target, internalformat, x, y, width))

DEFUN_ALIAS_VOID (CopyConvolutionFilter1DEXT, CopyConvolutionFilter1D,
    (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width),
    (target, internalformat, x, y, width))

DEFUN_ALIAS_VOID (CopyConvolutionFilter2DEXT, CopyConvolutionFilter2D,
    (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height),
    (target, internalformat, x, y, width, height))

DEFUN_ALIAS_VOID (CopyTexImage1DEXT, CopyTexImage1D,
    (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border),
    (target, level, internalformat, x, y, width, border))

DEFUN_ALIAS_VOID (CopyTexImage2DEXT, CopyTexImage2D,
    (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border),
    (target, level, internalformat, x, y, width, height, border))

DEFUN_ALIAS_VOID (CopyTexSubImage1DEXT, CopyTexSubImage1D,
    (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width),
    (target, level, xoffset, x, y, width))

DEFUN_ALIAS_VOID (CopyTexSubImage2DEXT, CopyTexSubImage2D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height),
    (target, level, xoffset, yoffset, x, y, width, height))

DEFUN_ALIAS_VOID (CopyTexSubImage3DEXT, CopyTexSubImage3D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height),
    (target, level, xoffset, yoffset, zoffset, x, y, width, height))

DEFUN_ALIAS_VOID (DeleteTexturesEXT, DeleteTextures,
    (GLsizei n, const GLuint *textures),
    (n, textures))

DEFUN_ALIAS_VOID (DrawArraysEXT, DrawArrays,
    (GLenum mode, GLint first, GLsizei count),
    (mode, first, count))

DEFUN_ALIAS_VOID (GetPointervEXT, GetPointerv,
    (GLenum pname, GLvoid * * params),
    (pname, params))

DEFUN_ALIAS_VOID (HistogramEXT, Histogram,
    (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink),
    (target, width, internalformat, sink))

DEFUN_ALIAS_VOID (MinmaxEXT, Minmax,
    (GLenum target, GLenum internalformat, GLboolean sink),
    (target, internalformat, sink))

DEFUN_ALIAS_VOID (PointParameterfSGIS, PointParameterf,
    (GLenum pname, GLfloat param),
    (pname, param))

DEFUN_ALIAS_VOID (PointParameterfvSGIS, PointParameterfv,
    (GLenum pname, const GLfloat * params),
    (pname, params))

DEFUN_ALIAS_VOID (PrioritizeTexturesEXT, PrioritizeTextures,
    (GLsizei n, const GLenum * textures, const GLclampf * priorities),
    (n, textures, priorities))

DEFUN_ALIAS_VOID (ResetHistogramEXT, ResetHistogram,
    (GLenum target),
    (target))

DEFUN_ALIAS_VOID (ResetMinmaxEXT, ResetMinmax,
    (GLenum target),
    (target))

DEFUN_ALIAS_VOID (SampleMaskEXT, SampleMaskSGIS,
    (GLclampf value, GLboolean invert),
    (value, invert))

DEFUN_ALIAS_VOID (SamplePatternEXT, SamplePatternSGIS,
    (GLenum pattern),
    (pattern))

DEFUN_ALIAS_VOID (SeparableFilter2DEXT, SeparableFilter2D,
    (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * row, const GLvoid * column),
    (target, internalformat, width, height, format, type, row, column))

DEFUN_ALIAS_VOID (TexImage3DEXT, TexImage3D,
    (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalformat, width, height, depth, border, format, type, pixels))

DEFUN_ALIAS_VOID (TexSubImage1DEXT, TexSubImage1D,
    (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, width, format, type, pixels))

DEFUN_ALIAS_VOID (TexSubImage2DEXT, TexSubImage2D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, width, height, format, type, pixels))

DEFUN_ALIAS_VOID (TexSubImage3DEXT, TexSubImage3D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))

#define INDIRECT_DISPATCH_INIT(d,p) \
do { \
    (d)[0] = (void *) &p ## Accum; \
    (d)[1] = (void *) &p ## AlphaFunc; \
    (d)[2] = (void *) &p ## AreTexturesResident; \
    (d)[3] = (void *) &p ## ArrayElement; \
    (d)[4] = (void *) &p ## Begin; \
    (d)[5] = (void *) &p ## BindTexture; \
    (d)[6] = (void *) &p ## Bitmap; \
    (d)[7] = (void *) &p ## BlendFunc; \
    (d)[8] = (void *) &p ## CallList; \
    (d)[9] = (void *) &p ## CallLists; \
    (d)[10] = (void *) &p ## Clear; \
    (d)[11] = (void *) &p ## ClearAccum; \
    (d)[12] = (void *) &p ## ClearColor; \
    (d)[13] = (void *) &p ## ClearDepth; \
    (d)[14] = (void *) &p ## ClearIndex; \
    (d)[15] = (void *) &p ## ClearStencil; \
    (d)[16] = (void *) &p ## ClipPlane; \
    (d)[17] = (void *) &p ## Color3b; \
    (d)[18] = (void *) &p ## Color3bv; \
    (d)[19] = (void *) &p ## Color3d; \
    (d)[20] = (void *) &p ## Color3dv; \
    (d)[21] = (void *) &p ## Color3f; \
    (d)[22] = (void *) &p ## Color3fv; \
    (d)[23] = (void *) &p ## Color3i; \
    (d)[24] = (void *) &p ## Color3iv; \
    (d)[25] = (void *) &p ## Color3s; \
    (d)[26] = (void *) &p ## Color3sv; \
    (d)[27] = (void *) &p ## Color3ub; \
    (d)[28] = (void *) &p ## Color3ubv; \
    (d)[29] = (void *) &p ## Color3ui; \
    (d)[30] = (void *) &p ## Color3uiv; \
    (d)[31] = (void *) &p ## Color3us; \
    (d)[32] = (void *) &p ## Color3usv; \
    (d)[33] = (void *) &p ## Color4b; \
    (d)[34] = (void *) &p ## Color4bv; \
    (d)[35] = (void *) &p ## Color4d; \
    (d)[36] = (void *) &p ## Color4dv; \
    (d)[37] = (void *) &p ## Color4f; \
    (d)[38] = (void *) &p ## Color4fv; \
    (d)[39] = (void *) &p ## Color4i; \
    (d)[40] = (void *) &p ## Color4iv; \
    (d)[41] = (void *) &p ## Color4s; \
    (d)[42] = (void *) &p ## Color4sv; \
    (d)[43] = (void *) &p ## Color4ub; \
    (d)[44] = (void *) &p ## Color4ubv; \
    (d)[45] = (void *) &p ## Color4ui; \
    (d)[46] = (void *) &p ## Color4uiv; \
    (d)[47] = (void *) &p ## Color4us; \
    (d)[48] = (void *) &p ## Color4usv; \
    (d)[49] = (void *) &p ## ColorMask; \
    (d)[50] = (void *) &p ## ColorMaterial; \
    (d)[51] = (void *) &p ## ColorPointer; \
    (d)[52] = (void *) &p ## CopyPixels; \
    (d)[53] = (void *) &p ## CopyTexImage1D; \
    (d)[54] = (void *) &p ## CopyTexImage2D; \
    (d)[55] = (void *) &p ## CopyTexSubImage1D; \
    (d)[56] = (void *) &p ## CopyTexSubImage2D; \
    (d)[57] = (void *) &p ## CullFace; \
    (d)[58] = (void *) &p ## DeleteLists; \
    (d)[59] = (void *) &p ## DeleteTextures; \
    (d)[60] = (void *) &p ## DepthFunc; \
    (d)[61] = (void *) &p ## DepthMask; \
    (d)[62] = (void *) &p ## DepthRange; \
    (d)[63] = (void *) &p ## Disable; \
    (d)[64] = (void *) &p ## DisableClientState; \
    (d)[65] = (void *) &p ## DrawArrays; \
    (d)[66] = (void *) &p ## DrawBuffer; \
    (d)[67] = (void *) &p ## DrawElements; \
    (d)[68] = (void *) &p ## DrawPixels; \
    (d)[69] = (void *) &p ## EdgeFlag; \
    (d)[70] = (void *) &p ## EdgeFlagPointer; \
    (d)[71] = (void *) &p ## EdgeFlagv; \
    (d)[72] = (void *) &p ## Enable; \
    (d)[73] = (void *) &p ## EnableClientState; \
    (d)[74] = (void *) &p ## End; \
    (d)[75] = (void *) &p ## EndList; \
    (d)[76] = (void *) &p ## EvalCoord1d; \
    (d)[77] = (void *) &p ## EvalCoord1dv; \
    (d)[78] = (void *) &p ## EvalCoord1f; \
    (d)[79] = (void *) &p ## EvalCoord1fv; \
    (d)[80] = (void *) &p ## EvalCoord2d; \
    (d)[81] = (void *) &p ## EvalCoord2dv; \
    (d)[82] = (void *) &p ## EvalCoord2f; \
    (d)[83] = (void *) &p ## EvalCoord2fv; \
    (d)[84] = (void *) &p ## EvalMesh1; \
    (d)[85] = (void *) &p ## EvalMesh2; \
    (d)[86] = (void *) &p ## EvalPoint1; \
    (d)[87] = (void *) &p ## EvalPoint2; \
    (d)[88] = (void *) &p ## FeedbackBuffer; \
    (d)[89] = (void *) &p ## Finish; \
    (d)[90] = (void *) &p ## Flush; \
    (d)[91] = (void *) &p ## Fogf; \
    (d)[92] = (void *) &p ## Fogfv; \
    (d)[93] = (void *) &p ## Fogi; \
    (d)[94] = (void *) &p ## Fogiv; \
    (d)[95] = (void *) &p ## FrontFace; \
    (d)[96] = (void *) &p ## Frustum; \
    (d)[97] = (void *) &p ## GenLists; \
    (d)[98] = (void *) &p ## GenTextures; \
    (d)[99] = (void *) &p ## GetBooleanv; \
    (d)[100] = (void *) &p ## GetClipPlane; \
    (d)[101] = (void *) &p ## GetDoublev; \
    (d)[102] = (void *) &p ## GetError; \
    (d)[103] = (void *) &p ## GetFloatv; \
    (d)[104] = (void *) &p ## GetIntegerv; \
    (d)[105] = (void *) &p ## GetLightfv; \
    (d)[106] = (void *) &p ## GetLightiv; \
    (d)[107] = (void *) &p ## GetMapdv; \
    (d)[108] = (void *) &p ## GetMapfv; \
    (d)[109] = (void *) &p ## GetMapiv; \
    (d)[110] = (void *) &p ## GetMaterialfv; \
    (d)[111] = (void *) &p ## GetMaterialiv; \
    (d)[112] = (void *) &p ## GetPixelMapfv; \
    (d)[113] = (void *) &p ## GetPixelMapuiv; \
    (d)[114] = (void *) &p ## GetPixelMapusv; \
    (d)[115] = (void *) &p ## GetPointerv; \
    (d)[116] = (void *) &p ## GetPolygonStipple; \
    (d)[117] = (void *) &p ## GetString; \
    (d)[118] = (void *) &p ## GetTexEnvfv; \
    (d)[119] = (void *) &p ## GetTexEnviv; \
    (d)[120] = (void *) &p ## GetTexGendv; \
    (d)[121] = (void *) &p ## GetTexGenfv; \
    (d)[122] = (void *) &p ## GetTexGeniv; \
    (d)[123] = (void *) &p ## GetTexImage; \
    (d)[124] = (void *) &p ## GetTexLevelParameterfv; \
    (d)[125] = (void *) &p ## GetTexLevelParameteriv; \
    (d)[126] = (void *) &p ## GetTexParameterfv; \
    (d)[127] = (void *) &p ## GetTexParameteriv; \
    (d)[128] = (void *) &p ## Hint; \
    (d)[129] = (void *) &p ## IndexMask; \
    (d)[130] = (void *) &p ## IndexPointer; \
    (d)[131] = (void *) &p ## Indexd; \
    (d)[132] = (void *) &p ## Indexdv; \
    (d)[133] = (void *) &p ## Indexf; \
    (d)[134] = (void *) &p ## Indexfv; \
    (d)[135] = (void *) &p ## Indexi; \
    (d)[136] = (void *) &p ## Indexiv; \
    (d)[137] = (void *) &p ## Indexs; \
    (d)[138] = (void *) &p ## Indexsv; \
    (d)[139] = (void *) &p ## Indexub; \
    (d)[140] = (void *) &p ## Indexubv; \
    (d)[141] = (void *) &p ## InitNames; \
    (d)[142] = (void *) &p ## InterleavedArrays; \
    (d)[143] = (void *) &p ## IsEnabled; \
    (d)[144] = (void *) &p ## IsList; \
    (d)[145] = (void *) &p ## IsTexture; \
    (d)[146] = (void *) &p ## LightModelf; \
    (d)[147] = (void *) &p ## LightModelfv; \
    (d)[148] = (void *) &p ## LightModeli; \
    (d)[149] = (void *) &p ## LightModeliv; \
    (d)[150] = (void *) &p ## Lightf; \
    (d)[151] = (void *) &p ## Lightfv; \
    (d)[152] = (void *) &p ## Lighti; \
    (d)[153] = (void *) &p ## Lightiv; \
    (d)[154] = (void *) &p ## LineStipple; \
    (d)[155] = (void *) &p ## LineWidth; \
    (d)[156] = (void *) &p ## ListBase; \
    (d)[157] = (void *) &p ## LoadIdentity; \
    (d)[158] = (void *) &p ## LoadMatrixd; \
    (d)[159] = (void *) &p ## LoadMatrixf; \
    (d)[160] = (void *) &p ## LoadName; \
    (d)[161] = (void *) &p ## LogicOp; \
    (d)[162] = (void *) &p ## Map1d; \
    (d)[163] = (void *) &p ## Map1f; \
    (d)[164] = (void *) &p ## Map2d; \
    (d)[165] = (void *) &p ## Map2f; \
    (d)[166] = (void *) &p ## MapGrid1d; \
    (d)[167] = (void *) &p ## MapGrid1f; \
    (d)[168] = (void *) &p ## MapGrid2d; \
    (d)[169] = (void *) &p ## MapGrid2f; \
    (d)[170] = (void *) &p ## Materialf; \
    (d)[171] = (void *) &p ## Materialfv; \
    (d)[172] = (void *) &p ## Materiali; \
    (d)[173] = (void *) &p ## Materialiv; \
    (d)[174] = (void *) &p ## MatrixMode; \
    (d)[175] = (void *) &p ## MultMatrixd; \
    (d)[176] = (void *) &p ## MultMatrixf; \
    (d)[177] = (void *) &p ## NewList; \
    (d)[178] = (void *) &p ## Normal3b; \
    (d)[179] = (void *) &p ## Normal3bv; \
    (d)[180] = (void *) &p ## Normal3d; \
    (d)[181] = (void *) &p ## Normal3dv; \
    (d)[182] = (void *) &p ## Normal3f; \
    (d)[183] = (void *) &p ## Normal3fv; \
    (d)[184] = (void *) &p ## Normal3i; \
    (d)[185] = (void *) &p ## Normal3iv; \
    (d)[186] = (void *) &p ## Normal3s; \
    (d)[187] = (void *) &p ## Normal3sv; \
    (d)[188] = (void *) &p ## NormalPointer; \
    (d)[189] = (void *) &p ## Ortho; \
    (d)[190] = (void *) &p ## PassThrough; \
    (d)[191] = (void *) &p ## PixelMapfv; \
    (d)[192] = (void *) &p ## PixelMapuiv; \
    (d)[193] = (void *) &p ## PixelMapusv; \
    (d)[194] = (void *) &p ## PixelStoref; \
    (d)[195] = (void *) &p ## PixelStorei; \
    (d)[196] = (void *) &p ## PixelTransferf; \
    (d)[197] = (void *) &p ## PixelTransferi; \
    (d)[198] = (void *) &p ## PixelZoom; \
    (d)[199] = (void *) &p ## PointSize; \
    (d)[200] = (void *) &p ## PolygonMode; \
    (d)[201] = (void *) &p ## PolygonOffset; \
    (d)[202] = (void *) &p ## PolygonStipple; \
    (d)[203] = (void *) &p ## PopAttrib; \
    (d)[204] = (void *) &p ## PopClientAttrib; \
    (d)[205] = (void *) &p ## PopMatrix; \
    (d)[206] = (void *) &p ## PopName; \
    (d)[207] = (void *) &p ## PrioritizeTextures; \
    (d)[208] = (void *) &p ## PushAttrib; \
    (d)[209] = (void *) &p ## PushClientAttrib; \
    (d)[210] = (void *) &p ## PushMatrix; \
    (d)[211] = (void *) &p ## PushName; \
    (d)[212] = (void *) &p ## RasterPos2d; \
    (d)[213] = (void *) &p ## RasterPos2dv; \
    (d)[214] = (void *) &p ## RasterPos2f; \
    (d)[215] = (void *) &p ## RasterPos2fv; \
    (d)[216] = (void *) &p ## RasterPos2i; \
    (d)[217] = (void *) &p ## RasterPos2iv; \
    (d)[218] = (void *) &p ## RasterPos2s; \
    (d)[219] = (void *) &p ## RasterPos2sv; \
    (d)[220] = (void *) &p ## RasterPos3d; \
    (d)[221] = (void *) &p ## RasterPos3dv; \
    (d)[222] = (void *) &p ## RasterPos3f; \
    (d)[223] = (void *) &p ## RasterPos3fv; \
    (d)[224] = (void *) &p ## RasterPos3i; \
    (d)[225] = (void *) &p ## RasterPos3iv; \
    (d)[226] = (void *) &p ## RasterPos3s; \
    (d)[227] = (void *) &p ## RasterPos3sv; \
    (d)[228] = (void *) &p ## RasterPos4d; \
    (d)[229] = (void *) &p ## RasterPos4dv; \
    (d)[230] = (void *) &p ## RasterPos4f; \
    (d)[231] = (void *) &p ## RasterPos4fv; \
    (d)[232] = (void *) &p ## RasterPos4i; \
    (d)[233] = (void *) &p ## RasterPos4iv; \
    (d)[234] = (void *) &p ## RasterPos4s; \
    (d)[235] = (void *) &p ## RasterPos4sv; \
    (d)[236] = (void *) &p ## ReadBuffer; \
    (d)[237] = (void *) &p ## ReadPixels; \
    (d)[238] = (void *) &p ## Rectd; \
    (d)[239] = (void *) &p ## Rectdv; \
    (d)[240] = (void *) &p ## Rectf; \
    (d)[241] = (void *) &p ## Rectfv; \
    (d)[242] = (void *) &p ## Recti; \
    (d)[243] = (void *) &p ## Rectiv; \
    (d)[244] = (void *) &p ## Rects; \
    (d)[245] = (void *) &p ## Rectsv; \
    (d)[246] = (void *) &p ## RenderMode; \
    (d)[247] = (void *) &p ## Rotated; \
    (d)[248] = (void *) &p ## Rotatef; \
    (d)[249] = (void *) &p ## Scaled; \
    (d)[250] = (void *) &p ## Scalef; \
    (d)[251] = (void *) &p ## Scissor; \
    (d)[252] = (void *) &p ## SelectBuffer; \
    (d)[253] = (void *) &p ## ShadeModel; \
    (d)[254] = (void *) &p ## StencilFunc; \
    (d)[255] = (void *) &p ## StencilMask; \
    (d)[256] = (void *) &p ## StencilOp; \
    (d)[257] = (void *) &p ## TexCoord1d; \
    (d)[258] = (void *) &p ## TexCoord1dv; \
    (d)[259] = (void *) &p ## TexCoord1f; \
    (d)[260] = (void *) &p ## TexCoord1fv; \
    (d)[261] = (void *) &p ## TexCoord1i; \
    (d)[262] = (void *) &p ## TexCoord1iv; \
    (d)[263] = (void *) &p ## TexCoord1s; \
    (d)[264] = (void *) &p ## TexCoord1sv; \
    (d)[265] = (void *) &p ## TexCoord2d; \
    (d)[266] = (void *) &p ## TexCoord2dv; \
    (d)[267] = (void *) &p ## TexCoord2f; \
    (d)[268] = (void *) &p ## TexCoord2fv; \
    (d)[269] = (void *) &p ## TexCoord2i; \
    (d)[270] = (void *) &p ## TexCoord2iv; \
    (d)[271] = (void *) &p ## TexCoord2s; \
    (d)[272] = (void *) &p ## TexCoord2sv; \
    (d)[273] = (void *) &p ## TexCoord3d; \
    (d)[274] = (void *) &p ## TexCoord3dv; \
    (d)[275] = (void *) &p ## TexCoord3f; \
    (d)[276] = (void *) &p ## TexCoord3fv; \
    (d)[277] = (void *) &p ## TexCoord3i; \
    (d)[278] = (void *) &p ## TexCoord3iv; \
    (d)[279] = (void *) &p ## TexCoord3s; \
    (d)[280] = (void *) &p ## TexCoord3sv; \
    (d)[281] = (void *) &p ## TexCoord4d; \
    (d)[282] = (void *) &p ## TexCoord4dv; \
    (d)[283] = (void *) &p ## TexCoord4f; \
    (d)[284] = (void *) &p ## TexCoord4fv; \
    (d)[285] = (void *) &p ## TexCoord4i; \
    (d)[286] = (void *) &p ## TexCoord4iv; \
    (d)[287] = (void *) &p ## TexCoord4s; \
    (d)[288] = (void *) &p ## TexCoord4sv; \
    (d)[289] = (void *) &p ## TexCoordPointer; \
    (d)[290] = (void *) &p ## TexEnvf; \
    (d)[291] = (void *) &p ## TexEnvfv; \
    (d)[292] = (void *) &p ## TexEnvi; \
    (d)[293] = (void *) &p ## TexEnviv; \
    (d)[294] = (void *) &p ## TexGend; \
    (d)[295] = (void *) &p ## TexGendv; \
    (d)[296] = (void *) &p ## TexGenf; \
    (d)[297] = (void *) &p ## TexGenfv; \
    (d)[298] = (void *) &p ## TexGeni; \
    (d)[299] = (void *) &p ## TexGeniv; \
    (d)[300] = (void *) &p ## TexImage1D; \
    (d)[301] = (void *) &p ## TexImage2D; \
    (d)[302] = (void *) &p ## TexParameterf; \
    (d)[303] = (void *) &p ## TexParameterfv; \
    (d)[304] = (void *) &p ## TexParameteri; \
    (d)[305] = (void *) &p ## TexParameteriv; \
    (d)[306] = (void *) &p ## TexSubImage1D; \
    (d)[307] = (void *) &p ## TexSubImage2D; \
    (d)[308] = (void *) &p ## Translated; \
    (d)[309] = (void *) &p ## Translatef; \
    (d)[310] = (void *) &p ## Vertex2d; \
    (d)[311] = (void *) &p ## Vertex2dv; \
    (d)[312] = (void *) &p ## Vertex2f; \
    (d)[313] = (void *) &p ## Vertex2fv; \
    (d)[314] = (void *) &p ## Vertex2i; \
    (d)[315] = (void *) &p ## Vertex2iv; \
    (d)[316] = (void *) &p ## Vertex2s; \
    (d)[317] = (void *) &p ## Vertex2sv; \
    (d)[318] = (void *) &p ## Vertex3d; \
    (d)[319] = (void *) &p ## Vertex3dv; \
    (d)[320] = (void *) &p ## Vertex3f; \
    (d)[321] = (void *) &p ## Vertex3fv; \
    (d)[322] = (void *) &p ## Vertex3i; \
    (d)[323] = (void *) &p ## Vertex3iv; \
    (d)[324] = (void *) &p ## Vertex3s; \
    (d)[325] = (void *) &p ## Vertex3sv; \
    (d)[326] = (void *) &p ## Vertex4d; \
    (d)[327] = (void *) &p ## Vertex4dv; \
    (d)[328] = (void *) &p ## Vertex4f; \
    (d)[329] = (void *) &p ## Vertex4fv; \
    (d)[330] = (void *) &p ## Vertex4i; \
    (d)[331] = (void *) &p ## Vertex4iv; \
    (d)[332] = (void *) &p ## Vertex4s; \
    (d)[333] = (void *) &p ## Vertex4sv; \
    (d)[334] = (void *) &p ## VertexPointer; \
    (d)[335] = (void *) &p ## Viewport; \
    (d)[336] = (void *) &p ## BlendFuncSeparateEXT; \
    (d)[337] = (void *) &p ## BlendColor; \
    (d)[338] = (void *) &p ## BlendEquation; \
    (d)[339] = (void *) &p ## LockArraysEXT; \
    (d)[340] = (void *) &p ## UnlockArraysEXT; \
    (d)[341] = (void *) &p ## ClientActiveTextureARB; \
    (d)[342] = (void *) &p ## ActiveTextureARB; \
    (d)[343] = (void *) &p ## MultiTexCoord1dARB; \
    (d)[344] = (void *) &p ## MultiTexCoord1dvARB; \
    (d)[345] = (void *) &p ## MultiTexCoord1fARB; \
    (d)[346] = (void *) &p ## MultiTexCoord1fvARB; \
    (d)[347] = (void *) &p ## MultiTexCoord1iARB; \
    (d)[348] = (void *) &p ## MultiTexCoord1ivARB; \
    (d)[349] = (void *) &p ## MultiTexCoord1sARB; \
    (d)[350] = (void *) &p ## MultiTexCoord1svARB; \
    (d)[351] = (void *) &p ## MultiTexCoord2dARB; \
    (d)[352] = (void *) &p ## MultiTexCoord2dvARB; \
    (d)[353] = (void *) &p ## MultiTexCoord2fARB; \
    (d)[354] = (void *) &p ## MultiTexCoord2fvARB; \
    (d)[355] = (void *) &p ## MultiTexCoord2iARB; \
    (d)[356] = (void *) &p ## MultiTexCoord2ivARB; \
    (d)[357] = (void *) &p ## MultiTexCoord2sARB; \
    (d)[358] = (void *) &p ## MultiTexCoord2svARB; \
    (d)[359] = (void *) &p ## MultiTexCoord3dARB; \
    (d)[360] = (void *) &p ## MultiTexCoord3dvARB; \
    (d)[361] = (void *) &p ## MultiTexCoord3fARB; \
    (d)[362] = (void *) &p ## MultiTexCoord3fvARB; \
    (d)[363] = (void *) &p ## MultiTexCoord3iARB; \
    (d)[364] = (void *) &p ## MultiTexCoord3ivARB; \
    (d)[365] = (void *) &p ## MultiTexCoord3sARB; \
    (d)[366] = (void *) &p ## MultiTexCoord3svARB; \
    (d)[367] = (void *) &p ## MultiTexCoord4dARB; \
    (d)[368] = (void *) &p ## MultiTexCoord4dvARB; \
    (d)[369] = (void *) &p ## MultiTexCoord4fARB; \
    (d)[370] = (void *) &p ## MultiTexCoord4fvARB; \
    (d)[371] = (void *) &p ## MultiTexCoord4iARB; \
    (d)[372] = (void *) &p ## MultiTexCoord4ivARB; \
    (d)[373] = (void *) &p ## MultiTexCoord4sARB; \
    (d)[374] = (void *) &p ## MultiTexCoord4svARB; \
    (d)[375] = (void *) &p ## LoadTransposeMatrixdARB; \
    (d)[376] = (void *) &p ## LoadTransposeMatrixfARB; \
    (d)[377] = (void *) &p ## MultTransposeMatrixdARB; \
    (d)[378] = (void *) &p ## MultTransposeMatrixfARB; \
    (d)[379] = (void *) &p ## CompressedTexImage3DARB; \
    (d)[380] = (void *) &p ## CompressedTexImage2DARB; \
    (d)[381] = (void *) &p ## CompressedTexImage1DARB; \
    (d)[382] = (void *) &p ## CompressedTexSubImage3DARB; \
    (d)[383] = (void *) &p ## CompressedTexSubImage2DARB; \
    (d)[384] = (void *) &p ## CompressedTexSubImage1DARB; \
    (d)[385] = (void *) &p ## GetCompressedTexImageARB; \
    (d)[386] = (void *) &p ## SecondaryColor3bEXT; \
    (d)[387] = (void *) &p ## SecondaryColor3bvEXT; \
    (d)[388] = (void *) &p ## SecondaryColor3dEXT; \
    (d)[389] = (void *) &p ## SecondaryColor3dvEXT; \
    (d)[390] = (void *) &p ## SecondaryColor3fEXT; \
    (d)[391] = (void *) &p ## SecondaryColor3fvEXT; \
    (d)[392] = (void *) &p ## SecondaryColor3iEXT; \
    (d)[393] = (void *) &p ## SecondaryColor3ivEXT; \
    (d)[394] = (void *) &p ## SecondaryColor3sEXT; \
    (d)[395] = (void *) &p ## SecondaryColor3svEXT; \
    (d)[396] = (void *) &p ## SecondaryColor3ubEXT; \
    (d)[397] = (void *) &p ## SecondaryColor3ubvEXT; \
    (d)[398] = (void *) &p ## SecondaryColor3uiEXT; \
    (d)[399] = (void *) &p ## SecondaryColor3uivEXT; \
    (d)[400] = (void *) &p ## SecondaryColor3usEXT; \
    (d)[401] = (void *) &p ## SecondaryColor3usvEXT; \
    (d)[402] = (void *) &p ## SecondaryColorPointerEXT; \
    (d)[403] = (void *) &p ## VertexArrayRangeNV; \
    (d)[405] = (void *) &p ## DrawRangeElements; \
    (d)[406] = (void *) &p ## ColorTable; \
    (d)[407] = (void *) &p ## ColorTableParameterfv; \
    (d)[408] = (void *) &p ## ColorTableParameteriv; \
    (d)[409] = (void *) &p ## CopyColorTable; \
    (d)[410] = (void *) &p ## GetColorTable; \
    (d)[411] = (void *) &p ## GetColorTableParameterfv; \
    (d)[412] = (void *) &p ## GetColorTableParameteriv; \
    (d)[413] = (void *) &p ## ColorSubTable; \
    (d)[414] = (void *) &p ## CopyColorSubTable; \
    (d)[415] = (void *) &p ## ConvolutionFilter1D; \
    (d)[416] = (void *) &p ## ConvolutionFilter2D; \
    (d)[417] = (void *) &p ## ConvolutionParameterf; \
    (d)[418] = (void *) &p ## ConvolutionParameterfv; \
    (d)[419] = (void *) &p ## ConvolutionParameteri; \
    (d)[420] = (void *) &p ## ConvolutionParameteriv; \
    (d)[421] = (void *) &p ## CopyConvolutionFilter1D; \
    (d)[422] = (void *) &p ## CopyConvolutionFilter2D; \
    (d)[423] = (void *) &p ## GetConvolutionFilter; \
    (d)[424] = (void *) &p ## GetConvolutionParameterfv; \
    (d)[425] = (void *) &p ## GetConvolutionParameteriv; \
    (d)[426] = (void *) &p ## GetSeparableFilter; \
    (d)[427] = (void *) &p ## SeparableFilter2D; \
    (d)[428] = (void *) &p ## GetHistogram; \
    (d)[429] = (void *) &p ## GetHistogramParameterfv; \
    (d)[430] = (void *) &p ## GetHistogramParameteriv; \
    (d)[431] = (void *) &p ## GetMinmax; \
    (d)[432] = (void *) &p ## GetMinmaxParameterfv; \
    (d)[433] = (void *) &p ## GetMinmaxParameteriv; \
    (d)[434] = (void *) &p ## Histogram; \
    (d)[435] = (void *) &p ## Minmax; \
    (d)[436] = (void *) &p ## ResetHistogram; \
    (d)[437] = (void *) &p ## ResetMinmax; \
    (d)[438] = (void *) &p ## TexImage3D; \
    (d)[439] = (void *) &p ## TexSubImage3D; \
    (d)[440] = (void *) &p ## CopyTexSubImage3D; \
    (d)[441] = (void *) &p ## CombinerParameterfvNV; \
    (d)[442] = (void *) &p ## CombinerParameterfNV; \
    (d)[443] = (void *) &p ## CombinerParameterivNV; \
    (d)[444] = (void *) &p ## CombinerParameteriNV; \
    (d)[445] = (void *) &p ## CombinerInputNV; \
    (d)[446] = (void *) &p ## CombinerOutputNV; \
    (d)[447] = (void *) &p ## FinalCombinerInputNV; \
    (d)[448] = (void *) &p ## GetCombinerInputParameterfvNV; \
    (d)[449] = (void *) &p ## GetCombinerInputParameterivNV; \
    (d)[450] = (void *) &p ## GetCombinerOutputParameterfvNV; \
    (d)[451] = (void *) &p ## GetCombinerOutputParameterivNV; \
    (d)[452] = (void *) &p ## GetFinalCombinerInputParameterfvNV; \
    (d)[453] = (void *) &p ## GetFinalCombinerInputParameterivNV; \
    (d)[459] = (void *) &p ## SampleCoverageARB; \
    (d)[540] = (void *) &p ## PointParameterfEXT; \
    (d)[541] = (void *) &p ## PointParameterfvEXT; \
    (d)[542] = (void *) &p ## PointParameteriNV; \
    (d)[543] = (void *) &p ## PointParameterivNV; \
    (d)[544] = (void *) &p ## FogCoordfEXT; \
    (d)[545] = (void *) &p ## FogCoordfvEXT; \
    (d)[546] = (void *) &p ## FogCoorddEXT; \
    (d)[547] = (void *) &p ## FogCoorddvEXT; \
    (d)[548] = (void *) &p ## FogCoordPointerEXT; \
    (d)[567] = (void *) &p ## MultiDrawArraysEXT; \
    (d)[568] = (void *) &p ## MultiDrawElementsEXT; \
    (d)[585] = (void *) &p ## ActiveStencilFaceEXT; \
} while (0)
