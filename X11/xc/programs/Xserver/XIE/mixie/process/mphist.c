/* $Xorg: mphist.c,v 1.4 2001/02/09 02:04:31 xorgcvs Exp $ */
/**** module mphist.c ****/
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
  
	mphist.c -- DDXIE match histogram element
  
	Larry Hare -- AGE Logic, Inc. August, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mphist.c,v 3.5 2001/12/14 19:58:45 dawes Exp $ */

#define _XIEC_MPHIST
#define _XIEC_PHIST

/*
 *  Include files
 */
#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif
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


/* routines referenced by other DDXIE modules
 */
int	miAnalyzeMatchHist();

/* routines used internal to this module
 */
static int CreateMatchHist();
static int InitializeMatchHist();
static int ActivateMatchHist();
static int ResetMatchHist();
static int DestroyMatchHist();

/* DDXIE Match Histogram entry points
 */
static ddElemVecRec MatchHistVec = {
    CreateMatchHist,
    InitializeMatchHist,
    ActivateMatchHist,
    (xieIntProc)NULL,
    ResetMatchHist,
    DestroyMatchHist
    };

/* declarations for private structures and actions procs ... */

typedef RealPixel MatchFloat;

static void match_hist(), flat_pdf(), gauss_pdf(), hyper_pdf();

typedef struct {
    CARD32	 histphase;
    CARD32	 histsize;
    CARD32	*histdata;
    void	(*histproc) ();
    void	(*lutproc) ();
} miMatchHistRec, *miMatchHistPtr;

static void doHistQ(), doHistP(), doHistB();
static void doLutQ(),  doLutP(),  doLutB();


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeMatchHist(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    /* for now just stash our entry point vector in the peDef */
    ped->ddVec = MatchHistVec;

    return TRUE;
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateMatchHist(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    /* attach an execution context to the photo element definition */

    /* always force syncing between inputs (is nop if only one input) */
    return MakePETex(flo, ped, sizeof(miMatchHistRec), SYNC, NO_SYNC);
}

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeMatchHist(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    peTexPtr    pet = ped->peTex;
    receptorPtr rcp = pet->receptor;
    bandPtr	iband;
    CARD32	nclip;
    xieFloMatchHistogram *raw = (xieFloMatchHistogram *)ped->elemRaw;
    miMatchHistPtr	  pvt = (miMatchHistPtr)(pet->private);
    
    iband = &(pet->receptor[SRCtag].band[0]);

    SetDepthFromLevels(iband->format->levels,nclip); nclip = 1 << nclip;

    switch (iband->format->class) {
	case	QUAD_PIXEL:	pvt->histproc = doHistQ;
				pvt->lutproc = doLutQ;
				break;
	case	PAIR_PIXEL:	pvt->histproc = doHistP;
				pvt->lutproc = doLutP;
				break;
	case	BYTE_PIXEL:	pvt->histproc = doHistB;
				pvt->lutproc = doLutB;
				break;
	default: ImplementationError(flo, ped, return(FALSE));
    }

    pvt->histphase = 1;
    pvt->histsize = nclip;
    if (!(pvt->histdata = (CARD32 *) XieCalloc(nclip * sizeof(CARD32))))
	AllocError(flo,ped,return(FALSE));

    return InitReceptor(flo,ped,&rcp[SRCt1],NO_DATAMAP,1,1,NO_BANDS) && 
	   InitProcDomain(flo, ped, raw->domainPhototag,
			raw->domainOffsetX, raw->domainOffsetY) &&
           InitEmitter(flo,ped,NO_DATAMAP,SRCt1);
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateMatchHist(flo,ped,pet)
    floDefPtr flo;
    peDefPtr  ped;
    peTexPtr  pet;
{
    xieFloMatchHistogram *raw = (xieFloMatchHistogram *)ped->elemRaw;
    miMatchHistPtr	  pvt = (miMatchHistPtr)(pet->private);
    receptorPtr	rcp = pet->receptor;
    bandPtr	sbnd = &rcp->band[0];
    bandPtr	dbnd = &pet->emitter[0];


    if (pvt->histphase == 1) {
	pointer src;

	src = GetCurrentSrc(flo,pet,sbnd);
	while(!ferrCode(flo) && src && SyncDomain(flo,ped,sbnd,KEEP)) {
	    INT32 x = 0, dx;
	    while (dx = GetRun(flo,pet,sbnd)) {
		if (dx > 0) {
		    (*(pvt->histproc)) (src,pvt->histdata,pvt->histsize,x,dx);
		    x += dx; /* nhist += dx; */
		} else 
		    x -= dx;
	    }
	    src = GetNextSrc(flo,pet,sbnd,KEEP);
	}

	/* Is it time to switch to phase2 */
	if (src || !sbnd->final) {
	    /* 
		Since we are keeping data for the second pass, we need to
		keep incrementing the threshold so that we won't get activated
		until new data is available. 
	    */
	    SetBandThreshold(sbnd, sbnd->current + 1);
	    return (TRUE);
	}
	pvt->histphase = 2;
    }

    /* finished with accumulation, figure out desired LUT */
    if (pvt->histphase == 2) {

	CARD32	nlev = sbnd->format->levels;
	CARD32  nclip = pvt->histsize;
	MatchFloat *pdfdata, pdftemp[256];


	    /*
	    ** NOTE: Could try to maintain for longer periods of time,
	    ** OR could try to put on stack for small sizes.
	    */
	    if (nclip <= 256)
		pdfdata = &pdftemp[0];
	    else if (!(pdfdata = (MatchFloat *)
			XieMalloc(nclip*sizeof(MatchFloat))))
		AllocError(flo,ped,return(FALSE));

	    /* generate LUT based on match curve type */
	    switch (raw->shape) {
	    case xieValHistogramFlat:
		flat_pdf(NULL,		pdfdata, nlev);
		break;
	    case xieValHistogramGaussian:
		gauss_pdf((pTecHistogramGaussianDefPtr)ped->techPvt,
					pdfdata, nlev);
		break;
	    case xieValHistogramHyperbolic:
		hyper_pdf((pTecHistogramHyperbolicDefPtr)ped->techPvt,
					pdfdata, nlev);
		break;
	    default:
		if (nclip > 256) (void) XieFree(pdfdata);
		ImplementationError(flo, ped, return(FALSE));
	    }

	    /* match the histogram and the new shape */
	    match_hist(pvt->histdata, pdfdata, nlev);

	    if (nclip > 256) (void) XieFree(pdfdata);


	if (nclip > nlev) { 
	    /* zero tail entries, then mask instead of compare input pixels */
	    bzero(pvt->histdata + nlev, (nclip - nlev) * sizeof(CARD32));
	}

	/* Reset src to start from scratch.
	** The domain automatically resyncs in the SyncDomain call.
	*/
	(void) GetSrc(flo,pet,sbnd,0,KEEP);
	pvt->histphase = 3;
    }
    /* now processing to create actual outputs */
    if (pvt->histphase == 3) {
	pointer src, dst;

	src = GetCurrentSrc(flo,pet,sbnd);
	dst = GetCurrentDst(flo,pet,dbnd);
	while(!ferrCode(flo) && src && dst && SyncDomain(flo,ped,dbnd,FLUSH)) {
	    INT32 x = 0, dx;
	    if (src != dst) memcpy (dst, src, dbnd->pitch);
	    while (dx = GetRun(flo,pet,dbnd)) {
		if (dx > 0) {
		    (*(pvt->lutproc)) (dst,pvt->histdata,pvt->histsize,x,dx);
		    x += dx; /* nhist += dx; */
		} else 
		    x -= dx;
	    }
	    src = GetNextSrc(flo,pet,sbnd,FLUSH);
	    dst = GetNextDst(flo,pet,dbnd,FLUSH);
	}
	FreeData(flo,pet,sbnd,sbnd->current);

	/* when dbnd->final goes true, we are done */
    }
    return TRUE;
}

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetMatchHist(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    miMatchHistPtr pvt = (miMatchHistPtr)(ped->peTex->private);
    if (pvt && pvt->histdata)
	    pvt->histdata = (CARD32 *) XieFree(pvt->histdata);
    
    ResetReceptors(ped);
    ResetEmitter(ped);
    
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyMatchHist(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
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

/*
**  NOTE: Consider revamping Export Client Histogram to not use
**  'pvt' structure so that more sharing of code (and thus potential
**  future optimizations) is possible.
*/

static void
doHistQ(svoid,hist,clip,x,dx)
    pointer	svoid;
    CARD32	*hist, clip, x, dx;
{
    QuadPixel *src = (QuadPixel *) svoid;

    for ( src += x, clip -= 1; dx > 0 ; dx--)
	hist[*src++ & clip]++;
}

static void
doHistP(svoid,hist,clip,x,dx)
    pointer	svoid;
    CARD32	*hist, clip, x, dx;
{
    PairPixel *src = (PairPixel *) svoid;

    for ( src += x, clip -= 1; dx > 0 ; dx--)
	hist[*src++ & clip]++;
}

static void
doHistB(svoid,hist,clip,x,dx)
    pointer	svoid;
    CARD32	*hist, clip, x, dx;
{
    BytePixel *src = (BytePixel *) svoid;

    for ( src += x, clip -= 1; dx > 0 ; dx--)
	hist[*src++ & clip]++;
}



/*------------------------------------------------------------------------
------------------------ action procs to process lut --------------------
------------------------------------------------------------------------*/

static void
doLutQ(dvoid,lut,clip,x,dx)
    pointer	dvoid;
    CARD32	*lut, clip, x, dx;
{
    QuadPixel	*dst = (QuadPixel *) dvoid;

    for ( dst += x, clip -= 1; dx > 0 ; dx--, dst++)
	*dst = lut[*dst & clip];
}

static void
doLutP(dvoid,lut,clip,x,dx)
    pointer	dvoid;
    CARD32	*lut, clip, x, dx;
{
    PairPixel	*dst = (PairPixel *) dvoid;

    for ( dst += x, clip -= 1; dx > 0 ; dx--, dst++)
	*dst = lut[*dst & clip];
}

static void
doLutB(dvoid,lut,clip,x,dx)
    pointer	dvoid;
    CARD32	*lut, clip, x, dx;
{
    BytePixel	*dst = (BytePixel *) dvoid;

    for ( dst += x, clip -= 1; dx > 0 ; dx--, dst++)
	*dst = lut[*dst & clip];
}


/*------------------------------------------------------------------------
-------------------------- process matching curves -----------------------
------------------------------------------------------------------------*/


static void
match_hist(hptr, pdf, nlev)
    CARD32 *hptr;
    MatchFloat *pdf;
    CARD32 nlev;
{
    double	sum, sf; 
    MatchFloat	match, closest, delta;
    CARD32	ilev, jlev, plev, hsum;
    
    /* sum up the probability curve distribution */
    for (sum=0., ilev = 0; ilev < nlev; ilev++)
	sum += pdf[ilev];

    /* normalize and accumulate the pdf */
    sf = 1.0 / (sum ? sum : 1.0);
    for (sum=0., ilev = 0; ilev < nlev; ilev++) {
	sum += pdf[ilev];
	pdf[ilev] = sum * sf;
    }

    /* count up the histogram entries */
    for (hsum=0, ilev = 0; ilev < nlev ; ilev++) {
	hsum += hptr[ilev];
    }

    /* match the histogram and the new shape */
    sf = 1.0 / (double) (hsum ? hsum : 1.0);
    for (ilev=0, plev=0, hsum=0; ilev < nlev ; ilev++) {
	hsum += hptr[ilev];
	match = (MatchFloat) hsum * sf;  /* normalize, accumulate */

	/* search forwards to find closest match */
	for (jlev = plev, closest = 99.0 ; jlev < nlev ; jlev++) {

	    delta = match - pdf[jlev];
	    if (delta < 0.) delta = -delta; 

	    if (delta == 0.)		{jlev++; break; }
	    else if (delta < closest)	closest = delta;
	    else if (delta > closest)	break;
	}
	hptr[ilev] = plev = jlev - 1;
    }
}

static void
flat_pdf(raw, pdf, nlev)
    pTecHistogramGaussianDefPtr raw;
    MatchFloat *pdf;
    CARD32 nlev;
{
    CARD32 ilev;
    double sf = 1.0 / (double) nlev;

    for (ilev = 0; ilev < nlev; ilev++)
	*pdf++ = sf;
}

static void
gauss_pdf(raw, pdf, nlev)
    pTecHistogramGaussianDefPtr raw;
    MatchFloat *pdf;
    CARD32 nlev;
{
    CARD32 ilev;
    double mean, sigma, a, b, xx;

    mean = raw->mean;
    sigma = raw->sigma;

    a = 1.0 / sqrt ( 2.0 * M_PI );	/* .39894228 ..... */
    b = 2.0 * sigma * sigma;

    for (ilev = 0; ilev < nlev; ilev++) {
	xx = (double) ilev - mean;
	pdf[ilev] = a * exp( - (xx * xx) / b);
    }
}

static void
hyper_pdf(raw, pdf, nlev)
    pTecHistogramHyperbolicDefPtr raw;
    MatchFloat *pdf;
    CARD32 nlev;
{
    CARD32 ilev;
    BOOL   sf;
    double cnst, lg;


    sf = raw->shapeFactor;
    cnst  = raw->constant;
    lg = log(1.0 + 1.0 / cnst);

    for (ilev = 0; ilev < nlev; ilev++)
	pdf[ilev] = 1.0 / (((double)(sf ? nlev-1-ilev : ilev) + cnst) * lg);
}

/* end module mphist.c */
