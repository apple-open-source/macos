/* $Xorg: domain.h,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module domain.h ****/
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
******************************************************************************
  
	domain.h -- DDXIE MI Process Domain definitions
  
	Dean Verheiden  -- AGE Logic, Inc. August 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/include/domain.h,v 1.5 2001/12/14 19:58:30 dawes Exp $ */


#ifndef _XIEH_DOMAIN
#define _XIEH_DOMAIN

#if !defined(_XIEC_DOMAIN)

#define	SyncDomain	(*pet->roiinit)
#define	GetRun		(*pet->roiget)

extern Bool  InitProcDomain(
			floDefPtr flo,
			peDefPtr ped,
			xieTypPhototag dtag,
			INT32 offX,
			INT32 offY);
extern void  ResetProcDomain(peDefPtr ped);

#endif /* ! defined _XIEC_DOMAIN */

/*
 * Rectangles of Interest structure definitions 
 */

#define RUNPTR(idx)	(&((runPtr)&lp[1])[idx])
#define LEND(rband)	((linePtr)&rband->strip->data[rband->strip->length])

typedef struct _runRec {
	CARD32 dstart; 		/* delta to start of run */
	CARD32 length;		/* length of run */
} runRec, *runPtr;

typedef struct _linerec {
	INT32	y;		/* starting y coodinate of this y-x band */
	CARD32	nline;		/* height of y-x band */
	CARD32  nrun;		/* number of runRecs for this y-x band */
/*      runRec  run[nrun]; */
} lineRec, *linePtr;

typedef struct _ROIrec {
	INT32   x, y;		/* min x and y coordinates of bounding box */
	CARD32  width;		/* width  of bounding box 		   */
	CARD32  height;		/* height of bounding box 		   */
	CARD32  nrects;		/* number of rectangles in this ROI 	   */
	linePtr lend;		/* first address past end of ROI table	   */
} ROIRec, *ROIPtr;

#endif /* module _XIEH_DOMAIN */
