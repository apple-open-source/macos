/* $Xorg: microi.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/**** module microi.h ****/
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
  
	microi.h -- DDXIE MI ImportClientROI definitions
  
	Dean Verheiden -- AGE Logic, Inc. August 1993
  
*****************************************************************************/

#ifndef _XIEH_MICROI
#define _XIEH_MICROI


typedef struct _XieDDXPoint {
    INT32 x, y;
} XieDDXPointRec, *XieDDXPointPtr;

typedef struct _XieBox {
    INT32 x1, y1, x2, y2;
} XieBoxRec, *XieBoxPtr;

typedef struct _XieRegData {
    long	size;
    long 	numRects;
/*  BoxRec	rects[size];   in memory but not explicitly declared */
} XieRegDataRec, *XieRegDataPtr;

typedef struct _XieRegion {
    XieBoxRec 		extents;
    XieRegDataPtr	data;
} XieRegionRec, *XieRegionPtr;

#ifdef _XIEC_MIREGION

#define XieNullRegion ((XieRegionPtr)0)

#define XIEREGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
#define XIEREGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define XIEREGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define XIEREGION_RECTS(reg) ((reg)->data ? (XieBoxPtr)((reg)->data + 1) \
			               : &(reg)->extents)
#define XIEREGION_BOXPTR(reg) ((XieBoxPtr)((reg)->data + 1))
#define XIEREGION_BOX(reg,i) (&XIEREGION_BOXPTR(reg)[i])
#define XIEREGION_TOP(reg) XIEREGION_BOX(reg, (reg)->data->numRects)
#define XIEREGION_END(reg) XIEREGION_BOX(reg, (reg)->data->numRects - 1)
#define XIEREGION_SZOF(n) (sizeof(XieRegDataRec) + ((n) * sizeof(XieBoxRec)))

#else  /* defined _XIEC_MIREGION */

extern XieRegionPtr miXieRegionCreate();
extern Bool miXieRegionValidate();

#endif

#endif /* module _XIEH_MICROI */
