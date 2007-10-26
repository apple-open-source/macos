/* $Xorg: dfaxg42d.c,v 1.3 2000/08/17 19:46:40 cpqbld Exp $ */
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
/* $XFree86: xc/lib/lbxutil/image/dfaxg42d.c,v 1.5 2001/01/17 19:43:35 dawes Exp $ */

#include <X11/Xos.h>
#include <X11/Xfuncproto.h>
#include <X11/Xfuncs.h>
#include <stdlib.h>
#include "g3states.h"
#include "lbxfax.h"
#include <X11/extensions/lbximage.h>

/*
 * -------------------------------------------------------------------------
 *              FAX G42D decoding for 1 bit images
 * -------------------------------------------------------------------------
 */

static short sp_data, sp_bit;


/*
 * Fetch a byte from the input stream
 */

static unsigned char
fetchByte (unsigned char **inbuf)

{
    unsigned char byte = **inbuf;
    (*inbuf)++;
    return (byte);
}


/*
 * Decode a run of white.
 */

static int
decode_white_run (unsigned char **inbuf)

{
    short state = sp_bit;
    short action;
    int runlen = 0;

    for (;;)
    {
	if (sp_bit == 0)
	{
	nextbyte:
	    sp_data = fetchByte (inbuf);
	}

	action = TIFFFax1DAction[state][sp_data];
	state = TIFFFax1DNextState[state][sp_data];
	if (action == ACT_INCOMP)
	    goto nextbyte;
	if (action == ACT_INVALID)
	    return (G3CODE_INVALID);
	if (action == ACT_EOL)
	    return (G3CODE_EOL);
	sp_bit = state;
	action = RUNLENGTH(action - ACT_WRUNT);
	runlen += action;
	if (action < 64)
	    return (runlen);
    }
}


/*
 * Decode a run of black.
 */

static int
decode_black_run (unsigned char **inbuf)

{
    short state = sp_bit + 8;
    short action;
    int runlen = 0;

    for (;;)
    {
	if (sp_bit == 0)
	{
	nextbyte:
	    sp_data = fetchByte (inbuf);
	}

	action = TIFFFax1DAction[state][sp_data];
	state = TIFFFax1DNextState[state][sp_data];
	if (action == ACT_INCOMP)
	    goto nextbyte;
	if (action == ACT_INVALID)
	    return (G3CODE_INVALID);
	if (action == ACT_EOL)
	    return (G3CODE_EOL);
	sp_bit = state;
	action = RUNLENGTH(action - ACT_BRUNT);
	runlen += action;
	if (action < 64)
	    return (runlen);
	state += 8;
    }
}


/*
 * Return the next uncompressed mode code word.
 */

static int
decode_uncomp_code (unsigned char **inbuf)

{
    short code;

    do {
	if (sp_bit == 0 || sp_bit > 7)
	    sp_data = fetchByte (inbuf);

	code = TIFFFaxUncompAction[sp_bit][sp_data];
	sp_bit = TIFFFaxUncompNextState[sp_bit][sp_data];
    } while (code == ACT_INCOMP);

    return (code);
}


/*
 * Fill a span with ones.
 */

static void
fillspan (char *cp,
	  int x, int count)

{
    static unsigned char masks[] =
        { 0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

    if (count <= 0)
	return;

    cp += x>>3;

    if (x &= 7)
    {
	/* align to byte boundary */

	if (count < 8 - x) {
	    *cp++ |= masks[count] >> x;
	    return;
	}

	*cp++ |= 0xff >> x;
	count -= 8 - x;
    }

    while (count >= 8)
    {
	*cp++ = (char)0xff;
	count -= 8;
    }

    *cp |= masks[count];
}


/*
 * Return the next bit in the input stream.  This is
 * used to extract 2D tag values and the color tag
 * at the end of a terminating uncompressed data code.
 */

static int
nextbit (unsigned char **inbuf)

{
    static unsigned char bitMask[8] =
        { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    int bit;

    if (sp_bit == 0)
	sp_data = fetchByte (inbuf);

    bit = sp_data & bitMask[sp_bit];

    if (++(sp_bit) > 7)
	sp_bit = 0;

    return (bit);
}


static int
DecodeFaxG42D (unsigned char **inbuf,
	       unsigned char *refline,
	       int pixels_per_line,
	       unsigned char *outbuf)

{
    int a0 = -1;
    int b1, b2;
    int run1, run2;		/* for horizontal mode */
    short mode;
    short color = 1;

    do {
	if (sp_bit == 0 || sp_bit > 7)
	    sp_data = fetchByte (inbuf);

	mode = TIFFFax2DMode[sp_bit][sp_data];
	sp_bit = TIFFFax2DNextState[sp_bit][sp_data];

	switch (mode)
	{
	case MODE_NULL:
	    break;

	case MODE_PASS:
	    b2 = LbxImageFindDiff (refline, a0, pixels_per_line, !color);
	    b1 = LbxImageFindDiff (refline, b2, pixels_per_line, color);
	    b2 = LbxImageFindDiff (refline, b1, pixels_per_line, !color);

	    if (color)
	    {
		if (a0 < 0)
		    a0 = 0;
		fillspan ((char *) outbuf, a0, b2 - a0);
	    }

	    a0 = b2;
	    break;

	case MODE_HORIZ:
	    if (color == 1)
	    {
		run1 = decode_white_run (inbuf);
		run2 = decode_black_run (inbuf);
	    }
	    else
	    {
		run1 = decode_black_run (inbuf);
		run2 = decode_white_run (inbuf);
	    }

	    /*
	     * Do the appropriate fill.  Note that we exit this logic with
	     * the same color that we enter with since we do 2 fills.  This
	     * explains the somewhat obscure logic below.
	     */

	    if (a0 < 0)
		a0 = 0;
	    if (a0 + run1 > pixels_per_line)
		run1 = pixels_per_line - a0;
	    if (color)
		fillspan ((char *) outbuf, a0, run1);
	    a0 += run1;
	    if (a0 + run2 > pixels_per_line)
		run2 = pixels_per_line - a0;
	    if (!color)
		fillspan ((char *) outbuf, a0, run2);
	    a0 += run2;
	    break;

	case MODE_VERT_V0:
	case MODE_VERT_VR1:
	case MODE_VERT_VR2:
	case MODE_VERT_VR3:
	case MODE_VERT_VL1:
	case MODE_VERT_VL2:
	case MODE_VERT_VL3:
	    b2 = LbxImageFindDiff (refline, a0, pixels_per_line, !color);
	    b1 = LbxImageFindDiff (refline, b2, pixels_per_line, color);
	    b1 += mode - MODE_VERT_V0;

	    if (color)
	    {
		if (a0 < 0)
		    a0 = 0;
		fillspan ((char *) outbuf, a0, b1 - a0);
	    }

	    color = !color;
	    a0 = b1;
	    break;

	case MODE_UNCOMP:
	    /*
	     * Uncompressed mode: select from the special set of code words.
	     */

	    if (a0 < 0)
		a0 = 0;
	    do
	    {
		mode = decode_uncomp_code (inbuf);
		switch (mode)
		{
		case UNCOMP_RUN1:
		case UNCOMP_RUN2:
		case UNCOMP_RUN3:
		case UNCOMP_RUN4:
		case UNCOMP_RUN5:
		    run1 = mode - UNCOMP_RUN0;
		    fillspan ((char *) outbuf, a0+run1-1, 1);
		    a0 += run1;
		    break;

		case UNCOMP_RUN6:
		    a0 += 5;
		    break;

		case UNCOMP_TRUN0:
		case UNCOMP_TRUN1:
		case UNCOMP_TRUN2:
		case UNCOMP_TRUN3:
		case UNCOMP_TRUN4:

		    run1 = mode - UNCOMP_TRUN0;
		    a0 += run1;
		    color = nextbit (inbuf) ? 0 : 1;
		    break;

		case UNCOMP_INVALID:
		    goto bad;

		case UNCOMP_EOF:
		    return (0);
		}
	    } while (mode < UNCOMP_EXIT);
	    break;

	case MODE_ERROR_1:
	    /* fall thru... */
	case MODE_ERROR:
	    goto bad;

	default:
	    return (0);
	}

    } while (a0 < pixels_per_line);

bad:
    return (a0 >= pixels_per_line);
}


int
LbxImageDecodeFaxG42D (unsigned char *inbuf,
		       unsigned char *outbuf,
		       int image_bytes,
		       int pixels_per_line,
		       int padded_bytes_per_scanline,
		       int reverse_bits)

{
    int bytes_per_scanline = ROUNDUP8 (pixels_per_line);
    unsigned char *refline, *refptr;
    unsigned char *outbuf_start = outbuf;
    int bytes_left = image_bytes;
    int i;

    refline = (unsigned char *) malloc (bytes_per_scanline + 1);
    refptr = refline + 1;

    for (i = 0; i < bytes_per_scanline + 1; i++)
	refline[i] = 0xff;

    bzero (outbuf, image_bytes);

    sp_bit = 0;
    sp_data = 0;

    while (bytes_left > 0)
    {
	if (!DecodeFaxG42D (&inbuf, refptr, pixels_per_line, outbuf))
	    return (0);

	memcpy (refptr, outbuf, bytes_per_scanline);

	outbuf += padded_bytes_per_scanline;
	bytes_left -= padded_bytes_per_scanline;
    }

    free ((char *) refline);

    if (reverse_bits)
	LbxReverseBits (outbuf_start, image_bytes);

    return (outbuf - outbuf_start);
}
