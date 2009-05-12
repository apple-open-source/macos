/* patch_dispatch.c
   Copyright (c) 2008 Apple Computer, Inc. All rights reserved.

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
   NONINFRINGEMENT.  IN NO EVENT SHALL THE THE ABOVE LISTED COPYRIGHT
   HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name(s) of the above
   copyright holders shall not be used in advertising or otherwise to
   promote the sale, use or other dealings in this Software without
   prior written authorization. 
*/

#include <stdio.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>
#include "glxclient.h"
#include "appledri.h"
#include "dri_driver.h"

static void DrawBuffer(GLIContext ctx, GLenum e) {
    GLenum buf[2];
    GLsizei n = 0;
    GLXContext gc;
    __DRIcontextPrivate *pcp;

    if(GL_BACK == e) {
	buf[0] = GL_BACK_LEFT;
	buf[1] = GL_BACK_RIGHT;
	n = 2;
    } else {
	buf[0] = e;
	n = 1;
    }

                                       
    gc = __glXGetCurrentContext(); 
    if (gc == NULL || !gc->isDirect)
	return;

    pcp = (__DRIcontextPrivate *) gc->driContext.private;

    if (pcp == NULL)
	return;

    pcp->ctx->disp.draw_buffers_ARB(ctx, n, buf);
}

void __glApplePatchDispatch(CGLContextObj obj) {
    obj->disp.draw_buffer = DrawBuffer;
}
