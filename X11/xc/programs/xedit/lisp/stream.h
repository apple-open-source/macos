/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/stream.h,v 1.8 2002/12/06 03:25:27 paulo Exp $ */

#ifndef Lisp_stream_h
#define Lisp_stream_h

#include "io.h"
#include "internal.h"

void LispStreamInit(void);

LispObj *Lisp_DeleteFile(LispBuiltin*);
LispObj *Lisp_RenameFile(LispBuiltin*);
LispObj *Lisp_InputStreamP(LispBuiltin*);
LispObj *Lisp_OpenStreamP(LispBuiltin*);
LispObj *Lisp_OutputStreamP(LispBuiltin*);
LispObj *Lisp_Open(LispBuiltin*);
LispObj *Lisp_MakePipe(LispBuiltin*);
LispObj *Lisp_PipeBroken(LispBuiltin*);
LispObj *Lisp_PipeErrorStream(LispBuiltin*);
LispObj *Lisp_PipeInputDescriptor(LispBuiltin*);
LispObj *Lisp_PipeErrorDescriptor(LispBuiltin*);
LispObj *Lisp_Close(LispBuiltin*);
LispObj *Lisp_Listen(LispBuiltin*);
LispObj *Lisp_Streamp(LispBuiltin*);
LispObj *Lisp_MakeStringInputStream(LispBuiltin*);
LispObj *Lisp_MakeStringOutputStream(LispBuiltin*);
LispObj *Lisp_GetOutputStreamString(LispBuiltin*);

#endif /* Lisp_stream_h */
