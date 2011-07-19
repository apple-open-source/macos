/*
 * Copyright 1996 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, Network Computing Devices
 */

#include "scope.h"
#include "x11.h"
#include "wcpscope.h"

void
WcpQueryVersion (
    FD fd,
    register const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
WcpQueryVersionReply (
    FD fd,
    const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* WcpRequest reply */ ;
  PrintField(RBf, 1, 1, WCPREPLY, WCPREPLYHEADER) /* WcpQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  PrintField(buf, 8, 2, CARD16, "major-version");
  PrintField(buf, 10, 2, CARD16, "minor-version");
}
    
#define NextByte(dst)  if (!len--) { error = "Out of data"; break; } dst = *data++;

static void
WcpAnalyzeImage1RLL (
    const char *buf,
    int	    len,
    int	    width,
    int	    height)
{
    unsigned char   byte;
    unsigned char   *data = (unsigned char *) buf;
    int		    x, y;
    const char	    *error = NULL;
    int		    bytewidth;
    int		    i;
    int		    w;
    
    y = 0;
    bytewidth = (width +  7) >> 3;
    while (!error && height--)
    {
	x = 0;
	if (Verbose > 2)
	    fprintf (stdout, "%s %9d %9d:", Leader, y, (char *) data - buf);
	NextByte (byte);
	if (Verbose > 2)
	    fprintf (stdout, " %2x", byte);
	switch (byte) {
	case 0xff:
	    for (i = 0; i < bytewidth; i++)
	    {
		NextByte(byte);
	    }
	    break;
	default:
	    w = width;
	    while (w)
	    {
		NextByte(byte);
		if (Verbose > 2)
		    fprintf (stdout, " %3d", byte);
		if (byte > w)
		{
		    error = "bad run";
		    break;
		}
		x += byte;
		w -= byte;
	    }
	    break;
	}
	if (Verbose > 2)
	    fprintf (stdout, "\n");
	y++;
    }
    if (error)
	fprintf (stdout, "%s%20s: %d, %d %d\n", Leader, error, 
		 x, y - 1, (char *) data - buf);
}

static void
WcpAnalyzeImageNRLL (
    const char *buf,
    int	    len,
    int	    width,
    int	    height,
    int	    bytes)
{
}

#define MAX_LRU_SHORT	0xe
#define LRU_LONG	0xf
#define MAX_LRU_RUN	(MAX_LRU_SHORT + 0xff)

#define LRU_CACHE	0xf
#define LRU_MISS	0xf

typedef unsigned long	PIXEL;

static void
init_cache (
    PIXEL   cache[LRU_CACHE],
    int	    BPP)
{
    int	    e;
    
    /*
     * initialize cache values
     */
    for (e = 0; e < 8; e++)
    {
	cache[e] = e;
	cache[LRU_CACHE-e-1] = ((1 << BPP) - 1) - e;
    }
}

static void
push (
    PIXEL cache[LRU_CACHE],
    PIXEL pixel)
{
    int    e;

    e = LRU_CACHE - 1;
    while (--e >= 0)
	cache[e+1] = cache[e];
    cache[0] = pixel;
}

static PIXEL
use (
    PIXEL  cache[LRU_CACHE],
    int    e)
{
    PIXEL   tmp;

    tmp = cache[e];
    if (e)
    {
	while (--e >= 0)
	    cache[e+1] = cache[e];
	cache[0] = tmp;
    }
    return tmp;
}

static void
WcpAnalyzeImageNLRU (
    const char *buf,
    int	    len,
    int	    width,
    int	    height,
    int	    bytes)
{
    unsigned char   *data = (unsigned char *) buf;
    unsigned char   byte;
    int		    x, y;
    int		    w;
    int		    i;
    int		    bytewidth;
    int		    cache;
    unsigned long   run;
    PIXEL	    lru[LRU_CACHE];
    PIXEL	    pix;
    const char	    *error = NULL;

    y = 0;
    bytewidth = width * bytes;
    while (!error && height--)
    {
	x = 0;
	if (Verbose > 2)
	    fprintf (stdout, "%s %9d %9d:", Leader, y, (char *) data - buf);
	NextByte (byte);
	if (Verbose > 2)
	    fprintf (stdout, " %2x", byte);
	switch (byte) {
	case 0xff:
	    for (i = 0; i < bytewidth; i++)
	    {
		NextByte (byte);
	    }
	    break;
	default:
	    init_cache (lru, bytes * 8);
	    w = width;
	    while (w)
	    {
		NextByte (byte);
		run = byte >> 4;
		cache = byte & 0xf;
		if (run == LRU_LONG)
		{
		    NextByte (byte);
		    run = (int) byte + MAX_LRU_SHORT;
		}
		run++;
		if (cache == LRU_MISS)
		{
		    pix = 0;
		    for (i = 0; i < bytes; i++)
		    {
			pix = pix << 8;
			NextByte (byte);
			pix |= byte;
		    }
		    push (lru, pix);
		}
		else
		{
		    pix = use (lru, cache);
		}
		w -= run;
		if (w < 0)
		{
		    error = "bad run";
		    break;
		}
		x += run;
		fprintf (stdout, " %3ld:%0*x", run, bytes * 2, pix);
	    }
	    break;
	}
	if (Verbose > 2)
	    fprintf (stdout, "\n");
	y++;
    }
    if (error)
	fprintf (stdout, "%s%20s: %d, %d %d\n", Leader, error, 
		 x, y - 1, (char *) data - buf);
}

static void
WcpAnalyzeImage (
    const char *buf,
    int	    len,
    int	    depth,
    int	    encoding,
    int	    width,
    int	    height)
{
    int	    bytes;
    
    bytes = depth / 8;
    switch (depth) {
    case 1:
	switch (encoding) {
	case 1:
	case 2:
	case 3:
	case 4:
	    WcpAnalyzeImage1RLL (buf, len, width, height);
	    return;
	default:
	    break;
	}
	break;
    case 4:
	switch (encoding) {
	case 1:
	case 2:
	case 3:
	case 4:
	default:
	    break;
	}
	break;
    case 8:
    case 16:
    case 24:
	switch (encoding) {
	case 1:
	case 2:
	case 3:
	    WcpAnalyzeImageNRLL (buf, len, width, height, bytes);
	    return;
	case 4:
	    WcpAnalyzeImageNLRU (buf, len, width, height, bytes);
	    return;
	default:
	    break;
	}
	break;
    }
    fprintf (stdout, "%s%20s: ", Leader, "invalid");
    fprintf (stdout, "encoding %d depth %d\n", encoding, depth);
}

void
WcpPutImage (
    FD fd,
    const unsigned char *buf)
{
  int n;
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  n = (IShort(&buf[2]) - 7) * 4;
  printreqlen(buf, fd, DVALUE2(7 + (n + p) / 4));
  PrintField(buf, 4, 4, DRAWABLE, "drawable");
  PrintField(buf, 8, 4, GCONTEXT, "gc");
  if (Verbose > 2)
    PrintValueRec (ILong(&buf[8]),
		   GC_function|
		   GC_plane_mask|
		   GC_foreground|
		   GC_background,
		   GC_BITMASK);
  PrintField(buf, 12, 4, CARD32, "lut");
  PrintField(buf, 16, 2, INT16, "dst-x");
  PrintField(buf, 18, 2, INT16, "dst-y");
  PrintField(buf, 20, 2, CARD16, "width");
  PrintField(buf, 22, 2, CARD16, "height");
  PrintField(buf, 24, 1, CARD8, "depth");
  PrintField(buf, 25, 1, CARD8, "encoding");
  PrintField(buf, 26, 1, CARD8, "left-pad");
  WcpAnalyzeImage ((const char *) &buf[28], (long) n, buf[24], buf[25],
		   IShort(&buf[20]) + buf[26],
		   IShort(&buf[22]));
  PrintBytes(&buf[28], (long)n, "data");
}

void
WcpGetImage (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, DRAWABLE, "drawable");
  PrintField(buf, 8, 4, CARD32, "lut");
  PrintField(buf, 12, 2, INT16, "x");
  PrintField(buf, 14, 2, INT16, "y");
  PrintField(buf, 16, 2, CARD16, "width");
  PrintField(buf, 18, 2, CARD16, "height");
  PrintField(buf, 20, 1, CARD8, "encoding");
}

void
WcpGetImageReply (
    FD fd,
    const unsigned char *buf)
{
  long	  n;

  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* WcpRequest reply */ ;
  PrintField(RBf, 1, 1, WCPREPLY, WCPREPLYHEADER) /* WcpGetImage reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(n), "reply length");
  PrintField(buf, 8, 4, CARD32, "visual");
  n = ILong (&buf[4]) * 4;
  PrintBytes (&buf[32], n, "data");
}

void
WcpCreateColorCursor (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
WcpCreateLut (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
WcpFreeLut (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
WcpCopyArea (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* WcpRequest */ ;
  PrintField (buf, 1, 1, WCPREQUEST, WCPREQUESTHEADER) /* WcpSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}
