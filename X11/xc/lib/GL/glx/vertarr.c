/* $XFree86: xc/lib/GL/glx/vertarr.c,v 1.5 2004/01/28 18:11:43 alanh Exp $ */
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
#include "packrender.h"
#include <string.h>
#include <limits.h>		/* INT_MAX */

/* macros for setting function pointers */
#define __GL_VERTEX_FUNC(NAME, let) \
    case GL_##NAME: \
      if (size == 2) \
	vertexPointer->proc = (void (*)(const void *))glVertex2##let##v; \
      else if (size == 3) \
	vertexPointer->proc = (void (*)(const void *))glVertex3##let##v; \
      else if (size == 4) \
	vertexPointer->proc = (void (*)(const void *))glVertex4##let##v; \
      break

#define __GL_NORMAL_FUNC(NAME, let) \
    case GL_##NAME: \
      normalPointer->proc = (void (*)(const void *))glNormal3##let##v; \
      break

#define __GL_COLOR_FUNC(NAME, let) \
    case GL_##NAME: \
      if (size == 3) \
	colorPointer->proc = (void (*)(const void *))glColor3##let##v; \
      else if (size == 4)\
	colorPointer->proc = (void (*)(const void *))glColor4##let##v; \
      break

#define __GL_SEC_COLOR_FUNC(NAME, let) \
    case GL_##NAME: \
      seccolorPointer->proc = (void (*)(const void *))glSecondaryColor3##let##v; \

#define __GL_FOG_FUNC(NAME, let) \
    case GL_##NAME: \
      fogPointer->proc = (void (*)(const void *))glFogCoord##let##v; \

#define __GL_INDEX_FUNC(NAME, let) \
    case GL_##NAME: \
      indexPointer->proc = (void (*)(const void *))glIndex##let##v; \
      break

#define __GL_TEXTURE_FUNC(NAME, let) \
    case GL_##NAME: \
      if (size == 1) { \
	texCoordPointer->proc = (void (*)(const void *))glTexCoord1##let##v; \
	texCoordPointer->mtex_proc = (void (*)(GLenum, const void *))glMultiTexCoord1##let##vARB; \
      } else if (size == 2) { \
	texCoordPointer->proc = (void (*)(const void *))glTexCoord2##let##v; \
	texCoordPointer->mtex_proc = (void (*)(GLenum, const void *))glMultiTexCoord2##let##vARB; \
      } else if (size == 3) { \
	texCoordPointer->proc = (void (*)(const void *))glTexCoord3##let##v; \
	texCoordPointer->mtex_proc = (void (*)(GLenum, const void *))glMultiTexCoord2##let##vARB; \
      } else if (size == 4) { \
	texCoordPointer->proc = (void (*)(const void *))glTexCoord4##let##v; \
	texCoordPointer->mtex_proc = (void (*)(GLenum, const void *))glMultiTexCoord4##let##vARB; \
      } break

static GLuint __glXTypeSize(GLenum enm)
{
    switch (enm) {
      case __GL_BOOLEAN_ARRAY:	return sizeof(GLboolean);
      case GL_BYTE: 		return sizeof(GLbyte); 	
      case GL_UNSIGNED_BYTE: 	return sizeof(GLubyte);
      case GL_SHORT: 		return sizeof(GLshort); 	
      case GL_UNSIGNED_SHORT: 	return sizeof(GLushort);
      case GL_INT: 		return sizeof(GLint); 		
      case GL_UNSIGNED_INT: 	return sizeof(GLint);
      case GL_FLOAT: 		return sizeof(GLfloat);
      case GL_DOUBLE:	 	return sizeof(GLdouble);
      default:			return 0;
    }
}

void __glXInitVertexArrayState(__GLXcontext *gc)
{
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertArrayState *va = &state->vertArray;
    GLint i;

    va->vertex.enable = GL_FALSE;
    va->vertex.proc = NULL;
    va->vertex.skip = 0;
    va->vertex.ptr = 0;
    va->vertex.size = 4;
    va->vertex.type = GL_FLOAT;
    va->vertex.stride = 0;

    va->normal.enable = GL_FALSE;
    va->normal.proc = NULL;
    va->normal.skip = 0;
    va->normal.ptr = 0;
    va->normal.size = 3;
    va->normal.type = GL_FLOAT;
    va->normal.stride = 0;

    va->color.enable = GL_FALSE;
    va->color.proc = NULL;
    va->color.skip = 0;
    va->color.ptr = 0;
    va->color.size = 4;
    va->color.type = GL_FLOAT;
    va->color.stride = 0;

    va->secondaryColor.enable = GL_FALSE;
    va->secondaryColor.proc = NULL;
    va->secondaryColor.skip = 0;
    va->secondaryColor.ptr = 0;
    va->secondaryColor.size = 3;
    va->secondaryColor.type = GL_FLOAT;
    va->secondaryColor.stride = 0;

    va->fogCoord.enable = GL_FALSE;
    va->fogCoord.proc = NULL;
    va->fogCoord.skip = 0;
    va->fogCoord.ptr = 0;
    va->fogCoord.size = 1;
    va->fogCoord.type = GL_FLOAT;
    va->fogCoord.stride = 0;

    va->index.enable = GL_FALSE;
    va->index.proc = NULL;
    va->index.skip = 0;
    va->index.ptr = 0;
    va->index.size = 1;
    va->index.type = GL_FLOAT;
    va->index.stride = 0;

    for (i=0; i<__GLX_MAX_TEXTURE_UNITS; ++i) {
        __GLXvertexArrayPointerState *texCoord = &va->texCoord[i];

	texCoord->enable = GL_FALSE;
	texCoord->proc = NULL;
	texCoord->skip = 0;
	texCoord->ptr = 0;
	texCoord->size = 4;
	texCoord->type = GL_FLOAT;
	texCoord->stride = 0;
    }

    va->edgeFlag.enable = GL_FALSE;
    va->edgeFlag.proc = NULL;
    va->edgeFlag.skip = 0;
    va->edgeFlag.ptr = 0;
    va->edgeFlag.size = 1;
    va->edgeFlag.type = GL_UNSIGNED_BYTE;
    va->edgeFlag.stride = 0;

    va->maxElementsVertices = INT_MAX;
    va->maxElementsIndices = INT_MAX;
}

/*****************************************************************************/

void glVertexPointer(GLint size, GLenum type, GLsizei stride,
		     const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *vertexPointer = &state->vertArray.vertex;

    /* Check arguments */
    if (size < 2 || size > 4 || stride < 0) {
        __glXSetError(gc, GL_INVALID_VALUE);
        return;
    }

    /* Choose appropriate api proc */
    switch(type) {
        __GL_VERTEX_FUNC(SHORT, s);
        __GL_VERTEX_FUNC(INT, i);
        __GL_VERTEX_FUNC(FLOAT, f);
        __GL_VERTEX_FUNC(DOUBLE, d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    vertexPointer->size = size;
    vertexPointer->type = type;
    vertexPointer->stride = stride;
    vertexPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
	vertexPointer->skip = __glXTypeSize(type) * size;
    } else {
	vertexPointer->skip = stride;
    }
}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *normalPointer = &state->vertArray.normal;

    /* Check arguments */
    if (stride < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /* Choose appropriate api proc */
    switch(type) {
	__GL_NORMAL_FUNC(BYTE, b);
	__GL_NORMAL_FUNC(SHORT, s);
	__GL_NORMAL_FUNC(INT, i);
	__GL_NORMAL_FUNC(FLOAT, f);
	__GL_NORMAL_FUNC(DOUBLE, d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    normalPointer->type = type;
    normalPointer->stride = stride;
    normalPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
	normalPointer->skip = 3 * __glXTypeSize(type);
    } else {
	normalPointer->skip = stride;
    }
}

void glColorPointer(GLint size, GLenum type, GLsizei stride,
		    const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *colorPointer = &state->vertArray.color;

    /* Check arguments */
    if (stride < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /* Choose appropriate api proc */
    switch(type) {
	__GL_COLOR_FUNC(BYTE, b);
	__GL_COLOR_FUNC(UNSIGNED_BYTE, ub);
	__GL_COLOR_FUNC(SHORT, s);
	__GL_COLOR_FUNC(UNSIGNED_SHORT, us);
	__GL_COLOR_FUNC(INT, i);
	__GL_COLOR_FUNC(UNSIGNED_INT, ui);
	__GL_COLOR_FUNC(FLOAT, f);
	__GL_COLOR_FUNC(DOUBLE, d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    colorPointer->size = size;
    colorPointer->type = type;
    colorPointer->stride = stride;
    colorPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
        colorPointer->skip = size * __glXTypeSize(type);
    } else {
        colorPointer->skip = stride;
    }
}

void glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *indexPointer = &state->vertArray.index;

    /* Check arguments */
    if (stride < 0) {
        __glXSetError(gc, GL_INVALID_VALUE);
        return;
    }

    /* Choose appropriate api proc */
    switch(type) {
	__GL_INDEX_FUNC(UNSIGNED_BYTE, ub);
        __GL_INDEX_FUNC(SHORT, s);
        __GL_INDEX_FUNC(INT, i);
        __GL_INDEX_FUNC(FLOAT, f);
        __GL_INDEX_FUNC(DOUBLE, d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    indexPointer->type = type;
    indexPointer->stride = stride;
    indexPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
	indexPointer->skip = __glXTypeSize(type);
    } else {
	indexPointer->skip = stride;
    }
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride,
		       const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *texCoordPointer =
    	&state->vertArray.texCoord[state->vertArray.activeTexture];

    /* Check arguments */
    if (size < 1 || size > 4 || stride < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /* Choose appropriate api proc */
    switch(type) {
	__GL_TEXTURE_FUNC(SHORT, s);
	__GL_TEXTURE_FUNC(INT, i);
	__GL_TEXTURE_FUNC(FLOAT, f);
	__GL_TEXTURE_FUNC(DOUBLE,  d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    texCoordPointer->size = size;
    texCoordPointer->type = type;
    texCoordPointer->stride = stride;
    texCoordPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
	texCoordPointer->skip = __glXTypeSize(type) * size;
    } else {
	texCoordPointer->skip = stride;
    }
}

void glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *edgeFlagPointer = &state->vertArray.edgeFlag;

    /* Check arguments */
    if (stride < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /* Choose appropriate api proc */
    edgeFlagPointer->proc = (void (*)(const void *))glEdgeFlagv;

    edgeFlagPointer->stride = stride;
    edgeFlagPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
	edgeFlagPointer->skip = sizeof(GLboolean);
    } else {
	edgeFlagPointer->skip = stride;
    }

}

void glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride,
			     const GLvoid * pointer )
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *seccolorPointer = &state->vertArray.secondaryColor;

    /* Check arguments */
    if ( (stride < 0) || (size != 3) ) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /* Choose appropriate api proc */
    switch(type) {
	__GL_SEC_COLOR_FUNC(BYTE, b);
	__GL_SEC_COLOR_FUNC(UNSIGNED_BYTE, ub);
	__GL_SEC_COLOR_FUNC(SHORT, s);
	__GL_SEC_COLOR_FUNC(UNSIGNED_SHORT, us);
	__GL_SEC_COLOR_FUNC(INT, i);
	__GL_SEC_COLOR_FUNC(UNSIGNED_INT, ui);
	__GL_SEC_COLOR_FUNC(FLOAT, f);
	__GL_SEC_COLOR_FUNC(DOUBLE, d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    seccolorPointer->size = size;
    seccolorPointer->type = type;
    seccolorPointer->stride = stride;
    seccolorPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
        seccolorPointer->skip = size * __glXTypeSize(type);
    } else {
        seccolorPointer->skip = stride;
    }
}

void glFogCoordPointer(GLenum type, GLsizei stride, const GLvoid * pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertexArrayPointerState *fogPointer = &state->vertArray.fogCoord;

    /* Check arguments */
    if (stride < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /* Choose appropriate api proc */
    switch(type) {
	__GL_FOG_FUNC(FLOAT, f);
	__GL_FOG_FUNC(DOUBLE, d);
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    fogPointer->size = 1;
    fogPointer->type = type;
    fogPointer->stride = stride;
    fogPointer->ptr = pointer;

    /* Set internal state */
    if (stride == 0) {
        fogPointer->skip = __glXTypeSize(type);
    } else {
        fogPointer->skip = stride;
    }
}

void glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    GLboolean tEnable = GL_FALSE, cEnable = GL_FALSE, nEnable = GL_FALSE;
    GLenum tType = GL_FLOAT, nType = GL_FLOAT, vType = GL_FLOAT;
    GLenum cType = GL_FALSE;
    GLint tSize = 0, cSize = 0, nSize = 3, vSize;
    int cOffset = 0, nOffset = 0, vOffset = 0;
    GLint trueStride, size;

    switch (format) {
      case GL_V2F:
	vSize = 2;
	size = __glXTypeSize(vType) * vSize;
	break;
      case GL_V3F:
	vSize = 3;
	size = __glXTypeSize(vType) * vSize;
	break;
      case GL_C4UB_V2F:
	cEnable = GL_TRUE;
	cSize = 4;
	cType = GL_UNSIGNED_BYTE;
	vSize = 2;
	vOffset = __glXTypeSize(cType) * cSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_C4UB_V3F:
	cEnable = GL_TRUE;
	cSize = 4;
	cType = GL_UNSIGNED_BYTE;
	vSize = 3;
	vOffset = __glXTypeSize(vType) * cSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_C3F_V3F:
	cEnable = GL_TRUE;
	cSize = 3;
	cType = GL_FLOAT;
	vSize = 3;
	vOffset = __glXTypeSize(cType) * cSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_N3F_V3F:
	nEnable = GL_TRUE;
	vSize = 3;
	vOffset = __glXTypeSize(nType) * nSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_C4F_N3F_V3F:
	cEnable = GL_TRUE;
	cSize = 4;
	cType = GL_FLOAT;
	nEnable = GL_TRUE;
	nOffset = __glXTypeSize(cType) * cSize;
	vSize = 3;
	vOffset = nOffset + __glXTypeSize(nType) * nSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T2F_V3F:
	tEnable = GL_TRUE;
	tSize = 2;
	vSize = 3;
	vOffset = __glXTypeSize(tType) * tSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T4F_V4F:
	tEnable = GL_TRUE;
	tSize = 4;
	vSize = 4;
	vOffset = __glXTypeSize(tType) * tSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T2F_C4UB_V3F:
	tEnable = GL_TRUE;
	tSize = 2;
	cEnable = GL_TRUE;
	cSize = 4;
	cType = GL_UNSIGNED_BYTE;
	cOffset = __glXTypeSize(tType) * tSize;
	vSize = 3;
	vOffset = cOffset + __glXTypeSize(cType) * cSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T2F_C3F_V3F:
	tEnable = GL_TRUE;
	tSize = 2;
	cEnable = GL_TRUE;
	cSize = 3;
	cType = GL_FLOAT;
	cOffset = __glXTypeSize(tType) * tSize;
	vSize = 3;
	vOffset = cOffset + __glXTypeSize(cType) * cSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T2F_N3F_V3F:
	tEnable = GL_TRUE;
	tSize = 2;
	nEnable = GL_TRUE;
	nOffset = __glXTypeSize(tType) * tSize;
	vSize = 3;
	vOffset = nOffset + __glXTypeSize(nType) * nSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T2F_C4F_N3F_V3F:
	tEnable = GL_TRUE;
	tSize = 2;
	cEnable = GL_TRUE;
	cSize = 4;
	cType = GL_FLOAT;
	cOffset = __glXTypeSize(tType) * tSize;
	nEnable = GL_TRUE;
	nOffset = cOffset + __glXTypeSize(cType) * cSize;
	vSize = 3;
	vOffset = nOffset + __glXTypeSize(nType) * nSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      case GL_T4F_C4F_N3F_V4F:
	tEnable = GL_TRUE;
	tSize = 4;
	cEnable = GL_TRUE;
	cSize = 4;
	cType = GL_FLOAT;
	cOffset = __glXTypeSize(tType) * tSize;
	nEnable = GL_TRUE;
	nOffset = cOffset + __glXTypeSize(cType) * cSize;
	vSize = 4;
	vOffset = nOffset + __glXTypeSize(nType) * nSize;
	size = vOffset + __glXTypeSize(vType) * vSize;
	break;
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    trueStride = (stride == 0) ? size : stride;

    glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
    glDisableClientState(GL_FOG_COORDINATE_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);
    if (tEnable) {
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(tSize, tType, trueStride, (const char *)pointer);
    } else {
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    if (cEnable) {
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(cSize, cType, trueStride, (const char *)pointer+cOffset);
    } else {
	glDisableClientState(GL_COLOR_ARRAY);
    }
    if (nEnable) {
	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(nType, trueStride, (const char *)pointer+nOffset);
    } else {
	glDisableClientState(GL_NORMAL_ARRAY);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(vSize, vType, trueStride, (const char *)pointer+vOffset);
}

/*****************************************************************************/

void glArrayElement(GLint i)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertArrayState *va = &state->vertArray;
    GLint j;

    if (va->edgeFlag.enable == GL_TRUE) {
	(*va->edgeFlag.proc)(va->edgeFlag.ptr+i*va->edgeFlag.skip);
    }

    for (j=0; j<__GLX_MAX_TEXTURE_UNITS; ++j) {
	if (va->texCoord[j].enable == GL_TRUE) {
	    (*va->texCoord[j].proc)(va->texCoord[j].ptr+i*va->texCoord[j].skip);
	}
    }

    if (va->color.enable == GL_TRUE) {
	(*va->color.proc)(va->color.ptr+i*va->color.skip);
    }

    if (va->secondaryColor.enable == GL_TRUE) {
	(*va->secondaryColor.proc)(va->secondaryColor.ptr+i*va->secondaryColor.skip);
    }

    if (va->index.enable == GL_TRUE) {
	(*va->index.proc)(va->index.ptr+i*va->index.skip);
    }

    if (va->normal.enable == GL_TRUE) {
	(*va->normal.proc)(va->normal.ptr+i*va->normal.skip);
    }

    if (va->fogCoord.enable == GL_TRUE) {
	(*va->fogCoord.proc)(va->fogCoord.ptr+i*va->fogCoord.skip);
    }

    if (va->vertex.enable == GL_TRUE) {
	(*va->vertex.proc)(va->vertex.ptr+i*va->vertex.skip);
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertArrayState *va = &state->vertArray;
    const GLubyte *vaPtr = NULL, *naPtr = NULL, *caPtr = NULL,
                  *scaPtr = NULL, *faPtr = NULL,
                  *iaPtr = NULL, *tcaPtr[__GLX_MAX_TEXTURE_UNITS];
    const GLboolean *efaPtr = NULL;
    GLint i, j;

    switch(mode) {
      case GL_POINTS:
      case GL_LINE_STRIP:
      case GL_LINE_LOOP:
      case GL_LINES:
      case GL_TRIANGLE_STRIP:
      case GL_TRIANGLE_FAN:
      case GL_TRIANGLES:
      case GL_QUAD_STRIP:
      case GL_QUADS:
      case GL_POLYGON:
	break;
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    if (count < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    /*
    ** Set up pointers for quick array traversal.
    */
    if (va->normal.enable == GL_TRUE) 
	naPtr = va->normal.ptr + first * va->normal.skip;
    if (va->color.enable == GL_TRUE) 
	caPtr = va->color.ptr + first * va->color.skip;
    if (va->secondaryColor.enable == GL_TRUE) 
	scaPtr = va->secondaryColor.ptr + first * va->secondaryColor.skip;
    if (va->fogCoord.enable == GL_TRUE) 
	faPtr = va->fogCoord.ptr + first * va->fogCoord.skip;
    if (va->index.enable == GL_TRUE) 
	iaPtr = va->index.ptr + first * va->index.skip;
    for (j=0; j<__GLX_MAX_TEXTURE_UNITS; ++j) {
	if (va->texCoord[j].enable == GL_TRUE) 
	    tcaPtr[j] = va->texCoord[j].ptr + first * va->texCoord[j].skip;
    }
    if (va->edgeFlag.enable == GL_TRUE) 
	efaPtr = va->edgeFlag.ptr + first * va->edgeFlag.skip;
    if (va->vertex.enable == GL_TRUE)
	vaPtr = va->vertex.ptr + first * va->vertex.skip;

    glBegin(mode);
        for (i = 0; i < count; i++) {
            if (va->edgeFlag.enable == GL_TRUE) {
                (*va->edgeFlag.proc)(efaPtr);
                efaPtr += va->edgeFlag.skip;
            }

	    
	    if (va->texCoord[0].enable == GL_TRUE) {
		(*va->texCoord[0].proc)(tcaPtr[0]);
		tcaPtr[0] += va->texCoord[0].skip;
	    }

	    /* Multitexturing is handled specially because the protocol
	     * requires an extra parameter.
	     */
	    for (j=1; j<__GLX_MAX_TEXTURE_UNITS; ++j) {
		if (va->texCoord[j].enable == GL_TRUE) {
		    (*va->texCoord[j].mtex_proc)(GL_TEXTURE0 + j, tcaPtr[j]);
		    tcaPtr[j] += va->texCoord[j].skip;
		}
	    }

            if (va->color.enable == GL_TRUE) {
                (*va->color.proc)(caPtr);
                caPtr += va->color.skip;
            }
            if (va->secondaryColor.enable == GL_TRUE) {
                (*va->secondaryColor.proc)(scaPtr);
                scaPtr += va->secondaryColor.skip;
            }
            if (va->fogCoord.enable == GL_TRUE) {
                (*va->fogCoord.proc)(faPtr);
                faPtr += va->fogCoord.skip;
            }
            if (va->index.enable == GL_TRUE) {
                (*va->index.proc)(iaPtr);
                iaPtr += va->index.skip;
            }
            if (va->normal.enable == GL_TRUE) {
                (*va->normal.proc)(naPtr);
                naPtr += va->normal.skip;
            }
            if (va->vertex.enable == GL_TRUE) {
                (*va->vertex.proc)(vaPtr);
                vaPtr += va->vertex.skip;
        }
    }
    glEnd();
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type,
		    const GLvoid *indices)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    __GLXvertArrayState *va = &state->vertArray;
    const GLubyte *iPtr1 = NULL;
    const GLushort *iPtr2 = NULL;
    const GLuint *iPtr3 = NULL;
    GLint i, j, offset = 0;

    switch (mode) {
      case GL_POINTS:
      case GL_LINE_STRIP:
      case GL_LINE_LOOP:
      case GL_LINES:
      case GL_TRIANGLE_STRIP:
      case GL_TRIANGLE_FAN:
      case GL_TRIANGLES:
      case GL_QUAD_STRIP:
      case GL_QUADS:
      case GL_POLYGON:
	break;
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    if (count < 0) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    } 

    switch (type) {
      case GL_UNSIGNED_BYTE:
	iPtr1 = (const GLubyte *)indices;
	break;
      case GL_UNSIGNED_SHORT:
	iPtr2 = (const GLushort *)indices;
	break;
      case GL_UNSIGNED_INT:
	iPtr3 = (const GLuint *)indices;
	break;
      default:
        __glXSetError(gc, GL_INVALID_ENUM);
        return;
    }

    glBegin(mode);
        for (i = 0; i < count; i++) {
	    switch (type) {
	      case GL_UNSIGNED_BYTE:
		offset = (GLint)(*iPtr1++);
		break;
	      case GL_UNSIGNED_SHORT:
		offset = (GLint)(*iPtr2++);
		break;
	      case GL_UNSIGNED_INT:
		offset = (GLint)(*iPtr3++);
		break;
	    }
            if (va->edgeFlag.enable == GL_TRUE) {
                (*va->edgeFlag.proc)(va->edgeFlag.ptr+(offset*va->edgeFlag.skip));
            }
	    for (j=0; j<__GLX_MAX_TEXTURE_UNITS; ++j) {
		if (va->texCoord[j].enable == GL_TRUE) {
		    (*va->texCoord[j].proc)(va->texCoord[j].ptr+
		    				(offset*va->texCoord[j].skip));
		}
	    }
            if (va->color.enable == GL_TRUE) {
                (*va->color.proc)(va->color.ptr+(offset*va->color.skip));
            }
            if (va->index.enable == GL_TRUE) {
                (*va->index.proc)(va->index.ptr+(offset*va->index.skip));
            }
            if (va->normal.enable == GL_TRUE) {
                (*va->normal.proc)(va->normal.ptr+(offset*va->normal.skip));
            }
            if (va->vertex.enable == GL_TRUE) {
                (*va->vertex.proc)(va->vertex.ptr+(offset*va->vertex.skip));
        }
    }
    glEnd();
}

void glDrawRangeElements(GLenum mode, GLuint start, GLuint end,
			 GLsizei count, GLenum type,
			 const GLvoid *indices)
{
    __GLXcontext *gc = __glXGetCurrentContext();

    if (end < start) {
	__glXSetError(gc, GL_INVALID_VALUE);
	return;
    }

    glDrawElements(mode,count,type,indices);
}

void glMultiDrawArrays(GLenum mode, GLint *first, GLsizei *count,
		       GLsizei primcount)
{
   GLsizei  i;

   for(i=0; i<primcount; i++) {
      if ( count[i] > 0 ) {
	  glDrawArrays( mode, first[i], count[i] );
      }
   }
}

void glMultiDrawElements(GLenum mode, const GLsizei *count,
			 GLenum type, const GLvoid ** indices,
			 GLsizei primcount)
{
   GLsizei  i;

   for(i=0; i<primcount; i++) {
      if ( count[i] > 0 ) {
	  glDrawElements( mode, count[i], type, indices[i] );
      }
   }
}

void glClientActiveTextureARB(GLenum texture)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    __GLXattribute * state = (__GLXattribute *)(gc->client_state_private);
    GLint unit = (GLint) texture - GL_TEXTURE0_ARB;

    if (unit < 0 || __GLX_MAX_TEXTURE_UNITS <= unit) {
	__glXSetError(gc, GL_INVALID_ENUM);
	return;
    }
    state->vertArray.activeTexture = unit;
}
