/* $Xorg: meuncomp.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module meuncomp.c ****/
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
  
	meuncomp.c -- DDXIE routines for exporting uncompressed data
  
	Dean Verheiden -- AGE Logic, Inc. October 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/meuncomp.c,v 1.6 2001/12/14 19:58:21 dawes Exp $ */

#define _XIEC_MEUNCOMP

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
#include <meuncomp.h>


/***********************************************************************/
/****************** Aligned data conversion routines *******************/
/***********************************************************************/

/* Band by Plane action routines */

/* bits to stream */
void btoS(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
	memcpy((char *)dst,(char *)src, (int)(pvt->width + 7) >> 3);
}

/* swapped bits to stream */
void sbtoS(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
int   i, count;

	count = (pvt->width  + 7) >> 3; /* Pack down to bits */

	for (i = 0; i < count; i++) *dst++ = _ByteReverseTable[*src++];
}

/* bits to stream with pad */
void btoIS(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
CARD32 stride = pvt->stride;
CARD32  width = pvt->width;
CARD32  pitch = pvt->pitch;
CARD32      j = pvt->bitOff;
CARD32 i;

	if (j) /* Don't bzero partial byte left from last scanline */
	    bzero(dst + 1, ((j + pitch + 7) >> 3) - 1); 
	else
	    bzero(dst, (pitch + 7) >> 3);

	for (i = 0; i < width; i++, j += stride) {
	    if (LOGBYTE_tstbit(src,i) != 0) {
		LOGBYTE_setbit(dst,j);	
	    }
	}
}

/* swapped bits to stream with pad */
void sbtoIS(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
CARD32 stride = pvt->stride;
CARD32  width = pvt->width;
CARD32  pitch = pvt->pitch;
CARD32      j = pvt->bitOff;
CARD32 i, s;

	if (j) /* Don't bzero partial byte left from last scanline */
	    bzero(dst + 1, ((j + pitch + 7) >> 3) - 1); 
	else
	    bzero(dst, (pitch + 7) >> 3);

	for (i = 0; i < width; i++, j += stride) {
	    s = j ^ 7; /* Larry swap */
	    if (LOGBYTE_tstbit(src,i) != 0) {
		LOGBYTE_setbit(dst,s);	
	    }
	}
}

/* BytePixel to Stream, no offsets */
void BtoS(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
int   width = pvt->width;

	memcpy((char *)dst,(char *)src,width);
}

/* PairPixel to Stream, no swapping */
void PtoS(PairPixel *src, PairPixel *dst, meUncompPtr pvt)
{
int   width = pvt->width << 1;

	memcpy((char *)dst,(char *)src,width);
}

/* PairPixel to Stream, with swapping */
void sPtoS(PairPixel *src, PairPixel *dst, meUncompPtr pvt)
{
CARD32   i;

	for (i = 0; i < pvt->width; i++) { cpswaps(src[i],dst[i]); }
}

/* QuadPixel to Stream, no swapping */
void QtoS(QuadPixel *src, QuadPixel *dst, meUncompPtr pvt)
{
int   width = pvt->width << 2;

	memcpy((char *)dst,(char *)src,width);
}

/* QuadPixel to Stream, with swapping */
void sQtoS(QuadPixel *src, QuadPixel *dst, meUncompPtr pvt)
{
CARD32   i;

	for (i = 0; i < pvt->width; i++) { cpswapl(src[i],dst[i]); }
}

/* QuadPixel (unswapped) to padded Stream */
void QtoIS(QuadPixel *src, QuadPixel *dst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD32 i;
	for (i = 0; i < width; i++, dst += Bstride) *dst = *src++;
}

/* QuadPixel (swapped) to padded Stream */
void sQtoIS(QuadPixel *src, QuadPixel *dst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD32 i;
	for (i = 0; i < width; i++, dst += Bstride) { cpswapl(src[i], *dst); }
}

/* Triple Band Byte by Pixel Action routines */

/* Bits to Interleaved stream bits */
void btoISb(BytePixel *isrc, BytePixel *dst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD8     mask = pvt->mask;
LogInt	  *src = (LogInt *)isrc;
int i;

	dst += pvt->dstoffset;
	for (i = 0; i < width; i++, dst += Bstride) 
	    if (LOG_tstbit(src,i) != 0)
		*dst |= mask;
}

/* BytePixels to Interleaved Stream in bits */
void BtoISb(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD8     mask = pvt->mask;
CARD8    shift = pvt->shift;
CARD32 i;

	dst += pvt->dstoffset;
	for (i = 0; i < width; i++, dst += Bstride) 
		*dst |= *src++ << shift & mask;
}

/* BytePixel to Interleaved Stream */
void BtoIS(BytePixel *src, BytePixel *dst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD32 i;

	dst += pvt->dstoffset;
	for (i = 0; i < width; i++, dst += Bstride) 
		*dst = *src++;
}

/* PairPixel (unswapped) to Interleaved Stream */
void PtoIS(PairPixel *src, PairPixel *idst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD8 *dst = ((CARD8 *)idst) + pvt->dstoffset;
CARD32 i;
	for (i = 0; i < width; i++, dst += Bstride) 
		*((PairPixel *)dst) = *src++;
}

/* PairPixel (swapped) to Interleaved Stream */
void sPtoIS(PairPixel *src, PairPixel *idst, meUncompPtr pvt)
{
CARD32 Bstride = pvt->Bstride;
CARD32   width = pvt->width;
CARD8 *dst = ((CARD8 *)idst) + pvt->dstoffset;
CARD32 i;
	for (i = 0; i < width; i++, dst += Bstride) {
		PairPixel sval = *src++;
		*dst = (sval >> 8) | (sval << 8);
	}
}

/***********************************************************************/
/***************** Unaligned data conversion routines ******************/
/***********************************************************************/

/* Pack a line of byte pixels          */
/* Pixel Order = LSB, Fill Order = LSB */
void BtoLLUB(src, dst, pvt)
BytePixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
BytePixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 stride = pvt->stride;     
CARD16 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;

    while (src < send) {
	outb |= (CARD16)*src++ << bits;
	bits += stride;
	while (bits > 7) {
		*dst++ = (CARD8)outb;
		outb >>= 8;
		bits -= 8;
	}
    }
    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of byte pixels          */
/* Pixel Order = LSB, Fill Order = MSB */
void BtoLMUB(src, dst, pvt)
BytePixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
BytePixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 stride = pvt->stride;     
CARD32 depth  = pvt->depth;
CARD16 bits   = pvt->bitOff;
CARD16 outb   = pvt->leftOver;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD8 sval = *src++;

	if (bits + depth <= 8) {	/* Pack into existing byte only  */
	    outb |= sval << (8 - bits - depth);
	    if (bits + depth == 8) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits += depth;
	} else {			/* Split between bytes		 */
	    outb |= (CARD8)(sval << bits) >> bits;/* Chop off top bits*/
	    *dst++ = (CARD8)outb;
	    if (bits + depth <= 16) {
	        outb = (CARD8)(sval >> (8 - bits)) << (16 - depth - bits);
	        bits = bits + depth - 8;  /* watch those signs */
		if (bits + depth == 16) {
		    *dst++ = (CARD8)outb;
		    outb = 0;
		    bits = 0;
		}
	    } 
	}
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }
    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of byte pixels          */
/* Pixel Order = MSB, Fill Order = LSB */
void BtoMLUB(src, dst, pvt)
BytePixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
BytePixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD16 bits   = pvt->bitOff;
CARD16 outb   = pvt->leftOver;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD8 sval = *src++;

	if (bits + depth <= 8) {	/* Pack into existing byte only  */
	    outb |= sval << bits;
	    if (bits + depth == 8) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits += depth;
	} else if (bits + depth <= 16) {
	    outb |= (CARD8)(sval >> (depth + bits - 8)) << bits;
	    *dst++ = (CARD8)outb;
	    outb = (CARD8)(sval << (16 - depth - bits)) >> (16 - depth - bits);
	    if (bits + depth == 16) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 8;
	} 
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }

    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

void BtoMMUB(src, dst, pvt) 
BytePixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
BytePixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD16 bits   = pvt->bitOff;
CARD16 outb   = pvt->leftOver;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD8 sval = *src++;

	if (bits + depth <= 8) {	/* Pack into existing byte only  */
	    outb |= sval << (8 - depth - bits);
	    if (bits + depth == 8) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits += depth;
	} else if (bits + depth <= 16) {
	    outb |= sval >> (depth + bits - 8);
	    *dst++ = (CARD8)outb;
	    outb = sval << (16 - depth - bits);
	    if (bits + depth == 16) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 8;
	} 
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }

    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of pair pixels          */
/* Pixel Order = LSB, Fill Order = LSB */
void PtoLLUP(src, dst, pvt)
PairPixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
PairPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 stride = pvt->stride;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;

    while (src < send) {
	outb |= *src++ << bits;
	bits += stride;
	while (bits > 7) {
		*dst++ = (CARD8)outb;
		outb >>= 8;
		bits -= 8;
	}
    }
    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of pair pixels          */	
/* Pixel Order = LSB, Fill Order = MSB */
void PtoLMUP(src, dst, pvt)
PairPixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
PairPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD16 sval = *src++;

        if (bits + depth <= 16) { 
            outb |= (CARD16)(sval << (8 + bits)) >> (8 + bits);
	    *dst++ = (CARD8)outb;
	    outb = (CARD8)(sval >> (8 - bits)) << (16 - depth - bits);
	    if (bits + depth == 16) {
	        *dst++ = (CARD8)outb;
	        outb = 0;
	        bits = 0;
	    } else 
	        bits = bits + depth - 8;  /* watch those signs */
       	} else { 
            outb |= (CARD16)(sval << (8 + bits)) >>  (8 + bits);
	    *dst++ = (CARD8)outb;
 	    *dst++ = (CARD8)((CARD16)(sval << bits) >> 8);
	    outb = (CARD8)(sval >> (16 - bits)) << (24 - depth - bits);
	    if (bits + depth == 24) {
	        *dst++ = (CARD8)outb;
	        outb = 0;
	        bits = 0;
	    } else 
	        bits = bits + depth - 16;  /* watch those signs */
	}
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }
    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of pair pixels          */ 
/* Pixel Order = MSB, Fill Order = LSB */
void PtoMLUP(src, dst, pvt)
PairPixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
PairPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD16 sval = *src++;

	if (bits + depth <= 16) {
	    outb |= (sval >> (depth + bits - 8)) << bits;
	    *dst++ = (CARD8)outb;
	    outb = (CARD16)(sval << (24 - depth - bits)) >> (24 - depth - bits); 
	    if (bits + depth == 16) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 8;
	} else {
	    outb |= (sval >> (depth + bits - 8)) << bits;
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));
	    outb = (CARD8)(sval << (24 - depth - bits)) >> (24 - depth - bits);
	    if (bits + depth == 24) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 16;
	} 
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }

    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

void PtoMMUP(src, dst, pvt) 
PairPixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
PairPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD16 sval = *src++;

	if (bits + depth <= 16) {
	    outb |= sval >> (depth + bits - 8);
	    *dst++ = (CARD8)outb;
	    outb = sval << (16 - depth - bits);
	    if (bits + depth == 16) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 8;
	} else {
	    outb |= sval >> (depth + bits - 8);
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));
	    outb = sval << (24 - depth - bits);
	    if (bits + depth == 24) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 16;
	}
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }

    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = (CARD8)outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of quad pixels          */
/* Pixel Order = LSB, Fill Order = LSB */
void QtoLLUQ(src, dst, pvt)
QuadPixel   *src;	
CARD8	    *dst;
meUncompPtr pvt;	
{
QuadPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 stride = pvt->stride;     
CARD32 outb0  = pvt->leftOver, outb1;
CARD16 bits   = pvt->bitOff;

    while (src < send) {
        if (bits) {	/* Yuck, do a multi word shift */
	    outb1  = *src >> (32 - bits);
	    outb0 |= *src++ << bits;
        } else {
	    outb1 = 0;
	    outb0 = *src++;
	}
        bits += stride;
	while (bits > 7) { 
		*dst++ = (CARD8)outb0;
		outb0 >>= 8;
		if (bits > 32) {
		    outb0 |= outb1 << 24; 
		    outb1 >>= 8;
		}
		bits -= 8;
	}
    }

    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = (CARD8)outb0; 
        } else {
            *dst = (CARD8)outb0;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of quad pixels          */
/* Pixel Order = LSB, Fill Order = MSB */
void QtoLMUQ(src, dst, pvt)	
QuadPixel *src;	
CARD8	  *dst;
meUncompPtr pvt;	
{
QuadPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 stride = pvt->stride;     
CARD32 depth  = pvt->depth;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD32 sval = *src++;

        if (bits + depth <= 24) { 
            outb |= (sval << (24 + bits)) >> (24 + bits);
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (8 - bits));
	    outb = (CARD8)(sval >> (16 - bits)) << (24 - depth - bits);
	    if (bits + depth == 24) {
	        *dst++ = (CARD8)outb;
	        outb = 0;
	        bits = 0;
	    } else
	        bits = bits + depth - 16;  /* watch those signs */
       	} else { 
            outb |= (sval << (24 + bits)) >> (24 + bits);
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (8 - bits));
	    *dst++ = (CARD8)(sval >> (16 - bits));
	    outb = (CARD8)(sval >> (24 - bits)) << (32 - depth - bits);
	    if (bits + depth == 32) {
	        *dst++ = (CARD8)outb;
	        outb = 0;
	        bits = 0;
	    } else
	        bits = bits + depth - 24;  /* watch those signs */
	}
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }
    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = (CARD8)outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of quad pixels          */
/* Pixel Order = MSB, Fill Order = LSB */
void QtoMLUQ(src, dst, pvt)	
QuadPixel *src;	
CARD8	  *dst;
meUncompPtr pvt;	
{
QuadPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD32 sval = *src++;

	if (bits + depth <= 24) {
	    outb |= (sval >> (depth + bits - 8)) << bits;
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));
	    outb = (sval << (48 - depth - bits)) >> (48 - depth - bits); 
	    if (bits + depth == 24) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 16;
	} else {
	    outb |= (sval >> (depth + bits - 8)) << bits;
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));
	    *dst++ = (CARD8)(sval >> (depth + bits - 24));
	    outb = (sval << (56 - depth - bits)) >> (56 - depth - bits);
	    if (bits + depth == 32) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 24;
	}
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }

    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a line of quad pixels          */
/* Pixel Order = MSB, Fill Order = MSB */
void QtoMMUQ(src, dst, pvt)	
QuadPixel *src;	
CARD8	  *dst;
meUncompPtr pvt;	
{
QuadPixel *send = &src[pvt->width];
CARD32 pitch  = pvt->pitch;     
CARD32 depth  = pvt->depth;     
CARD32 stride = pvt->stride;     
CARD32 outb   = pvt->leftOver;
CARD16 bits   = pvt->bitOff;
CARD32 pad    = stride - depth;

    while (src < send) {
	CARD32 sval = *src++;

	if (bits + depth <= 24) {
	    outb |= sval >> (depth + bits - 8);
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));
	    outb = (sval << (48 - depth - bits)) >> 24;
	    if (bits + depth == 24) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 16;	
	} else {
	    outb |= sval >> (depth + bits - 8);
	    *dst++ = (CARD8)outb;
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));
	    *dst++ = (CARD8)(sval >> (depth + bits - 24));
	    outb = (sval << (56 - depth - bits)) >> 24;
	    if (bits + depth == 32) {
		*dst++ = (CARD8)outb;
		outb = 0;
		bits = 0;
	    } else
	        bits = bits + depth - 24;
	} 
        if (bits + pad > 8) {
	    *dst++ = outb;
	    outb = 0;
	    bits = bits + pad - 8;
	    while (bits > 7) {	
	        *dst++ = 0;
	        bits -= 8;
	    }	
        } else 
	     bits += pad;						
    }

    
    /* If something leftover, either write it out or save for packing */
    if (bits) {
        if (pitch & 7) {
	    pvt->leftOver = (CARD8)outb; 
        } else {
            *dst = (CARD8)outb;
	    pvt->leftOver = 0;
	}
    } else 
       pvt->leftOver = 0;
}

/* Pack a triple band by pixel line of pixels          */
/* Pixel Order = LSB, Fill Order = LSB */

#define ProtoLLTB(fname,itype0,itype1,itype2)				\
void fname(								\
     itype0 *src0,							\
     itype1 *src1,							\
     itype2 *src2,							\
     CARD8 *dst,							\
     CARD32 tristride,							\
     meUncompPtr pvt)

#define ConvertToLLTB(fname,itype0,itype1,itype2)			\
extern ProtoLLTB(fname,itype0,itype1,itype2);				\
ProtoLLTB(fname,itype0,itype1,itype2)					\
{									\
itype0 *send = &src0[pvt->width];/* All three bands are same width */	\
CARD32 pitch  = pvt->pitch;     					\
CARD32 outb   = pvt->leftOver;						\
CARD16 bits   = pvt->bitOff;						\
CARD32 depth0 = pvt[0].depth;						\
CARD32 depth1 = pvt[1].depth;						\
CARD32 tripad = tristride - pvt[0].depth - pvt[1].depth;		\
    while (src0 < send) {						\
	outb |= (CARD32)*src0++ << bits;				\
	bits += depth0;							\
	while (bits > 7) {						\
		*dst++ = (CARD8)outb;					\
		outb >>= 8;						\
		bits -= 8;						\
	}								\
	outb |= (CARD32)*src1++ << bits;				\
	bits += depth1;							\
	while (bits > 7) {						\
		*dst++ = (CARD8)outb;					\
		outb >>= 8;						\
		bits -= 8;						\
	}								\
	outb |= (CARD32)*src2++ << bits;				\
	bits += tripad;							\
	while (bits > 7) {						\
		*dst++ = (CARD8)outb;					\
		outb >>= 8;						\
		bits -= 8;						\
	}								\
    }									\
    if (bits) {								\
        if (pitch & 7) {						\
	    pvt->leftOver = outb; 					\
        } else {							\
            *dst = (CARD8)outb;						\
	    pvt->leftOver = 0;						\
	}								\
    } else 								\
       pvt->leftOver = 0;						\
}

/* Pack a triple band by pixel line of pixels */
/* Pixel Order = LSB, Fill Order = MSB */
/* Broken in two parts because some preprocessors cannot handle the full size*/
#define  ConvertToLMTB2()						\
      if (bits + tripad > 8) {						\
	  *dst++ = outb;						\
	  outb = 0;							\
	  bits = bits + tripad - 8;					\
	  while (bits > 7) {						\
	      *dst++ = 0;						\
	      bits -= 8;						\
	  }								\
      } else 								\
	 bits += tripad;						\
    }									\
    if (bits) {								\
        if (pitch & 7) {						\
	    pvt->leftOver = outb; 					\
        } else {							\
            *dst = (CARD8)outb;						\
	    pvt->leftOver = 0;						\
	}								\
    } else 								\
       pvt->leftOver = 0;						

#define ProtoLMTB(fname,xietype0,xietype1,xietype2)			\
void fname(								\
     xietype0 *src0,							\
     xietype1 *src1,							\
     xietype2 *src2,							\
     CARD8 *dst,							\
     CARD32 tristride,							\
     meUncompPtr pvt)

#define ConvertToLMTB(fname,xietype0,xietype1,xietype2)			\
extern ProtoLMTB(fname,xietype0,xietype1,xietype2);			\
ProtoLMTB(fname,xietype0,xietype1,xietype2)				\
{									\
xietype0 *send = &src0[pvt->width];/* All three bands are same width */	\
CARD32 pitch    = pvt->pitch;     					\
CARD32 outb     = pvt->leftOver;					\
CARD16 bits     = pvt->bitOff;						\
CARD32 tripad   = tristride - pvt[0].depth - pvt[1].depth - pvt[2].depth;\
CARD32 b;								\
    while (src0 < send) {						\
      CARD16 svals[3];							\
      svals[0] = *src0++; svals[1] = *src1++; svals[2] = *src2++;	\
      for (b = 0; b < 3; b++) {						\
	  CARD16 sval  = svals[b];					\
	  CARD32 depth = pvt[b].depth;					\
	  if (bits + depth <= 8) {	/* Pack into existing byte only  */\
	      outb |= sval << (8 - depth - bits);			\
	      if (bits + depth == 8) {					\
		  *dst++ = (CARD8)outb;					\
		  outb = 0;						\
		  bits = 0;						\
	      } else							\
	          bits += depth;					\
          } else if (bits + depth <= 16) { 				\
              outb |= (CARD16)(sval << (8 + bits)) >> (8 + bits);	\
	      *dst++ = (CARD8)outb;					\
	      outb = (CARD8)(sval >> (8 - bits)) << (16 - depth - bits);\
	      if (bits + depth == 16) {					\
	          *dst++ = (CARD8)outb;					\
	          outb = 0;						\
	          bits = 0;						\
	      } else 							\
	          bits = bits + depth - 8;  /* watch those signs */	\
       	  } else { 							\
              outb |= (CARD16)(sval << (8 + bits)) >>  (8 + bits);	\
	      *dst++ = (CARD8)outb;					\
 	      *dst++ = (CARD8)((CARD16)(sval << bits) >> 8);		\
	      outb = (CARD8)(sval >> (16 - bits)) << (24 - depth - bits);\
	      if (bits + depth == 24) {					\
	          *dst++ = (CARD8)outb;					\
	          outb = 0;						\
	          bits = 0;						\
	      } else 							\
	          bits = bits + depth - 16;  /* watch those signs */	\
	  }								\
      }									\
      ConvertToLMTB2()							\
}

/* Pack a triple band by pixel line of pixels */
/* Pixel Order = MSB, Fill Order = LSB */
/* Broken in two parts because some preprocessors cannot handle the full size*/
#define ConvertToMLTB2()						\
      if (bits + tripad > 8) {						\
	  *dst++ = outb;						\
	  outb = 0;							\
	  bits = bits + tripad - 8;					\
	  while (bits > 7) {						\
	      *dst++ = 0;						\
	      bits -= 8;						\
	  }								\
      } else 								\
	 bits += tripad;						\
    }									\
    if (bits) {								\
        if (pitch & 7) {						\
	    pvt->leftOver = outb; 					\
        } else {							\
            *dst = (CARD8)outb;						\
	    pvt->leftOver = 0;						\
	}								\
    } else 								\
       pvt->leftOver = 0;						

#define ProtoMLTB(fname,itype0,itype1,itype2)				\
void fname(								\
     itype0 *src0,							\
     itype1 *src1,							\
     itype2 *src2,							\
     CARD8 *dst,							\
     CARD32 tristride,							\
     meUncompPtr pvt)

#define ConvertToMLTB(fname,itype0,itype1,itype2)			\
extern ProtoMLTB(fname,itype0,itype1,itype2);				\
ProtoMLTB(fname,itype0,itype1,itype2)					\
{									\
itype0 *send = &src0[pvt->width];/* All three bands are same width */	\
CARD32 pitch    = pvt->pitch;     					\
CARD32 outb     = pvt->leftOver;					\
CARD16 bits     = pvt->bitOff;						\
CARD32 tripad   = tristride - pvt[0].depth - pvt[1].depth - pvt[2].depth;\
CARD32 b;								\
    while (src0 < send) {						\
      CARD16 svals[3];							\
      svals[0] = *src0++; svals[1] = *src1++; svals[2] = *src2++;	\
      for (b = 0; b < 3; b++) {						\
	CARD16 sval = svals[b];						\
	CARD32 depth = pvt[b].depth;					\
	if (bits + depth <= 8) {	/* Pack into existing byte only  */\
	    outb |= sval << bits;					\
	    if (bits + depth == 8) {					\
		*dst++ = (CARD8)outb;					\
		outb = 0;						\
		bits = 0;						\
	    } else							\
	        bits += depth;						\
	} else if (bits + depth <= 16) {				\
	    outb |= (sval >> (depth + bits - 8)) << bits;		\
	    *dst++ = (CARD8)outb;					\
	    outb = (CARD16)(sval << (24 - depth - bits)) >> (24 - depth - bits); \
	    if (bits + depth == 16) {					\
		*dst++ = (CARD8)outb;					\
		outb = 0;						\
		bits = 0;						\
	    } else							\
	        bits = bits + depth - 8;				\
	} else {							\
	    outb |= (sval >> (depth + bits - 8)) << bits;		\
	    *dst++ = (CARD8)outb;					\
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));		\
	    outb = (CARD8)(sval << (24 - depth - bits)) >> (24 - depth - bits);\
	    if (bits + depth == 24) {					\
		*dst++ = (CARD8)outb;					\
		outb = 0;						\
		bits = 0;						\
	    } else							\
	        bits = bits + depth - 16;				\
	}								\
      }									\
      ConvertToMLTB2()							\
}

/* Pack a triple band by pixel line of pixels */
/* Pixel Order = MSB, Fill Order = MSB 	      */
/* Broken in two parts because some preprocessors cannot handle the full size*/
#define ConvertToMMTB2() 						\
      if (bits + tripad > 8) {						\
	  *dst++ = outb;						\
	  outb = 0;							\
	  bits = bits + tripad - 8;					\
	  while (bits > 7) {						\
	      *dst++ = 0;						\
	      bits -= 8;						\
	  }								\
      } else 								\
	 bits += tripad;						\
    }									\
    if (bits) {								\
        if (pitch & 7) {						\
	    pvt->leftOver = (CARD8)outb; 				\
        } else {							\
            *dst = (CARD8)outb;						\
	    pvt->leftOver = 0;						\
	}								\
    } else 								\
       pvt->leftOver = 0;						

#define ProtoMMTB(fname,itype0,itype1,itype2)				\
void fname(								\
     itype0 *src0,							\
     itype1 *src1,							\
     itype2 *src2,							\
     CARD8 *dst,							\
     CARD32 tristride,							\
     meUncompPtr pvt)

#define ConvertToMMTB(fname,itype0,itype1,itype2)			\
extern ProtoMMTB(fname,itype0,itype1,itype2);				\
ProtoMMTB(fname,itype0,itype1,itype2)					\
{									\
itype0 *send = &src0[pvt->width];/* All three bands are same width */   \
CARD32 pitch    = pvt->pitch;     					\
CARD32 outb     = pvt->leftOver;					\
CARD16 bits     = pvt->bitOff;						\
CARD32 tripad   = tristride - pvt[0].depth - pvt[1].depth - pvt[2].depth;\
CARD32 b;								\
    while (src0 < send) {						\
      CARD16 svals[3];							\
      svals[0] = *src0++; svals[1] = *src1++; svals[2] = *src2++;	\
      for (b = 0; b < 3; b++) {						\
	CARD16 sval = svals[b];						\
	CARD32 depth;							\
	depth = pvt[b].depth;						\
	if (bits + depth <= 8) {/* Pack into existing byte only  */	\
	    outb |= sval << (8 - depth - bits);				\
	    if (bits + depth == 8) {					\
		*dst++ = (CARD8)outb;					\
		outb = 0;						\
		bits = 0;						\
	    } else							\
	        bits += depth;						\
	} else if (bits + depth <= 16) {				\
	    outb |= sval >> (depth + bits - 8);				\
	    *dst++ = (CARD8)outb;					\
	    outb = sval << (16 - depth - bits);				\
	    if (bits + depth == 16) {					\
		*dst++ = (CARD8)outb;					\
		outb = 0;						\
		bits = 0;						\
	    } else							\
	        bits = bits + depth - 8;				\
	} else {							\
	    outb |= sval >> (depth + bits - 8);				\
	    *dst++ = (CARD8)outb;					\
	    *dst++ = (CARD8)(sval >> (depth + bits - 16));		\
	    outb = sval << (24 - depth - bits);				\
	    if (bits + depth == 24) {					\
		*dst++ = (CARD8)outb;					\
		outb = 0;						\
		bits = 0;						\
	    } else							\
	        bits = bits + depth - 16;				\
	}								\
      }									\
      ConvertToMMTB2()							\
}				


ConvertToLLTB(BBBtoLLTB,BytePixel,BytePixel,BytePixel)			
ConvertToLMTB(BBBtoLMTB,BytePixel,BytePixel,BytePixel)			
ConvertToMLTB(BBBtoMLTB,BytePixel,BytePixel,BytePixel)			
ConvertToMMTB(BBBtoMMTB,BytePixel,BytePixel,BytePixel)			

ConvertToLLTB(BBPtoLLTB,BytePixel,BytePixel,PairPixel)			
ConvertToLMTB(BBPtoLMTB,BytePixel,BytePixel,PairPixel)			
ConvertToMLTB(BBPtoMLTB,BytePixel,BytePixel,PairPixel)			
ConvertToMMTB(BBPtoMMTB,BytePixel,BytePixel,PairPixel)			

ConvertToLLTB(BPBtoLLTB,BytePixel,PairPixel,BytePixel)			
ConvertToLMTB(BPBtoLMTB,BytePixel,PairPixel,BytePixel)			
ConvertToMLTB(BPBtoMLTB,BytePixel,PairPixel,BytePixel)			
ConvertToMMTB(BPBtoMMTB,BytePixel,PairPixel,BytePixel)			

ConvertToLLTB(BPPtoLLTB,BytePixel,PairPixel,PairPixel)			
ConvertToLMTB(BPPtoLMTB,BytePixel,PairPixel,PairPixel)			
ConvertToMLTB(BPPtoMLTB,BytePixel,PairPixel,PairPixel)			
ConvertToMMTB(BPPtoMMTB,BytePixel,PairPixel,PairPixel)			

ConvertToLLTB(PBBtoLLTB,PairPixel,BytePixel,BytePixel)			
ConvertToLMTB(PBBtoLMTB,PairPixel,BytePixel,BytePixel)			
ConvertToMLTB(PBBtoMLTB,PairPixel,BytePixel,BytePixel)			
ConvertToMMTB(PBBtoMMTB,PairPixel,BytePixel,BytePixel)			

ConvertToLLTB(PBPtoLLTB,PairPixel,BytePixel,PairPixel)			
ConvertToLMTB(PBPtoLMTB,PairPixel,BytePixel,PairPixel)			
ConvertToMLTB(PBPtoMLTB,PairPixel,BytePixel,PairPixel)			
ConvertToMMTB(PBPtoMMTB,PairPixel,BytePixel,PairPixel)			

ConvertToLLTB(PPBtoLLTB,PairPixel,PairPixel,BytePixel)			
ConvertToLMTB(PPBtoLMTB,PairPixel,PairPixel,BytePixel)			
ConvertToMLTB(PPBtoMLTB,PairPixel,PairPixel,BytePixel)			
ConvertToMMTB(PPBtoMMTB,PairPixel,PairPixel,BytePixel)			

ConvertToLLTB(PPPtoLLTB,PairPixel,PairPixel,PairPixel)			
ConvertToLMTB(PPPtoLMTB,PairPixel,PairPixel,PairPixel)			
ConvertToMLTB(PPPtoMLTB,PairPixel,PairPixel,PairPixel)			
ConvertToMMTB(PPPtoMMTB,PairPixel,PairPixel,PairPixel)			

void (*EncodeTripleFuncs[2][2][2][2][2])() = 
{ { { {	{ BBBtoLLTB, BBPtoLLTB }, { BPBtoLLTB, BPPtoLLTB } },
      { { PBBtoLLTB, PBPtoLLTB }, { PPBtoLLTB, PPPtoLLTB } } },
    { {	{ BBBtoLMTB, BBPtoLMTB }, { BPBtoLMTB, BPPtoLMTB } },
      {	{ PBBtoLMTB, PBPtoLMTB }, { PPBtoLMTB, PPPtoLMTB } } } },
  { { {	{ BBBtoMLTB, BBPtoMLTB }, { BPBtoMLTB, BPPtoMLTB } },
      {	{ PBBtoMLTB, PBPtoMLTB }, { PPBtoMLTB, PPPtoMLTB } } },
    { {	{ BBBtoMMTB, BBPtoMMTB }, { BPBtoMMTB, BPPtoMMTB } },
      {	{ PBBtoMMTB, PBPtoMMTB }, { PPBtoMMTB, PPPtoMMTB } } } } };
/* end module meuncomp.c */
