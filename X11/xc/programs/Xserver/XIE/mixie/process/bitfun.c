/* $Xorg: bitfun.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/**** module bitfun.c ****/
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
  
	bitfun.c -- DDXIE utilities for bits and other fun things.
  
	Larry Hare -- AGE Logic, Inc. July, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/bitfun.c,v 1.5 2001/12/14 19:58:42 dawes Exp $ */

#define _XIEC_MPBITFUN

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

#include "xiemd.h"

/*---------------------------------------------------------------------------
--------------------------------  bitexpand  --------------------------------
---------------------------------------------------------------------------*/

/*
**  Following routine is used to expand a length of bit pixels to byte
**  pixels.  Similar routines could be written for other pixels sizes
**  but currently we don't really support them (eg 4 bits) or doesnt
**  really speed them up (16 bits).  
**
**  You could also do this with any other bit expansion tricks you 
**  have up your sleeve.  For instance, you could look up 8 bits
**  at a time, and have a table of 256 long longs; or use your 
**  favorite graphics accelerator, or ...  bitfun.
*/


CARD32 xie8StippleMasks[16] = {
0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff,
};

/* byte reversal table */
unsigned char _ByteReverseTable[]= {
 0x00,  0x80,  0x40,  0xc0,  0x20,  0xa0,  0x60,  0xe0, 
 0x10,  0x90,  0x50,  0xd0,  0x30,  0xb0,  0x70,  0xf0, 
 0x08,  0x88,  0x48,  0xc8,  0x28,  0xa8,  0x68,  0xe8, 
 0x18,  0x98,  0x58,  0xd8,  0x38,  0xb8,  0x78,  0xf8, 
 0x04,  0x84,  0x44,  0xc4,  0x24,  0xa4,  0x64,  0xe4, 
 0x14,  0x94,  0x54,  0xd4,  0x34,  0xb4,  0x74,  0xf4, 
 0x0c,  0x8c,  0x4c,  0xcc,  0x2c,  0xac,  0x6c,  0xec, 
 0x1c,  0x9c,  0x5c,  0xdc,  0x3c,  0xbc,  0x7c,  0xfc, 
 0x02,  0x82,  0x42,  0xc2,  0x22,  0xa2,  0x62,  0xe2, 
 0x12,  0x92,  0x52,  0xd2,  0x32,  0xb2,  0x72,  0xf2, 
 0x0a,  0x8a,  0x4a,  0xca,  0x2a,  0xaa,  0x6a,  0xea, 
 0x1a,  0x9a,  0x5a,  0xda,  0x3a,  0xba,  0x7a,  0xfa, 
 0x06,  0x86,  0x46,  0xc6,  0x26,  0xa6,  0x66,  0xe6, 
 0x16,  0x96,  0x56,  0xd6,  0x36,  0xb6,  0x76,  0xf6, 
 0x0e,  0x8e,  0x4e,  0xce,  0x2e,  0xae,  0x6e,  0xee, 
 0x1e,  0x9e,  0x5e,  0xde,  0x3e,  0xbe,  0x7e,  0xfe, 
 0x01,  0x81,  0x41,  0xc1,  0x21,  0xa1,  0x61,  0xe1, 
 0x11,  0x91,  0x51,  0xd1,  0x31,  0xb1,  0x71,  0xf1, 
 0x09,  0x89,  0x49,  0xc9,  0x29,  0xa9,  0x69,  0xe9, 
 0x19,  0x99,  0x59,  0xd9,  0x39,  0xb9,  0x79,  0xf9, 
 0x05,  0x85,  0x45,  0xc5,  0x25,  0xa5,  0x65,  0xe5, 
 0x15,  0x95,  0x55,  0xd5,  0x35,  0xb5,  0x75,  0xf5, 
 0x0d,  0x8d,  0x4d,  0xcd,  0x2d,  0xad,  0x6d,  0xed, 
 0x1d,  0x9d,  0x5d,  0xdd,  0x3d,  0xbd,  0x7d,  0xfd, 
 0x03,  0x83,  0x43,  0xc3,  0x23,  0xa3,  0x63,  0xe3, 
 0x13,  0x93,  0x53,  0xd3,  0x33,  0xb3,  0x73,  0xf3, 
 0x0b,  0x8b,  0x4b,  0xcb,  0x2b,  0xab,  0x6b,  0xeb, 
 0x1b,  0x9b,  0x5b,  0xdb,  0x3b,  0xbb,  0x7b,  0xfb, 
 0x07,  0x87,  0x47,  0xc7,  0x27,  0xa7,  0x67,  0xe7, 
 0x17,  0x97,  0x57,  0xd7,  0x37,  0xb7,  0x77,  0xf7, 
 0x0f,  0x8f,  0x4f,  0xcf,  0x2f,  0xaf,  0x6f,  0xef, 
 0x1f,  0x9f,  0x5f,  0xdf,  0x3f,  0xbf,  0x7f,  0xff

};

#if (IMAGE_BYTE_ORDER == MSBFirst)
#define g4	(c = (inval>>26) & 0x3c, inval <<= 4,			\
		*((CARD32 *) ((int) &xie8StippleMasks[0] + c)) & val)
#define g4r	(c = (inval>>26) & 0x3c, inval <<= 4,			\
		~(*((CARD32 *) ((int) &xie8StippleMasks[0] + c))) & val)
#define g4b	(c = (inval>>26) & 0x3c, inval <<= 4,			\
		 c = *((CARD32 *) ((int) &xie8StippleMasks[0] + c)), \
						(loval & ~c) | (hival & c))
#else
#define g4	(c = inval & 0xf, inval >>= 4, xie8StippleMasks[c] & val)
#define g4r	(c = inval & 0xf, inval >>= 4, ~xie8StippleMasks[c] & val)
#define g4b	(c = inval & 0xf, inval >>= 4, c = xie8StippleMasks[c], \
						(loval & ~c) | (hival & c))
#endif

#define p4 *outp++		

pointer bitexpand(inp,outp,bw,olow,ohigh)
	CARD32 *inp;			/* these are actualy bits... */
	CARD32 *outp;			/* its actually bytes but ... */
	int bw;				/* number of bits to extract */
	unsigned char olow;		/* zero bit maps to this */
	unsigned char ohigh;		/* one bit maps to this */
{
	pointer dst = (pointer)outp;
	int nw;
	CARD32 inval, c;
	if (olow == 0) {
	    CARD32 val = ohigh;
	    val = (val << 8) | val; val = (val << 16) | val;
	    for ( nw = bw >> 5; nw > 0; nw--) {
		inval = *inp++;
		p4 = g4; p4 = g4; p4 = g4; p4 = g4;
		p4 = g4; p4 = g4; p4 = g4; p4 = g4;
	    }
	    if ((bw &= 31) > 0) 
		for (inval = *inp; bw > 0; bw -= 4)
		    p4 = g4;
	} else if (ohigh == 0) {
	    CARD32 val = olow;
	    val = (val << 8) | val; val = (val << 16) | val;
	    for ( nw = bw >> 5; nw > 0; nw--) {
		inval = *inp++;
		p4 = g4r; p4 = g4r; p4 = g4r; p4 = g4r;
		p4 = g4r; p4 = g4r; p4 = g4r; p4 = g4r;
	    }
	    if ((bw &= 31) > 0) 
		for (inval = *inp; bw > 0; bw -= 4)
		    p4 = g4r;
	} else {
	    CARD32 loval = olow, hival = ohigh;
	    loval = (loval << 8) | loval; loval = (loval << 16) | loval;
	    hival = (hival << 8) | hival; hival = (hival << 16) | hival;
	    for ( nw = bw >> 5; nw > 0; nw--) {
		inval = *inp++;
		p4 = g4b; p4 = g4b; p4 = g4b; p4 = g4b;
		p4 = g4b; p4 = g4b; p4 = g4b; p4 = g4b;
	    }
	    if ((bw &= 31) > 0) 
		for (inval = *inp; bw > 0; bw -= 4)
		    p4 = g4b;
	}
	/********
	if (bw > 0)
	    for (M=LOGLEFT, inval = *inp++; bw; bw--, LOGRIGHT(M)) 
		*outp++ =  (inval & M) ? ohigh : olow ; 
	*********/
	return(dst);
}

/* byte to bit converter
 * everything less than "threshold" becomes a zero
 */
void
bitshrink(src,dst,width,threshold)
     unsigned char *src, *dst, threshold;
     int  width;
{
  BytePixel *i = src;
  LogInt    *o = (LogInt*)dst, M, v;
  int bw;
  
  for(bw = width; bw >= LOGSIZE; *o++ = v, bw -= LOGSIZE)
    for(v = 0, M = LOGLEFT; M;    LOGRIGHT(M)) if(*i++ >= threshold) v |= M;
  if(bw > 0) {
    for(v = 0, M = LOGLEFT; bw--; LOGRIGHT(M)) if(*i++ >= threshold) v |= M;
    *o++ = v;
  }
}

/*------------------------------------------------------------------------
------------------ Bit manipulation in ROI section -----------------------
------------------------------------------------------------------------*/

/* 
** The following routines are used to turn a set of bits in a ROI extent
** of a scanline to 0's, to 1's, or to invert them.  'ix' is the starting
** bit number, and 'run' is the number of bits.  The routines can be 
** applied to eg BytePixel arrays by scaling the coordinates appropriately.
*/

void
action_clear(d, run, ix)		/* turn off bits in region */
    LogInt	*d;
    int		run, ix;
{
    int		sbit = ix & LOGMASK;

    ix >>= LOGSHIFT; d += ix;
    if ((sbit + run) >= LOGSIZE) {
	if (sbit) {
	    *d &=  ~(BitRight(LOGONES,sbit));
	    run -= (LOGSIZE - sbit); d++;
	}
	for (sbit = run >> LOGSHIFT; sbit > 0; sbit--) {
	    *d++ = 0;
	}
	if (run &= LOGMASK) {
	    *d &= BitRight(LOGONES,run);
	}
    } else {
	*d &= ~(BitRight(LOGONES,sbit) & ~(BitRight(LOGONES,sbit+run)));
    }
}

void
action_set(d, run, ix)			/* turn on bits in region */
    LogInt	*d;
    int		run, ix;
{
    int		sbit = ix & LOGMASK;

    ix >>= LOGSHIFT; d += ix;
    if ((sbit + run) >= LOGSIZE) {
	if (sbit) {
	    *d |= BitRight(LOGONES,sbit);
	    run -= (LOGSIZE - sbit); d++;
	}
	for (sbit = run >> LOGSHIFT; sbit > 0; sbit--) {
	    *d++ = LOGONES;
	}
	if (run &= LOGMASK) {
	    *d |= ~BitRight(LOGONES,run);
	}
    } else {
	*d |= (BitRight(LOGONES,sbit) & ~(BitRight(LOGONES,sbit+run)));
    }
}

void
action_invert(d, run, ix)		/* invert bits in region */
    LogInt	*d;
    int		run, ix;
{
    int		sbit = ix & LOGMASK;

    ix >>= LOGSHIFT; d += ix;
    if ((sbit + run) >= LOGSIZE) {
	if (sbit) {
	    *d ^= BitRight(LOGONES,sbit);
	    run -= (LOGSIZE - sbit); d++;
	}
	for (sbit = run >> LOGSHIFT; sbit > 0; sbit--) {
	    *d++ ^= LOGONES;
	}
	if (run &= LOGMASK) {
	    *d ^= ~BitRight(LOGONES,run);
	}
    } else {
	*d ^= (BitRight(LOGONES,sbit) & ~(BitRight(LOGONES,sbit+run)));
    }
}

/*------------------------------------------------------------------------
------------------------ Pixel Copy in ROI section -----------------------
------------------------------------------------------------------------*/
/* 
** NOTE:  Nominally used to copy src1 to destination in the passive
**   regions of a ROI.  To implement this efficiently we have a variety
**   of issues and choices:
**	a)  Simply call memcpy for the correct number of bytes.
**	b)  Simply call passcopy_bit below for the correct # of bits.
**	d)  For small number, do a while loop, else call memcpy or passocpy_bit.
**  NOTE: do not call if s == d, or if s overlaps d.
*/

/*    	(*(pvt->passive)) (dvoid, svoid, -run, ix);	*/

void passcopy_byte(d,s,dx,x)
    BytePixel	*d, *s;
    CARD32	dx, x;
{
    d += x;
    s += x;
    if (dx < 15)  while (dx-- > 0) *d++ = *s++;
    else	  memcpy (d, s, dx);
}

void passcopy_pair(d,s,dx,x)
    PairPixel	*d, *s;
    CARD32	dx, x;
{
    d += x;
    s += x;
    if (dx < 12) while (dx-- > 0) *d++ = *s++;
    else	 memcpy (d, s, dx << 1);
}

void passcopy_quad(d,s,dx,x)
    QuadPixel	*d, *s;
    CARD32	dx, x;
{
    d += x;
    s += x;
    if (dx < 8)	while (dx-- > 0) *d++ = *s++;
    else	memcpy (d, s, dx << 2);
}

void passcopy_real(d,s,dx,x)
    RealPixel	*d, *s;
    CARD32	dx, x;
{
    d += x;
    s += x;
    if (dx < 8)	while (dx-- > 0) *d++ = *s++;
    else	memcpy (d, s, dx << 2);
}

void
passcopy_bit(d,s,dx,x)
    LogInt	*d, *s;
    CARD32	dx, x;
{
    CARD32	sbit = x & LOGMASK;
    LogInt	M;

    x >>= LOGSHIFT; s += x; d += x;

    if ((sbit + dx) >= LOGSIZE) {
	if (sbit) {
	    M = BitRight(LOGONES,sbit);
	    *d = (*d & ~M) | (*s & M);
	    dx -= (LOGSIZE - sbit); d++; s++;
	}
	for (sbit = dx >> LOGSHIFT; sbit > 0; sbit--) {
	    *d++ = *s++;
	}
	if (dx &= LOGMASK) {
	    M = ~BitRight(LOGONES,dx);
	    *d = (*d & ~M) | (*s & M);
	}
    } else {
	M = (BitRight(LOGONES,sbit) & ~(BitRight(LOGONES,sbit+dx)));
	*d = (*d & ~M) | (*s & M);
    }
}

void (*passive_copy[5])() = {
    passcopy_real, passcopy_bit, passcopy_byte, passcopy_pair, passcopy_quad
};

/**** module bitfun.c ****/
