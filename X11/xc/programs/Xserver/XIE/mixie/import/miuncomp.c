/* $Xorg: miuncomp.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module miuncomp.c ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


				NOTICE

This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.

     "Copyright 1993, 1994 by AGE Logic, Inc."

     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.

     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************

	miuncomp.c -- DDXIE routines for importing uncompressed data

	Dean Verheiden -- AGE Logic, Inc. September 1993

*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/miuncomp.c,v 1.6 2001/12/14 19:58:29 dawes Exp $ */

#define _XIEC_MIUNCOMP

/*
 *  Include files
 */
/*
 *  Core X Includes
 */
#include <X.h>
#include <Xproto.h>
/*
 *  XIE Includes
 */
#include <XIE.h>
#include <XIEproto.h>
/*
 *  more X server includes.
 */
#include <misc.h>
#include <dixstruct.h>
/*
 *  Server XIE Includes
 */
#include <error.h>
#include <macro.h>
#include <element.h>
#include <texstr.h>
#include <xiemd.h>
#include <miuncomp.h>

/***********************************************************************/
/****************** Aligned data conversion routines *******************/
/***********************************************************************/

/* Single band action routines */

void CPreverse_bits(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 i;
	count = (count + 7) >> 3;/* Pack down to bits */

	src += leftpad>>3;
	for (i = 0; i < count; i++) *dst++ = _ByteReverseTable[*src++];
}

void CPpass_bits(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
	src += leftpad >> 3;
	memcpy((char *)dst,(char *)src, (int)(count + 7) >> 3);
}

void CPextractstreambits(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 i,j;

	bzero(dst, (count + 7) >> 3);

	for (i = 0, j = leftpad; i < count; i++, j += stride) {
	    if (LOGBYTE_tstbit(src,j) != 0) {
		LOGBYTE_setbit(dst,i);
	    }
	}
}

void CPextractswappedstreambits(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 i,j,s;

	bzero(dst, (count + 7) >> 3);

	for (i = 0, j = leftpad; i < count; i++, j += stride) {
	    s = j ^ 7; /* Larry swap */
	    if (LOGBYTE_tstbit(src,s) != 0) {
		LOGBYTE_setbit(dst,i);
	    }
	}
}


void CPpass_bytes(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
	src += leftpad >> 3;
	if (stride == 8)
		memcpy((char *)dst, (char *)src, (int)count);
	else {
	    CARD32 i;
	    CARD32 Bstride = stride >> 3;
	    for (i = 0; i < count; i++, src += Bstride)
		    *dst++ = *src;
	}
}

void CPswap_pairs(
     PairPixel *src,
     PairPixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 i;
	src += leftpad >> 4;
	if (stride == 16) {
	    for (i = 0; i < count; i++) {
		cpswaps(src[i],dst[i]);
	    }
	} else {
	    int j;
	    CARD32 Bstride = stride >> 4;
	    for (i = 0, j = 0; i < count; i++, j += Bstride) {
		cpswaps(src[j],dst[i]);
	    }
	}
}

void CPpass_pairs(
     PairPixel *src,
     PairPixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
	src += leftpad >> 4;
	if (stride == 16)
	    memcpy((char *)dst, (char *)src, (int)count << 1);
	else {
	    CARD32 i,j;
	    CARD32 Bstride = stride >> 4;
	    for (i = 0, j = 0; i < count; i++, j += Bstride)
		dst[i] = src[j];
	}
}

void CPswap_quads(
     QuadPixel *src,
     QuadPixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 i;
	src += leftpad >> 5;
	if (stride == 32) {
	    for (i = 0; i < count; i++) {
		cpswapl(src[i],dst[i]);
	    }
	} else {
	    int j;
	    CARD32 Bstride = stride >> 5;
	    for (i = 0, j = 0; i < count; i++, j += Bstride) {
		cpswapl(src[j],dst[i]);
	    }
	}
}

void CPpass_quads(
     QuadPixel *src,
     QuadPixel *dst,
     CARD32 count,
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
	src += leftpad >> 5;
	if (stride == 32)
	    memcpy((char *)dst, (char *)src, (int)count << 2);
	else {
	    CARD32 i,j;
	    CARD32 Bstride = stride >> 5;
	    for (i = 0, j = 0; i < count; i++, j += Bstride)
		dst[i] = src[j];
	}
}

#if XIE_FULL
/* Triple Band Byte by Pixel Action routines */

/* Stream bits to bits */
void Sbtob(
     BytePixel *src,
     BytePixel *idst,
     CARD32 count,
     miUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD8     mask = pvt->mask;
LogInt	  *dst = (LogInt *)idst;
int i;

	bzero(dst, (count + 7) >>3);	/* zero out the output, only set ones */
	src += pvt->srcoffset;
	for (i = 0; i < count; i++, src += Bstride)
	    if ((*src & mask) != 0)
		LOG_setbit(dst,i);
}

/* Stream in nibbles to BytePixels */
void SbtoB(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     miUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD8     mask = pvt->mask;
CARD8    shift = pvt->shift;
CARD32 i;

	src += pvt->srcoffset;
	for (i = 0; i < count; i++, src += Bstride)
		*dst++ = (*src & mask) >> shift;
}

/* Stream to BytePixel */
void StoB(
     BytePixel *src,
     BytePixel *dst,
     CARD32 count,
     miUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32 i;

	src += pvt->srcoffset;
	for (i = 0; i < count; i++, src += Bstride)
		*dst++ = *src;
}

/* Stream to PairPixel (unswapped) */
void StoP(
     PairPixel *isrc,
     PairPixel *dst,
     CARD32 count,
     miUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD8 *src = ((CARD8 *)isrc) + pvt->srcoffset;
CARD32 i;
	for (i = 0; i < count; i++, src += Bstride)
		*dst++ = *(PairPixel *)src;
}

/* Stream to PairPixel (swapped) */
void StosP(
     PairPixel *isrc,
     PairPixel *dst,
     CARD32 count,
     miUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD8 *src = ((CARD8 *)isrc) + pvt->srcoffset;
CARD32 i;
	for (i = 0; i < count; i++, src += Bstride) {
		PairPixel sval = *(PairPixel *)src;
		*dst++ = (sval >> 8) | (sval << 8);
	}
}

#endif /* XIE_FULL */


/***********************************************************************/
/***************** Unaligned data conversion routines ******************/
/***********************************************************************/

/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = LSB, Fill Order = LSB		      */
void LLUBtoB(
     CARD8 *src,	/* source byte stream 			      */
     BytePixel *dst,	/* destination line			      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD8 bits, wval = 16 - depth, wval2 = 8 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 8)
	    *dst++ = *src >> bits | (CARD8)(*(src+1) << (wval - bits)) >> wval2;
	else {
	    *dst++ = (CARD8)(*src << (wval2 - bits)) >> wval2;
	}

	if ((longbits += stride) > 7) {
		src += longbits >> 3;
		longbits &= 7;
	}
    }
}

/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = LSB, Fill Order = MSB		      */
void LMUBtoB(
     CARD8 *src,	/* source byte stream 			      */
     BytePixel *dst,	/* destination byte stream		      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD8 bits, wval = 16 - depth, wval2 = 8 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 8)
	    *dst++ = (CARD8)(*src << bits) >> bits |
		     (CARD8)(*(src + 1) >> (wval - bits)) << (8 - bits);
	else
	    *dst++ = (CARD8)(*src << bits) >> wval2;

	if ((longbits += stride) > 7) {
		src += longbits >> 3;
		longbits &= 7;
	}
    }
}

/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = MSB, Fill Order = LSB		      */
void MLUBtoB(
     CARD8 *src,	/* source byte stream 			      */
     BytePixel *dst,	/* destination byte stream		      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD8 bits, wval = 16 - depth, wval2 = 8 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 8)
	    *dst++ = (CARD8)(*src >> bits) << (depth + bits - 8) |
		     (CARD8)(*(src + 1) << (wval - bits)) >> (wval - bits);
	else
	    *dst++ = (CARD8)(*src << (wval2 - bits)) >> wval2;

	if ((longbits += stride) > 7) {
		src += longbits >> 3;
		longbits &= 7;
	}
    }
}

/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = MSB, Fill Order = MSB		      */
void MMUBtoB(
     CARD8 *src,	/* source byte stream 			      */
     BytePixel *dst,	/* destination byte stream		      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD8 bits, wval = 16 - depth, wval2 = 8 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
        if (bits + depth > 8)
                *dst++ = (CARD8)(*src << bits) >> wval2 |
	                 (CARD8)*(src + 1) >> (wval - bits);
	    else
	        *dst++ = (CARD8)(*src << bits) >> wval2;

	if ((longbits += stride) > 7) {
		src += longbits >> 3;
		longbits &= 7;
	}
    }
}

/* Convert a single plane of unaligned pair pixels to pair pixels */
/* Pixel Order = LSB, Fill Order = LSB		      */
void LLUPtoP(
     CARD8 *src,	/* source byte stream 			      */
     PairPixel *dst,	/* destination line			      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD16 bits, wval1 = 32 - depth, wval2 = 24 - depth, wval3 = 16 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 16)
	    *dst++ = (CARD16)*src >> bits		|
		     (CARD16)*(src + 1) << (8 - bits) 	|
		     (CARD16)((CARD16)*(src + 2) << (wval1 - bits)) >> wval3;
	else
	    *dst++ = (CARD16)*src >> bits 	    |
		     (CARD16)((CARD16)*(src + 1) << (wval2 - bits)) >> wval3;

	longbits += stride;
	src += longbits >> 3;
	longbits &= 7;
    }
}

/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = LSB, Fill Order = MSB		      */
void LMUPtoP(
     CARD8 *src,	/* source byte stream 			      */
     PairPixel *dst,	/* destination byte stream		      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD16 bits, wval2 = 24 - depth, wval3 = 16 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 16)
	    *dst++ = (CARD16)((CARD16)*src << (8 + bits)) >> (8 + bits) |
		     (CARD16)*(src + 1) << (8 - bits) |
		     (CARD16)((CARD16)*(src + 2) >> (wval2 - bits)) << (16 - bits);
	else
	    *dst++ = (CARD16)((CARD16)*src << (8 + bits)) >> (8 + bits) |
		     (CARD16)((CARD16)*(src + 1) >> (wval3 - bits)) << (8 - bits);

	longbits += stride;
	src += longbits >> 3;
	longbits &= 7;
    }
}


/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = MSB, Fill Order = LSB		      */
void MLUPtoP(
     CARD8 *src,	/* source byte stream 			      */
     PairPixel *dst,	/* destination byte stream		      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD16 bits, wval1 = 32 - depth, wval2 = 24 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 16)
	    *dst++ = (CARD16)((CARD16)*src >> bits) << (depth + bits - 8)  |
		     (CARD16)*(src + 1) << (depth + bits - 16) |
		     (CARD16)((CARD16)*(src + 2) << (wval1 - bits)) >>
								(wval1 - bits);
	else
	    *dst++ = (CARD16)((CARD16)*src >> bits) << (depth + bits - 8) |
		     (CARD16)((CARD16)*(src + 1) << (wval2 - bits))
							>> (wval2 - bits);

	longbits += stride;
	src += longbits >> 3;
	longbits &= 7;
    }
}

/* Convert a single plane of unaligned bytes to bytes */
/* Pixel Order = MSB, Fill Order = MSB		      */
void MMUPtoP(
     CARD8 *src,	/* source byte stream 			      */
     PairPixel *dst,	/* destination byte stream		      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, longbits;
CARD16 bits, wval2 = 24 - depth, wval3 = 16 - depth;

    if (leftpad > 7) {
    	src += leftpad >> 3;
	longbits = leftpad & 7;
    } else
	longbits = leftpad;

    for (p = 0; p < numcmp; p++) {
	bits = longbits;
	if (bits + depth > 16)
	    *dst++ = (CARD16)((CARD16)*src << (8 + bits)) >> (wval2 - bits) |
		     (CARD16)*(src + 1) << (depth + bits - 16) |
		     (CARD16)*(src + 2) >> (wval2 - bits);
	else
	    *dst++ = (CARD16)((CARD16)*src << (8 + bits)) >> wval3 |
		     (CARD16)*(src + 1) >> (wval3 - bits);

	longbits += stride;
	src += longbits >> 3;
	longbits &= 7;
    }
}


/* Convert a single plane of unaligned quad pixels to quad pixels */
/* Pixel Order = LSB, Fill Order = LSB		      */
void LLUQtoQ(
     CARD8 *src,	/* source byte stream 			      */
     QuadPixel *dst,	/* destination line			      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, bits, wval1 = 56 - depth, wval2 = 48 - depth, wval3 = 32 - depth;

    /* Note: Maximum depth is 24 bits */

    if (leftpad > 7) {
    	src += leftpad >> 3;
	bits = leftpad & 7;
    } else
	bits = leftpad;

    for (p = 0; p < numcmp; p++) {
	if (bits + depth > 24)
	    *dst++ =  (CARD32)*src >> bits		|
		      (CARD32)*(src + 1) << (8 - bits) 	|
		      (CARD32)*(src + 2) << (16 - bits)	|
		     ((CARD32)*(src + 3) << (wval1 - bits)) >> wval3;
	else
	    *dst++ =  (CARD32)*src >> bits 	    	|
		      (CARD32)*(src + 1) << (8 - bits) 	|
		     ((CARD32)*(src + 2) << (wval2 - bits)) >> wval3;

	bits += stride;
	src += bits >> 3;
	bits &= 7;
    }
}

/* Convert a single plane of unaligned quad pixels to quad pixels */
/* Pixel Order = LSB, Fill Order = MSB		      */
void LMUQtoQ(
     CARD8 *src,	/* source byte stream 			      */
     QuadPixel *dst,	/* destination line			      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, bits, wval1 = 32 - depth, wval2 = 24 - depth;

    /* Note: Maximum depth is 24 bits */

    if (leftpad > 7) {
    	src += leftpad >> 3;
	bits = leftpad & 7;
    } else
	bits = leftpad;

    for (p = 0; p < numcmp; p++) {
	if (bits + depth > 24)
	    *dst++ = ((CARD32)*src << (24 + bits)) >> (24 + bits)	|
		      (CARD32)*(src + 1) << (8 - bits) 			|
		      (CARD32)*(src + 2) << (16 - bits) 		|
		     ((CARD32)*(src + 3) >> (wval1 - bits)) << (24 - bits);
	else
	    *dst++ = ((CARD32)*src << (24 + bits)) >> (24 + bits) 	|
		      (CARD32)*(src + 1) << (8 - bits) 	      		|
		     ((CARD32)*(src + 2) >> (wval2 - bits)) << (16 - bits);

	bits += stride;
	src += bits >> 3;
	bits &= 7;
    }
}

/* Convert a single plane of unaligned quad pixels to quad pixels */
/* Pixel Order = MSB, Fill Order = LSB		      */
void MLUQtoQ(
     CARD8 *src,	/* source byte stream 			      */
     QuadPixel *dst,	/* destination line			      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, bits, wval1 = 56 - depth, wval2 = 48 - depth;

    /* Note: Maximum depth is 24 bits */

    if (leftpad > 7) {
    	src += leftpad >> 3;
	bits = leftpad & 7;
    } else
	bits = leftpad;

    for (p = 0; p < numcmp; p++) {
	if (bits + depth > 24)
	    *dst++ = ((CARD32)*src >> bits) << (depth + bits - 8)  |
		      (CARD32)*(src + 1)    << (depth + bits - 16) |
		      (CARD32)*(src + 2)    << (depth + bits - 24) |
		     ((CARD32)*(src + 3)    << (wval1 - bits)) >> (wval1 - bits);
	else
	    *dst++ = ((CARD32)*src >> bits) << (depth + bits - 8)  |
		      (CARD32)*(src + 1)    << (depth + bits - 16) |
		     ((CARD32)*(src + 2)    << (wval2 - bits)) >> (wval2 - bits);

	bits += stride;
	src += bits >> 3;
	bits &= 7;
    }
}

/* Convert a single plane of unaligned quad pixels to quad pixels */
/* Pixel Order = MSB, Fill Order = MSB		      */
void MMUQtoQ(
     CARD8 *src,	/* source byte stream 			      */
     QuadPixel *dst,	/* destination line			      */
     CARD32 numcmp,	/* number of pixels on to be converted	      */
     CARD32 leftpad,	/* bit offset to start of first pixel	      */
     CARD32 depth,	/* number of bits per pixel		      */
     CARD32 stride)	/* number of bits needed for this band	      */
{
CARD32 p, bits, wval1 = 56 - depth, wval2 = 32 - depth;

    /* Note: Maximum depth is 24 bits */

    if (leftpad > 7) {
    	src += leftpad >> 3;
	bits = leftpad & 7;
    } else
	bits = leftpad;

    for (p = 0; p < numcmp; p++) {
	if (bits + depth > 24)
	    *dst++ = ((CARD32)*src << (24 + bits) >> wval2)	|
		      (CARD32)*(src + 1) << (depth + bits - 16) |
		      (CARD32)*(src + 2) << (depth + bits - 24) |
		      (CARD32)*(src + 3) >> (wval2 - bits);
	else
	    *dst++ = ((CARD32)*src << (24 + bits) >> wval2)	|
		      (CARD32)*(src + 1) << (depth + bits - 16) |
		      (CARD32)*(src + 2) >> (wval1 - bits);

	bits += stride;
	src += bits >> 3;
	bits &= 7;
    }
}

#if XIE_FULL
/* Triple band by pixel single unaligned pixel extraction routines */

#define getLLByte(src,bits,depth)					\
	(BytePixel)((bits + depth > 8) ?				\
	    (*src >> bits | (CARD8)(*(src + 1)		 		\
		<< (16 - depth - bits)) >> (8 - depth)) :		\
	    ((CARD8)(*src << (8 - depth - bits)) >> (8 - depth)))

#define getLMByte(src,bits,depth)					\
	(BytePixel)((bits + depth > 8) ? 				\
	    ((CARD8)(*src << bits) >> bits | 				\
		(CARD8)(*(src + 1) >> (16 - depth - bits)) << (8 - bits)) : \
	    ((CARD8)(*src << bits) >> (8 - depth)))

#define getMLByte(src,bits,depth)					\
	(BytePixel)((bits + depth > 8) ? 				\
	    ((CARD8)(*src >> bits) << (depth + bits - 8) | 		\
	        (CARD8)(*(src + 1) << (16 - depth - bits)) 		\
		      >> (16 - depth - bits)) :				\
	    ((CARD8)(*src << (8 - depth - bits)) >> (8 - depth)))

#define getMMByte(src,bits,depth)					\
	(BytePixel)((bits + depth > 8) ? 				\
	    ((CARD8)(*src << bits) >> (8 - depth) | 			\
		     (CARD8)*(src + 1) >> (16 - depth - bits)) :	\
	    ((CARD8)(*src << bits) >> (8 - depth)))

#define getLLPair(src,bits,depth)					\
	(PairPixel)((bits + depth > 16) ? 				\
	    ((CARD16)*src >> bits				      | \
		     (CARD16)*(src + 1) << (8 - bits) 		      | \
		     (CARD16)((CARD16)*(src + 2) << (32 - depth - bits))\
			>> (16 - depth)) :				\
	    ((CARD16)*src >> bits 	    			      | \
		     (CARD16)((CARD16)*(src + 1) 			\
			<< (24 - depth - bits)) >> (16 - depth)))

#define getLMPair(src,bits,depth)					\
	(PairPixel)((bits + depth > 16) ? 				\
	    ((CARD16)((CARD16)*src << (8 + bits)) >> (8 + bits)	      | \
		     (CARD16)*(src + 1) << (8 - bits) 		      | \
	     	     (CARD16)((CARD16)*(src + 2) >> (24 - depth - bits))\
				<< (16 - bits)) :			\
	    ((CARD16)((CARD16)*src << (8 + bits)) >> (8 + bits)	      | \
		     (CARD16)((CARD16)*(src + 1) >> (16 - depth - bits))\
				<< (8 - bits)))

#define getMLPair(src,bits,depth)					\
	(PairPixel)((bits + depth > 16) ? 				\
	    ((CARD16)((CARD16)*src >> bits) << (depth + bits - 8)     | \
		     (CARD16)*(src + 1) << (depth + bits - 16)        | \
		     (CARD16)((CARD16)*(src + 2) << 			\
			(32 - depth - bits)) >> (32 - depth - bits)) :	\
	    ((CARD16)((CARD16)*src >> bits) << (depth + bits - 8)     | \
		     (CARD16)((CARD16)*(src + 1) << (24 - depth - bits))\
			>> (24 - depth - bits)))

#define getMMPair(src,bits,depth)					\
	(PairPixel)((bits + depth > 16) ? 				\
	    ((CARD16)((CARD16)*src << (8 + bits)) >> (16 - depth)     | \
		     (CARD16)*(src + 1) << (depth + bits - 16) 	      | \
		     (CARD16)*(src + 2) >> (24 - depth - bits)) :	\
	    ((CARD16)((CARD16)*src << (8 + bits)) >> (16 - depth)     | \
		     (CARD16)*(src + 1) >> (16 - depth - bits)))


/* Convert a triple band by pixel line to three lines of BytePixels */
#define ProtoThree(fname,dtype0,dtype1,dtype2)				 \
void fname(								 \
     CARD8  *src0,    /* source byte stream 			      */ \
     dtype0 *dst0,    /* destination line			      */ \
     dtype1 *dst1,    /* destination line			      */ \
     dtype2 *dst2,    /* destination line			      */ \
     CARD32 numcmp,   /* number of triple pixels on to be converted   */ \
     CARD32 leftpad,  /* bit offset to start of first pixel	      */ \
     CARD32 depth0,   /* number of bits per pixel		      */ \
     CARD32 depth1,   /* number of bits per pixel		      */ \
     CARD32 depth2,   /* number of bits per pixel		      */ \
     CARD32 stride    /* number of bits between triples 	      */ \
     )

#define ExtractThree(fname,dtype0,dtype1,dtype2,extract0,extract1,extract2) \
extern ProtoThree(fname,dtype0,dtype1,dtype2);				 \
ProtoThree(fname,dtype0,dtype1,dtype2)					 \
{									 \
CARD32 p, bits;								 \
    if (leftpad > 7) {							 \
    	src0 += (leftpad >> 3);						 \
	bits = (leftpad & 7);						 \
    } else								 \
	bits = leftpad; 						 \
    for (p = 0; p < numcmp; p++) {					 \
	CARD32 offset1 = (bits + depth0) & 7;				 \
	CARD32 offset2 = (bits + depth0 + depth1) & 7;			 \
	CARD8 *src1 = src0 + ((bits + depth0) >> 3);			 \
	CARD8 *src2 = src0 + ((bits + depth0 + depth1) >> 3);		 \
	*dst0++ = extract0(src0, bits,    depth0);			 \
	*dst1++ = extract1(src1, offset1, depth1);			 \
	*dst2++ = extract2(src2, offset2, depth2);			 \
	if ((bits += stride) > 7) {					 \
		src0 += (bits >> 3);					 \
		bits &= 7;						 \
	}								 \
    }									 \
}

ExtractThree(LLTBtoBBB,BytePixel,BytePixel,BytePixel,getLLByte,getLLByte,getLLByte)
ExtractThree(LLTBtoBBP,BytePixel,BytePixel,PairPixel,getLLByte,getLLByte,getLLPair)
ExtractThree(LLTBtoBPB,BytePixel,PairPixel,BytePixel,getLLByte,getLLPair,getLLByte)
ExtractThree(LLTBtoBPP,BytePixel,PairPixel,PairPixel,getLLByte,getLLPair,getLLPair)

ExtractThree(LLTBtoPBB,PairPixel,BytePixel,BytePixel,getLLPair,getLLByte,getLLByte)
ExtractThree(LLTBtoPBP,PairPixel,BytePixel,PairPixel,getLLPair,getLLByte,getLLPair)
ExtractThree(LLTBtoPPB,PairPixel,PairPixel,BytePixel,getLLPair,getLLPair,getLLByte)
ExtractThree(LLTBtoPPP,PairPixel,PairPixel,PairPixel,getLLPair,getLLPair,getLLPair)

ExtractThree(LMTBtoBBB,BytePixel,BytePixel,BytePixel,getLMByte,getLMByte,getLMByte)
ExtractThree(LMTBtoBBP,BytePixel,BytePixel,PairPixel,getLMByte,getLMByte,getLMPair)
ExtractThree(LMTBtoBPB,BytePixel,PairPixel,BytePixel,getLMByte,getLMPair,getLMByte)
ExtractThree(LMTBtoBPP,BytePixel,PairPixel,PairPixel,getLMByte,getLMPair,getLMPair)

ExtractThree(LMTBtoPBB,PairPixel,BytePixel,BytePixel,getLMPair,getLMByte,getLMByte)
ExtractThree(LMTBtoPBP,PairPixel,BytePixel,PairPixel,getLMPair,getLMByte,getLMPair)
ExtractThree(LMTBtoPPB,PairPixel,PairPixel,BytePixel,getLMPair,getLMPair,getLMByte)
ExtractThree(LMTBtoPPP,PairPixel,PairPixel,PairPixel,getLMPair,getLMPair,getLMPair)

ExtractThree(MLTBtoBBB,BytePixel,BytePixel,BytePixel,getMLByte,getMLByte,getMLByte)
ExtractThree(MLTBtoBBP,BytePixel,BytePixel,PairPixel,getMLByte,getMLByte,getMLPair)
ExtractThree(MLTBtoBPB,BytePixel,PairPixel,BytePixel,getMLByte,getMLPair,getMLByte)
ExtractThree(MLTBtoBPP,BytePixel,PairPixel,PairPixel,getMLByte,getMLPair,getMLPair)

ExtractThree(MLTBtoPBB,PairPixel,BytePixel,BytePixel,getMLPair,getMLByte,getMLByte)
ExtractThree(MLTBtoPBP,PairPixel,BytePixel,PairPixel,getMLPair,getMLByte,getMLPair)
ExtractThree(MLTBtoPPB,PairPixel,PairPixel,BytePixel,getMLPair,getMLPair,getMLByte)
ExtractThree(MLTBtoPPP,PairPixel,PairPixel,PairPixel,getMLPair,getMLPair,getMLPair)

ExtractThree(MMTBtoBBB,BytePixel,BytePixel,BytePixel,getMMByte,getMMByte,getMMByte)
ExtractThree(MMTBtoBBP,BytePixel,BytePixel,PairPixel,getMMByte,getMMByte,getMMPair)
ExtractThree(MMTBtoBPB,BytePixel,PairPixel,BytePixel,getMMByte,getMMPair,getMMByte)
ExtractThree(MMTBtoBPP,BytePixel,PairPixel,PairPixel,getMMByte,getMMPair,getMMPair)

ExtractThree(MMTBtoPBB,PairPixel,BytePixel,BytePixel,getMMPair,getMMByte,getMMByte)
ExtractThree(MMTBtoPBP,PairPixel,BytePixel,PairPixel,getMMPair,getMMByte,getMMPair)
ExtractThree(MMTBtoPPB,PairPixel,PairPixel,BytePixel,getMMPair,getMMPair,getMMByte)
ExtractThree(MMTBtoPPP,PairPixel,PairPixel,PairPixel,getMMPair,getMMPair,getMMPair)

void (*ExtractTripleFuncs[2][2][2][2][2])() =
{ { { {	{ LLTBtoBBB, LLTBtoBBP }, { LLTBtoBPB, LLTBtoBPP } },
      { { LLTBtoPBB, LLTBtoPBP }, { LLTBtoPPB, LLTBtoPPP } } },
    { {	{ LMTBtoBBB, LMTBtoBBP }, { LMTBtoBPB, LMTBtoBPP } },
      {	{ LMTBtoPBB, LMTBtoPBP }, { LMTBtoPPB, LMTBtoPPP } } } },
  { { {	{ MLTBtoBBB, MLTBtoBBP }, { MLTBtoBPB, MLTBtoBPP } },
      {	{ MLTBtoPBB, MLTBtoPBP }, { MLTBtoPPB, MLTBtoPPP } } },
    { {	{ MMTBtoBBB, MMTBtoBBP }, { MMTBtoBPB, MMTBtoBPP } },
      {	{ MMTBtoPBB, MMTBtoPBP }, { MMTBtoPPB, MMTBtoPPP } } } } };
#endif /* XIE_FULL */

/* end module miuncomp.c */


