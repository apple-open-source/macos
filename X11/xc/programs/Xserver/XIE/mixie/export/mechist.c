/* $Xorg: mechist.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module mechist.c ****/
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
  
	mechist.c -- DDXIE export client histogram element
  
	Larry Hare -- AGE Logic, Inc. August, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/mechist.c,v 3.6 2001/12/14 19:58:18 dawes Exp $ */

#define _XIEC_MECHIST
#define _XIEC_ECHIST

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
#include <memory.h>

/* routines used internal to this module
 */
static int CreateECHist(floDefPtr flo, peDefPtr ped);
static int InitializeECHist(floDefPtr flo, peDefPtr ped);
static int ActivateECHist(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ResetECHist(floDefPtr flo, peDefPtr ped);
static int DestroyECHist(floDefPtr flo, peDefPtr ped);

/* DDXIE ExportClientHist entry points
 */
static ddElemVecRec ECHistVec = {
    CreateECHist,
    InitializeECHist,
    ActivateECHist,
    (xieIntProc)NULL,
    ResetECHist,
    DestroyECHist
    };

/* declarations for private structures and actions procs ... */

typedef struct {
    pointer	histdata;
    CARD32	 histsize;
    void	(*histproc) ();
} miECHistRec, *miECHistPtr;

extern void doHistQ(pointer svoid, CARD32 *hist, CARD32 clip, CARD32 x, CARD32 dx);
extern void doHistP(pointer svoid, CARD32 *hist, CARD32 clip, CARD32 x, CARD32 dx);
extern void doHistB(pointer svoid, CARD32 *hist, CARD32 clip, CARD32 x, CARD32 dx);
extern void doHistb(pointer svoid, CARD32 *hist, CARD32 clip, CARD32 x, CARD32 dx);


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeECHist(floDefPtr flo, peDefPtr ped)
{
    /* for now just stash our entry point vector in the peDef */
    ped->ddVec = ECHistVec;

    return TRUE;
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateECHist(floDefPtr flo, peDefPtr ped)
{
    /* attach an execution context to the photo element definition */

    /* always force syncing between inputs (is nop if only one input) */
    return MakePETex(flo, ped, sizeof(miECHistRec), SYNC, NO_SYNC);
}

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeECHist(floDefPtr flo, peDefPtr ped)
{
    xieFloExportClientHistogram *raw =
				(xieFloExportClientHistogram *)ped->elemRaw;
    peTexPtr    pet = ped->peTex;
    receptorPtr rcp = pet->receptor;
    miECHistPtr	pvt = (miECHistPtr)(pet->private);
    bandPtr	iband;
    CARD32	nclip;
    
    iband = &(pet->receptor[SRCtag].band[0]);

    switch (iband->format->class) {
	case	QUAD_PIXEL:	pvt->histproc = doHistQ;
				break;
	case	PAIR_PIXEL:	pvt->histproc = doHistP;
				break;
	case	BYTE_PIXEL:	pvt->histproc = doHistB;
				break;
	case	BIT_PIXEL:	pvt->histproc = doHistb;
				break;
	default: ImplementationError(flo, ped, return(FALSE));
    }

    SetDepthFromLevels(iband->format->levels, nclip);
    pvt->histsize = nclip = 1 << nclip;

    if (!(pvt->histdata = (pointer ) XieCalloc(nclip * sizeof(CARD32))))
	AllocError(flo,ped,return(FALSE));

    return InitReceptor(flo,ped,&rcp[SRCt1],NO_DATAMAP,1,1,NO_BANDS) && 
	   InitProcDomain(flo, ped, raw->domainPhototag,
			raw->domainOffsetX, raw->domainOffsetY) &&
           InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE);
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateECHist(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  xieFloExportClientHistogram *raw =
			(xieFloExportClientHistogram *)ped->elemRaw;
  miECHistPtr	pvt = (miECHistPtr)(pet->private);
  receptorPtr	rcp = pet->receptor;
  bandPtr	sbnd = &rcp->band[0];
  bandPtr	dbnd = &pet->emitter[0];
  pointer src;
  
  src = GetCurrentSrc(flo,pet,sbnd);
  while(src && SyncDomain(flo,ped,sbnd,FLUSH)) {
    INT32 x = 0, dx;
    while ((dx = GetRun(flo,pet,sbnd)) != 0) {
      if (dx > 0) {
	(*(pvt->histproc)) (src,pvt->histdata,pvt->histsize,x,dx);
	x += dx;
      } else 
	x -= dx;
    }
    src = GetNextSrc(flo,pet,sbnd,FLUSH);
  }
  FreeData(flo,pet,sbnd,sbnd->current);
  
  /* if finished with accumulation, send back to client */
  if (!src && sbnd->final) {
    CARD32	*hist;
    CARD32	ilev, nlev = sbnd->format->levels;
    CARD32	nhist = 0;
    xieTypHistogramData *histpair;
    
    /* Count populated cells */
    for (ilev = 0, hist = (CARD32*)pvt->histdata; ilev < nlev; ilev++, hist++)
      if (*hist) 
	nhist++;
    
    if(nhist) {
      if(!(histpair = (xieTypHistogramData*)GetDstBytes(flo,pet,dbnd,0,
				  nhist * sizeof(xieTypHistogramData),KEEP)))
	return FALSE;
      
      for (ilev = 0, hist = (CARD32*)pvt->histdata; ilev < nlev; ilev++, hist++)
	if (*hist) {
	  histpair->count = *hist;
	  histpair->value =  ilev;
	  histpair++;
	}
      SetBandFinal(dbnd);
      PutData(flo,pet,dbnd, nhist * sizeof(xieTypHistogramData));
    } else {
      /* signal that there is no data to send
       */
      DisableDst(flo,pet,dbnd);
    }
    switch(raw->notify) {
    case xieValFirstData:	/* fall thru */
    case xieValNewData:	SendExportAvailableEvent(flo,ped,
						 0,	/* band   */
						 nhist,	/* count  */
						 0,0);  /* unused */
    default:		break;
    }
  }
  return TRUE;
}


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetECHist(floDefPtr flo, peDefPtr ped)
{
    miECHistPtr pvt = (miECHistPtr) ped->peTex->private;

    /* free any dynamic private data */
    if (pvt->histdata)
	pvt->histdata = (pointer ) XieFree(pvt->histdata);

    ResetReceptors(ped);
    ResetEmitter(ped);
    
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyECHist(floDefPtr flo, peDefPtr ped)
{
    /* get rid of the peTex structure  */
    ped->peTex = (peTexPtr) XieFree(ped->peTex);

    /* zap this element's entry point vector */
    ped->ddVec.create     = (xieIntProc) NULL;
    ped->ddVec.initialize = (xieIntProc) NULL;
    ped->ddVec.activate   = (xieIntProc) NULL;
    ped->ddVec.reset      = (xieIntProc) NULL;
    ped->ddVec.destroy    = (xieIntProc) NULL;

    return TRUE;
}


/*------------------------------------------------------------------------
------------------------ action procs to do histogram --------------------
------------------------------------------------------------------------*/

void doHistQ(
     pointer	svoid,
     CARD32	*hist,
     CARD32	clip,
     CARD32	x,
     CARD32	dx)
{
    QuadPixel *src = (QuadPixel *) svoid;

    for ( src += x, clip -= 1; dx > 0 ; dx--)
	hist[*src++ & clip]++;
}

void doHistP(
     pointer	svoid,
     CARD32	*hist,
     CARD32	clip,
     CARD32	x,
     CARD32	dx)
{
    PairPixel *src = (PairPixel *) svoid;

    for ( src += x, clip -= 1; dx > 0 ; dx--)
	hist[*src++ & clip]++;
}

void doHistB(
     pointer	svoid,
     CARD32	*hist,
     CARD32	clip,
     CARD32	x,
     CARD32	dx)
{
    BytePixel *src = (BytePixel *) svoid;

    for ( src += x, clip -= 1; dx > 0 ; dx--)
	hist[*src++ & clip]++;			/* could pad to 256 .. */
}

void doHistb(
     pointer	svoid,
     CARD32	*hist,
     CARD32	clip,
     CARD32	x,
     CARD32	dx)
{
    LogInt *src = (LogInt *) svoid;
    CARD32 cnt0 = 0, cnt1 = 0;

    /* does anyone actually do bit histograms? :-) */
    for ( ; dx > 0 ; dx--, x++)
	if (LOG_tstbit(src,x))
	    cnt1++;
	else
	    cnt0++;

    hist[0] += cnt0;
    hist[1] += cnt1;
}

/* end module mechist.c */
