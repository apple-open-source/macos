/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef lint
static char rcsid[] = "$Header: /cvs/Darwin/boot/i386/util/tif_packbits.c,v 1.1.1.2 1999/08/04 21:17:19 wsanchez Exp $";
#endif

/*
 * Copyright (c) 1988, 1989, 1990, 1991 Sam Leffler
 * Copyright (c) 1991 Silicon Graphics, Inc.
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

/*
 * TIFF Library.
 *
 * PackBits Compression Algorithm Support
 */
#import "bitmap.h"

typedef unsigned int u_int;
typedef unsigned char u_char;

static
TIFFAppendToStrip(tif, strip, data, cc)
	TIFF *tif;
	u_int strip;
	u_char *data;
	u_int cc;
{
}



/*
 * Encode a scanline of pixels.
 */
int
PackBitsEncode(tif, bp, cc, s)
	TIFF *tif;
	u_char *bp;
	register int cc;
	u_int s;
{
	register char *op, *lastliteral;
	register int n, b;
	enum { BASE, LITERAL, RUN, LITERAL_RUN } state;
	char *ep;
	int slop;

	op = tif->tif_rawcp;
	ep = tif->tif_rawdata + tif->tif_rawdatasize;
	state = BASE;
	lastliteral = 0;
	while (cc > 0) {
		/*
		 * Find the longest string of identical bytes.
		 */
		b = *bp++, cc--, n = 1;
		for (; cc > 0 && b == *bp; cc--, bp++)
			n++;
	again:
		if (op + 2 >= ep) {		/* insure space for new data */
			/*
			 * Be careful about writing the last
			 * literal.  Must write up to that point
			 * and then copy the remainder to the
			 * front of the buffer.
			 */
			if (state == LITERAL || state == LITERAL_RUN) {
				slop = op - lastliteral;
				tif->tif_rawcc += lastliteral - tif->tif_rawcp;
				// no space left
				return (-1);
				op = tif->tif_rawcp;
				for (; slop-- > 0; *op++ = *lastliteral++)
					;
				lastliteral = tif->tif_rawcp;
			} else {
				tif->tif_rawcc += op - tif->tif_rawcp;
				// no space left
				return (-1);
				op = tif->tif_rawcp;
			}
		}
		switch (state) {
		case BASE:		/* initial state, set run/literal */
			if (n > 1) {
				state = RUN;
				if (n > 128) {
					*op++ = -127;
					*op++ = b;
					n -= 128;
					goto again;
				}
				*op++ = -(n-1);
				*op++ = b;
			} else {
				lastliteral = op;
				*op++ = 0;
				*op++ = b;
				state = LITERAL;
			}
			break;
		case LITERAL:		/* last object was literal string */
			if (n > 1) {
				state = LITERAL_RUN;
				if (n > 128) {
					*op++ = -127;
					*op++ = b;
					n -= 128;
					goto again;
				}
				*op++ = -(n-1);		/* encode run */
				*op++ = b;
			} else {			/* extend literal */
				if (++(*lastliteral) == 127)
					state = BASE;
				*op++ = b;
			}
			break;
		case RUN:		/* last object was run */
			if (n > 1) {
				if (n > 128) {
					*op++ = -127;
					*op++ = b;
					n -= 128;
					goto again;
				}
				*op++ = -(n-1);
				*op++ = b;
			} else {
				lastliteral = op;
				*op++ = 0;
				*op++ = b;
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
			if (n == 1 && op[-2] == (char)-1 &&
			    *lastliteral < 126) {
				state = (((*lastliteral) += 2) == 127 ?
				    BASE : LITERAL);
				op[-2] = op[-1];	/* replicate */
			} else
				state = RUN;
			goto again;
		}
	}
	tif->tif_rawcc += op - tif->tif_rawcp;
	tif->tif_rawcp = op;
	return (1);
}

