
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


/*
 * The purpose of this file is to collect all the header files that Mesa
 * uses into a single header so that we can get new compilers that support
 * pre-compiled headers to compile much faster.
 * All we do is list all the internal headers used by Mesa in this one
 * main header file, and most compilers will pre-compile all these headers
 * and use them over and over again for each source module. This makes a
 * big difference for Win32 support, because the <windows.h> headers take
 * a *long* time to compile.
 */


#ifndef SRC_ALL_H
#define SRC_ALL_H


#ifndef PC_HEADER
  This is an error.  all.h should be included only if PC_HEADER is defined.
#endif

#include "glheader.h"
#include "accum.h"
#include "attrib.h"
#include "blend.h"
#include "buffers.h"
#include "clip.h"
#include "colortab.h"
#include "config.h"
#include "context.h"
#include "convolve.h"
#include "dd.h"
#include "depth.h"
#include "dlist.h"
#include "drawpix.h"
#include "enable.h"
#include "enums.h"
#include "eval.h"
#include "extensions.h"
#include "feedback.h"
#include "fog.h"
#include "get.h"
#include "glapi.h"
#include "glthread.h"
#include "hash.h"
#include "hint.h"
#include "histogram.h"
#include "image.h"
#include "light.h"
#include "lines.h"
#include "macros.h"
#include "matrix.h"
#include "mem.h"
#include "mmath.h"
#include "pixel.h"
#include "points.h"
#include "polygon.h"
#include "rastpos.h"
#include "state.h"
#include "stencil.h"
#include "teximage.h"
#include "texobj.h"
#include "texstate.h"
#include "texstore.h"
#include "mtypes.h"
#include "varray.h"


#endif /*SRC_ALL_H*/
