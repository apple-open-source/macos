/* $XFree86: xc/programs/Xserver/glxStub/glxstub.c,v 1.4 1997/05/03 09:15:55 dawes Exp $ */

/*
 * Copyright 1997  The XFree86 Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
 * HARM HANEMAAYER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 */

#include "X.h"
#include "misc.h"
#include "dixstruct.h"

/*
 *  Stubs for systems that don't support loadable modules.
 *  This allows the server to be relinked via the linkkit with
 *  compatible implementations of the GLX protocol.
 */

void
GlxExtensionInit(INITARGS)
{
   ErrorF("GLX extension library not linked, use linkkit\n");
}

int
GlxInitVisuals (
#if NeedFunctionPrototypes
    VisualPtr * 	visualp,
    DepthPtr * 	depthp,
    int * 		nvisualp,
    int * 		ndepthp,
    int * 		rootDepthp,
    VisualID * 	defaultVisp,
    unsigned long 	sizes,
    int 		bitsPerRGB
#endif
) {}

