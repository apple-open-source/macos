/* $Xorg: dpackbits.c,v 1.4 2000/08/17 19:46:40 cpqbld Exp $ */
/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992 Sam Leffler
 * Copyright (c) 1991, 1992 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */
/* $XFree86: xc/lib/lbxutil/image/dpackbits.c,v 1.4 2001/01/17 19:43:35 dawes Exp $ */

#include <X11/Xos.h>
#include <X11/Xfuncproto.h>
#include "lbximage.h"

/*
 * -------------------------------------------------------------------------
 *              PackBits decoding for 8 bit color images
 * -------------------------------------------------------------------------
 */

static void
DecodePackBits (char **inbuf,
		int numPixels,
		char *outbuf)

{
    register char *inptr = *inbuf;
    register int n, b;

    while (numPixels > 0)
    {
	n = (int) *inptr++;

	/* Watch out for compilers that don't sign extend chars... */

	if (n >= 128)
	    n -= 256;

	if (n < 0)
	{
	    /* replicate next byte -n+1 times */

	    if (n == -128)	/* nop */
		continue;

	    n = -n + 1;
	    numPixels -= n;
	    for (b = *inptr++; n-- > 0;)
		*outbuf++ = b;
	}
	else
	{
	    /* copy next n+1 bytes literally */

	    memcpy (outbuf, inptr, ++n);
	    outbuf += n; numPixels -= n;
	    inptr += n;
	}
    }

    *inbuf = inptr;
}


int
LbxImageDecodePackBits (char *inbuf,
			char *outbuf,
			int num_scan_lines,
			int scan_line_size)

{
    char *outbuf_start = outbuf;
    int padded_scan_line_size = (scan_line_size % 4) ?
	scan_line_size + (4 - scan_line_size % 4) : scan_line_size;

    while (num_scan_lines > 0)
    {
	DecodePackBits (&inbuf, scan_line_size, outbuf);
	outbuf += padded_scan_line_size;
	num_scan_lines--;
    }

    return (outbuf - outbuf_start);
}
