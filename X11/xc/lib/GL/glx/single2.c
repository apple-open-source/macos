/* $XFree86: xc/lib/GL/glx/single2.c,v 1.4 2002/02/22 21:32:54 dawes Exp $ */
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
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
**
*/

#define NEED_GL_FUNCS_WRAPPED
#include "glxclient.h"
#include "packsingle.h"

/* Used for GL_ARB_transpose_matrix */
static void TransposeMatrixf(GLfloat m[16])
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < i; j++) {
            GLfloat tmp = m[i*4+j];
            m[i*4+j] = m[j*4+i];
            m[j*4+i] = tmp;
        }
    }
}

/* Used for GL_ARB_transpose_matrix */
static void TransposeMatrixb(GLboolean m[16])
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < i; j++) {
            GLboolean tmp = m[i*4+j];
            m[i*4+j] = m[j*4+i];
            m[j*4+i] = tmp;
        }
    }
}

/* Used for GL_ARB_transpose_matrix */
static void TransposeMatrixd(GLdouble m[16])
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < i; j++) {
            GLdouble tmp = m[i*4+j];
            m[i*4+j] = m[j*4+i];
            m[j*4+i] = tmp;
        }
    }
}

/* Used for GL_ARB_transpose_matrix */
static void TransposeMatrixi(GLint m[16])
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < i; j++) {
            GLint tmp = m[i*4+j];
            m[i*4+j] = m[j*4+i];
            m[j*4+i] = tmp;
        }
    }
}

GLenum glGetError(void)
{
    __GLX_SINGLE_DECLARE_VARIABLES();
    GLuint retval = GL_NO_ERROR;
    xGLXGetErrorReply reply;

    if (gc->error) {
	/* Use internal error first */
	retval = gc->error;
	gc->error = GL_NO_ERROR;
	return retval;
    }

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetError,0);
    __GLX_SINGLE_READ_XREPLY();
    retval = reply.error;
    __GLX_SINGLE_END();

    return retval;
}

void glGetClipPlane(GLenum plane, GLdouble *equation)
{
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;
    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetClipPlane,4);
    __GLX_SINGLE_PUT_LONG(0,plane);
    __GLX_SINGLE_READ_XREPLY();
    if (reply.length == 8) {
	__GLX_SINGLE_GET_DOUBLE_ARRAY(equation,4);
    }
    __GLX_SINGLE_END();
}

void glGetBooleanv(GLenum val, GLboolean *b)
{
    const GLenum origVal = val;
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;

    if (val == GL_TRANSPOSE_MODELVIEW_MATRIX_ARB) {
       val = GL_MODELVIEW_MATRIX;
    }
    else if (val == GL_TRANSPOSE_PROJECTION_MATRIX_ARB) {
       val = GL_PROJECTION_MATRIX;
    }
    else if (val == GL_TRANSPOSE_TEXTURE_MATRIX_ARB) {
       val = GL_TEXTURE_MATRIX;
    }
    else if (val == GL_TRANSPOSE_COLOR_MATRIX_ARB) {
       val = GL_COLOR_MATRIX;
    }

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetBooleanv,4);
    __GLX_SINGLE_PUT_LONG(0,val);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_SIZE(compsize);

    if (compsize == 0) {
	/*
	** Error occured; don't modify user's buffer.
	*/
    } else {
	/*
	** For all the queries listed here, we use the locally stored
	** values rather than the one returned by the server.  Note that
	** we still needed to send the request to the server in order to
	** find out whether it was legal to make a query (it's illegal,
	** for example, to call a query between glBegin() and glEnd()).
	*/
	switch (val) {
	  case GL_PACK_ROW_LENGTH:
	    *b = (GLboolean)gc->state.storePack.rowLength;
	    break;
	  case GL_PACK_IMAGE_HEIGHT:
	    *b = (GLboolean)gc->state.storePack.imageHeight;
	    break;
	  case GL_PACK_SKIP_ROWS:
	    *b = (GLboolean)gc->state.storePack.skipRows;
	    break;
	  case GL_PACK_SKIP_PIXELS:
	    *b = (GLboolean)gc->state.storePack.skipPixels;
	    break;
	  case GL_PACK_SKIP_IMAGES:
	    *b = (GLboolean)gc->state.storePack.skipImages;
	    break;
	  case GL_PACK_ALIGNMENT:
	    *b = (GLboolean)gc->state.storePack.alignment;
	    break;
	  case GL_PACK_SWAP_BYTES:
	    *b = (GLboolean)gc->state.storePack.swapEndian;
	    break;
	  case GL_PACK_LSB_FIRST:
	    *b = (GLboolean)gc->state.storePack.lsbFirst;
	    break;
	  case GL_UNPACK_ROW_LENGTH:
	    *b = (GLboolean)gc->state.storeUnpack.rowLength;
	    break;
	  case GL_UNPACK_IMAGE_HEIGHT:
	    *b = (GLboolean)gc->state.storeUnpack.imageHeight;
	    break;
	  case GL_UNPACK_SKIP_ROWS:
	    *b = (GLboolean)gc->state.storeUnpack.skipRows;
	    break;
	  case GL_UNPACK_SKIP_PIXELS:
	    *b = (GLboolean)gc->state.storeUnpack.skipPixels;
	    break;
	  case GL_UNPACK_SKIP_IMAGES:
	    *b = (GLboolean)gc->state.storeUnpack.skipImages;
	    break;
	  case GL_UNPACK_ALIGNMENT:
	    *b = (GLboolean)gc->state.storeUnpack.alignment;
	    break;
	  case GL_UNPACK_SWAP_BYTES:
	    *b = (GLboolean)gc->state.storeUnpack.swapEndian;
	    break;
	  case GL_UNPACK_LSB_FIRST:
	    *b = (GLboolean)gc->state.storeUnpack.lsbFirst;
	    break;
	  case GL_VERTEX_ARRAY:
	    *b = (GLboolean)gc->state.vertArray.vertex.enable;
	    break;
	  case GL_VERTEX_ARRAY_SIZE:
	    *b = (GLboolean)gc->state.vertArray.vertex.size;
	    break;
	  case GL_VERTEX_ARRAY_TYPE:
	    *b = (GLboolean)gc->state.vertArray.vertex.type;
	    break;
	  case GL_VERTEX_ARRAY_STRIDE:
	    *b = (GLboolean)gc->state.vertArray.vertex.stride;
	    break;
	  case GL_NORMAL_ARRAY:
	    *b = (GLboolean)gc->state.vertArray.normal.enable;
	    break;
	  case GL_NORMAL_ARRAY_TYPE:
	    *b = (GLboolean)gc->state.vertArray.normal.type;
	    break;
	  case GL_NORMAL_ARRAY_STRIDE:
	    *b = (GLboolean)gc->state.vertArray.normal.stride;
	    break;
	  case GL_COLOR_ARRAY:
	    *b = (GLboolean)gc->state.vertArray.color.enable;
	    break;
	  case GL_COLOR_ARRAY_SIZE:
	    *b = (GLboolean)gc->state.vertArray.color.size;
	    break;
	  case GL_COLOR_ARRAY_TYPE:
	    *b = (GLboolean)gc->state.vertArray.color.type;
	    break;
	  case GL_COLOR_ARRAY_STRIDE:
	    *b = (GLboolean)gc->state.vertArray.color.stride;
	    break;
	  case GL_INDEX_ARRAY:
	    *b = (GLboolean)gc->state.vertArray.index.enable;
	    break;
	  case GL_INDEX_ARRAY_TYPE:
	    *b = (GLboolean)gc->state.vertArray.index.type;
	    break;
	  case GL_INDEX_ARRAY_STRIDE:
	    *b = (GLboolean)gc->state.vertArray.index.stride;
	    break;
	  case GL_TEXTURE_COORD_ARRAY:
	    *b = (GLboolean)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].enable;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_SIZE:
	    *b = (GLboolean)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].size;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_TYPE:
	    *b = (GLboolean)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].type;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_STRIDE:
	    *b = (GLboolean)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].stride;
	    break;
	  case GL_EDGE_FLAG_ARRAY:
	    *b = (GLboolean)gc->state.vertArray.edgeFlag.enable;
	    break;
	  case GL_EDGE_FLAG_ARRAY_STRIDE:
	    *b = (GLboolean)gc->state.vertArray.edgeFlag.stride;
	    break;
	  case GL_MAX_ELEMENTS_VERTICES:
	    *b = (GLboolean)gc->state.vertArray.maxElementsVertices;
	    break;
	  case GL_MAX_ELEMENTS_INDICES:
	    *b = (GLboolean)gc->state.vertArray.maxElementsIndices;
	    break;
	  case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
	    *b = (GLboolean)__GL_CLIENT_ATTRIB_STACK_DEPTH;
	    break;
	  case GL_CLIENT_ACTIVE_TEXTURE_ARB:
	    *b = (GLboolean)(gc->state.vertArray.activeTexture + GL_TEXTURE0_ARB);
	    break;
	  default:
	    /*
	    ** Not a local value, so use what we got from the server.
	    */
	    if (compsize == 1) {
		__GLX_SINGLE_GET_CHAR(b);
	    } else {
		__GLX_SINGLE_GET_CHAR_ARRAY(b,compsize);
                if (val != origVal) {
                   /* matrix transpose */
                   TransposeMatrixb(b);
                }
	    }
	}
    }
    __GLX_SINGLE_END();
}

void glGetDoublev(GLenum val, GLdouble *d)
{
    const GLenum origVal = val;
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;

    if (val == GL_TRANSPOSE_MODELVIEW_MATRIX_ARB) {
       val = GL_MODELVIEW_MATRIX;
    }
    else if (val == GL_TRANSPOSE_PROJECTION_MATRIX_ARB) {
       val = GL_PROJECTION_MATRIX;
    }
    else if (val == GL_TRANSPOSE_TEXTURE_MATRIX_ARB) {
       val = GL_TEXTURE_MATRIX;
    }
    else if (val == GL_TRANSPOSE_COLOR_MATRIX_ARB) {
       val = GL_COLOR_MATRIX;
    }

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetDoublev,4);
    __GLX_SINGLE_PUT_LONG(0,val);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_SIZE(compsize);

    if (compsize == 0) {
	/*
	** Error occured; don't modify user's buffer.
	*/
    } else {
	/*
	** For all the queries listed here, we use the locally stored
	** values rather than the one returned by the server.  Note that
	** we still needed to send the request to the server in order to
	** find out whether it was legal to make a query (it's illegal,
	** for example, to call a query between glBegin() and glEnd()).
	*/
	switch (val) {
	  case GL_PACK_ROW_LENGTH:
	    *d = (GLdouble)gc->state.storePack.rowLength;
	    break;
	  case GL_PACK_IMAGE_HEIGHT:
	    *d = (GLdouble)gc->state.storePack.imageHeight;
	    break;
	  case GL_PACK_SKIP_ROWS:
	    *d = (GLdouble)gc->state.storePack.skipRows;
	    break;
	  case GL_PACK_SKIP_PIXELS:
	    *d = (GLdouble)gc->state.storePack.skipPixels;
	    break;
	  case GL_PACK_SKIP_IMAGES:
	    *d = (GLdouble)gc->state.storePack.skipImages;
	    break;
	  case GL_PACK_ALIGNMENT:
	    *d = (GLdouble)gc->state.storePack.alignment;
	    break;
	  case GL_PACK_SWAP_BYTES:
	    *d = (GLdouble)gc->state.storePack.swapEndian;
	    break;
	  case GL_PACK_LSB_FIRST:
	    *d = (GLdouble)gc->state.storePack.lsbFirst;
	    break;
	  case GL_UNPACK_ROW_LENGTH:
	    *d = (GLdouble)gc->state.storeUnpack.rowLength;
	    break;
	  case GL_UNPACK_IMAGE_HEIGHT:
	    *d = (GLdouble)gc->state.storeUnpack.imageHeight;
	    break;
	  case GL_UNPACK_SKIP_ROWS:
	    *d = (GLdouble)gc->state.storeUnpack.skipRows;
	    break;
	  case GL_UNPACK_SKIP_PIXELS:
	    *d = (GLdouble)gc->state.storeUnpack.skipPixels;
	    break;
	  case GL_UNPACK_SKIP_IMAGES:
	    *d = (GLdouble)gc->state.storeUnpack.skipImages;
	    break;
	  case GL_UNPACK_ALIGNMENT:
	    *d = (GLdouble)gc->state.storeUnpack.alignment;
	    break;
	  case GL_UNPACK_SWAP_BYTES:
	    *d = (GLdouble)gc->state.storeUnpack.swapEndian;
	    break;
	  case GL_UNPACK_LSB_FIRST:
	    *d = (GLdouble)gc->state.storeUnpack.lsbFirst;
	    break;
	  case GL_VERTEX_ARRAY:
	    *d = (GLdouble)gc->state.vertArray.vertex.enable;
	    break;
	  case GL_VERTEX_ARRAY_SIZE:
	    *d = (GLdouble)gc->state.vertArray.vertex.size;
	    break;
	  case GL_VERTEX_ARRAY_TYPE:
	    *d = (GLdouble)gc->state.vertArray.vertex.type;
	    break;
	  case GL_VERTEX_ARRAY_STRIDE:
	    *d = (GLdouble)gc->state.vertArray.vertex.stride;
	    break;
	  case GL_NORMAL_ARRAY:
	    *d = (GLdouble)gc->state.vertArray.normal.enable;
	    break;
	  case GL_NORMAL_ARRAY_TYPE:
	    *d = (GLdouble)gc->state.vertArray.normal.type;
	    break;
	  case GL_NORMAL_ARRAY_STRIDE:
	    *d = (GLdouble)gc->state.vertArray.normal.stride;
	    break;
	  case GL_COLOR_ARRAY:
	    *d = (GLdouble)gc->state.vertArray.color.enable;
	    break;
	  case GL_COLOR_ARRAY_SIZE:
	    *d = (GLdouble)gc->state.vertArray.color.size;
	    break;
	  case GL_COLOR_ARRAY_TYPE:
	    *d = (GLdouble)gc->state.vertArray.color.type;
	    break;
	  case GL_COLOR_ARRAY_STRIDE:
	    *d = (GLdouble)gc->state.vertArray.color.stride;
	    break;
	  case GL_INDEX_ARRAY:
	    *d = (GLdouble)gc->state.vertArray.index.enable;
	    break;
	  case GL_INDEX_ARRAY_TYPE:
	    *d = (GLdouble)gc->state.vertArray.index.type;
	    break;
	  case GL_INDEX_ARRAY_STRIDE:
	    *d = (GLdouble)gc->state.vertArray.index.stride;
	    break;
	  case GL_TEXTURE_COORD_ARRAY:
	    *d = (GLdouble)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].enable;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_SIZE:
	    *d = (GLdouble)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].size;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_TYPE:
	    *d = (GLdouble)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].type;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_STRIDE:
	    *d = (GLdouble)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].stride;
	    break;
	  case GL_EDGE_FLAG_ARRAY:
	    *d = (GLdouble)gc->state.vertArray.edgeFlag.enable;
	    break;
	  case GL_EDGE_FLAG_ARRAY_STRIDE:
	    *d = (GLdouble)gc->state.vertArray.edgeFlag.stride;
	    break;
	  case GL_MAX_ELEMENTS_VERTICES:
	    *d = (GLdouble)gc->state.vertArray.maxElementsVertices;
	    break;
	  case GL_MAX_ELEMENTS_INDICES:
	    *d = (GLdouble)gc->state.vertArray.maxElementsIndices;
	    break;
	  case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
	    *d = (GLdouble)__GL_CLIENT_ATTRIB_STACK_DEPTH;
	    break;
	  case GL_CLIENT_ACTIVE_TEXTURE_ARB:
	    *d = (GLdouble)(gc->state.vertArray.activeTexture + GL_TEXTURE0_ARB);
	    break;
	  default:
	    /*
	     ** Not a local value, so use what we got from the server.
	     */
	    if (compsize == 1) {
		__GLX_SINGLE_GET_DOUBLE(d);
	    } else {
		__GLX_SINGLE_GET_DOUBLE_ARRAY(d,compsize);
                if (val != origVal) {
                   /* matrix transpose */
                   TransposeMatrixd(d);
                }
	    }
	}
    }
    __GLX_SINGLE_END();
}

void glGetFloatv(GLenum val, GLfloat *f)
{
    const GLenum origVal = val;
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;

    if (val == GL_TRANSPOSE_MODELVIEW_MATRIX_ARB) {
       val = GL_MODELVIEW_MATRIX;
    }
    else if (val == GL_TRANSPOSE_PROJECTION_MATRIX_ARB) {
       val = GL_PROJECTION_MATRIX;
    }
    else if (val == GL_TRANSPOSE_TEXTURE_MATRIX_ARB) {
       val = GL_TEXTURE_MATRIX;
    }
    else if (val == GL_TRANSPOSE_COLOR_MATRIX_ARB) {
       val = GL_COLOR_MATRIX;
    }

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetFloatv,4);
    __GLX_SINGLE_PUT_LONG(0,val);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_SIZE(compsize);

    if (compsize == 0) {
	/*
	** Error occured; don't modify user's buffer.
	*/
    } else {
	/*
	** For all the queries listed here, we use the locally stored
	** values rather than the one returned by the server.  Note that
	** we still needed to send the request to the server in order to
	** find out whether it was legal to make a query (it's illegal,
	** for example, to call a query between glBegin() and glEnd()).
	*/
	switch (val) {
	  case GL_PACK_ROW_LENGTH:
	    *f = (GLfloat)gc->state.storePack.rowLength;
	    break;
	  case GL_PACK_IMAGE_HEIGHT:
	    *f = (GLfloat)gc->state.storePack.imageHeight;
	    break;
	  case GL_PACK_SKIP_ROWS:
	    *f = (GLfloat)gc->state.storePack.skipRows;
	    break;
	  case GL_PACK_SKIP_PIXELS:
	    *f = (GLfloat)gc->state.storePack.skipPixels;
	    break;
	  case GL_PACK_SKIP_IMAGES:
	    *f = (GLfloat)gc->state.storePack.skipImages;
	    break;
	  case GL_PACK_ALIGNMENT:
	    *f = (GLfloat)gc->state.storePack.alignment;
	    break;
	  case GL_PACK_SWAP_BYTES:
	    *f = (GLfloat)gc->state.storePack.swapEndian;
	    break;
	  case GL_PACK_LSB_FIRST:
	    *f = (GLfloat)gc->state.storePack.lsbFirst;
	    break;
	  case GL_UNPACK_ROW_LENGTH:
	    *f = (GLfloat)gc->state.storeUnpack.rowLength;
	    break;
	  case GL_UNPACK_IMAGE_HEIGHT:
	    *f = (GLfloat)gc->state.storeUnpack.imageHeight;
	    break;
	  case GL_UNPACK_SKIP_ROWS:
	    *f = (GLfloat)gc->state.storeUnpack.skipRows;
	    break;
	  case GL_UNPACK_SKIP_PIXELS:
	    *f = (GLfloat)gc->state.storeUnpack.skipPixels;
	    break;
	  case GL_UNPACK_SKIP_IMAGES:
	    *f = (GLfloat)gc->state.storeUnpack.skipImages;
	    break;
	  case GL_UNPACK_ALIGNMENT:
	    *f = (GLfloat)gc->state.storeUnpack.alignment;
	    break;
	  case GL_UNPACK_SWAP_BYTES:
	    *f = (GLfloat)gc->state.storeUnpack.swapEndian;
	    break;
	  case GL_UNPACK_LSB_FIRST:
	    *f = (GLfloat)gc->state.storeUnpack.lsbFirst;
	    break;
	  case GL_VERTEX_ARRAY:
	    *f = (GLfloat)gc->state.vertArray.vertex.enable;
	    break;
	  case GL_VERTEX_ARRAY_SIZE:
	    *f = (GLfloat)gc->state.vertArray.vertex.size;
	    break;
	  case GL_VERTEX_ARRAY_TYPE:
	    *f = (GLfloat)gc->state.vertArray.vertex.type;
	    break;
	  case GL_VERTEX_ARRAY_STRIDE:
	    *f = (GLfloat)gc->state.vertArray.vertex.stride;
	    break;
	  case GL_NORMAL_ARRAY:
	    *f = (GLfloat)gc->state.vertArray.normal.enable;
	    break;
	  case GL_NORMAL_ARRAY_TYPE:
	    *f = (GLfloat)gc->state.vertArray.normal.type;
	    break;
	  case GL_NORMAL_ARRAY_STRIDE:
	    *f = (GLfloat)gc->state.vertArray.normal.stride;
	    break;
	  case GL_COLOR_ARRAY:
	    *f = (GLfloat)gc->state.vertArray.color.enable;
	    break;
	  case GL_COLOR_ARRAY_SIZE:
	    *f = (GLfloat)gc->state.vertArray.color.size;
	    break;
	  case GL_COLOR_ARRAY_TYPE:
	    *f = (GLfloat)gc->state.vertArray.color.type;
	    break;
	  case GL_COLOR_ARRAY_STRIDE:
	    *f = (GLfloat)gc->state.vertArray.color.stride;
	    break;
	  case GL_INDEX_ARRAY:
	    *f = (GLfloat)gc->state.vertArray.index.enable;
	    break;
	  case GL_INDEX_ARRAY_TYPE:
	    *f = (GLfloat)gc->state.vertArray.index.type;
	    break;
	  case GL_INDEX_ARRAY_STRIDE:
	    *f = (GLfloat)gc->state.vertArray.index.stride;
	    break;
	  case GL_TEXTURE_COORD_ARRAY:
	    *f = (GLfloat)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].enable;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_SIZE:
	    *f = (GLfloat)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].size;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_TYPE:
	    *f = (GLfloat)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].type;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_STRIDE:
	    *f = (GLfloat)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].stride;
	    break;
	  case GL_EDGE_FLAG_ARRAY:
	    *f = (GLfloat)gc->state.vertArray.edgeFlag.enable;
	    break;
	  case GL_EDGE_FLAG_ARRAY_STRIDE:
	    *f = (GLfloat)gc->state.vertArray.edgeFlag.stride;
	    break;
	  case GL_MAX_ELEMENTS_VERTICES:
	    *f = (GLfloat)gc->state.vertArray.maxElementsVertices;
	    break;
	  case GL_MAX_ELEMENTS_INDICES:
	    *f = (GLfloat)gc->state.vertArray.maxElementsIndices;
	    break;
	  case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
	    *f = (GLfloat)__GL_CLIENT_ATTRIB_STACK_DEPTH;
	    break;
	  case GL_CLIENT_ACTIVE_TEXTURE_ARB:
	    *f = (GLfloat)(gc->state.vertArray.activeTexture + GL_TEXTURE0_ARB);
	    break;
	  default:
	    /*
	    ** Not a local value, so use what we got from the server.
	    */
	    if (compsize == 1) {
		__GLX_SINGLE_GET_FLOAT(f);
	    } else {
		__GLX_SINGLE_GET_FLOAT_ARRAY(f,compsize);
                if (val != origVal) {
                   /* matrix transpose */
                   TransposeMatrixf(f);
                }
	    }
	}
    }
    __GLX_SINGLE_END();
}

void glGetIntegerv(GLenum val, GLint *i)
{
    const GLenum origVal = val;
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;

    if (val == GL_TRANSPOSE_MODELVIEW_MATRIX_ARB) {
       val = GL_MODELVIEW_MATRIX;
    }
    else if (val == GL_TRANSPOSE_PROJECTION_MATRIX_ARB) {
       val = GL_PROJECTION_MATRIX;
    }
    else if (val == GL_TRANSPOSE_TEXTURE_MATRIX_ARB) {
       val = GL_TEXTURE_MATRIX;
    }
    else if (val == GL_TRANSPOSE_COLOR_MATRIX_ARB) {
       val = GL_COLOR_MATRIX;
    }

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetIntegerv,4);
    __GLX_SINGLE_PUT_LONG(0,val);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_SIZE(compsize);

    if (compsize == 0) {
	/*
	** Error occured; don't modify user's buffer.
	*/
    } else {
	/*
	** For all the queries listed here, we use the locally stored
	** values rather than the one returned by the server.  Note that
	** we still needed to send the request to the server in order to
	** find out whether it was legal to make a query (it's illegal,
	** for example, to call a query between glBegin() and glEnd()).
	*/
	switch (val) {
	  case GL_PACK_ROW_LENGTH:
	    *i = (GLint)gc->state.storePack.rowLength;
	    break;
	  case GL_PACK_IMAGE_HEIGHT:
	    *i = (GLint)gc->state.storePack.imageHeight;
	    break;
	  case GL_PACK_SKIP_ROWS:
	    *i = (GLint)gc->state.storePack.skipRows;
	    break;
	  case GL_PACK_SKIP_PIXELS:
	    *i = (GLint)gc->state.storePack.skipPixels;
	    break;
	  case GL_PACK_SKIP_IMAGES:
	    *i = (GLint)gc->state.storePack.skipImages;
	    break;
	  case GL_PACK_ALIGNMENT:
	    *i = (GLint)gc->state.storePack.alignment;
	    break;
	  case GL_PACK_SWAP_BYTES:
	    *i = (GLint)gc->state.storePack.swapEndian;
	    break;
	  case GL_PACK_LSB_FIRST:
	    *i = (GLint)gc->state.storePack.lsbFirst;
	    break;
	  case GL_UNPACK_ROW_LENGTH:
	    *i = (GLint)gc->state.storeUnpack.rowLength;
	    break;
	  case GL_UNPACK_IMAGE_HEIGHT:
	    *i = (GLint)gc->state.storeUnpack.imageHeight;
	    break;
	  case GL_UNPACK_SKIP_ROWS:
	    *i = (GLint)gc->state.storeUnpack.skipRows;
	    break;
	  case GL_UNPACK_SKIP_PIXELS:
	    *i = (GLint)gc->state.storeUnpack.skipPixels;
	    break;
	  case GL_UNPACK_SKIP_IMAGES:
	    *i = (GLint)gc->state.storeUnpack.skipImages;
	    break;
	  case GL_UNPACK_ALIGNMENT:
	    *i = (GLint)gc->state.storeUnpack.alignment;
	    break;
	  case GL_UNPACK_SWAP_BYTES:
	    *i = (GLint)gc->state.storeUnpack.swapEndian;
	    break;
	  case GL_UNPACK_LSB_FIRST:
	    *i = (GLint)gc->state.storeUnpack.lsbFirst;
	    break;
	  case GL_VERTEX_ARRAY:
	    *i = (GLint)gc->state.vertArray.vertex.enable;
	    break;
	  case GL_VERTEX_ARRAY_SIZE:
	    *i = (GLint)gc->state.vertArray.vertex.size;
	    break;
	  case GL_VERTEX_ARRAY_TYPE:
	    *i = (GLint)gc->state.vertArray.vertex.type;
	    break;
	  case GL_VERTEX_ARRAY_STRIDE:
	    *i = (GLint)gc->state.vertArray.vertex.stride;
	    break;
	  case GL_NORMAL_ARRAY:
	    *i = (GLint)gc->state.vertArray.normal.enable;
	    break;
	  case GL_NORMAL_ARRAY_TYPE:
	    *i = (GLint)gc->state.vertArray.normal.type;
	    break;
	  case GL_NORMAL_ARRAY_STRIDE:
	    *i = (GLint)gc->state.vertArray.normal.stride;
	    break;
	  case GL_COLOR_ARRAY:
	    *i = (GLint)gc->state.vertArray.color.enable;
	    break;
	  case GL_COLOR_ARRAY_SIZE:
	    *i = (GLint)gc->state.vertArray.color.size;
	    break;
	  case GL_COLOR_ARRAY_TYPE:
	    *i = (GLint)gc->state.vertArray.color.type;
	    break;
	  case GL_COLOR_ARRAY_STRIDE:
	    *i = (GLint)gc->state.vertArray.color.stride;
	    break;
	  case GL_INDEX_ARRAY:
	    *i = (GLint)gc->state.vertArray.index.enable;
	    break;
	  case GL_INDEX_ARRAY_TYPE:
	    *i = (GLint)gc->state.vertArray.index.type;
	    break;
	  case GL_INDEX_ARRAY_STRIDE:
	    *i = (GLint)gc->state.vertArray.index.stride;
	    break;
	  case GL_TEXTURE_COORD_ARRAY:
	    *i = (GLint)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].enable;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_SIZE:
	    *i = (GLint)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].size;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_TYPE:
	    *i = (GLint)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].type;
	    break;
	  case GL_TEXTURE_COORD_ARRAY_STRIDE:
	    *i = (GLint)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].stride;
	    break;
	  case GL_EDGE_FLAG_ARRAY:
	    *i = (GLint)gc->state.vertArray.edgeFlag.enable;
	    break;
	  case GL_EDGE_FLAG_ARRAY_STRIDE:
	    *i = (GLint)gc->state.vertArray.edgeFlag.stride;
	    break;
	  case GL_MAX_ELEMENTS_VERTICES:
	    *i = (GLint)gc->state.vertArray.maxElementsVertices;
	    break;
	  case GL_MAX_ELEMENTS_INDICES:
	    *i = (GLint)gc->state.vertArray.maxElementsIndices;
	    break;
	  case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
	    *i = (GLint)__GL_CLIENT_ATTRIB_STACK_DEPTH;
	    break;
	  case GL_CLIENT_ACTIVE_TEXTURE_ARB:
	    *i = (GLint)(gc->state.vertArray.activeTexture + GL_TEXTURE0_ARB);
	    break;
	  default:
	    /*
	    ** Not a local value, so use what we got from the server.
	    */
	    if (compsize == 1) {
		__GLX_SINGLE_GET_LONG(i);
	    } else {
		__GLX_SINGLE_GET_LONG_ARRAY(i,compsize);
                if (val != origVal) {
                   /* matrix transpose */
                   TransposeMatrixi(i);
                }
	    }
	}
    }
    __GLX_SINGLE_END();
}

/*
** Send all pending commands to server.
*/
void glFlush(void)
{
    __GLX_SINGLE_DECLARE_VARIABLES();

    if (!dpy) return;

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_Flush,0);
    __GLX_SINGLE_END();

    /* And finally flush the X protocol data */
    XFlush(dpy);
}

void glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
    __GLX_SINGLE_DECLARE_VARIABLES();

    if (!dpy) return;

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_FeedbackBuffer,8);
    __GLX_SINGLE_PUT_LONG(0,size);
    __GLX_SINGLE_PUT_LONG(4,type);
    __GLX_SINGLE_END();

    gc->feedbackBuf = buffer;
}

void glSelectBuffer(GLsizei numnames, GLuint *buffer)
{
    __GLX_SINGLE_DECLARE_VARIABLES();

    if (!dpy) return;

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_SelectBuffer,4);
    __GLX_SINGLE_PUT_LONG(0,numnames);
    __GLX_SINGLE_END();

    gc->selectBuf = buffer;
}

GLint glRenderMode(GLenum mode)
{
    __GLX_SINGLE_DECLARE_VARIABLES();
    GLint retval;
    xGLXRenderModeReply reply;

    if (!dpy) return -1;

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_RenderMode,4);
    __GLX_SINGLE_PUT_LONG(0,mode);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_RETVAL(retval,GLint);

    if (reply.newMode != mode) {
	/*
	** Switch to new mode did not take effect, therefore an error
	** occured.  When an error happens the server won't send us any
	** other data.
	*/
    } else {
	/* Read the feedback or selection data */
	if (gc->renderMode == GL_FEEDBACK) {
	    __GLX_SINGLE_GET_SIZE(compsize);
	    __GLX_SINGLE_GET_FLOAT_ARRAY(gc->feedbackBuf, compsize);
	} else
	if (gc->renderMode == GL_SELECT) {
	    __GLX_SINGLE_GET_SIZE(compsize);
	    __GLX_SINGLE_GET_LONG_ARRAY(gc->selectBuf, compsize);
	}
	gc->renderMode = mode;
    }
    __GLX_SINGLE_END();

    return retval;
}

void glFinish(void)
{
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_Finish,0);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_END();
}

const GLubyte *glGetString(GLenum name)
{
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;
    GLubyte *s = NULL;

    if (!dpy) return 0;

    /*
    ** Return the cached copy if the string has already been fetched
    */
    switch(name) {
      case GL_VENDOR:
	if (gc->vendor) return gc->vendor;
	break;
      case GL_RENDERER:
	if (gc->renderer) return gc->renderer;
	break;
      case GL_VERSION:
	if (gc->version) return gc->version;
	break;
      case GL_EXTENSIONS:
	if (gc->extensions) return gc->extensions;
	break;
      default:
	__glXSetError(gc, GL_INVALID_ENUM);
	return 0;
    }

    /*
    ** Get requested string from server
    */
    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_GetString,4);
    __GLX_SINGLE_PUT_LONG(0,name);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_SIZE(compsize);
    s = (GLubyte*) Xmalloc(compsize);
    if (!s) {
	/* Throw data on the floor */
	_XEatData(dpy, compsize);
	__glXSetError(gc, GL_OUT_OF_MEMORY);
    } else {
	__GLX_SINGLE_GET_CHAR_ARRAY(s,compsize);

	/*
	** Update local cache
	*/
	switch(name) {
	  case GL_VENDOR:
	    gc->vendor = s;
	    break;
	  case GL_RENDERER:
	    gc->renderer = s;
	    break;
	  case GL_VERSION:
	    gc->version = s;
	    break;
	  case GL_EXTENSIONS:
	    gc->extensions = s;
	    break;
	}
    }
    __GLX_SINGLE_END();
    return s;
}

GLboolean glIsEnabled(GLenum cap)
{
    __GLX_SINGLE_DECLARE_VARIABLES();
    xGLXSingleReply reply;
    GLboolean retval = 0;

    if (!dpy) return 0;

    switch(cap) {
      case GL_VERTEX_ARRAY:
	  return gc->state.vertArray.vertex.enable;
      case GL_NORMAL_ARRAY:
	  return gc->state.vertArray.normal.enable;
      case GL_COLOR_ARRAY:
	  return gc->state.vertArray.color.enable;
      case GL_INDEX_ARRAY:
	  return gc->state.vertArray.index.enable;
      case GL_TEXTURE_COORD_ARRAY:
	  return gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].enable;
      case GL_EDGE_FLAG_ARRAY:
	  return gc->state.vertArray.edgeFlag.enable;
    }

    __GLX_SINGLE_LOAD_VARIABLES();
    __GLX_SINGLE_BEGIN(X_GLsop_IsEnabled,4);
    __GLX_SINGLE_PUT_LONG(0,cap);
    __GLX_SINGLE_READ_XREPLY();
    __GLX_SINGLE_GET_RETVAL(retval, GLboolean);
    __GLX_SINGLE_END();
    return retval;
}

void glGetPointerv(GLenum pname, void **params)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    switch(pname) {
      case GL_VERTEX_ARRAY_POINTER:
	  *params = (void *)gc->state.vertArray.vertex.ptr;
	  return;
      case GL_NORMAL_ARRAY_POINTER:
	  *params = (void *)gc->state.vertArray.normal.ptr;
	  return;
      case GL_COLOR_ARRAY_POINTER:
	  *params = (void *)gc->state.vertArray.color.ptr;
	  return;
      case GL_INDEX_ARRAY_POINTER:
	  *params = (void *)gc->state.vertArray.index.ptr;
	  return;
      case GL_TEXTURE_COORD_ARRAY_POINTER:
	  *params = (void *)gc->state.vertArray.texCoord[gc->state.vertArray.activeTexture].ptr;
	  return;
      case GL_EDGE_FLAG_ARRAY_POINTER:
	  *params = (void *)gc->state.vertArray.edgeFlag.ptr;
	return;
      case GL_FEEDBACK_BUFFER_POINTER:
	*params = (void *)gc->feedbackBuf;
	return;
      case GL_SELECTION_BUFFER_POINTER:
	*params = (void *)gc->selectBuf;
	return;
      default:
	__glXSetError(gc, GL_INVALID_ENUM);
	return;
    }
}

