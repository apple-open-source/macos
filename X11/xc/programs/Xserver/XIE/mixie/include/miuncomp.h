/* $Xorg: miuncomp.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/**** module miuncomp.h ****/
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
  
	miuncomp.c -- DDXIE includes for importing uncompressed data
  
	Dean Verheiden -- AGE Logic, Inc. September 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/include/miuncomp.h,v 1.5 2001/12/14 19:58:32 dawes Exp $ */

#define MIUNCOMP_BP_ARGS \
     BytePixel *src, \
     BytePixel *dst, \
     CARD32 count, \
     CARD32 leftpad, \
     CARD32 depth, \
     CARD32 stride

typedef struct _miuncompdef {
  stripPtr next_strip;	    /* used by import photomap only */
  void   (*action)(/*MIUNCOMP_BP_ARGS*/);
  void	 (*tripleaction)(MIUNCOMP_BP_ARGS); /* (unknown params; not used?) */
  CARD32 Bstride;
  CARD8	 srcoffset;	    /* Number of bytes offset to this pixel's band*/
  CARD8  mask;	            /* For obtaining subbyte pixels 	 	*/
  CARD8  shift;	            /* Necessary shift after obtaining pixel 	*/
  CARD8	 bandMap;
  CARD8  leftPad;	    /* Scanline pad (in bits)			*/
  CARD8	 bitOff;	    /* more that we can chew? 			*/
  BOOL	 unaligned;	    /* Flag used by band-by-pixel decoding 	*/
  BOOL	 reformat;          /* Flag used to request data re-formatting  */
  pointer buf;
} miUncompRec, *miUncompPtr;

#ifdef _XIEC_MIUNCOMP

/* Bit reversal routine for single band uncompressed bitonal images */
extern void CPreverse_bits(MIUNCOMP_BP_ARGS);
extern void CPpass_bits(MIUNCOMP_BP_ARGS);
extern void CPextractstreambits(MIUNCOMP_BP_ARGS);
extern void CPextractswappedstreambits(MIUNCOMP_BP_ARGS);

/* Byte copy routine for nicely aligned data */
extern void CPpass_bytes(MIUNCOMP_BP_ARGS);

#define MIUNCOMP_PP_ARGS \
     PairPixel *src, \
     PairPixel *dst, \
     CARD32 count, \
     CARD32 leftpad, \
     CARD32 depth, \
     CARD32 stride

/* Pairpixel copy routine for nicely aligned data */
extern void CPpass_pairs(MIUNCOMP_PP_ARGS);
extern void CPswap_pairs(MIUNCOMP_PP_ARGS);

#define MIUNCOMP_QP_ARGS \
     QuadPixel *src, \
     QuadPixel *dst, \
     CARD32 count, \
     CARD32 leftpad, \
     CARD32 depth, \
     CARD32 stride

/* Quadpixel copy routine for nicely aligned data */
extern void CPpass_quads(MIUNCOMP_QP_ARGS);
extern void CPswap_quads(MIUNCOMP_QP_ARGS);

#if XIE_FULL

#define MIUNCOMP_SP_ARGS \
     PairPixel *isrc, \
     PairPixel *dst, \
     CARD32 count, \
     miUncompPtr pvt

#define MIUNCOMP_SB_ARGS \
     BytePixel *src, \
     BytePixel *dst, \
     CARD32 count, \
     miUncompPtr pvt

/* Action routines used by Triple band by pixel decoding */
extern void StoB(MIUNCOMP_SB_ARGS);
extern void StoP(MIUNCOMP_SP_ARGS);
extern void StosP(MIUNCOMP_SP_ARGS);
extern void SbtoB(MIUNCOMP_SB_ARGS);
extern void Sbtob(MIUNCOMP_SB_ARGS);
#endif /* XIE_FULL */

#define MIUNCOMP_UB_ARGS \
     CARD8 *src, \
     BytePixel *dst, \
     CARD32 numcmp, \
     CARD32 leftpad, \
     CARD32 depth, \
     CARD32 stride

#define MIUNCOMP_UP_ARGS \
     CARD8 *src, \
     PairPixel *dst, \
     CARD32 numcmp, \
     CARD32 leftpad, \
     CARD32 depth, \
     CARD32 stride

#define MIUNCOMP_UQ_ARGS \
     CARD8 *src, \
     QuadPixel *dst, \
     CARD32 numcmp, \
     CARD32 leftpad, \
     CARD32 depth, \
     CARD32 stride

/* Single band unaligned Stream to Pixel conversion routines */
extern void LLUBtoB(MIUNCOMP_UB_ARGS);
extern void LMUBtoB(MIUNCOMP_UB_ARGS);
extern void MLUBtoB(MIUNCOMP_UB_ARGS);
extern void MMUBtoB(MIUNCOMP_UB_ARGS);
extern void LLUPtoP(MIUNCOMP_UP_ARGS);
extern void LMUPtoP(MIUNCOMP_UP_ARGS);
extern void MLUPtoP(MIUNCOMP_UP_ARGS);
extern void MMUPtoP(MIUNCOMP_UP_ARGS);
extern void LLUQtoQ(MIUNCOMP_UQ_ARGS);
extern void LMUQtoQ(MIUNCOMP_UQ_ARGS);
extern void MLUQtoQ(MIUNCOMP_UQ_ARGS);
extern void MMUQtoQ(MIUNCOMP_UQ_ARGS);

#else /* ifdef _XIEC_MIUNCOMP */

/* Bit reversal routine for single band uncompressed bitonal images */
extern void CPreverse_bits();
extern void CPpass_bits();
extern void CPextractstreambits();
extern void CPextractswappedstreambits();

/* Byte copy routine for nicely aligned data */
extern void CPpass_bytes();

/* Pairpixel copy routine for nicely aligned data */
extern void CPpass_pairs();
extern void CPswap_pairs();

/* Quadpixel copy routine for nicely aligned data */
extern void CPpass_quads();
extern void CPswap_quads();

#if XIE_FULL
/* Action routines used by Triple band by pixel decoding */
extern void StoB();
extern void StoP();
extern void StosP();
extern void SbtoB();
extern void Sbtob();

/* Array of pointers to actions routines for unaligned triple band by pixel */
extern void (*ExtractTripleFuncs[2][2][2][2][2])();

#endif /* XIE_FULL */

/* Single band unaligned Stream to Pixel conversion routines */
extern void LLUBtoB();
extern void LMUBtoB();
extern void MLUBtoB();
extern void MMUBtoB();
extern void LLUPtoP();
extern void LMUPtoP();
extern void MLUPtoP();
extern void MMUPtoP();
extern void LLUQtoQ();
extern void LMUQtoQ();
extern void MLUQtoQ();
extern void MMUQtoQ();

#endif /* ifdef _XIEC_MIUNCOMP */
