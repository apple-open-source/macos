/* $Xorg: meuncomp.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
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
  
	meuncomp.c -- DDXIE includes for exporting uncompressed data
  
	Dean Verheiden -- AGE Logic, Inc. October 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/include/meuncomp.h,v 1.5 2001/12/14 19:58:32 dawes Exp $ */

typedef struct _meuncompdef {
  void     (*action)();
  CARD32   Bstride;
  CARD8	   dstoffset;	/* Number of bytes offset to this pixel's band 	*/
  CARD8    mask;	/* For obtaining subbyte pixels 	 	*/
  CARD8    shift;	/* Necessary shift after obtaining pixel 	*/
  CARD8	   bandMap;
  CARD8    clear_dst;	/* Scanline pad (in bits)			*/
  CARD8	   bitOff;	/* more that we can chew? 			*/
  CARD8	   leftOver;
  CARD8	   depth;
  CARD16   stride;
  CARD8	   unaligned;
  CARD8    pad;
  CARD32   width;
  CARD32   pitch;
  pointer  buf;
} meUncompRec, *meUncompPtr;

#define MEUNCOMP_BP_ARGS BytePixel *src, BytePixel *dst, meUncompPtr pvt
#define MEUNCOMP_PP_ARGS PairPixel *src, PairPixel *dst, meUncompPtr pvt
#define MEUNCOMP_QP_ARGS QuadPixel *src, QuadPixel *dst, meUncompPtr pvt

/* Action routines for encoding pixel planes */
extern void btoS(MEUNCOMP_BP_ARGS);
extern void sbtoS(MEUNCOMP_BP_ARGS);
extern void btoIS(MEUNCOMP_BP_ARGS);
extern void sbtoIS(MEUNCOMP_BP_ARGS);
extern void BtoS(MEUNCOMP_BP_ARGS);
extern void PtoS(MEUNCOMP_PP_ARGS);
extern void sPtoS(MEUNCOMP_PP_ARGS);
extern void QtoS(MEUNCOMP_QP_ARGS);
extern void sQtoS(MEUNCOMP_QP_ARGS);
extern void QtoIS(MEUNCOMP_QP_ARGS);
extern void sQtoIS(MEUNCOMP_QP_ARGS);

/* Action routines used by Triple band by pixel encoding */
extern void BtoIS(MEUNCOMP_BP_ARGS);
extern void PtoIS(MEUNCOMP_PP_ARGS);
extern void sPtoIS(MEUNCOMP_PP_ARGS);
extern void BtoISb(MEUNCOMP_BP_ARGS);
extern void btoISb(MEUNCOMP_BP_ARGS);

#define MEUNCOMP_UB_ARGS BytePixel *src, CARD8 *dst, meUncompPtr pvt	
#define MEUNCOMP_UP_ARGS PairPixel *src, CARD8 *dst, meUncompPtr pvt	
#define MEUNCOMP_UQ_ARGS QuadPixel *src, CARD8 *dst, meUncompPtr pvt	

/* Unaligned data packing routines */
extern void BtoLLUB(MEUNCOMP_UB_ARGS);
extern void BtoLMUB(MEUNCOMP_UB_ARGS);
extern void BtoMLUB(MEUNCOMP_UB_ARGS);
extern void BtoMMUB(MEUNCOMP_UB_ARGS);
extern void PtoLLUP(MEUNCOMP_UP_ARGS);
extern void PtoLMUP(MEUNCOMP_UP_ARGS);
extern void PtoMLUP(MEUNCOMP_UP_ARGS);
extern void PtoMMUP(MEUNCOMP_UP_ARGS);
extern void QtoLLUQ(MEUNCOMP_UQ_ARGS);
extern void QtoLMUQ(MEUNCOMP_UQ_ARGS);
extern void QtoMLUQ(MEUNCOMP_UQ_ARGS);
extern void QtoMMUQ(MEUNCOMP_UQ_ARGS);


#ifndef _XIEC_MEUNCOMP

#if XIE_FULL
/* Array of pointers to actions routines for unaligned triple band by pixel */
extern void (*EncodeTripleFuncs[2][2][2][2][2])();
#endif

#endif /* ifdef _XIEC_MEUNCOMP */
