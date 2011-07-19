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



#include <stdio.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#define _RENDER_SERVER_
#include "scope.h"
#include "x11.h"
#include "renderscope.h"
#include "extensions.h"

static unsigned char RENDERRequest, RENDERError;
#define RENDERNError	5


static void
render_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 0: RenderQueryVersion (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 1: RenderQueryPictFormats (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 2: RenderQueryPictIndexValues (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 3: RenderQueryDithers (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 4: RenderCreatePicture (fd, buf); break;
  case 5: RenderChangePicture (fd, buf); break;
  case 6: RenderSetPictureClipRectangles (fd, buf); break;
  case 7: RenderFreePicture (fd, buf); break;
  case 8: RenderComposite (fd, buf); break;
  case 9: RenderScale (fd, buf); break;
  case 10: RenderTrapezoids (fd, buf); break;
  case 11: RenderTriangles (fd, buf); break;
  case 12: RenderTriStrip (fd, buf); break;
  case 13: RenderTriFan (fd, buf); break;
  case 14: RenderColorTrapezoids (fd, buf); break;
  case 15: RenderColorTriangles (fd, buf); break;
  case 16: RenderTransform (fd, buf); break;
  case 17: RenderCreateGlyphSet (fd, buf); break;
  case 18: RenderReferenceGlyphSet (fd, buf); break;
  case 19: RenderFreeGlyphSet (fd, buf); break;
  case 20: RenderAddGlyphs (fd, buf); break;
  case 21: RenderAddGlyphsFromPicture (fd, buf); break;
  case 22: RenderFreeGlyphs (fd, buf); break;
  case 23: RenderCompositeGlyphs8 (fd, buf); break;
  case 24: RenderCompositeGlyphs16 (fd, buf); break;
  case 25: RenderCompositeGlyphs32 (fd, buf); break;
  case 26: RenderFillRectangles (fd, buf); break;
  case 27: RenderCreateCursor (fd, buf); break;
  case 28: RenderSetPictureTransform (fd, buf); break;
  case 29: RenderQueryFilters (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 30: RenderSetPictureFilter (fd, buf); break;
  case 31: RenderCreateAnimCursor (fd, buf); break;
  case 32: RenderAddTraps (fd, buf); break;
  case 33: RenderCreateSolidFill (fd, buf); break;
  case 34: RenderCreateLinearGradient (fd, buf); break;
  case 35: RenderCreateRadialGradient (fd, buf); break;
  case 36: RenderCreateConicalGradient (fd, buf); break;
  default:
    ExtendedRequest(fd, buf);
    ExtendedReplyExpected(fd, Major, Minor);
    break;
  }
}

static void
render_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 0: RenderQueryVersionReply (fd, buf); break;
    case 1: RenderQueryPictFormatsReply (fd, buf); break;
    case 2: RenderQueryPictIndexValuesReply (fd, buf); break;
    case 3: RenderQueryDithersReply (fd, buf); break;
    case 29: RenderQueryFiltersReply (fd, buf); break;
    default: UnknownReply(buf); break;
    }
}

static void
render_decode_error (
    FD fd,
    const unsigned char *buf)
{
    short error = IByte(&buf[1]) - RENDERError;
  
    switch (error) {
    case 0: RenderPictFormatError (fd, buf); break;
    case 1: RenderPictureError (fd, buf); break;
    case 2: RenderPictOpError (fd, buf); break;
    case 3: RenderGlyphSetError (fd, buf); break;
    case 4: RenderGlyphError (fd, buf); break;
    default:
	break;
    }
}

static int
PrintPICTURE (
    const unsigned char *buf)
{
    /* print a WINDOW -- CARD32  plus 0 = None */
    long    n = ILong (buf);
    if (n == 0)
          fprintf(stdout, "None");
    else
          fprintf(stdout, "PICTURE %08lx", n);
    return(4);
}

static int
PrintPICTFORMAT (
    const unsigned char *buf)
{
    /* print a WINDOW -- CARD32  plus 0 = None */
    long    n = ILong (buf);
    if (n == 0)
          fprintf(stdout, "None");
    else
          fprintf(stdout, "PICTFORMAT %08lx", n);
    return(4);
}

static int
PrintPICTFORMINFO (
    const unsigned char *buf)
{
  /* print a PictFormInfo */
  long	n = ILong(buf);
  short t = IByte(buf+4);
  short d = IByte(buf+5);
  
  fprintf (stdout, "PICTFORMINFO %08lx %s %d ",
	   n, t == 0 ? "Indexed" : "Direct", d);
  if (t == 0) {
    long c = ILong(buf+20);
    fprintf (stdout, "cmap %08lx", c);
  } else {
    short r = IShort(buf+8);
    short g = IShort(buf+12);
    short b = IShort(buf+16);
    short a = IShort(buf+20);
    fprintf (stdout, "%d %d %d %d", a, r, g, b);
  }
  return(28);
}

static int
PrintGLYPHSET (
    const unsigned char *buf)
{
    /* print a GLYPHSET -- CARD32  plus 0 = None */
    long    n = ILong (buf);
    if (n == 0)
          fprintf(stdout, "None");
    else
          fprintf(stdout, "GLYPHSET %08lx", n);
    return(4);
}

static int
PrintRENDERCOLOR (
    const unsigned char *buf)
{
    /* print a RENDERCOLOR */
    unsigned short  r, g, b, a;
    
    r = IShort(buf);
    g = IShort(buf+2);
    b = IShort(buf+4);
    a = IShort(buf+6);
    fprintf(stdout, "COLOR r:%04x g:%04x b:%04x a:%04x", r, g, b, a);
    return(8);
}

static int
PrintFIXED (
    const unsigned char *buf)
{
  /* print a PICTURE */
  long n = ILong (buf);
  fprintf (stdout, "FIXED %7.2f", n / 65536.0);
  return 4;
}

static int
PrintPOINTFIXED (
    const unsigned char *buf)
{
  long x = ILong (buf);
  long y = ILong (buf+4);
  fprintf (stdout, "POINT %7.2f, %7.2f", x / 65536.0, y / 65536.0);
  return 8;
}

static int
PrintTRAPEZOID (
    const unsigned char *buf)
{
    /* print a TRAPEZOID */
  PrintField (buf, 0, 4, FIXED, "top");
  PrintField (buf, 4, 4, FIXED, "bottom");
  PrintField (buf, 8, 8, POINTFIXED, "left top");
  PrintField (buf, 16, 8, POINTFIXED, "left bottom");
  PrintField (buf, 24, 8, POINTFIXED, "right top");
  PrintField (buf, 32, 8, POINTFIXED, "right bottom");
  return 40;
}

static int
PrintTRIANGLE (
    const unsigned char *buf)
{
    /* print a TRIANGLE */
    PrintField (buf, 0, 8, POINTFIXED, "p1");
    PrintField (buf, 8, 8, POINTFIXED, "p2");
    PrintField (buf,16, 8, POINTFIXED, "p3");
    return 24;
}

static int
PrintFILTERALIAS (
    const unsigned char *buf)
{
    /* print a FILTERALIAS -- CARD16  plus -1 = None */
    short    n = IShort (buf);
    if (n == -1)
          fprintf(stdout, "AliasNone");
    else
          fprintf(stdout, "FILTERALIAS %04x", n);
    return(2);
}

static int
PrintRENDERTRANSFORM(const unsigned char *buf)
{
  const unsigned char *next = buf;
  int i, j;

  for (i = 0 ; i < 3; i++) {
        for (j = 0 ; j < 3; j++) {
	    long f = ILong(next);
	    next += 4;
	    fprintf(stdout, " %7.2f", f / 65536.0);
	}
	if (i < 2) {
	    fprintf(stdout, "\n%s%20s  ", Leader, "");
	} else {
	    fprintf(stdout, "\n");
	}
  }
  return (next - buf);
}


void
InitializeRENDER (
    const unsigned char *buf)
{
  TYPE    p;
  int	errcode;

  RENDERRequest = (unsigned char)(buf[9]);
  RENDERError = (unsigned char)(buf[11]);

  DefineEValue (&TD[REQUEST], (unsigned long) RENDERRequest, "RenderRequest");
  DefineEValue (&TD[REPLY], (unsigned long) RENDERRequest, "RenderReply");
    
  DefineEValue (&TD[ERROR], (unsigned long) RENDERError + 0, "BadPictFormat");
  DefineEValue (&TD[ERROR], (unsigned long) RENDERError + 1, "BadPicture");
  DefineEValue (&TD[ERROR], (unsigned long) RENDERError + 2, "BadPictOp");
  DefineEValue (&TD[ERROR], (unsigned long) RENDERError + 3, "BadGlyphSet");
  DefineEValue (&TD[ERROR], (unsigned long) RENDERError + 4, "BadGlyph");

  p = DefineType(RENDERREQUEST, ENUMERATED, "RENDERREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 0L, "RenderQueryVersion");
  DefineEValue(p, 1L, "RenderQueryPictFormats");
  DefineEValue(p, 2L, "RenderQueryPictIndexValues");
  DefineEValue(p, 3L, "RenderQueryDithers");
  DefineEValue(p, 4L, "RenderCreatePicture");
  DefineEValue(p, 5L, "RenderChangePicture");
  DefineEValue(p, 6L, "RenderSetPictureClipRectangles");
  DefineEValue(p, 7L, "RenderFreePicture");
  DefineEValue(p, 8L, "RenderComposite");
  DefineEValue(p, 9L, "RenderScale");
  DefineEValue(p, 10L, "RenderTrapezoids");
  DefineEValue(p, 11L, "RenderTriangles");
  DefineEValue(p, 12L, "RenderTriStrip");
  DefineEValue(p, 13L, "RenderTriFan");
  DefineEValue(p, 14L, "RenderColorTrapezoids");
  DefineEValue(p, 15L, "RenderColorTriangles");
  DefineEValue(p, 16L, "RenderTransform");
  DefineEValue(p, 17L, "RenderCreateGlyphSet");
  DefineEValue(p, 18L, "RenderReferenceGlyphSet");
  DefineEValue(p, 19L, "RenderFreeGlyphSet");
  DefineEValue(p, 20L, "RenderAddGlyphs");
  DefineEValue(p, 21L, "RenderAddGlyphsFromPicture");
  DefineEValue(p, 22L, "RenderFreeGlyphs");
  DefineEValue(p, 23L, "RenderCompositeGlyphs8");
  DefineEValue(p, 24L, "RenderCompositeGlyphs16");
  DefineEValue(p, 25L, "RenderCompositeGlyphs32");
  DefineEValue(p, 26L, "RenderFillRectangles");
  /* Added in 0.5: */
  DefineEValue(p, 27L, "RenderCreateCursor");
  /* Added in 0.6: */
  DefineEValue(p, 28L, "RenderSetPictureTransform");
  DefineEValue(p, 29L, "RenderQueryFilters");
  DefineEValue(p, 30L, "RenderSetPictureFilter");
  /* Added in 0.8: */
  DefineEValue(p, 31L, "RenderCreateAnimCursor");
  /* Added in 0.9: */
  DefineEValue(p, 32L, "RenderAddTraps");
  /* Added in 0.10: */
  DefineEValue(p, 33L, "RenderCreateSolidFill");
  DefineEValue(p, 34L, "RenderCreateLinearGradient");
  DefineEValue(p, 35L, "RenderCreateRadialGradient");
  DefineEValue(p, 36L, "RenderCreateConicalGradient");
  /* no new requests in 0.11 */

  p = DefineType(RENDERREPLY, ENUMERATED, "RENDERREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "QueryVersion");
  DefineEValue (p, 1L, "QueryPictFormats");
  DefineEValue (p, 2L, "QueryPictIndexValues");
  DefineEValue (p, 3L, "QueryDithers");
  /* Added in 0.6: */
  DefineEValue (p, 29L, "QueryFilters");

  DefineType(PICTURE, BUILTIN, "PICTURE", PrintPICTURE);
  DefineType(PICTFORMAT, BUILTIN, "PICTFORMAT", PrintPICTFORMAT);
  DefineType(GLYPHSET, BUILTIN, "GLYPHSET", PrintGLYPHSET);
  DefineType(RENDERCOLOR, BUILTIN, "RENDERCOLOR", PrintRENDERCOLOR);
  DefineType(PICTFORMINFO, BUILTIN, "PICTFORMINFO", PrintPICTFORMINFO);
  
  p = DefineType(PICTURE_BITMASK, SET, "PICTURE_BITMASK", (PrintProcType) PrintSET);

  DefineValues(p, 0x00000001L, 1, BOOL, "repeat");
  DefineValues(p, 0x00000002L, 1, PICTURE, "alpha-map");
  DefineValues(p, 0x00000004L, 1, INT16, "alpha-x-origin");
  DefineValues(p, 0x00000008L, 1, INT16, "alpha-y-origin");
  DefineValues(p, 0x00000010L, 1, INT16, "clip-x-origin");
  DefineValues(p, 0x00000020L, 1, INT16, "clip-y-origin");
  DefineValues(p, 0x00000040L, 1, PIXMAP, "clip-mask");
  DefineValues(p, 0x00000080L, 1, BOOL, "graphics-exposures");
  DefineValues(p, 0x00000100L, 1, SUBWINMODE, "repeat");
  DefineValues(p, 0x00000200L, 1, BOOL, "poly-edge");
  DefineValues(p, 0x00000400L, 1, BOOL, "poly-mode");
  DefineValues(p, 0x00000800L, 1, ATOM, "dither");
  DefineValues(p, 0x00001000L, 1, BOOL, "component-alpha");

  p = DefineType(PICTOP, ENUMERATED, "PICTOP", (PrintProcType) PrintENUMERATED);
  DefineEValue (p,  0L, "Clear");
  DefineEValue (p,  1L, "Src");
  DefineEValue (p,  2L, "Dst");
  DefineEValue (p,  3L, "Over");
  DefineEValue (p,  4L, "OverReverse");
  DefineEValue (p,  5L, "In");
  DefineEValue (p,  6L, "InReverse");
  DefineEValue (p,  7L, "Out");
  DefineEValue (p,  8L, "OutReverse");
  DefineEValue (p,  9L, "Atop");
  DefineEValue (p,  10L, "AtopReverse");
  DefineEValue (p,  11L, "Xor");
  DefineEValue (p,  12L, "Add");
  DefineEValue (p,  13L, "Saturate");

  /* Operators only available in version 0.2 */
  DefineEValue (p,  0x10, "PictOpDisjointClear");
  DefineEValue (p,  0x11, "PictOpDisjointSrc");
  DefineEValue (p,  0x12, "PictOpDisjointDst");
  DefineEValue (p,  0x13, "PictOpDisjointOver");
  DefineEValue (p,  0x14, "PictOpDisjointOverReverse");
  DefineEValue (p,  0x15, "PictOpDisjointIn");
  DefineEValue (p,  0x16, "PictOpDisjointInReverse");
  DefineEValue (p,  0x17, "PictOpDisjointOut");
  DefineEValue (p,  0x18, "PictOpDisjointOutReverse");
  DefineEValue (p,  0x19, "PictOpDisjointAtop");
  DefineEValue (p,  0x1a, "PictOpDisjointAtopReverse");
  DefineEValue (p,  0x1b, "PictOpDisjointXor");

  DefineEValue (p,  0x20, "PictOpConjointClear");
  DefineEValue (p,  0x21, "PictOpConjointSrc");
  DefineEValue (p,  0x22, "PictOpConjointDst");
  DefineEValue (p,  0x23, "PictOpConjointOver");
  DefineEValue (p,  0x24, "PictOpConjointOverReverse");
  DefineEValue (p,  0x25, "PictOpConjointIn");
  DefineEValue (p,  0x26, "PictOpConjointInReverse");
  DefineEValue (p,  0x27, "PictOpConjointOut");
  DefineEValue (p,  0x28, "PictOpConjointOutReverse");
  DefineEValue (p,  0x29, "PictOpConjointAtop");
  DefineEValue (p,  0x2a, "PictOpConjointAtopReverse");
  DefineEValue (p,  0x2b, "PictOpConjointXor");

  /* Operators only available in version 0.11 */
  DefineEValue (p,  0x30, "PictOpMultiply");
  DefineEValue (p,  0x31, "PictOpScreen");
  DefineEValue (p,  0x32, "PictOpOverlay");
  DefineEValue (p,  0x33, "PictOpDarken");
  DefineEValue (p,  0x34, "PictOpLighten");
  DefineEValue (p,  0x35, "PictOpColorDodge");
  DefineEValue (p,  0x36, "PictOpColorBurn");
  DefineEValue (p,  0x37, "PictOpHardLight");
  DefineEValue (p,  0x38, "PictOpSoftLight");
  DefineEValue (p,  0x39, "PictOpDifference");
  DefineEValue (p,  0x3a, "PictOpExclusion");
  DefineEValue (p,  0x3b, "PictOpHSLHue");
  DefineEValue (p,  0x3c, "PictOpHSLSaturation");
  DefineEValue (p,  0x3d, "PictOpHSLColor");
  DefineEValue (p,  0x3e, "PictOpHSLLuminosity");

  DefineType(FIXED, BUILTIN, "FIXED", PrintFIXED);
  DefineType(POINTFIXED, BUILTIN, "POINTFIXED", PrintPOINTFIXED);
  DefineType(TRIANGLE, RECORD, "TRIANGLE", PrintTRIANGLE);
  DefineType(TRAPEZOID, RECORD, "TRAPEZOID", PrintTRAPEZOID);
  DefineType(FILTERALIAS, BUILTIN, "FILTERALIAS", PrintFILTERALIAS);
  DefineType(RENDERTRANSFORM, BUILTIN, "RENDERTRANSFORM", PrintRENDERTRANSFORM);

  InitializeExtensionDecoder(RENDERRequest, render_decode_req,
			     render_decode_reply);
  for (errcode = RENDERError; errcode < (RENDERError + RENDERNError) ;
       errcode ++) {
      InitializeExtensionErrorDecoder(errcode, render_decode_error);
  }
}
