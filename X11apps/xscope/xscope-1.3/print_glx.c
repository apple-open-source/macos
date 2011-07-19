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

#include "scope.h"
#include "x11.h"
#include "glxscope.h"

/* Print the portion of the glx request header common to all requests */
static inline void
GlxRequestHeader (FD fd, const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER);
  PrintField (buf, 1, 1, GLXREQUEST, GLXREQUESTHEADER);
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");
}

void
GLXRender (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXRenderLarge (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXCreateContext (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXDestroyContext (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXMakeCurrent (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void GLXMakeCurrentReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXIsDirect (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXIsDirectReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXQueryVersion (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXQueryVersionReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXWaitGL (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXWaitX (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXCopyContext (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXSwapBuffers (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXUseXFont (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXCreateGLXPixmap (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXGetVisualConfigs (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXGetVisualConfigsReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXDestroyGLXPixmap (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXVendorPrivate (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXVendorPrivateWithReply (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXVendorPrivateWithReplyReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXQueryExtensionsString (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXQueryExtensionsStringReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXQueryServerString (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXQueryServerStringReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXClientInfo (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXGetFBConfigs (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXGetFBConfigsReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXCreatePixmap (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXDestroyPixmap (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXCreateNewContext (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXQueryContext (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXQueryContextReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXMakeContextCurrent (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXMakeContextCurrentReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXCreatePbuffer (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXDestroyPbuffer (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXGetDrawableAttributes (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
 GLXGetDrawableAttributesReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER);
  PrintField(RBf, 1, 1, GLXREPLY, GLXREPLYHEADER);
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
GLXChangeDrawableAttributes (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXCreateWindow (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXDestroyWindow (FD fd, const unsigned char *buf)
{
  GlxRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
GLXBadContextError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadContextStateError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadDrawableError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadPixmapError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadContextTagError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadCurrentWindowError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadRenderRequestError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadLargeRequestError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXUnsupportedPrivateRequestError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadFBConfigError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadPbufferError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadCurrentDrawableError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
GLXBadWindowError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTFORMAT, "format");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}
