
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


#ifndef PB_H
#define PB_H


#include "mtypes.h"
#include "swrast.h"
#include "colormac.h"


/*
 * Pixel buffer size, must be larger than MAX_WIDTH.
 */
#define PB_SIZE (3*MAX_WIDTH)


struct pixel_buffer {
   GLchan currentColor[4];     /* Current color, for subsequent pixels */
   GLuint currentIndex;        /* Current index, for subsequent pixels */
   GLuint count;               /* Number of pixels in buffer */
   GLboolean mono;             /* Same color or index for all pixels? */
   GLboolean haveSpec;         /* any specular colors? */
   GLboolean haveCoverage;     /* apply AA coverage? */

   GLint x[PB_SIZE];           /* X window coord in [0,MAX_WIDTH) */
   GLint y[PB_SIZE];           /* Y window coord in [0,MAX_HEIGHT) */
   GLdepth z[PB_SIZE];         /* Z window coord in [0,Visual.MaxDepth] */
   GLfloat fog[PB_SIZE];       /* Fog window coord in [0,1] */
   GLchan rgba[PB_SIZE][4];    /* Colors */
   GLchan spec[PB_SIZE][3];    /* Separate specular colors */
   GLuint index[PB_SIZE];      /* Color indexes */
   GLfloat coverage[PB_SIZE];  /* Antialiasing coverage in [0,1] */
   GLfloat s[MAX_TEXTURE_UNITS][PB_SIZE];	/* Texture S coordinates */
   GLfloat t[MAX_TEXTURE_UNITS][PB_SIZE];	/* Texture T coordinates */
   GLfloat u[MAX_TEXTURE_UNITS][PB_SIZE];	/* Texture R coordinates */
   GLfloat lambda[MAX_TEXTURE_UNITS][PB_SIZE];  /* Texture lambda values */
};



/*
 * Set the color used for all subsequent pixels in the buffer.
 */
#define PB_SET_COLOR( PB, R, G, B, A )		\
do {						\
   if ((PB)->count > 0)				\
      (PB)->mono = GL_FALSE;			\
   (PB)->currentColor[RCOMP] = (R);		\
   (PB)->currentColor[GCOMP] = (G);		\
   (PB)->currentColor[BCOMP] = (B);		\
   (PB)->currentColor[ACOMP] = (A);		\
} while (0)


/*
 * Set the color index used for all subsequent pixels in the buffer.
 */
#define PB_SET_INDEX( PB, I )			\
do {						\
   if ((PB)->count > 0)				\
      (PB)->mono = GL_FALSE;			\
   (PB)->currentIndex = (I);			\
} while (0)


/*
 * "write" a pixel using current color or index
 */
#define PB_WRITE_PIXEL( PB, X, Y, Z, FOG )		\
do {							\
   GLuint count = (PB)->count;				\
   (PB)->x[count] = X;					\
   (PB)->y[count] = Y;					\
   (PB)->z[count] = Z;					\
   (PB)->fog[count] = FOG;				\
   COPY_CHAN4((PB)->rgba[count], (PB)->currentColor);	\
   (PB)->index[count] = (PB)->currentIndex;		\
   (PB)->count++;					\
} while (0)


/*
 * "write" an RGBA pixel
 */
#define PB_WRITE_RGBA_PIXEL( PB, X, Y, Z, FOG, R, G, B, A )	\
do {								\
   GLuint count = (PB)->count;					\
   (PB)->x[count] = X;						\
   (PB)->y[count] = Y;						\
   (PB)->z[count] = Z;						\
   (PB)->fog[count] = FOG;					\
   (PB)->rgba[count][RCOMP] = R;				\
   (PB)->rgba[count][GCOMP] = G;				\
   (PB)->rgba[count][BCOMP] = B;				\
   (PB)->rgba[count][ACOMP] = A;				\
   (PB)->mono = GL_FALSE;					\
   (PB)->count++;						\
} while (0)


/*
 * "write" a color-index pixel
 */
#define PB_WRITE_CI_PIXEL( PB, X, Y, Z, FOG, I ) \
do {						\
   GLuint count = (PB)->count;			\
   (PB)->x[count] = X;				\
   (PB)->y[count] = Y;				\
   (PB)->z[count] = Z;				\
   (PB)->fog[count] = FOG;			\
   (PB)->index[count] = I;			\
   (PB)->mono = GL_FALSE;			\
   (PB)->count++;				\
} while (0)



/*
 * "write" an RGBA pixel with texture coordinates
 */
#define PB_WRITE_TEX_PIXEL( PB, X, Y, Z, FOG, R, G, B, A, S, T, U )	\
do {							\
   GLuint count = (PB)->count;				\
   (PB)->x[count] = X;					\
   (PB)->y[count] = Y;					\
   (PB)->z[count] = Z;					\
   (PB)->fog[count] = FOG;				\
   (PB)->rgba[count][RCOMP] = R;			\
   (PB)->rgba[count][GCOMP] = G;			\
   (PB)->rgba[count][BCOMP] = B;			\
   (PB)->rgba[count][ACOMP] = A;			\
   (PB)->s[0][count] = S;				\
   (PB)->t[0][count] = T;				\
   (PB)->u[0][count] = U;				\
   (PB)->mono = GL_FALSE;				\
   (PB)->count++;					\
} while (0)


/*
 * "write" an RGBA pixel with multiple texture coordinates
 */
#define PB_WRITE_MULTITEX_PIXEL( PB, X, Y, Z, FOG, R, G, B, A, TEXCOORDS ) \
do {									\
   GLuint count = (PB)->count;						\
   GLuint unit;								\
   (PB)->x[count] = X;							\
   (PB)->y[count] = Y;							\
   (PB)->z[count] = Z;							\
   (PB)->fog[count] = FOG;						\
   (PB)->rgba[count][RCOMP] = R;					\
   (PB)->rgba[count][GCOMP] = G;					\
   (PB)->rgba[count][BCOMP] = B;					\
   (PB)->rgba[count][ACOMP] = A;					\
   for (unit = 0; unit < ctx->Const.MaxTextureUnits; unit++) {		\
      if (ctx->Texture.Unit[unit]._ReallyEnabled) {			\
         (PB)->s[unit][count] = TEXCOORDS[unit][0];			\
         (PB)->t[unit][count] = TEXCOORDS[unit][1];			\
         (PB)->u[unit][count] = TEXCOORDS[unit][2];			\
      }									\
   }									\
   (PB)->mono = GL_FALSE;						\
   (PB)->count++;							\
} while (0)


/*
 * "write" an RGBA pixel with multiple texture coordinates and specular color
 */
#define PB_WRITE_MULTITEX_SPEC_PIXEL( PB, X, Y, Z, FOG, R, G, B, A, SR, SG, SB, TEXCOORDS )\
do {									\
   GLuint count = (PB)->count;						\
   GLuint unit;								\
   (PB)->haveSpec = GL_TRUE;						\
   (PB)->x[count] = X;							\
   (PB)->y[count] = Y;							\
   (PB)->z[count] = Z;							\
   (PB)->fog[count] = FOG;						\
   (PB)->rgba[count][RCOMP] = R;					\
   (PB)->rgba[count][GCOMP] = G;					\
   (PB)->rgba[count][BCOMP] = B;					\
   (PB)->rgba[count][ACOMP] = A;					\
   (PB)->spec[count][RCOMP] = SR;					\
   (PB)->spec[count][GCOMP] = SG;					\
   (PB)->spec[count][BCOMP] = SB;					\
   for (unit = 0; unit < ctx->Const.MaxTextureUnits; unit++) {		\
      if (ctx->Texture.Unit[unit]._ReallyEnabled) {			\
         (PB)->s[unit][count] = TEXCOORDS[unit][0];			\
         (PB)->t[unit][count] = TEXCOORDS[unit][1];			\
         (PB)->u[unit][count] = TEXCOORDS[unit][2];			\
      }									\
   }									\
   (PB)->mono = GL_FALSE;						\
   (PB)->count++;							\
} while (0)


#define PB_COVERAGE(PB, COVERAGE)				\
   (PB)->coverage[(PB)->count] = COVERAGE;


/*
 * Call this function at least every MAX_WIDTH pixels:
 */
#define PB_CHECK_FLUSH( CTX, PB )			\
do {							\
   if ((PB)->count >= PB_SIZE - MAX_WIDTH) {		\
      _mesa_flush_pb( CTX );				\
   }							\
} while(0)


extern struct pixel_buffer *_mesa_alloc_pb(void);

extern void _mesa_flush_pb( GLcontext *ctx );


#endif
