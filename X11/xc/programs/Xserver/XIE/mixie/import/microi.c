/* $Xorg: microi.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module microi.c ****/
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
  
	microi.c -- DDXIE import client roi element
  
	Dean Verheiden  -- AGE Logic, Inc. August, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/microi.c,v 3.5 2001/12/14 19:58:27 dawes Exp $ */

#define _XIEC_MICROI

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
#include <microi.h>
#include <texstr.h>
#include <strip.h>
#include <memory.h>

/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeICROI();

/*
 *  routines used internal to this module
 */
static int CreateICROI();
static int InitializeICROI();
static int ActivateICROI();
static int ResetICROI();
static int DestroyICROI();

static void rectCvt();

/*
 * DDXIE ImportClientROI entry points
 */
static ddElemVecRec ICROIVec =
{
	CreateICROI,
	InitializeICROI,
	ActivateICROI,
	(xieIntProc)NULL,
	ResetICROI,
	DestroyICROI
};


typedef struct _microidef {
	XieRegionPtr roireg;
	CARD32	  currentRect;	/* Current rectangle from input stream */
} miCROIDefRec, *miCROIDefPtr;

/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeICROI(flo,ped)
floDefPtr flo;
peDefPtr  ped;
{
	/* for now just stash our entry point vector in the peDef */
	ped->ddVec = ICROIVec;
	return(TRUE);
}                               /* end miAnalyzeICROI */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateICROI(flo,ped)
floDefPtr flo;
peDefPtr  ped;
{
	/* attach an execution context to the roi element definition */
	return MakePETex(flo,ped,sizeof(miCROIDefRec),NO_SYNC,NO_SYNC);
}                               /* end CreateICROI */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeICROI(flo,ped)
floDefPtr flo;
peDefPtr  ped;
{
    xieFloImportClientROI *raw = (xieFloImportClientROI *)ped->elemRaw;
    miCROIDefPtr rp = (miCROIDefPtr)ped->peTex->private;
    XieRegionPtr miXieRegionCreate();

    /* init icroi private data */
    if (!(rp->roireg = miXieRegionCreate((XieBoxRec *)NULL, 
						(int)raw->rectangles)))
        AllocError(flo,ped, return(FALSE));

    rp->currentRect = 0;
    if (raw->rectangles > 1)
    	rp->roireg->data->numRects = raw->rectangles;

    return InitReceptors(flo,ped,NO_DATAMAP,1) &&
				InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE);
}                               /* end InitializeICROI */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateICROI(flo,ped,pet)
floDefPtr flo;
peDefPtr  ped;
peTexPtr pet;
{
    miCROIDefPtr  rp = (miCROIDefPtr) pet->private;
    bandPtr     sbnd = &pet->receptor[IMPORT].band[0];
    bandPtr     dbnd = &pet->emitter[0];
    CARD32   maxRect = ((xieFloImportClientROI *)ped->elemRaw)->rectangles;
    xieTypRectangle *irect = (xieTypRectangle*)GetSrcBytes(flo,pet,sbnd,
			      sbnd->current, sizeof(xieTypRectangle),KEEP);
    XieBoxRec *rects, *br;
    CARD32    yxbands, size;
    ROIPtr roi;
    
    if(dbnd->final) {
      /* the client is being over generous, quietly discard the extra data */
      FreeData(flo,pet,sbnd,sbnd->maxGlobal);
      return(TRUE);
    }

    /* Stuff rectangles into XieBoxRec struct
     */
    if (irect && maxRect == 1) {
	rectCvt(irect,&rp->roireg->extents);
	rp->currentRect++;
    } else {
        rects = (XieBoxRec *)&rp->roireg->data[1];
        br = &rects[rp->currentRect];
	while (irect && rp->currentRect < maxRect) {
	    rectCvt(irect,br++);
	    rp->currentRect++;
            irect = (xieTypRectangle*)GetSrcBytes(flo,pet,sbnd,
			sbnd->current + sizeof(xieTypRectangle),
			sizeof(xieTypRectangle),KEEP);
	}
    }

    /* Ran out of rectangles, see if all have arrived
     */
    if(!irect && sbnd->final || rp->currentRect >= maxRect) {
        if(rp->currentRect < maxRect) {
            /* the client lied about the number of rectangles! */
	    ValueError(flo,ped,maxRect,return(FALSE));
	} else { /* All rectangles received, band them */
	    Bool Overlap;
	    if (!miXieRegionValidate(rp->roireg,&Overlap)) 
		AllocError(flo,ped,return(FALSE)); /* Best guess */
        }
        SetBandThreshold(sbnd,1);
        FreeData(flo,pet,sbnd,sbnd->maxGlobal);
    } else if (!irect) {
        /* free whatever we've used so far and
         * set the threshold to one byte more than whatever is left over
         */
        FreeData(flo,pet,sbnd,sbnd->current);
        SetBandThreshold(sbnd,sbnd->available + 1);
	return(TRUE);
    }

    /* At this point, all rectangles are here and everything appears to be OK
     */
    if (rp->roireg->data && rp->roireg->data->numRects) {
    	rects = (XieBoxRec *)&rp->roireg->data[1]; 
	maxRect = rp->roireg->data->numRects;
    } else { /* Only one box */
    	rects = &rp->roireg->extents; 
	maxRect = 1;
    }

    /* Step through rectangles and count up the total number of (y-x) bands
     */
    if (maxRect) {
        CARD32  r = 1;
        INT32 y = rects[0].y1;

	yxbands = 1;
	while (1) {
       	    while (r < maxRect && y == rects[r].y1) r++;
	    if (r == maxRect) break;
	    y = rects[r].y1;
	    yxbands++;	
	}
    } else {
	yxbands = 0;
    }

    /* Allocate storage for run length table
     */
    size = sizeof(ROIRec)+sizeof(lineRec)*yxbands+sizeof(runRec)*maxRect;
   
    if(!(roi = (ROIRec*)GetDstBytes(flo,pet,dbnd,0,size,FALSE)))
        AllocError(flo,ped,return(FALSE));

    roi->x      = rp->roireg->extents.x1;
    roi->y      = rp->roireg->extents.y1;
    roi->width  = rp->roireg->extents.x2 - roi->x;
    roi->height = rp->roireg->extents.y2 - roi->y;
    roi->nrects = maxRect;
    roi->lend   = LEND(dbnd);

    /* Fill in the run lengths
     */
    if (maxRect) {
        CARD32   r = 1;
        INT32    y = rects[0].y1;
        linePtr lp = (linePtr)&roi[1];
        runPtr  rp = (runPtr)&lp[1];

	lp->y = rects[0].y1;
	lp->nline = rects[0].y2 - rects[0].y1;
	lp->nrun = 1;
	rp->dstart = rects[0].x1 - roi->x;
	(rp++)->length = rects[0].x2 - rects[0].x1;

	while (1) {
       	    while (r < maxRect && y == rects[r].y1) {
		rp->dstart = rects[r].x1 - rects[r-1].x2;
		(rp++)->length = rects[r].x2 - rects[r].x1;
		lp->nrun++;
		r++;
	    }
	    if (r == maxRect) break;

	    lp = (linePtr)rp;
 	    rp = (runPtr)&lp[1];	
	    lp->y = rects[r].y1;
	    lp->nline = rects[r].y2 - rects[r].y1;
	    lp->nrun = 1;
	    rp->dstart = (CARD32)rects[r].x1 - (CARD32)roi->x;
	    (rp++)->length = rects[r].x2 - rects[r].x1;
	    y = rects[r++].y1;
	}
    } 

   SetBandFinal(dbnd);
   PutData(flo,pet,dbnd,size);

   return (TRUE);
}                               /* end ActivateICROI */

/*------------------------------------------------------------------------
------------------------- rectangle to box converter ---------------------
------------------------------------------------------------------------*/
static void rectCvt(irect,br)
xieTypRectangle *irect;
XieBoxRec	        *br;
{
    br->x1 = irect->x;
    br->y1 = irect->y;
    br->x2 = br->x1 + irect->width;
    br->y2 = br->y1 + irect->height;
}


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetICROI(flo,ped)
floDefPtr flo;
peDefPtr  ped;
{
	miCROIDefPtr rp = (miCROIDefPtr)ped->peTex->private;

	if (rp && rp->roireg) {
		miXieRegionDestroy(rp->roireg); 
		rp->roireg = (XieRegionPtr)NULL;
	}
	ResetReceptors(ped);
	ResetEmitter(ped);
	return TRUE;
}                               /* end ResetICROI */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyICROI(flo,ped)
floDefPtr flo;
peDefPtr  ped;
{
	/* get rid of the peTex structure  */
	ped->peTex = (peTexPtr) XieFree(ped->peTex);

	/* zap this element's entry point vector */
	ped->ddVec.create = (xieIntProc) NULL;
	ped->ddVec.initialize = (xieIntProc) NULL;
	ped->ddVec.activate = (xieIntProc) NULL;
	ped->ddVec.reset = (xieIntProc) NULL;
	ped->ddVec.destroy = (xieIntProc) NULL;

	return TRUE;
}                               /* end DestroyICROI */

/* end module microi.c */
