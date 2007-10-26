/* $Xorg: epackbits.c,v 1.4 2000/08/17 19:46:41 cpqbld Exp $ */
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

#include <X11/Xfuncproto.h>
#include <X11/extensions/lbximage.h>

/*
 * -------------------------------------------------------------------------
 *              PackBits encoding for 8 bit color images
 * -------------------------------------------------------------------------
 */

#define PutByte(byte,bufptr,bytesLeft) \
{ \
    if (*bytesLeft < 1) {\
         *outbuf = outptr; \
         return (0); \
    } \
    *bufptr++ = byte; \
    (*bytesLeft)--; \
}
    

static int
EncodePackBits (char *inbuf,
		int numPixels,
		char **outbuf,
		int *bytesLeft)

{
    register int pixelsLeft = numPixels;
    register char *outptr = *outbuf;
    register char *lastliteral;
    register int n, b;
    enum { BASE, LITERAL, RUN, LITERAL_RUN } state;

    state = BASE;
    lastliteral = 0;

    while (pixelsLeft > 0)
    {
	/*
	 * Find the longest string of identical bytes.
	 */

	b = *inbuf++, pixelsLeft--, n = 1;
	for (; pixelsLeft > 0 && b == *inbuf; pixelsLeft--, inbuf++)
	    n++;

    again:

	switch (state)
	{
	case BASE:		/* initial state, set run/literal */
	    if (n > 1)
	    {
		state = RUN;

		if (n > 128)
		{
		    PutByte (-127, outptr, bytesLeft);
		    PutByte (b, outptr, bytesLeft);
		    n -= 128;
		    goto again;
		}

		PutByte (-(n-1), outptr, bytesLeft);
		PutByte (b, outptr, bytesLeft);
	    }
	    else
	    {
		lastliteral = outptr;
		PutByte (0, outptr, bytesLeft);
		PutByte (b, outptr, bytesLeft);
		state = LITERAL;
	    }

	    break;

	case LITERAL:		/* last object was literal string */
	    if (n > 1)
	    {
		state = LITERAL_RUN;

		if (n > 128)
		{
		    PutByte (-127, outptr, bytesLeft);
		    PutByte (b, outptr, bytesLeft);
		    n -= 128;
		    goto again;
		}

		    PutByte (-(n-1), outptr, bytesLeft); /* encode run */
		    PutByte (b, outptr, bytesLeft);
	    }
	    else
	    {			/* extend literal */
		if (++(*lastliteral) == 127)
		    state = BASE;
		PutByte (b, outptr, bytesLeft);
	    }

	    break;

	case RUN:		/* last object was run */
	    if (n > 1)
	    {
		if (n > 128)
		{
		    PutByte (-127, outptr, bytesLeft);
		    PutByte (b, outptr, bytesLeft);
		    n -= 128;
		    goto again;
		}
		PutByte (-(n-1), outptr, bytesLeft);
		PutByte (b, outptr, bytesLeft);
	    }
	    else
	    {
		lastliteral = outptr;
		PutByte (0, outptr, bytesLeft);
		PutByte (b, outptr, bytesLeft);
		state = LITERAL;
	    }

	    break;

	case LITERAL_RUN:	/* literal followed by a run */
	    /*
	     * Check to see if previous run should
	     * be converted to a literal, in which
	     * case we convert literal-run-literal
	     * to a single literal.
	     */

	    if (n == 1 && outptr[-2] == (char)-1 && *lastliteral < 126)
	    {
		state = (((*lastliteral) += 2) == 127 ? BASE : LITERAL);
		outptr[-2] = outptr[-1];	/* replicate */
	    }
	    else
		state = RUN;
	    goto again;
	}
    }

    *outbuf = outptr;

    return (1);
}


int
LbxImageEncodePackBits (char *inbuf,
			char *outbuf,
			int outbufSize,
			int format,
			int depth,
			int num_scan_lines,
			int scan_line_size,
			int *bytesCompressed)

{
    char *outbuf_start = outbuf;
    int padded_scan_line_size = (scan_line_size % 4) ?
	scan_line_size + (4 - scan_line_size % 4) : scan_line_size;
    int bytesLeft = outbufSize;

    while (num_scan_lines > 0)
    {
	if (!EncodePackBits (inbuf, scan_line_size, &outbuf, &bytesLeft))
	    return (LBX_IMAGE_COMPRESS_NOT_WORTH_IT);

	inbuf += padded_scan_line_size;
	num_scan_lines--;
    }

    *bytesCompressed = outbuf - outbuf_start;
    return (LBX_IMAGE_COMPRESS_SUCCESS);
}
