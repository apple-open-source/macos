/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *
 */


#include <stdio.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#define _GLX_SERVER_
#include "scope.h"
#include "x11.h"
#include "glxscope.h"
#include "extensions.h"

static unsigned char GLXRequest, GLXError, GLXEvent;
#define GLXNError	13


static void
glx_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 1: GLXRender (fd, buf); break;
  case 2: GLXRenderLarge (fd, buf); break;
  case 3: GLXCreateContext (fd, buf); break;
  case 4: GLXDestroyContext (fd, buf); break;
  case 5: GLXMakeCurrent (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 6: GLXIsDirect (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 7: GLXQueryVersion (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 8: GLXWaitGL (fd, buf); break;
  case 9: GLXWaitX (fd, buf); break;
  case 10: GLXCopyContext (fd, buf); break;
  case 11: GLXSwapBuffers (fd, buf); break;
  case 12: GLXUseXFont (fd, buf); break;
  case 13: GLXCreateGLXPixmap (fd, buf); break;
  case 14: GLXGetVisualConfigs (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 15: GLXDestroyGLXPixmap (fd, buf); break;
  case 16: GLXVendorPrivate (fd, buf); break;
  case 17: GLXVendorPrivateWithReply (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 18: GLXQueryExtensionsString (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 19: GLXQueryServerString (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 20: GLXClientInfo (fd, buf); break;
  case 21: GLXGetFBConfigs (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 22: GLXCreatePixmap (fd, buf); break;
  case 23: GLXDestroyPixmap (fd, buf); break;
  case 24: GLXCreateNewContext (fd, buf); break;
  case 25: GLXQueryContext (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 26: GLXMakeContextCurrent (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 27: GLXCreatePbuffer (fd, buf); break;
  case 28: GLXDestroyPbuffer (fd, buf); break;
  case 29: GLXGetDrawableAttributes (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 30: GLXChangeDrawableAttributes (fd, buf); break;
  case 31: GLXCreateWindow (fd, buf); break;
  case 32: GLXDestroyWindow (fd, buf); break;
  default:
    ExtendedRequest(fd, buf);
    ExtendedReplyExpected(fd, Major, Minor);
    break;
  }
}

static void
glx_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 5: GLXMakeCurrentReply (fd, buf); break;
    case 6: GLXIsDirectReply (fd, buf); break;
    case 7: GLXQueryVersionReply (fd, buf); break;
    case 14: GLXGetVisualConfigsReply (fd, buf); break;
    case 17: GLXVendorPrivateWithReplyReply (fd, buf); break;
    case 18: GLXQueryExtensionsStringReply (fd, buf); break;
    case 19: GLXQueryServerStringReply (fd, buf); break;
    case 21: GLXGetFBConfigsReply (fd, buf); break;
    case 25: GLXQueryContextReply (fd, buf); break;
    case 26: GLXMakeContextCurrentReply (fd, buf); break;
    case 29: GLXGetDrawableAttributesReply (fd, buf); break;
    default: UnknownReply(buf); break;
    }
}

static void
glx_decode_error (
    FD fd,
    const unsigned char *buf)
{
    short error = IByte(&buf[1]) - GLXError;
  
    switch (error) {
    case 0: GLXBadContextError (fd, buf); break;
    case 1: GLXBadContextStateError (fd, buf); break;
    case 2: GLXBadDrawableError (fd, buf); break;
    case 3: GLXBadPixmapError (fd, buf); break;
    case 4: GLXBadContextTagError (fd, buf); break;
    case 5: GLXBadCurrentWindowError (fd, buf); break;
    case 6: GLXBadRenderRequestError (fd, buf); break;
    case 7: GLXBadLargeRequestError (fd, buf); break;
    case 8: GLXUnsupportedPrivateRequestError (fd, buf); break;
    case 9: GLXBadFBConfigError (fd, buf); break;
    case 10: GLXBadPbufferError (fd, buf); break;
    case 11: GLXBadCurrentDrawableError (fd, buf); break;
    case 12: GLXBadWindowError (fd, buf); break;
    default:
	break;
    }
}

static void
glx_decode_event (
    FD fd,
    const unsigned char *buf)
{
  short	event = IByte(&buf[0]) - GLXEvent;

  switch (event) {
    break;
  }
}

void
InitializeGLX (const unsigned char *buf)
{
  TYPE    p;
  int	errcode;

  GLXRequest = (unsigned char)(buf[9]);
  GLXEvent = (unsigned char)(buf[10]);
  GLXError = (unsigned char)(buf[11]);

  DefineEValue (&TD[REQUEST], (unsigned long) GLXRequest, "GlxRequest");
  DefineEValue (&TD[REPLY], (unsigned long) GLXRequest, "GlxReply");

  DefineEValue (&TD[EVENT], (unsigned long) GLXEvent, "GlxEvent");

  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 0, "GLXBadContext");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 1, "GLXBadContextState");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 2, "GLXBadDrawable");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 3, "GLXBadPixmap");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 4, "GLXBadContextTag");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 5, "GLXBadCurrentWindow");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 6, "GLXBadRenderRequest");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 7, "GLXBadLargeRequest");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 8, "GLXUnsupportedPrivateRequest");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 9, "GLXBadFBConfig");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 10, "GLXBadPbuffer");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 11, "GLXBadCurrentDrawable");
  DefineEValue (&TD[ERROR], (unsigned long) GLXError + 12, "GLXBadWindow");


  p = DefineType(GLXREQUEST, ENUMERATED, "GLXREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 1L, "GLXRender");
  DefineEValue(p, 2L, "GLXRenderLarge");
  DefineEValue(p, 3L, "GLXCreateContext");
  DefineEValue(p, 4L, "GLXDestroyContext");
  DefineEValue(p, 5L, "GLXMakeCurrent");
  DefineEValue(p, 6L, "GLXIsDirect");
  DefineEValue(p, 7L, "GLXQueryVersion");
  DefineEValue(p, 8L, "GLXWaitGL");
  DefineEValue(p, 9L, "GLXWaitX");
  DefineEValue(p, 10L, "GLXCopyContext");
  DefineEValue(p, 11L, "GLXSwapBuffers");
  DefineEValue(p, 12L, "GLXUseXFont");
  DefineEValue(p, 13L, "GLXCreateGLXPixmap");
  DefineEValue(p, 14L, "GLXGetVisualConfigs");
  DefineEValue(p, 15L, "GLXDestroyGLXPixmap");
  DefineEValue(p, 16L, "GLXVendorPrivate");
  DefineEValue(p, 17L, "GLXVendorPrivateWithReply");
  DefineEValue(p, 18L, "GLXQueryExtensionsString");
  DefineEValue(p, 19L, "GLXQueryServerString");
  DefineEValue(p, 20L, "GLXClientInfo");
  DefineEValue(p, 21L, "GLXGetFBConfigs");
  DefineEValue(p, 22L, "GLXCreatePixmap");
  DefineEValue(p, 23L, "GLXDestroyPixmap");
  DefineEValue(p, 24L, "GLXCreateNewContext");
  DefineEValue(p, 25L, "GLXQueryContext");
  DefineEValue(p, 26L, "GLXMakeContextCurrent");
  DefineEValue(p, 27L, "GLXCreatePbuffer");
  DefineEValue(p, 28L, "GLXDestroyPbuffer");
  DefineEValue(p, 29L, "GLXGetDrawableAttributes");
  DefineEValue(p, 30L, "GLXChangeDrawableAttributes");
  DefineEValue(p, 31L, "GLXCreateWindow");
  DefineEValue(p, 32L, "GLXDestroyWindow");

  p = DefineType(GLXREPLY, ENUMERATED, "GLXREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 17L, "GLXVendorPrivateWithReply");
  DefineEValue(p, 5L, "GLXMakeCurrentReply");
  DefineEValue(p, 6L, "GLXIsDirectReply");
  DefineEValue(p, 7L, "GLXQueryVersionReply");
  DefineEValue(p, 14L, "GLXGetVisualConfigsReply");
  DefineEValue(p, 17L, "GLXVendorPrivateWithReplyReply");
  DefineEValue(p, 18L, "GLXQueryExtensionsStringReply");
  DefineEValue(p, 19L, "GLXQueryServerStringReply");
  DefineEValue(p, 21L, "GLXGetFBConfigsReply");
  DefineEValue(p, 25L, "GLXQueryContextReply");
  DefineEValue(p, 26L, "GLXMakeContextCurrentReply");
  DefineEValue(p, 29L, "GLXGetDrawableAttributesReply");

  p = DefineType(GLXEVENT, ENUMERATED, "GLXEVENT", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "PbufferClobberEvent");
  DefineEValue (p, 1L, "BufferSwapComplete");

  InitializeExtensionDecoder(GLXRequest, glx_decode_req,
			     glx_decode_reply);
  InitializeExtensionEventDecoder(GLXEvent, glx_decode_event);
  for (errcode = GLXError; errcode < (GLXError + GLXNError) ;
       errcode ++) {
      InitializeExtensionErrorDecoder(errcode, glx_decode_error);
  }
}
