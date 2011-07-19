/*
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "scope.h"
#include "x11.h"
#include "renderscope.h"

/* Print the portion of the render request header common to all requests */
static inline void
RenderRequestHeader (FD fd, const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* RenderRequest */ ;
  PrintField (buf, 1, 1, RENDERREQUEST, RENDERREQUESTHEADER) /* minor op */ ;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");
}

void
RenderQueryVersion (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, CARD32, "major-version");
  PrintField(buf, 8, 4, CARD32, "minor-version");
}

void
RenderQueryVersionReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RenderRequest reply */ ;
  PrintField(RBf, 1, 1, RENDERREPLY, RENDERREPLYHEADER) /* RenderQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  PrintField(buf, 8, 4, CARD32, "major-version");
  PrintField(buf, 12, 4, CARD32, "minor-version");
}

void
RenderQueryPictFormats (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
RenderQueryPictFormatsReply (FD fd, const unsigned char *buf)
{
  long	n;
  long	f;
  long  s;
  long  d;
  long  v;

  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RenderRequest reply */ ;
  PrintField(RBf, 1, 1, RENDERREPLY, RENDERREPLYHEADER) /* RenderQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  n = ILong (&buf[4]);
  f = ILong (&buf[8]);
  s = ILong (&buf[12]);
  d = ILong (&buf[16]);
  v = ILong (&buf[20]);
  PrintList (&buf[32], (long) f, PICTFORMINFO, "pict-formats");
}

void
RenderQueryPictIndexValues (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTFORMAT, "format");
}

void
RenderQueryPictIndexValuesReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RenderRequest reply */ ;
  PrintField(RBf, 1, 1, RENDERREPLY, RENDERREPLYHEADER) /* RenderQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
RenderQueryDithers (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
RenderQueryDithersReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RenderRequest reply */ ;
  PrintField(RBf, 1, 1, RENDERREPLY, RENDERREPLYHEADER) /* RenderQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
}

void
RenderCreatePicture (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, DVALUE2(5+n));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 4, DRAWABLE, "drawable");
  PrintField(buf, 12, 4, PICTFORMAT, "format");
  PrintField(buf, 16, 4, PICTURE_BITMASK, "value-mask");
  PrintValues(&buf[16], 4, PICTURE_BITMASK, &buf[20], "value-list");
}

void
RenderChangePicture (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, DVALUE2(3+n));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 4, PICTURE_BITMASK, "value-mask");
  PrintValues(&buf[8], 4, PICTURE_BITMASK, &buf[12], "value-list");
}

void
RenderSetPictureClipRectangles (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
RenderFreePicture (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
}

void
RenderComposite (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField (buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "mask");
  PrintField(buf, 16, 4, PICTURE, "dest");
  PrintField(buf, 20, 2, INT16, "source x");
  PrintField(buf, 22, 2, INT16, "source y");
  PrintField(buf, 24, 2, INT16, "mask x");
  PrintField(buf, 26, 2, INT16, "mask y");
  PrintField(buf, 28, 2, INT16, "dest x");
  PrintField(buf, 30, 2, INT16, "dest y");
  PrintField(buf, 32, 2, CARD16, "width");
  PrintField(buf, 34, 2, CARD16, "height");
}

void
RenderScale (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "source");
  PrintField(buf, 8, 4, PICTURE, "dest");
  PrintField(buf, 12, 4, CARD32, "color scale");
  PrintField(buf, 16, 4, CARD32, "alpha scale");
  PrintField(buf, 20, 2, INT16, "source x");
  PrintField(buf, 22, 2, INT16, "source y");
  PrintField(buf, 24, 2, INT16, "dest x");
  PrintField(buf, 26, 2, INT16, "dest y");
  PrintField(buf, 28, 2, CARD16, "width");
  PrintField(buf, 30, 2, CARD16, "height");
}

void
RenderTrapezoids (FD fd, const unsigned char *buf)
{
  int n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField (buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 2, INT16, "source x");
  PrintField(buf, 22, 2, INT16, "source y");
  n = (CS[fd].requestLen - 6) / 10;
  PrintList(&buf[24], (long)n, TRAPEZOID, "trapezoids");
}

void
RenderTriangles (FD fd, const unsigned char *buf)
{
  long n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField (buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 2, INT16, "source x");
  PrintField(buf, 22, 2, INT16, "source y");
  n = (CS[fd].requestLen - 6) / 6;
  PrintList(&buf[24], (long)n, TRIANGLE, "triangles");
}

void
RenderTriStrip (FD fd, const unsigned char *buf)
{
  long n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField (buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 2, INT16, "source x");
  PrintField(buf, 22, 2, INT16, "source y");
  n = (CS[fd].requestLen - 6) / 2;
  PrintList(&buf[24], n, POINTFIXED, "points");
}

void
RenderTriFan (FD fd, const unsigned char *buf)
{
  long n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField (buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 2, INT16, "source x");
  PrintField(buf, 22, 2, INT16, "source y");
  n = (CS[fd].requestLen - 6) / 2;
  PrintList(&buf[24], n, POINTFIXED, "points");
}

void
RenderColorTrapezoids (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
RenderColorTriangles (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
RenderTransform (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
}

void
RenderCreateGlyphSet (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
  PrintField(buf, 8, 4, PICTFORMAT, "format");
}

void
RenderReferenceGlyphSet (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
  PrintField(buf, 4, 4, GLYPHSET, "existing");
}

void
RenderFreeGlyphSet (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
}

static void
PrintGlyphs(const unsigned char *buf, int n, char *name)
{
  const unsigned char *gids;
  const unsigned char *glyphs;
  int i;
    
  fprintf(stdout, "%s%20s:\n", Leader, name);
  gids = buf;
  glyphs = gids + 4 * n;
  for (i = 0; i < n; i++)
  {
    PrintField(gids, 0, 4, CARD32, "glyphid");
    PrintField(glyphs, 0, 2, CARD16, "width");
    PrintField(glyphs, 2, 2, CARD16, "height");
    PrintField(glyphs, 4, 2, INT16, "x");
    PrintField(glyphs, 6, 2, INT16, "y");
    PrintField(glyphs, 8, 2, INT16, "xOff");
    PrintField(glyphs, 10, 2, INT16, "yOff");
    glyphs += 12;
  }
}

void
RenderAddGlyphs (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
  PrintField(buf, 8, 4, CARD32, "nglyphs");
  PrintGlyphs(&buf[12], ILong(&buf[8]), "glyphs");
}

void
RenderAddGlyphsFromPicture (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
  /* Todo: print list of glyphs */
}

void
RenderFreeGlyphs (FD fd, const unsigned char *buf)
{
  unsigned short n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
  n = (IShort(&buf[2]) - 2);
  (void)PrintList(&buf[8], (long)n, CARD32, "glyphs");
}

static void
PrintGlyphList(const unsigned char *buf, int length, char *name, int size)
{
  short   n;

  fprintf(stdout, "%s%20s:\n", Leader, name);
  while (length >= 8)
  {
    n = IByte(&buf[0]);
    if (n != 255)
    {
      printfield(buf, 0, 1, CARD8, "length");
      PrintField(buf, 4, 2, INT16, "delta x");
      PrintField(buf, 6, 2, INT16, "delta y");
      switch (size) {
      case 1:
	PrintTString8(&buf[8], (long) n, "glyph item 8 string"); break;
      case 2:
	PrintTString16(&buf[8], (long) n, "glyph item 16 string"); break;
      case 4:
	break;
      }
      n = ((n * size + 8) + 3) & ~3;
      buf += n;
      length -= n;
    }
    else
    {
      PrintField(buf, 8, 4, GLYPHSET, "glyphset");
      buf += 12;
      length -= 12;
    }
  }
}
	       
void
RenderCompositeGlyphs8 (FD fd, const unsigned char *buf)
{
  int n;
  
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  n = (CS[fd].requestLen - 7) * 4;
  PrintField(buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 4, GLYPHSET, "glyphset");
  PrintField(buf, 24, 2, INT16, "x-src");
  PrintField(buf, 26, 2, INT16, "y-src");
  PrintGlyphList(&buf[28], n, "items", 1);
}

void
RenderCompositeGlyphs16 (FD fd, const unsigned char *buf)
{
  int n;
  
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  n = (CS[fd].requestLen - 7) * 4;
  
  PrintField(buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 4, GLYPHSET, "glyphset");
  PrintField(buf, 24, 2, INT16, "x-src");
  PrintField(buf, 26, 2, INT16, "y-src");
  PrintGlyphList(&buf[28], n, "items", 2);
}

void
RenderCompositeGlyphs32 (FD fd, const unsigned char *buf)
{
  int n;
  
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  n = (CS[fd].requestLen - 7) * 4;
  PrintField(buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 4, PICTURE, "dest");
  PrintField(buf, 16, 4, PICTFORMAT, "mask format");
  PrintField(buf, 20, 4, GLYPHSET, "glyphset");
  PrintField(buf, 24, 2, INT16, "x-src");
  PrintField(buf, 26, 2, INT16, "y-src");
  PrintGlyphList(&buf[28], n, "items", 4);
}

void
RenderFillRectangles (FD fd, const unsigned char *buf)
{
  int n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  n = (CS[fd].requestLen - 5) / 2;
  PrintField(buf, 4, 1, PICTOP, "op");
  PrintField(buf, 8, 4, PICTURE, "dest");
  PrintField(buf, 12, 8, RENDERCOLOR, "color");
  (void)PrintList(&buf[20], (long)n, RECTANGLE, "rectangles");
}

void
RenderCreateCursor (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, CURSOR, "cursor-id");
  PrintField(buf, 8, 4, PICTURE, "source");
  PrintField(buf, 12, 2, CARD16, "x");
  PrintField(buf, 14, 2, CARD16, "y");
}

void
RenderSetPictureTransform (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 9 * 8, RENDERTRANSFORM, "transform");
}

void
RenderQueryFilters (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, DRAWABLE, "drawable");
}

void
RenderQueryFiltersReply (FD fd, const unsigned char *buf)
{
  long	a;
  long	f;

  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RenderRequest reply */ ;
  PrintField(RBf, 1, 1, RENDERREPLY, RENDERREPLYHEADER) /* RenderQueryFilters reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  a = ILong (&buf[8]);
  f = ILong (&buf[12]);
  PrintList (&buf[32], a, FILTERALIAS, "aliases");
  PrintListSTR (&buf[32 + (2 * a)], f, "filters");
}


void
RenderSetPictureFilter (FD fd, const unsigned char *buf)
{
  int n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  n = IShort(&buf[8]);
  PrintString8(&buf[12], n, "filter");
  /* Todo: print list of optional values */
}

void
RenderCreateAnimCursor (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, CURSOR, "cursor-id");
  /* Todo: print list of cursor animation elements */
}

void
RenderAddTraps (FD fd, const unsigned char *buf)
{
  int n;
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 2, CARD16, "xOff");
  PrintField(buf, 10, 2, CARD16, "yOff");
  n = (CS[fd].requestLen - 3) / 10;
  PrintList(&buf[24], (long)n, TRAPEZOID, "trapezoids");
}

void
RenderCreateSolidFill (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 8, RENDERCOLOR, "color");
}

void
RenderCreateLinearGradient (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 8, POINTFIXED, "p1");
  PrintField(buf, 16, 8, POINTFIXED, "p2");
  PrintField(buf, 24, 4, CARD32, "num stops");
  /* Todo: print list of stops & colors for each */
}

void
RenderCreateRadialGradient (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 8, POINTFIXED, "inner");
  PrintField(buf, 16, 8, POINTFIXED, "outer");
  PrintField(buf, 24, 4, FIXED, "inner_radius");
  PrintField(buf, 28, 4, FIXED, "outer_radius");
  PrintField(buf, 32, 4, CARD32, "num stops");
  /* Todo: print list of stops & colors for each */
}

void
RenderCreateConicalGradient (FD fd, const unsigned char *buf)
{
  RenderRequestHeader (fd, buf);
  if (Verbose < 1)
    return;

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 8, POINTFIXED, "center");
  PrintField(buf, 16, 4, FIXED, "angle");
  PrintField(buf, 20, 4, CARD32, "num stops");
  /* Todo: print list of stops & colors for each */
}


void
RenderPictFormatError (FD fd, const unsigned char *buf)
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
RenderPictureError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTURE, "picture");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
RenderPictOpError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, PICTOP, "pictop");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
RenderGlyphSetError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, GLYPHSET, "glyphset");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

void
RenderGlyphError (FD fd, const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, CARD32, "glyph");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}
