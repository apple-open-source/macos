
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
#include "mem.h"
#include "s_context.h"
#include "s_scissor.h"


/*
 * Apply the scissor test to a span of pixels.
 * Return:  0 = all pixels in the span are outside the scissor box.
 *          n = mask[n-1] is the last one to be visible
 */
GLuint
_mesa_scissor_span(GLcontext *ctx, GLuint n, GLint x, GLint y, GLubyte mask[])
{
   /* first check if whole span is outside the scissor box */
   if (y < ctx->DrawBuffer->_Ymin
       || y >= ctx->DrawBuffer->_Ymax
       || x > ctx->DrawBuffer->_Xmax
       || x + (GLint) n - 1 < ctx->DrawBuffer->_Xmin) {
      return 0;
   }
   else {
      const GLint xMin = ctx->DrawBuffer->_Xmin;
      const GLint xMax = ctx->DrawBuffer->_Xmax;

      if (x < xMin)
         BZERO(mask, (xMin-x) * sizeof(GLubyte));

      if (x + (GLint) n > xMax) {
         BZERO(mask + xMax - x, (n - (xMax - x)) * sizeof(GLubyte));
         return (GLuint) (xMax - x);
      }

      return n;
   }
}
