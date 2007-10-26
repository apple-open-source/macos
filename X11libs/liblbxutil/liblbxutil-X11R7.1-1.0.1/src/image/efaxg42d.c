/* $Xorg: efaxg42d.c,v 1.3 2000/08/17 19:46:41 cpqbld Exp $ */
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
/* $XFree86: xc/lib/lbxutil/image/efaxg42d.c,v 1.4 2001/01/17 19:43:35 dawes Exp $ */

#include <X11/Xos.h>
#include <X11/Xfuncproto.h>
#include <stdlib.h>
#include "lbxfax.h"
#include <X11/extensions/lbximage.h>
#include "lbxbwcodes.h"

/*
 * -------------------------------------------------------------------------
 *              FAX G42D encoding for 1 bit images
 * -------------------------------------------------------------------------
 */

static short sp_data, sp_bit;

static tableentry horizcode =
    { 3, 0x1 };		/* 001 */

static tableentry passcode =
    { 4, 0x1 };		/* 0001 */

static tableentry vcodes[7] = {
    { 7, 0x03 },	/* 0000 011 */
    { 6, 0x03 },	/* 0000 11 */
    { 3, 0x03 },	/* 011 */
    { 1, 0x1 },		/* 1 */
    { 3, 0x2 },		/* 010 */
    { 6, 0x02 },	/* 0000 10 */
    { 7, 0x02 }		/* 0000 010 */
};

typedef struct {
    unsigned char *bufStart;
    unsigned char *bufPtr;
    int bufSize;
    int bytesLeft;
} Buffer;



/*
 * Flush bits to output buffer.
 */

static int
flushbits (Buffer *outbuf)

{
    if (outbuf->bytesLeft > 0)
    {
	*(outbuf->bufPtr++) = sp_data;
	outbuf->bytesLeft--;

	sp_data = 0;
	sp_bit = 8;

	return (1);
    }
    else
	return (0);
}


/*
 * Write a variable-length bit-value to the output stream.  Values are
 * assumed to be at most 16 bits.
 */

static int
putbits (unsigned int bits,
	 unsigned int length,
	 Buffer *outbuf)

{
    static int mask[9] =
        {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

    while (length > sp_bit)
    {
	sp_data |= bits >> (length - sp_bit);
	length -= sp_bit;
	if (!flushbits (outbuf))
	    return (0);
    }

    sp_data |= (bits & mask[length]) << (sp_bit - length);
    sp_bit -= length;

    if (sp_bit == 0)
    {
	if (!flushbits (outbuf))
	    return (0);
    }

    return (1);
}


/*
 * Write a code to the output stream.
 */

static int
putcode (tableentry *te,
	 Buffer *outbuf)

{
    return (putbits (te->code, te->length, outbuf));
}


/*
 * Write the sequence of codes that describes the specified span of
 * zero's or one's.  The appropriate table that holds the make-up and
 * terminating codes is supplied.
 */

static int
putspan (int span,
	 tableentry *tab,
	 Buffer *outbuf)
	 
{
    while (span >= 2624)
    {
	tableentry *te = &tab[63 + (2560 >> 6)];
	if (!putcode (te, outbuf))
	    return (0);
	span -= te->runlen;
    }

    if (span >= 64)
    {
	tableentry *te = &tab[63 + (span >> 6)];
	if (!putcode (te, outbuf))
	    return (0);
	span -= te->runlen;
    }

    if (!putcode (&tab[span], outbuf))
	return (0);

    return (1);
}



#define	PIXEL(buf,ix)	((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)

static int
EncodeFaxG42D (unsigned char *inbuf,
	       unsigned char *refline,
	       int bits,
	       Buffer *outbuf)

{
    short white = 1;
    int a0 = 0;
    int a1 = (PIXEL (inbuf, 0) != white ?
	0 : LbxImageFindDiff (inbuf, 0, bits, white));
    int b1 = (PIXEL (refline, 0) != white ?
	0 : LbxImageFindDiff (refline, 0, bits, white));
    int a2, b2;

    for (;;)
    {
	b2 = LbxImageFindDiff (refline, b1, bits, PIXEL (refline, b1));
	if (b2 >= a1)
	{
	    int d = b1 - a1;
	    if (!(-3 <= d && d <= 3))
	    {
		/* horizontal mode */

		a2 = LbxImageFindDiff (inbuf, a1, bits, PIXEL (inbuf, a1));
		if (!putcode (&horizcode, outbuf))
		    return (0);

		if (a0 + a1 == 0 || PIXEL (inbuf, a0) == white)
		{
		    if (!putspan(a1 - a0, TIFFFaxWhiteCodes, outbuf))
			return (0);
		    if (!putspan(a2 - a1, TIFFFaxBlackCodes, outbuf))
			return (0);
		}
		else
		{
		    if (!putspan (a1 - a0, TIFFFaxBlackCodes, outbuf))
			return (0);
		    if (!putspan (a2 - a1, TIFFFaxWhiteCodes, outbuf))
			return (0);
		}

		a0 = a2;
	    }
	    else
	    {
		/* vertical mode */

		if (!putcode (&vcodes[d+3], outbuf))
		    return (0);
		a0 = a1;
	    }
	}
	else
	{
	    /* pass mode */

	    if (!putcode (&passcode, outbuf))
		return (0);
	    a0 = b2;
	}

	if (a0 >= bits)
	    break;

	a1 = LbxImageFindDiff (inbuf, a0, bits, PIXEL (inbuf, a0));
	b1 = LbxImageFindDiff (refline, a0, bits, !PIXEL (inbuf, a0));
	b1 = LbxImageFindDiff (refline, b1, bits, PIXEL (inbuf, a0));
    }

    return (1);
}


int
LbxImageEncodeFaxG42D (unsigned char *inbuf,
		       unsigned char *outbuf,
		       int outbufSize,
		       int image_bytes,
		       int pixels_per_line,
		       int padded_bytes_per_scanline,
		       int reverse_bits,
		       int *bytesCompressed)

{
    int bytes_per_scanline = ROUNDUP8 (pixels_per_line);
    unsigned char *refline, *refptr;
    unsigned char *save_inbuf = inbuf;
    int bytes_left = image_bytes;
    Buffer OutBuf;
    int status, i;

    OutBuf.bufStart = OutBuf.bufPtr = outbuf;
    OutBuf.bufSize = OutBuf.bytesLeft = outbufSize;

    if (!(refline = (unsigned char *) malloc (bytes_per_scanline + 1)))
	return (LBX_IMAGE_COMPRESS_BAD_MALLOC);

    refptr = refline + 1;

    for (i = 0; i < bytes_per_scanline + 1; i++)
	refline[i] = 0xff;

    if (reverse_bits)
	LbxReverseBits (inbuf, image_bytes);

    sp_bit = 8;
    sp_data = 0;

    while (bytes_left > 0)
    {
	if (!(status = EncodeFaxG42D (inbuf, refptr,
	    pixels_per_line, &OutBuf)))
	{
	    goto bad;
	}

	memcpy (refptr, inbuf, bytes_per_scanline);

	inbuf += padded_bytes_per_scanline;
	bytes_left -= padded_bytes_per_scanline;
    }

    status = putbits (EOL, 12, &OutBuf);
    if (status)
	status = putbits (EOL, 12, &OutBuf);
    if (status && sp_bit != 8)
    {
	status = flushbits (&OutBuf);
    }

 bad:

    free ((char *) refline);

    /* put the bits back the way they were */
    if (reverse_bits)
	LbxReverseBits (save_inbuf, image_bytes);

    if (status)
    {
	*bytesCompressed = OutBuf.bufPtr - OutBuf.bufStart;

	if (OutBuf.bytesLeft > 0)
	    return (LBX_IMAGE_COMPRESS_SUCCESS);
	else
	    return (LBX_IMAGE_COMPRESS_NOT_WORTH_IT);
    }
    else
	return (LBX_IMAGE_COMPRESS_NOT_WORTH_IT);
}
